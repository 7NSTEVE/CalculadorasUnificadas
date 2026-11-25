/**
 * Dilatación térmica: barras, lata con líquido, pandeo, cinta métrica, anillo, cilindro y láminas.
 * Lo que crece se exagera a propósito (y se avisa): a escala real no se vería nada.
 */

import { alfa, limitar, mezcla, num, suave, Visual } from './base.js';

export class VisualDilatacion extends Visual {
  constructor(spec) {
    super({ W: 560, H: 380 });
    this.s = spec;
    this.u = 0;
    this.manual = false;
    this.describe(`Dilatación térmica: ${spec.caso}`);
    const T0 = spec.T0 ?? 20;
    const rango = spec.dTmax ?? Math.abs((spec.T1 ?? T0 + (spec.dT ?? 60)) - T0);
    const signo = (spec.T1 ?? T0 + 1) < T0 ? -1 : 1;
    this.T0 = T0;
    this.rango = rango;
    this.signo = signo;
    this.sl = this.deslizador({
      etiqueta: 'Temperatura', min: 0, max: 1, paso: 0.01, valor: 0,
      formato: (v) => `${num(T0 + signo * rango * v, 4)} °C`,
      alMover: (v) => { this.u = v; this.manual = true; },
    });
  }

  nivel() {
    if (this.manual) return this.u;
    if (['calentar', 'crece', 'triangulo', 'altura', 'vacio', 'barra', 'cinta', 'lectura', 'listo', 'enfriar'].includes(this.foco)) {
      const k = this.animado() ? suave(this.fase(2.4)) : 1;
      if (['triangulo'].includes(this.foco)) return Math.max(this.u, 0.9 * k);
      this.u = Math.max(this.u, k);
      return this.foco === 'calentar' && this.animado() ? 0.5 - 0.5 * Math.cos(this.tFoco * 1.1) : k;
    }
    return this.u;
  }

  termometro(x, y, h, f, T) {
    this.caja(x, y, 16, h, { r: 8, relleno: 'tarjeta', borde: 'tinta2', grosor: 1.8 });
    const hh = (h - 12) * (0.12 + 0.82 * limitar(this.signo > 0 ? f : 1 - f));
    this.caja(x + 4, y + h - 4 - hh, 8, hh, { r: 4, relleno: 'caliente' });
    this.circulo(x + 8, y + h + 8, 14, { relleno: 'caliente', borde: 'tinta2', grosor: 1.8 });
    this.texto(`${num(T, 4)} °C`, x + 8, y - 10, { tam: 16, alin: 'center', negrita: true });
  }

  dibujar() {
    const f = this.nivel();
    if (!this.manual) this.sl.poner(f);
    const T = this.T0 + this.signo * this.rango * f;
    const d = { barras: 'barras', lata: 'lata', pandeo: 'pandeo', cinta: 'cinta', anillo: 'anillo', cilindro: 'cilindro', laminas: 'laminas', compuesta: 'compuesta' }[this.s.caso] || 'barras';
    this[`_${d}`](f, T);
    if (this.s.caso !== 'pandeo') this.termometro(512, 60, 210, f, T);
    else this.termometro(512, 40, 150, f, T);
    this.texto(this.s.pie || 'Crecimiento exagerado para que se note', 16, 368, { tam: 13.5, color: 'tinta3' });
  }

