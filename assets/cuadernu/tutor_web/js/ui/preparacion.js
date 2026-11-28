/**
 * Preparación exhaustiva para III Parcial Extraordinario (≤ 20.5) y Exámenes de Reposición (3 Parciales).
 *
 * Diseñada para pasos muy cortos siguiendo guías de interfaz:
 * - Una sola tarea protagonista ("Tu siguiente paso ahora") para eliminar parálisis por elección.
 * - Micro-sprints no punitivos de 10 o 15 min con aviso sereno de descanso.
 * - Calentamiento activo de 15 s (3 opciones de fórmula base) con opción directa de saltar.
 * - Micro-bloques de 3 a 5 ejercicios en acordeones `<details>` no exclusivos (solo el activo abierto).
 * - Transparencia total sobre los 4 problemas y 5 incisos omitidos por superar la sección 20.5.
 */

import { ajustes, bus, guardarAjuste, pedir } from '../nucleo/api.js';
import { banco, estaHecha, nombreTema } from './banco.js';
import { PanelIntensivo } from './intensivo.js';

const CLAVE_PREP = 'tutor.preparacion.atencion.v1';
// 'sprint6' = Sprint 6 días desde cero; 'repo26' = plan diario Reposición II-2026
const TABS = ['sprint6', 'repo26', 'extraordinario', 'radar', 'reposicion'];

/** Nombre del tipo de ejercicio para las etiquetas. */
function tipoTexto(t) {
  return t === 'seleccion' ? 'Selección' : t === 'teoria' ? 'Teoría' : 'Desarrollo';
}

/** Fecha local de hoy en formato AAAA-MM-DD (para marcar el día del calendario). */
function hoyISO() {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

const ICO = {
  play: 'M8 5v14l11-7L8 5Z',
  pausa: 'M7 5h3.5v14H7V5Zm6.5 0H17v14h-3.5V5Z',
  reloj: 'M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm1 5v4.6l3.5 2.1-1 1.7L11 12.8V7h2Z',
  reiniciar: 'M12 5V1.8L7 6.5l5 4.7V7.5a5 5 0 1 1-5 5H4.5A7.5 7.5 0 1 0 12 5Z',
  check: 'M9 16.2 4.8 12l-1.4 1.4L9 19 21 7l-1.4-1.4L9 16.2Z',
};

function svg(d) {
  return `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="${d}"/></svg>`;
}

function cargarEstado() {
  const base = { tab: 'repo26', sprintMin: 10, filtroEx: '', filtroPilar: 0, filtroRadar: 'todos', soloPend: false, calOk: {} };
  try {
    const raw = JSON.parse(localStorage.getItem(CLAVE_PREP) || '{}');
    if (raw && typeof raw === 'object') {
      if (TABS.includes(raw.tab)) base.tab = raw.tab;
      if ([0, 10, 15].includes(raw.sprintMin)) base.sprintMin = raw.sprintMin;
      if (typeof raw.filtroEx === 'string') base.filtroEx = raw.filtroEx;
      if ([0, 1, 2, 3].includes(raw.filtroPilar)) base.filtroPilar = raw.filtroPilar;
      if (['todos', 'hoy', 'extremo'].includes(raw.filtroRadar)) base.filtroRadar = raw.filtroRadar;
      if (typeof raw.soloPend === 'boolean') base.soloPend = raw.soloPend;
      if (raw.calOk && typeof raw.calOk === 'object') base.calOk = raw.calOk;
    }
  } catch { /* modo privado */ }
  return base;
}

function guardarEstado(st) {
  try {
    localStorage.setItem(CLAVE_PREP, JSON.stringify({
      tab: st.tab, sprintMin: st.sprintMin, filtroEx: st.filtroEx,
      filtroPilar: st.filtroPilar, filtroRadar: st.filtroRadar, soloPend: st.soloPend, calOk: st.calOk,
    }));
  } catch { /* modo privado */ }
}

/** Permuta determinísticamente las 3 opciones según el ID para que la correcta no sea siempre la primera. */
function ordenarOpciones(id, cal) {
  const ops = (cal && Array.isArray(cal.opciones)) ? cal.opciones : [];
  const corr = Number(cal && cal.correcta) || 0;
  let hash = 0;
  for (let i = 0; i < id.length; i += 1) hash = (hash * 31 + id.charCodeAt(i)) % 997;
  const shift = ops.length ? hash % ops.length : 0;
  return ops.map((_, idx) => {
    const orig = (idx + shift) % ops.length;
    return { texto: ops[orig], esCorrecta: orig === corr };
  });
}

function formatoReloj(seg) {
  const s = Math.max(0, Math.floor(seg));
  const m = Math.floor(s / 60);
  const r = s % 60;
  return `${String(m).padStart(2, '0')}:${String(r).padStart(2, '0')}`;
}

/** Convierte subíndices/superíndices ASCII (Q_H, W_neto, pV^γ, T^(γ−1)) a nodos DOM reales <sub>/<sup>. */
export function ponerTextoMate(el, texto) {
  el.replaceChildren();
  const s = String(texto || '');
  const re = /_\{([^}]+)\}|_([A-Za-z0-9ÁÉÍÓÚáéíóú⊥∥]+)|\^\{([^}]+)\}|\^\(([^)]+)\)|\^([A-Za-z0-9γ−\-+]+)/g;
  let ult = 0;
  let m;
  while ((m = re.exec(s)) !== null) {
    if (m.index > ult) el.appendChild(document.createTextNode(s.slice(ult, m.index)));
    if (m[1] !== undefined || m[2] !== undefined) {
      const sub = document.createElement('sub');
      sub.textContent = m[1] ?? m[2];
      el.appendChild(sub);
    } else {
      const sup = document.createElement('sup');
      sup.textContent = m[3] ?? m[4] ?? m[5];
      el.appendChild(sup);
    }
    ult = re.lastIndex;
  }
  if (ult < s.length) el.appendChild(document.createTextNode(s.slice(ult)));
}

