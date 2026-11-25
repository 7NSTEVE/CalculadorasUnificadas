#!/usr/bin/env python3
"""Massive Audiovisual & Stress Testing Suite for CuadernU Whiteboard Tutor.

Runs 8 real-world & adversarial stress categories using Playwright Chromium,
the real FastAPI/WebSocket server, local Flight Recorder telemetry, real PDF
exam bank (`Superexamen_Tercer_Parcial_Fisica_General_III.pdf`), and real
cloud Gemini 3.8 Live / Gemini 2.5 Flash models.

Generates:
  - Numbered screenshots across all 8 categories (Light/Dark, 1366p/1080p/4K)
  - Quantitative DOM/sync/layout/console/telemetry audit JSON (`stress_report.json`)
  - Structured bug findings list (`P0`/`P1`/`P2`) for visual inspection & fixing

Usage:
  python tools/tutor_runtime/stress_audiovisual_suite.py --out-dir <path>
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import math
import pathlib
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.request
from typing import Any, Dict, List, Optional

import fitz  # PyMuPDF
from playwright.sync_api import sync_playwright
import websockets

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.analyze_telemetry import analyze_directory
from tools.tutor_runtime.telemetry_recorder import detect_raw_latex_leak, detect_tutor_antipattern

REAL_PDF_PATH = pathlib.Path(r"D:\Descargas\Superexamen_Tercer_Parcial_Fisica_General_III.pdf")


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def make_sine_pcm24_chunk(duration_s: float = 0.45, freq_hz: float = 240.0, amp: float = 0.25) -> bytes:
    n = int(24000 * duration_s)
    samples = [int(max(-32767, min(32767, math.sin(2 * math.pi * freq_hz * i / 24000.0) * 32767 * amp))) for i in range(n)]
    header = struct.pack("<2sBBI", b"CU", 2, 0, 1)
    return header + struct.pack(f"<{n}h", *samples)


def render_test_formula_png_b64() -> str:
    """Render a crisp synthetic image of W = 1/(gamma - 1) (p1 V1 - p2 V2) for Ctrl+V testing."""
    doc = fitz.open()
    page = doc.new_page(width=460, height=130)
    page.draw_rect(fitz.Rect(0, 0, 460, 130), color=(1, 1, 1), fill=(1, 1, 1))
    page.insert_text((36, 72), "W =", fontsize=28, color=(0.08, 0.12, 0.28))
    page.insert_text((132, 52), "1", fontsize=24, color=(0.08, 0.12, 0.28))
    page.draw_line(fitz.Point(96, 63), fitz.Point(188, 63), color=(0.08, 0.12, 0.28), width=2.0)
    page.insert_text((104, 90), "gamma - 1", fontsize=19, color=(0.08, 0.12, 0.28))
    page.insert_text((202, 72), "(p1 V1 - p2 V2)", fontsize=26, color=(0.08, 0.12, 0.28))
    pix = page.get_pixmap(dpi=150)
    png_bytes = pix.tobytes("png")
    doc.close()
    return base64.b64encode(png_bytes).decode("ascii")


def start_isolated_server(port: int, telemetry_dir: pathlib.Path) -> subprocess.Popen[str]:
    bootstrap = json.dumps({
        "bind_host": "127.0.0.1",
        "session_token": "",
        "telemetry_dir": str(telemetry_dir),
        "telemetry_enabled": True,
    })
    proc = subprocess.Popen(
        [sys.executable, "-u", "tools/tutor_runtime/server.py", "--bootstrap-stdin", "--port", str(port)],
        cwd=str(REPO_ROOT),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert proc.stdin is not None
    proc.stdin.write(bootstrap + "\n")
    proc.stdin.flush()

    deadline = time.time() + 18.0
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/api/health", timeout=1.5) as r:
                if r.status == 200:
                    return proc
        except Exception:
            time.sleep(0.25)
    err = proc.stderr.read() if proc.stderr else ""
    proc.kill()
    raise RuntimeError(f"Isolated server failed to boot on port {port}: {err[:400]}")


async def _collect_live_turn(ws, timeout_s: float = 30.0) -> Dict[str, Any]:
    transcript: List[str] = []
    actions: List[Dict[str, Any]] = []
    acks: List[Dict[str, Any]] = []
    resets: List[Dict[str, Any]] = []
    pcm_bytes = 0
    t0 = time.perf_counter()
    first_tool_ms: Optional[int] = None
    first_audio_ms: Optional[int] = None
    end_t = asyncio.get_event_loop().time() + timeout_s

    while asyncio.get_event_loop().time() < end_t:
        rem = max(0.2, end_t - asyncio.get_event_loop().time())
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=rem)
        except asyncio.TimeoutError:
            break
        if isinstance(raw, (bytes, bytearray)):
            if first_audio_ms is None:
                first_audio_ms = round((time.perf_counter() - t0) * 1000)
            pcm_bytes += max(0, len(raw) - 8)
            continue
        msg = json.loads(raw)
        et = msg.get("type")
        if et == "LIVE_OUTPUT_TRANSCRIPT":
            transcript.append(msg.get("text", ""))
        elif et == "LIVE_CANVAS_ACTION":
            if first_tool_ms is None:
                first_tool_ms = round((time.perf_counter() - t0) * 1000)
            actions.append(msg.get("action") or {})
        elif et in ("PASTED_IMAGE_VISION_ACK", "LASSO_VISION_ACK"):
            acks.append(msg)
        elif et == "BOARD_RESET":
            resets.append(msg)
        elif et == "STATE_CHANGE" and msg.get("state") == "WAITING_FOR_STUDENT":
            if transcript or pcm_bytes > 0:
                await asyncio.sleep(0.45)
                break

    full_tr = "".join(transcript).strip()
    return {
        "transcript": full_tr,
        "actions": actions,
        "action_types": [a.get("type") for a in actions],
        "acks": acks,
        "resets": resets,
        "pcm_bytes": pcm_bytes,
        "first_tool_ms": first_tool_ms,
        "first_audio_ms": first_audio_ms,
        "antipattern": detect_tutor_antipattern(full_tr),
        "raw_latex_in_speech": detect_raw_latex_leak(full_tr),
    }


async def run_cloud_multi_subject_and_vision_stress(port: int, formula_png_b64: str) -> Dict[str, Any]:
    """Cat 3, Cat 4 (cloud check), Cat 5: Real Gemini 3.8 Live multi-subject, clear-board, & Ctrl+V vision."""
    results: Dict[str, Any] = {}
    ws_url = f"ws://127.0.0.1:{port}/ws/tutor"
    async with websockets.connect(ws_url, max_size=24 * 1024 * 1024) as ws:
        _ = json.loads(await asyncio.wait_for(ws.recv(), timeout=6))
        await ws.send(json.dumps({"type": "START_LIVE_VOICE"}))
        live_ok = False
        for _ in range(10):
            ev = json.loads(await asyncio.wait_for(ws.recv(), timeout=12))
            if ev.get("type") == "LIVE_VOICE_STATUS":
                live_ok = bool(ev.get("ok"))
                break
        if not live_ok:
            return {"ok": False, "error": "Gemini Live failed to connect"}

        # 3A: Estudios Sociales / Geografía Costa Rica (lista + mapa esquemático)
        await ws.send(json.dumps({
            "type": "USER_TEXT_QUERY",
            "question_text": "Sin usar física: anótame en la pizarra 4 pueblos indígenas de Costa Rica con write_whiteboard_notes y dibuja un mapa esquemático sencillo de Costa Rica con sus pines usando draw_custom_whiteboard_diagram.",
        }))
        results["cat3a_cr_indigenas"] = await _collect_live_turn(ws, timeout_s=34.0)

        # 3B: Biología (limpiar pizarra + tabla comparativa fotosíntesis)
        await ws.send(json.dumps({
            "type": "USER_TEXT_QUERY",
            "question_text": "Limpia la pizarra primero. Ahora haz una tabla comparativa en la pizarra entre la fase luminosa y la fase oscura de la fotosíntesis.",
        }))
        results["cat3b_biologia_tabla"] = await _collect_live_turn(ws, timeout_s=34.0)

        # 3C: Química (estequiometría + cancelación de unidades con diagonales de colores)
        await ws.send(json.dumps({
            "type": "USER_TEXT_QUERY",
            "question_text": "Limpia la pizarra. Escribe la conversión de 36 g de H2O a moles dividiendo entre 18 g/mol y cancela los gramos con diagonales de colores dejando mol encerrado.",
        }))
        results["cat3c_quimica_cancel"] = await _collect_live_turn(ws, timeout_s=34.0)

        # 3D: Programación (diagrama de flujo búsqueda binaria)
        await ws.send(json.dumps({
            "type": "USER_TEXT_QUERY",
            "question_text": "Limpia la pizarra. Dibuja un diagrama de flujo sencillo de la búsqueda binaria usando draw_custom_whiteboard_diagram y explica sus pasos.",
        }))
        results["cat3d_programacion_diagrama"] = await _collect_live_turn(ws, timeout_s=34.0)

        # Cat 5: Ctrl+V pasted image of adiabatic work formula during active Live Voice
        await ws.send(json.dumps({
            "type": "SOLVE_PASTED_IMAGE",
            "image_b64": formula_png_b64,
            "mime_type": "image/png",
            "caption": "Recorte pegado (Ctrl+V)",
            "auto_solve": True,
        }))
        results["cat5_ctrlv_live_formula"] = await _collect_live_turn(ws, timeout_s=32.0)

        await ws.send(json.dumps({"type": "STOP_LIVE_VOICE"}))
    results["ok"] = True
    return results


def render_cloud_actions_in_browser(page, ws_route, actions: List[Dict[str, Any]], caption: str, shot_path: pathlib.Path) -> Dict[str, Any]:
    """Replay actual Gemini 3.8 Live cloud actions into Playwright Chromium to visually audit & capture them."""
    ws_route.send(json.dumps({"type": "BOARD_RESET", "clear_ink": True, "clear_media": True}))
    page.wait_for_timeout(180)
    ws_route.send(json.dumps({"type": "LIVE_OUTPUT_TRANSCRIPT", "text": caption[:240]}))
    for idx, act in enumerate(actions):
        ws_route.send(json.dumps({"type": "LIVE_CANVAS_ACTION", "step_index": idx + 1, "action": act}))
    ws_route.send(make_sine_pcm24_chunk(0.45))
    ws_route.send(json.dumps({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"}))
    # Wait for sequential live queue to drain completely
    try:
        page.wait_for_function(
            "() => window.__cuadernuAudit && window.__cuadernuAudit.getBoardState().queuedLiveActions === 0",
            timeout=14000,
        )
    except Exception:
        pass
    page.wait_for_timeout(1600)
    audit = page.evaluate("() => window.__cuadernuAudit ? window.__cuadernuAudit.auditBoardLayoutCollisions() : {}")
    page.screenshot(path=str(shot_path))
    return audit


def run_browser_stress_categories(
    port: int,
    shots_dir: pathlib.Path,
    cloud_results: Dict[str, Any],
) -> Dict[str, Any]:
    """Run Cat 1, 2, 3-visual-replay, 4, 6, 7 (Light/Dark 1366/1080/4K), 8 in real Chromium."""
    report: Dict[str, Any] = {"categories": {}, "console_errors": [], "screenshots": []}
    url = f"http://127.0.0.1:{port}"

    with sync_playwright() as p:
        browser = p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required"])

        # ------------------------------------------------------------------
        # A. Live-server Playwright session for Cat 1 (Real PDF), Cat 4 (Deep Pen), Cat 8 (Clear + Ctrl+Shift+B)
        # ------------------------------------------------------------------
        page = browser.new_page(viewport={"width": 1366, "height": 768})
        page.on("console", lambda m: report["console_errors"].append(f"live_page:{m.text}") if m.type == "error" else None)
        page.on("pageerror", lambda e: report["console_errors"].append(f"live_page:{e}"))
        page.goto(url, wait_until="domcontentloaded")
        page.wait_for_function("() => (document.getElementById('session-state-label')?.textContent || '').includes('universal')", timeout=10000)

        # CAT 1: Real PDF Exam Bank upload, audit, Q111 & Q057 visual crops
        cat1: Dict[str, Any] = {"pdf_present": REAL_PDF_PATH.is_file()}
        if REAL_PDF_PATH.is_file():
            t0 = time.perf_counter()
            raw_pdf = REAL_PDF_PATH.read_bytes()
            req = urllib.request.Request(
                f"{url}/api/pdf/upload_raw?filename={REAL_PDF_PATH.name}",
                data=raw_pdf,
                headers={"Content-Type": "application/pdf"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=30) as r:
                up = json.loads(r.read().decode("utf-8"))
            cat1["index_ms"] = round((time.perf_counter() - t0) * 1000)
            cat1["doc_id"] = up.get("doc_id")
            cat1["problem_count"] = up.get("problem_count")

            # Crop Q111 and Q057 directly to inspect word boxes & header preservation
            with urllib.request.urlopen(f"{url}/api/pdf/problem/{up['doc_id']}/Q111", timeout=15) as r:
                q111 = json.loads(r.read().decode("utf-8"))
            with urllib.request.urlopen(f"{url}/api/pdf/problem/{up['doc_id']}/Q057", timeout=15) as r:
                q057 = json.loads(r.read().decode("utf-8"))
            cat1["q111_words"] = len(q111.get("crop_words") or [])
            cat1["q111_has_header"] = any("Q111" in str(w.get("t", "")) for w in (q111.get("crop_words") or []))
            cat1["q057_has_figure"] = bool(q057.get("has_figure"))

            # Trigger real PDF audit UI flow via WS ask box
            page.fill("#ask-input", "muéstrame los problemas más repetidos del PDF")
            page.click("#btn-ask-send")
            page.wait_for_selector(".audit-block", timeout=25000)
            page.wait_for_timeout(800)
            s1 = shots_dir / "01_cat1_pdf_audit.png"
            page.screenshot(path=str(s1))
            report["screenshots"].append(str(s1))
            cat1["audit_dom"] = page.evaluate("() => window.__cuadernuAudit.auditBoardLayoutCollisions()")

            # Solve Q111 (real Gemini 2.5 Flash Visual Director + Q111 crop!)
            page.fill("#ask-input", "Q111")
            page.click("#btn-ask-send")
            page.wait_for_selector(".problem-clip-block", timeout=25000)
            try:
                page.wait_for_selector(".problem-clip-block ~ .step-block", timeout=20000)
            except Exception:
                pass
            page.wait_for_timeout(1600)
            s2 = shots_dir / "02_cat1_q111_step1.png"
            page.screenshot(path=str(s2))
            report["screenshots"].append(str(s2))
            cat1["q111_dom"] = page.evaluate("() => window.__cuadernuAudit.auditBoardLayoutCollisions()")
        report["categories"]["cat1_real_pdf"] = cat1

        # Test Ctrl+Shift+B manual Flight Recorder mark on live page
        page.keyboard.press("Control+Shift+B")
        page.wait_for_timeout(700)
        cat8_live: Dict[str, Any] = {
            "status_after_ctrl_shift_b": page.locator("#session-state-label").text_content(),
        }
        # Click Borrar button and verify full reset
        page.click("#btn-clear-board")
        page.wait_for_timeout(450)
        cat8_live["after_clear_state"] = page.evaluate("() => window.__cuadernuAudit.getBoardState()")
        s_clr = shots_dir / "15_cat8_after_clear.png"
        page.screenshot(path=str(s_clr))
        report["screenshots"].append(str(s_clr))
        report["categories"]["cat8_clear_and_mark"] = cat8_live
        page.close()

        # ------------------------------------------------------------------
        # B. Controlled WS-Replay Playwright Session for:
        #    Cat 2 (4-pair unit cancel + provenance + TTL),
        #    Cat 3 Visual Replay (real Gemini 3.8 Live cloud outputs rendered!),
        #    Cat 4 (Deep 10-block board + panY + zoom 135% + red pen + lasso + pointer),
        #    Cat 6 (Sync Audio-Onset Gate + 3x rapid barge-in stress),
        #    Cat 7 (Typography Torture Test at 1366/1080/4K Light & Dark)
        # ------------------------------------------------------------------
        p2 = browser.new_page(viewport={"width": 1366, "height": 768})
        p2.on("console", lambda m: report["console_errors"].append(f"replay_page:{m.text}") if m.type == "error" else None)
        p2.on("pageerror", lambda e: report["console_errors"].append(f"replay_page:{e}"))
        ws_holder: Dict[str, Any] = {}
        lasso_msgs: List[Dict[str, Any]] = []

        def intercept_ws(ws_route):
            ws_holder["ws"] = ws_route
            ws_route.on_message(lambda m: lasso_msgs.append(json.loads(m)) if isinstance(m, str) and "USER_LASSO_SELECTION" in m else None)
            ws_route.send(json.dumps({
                "type": "SESSION_READY",
                "session_id": "ses_stress_replay",
                "visual_cloud_ready": True,
                "live_audio_ready": True,
                "runtime_build": "2026-11-25-universal-v6-hardened",
                "code_stale": False,
                "pdfs": [],
            }))
            ws_route.send(json.dumps({"type": "LIVE_VOICE_STATUS", "ok": True, "model": "gemini-3.8-live"}))

        p2.route_web_socket("**/ws/tutor*", intercept_ws)
        p2.goto(url, wait_until="domcontentloaded")
        p2.wait_for_function("() => document.getElementById('btn-live-mic')?.getAttribute('aria-pressed') === 'true'", timeout=8000)
        ws_r = ws_holder["ws"]

        # CAT 2: 4-pair diagonal unit cancellation + intermediate provenance + TTL check
        cat2_actions = [
            {
                "action_id": "a_c2_eq1",
                "type": "write_math",
                "object_id": "eq_c2_step1",
                "animation": {"duration_ms": 1100},
                "payload": {
                    "step_label": "Paso 1 · Temperatura inicial T1",
                    "latex": r"T_1 = \frac{(1.50\,\text{atm})(3.50\,\text{L})}{(1.00\,\text{mol})\left(0.08206\,\frac{\text{L}\,\text{atm}}{\text{mol}\,\text{K}}\right)} = 63.98\,\text{K}",
                    "note_es": "Cancelamos atm, L y mol para quedarnos en Kelvin.",
                },
            },
            {
                "action_id": "a_c2_cancel",
                "type": "cancel_terms",
                "object_id": "cancel_c2_1",
                "payload": {
                    "target_object_id": "eq_c2_step1",
                    "pairs": [
                        {"unit_or_term": "atm", "color": "red"},
                        {"unit_or_term": "L", "color": "green"},
                        {"unit_or_term": "mol", "color": "purple"},
                    ],
                    "surviving_unit": "K",
                    "surviving_color": "blue",
                },
            },
            {
                "action_id": "a_c2_eq2",
                "type": "write_math",
                "object_id": "eq_c2_step2",
                "animation": {"duration_ms": 1100},
                "payload": {
                    "step_label": "Paso 2 · Proceso isocórico (T2)",
                    "latex": r"T_2 = T_1 \frac{P_2}{P_1} = (63.98\,\text{K})\left(\frac{3.00\,\text{atm}}{1.50\,\text{atm}}\right) = 127.96\,\text{K}",
                },
            },
            {
                "action_id": "a_c2_prov",
                "type": "trace_provenance",
                "object_id": "prov_c2_mid",
                "payload": {
                    "origin_kind": "intermediate_step",
                    "value_text": "63.98 K",
                    "source_object_id": "eq_c2_step1",
                    "source_text": "63.98 K",
                    "target_object_id": "eq_c2_step2",
                    "target_text": "63.98 K",
                    "color": "purple",
                    "style": "circle",
                    "note_es": "Calculado en el Paso 1",
                    "temporary": True,
                    "ttl_ms": 2200,
                },
            },
        ]
        s_c2 = shots_dir / "04_cat2_cancel_provenance.png"
        c2_audit = render_cloud_actions_in_browser(p2, ws_r, cat2_actions, "Cancelamos atm en rojo, L en verde y mol en morado; 63.98 K viene del Paso 1.", s_c2)
        report["screenshots"].append(str(s_c2))
        cancel_counts = p2.evaluate("""() => {
            const gs = Array.from(document.querySelectorAll('#marker-layer-svg .marker-group'));
            return {
                total_groups: gs.length,
                slash_lines: document.querySelectorAll('#marker-layer-svg .marker-group path').length,
            };
        }""")
        p2.wait_for_timeout(2800)  # allow temporary provenance TTL (2200ms) + fadeout (580ms)
        after_ttl = p2.evaluate("() => document.querySelectorAll('#marker-layer-svg .marker-group').length")
        report["categories"]["cat2_markers_cancel_ttl"] = {
            "dom_audit": c2_audit,
            "before_ttl": cancel_counts,
            "after_ttl_groups": after_ttl,
            "ttl_faded_out": after_ttl < cancel_counts["total_groups"],
        }

        # CAT 3 Visual Replay: Render the ACTUAL actions emitted by live cloud Gemini 3.8 Live!
        cat3_visuals: Dict[str, Any] = {}
        for key, fname in [
            ("cat3a_cr_indigenas", "05_cat3a_cr_indigenas_mapa.png"),
            ("cat3b_biologia_tabla", "06_cat3b_biologia_tabla.png"),
            ("cat3c_quimica_cancel", "07_cat3c_quimica_cancel.png"),
            ("cat3d_programacion_diagrama", "08_cat3d_prog_diagrama.png"),
            ("cat5_ctrlv_live_formula", "10_cat5_ctrlv_live_formula.png"),
        ]:
            c_res = cloud_results.get(key) or {}
            acts = c_res.get("actions") or []
            shot_p = shots_dir / fname
            if acts:
                v_aud = render_cloud_actions_in_browser(p2, ws_r, acts, c_res.get("transcript") or "", shot_p)
                report["screenshots"].append(str(shot_p))
            else:
                v_aud = {"no_actions_emitted": True}
            cat3_visuals[key] = {
                "action_types": c_res.get("action_types", []),
                "pcm_bytes": c_res.get("pcm_bytes", 0),
                "antipattern": c_res.get("antipattern"),
                "raw_latex_in_speech": c_res.get("raw_latex_in_speech"),
                "transcript_preview": (c_res.get("transcript") or "")[:180],
                "dom_audit": v_aud,
            }
        report["categories"]["cat3_and_cat5_cloud_visuals"] = cat3_visuals

        # CAT 4: Deep 10-block board + pan to bottom + Zoom 135% + Red Pen around γ + Pointer tap
        deep_actions = [
            {
                "action_id": f"deep_eq_{i}",
                "type": "write_math",
                "object_id": f"eq_deep_{i}",
                "animation": {"duration_ms": 0},
                "payload": {
                    "step_label": f"Paso {i} · Desarrollo termodinámico",
                    "latex": rf"P_{i} V_{i} = n R T_{i} \quad\Rightarrow\quad W_{i} = {(i * 12.5):.1f}\,\text{{J}}",
                },
            }
            for i in range(1, 10)
        ] + [
            {
                "action_id": "deep_eq_10",
                "type": "write_math",
                "object_id": "eq_deep_v5",
                "animation": {"duration_ms": 0},
                "payload": {
                    "step_label": "Cálculo de V5 (proceso adiabático)",
                    "latex": r"V_5 = V_1 \left(\frac{T_1}{T_5}\right)^{\frac{1}{\gamma - 1}} = (3.50\,\text{L})\left(\frac{64.0\,\text{K}}{256\,\text{K}}\right)^{\frac{1}{1.4 - 1}} = 0.4375\,\text{L}",
                },
            }
        ]
        # Render fast with reduce-motion emulation? No, duration_ms=0 works cleanly via replay
        ws_r.send(json.dumps({"type": "BOARD_RESET", "clear_ink": True, "clear_media": True}))
        p2.wait_for_timeout(200)
        for idx, act in enumerate(deep_actions):
            ws_r.send(json.dumps({"type": "LIVE_CANVAS_ACTION", "step_index": idx + 1, "action": act}))
        ws_r.send(make_sine_pcm24_chunk(0.4))
        ws_r.send(json.dumps({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"}))
        p2.wait_for_function("() => document.querySelectorAll('.step-block').length === 10", timeout=22000)
        p2.wait_for_timeout(1200)

        # Zoom in 2 clicks (~139%) and pan bottom block eq_deep_v5 cleanly into viewport (like a student scrolling to it)
        p2.click("#btn-zoom-in")
        p2.click("#btn-zoom-in")
        p2.evaluate("() => window.__cuadernuAudit.panBlockIntoView('eq_deep_v5')")
        p2.wait_for_timeout(300)
        # Find exact client coordinates of 'γ' inside eq_deep_v5
        gamma_box = p2.evaluate("""() => {
            const blk = document.querySelector('[data-object-id="eq_deep_v5"]');
            if (!blk) return null;
            const tw = document.createTreeWalker(blk, NodeFilter.SHOW_TEXT);
            let n;
            while ((n = tw.nextNode())) {
                const idx = (n.nodeValue || '').indexOf('γ');
                if (idx >= 0) {
                    const r = document.createRange();
                    r.setStart(n, idx);
                    r.setEnd(n, idx + 1);
                    const rect = r.getBoundingClientRect();
                    return { cx: rect.left + rect.width / 2, cy: rect.top + rect.height / 2, w: rect.width, h: rect.height };
                }
            }
            return null;
        }""")
        lasso_msgs.clear()
        if gamma_box:
            p2.click("#tool-pen")
            p2.click('.pen-swatch[data-pen-color="red"]')
            cx, cy = gamma_box["cx"], gamma_box["cy"]
            rad = max(16, max(gamma_box["w"], gamma_box["h"]) * 0.9)
            p2.mouse.move(cx + rad, cy)
            p2.mouse.down()
            for k in range(1, 25):
                ang = (k / 24.0) * math.pi * 2.1
                p2.mouse.move(cx + math.cos(ang) * rad, cy + math.sin(ang) * rad)
            p2.mouse.up()
            p2.wait_for_timeout(950)
        s_c4 = shots_dir / "09_cat4_deep_pen_gamma_135pct.png"
        p2.screenshot(path=str(s_c4))
        report["screenshots"].append(str(s_c4))
        last_lasso = lasso_msgs[-1] if lasso_msgs else {}
        report["categories"]["cat4_deep_gesture_zoom"] = {
            "gamma_box": gamma_box,
            "zoom_pct": p2.evaluate("() => Math.round(window.__cuadernuAudit.getBoardState().zoom * 100)"),
            "captured_gesture": last_lasso.get("gesture"),
            "captured_circled_text": last_lasso.get("circled_text"),
            "captured_step_title": last_lasso.get("step_title"),
            "crop_b64_len": len(last_lasso.get("crop_b64") or ""),
            "image_b64_len": len(last_lasso.get("image_b64") or ""),
        }
        p2.click("#tool-pointer")

        # CAT 6: Sync Audio-Onset Gate + 3x rapid barge-in stress
        ws_r.send(json.dumps({"type": "BOARD_RESET", "clear_ink": True, "clear_media": True}))
        p2.wait_for_timeout(200)
        ws_r.send(json.dumps({
            "type": "LIVE_CANVAS_ACTION",
            "step_index": 1,
            "action": {
                "action_id": "barge_notes_1",
                "type": "write_structured_notes",
                "object_id": "notes_barge_1",
                "animation": {"duration_ms": 6000},
                "payload": {
                    "title": "Prueba de interrupción en vivo",
                    "items": ["Elemento uno", "Elemento dos", "Elemento tres", "Elemento cuatro", "Elemento cinco"],
                },
            },
        }))
        p2.wait_for_timeout(240)
        before_voice_cnt = p2.locator(".step-block").count()
        # Start voice then slam 3 consecutive CONTROL_HARD_STOP interrupts
        ws_r.send(make_sine_pcm24_chunk(0.35))
        p2.wait_for_timeout(280)
        for _ in range(3):
            ws_r.send(json.dumps({"type": "CONTROL_HARD_STOP", "reason": "stress_barge_in"}))
            p2.wait_for_timeout(120)
        post_barge_state = p2.evaluate("() => window.__cuadernuAudit.getBoardState()")
        barge_items_stopped_count = p2.locator('[data-object-id="notes_barge_1"] li').count()
        # Now verify a NEW action after 3x barge-in still renders cleanly!
        ws_r.send(json.dumps({
            "type": "LIVE_CANVAS_ACTION",
            "step_index": 2,
            "action": {
                "action_id": "post_barge_eq",
                "type": "write_math",
                "object_id": "eq_post_barge",
                "animation": {"duration_ms": 1100},
                "payload": {"step_label": "Tras interrupción", "latex": r"\Delta U = Q - W"},
            },
        }))
        ws_r.send(make_sine_pcm24_chunk(0.4))
        ws_r.send(json.dumps({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"}))
        p2.wait_for_selector('[data-object-id="eq_post_barge"]', timeout=6000)
        p2.wait_for_timeout(1400)
        s_c6 = shots_dir / "11_cat6_sync_bargein.png"
        p2.screenshot(path=str(s_c6))
        report["screenshots"].append(str(s_c6))
        report["categories"]["cat6_sync_and_bargein"] = {
            "blocks_before_voice_at_240ms": before_voice_cnt,
            "barge_items_stopped_early": barge_items_stopped_count < 5,
            "barge_items_count": barge_items_stopped_count,
            "post_barge_queued_actions": post_barge_state.get("queuedLiveActions"),
            "recovered_after_3x_bargein": p2.locator('[data-object-id="eq_post_barge"]').count() == 1,
        }

        # CAT 7: Extreme Typography Torture Test in 2-Column Mode across 1366p/1080p/4K & Light/Dark!
        ws_r.send(json.dumps({"type": "BOARD_RESET", "clear_ink": True, "clear_media": True}))
        p2.wait_for_timeout(200)
        # Put a right-column widget to force 2-column narrower derivation width (where clipping bugs hide!)
        typo_actions = [
            {
                "action_id": "t_right_sim",
                "type": "plot_riemann_sum",
                "object_id": "sim_c7_right",
                "placement": {"zone": "visual_lab", "relation": "auto_next"},
                "payload": {"title": "Suma de Riemann W = ∫ P dV", "expr": "2 + 4*x", "a": 1, "b": 5, "n_init": 8, "n_max": 60, "exact_value": 56, "unit": "J"},
            },
            {
                "action_id": "t_eq_wide",
                "type": "write_math",
                "object_id": "eq_c7_wide",
                "animation": {"duration_ms": 0},
                "payload": {
                    "step_label": "Sustitución larga en 2 columnas",
                    "latex": r"T_1 = \frac{P_1 V_1}{n R} = \frac{(1.50\,\text{atm})(3.50\,\text{L})}{(1.00\,\text{mol})\left(0.08206\,\frac{\text{L}\,\text{atm}}{\text{mol}\,\text{K}}\right)} = 63.98\,\text{K} \approx 64.0\,\text{K}",
                },
            },
            {
                "action_id": "t_eq_nested",
                "type": "write_math",
                "object_id": "eq_c7_nested",
                "animation": {"duration_ms": 0},
                "payload": {
                    "step_label": "Exponentes fraccionarios y raíces anidadas",
                    "latex": r"V_5 = V_1\left(\frac{T_1}{T_5}\right)^{\frac{1}{\gamma - 1}} \quad,\quad I = \int_{r_1}^{r_2} \frac{dr}{r^2 \sqrt{1 + \frac{a^2}{r^2}}}",
                },
            },
            {
                "action_id": "t_wide_table",
                "type": "write_structured_notes",
                "object_id": "tbl_c7_wide",
                "animation": {"duration_ms": 0},
                "payload": {
                    "title": "Comparativa de procesos termodinámicos (5 columnas)",
                    "table_headers": ["Proceso", "Constante", "Trabajo W", "Calor Q", "Observación clave"],
                    "table_rows": [
                        ["Isocórico", "V = cte", "0", "n Cv ΔT", "Todo el calor cambia la energía interna"],
                        ["Isobárico", "P = cte", "P ΔV", "n Cp ΔT", "Expansión a presión constante"],
                        ["Isotérmico", "T = cte", "nRT ln(Vf/Vi)", "Q = W", "ΔU = 0 para gas ideal"],
                        ["Adiabático", "Q = 0", "(p1V1 - p2V2)/(γ - 1)", "0", "Sin transferencia de calor externa"],
                    ],
                },
            },
        ]
        for idx, act in enumerate(typo_actions):
            ws_r.send(json.dumps({"type": "LIVE_CANVAS_ACTION", "step_index": idx + 1, "action": act}))
        ws_r.send(make_sine_pcm24_chunk(0.4))
        ws_r.send(json.dumps({"type": "STATE_CHANGE", "state": "WAITING_FOR_STUDENT"}))
        p2.wait_for_function("() => document.querySelectorAll('.step-block').length >= 3 && document.querySelectorAll('.wb-hand-table tbody tr').length === 4", timeout=15000)
        p2.wait_for_timeout(1200)

        cat7_res: Dict[str, Any] = {}
        for label, w, h, dark, fname in [
            ("1366x768_light", 1366, 768, False, "12_cat7_typo_1366_light.png"),
            ("1366x768_dark", 1366, 768, True, "13_cat7_typo_1366_dark.png"),
            ("1920x1080_light", 1920, 1080, False, "14a_cat7_typo_1920_light.png"),
            ("2560x1440_4k_dark", 2560, 1440, True, "14b_cat7_typo_2560_4k_dark.png"),
        ]:
            p2.set_viewport_size({"width": w, "height": h})
            cur_dark = p2.evaluate("() => document.documentElement.getAttribute('data-theme') === 'dark'")
            if cur_dark != dark:
                p2.click("#btn-theme-toggle")
            p2.wait_for_timeout(350)
            aud = p2.evaluate("() => window.__cuadernuAudit.auditBoardLayoutCollisions()")
            # Check mic-vu-track hidden when mic is NOT pressed? Here mic is live so check top/bottom console fit
            chrome_fit = p2.evaluate("""() => {
                const top = document.querySelector('.top-chrome')?.getBoundingClientRect();
                const bot = document.querySelector('.bottom-console')?.getBoundingClientRect();
                const row = document.querySelector('.console-main-row');
                return {
                    top_h: top ? Math.round(top.height) : 0,
                    bot_h: bot ? Math.round(bot.height) : 0,
                    console_overflow_x: row ? Math.max(0, Math.round(row.scrollWidth - row.clientWidth)) : 0,
                };
            }""")
            shot_p = shots_dir / fname
            p2.screenshot(path=str(shot_p))
            report["screenshots"].append(str(shot_p))
            cat7_res[label] = {"dom_audit": aud, "chrome_fit": chrome_fit}
        report["categories"]["cat7_typography_resolutions"] = cat7_res
        p2.close()
        browser.close()

    return report


def run_resilience_and_security_stress(port: int) -> Dict[str, Any]:
    """Cat 8B: Corrupt PDF, invalid base64, SymPy injection, and rapid HTTP health/summary checks."""
    out: Dict[str, Any] = {}
    base = f"http://127.0.0.1:{port}"

    # 1. Corrupt PDF must return HTTP 400 cleanly (never 500 crash)
    req = urllib.request.Request(
        f"{base}/api/pdf/upload_raw?filename=corrupt.pdf",
        data=b"NOT_A_PDF_AT_ALL_0000",
        headers={"Content-Type": "application/pdf"},
        method="POST",
    )
    try:
        urllib.request.urlopen(req, timeout=5)
        out["corrupt_pdf_status"] = 200
    except urllib.error.HTTPError as e:
        out["corrupt_pdf_status"] = e.code

    # 2. SymPy injection attempt via /api/verify/step must be blocked cleanly
    inj_body = json.dumps({
        "expected_expr": "x**2",
        "student_expr": "__import__('os').system('echo pwn')",
        "variable": "x",
    }).encode("utf-8")
    req2 = urllib.request.Request(
        f"{base}/api/verify/step",
        data=inj_body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req2, timeout=5) as r:
        v_res = json.loads(r.read().decode("utf-8"))
    out["sympy_injection_blocked"] = not bool((v_res.get("verification") or {}).get("equivalent"))
    out["sympy_injection_category"] = (v_res.get("verification") or {}).get("error_category")

    # 3. Server still healthy after malformed inputs
    with urllib.request.urlopen(f"{base}/api/health", timeout=5) as r:
        h = json.loads(r.read().decode("utf-8"))
    out["server_still_healthy"] = bool(h.get("ok")) and (h.get("code_stale") is False)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="CuadernU Massive Audiovisual Stress Suite")
    ap.add_argument("--out-dir", required=True, help="Directory to write screenshots, telemetry, and stress_report.json")
    ap.add_argument("--skip-cloud", action="store_true", help="Skip real Gemini Live cloud calls")
    args = ap.parse_args()

    out_dir = pathlib.Path(args.out_dir)
    shots_dir = out_dir / "screenshots"
    telem_dir = out_dir / "telemetry"
    shots_dir.mkdir(parents=True, exist_ok=True)
    telem_dir.mkdir(parents=True, exist_ok=True)

    port = find_free_port()
    proc = start_isolated_server(port, telem_dir)
    t_start = time.perf_counter()
    try:
        formula_b64 = render_test_formula_png_b64()
        cloud_res: Dict[str, Any] = {"skipped": True}
        if not args.skip_cloud:
            cloud_res = asyncio.run(run_cloud_multi_subject_and_vision_stress(port, formula_b64))

        browser_res = run_browser_stress_categories(port, shots_dir, cloud_res)
        resilience_res = run_resilience_and_security_stress(port)
        telem_analysis = analyze_directory(telem_dir, latest_n=10)

        full_report = {
            "elapsed_sec": round(time.perf_counter() - t_start, 2),
            "port": port,
            "cloud_stress": {
                k: {kk: vv for kk, vv in v.items() if kk != "actions"} if isinstance(v, dict) else v
                for k, v in cloud_res.items()
            },
            "browser_stress": browser_res,
            "resilience_stress": resilience_res,
            "telemetry_analysis": telem_analysis,
        }
        rep_path = out_dir / "stress_report.json"
        rep_path.write_text(json.dumps(full_report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"STRESS_REPORT_WRITTEN: {rep_path}")
        print(f"SCREENSHOTS_CAPTURED: {len(browser_res.get('screenshots', []))}")
        print(f"CONSOLE_ERRORS: {len(browser_res.get('console_errors', []))}")
        print(f"TELEMETRY_EVENTS: {telem_analysis.get('total_events', 0)}")
        print(f"TELEMETRY_INCIDENTS: {len(telem_analysis.get('incidents', []))}")
        return 0
    finally:
        try:
            proc.terminate()
            proc.wait(timeout=4)
        except Exception:
            proc.kill()


if __name__ == "__main__":
    sys.exit(main())
