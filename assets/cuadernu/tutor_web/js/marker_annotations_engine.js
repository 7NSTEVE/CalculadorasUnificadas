/**
 * Hand-Drawn Professor Marker Annotations & Unit-Cancellation Engine for CuadernU.
 *
 * Implements 8 real whiteboard teacher gestures rendered in organic SVG with a
 * shared moving marker nib, explicit Light/Dark calibrated palettes (zero raw
 * `light-dark()` strings), automatic ResizeObserver reflow, short-step study progressive
 * step dimming, temporary provenance TTL fade-out, and exact token-boundary
 * unit cancellation (e.g., `atm` red, `L` green without touching `mol`, `mol` purple).
 */

const SVG_NS = 'http://www.w3.org/2000/svg';

const PALETTE_LIGHT = {
  red: '#dc2626',
  blue: '#1d4ed8',
  green: '#047857',
  orange: '#ea580c',
  purple: '#7c3aed',
  yellow: '#d97706',
  highlight_yellow: '#fde047',
  teal: '#0f766e',
  pink: '#db2777',
  ink: '#172554',
};

const PALETTE_DARK = {
  red: '#f87171',
  blue: '#60a5fa',
  green: '#4ade80',
  orange: '#fb923c',
  purple: '#a78bfa',
  yellow: '#facc15',
  highlight_yellow: '#facc15',
  teal: '#2dd4bf',
  pink: '#f472b6',
  ink: '#f1f5f9',
};

function isDarkTheme() {
  return document.documentElement.getAttribute('data-theme') === 'dark';
}

export function resolveMarkerHex(colorName, { isHighlighter = false } = {}) {
  const pal = isDarkTheme() ? PALETTE_DARK : PALETTE_LIGHT;
  const key = String(colorName || 'blue')
    .toLowerCase()
    .replace(/^accent_/, '')
    .replace(/^ink_/, '')
    .trim();
  if (isHighlighter && (key === 'yellow' || key === 'amber' || key === 'gold')) {
    return pal.highlight_yellow;
  }
  const alias = {
    rojo: 'red',
    azul: 'blue',
    verde: 'green',
    naranja: 'orange',
    morado: 'purple',
    violeta: 'purple',
    indigo: 'purple',
    amarillo: 'yellow',
    turquesa: 'teal',
    cian: 'teal',
    rosa: 'pink',
    magenta: 'pink',
    primary: 'ink',
  };
  const mapped = alias[key] || key;
  return pal[mapped] || pal.blue;
}

