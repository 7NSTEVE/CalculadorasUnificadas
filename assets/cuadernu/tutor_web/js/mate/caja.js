/**
 * Maquetador matemático manuscrito (SVG, sin dependencias).
 *
 * Trabaja con cajas al estilo TeX: ancho, altura sobre la línea base y profundidad. Los
 * delimitadores, raíces, integrales y sumatorias se dibujan como trazos que crecen con su
 * contenido. El resultado es una lista ORDENADA de piezas (glifos y trazos) en el orden en
 * que una persona las escribiría, para poder animar el lapicero y anclar marcas a cada símbolo.
 */

import { analizarTex, textoPlano } from './tex.js';
import { letra } from '../nucleo/letra.js';
import { color as colorPaleta } from '../nucleo/paleta.js';

const NS = 'http://www.w3.org/2000/svg';
const RE_GRIEGO = /[Ͱ-Ͽ]/;
const RE_SIMBOLO = /[←-⋿′-‷ℐ-⅏■-➿]/;
const FUENTE_SIMBOLO = '"Segoe Print", "Cambria Math", "Segoe UI Symbol", serif';

let medidor = null;
const cacheMedidas = new Map();

function ctxMedidor() {
  if (!medidor) medidor = document.createElement('canvas').getContext('2d');
  return medidor;
}

export function vaciarCacheMedidas() {
  cacheMedidas.clear();
}

function fuenteDe(ch, est) {
  const l = letra();
  if (est.texto) return { fam: l.texto, peso: est.negrita ? 700 : 400, esc: 1, cursiva: false };
  if (RE_GRIEGO.test(ch)) return { fam: l.griego, peso: 700, esc: l.escalaGriego, cursiva: Boolean(l.cursivaMate) && !est.romano };
  if (RE_SIMBOLO.test(ch)) return { fam: FUENTE_SIMBOLO, peso: 500, esc: 0.78, cursiva: false };
  const esLetra = /[A-Za-zÀ-ÿ]/.test(ch);
  return {
    fam: l.mate,
    peso: est.negrita ? l.pesoNegrita : l.peso,
    esc: 1,
    cursiva: Boolean(l.cursivaMate) && esLetra && !est.romano,
  };
}

function medir(ch, est, px) {
  const f = fuenteDe(ch, est);
  const tam = px * f.esc * (est.texto ? letra().escalaTexto * 0.8 : letra().escalaMate);
  const font = `${f.cursiva ? 'italic ' : ''}${f.peso} ${tam.toFixed(2)}px ${f.fam}`;
  const clave = `${font}|${ch}`;
  let m = cacheMedidas.get(clave);
  if (!m) {
    const c = ctxMedidor();
    c.font = font;
    const r = c.measureText(ch);
    m = {
      font, tam, w: r.width,
      asc: r.actualBoundingBoxAscent || tam * 0.7,
      desc: r.actualBoundingBoxDescent || 0,
      izq: r.actualBoundingBoxLeft || 0,
      der: r.actualBoundingBoxRight || r.width,
    };
    if (ch === ' ') { m.asc = 0; m.desc = 0; m.w = tam * 0.26; }
    cacheMedidas.set(clave, m);
  }
  return m;
}

/** Altura del eje matemático (centro del signo +) sobre la línea base. */
function eje(px) {
  const m = medir('+', {}, px);
  return (m.asc - m.desc) / 2;
}

