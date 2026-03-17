import * as mqtt from 'mqtt';
import { MQTT_CONFIG, PRESET } from './config';

export type MqttComando = 'SUBIR' | 'DESCER' | 'PARAR' | 'CALIBRAR';

const COMANDOS_VALIDOS: MqttComando[] = ['SUBIR', 'DESCER', 'PARAR', 'CALIBRAR'];

export class MqttClient {
  private _client: mqtt.MqttClient | null = null;
  private _onComandoHandler: ((cmd: MqttComando) => void) | null = null;
  private _onPresetHandler: ((alvoEmCm: number) => void) | null = null;

  async connect(): Promise<void> {
    const { BROKER, PORT, USER, PASS, CLIENT_ID, RECONNECT_PERIOD_MS } = MQTT_CONFIG;

    this._client = await mqtt.connectAsync(`mqtt://${BROKER}:${PORT}`, {
      clientId: CLIENT_ID,
      username: USER || undefined,
      password: PASS || undefined,
      reconnectPeriod: RECONNECT_PERIOD_MS,
      will: {
        topic: 'mesa/disponivel',
        payload: Buffer.from('offline'),
        qos: 1,
        retain: true,
      },
    });

    // Anuncia disponibilidade
    await this._client.publishAsync('mesa/disponivel', 'online', { retain: true, qos: 1 });

    // Subscreve tópicos de comando
    await this._client.subscribeAsync(['mesa/comando', 'mesa/preset']);

    this._client.on('message', (topic: string, payload: Buffer) => {
      const msg = payload.toString().trim();
      if (topic === 'mesa/comando') {
        this._handleComando(msg);
      } else if (topic === 'mesa/preset') {
        this._handlePreset(msg);
      }
    });

    this._client.on('error', (err: Error) => {
      console.error('[MQTT] Erro:', err.message);
    });

    this._client.on('reconnect', () => {
      console.log('[MQTT] Reconectando...');
    });

    this._client.on('connect', () => {
      console.log('[MQTT] Conectado ao broker');
      // Re-anuncia ao reconectar
      this._client?.publish('mesa/disponivel', 'online', { retain: true, qos: 1 });
    });

    console.log(`[MQTT] Conectado a ${BROKER}:${PORT}`);
  }

  onComando(handler: (cmd: MqttComando) => void): void {
    this._onComandoHandler = handler;
  }

  onPreset(handler: (alvoEmCm: number) => void): void {
    this._onPresetHandler = handler;
  }

  publishStatus(status: string): void {
    this._publish('mesa/status', status, { retain: true });
  }

  publishPosicao(cm: number): void {
    this._publish('mesa/posicao', cm.toFixed(2), { retain: true });
  }

  publishStepsEsquerda(steps: number): void {
    this._publish('mesa/steps/esquerda', String(steps));
  }

  publishStepsDireita(steps: number): void {
    this._publish('mesa/steps/direita', String(steps));
  }

  publishCalibration(payload: object): void {
    this._publish('mesa/calibracao', JSON.stringify(payload));
  }

  publishInfo(payload: object): void {
    this._publish('mesa/info', JSON.stringify(payload));
  }

  publishAvailable(online: boolean): void {
    this._publish('mesa/disponivel', online ? 'online' : 'offline', { retain: true, qos: 1 });
  }

  async disconnect(): Promise<void> {
    if (!this._client) return;
    await this._client.publishAsync('mesa/disponivel', 'offline', { retain: true, qos: 1 });
    await this._client.endAsync();
    this._client = null;
  }

  private _handleComando(msg: string): void {
    const cmd = msg.toUpperCase() as MqttComando;
    if (!COMANDOS_VALIDOS.includes(cmd)) {
      console.warn(`[MQTT] Comando desconhecido: ${msg}`);
      return;
    }
    this._onComandoHandler?.(cmd);
  }

  private _handlePreset(msg: string): void {
    const alvo = parseFloat(msg);
    if (isNaN(alvo) || alvo < PRESET.ALTURA_MIN_CM || alvo > PRESET.ALTURA_MAX_CM) {
      console.warn(`[MQTT] Preset inválido: ${msg} (fora de ${PRESET.ALTURA_MIN_CM}–${PRESET.ALTURA_MAX_CM} cm)`);
      return;
    }
    this._onPresetHandler?.(alvo);
  }

  private _publish(topic: string, payload: string, opts?: mqtt.IClientPublishOptions): void {
    this._client?.publish(topic, payload, opts ?? {}, (err) => {
      if (err) console.error(`[MQTT] Erro ao publicar em ${topic}:`, err.message);
    });
  }
}
