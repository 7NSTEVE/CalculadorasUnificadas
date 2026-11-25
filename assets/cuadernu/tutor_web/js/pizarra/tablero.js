/**
 * La pizarra: una hoja de papel que se desplaza en vertical.
 *
 * Se abandonó el lienzo infinito con arrastre: perderse en él es fácil y volver cuesta mucho. Aquí
 * todo fluye hacia abajo, el tamaño se cambia agrandando la letra (nunca hay que arrastrar a los
 * lados) y la vista acompaña sola a lo que se está escribiendo, salvo que la persona se mueva.
 */

import { bus, menosMovimiento } from '../nucleo/api.js';
import { alCambiarTema } from '../nucleo/paleta.js';
import { vaciarCacheMedidas } from '../mate/caja.js';
import { Marcas } from './marcas.js';

export class PasoVista {
  constructor(tablero, { n = 0, total = 0, titulo = '', clase = '' } = {}) {
    this.tablero = tablero;
    this.n = n;
    this.el = document.createElement('section');
    this.el.className = `paso ${clase}`.trim();
    this.el.dataset.n = String(n);
    this.cab = document.createElement('button');
    this.cab.type = 'button';
    this.cab.className = 'paso-cab';
    this.cab.setAttribute('aria-expanded', 'true');
    this.num = document.createElement('span');
    this.num.className = 'paso-num';
    this.num.textContent = n ? String(n) : '';
    if (!n) this.num.hidden = true;
    this.tit = document.createElement('span');
    this.tit.className = 'paso-titulo';
    this.tit.textContent = titulo;
    this.cab.append(this.num, this.tit);
    this.cuerpo = document.createElement('div');
    this.cuerpo.className = 'paso-cuerpo';
    this.el.append(this.cab, this.cuerpo);
    this.cab.addEventListener('click', () => this.alternar());
    this.lineas = [];
    this.total = total;
  }

  alternar() {
    if (this.el.classList.contains('plegado')) this.desplegar(true);
    else if (this.el.classList.contains('anterior')) this.plegar();
  }

  plegar() {
    this.el.classList.add('plegado');
    this.cab.setAttribute('aria-expanded', 'false');
    this.tablero.programarRecolocar();
  }

  desplegar(aMano = false) {
    this.el.classList.remove('plegado');
    this.cab.setAttribute('aria-expanded', 'true');
    if (aMano) this.el.dataset.abierto = 'si';
    this.tablero.programarRecolocar();
  }

  estado(e) {
    this.el.classList.toggle('actual', e === 'actual');
    this.el.classList.toggle('anterior', e === 'anterior');
    if (e === 'actual') this.desplegar();
  }
}

export class Tablero {
  constructor({ vista, hoja, columna, capaMarcas, capaLapiz, volver }) {
    this.vista = vista;
    this.hoja = hoja;
    this.columna = columna;
    this.capaLapiz = capaLapiz;
    this.btnVolver = volver;
    this.tintas = new Map();
    this.orden = [];
    this.pasos = [];
    this.siguiendo = true;
    this.zoom = 1;
    this.objetivo = null;
    this.animando = null;
    this._recolocar = 0;
    this.marcas = new Marcas(this, capaMarcas);

    this._ancho = 0;
    this._ro = new ResizeObserver(() => this._alRedimensionar());
    this._ro.observe(this.columna);
    this._roV = new ResizeObserver(() => { if (this.siguiendo) this.acompanar(); });
    this._roV.observe(this.vista);

    for (const ev of ['wheel', 'touchmove']) {
      this.vista.addEventListener(ev, () => this._soltarSeguimiento(), { passive: true });
    }
    this.vista.addEventListener('keydown', (e) => {
      if (['PageUp', 'PageDown', 'Home', 'End', 'ArrowUp', 'ArrowDown'].includes(e.key)) this._soltarSeguimiento();
    });
    this.vista.addEventListener('pointerdown', (e) => {
      // Arrastrar la barra de desplazamiento también cuenta como moverse a mano
      if (e.target === this.vista && e.offsetX > this.vista.clientWidth - 2) this._soltarSeguimiento();
    });
    if (this.btnVolver) this.btnVolver.addEventListener('click', () => this.volverAlActual());
    alCambiarTema(() => this.marcas.recolocar());
    bus.on('ajuste:letra', () => { vaciarCacheMedidas(); this.rehacerTodo(); });
  }

  /* ---------- medidas ---------- */

  get px() { return Math.round(this.pxBase() * this.zoom); }

  pxBase() {
    // La letra crece con la ventana: 4K no puede verse diminuto
    const w = this.vista.clientWidth || 900;
    const h = this.vista.clientHeight || 700;
    const b = Math.min(w / 26, h / 18);
    return Math.max(25, Math.min(72, b));
  }

