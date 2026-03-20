#ifndef Controles_h
#define Controles_h

#include "Motor.h"
#include "Arduino.h"
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>

class Controles {
public:
  Controles(int inputSubir, int inputDescer, int inputAlturaCustom, Motor *esquerda, Motor *direita);
  void begin();
  void subir();
  void descer();
  void loop();
  void parar();
  void custom();
  void calibrar();
  void printStatus();
  DIRECAO getDirecao() const;
  int getErroSincPulsos() const;
  void irPara(float alvoEmCm);
  bool isBotaoSubir() const;
  bool isBotaoDescer() const;

private:
  void _verificaDirecao();
  void _sincronizarMotores();
  int _inputSubir;
  int _inputDescer;
  int _inputAlturaCustom;
  Motor* _motorEsquerda;
  Motor* _motorDireita;
  DIRECAO _direcao;
  DIRECAO _serialComando;
  int _lastStep;
  bool _calibrando;
  unsigned long _ultimaSincronizacao;
  int _erroSincAtual;
  bool _recuperando;
  int _stepsEInicio;
  int _stepsDInicio;
  bool _modoPreset;
  float _alvoEmCm;
  static constexpr float PRESET_TOLERANCIA_CM = 0.3f;
  static const int GANHO_SINC = 1500;
  static const int VEL_MIN_SINC = 160;
  static const int EMERGENCIA_PULSOS = 400;
};

#endif
