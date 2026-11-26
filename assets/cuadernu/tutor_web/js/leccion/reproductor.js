/**
 * Reproductor de lecciones: une voz, escritura, marcas y visual con un solo reloj.
 *
 * Lo que se ve depende de la POSICIÓN en lo que se está diciendo. Por eso se puede pausar,
 * retroceder, repetir, saltar o cambiar la velocidad y todo sigue cuadrando; y adelantar un paso
 * lo completa en vez de botar lo que faltaba.
 */

import { TintaMate, TintaTexto, tintaLista, tintaTabla } from '../mate/tinta.js';
import { ajustes, bus, menosMovimiento, pedir } from '../nucleo/api.js';
import { Enunciado } from './enunciado.js';
import { Citas } from '../pizarra/citas.js';
import { Leyenda } from '../pizarra/simbolos.js';
import { callarTodo, crearVoz, prepararVoz } from './voz_leccion.js';

const ACELERA = 2.6; // la mano puede ir hasta 2,6 veces su ritmo natural

/** Crea la tinta de una acción de escritura. */
export function tintaDe(a, tablero) {
  const px = tablero.px;
  switch (a.a) {
    case 'mate': {
      const t = new TintaMate(a.tex, { px, color: a.color, maxAncho: tablero.anchoUtil(), clase: a.rol ? `rol-${a.rol}` : '' });
      t.factor = 1;
      return t;
    }
    case 'texto': return new TintaTexto(a.txt, { clase: a.clase || '', color: a.color, px: px * 0.62 });
    case 'lista': return tintaLista(a.items || [], { px: px * 0.62, color: a.color });
    case 'tabla': return tintaTabla(a.cab || [], a.filas || [], { px: px * 0.58, resaltar: a.resaltar || [], pie: a.pie || '' });
    default: return null;
  }
}

/** Una acción en ejecución. */
class Obra {
  constructor(rep, acc, c0, c1, tramo, vista) {
    this.rep = rep;
    this.vista = vista;
    this.acc = acc;
    this.c0 = c0;
    this.c1 = Math.max(c0 + 0.5, c1);
    this.tramo = tramo;
    this.p = 0;
    this.empezada = false;
    this.terminada = false;
    this.cosa = null;
    this.dur = 600;
  }

  empezar() {
    if (this.empezada) return;
    this.empezada = true;
    const { rep, acc } = this;
    if (acc.a === 'visual') {
      rep.cambiarVisual(acc);
      this.terminar();
      return;
    }
    if (acc.a === 'opcion') {
      if (rep.enun) {
        rep.enun.opcionesEn(this.vista);
        rep.enun.opcion(acc.letra, acc.estado, acc.motivo);
        const li = this.vista && this.vista.el.querySelector(`.opcion[data-letra="${acc.letra}"]`);
        if (li) rep.tablero.seguir(li);
      }
      this.terminar();
      return;
    }
    if (acc.a === 'dato') {
      const m = rep.enun ? rep.enun.marcarDato(acc.frase, acc.color || 'naranja') : null;
      if (!m) { this.terminar(); return; }
      this.cosa = m;
      this.dur = 700;
      return;
    }
    if (acc.a === 'marca') {
      this.cosa = rep.tablero.marcas.crear({ ...acc, nota: acc.nota || '' });
      this.dur = this.cosa.coste * 95;
      return;
    }
    const t = tintaDe(acc, rep.tablero);
    if (!t) { this.terminar(); return; }
    t.progreso(0);
    rep.tablero.agregar(t, { paso: this.vista, id: acc.id });
    if (acc.a === 'mate' && rep.citas) rep.citas.aplicar(acc, t);
    if (acc.a === 'mate' && !rep._pintando) {
      rep.mostrarSimbolos(acc.id);
      if (/\\int/.test(acc.tex || '')) rep.integralEnVisual();
    }
    this.cosa = t;
    this.dur = t.duracionNatural;
    rep.tablero.seguir(t.el);
  }

