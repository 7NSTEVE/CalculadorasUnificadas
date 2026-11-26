/** Simulacro contrarreloj II-2026: modo predictivo (formato I-2026) y práctica rápida. */

import { TintaTexto } from '../mate/tinta.js';
import { bus, pedir, urlApi } from '../nucleo/api.js';
import { banco, nombreTema } from './banco.js';
import { aviso } from './avisos.js';

// Preguntas de semestres previos fuera del corte oficial II-2026 (Carnot y Van der Waals; entropía se filtra por tema)
const FUERA_II_2026 = new Set(['Q071', 'Q072', 'Q073', 'Q118', 'Q125']);

// Plantilla ranura por ranura de los exámenes I-2026 (4 conceptuales × 2 pts + 3 cálculos × 3 pts = 17 pts)
const RANURAS_CONCEPTUALES = [
  ['PR2-S13', 'PR2-S3', 'Q003', 'Q004', 'Q005', 'Q007', 'Q008'],
  ['PR2-S10', 'Q001', 'Q002', 'Q024', 'Q025', 'Q026', 'Q027', 'Q028'],
  ['PR2-S6', 'PR2-S9', 'PR2-S11', 'PR2-S12', 'Q077', 'Q078', 'Q084', 'Q088', 'Q091'],
  ['PR2-S4', 'PR2-S8', 'Q119', 'Q121', 'Q123', 'Q124', 'Q126', 'Q062', 'Q065'],
];

const RANURAS_CALCULO = [
  ['PR2-S1', 'PR2-S5', 'Q017', 'Q018', 'Q019', 'Q020', 'Q030', 'Q039', 'Q066', 'Q068'],
  ['Q074', 'Q075', 'Q080', 'Q093', 'Q094', 'Q097', 'Q102'],
  ['PR2-S7', 'PR2-S14', 'PR2-S2', 'Q117', 'Q127', 'Q129'],
];

// Tres desarrollos sugeridos al terminar el simulacro predictivo (Partes III y IV = 33 pts)
const DESARROLLOS_PREDICTIVOS = [
  { etiqueta: 'Parte III · Calorimetría o fórmula nueva (8 pts)', ids: ['P26-1', 'P26-2', 'P26-3', 'PR1-2', 'PR2-D7', 'PR2-D12'] },
  { etiqueta: 'Parte III · Conducción con integral (9 pts)', ids: ['PR1-4', 'PR2-D2', 'PR2-D10', 'PR1-3', 'Q049', 'PR2-D1'] },
  { etiqueta: 'Parte IV · Ciclo integrador o Otto (16 pts)', ids: ['PR2-D9', 'PR1-5', 'PR1-6', 'PR2-D11', 'PR2-D4', 'PR2-D5', 'Q111'] },
];

export function esDelCorteII2026(lec) {
  if (!lec) return false;
  return lec.tema !== 'entropia' && !FUERA_II_2026.has(lec.id);
}

function texto(txt, opt = {}) {
  const t = new TintaTexto(txt, opt);
  t.progreso(1);
  return t.el;
}

function baraja(a) {
  const b = [...a];
  for (let i = b.length - 1; i > 0; i -= 1) {
    const j = Math.floor(Math.random() * (i + 1));
    [b[i], b[j]] = [b[j], b[i]];
  }
  return b;
}

function elegirDeRanura(candidatos, disponibles, usadas) {
  const pool = candidatos.filter((id) => disponibles.has(id) && !usadas.has(id));
  if (!pool.length) return null;
  // Prioridad leve a prácticas y preguntas nuevas del II-2026 (PR2-S*)
  const nuevas = pool.filter((id) => id.startsWith('PR2-'));
  const fuente = nuevas.length && Math.random() < 0.65 ? nuevas : pool;
  return fuente[Math.floor(Math.random() * fuente.length)];
}

export class Simulacro {
  constructor({ el, acciones }) {
    this.el = el;
    this.acciones = acciones;
    this.reloj = null;
    this.incluirHistorico = false;
  }

