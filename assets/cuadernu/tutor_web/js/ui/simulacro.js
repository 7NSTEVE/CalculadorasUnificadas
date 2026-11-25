/** Simulacro de selección única con reloj. Corrige al final y enlaza la explicación de cada fallo. */

import { TintaTexto } from '../mate/tinta.js';
import { bus, pedir, urlApi } from '../nucleo/api.js';
import { banco, nombreTema } from './banco.js';
import { aviso } from './avisos.js';

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

export class Simulacro {
  constructor({ el, acciones }) {
    this.el = el;
    this.acciones = acciones;
    this.reloj = null;
  }

  async abrir(n = 8) {
    const pool = banco.lecciones.filter((l) => l.tipo === 'seleccion');
    if (pool.length < 3) { aviso('Todavía no hay suficientes preguntas de selección en el banco.'); return; }
    // Variado: se toma de cada tema por turnos
    const porTema = new Map();
    for (const l of baraja(pool)) { if (!porTema.has(l.tema)) porTema.set(l.tema, []); porTema.get(l.tema).push(l); }
    const elegidas = [];
    const listas = baraja([...porTema.values()]);
    while (elegidas.length < Math.min(n, pool.length)) {
      for (const g of listas) { const x = g.shift(); if (x) elegidas.push(x); if (elegidas.length >= n) break; }
      if (listas.every((g) => !g.length)) break;
    }
    const lecs = (await Promise.all(elegidas.map((l) => pedir(`/api/banco/${l.id}`).catch(() => null)))).filter(Boolean);
    this._pintar(lecs);
  }

  _pintar(lecs) {
    document.body.dataset.vista = 'inicio';
    const caja = document.createElement('div');
    caja.className = 'portada';
    const h = document.createElement('h1');
    h.textContent = 'Simulacro';
    const p = document.createElement('p');
    p.className = 'portada-sub';
    const reloj = document.createElement('strong');
    p.append(document.createTextNode(`${lecs.length} preguntas. Responde todas y al final te corrijo. Tiempo: `), reloj);
    caja.append(h, p);
    const resp = new Map();
    const bloques = [];
    lecs.forEach((lec, i) => {
      const s = document.createElement('section');
      s.className = 'seccion';
      const t = document.createElement('h2');
      t.textContent = `${i + 1}. ${nombreTema((banco.mapa.get(lec.id) || {}).tema || lec.tema)}`;
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
      bloques.push({ lec, s, ul });
    });
    const fin = document.createElement('div');
    fin.className = 'fila-botones';
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn btn-principal';
    b.textContent = 'Terminar y corregir';
    const salir = document.createElement('button');
    salir.type = 'button';
    salir.className = 'btn';
    salir.textContent = 'Salir';
    fin.append(b, salir);
    const nota = document.createElement('p');
    nota.className = 'portada-sub';
    caja.append(fin, nota);
    const t0 = Date.now();
    const pinta = () => { const s = Math.round((Date.now() - t0) / 1000); reloj.textContent = `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`; };
    pinta();
    clearInterval(this.reloj);
    this.reloj = setInterval(pinta, 1000);
    salir.addEventListener('click', () => { clearInterval(this.reloj); this.acciones.inicio(); });
    b.addEventListener('click', () => {
      clearInterval(this.reloj);
      let bien = 0;
      for (const { lec, s, ul } of bloques) {
        const r = resp.get(lec.id);
        const ok = r === lec.correcta;
        if (ok) bien += 1;
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
      nota.textContent = `Acertaste ${bien} de ${bloques.length}. ${bien === bloques.length ? 'Perfecto.' : 'Abre las que fallaste: cada una está explicada paso a paso.'}`;
      b.disabled = true;
      this.el.scrollTo({ top: 0, behavior: 'smooth' });
      h.textContent = `Simulacro: ${bien} de ${bloques.length}`;
    });
    this.el.replaceChildren(caja);
    this.el.scrollTop = 0;
  }
}
