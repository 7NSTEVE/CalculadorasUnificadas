/**
 * Escenas 3D interactivas del Parcial 3 (FI2103):
 *  9. Sólidos en conducción 3D con corte interactivo (esfera cortada O26-8 con triángulo de
 *     Pitágoras r²+y²=R², tronco de cono T-CONO, cascarón cilíndrico Q045, pared serie/k(y)).
 * 10. Cilindro-Pistón 3D con moléculas animadas, acoplado en vivo al diagrama 2D p-V,
 *     además de modos Otto (doble cilindro) y vibración molecular diatómica.
 * 11. Dilatación térmica 3D (barra, placa con hueco que crece, recipiente con derrame).
 * 12. Calorimetría y radiación térmica 3D (mezcla térmica hacia T_f y ondas Stefan-Boltzmann T⁴).
 */

import { num } from '../visuales/base.js';
import {
  THREE,
  crearFlecha3D,
  crearLinea3D,
  crearMaterial,
  crearPisoSutil,
} from './motor_3d.js';

const TAU = Math.PI * 2;

export function estadoCiclo(estados, procesos, gamma = 1.4, t = 0) {
  if (!procesos || !procesos.length || !estados) return { v: 1, p: 1, a: 'A', b: 'B', i: 0, u: 0, tipo: 'isobarico' };
  const n = procesos.length;
  const clamped = ((t % 1) + 1) % 1;
  const i = Math.min(n - 1, Math.floor(clamped * n));
  const u = clamped * n - i;
  const [a, b, tipo] = procesos[i];
  const [va, pa] = estados[a] || [1, 1];
  const [vb, pb] = estados[b] || [1, 1];
  if (u <= 1e-6) return { v: va, p: pa, a, b, i, u: 0, tipo };
  const v = va + (vb - va) * u;
  let p = pa + (pb - pa) * u;
  if ((tipo === 'isotermico' || tipo === 'adiabatico') && va > 0 && v > 0 && vb > 0) {
    const g = tipo === 'isotermico' ? 1 : gamma;
    p = pa * (va / v) ** g;
    p += u * (pb - pa * (va / vb) ** g);
  }
  return { v, p, a, b, i, u, tipo };
}

/** Integral de Simpson (n par). */
function simpson(f, a, b, n = 200) {
  if (!(Math.abs(b - a) > 0)) return 0;
  const m = n % 2 ? n + 1 : n;
  const d = (b - a) / m;
  let s = f(a) + f(b);
  for (let i = 1; i < m; i += 1) s += f(a + i * d) * (i % 2 ? 4 : 2);
  return (s * d) / 3;
}

/**
 * R8 en conducción: el pedacito barre de un límite al otro y se van sumando las resistencias
 * dx/(k·A(x)); al final, H = ΔT / R. Sin k (lecciones de teoría) se suma dx/A(x).
 */
function integralConduccion(p, var_, desde, hasta, actual, A, kDe) {
  const conK = typeof kDe === 'function';
  const f = conK ? (x) => 1 / (kDe(x) * A(x)) : (x) => 1 / A(x);
  const parcial = simpson(f, desde, actual);
  const total = simpson(f, desde, hasta);
  if (!(total > 0)) return null;
  const unidad = conK ? 'K/W' : 'm⁻¹';
  const nombre = conK ? `d${var_}/(kA)` : `d${var_}/A`;
  let texto = `Suma de ${nombre} de ${var_} = ${num(desde, 3)} m a ${var_} = ${num(actual, 3)} m: ${num(parcial, 4)} ${unidad} de ${num(total, 4)} ${unidad}`;
  const dT = Number(p.T1) - Number(p.T2);
  if (conK && Number.isFinite(dT) && Math.abs(actual - hasta) < Math.abs(hasta - desde) * 1e-3) {
    texto += ` ⇒ H = ΔT/R = ${num(Math.abs(dT) / total, 4)} W`;
  }
  return { actual: parcial, total, texto };
}

/** Conductividad: constante (k) o lineal en la posición (k0 + k1·x), si el enunciado la da. */
function conductividad(p) {
  if (Number.isFinite(Number(p.k0)) && Number.isFinite(Number(p.k1))) {
    const k0 = Number(p.k0); const k1 = Number(p.k1);
    return (x) => k0 + k1 * x;
  }
  const k = Number(p.k);
  return Number.isFinite(k) && k > 0 ? () => k : null;
}

