import { DuplexAudioEngine } from './audio_duplex_worklet.js';
import { createHandDrawnSketchBlock } from './handwriting_sketch_engine.js';
import {
  MarkerAnnotationsEngine,
  extractSymbolAtClientPoint,
  extractTextInsideClientRect,
  getContainedImageClientRect,
  getStageScale,
  resolveMarkerHex,
} from './marker_annotations_engine.js';
import { hitTestMathTerms, renderMath2D } from './math_handwriting_svg.js';
import {
  createCasioKeycapStrip,
  createRiemannWorkWidget,
} from './stem_widgets.js';

const params = new URLSearchParams(window.location.search);
const backendPort = params.get('port') || window.location.port || '8765';
const sessionToken = params.get('token') || '';
const apiBase = `http://127.0.0.1:${backendPort}`;
const wsBase = `ws://127.0.0.1:${backendPort}/ws/tutor${sessionToken ? `?token=${encodeURIComponent(sessionToken)}` : ''}`;
const EXPECTED_RUNTIME_BUILD = '2026-11-25-universal-v6-hardened';

// DOM refs
const elBoardGrid = document.getElementById('board-grid');
const elDerivation = document.getElementById('derivation-column');
const elVisualLab = document.getElementById('visual-lab-zone');
const elMediaLayer = document.getElementById('media-layer-zone');
const elFormulaDock = document.getElementById('formula-dock-zone');
const elClarification = document.getElementById('clarification-zone');
const elProvenanceSvg = document.getElementById('provenance-svg');
const elMarkerSvg = document.getElementById('marker-layer-svg');
const elWelcome = document.getElementById('empty-welcome');
const elThinking = document.getElementById('thinking-indicator');
const elThinkingLabel = document.getElementById('thinking-label');
const elViewport = document.getElementById('board-viewport');
const elStage = document.getElementById('board-stage');
const elInkCanvas = document.getElementById('ink-canvas');

const elConnDot = document.getElementById('conn-dot');
const elStateLabel = document.getElementById('session-state-label');
const elCaptionStrip = document.getElementById('caption-strip');
const elCaptionSpeaker = document.getElementById('caption-speaker');
const elCaptionText = document.getElementById('caption-text');
const elFollowups = document.getElementById('followup-chips');
const elPracticeBar = document.getElementById('practice-bar');
const elPracticePrompt = document.getElementById('practice-prompt');
const elPracticeInput = document.getElementById('practice-input');
const elPracticeFeedback = document.getElementById('practice-feedback');
const elCirclePop = document.getElementById('circle-popover');
const elCircleTitle = document.getElementById('circle-popover-title');
const elPopCustomInput = document.getElementById('pop-custom-input');
const elA11yStatus = document.getElementById('a11y-status');

// State
let ws = null;
let committedBeats = [];
let activeBeat = null;
let pendingCueActions = [];
let completedActionIds = [];
let currentWordIdx = 0;
let activeWordSpans = [];
let activeAlignmentWords = [];
let liveVoiceActive = false;
let activeTool = 'pointer'; // 'pointer' | 'pan' | 'lasso' | 'pen'
let reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
let pendingCircleSelection = null;
let expectedPracticeExpr = null;
let expectedPracticeConcept = 'calc.int.general';
let penStrokes = [];
let drawingPath = null;
let activeLassoSelection = null; // keeps student's red dashed circle visible + composited for Gemini Live
let studentPenColor = 'blue';
const cropWordsByObjectId = new Map();
let loadedPdfDocId = null;
let lastMathObjectId = '';
let selectedMicDeviceId = '';
let liveCaptionSpeaker = '';
let liveCaptionBuffer = '';
let micPeakRmsRecent = 0;
let micSilenceWatchTimer = null;
let serverConfirmedMic = false;
let boardSnapshotDebounce = null;

// Real-time Live Speech <-> Whiteboard Synchronization State
let liveTurnAudioStarted = false;
let liveTurnCompleted = false;
let liveTurnTranscriptFolded = '';
let liveActionQueue = [];
let liveActionDraining = false;
let liveOnsetFallbackTimer = null;
let liveSyncGeneration = 0;
let liveTurnAudioOnsetPerf = 0;
const liveTranscriptListeners = new Set();

const RAW_LATEX_LEAK_RE = /\\(?:frac|sqrt|int|gamma|Delta|theta|pi|alpha|beta|text|mathrm|mathbf|left|right|cdot|times)\b/;

/** Inspect live DOM geometry for overlapping step blocks/badges, horizontal clipping, or leaked raw LaTeX. */
function auditBoardLayoutCollisions() {
  const overlaps = [];
  const clipped = [];
  const rawLatexLeaks = [];
  const vr = elViewport ? elViewport.getBoundingClientRect() : null;

  const blocks = Array.from(elDerivation ? elDerivation.querySelectorAll(':scope > .step-block') : []);
  const rects = blocks.map((el) => ({ id: el.dataset.objectId || '', rect: el.getBoundingClientRect() }));
  for (let i = 0; i < rects.length - 1; i++) {
    const a = rects[i].rect;
    const b = rects[i + 1].rect;
    if (a.height > 4 && b.height > 4 && a.bottom - b.top > 6) {
      overlaps.push({ kind: 'step_block', a: rects[i].id, b: rects[i + 1].id, overlap_px: Math.round(a.bottom - b.top) });
    }
  }

  const badges = Array.from(elMarkerSvg ? elMarkerSvg.querySelectorAll('.marker-hand-badge') : []);
  const badgeRects = badges.map((el) => ({ text: (el.textContent || '').slice(0, 32), rect: el.getBoundingClientRect() }));
  for (let i = 0; i < badgeRects.length; i++) {
    for (let j = i + 1; j < badgeRects.length; j++) {
      const a = badgeRects[i].rect;
      const b = badgeRects[j].rect;
      if (a.width > 2 && b.width > 2 && !(a.right < b.left || a.left > b.right || a.bottom < b.top || a.top > b.bottom)) {
        overlaps.push({ kind: 'marker_badge', a: badgeRects[i].text, b: badgeRects[j].text });
      }
    }
  }

  // Detect overflow clipping on math hosts and tables
  document.querySelectorAll('.math-render-host, .wb-hand-table').forEach((el) => {
    if (el.scrollWidth > el.clientWidth + 16) {
      clipped.push({
        cls: el.className.slice(0, 40),
        scroll_w: Math.round(el.scrollWidth),
        client_w: Math.round(el.clientWidth),
      });
    }
  });

  // Detect any raw LaTeX text visible to the student
  blocks.forEach((blk) => {
    const txt = (blk.innerText || '').trim();
    const m = RAW_LATEX_LEAK_RE.exec(txt);
    if (m) {
      rawLatexLeaks.push({ object_id: blk.dataset.objectId || '', token: m[0], snippet: txt.slice(0, 80) });
    }
  });

  return {
    viewport: vr ? { w: Math.round(vr.width), h: Math.round(vr.height) } : null,
    theme: document.documentElement.getAttribute('data-theme') || 'light',
    zoom_pct: Math.round(boardZoom * 100),
    pan: { x: Math.round(boardPanX), y: Math.round(boardPanY) },
    step_count: blocks.length,
    marker_count: elMarkerSvg ? elMarkerSvg.querySelectorAll('.marker-group').length : 0,
    overlaps,
    clipped,
    raw_latex_leaks: rawLatexLeaks,
  };
}

function reportIncidentToFlightRecorder(reason = 'manual_bug_mark', note = '') {
  const domAudit = auditBoardLayoutCollisions();
  const snapB64 = captureBoardSnapshot('image/jpeg');
  sendWs({
    type: 'USER_REPORT_INCIDENT',
    reason,
    note,
    image_b64: snapB64 || '',
    mime_type: 'image/jpeg',
    dom_audit: domAudit,
  });
}

// Expose read-only diagnostics hook for automated visual stress audits
window.__cuadernuAudit = {
  auditBoardLayoutCollisions,
  panBlockIntoView: (objId) => {
    const el = elDerivation ? elDerivation.querySelector(`[data-object-id="${CSS.escape(String(objId || ''))}"]`) : null;
    if (el) panElementIntoView(el, { alignTop: true });
  },
  getBoardState: () => ({
    zoom: boardZoom,
    panX: boardPanX,
    panY: boardPanY,
    liveVoiceActive,
    liveTurnAudioStarted,
    liveTurnCompleted,
    queuedLiveActions: liveActionQueue.length,
    penStrokeCount: penStrokes.length,
    committedSteps: elDerivation ? elDerivation.querySelectorAll('.step-block').length : 0,
  }),
};

window.addEventListener('error', (ev) => {
  sendWs({
    type: 'CLIENT_TELEMETRY_EVENT',
    kind: 'js_error',
    severity: 'error',
    data: { message: String(ev.message || 'error'), source: String(ev.filename || ''), line: Number(ev.lineno || 0) },
  });
});
window.addEventListener('unhandledrejection', (ev) => {
  sendWs({
    type: 'CLIENT_TELEMETRY_EVENT',
    kind: 'js_error',
    severity: 'error',
    data: { message: String(ev.reason || 'unhandledrejection'), source: 'promise' },
  });
});

const SYNC_STOPWORDS = new Set([
  'los', 'las', 'del', 'con', 'por', 'para', 'una', 'uno', 'unos', 'unas',
  'que', 'como', 'mas', 'pero', 'son', 'esta', 'este', 'estos', 'estas',
  'entre', 'sobre', 'desde', 'hasta', 'donde', 'cuando', 'sus', 'muy',
  'paso', 'nota', 'zona', 'region', 'pueblo', 'pueblos', 'costa', 'rica',
]);

function foldSyncText(s) {
  return String(s || '')
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .toLowerCase();
}

function extractSyncKeywords(text) {
  const folded = foldSyncText(text);
  const rawTokens = folded.match(/[a-z0-9]{3,}/g) || [];
  const uniq = [];
  for (const tok of rawTokens) {
    if (SYNC_STOPWORDS.has(tok)) continue;
    if (!uniq.includes(tok)) uniq.push(tok);
    if (uniq.length >= 4) break;
  }
  return uniq;
}

function notifyLiveTranscriptWatchers() {
  for (const fn of Array.from(liveTranscriptListeners)) {
    try { fn(); } catch {}
  }
}

function resetLiveTurnSyncState({ abortInFlight = false } = {}) {
  if (liveOnsetFallbackTimer) {
    clearTimeout(liveOnsetFallbackTimer);
    liveOnsetFallbackTimer = null;
  }
  if (abortInFlight) {
    liveSyncGeneration += 1;
    liveActionQueue = [];
    liveActionDraining = false;
  }
  liveTurnAudioStarted = false;
  liveTurnCompleted = false;
  liveTurnTranscriptFolded = '';
  audioEngine.resetLiveTurnAudioFlag();
  notifyLiveTranscriptWatchers();
}

function waitForLiveTranscriptOrPace(keywords, maxWaitMs, gen) {
  if (reduceMotion || !liveVoiceActive) return Promise.resolve();
  const hasMatch = () => {
    if (!keywords || !keywords.length) return false;
    return keywords.some((kw) => liveTurnTranscriptFolded.includes(kw));
  };
  if (hasMatch() || liveTurnCompleted || gen !== liveSyncGeneration) {
    return Promise.resolve();
  }
  return new Promise((resolve) => {
    let settled = false;
    let lastNow = performance.now();
    let waitedMs = 0;
    let rafId = null;

    const finish = () => {
      if (settled) return;
      settled = true;
      liveTranscriptListeners.delete(onTranscript);
      if (rafId !== null) cancelAnimationFrame(rafId);
      resolve();
    };
    const onTranscript = () => {
      if (hasMatch() || liveTurnCompleted || gen !== liveSyncGeneration) finish();
    };
    liveTranscriptListeners.add(onTranscript);

    const tick = (now) => {
      if (settled) return;
      if (liveTurnCompleted || gen !== liveSyncGeneration || hasMatch()) {
        finish();
        return;
      }
      const dt = Math.min(80, Math.max(0, now - lastNow));
      lastNow = now;
      const spd = audioEngine.isLiveAudioPlaying() ? audioEngine.getLiveSpeechSpeedFactor() : 0.7;
      waitedMs += dt * Math.max(0.25, spd);
      if (waitedMs >= maxWaitMs) {
        finish();
        return;
      }
      rafId = requestAnimationFrame(tick);
    };
    rafId = requestAnimationFrame(tick);
  });
}

async function drainLiveActionQueue() {
  if (liveActionDraining) return;
  if (liveVoiceActive && !liveTurnAudioStarted) return;
  liveActionDraining = true;
  const myGen = liveSyncGeneration;
  try {
    while (liveActionQueue.length > 0) {
      if (myGen !== liveSyncGeneration) break;
      const nextItem = liveActionQueue.shift();
      if (!nextItem || !nextItem.action) continue;
      // For overlays (cancel_terms / annotate / trace), wait briefly for matching voice cue if already talking
      const actType = nextItem.action.type;
      if (
        liveVoiceActive &&
        (actType === 'cancel_terms' || actType === 'underline_term' || actType === 'circle_term' || actType === 'highlight_term')
      ) {
        const p = nextItem.action.payload || {};
        const kws = [];
        if (Array.isArray(p.pairs)) {
          for (const pr of p.pairs) {
            if (pr && pr.unit_or_term) kws.push(...extractSyncKeywords(pr.unit_or_term));
          }
        }
        if (p.match_text) kws.push(...extractSyncKeywords(p.match_text));
        if (kws.length) {
          await waitForLiveTranscriptOrPace(kws, 1400, myGen);
          if (myGen !== liveSyncGeneration) break;
        }
      }
      const writeStartPerf = performance.now();
      const onsetDelta = liveTurnAudioOnsetPerf > 0 ? Math.max(0, Math.round(writeStartPerf - liveTurnAudioOnsetPerf)) : 0;
      await executeCanvasAction(nextItem.action, nextItem.synthBeat, { liveSynced: true, syncGen: myGen });
      const writeDurMs = Math.round(performance.now() - writeStartPerf);
      sendWs({
        type: 'CLIENT_TELEMETRY_EVENT',
        kind: 'sync_metrics',
        severity: 'info',
        data: {
          action_type: actType,
          object_id: nextItem.action.object_id || '',
          onset_to_write_ms: onsetDelta,
          write_duration_ms: writeDurMs,
        },
      });
      const domCheck = auditBoardLayoutCollisions();
      if (domCheck.overlaps.length || domCheck.raw_latex_leaks.length) {
        sendWs({
          type: 'CLIENT_TELEMETRY_EVENT',
          kind: 'dom_audit_anomaly',
          severity: 'warn',
          data: {
            overlaps: domCheck.overlaps,
            raw_latex_leaks: domCheck.raw_latex_leaks,
            clipped: domCheck.clipped,
          },
        });
      }
    }
  } finally {
    liveActionDraining = false;
    if (liveActionQueue.length > 0 && myGen === liveSyncGeneration && (!liveVoiceActive || liveTurnAudioStarted)) {
      drainLiveActionQueue();
    }
  }
}

function enqueueLiveCanvasAction(act, synthBeat) {
  liveActionQueue.push({ action: act, synthBeat });
  if (!liveVoiceActive || liveTurnAudioStarted) {
    const leadMs = Math.min(180, audioEngine.getLivePlaybackLeadMs());
    if (leadMs > 28 && !liveActionDraining) {
      setTimeout(() => drainLiveActionQueue(), leadMs);
    } else {
      drainLiveActionQueue();
    }
    return;
  }
  // Hold until Gemini Live's FIRST PCM24 audio chunk starts playing (so pen never starts before voice!),
  // with a safe 2600ms fallback covering tool_call -> tool_response -> first audio cloud latency.
  if (!liveOnsetFallbackTimer) {
    const armedGen = liveSyncGeneration;
    liveOnsetFallbackTimer = setTimeout(() => {
      liveOnsetFallbackTimer = null;
      if (armedGen === liveSyncGeneration) {
        liveTurnAudioStarted = true;
        drainLiveActionQueue();
      }
    }, 2600);
  }
}

