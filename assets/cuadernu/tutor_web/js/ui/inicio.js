/**
 * Inicio «Hoy»: un plan activo, una sola acción grande y lo demás plegado.
 * Nada de paneles ni estadísticas: lo único que compite por la atención es el siguiente paso.
 */

import { ajustes, bus, guardarAjuste, pedir } from '../nucleo/api.js';
import { aMedias, banco, estaHecha, nombreTema } from './banco.js';
import { ponerTextoMate } from './preparacion.js';

const PLANES = [
  ['sprint6', 'Sprint 6 días (desde cero)'],
  ['repo26', 'Reposición II-2026'],
  ['extraordinario', 'Extraordinario'],
  ['reposicion', 'Reposiciones anteriores'],
  ['rutas', 'Rutas del III parcial'],
];

function diasParaReposicion() {
  const hoy = new Date();
  hoy.setHours(0, 0, 0, 0);
  const meta = new Date(2026, 11, 8); // martes 8-dic-2026
  return Math.round((meta - hoy) / 86400000);
}

// Una sola vez: quien venía estudiando para el III parcial pasa al plan de la reposición.
// Si después elige otro plan, se respeta.
try {
  if (!localStorage.getItem('tutor.plan.repo26.migrado')) {
    localStorage.setItem('tutor.plan.repo26.migrado', '1');
    if (['extraordinario', 'radar'].includes(ajustes.plan)) guardarAjuste('plan', 'repo26');
  }
} catch { /* modo privado */ }

const ICO = {
  seguir: 'M8 5v14l11-7L8 5Z',
  libro: 'M5 3h11a3 3 0 0 1 3 3v15H8a3 3 0 0 1-3-3V3Zm3 13a1 1 0 0 0 0 2h9v-2H8ZM8 6v2h8V6H8Zm0 4v2h6v-2H8Z',
  reloj: 'M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm1 5v4.6l3.5 2.1-1 1.7L11 12.8V7h2Z',
  formula: 'M4 4h16v3H9.2l4.8 5-4.8 5H20v3H4v-2.6L9.4 12 4 6.6V4Z',
};

function svg(d) {
  return `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="${d}"/></svg>`;
}

function boton(clase, html, alPulsar) {
  const b = document.createElement('button');
  b.type = 'button';
  b.className = clase;
  b.innerHTML = html;
  b.addEventListener('click', alPulsar);
  return b;
}

/** Título del banco sin marcas de LaTeX. */
function tituloLimpio(t) {
  return String(t || '').replace(/\$([^$]*)\$/g, '$1').replace(/[\\{}]/g, '');
}

const palabras = (s) => String(s || '')
  .normalize('NFD').replace(/[̀-ͯ]/g, '')
  .toLowerCase().replace(/[^a-z0-9ñ]+/g, ' ')
  .split(' ').filter(Boolean);

/** true si la nota casi repite el título (el 80 % o más de sus palabras ya están en él). */
function repiteTitulo(nota, titulo) {
  const pn = palabras(nota);
  if (!pn.length) return true;
  const pt = new Set(palabras(titulo));
  return pn.filter((p) => pt.has(p)).length / pn.length >= 0.8;
}

function planActual() {
  return PLANES.some(([k]) => k === ajustes.plan) ? ajustes.plan : 'repo26';
}

export class Inicio {
  constructor(el, acciones, preparacion) {
    this.el = el;
    this.acciones = acciones;
    this.preparacion = preparacion;
    this.gen = 0;
    bus.on('banco', () => this.pintar());
    bus.on('ajuste:plan', () => { if (!this.el.hidden) this.pintar(); });
    this.pintar();
  }

