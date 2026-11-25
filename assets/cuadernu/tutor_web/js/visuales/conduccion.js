/**
 * Conducción de calor: pared, capas en serie, barras en paralelo, cilindro, esfera y secciones
 * que cambian de área o de conductividad. Abajo siempre va la curva de temperatura.
 */

import { alfa, color, Ejes, evaluador, limitar, mezcla, num, suave, Visual } from './base.js';

function interpola(pts, s) {
  if (!pts || !pts.length) return 0;
  if (s <= pts[0][0]) return pts[0][1];
  for (let i = 1; i < pts.length; i += 1) {
    if (s <= pts[i][0]) {
      const [a, x] = pts[i - 1]; const [b, y] = pts[i];
      return x + ((y - x) * (s - a)) / (b - a || 1);
    }
  }
  return pts[pts.length - 1][1];
}

export class VisualConduccion extends Visual {
  constructor(spec) {
    super({ W: 560, H: 470 });
    this.s = spec;
    this.uT = spec.uT || '°C';
    const geo = spec.geo;
    this.radial = ['cilindro', 'esfera', 'cilindro2', 'esfera2'].includes(geo);
    if (this.radial) {
      const rs = [spec.r1, spec.r2, spec.r3].filter((x) => x !== undefined);
      this.a = rs[0];
      this.b = rs[rs.length - 1];
    } else { this.a = 0; this.b = spec.L ?? spec.h ?? spec.capas?.reduce((q, c) => q + c.e, 0) ?? 1; }
    this.T1 = spec.T1; this.Tn = spec.T3 ?? spec.T2;
    this.perfil = spec.pts || this._perfil();
    this.Tmax = Math.max(...this.perfil.map((p) => p[1]));
    this.Tmin = Math.min(...this.perfil.map((p) => p[1]));
    const exag = spec.exagera && this.perfil.length;
    this.e = new Ejes(this, {
      x: 70, y: 318, w: 420, h: 104, x0: this.a - (this.b - this.a) * 0.0, x1: this.b,
      y0: this.Tmin - (this.Tmax - this.Tmin) * 0.12, y1: this.Tmax + (this.Tmax - this.Tmin) * 0.14,
      etX: this.radial ? 'r (m)' : `${spec.eje || 'x'} (m)`, etY: `T (${this.uT})`,
    });
    this.x = null;
    this.exag = exag;
    const u = (this.b - this.a) / 200;
    const ini = spec.r ?? spec.xm ?? (this.a + this.b) / 2;
    this.sl = this.deslizador({
      etiqueta: this.radial ? 'Radio' : 'Posición', min: this.a, max: this.b, paso: u, valor: ini,
      formato: (v) => `${num(v, 3)} m → ${num(interpola(this.perfil, v), 4)} ${this.uT}`,
      alMover: (v) => { this.x = v; },
    });
    this.x0 = ini;
    this.describe(`Conducción de calor, geometría ${geo}. De ${num(this.T1)} a ${num(this.Tn)} ${this.uT}`);
  }

  _perfil() {
    const { s } = this;
    const N = 60;
    const out = [];
    if (s.geo === 'cilindro2' || s.geo === 'esfera2') {
      const capa = (a, b, A, B) => { for (let i = 0; i <= N; i += 1) { const r = a + ((b - a) * i) / N; const f = s.geo === 'cilindro2' ? Math.log(r / a) / Math.log(b / a) : (1 / a - 1 / r) / (1 / a - 1 / b); out.push([r, A + (B - A) * f]); } };
      capa(s.r1, s.r2, s.T1, s.T2);
      capa(s.r2, s.r3, s.T2, s.T3);
      return out;
    }
    if (s.geo === 'serie') {
      let x = 0;
      out.push([0, s.T[0]]);
      s.capas.forEach((c, i) => { x += c.e; out.push([x, s.T[i + 1]]); });
      return out;
    }
    for (let i = 0; i <= N; i += 1) {
      const r = this.a + ((this.b - this.a) * i) / N;
      let f;
      if (s.perfil === 'lineal' || s.geo === 'pared' || s.geo === 'paralelo') f = i / N;
      else if (s.geo === 'cilindro') f = Math.log(r / this.a) / Math.log(this.b / this.a);
      else if (s.geo === 'esfera') f = (1 / this.a - 1 / r) / (1 / this.a - 1 / this.b);
      else f = i / N;
      out.push([r, s.T1 + (this.Tn - s.T1) * f]);
    }
    return out;
  }