  _barras(f, T) {
    const bs = this.s.barras;
    const Lm = Math.max(...bs.map((b) => b.L));
    const k = 330 / Lm;
    const exag = this.s.exag || 0.16 / Math.max(...bs.map((b) => b.a * b.L / Lm * this.rango));
    const dT = this.signo * this.rango * f;
    bs.forEach((b, i) => {
      const y = 96 + i * 96;
      const L = b.L * (1 + b.a * dT * exag);
      const x1 = 60 + L * k;
      this.caja(60, y, b.L * k, 40, { r: 4, relleno: alfa(b.color, 0.16), borde: 'tinta3', grosor: 1.2 });
      this.caja(60, y, L * k, 40, { r: 4, relleno: alfa(b.color, 0.5), borde: 'tinta', grosor: 2.2 });
      this.texto(`${b.n}`, 70, y - 8, { tam: 18, negrita: true, color: b.color });
      this.texto(`L = ${num(b.L * (1 + b.a * dT), 6)} ${this.s.u || 'cm'}`, 200, y + 26, { tam: 17 });
      if (f > 0.02) this.flecha(60 + b.L * k, y + 52, x1, y + 52, { color: 'naranja', grosor: 2.4, punta: 7 });
      b.x1 = x1;
    });
    this.linea(60, 70, 60, 260 + (bs.length - 2) * 40, { grosor: 4 });
    if (bs.length === 2) {
      const [a, b] = bs;
      const xa = Math.min(a.x1, b.x1); const xb = Math.max(a.x1, b.x1);
      this.linea(xa, 130, xa, 300, { color: 'rojo', grosor: 1.6, guion: [5, 4] });
      this.linea(xb, 130, xb, 300, { color: 'rojo', grosor: 1.6, guion: [5, 4] });
      this.flecha((xa + xb) / 2, 296, xa, 296, { color: 'rojo', punta: 8 });
      this.flecha((xa + xb) / 2, 296, xb, 296, { color: 'rojo', punta: 8 });
      const dif = Math.abs(b.L * (1 + b.a * this.signo * this.rango * f) - a.L * (1 + a.a * this.signo * this.rango * f));
      this.texto(`diferencia: ${num(dif, 4)} ${this.s.u || 'cm'}`, (xa + xb) / 2, 322, { tam: 17, alin: 'center', color: 'rojo', negrita: true });
    }
  }

  _lata(f) {
    const { V, bc, bl, Vliq } = this.s;
    const dT = this.rango * f * this.signo;
    const ex = this.s.exag || 14;
    const Vc = V * (1 + bc * dT); const Vl = Vliq * (1 + bl * dT);
    const x = 120; const w = 150; const yb = 320;
    const H = 230;
    const hc = H * (1 + (Vc / V - 1) * ex * 0.4);
    const vacio = limitar((Vc - Vl) / V * 22, 0, 0.5);
    const hl = hc * (1 - vacio);
    const onda = (dx) => Math.sin(this.t * 2 + dx * 0.06) * 1.6;
    const pts = [[x, yb]];
    for (let i = 0; i <= 20; i += 1) pts.push([x + (w * i) / 20, yb - hl + onda((w * i) / 20)]);
    pts.push([x + w, yb]);
    this.poligono(pts, { relleno: alfa('azul', 0.34) });
    this.poligono([[x, yb - hc], [x, yb], [x + w, yb], [x + w, yb - hc]], { borde: 'tinta', grosor: 4, cerrar: false });
    this.linea(x - 4, yb - hc, x + w + 4, yb - hc, { grosor: 4, color: 'gris' });
    if (Vc - Vl > 0.01) {
      this.flecha(x + w + 22, yb - hl - 2, x + w + 22, yb - hc + 2, { color: 'naranja', punta: 7 });
      this.texto(`vacío: ${num(Vc - Vl, 3)} ${this.s.u || 'mL'}`, x + w + 34, yb - (hc + hl) / 2 + 5, { tam: 18, color: 'naranja', negrita: true });
    } else this.texto('¡llena justo!', x + w + 34, yb - hc + 6, { tam: 19, color: 'rojo', negrita: true });
    this.texto(`${this.s.nc || 'lata'}: ${num(Vc, 6)} ${this.s.u || 'mL'}`, x + w + 34, 210, { tam: 18 });
    this.texto(`${this.s.nl || 'agua'}: ${num(Vl, 6)} ${this.s.u || 'mL'}`, x + w + 34, 240, { tam: 18, color: 'azul' });
    this.texto(`β recipiente = ${num(bc, 2)}`, x + w + 34, 278, { tam: 14, color: 'tinta3' });
    this.texto(`β líquido = ${num(bl, 2)}`, x + w + 34, 298, { tam: 14, color: 'tinta3' });
  }

