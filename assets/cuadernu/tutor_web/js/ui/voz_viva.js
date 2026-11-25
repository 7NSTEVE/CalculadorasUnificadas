/**
 * Conversación hablada en vivo. El profesor oye, ve lo que hay en la hoja (contexto en texto) y
 * escribe con las mismas herramientas que las lecciones. Lo que escribe queda en la hoja.
 */

import { DuplexAudioEngine } from '../audio_duplex_worklet.js';
import { bus, enviar, enviarBinario } from '../nucleo/api.js';
import { tintaDe } from '../leccion/reproductor.js';
import { accionA } from './plan_ia.js';
import { aviso, anunciar } from './avisos.js';

const $ = (id) => document.getElementById(id);

export class VozViva {
  constructor({ asistente, rep }) {
    this.asistente = asistente;
    this.rep = rep;
    this.activa = false;
    this.cola = [];
    this.vista = null;
    this.quien = '';
    this.buf = '';
    this.pico = 0;
    this.audio = new DuplexAudioEngine({
      onMicFramePcm16: (p) => { if (this.activa) enviarBinario(p); },
      onMicLevel: (rms) => {
        this.pico = Math.max(this.pico * 0.92, rms);
        $('nivel-mic').style.width = `${Math.min(100, Math.round(Math.min(1, rms * 14) * 100))}%`;
      },
      onLiveAudioStart: () => {},
      onLiveAudioDrain: () => { this._apurar(); },
    });
    bus.on('binario', (buf) => this._binario(buf));
    bus.on('ws:LIVE_VOICE_STATUS', (m) => this._estado(m));
    bus.on('ws:LIVE_CANVAS_ACTION', (m) => this._accion(m));
    bus.on('ws:ASR_FINAL', (m) => this._transcribe('Tú', m.text || ''));
    bus.on('ws:LIVE_OUTPUT_TRANSCRIPT', (m) => this._transcribe('Profesor', m.text || ''));
    bus.on('ws:CONTROL_HARD_STOP', () => this.audio.hardStopPlayback());
    bus.on('ws:MIC_STREAM_CONFIRMED', () => bus.emit('estado', { texto: 'Te escucho', clase: 'vivo' }));
    bus.on('ws:STATE_CHANGE', (m) => { if (m.state === 'WAITING_FOR_STUDENT') { this.quien = ''; this.vista = null; this._apurar(); } });
    bus.on('conexion', ({ estado }) => { if (estado !== 'conectado' && this.activa) this._cortar('Se cortó la voz en vivo.'); });
  }

  async alternar() {
    if (this.activa || this.arrancando) { this._cortar(''); enviar({ type: 'STOP_LIVE_VOICE' }); return; }
    this.arrancando = true;
    this.rep.interrumpir();
    this.rep.cortarMini();
    try {
      bus.emit('estado', { texto: 'Activando micrófono', clase: '' });
      await this.audio.unlockPlayback();
      this.pico = 0;
      await this.audio.startMicCapture('');
      enviar({ type: 'START_LIVE_VOICE', contexto: this.asistente.contexto() });
      $('btn-mic').setAttribute('aria-pressed', 'true');
    } catch (err) {
      this.arrancando = false;
      bus.emit('estado', { texto: 'Listo', clase: 'bien' });
      aviso(`No pude usar el micrófono: ${err && err.message ? err.message : err}`, { mal: true });
    }
  }

  _estado(m) {
    if (m.ok && m.model) {
      this.activa = true;
      this.arrancando = false;
      $('btn-mic').setAttribute('aria-pressed', 'true');
      bus.emit('estado', { texto: 'Voz en vivo', clase: 'vivo' });
      anunciar('Voz en vivo activada');
      setTimeout(() => { if (this.activa && this.pico < 0.002) aviso('No llega sonido del micrófono. Revisa que no esté silenciado.', { mal: true }); }, 4500);
    } else if (m.active === false) this._cortar('');
    else if (!m.ok) this._cortar(`Voz en vivo no disponible: ${m.error || 'sin conexión'}`);
  }

