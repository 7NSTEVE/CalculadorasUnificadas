/**
 * Diagnóstico de bases (1 pregunta a la vez con refuerzo inmediato) y Minipruebas A/B
 * cronometradas de 60 min (30 pts: 3 selecciones × 2 pts + 3 desarrollos × 8 pts con rúbrica 2/4/2),
 * conectadas al motor local `/api/intensivo` y al progreso persistente.
 */

import { TintaTexto } from '../mate/tinta.js';
import { pedir, urlApi } from '../nucleo/api.js';
import { aviso } from './avisos.js';

function texto(txt, px = 17) {
  const t = new TintaTexto(txt || '', { px, clase: 'sin-mano' });
  t.progreso(1);
  return t.el;
}

export class PanelIntensivo {
  constructor({ acciones }) {
    this.acciones = acciones;
    this.datos = null;
    this.dlg = null;
    this.el = document.createElement('section');
    this.el.className = 'intensivo-panel';
    this.el.setAttribute('aria-label', 'Diagnóstico rápido y Minipruebas de 30 puntos');
  }

  async cargarYPintar() {
    try {
      this.datos = await pedir('/api/intensivo', { tiempo: 15000 });
    } catch {
      this.el.hidden = true;
      return;
    }
    this.el.hidden = false;
    this._pintarResumen();
  }

  _pintarResumen() {
    this.el.replaceChildren();
    const d = this.datos;
    if (!d) return;

    const diag = d.diagnostico || { reactivos: [], refuerzo: null };
    const contestados = diag.reactivos.filter((r) => r.elegida !== undefined).length;
    const aciertos = diag.reactivos.filter((r) => r.elegida !== undefined && r.elegida === r.correcta).length;

    const cab = document.createElement('div');
    cab.className = 'int-cab';
    const tit = document.createElement('strong');
    tit.textContent = 'Comprobación real de dominio (sin mirar soluciones)';
    const sub = document.createElement('small');
    sub.textContent = `Diagnóstico base: ${contestados}/${diag.reactivos.length} (${aciertos} aciertos) · Dos minipruebas reservadas de 60 min sobre 30 puntos (rúbrica 2+4+2).`;
    cab.append(tit, sub);

    const fila = document.createElement('div');
    fila.className = 'int-acc';

    const bDiag = document.createElement('button');
    bDiag.type = 'button';
    bDiag.className = 'btn btn-suave';
    bDiag.dataset.intensivo = 'diagnostico';
    bDiag.textContent = contestados < diag.reactivos.length ? 'Hacer diagnóstico base (5 min)' : 'Revisar diagnóstico base';
    bDiag.addEventListener('click', () => this.abrirDiagnostico());

    const bA = document.createElement('button');
    bA.type = 'button';
    bA.className = 'btn btn-principal';
    bA.dataset.intensivo = 'miniprueba-A';
    bA.textContent = 'Miniprueba A (60 min · 30 pts)';
    bA.addEventListener('click', () => this.abrirMiniprueba('A'));

    const bB = document.createElement('button');
    bB.type = 'button';
    bB.className = 'btn btn-suave';
    bB.dataset.intensivo = 'miniprueba-B';
    bB.textContent = 'Miniprueba B (60 min · 30 pts)';
    bB.addEventListener('click', () => this.abrirMiniprueba('B'));

    fila.append(bDiag, bA, bB);

    if (d.formulario_reposicion) {
      const aForm = document.createElement('a');
      aForm.className = 'btn btn-suave';
      aForm.href = urlApi(d.formulario_reposicion);
      aForm.target = '_blank';
      aForm.rel = 'noopener';
      aForm.textContent = 'Abrir formulario oficial de reposición';
      fila.appendChild(aForm);
    }

    this.el.append(cab, fila);
  }

