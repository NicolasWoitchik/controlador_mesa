# Controlador Mesa — RPi3 (Node.js + TypeScript)

Migração 100% do firmware ESP32 para Raspberry Pi 3 em TypeScript.

## Tabela de pinos (ESP32 → RPi GPIO BCM)

| Função | ESP32 | RPi GPIO (BCM) | Pino físico |
|---|---|---|---|
| BTS7960 Motor Esq RPWM (subir) | 17 | GPIO 12 (HW PWM0) | Pin 32 |
| BTS7960 Motor Esq LPWM (descer) | 16 | GPIO 18 (HW PWM0) | Pin 12 |
| Encoder esq | 26 | GPIO 5 | Pin 29 |
| BTS7960 Motor Dir RPWM (subir) | 18 | GPIO 13 (HW PWM1) | Pin 33 |
| BTS7960 Motor Dir LPWM (descer) | 19 | GPIO 19 (HW PWM1) | Pin 35 |
| Encoder dir | 13 ⚠️ | GPIO 6 | Pin 31 |
| Botão subir | 36 | GPIO 20 | Pin 38 |
| Botão descer | 39 | GPIO 21 | Pin 40 |
| Altura custom | 34 | GPIO 26 | Pin 37 |

> GPIO 12/18 compartilham canal PWM0; GPIO 13/19 compartilham PWM1.
> O pino inativo deve receber `digitalWrite(0)` — nunca `hardwarePwmWrite(0)`.

## Setup no RPi3

```bash
# 1. Habilitar pigpiod
sudo apt install pigpio
sudo systemctl enable pigpiod
sudo systemctl start pigpiod

# 2. Instalar Node.js (se necessário)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs

# ATENÇÃO: pigpio é addon nativo (node-gyp).
# Se falhar com Node 24, usar Node 18:
#   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
#   nvm install 18 && nvm use 18

# 3. Instalar dependências e compilar
cd /opt/controlador_mesa/rpi3
npm install
npm run build

# 4. Instalar serviço systemd
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mesa-rpi
sudo systemctl start mesa-rpi

# 5. Verificar logs
sudo journalctl -u mesa-rpi -f
```

## Desenvolvimento (sem compilar)

```bash
npm run dev        # roda via tsx diretamente
```

## Testes manuais

```bash
# Verificar pigpiod
sudo systemctl status pigpiod

# Smoke test de PWM (sem motor conectado)
sudo node -e "
const {Gpio} = require('pigpio');
const p = new Gpio(12, { mode: Gpio.OUTPUT });
p.hardwarePwmWrite(1000, 500000);
setTimeout(() => { p.hardwarePwmWrite(1000, 0); process.exit(); }, 3000);
"

# Enviar comandos via MQTT
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m SUBIR
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m PARAR
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/preset  -m 110.0
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m CALIBRAR

# Monitorar tudo
mosquitto_sub -h 192.168.15.20 -u mesa_pc -P mesa -t 'mesa/#' -v
```

## Tópicos MQTT

| Tópico | Direção | Payload |
|---|---|---|
| mesa/disponivel | pub | `online` / `offline` |
| mesa/status | pub | `subindo` / `descendo` / `parado` |
| mesa/posicao | pub | ex: `110.25` |
| mesa/steps/esquerda | pub | número |
| mesa/steps/direita | pub | número |
| mesa/calibracao | pub | `{"pulsosE":6800,"pulsosD":6810}` |
| mesa/info | pub | JSON com mensagens de evento |
| mesa/comando | sub | `SUBIR` / `DESCER` / `PARAR` / `CALIBRAR` |
| mesa/preset | sub | altura em cm ex: `110.0` |

## Estrutura de arquivos

```
rpi3/
├── package.json
├── tsconfig.json
├── calibracao.json       # gerado em runtime (gitignore)
├── src/
│   ├── index.ts          # entry point
│   ├── config.ts         # pinos e constantes
│   ├── Motor.ts          # PWM + encoder
│   ├── Controles.ts      # lógica de controle
│   └── MqttClient.ts     # pub/sub MQTT
├── dist/                 # compilado (gitignore)
└── systemd/
    └── mesa-rpi.service
```
