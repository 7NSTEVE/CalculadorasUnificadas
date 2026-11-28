/**
 * Voz de las lecciones. Todos los motores exponen lo mismo: una POSICIÓN en el texto hablado
 * (índice de carácter, con decimales). El reproductor escribe según esa posición, no según
 * milisegundos, así que da igual quién hable o a qué velocidad.
 *
 *   nube   -> <audio> con mapa tiempo→carácter (pausa, salto y velocidad sin cambiar el tono)
 *   local  -> voz de Windows (speechSynthesis), un tramo a la vez, con eventos por palabra
 *   mudo   -> reloj silencioso para quien prefiere leer
 */

import { ajustes, pedir, urlApi } from '../nucleo/api.js';

const CPS = 15.5; // caracteres por segundo de habla tranquila

/* ---------------------------------------------------------------- nube */

class VozNube {
  constructor(meta) {
    this.tipo = 'nube';
    this.meta = meta;
    this.audio = new Audio();
    this.audio.preload = 'auto';
    this.audio.src = urlApi(`/api/voz/audio/${meta.clave}`);
    this.audio.preservesPitch = true;
    this.mapa = meta.mapa || [[0, 0], [meta.dur_ms, 1]];
    this.fin = this.mapa[this.mapa.length - 1][1];
    this.terminada = false;
    this.alTerminar = null;
    this.audio.addEventListener('ended', () => { this.terminada = true; if (this.alTerminar) this.alTerminar(); });
    this.audio.addEventListener('error', () => { this.fallo = true; if (this.alFallar) this.alFallar(); });
  }

  async iniciar() {
    this.audio.playbackRate = ajustes.vel;
    await this.audio.play();
  }

  pausar() { this.audio.pause(); }

  async seguir() {
    this.audio.playbackRate = ajustes.vel;
    if (!this.terminada) await this.audio.play();
  }

  velocidad(v) { this.audio.playbackRate = v; }

  posicion() {
    if (this.terminada) return this.fin;
    const t = this.audio.currentTime * 1000;
    const m = this.mapa;
    let i = 1;
    while (i < m.length && m[i][0] < t) i += 1;
    if (i >= m.length) return this.fin;
    const [t0, c0] = m[i - 1];
    const [t1, c1] = m[i];
    return c0 + (c1 - c0) * Math.max(0, Math.min(1, (t - t0) / Math.max(1, t1 - t0)));
  }

  irA(car) {
    const m = this.mapa;
    let i = 1;
    while (i < m.length && m[i][1] < car) i += 1;
    const [t0, c0] = m[Math.max(0, i - 1)];
    const [t1, c1] = m[Math.min(m.length - 1, i)];
    const f = c1 > c0 ? (car - c0) / (c1 - c0) : 0;
    this.audio.currentTime = Math.max(0, (t0 + (t1 - t0) * f) / 1000);
    this.terminada = false;
  }

  get activa() { return !this.audio.paused && !this.terminada; }

  soltar() {
    this.alTerminar = null;
    this.alFallar = null;
    try { this.audio.pause(); this.audio.removeAttribute('src'); this.audio.load(); } catch { /* nada */ }
  }
}

/* ---------------------------------------------------------------- local */

let vocesCache = [];

export function vocesLocales() {
  if (!('speechSynthesis' in window)) return [];
  const todas = window.speechSynthesis.getVoices();
  if (todas.length) vocesCache = todas;
  return vocesCache.filter((v) => /^es(-|_|$)/i.test(v.lang));
}

if ('speechSynthesis' in window) {
  window.speechSynthesis.addEventListener('voiceschanged', vocesLocales);
  vocesLocales();
}

function mejorVoz(soloLocal = false) {
  const vs = vocesLocales().filter((v) => !soloLocal || v.localService);
  if (!vs.length) return null;
  if (ajustes.vozLocal) {
    const e = vs.find((v) => v.name === ajustes.vozLocal);
    if (e) return e;
  }
  const puntos = (v) => {
    let p = 0;
    if (/natural|online/i.test(v.name)) p += 6;
    if (/es-CR/i.test(v.lang)) p += 5;
    if (/es-MX|es-419|es-US/i.test(v.lang)) p += 3;
    if (/sabina|dalia|maria|juan|raul/i.test(v.name)) p += 2;
    if (v.localService) p += 1;
    return p;
  };
  return [...vs].sort((a, b) => puntos(b) - puntos(a))[0];
}

