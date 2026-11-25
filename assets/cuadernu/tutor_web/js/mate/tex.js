/**
 * Intérprete del subconjunto de TeX que usa el tutor.
 *
 * Regla de oro: un comando desconocido NUNCA se borra en silencio. Se convierte en
 * un nodo `error` visible y queda registrado en `desconocidos`, para que las pruebas
 * lo detecten antes de que llegue a la pantalla del estudiante.
 */

export const GRIEGO = {
  alpha: 'α', beta: 'β', gamma: 'γ', delta: 'δ', epsilon: 'ε', varepsilon: 'ε', zeta: 'ζ',
  eta: 'η', theta: 'θ', vartheta: 'θ', iota: 'ι', kappa: 'κ', lambda: 'λ', mu: 'μ', nu: 'ν',
  xi: 'ξ', pi: 'π', varpi: 'π', rho: 'ρ', varrho: 'ρ', sigma: 'σ', varsigma: 'ς', tau: 'τ',
  upsilon: 'υ', phi: 'φ', varphi: 'φ', chi: 'χ', psi: 'ψ', omega: 'ω',
  Gamma: 'Γ', Delta: 'Δ', Theta: 'Θ', Lambda: 'Λ', Xi: 'Ξ', Pi: 'Π', Sigma: 'Σ',
  Upsilon: 'Υ', Phi: 'Φ', Psi: 'Ψ', Omega: 'Ω',
};

const RELACIONES = {
  le: '≤', leq: '≤', leqslant: '≤', ge: '≥', geq: '≥', geqslant: '≥', ne: '≠', neq: '≠',
  approx: '≈', simeq: '≃', sim: '∼', cong: '≅', equiv: '≡', propto: '∝', ll: '≪', gg: '≫',
  to: '→', rightarrow: '→', longrightarrow: '→', leftarrow: '←', gets: '←',
  leftrightarrow: '↔', Rightarrow: '⇒', implies: '⇒', Longrightarrow: '⇒', Leftarrow: '⇐',
  Leftrightarrow: '⇔', iff: '⇔', mapsto: '↦', in: '∈', notin: '∉', parallel: '∥', perp: '⊥',
  therefore: '∴', because: '∵', mid: '|', uparrow: '↑', downarrow: '↓',
};

const BINARIOS = {
  pm: '±', mp: '∓', times: '×', cdot: '·', div: '÷', ast: '∗', star: '⋆', cup: '∪', cap: '∩',
  bullet: '•', setminus: '∖',
};

const ORDINARIOS = {
  infty: '∞', partial: '∂', nabla: '∇', ell: 'ℓ', hbar: 'ħ', prime: '′', degree: '°',
  circ: '°', angle: '∠', triangle: '△', square: '□', checkmark: '✓', forall: '∀', exists: '∃',
  emptyset: '∅', varnothing: '∅', Re: 'Re', Im: 'Im', ldots: '…', dots: '…', cdots: '⋯',
  vdots: '⋮', ddots: '⋱', dotsc: '…', dotsb: '⋯', lq: '‘', rq: '’', S: '§', pounds: '£',
  '%': '%', '#': '#', $: '$', '&': '&', _: '_', aleph: 'ℵ', imath: 'ı', jmath: 'ȷ', AA: 'Å',
};

const FUNCIONES = {
  ln: 'ln', log: 'log', exp: 'exp', sin: 'sen', sen: 'sen', cos: 'cos', tan: 'tan', cot: 'cot',
  sec: 'sec', csc: 'csc', arcsin: 'arcsen', arccos: 'arccos', arctan: 'arctan', sinh: 'senh',
  cosh: 'cosh', tanh: 'tanh', det: 'det', dim: 'dim', deg: 'deg', gcd: 'mcd', arg: 'arg',
};
const FUNCIONES_CON_LIMITES = { lim: 'lím', max: 'máx', min: 'mín', sup: 'sup', inf: 'ínf' };

