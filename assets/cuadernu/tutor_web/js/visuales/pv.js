/**
 * Diagrama p–V del problema REAL (con sus estados y procesos), pistón sincronizado y barras
 * de Q, W y ΔU. El punto recorre el proceso que se está explicando.
 */

import { alfa, color, Ejes, limitar, num, suave, Visual } from './base.js';

const COLOR = { isotermico: 'naranja', adiabatico: 'morado', isobarico: 'azul', isocorico: 'rojo', recta: 'verde' };
const NOMBRE = { isotermico: 'isotérmico', adiabatico: 'adiabático', isobarico: 'isobárico', isocorico: 'isocórico', recta: 'lineal' };

export class VisualPV extends Visual {
  constructor(spec) {
    super({ W: 560, H: 452 });
    this.spec = spec;
    this.estados = spec.estados;
    this.procesos = spec.procesos;
    this.g = spec.gamma || 1.4;
    this.valores = spec.valores || {};
    this.manual = null;
    this.cerrado = this.procesos.length > 1 && this.procesos[this.procesos.length - 1][1] === this.procesos[0][0];
    const vs = []; const ps = [];
    for (const pr of this.procesos) for (let i = 0; i <= 30; i += 1) { const [v, p] = this.punto(pr, i / 30); vs.push(v); ps.push(p); }
    for (const e of Object.values(this.estados)) { vs.push(e[0]); ps.push(e[1]); }
    const r = (a, b) => [Math.min(...a, b), Math.max(...a)];
    let [v0, v1] = r(vs, Infinity); let [p0, p1] = r(ps, Infinity);
    const cv = v0 / v1 > 0.45; const cp = p0 / p1 > 0.45;
    const dv = (v1 - v0) || v1 * 0.5; const dp = (p1 - p0) || p1 * 0.5;
    this.corteX = cv; this.corteY = cp;
    this.e = new Ejes(this, {
      x: 64, y: 34, w: 330, h: 268,
      x0: cv ? Math.max(0, v0 - dv * 0.42) : 0, x1: v1 + dv * 0.16,
      y0: cp ? Math.max(0, p0 - dp * 0.42) : 0, y1: p1 + dp * 0.14,
      etX: `V (${spec.uV || 'm³'})`, etY: `p (${spec.up || 'Pa'})`,
    });
    const lista = this.procesos.map((p) => `${p[0]}→${p[1]} ${NOMBRE[p[2]] || ''}`).join(', ');
    this.describe(`Diagrama presión volumen. Procesos: ${lista}`);
    this.sl = this.deslizador({
      etiqueta: 'Mover el punto', min: 0, max: this.procesos.length, paso: 0.005, valor: 0,
      formato: (x) => { const i = Math.min(this.procesos.length - 1, Math.floor(x)); return `${this.procesos[i][0]}→${this.procesos[i][1]}`; },
      alMover: (x) => { this.manual = x; },
    });
    this.sl.input.addEventListener('change', () => { setTimeout(() => { this.manual = null; }, 3500); });
  }

  punto([a, b, tipo], t) {
    const [va, pa] = this.estados[a];
    const [vb, pb] = this.estados[b];
    const v = va + (vb - va) * t;
    switch (tipo) {
      case 'isotermico': return [v, (pa * va) / v];
      case 'adiabatico': return [v, pa * (va / v) ** this.g];
      case 'isocorico': return [va, pa + (pb - pa) * t];
      default: return [v, pa + (pb - pa) * t];
    }
  }

