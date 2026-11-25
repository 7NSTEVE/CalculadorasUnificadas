/**
 * Marcas del profesor (círculo, subrayado, resaltador, tachado, flecha) ANCLADAS al símbolo.
 *
 * Cada marca sabe a qué término apunta, no en qué píxel está: si se inserta un bloque, cambia el
 * tamaño de la letra o se parte el renglón, se vuelve a calcular. Antes un círculo sobre C_V
 * terminaba rodeando una "b" de otro paso.
 * Las notas van como renglón propio debajo de lo marcado: nunca tapan la fórmula.
 */

import { TintaTexto } from '../mate/tinta.js';
import { color as colorPaleta } from '../nucleo/paleta.js';

const NS = 'http://www.w3.org/2000/svg';

function semilla(s) {
  let h = 7;
  for (const ch of String(s)) h = (h * 31 + ch.codePointAt(0)) | 0;
  let a = h >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function trazoCirculo(r, rnd) {
  const rx = r.w / 2 + Math.max(7, r.w * 0.09);
  const ry = r.h / 2 + Math.max(6, r.h * 0.14);
  const cx = r.x + r.w / 2;
  const cy = r.y + r.h / 2;
  const ini = -Math.PI * 0.62 + rnd() * 0.2;
  const pts = [];
  const N = 30;
  for (let i = 0; i <= N; i += 1) {
    const a = ini + (i / N) * Math.PI * 2 * 1.06;
    const k = 1 + 0.028 * Math.sin(a * 3 + rnd() * 0.4) + (i / N) * 0.03;
    pts.push([cx + Math.cos(a) * rx * k, cy + Math.sin(a) * ry * k]);
  }
  let d = `M ${pts[0][0].toFixed(1)} ${pts[0][1].toFixed(1)}`;
  for (let i = 1; i < pts.length; i += 1) {
    const p = pts[i - 1];
    const q = pts[i];
    d += ` Q ${p[0].toFixed(1)} ${p[1].toFixed(1)} ${((p[0] + q[0]) / 2).toFixed(1)} ${((p[1] + q[1]) / 2).toFixed(1)}`;
  }
  return d;
}

function trazoSubraya(r, rnd, dy = 4) {
  const y = r.y + r.h + dy;
  return `M ${(r.x - 2).toFixed(1)} ${(y + rnd()).toFixed(1)} Q ${(r.x + r.w * 0.5).toFixed(1)} ${(y + 2 + rnd() * 1.5).toFixed(1)} ${(r.x + r.w + 3).toFixed(1)} ${(y - 0.8).toFixed(1)}`;
}

function trazoTacha(r) {
  return `M ${(r.x - 3).toFixed(1)} ${(r.y + r.h + 2).toFixed(1)} L ${(r.x + r.w + 3).toFixed(1)} ${(r.y - 2).toFixed(1)}`;
}

function trazoFlecha(a, b) {
  const vertical = Math.abs((b.y + b.h / 2) - (a.y + a.h / 2)) > Math.max(a.h, b.h) * 0.8;
  let x1; let y1; let x2; let y2; let c1x; let c1y; let c2x; let c2y;
  if (vertical) {
    x1 = a.x + a.w + 6; y1 = a.y + a.h / 2;
    x2 = b.x + b.w + 8; y2 = b.y + b.h / 2;
    const bow = Math.max(x1, x2) + Math.min(90, 30 + Math.abs(y2 - y1) * 0.22);
    c1x = bow; c1y = y1; c2x = bow; c2y = y2;
  } else {
    x1 = a.x + a.w / 2; y1 = a.y + a.h + 4;
    x2 = b.x + b.w / 2; y2 = b.y + b.h + 8;
    const dip = Math.max(y1, y2) + 26;
    c1x = x1; c1y = dip; c2x = x2; c2y = dip;
  }
  const ang = Math.atan2(y2 - c2y, x2 - c2x);
  const L = 11;
  const p = (da) => `${(x2 - L * Math.cos(ang + da)).toFixed(1)} ${(y2 - L * Math.sin(ang + da)).toFixed(1)}`;
  return {
    cuerpo: `M ${x1.toFixed(1)} ${y1.toFixed(1)} C ${c1x.toFixed(1)} ${c1y.toFixed(1)} ${c2x.toFixed(1)} ${c2y.toFixed(1)} ${x2.toFixed(1)} ${y2.toFixed(1)}`,
    punta: `M ${p(-0.5)} L ${x2.toFixed(1)} ${y2.toFixed(1)} L ${p(0.5)}`,
  };
}

class Marca {
  constructor(capa, spec) {
    this.capa = capa;
    this.spec = spec;
    this.p = 0;
    this.g = document.createElementNS(NS, 'g');
    this.g.setAttribute('class', `marca marca-${spec.tipo}`);
    this.trazos = [];
    this.coste = spec.tipo === 'circulo' ? 7 : spec.tipo === 'flecha' ? 8 : 5;
    this.nota = null;
    capa.svg.appendChild(this.g);
    if (spec.nota) this._ponerNota();
    this.recolocar();
    this.progreso(0);
  }

  get tablero() { return this.capa.tablero; }

  _tintaDe(en, que) {
    const t = en ? this.tablero.tinta(en) : null;
    if (t) return t;
    return que ? this.tablero.buscarTinta(que) : null;
  }

  _rects(en, que, todas = false) {
    if (this.spec.caja) return [this.spec.caja()].filter(Boolean);
    const t = this._tintaDe(en, que);
    if (!t || !t.el.isConnected || t.el.offsetParent === null) return [];
    const base = this.tablero.rectEnHoja(t.el);
    if (!que || !t.piezasDe) return [{ x: base.x, y: base.y, w: base.w, h: base.h }];
    const grupos = t.piezasDe(que, { todas });
    if (!grupos.length) return this.spec.tipo === 'subraya' || this.spec.tipo === 'resalta' ? [base] : [];
    // El SVG puede estar desplazado dentro del renglón
    const svg = t.m.svg;
    const rs = this.tablero.rectEnHoja(svg);
    const k = rs.w / Math.max(1, t.m.ancho);
    return grupos.map((g) => {
      const r = t.rectDe(g);
      return { x: rs.x + r.x * k, y: rs.y + r.y * k, w: r.w * k, h: r.h * k };
    });
  }

  _ponerNota() {
    const t = this._tintaDe(this.spec.en, this.spec.que);
    if (!t) return;
    const n = new TintaTexto(this.spec.nota, { clase: 'nota-marca', color: this.spec.color, px: this.tablero.px * 0.58 });
    n.progreso(0);
    t.el.after(n.el);
    this.nota = n;
    this.coste += n.coste * 0.7;
  }

  recolocar() {
    this.g.replaceChildren();
    this.trazos = [];
    const s = this.spec;
    const rnd = semilla(`${s.tipo}${s.que}${s.en}`);
    const col = `var(--${s.color}, ${colorPaleta(s.color)})`;
    const agrega = (d, ancho, extra = {}) => {
      const p = document.createElementNS(NS, 'path');
      p.setAttribute('d', d);
      p.setAttribute('fill', 'none');
      p.style.stroke = col;
      p.setAttribute('stroke-width', String(ancho));
      p.setAttribute('stroke-linecap', 'round');
      p.setAttribute('stroke-linejoin', 'round');
      for (const [k, v] of Object.entries(extra)) p.setAttribute(k, v);
      this.g.appendChild(p);
      this.trazos.push(p);
    };
    const g = Math.max(2.2, this.tablero.px * 0.075);
    if (s.tipo === 'flecha') {
      const a = this._rects(s.en, s.que)[0];
      const b = this._rects(s.a_en, s.a_que)[0];
      if (a && b) {
        const f = trazoFlecha(a, b);
        agrega(f.cuerpo, g);
        agrega(f.punta, g);
      }
    } else {
      for (const r of this._rects(s.en, s.que, s.todas)) {
        if (s.tipo === 'circulo') agrega(trazoCirculo(r, rnd), g);
        else if (s.tipo === 'tacha') agrega(trazoTacha(r), g * 1.05);
        else if (s.tipo === 'resalta') {
          const alto = Math.max(12, r.h * 0.95);
          const y = r.y + r.h / 2;
          agrega(`M ${(r.x + alto * 0.2).toFixed(1)} ${(y + 1).toFixed(1)} L ${(r.x + r.w - alto * 0.2).toFixed(1)} ${(y - 1).toFixed(1)}`, alto, { class: 'resaltador' });
        } else agrega(trazoSubraya(r, rnd, s.dy ?? 4), g);
      }
    }
    for (const t of this.trazos) {
      try { t.L = t.getTotalLength(); } catch { t.L = 100; }
    }
    this._pintar();
  }

  progreso(p) {
    this.p = Math.max(0, Math.min(1, p));
    this._pintar();
  }

  _pintar() {
    const corte = this.nota ? 0.62 : 1;
    const pt = Math.min(1, this.p / corte);
    const n = this.trazos.length;
    this.trazos.forEach((t, i) => {
      const f = Math.max(0, Math.min(1, pt * n - i));
      if (f <= 0) { t.style.visibility = 'hidden'; return; }
      t.style.visibility = 'visible';
      if (f >= 1) { t.style.strokeDasharray = ''; t.style.strokeDashoffset = ''; } else {
        t.style.strokeDasharray = `${t.L}`;
        t.style.strokeDashoffset = `${(t.L * (1 - f)).toFixed(1)}`;
      }
    });
    if (this.nota) this.nota.progreso(this.p <= corte ? 0 : (this.p - corte) / (1 - corte));
  }

  completar() { this.progreso(1); }

  quitar() {
    this.g.remove();
    if (this.nota) this.nota.el.remove();
    this.capa.lista.delete(this);
  }
}

export class Marcas {
  constructor(tablero, svg) {
    this.tablero = tablero;
    this.svg = svg;
    this.lista = new Set();
  }

  crear(spec) {
    const m = new Marca(this, { color: 'rojo', ...spec });
    this.lista.add(m);
    return m;
  }

  recolocar() {
    const h = this.tablero.hoja;
    this.svg.setAttribute('width', String(h.scrollWidth));
    this.svg.setAttribute('height', String(h.scrollHeight));
    for (const m of this.lista) m.recolocar();
  }

  quitarDe(el) {
    for (const m of [...this.lista]) {
      const t = m._tintaDe(m.spec.en, m.spec.que);
      if (t && el.contains(t.el)) m.quitar();
    }
  }

  limpiar() {
    for (const m of [...this.lista]) m.quitar();
    this.lista.clear();
    this.svg.replaceChildren();
  }
}
