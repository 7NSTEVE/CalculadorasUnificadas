/**
 * Modo «Hazlo tú sin ver» (Recuperación Activa Ciega) contra variantes deterministas de Python.
 *
 * Elimina la ilusión de competencia: genera el mismo arquetipo con números nuevos, oculta la
 * solución, ofrece una escalera gradual de 3 pistas (Fórmula → Planteo → Solución completa) y
 * verifica cada inciso contra `/api/practica/verificar` alimentando FSRS-5 (`blind: true`).
 */

import { TintaTexto } from '../mate/tinta.js';
import { pedir } from '../nucleo/api.js';
import { aviso } from './avisos.js';

export class PracticaCiega {
  constructor({ acciones }) {
    this.acciones = acciones;
    this.dlg = null;
  }

  async abrir(idOArquetipo) {
    let lec;
    try {
      lec = await pedir(`/api/banco/variante/${encodeURIComponent(idOArquetipo)}`, { tiempo: 15000 });
    } catch {
      aviso('No pude generar una variante nueva ahora.', { mal: true });
      return;
    }
    this._mostrarDialogo(lec, idOArquetipo);
  }

  _mostrarDialogo(lec, origenId) {
    if (this.dlg) this.dlg.remove();
    const dlg = document.createElement('dialog');
    dlg.className = 'visor visor-ciega';
    dlg.setAttribute('aria-label', 'Práctica sin ver');

    const cab = document.createElement('div');
    cab.className = 'visor-form-cab';
    const tit = document.createElement('strong');
    tit.textContent = `Hazlo tú sin ver · ${lec.titulo.replace(/^Variante ·\s*/i, '')}`;
    const accCab = document.createElement('div');
    accCab.className = 'ciega-cab-acc';

    const btnOtra = document.createElement('button');
    btnOtra.type = 'button';
    btnOtra.className = 'btn btn-suave';
    btnOtra.textContent = 'Otros números';
    btnOtra.addEventListener('click', () => this.abrir(origenId));

    const btnPiz = document.createElement('button');
    btnPiz.type = 'button';
    btnPiz.className = 'btn btn-suave';
    btnPiz.textContent = 'Ver solución paso a paso';
    btnPiz.addEventListener('click', () => {
      dlg.close();
      this.acciones.abrirSuelta(lec);
    });

    const cerrar = document.createElement('button');
    cerrar.type = 'button';
    cerrar.className = 'btn';
    cerrar.textContent = 'Cerrar';
    cerrar.addEventListener('click', () => dlg.close());

    accCab.append(btnOtra, btnPiz, cerrar);
    cab.append(tit, accCab);

    const cuerpo = document.createElement('div');
    cuerpo.className = 'ciega-cuerpo';

    const nota = document.createElement('p');
    nota.className = 'ciega-nota';
    nota.textContent = 'Inténtalo primero con tu hoja oficial y la calculadora. Acertar 2 veces sin ver marca este tipo como dominado.';
    cuerpo.appendChild(nota);

    const enunBox = document.createElement('div');
    enunBox.className = 'ciega-enun';
    const tt = new TintaTexto(lec.enunciado || '', { px: 19, clase: 'sin-mano' });
    tt.progreso(1);
    enunBox.appendChild(tt.el);
    cuerpo.appendChild(enunBox);

    // Respuestas evaluables
    const resps = (lec.respuestas || []).filter((r) => r.valor !== null && r.valor !== undefined);
    const lista = document.createElement('div');
    lista.className = 'ciega-lista';

    const formulas = (lec.formulario_mapa?.celdas || []).filter((c) => !c.es_constante).map((c) => c.nombre);
    const planteos = (lec.pasos || []).map((p) => `${p.titulo}: ${p.porque || ''}`).filter(Boolean);

    resps.forEach((r, idx) => {
      const fila = document.createElement('div');
      fila.className = 'ciega-item';

      const lbl = document.createElement('label');
      lbl.className = 'ciega-lbl';
      lbl.textContent = `Inciso ${r.inciso || (idx + 1)}`;

      const inp = document.createElement('input');
      inp.type = 'text';
      inp.inputMode = 'decimal';
      inp.placeholder = 'Escribe tu valor numérico';
      inp.className = 'ciega-inp';

      const u = document.createElement('span');
      u.className = 'ciega-u';
      u.textContent = r.unidad || '';

      const btnRev = document.createElement('button');
      btnRev.type = 'button';
      btnRev.className = 'btn btn-principal';
      btnRev.textContent = 'Comprobar';

      const estado = document.createElement('span');
      estado.className = 'ciega-estado';
      estado.setAttribute('role', 'status');

      const comprobar = async () => {
        if (!inp.value.trim()) {
          estado.className = 'ciega-estado mal';
          estado.textContent = 'Escribe un número antes de comprobar.';
          return;
        }
        btnRev.disabled = true;
        try {
          const res = await pedir('/api/practica/verificar', {
            metodo: 'POST',
            cuerpo: {
              id: `${origenId}:${r.inciso || idx}`,
              titulo: `${lec.titulo} (${r.inciso || idx})`,
              esperado: r.valor,
              respuesta: inp.value.trim(),
              tol: r.tol || 0.03,
              ciega: true,
            },
          });
          estado.className = `ciega-estado ${res.ok ? 'bien' : 'mal'}`;
          estado.textContent = res.nota || (res.ok ? '¡Correcto!' : 'Todavía no coincide.');
        } catch {
          estado.className = 'ciega-estado mal';
          estado.textContent = 'No pude verificar ahora.';
        } finally {
          btnRev.disabled = false;
        }
      };

      btnRev.addEventListener('click', comprobar);
      inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') comprobar(); });

      fila.append(lbl, inp, u, btnRev, estado);
      lista.appendChild(fila);
    });
    cuerpo.appendChild(lista);

    // Escalera gradual de 3 pistas (nunca te deja bloqueado)
    let nivelPista = 0;
    const pistasBox = document.createElement('div');
    pistasBox.className = 'ciega-pistas';
    const btnPista = document.createElement('button');
    btnPista.type = 'button';
    btnPista.className = 'btn btn-suave';
    btnPista.textContent = 'Pista 1: ¿Qué fórmula de la hoja usar?';
    const txtPista = document.createElement('div');
    txtPista.className = 'ciega-pista-txt';
    btnPista.addEventListener('click', () => {
      nivelPista += 1;
      if (nivelPista === 1) {
        txtPista.textContent = formulas.length
          ? `Fórmulas clave de la hoja: ${formulas.join(' · ')}.`
          : 'Identifica primero qué datos tienes y convierte todo al Sistema Internacional.';
        btnPista.textContent = 'Pista 2: ¿Cómo se plantea?';
      } else if (nivelPista === 2) {
        txtPista.textContent = planteos.slice(0, 3).join(' | ') || 'Despeja la incógnita paso a paso sin saltarte álgebra.';
        btnPista.textContent = 'Pista 3: Ver respuestas numéricas';
      } else {
        txtPista.textContent = `Valores esperados: ${resps.map((x) => `${x.inciso}: ${String(x.valor).replace('.', ',')} ${x.unidad || ''}`).join(' · ')}`;
        btnPista.hidden = true;
      }
    });
    pistasBox.append(btnPista, txtPista);
    cuerpo.appendChild(pistasBox);

    dlg.append(cab, cuerpo);
    dlg.addEventListener('click', (e) => { if (e.target === dlg) dlg.close(); });
    dlg.addEventListener('close', () => { dlg.remove(); if (this.dlg === dlg) this.dlg = null; });
    document.body.appendChild(dlg);
    this.dlg = dlg;
    dlg.showModal();
  }
}
