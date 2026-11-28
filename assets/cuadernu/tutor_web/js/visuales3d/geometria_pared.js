/**
 * Geometría real de una pared o compuerta con agua (y, si hay, una capa de otro líquido encima),
 * para el 3D: de dónde a dónde barre la franja, cuánto mide L(h), la presión p(h) y la suma
 * F(h) = ∫ p(h') L(h') dh' que crece hasta la fuerza total (R8).
 *
 * Son las mismas formas y los mismos nombres de datos que el dibujo 2D (visuales/fluidos.js):
 *   rectangulo: ancho                      trapecio: ancho_sup, ancho_inf
 *   parabola:  k (y = k·x², y desde el fondo)  mixta: rect (mitad rectangular) + parábola k
 *   ventana:   ancho, y_inf, y_sup (m sobre el fondo)
 *   semicirculo: R (o alto), borde recto arriba a la profundidad h_borde (por defecto, la
 *                interfaz entre las capas) y el arco hacia abajo.
 * H es la profundidad total del líquido. capas = [{ n, rho, e }] de arriba hacia abajo (opcional);
 * sin capas, un solo líquido de densidad rho (1000 kg/m³ por defecto). g = 9,8 m/s².
 */

export const G = 9.8;

function simpson(f, a, b, n = 240) {
  if (!(b > a)) return 0;
  const m = n % 2 ? n + 1 : n;
  const d = (b - a) / m;
  let s = f(a) + f(b);
  for (let i = 1; i < m; i += 1) s += f(a + i * d) * (i % 2 ? 4 : 2);
  return (s * d) / 3;
}

export function modeloPared(p = {}) {
  const forma = String(p.forma || 'rectangulo');
  const capas = Array.isArray(p.capas) && p.capas.length
    ? p.capas.map((c) => ({ n: c.n || '', rho: Number(c.rho) || 1000, e: Math.max(0, Number(c.e) || 0) }))
    : null;
  const H = capas
    ? capas.reduce((s, c) => s + c.e, 0)
    : Math.max(0.01, Number(p.H || p.alto || 4));
  const rho = Number(p.rho || 1000);

  // Presión manométrica a la profundidad h (sumando capa por capa)
  const presion = (h) => {
    if (!capas) return rho * G * Math.max(0, h);
    let resto = Math.max(0, h);
    let pr = 0;
    for (const c of capas) {
      const tramo = Math.min(resto, c.e);
      pr += c.rho * G * tramo;
      resto -= tramo;
      if (resto <= 0) break;
    }
    if (resto > 0) pr += capas[capas.length - 1].rho * G * resto;
    return pr;
  };

  let h0 = 0;
  let h1 = H;
  let medio;
  if (forma === 'ventana') {
    const yi = Number(p.y_inf ?? 0);
    const ys = Number(p.y_sup ?? H);
    h0 = Math.max(0, H - ys);
    h1 = Math.max(h0 + 0.001, H - yi);
    const a = Number(p.ancho || 1);
    medio = () => [-a / 2, a / 2];
  } else if (forma === 'semicirculo') {
    const R = Number(p.R || p.alto || 0.3);
    h0 = Number(p.h_borde ?? (capas && capas.length > 1 ? capas[0].e : 0));
    h1 = Math.min(H, h0 + R);
    medio = (h) => {
      const w = Math.sqrt(Math.max(0, R * R - (h - h0) ** 2));
      return [-w, w];
    };
  } else if (forma === 'trapecio') {
    const bs = Number(p.ancho_sup || 2);
    const bi = Number(p.ancho_inf || 1);
    medio = (h) => {
      const L = bs + ((bi - bs) * h) / H;
      return [-L / 2, L / 2];
    };
  } else if (forma === 'parabola') {
    const k = Number(p.k || 2);
    medio = (h) => {
      const w = Math.sqrt(Math.max(0, H - h) / k);
      return [-w, w];
    };
  } else if (forma === 'mixta') {
    const r = Number(p.rect || 1);
    const k = Number(p.k || 2);
    medio = (h) => [-r, Math.sqrt(Math.max(0, H - h) / k)];
  } else {
    const a = Number(p.ancho || 1);
    medio = () => [-a / 2, a / 2];
  }
  const L = (h) => {
    const [x0, x1] = medio(h);
    return Math.max(0, x1 - x0);
  };
  const dF = (h) => presion(h) * L(h);
  const total = simpson(dF, h0, h1);

  // Caja que ocupa la compuerta (para escalarla a la escena)
  let izq = 0;
  let der = 0;
  for (let i = 0; i <= 120; i += 1) {
    const h = h0 + ((h1 - h0) * i) / 120;
    const [x0, x1] = medio(h);
    izq = Math.min(izq, x0);
    der = Math.max(der, x1);
  }

  return {
    forma, H, h0, h1, capas, rho, medio, L, presion, total, izq, der,
    /** Suma de las franjas desde el borde de arriba (h0) hasta la profundidad h. */
    hasta: (h) => simpson(dF, h0, Math.max(h0, Math.min(h1, h))),
  };
}
