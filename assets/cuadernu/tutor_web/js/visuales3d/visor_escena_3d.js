/**
 * Visor 3D Interactivo Unificado para el Tutor de CuadernU.
 *
 * Implementa el mismo contrato de ciclo de vida que `Visual` (`el`, `canvas`, `cambiar()`,
 * `pausar()`, `seguir()`, `destruir()`) pero renderiza WebGL2 real con Three.js r187 offline:
 *   - 5 vistas de cámara en 1 clic: 3D · Frontal · Lateral · Superior · Corte (sección real).
 *   - Chips «Ver en 3D» conectados al enunciado, a los incisos (A, B, C) y a los datos.
 *   - Controles físicos interactivos (tiempo/corte + explorador «¿Qué pasa si…?»).
 *   - Gráfica 2D acoplada en vivo para ciclos p-V y barras de energía K+U.
 *   - Botón explícito «Ampliar 3D» separado del arrastre orbital (nunca abre modal al girar).
 */

import { menosMovimiento } from '../nucleo/api.js';
import { alCambiarTema, color, paleta } from '../nucleo/paleta.js';
import { num } from '../visuales/base.js';
import { CapaEtiquetas3D } from './etiquetas_3d.js';
import { Motor3D, VISTAS_3D } from './motor_3d.js';
import {
  construirCuerda3D,
  construirPendulo3D,
  construirResorte3D,
  construirSonido3D,
} from './escenas_p1.js';
import {
  construirCompuerta3D,
  construirFlotacion3D,
  construirManometro3D,
  construirTension3D,
  construirTuberia3D,
} from './escenas_p2.js';
import {
  construirCalor3D,
  construirDilatacion3D,
  construirPiston3D,
  construirSolido3D,
  estadoCiclo,
} from './escenas_p3.js';

const CONSTRUCTORES = {
  resorte: construirResorte3D,
  pendulo: construirPendulo3D,
  cuerda: construirCuerda3D,
  sonido: construirSonido3D,
  compuerta: construirCompuerta3D,
  flotacion: construirFlotacion3D,
  tuberia: construirTuberia3D,
  manometro: construirManometro3D,
  tension: construirTension3D,
  solido: construirSolido3D,
  piston: construirPiston3D,
  dilatacion: construirDilatacion3D,
  calor: construirCalor3D,
  radiacion: construirCalor3D,
};

const CONTROLES = {
  resorte: 'Fase del resorte',
  pendulo: 'Tiempo del péndulo',
  cuerda: 'Fase de la onda',
  sonido: 'Distancia r',
  compuerta: 'Profundidad de franja h',
  flotacion: 'Comparar fluido / caída',
  tuberia: 'Flujo del fluido',
  manometro: 'Tramo de presión',
  tension: 'Deformación / recorrido',
  solido: 'Posición del corte',
  piston: 'Recorrido del proceso',
  dilatacion: 'Calentamiento ΔT',
  calor: 'Avance al equilibrio',
  radiacion: 'Temperatura T (K)',
};

const CAMARA_POR_FOCO = {
  corte: 'corte',
  franja: 'perspectiva',
  superficie: 'frontal',
  presion: 'frontal',
  nodos: 'frontal',
  equilibrio: 'frontal',
  pivote: 'frontal',
  centro: 'frontal',
  volumen: 'corte',
  calor: 'corte',
  limites: 'perspectiva',
};

const ICO_AMPLIAR = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 11V3h-8l3.29 3.29-10 10L3 13v8h8l-3.29-3.29 10-10L21 11Z"/></svg>';

export function soportaEscena3D(modelo) {
  return Boolean(modelo && CONSTRUCTORES[modelo]);
}

