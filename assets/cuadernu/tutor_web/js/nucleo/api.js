/**
 * Conexión con el motor local (HTTP + WebSocket) y bus de eventos.
 * La sesión sobrevive a las reconexiones: se identifica con un id propio y, si el servidor
 * la recuerda, sigue donde estaba.
 */

const params = new URLSearchParams(window.location.search);
export const puerto = params.get('port') || window.location.port || '8765';
export const ficha = params.get('token') || '';
export const enCuaderno = params.get('host') === 'cuadernu_win32' || Boolean(window.chrome && window.chrome.webview);
export const base = `http://127.0.0.1:${puerto}`;

const oyentes = new Map();
export const bus = {
  on(ev, fn) {
    if (!oyentes.has(ev)) oyentes.set(ev, new Set());
    oyentes.get(ev).add(fn);
    return () => oyentes.get(ev).delete(fn);
  },
  emit(ev, dato) {
    for (const fn of [...(oyentes.get(ev) || [])]) {
      try { fn(dato); } catch (err) { console.error(`bus:${ev}`, err); }
    }
  },
};

function cabeceras(extra = {}) {
  return ficha ? { 'X-CuadernU-Token': ficha, ...extra } : extra;
}

export function urlApi(ruta) {
  const u = new URL(ruta, base);
  if (ficha) u.searchParams.set('token', ficha);
  return u.toString();
}

export async function pedir(ruta, { metodo = 'GET', cuerpo = null, tiempo = 30000, crudo = null, tipo = '' } = {}) {
  const corte = new AbortController();
  const t = setTimeout(() => corte.abort(), tiempo);
  try {
    const r = await fetch(`${base}${ruta}`, {
      method: metodo,
      headers: crudo ? cabeceras({ 'Content-Type': tipo || 'application/octet-stream' }) : cuerpo ? cabeceras({ 'Content-Type': 'application/json' }) : cabeceras(),
      body: crudo || (cuerpo ? JSON.stringify(cuerpo) : undefined),
      signal: corte.signal,
    });
    if (!r.ok) {
      let detalle = '';
      try { detalle = (await r.json()).detail || ''; } catch { /* sin cuerpo */ }
      const e = new Error(detalle || `Error ${r.status}`);
      e.estado = r.status;
      throw e;
    }
    return await r.json();
  } finally {
    clearTimeout(t);
  }
}

/* ---------------- WebSocket con reintento y cola ---------------- */

let ws = null;
let intentos = 0;
let cerrado = false;
const cola = [];

function idSesion() {
  let id = '';
  try { id = sessionStorage.getItem('tutor.sesion') || ''; } catch { /* modo privado */ }
  if (!/^[A-Za-z0-9_-]{8,40}$/.test(id)) {
    id = `s${Math.random().toString(36).slice(2, 10)}${Date.now().toString(36)}`;
    try { sessionStorage.setItem('tutor.sesion', id); } catch { /* modo privado */ }
  }
  return id;
}

export const sesion = idSesion();

export function conectado() {
  return Boolean(ws && ws.readyState === WebSocket.OPEN);
}

export function enviar(obj, { encolar = true } = {}) {
  if (conectado()) { ws.send(JSON.stringify(obj)); return true; }
  if (encolar && cola.length < 40) cola.push(obj);
  return false;
}

export function enviarBinario(buf) {
  if (conectado()) ws.send(buf);
}

export function conectar() {
  cerrado = false;
  const q = new URLSearchParams();
  if (ficha) q.set('token', ficha);
  q.set('sesion', sesion);
  q.set('cliente', '2');
  ws = new WebSocket(`ws://127.0.0.1:${puerto}/ws/tutor?${q}`);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => {
    intentos = 0;
    bus.emit('conexion', { estado: 'conectado' });
    while (cola.length && conectado()) ws.send(JSON.stringify(cola.shift()));
  };
  ws.onclose = (ev) => {
    if (cerrado) return;
    intentos += 1;
    bus.emit('conexion', { estado: ev.code === 4401 ? 'rechazado' : 'cortado', intentos });
    if (ev.code === 4401) return;
    const espera = Math.min(12000, 700 * 1.7 ** Math.min(6, intentos - 1));
    setTimeout(conectar, espera);
  };
  ws.onerror = () => {};
  ws.onmessage = (ev) => {
    if (ev.data instanceof ArrayBuffer) { bus.emit('binario', ev.data); return; }
    let m;
    try { m = JSON.parse(ev.data); } catch { return; }
    bus.emit('mensaje', m);
    if (m.type) bus.emit(`ws:${m.type}`, m);
  };
}