  pintar() {
    const gen = ++this.gen;
    const caja = document.createElement('div');
    caja.className = 'portada hoy';

    const cab = document.createElement('div');
    const h = document.createElement('h1');
    h.textContent = 'Hoy';
    const sub = document.createElement('p');
    sub.className = 'portada-sub';
    const dRest = diasParaReposicion();
    const cuenta = dRest > 1 ? `Faltan ${dRest} días para la Reposición (martes 8-dic). ` : dRest === 1 ? 'Mañana es la Reposición (martes 8-dic). ' : dRest === 0 ? 'Hoy es la Reposición. ' : '';
    sub.textContent = banco.listo
      ? `${cuenta}Una cosa a la vez. Puedes pausar, girar el 3D o repetir cuando quieras.`
      : banco.viejo
        ? 'El motor que está abierto es de una versión anterior. Ciérralo y vuelve a abrir el tutor para usar el banco.'
        : 'Preparando el motor. Esto toma unos segundos.';
    cab.append(h, sub);
    caja.appendChild(cab);

    if (banco.listo) {
      const plan = planActual();
      caja.appendChild(this._selectorPlan(plan));
      const repasoBox = document.createElement('div');
      repasoBox.className = 'hoy-repaso-slot';
      caja.appendChild(repasoBox);
      this._llenarRepaso(gen, repasoBox);
      const hero = document.createElement('div');
      hero.className = 'hoy-hero';
      caja.appendChild(hero);
      caja.appendChild(this._accesos());
      const rutas = this._rutasPlegadas(plan);
      caja.appendChild(rutas);
      this._llenarHero(gen, hero, plan, rutas);
    }
    this.el.replaceChildren(caja);
  }

  /* ---------------------------------------------------------------- plan activo */

  _selectorPlan(plan) {
    const fila = document.createElement('div');
    fila.className = 'hoy-plan';
    const lbl = document.createElement('span');
    lbl.className = 'hoy-plan-lbl';
    lbl.id = 'hoy-plan-lbl';
    lbl.textContent = 'Estudiando para';
    const seg = document.createElement('div');
    seg.className = 'seg';
    seg.setAttribute('role', 'group');
    seg.setAttribute('aria-labelledby', 'hoy-plan-lbl');
    for (const [k, txt] of PLANES) {
      const b = document.createElement('button');
      b.type = 'button';
      b.textContent = txt;
      b.setAttribute('aria-pressed', String(k === plan));
      b.addEventListener('click', () => { if (k !== planActual()) guardarAjuste('plan', k); });
      seg.appendChild(b);
    }
    fila.append(lbl, seg);
    return fila;
  }

  async _llenarRepaso(gen, slot) {
    try {
      const data = await pedir('/api/repaso/hoy?limite=4', { tiempo: 8000 });
      if (gen !== this.gen || !slot.isConnected) return;
      const cola = Array.isArray(data.cola) ? data.cola : [];
      if (!cola.length) return;
      const prim = cola[0];
      const baseId = String(prim.skill_id || '').split(':')[0];
      const box = document.createElement('div');
      box.className = 'repaso-rescate';
      const txt = document.createElement('div');
      const tit = document.createElement('strong');
      tit.textContent = `Rescate rápido (5 min) · ${cola.length} pendiente${cola.length === 1 ? '' : 's'} por repasar`;
      const det = document.createElement('small');
      det.textContent = prim.error_activo
        ? `Prioridad: ${prim.titulo} — ${prim.error_activo.nota}`
        : `Refuerza sin ver: ${prim.titulo} (dominio actual ${Math.round((prim.p_know || 0) * 100)} %)`;
      txt.append(tit, det);
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'btn btn-principal';
      btn.textContent = 'Practicar sin ver';
      btn.addEventListener('click', () => {
        if (this.acciones.practicaCiega) this.acciones.practicaCiega(baseId);
        else if (banco.mapa.has(baseId)) this.acciones.abrir(baseId);
      });
      box.append(txt, btn);
      slot.replaceChildren(box);
    } catch { /* sin repasos aún */ }
  }

  /* ---------------------------------------------------------------- la única acción grande */

