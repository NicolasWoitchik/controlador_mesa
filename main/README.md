# Mesa Controller

Um controlador open-source para mesas reguláveis em altura (standing desks) com dois motores DC sincronizados. Desenvolvido para integrar com Home Assistant via MQTT, com suporte a presets de altura, calibração automática e botões físicos.

---

## O problema

Mesas motorizadas com duas pernas independentes carregam um desafio mecânico sério: os dois motores precisam subir e descer **ao mesmo tempo e na mesma velocidade**. Se um motor se adiantar em relação ao outro, a mesa começa a torcer — e se a torção for severa o suficiente, os motores travam ou se danificam permanentemente.

A maioria dos controladores comerciais resolve isso com hardware proprietário e fechado. Este projeto resolve via software: um algoritmo de sincronização proporcional que lê encoders Hall de ambos os motores e ajusta a velocidade do motor mais adiantado em tempo real, a cada 20ms.

---

## Como funciona

```
Comando (botão físico ou MQTT)
        ↓
   Controles — orquestra o movimento
        ↓
Motor Esquerdo ←── Sincronização Proporcional (20ms) ──→ Motor Direito
      ↓                                                         ↓
 PWM + Encoder Hall                                    PWM + Encoder Hall
```

### Algoritmo de sincronização

A cada 20ms, o controlador:

1. Normaliza os pulsos de cada encoder (posição relativa entre 0 e 1)
2. Calcula o **erro fracionário** entre os dois lados
3. Reduz a velocidade do motor mais adiantado proporcionalmente ao erro
4. Se o erro ultrapassar 400 pulsos → **parada de emergência** + recuperação automática

```
Erro = steps_esquerda_normalizado - steps_direita_normalizado

|Erro| > 400 → parada de emergência
Erro > limiar → reduz velocidade do motor esquerdo
Erro < -limiar → reduz velocidade do motor direito
```

### Calibração automática

Na primeira execução (ou sob comando), o controlador:

1. Desce até detectar travamento mecânico → zera a contagem de pulsos
2. Sobe até detectar travamento → registra o total de pulsos percorridos
3. Salva os valores para converter pulsos ↔ centímetros em todas as operações seguintes

### Presets de altura

Envie uma altura em cm via MQTT (`mesa/preset`) e o controlador move a mesa automaticamente até atingir o alvo com tolerância de ±0.3 cm. O botão físico cancela o movimento a qualquer momento.

---

## Hardware necessário

| Componente                   | Descrição                                      |
| ---------------------------- | ---------------------------------------------- |
| Raspberry Pi 3 (ou superior) | Computador de controle (implementação atual)   |
| ESP32                        | Alternativa embarcada (implementação original) |
| Driver BTS7960 × 2           | Ponte H para controle de motores DC            |
| Motores DC com encoder Hall  | Atuadores lineares das pernas da mesa          |
| Botões momentâneos × 3       | Subir, Descer, Altura customizada              |

**Range de altura suportado:** 84 cm – 144 cm
**Tolerância de preset:** ±0.3 cm
**Pulsos por curso completo (60 cm):** ~6800

---

## Implementações

O projeto evoluiu por duas plataformas. Ambas usam o mesmo algoritmo de sincronização.

### `rpi3/` — Implementação atual

Roda em um **Raspberry Pi 3** conectado diretamente aos drivers via GPIO. Publica e consome MQTT de forma nativa, sem intermediários.

| Item          | Detalhe                           |
| ------------- | --------------------------------- |
| Linguagem     | TypeScript (Node.js 18 LTS)       |
| GPIO/PWM      | pigpio (hardware PWM, 1000 Hz)    |
| Comunicação   | MQTT nativo                       |
| Persistência  | `calibracao.json` no filesystem   |
| Inicialização | Serviço systemd com auto-reinício |

### `esp32/` — Implementação original

Roda em um microcontrolador **ESP32** diretamente conectado aos drivers. A comunicação externa é feita via porta serial → JSON, com uma ponte externa para MQTT.