// Interactive Pan & Zoom Camera State
let boardZoom = 1.0;
let boardPanX = 0;
let boardPanY = 0;
let panDragState = null;
let spacePanHeld = false;
let spacePanDragged = false;

function applyBoardTransform() {
  elStage.style.transform = `translate(${boardPanX.toFixed(1)}px, ${boardPanY.toFixed(1)}px) scale(${boardZoom.toFixed(4)})`;
  const dotStep = Math.max(10, 24 * boardZoom);
  elViewport.style.backgroundSize = `${dotStep.toFixed(2)}px ${dotStep.toFixed(2)}px`;
  elViewport.style.backgroundPosition = `${boardPanX.toFixed(1)}px ${boardPanY.toFixed(1)}px`;
  const pctBtn = document.getElementById('btn-zoom-reset');
  if (pctBtn) pctBtn.textContent = `${Math.round(boardZoom * 100)}%`;
}

function zoomBoardAroundClient(nextZoom, clientX, clientY) {
  const clamped = Math.max(0.45, Math.min(2.4, nextZoom));
  if (Math.abs(clamped - boardZoom) < 0.001) return;
  const sr = elStage.getBoundingClientRect();
  const stagePtX = (clientX - sr.left) / boardZoom;
  const stagePtY = (clientY - sr.top) / boardZoom;
  boardPanX += (clientX - sr.left) - stagePtX * clamped;
  boardPanY += (clientY - sr.top) - stagePtY * clamped;
  boardZoom = clamped;
  applyBoardTransform();
}

function zoomBoardByStep(deltaMultiplier) {
  const vr = elViewport.getBoundingClientRect();
  // Anchor zoom near left reading margin so equations NEVER clip off the left edge when zooming in
  const anchorX = vr.left + Math.min(200, vr.width * 0.16);
  const anchorY = vr.top + vr.height * 0.36;
  zoomBoardAroundClient(boardZoom * deltaMultiplier, anchorX, anchorY);
  if (boardPanX > 48) {
    boardPanX = 0;
    applyBoardTransform();
  }
}

function resetBoardCamera() {
  boardZoom = 1.0;
  boardPanX = 0;
  boardPanY = 0;
  applyBoardTransform();
}

function panElementIntoView(el, { alignTop = false } = {}) {
  if (!el || !elViewport) return;
  const vr = elViewport.getBoundingClientRect();
  const er = el.getBoundingClientRect();
  // For tall blocks (e.g. PDF audits or large sketches), always align their top inside view
  if (alignTop || er.height > vr.height * 0.72) {
    const desiredTop = vr.top + 22;
    if (er.top < desiredTop - 8 || er.top > vr.bottom - 140) {
      boardPanY += desiredTop - er.top;
      if (boardPanY > 0) boardPanY = 0;
      applyBoardTransform();
    }
    return;
  }
  if (er.bottom > vr.bottom - 30) {
    boardPanY -= er.bottom - (vr.bottom - 30);
    if (boardPanY > 0) boardPanY = 0;
    applyBoardTransform();
  } else if (er.top < vr.top + 18) {
    boardPanY += (vr.top + 18) - er.top;
    if (boardPanY > 0) boardPanY = 0;
    applyBoardTransform();
  }
}

const markerEngine = new MarkerAnnotationsEngine(elStage, elMarkerSvg, {
  reduceMotion,
  onEnsureVisible: (el) => panElementIntoView(el),
});

function announce(msg) {
  elA11yStatus.textContent = msg;
}

function setStatus(label, mode = 'connected') {
  elStateLabel.textContent = label;
  elConnDot.className = `status-dot ${mode}`;
}

function setThinking(visible, label = 'El profesor está preparando el dibujo y el paso…') {
  if (!elThinking) return;
  elThinkingLabel.textContent = label;
  elThinking.hidden = !visible;
  if (visible) {
    elDerivation.appendChild(elThinking);
    panElementIntoView(elThinking);
  }
}

function syncTwoColumnLayout() {
  const hasRightContent =
    elVisualLab.childElementCount > 0 ||
    elMediaLayer.childElementCount > 0 ||
    elFormulaDock.childElementCount > 0 ||
    elClarification.childElementCount > 0;
  elBoardGrid.classList.toggle('single-col', !hasRightContent);
  resizeInkCanvas();
}

/** Animate professor handwriting line character-by-character directly on the dot-grid (speech-paced). */
function animateHandwrittenText(el, fullText, durationMs = 0, { getSpeedFactor = null, isAborted = null } = {}) {
  const txt = String(fullText || '');
  if (reduceMotion || txt.length <= 2) {
    el.textContent = txt;
    return Promise.resolve();
  }
  el.textContent = '';
  const autoDur = Math.max(650, Math.min(4800, txt.length * 46));
  const dur = durationMs > 0 ? Math.max(480, Math.min(5500, durationMs)) : autoDur;
  return new Promise((resolve) => {
    let lastNow = performance.now();
    let progressMs = 0;
    function tick(now) {
      if (isAborted && isAborted()) {
        // Stop handwriting right where the student interrupted instead of dumping remaining text
        resolve();
        return;
      }
      const dt = Math.min(80, Math.max(0, now - lastNow));
      lastNow = now;
      const speed = typeof getSpeedFactor === 'function' ? Math.max(0.18, Math.min(2.2, Number(getSpeedFactor()) || 1.0)) : 1.0;
      progressMs += dt * speed;
      const p = Math.min(1, progressMs / dur);
      const n = Math.max(1, Math.round(txt.length * p));
      el.textContent = txt.slice(0, n);
      if (p < 1) {
        requestAnimationFrame(tick);
      } else {
        el.textContent = txt;
        resolve();
      }
    }
    requestAnimationFrame(tick);
  });
}

// Audio engine
const audioEngine = new DuplexAudioEngine({
  onMicFramePcm16: (packetBuf) => {
    if (ws && ws.readyState === WebSocket.OPEN && liveVoiceActive) {
      ws.send(packetBuf);
    }
  },
  onMicLevel: (rms) => {
    micPeakRmsRecent = Math.max(micPeakRmsRecent * 0.92, rms);
    const fill = document.getElementById('mic-vu-fill');
    if (fill) {
      const pct = Math.min(100, Math.max(8, Math.round(Math.min(1, rms * 14) * 100)));
      fill.style.width = `${pct}%`;
    }
  },
  onPlaybackClockMs: (ms) => {
    driveSyncedTimeline(ms);
  },
  onPlaybackEnded: () => {
    flushRemainingCueActions();
    if (!liveVoiceActive) setStatus('Listo · responde, dibuja o pregunta', 'connected');
  },
  onLiveAudioStart: (delayMs) => {
    if (liveOnsetFallbackTimer) {
      clearTimeout(liveOnsetFallbackTimer);
      liveOnsetFallbackTimer = null;
    }
    liveTurnCompleted = false;
    const armedGen = liveSyncGeneration;
    setTimeout(() => {
      if (armedGen !== liveSyncGeneration) return;
      liveTurnAudioStarted = true;
      liveTurnAudioOnsetPerf = performance.now();
      drainLiveActionQueue();
    }, Math.max(0, Math.min(220, delayMs + 30)));
  },
  onLiveAudioDrain: () => {
    liveTurnCompleted = true;
    notifyLiveTranscriptWatchers();
  },
});

async function refreshMicDeviceOptions() {
  const sel = document.getElementById('opt-mic-select');
  if (!sel) return;
  const inputs = await audioEngine.listAudioInputs();
  const prev = selectedMicDeviceId || sel.value;
  sel.replaceChildren();
  const defOpt = document.createElement('option');
  defOpt.value = '';
  defOpt.textContent = 'Predeterminado de Windows';
  sel.appendChild(defOpt);
  inputs.forEach((d, idx) => {
    const o = document.createElement('option');
    o.value = d.deviceId;
    o.textContent = d.label || `Micrófono ${idx + 1}`;
    sel.appendChild(o);
  });
  if (prev && Array.from(sel.options).some((o) => o.value === prev)) {
    sel.value = prev;
  }
}

function sendWs(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

function connectWebSocket() {
  ws = new WebSocket(wsBase);
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    setStatus('Conectado · Pizarra lista', 'connected');
  };

  ws.onclose = () => {
    setStatus('Reconectando…', '');
    setTimeout(connectWebSocket, 1800);
  };

  ws.onmessage = async (ev) => {
    if (ev.data instanceof ArrayBuffer) {
      const view = new Uint8Array(ev.data);
      if (view.byteLength > 8 && view[0] === 0x43 && view[1] === 0x55 && view[2] === 2) {
        audioEngine.enqueueLivePcm24(view.subarray(8));
      }
      return;
    }
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }

    switch (msg.type) {
      case 'SESSION_READY':
        if (msg.runtime_build !== EXPECTED_RUNTIME_BUILD || msg.code_stale) {
          setStatus('Servidor desactualizado · reinicia server.py para activar las mejoras', '');
        } else {
          setStatus(msg.visual_cloud_ready ? 'Listo · Profesor universal en línea' : 'Listo · Modo local', 'connected');
        }
        break;
      case 'BOARD_RESET':
        resetBoardVisuals({
          clearInk: msg.clear_ink !== false,
          clearMedia: Boolean(msg.clear_media),
          keepLiveVoice: liveVoiceActive,
        });
        break;
      case 'BEAT_PLAN':
        setThinking(false);
        queueBeatForSync(msg.beat, msg.meta || {});
        break;
      case 'LIVE_CANVAS_ACTION': {
        setThinking(false);
        if (elWelcome && elWelcome.parentElement) elWelcome.remove();
        const act = msg.action;
        if (!act) break;
        const synthBeat = { step_index: Number(msg.step_index || committedBeats.length + 1) };
        enqueueLiveCanvasAction(act, synthBeat);
        break;
      }
      case 'LASSO_VISION_ACK':
        if (msg.sent_to_live) {
          const sym = msg.circled_text ? `"${msg.circled_text.slice(0, 36)}"` : 'tu marca en rojo';
          const where = msg.step_title ? ` (${msg.step_title.slice(0, 28)})` : '';
          setStatus(`El profesor ya ve ${sym}${where} · pregúntale ahora`, 'live');
        }
        break;
      case 'PASTED_IMAGE_VISION_ACK':
        setThinking(false);
        if (msg.sent_to_live) {
          setStatus('El profesor ya ve tu imagen pegada · puedes preguntarle ahora', 'live');
        }
        break;
      case 'REQUEST_INCIDENT_SNAPSHOT':
        reportIncidentToFlightRecorder(msg.reason || 'auto_incident', msg.note || '');
        break;
      case 'INCIDENT_SAVED':
        if (msg.ok) {
          setStatus('Fallo guardado en telemetría local (private_local/telemetry)', liveVoiceActive ? 'live' : 'connected');
          announce('Fallo guardado en registro local');
        }
        break;
      case 'AUDIO_ALIGNMENT':
        setThinking(false);
        startBeatPlaybackWithAlignment(msg);
        break;
      case 'CONTROL_HARD_STOP':
        audioEngine.hardStopPlayback();
        resetLiveTurnSyncState({ abortInFlight: true });
        setStatus('Interrupción detectada · te escucho', 'live');
        break;
      case 'ASR_FINAL': {
        const chunk = msg.text || '';
        if (liveCaptionSpeaker !== 'user') {
          liveCaptionSpeaker = 'user';
          liveCaptionBuffer = chunk;
          // Student started a new spoken turn -> arm audio-onset gate for the tutor's next reply
          resetLiveTurnSyncState({ abortInFlight: false });
        } else {
          liveCaptionBuffer += chunk;
        }
        elCaptionStrip.hidden = false;
        elCaptionSpeaker.textContent = 'Tú';
        elCaptionText.textContent = liveCaptionBuffer.trim();
        break;
      }
      case 'LIVE_OUTPUT_TRANSCRIPT': {
        const chunk = msg.text || '';
        if (liveCaptionSpeaker !== 'tutor') {
          liveCaptionSpeaker = 'tutor';
          liveCaptionBuffer = chunk;
          liveTurnTranscriptFolded = foldSyncText(chunk);
          liveTurnCompleted = false;
        } else {
          liveCaptionBuffer += chunk;
          liveTurnTranscriptFolded = foldSyncText(liveCaptionBuffer);
        }
        notifyLiveTranscriptWatchers();
        elCaptionStrip.hidden = false;
        elCaptionSpeaker.textContent = 'Profesor en vivo';
        elCaptionText.textContent = liveCaptionBuffer.trim();
        break;
      }
      case 'MIC_STREAM_CONFIRMED':
        if (!serverConfirmedMic) {
          serverConfirmedMic = true;
          setStatus('Te escucho en vivo · habla normal cuando quieras', 'live');
        }
        break;
      case 'LIVE_VOICE_STATUS':
        if (msg.ok && msg.model) {
          liveVoiceActive = true;
          serverConfirmedMic = false;
          liveCaptionSpeaker = '';
          liveCaptionBuffer = '';
          document.getElementById('btn-live-mic').setAttribute('aria-pressed', 'true');
          document.getElementById('mic-vu-track').hidden = false;
          setStatus('Voz en vivo activa · enviando audio…', 'live');
          announce('Voz en vivo activada');
          // Immediately share what is already drawn/pasted on the whiteboard so Gemini Live sees it on Turn 1!
          sendBoardSnapshotToLive();
          if (micSilenceWatchTimer) clearTimeout(micSilenceWatchTimer);
          micSilenceWatchTimer = setTimeout(() => {
            if (liveVoiceActive && micPeakRmsRecent < 0.002) {
              setStatus('Sin señal del micrófono · cambia dispositivo en Más opciones (⋮)', '');
            }
          }, 4000);
        } else if (msg.active === false) {
          liveVoiceActive = false;
          serverConfirmedMic = false;
          if (micSilenceWatchTimer) clearTimeout(micSilenceWatchTimer);
          document.getElementById('btn-live-mic').setAttribute('aria-pressed', 'false');
          document.getElementById('mic-vu-track').hidden = true;
          setStatus('Voz en vivo detenida', 'connected');
        } else if (!msg.ok) {
          liveVoiceActive = false;
          serverConfirmedMic = false;
          if (micSilenceWatchTimer) clearTimeout(micSilenceWatchTimer);
          audioEngine.stopMicCapture();
          document.getElementById('btn-live-mic').setAttribute('aria-pressed', 'false');
          document.getElementById('mic-vu-track').hidden = true;
          setStatus(`Error de voz en vivo: ${msg.error || 'sin conexión'}`, '');
        }
        break;
      case 'STATE_CHANGE':
        if (msg.state === 'THINKING_VERIFYING') {
          setThinking(true, 'El profesor está preparando el dibujo y el siguiente paso…');
          setStatus('Dibujando y verificando paso…', '');
        } else if (msg.state === 'WAITING_FOR_STUDENT') {
          setThinking(false);
          liveCaptionSpeaker = '';
          liveTurnCompleted = true;
          notifyLiveTranscriptWatchers();
          if (liveActionQueue.length > 0) {
            liveTurnAudioStarted = true;
            drainLiveActionQueue();
          } else {
            liveTurnAudioStarted = false;
            audioEngine.resetLiveTurnAudioFlag();
          }
          setStatus(liveVoiceActive ? 'Te escucho · interrumpe o responde' : 'Esperando tu respuesta', liveVoiceActive ? 'live' : 'connected');
        }
        break;
      case 'STUDENT_WORK_VERIFIED':
        renderPracticeResult(msg.verification);
        break;
      case 'PDF_INDEXED':
        setThinking(false);
        loadedPdfDocId = msg.doc_id || loadedPdfDocId;
        document.getElementById('btn-audit-pdf').hidden = !loadedPdfDocId;
        if (msg.first_page) {
          addPdfPageCard(msg.doc_id, msg.filename, msg.page_count, msg.first_page, msg.problem_count || 0);
        }
        setStatus(`PDF listo: ${msg.filename} (${msg.problem_count || 0} problemas)`, 'connected');
        announce(`PDF ${msg.filename} indexado con ${msg.problem_count || 0} problemas`);
        break;
      case 'ERROR_RECOVERY':
        setThinking(false);
        setStatus(`Aviso: ${msg.message || 'recuperando conexión'}`, '');
        break;
      default:
        break;
    }
  };
}