function azar(semilla) {
  let a = semilla >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

class Salida {
  constructor() {
    this.piezas = [];
    this.clases = [];
    this.colores = [];
    this.ocultas = 0;
  }

  meta() {
    return {
      clases: [...this.clases],
      color: this.colores.length ? this.colores[this.colores.length - 1] : '',
    };
  }

  glifo(ch, x, y, m, est) {
    if (this.ocultas || ch === ' ') {
      if (ch === ' ' && !this.ocultas) this.piezas.push({ tipo: 'hueco', ch: ' ', x, y: y - m.tam * 0.5, w: m.w, h: m.tam * 0.5, coste: 0, ...this.meta() });
      return;
    }
    this.piezas.push({
      tipo: 'glifo', ch, x, base: y, font: m.font, tam: m.tam, est,
      bx: x - Math.max(0, m.izq), by: y - m.asc, bw: Math.max(m.w, m.der) + Math.max(0, m.izq), bh: m.asc + m.desc,
      coste: 0.55 + Math.min(1.1, m.w / (m.tam * 0.62)), ...this.meta(),
    });
  }

  trazo(d, caja, grosor, largo, rol = '') {
    if (this.ocultas) return;
    this.piezas.push({
      tipo: 'trazo', d, grosor, largo: Math.max(2, largo), rol, ch: rol === 'fraccion' ? '/' : '',
      bx: caja.x, by: caja.y, bw: caja.w, bh: caja.h,
      coste: Math.max(0.35, Math.min(3.2, largo / 44)), ...this.meta(),
    });
  }
}

/* ---------- utilidades de cajas ---------- */

function cajaVacia(w = 0) {
  return { w, h: 0, d: 0, cls: 'espacio', pintar() {} };
}

function glifo(ch, est, px, cls) {
  const m = medir(ch, est, px);
  // El griego viene de otra fuente y trae margenes minimos: se le da aire para que no se pegue
  const lado = RE_GRIEGO.test(ch) ? px * 0.035 : 0;
  return {
    w: m.w + lado * 2, h: m.asc, d: m.desc, cls, ch, m, sale: Math.max(0, m.der - m.w),
    pintar(o, x, y) { o.glifo(ch, x + lado, y, m, est); },
  };
}

function cadena(s, est, px, cls) {
  const cajas = [...s].map((c) => glifo(c, est, px, cls));
  const w = cajas.reduce((a, b) => a + b.w, 0);
  return {
    w, h: Math.max(0, ...cajas.map((c) => c.h)), d: Math.max(0, ...cajas.map((c) => c.d)), cls,
    pintar(o, x, y) {
      let cx = x;
      for (const c of cajas) { c.pintar(o, cx, y); cx += c.w; }
    },
  };
}

function linea(x1, y1, x2, y2, r, ond = 0.5) {
  const mx = (x1 + x2) / 2 + (r() - 0.5) * ond;
  const my = (y1 + y2) / 2 + (r() - 0.5) * ond * 2;
  return `M ${x1.toFixed(2)} ${y1.toFixed(2)} Q ${mx.toFixed(2)} ${my.toFixed(2)} ${x2.toFixed(2)} ${y2.toFixed(2)}`;
}

/* ---------- espaciado entre átomos ---------- */

function separacion(a, b, nivel) {
  if (!a || !b || a === 'espacio' || b === 'espacio' || a === 'salto' || b === 'salto') return 0;
  const guion = nivel > 0;
  if (a === 'rel' || b === 'rel') return guion ? 0.1 : 0.26;
  if (a === 'bin' || b === 'bin') return guion ? 0.06 : 0.2;
  if (a === 'punt') return guion ? 0.08 : 0.22;
  if (a === 'fun' && (b === 'abre' || b === 'delim')) return 0.04;
  if (a === 'fun' || b === 'fun') return b === 'cierra' || a === 'abre' ? 0.02 : 0.16;
  if (a === 'grande') return 0.06;
  if (b === 'grande') return 0.12;
  if (a === 'inner' && b === 'inner') return 0.16;
  if (a === 'inner' || b === 'inner') return (a === 'abre' || b === 'cierra' || b === 'punt') ? 0.04 : 0.1;
  if (a === 'texto' && b === 'texto') return 0.12;
  if (a === 'delim' || b === 'delim') return (a === 'abre' || b === 'cierra' || b === 'punt') ? 0.02 : 0.055;
  return 0;
}

function claseDe(n) {
  switch (n.k) {
    case 'bin': return n.unario ? 'ord' : 'bin';
    case 'rel': case 'flechalarga': return 'rel';
    case 'abre': case 'cierra': case 'punt': return n.k;
    case 'fun': case 'funlim': return 'fun';
    case 'grande': return 'grande';
    case 'frac': case 'raiz': case 'matriz': case 'caja': case 'apilado': case 'llave': return 'inner';
    case 'delim': return 'delim';
    case 'delimfijo': return n.lado === 'abre' ? 'abre' : n.lado === 'cierra' ? 'cierra' : 'ord';
    case 'scripts': return claseDe(n.base);
    case 'clase': case 'color': case 'negrita': case 'romano': case 'tachado':
      return n.cuerpo && n.cuerpo.hijos && n.cuerpo.hijos.length === 1 ? claseDe(n.cuerpo.hijos[0]) : 'ord';
    case 'espacio': case 'fantasma': return 'espacio';
    case 'salto': return 'salto';
    case 'texto': return 'texto';
    default: return 'ord';
  }
}

/* ---------- delimitadores extensibles ---------- */

function delimTrazo(s, alto, px, r) {
  const g = Math.max(1.4, px * 0.058);
  const anchos = { '(': 0.24, ')': 0.24, '[': 0.2, ']': 0.2, '{': 0.3, '}': 0.3, '|': 0.1, '‖': 0.22, '⟨': 0.24, '⟩': 0.24, '⌊': 0.2, '⌋': 0.2, '⌈': 0.2, '⌉': 0.2, '/': 0.4 };
  const w = px * (anchos[s] || 0.24) + alto * 0.028;
  const pad = px * 0.05;
  return {
    w: w + pad * 2,
    pintar(o, x, yArriba) {
      const X = (u) => (x + pad + u * w).toFixed(2);
      const Y = (v) => (yArriba + v * alto).toFixed(2);
      const j = () => (r() - 0.5) * 0.03;
      let d = '';
      switch (s) {
        case '(': d = `M ${X(0.92)} ${Y(0)} C ${X(0.02 + j())} ${Y(0.2)} ${X(0.02 + j())} ${Y(0.8)} ${X(0.94)} ${Y(1)}`; break;
        case ')': d = `M ${X(0.08)} ${Y(0)} C ${X(0.98 + j())} ${Y(0.2)} ${X(0.98 + j())} ${Y(0.8)} ${X(0.06)} ${Y(1)}`; break;
        case '[': d = `M ${X(0.95)} ${Y(0.004)} L ${X(0.12)} ${Y(0)} L ${X(0.1)} ${Y(1)} L ${X(0.97)} ${Y(0.998)}`; break;
        case ']': d = `M ${X(0.05)} ${Y(0)} L ${X(0.88)} ${Y(0.003)} L ${X(0.9)} ${Y(1)} L ${X(0.04)} ${Y(0.997)}`; break;
        case '⌊': d = `M ${X(0.12)} ${Y(0)} L ${X(0.1)} ${Y(1)} L ${X(0.97)} ${Y(0.998)}`; break;
        case '⌋': d = `M ${X(0.88)} ${Y(0)} L ${X(0.9)} ${Y(1)} L ${X(0.04)} ${Y(0.997)}`; break;
        case '⌈': d = `M ${X(0.95)} ${Y(0.004)} L ${X(0.12)} ${Y(0)} L ${X(0.1)} ${Y(1)}`; break;
        case '⌉': d = `M ${X(0.05)} ${Y(0)} L ${X(0.88)} ${Y(0.003)} L ${X(0.9)} ${Y(1)}`; break;
        case '{': d = `M ${X(0.95)} ${Y(0)} C ${X(0.42)} ${Y(0.01)} ${X(0.5)} ${Y(0.12)} ${X(0.5)} ${Y(0.26)} C ${X(0.5)} ${Y(0.42)} ${X(0.42)} ${Y(0.49)} ${X(0.02)} ${Y(0.5)} C ${X(0.42)} ${Y(0.51)} ${X(0.5)} ${Y(0.58)} ${X(0.5)} ${Y(0.74)} C ${X(0.5)} ${Y(0.88)} ${X(0.42)} ${Y(0.99)} ${X(0.95)} ${Y(1)}`; break;
        case '}': d = `M ${X(0.05)} ${Y(0)} C ${X(0.58)} ${Y(0.01)} ${X(0.5)} ${Y(0.12)} ${X(0.5)} ${Y(0.26)} C ${X(0.5)} ${Y(0.42)} ${X(0.58)} ${Y(0.49)} ${X(0.98)} ${Y(0.5)} C ${X(0.58)} ${Y(0.51)} ${X(0.5)} ${Y(0.58)} ${X(0.5)} ${Y(0.74)} C ${X(0.5)} ${Y(0.88)} ${X(0.58)} ${Y(0.99)} ${X(0.05)} ${Y(1)}`; break;
        case '⟨': d = `M ${X(0.9)} ${Y(0)} L ${X(0.1)} ${Y(0.5)} L ${X(0.9)} ${Y(1)}`; break;
        case '⟩': d = `M ${X(0.1)} ${Y(0)} L ${X(0.9)} ${Y(0.5)} L ${X(0.1)} ${Y(1)}`; break;
        case '‖': d = `M ${X(0.2)} ${Y(0)} L ${X(0.2)} ${Y(1)} M ${X(0.8)} ${Y(0)} L ${X(0.8)} ${Y(1)}`; break;
        case '/': d = `M ${X(0.95)} ${Y(0)} L ${X(0.05)} ${Y(1)}`; break;
        default: d = `M ${X(0.5)} ${Y(0)} L ${X(0.5)} ${Y(1)}`; break;
      }
      o.trazo(d, { x: x + pad, y: yArriba, w, h: alto }, g, alto * (s === '‖' ? 2 : 1.12), 'delim');
    },
  };
}

/* ---------- nodo -> caja ---------- */

function cajaFila(hijos, c) {
  const cajas = [];
  let previa = null;
  let w = 0;
  let h = 0;
  let d = 0;
  for (const n of hijos) {
    if (n.k === 'salto') continue;
    const b = caja(n, c);
    if (!b) continue;
    const cls = claseDe(n);
    const sep = separacion(previa, cls, c.nivel) * c.px;
    cajas.push({ b, sep });
    w += sep + b.w;
    h = Math.max(h, b.h);
    d = Math.max(d, b.d);
    if (cls !== 'espacio') previa = cls;
  }
  return {
    w, h, d, cls: 'ord', n: cajas.length,
    pintar(o, x, y) {
      let cx = x;
      for (const { b, sep } of cajas) { cx += sep; b.pintar(o, cx, y); cx += b.w; }
    },
  };
}

function hijoNivel(c, n = 1) {
  const nivel = Math.min(2, c.nivel + n);
  const base = c.px0 || c.px;
  const px = nivel === 0 ? base : nivel === 1 ? base * 0.68 : base * 0.54;
  return { ...c, nivel, px, px0: base };
}

function cajaFrac(n, c) {
  const enGuion = c.nivel > 0;
  const prof = (c.profFrac || 0) + 1;
  const factor = n.chica ? 0.74 : enGuion ? 0.92 : prof >= 2 ? 0.8 : 0.94;
  const ci = { ...c, px: c.px * factor, px0: (c.px0 || c.px) * factor, profFrac: prof };
  const a = cajaFila(n.num.hijos, ci);
  const b = cajaFila(n.den.hijos, ci);
  const ax = eje(c.px);
  const g = Math.max(1.5, c.px * 0.055);
  const gap = c.px * 0.13 + g / 2;
  const w = Math.max(a.w, b.w) + c.px * 0.22;
  const subir = ax + gap + Math.max(a.d, c.px * 0.04);
  const bajar = -ax + gap + Math.max(b.h, c.px * 0.4);
  return {
    w: w + c.px * 0.06, h: subir + a.h, d: bajar + b.d, cls: 'inner',
    pintar(o, x, y) {
      const x0 = x + c.px * 0.03;
      a.pintar(o, x0 + (w - a.w) / 2, y - subir);
      if (!n.sinBarra) {
        o.trazo(linea(x0, y - ax, x0 + w, y - ax, c.r, 1), { x: x0, y: y - ax - g, w, h: g * 2 }, g, w, 'fraccion');
      }
      b.pintar(o, x0 + (w - b.w) / 2, y + bajar);
    },
  };
}

function cajaRaiz(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const g = Math.max(1.5, c.px * 0.055);
  const h = Math.max(b.h, c.px * 0.55) + c.px * 0.17;
  const d = Math.max(b.d, c.px * 0.06) + c.px * 0.03;
  const T = h + d;
  const ws = c.px * 0.44 + T * 0.045;
  const idx = n.indice && n.indice.hijos.length ? cajaFila(n.indice.hijos, hijoNivel(c, 2)) : null;
  const wi = idx ? Math.max(0, idx.w - ws * 0.42) : 0;
  return {
    w: wi + ws + b.w + c.px * 0.14, h: h + g, d, cls: 'inner',
    pintar(o, x, y) {
      if (idx) idx.pintar(o, x, y - T * 0.42);
      const x0 = x + wi;
      const yb = y + d;
      const yt = y - h;
      const xf = x0 + ws + b.w + c.px * 0.1;
      const P = (u, v) => `${(x0 + u * ws).toFixed(2)} ${(yt + v * T).toFixed(2)}`;
      const dd = `M ${P(0.02, 0.64)} L ${P(0.24, 0.54)} L ${P(0.52, 1)} L ${P(0.96, 0)} L ${xf.toFixed(2)} ${(yt + (c.r() - 0.5)).toFixed(2)}`;
      o.trazo(dd, { x: x0, y: yt, w: xf - x0, h: T }, g, T * 1.7 + (xf - x0), 'raiz');
      b.pintar(o, x0 + ws + c.px * 0.04, y);
    },
  };
}

function cajaGrande(n, c) {
  const g = Math.max(1.8, c.px * 0.075);
  const ax = eje(c.px);
  const sup = n.sup ? cajaFila(n.sup.hijos, hijoNivel(c)) : null;
  const sub = n.sub ? cajaFila(n.sub.hijos, hijoNivel(c)) : null;
  if (n.tipo === 'integral') {
    const alto = c.px * (c.nivel > 0 ? 1.45 : 2.05);
    const ancho = c.px * 0.5;
    const copias = n.n || 1;
    const wSim = ancho + (copias - 1) * c.px * 0.3;
    const hh = alto / 2 + ax;
    const dd = alto / 2 - ax;
    const xs = wSim * 0.96;
    const xi = wSim * 0.62;
    const wLim = Math.max(sup ? xs + sup.w : 0, sub ? xi + sub.w : 0);
    const supBase = hh - (sup ? Math.min(sup.h, c.px * 0.42) * 0.55 : 0);
    const subBase = dd - (sub ? c.px * 0.02 : 0);
    return {
      w: Math.max(wSim, wLim) + c.px * 0.1,
      h: Math.max(hh, sup ? supBase + sup.h : 0), d: Math.max(dd, sub ? subBase + sub.d : 0), cls: 'grande',
      pintar(o, x, y) {
        const yt = y - hh;
        for (let k = 0; k < copias; k += 1) {
          const x0 = x + k * c.px * 0.3;
          const P = (u, v) => `${(x0 + u * ancho).toFixed(2)} ${(yt + v * alto).toFixed(2)}`;
          o.trazo(
            `M ${P(0.98, 0.1)} C ${P(0.94, -0.012)} ${P(0.68, -0.02)} ${P(0.62, 0.13)} C ${P(0.55, 0.38)} ${P(0.48, 0.62)} ${P(0.42, 0.87)} C ${P(0.36, 1.02)} ${P(0.12, 1.012)} ${P(0.06, 0.9)}`,
            { x: x0, y: yt, w: ancho, h: alto }, g, alto * 1.25, 'integral',
          );
        }
        if (n.cerrada) {
          const cx = x + wSim * 0.52;
          const rr = c.px * 0.2;
          o.trazo(`M ${(cx + rr).toFixed(2)} ${(y - ax).toFixed(2)} a ${rr} ${rr} 0 1 0 ${(-rr * 2).toFixed(2)} 0 a ${rr} ${rr} 0 1 0 ${(rr * 2).toFixed(2)} 0`, { x: cx - rr, y: y - ax - rr, w: rr * 2, h: rr * 2 }, g * 0.7, rr * 6.3, 'integral');
        }
        if (sub) sub.pintar(o, x + xi, y + subBase);
        if (sup) sup.pintar(o, x + xs, y - supBase);
      },
    };
  }
  const alto = c.px * (c.nivel > 0 ? 0.95 : 1.28);
  const ancho = c.px * (n.tipo === 'suma' ? 0.92 : 0.95);
  const w = Math.max(ancho, sup ? sup.w : 0, sub ? sub.w : 0);
  const hh = alto / 2 + ax;
  const dd = alto / 2 - ax;
  const gap = c.px * 0.12;
  return {
    w: w + c.px * 0.08, h: hh + (sup ? gap + sup.d + sup.h : 0), d: dd + (sub ? gap + sub.h + sub.d : 0), cls: 'grande',
    pintar(o, x, y) {
      const x0 = x + (w - ancho) / 2;
      const yt = y - hh;
      const P = (u, v) => `${(x0 + u * ancho).toFixed(2)} ${(yt + v * alto).toFixed(2)}`;
      const dd2 = n.tipo === 'suma'
        ? `M ${P(0.94, 0.14)} L ${P(0.9, 0.02)} L ${P(0.08, 0.03)} L ${P(0.54, 0.5)} L ${P(0.06, 0.97)} L ${P(0.92, 0.96)} L ${P(0.97, 0.84)}`
        : `M ${P(0.03, 0.05)} L ${P(0.97, 0.03)} M ${P(0.24, 0.05)} L ${P(0.22, 0.98)} M ${P(0.76, 0.04)} L ${P(0.79, 0.98)}`;
      o.trazo(dd2, { x: x0, y: yt, w: ancho, h: alto }, g * 0.82, alto * 2 + ancho * 2, 'suma');
      if (sub) sub.pintar(o, x + (w - sub.w) / 2, y + dd + gap + sub.h);
      if (sup) sup.pintar(o, x + (w - sup.w) / 2, y - hh - gap - sup.d);
    },
  };
}

function cajaScripts(n, c) {
  const b = caja(n.base, c) || cajaVacia();
  const cs = hijoNivel(c);
  const sup = n.sup ? cajaFila(n.sup.hijos, cs) : null;
  const sub = n.sub ? cajaFila(n.sub.hijos, cs) : null;
  // ° pegado a la base: grados, no un exponente alto
  const grados = n.sup && n.sup.hijos.length === 1 && n.sup.hijos[0].s === '°';
  const supB = sup
    ? (grados ? 0 : Math.max(c.px * 0.33, b.h - sup.h * 0.48 - c.px * 0.02, sup.d + c.px * 0.22))
    : 0;
  let subB = sub ? Math.max(c.px * 0.16, b.d * 0.82 + c.px * 0.08) : 0;
  if (sup && sub && !grados) {
    const hueco = (supB - sup.d) + (subB - sub.h);
    const minimo = c.px * 0.1;
    if (hueco < minimo) subB += minimo - hueco;
  }
  const cursivo = b.sale ? b.sale * 0.8 + c.px * 0.015 : 0;
  const wSup = sup ? sup.w + cursivo : 0;
  const wSub = sub ? sub.w - (sup ? 0 : cursivo * 0.4) : 0;
  const boxGrados = grados ? glifo('°', {}, c.px, 'ord') : null;
  if (boxGrados) boxGrados.w += c.px * 0.05;
  return {
    w: b.w + (grados ? Math.max(boxGrados.w, wSub) : Math.max(wSup, wSub)) + c.px * 0.03,
    h: Math.max(b.h, sup && !grados ? supB + sup.h : 0, grados ? boxGrados.h : 0),
    d: Math.max(b.d, sub ? subB + sub.d : 0),
    cls: 'ord', ch: b.ch,
    pintar(o, x, y) {
      b.pintar(o, x, y);
      const xs = x + b.w + c.px * 0.012;
      if (sub) sub.pintar(o, xs, y + subB);
      if (grados) boxGrados.pintar(o, xs, y);
      else if (sup) sup.pintar(o, xs + cursivo, y - supB);
    },
  };
}

function cajaDelim(n, c) {
  const partes = n.partes.map((p) => cajaFila(p.hijos, c));
  const ax = eje(c.px);
  const h = Math.max(...partes.map((p) => p.h), c.px * 0.5);
  const d = Math.max(...partes.map((p) => p.d), c.px * 0.1);
  const medio = Math.max(h - ax, d + ax) + c.px * 0.05;
  const alto = medio * 2;
  const chico = alto < c.px * 1.12;
  const haz = (s, cls) => {
    if (!s) return null;
    if (chico && '()[]|'.includes(s)) {
      const g = glifo(s, {}, c.px, cls);
      return { w: g.w, pintar: (o, x, yBase) => g.pintar(o, x, yBase), h: g.h, d: g.d, glifo: true };
    }
    const t = delimTrazo(s, alto, c.px, c.r);
    return { w: t.w, pintar: (o, x, yBase) => t.pintar(o, x, yBase - ax - medio), h: ax + medio, d: medio - ax };
  };
  const izq = haz(n.izq, 'abre');
  const der = haz(n.der, 'cierra');
  const medios = n.medios.map((m) => haz(m, 'ord'));
  const pad = chico ? 0 : c.px * 0.03;
  const w = (izq ? izq.w : 0) + (der ? der.w : 0) + partes.reduce((a, p) => a + p.w, 0)
    + medios.reduce((a, m) => a + (m ? m.w : 0), 0) + pad * 2;
  return {
    w, cls: 'delim',
    h: Math.max(h, izq ? izq.h : 0, der ? der.h : 0), d: Math.max(d, izq ? izq.d : 0, der ? der.d : 0),
    pintar(o, x, y) {
      let cx = x;
      if (izq) { izq.pintar(o, cx, y); cx += izq.w; }
      cx += pad;
      partes.forEach((p, i) => {
        p.pintar(o, cx, y);
        cx += p.w;
        if (medios[i]) { medios[i].pintar(o, cx, y); cx += medios[i].w; }
      });
      cx += pad;
      if (der) der.pintar(o, cx, y);
    },
  };
}

function cajaMatriz(n, c) {
  const filas = n.filas.map((f) => f.map((cel) => cajaFila(cel.hijos, c)));
  const cols = Math.max(1, ...filas.map((f) => f.length));
  const alineado = /^(aligned|align|alignedat|split|equation)$/.test(n.entorno);
  const casos = n.entorno === 'cases';
  const anchos = Array.from({ length: cols }, (_, j) => Math.max(0, ...filas.map((f) => (f[j] ? f[j].w : 0))));
  const altos = filas.map((f) => ({
    h: Math.max(c.px * 0.62, ...f.map((b) => b.h)), d: Math.max(c.px * 0.18, ...f.map((b) => b.d)),
  }));
  const gapF = c.px * (alineado ? 0.34 : 0.24);
  const gapC = c.px * (alineado ? 0.04 : casos ? 0.7 : 0.5);
  const total = altos.reduce((a, f) => a + f.h + f.d, 0) + gapF * Math.max(0, filas.length - 1);
  const ax = eje(c.px);
  const w = anchos.reduce((a, b) => a + b, 0) + gapC * Math.max(0, cols - 1);
  const especif = (n.alinea || '').replace(/[^lcr]/g, '');
  return {
    w, h: total / 2 + ax, d: total / 2 - ax, cls: 'inner',
    pintar(o, x, y) {
      let yy = y - ax - total / 2;
      filas.forEach((f, i) => {
        const yb = yy + altos[i].h;
        let cx = x;
        for (let j = 0; j < cols; j += 1) {
          const b = f[j];
          if (b) {
            let dx = 0;
            const al = alineado ? (j % 2 === 0 ? 'r' : 'l') : casos ? 'l' : especif[j] || (n.entorno === 'gathered' ? 'c' : 'c');
            if (al === 'r') dx = anchos[j] - b.w;
            else if (al === 'c') dx = (anchos[j] - b.w) / 2;
            b.pintar(o, cx + dx + (alineado && j % 2 === 1 ? c.px * 0.12 : 0), yb);
          }
          cx += anchos[j] + gapC;
        }
        yy += altos[i].h + altos[i].d + gapF;
      });
    },
  };
}

function cajaAcento(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const g = Math.max(1.4, c.px * 0.05);
  const sube = c.px * (n.tipo === 'subraya' ? 0 : 0.2);
  return {
    w: b.w, h: b.h + sube, d: b.d + (n.tipo === 'subraya' ? c.px * 0.12 : 0), cls: 'ord',
    pintar(o, x, y) {
      b.pintar(o, x, y);
      const cx = x + b.w / 2;
      const yt = y - b.h - c.px * 0.1;
      const ancho = Math.max(c.px * 0.3, b.w * 0.96);
      const caja2 = { x, y: yt - c.px * 0.1, w: b.w, h: c.px * 0.14 };
      const punto = (px) => o.trazo(`M ${px.toFixed(2)} ${yt.toFixed(2)} l 0.01 0.01`, { x: px - g, y: yt - g, w: g * 2, h: g * 2 }, g * 2, 2, 'acento');
      switch (n.tipo) {
        case 'punto': punto(cx + c.px * 0.04); break;
        case 'dospuntos': punto(cx - c.px * 0.07); punto(cx + c.px * 0.13); break;
        case 'flecha': {
          const x0 = cx - ancho / 2;
          const x1 = cx + ancho / 2 + c.px * 0.04;
          o.trazo(`M ${x0.toFixed(2)} ${yt.toFixed(2)} L ${x1.toFixed(2)} ${yt.toFixed(2)} M ${(x1 - c.px * 0.12).toFixed(2)} ${(yt - c.px * 0.08).toFixed(2)} L ${x1.toFixed(2)} ${yt.toFixed(2)} L ${(x1 - c.px * 0.12).toFixed(2)} ${(yt + c.px * 0.08).toFixed(2)}`, caja2, g, ancho * 1.4, 'acento');
          break;
        }
        case 'gorro': o.trazo(`M ${(cx - c.px * 0.12).toFixed(2)} ${(yt + c.px * 0.06).toFixed(2)} L ${(cx + c.px * 0.03).toFixed(2)} ${(yt - c.px * 0.07).toFixed(2)} L ${(cx + c.px * 0.17).toFixed(2)} ${(yt + c.px * 0.06).toFixed(2)}`, caja2, g, c.px * 0.4, 'acento'); break;
        case 'virgulilla': o.trazo(`M ${(cx - c.px * 0.16).toFixed(2)} ${(yt + c.px * 0.03).toFixed(2)} q ${(c.px * 0.08).toFixed(2)} ${(-c.px * 0.12).toFixed(2)} ${(c.px * 0.16).toFixed(2)} 0 t ${(c.px * 0.16).toFixed(2)} 0`, caja2, g, c.px * 0.5, 'acento'); break;
        case 'agudo': o.trazo(`M ${(cx - c.px * 0.02).toFixed(2)} ${(yt + c.px * 0.04).toFixed(2)} L ${(cx + c.px * 0.12).toFixed(2)} ${(yt - c.px * 0.08).toFixed(2)}`, caja2, g, c.px * 0.2, 'acento'); break;
        case 'grave': o.trazo(`M ${(cx - c.px * 0.1).toFixed(2)} ${(yt - c.px * 0.08).toFixed(2)} L ${(cx + c.px * 0.04).toFixed(2)} ${(yt + c.px * 0.04).toFixed(2)}`, caja2, g, c.px * 0.2, 'acento'); break;
        case 'subraya': {
          const yb = y + b.d + c.px * 0.08;
          o.trazo(linea(x, yb, x + b.w, yb, c.r, 1), { x, y: yb - g, w: b.w, h: g * 2 }, g, b.w, 'acento');
          break;
        }
        default: o.trazo(linea(cx - ancho / 2 + c.px * 0.03, yt, cx + ancho / 2 + c.px * 0.03, yt, c.r, 0.8), caja2, g, ancho, 'acento'); break;
      }
    },
  };
}

function cajaCaja(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const pad = c.px * 0.22;
  const g = Math.max(1.6, c.px * 0.06);
  const h = b.h + pad;
  const d = b.d + pad;
  return {
    w: b.w + pad * 2, h: h + g, d: d + g, cls: 'inner',
    pintar(o, x, y) {
      b.pintar(o, x + pad, y);
      const x0 = x + g;
      const x1 = x + b.w + pad * 2 - g;
      const y0 = y - h;
      const y1 = y + d;
      const r = c.px * 0.16;
      o.trazo(
        `M ${(x0 + r).toFixed(1)} ${y0.toFixed(1)} L ${(x1 - r).toFixed(1)} ${(y0 + 0.6).toFixed(1)} Q ${x1.toFixed(1)} ${y0.toFixed(1)} ${x1.toFixed(1)} ${(y0 + r).toFixed(1)} L ${(x1 - 0.5).toFixed(1)} ${(y1 - r).toFixed(1)} Q ${x1.toFixed(1)} ${y1.toFixed(1)} ${(x1 - r).toFixed(1)} ${y1.toFixed(1)} L ${(x0 + r).toFixed(1)} ${(y1 + 0.5).toFixed(1)} Q ${x0.toFixed(1)} ${y1.toFixed(1)} ${x0.toFixed(1)} ${(y1 - r).toFixed(1)} L ${(x0 + 0.4).toFixed(1)} ${(y0 + r).toFixed(1)} Q ${x0.toFixed(1)} ${y0.toFixed(1)} ${(x0 + r * 1.4).toFixed(1)} ${(y0 - 0.4).toFixed(1)}`,
        { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }, g, 2 * (x1 - x0 + y1 - y0), 'caja',
      );
    },
  };
}

function cajaTachado(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const a = n.a ? cajaFila(n.a.hijos, hijoNivel(c)) : null;
  const g = Math.max(1.8, c.px * 0.065);
  return {
    w: b.w + (a ? a.w + c.px * 0.1 : 0), h: Math.max(b.h, a ? b.h * 0.8 + a.h : 0), d: b.d, cls: 'ord',
    pintar(o, x, y) {
      b.pintar(o, x, y);
      const pad = c.px * 0.06;
      const x0 = x - pad * 0.5;
      const x1 = x + b.w + pad;
      const y0 = y + b.d + pad * 0.6;
      const y1 = y - b.h - pad * 0.6;
      o.colores.push('rojo');
      const d1 = `M ${x0.toFixed(2)} ${y0.toFixed(2)} L ${x1.toFixed(2)} ${y1.toFixed(2)}`;
      const d2 = `M ${x0.toFixed(2)} ${y1.toFixed(2)} L ${x1.toFixed(2)} ${y0.toFixed(2)}`;
      const largo = Math.hypot(x1 - x0, y1 - y0);
      if (n.tipo !== 'bcancel') o.trazo(d1, { x: x0, y: y1, w: x1 - x0, h: y0 - y1 }, g, largo, 'tachado');
      if (n.tipo === 'bcancel' || n.tipo === 'xcancel') o.trazo(d2, { x: x0, y: y1, w: x1 - x0, h: y0 - y1 }, g, largo, 'tachado');
      o.colores.pop();
      if (a) a.pintar(o, x1 + c.px * 0.04, y - b.h * 0.8);
    },
  };
}

function cajaLlave(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const et = n.etiqueta ? cajaFila(n.etiqueta.hijos, hijoNivel(c)) : null;
  const g = Math.max(1.4, c.px * 0.05);
  const alto = c.px * 0.22;
  const gap = c.px * 0.08;
  const w = Math.max(b.w, et ? et.w : 0);
  const extra = gap + alto + (et ? gap + et.h + et.d : 0);
  return {
    w, h: b.h + (n.abajo ? 0 : extra), d: b.d + (n.abajo ? extra : 0), cls: 'inner',
    pintar(o, x, y) {
      const xb = x + (w - b.w) / 2;
      b.pintar(o, xb, y);
      const y0 = n.abajo ? y + b.d + gap : y - b.h - gap;
      const s = n.abajo ? 1 : -1;
      const m = xb + b.w / 2;
      const X = (v) => v.toFixed(2);
      o.trazo(`M ${X(xb)} ${X(y0)} C ${X(xb)} ${X(y0 + s * alto * 0.62)} ${X(xb + b.w * 0.16)} ${X(y0 + s * alto * 0.5)} ${X(xb + b.w * 0.3)} ${X(y0 + s * alto * 0.5)} C ${X(m - b.w * 0.06)} ${X(y0 + s * alto * 0.5)} ${X(m)} ${X(y0 + s * alto * 0.6)} ${X(m)} ${X(y0 + s * alto)} C ${X(m)} ${X(y0 + s * alto * 0.6)} ${X(m + b.w * 0.06)} ${X(y0 + s * alto * 0.5)} ${X(xb + b.w * 0.7)} ${X(y0 + s * alto * 0.5)} C ${X(xb + b.w * 0.84)} ${X(y0 + s * alto * 0.5)} ${X(xb + b.w)} ${X(y0 + s * alto * 0.62)} ${X(xb + b.w)} ${X(y0)}`,
        { x: xb, y: Math.min(y0, y0 + s * alto), w: b.w, h: alto }, g, b.w * 1.2, 'llave');
      if (et) et.pintar(o, x + (w - et.w) / 2, n.abajo ? y0 + alto + gap + et.h : y0 - alto - gap - et.d);
    },
  };
}

function cajaApilado(n, c) {
  const b = cajaFila(n.cuerpo.hijos, c);
  const ar = n.arriba ? cajaFila(n.arriba.hijos, hijoNivel(c)) : null;
  const ab = n.abajo ? cajaFila(n.abajo.hijos, hijoNivel(c)) : null;
  const gap = c.px * 0.1;
  const w = Math.max(b.w, ar ? ar.w : 0, ab ? ab.w : 0);
  return {
    w, h: b.h + (ar ? gap + ar.d + ar.h : 0), d: b.d + (ab ? gap + ab.h + ab.d : 0), cls: 'inner',
    pintar(o, x, y) {
      b.pintar(o, x + (w - b.w) / 2, y);
      if (ar) ar.pintar(o, x + (w - ar.w) / 2, y - b.h - gap - ar.d);
      if (ab) ab.pintar(o, x + (w - ab.w) / 2, y + b.d + gap + ab.h);
    },
  };
}

function cajaFlecha(n, c) {
  const ar = n.arriba ? cajaFila(n.arriba.hijos, hijoNivel(c)) : null;
  const ab = n.abajo ? cajaFila(n.abajo.hijos, hijoNivel(c)) : null;
  const ax = eje(c.px);
  const g = Math.max(1.5, c.px * 0.055);
  const w = Math.max(c.px * 0.9, (ar ? ar.w : 0) + c.px * 0.5, (ab ? ab.w : 0) + c.px * 0.5);
  return {
    w, h: ax + c.px * 0.12 + (ar ? ar.h + ar.d : 0), d: Math.max(0, -ax + c.px * 0.16 + (ab ? ab.h + ab.d : 0)), cls: 'rel',
    pintar(o, x, y) {
      const yy = y - ax;
      const x1 = x + w;
      o.trazo(`M ${x.toFixed(2)} ${yy.toFixed(2)} L ${x1.toFixed(2)} ${yy.toFixed(2)} M ${(x1 - c.px * 0.18).toFixed(2)} ${(yy - c.px * 0.12).toFixed(2)} L ${x1.toFixed(2)} ${yy.toFixed(2)} L ${(x1 - c.px * 0.18).toFixed(2)} ${(yy + c.px * 0.12).toFixed(2)}`, { x, y: yy - c.px * 0.12, w, h: c.px * 0.24 }, g, w * 1.3, 'flecha');
      if (ar) ar.pintar(o, x + (w - ar.w) / 2, yy - c.px * 0.1 - ar.d);
      if (ab) ab.pintar(o, x + (w - ab.w) / 2, yy + c.px * 0.12 + ab.h);
    },
  };
}

function caja(n, c) {
  switch (n.k) {
    case 'fila': case 'grupo': return cajaFila(n.hijos, c);
    case 'ord': return glifoO(n.s, { romano: c.romano, negrita: c.negrita }, c.px, 'ord');
    case 'bin': case 'rel': case 'punt': case 'abre': case 'cierra': case 'barra':
      return glifoO(n.s, { negrita: c.negrita }, c.px, claseDe(n));
    case 'espacio': return cajaVacia(n.em * c.px);
    case 'texto': return cadena(n.s, { texto: true, negrita: n.negrita || c.negrita }, c.px, 'texto');
    case 'fun': return n.cuerpo ? cajaFila(n.cuerpo.hijos, { ...c, romano: true }) : cadena(n.s, { romano: true }, c.px, 'fun');
    case 'funlim': {
      const b = cadena(n.s, { romano: true }, c.px, 'fun');
      return n.sub || n.sup ? cajaApilado({ cuerpo: { hijos: [{ k: 'fun', s: n.s }] }, abajo: n.sub, arriba: n.sup }, c) : b;
    }
    case 'romano': return cajaFila(n.cuerpo.hijos, { ...c, romano: true });
    case 'negrita': return cajaFila(n.cuerpo.hijos, { ...c, negrita: true });
    case 'frac': return cajaFrac(n, c);
    case 'raiz': return cajaRaiz(n, c);
    case 'grande': return cajaGrande(n, c);
    case 'scripts': return cajaScripts(n, c);
    case 'delim': return cajaDelim(n, c);
    case 'delimfijo': {
      if (!n.s) return cajaVacia();
      const ax = eje(c.px);
      const alto = c.px * n.tam;
      const t = delimTrazo(n.s, alto, c.px, c.r);
      return { w: t.w, h: alto / 2 + ax, d: alto / 2 - ax, cls: claseDe(n), pintar: (o, x, y) => t.pintar(o, x, y - ax - alto / 2) };
    }
    case 'matriz': return cajaMatriz(n, c);
    case 'acento': return cajaAcento(n, c);
    case 'caja': return cajaCaja(n, c);
    case 'tachado': return cajaTachado(n, c);
    case 'llave': return cajaLlave(n, c);
    case 'apilado': return cajaApilado(n, c);
    case 'flechalarga': return cajaFlecha(n, c);
    case 'clase': {
      const b = cajaFila(n.cuerpo.hijos, c);
      return { ...b, pintar(o, x, y) { o.clases.push(...n.cls.split(/\s+/).filter(Boolean)); const k = n.cls.split(/\s+/).filter(Boolean).length; b.pintar(o, x, y); o.clases.splice(o.clases.length - k, k); } };
    }
    case 'color': {
      const b = cajaFila(n.cuerpo.hijos, c);
      return { ...b, pintar(o, x, y) { o.colores.push(n.color); b.pintar(o, x, y); o.colores.pop(); } };
    }
    case 'fantasma': {
      const b = cajaFila(n.cuerpo.hijos, c);
      return { ...b, pintar(o, x, y) { o.ocultas += 1; b.pintar(o, x, y); o.ocultas -= 1; } };
    }
    case 'error': {
      const b = cadena(n.s, { texto: true, negrita: true }, c.px * 0.8, 'ord');
      return { ...b, pintar(o, x, y) { o.colores.push('rojo'); o.clases.push('mate-error'); b.pintar(o, x, y); o.clases.pop(); o.colores.pop(); } };
    }
    default: return cajaVacia();
  }
}

function glifoO(s, est, px, cls) {
  return [...s].length === 1 ? glifo(s, est, px, cls) : cadena(s, est, px, cls);
}

/* ---------- partir en renglones ---------- */

function partirEnLineas(hijos, c, maxAncho) {
  // Saltos explícitos primero
  const bloques = [[]];
  for (const n of hijos) {
    if (n.k === 'salto') bloques.push([]);
    else bloques[bloques.length - 1].push(n);
  }
  const lineas = [];
  for (const bl of bloques) {
    if (!bl.length) continue;
    const total = cajaFila(bl, c);
    if (total.w <= maxAncho) { lineas.push({ nodos: bl, sangria: 0 }); continue; }
    // Corta antes de relaciones; si no alcanza, antes de + y −
    const trozos = [];
    let actual = [];
    for (const n of bl) {
      const cls = claseDe(n);
      if ((cls === 'rel' || (cls === 'bin' && !n.unario)) && actual.length) { trozos.push({ cls: null, nodos: actual }); actual = [n]; trozos.prox = cls; } else actual.push(n);
    }
    if (actual.length) trozos.push({ cls: null, nodos: actual });
    let linea2 = [];
    let sangria = 0;
    for (const t of trozos) {
      const prueba = cajaFila([...linea2, ...t.nodos], c).w + sangria;
      if (linea2.length && prueba > maxAncho) {
        lineas.push({ nodos: linea2, sangria });
        linea2 = [...t.nodos];
        sangria = c.px * 0.9;
      } else linea2.push(...t.nodos);
    }
    if (linea2.length) lineas.push({ nodos: linea2, sangria });
  }
  return lineas;
}

/* ---------- API ---------- */

let contador = 0;

/**
 * @param {string} tex
 * @param {{px?: number, maxAncho?: number, color?: string, jitter?: boolean}} op
 */
export function maquetar(tex, { px = 34, maxAncho = 1e9, color = '', jitter = true, margen = 0.16 } = {}) {
  const { arbol, desconocidos } = analizarTex(tex);
  const r = azar([...String(tex)].reduce((a, ch) => (a * 31 + ch.codePointAt(0)) | 0, 7));
  const base = { px, px0: px, nivel: 0, r, romano: false, negrita: false, profFrac: 0 };
  let escala = 1;
  let lineas = partirEnLineas(arbol.hijos, base, maxAncho);
  let cajas = lineas.map((l) => ({ ...l, b: cajaFila(l.nodos, base) }));
  const masAncha = Math.max(1, ...cajas.map((l) => l.b.w + l.sangria));
  if (masAncha > maxAncho) {
    escala = Math.max(0.52, maxAncho / masAncha);
    const c2 = { ...base, px: px * escala, px0: px * escala };
    lineas = partirEnLineas(arbol.hijos, c2, maxAncho);
    cajas = lineas.map((l) => ({ ...l, b: cajaFila(l.nodos, c2) }));
  }
  const p = px * escala;
  const pad = Math.ceil(p * margen);
  const gap = p * 0.34;
  const o = new Salida();
  let y = pad;
  let W = 0;
  let baseUna = 0;
  cajas.forEach((l, i) => {
    const h = Math.max(l.b.h, p * 0.56);
    const d = Math.max(l.b.d, p * 0.2);
    const yb = y + h;
    if (i === 0) baseUna = yb;
    l.b.pintar(o, pad + l.sangria, yb);
    W = Math.max(W, pad * 2 + l.sangria + l.b.w);
    y = yb + d + (i < cajas.length - 1 ? gap : 0);
  });
  const H = y + pad;

  const svg = document.createElementNS(NS, 'svg');
  svg.setAttribute('class', 'mate');
  svg.setAttribute('width', W.toFixed(1));
  svg.setAttribute('height', H.toFixed(1));
  svg.setAttribute('viewBox', `0 0 ${W.toFixed(1)} ${H.toFixed(1)}`);
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', textoPlano(tex));
  svg.style.overflow = 'visible';
  if (color) svg.style.color = `var(--${color}, ${colorPaleta(color)})`;
  svg.dataset.baja = (H - baseUna).toFixed(1);

  const id = `m${(contador += 1)}`;
  const defs = document.createElementNS(NS, 'defs');
  const clip = document.createElementNS(NS, 'clipPath');
  clip.setAttribute('id', `${id}-c`);
  clip.setAttribute('clipPathUnits', 'userSpaceOnUse');
  const rect = document.createElementNS(NS, 'rect');
  clip.appendChild(rect);
  defs.appendChild(clip);
  svg.appendChild(defs);

  const g = document.createElementNS(NS, 'g');
  svg.appendChild(g);
  const manuscrita = jitter && letra() && !letra().cursivaMate;
  for (const pz of o.piezas) {
    if (pz.tipo === 'hueco') continue;
    let el;
    if (pz.tipo === 'glifo') {
      el = document.createElementNS(NS, 'text');
      const jy = manuscrita ? (r() - 0.5) * pz.tam * 0.028 : 0;
      const rot = manuscrita && /[A-Za-z0-9]/.test(pz.ch) ? (r() - 0.5) * 2.4 : 0;
      el.setAttribute('x', pz.x.toFixed(2));
      el.setAttribute('y', (pz.base + jy).toFixed(2));
      if (Math.abs(rot) > 0.2) el.setAttribute('transform', `rotate(${rot.toFixed(2)} ${(pz.x + pz.bw / 2).toFixed(1)} ${pz.base.toFixed(1)})`);
      el.style.font = pz.font;
      el.setAttribute('fill', 'currentColor');
      el.textContent = pz.ch;
    } else {
      el = document.createElementNS(NS, 'path');
      el.setAttribute('d', pz.d);
      el.setAttribute('fill', 'none');
      el.setAttribute('stroke', 'currentColor');
      el.setAttribute('stroke-width', pz.grosor.toFixed(2));
      el.setAttribute('stroke-linecap', 'round');
      el.setAttribute('stroke-linejoin', 'round');
    }
    if (pz.clases.length) el.setAttribute('class', pz.clases.join(' '));
    if (pz.color) el.style.color = `var(--${NOMBRE_VAR[pz.color] || pz.color}, ${colorPaleta(pz.color)})`;
    pz.el = el;
    g.appendChild(el);
  }
  const piezas = o.piezas;
  return { svg, g, rect, clipId: `${id}-c`, piezas, ancho: W, alto: H, escala, px: p, desconocidos, tex, baja: H - baseUna };
}

const NOMBRE_VAR = {
  red: 'rojo', blue: 'azul', green: 'verde', orange: 'naranja', purple: 'morado', teal: 'turquesa',
  pink: 'rosa', yellow: 'amarillo', gray: 'gris', black: 'tinta',
};

/** Texto lineal de las piezas, con índice para buscar términos. */
export function indiceTexto(piezas) {
  let s = '';
  const mapa = [];
  piezas.forEach((p, i) => {
    if (p.tipo === 'hueco') { if (!s.endsWith(' ')) { s += ' '; mapa.push(i); } return; }
    const ch = p.ch || '';
    for (const c of ch) { s += c; mapa.push(i); }
  });
  return { s, mapa };
}