/** 9. Sólidos en conducción 3D con plano de corte y el pedacito dy / dx / dr que barre entre los límites. */
export function construirSolido3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const geo = p.geo || 'pared';
  const kDe = conductividad(p);
  const u = Math.max(0, Math.min(1, t));

  g.add(crearPisoSutil(-1.68));

  if (geo === 'dilatacion') {
    return construirDilatacion3D(motor, p, t, foco);
  }

  // Esfera cortada por dos planos (O26-8): disco de radio r(y) = √(R² − y²) y grosor dy
  if (geo === 'esfera_seccionada') {
    const R = Number(p.R || 0.1);
    const L = Math.min(R * 0.96, Number(p.L || 0.08));
    const esc = 1.45 / R;
    const Rvis = R * esc;
    const Lvis = L * esc;

    const pts = [];
    const M = 28;
    for (let i = 0; i <= M; i += 1) {
      const y = -Lvis + (2 * Lvis * i) / M;
      pts.push(new THREE.Vector2(Math.sqrt(Math.max(0.001, Rvis * Rvis - y * y)), y));
    }
    const cuerpo = new THREE.Mesh(
      new THREE.LatheGeometry(pts, 36),
      crearMaterial('gris', { opacidad: 0.34, dobleCara: true, corte }),
    );
    cuerpo.userData.info = {
      titulo: 'Esfera cortada (−L ≤ y ≤ L)',
      detalle: 'El calor va de una cara plana a la otra; el área del disco cambia con y.',
      formula: 'A(y) = πr² = π(R² − y²)',
    };
    g.add(cuerpo);

    const rTapa = Math.sqrt(Math.max(0.01, Rvis * Rvis - Lvis * Lvis));
    const tapa1 = new THREE.Mesh(new THREE.CylinderGeometry(rTapa, rTapa, 0.05, 32), crearMaterial('rojo', { opacidad: 0.82, corte }));
    tapa1.position.y = -Lvis;
    const tapa2 = new THREE.Mesh(new THREE.CylinderGeometry(rTapa, rTapa, 0.05, 32), crearMaterial('azul', { opacidad: 0.82, corte }));
    tapa2.position.y = Lvis;
    g.add(tapa1, tapa2);

    const yDatos = -L + 2 * L * u;
    const yVis = yDatos * esc;
    const rDisco = Math.sqrt(Math.max(0.01, Rvis * Rvis - yVis * yVis));
    const disco = new THREE.Mesh(
      new THREE.CylinderGeometry(rDisco, rDisco, 0.09, 32),
      crearMaterial(foco === 'corte' ? 'naranja' : 'morado', { opacidad: 0.86, emisivo: 'naranja', intensidadEmisiva: 0.25 }),
    );
    disco.position.y = yVis;
    disco.userData.info = {
      titulo: `Disco de grosor dy en y = ${num(yDatos, 3)} m`,
      detalle: 'Por Pitágoras desde el centro O: r² + y² = R², entonces A(y) = π(R² − y²).',
      formula: 'H = −k π(R² − y²) dT/dy',
    };
    g.add(disco);

    // Triángulo de Pitágoras (centro O → eje y → borde del disco)
    g.add(
      crearLinea3D([[0, 0, 0.06], [0, yVis, 0.06]], 'verde'),
      crearLinea3D([[0, yVis, 0.06], [rDisco, yVis, 0.06]], 'naranja'),
      crearLinea3D([[0, 0, 0.06], [rDisco, yVis, 0.06]], 'morado'),
    );
    g.add(crearFlecha3D([Rvis + 0.45, -Lvis, 0], [Rvis + 0.45, Lvis, 0], 'rojo'));

    const rReal = Math.sqrt(Math.max(0, R * R - yDatos * yDatos));
    tags.push(
      { id: 'disc', pos: [rDisco * 0.6, yVis + 0.24, 0.12], texto: `r(y) = √(R²−y²) = ${num(rReal, 3)} m`, activo: foco === 'corte', color: 'naranja' },
      { id: 'dy', pos: [-rDisco - 0.3, yVis, 0.12], texto: 'dy', color: 'naranja' },
      { id: 'hip', pos: [rDisco * 0.45, yVis * 0.42, 0.12], texto: 'R', color: 'morado' },
      { id: 't1', pos: [-rTapa - 0.35, -Lvis, 0], texto: `T₁ = ${num(p.T1 ?? 100, 3)} °C`, activo: foco === 'limites', color: 'rojo' },
      { id: 't2', pos: [-rTapa - 0.35, Lvis, 0], texto: `T₂ = ${num(p.T2 ?? 20, 3)} °C`, activo: foco === 'limites', color: 'azul' },
      { id: 'fl', pos: [Rvis + 0.8, 0, 0], texto: 'Flujo H', activo: foco === 'calor', color: 'rojo' },
    );
    const A = (y) => Math.PI * Math.max(1e-12, R * R - y * y);
    return {
      tags,
      medida: `y = ${num(yDatos, 3)} m · A(y) = π(R²−y²) = ${num(A(yDatos), 4)} m²`,
      integral: integralConduccion(p, 'y', -L, L, yDatos, A, kDe),
      anclas: {
        limites: [-rTapa - 0.35, -Lvis - 0.35, 0], corte: [rDisco * 0.6, yVis + 0.55, 0.12],
        calor: [Rvis + 0.8, 0.35, 0], _datos: [-2.45, 1.45, 0], _: [Rvis + 0.85, -0.45, 0],
      },
    };
  }

  // Tronco de cono (T-CONO): disco de radio r(x) = r₁ + (r₂ − r₁)·x/L y grosor dx
  if (geo === 'cono') {
    const r1 = Number(p.r1 || 0.01);
    const r2 = Number(p.r2 || 0.03);
    const L = Number(p.L || 0.2);
    const rMax = Math.max(r1, r2);
    const r1v = 0.35 + 0.72 * (r1 / rMax);
    const r2v = 0.35 + 0.72 * (r2 / rMax);
    const Lvis = 3.2;

    const cono = new THREE.Mesh(
      new THREE.CylinderGeometry(r2v, r1v, Lvis, 32, 1, false),
      crearMaterial('gris', { opacidad: 0.32, dobleCara: true, corte }),
    );
    cono.rotation.z = -Math.PI / 2;
    cono.userData.info = { titulo: 'Tronco de cono', detalle: 'El radio crece en línea recta: r(x) = r₁ + (r₂−r₁)·x/L.', formula: 'A(x) = π r(x)²' };
    g.add(cono);

    const xVis = -Lvis / 2 + u * Lvis;
    const rVis = r1v + (r2v - r1v) * u;
    const disco = new THREE.Mesh(
      new THREE.CylinderGeometry(rVis, rVis, 0.09, 32),
      crearMaterial('naranja', { opacidad: 0.9, emisivo: 'naranja', intensidadEmisiva: 0.25 }),
    );
    disco.rotation.z = -Math.PI / 2;
    disco.position.x = xVis;
    g.add(disco);
    g.add(crearFlecha3D([-Lvis / 2, -1.28, 0], [Lvis / 2, -1.28, 0], 'rojo'));

    const xReal = u * L;
    const rDe = (x) => r1 + ((r2 - r1) * x) / L;
    tags.push(
      { id: 'c', pos: [xVis, rVis + 0.28, 0], texto: `r(x) = ${num(rDe(xReal), 3)} m · A = πr²`, activo: foco === 'corte', color: 'naranja' },
      { id: 'dx', pos: [xVis, -rVis - 0.28, 0], texto: 'dx', color: 'naranja' },
      { id: 't1', pos: [-Lvis / 2, -r1v - 0.32, 0], texto: `r₁: ${num(p.T1 ?? 100, 3)} °C`, color: 'rojo' },
      { id: 't2', pos: [Lvis / 2, -r2v - 0.32, 0], texto: `r₂: ${num(p.T2 ?? 0, 3)} °C`, color: 'azul' },
    );
    const A = (x) => Math.PI * rDe(x) ** 2;
    return {
      tags,
      medida: `x = ${num(xReal, 3)} m · r(x) = ${num(rDe(xReal), 4)} m · A(x) = ${num(A(xReal), 4)} m²`,
      integral: integralConduccion(p, 'x', 0, L, xReal, A, kDe),
      anclas: {
        limites: [-Lvis / 2, r1v + 0.35, 0], corte: [xVis, rVis + 0.6, 0], calor: [0, -1.55, 0],
        _datos: [-2.45, 1.5, 0], _: [Lvis / 2 + 0.2, r2v + 0.45, 0],
      },
    };
  }

  // Cascarón cilíndrico (Q045): área lateral 2πrL y grosor dr, de r₁ a r₂
  if (geo === 'cilindro' || geo === 'cilindro2') {
    const r1 = Number(p.r1 || 0.04);
    const r2 = Number(p.r2 || 0.08);
    const Lc = Number(p.L || 1);
    const rVis1 = 0.46;
    const rVis2 = 1.18;
    const rActVis = rVis1 + u * (rVis2 - rVis1);

    const ext = new THREE.Mesh(
      new THREE.CylinderGeometry(rVis2, rVis2, 3.1, 32, 1, true),
      crearMaterial('azul', { opacidad: 0.25, dobleCara: true, corte }),
    );
    ext.rotation.z = Math.PI / 2;
    const intC = new THREE.Mesh(
      new THREE.CylinderGeometry(rVis1, rVis1, 3.14, 28, 1, false),
      crearMaterial('rojo', { opacidad: 0.72, corte }),
    );
    intC.rotation.z = Math.PI / 2;
    const cascaron = new THREE.Mesh(
      new THREE.CylinderGeometry(rActVis, rActVis, 3.06, 32, 1, true),
      crearMaterial('naranja', { opacidad: 0.65, dobleCara: true, corte, emisivo: 'naranja', intensidadEmisiva: 0.2 }),
    );
    cascaron.rotation.z = Math.PI / 2;
    cascaron.userData.info = {
      titulo: 'Cascarón cilíndrico (grosor dr)',
      detalle: 'El calor sale en dirección radial y atraviesa el área lateral 2πrL.',
      formula: 'H = −k (2πrL) dT/dr',
    };
    g.add(ext, intC, cascaron);

    g.add(crearFlecha3D([0, rVis1, 0], [0, rVis2 + 0.35, 0], 'rojo'));
    const rReal = r1 + u * (r2 - r1);
    tags.push(
      { id: 'cas', pos: [0, rActVis + 0.28, 0.2], texto: `r = ${num(rReal, 3)} m · A(r) = 2πrL`, activo: foco === 'corte', color: 'naranja' },
      { id: 'dr', pos: [0.75, rActVis, 0.2], texto: 'dr', color: 'naranja' },
      { id: 'fl', pos: [0.4, rVis2 + 0.38, 0], texto: 'H radial', activo: foco === 'calor', color: 'rojo' },
    );
    const A = (r) => 2 * Math.PI * r * Lc;
    return {
      tags,
      medida: `r = ${num(rReal, 4)} m · A(r) = 2πrL = ${num(A(rReal), 4)} m² · al integrar dr/r sale ln(r₂/r₁)`,
      integral: integralConduccion(p, 'r', r1, r2, rReal, A, kDe),
      anclas: {
        limites: [-1.9, rVis1 + 0.2, 0], corte: [0, rActVis + 0.6, 0.2], calor: [0.4, rVis2 + 0.7, 0],
        _datos: [-2.45, 1.6, 0], _: [1.9, rVis2 + 0.2, 0],
      },
    };
  }

  // Pared plana, capas en serie o lámina con k(y) variable (Q050, Q051, O26-5)
  const Lvis = 3.4;
  if (geo === 'serie' && Array.isArray(p.capas) && p.capas.length > 1) {
    const tot = p.capas.reduce((s, c) => s + (c.e || 1), 0) || 1;
    let x = -Lvis / 2;
    p.capas.forEach((c, idx) => {
      const w = (Lvis * (c.e || 1)) / tot;
      const m = new THREE.Mesh(
        new THREE.BoxGeometry(w * 0.98, 1.7, 1.3),
        crearMaterial(idx % 2 === 0 ? 'naranja' : 'azul', { opacidad: 0.45, corte }),
      );
      m.position.set(x + w / 2, 0, 0);
      g.add(m);
      tags.push({ id: `cp${idx}`, pos: [x + w / 2, 1.08, 0], texto: c.n || `Capa ${idx + 1}` });
      x += w;
    });
  } else {
    const bloque = new THREE.Mesh(
      new THREE.BoxGeometry(Lvis, 1.72, 1.28),
      crearMaterial('gris', { opacidad: 0.28, corte }),
    );
    const caraCal = new THREE.Mesh(new THREE.BoxGeometry(0.08, 1.74, 1.3), crearMaterial('rojo', { opacidad: 0.85 }));
    caraCal.position.x = -Lvis / 2;
    const caraFri = new THREE.Mesh(new THREE.BoxGeometry(0.08, 1.74, 1.3), crearMaterial('azul', { opacidad: 0.85 }));
    caraFri.position.x = Lvis / 2;
    const lamina = new THREE.Mesh(
      new THREE.BoxGeometry(0.11, 1.78, 1.34),
      crearMaterial('naranja', { opacidad: 0.82, emisivo: 'naranja', intensidadEmisiva: 0.2 }),
    );
    lamina.position.x = -Lvis / 2 + u * Lvis;
    lamina.userData.info = {
      titulo: 'Sección transversal (grosor dx o dy)',
      detalle: 'Si k o A cambian con la posición, se separan variables y se integra la ley de Fourier.',
      formula: 'H = −k A dT/dx',
    };
    g.add(bloque, caraCal, caraFri, lamina);
    tags.push({ id: 'sec', pos: [lamina.position.x, 1.12, 0], texto: 'Corte de grosor dx', activo: foco === 'corte', color: 'naranja' });
  }
  g.add(crearFlecha3D([-Lvis / 2, -1.25, 0], [Lvis / 2, -1.25, 0], 'rojo'));
  tags.push(
    { id: 't1', pos: [-Lvis / 2, -1.52, 0], texto: p.T1 !== undefined ? `${num(p.T1, 3)} °C` : 'T_H (caliente)', color: 'rojo' },
    { id: 't2', pos: [Lvis / 2, -1.52, 0], texto: p.T2 !== undefined ? `${num(p.T2, 3)} °C` : 'T_C (fría)', color: 'azul' },
  );
  const Lreal = Number(p.L);
  const Area = Number(p.A);
  const integral = Number.isFinite(Lreal) && Lreal > 0 && Number.isFinite(Area) && Area > 0
    ? integralConduccion(p, p.eje === 'y' ? 'y' : 'x', 0, Lreal, u * Lreal, () => Area, kDe)
    : null;
  return {
    tags,
    medida: 'El calor H va siempre de la cara caliente a la fría (en estado estacionario, H es el mismo en todo corte).',
    integral,
    anclas: {
      limites: [-Lvis / 2, 1.3, 0], corte: [-Lvis / 2 + u * Lvis, 1.45, 0], calor: [0, -1.55, 0],
      _datos: [-2.5, 1.75, 0], _: [Lvis / 2 + 0.3, 0.6, 0],
    },
  };
}

