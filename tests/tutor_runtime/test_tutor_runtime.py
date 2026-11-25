"""Comprehensive automated verification suite for CuadernU Audiovisual Tutor Runtime.

Covers positive paths, negative/failure paths, security/DPAPI redaction, SymPy
neuro-symbolic verification, PyMuPDF PDF RAG + region crop, BKT+FSRS-5 pedagogy
memory, aligned cue parsing, and FastAPI/WebSocket end-to-end integration.
"""

from __future__ import annotations

import base64
import json
import pathlib
import sys
import tempfile
import unittest

import fitz
from fastapi.testclient import TestClient

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.demo_lessons import get_lesson_beats, get_seed_lessons_catalog
from tools.tutor_runtime.gemini_engines import (
    _split_speech_for_parallel_tts,
    align_cues_with_wav_rms,
    ensure_wav_and_duration_ms,
    estimate_cues_fallback,
    normalize_gemini_voice,
    parse_cue_markup,
    rescale_alignment_to_duration,
)
from tools.tutor_runtime.pdf_rag import LocalPdfRagStore, find_phrase_norm_box
from tools.tutor_runtime.pedagogy_memory import PedagogyMemoryStore
from tools.tutor_runtime.secrets_dpapi import dpapi_protect, dpapi_unprotect, sanitize_text
from tools.tutor_runtime.server import create_app, detect_live_clear_intent
from tools.tutor_runtime.sympy_verifier import (
    build_verified_integral_plan,
    check_dimensional_work_pv,
    verify_student_step,
)


class TestSecretsAndSanitization(unittest.TestCase):
    def test_dpapi_round_trip(self) -> None:
        raw = b'{"test_secret":"not-a-real-key-123456"}'
        enc = dpapi_protect(raw)
        self.assertNotIn(b"not-a-real-key", enc)
        self.assertEqual(dpapi_unprotect(enc), raw)

    def test_sanitize_redacts_tokens(self) -> None:
        dirty = "ERR url=https://x/y?token=abcdef123456 key=AQ.Ab8RN6FakeSecretToken999999"
        clean = sanitize_text(dirty)
        self.assertNotIn("abcdef123456", clean)
        self.assertNotIn("FakeSecretToken", clean)
        self.assertIn("[REDACTED]", clean)


class TestSymPyNeuroSymbolicVerifier(unittest.TestCase):
    def test_definite_power_integral_3x2(self) -> None:
        plan = build_verified_integral_plan("3*x^2", "x", "2", "4")
        self.assertTrue(plan.ok)
        self.assertTrue(plan.derivative_check_passed)
        self.assertTrue(plan.is_definite)
        self.assertAlmostEqual(plan.definite_numeric or 0.0, 56.0, places=6)
        self.assertGreaterEqual(len(plan.steps), 3)

    def test_isothermal_work_integral(self) -> None:
        plan = build_verified_integral_plan("n*R*T/V", "V", "V_i", "V_f", physics_context="trabajo isotermico")
        self.assertTrue(plan.ok)
        self.assertTrue(plan.derivative_check_passed)
        self.assertIn("J [OK]", plan.dimensional_note)

    def test_spherical_conduction_inverse_square(self) -> None:
        plan = build_verified_integral_plan("1/r^2", "r", "r_1", "r_2", physics_context="conduccion esferica")
        self.assertTrue(plan.ok)
        self.assertIn("-1/r", plan.antiderivative_expr)
        self.assertIn("W [OK]", plan.dimensional_note)

    def test_u_substitution_integral(self) -> None:
        plan = build_verified_integral_plan("2*x*(x^2 + 1)^3", "x")
        self.assertTrue(plan.ok)
        self.assertTrue(plan.derivative_check_passed)

    def test_negative_security_blocks_code_injection(self) -> None:
        plan = build_verified_integral_plan("__import__('os').system('echo pwned')", "x")
        self.assertFalse(plan.ok)
        self.assertIn("seguridad", plan.error.lower())

    def test_student_error_classification(self) -> None:
        # Correct
        ok = verify_student_step("-1/r", "-1/r", var_name="r")
        self.assertTrue(ok["equivalent"])
        self.assertEqual(ok["error_category"], "None")

        # SignError (classic physics III spherical conduction error)
        sign_err = verify_student_step("-1/r", "1/r", var_name="r")
        self.assertFalse(sign_err["equivalent"])
        self.assertEqual(sign_err["error_category"], "SignError")

        # PowerRuleOffset (differentiated instead of integrated)
        pow_err = verify_student_step("x^3", "6*x", var_name="x", integrand_text="3*x^2")
        self.assertFalse(pow_err["equivalent"])
        self.assertEqual(pow_err["error_category"], "PowerRuleOffset")

        # MissingConstantFactor
        const_err = verify_student_step("x^3/3", "x^3", var_name="x")
        self.assertFalse(const_err["equivalent"])
        self.assertEqual(const_err["error_category"], "MissingConstantFactor")

        # SyntaxError
        syn_err = verify_student_step("x^3", "(((+", var_name="x")
        self.assertFalse(syn_err["equivalent"])
        self.assertEqual(syn_err["error_category"], "SyntaxError")

    def test_dimensional_work_check(self) -> None:
        self.assertTrue(check_dimensional_work_pv("Pa", "m3")["ok"])
        bad = check_dimensional_work_pv("kPa", "cm3")
        self.assertFalse(bad["ok"])
        self.assertEqual(bad["error_category"], "DimensionalMismatch")


