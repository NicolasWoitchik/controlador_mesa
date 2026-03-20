#include "Motor.h"
#include "Arduino.h"
#include "driver/gpio.h"
#include "esp32-hal-ledc.h"
#include <Preferences.h>

Motor::Motor(int outputSobe, int outputDesce, int encoderPin, int encoderPinDir) {
  this->_outputSobe = outputSobe;
  this->_outputDesce = outputDesce;
  this->_velocidade = 255;
  this->_encoderPin = encoderPin;
  this->_encoderPinDir = encoderPinDir;
  this->_direcao = PARAR;
  this->_ultimoPulsoTime = 0;
  this->_pulsosCalibrados = 6800;
  this->_taskHandle = nullptr;
  this->_ultimoStepAmostrado = 0;
}

void Motor::begin() {
  pinMode(this->_outputSobe, OUTPUT);
  pinMode(this->_outputDesce, OUTPUT);

  pinMode(this->_encoderPin, INPUT_PULLUP);
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  this->_encoder.attachSingleEdge(this->_encoderPin, this->_encoderPinDir);
  // A lib seta encoderPinDir como INPUT — reconfigurar como INPUT_OUTPUT
  // para que o PCNT leia o nivel que estamos acionando via digitalWrite.
  // GPIO_MODE_INPUT_OUTPUT = output driver ativo + input buffer habilitado.
  gpio_set_level((gpio_num_t)this->_encoderPinDir, 0);
  gpio_set_direction((gpio_num_t)this->_encoderPinDir, GPIO_MODE_INPUT_OUTPUT);

  // Carrega pulsos calibrados e steps salvos da NVS
  Preferences prefs;
  char keyP[8], keyS[8];
  snprintf(keyP, sizeof(keyP), "p%d", this->_encoderPin);
  snprintf(keyS, sizeof(keyS), "s%d", this->_encoderPin);
  prefs.begin("motor", true);  // read-only
  int savedPulsos = prefs.getInt(keyP, 6800);
  int savedSteps = prefs.getInt(keyS, 0);
  prefs.end();
  this->_pulsosCalibrados = (savedPulsos > 0) ? savedPulsos : 6800;

  // Valida: steps salvos devem estar entre 0 e pulsosCalibrados
  if (savedSteps >= 0 && savedSteps <= this->_pulsosCalibrados) {
    this->_encoder.setCount(savedSteps);
  } else {
    this->_encoder.setCount(0);
  }
}

void Motor::subir() {
  // analogWrite usa LEDC: digitalWrite no pino inativo NÃO desliga o PWM — ambos podem ficar ativos.
  if (this->_direcao != SUBIR) {
    ledcWrite(this->_outputSobe, 0);
    ledcWrite(this->_outputDesce, 0);
    digitalWrite(this->_outputSobe, LOW);
    digitalWrite(this->_outputDesce, LOW);
    delayMicroseconds(MOTOR_DEAD_TIME_US);
  }
  this->_direcao = SUBIR;
  this->_ultimoPulsoTime = millis();
  digitalWrite(this->_encoderPinDir, LOW);  // PCNT: ctrl LOW = conta pra cima
  ledcWrite(this->_outputDesce, 0);
  digitalWrite(this->_outputDesce, LOW);
  analogWrite(this->_outputSobe, this->_velocidade);
}

void Motor::descer() {
  if (this->_direcao != DESCER) {
    ledcWrite(this->_outputSobe, 0);
    ledcWrite(this->_outputDesce, 0);
    digitalWrite(this->_outputSobe, LOW);
    digitalWrite(this->_outputDesce, LOW);
    delayMicroseconds(MOTOR_DEAD_TIME_US);
  }
  this->_direcao = DESCER;
  this->_ultimoPulsoTime = millis();
  digitalWrite(this->_encoderPinDir, HIGH);  // PCNT: ctrl HIGH = conta pra baixo
  ledcWrite(this->_outputSobe, 0);
  digitalWrite(this->_outputSobe, LOW);
  analogWrite(this->_outputDesce, this->_velocidade);
}

void Motor::parar() {
  this->_direcao = PARAR;
  ledcWrite(this->_outputSobe, 0);
  ledcWrite(this->_outputDesce, 0);
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
  if (this->_pulsosCalibrados <= 0) return 84.0f;
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
    ledcWrite(_outputDesce, 0);
    digitalWrite(_outputDesce, LOW);
    analogWrite(_outputSobe, _velocidade);
  } else if (_direcao == DESCER) {
    ledcWrite(_outputSobe, 0);
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

void Motor::startEncoderTask() {
  _ultimoStepAmostrado = getSteps();
  xTaskCreatePinnedToCore(
    _tarefaEncoder,
    "encoderTask",
    2048,
    this,
    1,
    &_taskHandle,
    0  // Core 0
  );
}

void Motor::_tarefaEncoder(void* arg) {
  Motor* m = static_cast<Motor*>(arg);
  for (;;) {
    int s = m->getSteps();
    if (s != m->_ultimoStepAmostrado) {
      m->_ultimoPulsoTime = millis();
      m->_ultimoStepAmostrado = s;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