// Dónde se marcan los datos y lo que piden en el pistón (R7): trabajo junto al émbolo,
// estados arriba del cilindro, calor y eficiencia junto al gas.
const ANCLAS_PISTON = {
  volumen: [1.55, 1.4, 0], presion: [0, 1.95, 0], energia: [-1.55, -0.35, 0.5],
  _datos: [-2.45, 1.5, 0], _: [1.95, 0.55, 0],
};

/** 10. Cilindro-Pistón 3D acoplado al ciclo p-V, modo Otto o vibración diatómica. */
export function construirPiston3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();

  g.add(crearPisoSutil(-1.68));

  if (p.modo === 'modos') {
    const s = 0.72 + 0.22 * t * Math.sin(TAU * t * 4);
    const a1 = new THREE.Mesh(new THREE.SphereGeometry(0.38, 24, 20), crearMaterial('azul', { metal: 0.2 }));
    a1.position.set(-s, 0, 0);
    const a2 = new THREE.Mesh(new THREE.SphereGeometry(0.38, 24, 20), crearMaterial('azul', { metal: 0.2 }));
    a2.position.set(s, 0, 0);
    const enlace = new THREE.Mesh(new THREE.CylinderGeometry(0.08, 0.08, 2 * s, 14), crearMaterial('morado'));
    enlace.rotation.z = Math.PI / 2;
    g.add(a1, a2, enlace);
    tags.push({ id: 'mol', pos: [0, -0.78, 0], texto: t > 0.45 ? 'Vibración activa (C_V sube)' : 'Traslación + Rotación (5/2 R)' });
    return {
      tags,
      medida: 'A mayor temperatura se activan los modos de vibración y C_V del diatómico aumenta.',
      anclas: { _datos: [-2.3, 1.35, 0], _: [0, 1.15, 0] },
    };
  }

  const armarCilindro = (x0, fracVol, calT, etiqueta, sub = '', colGas = 'azul') => {
    const hMax = 2.2;
    const yBase = -1.25;
    const hGas = Math.max(0.35, 0.35 + fracVol * (hMax - 0.5));
    const pared = new THREE.Mesh(
      new THREE.CylinderGeometry(0.88, 0.88, hMax + 0.25, 28, 1, true),
      crearMaterial('borde', { opacidad: 0.25, dobleCara: true, corte }),
    );
    pared.position.set(x0, yBase + (hMax + 0.25) / 2, 0);
    const base = new THREE.Mesh(
      new THREE.CylinderGeometry(0.9, 0.9, 0.12, 28),
      crearMaterial('tinta3', { opacidad: 0.78 }),
    );
    base.position.set(x0, yBase - 0.06, 0);
    const gas = new THREE.Mesh(
      new THREE.CylinderGeometry(0.82, 0.82, hGas, 26),
      crearMaterial(colGas, { opacidad: 0.24, corte }),
    );
    gas.position.set(x0, yBase + hGas / 2, 0);
    const embolo = new THREE.Mesh(
      new THREE.CylinderGeometry(0.85, 0.85, 0.18, 28),
      crearMaterial(foco === 'volumen' ? 'morado' : 'tinta2', { metal: 0.45, rugosidad: 0.32 }),
    );
    embolo.position.set(x0, yBase + hGas + 0.09, 0);
    embolo.userData.info = {
      titulo: 'Pistón móvil (W = ∫ p dV)',
      detalle: 'Si sube (expansión), el gas hace trabajo positivo W > 0. Si baja (compresión), W < 0. Si no se mueve, W = 0.',
      formula: 'ΔU = Q − W',
    };
    const vastago = new THREE.Mesh(
      new THREE.CylinderGeometry(0.08, 0.08, 0.82, 14),
      crearMaterial('tinta3', { metal: 0.5 }),
    );
    vastago.position.set(x0, yBase + hGas + 0.56, 0);
    g.add(pared, base, gas, embolo, vastago);

    // Moléculas agitadas dentro del volumen del gas
    for (let i = 0; i < 10; i += 1) {
      const ang = i * 2.1 + t * TAU * (1 + (i % 3));
      const rad = 0.22 + 0.42 * ((i * 37) % 10) / 10;
      const ym = yBase + 0.18 + ((i * 0.19 + t * (1.2 + calT)) % 1) * Math.max(0.15, hGas - 0.3);
      const mol = new THREE.Mesh(
        new THREE.SphereGeometry(0.065, 10, 10),
        crearMaterial(calT > 0.55 ? 'rojo' : 'azul'),
      );
      mol.position.set(x0 + Math.cos(ang) * rad, ym, Math.sin(ang) * rad);
      g.add(mol);
    }

    if (etiqueta) tags.push({ id: `c_${x0}`, pos: [x0, yBase + hMax + 0.72, 0], texto: etiqueta });
    if (sub) tags.push({ id: `s_${x0}`, pos: [x0, yBase - 0.34, 0], texto: sub });
  };

  if (p.modo === 'otto') {
    const f = 0.5 + 0.45 * Math.cos(TAU * t);
    armarCilindro(-1.28, f, 1 - f, 'Motor A (aire)', 'γ = 1,40', 'azul');
    armarCilindro(1.28, f, 1 - f, 'Motor B (mono)', 'γ = 1,67', 'turquesa');
    return {
      tags,
      medida: 'Ciclo Otto: e = 1 − 1 / r^(γ−1). A igual compresión r, mayor γ da mayor eficiencia.',
      anclas: { energia: [0, 1.85, 0], _datos: [-2.55, 1.7, 0], _: [0, 1.85, 0] },
    };
  }

  // Si tiene estados/procesos p-V, el pistón se sincroniza con el volumen real del ciclo
  if (p.estados && Array.isArray(p.procesos) && p.procesos.length) {
    const e = estadoCiclo(p.estados, p.procesos, p.gamma || 1.4, t);
    const vals = Object.values(p.estados);
    const vMax = Math.max(...vals.map(([v]) => v), 1e-6);
    const pMax = Math.max(...vals.map(([, pr]) => pr), 1e-6);
    const fracV = Math.max(0.12, Math.min(1, e.v / vMax));
    const calT = Math.max(0.1, Math.min(1, (e.p * e.v) / (pMax * vMax)));
    const col = calT > 0.6 ? 'rojo' : calT > 0.35 ? 'naranja' : 'azul';
    const NOMBRES_PROC = {
      isobarico: 'isobárico',
      isocorico: 'isocórico',
      isotermico: 'isotérmico',
      adiabatico: 'adiabático',
      lineal: 'lineal',
    };
    const tipoTxt = NOMBRES_PROC[e.tipo] || e.tipo;
    armarCilindro(0, fracV, calT, `Tramo ${e.a} → ${e.b} (${tipoTxt})`, `V = ${num(e.v, 3)} ${p.uV || 'm³'}`, col);

    // Flecha visual de trabajo / calor según el proceso
    if (e.tipo !== 'isocorico') {
      const va = p.estados[e.a][0];
      const vb = p.estados[e.b][0];
      const exp = vb > va;
      g.add(crearFlecha3D([1.28, 0.2, 0], [1.28, exp ? 1.05 : -0.65, 0], exp ? 'verde' : 'rojo'));
      tags.push({ id: 'w', pos: [1.78, 0.25, 0], texto: exp ? 'W > 0' : 'W < 0', color: exp ? 'verde' : 'rojo' });
    } else {
      tags.push({ id: 'w0', pos: [1.52, 0.25, 0], texto: 'W = 0 (V fijo)', color: 'morado' });
    }
    return {
      tags,
      ciclo: e,
      medida: `${e.a} → ${e.b} (${tipoTxt}) · V = ${num(e.v, 4)} ${p.uV || 'm³'} · p = ${num(e.p, 4)} ${p.up || 'Pa'}`,
      anclas: ANCLAS_PISTON,
    };
  }

  // Gas isocórico o libre
  const fracV = p.modo === 'isocorico' ? 0.55 : 0.28 + 0.65 * t;
  armarCilindro(0, fracV, t, p.modo === 'isocorico' ? 'Volumen constante (W = 0)' : 'Gas ideal en pistón', 'pV = nRT', t > 0.5 ? 'rojo' : 'azul');
  return {
    tags,
    medida: 'Al aumentar T, las moléculas chocan más rápido y sube la presión o se expande el pistón.',
    anclas: ANCLAS_PISTON,
  };
}