const GRANDES = {
  int: { s: '∫', tipo: 'integral' }, iint: { s: '∬', tipo: 'integral', n: 2 },
  oint: { s: '∮', tipo: 'integral', cerrada: true },
  sum: { s: '∑', tipo: 'suma' }, prod: { s: '∏', tipo: 'producto' },
};

const ESPACIOS = {
  ',': 0.17, ':': 0.22, ';': 0.28, '!': -0.17, ' ': 0.3, quad: 1, qquad: 2, enspace: 0.5,
  thinspace: 0.17, medspace: 0.22, thickspace: 0.28, negthinspace: -0.17, hfill: 1,
};

const ACENTOS = {
  dot: 'punto', ddot: 'dospuntos', bar: 'barra', overline: 'barra', vec: 'flecha',
  overrightarrow: 'flecha', hat: 'gorro', widehat: 'gorro', tilde: 'virgulilla',
  widetilde: 'virgulilla', underline: 'subraya', acute: 'agudo', grave: 'grave',
};

const DELIMITADORES = {
  '(': '(', ')': ')', '[': '[', ']': ']', '|': '|', '/': '/', '.': '',
  '\\{': '{', '\\}': '}', '\\|': '‖', '\\lbrace': '{', '\\rbrace': '}', '\\lbrack': '[',
  '\\rbrack': ']', '\\langle': '⟨', '\\rangle': '⟩', '\\lvert': '|', '\\rvert': '|',
  '\\lVert': '‖', '\\rVert': '‖', '\\vert': '|', '\\Vert': '‖', '\\lfloor': '⌊',
  '\\rfloor': '⌋', '\\lceil': '⌈', '\\rceil': '⌉', '<': '⟨', '>': '⟩', '\\uparrow': '↑',
  '\\downarrow': '↓',
};

const TEXTO_CMDS = new Set([
  'text', 'textrm', 'textnormal', 'textup', 'mbox', 'hbox', 'textsf', 'texttt', 'textit', 'textbf', 'emph',
]);
const ROMANO_CMDS = new Set(['mathrm', 'operatorname', 'mathsf', 'mathtt', 'rm', 'mathup']);
const NEGRITA_CMDS = new Set(['mathbf', 'boldsymbol', 'bm', 'bf']);
const IGUAL_CMDS = new Set(['mathit', 'mathcal', 'mathscr', 'mathbb', 'mathfrak', 'it', 'mathnormal', 'ensuremath']);
const IGNORAR = new Set([
  'displaystyle', 'textstyle', 'scriptstyle', 'scriptscriptstyle', 'limits', 'nolimits',
  'nonumber', 'notag', 'left.', 'allowbreak', 'nobreak', 'relax', 'centering', 'hline', 'strut',
  'mathstrut', 'bigl.', 'noindent',
]);
const TAMANOS = { big: 1.25, Big: 1.6, bigg: 2.1, Bigg: 2.6 };

function tokenizar(src) {
  const toks = [];
  let i = 0;
  const s = String(src || '');
  while (i < s.length) {
    const c = s[i];
    if (c === '\\') {
      const m = /^\\([A-Za-z]+)\*?/.exec(s.slice(i));
      if (m) {
        toks.push({ t: 'cmd', v: m[1], pos: i });
        i += m[0].length;
        // TeX se come el espacio que sigue a un comando con letras
        while (i < s.length && /\s/.test(s[i])) i += 1;
        continue;
      }
      const sig = s[i + 1];
      if (sig === '\\') {
        toks.push({ t: 'salto', v: '\\\\', pos: i });
        i += 2;
        // Espaciado opcional \\[6pt]
        const esp = /^\s*\[\s*-?\d+(?:\.\d+)?\s*(?:pt|em|ex|mm|cm|px|mu)\s*\]/.exec(s.slice(i));
        if (esp) i += esp[0].length;
        continue;
      }
      if (sig === undefined) { i += 1; continue; }
      toks.push({ t: 'cmd', v: sig === '\n' ? ' ' : sig, pos: i });
      i += 2;
      continue;
    }
    if (c === '{' || c === '}' || c === '^' || c === '_' || c === '&') {
      toks.push({ t: c, v: c, pos: i });
      i += 1;
      continue;
    }
    if (c === '$') { i += 1; continue; }
    if (/\s/.test(c)) {
      toks.push({ t: 'esp', v: ' ', pos: i });
      i += 1;
      continue;
    }
    if (c === '~') { toks.push({ t: 'cmd', v: ' ', pos: i }); i += 1; continue; }
    // Respeta pares sustitutos
    const cp = String.fromCodePoint(s.codePointAt(i));
    toks.push({ t: 'car', v: cp, pos: i });
    i += cp.length;
  }
  return { toks, s };
}

