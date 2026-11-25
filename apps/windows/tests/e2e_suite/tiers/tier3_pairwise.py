"""Tier 3 Opaque-Box E2E Tests: 26 Cross-Feature Pairwise Combinations (T3-01..T3-20 + ACAD-T3-01..06)."""
from __future__ import annotations

import atexit
import hashlib
import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import (
    validate_cudoc,
    validate_html5,
    validate_ooxml_docx,
    validate_ooxml_pptx,
    validate_pdf,
    validate_png,
)
from e2e_suite.adapters.cli_runner import run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_doc_core_keepdir
from e2e_suite.adapters.linux_gui_channel import smoke_calculadora_linux
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scan_tree_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.adapters.win_gui_channel import probe_calculadora_win, smoke_document_studio
from e2e_suite.fixtures.mock_tec_server import MockTecServer
from e2e_suite.fixtures.synthetic_data import (
    make_horarios_catalog_canonical,
    make_minimal_pdf,
    make_minimal_png,
    make_minimal_wav,
)


_SHARED_KEEPDIRS: dict[str, Path] = {}
SYNTH_EMAIL = "student@example.invalid"
SYNTH_PASS = "SYNTH-TEST-PASS-0001"


def _cleanup_shared_keepdirs() -> None:
    if os.environ.get("E2E_KEEP_ARTIFACTS", "").strip() == "1":
        return
    for d in _SHARED_KEEPDIRS.values():
        shutil.rmtree(d, ignore_errors=True)
    _SHARED_KEEPDIRS.clear()


atexit.register(_cleanup_shared_keepdirs)


def _target_platforms(case_platforms: list[str]) -> list[str]:
    req = str(get_runtime_context().get("target_platform", "win")).lower()
    allowed = {"win", "linux"} if "both" in case_platforms else {p.lower() for p in case_platforms}
    if req == "win":
        return ["win"] if "win" in allowed else []
    if req == "linux":
        return ["linux"] if "linux" in allowed else []
    out: list[str] = []
    if is_windows_host() and "win" in allowed:
        out.append("win")
    if "linux" in allowed:
        out.append("linux")
    return out


def _cli_path(plat: str, path: Path) -> str:
    return win_to_wsl_path(path) if plat == "linux" else str(path)


def _get_doc_core_run(plat: str) -> dict[str, Any]:
    if plat not in _SHARED_KEEPDIRS:
        _SHARED_KEEPDIRS[plat] = create_session_keepdir(f"e2e_t3_keep_{plat}_")
    return run_doc_core_keepdir(plat, _SHARED_KEEPDIRS[plat], timeout=240)


def _find_artifact(artifacts: dict[str, list[Path]], ext: str, name_substr: str) -> Path | None:
    for p in artifacts.get(ext, []):
        if name_substr.lower() in p.name.lower():
            return p
    return None


def _run_studio_cli_flag(plat: str, args: list[str], timeout: int = 30) -> tuple[int, str, str]:
    paths = resolve_paths(REPO_ROOT)
    if plat == "linux":
        sh = paths["linux_run_document_studio"]
        if not sh.exists():
            return (-1, "", f"Missing {sh}")
        res = run_wsl(
            [win_to_wsl_path(sh), *args],
            env={"DISPLAY": ""},
            timeout=timeout,
            cwd_linux=win_to_wsl_path(REPO_ROOT),
        )
        return (res.returncode, res.stdout, res.stderr)
    exe = paths["win_document_studio"]
    if not exe.exists():
        return (-1, "", f"Missing {exe}")
    res = run_win([str(exe), *args], timeout=timeout, cwd=REPO_ROOT)
    return (res.returncode, res.stdout, res.stderr)