export class VisorEscena3D {
  constructor(apoyo, { alAmpliar = null, alAlternar2D = null, tiene2D = false } = {}) {
    this.apoyo = apoyo || {};
    this.modelo = this.apoyo.modelo || 'resorte';
    this.params = this.apoyo.parametros || {};
    this.es3D = true;
    this.foco = null;
    this.porcion = this.modelo === 'compuerta' || this.modelo === 'solido' ? 0.55 : 0.0;
    this.extra = {};
    // Abre en pausa por defecto (para no distraer: nada se mueve sin pedirlo; siempre responde a giro, corte y teclado)
    this.enMarcha = false;
    this.vel = 1;
    this.vivo = true;
    this.visible = true;
    this.sucio = true;
    this._t = 0;
    this._antes = performance.now();
    this._raf = 0;
    this._bucle = this._bucle.bind(this);

    this.el = document.createElement('div');
    this.el.className = 'visual visual-3d';
    this.el.dataset.modelo = this.modelo;

    // Cabecera limpia con título + acciones discretas (Ampliar 3D / Ver 2D)
    const cab = document.createElement('div');
    cab.className = 'v3d-cab';
    const tit = document.createElement('strong');
    tit.className = 'v3d-tit';
    tit.textContent = this.apoyo.titulo || 'Modelo 3D interactivo';
    const acc = document.createElement('div');
    acc.className = 'v3d-cab-acc';
    if (tiene2D && alAlternar2D) {
      const b2d = document.createElement('button');
      b2d.type = 'button';
      b2d.className = 'v3d-btn-mini';
      b2d.textContent = 'Gráfica 2D';
      b2d.title = 'Alternar con la gráfica clásica 2D';
      b2d.addEventListener('click', () => alAlternar2D());
      acc.appendChild(b2d);
    }
    if (alAmpliar) {
      const bAmp = document.createElement('button');
      bAmp.type = 'button';
      bAmp.className = 'v3d-btn-mini v3d-btn-ampliar';
      bAmp.innerHTML = `${ICO_AMPLIAR}<span>Ampliar</span>`;
      bAmp.title = 'Ver el modelo 3D en pantalla grande';
      bAmp.addEventListener('click', (e) => { e.stopPropagation(); alAmpliar(); });
      acc.appendChild(bAmp);
    }
    cab.append(tit, acc);
    this.el.appendChild(cab);

    // Barra segmentada de 5 cámaras (guías de interfaz)
    const barCam = document.createElement('div');
    barCam.className = 'v3d-vistas';
    barCam.setAttribute('role', 'group');
    barCam.setAttribute('aria-label', 'Punto de vista 3D');
    this._btnsVista = new Map();
    for (const [id, info] of Object.entries(VISTAS_3D)) {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'v3d-cam-btn';
      b.dataset.camara = id;
      b.textContent = info.nombre;
      b.setAttribute('aria-pressed', String(id === 'perspectiva'));
      b.addEventListener('click', () => this.irAVista(id));
      barCam.appendChild(b);
      this._btnsVista.set(id, b);
    }
    this.el.appendChild(barCam);

    // Escenario WebGL2 + capa de etiquetas 2D nítidas
    this.marco = document.createElement('div');
    this.marco.className = 'v3d-marco';
    this.canvas = document.createElement('canvas');
    this.canvas.className = 'v3d-canvas';
    this.canvas.tabIndex = 0;
    this.canvas.setAttribute('role', 'img');
    this.canvas.setAttribute('aria-label', `${this.apoyo.titulo || 'Escena 3D'}. Arrastra para girar, rueda para acercar, flechas del teclado para orbitar.`);
    this.marco.appendChild(this.canvas);
    this.etiquetas = new CapaEtiquetas3D(this.marco);
    this.el.appendChild(this.marco);

    // Mini gráfica 2D sincronizada (ciclo p-V vivo junto al pistón 3D, o barras de energía MAS)
    this.miniCanvas = null;
    if (this.modelo === 'piston' && this.params.estados && this.params.procesos?.length) {
      this.miniCanvas = document.createElement('canvas');
      this.miniCanvas.className = 'v3d-mini2d';
      this.miniCanvas.width = 480;
      this.miniCanvas.height = 175;
      this.el.appendChild(this.miniCanvas);
    }

    // Lectura numérica viva en 1 línea
    this.medidas = document.createElement('p');
    this.medidas.className = 'v3d-medidas';
    this.el.appendChild(this.medidas);

    // Chips «Ver en 3D» conectados a incisos del problema
    const piden = Array.isArray(this.apoyo.piden) ? this.apoyo.piden : [];
    if (piden.length) {
      const filaInc = document.createElement('div');
      filaInc.className = 'v3d-chips';
      filaInc.setAttribute('role', 'group');
      filaInc.setAttribute('aria-label', 'Enfocar inciso en 3D');
      for (const it of piden.slice(0, 6)) {
        const ch = document.createElement('button');
        ch.type = 'button';
        ch.className = 'v3d-chip';
        ch.textContent = it.titulo;
        ch.title = it.texto || it.titulo;
        ch.addEventListener('click', () => {
          filaInc.querySelectorAll('.v3d-chip').forEach((x) => x.setAttribute('aria-pressed', 'false'));
          ch.setAttribute('aria-pressed', 'true');
          const focoId = it.foco?.id || null;
          this.cambiar({ foco: focoId, camara: it.camara || CAMARA_POR_FOCO[focoId] || 'perspectiva' });
          this.etiquetas.mostrarPieza({
            titulo: it.titulo,
            detalle: it.texto,
            formula: it.foco?.texto ? `Enfoque 3D: ${it.foco.texto}` : '',
          });
        });
        filaInc.appendChild(ch);
      }
      this.el.appendChild(filaInc);
    }

    // Controles: Animar/Pausar + deslizador principal + deslizador explorador («¿Qué pasa si…?»)
    this.mandos = document.createElement('div');
    this.mandos.className = 'visual-mandos v3d-mandos';
    const filaAcc = document.createElement('div');
    filaAcc.className = 'v3d-fila-acc';
    this.btnAnimar = document.createElement('button');
    this.btnAnimar.type = 'button';
    this.btnAnimar.className = 'mando-btn';
    this.btnAnimar.addEventListener('click', () => this._alternarAnimacion());
    filaAcc.appendChild(this.btnAnimar);

    const hint = document.createElement('span');
    hint.className = 'v3d-hint';
    hint.textContent = 'Arrastra para girar · Toca una pieza';
    filaAcc.appendChild(hint);
    this.mandos.appendChild(filaAcc);

    this._slPrincipal = this._crearDeslizador(
      CONTROLES[this.modelo] || 'Avance',
      0, 1, 0.01, this.porcion,
      (v) => `${Math.round(v * 100)} %`,
      (v) => {
        this.pausar();
        this.porcion = v;
        this.sucio = true;
        this._pedir();
      },
    );

    // Explorador físico adicional según el modelo (intuición instantánea)
    if (this.modelo === 'compuerta') {
      this._crearDeslizador('Sumar N franjas (integral)', 1, 24, 1, 1,
        (v) => (v <= 1 ? '1 franja' : `${v} franjas`),
        (v) => { this.extra.franjas = v; this.sucio = true; this._pedir(); },
      );
    } else if (this.modelo === 'cuerda' && (this.params.onda || 'estacionaria') === 'estacionaria') {
      const nIni = this.params.armonico || 2;
      this.extra.armonico = nIni;
      this._crearDeslizador('Armónico n', 1, 5, 1, nIni,
        (v) => `n = ${v}`,
        (v) => { this.extra.armonico = v; this.sucio = true; this._pedir(); },
      );
    } else if (this.modelo === 'resorte' && !this.params.comparar) {
      this.extra.escalaA = 1;
      this._crearDeslizador('Escala de amplitud A', 0.5, 1.5, 0.1, 1,
        (v) => `${num(v, 2)}×A`,
        (v) => { this.extra.escalaA = v; this.sucio = true; this._pedir(); },
      );
    }

    this.el.appendChild(this.mandos);

    // Arrancar motor WebGL2
    this.motor = new Motor3D(this.canvas, {
      alCambiarVista: (id) => this._marcarVista(id),
      alTocarPieza: (info) => this.etiquetas.mostrarPieza(info),
    });
    if (this.apoyo.camara_inicial && VISTAS_3D[this.apoyo.camara_inicial]) {
      this.motor.irAVista(this.apoyo.camara_inicial, { inmediato: true });
    }

    this._actualizarBotonAnim();
    this._quitaTema = alCambiarTema(() => { this.sucio = true; this._pedir(); });
    this._ro = new ResizeObserver(() => this._medir());
    this._ro.observe(this.marco);
    this._io = new IntersectionObserver((es) => {
      this.visible = es.some((e) => e.isIntersecting);
      if (this.visible) this._pedir();
    });
    this._io.observe(this.canvas);
    requestAnimationFrame(() => { this._medir(); this._pedir(); });
  }

