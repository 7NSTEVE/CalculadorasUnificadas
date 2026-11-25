/**
 * Interactive Physics III & Calculus Visual Widgets (Canvas2D + Vector UI).
 * - RiemannWorkWidget: live N=1..100 slider comparing rectangle sum vs exact integral
 * - PVDiagramWidget: isothermal / adiabatic / isobaric P-V shaded work area
 * - ThermalShellWidget: spherical / cylindrical differential shell dr with radial flux
 * - CasioKeycapStrip: graphical Casio keycap verification badges
 */

function evalSimpleFunc(expr, x) {
  // Deterministic safe evaluator for basic polynomials/reciprocals without eval()
  const s = String(expr || '2 + 4*x').replace(/\s+/g, '');
  if (s === '2+4*x') return 2 + 4 * x;
  if (s === '2+0.5*x') return 2 + 0.5 * x;
  if (s === '3*x^2' || s === '3*x**2') return 3 * x * x;
  if (s === '1/x') return 1 / Math.max(0.05, x);
  if (s === '1/x^2') return 1 / Math.max(0.01, x * x);
  // Linear fallback a + b*x
  const lin = /^([+-]?\d*\.?\d+)\+([+-]?\d*\.?\d+)\*x$/.exec(s);
  if (lin) return Number(lin[1]) + Number(lin[2]) * x;
  return 2 + 4 * x;
}

export function createRiemannWorkWidget(payload) {
  const wrap = document.createElement('div');
  wrap.className = 'widget-box';

  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = payload.title || 'Visualización de integral: suma de rectángulos';
  const metric = document.createElement('span');
  metric.className = 'step-label';
  hdr.append(title, metric);

  const canvas = document.createElement('canvas');
  canvas.className = 'widget-canvas';
  canvas.width = 520;
  canvas.height = 220;

  const ctrl = document.createElement('div');
  ctrl.className = 'widget-controls';
  const nLabel = document.createElement('span');
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.min = '1';
  slider.max = String(payload.n_max || 100);
  slider.value = String(payload.n_init || 6);
  slider.setAttribute('aria-label', 'Cantidad de rectángulos');
  const exactLabel = document.createElement('span');
  exactLabel.style.fontWeight = '600';
  ctrl.append(nLabel, slider, exactLabel);

  wrap.append(hdr, canvas, ctrl);

  const a = Number(payload.a ?? 1);
  const b = Number(payload.b ?? 5);
  const exact = Number(payload.exact_value ?? 56);
  const unit = payload.unit || 'J';

  function draw() {
    const n = Number(slider.value);
    const ctx = canvas.getContext('2d');
    const W = canvas.width;
    const H = canvas.height;
    const padL = 44, padR = 18, padT = 18, padB = 34;
    ctx.clearRect(0, 0, W, H);

    const style = getComputedStyle(document.documentElement);
    const ink = style.getPropertyValue('--ink-primary').trim() || '#161a22';
    const sub = style.getPropertyValue('--ink-secondary').trim() || '#4c5666';
    const blue = style.getPropertyValue('--accent-blue').trim() || '#0066e0';
    const green = style.getPropertyValue('--accent-green').trim() || '#147a36';

    let yMax = 0;
    for (let i = 0; i <= 80; i++) {
      const x = a + ((b - a) * i) / 80;
      yMax = Math.max(yMax, evalSimpleFunc(payload.expr, x));
    }
    yMax *= 1.15;

    const sx = (x) => padL + ((x - a) / Math.max(0.001, b - a)) * (W - padL - padR);
    const sy = (y) => H - padB - (y / Math.max(0.001, yMax)) * (H - padT - padB);

    // Draw Riemann left-midpoint rectangles
    const dx = (b - a) / n;
    let approx = 0;
    ctx.fillStyle = blue + '33';
    ctx.strokeStyle = blue;
    ctx.lineWidth = n > 40 ? 0.6 : 1.2;
    for (let i = 0; i < n; i++) {
      const x0 = a + i * dx;
      const y = evalSimpleFunc(payload.expr, x0);
      approx += y * dx;
      const rx = sx(x0);
      const rw = sx(x0 + dx) - rx;
      const ry = sy(y);
      const rh = sy(0) - ry;
      ctx.fillRect(rx, ry, rw, rh);
      ctx.strokeRect(rx, ry, rw, rh);
    }

    // Draw exact curve
    ctx.beginPath();
    ctx.strokeStyle = green;
    ctx.lineWidth = 2.4;
    for (let i = 0; i <= 100; i++) {
      const x = a + ((b - a) * i) / 100;
      const y = evalSimpleFunc(payload.expr, x);
      if (i === 0) ctx.moveTo(sx(x), sy(y));
      else ctx.lineTo(sx(x), sy(y));
    }
    ctx.stroke();

    // Axes
    ctx.strokeStyle = sub;
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(padL, padT);
    ctx.lineTo(padL, H - padB);
    ctx.lineTo(W - padR, H - padB);
    ctx.stroke();

    ctx.fillStyle = ink;
    ctx.font = '12px Segoe UI, sans-serif';
    ctx.fillText(payload.x_label || 'V (m³)', W - 62, H - 10);
    ctx.fillText(payload.y_label || 'P (Pa)', 8, 16);

    nLabel.textContent = `Rectángulos: ${n}`;
    metric.textContent = `Aprox: ${approx.toFixed(2)} ${unit}`;
    exactLabel.textContent = `Exacto: ${exact.toFixed(2)} ${unit}`;
  }

  slider.addEventListener('input', draw);
  requestAnimationFrame(draw);
  return wrap;
}

