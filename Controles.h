#ifndef Controles_h
#define Conroles_h
#include "Motor.h"
#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum DIRECAO {SUBIR, DESCER, PARAR};

class Controles {
public:
  Controles(int inputSubir, int inputDescer, int inputAlturaCustom, Motor *esquerda, Motor *direita);
  void begin();
  void subir();
  void descer();
  void loop();
  void parar();
  void custom();
  void printStatus(Adafruit_SSD1306 *tela);
private:
  int _inputSubir;
  int _inputDescer;
  int _inputAlturaCustom;
  Motor* _motorEsquerda;
  Motor* _motorDireita;
  DIRECAO _direcao;
};

#endif