  avanzar(pos, dt) {
    if (this.terminada) return;
    if (!this.empezada) {
      if (pos < this.c0) return;
      this.empezar();
      if (this.terminada) return;
    }
    const meta = Math.max(0, Math.min(1, (pos - this.c0) / (this.c1 - this.c0)));
    const tope = this.p + (dt / Math.max(120, this.dur)) * ACELERA * Math.max(1, ajustes.vel);
    this.p = menosMovimiento() ? (meta > 0 ? 1 : 0) : Math.min(meta, tope);
    if (this.cosa) this.cosa.progreso(this.p);
    if (this.p >= 1) this.terminar();
  }

  /** Milisegundos que le faltan a la mano para alcanzar a la voz. */
  atraso(pos) {
    if (this.terminada || pos < this.c1) return 0;
    return ((1 - this.p) * this.dur) / ACELERA;
  }

  terminar() {
    this.terminada = true;
    this.p = 1;
    if (this.cosa) this.cosa.completar();
  }

  completar() {
    this.empezar();
    this.terminar();
  }
}

export class Reproductor {
  constructor({ tablero, lateral }) {
    this.citas = new Citas(tablero);
    this.leyenda = new Leyenda({ lateral: lateral && lateral.el, escena: document.getElementById('escena') });
    this.mapaSimbolos = {};
    this._pintando = false;
    this.tablero = tablero;
    this.lateral = lateral;
    this.lec = null;
    this.i = -1;
    this.estado = 'vacio';
    this.voz = null;
    this.obras = [];
    this.vistas = [];
    this.vista = null;
    this.enun = null;
    this.gen = 0;
    this._raf = 0;
    this._bucle = this._bucle.bind(this);
    document.addEventListener('visibilitychange', () => {
      // Al volver a la pestaña no se "salta" lo que pasó en segundo plano
      this._antes = performance.now();
    });
    bus.on('ajuste:vel', (v) => { if (this.voz && this.voz.velocidad) this.voz.velocidad(v); });
  }

  /** Muestra unos segundos qué es cada letra nueva del renglón que se está escribiendo. */
  mostrarSimbolos(id) {
    const items = id && this.mapaSimbolos[id];
    if (items && items.length) this.leyenda.mostrar(items);
  }

  /**
   * Cuando se escribe una integral de conducción y la lección no pidió nada al visual, el visual barre
   * las capas y va sumando los escalones de temperatura: así se ve que la integral es esa suma.
   */
  integralEnVisual() {
    const v = this.lateral && this.lateral.visual;
    if (!v || typeof v._armar !== 'function' || v.armado) return;
    const f = String(v.foco ?? '');
    if (['barrido', 'dr', 'dx', 'perfil', 'listo'].includes(f) || f.startsWith('armar')) return;
    this.lateral.cambiarVisual({ foco: 'barrido' }, false);
  }

  get total() { return this.lec ? this.lec.pasos.length : 0; }
  get paso() { return this.lec && this.lec.pasos[this.i] ? this.lec.pasos[this.i] : null; }

  _estado(e) {
    this.estado = e;
    bus.emit('leccion:estado', { estado: e, i: this.i, total: this.total, lec: this.lec });
  }

  /* ---------------------------------------------------------------- carga */

  async cargar(lec, { desde = 0, reproducir = true } = {}) {
    this.soltar();
    const gen = (this.gen += 1);
    this.lec = lec;
    this.i = -1;
    this.motorLec = null;
    this.vistas = [];
    this.mapaSimbolos = (lec.simbolos && lec.simbolos.por_accion) || {};
    this.leyenda.ocultar();
    this.tablero.limpiar();
    this.lateral.abrirLeccion(lec);
    this.citas.usar(lec);
    this.enun = new Enunciado(this.tablero, lec);
    this.enun.alDato = (caja, color) => this.lateral.marcarDato(caja, color);
    bus.emit('leccion:abierta', lec);
    await this.enun.lista;
    if (gen !== this.gen) return;
    this.irA(Math.max(0, Math.min(desde, lec.pasos.length - 1)), { reproducir });
  }