  _crearDeslizador(etiqueta, min, max, paso, valor, formato, alMover) {
    const fila = document.createElement('label');
    fila.className = 'mando';
    const t = document.createElement('span');
    t.className = 'mando-txt';
    t.textContent = etiqueta;
    const r = document.createElement('input');
    r.type = 'range';
    r.min = String(min);
    r.max = String(max);
    r.step = String(paso);
    r.value = String(valor);
    r.setAttribute('aria-label', etiqueta);
    const out = document.createElement('output');
    out.className = 'mando-val';
    const pinta = () => { out.textContent = formato(Number(r.value)); };
    r.addEventListener('input', () => { pinta(); alMover(Number(r.value)); });
    pinta();
    fila.append(t, r, out);
    this.mandos.appendChild(fila);
    return { poner: (x) => { r.value = String(x); pinta(); } };
  }

  _marcarVista(id) {
    for (const [k, b] of this._btnsVista) b.setAttribute('aria-pressed', String(k === id));
    this.sucio = true;
    this._pedir();
  }

  irAVista(id) {
    this.motor.irAVista(id);
    this.sucio = true;
    this._pedir();
  }

  _alternarAnimacion() {
    if (this.enMarcha) {
      this.pausar();
      return;
    }
    if (menosMovimiento()) {
      this.porcion = (this.porcion + 0.16) % 1;
      this._slPrincipal.poner(this.porcion);
      this.sucio = true;
      this._pedir();
      return;
    }
    this.enMarcha = true;
    this._actualizarBotonAnim();
    this._pedir();
  }

