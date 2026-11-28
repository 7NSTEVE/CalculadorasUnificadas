/**
 * Escenas 3D interactivas del Parcial 1 (FI2103):
 *  1. Resorte 3D + Círculo fasorial de fase φ + Vectores v(t) y F_x=-kx
 *  2. Péndulo simple y péndulo físico 3D (pivote, centro de masa d, comparación lado a lado)
 *  3. Cuerda vibrante 3D (armónicos n=1..5, nodos, antinodos, traste, onda viajera y pulsos)
 *  4. Frentes de onda sonora esférica 3D (fuente puntual / dron, ley del inverso del cuadrado)
 */

import { num } from '../visuales/base.js';
import {
  THREE,
  crearFlecha3D,
  crearLinea3D,
  crearMaterial,
  crearPisoSutil,
  crearResorte3D,
} from './motor_3d.js';

const TAU = Math.PI * 2;

export function estadoResorte(p, t, extra = {}) {
  const k = p.k || (p.cruce && p.energia / p.cruce ** 2);
  const exacto = Boolean(k && p.masa);
  const aBase = p.amplitud || (p.energia && k && Math.sqrt(2 * p.energia / k)) || 1;
  const a = aBase * (extra.escalaA ?? 1);
  const inicial = (p.x0 ?? (aBase * (p.fraccion ?? 1))) / aBase;
  const fase = (p.sentido === 1 ? -1 : 1) * Math.acos(Math.min(1, Math.max(-1, inicial)));
  const ang = TAU * t + fase;
  const x = a * Math.cos(ang);
  const energia = exacto ? 0.5 * k * a * a : 1;
  const U = energia * (x / a) ** 2;
  const periodo = exacto ? TAU * Math.sqrt(p.masa / k) : null;
  return {
    x, a, U, K: Math.max(0, energia - U), energia, periodo,
    tiempo: periodo ? t * periodo : null,
    ang, fase, v: -Math.sin(ang), exacto,
  };
}

