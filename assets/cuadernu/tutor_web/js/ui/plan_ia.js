/**
 * Convierte lo que devuelve la IA (texto con fórmulas) en mini pasos del reproductor y limpia la
 * narración: la voz nunca debe leer LaTeX.
 */

import { miniPaso } from '../leccion/reproductor.js';

const GRIEGAS = {
  alpha: 'alfa', beta: 'beta', gamma: 'gama', delta: 'delta', epsilon: 'épsilon', varepsilon: 'épsilon', eta: 'eta', theta: 'teta',
  kappa: 'kapa', lambda: 'lambda', mu: 'mu', nu: 'nu', pi: 'pi', rho: 'ro', sigma: 'sigma', tau: 'tau', phi: 'fi', varphi: 'fi',
  chi: 'ji', psi: 'psi', omega: 'omega', Gamma: 'gama', Delta: 'delta', Theta: 'teta', Lambda: 'lambda', Sigma: 'sigma', Phi: 'fi', Psi: 'psi', Omega: 'omega',
};

const PALABRAS = {
  cdot: ' por ', times: ' por ', div: ' entre ', pm: ' más o menos ', le: ' menor o igual que ', leq: ' menor o igual que ',
  ge: ' mayor o igual que ', geq: ' mayor o igual que ', ne: ' distinto de ', neq: ' distinto de ', approx: ' aproximadamente ',
  simeq: ' aproximadamente ', propto: ' proporcional a ', to: ' tiende a ', rightarrow: ' lleva a ', Rightarrow: ' entonces ', implies: ' entonces ',
  int: ' integral de ', oint: ' integral cerrada de ', sum: ' suma de ', prod: ' producto de ', infty: ' infinito ', partial: ' derivada parcial ',
  ln: ' logaritmo natural de ', log: ' logaritmo de ', sin: ' seno de ', cos: ' coseno de ', tan: ' tangente de ', exp: ' exponencial de ',
  lim: ' límite ', degree: ' grados ', circ: ' grados ', quad: ' ', qquad: ' ', dots: ' ', ldots: ' ', cdots: ' ', left: ' ', right: ' ',
  displaystyle: ' ', text: '', mathrm: '', mathbf: '', boxed: '', vec: ' vector ', hat: '', bar: '', dot: '', nabla: ' nabla ', forall: ' para todo ', exists: ' existe ', in: ' pertenece a ',
};

/** Lee una fórmula en español corrido. No pretende ser elegante: solo que nunca suene a código. */
export function texAVoz(tex) {
  let s = String(tex || '');
  s = s.replace(/\\\\|&/g, ' ').replace(/\\[,;:!]|~/g, ' ').replace(/\{,\}/g, ',').replace(/\\%/g, ' por ciento ');
  for (let k = 0; k < 6; k += 1) {
    s = s.replace(/\\[dt]?frac\s*\{([^{}]*)\}\s*\{([^{}]*)\}/g, ' $1 sobre $2 ');
    s = s.replace(/\\[dt]?frac\s*(\w)\s*(\w)/g, ' $1 sobre $2 ');
    s = s.replace(/\\sqrt\s*\[([^\]]*)\]\s*\{([^{}]*)\}/g, ' raíz $1 de $2 ').replace(/\\sqrt\s*\{([^{}]*)\}/g, ' raíz de $1 ');
    s = s.replace(/\^\s*\{?\\circ\}?\s*(?:\\text\{C\}|C)/g, ' grados Celsius ').replace(/\^\s*\{?\\circ\}?/g, ' grados ');
    s = s.replace(/\^\s*\{?2\}?(?![0-9])/g, ' al cuadrado ').replace(/\^\s*\{?3\}?(?![0-9])/g, ' al cubo ');
    s = s.replace(/\^\s*\{([^{}]*)\}/g, ' a la $1 ').replace(/\^\s*(\w)/g, ' a la $1 ');
    s = s.replace(/_\s*\{([^{}]*)\}/g, ' $1 ').replace(/_\s*(\w)/g, ' $1 ');
    s = s.replace(/\\(?:text|mathrm|mathbf|boxed|operatorname)\s*\{([^{}]*)\}/g, ' $1 ');
  }
  s = s.replace(/\\([A-Za-z]+)/g, (_, w) => (w in GRIEGAS ? ` ${GRIEGAS[w]} ` : w in PALABRAS ? PALABRAS[w] : ' '));
  s = s.replace(/[{}$\\]/g, ' ').replace(/=/g, ' es igual a ').replace(/\+/g, ' más ').replace(/(?<=[\s\w)])[-−](?=[\s\w(])/g, ' menos ').replace(/^[-−]/, 'menos ')
    .replace(/\//g, ' entre ').replace(/\*/g, ' por ').replace(/</g, ' menor que ').replace(/>/g, ' mayor que ').replace(/[()[\]|]/g, ' ');
  return s.replace(/\s+/g, ' ').trim();
}