class Parser {
  constructor(src) {
    const { toks, s } = tokenizar(src);
    this.toks = toks;
    this.src = s;
    this.i = 0;
    this.desconocidos = [];
  }

  ver(o = 0) { return this.toks[this.i + o]; }
  sig() { return this.toks[this.i++]; }
  saltarEsp() { while (this.ver() && this.ver().t === 'esp') this.i += 1; }

  /** Lee el contenido textual literal de un grupo {...} (para \text, \begin, \class). */
  grupoLiteral() {
    this.saltarEsp();
    const t = this.ver();
    if (!t) return '';
    if (t.t !== '{') { this.i += 1; return t.t === 'cmd' ? `\\${t.v}` : t.v; }
    const ini = t.pos + 1;
    let prof = 0;
    let fin = this.src.length;
    while (this.ver()) {
      const k = this.sig();
      if (k.t === '{') prof += 1;
      else if (k.t === '}') { prof -= 1; if (prof === 0) { fin = k.pos; break; } }
    }
    return this.src.slice(ini, fin);
  }

  /** Argumento opcional [..]; devuelve null si no existe. */
  opcional() {
    const guarda = this.i;
    this.saltarEsp();
    const t = this.ver();
    if (!t || t.t !== 'car' || t.v !== '[') { this.i = guarda; return null; }
    this.i += 1;
    const nodos = [];
    while (this.ver() && !(this.ver().t === 'car' && this.ver().v === ']')) {
      const n = this.atomo();
      if (n) nodos.push(n);
    }
    if (this.ver()) this.i += 1;
    return { k: 'fila', hijos: nodos };
  }

  /** Argumento obligatorio: grupo {..} o un solo átomo. */
  arg() {
    this.saltarEsp();
    const t = this.ver();
    if (!t) return { k: 'fila', hijos: [] };
    if (t.t === '{') { this.i += 1; return this.fila(['}']); }
    const n = this.atomoSinScripts();
    return { k: 'fila', hijos: n ? [n] : [] };
  }

  fila(fines = []) {
    const hijos = [];
    while (this.ver()) {
      const t = this.ver();
      if (fines.includes(t.t) || (t.t === 'cmd' && fines.includes(`\\${t.v}`))) {
        if (t.t === '}' ) this.i += 1;
        break;
      }
      if (t.t === '}') { this.i += 1; break; }
      if (t.t === 'esp') { this.i += 1; continue; }
      const n = this.atomo();
      if (n) hijos.push(n);
    }
    return { k: 'fila', hijos };
  }

  atomo() {
    const base = this.atomoSinScripts();
    if (!base) return null;
    return this.conScripts(base);
  }