  abrir() {
    clearInterval(this.reloj);
    document.body.dataset.vista = 'inicio';
    const caja = document.createElement('div');
    caja.className = 'portada';

    const cab = document.createElement('div');
    const h = document.createElement('h1');
    h.textContent = 'Simulacro II-2026';
    const sub = document.createElement('p');
    sub.className = 'portada-sub';
    sub.textContent = 'Alineado al corte del III parcial: sin Carnot, entropía ni Van der Waals por defecto.';
    cab.append(h, sub);
    caja.appendChild(cab);

    const btnPred = document.createElement('button');
    btnPred.type = 'button';
    btnPred.className = 'accion-grande';
    btnPred.dataset.modo = 'predictivo';
    btnPred.innerHTML = '<span><strong>Simulacro predictivo II-2026</strong><small>Formato I-2026: 4 conceptuales (2 pts) + 3 cálculos (3 pts) = 17 pts y 3 desarrollos sugeridos</small></span><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M8 5v14l11-7L8 5Z"/></svg>';
    btnPred.addEventListener('click', () => this.iniciar('predictivo'));
    caja.appendChild(btnPred);

    const acc = document.createElement('div');
    acc.className = 'accesos';

    const btnRap = document.createElement('button');
    btnRap.type = 'button';
    btnRap.className = 'acceso';
    btnRap.dataset.modo = 'rapido';
    btnRap.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm1 5v4.6l3.5 2.1-1 1.7L11 12.8V7h2Z"/></svg><span><strong>Práctica rápida (8 preguntas)</strong><small>Selección variada del corte II-2026 por temas</small></span>';
    btnRap.addEventListener('click', () => this.iniciar('rapido', { n: 8 }));

    const btnVolver = document.createElement('button');
    btnVolver.type = 'button';
    btnVolver.className = 'acceso';
    btnVolver.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M15.4 7.4 14 6l-6 6 6 6 1.4-1.4L10.8 12l4.6-4.6Z"/></svg><span><strong>Volver al inicio</strong><small>Regresar a las rutas de estudio</small></span>';
    btnVolver.addEventListener('click', () => this.acciones.inicio());

    acc.append(btnRap, btnVolver);
    caja.appendChild(acc);

    const opt = document.createElement('label');
    opt.className = 'simulacro-opt';
    const chk = document.createElement('input');
    chk.type = 'checkbox';
    chk.checked = this.incluirHistorico;
    chk.addEventListener('change', () => { this.incluirHistorico = chk.checked; });
    const sp = document.createElement('span');
    sp.textContent = 'Incluir histórico completo (Carnot, entropía y Van der Waals)';
    opt.append(chk, sp);
    caja.appendChild(opt);

    this.el.replaceChildren(caja);
    this.el.scrollTop = 0;
  }

  async iniciar(modo = 'predictivo', { n = 8, incluirHistorico = this.incluirHistorico } = {}) {
    const todo = banco.lecciones.filter((l) => l.tipo === 'seleccion');
    const pool = incluirHistorico ? todo : todo.filter(esDelCorteII2026);
    if (pool.length < 3) {
      aviso('Todavía no hay suficientes preguntas de selección en el banco.');
      return;
    }

    let elegidas = [];
    if (modo === 'predictivo') {
      const disp = new Set(pool.map((l) => l.id));
      const usadas = new Set();
      for (const ranura of RANURAS_CONCEPTUALES) {
        const id = elegirDeRanura(ranura, disp, usadas);
        if (id) {
          usadas.add(id);
          elegidas.push({ lec: banco.mapa.get(id), parte: 'Parte I · Conceptual', pts: 2 });
        }
      }
      for (const ranura of RANURAS_CALCULO) {
        const id = elegirDeRanura(ranura, disp, usadas);
        if (id) {
          usadas.add(id);
          elegidas.push({ lec: banco.mapa.get(id), parte: 'Parte II · Cálculo', pts: 3 });
        }
      }
      // Respaldo si faltó alguna ranura
      if (elegidas.length < 7) {
        for (const l of baraja(pool)) {
          if (usadas.has(l.id)) continue;
          usadas.add(l.id);
          const esCalc = elegidas.length >= 4;
          elegidas.push({ lec: l, parte: esCalc ? 'Parte II · Cálculo' : 'Parte I · Conceptual', pts: esCalc ? 3 : 2 });
          if (elegidas.length >= 7) break;
        }
      }
    } else {
      const porTema = new Map();
      for (const l of baraja(pool)) {
        if (!porTema.has(l.tema)) porTema.set(l.tema, []);
        porTema.get(l.tema).push(l);
      }
      const listas = baraja([...porTema.values()]);
      while (elegidas.length < Math.min(n, pool.length)) {
        for (const g of listas) {
          const x = g.shift();
          if (x) elegidas.push({ lec: x, parte: '', pts: 1 });
          if (elegidas.length >= n) break;
        }
        if (listas.every((g) => !g.length)) break;
      }
    }

    const cargadas = (await Promise.all(
      elegidas.map(async (item) => {
        try {
          const d = await pedir(`/api/banco/${item.lec.id}`);
          return { ...item, lec: d };
        } catch {
          return null;
        }
      }),
    )).filter(Boolean);

    this._pintar(cargadas, { modo, incluirHistorico });
  }

