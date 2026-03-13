#ifndef Controles_h
#define Controles_h

#include "Motor.h"
#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Controles {
public:
  Controles(int inputSubir, int inputDescer, int inputAlturaCustom, Motor *esquerda, Motor *direita);
  void begin();
  void subir();
  void descer();
  void loop();
  void parar();
  void custom();
  void calibrar(Adafruit_SSD1306 *tela);
  void printStatus(Adafruit_SSD1306 *tela);
  DIRECAO getDirecao() const;

private:
  void _verificaDirecao();
  int _inputSubir;
  int _inputDescer;
  int _inputAlturaCustom;
  Motor* _motorEsquerda;
  Motor* _motorDireita;
  DIRECAO _direcao;
  DIRECAO _serialComando;
  int _lastStep;
  bool _calibrando;
};

#endif