  soltar() {
    this.gen += 1;
    this.cortarMini();
    if (this._raf) cancelAnimationFrame(this._raf);
    this._raf = 0;
    if (this.voz) { this.voz.soltar(); this.voz = null; }
    callarTodo();
    this.obras = [];
    bus.emit('subtitulo', null);
  }

  cerrar() {
    this.soltar();
    this.leyenda.ocultar();
    this.lec = null;
    this.i = -1;
    this.enun = null;
    this.vistas = [];
    this._estado('vacio');
  }

  /* ---------------------------------------------------------------- pasos */

  _armarObras(paso, vista = this.vista) {
    const obras = [];
    for (const [k, tr] of paso.tramos.entries()) {
      const acc = tr.acc || [];
      const costes = acc.map((a) => {
        if (a.a === 'visual' || a.a === 'opcion') return 0;
        if (a.a === 'marca' || a.a === 'dato') return 6;
        const largo = a.tex ? a.tex.replace(/\\[A-Za-z]+|[{}]/g, '').length : JSON.stringify(a.items || a.filas || a.txt || '').length;
        return Math.max(6, largo);
      });
      const suma = costes.reduce((x, y) => x + y, 0) || 1;
      let c = tr.c0;
      acc.forEach((a, j) => {
        const ancho = ((tr.c1 - tr.c0) * costes[j]) / suma;
        obras.push(new Obra(this, a, c, c + ancho, k, vista));
        c += ancho;
      });
    }
    return obras;
  }

  _vistaDe(i) {
    const p = this.lec.pasos[i];
    const v = this.tablero.nuevoPaso({ n: i + 1, total: this.total, titulo: p.titulo });
    const mapa = this.lec.formulario_mapa;
    if (mapa && Array.isArray(mapa.por_paso) && Array.isArray(mapa.celdas)) {
      const ids = mapa.por_paso[i] || [];
      if (ids.length) {
        const porId = new Map(mapa.celdas.map((c) => [c.id, c]));
        const celdasPaso = ids.map((id) => porId.get(id)).filter(Boolean);
        v.ponerFuentesFormulario(celdasPaso);
      }
    }
    this.vistas[i] = v;
    return v;
  }

  _enfocar(i) {
    this.vistas.forEach((v, k) => {
      if (!v) return;
      v.estado(k === i ? 'actual' : 'anterior');
      if (k < i - 1 && ajustes.foco && v.el.dataset.abierto !== 'si') v.plegar();
      else if (k === i - 1) v.desplegar();
    });
  }

  /** Dibuja un paso completo, sin voz (para saltar hacia adelante). */
  _pintarCompleto(i) {
    this.i = i;
    this.vista = this._vistaDe(i);
    const obras = this._armarObras(this.lec.pasos[i]);
    // Al pintar de golpe no salen las tarjetas de símbolos (serían una ráfaga)
    this._pintando = true;
    try { for (const o of obras) o.completar(); } finally { this._pintando = false; }
  }

