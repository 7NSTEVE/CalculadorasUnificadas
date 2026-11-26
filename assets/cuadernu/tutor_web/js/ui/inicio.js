/** Inicio: una sola acción grande y, debajo, la ruta. Nada de paneles ni estadísticas. */

import { bus } from '../nucleo/api.js';
import { aMedias, banco, estaHecha, nombreTema } from './banco.js';

const ICO = {
  seguir: 'M8 5v14l11-7L8 5Z',
  libro: 'M5 3h11a3 3 0 0 1 3 3v15H8a3 3 0 0 1-3-3V3Zm3 13a1 1 0 0 0 0 2h9v-2H8ZM8 6v2h8V6H8Zm0 4v2h6v-2H8Z',
  reloj: 'M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm1 5v4.6l3.5 2.1-1 1.7L11 12.8V7h2Z',
  hoja: 'M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Zm-1 7V3.5L18.5 9H13Z',
  idea: 'M12 2a7 7 0 0 0-4 12.7V17h8v-2.3A7 7 0 0 0 12 2ZM9 19h6v1.5a1.5 1.5 0 0 1-1.5 1.5h-3A1.5 1.5 0 0 1 9 20.5V19Z',
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

export class Inicio {
  constructor(el, acciones) {
    this.el = el;
    this.acciones = acciones;
    bus.on('banco', () => this.pintar());
    this.pintar();
  }

  pintar() {
    const { el, acciones } = this;
    const caja = document.createElement('div');
    caja.className = 'portada';

    const cab = document.createElement('div');
    const h = document.createElement('h1');
    h.textContent = 'Pizarra del profesor';
    const sub = document.createElement('p');
    sub.className = 'portada-sub';
    sub.textContent = banco.listo
      ? 'Un paso a la vez, con voz y dibujo. Puedes pausar, repetir o volver atrás cuando quieras.'
      : banco.viejo
        ? 'El motor que está abierto es de una versión anterior. Ciérralo y vuelve a abrir el tutor para usar el banco.'
        : 'Preparando el motor. Esto toma unos segundos.';
    cab.append(h, sub);
    caja.appendChild(cab);

    if (banco.listo) {
      const medias = aMedias();
      const ruta = banco.rutas[0];
      const prox = ruta ? ruta.items.find((x) => !estaHecha(x.id)) : null;
      if (medias) {
        caja.appendChild(boton('accion-grande',
          `<span><strong>Seguir donde quedaste</strong><small></small></span>${svg(ICO.seguir)}`,
          () => acciones.abrir(medias.id, { desde: Math.min(medias.e.paso || 0, (medias.e.total || 1) - 1) })));
        caja.lastElementChild.querySelector('small').textContent = `${medias.lec.titulo} · paso ${(medias.e.paso || 0) + 1} de ${medias.e.total}`;
      } else if (prox) {
        caja.appendChild(boton('accion-grande',
          `<span><strong></strong><small></small></span>${svg(ICO.seguir)}`,
          () => acciones.abrir(prox.id)));
        caja.lastElementChild.querySelector('strong').textContent = ruta.items.some((x) => estaHecha(x.id)) ? `Seguir: ${ruta.titulo}` : `Empezar: ${ruta.titulo}`;
        caja.lastElementChild.querySelector('small').textContent = `Sigue ${banco.mapa.get(prox.id) ? banco.mapa.get(prox.id).titulo : prox.id}`;
      }

      const acc = document.createElement('div');
      acc.className = 'accesos';
      const nEj = banco.lecciones.filter((l) => l.tipo !== 'teoria').length;
      acc.append(
        boton('acceso', `${svg(ICO.libro)}<span><strong>Banco de ejercicios</strong><small>${nEj} resueltos paso a paso</small></span>`, () => acciones.biblioteca()),
        boton('acceso', `${svg(ICO.hoja)}<span><strong>Abrir PDF o imagen</strong><small>También puedes pegar con Ctrl+V</small></span>`, () => acciones.adjuntar()),
        boton('acceso', `${svg(ICO.reloj)}<span><strong>Simulacro II-2026</strong><small>Predictivo (17 pts) o rápido</small></span>`, () => acciones.simulacro()),
        boton('acceso', `${svg(ICO.idea)}<span><strong>Preguntar lo que sea</strong><small>Cualquier materia</small></span>`, () => acciones.preguntar()),
      );
      caja.appendChild(acc);

      for (const r of banco.rutas) caja.appendChild(this._ruta(r));
    }
    el.replaceChildren(caja);
  }

  _ruta(r) {
    const s = document.createElement('section');
    s.className = 'ruta';
    const h = document.createElement('h2');
    const hechas = r.items.filter((x) => estaHecha(x.id)).length;
    h.textContent = r.titulo;
    const p = document.createElement('p');
    p.className = 'portada-sub';
    p.style.fontSize = '1em';
    p.style.margin = '0 0 0.7em';
    p.textContent = `${r.descripcion} ${hechas ? `Llevas ${hechas} de ${r.items.length}.` : ''}`.trim();
    s.append(h, p);
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
      b.querySelector('strong').textContent = l.titulo;
      b.querySelector('small').textContent = it.nota || `${nombreTema(l.tema)} · ${l.id}`;
      b.querySelector('.min').textContent = l.min ? `${l.min} min` : '';
      li.appendChild(b);
      ol.appendChild(li);
    });
    s.appendChild(ol);
    return s;
  }
}