function escapeRegExp(s) {
  return String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function normalizeMathStr(s) {
  return String(s || '')
    .replace(/\u2212/g, '-')
    .replace(/\u00a0/g, ' ')
    .replace(/\s+/g, ' ');
}

function rectsOverlap(a, b) {
  return !(a.right < b.left || a.left > b.right || a.bottom < b.top || a.top > b.bottom);
}

/**
 * Compute the exact rendered pixel rectangle of an <img> accounting for `object-fit: contain`
 * horizontal/vertical letterboxing so normalized PDF coordinates align to the single pixel.
 */
export function getContainedImageClientRect(img) {
  const ir = img.getBoundingClientRect();
  const nw = img.naturalWidth || 0;
  const nh = img.naturalHeight || 0;
  if (!nw || !nh || ir.width < 2 || ir.height < 2) {
    return ir;
  }
  const boxRatio = ir.width / ir.height;
  const imgRatio = nw / nh;
  let rw = ir.width;
  let rh = ir.height;
  let ox = 0;
  let oy = 0;
  if (imgRatio > boxRatio) {
    rh = ir.width / imgRatio;
    oy = (ir.height - rh) * 0.5;
  } else if (imgRatio < boxRatio) {
    rw = ir.height * imgRatio;
    ox = (ir.width - rw) * 0.5;
  }
  return {
    left: ir.left + ox,
    top: ir.top + oy,
    right: ir.left + ox + rw,
    bottom: ir.top + oy + rh,
    width: rw,
    height: rh,
  };
}

/**
 * Extract the exact characters/symbols physically located inside a student-drawn clientBox.
 * Works at CHARACTER-LEVEL precision inside handwritten DOM equations (.step-block) so
 * circling just `γ` inside `1/(γ−1)` or just `1.50 atm` inside `(1.50 atm)(3.50 L)` returns
 * the exact symbol, plus the step title and full formula context. Also supports PyMuPDF
 * `crop_words` on cropped PDF problem images.
 */
export function extractTextInsideClientRect(rootEl, clientBox, cropWordsByObjectId = new Map()) {
  let hitObjectId = '';
  let hitPdfNormBox = null;

  // 1. Check PDF problem crops with exact PyMuPDF word boxes first
  const imgs = rootEl.querySelectorAll('.problem-clip-img');
  for (const img of imgs) {
    const ir = getContainedImageClientRect(img);
    if (ir.width < 10 || ir.height < 10 || !rectsOverlap(ir, clientBox)) continue;
    const block = img.closest('.step-block');
    const oid = block ? (block.dataset.objectId || '') : '';
    const titleEl = block ? block.querySelector('.step-label') : null;
    const stepTitle = titleEl ? (titleEl.textContent || '').trim() : 'Enunciado del problema';
    const nx0 = Math.max(0, (clientBox.left - ir.left) / ir.width);
    const ny0 = Math.max(0, (clientBox.top - ir.top) / ir.height);
    const nw = Math.min(1 - nx0, Math.max(0.02, (clientBox.right - clientBox.left) / ir.width));
    const nh = Math.min(1 - ny0, Math.max(0.02, (clientBox.bottom - clientBox.top) / ir.height));
    hitObjectId = oid;
    hitPdfNormBox = { x: nx0, y: ny0, w: nw, h: nh };

    const words = cropWordsByObjectId.get(oid) || [];
    const matchedWords = [];
    for (const cw of words) {
      const cx = cw.x + cw.w * 0.5;
      const cy = cw.y + cw.h * 0.5;
      if (cx >= nx0 - 0.012 && cx <= nx0 + nw + 0.012 && cy >= ny0 - 0.012 && cy <= ny0 + nh + 0.012) {
        matchedWords.push(cw.t);
      }
    }
    if (matchedWords.length) {
      return {
        text: matchedWords.join(' ').trim(),
        objectId: oid,
        stepTitle,
        lineContext: matchedWords.join(' ').trim(),
        pdfNormBox: hitPdfNormBox,
        sourceKind: 'pdf_statement',
      };
    }
  }

  // 2. Walk text nodes inside overlapping .step-block elements with CHARACTER-LEVEL hit testing!
  const blocks = Array.from(rootEl.querySelectorAll('.step-block[data-object-id]'));
  const boxCx = (clientBox.left + clientBox.right) * 0.5;
  const boxCy = (clientBox.top + clientBox.bottom) * 0.5;

  // Prefer the overlapping block closest to the center of the drawn mark
  const overlappingBlocks = blocks
    .map((block) => ({ block, br: block.getBoundingClientRect() }))
    .filter(({ br }) => br.width >= 4 && br.height >= 4 && rectsOverlap(br, clientBox))
    .sort((a, b) => {
      const da = Math.hypot((a.br.left + a.br.right) * 0.5 - boxCx, (a.br.top + a.br.bottom) * 0.5 - boxCy);
      const db = Math.hypot((b.br.left + b.br.right) * 0.5 - boxCx, (b.br.top + b.br.bottom) * 0.5 - boxCy);
      return da - db;
    });

  for (const { block } of overlappingBlocks) {
    if (!hitObjectId) hitObjectId = block.dataset.objectId || '';
    const titleEl = block.querySelector('.step-label, .wb-sketch-title');
    const stepTitle = titleEl ? (titleEl.textContent || '').trim() : '';

    // Check if the student circled inside a hand-drawn sketch block
    if (block.querySelector('.wb-sketch-canvas') && !block.querySelector('.math-render-host')) {
      return {
        text: stepTitle || 'Dibujo en pizarra',
        objectId: block.dataset.objectId || hitObjectId,
        stepTitle,
        lineContext: (block.textContent || '').replace(/\s+/g, ' ').trim().slice(0, 220),
        pdfNormBox: null,
        sourceKind: 'sketch_diagram',
      };
    }

    const searchRoot = block.querySelector('.math-render-host') || block;
    const hitChars = [];
    const walker = document.createTreeWalker(searchRoot, NodeFilter.SHOW_TEXT, {
      acceptNode(n) {
        const p = n.parentElement;
        if (!p || !n.nodeValue || !n.nodeValue.trim()) return NodeFilter.FILTER_REJECT;
        if (p.closest('.step-actions-mini, .verify-foot, .wb-pen-nib')) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT;
      },
    });
    let node;
    while ((node = walker.nextNode())) {
      const txt = node.nodeValue || '';
      for (let i = 0; i < txt.length; i++) {
        const ch = txt[i];
        if (!ch.trim()) continue;
        try {
          const range = document.createRange();
          range.setStart(node, i);
          range.setEnd(node, i + 1);
          const rr = range.getBoundingClientRect();
          if (rr.width <= 0 || rr.height <= 0) continue;
          // Require character center (or >= 45% area overlap) inside clientBox so neighboring chars aren't grabbed accidentally!
          const chCx = (rr.left + rr.right) * 0.5;
          const chCy = (rr.top + rr.bottom) * 0.5;
          const centerInside = chCx >= clientBox.left && chCx <= clientBox.right && chCy >= clientBox.top && chCy <= clientBox.bottom;
          const ovW = Math.max(0, Math.min(rr.right, clientBox.right) - Math.max(rr.left, clientBox.left));
          const ovH = Math.max(0, Math.min(rr.bottom, clientBox.bottom) - Math.max(rr.top, clientBox.top));
          const fracOverlap = (ovW * ovH) / Math.max(1, rr.width * rr.height);
          if (centerInside || fracOverlap >= 0.45) {
            hitChars.push({ ch, rect: rr });
          }
        } catch {}
      }
    }

    if (hitChars.length) {
      let joined = '';
      for (let i = 0; i < hitChars.length; i++) {
        const cur = hitChars[i];
        if (i === 0) {
          joined = cur.ch;
        } else {
          const prev = hitChars[i - 1];
          const sameLine = Math.abs(cur.rect.top - prev.rect.top) < Math.max(12, prev.rect.height * 0.65);
          const gap = cur.rect.left - prev.rect.right;
          joined += (sameLine && gap >= -3 && gap <= 4.5) ? cur.ch : ` ${cur.ch}`;
        }
      }
      const lineContext = (searchRoot.textContent || '').replace(/\s+/g, ' ').trim().slice(0, 260);
      return {
        text: joined.trim(),
        objectId: block.dataset.objectId || hitObjectId,
        stepTitle,
        lineContext,
        pdfNormBox: hitPdfNormBox,
        sourceKind: 'whiteboard_step',
      };
    }
  }

  return {
    text: '',
    objectId: hitObjectId,
    stepTitle: '',
    lineContext: '',
    pdfNormBox: hitPdfNormBox,
    sourceKind: hitPdfNormBox ? 'pdf_statement' : 'board_region',
  };
}

/** Extract the symbol/word located immediately around a pointer click or arrow tip. */
export function extractSymbolAtClientPoint(rootEl, clientX, clientY, cropWordsByObjectId = new Map()) {
  for (const pad of [14, 28]) {
    const box = {
      left: clientX - pad,
      top: clientY - pad,
      right: clientX + pad,
      bottom: clientY + pad,
    };
    const res = extractTextInsideClientRect(rootEl, box, cropWordsByObjectId);
    if (res && res.text) return res;
  }
  return extractTextInsideClientRect(rootEl, {
    left: clientX - 24,
    top: clientY - 24,
    right: clientX + 24,
    bottom: clientY + 24,
  }, cropWordsByObjectId);
}

/** Return current CSS zoom scale factor of `stageEl` (1.0 when unzoomed). */
export function getStageScale(stageEl) {
  if (!stageEl) return 1;
  const layoutW = stageEl.offsetWidth || stageEl.clientWidth || 1;
  const rectW = stageEl.getBoundingClientRect().width;
  const s = rectW > 0 && layoutW > 0 ? rectW / layoutW : 1;
  return Number.isFinite(s) && s > 0.1 ? s : 1;
}

function buildStageRelativeRect(stageEl, clientRect) {
  const sr = stageEl.getBoundingClientRect();
  const scale = getStageScale(stageEl);
  const left = (clientRect.left - sr.left) / scale;
  const top = (clientRect.top - sr.top) / scale;
  const width = clientRect.width / scale;
  const height = clientRect.height / scale;
  return {
    left,
    top,
    right: left + width,
    bottom: top + height,
    width,
    height,
    cx: left + width * 0.5,
    cy: top + height * 0.5,
  };
}

/**
 * Find ALL stage-relative bounding rects matching `matchText` inside `rootEl`,
 * using TreeWalker + Range.getClientRects() across adjacent inline spans.
 */
function findTextMatchRectsInElement(stageEl, rootEl, matchText, { wholeToken = false, caseSensitive = false } = {}) {
  const rawNeedle = String(matchText || '').trim();
  if (!rawNeedle || !rootEl) return [];

  const walker = document.createTreeWalker(rootEl, NodeFilter.SHOW_TEXT, {
    acceptNode(n) {
      const parent = n.parentElement;
      if (!parent) return NodeFilter.FILTER_REJECT;
      if (parent.closest('.step-actions-mini, .verify-foot, .wb-pen-nib, .problem-meta-strip')) {
        return NodeFilter.FILTER_REJECT;
      }
      return NodeFilter.FILTER_ACCEPT;
    },
  });

  // Build flat character index -> (node, offset), inserting a virtual separator space
  // across fraction/block boundaries (.m2d-num -> .m2d-den) so "atm" + "mol" never glue into "atmmol".
  const segments = [];
  let flat = '';
  let prevBoundaryEl = null;
  let node;
  while ((node = walker.nextNode())) {
    const val = node.nodeValue || '';
    if (!val) continue;
    const boundaryEl = node.parentElement
      ? (node.parentElement.closest('.m2d-num, .m2d-den, .m2d-frac, .m2d-sqrt-inner, .step-block, p, div') || node.parentElement)
      : null;
    if (prevBoundaryEl && boundaryEl && boundaryEl !== prevBoundaryEl && flat.length > 0 && !/\s$/.test(flat)) {
      flat += ' ';
    }
    prevBoundaryEl = boundaryEl;
    const norm = val.replace(/\u2212/g, '-').replace(/\u00a0/g, ' ');
    const startIdx = flat.length;
    flat += norm;
    segments.push({ node, startIdx, endIdx: flat.length, rawLen: val.length });
  }
  if (!flat) return [];

  const needleNorm = normalizeMathStr(rawNeedle);
  const needleCompact = needleNorm.replace(/\s+/g, '');

  const flags = caseSensitive ? 'g' : 'gi';
  let regex;
  if (wholeToken) {
    // Strict token boundary for physical units & symbols: 'L' NEVER matches 'l' in 'mol'
    const esc = escapeRegExp(needleNorm).replace(/\s+/g, '\\s*');
    const startsAlpha = /^[A-Za-zÀ-ÿ]/.test(needleNorm);
    const endsAlpha = /[A-Za-zÀ-ÿ]$/.test(needleNorm);
    const prefix = startsAlpha ? '(?<![A-Za-zÀ-ÿ])' : '';
    const suffix = endsAlpha ? '(?![A-Za-zÀ-ÿ])' : '';
    regex = new RegExp(`${prefix}${esc}${suffix}`, flags);
  } else {
    const esc = escapeRegExp(needleNorm).replace(/\s+/g, '\\s*');
    regex = new RegExp(esc, flags);
  }

  const matches = [];
  let m;
  while ((m = regex.exec(flat)) !== null) {
    if (m[0].length === 0) {
      regex.lastIndex++;
      continue;
    }
    matches.push({ start: m.index, end: m.index + m[0].length });
  }

  // Fallback for compact math matches (e.g. '1.50atm' vs '1.50 atm' or 'P_1' vs 'P1')
  if (!matches.length && !wholeToken && needleCompact.length >= 1) {
    const cleaned = needleCompact.replace(/[_^{}\\]/g, '');
    if (cleaned.length >= 1) {
      const flexPattern = cleaned.split('').map(escapeRegExp).join('\\s*');
      const flexRe = new RegExp(flexPattern, 'gi');
      while ((m = flexRe.exec(flat)) !== null) {
        if (m[0].length === 0) { flexRe.lastIndex++; continue; }
        matches.push({ start: m.index, end: m.index + m[0].length });
      }
    }
  }

  function locatePos(charIdx) {
    for (let i = 0; i < segments.length; i++) {
      const seg = segments[i];
      if (charIdx >= seg.startIdx && charIdx <= seg.endIdx) {
        const off = Math.max(0, Math.min(seg.rawLen, charIdx - seg.startIdx));
        return { node: seg.node, offset: off };
      }
    }
    const last = segments[segments.length - 1];
    return { node: last.node, offset: last.rawLen };
  }

  const out = [];
  for (const span of matches) {
    try {
      const p0 = locatePos(span.start);
      const p1 = locatePos(span.end);
      const range = document.createRange();
      range.setStart(p0.node, p0.offset);
      range.setEnd(p1.node, p1.offset);
      const clientRects = Array.from(range.getClientRects()).filter((r) => r.width > 0.5 && r.height > 0.5);
      if (!clientRects.length) continue;
      let minL = Infinity, minT = Infinity, maxR = -Infinity, maxB = -Infinity;
      for (const cr of clientRects) {
        minL = Math.min(minL, cr.left);
        minT = Math.min(minT, cr.top);
        maxR = Math.max(maxR, cr.right);
        maxB = Math.max(maxB, cr.bottom);
      }
      if (maxR > minL && maxB > minT) {
        out.push({
          rect: buildStageRelativeRect(stageEl, {
            left: minL,
            top: minT,
            right: maxR,
            bottom: maxB,
            width: maxR - minL,
            height: maxB - minT,
          }),
          range,
          clientRect: { left: minL, top: minT, right: maxR, bottom: maxB },
          element: p0.node.parentElement,
        });
      }
    } catch {}
  }
  return out;
}

export class MarkerAnnotationsEngine {
  constructor(stageEl, svgLayerEl, { reduceMotion = false, onEnsureVisible = null } = {}) {
    this.stageEl = stageEl;
    this.svgEl = svgLayerEl;
    this.reduceMotion = Boolean(reduceMotion);
    this.onEnsureVisible = typeof onEnsureVisible === 'function' ? onEnsureVisible : null;

    this._records = []; // persisted & temporary SVG annotations
    this._colorizedSpans = [];
    this._stepGeneration = 0;
    this._lastPointerPos = { x: 80, y: 80 };
    this._laserTimer = null;

    // Shared moving marker nib + gliding laser dot inside stage
    this._nibEl = document.getElementById('prof-marker-cursor');
    if (!this._nibEl) {
      this._nibEl = document.createElement('span');
      this._nibEl.id = 'prof-marker-cursor';
      this._nibEl.className = 'prof-marker-cursor';
      this._nibEl.hidden = true;
      this.stageEl.appendChild(this._nibEl);
    }

    this._laserEl = document.getElementById('prof-laser-dot');
    if (!this._laserEl) {
      this._laserEl = document.createElement('span');
      this._laserEl.id = 'prof-laser-dot';
      this._laserEl.className = 'prof-laser-dot';
      this._laserEl.hidden = true;
      this.stageEl.appendChild(this._laserEl);
    }

    this._resizeObs = new ResizeObserver(() => this._syncSvgBounds());
    this._resizeObs.observe(this.stageEl);
    this._syncSvgBounds();
  }

  setReduceMotion(val) {
    this.reduceMotion = Boolean(val);
  }

  _syncSvgBounds() {
    if (!this.svgEl || !this.stageEl) return;
    const w = Math.max(320, this.stageEl.scrollWidth || this.stageEl.clientWidth || 1000);
    const h = Math.max(320, this.stageEl.scrollHeight || this.stageEl.clientHeight || 600);
    this.svgEl.setAttribute('width', String(w));
    this.svgEl.setAttribute('height', String(h));
    this.svgEl.setAttribute('viewBox', `0 0 ${w} ${h}`);
  }

  _findBlockEl(objectId) {
    if (objectId) {
      const exact = this.stageEl.querySelector(`[data-object-id="${CSS.escape(objectId)}"]`);
      if (exact) return exact;
    }
    // Fallback to latest math step block
    const mathBlocks = this.stageEl.querySelectorAll('.step-block[data-object-id]');
    for (let i = mathBlocks.length - 1; i >= 0; i--) {
      if (mathBlocks[i].querySelector('.math-render-host')) return mathBlocks[i];
    }
    return mathBlocks.length ? mathBlocks[mathBlocks.length - 1] : null;
  }

  /**
   * Resolve 1 or ALL stage-relative target rects for an annotation payload.
   */
  resolveTargets(payload = {}, { all = false, wholeToken = false, caseSensitive = false } = {}) {
    this._syncSvgBounds();
    const oid = payload.target_object_id || payload.object_id || '';
    const block = this._findBlockEl(oid);

    // 1. Exact PyMuPDF normalized box on a cropped PDF problem or pasted image (object-fit: contain aware!)
    const normBox = payload.norm_box || payload.source_norm_box;
    if (normBox && typeof normBox === 'object' && typeof normBox.x === 'number') {
      const img =
        (block && block.querySelector('.problem-clip-img, img')) ||
        this.stageEl.querySelector('.problem-clip-img, #media-layer-zone img');
      if (img) {
        const ir = getContainedImageClientRect(img);
        if (ir.width > 12 && ir.height > 12) {
          const rect = buildStageRelativeRect(this.stageEl, {
            left: ir.left + normBox.x * ir.width,
            top: ir.top + normBox.y * ir.height,
            right: ir.left + (normBox.x + normBox.w) * ir.width,
            bottom: ir.top + (normBox.y + normBox.h) * ir.height,
            width: normBox.w * ir.width,
            height: normBox.h * ir.height,
          });
          return [{ rect, element: img, range: null }];
        }
      }
    }

    // 2. Explicit \class{term-...} inside target block
    if (payload.term_class && block) {
      const els = Array.from(block.querySelectorAll(`.${CSS.escape(payload.term_class)}`));
      if (els.length) {
        const res = els.map((el) => ({
          rect: buildStageRelativeRect(this.stageEl, el.getBoundingClientRect()),
          element: el,
          range: null,
        }));
        return all ? res : [res[0]];
      }
    }

    // 3. Exact text/symbol match via TreeWalker + Range.getClientRects()
    const matchText = String(payload.match_text || payload.target_text || payload.text || '').trim();
    if (matchText) {
      const scopes = [];
      if (block) {
        const mh = block.querySelector('.math-render-host');
        if (mh) scopes.push(mh);
        scopes.push(block);
      }
      scopes.push(this.stageEl);

      for (const sc of scopes) {
        const hits = findTextMatchRectsInElement(this.stageEl, sc, matchText, {
          wholeToken,
          caseSensitive,
        });
        if (hits.length) {
          return all ? hits : [hits[0]];
        }
      }
    }

    // 4. Fallback to the math formula host or block itself
    if (block) {
      const anchor = block.querySelector('.math-render-host') || block;
      const ar = anchor.getBoundingClientRect();
      if (ar.width > 4 && ar.height > 4) {
        return [{ rect: buildStageRelativeRect(this.stageEl, ar), element: anchor, range: null }];
      }
    }
    return [];
  }

  _animatePathWithNib(pathEl, hexColor, durationMs = 420) {
    let totalLen = 0;
    try { totalLen = pathEl.getTotalLength(); } catch { totalLen = 0; }
    if (!totalLen || this.reduceMotion || durationMs <= 0) {
      return Promise.resolve();
    }

    pathEl.style.strokeDasharray = `${totalLen}`;
    pathEl.style.strokeDashoffset = `${totalLen}`;

    const nib = this._nibEl;
    if (nib) {
      nib.style.background = hexColor;
      nib.style.boxShadow = `0 0 0 5px ${hexColor}44`;
      nib.hidden = false;
    }

    return new Promise((resolve) => {
      const start = performance.now();
      const dur = Math.max(140, Math.min(1800, durationMs));
      const step = (now) => {
        const p = Math.min(1, (now - start) / dur);
        const eased = 1 - Math.pow(1 - p, 2.1);
        pathEl.style.strokeDashoffset = `${(totalLen * (1 - eased)).toFixed(1)}`;
        try {
          const pt = pathEl.getPointAtLength(totalLen * eased);
          if (nib) {
            nib.style.left = `${pt.x.toFixed(1)}px`;
            nib.style.top = `${pt.y.toFixed(1)}px`;
          }
          this._lastPointerPos = { x: pt.x, y: pt.y };
        } catch {}
        if (p < 1) {
          requestAnimationFrame(step);
        } else {
          pathEl.style.strokeDasharray = '';
          pathEl.style.strokeDashoffset = '';
          if (nib) nib.hidden = true;
          resolve();
        }
      };
      requestAnimationFrame(step);
    });
  }

  _registerGroup(groupEl, { colorName, isHighlighter = false, temporary = false, ttlMs = 0 }) {
    const rec = {
      groupEl,
      colorName: colorName || 'blue',
      isHighlighter,
      temporary: Boolean(temporary),
      generation: this._stepGeneration,
      timerId: null,
    };
    this._records.push(rec);
    if (temporary && ttlMs > 0) {
      rec.timerId = setTimeout(() => this._fadeAndRemoveRecord(rec), ttlMs);
    }
    return rec;
  }

  _fadeAndRemoveRecord(rec) {
    if (!rec || !rec.groupEl) return;
    if (rec.timerId) {
      clearTimeout(rec.timerId);
      rec.timerId = null;
    }
    rec.groupEl.classList.add('marker-fade-out');
    setTimeout(() => {
      try { rec.groupEl.remove(); } catch {}
      const idx = this._records.indexOf(rec);
      if (idx >= 0) this._records.splice(idx, 1);
    }, 620);
  }

  _createHandNoteTag(x, y, text, hexColor) {
    if (!text) return null;
    const fo = document.createElementNS(SVG_NS, 'foreignObject');
    const maxW = 240;
    const stageW = this.stageEl.scrollWidth || 960;
    const clampedX = Math.max(8, Math.min(stageW - maxW - 10, x));
    let clampedY = Math.max(6, y);

    // Avoid colliding with any existing visible badge in the SVG overlay
    const existing = Array.from(this.svgEl.querySelectorAll('.marker-note-fo'));
    for (let attempt = 0; attempt < 4; attempt++) {
      const collides = existing.some((other) => {
        const ox = parseFloat(other.getAttribute('x') || '0');
        const oy = parseFloat(other.getAttribute('y') || '0');
        return Math.abs(ox - clampedX) < 150 && Math.abs(oy - clampedY) < 34;
      });
      if (!collides) break;
      clampedY += 36;
    }

    fo.setAttribute('x', clampedX.toFixed(1));
    fo.setAttribute('y', clampedY.toFixed(1));
    fo.setAttribute('width', String(maxW));
    fo.setAttribute('height', '68');
    fo.setAttribute('class', 'marker-note-fo');

    const pill = document.createElement('div');
    pill.className = 'marker-hand-badge';
    pill.style.borderColor = hexColor;
    pill.style.color = hexColor;
    pill.textContent = text;
    fo.appendChild(pill);
    return fo;
  }

  /** Gesture 1: Hand-drawn colored underline (solid | wavy | double) */
  async underline(payload = {}) {
    const targets = this.resolveTargets(payload);
    if (!targets.length) return false;
    const { rect } = targets[0];
    const colorName = payload.color || 'red';
    const hex = resolveMarkerHex(colorName);
    const style = String(payload.style || 'solid').toLowerCase();

    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = colorName;

    const x0 = rect.left - 3;
    const x1 = rect.right + 3;
    const yBase = rect.bottom + 3;
    const paths = [];

    if (style === 'wavy') {
      const span = Math.max(16, x1 - x0);
      const waves = Math.max(3, Math.round(span / 12));
      const step = span / waves;
      let d = `M ${x0.toFixed(1)} ${yBase.toFixed(1)}`;
      for (let i = 0; i < waves; i++) {
        const xm = x0 + step * (i + 0.5);
        const xe = x0 + step * (i + 1);
        const amp = i % 2 === 0 ? -3.4 : 3.4;
        d += ` Q ${xm.toFixed(1)} ${(yBase + amp).toFixed(1)} ${xe.toFixed(1)} ${yBase.toFixed(1)}`;
      }
      paths.push(d);
    } else if (style === 'double') {
      const mid = (x0 + x1) * 0.5;
      paths.push(`M ${x0.toFixed(1)} ${(yBase + 0.5).toFixed(1)} Q ${mid.toFixed(1)} ${(yBase + 2.4).toFixed(1)} ${x1.toFixed(1)} ${(yBase - 0.8).toFixed(1)}`);
      paths.push(`M ${(x0 + 2).toFixed(1)} ${(yBase + 5.2).toFixed(1)} Q ${mid.toFixed(1)} ${(yBase + 6.8).toFixed(1)} ${(x1 - 1).toFixed(1)} ${(yBase + 4.4).toFixed(1)}`);
    } else {
      const mid = (x0 + x1) * 0.5;
      paths.push(`M ${x0.toFixed(1)} ${(yBase + 1).toFixed(1)} Q ${mid.toFixed(1)} ${(yBase + 3.2).toFixed(1)} ${x1.toFixed(1)} ${(yBase - 0.6).toFixed(1)}`);
    }

    const svgPaths = paths.map((d) => {
      const p = document.createElementNS(SVG_NS, 'path');
      p.setAttribute('d', d);
      p.setAttribute('fill', 'none');
      p.setAttribute('stroke', hex);
      p.setAttribute('stroke-width', '3.1');
      p.setAttribute('stroke-linecap', 'round');
      p.setAttribute('stroke-linejoin', 'round');
      g.appendChild(p);
      return p;
    });

    if (payload.note_es) {
      const tag = this._createHandNoteTag(rect.left, yBase + 8, payload.note_es, hex);
      if (tag) g.appendChild(tag);
    }

    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName,
      temporary: Boolean(payload.temporary),
      ttlMs: Number(payload.ttl_ms || 0),
    });

    for (const sp of svgPaths) {
      await this._animatePathWithNib(sp, hex, 360);
    }
    return true;
  }

  /** Gesture 2: Hand-drawn organic 385-degree oval circle */
  async circle(payload = {}) {
    const targets = this.resolveTargets(payload);
    if (!targets.length) return false;
    const { rect } = targets[0];
    const colorName = payload.color || 'orange';
    const hex = resolveMarkerHex(colorName);

    const rx = Math.max(18, rect.width * 0.5 + 10);
    const ry = Math.max(14, rect.height * 0.5 + 7);
    const cx = rect.cx;
    const cy = rect.cy;

    // 385-degree organic closed loop with subtle human wobble and overshoot
    const steps = 28;
    const totalAngle = (385 * Math.PI) / 180;
    const startAngle = -Math.PI * 0.58;
    const pts = [];
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const ang = startAngle + totalAngle * t;
      const wobR = 1 + 0.045 * Math.sin(ang * 3 + 0.7) + (t > 0.85 ? 0.04 * (t - 0.85) : 0);
      const px = cx + Math.cos(ang) * rx * wobR;
      const py = cy + Math.sin(ang) * ry * wobR;
      pts.push([px, py]);
    }
    let d = `M ${pts[0][0].toFixed(1)} ${pts[0][1].toFixed(1)}`;
    for (let i = 1; i < pts.length; i++) {
      const prev = pts[i - 1];
      const cur = pts[i];
      const mx = ((prev[0] + cur[0]) * 0.5).toFixed(1);
      const my = ((prev[1] + cur[1]) * 0.5).toFixed(1);
      d += ` Q ${prev[0].toFixed(1)} ${prev[1].toFixed(1)} ${mx} ${my}`;
    }

    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = colorName;

    const path = document.createElementNS(SVG_NS, 'path');
    path.setAttribute('d', d);
    path.setAttribute('fill', 'none');
    path.setAttribute('stroke', hex);
    path.setAttribute('stroke-width', '3.0');
    path.setAttribute('stroke-linecap', 'round');
    path.setAttribute('stroke-linejoin', 'round');
    g.appendChild(path);

    if (payload.note_es) {
      const tag = this._createHandNoteTag(cx + rx + 6, cy - 18, payload.note_es, hex);
      if (tag) g.appendChild(tag);
    }

    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName,
      temporary: Boolean(payload.temporary),
      ttlMs: Number(payload.ttl_ms || 0),
    });
    await this._animatePathWithNib(path, hex, 490);
    return true;
  }

  /** Gesture 3: Translucent Stabilo-style highlighter sweep */
  async highlight(payload = {}) {
    const targets = this.resolveTargets(payload);
    if (!targets.length) return false;
    const { rect } = targets[0];
    const colorName = payload.color || 'yellow';
    const hex = resolveMarkerHex(colorName, { isHighlighter: true });

    const yMid = rect.cy;
    const x0 = rect.left - 4;
    const x1 = rect.right + 4;
    const strokeW = Math.max(16, Math.min(54, rect.height * 0.92));

    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group marker-highlighter');
    g.dataset.colorName = colorName;

    const path = document.createElementNS(SVG_NS, 'path');
    path.setAttribute('d', `M ${x0.toFixed(1)} ${(yMid + 1).toFixed(1)} Q ${((x0 + x1) * 0.5).toFixed(1)} ${(yMid - 1.5).toFixed(1)} ${x1.toFixed(1)} ${yMid.toFixed(1)}`);
    path.setAttribute('fill', 'none');
    path.setAttribute('stroke', hex);
    path.setAttribute('stroke-width', strokeW.toFixed(1));
    path.setAttribute('stroke-opacity', isDarkTheme() ? '0.28' : '0.34');
    path.setAttribute('stroke-linecap', 'round');
    g.appendChild(path);

    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName,
      isHighlighter: true,
      temporary: Boolean(payload.temporary),
      ttlMs: Number(payload.ttl_ms || 0),
    });
    await this._animatePathWithNib(path, hex, 350);
    return true;
  }

  /** Gesture 4: Live semantic ink colorizing of a term/symbol */
  async colorize(payload = {}) {
    const targets = this.resolveTargets(payload, { all: Boolean(payload.all) });
    if (!targets.length) return false;
    const colorName = payload.color || 'red';
    const hex = resolveMarkerHex(colorName);

    for (const hit of targets) {
      let targetSpan = null;
      if (hit.range && hit.range.startContainer === hit.range.endContainer && hit.range.startContainer.nodeType === Node.TEXT_NODE) {
        try {
          const sp = document.createElement('span');
          sp.className = 'marker-colorized-term';
          hit.range.surroundContents(sp);
          targetSpan = sp;
        } catch {}
      }
      if (!targetSpan && hit.element && hit.element !== this.stageEl) {
        targetSpan = hit.element;
      }
      if (targetSpan) {
        targetSpan.dataset.markerColorName = colorName;
        targetSpan.style.transition = 'color 260ms ease, transform 240ms ease';
        targetSpan.style.color = hex;
        targetSpan.style.fontWeight = '700';
        this._colorizedSpans.push(targetSpan);
        if (!this.reduceMotion) {
          targetSpan.classList.add('marker-color-pop');
          setTimeout(() => targetSpan.classList.remove('marker-color-pop'), 520);
        }
      }
    }
    return true;
  }

  /** Gesture 5: Gliding laser pointer with concentric ripple */
  async point(payload = {}) {
    const targets = this.resolveTargets(payload);
    if (!targets.length) return false;
    const { rect } = targets[0];
    const colorName = payload.color || 'red';
    const hex = resolveMarkerHex(colorName);

    const laser = this._laserEl;
    if (laser) {
      laser.style.background = hex;
      laser.style.boxShadow = `0 0 14px 4px ${hex}88`;
      laser.hidden = false;
      laser.style.left = `${this._lastPointerPos.x.toFixed(1)}px`;
      laser.style.top = `${this._lastPointerPos.y.toFixed(1)}px`;
      // Trigger smooth CSS glide to target center
      requestAnimationFrame(() => {
        laser.style.left = `${rect.cx.toFixed(1)}px`;
        laser.style.top = `${rect.cy.toFixed(1)}px`;
      });
      this._lastPointerPos = { x: rect.cx, y: rect.cy };
      if (this._laserTimer) clearTimeout(this._laserTimer);
      this._laserTimer = setTimeout(() => { laser.hidden = true; }, Number(payload.ttl_ms || 2400));
    }

    // Concentric ripple rings
    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = colorName;
    for (let rIdx = 0; rIdx < 2; rIdx++) {
      const c = document.createElementNS(SVG_NS, 'circle');
      c.setAttribute('cx', rect.cx.toFixed(1));
      c.setAttribute('cy', rect.cy.toFixed(1));
      c.setAttribute('r', String(14 + rIdx * 10));
      c.setAttribute('fill', 'none');
      c.setAttribute('stroke', hex);
      c.setAttribute('stroke-width', '2.2');
      c.setAttribute('class', rIdx === 0 ? 'laser-ring-1' : 'laser-ring-2');
      g.appendChild(c);
    }
    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName,
      temporary: true,
      ttlMs: Number(payload.ttl_ms || 2400),
    });
    return true;
  }

  /** Gesture 6: Hand-drawn curved origin arrow between two rects or refs */
  async drawArrowBetweenRects(fromRect, toRect, { color = 'orange', ruleEs = '', temporary = true, ttlMs = 9500 } = {}) {
    if (!fromRect || !toRect) return false;
    const hex = resolveMarkerHex(color);
    const stageW = this.stageEl.scrollWidth || 960;

    // Choose the cleaner side (right margin arc if stacked vertically, or direct curved path)
    const verticalGap = Math.abs(toRect.cy - fromRect.cy);
    let x1, y1, x2, y2, cx1, cy1, cx2, cy2;
    if (verticalGap > 42) {
      x1 = Math.min(stageW - 60, fromRect.right + 8);
      y1 = fromRect.cy;
      x2 = Math.min(stageW - 60, toRect.right + 8);
      y2 = toRect.cy;
      const bow = Math.min(stageW - 22, Math.max(x1, x2) + Math.min(95, 42 + verticalGap * 0.18));
      cx1 = bow;
      cy1 = y1 + (y2 - y1) * 0.22;
      cx2 = bow;
      cy2 = y2 - (y2 - y1) * 0.22;
    } else {
      x1 = fromRect.cx;
      y1 = fromRect.bottom + 6;
      x2 = toRect.cx;
      y2 = toRect.bottom + 6;
      const dip = Math.max(y1, y2) + 36;
      cx1 = x1 + (x2 - x1) * 0.28;
      cy1 = dip;
      cx2 = x1 + (x2 - x1) * 0.72;
      cy2 = dip;
    }

    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = color;

    const path = document.createElementNS(SVG_NS, 'path');
    path.setAttribute('d', `M ${x1.toFixed(1)} ${y1.toFixed(1)} C ${cx1.toFixed(1)} ${cy1.toFixed(1)}, ${cx2.toFixed(1)} ${cy2.toFixed(1)}, ${x2.toFixed(1)} ${y2.toFixed(1)}`);
    path.setAttribute('fill', 'none');
    path.setAttribute('stroke', hex);
    path.setAttribute('stroke-width', '2.8');
    path.setAttribute('stroke-linecap', 'round');
    g.appendChild(path);

    // Hand-drawn arrowhead at (x2, y2) pointing along end tangent (cx2,cy2)->(x2,y2)
    const ang = Math.atan2(y2 - cy2, x2 - cx2);
    const hl = 11;
    const h1x = x2 - hl * Math.cos(ang - 0.52);
    const h1y = y2 - hl * Math.sin(ang - 0.52);
    const h2x = x2 - hl * Math.cos(ang + 0.52);
    const h2y = y2 - hl * Math.sin(ang + 0.52);
    const head = document.createElementNS(SVG_NS, 'path');
    head.setAttribute('d', `M ${h1x.toFixed(1)} ${h1y.toFixed(1)} L ${x2.toFixed(1)} ${y2.toFixed(1)} L ${h2x.toFixed(1)} ${h2y.toFixed(1)}`);
    head.setAttribute('fill', 'none');
    head.setAttribute('stroke', hex);
    head.setAttribute('stroke-width', '2.8');
    head.setAttribute('stroke-linecap', 'round');
    head.setAttribute('stroke-linejoin', 'round');
    g.appendChild(head);

    if (ruleEs) {
      const midX = (cx1 + cx2) * 0.5 - 110;
      const midY = (y1 + y2) * 0.5 - 22;
      const tag = this._createHandNoteTag(midX, midY, ruleEs, hex);
      if (tag) g.appendChild(tag);
    }

    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName: color,
      temporary: Boolean(temporary),
      ttlMs: Number(ttlMs || 9500),
    });
    await this._animatePathWithNib(path, hex, 520);
    return true;
  }

  async traceOrigin(payload = {}) {
    const parseRef = (ref) => {
      const s = String(ref || '').trim();
      if (!s) return null;
      if (s.includes(':')) {
        const [oid, rest] = s.split(':', 2);
        if (rest.startsWith('term-')) {
          return this.resolveTargets({ target_object_id: oid, term_class: rest })[0];
        }
        return this.resolveTargets({ target_object_id: oid, match_text: rest })[0];
      }
      const byObj = this.stageEl.querySelector(`[data-object-id="${CSS.escape(s)}"]`);
      if (byObj) return this.resolveTargets({ target_object_id: s })[0];
      return this.resolveTargets({ match_text: s })[0];
    };

    const fromHit = parseRef(payload.from_ref);
    const toHit = parseRef(payload.to_ref);
    if (!fromHit || !toHit) return false;

    const color = payload.color || 'red';
    const temporary = payload.temporary !== false;
    const ttlMs = Number(payload.ttl_ms || 9000);
    await this._circleRect(fromHit.rect, { color, temporary, ttlMs }).catch(() => {});
    return this.drawArrowBetweenRects(fromHit.rect, toHit.rect, {
      color,
      ruleEs: payload.rule_es || '',
      temporary,
      ttlMs,
    });
  }

  /** Gesture 7: Handwritten margin note with short leader arrow */
  async marginNote(payload = {}) {
    const targets = this.resolveTargets(payload);
    if (!targets.length) return false;
    const { rect } = targets[0];
    const colorName = payload.color || 'teal';
    const hex = resolveMarkerHex(colorName);
    const text = String(payload.note_es || payload.text || '').trim();
    if (!text) return false;

    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = colorName;

    const x0 = rect.right + 8;
    const y0 = rect.cy;
    const x1 = x0 + 34;
    const y1 = y0 - 8;
    const tick = document.createElementNS(SVG_NS, 'path');
    tick.setAttribute('d', `M ${x0.toFixed(1)} ${y0.toFixed(1)} Q ${(x0 + 16).toFixed(1)} ${(y0 - 12).toFixed(1)} ${x1.toFixed(1)} ${y1.toFixed(1)}`);
    tick.setAttribute('fill', 'none');
    tick.setAttribute('stroke', hex);
    tick.setAttribute('stroke-width', '2.3');
    tick.setAttribute('stroke-linecap', 'round');
    g.appendChild(tick);

    const tag = this._createHandNoteTag(x1 + 4, y1 - 16, text, hex);
    if (tag) g.appendChild(tag);

    this.svgEl.appendChild(g);
    this._registerGroup(g, {
      colorName,
      temporary: Boolean(payload.temporary),
      ttlMs: Number(payload.ttl_ms || 0),
    });
    await this._animatePathWithNib(tick, hex, 300);
    return true;
  }

  /**
   * Gesture 8: Colored diagonal unit & factor cancellation (`/`).
   * Slashes matching numerator/denominator occurrences pair-by-pair in distinct colors
   * (e.g., `atm` red, `L` green without touching `mol`, `mol` purple) and optionally
   * circles the surviving unit (e.g., `K` blue).
   */
  async cancelTerms(payload = {}) {
    const pairs = Array.isArray(payload.pairs) ? payload.pairs : [];
    const paletteCycle = ['red', 'green', 'purple', 'orange', 'teal', 'pink', 'blue'];
    const targetOid = payload.target_object_id || payload.object_id || '';
    const temporary = Boolean(payload.temporary);
    const ttlMs = Number(payload.ttl_ms || 0);

    if (!pairs.length && payload.match_text) {
      pairs.push({ unit_or_term: payload.match_text, color: payload.color || 'red' });
    }

    let drewAny = false;
    for (let pIdx = 0; pIdx < pairs.length; pIdx++) {
      const pair = pairs[pIdx] || {};
      const unitStr = String(pair.unit_or_term || pair.unit || pair.term || '').trim();
      if (!unitStr) continue;
      const colorName = pair.color || paletteCycle[pIdx % paletteCycle.length];
      const hex = resolveMarkerHex(colorName);

      // Strict whole-token + case-sensitive matching for physical units so 'L' NEVER slashes 'l' in 'mol'
      const hits = this.resolveTargets(
        { target_object_id: targetOid, match_text: unitStr },
        { all: true, wholeToken: true, caseSensitive: true },
      );
      if (!hits.length) continue;

      const g = document.createElementNS(SVG_NS, 'g');
      g.setAttribute('class', 'marker-group marker-cancel-pair');
      g.dataset.colorName = colorName;
      this.svgEl.appendChild(g);
      this._registerGroup(g, { colorName, temporary, ttlMs });

      for (let hIdx = 0; hIdx < hits.length; hIdx++) {
        const { rect } = hits[hIdx];
        const x0 = rect.left - 2;
        const y0 = rect.bottom + 1;
        const x1 = rect.right + 2.5;
        const y1 = rect.top - 1;
        const mx = (x0 + x1) * 0.5 - 1.2;
        const my = (y0 + y1) * 0.5 + 1.0;

        const slash = document.createElementNS(SVG_NS, 'path');
        slash.setAttribute('d', `M ${x0.toFixed(1)} ${y0.toFixed(1)} Q ${mx.toFixed(1)} ${my.toFixed(1)} ${x1.toFixed(1)} ${y1.toFixed(1)}`);
        slash.setAttribute('fill', 'none');
        slash.setAttribute('stroke', hex);
        slash.setAttribute('stroke-width', '2.85');
        slash.setAttribute('stroke-linecap', 'round');
        g.appendChild(slash);

        if (pair.replacement_note) {
          const lbl = document.createElementNS(SVG_NS, 'text');
          lbl.setAttribute('x', (x1 + 2).toFixed(1));
          lbl.setAttribute('y', (y1 + 2).toFixed(1));
          lbl.setAttribute('fill', hex);
          lbl.setAttribute('font-family', '"Caveat", "Patrick Hand", cursive');
          lbl.setAttribute('font-size', '18');
          lbl.setAttribute('font-weight', '700');
          lbl.textContent = pair.replacement_note;
          g.appendChild(lbl);
        }

        await this._animatePathWithNib(slash, hex, 230);
        drewAny = true;
      }
    }

    // Circle the surviving unit at the end (e.g., 'K' in blue)
    const surv = String(payload.surviving_unit || '').trim();
    if (surv) {
      const survHits = this.resolveTargets(
        { target_object_id: targetOid, match_text: surv },
        { all: true, wholeToken: true, caseSensitive: true },
      );
      if (survHits.length) {
        const lastHit = survHits[survHits.length - 1];
        await this._circleRect(lastHit.rect, {
          color: payload.surviving_color || 'blue',
          noteEs: `Queda ${surv}`,
          badgePos: 'above',
          temporary,
          ttlMs,
        });
        drewAny = true;
      }
    }
    return drewAny;
  }

  async _circleRect(rect, { color = 'blue', noteEs = '', badgePos = 'right', temporary = false, ttlMs = 0 } = {}) {
    const hex = resolveMarkerHex(color);
    const rx = Math.max(17, rect.width * 0.5 + 9);
    const ry = Math.max(14, rect.height * 0.5 + 6);
    const cx = rect.cx;
    const cy = rect.cy;
    const steps = 26;
    const totalAngle = (385 * Math.PI) / 180;
    const startAngle = -Math.PI * 0.55;
    const pts = [];
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const ang = startAngle + totalAngle * t;
      const wobR = 1 + 0.04 * Math.sin(ang * 3 + 0.5);
      pts.push([cx + Math.cos(ang) * rx * wobR, cy + Math.sin(ang) * ry * wobR]);
    }
    let d = `M ${pts[0][0].toFixed(1)} ${pts[0][1].toFixed(1)}`;
    for (let i = 1; i < pts.length; i++) {
      const p0 = pts[i - 1];
      const p1 = pts[i];
      d += ` Q ${p0[0].toFixed(1)} ${p0[1].toFixed(1)} ${((p0[0] + p1[0]) * 0.5).toFixed(1)} ${((p0[1] + p1[1]) * 0.5).toFixed(1)}`;
    }
    const g = document.createElementNS(SVG_NS, 'g');
    g.setAttribute('class', 'marker-group');
    g.dataset.colorName = color;
    const path = document.createElementNS(SVG_NS, 'path');
    path.setAttribute('d', d);
    path.setAttribute('fill', 'none');
    path.setAttribute('stroke', hex);
    path.setAttribute('stroke-width', '3.0');
    path.setAttribute('stroke-linecap', 'round');
    g.appendChild(path);
    if (noteEs) {
      const tagX = badgePos === 'above' ? Math.max(8, cx - 34) : (cx + rx + 6);
      const tagY = badgePos === 'above' ? Math.max(4, cy - ry - 36) : (cy - 16);
      const tag = this._createHandNoteTag(tagX, tagY, noteEs, hex);
      if (tag) g.appendChild(tag);
    }
    this.svgEl.appendChild(g);
    this._registerGroup(g, { colorName: color, temporary, ttlMs });
    await this._animatePathWithNib(path, hex, 420);
  }

  /**
   * Composite Visual Data Provenance Tracer ("¿De dónde salió este 1.50 atm / 63.98 K?"):
   * - `statement`: underlines/circles the exact PyMuPDF word box on the PDF crop + circles
   *   target in equation + draws temporary curved origin arrow.
   * - `intermediate_step`: circles the calculated value in the earlier step block + circles
   *   where it is reused + draws temporary curved arrow between steps.
   * - `constant_or_law`: underlines keyword in statement if found + temporary margin note.
   */
  async traceDataProvenance(payload = {}) {
    const originKind = String(payload.origin_kind || 'statement').toLowerCase();
    const color = payload.color || (originKind === 'intermediate_step' ? 'purple' : originKind === 'constant_or_law' ? 'teal' : 'orange');
    const style = String(payload.style || (originKind === 'intermediate_step' ? 'circle' : 'underline')).toLowerCase();
    const noteEs = String(payload.note_es || '').trim();
    const temporary = payload.temporary !== false;
    const ttlMs = Number(payload.ttl_ms || 9500);

    const srcHits = this.resolveTargets({
      target_object_id: payload.source_object_id || '',
      match_text: payload.source_text || payload.value_text || '',
      norm_box: payload.source_norm_box || null,
    });
    const tgtHits = this.resolveTargets({
      target_object_id: payload.target_object_id || '',
      match_text: payload.target_text || payload.value_text || '',
    });

    // Gently ensure source element is visible if panned/scrolled out of view
    if (srcHits.length && srcHits[0].element) {
      if (this.onEnsureVisible) {
        this.onEnsureVisible(srcHits[0].element);
      } else if (typeof srcHits[0].element.scrollIntoView === 'function') {
        const er = srcHits[0].element.getBoundingClientRect();
        if (er.top < 48 || er.bottom > window.innerHeight - 90) {
          srcHits[0].element.scrollIntoView({ behavior: this.reduceMotion ? 'auto' : 'smooth', block: 'nearest' });
        }
      }
    }

    const willDrawArrow = Boolean(srcHits.length && tgtHits.length);

    // 1. Mark source (underline on statement OR colored circle on intermediate step)
    if (srcHits.length) {
      if (originKind === 'intermediate_step' || style === 'circle') {
        await this._circleRect(srcHits[0].rect, {
          color,
          noteEs: willDrawArrow ? '' : (noteEs || 'Calculado aquí'),
          temporary,
          ttlMs,
        });
      } else {
        await this.underline({
          target_object_id: payload.source_object_id || '',
          match_text: payload.source_text || payload.value_text || '',
          norm_box: payload.source_norm_box || null,
          color,
          style: 'double',
          note_es: willDrawArrow ? '' : (noteEs || 'Dato del enunciado'),
          temporary,
          ttl_ms: ttlMs,
        });
      }
    }

    // 2. Mark destination in current equation with matching temporary color
    if (tgtHits.length) {
      await this._circleRect(tgtHits[0].rect, {
        color,
        noteEs: originKind === 'constant_or_law' ? noteEs : '',
        temporary,
        ttlMs,
      });
    }

    // 3. Draw temporary curved origin arrow connecting source -> destination
    if (srcHits.length && tgtHits.length) {
      await this.drawArrowBetweenRects(srcHits[0].rect, tgtHits[0].rect, {
        color,
        ruleEs: noteEs || (originKind === 'statement' ? 'Viene del enunciado' : 'Del paso anterior'),
        temporary,
        ttlMs,
      });
    } else if (originKind === 'constant_or_law' && tgtHits.length && noteEs) {
      await this.marginNote({
        target_object_id: payload.target_object_id || '',
        match_text: payload.target_text || payload.value_text || '',
        color,
        note_es: noteEs,
        temporary,
        ttl_ms: ttlMs,
      });
    }
    return Boolean(srcHits.length || tgtHits.length);
  }

  /** short-step study progressive dimming: fade temporary traces and soften previous step marks */
  onNewStepCommitted() {
    this._stepGeneration += 1;
    for (const rec of [...this._records]) {
      if (rec.temporary) {
        this._fadeAndRemoveRecord(rec);
      } else if (rec.generation < this._stepGeneration && rec.groupEl) {
        rec.groupEl.classList.add('marker-prev-step');
      }
    }
  }

  /** Re-ink all persisted SVG marks and colorized terms on Light/Dark toggle */
  reinkForTheme() {
    for (const rec of this._records) {
      if (!rec.groupEl) continue;
      const hex = resolveMarkerHex(rec.colorName, { isHighlighter: rec.isHighlighter });
      rec.groupEl.querySelectorAll('path, circle').forEach((el) => {
        if (el.getAttribute('stroke') && el.getAttribute('stroke') !== 'none') {
          el.setAttribute('stroke', hex);
          if (rec.isHighlighter) {
            el.setAttribute('stroke-opacity', isDarkTheme() ? '0.28' : '0.34');
          }
        }
        if (el.getAttribute('fill') && el.getAttribute('fill') !== 'none') {
          el.setAttribute('fill', hex);
        }
      });
      rec.groupEl.querySelectorAll('text').forEach((el) => el.setAttribute('fill', hex));
      rec.groupEl.querySelectorAll('.marker-hand-badge').forEach((el) => {
        el.style.borderColor = hex;
        el.style.color = hex;
      });
    }
    for (const sp of this._colorizedSpans) {
      if (!sp || !sp.isConnected) continue;
      const cn = sp.dataset.markerColorName;
      if (cn) sp.style.color = resolveMarkerHex(cn);
    }
  }

  clearAll() {
    for (const rec of this._records) {
      if (rec.timerId) clearTimeout(rec.timerId);
    }
    this._records = [];
    this._colorizedSpans = [];
    if (this.svgEl) this.svgEl.replaceChildren();
    if (this._nibEl) this._nibEl.hidden = true;
    if (this._laserEl) this._laserEl.hidden = true;
    if (this._laserTimer) {
      clearTimeout(this._laserTimer);
      this._laserTimer = null;
    }
  }
}