class TestPdfRagAndRegionCrop(unittest.TestCase):
    def _make_sample_pdf_bytes(self) -> bytes:
        doc = fitz.open()
        p1 = doc.new_page(width=595, height=842)
        p1.insert_text((72, 90), "Formulario Fisica General III TEC", fontsize=16)
        p1.insert_text((72, 140), "Trabajo termodinamico: W = integral P(V) dV", fontsize=12)
        p1.insert_text((72, 180), "Proceso isotermico gas ideal: PV = nRT", fontsize=12)
        data = doc.tobytes()
        doc.close()
        return data

    def test_index_render_search_and_crop(self) -> None:
        store = LocalPdfRagStore(":memory:")
        pdf_bytes = self._make_sample_pdf_bytes()
        meta = store.index_pdf_bytes("Formulario_F3.pdf", pdf_bytes)
        self.assertEqual(meta.page_count, 1)

        page_png = store.render_page_png(meta.doc_id, 1)
        self.assertGreater(page_png["width"], 400)
        self.assertTrue(base64.b64decode(page_png["png_b64"]).startswith(b"\x89PNG"))

        hits = store.search("isotermico gas ideal")
        self.assertGreaterEqual(len(hits), 1)
        self.assertEqual(hits[0]["page"], 1)

        region = store.extract_region_text_and_crop(meta.doc_id, 1, {"x": 0.05, "y": 0.12, "w": 0.85, "h": 0.16})
        self.assertIn("Trabajo", region["region_text"])
        self.assertTrue(base64.b64decode(region["crop_png_b64"]).startswith(b"\x89PNG"))
        store.close()

    def test_rejects_corrupt_or_empty_pdf(self) -> None:
        store = LocalPdfRagStore(":memory:")
        with self.assertRaises(ValueError):
            store.index_pdf_bytes("empty.pdf", b"")
        with self.assertRaises(ValueError):
            store.index_pdf_bytes("corrupt.pdf", b"NOT_A_REAL_PDF_HEADER")
        store.close()

    def test_problem_segmentation_crop_search_and_duplicate_audit(self) -> None:
        doc = fitz.open()
        # Problem Q057 and near-duplicate Q060 + distinct Q122
        samples = [
            (
                "Q057\nNivel 4/5\nFamilia: Transferencia de calor Tema: 5.4\n"
                "Subtema: Conducción y flujo estacionario\n"
                "Procedimiento: Ley de Fourier, resistencias en serie y geometría\n"
                "Considere una esfera hueca de radio interno 0.150 m y radio externo 0.600 m con conductividad k = k0 r. "
                "La parte interna esta a 20 C y la externa a 100 C. Calcule el flujo calorico radial."
            ),
            (
                "Q060\nNivel 4/5\nFamilia: Transferencia de calor Tema: 5.4\n"
                "Subtema: Conducción y flujo estacionario\n"
                "Procedimiento: Ley de Fourier, resistencias en serie y geometría\n"
                "Considere una esfera hueca de radio interno 0.200 m y radio externo 0.800 m con conductividad k = k0 r. "
                "La parte interna esta a 30 C y la externa a 120 C. Calcule el flujo calorico radial."
            ),
            (
                "Q122\nNivel 4/5\nFamilia: Termodinámica Tema: 6.14\n"
                "Subtema: Máquinas, refrigeradores y bombas\n"
                "Procedimiento: Rendimiento y coeficiente de desempeño\n"
                "Se diseno un ciclo termodinamico con 0.300 mol de gas ideal: compresion isobarica, expansion adiabatica y calentamiento isocorico."
            ),
        ]
        for txt in samples:
            pg = doc.new_page(width=595, height=842)
            y = 70
            for ln in txt.splitlines():
                pg.insert_text((60, y), ln, fontsize=11)
                y += 18
        pdf_bytes = doc.tobytes()
        doc.close()

        store = LocalPdfRagStore(":memory:")
        meta = store.index_pdf_bytes("Mock_Superexamen.pdf", pdf_bytes)
        self.assertEqual(meta.problem_count, 3)

        p57 = store.get_problem_by_id("q57", meta.doc_id)
        self.assertIsNotNone(p57)
        self.assertEqual(p57["problem_id"], "Q057")

        crop = store.crop_problem_visual(meta.doc_id, "q057")
        self.assertTrue(base64.b64decode(crop["problem_png_b64"]).startswith(b"\x89PNG"))
        self.assertGreater(crop["width"], 200)

        hits = store.search_problems("ciclo termodinamico gas ideal", meta.doc_id)
        self.assertGreaterEqual(len(hits), 1)
        self.assertEqual(hits[0]["problem_id"], "Q122")

        audit = store.audit_repeated_problems(meta.doc_id)
        self.assertTrue(audit["ok"])
        self.assertEqual(audit["total_problems"], 3)
        self.assertEqual(audit["top_archetypes"][0]["count"], 2)
        self.assertGreaterEqual(len(audit["near_duplicate_clusters"]), 1)
        self.assertEqual(set(audit["near_duplicate_clusters"][0]["problem_ids"]), {"Q057", "Q060"})
        store.close()


