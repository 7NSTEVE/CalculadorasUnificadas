/**
 * Base de los visuales animados: lienzo nítido (DPR), bucle que se detiene solo cuando nadie
 * mira, colores del tema y herramientas de dibujo. Se dibuja en unidades virtuales (W×H) y se
 * escala al ancho disponible, así que se ve igual en un portátil y en 4K.
 */

import { menosMovimiento } from '../nucleo/api.js';
import { alfa, alCambiarTema, color, mezcla } from '../nucleo/paleta.js';
import { letra } from '../nucleo/letra.js';

export { alfa, color, mezcla };

/** Número corto con coma decimal. */
export function num(x, cifras = 3) {
  if (!Number.isFinite(x)) return '—';
  if (x === 0) return '0';
  const e = Math.floor(Math.log10(Math.abs(x)));
  if (e >= 5 || e <= -4) {
    const m = x / 10 ** e;
    const sup = String(e).replace('-', '⁻').replace(/[0-9]/g, (d) => '⁰¹²³⁴⁵⁶⁷⁸⁹'[+d]);
    return `${m.toFixed(Math.max(0, cifras - 1)).replace('.', ',')}×10${sup}`;
  }
  const d = Math.max(0, Math.min(6, cifras - 1 - e));
  return x.toFixed(d).replace('.', ',').replace('-', '−');
}

export function limitar(x, a = 0, b = 1) { return Math.max(a, Math.min(b, x)); }
export function suave(t) { const k = limitar(t); return k * k * (3 - 2 * k); }

/** Evaluador seguro de expresiones en x (sin eval). */
export function evaluador(expr) {
  const s = String(expr).replace(/\s+/g, '').replace(/\*\*/g, '^').replace(/,/g, '.');
  return (x) => {
    let i = 0;
    const ver = () => s[i];
    const suma = () => { let v = prod(); while (ver() === '+' || ver() === '-') { const o = s[i++]; const r = prod(); v = o === '+' ? v + r : v - r; } return v; };
    const prod = () => { let v = pot(); while (ver() === '*' || ver() === '/') { const o = s[i++]; const r = pot(); v = o === '*' ? v * r : v / r; } return v; };
    const pot = () => { const b = uni(); if (ver() === '^') { i += 1; return b ** pot(); } return b; };
    const uni = () => { if (ver() === '-') { i += 1; return -pot(); } if (ver() === '+') { i += 1; return pot(); } return prim(); };
    const prim = () => {
      if (ver() === '(') { i += 1; const v = suma(); if (ver() === ')') i += 1; return v; }
      const n = /^\d*\.?\d+(?:e[+-]?\d+)?/i.exec(s.slice(i));
      if (n) { i += n[0].length; return Number(n[0]); }
      const id = /^[A-Za-z]+/.exec(s.slice(i));
      if (!id) { i = s.length; return NaN; }
      i += id[0].length;
      if (ver() === '(') {
        i += 1; const a = suma(); if (ver() === ')') i += 1;
        const f = { sin: Math.sin, sen: Math.sin, cos: Math.cos, tan: Math.tan, exp: Math.exp, ln: Math.log, log: Math.log10, sqrt: Math.sqrt, abs: Math.abs }[id[0].toLowerCase()];
        return f ? f(a) : NaN;
      }
      if (id[0] === 'pi') return Math.PI;
      if (id[0] === 'e') return Math.E;
      return x;
    };
    const v = suma();
    return Number.isFinite(v) ? v : NaN;
  };
}

export class Visual {
  constructor({ W = 560, H = 420, clase = '' } = {}) {
    this.W = W;
    this.H = H;
    this.el = document.createElement('div');
    this.el.className = `visual ${clase}`.trim();
    this.marco = document.createElement('div');
    this.marco.className = 'visual-marco';
    this.canvas = document.createElement('canvas');
    this.canvas.setAttribute('role', 'img');
    this.marco.appendChild(this.canvas);
    this.el.appendChild(this.marco);
    this.mandos = document.createElement('div');
    this.mandos.className = 'visual-mandos';
    this.mandos.hidden = true;
    this.el.appendChild(this.mandos);
    this.ctx = this.canvas.getContext('2d');
    this.t = 0;
    this.tFoco = 0;
    this.foco = null;
    this.pausado = false;
    this.visible = true;
    this.sucio = true;
    this.vivo = true;
    this._quitaTema = alCambiarTema(() => { this.sucio = true; this._pedir(); });
    this._ro = new ResizeObserver(() => this._medir());
    this._ro.observe(this.marco);
    this._io = new IntersectionObserver((es) => { this.visible = es.some((e) => e.isIntersecting); this._pedir(); });
    this._io.observe(this.canvas);
    this._raf = 0;
    this._antes = performance.now();
    this._bucle = this._bucle.bind(this);
    requestAnimationFrame(() => { this._medir(); this._pedir(); });
  }

