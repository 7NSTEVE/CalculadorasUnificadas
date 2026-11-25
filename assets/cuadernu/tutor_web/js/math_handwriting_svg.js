/**
 * Handwritten Whiteboard 2D Math Renderer + Sub-Term Hit Testing.
 * Renders natural 2D mathematical notation (stacked fractions, big integrals with
 * limits, square roots, Greek symbols, superscripts/subscripts) in authentic
 * professor marker handwriting with progressive left-to-right pen-nib writing!
 * Never shows raw LaTeX code to the student.
 */

/** Estimate realistic human whiteboard marker writing duration (ms) from LaTeX visual complexity. */
export function estimateMathWriteDurationMs(latexStr) {
  const s = String(latexStr || '');
  const fracs = (s.match(/\\frac\b/g) || []).length;
  const specials = (s.match(/\\(?:int|sqrt|sum|lim|Delta|gamma|pi|theta|alpha|beta|sigma|lambda|omega)\b/g) || []).length;
  const visible = s
    .replace(/\\class\{[^}]*\}/g, '')
    .replace(/\\(?:text|mathrm|mathbf|left|right|displaystyle|quad|qquad)\b/g, '')
    .replace(/\\[A-Za-z]+/g, 'x')
    .replace(/[{}\\_^,\s]/g, '').length;
  const est = 900 + visible * 78 + fracs * 480 + specials * 220;
  return Math.max(1350, Math.min(6200, Math.round(est)));
}

/** Gentle S-ramp at very start/end with steady human writing speed across the body (no 70%-in-400ms burst). */
function humanMarkerCurve(p) {
  const t = Math.max(0, Math.min(1, p));
  if (t < 0.08) return (t * t) / 0.16;
  if (t > 0.92) {
    const u = 1 - t;
    return 1 - (u * u) / 0.16;
  }
  return 0.04 + ((t - 0.08) / 0.84) * 0.92;
}

export async function renderMath2D(
  containerEl,
  latexStr,
  {
    animate = true,
    durationMs = 0,
    reduceMotion = false,
    getSpeedFactor = null,
    isAborted = null,
  } = {},
) {
  containerEl.replaceChildren();
  const cleanLatex = String(latexStr || '').trim();
  if (!cleanLatex) return { engine: 'empty' };

  const stage = document.createElement('div');
  stage.className = 'm2d-hand-stage';

  const dom = buildLocal2DMathDom(cleanLatex);
  stage.appendChild(dom);

  const pen = document.createElement('span');
  pen.className = 'wb-pen-nib m2d-pen';
  pen.hidden = true;
  stage.appendChild(pen);

  containerEl.appendChild(stage);

  if (animate && !reduceMotion) {
    const autoDur = estimateMathWriteDurationMs(cleanLatex);
    const dur = durationMs > 0
      ? Math.max(1100, Math.min(8800, Math.max(durationMs, Math.round(autoDur * 0.78))))
      : autoDur;
    pen.hidden = false;
    dom.style.clipPath = 'inset(0 100% 0 0)';
    await new Promise((resolve) => {
      let lastNow = performance.now();
      let progressMs = 0;
      function frame(now) {
        if (isAborted && isAborted()) {
          dom.style.clipPath = 'none';
          pen.hidden = true;
          resolve();
          return;
        }
        const dt = Math.min(80, Math.max(0, now - lastNow));
        lastNow = now;
        const speed = typeof getSpeedFactor === 'function' ? Math.max(0.15, Math.min(2.2, Number(getSpeedFactor()) || 1.0)) : 1.0;
        progressMs += dt * speed;
        const p = Math.min(1, progressMs / dur);
        const eased = humanMarkerCurve(p);
        const pctLeft = eased * 100;
        dom.style.clipPath = `inset(0 ${Math.max(0, 100 - pctLeft).toFixed(2)}% 0 0)`;
        pen.style.left = `${Math.min(100, pctLeft).toFixed(1)}%`;
        pen.style.top = `${50 + Math.sin(p * Math.PI * 10) * 14}%`;
        if (p < 1) {
          requestAnimationFrame(frame);
        } else {
          dom.style.clipPath = 'none';
          pen.hidden = true;
          resolve();
        }
      }
      requestAnimationFrame(frame);
    });
  }

  return { engine: 'handwritten_2d_math' };
}

