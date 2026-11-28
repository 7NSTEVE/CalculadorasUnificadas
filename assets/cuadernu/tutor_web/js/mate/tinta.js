/**
 * Tinta progresiva: todo lo que el profesor escribe se controla con un único número
 * `progreso(p)` entre 0 y 1. No hay temporizadores internos: quien manda es el reloj de la
 * voz. Así pausar, retroceder, saltar o cambiar la velocidad funciona siempre y nada se
 * pierde ni se dibuja a destiempo.
 */

import { indiceTexto, maquetar } from './caja.js';
import { color as colorPaleta } from '../nucleo/paleta.js';

/** Milisegundos naturales por unidad de coste (≈ un carácter escrito a mano). */
export const MS_POR_UNIDAD = 78;

const VAR_COLOR = {
  red: 'rojo', blue: 'azul', green: 'verde', orange: 'naranja', purple: 'morado', teal: 'turquesa',
  pink: 'rosa', yellow: 'amarillo', ink_primary: 'tinta', ink: 'tinta', accent_blue: 'azul',
  accent_red: 'rojo', accent_green: 'verde', accent_orange: 'naranja', accent_indigo: 'morado',
  accent_purple: 'morado', accent_teal: 'turquesa',
};

export function varColor(nombre) {
  if (!nombre) return '';
  const k = VAR_COLOR[nombre] || nombre;
  return `var(--${k}, ${colorPaleta(k)})`;
}

class Tinta {
  constructor() {
    this.el = null;
    this.coste = 1;
    this.p = -1;
  }

  get duracionNatural() { return Math.max(260, this.coste * MS_POR_UNIDAD); }

  completar() { this.progreso(1); }

  /** Vuelve a calcular la disposición para un ancho disponible. */
  ajustar() {}

  /** Rectángulos (cliente) de un término. */
  buscar() { return []; }
}

/* ===================== Matemática ===================== */

export class TintaMate extends Tinta {
  constructor(tex, { px = 34, color = '', maxAncho = 900, clase = '' } = {}) {
    super();
    this.tex = tex;
    this.px = px;
    this.color = color;
    this.maxAncho = maxAncho;
    this.el = document.createElement('div');
    this.el.className = `linea-mate ${clase}`.trim();
    this.nib = document.createElement('span');
    this.nib.className = 'lapicero';
    this.nib.hidden = true;
    this._armar();
  }

  _armar() {
    const m = maquetar(this.tex, { px: this.px, maxAncho: this.maxAncho });
    this.m = m;
    this.coste = Math.max(1, m.piezas.reduce((a, b) => a + b.coste, 0));
    this.el.replaceChildren(m.svg, this.nib);
    this.el.style.color = varColor(this.color);
    this.escribibles = m.piezas.filter((z) => z.tipo !== 'hueco');
    this.acum = [];
    let s = 0;
    for (const z of this.escribibles) { this.acum.push(s); s += z.coste; }
    this.total = Math.max(0.001, s);
    this.hecho = -1;
    if (m.desconocidos.length) this.el.dataset.desconocidos = m.desconocidos.join(' ');
  }

  ajustar(maxAncho) {
    if (!maxAncho || Math.abs(maxAncho - this.maxAncho) < 2) return false;
    this.maxAncho = maxAncho;
    const p = this.p;
    this._armar();
    this.p = -1;
    this.progreso(p < 0 ? 1 : p);
    return true;
  }

  rehacer() {
    const p = this.p;
    this._armar();
    this.p = -1;
    this.progreso(p < 0 ? 1 : p);
  }

  progreso(p) {
    const v = Math.max(0, Math.min(1, p));
    if (v === this.p) return;
    this.p = v;
    const pos = v * this.total;
    const zs = this.escribibles;
    let actual = -1;
    for (let i = 0; i < zs.length; i += 1) {
      const z = zs[i];
      const ini = this.acum[i];
      const fin = ini + z.coste;
      if (v >= 1 || pos >= fin) this._mostrar(z, 1);
      else if (pos <= ini) this._mostrar(z, 0);
      else { this._mostrar(z, (pos - ini) / z.coste); actual = i; }
    }
    if (actual < 0 || v >= 1 || v <= 0) {
      this.nib.hidden = true;
      return;
    }
    const z = zs[actual];
    const f = (pos - this.acum[actual]) / z.coste;
    let nx = z.bx + z.bw * f;
    let ny = z.by + z.bh * (0.5 + Math.sin(f * Math.PI * 3) * 0.32);
    if (z.tipo === 'trazo') {
      try {
        const L = z.largoReal || (z.largoReal = z.el.getTotalLength());
        const pt = z.el.getPointAtLength(L * f);
        nx = pt.x; ny = pt.y;
      } catch { /* sin geometría aún */ }
    }
    this.nib.hidden = false;
    this.nib.style.transform = `translate(${nx.toFixed(1)}px, ${ny.toFixed(1)}px)`;
  }