  trazo(pr, hasta = 1, op = {}) {
    const c = this.ctx;
    const e = this.e;
    c.save();
    c.globalAlpha = op.a ?? 1;
    c.strokeStyle = color(op.color || COLOR[pr[2]] || 'azul');
    c.lineWidth = op.grosor || 3;
    c.beginPath();
    const N = 48;
    const fin = Math.max(1, Math.round(N * hasta));
    for (let i = 0; i <= fin; i += 1) {
      const [v, p] = this.punto(pr, i / N);
      if (i) c.lineTo(e.X(v), e.Y(p)); else c.moveTo(e.X(v), e.Y(p));
    }
    c.stroke();
    c.restore();
    if (hasta >= 1) {
      const [v1, p1] = this.punto(pr, 0.52);
      const [v2, p2] = this.punto(pr, 0.56);
      const x1 = e.X(v1); const y1 = e.Y(p1);
      const dx = e.X(v2) - x1; const dy = e.Y(p2) - y1;
      const L = Math.hypot(dx, dy) || 1;
      this.flecha(x1 - (dx / L) * 4, y1 - (dy / L) * 4, x1 + (dx / L) * 8, y1 + (dy / L) * 8, { color: op.color || COLOR[pr[2]], grosor: op.grosor || 3, punta: 11, a: op.a ?? 1 });
    }
  }

  sombra(pr, hasta, rel) {
    const e = this.e;
    const pts = [];
    const N = 40;
    for (let i = 0; i <= N; i += 1) { const [v, p] = this.punto(pr, (i / N) * hasta); pts.push([e.X(v), e.Y(p)]); }
    const yb = e.y + e.h;
    this.poligono([[pts[0][0], yb], ...pts, [pts[pts.length - 1][0], yb]], { relleno: rel });
  }

  piston(v, p, foco) {
    const x = 432; const y = 46; const w = 92; const h = 170;
    const vs = Object.values(this.estados).map((q) => q[0]);
    const vmax = Math.max(...vs);
    const vmin = Math.min(...vs);
    const f = vmax > vmin ? 0.28 + 0.6 * ((v - vmin) / (vmax - vmin)) : 0.6;
    const ts = Object.values(this.estados).map((q) => q[0] * q[1]);
    const tn = Math.max(...ts) > Math.min(...ts) ? (v * p - Math.min(...ts)) / (Math.max(...ts) - Math.min(...ts)) : 0.5;
    const hg = h * f;
    const yb = y + h;
    this.caja(x, yb - hg, w, hg, { r: 0, relleno: alfa(tn > 0.5 ? 'caliente' : 'frio', 0.14 + 0.3 * Math.abs(tn - 0.5) * 2) });
    const c = this.ctx;
    const N = 22;
    for (let i = 0; i < N; i += 1) {
      const s1 = Math.sin(i * 12.9898) * 43758.5453; const s2 = Math.sin(i * 78.233) * 12543.123;
      const vel = 0.35 + 0.9 * Math.sqrt(0.25 + tn);
      const fx = Math.abs((((s1 - Math.floor(s1)) + this.t * vel * (0.3 + 0.2 * ((i % 5) / 5))) % 2) - 1);
      const fy = Math.abs((((s2 - Math.floor(s2)) + this.t * vel * (0.23 + 0.2 * ((i % 7) / 7))) % 2) - 1);
      c.beginPath();
      c.arc(x + 6 + fx * (w - 12), yb - 6 - fy * Math.max(4, hg - 14), 2.6, 0, 7);
      c.fillStyle = color(tn > 0.5 ? 'caliente' : 'frio');
      c.fill();
    }
    this.linea(x, y, x, yb, { grosor: 3 }); this.linea(x, yb, x + w, yb, { grosor: 3 }); this.linea(x + w, yb, x + w, y, { grosor: 3 });
    this.caja(x + 3, yb - hg - 12, w - 6, 12, { r: 2, relleno: alfa('gris', 0.5), borde: 'tinta', grosor: 2 });
    this.linea(x + w / 2, yb - hg - 12, x + w / 2, y - 12, { grosor: 3 });
    const val = this.valores[foco] || null;
    if (val) {
      const pulso = 0.5 + 0.5 * Math.sin(this.t * 4);
      if (Math.abs(val.Q) > 1e-9) {
        const entra = val.Q > 0;
        const y1 = entra ? yb + 40 : yb + 8;
        const y2 = entra ? yb + 8 : yb + 40;
        this.flecha(x + w / 2, y1, x + w / 2, y2, { color: entra ? 'caliente' : 'frio', grosor: 3, ondas: 2, a: 0.6 + 0.4 * pulso });
        this.texto(entra ? 'entra calor' : 'sale calor', x + w / 2, yb + 58, { tam: 14, alin: 'center', color: entra ? 'caliente' : 'frio' });
      } else this.texto('sin calor', x + w / 2, yb + 26, { tam: 14, alin: 'center', color: 'tinta2' });
      if (Math.abs(val.W) > 1e-9) {
        const exp = val.W > 0;
        const yy = yb - hg - 34;
        this.flecha(x + w + 16, exp ? yy + 34 : yy - 2, x + w + 16, exp ? yy - 2 : yy + 34, { color: 'verde', grosor: 3, a: 0.6 + 0.4 * pulso });
      }
    }
  }

