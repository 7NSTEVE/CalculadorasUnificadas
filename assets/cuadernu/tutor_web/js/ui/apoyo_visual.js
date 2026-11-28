import { bus } from '../nucleo/api.js';
import { TintaTexto } from '../mate/tinta.js';
import { crearVisual, crearVisual3D } from '../visuales/index.js';

function texto(el, contenido) {
  const t = new TintaTexto(contenido);
  t.progreso(1);
  el.replaceChildren(t.el);
}

function boton(rotulo, alPulsar, clase = 'btn btn-suave') {
  const b = document.createElement('button');
  b.type = 'button';
  b.className = clase;
  b.textContent = rotulo;
  b.addEventListener('click', alPulsar);
  return b;
}

/**
 * Conserva el mismo modelo 3D WebGL2 interactivo al cambiar de inciso, de paso o de tamaño,
 * permitiendo alternar entre la vista 3D (con 5 cámaras y corte) y la gráfica 2D clásica.
 */
export class ApoyoVisual {
  constructor(id, info, original) {
    this.id = id;
    this.info = info;
    this.paso = info.pasos[0];
    this.tipo = 'paso';
    this.eleccion = 0;
    this.original = original;
    this.grande = null;
    this.modo = 'escena';
    this.cambio = {};
    this.escena = crearVisual3D(info);
    this.grafica = null;
    this._graficaEnMarcha = false;
    this.el = document.createElement('section');
    this.el.className = 'apoyo-interactivo';
    this.el.dataset.apoyo = info.modelo;
    this.el.dataset.motor3d = 'webgl2';
    this.el.setAttribute('aria-label', info.titulo);

    this.lectura = document.createElement('div');
    this.lectura.className = 'apoyo-lectura';
    const nav = document.createElement('div');
    nav.className = 'seg apoyo-secciones';
    nav.setAttribute('role', 'group');
    nav.setAttribute('aria-label', 'Qué querés mirar');
    this.secciones = new Map();
    for (const [clave, etiqueta] of [['datos', 'Datos'], ['piden', 'Qué piden'], ['paso', 'Paso actual']]) {
      const b = boton(etiqueta, () => { this.tipo = clave; this.eleccion = 0; this._mostrar(); }, '');
      b.dataset.apoyoSeccion = clave;
      nav.append(b);
      this.secciones.set(clave, b);
    }
    this.opciones = document.createElement('select');
    this.opciones.addEventListener('change', () => {
      this.eleccion = Number(this.opciones.value);
      this._mostrar();
    });
    this.texto = document.createElement('div');
    this.texto.className = 'apoyo-instruccion';
    this.texto.setAttribute('aria-live', 'polite');
    this.verOriginal = boton('Ver enunciado', () => this._verEnunciado());
    this.irPaso = boton('Ir al paso', () => bus.emit('apoyo:paso', { id, n: this.elegida.paso }));
    this.lectura.append(nav, this.opciones, this.texto, this.verOriginal, this.irPaso);

    this.tira = document.createElement('div');
    this.tira.className = 'apoyo-vistas';
    this.alternar = boton('Ver gráfica 2D', () => {
      this.modo = this.modo === 'grafica' ? 'escena' : 'grafica';
      this._montarFigura();
    });
    this.animarGrafica = boton('Animar', () => {
      this._graficaEnMarcha = !this._graficaEnMarcha;
      if (this._graficaEnMarcha) this.grafica?.seguir();
      else this.grafica?.pausar();
      this.animarGrafica.textContent = this._graficaEnMarcha ? 'Pausar' : 'Animar';
    });
    this.tira.append(this.alternar, this.animarGrafica);
    this.figura = document.createElement('div');
    this.figura.className = 'apoyo-figura';
    this.el.append(this.lectura, this.tira, this.figura);
    this._montarFigura();
    this._mostrar();
  }

  _montarFigura() {
    const original = this.modo === 'grafica' || !this.escena;
    if (original && !this.grafica && this.original) {
      this.grafica = crearVisual(this.original);
      this.grafica?.pausar();
      this.grafica?.cambiar(this.cambio);
    }
    this.activa?.pausar();
    this.activa = original ? this.grafica : this.escena;
    this._graficaEnMarcha = false;
    this.alternar.hidden = !this.escena || !this.original;
    this.alternar.textContent = original ? 'Ver modelo 3D' : 'Ver gráfica 2D';
    this.animarGrafica.hidden = !original || !this.grafica;
    this.animarGrafica.textContent = 'Animar';
    this.figura.replaceChildren();
    if (this.activa) this.figura.append(this.activa.el);
  }

  _verEnunciado() {
    if (this.tipo === 'datos' && this.elegida?.id) {
      bus.emit('apoyo:dato', { id: this.id, dato: this.elegida.id });
    } else {
      bus.emit('apoyo:enunciado', { id: this.id });
    }
  }

  _mostrar() {
    for (const [clave, b] of this.secciones) b.setAttribute('aria-pressed', String(clave === this.tipo));
    let lista = [];
    if (this.tipo === 'datos') lista = this.info.datos || [];
    else if (this.tipo === 'piden') lista = this.info.piden || [];
    this.opciones.hidden = lista.length < 2;
    this.opciones.setAttribute('aria-label', this.tipo === 'datos' ? 'Elegí un dato del enunciado' : 'Elegí un inciso');
    this.opciones.replaceChildren(...lista.map((item, i) => {
      const o = document.createElement('option');
      o.value = String(i);
      o.textContent = item.titulo || `Dato ${i + 1}`;
      return o;
    }));
    this.opciones.value = String(this.eleccion);
    const actual = lista[this.eleccion];
    this.elegida = actual;
    this.irPaso.hidden = this.tipo !== 'piden' || !actual?.paso;
    this.verOriginal.hidden = this.tipo === 'paso';
    this.verOriginal.textContent = this.tipo === 'datos' && actual ? 'Señalar en el enunciado' : 'Ver enunciado';
    if (this.tipo === 'paso') {
      texto(this.texto, `${this.paso.titulo}. ${this.info.tarea}`);
    } else {
      texto(this.texto, actual?.texto || 'Mirá los datos y las condiciones de este problema.');
    }
    const foco = this.tipo === 'paso' ? this.paso.foco : actual?.foco;
    if (foco || this.tipo !== 'paso' || !this.cambio.foco) {
      this.escena?.cambiar({ foco: foco?.id || null });
    }
  }

  actualizarPaso(i) {
    this.paso = this.info.pasos[i] || this.info.pasos[0];
    if (this.tipo !== 'paso') return;
    if (this.cambio.foco) this.cambio = { ...this.cambio, foco: null };
    this._mostrar();
  }

  ponerGrafica(spec) {
    this.grafica?.destruir();
    this.grafica = null;
    this.original = spec;
    this._montarFigura();
  }

  cambiar(cambio) {
    this.cambio = { ...this.cambio, ...cambio };
    this.grafica?.cambiar(cambio);
    if (this.tipo === 'paso') this.escena?.cambiar(cambio);
  }

  pausar() {
    this.activa?.pausar();
    this._graficaEnMarcha = false;
    this.animarGrafica.textContent = 'Animar';
  }

  seguir() {}

  destruir() {
    this.escena?.destruir();
    this.grafica?.destruir();
  }
}
