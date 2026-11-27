/**
 * Ayuda dentro de la lección, pensada para estudiar en pasos cortos: un solo botón «Ayuda» (antes eran cuatro botones
 * siempre a la vista compitiendo con «Siguiente») con «¿En qué iba?», «Más despacio»,
 * «¿De dónde salió?» y «¿Por qué?», cada una con su tecla.
 *
 * El resumen «¿En qué iba?» ya no depende de acordarse de pulsarlo: se abre solo cuando la persona
 * vuelve después de un rato sin actividad o cuando la ventana recupera el foco. Su meta dice qué hay
 * que hallar (la meta del plan o los incisos del enunciado), no repite el título.
 */

import { ponerTextoMate } from './preparacion.js';

/* ------------------------------------------------------------------ meta: qué hay que hallar */

const GRIEGAS = {
  alpha: 'α', beta: 'β', gamma: 'γ', delta: 'δ', Delta: 'Δ', epsilon: 'ε', varepsilon: 'ε', eta: 'η',
  theta: 'θ', Theta: 'Θ', lambda: 'λ', mu: 'μ', nu: 'ν', rho: 'ρ', sigma: 'σ', Sigma: 'Σ', tau: 'τ',
  phi: 'φ', varphi: 'φ', omega: 'ω', Omega: 'Ω', pi: 'π',
};

/** Texto de una línea sin marcas de TeX; deja `_` y `^` para que se vean como sub y superíndices. */
export function textoLegible(s) {
  return String(s || '')
    .replace(/\*\*/g, '')
    .replace(/\$([^$]*)\$/g, (_, m) => m
      .replace(/\\(?:text|mathrm|mathit|mathbf|operatorname)\s*\{([^}]*)\}/g, '$1')
      .replace(/\{,\}/g, ',')
      .replace(/\^\s*\{?\\circ\}?/g, '°')
      .replace(/\\circ/g, '°')
      .replace(/\\times/g, '×')
      .replace(/\\cdot/g, '·')
      .replace(/\\(?:,|;|:|!|\s)/g, ' ')
      .replace(/\\([A-Za-z]+)/g, (_t, n) => GRIEGAS[n] || n)
      .replace(/(^|[^_^])\{([^{}]*)\}/g, '$1$2'))
    .replace(/\s+/g, ' ')
    .trim();
}

/** Frases de un texto, cortadas en . : ? ! que no estén dentro de $…$ ni en medio de un número. */
function frasesDe(s) {
  const t = String(s || '');
  const out = [];
  let mate = false;
  let ini = 0;
  for (let i = 0; i < t.length; i += 1) {
    const c = t[i];
    if (c === '$') { mate = !mate; continue; }
    if (mate || !'.:?!'.includes(c)) continue;
    if (i + 1 < t.length && !/\s/.test(t[i + 1])) continue;
    const txt = t.slice(ini, i).trim();
    if (txt) out.push({ txt, fin: c });
    ini = i + 1;
  }
  const resto = t.slice(ini).trim();
  if (resto) out.push({ txt: resto, fin: '' });
  return out;
}

const conFin = (f) => f.txt + (f.fin === '?' || f.fin === '!' ? f.fin : '');

/** Incisos a), b), c)… del enunciado, en orden, con la primera frase de cada uno. */
export function incisosDe(enunciado) {
  const s = String(enunciado || '');
  const re = /(^|\s)\(?([a-h])\)\s+/g;
  const marcas = [];
  let m;
  while ((m = re.exec(s)) !== null) {
    const cab = m.index + m[1].length;
    if (((s.slice(0, cab).match(/\$/g) || []).length % 2) === 1) continue; // dentro de $…$
    if (m[2] !== String.fromCharCode(97 + marcas.length)) continue; // solo a, b, c… seguidas
    marcas.push({ letra: m[2], cab, ini: m.index + m[0].length });
  }
  return marcas
    .map((mk, k) => {
      const frase = frasesDe(s.slice(mk.ini, k + 1 < marcas.length ? marcas[k + 1].cab : s.length))[0];
      return { letra: mk.letra, texto: frase ? conFin(frase) : '' };
    })
    .filter((x) => x.texto);
}

const VERBO = /\b(halle|hallar|encuentre|encontrar|calcule|calcular|determine|determinar|obtenga|obtener|estime|estimar|compare|comparar|explique|explicar|demuestre|demostrar)\b/i;

function recortar(s, n) {
  return s.length > n ? `${s.slice(0, n - 1).trimEnd()}…` : s;
}