  barras(val) {
    const items = [['Q', 'caliente'], ['W', 'verde'], ['ΔU', 'morado']];
    const tope = Math.max(1e-9, ...Object.values(this.valores).flatMap((x) => [Math.abs(x.Q), Math.abs(x.W), Math.abs(x.U)]));
    const x0 = 250;
    items.forEach(([n, col], i) => {
      const x = val[n === 'ΔU' ? 'U' : n];
      const y = 362 + i * 29;
      const w = (x / tope) * 178 * suave(this.fase(1.1));
      this.linea(x0, y - 10, x0, y + 13, { color: 'borde', grosor: 1.5 });
      this.caja(Math.min(x0, x0 + w), y - 8, Math.max(2, Math.abs(w)), 18, { r: 4, relleno: col, a: 0.88 });
      this.texto(n, 26, y + 6, { tam: 17, mate: true, color: col, negrita: true });
      const s = Math.abs(x) < 1e-6 ? '0' : `${x > 0 ? '+' : ''}${num(x, 4)} J`;
      this.texto(s, x0 + (x >= 0 ? Math.max(0, w) + 8 : 8), y + 6, { tam: 15, color: 'tinta', negrita: true });
    });
  }

  tablaBarras() {
    const ks = Object.keys(this.valores);
    const tope = Math.max(1e-9, ...ks.flatMap((k) => [Math.abs(this.valores[k].Q), Math.abs(this.valores[k].W)]));
    const n = ks.length;
    const w = 500 / n;
    const y0 = 392;
    this.linea(28, y0, 532, y0, { color: 'borde', grosor: 1.5 });
    ks.forEach((k, i) => {
      const x = 32 + i * w;
      [['Q', 'caliente'], ['W', 'verde'], ['U', 'morado']].forEach(([q, col], j) => {
        const alt = (this.valores[k][q] / tope) * 36 * suave(this.fase(1));
        const bw = Math.min(17, w / 4.4);
        this.caja(x + j * (bw + 3) + (w - 3 * bw - 6) / 2, alt >= 0 ? y0 - alt : y0, bw, Math.max(1.5, Math.abs(alt)), { r: 2, relleno: col });
      });
      this.texto(`${k[0]}→${k[1]}`, x + w / 2, 446, { tam: 14, alin: 'center', color: 'tinta2' });
    });
    [['Q', 'caliente'], ['W', 'verde'], ['ΔU', 'morado']].forEach(([s, col], i) => this.texto(s, 452 + i * 34, 352, { tam: 14, color: col, negrita: true, mate: true }));
  }

