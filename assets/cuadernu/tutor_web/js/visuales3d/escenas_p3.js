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

/** 9. Sólidos en conducción 3D con plano de corte y elemento diferencial dA / dr / dy. */
export function construirSolido3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const geo = p.geo || 'pared';

  g.add(crearPisoSutil(-1.68));

  if (geo === 'dilatacion') {
    return construirDilatacion3D(motor, p, t, foco);
  }

  // Esfera seccionada (O26-8): sólido de revolución LatheGeometry + disco horizontal brillante + Pitágoras 3D
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
      const r = Math.sqrt(Math.max(0.001, Rvis * Rvis - y * y));
      pts.push(new THREE.Vector2(r, y));
    }
    const cuerpo = new THREE.Mesh(
      new THREE.LatheGeometry(pts, 36),
      crearMaterial('gris', { opacidad: 0.34, dobleCara: true, corte }),
    );
    cuerpo.userData.info = {
      titulo: 'Esfera cortada (−L ≤ y ≤ L)',
      detalle: 'El calor viaja de la cara plana caliente a la fría; el área del disco varía con y.',
      formula: 'A(y) = πr² = π(R² − y²)',
    };
    g.add(cuerpo);

    // Tapas caliente (arriba o abajo) y fría
    const rTapa = Math.sqrt(Math.max(0.01, Rvis * Rvis - Lvis * Lvis));
    const tapa1 = new THREE.Mesh(new THREE.CylinderGeometry(rTapa, rTapa, 0.05, 32), crearMaterial('rojo', { opacidad: 0.82, corte }));
    tapa1.position.y = -Lvis;
    const tapa2 = new THREE.Mesh(new THREE.CylinderGeometry(rTapa, rTapa, 0.05, 32), crearMaterial('azul', { opacidad: 0.82, corte }));
    tapa2.position.y = Lvis;
    g.add(tapa1, tapa2);

    // Disco diferencial brillante a altura y
    const yDatos = -L + 2 * L * t;
    const yVis = yDatos * esc;
    const rDisco = Math.sqrt(Math.max(0.01, Rvis * Rvis - yVis * yVis));
    const disco = new THREE.Mesh(
      new THREE.CylinderGeometry(rDisco, rDisco, 0.09, 32),
      crearMaterial(foco === 'corte' ? 'naranja' : 'morado', { opacidad: 0.86, emisivo: 'naranja', intensidadEmisiva: 0.25 }),
    );
    disco.position.y = yVis;
    disco.userData.info = {
      titulo: `Disco de grosor dy en y = ${num(yDatos, 3)} m`,
      detalle: 'Por Pitágoras desde el centro O: r² + y² = R² ⇒ A(y) = π(R² − y²).',
      formula: 'H = −k π(R² − y²) dT/dy',
    };
    g.add(disco);

    // Triángulo rectángulo de Pitágoras 3D (centro O -> eje y -> borde del disco)
    g.add(
      crearLinea3D([[0, 0, 0.06], [0, yVis, 0.06]], 'verde'),
      crearLinea3D([[0, yVis, 0.06], [rDisco, yVis, 0.06]], 'naranja'),
      crearLinea3D([[0, 0, 0.06], [rDisco, yVis, 0.06]], 'morado'),
    );
    g.add(crearFlecha3D([Rvis + 0.45, -Lvis, 0], [Rvis + 0.45, Lvis, 0], 'rojo'));

    const areaReal = Math.PI * Math.max(0, R * R - yDatos * yDatos);
    tags.push(
      { id: 'disc', pos: [rDisco * 0.6, yVis + 0.24, 0.12], texto: 'r = √(R²−y²)', activo: foco === 'corte', color: 'naranja' },
      { id: 'hip', pos: [rDisco * 0.45, yVis * 0.42, 0.12], texto: 'R', color: 'morado' },
      { id: 't1', pos: [-rTapa - 0.35, -Lvis, 0], texto: `T₁ = ${num(p.T1 ?? 100, 3)} °C`, activo: foco === 'limites', color: 'rojo' },
      { id: 't2', pos: [-rTapa - 0.35, Lvis, 0], texto: `T₂ = ${num(p.T2 ?? 20, 3)} °C`, activo: foco === 'limites', color: 'azul' },
      { id: 'fl', pos: [Rvis + 0.8, 0, 0], texto: 'Flujo H', activo: foco === 'calor', color: 'rojo' },
    );
    return { tags, medida: `y = ${num(yDatos, 4)} m · A(y) = π(R²−y²) = ${num(areaReal, 4)} m²` };
  }

  // Tronco de cono (T-CONO)
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
    cono.userData.info = { titulo: 'Tronco de cono', detalle: 'El radio crece linealmente: r(x) = r₁ + (r₂−r₁)·x/L.', formula: 'A(x) = π r(x)²' };
    g.add(cono);

    const xVis = -Lvis / 2 + t * Lvis;
    const rVis = r1v + (r2v - r1v) * t;
    const disco = new THREE.Mesh(
      new THREE.CylinderGeometry(rVis, rVis, 0.09, 32),
      crearMaterial('naranja', { opacidad: 0.9, emisivo: 'naranja', intensidadEmisiva: 0.25 }),
    );
    disco.rotation.z = -Math.PI / 2;
    disco.position.x = xVis;
    g.add(disco);
    g.add(crearFlecha3D([-Lvis / 2, -1.28, 0], [Lvis / 2, -1.28, 0], 'rojo'));

    const rReal = r1 + (r2 - r1) * t;
    tags.push(
      { id: 'c', pos: [xVis, rVis + 0.28, 0], texto: `Disco A(x) = πr²`, activo: foco === 'corte', color: 'naranja' },
      { id: 't1', pos: [-Lvis / 2, -r1v - 0.32, 0], texto: `r₁ (${num(p.T1 ?? 100, 3)} °C)`, color: 'rojo' },
      { id: 't2', pos: [Lvis / 2, -r2v - 0.32, 0], texto: `r₂ (${num(p.T2 ?? 20, 3)} °C)`, color: 'azul' },
    );
    return { tags, medida: `x = ${num(t * L, 3)} m · r(x) = ${num(rReal, 4)} m · A(x) = ${num(Math.PI * rReal * rReal, 4)} m²` };
  }

  // Cascarón cilíndrico radial (Q045 / tubo)
  if (geo === 'cilindro' || geo === 'cilindro2') {
    const r1 = Number(p.r1 || 0.04);
    const r2 = Number(p.r2 || 0.08);
    const rVis1 = 0.46;
    const rVis2 = 1.18;
    const rActVis = rVis1 + t * (rVis2 - rVis1);

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
      titulo: 'Cascarón cilíndrico (dr)',
      detalle: 'El calor fluye radialmente hacia afuera atravesando el área lateral 2πrL.',
      formula: 'H = −k (2πrL) dT/dr ⇒ ln(r₂/r₁)',
    };
    g.add(ext, intC, cascaron);

    g.add(crearFlecha3D([0, rVis1, 0], [0, rVis2 + 0.35, 0], 'rojo'));
    const rReal = r1 + t * (r2 - r1);
    tags.push(
      { id: 'cas', pos: [0, rActVis + 0.28, 0.2], texto: `Cascarón A(r) = 2πrL`, activo: foco === 'corte', color: 'naranja' },
      { id: 'fl', pos: [0.4, rVis2 + 0.38, 0], texto: 'H radial', activo: foco === 'calor', color: 'rojo' },
    );
    return { tags, medida: `Radio r = ${num(rReal, 4)} m · Al integrar dr/r sale ln(r₂/r₁).` };
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
    lamina.position.x = -Lvis / 2 + t * Lvis;
    lamina.userData.info = {
      titulo: 'Sección transversal (dx o dy)',
      detalle: 'Si k o A cambian con la posición, separa variables e integra la ley de Fourier.',
      formula: 'H = −k(y) A dT/dy',
    };
    g.add(bloque, caraCal, caraFri, lamina);
    tags.push({ id: 'sec', pos: [lamina.position.x, 1.12, 0], texto: 'Corte dx / dy', activo: foco === 'corte', color: 'naranja' });
  }
  g.add(crearFlecha3D([-Lvis / 2, -1.25, 0], [Lvis / 2, -1.25, 0], 'rojo'));
  tags.push(
    { id: 't1', pos: [-Lvis / 2, -1.52, 0], texto: p.T1 !== undefined ? `${num(p.T1, 3)} °C` : 'T_H (caliente)', color: 'rojo' },
    { id: 't2', pos: [Lvis / 2, -1.52, 0], texto: p.T2 !== undefined ? `${num(p.T2, 3)} °C` : 'T_C (fría)', color: 'azul' },
  );
  return { tags, medida: 'El calor H fluye siempre de la cara caliente a la fría (H constante en estado estacionario).' };
}

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
    return { tags, medida: 'A mayor temperatura se activan los modos de vibración y C_V del diatómico aumenta.' };
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
    return { tags, medida: 'Ciclo Otto: e = 1 − 1 / r^(γ−1). A igual compresión r, mayor γ da mayor eficiencia.' };
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
    };
  }

  // Gas isocórico o libre
  const fracV = p.modo === 'isocorico' ? 0.55 : 0.28 + 0.65 * t;
  armarCilindro(0, fracV, t, p.modo === 'isocorico' ? 'Volumen constante (W = 0)' : 'Gas ideal en pistón', 'pV = nRT', t > 0.5 ? 'rojo' : 'azul');
  return { tags, medida: 'Al aumentar T, las moléculas chocan más rápido y sube la presión o se expande el pistón.' };
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

