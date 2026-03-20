#!/usr/bin/env python3
"""Ponte Serial <-> MQTT para o controlador de mesa regulavel.

ESP32 conectado via USB envia JSON pelo Serial e recebe comandos simples.
Este script faz a bridge com um broker MQTT (ex: Home Assistant Mosquitto).

Configuracao via variaveis de ambiente ou editando as constantes abaixo.
"""

import json
import os
import sys
import time

import paho.mqtt.client as mqtt
import serial
from serial.tools import list_ports

# --- Configuracao ---
SERIAL_PORT = os.environ.get("MESA_SERIAL_PORT", "/dev/ttyACM0")  # vazio = auto-detectar
SERIAL_BAUD = int(os.environ.get("MESA_SERIAL_BAUD", "115200"))
VERBOSE = os.environ.get("MESA_VERBOSE", "0") == "1"
MQTT_BROKER = os.environ.get("MESA_MQTT_BROKER", "192.168.15.20")
MQTT_PORT = int(os.environ.get("MESA_MQTT_PORT", "1883"))
MQTT_USER = os.environ.get("MESA_MQTT_USER", "mesa_pc")
MQTT_PASS = os.environ.get("MESA_MQTT_PASS", "mesa")

# --- Topics MQTT ---
TOPIC_DISPONIVEL = "mesa/disponivel"
TOPIC_STATUS = "mesa/status"
TOPIC_POSICAO = "mesa/posicao"
TOPIC_STEPS_E = "mesa/steps/esquerda"
TOPIC_STEPS_D = "mesa/steps/direita"
TOPIC_CALIBRACAO = "mesa/calibracao"
TOPIC_COMANDO = "mesa/comando"
TOPIC_PRESET = "mesa/preset"
TOPIC_INFO = "mesa/info"

ser = None
mqtt_client = None


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Conectado ao broker {MQTT_BROKER}:{MQTT_PORT}", flush=True)
        client.publish(TOPIC_DISPONIVEL, "online", retain=True)
        client.subscribe(TOPIC_COMANDO)
        client.subscribe(TOPIC_PRESET)
        print(f"[MQTT] Inscrito em {TOPIC_COMANDO} e {TOPIC_PRESET}", flush=True)
    else:
        print(f"[MQTT] Falha na conexao: rc={rc}", flush=True)


def on_disconnect(client, userdata, rc):
    print(f"[MQTT] Desconectado: rc={rc}", flush=True)


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8").strip()

    if msg.topic == TOPIC_PRESET:
        try:
            alvo = float(payload)
        except ValueError:
            print(f"[MQTT] Preset invalido (nao e numero): {payload}", flush=True)
            return
        if alvo < 84.0 or alvo > 144.0:
            print(f"[MQTT] Preset fora de faixa (84-144cm): {alvo}", flush=True)
            return
        print(f"[MQTT] Preset recebido: {alvo:.1f}cm", flush=True)
        if ser and ser.is_open:
            ser.write(f"IR:{alvo:.1f}\n".encode("utf-8"))
        else:
            print("[MQTT] Serial nao disponivel, preset descartado", flush=True)
        return

    cmd = payload.upper()
    print(f"[MQTT] Comando recebido: {cmd}", flush=True)
    if cmd not in ("SUBIR", "DESCER", "PARAR", "CALIBRAR"):
        print(f"[MQTT] Comando desconhecido ignorado: {cmd}", flush=True)
        return
    if ser and ser.is_open:
        ser.write((cmd + "\n").encode("utf-8"))
    else:
        print("[MQTT] Serial nao disponivel, comando descartado", flush=True)


