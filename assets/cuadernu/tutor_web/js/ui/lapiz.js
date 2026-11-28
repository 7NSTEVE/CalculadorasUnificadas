/**
 * Puntero, encerrar y lápiz.
 *
 *  - Puntero: tocar un símbolo abre un menú junto a él (nunca encima).
 *  - Encerrar: se rodea algo y el menú pregunta qué hacer con eso.
 *  - Lápiz: trazos en SVG, pegados a la hoja (se desplazan con ella).
 */

import { bus } from '../nucleo/api.js';

const NS = 'http://www.w3.org/2000/svg';
const $ = (id) => document.getElementById(id);

function rutaDe(pts) {
  if (!pts.length) return '';
  let d = `M ${pts[0][0].toFixed(1)} ${pts[0][1].toFixed(1)}`;
  for (let i = 1; i < pts.length; i += 1) {
    const [x0, y0] = pts[i - 1];
    const [x1, y1] = pts[i];
    d += ` Q ${x0.toFixed(1)} ${y0.toFixed(1)} ${((x0 + x1) / 2).toFixed(1)} ${((y0 + y1) / 2).toFixed(1)}`;
  }
  if (pts.length === 1) d += ` l 0.1 0.1`;
  return d;
}

export class Lapiz {
  constructor({ tablero, rep, asistente, acciones, svg }) {
    Object.assign(this, { tablero, rep, asistente, acciones, svg });
    this.herr = 'puntero';
    this.color = 'azul';
    this.trazos = [];
    this.sel = null;
    this.barra = document.querySelector('.herramientas');
    this.barra.addEventListener('click', (e) => {
      const b = e.target.closest('[data-herr]');
      if (b) this.usar(b.dataset.herr);
      const c = e.target.closest('[data-color]');
      if (c) {
        this.color = c.dataset.color;
        this.barra.querySelectorAll('[data-color]').forEach((x) => x.setAttribute('aria-pressed', String(x === c)));
      }
    });
    $('btn-borrar-lapiz').addEventListener('click', () => this.borrarTodo());
    svg.addEventListener('pointerdown', (e) => this._abajo(e));
    tablero.hoja.addEventListener('click', (e) => { if (this.herr === 'puntero') this._toque(e); });
    tablero.hoja.addEventListener('pointermove', (e) => { if (this.herr === 'puntero') this._sobre(e); });

    this.menu = $('menu-seleccion');
    this.menu.addEventListener('click', (e) => {
      const b = e.target.closest('[data-sel]');
      if (b) this._accion(b.dataset.sel);
    });
    $('sel-enviar').addEventListener('click', () => this._accion('preguntar'));
    $('sel-pregunta').addEventListener('keydown', (e) => { if (e.key === 'Enter') this._accion('preguntar'); });
    document.addEventListener('pointerdown', (e) => {
      if (!this.menu.hidden && !this.menu.contains(e.target) && !svg.contains(e.target)) this.cerrarMenu();
    }, true);
    tablero.vista.addEventListener('scroll', () => this.cerrarMenu(), { passive: true });
    bus.on('leccion:abierta', () => { this.borrarTodo(); this.cerrarMenu(); });
  }

