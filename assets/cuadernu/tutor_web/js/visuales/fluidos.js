/**
 * Fuerza del agua sobre una pared: de la geometría a la integral.
 *
 * Se ve la pared de frente con el agua detrás. Una franja horizontal delgada, a profundidad h,
 * tiene ancho L(h) y alto dh; en ella toda el agua está a la misma profundidad, así que la presión
 * es una sola: p = ρgh. Con «Franjas para sumar» la pared se parte en n franjas y se suma
 * p·L·Δh de cada una (con la presión del borde de arriba): la suma se acerca a la integral exacta.
 *
 * spec: { tipo: 'pared', forma: 'rectangulo' | 'trapecio' | 'parabola' | 'mixta' | 'ventana',
 *         H (profundidad del agua hasta el fondo, m), ancho (rectángulo o ventana, m),
 *         ancho_sup / ancho_inf (trapecio, m), k (parábola y = k·x², con y desde el fondo),
 *         rect (ancho de la mitad rectangular en 'mixta'), y_inf / y_sup (ventana, m sobre el fondo),
 *         rho (kg/m³, por defecto 1000), titulo }
 */
import { Visual, alfa, num } from './base.js';

const G = 9.8;

export class VisualPared extends Visual {
  constructor(spec) {
    super({ W: 560, H: 420 });
    this.s = spec;
    this.rho = Number(spec.rho || 1000);
    this.Hagua = Math.max(0.01, Number(spec.H || 4));
    const forma = String(spec.forma || 'rectangulo');
    this.forma = forma;
    // Rango de profundidades que ocupa la pared (desde la superficie hacia abajo)
    if (forma === 'ventana') {
      const yi = Number(spec.y_inf ?? 0); const ys = Number(spec.y_sup ?? this.Hagua);
      this.h0 = Math.max(0, this.Hagua - ys);
      this.h1 = Math.max(this.h0 + 0.001, this.Hagua - yi);
    } else {
      this.h0 = 0;
      this.h1 = this.Hagua;
    }
    // Ancho de la franja a profundidad h
    const a = Number(spec.ancho || 1);
    const k = Number(spec.k || 2);
    if (forma === 'trapecio') {
      const bs = Number(spec.ancho_sup || 2); const bi = Number(spec.ancho_inf || 1);
      this.L = (h) => bs + ((bi - bs) * h) / this.Hagua;
      this.medio = (h) => [-this.L(h) / 2, this.L(h) / 2];
    } else if (forma === 'parabola') {
      this.L = (h) => 2 * Math.sqrt(Math.max(0, this.Hagua - h) / k);
      this.medio = (h) => [-this.L(h) / 2, this.L(h) / 2];
    } else if (forma === 'mixta') {
      const r = Number(spec.rect || 1);
      this.L = (h) => r + Math.sqrt(Math.max(0, this.Hagua - h) / k);
      this.medio = (h) => [-r, Math.sqrt(Math.max(0, this.Hagua - h) / k)];
    } else {
      this.L = () => a;
      this.medio = () => [-a / 2, a / 2];
    }
    this.maxL = 0;
    this.izq = 0;
    this.der = 0;
    for (let i = 0; i <= 200; i += 1) {
      const h = this.h0 + ((this.h1 - this.h0) * i) / 200;
      const [x0, x1] = this.medio(h);
      this.izq = Math.min(this.izq, x0);
      this.der = Math.max(this.der, x1);
      this.maxL = Math.max(this.maxL, this.L(h));
    }
    this.exacto = this._integral();
    this.h = this.h0 + (this.h1 - this.h0) * 0.6;
    this.modo = 'franja';
    this.n = 4;
    const dh = (this.h1 - this.h0);
    this.deslizador({
      etiqueta: 'Profundidad de la franja h', min: this.h0, max: this.h1 - dh / 60, paso: dh / 300, valor: this.h,
      formato: (v) => `${num(v, 3)} m`, alMover: (v) => { this.h = v; this.modo = 'franja'; },
    });
    this.deslizador({
      etiqueta: 'Franjas para sumar', min: 1, max: 40, paso: 1, valor: this.n,
      formato: (v) => String(v), alMover: (v) => { this.n = v; this.modo = 'suma'; },
    });
    this.describe('Pared con agua: una franja horizontal a profundidad h, su ancho L(h) y su alto dh; la suma de franjas se acerca a la fuerza total.');
  }

  animado() { return false; }

  /** F = ∫ ρ g h L(h) dh con Simpson (para el valor exacto que se muestra). */
  _integral() {
    const N = 400; const a = this.h0; const b = this.h1; const d = (b - a) / N;
    const f = (h) => this.rho * G * h * this.L(h);
    let s = f(a) + f(b);
    for (let i = 1; i < N; i += 1) s += f(a + i * d) * (i % 2 ? 4 : 2);
    return (s * d) / 3;
  }

  // Del mundo (metros) al lienzo
  X(x) { return 150 + ((x - this.izq) / ((this.der - this.izq) || 1)) * 230; }
  Y(h) { return 92 + ((h - this.h0) / ((this.h1 - this.h0) || 1)) * 270; }

  _contorno() {
    const pts = [];
    const N = 80;
    for (let i = 0; i <= N; i += 1) { const h = this.h0 + ((this.h1 - this.h0) * i) / N; pts.push([this.X(this.medio(h)[1]), this.Y(h)]); }
    for (let i = N; i >= 0; i -= 1) { const h = this.h0 + ((this.h1 - this.h0) * i) / N; pts.push([this.X(this.medio(h)[0]), this.Y(h)]); }
    return pts;
  }