  anchoUtil(el = this.columna) {
    const cs = getComputedStyle(el);
    return Math.max(160, el.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight) - 6);
  }

  fijarZoom(z) {
    this.zoom = Math.max(0.6, Math.min(2.2, z));
    this.rehacerTodo(true);
    bus.emit('zoom', this.zoom);
  }

  _aplicarEscala() {
    const px = this.px;
    this.hoja.style.setProperty('--px', `${px}px`);
    this.hoja.style.setProperty('--px-texto', `${(px * 0.66).toFixed(1)}px`);
  }

  rehacerTodo(forzar = true) {
    this._aplicarEscala();
    for (const t of this.orden) {
      if (forzar && 'px' in t && t.px !== undefined && t.constructor.name === 'TintaMate') t.px = this.px * (t.factor || 1);
      if (t.rehacer) t.rehacer();
    }
    for (const t of this.orden) this._ajustar(t);
    this.programarRecolocar();
  }

  _ajustar(t) {
    const cont = t.el.closest('.paso-cuerpo') || this.columna;
    if (t.ajustar && t.el.isConnected) t.ajustar(this.anchoUtil(cont));
  }

  _alRedimensionar() {
    const w = this.columna.clientWidth;
    const px = this.px;
    if (Math.abs(w - this._ancho) < 2 && this._px === px) { this.programarRecolocar(); return; }
    const cambioPx = this._px !== px;
    this._ancho = w;
    this._px = px;
    this._aplicarEscala();
    for (const t of this.orden) {
      if (cambioPx && t.tex !== undefined) { t.px = px * (t.factor || 1); t.rehacer(); }
      this._ajustar(t);
    }
    this.programarRecolocar();
  }

  programarRecolocar() {
    if (this._recolocar) return;
    this._recolocar = requestAnimationFrame(() => {
      this._recolocar = 0;
      this.marcas.recolocar();
      if (this.siguiendo && this.objetivo) this.acompanar();
    });
  }

  /* ---------- contenido ---------- */

  limpiar() {
    this.marcas.limpiar();
    this.tintas.clear();
    this.orden = [];
    this.pasos = [];
    this.columna.replaceChildren();
    this.objetivo = null;
    this.siguiendo = true;
    if (this.btnVolver) this.btnVolver.hidden = true;
    this.vista.scrollTop = 0;
    this._aplicarEscala();
  }

  nuevoPaso(datos = {}, { despuesDe = null } = {}) {
    const p = new PasoVista(this, datos);
    if (despuesDe) despuesDe.after(p.el);
    else this.columna.appendChild(p.el);
    this.pasos.push(p);
    return p;
  }

  quitarPaso(p) {
    const quitar = new Set();
    p.el.querySelectorAll('[data-tid]').forEach((e) => quitar.add(e.dataset.tid));
    if (p.el.dataset.tid) quitar.add(p.el.dataset.tid);
    this.marcas.quitarDe(p.el);
    p.el.remove();
    this.orden = this.orden.filter((t) => !quitar.has(t.tid));
    for (const id of quitar) this.tintas.delete(id);
    this.pasos = this.pasos.filter((x) => x !== p);
  }

  agregar(tinta, { paso = null, id = '', clase = '', antesDe = null } = {}) {
    const tid = id || `t${this.orden.length}_${Math.random().toString(36).slice(2, 7)}`;
    tinta.tid = tid;
    tinta.el.dataset.tid = tid;
    if (clase) tinta.el.classList.add(...clase.split(' '));
    const cont = paso ? paso.cuerpo : this.columna;
    if (antesDe) cont.insertBefore(tinta.el, antesDe);
    else cont.appendChild(tinta.el);
    this.tintas.set(tid, tinta);
    this.orden.push(tinta);
    if (paso) paso.lineas.push(tinta);
    this._ajustar(tinta);
    this.programarRecolocar();
    return tid;
  }

  tinta(id) { return this.tintas.get(id) || null; }

  /** Última tinta que contiene un término (de la más reciente a la más vieja). */
  buscarTinta(termino) {
    for (let i = this.orden.length - 1; i >= 0; i -= 1) {
      const t = this.orden[i];
      if (t.piezasDe && t.piezasDe(termino).length) return t;
    }
    return null;
  }

  /** Rectángulo de un elemento en coordenadas de la hoja. */
  rectEnHoja(el) {
    const a = el.getBoundingClientRect();
    const b = this.hoja.getBoundingClientRect();
    return { x: a.left - b.left, y: a.top - b.top, w: a.width, h: a.height };
  }

  /* ---------- cámara ---------- */

  _soltarSeguimiento() {
    if (!this.siguiendo) return;
    this.siguiendo = false;
    if (this.animando) cancelAnimationFrame(this.animando);
    this.animando = null;
    if (this.btnVolver && this.objetivo) this.btnVolver.hidden = false;
  }

  volverAlActual() {
    this.siguiendo = true;
    if (this.btnVolver) this.btnVolver.hidden = true;
    this.acompanar(true);
  }

  seguir(el) {
    this.objetivo = el;
    if (this.siguiendo) this.acompanar();
    else if (this.btnVolver) this.btnVolver.hidden = false;
  }

  acompanar(fuerte = false) {
    const el = this.objetivo;
    if (!el || !el.isConnected) return;
    const v = this.vista.getBoundingClientRect();
    if (v.height < 60) return;
    const r = el.getBoundingClientRect();
    const arriba = v.top + v.height * 0.1;
    const abajo = v.top + v.height * 0.74;
    let delta = 0;
    if (r.height > (abajo - arriba)) delta = r.top - arriba;
    else if (r.bottom > abajo) delta = r.bottom - abajo;
    else if (r.top < arriba) delta = r.top - arriba;
    if (fuerte && delta === 0) delta = r.top - (v.top + v.height * 0.3);
    if (Math.abs(delta) < 3) return;
    const destino = Math.max(0, Math.min(this.vista.scrollHeight - this.vista.clientHeight, this.vista.scrollTop + delta));
    this._irA(destino);
  }

  _irA(destino) {
    if (menosMovimiento()) { this.vista.scrollTop = destino; return; }
    this._destino = destino;
    if (this.animando) return;
    let antes = performance.now();
    const paso = (t) => {
      const dt = Math.min(64, t - antes);
      antes = t;
      const dif = this._destino - this.vista.scrollTop;
      if (Math.abs(dif) < 1.2) { this.vista.scrollTop = this._destino; this.animando = null; return; }
      this.vista.scrollTop += dif * (1 - Math.exp(-dt / 130));
      this.animando = requestAnimationFrame(paso);
    };
    this.animando = requestAnimationFrame(paso);
  }
}