  async _llenarHero(gen, hero, plan, rutasEl) {
    if (plan === 'sprint6') {
      const b = boton('accion-grande', `<span><strong>Sprint 6 días desde cero (0 → 100 % Reposición)</strong><small>Micro-bloques de 10–15 min: bases, modelos 3D, práctica sin ver y 2 simulacros</small></span>${svg(ICO.seguir)}`, () => this.acciones.preparacion('sprint6'));
      hero.replaceChildren(b);
      return;
    }
    if (plan === 'rutas') {
      this._heroRutas(hero, rutasEl);
      return;
    }
    const espera = boton('accion-grande', `<span><strong>Buscando tu siguiente ejercicio…</strong><small>Un momento</small></span>${svg(ICO.seguir)}`, () => {});
    espera.disabled = true;
    hero.replaceChildren(espera);

    const r = await this.preparacion.resumenParaInicio(plan);
    if (gen !== this.gen || !hero.isConnected) return;

    if (r.error) {
      const aviso = document.createElement('div');
      aviso.className = 'hoy-aviso';
      const p = document.createElement('p');
      p.textContent = 'No pude cargar el plan. Revisa que el motor siga abierto.';
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn btn-principal';
      b.textContent = 'Reintentar';
      b.addEventListener('click', () => this.pintar());
      aviso.append(p, b);
      hero.replaceChildren(aviso);
      return;
    }

    const verPlan = () => this.acciones.preparacion(plan);
    if (!r.it) {
      const fin = boton('accion-grande', `<span><strong>Terminaste este plan</strong><small>Repasa cualquier bloque o elige otro plan arriba</small></span>${svg(ICO.seguir)}`, verPlan);
      hero.replaceChildren(fin, this._avance(r.hechos, r.total, verPlan));
      return;
    }

    const it = r.it;
    const e = it._enCurso;
    const b = boton('accion-grande', `<span><strong></strong><small></small></span>${svg(ICO.seguir)}`, () => this.preparacion.abrirItem(it));
    const st = b.querySelector('strong');
    st.textContent = e ? 'Seguir: ' : 'Siguiente: ';
    const t = document.createElement('span');
    ponerTextoMate(t, it.titulo);
    st.appendChild(t);
    b.querySelector('small').textContent = [
      r.bloque,
      e && e.total ? `paso ${Math.min((e.paso || 0) + 1, e.total)} de ${e.total}` : (it.minutos ? `~${it.minutos} min` : ''),
    ].filter(Boolean).join(' · ');
    hero.replaceChildren(b, this._avance(r.hechos, r.total, verPlan));
  }

  _heroRutas(hero, rutasEl) {
    const ids = new Set(banco.rutas.flatMap((r) => r.items.map((x) => x.id)).filter((id) => banco.mapa.has(id)));
    const verPlan = () => {
      rutasEl.open = true;
      rutasEl.scrollIntoView({ behavior: 'smooth', block: 'start' });
    };
    const medias = aMedias({ dentro: ids });
    let b;
    if (medias) {
      b = boton('accion-grande', `<span><strong></strong><small></small></span>${svg(ICO.seguir)}`,
        () => this.acciones.abrir(medias.id, { desde: Math.min(medias.e.paso || 0, (medias.e.total || 1) - 1) }));
      b.querySelector('strong').textContent = `Seguir: ${tituloLimpio(medias.lec.titulo)}`;
      b.querySelector('small').textContent = medias.e.total ? `paso ${(medias.e.paso || 0) + 1} de ${medias.e.total}` : '';
    } else {
      let prox = null;
      let deRuta = null;
      for (const r of banco.rutas) {
        const x = r.items.find((y) => banco.mapa.has(y.id) && !estaHecha(y.id));
        if (x) { prox = x; deRuta = r; break; }
      }
      if (prox) {
        const l = banco.mapa.get(prox.id);
        b = boton('accion-grande', `<span><strong></strong><small></small></span>${svg(ICO.seguir)}`, () => this.acciones.abrir(prox.id));
        b.querySelector('strong').textContent = `Siguiente: ${tituloLimpio(l.titulo)}`;
        b.querySelector('small').textContent = [deRuta.titulo, l.min ? `~${l.min} min` : ''].filter(Boolean).join(' · ');
      } else {
        b = boton('accion-grande', `<span><strong>Terminaste las rutas</strong><small>Repasa cualquiera abajo</small></span>${svg(ICO.seguir)}`, verPlan);
      }
    }
    const hechos = [...ids].filter((id) => estaHecha(id)).length;
    hero.replaceChildren(b, this._avance(hechos, ids.size, verPlan));
  }

  _avance(hechos, total, verPlan) {
    const prog = document.createElement('div');
    prog.className = 'prep-progreso hoy-avance';
    const top = document.createElement('div');
    top.className = 'prep-progreso-top';
    const cuenta = document.createElement('strong');
    cuenta.textContent = `${hechos} de ${total} hechos`;
    const ver = document.createElement('button');
    ver.type = 'button';
    ver.className = 'hoy-enlace';
    ver.textContent = 'Ver todo el plan';
    ver.addEventListener('click', verPlan);
    top.append(cuenta, ver);
    const pista = document.createElement('div');
    pista.className = 'prep-progreso-pista';
    pista.setAttribute('role', 'progressbar');
    pista.setAttribute('aria-label', 'Avance del plan');
    pista.setAttribute('aria-valuemin', '0');
    pista.setAttribute('aria-valuemax', String(total));
    pista.setAttribute('aria-valuenow', String(hechos));
    const val = document.createElement('div');
    val.className = 'prep-progreso-val';
    val.style.width = `${total ? Math.round((hechos / total) * 100) : 0}%`;
    pista.appendChild(val);
    prog.append(top, pista);
    return prog;
  }