class TestPedagogyMemoryBktFsrs(unittest.TestCase):
    def test_bkt_convergence_fading_scaffold_and_misconceptions(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            db_path = pathlib.Path(td) / "mem.sqlite"
            mem = PedagogyMemoryStore(db_path)
            r1 = mem.record_practice_outcome("calc.int.inv_sq", "Integral 1/r^2", False, now_sec=1000)
            self.assertEqual(r1["scaffold_level"], 3)
            m1 = mem.log_misconception("calc.int.inv_sq", "SignError", "1/r", "-1/r", "Signo invertido", now_sec=1001)
            self.assertFalse(m1["needs_remediation"])
            m2 = mem.log_misconception("calc.int.inv_sq", "SignError", "1/x", "-1/x", "Signo invertido otra vez", now_sec=1002)
            self.assertTrue(m2["needs_remediation"])

            last = r1
            for k in range(5):
                last = mem.record_practice_outcome("calc.int.inv_sq", "Integral 1/r^2", True, now_sec=2000 + k * 100)
            self.assertGreater(last["p_know"], 0.85)
            self.assertLessEqual(last["scaffold_level"], 1)
            self.assertGreater(last["fsrs_stability_days"], 1.5)
            mem.close()

            # Verify persistence after reopen
            mem2 = PedagogyMemoryStore(db_path)
            summary = mem2.build_pedagogical_summary()
            self.assertEqual(len(summary["skills"]), 1)
            self.assertEqual(summary["active_misconceptions"][0]["occurrences"], 2)
            mem2.close()


class TestCueParsingAndFastApiWs(unittest.TestCase):
    def test_cue_markup_parsing_and_fallback_timing(self) -> None:
        clean, cues = parse_cue_markup("Primero <cue id='c1'/> sacamos 3 y <cue id='c2'/> integramos.")
        self.assertEqual(clean, "Primero sacamos 3 y integramos.")
        self.assertEqual([c[0] for c in cues], ["c1", "c2"])
        fb = estimate_cues_fallback(clean, cues, 0.96)
        self.assertLess(fb["cues_ms"]["c1"], fb["cues_ms"]["c2"])
        self.assertGreater(fb["duration_ms"], 500)

        # Verify raw 24 kHz PCM16 -> RIFF/WAVE wrapping and exact duration scaling
        one_sec_pcm24 = b"\x00\x00" * 24000
        wav_bytes, dur_ms = ensure_wav_and_duration_ms(one_sec_pcm24, "audio/L16;codec=pcm;rate=24000")
        self.assertTrue(wav_bytes.startswith(b"RIFF"))
        self.assertEqual(dur_ms, 1000)
        wav_bytes2, dur_ms2 = ensure_wav_and_duration_ms(wav_bytes, "audio/wav")
        self.assertEqual(dur_ms2, 1000)
        self.assertEqual(wav_bytes2, wav_bytes)

        scaled = rescale_alignment_to_duration(fb, 4000)
        self.assertEqual(scaled["duration_ms"], 4000)
        self.assertLess(scaled["cues_ms"]["c1"], scaled["cues_ms"]["c2"])
        self.assertEqual(normalize_gemini_voice("es-CR-MariaNeural"), "Kore")
        self.assertEqual(normalize_gemini_voice("Aoede"), "Aoede")

    def test_seed_lessons_schema_integrity(self) -> None:
        cat = get_seed_lessons_catalog()
        self.assertEqual(len(cat), 3)
        for item in cat:
            beats = get_lesson_beats(item["id"], "ses_unit_01")
            self.assertGreaterEqual(len(beats), 2)
            for b in beats:
                self.assertEqual(b["protocol"], "cuadernu.tutor.v1.1")
                self.assertGreaterEqual(len(b["actions"]), 1)

    def test_fastapi_http_and_ws_flow_with_token_auth(self) -> None:
        app = create_app({"session_token": "tok_secret_unit_12345"})
        client = TestClient(app)

        # Negative auth check
        self.assertEqual(client.get("/api/health").status_code, 401)

        # Positive health check
        h = client.get("/api/health?token=tok_secret_unit_12345")
        self.assertEqual(h.status_code, 200)
        body = h.json()
        self.assertTrue(body["ok"])
        self.assertEqual(body["protocol"], "cuadernu.tutor.v1.1")

        # Verify step endpoint
        vs = client.post(
            "/api/verify/step?token=tok_secret_unit_12345",
            json={"expected_expr": "-1/r", "student_expr": "1/r", "variable": "r"},
        )
        self.assertEqual(vs.status_code, 200)
        self.assertEqual(vs.json()["verification"]["error_category"], "SignError")

        # WebSocket session: silent mode load lesson + interrupt + verify
        with client.websocket_connect("/ws/tutor?token=tok_secret_unit_12345") as ws:
            ready = json.loads(ws.receive_text())
            self.assertEqual(ready["type"], "SESSION_READY")

            ws.send_text(json.dumps({"type": "UPDATE_SETTINGS", "speech_enabled": False}))
            upd = json.loads(ws.receive_text())
            self.assertEqual(upd["type"], "SETTINGS_UPDATED")

            ws.send_text(json.dumps({"type": "LOAD_LESSON", "lesson_id": "fis3_conduccion_esferica"}))
            reset_msg = json.loads(ws.receive_text())
            self.assertEqual(reset_msg["type"], "BOARD_RESET")
            beat_msg = json.loads(ws.receive_text())
            self.assertEqual(beat_msg["type"], "BEAT_PLAN")
            align_msg = json.loads(ws.receive_text())
            self.assertEqual(align_msg["type"], "AUDIO_ALIGNMENT")
            self.assertIn("c1", align_msg["cues_ms"])

            ws.send_text(json.dumps({"type": "CLIENT_INTERRUPT", "beat_id": beat_msg["beat"]["beat_id"], "last_word_index": 3}))
            int_msg = json.loads(ws.receive_text())
            self.assertEqual(int_msg["type"], "STATE_CHANGE")
            self.assertEqual(int_msg["state"], "INTERRUPTED")

            # Student lasso selection over board triggers immediate LASSO_VISION_ACK
            ws.send_text(json.dumps({
                "type": "USER_LASSO_SELECTION",
                "circled_text": "1.50 atm",
                "object_id": "eq_test_1",
                "image_b64": "",
            }))
            ack_msg = json.loads(ws.receive_text())
            self.assertEqual(ack_msg["type"], "LASSO_VISION_ACK")
            self.assertEqual(ack_msg["circled_text"], "1.50 atm")
            self.assertEqual(ack_msg["object_id"], "eq_test_1")


class TestAcousticRmsSyncAndPdfWordBoxes(unittest.TestCase):
    def _build_three_clause_wav(self, sr: int = 24000) -> bytes:
        """Build synthetic 24 kHz WAV with speech bursts separated by exact silence valleys:
        Clause 0: 100..900 ms
        Silence 1: 900..1200 ms -> Clause 1 starts at 1200 ms
        Clause 1: 1200..2500 ms
        Silence 2: 2500..2800 ms -> Clause 2 starts at 2800 ms
        Clause 2: 2800..3800 ms
        """
        import array
        import math

        total_ms = 4000
        n = int(sr * total_ms / 1000)
        samples = array.array("h", [0] * n)

        def fill_tone(start_ms: int, end_ms: int, freq: float = 340.0) -> None:
            i0 = int(sr * start_ms / 1000)
            i1 = int(sr * end_ms / 1000)
            for i in range(i0, min(n, i1)):
                samples[i] = int(9500 * math.sin(2.0 * math.pi * freq * i / sr))

        fill_tone(100, 900, 320.0)
        fill_tone(1200, 2500, 410.0)
        fill_tone(2800, 3800, 360.0)
        wav_bytes, _ = ensure_wav_and_duration_ms(samples.tobytes(), f"audio/L16;codec=pcm;rate={sr}")
        return wav_bytes

    def test_align_cues_with_wav_rms_snaps_within_40ms(self) -> None:
        markup = (
            "<cue id='c0'/> Primero miramos la presión inicial. "
            "<cue id='c1'/> Ahora despejamos la temperatura en la fracción. "
            "<cue id='c2'/> Y cancelamos las unidades repetidas."
        )
        clean, cues = parse_cue_markup(markup)
        fb = estimate_cues_fallback(clean, cues, 0.96)
        wav_bytes = self._build_three_clause_wav()
        aligned = align_cues_with_wav_rms(wav_bytes, clean, cues, fb)
        self.assertLessEqual(abs(aligned["cues_ms"]["c0"] - 100), 40)
        self.assertLessEqual(abs(aligned["cues_ms"]["c1"] - 1200), 40)
        self.assertLessEqual(abs(aligned["cues_ms"]["c2"] - 2800), 40)
        self.assertGreater(len(aligned["words"]), 10)

    def test_parallel_tts_split_balances_multi_clause_markup(self) -> None:
        markup = (
            "<cue id='c1'/> Primero tomamos la presión inicial de uno punto cinco atmósferas del enunciado. "
            "<cue id='c2'/> Luego sustituimos en la ley del gas ideal y cancelamos las unidades paso a paso."
        )
        segs = _split_speech_for_parallel_tts(markup)
        self.assertIsNotNone(segs)
        left, right = segs
        self.assertIn("c1", left)
        self.assertTrue(right.startswith("<cue id='c2'/>"))

    def test_pdf_crop_words_and_exact_phrase_norm_box_q111(self) -> None:
        doc = fitz.open()
        pg = doc.new_page(width=595, height=842)
        lines = [
            "Q111                                             Nivel 4/5",
            "Familia: Termodinamica Tema: 6.11",
            "Se tiene un mol de un gas ideal diatomico a una presion de 1.50 atm y un volumen de 3.50 L.",
            "El gas experimenta un proceso isocorico hasta 3.00 atm y luego a 256 K.",
            "(a) Calcular (p, V, T) para cada estado.",
            "(b) Calcular Q, W y cambio de energia interna.",
        ]
        y = 42  # intentionally close to top to verify Q111 header is NEVER clipped
        for ln in lines:
            pg.insert_text((56, y), ln, fontsize=11)
            y += 18
        pdf_bytes = doc.tobytes()
        doc.close()

        store = LocalPdfRagStore(":memory:")
        meta = store.index_pdf_bytes("Q111_Mock.pdf", pdf_bytes)
        crop = store.crop_problem_visual(meta.doc_id, "Q111")
        words = crop.get("crop_words") or []
        self.assertGreater(len(words), 20)
        # Q111 header itself must be inside crop_words (not clipped off top!)
        self.assertTrue(any(w["t"] == "Q111" for w in words))

        box_atm = find_phrase_norm_box("1.50 atm", words)
        self.assertIsNotNone(box_atm)
        self.assertIn("1.50", box_atm["matched_text"])
        self.assertIn("atm", box_atm["matched_text"])
        self.assertGreater(box_atm["w"], 0.02)
        self.assertGreater(box_atm["h"], 0.01)

        # Numeric equivalence '1.5 atm' -> '1.50 atm'
        box_atm_short = find_phrase_norm_box("1.5 atm", words)
        self.assertIsNotNone(box_atm_short)
        self.assertAlmostEqual(box_atm_short["x"], box_atm["x"], places=3)

        box_vol = find_phrase_norm_box("3.50 L", words)
        self.assertIsNotNone(box_vol)
        self.assertIn("3.50", box_vol["matched_text"])

        box_dia = find_phrase_norm_box("diatómico", words)
        self.assertIsNotNone(box_dia)
        store.close()

    def test_live_clear_board_guardrail_catches_asr_and_model_claims(self) -> None:
        # Positive ASR commands
        self.assertTrue(detect_live_clear_intent("ASR_FINAL", "por favor limpia la pizarra"))
        self.assertTrue(detect_live_clear_intent("ASR_FINAL", "borra todo de la pizarra"))
        self.assertTrue(detect_live_clear_intent("ASR_FINAL", "deja la pizarra en blanco"))
        # Positive model hallucination claims
        self.assertTrue(detect_live_clear_intent("LIVE_OUTPUT_TRANSCRIPT", "Listo, ya limpié la pizarra. ¿Qué vemos?"))
        self.assertTrue(detect_live_clear_intent("LIVE_OUTPUT_TRANSCRIPT", "Ya está limpia la pizarra."))
        self.assertTrue(detect_live_clear_intent("LIVE_OUTPUT_TRANSCRIPT", "Ya he borrado todo de la pizarra."))
        # Negative normal physics speech (must NOT trigger accidental clear!)
        self.assertFalse(detect_live_clear_intent("ASR_FINAL", "de donde salio ese 1.50 atm"))
        self.assertFalse(detect_live_clear_intent("LIVE_OUTPUT_TRANSCRIPT", "Ese valor de gamma viene de que el gas es diatómico."))

    def test_health_exposes_runtime_build_and_not_stale(self) -> None:
        from fastapi.testclient import TestClient
        from tools.tutor_runtime.server import RUNTIME_BUILD, create_app

        app = create_app({"session_token": ""})
        client = TestClient(app)
        resp = client.get("/api/health")
        self.assertEqual(resp.status_code, 200)
        body = resp.json()
        self.assertEqual(body.get("runtime_build"), RUNTIME_BUILD)
        self.assertFalse(body.get("code_stale"))
        caps = body.get("capabilities") or []
        for req in ("universal_notes", "custom_diagram", "pasted_image_live_vision", "speech_locked_handwriting", "local_telemetry_flight_recorder", "clear_guardrail"):
            self.assertIn(req, caps)

    def test_pasted_image_ws_forwards_to_live_vision_bundle(self) -> None:
        import base64
        from unittest.mock import AsyncMock, patch
        from fastapi.testclient import TestClient
        from tools.tutor_runtime.server import create_app

        # 1x1 white PNG
        tiny_png = base64.b64encode(
            b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde"
            b"\x00\x00\x00\x0cIDATx\x9cc\xf8\xff\xff?\x00\x05\xfe\x02\xfeA\x89\x9a\x18\x00\x00\x00\x00IEND\xaeB`\x82"
        ).decode("ascii")

        app = create_app({"session_token": ""})
        client = TestClient(app)
        mock_bundle = AsyncMock()
        with patch("tools.tutor_runtime.gemini_engines.GeminiLiveVoiceBridge.is_running", new_callable=lambda: property(lambda self: True)), \
             patch("tools.tutor_runtime.gemini_engines.GeminiLiveVoiceBridge.send_visual_context_bundle", mock_bundle):
            with client.websocket_connect("/ws/tutor") as ws:
                ready = ws.receive_json()
                self.assertEqual(ready.get("type"), "SESSION_READY")
                ws.send_json({
                    "type": "SOLVE_PASTED_IMAGE",
                    "image_b64": tiny_png,
                    "mime_type": "image/png",
                    "caption": "Formula adiabatica",
                    "auto_solve": True,
                })
                ack = ws.receive_json()
                self.assertEqual(ack.get("type"), "PASTED_IMAGE_VISION_ACK")
                self.assertTrue(ack.get("sent_to_live"))
                self.assertTrue(ack.get("auto_solve"))
        self.assertEqual(mock_bundle.await_count, 1)
        args, kwargs = mock_bundle.await_args
        self.assertEqual(len(args[0]), 1)
        self.assertIn("IMAGEN PEGADA POR EL ESTUDIANTE", args[1])
        self.assertTrue(kwargs.get("turn_complete"))

    def test_flight_recorder_telemetry_redaction_incidents_and_analyzer(self) -> None:
        import base64
        import tempfile
        from fastapi.testclient import TestClient
        from tools.tutor_runtime.analyze_telemetry import analyze_directory
        from tools.tutor_runtime.server import create_app
        from tools.tutor_runtime.telemetry_recorder import (
            detect_raw_latex_leak,
            detect_tutor_antipattern,
            detect_user_correction,
        )

        # Positive & negative pattern checks
        self.assertIsNotNone(detect_user_correction("mae no ves lo que encerré en rojo"))
        self.assertIsNotNone(detect_user_correction("escribe muy rápido y no se sincroniza"))
        self.assertIsNone(detect_user_correction("excelente, sigamos con el paso 2"))
        self.assertIsNotNone(detect_tutor_antipattern("Por limitaciones técnicas no puedo plasmar mapas"))
        self.assertIsNone(detect_tutor_antipattern("Vamos anotando los pueblos indígenas en la pizarra"))
        self.assertEqual(detect_raw_latex_leak(r"El resultado es \frac{1}{2}"), r"\frac")
        self.assertIsNone(detect_raw_latex_leak("El resultado es 1/2 en atmósferas"))

        with tempfile.TemporaryDirectory() as tmp:
            tdir = pathlib.Path(tmp) / "telemetry"
            app = create_app({"session_token": "", "telemetry_dir": str(tdir), "telemetry_enabled": True})
            client = TestClient(app)

            tiny_jpg = base64.b64encode(b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xd9").decode("ascii")
            with client.websocket_connect("/ws/tutor") as ws:
                ready = ws.receive_json()
                self.assertEqual(ready.get("type"), "SESSION_READY")
                ws.send_json({
                    "type": "CLIENT_TELEMETRY_EVENT",
                    "kind": "sync_metrics",
                    "data": {"onset_to_write_ms": 55, "write_duration_ms": 2100, "api_key": "AIzaSyFakeSecret12345678901234567890"},
                })
                ws.send_json({
                    "type": "USER_REPORT_INCIDENT",
                    "reason": "manual_hotkey_mark",
                    "note": "Prueba de fallo",
                    "image_b64": tiny_jpg,
                    "mime_type": "image/jpeg",
                    "dom_audit": {"overlaps": [], "raw_latex_leaks": []},
                })
                saved = ws.receive_json()
                self.assertEqual(saved.get("type"), "INCIDENT_SAVED")
                self.assertTrue(saved.get("ok"))
                self.assertGreater(saved.get("snapshot_bytes", 0), 0)

            summary = client.get("/api/telemetry/summary").json()
            self.assertTrue(summary.get("enabled"))
            self.assertGreaterEqual(summary.get("stored_snapshots", 0), 1)

            # Analyzer reads directory cleanly and secrets are strictly redacted
            report = analyze_directory(tdir, latest_n=5)
            self.assertTrue(report.get("ok"))
            self.assertEqual(report["sync_latency"]["samples"], 1)
            self.assertEqual(len(report["incidents"]), 1)
            raw_jsonl = "".join(p.read_text(encoding="utf-8") for p in tdir.glob("session_*.jsonl"))
            self.assertNotIn("AIzaSyFakeSecret", raw_jsonl)

    def test_write_whiteboard_notes_normalizes_string_dict_and_list_table_rows(self) -> None:
        from unittest.mock import AsyncMock, patch
        from fastapi.testclient import TestClient
        from tools.tutor_runtime.server import create_app

        app = create_app({"session_token": "", "telemetry_enabled": False})
        client = TestClient(app)

        captured_bridge = {}

        async def fake_start(self_bridge, board_context_summary=""):
            captured_bridge["bridge"] = self_bridge
            self_bridge._running = True
            return {"ok": True, "model": "mock-live"}

        with patch("tools.tutor_runtime.gemini_engines.GeminiLiveVoiceBridge.start", fake_start), \
             patch("tools.tutor_runtime.gemini_engines.GeminiLiveVoiceBridge.stop", AsyncMock()):
            with client.websocket_connect("/ws/tutor") as ws:
                _ = ws.receive_json()
                ws.send_json({"type": "START_LIVE_VOICE"})
                st = ws.receive_json()
                self.assertTrue(st.get("ok"))

                # Trigger write_whiteboard_notes with pipe-delimited string table_rows (exact Gemini 3.8 Live pattern!)
                import asyncio
                bridge = captured_bridge["bridge"]
                loop = asyncio.new_event_loop()
                try:
                    res = loop.run_until_complete(
                        bridge._on_tool_beat({
                            "tool_name": "write_whiteboard_notes",
                            "args": {
                                "title": "Fases de la fotosíntesis",
                                "table_headers": ["Fase", "Lugar", "Luz", "Produce"],
                                "table_rows": [
                                    "Fase luminosa | Tilacoides | Sí | ATP, NADPH, O2",
                                    ["Fase oscura", "Estroma", "No", "Glucosa"],
                                ],
                            },
                        })
                    )
                finally:
                    loop.close()
                self.assertTrue(res.get("ok"))
                self.assertEqual(res.get("table_rows"), 2)
                action_msg = ws.receive_json()
                self.assertEqual(action_msg.get("type"), "LIVE_CANVAS_ACTION")
                rows = action_msg["action"]["payload"]["table_rows"]
                self.assertEqual(len(rows), 2)
                self.assertEqual(rows[0], ["Fase luminosa", "Tilacoides", "Sí", "ATP, NADPH, O2"])
                self.assertEqual(rows[1], ["Fase oscura", "Estroma", "No", "Glucosa"])


if __name__ == "__main__":
    unittest.main()