| Item         | Detalhe                        |
| ------------ | ------------------------------ |
| Linguagem    | C++ (Arduino)                  |
| Display      | OLED 128×64 (Adafruit SSD1306) |
| Comunicação  | Serial (115200 baud) → JSON    |
| Persistência | Flash NVS do ESP32             |

> A migração para o RPi3 eliminou a ponte serial (que era um ponto único de falha), simplificou a infraestrutura e viabilizou integração direta com brokers MQTT como o do Home Assistant.

---

## Tecnologias

- **TypeScript / Node.js** — lógica de controle principal (rpi3)
- **pigpio** — acesso ao hardware PWM e GPIO do Raspberry Pi
- **MQTT** — protocolo de comunicação com automações externas
- **systemd** — execução como serviço com reinício automático
- **C++ / Arduino** — implementação embarcada no ESP32
- **Adafruit SSD1306** — display OLED para feedback visual (ESP32)

---

## Tópicos MQTT

| Tópico                | Direção | Valores                                |
| --------------------- | ------- | -------------------------------------- |
| `mesa/comando`        | entrada | `SUBIR`, `DESCER`, `PARAR`, `CALIBRAR` |
| `mesa/preset`         | entrada | altura em cm (ex: `110.0`)             |
| `mesa/status`         | saída   | `subindo`, `descendo`, `parado`        |
| `mesa/posicao`        | saída   | posição atual em cm (ex: `110.25`)     |
| `mesa/steps/esquerda` | saída   | pulsos do encoder esquerdo             |
| `mesa/steps/direita`  | saída   | pulsos do encoder direito              |
| `mesa/calibracao`     | saída   | `{"pulsosE": 6800, "pulsosD": 6810}`   |
| `mesa/info`           | saída   | eventos e diagnósticos                 |
| `mesa/disponivel`     | saída   | `online` / `offline` (LWT, retain)     |

---

## Instalação rápida (RPi3)

```bash
# Instalar pigpio
sudo apt install pigpio
sudo systemctl enable pigpiod --now

# Instalar Node.js 18
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs

# Clonar e instalar dependências
git clone https://github.com/seu-usuario/controlador-mesa
cd controlador-mesa/rpi3
npm install
npm run build

# Rodar em modo desenvolvimento
npm run dev

# Instalar como serviço (produção)
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mesa-rpi --now

# Acompanhar logs
sudo journalctl -u mesa-rpi -f
```

Consulte [rpi3/README.md](rpi3/README.md) para instruções detalhadas de pinagem, variáveis de ambiente e calibração.

---

## Pinagem (RPi3, GPIO BCM)

| Função                    | GPIO         | Pino Físico |
| ------------------------- | ------------ | ----------- |
| Motor Esq — subir (RPWM)  | 12 (HW PWM0) | 32          |
| Motor Esq — descer (LPWM) | 18 (HW PWM0) | 12          |
| Encoder esquerdo          | 5            | 29          |
| Motor Dir — subir (RPWM)  | 13 (HW PWM1) | 33          |
| Motor Dir — descer (LPWM) | 19 (HW PWM1) | 35          |
| Encoder direito           | 6            | 31          |
| Botão subir               | 20           | 38          |
| Botão descer              | 21           | 40          |
| Botão altura custom       | 26           | 37          |

---

## Estrutura do projeto

```
controlador_mesa/
├── rpi3/               # Implementação atual (TypeScript / RPi3)
│   ├── src/
│   │   ├── index.ts    # Entry point e loop de publicação MQTT
│   │   ├── Motor.ts    # Controle PWM + leitura de encoder
│   │   ├── Controles.ts  # Sincronização, calibração, presets
│   │   ├── MqttClient.ts # Cliente MQTT com reconexão automática
│   │   └── config.ts   # Pinos, constantes e configuração
│   └── systemd/
│       └── mesa-rpi.service
│
└── esp32/              # Implementação original (C++ / Arduino)
    ├── main.ino
    ├── Motor.cpp / Motor.h
    ├── Controles.cpp / Controles.h
    └── serial_bridge.ino
```

---

## Licença

MIT — use, modifique e distribua livremente.