/** 1. Masa-resorte 3D con círculo fasorial sincronizado. */
export function construirResorte3D(motor, p, t, foco, extra = {}) {
  const e = estadoResorte(p, t, extra);
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();

  g.add(crearPisoSutil(-1.55));

  if (p.vertical) {
    const z = 1.15 * (e.x / e.a);
    const techo = new THREE.Mesh(
      new THREE.BoxGeometry(2.0, 0.18, 1.0),
      crearMaterial('tinta3', { opacidad: 0.75, corte }),
    );
    techo.position.set(0, 1.85, 0);
    g.add(techo);

    g.add(crearResorte3D([0, 1.76, 0], [0, z + 0.32, 0], {
      vueltas: 10, radioHelice: 0.22, nombreColor: foco === 'resorte' ? 'morado' : 'tinta2',
      info: { titulo: 'Resorte (k)', detalle: 'Fuerza restauradora proporcional al estiramiento.', formula: 'F_x = −kx' },
    }));

    const bloque = new THREE.Mesh(
      new THREE.BoxGeometry(0.76, 0.64, 0.72),
      crearMaterial(foco === 'masa' ? 'morado' : 'azul', { opacidad: 0.92, corte }),
    );
    bloque.position.set(0, z, 0);
    bloque.userData.info = { titulo: 'Bloque (m)', detalle: 'Oscila alrededor del equilibrio.', formula: 'ω² = k/m' };
    g.add(bloque);

    // Plano de equilibrio
    const planoEq = new THREE.Mesh(
      new THREE.PlaneGeometry(2.6, 1.4),
      crearMaterial(foco === 'equilibrio' ? 'morado' : 'gris', { opacidad: 0.18, dobleCara: true }),
    );
    planoEq.rotation.x = -Math.PI / 2;
    g.add(planoEq);

    if (Math.abs(e.v) > 0.08) {
      g.add(crearFlecha3D([0.58, z, 0], [0.58, z + Math.sign(e.v) * 0.72, 0], 'verde', {
        info: { titulo: 'Velocidad v(t)', detalle: 'Máxima al pasar por el equilibrio; cero en los extremos.', formula: 'v = −ωA sen(ωt+φ)' },
      }));
    }
    tags.push(
      { id: 'eq', pos: [-1.45, 0.05, 0], texto: 'Equilibrio (x=0)', activo: foco === 'equilibrio' },
      { id: 'masa', pos: [0.95, z, 0], texto: 'Masa', activo: foco === 'masa' },
    );
  } else {
    const dibujarPista = (yBase, ampVis, nombre, doble = false) => {
      const xVis = (e.x / e.a) * ampVis;
      // Riel y pared fija
      const riel = new THREE.Mesh(
        new THREE.BoxGeometry(5.1, 0.14, 1.05),
        crearMaterial('borde', { opacidad: 0.82, corte }),
      );
      riel.position.set(-0.05, yBase - 0.44, 0);
      const pared = new THREE.Mesh(
        new THREE.BoxGeometry(0.18, 1.05, 1.05),
        crearMaterial('tinta3', { opacidad: 0.85, corte }),
      );
      pared.position.set(-2.55, yBase + 0.02, 0);
      g.add(riel, pared);

      g.add(crearResorte3D([-2.46, yBase, 0], [xVis - 0.38, yBase, 0], {
        vueltas: 11, radioHelice: 0.21,
        nombreColor: foco === 'resorte' ? 'morado' : 'tinta2',
        info: { titulo: 'Resorte elástico', detalle: 'Almacena energía potencial U = ½kx².', formula: 'F_x = −kx' },
      }));

      const bloque = new THREE.Mesh(
        new THREE.BoxGeometry(0.76, 0.70, 0.74),
        crearMaterial(doble ? 'turquesa' : (foco === 'masa' ? 'morado' : 'azul'), { opacidad: 0.94, corte }),
      );
      bloque.position.set(xVis, yBase - 0.02, 0);
      bloque.userData.info = {
        titulo: nombre,
        detalle: e.exacto ? `x = ${num(e.x, 3)} m · U = ${num(e.U, 3)} J · K = ${num(e.K, 3)} J` : 'Oscilador armónico simple.',
        formula: 'x(t) = A cos(ωt + φ)',
      };
      g.add(bloque);

      // Marca de equilibrio y extremos -A, +A
      g.add(crearLinea3D([[0, yBase - 0.36, 0.54], [0, yBase + 0.65, 0.54]], foco === 'equilibrio' ? 'morado' : 'gris', { punteada: true }));
      g.add(crearLinea3D([[-ampVis, yBase - 0.36, 0.54], [ampVis, yBase - 0.36, 0.54]], 'naranja'));

      if (Math.abs(e.v) > 0.08) {
        g.add(crearFlecha3D([xVis, yBase + 0.48, 0.15], [xVis + Math.sign(e.v) * 0.78, yBase + 0.48, 0.15], 'verde', {
          info: { titulo: 'Velocidad v(t)', detalle: 'El signo de v(0) decide el cuadrante correcto de la fase φ.', formula: 'v = −ωA sen(ωt + φ)' },
        }));
      }
      if (Math.abs(xVis) > 0.12) {
        g.add(crearFlecha3D([xVis, yBase - 0.02, 0.48], [xVis - Math.sign(xVis) * 0.66, yBase - 0.02, 0.48], 'rojo', {
          info: { titulo: 'Fuerza restauradora F_x', detalle: 'Siempre apunta hacia el punto de equilibrio x = 0.', formula: 'F_x = −kx' },
        }));
      }
      tags.push({ id: `m_${nombre}`, pos: [xVis, yBase + 0.76, 0], texto: nombre, activo: foco === 'masa' });
    };

    if (p.comparar) {
      dibujarPista(0.75, 0.85, 'Amplitud A', false);
      dibujarPista(-0.62, 1.70, 'Amplitud 2A', true);
    } else {
      dibujarPista(0.28, 1.35, 'Masa m', false);
      // Círculo fasorial 3D detrás/abajo mostrando el ángulo de fase φ
      const rF = 0.76;
      const cy = -0.96;
      const cz = -0.78;
      const aro = new THREE.Mesh(
        new THREE.TorusGeometry(rF, 0.022, 10, 48),
        crearMaterial(foco === 'equilibrio' ? 'morado' : 'tinta3', { opacidad: 0.55 }),
      );
      aro.position.set(0, cy, cz);
      g.add(aro);
      const px = rF * Math.cos(e.ang);
      const py = cy + rF * Math.sin(e.ang);
      g.add(crearFlecha3D([0, cy, cz], [px, py, cz], 'morado', {
        radio: 0.03, punta: 0.16,
        info: { titulo: 'Fasor de fase (ωt + φ)', detalle: 'Su proyección horizontal es exactamente x(t) = A cos(ωt + φ).', formula: 'tan φ = −v_0/(ω x_0)' },
      }));
      g.add(crearLinea3D([[px, py, cz], [(e.x / e.a) * 1.35, 0.28, 0]], 'morado', { punteada: true, opacidad: 0.6 }));
      tags.push(
        { id: 'fasor', pos: [px + 0.35, py, cz], texto: 'ωt+φ', activo: foco === 'equilibrio', color: 'morado' },
        { id: 'eq0', pos: [0, 0.85, 0.54], texto: 'x = 0', activo: foco === 'equilibrio' },
        { id: 'extA', pos: [1.38, -0.2, 0.55], texto: '+A' },
        { id: 'extmA', pos: [-1.38, -0.2, 0.55], texto: '−A' },
      );
    }
  }

  const suf = e.exacto ? ' J' : ' E';
  const med = (e.exacto ? `x = ${num(e.x, 3)} m · ` : '')
    + `U = ${num(e.U, 3)}${suf} · K = ${num(e.K, 3)}${suf}`
    + (e.periodo ? ` · T = ${num(e.periodo, 3)} s` : '');
  return { tags, medida: med, energia: { K: e.K, U: e.U, E: e.energia, x: e.x / e.a } };
}