  _cortar(msj) {
    this.activa = false;
    this.arrancando = false;
    this.audio.stopMicCapture();
    this.audio.hardStopPlayback();
    $('btn-mic').setAttribute('aria-pressed', 'false');
    $('nivel-mic').style.width = '0';
    bus.emit('estado', { texto: 'Listo', clase: 'bien' });
    bus.emit('subtitulo', null);
    this._apurar();
    if (msj) aviso(msj, { mal: true });
  }

  _binario(buf) {
    const b = new Uint8Array(buf);
    if (b.length < 10 || b[0] !== 0x43 || b[1] !== 0x55 || b[2] !== 2) return;
    this.audio.enqueueLivePcm24(b.subarray(8));
  }

  _transcribe(quien, txt) {
    if (!txt) return;
    if (this.quien !== quien) { this.quien = quien; this.buf = ''; }
    this.buf += txt;
    $('subtitulo-quien').textContent = quien;
    const pal = this.buf.trim().split(/\s+/).slice(-34);
    bus.emit('subtitulo', { palabras: pal.map((p) => [p, 0, 0]), actual: pal.length - 1, clave: `viva:${quien}:${this.buf.length}` });
    if (quien === 'Profesor') setTimeout(() => { $('subtitulo-quien').textContent = 'Profesor'; }, 0);
  }

  /* ------------------------------------------------ escritura mientras habla */

  _accion(m) {
    const accs = accionA(m.action || m);
    if (!accs.length) return;
    const espera = Math.max(0, this.audio.getLivePlaybackLeadMs() - 250);
    setTimeout(() => { this.cola.push(...accs); this._seguir(); }, Math.min(4000, espera));
  }

  _vista() {
    if (this.vista && this.vista.el.isConnected) return this.vista;
    this.asistente.acciones.pizarra();
    this.vista = this.rep.lec ? this.rep.aclaracion('En vivo', 'pregunta') : this.rep.tablero.nuevoPaso({ n: 0, titulo: 'En vivo', clase: 'aclaracion pregunta' });
    return this.vista;
  }

  _seguir() {
    if (this.escribiendo) return;
    const a = this.cola.shift();
    if (!a) return;
    if (a.a === 'visual') { this.rep.cambiarVisual(a); this._seguir(); return; }
    const t = tintaDe(a, this.rep.tablero);
    if (!t) { this._seguir(); return; }
    t.progreso(0);
    this.rep.tablero.agregar(t, { paso: this._vista(), id: a.id });
    this.rep.tablero.seguir(t.el);
    this.escribiendo = t;
    let p = 0;
    let antes = performance.now();
    const paso = (ahora) => {
      if (this.escribiendo !== t) return;
      const dt = Math.min(100, ahora - antes);
      antes = ahora;
      const ritmo = this.audio.isLiveAudioPlaying() ? this.audio.getLiveSpeechSpeedFactor() : 1.4;
      p = Math.min(1, p + (dt / t.duracionNatural) * Math.max(0.3, ritmo) * (this.cola.length > 1 ? 1.8 : 1.1));
      t.progreso(p);
      if (p >= 1) { this.escribiendo = null; this._seguir(); return; }
      requestAnimationFrame(paso);
    };
    requestAnimationFrame(paso);
  }

  _apurar() {
    if (this.escribiendo) { this.escribiendo.completar(); this.escribiendo = null; }
    while (this.cola.length) {
      const a = this.cola.shift();
      if (a.a === 'visual') { this.rep.cambiarVisual(a); continue; }
      const t = tintaDe(a, this.rep.tablero);
      if (!t) continue;
      this.rep.tablero.agregar(t, { paso: this._vista(), id: a.id });
      t.completar();
    }
  }
}
