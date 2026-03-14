#include "Motor.h"
#include "Arduino.h"
#include "Controles.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

int dir;
int lastDir;

Controles::Controles(int inputSubir, int inputDescer, int inputAlturaCustom, Motor *esquerda, Motor *direita) {
  this->_inputSubir = inputSubir;
  this->_inputDescer = inputDescer;
  this->_motorEsquerda = esquerda;
  this->_motorDireita = direita;
  this->_lastStep = 0;
  this->_direcao = PARAR;
  this->_serialComando = PARAR;
  this->_calibrando = false;
  this->_ultimaSincronizacao = 0;
  this->_erroSincAtual = 0;
  this->_recuperando = false;
  this->_stepsEInicio = 0;
  this->_stepsDInicio = 0;
  this->_modoPreset = false;
  this->_alvoEmCm = 0.0f;
}

void Controles::begin() {
  pinMode(this->_inputSubir, INPUT);
  pinMode(this->_inputDescer, INPUT);
}

void Controles::loop() {
  if (_calibrando) return;

  bool isSubir = digitalRead(this->_inputSubir);
  bool isDescer = digitalRead(this->_inputDescer);

  // Botao fisico cancela recuperacao
  if (_recuperando && (isSubir || isDescer)) {
    _recuperando = false;
    _motorEsquerda->parar();
    _motorDireita->parar();
  }

  // Durante recuperacao, volta para posicao inicial
  if (_recuperando) {
    int stepsE = _motorEsquerda->getSteps();
    if (stepsE > _stepsEInicio) {
      _motorEsquerda->descer();
      _motorDireita->descer();
      dir = -1;
    } else if (stepsE < _stepsEInicio) {
      _motorEsquerda->subir();
      _motorDireita->subir();
      dir = 1;
    } else {
      _motorEsquerda->parar();
      _motorDireita->parar();
      _recuperando = false;
      dir = 0;
      Serial.println("{\"tipo\":\"info\",\"msg\":\"recuperacao_concluida\"}");
    }
    this->_verificaDirecao();
    return;
  }

  // Modo preset: mover para altura alvo
  if (_modoPreset) {
    if (isSubir || isDescer) {
      _modoPreset = false;
      _motorEsquerda->parar();
      _motorDireita->parar();
      dir = 0;
      this->_verificaDirecao();
      return;
    }
    float posAtual = _motorEsquerda->getPosicaoEmCentimetros();
    float diff = _alvoEmCm - posAtual;
    if (abs(diff) < PRESET_TOLERANCIA_CM) {
      _motorEsquerda->parar();
      _motorDireita->parar();
      _modoPreset = false;
      dir = 0;
      char json[80];
      snprintf(json, sizeof(json),
        "{\"tipo\":\"info\",\"msg\":\"preset_concluido\",\"posicao\":%.2f}", posAtual);
      Serial.println(json);
    } else if (diff > 0) {
      if (_direcao == PARAR) {
        _stepsEInicio = _motorEsquerda->getSteps();
        _stepsDInicio = _motorDireita->getSteps();
      }
      _motorEsquerda->subir();
      _motorDireita->subir();
      dir = 1;
    } else {
      if (_direcao == PARAR) {
        _stepsEInicio = _motorEsquerda->getSteps();
        _stepsDInicio = _motorDireita->getSteps();
      }
      _motorEsquerda->descer();
      _motorDireita->descer();
      dir = -1;
    }
    if (millis() - _ultimaSincronizacao >= 20) {
      _sincronizarMotores();
      _ultimaSincronizacao = millis();
    }
    this->_verificaDirecao();
    return;
  }

  if (isSubir && !isDescer) {
    if (_direcao == PARAR) {
      _stepsEInicio = _motorEsquerda->getSteps();
      _stepsDInicio = _motorDireita->getSteps();
    }
    _serialComando = PARAR;  // botao cancela comando serial
    this->_motorEsquerda->subir();
    this->_motorDireita->subir();
    dir = 1;
  } else if (!isSubir && isDescer) {
    if (_direcao == PARAR) {
      _stepsEInicio = _motorEsquerda->getSteps();
      _stepsDInicio = _motorDireita->getSteps();
    }
    _serialComando = PARAR;  // botao cancela comando serial
    this->_motorEsquerda->descer();
    this->_motorDireita->descer();
    dir = -1;
  } else if (_serialComando == SUBIR) {
    this->_motorEsquerda->subir();
    this->_motorDireita->subir();
    dir = 1;
  } else if (_serialComando == DESCER) {
    this->_motorEsquerda->descer();
    this->_motorDireita->descer();
    dir = -1;
  } else {
    this->_motorEsquerda->parar();
    this->_motorDireita->parar();
    dir = 0;
  }

  if (millis() - _ultimaSincronizacao >= 20) {
    _sincronizarMotores();
    _ultimaSincronizacao = millis();
  }

  this->_verificaDirecao();
}

