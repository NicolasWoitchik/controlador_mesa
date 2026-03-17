import { MOTOR } from './config';

export type MotorId = 'esquerda' | 'direita';
export type Direcao = 'SUBIR' | 'DESCER' | 'PARAR';

export interface CalibracaoMotor {
  pulsosCalibrados: number;
  steps: number;
}

export class Motor {
  private readonly _id: MotorId;
  private readonly _pinSobe: number;
  private readonly _pinDesce: number;
  private readonly _encoderPin: number;
  private readonly _pig: any;

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _gpioSobe!: any;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _gpioDesce!: any;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private _gpioEncoder!: any;

  private _direcao: Direcao = 'PARAR';
  private _velocidade: number = MOTOR.VEL_DEFAULT;
  private _steps: number = 0;
  private _pulsosCalibrados: number = MOTOR.PULSOS_DEFAULT;
  private _ultimoPulsoTime: number = Date.now();

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  constructor(pinSobe: number, pinDesce: number, encoderPin: number, id: MotorId, pig: any) {
    this._pinSobe = pinSobe;
    this._pinDesce = pinDesce;
    this._encoderPin = encoderPin;
    this._id = id;
    this._pig = pig;
  }

  async begin(calibData?: Partial<Record<MotorId, CalibracaoMotor>>): Promise<void> {
    // Configura pinos de saída (motor)
    this._gpioSobe  = this._pig.gpio(this._pinSobe);
    this._gpioDesce = this._pig.gpio(this._pinDesce);

    await this._gpioSobe.modeSet('output');
    await this._gpioDesce.modeSet('output');

    // Garante ambos em LOW ao iniciar
    await this._gpioSobe.write(0);
    await this._gpioDesce.write(0);

    // Configura encoder: pull-up + notify em borda (substitui alert do pigpio)
    this._gpioEncoder = this._pig.gpio(this._encoderPin);
    await this._gpioEncoder.modeSet('input');
    await this._gpioEncoder.pullUpDown(2);   // PUD_UP = 2
    await this._gpioEncoder.glitchSet(1000); // Filtro de 1ms (equivalente ao glitchFilter do pigpio)

    this._gpioEncoder.notify((level: number, _tick: number) => {
      if (level === 1) {
        if (this._direcao === 'SUBIR') {
          this._steps = Math.min(this._steps + 1, this._pulsosCalibrados);
        } else if (this._direcao === 'DESCER') {
          this._steps = Math.max(this._steps - 1, 0);
        }
        this._ultimoPulsoTime = Date.now();
      }
    });

    // Carrega calibração salva (ou usa defaults)
    const saved = calibData?.[this._id];
    if (saved) {
      this._pulsosCalibrados = saved.pulsosCalibrados;
      if (saved.steps >= 0 && saved.steps <= this._pulsosCalibrados) {
        this._steps = saved.steps;
      }
    }
  }

  subir(): void {
    // BTS7960: LPWM = 0, RPWM = PWM duty
    this._gpioDesce.write(0).catch((e: Error) =>
      console.error(`[Motor ${this._id}] write error:`, e.message));
    this._gpioSobe.hardwarePWM(MOTOR.PWM_FREQ, this._dutyFromVel()).catch((e: Error) =>
      console.error(`[Motor ${this._id}] PWM error:`, e.message));
    this._direcao = 'SUBIR';
    this._ultimoPulsoTime = Date.now();
  }

  descer(): void {
    // BTS7960: RPWM = 0, LPWM = PWM duty
    this._gpioSobe.write(0).catch((e: Error) =>
      console.error(`[Motor ${this._id}] write error:`, e.message));
    this._gpioDesce.hardwarePWM(MOTOR.PWM_FREQ, this._dutyFromVel()).catch((e: Error) =>
      console.error(`[Motor ${this._id}] PWM error:`, e.message));
    this._direcao = 'DESCER';
    this._ultimoPulsoTime = Date.now();
  }

  parar(): void {
    this._gpioSobe.write(0).catch((e: Error) =>
      console.error(`[Motor ${this._id}] write error:`, e.message));
    this._gpioDesce.write(0).catch((e: Error) =>
      console.error(`[Motor ${this._id}] write error:`, e.message));
    this._direcao = 'PARAR';
  }

  setVelocidade(v: number): void {
    this._velocidade = Math.max(0, Math.min(255, v));
    if (this._direcao === 'SUBIR') {
      this._gpioSobe.hardwarePWM(MOTOR.PWM_FREQ, this._dutyFromVel()).catch((e: Error) =>
        console.error(`[Motor ${this._id}] PWM error:`, e.message));
    } else if (this._direcao === 'DESCER') {
      this._gpioDesce.hardwarePWM(MOTOR.PWM_FREQ, this._dutyFromVel()).catch((e: Error) =>
        console.error(`[Motor ${this._id}] PWM error:`, e.message));
    }
  }

  getVelocidade(): number {
    return this._velocidade;
  }

  getDirecao(): Direcao {
    return this._direcao;
  }

  getSteps(): number {
    return this._steps;
  }

  setSteps(s: number): void {
    this._steps = s;
  }

  getEncoderPin(): number {
    return this._encoderPin;
  }

  getId(): MotorId {
    return this._id;
  }

  getPulsosCalibrados(): number {
    return this._pulsosCalibrados;
  }

  setPulsosCalibrados(p: number): void {
    this._pulsosCalibrados = p;
  }

  getPosicaoEmCentimetros(): number {
    return (this._steps * MOTOR.COMPRIMENTO_ASTE / this._pulsosCalibrados) + MOTOR.ALTURA_BASE_CM;
  }

  isTravado(ms: number): boolean {
    return Date.now() - this._ultimoPulsoTime > ms;
  }

  teardown(): void {
    this.parar();
    try { this._gpioEncoder?.endNotify(); } catch { /* noop */ }
  }

  private _dutyFromVel(): number {
    // Converte 0–255 para escala pigpio hardwarePWM: 0–1.000.000
    return Math.round(this._velocidade / 255 * 1_000_000);
  }
}
