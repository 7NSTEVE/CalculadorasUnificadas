/** Catálogo de lecciones, progreso y rutas, compartido por el inicio y la biblioteca. */

import { bus, pedir } from '../nucleo/api.js';

export const banco = { lecciones: [], mapa: new Map(), progreso: {}, practicas: {}, rutas: [], listo: false };

export const TEMAS = {
  dilatacion: 'Dilatación', calorimetria: 'Calorimetría', conduccion: 'Conducción', radiacion: 'Radiación',
  gases: 'Gases y teoría cinética', primera_ley: 'Primera ley', ciclos: 'Ciclos p-V',
  maquinas: 'Máquinas y refrigeradores', entropia: 'Entropía (no entra en el III parcial)', teoria: 'Teoría',
};

export function nombreTema(t) { return TEMAS[t] || t; }

/** "Ejercicios Recomendados 3 · problema 16 · pág. 5" */
export function textoFuente(f) {
  if (!f) return '';
  if (f.texto) return f.texto;
  return [f.nombre, f.num ? `problema ${f.num}` : '', f.pag ? `pág. ${f.pag}` : ''].filter(Boolean).join(' · ');
}

export async function cargarBanco({ intentos = 60 } = {}) {
  for (let i = 0; i < intentos; i += 1) {
    try {
      const d = await pedir('/api/banco', { tiempo: 20000 });
      banco.lecciones = d.lecciones || [];
      banco.mapa = new Map(banco.lecciones.map((l) => [l.id, l]));
      banco.progreso = (d.progreso && d.progreso.lecciones) || {};
      banco.practicas = (d.progreso && d.progreso.practicas) || {};
      banco.rutas = d.rutas || [];
      banco.listo = true;
      bus.emit('banco', banco);
      return banco;
    } catch (err) {
      if (err && err.estado === 401) { bus.emit('aviso', { texto: 'El motor rechazó el acceso. Vuelve a abrir la pizarra desde CuadernU.', mal: true }); return null; }
      if (err && err.estado === 404) {
        // El motor abierto es de una versión anterior: no tiene el banco
        banco.viejo = true;
        bus.emit('banco', banco);
        return null;
      }
      await new Promise((r) => setTimeout(r, Math.min(2500, 500 + i * 200)));
    }
  }
  return null;
}

bus.on('progreso', ({ id, paso, total, hecha }) => {
  const e = banco.progreso[id] || { veces: 0, hecha: false };
  e.paso = paso;
  e.total = total;
  e.cuando = Math.floor(Date.now() / 1000);
  if (hecha) { e.hecha = true; e.veces = (e.veces || 0) + 1; }
  banco.progreso[id] = e;
});

export function estaHecha(id) {
  const e = banco.progreso[id];
  return Boolean(e && e.hecha);
}

/** Última lección a medias, para "seguir donde quedé". */
export function aMedias() {
  let mejor = null;
  for (const [id, e] of Object.entries(banco.progreso)) {
    if (!banco.mapa.has(id) || e.hecha) continue;
    if (!mejor || (e.cuando || 0) > (mejor.e.cuando || 0)) mejor = { id, e, lec: banco.mapa.get(id) };
  }
  return mejor;
}

const limpia = (s) => String(s || '').normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase();

/** "ejercicio 16", "e16", "Q49", "pregunta 49" -> id del banco (o null). */
export function idDeTexto(txt) {
  const t = limpia(txt).trim();
  let m = /^(?:la\s+|el\s+)?(?:pregunta|q|item)\s*\.?\s*#?\s*0*(\d{1,3})$/.exec(t);
  if (m) { const id = `Q${String(m[1]).padStart(3, '0')}`; if (banco.mapa.has(id)) return id; }
  m = /^(?:la\s+|el\s+)?(?:ejercicio|ejercicios recomendados|recomendado|ej|e)\s*\.?\s*#?\s*0*(\d{1,2})$/.exec(t);
  if (m) { const id = `E${m[1]}`; if (banco.mapa.has(id)) return id; }
  // "resuélveme el ejercicio 16", "explica la Q049"
  m = /\bq\s*0*(\d{1,3})\b/.exec(t);
  if (m && t.split(' ').length <= 6) { const id = `Q${String(m[1]).padStart(3, '0')}`; if (banco.mapa.has(id)) return id; }
  m = /\b(?:ejercicio|ej|recomendado)\s*#?\s*0*(\d{1,2})\b/.exec(t);
  if (m && t.split(' ').length <= 6 && !/(examen|parcial|ordinario|extra|reposicion|folleto|20\d\d)/.test(t)) {
    const id = `E${m[1]}`;
    if (banco.mapa.has(id)) return id;
  }
  return null;
}

/** Búsqueda local instantánea. */
export function filtrar(q, { tema = '', tipo = '', fuente = '' } = {}) {
  const ts = limpia(q).split(/\s+/).filter(Boolean);
  return banco.lecciones.filter((l) => {
    if (tema && l.tema !== tema) return false;
    if (tipo && l.tipo !== tipo) return false;
    if (fuente === 'E' && !/^E\d/.test(l.id)) return false;
    if (fuente === 'Q' && !/^Q\d/.test(l.id)) return false;
    if (fuente === 'pend' && estaHecha(l.id)) return false;
    if (!ts.length) return true;
    const hay = limpia(`${l.id} ${l.titulo} ${nombreTema(l.tema)} ${(l.claves || []).join(' ')} ${textoFuente(l.fuente)}`);
    return ts.every((t) => hay.includes(t));
  });
}

/** Lección que sigue dentro de la ruta o del banco. */
export function siguienteDe(id) {
  for (const r of banco.rutas) {
    const ids = r.items.map((x) => x.id);
    const i = ids.indexOf(id);
    if (i >= 0 && i + 1 < ids.length) return ids[i + 1];
  }
  const i = banco.lecciones.findIndex((l) => l.id === id);
  return i >= 0 && i + 1 < banco.lecciones.length ? banco.lecciones[i + 1].id : null;
}