/** 2. Péndulo simple y péndulo físico 3D. */
export function construirPendulo3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const pares = {
    largo_cuatro: { largo: 1.9, ref: 0.65, periodo: 2.0, titulo: '4L (T = 2T₀)', centro: 1 },
    largo_cuarto: { largo: 0.55, ref: 2.05, periodo: 0.5, titulo: 'L/4 (T = T₀/2)', centro: 1 },
    centro_cuarto: { largo: 1.85, ref: 1.85, periodo: 2.0, titulo: 'd/4 (físico)', centro: 0.25 },
    fisico: { largo: 1.95, ref: 1.95, periodo: Math.sqrt(8 / 9), titulo: 'Varilla + esfera', centro: 0.75 },
  };
  const otro = pares[p.comparar];
  const refL = otro ? otro.ref : 2.05;

  g.add(crearPisoSutil(-1.45));

  const armarUno = (x0, largo, factorT, titulo, centroRel = 1, colorEsfera = 'azul') => {
    const piv = [x0, 1.45, 0];
    const theta = 0.34 * Math.sin(TAU * t / factorT);
    const fin = [x0 + largo * Math.sin(theta), 1.45 - largo * Math.cos(theta), 0];
    const cm = [
      piv[0] + (fin[0] - piv[0]) * centroRel,
      piv[1] + (fin[1] - piv[1]) * centroRel,
      0,
    ];

    const soporte = new THREE.Mesh(
      new THREE.BoxGeometry(0.9, 0.16, 0.6),
      crearMaterial('tinta3', { opacidad: 0.82 }),
    );
    soporte.position.set(x0, 1.56, 0);
    g.add(soporte);

    const pivMesh = new THREE.Mesh(
      new THREE.SphereGeometry(0.085, 16, 16),
      crearMaterial(foco === 'pivote' ? 'morado' : 'tinta'),
    );
    pivMesh.position.set(...piv);
    pivMesh.userData.info = { titulo: 'Pivote (eje O)', detalle: 'La inercia I se mide respecto a este eje (Steiner).', formula: 'I = I_cm + md²' };
    g.add(pivMesh);

    const rVar = centroRel === 1 ? 0.025 : 0.068;
    const dir = new THREE.Vector3(fin[0] - piv[0], fin[1] - piv[1], 0);
    const varilla = new THREE.Mesh(
      new THREE.CylinderGeometry(rVar, rVar, largo, 14),
      crearMaterial(centroRel === 1 ? 'tinta2' : 'gris', { metal: 0.35 }),
    );
    varilla.position.set((piv[0] + fin[0]) / 2, (piv[1] + fin[1]) / 2, 0);
    varilla.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.clone().normalize());
    g.add(varilla);

    const bola = new THREE.Mesh(
      new THREE.SphereGeometry(0.24, 24, 24),
      crearMaterial(colorEsfera, { metal: 0.18, rugosidad: 0.32 }),
    );
    bola.position.set(...fin);
    bola.userData.info = { titulo, detalle: 'El período depende de L y g (o de I y d en el físico), no de la masa en el simple.', formula: 'ω² = g/L  ·  ω² = mgd/I' };
    g.add(bola);

    // Centro de masa iluminado
    const cmMesh = new THREE.Mesh(
      new THREE.SphereGeometry(0.095, 16, 16),
      crearMaterial('morado', { emisivo: 'morado', intensidadEmisiva: 0.35 }),
    );
    cmMesh.position.set(...cm);
    cmMesh.userData.info = { titulo: 'Centro de masa (d)', detalle: 'Distancia d medida desde el pivote hasta el centro de masa.', formula: 'T = 2π√(I / mgd)' };
    g.add(cmMesh);

    // Línea de referencia vertical + arco
    g.add(crearLinea3D([piv, [x0, 1.45 - largo - 0.15, 0]], 'gris', { punteada: true }));
    const arco = Array.from({ length: 25 }, (_, i) => {
      const a = -0.34 + (0.68 * i) / 24;
      return [x0 + largo * Math.sin(a), 1.45 - largo * Math.cos(a), 0];
    });
    g.add(crearLinea3D(arco, 'azul', { opacidad: 0.45 }));

    tags.push({ id: `p_${x0}`, pos: [x0, 1.85, 0], texto: titulo, activo: foco === 'largo' });
    if (foco === 'centro' || centroRel < 1) {
      tags.push({ id: `cm_${x0}`, pos: [cm[0] + 0.55, cm[1], 0], texto: 'CM (d)', activo: foco === 'centro', color: 'morado' });
    }
  };

  armarUno(otro ? -1.35 : 0, refL, 1.0, otro ? 'Referencia' : 'Péndulo simple', 1, 'azul');
  if (otro) armarUno(1.35, otro.largo, otro.periodo, otro.titulo, otro.centro, 'turquesa');

  const med = p.largo
    ? `L = ${num(p.largo, 3)} m · d_CM = ${num(p.centro ?? p.largo, 3)} m`
    : 'El punto violeta indica el centro de masa (distancia d al pivote).';
  return { tags, medida: med };
}

