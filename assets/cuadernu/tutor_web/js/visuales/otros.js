/**
 * Gas y rapideces, distribución N(v), radiación, máquinas térmicas, Otto, entropía y función libre.
 */

import { alfa, color, Ejes, evaluador, limitar, mezcla, num, suave, Visual } from './base.js';

/* ============================================================== Gas con rapideces */
export class VisualGas extends Visual {
  constructor(spec) {
    super({ W: 560, H: 420 });
    this.s = spec;
    const g = spec.grupos || [];
    this.N = g.reduce((a, [, n]) => a + n, 0) || 40;
    this.part = [];
    const semilla = (i) => { const x = Math.sin(i * 12.9898 + 7.1) * 43758.5453; return x - Math.floor(x); };
    let i = 0;
    if (g.length) for (const [v, n] of g) for (let k = 0; k < n; k += 1) { i += 1; this.part.push({ x: semilla(i), y: semilla(i + 50), a: semilla(i + 90) * 6.28, v }); }
    else for (; i < 46; i += 1) { const r = Math.sqrt(-2 * Math.log(1 - 0.98 * semilla(i))) * 1.25; this.part.push({ x: semilla(i + 3), y: semilla(i + 55), a: semilla(i + 97) * 6.28, v: r }); }
    this.vmax = Math.max(...this.part.map((p) => p.v));
    this.Tf = 1;
    if (spec.modo === 'temperatura' || !g.length) {
      this.deslizador({ etiqueta: 'Temperatura', min: 0.4, max: 2.4, paso: 0.02, valor: 1, formato: (v) => `${num(300 * v, 3)} K`, alMover: (v) => { this.Tf = v; } });
    }
    this.describe('Gas: moléculas moviéndose y conteo de rapideces');
  }

  dibujar() {
    const x = 24; const y = 24; const w = 236; const h = 236;
    this.caja(x, y, w, h, { r: 4, borde: 'tinta', grosor: 3 });
    const k = 0.11 * Math.sqrt(this.Tf);
    this.part.forEach((p) => {
      const px = Math.abs((((p.x + Math.cos(p.a) * p.v * k * this.t) % 2) + 2) % 2 - 1);
      const py = Math.abs((((p.y + Math.sin(p.a) * p.v * k * this.t) % 2) + 2) % 2 - 1);
      const X = x + 8 + px * (w - 16); const Y = y + 8 + py * (h - 16);
      this.circulo(X, Y, 5, { relleno: mezcla('frio', 'caliente', p.v / this.vmax), borde: null });
    });
    if (this.foco === 'presion') {
      const pul = 0.5 + 0.5 * Math.sin(this.t * 5);
      for (let i = 0; i < 4; i += 1) { const yy = y + 34 + i * 56; this.flecha(x + w - 26, yy, x + w + 18, yy, { color: 'rojo', grosor: 3, a: 0.4 + 0.6 * pul }); }
      this.texto('los choques empujan la pared', x, y + h + 24, { tam: 16, color: 'rojo' });
    }
    const g = this.s.grupos;
    if (g && g.length) {
      const e = new Ejes(this, { x: 312, y: 46, w: 212, h: 214, x0: 0, x1: this.vmax + 1, y0: 0, y1: Math.max(...g.map((q) => q[1])) + 1, etX: 'v', etY: 'cuántas' });
      e.dibujar();
      const bw = (e.w / (this.vmax + 1)) * 0.68;
      g.forEach(([v, n]) => {
        const alt = (e.y + e.h - e.Y(n)) * suave(this.foco === 'barras' ? this.fase(1) : 1);
        this.caja(e.X(v) - bw / 2, e.y + e.h - alt, bw, alt, { r: 3, relleno: mezcla('frio', 'caliente', v / this.vmax), borde: null });
        this.texto(String(n), e.X(v), e.y + e.h - alt - 6, { tam: 15, alin: 'center', negrita: true });
        this.texto(v === 1 ? 'v' : `${v}v`, e.X(v), e.y + e.h + 18, { tam: 14, alin: 'center', color: 'tinta2' });
      });
      const raya = (val, et, col, dy) => { this.linea(e.X(val), e.y - 4, e.X(val), e.y + e.h, { color: col, grosor: 2.6, guion: [6, 4] }); this.texto(et, e.X(val) + 5, e.y + dy, { tam: 16, color: col, negrita: true, halo: true }); };
      if (['promedio', 'rms', 'presion', 'listo'].includes(this.foco)) raya(this.s.vm, `prom = ${num(this.s.vm, 3)}v`, 'verde', 4);
      if (['rms', 'presion', 'listo'].includes(this.foco)) raya(this.s.vrms, `rms = ${num(this.s.vrms, 3)}v`, 'rojo', 28);
      this.texto('azul = lentas · rojo = rápidas', 24, 320, { tam: 15, color: 'tinta3' });
    } else {
      const e = new Ejes(this, { x: 312, y: 46, w: 212, h: 214, x0: 0, x1: 4.4, y0: 0, y1: 0.72, etX: 'v', etY: 'N(v)' });
      e.dibujar();
      const f = (T) => (v) => Math.sqrt(2 / Math.PI) * (v * v / T ** 1.5) * Math.exp((-v * v) / (2 * T));
      e.area(f(this.Tf), 0, 4.4, { relleno: alfa('naranja', 0.18) });
      e.curva(f(this.Tf), 0, 4.4, { color: 'naranja' });
      this.texto('Más temperatura: más energía cinética media', 24, 320, { tam: 16, color: 'tinta2' });
      this.texto(this.s.pie || 'La curva se corre a la derecha y se aplana', 24, 344, { tam: 15, color: 'tinta3' });
    }
  }
}