function resetBoardVisuals({ clearInk = true, clearMedia = false, keepLiveVoice = false } = {}) {
  resetLiveTurnSyncState({ abortInFlight: true });
  if (!keepLiveVoice) {
    audioEngine.hardStopPlayback();
  }
  if (boardSnapshotDebounce) {
    clearTimeout(boardSnapshotDebounce);
    boardSnapshotDebounce = null;
  }
  setThinking(false);
  committedBeats = [];
  activeBeat = null;
  pendingCueActions = [];
  lastMathObjectId = '';
  elDerivation.replaceChildren();
  elVisualLab.replaceChildren();
  elFormulaDock.replaceChildren();
  elClarification.replaceChildren();
  elProvenanceSvg.replaceChildren();
  markerEngine.clearAll();
  cropWordsByObjectId.clear();
  activeLassoSelection = null;
  resetBoardCamera();
  if (clearMedia) {
    elMediaLayer.replaceChildren();
  }
  if (clearInk) {
    penStrokes = [];
    drawingPath = null;
    redrawInkCanvas();
  }
  elPracticeBar.hidden = true;
  if (!keepLiveVoice) elCaptionStrip.hidden = true;
  elCirclePop.hidden = true;
  syncTwoColumnLayout();
  // Send a blank board image if live voice is open so the tutor knows the board is actually empty now
  if (liveVoiceActive) {
    const off = document.createElement('canvas');
    off.width = 512;
    off.height = 320;
    const octx = off.getContext('2d');
    octx.fillStyle = document.documentElement.getAttribute('data-theme') === 'dark' ? '#13171f' : '#ffffff';
    octx.fillRect(0, 0, 512, 320);
    const blankB64 = off.toDataURL('image/jpeg', 0.6).split(',')[1];
    if (blankB64) sendWs({ type: 'SEND_CANVAS_SNAPSHOT', image_b64: blankB64, mime_type: 'image/jpeg' });
  }
}

/** Safety net: if the model sent only plain text on a physics topic without any drawing, attach a real hand-drawn sketch automatically. */
function ensureVisualSketchIfConceptual(beat) {
  const actions = Array.isArray(beat.actions) ? beat.actions : [];
  const hasVisual = actions.some((a) =>
    ['draw_sketch', 'plot_riemann_sum', 'plot_pv_diagram', 'draw_thermal_shell', 'draw_inline_graph', 'paste_problem_clip', 'render_pdf_audit'].includes(a.type)
  );
  if (hasVisual) return actions;

  const combined = `${beat.speech_markup || ''} ${actions.map((a) => JSON.stringify(a.payload || {})).join(' ')}`.toLowerCase();
  let kind = '';
  let title = '';
  if (/entrop|segunda ley|desorden/.test(combined)) {
    kind = 'entropy_particles';
    title = 'Segunda Ley y Entropía (ΔS > 0)';
  } else if (/primera ley|termodin|pist[oó]n|energ[ií]a interna/.test(combined)) {
    kind = 'piston_cylinder';
    title = 'Primera Ley: Conservación de la Energía';
  } else if (/carnot|m[aá]quina|refrigerador|rendimiento/.test(combined)) {
    kind = 'heat_engine_carnot';
    title = 'Ciclo Térmico entre TH y TC';
  } else if (/esfer|cilindr/.test(combined)) {
    kind = 'thermal_sphere';
    title = 'Conducción Radial en Cascarón dr';
  } else if (/fourier|conducci[oó]n|pared|barra/.test(combined)) {
    kind = 'thermal_bar';
    title = 'Conducción de Calor (Ley de Fourier)';
  } else if (/ciclo|adiab[aá]tic|isob[aá]ric|isoc[oó]ric/.test(combined)) {
    kind = 'pv_cycle';
    title = 'Ciclo Termodinámico P–V';
  }

  if (!kind) return actions;
  return [
    {
      action_id: `act_autosketch_${beat.beat_id || Date.now()}`,
      type: 'draw_sketch',
      object_id: `sketch_auto_${beat.beat_id || Date.now()}`,
      placement: { zone: 'derivation_flow', relation: 'auto_next', gap_px: 18 },
      animation: { style: 'draw_stroke', duration_ms: 2100, cue_id: 'c1' },
      color_token: 'accent_blue',
      payload: { kind, title },
    },
    ...actions,
  ];
}

function queueBeatForSync(beat, meta) {
  if (elWelcome && elWelcome.parentElement) elWelcome.remove();
  committedBeats.push(beat);
  activeBeat = beat;
  completedActionIds = [];
  const enrichedActions = ensureVisualSketchIfConceptual(beat);
  pendingCueActions = enrichedActions.map((a) => ({ action: a, fired: false, triggerMs: 0 }));

  const ped = beat.pedagogy || {};
  elFollowups.replaceChildren();
  const chips = Array.isArray(ped.suggested_followups) && ped.suggested_followups.length
    ? ped.suggested_followups
    : ['Sí, entendí · Siguiente paso', '¿De dónde salió?', 'Espacio con dibujo'];
  for (const label of chips.slice(0, 4)) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'chip-btn';
    b.textContent = label;
    b.addEventListener('click', () => handleFollowupChip(label));
    elFollowups.appendChild(b);
  }

  // ONLY show math expression input when step explicitly expects an algebraic expression
  if (ped.expected_answer_expr) {
    elPracticeBar.hidden = false;
    elPracticePrompt.textContent = ped.check_question || 'Prueba escribir tu resultado:';
    elPracticeFeedback.hidden = true;
    elPracticeInput.value = '';
    expectedPracticeExpr = ped.expected_answer_expr;
    expectedPracticeConcept = ped.concept_tag || 'calc.int.general';
  } else {
    elPracticeBar.hidden = true;
    expectedPracticeExpr = null;
  }

  if (meta.model) {
    setStatus('El profesor está dibujando y explicando…', 'connected');
  }
}

function startBeatPlaybackWithAlignment(alignMsg) {
  const cuesMap = alignMsg.cues_ms || {};
  let minTrigger = Infinity;
  for (const item of pendingCueActions) {
    const cid = item.action.animation && item.action.animation.cue_id;
    const off = (item.action.animation && item.action.animation.offset_ms) || 0;
    item.triggerMs = cid && cid in cuesMap ? Math.max(0, cuesMap[cid] + off) : 0;
    if (item.triggerMs < minTrigger) minTrigger = item.triggerMs;
  }
  // Never leave the whiteboard blank waiting seconds for the first cue: shift earliest cue to <= 250ms
  if (Number.isFinite(minTrigger) && minTrigger > 250) {
    const shift = minTrigger - 250;
    for (const item of pendingCueActions) {
      item.triggerMs = Math.max(0, item.triggerMs - shift);
    }
  }
  // Assign realistic speech-span durations so Beat/TTS writing matches the exact seconds spent narrating each clause
  const totalSpeechMs = Math.max(1800, Number(alignMsg.duration_ms || 3200));
  const sortedUniqueTriggers = Array.from(new Set(pendingCueActions.map((x) => x.triggerMs))).sort((a, b) => a - b);
  for (const item of pendingCueActions) {
    const nextTrig = sortedUniqueTriggers.find((t) => t > item.triggerMs + 150);
    const spanMs = (nextTrig !== undefined ? nextTrig : totalSpeechMs) - item.triggerMs;
    if (spanMs >= 1100) {
      item.action._syncDurationMs = Math.min(8200, Math.round(spanMs * 0.88));
    }
  }

  const hasSpeech = Boolean((alignMsg.plain_text || '').trim());
  elCaptionStrip.hidden = !hasSpeech;
  if (hasSpeech) {
    elCaptionSpeaker.textContent = alignMsg.engine === 'silent_visual_only' ? 'Guía' : 'Profesor';
    elCaptionText.replaceChildren();
    activeWordSpans = [];
    activeAlignmentWords = alignMsg.words || [];
    currentWordIdx = 0;

    if (activeAlignmentWords.length) {
      activeAlignmentWords.forEach((wObj, idx) => {
        const sp = document.createElement('span');
        sp.className = 'caption-word';
        sp.textContent = (idx > 0 ? ' ' : '') + wObj.w;
        elCaptionText.appendChild(sp);
        activeWordSpans.push(sp);
      });
    } else {
      elCaptionText.textContent = alignMsg.plain_text || '';
    }
  }

  driveSyncedTimeline(0);
  const audioB64 = alignMsg.audio_b64 || alignMsg.audio_mp3_b64 || '';
  const audioMime = alignMsg.audio_b64 ? (alignMsg.audio_mime || 'audio/wav') : 'audio/mpeg';
  audioEngine.playAlignedAudioOrClock(
    audioB64,
    audioMime,
    alignMsg.duration_ms || 3200,
    alignMsg.speech_rate || 1.0,
  );
}

function driveSyncedTimeline(clockMs) {
  for (const item of pendingCueActions) {
    if (!item.fired && clockMs >= item.triggerMs) {
      item.fired = true;
      executeCanvasAction(item.action, activeBeat);
      completedActionIds.push(item.action.action_id);
    }
  }
  for (let i = 0; i < activeAlignmentWords.length; i++) {
    const w = activeAlignmentWords[i];
    if (clockMs >= w.start_ms && clockMs <= w.end_ms + 90) {
      if (activeWordSpans[currentWordIdx]) activeWordSpans[currentWordIdx].classList.remove('active-word');
      currentWordIdx = i;
      if (activeWordSpans[i]) activeWordSpans[i].classList.add('active-word');
      break;
    }
  }
}

function flushRemainingCueActions() {
  for (const item of pendingCueActions) {
    if (!item.fired) {
      item.fired = true;
      executeCanvasAction(item.action, activeBeat);
      completedActionIds.push(item.action.action_id);
    }
  }
}