export class PreparacionExamenes {
  constructor({ el, acciones }) {
    this.el = el;
    this.acciones = acciones;
    this.datos = null;
    this.cargando = false;
    this.st = cargarEstado();
    this.calElegido = {}; // { [id]: { idx, ok } }
    this.sprintRestante = this.st.sprintMin * 60;
    this.sprintActivo = false;
    this.sprintPausaAviso = false;
    this._timer = null;
    this.intensivo = new PanelIntensivo({ acciones });

    bus.on('progreso', () => {
      if (!this.el.hidden && this.datos) this.pintar();
    });
  }

  async abrir({ tab = null } = {}) {
    if (TABS.includes(tab)) {
      if (this.st.tab !== tab) {
        this.st.filtroEx = '';
        this.st.filtroPilar = 0;
        this.st.filtroRadar = 'todos';
      }
      this.st.tab = tab;
      guardarEstado(this.st);
    }
    if (!this.datos) {
      this._pintarCargando();
      try {
        await this._cargar();
      } catch {
        this._pintarError();
        return;
      }
    }
    this.pintar();
  }

  /** Una sola carga de `/api/preparacion`, compartida entre esta vista y el Inicio. */
  _cargar() {
    if (this.datos) return Promise.resolve(this.datos);
    if (!this._promesa) {
      this.cargando = true;
      this._promesa = pedir('/api/preparacion', { tiempo: 20000 })
        .then((d) => { this.datos = d; return d; })
        .finally(() => { this.cargando = false; this._promesa = null; });
    }
    return this._promesa;
  }

  /* ---------------------------------------------------------------- para el Inicio y el cierre */

  /** Lo que muestra el Inicio «Hoy» para el plan elegido: qué toca, cuánto va hecho y el bloque. */
  async resumenParaInicio(plan) {
    try {
      await this._cargar();
    } catch {
      return { error: true };
    }
    const mod = plan === 'sprint6'
      ? this.datos.sprint_6_dias
      : plan === 'repo26'
        ? this.datos.reposicion_ii_2026
        : plan === 'reposicion'
          ? this.datos.reposicion
          : plan === 'radar'
            ? this.datos.radar_ii_2026
            : this.datos.extraordinario;
    if (!mod) return { error: true };
    const todos = this._todosItems(mod);
    const it = this._siguientePendiente(mod);
    return {
      it,
      hechos: todos.filter((x) => estaHecha(x.id)).length,
      total: todos.length,
      bloque: it ? ((mod.bloques[it._bloqueIdx] || {}).titulo || '') : '',
    };
  }

  /**
   * El siguiente pendiente del plan después de `id`. Primero se termina el bloque actual
   * (antes el que quedó a medias), luego los bloques siguientes y por último los anteriores.
   * Devuelve null si `id` no está en ningún plan; `{ it: null }` si ya no quedan pendientes.
   */
  siguienteTras(id) {
    if (!this.datos) return null;
    const r26 = this.datos.reposicion_ii_2026;
    const s6 = this.datos.sprint_6_dias;
    const ordenMods = {
      sprint6: [s6, r26, this.datos.reposicion, this.datos.extraordinario],
      repo26: [r26, s6, this.datos.reposicion, this.datos.radar_ii_2026, this.datos.extraordinario],
      radar: [this.datos.radar_ii_2026, this.datos.extraordinario, this.datos.reposicion, r26],
      reposicion: [this.datos.reposicion, r26, this.datos.radar_ii_2026, this.datos.extraordinario],
      extraordinario: [this.datos.extraordinario, this.datos.radar_ii_2026, this.datos.reposicion, r26],
    };
    const mods = ordenMods[this.st.tab] || ordenMods.extraordinario;
    const pendiente = (x) => x.id !== id && !estaHecha(x.id);
    const aMedias = (x) => pendiente(x) && ((banco.progreso[x.id] || {}).paso || 0) > 0;
    for (const mod of mods) {
      if (!mod) continue;
      const todos = this._todosItems(mod);
      const i = todos.findIndex((x) => x.id === id);
      if (i < 0) continue;
      const blq = todos[i]._bloqueIdx;
      const delBloque = todos.filter((x) => x._bloqueIdx === blq);
      const j = delBloque.findIndex((x) => x.id === id);
      const ordenBloque = [...delBloque.slice(j + 1), ...delBloque.slice(0, j)];
      // De los que quedaron a medias, el que se tocó más recientemente
      const cuando = (x) => (banco.progreso[x.id] || {}).cuando || 0;
      const enCurso = ordenBloque.filter(aMedias).sort((a, b) => cuando(b) - cuando(a))[0];
      const enBloque = enCurso || ordenBloque.find(pendiente);
      if (enBloque) return { it: enBloque };
      const resto = [...todos.slice(i + 1), ...todos.slice(0, i)].filter((x) => x._bloqueIdx !== blq);
      return { it: resto.find(pendiente) || null };
    }
    return null;
  }

  /** Abre un ejercicio del plan con su contexto (corte, meta y «volver a la preparación»). */
  abrirItem(it, desde = null) {
    let d = desde;
    if (d === null) {
      const e = banco.progreso[it.id];
      d = e && !e.hecha ? Math.min(e.paso || 0, Math.max(0, (e.total || 1) - 1)) : 0;
    }
    this._abrirEjercicio(it, d);
  }

  _pintarCargando() {
    const wrap = document.createElement('div');
    wrap.className = 'prep-wrap';
    wrap.innerHTML = '<div class="prep-hero"><strong>Preparando tus bloques de enfoque…</strong><p>Cargando los 42 ejercicios del Extraordinario y los 22 de Reposición.</p></div>';
    this.el.replaceChildren(wrap);
  }

  _pintarError() {
    const wrap = document.createElement('div');
    wrap.className = 'prep-wrap';
    const box = document.createElement('div');
    box.className = 'prep-hero';
    const st = document.createElement('strong');
    st.textContent = 'No se pudo cargar la preparación';
    const p = document.createElement('p');
    p.textContent = 'Revisa que el motor local siga activo y vuelve a intentarlo.';
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn btn-principal';
    b.textContent = 'Reintentar';
    b.addEventListener('click', () => this.abrir());
    box.append(st, p, b);
    wrap.appendChild(box);
    this.el.replaceChildren(wrap);
  }

  /* ---------------------------------------------------------------- micro-sprint no punitivo */