function unirY(xs) {
  return xs.length < 2 ? (xs[0] || '') : `${xs.slice(0, -1).join(', ')} y ${xs[xs.length - 1]}`;
}

/** Variables que piden las respuestas («T_f=100 °C» → «T_f»). */
function variablesDe(respuestas) {
  const vs = [];
  for (const r of respuestas || []) {
    const tex = String(r.tex || '');
    const k = tex.indexOf('=');
    if (k <= 0) continue;
    const izq = tex.slice(0, k).trim();
    if (izq.length > 16 || /\\(?:text|mathrm)/.test(izq)) continue;
    const v = textoLegible(`$${izq}$`);
    if (v && !vs.includes(v)) vs.push(v);
  }
  return vs;
}

/**
 * Qué hay que hallar en esta lección. Devuelve `{ incisos: [{ letra, texto }] }` o `{ texto }`.
 * Orden: la meta del plan de Preparación; los incisos del enunciado; la pregunta o la orden
 * («Halle…») del enunciado; en selección, la frase que hay que completar; las variables de las
 * respuestas; y solo si no hay nada más, el título.
 */
export function metaDeLeccion(lec, metaExterna = '') {
  if (metaExterna) return { texto: textoLegible(metaExterna) };
  if (!lec) return { texto: '' };
  const enun = String(lec.enunciado || '');
  const incisos = incisosDe(enun);
  if (incisos.length) return { incisos: incisos.map((x) => ({ letra: x.letra, texto: textoLegible(x.texto) })) };
  const frases = frasesDe(enun);
  if (lec.tipo === 'teoria' && frases.length) return { texto: textoLegible(conFin(frases[0])) };
  const pregunta = [...frases].reverse().find((f) => f.fin === '?' && f.txt.includes('¿'));
  if (pregunta) {
    const q = `${pregunta.txt.slice(pregunta.txt.indexOf('¿'))}?`.replace(/^¿\s*(\p{Ll})/u, (_m, l) => `¿${l.toUpperCase()}`);
    return { texto: recortar(textoLegible(q), 160) };
  }
  const orden = frases.find((f) => VERBO.test(f.txt));
  if (orden) {
    const t = orden.txt.slice(orden.txt.search(VERBO));
    return { texto: recortar(textoLegible(t.charAt(0).toUpperCase() + t.slice(1)), 160) };
  }
  if (lec.tipo === 'seleccion') {
    const abierta = [...frases].reverse().find((f) => f.fin === ':');
    if (abierta) {
      let frase = abierta.txt;
      const k = frase.lastIndexOf(', ');
      if (k > 0 && frase.slice(k + 2).split(/\s+/).length >= 4) frase = `…${frase.slice(k + 2)}`;
      return { texto: `Elegir la opción que completa: «${recortar(textoLegible(frase), 110)}…»` };
    }
    return { texto: 'Elegir la opción correcta' };
  }
  const vars = variablesDe(lec.respuestas);
  if (vars.length) return { texto: `Hallar ${unirY(vars)}` };
  return { texto: textoLegible(lec.titulo || '') };
}

/* ------------------------------------------------------------------ ayuda de la lección */

/** Teclas de las opciones (también se ven en el menú). */
const TECLAS = { i: 'resumen', d: 'despacio', o: 'origen', p: 'porque' };

/** Abre un popover ligado a su botón (si el navegador acepta `source`) sin fallar si ya está abierto. */
function abrirPopover(el, source) {
  if (el.matches(':popover-open')) return;
  try { el.showPopover({ source }); } catch { try { el.showPopover(); } catch { /* fuera del documento */ } }
}

function hayAlgoAbierto() {
  try { return Boolean(document.querySelector(':popover-open, dialog[open]')); } catch { return Boolean(document.querySelector('dialog[open]')); }
}