  colorT(T, a = 0.75) {
    const k = (T - this.Tmin) / (this.Tmax - this.Tmin || 1);
    const h = mezcla('frio', 'caliente', k);
    return a >= 1 ? h : alfa(h, a);
  }

  /** Radio real -> radio dibujado (las capas finísimas se exageran para poder verlas). */
  R(r) {
    const rs = [this.s.r1, this.s.r2, this.s.r3].filter((x) => x !== undefined);
    const m = 122;
    if (this.s.exagera && rs.length === 3) {
      const [a, b, c] = rs;
      if (r <= a) return 40 * (r / a);
      if (r <= b) return 40 + (18 * (r - a)) / (b - a);
      return 58 + ((m - 58) * (r - b)) / (c - b);
    }
    const ini = Math.max(30, Math.min(m * 0.62, (this.a / this.b) * m));
    return ini + ((m - ini) * (r - this.a)) / (this.b - this.a);
  }

  seccionRadial() {
    const c = this.ctx;
    const cx = 168; const cy = 148;
    const N = 40;
    for (let i = N; i >= 0; i -= 1) {
      const r = this.a + ((this.b - this.a) * i) / N;
      c.beginPath();
      c.arc(cx, cy, this.R(r), 0, Math.PI * 2);
      c.fillStyle = this.colorT(interpola(this.perfil, r), 1);
      c.globalAlpha = 0.8;
      c.fill();
    }
    c.globalAlpha = 1;
    const ri = this.R(this.a);
    this.circulo(cx, cy, ri, { relleno: 'tarjeta', borde: 'tinta', grosor: 2.2 });
    this.circulo(cx, cy, this.R(this.b), { borde: 'tinta', grosor: 2.2 });
    if (this.s.r3 !== undefined) this.circulo(cx, cy, this.R(this.s.r2), { borde: 'tinta', grosor: 1.8, guion: [5, 4] });
    this.texto(this.s.n0 || (this.s.geo.startsWith('esf') ? 'adentro' : 'fluido'), cx, cy + 5, { tam: 15, alin: 'center', color: 'tinta2' });
    // Flechas de calor que salen (o entran)
    const sale = this.T1 > this.Tn;
    const nF = 8;
    for (let i = 0; i < nF; i += 1) {
      const ang = (i / nF) * Math.PI * 2 + 0.2;
      const u = this.animado() ? (this.t * 0.42 + i * 0.13) % 1 : 0.5;
      const k = sale ? u : 1 - u;
      const r0 = ri + 5 + k * (this.R(this.b) - ri - 4);
      const d = sale ? 13 : -13;
      const a = 0.35 + 0.65 * Math.sin(u * Math.PI);
      this.flecha(cx + Math.cos(ang) * r0, cy + Math.sin(ang) * r0, cx + Math.cos(ang) * (r0 + d), cy + Math.sin(ang) * (r0 + d), { color: 'tinta', grosor: 2.6, punta: 8, a });
    }
    // Cascarón diferencial
    const barr = this.foco === 'dr' || this.foco === 'barrido';
    const r = this.foco === 'barrido' ? this.a + (this.b - this.a) * suave((this.tFoco % 3.4) / 2.6)
      : this.x !== null ? this.x : this.foco === 'dr' ? this.a + (this.b - this.a) * (0.5 + 0.06 * Math.sin(this.t * 2)) : this.foco === 'punto' ? (this.s.r ?? this.x0) : null;
    this.rMarcado = r;
    if (r !== null && r !== undefined) {
      const rr = this.R(r);
      this.circulo(cx, cy, rr, { borde: 'amarillo', grosor: barr ? 8 : 4, a: 0.9 });
      this.circulo(cx, cy, rr - 4, { borde: 'tinta', grosor: 1.2, a: 0.7 });
      this.circulo(cx, cy, rr + 4, { borde: 'tinta', grosor: 1.2, a: 0.7 });
      this.linea(cx, cy, cx + rr * Math.cos(-0.5), cy + rr * Math.sin(-0.5), { color: 'tinta', grosor: 1.8, guion: [5, 4] });
      this.texto('r', cx + rr * 0.5 * Math.cos(-0.5) - 4, cy + rr * 0.5 * Math.sin(-0.5) - 6, { tam: 18, mate: true, halo: true });
      if (barr) this.texto('dr', cx + (rr + 14) * Math.cos(0.55), cy + (rr + 14) * Math.sin(0.55) + 6, { tam: 19, mate: true, color: 'naranja', negrita: true, halo: true });
    }
    const x = 330;
    const fila = (y, et, val, col) => { this.circulo(x, y - 5, 6, { relleno: col, borde: null }); this.texto(et, x + 14, y, { tam: 17 }); this.texto(val, x + 14, y + 22, { tam: 17, color: 'tinta2' }); };
    const et = [this.s.etq1 || 'r_1', this.s.etq2 || 'r_2', 'r_3'];
    if (this.s.r3 !== undefined) {
      fila(48, `${this.s.eti1 || 'r_1'} = ${num(this.s.r1, 3)} m`, `${num(this.s.T1, 4)} ${this.uT}`, this.colorT(this.s.T1, 1));
      fila(112, `${this.s.eti2 || 'r_2'} = ${num(this.s.r2, 3)} m`, this.foco === 'perfil' || this.foco === 'listo' ? `${num(this.s.T2, 4)} ${this.uT}` : `T = ?`, this.colorT(this.s.T2, 1));
      fila(176, `${this.s.eti3 || 'r_3'} = ${num(this.s.r3, 3)} m`, `${num(this.s.T3, 4)} ${this.uT}`, this.colorT(this.s.T3, 1));
      this.texto(`${this.s.n1}: k = ${num(this.s.k1, 3)}`, x, 236, { tam: 15, color: 'tinta2' });
      this.texto(`${this.s.n2}: k = ${num(this.s.k2, 4)}`, x, 256, { tam: 15, color: 'tinta2' });
      if (this.s.exagera) this.texto('(capa fina exagerada)', x, 278, { tam: 13, color: 'tinta3' });
    } else {
      fila(60, `${et[0]} = ${num(this.a, 3)} m`, `${num(this.T1, 4)} ${this.uT}`, this.colorT(this.T1, 1));
      const oculta = this.s.oculta2 && !['perfil', 'listo'].includes(this.foco);
      fila(130, `${et[1]} = ${num(this.b, 3)} m`, oculta ? 'T = ?' : `${num(this.Tn, 4)} ${this.uT}`, this.colorT(this.Tn, 1));
      const A = this.s.geo === 'esfera' ? 'A = 4πr^2' : 'A = 2πrL';
      this.texto(A, x, 208, { tam: 22, mate: true, color: 'azul', negrita: true });
      if (this.s.k !== undefined) this.texto(`k = ${typeof this.s.k === 'number' ? num(this.s.k, 4) : this.s.k}`, x, 238, { tam: 17, color: 'tinta2' });
    }
  }