  async irA(i, { reproducir = true } = {}) {
    if (!this.lec || i < 0 || i >= this.total) return;
    this.cortarMini();
    const gen = (this.gen += 1);
    if (this._raf) cancelAnimationFrame(this._raf);
    this._raf = 0;
    if (this.voz) { this.voz.soltar(); this.voz = null; }
    callarTodo();
    for (const o of this.obras) if (!o.terminada) o.completar();
    this.obras = [];

    // Quita lo que está de ese paso en adelante y lo agregado después (aclaraciones, cierre)
    for (let k = this.vistas.length - 1; k >= i; k -= 1) {
      if (this.vistas[k]) this.tablero.quitarPaso(this.vistas[k]);
    }
    for (const p of [...this.tablero.pasos]) {
      if (p.el.classList.contains('cierre')) this.tablero.quitarPaso(p);
      else if (p.el.classList.contains('aclaracion') && Number(p.el.dataset.de || 0) > i) this.tablero.quitarPaso(p);
    }
    this.vistas.length = Math.min(this.vistas.length, i);
    if (i === 0 && this.enun) { this.enun.reiniciarOpciones(); this.lateral.quitarDatos(); }
    if (i === 0 && this.lec.recorte) this.lateral.ver('enunciado');
    else if (i === 1 && this.lateral.visual) this.lateral.ver('visual');
    for (let k = this.vistas.length; k < i; k += 1) this._pintarCompleto(k);

    this.i = i;
    const paso = this.lec.pasos[i];
    this.vista = this._vistaDe(i);
    this._enfocar(i);
    bus.emit('subtitulo', null);
    this._sk = -1;
    this.obras = this._armarObras(paso);
    this._repuestos = new Set();
    bus.emit('leccion:paso', { i, total: this.total, paso, lec: this.lec });
    this.tablero.seguir(this.vista.el);
    this._guardar(false);
    if (i + 1 < this.total) setTimeout(() => { if (gen === this.gen) prepararVoz(this.lec.pasos[i + 1], false); }, 350);

    if (!reproducir) { this._estado('pausado'); return; }
    this._estado('cargando');
    const voz = await crearVoz(paso, this._vozDeLeccion());
    if (gen !== this.gen) { voz.soltar(); return; }
    this._recordarMotor(voz);
    this._usarVoz(voz, gen, paso);
  }

  /** Una sola voz por lección: si empezó con una, no cambia de persona a media explicación. */
  _vozDeLeccion() {
    return { motor: this.motorLec === 'local' ? 'local' : 'auto', paciencia: this.motorLec === 'nube' ? 11000 : 9500 };
  }

  _recordarMotor(voz) {
    if (voz.tipo === 'muda') return;
    if (this.motorLec === 'nube' && voz.tipo === 'local') bus.emit('aviso', { texto: 'La voz en la nube no responde; sigo con la de Windows.' });
    if (!this.motorLec || voz.tipo === 'local') this.motorLec = voz.tipo;
  }

  async _usarVoz(voz, gen, paso) {
    this.voz = voz;
    if (voz.tipo === 'local') {
      voz.alTramo = (k) => this._esperarMano(k, gen);
    }
    voz.alFallar = () => {
      // El audio se cayó a media frase: se sigue con voz local desde donde iba
      if (gen !== this.gen) return;
      const pos = this.pos || 0;
      bus.emit('aviso', { texto: 'Se cortó el audio; sigo con la voz de Windows.' });
      this.motorLec = 'local';
      crearVoz({ ...paso }, { paciencia: 0, motor: 'local' }).then((v) => { if (gen !== this.gen) { v.soltar(); return; } this.voz = v; v.iniciar().then(() => v.irA(pos)); });
    };
    try {
      await voz.iniciar();
    } catch (err) {
      // El navegador exige un clic antes de sonar
      if (gen !== this.gen) return;
      this._estado('pausado');
      bus.emit('aviso', { texto: 'Pulsa Continuar para activar el sonido.' });
      this._antes = performance.now();
      return;
    }
    if (gen !== this.gen) return;
    this._estado('hablando');
    bus.emit('voz:motor', voz.tipo);
    this._antes = performance.now();
    this._raf = requestAnimationFrame(this._bucle);
  }

  _esperarMano(k, gen) {
    return new Promise((res) => {
      const t0 = performance.now();
      const ver = () => {
        if (gen !== this.gen) { res(); return; }
        const pend = this.obras.some((o) => o.tramo < k && !o.terminada);
        if (!pend || performance.now() - t0 > 5000) { res(); return; }
        setTimeout(ver, 60);
      };
      ver();
    });
  }

  /* ---------------------------------------------------------------- bucle */