  dibujar() {
    const e = this.e;
    const f = this.foco;
    const clave = typeof f === 'string' ? f : '';
    const proc = this.procesos.find((p) => p[0] + p[1] === clave);
    const estado = this.estados[clave] ? clave : null;
    e.dibujar({ tam: 17, corteX: this.corteX, corteY: this.corteY });

    if (f === 'neto' || (f === 'ciclo' && this.cerrado)) {
      const pts = [];
      for (const pr of this.procesos) for (let i = 0; i <= 24; i += 1) { const [v, p] = this.punto(pr, i / 24); pts.push([e.X(v), e.Y(p)]); }
      this.poligono(pts, { relleno: alfa('verde', f === 'neto' ? 0.1 + 0.2 * suave(this.fase(1)) : 0.07) });
    }
    // Isotermas de apoyo
    const iso = this.procesos.filter((p) => p[2] === 'isotermico');
    const dib = f === 'ciclo' && !this.yaDibujado ? suave(this.fase(2.2)) : 1;
    if (dib >= 1) this.yaDibujado = true;
    const total = this.procesos.length;
    this.procesos.forEach((pr, i) => {
      const enFoco = proc === pr;
      const atenua = (proc && !enFoco) || (estado && true);
      const h = limitar(dib * total - i);
      if (h <= 0) return;
      this.trazo(pr, h, { a: atenua ? 0.28 : 1, grosor: enFoco ? 4.6 : 3 });
    });
    if (iso.length === 0) { /* nada */ }

    if (proc) {
      const u = this.manual !== null ? null : suave(((this.tFoco % 3.6) / 2.6));
      const k = this.animado() ? u : 1;
      const val = this.valores[clave];
      if (val && Math.abs(val.W) > 1e-9) this.sombra(proc, k ?? 1, alfa(val.W > 0 ? 'azul' : 'rojo', 0.2));
      const [v, p] = this.punto(proc, k ?? 1);
      this.circulo(e.X(v), e.Y(p), 8, { relleno: 'amarillo', borde: 'tinta', grosor: 2 });
      this.piston(v, p, clave);
      const [vm, pm] = this.punto(proc, 0.5);
      this.texto(NOMBRE[proc[2]] || '', e.X(vm) + (proc[2] === 'isocorico' ? 12 : 10), e.Y(pm) - 12, { tam: 17, color: COLOR[proc[2]], negrita: true, halo: true });
      if (val) this.barras(val);
    } else {
      let idx = 0; let tt = 0;
      const per = 2.1;
      if (this.manual !== null) { idx = Math.min(total - 1, Math.floor(this.manual)); tt = this.manual - idx; }
      else if (f === 'ciclo' || f === 'neto' || f === 'barras' || f === null) {
        const s = this.animado() ? (this.t % (total * per)) / per : 0;
        idx = Math.min(total - 1, Math.floor(s)); tt = suave(Math.min(1, (s - idx) * 1.25));
      }
      const [v, p] = estado ? this.estados[estado] : this.punto(this.procesos[idx], tt);
      if (f !== 'estados') this.circulo(e.X(v), e.Y(p), estado ? 9 : 7, { relleno: 'amarillo', borde: 'tinta', grosor: 2 });
      this.piston(v, p, estado ? '' : this.procesos[idx][0] + this.procesos[idx][1]);
      if (f === 'barras' || f === 'neto') this.tablaBarras();
      else if (!estado) this.texto('Arrastra el control para recorrer el ciclo', 280, 384, { tam: 15, alin: 'center', color: 'tinta3' });
    }

    if (this.manual !== null) {
      const i = Math.min(total - 1, Math.floor(this.manual));
      const [v, p] = this.punto(this.procesos[i], this.manual - i);
      this.circulo(e.X(v), e.Y(p), 8, { relleno: 'amarillo', borde: 'tinta' });
      this.texto(`V = ${num(v, 3)} ${this.spec.uV || ''}   p = ${num(p, 3)} ${this.spec.up || ''}`, 70, 344, { tam: 15, color: 'tinta2' });
    }

    // Estados con su letra; líneas guía si hace falta
    for (const [k, [v, p]] of Object.entries(this.estados)) {
      const x = e.X(v); const y = e.Y(p);
      const guia = f === 'estados' || estado === k;
      if (guia) {
        this.linea(x, y, x, e.y + e.h, { color: 'tinta3', grosor: 1.2, guion: [4, 4] });
        this.linea(e.x, y, x, y, { color: 'tinta3', grosor: 1.2, guion: [4, 4] });
        if (estado === k) {
          this.texto(num(v, 3), x, e.y + e.h + 17, { tam: 14, alin: 'center', color: 'azul', halo: true });
          this.texto(num(p, 3), e.x - 5, y - 6, { tam: 14, alin: 'right', color: 'azul', halo: true });
        }
      }
      this.circulo(x, y, 5, { relleno: 'tinta', borde: null });
      const cx = e.x + e.w / 2;
      this.texto(k, x + (x > cx ? 10 : -10), y - 9, { tam: 21, mate: true, negrita: true, alin: x > cx ? 'left' : 'right', halo: true });
    }
  }
}
