#include "Motor.h"
#include "Arduino.h"
#include "Controles.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

int dir;
int lastDir;

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
    this->subir();
    dir = 1;
  } else if (!isSubir && isDescer) {
    this->descer();
    dir = -1;
  } else {
    this->parar();
    dir = 0;
  }

  this->_verificaDirecao();
}

void Controles::_verificaDirecao(){
  int currentStep = this->_motorDireita->getSteps();

  if(currentStep > this->_lastStep){
    this->_direcao = SUBIR;
  }
  else if(currentStep < this->_lastStep){
    this->_direcao = DESCER;
  }
  else{
    this->_direcao = PARAR;
  }

  this->_lastStep = currentStep;
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
  tela->setTextSize(2);

  if (this->_direcao == SUBIR) {
  tela->println("Subindo");
  } else if (this->_direcao == DESCER) {
    tela->println("Descendo");
  } else {
    tela->println("Parado");
  }

  tela->setTextSize(4);
  // tela->println("");

  // tela->print("E-Pos: ");
  // tela->println(this->_motorEsquerda->getPosicaoEmCentimetros());
  // tela->print("E-Steps: ");
  // tela->println(this->_motorEsquerda->getSteps());
  // tela->println("");
  // tela->print("D-Pos: ");

  char total[8]; // Buffer big enough for 7-character float
  dtostrf(this->_motorDireita->getPosicaoEmCentimetros(), 3, 2, total); // Leave room for too large numbers!

  char * cm = strtok(total, ".");
  tela->print(cm);
  tela->println("cm");

  char * mm = strtok(NULL,".");
  tela->setTextSize(1);
  tela->print(mm);
  tela->print("mm");

  tela -> printf(" | %d",dir);
  // tela->print("D-Steps: ");
  // tela->println(this->_motorDireita->getSteps());
  
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