async function executeCanvasAction(action, beat, syncOpts = {}) {
  const payload = action.payload || {};
  const anim = action.animation || {};
  const placement = action.placement || { zone: 'derivation_flow', relation: 'auto_next' };
  const colorCls = action.color_token ? `ink-${action.color_token}` : '';
  const liveSynced = Boolean(syncOpts.liveSynced && liveVoiceActive);
  const mySyncGen = syncOpts.syncGen ?? liveSyncGeneration;
  const speedProvider = liveSynced
    ? () => (liveTurnCompleted ? 2.1 : audioEngine.getLiveSpeechSpeedFactor())
    : null;
  const abortCheck = liveSynced ? () => mySyncGen !== liveSyncGeneration : null;

  if (action.type === 'draw_sketch') {
    markerEngine.onNewStepCommitted();
    const wrap = document.createElement('article');
    wrap.className = 'step-block';
    wrap.dataset.objectId = action.object_id || '';
    const sk = createHandDrawnSketchBlock(payload, {
      reduceMotion,
      durationMs: action._syncDurationMs || anim.duration_ms || 3400,
      getSpeedFactor: speedProvider,
      isAborted: abortCheck,
    });
    wrap.appendChild(sk);
    insertBlockInZone(wrap, placement);
    panElementIntoView(wrap);
    if (liveSynced && sk._finishedPromise) {
      await sk._finishedPromise;
    }
  } else if (action.type === 'write_math') {
    markerEngine.onNewStepCommitted();
    const block = document.createElement('article');
    block.className = 'step-block';
    block.dataset.objectId = action.object_id || '';
    block.dataset.stepIndex = String(beat ? beat.step_index : 0);

    const head = document.createElement('div');
    head.className = 'step-head';
    const lbl = document.createElement('span');
    lbl.className = 'step-label';
    lbl.textContent = payload.step_label || `Paso ${beat ? beat.step_index : ''}`;

    const mini = document.createElement('div');
    mini.className = 'step-actions-mini';
    const btnOrig = document.createElement('button');
    btnOrig.type = 'button';
    btnOrig.className = 'mini-link-btn';
    btnOrig.textContent = '¿De dónde salió?';
    btnOrig.addEventListener('click', () => triggerOriginQuery(action.object_id, ''));
    const btnMid = document.createElement('button');
    btnMid.type = 'button';
    btnMid.className = 'mini-link-btn';
    btnMid.textContent = 'Espacio con pasos/dibujo';
    btnMid.addEventListener('click', () => triggerIntermediateSteps(action.object_id));
    mini.append(btnOrig, btnMid);
    head.append(lbl, mini);

    const mathHost = document.createElement('div');
    mathHost.className = `math-render-host ${colorCls}`;
    block.append(head, mathHost);

    let noteEl = null;
    if (payload.note_es) {
      noteEl = document.createElement('p');
      noteEl.className = 'wb-hand-note';
      block.appendChild(noteEl);
    }

    const ver = beat && beat.verification;
    if (ver && (ver.status === 'sympy_verified' || ver.dimensional_check)) {
      const foot = document.createElement('div');
      foot.className = 'verify-foot';
      if (ver.status === 'sympy_verified') {
        const sp = document.createElement('span');
        sp.textContent = 'Verificado simbólicamente';
        foot.appendChild(sp);
      }
      if (ver.dimensional_check) {
        const sp = document.createElement('span');
        sp.textContent = ver.dimensional_check;
        foot.appendChild(sp);
      }
      if (ver.source_citation && ver.source_citation.doc_name) {
        const sp = document.createElement('span');
        sp.textContent = `Fuente: ${ver.source_citation.doc_name} (pág. ${ver.source_citation.page || 1})`;
        foot.appendChild(sp);
      }
      block.appendChild(foot);
    }

    insertBlockInZone(block, placement);
    if (action.object_id) lastMathObjectId = action.object_id;
    panElementIntoView(block);
    await renderMath2D(mathHost, payload.latex || '', {
      animate: true,
      durationMs: action._syncDurationMs || anim.duration_ms || 0,
      reduceMotion,
      getSpeedFactor: speedProvider,
      isAborted: abortCheck,
    });
    if (abortCheck && abortCheck()) return;
    if (noteEl && payload.note_es) {
      await animateHandwrittenText(noteEl, payload.note_es, 0, {
        getSpeedFactor: speedProvider,
        isAborted: abortCheck,
      });
    }

    mathHost.addEventListener('click', (ev) => {
      if (activeTool !== 'pointer') return;
      const termEl = ev.target.closest('[class*="term-"]');
      const termCls = termEl ? (Array.from(termEl.classList).find((c) => c.startsWith('term-')) || '') : '';
      const tapped = extractSymbolAtClientPoint(elStage, ev.clientX, ev.clientY, cropWordsByObjectId);
      const symText = tapped.text || (termCls ? termCls.replace('term-', '') : '');
      const tapBox = {
        left: ev.clientX - 24,
        top: ev.clientY - 22,
        right: ev.clientX + 24,
        bottom: ev.clientY + 22,
      };
      const tapSel = {
        text: symText,
        objectId: action.object_id || tapped.objectId || '',
        stepTitle: tapped.stepTitle || payload.step_label || '',
        lineContext: tapped.lineContext || '',
        pointerClientPt: { x: ev.clientX, y: ev.clientY },
      };
      const fullB64 = captureBoardSnapshot('image/jpeg', { highlightSelection: tapSel });
      const cropB64 = captureZoomedLassoCrop(tapBox, null, { x: ev.clientX, y: ev.clientY });
      sendWs({
        type: 'USER_LASSO_SELECTION',
        gesture: 'pointer_tap',
        image_b64: fullB64 || '',
        crop_b64: cropB64 || '',
        mime_type: 'image/jpeg',
        circled_text: symText,
        object_id: tapSel.objectId,
        step_title: tapSel.stepTitle,
        line_context: tapSel.lineContext,
      });
      if (symText) {
        markerEngine.point({ target_object_id: tapSel.objectId, match_text: symText, color: 'red', ttl_ms: 2000 }).catch(() => {});
      }
      showCirclePopover(ev.clientX, ev.clientY, {
        title: symText ? `Señalaste: "${symText.slice(0, 42)}"` : (payload.step_label || 'Paso seleccionado'),
        circled_text: symText,
        hit_targets: [{ kind: 'tutor_math_term', object_id: action.object_id, term_class: termCls, step_index: beat ? beat.step_index : 0 }],
        object_id: tapSel.objectId,
        term_class: termCls,
        visual_crop_png_b64: cropB64,
      });
    });

    panElementIntoView(block);
  } else if (action.type === 'expand_intermediate_steps') {
    const wrap = document.createElement('article');
    wrap.className = 'step-block intermediate-expansion';
    wrap.dataset.objectId = action.object_id || '';

    const head = document.createElement('div');
    head.className = 'step-head';
    const lbl = document.createElement('span');
    lbl.className = 'step-label';
    lbl.textContent = payload.title || 'Espacio de aclaración · paso a paso';
    head.appendChild(lbl);
    wrap.appendChild(head);

    for (const sub of payload.substeps || []) {
      const m = document.createElement('div');
      m.className = 'math-render-host';
      wrap.appendChild(m);
      await renderMath2D(m, sub.latex || '', { animate: true, durationMs: 850, reduceMotion });
      if (sub.note) {
        const p = document.createElement('p');
        p.className = 'wb-hand-note';
        wrap.appendChild(p);
        animateHandwrittenText(p, sub.note, 750);
      }
    }

    if (payload.visual_graph && typeof payload.visual_graph === 'object') {
      wrap.appendChild(createHandDrawnSketchBlock(payload.visual_graph, { reduceMotion, durationMs: 1800 }));
    }

    const anchorId = payload.after_object_id || lastMathObjectId;
    const anchorEl = anchorId
      ? elDerivation.querySelector(`[data-object-id="${CSS.escape(anchorId)}"]`)
      : null;
    if (anchorEl && anchorEl.parentElement) {
      anchorEl.insertAdjacentElement('afterend', wrap);
    } else {
      elDerivation.appendChild(wrap);
    }
    panElementIntoView(wrap);
  } else if (action.type === 'draw_inline_graph') {
    const wrap = document.createElement('article');
    wrap.className = 'step-block intermediate-expansion';
    wrap.dataset.objectId = action.object_id || '';
    wrap.appendChild(createHandDrawnSketchBlock(payload, { reduceMotion, durationMs: 1800 }));
    const anchorId = payload.after_object_id || (placement && placement.target_id) || lastMathObjectId;
    const anchorEl = anchorId
      ? elDerivation.querySelector(`[data-object-id="${CSS.escape(anchorId)}"]`)
      : null;
    if (anchorEl && anchorEl.parentElement) {
      anchorEl.insertAdjacentElement('afterend', wrap);
    } else {
      elDerivation.appendChild(wrap);
    }
    panElementIntoView(wrap);
  } else if (action.type === 'paste_problem_clip') {
    const block = document.createElement('article');
    block.className = 'step-block problem-clip-block';
    block.dataset.objectId = action.object_id || '';

    const head = document.createElement('div');
    head.className = 'step-head';
    const lbl = document.createElement('span');
    lbl.className = 'step-label';
    lbl.textContent = `${payload.problem_id || 'Problema'} · Pág. ${payload.page || 1}${payload.subtema ? ` · ${payload.subtema}` : ''}`;
    head.appendChild(lbl);
    block.appendChild(head);

    if (payload.problem_png_b64) {
      const img = document.createElement('img');
      img.className = 'problem-clip-img';
      img.src = `data:image/png;base64,${payload.problem_png_b64}`;
      img.alt = `Recorte de ${payload.problem_id || 'problema del PDF'}`;
      if (payload.doc_id) img.dataset.pdfDocId = payload.doc_id;
      if (payload.page) img.dataset.pdfPage = String(payload.page);
      block.appendChild(img);
    }

    const metaStrip = document.createElement('div');
    metaStrip.className = 'problem-meta-strip';
    if (payload.procedimiento) {
      const sp = document.createElement('span');
      sp.textContent = payload.procedimiento;
      metaStrip.appendChild(sp);
    }
    if (payload.has_figure) {
      const spf = document.createElement('span');
      spf.textContent = 'Incluye figura (puedes rodearla con Encerrar)';
      metaStrip.appendChild(spf);
    }
    if (metaStrip.childElementCount) block.appendChild(metaStrip);

    if (Array.isArray(payload.alternatives) && payload.alternatives.length) {
      const altWrap = document.createElement('div');
      altWrap.className = 'audit-chips';
      const altLbl = document.createElement('span');
      altLbl.className = 'widget-title';
      altLbl.textContent = 'Otros similares:';
      altWrap.appendChild(altLbl);
      for (const alt of payload.alternatives) {
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'audit-id-chip';
        b.textContent = `${alt.problem_id} (p.${alt.page})`;
        b.addEventListener('click', () => {
          audioEngine.hardStopPlayback();
          sendWs({ type: 'SOLVE_PDF_PROBLEM', doc_id: payload.doc_id || loadedPdfDocId, problem_id: alt.problem_id });
        });
        altWrap.appendChild(b);
      }
      block.appendChild(altWrap);
    }

    if (action.object_id && Array.isArray(payload.crop_words) && payload.crop_words.length) {
      cropWordsByObjectId.set(action.object_id, payload.crop_words);
    }

    elDerivation.appendChild(block);
    panElementIntoView(block, { alignTop: true });
  } else if (action.type === 'render_pdf_audit') {
    const block = document.createElement('article');
    block.className = 'step-block audit-block';
    block.dataset.objectId = action.object_id || '';

    const head = document.createElement('div');
    head.className = 'step-head';
    const lbl = document.createElement('span');
    lbl.className = 'step-label';
    lbl.textContent = `Auditoría · ${payload.filename || 'PDF'} (${payload.total_problems || 0} problemas, ${payload.problems_with_figures || 0} con figura)`;
    head.appendChild(lbl);
    block.appendChild(head);

    const archs = Array.isArray(payload.top_archetypes) ? payload.top_archetypes : [];
    if (archs.length) {
      const t1 = document.createElement('div');
      t1.className = 'audit-section-title';
      t1.textContent = 'Temas que más se repiten';
      block.appendChild(t1);
      const maxCnt = Math.max(1, archs[0].count || 1);
      for (const a of archs) {
        const row = document.createElement('div');
        row.className = 'audit-row';
        const top = document.createElement('div');
        top.className = 'audit-row-top';
        const name = document.createElement('strong');
        name.textContent = a.subtema || a.procedimiento || 'Tema';
        const stat = document.createElement('span');
        stat.textContent = `${a.count} veces (${a.pct}%) · ${a.with_figures} con figura`;
        top.append(name, stat);

        const track = document.createElement('div');
        track.className = 'audit-bar-track';
        const fill = document.createElement('div');
        fill.className = 'audit-bar-fill';
        fill.style.width = `${Math.round((a.count / maxCnt) * 100)}%`;
        track.appendChild(fill);

        const chips = document.createElement('div');
        chips.className = 'audit-chips';
        for (const pid of (a.sample_ids || []).slice(0, 6)) {
          const btn = document.createElement('button');
          btn.type = 'button';
          btn.className = 'audit-id-chip';
          btn.textContent = pid;
          btn.addEventListener('click', () => {
            audioEngine.hardStopPlayback();
            sendWs({ type: 'SOLVE_PDF_PROBLEM', doc_id: payload.doc_id || loadedPdfDocId, problem_id: pid });
          });
          chips.appendChild(btn);
        }
        row.append(top, track, chips);
        block.appendChild(row);
      }
    }

    const dups = Array.isArray(payload.near_duplicate_clusters) ? payload.near_duplicate_clusters : [];
    if (dups.length) {
      const t2 = document.createElement('div');
      t2.className = 'audit-section-title';
      t2.textContent = 'Problemas casi repetidos (mismo método / distintos números)';
      block.appendChild(t2);
      for (const c of dups) {
        const row = document.createElement('div');
        row.className = 'audit-row';
        const top = document.createElement('div');
        top.className = 'audit-row-top';
        const sub = document.createElement('strong');
        sub.textContent = `${c.subtema || 'Mismo patrón'} · ${c.similarity_pct}% similitud`;
        const cnt = document.createElement('span');
        cnt.textContent = `${c.cluster_size} variantes`;
        top.append(sub, cnt);

        const exc = document.createElement('p');
        exc.className = 'wb-hand-note';
        exc.textContent = c.sample_excerpt || '';

        const chips = document.createElement('div');
        chips.className = 'audit-chips';
        for (const pid of c.problem_ids || []) {
          const btn = document.createElement('button');
          btn.type = 'button';
          btn.className = 'audit-id-chip';
          btn.textContent = `Resolver ${pid}`;
          btn.addEventListener('click', () => {
            audioEngine.hardStopPlayback();
            sendWs({ type: 'SOLVE_PDF_PROBLEM', doc_id: payload.doc_id || loadedPdfDocId, problem_id: pid });
          });
          chips.appendChild(btn);
        }
        row.append(top, exc, chips);
        block.appendChild(row);
      }
    }

    elDerivation.appendChild(block);
    panElementIntoView(block, { alignTop: true });
  } else if (action.type === 'trace_provenance') {
    await markerEngine.traceDataProvenance(payload);
  } else if (action.type === 'trace_origin') {
    const handled = await markerEngine.traceOrigin({
      ...payload,
      color: payload.color || action.color_token || 'red',
    });
    if (!handled) {
      drawOriginArrow(payload.from_ref || '', payload.to_ref || '', payload.rule_es || '');
    }
  } else if (action.type === 'cancel_terms') {
    await markerEngine.cancelTerms(payload);
  } else if (action.type === 'underline_term') {
    await markerEngine.underline({ ...payload, color: payload.color || action.color_token || 'red' });
  } else if (action.type === 'circle_term') {
    await markerEngine.circle({ ...payload, color: payload.color || action.color_token || 'orange' });
  } else if (action.type === 'highlight_term') {
    await markerEngine.highlight({ ...payload, color: payload.color || action.color_token || 'yellow' });
  } else if (action.type === 'colorize_term') {
    await markerEngine.colorize({ ...payload, color: payload.color || action.color_token || 'red' });
  } else if (action.type === 'point_at_term') {
    await markerEngine.point({ ...payload, color: payload.color || action.color_token || 'red' });
  } else if (action.type === 'write_margin_note') {
    await markerEngine.marginNote({ ...payload, color: payload.color || action.color_token || 'teal' });
  } else if (action.type === 'write_structured_notes') {
    markerEngine.onNewStepCommitted();
    const block = document.createElement('article');
    block.className = 'step-block wb-notes-block';
    block.dataset.objectId = action.object_id || '';
    block.dataset.stepIndex = String(beat ? beat.step_index : 0);

    const head = document.createElement('div');
    head.className = 'step-head';
    const lbl = document.createElement('span');
    lbl.className = 'step-label';
    head.appendChild(lbl);
    block.appendChild(head);

    insertBlockInZone(block, placement);
    panElementIntoView(block);

    const titleTxt = payload.title || 'Apuntes en pizarra';
    // Crisp fast header so bullet #1 starts immediately right as the professor opens her mouth
    await animateHandwrittenText(lbl, titleTxt, Math.max(180, Math.min(360, titleTxt.length * 12)), {
      getSpeedFactor: speedProvider,
      isAborted: abortCheck,
    });

    const items = Array.isArray(payload.items) ? payload.items : [];
    const headers = Array.isArray(payload.table_headers) ? payload.table_headers : [];
    const rows = Array.isArray(payload.table_rows) ? payload.table_rows : [];
    const totalEntries = Math.max(1, items.length + rows.length);
    const spanBudget = action._syncDurationMs || anim.duration_ms || 0;
    const perItemPace = spanBudget > 0
      ? Math.max(700, Math.min(1800, Math.round((spanBudget * 0.65) / totalEntries)))
      : 1100;

    if (items.length) {
      const ul = document.createElement('ul');
      ul.className = 'wb-hand-list';
      block.appendChild(ul);
      for (let idx = 0; idx < items.length; idx++) {
        if (abortCheck && abortCheck()) break;
        const itTxt = String(items[idx]);
        const nextKws = idx + 1 < items.length ? extractSyncKeywords(String(items[idx + 1])) : [];
        if (liveSynced && idx > 0) {
          const kws = extractSyncKeywords(itTxt);
          await waitForLiveTranscriptOrPace(kws, Math.min(1850, perItemPace + 450), mySyncGen);
          if (abortCheck && abortCheck()) break;
        } else if (!liveSynced && !reduceMotion && idx > 0) {
          await new Promise((r) => setTimeout(r, Math.min(420, Math.round(perItemPace * 0.25))));
        }
        const li = document.createElement('li');
        ul.appendChild(li);
        panElementIntoView(li);
        const writeDur = Math.max(480, Math.min(1650, itTxt.length * 32));
        // Adaptive PLL catch-up: if voice ALREADY moved to the next bullet, smoothly accelerate to catch up!
        const itemSpeedProvider = liveSynced
          ? () => {
              if (liveTurnCompleted) return 2.2;
              if (nextKws.length && nextKws.some((k) => liveTurnTranscriptFolded.includes(k))) return 2.4;
              return audioEngine.getLiveSpeechSpeedFactor();
            }
          : null;
        await animateHandwrittenText(li, itTxt, writeDur, {
          getSpeedFactor: itemSpeedProvider,
          isAborted: abortCheck,
        });
      }
    }

    if (headers.length || rows.length) {
      const tbl = document.createElement('table');
      tbl.className = 'wb-hand-table';
      if (headers.length) {
        const thead = document.createElement('thead');
        const tr = document.createElement('tr');
        for (const h of headers) {
          const th = document.createElement('th');
          th.textContent = String(h);
          tr.appendChild(th);
        }
        thead.appendChild(tr);
        tbl.appendChild(thead);
      }
      const tbody = rows.length ? document.createElement('tbody') : null;
      if (tbody) tbl.appendChild(tbody);
      block.appendChild(tbl);
      if (tbody) {
        for (let rIdx = 0; rIdx < rows.length; rIdx++) {
          if (abortCheck && abortCheck()) break;
          const row = rows[rIdx];
          const rowJoined = row.map((c) => String(c)).join(' ');
          if (liveSynced && rIdx > 0) {
            await waitForLiveTranscriptOrPace(extractSyncKeywords(rowJoined), Math.min(2600, perItemPace + 500), mySyncGen);
            if (abortCheck && abortCheck()) break;
          }
          const tr = document.createElement('tr');
          tbody.appendChild(tr);
          panElementIntoView(tr);
          for (const cell of row) {
            if (abortCheck && abortCheck()) break;
            const td = document.createElement('td');
            tr.appendChild(td);
            const cellTxt = String(cell);
            await animateHandwrittenText(td, cellTxt, Math.max(320, Math.min(1300, cellTxt.length * 36)), {
              getSpeedFactor: speedProvider,
              isAborted: abortCheck,
            });
          }
        }
      }
    }

    if (payload.note_es) {
      const p = document.createElement('p');
      p.className = 'wb-hand-note';
      block.appendChild(p);
      await animateHandwrittenText(p, payload.note_es, 0, {
        getSpeedFactor: speedProvider,
        isAborted: abortCheck,
      });
    }
    panElementIntoView(block);
  } else if (action.type === 'write_text' || action.type === 'annotate_step') {
    const box = document.createElement('article');
    box.className = 'step-block';
    box.dataset.objectId = action.object_id || '';
    const p = document.createElement('p');
    p.className = action.type === 'write_text' ? `wb-hand-line ${colorCls}` : `wb-hand-note ${colorCls}`;
    box.appendChild(p);
    insertBlockInZone(box, placement);
    animateHandwrittenText(p, payload.text || '', anim.duration_ms || 950);
    panElementIntoView(box);
  } else if (action.type === 'plot_riemann_sum') {
    const w = createRiemannWorkWidget(payload);
    w.dataset.objectId = action.object_id || '';
    elVisualLab.appendChild(w);
  } else if (action.type === 'plot_pv_diagram') {
    const wrap = document.createElement('article');
    wrap.className = 'step-block';
    wrap.dataset.objectId = action.object_id || '';
    wrap.appendChild(createHandDrawnSketchBlock({ ...payload, kind: 'pv_diagram' }, { reduceMotion, durationMs: 2000 }));
    elDerivation.appendChild(wrap);
  } else if (action.type === 'draw_thermal_shell') {
    const wrap = document.createElement('article');
    wrap.className = 'step-block';
    wrap.dataset.objectId = action.object_id || '';
    const k = payload.geometry === 'cylindrical' ? 'thermal_cylinder' : 'thermal_sphere';
    wrap.appendChild(createHandDrawnSketchBlock({ ...payload, kind: k }, { reduceMotion, durationMs: 2000 }));
    elDerivation.appendChild(wrap);
  } else if (action.type === 'sync_calculator_state') {
    const w = createCasioKeycapStrip(payload);
    w.dataset.objectId = action.object_id || '';
    elFormulaDock.appendChild(w);
  }
  syncTwoColumnLayout();
}