/* ============================================================== Distribución por tramos */
export class VisualDistribucion extends Visual {
  constructor(spec) {
    super({ W: 560, H: 360 });
    this.s = spec;
    this.tr = spec.tramos.map((t) => ({ ...t, fn: evaluador(t.f) }));
    const x1 = Math.max(...this.tr.map((t) => t.b));
    let ym = 0;
    for (const t of this.tr) for (let i = 0; i <= 20; i += 1) ym = Math.max(ym, t.fn(t.a + ((t.b - t.a) * i) / 20));
    this.e = new Ejes(this, { x: 62, y: 40, w: 440, h: 238, x0: 0, x1: x1 * 1.06, y0: 0, y1: ym * 1.16, etX: spec.etX || 'v', etY: spec.etY || 'N(v)' });
    this.describe('Distribución de rapideces por tramos');
  }

  dibujar() {
    const { e } = this;
    e.dibujar({ tam: 18 });
    const cols = ['azul', 'verde', 'naranja', 'rojo', 'morado'];
    this.tr.forEach((t, i) => {
      const act = this.foco === i || this.foco === 'todo' || this.foco === 'media' || this.foco === null;
      const h = this.foco === i || this.foco === 'todo' ? suave(this.fase(1.1)) : 1;
      if (act) e.area(t.fn, t.a, t.a + (t.b - t.a) * h, { relleno: alfa(cols[i % 5], this.foco === i ? 0.36 : 0.2) });
      e.curva(t.fn, t.a, t.b, { color: cols[i % 5], grosor: this.foco === i ? 5 : 3.4 });
      const xm = (t.a + t.b) / 2;
      this.texto(t.n, e.X(xm), e.Y(Math.max(t.fn(xm), 0.12)) - 14, { tam: 15, alin: 'center', color: cols[i % 5], negrita: this.foco === i, halo: true });
      this.linea(e.X(t.b), e.y + e.h, e.X(t.b), e.y + e.h + 6, { grosor: 2 });
      this.texto(t.b === 1 ? 'v_0' : `${t.b}v_0`, e.X(t.b), e.y + e.h + 24, { tam: 17, alin: 'center', mate: true });
    });
    for (const [v, n] of this.s.marcasY || [[1, 'N_0'], [2, '2N_0']]) { this.linea(e.x - 5, e.Y(v), e.x + 5, e.Y(v), { grosor: 2 }); this.texto(n, e.x - 9, e.Y(v) + 6, { tam: 17, alin: 'right', mate: true }); }
    if (this.foco === 'pico' && this.s.vp !== undefined) {
      const y = Math.max(...this.tr.map((t) => (this.s.vp >= t.a && this.s.vp <= t.b ? t.fn(this.s.vp) : 0)));
      this.linea(e.X(this.s.vp), e.y + e.h, e.X(this.s.vp), e.Y(y), { color: 'rojo', grosor: 2.4, guion: [6, 4] });
      this.circulo(e.X(this.s.vp), e.Y(y), 8 + 2 * Math.sin(this.t * 4), { relleno: 'amarillo', borde: 'rojo' });
      this.texto('el pico', e.X(this.s.vp) + 12, e.Y(y) - 4, { tam: 18, color: 'rojo', negrita: true });
    }
    if (this.foco === 'media' && this.s.vm) { this.linea(e.X(this.s.vm), e.y, e.X(this.s.vm), e.y + e.h, { color: 'morado', grosor: 3, guion: [7, 4] }); this.texto(`media = ${num(this.s.vm, 3)}v_0`, e.X(this.s.vm) + 8, e.y + 10, { tam: 18, color: 'morado', negrita: true, halo: true }); }
    this.texto('El área bajo la curva es el número de moléculas', 280, 346, { tam: 15, alin: 'center', color: 'tinta3' });
  }
}

