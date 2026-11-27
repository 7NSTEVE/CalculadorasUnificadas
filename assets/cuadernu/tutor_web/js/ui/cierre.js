/** Preguntas rápidas, respuestas finales y el mini ejercicio "Te toca". */

import { TintaMate, TintaTexto } from '../mate/tinta.js';
import { bus, pedir } from '../nucleo/api.js';
import { evaluador } from '../visuales/base.js';
import { siguienteDe, banco } from './banco.js';

/** Acepta "4,07", "4.07e3", "1,2x10^-3", "3/5", "-12 %" -> número. */
export function leerNumero(txt) {
  let s = String(txt || '').trim().toLowerCase().replace(/\s+/g, '').replace(/−/g, '-');
  s = s.replace(/(?:×|x|\*)10\^?\(?([+-]?\d+)\)?/g, 'e$1').replace(/(\d)10\^/g, '$1e');
  s = s.replace(/[a-zµ°%/·³²]+$/i, (u) => (/^e$/.test(u) ? u : ''));
  if (/^\d{1,3}(?:\.\d{3})+(?:,\d+)?$/.test(s)) s = s.replace(/\./g, '');
  s = s.replace(',', '.');
  if (/^[+-]?(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?$/.test(s)) return Number(s);
  if (/^[0-9+\-*/().e^]+$/.test(s)) { try { const v = evaluador(s)(0); return Number.isFinite(v) ? v : NaN; } catch { return NaN; } }
  return NaN;
}

function texto(txt, opt = {}) {
  const t = new TintaTexto(txt, opt);
  t.progreso(1);
  return t.el;
}

/** Pregunta rápida al terminar un paso. Responder bien no es requisito para seguir. */
export function ponerPregunta(rep, paso) {
  const q = paso.pregunta;
  if (!q) return null;
  const v = rep.aclaracion('Piensa un segundo', 'pregunta');
  const tab = rep.tablero;
  v.cuerpo.appendChild(texto(q.texto, { px: tab.px * 0.62 }));
  const fila = document.createElement('div');
  fila.className = 'quiz';
  const exp = document.createElement('div');
  exp.hidden = true;
  q.opciones.forEach((o, i) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.appendChild(texto(o, { px: tab.px * 0.54, etiqueta: 'span' }));
    b.addEventListener('click', () => {
      const bien = i === q.correcta;
      b.classList.add(bien ? 'bien' : 'mal');
      if (bien) fila.querySelectorAll('button').forEach((x) => { x.disabled = true; });
      else b.disabled = true;
      exp.replaceChildren(texto(`${bien ? '**Sí.** ' : '**Casi.** '}${bien ? q.explica : 'Inténtalo otra vez: piensa en lo que acabamos de escribir.'}`, { px: tab.px * 0.58 }));
      exp.hidden = false;
      tab.programarRecolocar();
      if (bien) bus.emit('pregunta:bien', paso);
    });
    fila.appendChild(b);
  });
  v.cuerpo.append(fila, exp);
  tab.seguir(v.el);
  return v;
}

/** Bloque final de la lección. */
export function ponerCierre(rep, lec, acciones) {
  const tab = rep.tablero;
  const v = tab.nuevoPaso({ n: 0, titulo: lec.tipo === 'teoria' ? 'Lo que hay que recordar' : 'Respuestas', clase: 'cierre' });
  if (lec.tipo === 'seleccion' && lec.correcta) {
    v.cuerpo.appendChild(texto(`Opción correcta: **${lec.correcta}**`, { px: tab.px * 0.7 }));
  }
  // Respuestas agrupadas por inciso: la letra se escribe una sola vez («b) agua…» y debajo «vapor…»)
  let grupo = null;
  let letraGrupo = '';
  for (const r of lec.respuestas || []) {
    const letra = r.inciso ? String(r.inciso).trim() : '';
    const m = new TintaMate(r.tex, { px: tab.px * 0.92, color: 'verde', maxAncho: tab.anchoUtil(v.cuerpo) - (letra ? tab.px : 0) });
    tab.agregar(m, { paso: v });
    m.progreso(1);
    if (!letra) { grupo = null; letraGrupo = ''; continue; }
    if (!grupo || letra !== letraGrupo) {
      const fila = document.createElement('div');
      fila.className = 'resp-inciso';
      const marca = texto(`**${letra})**`, { px: tab.px * 0.62, etiqueta: 'span' });
      marca.classList.add('resp-letra');
      grupo = document.createElement('div');
      grupo.className = 'resp-valores';
      fila.append(marca, grupo);
      v.cuerpo.insertBefore(fila, m.el);
      letraGrupo = letra;
    }
    grupo.appendChild(m.el);
  }
  if (lec.cotejo) {
    const s = document.createElement('p');
    s.className = `sello${lec.cotejo.estado === 'difiere' ? ' difiere' : ''}`;
    s.textContent = lec.cotejo.texto;
    v.cuerpo.appendChild(s);
  }
  if (lec.practica) ponerPractica(v.cuerpo, lec, tab);
  const fila = document.createElement('div');
  fila.className = 'fila-botones';
  // «Siguiente» respeta el plan de donde se vino (Preparación) y no lleva a temas fuera del corte
  let sig = null;
  if (acciones.siguiente) {
    sig = acciones.siguiente(lec);
  } else {
    const id = siguienteDe(lec.id);
    if (id && banco.mapa.has(id)) {
      sig = { texto: `Siguiente: ${banco.mapa.get(id).titulo.replace(/\$[^$]*\$/g, '').slice(0, 60)}`, abrir: () => acciones.abrir(id) };
    }
  }
  if (sig) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn btn-principal';
    b.textContent = sig.texto;
    b.addEventListener('click', sig.abrir);
    fila.appendChild(b);
  }
  if (acciones.variante && lec.tipo !== 'teoria') {
    const vbtn = document.createElement('button');
    vbtn.type = 'button';
    vbtn.className = 'btn btn-suave';
    vbtn.textContent = 'Practicar con otros números';
    vbtn.addEventListener('click', () => acciones.variante(lec.origenVariante || lec.id));
    fila.appendChild(vbtn);
  }
  const otra = document.createElement('button');
  otra.type = 'button';
  otra.className = 'btn';
  otra.textContent = 'Verla otra vez';
  otra.addEventListener('click', () => rep.irA(0));
  fila.appendChild(otra);
  v.cuerpo.appendChild(fila);
  if (lec.auditoria_fisica && Array.isArray(lec.auditoria_fisica.niveles)) v.cuerpo.appendChild(detalleRevisado(lec.auditoria_fisica));
  tab.seguir(v.el);
  return v;
}