  _pandeo(f) {
    const { L, y: yReal } = this.s;
    const x0 = 62; const x1 = 470; const yb = 250; const xm = (x0 + x1) / 2;
    const alt = 118 * Math.sqrt(limitar(f));
    this.caja(x0 - 26, yb - 60, 26, 110, { r: 2, relleno: alfa('gris', 0.35), borde: 'tinta' });
    this.caja(x1, yb - 60, 26, 110, { r: 2, relleno: alfa('gris', 0.35), borde: 'tinta' });
    const losa = (ax, ay, bx, by) => {
      const L2 = Math.hypot(bx - ax, by - ay); const nx = -(by - ay) / L2; const ny = (bx - ax) / L2; const g = 13;
      this.poligono([[ax, ay], [bx, by], [bx + nx * g, by + ny * g], [ax + nx * g, ay + ny * g]], { relleno: alfa(mezcla('gris', 'caliente', f * 0.7), 0.55), borde: 'tinta', grosor: 2 });
    };
    const tri = this.foco === 'triangulo' || this.foco === 'altura';
    this.linea(x0, yb, x1, yb, { color: 'tinta3', grosor: 1.4, guion: [6, 5] });
    losa(x0, yb, xm, yb - alt);
    losa(xm, yb - alt, x1, yb);
    if (alt > 4) {
      this.flecha(xm, yb - alt / 2, xm, yb - alt + 2, { color: 'rojo', punta: 8 });
      this.flecha(xm, yb - alt / 2, xm, yb - 2, { color: 'rojo', punta: 8 });
      this.texto(this.foco === 'altura' ? `y = ${num(yReal, 3)} m` : 'y = ?', xm + 12, yb - alt / 2 + 6, { tam: 20, mate: true, color: 'rojo', negrita: true, halo: true });
    }
    this.texto(`${num(L / 2, 4)} m`, (x0 + xm) / 2, yb + 34, { tam: 18, alin: 'center', color: tri ? 'azul' : 'tinta2', negrita: tri });
    this.texto(`${num(L / 2, 4)} m`, (x1 + xm) / 2, yb + 34, { tam: 18, alin: 'center', color: 'tinta2' });
    if (tri) {
      this.poligono([[x0, yb], [xm, yb], [xm, yb - alt]], { relleno: alfa('azul', 0.14), borde: 'azul', grosor: 2.6 });
      this.caja(xm - 13, yb - 13, 13, 13, { r: 0, borde: 'azul', grosor: 1.6 });
      this.texto('hipotenusa = losa dilatada', (x0 + xm) / 2 - 26, yb - alt * 0.5 - 20, { tam: 16, alin: 'center', color: 'azul', halo: true, angulo: -Math.atan2(alt, xm - x0) });
    }
    this.texto(`Largo total fijo: ${num(L, 4)} m`, xm, yb + 70, { tam: 16, alin: 'center', color: 'tinta3' });
  }

