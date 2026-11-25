/**
 * Hand-Drawn Professor Sketch & Whiteboard Marker Engine (Zero external deps).
 * Renders organic Rough/Excalidraw-style marker strokes progressively with a
 * live moving pen tip so the tutor genuinely DRAWS physics diagrams, graphs,
 * underlines, circles, and arrows by hand on the whiteboard.
 */

function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function resolveInkColor(token) {
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  const lightMap = {
    ink_primary: '#172554',
    ink: '#172554',
    accent_blue: '#1d4ed8',
    blue: '#1d4ed8',
    accent_red: '#dc2626',
    red: '#dc2626',
    accent_green: '#047857',
    green: '#047857',
    accent_orange: '#d97706',
    orange: '#d97706',
    accent_indigo: '#6d28d9',
    purple: '#6d28d9',
    teal: '#0f766e',
    pink: '#db2777',
    gray: '#475569',
  };
  const darkMap = {
    ink_primary: '#f1f5f9',
    ink: '#f1f5f9',
    accent_blue: '#60a5fa',
    blue: '#60a5fa',
    accent_red: '#f87171',
    red: '#f87171',
    accent_green: '#4ade80',
    green: '#4ade80',
    accent_orange: '#fb923c',
    orange: '#fb923c',
    accent_indigo: '#c4b5fd',
    purple: '#c4b5fd',
    teal: '#2dd4bf',
    pink: '#f472b6',
    gray: '#94a3b8',
  };
  const map = isDark ? darkMap : lightMap;
  return map[token] || (token && /^#[0-9a-f]{3,8}$/i.test(token) ? token : map.ink_primary);
}

/** Generate organic hand-drawn jittered polyline between (x1,y1) and (x2,y2). */
function sketchyLinePts(x1, y1, x2, y2, rng, roughness = 1.6) {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const dist = Math.hypot(dx, dy);
  const segs = Math.max(4, Math.min(28, Math.round(dist / 14)));
  const nx = dist > 1 ? -dy / dist : 0;
  const ny = dist > 1 ? dx / dist : 0;
  const bow = (rng() - 0.5) * roughness * 2.2;
  const pts = [];
  for (let i = 0; i <= segs; i++) {
    const t = i / segs;
    const arch = Math.sin(t * Math.PI) * bow;
    const jit = (i === 0 || i === segs) ? 0 : (rng() - 0.5) * roughness;
    pts.push({
      x: x1 + dx * t + nx * (arch + jit),
      y: y1 + dy * t + ny * (arch + jit),
    });
  }
  return pts;
}

function sketchyRectStrokes(x, y, w, h, color, rng, fillTint = '') {
  const strokes = [];
  if (fillTint) {
    // Diagonal marker hatch fill
    const step = 12;
    for (let d = -h; d < w + h; d += step) {
      const lx1 = Math.max(x + 3, x + d);
      const ly1 = Math.max(y + 3, y + (lx1 - (x + d)) + (d < 0 ? -d : 0));
      const lx2 = Math.min(x + w - 3, x + d + h);
      const ly2 = Math.min(y + h - 3, y + (lx2 - (x + d)));
      if (lx2 > lx1 + 2 && ly2 > ly1 + 2) {
        strokes.push({ kind: 'path', pts: sketchyLinePts(lx1, ly2, lx2, ly1, rng, 0.7), color: fillTint, width: 1.15, alpha: 0.22 });
      }
    }
  }
  // Double-pass hand-sketched borders
  const sides = [
    [x, y, x + w, y],
    [x + w, y, x + w, y + h],
    [x + w, y + h, x, y + h],
    [x, y + h, x, y],
  ];
  for (const [ax, ay, bx, by] of sides) {
    strokes.push({ kind: 'path', pts: sketchyLinePts(ax, ay, bx, by, rng, 1.5), color, width: 2.3, alpha: 0.96 });
    strokes.push({ kind: 'path', pts: sketchyLinePts(ax + 0.6, ay - 0.5, bx - 0.5, by + 0.6, rng, 1.8), color, width: 1.2, alpha: 0.45 });
  }
  return strokes;
}

function sketchyEllipseStrokes(cx, cy, rx, ry, color, rng, fillTint = '', dashed = false) {
  const strokes = [];
  if (fillTint) {
    strokes.push({ kind: 'fill_ellipse', cx, cy, rx: rx - 2, ry: ry - 2, color: fillTint, alpha: 0.14 });
  }
  const steps = 42;
  const pts = [];
  const startAng = rng() * 0.4;
  for (let i = 0; i <= steps + 2; i++) {
    const t = startAng + (i / steps) * Math.PI * 2;
    const jx = (rng() - 0.5) * 2.0;
    const jy = (rng() - 0.5) * 2.0;
    pts.push({ x: cx + Math.cos(t) * rx + jx, y: cy + Math.sin(t) * ry + jy });
  }
  strokes.push({ kind: 'path', pts, color, width: 2.3, alpha: 0.96, dashed });
  return strokes;
}

function sketchyArrowStrokes(x1, y1, x2, y2, color, rng, wavy = false, width = 2.4) {
  const strokes = [];
  let shaft;
  if (wavy) {
    const dx = x2 - x1, dy = y2 - y1;
    const dist = Math.hypot(dx, dy);
    const nx = dist > 1 ? -dy / dist : 0;
    const ny = dist > 1 ? dx / dist : 0;
    const steps = Math.max(18, Math.round(dist / 5));
    shaft = [];
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      const amp = Math.sin(t * Math.PI * 5) * 4.5 * Math.sin(t * Math.PI);
      shaft.push({ x: x1 + dx * t + nx * amp, y: y1 + dy * t + ny * amp });
    }
  } else {
    shaft = sketchyLinePts(x1, y1, x2, y2, rng, 1.5);
  }
  strokes.push({ kind: 'path', pts: shaft, color, width, alpha: 0.96 });

  const ang = Math.atan2(y2 - y1, x2 - x1);
  const hl = 11;
  const h1 = { x: x2 - hl * Math.cos(ang - 0.45), y: y2 - hl * Math.sin(ang - 0.45) };
  const h2 = { x: x2 - hl * Math.cos(ang + 0.45), y: y2 - hl * Math.sin(ang + 0.45) };
  strokes.push({ kind: 'path', pts: sketchyLinePts(h1.x, h1.y, x2, y2, rng, 0.7), color, width, alpha: 0.96 });
  strokes.push({ kind: 'path', pts: sketchyLinePts(h2.x, h2.y, x2, y2, rng, 0.7), color, width, alpha: 0.96 });
  return strokes;
}