  abrirDiagnostico() {
    if (!this.datos) return;
    if (this.dlg) this.dlg.remove();
    const dlg = document.createElement('dialog');
    dlg.className = 'visor visor-ciega';
    dlg.setAttribute('aria-label', 'Diagnóstico rápido de bases');

    const cab = document.createElement('div');
    cab.className = 'visor-form-cab';
    const tit = document.createElement('strong');
    tit.textContent = 'Diagnóstico base · 1 pregunta a la vez';
    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'btn';
    cerrar.textContent = 'Cerrar';
    cerrar.addEventListener('click', () => dlg.close());
    cab.append(tit, cerrar);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'ciega-cuerpo';

    let idx = Math.max(0, (this.datos.diagnostico.reactivos || []).findIndex((r) => r.elegida === undefined));
    const pintarPaso = () => {
      cuerpo.replaceChildren();
      const lista = this.datos.diagnostico.reactivos || [];
      const ref = this.datos.diagnostico.refuerzo;
      const esRef = idx >= lista.length;
      const q = esRef ? ref : lista[idx];
      if (!q) {
        const fin = document.createElement('p');
        fin.textContent = 'Diagnóstico completado. Ya puedes continuar con tu siguiente bloque del Sprint de 6 días.';
        cuerpo.appendChild(fin);
        return;
      }

      const badge = document.createElement('small');
      badge.className = 'ciega-nota';
      badge.textContent = esRef ? 'Refuerzo corto sobre la primera dificultad detectada' : `Pregunta ${idx + 1} de ${lista.length}`;
      cuerpo.appendChild(badge);

      if (esRef && q.ejemplo) {
        const ej = document.createElement('div');
        ej.className = 'ciega-enun';
        ej.append(texto(`Ejemplo guía: ${q.ejemplo}`, 16));
        cuerpo.appendChild(ej);
      }

      const preg = document.createElement('div');
      preg.className = 'ciega-enun';
      preg.appendChild(texto(q.pregunta, 19));
      cuerpo.appendChild(preg);

      const ops = document.createElement('div');
      ops.className = 'quiz';
      const feed = document.createElement('div');
      feed.className = 'ciega-pista-txt';

      (q.opciones || []).forEach((opTxt, oi) => {
        const b = document.createElement('button');
        b.type = 'button';
        b.appendChild(texto(opTxt, 16));
        if (q.elegida !== undefined) {
          b.disabled = true;
          if (oi === q.correcta) b.classList.add('bien');
          else if (oi === q.elegida) b.classList.add('mal');
        }
        b.addEventListener('click', async () => {
          try {
            const res = await pedir('/api/progreso', {
              metodo: 'POST',
              cuerpo: { tipo: 'intensivo', evento: 'diagnostico', id: q.id, opcion: oi },
            });
            if (res.diagnostico) this.datos.diagnostico = res.diagnostico;
            this._pintarResumen();
            pintarPaso();
          } catch {
            aviso('No pude guardar esa respuesta.', { mal: true });
          }
        });
        ops.appendChild(b);
      });
      cuerpo.appendChild(ops);

      if (q.elegida !== undefined) {
        const bien = q.elegida === q.correcta;
        feed.appendChild(texto(`${bien ? '**Correcto.** ' : '**Ojo.** '}${q.explicacion || ''}`, 16));
        cuerpo.appendChild(feed);
        const nav = document.createElement('div');
        nav.className = 'fila-botones';
        if (idx < lista.length) {
          const bSig = document.createElement('button');
          bSig.type = 'button';
          bSig.className = 'btn btn-principal';
          bSig.textContent = idx + 1 < lista.length ? 'Siguiente pregunta' : 'Ir al refuerzo';
          bSig.addEventListener('click', () => { idx += 1; pintarPaso(); });
          nav.appendChild(bSig);
        }
        cuerpo.appendChild(nav);
      }
    };

    pintarPaso();
    dlg.append(cab, cuerpo);
    dlg.addEventListener('close', () => { dlg.remove(); if (this.dlg === dlg) this.dlg = null; });
    document.body.appendChild(dlg);
    this.dlg = dlg;
    dlg.showModal();
  }