export function hayVozLocal() {
  return Boolean(mejorVoz());
}

class VozLocal {
  /** tramos: [{c0, c1}] sobre `texto`. Se habla tramo por tramo para poder pausar de verdad. */
  constructor(texto, tramos, voz = mejorVoz(), ritmo = 1) {
    this.tipo = 'local';
    this.texto = texto;
    this.tramos = tramos.length ? tramos : [{ c0: 0, c1: texto.length }];
    this.fin = texto.length;
    this.i = 0;
    this.pos = 0;
    this.terminada = false;
    this.pausada = true;
    this.generacion = 0;
    this.ancla = { pos: 0, t: performance.now(), cps: CPS };
    this.voz = voz;
    this.ritmo = ritmo;
    this.alTerminar = null;
  }

  async iniciar() {
    this.pausada = false;
    this._hablar(0);
  }

  _hablar(desde = 0) {
    if (this.i >= this.tramos.length) { this._fin(); return; }
    const gen = (this.generacion += 1);
    const tr = this.tramos[this.i];
    const ini = tr.c0 + desde;
    const u = new SpeechSynthesisUtterance(this.texto.slice(ini, tr.c1));
    if (this.voz) { u.voice = this.voz; u.lang = this.voz.lang; } else u.lang = 'es-MX';
    u.rate = Math.max(0.6, Math.min(1.8, ajustes.vel * 1.02 * this.ritmo));
    this.ancla = { pos: ini, t: performance.now() + 120, cps: CPS * ajustes.vel * this.ritmo };
    this.tope = tr.c1;
    let vivo = false;
    const guardia = setTimeout(() => {
      // Algunas voces nunca disparan eventos: se sigue con el reloj y no se queda colgado
      if (gen === this.generacion && !vivo) this._sinEventos(gen, ini, tr.c1);
    }, 2500);
    u.onstart = () => { vivo = true; this.ancla.t = performance.now(); };
    u.onboundary = (e) => {
      if (gen !== this.generacion || e.name === 'sentence') return;
      vivo = true;
      const p = ini + e.charIndex;
      const ahora = performance.now();
      const dt = (ahora - this.ancla.t) / 1000;
      const dp = p - this.ancla.pos;
      let cps = this.ancla.cps;
      if (dt > 0.15 && dp > 1) cps = cps * 0.6 + Math.max(6, Math.min(34, dp / dt)) * 0.4;
      this.ancla = { pos: p, t: ahora, cps, palabra: e.charLength || 8 };
    };
    u.onend = () => {
      clearTimeout(guardia);
      if (gen !== this.generacion) return;
      this.pos = tr.c1;
      this.ancla = { pos: tr.c1, t: performance.now(), cps: 0 };
      this.i += 1;
      if (this.alTramo) { const r = this.alTramo(this.i); if (r && r.then) { r.then(() => { if (gen === this.generacion && !this.pausada) this._hablar(0); }); return; } }
      if (!this.pausada) this._hablar(0);
    };
    u.onerror = (e) => {
      clearTimeout(guardia);
      if (gen !== this.generacion || e.error === 'interrupted' || e.error === 'canceled') return;
      this._sinEventos(gen, ini, tr.c1);
    };
    window.speechSynthesis.cancel();
    window.speechSynthesis.speak(u);
  }

  _sinEventos(gen, ini, fin) {
    const cps = CPS * ajustes.vel * this.ritmo;
    const ms = ((fin - ini) / cps) * 1000;
    this.ancla = { pos: ini, t: performance.now(), cps };
    setTimeout(() => {
      if (gen !== this.generacion || this.pausada) return;
      this.i += 1;
      this._hablar(0);
    }, ms);
  }

  _fin() {
    this.terminada = true;
    this.pos = this.fin;
    if (this.alTerminar) this.alTerminar();
  }

  posicion() {
    if (this.terminada) return this.fin;
    if (this.pausada) return this.pos;
    const a = this.ancla;
    const est = a.pos + Math.max(0, (performance.now() - a.t) / 1000) * a.cps;
    // Nunca adelantarse más de una palabra y pico a lo último que se oyó
    const techo = Math.min(this.tope ?? this.fin, a.pos + (a.palabra || 10) + 14);
    this.pos = Math.max(this.pos, Math.min(est, techo));
    return this.pos;
  }