  _fijarSprint(min) {
    this._detenerTimer();
    this.st.sprintMin = min;
    this.sprintRestante = min * 60;
    this.sprintActivo = false;
    this.sprintPausaAviso = false;
    guardarEstado(this.st);
    this.pintar();
  }

  _alternarSprint() {
    if (this.st.sprintMin === 0) return;
    if (this.sprintActivo) {
      this._detenerTimer();
      this.sprintActivo = false;
    } else {
      if (this.sprintRestante <= 0) this.sprintRestante = this.st.sprintMin * 60;
      this.sprintPausaAviso = false;
      this.sprintActivo = true;
      this._timer = setInterval(() => {
        this.sprintRestante = Math.max(0, this.sprintRestante - 1);
        const rel = this.el.querySelector('#prep-reloj');
        if (rel) rel.textContent = formatoReloj(this.sprintRestante);
        if (this.sprintRestante <= 0) {
          this._detenerTimer();
          this.sprintActivo = false;
          this.sprintPausaAviso = true;
          this.pintar();
        }
      }, 1000);
    }
    this.pintar();
  }

  _reiniciarSprint() {
    this._detenerTimer();
    this.sprintActivo = false;
    this.sprintPausaAviso = false;
    this.sprintRestante = this.st.sprintMin * 60;
    this.pintar();
  }

  _detenerTimer() {
    if (this._timer) {
      clearInterval(this._timer);
      this._timer = null;
    }
  }

  /* ---------------------------------------------------------------- helpers de datos */

  _moduloActivo() {
    if (this.st.tab === 'sprint6' && this.datos.sprint_6_dias) return this.datos.sprint_6_dias;
    if (this.st.tab === 'repo26' && this.datos.reposicion_ii_2026) return this.datos.reposicion_ii_2026;
    if (this.st.tab === 'reposicion') return this.datos.reposicion;
    if (this.st.tab === 'radar' && this.datos.radar_ii_2026) return this.datos.radar_ii_2026;
    return this.datos.extraordinario;
  }

  _todosItems(mod) {
    return (mod.bloques || []).flatMap((b, bi) => (b.items || []).map((it) => ({ ...it, _bloqueIdx: bi, _bloqueId: b.id, _bloqueTit: b.titulo })));
  }

  _siguientePendiente(mod) {
    const todos = this._todosItems(mod);
    // Primero: si la persona dejó uno a medias dentro de este módulo, retomarlo
    let enCurso = null;
    for (const it of todos) {
      const e = banco.progreso[it.id];
      if (e && !e.hecha && (e.paso || 0) > 0) {
        if (!enCurso || (e.cuando || 0) > (enCurso._cuando || 0)) {
          enCurso = { ...it, _enCurso: e, _cuando: e.cuando || 0 };
        }
      }
    }
    if (enCurso) return enCurso;
    return todos.find((it) => !estaHecha(it.id)) || null;
  }

  _abrirEjercicio(it, desde = 0) {
    this.acciones.abrir(it.id, {
      desde,
      corte: '20.5',
      meta: it.meta_1_linea,
      volverA: 'preparacion',
    });
  }

  /* ---------------------------------------------------------------- render */

