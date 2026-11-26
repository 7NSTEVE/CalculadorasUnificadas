/**
 * Arranque del tutor: conecta inicio, banco, pizarra, voz, lápiz e inteligencia.
 * Este archivo solo orquesta; la lógica vive en los módulos.
 */

import { ajustes, bus, conectar, enCuaderno, enviar, guardarAjuste, pedir } from './nucleo/api.js';
import { usarLetra } from './nucleo/letra.js';
import { aplicarTema } from './nucleo/paleta.js';
import { Tablero } from './pizarra/tablero.js';
import { Reproductor, miniPaso, tintaDe } from './leccion/reproductor.js';
import { Lateral } from './ui/lateral.js';
import { Inicio } from './ui/inicio.js';
import { Biblioteca } from './ui/biblioteca.js';
import { EstudioFormulario } from './ui/formulario_estudio.js';
import { PreparacionExamenes } from './ui/preparacion.js';
import { armarAjustes } from './ui/ajustes.js';
import { banco, cargarBanco, idDeTexto, textoFuente } from './ui/banco.js';
import { ponerCierre, ponerPregunta } from './ui/cierre.js';
import { anunciar, aviso } from './ui/avisos.js';
import { Lapiz } from './ui/lapiz.js';
import { Asistente } from './ui/asistente.js';
import { Material } from './ui/material.js';
import { Simulacro } from './ui/simulacro.js';
import { guardarEnCuaderno } from './ui/cuaderno.js';

const $ = (id) => document.getElementById(id);

const tablero = new Tablero({ vista: $('tablero'), hoja: $('hoja'), columna: $('columna'), capaMarcas: $('capa-marcas'), capaLapiz: $('capa-lapiz'), volver: $('btn-volver') });
const lateral = new Lateral($('lateral'));
const rep = new Reproductor({ tablero, lateral });

/* ------------------------------------------------------------------ vistas */

let vista = 'inicio';

function verVista(v) {
  vista = v;
  document.body.dataset.vista = v;
  $('vista-inicio').hidden = v !== 'inicio';
  $('vista-biblioteca').hidden = v !== 'biblioteca';
  $('vista-formulario').hidden = v !== 'formulario';
  $('vista-preparacion').hidden = v !== 'preparacion';
  $('vista-pizarra').hidden = v !== 'pizarra';
  $('transporte').hidden = v !== 'pizarra' || !rep.lec;
  $('progreso').hidden = v !== 'pizarra' || !rep.lec || rep.total < 2;
  if (v !== 'pizarra') {
    document.body.classList.remove('enfoque');
    $('btn-enfoque').setAttribute('aria-pressed', 'false');
    $('titulo').textContent = v === 'biblioteca'
      ? 'Banco de ejercicios'
      : v === 'formulario'
        ? 'Estudio del formulario oficial'
        : v === 'preparacion'
          ? 'Preparación Extraordinario y Reposición'
          : 'Pizarra del profesor';
    $('subtitulo-barra').textContent = '';
    $('subtitulo').hidden = true;
  }
  $('btn-enfoque').hidden = v !== 'pizarra';
}

function aInicio() {
  if (rep.estado === 'hablando') rep.pausar();
  rep.cortarMini();
  asistente.callar();
  inicio.pintar();
  verVista('inicio');
}

function aBiblioteca() {
  if (rep.estado === 'hablando') rep.pausar();
  verVista('biblioteca');
  biblioteca.abrir();
}

function aPizarra() {
  verVista('pizarra');
}

let contextoAtencion = { meta: '', corte: '', volverA: '' };

