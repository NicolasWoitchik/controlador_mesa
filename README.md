# Controlador Mesa

Sistema de controle para mesa regulável em altura (standing desk) com dois motores DC sincronizados.

## O que é isso?

A mesa possui dois motores DC independentes — um em cada perna — que precisam subir e descer **ao mesmo tempo e na mesma velocidade**, caso contrário a mesa torce e os motores podem travar ou se danificar.

Este projeto implementa:
- Controle de velocidade via PWM para dois motores com driver BTS7960 (ponte H)
- Leitura de encoders Hall para medir posição
- Algoritmo de sincronização proporcional que corrige desvios em tempo real
- Calibração automática (detecta os limites físicos e calcula o curso total)
- Presets de altura (ir para uma altura específica em centímetros)
- Botões físicos na mesa para subir/descer manualmente

## Por que duas pastas?

O projeto passou por uma migração de hardware. Ambas as implementações resolvem o mesmo problema com o mesmo algoritmo, mas em plataformas diferentes:

### `esp32/` — Implementação original (C++ / Arduino)

Roda em um microcontrolador **ESP32** conectado diretamente ao driver dos motores. A comunicação com o mundo externo (Home Assistant, automações) é feita via **porta serial**, com uma ponte externa que traduz para MQTT.

- Linguagem: C++ (Arduino IDE)
- Comunicação: Serial (115200 baud) → JSON
- Interface: Display OLED 128x64 embutido
- Armazenamento: Flash NVS do próprio ESP32

Limitação principal: a ponte serial era um ponto fraco da arquitetura — um processo Python separado precisava rodar em outra máquina para fazer a tradução serial↔MQTT.

### `rpi3/` — Implementação atual (TypeScript / Node.js)

Roda em um **Raspberry Pi 3** conectado diretamente ao driver dos motores via GPIO. A comunicação com o mundo externo é feita diretamente via **MQTT**, sem intermediários.

- Linguagem: TypeScript (Node.js 18 LTS)
- Comunicação: MQTT nativo (publica/subscreve tópicos `mesa/*`)
- Armazenamento: arquivo `calibracao.json` no filesystem
- Inicialização: serviço `systemd` com auto-reinício

A migração para o RPi3 eliminou a ponte serial, simplificou a infraestrutura e permitiu integração direta com o broker MQTT do Home Assistant.

## Arquitetura

Ambas as implementações compartilham a mesma estrutura lógica:

```
Comando (botão físico ou MQTT/Serial)
    ↓
Controles — orquestra os dois motores
    ↓
Motor (esquerdo) ←── Sincronização proporcional (20ms) ──→ Motor (direito)
    ↓                                                              ↓
PWM + encoder                                              PWM + encoder
```

A sincronização compara os pulsos normalizados de cada encoder e ajusta a velocidade do motor mais adiantado para corrigir o desvio.

## Tópicos MQTT (rpi3)

| Tópico | Direção | Descrição |
|--------|---------|-----------|
| `mesa/comando` | entrada | `SUBIR`, `DESCER`, `PARAR`, `CALIBRAR` |
| `mesa/preset` | entrada | altura alvo em cm (ex: `110.0`) |
| `mesa/status` | saída | `subindo`, `descendo`, `parado` |
| `mesa/posicao` | saída | posição atual em cm |
| `mesa/disponivel` | saída | `online` / `offline` (LWT) |

## Hardware

- Driver de motor: BTS7960 (ponte H dupla)
- Encoders: sensores Hall nos motores
- Range de altura: 84–144 cm
- Tolerância de preset: ±0.3 cm

## Setup rápido (RPi3)

```bash
# Instalar dependências
sudo apt install pigpio
sudo systemctl enable pigpiod --now
npm install

# Rodar em desenvolvimento
npm run dev

# Instalar como serviço
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl enable mesa-rpi --now
```

Veja [rpi3/README.md](rpi3/README.md) para instruções detalhadas de configuração e pinagem.
