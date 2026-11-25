/**
 * El enunciado pegado en la pizarra: el recorte del documento original (o el texto, si el
 * documento no está a mano) y, en selección única, las opciones.
 */

import { TintaTexto } from '../mate/tinta.js';
import { bus, urlApi } from '../nucleo/api.js';

function norma(s) {
  return String(s || '').normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase()
    .replace(/[‐-―−]/g, '-').replace(/\s+/g, ' ').trim();
}

/** Busca una frase en las palabras del recorte y devuelve su caja normalizada. */
export function cajaDeFrase(frase, palabras) {
  if (!frase || !palabras || !palabras.length) return null;
  const limpia = (t) => norma(t).replace(/^[([{"'¿¡]+|[)\]}"'.,;:!?]+$/g, '');
  const q = norma(frase).split(' ').map(limpia).filter(Boolean);
  const ws = palabras.map((w) => limpia(w.t));
  for (let i = 0; i + q.length <= ws.length; i += 1) {
    let ok = true;
    for (let k = 0; k < q.length; k += 1) {
      const a = ws[i + k];
      if (a !== q[k] && !(q[k].length > 3 && a.startsWith(q[k]))) { ok = false; break; }
    }
    if (!ok) continue;
    const g = palabras.slice(i, i + q.length);
    // Solo se marca la parte que quede en un mismo renglón
    const y0 = g[0].y;
    const mismo = g.filter((w) => Math.abs(w.y - y0) < Math.max(0.012, g[0].h * 0.7));
    const x = Math.min(...mismo.map((w) => w.x));
    const y = Math.min(...mismo.map((w) => w.y));
    return { x, y, w: Math.max(...mismo.map((w) => w.x + w.w)) - x, h: Math.max(...mismo.map((w) => w.y + w.h)) - y };
  }
  return null;
}

export class Enunciado {
  constructor(tablero, lec) {
    this.tablero = tablero;
    this.lec = lec;
    this.paso = tablero.nuevoPaso({ n: 0, titulo: lec.recorte ? `${lec.recorte.etiqueta || 'Enunciado'}` : 'Enunciado', clase: 'enunciado' });
    this.paso.cab.classList.add('cab-enunciado');
    this.palabras = [];
    this.opciones = new Map();
    this.lista = Promise.resolve();
    if (lec.recorte && lec.recorte.url) this._recorte(lec.recorte);
    else this._texto();
    if (lec.aviso) {
      const p = document.createElement('p');
      p.className = 'aviso-fuente';
      p.textContent = lec.aviso;
      this.paso.cuerpo.appendChild(p);
    }
    if (lec.tipo === 'seleccion' && lec.opciones && Object.keys(lec.opciones).length) this._opciones();
  }

  _recorte(r) {
    const fig = document.createElement('figure');
    fig.className = 'recorte';
    const img = document.createElement('img');
    img.alt = this.lec.enunciado ? this.lec.enunciado.replace(/\$[^$]*\$/g, '') : 'Recorte del problema';
    img.decoding = 'async';
    img.draggable = false;
    this.lista = new Promise((res) => {
      const fin = () => { this.tablero.programarRecolocar(); res(); };
      img.addEventListener('load', fin, { once: true });
      img.addEventListener('error', () => { fig.remove(); this._texto(); fin(); }, { once: true });
      setTimeout(res, 2500);
    });
    img.src = urlApi(r.url);
    img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
    this.img = img;
    this.palabras = r.palabras || [];
    const pie = document.createElement('figcaption');
    pie.textContent = r.pie || '';
    fig.append(img, pie);
    this.paso.cuerpo.appendChild(fig);
  }

  _texto() {
    if (!this.lec.enunciado) return;
    const t = new TintaTexto(this.lec.enunciado, { clase: 'enunciado-texto', px: this.tablero.px * 0.6 });
    this.tablero.agregar(t, { paso: this.paso, id: 'enunciado' });
    t.progreso(1);
  }

  _opciones() {
    const ul = document.createElement('ul');
    ul.className = 'opciones';
    for (const [letra, txt] of Object.entries(this.lec.opciones)) {
      const li = document.createElement('li');
      li.className = 'opcion';
      li.dataset.letra = letra;
      const b = document.createElement('span');
      b.className = 'opcion-letra';
      b.textContent = letra;
      const c = document.createElement('span');
      c.className = 'opcion-txt';
      const t = new TintaTexto(txt, { etiqueta: 'span', px: this.tablero.px * 0.56 });
      t.progreso(1);
      c.appendChild(t.el);
      const m = document.createElement('span');
      m.className = 'opcion-motivo';
      m.hidden = true;
      c.appendChild(m);
      li.append(b, c);
      ul.appendChild(li);
      this.opciones.set(letra, { li, m });
    }
    this.paso.cuerpo.appendChild(ul);
  }

  opcion(letra, estado, motivo) {
    const o = this.opciones.get(letra);
    if (!o) return;
    o.li.classList.remove('correcta', 'descartada');
    if (estado) o.li.classList.add(estado);
    if (motivo) { o.m.textContent = motivo; o.m.hidden = false; }
    this.tablero.programarRecolocar();
  }

  reiniciarOpciones() {
    for (const { li, m } of this.opciones.values()) { li.classList.remove('correcta', 'descartada'); m.hidden = true; }
  }

  /** Marca de un dato sobre el recorte (o sobre el texto si no hay imagen). */
  marcarDato(frase, color) {
    const marcas = this.tablero.marcas;
    if (this.img && this.img.isConnected) {
      const cj = cajaDeFrase(frase, this.palabras);
      if (!cj) return null;
      return marcas.crear({
        tipo: 'subraya', que: frase, en: 'recorte', color, dy: 1,
        caja: () => {
          if (!this.img.isConnected || this.img.offsetParent === null) return null;
          const r = this.tablero.rectEnHoja(this.img);
          return { x: r.x + cj.x * r.w, y: r.y + cj.y * r.h, w: cj.w * r.w, h: cj.h * r.h };
        },
      });
    }
    const t = this.tablero.tinta('enunciado');
    if (!t) return null;
    const q = norma(frase).split(' ');
    const us = t.unidades;
    for (let i = 0; i + q.length <= us.length; i += 1) {
      let ok = true;
      for (let k = 0; k < q.length; k += 1) {
        const a = norma(us[i + k].el.textContent).replace(/[.,;:()]+$/g, '');
        if (a !== q[k].replace(/[.,;:()]+$/g, '')) { ok = false; break; }
      }
      if (!ok) continue;
      const grupo = us.slice(i, i + q.length).map((u) => u.el);
      return marcas.crear({
        tipo: 'subraya', que: frase, en: 'enunciado', color, dy: 0,
        caja: () => {
          const rs = grupo.map((e) => this.tablero.rectEnHoja(e));
          const y0 = rs[0].y;
          const m = rs.filter((r) => Math.abs(r.y - y0) < 6);
          const x = Math.min(...m.map((r) => r.x));
          return { x, y: y0, w: Math.max(...m.map((r) => r.x + r.w)) - x, h: Math.max(...m.map((r) => r.h)) };
        },
      });
    }
    return null;
  }
}
