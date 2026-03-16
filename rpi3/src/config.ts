export const PINS = {
  MOTOR_ESQ_SOBE:  12,  // BTS7960 RPWM — HW PWM0
  MOTOR_ESQ_DESCE: 18,  // BTS7960 LPWM — HW PWM0
  ENCODER_ESQ:      5,

  MOTOR_DIR_SOBE:  13,  // BTS7960 RPWM — HW PWM1
  MOTOR_DIR_DESCE: 19,  // BTS7960 LPWM — HW PWM1
  ENCODER_DIR:      6,

  BTN_SUBIR:       20,
  BTN_DESCER:      21,
  BTN_CUSTOM:      26,
} as const;

export const MOTOR = {
  PWM_FREQ:           1000,   // Hz
  VEL_DEFAULT:         255,   // 0–255
  ENCODER_TIMEOUT_MS:  800,   // ms sem pulso → travado
  PULSOS_DEFAULT:     6800,   // pulsos calibrados padrão
  ALTURA_BASE_CM:     84.0,   // cm — posição mínima (totalmente descido)
  COMPRIMENTO_ASTE:   60.0,   // cm — curso total do atuador
} as const;

export const SINC = {
  INTERVALO_MS:       20,     // ms — frequência do loop de sincronização
  GANHO:            1500,     // ganho proporcional para correção de velocidade
  VEL_MIN:           160,     // velocidade mínima durante correção (evita stall)
  EMERGENCIA_PULSOS:  400,    // limite de erro em pulsos normalizados → parada de emergência
  THRESHOLD:       0.0005,    // erro fracionário mínimo para acionar correção
} as const;

export const PRESET = {
  TOLERANCIA_CM:   0.3,       // cm — tolerância para considerar altura atingida
  ALTURA_MIN_CM:  84.0,
  ALTURA_MAX_CM: 144.0,
} as const;

export const PUBLICACAO = {
  INTERVALO_MOVIMENTO_MS:  500,   // ms — publicação periódica em movimento
  INTERVALO_PARADO_MS:    5000,   // ms — publicação periódica parado
  DELTA_CM:               0.05,   // cm — delta mínimo de posição para forçar publicação
} as const;

export const MQTT_CONFIG = {
  BROKER:    process.env['MESA_MQTT_BROKER'] ?? 'localhost',
  PORT:   +( process.env['MESA_MQTT_PORT']   ?? 1883),
  USER:      process.env['MESA_MQTT_USER']   ?? '',
  PASS:      process.env['MESA_MQTT_PASS']   ?? '',
  CLIENT_ID: 'mesa-rpi3',
  RECONNECT_PERIOD_MS: 5000,
} as const;

export const CALIBRACAO_PATH = './calibracao.json';