  conScripts(base) {
    let sup = null;
    let sub = null;
    let primas = 0;
    for (;;) {
      const guarda = this.i;
      this.saltarEsp();
      const t = this.ver();
      if (t && t.t === 'car' && t.v === "'") { this.i += 1; primas += 1; continue; }
      if (t && t.t === 'cmd' && (t.v === 'limits' || t.v === 'nolimits')) { this.i += 1; continue; }
      if (t && t.t === '^' && sup === null) { this.i += 1; sup = this.arg(); continue; }
      if (t && t.t === '_' && sub === null) { this.i += 1; sub = this.arg(); continue; }
      this.i = guarda;
      break;
    }
    if (primas) {
      const p = { k: 'ord', s: '′'.repeat(primas) };
      sup = sup ? { k: 'fila', hijos: [p, ...sup.hijos] } : { k: 'fila', hijos: [p] };
    }
    if (!sup && !sub) return base;
    if (base.k === 'grande' || base.k === 'funlim') { base.sup = sup; base.sub = sub; return base; }
    if (base.k === 'llave') { base.etiqueta = base.abajo ? (sub || sup) : (sup || sub); return base; }
    return { k: 'scripts', base, sup, sub };
  }

  desconocido(nombre) {
    this.desconocidos.push(nombre);
    return { k: 'error', s: `\\${nombre}` };
  }

  delimitador() {
    this.saltarEsp();
    const t = this.sig();
    if (!t) return '';
    const clave = t.t === 'cmd' ? `\\${t.v}` : t.v;
    if (clave in DELIMITADORES) return DELIMITADORES[clave];
    this.desconocidos.push(`delimitador ${clave}`);
    return clave;
  }

  izquierdaDerecha() {
    const izq = this.delimitador();
    const partes = [];
    let actual = [];
    const medios = [];
    let der = '';
    for (;;) {
      const t = this.ver();
      if (!t) break;
      if (t.t === 'cmd' && t.v === 'right') { this.i += 1; der = this.delimitador(); break; }
      if (t.t === 'cmd' && t.v === 'middle') {
        this.i += 1;
        medios.push(this.delimitador());
        partes.push({ k: 'fila', hijos: actual });
        actual = [];
        continue;
      }
      if (t.t === 'esp') { this.i += 1; continue; }
      if (t.t === '}') break;
      const n = this.atomo();
      if (n) actual.push(n);
    }
    partes.push({ k: 'fila', hijos: actual });
    return { k: 'delim', izq, der, partes, medios };
  }

  entorno() {
    const nombre = this.grupoLiteral().replace(/\*$/, '');
    let alinea = '';
    if (nombre === 'array') alinea = this.grupoLiteral();
    const filas = [];
    let fila = [];
    let celda = [];
    const cerrarCelda = () => { fila.push({ k: 'fila', hijos: celda }); celda = []; };
    const cerrarFila = () => { cerrarCelda(); filas.push(fila); fila = []; };
    for (;;) {
      const t = this.ver();
      if (!t) break;
      if (t.t === 'cmd' && t.v === 'end') { this.i += 1; this.grupoLiteral(); break; }
      if (t.t === '&') { this.i += 1; cerrarCelda(); continue; }
      if (t.t === 'salto') { this.i += 1; cerrarFila(); continue; }
      if (t.t === 'esp') { this.i += 1; continue; }
      if (t.t === '}') { this.i += 1; continue; }
      const n = this.atomo();
      if (n) celda.push(n);
    }
    if (celda.length || fila.length) cerrarFila();
    const base = { k: 'matriz', filas, entorno: nombre, alinea };
    if (nombre === 'cases') return { k: 'delim', izq: '{', der: '', partes: [{ k: 'fila', hijos: [base] }], medios: [] };
    if (nombre === 'pmatrix') return { k: 'delim', izq: '(', der: ')', partes: [{ k: 'fila', hijos: [base] }], medios: [] };
    if (nombre === 'bmatrix') return { k: 'delim', izq: '[', der: ']', partes: [{ k: 'fila', hijos: [base] }], medios: [] };
    if (nombre === 'vmatrix') return { k: 'delim', izq: '|', der: '|', partes: [{ k: 'fila', hijos: [base] }], medios: [] };
    const conocidos = ['aligned', 'align', 'alignedat', 'gathered', 'gather', 'split', 'matrix', 'array', 'equation', 'smallmatrix'];
    if (!conocidos.includes(nombre)) this.desconocidos.push(`entorno ${nombre}`);
    return base;
  }