function insertBlockInZone(el, placement) {
  const zone = (placement && placement.zone) || 'derivation_flow';
  if (zone === 'clarification_sidecar') {
    elClarification.appendChild(el);
    return;
  }
  if (zone === 'visual_lab') {
    elVisualLab.appendChild(el);
    return;
  }
  if (zone === 'formula_dock') {
    elFormulaDock.appendChild(el);
    return;
  }
  if (placement && placement.relation === 'between' && placement.target_id) {
    const anchor = elDerivation.querySelector(`[data-object-id="${CSS.escape(placement.target_id)}"]`);
    if (anchor) {
      anchor.insertAdjacentElement('afterend', el);
      return;
    }
  }
  elDerivation.appendChild(el);
}

function resolveRefElement(refStr) {
  if (!refStr) return null;
  const [objId, termCls] = String(refStr).split(':');
  const block = elStage.querySelector(`[data-object-id="${CSS.escape(objId)}"]`);
  if (!block) return null;
  if (termCls) {
    const sub = block.querySelector(`.${CSS.escape(termCls)}`);
    if (sub) return sub;
  }
  return block;
}

function drawOriginArrow(fromRef, toRef, ruleText) {
  const fromEl = resolveRefElement(fromRef);
  const toEl = resolveRefElement(toRef);
  if (!fromEl || !toEl) return;

  fromEl.classList.add('term-origin-glow');
  toEl.classList.add('term-pulse');
  setTimeout(() => {
    fromEl.classList.remove('term-origin-glow');
    toEl.classList.remove('term-pulse');
  }, 5500);

  const stageRect = elStage.getBoundingClientRect();
  const r1 = fromEl.getBoundingClientRect();
  const r2 = toEl.getBoundingClientRect();
  const x1 = Math.min(stageRect.width - 80, r1.right - stageRect.left + 8);
  const y1 = r1.top + r1.height / 2 - stageRect.top;
  const x2 = Math.min(stageRect.width - 80, r2.right - stageRect.left + 8);
  const y2 = r2.top + r2.height / 2 - stageRect.top;
  const ctrlX = Math.min(stageRect.width - 20, Math.max(x1, x2) + 54);

  const ns = 'http://www.w3.org/2000/svg';
  const g = document.createElementNS(ns, 'g');
  const path = document.createElementNS(ns, 'path');
  path.setAttribute('d', `M ${x1} ${y1} C ${ctrlX} ${y1}, ${ctrlX} ${y2}, ${x2} ${y2}`);
  path.setAttribute('fill', 'none');
  path.setAttribute('stroke', 'var(--accent-red)');
  path.setAttribute('stroke-width', '2.5');
  path.setAttribute('stroke-linecap', 'round');

  const dot = document.createElementNS(ns, 'circle');
  dot.setAttribute('cx', String(x2));
  dot.setAttribute('cy', String(y2));
  dot.setAttribute('r', '5');
  dot.setAttribute('fill', 'var(--accent-red)');

  g.append(path, dot);
  if (ruleText) {
    const fo = document.createElementNS(ns, 'foreignObject');
    fo.setAttribute('x', String(Math.max(8, ctrlX - 180)));
    fo.setAttribute('y', String((y1 + y2) / 2 - 22));
    fo.setAttribute('width', '220');
    fo.setAttribute('height', '64');
    const div = document.createElement('div');
    div.style.cssText = 'font-family:"Patrick Hand","Ink Free",cursive;font-size:15px;padding:4px 9px;border-radius:8px;background:var(--card);border:1.5px solid var(--accent-red);color:var(--ink-primary);box-shadow:0 4px 12px rgba(0,0,0,0.1);';
    div.textContent = ruleText;
    fo.appendChild(div);
    g.appendChild(fo);
  }
  elProvenanceSvg.appendChild(g);
  setTimeout(() => g.remove(), 8000);
}

// ============================================================================
// Circle-to-Ask Lasso, Freehand Pen, Ctrl+V Snips, PDF Pages & Student Notes
// ============================================================================

function resizeInkCanvas() {
  // Use unscaled CSS layout dimensions so zooming NEVER clears or distorts ink strokes
  const w = Math.max(320, elStage.offsetWidth || elStage.scrollWidth);
  const h = Math.max(elStage.scrollHeight, elViewport.clientHeight - 40);
  if (elInkCanvas.width !== w || elInkCanvas.height !== h) {
    elInkCanvas.width = w;
    elInkCanvas.height = h;
    redrawInkCanvas();
  }
}

function redrawInkCanvas() {
  const ctx = elInkCanvas.getContext('2d');
  ctx.clearRect(0, 0, elInkCanvas.width, elInkCanvas.height);
  for (const s of penStrokes) drawStrokeOnCtx(ctx, s.points, s.color, s.width, s.dashed);
  if (activeLassoSelection && activeLassoSelection.points && activeLassoSelection.points.length >= 2) {
    const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
    drawStrokeOnCtx(ctx, activeLassoSelection.points, isDark ? '#f87171' : '#dc2626', 3.2, true);
  }
  if (drawingPath) drawStrokeOnCtx(ctx, drawingPath.points, drawingPath.color, drawingPath.width, drawingPath.dashed);
}

function drawStrokeOnCtx(ctx, pts, color, width, dashed) {
  if (!pts || pts.length < 2) return;
  ctx.save();
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  if (dashed) ctx.setLineDash([7, 4]);
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
  ctx.stroke();
  ctx.restore();
}

function setActiveTool(toolName) {
  activeTool = toolName;
  for (const [id, name] of [['tool-pointer', 'pointer'], ['tool-pan', 'pan'], ['tool-lasso', 'lasso'], ['tool-pen', 'pen']]) {
    const el = document.getElementById(id);
    if (el) el.setAttribute('aria-pressed', String(name === toolName));
  }
  const penStrip = document.getElementById('pen-color-strip');
  if (penStrip) penStrip.hidden = toolName !== 'pen';
  elViewport.classList.toggle('pan-ready', toolName === 'pan' || spacePanHeld);
  elInkCanvas.classList.toggle('interactive', (toolName === 'lasso' || toolName === 'pen') && !spacePanHeld);
  if (toolName !== 'lasso') elCirclePop.hidden = true;
  resizeInkCanvas();
}

/**
 * Paint all DOM step blocks (labels, handwritten 2D math, fraction bars, notes) intersecting
 * `viewClientRect` onto a canvas context mapped by `(clientX - viewClientRect.left) * scaleX`.
 * Works identically at ANY scroll/pan depth (y = 0 or y = 6000 px) and ANY zoom level!
 */
function _paintVisibleDomIntoClientWindow(ctx, viewClientRect, scaleX, scaleY, isDark) {
  const defaultInk = isDark ? '#f1f5f9' : '#172554';
  const labelInk = isDark ? '#60a5fa' : '#1d4ed8';
  const stageScale = getStageScale(elStage);
  const blocks = elStage.querySelectorAll('.step-block');

  for (const block of blocks) {
    const br = block.getBoundingClientRect();
    if (
      br.width < 2 ||
      br.height < 2 ||
      br.right < viewClientRect.left ||
      br.left > viewClientRect.right ||
      br.bottom < viewClientRect.top ||
      br.top > viewClientRect.bottom
    ) {
      continue;
    }

    // 1. Fraction divider lines (.m2d-num border-bottom)
    block.querySelectorAll('.m2d-num').forEach((numEl) => {
      const nr = numEl.getBoundingClientRect();
      if (nr.width < 3 || nr.height < 3) return;
      ctx.save();
      ctx.strokeStyle = defaultInk;
      ctx.lineWidth = Math.max(1.6, 2.2 * scaleY);
      ctx.beginPath();
      ctx.moveTo((nr.left - viewClientRect.left) * scaleX, (nr.bottom - viewClientRect.top) * scaleY);
      ctx.lineTo((nr.right - viewClientRect.left) * scaleX, (nr.bottom - viewClientRect.top) * scaleY);
      ctx.stroke();
      ctx.restore();
    });

    // 2. Crisp DOM text fitted into each token's exact bounding width (zero word collision!)
    const walker = document.createTreeWalker(block, NodeFilter.SHOW_TEXT, {
      acceptNode(n) {
        const p = n.parentElement;
        if (!p || !n.nodeValue || !n.nodeValue.trim()) return NodeFilter.FILTER_REJECT;
        if (p.closest('.step-actions-mini, .verify-foot, .problem-meta-strip, .audit-chips')) {
          return NodeFilter.FILTER_REJECT;
        }
        return NodeFilter.FILTER_ACCEPT;
      },
    });
    let node;
    while ((node = walker.nextNode())) {
      const parent = node.parentElement;
      const cs = getComputedStyle(parent);
      const fontSizePx = Math.max(10, Math.min(48, (parseFloat(cs.fontSize) || 18) * stageScale));
      const isLabel = Boolean(parent.closest('.step-label'));
      const textColor = isLabel ? labelInk : (cs.color && !cs.color.includes('light-dark') ? cs.color : defaultInk);
      const re = /\S+/g;
      let m;
      const txt = node.nodeValue || '';
      while ((m = re.exec(txt)) !== null) {
        try {
          const range = document.createRange();
          range.setStart(node, m.index);
          range.setEnd(node, m.index + m[0].length);
          const rr = range.getBoundingClientRect();
          if (
            rr.width < 1 ||
            rr.height < 1 ||
            rr.bottom < viewClientRect.top ||
            rr.top > viewClientRect.bottom ||
            rr.right < viewClientRect.left ||
            rr.left > viewClientRect.right
          ) {
            continue;
          }
          const x = (rr.left - viewClientRect.left) * scaleX;
          const y = (rr.top - viewClientRect.top) * scaleY;
          const maxW = Math.max(6, rr.width * scaleX);
          ctx.save();
          ctx.fillStyle = textColor;
          ctx.font = `${isLabel ? '700' : '600'} ${Math.max(10, Math.round(fontSizePx * scaleY * 0.88))}px "Segoe UI", Arial, sans-serif`;
          ctx.textBaseline = 'top';
          ctx.fillText(m[0], x, y, maxW);
          ctx.restore();
        } catch {}
      }
    }
  }
}

function _drawStageStrokeInClientWindow(ctx, stagePts, color, width, dashed, viewClientRect, scaleX, scaleY) {
  if (!stagePts || stagePts.length < 2) return;
  const sr = elStage.getBoundingClientRect();
  const stageScale = getStageScale(elStage);
  ctx.save();
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(2.4, width * stageScale * scaleX);
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  if (dashed) ctx.setLineDash([7 * scaleX, 4 * scaleX]);
  for (let i = 0; i < stagePts.length; i++) {
    const cx = sr.left + stagePts[i].x * stageScale;
    const cy = sr.top + stagePts[i].y * stageScale;
    const vx = (cx - viewClientRect.left) * scaleX;
    const vy = (cy - viewClientRect.top) * scaleY;
    if (i === 0) ctx.moveTo(vx, vy);
    else ctx.lineTo(vx, vy);
  }
  ctx.stroke();
  ctx.restore();
}

/**
 * Capture the EXACT CURRENT VISIBLE VIEWPORT (`#board-viewport`), 1:1 with what the student
 * sees on screen at ANY pan/scroll depth (never chopped at y=1800!) and ANY zoom level.
 */