  pintar() {
    if (!this.datos) return;
    const mod = this._moduloActivo();
    const esRepos = this.st.tab === 'reposicion';
    const esRadar = this.st.tab === 'radar';
    const esRepo26 = this.st.tab === 'repo26';
    const hoy = hoyISO();
    const todos = this._todosItems(mod);
    const hechos = todos.filter((x) => estaHecha(x.id)).length;
    const total = todos.length;
    const siguiente = this._siguientePendiente(mod);
    const bloqueActivoIdx = siguiente ? siguiente._bloqueIdx : 0;

    const wrap = document.createElement('div');
    wrap.className = 'prep-wrap';

    // 1. Barra superior única: selector segmentado (3 pestañas en una sola fila) + micro-sprint + volver
    const barra = document.createElement('div');
    barra.className = 'est-barra prep-barra';

    const seg = document.createElement('div');
    seg.className = 'seg est-modos';
    seg.setAttribute('role', 'tablist');
    seg.setAttribute('aria-label', 'Módulo de preparación');
    const pestanas = [];
    if (this.datos.sprint_6_dias) {
      pestanas.push(['sprint6', `Sprint 6 días (${this.datos.sprint_6_dias.total})`]);
    }
    if (this.datos.reposicion_ii_2026) {
      pestanas.push(['repo26', `Reposición II-2026 (${this.datos.reposicion_ii_2026.total})`]);
    }
    pestanas.push(['extraordinario', `Extraordinario (${this.datos.extraordinario.total_activos})`]);
    if (this.datos.radar_ii_2026) {
      pestanas.push(['radar', `Radar Cartago (${this.datos.radar_ii_2026.total})`]);
    }
    pestanas.push(['reposicion', `Reposición (${this.datos.reposicion.total})`]);

    for (const [clave, label] of pestanas) {
      const b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('role', 'tab');
      b.setAttribute('aria-selected', String(this.st.tab === clave));
      b.textContent = label;
      b.addEventListener('click', () => {
        if (this.st.tab === clave) return;
        this.st.tab = clave;
        this.st.filtroEx = '';
        this.st.filtroPilar = 0;
        this.st.filtroRadar = 'todos';
        guardarEstado(this.st);
        if (ajustes.plan !== clave) guardarAjuste('plan', clave);
        this.pintar();
        const sel = this.el.querySelector('[role="tab"][aria-selected="true"]');
        if (sel && sel.scrollIntoView) sel.scrollIntoView({ block: 'nearest', inline: 'nearest' });
      });
      seg.appendChild(b);
    }

    const sprint = document.createElement('div');
    sprint.className = 'prep-sprint';
    sprint.setAttribute('aria-label', 'Temporizador de micro-sprint');
    const spSeg = document.createElement('div');
    spSeg.className = 'seg prep-sprint-seg';
    for (const [m, txt] of [[10, '10 min'], [15, '15 min'], [0, 'Sin reloj']]) {
      const bm = document.createElement('button');
      bm.type = 'button';
      bm.setAttribute('aria-pressed', String(this.st.sprintMin === m));
      bm.textContent = txt;
      bm.addEventListener('click', () => this._fijarSprint(m));
      spSeg.appendChild(bm);
    }
    sprint.appendChild(spSeg);

    if (this.st.sprintMin > 0) {
      const rel = document.createElement('span');
      rel.id = 'prep-reloj';
      rel.className = `prep-reloj${this.sprintActivo ? ' activo' : ''}`;
      rel.textContent = formatoReloj(this.sprintRestante);

      const bPlay = document.createElement('button');
      bPlay.type = 'button';
      bPlay.className = 'btn btn-suave prep-sprint-btn';
      bPlay.innerHTML = `${svg(this.sprintActivo ? ICO.pausa : ICO.play)}<span>${this.sprintActivo ? 'Pausar' : 'Iniciar'}</span>`;
      bPlay.addEventListener('click', () => this._alternarSprint());

      const bReset = document.createElement('button');
      bReset.type = 'button';
      bReset.className = 'btn btn-suave prep-sprint-ico';
      bReset.title = 'Reiniciar bloque de tiempo';
      bReset.setAttribute('aria-label', 'Reiniciar temporizador');
      bReset.innerHTML = svg(ICO.reiniciar);
      bReset.addEventListener('click', () => this._reiniciarSprint());

      sprint.append(rel, bPlay, bReset);
    }

    const fin = document.createElement('div');
    fin.className = 'est-barra-fin';
    const bVol = document.createElement('button');
    bVol.type = 'button';
    bVol.className = 'btn';
    bVol.textContent = 'Volver';
    bVol.addEventListener('click', () => this.acciones.inicio());
    fin.append(sprint, bVol);

    barra.append(seg, fin);
    wrap.appendChild(barra);

    // Aviso sereno de descanso al terminar un micro-sprint
    if (this.sprintPausaAviso) {
      const av = document.createElement('div');
      av.className = 'prep-pausa-banner';
      av.setAttribute('role', 'status');
      const txt = document.createElement('div');
      txt.innerHTML = '<strong>Bloque de enfoque terminado</strong><span>Levántate dos minutos, toma agua y descansa la vista. No hay penalización: sigues exactamente donde quedaste.</span>';
      const bOk = document.createElement('button');
      bOk.type = 'button';
      bOk.className = 'btn btn-principal';
      bOk.textContent = 'Listo para otro bloque';
      bOk.addEventListener('click', () => this._reiniciarSprint());
      av.append(txt, bOk);
      wrap.appendChild(av);
    }

    // 2. Tarjeta única protagonista: "Tu siguiente paso ahora"
    wrap.appendChild(this._construirHero(mod, siguiente, hechos, total));

    // 2s. Sprint 6 días: Diagnóstico 1-pregunta-a-la-vez + Minipruebas A y B (30 pts, rúbrica 2+4+2)
    if (this.st.tab === 'sprint6') {
      wrap.appendChild(this.intensivo.el);
      this.intensivo.cargarYPintar();
    }

    // 2a. Reposición II-2026: qué viene en el examen y las recetas fijas (plegados)
    if (esRepo26 && mod.prediccion) {
      wrap.appendChild(this._construirPrediccion(mod.prediccion));
      wrap.appendChild(this._construirRecetas(mod.prediccion));
    }

    // 2b. Protocolo anti-bloqueo de 30 s (solo en Radar Cartago Extremo, colapsado por defecto)
    if (esRadar && Array.isArray(mod.protocolo_30s) && mod.protocolo_30s.length) {
      wrap.appendChild(this._construirProtocolo30s(mod));
    }

    // 3. Barra serena de progreso del bloque actual + total
    const blkAct = mod.bloques[bloqueActivoIdx];
    const blkHechos = blkAct ? blkAct.items.filter((x) => estaHecha(x.id)).length : 0;
    const blkTotal = blkAct ? blkAct.items.length : 0;
    const prog = document.createElement('div');
    prog.className = 'prep-progreso';
    const pct = total ? Math.round((hechos / total) * 100) : 0;
    const pTop = document.createElement('div');
    pTop.className = 'prep-progreso-top';
    const pIzq = document.createElement('span');
    pIzq.textContent = blkAct
      ? `${blkAct.titulo} · ${blkHechos} de ${blkTotal} hechos`
      : 'Todos los bloques completados';
    const pDer = document.createElement('strong');
    pDer.textContent = `${hechos} de ${total} completados`;
    pTop.append(pIzq, pDer);
    const pista = document.createElement('div');
    pista.className = 'prep-progreso-pista';
    const barraVal = document.createElement('div');
    barraVal.className = 'prep-progreso-val';
    barraVal.style.width = `${pct}%`;
    pista.appendChild(barraVal);
    prog.append(pTop, pista);
    wrap.appendChild(prog);

    // 4. Filtros compactos (por pilar en Reposición, por examen en ambos, solo pendientes)
    const fil = document.createElement('div');
    fil.className = 'filtros prep-filtros';

    if (esRepos || esRepo26) {
      const nP = (p) => todos.filter((x) => x.parcial === p).length;
      const pilares = esRepo26
        ? [
          [0, `Todos (${mod.total})`],
          [1, `Parcial 1 · MAS y ondas (${nP(1)})`],
          [2, `Parcial 2 · Fluidos (${nP(2)})`],
          [3, `Parcial 3 · Termo (${nP(3)})`],
        ]
        : [
          [0, `Todos (${mod.total})`],
          [1, 'Parcial 1 · Ondas (7)'],
          [2, 'Parcial 2 · Fluidos (7)'],
          [3, 'Parcial 3 · Termo ≤ 20.5 (8)'],
        ];
      for (const [pnum, lbl] of pilares) {
        const bp = document.createElement('button');
        bp.type = 'button';
        bp.className = 'chip';
        bp.setAttribute('aria-pressed', String(this.st.filtroPilar === pnum));
        bp.textContent = lbl;
        bp.addEventListener('click', () => {
          this.st.filtroPilar = pnum;
          guardarEstado(this.st);
          this.pintar();
        });
        fil.appendChild(bp);
      }
    } else if (esRadar) {
      const cats = [
        ['todos', `Todo (${mod.total})`],
        ['hoy', `Ordinario de hoy (${mod.total_hoy || 9})`],
        ['extremo', `Cartago Extremo (${mod.total_extremo || 23})`],
      ];
      for (const [cat, lbl] of cats) {
        const bc = document.createElement('button');
        bc.type = 'button';
        bc.className = 'chip';
        bc.setAttribute('aria-pressed', String(this.st.filtroRadar === cat));
        bc.textContent = lbl;
        bc.addEventListener('click', () => {
          this.st.filtroRadar = cat;
          guardarEstado(this.st);
          this.pintar();
        });
        fil.appendChild(bc);
      }
    }

    const selEx = document.createElement('select');
    selEx.className = 'prep-select';
    selEx.setAttribute('aria-label', 'Filtrar por examen');
    const optTodos = document.createElement('option');
    optTodos.value = '';
    optTodos.textContent = 'Todos los exámenes';
    selEx.appendChild(optTodos);
    for (const ex of mod.examenes || []) {
      const o = document.createElement('option');
      o.value = ex;
      o.textContent = ex;
      selEx.appendChild(o);
    }
    selEx.value = this.st.filtroEx;
    selEx.addEventListener('change', () => {
      this.st.filtroEx = selEx.value;
      guardarEstado(this.st);
      this.pintar();
    });
    fil.appendChild(selEx);

    const bPend = document.createElement('button');
    bPend.type = 'button';
    bPend.className = 'chip';
    bPend.setAttribute('aria-pressed', String(this.st.soloPend));
    bPend.textContent = 'Solo pendientes';
    bPend.addEventListener('click', () => {
      this.st.soloPend = !this.st.soloPend;
      guardarEstado(this.st);
      this.pintar();
    });
    fil.appendChild(bPend);

    wrap.appendChild(fil);

    // 5. Micro-bloques en acordeones <details> no exclusivos (solo bloque activo abierto por defecto)
    const secBloques = document.createElement('div');
    secBloques.id = 'prep-bloques';
    secBloques.className = 'prep-bloques';
    const hayFiltro = Boolean(this.st.filtroEx || this.st.filtroPilar || (esRadar && this.st.filtroRadar !== 'todos') || this.st.soloPend);

    let bloquesVisibles = 0;
    mod.bloques.forEach((blk, idx) => {
      if (esRepos && this.st.filtroPilar && blk.parcial !== this.st.filtroPilar) return;
      if (esRadar && this.st.filtroRadar !== 'todos' && blk.categoria !== this.st.filtroRadar) return;
      const items = blk.items.filter((it) => {
        if (this.st.filtroEx && it.examen !== this.st.filtroEx) return false;
        if (esRepo26 && this.st.filtroPilar && it.parcial !== this.st.filtroPilar) return false;
        if (this.st.soloPend && estaHecha(it.id)) return false;
        return true;
      });
      const pendientes = esRepo26 && !hayFiltro ? (blk.pendientes || []) : [];
      if (!items.length && !pendientes.length) return;
      bloquesVisibles += 1;
      const esHoy = esRepo26 && blk.fecha === hoy;

      const det = document.createElement('details');
      det.className = 'est-det prep-bloque';
      if (hayFiltro || idx === bloqueActivoIdx || esHoy) det.open = true;

      const sum = document.createElement('summary');
      sum.className = 'prep-bloque-sum';
      const nHech = blk.items.filter((x) => estaHecha(x.id)).length;
      const sumTxt = document.createElement('span');
      sumTxt.className = 'prep-bloque-tit';
      sumTxt.textContent = blk.titulo;
      const sumMeta = document.createElement('span');
      sumMeta.className = 'prep-bloque-meta';
      sumMeta.textContent = blk.simulacro
        ? `${nHech}/${blk.items.length} · 2 h 30 min`
        : `${nHech}/${blk.items.length} · ~${blk.minutos} min`;
      sum.append(sumTxt);
      if (esHoy || blk.simulacro) {
        const bDia = document.createElement('span');
        bDia.className = `prep-badge ${esHoy ? 'prep-hoy' : 'prep-ext'}`;
        bDia.textContent = esHoy ? 'Hoy' : 'Simulacro';
        sum.appendChild(bDia);
      }
      sum.appendChild(sumMeta);
      det.appendChild(sum);

      const cuerpo = document.createElement('div');
      cuerpo.className = 'est-det-cuerpo prep-bloque-cuerpo';
      const sub = document.createElement('p');
      sub.className = 'prep-bloque-sub';
      sub.textContent = blk.subtitulo;
      cuerpo.appendChild(sub);

      const ul = document.createElement('ul');
      ul.className = 'prep-lista';
      for (const it of items) {
        const li = document.createElement('li');
        const hecha = estaHecha(it.id);
        const esSig = siguiente && siguiente.id === it.id;
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `prep-item${hecha ? ' hecha' : ''}${esSig ? ' sigue' : ''}`;
        btn.dataset.id = it.id;
        btn.addEventListener('click', () => {
          const e = banco.progreso[it.id];
          const desde = e && !e.hecha ? Math.min(e.paso || 0, Math.max(0, (e.total || 1) - 1)) : 0;
          this._abrirEjercicio(it, desde);
        });

        const est = document.createElement('span');
        est.className = 'prep-item-est';
        est.innerHTML = hecha ? svg(ICO.check) : '';
        if (!hecha) est.textContent = it.tipo === 'teoria' ? 'T' : `P${it.num}`;

        const mid = document.createElement('span');
        mid.className = 'prep-item-mid';
        const l1 = document.createElement('span');
        l1.className = 'prep-item-top';
        const tit = document.createElement('strong');
        ponerTextoMate(tit, it.titulo);
        const tags = document.createElement('span');
        tags.className = 'prep-item-tags';
        const bEx = document.createElement('span');
        bEx.className = 'prep-badge';
        bEx.textContent = `${it.examen} · ${it.id}`;
        tags.appendChild(bEx);
        if (esRepos || esRepo26) {
          const bP = document.createElement('span');
          bP.className = `prep-badge prep-p${it.parcial}`;
          bP.textContent = `Parcial ${it.parcial}`;
          tags.appendChild(bP);
        } else if (esRadar) {
          const bCat = document.createElement('span');
          bCat.className = `prep-badge ${blk.categoria === 'hoy' ? 'prep-hoy' : 'prep-ext'}`;
          bCat.textContent = blk.categoria === 'hoy' ? 'Hoy II-2026' : 'Cartago Extremo';
          tags.appendChild(bCat);
        }
        const bTipo = document.createElement('span');
        bTipo.className = 'prep-badge';
        bTipo.textContent = tipoTexto(it.tipo);
        tags.appendChild(bTipo);
        if (it.inciso_omitido) {
          const bC = document.createElement('span');
          bC.className = 'prep-badge prep-corte';
          bC.textContent = 'Corte ≤ 20.5';
          tags.appendChild(bC);
        }
        l1.append(tit, tags);

        const meta = document.createElement('span');
        meta.className = 'prep-item-meta';
        ponerTextoMate(meta, it.meta_1_linea);
        mid.append(l1, meta);

        const der = document.createElement('span');
        der.className = 'prep-item-min';
        der.textContent = `${it.minutos} min`;

        btn.append(est, mid, der);
        li.appendChild(btn);
        ul.appendChild(li);
      }
      cuerpo.appendChild(ul);
      if (pendientes.length) {
        const pp = document.createElement('p');
        pp.className = 'prep-bloque-sub';
        pp.textContent = pendientes.length === 1
          ? 'Hay 1 ejercicio más de este día en preparación; aparece aquí solo cuando esté listo.'
          : `Hay ${pendientes.length} ejercicios más de este día en preparación; aparecen aquí solos cuando estén listos.`;
        cuerpo.appendChild(pp);
      }
      det.appendChild(cuerpo);
      secBloques.appendChild(det);
    });

    if (bloquesVisibles === 0) {
      const vac = document.createElement('p');
      vac.className = 'prep-vacio';
      vac.textContent = 'No quedan ejercicios con este filtro. Limpia el filtro para ver el resto.';
      secBloques.appendChild(vac);
    }
    wrap.appendChild(secBloques);

    // 6. Transparencia de corte ≤ 20.5 (y 19 casillas vírgenes en Radar; casillas nuevas en Reposición II-2026)
    wrap.appendChild(esRepo26 && mod.prediccion
      ? this._construirNuevas(mod.prediccion)
      : this._construirTransparencia(mod, esRepos, esRadar));

    this.el.replaceChildren(wrap);
  }

