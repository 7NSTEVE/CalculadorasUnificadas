/**
 * Escenas 3D interactivas del Parcial 2 (FI2103):
 *  5. Represa y compuerta 3D (rectángulo, trapecio, parábola, mixta, semicírculo, ventana)
 *     con prisma 3D de presión hidrostática, franja brillante dA = L(h)dh y suma de N franjas.
 *  6. Tanque de flotación 3D (Arquímedes, comparación 2 líquidos, tensión de cable, Stokes).
 *  7. Tuberías 3D (Venturi con 2 columnas piezométricas, tanque/Torricelli, Pitot, descenso,
 *     ramas, perfil parabólico 3D de Poiseuille con partículas animadas).
 *  8. Manómetro multilíquido 3D (4 líquidos con planos de igual presión) + Tensión / Young 3D.
 */

import { num } from '../visuales/base.js';
import {
  THREE,
  crearFlecha3D,
  crearLinea3D,
  crearMaterial,
  crearPisoSutil,
} from './motor_3d.js';

const G = 9.8;

function alturaSumergidaEsfera(fraccion) {
  let a = 0;
  let b = 2;
  for (let i = 0; i < 24; i += 1) {
    const m = (a + b) / 2;
    if (m * m * (3 - m) / 4 < fraccion) a = m;
    else b = m;
  }
  return (a + b) / 2;
}