export function createPVDiagramWidget(payload) {
  const wrap = document.createElement('div');
  wrap.className = 'widget-box';
  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = payload.title || 'Diagrama P–V · Trabajo bajo la curva';
  const badge = document.createElement('span');
  badge.className = 'step-label';
  hdr.append(title, badge);

  const canvas = document.createElement('canvas');
  canvas.className = 'widget-canvas';
  canvas.width = 520;
  canvas.height = 220;
  wrap.append(hdr, canvas);

  const Vi = Number(payload.Vi ?? 1.0);
  const Vf = Number(payload.Vf ?? 3.0);
  const nRT = Number(payload.nRT ?? 2400.0);
  const isAdiab = payload.process === 'adiabatic';
  const gamma = 1.4;
  const C = nRT * Math.pow(Vi, gamma - 1);
  const P = (v) => (isAdiab ? C / Math.pow(v, gamma) : nRT / v);
  const exactW = isAdiab
    ? (P(Vi) * Vi - P(Vf) * Vf) / (gamma - 1)
    : nRT * Math.log(Vf / Vi);
  badge.textContent = `W = ${exactW.toFixed(1)} J`;

  requestAnimationFrame(() => {
    const ctx = canvas.getContext('2d');
    const W = canvas.width, H = canvas.height;
    const padL = 48, padR = 22, padT = 20, padB = 34;
    const style = getComputedStyle(document.documentElement);
    const ink = style.getPropertyValue('--ink-primary').trim() || '#161a22';
    const sub = style.getPropertyValue('--ink-secondary').trim() || '#4c5666';
    const blue = style.getPropertyValue('--accent-blue').trim() || '#0066e0';

    const vMax = Vf * 1.2;
    const pMax = P(Vi) * 1.18;
    const sx = (v) => padL + (v / vMax) * (W - padL - padR);
    const sy = (p) => H - padB - (p / pMax) * (H - padT - padB);

    // Shaded work area under P(V)
    ctx.beginPath();
    ctx.moveTo(sx(Vi), sy(0));
    for (let i = 0; i <= 80; i++) {
      const v = Vi + ((Vf - Vi) * i) / 80;
      ctx.lineTo(sx(v), sy(P(v)));
    }
    ctx.lineTo(sx(Vf), sy(0));
    ctx.closePath();
    ctx.fillStyle = blue + '2b';
    ctx.fill();

    // Curve
    ctx.beginPath();
    ctx.strokeStyle = blue;
    ctx.lineWidth = 2.6;
    for (let i = 0; i <= 80; i++) {
      const v = Vi + ((Vf - Vi) * i) / 80;
      if (i === 0) ctx.moveTo(sx(v), sy(P(v)));
      else ctx.lineTo(sx(v), sy(P(v)));
    }
    ctx.stroke();

    // Endpoints Vi, Vf
    for (const [label, v] of [['Estado A (Vi)', Vi], ['Estado B (Vf)', Vf]]) {
      ctx.beginPath();
      ctx.arc(sx(v), sy(P(v)), 4.5, 0, Math.PI * 2);
      ctx.fillStyle = blue;
      ctx.fill();
      ctx.fillStyle = ink;
      ctx.font = '11.5px Segoe UI, sans-serif';
      ctx.fillText(label, sx(v) - 12, sy(P(v)) - 10);
    }

    // Axes
    ctx.strokeStyle = sub;
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(padL, padT);
    ctx.lineTo(padL, H - padB);
    ctx.lineTo(W - padR, H - padB);
    ctx.stroke();
    ctx.fillStyle = ink;
    ctx.fillText('V (m³)', W - 52, H - 10);
    ctx.fillText('P (Pa)', 8, 16);
  });

  return wrap;
}

