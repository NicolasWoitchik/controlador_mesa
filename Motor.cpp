#include "esp32-hal-gpio.h"
#include "Motor.h"
#include "Arduino.h"
#include "Controles.h"

Motor::Motor(int outputSobe, int outputDesce, int encoderPin) {
  this->_outputSobe = outputSobe;
  this->_outputDesce = outputDesce;

  this->_velocidade = 255;
  this->_encoderPin = encoderPin;
}

void Motor::begin() {
  pinMode(this->_outputSobe, OUTPUT);
  pinMode(this->_outputDesce, OUTPUT);
  pinMode(this->_encoderPin, INPUT);
  this->_steps = 0;
}

void Motor::subir() {
  this->_direcao = SUBIR;
  analogWrite(this->_outputSobe, this->_velocidade);
  analogWrite(this->_outputDesce, 0);
}

void Motor::descer() {
  this->_direcao = DESCER;
  analogWrite(this->_outputSobe, 0);
  analogWrite(this->_outputDesce, this->_velocidade);
}

void Motor::parar() {
  this->_direcao = PARAR;
  analogWrite(this->_outputSobe, 0);
  analogWrite(this->_outputDesce, 0);
  
}

void Motor::step() {
  if (this->_direcao == SUBIR) {
    this->_steps++;
  } else if (this->_direcao == DESCER && this->_steps > 0) {
    this->_steps--;
  }
}

int Motor::getEncoderPin() {
  return this->_encoderPin;
}

int Motor::getSteps() {
  return this->_steps;
}

float Motor::getPosicaoEmCentimetros() {
  const float comprimentoAste = 60.0;  // Comprimento da aste em centímetros
  const int pulsosPorCiclo = 3100;     // Número de pulsos por ciclo completo do atuador

  // Calculando a resolução do sensor (deslocamento em centímetros por pulso)
  float resolucaoSensor = comprimentoAste / pulsosPorCiclo;

  // Lendo o número de pulsos do sensor hall
  // int pulsos = pulseIn(this->_encoderPin, HIGH);

  // Calculando a posição em centímetros
  float posicaoEmCentimetros = _steps * resolucaoSensor;

  return posicaoEmCentimetros + 84;
}