  describe(txt) { this.canvas.setAttribute('aria-label', txt); }

  _medir() {
    const w = this.marco.clientWidth;
    if (w < 20) return;
    const dpr = Math.min(3, window.devicePixelRatio || 1);
    const topeH = this.marco.clientHeight > 60 ? this.marco.clientHeight : 1e9;
    const k = Math.min(w / this.W, topeH / this.H);
    const pw = Math.round(this.W * k * dpr);
    const ph = Math.round(this.H * k * dpr);
    if (this.canvas.width !== pw || this.canvas.height !== ph) {
      this.canvas.width = pw;
      this.canvas.height = ph;
      this.canvas.style.width = `${(this.W * k).toFixed(1)}px`;
      this.canvas.style.height = `${(this.H * k).toFixed(1)}px`;
    }
    this.k = k * dpr;
    this.sucio = true;
    this._pedir();
  }

  _pedir() {
    if (!this.vivo || this._raf) return;
    this._antes = performance.now();
    this._raf = requestAnimationFrame(this._bucle);
  }

  _bucle(ahora) {
    this._raf = 0;
    if (!this.vivo) return;
    const dt = Math.min(0.08, (ahora - this._antes) / 1000);
    this._antes = ahora;
    const anima = this.animado() && !this.pausado && this.visible && !document.hidden;
    if (anima) { this.t += dt; this.tFoco += dt; }
    if ((anima || this.sucio) && this.k) {
      const c = this.ctx;
      c.setTransform(1, 0, 0, 1, 0, 0);
      c.clearRect(0, 0, this.canvas.width, this.canvas.height);
      c.setTransform(this.k, 0, 0, this.k, 0, 0);
      c.lineCap = 'round';
      c.lineJoin = 'round';
      this.sucio = false;
      try { this.dibujar(c); } catch (err) { console.error('visual', err); }
    }
    if (anima) this._raf = requestAnimationFrame(this._bucle);
  }

  /** ¿Hay algo moviéndose? Con "menos movimiento" se muestra el estado final, quieto. */
  animado() { return !menosMovimiento(); }

  /** Tiempo del foco, saturado si no se permite movimiento. */
  fase(dur = 1.2) { return menosMovimiento() ? 1 : limitar(this.tFoco / dur); }

  cambiar(cambio = {}) {
    if ('foco' in cambio && cambio.foco !== this.foco) { this.foco = cambio.foco; this.tFoco = 0; }
    Object.assign(this, Object.fromEntries(Object.entries(cambio).filter(([k]) => k !== 'foco')));
    this.sucio = true;
    this._pedir();
  }

  pausar() { this.pausado = true; }
  seguir() { this.pausado = false; this._pedir(); }

  destruir() {
    this.vivo = false;
    if (this._raf) cancelAnimationFrame(this._raf);
    this._quitaTema();
    this._ro.disconnect();
    this._io.disconnect();
    this.el.remove();
  }

  /* ---------- mandos ---------- */

  deslizador({ etiqueta, min, max, paso, valor, formato = (v) => num(v), alMover }) {
    this.mandos.hidden = false;
    const fila = document.createElement('label');
    fila.className = 'mando';
    const t = document.createElement('span');
    t.className = 'mando-txt';
    const r = document.createElement('input');
    r.type = 'range';
    r.min = String(min); r.max = String(max); r.step = String(paso); r.value = String(valor);
    r.setAttribute('aria-label', etiqueta);
    const v = document.createElement('output');
    v.className = 'mando-val';
    const pinta = () => { v.textContent = formato(Number(r.value)); };
    t.textContent = etiqueta;
    r.addEventListener('input', () => { pinta(); alMover(Number(r.value)); this.sucio = true; this._pedir(); });
    pinta();
    fila.append(t, r, v);
    this.mandos.appendChild(fila);
    return { poner: (x) => { r.value = String(x); pinta(); }, el: fila, input: r };
  }

  boton(texto, alPulsar) {
    this.mandos.hidden = false;
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'mando-btn';
    b.textContent = texto;
    b.addEventListener('click', () => { alPulsar(); this.sucio = true; this._pedir(); });
    this.mandos.appendChild(b);
    return b;
  }

  /* ---------- dibujo ---------- */

  fuente(tam = 15, { negrita = false, mate = false } = {}) {
    const l = letra();
    return `${negrita ? 700 : mate ? l.peso : 400} ${tam}px ${mate ? l.mate : l.texto}`;
  }