  bloque(x0, y0, w, h, desde, hasta, forma) {
    // Rebanadas coloreadas por temperatura; forma(u) devuelve semialto relativo 0..1
    const N = 48;
    for (let i = 0; i < N; i += 1) {
      const u = i / N;
      const T = interpola(this.perfil, desde + (hasta - desde) * (u + 0.5 / N));
      const f1 = forma(u); const f2 = forma(u + 1 / N);
      this.poligono([[x0 + w * u, y0 - (h / 2) * f1], [x0 + w * (u + 1 / N) + 0.6, y0 - (h / 2) * f2], [x0 + w * (u + 1 / N) + 0.6, y0 + (h / 2) * f2], [x0 + w * u, y0 + (h / 2) * f1]], { relleno: this.colorT(T, 1), a: 0.8 });
    }
    const arriba = []; const abajo = [];
    for (let i = 0; i <= N; i += 1) { const u = i / N; arriba.push([x0 + w * u, y0 - (h / 2) * forma(u)]); abajo.push([x0 + w * u, y0 + (h / 2) * forma(u)]); }
    this.poligono(arriba, { borde: 'tinta', grosor: 2.2, cerrar: false });
    this.poligono(abajo, { borde: 'tinta', grosor: 2.2, cerrar: false });
  }

  focos(x0, x1, yc, alto, izq, der) {
    this.caja(x0 - 44, yc - alto / 2, 44, alto, { r: 4, relleno: this.colorT(izq, 0.9), borde: 'tinta' });
    this.caja(x1, yc - alto / 2, 44, alto, { r: 4, relleno: this.colorT(der, 0.9), borde: 'tinta' });
    this.texto(`${num(izq, 4)}`, x0 - 22, yc - alto / 2 - 8, { tam: 16, alin: 'center', negrita: true });
    const oculta = this.s.oculta2 && !['perfil', 'listo'].includes(this.foco);
    this.texto(oculta ? '?' : `${num(der, 4)}`, x1 + 22, yc - alto / 2 - 8, { tam: 16, alin: 'center', negrita: true });
    this.texto(this.uT, x0 - 22, yc + alto / 2 + 16, { tam: 13, alin: 'center', color: 'tinta3' });
  }