  _mostrar(z, f) {
    if (z.f === f) return;
    z.f = f;
    const el = z.el;
    if (f <= 0) { el.style.visibility = 'hidden'; return; }
    el.style.visibility = 'visible';
    if (z.tipo === 'glifo') {
      if (f >= 1) {
        if (z.recortado) { el.removeAttribute('clip-path'); z.recortado = false; }
      } else {
        const r = this.m.rect;
        r.setAttribute('x', (z.bx - 3).toFixed(1));
        r.setAttribute('y', (z.by - 6).toFixed(1));
        r.setAttribute('width', Math.max(0.5, (z.bw + 6) * f).toFixed(1));
        r.setAttribute('height', (z.bh + 12).toFixed(1));
        if (!z.recortado) { el.setAttribute('clip-path', `url(#${this.m.clipId})`); z.recortado = true; }
      }
    } else if (f >= 1) {
      el.style.strokeDasharray = '';
      el.style.strokeDashoffset = '';
    } else {
      const L = z.largoReal || (z.largoReal = (() => { try { return el.getTotalLength() || z.largo; } catch { return z.largo; } })());
      el.style.strokeDasharray = `${L}`;
      el.style.strokeDashoffset = `${(L * (1 - f)).toFixed(2)}`;
    }
  }

  /** Piezas que forman un término. Acepta 'C_V', '\\Delta T', '0{,}256' o el texto visible. */
  piezasDe(termino, { todas = false, clase = '' } = {}) {
    if (clase) {
      const zs = this.m.piezas.filter((z) => z.clases.includes(clase) && z.tipo !== 'hueco');
      return zs.length ? [zs] : [];
    }
    const { s, mapa } = indiceTexto(this.m.piezas);
    const limpio = (t) => String(t || '')
      .replace(/\\(?:text|mathrm|mathbf|class\{[^}]*\})/g, '')
      .replace(/\\Delta/g, 'Δ').replace(/\\gamma/g, 'γ').replace(/\\alpha/g, 'α').replace(/\\beta/g, 'β')
      .replace(/\\sigma/g, 'σ').replace(/\\pi/g, 'π').replace(/\\eta/g, 'η').replace(/\\rho/g, 'ρ')
      .replace(/\\theta/g, 'θ').replace(/\\lambda/g, 'λ').replace(/\\mu/g, 'μ').replace(/\\varepsilon|\\epsilon/g, 'ε')
      .replace(/\\(?:cdot|times)/g, (x) => (x === '\\cdot' ? '·' : '×'))
      .replace(/-/g, '−').replace(/[{}\\_^\s]/g, '');
    const aguja = limpio(termino);
    if (!aguja) return [];
    const compacto = [];
    let pajar = '';
    for (let i = 0; i < s.length; i += 1) {
      if (s[i] === ' ') continue;
      pajar += s[i];
      compacto.push(mapa[i]);
    }
    const esAlnum = (c) => c !== undefined && /[A-Za-z0-9À-ÿͰ-Ͽ]/.test(c);
    const estrictos = [];
    const sueltos = [];
    let k = pajar.indexOf(aguja);
    while (k >= 0) {
      const antes = pajar[k - 1];
      const desp = pajar[k + aguja.length];
      const num = /^[0-9]/.test(aguja) ? /[0-9,.]/.test(antes || ' ') : false;
      const numFin = /[0-9]$/.test(aguja) ? /[0-9,]/.test(desp || ' ') && /[0-9]/.test(pajar[k + aguja.length + (desp === ',' ? 1 : 0)] || '') : false;
      const letras = aguja.length === 1 && esAlnum(aguja) && ((antes && /[a-z]/.test(antes) && /[a-z]/.test(aguja)) || (desp && /[a-z]/.test(desp) && /[a-z]/.test(aguja)));
      (num || numFin || letras ? sueltos : estrictos).push(k);
      k = pajar.indexOf(aguja, k + 1);
    }
    const pos = estrictos.length ? estrictos : sueltos;
    const grupos = pos.map((ini) => {
      const idx = new Set();
      for (let j = ini; j < ini + aguja.length; j += 1) idx.add(compacto[j]);
      return [...idx].map((i) => this.m.piezas[i]).filter((z) => z && z.tipo !== 'hueco');
    }).filter((g) => g.length);
    return todas ? grupos : grupos.slice(0, 1);
  }

  /** Rectángulos en coordenadas del propio bloque (`el`), independientes de zoom y scroll. */
  rectDe(piezas) {
    let x0 = Infinity; let y0 = Infinity; let x1 = -Infinity; let y1 = -Infinity;
    for (const z of piezas) {
      x0 = Math.min(x0, z.bx); y0 = Math.min(y0, z.by);
      x1 = Math.max(x1, z.bx + z.bw); y1 = Math.max(y1, z.by + z.bh);
    }
    return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
  }

  /** Término bajo un punto (coordenadas del bloque). */
  piezaEn(x, y, holgura = 5) {
    let mejor = null;
    for (const z of this.m.piezas) {
      if (z.tipo !== 'glifo') continue;
      if (x >= z.bx - holgura && x <= z.bx + z.bw + holgura && y >= z.by - holgura && y <= z.by + z.bh + holgura) {
        const d = Math.hypot(x - z.bx - z.bw / 2, y - z.by - z.bh / 2);
        if (!mejor || d < mejor.d) mejor = { z, d };
      }
    }
    return mejor ? mejor.z : null;
  }

  get texto() { return this.m.svg.getAttribute('aria-label') || ''; }
}