  _pintar(items, { modo = 'rapido', incluirHistorico = false } = {}) {
    document.body.dataset.vista = 'inicio';
    const esPred = modo === 'predictivo';
    const totalPts = items.reduce((s, it) => s + it.pts, 0);

    const caja = document.createElement('div');
    caja.className = 'portada';
    caja.dataset.simulacroModo = modo;

    const h = document.createElement('h1');
    h.textContent = esPred ? 'Simulacro predictivo II-2026' : 'Simulacro rápido';
    const p = document.createElement('p');
    p.className = 'portada-sub';
    const reloj = document.createElement('strong');
    const desc = esPred
      ? `${items.length} preguntas (${totalPts} pts · formato I-2026${incluirHistorico ? ', con histórico' : ''}). Responde todas y corrige al final. Tiempo: `
      : `${items.length} preguntas (${incluirHistorico ? 'histórico completo' : 'corte II-2026'}). Responde todas y corrige al final. Tiempo: `;
    p.append(document.createTextNode(desc), reloj);
    caja.append(h, p);

    const resp = new Map();
    const bloques = [];
    items.forEach(({ lec, parte, pts }, i) => {
      const s = document.createElement('section');
      s.className = 'seccion';
      s.dataset.lecId = lec.id;
      const t = document.createElement('h2');
      const temaNom = nombreTema((banco.mapa.get(lec.id) || {}).tema || lec.tema);
      t.textContent = esPred ? `${i + 1}. ${parte} · ${temaNom}` : `${i + 1}. ${temaNom}`;
      if (esPred) {
        const bp = document.createElement('span');
        bp.className = 'simulacro-pts';
        bp.textContent = `${pts} pts`;
        t.append(' ', bp);
      }
      s.appendChild(t);
      if (lec.recorte && lec.recorte.url) {
        const img = document.createElement('img');
        img.src = urlApi(lec.recorte.url);
        img.alt = `Pregunta ${i + 1}`;
        img.style.maxWidth = '100%';
        img.style.background = '#fff';
        img.style.borderRadius = '8px';
        img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
        s.appendChild(img);
      }
      s.appendChild(texto(lec.enunciado, { px: 21 }));
      const ul = document.createElement('div');
      ul.className = 'opciones';
      for (const [L, v] of Object.entries(lec.opciones)) {
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'opcion';
        b.style.textAlign = 'left';
        b.style.width = '100%';
        b.setAttribute('aria-pressed', 'false');
        const sl = document.createElement('span');
        sl.className = 'opcion-letra';
        sl.textContent = L;
        const sc = document.createElement('span');
        sc.className = 'opcion-txt';
        sc.appendChild(texto(v, { etiqueta: 'span', px: 20 }));
        b.append(sl, sc);
        b.addEventListener('click', () => {
          resp.set(lec.id, L);
          ul.querySelectorAll('button').forEach((x) => { x.setAttribute('aria-pressed', 'false'); x.style.borderColor = ''; });
          b.setAttribute('aria-pressed', 'true');
          b.style.borderColor = 'var(--azul)';
        });
        ul.appendChild(b);
      }
      s.appendChild(ul);
      caja.appendChild(s);
      bloques.push({ lec, pts, s, ul });
    });

    const fin = document.createElement('div');
    fin.className = 'fila-botones';
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn btn-principal';
    b.textContent = 'Terminar y corregir';
    const nuevo = document.createElement('button');
    nuevo.type = 'button';
    nuevo.className = 'btn';
    nuevo.textContent = 'Cambiar formato';
    const salir = document.createElement('button');
    salir.type = 'button';
    salir.className = 'btn';
    salir.textContent = 'Salir';
    fin.append(b, nuevo, salir);

    const nota = document.createElement('p');
    nota.className = 'portada-sub';
    const desBox = document.createElement('div');
    caja.append(fin, nota, desBox);

    const t0 = Date.now();
    const pinta = () => {
      const seg = Math.round((Date.now() - t0) / 1000);
      reloj.textContent = `${Math.floor(seg / 60)}:${String(seg % 60).padStart(2, '0')}`;
    };
    pinta();
    clearInterval(this.reloj);
    this.reloj = setInterval(pinta, 1000);

    salir.addEventListener('click', () => { clearInterval(this.reloj); this.acciones.inicio(); });
    nuevo.addEventListener('click', () => this.abrir());
    b.addEventListener('click', () => {
      clearInterval(this.reloj);
      let bien = 0;
      let ptsGanados = 0;
      for (const { lec, pts, s, ul } of bloques) {
        const r = resp.get(lec.id);
        const ok = r === lec.correcta;
        if (ok) { bien += 1; ptsGanados += pts; }
        pedir('/api/progreso', { metodo: 'POST', cuerpo: { tipo: 'practica', id: lec.id, bien: ok } }).catch(() => {});
        [...ul.children].forEach((x) => {
          const L = x.querySelector('.opcion-letra').textContent;
          x.disabled = true;
          x.classList.toggle('correcta', L === lec.correcta);
          if (L === r && !ok) x.classList.add('descartada');
        });
        const v = document.createElement('button');
        v.type = 'button';
        v.className = `btn ${ok ? '' : 'btn-principal'}`;
        v.textContent = ok ? 'Ver la explicación' : 'Ver por qué, paso a paso';
        v.addEventListener('click', () => this.acciones.abrir(lec.id));
        s.appendChild(v);
      }
      nota.textContent = esPred
        ? `Acertaste ${bien} de ${bloques.length} (${ptsGanados} de ${totalPts} pts). ${bien === bloques.length ? 'Excelente selección.' : 'Abre cada fallo para verlo paso a paso.'}`
        : `Acertaste ${bien} de ${bloques.length}. ${bien === bloques.length ? 'Perfecto.' : 'Abre las que fallaste: cada una está explicada paso a paso.'}`;
      b.disabled = true;
      h.textContent = esPred
        ? `Simulacro predictivo: ${ptsGanados}/${totalPts} pts (${bien}/${bloques.length})`
        : `Simulacro: ${bien} de ${bloques.length}`;

      if (esPred) {
        this._pintarDesarrollosSugeridos(desBox);
      }
      this.el.scrollTo({ top: 0, behavior: 'smooth' });
    });
    this.el.replaceChildren(caja);
    this.el.scrollTop = 0;
  }