/** Texto con $fórmulas$ y markdown -> texto hablable. */
export function limpiarHabla(txt) {
  let s = String(txt || '');
  s = s.replace(/<[^>]+>/g, ' ');
  s = s.replace(/\$\$([\s\S]+?)\$\$/g, (_, m) => ` ${texAVoz(m)} `).replace(/\$([^$]+)\$/g, (_, m) => ` ${texAVoz(m)} `);
  s = s.replace(/\\\(([\s\S]+?)\\\)/g, (_, m) => ` ${texAVoz(m)} `).replace(/\\\[([\s\S]+?)\\\]/g, (_, m) => ` ${texAVoz(m)} `);
  if (/\\[A-Za-z]+|[_^]\{/.test(s)) s = texAVoz(s);
  s = s.replace(/[*_`#>]+/g, ' ').replace(/·/g, ' ').replace(/\s+([.,;:!?])/g, '$1');
  return s.replace(/\s+/g, ' ').trim();
}

/** Asegura que un texto visible lleve su matemática entre $…$. */
export function visible(txt) {
  let s = String(txt || '').replace(/<cue[^>]*>/g, ' ').replace(/\$\$/g, '$').replace(/\\\(|\\\)/g, '$').replace(/\\\[|\\\]/g, '$');
  s = s.replace(/^#+\s*/gm, '').replace(/\s+/g, ' ').trim();
  if (!s.includes('$') && /\\[A-Za-z]+/.test(s)) return /\s[a-záéíóúñ]{3,}\s/i.test(s.replace(/\\[A-Za-z]+/g, '')) ? s : `$${s}$`;
  return s;
}

let cont = 0;
function nid(p) { cont += 1; return `${p}${Date.now().toString(36)}_${cont}`; }

/** Acción de la IA (cualquier formato conocido) -> acciones del reproductor. */
export function accionA(a) {
  const p = a.payload || a;
  const id = nid('ia');
  const col = String(a.color_token || p.color || '').replace(/^accent_/, '');
  const tex = p.latex || p.tex || p.expression || '';
  switch (a.type || a.a) {
    case 'mate': return [{ a: 'mate', tex, id, color: p.color || '', rol: p.rol || '' }];
    case 'texto': return [{ a: 'texto', txt: visible(p.txt), id, clase: p.clase || '' }];
    case 'lista': return [{ a: 'lista', items: (p.items || []).map(visible), id }];
    case 'tabla': return [{ a: 'tabla', cab: (p.cab || []).map(visible), filas: (p.filas || []).map((f) => f.map(visible)), id }];
    case 'visual': return p.tipo ? [{ a: 'visual', tipo: p.tipo, spec: p.spec || {}, id }] : [];
    case 'write_math': return tex ? [{ a: 'mate', tex, id, color: col === 'ink_primary' ? '' : col }] : [];
    case 'write_text': case 'teacher_note': return p.text || p.text_es ? [{ a: 'texto', txt: visible(p.text || p.text_es), id, clase: 'nota' }] : [];
    case 'write_structured_notes': {
      const items = (p.items || p.bullets || p.lines || []).map((x) => visible(typeof x === 'string' ? x : `${x.label ? `**${x.label}:** ` : ''}${x.text || x.value || ''}`)).filter(Boolean);
      const out = [];
      if (p.title) out.push({ a: 'texto', txt: `**${visible(p.title)}**`, id: nid('ia') });
      if (items.length) out.push({ a: 'lista', items, id });
      return out;
    }
    case 'expand_intermediate_steps': {
      const st = p.steps || p.intermediate_steps || [];
      return st.flatMap((x) => {
        const o = [];
        const nota = typeof x === 'string' ? '' : x.explanation || x.note_es || x.text || '';
        const t = typeof x === 'string' ? x : x.latex || x.tex || '';
        if (nota) o.push({ a: 'texto', txt: visible(nota), id: nid('ia'), clase: 'nota' });
        if (t) o.push({ a: 'mate', tex: t, id: nid('ia') });
        return o;
      });
    }
    case 'plot_pv_diagram': return p.estados ? [{ a: 'visual', tipo: 'pv', spec: p, id }] : [];
    default: return [];
  }
}

/** BEAT_PLAN -> mini paso. Divide la voz por marcas <cue id='…'/> y cuelga de cada una su acción. */
export function planAMini(beat) {
  const acc = beat.actions || [];
  const voz = String(beat.speech_markup || '');
  const trozos = [];
  const re = /<cue\s+id=['"]([^'"]+)['"]\s*\/?>/g;
  let m;
  let i = 0;
  let cue = '';
  while ((m = re.exec(voz)) !== null) {
    trozos.push({ cue, txt: voz.slice(i, m.index) });
    cue = m[1];
    i = m.index + m[0].length;
  }
  trozos.push({ cue, txt: voz.slice(i) });
  const usados = new Set();
  const tramos = trozos.filter((t) => limpiarHabla(t.txt)).map((t) => {
    const mias = acc.filter((a) => a.animation && a.animation.cue_id === t.cue && t.cue);
    mias.forEach((a) => usados.add(a));
    return { habla: limpiarHabla(t.txt), acc: mias.flatMap(accionA) };
  });
  const sueltas = acc.filter((a) => !usados.has(a)).flatMap(accionA);
  if (sueltas.length) {
    if (tramos.length) tramos[tramos.length - 1].acc.push(...sueltas);
    else tramos.push({ habla: '', acc: sueltas });
  }
  const pq = beat.pedagogy && beat.pedagogy.check_question;
  if (pq) tramos.push({ habla: limpiarHabla(pq), acc: [{ a: 'texto', txt: visible(pq), id: nid('ia'), clase: 'nota' }] });
  if (!tramos.length) return null;
  const mini = miniPaso(tramos);
  mini.titulo = beat.pedagogy && beat.pedagogy.micro_goal ? beat.pedagogy.micro_goal : 'Respuesta';
  return mini;
}