  _actualizarBotonAnim() {
    this.btnAnimar.textContent = menosMovimiento() ? 'Avanzar paso' : this.enMarcha ? 'Pausar' : 'Animar';
    this.btnAnimar.setAttribute('aria-pressed', String(this.enMarcha && !menosMovimiento()));
  }

  pausar() {
    this.enMarcha = false;
    this._actualizarBotonAnim();
  }

  seguir() {
    if (!menosMovimiento()) {
      this.enMarcha = true;
      this._actualizarBotonAnim();
      this._pedir();
    }
  }

  cambiar(cambio = {}) {
    if (!cambio) return;
    if ('foco' in cambio) this.foco = cambio.foco;
    if (cambio.camara && VISTAS_3D[cambio.camara]) {
      this.motor.irAVista(cambio.camara);
    } else if (cambio.foco && CAMARA_POR_FOCO[cambio.foco]) {
      this.motor.irAVista(CAMARA_POR_FOCO[cambio.foco]);
    }
    if (typeof cambio.pos === 'number') {
      this.porcion = Math.max(0, Math.min(1, cambio.pos));
      this._slPrincipal.poner(this.porcion);
    }
    if (this.modelo === 'piston' && cambio.foco && this.params.procesos?.length) {
      const idx = this.params.procesos.findIndex(([a, b]) => `${a}${b}` === cambio.foco);
      if (idx >= 0) {
        this.porcion = (idx + 0.5) / this.params.procesos.length;
        this._slPrincipal.poner(this.porcion);
      }
    }
    if (this.modelo === 'compuerta' && typeof cambio.n === 'number') {
      this.extra.franjas = cambio.n;
    }
    this.sucio = true;
    this._pedir();
  }

  _medir() {
    const w = this.marco.clientWidth;
    if (w < 40) return;
    const h = this.marco.clientHeight > 120 ? this.marco.clientHeight : Math.max(240, Math.min(440, Math.round(w * 0.66)));
    this.motor.redimensionar(w, h);
    this.sucio = true;
    this._pedir();
  }

  _pedir() {
    if (!this.vivo || this._raf) return;
    this._antes = performance.now();
    this._raf = requestAnimationFrame(this._bucle);
  }

