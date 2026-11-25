/**
 * Paleta única del tutor en valores concretos.
 *
 * Los lienzos (canvas) no entienden `light-dark()` ni variables CSS sin resolver: pedirle
 * a `getComputedStyle` una variable devolvía la expresión cruda y todo se pintaba negro.
 * Aquí el color siempre sale como `#rrggbb`, para claro, oscuro y alto contraste.
 */

const CLARO = {
  papel: '#fbfbf8', tarjeta: '#ffffff', elevada: '#f4f6fa', borde: '#d5dde8', punto: '#d7dee8',
  tinta: '#16213f', tinta2: '#46536a', tinta3: '#7b879b',
  azul: '#1d4ed8', rojo: '#d0261f', verde: '#0a7a4b', naranja: '#c25a08', morado: '#6d28d9',
  turquesa: '#0f766e', rosa: '#be2569', amarillo: '#b7791f', gris: '#64748b',
  resaltador: '#fde047', caliente: '#dc4a26', frio: '#2563eb',
};

const OSCURO = {
  papel: '#13171f', tarjeta: '#1b202b', elevada: '#222836', borde: '#333d4f', punto: '#252d3d',
  tinta: '#eef2f8', tinta2: '#b3c0d2', tinta3: '#8794a8',
  azul: '#6ea8fe', rojo: '#ff8178', verde: '#4ade80', naranja: '#fda65a', morado: '#b9a2fd',
  turquesa: '#3fd5c3', rosa: '#f58bc0', amarillo: '#facc15', gris: '#94a3b8',
  resaltador: '#facc15', caliente: '#ff8a65', frio: '#70aeff',
};

const CONTRASTE = {
  papel: '#000000', tarjeta: '#000000', elevada: '#0a0a0a', borde: '#ffffff', punto: '#333333',
  tinta: '#ffffff', tinta2: '#ffffff', tinta3: '#e5e5e5',
  azul: '#8cc8ff', rojo: '#ff9a94', verde: '#7dff9e', naranja: '#ffc266', morado: '#d8c4ff',
  turquesa: '#7ff5e6', rosa: '#ffb3dc', amarillo: '#ffee58', gris: '#d4d4d4',
  resaltador: '#ffee58', caliente: '#ff9e80', frio: '#8cc8ff',
};

const ALIAS = {
  red: 'rojo', blue: 'azul', green: 'verde', orange: 'naranja', purple: 'morado', violet: 'morado',
  indigo: 'morado', yellow: 'amarillo', teal: 'turquesa', cyan: 'turquesa', pink: 'rosa',
  magenta: 'rosa', ink: 'tinta', primary: 'tinta', ink_primary: 'tinta', gray: 'gris', grey: 'gris',
  black: 'tinta', azul_claro: 'azul', violeta: 'morado', amber: 'amarillo', hot: 'caliente',
  cold: 'frio', rojo_calor: 'caliente',
};

export function temaActual() {
  return document.documentElement.getAttribute('data-theme') || 'light';
}

export function esOscuro() {
  return temaActual() !== 'light';
}

export function paleta() {
  const t = temaActual();
  if (t === 'high-contrast') return CONTRASTE;
  return t === 'dark' ? OSCURO : CLARO;
}

/** Resuelve un nombre (español, inglés o token del protocolo) a `#rrggbb`. */
export function color(nombre, porDefecto = 'tinta') {
  const pal = paleta();
  if (typeof nombre === 'string' && /^#[0-9a-f]{3,8}$/i.test(nombre)) return nombre;
  const clave = String(nombre || porDefecto).toLowerCase().replace(/^accent_/, '').replace(/^ink_(?!primary)/, '').trim();
  return pal[clave] || pal[ALIAS[clave]] || pal[porDefecto] || pal.tinta;
}

/** Mismo color con transparencia, como `rgba(...)`. */
export function alfa(nombre, a) {
  const h = color(nombre);
  const n = h.length === 4
    ? h.slice(1).split('').map((x) => parseInt(x + x, 16))
    : [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
  return `rgba(${n[0]}, ${n[1]}, ${n[2]}, ${a})`;
}

/** Mezcla dos colores (0 = a, 1 = b). Sirve para pintar temperaturas. */
export function mezcla(a, b, t) {
  const p = (h) => [1, 3, 5].map((i) => parseInt(color(h).slice(i, i + 2), 16));
  const A = p(a);
  const B = p(b);
  const k = Math.max(0, Math.min(1, t));
  const hex = A.map((v, i) => Math.round(v + (B[i] - v) * k).toString(16).padStart(2, '0')).join('');
  return `#${hex}`;
}

/** Aplica el tema: escribe las variables CSS y avisa a quien deba redibujarse. */
const oyentes = new Set();
export function alCambiarTema(fn) {
  oyentes.add(fn);
  return () => oyentes.delete(fn);
}

export function aplicarTema(modo) {
  const raiz = document.documentElement;
  raiz.setAttribute('data-theme', modo);
  raiz.style.colorScheme = modo === 'light' ? 'light' : 'dark';
  const pal = paleta();
  for (const [k, v] of Object.entries(pal)) raiz.style.setProperty(`--${k}`, v);
  for (const fn of [...oyentes]) {
    try { fn(modo); } catch (err) { console.error('tema', err); }
  }
}