  _pintarDesarrollosSugeridos(contenedor) {
    const sec = document.createElement('section');
    sec.className = 'ruta';
    const h2 = document.createElement('h2');
    h2.textContent = 'Partes III y IV · 3 desarrollos sugeridos (33 pts · ~65 min)';
    const sub = document.createElement('p');
    sub.className = 'portada-sub';
    sub.style.fontSize = '1em';
    sub.style.margin = '0 0 0.7em';
    sub.textContent = 'Resuélvelos en papel con el formulario oficial y luego abre cada uno en la pizarra para verificar paso a paso.';
    const ol = document.createElement('ol');
    ol.className = 'ruta-lista';

    DESARROLLOS_PREDICTIVOS.forEach((grupo, idx) => {
      const disp = grupo.ids.filter((id) => banco.mapa.has(id));
      if (!disp.length) return;
      const id = disp[Math.floor(Math.random() * disp.length)];
      const lec = banco.mapa.get(id);
      const li = document.createElement('li');
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'ruta-item';
      btn.innerHTML = `<span class="num">${idx + 1}</span><span><strong></strong><small></small></span><span class="min"></span>`;
      btn.querySelector('strong').textContent = `${lec.id} · ${lec.titulo}`;
      btn.querySelector('small').textContent = grupo.etiqueta;
      btn.querySelector('.min').textContent = lec.min ? `${lec.min} min` : '';
      btn.addEventListener('click', () => this.acciones.abrir(lec.id));
      li.appendChild(btn);
      ol.appendChild(li);
    });

    sec.append(h2, sub, ol);
    contenedor.replaceChildren(sec);
  }
}
