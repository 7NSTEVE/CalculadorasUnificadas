/**
 * Familias de letra de la pizarra y espera de carga.
 * La letra manuscrita es local (sin CDN). "Clara" existe para quien se cansa con la cursiva.
 */

export const ESTILOS_LETRA = {
  manuscrita: {
    nombre: 'Manuscrita',
    mate: '"Caveat Tutor", "Ink Free", "Segoe Print", cursive',
    griego: '"Segoe Print", "Ink Free", cursive',
    texto: '"Patrick Hand Tutor", "Ink Free", "Segoe Print", cursive',
    titulo: '"Caveat Tutor", "Ink Free", "Segoe Print", cursive',
    peso: 600, pesoNegrita: 700, escalaMate: 1, escalaTexto: 1, escalaGriego: 0.72,
  },
  clara: {
    nombre: 'Clara',
    mate: '"Cambria Math", Cambria, "Times New Roman", serif',
    griego: '"Cambria Math", Cambria, serif',
    texto: '"Segoe UI Variable", "Segoe UI", system-ui, sans-serif',
    titulo: '"Segoe UI Variable", "Segoe UI", system-ui, sans-serif',
    peso: 400, pesoNegrita: 700, escalaMate: 0.82, escalaTexto: 0.9, escalaGriego: 1, cursivaMate: true,
  },
  propia: {
    nombre: 'Mi letra',
    mate: '"Manuscrita", "Caveat Tutor", "Ink Free", cursive',
    griego: '"Segoe Print", "Ink Free", cursive',
    texto: '"Apuntes", "Patrick Hand Tutor", "Ink Free", cursive',
    titulo: '"Manuscrita", "Caveat Tutor", cursive',
    peso: 400, pesoNegrita: 400, escalaMate: 0.9, escalaTexto: 0.92, escalaGriego: 0.72,
  },
};

let actual = 'manuscrita';
let listas = null;

export function letra() {
  return ESTILOS_LETRA[actual];
}

export function letraActual() {
  return actual;
}

export async function usarLetra(nombre) {
  actual = ESTILOS_LETRA[nombre] ? nombre : 'manuscrita';
  document.documentElement.setAttribute('data-letra', actual);
  const l = letra();
  document.documentElement.style.setProperty('--letra-mate', l.mate);
  document.documentElement.style.setProperty('--letra-texto', l.texto);
  document.documentElement.style.setProperty('--letra-titulo', l.titulo);
  await cargarLetras(true);
}

/** Espera a que las fuentes estén listas: medir antes produce cajas mal calculadas. */
export function cargarLetras(forzar = false) {
  if (listas && !forzar) return listas;
  const l = letra();
  const pedidos = [
    `${l.peso} 34px ${l.mate}`, `${l.pesoNegrita} 34px ${l.mate}`, `400 22px ${l.texto}`, `700 26px ${l.titulo}`,
    `600 30px ${l.griego}`,
  ];
  const muestra = 'AaBb0123αΔ∫';
  listas = Promise.race([
    Promise.all(pedidos.map((f) => document.fonts.load(f, muestra).catch(() => null))),
    new Promise((r) => setTimeout(r, 2500)),
  ]).then(() => undefined);
  return listas;
}