void Controles::_verificaDirecao() {
  int currentStep = this->_motorEsquerda->getSteps();
  DIRECAO anterior = this->_direcao;

  if (currentStep > this->_lastStep) {
    this->_direcao = SUBIR;
  } else if (currentStep < this->_lastStep) {
    this->_direcao = DESCER;
  } else {
    this->_direcao = PARAR;
  }

  // Salva posicao na NVS ao parar
  if (anterior != PARAR && this->_direcao == PARAR) {
    _motorEsquerda->saveSteps();
    _motorDireita->saveSteps();
  }

  this->_lastStep = currentStep;
}

void Controles::subir() {
  _stepsEInicio = _motorEsquerda->getSteps();
  _stepsDInicio = _motorDireita->getSteps();
  _recuperando = false;
  _serialComando = SUBIR;
  this->_motorEsquerda->subir();
  this->_motorDireita->subir();
}

void Controles::descer() {
  _stepsEInicio = _motorEsquerda->getSteps();
  _stepsDInicio = _motorDireita->getSteps();
  _recuperando = false;
  _serialComando = DESCER;
  this->_motorEsquerda->descer();
  this->_motorDireita->descer();
}

void Controles::parar() {
  _serialComando = PARAR;
  _modoPreset = false;
  this->_motorEsquerda->parar();
  this->_motorDireita->parar();
}

void Controles::irPara(float alvoEmCm) {
  if (alvoEmCm < 84.0f || alvoEmCm > 144.0f) {
    Serial.println("{\"tipo\":\"erro\",\"msg\":\"preset_fora_de_faixa\"}");
    return;
  }
  _alvoEmCm = alvoEmCm;
  _stepsEInicio = _motorEsquerda->getSteps();
  _stepsDInicio = _motorDireita->getSteps();
  _modoPreset = true;
  _recuperando = false;
  char json[72];
  snprintf(json, sizeof(json),
    "{\"tipo\":\"info\",\"msg\":\"preset_iniciado\",\"alvo\":%.1f}", alvoEmCm);
  Serial.println(json);
}

void Controles::custom() {
}

DIRECAO Controles::getDirecao() const {
  return _direcao;
}

bool Controles::isBotaoSubir() const {
  return digitalRead(_inputSubir);
}

bool Controles::isBotaoDescer() const {
  return digitalRead(_inputDescer);
}

int Controles::getErroSincPulsos() const {
  return _erroSincAtual;
}

void Controles::_sincronizarMotores() {
  if (_recuperando) return;

  if (_direcao == PARAR) {
    _motorEsquerda->setVelocidade(255);
    _motorDireita->setVelocidade(255);
    _erroSincAtual = 0;
    return;
  }

  int stepsE = _motorEsquerda->getSteps();
  int stepsD = _motorDireita->getSteps();
  int pulsosE = _motorEsquerda->getPulsosCalibrados();
  int pulsosD = _motorDireita->getPulsosCalibrados();

  float posNormE = (float)stepsE / pulsosE;
  float posNormD = (float)stepsD / pulsosD;
  float erro = posNormE - posNormD;

  _erroSincAtual = (int)(erro * 6800);

  if (abs(_erroSincAtual) > EMERGENCIA_PULSOS) {
    _motorEsquerda->parar();
    _motorDireita->parar();
    _serialComando = PARAR;
    _recuperando = true;
    _modoPreset = false;
    // Distingue encoder inativo (um lado zerado) de desincronizacao real
    bool encoderInativo = (stepsD == _stepsDInicio) || (stepsE == _stepsEInicio);
    if (encoderInativo) {
      Serial.println("{\"tipo\":\"erro\",\"msg\":\"encoder_inativo\"}");
    } else {
      Serial.println("{\"tipo\":\"erro\",\"msg\":\"sinc_emergencia\"}");
    }
    return;
  }

  if (erro > 0.0005f) {
    int velE = constrain((int)(255.0f - GANHO_SINC * erro), VEL_MIN_SINC, 255);
    _motorEsquerda->setVelocidade(velE);
    _motorDireita->setVelocidade(255);
  } else if (erro < -0.0005f) {
    int velD = constrain((int)(255.0f + GANHO_SINC * erro), VEL_MIN_SINC, 255);
    _motorEsquerda->setVelocidade(255);
    _motorDireita->setVelocidade(velD);
  } else {
    _motorEsquerda->setVelocidade(255);
    _motorDireita->setVelocidade(255);
  }
}

