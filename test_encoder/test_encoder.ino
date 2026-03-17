// Diagnóstico: leitura bruta dos encoders Hall (esquerdo e direito)
// Comandos Serial: SUBIR / DESCER / PARAR / ZERO
// Output: JSON a cada 200ms com contagens brutas dos dois encoders

#include "driver/gpio.h"
#include <ESP32Encoder.h>

// --- Pinagem ---
#define PIN_MOT_E_SOBE   17
#define PIN_MOT_E_DESCE  16
#define PIN_MOT_D_SOBE   18
#define PIN_MOT_D_DESCE  19
#define PIN_ENC_ESQ      26
#define PIN_ENC_DIR      13

// --- Encoders PCNT ---
ESP32Encoder encEsq;
ESP32Encoder encDir;

static unsigned long ultimaImpressao = 0;

void subirMotores();
void descerMotores();
void pararMotores();

void setup() {
  Serial.begin(115200);
  Serial.println("{\"msg\":\"iniciando_test_encoder\"}");

  // Configura LEDC: pin-centric API (ESP32 Arduino Core 3.x), 5kHz, 8 bits
  ledcAttach(PIN_MOT_E_SOBE,  5000, 8);
  ledcAttach(PIN_MOT_E_DESCE, 5000, 8);
  ledcAttach(PIN_MOT_D_SOBE,  5000, 8);
  ledcAttach(PIN_MOT_D_DESCE, 5000, 8);
  pararMotores();

  // GPIO 13: libera JTAG e força pull-up
  gpio_reset_pin(GPIO_NUM_13);
  pinMode(PIN_ENC_DIR, INPUT_PULLUP);

  // Encoders PCNT com pull-up interno
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encEsq.attachSingleEdge(PIN_ENC_ESQ, -1);
  encDir.attachSingleEdge(PIN_ENC_DIR, -1);
  encEsq.setCount(0);
  encDir.setCount(0);

  Serial.println("{\"msg\":\"pronto\",\"cmds\":\"SUBIR|DESCER|PARAR|ZERO\"}");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SUBIR") {
      subirMotores();
      Serial.println("{\"msg\":\"subindo\"}");
    } else if (cmd == "DESCER") {
      descerMotores();
      Serial.println("{\"msg\":\"descendo\"}");
    } else if (cmd == "PARAR") {
      pararMotores();
      Serial.println("{\"msg\":\"parado\"}");
    } else if (cmd == "ZERO") {
      encEsq.setCount(0);
      encDir.setCount(0);
      Serial.println("{\"msg\":\"zerado\"}");
    }
  }

  unsigned long agora = millis();
  if (agora - ultimaImpressao >= 200) {
    ultimaImpressao = agora;
    long cntE = encEsq.getCount();
    long cntD = encDir.getCount();
    char buf[96];
    snprintf(buf, sizeof(buf),
      "{\"t\":%lu,\"encE\":%ld,\"encD\":%ld,\"diff\":%ld}",
      agora, cntE, cntD, cntE - cntD);
    Serial.println(buf);
  }
}

void subirMotores() {
  ledcWrite(PIN_MOT_E_DESCE, 0);
  ledcWrite(PIN_MOT_D_DESCE, 0);
  ledcWrite(PIN_MOT_E_SOBE, 255);
  ledcWrite(PIN_MOT_D_SOBE, 255);
}

void descerMotores() {
  ledcWrite(PIN_MOT_E_SOBE, 0);
  ledcWrite(PIN_MOT_D_SOBE, 0);
  ledcWrite(PIN_MOT_E_DESCE, 255);
  ledcWrite(PIN_MOT_D_DESCE, 255);
}

void pararMotores() {
  ledcWrite(PIN_MOT_E_SOBE,  0);
  ledcWrite(PIN_MOT_E_DESCE, 0);
  ledcWrite(PIN_MOT_D_SOBE,  0);
}
  ledcWrite(PIN_MOT_D_DESCE, 0);