/* ============================================================== Radiación */
export class VisualRadiacion extends Visual {
  constructor(spec) {
    super({ W: 560, H: 380 });
    this.s = spec;
    this.Tk = spec.T;
    this.em = spec.e ?? 1;
    const Tmax = spec.Tmax || spec.T * 1.6;
    this.deslizador({ etiqueta: 'Temperatura', min: spec.T * 0.4, max: Tmax, paso: spec.T / 200, valor: spec.T, formato: (v) => `${num(v, 4)} K`, alMover: (v) => { this.Tk = v; } });
    this.deslizador({ etiqueta: 'Emisividad', min: 0.02, max: 1, paso: 0.01, valor: this.em, formato: (v) => num(v, 2), alMover: (v) => { this.em = v; } });
    this.describe('Cuerpo que radia energía; la potencia crece con la temperatura a la cuarta');
  }

  dibujar() {
    const rel = (this.Tk / this.s.T) ** 4 * (this.em / (this.s.e ?? 1));
    const cx = 170; const cy = 180;
    const brillo = limitar(0.25 + 0.4 * (this.Tk / this.s.T - 0.4));
    const c = this.ctx;
    const g = c.createRadialGradient(cx, cy, 20, cx, cy, 140);
    g.addColorStop(0, alfa('caliente', 0.5 * brillo + 0.1)); g.addColorStop(1, alfa('caliente', 0));
    c.fillStyle = g; c.fillRect(20, 20, 300, 320);
    const n = 12;
    for (let i = 0; i < n; i += 1) {
      const a = (i / n) * Math.PI * 2;
      const u = this.animado() ? (this.t * 0.5 + i * 0.37) % 1 : 0.5;
      const r0 = 66 + u * 60;
      this.flecha(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0, cx + Math.cos(a) * (r0 + 26), cy + Math.sin(a) * (r0 + 26), { color: 'caliente', grosor: 1.6 + limitar(rel, 0, 4), ondas: 1.4, punta: 7, a: Math.sin(u * Math.PI) * limitar(0.25 + rel * 0.5) });
    }
    if (this.s.objeto === 'filamento') {
      c.save(); c.strokeStyle = mezcla('naranja', 'amarillo', brillo); c.lineWidth = 7; c.beginPath();
      for (let i = 0; i <= 60; i += 1) { const x = cx - 52 + (104 * i) / 60; const y = cy + Math.sin(i * 0.8) * 18; if (i) c.lineTo(x, y); else c.moveTo(x, y); }
      c.stroke(); c.restore();
      this.circulo(cx, cy, 62, { borde: 'gris', grosor: 2 });
    } else {
      this.caja(cx - 48, cy - 48, 96, 96, { r: 10, relleno: mezcla('gris', 'caliente', brillo), borde: 'tinta' });
      if (this.s.objeto) this.texto(this.s.objeto, cx, cy + 6, { tam: 16, alin: 'center', negrita: true, halo: true });
    }
    if (this.s.Ts) {
      this.texto(`ambiente: ${num(this.s.Ts, 4)} K`, 40, 350, { tam: 16, color: 'frio' });
      for (let i = 0; i < 3; i += 1) { const u = (this.t * 0.4 + i / 3) % 1; this.flecha(296 - u * 60, 90 + i * 86, 266 - u * 60, 100 + i * 70, { color: 'frio', grosor: 2, ondas: 1.2, punta: 7, a: Math.sin(u * Math.PI) }); }
    }
    this.texto('H = Aeσ T^4', 344, 70, { tam: 26, mate: true, color: 'azul', negrita: true });
    this.texto(`T = ${num(this.Tk, 4)} K`, 344, 112, { tam: 18 });
    this.texto(`e = ${num(this.em, 2)}`, 344, 138, { tam: 18 });
    if (this.s.H) this.texto(`H = ${num(this.s.H * rel, 4)} W`, 344, 176, { tam: 20, color: 'caliente', negrita: true });
    if (this.s.A) this.texto(`A = ${num(this.s.A, 3)} ${this.s.uA || 'm²'}`, 344, 204, { tam: 17, color: 'tinta2' });
    this.barra(344, 246, 186, 16, rel / 4, { color: 'caliente', etiqueta: 'potencia', valor: `×${num(rel, 3)}` });
    this.texto('Si T se duplica, H se multiplica por 16', 344, 300, { tam: 15, color: 'tinta2' });
    this.texto('T siempre en kelvin', 344, 324, { tam: 15, color: 'rojo' });
  }
}