  async abrirMiniprueba(forma = 'A') {
    let ev;
    try {
      ev = await pedir(`/api/intensivo/evaluacion/${encodeURIComponent(forma)}`, { tiempo: 20000 });
    } catch {
      aviso(`No pude abrir la Miniprueba ${forma}.`, { mal: true });
      return;
    }
    this._mostrarMiniprueba(ev);
  }

  _mostrarMiniprueba(ev) {
    if (this.dlg) this.dlg.remove();
    const dlg = document.createElement('dialog');
    dlg.className = 'visor visor-ciega mini-prueba';
    dlg.dataset.miniprueba = ev.id;
    dlg.setAttribute('aria-label', `Miniprueba ${ev.id}`);

    const cab = document.createElement('div');
    cab.className = 'visor-form-cab';
    const tit = document.createElement('strong');
    tit.textContent = `Miniprueba ${ev.id} · 60 min · ${ev.puntos}/${ev.maximo} pts`;
    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'btn';
    cerrar.textContent = 'Cerrar';
    cerrar.addEventListener('click', () => dlg.close());
    cab.append(tit, cerrar);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'ciega-cuerpo';

    const avisoModo = document.createElement('p');
    avisoModo.className = 'ciega-nota';
    avisoModo.textContent = ev.entregada
      ? `Entregada. Puntaje actual: ${ev.puntos} de ${ev.maximo} puntos (selecciones 2 pts; desarrollos 2 planteo + 4 desarrollo + 2 resultado/unidades).`
      : 'Soluciones y pistas cerradas hasta entregar. Usa las hojas oficiales y tu calculadora.';
    cuerpo.appendChild(avisoModo);

    if (Array.isArray(ev.vistas) && ev.vistas.length) {
      const av = document.createElement('p');
      av.className = 'ciega-nota';
      av.textContent = `Nota: ya habías abierto ${ev.vistas.length} de estos ejercicios antes, por lo que cuentan como repaso.`;
      cuerpo.appendChild(av);
    }

    (ev.items || []).forEach((q, idx) => {
      const card = document.createElement('section');
      card.className = 'seccion';
      const h = document.createElement('h3');
      h.textContent = `${idx + 1}. Parcial ${q.parcial} · ${q.inciso ? `Desarrollo inciso ${q.inciso.toUpperCase()} (8 pts)` : 'Selección (2 pts)'}`;
      card.appendChild(h);
      card.appendChild(texto(q.enunciado, 17));
      if (q.pide) {
        const pd = document.createElement('p');
        pd.className = 'ciega-nota';
        pd.appendChild(texto(`**Consigna:** ${q.pide}`, 15));
        card.appendChild(pd);
      }

      if (!q.inciso && q.opciones) {
        const ops = document.createElement('div');
        ops.className = 'opciones';
        for (const [L, val] of Object.entries(q.opciones)) {
          const b = document.createElement('button');
          b.type = 'button';
          b.className = 'opcion';
          b.style.textAlign = 'left';
          b.style.width = '100%';
          b.disabled = Boolean(ev.entregada);
          b.setAttribute('aria-pressed', String(q.entrada === L));
          b.innerHTML = `<span class="opcion-letra">${L}</span>`;
          b.appendChild(texto(val, 15));
          b.addEventListener('click', async () => {
            q.entrada = L;
            ops.querySelectorAll('.opcion').forEach((x) => x.setAttribute('aria-pressed', 'false'));
            b.setAttribute('aria-pressed', 'true');
            await pedir('/api/progreso', {
              metodo: 'POST',
              cuerpo: { tipo: 'intensivo', evento: 'respuesta', id: ev.id, i: idx, entrada: L },
            }).catch(() => {});
          });
          ops.appendChild(b);
        }
        card.appendChild(ops);
        if (ev.entregada && q.correcta) {
          const ok = q.entrada === q.correcta;
          const st = document.createElement('p');
          st.className = `sim-feed ${ok ? 'bien' : 'mal'}`;
          st.textContent = ok ? `Correcta (${q.correcta}) · 2/2 pts` : `Correcta: ${q.correcta} (marcaste ${q.entrada || '—'}) · 0/2 pts`;
          card.appendChild(st);
        }
      } else {
        const ta = document.createElement('textarea');
        ta.rows = 3;
        ta.className = 'ciega-inp';
        ta.style.width = '100%';
        ta.value = q.entrada || '';
        ta.disabled = Boolean(ev.entregada);
        ta.placeholder = 'Escribe tu planteo, resultado numérico, signo y unidades…';
        ta.addEventListener('change', () => {
          pedir('/api/progreso', {
            metodo: 'POST',
            cuerpo: { tipo: 'intensivo', evento: 'respuesta', id: ev.id, i: idx, entrada: ta.value },
          }).catch(() => {});
        });
        card.appendChild(ta);

        if (ev.entregada && q.rubrica) {
          const rub = document.createElement('div');
          rub.className = 'ciega-enun';
          rub.append(
            texto(`**Planteo (0–2):** ${q.rubrica.planteo}`, 15),
            texto(`**Desarrollo (0–4):** ${q.rubrica.desarrollo}`, 15),
            texto(`**Resultado esperado (0–2):** ${(q.rubrica.resultado || []).join(' · ')}`, 15),
          );
          const vals = Array.isArray(q.criterios) ? [...q.criterios] : [0, 0, 0];
          const filaRub = document.createElement('div');
          filaRub.className = 'rubrica-fila';
          const selects = [['Planteo', 2], ['Desarrollo', 4], ['Resultado/u', 2]].map(([lbl, max], ci) => {
            const lab = document.createElement('label');
            lab.textContent = `${lbl}: `;
            const sel = document.createElement('select');
            for (let v = 0; v <= max; v += 1) {
              const o = document.createElement('option');
              o.value = String(v); o.textContent = `${v}/${max}`; o.selected = vals[ci] === v;
              sel.appendChild(o);
            }
            sel.addEventListener('change', async () => {
              vals[ci] = Number(sel.value);
              await pedir('/api/progreso', {
                metodo: 'POST',
                cuerpo: { tipo: 'intensivo', evento: 'rubrica', id: ev.id, i: idx, criterios: vals },
              }).catch(() => {});
              this.abrirMiniprueba(ev.id);
            });
            lab.appendChild(sel);
            filaRub.appendChild(lab);
            return sel;
          });
          void selects;
          const bAbrir = document.createElement('button');
          bAbrir.type = 'button';
          bAbrir.className = 'btn btn-suave';
          bAbrir.textContent = 'Abrir solución en pizarra 3D';
          bAbrir.addEventListener('click', () => { dlg.close(); this.acciones.abrir(q.id, { corte: '20.5' }); });
          rub.append(filaRub, bAbrir);
          card.appendChild(rub);
        }
      }
      cuerpo.appendChild(card);
    });

    if (!ev.entregada) {
      const bEnt = document.createElement('button');
      bEnt.type = 'button';
      bEnt.className = 'btn btn-principal';
      bEnt.textContent = `Entregar Miniprueba ${ev.id} y abrir rúbrica 2+4+2`;
      bEnt.addEventListener('click', async () => {
        await pedir('/api/progreso', {
          metodo: 'POST',
          cuerpo: { tipo: 'intensivo', evento: 'entregar', id: ev.id },
        });
        this.abrirMiniprueba(ev.id);
      });
      cuerpo.appendChild(bEnt);
    }

    dlg.append(cab, cuerpo);
    dlg.addEventListener('close', () => { dlg.remove(); if (this.dlg === dlg) this.dlg = null; });
    document.body.appendChild(dlg);
    this.dlg = dlg;
    dlg.showModal();
  }
}