  _construirProtocolo30s(mod) {
    const det = document.createElement('details');
    det.className = 'est-det prep-excl prep-proto';
    const sum = document.createElement('summary');
    sum.textContent = 'Protocolo anti-bloqueo de 30 s · Qué escribir primero en Cartago Extremo';
    det.appendChild(sum);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo prep-proto-grid';
    for (const f of mod.protocolo_30s) {
      const card = document.createElement('div');
      card.className = 'prep-proto-card';
      const cab = document.createElement('div');
      cab.className = 'prep-proto-cab';
      const tit = document.createElement('strong');
      tit.textContent = f.familia;
      const cels = document.createElement('span');
      cels.className = 'prep-proto-cels';
      cels.textContent = (f.casillas || []).join(' · ');
      cab.append(tit, cels);
      const ul = document.createElement('ul');
      ul.className = 'prep-proto-pasos';
      for (const paso of f.pasos || []) {
        const li = document.createElement('li');
        ponerTextoMate(li, paso);
        ul.appendChild(li);
      }
      card.append(cab, ul);
      cuerpo.appendChild(card);
    }
    det.appendChild(cuerpo);
    return det;
  }

  _construirHero(mod, it, hechos, total) {
    const hero = document.createElement('section');
    hero.className = 'prep-hero';
    hero.setAttribute('aria-label', 'Tu siguiente paso ahora');

    if (!it) {
      const h = document.createElement('h2');
      h.textContent = 'Completaste todos los ejercicios de este módulo';
      const p = document.createElement('p');
      p.textContent = `Llevas ${hechos} de ${total} resueltos. Puedes repasar cualquier bloque abajo o pasar al otro módulo.`;
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn btn-principal';
      const destino = this.st.tab === 'repo26' ? 'reposicion' : this.st.tab === 'extraordinario' ? 'reposicion' : 'extraordinario';
      b.textContent = this.st.tab === 'repo26'
        ? 'Repasar las reposiciones de semestres anteriores'
        : this.st.tab === 'extraordinario' ? 'Ir a Reposición (3 Parciales)' : 'Ir a Extraordinario ≤ 20.5';
      b.addEventListener('click', () => {
        this.st.tab = destino;
        guardarEstado(this.st);
        this.pintar();
      });
      hero.append(h, p, b);
      return hero;
    }

    const eye = document.createElement('div');
    eye.className = 'prep-hero-eye';
    const lbl = document.createElement('span');
    lbl.className = 'prep-eye-lbl';
    lbl.textContent = it._enCurso ? 'Retoma donde quedaste' : 'Tu siguiente paso ahora';
    const bEx = document.createElement('span');
    bEx.className = 'prep-badge';
    bEx.textContent = it.tipo === 'teoria' ? `Teoría · ${it.id}` : `${it.examen} · P${it.num} · ${it.id}`;
    const bMin = document.createElement('span');
    bMin.className = 'prep-badge';
    bMin.textContent = `~${it.minutos} min · ${tipoTexto(it.tipo)}`;
    eye.append(lbl, bEx, bMin);
    if (it.parcial !== 3) {
      const bP = document.createElement('span');
      bP.className = `prep-badge prep-p${it.parcial}`;
      bP.textContent = `Hoja Parcial ${it.parcial}`;
      eye.appendChild(bP);
    }
    if (it.inciso_omitido) {
      const bC = document.createElement('span');
      bC.className = 'prep-badge prep-corte';
      bC.textContent = it.inciso_omitido;
      eye.appendChild(bC);
    }

    const tit = document.createElement('h2');
    tit.className = 'prep-hero-tit';
    ponerTextoMate(tit, it.titulo);

    const meta = document.createElement('p');
    meta.className = 'prep-hero-meta';
    ponerTextoMate(meta, it.meta_1_linea);

    hero.append(eye, tit, meta);

    // Casillas clave de la hoja oficial
    if (Array.isArray(it.celdas_clave) && it.celdas_clave.length) {
      const cels = document.createElement('div');
      cels.className = 'prep-celdas';
      for (const c of it.celdas_clave) {
        const sp = document.createElement('span');
        sp.className = `prep-celda form-c-${c.color}`;
        sp.textContent = `${c.id} · ${c.nombre}`;
        cels.appendChild(sp);
      }
      hero.appendChild(cels);
    }

    // Calentamiento activo de 15 s
    if (it.calentamiento && Array.isArray(it.calentamiento.opciones) && it.calentamiento.opciones.length) {
      const calBox = document.createElement('div');
      calBox.className = 'prep-cal';
      const calTop = document.createElement('div');
      calTop.className = 'prep-cal-top';
      const calTit = document.createElement('strong');
      ponerTextoMate(calTit, `Calentamiento de 15 s · ${it.calentamiento.pregunta}`);
      calTop.appendChild(calTit);
      calBox.appendChild(calTop);

      const ops = ordenarOpciones(it.id, it.calentamiento);
      const prev = this.calElegido[it.id] || null;
      const listaOps = document.createElement('div');
      listaOps.className = 'prep-cal-ops';
      ops.forEach((op, idx) => {
        const bo = document.createElement('button');
        bo.type = 'button';
        bo.className = 'prep-cal-opt';
        if (prev) {
          if (op.esCorrecta) bo.classList.add('bien');
          else if (prev.idx === idx && !op.esCorrecta) bo.classList.add('mal');
        }
        ponerTextoMate(bo, op.texto);
        bo.addEventListener('click', () => {
          this.calElegido[it.id] = { idx, ok: op.esCorrecta };
          if (op.esCorrecta) {
            this.st.calOk[it.id] = true;
            guardarEstado(this.st);
          }
          this.pintar();
        });
        listaOps.appendChild(bo);
      });
      calBox.appendChild(listaOps);

      if (prev) {
        const fb = document.createElement('p');
        fb.className = `prep-cal-fb${prev.ok ? ' bien' : ' mal'}`;
        ponerTextoMate(fb, `${prev.ok ? 'Exacto.' : 'Casi — revisa la opción marcada en verde.'} ${it.calentamiento.explica}`);
        calBox.appendChild(fb);
      }
      hero.appendChild(calBox);
    }

    // Botones principales: cero fricción
    const acc = document.createElement('div');
    acc.className = 'prep-hero-acc';
    const bStart = document.createElement('button');
    bStart.type = 'button';
    bStart.id = 'prep-btn-empezar';
    bStart.className = 'btn btn-principal prep-btn-big';
    bStart.innerHTML = `${svg(ICO.play)}<span>${it._enCurso ? `Seguir desde el paso ${(it._enCurso.paso || 0) + 1}` : 'Empezar ahora en la pizarra'}</span>`;
    bStart.addEventListener('click', () => {
      const desde = it._enCurso ? Math.min(it._enCurso.paso || 0, Math.max(0, (it._enCurso.total || 1) - 1)) : 0;
      this._abrirEjercicio(it, desde);
    });

    const bVerBloques = document.createElement('button');
    bVerBloques.type = 'button';
    bVerBloques.className = 'btn btn-suave';
    bVerBloques.textContent = 'Elegir otro ejercicio';
    bVerBloques.addEventListener('click', () => {
      const target = this.el.querySelector('#prep-bloques');
      if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' });
    });

    acc.append(bStart, bVerBloques);
    hero.appendChild(acc);
    return hero;
  }

