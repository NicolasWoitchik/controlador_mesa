#ifndef Motor_h
#define Motor_h
#include "Arduino.h"

class Motor {
public:
  Motor(int outputSobe, int outputDesce, int encoderPin);
  void subir();
  void descer();
  void parar();
  void step();
  int getSteps();
  int getEncoderPin();
  void begin();
  float getPositionInCentimeters();
private:
  int _outputSobe;
  int _outputDesce;
  int _velocidade;
  int _encoderPin;
  int _direcao;
  int _steps;
};

#endif