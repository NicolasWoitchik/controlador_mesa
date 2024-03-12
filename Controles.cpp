#include "Motor.h"
#include "Arduino.h"
#include "Controles.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Controles::Controles(int inputSubir, int inputDescer, int inputAlturaCustom, Motor *esquerda, Motor *direita) {
  this->_inputSubir = inputSubir;
  this->_inputDescer = inputDescer;

  this->_motorEsquerda = esquerda;
  this->_motorDireita = direita;
}

void Controles::begin() {
  pinMode(this->_inputSubir, INPUT);
  pinMode(this->_inputDescer, INPUT);
}

void Controles::loop() {
  bool isSubir = digitalRead(this->_inputSubir);
  bool isDescer = digitalRead(this->_inputDescer);

  if (isSubir && !isDescer) {
    this->_direcao = SUBIR;
  } else if (!isSubir && isDescer) {
    this->_direcao = DESCER;
  } else {
    this->_direcao = PARAR;
  }

  // Serial.print("Direcao: ");
  // Serial.println(this->_direcao);

  if (this->_direcao == SUBIR) {
    this->subir();
  } else if (this->_direcao == DESCER) {
    this->descer();
  } else {
    this->parar();
  }
}

void Controles::subir() {
  bool isSubir = digitalRead(this->_inputSubir);

  if (!isSubir) return;

  this->_motorEsquerda->subir();
  this->_motorDireita->subir();
}

void Controles::descer() {
  bool isDescer = digitalRead(this->_inputDescer);

  if (!isDescer) return;

  this->_motorEsquerda->descer();
  this->_motorDireita->descer();
}

void Controles::parar() {
  this->_motorEsquerda->parar();
  this->_motorDireita->parar();
}

void Controles::custom() {
}

void Controles::printStatus(Adafruit_SSD1306 *tela) {
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->print("Direcao -> ");

  if (this->_direcao == SUBIR) {
  tela->println("Subindo");
  } else if (this->_direcao == DESCER) {
    tela->println("Descendo");
  } else {
    tela->println("Parado");
  }


  tela->print("Esquerda Pos: ");
  tela->println(this->_motorEsquerda->getPositionInCentimeters());
  tela->print("Direita Pos: ");
  tela->println(this->_motorDireita->getPositionInCentimeters());

  tela->print("Esquerda Steps: ");
  tela->println(this->_motorEsquerda->getSteps());
  tela->print("Direita Steps: ");
  tela->println(this->_motorDireita->getSteps());
  
  tela->display();
}


// void blinkLed()
// {
//   digitalWrite(ledPin, HIGH);
//   delay(200);
//   digitalWrite(ledPin, LOW);
//   delay(200);
//   digitalWrite(ledPin, HIGH);
// }