/** 5. Represa / compuerta 3D con prisma de presión y franja diferencial dA = L(h)·dh. */
export function construirCompuerta3D(motor, p, t, foco, extra = {}) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const forma = p.forma || 'rectangulo';
  const rho = Number(p.rho || 1000);
  const Hagua = Math.max(0.1, Number(p.H || p.alto || 4));
  const nFranjas = Math.round(extra.franjas || 0); // >0 activa modo suma de N franjas

  g.add(crearPisoSutil(-1.68));

  // Volumen de agua 3D translúcido detrás de la pared (z negativo hacia z=0.18)
  const agua = new THREE.Mesh(
    new THREE.BoxGeometry(3.8, 2.6, 1.55),
    crearMaterial('azul', { opacidad: 0.22, dobleCara: true, corte }),
  );
  agua.position.set(0, -0.22, -0.62);
  agua.userData.info = { titulo: 'Columna de agua', detalle: 'La presión aumenta linealmente con la profundidad h.', formula: 'p = ρgh' };
  g.add(agua);

  // Superficie libre brillante
  const sup = new THREE.Mesh(
    new THREE.PlaneGeometry(3.82, 1.56),
    crearMaterial('turquesa', { opacidad: 0.42, dobleCara: true }),
  );
  sup.rotation.x = -Math.PI / 2;
  sup.position.set(0, 1.08, -0.62);
  g.add(sup);

  // Muro contenedor semitransparente
  const muro = new THREE.Mesh(
    new THREE.BoxGeometry(3.84, 2.95, 0.26),
    crearMaterial('gris', { opacidad: 0.28, corte }),
  );
  muro.position.set(0, -0.1, 0.28);
  g.add(muro);

  // Perfil normalizado u in [0,1] (0 = fondo, 1 = borde superior de la compuerta)
  const yBot = -1.25;
  const yTop = forma === 'ventana' ? 0.25 : 0.95;
  const anchoMitad = (u) => {
    if (forma === 'trapecio') return 0.58 + 0.54 * u;
    if (forma === 'semicirculo') return 1.08 * Math.sqrt(Math.max(0, 1 - (1 - u) ** 2));
    if (forma === 'parabola') return 0.25 + 0.85 * Math.sqrt(Math.max(0, u));
    if (forma === 'mixta') return 0.95; // manejado asimétrico abajo
    return 0.84;
  };
  const bordes = (u) => {
    if (forma === 'mixta') return [-0.72, 0.86 * Math.sqrt(Math.max(0, u))];
    const w = anchoMitad(u);
    return [-w, w];
  };

  // Malla 2D extruida de la compuerta
  const shape = new THREE.Shape();
  const N = 32;
  for (let i = 0; i <= N; i += 1) {
    const u = i / N;
    const [, xR] = bordes(u);
    const y = yBot + u * (yTop - yBot);
    if (i === 0) shape.moveTo(xR, y);
    else shape.lineTo(xR, y);
  }
  for (let i = N; i >= 0; i -= 1) {
    const u = i / N;
    const [xL] = bordes(u);
    const y = yBot + u * (yTop - yBot);
    shape.lineTo(xL, y);
  }
  shape.closePath();
  const compMesh = new THREE.Mesh(
    new THREE.ExtrudeGeometry(shape, { depth: 0.08, bevelEnabled: false }),
    crearMaterial('azul', { opacidad: 0.52, rugosidad: 0.45 }),
  );
  compMesh.position.set(0, 0, 0.41);
  compMesh.userData.info = {
    titulo: `Compuerta (${forma})`,
    detalle: 'Como la presión cambia con h, cortamos en franjas horizontales donde h es casi constante.',
    formula: 'F = ∫ p dA = ∫ ρg h L(h) dh',
  };
  g.add(compMesh);

  // Prisma 3D de flechas de presión hidrostática creciendo con la profundidad
  for (const frac of [0.15, 0.42, 0.68, 0.92]) {
    const y = 1.08 - frac * (1.08 - yBot);
    const largo = 0.26 + frac * 1.05;
    g.add(crearFlecha3D([-1.45 - largo, y, -0.35], [-1.45, y, -0.35], 'azul', {
      radio: 0.028, punta: 0.14,
      info: { titulo: 'Presión hidrostática p(h)', detalle: 'Crece linealmente hacia abajo.', formula: 'p = ρgh' },
    }));
  }

  // Franja única brillante o suma de N franjas
  const hAct = (1 - t) * Hagua;
  if (nFranjas > 1) {
    for (let i = 0; i < nFranjas; i += 1) {
      const u0 = i / nFranjas;
      const u1 = (i + 1) / nFranjas;
      const um = (u0 + u1) / 2;
      const [xL, xR] = bordes(um);
      const y0 = yBot + u0 * (yTop - yBot);
      const dy = (yTop - yBot) / nFranjas;
      const fr = new THREE.Mesh(
        new THREE.BoxGeometry(Math.max(0.05, xR - xL), Math.max(0.03, dy * 0.88), 0.10),
        crearMaterial('naranja', { opacidad: 0.28 + 0.55 * (1 - um) }),
      );
      fr.position.set((xL + xR) / 2, y0 + dy / 2, 0.49);
      g.add(fr);
    }
  } else {
    const u = Math.max(0.03, Math.min(0.97, 1 - t));
    const [xL, xR] = bordes(u);
    const yF = yBot + u * (yTop - yBot);
    const franja = new THREE.Mesh(
      new THREE.BoxGeometry(Math.max(0.08, xR - xL), 0.13, 0.12),
      crearMaterial(foco === 'franja' ? 'naranja' : 'morado', { emisivo: 'naranja', intensidadEmisiva: 0.3 }),
    );
    franja.position.set((xL + xR) / 2, yF, 0.51);
    franja.userData.info = {
      titulo: 'Franja horizontal dA = L(h)·dh',
      detalle: 'Toda esta franja está a la misma profundidad h, por eso tiene una sola presión p = ρgh.',
      formula: 'dF = ρg h · L(h) · dh',
    };
    g.add(franja);

    // Cota vertical de profundidad h desde la superficie
    g.add(crearFlecha3D([1.55, 1.08, 0.5], [1.55, yF, 0.5], 'naranja', { radio: 0.025, punta: 0.13 }));
    tags.push(
      { id: 'fr', pos: [(xL + xR) / 2 + 0.42, yF + 0.34, 0.68], texto: 'Franja dA = L(h)dh', activo: foco === 'franja', color: 'naranja' },
      { id: 'ch', pos: [2.05, (1.08 + yF) / 2, 0.5], texto: `h = ${num(hAct, 3)} m`, color: 'naranja' },
    );
  }

  tags.push(
    { id: 'sup', pos: [0, 1.38, -0.35], texto: 'Superficie (h = 0)', activo: foco === 'superficie', color: 'turquesa' },
    { id: 'p', pos: [-2.45, -1.02, 0.15], texto: 'p = ρgh', color: 'azul' },
  );
  const pPa = rho * G * hAct;
  return {
    tags,
    medida: nFranjas > 1
      ? `Suma de ${nFranjas} franjas → al crecer N se vuelve F = ∫ ρg h L(h) dh`
      : `Profundidad h = ${num(hAct, 3)} m · p = ${num(pPa / 1000, 3)} kPa`,
  };
}

