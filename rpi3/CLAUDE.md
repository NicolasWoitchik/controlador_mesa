# CLAUDE.md — Controlador Mesa RPi3

Contexto para Claude Code. Leia isto antes de qualquer tarefa.

---

## O que é este projeto

Firmware de controle de uma **standing desk motorizada** rodando em **Raspberry Pi 3**.
É uma porta do firmware original em C++ (ESP32 + FreeRTOS) para **Node.js + TypeScript**.

O RPi3 substitui o ESP32 + bridge serial: conecta diretamente ao broker MQTT via rede, controla dois motores lineares (atuadores) por PWM de hardware usando a biblioteca `pigpio`, e lê encoders ópticos de cada motor para sincronização e cálculo de posição.

---

## Hardware

### Componentes principais

- **Raspberry Pi 3 Model B**
- **2× atuadores lineares** com motor DC interno
- **2× drivers BTS7960** (ponte H dupla) — um por motor
- **2× encoders ópticos** — um por motor
- **2× botões físicos** (subir / descer)

### Mapeamento de pinos (numeração BCM)

| Função | GPIO BCM | Pino físico | Obs |
|---|---|---|---|
| Motor Esq — RPWM (subir) | 12 | 32 | HW PWM0 |
| Motor Esq — LPWM (descer) | 18 | 12 | HW PWM0 |
| Encoder esquerda | 5 | 29 | pull-up interno |
| Motor Dir — RPWM (subir) | 13 | 33 | HW PWM1 |
| Motor Dir — LPWM (descer) | 19 | 35 | HW PWM1 |
| Encoder direita | 6 | 31 | pull-up interno |
| Botão subir | 20 | 38 | pull-down interno |
| Botão descer | 21 | 40 | pull-down interno |
| Botão custom (altura) | 26 | 37 | pull-down interno |

> GPIO 12/18 compartilham canal PWM0; GPIO 13/19 compartilham PWM1.
> O pino inativo de cada par RPWM/LPWM deve receber `digitalWrite(0)` — nunca `hardwarePwmWrite(0)`.

### BTS7960 — lógica de acionamento

| Movimento | RPWM | LPWM |
|---|---|---|
| Subir | PWM (duty variável) | LOW (digitalWrite 0) |
| Descer | LOW (digitalWrite 0) | PWM (duty variável) |
| Parar | LOW | LOW |

---

## Arquitetura do software

```
src/
├── index.ts        — entry point: inicialização, loop de publicação MQTT (100ms), shutdown graceful
├── config.ts       — todos os pinos, constantes físicas e tunning em um único lugar
├── Motor.ts        — classe Motor: controle PWM BTS7960 + leitura de encoder + cálculo de posição
├── Controles.ts    — lógica de alto nível: sync, calibração, preset, botões físicos, recuperação
└── MqttClient.ts   — pub/sub MQTT com LWT (Last Will Testament)
```

### Fluxo de controle

1. `index.ts` inicializa motores, MQTT e `Controles`, depois entra nos três loops:
   - **Loop de controle** (`Controles._loopStep`, 5ms): lê botões físicos e executa comando ativo
   - **Loop de sincronização** (`Controles._sincronizarMotores`, 20ms): controle proporcional de velocidade para manter os dois motores em fase
   - **Loop de publicação** (`index.ts`, 100ms): publica status/posição via MQTT com throttle adaptativo

2. Comandos MQTT chegam via `onComando()` e atualizam `_mqttComando` em `Controles`.

3. Preset (`irPara(cm)`) ativa `_modoPreset`: o loop de controle guia os motores até o alvo e para automaticamente.

4. Calibração (`calibrar()`): desce até travar → zera encoders → sobe até travar → salva pulsos em `calibracao.json`.

### Cálculo de posição

```
posicao_cm = (steps / pulsosCalibrados) × COMPRIMENTO_ASTE + ALTURA_BASE_CM
```

- `ALTURA_BASE_CM = 84.0 cm` — mesa totalmente descida
- `COMPRIMENTO_ASTE = 60.0 cm` — curso total do atuador
- `pulsosCalibrados` — medido em runtime pelo processo de calibração (aprox. 6800)

### Encoder — direction-aware

O encoder conta pulsos em ambas as direções. A lógica correta:

```ts
if (direcao === 'SUBIR')  steps = Math.min(steps + 1, pulsosCalibrados);
if (direcao === 'DESCER') steps = Math.max(steps - 1, 0);
```

Isso garante que `steps` nunca fica negativo nem ultrapassa `pulsosCalibrados`.

### Sincronização proporcional

O erro é calculado como diferença de progresso fracionário entre os dois motores:

```
erro = (stepsE - stepsEInicio) / pulsosE  −  (stepsD - stepsDInicio) / pulsosD
```

O motor adiantado tem sua velocidade reduzida proporcionalmente (`GANHO = 1500`).
Se `|erro_em_pulsos| > EMERGENCIA_PULSOS (400)`, os dois motores param e entra em modo de recuperação (`_recuperando`).

---

## Tópicos MQTT

| Tópico | Direção | Retain | Payload |
|---|---|---|---|
| `mesa/disponivel` | pub | sim | `online` / `offline` |
| `mesa/status` | pub | sim | `subindo` / `descendo` / `parado` |
| `mesa/posicao` | pub | sim | ex: `110.25` (cm) |
| `mesa/steps/esquerda` | pub | não | número inteiro |
| `mesa/steps/direita` | pub | não | número inteiro |
| `mesa/calibracao` | pub | não | `{"pulsosE":6800,"pulsosD":6810}` |
| `mesa/info` | pub | não | JSON com eventos/erros |
| `mesa/comando` | sub | — | `SUBIR` / `DESCER` / `PARAR` / `CALIBRAR` |
| `mesa/preset` | sub | — | altura em cm, ex: `110.0` (range: 84–144) |