function actualizarResumenAtencion() {
  const caja = $('resumen-atencion');
  if (!caja || !rep.lec) return;
  const lec = rep.lec;
  const paso = (lec.pasos && lec.pasos[rep.i]) || null;
  const mapa = lec.formulario_mapa || null;
  const pasoCids = (mapa && mapa.por_paso && mapa.por_paso[rep.i]) || [];
  const celdasPorId = new Map(((mapa && mapa.celdas) || []).map((c) => [c.id, c]));
  const cidsMostrar = pasoCids.length ? pasoCids : ((mapa && mapa.celdas && mapa.celdas[0]) ? [mapa.celdas[0].id] : []);
  const celdas = cidsMostrar.map((id) => celdasPorId.get(id)).filter(Boolean).slice(0, 2);

  caja.replaceChildren();
  const cab = document.createElement('div');
  cab.className = 'atencion-res-cab';
  const h = document.createElement('strong');
  h.textContent = 'Retoma el hilo en 3 segundos';
  const num = document.createElement('span');
  num.textContent = rep.total ? `Paso ${rep.i + 1} de ${rep.total}` : '';
  cab.append(h, num);
  caja.appendChild(cab);

  const dl = document.createElement('dl');
  dl.className = 'atencion-res-lista';

  const fila = (et, val) => {
    const dt = document.createElement('dt');
    dt.textContent = et;
    const dd = document.createElement('dd');
    dd.textContent = val;
    dl.append(dt, dd);
  };

  fila('1. Meta del ejercicio', contextoAtencion.meta || lec.titulo.replace(/\$([^$]*)\$/g, '$1').replace(/[\\{}]/g, ''));
  fila('2. Dónde vas ahora', paso ? `Paso ${rep.i + 1} de ${rep.total}: ${paso.titulo}` : 'Inicio del problema');

  const dt3 = document.createElement('dt');
  dt3.textContent = '3. Fórmula base activa';
  const dd3 = document.createElement('dd');
  if (celdas.length) {
    for (const c of celdas) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = `atencion-res-celda form-c-${c.color}`;
      b.textContent = `${c.id} · ${c.nombre}`;
      b.addEventListener('click', () => {
        caja.hidePopover?.();
        lateral.enfocarFormulario(c.id);
      });
      dd3.appendChild(b);
    }
  } else {
    dd3.textContent = 'Razonamiento paso a paso en la pizarra';
  }
  dl.append(dt3, dd3);
  caja.appendChild(dl);

  if (lec.inciso_omitido) {
    const nota = document.createElement('p');
    nota.className = 'atencion-res-corte';
    nota.textContent = `Corte ≤ 20.5: ${lec.inciso_omitido}`;
    caja.appendChild(nota);
  }

  const pie = document.createElement('div');
  pie.className = 'atencion-res-pie';
  const bSeg = document.createElement('button');
  bSeg.type = 'button';
  bSeg.className = 'btn btn-principal';
  bSeg.textContent = 'Seguir desde aquí';
  bSeg.addEventListener('click', () => caja.hidePopover?.());
  pie.appendChild(bSeg);

  if (contextoAtencion.volverA === 'preparacion') {
    const bVol = document.createElement('button');
    bVol.type = 'button';
    bVol.className = 'btn btn-suave';
    bVol.textContent = 'Volver a preparación';
    bVol.addEventListener('click', () => {
      caja.hidePopover?.();
      acciones.preparacion();
    });
    pie.appendChild(bVol);
  }
  caja.appendChild(pie);
}

async function abrirLeccion(id, { desde = 0, corte = '', meta = '', volverA = '' } = {}) {
  asistente.callar();
  contextoAtencion = { meta, corte, volverA };
  try {
    const qs = corte ? `?corte=${encodeURIComponent(corte)}` : '';
    const lec = await pedir(`/api/banco/${encodeURIComponent(id)}${qs}`, { tiempo: 20000 });
    aPizarra();
    await rep.cargar(lec, { desde });
    $('transporte').hidden = false;
    $('progreso').hidden = rep.total < 2;
    actualizarResumenAtencion();
  } catch (err) {
    aviso(err.estado === 404 ? `${id} todavía no está en el banco.` : 'No pude abrir la lección. Revisa que el motor siga abierto.', { mal: true });
  }
}

/** Abre una lección ya armada (la de la IA o la de un documento). */
async function abrirLeccionSuelta(lec, opt = {}) {
  aPizarra();
  await rep.cargar(lec, opt);
  $('transporte').hidden = false;
  $('progreso').hidden = rep.total < 2;
}

