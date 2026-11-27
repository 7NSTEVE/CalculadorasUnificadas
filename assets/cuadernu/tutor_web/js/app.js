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
import { banco, cargarBanco, idDeTexto, siguienteDe, textoFuente } from './ui/banco.js';
import { ponerCierre, ponerPregunta } from './ui/cierre.js';
import { anunciar, aviso } from './ui/avisos.js';
import { Lapiz } from './ui/lapiz.js';
import { Asistente } from './ui/asistente.js';
import { Material } from './ui/material.js';
import { Simulacro } from './ui/simulacro.js';
import { guardarEnCuaderno } from './ui/cuaderno.js';
import { AyudaLeccion } from './ui/ayuda.js';

const $ = (id) => document.getElementById(id);

const tablero = new Tablero({ vista: $('tablero'), hoja: $('hoja'), columna: $('columna'), capaMarcas: $('capa-marcas'), capaLapiz: $('capa-lapiz'), volver: $('btn-volver') });
const lateral = new Lateral($('lateral'));
const rep = new Reproductor({ tablero, lateral });

/* ------------------------------------------------------------------ vistas */

let vista = 'inicio';

/**
 * Modo enfoque: encendido por defecto en cada lección y recordado (si la persona lo apaga con el
 * botón, la tecla F o Ajustes, las siguientes lecciones abren sin él). Solo aplica en la pizarra con
 * una lección abierta; en las preguntas libres la barra de preguntar siempre se ve.
 */
let enfoqueLeccion = ajustes.enfoque;

function aplicarEnfoque() {
  const si = vista === 'pizarra' && Boolean(rep.lec) && enfoqueLeccion;
  document.body.classList.toggle('enfoque', si);
  $('btn-enfoque').setAttribute('aria-pressed', String(si));
  $('btn-enfoque').hidden = vista !== 'pizarra' || !rep.lec;
}

function alternarEnfoque() {
  enfoqueLeccion = !enfoqueLeccion;
  guardarAjuste('enfoque', enfoqueLeccion);
  aplicarEnfoque();
}

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
  aplicarEnfoque();
  lateral.actualizarBoton();
  if (v !== 'pizarra') {
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

/** De dónde se abrió la lección: la meta del plan («meta_1_linea») y a dónde volver. */
let contextoAtencion = { id: '', meta: '', corte: '', volverA: '' };

async function abrirLeccion(id, { desde = 0, corte = '', meta = '', volverA = '' } = {}) {
  asistente.callar();
  contextoAtencion = { id, meta, corte, volverA };
  try {
    const qs = corte ? `?corte=${encodeURIComponent(corte)}` : '';
    const lec = await pedir(`/api/banco/${encodeURIComponent(id)}${qs}`, { tiempo: 20000 });
    aPizarra();
    await rep.cargar(lec, { desde });
    $('transporte').hidden = false;
    $('progreso').hidden = rep.total < 2;
    ayuda.actualizar();
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

/** Título sin marcas de LaTeX ni de sub/superíndices, corto, para el texto de un botón. */
function textoPlano(t) {
  return String(t || '')
    .replace(/\$([^$]*)\$/g, '$1')
    .replace(/[\\{}]/g, '')
    .replace(/[_^]\(([^)]*)\)/g, '$1')
    .replace(/[_^]/g, '')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, 60);
}

/**
 * A dónde lleva «Siguiente» al terminar una lección.
 * Si se vino de Preparación: al siguiente pendiente de ese plan (o de vuelta a la preparación).
 * Si no: a la siguiente de la ruta o del banco, sin entrar a temas fuera del corte.
 */
function siguienteTras(lec) {
  if (contextoAtencion.volverA === 'preparacion') {
    const r = preparacion.siguienteTras(lec.origenVariante || lec.id);
    if (r && r.it) {
      const it = r.it;
      return { texto: `Siguiente: ${textoPlano(it.titulo)}`, abrir: () => preparacion.abrirItem(it) };
    }
    return { texto: 'Volver a la preparación', abrir: () => acciones.preparacion() };
  }
  const sig = siguienteDe(lec.id);
  if (sig && banco.mapa.has(sig)) {
    return { texto: `Siguiente: ${textoPlano(banco.mapa.get(sig).titulo)}`, abrir: () => abrirLeccion(sig) };
  }
  return null;
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
  siguiente: siguienteTras,
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
  // En modo enfoque la barra de preguntar está oculta: se muestra para esta lección (sin cambiar el ajuste)
  preguntar: () => {
    if (rep.estado === 'hablando') rep.pausar();
    enfoqueLeccion = false;
    aPizarra();
    $('pregunta').focus();
  },
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
const ayuda = new AyudaLeccion({
  boton: $('btn-ayuda'), menu: $('menu-ayuda'), resumen: $('resumen-atencion'), rep, asistente, lateral, acciones,
  // La meta del plan solo vale para la lección (o su variante) que se abrió desde el plan
  contexto: () => {
    const l = rep.lec;
    const propia = Boolean(l && contextoAtencion.id && (l.id === contextoAtencion.id || l.origenVariante === contextoAtencion.id));
    return { meta: propia ? contextoAtencion.meta : '', volverA: contextoAtencion.volverA };
  },
});
const material = new Material({ el: $('material'), tablero, acciones, asistente });
const lapiz = new Lapiz({ tablero, rep, asistente, acciones, svg: $('capa-lapiz') });
const simulacro = new Simulacro({ el: $('vista-inicio'), acciones });
// Preparación va antes que el Inicio: el Inicio «Hoy» le pide el siguiente ejercicio del plan
const preparacion = new PreparacionExamenes({ el: $('vista-preparacion'), acciones });
const inicio = new Inicio($('vista-inicio'), acciones, preparacion);
const biblioteca = new Biblioteca($('vista-biblioteca'), acciones);
const estudioFormulario = new EstudioFormulario({ el: $('vista-formulario'), acciones });
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
  // Cada lección abre con el modo enfoque que la persona dejó guardado
  enfoqueLeccion = ajustes.enfoque;
  aplicarEnfoque();
  ayuda.actualizar();
});

