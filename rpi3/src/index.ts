import * as fs from 'fs';
import { Motor, CalibracaoMotor, MotorId } from './Motor';
import { Controles } from './Controles';
import { MqttClient } from './MqttClient';
import { PINS, CALIBRACAO_PATH, PUBLICACAO } from './config';

// eslint-disable-next-line @typescript-eslint/no-require-imports
const pigpioLib = require('pigpio-client');

// ---------------------------------------------------------------------------
// Conecta ao pigpiod via socket TCP (sem addon nativo — funciona com qualquer Node)
// ---------------------------------------------------------------------------

function conectarPigpio(): Promise<unknown> {
  return new Promise((resolve, reject) => {
    const pig = pigpioLib.pigpio({ host: 'localhost', port: 8888, timeout: 2 });
    pig.once('connected', () => {
      console.log('[pigpio] Conectado ao pigpiod');
      resolve(pig);
    });
    pig.once('error', (err: Error) => reject(err));
  });
}

// ---------------------------------------------------------------------------
// Carrega calibração salva (graceful fallback se não existir)
// ---------------------------------------------------------------------------

type CalibracaoFile = Partial<Record<MotorId, CalibracaoMotor>>;

function carregarCalibracao(): CalibracaoFile {
  try {
    const raw = fs.readFileSync(CALIBRACAO_PATH, 'utf-8');
    return JSON.parse(raw) as CalibracaoFile;
  } catch {
    console.log('[Init] calibracao.json não encontrado — usando defaults');
    return {};
  }
}

// ---------------------------------------------------------------------------
// Inicialização
// ---------------------------------------------------------------------------

async function main(): Promise<void> {
  console.log('[Init] Controlador Mesa RPi3 iniciando...');

  // Conecta ao daemon pigpiod (deve estar rodando: sudo pigpiod)
  const pig = await conectarPigpio();

  const calibData = carregarCalibracao();

  // Motores
  const motorEsq = new Motor(PINS.MOTOR_ESQ_SOBE,  PINS.MOTOR_ESQ_DESCE, PINS.ENCODER_ESQ, 'esquerda', pig);
  const motorDir = new Motor(PINS.MOTOR_DIR_SOBE, PINS.MOTOR_DIR_DESCE, PINS.ENCODER_DIR, 'direita',  pig);

  await motorEsq.begin(calibData);
  await motorDir.begin(calibData);

  // MQTT
  const mqttClient = new MqttClient();
  await mqttClient.connect();

  // Controles
  const controles = new Controles(
    PINS.BTN_SUBIR,
    PINS.BTN_DESCER,
    motorEsq,
    motorDir,
    pig,
  );
  await controles.begin();

  // Roteamento de comandos MQTT → Controles
  mqttClient.onComando((cmd) => {
    console.log(`[MQTT] Comando: ${cmd}`);
    if (cmd === 'SUBIR')       controles.subir();
    else if (cmd === 'DESCER') controles.descer();
    else if (cmd === 'PARAR')  controles.parar();
    else if (cmd === 'CALIBRAR') {
      controles.calibrar(mqttClient).catch((e) =>
        console.error('[Calibracao] Erro:', e),
      );
    }
  });

  mqttClient.onPreset((alvo) => {
    console.log(`[MQTT] Preset: ${alvo} cm`);
    controles.irPara(alvo);
  });

  // ---------------------------------------------------------------------------
  // Loop de publicação MQTT (100ms)
  // ---------------------------------------------------------------------------

  let ultimaDirecao = '';
  let ultimaPosicao = 0;
  let ultimaPublicacao = 0;

  const pubInterval = setInterval(() => {
    const payload = controles.getStatusPayload();
    const agora = Date.now();
    const emMovimento = payload.status !== 'parado';

    const intervaloPub = emMovimento
      ? PUBLICACAO.INTERVALO_MOVIMENTO_MS
      : PUBLICACAO.INTERVALO_PARADO_MS;

    const direcaoMudou  = payload.status !== ultimaDirecao;
    const posicaoMudou  = Math.abs(payload.posicao - ultimaPosicao) >= PUBLICACAO.DELTA_CM;
    const intervaloPass = agora - ultimaPublicacao >= intervaloPub;

    if (direcaoMudou || posicaoMudou || intervaloPass) {
      mqttClient.publishStatus(payload.status);
      mqttClient.publishPosicao(payload.posicao);
      mqttClient.publishStepsEsquerda(payload.stepsE);
      mqttClient.publishStepsDireita(payload.stepsD);

      ultimaDirecao    = payload.status;
      ultimaPosicao    = payload.posicao;
      ultimaPublicacao = agora;
    }
  }, 100);

  console.log('[Init] Pronto.');

  // ---------------------------------------------------------------------------
  // Shutdown graceful
  // ---------------------------------------------------------------------------

  let isShuttingDown = false;

  async function shutdown(signal: string): Promise<void> {
    if (isShuttingDown) return;
    isShuttingDown = true;
    console.log(`[Shutdown] Sinal ${signal} recebido — encerrando...`);
    // Safety net: força saída após 3s caso alguma operação async trave
    setTimeout(() => process.exit(0), 3000).unref();
    clearInterval(pubInterval);
    controles.teardown();
    motorEsq.teardown();
    motorDir.teardown();
    await mqttClient.disconnect().catch(() => {});
    // Desconecta do pigpiod (não encerra o daemon, apenas fecha o socket)
    try { (pig as any).end(() => {}); } catch { /* noop */ }
    process.exit(0);
  }

  process.on('SIGTERM', () => shutdown('SIGTERM').catch(console.error));
  process.on('SIGINT',  () => shutdown('SIGINT').catch(console.error));
}

main().catch((err) => {
  console.error('[Fatal]', err);
  process.exit(1);
});