/** Abre una variante numérica nueva calculada en Python. */
async function abrirVariante(id) {
  asistente.callar();
  try {
    const lec = await pedir(`/api/banco/variante/${encodeURIComponent(id)}`, { tiempo: 15000 });
    await abrirLeccionSuelta(lec);
    aviso('Variante con nuevos números calculada en Python.');
  } catch {
    aviso('No pude generar una variante nueva ahora.', { mal: true });
  }
}

const acciones = {
  abrir: abrirLeccion,
  abrirSuelta: abrirLeccionSuelta,
  variante: abrirVariante,
  abrirProblemaDoc: (doc, pr) => material.abrirProblema(doc, pr),
  inicio: aInicio,
  biblioteca: aBiblioteca,
  pizarra: aPizarra,
  adjuntar: () => $('archivo').click(),
  // El simulacro se dibuja en la vista de inicio: hay que mostrarla aunque se pida desde el banco o la pizarra
  simulacro: () => {
    if (rep.estado === 'hablando') rep.pausar();
    asistente.callar();
    verVista('inicio');
    return simulacro.abrir();
  },
  estudiarFormulario: (celdaId = null, modo = null) => {
    if (rep.estado === 'hablando') rep.pausar();
    asistente.callar();
    verVista('formulario');
    return estudioFormulario.abrir({ celda: celdaId, modo });
  },
  preparacion: (tab = null) => {
    if (rep.estado === 'hablando') rep.pausar();
    asistente.callar();
    verVista('preparacion');
    return preparacion.abrir({ tab });
  },
  preguntar: () => { aPizarra(); $('pregunta').focus(); },
  cambioVoz: () => { rep.motorLec = null; if (rep.lec && ['hablando', 'pausado'].includes(rep.estado)) rep.irA(rep.i, { reproducir: rep.estado === 'hablando' }); },
  cambioModelo: (m) => enviar({ type: 'UPDATE_SETTINGS', visual_model: m }),
  modelos: () => [
    ['gemini-4-hydrogen', 'Gemini 4 Hydrogen · Nuevo', 'Gratis con API key de Google'],
    ['gemini-3.8-flash', 'Gemini 3.8 Flash · Avanzado', 'Gratis con API key de Google'],
    ['gemini-3.7-flash', 'Gemini 3.7 Flash', 'Gratis con API key de Google'],
    ['gemini-3.6-flash', 'Gemini 3.6 Flash · Equilibrado', 'Gratis con API key de Google'],
    ['gemini-3.5-flash', 'Gemini 3.5 Flash', 'Gratis con API key de Google'],
    ['gemini-3.5-flash-lite', 'Gemini 3.5 Flash-Lite · Rápido', 'Gratis con API key de Google'],
    ['gemini-3.1-flash-lite', 'Gemini 3.1 Flash-Lite · Ligero', 'Gratis con API key de Google'],
    ['gemma-4-31b-it', 'Gemma 4 31B · Abierto', 'Gratis con API key de Google'],
    ['gemma-4-26b-a4b-it', 'Gemma 4 26B MoE · Abierto', 'Gratis con API key de Google'],
    ['gemini-4-argon', 'Gemini 4 Argon · Pago', 'Solo plan de pago (Paid Tier)'],
    ['gemini-4-neon', 'Gemini 4 Neon · Pago', 'Solo plan de pago (Paid Tier)'],
    ['gemini-4-carbon', 'Gemini 4 Carbon · Pago', 'Solo plan de pago (Paid Tier)'],
  ],
  reportar: () => asistente.reportar('Marcado por la persona'),
  guardarCuaderno: () => guardarEnCuaderno(rep, asistente.viva),
  enCuaderno,
  aviso,
};