  pausar() {
    this.posicion();
    this.pausada = true;
    this.generacion += 1;
    window.speechSynthesis.cancel();
  }

  async seguir() {
    if (this.terminada || !this.pausada) return;
    this.pausada = false;
    // Retoma desde el inicio de la palabra donde quedó
    const tr = this.tramos[Math.min(this.i, this.tramos.length - 1)];
    let off = Math.max(0, Math.min(this.pos, tr.c1 - 1) - tr.c0);
    const seg = this.texto.slice(tr.c0, tr.c0 + off);
    off = Math.max(0, seg.lastIndexOf(' ') + 1);
    this._hablar(off);
  }

  velocidad() { if (!this.pausada && !this.terminada) { this.pausar(); this.seguir(); } }

  irA(car) {
    this.generacion += 1;
    window.speechSynthesis.cancel();
    this.terminada = false;
    this.i = Math.max(0, this.tramos.findIndex((t) => car < t.c1));
    this.pos = car;
    if (!this.pausada) { const tr = this.tramos[this.i]; this._hablar(Math.max(0, car - tr.c0)); }
  }

  get activa() { return !this.pausada && !this.terminada; }

  soltar() {
    this.alTerminar = null;
    this.alTramo = null;
    this.generacion += 1;
    this.pausada = true;
    if ('speechSynthesis' in window) window.speechSynthesis.cancel();
  }
}

/* ---------------------------------------------------------------- muda */

class VozMuda {
  constructor(texto, factor = 1) {
    this.tipo = 'muda';
    this.fin = Math.max(1, texto.length);
    this.pos = 0;
    this.pausada = true;
    this.factor = factor;
    this.terminada = false;
    this.alTerminar = null;
  }

  async iniciar() { this.pausada = false; this.t = performance.now(); }
  posicion() {
    if (!this.pausada && !this.terminada) {
      const ahora = performance.now();
      this.pos += ((ahora - this.t) / 1000) * CPS * ajustes.vel * this.factor;
      this.t = ahora;
      if (this.pos >= this.fin) { this.pos = this.fin; this.terminada = true; if (this.alTerminar) this.alTerminar(); }
    }
    return this.pos;
  }

  pausar() { this.posicion(); this.pausada = true; }
  async seguir() { this.pausada = false; this.t = performance.now(); }
  velocidad() {}
  irA(car) { this.pos = car; this.terminada = false; this.t = performance.now(); }
  get activa() { return !this.pausada && !this.terminada; }
  soltar() { this.alTerminar = null; this.pausada = true; }
}

/* ---------------------------------------------------------------- fábrica */

const metas = new Map();

function vozActivaParaMotor() {
  if (ajustes.motorVoz === 'local_http') return ajustes.vozLocalHttp || 'es-default';
  if (ajustes.motorVoz === 'kokoro') return ajustes.vozKokoro || 'ef_dora';
  return ajustes.voz;
}

function claveMeta(texto, estilo = '') {
  return `${ajustes.motorVoz}|${vozActivaParaMotor()}|${estilo}|${texto}`;
}

/** Pide (o reutiliza) el audio de un paso. `urgente=false` sirve para precargar el siguiente. */
export function prepararVoz(paso, urgente = true) {
  const texto = paso.habla || '';
  if (!texto || !ajustes.narracion || ajustes.motorVoz === 'local') return Promise.resolve(null);
  const estilo = paso.estilo || '';
  const k = claveMeta(texto, estilo);
  const guardada = metas.get(k);
  if (guardada) return guardada;
  const cuerpo = {
    texto,
    cortes: paso.tramos.map((t) => t.c0),
    voz: vozActivaParaMotor(),
    motor: ajustes.motorVoz,
    estilo,
    urgente,
  };
  if (ajustes.motorVoz === 'local_http' && ajustes.servidorVozLocal) {
    cuerpo.servidor_local = ajustes.servidorVozLocal;
  }
  const esLocalPesado = ajustes.motorVoz === 'kokoro' || ajustes.motorVoz === 'local_http';
  const pr = pedir('/api/voz', {
    metodo: 'POST', tiempo: urgente ? (esLocalPesado ? 30000 : 12000) : 70000,
    cuerpo,
  }).catch(() => ({ motor: 'local', motivo: 'sin conexión' }));
  metas.set(k, pr);
  pr.then((m) => { if (!m || !m.clave) metas.delete(k); else if (metas.size > 80) metas.delete(metas.keys().next().value); });
  return pr;
}

