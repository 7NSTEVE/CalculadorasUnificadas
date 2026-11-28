/**
 * Capa de etiquetas 2D proyectadas desde el mundo 3D.
 * Mantiene tipografía matemática nítida a cualquier DPI (incluido 4K) sin borrosidad de texturas.
 */

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

  /** items: [{ id, pos: [x,y,z], texto, activo, color }] */
  actualizar(motor, items = []) {
    const vivos = new Set();
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
      nodo.dataset.color = it.color || '';
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