  /* ---------------------------------------------------------------- accesos y rutas plegadas */

  _accesos() {
    const nav = document.createElement('nav');
    nav.className = 'hoy-accesos';
    nav.setAttribute('aria-label', 'Otras formas de estudiar');
    const nEj = banco.lecciones.filter((l) => l.tipo !== 'teoria').length;
    const fila = [
      ['Formulario', ICO.formula, '42 casillas: uso, trampas y patrones', () => this.acciones.estudiarFormulario()],
      ['Banco de ejercicios', ICO.libro, `${nEj} resueltos paso a paso`, () => this.acciones.biblioteca()],
      ['Simulacro', ICO.reloj, 'Predictivo (17 pts) o rápido', () => this.acciones.simulacro()],
    ];
    for (const [txt, ico, ayuda, fn] of fila) {
      const b = boton('hoy-acceso', `${svg(ico)}<span></span>`, fn);
      b.querySelector('span').textContent = txt;
      b.title = ayuda;
      nav.appendChild(b);
    }
    return nav;
  }

  _rutasPlegadas(plan) {
    const det = document.createElement('details');
    det.className = 'est-det mas-rutas';
    const sum = document.createElement('summary');
    sum.textContent = plan === 'rutas'
      ? `Las rutas del III parcial (${banco.rutas.length})`
      : `Otras rutas (${banco.rutas.length})`;
    det.appendChild(sum);
    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo mas-rutas-cuerpo';
    for (const r of banco.rutas) cuerpo.appendChild(this._ruta(r));
    det.appendChild(cuerpo);
    return det;
  }

  /** Una ruta plegada: su lista se arma la primera vez que se abre. */
  _ruta(r) {
    const det = document.createElement('details');
    det.className = 'est-det ruta';
    const validos = r.items.filter((x) => banco.mapa.has(x.id));
    const hechas = validos.filter((x) => estaHecha(x.id)).length;
    const sum = document.createElement('summary');
    const tit = document.createElement('span');
    tit.textContent = r.titulo;
    const meta = document.createElement('span');
    meta.className = 'ruta-meta';
    meta.textContent = `${hechas} de ${validos.length}`;
    sum.append(tit, meta);
    det.appendChild(sum);
    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo';
    det.appendChild(cuerpo);
    det.addEventListener('toggle', () => {
      if (det.open && !cuerpo.childElementCount) this._llenarRuta(cuerpo, r);
    });
    return det;
  }

  _llenarRuta(cuerpo, r) {
    const p = document.createElement('p');
    p.className = 'ruta-desc';
    p.textContent = r.descripcion;
    cuerpo.appendChild(p);
    const ol = document.createElement('ol');
    ol.className = 'ruta-lista';
    let marcada = false;
    let bloque = '';
    r.items.forEach((it, i) => {
      const l = banco.mapa.get(it.id);
      if (!l) return;
      if (it.bloque && it.bloque !== bloque) {
        bloque = it.bloque;
        const t = document.createElement('li');
        t.className = 'grupo-tit';
        t.textContent = bloque;
        ol.appendChild(t);
      }
      const hecha = estaHecha(it.id);
      const sigue = !hecha && !marcada;
      if (sigue) marcada = true;
      const li = document.createElement('li');
      const b = boton(`ruta-item${hecha ? ' hecha' : ''}${sigue ? ' sigue' : ''}`,
        `<span class="num">${hecha ? '✓' : i + 1}</span><span><strong></strong><small></small></span><span class="min"></span>`,
        () => this.acciones.abrir(it.id));
      b.title = l.id;
      b.querySelector('strong').textContent = tituloLimpio(l.titulo);
      // Sin subtítulos que repiten el título: en ese caso basta con el tema
      b.querySelector('small').textContent = it.nota && !repiteTitulo(it.nota, l.titulo) ? it.nota : nombreTema(l.tema);
      b.querySelector('.min').textContent = l.min ? `${l.min} min` : '';
      li.appendChild(b);
      ol.appendChild(li);
    });
    cuerpo.appendChild(ol);
  }
}