/* ============================================================== Máquinas */
export class VisualMaquina extends Visual {
  constructor(spec) {
    super({ W: 560, H: 420 });
    this.s = spec;
    this.describe('Flujo de energía de una máquina térmica o refrigerador');
  }

  una(x, d, { nombre = '', ancho = 230, refri = false, valores = true, TH, TC, u = 'J' } = {}) {
    const esc = 64 / Math.max(d.QH, 1e-9);
    const gH = Math.max(10, d.QH * esc); const gC = Math.max(7, d.QC * esc); const gW = Math.max(7, d.W * esc);
    const cx = x + ancho * 0.4;
    this.caja(x, 34, ancho * 0.8, 48, { r: 6, relleno: alfa('caliente', 0.25), borde: 'caliente' });
    this.texto(TH ? `Caliente · ${num(TH, 4)} K` : 'Foco caliente', cx, 64, { tam: 17, alin: 'center', negrita: true, color: 'caliente' });
    this.caja(x, 330, ancho * 0.8, 48, { r: 6, relleno: alfa('frio', 0.22), borde: 'frio' });
    this.texto(TC ? `Frío · ${num(TC, 4)} K` : 'Foco frío', cx, 360, { tam: 17, alin: 'center', negrita: true, color: 'frio' });
    const R = 44;
    const yc = 206;
    const tubo = (x0, y0, x1, y1, g, col, dir) => {
      this.linea(x0, y0, x1, y1, { color: col, grosor: g, a: 0.35 });
      const L = Math.hypot(x1 - x0, y1 - y0);
      const n = 3;
      for (let i = 0; i < n; i += 1) {
        let k = this.animado() ? (this.t * 0.55 + i / n) % 1 : 0.3 + i * 0.25;
        if (dir < 0) k = 1 - k;
        const px = x0 + (x1 - x0) * k; const py = y0 + (y1 - y0) * k;
        const ux = ((x1 - x0) / L) * dir; const uy = ((y1 - y0) / L) * dir;
        this.flecha(px - ux * 8, py - uy * 8, px + ux * 10, py + uy * 10, { color: col, grosor: Math.min(5, 2 + g * 0.06), punta: 11 });
      }
    };
    tubo(cx, 84, cx, yc - R, gH, 'caliente', refri ? -1 : 1);
    tubo(cx, yc + R, cx, 328, gC, 'frio', refri ? -1 : 1);
    tubo(cx + R, yc, cx + R + 92, yc, gW, 'verde', refri ? -1 : 1);
    this.circulo(cx, yc, R, { relleno: 'tarjeta', borde: 'tinta', grosor: 3 });
    const c = this.ctx;
    c.save(); c.translate(cx, yc); c.rotate((refri ? -1 : 1) * (this.animado() ? this.t * 1.6 : 0));
    for (let i = 0; i < 3; i += 1) { c.rotate((Math.PI * 2) / 3); this.linea(0, 0, 0, -28, { color: 'gris', grosor: 5 }); }
    c.restore();
    if (nombre) this.texto(nombre, cx, yc + 8, { tam: 24, alin: 'center', negrita: true, halo: true, mate: true });
    const v = (q) => (valores ? ` = ${num(q, 4)} ${u}` : '');
    this.texto(`Q_H${v(d.QH)}`, cx + gH / 2 + 8, 130, { tam: 18, color: 'caliente', negrita: true, halo: true });
    this.texto(`Q_C${v(d.QC)}`, cx + gH / 2 + 8, 296, { tam: 18, color: 'frio', negrita: true, halo: true });
    this.texto(`W${v(d.W)}`, cx + R + 8, yc - 16, { tam: 18, color: 'verde', negrita: true, halo: true });
  }

