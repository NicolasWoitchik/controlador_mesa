#ifndef Motor_h
#define Motor_h

#include "Arduino.h"

enum DIRECAO { SUBIR, DESCER, PARAR };

class Motor {
public:
  Motor(int outputSobe, int outputDesce, int encoderPin);
  void subir();
  void descer();
  void parar();
  void step();
  int getSteps();
  void setSteps(int s);
  int getEncoderPin();
  void begin();
  float getPosicaoEmCentimetros();
  bool isTravado(int timeoutMs);
  int getPulsosCalibrados();
  void setPulsosCalibrados(int p);

private:
  int _outputSobe;
  int _outputDesce;
  int _velocidade;
  int _encoderPin;
  volatile int _direcao;
  volatile int _steps;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
  volatile unsigned long _ultimoPulsoTime;
  int _pulsosCalibrados;
};

#endif