function buildLocal2DMathDom(latex) {
  const root = document.createElement('span');
  root.className = 'm2d-root';
  parseInto(root, latex);
  root.normalize();
  return root;
}

function readBraced(str, startIdx) {
  let i = startIdx;
  while (i < str.length && str[i] === ' ') i++;
  if (str[i] !== '{') return { content: str[i] || '', next: Math.min(str.length, i + 1) };
  let depth = 0;
  const open = i;
  while (i < str.length) {
    if (str[i] === '{') depth++;
    else if (str[i] === '}') {
      depth--;
      if (depth === 0) return { content: str.slice(open + 1, i), next: i + 1 };
    }
    i++;
  }
  return { content: str.slice(open + 1), next: str.length };
}

function parseInto(parent, s) {
  const symMap = {
    '\\pi': 'π', '\\theta': 'θ', '\\Delta': 'Δ', '\\delta': 'δ', '\\gamma': 'γ',
    '\\rho': 'ρ', '\\alpha': 'α', '\\beta': 'β', '\\sigma': 'σ', '\\eta': 'η',
    '\\lambda': 'λ', '\\mu': 'μ', '\\omega': 'ω', '\\Omega': 'Ω', '\\phi': 'φ',
    '\\cdot': ' · ', '\\times': ' × ', '\\pm': ' ± ', '\\infty': '∞',
    '\\leq': ' ≤ ', '\\geq': ' ≥ ', '\\neq': ' ≠ ', '\\approx': ' ≈ ',
    '\\to': ' → ', '\\rightarrow': ' → ', '\\Rightarrow': ' ⇒ ',
    '\\qquad': '   ', '\\quad': '  ', '\\,': '\u00a0', '\\;': '\u00a0', '\\:': '\u00a0',
    '\\left': '', '\\right': '', '\\displaystyle': '', '\\text': '',
    '\\ln': 'ln', '\\log': 'log', '\\sin': 'sen', '\\cos': 'cos', '\\tan': 'tan',
    '\\exp': 'exp', '\\circ': '°',
  };

  let i = 0;
  while (i < s.length) {
    if (s.startsWith('\\class{', i)) {
      const cls = readBraced(s, i + 6);
      const body = readBraced(s, cls.next);
      const sp = document.createElement('span');
      sp.className = cls.content.replace(/[^A-Za-z0-9_\- ]/g, '');
      parseInto(sp, body.content);
      parent.appendChild(sp);
      i = body.next;
      continue;
    }
    if (s.startsWith('\\text{', i) || s.startsWith('\\mathrm{', i) || s.startsWith('\\mathbf{', i)) {
      const cmdEnd = s.indexOf('{', i);
      const body = readBraced(s, cmdEnd);
      const sp = document.createElement('span');
      sp.className = 'm2d-text';
      parseInto(sp, body.content);
      parent.appendChild(sp);
      i = body.next;
      continue;
    }
    if (s.startsWith('\\sqrt', i)) {
      const body = readBraced(s, i + 5);
      const wrap = document.createElement('span');
      wrap.className = 'm2d-sqrt';
      const rad = document.createElement('span');
      rad.className = 'm2d-sqrt-rad';
      rad.textContent = '√';
      const inner = document.createElement('span');
      inner.className = 'm2d-sqrt-inner';
      parseInto(inner, body.content);
      wrap.append(rad, inner);
      parent.appendChild(wrap);
      i = body.next;
      continue;
    }
    if (s.startsWith('\\frac', i)) {
      const num = readBraced(s, i + 5);
      const den = readBraced(s, num.next);
      const frac = document.createElement('span');
      frac.className = 'm2d-frac';
      const nEl = document.createElement('span');
      nEl.className = 'm2d-num';
      parseInto(nEl, num.content);
      const dEl = document.createElement('span');
      dEl.className = 'm2d-den';
      parseInto(dEl, den.content);
      frac.append(nEl, dEl);
      parent.appendChild(frac);
      i = den.next;
      continue;
    }
    if (s.startsWith('\\int', i)) {
      let next = i + 4;
      let lower = '';
      let upper = '';
      while (next < s.length && (s[next] === '_' || s[next] === '^' || s[next] === ' ')) {
        if (s[next] === ' ') { next++; continue; }
        const mark = s[next];
        const b = readBraced(s, next + 1);
        if (mark === '_') lower = b.content;
        else upper = b.content;
        next = b.next;
      }
      const wrap = document.createElement('span');
      wrap.className = 'm2d-int';
      const sym = document.createElement('span');
      sym.className = 'm2d-int-sym';
      sym.textContent = '∫';
      wrap.appendChild(sym);
      if (lower || upper) {
        const lims = document.createElement('span');
        lims.className = 'm2d-lims';
        const uEl = document.createElement('span');
        parseInto(uEl, upper);
        const lEl = document.createElement('span');
        parseInto(lEl, lower);
        lims.append(uEl, lEl);
        wrap.appendChild(lims);
      }
      parent.appendChild(wrap);
      i = next;
      continue;
    }
    if (s[i] === '^' || s[i] === '_') {
      const tag = s[i] === '^' ? 'sup' : 'sub';
      const b = readBraced(s, i + 1);
      const el = document.createElement(tag);
      parseInto(el, b.content);
      parent.appendChild(el);
      i = b.next;
      continue;
    }
    if (s[i] === '\\') {
      const m = /^\\[A-Za-z]+|^\\[,;:!]/.exec(s.slice(i));
      if (m) {
        const cmd = m[0];
        if (cmd in symMap) {
          parent.appendChild(document.createTextNode(symMap[cmd]));
        }
        i += cmd.length;
        continue;
      }
    }
    if (s[i] === '{' || s[i] === '}') {
      i++;
      continue;
    }
    parent.appendChild(document.createTextNode(s[i] === '-' ? '−' : s[i]));
    i++;
  }
}