void Controles::calibrar(Adafruit_SSD1306 *tela) {
  _calibrando = true;
  _serialComando = PARAR;

  // 1. Exibir mensagem e publicar status
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->setTextSize(1);
  tela->println("Calibrando...");
  tela->println("Descendo...");
  tela->display();
  Serial.println("{\"tipo\":\"status\",\"status\":\"calibrando\"}");

  // 2. Descer ambos os motores
  _motorEsquerda->descer();
  _motorDireita->descer();

  // 3. Aguardar ate ambos travarem (fim de curso inferior)
  delay(2000);  // tempo minimo para comecar a mover
  while (!(_motorEsquerda->isTravado(800) && _motorDireita->isTravado(800))) {
    delay(50);
  }

  // 4. Parar e zerar steps
  _motorEsquerda->parar();
  _motorDireita->parar();
  delay(300);
  _motorEsquerda->setSteps(0);
  _motorDireita->setSteps(0);

  // 5. Exibir mensagem de subida
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->println("Subindo para");
  tela->println("medir...");
  tela->display();
  Serial.println("{\"tipo\":\"status\",\"status\":\"calibrando_subindo\"}");

  delay(500);

  // 6. Subir ambos os motores
  _motorEsquerda->subir();
  _motorDireita->subir();

  // 7. Aguardar ate ambos travarem (fim de curso superior)
  delay(2000);
  while (!(_motorEsquerda->isTravado(800) && _motorDireita->isTravado(800))) {
    delay(50);
  }

  // 8. Registrar pulsos medidos
  int pulsosE = _motorEsquerda->getSteps();
  int pulsosD = _motorDireita->getSteps();

  // 9. Parar motores
  _motorEsquerda->parar();
  _motorDireita->parar();

  // 10. Salvar via Preferences (NVS)
  _motorEsquerda->setPulsosCalibrados(pulsosE);
  _motorDireita->setPulsosCalibrados(pulsosD);
  _motorEsquerda->saveSteps();
  _motorDireita->saveSteps();

  // 11. Exibir resultado e publicar
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->println("Calibrado!");
  char buf[32];
  snprintf(buf, sizeof(buf), "E:%d D:%d", pulsosE, pulsosD);
  tela->println(buf);
  tela->display();

  char json[96];
  snprintf(json, sizeof(json),
    "{\"tipo\":\"calibracao\",\"pulsosE\":%d,\"pulsosD\":%d}",
    pulsosE, pulsosD);
  Serial.println(json);

  _calibrando = false;
}

void Controles::printStatus(Adafruit_SSD1306 *tela) {
  tela->clearDisplay();
  tela->setCursor(0, 0);
  tela->setTextSize(2);

  if (this->_direcao == SUBIR) {
    tela->println("Subindo");
  } else if (this->_direcao == DESCER) {
    tela->println("Descendo");
  } else {
    tela->println("Parado");
  }

  tela->setTextSize(4);

  char total[16];
  float posicaoEmCentimetros = this->_motorEsquerda->getPosicaoEmCentimetros();
  dtostrf(posicaoEmCentimetros, 3, 2, total);

  char *cm = strtok(total, ".");
  tela->print(cm);
  tela->println("cm");

  char *mm = strtok(NULL, ".");
  tela->setTextSize(1);
  tela->print(mm);
  tela->print("mm");

  tela->printf(" S:%d", _erroSincAtual);

  tela->setCursor(0, 56);
  tela->setTextSize(1);
  tela->print(digitalRead(_inputSubir) ? "[^]" : " ^ ");
  tela->print(digitalRead(_inputDescer) ? "[v]" : " v ");

  tela->display();
}