  /** «Qué viene en el examen»: formato probable, tipos que más se repiten y estado de la hoja. */
  _construirPrediccion(pred) {
    const det = document.createElement('details');
    det.className = 'est-det prep-excl';
    const f = pred.formato || {};
    const sum = document.createElement('summary');
    sum.textContent = `Qué viene en el examen · ${f.puntos || 52} puntos en 2 h 30 min · 6 selecciones y 3 desarrollos`;
    det.appendChild(sum);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo';
    const p0 = document.createElement('p');
    p0.className = 'prep-excl-sub';
    p0.textContent = f.titulo || '';
    cuerpo.appendChild(p0);
    const ul = document.createElement('ul');
    ul.className = 'prep-excl-lista';
    for (const parte of f.partes || []) {
      const li = document.createElement('li');
      const st = document.createElement('strong');
      st.textContent = `${parte.nombre} (${parte.puntos} pts)`;
      const sp = document.createElement('span');
      ponerTextoMate(sp, ` — ${parte.detalle}`);
      li.append(st, sp);
      ul.appendChild(li);
    }
    if (f.otra_posibilidad) {
      const li = document.createElement('li');
      ponerTextoMate(li, `Otra posibilidad: ${f.otra_posibilidad}`);
      ul.appendChild(li);
    }
    cuerpo.appendChild(ul);

    const p1 = document.createElement('p');
    p1.className = 'prep-excl-sub';
    const nRep = ((pred.arquetipos || [])[0] || {}).de || 5;
    p1.textContent = `Cuántas de las ${nRep} reposiciones con preguntas trajeron cada tipo de problema:`;
    cuerpo.appendChild(p1);
    const ul2 = document.createElement('ul');
    ul2.className = 'prep-excl-lista';
    for (const a of pred.arquetipos || []) {
      const li = document.createElement('li');
      const st = document.createElement('strong');
      st.textContent = `${a.veces} de ${a.de}`;
      const sp = document.createElement('span');
      ponerTextoMate(sp, ` · Parcial ${a.parcial} · ${a.nombre}`);
      li.append(st, sp);
      ul2.appendChild(li);
    }
    cuerpo.appendChild(ul2);

    if (pred.hoja) {
      const p2 = document.createElement('p');
      p2.className = 'prep-excl-sub';
      p2.textContent = `Formulario: ${pred.hoja.estado} ${pred.hoja.prediccion}`;
      cuerpo.appendChild(p2);
    }
    const ul3 = document.createElement('ul');
    ul3.className = 'prep-excl-lista';
    for (const r of pred.reglas || []) {
      const li = document.createElement('li');
      ponerTextoMate(li, r);
      ul3.appendChild(li);
    }
    cuerpo.appendChild(ul3);
    det.appendChild(cuerpo);
    return det;
  }