/** 6. Tanque de flotación 3D (Arquímedes, cable, velocidad terminal Stokes). */
export function construirFlotacion3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const esfera = p.forma === 'esfera';
  const m = t >= 0.5 ? 1 : 0;
  const frac = p.hundido ? (p.hundido[m] ?? 0.5) : 1.0;

  g.add(crearPisoSutil(-1.68));

  const nivelY = p.hundido ? 0.15 : 0.95;
  const liqColor = m === 1 ? 'amarillo' : 'azul';
  const tanque = new THREE.Mesh(
    new THREE.BoxGeometry(3.4, 2.7, 2.1),
    crearMaterial('borde', { opacidad: 0.14, dobleCara: true, corte }),
  );
  tanque.position.set(0, -0.15, 0);
  const agua = new THREE.Mesh(
    new THREE.BoxGeometry(3.32, nivelY + 1.48, 2.02),
    crearMaterial(liqColor, { opacidad: 0.24, dobleCara: true, corte }),
  );
  agua.position.set(0, (-1.48 + nivelY) / 2, 0);
  g.add(tanque, agua);

  // Posición vertical real según fracción sumergida
  let yObj = -0.35;
  if (p.hundido) {
    yObj = esfera
      ? nivelY + 0.62 * (1 - alturaSumergidaEsfera(frac))
      : nivelY + 0.62 - 1.24 * frac;
  } else if (p.modo === 'terminal') {
    yObj = 0.55 - t * 1.45;
  }

  const cuerpo = new THREE.Mesh(
    esfera ? new THREE.SphereGeometry(0.62, 28, 22) : new THREE.BoxGeometry(1.18, 1.24, 1.05),
    crearMaterial('naranja', { opacidad: 0.92, rugosidad: 0.4 }),
  );
  cuerpo.position.set(0, yObj, 0);
  cuerpo.userData.info = {
    titulo: esfera ? 'Esfera sumergida' : 'Bloque flotante',
    detalle: p.hundido ? `Volumen sumergido: ${num(frac * 100, 2)} %. Flotando: Empuje = Peso.` : 'Sumergido en el fluido.',
    formula: 'F_B = ρ_líq g V_sum',
  };
  g.add(cuerpo);

  if (p.modo !== 'compresion') {
    g.add(crearFlecha3D([0.15, yObj, 0.72], [0.15, yObj + 1.05, 0.72], 'verde', {
      info: { titulo: 'Empuje F_B', detalle: 'Igual al peso del volumen de líquido desplazado.', formula: 'F_B = ρ g V_sum' },
    }));
    g.add(crearFlecha3D([-0.15, yObj, 0.72], [-0.15, yObj - 1.02, 0.72], 'rojo', {
      info: { titulo: 'Peso mg', detalle: 'El peso del cuerpo NO cambia al cambiar de líquido.', formula: 'W = m g = ρ_obj V g' },
    }));
    tags.push(
      { id: 'fb', pos: [0.68, yObj + 0.95, 0.72], texto: 'Empuje F_B', activo: foco === 'fuerzas', color: 'verde' },
      { id: 'mg', pos: [-0.65, yObj - 0.95, 0.72], texto: 'Peso mg', activo: foco === 'fuerzas', color: 'rojo' },
    );
  }

  if (p.modo === 'cable') {
    g.add(crearLinea3D([[0, yObj + 0.62, 0], [0, 1.85, 0]], 'tinta'));
    g.add(crearFlecha3D([-0.58, yObj + 0.15, 0.72], [-0.58, yObj + 1.05, 0.72], 'morado'));
    tags.push({ id: 'ten', pos: [-1.15, yObj + 0.85, 0.72], texto: 'Tensión T', color: 'morado' });
  } else if (p.modo === 'terminal') {
    g.add(crearFlecha3D([-0.58, yObj, 0.72], [-0.58, yObj + 0.78, 0.72], 'morado'));
    tags.push({ id: 'stk', pos: [-1.25, yObj + 0.65, 0.72], texto: 'Arrastre F_S', color: 'morado' });
  }

  tags.push({ id: 'sup', pos: [1.35, nivelY + 0.2, 0], texto: p.nombres ? p.nombres[m] : 'Superficie', activo: foco === 'superficie' });
  return {
    tags,
    medida: p.hundido
      ? `${p.nombres ? p.nombres[m] : 'Fluido'} · ${num(frac * 100, 2)} % del volumen sumergido`
      : p.profundidad
        ? `Profundidad h = ${num(p.profundidad, 3)} m · Compresión por presión hidrostática`
        : 'Equilibrio vertical: suma de fuerzas igual a cero.',
  };
}