bus.on('leccion:paso', ({ i, total, paso }) => {
  pintarProgreso();
  ayuda.alPaso();
  ayuda.actualizar();
  anunciar(`Paso ${i + 1} de ${total}: ${paso.titulo}`);
});

bus.on('leccion:estado', ({ estado, i, total, lec }) => {
  const hablando = estado === 'hablando' || estado === 'cargando';
  $('ico-pausa').setAttribute('d', hablando ? ICO_PAUSA : ICO_SEGUIR);
  $('txt-pausa').textContent = hablando ? 'Pausa' : estado === 'espera' || estado === 'fin' ? 'Escuchar otra vez' : 'Continuar';
  $('btn-pausa').disabled = estado === 'cargando';
  $('btn-anterior').disabled = i <= 0;
  const ultimo = i >= total - 1;
  $('txt-siguiente').textContent = !ultimo ? 'Siguiente'
    : lec && lec.sinCierre ? 'Terminar'
      : lec && lec.tipo === 'teoria' ? 'Ver resumen' : 'Ver respuestas';
  // Con las respuestas a la vista, el «Siguiente» que sirve es el del cierre: aquí no queda uno apagado
  $('btn-siguiente').hidden = estado === 'fin';
  $('btn-siguiente').disabled = false;
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
$('btn-inicio').addEventListener('click', aInicio);

$('btn-enfoque').addEventListener('click', alternarEnfoque);
// La casilla de Ajustes cambia el modo de esta lección y de las siguientes
bus.on('ajuste:enfoque', (v) => { enfoqueLeccion = Boolean(v); aplicarEnfoque(); });

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
  // Quien está preguntando necesita la barra para la siguiente pregunta: el enfoque se quita en esta lección
  enfoqueLeccion = false;
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
  if (e.key === 'Escape') {
    lapiz.cerrarMenu();
    // Esc cierra el cajón del panel (ventana angosta), salvo que haya un diálogo o menú encima
    if (lateral.esCajon && lateral.abierto && !lateral.el.hidden && !capaAbierta()) lateral.plegar({ devolverFoco: true });
    return;
  }
  if (escribiendo() || e.ctrlKey || e.metaKey || e.altKey || document.querySelector('dialog[open]') || vista !== 'pizarra' || !rep.lec) return;
  const enBoton = document.activeElement && document.activeElement.tagName === 'BUTTON';
  if (e.code === 'Space' && !enBoton) { e.preventDefault(); if (rep.estado === 'fin') rep.repetir(); else rep.alternar(); }
  else if (e.key === 'ArrowRight' && !e.target.closest?.('[role="tablist"]')) { e.preventDefault(); rep.siguiente(); }
  else if (e.key === 'ArrowLeft' && !e.target.closest?.('[role="tablist"]')) { e.preventDefault(); rep.anterior(); }
  else if (/^r$/i.test(e.key)) rep.repetir();
  else if (/^f$/i.test(e.key)) alternarEnfoque();
  else if (ayuda.tecla(e)) e.preventDefault(); // A: menú Ayuda · I, D, O, P: sus opciones
});

function capaAbierta() {
  try { return Boolean(document.querySelector(':popover-open, dialog[open]')); } catch { return Boolean(document.querySelector('dialog[open]')); }
}

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
window.__tutor = { rep, tablero, lateral, lapiz, material, asistente, banco, acciones, ajustes, bus, aplicarTema, estudioFormulario, preparacion, ayuda };
