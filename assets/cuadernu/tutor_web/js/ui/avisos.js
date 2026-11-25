/** Avisos breves y anuncios para lectores de pantalla. */

const $ = (id) => document.getElementById(id);
let ultimo = '';
let cuando = 0;

export function aviso(texto, { mal = false, ms = 3600 } = {}) {
  const t = String(texto || '').trim();
  if (!t) return;
  const ahora = performance.now();
  if (t === ultimo && ahora - cuando < 2500) return;
  ultimo = t;
  cuando = ahora;
  const caja = $('avisos');
  while (caja.childElementCount >= 2) caja.firstElementChild.remove();
  const el = document.createElement('div');
  el.className = `aviso${mal ? ' mal' : ''}`;
  el.textContent = t;
  caja.appendChild(el);
  setTimeout(() => el.remove(), mal ? Math.max(ms, 5200) : ms);
}

export function anunciar(texto) {
  const l = $('lector');
  if (!l) return;
  l.textContent = '';
  requestAnimationFrame(() => { l.textContent = texto; });
}