export class AyudaLeccion {
  /**
   * @param {object} o
   * @param {HTMLButtonElement} o.boton  botón «Ayuda» del transporte
   * @param {HTMLElement} o.menu         menú (popover) con las opciones
   * @param {HTMLElement} o.resumen      popover «¿En qué iba?»
   * @param {() => {meta?: string, volverA?: string}} o.contexto  de dónde se abrió la lección
   */
  constructor({ boton, menu, resumen, rep, asistente, lateral, acciones, contexto }) {
    Object.assign(this, { boton, menu, resumen, rep, asistente, lateral, acciones, contexto });
    // Regreso: 2 min sin tocar nada (sin contar el tiempo en que el profesor habla) o volver a la
    // ventana tras un rato fuera. Un vistazo de pocos segundos a otra ventana no cuenta.
    this.umbral = { inactivo: 2 * 60 * 1000, fuera: 15 * 1000 };
    this.ultima = Date.now();
    this.fueraDesde = 0;
    this._teclado = false;

    this.menu.addEventListener('click', (e) => {
      const b = e.target.closest('[data-ayuda]');
      if (!b) return;
      this.menu.hidePopover?.();
      this.ejecutar(b.dataset.ayuda);
    });
    this.menu.addEventListener('keydown', (e) => this._teclaMenu(e));
    this.menu.addEventListener('beforetoggle', (e) => {
      if (e.newState !== 'open') return;
      this.alPaso();
      this._anclar(this.menu);
    });
    this.menu.addEventListener('toggle', (e) => {
      if (e.newState === 'open' && this._teclado) this._items()[0]?.focus();
      this._teclado = false;
    });
    this.resumen.addEventListener('beforetoggle', (e) => {
      if (e.newState !== 'open') return;
      this.actualizar();
      this._anclar(this.resumen);
    });

    const actividad = () => this._actividad();
    for (const ev of ['pointerdown', 'pointermove', 'keydown', 'wheel', 'touchstart']) {
      window.addEventListener(ev, actividad, { capture: true, passive: true });
    }
    document.addEventListener('visibilitychange', () => { if (document.hidden) this._salir(); else this._volver(); });
    window.addEventListener('blur', () => this._salir());
    window.addEventListener('focus', () => this._volver());
    // Escuchar al profesor también es estar presente
    setInterval(() => { if (this._escuchando()) this.ultima = Date.now(); }, 5000);
  }

  /* ---------------------------------------------------------------- opciones */

  _disponible(op) {
    const lec = this.rep.lec;
    const paso = this.rep.paso;
    if (!lec) return false;
    if (op === 'origen') return Boolean((paso && paso.origen && paso.origen.length) || (lec.formulas && lec.formulas.length));
    if (op === 'porque') return Boolean(paso && paso.porque);
    if (op === 'preguntar') {
      // Solo cuando la barra de preguntas está oculta (modo enfoque): si se ve, sería un control repetido
      const barra = document.querySelector('.preguntar');
      return Boolean(barra) && !barra.getClientRects().length;
    }
    return true;
  }

  /** Muestra en el menú solo las opciones que sirven en este paso. */
  alPaso() {
    for (const b of this.menu.querySelectorAll('[data-ayuda]')) b.hidden = !this._disponible(b.dataset.ayuda);
    const sep = this.menu.querySelector('[data-ayuda-sep]');
    if (sep) sep.hidden = !this._disponible('preguntar');
  }

  _items() {
    return [...this.menu.querySelectorAll('[data-ayuda]')].filter((b) => !b.hidden);
  }

  ejecutar(op) {
    if (!this.rep.lec || !this._disponible(op)) return;
    if (op === 'resumen') this.mostrarResumen();
    else if (op === 'despacio') this.asistente.despacio();
    else if (op === 'origen') this.asistente.origen();
    else if (op === 'porque') this.asistente.porque();
    else if (op === 'preguntar') this.acciones.preguntar();
  }

  abrirMenu({ teclado = false } = {}) {
    if (this.menu.matches(':popover-open')) { this.menu.hidePopover(); return; }
    this._teclado = teclado;
    abrirPopover(this.menu, this.boton);
  }

  /** Atajos de la ayuda. Devuelve true si la tecla era suya. */
  tecla(e) {
    const k = String(e.key || '').toLowerCase();
    if (k === 'a') { this.abrirMenu({ teclado: true }); return true; }
    const op = TECLAS[k];
    if (!op) return false;
    if (this.menu.matches(':popover-open')) this.menu.hidePopover();
    if (op === 'resumen' && this.resumen.matches(':popover-open')) this.resumen.hidePopover();
    else this.ejecutar(op);
    return true;
  }

  _teclaMenu(e) {
    const items = this._items();
    if (!items.length) return;
    const i = items.indexOf(document.activeElement);
    let n = null;
    if (e.key === 'ArrowDown') n = items[(i + 1) % items.length];
    else if (e.key === 'ArrowUp') n = items[(i - 1 + items.length) % items.length];
    else if (e.key === 'Home') n = items[0];
    else if (e.key === 'End') n = items[items.length - 1];
    if (!n) return;
    e.preventDefault();
    e.stopPropagation();
    n.focus();
  }

