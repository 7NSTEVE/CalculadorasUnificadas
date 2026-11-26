/**
 * Estudio interactivo del formulario oficial FI2103 II-2026 (42 casillas).
 *
 * Tres modos en una sola vista limpia:
 *   1. Explorar hoja: toca cualquier casilla en la hoja oficial real para ver qué es,
 *      símbolos y unidades SI, cuándo SÍ/NO usarla, trampas, derivaciones y patrones 2023–2026.
 *   2. ¿Qué fórmula uso?: guía de decisión por síntoma del enunciado + qué quedó fuera en II-2026.
 *   3. Entrenador: 32 retos de reconocimiento rápido tocando directamente la hoja oficial.
 */

import { TintaMate } from '../mate/tinta.js';
import { bus, pedir, urlApi } from '../nucleo/api.js';

const SVG_NS = 'http://www.w3.org/2000/svg';
const CLAVE_DOMINADAS = 'tutor.formulario.dominadas.v1';

const ESTADO_ETIQUETA = {
  nueva: 'Nueva II-2026',
  reintroducida: 'Reintroducida II-2026',
  reciente: 'Reciente I-2026',
  fija: 'Fija',
};

const CAT_ENTRENADOR = {
  todas: 'Todos',
  nuevas: 'Nuevas II-2026',
  trampas: 'Trampas frecuentes',
};

function svgEl(tag, attrs = {}) {
  const el = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, String(v));
  return el;
}

function plano(s) {
  return String(s || '')
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .toLowerCase();
}

function cargarDominadas() {
  try {
    const raw = window.localStorage.getItem(CLAVE_DOMINADAS);
    const arr = raw ? JSON.parse(raw) : [];
    return new Set(Array.isArray(arr) ? arr.filter((x) => typeof x === 'string') : []);
  } catch {
    return new Set();
  }
}

function guardarDominadas(set) {
  try {
    window.localStorage.setItem(CLAVE_DOMINADAS, JSON.stringify([...set]));
  } catch {
    // Sin almacenamiento persistente disponible
  }
}

/** Trazo orgánico de círculo a mano alrededor de una celda del formulario oficial. */
function rutaCirculoMano(x, y, w, h, semilla = 1) {
  const padX = Math.min(10, w * 0.03);
  const padY = Math.min(8, h * 0.08);
  const rx = (w - padX * 2) * 0.505;
  const ry = (h - padY * 2) * 0.515;
  const cx = x + w * 0.5;
  const cy = y + h * 0.5;
  const s = ((semilla * 37) % 11) - 5;
  const ox = s * 0.5;
  const oy = ((semilla * 19) % 7 - 3) * 0.4;
  const x0 = cx - rx * 0.88;
  const y0 = cy - ry * 0.46;
  const topY = cy - ry - 1.5;
  const derX = cx + rx + 2;
  const abaY = cy + ry + 1.8;
  const izqX = cx - rx - 1.5;
  const finX = cx - rx * 0.62;
  const finY = cy - ry * 0.84;
  return [
    `M ${x0.toFixed(1)} ${y0.toFixed(1)}`,
    `C ${(cx - rx * 0.65).toFixed(1)} ${(topY - 2).toFixed(1)}, ${(cx + rx * 0.55).toFixed(1)} ${(topY - 1).toFixed(1)}, ${(cx + rx * 0.88).toFixed(1)} ${(cy - ry * 0.48).toFixed(1)}`,
    `C ${(derX + 3).toFixed(1)} ${(cy - ry * 0.12).toFixed(1)}, ${(derX + 2).toFixed(1)} ${(cy + ry * 0.46).toFixed(1)}, ${(cx + rx * 0.76).toFixed(1)} ${(cy + ry * 0.72).toFixed(1)}`,
    `C ${(cx + rx * 0.38).toFixed(1)} ${(abaY + 2).toFixed(1)}, ${(cx - rx * 0.52).toFixed(1)} ${(abaY + 1).toFixed(1)}, ${(cx - rx * 0.85).toFixed(1)} ${(cy + ry * 0.52).toFixed(1)}`,
    `C ${(izqX - 2.5).toFixed(1)} ${(cy + ry * 0.14).toFixed(1)}, ${(izqX - 1.5).toFixed(1)} ${(cy - ry * 0.58).toFixed(1)}, ${finX.toFixed(1)} ${finY.toFixed(1)}`,
  ].join(' ');
}

function recorteCasilla(celda, aspecto, hojaUrl, ancho = 196) {
  const [bx0, by0, bx1, by1] = celda.caja;
  const ix = (bx1 - bx0) * 0.014;
  const iy = (by1 - by0) * 0.065;
  const [x0, y0, x1, y1] = [bx0 + ix, by0 + iy, bx1 - ix, by1 - iy];
  const [aw, ah] = aspecto || [1000, 1468];
  const W = Math.round(ancho);
  const H = Math.max(36, Math.round((W * (y1 - y0) * ah) / ((x1 - x0) * aw)));
  const total = W / (x1 - x0);
  const caja = document.createElement('span');
  caja.className = `est-recorte form-c-${celda.color}`;
  caja.style.width = `${W}px`;
  caja.style.height = `${H}px`;
  caja.style.backgroundImage = `url("${urlApi(hojaUrl || '/api/formulario/hoja.png')}")`;
  caja.style.backgroundSize = `${total.toFixed(1)}px ${(total * (ah / aw)).toFixed(1)}px`;
  caja.style.backgroundPosition = `${(-x0 * total).toFixed(1)}px ${(-y0 * total * (ah / aw)).toFixed(1)}px`;
  caja.setAttribute('role', 'img');
  caja.setAttribute('aria-label', `Recorte real de ${celda.id}: ${celda.nombre}`);
  return caja;
}

function nodoSimbolo(s) {
  const sp = document.createElement('span');
  sp.className = 'est-sim-s';
  const k = s.indexOf('_');
  const i = document.createElement('i');
  i.textContent = k < 0 ? s : s.slice(0, k);
  sp.appendChild(i);
  if (k >= 0) {
    const sub = document.createElement('sub');
    sub.textContent = s.slice(k + 1);
    sp.appendChild(sub);
  }
  return sp;
}