  _cinta(f) {
    const { L, ab, aa } = this.s;
    const dT = this.signo * this.rango * f;
    const ex = 130;
    const enBarra = ['barra', 'lectura', 'listo'].includes(this.foco) || this.manual || this.foco === null;
    const enCinta = ['cinta', 'lectura', 'listo'].includes(this.foco) || this.manual || this.foco === null;
    const kb = 1 + ab * (enBarra ? dT : 0) * ex; const kc = 1 + aa * (enCinta ? dT : 0) * ex;
    const x0 = 46; const W = 400;
    this.texto('barra de latón', x0, 74, { tam: 18, color: 'naranja', negrita: true });
    this.caja(x0, 86, W, 40, { r: 3, relleno: alfa('naranja', 0.12), borde: 'tinta3', grosor: 1 });
    this.caja(x0, 86, W * kb, 40, { r: 3, relleno: alfa('naranja', 0.5), borde: 'tinta', grosor: 2.2 });
    this.texto('cinta de acero', x0, 176, { tam: 18, color: 'azul', negrita: true });
    const Wc = 440 * kc;
    this.caja(x0, 186, Wc, 46, { r: 3, relleno: alfa('azul', 0.16), borde: 'tinta', grosor: 2 });
    const n = 22;
    for (let i = 0; i <= n; i += 1) {
      const x = x0 + (Wc * i) / n;
      if (x > 500) break;
      this.linea(x, 186, x, 186 + (i % 5 === 0 ? 22 : 12), { grosor: 1.6 });
    }
    this.linea(x0 + W * kb, 80, x0 + W * kb, 246, { color: 'rojo', grosor: 2, guion: [6, 4] });
    const real = L * (1 + ab * (enBarra ? dT : 0));
    const sep = 1 + aa * (enCinta ? dT : 0);
    this.texto(`largo real: ${num(real, 6)} cm`, x0, 276, { tam: 18 });
    this.texto(`cada "1 cm" mide: ${num(sep, 6)} cm`, x0, 302, { tam: 18 });
    this.texto(`la cinta marca: ${num(real / sep, 6)} cm`, x0, 330, { tam: 19, color: 'rojo', negrita: true });
  }

  _anillo(f) {
    const { D, d, a, unidad = 'cm', T1b, T1a } = this.s;
    const ex = 14;
    const dT = this.rango * f * this.signo;
    const k = 120 / Math.max(D, d);
    const modo = this.s.modo || 'anillo';
    const Rb = (D * k) * (1 + (modo === 'barra' ? a * dT * ex : 0));
    const Ra = (d * k) * (1 + (modo === 'anillo' ? a * dT * ex : 0));
    const cx = 170; const cy = 190;
    this.circulo(cx, cy, Ra * 0.82 + 34, { relleno: alfa(mezcla('gris', 'caliente', modo === 'anillo' ? limitar(f * this.signo + 0.1) : 0), 0.5), borde: 'tinta' });
    this.circulo(cx, cy, Ra * 0.82, { relleno: 'tarjeta', borde: 'tinta' });
    this.circulo(390, cy, Rb * 0.82, { relleno: alfa(mezcla('gris', 'frio', modo === 'barra' ? f : 0), 0.5), borde: 'tinta' });
    const dr = d * (1 + (modo === 'anillo' ? a * dT : 0)); const Dr = D * (1 + (modo === 'barra' ? a * dT : 0));
    this.texto(`hueco: ${num(dr, 5)} ${unidad}`, cx, cy + 6, { tam: 17, alin: 'center', negrita: true });
    this.texto(`${this.s.nb || 'barra'}: ${num(Dr, 5)} ${unidad}`, 390, cy + 6, { tam: 17, alin: 'center', negrita: true });
    const cabe = dr >= Dr - 1e-9;
    this.texto(cabe ? 'Ya entra' : 'No entra', 280, 60, { tam: 22, alin: 'center', color: cabe ? 'verde' : 'rojo', negrita: true });
    if (T1a !== undefined || T1b !== undefined) { /* los valores los escribe la lección */ }
  }