/** 12. Calorimetría (mezcla térmica) y Radiación térmica 3D (Stefan-Boltzmann T⁴). */
export function construirCalor3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  g.add(crearPisoSutil(-1.58));

  if (p.tipo === 'radiacion' || p.modo === 'radiacion') {
    const T = 300 + t * 500;
    const radEsc = 0.65 + 0.95 * ((T / 800) ** 2);
    const cuerpo = new THREE.Mesh(
      new THREE.SphereGeometry(0.62, 26, 20),
      crearMaterial(t > 0.5 ? 'rojo' : 'naranja', { emisivo: 'rojo', intensidadEmisiva: 0.15 + t * 0.55 }),
    );
    cuerpo.userData.info = { titulo: 'Cuerpo radiante', detalle: 'Emite energía electromagnética aunque esté en el vacío.', formula: 'H = A e σ T⁴ (T en Kelvin)' };
    const halo1 = new THREE.Mesh(
      new THREE.SphereGeometry(0.82 + 0.72 * radEsc, 24, 18),
      crearMaterial('naranja', { opacidad: 0.16, dobleCara: true, corte }),
    );
    g.add(cuerpo, halo1);
    tags.push({ id: 'rad', pos: [0, 1.42, 0], texto: `T = ${num(T, 3)} K → T⁴ crece rapidísimo`, color: 'rojo' });
    return { tags, medida: `Stefan-Boltzmann: H = AeσT⁴ · Si T se duplica (en K), H se multiplica por 16.` };
  }

  // Mezcla en calorímetro aislado (cuerpo caliente cede Q < 0, cuerpo frío gana Q > 0)
  const vaso = new THREE.Mesh(
    new THREE.CylinderGeometry(1.45, 1.45, 2.0, 28, 1, true),
    crearMaterial('borde', { opacidad: 0.25, dobleCara: true, corte }),
  );
  vaso.position.y = -0.2;
  const agua = new THREE.Mesh(
    new THREE.CylinderGeometry(1.38, 1.38, 1.45, 26),
    crearMaterial(t > 0.6 ? 'turquesa' : 'azul', { opacidad: 0.28, corte }),
  );
  agua.position.y = -0.45;
  const bloque = new THREE.Mesh(
    new THREE.BoxGeometry(0.76, 0.72, 0.76),
    crearMaterial(t > 0.7 ? 'turquesa' : 'rojo', { metal: 0.35 }),
  );
  bloque.position.set(-0.25, -0.58, 0);
  bloque.userData.info = { titulo: 'Cuerpo caliente', detalle: 'Cede calor (Q < 0) hasta igualar T_f con el resto.', formula: 'Σ Q = 0' };
  const hielo = new THREE.Mesh(
    new THREE.BoxGeometry(0.55 * (1 - 0.55 * t), 0.55 * (1 - 0.55 * t), 0.55 * (1 - 0.55 * t)),
    crearMaterial('azul', { opacidad: 0.62 }),
  );
  hielo.position.set(0.62, 0.12, 0);
  g.add(vaso, agua, bloque, hielo);

  if (t < 0.85) {
    g.add(crearFlecha3D([-0.15, -0.1, 0.3], [0.45, 0.05, 0.3], 'rojo'));
  }
  tags.push(
    { id: 'c1', pos: [-0.35, -1.25, 0], texto: 'Cede calor (Q < 0)', color: 'rojo' },
    { id: 'c2', pos: [0.68, 0.62, 0], texto: 'Absorbe calor (Q > 0)', color: 'azul' },
    { id: 'eq', pos: [0, 1.05, 0], texto: t > 0.8 ? 'Equilibrio térmico (T_f)' : 'Recipiente aislado (ΣQ = 0)', activo: foco === 'equilibrio' },
  );
  return { tags, medida: 'Calorimetría: lo que cede el cuerpo caliente lo absorbe el frío (ΣQ = 0).' };
}