def processar_linha(linha: str):
    """Parseia uma linha JSON do serial e publica os dados no MQTT."""
    linha = linha.strip()
    if not linha:
        return

    if VERBOSE:
        print(f"[SERIAL] {linha}", flush=True)

    try:
        data = json.loads(linha)
    except json.JSONDecodeError:
        # Linhas de debug do firmware nao-JSON sao ignoradas silenciosamente
        return

    tipo = data.get("tipo")

    if tipo == "erro":
        print(f"[ESP32] ERRO: {data.get('msg', linha)}", flush=True)
        return

    if tipo == "info":
        msg = data.get('msg', '')
        print(f"[ESP32] INFO: {msg}", flush=True)
        mqtt_client.publish(TOPIC_INFO, json.dumps(data))
        return

    # if tipo == "status":
    #     if VERBOSE:
    #         print(f"[STATUS] pos={data.get('posicao')} status={data.get('status')} "
    #               f"stepsE={data.get('stepsE')} stepsD={data.get('stepsD')} "
    #               f"erroSync={data.get('erroSync')} velE={data.get('velE')} velD={data.get('velD')}", flush=True)
    #     if "status" in data:
    #         mqtt_client.publish(TOPIC_STATUS, data["status"])
    #     if "posicao" in data:
    #         mqtt_client.publish(TOPIC_POSICAO, f"{data['posicao']:.2f}")
    #     if "stepsE" in data:
    #         mqtt_client.publish(TOPIC_STEPS_E, str(data["stepsE"]))
    #     if "stepsD" in data:
    #         mqtt_client.publish(TOPIC_STEPS_D, str(data["stepsD"]))

    elif tipo == "calibracao":
        payload = json.dumps({
            "esquerda": data.get("pulsosE"),
            "direita": data.get("pulsosD"),
        })
        mqtt_client.publish(TOPIC_CALIBRACAO, payload)
        mqtt_client.publish(TOPIC_STATUS, "calibrado")
        print(f"[BRIDGE] Calibracao concluida: {payload}", flush=True)


def detectar_porta() -> str:
    """Retorna a porta configurada, ou tenta detectar o ESP32 automaticamente."""
    if SERIAL_PORT:
        return SERIAL_PORT


    candidatos = sorted([
        p.device for p in list_ports.comports()
        if "ACM" in p.device or "USB" in p.device
    ])
    print(f"[BRIDGE] Auto-detectando porta... candidatos: {candidatos}", flush=True)

    for porta in candidatos:
        try:
            with serial.Serial(porta, SERIAL_BAUD, timeout=2) as s:
                for _ in range(20):
                    linha = s.readline().decode("utf-8", errors="replace").strip()
                    if not linha:
                        continue
                    try:
                        data = json.loads(linha)
                        if "tipo" in data:
                            print(f"[BRIDGE] ESP32 detectado em {porta}", flush=True)
                            return porta
                    except json.JSONDecodeError:
                        pass
        except serial.SerialException:
            pass

    fallback = candidatos[0] if candidatos else "/dev/ttyACM0"
    print(f"[BRIDGE] ESP32 nao detectado automaticamente, usando {fallback}", flush=True)
    return fallback


def abrir_serial() -> serial.Serial:
    while True:
        try:
            porta = detectar_porta()
            s = serial.Serial(porta, SERIAL_BAUD, timeout=1)
            print(f"[SERIAL] Porta {porta} aberta a {SERIAL_BAUD} baud", flush=True)
            return s
        except serial.SerialException as e:
            print(f"[SERIAL] Erro ao abrir porta: {e}. Tentando novamente em 5s...", flush=True)
            time.sleep(5)


def main():
    global ser, mqtt_client

    mqtt_client = mqtt.Client()
    mqtt_client.will_set(TOPIC_DISPONIVEL, "offline", retain=True)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_message = on_message

    if MQTT_USER:
        mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)

    print(f"[MQTT] Conectando a {MQTT_BROKER}:{MQTT_PORT}...", flush=True)
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
    mqtt_client.loop_start()

    ser = abrir_serial()

    try:
        while True:
            try:
                linha = ser.readline().decode("utf-8", errors="replace")
                if linha:
                    processar_linha(linha)
            except serial.SerialException as e:
                print(f"[SERIAL] Conexao perdida: {e}", flush=True)
                mqtt_client.publish(TOPIC_DISPONIVEL, "offline", retain=True)
                ser = None
                time.sleep(5)
                ser = abrir_serial()
                mqtt_client.publish(TOPIC_DISPONIVEL, "online", retain=True)

    except KeyboardInterrupt:
        print("\n[BRIDGE] Encerrando...", flush=True)
    finally:
        mqtt_client.publish(TOPIC_DISPONIVEL, "offline", retain=True)
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        if ser and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