export function createThermalShellWidget(payload) {
  const wrap = document.createElement('div');
  wrap.className = 'widget-box';
  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = payload.title || 'Cascarón térmico diferencial dr';
  const info = document.createElement('span');
  info.className = 'step-label';
  info.textContent = payload.geometry === 'cylindrical' ? 'A(r) = 2πrL' : 'A(r) = 4πr²';
  hdr.append(title, info);

  const canvas = document.createElement('canvas');
  canvas.className = 'widget-canvas';
  canvas.width = 520;
  canvas.height = 220;
  wrap.append(hdr, canvas);

  requestAnimationFrame(() => {
    const ctx = canvas.getContext('2d');
    const W = canvas.width, H = canvas.height;
    const cx = W * 0.42, cy = H * 0.52;
    const style = getComputedStyle(document.documentElement);
    const ink = style.getPropertyValue('--ink-primary').trim() || '#161a22';
    const orange = style.getPropertyValue('--accent-orange').trim() || '#d97706';
    const red = style.getPropertyValue('--accent-red').trim() || '#dc2626';
    const blue = style.getPropertyValue('--accent-blue').trim() || '#0066e0';

    // Outer shell r2
    ctx.beginPath();
    ctx.arc(cx, cy, 86, 0, Math.PI * 2);
    ctx.fillStyle = blue + '18';
    ctx.fill();
    ctx.strokeStyle = blue;
    ctx.lineWidth = 2;
    ctx.stroke();

    // Differential ring dr
    ctx.beginPath();
    ctx.arc(cx, cy, 62, 0, Math.PI * 2);
    ctx.strokeStyle = orange;
    ctx.lineWidth = 10;
    ctx.stroke();

    // Hot inner cavity r1
    ctx.beginPath();
    ctx.arc(cx, cy, 34, 0, Math.PI * 2);
    ctx.fillStyle = red + '2e';
    ctx.fill();
    ctx.strokeStyle = red;
    ctx.lineWidth = 2;
    ctx.stroke();

    // Radial heat arrows H
    for (const ang of [-0.4, 0.35, 2.6, -2.5]) {
      const x0 = cx + Math.cos(ang) * 38;
      const y0 = cy + Math.sin(ang) * 38;
      const x1 = cx + Math.cos(ang) * 80;
      const y1 = cy + Math.sin(ang) * 80;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.strokeStyle = red;
      ctx.lineWidth = 1.6;
      ctx.stroke();
    }

    ctx.fillStyle = ink;
    ctx.font = '12.5px Segoe UI, sans-serif';
    ctx.fillText(`T₁ = ${payload.T1 ?? 380} K (interior r₁)`, cx + 104, cy - 36);
    ctx.fillText(`Capa diferencial dr (radio r)`, cx + 104, cy);
    ctx.fillText(`T₂ = ${payload.T2 ?? 300} K (exterior r₂)`, cx + 104, cy + 36);
  });

  return wrap;
}

export function createCasioKeycapStrip(payload) {
  const wrap = document.createElement('div');
  wrap.className = 'widget-box';
  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = `Verificación en ${payload.model || 'fx-CG50'} (${payload.mode || 'Calculate'})`;
  const res = document.createElement('span');
  res.className = 'step-label';
  res.textContent = payload.expected_result ? `Resultado: ${payload.expected_result}` : '';
  hdr.append(title, res);

  const strip = document.createElement('div');
  strip.className = 'casio-strip';
  for (const k of payload.keys || []) {
    const cap = document.createElement('span');
    cap.className = 'casio-key';
    cap.textContent = String(k);
    strip.appendChild(cap);
  }
  wrap.append(hdr, strip);
  return wrap;
}

// ============================================================================
// Safe Recursive-Descent Math Evaluator (Zero eval(), CSP-safe) + Inline Graph
// ============================================================================