/** 7. Tuberías 3D (Venturi, tanque Torricelli, Pitot, descenso, ramas, Poiseuille laminar). */
export function construirTuberia3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const corte = motor.planosCorte();
  const modo = p.modo || 'venturi';
  const [d1 = 0.06, d2 = p.igual ? d1 : d1 * 0.52] = p.diametros || [];
  const esc = 0.68 / Math.max(d1, d2);
  const r1 = Math.max(0.18, d1 * esc);
  const r2 = Math.max(0.14, d2 * esc);

  g.add(crearPisoSutil(-1.55));

  if (modo === 'tanque') {
    const tanq = new THREE.Mesh(
      new THREE.CylinderGeometry(1.18, 1.18, 2.55, 28, 1, true),
      crearMaterial('borde', { opacidad: 0.25, dobleCara: true, corte }),
    );
    tanq.position.set(-0.7, 0.0, 0);
    const liq = new THREE.Mesh(
      new THREE.CylinderGeometry(1.14, 1.14, 2.05, 28),
      crearMaterial('azul', { opacidad: 0.30, corte }),
    );
    liq.position.set(-0.7, -0.25, 0);
    const boq = new THREE.Mesh(
      new THREE.CylinderGeometry(0.17, 0.17, 0.62, 18),
      crearMaterial('tinta2', { opacidad: 0.72, corte }),
    );
    boq.rotation.z = Math.PI / 2;
    boq.position.set(0.72, -0.95, 0);
    g.add(tanq, liq, boq);

    // Chorro parabólico animado
    const pts = Array.from({ length: 24 }, (_, i) => {
      const u = i / 23;
      return new THREE.Vector3(1.0 + u * 1.55, -0.95 - 0.62 * u * u, 0);
    });
    g.add(new THREE.Mesh(
      new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 24, 0.085, 10, false),
      crearMaterial('turquesa', { opacidad: 0.78 }),
    ));
    const uG = t;
    const gota = new THREE.Mesh(new THREE.SphereGeometry(0.11, 14, 14), crearMaterial('morado'));
    gota.position.set(1.0 + uG * 1.55, -0.95 - 0.62 * uG * uG, 0);
    g.add(gota);

    g.add(crearFlecha3D([-2.15, 0.78, 0], [-2.15, -0.95, 0], 'naranja'));
    tags.push(
      { id: 's1', pos: [-0.7, 1.05, 0], texto: 'Superficie (v₁ ≈ 0)', activo: foco === 'entrada' },
      { id: 'h', pos: [-2.45, -0.05, 0], texto: 'h', activo: foco === 'presion', color: 'naranja' },
      { id: 's2', pos: [1.4, -0.52, 0], texto: 'Orificio (v₂)', activo: foco === 'salida', color: 'morado' },
    );
    return { tags, medida: 'Bernoulli entre superficie libre y orificio (ambos a p_atm).' };
  }

  if (modo === 'perfil') {
    // Poiseuille: cilindro transparente cortado + paraboloide 3D de flechas v(r)
    const tubo = new THREE.Mesh(
      new THREE.CylinderGeometry(0.88, 0.88, 3.8, 28, 1, true),
      crearMaterial('borde', { opacidad: 0.26, dobleCara: true, corte }),
    );
    tubo.rotation.z = Math.PI / 2;
    tubo.userData.info = {
      titulo: 'Flujo laminar de Poiseuille',
      detalle: 'En las paredes v = 0; en el centro (r = 0) la rapidez es máxima (2·v_med).',
      formula: 'v(r) = [(p₁−p₂)/(4ηL)](R² − r²)',
    };
    g.add(tubo);

    for (const rRel of [-0.75, -0.45, -0.2, 0, 0.2, 0.45, 0.75]) {
      const len = 1.85 * (1 - rRel * rRel);
      g.add(crearFlecha3D([-0.95, rRel * 0.85, 0], [-0.95 + len, rRel * 0.85, 0], Math.abs(rRel) < 0.05 ? 'morado' : 'azul', {
        radio: 0.028, punta: 0.14,
      }));
    }
    tags.push(
      { id: 'vm', pos: [0.95, 0.18, 0], texto: 'v_máx (r=0)', activo: true, color: 'morado' },
      { id: 'vw', pos: [0, 1.05, 0], texto: 'v = 0 en pared' },
    );
    return { tags, medida: 'Perfil parabólico laminar · Caudal dV/dt = (p₁−p₂)πR⁴ / (8ηL).' };
  }

  // Tubería horizontal, descenso o vertical con 3 tramos (ancha -> transición -> angosta)
  const y1 = modo === 'descenso' ? 0.55 : 0.0;
  const y2 = modo === 'descenso' ? -0.65 : 0.0;
  const tramo = (a, b, ra, rb, col = 'azul', op = 0.28) => {
    const va = new THREE.Vector3(...a);
    const vb = new THREE.Vector3(...b);
    const dir = new THREE.Vector3().subVectors(vb, va);
    const m = new THREE.Mesh(
      new THREE.CylinderGeometry(rb, ra, dir.length(), 24, 1, true),
      crearMaterial(col, { opacidad: op, dobleCara: true, corte }),
    );
    m.position.copy(va).add(vb).multiplyScalar(0.5);
    m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.normalize());
    g.add(m);
  };

  if (modo === 'vertical') {
    tramo([0, 1.75, 0], [0, 0.55, 0], r1, r1);
    tramo([0, 0.55, 0], [0, -0.45, 0], r1, r2);
    tramo([0, -0.45, 0], [0, -1.55, 0], r2, r2);
  } else {
    tramo([-2.4, y1, 0], [-0.65, y1, 0], r1, r1);
    tramo([-0.65, y1, 0], [0.55, y2, 0], r1, r2);
    tramo([0.55, y2, 0], [2.4, y2, 0], r2, r2);
  }

  // Partículas animadas acelerando en la sección angosta (A₁v₁ = A₂v₂)
  for (let i = 0; i < 6; i += 1) {
    const u = (t + i / 6) % 1;
    // En la sección angosta avanza más rápido
    const xP = -2.3 + u * 4.6;
    const yP = xP < -0.65 ? y1 : xP > 0.55 ? y2 : y1 + ((xP + 0.65) / 1.2) * (y2 - y1);
    const part = new THREE.Mesh(
      new THREE.SphereGeometry(0.08, 12, 12),
      crearMaterial(xP > 0.4 ? 'morado' : 'turquesa'),
    );
    part.position.set(modo === 'vertical' ? 0 : xP, modo === 'vertical' ? 1.65 - u * 3.1 : yP, (i % 2 ? 0.1 : -0.1));
    g.add(part);
  }

  // Tubos piezométricos en Venturi o viscoso
  if (modo === 'venturi' || modo === 'viscoso') {
    for (const [idx, xp, alt] of [[0, -1.45, 1.55], [1, 1.45, 0.78]]) {
      const vidrio = new THREE.Mesh(
        new THREE.CylinderGeometry(0.14, 0.14, 1.9, 18, 1, true),
        crearMaterial('borde', { opacidad: 0.30, dobleCara: true }),
      );
      vidrio.position.set(xp, y1 + 0.95, 0);
      const colLiq = new THREE.Mesh(
        new THREE.CylinderGeometry(0.10, 0.10, alt, 16),
        crearMaterial(idx === 0 ? 'azul' : 'morado', { opacidad: 0.78 }),
      );
      colLiq.position.set(xp, y1 + alt / 2, 0);
      colLiq.userData.info = {
        titulo: idx === 0 ? 'Columna 1 (mayor presión)' : 'Columna 2 (menor presión)',
        detalle: 'Donde el tubo se angosta, sube la rapidez v y baja la presión p (Bernoulli).',
        formula: 'p₁ + ½ρv₁² = p₂ + ½ρv₂²',
      };
      g.add(vidrio, colLiq);
      tags.push({ id: `nv${idx}`, pos: [xp, y1 + 2.02, 0], texto: idx === 0 ? 'Nivel 1 (p₁)' : 'Nivel 2 (p₂)', activo: foco === 'presion' });
    }
  }

  tags.push(
    { id: 'sec1', pos: [modo === 'vertical' ? -1.1 : -1.55, modo === 'vertical' ? 1.1 : y1 - 0.92, 0], texto: 'Sección 1 (A₁, v₁)', activo: foco === 'entrada' },
    { id: 'sec2', pos: [modo === 'vertical' ? 1.1 : 1.55, modo === 'vertical' ? -1.0 : y2 - 0.85, 0], texto: 'Sección 2 (A₂, v₂)', activo: foco === 'salida' },
  );
  return {
    tags,
    medida: `d₁ = ${num(d1, 3)} m · d₂ = ${num(d2, 3)} m · Continuidad: A₁v₁ = A₂v₂`,
  };
}