/** 3. Cuerda vibrante y ondas 3D (estacionaria, viajera, pulsos, comparación). */
export function construirCuerda3D(motor, p, t, foco, extra = {}) {
  const g = motor.raiz;
  const tags = [];
  const tipo = p.onda || 'estacionaria';
  const n = Math.max(1, Math.round(extra.armonico ?? p.armonico ?? 2));
  const Lvis = 4.6;
  const x0 = -Lvis / 2;

  g.add(crearPisoSutil(-1.45));

  const dibujarCuerda = (fn, yOff, col, nombre, conNodos = false) => {
    // Soportes en los extremos
    for (const sx of [x0, -x0]) {
      const poste = new THREE.Mesh(
        new THREE.CylinderGeometry(0.06, 0.08, 0.72, 12),
        crearMaterial('tinta3', { opacidad: 0.85 }),
      );
      poste.position.set(sx, yOff, 0);
      g.add(poste);
    }
    g.add(crearLinea3D([[x0, yOff, 0], [-x0, yOff, 0]], 'gris', { punteada: true, opacidad: 0.55 }));

    const pts = Array.from({ length: 72 }, (_, i) => {
      const u = i / 71;
      return new THREE.Vector3(x0 + u * Lvis, yOff + fn(u), 0);
    });
    const tubo = new THREE.Mesh(
      new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 72, 0.042, 10, false),
      crearMaterial(col, { rugosidad: 0.35, emisivo: col, intensidadEmisiva: 0.12 }),
    );
    tubo.userData.info = {
      titulo: nombre,
      detalle: tipo === 'estacionaria' ? `Armónico n = ${n}: λ_n = 2L/${n}, f_n = ${n}·f_1.` : 'Rapidez de onda v = √(F/μ) = λf = ω/k.',
      formula: tipo === 'estacionaria' ? 'y(x,t) = A_OE sen(kx) sen(ωt)' : 'v = √(F/μ) = λf',
    };
    g.add(tubo);

    if (conNodos) {
      for (let i = 0; i <= n; i += 1) {
        const nx = x0 + (Lvis * i) / n;
        const nodo = new THREE.Mesh(
          new THREE.SphereGeometry(0.095, 16, 16),
          crearMaterial(foco === 'nodos' ? 'morado' : 'rojo'),
        );
        nodo.position.set(nx, yOff, 0);
        nodo.userData.info = { titulo: `Nodo ${i + 1}`, detalle: 'Punto que nunca se mueve. Dos nodos seguidos están a λ/2.', formula: 'Δx_nodos = λ/2' };
        g.add(nodo);
      }
      // Antinodo central con vector de rapidez transversal
      const ax = x0 + Lvis / (2 * n);
      const ay = yOff + fn(1 / (2 * n));
      g.add(crearFlecha3D([ax, yOff, 0.18], [ax, ay, 0.18], 'verde', {
        radio: 0.028, punta: 0.14,
        info: { titulo: 'Antinodo (vientre)', detalle: 'Oscila con amplitud máxima A_OE y rapidez transversal máxima ω·A_OE.', formula: 'v_y,máx = ω A_OE' },
      }));
      tags.push(
        { id: 'nodo0', pos: [x0 + Lvis / n, yOff - 0.34, 0], texto: 'Nodo', activo: foco === 'nodos', color: 'rojo' },
        { id: 'anti0', pos: [ax, yOff + 0.92, 0], texto: 'Antinodo', activo: foco === 'nodos', color: 'verde' },
      );
      if (p.traste) {
        const traste = new THREE.Mesh(
          new THREE.BoxGeometry(0.08, 0.55, 0.45),
          crearMaterial('naranja'),
        );
        traste.position.set(x0 + 0.72, yOff - 0.12, 0);
        g.add(traste);
        tags.push({ id: 'traste', pos: [x0 + 0.72, yOff - 0.58, 0], texto: 'Traste', activo: foco === 'extremos', color: 'naranja' });
      }
    }
  };

  if (tipo === 'pulsos') {
    const f1 = (u) => 0.65 * Math.exp(-(((u - t) / 0.08) ** 2));
    const f2 = (u) => -0.65 * Math.exp(-(((u - (1 - t)) / 0.08) ** 2));
    dibujarCuerda(f1, 1.05, 'azul', 'Pulso 1');
    dibujarCuerda(f2, 0.0, 'rojo', 'Pulso 2');
    dibujarCuerda((u) => f1(u) + f2(u), -1.05, 'morado', 'Suma (superposición)');
    tags.push(
      { id: 'p1', pos: [-2.1, 1.45, 0], texto: 'Pulso 1' },
      { id: 'p2', pos: [-2.1, 0.4, 0], texto: 'Pulso 2' },
      { id: 'ps', pos: [-2.1, -0.65, 0], texto: 'Suma' },
    );
    return { tags, medida: 'Superposición: en cada punto se suman los desplazamientos con su signo.' };
  }

  if (tipo === 'viajera') {
    const k = p.k || 4;
    dibujarCuerda((u) => 0.68 * Math.cos(k * u * Math.PI - TAU * t), 0.1, 'azul', 'Onda viajera');
    g.add(crearFlecha3D([-0.6, 1.02, 0], [0.7, 1.02, 0], 'verde'));
    tags.push({ id: 'v', pos: [0.05, 1.25, 0], texto: 'v = ω/k', activo: foco === 'onda', color: 'verde' });
    return { tags, medida: `Onda viajera · A = ${num(p.amplitud || 0.025, 3)} m · k = ${k} rad/m · ω = ${p.omega || 50} rad/s` };
  }

  if (tipo === 'comparacion') {
    const [f1, f2] = p.frecuencias || [1, 1];
    const [a1, a2] = p.amplitudes || [0.85, 0.55];
    const esc = p.tensiones ? Math.sqrt(p.tensiones[1] / p.densidades[1]) : 1;
    dibujarCuerda((u) => 0.52 * a1 * Math.sin(TAU * f1 * (u - t)), 0.72, 'azul', 'Onda 1');
    dibujarCuerda((u) => 0.52 * a2 * Math.sin(TAU * f2 * (u / esc - t)), -0.68, 'rojo', 'Onda 2');
    tags.push({ id: 'o1', pos: [-2.0, 1.22, 0], texto: 'Onda 1' }, { id: 'o2', pos: [-2.0, -0.18, 0], texto: 'Onda 2' });
    return { tags, medida: 'Compara longitudes de onda, frecuencias y amplitudes entre ambas cuerdas.' };
  }

  // Estacionaria (+ descomposición tenue en 2 ondas viajeras opuestas cuando el foco es 'onda')
  dibujarCuerda((u) => 0.78 * Math.sin(n * Math.PI * u) * Math.cos(TAU * t), 0.1, 'azul', `Onda estacionaria (n=${n})`, true);
  if (foco === 'onda') {
    const ptsDer = Array.from({ length: 64 }, (_, i) => {
      const u = i / 63;
      return [x0 + u * Lvis, 0.1 + 0.39 * Math.sin(n * Math.PI * u - TAU * t), 0.35];
    });
    const ptsIzq = Array.from({ length: 64 }, (_, i) => {
      const u = i / 63;
      return [x0 + u * Lvis, 0.1 + 0.39 * Math.sin(n * Math.PI * u + TAU * t), -0.35];
    });
    g.add(crearLinea3D(ptsDer, 'naranja', { opacidad: 0.75 }), crearLinea3D(ptsIzq, 'turquesa', { opacidad: 0.75 }));
  }

  return {
    tags,
    medida: `Armónico n = ${n} (${n} ${n === 1 ? 'vientre' : 'vientres'} · ${n + 1} nodos)`
      + (p.nodos_distancia ? ` · Δx = ${num(p.nodos_distancia, 3)} m` : ''),
  };
}

