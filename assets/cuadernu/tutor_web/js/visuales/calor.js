/**
 * Calorimetría: quién pide calor, quién puede darlo y qué queda al final.
 * Y la curva de calentamiento del agua, con un punto que la recorre.
 */

import { alfa, color, Ejes, limitar, num, suave, Visual } from './base.js';

const FASE = {
  hielo: { c: 'turquesa', n: 'hielo' }, agua: { c: 'azul', n: 'agua' }, vapor: { c: 'gris', n: 'vapor' },
  solido: { c: 'morado', n: 'recipiente' }, frio: { c: 'frio', n: 'frío' }, caliente: { c: 'caliente', n: 'caliente' },
};

export class VisualCalor extends Visual {
  constructor(spec) {
    super({ W: 560, H: 452 });
    this.s = spec;
    this.etapa = -1;
    this.describe('Balance de calor entre los componentes');
  }

  particulas(x, y, w, h, fase, n, t) {
    const c = this.ctx;
    for (let i = 0; i < n; i += 1) {
      const s1 = Math.sin(i * 12.9898 + 3) * 43758.5; const s2 = Math.sin(i * 78.233 + 5) * 12543.1;
      let px; let py;
      if (fase === 'hielo' || fase === 'solido' || fase === 'frio') {
        const col = Math.ceil(Math.sqrt(n));
        px = x + ((i % col) + 0.5) * (w / col) + Math.sin(t * 6 + i) * 0.9;
        py = y + (Math.floor(i / col) + 0.5) * (h / Math.ceil(n / col)) + Math.cos(t * 6.4 + i) * 0.9;
      } else {
        const v = fase === 'vapor' ? 0.5 : 0.16;
        px = x + 4 + Math.abs((((s1 - Math.floor(s1)) + t * v * (0.6 + (i % 4) / 5)) % 2) - 1) * (w - 8);
        py = y + 4 + Math.abs((((s2 - Math.floor(s2)) + t * v * (0.5 + (i % 5) / 6)) % 2) - 1) * (h - 8);
      }
      c.beginPath();
      c.arc(px, py, 2.7, 0, 7);
      c.fillStyle = color(FASE[fase]?.c || 'azul');
      c.fill();
    }
  }