  dibujar() {
    const { s } = this;
    if (s.modo === 'dos') {
      this.una(8, s.B, { nombre: 'B', ancho: 250, u: '' });
      this.una(296, s.A, { nombre: 'A', ancho: 250, u: '' });
      this.texto(`e = ${num(s.B.W / s.B.QH, 3)}`, 108, 408, { tam: 19, alin: 'center', negrita: true });
      this.texto(`e = ${num(s.A.W / s.A.QH, 3)}`, 396, 408, { tam: 19, alin: 'center', negrita: true });
      return;
    }
    const refri = s.modo === 'refrigerador';
    this.una(40, s, { ancho: 300, refri, valores: s.valores !== false, TH: s.TH, TC: s.TC, u: s.u || 'J' });
    const x = 396;
    if (this.foco === 'balance') { this.texto('lo que entra =', x, 92, { tam: 18 }); this.texto('lo que sale', x, 114, { tam: 18 }); this.texto('Q_H = W + Q_C', x, 150, { tam: 22, mate: true, color: 'azul', negrita: true }); }
    if (this.foco === 'eficiencia' || this.foco === 'carnot' || this.foco === 'listo') {
      if (s.W && s.QH) this.barra(x, 96, 140, 16, refri ? limitar(s.QC / s.W / 8) : s.W / s.QH, { color: 'verde', etiqueta: refri ? 'K' : 'e', valor: refri ? num(s.QC / s.W, 3) : `${num((s.W / s.QH) * 100, 3)} %` });
      if (s.TH && s.TC && !refri) this.barra(x, 158, 140, 16, 1 - s.TC / s.TH, { color: 'morado', etiqueta: 'e máx (Carnot)', valor: `${num((1 - s.TC / s.TH) * 100, 3)} %` });
      if (s.TH && s.TC && refri) this.barra(x, 158, 140, 16, limitar((s.TC / (s.TH - s.TC)) / 8), { color: 'morado', etiqueta: 'K máx (Carnot)', valor: num(s.TC / (s.TH - s.TC), 3) });
    }
  }
}

export class VisualOtto extends Visual {
  constructor(spec) {
    super({ W: 560, H: 360 });
    this.s = spec;
    this.r = null;
    this.e = new Ejes(this, { x: 66, y: 40, w: 440, h: 244, x0: 1, x1: 14, y0: 0, y1: 0.75, etX: 'r = V_{máx}/V_{mín}', etY: 'e' });
    this.deslizador({ etiqueta: 'Compresión r', min: 1.2, max: 14, paso: 0.1, valor: 8.5, formato: (v) => `${num(v, 3)} → ${num((1 - v ** (1 - spec.gamma)) * 100, 3)} %`, alMover: (v) => { this.r = v; } });
    this.describe('Eficiencia del ciclo Otto contra la relación de compresión');
  }