  /** Pone un popover justo encima del botón «Ayuda», alineado a su borde derecho. */
  _anclar(pop) {
    const b = this.boton;
    const r = b && b.getClientRects().length ? b.getBoundingClientRect() : null;
    pop.classList.toggle('anclado', Boolean(r));
    if (!r) { pop.style.removeProperty('right'); pop.style.removeProperty('bottom'); return; }
    pop.style.right = `${Math.max(8, Math.round(innerWidth - r.right))}px`;
    pop.style.bottom = `${Math.max(8, Math.round(innerHeight - r.top + 8))}px`;
  }

  /* ---------------------------------------------------------------- resumen «¿En qué iba?» */

  mostrarResumen() {
    if (!this.rep.lec) return;
    abrirPopover(this.resumen, this.boton);
  }

  actualizar() {
    const caja = this.resumen;
    const { rep, lateral, acciones } = this;
    if (!caja || !rep.lec) return;
    const lec = rep.lec;
    const ctx = (this.contexto && this.contexto()) || {};
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

    // 1. Qué hay que hallar (no el título: ese ya está arriba en la barra)
    const meta = metaDeLeccion(lec, ctx.meta);
    const dt1 = document.createElement('dt');
    dt1.textContent = '1. Meta';
    const dd1 = document.createElement('dd');
    if (meta.incisos) {
      const ul = document.createElement('ul');
      ul.className = 'atencion-res-incisos';
      for (const x of meta.incisos) {
        const li = document.createElement('li');
        const letra = document.createElement('span');
        letra.className = 'atencion-res-letra';
        letra.textContent = `${x.letra})`;
        const t = document.createElement('span');
        ponerTextoMate(t, x.texto);
        li.append(letra, t);
        ul.appendChild(li);
      }
      dd1.appendChild(ul);
    } else {
      ponerTextoMate(dd1, meta.texto);
    }
    dl.append(dt1, dd1);

    // 2. Dónde va (el número del paso ya está en la cabecera)
    const dt2 = document.createElement('dt');
    dt2.textContent = '2. Dónde vas ahora';
    const dd2 = document.createElement('dd');
    ponerTextoMate(dd2, paso ? textoLegible(paso.titulo) : 'Inicio del problema');
    dl.append(dt2, dd2);

    // 3. Fórmula activa del formulario oficial
    const dt3 = document.createElement('dt');
    dt3.textContent = '3. Fórmula base activa';
    const dd3 = document.createElement('dd');
    dd3.className = 'atencion-res-celdas';
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
    bSeg.addEventListener('click', () => {
      caja.hidePopover?.();
      if (rep.estado === 'pausado') rep.seguir();
    });
    pie.appendChild(bSeg);
    if (ctx.volverA === 'preparacion') {
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

  /* ---------------------------------------------------------------- regreso */

  _escuchando() {
    return ['hablando', 'cargando'].includes(this.rep.estado) || Boolean(this.rep._mini) || Boolean(this.asistente && this.asistente.pensando);
  }

  _actividad() {
    const ahora = Date.now();
    const quieto = ahora - this.ultima;
    this.ultima = ahora;
    if (this.fueraDesde) return; // el regreso a la ventana lo decide _volver()
    if (quieto >= this.umbral.inactivo) this._ofrecer();
  }

  _salir() {
    if (!this.fueraDesde) this.fueraDesde = Date.now();
  }

  _volver() {
    if (!this.fueraDesde) return;
    const fuera = Date.now() - this.fueraDesde;
    this.fueraDesde = 0;
    this.ultima = Date.now();
    if (fuera >= this.umbral.fuera) this._ofrecer();
  }

  /** ¿Tiene sentido abrir el resumen ahora? Solo en una lección a medias y sin tapar otra cosa. */
  _puedeAbrir() {
    if (document.hidden || document.body.dataset.vista !== 'pizarra') return false;
    if (!this.rep.lec || this.rep.estado === 'fin' || this.rep.estado === 'vacio') return false;
    if (hayAlgoAbierto()) return false;
    const a = document.activeElement;
    if (a && (/^(INPUT|TEXTAREA|SELECT)$/.test(a.tagName) || a.isContentEditable)) return false;
    return true;
  }

  _ofrecer() {
    // Un instante después: si el regreso fue un clic, que ese clic no cierre el resumen recién abierto
    setTimeout(() => { if (this._puedeAbrir()) this.mostrarResumen(); }, 350);
  }
}