function evalSafeExpr(exprStr, xVal) {
  const s = String(exprStr || 'x').replace(/\s+/g, '').replace(/\*\*/g, '^');
  let pos = 0;
  const peek = () => s[pos] || '';
  function parseExpr() {
    let v = parseTerm();
    while (peek() === '+' || peek() === '-') {
      const op = s[pos++];
      const r = parseTerm();
      v = op === '+' ? v + r : v - r;
    }
    return v;
  }
  function parseTerm() {
    let v = parsePow();
    while (peek() === '*' || peek() === '/') {
      const op = s[pos++];
      const r = parsePow();
      v = op === '*' ? v * r : (Math.abs(r) < 1e-9 ? NaN : v / r);
    }
    return v;
  }
  function parsePow() {
    let b = parseUnary();
    if (peek() === '^') { pos++; b = Math.pow(b, parsePow()); }
    return b;
  }
  function parseUnary() {
    if (peek() === '+') { pos++; return parseUnary(); }
    if (peek() === '-') { pos++; return -parseUnary(); }
    return parsePrim();
  }
  function parsePrim() {
    if (peek() === '(') { pos++; const v = parseExpr(); if (peek() === ')') pos++; return v; }
    const nm = /^\d*\.?\d+(?:[eE][+-]?\d+)?/.exec(s.slice(pos));
    if (nm) { pos += nm[0].length; return Number(nm[0]); }
    const id = /^[A-Za-z_]+/.exec(s.slice(pos));
    if (id) {
      const n = id[0].toLowerCase();
      pos += id[0].length;
      if (peek() === '(') {
        pos++;
        const a = parseExpr();
        if (peek() === ')') pos++;
        if (n === 'sin') return Math.sin(a);
        if (n === 'cos') return Math.cos(a);
        if (n === 'tan') return Math.tan(a);
        if (n === 'exp') return Math.exp(a);
        if (n === 'ln' || n === 'log') return a > 0 ? Math.log(a) : NaN;
        if (n === 'sqrt') return a >= 0 ? Math.sqrt(a) : NaN;
        if (n === 'abs') return Math.abs(a);
        return NaN;
      }
      if (n === 'pi') return Math.PI;
      if (n === 'e') return Math.E;
      return xVal;
    }
    return NaN;
  }
  try {
    const out = parseExpr();
    return Number.isFinite(out) ? out : NaN;
  } catch {
    return NaN;
  }
}

