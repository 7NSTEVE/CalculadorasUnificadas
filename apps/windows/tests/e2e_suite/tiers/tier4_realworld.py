"""Tier 4 Opaque-Box E2E Tests: 16 Real-World Scenarios (T4-01..T4-10 + ACAD-T4-01..06)."""
from __future__ import annotations

import atexit
import http.client
import json
import os
import shutil
import tempfile
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import (
    validate_cudoc,
    validate_html5,
    validate_ooxml_docx,
    validate_ooxml_pptx,
    validate_pdf,
    validate_png,
    validate_zip_container,
)
from e2e_suite.adapters.cli_runner import run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_doc_core_keepdir
from e2e_suite.adapters.linux_gui_channel import smoke_calculadora_linux
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scan_tree_for_leaks, scrub_secrets
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.adapters.win_gui_channel import probe_calculadora_win, smoke_document_studio
from e2e_suite.fixtures.corrupt_generators import (
    make_bad_crc_zip,
    make_bad_magic_pdf,
    make_corrupt_wal,
    make_path_traversal_zip,
    make_truncated_pdf,
    make_zip_bomb_payload,
)
from e2e_suite.fixtures.mock_tec_server import MockTecServer
from e2e_suite.fixtures.synthetic_data import make_horarios_catalog_canonical


_SHARED_T4_KEEPDIRS: dict[str, Path] = {}
SYNTH_EMAIL = "student@example.invalid"
SYNTH_UNKNOWN_EMAIL = "unknown.user@example.invalid"
SYNTH_PASS = "SYNTH-TEST-PASS-0001"
SYNTH_WRONG_PASS = "SYNTH-WRONG-PASS-9999"


def _cleanup_t4_keepdirs() -> None:
    if os.environ.get("E2E_KEEP_ARTIFACTS", "").strip() == "1":
        return
    for d in _SHARED_T4_KEEPDIRS.values():
        shutil.rmtree(d, ignore_errors=True)
    _SHARED_T4_KEEPDIRS.clear()


atexit.register(_cleanup_t4_keepdirs)


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
    if plat not in _SHARED_T4_KEEPDIRS:
        _SHARED_T4_KEEPDIRS[plat] = create_session_keepdir(f"e2e_t4_keep_{plat}_")
    return run_doc_core_keepdir(plat, _SHARED_T4_KEEPDIRS[plat], timeout=240)


def _find_artifact(artifacts: dict[str, list[Path]], ext: str, name_substr: str) -> Path | None:
    for p in artifacts.get(ext, []):
        if name_substr.lower() in p.name.lower():
            return p
    return None


def _run_studio_cli(plat: str, args: list[str], env: dict[str, str] | None = None, timeout: int = 45) -> tuple[int, str, str]:
    paths = resolve_paths(REPO_ROOT)
    if plat == "linux":
        sh = paths["linux_run_document_studio"]
        if not sh.exists():
            return (-1, "", f"Missing {sh}")
        merged_env = {"DISPLAY": ""}
        if env:
            merged_env.update(env)
        res = run_wsl(
            [win_to_wsl_path(sh), *args],
            env=merged_env,
            timeout=timeout,
            cwd_linux=win_to_wsl_path(REPO_ROOT),
        )
        return (res.returncode, res.stdout, res.stderr)
    exe = paths["win_document_studio"]
    if not exe.exists():
        return (-1, "", f"Missing {exe}")
    res = run_win([str(exe), *args], env=env, timeout=timeout, cwd=REPO_ROOT)
    return (res.returncode, res.stdout, res.stderr)


class _NoRedirectHandler(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req: Any, fp: Any, code: int, msg: str, headers: Any, newurl: str) -> Any:
        return None