export function desconectar() {
  cerrado = true;
  if (ws) ws.close();
}

/* ---------------- ajustes por persona ---------------- */

const CLAVE_AJUSTES = 'tutor.ajustes.v2';
const VOCES_LEGADO = {
  Kore: 'es-MX-DaliaNeural',
  Aoede: 'es-CO-SalomeNeural',
  Charon: 'es-MX-JorgeNeural',
  Puck: 'es-CR-JuanNeural',
  Fenrir: 'es-CO-GonzaloNeural',
};
const MODELOS_LEGADO = {
  'gemini-2.5-flash': 'gemini-4-hydrogen',
  'gemini-2.5-pro': 'gemini-4-hydrogen',
  'gemini-2.5-flash-lite': 'gemini-3.5-flash-lite',
  'gemini-3.1-pro-preview': 'gemini-4-hydrogen',
};
const POR_DEFECTO = {
  tema: 'light', letra: 'manuscrita', vel: 1, voz: 'es-MX-DaliaNeural', motorVoz: 'auto', vozLocal: '',
  servidorVozLocal: 'http://127.0.0.1:8880/v1/audio/speech', vozLocalHttp: 'es-default', vozKokoro: 'ef_dora',
  narracion: true, foco: true, menosMovimiento: false, resolverAlPegar: true, modelo: 'gemini-4-hydrogen',
  zoom: 1, autoSeguir: false, sinSaltos: true, simbolos: true,
  // Plan activo del Inicio «Hoy»: 'repo26' (Reposición II-2026), 'extraordinario', 'radar',
  // 'reposicion' (reposiciones anteriores) o 'rutas' (las rutas del III parcial)
  plan: 'repo26',
  // La lección abre en modo enfoque (la pizarra manda) y se recuerda si la persona lo apaga
  enfoque: true,
  // Panel de apoyo (visual, enunciado, fórmulas) junto a la pizarra en ventanas anchas
  apoyo: true,
};
// Los despejes intermedios se muestran siempre por defecto. Antes venían apagados y se guardaban
// apagados junto con cualquier otro ajuste; esta marca los enciende una sola vez (luego se respeta lo que elija).
const MARCA_SIN_SALTOS = 'tutor.ajustes.sinSaltos.encendido';

export const ajustes = (() => {
  let g = {};
  try { g = JSON.parse(localStorage.getItem(CLAVE_AJUSTES) || '{}') || {}; } catch { g = {}; }
  const s = { ...POR_DEFECTO };
  for (const k of Object.keys(POR_DEFECTO)) if (typeof g[k] === typeof POR_DEFECTO[k]) s[k] = g[k];
  if (VOCES_LEGADO[s.voz]) s.voz = VOCES_LEGADO[s.voz];
  if (MODELOS_LEGADO[s.modelo]) {
    s.modelo = MODELOS_LEGADO[s.modelo];
    try { localStorage.setItem(CLAVE_AJUSTES, JSON.stringify(s)); } catch { /* modo privado */ }
  }
  try {
    if (!localStorage.getItem(MARCA_SIN_SALTOS)) {
      s.sinSaltos = true;
      localStorage.setItem(MARCA_SIN_SALTOS, '1');
      localStorage.setItem(CLAVE_AJUSTES, JSON.stringify(s));
    }
  } catch { s.sinSaltos = true; }
  return s;
})();

export function guardarAjuste(k, v) {
  ajustes[k] = v;
  try { localStorage.setItem(CLAVE_AJUSTES, JSON.stringify(ajustes)); } catch { /* modo privado */ }
  bus.emit('ajuste', { k, v });
  bus.emit(`ajuste:${k}`, v);
}

export function menosMovimiento() {
  return ajustes.menosMovimiento || window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}
