/**
 * Guardar la pizarra en el cuaderno de CuadernU. El anfitrión valida cada acción contra el
 * protocolo v1.1, así que todo se convierte a los tipos que conoce.
 */

import { aviso } from './avisos.js';

const CUADERNO = 'cuadernu.tutor.v1.1';

function accionCuaderno(a, n, k) {
  const base = {
    action_id: `act_${n}_${k}`,
    object_id: `obj_${n}_${k}`,
    placement: { zone: 'derivation_flow', relation: 'auto_next', gap_px: 18 },
    animation: { style: 'handwrite', duration_ms: 400, cue_id: 'c1' },
    color_token: a.color === 'verde' ? 'accent_green' : a.color === 'azul' ? 'accent_blue' : a.color === 'rojo' ? 'accent_red' : 'ink_primary',
  };
  if (a.a === 'mate') return { ...base, type: 'write_math', payload: { latex: a.tex, steps: [a.tex] } };
  if (a.a === 'texto') return { ...base, type: 'write_text', payload: { text: a.txt.replace(/\*\*/g, '') } };
  if (a.a === 'lista') return { ...base, type: 'write_text', payload: { text: (a.items || []).map((x) => `• ${x.replace(/\*\*/g, '')}`).join('\n') } };
  if (a.a === 'tabla') {
    const filas = [a.cab || [], ...(a.filas || [])].map((f) => f.join('   |   ').replace(/\*\*/g, ''));
    return { ...base, type: 'write_text', payload: { text: filas.join('\n') } };
  }
  return null;
}

/** Lección (hasta el paso actual) -> beats del protocolo. */
export function leccionABeats(lec, hasta) {
  const beats = [];
  const sesion = `leccion_${lec.id}`;
  let n = 0;
  const agrega = (titulo, acciones, voz) => {
    for (let i = 0; i < acciones.length; i += 8) {
      beats.push({
        protocol: CUADERNO, session_id: sesion, turn_id: `turno_${n + 1}`, beat_id: `beat_${String(n + 1).padStart(4, '0')}`,
        step_index: n, checkpoint_id: `cp_${n + 1}`, branch_id: null, speech_markup: i ? '' : voz, speech_rate: 1,
        actions: acciones.slice(i, i + 8), provenance: [], verification: { status: 'conceptual' },
        pedagogy: { micro_goal: titulo, concept_tag: lec.tema || 'general', wait_for_student: false },
      });
      n += 1;
    }
  };
  if (lec.enunciado) agrega('Enunciado', [accionCuaderno({ a: 'texto', txt: lec.enunciado }, n, 0)], '');
  lec.pasos.slice(0, hasta + 1).forEach((p, i) => {
    const acc = [accionCuaderno({ a: 'texto', txt: `${i + 1}. ${p.titulo}`, color: 'azul' }, n, 99)];
    p.tramos.forEach((t) => t.acc.forEach((a) => { const c = accionCuaderno(a, n, acc.length); if (c) acc.push(c); }));
    agrega(p.titulo, acc, p.habla || '');
  });
  return beats;
}

/** Registro de conversación hablada en vivo -> beats del protocolo v1.1. */
export function vozVivaABeats(registro, inicio = 0, sesion = 'sesion_voz_viva') {
  if (!Array.isArray(registro) || !registro.length) return [];
  const beats = [];
  let n = inicio;
  let acc = [];
  let voz = [];
  const volcar = (titulo = 'Explicación en vivo') => {
    if (!acc.length && !voz.length) return;
    const acciones = acc.length ? acc : [accionCuaderno({ a: 'texto', txt: voz.join('\n') }, n, 0)].filter(Boolean);
    for (let i = 0; i < acciones.length; i += 8) {
      beats.push({
        protocol: CUADERNO,
        session_id: sesion,
        turn_id: `vivo_${n + 1}`,
        beat_id: `beat_${String(n + 1).padStart(4, '0')}`,
        step_index: n,
        checkpoint_id: `cp_vivo_${n + 1}`,
        branch_id: null,
        speech_markup: i ? '' : voz.join(' ').slice(0, 600),
        speech_rate: 1,
        actions: acciones.slice(i, i + 8),
        provenance: [],
        verification: { status: 'conceptual' },
        pedagogy: { micro_goal: titulo, concept_tag: 'voz_en_vivo', wait_for_student: false },
      });
      n += 1;
    }
    acc = [];
    voz = [];
  };

  for (const item of registro) {
    if (!item) continue;
    if (item.tipo === 'accion' && item.acc) {
      const c = accionCuaderno(item.acc, n, acc.length);
      if (c) acc.push(c);
      if (acc.length >= 8) volcar();
    } else if (item.tipo === 'habla' && item.texto) {
      if (item.quien === 'Tú' && (acc.length || voz.length)) volcar();
      const linea = `${item.quien}: ${item.texto}`;
      voz.push(linea);
      const c = accionCuaderno({ a: 'texto', txt: linea, color: item.quien === 'Tú' ? 'azul' : '' }, n, acc.length);
      if (c) acc.push(c);
    }
  }
  volcar();
  return beats;
}

export function guardarEnCuaderno(rep, viva = null) {
  const beats = [];
  if (rep && rep.lec) beats.push(...leccionABeats(rep.lec, rep.i));
  if (viva && Array.isArray(viva.registro) && viva.registro.length) {
    const ses = rep && rep.lec ? `leccion_${rep.lec.id}` : 'sesion_voz_viva';
    beats.push(...vozVivaABeats(viva.registro, beats.length, ses));
  }
  if (!beats.length) { aviso('Abre una lección o explica algo en voz en vivo antes de guardar.'); return; }
  if (!(window.chrome && window.chrome.webview)) { aviso('Guardar está disponible al abrir la pizarra dentro de CuadernU.'); return; }
  window.chrome.webview.postMessage({ type: 'WEB_BAKE_SESSION', beats });
  aviso('Guardando en el cuaderno…');
}
