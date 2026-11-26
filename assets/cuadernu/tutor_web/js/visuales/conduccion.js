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
    this.esferaSec = geo === 'esfera_seccionada';
    if (this.radial) {
      const rs = [spec.r1, spec.r2, spec.r3].filter((x) => x !== undefined);
      this.a = rs[0];
      this.b = rs[rs.length - 1];
    } else if (this.esferaSec) {
      this.a = -(spec.L ?? 0.06);
      this.b = spec.L ?? 0.06;
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
    // Armar la integral paso a paso: capas, una capa, Fourier, separar y sumar (botones o foco "armar1".."armar5")
    this.armado = 0;
    this._capa = null;
    if (this.radial || geo === 'pared' || this.esferaSec) {
      this.bArmar = this.boton('Cómo se arma la integral', () => this._armar(this.armado ? 0 : 1));
      this.bAnt = this.boton('◀ Paso anterior', () => this._armar(Math.max(1, this.armado - 1)));
      this.bSig = this.boton('Paso siguiente ▶', () => this._armar(Math.min(5, this.armado + 1)));
      this.bArmar.setAttribute('aria-pressed', 'false');
      this.bAnt.hidden = true;
      this.bSig.hidden = true;
    }
    this.describe(`Conducción de calor, geometría ${geo}. De ${num(this.T1)} a ${num(this.Tn)} ${this.uT}`);
  }

  /* ---------------------------------------------------------------- armar la integral */

  _armar(n) {
    this.armado = n;
    this.tFoco = 0;
    if (this.bArmar) {
      this.bArmar.textContent = n ? 'Salir de la integral' : 'Cómo se arma la integral';
      this.bArmar.setAttribute('aria-pressed', n ? 'true' : 'false');
      this.bAnt.hidden = !n || n <= 1;
      this.bSig.hidden = !n || n >= 5;
    }
    this.sucio = true;
    this._pedir();
  }

  _etapa() {
    if (this.armado) return this.armado;
    const m = /^armar([1-5])$/.exec(String(this.foco ?? ''));
    return m ? Number(m[1]) : 0;
  }

  _nCapas(etapa) {
    if (etapa === 1) {
      const serie = [3, 5, 8, 12, 18, 26];
      return this.animado() ? serie[Math.floor(this.tFoco / 1.1) % serie.length] : serie[serie.length - 1];
    }
    return etapa === 5 || this.foco === 'barrido' ? 16 : 10;
  }

  _capaResaltada(N) {
    const u = ((this.x ?? this.x0) - this.a) / (this.b - this.a);
    return Math.min(N - 1, Math.max(0, Math.floor(limitar(u) * N)));
  }

  _textos() {
    const g = this.s.geo;
    if (g === 'esfera_seccionada') {
      return {
        v: 'y',
        dos: false,
        capa: 'disco horizontal',
        A: 'A(y) = πx² = π(R² − y²)',
        sep: 'dT = −(H / πk) · dy / (R² − y²)',
        res: 'T₁ − T₂ = (H / πkR) · ln((R + L)/(R − L))',
      };
    }
    const esf = g.startsWith('esfera');
    const cil = g.startsWith('cilindro');
    const forma = Boolean(this.s.forma);
    const dos = this.s.r3 !== undefined;
    const v = this.radial ? 'r' : (this.s.eje || 'x');
    let res;
    if (cil) res = dos ? 'T₁ − T₃ = (H/2πL)·[ln(r₂/r₁)/k₁ + ln(r₃/r₂)/k₂]' : 'T₁ − T₂ = (H/2πkL)·ln(r₂/r₁)';
    else if (esf) res = dos ? 'T₁ − T₃ = (H/4π)·[(1/r₁ − 1/r₂)/k₁ + (1/r₂ − 1/r₃)/k₂]' : 'T₁ − T₂ = (H/4πk)·(1/r₁ − 1/r₂)';
    else res = forma ? `T₁ − T₂ = (H/k)·∫ d${v}/A(${v})` : 'T₁ − T₂ = H·L/(k·A)';
    return {
      v,
      dos,
      capa: this.radial ? (esf ? 'cáscara' : 'tubo') : 'rebanada',
      A: cil ? 'A = 2πrL' : esf ? 'A = 4πr²' : forma ? `A(${v}) cambia con ${v}` : 'A es la misma en todas',
      sep: cil ? 'dT = −(H/k)·dr/(2πrL)' : esf ? 'dT = −(H/k)·dr/(4πr²)' : forma ? `dT = −(H/k)·d${v}/A(${v})` : `dT = −(H/k)·d${v}/A`,
      res,
    };
  }

  _rotulos(etapa) {
    const t = this._textos();
    const d = `d${t.v}`;
    const desc2 = this.esferaSec
      ? 'Por Pitágoras x² + y² = R², cada disco horizontal tiene área:'
      : this.radial
        ? (t.capa === 'cáscara' ? 'Es la cáscara de una pelota; su área es:' : 'Desenrollada es un rectángulo de 2πr por L; su área es:')
        : 'Su área es la cara que cruza el calor:';
    const lim5 = this.esferaSec ? 'Sumando desde −L hasta +L sale la bajada total:' : `Sumando de ${t.v}₁ a ${t.v}${t.dos ? '₃' : '₂'} sale la bajada total:`;
    const lista = [
      null,
      ['1 · Cortamos en capas delgaditas', [`Cada ${t.capa} es tan fina que adentro la temperatura casi no cambia.`, 'Así se puede usar la fórmula de la hoja en cada una.']],
      [this.radial ? `2 · Una capa: radio r, grosor ${d}` : `2 · Una rebanada en ${t.v}, grosor ${d}`, [desc2, t.A]],
      ['3 · Fourier de la hoja en esa capa', [`H = −k·A·dT/${d}`, 'El mismo H cruza todas las capas, una tras otra.']],
      ['4 · Separamos las letras', [t.sep, `Lo que tiene T, a la izquierda; lo que tiene ${t.v}, a la derecha.`]],
      ['5 · Sumamos todas las capas: la integral', ['Cada capa baja un poquito la temperatura: son los escalones de abajo.', lim5, t.res]],
    ];
    return lista[etapa];
  }

  /** Escribe renglones con corte de línea; los que llevan "=" van como fórmula. Devuelve la y final. */
  _parrafo(lineas, x, y, ancho, { tam = 15, color: col = 'tinta2', negrita = false } = {}) {
    const c = this.ctx;
    let yy = y;
    for (const l of lineas) {
      const esF = !negrita && /=/.test(l);
      const t = esF ? tam + 2 : tam;
      c.font = this.fuente(t, { negrita: negrita || esF, mate: esF });
      const salida = [];
      let linea = '';
      for (const p of l.split(' ')) {
        const prueba = linea ? `${linea} ${p}` : p;
        if (linea && c.measureText(prueba.replace(/[_^]/g, '')).width > ancho) { salida.push(linea); linea = p; } else linea = prueba;
      }
      if (linea) salida.push(linea);
      for (const s of salida) {
        this.texto(s, x, yy, { tam: t, color: esF ? 'azul' : col, mate: esF, negrita: negrita || esF, halo: true });
        yy += t * 1.38;
      }
      yy += 3;
    }
    return yy;
  }

  _capasRadiales(cx, cy, etapa) {
    const N = this._nCapas(etapa);
    const rs = Array.from({ length: N + 1 }, (_, i) => this.a + ((this.b - this.a) * i) / N);
    for (let i = 1; i < N; i += 1) this.circulo(cx, cy, this.R(rs[i]), { borde: 'tinta', grosor: 1, a: 0.55 });
    if (etapa >= 2 && etapa <= 4) {
      const i = this._capaResaltada(N);
      const c = this.ctx;
      c.save();
      c.beginPath();
      c.arc(cx, cy, this.R(rs[i + 1]), 0, Math.PI * 2);
      c.arc(cx, cy, this.R(rs[i]), 0, Math.PI * 2, true);
      c.fillStyle = alfa('amarillo', 0.72);
      c.fill('evenodd');
      c.restore();
      this.circulo(cx, cy, this.R(rs[i]), { borde: 'naranja', grosor: 2 });
      this.circulo(cx, cy, this.R(rs[i + 1]), { borde: 'naranja', grosor: 2 });
      const rm = this.R((rs[i] + rs[i + 1]) / 2);
      this.linea(cx, cy, cx + rm * Math.cos(-0.6), cy + rm * Math.sin(-0.6), { color: 'tinta', grosor: 1.6, guion: [5, 4] });
      this.texto('r', cx + rm * 0.5 * Math.cos(-0.6) - 2, cy + rm * 0.5 * Math.sin(-0.6) - 8, { tam: 18, mate: true, halo: true });
      this.texto('dr', cx + (rm + 14) * Math.cos(0.5), cy + (rm + 14) * Math.sin(0.5) + 6, { tam: 18, mate: true, color: 'naranja', negrita: true, halo: true });
      this.rMarcado = (rs[i] + rs[i + 1]) / 2;
      this._capa = [rs[i], rs[i + 1]];
    }
  }

  _capasPlanas(x0, w, yc, alto, forma, etapa) {
    const N = this._nCapas(etapa);
    const h2 = (u) => (alto / 2) * Math.max(0.05, forma(limitar(u)));
    for (let i = 1; i < N; i += 1) {
      const u = i / N;
      const X = x0 + w * u;
      this.linea(X, yc - h2(u), X, yc + h2(u), { color: 'tinta', grosor: 1, a: 0.55 });
    }
    if (etapa >= 2 && etapa <= 4) {
      const i = this._capaResaltada(N);
      const u0 = i / N;
      const u1 = (i + 1) / N;
      const um = (u0 + u1) / 2;
      this.poligono([[x0 + w * u0, yc - h2(u0)], [x0 + w * u1, yc - h2(u1)], [x0 + w * u1, yc + h2(u1)], [x0 + w * u0, yc + h2(u0)]],
        { relleno: alfa('amarillo', 0.72), borde: 'naranja', grosor: 2 });
      this.texto(`d${this.s.eje || 'x'}`, x0 + w * um, yc + h2(um) + 20, { tam: 18, mate: true, alin: 'center', color: 'naranja', negrita: true, halo: true });
      this.texto('A', x0 + w * um, yc - h2(um) - 10, { tam: 18, mate: true, alin: 'center', color: 'azul', negrita: true, halo: true });
      this.rMarcado = this.a + (this.b - this.a) * um;
      this._capa = [this.a + (this.b - this.a) * u0, this.a + (this.b - this.a) * u1];
    }
  }

  /** Abajo: los escalones de temperatura que se van sumando (la integral como suma de bajadas). */
  _escalera(etapa) {
    const { e } = this;
    const N = this._nCapas(5);
    let avance = 1;
    if (etapa === 5) avance = this.animado() ? limitar(this.tFoco / 5.5) : 1;
    else if (this.rMarcado !== null && this.rMarcado !== undefined) avance = (this.rMarcado - this.a) / (this.b - this.a);
    const k = Math.max(1, Math.min(N, Math.round(avance * N)));
    const xs = (i) => this.a + ((this.b - this.a) * i) / N;
    for (let i = 0; i < k; i += 1) {
      const xa = xs(i);
      const xb = xs(i + 1);
      const Ta = interpola(this.perfil, xa);
      const Tb = interpola(this.perfil, xb);
      this.linea(e.X(xa), e.Y(Ta), e.X(xb), e.Y(Ta), { color: 'naranja', grosor: 2.6 });
      this.linea(e.X(xb), e.Y(Ta), e.X(xb), e.Y(Tb), { color: 'naranja', grosor: 2.6 });
    }
    const xk = xs(k);
    const Tk = interpola(this.perfil, xk);
    const der = k > N * 0.55;
    // Con el barrido de la lección ya hay una etiqueta arriba del punto: esta va abajo de la curva
    const dy = etapa === 5 ? -12 : 24;
    this.texto(`${k} ${k === 1 ? 'capa' : 'capas'}: bajó ${num(this.T1 - Tk, 3)} ${this.uT}`, e.X(xk) + (der ? -10 : 10), e.Y(Tk) + dy,
      { tam: 14, color: 'naranja', negrita: true, halo: true, alin: der ? 'right' : 'left' });
    if (etapa === 5 && k >= N) {
      const fin = this.s.r3 !== undefined ? 'T₁ − T₃' : 'T₁ − T₂';
      this.texto(`Suma de todas = ${fin} = ${num(this.T1 - this.Tn, 4)} ${this.uT}`, e.x + e.w, e.y - 8, { tam: 15, color: 'azul', negrita: true, alin: 'right', halo: true });
    }
  }

  /** Abajo: la bajadita de temperatura dT dentro de una sola capa. */
  _bajadaCapa() {
    const { e } = this;
    const [xa, xb] = this._capa;
    const Ta = interpola(this.perfil, xa);
    const Tb = interpola(this.perfil, xb);
    this.caja(e.X(xa), e.y, Math.max(2, e.X(xb) - e.X(xa)), e.h, { r: 0, relleno: alfa('amarillo', 0.35) });
    this.linea(e.X(xb) + 7, e.Y(Ta), e.X(xb) + 7, e.Y(Tb), { color: 'naranja', grosor: 3.2 });
    this.texto('dT', e.X(xb) + 13, (e.Y(Ta) + e.Y(Tb)) / 2 + 5, { tam: 16, mate: true, color: 'naranja', negrita: true, halo: true });
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
      else if (s.geo === 'esfera_seccionada') {
        const R = s.R || 0.10;
        const L = s.L || 0.08;
        const lnTot = Math.log((R + L) / (R - L));
        f = (Math.log((R + r) / (R - r)) + lnTot) / (2 * lnTot);
      } else f = i / N;
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
    const etapa = this._etapa();
    if (etapa) {
      this._capasRadiales(cx, cy, etapa);
      const [tit, cuerpo] = this._rotulos(etapa);
      const y = this._parrafo([tit], 330, 40, 218, { tam: 16, color: 'azul', negrita: true });
      this._parrafo(cuerpo, 330, y + 6, 218, { tam: 15 });
      return;
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
    const etapa = this._etapa();
    if (etapa) {
      this._capasPlanas(x0, w, yc, alto, forma, etapa);
      const [tit, cuerpo] = this._rotulos(etapa);
      this.texto(tit, 280, 24, { tam: 16, color: 'azul', negrita: true, alin: 'center', halo: true });
      this._parrafo(cuerpo, 66, yc + alto / 2 + 34, 430, { tam: 14 });
      return;
    }
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

  seccionEsferaSeccionada() {
    const { s } = this;
    const c = this.ctx;
    const cx = 174;
    const cy = 148;
    const R = s.R || 0.10;
    const L = s.L || 0.06;
    const Rpx = 100;
    const Lpx = Rpx * (L / R);
    const yToPy = (y) => cy - (y / R) * Rpx;
    const xHalf = (y) => Math.sqrt(Math.max(0, R * R - y * y)) * (Rpx / R);

    // Contorno punteado de la esfera original (referencia visual del corte)
    this.circulo(cx, cy, Rpx, { borde: 'tinta3', grosor: 1.2, guion: [4, 4], a: 0.55 });

    // Rebanadas horizontales coloreadas por temperatura desde y = -L (abajo, T1) hasta y = +L (arriba, T2)
    const N = 48;
    for (let i = 0; i < N; i += 1) {
      const y0 = -L + (2 * L * i) / N;
      const y1 = -L + (2 * L * (i + 1)) / N;
      const T = interpola(this.perfil, (y0 + y1) / 2);
      const w0 = xHalf(y0);
      const w1 = xHalf(y1);
      const py0 = yToPy(y0);
      const py1 = yToPy(y1);
      this.poligono(
        [[cx - w0, py0 + 0.5], [cx + w0, py0 + 0.5], [cx + w1, py1 - 0.5], [cx - w1, py1 - 0.5]],
        { relleno: this.colorT(T, 1), a: 0.82 },
      );
    }

    // Contorno grueso de la esfera seccionada + marcas sutiles de aislante lateral
    const ang = Math.asin(Math.min(0.99, L / R));
    c.save();
    c.strokeStyle = color('tinta');
    c.lineWidth = 2.4;
    c.beginPath();
    c.arc(cx, cy, Rpx, -ang, ang);
    c.stroke();
    c.beginPath();
    c.arc(cx, cy, Rpx, Math.PI - ang, Math.PI + ang);
    c.stroke();
    c.restore();

    // Trazos cortos de aislante sobre los arcos laterales
    for (const side of [-1, 1]) {
      for (let k = -3; k <= 3; k += 1) {
        const th = (k / 3.6) * ang;
        const bx = cx + side * Rpx * Math.cos(th);
        const by = cy - Rpx * Math.sin(th);
        this.linea(bx, by, bx + side * 8, by - 4, { color: 'tinta3', grosor: 1.3, a: 0.75 });
      }
    }

    // Depósitos térmicos planos: abajo T1 (caliente, y = -L) y arriba T2 (fría, y = +L)
    const wCap = xHalf(L);
    const pTop = yToPy(L);
    const pBot = yToPy(-L);
    const wDep = Math.max(wCap * 2 + 28, 176);
    this.caja(cx - wDep / 2, pTop - 26, wDep, 26, { r: 4, relleno: this.colorT(this.Tn, 0.88), borde: 'tinta', grosor: 1.8 });
    this.caja(cx - wDep / 2, pBot, wDep, 26, { r: 4, relleno: this.colorT(this.T1, 0.88), borde: 'tinta', grosor: 1.8 });
    this.texto('T₂ (y = +L)', cx, pTop - 8, { tam: 15, alin: 'center', negrita: true, halo: true });
    this.texto('T₁ (y = −L)', cx, pBot + 18, { tam: 15, alin: 'center', negrita: true, halo: true });

    // Ejes x-y centrados con puntas limpias y rótulos despejados
    this.flecha(cx - Rpx - 18, cy, cx + Rpx + 24, cy, { color: 'tinta2', grosor: 1.25, punta: 6, a: 0.72 });
    this.flecha(cx, pBot + 30, cx, pTop - 38, { color: 'tinta2', grosor: 1.25, punta: 6, a: 0.72 });
    this.texto('x', cx + Rpx + 20, cy - 9, { tam: 15, mate: true, color: 'tinta2', halo: true });
    this.texto('y', cx + 11, pTop - 32, { tam: 15, mate: true, color: 'tinta2', halo: true });

    for (let i = 0; i < 3; i += 1) {
      const u = this.animado() ? (this.t * 0.42 + i / 3) % 1 : 0.25 + i * 0.25;
      const py = pBot - 8 - u * (pBot - pTop - 28);
      const xOff = (i - 1) * 32;
      this.flecha(cx + xOff, py, cx + xOff, py - 22, { color: 'tinta', grosor: 2.3, punta: 8, a: 0.35 + 0.65 * Math.sin(u * Math.PI) });
    }

    // Modo "Cómo se arma la integral" (etapas 1..5)
    const etapa = this._etapa();
    if (etapa) {
      const nC = this._nCapas(etapa);
      for (let i = 1; i < nC; i += 1) {
        const yi = -L + (2 * L * i) / nC;
        const wi = xHalf(yi);
        this.linea(cx - wi, yToPy(yi), cx + wi, yToPy(yi), { color: 'tinta', grosor: 1, a: 0.55 });
      }
      if (etapa >= 2 && etapa <= 4) {
        const idx = this._capaResaltada(nC);
        const y0 = -L + (2 * L * idx) / nC;
        const y1 = -L + (2 * L * (idx + 1)) / nC;
        const ym = (y0 + y1) / 2;
        const w0 = xHalf(y0);
        const w1 = xHalf(y1);
        const pym = yToPy(ym);
        const wm = xHalf(ym);
        this.poligono(
          [[cx - w0, yToPy(y0)], [cx + w0, yToPy(y0)], [cx + w1, yToPy(y1)], [cx - w1, yToPy(y1)]],
          { relleno: alfa('amarillo', 0.72), borde: 'naranja', grosor: 2 },
        );
        this.linea(cx, cy, cx + wm, pym, { color: 'azul', grosor: 1.9, guion: [4, 3] });
        const rYOff = pym <= cy ? 15 : -8;
        this.texto('R', cx + wm * 0.48, (cy + pym) / 2 + rYOff, { tam: 16, mate: true, color: 'azul', negrita: true, halo: true });
        this.texto('dy', cx - Math.max(w0, w1) - 10, pym + 5, { tam: 16, mate: true, alin: 'right', color: 'naranja', negrita: true, halo: true });
        this.rMarcado = ym;
        this._capa = [y0, y1];
      }
      const [tit, cuerpo] = this._rotulos(etapa);
      const yTxt = this._parrafo([tit], 324, 38, 224, { tam: 15, color: 'azul', negrita: true });
      this._parrafo(cuerpo, 324, yTxt + 6, 224, { tam: 14 });
      return;
    }

    // Rebanada activa + triángulo rectángulo de Pitágoras (x² + y² = R²) sin solapes
    const barr = this.foco === 'dx' || this.foco === 'dr' || this.foco === 'barrido';
    const yAct = this.foco === 'barrido'
      ? -L + 2 * L * suave((this.tFoco % 3.4) / 2.6)
      : this.x !== null ? this.x : barr ? this.x0 + L * 0.22 * Math.sin(this.t * 1.6) : this.x0;
    const yCl = Math.max(-L + 0.002, Math.min(L - 0.002, yAct));
    this.rMarcado = yCl;
    const py = yToPy(yCl);
    const wx = xHalf(yCl);
    this.caja(cx - wx, py - 5, wx * 2, 10, { r: 2, relleno: alfa('amarillo', 0.65), borde: 'naranja', grosor: 1.7 });
    // Triángulo (0,0) -> (0,y) -> (x,y)
    const dyPx = cy - py; // >0 arriba del ecuador, <0 abajo
    if (Math.abs(dyPx) >= 8) {
      this.linea(cx, cy, cx, py, { color: 'verde', grosor: 2.2 });
    }
    this.linea(cx, cy, cx + wx, py, { color: 'azul', grosor: 2.0, guion: [5, 3] });
    this.linea(cx, py, cx + wx, py, { color: 'naranja', grosor: 2.2 });
    if (dyPx >= 0) {
      // Mitad superior: x arriba de la rebanada, R claramente debajo de la hipotenusa
      this.texto('x', cx + wx * 0.54, py - 10, { tam: 16, mate: true, alin: 'center', color: 'naranja', negrita: true, halo: true });
      const rY = Math.abs(dyPx) < 16 ? py + 20 : (cy + py) / 2 + 16;
      this.texto('R', cx + wx * 0.56, rY, { tam: 16, mate: true, alin: 'center', color: 'azul', negrita: true, halo: true });
    } else {
      // Mitad inferior: x abajo de la rebanada, R claramente arriba de la hipotenusa
      this.texto('x', cx + wx * 0.54, py + 20, { tam: 16, mate: true, alin: 'center', color: 'naranja', negrita: true, halo: true });
      const rY = Math.abs(dyPx) < 16 ? py - 12 : (cy + py) / 2 - 9;
      this.texto('R', cx + wx * 0.56, rY, { tam: 16, mate: true, alin: 'center', color: 'azul', negrita: true, halo: true });
    }
    if (Math.abs(dyPx) >= 18) {
      this.texto('y', cx - 12, (cy + py) / 2 + 5, { tam: 15, mate: true, alin: 'right', color: 'verde', negrita: true, halo: true });
    }
    if (barr) {
      this.texto('dy', cx - wx - 10, py + 5, { tam: 16, mate: true, alin: 'right', color: 'naranja', negrita: true, halo: true });
    }

    // Panel derecho de fórmulas clave (sin encimarse con la esfera)
    const rx = 326;
    this.texto('Esfera seccionada', rx, 44, { tam: 17, negrita: true, color: 'tinta' });
    this.texto('Cortes en y = −L y y = +L', rx, 66, { tam: 14, color: 'tinta2' });
    this.texto('x² + y² = R²', rx, 102, { tam: 19, mate: true, color: 'tinta', negrita: true });
    this.texto('A(y) = π(R² − y²)', rx, 134, { tam: 19, mate: true, color: 'azul', negrita: true });
    this.texto('Pared curva aislada', rx, 168, { tam: 14, color: 'tinta2' });
    this.texto('Flujo vertical constante H', rx, 190, { tam: 14, color: 'tinta2' });
    this.texto('0 < L < R', rx, 222, { tam: 15, mate: true, color: 'naranja', negrita: true });
  }

  dibujar() {
    this.rMarcado = null;
    this._capa = null;
    if (this.radial) this.seccionRadial();
    else if (this.esferaSec) this.seccionEsferaSeccionada();
    else this.seccionPlana();
    const { e } = this;
    const c = this.ctx;
    e.dibujar({ tam: 15, corteX: this.a > 0 });
    const etapa = this._etapa();
    const enPerfil = ['perfil', 'listo', 'punto', 'barrido', null, undefined].includes(this.foco) || this.x !== null || etapa > 0;
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
    // La integral como suma de bajadas (escalones) o la bajadita dT de una sola capa
    if (!oculta) {
      if (etapa === 5 || (!etapa && this.foco === 'barrido')) this._escalera(etapa);
      else if (etapa >= 2 && etapa <= 4 && this._capa) this._bajadaCapa();
      else if (!etapa && (this.foco === 'dr' || this.foco === 'dx') && this.rMarcado !== null && this.rMarcado !== undefined) {
        const d = (this.b - this.a) / 24;
        this._capa = [Math.max(this.a, this.rMarcado - d), Math.min(this.b, this.rMarcado + d)];
        this._bajadaCapa();
      }
    }
    const r = this.rMarcado ?? (this.foco === 'punto' ? this.x0 : null);
    if (r !== null && r !== undefined && !oculta) {
      const T = interpola(this.perfil, r);
      this.linea(e.X(r), e.y + e.h, e.X(r), e.Y(T), { color: 'amarillo', grosor: 2, guion: [5, 4] });
      this.linea(e.x, e.Y(T), e.X(r), e.Y(T), { color: 'amarillo', grosor: 2, guion: [5, 4] });
      this.circulo(e.X(r), e.Y(T), 6, { relleno: 'amarillo', borde: 'tinta' });
      const izq = Boolean(this._capa) && e.X(r) > e.x + 92;
      this.texto(`${num(T, 4)} ${this.uT}`, e.X(r) + (izq ? -12 : 12), e.Y(T) - 13, {
        tam: 15, negrita: true, halo: true, color: 'naranja', alin: izq ? 'right' : 'left',
      });
    }
  }
}