/** Build ordered list of hand-drawn strokes for any named physics sketch or custom element list. */
function buildSketchPlan(payload, W, H) {
  const rng = mulberry32(String(payload.title || payload.kind || 'sketch').length * 9301 + 49297);
  const ink = resolveInkColor('ink');
  const blue = resolveInkColor('blue');
  const red = resolveInkColor('red');
  const green = resolveInkColor('green');
  const orange = resolveInkColor('orange');
  const purple = resolveInkColor('purple');
  const kind = String(payload.kind || 'piston_cylinder').toLowerCase();
  const strokes = [];

  if (kind === 'piston_cylinder' || kind === 'first_law') {
    // Hand-drawn cylinder walls (U-shape), comfortably framed inside 560x320
    const cx = 130, cy = 54, cw = 152, ch = 152;
    strokes.push({ kind: 'path', pts: sketchyLinePts(cx, cy, cx, cy + ch, rng, 1.8), color: ink, width: 3.0 });
    strokes.push({ kind: 'path', pts: sketchyLinePts(cx, cy + ch, cx + cw, cy + ch, rng, 1.8), color: ink, width: 3.0 });
    strokes.push({ kind: 'path', pts: sketchyLinePts(cx + cw, cy + ch, cx + cw, cy, rng, 1.8), color: ink, width: 3.0 });
    // Movable piston slab
    strokes.push(...sketchyRectStrokes(cx + 4, cy + 32, cw - 8, 22, blue, rng, blue));
    // Piston rod
    strokes.push({ kind: 'path', pts: sketchyLinePts(cx + cw / 2, cy + 32, cx + cw / 2, cy - 18, rng, 1.2), color: blue, width: 3.0 });
    // Gas molecules cleanly positioned in upper-mid chamber (clear of bottom label)
    const gasPts = [[162, 134], [206, 126], [248, 138], [182, 160], [234, 162]];
    for (const [gx, gy] of gasPts) {
      strokes.push(...sketchyEllipseStrokes(gx, gy, 4.5, 4.5, purple, rng, purple));
      strokes.push(...sketchyArrowStrokes(gx + 5, gy - 2, gx + 15, gy - 10, purple, rng, false, 1.4));
    }
    strokes.push({ kind: 'label', x: cx + 28, y: cy + ch - 12, text: payload.gas_label || 'Gas ideal (ΔU)', color: purple, size: 19, bold: true });
    // Wavy red heat arrows Q entering from below
    strokes.push(...sketchyArrowStrokes(cx + 42, cy + ch + 52, cx + 42, cy + ch + 8, red, rng, true, 2.7));
    strokes.push(...sketchyArrowStrokes(cx + 110, cy + ch + 52, cx + 110, cy + ch + 8, red, rng, true, 2.7));
    strokes.push({ kind: 'label', x: cx + 10, y: cy + ch + 78, text: payload.heat_label || 'Calor Q entra (+)', color: red, size: 21, bold: true });
    // Work arrow W pushing piston up
    strokes.push(...sketchyArrowStrokes(cx + cw + 22, cy + 68, cx + cw + 22, cy + 10, green, rng, false, 2.9));
    strokes.push({ kind: 'label', x: cx + cw + 34, y: cy + 34, text: payload.work_label || 'Trabajo W', color: green, size: 21, bold: true });
    strokes.push({ kind: 'label', x: cx + cw + 34, y: cy + 56, text: '(empuja el pistón)', color: green, size: 17 });
    // Summary formula on the right
    strokes.push(...sketchyRectStrokes(334, 138, 194, 68, orange, rng, orange));
    strokes.push({ kind: 'label', x: 364, y: 180, text: 'ΔU = Q − W', color: ink, size: 26, bold: true });
    strokes.push({ kind: 'label', x: 350, y: 230, text: 'La energía se conserva', color: orange, size: 19, bold: true });
  } else if (kind === 'entropy_particles' || kind === 'second_law') {
    // Left box: Ordered (low entropy)
    strokes.push(...sketchyRectStrokes(38, 62, 185, 155, blue, rng, ''));
    strokes.push({ kind: 'label', x: 52, y: 46, text: 'Ordenado (baja S)', color: blue, size: 19, bold: true });
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        strokes.push(...sketchyEllipseStrokes(68 + c * 22, 96 + r * 26, 5.5, 5.5, red, rng, red));
      }
    }
    strokes.push({ kind: 'path', pts: sketchyLinePts(134, 64, 134, 215, rng, 1.5), color: ink, width: 2.0, dashed: true });
    strokes.push({ kind: 'label', x: 146, y: 144, text: 'Vacío', color: resolveInkColor('gray'), size: 17 });
    // Center arrow
    strokes.push(...sketchyArrowStrokes(234, 138, 316, 138, orange, rng, false, 3.0));
    strokes.push({ kind: 'label', x: 242, y: 122, text: 'Espontáneo', color: orange, size: 17, bold: true });
    strokes.push({ kind: 'label', x: 248, y: 164, text: 'ΔS > 0', color: red, size: 22, bold: true });
    // Right box: Disordered (high entropy)
    strokes.push(...sketchyRectStrokes(330, 62, 195, 155, red, rng, ''));
    strokes.push({ kind: 'label', x: 340, y: 46, text: 'Disperso (alta S)', color: red, size: 19, bold: true });
    const spread = [[358, 92], [412, 82], [488, 102], [376, 146], [442, 132], [500, 168], [362, 194], [428, 188], [478, 196]];
    for (const [px, py] of spread) {
      strokes.push(...sketchyEllipseStrokes(px, py, 5.5, 5.5, red, rng, red));
      strokes.push(...sketchyArrowStrokes(px, py, px + (rng() - 0.5) * 26, py + (rng() - 0.5) * 26, orange, rng, false, 1.3));
    }
    strokes.push({ kind: 'label', x: 88, y: 256, text: 'El calor fluye solo de caliente → frío · el desorden total aumenta', color: ink, size: 18 });
  } else if (kind === 'heat_engine_carnot' || kind === 'carnot') {
    // Hot reservoir
    strokes.push(...sketchyRectStrokes(145, 22, 210, 48, red, rng, red));
    strokes.push({ kind: 'label', x: 172, y: 52, text: payload.th_label || 'Foco Caliente (TH)', color: red, size: 19, bold: true });
    // QH arrow down
    strokes.push(...sketchyArrowStrokes(250, 72, 250, 114, red, rng, true, 2.8));
    strokes.push({ kind: 'label', x: 264, y: 100, text: 'QH', color: red, size: 20, bold: true });
    // Engine circle
    strokes.push(...sketchyEllipseStrokes(250, 148, 36, 32, ink, rng, blue));
    strokes.push({ kind: 'label', x: 229, y: 155, text: 'Ciclo', color: ink, size: 18, bold: true });
    // Work arrow W out right
    strokes.push(...sketchyArrowStrokes(288, 148, 384, 148, green, rng, false, 3.0));
    strokes.push({ kind: 'label', x: 392, y: 154, text: 'W neto = QH − QC', color: green, size: 19, bold: true });
    // QC arrow down
    strokes.push(...sketchyArrowStrokes(250, 182, 250, 224, blue, rng, true, 2.6));
    strokes.push({ kind: 'label', x: 264, y: 210, text: 'QC', color: blue, size: 20, bold: true });
    // Cold reservoir
    strokes.push(...sketchyRectStrokes(145, 228, 210, 48, blue, rng, blue));
    strokes.push({ kind: 'label', x: 180, y: 258, text: payload.tc_label || 'Foco Frío (TC)', color: blue, size: 19, bold: true });
  } else if (kind === 'thermal_sphere' || kind === 'thermal_cylinder') {
    const cx = 195, cy = 148;
    strokes.push(...sketchyEllipseStrokes(cx, cy, 108, 108, blue, rng, blue));
    strokes.push(...sketchyEllipseStrokes(cx, cy, 74, 74, orange, rng, '', true));
    strokes.push(...sketchyEllipseStrokes(cx, cy, 40, 40, red, rng, red));
    // Radial heat arrows H
    for (const ang of [-0.35, 0.45, 2.4, -2.2]) {
      strokes.push(...sketchyArrowStrokes(
        cx + Math.cos(ang) * 44, cy + Math.sin(ang) * 44,
        cx + Math.cos(ang) * 102, cy + Math.sin(ang) * 102,
        red, rng, false, 2.0,
      ));
    }
    strokes.push({ kind: 'label', x: 326, y: 82, text: payload.inner_label || 'Interior: radio r₁, T₁', color: red, size: 19, bold: true });
    strokes.push({ kind: 'label', x: 326, y: 132, text: payload.shell_label || 'Cascarón fino dr (radio r)', color: orange, size: 19, bold: true });
    strokes.push({ kind: 'label', x: 326, y: 182, text: payload.outer_label || 'Exterior: radio r₂, T₂', color: blue, size: 19, bold: true });
    strokes.push({ kind: 'label', x: 326, y: 232, text: kind === 'thermal_cylinder' ? 'Área A(r) = 2πrL' : 'Área A(r) = 4πr²', color: ink, size: 20, bold: true });
  } else if (kind === 'thermal_bar' || kind === 'conduction_wall') {
    strokes.push(...sketchyRectStrokes(42, 74, 74, 140, red, rng, red));
    strokes.push({ kind: 'label', x: 64, y: 150, text: 'TH', color: red, size: 22, bold: true });
    strokes.push(...sketchyRectStrokes(118, 92, 286, 104, ink, rng, ''));
    // Differential slice dx
    strokes.push(...sketchyRectStrokes(244, 92, 34, 104, orange, rng, orange));
    strokes.push({ kind: 'label', x: 251, y: 220, text: 'dx', color: orange, size: 20, bold: true });
    // Heat flow arrows H
    strokes.push(...sketchyArrowStrokes(136, 125, 384, 125, red, rng, true, 2.4));
    strokes.push(...sketchyArrowStrokes(136, 162, 384, 162, red, rng, true, 2.4));
    strokes.push({ kind: 'label', x: 215, y: 74, text: 'Flujo H = −k A (dT/dx)', color: red, size: 20, bold: true });
    strokes.push(...sketchyRectStrokes(406, 74, 74, 140, blue, rng, blue));
    strokes.push({ kind: 'label', x: 428, y: 150, text: 'TC', color: blue, size: 22, bold: true });
  } else if (kind === 'pv_cycle' || kind === 'pv_diagram') {
    const padL = 68, padB = 46, padT = 26, padR = 36;
    // Hand-drawn axes with arrows
    strokes.push(...sketchyArrowStrokes(padL, H - padB, padL, padT, ink, rng, false, 2.3));
    strokes.push(...sketchyArrowStrokes(padL, H - padB, W - padR, H - padB, ink, rng, false, 2.3));
    strokes.push({ kind: 'label', x: 22, y: padT + 14, text: payload.y_label || 'P (Pa)', color: ink, size: 18, bold: true });
    strokes.push({ kind: 'label', x: W - padR - 48, y: H - 14, text: payload.x_label || 'V (m³)', color: ink, size: 18, bold: true });

    if (kind === 'pv_cycle') {
      const A = { x: 410, y: 74 }, B = { x: 182, y: 74 }, C = { x: 410, y: 214 };
      // A -> B isobaric
      strokes.push(...sketchyArrowStrokes(A.x, A.y, B.x + 8, B.y, blue, rng, false, 2.8));
      strokes.push({ kind: 'label', x: 266, y: 60, text: 'P = cte', color: blue, size: 17 });
      // B -> C adiabatic curve
      const curvePts = [];
      for (let i = 0; i <= 26; i++) {
        const t = i / 26;
        const x = B.x + (C.x - B.x) * t;
        const y = B.y + (C.y - B.y) * Math.pow(t, 0.72);
        curvePts.push({ x, y });
      }
      strokes.push({ kind: 'path', pts: curvePts, color: orange, width: 2.8 });
      strokes.push({ kind: 'label', x: 215, y: 172, text: 'Adiabática', color: orange, size: 17 });
      // C -> A isochoric
      strokes.push(...sketchyArrowStrokes(C.x, C.y, A.x, A.y + 8, red, rng, false, 2.8));
      strokes.push({ kind: 'label', x: 422, y: 152, text: 'V = cte', color: red, size: 17 });
      for (const [pt, lbl] of [[A, 'A'], [B, 'B'], [C, 'C']]) {
        strokes.push(...sketchyEllipseStrokes(pt.x, pt.y, 5, 5, ink, rng, ink));
        strokes.push({ kind: 'label', x: pt.x + 10, y: pt.y - 6, text: lbl, color: ink, size: 20, bold: true });
      }
    } else {
      // Single P(V) curve with hand-hatched area W = ∫ P dV
      const curvePts = [];
      for (let i = 0; i <= 40; i++) {
        const t = i / 40;
        const x = 135 + t * 320;
        const y = 60 + (1 - 1 / (1 + 2.4 * t)) * 165;
        curvePts.push({ x, y });
      }
      // Hand-hatched vertical strips under curve
      for (let i = 2; i < curvePts.length - 1; i += 3) {
        const p = curvePts[i];
        strokes.push({ kind: 'path', pts: sketchyLinePts(p.x, H - padB - 2, p.x, p.y + 3, rng, 0.8), color: blue, width: 1.5, alpha: 0.32 });
      }
      strokes.push({ kind: 'path', pts: curvePts, color: blue, width: 3.0 });
      strokes.push({ kind: 'label', x: 230, y: H - padB - 26, text: payload.area_label || 'Área = W = ∫ P dV', color: blue, size: 20, bold: true });
    }
  } else if (kind === 'function_graph' || kind === 'thermal_gradient') {
    const padL = 62, padB = 42, padT = 26, padR = 30;
    const xMin = Number(payload.x_min ?? 0.15);
    const xMax = Number(payload.x_max ?? 1.0);
    const lo = Math.min(xMin, xMax);
    const hi = Math.max(xMin, xMax === xMin ? xMin + 1 : xMax);
    const expr = payload.expr || (kind === 'thermal_gradient' ? '100 - 80*(ln(x/0.15)/ln(4))' : '1/(x^2)');

    strokes.push(...sketchyArrowStrokes(padL, H - padB, padL, padT, ink, rng, false, 2.2));
    strokes.push(...sketchyArrowStrokes(padL, H - padB, W - padR, H - padB, ink, rng, false, 2.2));
    strokes.push({ kind: 'label', x: 16, y: padT + 14, text: payload.y_label || 'y', color: ink, size: 17, bold: true });
    strokes.push({ kind: 'label', x: W - padR - 46, y: H - 12, text: payload.x_label || 'x', color: ink, size: 17, bold: true });

    const vals = [];
    let yMin = Infinity, yMax = -Infinity;
    for (let i = 0; i <= 60; i++) {
      const x = lo + ((hi - lo) * i) / 60;
      const y = evalSafeExpr(expr, x);
      if (Number.isFinite(y)) {
        vals.push({ x, y });
        yMin = Math.min(yMin, y);
        yMax = Math.max(yMax, y);
      }
    }
    if (vals.length >= 2) {
      if (yMin > 0) yMin = 0;
      const ySpan = Math.max(1e-6, yMax - yMin) * 1.15;
      const sx = (x) => padL + ((x - lo) / Math.max(1e-6, hi - lo)) * (W - padL - padR);
      const sy = (y) => H - padB - ((y - yMin) / ySpan) * (H - padT - padB);
      const pts = vals.map((p) => ({ x: sx(p.x), y: sy(p.y) }));
      const curveColor = kind === 'thermal_gradient' ? orange : blue;
      for (let i = 2; i < pts.length - 1; i += 4) {
        strokes.push({ kind: 'path', pts: sketchyLinePts(pts[i].x, H - padB - 2, pts[i].x, pts[i].y + 2, rng, 0.7), color: curveColor, width: 1.4, alpha: 0.26 });
      }
      strokes.push({ kind: 'path', pts, color: curveColor, width: 2.8 });
      strokes.push({ kind: 'label', x: padL - 4, y: H - 14, text: lo.toFixed(2), color: resolveInkColor('gray'), size: 14 });
      strokes.push({ kind: 'label', x: W - padR - 72, y: H - 14, text: hi.toFixed(2), color: resolveInkColor('gray'), size: 14 });
    }
  } else if (kind === 'custom_diagram' && Array.isArray(payload.elements)) {
    const placedPinLabels = [];
    const placedBoxes = [];
    for (const el of payload.elements.slice(0, 28)) {
      if (!el || typeof el !== 'object') continue;
      const c = resolveInkColor(el.color || 'ink');
      const k = String(el.kind || '').toLowerCase();
      if (k === 'box' || k === 'rect') {
        const bx = Math.max(10, Math.min(W - 90, Number(el.x || 40)));
        let by = Math.max(10, Math.min(H - 44, Number(el.y || 40)));
        const txt = String(el.text || '').trim();
        let bw = Math.max(84, Number(el.w || 130));
        let bh = Math.max(40, Number(el.h || 56));
        // Auto-expand box slightly if label is wider than box, without exceeding canvas right edge
        if (txt) {
          const neededW = Math.min(260, Math.round(txt.length * 8.6 + 24));
          if (neededW > bw) bw = Math.min(W - bx - 12, neededW);
        }
        // Prevent vertical overlap with previously placed boxes sharing same horizontal column
        for (const prev of placedBoxes) {
          const horizOverlap = !(bx + bw < prev.x + 6 || bx > prev.x + prev.w - 6);
          const vertOverlap = by < prev.y + prev.h + 10 && by + bh > prev.y - 6;
          if (horizOverlap && vertOverlap) {
            by = Math.min(H - bh - 10, prev.y + prev.h + 14);
          }
        }
        placedBoxes.push({ x: bx, y: by, w: bw, h: bh });
        strokes.push(...sketchyRectStrokes(bx, by, bw, bh, c, rng, el.fill ? resolveInkColor(el.fill) : ''));
        if (txt) {
          const maxTextW = Math.max(44, bw - 18);
          const estW = txt.length * 8.2;
          if (estW > maxTextW && txt.includes(' ')) {
            // Clean 2-line wrap inside box
            const mid = Math.floor(txt.length / 2);
            let split = txt.lastIndexOf(' ', mid + 4);
            if (split < 2) split = txt.indexOf(' ', mid);
            const l1 = split > 0 ? txt.slice(0, split).trim() : txt;
            const l2 = split > 0 ? txt.slice(split + 1).trim() : '';
            strokes.push({ kind: 'label', x: bx + 10, y: by + bh / 2 - 4, text: l1, color: c, size: 15, bold: true, maxWidth: maxTextW });
            if (l2) strokes.push({ kind: 'label', x: bx + 10, y: by + bh / 2 + 14, text: l2, color: c, size: 15, bold: true, maxWidth: maxTextW });
          } else {
            const fitSize = estW > maxTextW ? 15 : 17;
            strokes.push({ kind: 'label', x: bx + 10, y: by + bh / 2 + 6, text: txt, color: c, size: fitSize, bold: true, maxWidth: maxTextW });
          }
        }
      } else if (k === 'circle' || k === 'ellipse') {
        const rx = Number(el.rx || el.r || 36);
        const ry = Number(el.ry || el.r || 36);
        strokes.push(...sketchyEllipseStrokes(Number(el.x || 180), Number(el.y || 140), rx, ry, c, rng, el.fill ? resolveInkColor(el.fill) : ''));
        if (el.text) strokes.push({ kind: 'label', x: Number(el.x || 180) - rx * 0.65, y: Number(el.y || 140) + 6, text: String(el.text), color: c, size: 16, bold: true, maxWidth: Math.max(40, rx * 1.4) });
      } else if (k === 'arrow' || k === 'wavy_arrow') {
        strokes.push(...sketchyArrowStrokes(Number(el.x || 40), Number(el.y || 140), Number(el.x2 || 180), Number(el.y2 || 140), c, rng, k === 'wavy_arrow', 2.5));
        if (el.text) strokes.push({ kind: 'label', x: (Number(el.x || 40) + Number(el.x2 || 180)) / 2 - 20, y: (Number(el.y || 140) + Number(el.y2 || 140)) / 2 - 10, text: String(el.text), color: c, size: 16, bold: true });
      } else if (k === 'line') {
        strokes.push({ kind: 'path', pts: sketchyLinePts(Number(el.x || 40), Number(el.y || 140), Number(el.x2 || 200), Number(el.y2 || 140), rng, 1.4), color: c, width: 2.3 });
      } else if ((k === 'polygon' || k === 'polyline') && Array.isArray(el.points) && el.points.length >= 2) {
        const rawPts = el.points.map((pt) => ({ x: Number(pt.x || 0), y: Number(pt.y || 0) }));
        if (k === 'polygon' && el.fill) {
          strokes.push({ kind: 'fill_poly', pts: rawPts, color: resolveInkColor(el.fill), alpha: 0.14 });
        }
        const count = k === 'polygon' ? rawPts.length : rawPts.length - 1;
        for (let idx = 0; idx < count; idx++) {
          const a = rawPts[idx];
          const b = rawPts[(idx + 1) % rawPts.length];
          strokes.push({ kind: 'path', pts: sketchyLinePts(a.x, a.y, b.x, b.y, rng, 1.2), color: c, width: 2.3, alpha: 0.96 });
        }
        if (el.text) {
          const cx = rawPts.reduce((s, p) => s + p.x, 0) / rawPts.length;
          const cy = rawPts.reduce((s, p) => s + p.y, 0) / rawPts.length;
          strokes.push({ kind: 'label', x: cx - 34, y: cy + 5, text: String(el.text), color: c, size: 17, bold: true });
        }
      } else if (k === 'pin') {
        const px = Number(el.x || 180);
        const py = Number(el.y || 140);
        const pc = resolveInkColor(el.color || 'red');
        strokes.push(...sketchyEllipseStrokes(px, py, 6, 6, pc, rng, pc));
        if (el.text) {
          let lx = px + 10;
          let ly = py + 5;
          for (const prev of placedPinLabels) {
            if (Math.abs(lx - prev.x) < 92 && Math.abs(ly - prev.y) < 18) {
              ly = prev.y + 20;
            }
          }
          placedPinLabels.push({ x: lx, y: ly });
          strokes.push({ kind: 'label', x: lx, y: Math.min(H - 12, ly), text: String(el.text), color: pc, size: Number(el.size || 16), bold: true });
        }
      } else if (k === 'label' || k === 'text') {
        strokes.push({ kind: 'label', x: Number(el.x || 40), y: Number(el.y || 60), text: String(el.text || ''), color: c, size: Number(el.size || 19), bold: Boolean(el.bold) });
      }
    }
  } else {
    // Fallback expressive physics sketch if unknown kind
    return buildSketchPlan({ ...payload, kind: 'piston_cylinder' }, W, H);
  }
  return strokes;
}

