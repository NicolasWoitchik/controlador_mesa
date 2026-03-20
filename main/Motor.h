#ifndef Motor_h
#define Motor_h

#include "Arduino.h"
#include <ESP32Encoder.h>

enum DIRECAO { SUBIR, DESCER, PARAR };

class Motor {
public:
  Motor(int outputSobe, int outputDesce, int encoderPin, int encoderPinDir);
  void subir();
  void descer();
  void parar();
  int getSteps();
  void setSteps(int s);
  int getEncoderPin();
  void begin();
  float getPosicaoEmCentimetros();
  bool isTravado(int timeoutMs);
  int getPulsosCalibrados();
  void setPulsosCalibrados(int p);
  void setVelocidade(int v);
  int getVelocidade() const;
  void saveSteps();
  void startEncoderTask();

private:
  static void _tarefaEncoder(void* arg);
  /** Dead-time (µs) entre desligar um lado do H-bridge e ligar o outro — só na troca de direção. */
  static constexpr uint8_t MOTOR_DEAD_TIME_US = 15;
  int _outputSobe;
  int _outputDesce;
  int _velocidade;
  int _encoderPin;
  int _encoderPinDir;
  int _direcao;
  ESP32Encoder _encoder;
  volatile unsigned long _ultimoPulsoTime;
  int _pulsosCalibrados;
  TaskHandle_t _taskHandle;
  int _ultimoStepAmostrado;
};

#endif