/** 8. Manómetro multilíquido 3D (o tensión superficial / elasticidad). */
export function construirManometro3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  const alt = p.alturas || { A: 2.4, agua: 0.85, mercurio: 1.35, aceite: 0.7, B: 0.2, C: 1.8, libre: 2.65 };
  const mapY = (h) => h * 1.15 - 1.55;
  const xPos = { A: -2.2, agua: -1.65, mercurio: -0.45, aceite: 0.85, B: -0.45, C: 0.85, libre: 2.05 };

  g.add(crearPisoSutil(-1.72));

  const tuboSeg = (a, b, col, op = 0.72, r = 0.11) => {
    const va = new THREE.Vector3(...a);
    const vb = new THREE.Vector3(...b);
    const dir = new THREE.Vector3().subVectors(vb, va);
    if (dir.length() < 0.01) return;
    const m = new THREE.Mesh(
      new THREE.CylinderGeometry(r, r, dir.length(), 16),
      crearMaterial(col, { opacidad: op }),
    );
    m.position.copy(va).add(vb).multiplyScalar(0.5);
    m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.normalize());
    g.add(m);
  };

  // Bulbo A
  const bulbo = new THREE.Mesh(new THREE.SphereGeometry(0.36, 22, 18), crearMaterial('azul', { opacidad: 0.55 }));
  bulbo.position.set(xPos.A, mapY(alt.A), 0);
  bulbo.userData.info = { titulo: 'Tanque A', detalle: `Presión inicial p_A = ${num(p.presion || 64, 3)} kPa manométricos.`, formula: 'p₂ − p₁ = −ρg(y₂ − y₁)' };
  g.add(bulbo);

  const yBot = -1.52;
  const yTop = 0.65;
  const ramas = [
    { col: 'azul', pts: [[xPos.A, mapY(alt.A), 0], [xPos.agua, mapY(alt.A), 0], [xPos.agua, mapY(alt.agua), 0]] },
    { col: 'gris', pts: [[xPos.agua, mapY(alt.agua), 0], [xPos.agua, yBot, 0], [xPos.mercurio, yBot, 0], [xPos.mercurio, mapY(alt.mercurio), 0]] },
    { col: 'verde', pts: [[xPos.mercurio, mapY(alt.mercurio), 0], [xPos.mercurio, yTop, 0], [xPos.aceite, yTop, 0], [xPos.aceite, mapY(alt.aceite), 0]] },
    { col: 'morado', pts: [[xPos.aceite, mapY(alt.aceite), 0], [xPos.aceite, yBot, 0], [xPos.libre, yBot, 0], [xPos.libre, mapY(alt.libre), 0]] },
  ];
  const idx = foco === 'A' || foco === 'agua' ? 0 : foco === 'mercurio' || foco === 'B' ? 1
    : foco === 'aceite' || foco === 'C' ? 2 : Math.min(3, Math.floor(t * 4));

  ramas.forEach((rm, i) => {
    for (let k = 1; k < rm.pts.length; k += 1) {
      tuboSeg(rm.pts[k - 1], rm.pts[k], rm.col, i === idx ? 0.92 : 0.42);
    }
  });

  const nombres = { A: 'A', agua: 'Agua/Hg', mercurio: 'Hg/Glic.', aceite: 'Glic./X', libre: 'Libre (p_atm)' };
  for (const id of ['A', 'agua', 'mercurio', 'aceite', 'libre']) {
    const pos = [xPos[id], mapY(alt[id]), 0];
    const pto = new THREE.Mesh(new THREE.SphereGeometry(foco === id ? 0.14 : 0.10, 14, 14), crearMaterial(foco === id ? 'naranja' : 'tinta'));
    pto.position.set(...pos);
    g.add(pto);
    tags.push({ id: `mn_${id}`, pos: [pos[0], pos[1] + 0.34, 0.15], texto: nombres[id], activo: foco === id });
  }
  return { tags, medida: `Manómetro multilíquido · Bajar suma +ρgΔh, subir resta −ρgΔh.` };
}