  calor(x0, x1, yc, sep, n = 3, dir = 1) {
    for (let i = 0; i < n; i += 1) {
      const u = this.animado() ? (this.t * 0.4 + i / n) % 1 : 0.4 + i * 0.1;
      const k = dir > 0 ? u : 1 - u;
      const x = x0 + (x1 - x0 - 30) * k;
      const y = yc + (i - (n - 1) / 2) * sep;
      this.flecha(dir > 0 ? x : x + 30, y, dir > 0 ? x + 30 : x, y, { color: 'tinta', grosor: 2.6, punta: 9, a: 0.3 + 0.7 * Math.sin(u * Math.PI) });
    }
  }

  seccionPlana() {
    const { s } = this;
    const x0 = 92; const x1 = 468; const yc = 146;
    const w = x1 - x0;
    const dir = this.T1 >= this.Tn ? 1 : -1;
    if (s.geo === 'serie') {
      const total = s.capas.reduce((q, c) => q + c.e, 0);
      let x = x0;
      let ac = 0;
      const alto = 130;
      // Con capas muy distintas se da un ancho mínimo para que todas se vean
      const pesos = s.capas.map((c) => Math.max(0.16, c.e / total));
      const sp = pesos.reduce((q, z) => q + z, 0);
      this.anchos = [];
      s.capas.forEach((c, i) => {
        const wi = (w * pesos[i]) / sp;
        this.bloque(x, yc, wi, alto, ac, ac + c.e, () => 1);
        this.linea(x, yc - alto / 2, x, yc + alto / 2, { grosor: 2.2 });
        this.texto(c.n, x + wi / 2, yc + 5, { tam: 17, alin: 'center', halo: true, negrita: true });
        this.texto(`k = ${c.kt || num(c.k, 4)}`, x + wi / 2, yc + 26, { tam: 14, alin: 'center', halo: true, color: 'tinta2' });
        if (i > 0) {
          const inc = s.incognitas && s.incognitas.includes(i) && !['perfil', 'listo'].includes(this.foco);
          this.texto(inc ? 'T = ?' : `${num(s.T[i], 4)} ${this.uT}`, x, yc + alto / 2 + 20, { tam: 15, alin: 'center', color: 'naranja', negrita: true });
          this.circulo(x, yc + alto / 2, 4, { relleno: 'naranja', borde: null });
        }
        this.anchos.push([x, wi, ac, c.e]);
        x += wi; ac += c.e;
      });
      this.focos(x0, x1, yc, alto, s.T[0], s.T[s.T.length - 1]);
      this.calor(x0 + 6, x1 - 6, yc - 34, 0, 3, dir);
      return;
    }
    if (s.geo === 'paralelo') {
      const n = s.barras.length;
      const alto = 190;
      const maxA = Math.max(...s.barras.map((b) => b.A || 1));
      s.barras.forEach((b, i) => {
        const y = yc - alto / 2 + (alto / n) * (i + 0.5);
        const hh = (alto / n) * 0.66 * Math.sqrt((b.A || 1) / maxA);
        if (b.partes) {
          const tot = b.partes.reduce((q, c) => q + c.e, 0);
          let x = x0;
          b.partes.forEach((c) => {
            const wi = (w * c.e) / tot;
            this.bloque(x, y, wi, hh, 0, this.b, () => 1);
            this.texto(c.n, x + wi / 2, y + 6, { tam: 17, alin: 'center', halo: true, negrita: true, mate: true });
            this.linea(x, y - hh / 2, x, y + hh / 2, { grosor: 1.6 });
            x += wi;
          });
        } else {
          this.bloque(x0, y, w, hh, 0, this.b, b.forma ? evaluador(b.forma) : () => 1);
          this.texto(b.n, x0 + w / 2, y + 6, { tam: 18, alin: 'center', halo: true, negrita: true, mate: true });
        }
        const act = this.foco === i || this.foco === 'total' || this.foco === null || this.foco === 'flujo';
        if (act) this.calor(x0 + 6, x1 - 6, y - hh * 0.28, 0, 2, dir);
      });
      this.focos(x0, x1, yc, alto, this.T1, this.Tn);
      return;
    }
    const forma = s.forma ? evaluador(s.forma) : () => 1;
    const alto = s.alto || 150;
    this.bloque(x0, yc, w, alto, this.a, this.b, (u) => Math.max(0.05, forma(u)));
    this.focos(x0, x1, yc, Math.max(90, alto * 0.8), this.T1, this.Tn);
    this.calor(x0 + 6, x1 - 6, yc, 34, 3, dir);
    const barr = this.foco === 'dr' || this.foco === 'dx' || this.foco === 'barrido';
    const u = this.foco === 'barrido' ? suave((this.tFoco % 3.4) / 2.6) : this.x !== null ? (this.x - this.a) / (this.b - this.a) : barr ? 0.5 + 0.05 * Math.sin(this.t * 2) : this.foco === 'punto' ? (this.x0 - this.a) / (this.b - this.a) : null;
    if (u !== null) {
      const X = x0 + w * limitar(u);
      const h2 = (alto / 2) * Math.max(0.05, forma(limitar(u)));
      this.rMarcado = this.a + (this.b - this.a) * limitar(u);
      this.caja(X - 7, yc - h2, 14, h2 * 2, { r: 1, relleno: alfa('amarillo', 0.55), borde: 'tinta', grosor: 1.6 });
      if (barr) this.texto(`d${s.eje || 'x'}`, X, yc + h2 + 20, { tam: 20, mate: true, alin: 'center', color: 'naranja', negrita: true });
      if (barr && s.etA) this.texto(s.etA, X, yc - h2 - 10, { tam: 18, mate: true, alin: 'center', color: 'azul', negrita: true, halo: true });
    }
    if (s.nota) this.texto(s.nota, 280, yc + alto / 2 + 44, { tam: 15, alin: 'center', color: 'tinta2' });
  }

