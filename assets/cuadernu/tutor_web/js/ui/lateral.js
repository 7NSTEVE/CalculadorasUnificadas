/**
 * Panel lateral: el visual animado que evoluciona con la explicación, el enunciado a mano y las
 * fórmulas que se están usando. Se puede ocultar (modo enfoque).
 */

import { TintaMate, TintaTexto } from '../mate/tinta.js';
import { bus, urlApi } from '../nucleo/api.js';
import { crearVisual } from '../visuales/index.js';

export class Lateral {
  constructor(el) {
    this.el = el;
    this.visual = null;
    this.lec = null;
    this.tab = 'visual';
    this.barra = el.querySelector('.lateral-tabs');
    this.paneles = {
      visual: el.querySelector('#panel-visual'),
      enunciado: el.querySelector('#panel-enunciado'),
      formulas: el.querySelector('#panel-formulas'),
    };
    this.barra.addEventListener('click', (e) => {
      const b = e.target.closest('[data-tab]');
      if (b) this.ver(b.dataset.tab);
    });
    this.barra.addEventListener('keydown', (e) => {
      const bs = [...this.barra.querySelectorAll('[data-tab]:not([hidden])')];
      const i = bs.indexOf(document.activeElement);
      if (i < 0) return;
      const d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
      if (!d) return;
      e.preventDefault();
      const n = bs[(i + d + bs.length) % bs.length];
      n.focus();
      this.ver(n.dataset.tab);
    });
    bus.on('ajuste:letra', () => { if (this.lec) this._formulas(this.lec); });
  }

  ver(tab) {
    this.tab = tab;
    for (const [k, p] of Object.entries(this.paneles)) p.hidden = k !== tab;
    this.barra.querySelectorAll('[data-tab]').forEach((b) => {
      const act = b.dataset.tab === tab;
      b.setAttribute('aria-selected', String(act));
      b.tabIndex = act ? 0 : -1;
    });
  }

  mostrar(si) {
    this.el.hidden = !si;
    document.body.classList.toggle('sin-lateral', !si);
  }

  _boton(tab, visible) {
    const b = this.barra.querySelector(`[data-tab="${tab}"]`);
    if (b) b.hidden = !visible;
  }

  ponerVisual(spec) {
    if (this.visual) { this.visual.destruir(); this.visual = null; }
    const v = crearVisual(spec);
    this.paneles.visual.replaceChildren();
    this._boton('visual', Boolean(v));
    if (!v) { if (this.tab === 'visual') this.ver('enunciado'); return; }
    this.visual = v;
    this.paneles.visual.appendChild(v.el);
    this.ver('visual');
  }

  /** `mostrar` = traer la pestaña del visual al frente. */
  cambiarVisual(cambio, mostrar = true) {
    if (!this.visual) return;
    this.visual.cambiar(cambio);
    if (mostrar && this.tab !== 'visual') this.ver('visual');
  }

  /** Subraya un dato sobre el enunciado del panel, a la vez que en la hoja. */
  marcarDato(caja, color = 'naranja') {
    if (!this.capa) return;
    const m = document.createElement('span');
    m.className = 'dato-marca';
    m.style.left = `${(caja.x * 100).toFixed(2)}%`;
    m.style.top = `${((caja.y + caja.h) * 100).toFixed(2)}%`;
    m.style.width = `${(caja.w * 100).toFixed(2)}%`;
    m.style.background = `var(--${color})`;
    this.capa.appendChild(m);
    if (this.tab === 'enunciado') {
      const r = m.getBoundingClientRect();
      const q = this.paneles.enunciado.getBoundingClientRect();
      if (r.top > q.bottom - 30 || r.top < q.top + 20) m.scrollIntoView({ block: 'center', behavior: 'smooth' });
    }
  }

  quitarDatos() {
    if (this.capa) this.capa.querySelectorAll('.dato-marca').forEach((m) => m.remove());
  }

  abrirLeccion(lec) {
    this.lec = lec;
    this.mostrar(true);
    this.ponerVisual(lec.visual);
    this._enunciado(lec);
    this._formulas(lec);
    this.ver(this.capa || !this.visual ? 'enunciado' : 'visual');
  }

  _enunciado(lec) {
    const p = this.paneles.enunciado;
    p.replaceChildren();
    this.capa = null;
    if (lec.recorte && lec.recorte.url) {
      const marco = document.createElement('div');
      marco.className = 'lateral-recorte';
      const img = document.createElement('img');
      img.alt = 'Enunciado del problema';
      img.src = urlApi(lec.recorte.url);
      img.addEventListener('click', () => bus.emit('ver-imagen', { src: img.src, alt: img.alt }));
      marco.appendChild(img);
      p.appendChild(marco);
      this.capa = marco;
    }
    if (lec.enunciado && !lec.recorte) {
      const t = new TintaTexto(lec.enunciado, { clase: 'lateral-enunciado', px: 19 });
      t.progreso(1);
      p.appendChild(t.el);
    }
    this._boton('enunciado', Boolean(p.childElementCount));
  }

  _formulas(lec) {
    const p = this.paneles.formulas;
    p.replaceChildren();
    for (const f of lec.formulas || []) {
      const t = new TintaMate(f, { px: 30, color: 'azul', maxAncho: Math.max(200, this.el.clientWidth - 60) });
      t.progreso(1);
      p.appendChild(t.el);
    }
    this._boton('formulas', Boolean(p.childElementCount));
  }

  cerrar() {
    if (this.visual) { this.visual.destruir(); this.visual = null; }
    this.lec = null;
    for (const p of Object.values(this.paneles)) p.replaceChildren();
    this.mostrar(false);
  }
}
