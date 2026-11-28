/**
 * Citas del formulario oficial dentro de la pizarra.
 *
 * Cuando el profesor toma una ecuación del formulario II-2026, aparece el recorte real de esa
 * casilla de la hoja, encerrado en un círculo del mismo color y número que en la pestaña
 * Fórmulas. Los renglones que salen de ella (igualar, despejar, sustituir) llevan una barra de
 * ese color con el número de la casilla. Así se ve, sin buscarlo, de dónde sale cada cosa.
 */

import { bus, menosMovimiento, urlApi } from '../nucleo/api.js';
import { TintaMate } from '../mate/tinta.js';

const NS = 'http://www.w3.org/2000/svg';

/** Círculo hecho a mano alrededor de un rectángulo de w×h (coordenadas del propio SVG). */
function rutaCirculo(w, h, semilla) {
  const cx = w / 2;
  const cy = h / 2;
  const rx = w / 2 - 3;
  const ry = h / 2 - 3;
  const g = ((semilla * 37) % 11) / 10 - 0.5;
  const p = (a, k = 1) => [cx + Math.cos(a) * rx * k, cy + Math.sin(a) * ry * k];
  const pts = [];
  const N = 28;
  const ini = -2.2 + g * 0.3;
  for (let i = 0; i <= N; i += 1) {
    const a = ini + (i / N) * Math.PI * 2.08;
    pts.push(p(a, 1 + 0.025 * Math.sin(a * 3 + g) + (i / N) * 0.02));
  }
  let d = `M ${pts[0][0].toFixed(1)} ${pts[0][1].toFixed(1)}`;
  for (let i = 1; i < pts.length; i += 1) {
    const [x0, y0] = pts[i - 1];
    const [x1, y1] = pts[i];
    d += ` Q ${x0.toFixed(1)} ${y0.toFixed(1)} ${((x0 + x1) / 2).toFixed(1)} ${((y0 + y1) / 2).toFixed(1)}`;
  }
  return d;
}

export class Citas {
  constructor(tablero) {
    this.tablero = tablero;
    this.mapa = null;
    this.porId = new Map();
  }

  usar(lec) {
    this.mapa = lec && lec.formulario_mapa ? lec.formulario_mapa : null;
    this.porId = new Map((this.mapa && this.mapa.celdas ? this.mapa.celdas : []).map((c) => [c.id, c]));
  }

  /** Celdas (con número y color) que usa una acción de escritura. */
  celdasDe(acc) {
    if (!this.mapa || !this.mapa.por_accion || !acc || !acc.id) return null;
    const u = this.mapa.por_accion[acc.id];
    if (!u) return null;
    const cs = u.celdas.map((id) => this.porId.get(id)).filter(Boolean);
    return cs.length ? { celdas: cs, formula: u.formula, hereda: !!u.hereda } : null;
  }

  /** Recorte real de la casilla, con su círculo de color. */
  recorte(celda, ancho) {
    // Un poco hacia adentro para que no se vean las líneas de la tabla
    const [bx0, by0, bx1, by1] = celda.caja;
    const ix = (bx1 - bx0) * 0.012;
    const iy = (by1 - by0) * 0.07;
    const [x0, y0, x1, y1] = [bx0 + ix, by0 + iy, bx1 - ix, by1 - iy];
    const [aw, ah] = (this.mapa && this.mapa.aspecto) || [1000, 1468];
    const W = Math.round(ancho);
    const H = Math.round((W * (y1 - y0) * ah) / ((x1 - x0) * aw));
    const total = W / (x1 - x0);
    const caja = document.createElement('span');
    caja.className = 'cita-recorte';
    caja.style.width = `${W}px`;
    caja.style.height = `${H}px`;
    const hoja = this.mapa?.hoja_url_versionada || this.mapa?.hoja_url || '/api/formulario/hoja.png';
    caja.style.backgroundImage = `url("${urlApi(hoja)}")`;
    caja.style.backgroundSize = `${total.toFixed(1)}px ${(total * (ah / aw)).toFixed(1)}px`;
    caja.style.backgroundPosition = `${(-x0 * total).toFixed(1)}px ${(-y0 * total * (ah / aw)).toFixed(1)}px`;
    caja.setAttribute('role', 'img');
    caja.setAttribute('aria-label', `Casilla del formulario: ${celda.nombre}`);
    const svg = document.createElementNS(NS, 'svg');
    svg.setAttribute('class', 'cita-circulo');
    svg.setAttribute('viewBox', `0 0 ${W + 16} ${H + 14}`);
    svg.setAttribute('aria-hidden', 'true');
    const p = document.createElementNS(NS, 'path');
    p.setAttribute('d', rutaCirculo(W + 16, H + 14, celda.num));
    svg.appendChild(p);
    caja.appendChild(svg);
    return caja;
  }