function textoBusquedaCelda(c) {
  const sims = (c.simbolos || []).flat().join(' ');
  const deriv = (c.se_deriva || []).map((d) => `${d.tex} ${d.como}`).join(' ');
  return plano([
    c.id,
    c.nombre,
    c.etiqueta,
    c.bloque_nombre,
    c.estado_ii_2026,
    c.que_es,
    c.unidades,
    ...(c.cuando_si || []),
    ...(c.cuando_no || []),
    ...(c.errores || []),
    ...(c.pistas_enunciado || []),
    c.patron_examen?.en_formularios,
    c.patron_examen?.parte,
    c.patron_examen?.historial,
    sims,
    deriv,
  ].join(' '));
}

export class EstudioFormulario {
  constructor({ el, acciones }) {
    this.el = el;
    this.acciones = acciones;
    this.guia = null;
    this.porId = new Map();
    this.indiceTexto = new Map();
    this.dominadas = cargarDominadas();

    this.modo = 'hoja'; // 'hoja' | 'decisiones' | 'entrenador'
    this.celdaId = 'F1C3';
    this.filtro = 'todas'; // 'todas' | 'nuevas' | 'criticas' | 'constantes' | 'pendientes'
    this.busqueda = '';
    this.busquedaDec = '';

    // Estado del entrenador
    this.entCat = 'todas';
    this.entOrden = [];
    this.entIdx = 0;
    this.entAciertos = 0;
    this.entIntentos = 0;
    this.entEval = null; // { elegida, validas, acerto }

    this._cargando = null;
    this._hojaSvg = null;
    this._cuerpo = null;
    this._ficha = null;
    this._estadoBarra = null;

    bus.on('ajuste:letra', () => {
      if (document.body.dataset.vista === 'formulario' && this.guia) {
        this._pintarModo();
      }
    });

    window.addEventListener('keydown', (e) => this._alTecla(e));
  }

  async _cargar() {
    if (this.guia) return this.guia;
    if (!this._cargando) {
      this._cargando = pedir('/api/formulario/guia', { tiempo: 20000 }).then((g) => {
        this.guia = g;
        this.porId = new Map((g.celdas || []).map((c) => [c.id, c]));
        this.indiceTexto = new Map((g.celdas || []).map((c) => [c.id, textoBusquedaCelda(c)]));
        this._prepararEntrenador(false);
        return g;
      });
    }
    try {
      return await this._cargando;
    } catch (err) {
      this._cargando = null;
      throw err;
    }
  }

  async abrir({ celda = null, modo = null } = {}) {
    if (!this.guia) {
      this.el.innerHTML = '<div class="est-wrap"><p class="vacio">Cargando guía de las 42 casillas del formulario oficial…</p></div>';
    }
    try {
      await this._cargar();
    } catch {
      this.el.innerHTML = '<div class="est-wrap"><p class="vacio">No pude cargar el formulario oficial. Verifica que el motor local siga activo.</p></div>';
      return;
    }
    if (modo && ['hoja', 'decisiones', 'entrenador'].includes(modo)) {
      this.modo = modo;
    }
    if (celda && this.porId.has(celda)) {
      this.celdaId = celda;
      this.modo = 'hoja';
      // Si el filtro activo ocultara la celda pedida, volver a Todas para que se vea destacada
      if (!this._filtradas().some((c) => c.id === celda)) {
        this.filtro = 'todas';
        this.busqueda = '';
      }
    } else if (!this.porId.has(this.celdaId) && this.guia.celdas.length) {
      this.celdaId = this.guia.celdas[0].id;
    }
    this._pintarTodo();
    this.el.scrollTop = 0;
  }

  seleccionar(celdaId, { irAHoja = false } = {}) {
    if (!this.porId.has(celdaId)) return;
    this.celdaId = celdaId;
    if (irAHoja && this.modo !== 'hoja') {
      this.modo = 'hoja';
      if (!this._filtradas().some((c) => c.id === celdaId)) {
        this.filtro = 'todas';
        this.busqueda = '';
      }
      this._pintarTodo();
      return;
    }
    if (this.modo === 'hoja') {
      this._actualizarHojaSeleccion();
      this._pintarFicha();
    }
  }

  _alternarDominada(celdaId) {
    if (this.dominadas.has(celdaId)) this.dominadas.delete(celdaId);
    else this.dominadas.add(celdaId);
    guardarDominadas(this.dominadas);
    this._actualizarContadorDominadas();
    if (this.modo === 'hoja') {
      if (this.filtro === 'pendientes') {
        this._pintarModo();
      } else {
        this._actualizarHojaSeleccion();
        this._pintarFicha();
      }
    }
  }

  _filtradas() {
    if (!this.guia) return [];
    const q = plano(this.busqueda.trim());
    const toks = q ? q.split(/\s+/).filter(Boolean) : [];
    return this.guia.celdas.filter((c) => {
      if (this.filtro === 'nuevas' && !['nueva', 'reintroducida', 'reciente'].includes(c.estado_ii_2026)) return false;
      if (this.filtro === 'virgenes' && c.estado_ordinario_ii_2026 !== 'virgen') return false;
      if (this.filtro === 'evaluadas' && c.estado_ordinario_ii_2026 !== 'evaluada') return false;
      if (this.filtro === 'criticas' && c.prioridad !== 'critica') return false;
      if (this.filtro === 'constantes' && !c.es_constante) return false;
      if (this.filtro === 'pendientes' && this.dominadas.has(c.id)) return false;
      if (toks.length) {
        const blob = this.indiceTexto.get(c.id) || '';
        for (const t of toks) {
          if (!blob.includes(t)) return false;
        }
      }
      return true;
    });
  }