  /** Texto con subíndices y superíndices sencillos: "T_1", "r^2", "W_{neto}". */
  texto(s, x, y, { tam = 15, color: col = 'tinta', alin = 'left', negrita = false, mate = false, halo = false, angulo = 0 } = {}) {
    const c = this.ctx;
    const partes = [];
    const re = /([_^])(?:\{([^}]*)\}|(.))/g;
    let i = 0;
    let m;
    const src = String(s);
    while ((m = re.exec(src)) !== null) {
      if (m.index > i) partes.push({ t: src.slice(i, m.index), n: 0 });
      partes.push({ t: m[2] !== undefined ? m[2] : m[3], n: m[1] === '_' ? -1 : 1 });
      i = m.index + m[0].length;
    }
    if (i < src.length) partes.push({ t: src.slice(i), n: 0 });
    const f = (n) => this.fuente(n ? tam * 0.72 : tam, { negrita, mate });
    let w = 0;
    for (const p of partes) { c.font = f(p.n); p.w = c.measureText(p.t).width; w += p.w; }
    c.save();
    c.translate(x, y);
    if (angulo) c.rotate(angulo);
    let cx = alin === 'center' ? -w / 2 : alin === 'right' ? -w : 0;
    c.textAlign = 'left';
    c.textBaseline = 'alphabetic';
    for (const p of partes) {
      c.font = f(p.n);
      const dy = p.n === -1 ? tam * 0.22 : p.n === 1 ? -tam * 0.36 : 0;
      if (halo) { c.lineWidth = Math.max(3, tam * 0.28); c.strokeStyle = alfa('tarjeta', 0.92); c.strokeText(p.t, cx, dy); }
      c.fillStyle = color(col);
      c.fillText(p.t, cx, dy);
      cx += p.w;
    }
    c.restore();
    return w;
  }

  linea(x1, y1, x2, y2, { color: col = 'tinta', grosor = 2, guion = null, a = 1 } = {}) {
    const c = this.ctx;
    c.save();
    c.globalAlpha = a;
    c.strokeStyle = color(col);
    c.lineWidth = grosor;
    if (guion) c.setLineDash(guion);
    c.beginPath();
    c.moveTo(x1, y1);
    c.lineTo(x2, y2);
    c.stroke();
    c.restore();
  }

  flecha(x1, y1, x2, y2, { color: col = 'tinta', grosor = 2.4, punta = 9, ondas = 0, guion = null, a = 1 } = {}) {
    const c = this.ctx;
    c.save();
    c.globalAlpha = a;
    c.strokeStyle = color(col);
    c.fillStyle = color(col);
    c.lineWidth = grosor;
    if (guion) c.setLineDash(guion);
    const dx = x2 - x1; const dy = y2 - y1;
    const L = Math.hypot(dx, dy) || 1;
    const nx = -dy / L; const ny = dx / L;
    const ux = dx / L; const uy = dy / L;
    c.beginPath();
    if (ondas) {
      const N = Math.max(12, Math.round(L / 3));
      const util = L - punta * 0.9;
      for (let i = 0; i <= N; i += 1) {
        const t = i / N;
        const amp = Math.sin(t * Math.PI * 2 * ondas) * 4 * Math.sin(Math.min(1, t * 4) * Math.PI / 2);
        const px = x1 + ux * util * t + nx * amp;
        const py = y1 + uy * util * t + ny * amp;
        if (i) c.lineTo(px, py); else c.moveTo(px, py);
      }
    } else { c.moveTo(x1, y1); c.lineTo(x2 - ux * punta * 0.7, y2 - uy * punta * 0.7); }
    c.stroke();
    c.setLineDash([]);
    c.beginPath();
    c.moveTo(x2, y2);
    c.lineTo(x2 - ux * punta + nx * punta * 0.55, y2 - uy * punta + ny * punta * 0.55);
    c.lineTo(x2 - ux * punta - nx * punta * 0.55, y2 - uy * punta - ny * punta * 0.55);
    c.closePath();
    c.fill();
    c.restore();
  }

  circulo(x, y, r, { relleno = null, borde = 'tinta', grosor = 2, a = 1, guion = null } = {}) {
    const c = this.ctx;
    c.save();
    c.globalAlpha = a;
    c.beginPath();
    c.arc(x, y, Math.max(0.1, r), 0, Math.PI * 2);
    if (relleno) { c.fillStyle = relleno.startsWith('rgba') || relleno.startsWith('#') ? relleno : color(relleno); c.fill(); }
    if (borde) { c.strokeStyle = color(borde); c.lineWidth = grosor; if (guion) c.setLineDash(guion); c.stroke(); }
    c.restore();
  }

  caja(x, y, w, h, { r = 6, relleno = null, borde = null, grosor = 1.8, a = 1 } = {}) {
    const c = this.ctx;
    c.save();
    c.globalAlpha = a;
    c.beginPath();
    c.roundRect(x, y, w, h, r);
    if (relleno) { c.fillStyle = relleno.startsWith('rgba') || relleno.startsWith('#') ? relleno : color(relleno); c.fill(); }
    if (borde) { c.strokeStyle = color(borde); c.lineWidth = grosor; c.stroke(); }
    c.restore();
  }

  poligono(pts, { relleno = null, borde = null, grosor = 2, cerrar = true, a = 1, guion = null } = {}) {
    const c = this.ctx;
    c.save();
    c.globalAlpha = a;
    c.beginPath();
    pts.forEach(([x, y], i) => (i ? c.lineTo(x, y) : c.moveTo(x, y)));
    if (cerrar) c.closePath();
    if (relleno) { c.fillStyle = relleno.startsWith('rgba') || relleno.startsWith('#') ? relleno : color(relleno); c.fill(); }
    if (borde) { c.strokeStyle = color(borde); c.lineWidth = grosor; if (guion) c.setLineDash(guion); c.stroke(); }
    c.restore();
  }

  /** Barra con valor a la derecha. */
  barra(x, y, w, h, f, { color: col = 'azul', etiqueta = '', valor = '', tam = 13 } = {}) {
    this.caja(x, y, w, h, { r: h / 2, relleno: alfa(col, 0.14) });
    if (f > 0.004) this.caja(x, y, Math.max(h * 0.7, w * limitar(f)), h, { r: h / 2, relleno: col });
    if (etiqueta) this.texto(etiqueta, x, y - 5, { tam, color: 'tinta2' });
    if (valor) this.texto(valor, x + w, y - 5, { tam, color: col, alin: 'right', negrita: true });
  }
}