/**
 * Create a hand-drawn whiteboard sketch element that animates stroke-by-stroke
 * with a live visible marker nib tracking along every stroke.
 */
export function createHandDrawnSketchBlock(
  payload,
  { reduceMotion = false, durationMs = 2200, getSpeedFactor = null, isAborted = null } = {},
) {
  const wrap = document.createElement('div');
  wrap.className = 'wb-sketch-entry';

  if (payload.title) {
    const title = document.createElement('div');
    title.className = 'wb-sketch-title';
    title.textContent = payload.title;
    wrap.appendChild(title);
  }

  const stage = document.createElement('div');
  stage.className = 'wb-sketch-stage';

  const W = 560, H = 320;
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const canvas = document.createElement('canvas');
  canvas.className = 'wb-sketch-canvas';
  canvas.width = W * dpr;
  canvas.height = H * dpr;
  stage.appendChild(canvas);

  const penNib = document.createElement('div');
  penNib.className = 'wb-pen-nib';
  penNib.hidden = true;
  stage.appendChild(penNib);
  wrap.appendChild(stage);

  if (payload.caption_es) {
    const cap = document.createElement('p');
    cap.className = 'wb-hand-note';
    cap.textContent = payload.caption_es;
    wrap.appendChild(cap);
  }

  let strokes = buildSketchPlan(payload, W, H);
  const ctx = canvas.getContext('2d');
  ctx.scale(dpr, dpr);

  // Re-ink cleanly whenever the user toggles Light/Dark whiteboard
  wrap._reinkForTheme = () => {
    strokes = buildSketchPlan(payload, W, H);
    renderAtProgress(1);
  };

  function drawStrokeSlice(st, fraction) {
    ctx.save();
    ctx.globalAlpha = st.alpha ?? 1;
    if (st.kind === 'fill_ellipse') {
      ctx.fillStyle = st.color;
      ctx.beginPath();
      ctx.ellipse(st.cx, st.cy, st.rx, st.ry, 0, 0, Math.PI * 2 * Math.min(1, fraction));
      ctx.fill();
      ctx.restore();
      return { x: st.cx + st.rx, y: st.cy };
    }
    if (st.kind === 'fill_poly' && st.pts && st.pts.length >= 3) {
      ctx.fillStyle = st.color;
      ctx.globalAlpha = (st.alpha ?? 0.14) * Math.min(1, fraction);
      ctx.beginPath();
      ctx.moveTo(st.pts[0].x, st.pts[0].y);
      for (let i = 1; i < st.pts.length; i++) ctx.lineTo(st.pts[i].x, st.pts[i].y);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
      return st.pts[0];
    }
    if (st.kind === 'label') {
      const txt = st.text || '';
      const chars = Math.max(1, Math.round(txt.length * Math.min(1, fraction)));
      const sub = txt.slice(0, chars);
      ctx.fillStyle = st.color;
      ctx.font = `${st.bold ? '700' : '600'} ${st.size || 19}px "Caveat", "Patrick Hand", "Ink Free", "Segoe Print", cursive`;
      if (st.maxWidth && st.maxWidth > 20) {
        ctx.fillText(sub, st.x, st.y, st.maxWidth);
      } else {
        ctx.fillText(sub, st.x, st.y);
      }
      const m = ctx.measureText(sub);
      const effW = st.maxWidth ? Math.min(st.maxWidth, m.width) : m.width;
      ctx.restore();
      return { x: st.x + effW, y: st.y - 4 };
    }
    if (st.kind === 'path' && st.pts && st.pts.length >= 2) {
      const maxIdx = Math.max(1, Math.floor((st.pts.length - 1) * Math.min(1, fraction)));
      ctx.strokeStyle = st.color;
      ctx.lineWidth = st.width || 2.3;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      if (st.dashed) ctx.setLineDash([7, 5]);
      ctx.beginPath();
      ctx.moveTo(st.pts[0].x, st.pts[0].y);
      for (let i = 1; i <= maxIdx; i++) {
        ctx.lineTo(st.pts[i].x, st.pts[i].y);
      }
      ctx.stroke();
      const tip = st.pts[Math.min(maxIdx, st.pts.length - 1)];
      ctx.restore();
      return tip;
    }
    ctx.restore();
    return null;
  }

  function renderAtProgress(p) {
    ctx.clearRect(0, 0, W, H);
    const total = strokes.length;
    if (!total) return null;
    const scaled = p * total;
    let tip = null;
    for (let i = 0; i < total; i++) {
      if (scaled <= i) break;
      const frac = Math.min(1, scaled - i);
      tip = drawStrokeSlice(strokes[i], frac) || tip;
    }
    return tip;
  }

  if (reduceMotion || durationMs <= 0) {
    wrap._finishedPromise = Promise.resolve();
    requestAnimationFrame(() => renderAtProgress(1));
  } else {
    const autoDur = Math.max(2000, Math.min(7200, 1100 + strokes.length * 135));
    const totalDur = Math.max(1400, Math.min(9000, Math.max(durationMs || 0, Math.round(autoDur * 0.8))));
    penNib.hidden = false;
    wrap._finishedPromise = new Promise((resolve) => {
      let lastNow = performance.now();
      let progressMs = 0;
      function frame(now) {
        if (isAborted && isAborted()) {
          renderAtProgress(1);
          penNib.hidden = true;
          resolve();
          return;
        }
        const dt = Math.min(80, Math.max(0, now - lastNow));
        lastNow = now;
        const speed = getSpeedFactor ? Math.max(0.18, Math.min(2.2, Number(getSpeedFactor()) || 1.0)) : 1.0;
        progressMs += dt * speed;
        const p = Math.min(1, progressMs / totalDur);
        const tip = renderAtProgress(p);
        if (tip && p < 1) {
          penNib.style.left = `${(tip.x / W) * 100}%`;
          penNib.style.top = `${(tip.y / H) * 100}%`;
          requestAnimationFrame(frame);
        } else {
          renderAtProgress(1);
          penNib.hidden = true;
          resolve();
        }
      }
      requestAnimationFrame(frame);
    });
  }

  return wrap;
}