export function construirTension3D(motor, p, t, foco) {
  const g = motor.raiz;
  const tags = [];
  g.add(crearPisoSutil(-1.45));

  if (p.modo === 'pelicula') {
    const xM = -0.1 + t * 1.45;
    // Marco en U + barra móvil + película jabonosa translúcida (2 caras)
    const barraSup = new THREE.Mesh(new THREE.CylinderGeometry(0.045, 0.045, 3.6, 12), crearMaterial('tinta2'));
    barraSup.rotation.z = Math.PI / 2;
    barraSup.position.set(0, 0.85, 0);
    const barraInf = barraSup.clone();
    barraInf.position.set(0, -0.85, 0);
    const barraIzq = new THREE.Mesh(new THREE.CylinderGeometry(0.045, 0.045, 1.78, 12), crearMaterial('tinta2'));
    barraIzq.position.set(-1.8, 0, 0);
    const movil = new THREE.Mesh(new THREE.CylinderGeometry(0.06, 0.06, 1.84, 14), crearMaterial('morado'));
    movil.position.set(xM, 0, 0);
    const film = new THREE.Mesh(
      new THREE.BoxGeometry(Math.max(0.1, xM + 1.8), 1.66, 0.04),
      crearMaterial('turquesa', { opacidad: 0.42 }),
    );
    film.position.set((-1.8 + xM) / 2, 0, 0);
    g.add(barraSup, barraInf, barraIzq, movil, film);
    g.add(crearFlecha3D([xM + 0.08, 0, 0], [xM + 1.05, 0, 0], 'naranja'));
    tags.push(
      { id: 'mv', pos: [xM, 1.15, 0], texto: 'Alambre móvil (l)', activo: foco === 'fuerza', color: 'morado' },
      { id: 'f', pos: [xM + 0.72, -0.32, 0], texto: 'F = 2γl (2 caras)', color: 'naranja' },
    );
    return { tags, medida: 'La película tiene DOS superficies (frente y dorso): γ = F / (2l).' };
  }

  // Tracción elástica (Módulo de Young)
  const dL = 0.75 * t;
  const barra = new THREE.Mesh(new THREE.BoxGeometry(2.8 + dL, 0.54, 0.54), crearMaterial('azul', { opacidad: 0.85 }));
  barra.position.set(-1.5 + (2.8 + dL) / 2, 0, 0);
  g.add(barra);
  g.add(crearFlecha3D([-1.5 + 2.8 + dL, 0, 0], [-1.5 + 3.65 + dL, 0, 0], 'rojo'));
  g.add(crearLinea3D([[1.3, -0.65, 0], [1.3, 0.75, 0]], 'gris', { punteada: true }));
  tags.push(
    { id: 'l0', pos: [1.3, 0.95, 0], texto: 'L₀' },
    { id: 'dl', pos: [1.3 + dL, -0.68, 0], texto: 'L₀ + ΔL', activo: foco === 'deforma', color: 'rojo' },
  );
  return { tags, medida: 'Zona elástica: módulo de Young Y = esfuerzo / deformación = σ / ε.' };
}
