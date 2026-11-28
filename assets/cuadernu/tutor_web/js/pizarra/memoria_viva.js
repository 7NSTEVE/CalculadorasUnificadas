/**
 * Tarjeta de Memoria Viva + Selector de 3 Capas (Intuición · Puente · Examen).
 *
 * Resuelve la sobrecarga de memoria de trabajo (pasos muy cortos) manteniendo anclado en una franja
 * discreta estilo guías de interfaz:
 *   1. Selector de profundidad: Intuición (Capa 0 sin fórmulas) · Puente (símbolos) · Examen.
 *   2. Qué se pide en el paso/inciso actual y datos clave del enunciado.
 *   3. Acceso en 1 clic a «Hazlo sin ver» (práctica activa con otros números).
 */

import { TintaMate, TintaTexto } from '../mate/tinta.js';
import { ajustes, bus } from '../nucleo/api.js';

export class MemoriaViva {
  constructor({ contenedor, acciones, lateral }) {
    this.acciones = acciones;
    this.lateral = lateral;
    this.lec = null;
    this.paso = null;
    this.capa = 'examen'; // 'intuicion' | 'puente' | 'examen'
    this.abierta = false;

    this.el = document.createElement('section');
    this.el.className = 'memoria-viva';
    this.el.hidden = true;
    this.el.setAttribute('aria-label', 'Memoria viva y capas de profundidad');

    const barra = document.createElement('div');
    barra.className = 'mv-barra';

    this.metaTxt = document.createElement('span');
    this.metaTxt.className = 'mv-meta';

    const seg = document.createElement('div');
    seg.className = 'seg mv-capas';
    seg.setAttribute('role', 'group');
    seg.setAttribute('aria-label', 'Nivel de explicación');
    this._btns = new Map();
    for (const [id, lbl] of [['intuicion', 'Intuición'], ['puente', 'Puente'], ['examen', 'Examen']]) {
      const b = document.createElement('button');
      b.type = 'button';
      b.dataset.capa = id;
      b.textContent = lbl;
      b.setAttribute('aria-pressed', String(id === 'examen'));
      b.addEventListener('click', () => this.elegirCapa(id));
      seg.appendChild(b);
      this._btns.set(id, b);
    }

    const acc = document.createElement('div');
    acc.className = 'mv-acc';
    this.btnDatos = document.createElement('button');
    this.btnDatos.type = 'button';
    this.btnDatos.className = 'v3d-btn-mini';
    this.btnDatos.textContent = 'Datos y letras';
    this.btnDatos.setAttribute('aria-expanded', 'false');
    this.btnDatos.addEventListener('click', () => {
      this.abierta = !this.abierta;
      this._pintarCuerpo();
    });

    this.btnCiega = document.createElement('button');
    this.btnCiega.type = 'button';
    this.btnCiega.className = 'v3d-btn-mini mv-btn-ciega';
    this.btnCiega.textContent = 'Hazlo sin ver';
    this.btnCiega.title = 'Practicar este tipo de problema sin ver la solución';
    this.btnCiega.addEventListener('click', () => {
      if (this.lec && this.acciones.practicaCiega) {
        this.acciones.practicaCiega(this.lec.origenVariante || this.lec.id);
      }
    });

    acc.append(this.btnDatos, this.btnCiega);
    barra.append(this.metaTxt, seg, acc);

    this.cuerpo = document.createElement('div');
    this.cuerpo.className = 'mv-cuerpo';
    this.cuerpo.hidden = true;

    this.el.append(barra, this.cuerpo);
    contenedor.prepend(this.el);

    bus.on('leccion:abierta', (lec) => this.cargar(lec));
    bus.on('leccion:paso', ({ paso }) => this.alPaso(paso));
  }

  cargar(lec) {
    this.lec = lec;
    this.paso = lec?.pasos?.[0] || null;
    this.capa = 'examen';
    this.abierta = false;
    this.el.hidden = !lec || ajustes.memoriaViva === false;
    this.btnCiega.hidden = !lec || lec.tipo === 'teoria';
    this._actualizarBtns();
    this._pintarBarra();
    this._pintarCuerpo();
  }

  alPaso(paso) {
    this.paso = paso;
    this._pintarBarra();
    if (this.abierta) this._pintarCuerpo();
  }

  elegirCapa(id) {
    this.capa = id;
    this.abierta = id !== 'examen';
    this._actualizarBtns();
    this._pintarCuerpo();
    if (id === 'intuicion' && this.lateral && !this.lateral.abierto) {
      this.lateral.abrir(true);
      this.lateral.ver('visual');
    }
  }