function evalSafeMathExpr(exprStr, xVal) {
  const s = String(exprStr || 'x').replace(/\s+/g, '').replace(/\*\*/g, '^');
  let pos = 0;

  function peek() { return s[pos] || ''; }
  function parseExpr() {
    let val = parseTerm();
    while (peek() === '+' || peek() === '-') {
      const op = s[pos++];
      const rhs = parseTerm();
      val = op === '+' ? val + rhs : val - rhs;
    }
    return val;
  }
  function parseTerm() {
    let val = parsePower();
    while (peek() === '*' || peek() === '/') {
      const op = s[pos++];
      const rhs = parsePower();
      val = op === '*' ? val * rhs : (Math.abs(rhs) < 1e-9 ? NaN : val / rhs);
    }
    return val;
  }
  function parsePower() {
    let base = parseUnary();
    if (peek() === '^') {
      pos++;
      const exp = parsePower();
      base = Math.pow(base, exp);
    }
    return base;
  }
  function parseUnary() {
    if (peek() === '+') { pos++; return parseUnary(); }
    if (peek() === '-') { pos++; return -parseUnary(); }
    return parsePrimary();
  }
  function parsePrimary() {
    if (peek() === '(') {
      pos++;
      const v = parseExpr();
      if (peek() === ')') pos++;
      return v;
    }
    const numMatch = /^\d*\.?\d+(?:[eE][+-]?\d+)?/.exec(s.slice(pos));
    if (numMatch) {
      pos += numMatch[0].length;
      return Number(numMatch[0]);
    }
    const idMatch = /^[A-Za-z_]+/.exec(s.slice(pos));
    if (idMatch) {
      const name = idMatch[0].toLowerCase();
      pos += idMatch[0].length;
      if (peek() === '(') {
        pos++;
        const arg = parseExpr();
        if (peek() === ')') pos++;
        if (name === 'sin') return Math.sin(arg);
        if (name === 'cos') return Math.cos(arg);
        if (name === 'tan') return Math.tan(arg);
        if (name === 'exp') return Math.exp(arg);
        if (name === 'ln' || name === 'log') return arg > 0 ? Math.log(arg) : NaN;
        if (name === 'log10') return arg > 0 ? Math.log10(arg) : NaN;
        if (name === 'sqrt') return arg >= 0 ? Math.sqrt(arg) : NaN;
        if (name === 'abs') return Math.abs(arg);
        return NaN;
      }
      if (name === 'pi') return Math.PI;
      if (name === 'e') return Math.E;
      // Treat any independent variable symbol (x, r, v, t) as xVal
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

export function createInlineConceptGraphWidget(spec) {
  const wrap = document.createElement('div');
  wrap.className = 'inline-graph-pocket';

  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = (spec && spec.title) || 'Explicación visual';
  hdr.appendChild(title);

  const canvas = document.createElement('canvas');
  canvas.className = 'widget-canvas';
  canvas.width = 500;
  canvas.height = 190;
  wrap.append(hdr, canvas);

  if (spec && spec.caption_es) {
    const cap = document.createElement('p');
    cap.className = 'step-note';
    cap.textContent = spec.caption_es;
    wrap.appendChild(cap);
  }

  const kind = (spec && spec.kind) || 'function_plot';
  const xMin = Number(spec && spec.x_min != null ? spec.x_min : 0.15);
  const xMax = Number(spec && spec.x_max != null ? spec.x_max : 1.0);
  const lo = Math.min(xMin, xMax);
  const hi = Math.max(xMin, xMax === xMin ? xMin + 1 : xMax);
  const expr = (spec && spec.expr) || (kind === 'thermal_gradient' ? '100 - 80*(ln(x/0.15)/ln(4))' : '1/(x^2)');

  requestAnimationFrame(() => {
    const ctx = canvas.getContext('2d');
    const W = canvas.width, H = canvas.height;
    const padL = 48, padR = 22, padT = 18, padB = 30;
    const style = getComputedStyle(document.documentElement);
    const ink = style.getPropertyValue('--ink-primary').trim() || '#161a22';
    const sub = style.getPropertyValue('--ink-secondary').trim() || '#4c5666';
    const blue = style.getPropertyValue('--accent-blue').trim() || '#0066e0';
    const orange = style.getPropertyValue('--accent-orange').trim() || '#d97706';

    ctx.clearRect(0, 0, W, H);

    if (kind === 'pv_cycle') {
      // Schematic cyclic P-V loop A -> B -> C -> A
      const pts = [
        { x: padL + 290, y: padT + 30, lbl: 'A' },
        { x: padL + 110, y: padT + 30, lbl: 'B' },
        { x: padL + 290, y: H - padB - 28, lbl: 'C' },
      ];
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      ctx.lineTo(pts[1].x, pts[1].y);
      ctx.quadraticCurveTo(padL + 175, H - padB - 55, pts[2].x, pts[2].y);
      ctx.closePath();
      ctx.fillStyle = blue + '24';
      ctx.fill();
      ctx.strokeStyle = blue;
      ctx.lineWidth = 2.4;
      ctx.stroke();

      for (const p of pts) {
        ctx.beginPath();
        ctx.arc(p.x, p.y, 4.5, 0, Math.PI * 2);
        ctx.fillStyle = orange;
        ctx.fill();
        ctx.fillStyle = ink;
        ctx.font = 'bold 12px Segoe UI, sans-serif';
        ctx.fillText(p.lbl, p.x + 8, p.y - 6);
      }
      ctx.strokeStyle = sub;
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      ctx.moveTo(padL, padT);
      ctx.lineTo(padL, H - padB);
      ctx.lineTo(W - padR, H - padB);
      ctx.stroke();
      ctx.fillStyle = ink;
      ctx.font = '11.5px Segoe UI, sans-serif';
      ctx.fillText((spec && spec.x_label) || 'V (m³)', W - 56, H - 8);
      ctx.fillText((spec && spec.y_label) || 'P (Pa)', 8, 16);
      return;
    }

    // General function_plot / thermal_gradient curve
    const samples = [];
    let yMin = Infinity, yMax = -Infinity;
    for (let i = 0; i <= 100; i++) {
      const x = lo + ((hi - lo) * i) / 100;
      const y = evalSafeMathExpr(expr, x);
      if (Number.isFinite(y)) {
        samples.push({ x, y });
        yMin = Math.min(yMin, y);
        yMax = Math.max(yMax, y);
      }
    }
    if (samples.length < 2) return;
    if (yMin > 0) yMin = 0;
    const spanY = Math.max(1e-6, yMax - yMin) * 1.15;
    const sx = (x) => padL + ((x - lo) / Math.max(1e-6, hi - lo)) * (W - padL - padR);
    const sy = (y) => H - padB - ((y - yMin) / spanY) * (H - padT - padB);

    // Optional shaded area
    const shA = spec && spec.shade_from != null ? Number(spec.shade_from) : lo;
    const shB = spec && spec.shade_to != null ? Number(spec.shade_to) : hi;
    const shPts = samples.filter((p) => p.x >= Math.min(shA, shB) && p.x <= Math.max(shA, shB));
    if (shPts.length >= 2) {
      ctx.beginPath();
      ctx.moveTo(sx(shPts[0].x), sy(yMin));
      for (const p of shPts) ctx.lineTo(sx(p.x), sy(p.y));
      ctx.lineTo(sx(shPts[shPts.length - 1].x), sy(yMin));
      ctx.closePath();
      ctx.fillStyle = blue + '24';
      ctx.fill();
    }

    // Curve
    ctx.beginPath();
    ctx.strokeStyle = kind === 'thermal_gradient' ? orange : blue;
    ctx.lineWidth = 2.5;
    samples.forEach((p, idx) => {
      if (idx === 0) ctx.moveTo(sx(p.x), sy(p.y));
      else ctx.lineTo(sx(p.x), sy(p.y));
    });
    ctx.stroke();

    // Axes & min/max ticks
    ctx.strokeStyle = sub;
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(padL, padT);
    ctx.lineTo(padL, H - padB);
    ctx.lineTo(W - padR, H - padB);
    ctx.stroke();

    ctx.fillStyle = ink;
    ctx.font = '11px Segoe UI, sans-serif';
    ctx.fillText(lo.toFixed(2), padL - 6, H - 10);
    ctx.fillText(hi.toFixed(2), W - padR - 26, H - 10);
    ctx.fillText((spec && spec.x_label) || 'x', (padL + W - padR) / 2 - 12, H - 10);
    ctx.fillText((spec && spec.y_label) || 'f(x)', 6, 15);
  });

  return wrap;
}
