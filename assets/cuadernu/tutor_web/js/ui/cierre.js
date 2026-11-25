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
  for (const r of lec.respuestas || []) {
    const fila = document.createElement('div');
    if (r.inciso) fila.appendChild(texto(`**${r.inciso}**`, { px: tab.px * 0.56 }));
    const m = new TintaMate(r.tex, { px: tab.px * 0.92, color: 'verde', maxAncho: tab.anchoUtil(v.cuerpo) });
    tab.agregar(m, { paso: v });
    m.progreso(1);
    if (r.inciso) v.cuerpo.insertBefore(fila, m.el);
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
  const sig = siguienteDe(lec.id);
  if (sig && banco.mapa.has(sig)) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn btn-principal';
    b.textContent = `Siguiente: ${banco.mapa.get(sig).titulo.replace(/\$[^$]*\$/g, '').slice(0, 60)}`;
    b.addEventListener('click', () => acciones.abrir(sig));
    fila.appendChild(b);
  }
  const otra = document.createElement('button');
  otra.type = 'button';
  otra.className = 'btn';
  otra.textContent = 'Verla otra vez';
  otra.addEventListener('click', () => rep.irA(0));
  fila.appendChild(otra);
  v.cuerpo.appendChild(fila);
  tab.seguir(v.el);
  return v;
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