  _alTecla(e) {
    if (document.body.dataset.vista !== 'formulario' || !this.guia) return;
    const a = document.activeElement;
    const escribiendo = a && (/^(INPUT|TEXTAREA|SELECT)$/.test(a.tagName) || a.isContentEditable);
    if (e.key === 'Escape' && !escribiendo) {
      e.preventDefault();
      this.acciones.inicio();
      return;
    }
    if (escribiendo || e.ctrlKey || e.metaKey || e.altKey || this.modo !== 'hoja') return;
    if (e.target && e.target.closest?.('[role="tablist"]')) return;
    if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
      const lista = this._filtradas();
      if (!lista.length) return;
      e.preventDefault();
      const idx = lista.findIndex((c) => c.id === this.celdaId);
      const sig = idx < 0 ? 0 : (idx + (e.key === 'ArrowRight' ? 1 : -1) + lista.length) % lista.length;
      this.seleccionar(lista[sig].id);
    }
  }

  /* ------------------------------------------------------------------ marco principal */

  _pintarTodo() {
    const wrap = document.createElement('div');
    wrap.className = 'est-wrap';

    // Barra superior compacta de una sola fila (sin apilar barras fijas)
    const barra = document.createElement('div');
    barra.className = 'est-barra';

    const seg = document.createElement('div');
    seg.className = 'seg est-modos';
    seg.setAttribute('role', 'tablist');
    seg.setAttribute('aria-label', 'Modos de estudio del formulario');
    const modos = [
      ['hoja', 'Explorar hoja'],
      ['decisiones', '¿Qué fórmula uso?'],
      ['entrenador', 'Entrenador'],
    ];
    for (const [id, lbl] of modos) {
      const b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('role', 'tab');
      b.dataset.modo = id;
      const act = this.modo === id;
      b.setAttribute('aria-selected', String(act));
      b.setAttribute('aria-pressed', String(act));
      b.textContent = lbl;
      b.addEventListener('click', () => {
        if (this.modo === id) return;
        this.modo = id;
        this._pintarTodo();
      });
      seg.appendChild(b);
    }

    const fin = document.createElement('div');
    fin.className = 'est-barra-fin';

    const dom = document.createElement('span');
    dom.className = 'est-dom-cuenta';
    this._estadoBarra = dom;
    this._actualizarContadorDominadas();

    const volver = document.createElement('button');
    volver.type = 'button';
    volver.className = 'btn';
    volver.textContent = 'Volver';
    volver.addEventListener('click', () => this.acciones.inicio());

    fin.append(dom, volver);
    barra.append(seg, fin);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-cuerpo';
    this._cuerpo = cuerpo;

    wrap.append(barra, cuerpo);
    this.el.replaceChildren(wrap);
    this._pintarModo();
  }

  _actualizarContadorDominadas() {
    if (!this.guia) return;
    const n = this.dominadas.size;
    const tot = this.guia.total || 42;
    if (this._estadoBarra) {
      this._estadoBarra.textContent = `${n} / ${tot} dominadas`;
    }
    const btnPend = this.el.querySelector('.est-filtros [data-filtro="pendientes"]');
    if (btnPend) {
      btnPend.textContent = `Sin dominar (${Math.max(0, tot - n)})`;
    }
  }

  _pintarModo() {
    if (!this._cuerpo || !this.guia) return;
    this._hojaSvg = null;
    this._ficha = null;
    if (this.modo === 'hoja') this._pintarExplorar();
    else if (this.modo === 'decisiones') this._pintarDecisiones();
    else this._pintarEntrenador();
  }

  /* ------------------------------------------------------------------ hoja oficial SVG 42 celdas */

  _construirHoja42({ modo, activasSet = null, seleccionada = null, evalEstado = null, alClic = null }) {
    const [vw, vh] = this.guia.aspecto || [1000, 1468];
    const marco = document.createElement('div');
    marco.className = `hoja-formulario est-hoja est-hoja-${modo}`;

    const img = document.createElement('img');
    img.className = 'hoja-formulario-img';
    img.width = vw;
    img.height = vh;
    img.alt = 'Formulario oficial del III Parcial FI2103 II-2026 (42 casillas)';
    img.src = urlApi(this.guia.hoja_url || '/api/formulario/hoja.png');
    marco.appendChild(img);

    const svg = svgEl('svg', {
      class: 'hoja-formulario-svg est-svg',
      viewBox: `0 0 ${vw} ${vh}`,
      preserveAspectRatio: 'none',
      role: 'group',
      'aria-label': 'Casillas interactivas del formulario oficial II-2026',
    });

    this.guia.celdas.forEach((c, idx) => {
      const [x0, y0, x1, y1] = c.caja;
      const x = x0 * vw;
      const y = y0 * vh;
      const w = (x1 - x0) * vw;
      const h = (y1 - y0) * vh;

      const g = svgEl('g', {
        class: `est-celda form-c-${c.color}`,
        'data-celda': c.id,
        tabindex: '0',
        role: 'button',
        'aria-label': modo === 'entrenador'
          ? `${c.id} (${c.etiqueta})`
          : `${c.id}: ${c.nombre} (${c.etiqueta})`,
      });

      const bg = svgEl('rect', {
        class: 'est-celda-bg',
        x: (x + 4).toFixed(1),
        y: (y + 4).toFixed(1),
        width: Math.max(8, w - 8).toFixed(1),
        height: Math.max(8, h - 8).toFixed(1),
        rx: 10,
      });

      const circ = svgEl('path', {
        class: 'est-celda-circulo',
        d: rutaCirculoMano(x, y, w, h, idx + 1),
      });

      g.append(bg, circ);

      if (modo === 'hoja') {
        // Indicador discreto de estado (dominada = check verde; nueva/reintroducida = punto sutil)
        const mx = x + w - 16;
        const my = y + 16;
        const marcaDom = svgEl('g', { class: 'est-marca-dom' });
        marcaDom.append(
          svgEl('circle', { cx: mx.toFixed(1), cy: my.toFixed(1), r: 10 }),
          svgEl('path', {
            d: `M ${(mx - 4.5).toFixed(1)} ${(my + 0.3).toFixed(1)} L ${(mx - 1.2).toFixed(1)} ${(my + 3.6).toFixed(1)} L ${(mx + 5).toFixed(1)} ${(my - 3.6).toFixed(1)}`,
          }),
        );
        g.appendChild(marcaDom);

        if (c.estado_ii_2026 === 'nueva' || c.estado_ii_2026 === 'reintroducida') {
          g.appendChild(svgEl('circle', {
            class: `est-marca-nueva est-est-${c.estado_ii_2026}`,
            cx: (x + 14).toFixed(1),
            cy: (y + 14).toFixed(1),
            r: 5.5,
          }));
        }
      }

      if (alClic) {
        g.addEventListener('click', () => alClic(c.id));
        g.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();
            alClic(c.id);
          }
        });
      }
      svg.appendChild(g);
    });

    marco.appendChild(svg);
    this._hojaSvg = svg;

    if (modo === 'hoja') {
      this._aplicarEstadosHoja(activasSet, seleccionada);
    } else if (evalEstado) {
      this._aplicarEstadosEval(evalEstado);
    }

    return marco;
  }

  _aplicarEstadosHoja(activasSet, seleccionada) {
    if (!this._hojaSvg) return;
    const hayFiltro = activasSet && activasSet.size < this.guia.celdas.length;
    this._hojaSvg.querySelectorAll('.est-celda').forEach((g) => {
      const id = g.dataset.celda;
      const esSel = id === seleccionada;
      const pasa = !activasSet || activasSet.has(id);
      const esDom = this.dominadas.has(id);
      g.classList.toggle('seleccionada', esSel);
      g.classList.toggle('coincide', Boolean(hayFiltro && pasa && !esSel));
      g.classList.toggle('fuera', Boolean(hayFiltro && !pasa));
      g.classList.toggle('dominada', esDom);
      g.setAttribute('aria-pressed', String(esSel));
    });
  }

  _aplicarEstadosEval(ev) {
    if (!this._hojaSvg || !ev) return;
    const validas = new Set(ev.validas || []);
    this._hojaSvg.querySelectorAll('.est-celda').forEach((g) => {
      const id = g.dataset.celda;
      const esBien = validas.has(id);
      const esMal = !ev.acerto && id === ev.elegida;
      g.classList.toggle('eval-bien', esBien);
      g.classList.toggle('eval-mal', esMal);
      g.classList.toggle('fuera', !esBien && !esMal);
    });
  }

  _actualizarHojaSeleccion() {
    const filtradas = this._filtradas();
    const set = new Set(filtradas.map((c) => c.id));
    this._aplicarEstadosHoja(set, this.celdaId);
  }

  /* ------------------------------------------------------------------ MODO 1: EXPLORAR HOJA */

  _pintarExplorar() {
    const cont = document.createElement('div');
    cont.className = 'est-explorar';

    // Controles de búsqueda y filtros rápidos
    const controles = document.createElement('div');
    controles.className = 'est-controles';

    const input = document.createElement('input');
    input.type = 'search';
    input.className = 'est-buscar';
    input.value = this.busqueda;
    input.placeholder = 'Buscar fórmula, símbolo, unidad o trampa (ej. esfuerzo, cono, kelvin, Otto)…';
    input.setAttribute('aria-label', 'Buscar en las 42 casillas del formulario');

    const filaFiltros = document.createElement('div');
    filaFiltros.className = 'filtros est-filtros';
    const nNuevas = this.guia.celdas.filter((c) => ['nueva', 'reintroducida', 'reciente'].includes(c.estado_ii_2026)).length;
    const nVirg = this.guia.celdas.filter((c) => c.estado_ordinario_ii_2026 === 'virgen').length;
    const nEval = this.guia.celdas.filter((c) => c.estado_ordinario_ii_2026 === 'evaluada').length;
    const nCriticas = this.guia.celdas.filter((c) => c.prioridad === 'critica').length;
    const nConst = this.guia.celdas.filter((c) => c.es_constante).length;
    const nPend = this.guia.celdas.length - this.dominadas.size;
    const defs = [
      ['todas', `Todas (${this.guia.celdas.length})`],
      ['virgenes', `Vírgenes en II-2026 (${nVirg})`],
      ['evaluadas', `Evaluadas hoy (${nEval})`],
      ['nuevas', `Novedades II-2026 (${nNuevas})`],
      ['criticas', `Críticas (${nCriticas})`],
      ['constantes', `Constantes (${nConst})`],
      ['pendientes', `Sin dominar (${nPend})`],
    ];

    const cuenta = document.createElement('span');
    cuenta.className = 'est-cuenta';

    for (const [id, lbl] of defs) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'filtro';
      b.dataset.filtro = id;
      b.setAttribute('aria-pressed', String(this.filtro === id));
      b.textContent = lbl;
      b.addEventListener('click', () => {
        this.filtro = id;
        filaFiltros.querySelectorAll('.filtro').forEach((x) => x.setAttribute('aria-pressed', String(x.dataset.filtro === id)));
        this._refrescarFiltroExplorar(cuenta);
      });
      filaFiltros.appendChild(b);
    }
    filaFiltros.appendChild(cuenta);

    controles.append(input, filaFiltros);

    // Rejilla de 2 columnas: hoja real interactiva + ficha completa
    const grid = document.createElement('div');
    grid.className = 'est-grid';

    const colHoja = document.createElement('div');
    colHoja.className = 'est-hoja-col';

    const filtradas = this._filtradas();
    if (filtradas.length && !filtradas.some((c) => c.id === this.celdaId)) {
      this.celdaId = filtradas[0].id;
    }

    const hoja = this._construirHoja42({
      modo: 'hoja',
      activasSet: new Set(filtradas.map((c) => c.id)),
      seleccionada: this.celdaId,
      alClic: (cid) => this.seleccionar(cid),
    });

    const pie = document.createElement('p');
    pie.className = 'est-hoja-pie';
    pie.textContent = 'Toca cualquier casilla de la hoja oficial o usa las flechas ← y → para recorrerla.';

    colHoja.append(hoja, pie);

    const colFicha = document.createElement('article');
    colFicha.className = 'est-ficha-col';
    colFicha.setAttribute('aria-live', 'polite');
    this._ficha = colFicha;

    grid.append(colHoja, colFicha);
    cont.append(controles, grid);
    this._cuerpo.replaceChildren(cont);

    const actualizarCuenta = (lista) => {
      cuenta.textContent = lista.length === this.guia.celdas.length
        ? ''
        : `${lista.length} de ${this.guia.celdas.length}`;
    };
    actualizarCuenta(filtradas);

    input.addEventListener('input', () => {
      this.busqueda = input.value;
      this._refrescarFiltroExplorar(cuenta);
    });

    this._pintarFicha();
  }

  _refrescarFiltroExplorar(cuentaEl) {
    const filtradas = this._filtradas();
    if (cuentaEl) {
      cuentaEl.textContent = filtradas.length === this.guia.celdas.length
        ? ''
        : `${filtradas.length} de ${this.guia.celdas.length}`;
    }
    if (filtradas.length && !filtradas.some((c) => c.id === this.celdaId)) {
      this.celdaId = filtradas[0].id;
    }
    this._aplicarEstadosHoja(new Set(filtradas.map((c) => c.id)), this.celdaId);
    this._pintarFicha();
  }

  _pintarFicha() {
    if (!this._ficha || !this.guia) return;
    const filtradas = this._filtradas();
    if (!filtradas.length) {
      this._ficha.innerHTML = '<div class="est-vacio-ficha"><strong>Ninguna casilla coincide con ese filtro.</strong><p>Prueba otra palabra o vuelve a mostrar las 42 casillas.</p></div>';
      return;
    }
    const c = this.porId.get(this.celdaId) || filtradas[0];
    this.celdaId = c.id;
    const idx = Math.max(0, filtradas.findIndex((x) => x.id === c.id));
    const dom = this.dominadas.has(c.id);

    const frag = document.createDocumentFragment();

    // Cabecera de la ficha
    const cab = document.createElement('header');
    cab.className = `est-ficha-cab form-c-${c.color}`;

    const meta = document.createElement('div');
    meta.className = 'est-ficha-meta';

    const lineaId = document.createElement('div');
    lineaId.className = 'est-ficha-eyebrow';
    const cod = document.createElement('span');
    cod.className = 'est-cod';
    cod.textContent = `${c.id} · ${c.etiqueta}`;
    const blk = document.createElement('span');
    blk.className = 'est-blk';
    blk.textContent = c.bloque_nombre;
    const est = document.createElement('span');
    est.className = `est-tag est-tag-${c.estado_ii_2026}`;
    est.textContent = ESTADO_ETIQUETA[c.estado_ii_2026] || c.estado_ii_2026;
    lineaId.append(cod, blk, est);
    if (c.estado_ordinario_ii_2026 === 'virgen' || c.estado_ordinario_ii_2026 === 'evaluada') {
      const ordTag = document.createElement('span');
      ordTag.className = `est-tag est-tag-${c.estado_ordinario_ii_2026}`;
      ordTag.textContent = c.estado_ordinario_ii_2026 === 'virgen' ? 'Virgen en Ord. II-2026' : 'Evaluada hoy';
      if (c.en_ordinario_ii_2026) ordTag.title = c.en_ordinario_ii_2026;
      lineaId.appendChild(ordTag);
    }

    const h2 = document.createElement('h2');
    h2.className = 'est-ficha-tit';
    h2.textContent = c.nombre;

    meta.append(lineaId, h2);

    const acc = document.createElement('div');
    acc.className = 'est-ficha-acc';

    const btnAnt = document.createElement('button');
    btnAnt.type = 'button';
    btnAnt.className = 'btn est-btn-nav';
    btnAnt.title = 'Casilla anterior (←)';
    btnAnt.setAttribute('aria-label', 'Casilla anterior');
    btnAnt.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M15.5 5 8 12l7.5 7 1.4-1.5L11 12l5.9-5.5L15.5 5Z"/></svg>';
    btnAnt.addEventListener('click', () => {
      const ant = filtradas[(idx - 1 + filtradas.length) % filtradas.length];
      this.seleccionar(ant.id);
    });

    const btnSig = document.createElement('button');
    btnSig.type = 'button';
    btnSig.className = 'btn est-btn-nav';
    btnSig.title = 'Casilla siguiente (→)';
    btnSig.setAttribute('aria-label', 'Casilla siguiente');
    btnSig.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="m8.5 5 7.5 7-7.5 7-1.4-1.5L13 12 7.1 6.5 8.5 5Z"/></svg>';
    btnSig.addEventListener('click', () => {
      const sig = filtradas[(idx + 1) % filtradas.length];
      this.seleccionar(sig.id);
    });

    const btnDom = document.createElement('button');
    btnDom.type = 'button';
    btnDom.className = `btn est-btn-dom${dom ? ' activa' : ''}`;
    btnDom.setAttribute('aria-pressed', String(dom));
    btnDom.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 16.2 4.8 12l-1.4 1.4L9 19 21 7l-1.4-1.4L9 16.2Z"/></svg><span></span>';
    btnDom.querySelector('span').textContent = dom ? 'Dominada' : 'Marcar dominada';
    btnDom.addEventListener('click', () => this._alternarDominada(c.id));

    acc.append(btnAnt, btnSig, btnDom);
    cab.append(meta, acc);
    frag.appendChild(cab);

    // Bloque de fórmula: recorte oficial + 2D math + qué es
    const eqBloque = document.createElement('section');
    eqBloque.className = 'est-eq-bloque';

    const eqFila = document.createElement('div');
    eqFila.className = 'est-eq-fila';
    eqFila.appendChild(recorteCasilla(c, this.guia.aspecto, this.guia.hoja_url, 186));

    const eqBox = document.createElement('div');
    eqBox.className = 'est-eq-mate';
    const maxW = Math.max(240, (this._ficha.clientWidth || 560) - 240);
    const tm = new TintaMate(c.tex, { px: 28, color: c.color, maxAncho: maxW });
    tm.progreso(1);
    eqBox.appendChild(tm.el);
    eqFila.appendChild(eqBox);

    const queEs = document.createElement('p');
    queEs.className = 'est-que-es';
    queEs.textContent = c.que_es;

    eqBloque.append(eqFila, queEs);
    frag.appendChild(eqBloque);

    // Símbolos y unidades SI
    const secSim = document.createElement('section');
    secSim.className = 'est-sec';
    const hSim = document.createElement('h3');
    hSim.className = 'est-sec-tit';
    hSim.textContent = 'Símbolos y unidades';
    secSim.appendChild(hSim);

    if (Array.isArray(c.simbolos) && c.simbolos.length) {
      const dl = document.createElement('dl');
      dl.className = 'est-sim-lista';
      for (const [s, que, u] of c.simbolos) {
        const fila = document.createElement('div');
        fila.className = 'est-sim-fila';
        const dt = document.createElement('dt');
        dt.appendChild(nodoSimbolo(s));
        const dd = document.createElement('dd');
        const txt = document.createElement('span');
        txt.textContent = que;
        dd.appendChild(txt);
        if (u) {
          const unit = document.createElement('span');
          unit.className = 'leyenda-u';
          unit.textContent = u;
          dd.appendChild(unit);
        }
        fila.append(dt, dd);
        dl.appendChild(fila);
      }
      secSim.appendChild(dl);
    }

    const uni = document.createElement('p');
    uni.className = 'est-unidades';
    const uniLbl = document.createElement('strong');
    uniLbl.textContent = 'Regla de unidades: ';
    uni.append(uniLbl, document.createTextNode(c.unidades));
    secSim.appendChild(uni);
    frag.appendChild(secSim);

    // Cuándo SÍ / Cuándo NO
    const siNo = document.createElement('section');
    siNo.className = 'est-si-no';

    const colSi = document.createElement('div');
    colSi.className = 'est-col-si';
    const hSi = document.createElement('h3');
    hSi.className = 'est-sec-tit';
    hSi.textContent = 'Cuándo SÍ se usa';
    const ulSi = document.createElement('ul');
    for (const item of c.cuando_si || []) {
      const li = document.createElement('li');
      li.textContent = item;
      ulSi.appendChild(li);
    }
    colSi.append(hSi, ulSi);

    const colNo = document.createElement('div');
    colNo.className = 'est-col-no';
    const hNo = document.createElement('h3');
    hNo.className = 'est-sec-tit';
    hNo.textContent = 'Cuándo NO usarla';
    const ulNo = document.createElement('ul');
    for (const item of c.cuando_no || []) {
      const li = document.createElement('li');
      li.textContent = item;
      ulNo.appendChild(li);
    }
    colNo.append(hNo, ulNo);

    siNo.append(colSi, colNo);
    frag.appendChild(siNo);

    // Patrón histórico en exámenes 2023–2026
    const secPat = document.createElement('section');
    secPat.className = 'est-sec est-patron';
    const hPat = document.createElement('h3');
    hPat.className = 'est-sec-tit';
    hPat.textContent = 'Cómo identificarla en el examen (patrones 2023–2026)';

    const patMeta = document.createElement('div');
    patMeta.className = 'est-patron-meta';
    const pFreq = document.createElement('span');
    pFreq.textContent = c.patron_examen?.en_formularios || '';
    const pParte = document.createElement('span');
    pParte.textContent = c.patron_examen?.parte || '';
    patMeta.append(pFreq, pParte);

    const patHist = document.createElement('p');
    patHist.className = 'est-patron-hist';
    patHist.textContent = c.patron_examen?.historial || '';

    secPat.append(hPat, patMeta, patHist);

    if (Array.isArray(c.pistas_enunciado) && c.pistas_enunciado.length) {
      const pistasTit = document.createElement('div');
      pistasTit.className = 'est-sublbl';
      pistasTit.textContent = 'Palabras clave en el enunciado:';
      const ulP = document.createElement('ul');
      ulP.className = 'est-pistas';
      for (const p of c.pistas_enunciado) {
        const li = document.createElement('li');
        li.textContent = p;
        ulP.appendChild(li);
      }
      secPat.append(pistasTit, ulP);
    }
    frag.appendChild(secPat);

    // Acordeones nativos NO exclusivos (sin atributo name, sin headings dentro de summary)
    const acor = document.createElement('div');
    acor.className = 'est-acordeon';

    // 1. Errores típicos, cuidados y confusiones (abierto por defecto)
    const detErr = document.createElement('details');
    detErr.className = 'est-det';
    detErr.open = true;
    const sumErr = document.createElement('summary');
    sumErr.textContent = 'Errores típicos, cuidados y confusiones';
    const boxErr = document.createElement('div');
    boxErr.className = 'est-det-cuerpo';

    const ulErr = document.createElement('ul');
    ulErr.className = 'est-lista-err';
    for (const err of c.errores || []) {
      const li = document.createElement('li');
      li.textContent = err;
      ulErr.appendChild(li);
    }
    boxErr.appendChild(ulErr);

    if (Array.isArray(c.confundir_con) && c.confundir_con.length) {
      const titConf = document.createElement('div');
      titConf.className = 'est-sublbl';
      titConf.textContent = 'No confundir con:';
      boxErr.appendChild(titConf);

      const listaConf = document.createElement('div');
      listaConf.className = 'est-confusiones';
      for (const conf of c.confundir_con) {
        const gem = this.porId.get(conf.id);
        const fila = document.createElement('div');
        fila.className = 'est-conf-fila';

        const btnGem = document.createElement('button');
        btnGem.type = 'button';
        btnGem.className = `est-link-celda form-c-${gem ? gem.color : 'azul'}`;
        btnGem.textContent = gem ? `${gem.id} · ${gem.nombre}` : conf.id;
        btnGem.addEventListener('click', () => this.seleccionar(conf.id));

        const mot = document.createElement('span');
        mot.className = 'est-conf-mot';
        mot.textContent = conf.motivo;

        fila.append(btnGem, mot);
        listaConf.appendChild(fila);
      }
      boxErr.appendChild(listaConf);
    }
    detErr.append(sumErr, boxErr);
    acor.appendChild(detErr);

    // 2. Derivaciones permitidas desde esta casilla
    if (Array.isArray(c.se_deriva) && c.se_deriva.length) {
      const detDer = document.createElement('details');
      detDer.className = 'est-det';
      const sumDer = document.createElement('summary');
      sumDer.textContent = `Qué se puede derivar desde ${c.id} (${c.se_deriva.length})`;
      const boxDer = document.createElement('div');
      boxDer.className = 'est-det-cuerpo est-deriv-lista';
      const derivW = Math.max(220, (this._ficha.clientWidth || 520) - 60);
      for (const d of c.se_deriva) {
        const item = document.createElement('div');
        item.className = 'est-deriv-item';
        const td = new TintaMate(d.tex, { px: 23, color: c.color, maxAncho: derivW });
        td.progreso(1);
        const como = document.createElement('p');
        como.textContent = d.como;
        item.append(td.el, como);
        boxDer.appendChild(item);
      }
      detDer.append(sumDer, boxDer);
      acor.appendChild(detDer);
    }

    // 3. Practicar en la pizarra
    if (Array.isArray(c.ejemplos) && c.ejemplos.length) {
      const detEj = document.createElement('details');
      detEj.className = 'est-det';
      detEj.open = true;
      const sumEj = document.createElement('summary');
      sumEj.textContent = `Practicar en la pizarra (${c.ejemplos.length})`;
      const boxEj = document.createElement('div');
      boxEj.className = 'est-det-cuerpo est-ej-lista';
      for (const ej of c.ejemplos) {
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'est-ej-btn';
        const codEj = document.createElement('span');
        codEj.className = 'cod';
        codEj.textContent = ej.id;
        const titEj = document.createElement('strong');
        titEj.textContent = ej.titulo;
        const tipEj = document.createElement('span');
        tipEj.className = 'est-ej-tipo';
        tipEj.textContent = ej.tipo === 'seleccion' ? 'Selección' : 'Desarrollo';
        b.append(codEj, titEj, tipEj);
        b.addEventListener('click', () => this.acciones.abrir(ej.id));
        boxEj.appendChild(b);
      }
      detEj.append(sumEj, boxEj);
      acor.appendChild(detEj);
    }

    frag.appendChild(acor);
    this._ficha.replaceChildren(frag);
  }

  /* ------------------------------------------------------------------ MODO 2: ¿QUÉ FÓRMULA USO? */

  _pintarDecisiones() {
    const cont = document.createElement('div');
    cont.className = 'est-dec-wrap';

    const buscar = document.createElement('input');
    buscar.type = 'search';
    buscar.className = 'est-buscar';
    buscar.value = this.busquedaDec;
    buscar.placeholder = 'Describe el enunciado (ej. cono, hielo, barra sujeta, antihorario, Otto, °F)…';
    buscar.setAttribute('aria-label', 'Buscar por síntoma del problema');

    const listaEl = document.createElement('div');
    listaEl.className = 'est-dec-lista';

    const pintarLista = () => {
      const q = plano(this.busquedaDec.trim());
      const toks = q ? q.split(/\s+/).filter(Boolean) : [];
      const items = (this.guia.decisiones || []).filter((d) => {
        if (!toks.length) return true;
        const nombres = (d.celdas || []).map((cid) => `${cid} ${this.porId.get(cid)?.nombre || ''}`).join(' ');
        const blob = plano(`${d.sintoma} ${d.trampa} ${d.leccion} ${nombres}`);
        return toks.every((t) => blob.includes(t));
      });

      if (!items.length) {
        listaEl.innerHTML = '<p class="vacio">No encontré ese síntoma. Prueba con una palabra más corta (como «cono», «hielo», «isoterma» o «Otto»).</p>';
        return;
      }

      const frag = document.createDocumentFragment();
      for (const d of items) {
        const fila = document.createElement('article');
        fila.className = 'est-dec-item';

        const top = document.createElement('div');
        top.className = 'est-dec-top';
        const h3 = document.createElement('h2');
        h3.className = 'est-dec-sintoma';
        h3.textContent = d.sintoma;

        const btnPr = document.createElement('button');
        btnPr.type = 'button';
        btnPr.className = 'btn btn-suave est-dec-prac';
        btnPr.textContent = `Practicar ${d.leccion}`;
        btnPr.addEventListener('click', () => this.acciones.abrir(d.leccion));
        top.append(h3, btnPr);

        const celdasRow = document.createElement('div');
        celdasRow.className = 'est-dec-celdas';
        for (const cid of d.celdas || []) {
          const c = this.porId.get(cid);
          const b = document.createElement('button');
          b.type = 'button';
          b.className = `est-link-celda form-c-${c ? c.color : 'azul'}`;
          b.textContent = c ? `${c.id} · ${c.nombre}` : cid;
          b.addEventListener('click', () => this.seleccionar(cid, { irAHoja: true }));
          celdasRow.appendChild(b);
        }

        const trampa = document.createElement('p');
        trampa.className = 'est-dec-trampa';
        const ojo = document.createElement('strong');
        ojo.textContent = 'Cuidado: ';
        trampa.append(ojo, document.createTextNode(d.trampa));

        fila.append(top, celdasRow, trampa);
        frag.appendChild(fila);
      }
      listaEl.replaceChildren(frag);
    };

    buscar.addEventListener('input', () => {
      this.busquedaDec = buscar.value;
      pintarLista();
    });

    // Sección explícita: Qué NO viene en el formulario II-2026
    const fuera = document.createElement('section');
    fuera.className = 'est-fuera-sec';
    const hFuera = document.createElement('h2');
    hFuera.className = 'est-fuera-tit';
    hFuera.textContent = 'Qué NO viene en el formulario II-2026';
    fuera.appendChild(hFuera);

    const fueraLista = document.createElement('div');
    fueraLista.className = 'est-fuera-lista';
    for (const item of this.guia.fuera_de_hoja || []) {
      const box = document.createElement('div');
      box.className = 'est-fuera-item';
      const t = document.createElement('strong');
      t.textContent = item.tema;
      const h = document.createElement('span');
      h.className = 'est-fuera-hist';
      h.textContent = item.historial;
      const n = document.createElement('p');
      n.textContent = item.nota;
      box.append(t, h, n);
      fueraLista.appendChild(box);
    }
    fuera.appendChild(fueraLista);

    cont.append(buscar, listaEl, fuera);
    this._cuerpo.replaceChildren(cont);
    pintarLista();
  }

  /* ------------------------------------------------------------------ MODO 3: ENTRENADOR */

  _prepararEntrenador(mezclar = false) {
    if (!this.guia) return;
    const base = (this.guia.entrenador || []).filter((e) => this.entCat === 'todas' || e.categoria === this.entCat);
    const lista = [...base];
    if (mezclar) {
      for (let i = lista.length - 1; i > 0; i -= 1) {
        const j = Math.floor(Math.random() * (i + 1));
        [lista[i], lista[j]] = [lista[j], lista[i]];
      }
    }
    this.entOrden = lista;
    this.entIdx = 0;
    this.entEval = null;
  }

  _opcionesRapidas(reto) {
    const elegidas = [];
    const agrega = (id) => {
      if (id && this.porId.has(id) && !elegidas.includes(id)) elegidas.push(id);
    };
    agrega(reto.celdas_validas[0]);
    agrega(reto.distractor);
    const cBase = this.porId.get(reto.celdas_validas[0]);
    if (cBase) {
      for (const g of cBase.confundir_con || []) {
        if (elegidas.length < 4) agrega(g.id);
      }
      for (const otra of this.guia.celdas) {
        if (elegidas.length < 4 && otra.bloque === cBase.bloque) agrega(otra.id);
      }
    }
    for (const otra of this.guia.celdas) {
      if (elegidas.length < 4) agrega(otra.id);
    }
    // Orden determinista por id de reto para que no siempre quede primera la correcta
    const sem = [...reto.id].reduce((acc, ch) => acc + ch.charCodeAt(0), 0);
    return elegidas
      .map((id, i) => ({ id, k: ((sem + i * 17) * 29) % 97 }))
      .sort((a, b) => a.k - b.k)
      .map((x) => x.id);
  }

  _responderEntrenador(celdaId) {
    if (this.entEval || !this.entOrden.length) return;
    const reto = this.entOrden[this.entIdx];
    const acerto = (reto.celdas_validas || []).includes(celdaId);
    this.entIntentos += 1;
    if (acerto) this.entAciertos += 1;
    this.entEval = { elegida: celdaId, validas: reto.celdas_validas, acerto };
    this._aplicarEstadosEval(this.entEval);
    this._pintarPanelEntrenador();
  }

  _pintarEntrenador() {
    if (!this.entOrden.length) this._prepararEntrenador(false);

    const cont = document.createElement('div');
    cont.className = 'est-ent-wrap';

    // Barra compacta del entrenador
    const top = document.createElement('div');
    top.className = 'est-ent-barra';

    const filtros = document.createElement('div');
    filtros.className = 'filtros';
    for (const [id, lbl] of Object.entries(CAT_ENTRENADOR)) {
      const n = (this.guia.entrenador || []).filter((e) => id === 'todas' || e.categoria === id).length;
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'filtro';
      b.setAttribute('aria-pressed', String(this.entCat === id));
      b.textContent = `${lbl} (${n})`;
      b.addEventListener('click', () => {
        this.entCat = id;
        this._prepararEntrenador(false);
        this._pintarEntrenador();
      });
      filtros.appendChild(b);
    }

    const der = document.createElement('div');
    der.className = 'est-ent-der';
    const marc = document.createElement('span');
    marc.className = 'est-ent-score';
    marc.id = 'est-ent-score';
    marc.textContent = `Aciertos: ${this.entAciertos} / ${this.entIntentos}`;
    const btnMez = document.createElement('button');
    btnMez.type = 'button';
    btnMez.className = 'btn btn-suave';
    btnMez.textContent = 'Mezclar retos';
    btnMez.addEventListener('click', () => {
      this._prepararEntrenador(true);
      this._pintarEntrenador();
    });
    der.append(marc, btnMez);
    top.append(filtros, der);

    const grid = document.createElement('div');
    grid.className = 'est-grid';

    const colHoja = document.createElement('div');
    colHoja.className = 'est-hoja-col';
    const hoja = this._construirHoja42({
      modo: 'entrenador',
      evalEstado: this.entEval,
      alClic: (cid) => this._responderEntrenador(cid),
    });
    const pie = document.createElement('p');
    pie.className = 'est-hoja-pie';
    pie.textContent = 'Toca directamente la casilla correcta en la hoja oficial del examen.';
    colHoja.append(hoja, pie);

    const panel = document.createElement('section');
    panel.className = 'est-ficha-col est-ent-panel';
    panel.setAttribute('aria-live', 'polite');
    this._ficha = panel;

    grid.append(colHoja, panel);
    cont.append(top, grid);
    this._cuerpo.replaceChildren(cont);
    this._pintarPanelEntrenador();
  }

  _pintarPanelEntrenador() {
    if (!this._ficha || !this.entOrden.length) return;
    const score = this.el.querySelector('#est-ent-score');
    if (score) score.textContent = `Aciertos: ${this.entAciertos} / ${this.entIntentos}`;

    const reto = this.entOrden[this.entIdx];
    const frag = document.createDocumentFragment();

    const meta = document.createElement('div');
    meta.className = 'est-ficha-eyebrow';
    const num = document.createElement('span');
    num.className = 'est-cod';
    num.textContent = `Reto ${this.entIdx + 1} de ${this.entOrden.length}`;
    const cat = document.createElement('span');
    cat.className = `est-tag est-tag-${reto.categoria === 'nuevas' ? 'nueva' : reto.categoria === 'trampas' ? 'reintroducida' : 'fija'}`;
    cat.textContent = CAT_ENTRENADOR[reto.categoria] || 'General';
    meta.append(num, cat);

    const enun = document.createElement('p');
    enun.className = 'est-ent-enun';
    enun.textContent = reto.enunciado;

    const instr = document.createElement('p');
    instr.className = 'est-ent-instr';
    instr.textContent = this.entEval
      ? 'Revisa la casilla marcada en verde en la hoja:'
      : 'Toca la casilla correcta en la hoja oficial o elige una de las 4 opciones rápidas:';

    frag.append(meta, enun, instr);

    // 4 opciones rápidas (accesibles en móvil/teclado sin tener que hacer zoom a la hoja)
    const opts = document.createElement('div');
    opts.className = 'est-ent-opts';
    for (const cid of this._opcionesRapidas(reto)) {
      const c = this.porId.get(cid);
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'est-ent-opt';
      b.disabled = Boolean(this.entEval);
      if (this.entEval) {
        if (this.entEval.validas.includes(cid)) b.classList.add('bien');
        else if (this.entEval.elegida === cid) b.classList.add('mal');
      }
      const cod = document.createElement('span');
      cod.className = 'cod';
      cod.textContent = cid;
      const nom = document.createElement('span');
      nom.textContent = c ? c.nombre : cid;
      b.append(cod, nom);
      b.addEventListener('click', () => this._responderEntrenador(cid));
      opts.appendChild(b);
    }
    frag.appendChild(opts);

    // Resultado tras responder
    if (this.entEval) {
      const box = document.createElement('div');
      box.className = `est-eval-box ${this.entEval.acerto ? 'bien' : 'mal'}`;

      const targetId = reto.celdas_validas[0];
      const target = this.porId.get(targetId);
      const eleg = this.porId.get(this.entEval.elegida);

      const tit = document.createElement('strong');
      tit.textContent = this.entEval.acerto
        ? `Correcto · ${targetId} (${target ? target.nombre : ''})`
        : `Elegiste ${this.entEval.elegida} (${eleg ? eleg.nombre : ''}) · La casilla correcta es ${targetId} (${target ? target.nombre : ''})`;

      const exp = document.createElement('p');
      exp.textContent = reto.explicacion;

      const accionesFila = document.createElement('div');
      accionesFila.className = 'fila-botones';

      const btnSig = document.createElement('button');
      btnSig.type = 'button';
      btnSig.className = 'btn btn-principal';
      btnSig.textContent = this.entIdx + 1 < this.entOrden.length ? 'Siguiente reto' : 'Volver al primer reto';
      btnSig.addEventListener('click', () => {
        this.entIdx = (this.entIdx + 1) % this.entOrden.length;
        this.entEval = null;
        this._pintarEntrenador();
      });

      const btnFicha = document.createElement('button');
      btnFicha.type = 'button';
      btnFicha.className = 'btn';
      btnFicha.textContent = `Ver ficha de ${targetId}`;
      btnFicha.addEventListener('click', () => this.seleccionar(targetId, { irAHoja: true }));

      const btnPrac = document.createElement('button');
      btnPrac.type = 'button';
      btnPrac.className = 'btn btn-suave';
      btnPrac.textContent = `Practicar ${reto.leccion}`;
      btnPrac.addEventListener('click', () => this.acciones.abrir(reto.leccion));

      accionesFila.append(btnSig, btnFicha, btnPrac);
      box.append(tit, exp, accionesFila);
      frag.appendChild(box);
    }

    this._ficha.replaceChildren(frag);
  }
}