  /** Recetas fijas por tipo de problema: qué escribir primero, renglón por renglón. */
  _construirRecetas(pred) {
    const det = document.createElement('details');
    det.className = 'est-det prep-excl prep-proto';
    const sum = document.createElement('summary');
    sum.textContent = 'Recetas: qué escribir primero en cada tipo de problema';
    det.appendChild(sum);
    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo prep-proto-grid';
    for (const r of pred.recetas || []) {
      const card = document.createElement('div');
      card.className = 'prep-proto-card';
      const cab = document.createElement('div');
      cab.className = 'prep-proto-cab';
      const tit = document.createElement('strong');
      tit.textContent = r.titulo;
      const cels = document.createElement('span');
      cels.className = 'prep-proto-cels';
      cels.textContent = (r.casillas || []).join(' · ');
      cab.append(tit, cels);
      const ol = document.createElement('ol');
      ol.className = 'prep-proto-pasos';
      for (const paso of r.pasos || []) {
        const li = document.createElement('li');
        ponerTextoMate(li, paso);
        ol.appendChild(li);
      }
      card.append(cab, ol);
      cuerpo.appendChild(card);
    }
    det.appendChild(cuerpo);
    return det;
  }

  /** Casillas nuevas del II-2026 y lo que quedó fuera de la hoja. */
  _construirNuevas(pred) {
    const det = document.createElement('details');
    det.className = 'est-det prep-excl';
    const sum = document.createElement('summary');
    const n = (pred.casillas_nuevas || []).length;
    sum.textContent = `Casillas nuevas del II-2026 (${n}) y lo que ya no entra`;
    det.appendChild(sum);
    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo';
    const p0 = document.createElement('p');
    p0.className = 'prep-excl-sub';
    p0.textContent = 'Nunca estuvieron en una hoja de reposición. Son candidatas fuertes para las selecciones:';
    cuerpo.appendChild(p0);
    const ul = document.createElement('ul');
    ul.className = 'prep-excl-lista';
    for (const c of [...(pred.casillas_nuevas || []), ...(pred.casillas_que_vuelven || [])]) {
      const li = document.createElement('li');
      const st = document.createElement('strong');
      st.textContent = `${c.id} · ${c.tex}`;
      const sp = document.createElement('span');
      ponerTextoMate(sp, ` — ${c.salio_en}`);
      li.append(st, sp);
      ul.appendChild(li);
    }
    cuerpo.appendChild(ul);
    const p1 = document.createElement('p');
    p1.className = 'prep-excl-sub';
    p1.textContent = 'Ya no está en la hoja, así que no se pregunta:';
    cuerpo.appendChild(p1);
    const ul2 = document.createElement('ul');
    ul2.className = 'prep-excl-lista';
    for (const t of pred.fuera || []) {
      const li = document.createElement('li');
      ponerTextoMate(li, t);
      ul2.appendChild(li);
    }
    cuerpo.appendChild(ul2);
    det.appendChild(cuerpo);
    return det;
  }