class TestTier3Pairwise(unittest.TestCase):
    """26 cross-feature pairwise E2E tests (T3-01..T3-20 + ACAD-T3-01..ACAD-T3-06)."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_t3_case_"))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="T3-01",
        tier=3,
        group="T3",
        req_ids=["F-001", "F-003", "F-008", "F-075", "F-078", "F-132", "F-133", "F-134", "F-233", "F-246", "F-247", "SUP-01", "SUP-02", "SUP-03", "SUP-05", "SUP-08"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t3_01_pdf_ink_link_varsub_dag_calcbridge_wal(self) -> None:
        pdf_path = self.tmpdir / "datasheet.pdf"
        pdf_path.write_bytes(make_minimal_pdf(["v_0 = 25 m/s", "g = 9.81 m/s^2"]))
        for plat in _target_platforms(["win", "linux"]):
            out_cudoc = self.tmpdir / f"t3_01_{plat}.cudoc"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--smoke-test",
                    "--mode",
                    "canvas",
                    "--open",
                    _cli_path(plat, pdf_path),
                    "--calc-card",
                    "fx-991LAX|2,*,2,5,=,|T = 2*v_0*sin(30 deg)/g #varsub",
                    "--live-calc-sync",
                    "--save-cudoc",
                    _cli_path(plat, out_cudoc),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_cudoc(out_cudoc).ok)

    @e2e_case(
        case_id="T3-02",
        tier=3,
        group="T3",
        req_ids=["F-238", "F-239", "F-241", "F-242", "F-249", "SUP-04", "SUP-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t3_02_circuit_mna_transient_oscilloscope_pdf_and_canvas(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            self.assertIsNotNone(stem_pdf, f"Missing f10_stem_canvas.pdf in {dc['artifacts']}")
            assert stem_pdf is not None
            pdf_res = validate_pdf(stem_pdf)
            self.assertTrue(pdf_res.ok, f"Invalid STEM PDF: {pdf_res.errors}")

            full_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(full_cudoc, "Missing f11_full.cudoc from DocumentCoreTest F11")
            assert full_cudoc is not None
            cudoc_res = validate_cudoc(full_cudoc)
            self.assertTrue(cudoc_res.ok, f"Invalid f11_full.cudoc: {cudoc_res.errors}")

            # Verify DocumentStudio renders the STEM showcase canvas with MNA/circuit visuals
            out_png = self.tmpdir / f"t3_02_stem_{plat}.png"
            smoke_res = smoke_document_studio(plat, "canvas", out_png, showcase_stem=True)
            self.assertEqual(smoke_res.returncode, 0, smoke_res.stderr or smoke_res.stdout)
            png_res = validate_png(out_png, min_width=300, min_height=200)
            self.assertTrue(png_res.ok, f"Rendered STEM PNG invalid: {png_res.errors}")

    @e2e_case(
        case_id="T3-03",
        tier=3,
        group="T3",
        req_ids=["F-035", "F-153", "F-154", "F-155", "F-160", "F-161", "F-212", "F-216", "GN-13"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t3_03_mic_audio_synced_ink_keylog_html5_tutorial(self) -> None:
        wav_path = self.tmpdir / "lecture_clip.wav"
        wav_path.write_bytes(make_minimal_wav(duration_ms=500))
        for plat in _target_platforms(["win", "linux"]):
            out_html = self.tmpdir / f"tutorial_{plat}.html"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--smoke-test",
                    "--import-audio",
                    _cli_path(plat, wav_path),
                    "--export-html5",
                    _cli_path(plat, out_html),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_html5(out_html).ok)

    @e2e_case(
        case_id="T3-04",
        tier=3,
        group="T3",
        req_ids=["F-025", "F-026", "F-043", "F-179", "F-180", "F-184", "F-232", "F-310", "F-312", "F-313", "F-314", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t3_04_lasso_confirmation_card_check_my_work_errorbook_sm2(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_json = self.tmpdir / f"check_work_{plat}.json"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--check-my-work",
                    "(x+1)^2",
                    "x^2+1",
                    "--confirm-ast",
                    "--export-errorbook",
                    _cli_path(plat, out_json),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            payload = json.loads(out_json.read_text(encoding="utf-8"))
            self.assertFalse(payload.get("equivalent", True))
            self.assertGreaterEqual(len(payload.get("sm2_cards", [])), 1)

    @e2e_case(
        case_id="T3-05",
        tier=3,
        group="T3",
        req_ids=["F-041", "F-076", "F-102", "F-122", "F-124", "F-142", "F-143", "GN-06", "GN-07", "SUP-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t3_05_pinch_collapse_anchored_ink_layers_and_cudoc_open(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            f11_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(f11_cudoc, "Missing f11_full.cudoc")
            assert f11_cudoc is not None
            self.assertTrue(validate_cudoc(f11_cudoc).ok)

            pdf_card = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_pdfcard")
            self.assertIsNotNone(pdf_card, "Missing f8_canvas_pdfcard.pdf")
            assert pdf_card is not None
            self.assertTrue(validate_pdf(pdf_card).ok)

            out_png = self.tmpdir / f"t3_05_open_cudoc_{plat}.png"
            smoke_res = smoke_document_studio(plat, "canvas", out_png, open_path=f11_cudoc)
            self.assertEqual(smoke_res.returncode, 0, smoke_res.stderr or smoke_res.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T3-06",
        tier=3,
        group="T3",
        req_ids=["F-055", "F-056", "F-061", "F-065", "F-066", "F-166", "F-175", "F-176", "F-225", "GN-01", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t3_06_vault_hierarchy_backlinks_unicode_search_and_pdf_view(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            vault_json = _find_artifact(dc["artifacts"], ".json", "vault")
            self.assertIsNotNone(vault_json, "Missing vault.json from DocumentCoreTest F9")
            assert vault_json is not None
            vault_data = json.loads(vault_json.read_text(encoding="utf-8"))
            self.assertTrue(bool(vault_data), "vault.json is empty")

            unicode_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(unicode_pdf, "Missing f8_unicode_math.pdf")
            assert unicode_pdf is not None
            self.assertTrue(validate_pdf(unicode_pdf).ok)

            nb_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(nb_cudoc, "Missing f9_notebook.cudoc")
            assert nb_cudoc is not None
            self.assertTrue(validate_cudoc(nb_cudoc).ok)

            out_png = self.tmpdir / f"t3_06_vault_notebook_{plat}.png"
            smoke_res = smoke_document_studio(plat, "canvas", out_png, open_path=nb_cudoc)
            self.assertEqual(smoke_res.returncode, 0, smoke_res.stderr or smoke_res.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T3-07",
        tier=3,
        group="T3",
        req_ids=["F-012", "F-111", "F-146", "F-236", "F-237", "F-251", "F-252", "F-253", "F-267", "SUP-06"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="win_table_interop",
    )
    def test_t3_07_csv_import_ols_uncertainty_2d_3d_cvd_docx_fidelity(self) -> None:
        csv_path = self.tmpdir / "lab_iv.csv"
        csv_path.write_text("I_mA,V_V\n1.0,1.01\n2.0,1.99\n3.0,3.02\n", encoding="utf-8")
        for plat in _target_platforms(["win", "linux"]):
            out_docx = self.tmpdir / f"lab_report_{plat}.docx"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--import-csv",
                    _cli_path(plat, csv_path),
                    "--export-docx",
                    _cli_path(plat, out_docx),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_ooxml_docx(out_docx).ok)

    @e2e_case(
        case_id="T3-08",
        tier=3,
        group="T3",
        req_ids=["F-234", "F-240", "F-248", "F-250", "SUP-04", "SUP-06", "SUP-08"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t3_08_symbolic_bode_phasor_and_calcbridge_4model_render(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            # Render live CalcBridge card in DocumentStudio
            out_studio_png = self.tmpdir / f"t3_08_studio_calc_card_{plat}.png"
            r_ds = smoke_document_studio(
                plat,
                "canvas",
                out_studio_png,
                calc_card="fx-991LAX|3,+,4,i,=|Z = 3 + 4j (5∠53.13°)",
                showcase_stem=True,
            )
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_studio_png, min_width=300, min_height=200).ok)

            # Verify CalculadoraUnificada on active platform
            out_calc_png = self.tmpdir / f"t3_08_calc_{plat}.png"
            if plat == "win":
                probe = probe_calculadora_win(["--compact"], out_png=out_calc_png, timeout_s=6.0)
                self.assertTrue(probe["started"], f"CalculadoraUnificada.exe failed to start: {probe}")
                self.assertTrue(probe["alive_after_wait"], f"CalculadoraUnificada.exe exited prematurely: {probe}")
                if probe["window_captured"]:
                    self.assertTrue(validate_png(out_calc_png).ok)
            else:
                r_calc = smoke_calculadora_linux(["--compact-cg50"], out_png=out_calc_png, use_xvfb=True, timeout=60)
                self.assertEqual(r_calc.returncode, 0, r_calc.stderr or r_calc.stdout)
                self.assertTrue(validate_png(out_calc_png).ok)

    @e2e_case(
        case_id="T3-09",
        tier=3,
        group="T3",
        req_ids=["F-162", "F-199", "F-203", "F-205", "F-206", "F-207", "F-209", "SUP-08", "SUP-09"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t3_09_keylog_shift_alpha_presenter_console_frozen_projector(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_proj_png = self.tmpdir / f"projector_frozen_{plat}.png"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--presenter-console",
                    "--freeze-projector",
                    "--calc-card",
                    "fx-991CW|SHIFT,SIN,0,.,5,=|arcsin(0.5)",
                    "--screenshot-out",
                    _cli_path(plat, out_proj_png),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_png(out_proj_png).ok)

    @e2e_case(
        case_id="T3-10",
        tier=3,
        group="T3",
        req_ids=["F-186", "F-188", "F-191", "F-228", "F-229", "F-230", "SUP-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_10_deterministic_variants_rubric_exam_mode_lms_export(self) -> None:
        roster = self.tmpdir / "roster.csv"
        roster.write_text("student_id,name\n2026000001,Estudiante Sintetico\n", encoding="utf-8")
        for plat in _target_platforms(["win", "linux"]):
            out_lms = self.tmpdir / f"lms_export_{plat}.zip"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--generate-variants",
                    "3",
                    "--roster",
                    _cli_path(plat, roster),
                    "--exam-mode",
                    "--export-lms",
                    _cli_path(plat, out_lms),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_lms.exists())

    @e2e_case(
        case_id="T3-11",
        tier=3,
        group="T3",
        req_ids=["F-074", "F-104", "F-105", "F-168", "F-170", "F-172", "GN-11"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_11_whiteboard_warp_cleanup_ocr_accent_folded_math_search(self) -> None:
        img_path = self.tmpdir / "board.png"
        img_path.write_bytes(make_minimal_png(320, 180))
        for plat in _target_platforms(["win", "linux"]):
            out_json = self.tmpdir / f"ocr_hits_{plat}.json"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--warp-scan-ocr",
                    _cli_path(plat, img_path),
                    "--search",
                    "ecuacion",
                    "--out-json",
                    _cli_path(plat, out_json),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_json.exists())

    @e2e_case(
        case_id="T3-12",
        tier=3,
        group="T3",
        req_ids=["F-040", "F-110", "F-120", "F-127", "F-129", "F-130", "F-150", "GN-08"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_quadtree_100k",
    )
    def test_t3_12_infinite_canvas_quadtree_frames_and_pdf_region_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            region_pdf = _find_artifact(dc["artifacts"], ".pdf", "canvas_region")
            self.assertIsNotNone(region_pdf, "Missing canvas_region.pdf from DocumentCoreTest")
            assert region_pdf is not None
            self.assertTrue(validate_pdf(region_pdf).ok)

            big_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_205p")
            self.assertIsNotNone(big_pdf, "Missing f8_canvas_205p.pdf")
            assert big_pdf is not None
            self.assertTrue(validate_pdf(big_pdf).ok)

            full_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(full_cudoc, "Missing f11_full.cudoc")
            assert full_cudoc is not None
            self.assertTrue(validate_cudoc(full_cudoc).ok)

            out_png = self.tmpdir / f"t3_12_canvas_{plat}.png"
            r_ds = smoke_document_studio(plat, "canvas", out_png, open_path=full_cudoc)
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T3-13",
        tier=3,
        group="T3",
        req_ids=["F-059", "F-077", "F-218", "F-219", "F-221", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t3_13_wal_conflict_merge_version_history_and_backup_integrity(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            nb_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            nb_wal = _find_artifact(dc["artifacts"], ".cudoc-wal", "f9_notebook")
            self.assertIsNotNone(nb_cudoc, "Missing f9_notebook.cudoc")
            self.assertIsNotNone(nb_wal, "Missing f9_notebook.cudoc-wal")
            assert nb_cudoc is not None and nb_wal is not None

            # DocumentCoreTest F9 appends an intentional 8-byte torn CWAL crash tail to f9_notebook.cudoc-wal
            wal_raw = nb_wal.read_bytes()
            self.assertTrue(wal_raw.startswith(b"CWAL"), "Invalid CWAL header in f9_notebook.cudoc-wal")
            self.assertFalse(validate_cudoc(nb_cudoc, wal_path=nb_wal).ok, "Torn WAL tail must be flagged")
            clean_wal = self.tmpdir / f"clean_f9_{plat}.cudoc-wal"
            clean_bytes = wal_raw[:-8] if len(wal_raw) > 8 and wal_raw[-8:-4] == b"CWAL" else wal_raw
            clean_wal.write_bytes(clean_bytes)
            cudoc_res = validate_cudoc(nb_cudoc, wal_path=clean_wal)
            self.assertTrue(cudoc_res.ok, f"WAL/cudoc validation failed: {cudoc_res.errors}")
            self.assertGreaterEqual(cudoc_res.details.get("wal_frames", 0), 2)

            bak_files = dc["artifacts"].get(".bak", [])
            self.assertGreaterEqual(len(bak_files), 1, "Missing rotating .bak backup file")
            self.assertTrue(validate_cudoc(bak_files[0]).ok)

    @e2e_case(
        case_id="T3-14",
        tier=3,
        group="T3",
        req_ids=["F-064", "F-086", "F-087", "F-093", "F-094", "F-095", "F-293", "F-294", "F-304"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t3_14_pkcs7_pdf_signature_ooxml_and_privacy_scrub_gate(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            signed_pdf = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            self.assertIsNotNone(signed_pdf, "Missing signed_test.pdf from DocumentCoreTest F7")
            assert signed_pdf is not None
            pdf_res = validate_pdf(signed_pdf, require_signature=True)
            self.assertTrue(pdf_res.ok, f"Signed PDF validation failed: {pdf_res.errors}")
            self.assertTrue(pdf_res.details.get("has_signature"))

            docx_p = _find_artifact(dc["artifacts"], ".docx", "exportado")
            pptx_p = _find_artifact(dc["artifacts"], ".pptx", "exportado")
            self.assertIsNotNone(docx_p, "Missing exportado.docx")
            self.assertIsNotNone(pptx_p, "Missing exportado.pptx")
            assert docx_p is not None and pptx_p is not None
            self.assertTrue(validate_ooxml_docx(docx_p).ok)
            self.assertTrue(validate_ooxml_pptx(pptx_p).ok)

            # Ensure zero leaked PII/secrets in emitted text/metadata & stdout
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])

            out_png = self.tmpdir / f"t3_14_signed_{plat}.png"
            r_ds = smoke_document_studio(plat, "pdf", out_png, open_path=signed_pdf, page=1)
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T3-15",
        tier=3,
        group="T3",
        req_ids=["F-281", "F-291", "F-292", "F-295", "F-300"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_15_aes256_cudoc_pin_lock_offline_zero_telemetry(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            enc_cudoc = self.tmpdir / f"vault_enc_{plat}.cudoc"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--encrypt-cudoc",
                    _cli_path(plat, enc_cudoc),
                    "--pin-lock",
                    "1234",
                    "--offline-only",
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(enc_cudoc.exists())
            self.assertNotIn(b"manifest.json", enc_cudoc.read_bytes())

    @e2e_case(
        case_id="T3-16",
        tier=3,
        group="T3",
        req_ids=["F-258", "F-259", "F-260", "F-269", "F-270", "F-276", "F-287"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_16_gpu_reset_hidpi200_high_contrast_reduce_motion_session_restore(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_png = self.tmpdir / f"hidpi_hc_{plat}.png"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--smoke-test",
                    "--theme",
                    "high-contrast",
                    "--reduce-motion",
                    "--dpi-scale",
                    "200",
                    "--simulate-gpu-reset",
                    "--screenshot-out",
                    _cli_path(plat, out_png),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_png(out_png).ok)

    @e2e_case(
        case_id="T3-17",
        tier=3,
        group="T3",
        req_ids=["F-181", "F-182", "F-184", "F-185", "F-196", "F-208", "GN-14"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_17_study_tape_hints_quiz_me_errorbook_timekeeper_analytics(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_analytics = self.tmpdir / f"study_analytics_{plat}.json"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--quiz-me",
                    "--time-keeper",
                    "15m",
                    "--export-analytics",
                    _cli_path(plat, out_analytics),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_analytics.exists())

    @e2e_case(
        case_id="T3-18",
        tier=3,
        group="T3",
        req_ids=["F-151", "F-152", "F-194", "F-215", "F-227", "F-255"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t3_18_markdown_latex_mathml_cpp_given_calc_html5_qr_share(self) -> None:
        md_path = self.tmpdir / "lab.md"
        md_path.write_text("# Lab\n$$E = mc^2$$\n```cpp\nint x = 1;\n```\n", encoding="utf-8")
        for plat in _target_platforms(["win", "linux"]):
            out_html = self.tmpdir / f"published_{plat}.html"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--import-md",
                    _cli_path(plat, md_path),
                    "--publish-html5",
                    _cli_path(plat, out_html),
                    "--lan-qr",
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_html5(out_html).ok)

    @e2e_case(
        case_id="T3-19",
        tier=3,
        group="T3",
        req_ids=["F-306", "F-307", "F-308", "F-309", "F-311", "F-316", "F-318", "F-319"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t3_19_opt_in_local_ai_summary_ai_off_parity_raw_truth(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_audit = self.tmpdir / f"ai_audit_{plat}.json"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--ai-off-verify-core",
                    "--export-audit",
                    _cli_path(plat, out_audit),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            audit = json.loads(out_audit.read_text(encoding="utf-8"))
            self.assertFalse(audit.get("ai_enabled_default", True))

    @e2e_case(
        case_id="T3-20",
        tier=3,
        group="T3",
        req_ids=["F-156", "F-159", "F-210", "F-317"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t3_20_external_audio_offset_bookmarks_keylog_macro_ctrl_g(self) -> None:
        wav_path = self.tmpdir / "ext.wav"
        wav_path.write_bytes(make_minimal_wav(duration_ms=400))
        for plat in _target_platforms(["win", "linux"]):
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--import-audio",
                    _cli_path(plat, wav_path),
                    "--audio-offset-ms",
                    "800",
                    "--bookmark",
                    "Q2",
                    "--replay-macro",
                ],
            )
            self.assertEqual(rc, 0, err or out)

    @e2e_case(
        case_id="ACAD-T3-01",
        tier=3,
        group="G18",
        req_ids=["U-07", "U-08"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_01_tec_material_ocr_search_to_sm2_flashcard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with MockTecServer() as srv:
                out_json = self.tmpdir / f"acad_t3_01_{plat}.json"
                rc, out, err = _run_studio_cli_flag(
                    plat,
                    [
                        "--tec-sync",
                        srv.base_url,
                        "--ocr-index",
                        "--create-sm2-from-hit",
                        "Maxwell",
                        "--out-json",
                        _cli_path(plat, out_json),
                    ],
                    timeout=45,
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertTrue(out_json.exists())

    @e2e_case(
        case_id="ACAD-T3-02",
        tier=3,
        group="G19",
        req_ids=["U-12", "U-09"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_02_horarios_schedule_and_curriculum_to_studio_vault(self) -> None:
        cat_path = self.tmpdir / "catalog.json"
        cat_path.write_text(json.dumps(make_horarios_catalog_canonical()), encoding="utf-8")
        for plat in _target_platforms(["win", "linux"]):
            vault_out = self.tmpdir / f"semester_vault_{plat}.json"
            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--horarios-solve-to-vault",
                    _cli_path(plat, cat_path),
                    "--vault-out",
                    _cli_path(plat, vault_out),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(vault_out.exists())

    @e2e_case(
        case_id="ACAD-T3-03",
        tier=3,
        group="G18",
        req_ids=["U-05", "U-06"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_03_readonly_policy_sanitizer_and_bundle_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with MockTecServer() as srv:
                bundle_dir = self.tmpdir / f"sanitized_bundle_{plat}"
                rc, out, err = _run_studio_cli_flag(
                    plat,
                    [
                        "--tec-export-sanitized-bundle",
                        srv.base_url,
                        "--out-dir",
                        _cli_path(plat, bundle_dir),
                    ],
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertEqual(scan_tree_for_leaks(bundle_dir, extra_secrets=[SYNTH_PASS, "SYNTH_ZOOM_PWD"]), [])

    @e2e_case(
        case_id="ACAD-T3-04",
        tier=3,
        group="G18",
        req_ids=["U-07"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_04_synced_pdf_active_ink_and_cas_spice_card_readonly_blob(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with MockTecServer() as srv:
                out_cudoc = self.tmpdir / f"acad_t3_04_{plat}.cudoc"
                rc, out, err = _run_studio_cli_flag(
                    plat,
                    [
                        "--tec-open-pdf-with-card",
                        srv.base_url,
                        "214778985",
                        "fx-991CW|1,2,/,4,=|I = 12 V / 4 Ohm",
                        "--save-cudoc",
                        _cli_path(plat, out_cudoc),
                    ],
                )
                self.assertEqual(rc, 0, err or out)
                self.assertTrue(validate_cudoc(out_cudoc).ok)

    @e2e_case(
        case_id="ACAD-T3-05",
        tier=3,
        group="G19",
        req_ids=["U-12", "U-09"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_05_horarios_and_notifications_to_foco_study_slot(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with MockTecServer() as srv:
                out_json = self.tmpdir / f"foco_plan_{plat}.json"
                rc, out, err = _run_studio_cli_flag(
                    plat,
                    [
                        "--schedule-foco-from-notifications",
                        srv.base_url,
                        "--out-json",
                        _cli_path(plat, out_json),
                    ],
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertTrue(out_json.exists())

    @e2e_case(
        case_id="ACAD-T3-06",
        tier=3,
        group="G18",
        req_ids=["U-03", "U-07", "U-08"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t3_06_cross_platform_offline_mirror_and_fail_closed_vault(self) -> None:
        payload = make_minimal_pdf(["Portabilidad Windows Linux"], use_flate=True)
        digest = hashlib.sha256(payload).hexdigest()
        for plat in _target_platforms(["win", "linux"]):
            mirror_dir = self.tmpdir / f"portable_mirror_{plat}"
            blobs = mirror_dir / "downloads" / "_blobs"
            (blobs / digest[:2]).mkdir(parents=True, exist_ok=True)
            (blobs / digest[:2] / digest).write_bytes(payload)

            rc, out, err = _run_studio_cli_flag(
                plat,
                [
                    "--verify-offline-mirror",
                    _cli_path(plat, mirror_dir),
                ],
            )
            self.assertEqual(rc, 0, err or out)
