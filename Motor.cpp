#include "esp32-hal-gpio.h"
#include "Motor.h"
#include "Arduino.h"
#include <Preferences.h>

Motor::Motor(int outputSobe, int outputDesce, int encoderPin) {
  this->_outputSobe = outputSobe;
  this->_outputDesce = outputDesce;
  this->_velocidade = 255;
  this->_encoderPin = encoderPin;
  this->_direcao = PARAR;
  this->_steps = 0;
  this->_ultimoPulsoTime = 0;
  this->_pulsosCalibrados = 6800;
}

void Motor::begin() {
  pinMode(this->_outputSobe, OUTPUT);
  pinMode(this->_outputDesce, OUTPUT);
  pinMode(this->_encoderPin, INPUT);

  // Carrega pulsos calibrados da NVS (Preferences)
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
    this->_steps = savedSteps;
  } else {
    this->_steps = 0;
  }
}

void Motor::subir() {
  portENTER_CRITICAL(&_mux);
  this->_direcao = SUBIR;
  portEXIT_CRITICAL(&_mux);
  this->_ultimoPulsoTime = millis();  // evita isTravado() disparar imediatamente
  analogWrite(this->_outputSobe, this->_velocidade);
  analogWrite(this->_outputDesce, 0);
}

void Motor::descer() {
  portENTER_CRITICAL(&_mux);
  this->_direcao = DESCER;
  portEXIT_CRITICAL(&_mux);
  this->_ultimoPulsoTime = millis();  // evita isTravado() disparar imediatamente
  analogWrite(this->_outputSobe, 0);
  analogWrite(this->_outputDesce, this->_velocidade);
}

void Motor::parar() {
  portENTER_CRITICAL(&_mux);
  this->_direcao = PARAR;
  portEXIT_CRITICAL(&_mux);
  analogWrite(this->_outputSobe, 0);
  analogWrite(this->_outputDesce, 0);
}

// Chamado exclusivamente de ISR
void Motor::step() {
  portENTER_CRITICAL_ISR(&_mux);
  if (this->_direcao == SUBIR) {
    this->_steps++;
  } else if (this->_direcao == DESCER && this->_steps > 0) {
    this->_steps--;
  }
  portEXIT_CRITICAL_ISR(&_mux);
  this->_ultimoPulsoTime = millis();
}

int Motor::getEncoderPin() {
  return this->_encoderPin;
}

int Motor::getSteps() {
  portENTER_CRITICAL(&_mux);
  int s = this->_steps;
  portEXIT_CRITICAL(&_mux);
  return s;
}

void Motor::setSteps(int s) {
  portENTER_CRITICAL(&_mux);
  this->_steps = s;
  portEXIT_CRITICAL(&_mux);
}

float Motor::getPosicaoEmCentimetros() {
  const float comprimentoAste = 60.0;
  float resolucaoSensor = comprimentoAste / (float)this->_pulsosCalibrados;
  portENTER_CRITICAL(&_mux);
  int s = this->_steps;
  portEXIT_CRITICAL(&_mux);
  return s * resolucaoSensor + 84.0f;
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
  portENTER_CRITICAL(&_mux);
  int d = _direcao;
  portEXIT_CRITICAL(&_mux);
  if (d == SUBIR) {
    analogWrite(_outputSobe, _velocidade);
    analogWrite(_outputDesce, 0);
  } else if (d == DESCER) {
    analogWrite(_outputSobe, 0);
    analogWrite(_outputDesce, _velocidade);
  }
}

void Motor::saveSteps() {
  portENTER_CRITICAL(&_mux);
  int s = this->_steps;
  portEXIT_CRITICAL(&_mux);
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
  prefs.begin("motor", false);  // read-write
  prefs.putInt(key, p);
  prefs.end();
}
