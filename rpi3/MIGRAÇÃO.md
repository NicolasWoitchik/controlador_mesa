# Migração pigpio → pigpio-client (2026-03-17)

Documentação da migração realizada para tornar o firmware compatível com Node.js moderno e Debian 13 (Trixie) no Raspberry Pi 3.

---

## Contexto / Problema

O firmware usava o pacote npm `pigpio` — um addon nativo C++ que:

- Só compila com **Node.js ≤ 16** (EOL desde abril 2024)
- Não possui pacote `pigpio` disponível via `apt` no **Debian 13 (Trixie) arm64**
- Falhava com `TypeError: pigpio.gpioInitialise is not a function` no Node 24

---

## Solução adotada

Substituição por **`pigpio-client`**: cliente JavaScript puro que se comunica com o daemon `pigpiod` via socket TCP (porta 8888). Sem addon nativo, funciona com qualquer versão do Node.

| | Antes | Depois |
|---|---|---|
| Pacote | `pigpio` ^3.3.1 | `pigpio-client` ^1.5.2 |
| Tipo | Addon nativo C++ | JavaScript puro |
| Node suportado | ≤ 16 | ≥ 20 |
| Compilação nativa | `npm rebuild` necessário | Não existe |
| Comunicação com daemon | Direta (in-process) | TCP socket localhost:8888 |

---

## Passo 1 — Compilar pigpiod do fonte

O pacote `pigpio` não está disponível via `apt` no Debian 13 Trixie arm64. É necessário compilar:

```bash
sudo apt-get install -y git build-essential
cd /tmp
git clone https://github.com/joan2937/pigpio.git
cd pigpio
make
sudo make install
```

Resultado: binários `pigpiod` e `pigs` em `/usr/local/bin/`.

Para verificar:
```bash
pigpiod -v   # deve imprimir 79 (ou superior)
```

---

## Passo 2 — Alterações no código

### `package.json`
- Removido: `pigpio`
- Adicionado: `pigpio-client`, `dotenv`
- `engines.node` atualizado para `>=20.0.0`

### `src/index.ts`
- Removido `import * as pigpio from 'pigpio'` e `pigpio.terminate()`
- Adicionado `import 'dotenv/config'` (carrega `.env` automaticamente)
- Nova função `conectarPigpio()` — conecta ao daemon via evento `'connected'`
- Instância `pig` passada para `Motor` e `Controles`
- `motorEsq.begin()` / `motorDir.begin()` / `controles.begin()` passaram a ser `await`
- Shutdown: adicionado safety net `setTimeout(() => process.exit(0), 3000).unref()` para evitar travamento no disconnect MQTT
- Intervalo de publicação guardado em `pubInterval` para ser limpo no shutdown

### `src/Motor.ts`
- `constructor` recebe `pig: any` (instância pigpio-client)
- `begin()` virou `async`:
  - `modeSet('output')` / `write(0)` para pinos de motor
  - `modeSet('input')` / `pullUpDown(2)` / `glitchSet(1000)` para encoder
  - `notify(callback)` substitui `on('alert', callback)` do pigpio
- `subir()` / `descer()` / `parar()` / `setVelocidade()` — permanecem síncronos (fire-and-forget com `.catch`)
  - `hardwarePwmWrite(freq, duty)` → `hardwarePWM(freq, duty)`
  - `digitalWrite(0)` → `write(0)`
- `teardown()`: `endNotify()` envolto em `try/catch` (retorna `undefined`, não Promise)

### `src/Controles.ts`
- `constructor` recebe `pig: any`
- `begin()` virou `async`:
  - Botões configurados com `modeSet('input')` / `pullUpDown(1)` (PUD_DOWN)
  - Leitura inicial com `await read()` para estado correto desde o início
  - `notify()` nos botões substitui `digitalRead()` a cada 5ms — estado cacheado em `_btnSubirLevel` / `_btnDescerLevel`
- `_isBotaoSubir()` / `_isBotaoDescer()` — leem variável local (síncronos, sem IO)
- `teardown()`: `endNotify()` envolto em `try/catch`

---

## Passo 3 — Arquivo `.env`

Criado `.env` na raiz do projeto para credenciais MQTT locais:

```ini
MESA_MQTT_BROKER=192.168.15.20
MESA_MQTT_PORT=1883
MESA_MQTT_USER=mesa_pc
MESA_MQTT_PASS=mesa
```

O arquivo está no `.gitignore`. Em produção (systemd), as variáveis continuam sendo definidas no service file.

---

## Passo 4 — Systemd

### Novo: `systemd/pigpiod.service`

```ini
[Unit]
Description=Pigpio GPIO daemon
After=network.target

[Service]
Type=forking
PIDFile=/var/run/pigpio.pid
ExecStart=/usr/local/bin/pigpiod
ExecStop=/bin/kill -s SIGINT $MAINPID
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

### Atualizado: `systemd/mesa-rpi.service`

Correções aplicadas:
- `User=root` → `User=nicolas`
- `WorkingDirectory=/opt/...` → caminho real do projeto
- `ExecStart=/usr/bin/node` → caminho do Node via nvm

### Instalação

```bash
sudo cp systemd/pigpiod.service /etc/systemd/system/
sudo cp systemd/mesa-rpi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable pigpiod mesa-rpi
sudo systemctl start pigpiod
sudo systemctl start mesa-rpi
```

---

## Resultado

```
● pigpiod.service   — active (running)
● mesa-rpi.service  — active (running)

[pigpio] Conectado ao pigpiod
[MQTT] Conectado a 192.168.15.20:1883
[Init] Pronto.
```

Shutdown limpo com `EXIT: 0` via SIGTERM.