export const VOCES_NEURALES = [
  ['es-MX-DaliaNeural', 'Dalia (México · clara y natural)'],
  ['es-CR-MariaNeural', 'María (Costa Rica · natural)'],
  ['es-CR-JuanNeural', 'Juan (Costa Rica · natural)'],
  ['es-CO-SalomeNeural', 'Salomé (Colombia · cálida)'],
  ['es-CO-GonzaloNeural', 'Gonzalo (Colombia · cálido)'],
  ['es-MX-JorgeNeural', 'Jorge (México · claro y firme)'],
  ['en-US-AvaMultilingualNeural', 'Ava (Multilingüe · muy expresiva)'],
  ['en-US-AndrewMultilingualNeural', 'Andrew (Multilingüe · conversacional)'],
  ['es-ES-XimenaNeural', 'Ximena (España · expresiva)'],
  ['es-ES-AlvaroNeural', 'Álvaro (España · claro)'],
];

export const VOCES_KOKORO = [
  ['ef_dora', 'Dora (Kokoro-82M · femenina)'],
  ['em_alex', 'Alex (Kokoro-82M · masculina)'],
  ['em_santa', 'Santa (Kokoro-82M · masculina)'],
];

let audioPrueba = null;

/** Reproduce una frase corta de muestra con la voz y motor elegidos para compararla al instante. */
export async function probarVoz(vozId) {
  if (audioPrueba) {
    try { audioPrueba.pause(); audioPrueba.removeAttribute('src'); audioPrueba.load(); } catch { /* nada */ }
    audioPrueba = null;
  }
  const muestra = 'Hola, así suena mi voz mientras resolvemos física paso a paso en la pizarra.';
  const cuerpo = {
    texto: muestra,
    cortes: [0],
    voz: vozId || vozActivaParaMotor(),
    motor: ajustes.motorVoz,
    urgente: true,
  };
  if (ajustes.motorVoz === 'local_http' && ajustes.servidorVozLocal) {
    cuerpo.servidor_local = ajustes.servidorVozLocal;
  }
  const meta = await pedir('/api/voz', { metodo: 'POST', tiempo: 30000, cuerpo });
  if (!meta || !meta.clave) return false;
  const a = new Audio(urlApi(`/api/voz/audio/${meta.clave}`));
  a.preservesPitch = true;
  a.playbackRate = ajustes.vel;
  audioPrueba = a;
  await a.play();
  return true;
}

/**
 * Crea la voz de un paso. Si la voz neural no responde en `paciencia` ms se usa la voz del sistema:
 * nadie se queda viendo una pizarra vacía.
 */
export async function crearVoz(paso, { paciencia = 9000, motor = 'auto', soloLocal = false } = {}) {
  const texto = paso.habla || '';
  const ritmo = paso.estilo === 'despacio' ? 0.8 : 1;
  if (!texto || !ajustes.narracion) return new VozMuda(texto || ' '.repeat(40), 1.15 * ritmo);
  const tramos = paso.tramos.map((t) => ({ c0: t.c0, c1: t.c1 }));
  if (soloLocal) {
    const voz = mejorVoz(true);
    return voz ? new VozLocal(texto, tramos, voz, ritmo) : new VozMuda(texto, ritmo);
  }
  const localOk = hayVozLocal();
  const esLocalPesado = ajustes.motorVoz === 'kokoro' || ajustes.motorVoz === 'local_http';
  const pac = esLocalPesado ? Math.max(paciencia, 28000) : paciencia;
  if (ajustes.motorVoz !== 'local' && (motor !== 'local' || !localOk)) {
    const meta = await Promise.race([
      prepararVoz(paso, true),
      new Promise((r) => setTimeout(() => r(null), localOk ? pac : 30000)),
    ]);
    if (meta && meta.clave) return new VozNube(meta);
  }
  if (localOk) return new VozLocal(texto, tramos);
  return new VozMuda(texto);
}

export function callarTodo() {
  if (audioPrueba) {
    try { audioPrueba.pause(); audioPrueba.removeAttribute('src'); audioPrueba.load(); } catch { /* nada */ }
    audioPrueba = null;
  }
  if ('speechSynthesis' in window) window.speechSynthesis.cancel();
}
