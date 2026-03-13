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
  this->_lastStep = 0;
  this->_direcao = PARAR;
  this->_serialComando = PARAR;
  this->_calibrando = false;
}

void Controles::begin() {
  pinMode(this->_inputSubir, INPUT);
  pinMode(this->_inputDescer, INPUT);
}

void Controles::loop() {
  if (_calibrando) return;

  bool isSubir = digitalRead(this->_inputSubir);
  bool isDescer = digitalRead(this->_inputDescer);

  if (isSubir && !isDescer) {
    _serialComando = PARAR;  // botao cancela comando serial
    this->_motorEsquerda->subir();
    this->_motorDireita->subir();
    dir = 1;
  } else if (!isSubir && isDescer) {
    _serialComando = PARAR;  // botao cancela comando serial
    this->_motorEsquerda->descer();
    this->_motorDireita->descer();
    dir = -1;
  } else if (_serialComando == SUBIR) {
    this->_motorEsquerda->subir();
    this->_motorDireita->subir();
    dir = 1;
  } else if (_serialComando == DESCER) {
    this->_motorEsquerda->descer();
    this->_motorDireita->descer();
    dir = -1;
  } else {
    this->_motorEsquerda->parar();
    this->_motorDireita->parar();
    dir = 0;
  }

  this->_verificaDirecao();
}

void Controles::_verificaDirecao() {
  int currentStep = this->_motorEsquerda->getSteps();

  if (currentStep > this->_lastStep) {
    this->_direcao = SUBIR;
  } else if (currentStep < this->_lastStep) {
    this->_direcao = DESCER;
  } else {
    this->_direcao = PARAR;
  }

  this->_lastStep = currentStep;
}

void Controles::subir() {
  _serialComando = SUBIR;
  this->_motorEsquerda->subir();
  this->_motorDireita->subir();
}

void Controles::descer() {
  _serialComando = DESCER;
  this->_motorEsquerda->descer();
  this->_motorDireita->descer();
}

void Controles::parar() {
  _serialComando = PARAR;
  this->_motorEsquerda->parar();
  this->_motorDireita->parar();
}

void Controles::custom() {
}

DIRECAO Controles::getDirecao() const {
  return _direcao;
}

void Controles::calibrar(Adafruit_SSD1306 *tela) {
  _calibrando = true;
  _serialComando = PARAR;

  // 1. Exibir mensagem e publicar status
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->setTextSize(1);
  tela->println("Calibrando...");
  tela->println("Descendo...");
  tela->display();
  Serial.println("{\"tipo\":\"status\",\"status\":\"calibrando\"}");

  // 2. Descer ambos os motores
  _motorEsquerda->descer();
  _motorDireita->descer();

  // 3. Aguardar ate ambos travarem (fim de curso inferior)
  delay(2000);  // tempo minimo para comecar a mover
  while (!(_motorEsquerda->isTravado(800) && _motorDireita->isTravado(800))) {
    delay(50);
  }

  // 4. Parar e zerar steps
  _motorEsquerda->parar();
  _motorDireita->parar();
  delay(300);
  _motorEsquerda->setSteps(0);
  _motorDireita->setSteps(0);

  // 5. Exibir mensagem de subida
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->println("Subindo para");
  tela->println("medir...");
  tela->display();
  Serial.println("{\"tipo\":\"status\",\"status\":\"calibrando_subindo\"}");

  delay(500);

  // 6. Subir ambos os motores
  _motorEsquerda->subir();
  _motorDireita->subir();

  // 7. Aguardar ate ambos travarem (fim de curso superior)
  delay(2000);
  while (!(_motorEsquerda->isTravado(800) && _motorDireita->isTravado(800))) {
    delay(50);
  }

  // 8. Registrar pulsos medidos
  int pulsosE = _motorEsquerda->getSteps();
  int pulsosD = _motorDireita->getSteps();

  // 9. Parar motores
  _motorEsquerda->parar();
  _motorDireita->parar();

  // 10. Salvar via Preferences (NVS)
  _motorEsquerda->setPulsosCalibrados(pulsosE);
  _motorDireita->setPulsosCalibrados(pulsosD);

  // 11. Exibir resultado e publicar
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->println("Calibrado!");
  char buf[32];
  snprintf(buf, sizeof(buf), "E:%d D:%d", pulsosE, pulsosD);
  tela->println(buf);
  tela->display();

  char json[96];
  snprintf(json, sizeof(json),
    "{\"tipo\":\"calibracao\",\"pulsosE\":%d,\"pulsosD\":%d}",
    pulsosE, pulsosD);
  Serial.println(json);

  _calibrando = false;
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

  char total[16];
  float posicaoEmCentimetros = this->_motorEsquerda->getPosicaoEmCentimetros();
  dtostrf(posicaoEmCentimetros, 3, 2, total);

  char *cm = strtok(total, ".");
  tela->print(cm);
  tela->println("cm");

  char *mm = strtok(NULL, ".");
  tela->setTextSize(1);
  tela->print(mm);
  tela->print("mm");

  tela->printf(" | %d", dir);

  tela->display();
}