  usar(h) {
    this.herr = h;
    this.barra.querySelectorAll('[data-herr]').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.herr === h)));
    this.svg.classList.toggle('activa', h !== 'puntero');
    $('colores-lapiz').hidden = h !== 'lapiz';
    $('btn-borrar-lapiz').hidden = h !== 'lapiz' && !this.trazos.length;
    this.cerrarMenu();
  }

  _pt(e) {
    const r = this.tablero.hoja.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  }

  _abajo(e) {
    if (e.button !== 0) return;
    e.preventDefault();
    const lazo = this.herr === 'encerrar';
    const p = document.createElementNS(NS, 'path');
    p.setAttribute('fill', lazo ? 'var(--azul)' : 'none');
    if (lazo) p.style.fillOpacity = '0.08';
    p.style.stroke = lazo ? 'var(--azul)' : `var(--${this.color})`;
    p.setAttribute('stroke-width', lazo ? '2' : String(Math.max(2.4, this.tablero.px * 0.085)));
    if (lazo) p.setAttribute('stroke-dasharray', '6 5');
    p.setAttribute('stroke-linecap', 'round');
    p.setAttribute('stroke-linejoin', 'round');
    this.svg.appendChild(p);
    const pts = [this._pt(e)];
    try { this.svg.setPointerCapture(e.pointerId); } catch { /* puntero sintético */ }
    const mover = (ev) => {
      const q = this._pt(ev);
      const u = pts[pts.length - 1];
      if (Math.hypot(q[0] - u[0], q[1] - u[1]) < 2) return;
      pts.push(q);
      p.setAttribute('d', rutaDe(pts) + (lazo ? ' Z' : ''));
    };
    const arriba = () => {
      this.svg.removeEventListener('pointermove', mover);
      this.svg.removeEventListener('pointerup', arriba);
      this.svg.removeEventListener('pointercancel', arriba);
      if (lazo) { this._lazo(pts, p); return; }
      p.setAttribute('d', rutaDe(pts));
      this.trazos.push({ p, pts, color: this.color });
      $('btn-borrar-lapiz').hidden = false;
    };
    this.svg.addEventListener('pointermove', mover);
    this.svg.addEventListener('pointerup', arriba);
    this.svg.addEventListener('pointercancel', arriba);
  }

  borrarTodo() {
    for (const t of this.trazos) t.p.remove();
    this.trazos = [];
    this.svg.replaceChildren();
    if (this.herr !== 'lapiz') $('btn-borrar-lapiz').hidden = true;
  }

  /* ------------------------------------------------------------ selección */

  _tintaEn(el) {
    const n = el && el.closest ? el.closest('[data-tid]') : null;
    if (!n || !this.tablero.hoja.contains(n)) return null;
    return this.tablero.tinta(n.dataset.tid);
  }

  _sobre(e) {
    const t = this._tintaEn(e.target);
    this.tablero.hoja.style.cursor = t && t.piezaEn && this._piezaEn(t, e) ? 'pointer' : '';
  }

  _piezaEn(t, e) {
    const svg = t.m.svg;
    const r = svg.getBoundingClientRect();
    if (!r.width) return null;
    const k = t.m.ancho / r.width;
    return t.piezaEn((e.clientX - r.left) * k, (e.clientY - r.top) * k);
  }

  /** Toque sobre un símbolo: se extiende a su término (letra con subíndice, número completo). */
  _toque(e) {
    if (e.target.closest('button, input, a, .quiz, .recorte')) return;
    const t = this._tintaEn(e.target);
    if (!t || !t.piezaEn) return;
    const z = this._piezaEn(t, e);
    if (!z) return;
    const term = this._termino(t, z);
    const r = t.rectDe(term.piezas);
    const svg = t.m.svg.getBoundingClientRect();
    const k = svg.width / Math.max(1, t.m.ancho);
    const caja = { left: svg.left + r.x * k, top: svg.top + r.y * k, right: svg.left + (r.x + r.w) * k, bottom: svg.top + (r.y + r.h) * k };
    this._marcaTemporal(caja);
    this.abrirMenu({ texto: term.texto, tid: t.tid, tex: t.tex, caja });
  }

  _termino(t, z) {
    const ps = t.m.piezas.filter((p) => p.tipo === 'glifo');
    const i = ps.indexOf(z);
    const esNum = (c) => /[0-9,.]/.test(c);
    const c = z.ch || z.texto || '';
    let a = i;
    let b = i;
    if (esNum(c)) {
      while (a > 0 && esNum(ps[a - 1].ch || '') && Math.abs(ps[a - 1].by + ps[a - 1].bh - z.by - z.bh) < 6) a -= 1;
      while (b + 1 < ps.length && esNum(ps[b + 1].ch || '') && Math.abs(ps[b + 1].by + ps[b + 1].bh - z.by - z.bh) < 6) b += 1;
    } else {
      // subíndice: glifos más pequeños pegados a la derecha y hacia abajo
      const chico = (p) => p && p.bh && p.tam && p.tam < (ps[a].tam || 1) * 0.92;
      while (b + 1 < ps.length && chico(ps[b + 1]) && ps[b + 1].bx - (ps[b].bx + ps[b].bw) < 6) b += 1;
      while (a > 0 && z.tam && ps[a - 1].tam && z.tam < ps[a - 1].tam * 0.92 && z.bx - (ps[a - 1].bx + ps[a - 1].bw) < 6) { a -= 1; break; }
    }
    const piezas = ps.slice(a, b + 1);
    return { piezas, texto: piezas.map((p) => p.ch || '').join('') };
  }

  _marcaTemporal(caja) {
    this._limpiaTemporal();
    const h = this.tablero.hoja.getBoundingClientRect();
    const p = document.createElementNS(NS, 'rect');
    p.setAttribute('x', String(caja.left - h.left - 5));
    p.setAttribute('y', String(caja.top - h.top - 4));
    p.setAttribute('width', String(caja.right - caja.left + 10));
    p.setAttribute('height', String(caja.bottom - caja.top + 8));
    p.setAttribute('rx', '8');
    p.style.fill = 'var(--azul)';
    p.style.fillOpacity = '0.12';
    p.style.stroke = 'var(--azul)';
    p.setAttribute('stroke-width', '2');
    this.svg.appendChild(p);
    this.temporal = p;
  }

  _limpiaTemporal() { if (this.temporal) { this.temporal.remove(); this.temporal = null; } }

  _lazo(pts, path) {
    if (pts.length < 8) { path.remove(); return; }
    const xs = pts.map((p) => p[0]);
    const ys = pts.map((p) => p[1]);
    const x0 = Math.min(...xs); const x1 = Math.max(...xs); const y0 = Math.min(...ys); const y1 = Math.max(...ys);
    const h = this.tablero.hoja.getBoundingClientRect();
    const dentro = (x, y) => {
      let d = false;
      for (let i = 0, j = pts.length - 1; i < pts.length; j = i, i += 1) {
        if ((pts[i][1] > y) !== (pts[j][1] > y) && x < ((pts[j][0] - pts[i][0]) * (y - pts[i][1])) / (pts[j][1] - pts[i][1]) + pts[i][0]) d = !d;
      }
      return d;
    };
    const partes = [];
    let tid = '';
    let tex = '';
    for (const t of this.tablero.orden) {
      if (!t.el.isConnected || t.el.offsetParent === null) continue;
      if (t.m && t.m.piezas) {
        const r = this.tablero.rectEnHoja(t.m.svg);
        const k = r.w / Math.max(1, t.m.ancho);
        const zs = t.m.piezas.filter((z) => z.tipo === 'glifo' && dentro(r.x + (z.bx + z.bw / 2) * k, r.y + (z.by + z.bh / 2) * k));
        if (zs.length) { partes.push(zs.map((z) => z.ch || '').join('')); tid = tid || t.tid; tex = tex ? `${tex} ; ${t.tex}` : t.tex; }
      } else if (t.unidades) {
        const us = t.unidades.filter((u) => { const r = this.tablero.rectEnHoja(u.el); return dentro(r.x + r.w / 2, r.y + r.h / 2); });
        if (us.length) { partes.push(us.map((u) => (u.m ? '' : u.el.textContent)).join(' ')); tid = tid || t.tid; tex = tex || t.texto; }
      }
    }
    let imagen = null;
    const img = this.tablero.hoja.querySelector('.recorte img, .pegado img');
    if (!partes.length && img) {
      const r = this.tablero.rectEnHoja(img);
      if (x0 < r.x + r.w && x1 > r.x && y0 < r.y + r.h && y1 > r.y) imagen = { img, r, x0, y0, x1, y1 };
    }
    const caja = { left: h.left + x0, right: h.left + x1, top: h.top + y0, bottom: h.top + y1 };
    this._limpiaTemporal();
    this.temporal = path;
    if (!partes.length && !imagen) { setTimeout(() => this._limpiaTemporal(), 900); return; }
    this.abrirMenu({ texto: partes.join('   ').trim(), tid, tex, caja, imagen });
  }

  /** Recorta de la imagen la zona encerrada. */
  _recorteDe(sel) {
    if (!sel.imagen) return '';
    const { img, r, x0, y0, x1, y1 } = sel.imagen;
    const k = img.naturalWidth / Math.max(1, r.w);
    const sx = Math.max(0, (x0 - r.x) * k);
    const sy = Math.max(0, (y0 - r.y) * k);
    const sw = Math.min(img.naturalWidth - sx, (x1 - x0) * k);
    const sh = Math.min(img.naturalHeight - sy, (y1 - y0) * k);
    if (sw < 8 || sh < 8) return '';
    const c = document.createElement('canvas');
    c.width = Math.round(sw); c.height = Math.round(sh);
    try {
      c.getContext('2d').drawImage(img, sx, sy, sw, sh, 0, 0, c.width, c.height);
      return c.toDataURL('image/png').split(',')[1];
    } catch { return ''; }
  }

  /* ------------------------------------------------------------ menú */

  abrirMenu(sel) {
    this.sel = sel;
    const m = this.menu;
    $('sel-titulo').textContent = sel.texto ? `Seleccionaste: ${sel.texto.slice(0, 70)}` : 'Parte de la imagen';
    $('sel-pregunta').value = '';
    m.hidden = false;
    const w = m.offsetWidth;
    const h = m.offsetHeight;
    const c = sel.caja;
    // Nunca encima de lo seleccionado: abajo, arriba, derecha o izquierda según quepa
    let x = Math.min(window.innerWidth - w - 10, Math.max(10, (c.left + c.right) / 2 - w / 2));
    let y = c.bottom + 14;
    if (y + h > window.innerHeight - 10) {
      if (c.top - h - 14 > 10) y = c.top - h - 14;
      else {
        y = Math.max(10, Math.min(window.innerHeight - h - 10, (c.top + c.bottom) / 2 - h / 2));
        x = c.right + 14 + w < window.innerWidth ? c.right + 14 : Math.max(10, c.left - w - 14);
      }
    }
    m.style.left = `${x}px`;
    m.style.top = `${y}px`;
  }

  cerrarMenu() {
    if (this.menu.hidden) return;
    this.menu.hidden = true;
    this.sel = null;
    this._limpiaTemporal();
  }

  _accion(que) {
    const sel = this.sel;
    if (!sel) return;
    const q = $('sel-pregunta').value.trim();
    this.cerrarMenu();
    const que2 = sel.texto || 'lo que encerré en la imagen';
    if (this.rep.lec) {
      if (que === 'origen') return this.asistente.origen(sel);
      if (que === 'despacio') return this.asistente.despacio(sel);
    }
    const textos = {
      origen: `¿De dónde salió "${que2}"? Explica paso a paso, sin saltarte nada.`,
      cero: `Explícame desde cero qué es "${que2}" y para qué sirve aquí.`,
      despacio: `Muéstrame todos los pasos intermedios de "${que2}", uno por renglón.`,
      preguntar: q || `Explícame "${que2}".`,
    };
    this.asistente.preguntar(textos[que], { seleccion: { texto: sel.texto, tex: sel.tex || '' }, imagen: this._recorteDe(sel) });
  }

}
