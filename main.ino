/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-websocket-server-arduino/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <WiFi.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_JSON.h>
#include "Motor.h"
#include "Controles.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3c
#define OLED_RESET -1
// Replace with your network credentials
const char *ssid = "WOITCHIK";
const char *password = "nicolastalita97";

bool ledState = 0;
const int ledPin = 2;

Motor esquerda(18, 19, 14);
Motor direita(17, 16, 26);

Controles controles(
  36,
  34,
  39,
  &esquerda,
  &direita);

Adafruit_SSD1306 tela(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// const int motorEsquerdaDirecao1Output = 18;
// const int motorEsquerdaDirecao2Output = 19;
// const int motorDireitaDirecao1Output = 16;
// const int motorDireitaDirecao2Output = 17;
// const int motorEsquerdaHall1Input = 13;
// const int motorEsquerdaHall2Input = 14;
// const int motorDireitaHall1Input = 27;
// const int motorDireitaHall2Input = 26;
// int motorEsquerdaHALLInput = 2;
// int motorDireitaHALLInput = 0;
// long motorEsquerdaPosicaoHALL = 0;
// long motorDireitaPosicaoHALL = 0;
// int motorEsquerdaVelocidade = 255;
// int motorDireitaVelocidade = 255;

unsigned long lastStepTime = 0; // Time stamp of last pulse
int trigDelay = 4000;

void countStepsLeft(void) {
  if(micros()-lastStepTime > trigDelay){
    esquerda.step();
    lastStepTime = micros();
  }
}

void countStepsRight(void) {
  if(micros()-lastStepTime > trigDelay){
    direita.step();
    lastStepTime = micros();
  }
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  esquerda.begin();
  direita.begin();
  controles.begin();

  // attachInterrupt(digitalPinToInterrupt(esquerda.getEncoderPin()), countStepsLeft, RISING);
  attachInterrupt(digitalPinToInterrupt(direita.getEncoderPin()), countStepsRight, RISING);


  pinMode(ledPin, OUTPUT);

  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;  // Don't proceed, loop forever
  }

  tela.clearDisplay();

  tela.setTextSize(1);
  tela.setTextColor(WHITE);
  tela.setCursor(0, 0);
  tela.print("Iniciando...");
  tela.display();

  // digitalWrite(ledPin, LOW);

  // Connect to Wi-Fi
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   Serial.println("Connecting to WiFi..");
  // }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
  tela.clearDisplay();
  tela.setCursor(0, 0);
  tela.print("IP: ");
  tela.println(WiFi.localIP());
  tela.display();
}

void loop() {
  controles.loop();
  controles.printStatus(&tela);

  // Serial.print("HALL1:");
  // Serial.print(digitalRead(esquerda.getEncoderPin()));
  // Serial.print(",");
  // Serial.print("HALL2:");
  // Serial.println(digitalRead(direita.getEncoderPin()));
}