  dibujar() {
    const { s } = this;
    const comp = s.componentes || [];
    const n = comp.length;
    const w = Math.min(124, 520 / n - 14);
    const x0 = (560 - n * (w + 14) + 14) / 2;
    const maxM = Math.max(...comp.map((k) => k.m));
    const fin = this.foco === 'final';
    comp.forEach((k, i) => {
      const x = x0 + i * (w + 14);
      const h = 44 + 62 * Math.sqrt(k.m / maxM);
      const y = 148 - h;
      const f = FASE[k.fase] || FASE.agua;
      this.caja(x, y, w, h, { r: 8, relleno: alfa(f.c, 0.16), borde: f.c, grosor: 2 });
      this.particulas(x, y, w, h, k.fase, Math.round(8 + 14 * (k.m / maxM)), this.t);
      this.texto(k.n, x + w / 2, 168, { tam: 17, alin: 'center', negrita: true, color: f.c });
      this.texto(`${num(k.m, 4)} g · ${num(k.T, 3)} °C`, x + w / 2, 187, { tam: 14, alin: 'center', color: 'tinta2' });
    });
    if (s.Tf !== undefined && ['comparar', 'final', 'listo'].includes(this.foco)) this.texto(`T final = ${num(s.Tf, 4)} °C`, 280, 28, { tam: 19, alin: 'center', negrita: true, color: 'verde' });

    if (fin && s.final) {
      const tot = s.final.reduce((q, z) => q + z.m, 0);
      let x = 30;
      this.texto('Así queda la mezcla', 30, 236, { tam: 17, negrita: true });
      s.final.forEach((z) => {
        const ww = (500 * z.m) / tot * suave(this.fase(1.2));
        const fk = FASE[z.n] || FASE.agua;
        this.caja(x, 250, Math.max(3, ww), 52, { r: 5, relleno: alfa(fk.c, 0.4), borde: fk.c });
        this.texto(`${z.n}: ${num(z.m, 4)} g`, x + 8, 330, { tam: 18, color: fk.c, negrita: true });
        x += Math.max(160, ww + 4);
      });
      return;
    }
    const nec = s.necesita || [];
    const dis = s.disponible || [];
    const tn = nec.reduce((q, z) => q + Math.abs(z.Q), 0);
    const td = dis.reduce((q, z) => q + Math.abs(z.Q), 0);
    const tope = Math.max(tn, td, 1);
    this.texto('Calor que se PIDE', 30, 226, { tam: 16, color: 'frio', negrita: true });
    let y = 236;
    const alto = Math.min(26, 96 / Math.max(1, nec.length));
    const todas = this.foco === 'comparar' || this.foco === 'listo';
    let acum = 0;
    nec.forEach((z, i) => {
      const activa = todas || this.etapa < 0 || i <= this.etapa;
      const k = activa ? (i === this.etapa && !todas ? suave(this.fase(1)) : 1) : 0;
      const ww = Math.max(2, (500 * Math.abs(z.Q)) / tope) * k;
      this.caja(30, y, ww, alto - 5, { r: 4, relleno: i === this.etapa && !todas ? 'frio' : alfa('frio', 0.45) });
      this.texto(`${z.n}`, 36 + (ww > 260 ? 0 : ww), y + alto - 10, { tam: 14, color: activa ? 'tinta' : 'tinta3', halo: ww <= 260 });
      this.texto(activa && k > 0.9 ? `${num(Math.abs(z.Q), 4)} J` : '', 548, y + alto - 10, { tam: 14, alin: 'right', negrita: true, halo: true });
      y += alto;
      acum += Math.abs(z.Q);
    });
    this.linea(30, y + 4, 548, y + 4, { color: 'borde', grosor: 1.2 });
    if (todas) this.texto(`total: ${num(acum, 4)} J`, 548, y + 22, { tam: 16, alin: 'right', color: 'frio', negrita: true });
    y += 48;
    this.texto(s.tDisp || 'Calor que se PUEDE DAR', 30, y, { tam: 16, color: 'caliente', negrita: true });
    dis.forEach((z) => {
      const k = todas ? suave(this.fase(1.2)) : this.foco === 'flujo' ? 0.0 : 0;
      const ww = Math.max(2, (500 * Math.abs(z.Q)) / tope) * k;
      this.caja(30, y + 10, Math.max(2, ww), 26, { r: 4, relleno: alfa('caliente', 0.5) });
      this.texto(z.n, 36, y + 28, { tam: 14, halo: true });
      if (todas) this.texto(`${num(Math.abs(z.Q), 4)} J`, 548, y + 28, { tam: 15, alin: 'right', color: 'caliente', negrita: true, halo: true });
      y += 36;
    });
    if (todas && td > 0 && tn > 0) {
      const sobra = td > tn * 1.0001;
      const igual = Math.abs(td - tn) < tn * 0.002;
      this.texto(igual ? 'Lo que uno cede, los otros lo ganan' : sobra ? 'Sobra quien da calor' : 'No alcanza', 30, Math.min(444, y + 30), { tam: 18, negrita: true, color: igual ? 'verde' : sobra ? 'caliente' : 'frio' });
    }
  }
}