/* ===================== Texto con matemática en línea ===================== */

/** Parte "texto $x^2$ **negrita**" en trozos. */
export function partirTexto(s) {
  const out = [];
  const re = /\$([^$]+)\$|\\\(([\s\S]+?)\\\)|\*\*([^*]+)\*\*/g;
  let i = 0;
  let m;
  const src = String(s || '');
  while ((m = re.exec(src)) !== null) {
    if (m.index > i) out.push({ t: 'txt', v: src.slice(i, m.index) });
    if (m[1] !== undefined || m[2] !== undefined) out.push({ t: 'mate', v: m[1] !== undefined ? m[1] : m[2] });
    else {
      // La negrita puede llevar fórmulas adentro: "**$8\pi$**" se dibuja como matemática, no como texto
      for (const q of partirTexto(m[3])) out.push(q.t === 'txt' ? { t: 'neg', v: q.v } : q);
    }
    i = m.index + m[0].length;
  }
  if (i < src.length) out.push({ t: 'txt', v: src.slice(i) });
  return out;
}

export class TintaTexto extends Tinta {
  constructor(texto, { clase = '', color = '', px = 22, etiqueta = 'p' } = {}) {
    super();
    this.texto = String(texto || '');
    this.px = px;
    this.el = document.createElement(etiqueta);
    this.el.className = `linea-texto ${clase}`.trim();
    if (color) this.el.style.color = varColor(color);
    this.unidades = [];
    for (const tr of partirTexto(this.texto)) {
      if (tr.t === 'mate') {
        const m = maquetar(tr.v, { px: px * 1.16, maxAncho: 1e9, margen: 0.03 });
        const sp = document.createElement('span');
        sp.className = 'mate-enlinea';
        sp.style.verticalAlign = `${(-m.baja).toFixed(1)}px`;
        sp.appendChild(m.svg);
        this.el.appendChild(sp);
        this.unidades.push({ el: sp, coste: Math.max(1.2, m.piezas.reduce((a, b) => a + b.coste, 0) * 0.7), m, tex: tr.v });
        if (m.desconocidos.length) this.el.dataset.desconocidos = m.desconocidos.join(' ');
        continue;
      }
      const palabras = tr.v.split(/(\s+)/);
      for (const pal of palabras) {
        if (!pal) continue;
        if (/^\s+$/.test(pal)) { this.el.appendChild(document.createTextNode(' ')); continue; }
        const sp = document.createElement(tr.t === 'neg' ? 'strong' : 'span');
        sp.className = 'palabra';
        sp.textContent = pal;
        this.el.appendChild(sp);
        this.unidades.push({ el: sp, coste: Math.max(0.8, pal.length * 0.62) });
      }
    }
    this.coste = Math.max(1, this.unidades.reduce((a, b) => a + b.coste, 0));
    this.nib = document.createElement('span');
    this.nib.className = 'lapicero';
    this.nib.hidden = true;
    this.el.appendChild(this.nib);
  }

