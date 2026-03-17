# Controlador Mesa

Firmware Arduino para ESP32 que controla uma mesa regulável em altura (standing desk).

## O que o projeto faz

Controla dois motores DC (esquerdo e direito) que movem a mesa para cima e para baixo via botoes fisicos. A posicao atual da mesa e exibida em um display OLED 128x64. A altura e calculada a partir de pulsos de encoders Hall acoplados aos motores.

- Aste do atuador: 60cm de comprimento
- Resolucao: 6800 pulsos por ciclo completo
- Posicao base (offset): 84cm

## Plataforma e toolchain

- Microcontrolador: **ESP32**
- IDE: **Arduino IDE** (arquivos `.ino`)
- Build output: `build/esp32.esp32.esp32/`
- Para compilar e fazer upload, usar o Arduino IDE ou `arduino-cli`

## Estrutura dos arquivos

```
main/
  main.ino          # Setup, loop, interrupcoes dos encoders
  Motor.cpp/h       # Classe Motor - controla um motor DC com encoder
  Controles.cpp/h   # Classe Controles - le botoes e coordena os dois motores
  http.ino          # Servidor WebSocket (atualmente comentado/desativado)
  data/
    index.html      # Interface web (WebSocket dashboard)
    script.js
    style.css
```

## Hardware - Pinagem ESP32

| Funcao                   | Pino |
|--------------------------|------|
| Motor esquerdo - subir   | 17   |
| Motor esquerdo - descer  | 16   |
| Encoder esquerdo (Hall)  | 26   |
| Motor direito - subir    | 18   |
| Motor direito - descer   | 19   |
| Encoder direito (Hall)   | 13   |
| Botao subir              | 36   |
| Botao descer             | 39   |
| Altura custom            | 34   |
| LED                      | 2    |
| Display OLED (I2C)       | SDA/SCL (Wire) - endereco 0x3C |

## Dependencias (bibliotecas Arduino)

- `Adafruit_GFX`
- `Adafruit_SSD1306`
- `Arduino_JSON`
- `Wire` (builtin)

Dependencias comentadas (para a interface web, quando reativada):
- `AsyncTCP`
- `ESPAsyncWebServer`

## Logica principal

- `Motor`: controla direcao via `analogWrite` nos pinos de subir/descer. Conta passos via `step()` chamado por interrupcao. Calcula posicao em centimetros.
- `Controles`: le os botoes digitais a cada `loop()` e aciona ambos os motores na mesma direcao. Exibe estado no display OLED ("Subindo", "Descendo", "Parado") e a altura atual em cm/mm.
- Interrupcao no pino do encoder esquerdo (pino 26) chama `contaPassosEsquerda()` a cada borda de mudanca (CHANGE).

## Interface web (desativada)

O arquivo `http.ino` contem o servidor WebSocket comentado. A interface em `data/index.html` esta ativa e pronta, mas o backend no ESP32 precisa ser reativado para funcionar. Comandos WebSocket: `sobe`, `desce`, `para`.
