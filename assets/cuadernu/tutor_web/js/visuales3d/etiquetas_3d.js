/**
 * Capa de etiquetas 2D proyectadas desde el mundo 3D.
 * Mantiene tipografía matemática nítida a cualquier DPI (incluido 4K) sin borrosidad de texturas.
 */

import { textoPlano } from '../mate/tex.js';

/**
 * R7: texto corto para una etiqueta 3D, sin LaTeX crudo: cada tramo $…$ pasa a texto plano
 * (dF_⊥, Δh, ρ, …) y los subíndices o exponentes de un solo símbolo quedan agrupados para que
 * `ponerMateCorto` los dibuje como <sub>/<sup>.
 */
export function textoEtiqueta(s, max = 0) {
  let out = String(s || '').replace(/\$([^$]*)\$/g, (_, tex) => textoPlano(tex));
  out = out.replace(/([_^])([^\s{A-Za-z0-9])/g, '$1{$2}');
  // (3)/(2) → 3/2, y un espacio antes de la letra que sigue: «3/2 R»
  out = out.replace(/\(([^()\s]{1,8})\)\/\(([^()\s]{1,8})\)/g, '$1/$2');
  out = out.replace(/(\d\/\d+)([A-Za-zΑ-ω])/g, '$1 $2');
  out = out.replace(/\s*=\s*/g, ' = ');
  out = out.replace(/\s+/g, ' ').trim();
  if (max && out.length > max) out = `${out.slice(0, max - 1).trimEnd()}…`;
  return out;
}

function ponerMateCorto(el, txt) {
  el.replaceChildren();
  const s = String(txt || '');
  const re = /_\{([^}]+)\}|_([A-Za-z0-9]+)|\^\{([^}]+)\}|\^([A-Za-z0-9−\-+]+)/g;
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
      sup.textContent = m[3] ?? m[4];
      el.appendChild(sup);
    }
    ult = re.lastIndex;
  }
  if (ult < s.length) el.appendChild(document.createTextNode(s.slice(ult)));
}

export class CapaEtiquetas3D {
  constructor(contenedor) {
    this.el = document.createElement('div');
    this.el.className = 'v3d-etiquetas';
    this.el.setAttribute('aria-hidden', 'true');
    contenedor.appendChild(this.el);

    this.ficha = document.createElement('div');
    this.ficha.className = 'v3d-ficha';
    this.ficha.hidden = true;
    contenedor.appendChild(this.ficha);

    this._nodos = new Map();
  }

  /**
   * items: [{ id, pos: [x,y,z], texto, activo, color, clase }]
   * Con `columna: 'izq' | 'der'` (en vez de `pos`) la etiqueta va fija en esa columna sobre la
   * escena, una debajo de la otra: así los datos y lo que piden no se tapan entre sí ni tapan la pieza.
   */
  actualizar(motor, items = []) {
    const vivos = new Set();
    const filas = { izq: 0, der: 0 };
    for (const it of items) {
      vivos.add(it.id);
      let nodo = this._nodos.get(it.id);
      if (!nodo) {
        nodo = document.createElement('span');
        nodo.className = 'v3d-tag';
        this.el.appendChild(nodo);
        this._nodos.set(it.id, nodo);
      }
      if (nodo.dataset.txt !== it.texto) {
        nodo.dataset.txt = it.texto;
        ponerMateCorto(nodo, it.texto);
      }
      nodo.classList.toggle('activo', Boolean(it.activo));
      nodo.classList.toggle('dato', it.clase === 'dato');
      nodo.classList.toggle('pide', it.clase === 'pide');
      nodo.dataset.color = it.color || '';
      nodo.classList.toggle('col-izq', it.columna === 'izq');
      nodo.classList.toggle('col-der', it.columna === 'der');
      if (it.columna === 'izq' || it.columna === 'der') {
        const k = filas[it.columna];
        filas[it.columna] += 1;
        nodo.hidden = false;
        nodo.style.transform = `translateY(${6 + k * 23}px)`;
        continue;
      }
      const p = motor.proyectarPantalla(it.pos);
      if (!p.visible) {
        nodo.hidden = true;
      } else {
        nodo.hidden = false;
        nodo.style.transform = `translate(${p.x.toFixed(1)}px, ${p.y.toFixed(1)}px) translate(-50%, -50%)`;
      }
    }
    for (const [id, nodo] of this._nodos.entries()) {
      if (!vivos.has(id)) {
        nodo.remove();
        this._nodos.delete(id);
      }
    }
  }

  mostrarPieza(info) {
    if (!info) {
      this.ficha.hidden = true;
      return;
    }
    this.ficha.replaceChildren();
    const cab = document.createElement('div');
    cab.className = 'v3d-ficha-cab';
    const tit = document.createElement('strong');
    ponerMateCorto(tit, info.titulo || 'Elemento');
    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'v3d-ficha-x';
    cerrar.setAttribute('aria-label', 'Cerrar detalle');
    cerrar.textContent = '×';
    cerrar.addEventListener('click', () => { this.ficha.hidden = true; });
    cab.append(tit, cerrar);
    this.ficha.appendChild(cab);

    if (info.detalle) {
      const p = document.createElement('p');
      ponerMateCorto(p, info.detalle);
      this.ficha.appendChild(p);
    }
    if (info.formula) {
      const f = document.createElement('code');
      f.className = 'v3d-ficha-eq';
      ponerMateCorto(f, info.formula);
      this.ficha.appendChild(f);
    }
    this.ficha.hidden = false;
  }

  destruir() {
    this.el.remove();
    this.ficha.remove();
    this._nodos.clear();
  }
}