  /** Bloque "Del formulario" que va justo antes del renglón que toma la ecuación. */
  cita(celdas) {
    const px = this.tablero.px;
    const fila = document.createElement('div');
    fila.className = 'cita-form';
    for (const c of celdas.filter((x) => !x.es_constante).slice(0, 2)) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = `cita-item form-c-${c.color}`;
      const num = document.createElement('span');
      num.className = 'cita-num';
      num.textContent = String(c.num);
      const txt = document.createElement('span');
      txt.className = 'cita-txt';
      const hayHoja = this.mapa?.hoja_real !== false;
      txt.textContent = (hayHoja ? 'Del formulario · ' : 'Del catálogo · ') + c.nombre;
      const ancho = Math.max(170, Math.min(330, px * 6.4));
      let detalle;
      if (hayHoja) {
        detalle = this.recorte(c, ancho);
      } else {
        const formula = new TintaMate(c.tex, { px: px * 0.72, maxAncho: ancho });
        formula.progreso(1);
        detalle = formula.el;
        detalle.classList.add('cita-ecuacion');
      }
      b.title = (hayHoja ? 'Casilla del formulario. ' : 'Ecuación del catálogo. ') + c.nombre;
      b.append(num, detalle, txt);
      b.addEventListener('click', () => bus.emit('formulario:enfocar', { celdaId: c.id }));
      fila.appendChild(b);
    }
    if (!fila.childElementCount) return null;
    if (!menosMovimiento()) fila.classList.add('anima');
    return fila;
  }

  /** Barra de color y números en un renglón que sale de una o más casillas. */
  marcarUso(el, celdas) {
    const base = celdas.find((c) => !c.es_constante) || celdas[0];
    el.classList.add('usa-form', `form-c-${base.color}`);
    const g = document.createElement('span');
    g.className = 'usos';
    g.setAttribute('aria-label', `Sale de: ${celdas.map((c) => c.nombre).join(', ')}`);
    for (const c of celdas.slice(0, 3)) {
      const i = document.createElement('i');
      i.className = `form-c-${c.color}`;
      i.textContent = String(c.num);
      i.title = `${c.nombre} (${c.etiqueta})`;
      g.appendChild(i);
    }
    el.appendChild(g);
  }

  /** Aplica cita o marca a la tinta recién puesta en la pizarra. */
  aplicar(acc, tinta) {
    const u = this.celdasDe(acc);
    if (!u || !tinta || !tinta.el) return;
    const color = (u.celdas.find((c) => !c.es_constante) || u.celdas[0]).color;
    const cita = u.formula ? this.cita(u.celdas) : null;
    if (cita) {
      tinta.el.before(cita);
      tinta.el.classList.add('usa-form', 'es-formula', `form-c-${color}`);
    } else if (u.hereda) {
      // Sigue saliendo de la misma casilla (despeje, sustitución, resultado): solo la barra de color
      tinta.el.classList.add('usa-form', 'hereda', `form-c-${color}`);
      tinta.el.title = `Sale de: ${u.celdas.map((c) => c.nombre).join(', ')}`;
    } else this.marcarUso(tinta.el, u.celdas);
    this.tablero.programarRecolocar();
  }
}
