/**
 * Todo lo que pasa cuando la persona se pierde o pregunta: ¿por qué?, ¿de dónde salió?, más
 * despacio, preguntas libres a la IA y voz en vivo. Lo determinista va primero: si la lección ya
 * trae la explicación no se espera a la nube.
 */

import { ajustes, bus, conectado, enviar } from '../nucleo/api.js';
import { planAMini, limpiarHabla } from './plan_ia.js';
import { VozViva } from './voz_viva.js';
import { aviso } from './avisos.js';

/** Quita la matemática de un texto para poder narrarlo. */
function hablable(txt) {
  return limpiarHabla(txt);
}

export class Asistente {
  constructor({ rep, tablero, lateral, acciones, miniPaso }) {
    Object.assign(this, { rep, tablero, lateral, acciones, miniPaso });
    this.pensando = null;
    this.libre = null;
    this.cola = Promise.resolve();
    this.viva = new VozViva({ asistente: this, rep });
    bus.on('ws:BEAT_PLAN', (m) => this._plan(m));
    bus.on('ws:LECCION_IA', (m) => this._leccion(m));
    bus.on('ws:STATE_CHANGE', (m) => this._estado(m));
    bus.on('ws:ERROR_RECOVERY', (m) => { this._fin(); aviso(m.message || 'No pude responder. Intenta otra vez.', { mal: true }); });
    bus.on('ws:ABRIR_LECCION', (m) => { this._fin(); if (m.id) this.acciones.abrir(m.id); });
    bus.on('ws:INCIDENT_SAVED', () => aviso('Fallo guardado para revisarlo.'));
    bus.on('conexion', ({ estado }) => { if (estado !== 'conectado') this._fin(); });
  }

  /* -------------------------------------------------------- lo que ya sabe la lección */

  porque() {
    const p = this.rep.paso;
    if (!p || !p.porque) return;
    this.rep.contar(this.miniPaso([{ habla: hablable(p.porque), acc: [{ a: 'texto', txt: p.porque, id: `pq${this.rep.i}_${Date.now()}` }] }]), '¿Por qué?');
  }

  origen() {
    const p = this.rep.paso;
    if (!p) return;
    const filas = (p.origen && p.origen.length) ? p.origen : (this.rep.lec.formulas || []).map((f) => ({ que: `$${f}$`, de: 'formulario oficial' }));
    if (!filas.length) { this.preguntar('¿De dónde sale lo que acabas de escribir?'); return; }
    const t = Date.now();
    const tramos = filas.map((f, k) => ({
      habla: hablable(`${f.voz || f.que}: ${f.de}`) || f.de,
      acc: [{ a: 'texto', txt: `${f.que} · ${f.de}`, id: `or${t}_${k}` }],
    }));
    this.rep.contar(this.miniPaso(tramos), '¿De dónde salió?');
  }

  despacio() {
    const p = this.rep.paso;
    if (!p) return;
    if (p.lento && p.lento.length) {
      const t = Date.now();
      const tramos = p.lento.map((r, k) => ({
        habla: hablable(r.voz || r.nota || 'Seguimos.') || 'Seguimos.',
        acc: [
          ...(r.nota ? [{ a: 'texto', txt: r.nota, clase: 'nota', id: `ln${t}_${k}` }] : []),
          ...(r.tex ? [{ a: 'mate', tex: r.tex, id: `lm${t}_${k}` }] : []),
        ],
      }));
      this.rep.contar(this.miniPaso(tramos), 'Más despacio');
      return;
    }
    // Sin renglones extra: se repite el paso a ritmo lento
    const antes = ajustes.vel;
    ajustes.vel = Math.max(0.6, antes * 0.8);
    this.rep.repetir();
    const quita = bus.on('leccion:finpaso', () => { ajustes.vel = antes; quita(); });
    aviso('Repito este paso más lento.');
  }

  /* -------------------------------------------------------- preguntas libres */

