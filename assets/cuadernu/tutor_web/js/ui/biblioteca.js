/** Banco de ejercicios: buscador instantáneo y filtros, agrupado por tema. */

import { TintaTexto } from '../mate/tinta.js';
import { bus } from '../nucleo/api.js';
import { banco, estaHecha, filtrar, nombreTema, TEMAS, textoFuente } from './banco.js';

export class Biblioteca {
  constructor(el, acciones) {
    this.el = el;
    this.acciones = acciones;
    this.f = { tema: '', tipo: '', fuente: '' };
    this.caja = document.createElement('div');
    this.caja.className = 'biblio';
    const bus1 = document.createElement('div');
    bus1.className = 'biblio-buscar';
    this.q = document.createElement('input');
    this.q.type = 'search';
    this.q.placeholder = 'Busca por tema, número o palabra: “refrigerador”, “Q049”, “hielo”…';
    this.q.setAttribute('aria-label', 'Buscar ejercicio');
    this.q.addEventListener('input', () => this._lista());
    this.q.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') { const r = filtrar(this.q.value, this.f); if (r.length) this.acciones.abrir(r[0].id); }
      if (e.key === 'Escape') this.acciones.inicio();
    });
    this.filtros = document.createElement('div');
    this.filtros.className = 'filtros';
    bus1.append(this.q, this.filtros);
    this.lista = document.createElement('div');
    this.caja.append(bus1, this.lista);
    el.replaceChildren(this.caja);
    bus.on('banco', () => this.pintar());
    this.pintar();
  }

  abrir() {
    this.pintar();
    requestAnimationFrame(() => this.q.focus());
  }

  _filtro(txt, clave, valor) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'filtro';
    b.textContent = txt;
    b.setAttribute('aria-pressed', String(this.f[clave] === valor));
    b.addEventListener('click', () => { this.f[clave] = this.f[clave] === valor ? '' : valor; this.pintar(); });
    return b;
  }

  pintar() {
    this.filtros.replaceChildren(
      this._filtro('Recomendados', 'fuente', 'E'),
      this._filtro('Superexamen', 'fuente', 'Q'),
      this._filtro('Me faltan', 'fuente', 'pend'),
      this._filtro('Selección', 'tipo', 'seleccion'),
      this._filtro('Desarrollo', 'tipo', 'desarrollo'),
    );
    const hay = new Set(banco.lecciones.map((l) => l.tema));
    for (const t of Object.keys(TEMAS)) if (hay.has(t)) this.filtros.appendChild(this._filtro(TEMAS[t], 'tema', t));
    this._lista();
  }

  _lista() {
    const r = filtrar(this.q.value, this.f);
    this.lista.replaceChildren();
    if (!r.length) {
      const p = document.createElement('p');
      p.className = 'vacio';
      p.textContent = banco.listo ? 'No hay ejercicios con esa búsqueda. Prueba con otra palabra o quita filtros.' : 'Cargando el banco…';
      this.lista.appendChild(p);
      return;
    }
    const grupos = new Map();
    for (const l of r) { if (!grupos.has(l.tema)) grupos.set(l.tema, []); grupos.get(l.tema).push(l); }
    const orden = Object.keys(TEMAS);
    for (const t of [...grupos.keys()].sort((a, b) => orden.indexOf(a) - orden.indexOf(b))) {
      const h = document.createElement('h2');
      h.className = 'grupo-tit';
      const ls = grupos.get(t);
      const listas = ls.filter((l) => estaHecha(l.id)).length;
      h.textContent = `${nombreTema(t)} · ${ls.length}${listas ? ` (${listas} vistos)` : ''}`;
      const ul = document.createElement('ul');
      ul.className = 'lista-ejs';
      for (const l of ls) {
        const li = document.createElement('li');
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'ej';
        b.dataset.id = l.id;
        const c = document.createElement('span');
        c.className = 'cod';
        c.textContent = /^[EQ]\d/.test(l.id) ? l.id : '·';
        const m = document.createElement('span');
        const s = document.createElement('strong');
        const tt = new TintaTexto(l.titulo, { etiqueta: 'span', px: 16, clase: 'sin-mano' });
        tt.progreso(1);
        s.appendChild(tt.el);
        const d = document.createElement('small');
        d.textContent = [l.tipo === 'seleccion' ? 'Selección' : l.tipo === 'teoria' ? 'Teoría' : 'Desarrollo', l.min ? `${l.min} min` : '', /^E\d/.test(l.id) ? '' : textoFuente(l.fuente)].filter(Boolean).join(' · ');
        m.append(s, d);
        const e = document.createElement('span');
        e.className = 'hecho';
        e.textContent = estaHecha(l.id) ? '✓' : '';
        if (estaHecha(l.id)) e.setAttribute('aria-label', 'Ya vista');
        b.append(c, m, e);
        b.addEventListener('click', () => this.acciones.abrir(l.id));
        li.appendChild(b);
        ul.appendChild(li);
      }
      this.lista.append(h, ul);
    }
  }
}