const asistente = new Asistente({ rep, tablero, lateral, acciones, miniPaso });
const material = new Material({ el: $('material'), tablero, acciones, asistente });
const lapiz = new Lapiz({ tablero, rep, asistente, acciones, svg: $('capa-lapiz') });
const simulacro = new Simulacro({ el: $('vista-inicio'), acciones });
const inicio = new Inicio($('vista-inicio'), acciones);
const biblioteca = new Biblioteca($('vista-biblioteca'), acciones);
const estudioFormulario = new EstudioFormulario({ el: $('vista-formulario'), acciones });
const preparacion = new PreparacionExamenes({ el: $('vista-preparacion'), acciones });
armarAjustes($('ajustes'), acciones);
bus.on('formulario:estudiar', ({ celdaId = null, modo = null } = {}) => acciones.estudiarFormulario(celdaId, modo));

/* ------------------------------------------------------------------ estado de la lección */

const ICO_PAUSA = 'M7 5h3.5v14H7V5Zm6.5 0H17v14h-3.5V5Z';
const ICO_SEGUIR = 'M8 5v14l11-7L8 5Z';

function pintarProgreso() {
  const caja = $('progreso-puntos');
  if (caja.childElementCount !== rep.total || caja.dataset.lec !== (rep.lec && rep.lec.id)) {
    caja.dataset.lec = rep.lec ? rep.lec.id : '';
    caja.replaceChildren(...Array.from({ length: rep.total }, (_, i) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('aria-label', `Ir al paso ${i + 1}: ${rep.lec.pasos[i].titulo}`);
      b.title = `${i + 1}. ${rep.lec.pasos[i].titulo}`;
      b.addEventListener('click', () => rep.irA(i));
      return b;
    }));
  }
  [...caja.children].forEach((b, i) => {
    b.classList.toggle('actual', i === rep.i);
    b.classList.toggle('hecho', i < rep.i || (rep.estado === 'fin' && i === rep.i));
  });
  $('progreso-txt').textContent = rep.total ? `Paso ${rep.i + 1} de ${rep.total}` : '';
}

bus.on('leccion:abierta', (lec) => {
  $('titulo').textContent = lec.titulo.replace(/\$([^$]*)\$/g, '$1').replace(/[\\{}]/g, '');
  $('subtitulo-barra').textContent = [/^([EQ]\d|PR\d|P26-|RP-)/.test(lec.id) ? lec.id : '', textoFuente(lec.fuente)].filter(Boolean).join(' · ');
  $('transporte').hidden = false;
  $('btn-enfoque').hidden = false;
  actualizarResumenAtencion();
});

bus.on('leccion:paso', ({ i, total, paso }) => {
  pintarProgreso();
  actualizarResumenAtencion();
  anunciar(`Paso ${i + 1} de ${total}: ${paso.titulo}`);
  $('btn-despacio').disabled = false;
  $('btn-origen').hidden = !(paso.origen && paso.origen.length) && !(rep.lec.formulas && rep.lec.formulas.length);
  $('btn-porque').hidden = !paso.porque;
});

bus.on('leccion:estado', ({ estado, i, total }) => {
  const hablando = estado === 'hablando' || estado === 'cargando';
  $('ico-pausa').setAttribute('d', hablando ? ICO_PAUSA : ICO_SEGUIR);
  $('txt-pausa').textContent = hablando ? 'Pausa' : estado === 'espera' || estado === 'fin' ? 'Escuchar otra vez' : 'Continuar';
  $('btn-pausa').disabled = estado === 'cargando';
  $('btn-anterior').disabled = i <= 0;
  const ultimo = i >= total - 1;
  $('txt-siguiente').textContent = ultimo ? 'Ver respuestas' : 'Siguiente';
  $('btn-siguiente').disabled = estado === 'fin';
  $('btn-siguiente').classList.toggle('llama', estado === 'espera');
  document.body.dataset.estado = estado;
  pintarProgreso();
});

