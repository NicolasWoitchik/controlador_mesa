import { Gpio } from 'pigpio';
import { Motor, Direcao } from './Motor';
import { MqttClient, MqttComando } from './MqttClient';
import { SINC, PRESET, MOTOR } from './config';
import * as fs from 'fs/promises';
import { CALIBRACAO_PATH } from './config';

export interface StatusPayload {
  status: string;
  posicao: number;
  stepsE: number;
  stepsD: number;
  erroSync: number;
  velE: number;
  velD: number;
  btnS: boolean;
  btnD: boolean;
}

export class Controles {
  private readonly _motorEsquerda: Motor;
  private readonly _motorDireita: Motor;
  private readonly _pinSubir: number;
  private readonly _pinDescer: number;

  private _gpioBtnSubir!: Gpio;
  private _gpioBtnDescer!: Gpio;

  private _mqttComando: MqttComando = 'PARAR';
  private _calibrando: boolean = false;
  private _recuperando: boolean = false;
  private _modoPreset: boolean = false;
  private _alvoEmCm: number = 0;

  private _stepsEInicio: number = 0;
  private _stepsDInicio: number = 0;
  private _erroSincAtual: number = 0;

  private _pollInterval: ReturnType<typeof setInterval> | null = null;
  private _sincInterval: ReturnType<typeof setInterval> | null = null;

  constructor(
    pinSubir: number,
    pinDescer: number,
    motorEsquerda: Motor,
    motorDireita: Motor,
  ) {
    this._pinSubir = pinSubir;
    this._pinDescer = pinDescer;
    this._motorEsquerda = motorEsquerda;
    this._motorDireita = motorDireita;
  }

  begin(): void {
    // Botões: pull-down → lêem HIGH quando pressionados (ligados ao 3.3V)
    this._gpioBtnSubir  = new Gpio(this._pinSubir,  { mode: Gpio.INPUT, pullUpDown: Gpio.PUD_DOWN });
    this._gpioBtnDescer = new Gpio(this._pinDescer, { mode: Gpio.INPUT, pullUpDown: Gpio.PUD_DOWN });

    // Loop de leitura de botões + controle: 5ms (200 Hz)
    this._pollInterval = setInterval(() => this._loopStep(), 5);

    // Loop de sincronização: 20ms (50 Hz) — porta direta do C++
    this._sincInterval = setInterval(() => this._sincronizarMotores(), SINC.INTERVALO_MS);
  }

  subir(): void {
    this._salvarStepsInicio();
    this._mqttComando = 'SUBIR';
    this._modoPreset = false;
    this._motorEsquerda.setVelocidade(MOTOR.VEL_DEFAULT);
    this._motorDireita.setVelocidade(MOTOR.VEL_DEFAULT);
    this._motorEsquerda.subir();
    this._motorDireita.subir();
  }

  descer(): void {
    this._salvarStepsInicio();
    this._mqttComando = 'DESCER';
    this._modoPreset = false;
    this._motorEsquerda.setVelocidade(MOTOR.VEL_DEFAULT);
    this._motorDireita.setVelocidade(MOTOR.VEL_DEFAULT);
    this._motorEsquerda.descer();
    this._motorDireita.descer();
  }

  parar(): void {
    this._mqttComando = 'PARAR';
    this._modoPreset = false;
    this._motorEsquerda.parar();
    this._motorDireita.parar();
    this._salvarStepsJson().catch((e) => console.error('[Controles] Erro ao salvar steps:', e));
  }

  irPara(alvoEmCm: number): void {
    if (alvoEmCm < PRESET.ALTURA_MIN_CM || alvoEmCm > PRESET.ALTURA_MAX_CM) {
      console.warn(`[Controles] Preset fora de faixa: ${alvoEmCm} cm`);
      return;
    }
    this._alvoEmCm = alvoEmCm;
    this._modoPreset = true;
    this._salvarStepsInicio();
  }