  contexto() {
    const { rep } = this;
    if (!rep.lec) return null;
    const p = rep.paso;
    return {
      id: rep.lec.id,
      titulo: rep.lec.titulo,
      enunciado: rep.lec.enunciado || '',
      paso: rep.i + 1,
      total: rep.total,
      titulo_paso: p ? p.titulo : '',
      escrito: p ? p.tramos.flatMap((t) => t.acc.filter((a) => a.tex || a.txt).map((a) => a.tex || a.txt)).slice(0, 16) : [],
      escrito_antes: rep.lec.pasos.slice(Math.max(0, rep.i - 2), Math.max(0, rep.i)).flatMap((q) => q.tramos.flatMap((t) => t.acc.filter((a) => a.tex).map((a) => a.tex))).slice(-14),
      respuestas: (rep.lec.respuestas || []).map((r) => `${r.inciso ? `${r.inciso}) ` : ''}${r.tex}`),
    };
  }

  preguntar(q, { seleccion = null, imagen = '' } = {}) {
    if (!conectado()) { aviso('El motor se está reconectando; intenta en unos segundos.', { mal: true }); return; }
    this.rep.interrumpir();
    this._pensar(q);
    enviar({
      type: 'PREGUNTA_LIBRE',
      texto: q,
      contexto: this.contexto(),
      seleccion,
      imagen,
      modelo: ajustes.modelo,
    });
  }

  imagen(b64, { nota = '', texto = '', nombre = 'Imagen', tipo = 'image/png' } = {}) {
    if (!conectado()) { aviso('El motor se está reconectando; la imagen quedó en la hoja.', { mal: true }); return; }
    this.rep.interrumpir();
    this._pensar('Leyendo la imagen');
    enviar({ type: 'RESOLVER_IMAGEN', imagen: b64, tipo, nota, texto, nombre, modelo: ajustes.modelo, sin_saltos: true });
  }

  /** Problema de un PDF que no está en el banco: recorte + solución oficial expandida sin saltos. */
  documento(r) {
    if (!conectado()) { aviso('El motor se está reconectando; el recorte quedó en la hoja.', { mal: true }); return; }
    this.rep.interrumpir();
    this._pensar(`Resolviendo ${r.etiqueta || 'el problema'} sin saltos`);
    enviar({ type: 'RESOLVER_DOCUMENTO', doc: r.doc, clave: r.clave, modelo: ajustes.modelo, sin_saltos: true });
  }

  _pensar(q) {
    this._fin();
    this.acciones.pizarra();
    let v;
    if (this.rep.lec && !this.rep.lec.libre) v = this.rep.aclaracion(q.length > 90 ? `${q.slice(0, 88)}…` : q, 'pregunta');
    else v = this.tablero.nuevoPaso({ n: 0, titulo: q.length > 90 ? `${q.slice(0, 88)}…` : q, clase: 'aclaracion pregunta' });
    const d = document.createElement('p');
    d.className = 'pensando';
    d.setAttribute('role', 'status');
    d.append(document.createElement('i'), document.createTextNode('Pensando'));
    const c = document.createElement('button');
    c.type = 'button';
    c.className = 'btn btn-suave';
    c.textContent = 'Cancelar';
    c.addEventListener('click', () => { enviar({ type: 'CLIENT_INTERRUPT' }, { encolar: false }); this._fin(); });
    v.cuerpo.append(d, c);
    const t0 = performance.now();
    const reloj = setInterval(() => {
      const s = Math.round((performance.now() - t0) / 1000);
      d.lastChild.textContent = s > 25 ? `Pensando, ${s} s (la nube está lenta)` : `Pensando, ${s} s`;
    }, 1000);
    this.pensando = { v, d, c, reloj };
    this.tablero.seguir(v.el);
    this.vistaRespuesta = v;
  }

  _fin(conservar = false) {
    if (!this.pensando) return;
    clearInterval(this.pensando.reloj);
    this.pensando.d.remove();
    this.pensando.c.remove();
    if (!conservar && !this.pensando.v.cuerpo.childElementCount) this.tablero.quitarPaso(this.pensando.v);
    this.pensando = null;
  }

  _estado(m) {
    if (m.state === 'INTERRUPTED') this._fin();
  }

  /** Respuesta corta de la IA: se cuenta como aclaración, con la misma sincronía. */
  _plan(m) {
    const mini = planAMini(m.beat || {}, m.meta || {});
    if (!mini) { this._fin(); return; }
    const v = this.pensando ? this.pensando.v : null;
    this._fin(true);
    this.cola = this.cola.then(() => this.rep.contar(mini, mini.titulo || 'Respuesta', 'pregunta', v)).catch((e) => console.error('plan', e));
  }

  /** Lección completa armada por la IA (imagen o problema nuevo), ya recalculada por el servidor. */
  _leccion(m) {
    this._fin();
    if (!m.leccion || !m.leccion.pasos || !m.leccion.pasos.length) { aviso('No pude armar la solución de eso.', { mal: true }); return; }
    this.acciones.abrirSuelta(m.leccion);
  }

  callar() {
    this._fin();
    enviar({ type: 'CLIENT_INTERRUPT' }, { encolar: false });
  }

  alternarVoz() { this.viva.alternar(); }

  reportar(motivo) {
    enviar({
      type: 'USER_REPORT_INCIDENT', trigger: 'manual_bug_mark', note: motivo,
      ui_state: { leccion: this.rep.lec && this.rep.lec.id, paso: this.rep.i, estado: this.rep.estado, zoom: this.tablero.zoom, ventana: [innerWidth, innerHeight, devicePixelRatio] },
    });
  }
}