class TestTier4RealWorld(unittest.TestCase):
    """16 real-world end-to-end scenarios (T4-01..T4-10 + ACAD-T4-01..ACAD-T4-06)."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_t4_case_"))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="T4-01",
        tier=4,
        group="T4",
        req_ids=["F-001", "F-006", "F-013", "F-048", "F-049", "F-054", "F-066", "F-075", "F-090", "F-092", "F-096", "F-132", "F-133", "F-136", "F-146", "F-152", "F-194", "F-231", "F-232", "F-233", "F-237", "F-243", "F-246", "F-313", "F-315", "SUP-01", "SUP-02", "SUP-03", "SUP-05", "SUP-06", "SUP-08"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t4_01_structural_engineering_memo_units_varsub_calcbridge(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            unicode_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            f11_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            docx_p = _find_artifact(dc["artifacts"], ".docx", "exportado")

            self.assertIsNotNone(stem_pdf)
            self.assertIsNotNone(unicode_pdf)
            self.assertIsNotNone(f11_cudoc)
            self.assertIsNotNone(docx_p)
            assert stem_pdf and unicode_pdf and f11_cudoc and docx_p

            self.assertTrue(validate_pdf(stem_pdf).ok)
            self.assertTrue(validate_pdf(unicode_pdf).ok)
            self.assertTrue(validate_cudoc(f11_cudoc).ok)
            self.assertTrue(validate_ooxml_docx(docx_p).ok)

            out_png = self.tmpdir / f"t4_01_memo_{plat}.png"
            r_ds = smoke_document_studio(
                plat,
                "canvas",
                out_png,
                calc_card="fx-991LAX|1,0,*,1,0,=|M = 10 kip * 10 ft | kN*m",
                showcase_stem=True,
            )
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T4-02",
        tier=4,
        group="T4",
        req_ids=["F-049", "F-109", "F-111", "F-128", "F-173", "F-234", "F-235", "F-236", "F-238", "F-242", "F-249", "F-254", "F-256", "F-315", "SUP-04", "SUP-06"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t4_02_analog_power_electronics_lab_bench_report(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            f11_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(stem_pdf)
            self.assertIsNotNone(f11_cudoc)
            assert stem_pdf and f11_cudoc

            self.assertTrue(validate_pdf(stem_pdf).ok)
            self.assertTrue(validate_cudoc(f11_cudoc).ok)

            out_png = self.tmpdir / f"t4_02_electronics_{plat}.png"
            r_ds = smoke_document_studio(plat, "canvas", out_png, open_path=f11_cudoc, showcase_stem=True)
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T4-03",
        tier=4,
        group="T4",
        req_ids=["F-016", "F-046", "F-069", "F-153", "F-165", "F-179", "F-189", "GN-01", "GN-14"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t4_03_university_semester_study_and_exam_prep(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_report = self.tmpdir / f"study_analytics_{plat}.json"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--smoke-test",
                    "--study-session-e2e",
                    "--exam-mode",
                    "--export-analytics",
                    _cli_path(plat, out_report),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_report.exists())

    @e2e_case(
        case_id="T4-04",
        tier=4,
        group="T4",
        req_ids=["F-041", "F-112", "F-120", "F-148", "F-160", "F-198", "F-216", "F-227", "SUP-08", "SUP-09"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t4_04_lecturer_live_classroom_dual_calculator_projection(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_html = self.tmpdir / f"classroom_recap_{plat}.html"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--presenter-console",
                    "--freeze-projector",
                    "--lan-qr",
                    "--export-html5",
                    _cli_path(plat, out_html),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_html5(out_html).ok)

    @e2e_case(
        case_id="T4-05",
        tier=4,
        group="T4",
        req_ids=["F-005", "F-013", "F-050", "F-060", "F-079", "F-095", "F-135", "F-147", "F-217", "F-292", "F-305", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t4_05_cross_platform_signed_pdf_ooxml_and_cudoc_deliverable(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            signed_pdf = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            docx_p = _find_artifact(dc["artifacts"], ".docx", "exportado")
            pptx_p = _find_artifact(dc["artifacts"], ".pptx", "exportado")
            f11_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")

            self.assertIsNotNone(signed_pdf)
            self.assertIsNotNone(docx_p)
            self.assertIsNotNone(pptx_p)
            self.assertIsNotNone(f11_cudoc)
            assert signed_pdf and docx_p and pptx_p and f11_cudoc

            sig_res = validate_pdf(signed_pdf, require_signature=True)
            self.assertTrue(sig_res.ok, f"PKCS#7 validation failed: {sig_res.errors}")
            self.assertTrue(validate_ooxml_docx(docx_p).ok)
            self.assertTrue(validate_ooxml_pptx(pptx_p).ok)
            self.assertTrue(validate_cudoc(f11_cudoc).ok)

            out_png = self.tmpdir / f"t4_05_signed_pdf_{plat}.png"
            r_ds = smoke_document_studio(plat, "pdf", out_png, open_path=signed_pdf, page=1)
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T4-06",
        tier=4,
        group="T4",
        req_ids=["F-019", "F-099", "F-110", "F-127", "F-150", "F-168", "F-310", "GN-05", "GN-08", "SUP-05", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t4_06_field_whiteboard_digitization_and_scaled_cad_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_dxf = self.tmpdir / f"scaled_cad_{plat}.dxf"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--scale",
                    "1:50",
                    "--export-dxf",
                    _cli_path(plat, out_dxf),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_dxf.exists())

    @e2e_case(
        case_id="T4-07",
        tier=4,
        group="T4",
        req_ids=["F-032", "F-257", "F-267", "F-269", "F-271"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t4_07_accessible_bilingual_keyboard_and_screen_reader_workflow(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_png = self.tmpdir / f"t4_07_a11y_{plat}.png"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--lang",
                    "es",
                    "--theme",
                    "high-contrast",
                    "--ui-text-scale",
                    "150",
                    "--dpi-scale",
                    "200",
                    "--reduce-motion",
                    "--screenshot-out",
                    _cli_path(plat, out_png),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_png(out_png).ok)

    @e2e_case(
        case_id="T4-08",
        tier=4,
        group="T4",
        req_ids=["F-072", "F-073", "F-113", "F-134", "F-162", "F-206", "F-247", "F-248", "SUP-08"],
        apps=["CalculadoraUnificada", "DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t4_08_four_model_casio_parity_manuals_and_calcbridge(self) -> None:
        paths = resolve_paths(REPO_ROOT)
        for plat in _target_platforms(["win", "linux"]):
            if plat == "win":
                # Verify 4 Casio models, 16 PDF manuals, and CG50 backend contract
                ui_test = paths["win_ui_core_test"]
                self.assertTrue(ui_test.exists(), f"Missing {ui_test}")
                r_ui = run_win([str(ui_test)], timeout=90, cwd=REPO_ROOT)
                self.assertEqual(r_ui.returncode, 0, r_ui.stderr or r_ui.stdout)
                self.assertIn("ALL UiCoreModules TESTS PASSED", r_ui.stdout)

                cg50_test = paths["win_cg50_contract_test"]
                self.assertTrue(cg50_test.exists(), f"Missing {cg50_test}")
                r_cg = run_win([str(cg50_test)], timeout=240, cwd=REPO_ROOT)
                self.assertEqual(r_cg.returncode, 0, r_cg.stderr or r_cg.stdout)

                out_calc_png = self.tmpdir / f"t4_08_win_calc_{plat}.png"
                probe = probe_calculadora_win(["--compact"], out_png=out_calc_png, timeout_s=6.0)
                self.assertTrue(probe["started"] and probe["alive_after_wait"], f"Win32 Calc probe failed: {probe}")
                if probe["window_captured"]:
                    self.assertTrue(validate_png(out_calc_png).ok)
            else:
                # Verify Linux 4 Casio engines + 16 manuals + CG50 + Xvfb GTK render
                eng_test = paths["linux_engines_test"]
                self.assertTrue(eng_test.exists(), f"Missing {eng_test}")
                r_eng = run_wsl(
                    [win_to_wsl_path(eng_test)],
                    timeout=120,
                    cwd_linux=win_to_wsl_path(REPO_ROOT),
                )
                self.assertEqual(r_eng.returncode, 0, r_eng.stderr or r_eng.stdout)
                self.assertIn("failed=0", r_eng.stdout)

                out_calc_png = self.tmpdir / f"t4_08_linux_calc_{plat}.png"
                r_calc = smoke_calculadora_linux(["--compact-cg50"], out_png=out_calc_png, use_xvfb=True, timeout=60)
                self.assertEqual(r_calc.returncode, 0, r_calc.stderr or r_calc.stdout)
                self.assertTrue(validate_png(out_calc_png).ok)

            # Verify CalcBridge card renders inside DocumentStudio on active platform
            out_ds_png = self.tmpdir / f"t4_08_studio_bridge_{plat}.png"
            r_ds = smoke_document_studio(
                plat,
                "canvas",
                out_ds_png,
                calc_card="fx-991CW|2,+,3,*,4,=|2+3*4=14",
                showcase_stem=True,
            )
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_ds_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="T4-09",
        tier=4,
        group="T4",
        req_ids=["F-084", "F-186", "F-188", "F-189", "F-191", "F-193", "F-197", "F-222", "F-228", "F-230", "F-293", "F-296", "SUP-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t4_09_high_stakes_assessment_authoring_and_rubric_grading(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_lms = self.tmpdir / f"assessment_lms_{plat}.zip"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--generate-variants",
                    "10",
                    "--exam-mode",
                    "--grade-rubric",
                    "--export-lms",
                    _cli_path(plat, out_lms),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(out_lms.exists())

    @e2e_case(
        case_id="T4-10",
        tier=4,
        group="T4",
        req_ids=["F-001", "F-005", "F-009", "F-010", "F-014", "F-058", "F-085", "F-272", "F-284", "F-291", "F-297", "F-300", "GN-12"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t4_10_catastrophic_failure_fuzz_resilience_and_wal_recovery(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _get_doc_core_run(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"] or dc["stdout"][-400:])

            # 1. Verify WAL + .bak recovery artifacts emitted by DocumentCoreTest
            nb_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            nb_wal = _find_artifact(dc["artifacts"], ".cudoc-wal", "f9_notebook")
            baks = dc["artifacts"].get(".bak", [])
            self.assertIsNotNone(nb_cudoc)
            self.assertIsNotNone(nb_wal)
            self.assertGreaterEqual(len(baks), 1)
            assert nb_cudoc and nb_wal
            wal_raw = nb_wal.read_bytes()
            self.assertTrue(wal_raw.startswith(b"CWAL"))
            self.assertFalse(validate_cudoc(nb_cudoc, wal_path=nb_wal).ok)
            clean_wal = self.tmpdir / f"clean_f9_t4_{plat}.cudoc-wal"
            clean_wal.write_bytes(wal_raw[:-8] if len(wal_raw) > 8 and wal_raw[-8:-4] == b"CWAL" else wal_raw)
            wal_res = validate_cudoc(nb_cudoc, wal_path=clean_wal)
            self.assertTrue(wal_res.ok, f"Valid WAL verification failed: {wal_res.errors}")
            self.assertGreaterEqual(wal_res.details.get("wal_frames", 0), 2)
            self.assertTrue(validate_cudoc(baks[0]).ok)

            # 2. Verify corrupt/fuzz containers are deterministically rejected
            bad_crc_p = self.tmpdir / f"bad_crc_{plat}.cudoc"
            bad_crc_p.write_bytes(make_bad_crc_zip())
            self.assertFalse(validate_cudoc(bad_crc_p).ok)

            trav_p = self.tmpdir / f"traversal_{plat}.zip"
            trav_p.write_bytes(make_path_traversal_zip("../../evil.txt"))
            self.assertFalse(validate_zip_container(trav_p).ok)

            bomb_p = self.tmpdir / f"bomb_{plat}.zip"
            bomb_p.write_bytes(make_zip_bomb_payload(uncompressed_mb=10))
            self.assertFalse(validate_zip_container(bomb_p, max_ratio=50.0).ok)

            trunc_pdf = self.tmpdir / f"trunc_{plat}.pdf"
            trunc_pdf.write_bytes(make_truncated_pdf())
            self.assertFalse(validate_pdf(trunc_pdf).ok)

            spoof_pdf = self.tmpdir / f"spoof_{plat}.pdf"
            spoof_pdf.write_bytes(make_bad_magic_pdf(as_html_login=False))
            self.assertFalse(validate_pdf(spoof_pdf).ok)

            corrupt_wal_p = self.tmpdir / f"corrupt_{plat}.cudoc-wal"
            corrupt_wal_p.write_bytes(make_corrupt_wal(corrupt_tail=True))
            self.assertFalse(validate_cudoc(nb_cudoc, wal_path=corrupt_wal_p).ok)

            # 3. Verify DocumentStudio loads valid recovered .cudoc offline cleanly
            out_png = self.tmpdir / f"t4_10_recovered_{plat}.png"
            r_ds = smoke_document_studio(plat, "canvas", out_png, open_path=nb_cudoc)
            self.assertEqual(r_ds.returncode, 0, r_ds.stderr or r_ds.stdout)
            self.assertTrue(validate_png(out_png, min_width=300, min_height=200).ok)

    @e2e_case(
        case_id="ACAD-T4-01",
        tier=4,
        group="G19",
        req_ids=["U-01", "U-03", "U-09", "U-12"],
        apps=["DocumentStudio", "AcademicTecDigital", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t4_01_first_day_semester_onboarding(self) -> None:
        cat_path = self.tmpdir / "catalog.json"
        cat_path.write_text(json.dumps(make_horarios_catalog_canonical()), encoding="utf-8")
        for plat in _target_platforms(["win", "linux"]):
            out_dir = self.tmpdir / f"onboarding_{plat}"
            with MockTecServer() as srv:
                rc, out, err = _run_studio_cli(
                    plat,
                    [
                        "--tec-onboard",
                        srv.base_url,
                        "--horarios-catalog",
                        _cli_path(plat, cat_path),
                        "--out-dir",
                        _cli_path(plat, out_dir),
                    ],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertEqual(scan_tree_for_leaks(out_dir, extra_secrets=[SYNTH_PASS]), [])

    @e2e_case(
        case_id="ACAD-T4-02",
        tier=4,
        group="G18",
        req_ids=["U-07", "U-08"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t4_02_interrupted_campus_wifi_sync_resume_and_quarantine(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            mirror_dir = self.tmpdir / f"wifi_mirror_{plat}"
            with MockTecServer() as srv:
                rc, out, err = _run_studio_cli(
                    plat,
                    [
                        "--tec-sync-fault-drill",
                        srv.base_url,
                        "--out-dir",
                        _cli_path(plat, mirror_dir),
                    ],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertTrue((mirror_dir / "material_index" / "stats.json").exists())

    @e2e_case(
        case_id="ACAD-T4-03",
        tier=4,
        group="G18",
        req_ids=["U-08"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t4_03_exam_week_offline_multimodal_study_session(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            out_html = self.tmpdir / f"exam_report_{plat}.html"
            rc, out, err = _run_studio_cli(
                plat,
                [
                    "--offline-only",
                    "--exam-week-session",
                    "--export-html5",
                    _cli_path(plat, out_html),
                ],
            )
            self.assertEqual(rc, 0, err or out)
            self.assertTrue(validate_html5(out_html).ok)

    @e2e_case(
        case_id="ACAD-T4-04",
        tier=4,
        group="G18",
        req_ids=["U-05", "U-09", "U-12"],
        apps=["DocumentStudio", "AcademicTecDigital", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t4_04_academic_progress_and_prematricula_readonly_audit(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with MockTecServer() as srv:
                out_json = self.tmpdir / f"progress_audit_{plat}.json"
                rc, out, err = _run_studio_cli(
                    plat,
                    [
                        "--tec-academic-audit",
                        srv.base_url,
                        "--out-json",
                        _cli_path(plat, out_json),
                    ],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(rc, 0, err or out)
                self.assertEqual(srv.mutation_received_count, 0)
                self.assertTrue(out_json.exists())

    @e2e_case(
        case_id="ACAD-T4-05",
        tier=4,
        group="G18",
        req_ids=["U-01", "U-05", "U-09"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_ui_wiring",
    )
    def test_acad_t4_05_live_tec_digital_parameterized_readonly_smoke(self) -> None:
        live_flag = (
            os.environ.get("TEC_E2E_LIVE", "").strip() == "1"
            or os.environ.get("E2E_LIVE_TEC", "").strip() == "1"
        )
        live_email = os.environ.get("TEC_EMAIL", "")
        live_pass = os.environ.get("TEC_PASSWORD", "")

        for plat in _target_platforms(["win", "linux"]):
            if live_flag and live_email and live_pass:
                out_json = self.tmpdir / f"live_smoke_{plat}.json"
                rc, out, err = _run_studio_cli(
                    plat,
                    [
                        "--tec-live-readonly-smoke",
                        "https://tecdigital.tec.ac.cr",
                        "--out-json",
                        _cli_path(plat, out_json),
                    ],
                    env={
                        "TEC_EMAIL": live_email,
                        "TEC_PASSWORD": live_pass,
                        "TEC_E2E_LIVE": "1",
                    },
                    timeout=60,
                )
                combined = out + "\n" + err
                self.assertEqual(scan_text_for_leaks(combined, extra_secrets=[live_email, live_pass]), [])
                self.assertEqual(rc, 0, scrub_secrets(combined, extra_secrets=[live_email, live_pass]))
                self.assertEqual(scan_tree_for_leaks(self.tmpdir, extra_secrets=[live_email, live_pass]), [])
            else:
                # Clean Mode A fallback using loopback MockTecServer
                with MockTecServer() as srv:
                    out_json = self.tmpdir / f"mock_smoke_{plat}.json"
                    rc, out, err = _run_studio_cli(
                        plat,
                        [
                            "--tec-live-readonly-smoke",
                            srv.base_url,
                            "--out-json",
                            _cli_path(plat, out_json),
                        ],
                        env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                        timeout=30,
                    )
                    self.assertEqual(rc, 0, err or out)
                    self.assertEqual(srv.mutation_received_count, 0)

    @e2e_case(
        case_id="ACAD-T4-06",
        tier=4,
        group="G18",
        req_ids=["U-05", "U-06", "R3"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_acad_t4_06_mock_tec_contract_privacy_guard_and_clean_workspace(self) -> None:
        # 1. Verify MockTecServer end-to-end HTTP contracts (auth, cookies, Range 206, 403/404, sedes, read-only guard)
        with MockTecServer() as srv:
            opener_no_redir = urllib.request.build_opener(_NoRedirectHandler())
            with self.assertRaises(urllib.error.HTTPError) as redir_ctx:
                opener_no_redir.open(f"{srv.base_url}/dotlrn/", timeout=10)
            self.assertEqual(redir_ctx.exception.code, 302)
            self.assertIn("/register/", redir_ctx.exception.headers.get("Location", ""))

            # Invalid auth paths: no_account & bad_password
            for email, pw, expected_status in (
                (SYNTH_UNKNOWN_EMAIL, SYNTH_PASS, "no_account"),
                (SYNTH_EMAIL, SYNTH_WRONG_PASS, "bad_password"),
            ):
                req_bad = urllib.request.Request(
                    f"{srv.base_url}/api/login/new-form/",
                    data=json.dumps({"email": email, "password": pw, "returnUrl": "/dotlrn/"}).encode("utf-8"),
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with urllib.request.urlopen(req_bad, timeout=10) as r:
                    self.assertEqual(json.loads(r.read().decode("utf-8")).get("status"), expected_status)

            # Valid synthetic login
            req_ok = urllib.request.Request(
                f"{srv.base_url}/api/login/new-form/",
                data=json.dumps({"email": SYNTH_EMAIL, "password": SYNTH_PASS, "returnUrl": "/dotlrn/"}).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req_ok, timeout=10) as r:
                self.assertEqual(json.loads(r.read().decode("utf-8")).get("status"), "ok")
                cookies = "; ".join(c.split(";", 1)[0] for c in (r.headers.get_all("Set-Cookie") or []))

            # Authenticated /dotlrn/ portal
            req_portal = urllib.request.Request(f"{srv.base_url}/dotlrn/", headers={"Cookie": cookies})
            with urllib.request.urlopen(req_portal, timeout=10) as r:
                html = r.read().decode("utf-8")
                self.assertIn("153108817", html)

            # Non-null sedes verification
            with urllib.request.urlopen(f"{srv.base_url}/tds-curriculum-exp/ajax/carga_sedes_json", timeout=10) as r:
                sedes = json.loads(r.read().decode("utf-8"))
                self.assertGreaterEqual(len(sedes), 2)
                for s in sedes:
                    self.assertTrue(s.get("id") and s.get("nombre"))

            # Fault 900001 partial drop + Range 206 resume -> valid PDF
            part1 = b""
            try:
                with urllib.request.urlopen(f"{srv.base_url}/dotlrn/file-storage/view/900001", timeout=10) as r:
                    part1 = r.read()
            except http.client.IncompleteRead as inc:
                part1 = inc.partial
            self.assertGreater(len(part1), 0, "Expected partial chunk before simulated 900001 drop")
            req_rng = urllib.request.Request(
                f"{srv.base_url}/dotlrn/file-storage/view/900001",
                headers={"Range": f"bytes={len(part1)}-"},
            )
            with urllib.request.urlopen(req_rng, timeout=10) as r:
                self.assertEqual(r.status, 206)
                part2 = r.read()
            resumed_pdf = self.tmpdir / "resumed_900001.pdf"
            resumed_pdf.write_bytes(part1 + part2)
            self.assertTrue(validate_pdf(resumed_pdf).ok)

            # 403 & 404 distinction
            for oid, code in (("900004", 403), ("900005", 404)):
                with self.assertRaises(urllib.error.HTTPError) as ctx:
                    urllib.request.urlopen(f"{srv.base_url}/dotlrn/file-storage/view/{oid}", timeout=10)
                self.assertEqual(ctx.exception.code, code)

            self.assertEqual(srv.mutation_received_count, 0)

        # 2. Live privacy_guard scan across the entire tests/e2e_suite/ tree
        suite_dir = REPO_ROOT / "tests" / "e2e_suite"
        tree_leaks = scan_tree_for_leaks(suite_dir)
        self.assertEqual(tree_leaks, [], f"Privacy leaks detected in tests/e2e_suite: {tree_leaks}")

        # 3. Verify scrub_secrets neutralizes ephemeral tokens/cookies/?pwd= strings cleanly
        ephem_sess = "RAW_EPHEMERAL_SESS_99"
        ephem_zoom = "ZOOM_RAW_SECRET_99"
        ephem_tok = "LIVE_TOKEN_99"
        raw_trace = (
            "Cookie: ad_session_id" + "=" + ephem_sess + "\n"
            "URL: https://zoom.example.invalid/j/999?pwd" + "=" + ephem_zoom + "\n"
            "Authorization" + ": Bearer " + ephem_tok + "\n"
        )
        scrubbed = scrub_secrets(raw_trace, extra_secrets=[ephem_sess, ephem_zoom, ephem_tok])
        self.assertEqual(
            scan_text_for_leaks(scrubbed, extra_secrets=[ephem_sess, ephem_zoom, ephem_tok]),
            [],
        )

        # 4. Assert zero stray credential/vault artifacts left in repository root
        for stray in ("session.dpapi", "session_meta.json", ".env"):
            self.assertFalse((REPO_ROOT / stray).exists(), f"Forbidden credential artifact in repo root: {stray}")