/** Ejes con escala lineal. */
export class Ejes {
  constructor(v, { x, y, w, h, x0 = 0, x1 = 1, y0 = 0, y1 = 1, etX = '', etY = '' }) {
    Object.assign(this, { v, x, y, w, h, x0, x1, y0, y1, etX, etY });
  }

  X(a) { return this.x + ((a - this.x0) / (this.x1 - this.x0 || 1)) * this.w; }
  Y(b) { return this.y + this.h - ((b - this.y0) / (this.y1 - this.y0 || 1)) * this.h; }

  dibujar({ tam = 15, corteX = false, corteY = false } = {}) {
    const { v, x, y, w, h } = this;
    v.flecha(x, y + h, x, y - 12, { color: 'tinta2', grosor: 1.8, punta: 8 });
    v.flecha(x, y + h, x + w + 12, y + h, { color: 'tinta2', grosor: 1.8, punta: 8 });
    v.texto(this.etY, x - 6, y - 16, { tam, color: 'tinta', alin: 'left', mate: true });
    v.texto(this.etX, x + w + 12, y + h + tam + 5, { tam, color: 'tinta', alin: 'right', mate: true });
    if (corteX) { v.linea(x + 8, y + h + 5, x + 13, y + h - 5, { grosor: 1.6, color: 'tinta2' }); v.linea(x + 13, y + h + 5, x + 18, y + h - 5, { grosor: 1.6, color: 'tinta2' }); }
    if (corteY) { v.linea(x - 5, y + h - 8, x + 5, y + h - 13, { grosor: 1.6, color: 'tinta2' }); v.linea(x - 5, y + h - 13, x + 5, y + h - 18, { grosor: 1.6, color: 'tinta2' }); }
  }

  curva(f, a, b, { N = 90, color: col = 'azul', grosor = 3, hasta = 1, guion = null, a: al = 1 } = {}) {
    const c = this.v.ctx;
    c.save();
    c.globalAlpha = al;
    c.strokeStyle = color(col);
    c.lineWidth = grosor;
    if (guion) c.setLineDash(guion);
    c.beginPath();
    const tope = Math.max(1, Math.round(N * hasta));
    for (let i = 0; i <= tope; i += 1) {
      const u = a + ((b - a) * i) / N;
      const yy = f(u);
      if (!Number.isFinite(yy)) continue;
      if (i) c.lineTo(this.X(u), this.Y(yy)); else c.moveTo(this.X(u), this.Y(yy));
    }
    c.stroke();
    c.restore();
  }

  area(f, a, b, { N = 80, relleno, base = this.y0 } = {}) {
    const pts = [[this.X(a), this.Y(base)]];
    for (let i = 0; i <= N; i += 1) {
      const u = a + ((b - a) * i) / N;
      pts.push([this.X(u), this.Y(f(u))]);
    }
    pts.push([this.X(b), this.Y(base)]);
    this.v.poligono(pts, { relleno });
  }
}