export function hitTestMathTerms(lassoRect, rootEl) {
  const hits = [];
  const candidates = rootEl.querySelectorAll('.step-block [class*="term-"]');
  for (const el of candidates) {
    const r = el.getBoundingClientRect();
    if (r.width === 0 || r.height === 0) continue;
    const overlap = !(r.right < lassoRect.left || r.left > lassoRect.right || r.bottom < lassoRect.top || r.top > lassoRect.bottom);
    if (overlap) {
      const block = el.closest('.step-block');
      const termCls = Array.from(el.classList).find((c) => c.startsWith('term-')) || '';
      hits.push({
        kind: 'tutor_math_term',
        object_id: block ? block.dataset.objectId : '',
        term_class: termCls,
        step_index: block ? Number(block.dataset.stepIndex || 0) : 0,
        element: el,
      });
    }
  }
  if (!hits.length) {
    const blocks = rootEl.querySelectorAll('.step-block[data-object-id]');
    for (const block of blocks) {
      const r = block.getBoundingClientRect();
      const overlap = !(r.right < lassoRect.left || r.left > lassoRect.right || r.bottom < lassoRect.top || r.top > lassoRect.bottom);
      if (overlap) {
        hits.push({
          kind: 'tutor_step_block',
          object_id: block.dataset.objectId,
          term_class: '',
          step_index: Number(block.dataset.stepIndex || 0),
          element: block,
        });
      }
    }
  }
  return hits;
}