  dibujar() {
    const s = this.s;
    if (s.titulo) this.texto(s.titulo, 20, 28, { tam: 17, negrita: true });
    // Agua y superficie (si la pared empieza en la superficie)
    this.caja(130, 92, 270, 270, { r: 0, relleno: alfa('azul', 0.08) });
    if (this.h0 === 0) {
      this.linea(122, 92, 408, 92, { color: 'azul', grosor: 2.2 });
      this.texto('superficie: h = 0', 130, 82, { tam: 13, color: 'tinta3' });
    } else {
      this.texto(`el agua sigue ${num(this.h0, 3)} m más arriba`, 130, 82, { tam: 13, color: 'tinta3' });
    }
    // Pared
    this.poligono(this._contorno(), { relleno: alfa('tinta', 0.06), borde: 'tinta', grosor: 2 });
    // Eje de profundidad
    this.flecha(108, 92, 108, 370, { color: 'tinta2', grosor: 1.6, punta: 8 });
    this.texto('h', 96, 388, { tam: 16, mate: true });
    this.texto(num(this.h0, 3), 100, 96, { tam: 12, alin: 'right', color: 'tinta3' });
    this.texto(num(this.h1, 3), 100, 366, { tam: 12, alin: 'right', color: 'tinta3' });

    if (this.modo === 'suma') this._dibujarSuma(); else this._dibujarFranja();
    this.texto('dibujo sin escala', 548, 410, { tam: 11, alin: 'right', color: 'tinta3' });
  }

  _dibujarFranja() {
    const h = this.h;
    const dh = (this.h1 - this.h0) / 24;
    const [x0, x1] = this.medio(h);
    const y0 = this.Y(h); const y1 = this.Y(Math.min(this.h1, h + dh));
    this.caja(this.X(x0), y0, this.X(x1) - this.X(x0), Math.max(3, y1 - y0), { r: 0, relleno: alfa('naranja', 0.55), borde: 'naranja', grosor: 1.4 });
    // Ancho L(h)
    this.linea(this.X(x0), y0 - 7, this.X(x1), y0 - 7, { color: 'naranja', grosor: 1.4 });
    this.texto(`L(h) = ${num(this.L(h), 3)} m`, (this.X(x0) + this.X(x1)) / 2, y0 - 12, { tam: 14, alin: 'center', color: 'naranja', halo: true });
    this.texto('dh', this.X(x1) + 8, (y0 + y1) / 2 + 5, { tam: 14, color: 'naranja', mate: true });
    // Profundidad
    this.linea(118, this.Y(this.h0), 118, y0, { color: 'naranja', grosor: 1.2, guion: [4, 3] });
    this.texto(`h = ${num(h, 3)} m`, 118, y0 + 18, { tam: 13, color: 'tinta2' });
    // Presión y fuerza de la franja
    const p = this.rho * G * h;
    const x = 418;
    this.texto('En esta franja:', x, 120, { tam: 14, negrita: true });
    this.texto('p = ρgh', x, 146, { tam: 15, mate: true });
    this.texto(`= ${num(p, 4)} Pa`, x, 168, { tam: 14, color: 'azul' });
    this.texto('dA = L(h)·dh', x, 200, { tam: 15, mate: true });
    this.texto('dF = p·L(h)·dh', x, 232, { tam: 15, mate: true, color: 'naranja' });
    this.texto('Más hondo,', x, 272, { tam: 13, color: 'tinta3' });
    this.texto('más presión.', x, 290, { tam: 13, color: 'tinta3' });
    // Flechas de presión que crecen con la profundidad
    for (const f of [0.15, 0.5, 0.85]) {
      const hh = this.h0 + (this.h1 - this.h0) * f;
      const largo = 10 + 46 * ((hh) / (this.h1 || 1));
      const yy = this.Y(hh);
      this.flecha(this.X(this.medio(hh)[0]) - largo - 6, yy, this.X(this.medio(hh)[0]) - 4, yy, { color: 'azul', grosor: 2, punta: 7, a: 0.8 });
    }
  }

  _dibujarSuma() {
    const n = this.n;
    const d = (this.h1 - this.h0) / n;
    let suma = 0;
    for (let i = 0; i < n; i += 1) {
      const h = this.h0 + i * d;
      const L = this.L(h + d / 2);
      const [x0, x1] = this.medio(h + d / 2);
      suma += this.rho * G * h * L * d; // presión del borde de arriba de la franja
      this.caja(this.X(x0), this.Y(h), this.X(x1) - this.X(x0), this.Y(h + d) - this.Y(h), {
        r: 0, relleno: alfa('naranja', 0.16 + 0.5 * (i / Math.max(1, n - 1))), borde: 'naranja', grosor: 1,
      });
    }
    const x = 418;
    this.texto(`${n} franja${n === 1 ? '' : 's'}:`, x, 120, { tam: 14, negrita: true });
    this.texto('F ≈ suma de p·L·Δh', x, 146, { tam: 14, mate: true });
    this.texto(`≈ ${num(suma, 4)} N`, x, 170, { tam: 15, color: 'naranja', negrita: true });
    this.texto('Integral exacta:', x, 212, { tam: 14, negrita: true });
    this.texto(`F = ${num(this.exacto, 4)} N`, x, 236, { tam: 15, color: 'verde', negrita: true });
    this.texto('Más franjas:', x, 276, { tam: 13, color: 'tinta3' });
    this.texto('la suma llega', x, 294, { tam: 13, color: 'tinta3' });
    this.texto('al valor exacto.', x, 312, { tam: 13, color: 'tinta3' });
  }
}