  async calibrar(mqtt: MqttClient): Promise<void> {
    if (this._calibrando) return;
    this._calibrando = true;
    this._mqttComando = 'PARAR';
    this._modoPreset = false;

    try {
      console.log('[Calibracao] Iniciando — descendo...');
      mqtt.publishInfo({ tipo: 'status', status: 'calibrando', fase: 'descendo' });

      // Fase 1: descer até travar
      this._motorEsquerda.setVelocidade(MOTOR.VEL_DEFAULT);
      this._motorDireita.setVelocidade(MOTOR.VEL_DEFAULT);
      this._motorEsquerda.descer();
      this._motorDireita.descer();

      await _sleep(2000);
      await this._waitUntilStalled(MOTOR.ENCODER_TIMEOUT_MS);

      this._motorEsquerda.parar();
      this._motorDireita.parar();
      await _sleep(300);

      // Zera encoders na posição mínima
      this._motorEsquerda.setSteps(0);
      this._motorDireita.setSteps(0);

      // Fase 2: subir até travar
      console.log('[Calibracao] Subindo para medir...');
      mqtt.publishInfo({ tipo: 'status', status: 'calibrando', fase: 'subindo' });

      await _sleep(500);
      this._motorEsquerda.subir();
      this._motorDireita.subir();

      await _sleep(2000);
      await this._waitUntilStalled(MOTOR.ENCODER_TIMEOUT_MS);

      const pulsosE = this._motorEsquerda.getSteps();
      const pulsosD = this._motorDireita.getSteps();

      this._motorEsquerda.parar();
      this._motorDireita.parar();

      // Salva calibração
      this._motorEsquerda.setPulsosCalibrados(pulsosE);
      this._motorDireita.setPulsosCalibrados(pulsosD);

      await this._salvarStepsJson();

      console.log(`[Calibracao] Concluída — E:${pulsosE} D:${pulsosD}`);
      mqtt.publishCalibration({ pulsosE, pulsosD });
    } finally {
      this._calibrando = false;
    }
  }

  getStatusPayload(): StatusPayload {
    const direcaoE = this._motorEsquerda.getDirecao();
    const posicao  = this._motorEsquerda.getPosicaoEmCentimetros();

    let status: string;
    if (direcaoE === 'SUBIR')   status = 'subindo';
    else if (direcaoE === 'DESCER') status = 'descendo';
    else status = 'parado';

    return {
      status,
      posicao:  Math.round(posicao * 100) / 100,
      stepsE:   this._motorEsquerda.getSteps(),
      stepsD:   this._motorDireita.getSteps(),
      erroSync: this._erroSincAtual,
      velE:     this._motorEsquerda.getVelocidade(),
      velD:     this._motorDireita.getVelocidade(),
      btnS:     this._isBotaoSubir(),
      btnD:     this._isBotaoDescer(),
    };
  }

  teardown(): void {
    if (this._pollInterval) clearInterval(this._pollInterval);
    if (this._sincInterval) clearInterval(this._sincInterval);
    this._motorEsquerda.parar();
    this._motorDireita.parar();
  }

  // ---------------------------------------------------------------------------
  // Loop principal (5ms)
  // ---------------------------------------------------------------------------

  private _loopStep(): void {
    if (this._calibrando) return;

    const btnSubir  = this._isBotaoSubir();
    const btnDescer = this._isBotaoDescer();

    // Botão físico cancela recovery e preset
    if (this._recuperando && (btnSubir || btnDescer)) {
      this._recuperando = false;
      this._motorEsquerda.parar();
      this._motorDireita.parar();
      return;
    }

    // Modo recovery: retorna à posição inicial com tolerância ±5 steps
    if (this._recuperando) {
      const stepsE = this._motorEsquerda.getSteps();
      const diff = stepsE - this._stepsEInicio;
      if (Math.abs(diff) <= 5) {
        this._recuperando = false;
        this._motorEsquerda.parar();
        this._motorDireita.parar();
        this._salvarStepsJson().catch(() => {});
      } else if (diff > 5) {
        this._motorEsquerda.descer();
        this._motorDireita.descer();
      } else {
        this._motorEsquerda.subir();
        this._motorDireita.subir();
      }
      return;
    }

    // Botão físico cancela preset e executa manualmente
    if (btnSubir || btnDescer) {
      if (this._modoPreset) {
        this._modoPreset = false;
        this._motorEsquerda.parar();
        this._motorDireita.parar();
        return;
      }

      if (btnSubir && !btnDescer) {
        if (this._mqttComando !== 'SUBIR') {
          this._salvarStepsInicio();
          this._mqttComando = 'PARAR'; // cancela comando serial
        }
        this._motorEsquerda.subir();
        this._motorDireita.subir();
      } else if (btnDescer && !btnSubir) {
        if (this._mqttComando !== 'DESCER') {
          this._salvarStepsInicio();
          this._mqttComando = 'PARAR';
        }
        this._motorEsquerda.descer();
        this._motorDireita.descer();
      } else {
        // Ambos pressionados → para
        this._motorEsquerda.parar();
        this._motorDireita.parar();
      }
      return;
    }

    // Modo preset: busca altura alvo
    if (this._modoPreset) {
      const posAtual = this._motorEsquerda.getPosicaoEmCentimetros();
      const diff = this._alvoEmCm - posAtual;

      if (Math.abs(diff) <= PRESET.TOLERANCIA_CM) {
        this._modoPreset = false;
        this._motorEsquerda.parar();
        this._motorDireita.parar();
        this._salvarStepsJson().catch(() => {});
      } else if (diff > 0) {
        this._motorEsquerda.subir();
        this._motorDireita.subir();
      } else {
        this._motorEsquerda.descer();
        this._motorDireita.descer();
      }
      return;
    }

    // Executa comando MQTT
    if (this._mqttComando === 'SUBIR') {
      this._motorEsquerda.subir();
      this._motorDireita.subir();
    } else if (this._mqttComando === 'DESCER') {
      this._motorEsquerda.descer();
      this._motorDireita.descer();
    } else {
      this._motorEsquerda.parar();
      this._motorDireita.parar();
    }
  }