  dibujar() {
    const { e, s } = this;
    const f = (r) => 1 - r ** (1 - s.gamma);
    e.dibujar({ tam: 17 });
    for (const y of [0.2, 0.4, 0.6]) { this.linea(e.x, e.Y(y), e.x + e.w, e.Y(y), { color: 'borde', grosor: 1 }); this.texto(`${y * 100} %`, e.x - 7, e.Y(y) + 5, { tam: 13, alin: 'right', color: 'tinta3' }); }
    for (let r = 2; r <= 14; r += 2) this.texto(String(r), e.X(r), e.y + e.h + 17, { tam: 13, alin: 'center', color: 'tinta3' });
    e.curva(f, 1, 14, { color: 'azul', hasta: this.foco === 'curva' ? suave(this.fase(1.6)) : 1 });
    (s.puntos || []).forEach(([r, v, n], i) => {
      const act = this.foco === i; const ver = act || this.foco === 'todo' || (typeof this.foco === 'number' && i < this.foco);
      if (!ver) { this.circulo(e.X(r), e.Y(v), 4, { relleno: alfa('gris', 0.6), borde: null }); return; }
      this.circulo(e.X(r), e.Y(v), act ? 9 + Math.sin(this.t * 4) : 6, { relleno: 'amarillo', borde: 'tinta' });
      this.linea(e.X(r), e.y + e.h, e.X(r), e.Y(v), { color: 'tinta3', grosor: 1.3, guion: [4, 4] });
      this.texto(`${n}: ${num(v * 100, 3)} %`, e.X(r) + 6, e.Y(v) + 24 + (i % 2) * 18, { tam: 15, negrita: act, halo: true });
    });
    if (this.r) this.circulo(e.X(this.r), e.Y(f(this.r)), 8, { relleno: 'naranja', borde: 'tinta' });
    this.texto('Cada vez cuesta más ganar eficiencia: la curva se aplana', 280, 348, { tam: 15, alin: 'center', color: 'tinta3' });
  }
}

/* ============================================================== Entropía */
export class VisualEntropia extends Visual {
  constructor(spec) {
    super({ W: 560, H: 400 });
    this.s = spec;
    this.n = 99;
    this.describe('Cambios de entropía de cada parte y del total');
  }

  dibujar() {
    const { s } = this;
    const ps = s.partes || [];
    const tot = ps.reduce((a, p) => a + p.dS, 0);
    const tope = Math.max(1e-9, ...ps.map((p) => Math.abs(p.dS)), Math.abs(tot));
    const ver = this.foco === 'total' || this.foco === null || this.foco === 'listo' ? ps.length : typeof this.foco === 'number' ? this.foco + 1 : ps.length;
    const x0 = 290;
    this.linea(x0, 40, x0, 60 + ps.length * 58, { color: 'tinta2', grosor: 2 });
    this.texto('pierde entropía', x0 - 12, 34, { tam: 15, alin: 'right', color: 'frio' });
    this.texto('gana entropía', x0 + 12, 34, { tam: 15, color: 'caliente' });
    ps.forEach((p, i) => {
      if (i >= ver) return;
      const y = 60 + i * 58;
      const k = i === this.foco ? suave(this.fase(1.1)) : 1;
      const w = (p.dS / tope) * 190 * k;
      this.caja(Math.min(x0, x0 + w), y, Math.max(2, Math.abs(w)), 30, { r: 5, relleno: alfa(p.dS >= 0 ? 'caliente' : 'frio', 0.55) });
      this.texto(p.n, p.dS >= 0 ? x0 - 12 : x0 + 12, y + 21, { tam: 17, alin: p.dS >= 0 ? 'right' : 'left', negrita: true });
      this.texto(`${p.dS > 0 ? '+' : ''}${num(p.dS, 4)} ${s.u || 'J/K'}`, p.dS >= 0 ? x0 + w + 8 : x0 + w - 8, y + 21, { tam: 16, alin: p.dS >= 0 ? 'left' : 'right', halo: true });
    });
    if (this.foco === 'total' || this.foco === 'listo' || this.foco === null) {
      const y = 82 + ps.length * 58;
      this.linea(60, y - 8, 500, y - 8, { color: 'borde', grosor: 2 });
      const cero = Math.abs(tot) < tope * 1e-3;
      const w = (tot / tope) * 190 * suave(this.fase(1.2));
      this.caja(Math.min(x0, x0 + w), y + 4, Math.max(3, Math.abs(w)), 34, { r: 5, relleno: cero ? 'gris' : 'verde' });
      this.texto('total', x0 - 12, y + 28, { tam: 20, alin: 'right', negrita: true });
      this.texto(cero ? '0' : `${tot > 0 ? '+' : ''}${num(tot, 4)} ${s.u || 'J/K'}`, x0 + Math.max(0, w) + 10, y + 28, { tam: 19, negrita: true, color: 'verde', halo: true });
      this.texto(cero ? 'Reversible: la entropía del universo no cambia' : tot > 0 ? 'Irreversible: la entropía del universo aumentó' : 'Imposible: no puede disminuir', 280, Math.min(388, y + 72), { tam: 17, alin: 'center', color: cero ? 'tinta2' : tot > 0 ? 'verde' : 'rojo', negrita: true });
    }
  }
}