  _bucle(ahora) {
    this._raf = 0;
    if (!this.voz || this.estado === 'pausado') return;
    const dt = Math.min(120, ahora - this._antes);
    this._antes = ahora;
    const pos = this.voz.posicion();
    this.pos = pos;
    let todo = true;
    let atraso = 0;
    for (const o of this.obras) {
      // Nunca dos manos a la vez: si la anterior no terminó, esta espera
      o.avanzar(pos, dt);
      if (!o.terminada) { todo = false; atraso = Math.max(atraso, o.atraso(pos)); if (o.empezada) break; }
    }
    // Si la mano va muy atrás, la voz la espera en la pausa entre frases
    if (this.voz.tipo === 'nube' && this.voz.activa) {
      const k = this.paso.tramos.findIndex((t) => pos >= t.c0 && pos < t.c1);
      if (k > 0 && k !== this._tramo) {
        this._tramo = k;
        if (atraso > 900 && !this._repuestos.has(k)) { this._repuestos.add(k); this.voz.pausar(); this._reteniendo = true; }
      }
    }
    if (this._reteniendo && atraso < 120) { this._reteniendo = false; this.voz.seguir().catch(() => {}); }
    this._subtitulo(pos);
    if (this.voz.terminada && todo) { this._finPaso(); return; }
    this._raf = requestAnimationFrame(this._bucle);
  }

  _subtitulo(pos) {
    const tr = this.paso.tramos;
    let k = tr.findIndex((t) => pos < t.c1 + 0.5);
    if (k < 0) k = tr.length - 1;
    let w = -1;
    const pal = tr[k].pal;
    for (let j = 0; j < pal.length; j += 1) if (pos >= pal[j][1] - 0.5) w = j;
    if (k === this._sk && w === this._sw && this._si === this.i) return;
    this._sk = k; this._sw = w; this._si = this.i;
    bus.emit('subtitulo', { palabras: pal, actual: w, clave: `${this.lec.id}:${this.i}:${k}` });
  }

  _finPaso() {
    this._reteniendo = false;
    const ultimo = this.i >= this.total - 1;
    this._estado(ultimo ? 'fin' : 'espera');
    if (this.paso.tramos.length) this._subtitulo(1e9);
    bus.emit('leccion:finpaso', { i: this.i, total: this.total, paso: this.paso, lec: this.lec });
    this._guardar(ultimo);
    if (!ultimo && ajustes.autoSeguir && !this.paso.pregunta) {
      const gen = this.gen;
      setTimeout(() => { if (gen === this.gen && this.estado === 'espera') this.siguiente(); }, 1400);
    }
  }

  _guardar(hecha) {
    if (!this.lec) return;
    pedir('/api/progreso', { metodo: 'POST', cuerpo: { tipo: 'leccion', id: this.lec.id, paso: this.i, total: this.total, hecha } }).catch(() => {});
    bus.emit('progreso', { id: this.lec.id, paso: this.i, total: this.total, hecha });
  }

  /* ---------------------------------------------------------------- mandos */

  pausar() {
    if (!this.voz || !['hablando'].includes(this.estado)) return;
    this.voz.pausar();
    if (this.lateral.visual) this.lateral.visual.pausar();
    this._estado('pausado');
  }

  async seguir() {
    if (this.estado !== 'pausado') return;
    if (!this.voz) { this.irA(Math.max(0, this.i)); return; }
    if (this.lateral.visual) this.lateral.visual.seguir();
    this._estado('hablando');
    this._antes = performance.now();
    try { await this.voz.seguir(); } catch { this._estado('pausado'); return; }
    if (!this._raf) this._raf = requestAnimationFrame(this._bucle);
  }

  alternar() {
    if (this.estado === 'hablando') this.pausar();
    else if (this.estado === 'pausado') this.seguir();
    else if (this.estado === 'espera') this.siguiente();
  }

  repetir() { if (this.lec) this.irA(Math.max(0, this.i)); }

  siguiente() {
    if (!this.lec) return;
    if (this.i + 1 < this.total) this.irA(this.i + 1);
    else if (this.estado !== 'fin') {
      for (const o of this.obras) if (!o.terminada) o.completar();
      if (this.voz) { this.voz.soltar(); this.voz = null; }
      if (this._raf) cancelAnimationFrame(this._raf);
      this._raf = 0;
      bus.emit('subtitulo', null);
      this._finPaso();
    }
  }

  anterior() { if (this.lec && this.i > 0) this.irA(this.i - 1); }

  /** Detiene la voz sin perder lo escrito (para preguntar algo). */
  interrumpir() {
    if (this.estado === 'hablando') this.pausar();
  }

