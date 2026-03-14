// Ponte Serial <-> MQTT (lado ESP32)
// Funcoes chamadas de main.ino: lerComandoSerial() e publicarSerial()
// As variaveis esquerda, direita, controles e tela sao globais de main.ino

static DIRECAO _ultimaDirecao = PARAR;
static float _ultimaPosicao = -1.0f;
static unsigned long _ultimaPublicacao = 0;

// Le uma linha do Serial e executa o comando correspondente
void lerComandoSerial() {
  if (!Serial.available()) return;

  String linha = Serial.readStringUntil('\n');
  linha.trim();

  if (linha == "SUBIR") {
    controles.subir();
  } else if (linha == "DESCER") {
    controles.descer();
  } else if (linha == "PARAR") {
    controles.parar();
  } else if (linha == "CALIBRAR") {
    controles.calibrar(&tela);
  } else if (linha.startsWith("IR:")) {
    float alvo = linha.substring(3).toFloat();
    controles.irPara(alvo);
  }
}

// Publica JSON no Serial quando ha mudanca de estado ou posicao
void publicarSerial() {
  float posE = esquerda.getPosicaoEmCentimetros();
  DIRECAO direcaoAtual = controles.getDirecao();
  unsigned long agora = millis();

  bool mudouDirecao = (direcaoAtual != _ultimaDirecao);
  bool mudouPosicao = (posE - _ultimaPosicao > 0.05f || _ultimaPosicao - posE > 0.05f);
  bool tempoPassou500  = (agora - _ultimaPublicacao) > 500;
  bool tempoPassou5s   = (agora - _ultimaPublicacao) > 5000;

  // Publica se: mudou algo, a cada 500ms em movimento, ou a cada 5s parada (para o HA)
  if (!mudouDirecao && !mudouPosicao
      && !(direcaoAtual != PARAR && tempoPassou500)
      && !tempoPassou5s) {
    return;
  }

  int stepsE = esquerda.getSteps();
  int stepsD = direita.getSteps();

  const char* statusStr;
  if (direcaoAtual == SUBIR) statusStr = "subindo";
  else if (direcaoAtual == DESCER) statusStr = "descendo";
  else statusStr = "parado";

  int erroSync = controles.getErroSincPulsos();
  int velE = esquerda.getVelocidade();
  int velD = direita.getVelocidade();

  int btnS = controles.isBotaoSubir() ? 1 : 0;
  int btnD = controles.isBotaoDescer() ? 1 : 0;

  char json[256];
  snprintf(json, sizeof(json),
    "{\"tipo\":\"status\",\"posicao\":%.2f,\"status\":\"%s\",\"stepsE\":%d,\"stepsD\":%d,\"erroSync\":%d,\"velE\":%d,\"velD\":%d,\"isrE\":%d,\"isrD\":%d,\"btnS\":%d,\"btnD\":%d}",
    posE, statusStr, stepsE, stepsD, erroSync, velE, velD, isrBrutoEsquerda, isrBrutoDireita, btnS, btnD);
  Serial.println(json);

  _ultimaDirecao = direcaoAtual;
  _ultimaPosicao = posE;
  _ultimaPublicacao = agora;
}