  // ---------------------------------------------------------------------------
  // Sincronização proporcional (20ms) — porta direta do C++
  // ---------------------------------------------------------------------------

  private _sincronizarMotores(): void {
    if (this._calibrando || this._recuperando) return;

    const direcaoE = this._motorEsquerda.getDirecao();
    const direcaoD = this._motorDireita.getDirecao();
    if (direcaoE === 'PARAR' && direcaoD === 'PARAR') return;

    const stepsE = this._motorEsquerda.getSteps();
    const stepsD = this._motorDireita.getSteps();
    const pulsosE = this._motorEsquerda.getPulsosCalibrados();
    const pulsosD = this._motorDireita.getPulsosCalibrados();

    if (pulsosE === 0 || pulsosD === 0) {
      console.error('[Sinc] pulsosCalibrados inválido — sincronização abortada');
      return;
    }

    const deltaE = (stepsE - this._stepsEInicio) / pulsosE;
    const deltaD = (stepsD - this._stepsDInicio) / pulsosD;
    const erro   = deltaE - deltaD;

    const pulsosRef = (pulsosE + pulsosD) / 2;
    this._erroSincAtual = Math.round(erro * pulsosRef);

    // Detecta encoder inativo (um lado ficou em stepsInicio)
    const encoderInativo =
      stepsE === this._stepsEInicio || stepsD === this._stepsDInicio;

    if (Math.abs(this._erroSincAtual) > SINC.EMERGENCIA_PULSOS) {
      this._motorEsquerda.parar();
      this._motorDireita.parar();
      this._recuperando = true;
      const msg = encoderInativo ? 'encoder_inativo' : 'sinc_emergencia';
      console.error(`[Sinc] Emergência: ${msg} — erroSync=${this._erroSincAtual}`);
      return;
    }

    // Controle proporcional
    if (erro > SINC.THRESHOLD) {
      // Esquerda adiantada → freia esquerda
      const velE = Math.max(SINC.VEL_MIN, Math.min(255, Math.round(255 - SINC.GANHO * erro)));
      this._motorEsquerda.setVelocidade(velE);
      this._motorDireita.setVelocidade(MOTOR.VEL_DEFAULT);
    } else if (erro < -SINC.THRESHOLD) {
      // Direita adiantada → freia direita
      const velD = Math.max(SINC.VEL_MIN, Math.min(255, Math.round(255 + SINC.GANHO * erro)));
      this._motorEsquerda.setVelocidade(MOTOR.VEL_DEFAULT);
      this._motorDireita.setVelocidade(velD);
    } else {
      // Sincronizados
      this._motorEsquerda.setVelocidade(MOTOR.VEL_DEFAULT);
      this._motorDireita.setVelocidade(MOTOR.VEL_DEFAULT);
    }
  }

  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  private _salvarStepsInicio(): void {
    this._stepsEInicio = this._motorEsquerda.getSteps();
    this._stepsDInicio = this._motorDireita.getSteps();
  }

  private _isBotaoSubir(): boolean {
    return this._gpioBtnSubir.digitalRead() === 1;
  }

  private _isBotaoDescer(): boolean {
    return this._gpioBtnDescer.digitalRead() === 1;
  }

  private _waitUntilStalled(timeoutMs: number, maxWaitMs = 90_000): Promise<void> {
    return new Promise((resolve, reject) => {
      const start = Date.now();
      const check = setInterval(() => {
        if (this._motorEsquerda.isTravado(timeoutMs) && this._motorDireita.isTravado(timeoutMs)) {
          clearInterval(check);
          resolve();
        } else if (Date.now() - start > maxWaitMs) {
          clearInterval(check);
          reject(new Error('Timeout: motor não travou dentro do tempo esperado'));
        }
      }, 50);
    });
  }

  private async _salvarStepsJson(): Promise<void> {
    const data = {
      esquerda: {
        pulsosCalibrados: this._motorEsquerda.getPulsosCalibrados(),
        steps: this._motorEsquerda.getSteps(),
      },
      direita: {
        pulsosCalibrados: this._motorDireita.getPulsosCalibrados(),
        steps: this._motorDireita.getSteps(),
      },
    };
    await fs.writeFile(CALIBRACAO_PATH, JSON.stringify(data, null, 2), 'utf-8');
  }
}

function _sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