bus.on('leccion:finpaso', ({ i, total, paso, lec }) => {
  if (ajustes.sinSaltos && paso.lento && paso.lento.length) {
    const v = rep.aclaracion('Despeje sin saltos', 'pregunta');
    for (const r of paso.lento) {
      if (r.nota) {
        const tn = tintaDe({ a: 'texto', txt: r.nota, clase: 'nota' }, rep.tablero);
        if (tn) { rep.tablero.agregar(tn, { paso: v }); tn.completar(); }
      }
      if (r.tex) {
        const tm = tintaDe({ a: 'mate', tex: r.tex }, rep.tablero);
        if (tm) { rep.tablero.agregar(tm, { paso: v }); tm.completar(); }
      }
    }
  }
  if (paso.pregunta) ponerPregunta(rep, paso);
  if (i >= total - 1 && !lec.sinCierre) ponerCierre(rep, lec, acciones);
});

let claveSub = '';
bus.on('subtitulo', (s) => {
  const caja = $('subtitulo');
  if (!s) { caja.hidden = true; claveSub = ''; return; }
  caja.hidden = vista !== 'pizarra';
  const p = $('subtitulo-txt');
  if (s.clave !== claveSub) {
    claveSub = s.clave;
    p.replaceChildren(...s.palabras.flatMap(([v], j) => {
      const sp = document.createElement('span');
      sp.textContent = v;
      return j ? [document.createTextNode(' '), sp] : [sp];
    }));
  }
  const sps = p.querySelectorAll('span');
  sps.forEach((sp, j) => { sp.className = j < s.actual ? 'dicha' : j === s.actual ? 'ahora' : ''; });
});

bus.on('aviso', ({ texto, mal }) => aviso(texto, { mal }));

/* ------------------------------------------------------------------ transporte */

$('btn-pausa').addEventListener('click', () => {
  if (rep.estado === 'espera' || rep.estado === 'fin') rep.repetir();
  else rep.alternar();
});
$('btn-repetir').addEventListener('click', () => rep.repetir());
$('btn-siguiente').addEventListener('click', () => rep.siguiente());
$('btn-anterior').addEventListener('click', () => rep.anterior());
$('btn-despacio').addEventListener('click', () => asistente.despacio());
$('btn-origen').addEventListener('click', () => asistente.origen());
$('btn-porque').addEventListener('click', () => asistente.porque());
$('btn-inicio').addEventListener('click', aInicio);

$('btn-enfoque').addEventListener('click', () => {
  const si = !document.body.classList.contains('enfoque');
  document.body.classList.toggle('enfoque', si);
  $('btn-enfoque').setAttribute('aria-pressed', String(si));
});

$('btn-menos').addEventListener('click', () => zoom(1 / 1.12));
$('btn-mas').addEventListener('click', () => zoom(1.12));
function zoom(f) {
  tablero.fijarZoom(f === 0 ? 1 : tablero.zoom * f);
  guardarAjuste('zoom', tablero.zoom);
}

/* ------------------------------------------------------------------ preguntas */

async function preguntar() {
  const q = $('pregunta').value.trim();
  if (!q) { $('pregunta').focus(); return; }
  $('pregunta').value = '';
  const id = idDeTexto(q);
  if (id) { abrirLeccion(id); return; }
  const doc = await material.resolverPedido(q);
  if (doc) return;
  aPizarra();
  asistente.preguntar(q);
}
$('btn-enviar').addEventListener('click', preguntar);
$('pregunta').addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); preguntar(); } });
$('btn-adjuntar').addEventListener('click', () => $('archivo').click());
$('btn-mic').addEventListener('click', () => asistente.alternarVoz());

/* ------------------------------------------------------------------ visor de imagen */

const visor = $('visor');
bus.on('ver-imagen', ({ src, alt }) => {
  $('visor-img').src = src;
  $('visor-img').alt = alt || '';
  visor.showModal();
});
$('visor-cerrar').addEventListener('click', () => visor.close());
visor.addEventListener('click', (e) => { if (e.target === visor) visor.close(); });

/* ------------------------------------------------------------------ teclado */

function escribiendo() {
  const a = document.activeElement;
  return a && (/^(INPUT|TEXTAREA|SELECT)$/.test(a.tagName) || a.isContentEditable);
}

