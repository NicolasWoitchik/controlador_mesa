# Controlador Mesa — RPi3

Firmware da standing desk migrado do ESP32 para **Raspberry Pi 3** em Node.js + TypeScript.

Conecta diretamente ao broker MQTT — sem bridge serial intermediária.

## Estrutura de arquivos

```
rpi3/
├── package.json
├── tsconfig.json
├── calibracao.json        # gerado em runtime (gitignore)
├── src/
│   ├── index.ts           # entry point, orquestração e loop de publicação
│   ├── config.ts          # pinos e constantes
│   ├── Motor.ts           # controle PWM BTS7960 + leitura de encoder
│   ├── Controles.ts       # lógica de sync, calibração, preset e botões
│   └── MqttClient.ts      # pub/sub MQTT com LWT
├── dist/                  # output compilado (gitignore)
└── systemd/
    └── mesa-rpi.service   # unit systemd
```

## Tabela de pinos

Mapeamento do ESP32 para RPi GPIO (numeração BCM):

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
> O pino inativo sempre recebe `digitalWrite(0)` para garantir LOW real na BTS7960.

## Tópicos MQTT

| Tópico | Direção | Payload |
|---|---|---|
| `mesa/disponivel` | pub | `online` / `offline` (retain) |
| `mesa/status` | pub | `subindo` / `descendo` / `parado` |
| `mesa/posicao` | pub | ex: `110.25` (cm) |
| `mesa/steps/esquerda` | pub | número inteiro |
| `mesa/steps/direita` | pub | número inteiro |
| `mesa/calibracao` | pub | `{"pulsosE":6800,"pulsosD":6810}` |
| `mesa/info` | pub | JSON com eventos e erros |
| `mesa/comando` | sub | `SUBIR` / `DESCER` / `PARAR` / `CALIBRAR` |
| `mesa/preset` | sub | altura em cm ex: `110.0` (range: 84–144) |

## Setup

### 1. Pré-requisitos

```bash
# pigpiod — daemon obrigatório para acesso ao GPIO
sudo apt install pigpio
sudo systemctl enable pigpiod
sudo systemctl start pigpiod
```

### 2. Node.js

```bash
# Node 18 LTS (recomendado — pigpio é addon nativo compilado com node-gyp)
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs
```

> Se estiver usando Node 24 e `npm install` falhar ao compilar o `pigpio`:
> ```bash
> curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
> nvm install 18 && nvm use 18
> ```

### 3. Instalar e compilar

```bash
cd /opt/controlador_mesa/rpi3
npm install
npm run build      # compila TypeScript → dist/
```

### 4. Serviço systemd

```bash
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mesa-rpi
sudo systemctl start mesa-rpi

# Acompanhar logs em tempo real
sudo journalctl -u mesa-rpi -f
```

Edite as variáveis de ambiente no arquivo `systemd/mesa-rpi.service` antes de instalar:

```ini
Environment="MESA_MQTT_BROKER=192.168.15.20"
Environment="MESA_MQTT_PORT=1883"
Environment="MESA_MQTT_USER=mesa_pc"
Environment="MESA_MQTT_PASS=mesa"
```

## Desenvolvimento

```bash
npm run dev   # executa via tsx sem compilar
```

## Testes manuais

```bash
# Verificar pigpiod
sudo systemctl status pigpiod

# Smoke test de PWM no GPIO 12 (sem motor conectado)
sudo node -e "
const {Gpio} = require('pigpio');
const p = new Gpio(12, { mode: Gpio.OUTPUT });
p.hardwarePwmWrite(1000, 500000);
setTimeout(() => { p.hardwarePwmWrite(1000, 0); process.exit(); }, 3000);
"

# Comandos via MQTT
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m SUBIR
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m PARAR
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/preset  -m 110.0
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m CALIBRAR

# Monitorar todos os tópicos
mosquitto_sub -h 192.168.15.20 -u mesa_pc -P mesa -t 'mesa/#' -v
```
