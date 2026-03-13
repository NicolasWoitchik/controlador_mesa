/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <Wire.h>
#include "driver/gpio.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_JSON.h>
#include "Motor.h"
#include "Controles.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3c
#define OLED_RESET -1

bool ledState = 0;
const int ledPin = 2;

Motor esquerda(17, 16, 26);
Motor direita(18, 19, 13);

Controles controles(
  36,
  39,
  34,
  &esquerda,
  &direita);

Adafruit_SSD1306 tela(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

volatile unsigned long direitaUltimoPassoTime = 0;
volatile unsigned long esquerdaUltimoPassoTime = 0;
int tempoDeEspera = 4000;

void IRAM_ATTR contaPassosEsquerda(void) {
  if (micros() - esquerdaUltimoPassoTime > tempoDeEspera) {
    esquerda.step();
    esquerdaUltimoPassoTime = micros();
  }
}

void IRAM_ATTR contaPassosDireita(void) {
  if (micros() - direitaUltimoPassoTime > tempoDeEspera) {
    direita.step();
    direitaUltimoPassoTime = micros();
  }
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println("Inicializando");
  pinMode(ledPin, OUTPUT);

  // Libera pino 13 do modo JTAG/MTCK para uso como GPIO normal
  gpio_reset_pin(GPIO_NUM_13);

  esquerda.begin();
  direita.begin();
  controles.begin();

  attachInterrupt(digitalPinToInterrupt(esquerda.getEncoderPin()), contaPassosEsquerda, CHANGE);
  attachInterrupt(digitalPinToInterrupt(direita.getEncoderPin()), contaPassosDireita, CHANGE);

  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;) {
      Serial.println("Falha ao conectar com a tela");
      delay(1000);
    };  // Don't proceed, loop forever
  }

  tela.clearDisplay();

  tela.setTextSize(1);
  tela.setTextColor(WHITE);
  tela.setCursor(0, 0);
  tela.print("Iniciando...");
  tela.display();
}

void loop() {
  lerComandoSerial();
  controles.loop();
  controles.printStatus(&tela);
  publicarSerial();
}