  _actualizarBtns() {
    for (const [k, b] of this._btns) b.setAttribute('aria-pressed', String(k === this.capa));
  }

  _pintarBarra() {
    if (!this.lec) return;
    const p = this.paso;
    const tit = p ? p.titulo : this.lec.titulo;
    this.metaTxt.textContent = tit ? `Ahora: ${tit}` : '';
  }

  _pintarCuerpo() {
    this.btnDatos.setAttribute('aria-expanded', String(this.abierta));
    if (!this.abierta || !this.lec) {
      this.cuerpo.hidden = true;
      return;
    }
    this.cuerpo.hidden = false;
    this.cuerpo.replaceChildren();

    const capas = this.lec.capas_cero || {};
    if (this.capa === 'intuicion') {
      const box = document.createElement('div');
      box.className = 'mv-capa-box';
      const h = document.createElement('strong');
      h.textContent = 'Capa 0 · Intuición física (sin fórmulas)';
      const p1 = document.createElement('p');
      p1.textContent = capas.capa0_analogia || 'Piensa primero qué ocurre físicamente antes de tocar una sola ecuación.';
      const p2 = document.createElement('p');
      p2.className = 'mv-reto';
      p2.textContent = `Prueba en el 3D: ${capas.capa0_reto_3d || 'gira la escena y mueve el deslizador.'}`;
      box.append(h, p1, p2);
      this.cuerpo.appendChild(box);
      return;
    }

    if (this.capa === 'puente') {
      const box = document.createElement('div');
      box.className = 'mv-capa-box';
      const h = document.createElement('strong');
      h.textContent = 'Capa 1 · Traductor de fórmulas y letras';
      box.appendChild(h);
      const lista = capas.capa1_puente || [];
      if (!lista.length) {
        const p = document.createElement('p');
        p.textContent = 'Esta lección usa definiciones básicas; revisa la pestaña Fórmulas para ver cada símbolo.';
        box.appendChild(p);
      } else {
        const grid = document.createElement('div');
        grid.className = 'mv-puente-grid';
        for (const c of lista) {
          const card = document.createElement('div');
          card.className = 'mv-puente-item';
          const nom = document.createElement('span');
          nom.className = 'mv-puente-nom';
          nom.textContent = c.nombre;
          const eq = new TintaMate(c.tex, { px: 19, color: 'azul', maxAncho: 260 });
          eq.progreso(1);
          card.append(nom, eq.el);
          if (Array.isArray(c.simbolos) && c.simbolos.length) {
            const ul = document.createElement('ul');
            ul.className = 'mv-sim-list';
            for (const [s, que, u] of c.simbolos.slice(0, 5)) {
              const li = document.createElement('li');
              li.textContent = `${s}: ${que}${u ? ` (${u})` : ''}`;
              ul.appendChild(li);
            }
            card.appendChild(ul);
          }
          grid.appendChild(card);
        }
        box.appendChild(grid);
      }
      this.cuerpo.appendChild(box);
      return;
    }

    // Vista «Datos y letras» en modo Examen
    const datos = this.lec.apoyo_visual?.datos || [];
    const colDatos = document.createElement('div');
    colDatos.className = 'mv-col';
    const hd = document.createElement('strong');
    hd.textContent = 'Datos clave del enunciado';
    colDatos.appendChild(hd);
    if (datos.length) {
      const ul = document.createElement('ul');
      ul.className = 'mv-datos-ul';
      for (const d of datos.slice(0, 7)) {
        const li = document.createElement('li');
        li.textContent = d.texto;
        ul.appendChild(li);
      }
      colDatos.appendChild(ul);
    } else {
      const t = new TintaTexto((this.lec.enunciado || '').slice(0, 220), { px: 15, clase: 'sin-mano' });
      t.progreso(1);
      colDatos.appendChild(t.el);
    }

    const colSim = document.createElement('div');
    colSim.className = 'mv-col';
    const hs = document.createElement('strong');
    hs.textContent = 'Por qué este paso';
    colSim.appendChild(hs);
    const pq = document.createElement('p');
    pq.textContent = this.paso?.porque || 'Sigue el procedimiento renglón por renglón usando la casilla marcada en Fórmulas.';
    colSim.appendChild(pq);

    const wrap = document.createElement('div');
    wrap.className = 'mv-cols';
    wrap.append(colDatos, colSim);
    this.cuerpo.appendChild(wrap);
  }
}
