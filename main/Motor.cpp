#include "Motor.h"
#include "Arduino.h"
#include <Preferences.h>

Motor::Motor(int outputSobe, int outputDesce, int encoderPin) {
  this->_outputSobe = outputSobe;
  this->_outputDesce = outputDesce;
  this->_velocidade = 255;
  this->_encoderPin = encoderPin;
  this->_direcao = PARAR;
  this->_ultimoPulsoTime = 0;
  this->_pulsosCalibrados = 6800;
}

void Motor::begin() {
  pinMode(this->_outputSobe, OUTPUT);
  pinMode(this->_outputDesce, OUTPUT);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  this->_encoder.attachSingleEdge(this->_encoderPin, -1);

  // Carrega pulsos calibrados e steps salvos da NVS
  Preferences prefs;
  char keyP[8], keyS[8];
  snprintf(keyP, sizeof(keyP), "p%d", this->_encoderPin);
  snprintf(keyS, sizeof(keyS), "s%d", this->_encoderPin);
  prefs.begin("motor", true);  // read-only
  this->_pulsosCalibrados = prefs.getInt(keyP, 6800);
  int savedSteps = prefs.getInt(keyS, 0);
  prefs.end();

  // Valida: steps salvos devem estar entre 0 e pulsosCalibrados
  if (savedSteps >= 0 && savedSteps <= this->_pulsosCalibrados) {
    this->_encoder.setCount(savedSteps);
  } else {
    this->_encoder.setCount(0);
  }
}

void Motor::subir() {
  this->_direcao = SUBIR;
  this->_ultimoPulsoTime = millis();
  digitalWrite(this->_outputDesce, LOW);
  analogWrite(this->_outputSobe, this->_velocidade);
}

void Motor::descer() {
  this->_direcao = DESCER;
  this->_ultimoPulsoTime = millis();
  digitalWrite(this->_outputSobe, LOW);
  analogWrite(this->_outputDesce, this->_velocidade);
}

void Motor::parar() {
  this->_direcao = PARAR;
  digitalWrite(this->_outputSobe, LOW);
  digitalWrite(this->_outputDesce, LOW);
}

int Motor::getEncoderPin() {
  return this->_encoderPin;
}

int Motor::getSteps() {
  return (int)this->_encoder.getCount();
}

void Motor::setSteps(int s) {
  this->_encoder.setCount(s);
}

float Motor::getPosicaoEmCentimetros() {
  const float comprimentoAste = 60.0;
  float resolucaoSensor = comprimentoAste / (float)this->_pulsosCalibrados;
  return (float)this->getSteps() * resolucaoSensor + 84.0f;
}

bool Motor::isTravado(int timeoutMs) {
  return (millis() - this->_ultimoPulsoTime) > (unsigned long)timeoutMs;
}

int Motor::getPulsosCalibrados() {
  return this->_pulsosCalibrados;
}

int Motor::getVelocidade() const {
  return _velocidade;
}

void Motor::setVelocidade(int v) {
  _velocidade = constrain(v, 0, 255);
  if (_direcao == SUBIR) {
    digitalWrite(_outputDesce, LOW);
    analogWrite(_outputSobe, _velocidade);
  } else if (_direcao == DESCER) {
    digitalWrite(_outputSobe, LOW);
    analogWrite(_outputDesce, _velocidade);
  }
}

void Motor::saveSteps() {
  int s = this->getSteps();
  Preferences prefs;
  char key[8];
  snprintf(key, sizeof(key), "s%d", this->_encoderPin);
  prefs.begin("motor", false);
  prefs.putInt(key, s);
  prefs.end();
}

void Motor::setPulsosCalibrados(int p) {
  this->_pulsosCalibrados = p;
  Preferences prefs;
  char key[8];
  snprintf(key, sizeof(key), "p%d", this->_encoderPin);
  prefs.begin("motor", false);
  prefs.putInt(key, p);
  prefs.end();
}