  _dibujarMiniPV(e) {
    if (!this.miniCanvas || !this.params.estados || !this.params.procesos) return;
    const ctx = this.miniCanvas.getContext('2d');
    if (!ctx) return;
    const W = this.miniCanvas.width;
    const H = this.miniCanvas.height;
    const pal = paleta();
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = pal.tarjeta;
    ctx.fillRect(0, 0, W, H);

    const pts = Object.values(this.params.estados);
    const vMax = Math.max(...pts.map(([v]) => v)) * 1.15;
    const pMax = Math.max(...pts.map(([, p]) => p)) * 1.15;
    const X = (v) => 52 + (v / (vMax || 1)) * (W - 76);
    const Y = (p) => H - 30 - (p / (pMax || 1)) * (H - 50);

    // Ejes
    ctx.strokeStyle = pal.tinta2;
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.moveTo(52, 16);
    ctx.lineTo(52, H - 30);
    ctx.lineTo(W - 18, H - 30);
    ctx.stroke();

    ctx.fillStyle = pal.tinta2;
    ctx.font = '600 12px system-ui, sans-serif';
    ctx.fillText(`p (${this.params.up || 'Pa'})`, 10, 18);
    ctx.fillText(`V (${this.params.uV || 'm³'})`, W - 64, H - 10);

    const cols = ['azul', 'morado', 'verde', 'naranja'];
    const procesos = this.params.procesos;
    procesos.forEach((proc, idx) => {
      ctx.strokeStyle = color(cols[idx % cols.length]);
      ctx.lineWidth = idx === e.i ? 3.5 : 2;
      ctx.beginPath();
      for (let j = 0; j <= 24; j += 1) {
        const o = estadoCiclo(this.params.estados, procesos, this.params.gamma || 1.4, (idx + j / 24) / procesos.length);
        const px = X(o.v);
        const py = Y(o.p);
        if (j === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }
      ctx.stroke();
    });

    const pMin = Math.min(...pts.map(([, p]) => p));
    for (const [id, [v, p]] of Object.entries(this.params.estados)) {
      ctx.fillStyle = pal.tinta;
      ctx.beginPath();
      ctx.arc(X(v), Y(p), 3.2, 0, Math.PI * 2);
      ctx.fill();
      const dy = p <= pMin + 0.08 * (pMax - pMin) ? 14 : -7;
      ctx.fillText(id, X(v) + 5, Y(p) + dy);
    }

    // Punto vivo sincronizado con el pistón 3D
    ctx.fillStyle = color('naranja');
    ctx.strokeStyle = pal.tinta;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(X(e.v), Y(e.p), 6.2, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();
  }

  _bucle(ahora) {
    this._raf = 0;
    if (!this.vivo) return;
    const dt = Math.min(0.06, (ahora - this._antes) / 1000);
    this._antes = ahora;
    const anima = this.enMarcha && !menosMovimiento() && this.visible && !document.hidden;
    if (anima) {
      this._t += dt * this.vel;
      this.porcion = (this.porcion + dt * 0.14 * this.vel) % 1;
      this._slPrincipal.poner(this.porcion);
      this.sucio = true;
    }

    const camMov = this.motor.camaraEnMovimiento();
    if (this.sucio) {
      this.sucio = false;
      this.motor.limpiarEscena();
      const fn = CONSTRUCTORES[this.modelo] || construirResorte3D;
      try {
        const res = fn(this.motor, this.params, this.porcion, this.foco, this.extra) || {};
        this._ultTags = res.tags || [];
        if (res.medida && this.medidas.textContent !== res.medida) {
          this.medidas.textContent = res.medida;
        }
        if (res.ciclo) this._dibujarMiniPV(res.ciclo);
      } catch (err) {
        console.error('escena3d', this.modelo, err);
      }
    }

    if (anima || camMov || this._ultTags) {
      this.motor.renderizar();
      this.etiquetas.actualizar(this.motor, this._ultTags || []);
    }

    if (anima || camMov) {
      this._raf = requestAnimationFrame(this._bucle);
    }
  }

  destruir() {
    this.vivo = false;
    if (this._raf) cancelAnimationFrame(this._raf);
    this._quitaTema?.();
    this._ro?.disconnect();
    this._io?.disconnect();
    this.etiquetas?.destruir();
    this.motor?.destruir();
    this.el.remove();
  }
}
