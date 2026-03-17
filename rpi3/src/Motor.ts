import { Gpio } from 'pigpio';
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

  private _gpioSobe!: Gpio;
  private _gpioDesce!: Gpio;
  private _gpioEncoder!: Gpio;

  private _direcao: Direcao = 'PARAR';
  private _velocidade: number = MOTOR.VEL_DEFAULT;
  private _steps: number = 0;
  private _pulsosCalibrados: number = MOTOR.PULSOS_DEFAULT;
  private _ultimoPulsoTime: number = Date.now();

  constructor(pinSobe: number, pinDesce: number, encoderPin: number, id: MotorId) {
    this._pinSobe = pinSobe;
    this._pinDesce = pinDesce;
    this._encoderPin = encoderPin;
    this._id = id;
  }

  begin(calibData?: Partial<Record<MotorId, CalibracaoMotor>>): void {
    // Configura pinos de saída (motor)
    this._gpioSobe  = new Gpio(this._pinSobe,  { mode: Gpio.OUTPUT });
    this._gpioDesce = new Gpio(this._pinDesce, { mode: Gpio.OUTPUT });

    // Garante ambos em LOW ao iniciar
    this._gpioSobe.digitalWrite(0);
    this._gpioDesce.digitalWrite(0);

    // Configura encoder: pull-up + alert em borda de subida
    this._gpioEncoder = new Gpio(this._encoderPin, {
      mode: Gpio.INPUT,
      pullUpDown: Gpio.PUD_UP,
      alert: true,
    });
    // Filtro de 1ms — substitui o PCNT debounce do ESP32
    this._gpioEncoder.glitchFilter(1000);
    this._gpioEncoder.on('alert', (level: number) => {
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
      // Valida steps salvos antes de restaurar
      if (saved.steps >= 0 && saved.steps <= this._pulsosCalibrados) {
        this._steps = saved.steps;
      }
    }
  }

  subir(): void {
    // BTS7960: LPWM = 0, RPWM = PWM duty
    this._gpioDesce.digitalWrite(0);
    this._gpioSobe.hardwarePwmWrite(MOTOR.PWM_FREQ, this._dutyFromVel());
    this._direcao = 'SUBIR';
    this._ultimoPulsoTime = Date.now();
  }

  descer(): void {
    // BTS7960: RPWM = 0, LPWM = PWM duty
    this._gpioSobe.digitalWrite(0);
    this._gpioDesce.hardwarePwmWrite(MOTOR.PWM_FREQ, this._dutyFromVel());
    this._direcao = 'DESCER';
    this._ultimoPulsoTime = Date.now();
  }

  parar(): void {
    this._gpioSobe.digitalWrite(0);
    this._gpioDesce.digitalWrite(0);
    this._direcao = 'PARAR';
  }

  setVelocidade(v: number): void {
    this._velocidade = Math.max(0, Math.min(255, v));
    // Re-aplica PWM se estiver em movimento
    if (this._direcao === 'SUBIR') {
      this._gpioSobe.hardwarePwmWrite(MOTOR.PWM_FREQ, this._dutyFromVel());
    } else if (this._direcao === 'DESCER') {
      this._gpioDesce.hardwarePwmWrite(MOTOR.PWM_FREQ, this._dutyFromVel());
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
    // Remove o listener de encoder
    this._gpioEncoder.removeAllListeners('alert');
  }

  private _dutyFromVel(): number {
    // Converte 0–255 para escala pigpio hardwarePwm: 0–1.000.000
    return Math.round(this._velocidade / 255 * 1_000_000);
  }
}
