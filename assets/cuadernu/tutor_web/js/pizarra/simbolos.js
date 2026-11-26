/**
 * Qué es cada letra: mientras la pizarra escribe una fórmula, aparece unos segundos una tarjeta con
 * el significado y las unidades de cada letra nueva de ese renglón (lo calcula el servidor en
 * `lec.simbolos.por_accion`). No tapa la pizarra: va al pie del panel de apoyo, debajo del visual,
 * y solo si ese panel está oculto se pone en una esquina de la escena.
 */

import { ajustes } from '../nucleo/api.js';

const ESC = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' };
const esc = (t) => String(t).replace(/[&<>"]/g, (c) => ESC[c]);

/** 'T_1' → T<sub>1</sub>; 'Q_H,A' → Q<sub>H,A</sub>; 'ΔT', 'dr', '∫' tal cual. */
function simboloHtml(s) {
  const k = s.indexOf('_');
  if (k < 0) return `<span class="ley-base">${esc(s)}</span>`;
  return `<span class="ley-base">${esc(s.slice(0, k))}</span><sub>${esc(s.slice(k + 1))}</sub>`;
}

export class Leyenda {
  constructor({ lateral, escena }) {
    this.lateral = lateral;
    this.escena = escena;
    this.timer = 0;
    this.quieta = false;
    const el = document.createElement('section');
    el.className = 'leyenda';
    el.hidden = true;
    el.setAttribute('aria-label', 'Qué es cada letra');
    el.innerHTML = '<div class="leyenda-cab"><span>Qué es cada letra</span>'
      + '<button type="button" class="leyenda-x" aria-label="Cerrar">×</button></div><dl class="leyenda-lista"></dl>';
    this.el = el;
    this.lista = el.querySelector('.leyenda-lista');
    el.querySelector('.leyenda-x').addEventListener('click', () => this.ocultar());
    // Mientras se mira (puntero encima) no se va
    el.addEventListener('pointerenter', () => { this.quieta = true; clearTimeout(this.timer); });
    el.addEventListener('pointerleave', () => { this.quieta = false; this._programar(3000); });
  }

  _lugar() {
    const enLateral = Boolean(this.lateral && !this.lateral.hidden && this.lateral.getClientRects().length);
    const destino = enLateral ? this.lateral : this.escena;
    this.el.classList.toggle('flotante', !enLateral);
    if (destino && this.el.parentElement !== destino) destino.appendChild(this.el);
  }

  /** items: [[símbolo, significado, unidades], …] */
  mostrar(items) {
    if (!ajustes.simbolos || !Array.isArray(items) || !items.length) return;
    this._lugar();
    this.lista.replaceChildren(...items.map(([s, que, u]) => {
      const fila = document.createElement('div');
      fila.className = 'leyenda-fila';
      const dt = document.createElement('dt');
      dt.innerHTML = simboloHtml(s);
      const dd = document.createElement('dd');
      dd.append(que);
      if (u) {
        const sp = document.createElement('span');
        sp.className = 'leyenda-u';
        sp.textContent = u;
        dd.append(' ', sp);
      }
      fila.append(dt, dd);
      return fila;
    }));
    this.el.hidden = false;
    this.el.classList.remove('sale');
    void this.el.offsetWidth; // reinicia la animación de entrada
    this.el.classList.add('entra');
    // Tiempo para leerla con calma: más filas, más tiempo
    this._programar(Math.min(18000, Math.max(7000, 2600 * items.length)));
  }

  _programar(ms) {
    clearTimeout(this.timer);
    this.timer = setTimeout(() => { if (!this.quieta) this.ocultar(); }, ms);
  }

  ocultar() {
    clearTimeout(this.timer);
    if (this.el.hidden) return;
    this.el.classList.remove('entra');
    this.el.classList.add('sale');
    setTimeout(() => { if (this.el.classList.contains('sale')) this.el.hidden = true; }, 280);
  }
}