/**
 * Las comprobaciones automáticas de la solución, como un detalle discreto al pie: «Revisado».
 * El conteo técnico («7 de 7 comprobaciones… · 0 alertas físicas») queda adentro, para quien lo abra.
 */
function detalleRevisado(af) {
  const n = Array.isArray(af.alertas) ? af.alertas.length : 0;
  const det = document.createElement('details');
  det.className = `auditoria-fisica${n ? ' con-alerta' : ''}`;
  const sum = document.createElement('summary');
  sum.textContent = n ? `Revisado · ${n === 1 ? '1 aviso' : `${n} avisos`}` : 'Revisado';
  det.appendChild(sum);
  if (af.resumen) {
    const p = document.createElement('p');
    p.className = 'auditoria-resumen';
    p.textContent = af.resumen;
    det.appendChild(p);
  }
  const ul = document.createElement('ul');
  for (const nv of af.niveles) {
    const li = document.createElement('li');
    li.className = `niv-${nv.estado}`;
    const tag = nv.estado === 'comprobado' ? 'OK' : nv.estado === 'alerta' ? 'ALERTA' : '—';
    const b = document.createElement('strong');
    b.textContent = `${nv.nivel}. ${nv.nombre} (${tag}): `;
    li.append(b, document.createTextNode(nv.detalle || ''));
    ul.appendChild(li);
  }
  det.appendChild(ul);
  return det;
}

export function ponerPractica(cuerpo, lec, tab) {
  const p = lec.practica;
  const h = document.createElement('div');
  h.appendChild(texto('**Te toca.** ' + p.texto, { px: tab.px * 0.6 }));
  const fila = document.createElement('div');
  fila.className = 'practica';
  const inp = document.createElement('input');
  inp.type = 'text';
  inp.inputMode = 'decimal';
  inp.placeholder = 'Tu respuesta';
  inp.setAttribute('aria-label', `Respuesta en ${p.unidad || 'número'}`);
  const u = document.createElement('span');
  u.textContent = p.unidad || '';
  const b = document.createElement('button');
  b.type = 'button';
  b.className = 'btn';
  b.textContent = 'Revisar';
  const r = document.createElement('span');
  r.className = 'resp';
  r.setAttribute('role', 'status');
  const ver = document.createElement('button');
  ver.type = 'button';
  ver.className = 'btn btn-suave';
  ver.textContent = 'Ver la pista';
  let nivel = 0;
  const sol = document.createElement('div');
  ver.addEventListener('click', () => {
    nivel += 1;
    if (nivel === 1 && p.pista) { sol.replaceChildren(texto(p.pista, { px: tab.px * 0.56 })); ver.textContent = 'Ver la solución'; return; }
    sol.replaceChildren(texto(p.solucion || `La respuesta es ${String(p.valor).replace('.', ',')} ${p.unidad}.`, { px: tab.px * 0.56 }));
    ver.hidden = true;
  });
  const revisar = () => {
    const x = leerNumero(inp.value);
    if (!Number.isFinite(x)) { r.className = 'resp mal'; r.textContent = 'Escribe solo el número'; return; }
    const tol = Math.max(Math.abs(p.valor) * (p.tol || 0.02), 1e-9);
    const bien = Math.abs(x - p.valor) <= tol;
    r.className = `resp ${bien ? 'bien' : 'mal'}`;
    if (bien) r.textContent = '¡Correcto!';
    else if (Math.abs(x + p.valor) <= tol) r.textContent = 'El número está bien; revisa el signo';
    else if (p.valor && Math.abs(Math.log10(Math.abs(x / p.valor))) > 0.95 && Math.abs((Math.log10(Math.abs(x / p.valor)) % 1 + 1) % 1) < 0.02) r.textContent = 'Parece un problema de unidades (potencia de diez)';
    else r.textContent = 'Todavía no';
    pedir('/api/progreso', { metodo: 'POST', cuerpo: { tipo: 'practica', id: lec.id, bien } }).catch(() => {});
  };
  b.addEventListener('click', revisar);
  inp.addEventListener('keydown', (e) => { if (e.key === 'Enter') revisar(); });
  fila.append(inp, u, b, r, ver);
  cuerpo.append(h, fila, sol);
}