window.addEventListener('keydown', (e) => {
  if ((e.ctrlKey || e.metaKey) && !e.altKey && !e.shiftKey) {
    if (e.key === '+' || e.key === '=') { e.preventDefault(); zoom(1.12); return; }
    if (e.key === '-') { e.preventDefault(); zoom(1 / 1.12); return; }
    if (e.key === '0') { e.preventDefault(); zoom(0); return; }
  }
  if ((e.ctrlKey || e.metaKey) && e.shiftKey && /^b$/i.test(e.key)) { e.preventDefault(); asistente.reportar('Marcado con Ctrl+Shift+B'); return; }
  if (e.key === 'Escape') { lapiz.cerrarMenu(); return; }
  if (escribiendo() || e.ctrlKey || e.metaKey || e.altKey || visor.open || vista !== 'pizarra' || !rep.lec) return;
  const enBoton = document.activeElement && document.activeElement.tagName === 'BUTTON';
  if (e.code === 'Space' && !enBoton) { e.preventDefault(); if (rep.estado === 'fin') rep.repetir(); else rep.alternar(); }
  else if (e.key === 'ArrowRight' && !e.target.closest?.('[role="tablist"]')) { e.preventDefault(); rep.siguiente(); }
  else if (e.key === 'ArrowLeft' && !e.target.closest?.('[role="tablist"]')) { e.preventDefault(); rep.anterior(); }
  else if (/^r$/i.test(e.key)) rep.repetir();
  else if (/^f$/i.test(e.key)) $('btn-enfoque').click();
});

/* ------------------------------------------------------------------ conexión y anfitrión */

const ESTADOS = {
  conectado: ['bien', 'Listo'], cortado: ['', 'Reconectando'], rechazado: ['mal', 'Acceso rechazado'],
};
let conectadoAntes = false;
bus.on('conexion', ({ estado }) => {
  const [c, t] = ESTADOS[estado] || ['', estado];
  $('punto').className = `punto ${c}`;
  $('estado-txt').textContent = t;
  if (estado === 'conectado') {
    if (ajustes.modelo) enviar({ type: 'UPDATE_SETTINGS', visual_model: ajustes.modelo });
    if (conectadoAntes && !banco.listo) cargarBanco();
    conectadoAntes = true;
  }
});
bus.on('estado', ({ texto, clase }) => {
  $('estado-txt').textContent = texto;
  if (clase !== undefined) $('punto').className = `punto ${clase}`;
});

if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener('message', (ev) => {
    let d = ev.data;
    if (typeof d === 'string') { try { d = JSON.parse(d); } catch { return; } }
    if (!d) return;
    if (d.type === 'HOST_THEME_SYNC') {
      const m = ['dark', 'high-contrast'].includes(d.mode) ? d.mode : 'light';
      ajustes.menosMovimiento = Boolean(d.reduceMotion) || ajustes.menosMovimiento;
      ajustes.tema = m;
      aplicarTema(m);
    } else if (d.type === 'HOST_BAKE_RESULT') {
      aviso(d.summary || 'Guardado en el cuaderno', { mal: d.ok === false });
    }
  });
}

window.addEventListener('beforeunload', () => rep.soltar());

/* ------------------------------------------------------------------ arranque */

const pedida = new URLSearchParams(window.location.search).get('leccion');

(async () => {
  aplicarTema(ajustes.tema);
  await usarLetra(ajustes.letra);
  tablero.zoom = Math.max(0.6, Math.min(2.2, ajustes.zoom || 1));
  verVista('inicio');
  lateral.mostrar(false);
  conectar();
  await cargarBanco();
  if (pedida && banco.mapa.has(pedida)) abrirLeccion(pedida);
})();

/* Gancho de pruebas: no se usa en la aplicación */
window.__tutor = { rep, tablero, lateral, lapiz, material, asistente, banco, acciones, ajustes, bus, aplicarTema, estudioFormulario, preparacion };