Credenciais configuradas no systemd service: broker `192.168.15.20:1883`, user `mesa_pc`, pass `mesa`.

---

## Dependências importantes

- **`pigpio`** — addon nativo (compilado com `node-gyp`). **Só funciona no RPi com `pigpiod` rodando.**
  - Requer Node 18 LTS. Node 20+ pode falhar na compilação nativa.
  - No desktop (x86/Linux), o módulo carrega mas lança `"Module did not self-register"` ao tentar usar GPIO.
  - `pigpio.terminate()` é chamado no shutdown para liberar recursos do daemon.
- **`mqtt`** — cliente MQTT.js puro, sem dependências nativas.
- **`typescript` + `tsx`** — compilação e modo dev.

---

## Como rodar

### Pré-requisitos no RPi

```bash
# pigpiod — obrigatório
sudo apt install pigpio
sudo systemctl enable pigpiod
sudo systemctl start pigpiod

# Node 18 LTS
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs
```

### Instalar e compilar

```bash
cd /opt/controlador_mesa/rpi3
npm install          # compila addon nativo pigpio aqui
npm run build        # TypeScript → dist/
```

### Rodar diretamente (sem systemd)

```bash
sudo node dist/index.js
# ou em dev (sem compilar):
sudo npx tsx src/index.ts
```

> `sudo` é necessário porque `pigpiod` requer acesso root ao GPIO.

### Serviço systemd

```bash
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mesa-rpi
sudo systemctl start mesa-rpi
sudo journalctl -u mesa-rpi -f   # logs em tempo real
```

---

## Estado atual do código

### Bugs corrigidos (sessão 2026-03-17)

Foram corrigidos 8 bugs identificados na análise comparativa com o firmware ESP32 original:

1. **[CRÍTICO] Encoder direction-aware** (`Motor.ts:52`) — encoder agora decrementa ao descer e incrementa ao subir, com clamp em `[0, pulsosCalibrados]`. Antes: `_steps++` sempre, independente da direção.

2. **[CRÍTICO] Timeout em `_waitUntilStalled`** (`Controles.ts`) — Promise agora rejeita após 90s se os motores não travarem. Antes: Promise pendente para sempre em caso de falha de encoder durante calibração. O `calibrar()` tem `finally { this._calibrando = false }` para garantir reset mesmo em erro.

3. **[MÉDIO] Hardcoded `6800` no erro de sync** (`Controles.ts`) — substituído pela média dos pulsos calibrados reais dos dois motores. Antes: `Math.round(erro * 6800)` usava valor default fixo.

4. **[MÉDIO] Guard divisão por zero** (`Controles.ts`) — se `pulsosCalibrados` for 0 (estado inválido pós-falha), `_sincronizarMotores` aborta antes de calcular. Antes: produzia `Infinity`/`NaN`.

5. **[MÉDIO] Dead code removido** (`Controles.ts`) — removidos `_ultimaDirecao`, `_ultimaPosicao`, `_ultimaPublicacao`. Esses campos eram declarados mas nunca lidos; o throttle de publicação já existe em `index.ts`.

6. **[MÉDIO] Guard shutdown duplo** (`index.ts`) — flag `isShuttingDown` previne dupla execução se SIGTERM e SIGINT chegarem simultaneamente.

7. **[MENOR] `publishAvailable(false)` redundante removido** (`index.ts`) — `mqttClient.disconnect()` já publica `offline` internamente. A chamada anterior causava dois publishes `offline`.

8. **[MENOR] `retain: true` em status e posição** (`MqttClient.ts`) — `publishStatus()` e `publishPosicao()` agora publicam com `retain: true`. Antes: clientes que conectassem com a mesa parada não recebiam dados até o próximo ciclo.

---

## Troubleshooting comum

### `TypeError: pigpio.gpioInitialise is not a function`
A API do módulo Node.js expõe `initialize` (não `gpioInitialise`). Se aparecer esse erro, verifique:
1. O `pigpiod` está rodando? → `sudo systemctl status pigpiod`
2. O `npm install` compilou o addon nativo no RPi? → `npm rebuild`
3. A versão do Node é 18? → `node --version`

### `Module did not self-register`
O addon `.node` foi compilado para outra plataforma ou versão de Node. Solução: `npm rebuild` no RPi com a versão correta de Node.

### Encoder não conta / contagem errada
- Verificar se `pigpiod` está rodando (sem ele o `glitchFilter` e alerts não funcionam)
- Verificar conexão física do encoder (pull-up interno ativo em GPIO 5 e 6)
- Executar calibração antes de usar preset

### Mesa dessincroniza durante movimento
- Verificar se `erroSync` nos logs está abaixo de 400 pulsos
- Se `_recuperando` ativar com frequência, o encoder de um lado pode estar falhando

---

## Comandos úteis de diagnóstico

```bash
# Status do serviço
sudo systemctl status mesa-rpi

# Monitorar todos os tópicos MQTT em tempo real
mosquitto_sub -h 192.168.15.20 -u mesa_pc -P mesa -t 'mesa/#' -v

# Enviar comandos manualmente
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m CALIBRAR
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/preset  -m 110.0
mosquitto_pub -h 192.168.15.20 -u mesa_pc -P mesa -t mesa/comando -m PARAR

# Smoke test de PWM (sem motor — só verifica que pigpio funciona)
sudo node -e "
const {Gpio} = require('pigpio');
const p = new Gpio(12, { mode: Gpio.OUTPUT });
p.hardwarePwmWrite(1000, 500000);
setTimeout(() => { p.hardwarePwmWrite(1000, 0); process.exit(); }, 2000);
"
```