  dibujar() {
    this.rMarcado = null;
    if (this.radial) this.seccionRadial(); else this.seccionPlana();
    const { e } = this;
    const c = this.ctx;
    e.dibujar({ tam: 15, corteX: this.a > 0 });
    const enPerfil = ['perfil', 'listo', 'punto', 'barrido', null, undefined].includes(this.foco) || this.x !== null;
    const oculta = this.s.oculta2 && !['perfil', 'listo'].includes(this.foco);
    c.save();
    c.globalAlpha = oculta ? 0.25 : enPerfil ? 1 : 0.5;
    c.lineWidth = 3.4;
    const grad = c.createLinearGradient(e.X(this.a), 0, e.X(this.b), 0);
    for (let i = 0; i <= 10; i += 1) grad.addColorStop(i / 10, this.colorT(interpola(this.perfil, this.a + ((this.b - this.a) * i) / 10), 1));
    c.strokeStyle = grad;
    c.beginPath();
    this.perfil.forEach(([s, T], i) => (i ? c.lineTo(e.X(s), e.Y(T)) : c.moveTo(e.X(s), e.Y(T))));
    c.stroke();
    c.restore();
    this.texto(num(this.T1, 4), e.x - 6, e.Y(this.T1) + 5, { tam: 14, alin: 'right', color: 'caliente' });
    if (!oculta) this.texto(num(this.Tn, 4), e.x + e.w + 6, e.Y(this.Tn) + 5, { tam: 14, color: 'frio' });
    const r = this.rMarcado ?? (this.foco === 'punto' ? this.x0 : null);
    if (r !== null && r !== undefined && !oculta) {
      const T = interpola(this.perfil, r);
      this.linea(e.X(r), e.y + e.h, e.X(r), e.Y(T), { color: 'amarillo', grosor: 2, guion: [5, 4] });
      this.linea(e.x, e.Y(T), e.X(r), e.Y(T), { color: 'amarillo', grosor: 2, guion: [5, 4] });
      this.circulo(e.X(r), e.Y(T), 6, { relleno: 'amarillo', borde: 'tinta' });
      this.texto(`${num(T, 4)} ${this.uT}`, e.X(r) + 9, e.Y(T) - 9, { tam: 16, negrita: true, halo: true, color: 'naranja' });
    }
  }
}