  atomoSinScripts() {
    const t = this.sig();
    if (!t) return null;
    switch (t.t) {
      case 'esp': return null;
      case '{': { const f = this.fila(['}']); return { k: 'grupo', hijos: f.hijos }; }
      case '}': return null;
      case '&': return null;
      case 'salto': return { k: 'salto' };
      case '^': case '_': {
        // Script sin base: se apoya en un átomo vacío
        this.i -= 1;
        return { k: 'grupo', hijos: [] };
      }
      case 'car': return this.caracter(t.v);
      default: return this.comando(t.v);
    }
  }

  caracter(c) {
    if ('+−-*'.includes(c)) return { k: 'bin', s: c === '-' ? '−' : c === '*' ? '·' : c };
    if ('=<>'.includes(c)) return { k: 'rel', s: c };
    if ('≤≥≠≈≡∝→⇒⇔∼←↔∈'.includes(c)) return { k: 'rel', s: c };
    if ('±×÷·∓'.includes(c)) return { k: 'bin', s: c };
    if ('([{'.includes(c)) return { k: 'abre', s: c };
    if (')]}'.includes(c)) return { k: 'cierra', s: c };
    if (c === '|') return { k: 'barra', s: '|' };
    if (c === ',' || c === ';') return { k: 'punt', s: c };
    if (c === ':') return { k: 'rel', s: ':' };
    return { k: 'ord', s: c };
  }