export class VisualCalentamiento extends Visual {
  /** spec: {m, Ti, Tf, ch, ca, cv, Lf, Lv, marca?} — curva T contra Q del agua. */
  constructor(spec) {
    super({ W: 560, H: 400 });
    this.s = spec;
    const { m = 1, ch = 2090, ca = 4190, cv = 2010, Lf = 3.34e5, Lv = 2.256e6 } = spec;
    const Ti = spec.Ti ?? -20; const Tf = spec.Tf ?? 120;
    const pts = [[0, Ti]];
    let Q = 0;
    const nombres = [];
    const sube = (hasta, c, n) => { if (pts[pts.length - 1][1] < hasta) { Q += m * c * (hasta - pts[pts.length - 1][1]); pts.push([Q, hasta]); nombres.push(n); } };
    const plano = (L, n) => { Q += m * L; pts.push([Q, pts[pts.length - 1][1]]); nombres.push(n); };
    if (Ti < 0) sube(Math.min(0, Tf), ch, 'hielo');
    if (Tf >= 0 && Ti <= 0) plano(Lf, 'se funde');
    sube(Math.min(100, Tf), ca, 'agua');
    if (Tf >= 100) { plano(Lv, 'hierve'); sube(Tf, cv, 'vapor'); }
    this.pts = pts;
    this.nombres = nombres;
    const Qmax = pts[pts.length - 1][0];
    // El hervor es enorme: se comprime el eje de calor para que todo se vea
    this.fq = (q) => Math.sqrt(q / Qmax);
    this.e = new Ejes(this, { x: 66, y: 40, w: 440, h: 284, x0: 0, x1: 1, y0: Math.min(-10, Ti - 8), y1: Math.max(112, Tf + 8), etX: 'Q (calor)', etY: 'T (°C)' });
    this.u = null;
    this.deslizador({ etiqueta: 'Calor entregado', min: 0, max: 1, paso: 0.002, valor: 0, formato: (v) => `${num(Qmax * v * v, 3)} J`, alMover: (v) => { this.u = v; } });
    this.describe('Curva de calentamiento del agua: hielo, fusión, agua, ebullición y vapor');
    this.etapa = -1;
  }

  dibujar() {
    const { e } = this;
    e.dibujar();
    for (const T of [0, 100]) { this.linea(e.x, e.Y(T), e.x + e.w, e.Y(T), { color: 'borde', grosor: 1.2, guion: [4, 5] }); this.texto(`${T}`, e.x - 8, e.Y(T) + 5, { tam: 15, alin: 'right', color: 'tinta2' }); }
    const cols = ['turquesa', 'frio', 'azul', 'naranja', 'gris'];
    const base = { hielo: 'turquesa', 'se funde': 'frio', agua: 'azul', hierve: 'naranja', vapor: 'gris' };
    for (let i = 1; i < this.pts.length; i += 1) {
      const [q0, t0] = this.pts[i - 1]; const [q1, t1] = this.pts[i];
      const act = this.etapa === i - 1;
      const c = this.ctx;
      c.save();
      c.globalAlpha = this.etapa >= 0 && !act ? 0.4 : 1;
      c.strokeStyle = color(base[this.nombres[i - 1]] || cols[i - 1]);
      c.lineWidth = act ? 6 : 4;
      c.beginPath();
      for (let k = 0; k <= 20; k += 1) { const q = q0 + ((q1 - q0) * k) / 20; const t = t0 + ((t1 - t0) * k) / 20; if (k) c.lineTo(e.X(this.fq(q)), e.Y(t)); else c.moveTo(e.X(this.fq(q)), e.Y(t)); }
      c.stroke();
      c.restore();
      const xm = e.X((this.fq(q0) + this.fq(q1)) / 2);
      const ym = e.Y((t0 + t1) / 2);
      this.texto(this.nombres[i - 1], xm, ym - 14, { tam: 16, alin: 'center', color: base[this.nombres[i - 1]], negrita: act, halo: true });
    }
    const Qmax = this.pts[this.pts.length - 1][0];
    const u = this.u !== null ? this.u : this.animado() ? (this.t * 0.09) % 1 : 1;
    const q = Qmax * u * u;
    let T = this.pts[0][1];
    for (let i = 1; i < this.pts.length; i += 1) if (q >= this.pts[i - 1][0]) { const [q0, t0] = this.pts[i - 1]; const [q1, t1] = this.pts[i]; T = t0 + (t1 - t0) * limitar((q - q0) / (q1 - q0 || 1)); }
    this.circulo(e.X(u), e.Y(T), 8, { relleno: 'amarillo', borde: 'tinta' });
    this.texto(`${num(T, 3)} °C`, e.X(u) + 12, e.Y(T) + 20, { tam: 16, negrita: true, halo: true });
    this.texto('En los tramos planos la temperatura no sube: cambia de fase', 280, 388, { tam: 14.5, alin: 'center', color: 'tinta3' });
  }
}