  cambiarVisual(a) {
    if (a.tipo) this.lateral.ponerVisual({ tipo: a.tipo, ...(a.spec || {}) });
    else if (a.cambio) this.lateral.cambiarVisual(a.cambio, this.i > 0 || !(this.lec && this.lec.recorte));
  }

  /* ---------------------------------------------------------------- aclaraciones */

  /** Bloque extra debajo del paso actual (por qué, de dónde salió, más despacio, respuesta de la IA). */
  aclaracion(titulo, clase = '') {
    const despues = (() => {
      let el = this.vista ? this.vista.el : null;
      while (el && el.nextElementSibling && el.nextElementSibling.classList.contains('aclaracion')) el = el.nextElementSibling;
      return el ? { el } : null;
    })();
    const v = this.tablero.nuevoPaso({ n: 0, titulo, clase: `aclaracion ${clase}` }, { despuesDe: despues && despues.el });
    v.el.dataset.de = String(this.i);
    return v;
  }

  /** Reproduce un mini paso (voz + escritura) dentro de una aclaración. */
  async contar(mini, titulo, clase = '', vistaYa = null) {
    const estabaHablando = this.estado === 'hablando';
    if (estabaHablando) this.pausar();
    if (this._mini) this._mini.cortar();
    const vista = vistaYa && vistaYa.el.isConnected ? vistaYa : this.aclaracion(titulo, clase);
    this.tablero.seguir(vista.el);
    const ctrl = { vivo: true, cortar() { this.vivo = false; if (this.voz) this.voz.soltar(); } };
    this._mini = ctrl;
    const obras = this._armarObras(mini, vista);
    const voz = await crearVoz(mini, this._vozDeLeccion());
    ctrl.voz = voz;
    if (!ctrl.vivo) { voz.soltar(); return vista; }
    try { await voz.iniciar(); } catch { /* sin sonido: se escribe igual */ }
    bus.emit('mini', true);
    await new Promise((res) => {
      let antes = performance.now();
      const paso = (t) => {
        if (!ctrl.vivo) { for (const o of obras) o.completar(); res(); return; }
        const dt = Math.min(120, t - antes);
        antes = t;
        const pos = voz.posicion();
        let todo = true;
        for (const o of obras) { o.avanzar(pos, dt); if (!o.terminada) { todo = false; if (o.empezada) break; } }
        const k = Math.max(0, mini.tramos.findIndex((q) => pos < q.c1 + 0.5));
        const pal = mini.tramos[k] ? mini.tramos[k].pal : [];
        let w = -1;
        pal.forEach((q, j) => { if (pos >= q[1] - 0.5) w = j; });
        bus.emit('subtitulo', { palabras: pal, actual: w, clave: `mini:${titulo}:${k}` });
        if (voz.terminada && todo) { res(); return; }
        requestAnimationFrame(paso);
      };
      requestAnimationFrame(paso);
    });
    voz.soltar();
    if (this._mini === ctrl) { this._mini = null; bus.emit('mini', false); bus.emit('subtitulo', null); }
    return vista;
  }

  cortarMini() { if (this._mini) this._mini.cortar(); }
}

/** Convierte un texto suelto (o renglones) en un mini paso reproducible. */
export function miniPaso(tramos) {
  let habla = '';
  const out = [];
  for (const t of tramos) {
    const hab = (t.habla ?? t.texto ?? '').replace(/\$[^$]*\$/g, ' ').replace(/[*_]/g, '').replace(/\s+/g, ' ').trim();
    const c0 = habla.length + (habla ? 1 : 0);
    habla = habla ? `${habla} ${hab}` : hab;
    const pal = [];
    let cur = c0;
    for (const p of hab.split(' ').filter(Boolean)) { pal.push([p, cur, cur + p.length]); cur += p.length + 1; }
    out.push({ v: hab, c0, c1: Math.max(c0 + 1, habla.length), pal, acc: t.acc || [] });
  }
  return { titulo: '', habla, tramos: out };
}