  _cilindro(f) {
    const { R, L, a } = this.s;
    const dT = this.rango * f * this.signo;
    const ex = this.s.exag || 18;
    const k = 1 + a * dT * ex;
    const w = 120 * k; const h = 190 * k; const cx = 180; const yb = 320;
    const c = this.ctx;
    this.poligono([[cx - w / 2, yb - h], [cx - w / 2, yb], [cx + w / 2, yb], [cx + w / 2, yb - h]], { relleno: alfa(mezcla('gris', 'caliente', f), 0.5), borde: 'tinta', cerrar: false });
    c.save(); c.beginPath(); c.ellipse(cx, yb - h, w / 2, 16 * k, 0, 0, 7); c.fillStyle = alfa(mezcla('gris', 'caliente', f), 0.8); c.fill(); c.strokeStyle = mezcla('tinta', 'tinta', 0); c.lineWidth = 2.2; c.stroke();
    c.beginPath(); c.ellipse(cx, yb, w / 2, 16 * k, 0, 0, Math.PI); c.stroke(); c.restore();
    const r2 = R * (1 + a * dT); const l2 = L * (1 + a * dT);
    this.flecha(cx, yb - h, cx + w / 2, yb - h, { color: 'rojo', punta: 7 });
    this.texto('R', cx + w / 4, yb - h - 8, { tam: 18, mate: true, color: 'rojo' });
    this.flecha(cx - w / 2 - 18, yb, cx - w / 2 - 18, yb - h, { color: 'azul', punta: 7 });
    this.texto('L', cx - w / 2 - 40, yb - h / 2, { tam: 18, mate: true, color: 'azul' });
    this.texto(`R = ${num(r2, 5)} ${this.s.u || 'cm'}`, 310, 120, { tam: 18, color: 'rojo' });
    this.texto(`L = ${num(l2, 6)} ${this.s.u || 'cm'}`, 310, 150, { tam: 18, color: 'azul' });
    this.texto(`cambio de R: ${num((r2 / R - 1) * 100, 3)} %`, 310, 194, { tam: 16, color: 'tinta2' });
    this.texto(`cambio de área: ${num(((r2 / R) ** 2 - 1) * 100, 3)} %`, 310, 218, { tam: 16, color: 'tinta2' });
    this.texto(`cambio de volumen: ${num(((r2 / R) ** 2 * (l2 / L) - 1) * 100, 3)} %`, 310, 242, { tam: 16, color: 'tinta2' });
  }

  _laminas(f) {
    const ls = this.s.laminas;
    const ex = 0.16 * f;
    let x = 34;
    const k = 44;
    const maxA = Math.max(...ls.map((l) => l.w * l.h));
    ls.forEach((l) => {
      const w = l.w * k; const h = l.h * k;
      const A = l.w * l.h;
      const yb = 280;
      this.caja(x, yb - h, w, h, { r: 2, relleno: alfa('azul', 0.14), borde: 'tinta3' });
      this.caja(x, yb - h * (1 + ex), w * (1 + ex), h * (1 + ex), { r: 2, relleno: alfa(A === maxA ? 'naranja' : 'azul', 0.22), borde: 'tinta', grosor: 2.2 });
      this.texto(l.n, x + w / 2, yb - h / 2 + 6, { tam: 22, mate: true, negrita: true, alin: 'center' });
      this.texto(`${num(A, 2)} L^2`, x + w / 2, yb + 26, { tam: 17, alin: 'center', color: A === maxA ? 'naranja' : 'tinta2', negrita: A === maxA });
      x += w + 40;
    });
    this.texto('Más área inicial, más aumento de área', 34, 44, { tam: 18, color: 'naranja' });
  }

  _compuesta(f) {
    const cs = this.s.combos;
    cs.forEach((co, i) => {
      const y = 70 + i * 88;
      let x = 40;
      let total = 0;
      co.partes.forEach((p) => {
        const w = 150 * (1 + p.a * 0.11 * f);
        this.caja(x, y, w, 36, { r: 3, relleno: alfa(p.color, 0.45), borde: 'tinta', grosor: 2 });
        this.texto(`${p.n}`, x + w / 2, y + 23, { tam: 16, alin: 'center' });
        x += w; total += p.a;
      });
      this.texto(co.n, 40, y - 8, { tam: 17, negrita: true });
      this.texto(`suma: ${total}α_1`, x + 14, y + 24, { tam: 18, color: 'naranja', negrita: true });
    });
  }
}