/** 4. Frentes de onda sonora esférica 3D (ley del inverso del cuadrado). */
export function construirSonido3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const dMax = p.distancia || 7.5;
  const i1 = p.intensidad || 0.11;
  const iUmb = p.umbral || 1.0;
  const rCerc = dMax * Math.sqrt(i1 / iUmb);
  const rAct = rCerc + t * (dMax - rCerc);
  const rVis = 0.55 + 1.45 * (rAct / dMax);

  g.add(crearPisoSutil(-1.75));

  // Fuente puntual (dron)
  const fuente = new THREE.Mesh(
    new THREE.OctahedronGeometry(0.22, 1),
    crearMaterial(foco === 'fuente' ? 'morado' : 'naranja', { emisivo: 'naranja', intensidadEmisiva: 0.3 }),
  );
  fuente.userData.info = { titulo: 'Fuente puntual (P)', detalle: 'Emite potencia sonora P uniforme en las 3 direcciones.', formula: 'I = P / (4πr²)' };
  g.add(fuente);

  // Cascarón esférico de referencia r_1 y cascarón móvil r
  const esf1 = new THREE.Mesh(
    new THREE.SphereGeometry(2.0, 28, 22),
    crearMaterial('azul', { opacidad: 0.12, dobleCara: true, corte }),
  );
  const esfAct = new THREE.Mesh(
    new THREE.SphereGeometry(rVis, 32, 24),
    crearMaterial('morado', { opacidad: 0.24, dobleCara: true, corte }),
  );
  esfAct.userData.info = {
    titulo: `Frente de onda a r = ${num(rAct, 3)} m`,
    detalle: 'La misma potencia P se reparte en un área esférica 4πr².',
    formula: 'I_1/I_2 = r_2²/r_1²',
  };
  g.add(esf1, esfAct);

  // Radio r y receptor
  g.add(crearFlecha3D([0, 0, 0], [rVis, 0, 0], 'morado'));
  const sensor = new THREE.Mesh(new THREE.SphereGeometry(0.11, 14, 14), crearMaterial('verde'));
  sensor.position.set(rVis, 0, 0);
  g.add(sensor);

  const Iact = i1 * (dMax / rAct) ** 2;
  tags.push(
    { id: 'f', pos: [0, -0.42, 0], texto: 'Fuente (P)', activo: foco === 'fuente', color: 'naranja' },
    { id: 'r', pos: [rVis * 0.55, 0.24, 0], texto: `r = ${num(rAct, 3)} m`, activo: foco === 'onda', color: 'morado' },
    { id: 'i', pos: [rVis + 0.35, -0.3, 0], texto: `I = ${num(Iact, 3)} W/m²`, color: 'verde' },
  );
  return { tags, medida: `r = ${num(rAct, 3)} m · Área 4πr² = ${num(4 * Math.PI * rAct * rAct, 4)} m² · I = ${num(Iact, 3)} W/m²` };
}