/** 11. Dilatación térmica 3D (barra, placa con hueco que crece, derrame líquido). */
export function construirDilatacion3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  g.add(crearPisoSutil(-1.5));

  if (p.caso === 'hueco' || p.caso === 'anillo') {
    const esc = 1 + 0.22 * t;
    const placa = new THREE.Mesh(
      new THREE.TorusGeometry(0.92 * esc, 0.32 * esc, 18, 42),
      crearMaterial(t > 0.5 ? 'rojo' : 'azul', { metal: 0.35, rugosidad: 0.4 }),
    );
    placa.userData.info = {
      titulo: 'Placa con agujero',
      detalle: 'Al calentar un sólido, el hueco crece IGUAL que si estuviera lleno del mismo material.',
      formula: 'Δd = α d₀ ΔT > 0',
    };
    g.add(placa);
    g.add(crearFlecha3D([0, 0, 0.2], [0.58 * esc, 0, 0.2], 'naranja'));
    tags.push(
      { id: 'h', pos: [0, 1.48, 0], texto: 'Al calentar, el hueco CRECE', activo: true, color: 'naranja' },
      { id: 'r', pos: [0.3, -0.22, 0.25], texto: 'r(T) ↑' },
    );
    return { tags, medida: 'Trampa clásica de examen: un agujero en un metal siempre SE AGRANDA al calentar.' };
  }

  // Comparación de dos barras o una barra que se dilata ΔL = α L₀ ΔT
  const d1 = 0.55 * t;
  const d2 = 1.10 * t;
  const b1 = new THREE.Mesh(new THREE.CylinderGeometry(0.22, 0.22, 2.8 + d1, 20), crearMaterial('gris', { metal: 0.4 }));
  b1.rotation.z = Math.PI / 2;
  b1.position.set(-1.5 + (2.8 + d1) / 2, 0.55, 0);
  const b2 = new THREE.Mesh(new THREE.CylinderGeometry(0.22, 0.22, 2.8 + d2, 20), crearMaterial(t > 0.5 ? 'rojo' : 'azul', { metal: 0.35 }));
  b2.rotation.z = Math.PI / 2;
  b2.position.set(-1.5 + (2.8 + d2) / 2, -0.52, 0);
  g.add(b1, b2);
  g.add(crearLinea3D([[1.3, -1.05, 0], [1.3, 1.05, 0]], 'naranja', { punteada: true }));
  tags.push(
    { id: 'a', pos: [0, 0.96, 0], texto: 'Material con α' },
    { id: 'b', pos: [0, -0.96, 0], texto: 'Material con 2α (crece el doble)', activo: foco === 'calentar', color: 'rojo' },
  );
  return { tags, medida: 'ΔL = α L₀ ΔT · Crece proporcional a α, al largo inicial L₀ y al cambio ΔT.' };
}