  comando(v) {
    if (v in GRIEGO) return { k: 'ord', s: GRIEGO[v], griego: true };
    if (v in RELACIONES) return { k: 'rel', s: RELACIONES[v] };
    if (v in BINARIOS) return { k: 'bin', s: BINARIOS[v] };
    if (v in ORDINARIOS) return { k: 'ord', s: ORDINARIOS[v] };
    if (v in FUNCIONES) return { k: 'fun', s: FUNCIONES[v] };
    if (v in FUNCIONES_CON_LIMITES) return { k: 'funlim', s: FUNCIONES_CON_LIMITES[v] };
    if (v in GRANDES) return { k: 'grande', ...GRANDES[v] };
    if (v in ESPACIOS) return { k: 'espacio', em: ESPACIOS[v] };
    if (v in ACENTOS) return { k: 'acento', tipo: ACENTOS[v], cuerpo: this.arg() };
    if (IGNORAR.has(v)) return null;
    if (TEXTO_CMDS.has(v)) return { k: 'texto', s: this.grupoLiteral().replace(/\\([%$#&_{} ])/g, '$1').replace(/~/g, ' '), negrita: v === 'textbf' };
    if (ROMANO_CMDS.has(v)) {
      const n = this.arg();
      return v === 'operatorname' ? { k: 'fun', cuerpo: n, s: '' } : { k: 'romano', cuerpo: n };
    }
    if (NEGRITA_CMDS.has(v)) return { k: 'negrita', cuerpo: this.arg() };
    if (IGUAL_CMDS.has(v)) { const n = this.arg(); return { k: 'grupo', hijos: n.hijos }; }
    switch (v) {
      case '{': return { k: 'abre', s: '{' };
      case '}': return { k: 'cierra', s: '}' };
      case '|': return { k: 'barra', s: '‖' };
      case 'lbrace': return { k: 'abre', s: '{' };
      case 'rbrace': return { k: 'cierra', s: '}' };
      case 'langle': return { k: 'abre', s: '⟨' };
      case 'rangle': return { k: 'cierra', s: '⟩' };
      case 'lvert': return { k: 'abre', s: '|' };
      case 'rvert': return { k: 'cierra', s: '|' };
      case 'vert': return { k: 'barra', s: '|' };
      case 'colon': return { k: 'punt', s: ':' };
      case 'frac': case 'dfrac': case 'tfrac': case 'cfrac':
        return { k: 'frac', num: this.arg(), den: this.arg(), chica: v === 'tfrac' };
      case 'binom': {
        const a = this.arg(); const b = this.arg();
        return { k: 'delim', izq: '(', der: ')', medios: [], partes: [{ k: 'fila', hijos: [{ k: 'frac', num: a, den: b, sinBarra: true }] }] };
      }
      case 'sqrt': { const idx = this.opcional(); return { k: 'raiz', indice: idx, cuerpo: this.arg() }; }
      case 'left': return this.izquierdaDerecha();
      case 'right': case 'middle': { this.delimitador(); return null; }
      case 'big': case 'Big': case 'bigg': case 'Bigg':
      case 'bigl': case 'bigr': case 'Bigl': case 'Bigr': case 'biggl': case 'biggr': case 'Biggl': case 'Biggr':
      case 'bigm': case 'Bigm': {
        const tam = TAMANOS[v.replace(/[lrm]$/, '')] || 1.25;
        const d = this.delimitador();
        return { k: 'delimfijo', s: d, tam, lado: v.endsWith('l') ? 'abre' : v.endsWith('r') ? 'cierra' : 'ord' };
      }
      case 'begin': return this.entorno();
      case 'end': { this.grupoLiteral(); return null; }
      case 'boxed': case 'fbox': return { k: 'caja', cuerpo: this.arg() };
      case 'cancel': case 'bcancel': case 'xcancel': case 'sout': return { k: 'tachado', cuerpo: this.arg(), tipo: v };
      case 'cancelto': { const a = this.arg(); return { k: 'tachado', a, cuerpo: this.arg(), tipo: 'cancel' }; }
      case 'underbrace': return { k: 'llave', abajo: true, cuerpo: this.arg(), etiqueta: null };
      case 'overbrace': return { k: 'llave', abajo: false, cuerpo: this.arg(), etiqueta: null };
      case 'overset': case 'stackrel': { const a = this.arg(); return { k: 'apilado', arriba: a, cuerpo: this.arg() }; }
      case 'underset': { const a = this.arg(); return { k: 'apilado', abajo: a, cuerpo: this.arg() }; }
      case 'xrightarrow': { const ab = this.opcional(); return { k: 'flechalarga', abajo: ab, arriba: this.arg() }; }
      case 'class': case 'cssId': { const cls = this.grupoLiteral(); return { k: 'clase', cls: v === 'class' ? cls : '', cuerpo: this.arg() }; }
      case 'color': case 'textcolor': { const col = this.grupoLiteral(); return { k: 'color', color: col, cuerpo: this.arg() }; }
      case 'phantom': case 'hphantom': case 'vphantom': return { k: 'fantasma', cuerpo: this.arg() };
      case 'hspace': case 'hskip': case 'kern': { this.grupoLiteral(); return { k: 'espacio', em: 0.5 }; }
      case 'not': {
        this.saltarEsp();
        const n = this.atomoSinScripts();
        if (n && n.k === 'rel' && n.s === '=') return { k: 'rel', s: '≠' };
        return n;
      }
      case 'label': case 'tag': case 'ref': { this.grupoLiteral(); return null; }
      case 'substack': { const n = this.arg(); return { k: 'grupo', hijos: n.hijos }; }
      case '-': return null;
      case '/': return { k: 'espacio', em: 0.05 };
      case "'": case '"': case '`': case '=': case '.': return { k: 'grupo', hijos: this.arg().hijos };
      default: return this.desconocido(v);
    }
  }
}

/** Decide si un signo es binario o unario y trata la coma decimal. */
function normalizar(nodo) {
  if (!nodo) return nodo;
  if (Array.isArray(nodo.hijos)) {
    const hs = nodo.hijos;
    for (let i = 0; i < hs.length; i += 1) {
      const n = hs[i];
      if (n.k === 'bin') {
        const prev = hs.slice(0, i).reverse().find((x) => x.k !== 'espacio' && x.k !== 'fantasma');
        if (!prev || ['bin', 'rel', 'abre', 'punt', 'salto', 'grande', 'fun', 'funlim'].includes(prev.k)) n.unario = true;
        if (prev && prev.k === 'delimfijo' && prev.lado === 'abre') n.unario = true;
      }
      if (n.k === 'punt' && n.s === ',') {
        const a = hs[i - 1];
        const b = hs[i + 1];
        const esDig = (x) => x && x.k === 'ord' && /^[0-9]$/.test(x.s);
        if (esDig(a) && esDig(b)) { n.k = 'ord'; n.decimal = true; }
      }
      // Barra vertical: abre o cierra según lo que la precede
      if (n.k === 'barra') {
        const prev = hs.slice(0, i).reverse().find((x) => x.k !== 'espacio');
        const abierta = hs.slice(0, i).filter((x) => x.esBarra).length % 2 === 1;
        n.esBarra = true;
        n.k = abierta ? 'cierra' : (!prev || ['bin', 'rel', 'abre', 'punt', 'fun'].includes(prev.k) ? 'abre' : 'cierra');
      }
      // {,} -> coma decimal sin espacio
      if (n.k === 'grupo' && n.hijos.length === 1 && n.hijos[0].k === 'punt' && n.hijos[0].s === ',') {
        hs[i] = { k: 'ord', s: ',', decimal: true };
      }
    }
  }
  for (const c of ['num', 'den', 'cuerpo', 'indice', 'sup', 'sub', 'base', 'arriba', 'abajo', 'etiqueta', 'a']) {
    if (nodo[c] && typeof nodo[c] === 'object') normalizar(nodo[c]);
  }
  if (Array.isArray(nodo.partes)) nodo.partes.forEach(normalizar);
  if (Array.isArray(nodo.filas)) nodo.filas.forEach((f) => f.forEach(normalizar));
  if (Array.isArray(nodo.hijos)) nodo.hijos.forEach(normalizar);
  return nodo;
}

/** @returns {{arbol: object, desconocidos: string[]}} */
export function analizarTex(src) {
  const p = new Parser(src);
  const hijos = [];
  while (p.ver()) {
    const antes = p.i;
    const n = p.atomo();
    if (n) hijos.push(n);
    if (p.i === antes) p.i += 1;
  }
  const arbol = normalizar({ k: 'fila', hijos });
  return { arbol, desconocidos: p.desconocidos };
}

/** Texto plano legible (para buscar, para etiquetas accesibles y para la cámara). */
export function textoPlano(src) {
  const { arbol } = analizarTex(src);
  const out = [];
  const ir = (n) => {
    if (!n) return;
    switch (n.k) {
      case 'frac': out.push('('); ir(n.num); out.push(')/('); ir(n.den); out.push(')'); return;
      case 'raiz': out.push('√('); ir(n.cuerpo); out.push(')'); return;
      case 'scripts':
        ir(n.base);
        if (n.sub) { out.push('_'); ir(n.sub); }
        if (n.sup) { out.push('^'); ir(n.sup); }
        return;
      case 'grande': case 'funlim': out.push(n.s); if (n.sub) ir(n.sub); if (n.sup) ir(n.sup); return;
      case 'delim': out.push(n.izq || ''); n.partes.forEach(ir); out.push(n.der || ''); return;
      case 'texto': out.push(n.s); return;
      case 'espacio': out.push(' '); return;
      case 'matriz': n.filas.forEach((f) => { f.forEach(ir); out.push('; '); }); return;
      default: break;
    }
    if (n.s) out.push(n.s);
    if (n.hijos) n.hijos.forEach(ir);
    if (n.cuerpo) ir(n.cuerpo);
    if (n.arriba) ir(n.arriba);
  };
  ir(arbol);
  return out.join('').replace(/\s+/g, ' ').trim();
}