  progreso(p) {
    const v = Math.max(0, Math.min(1, p));
    if (v === this.p) return;
    this.p = v;
    const pos = v * this.coste;
    let acum = 0;
    let act = null;
    let fAct = 0;
    for (const u of this.unidades) {
      const fin = acum + u.coste;
      const f = v >= 1 || pos >= fin ? 1 : pos <= acum ? 0 : (pos - acum) / u.coste;
      if (u.f !== f) {
        u.f = f;
        if (f <= 0) { u.el.style.visibility = 'hidden'; u.el.style.clipPath = ''; }
        else {
          u.el.style.visibility = 'visible';
          u.el.style.clipPath = f >= 1 ? '' : `inset(-20% ${(100 - f * 100).toFixed(1)}% -20% -4px)`;
        }
      }
      if (f > 0 && f < 1) { act = u; fAct = f; }
      acum = fin;
    }
    if (!act) { this.nib.hidden = true; return; }
    this.nib.hidden = false;
    const x = act.el.offsetLeft + act.el.offsetWidth * fAct;
    const y = act.el.offsetTop + act.el.offsetHeight * (0.55 + Math.sin(fAct * 9) * 0.16);
    this.nib.style.transform = `translate(${x.toFixed(1)}px, ${y.toFixed(1)}px)`;
  }
}

/* ===================== Compuestos ===================== */

export class TintaSecuencia extends Tinta {
  constructor(hijos, el = null) {
    super();
    this.hijos = hijos.filter(Boolean);
    this.coste = Math.max(1, this.hijos.reduce((a, h) => a + h.coste, 0));
    this.el = el || document.createElement('div');
    if (!el) this.hijos.forEach((h) => this.el.appendChild(h.el));
  }

  progreso(p) {
    const v = Math.max(0, Math.min(1, p));
    if (v === this.p) return;
    this.p = v;
    const pos = v * this.coste;
    let a = 0;
    for (const h of this.hijos) {
      h.progreso(v >= 1 ? 1 : (pos - a) / h.coste);
      a += h.coste;
    }
  }

  ajustar(w) { this.hijos.forEach((h) => h.ajustar && h.ajustar(w)); }
  rehacer() { this.hijos.forEach((h) => h.rehacer && h.rehacer()); }
}

export function tintaLista(items, { px = 22, color = '', ordenada = false } = {}) {
  const ul = document.createElement(ordenada ? 'ol' : 'ul');
  ul.className = 'lista-mano';
  const hijos = items.map((it) => {
    const t = new TintaTexto(it, { etiqueta: 'li', px, color });
    ul.appendChild(t.el);
    return t;
  });
  return new TintaSecuencia(hijos, ul);
}

/** Celda: si empieza y termina en $ es matemática pura; si no, texto. */
function tintaCelda(td, v, px) {
  const s = String(v ?? '');
  const t = new TintaTexto(s, { etiqueta: 'span', px });
  td.appendChild(t.el);
  return t;
}

export function tintaTabla(cabecera, filas, { px = 20, resaltar = [], pie = '' } = {}) {
  const caja = document.createElement('div');
  caja.className = 'tabla-caja';
  const tabla = document.createElement('table');
  tabla.className = 'tabla-mano';
  const hijos = [];
  if (cabecera && cabecera.length) {
    const tr = document.createElement('tr');
    for (const h of cabecera) {
      const th = document.createElement('th');
      hijos.push(tintaCelda(th, h, px));
      tr.appendChild(th);
    }
    const thead = document.createElement('thead');
    thead.appendChild(tr);
    tabla.appendChild(thead);
  }
  const tb = document.createElement('tbody');
  filas.forEach((f, i) => {
    const tr = document.createElement('tr');
    f.forEach((v, j) => {
      const td = document.createElement('td');
      if (resaltar.some(([a, b]) => a === i && (b === j || b === undefined || b === null))) td.className = 'celda-clave';
      hijos.push(tintaCelda(td, v, px));
      tr.appendChild(td);
    });
    tb.appendChild(tr);
  });
  tabla.appendChild(tb);
  caja.appendChild(tabla);
  if (pie) {
    const t = new TintaTexto(pie, { clase: 'nota', px: px * 0.94 });
    caja.appendChild(t.el);
    hijos.push(t);
  }
  return new TintaSecuencia(hijos, caja);
}