  _construirTransparencia(mod, esRepos, esRadar = false) {
    const det = document.createElement('details');
    det.className = 'est-det prep-excl';
    const sum = document.createElement('summary');
    const nExcl = (mod.excluidos_corte || []).length;
    const nMix = (mod.mixtos_recortados || []).length;
    const nVirg = (mod.casillas_virgenes || []).length;
    sum.textContent = esRadar
      ? `Radar de ${nVirg} ecuaciones vírgenes del Ordinario II-2026 + corte ≤ 20.5`
      : esRepos
        ? `Corte ≤ 20.5 en Reposición · ${nMix} inciso de entropía omitido automáticamente`
        : `Qué se ignoró por superar 20.5 (Carnot 20.6 y Entropía 20.7) · ${nExcl} excluidos + ${nMix} incisos omitidos`;
    det.appendChild(sum);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'est-det-cuerpo';

    if (esRadar && nVirg) {
      const pv = document.createElement('p');
      pv.className = 'prep-excl-sub';
      pv.textContent = 'Estas 19 ecuaciones del formulario II-2026 NO se usaron en los 9 ítems confirmados de hoy (prioridad máxima para el Extraordinario):';
      cuerpo.appendChild(pv);
      const grid = document.createElement('div');
      grid.className = 'prep-virgenes-grid';
      for (const c of mod.casillas_virgenes) {
        const sp = document.createElement('span');
        sp.className = 'prep-virgen-pill';
        sp.textContent = `${c.id} · ${c.nombre}`;
        grid.appendChild(sp);
      }
      cuerpo.appendChild(grid);
    }

    if (nExcl) {
      const p1 = document.createElement('p');
      p1.className = 'prep-excl-sub';
      p1.textContent = 'Problemas 100 % fuera del corte ≤ 20.5 (excluidos de la ruta):';
      cuerpo.appendChild(p1);
      const ul = document.createElement('ul');
      ul.className = 'prep-excl-lista';
      for (const ex of mod.excluidos_corte) {
        const li = document.createElement('li');
        li.innerHTML = `<strong>${ex.examen} P${ex.num} (${ex.id}) · ${ex.seccion}</strong> — ${ex.titulo}. <span>${ex.motivo}</span>`;
        ul.appendChild(li);
      }
      cuerpo.appendChild(ul);
    }

    if (nMix) {
      const p2 = document.createElement('p');
      p2.className = 'prep-excl-sub';
      p2.textContent = 'Problemas mixtos incluidos al 100 % hasta 20.5 (solo se oculta el inciso que supera 20.5):';
      cuerpo.appendChild(p2);
      const ul2 = document.createElement('ul');
      ul2.className = 'prep-excl-lista';
      for (const mx of mod.mixtos_recortados) {
        const li = document.createElement('li');
        li.innerHTML = `<strong>${mx.id}</strong> — ${mx.nota}`;
        ul2.appendChild(li);
      }
      cuerpo.appendChild(ul2);
    }

    det.appendChild(cuerpo);
    return det;
  }
}