/**
 * 12. Calorimetría (los cuerpos reales del enunciado: agua, metal, hielo, con su masa) y
 * radiación térmica (Stefan-Boltzmann T⁴). El deslizador va del estado inicial (0 %) al
 * equilibrio (100 %): lo caliente se enfría, lo frío se calienta y el hielo se derrite si alcanza.
 */
export function construirCalor3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const u = Math.max(0, Math.min(1, t));
  g.add(crearPisoSutil(-1.58));

  if (p.tipo === 'radiacion' || p.modo === 'radiacion') {
    const T = 300 + u * 500;
    const radEsc = 0.65 + 0.95 * ((T / 800) ** 2);
    const cuerpo = new THREE.Mesh(
      new THREE.SphereGeometry(0.62, 26, 20),
      crearMaterial(u > 0.5 ? 'rojo' : 'naranja', { emisivo: 'rojo', intensidadEmisiva: 0.15 + u * 0.55 }),
    );
    cuerpo.userData.info = { titulo: 'Cuerpo radiante', detalle: 'Emite energía electromagnética aunque esté en el vacío.', formula: 'H = A e σ T⁴ (T en kelvin)' };
    const halo1 = new THREE.Mesh(
      new THREE.SphereGeometry(0.82 + 0.72 * radEsc, 24, 18),
      crearMaterial('naranja', { opacidad: 0.16, dobleCara: true, corte }),
    );
    g.add(cuerpo, halo1);
    tags.push({ id: 'rad', pos: [0, 1.42, 0], texto: `T = ${num(T, 3)} K → T⁴ crece muy rápido`, color: 'rojo' });
    return {
      tags,
      medida: 'Stefan-Boltzmann: H = AeσT⁴ · si T (en kelvin) se duplica, H se multiplica por 16.',
      anclas: { calentar: [0, 1.75, 0], _datos: [-2.3, 1.5, 0], _: [1.6, 1.1, 0] },
    };
  }

  const comps = Array.isArray(p.componentes) && p.componentes.length
    ? p.componentes
    : [{ n: 'cuerpo caliente', m: 300, T: 80, fase: 'solido' }, { n: 'agua', m: 500, T: 20, fase: 'liquido' }];
  const liquidos = comps.filter((c) => c.fase === 'liquido');
  const solidos = comps.filter((c) => c.fase === 'solido');
  const hielos = comps.filter((c) => c.fase === 'hielo');
  const necesita = (p.necesita || []).reduce((s, x) => s + Number(x.Q || 0), 0);
  const disponible = (p.disponible || []).reduce((s, x) => s + Number(x.Q || 0), 0);
  // ¿Alcanza el calor disponible para derretir todo el hielo? Si no hay datos, se supone que sí.
  const seDerrite = !(disponible > 0 && necesita > 0) || disponible >= necesita;
  const anclas = { _datos: [-2.45, 1.55, 0], _: [1.95, 1.15, 0] };

  if (liquidos.length) {
    const mLiq = liquidos.reduce((s, c) => s + Number(c.m || 0), 0);
    const nivel = Math.max(0.6, Math.min(1.7, 0.6 + mLiq / 600));
    const vaso = new THREE.Mesh(
      new THREE.CylinderGeometry(1.45, 1.45, 2.1, 28, 1, true),
      crearMaterial('borde', { opacidad: 0.25, dobleCara: true, corte }),
    );
    vaso.position.y = -0.35;
    vaso.userData.info = { titulo: 'Recipiente aislado', detalle: 'No entra ni sale calor: todo lo que cede uno lo gana otro.', formula: 'ΣQ = 0' };
    const agua = new THREE.Mesh(
      new THREE.CylinderGeometry(1.38, 1.38, nivel, 26),
      crearMaterial(u > 0.6 ? 'turquesa' : 'azul', { opacidad: 0.28, corte }),
    );
    agua.position.y = -1.4 + nivel / 2;
    g.add(vaso, agua);
    const c0 = liquidos[0];
    tags.push({ id: 'liq', pos: [-1.05, -1.4 + nivel + 0.18, 0.9], texto: `${c0.n || 'agua'}${liquidos.length > 1 ? ' (+)' : ''}`, color: 'azul' });
    anclas.equilibrio = [0, -1.4 + nivel + 0.55, 0];
  }

  solidos.forEach((c, i) => {
    const lado = 0.42 + 0.32 * Math.cbrt(Math.max(1, Number(c.m || 300)) / 300);
    const bloque = new THREE.Mesh(
      new THREE.BoxGeometry(lado, lado, lado),
      crearMaterial(u > 0.7 ? 'turquesa' : (Number(c.T) > 30 ? 'rojo' : 'gris'), { metal: 0.35 }),
    );
    const x = liquidos.length ? -0.45 + i * 0.55 : 0;
    bloque.position.set(x, liquidos.length ? -1.4 + lado / 2 + 0.02 : -0.85 + lado / 2, 0);
    bloque.userData.info = {
      titulo: c.n ? `Bloque de ${c.n}` : 'Bloque',
      detalle: 'Cambia de temperatura hasta igualar a los demás: Q = m c ΔT.',
      formula: 'Q = m c (T_f − T_i)',
    };
    g.add(bloque);
    tags.push({ id: `sol${i}`, pos: [x, bloque.position.y + lado / 2 + 0.22, 0.4], texto: c.n || 'bloque', color: 'rojo' });
  });

  if (!liquidos.length) {
    // Un solo cuerpo que recibe calor (como Q020): placa caliente y flecha de calor que entra.
    const placa = new THREE.Mesh(new THREE.BoxGeometry(1.6, 0.12, 1.2), crearMaterial('rojo', { emisivo: 'rojo', intensidadEmisiva: 0.2 }));
    placa.position.y = -0.92;
    g.add(placa);
    g.add(crearFlecha3D([0.95, -1.05, 0.4], [0.35, -0.55, 0.4], 'rojo'));
    tags.push({ id: 'q', pos: [1.3, -1.1, 0.4], texto: 'Q entra', color: 'rojo' });
    anclas.equilibrio = [0, 0.35, 0];
  }

  hielos.forEach((c, i) => {
    const queda = seDerrite ? Math.max(0.06, 1 - u) : Math.max(0.45, 1 - 0.55 * u);
    const lado = (0.38 + 0.25 * Math.cbrt(Math.max(1, Number(c.m || 150)) / 150)) * Math.cbrt(queda);
    const hielo = new THREE.Mesh(new THREE.BoxGeometry(lado, lado, lado), crearMaterial('azul', { opacidad: 0.62 }));
    hielo.position.set(0.62 + i * 0.4, 0.15, 0);
    hielo.userData.info = {
      titulo: 'Hielo',
      detalle: 'Primero se calienta hasta 0 °C, después se derrite (L_f) y recién entonces el agua que forma se calienta.',
      formula: 'Q = m c_h ΔT + m L_f + m c_a ΔT',
    };
    g.add(hielo);
    tags.push({ id: `hie${i}`, pos: [0.62 + i * 0.4, 0.15 + lado / 2 + 0.22, 0.3], texto: u > 0.9 && seDerrite ? 'hielo derretido' : (c.n || 'hielo'), color: 'azul' });
  });

  if (u < 0.85 && (liquidos.length || hielos.length) && (solidos.length || hielos.length)) {
    g.add(crearFlecha3D([-0.15, -0.1, 0.3], [0.45, 0.05, 0.3], 'rojo'));
  }
  tags.push({
    id: 'eq',
    pos: [0, 1.15, 0],
    texto: u > 0.8 ? 'Equilibrio: todos a la misma T_f' : (liquidos.length ? 'Recipiente aislado: ΣQ = 0' : 'El bloque se calienta: Q = mcΔT'),
    activo: foco === 'equilibrio',
  });
  return {
    tags,
    medida: liquidos.length
      ? 'Lo que cede lo caliente lo gana lo frío (ΣQ = 0); el hielo primero llega a 0 °C y después se derrite.'
      : 'El calor que entra sube la temperatura: Q = m c ΔT.',
    anclas,
  };
}