/* ============================================================== Función libre */
export class VisualFuncion extends Visual {
  constructor(spec) {
    super({ W: 560, H: 380 });
    this.s = spec;
    this.f = evaluador(spec.expr || 'x');
    const a = Number(spec.x_min ?? spec.a ?? 0); const b = Number(spec.x_max ?? spec.b ?? 5);
    this.a = Math.min(a, b); this.b = Math.max(a, b) === Math.min(a, b) ? Math.min(a, b) + 1 : Math.max(a, b);
    let y0 = Infinity; let y1 = -Infinity;
    for (let i = 0; i <= 120; i += 1) { const y = this.f(this.a + ((this.b - this.a) * i) / 120); if (Number.isFinite(y)) { y0 = Math.min(y0, y); y1 = Math.max(y1, y); } }
    if (!Number.isFinite(y0)) { y0 = 0; y1 = 1; }
    if (y0 > 0) y0 = 0;
    if (y1 < 0) y1 = 0;
    const d = (y1 - y0) || 1;
    this.e = new Ejes(this, { x: 66, y: 46, w: 440, h: 260, x0: this.a, x1: this.b, y0: y0 - d * 0.02, y1: y1 + d * 0.12, etX: spec.x_label || 'x', etY: spec.y_label || 'y' });
    this.x = null;
    this.deslizador({ etiqueta: spec.x_label || 'x', min: this.a, max: this.b, paso: (this.b - this.a) / 200, valor: (this.a + this.b) / 2, formato: (v) => `${num(v, 3)} → ${num(this.f(v), 4)}`, alMover: (v) => { this.x = v; } });
    if (spec.riemann) {
      this.n = 6;
      this.deslizador({ etiqueta: 'Rectángulos', min: 1, max: 60, paso: 1, valor: 6, formato: (v) => String(v), alMover: (v) => { this.n = v; } });
    }
    this.describe(`Gráfica de ${spec.expr}`);
  }

  dibujar() {
    const { e, s } = this;
    const sa = s.shade_from != null ? Number(s.shade_from) : this.a; const sb = s.shade_to != null ? Number(s.shade_to) : this.b;
    if (s.riemann) {
      const dx = (this.b - this.a) / this.n;
      let suma = 0;
      for (let i = 0; i < this.n; i += 1) {
        const x = this.a + i * dx; const y = this.f(x + dx / 2); suma += y * dx;
        this.caja(e.X(x), e.Y(Math.max(0, y)), e.X(x + dx) - e.X(x), Math.abs(e.Y(0) - e.Y(y)), { r: 0, relleno: alfa('azul', 0.2), borde: 'azul', grosor: 1 });
      }
      this.texto(`suma ≈ ${num(suma, 4)} ${s.unit || ''}`, 520, 40, { tam: 18, alin: 'right', color: 'azul', negrita: true });
      if (s.exact_value !== undefined) this.texto(`exacto = ${num(Number(s.exact_value), 4)} ${s.unit || ''}`, 520, 64, { tam: 16, alin: 'right', color: 'verde' });
    } else if (s.shade !== false && (s.shade_from != null || s.area)) e.area(this.f, sa, sb, { relleno: alfa('azul', 0.18), base: 0 });
    e.dibujar({ tam: 17 });
    e.curva(this.f, this.a, this.b, { color: 'azul', hasta: suave(this.fase(1.6)) });
    for (let i = 0; i <= 4; i += 1) { const x = this.a + ((this.b - this.a) * i) / 4; this.texto(num(x, 3), e.X(x), e.y + e.h + 18, { tam: 14, alin: 'center', color: 'tinta3' }); const y = e.y0 + ((e.y1 - e.y0) * i) / 4; this.texto(num(y, 3), e.x - 6, e.Y(y) + 4, { tam: 13, alin: 'right', color: 'tinta3' }); }
    if (this.x !== null) { const y = this.f(this.x); this.linea(e.X(this.x), e.Y(0), e.X(this.x), e.Y(y), { color: 'naranja', guion: [5, 4] }); this.circulo(e.X(this.x), e.Y(y), 7, { relleno: 'amarillo', borde: 'tinta' }); }
    if (s.title) this.texto(s.title, 66, 26, { tam: 18, negrita: true });
  }
}