function captureBoardSnapshot(mimeType = 'image/jpeg', { highlightSelection = null } = {}) {
  const hasInk = penStrokes.length > 0;
  const sel = highlightSelection || activeLassoSelection;
  const hasHighlightBanner = Boolean(sel && (sel.text || sel.stepTitle || (sel.points && sel.points.length >= 2) || sel.pointerClientPt));
  const canvases = Array.from(elStage.querySelectorAll('.wb-sketch-canvas'));
  const imgs = Array.from(elStage.querySelectorAll('.problem-clip-img, #media-layer-zone img'));
  const stepBlocks = elStage.querySelectorAll('.step-block');
  if (!hasInk && !hasHighlightBanner && !canvases.length && !imgs.length && !stepBlocks.length) return null;

  const vr = elViewport.getBoundingClientRect();
  const viewW = Math.max(320, vr.width || 1200);
  const viewH = Math.max(240, vr.height || 640);
  const bannerH = hasHighlightBanner ? 44 : 0;
  const targetW = 1152;
  const boardTargetH = Math.max(360, Math.min(820, Math.round((viewH / viewW) * targetW)));
  const targetH = boardTargetH + bannerH;
  const sx = targetW / viewW;
  const sy = boardTargetH / viewH;

  const off = document.createElement('canvas');
  off.width = targetW;
  off.height = targetH;
  const ctx = off.getContext('2d');
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  ctx.fillStyle = isDark ? '#13171f' : '#ffffff';
  ctx.fillRect(0, 0, targetW, targetH);

  ctx.save();
  ctx.beginPath();
  ctx.rect(0, bannerH, targetW, boardTargetH);
  ctx.clip();
  ctx.translate(0, bannerH);

  // 1. Composite visible PDF crops, pasted images, and professor sketches (exact viewport coords!)
  for (const el of [...imgs, ...canvases]) {
    try {
      const r = el.tagName === 'IMG' ? getContainedImageClientRect(el) : el.getBoundingClientRect();
      if (r.width < 10 || r.height < 10) continue;
      if (r.bottom < vr.top || r.top > vr.bottom || r.right < vr.left || r.left > vr.right) continue;
      ctx.drawImage(
        el,
        (r.left - vr.left) * sx,
        (r.top - vr.top) * sy,
        r.width * sx,
        r.height * sy,
      );
    } catch {}
  }

  // 2. Composite ALL visible DOM handwritten equations, step labels, and notes!
  _paintVisibleDomIntoClientWindow(ctx, vr, sx, sy, isDark);

  // 3. Composite all student freehand pen strokes in exact viewport coordinates
  for (const s of penStrokes) {
    _drawStageStrokeInClientWindow(ctx, s.points, s.color, Math.max(3.2, s.width * 1.3), s.dashed, vr, sx, sy);
  }

  // 4. Composite bright-red lasso ring or pointer tap target ring
  if (sel && sel.points && sel.points.length >= 2) {
    _drawStageStrokeInClientWindow(ctx, sel.points, '#dc2626', 4.2, false, vr, sx, sy);
  }
  if (sel && sel.pointerClientPt) {
    const px = (sel.pointerClientPt.x - vr.left) * sx;
    const py = (sel.pointerClientPt.y - vr.top) * sy;
    ctx.save();
    ctx.strokeStyle = '#dc2626';
    ctx.lineWidth = 4;
    ctx.beginPath();
    ctx.arc(px, py, 24, 0, Math.PI * 2);
    ctx.stroke();
    ctx.fillStyle = '#dc2626';
    ctx.beginPath();
    ctx.arc(px, py, 5, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }
  ctx.restore();

  // 5. High-contrast red top banner naming the exact pointed/circled symbol & step
  if (hasHighlightBanner) {
    ctx.fillStyle = '#dc2626';
    ctx.fillRect(0, 0, targetW, bannerH);
    ctx.fillStyle = '#ffffff';
    ctx.font = '700 18px "Segoe UI", Arial, sans-serif';
    ctx.textBaseline = 'middle';
    const whereStr = sel.stepTitle ? ` en "${sel.stepTitle}"` : (sel.objectId ? ` (${sel.objectId})` : '');
    const labelTxt = sel.text
      ? `SEÑALADO/ENCERRADO EN ROJO POR EL ESTUDIANTE: "${sel.text}"${whereStr}`
      : `ZONA SEÑALADA EN ROJO POR EL ESTUDIANTE${whereStr}`;
    ctx.fillText(labelTxt.slice(0, 118), 14, bannerH * 0.5);
  }

  const dataUrl = off.toDataURL(mimeType, 0.86);
  return dataUrl.split(',')[1] || null;
}

/**
 * Capture a crisp zoomed-in PNG close-up centered on `clientBox` at ANY pan depth & zoom level.
 */
function captureZoomedLassoCrop(clientBox, lassoPoints = null, pointerClientPt = null) {
  const padX = Math.max(54, (clientBox.right - clientBox.left) * 0.45);
  const padY = Math.max(42, (clientBox.bottom - clientBox.top) * 0.55);
  const cropRect = {
    left: clientBox.left - padX,
    top: clientBox.top - padY,
    right: clientBox.right + padX,
    bottom: clientBox.bottom + padY,
    width: (clientBox.right - clientBox.left) + padX * 2,
    height: (clientBox.bottom - clientBox.top) + padY * 2,
  };

  const scale = Math.max(1.2, Math.min(2.8, 680 / Math.max(140, cropRect.width)));
  const outW = Math.max(220, Math.min(920, Math.round(cropRect.width * scale)));
  const outH = Math.max(140, Math.min(640, Math.round(cropRect.height * scale)));
  const sx = outW / cropRect.width;
  const sy = outH / cropRect.height;

  const off = document.createElement('canvas');
  off.width = outW;
  off.height = outH;
  const ctx = off.getContext('2d');
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  ctx.fillStyle = isDark ? '#13171f' : '#ffffff';
  ctx.fillRect(0, 0, outW, outH);

  const imgs = Array.from(elStage.querySelectorAll('.problem-clip-img, #media-layer-zone img, .wb-sketch-canvas'));
  for (const el of imgs) {
    try {
      const r = el.tagName === 'IMG' ? getContainedImageClientRect(el) : el.getBoundingClientRect();
      if (r.width < 10 || r.height < 10) continue;
      if (r.bottom < cropRect.top || r.top > cropRect.bottom || r.right < cropRect.left || r.left > cropRect.right) continue;
      ctx.drawImage(
        el,
        (r.left - cropRect.left) * sx,
        (r.top - cropRect.top) * sy,
        r.width * sx,
        r.height * sy,
      );
    } catch {}
  }

  _paintVisibleDomIntoClientWindow(ctx, cropRect, sx, sy, isDark);

  for (const s of penStrokes) {
    _drawStageStrokeInClientWindow(ctx, s.points, s.color, Math.max(3, s.width), s.dashed, cropRect, sx, sy);
  }
  if (lassoPoints && lassoPoints.length >= 2) {
    _drawStageStrokeInClientWindow(ctx, lassoPoints, '#dc2626', 3.6, false, cropRect, sx, sy);
  }
  if (pointerClientPt) {
    const px = (pointerClientPt.x - cropRect.left) * sx;
    const py = (pointerClientPt.y - cropRect.top) * sy;
    ctx.save();
    ctx.strokeStyle = '#dc2626';
    ctx.lineWidth = 3.5;
    ctx.beginPath();
    ctx.arc(px, py, 22, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
  }

  return off.toDataURL('image/jpeg', 0.90).split(',')[1] || null;
}

function _strokePointsToClientBox(pts) {
  const canvasRect = elInkCanvas.getBoundingClientRect();
  const scale = getStageScale(elStage);
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (const p of pts) {
    if (p.x < minX) minX = p.x;
    if (p.y < minY) minY = p.y;
    if (p.x > maxX) maxX = p.x;
    if (p.y > maxY) maxY = p.y;
  }
  return {
    left: canvasRect.left + minX * scale,
    top: canvasRect.top + minY * scale,
    right: canvasRect.left + maxX * scale,
    bottom: canvasRect.top + maxY * scale,
  };
}

/**
 * Analyze recent freehand pen marks (e.g., circling `γ` + drawing an arrow pointing at it in red!)
 * and push BOTH the exact current viewport snapshot AND a high-res zoomed close-up + extracted
 * symbol/step context directly to Gemini Live so she ALWAYS sees what the student marked!
 */
function analyzeAndSendRecentPenMarkToLive() {
  if (!penStrokes.length) return;
  const latest = penStrokes[penStrokes.length - 1];
  const now = Date.now();
  const latestBox = _strokePointsToClientBox(latest.points);

  // Cluster strokes drawn within the last 5.5 s and within 220 px of the latest stroke
  const cluster = penStrokes.filter((s) => {
    if (!s.points || s.points.length < 2) return false;
    if (s.createdAt && now - s.createdAt > 5500) return false;
    const sb = _strokePointsToClientBox(s.points);
    const dx = Math.max(0, Math.max(sb.left - latestBox.right, latestBox.left - sb.right));
    const dy = Math.max(0, Math.max(sb.top - latestBox.bottom, latestBox.top - sb.bottom));
    return Math.hypot(dx, dy) <= 220;
  });
  const activeCluster = cluster.length ? cluster : [latest];

  // Union bounding box of the whole cluster (for zoomed close-up framing)
  let uL = Infinity, uT = Infinity, uR = -Infinity, uB = -Infinity;
  for (const s of activeCluster) {
    const sb = _strokePointsToClientBox(s.points);
    uL = Math.min(uL, sb.left);
    uT = Math.min(uT, sb.top);
    uR = Math.max(uR, sb.right);
    uB = Math.max(uB, sb.bottom);
  }
  const unionBox = { left: uL, top: uT, right: uR, bottom: uB };

  // Find the tightest symbol circled or pointed at by any stroke in the cluster!
  // Prefer compact strokes that directly enclose a symbol (e.g. circling 'γ') over long arrow shafts.
  let bestHit = null;
  let bestScore = Infinity;
  const sortedByArea = activeCluster
    .map((s) => {
      const box = _strokePointsToClientBox(s.points);
      const area = Math.max(1, (box.right - box.left) * (box.bottom - box.top));
      return { s, box, area };
    })
    .sort((a, b) => a.area - b.area);

  for (const item of sortedByArea) {
    const res = extractTextInsideClientRect(elStage, item.box, cropWordsByObjectId);
    if (res && res.text && res.sourceKind !== 'sketch_diagram') {
      const score = item.area + res.text.length * 120;
      if (score < bestScore) {
        bestScore = score;
        bestHit = { res, focusBox: item.box };
      }
    }
  }
  // If student drew an arrow pointing at a symbol without circling it, probe both stroke endpoints!
  if (!bestHit) {
    const canvasRect = elInkCanvas.getBoundingClientRect();
    const scale = getStageScale(elStage);
    for (const s of activeCluster) {
      const pFirst = s.points[0];
      const pLast = s.points[s.points.length - 1];
      for (const ep of [pFirst, pLast]) {
        const cx = canvasRect.left + ep.x * scale;
        const cy = canvasRect.top + ep.y * scale;
        const res = extractSymbolAtClientPoint(elStage, cx, cy, cropWordsByObjectId);
        if (res && res.text) {
          bestHit = { res, focusBox: unionBox };
          break;
        }
      }
      if (bestHit) break;
    }
  }
  if (!bestHit) {
    bestHit = {
      res: extractTextInsideClientRect(elStage, unionBox, cropWordsByObjectId),
      focusBox: unionBox,
    };
  }

  const hit = bestHit.res || { text: '', objectId: '', stepTitle: '', lineContext: '', pdfNormBox: null };
  const highlightSel = {
    text: hit.text || '',
    objectId: hit.objectId || '',
    stepTitle: hit.stepTitle || '',
    lineContext: hit.lineContext || '',
  };

  const fullB64 = captureBoardSnapshot('image/jpeg', { highlightSelection: highlightSel });
  const cropB64 = captureZoomedLassoCrop(unionBox);

  if (hit.text || hit.objectId) {
    sendWs({
      type: 'USER_LASSO_SELECTION',
      gesture: 'pen_mark',
      image_b64: fullB64 || '',
      crop_b64: cropB64 || '',
      mime_type: 'image/jpeg',
      circled_text: hit.text || '',
      object_id: hit.objectId || '',
      step_title: hit.stepTitle || '',
      line_context: hit.lineContext || '',
      pdf_norm_box: hit.pdfNormBox || null,
    });
  } else if (liveVoiceActive && fullB64) {
    // Student wrote freehand math (e.g. '1 + 2 =') on blank dot-grid paper -> send full viewport snapshot
    sendWs({ type: 'SEND_CANVAS_SNAPSHOT', image_b64: fullB64, mime_type: 'image/jpeg' });
  }
}

function sendBoardSnapshotToLive() {
  if (!liveVoiceActive) return;
  const b64 = captureBoardSnapshot('image/jpeg');
  if (b64) {
    sendWs({ type: 'SEND_CANVAS_SNAPSHOT', image_b64: b64, mime_type: 'image/jpeg' });
  }
}

function scheduleBoardSnapshotToLive() {
  if (boardSnapshotDebounce) clearTimeout(boardSnapshotDebounce);
  boardSnapshotDebounce = setTimeout(() => {
    boardSnapshotDebounce = null;
    analyzeAndSendRecentPenMarkToLive();
  }, 380);
}

elInkCanvas.addEventListener('pointerdown', (ev) => {
  if (ev.button === 1 || spacePanHeld) return; // let viewport handle middle-click or Space pan
  if (activeTool !== 'lasso' && activeTool !== 'pen') return;
  if (activeTool === 'pen' && elWelcome && elWelcome.parentElement) {
    elWelcome.remove();
  }
  if (activeTool === 'lasso') {
    activeLassoSelection = null;
  }
  elInkCanvas.setPointerCapture(ev.pointerId);
  const r = elInkCanvas.getBoundingClientRect();
  const scale = getStageScale(elStage);
  const pt = { x: (ev.clientX - r.left) / scale, y: (ev.clientY - r.top) / scale };
  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  drawingPath = {
    points: [pt],
    color: activeTool === 'lasso'
      ? (isDark ? '#f87171' : '#dc2626')
      : resolveMarkerHex(studentPenColor),
    width: activeTool === 'lasso' ? 3.0 : 2.8,
    dashed: activeTool === 'lasso',
  };
  redrawInkCanvas();
});

elInkCanvas.addEventListener('pointermove', (ev) => {
  if (!drawingPath) return;
  const r = elInkCanvas.getBoundingClientRect();
  const scale = getStageScale(elStage);
  drawingPath.points.push({ x: (ev.clientX - r.left) / scale, y: (ev.clientY - r.top) / scale });
  redrawInkCanvas();
});

elInkCanvas.addEventListener('pointerup', (ev) => {
  if (!drawingPath) return;
  const finished = drawingPath;
  drawingPath = null;

  if (activeTool === 'pen') {
    finished.createdAt = Date.now();
    penStrokes.push(finished);
    redrawInkCanvas();
    scheduleBoardSnapshotToLive();
    return;
  }

  const xs = finished.points.map((p) => p.x);
  const ys = finished.points.map((p) => p.y);
  const canvasRect = elInkCanvas.getBoundingClientRect();
  const scale = getStageScale(elStage);
  const clientBox = {
    left: canvasRect.left + Math.min(...xs) * scale,
    top: canvasRect.top + Math.min(...ys) * scale,
    right: canvasRect.left + Math.max(...xs) * scale,
    bottom: canvasRect.top + Math.max(...ys) * scale,
  };

  const mathHits = hitTestMathTerms(clientBox, elStage);
  const extracted = extractTextInsideClientRect(elStage, clientBox, cropWordsByObjectId);
  const targetObjectId = extracted.objectId || (mathHits.length ? mathHits[0].object_id : '');
  const circledText = extracted.text || '';

  // Persist visible bright-red lasso ring on canvas so student AND Gemini Live clearly see it
  activeLassoSelection = {
    points: finished.points,
    clientBox,
    text: circledText,
    objectId: targetObjectId,
    stepTitle: extracted.stepTitle || '',
    lineContext: extracted.lineContext || '',
    pdfNormBox: extracted.pdfNormBox || null,
  };
  redrawInkCanvas();

  let cropPngB64 = null;
  let pdfRef = null;
  const mediaImgs = elStage.querySelectorAll('#media-layer-zone img, .problem-clip-img');
  for (const img of mediaImgs) {
    const ir = getContainedImageClientRect(img);
    const overlaps = !(ir.right < clientBox.left || ir.left > clientBox.right || ir.bottom < clientBox.top || ir.top > clientBox.bottom);
    if (overlaps && img.naturalWidth > 0) {
      const ix0 = Math.max(0, (clientBox.left - ir.left) / ir.width);
      const iy0 = Math.max(0, (clientBox.top - ir.top) / ir.height);
      const iw = Math.min(1 - ix0, Math.max(0.08, (clientBox.right - clientBox.left) / ir.width));
      const ih = Math.min(1 - iy0, Math.max(0.08, (clientBox.bottom - clientBox.top) / ir.height));

      if (img.dataset.pdfDocId) {
        pdfRef = {
          doc_id: img.dataset.pdfDocId,
          page: Number(img.dataset.pdfPage || 1),
          rect_norm: { x: ix0, y: iy0, w: iw, h: ih },
        };
      }
      const padX = iw * 0.25, padY = ih * 0.25;
      const sx = Math.max(0, (ix0 - padX) * img.naturalWidth);
      const sy = Math.max(0, (iy0 - padY) * img.naturalHeight);
      const sw = Math.min(img.naturalWidth - sx, (iw + 2 * padX) * img.naturalWidth);
      const sh = Math.min(img.naturalHeight - sy, (ih + 2 * padY) * img.naturalHeight);
      const off = document.createElement('canvas');
      off.width = Math.max(64, Math.round(sw));
      off.height = Math.max(64, Math.round(sh));
      off.getContext('2d').drawImage(img, sx, sy, sw, sh, 0, 0, off.width, off.height);
      cropPngB64 = off.toDataURL('image/png').split(',')[1];
      break;
    }
  }

  // Always produce a zoomed-in composite crop (equations + red circle + ink) when not over a raw PDF image
  if (!cropPngB64) {
    cropPngB64 = captureZoomedLassoCrop(clientBox, finished.points);
  }

  // IMMEDIATELY push exact viewport snapshot + zoomed crop + character-level circled text to Gemini Live!
  const fullBoardB64 = captureBoardSnapshot('image/jpeg', { highlightSelection: activeLassoSelection });
  sendWs({
    type: 'USER_LASSO_SELECTION',
    gesture: 'lasso',
    image_b64: fullBoardB64 || '',
    crop_b64: cropPngB64 || '',
    mime_type: 'image/jpeg',
    circled_text: circledText,
    object_id: targetObjectId,
    step_title: extracted.stepTitle || '',
    line_context: extracted.lineContext || '',
    pdf_norm_box: extracted.pdfNormBox || null,
  });

  const label = circledText
    ? `Rodeaste: "${circledText.slice(0, 48)}"`
    : pdfRef
      ? `Recorte de PDF (pág. ${pdfRef.page})`
      : mathHits.length
        ? `Rodeaste: ${mathHits[0].term_class ? mathHits[0].term_class.replace('term-', '') : mathHits[0].object_id}`
        : 'Zona rodeada en la pizarra';

  showCirclePopover(ev.clientX, ev.clientY, {
    title: label,
    circled_text: circledText,
    hit_targets: mathHits.map(({ element, ...rest }) => rest),
    object_id: targetObjectId,
    term_class: mathHits.length ? mathHits[0].term_class : '',
    visual_crop_png_b64: cropPngB64,
    pdf_region_ref: pdfRef,
  });
});

function showCirclePopover(clientX, clientY, selection) {
  pendingCircleSelection = selection;
  elCircleTitle.textContent = selection.title || 'Selección';
  elPopCustomInput.value = '';
  elCirclePop.hidden = false;
  const x = Math.max(14, Math.min(window.innerWidth - 300, clientX + 12));
  const y = Math.max(56, Math.min(window.innerHeight - 250, clientY - 24));
  elCirclePop.style.left = `${x}px`;
  elCirclePop.style.top = `${y}px`;
}

function dispatchCircleAction(intent, questionText) {
  if (!pendingCircleSelection) return;
  const sel = pendingCircleSelection;
  elCirclePop.hidden = true;
  audioEngine.hardStopPlayback();
  setThinking(true, 'Preparando explicación y dibujo sobre lo que encerraste…');
  sendWs({
    type: intent === 'trace_origin' ? 'USER_TRACE_ORIGIN' : intent === 'expand_intermediate' ? 'USER_EXPAND_INTERMEDIATE' : 'USER_CIRCLE_QUERY',
    intent,
    question_text: questionText,
    circled_text: sel.circled_text || '',
    object_id: sel.object_id || '',
    term_class: sel.term_class || '',
    hit_targets: sel.hit_targets || [],
    visual_crop_png_b64: sel.visual_crop_png_b64 || null,
    pdf_region_ref: sel.pdf_region_ref || null,
  });
}

function triggerOriginQuery(objectId, termClass) {
  audioEngine.hardStopPlayback();
  setThinking(true, 'Señalando de dónde salió…');
  sendWs({
    type: 'USER_TRACE_ORIGIN',
    intent: 'trace_origin',
    object_id: objectId,
    term_class: termClass,
    question_text: `¿De dónde salió exactamente ${termClass || objectId}? Señala su origen con una flecha y explícalo corto con un dibujo si ayuda.`,
  });
}

function triggerIntermediateSteps(objectId) {
  audioEngine.hardStopPlayback();
  setThinking(true, 'Abriendo espacio intermedio con desarrollo y dibujo…');
  sendWs({
    type: 'USER_EXPAND_INTERMEDIATE',
    intent: 'expand_intermediate',
    object_id: objectId,
    question_text: `Abre un espacio en blanco debajo de ${objectId} y explica los pasos intermedios con álgebra clara y una gráfica/dibujo.`,
  });
}

function handleFollowupChip(label) {
  audioEngine.hardStopPlayback();
  const qMatch = /\b([qQ]\d{2,4})\b/.exec(label);
  if (qMatch && loadedPdfDocId) {
    setThinking(true, `Recortando ${qMatch[1].toUpperCase()} del PDF…`);
    sendWs({ type: 'SOLVE_PDF_PROBLEM', doc_id: loadedPdfDocId, problem_id: qMatch[1].toUpperCase() });
    return;
  }
  if (/seguir|continuar|siguiente|entendí/i.test(label)) {
    setThinking(true, 'Preparando el siguiente paso…');
    sendWs({ type: 'NEXT_STEP' });
    return;
  }
  if (/de dónde salió/i.test(label) && lastMathObjectId) {
    triggerOriginQuery(lastMathObjectId, '');
    return;
  }
  if (/pasos intermedios|espacio|dibujo|gráfica|grafica/i.test(label)) {
    triggerIntermediateSteps(lastMathObjectId);
    return;
  }
  setThinking(true, 'Preparando respuesta en pizarra…');
  sendWs({ type: 'USER_TEXT_QUERY', question_text: label });
}

// Paste images/snips directly from Windows clipboard (Ctrl+V / Win+Shift+S)
window.addEventListener('paste', (ev) => {
  const items = ev.clipboardData && ev.clipboardData.items;
  if (!items) return;
  for (const it of items) {
    if (it.type && it.type.startsWith('image/')) {
      ev.preventDefault();
      const file = it.getAsFile();
      if (!file) continue;
      const reader = new FileReader();
      reader.onload = () => {
        addImageClipCard(reader.result, 'Recorte pegado (Ctrl+V)');
        const autoSolveEl = document.getElementById('opt-auto-solve-paste');
        const autoSolve = Boolean(autoSolveEl && autoSolveEl.checked);
        const rawB64 = String(reader.result).split(',')[1] || '';
        if (rawB64) {
          if (autoSolve && !liveVoiceActive) {
            audioEngine.hardStopPlayback();
            setThinking(true, 'Leyendo tu recorte y dibujando el Paso 1…');
          } else if (liveVoiceActive) {
            setStatus('Enviando tu imagen al profesor en vivo…', 'live');
          }
          sendWs({
            type: 'SOLVE_PASTED_IMAGE',
            image_b64: rawB64,
            mime_type: file.type || 'image/png',
            caption: 'Recorte pegado (Ctrl+V)',
            auto_solve: autoSolve,
          });
        }
      };
      reader.readAsDataURL(file);
      announce('Imagen pegada en la pizarra');
      break;
    }
  }
});

// Drag-and-drop PDF or image onto board
elViewport.addEventListener('dragover', (ev) => ev.preventDefault());
elViewport.addEventListener('drop', (ev) => {
  ev.preventDefault();
  const file = ev.dataTransfer && ev.dataTransfer.files && ev.dataTransfer.files[0];
  if (file) handleUploadedFile(file);
});

async function handleUploadedFile(file) {
  if (file.type === 'application/pdf' || file.name.toLowerCase().endsWith('.pdf')) {
    setThinking(true, `Indexando y auditando ${file.name}…`);
    setStatus(`Indexando ${file.name}…`, '');
    const q = new URLSearchParams({ filename: file.name });
    if (sessionToken) q.set('token', sessionToken);
    const resp = await fetch(`${apiBase}/api/pdf/upload_raw?${q.toString()}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/pdf' },
      body: file,
    });
    setThinking(false);
    if (!resp.ok) {
      setStatus('No se pudo cargar el PDF', '');
      return;
    }
    const data = await resp.json();
    loadedPdfDocId = data.doc_id;
    document.getElementById('btn-audit-pdf').hidden = false;
    addPdfPageCard(data.doc_id, data.filename, data.page_count, data.first_page, data.problem_count || 0);
    setStatus(`PDF listo: ${data.filename} (${data.problem_count || 0} problemas)`, 'connected');
    announce(`PDF cargado con ${data.problem_count || 0} problemas`);
  } else if (file.type.startsWith('image/')) {
    const reader = new FileReader();
    reader.onload = () => {
      addImageClipCard(reader.result, file.name);
      const autoSolveEl = document.getElementById('opt-auto-solve-paste');
      const autoSolve = Boolean(autoSolveEl && autoSolveEl.checked);
      const rawB64 = String(reader.result).split(',')[1] || '';
      if (rawB64) {
        if (autoSolve && !liveVoiceActive) {
          audioEngine.hardStopPlayback();
          setThinking(true, 'Leyendo imagen y preparando Paso 1…');
        } else if (liveVoiceActive) {
          setStatus('Enviando tu imagen al profesor en vivo…', 'live');
        }
        sendWs({
          type: 'SOLVE_PASTED_IMAGE',
          image_b64: rawB64,
          mime_type: file.type || 'image/png',
          caption: file.name,
          auto_solve: autoSolve,
        });
      }
    };
    reader.readAsDataURL(file);
  }
}

function addImageClipCard(dataUrl, caption) {
  if (elWelcome && elWelcome.parentElement) elWelcome.remove();
  const box = document.createElement('div');
  box.className = 'media-box';
  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const t = document.createElement('span');
  t.className = 'widget-title';
  t.textContent = caption;
  const askBtn = document.createElement('button');
  askBtn.type = 'button';
  askBtn.className = 'mini-link-btn';
  askBtn.textContent = 'Resolver paso a paso';
  askBtn.addEventListener('click', () => {
    const rawB64 = String(dataUrl).split(',')[1] || '';
    const mimeMatch = /^data:([^;]+);/i.exec(String(dataUrl));
    const mimeType = mimeMatch ? mimeMatch[1] : 'image/png';
    if (!liveVoiceActive) {
      audioEngine.hardStopPlayback();
      setThinking(true, 'Leyendo tu imagen y preparando Paso 1…');
    } else {
      setStatus('Enviando tu imagen al profesor en vivo…', 'live');
    }
    sendWs({ type: 'SOLVE_PASTED_IMAGE', image_b64: rawB64, mime_type: mimeType, caption, auto_solve: true });
  });
  hdr.append(t, askBtn);

  const img = document.createElement('img');
  img.src = dataUrl;
  img.alt = caption;
  box.append(hdr, img);
  elMediaLayer.prepend(box);
  syncTwoColumnLayout();
}

function addPdfPageCard(docId, filename, pageCount, pagePayload, problemCount = 0) {
  if (elWelcome && elWelcome.parentElement) elWelcome.remove();
  let currentPage = pagePayload.page || 1;
  const box = document.createElement('div');
  box.className = 'media-box';

  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const title = document.createElement('span');
  title.className = 'widget-title';
  title.textContent = `${filename} · Pág. ${currentPage}/${pageCount}${problemCount ? ` (${problemCount} prob.)` : ''}`;

  const nav = document.createElement('div');
  nav.style.display = 'flex';
  nav.style.gap = '6px';
  const auditBtn = document.createElement('button');
  auditBtn.type = 'button';
  auditBtn.className = 'mini-link-btn';
  auditBtn.textContent = 'Más repetidos';
  auditBtn.addEventListener('click', () => {
    audioEngine.hardStopPlayback();
    sendWs({ type: 'AUDIT_PDF', doc_id: docId });
  });
  const prev = document.createElement('button');
  prev.type = 'button';
  prev.className = 'mini-link-btn';
  prev.textContent = 'Anterior';
  const next = document.createElement('button');
  next.type = 'button';
  next.className = 'mini-link-btn';
  next.textContent = 'Siguiente';
  nav.append(auditBtn, prev, next);
  hdr.append(title, nav);

  const img = document.createElement('img');
  img.src = `data:image/png;base64,${pagePayload.png_b64}`;
  img.alt = `${filename} página ${currentPage}`;
  img.dataset.pdfDocId = docId;
  img.dataset.pdfPage = String(currentPage);

  async function loadPage(p) {
    const target = Math.max(1, Math.min(pageCount, p));
    const r = await fetch(`${apiBase}/api/pdf/page/${docId}/${target}${sessionToken ? `?token=${encodeURIComponent(sessionToken)}` : ''}`);
    if (!r.ok) return;
    const d = await r.json();
    currentPage = d.page;
    title.textContent = `${filename} · Pág. ${currentPage}/${pageCount}${problemCount ? ` (${problemCount} prob.)` : ''}`;
    img.src = `data:image/png;base64,${d.png_b64}`;
    img.dataset.pdfPage = String(currentPage);
  }
  prev.addEventListener('click', () => loadPage(currentPage - 1));
  next.addEventListener('click', () => loadPage(currentPage + 1));

  box.append(hdr, img);
  elMediaLayer.prepend(box);
  syncTwoColumnLayout();
}

function addStudentNoteCard() {
  if (elWelcome && elWelcome.parentElement) elWelcome.remove();
  const box = document.createElement('div');
  box.className = 'student-note-box';
  const hdr = document.createElement('div');
  hdr.className = 'widget-header';
  const t = document.createElement('span');
  t.className = 'widget-title';
  t.textContent = 'Mi nota';
  hdr.appendChild(t);
  const ta = document.createElement('textarea');
  ta.placeholder = 'Escribe tus apuntes o dudas aquí…';
  box.append(hdr, ta);
  elMediaLayer.appendChild(box);
  syncTwoColumnLayout();
  ta.focus();
}

function renderPracticeResult(ver) {
  if (!ver) return;
  elPracticeFeedback.hidden = false;
  elPracticeFeedback.className = `practice-feedback ${ver.equivalent ? 'ok' : 'err'}`;
  elPracticeFeedback.textContent = ver.hint_es ? `${ver.message_es} ${ver.hint_es}` : ver.message_es;
}

function reinkAllSketchesForTheme() {
  document.querySelectorAll('.wb-sketch-entry').forEach((el) => {
    if (typeof el._reinkForTheme === 'function') el._reinkForTheme();
  });
}

function toggleTheme() {
  const cur = document.documentElement.getAttribute('data-theme');
  const next = cur === 'dark' ? 'light' : 'dark';
  document.documentElement.setAttribute('data-theme', next);
  document.documentElement.style.colorScheme = next;
  const btn = document.getElementById('btn-theme-toggle');
  if (btn) btn.textContent = next === 'dark' ? 'Pizarra clara' : 'Pizarra oscura';
  reinkAllSketchesForTheme();
  markerEngine.reinkForTheme();
  redrawInkCanvas();
}

// ============================================================================
// Wire Controls + Welcome Quick-Starts + Roving Tabindex Fallback
// ============================================================================

document.getElementById('btn-start-lesson').addEventListener('click', () => {
  const id = document.getElementById('lesson-select').value;
  sendWs({ type: 'LOAD_LESSON', lesson_id: id });
});

document.getElementById('btn-audit-pdf').addEventListener('click', () => {
  if (!loadedPdfDocId) return;
  audioEngine.hardStopPlayback();
  sendWs({ type: 'AUDIT_PDF', doc_id: loadedPdfDocId });
});

document.getElementById('btn-theme-toggle').addEventListener('click', toggleTheme);

// Welcome starter actions
document.getElementById('welcome-open-pdf').addEventListener('click', () => {
  document.getElementById('pdf-file-input').click();
});
document.getElementById('welcome-start-voice').addEventListener('click', () => {
  document.getElementById('btn-live-mic').click();
});
document.getElementById('welcome-demo-thermo').addEventListener('click', () => {
  if (elWelcome && elWelcome.parentElement) elWelcome.remove();
  setThinking(true, 'Dibujando la Primera Ley de la Termodinámica a mano…');
  sendWs({ type: 'USER_TEXT_QUERY', question_text: 'Explícame la Primera Ley de la Termodinámica desde cero dibujando un pistón a mano.' });
});

document.getElementById('btn-next-step').addEventListener('click', () => {
  audioEngine.hardStopPlayback();
  setThinking(true, 'Preparando el siguiente paso…');
  sendWs({ type: 'NEXT_STEP' });
});
document.getElementById('btn-repeat-step').addEventListener('click', () => {
  audioEngine.hardStopPlayback();
  sendWs({ type: 'REPEAT_STEP' });
});

document.getElementById('btn-stop-interrupt').addEventListener('click', () => {
  audioEngine.hardStopPlayback();
  setThinking(false);
  sendWs({
    type: 'CLIENT_INTERRUPT',
    beat_id: activeBeat ? activeBeat.beat_id : '',
    last_word_index: currentWordIdx,
    completed_action_ids: completedActionIds,
  });
  setStatus('Pausado · pregunta o continúa cuando gustes', 'connected');
  announce('Narración detenida');
});

document.getElementById('btn-live-mic').addEventListener('click', async () => {
  if (liveVoiceActive) {
    audioEngine.stopMicCapture();
    audioEngine.hardStopPlayback();
    sendWs({ type: 'STOP_LIVE_VOICE' });
    liveVoiceActive = false;
    serverConfirmedMic = false;
    if (micSilenceWatchTimer) clearTimeout(micSilenceWatchTimer);
    document.getElementById('btn-live-mic').setAttribute('aria-pressed', 'false');
    document.getElementById('mic-vu-track').hidden = true;
    setStatus('Voz en vivo desconectada', 'connected');
    return;
  }
  try {
    setStatus('Activando micrófono y conectando voz en vivo…', '');
    await audioEngine.unlockPlayback();
    micPeakRmsRecent = 0;
    const micInfo = await audioEngine.startMicCapture(selectedMicDeviceId);
    await refreshMicDeviceOptions();
    setStatus(`Micrófono listo (${micInfo.label}) · conectando…`, '');
    sendWs({ type: 'START_LIVE_VOICE' });
  } catch (err) {
    setStatus(`Micrófono no disponible: ${err.message || err}`, '');
  }
});

document.getElementById('opt-mic-select').addEventListener('change', async (e) => {
  selectedMicDeviceId = e.target.value || '';
  if (liveVoiceActive) {
    try {
      micPeakRmsRecent = 0;
      const micInfo = await audioEngine.startMicCapture(selectedMicDeviceId);
      setStatus(`Micrófono cambiado a ${micInfo.label}`, 'live');
    } catch (err) {
      setStatus(`No se pudo cambiar micrófono: ${err.message || err}`, '');
    }
  }
});

document.getElementById('tool-pointer').addEventListener('click', () => setActiveTool('pointer'));
document.getElementById('tool-pan').addEventListener('click', () => setActiveTool('pan'));
document.getElementById('tool-lasso').addEventListener('click', () => setActiveTool('lasso'));
document.getElementById('tool-pen').addEventListener('click', () => setActiveTool('pen'));

// Zoom HUD buttons
document.getElementById('btn-zoom-in').addEventListener('click', () => zoomBoardByStep(1.18));
document.getElementById('btn-zoom-out').addEventListener('click', () => zoomBoardByStep(1 / 1.18));
document.getElementById('btn-zoom-reset').addEventListener('click', () => resetBoardCamera());

// Smooth Wheel Pan & Ctrl+Wheel / Pinch Zoom on the whiteboard viewport
elViewport.addEventListener('wheel', (ev) => {
  // Let internal scrollable caption/textarea boxes scroll normally if hovered
  if (ev.target.closest('textarea, select')) return;
  ev.preventDefault();
  if (ev.ctrlKey || ev.metaKey) {
    const factor = Math.exp(-ev.deltaY * 0.0022);
    zoomBoardAroundClient(boardZoom * factor, ev.clientX, ev.clientY);
  } else if (ev.shiftKey) {
    boardPanX -= (ev.deltaY || ev.deltaX);
    applyBoardTransform();
  } else {
    boardPanX -= ev.deltaX;
    boardPanY -= ev.deltaY;
    // Gentle soft top bound so wheel scroll never vanishes above page top
    if (boardPanY > 260) boardPanY = 260;
    applyBoardTransform();
  }
}, { passive: false });

// Drag-to-Pan: active in "Mover" tool, Middle-Mouse, Space+Drag, or dragging empty background in Pointer mode
elViewport.addEventListener('pointerdown', (ev) => {
  if (ev.target.closest('#zoom-hud, button, input, textarea, select')) return;
  const isMiddle = ev.button === 1;
  const isEmptyPaperDrag =
    activeTool === 'pointer' &&
    ev.button === 0 &&
    !ev.target.closest('.math-render-host, .widget-box, .student-note-box, .welcome-card-btn');
  const shouldPan = isMiddle || spacePanHeld || (activeTool === 'pan' && ev.button === 0) || isEmptyPaperDrag;
  if (!shouldPan) return;

  ev.preventDefault();
  elViewport.setPointerCapture(ev.pointerId);
  elViewport.classList.add('is-panning');
  panDragState = {
    pointerId: ev.pointerId,
    startX: ev.clientX,
    startY: ev.clientY,
    origPanX: boardPanX,
    origPanY: boardPanY,
  };
  if (spacePanHeld) spacePanDragged = true;
});

elViewport.addEventListener('pointermove', (ev) => {
  if (!panDragState || ev.pointerId !== panDragState.pointerId) return;
  boardPanX = panDragState.origPanX + (ev.clientX - panDragState.startX);
  boardPanY = panDragState.origPanY + (ev.clientY - panDragState.startY);
  applyBoardTransform();
});

function endBoardPan(ev) {
  if (!panDragState || (ev && ev.pointerId !== panDragState.pointerId)) return;
  try { elViewport.releasePointerCapture(panDragState.pointerId); } catch {}
  panDragState = null;
  elViewport.classList.remove('is-panning');
}
elViewport.addEventListener('pointerup', endBoardPan);
elViewport.addEventListener('pointercancel', endBoardPan);
document.querySelectorAll('.pen-swatch').forEach((sw) => {
  sw.addEventListener('click', () => {
    studentPenColor = sw.dataset.penColor || 'blue';
    document.querySelectorAll('.pen-swatch').forEach((b) => b.setAttribute('aria-pressed', String(b === sw)));
  });
});
document.getElementById('btn-clear-board').addEventListener('click', () => {
  sendWs({ type: 'CLEAR_BOARD', clear_ink: true, clear_media: false });
  resetBoardVisuals({ clearInk: true, clearMedia: false, keepLiveVoice: liveVoiceActive });
  announce('Pizarra borrada');
});
document.getElementById('btn-add-note').addEventListener('click', addStudentNoteCard);

function submitTextAsk() {
  const inp = document.getElementById('ask-input');
  const q = inp.value.trim();
  if (!q) return;
  inp.value = '';
  audioEngine.hardStopPlayback();
  setThinking(true, 'El profesor está preparando el dibujo y el paso…');
  const inkPng = penStrokes.length > 0 ? captureBoardSnapshot('image/png') : null;
  sendWs({
    type: 'USER_TEXT_QUERY',
    question_text: q,
    visual_crop_png_b64: inkPng,
  });
}
document.getElementById('btn-ask-send').addEventListener('click', submitTextAsk);
document.getElementById('ask-input').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') submitTextAsk();
});

document.getElementById('btn-import-pdf').addEventListener('click', () => {
  document.getElementById('pdf-file-input').click();
});
document.getElementById('pdf-file-input').addEventListener('change', (ev) => {
  const f = ev.target.files && ev.target.files[0];
  if (f) handleUploadedFile(f);
  ev.target.value = '';
});

document.getElementById('pop-btn-origin').addEventListener('click', () => dispatchCircleAction('trace_origin', '¿De dónde salió exactamente esto?'));
document.getElementById('pop-btn-intermediate').addEventListener('click', () => dispatchCircleAction('expand_intermediate', 'Abre un espacio en blanco aquí debajo y explica los pasos intermedios con álgebra y un dibujo.'));
document.getElementById('pop-btn-explain').addEventListener('click', () => dispatchCircleAction('explain_from_zero', 'Explícame esto desde cero dibujándolo a mano.'));
document.getElementById('pop-btn-send').addEventListener('click', () => dispatchCircleAction('circle_question', elPopCustomInput.value.trim()));
elPopCustomInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') dispatchCircleAction('circle_question', elPopCustomInput.value.trim());
});

document.getElementById('btn-verify-practice').addEventListener('click', () => {
  const val = elPracticeInput.value.trim();
  if (!val) return;
  if (expectedPracticeExpr) {
    sendWs({
      type: 'VERIFY_STUDENT_WORK',
      expected_expr: expectedPracticeExpr,
      student_expr: val,
      concept_tag: expectedPracticeConcept,
    });
  } else {
    sendWs({ type: 'USER_TEXT_QUERY', question_text: `Mi respuesta es: ${val}` });
  }
});

document.getElementById('btn-bake-cudoc').addEventListener('click', () => {
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({ type: 'WEB_BAKE_SESSION', beats: committedBeats });
    announce('Guardando pizarra en el cuaderno CuadernU');
  } else {
    setStatus('Disponible al abrir la pizarra dentro de CuadernU.exe', 'connected');
  }
});

// More popover settings
document.getElementById('opt-narration-audio').addEventListener('change', (e) => {
  sendWs({ type: 'UPDATE_SETTINGS', speech_enabled: e.target.checked });
});
document.getElementById('opt-speech-rate').addEventListener('input', (e) => {
  const v = Number(e.target.value);
  document.getElementById('opt-speech-rate-val').textContent = `${v.toFixed(2)}×`;
  sendWs({ type: 'UPDATE_SETTINGS', speech_rate: v });
});
document.getElementById('opt-voice-select').addEventListener('change', (e) => {
  sendWs({ type: 'UPDATE_SETTINGS', tts_voice: e.target.value });
});
document.getElementById('opt-visual-model').addEventListener('change', (e) => {
  sendWs({ type: 'UPDATE_SETTINGS', visual_model: e.target.value });
});
document.getElementById('opt-clear-ink').addEventListener('click', () => {
  penStrokes = [];
  activeLassoSelection = null;
  redrawInkCanvas();
  announce('Trazos de lápiz limpiados');
});
const btnMarkBug = document.getElementById('btn-mark-bug');
if (btnMarkBug) {
  btnMarkBug.addEventListener('click', () => {
    const pop = document.getElementById('more-popover');
    if (pop && typeof pop.hidePopover === 'function') {
      try { pop.hidePopover(); } catch {}
    }
    reportIncidentToFlightRecorder('manual_bug_mark', 'Marcado por el usuario desde menú');
  });
}

// Keyboard shortcuts: Escape closes popover; Ctrl+Shift+B marks incident; Ctrl +/-/0 zooms; Space tap interrupts, Space+Drag pans
window.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') elCirclePop.hidden = true;
  if ((e.ctrlKey || e.metaKey) && e.shiftKey && !e.altKey && (e.key === 'B' || e.key === 'b')) {
    e.preventDefault();
    reportIncidentToFlightRecorder('manual_hotkey_mark', 'Marcado con Ctrl+Shift+B');
    return;
  }
  if ((e.ctrlKey || e.metaKey) && !e.altKey) {
    if (e.key === '=' || e.key === '+') {
      e.preventDefault();
      zoomBoardByStep(1.18);
      return;
    }
    if (e.key === '-') {
      e.preventDefault();
      zoomBoardByStep(1 / 1.18);
      return;
    }
    if (e.key === '0') {
      e.preventDefault();
      resetBoardCamera();
      return;
    }
  }
  if (e.code === 'Space') {
    const tag = (document.activeElement && document.activeElement.tagName) || '';
    if (tag !== 'INPUT' && tag !== 'TEXTAREA' && tag !== 'SELECT' && tag !== 'BUTTON') {
      e.preventDefault();
      if (!spacePanHeld) {
        spacePanHeld = true;
        spacePanDragged = false;
        elViewport.classList.add('pan-ready');
        elInkCanvas.classList.remove('interactive');
      }
    }
  }
});

window.addEventListener('keyup', (e) => {
  if (e.code === 'Space' && spacePanHeld) {
    const didDrag = spacePanDragged;
    spacePanHeld = false;
    spacePanDragged = false;
    elViewport.classList.toggle('pan-ready', activeTool === 'pan');
    elInkCanvas.classList.toggle('interactive', activeTool === 'lasso' || activeTool === 'pen');
    if (!didDrag) {
      document.getElementById('btn-stop-interrupt').click();
    }
  }
});

// Sync theme & messages from C++ CuadernU WebView2 host
if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener('message', (ev) => {
    const data = typeof ev.data === 'string' ? JSON.parse(ev.data) : ev.data;
    if (!data) return;
    if (data.type === 'HOST_THEME_SYNC') {
      const mode = data.mode === 'dark' ? 'dark' : 'light';
      document.documentElement.setAttribute('data-theme', mode);
      document.documentElement.style.colorScheme = mode;
      document.documentElement.setAttribute('data-reduce-transparency', String(Boolean(data.reduceTransparency)));
      reduceMotion = Boolean(data.reduceMotion);
      const btn = document.getElementById('btn-theme-toggle');
      if (btn) btn.textContent = mode === 'dark' ? 'Pizarra clara' : 'Pizarra oscura';
      reinkAllSketchesForTheme();
      markerEngine.setReduceMotion(reduceMotion);
      markerEngine.reinkForTheme();
      redrawInkCanvas();
    } else if (data.type === 'HOST_BAKE_RESULT') {
      setStatus(data.summary || 'Guardado en cuaderno', 'connected');
      announce(data.summary || 'Guardado en cuaderno');
    }
  });
}

// Toolbar roving tabindex fallback ONLY when native focusgroup is unsupported
if (!('focusGroup' in HTMLElement.prototype)) {
  const bar = document.querySelector('[role="toolbar"]');
  if (bar) {
    const items = Array.from(bar.querySelectorAll('button'));
    items.forEach((b, i) => { b.tabIndex = i === 0 ? 0 : -1; });
    bar.addEventListener('focusin', ({ target }) => {
      if (items.includes(target)) items.forEach((b) => { b.tabIndex = b === target ? 0 : -1; });
    });
    bar.addEventListener('keydown', (e) => {
      const idx = items.indexOf(document.activeElement);
      if (idx < 0) return;
      let next = -1;
      if (e.key === 'ArrowRight') next = (idx + 1) % items.length;
      else if (e.key === 'ArrowLeft') next = (idx - 1 + items.length) % items.length;
      else if (e.key === 'Home') next = 0;
      else if (e.key === 'End') next = items.length - 1;
      if (next >= 0) {
        e.preventDefault();
        items[next].focus();
      }
    });
  }
}

window.addEventListener('resize', resizeInkCanvas);
connectWebSocket();
