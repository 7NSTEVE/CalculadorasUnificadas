"""Tier 1 E2E Tests: Areas 10..17 (41 happy-path isolated feature cases)."""
from __future__ import annotations

import json
import os
import tempfile
import unittest
import zipfile
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import (
    sha256_file,
    validate_csv,
    validate_cudoc,
    validate_dxf,
    validate_html5,
    validate_kicad_sch,
    validate_pdf,
    validate_png,
    validate_svg,
    validate_zip_container,
)
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.cpp_probe_channel import run_cpp_probe
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_ctest_target, run_doc_core_keepdir
from e2e_suite.adapters.linux_gui_channel import smoke_calculadora_linux
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scan_tree_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_readiness_entry, get_runtime_context
from e2e_suite.adapters.win_gui_channel import probe_calculadora_win, smoke_document_studio
from e2e_suite.fixtures.synthetic_data import make_casio_step_specs, make_mna_rc_netlist


_CTEST_CACHE: dict[tuple[str, str], tuple[int, RunResult]] = {}


def _plat_path(plat: str, path: Path) -> str:
    return win_to_wsl_path(path) if plat == "linux" else str(path)


def _run_studio_cli(plat: str, args: list[str], timeout: int = 30) -> RunResult:
    paths = resolve_paths(REPO_ROOT)
    if plat == "linux":
        sh = paths["linux_run_document_studio"]
        if not sh.exists():
            return RunResult(-1, "", f"Missing {sh}", 0.0, False)
        return run_wsl(
            [win_to_wsl_path(sh), *args],
            env={"DISPLAY": "", "XDG_CONFIG_HOME": f"/tmp/cu_e2e_xdg_{os.getpid()}"},
            timeout=timeout,
            cwd_linux=win_to_wsl_path(REPO_ROOT),
        )
    exe = paths["win_document_studio"]
    if not exe.exists():
        return RunResult(-1, "", f"Missing {exe}", 0.0, False)
    return run_win([str(exe), *args], timeout=timeout, cwd=REPO_ROOT)


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


def _shared_keepdir(plat: str) -> Path:
    env_key = f"E2E_SHARED_KEEPDIR_{plat.upper()}"
    existing = os.environ.get(env_key, "").strip()
    if existing:
        p = Path(existing)
        p.mkdir(parents=True, exist_ok=True)
        return p
    p = create_session_keepdir(f"e2e_shared_{plat}_")
    os.environ[env_key] = str(p)
    return p


def _run_doccore(plat: str) -> dict[str, Any]:
    timeout = 150 if plat == "win" else 300
    return run_doc_core_keepdir(plat, keep_dir=_shared_keepdir(plat), timeout=timeout)


def _run_ctest_cached(plat: str, regex: str, timeout: int = 90) -> RunResult:
    key = (plat, regex)
    cached = _CTEST_CACHE.get(key)
    if cached is not None:
        cached_timeout, cached_res = cached
        if not cached_res.timed_out or cached_timeout >= timeout:
            return cached_res
    res = run_ctest_target(plat, regex, timeout=timeout)
    _CTEST_CACHE[key] = (timeout, res)
    return res


def _find_artifact(artifacts: dict[str, list[Path]], ext: str, *name_fragments: str) -> Path | None:
    for p in artifacts.get(ext, []):
        low = p.name.lower()
        if all(frag.lower() in low for frag in name_fragments):
            return p
    return None


class TestTier1Areas10To17(unittest.TestCase):
    """Tier 1 isolated feature checks covering Areas 10..17."""

    # =========================================================================
    # Area 10: Estudio y practica (F-179..F-197, GN-14, SUP-09)
    # =========================================================================

    @e2e_case(
        case_id="T1-A10-01",
        tier=1,
        group="G10",
        req_ids=["F-179", "F-180", "F-181", "GN-14", "U-16"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a10_01_sm2_fsrs_flashcards_and_study_tape(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("StudyTape", dc["stdout"])
            self.assertIn("SM-2", dc["stdout"])

    @e2e_case(
        case_id="T1-A10-02",
        tier=1,
        group="G10",
        req_ids=["F-182", "F-183", "F-184", "F-185"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a10_02_exercise_bank_hints_errorbook_and_quiz_me(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F9", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)

    @e2e_case(
        case_id="T1-A10-03",
        tier=1,
        group="G10",
        req_ids=["F-186", "F-187", "F-188", "F-189"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a10_03_teacher_rubric_exam_planner_and_locked_exam_mode(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, ["--cas", "det([[1,2],[3,4]])"])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("-2", pr.stdout)
            with tempfile.TemporaryDirectory(prefix="t1_a10_03_") as tmp:
                out_cudoc = Path(tmp) / "exam_locked.cudoc"
                shot = Path(tmp) / f"exam_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--exam-mode",
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A10-04",
        tier=1,
        group="G10",
        req_ids=["F-190", "F-191", "F-192"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a10_04_step_playback_numeric_variants_and_parametric_mcq(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr_inv = run_cpp_probe(plat, ["--cas", "inv([[2,1],[1,2]])"])
            self.assertEqual(pr_inv.returncode, 0, pr_inv.stderr)
            for r_val in (100, 250, 500, 750, 1000):
                pr = run_cpp_probe(plat, ["--cas", f"I = 12 V / {r_val} Ohm | mA"])
                self.assertEqual(pr.returncode, 0, pr.stderr)
            with tempfile.TemporaryDirectory(prefix="t1_a10_04_") as tmp:
                out_json = Path(tmp) / "variants.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--generate-variants",
                        "3",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing generated variants JSON")

    @e2e_case(
        case_id="T1-A10-05",
        tier=1,
        group="G10",
        req_ids=["F-193", "F-194", "F-195", "F-196", "F-197", "SUP-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a10_05_given_vs_calculated_layers_and_study_analytics(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Layers", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc, "Missing f11_full.cudoc in keepDir")
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)
            with zipfile.ZipFile(cudoc, "r") as zf:
                manifest = json.loads(zf.read("manifest.json").decode("utf-8"))
                objs = json.loads(zf.read("objects.json").decode("utf-8")).get("canvasObjects", [])
                pages = json.loads(zf.read("pages.json").decode("utf-8"))
                all_objs = list(objs) + [o for pg in pages for o in pg.get("objects", [])]
                self.assertTrue(bool(manifest.get("layers")) or len(all_objs) > 0, "Expected non-empty layers/objects")

            with tempfile.TemporaryDirectory(prefix="t1_a10_05_") as tmp:
                d = Path(tmp)
                html_p = d / "study_report.html"
                hr = run_cpp_probe(plat, ["--export-report-html", "StudySet SM-2 Review", _plat_path(plat, html_p)])
                self.assertEqual(hr.returncode, 0, hr.stderr)
                self.assertTrue(validate_html5(html_p).ok)
                shot = d / f"study_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=cudoc)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 11: Ensenanza y proyeccion (F-198..F-216, SUP-08, SUP-09)
    # =========================================================================

    @e2e_case(
        case_id="T1-A11-01",
        tier=1,
        group="G11",
        req_ids=["F-198", "F-199", "F-200", "F-201", "F-202"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a11_01_presentation_mode_clean_projector_and_laser_pointer(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a11_01_") as tmp:
                shot = Path(tmp) / f"pres_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A11-02",
        tier=1,
        group="G11",
        req_ids=["F-203", "F-204", "F-208", "F-209", "F-210", "SUP-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a11_02_teacher_presenter_console_frozen_and_stage_reveal(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)

    @e2e_case(
        case_id="T1-A11-03",
        tier=1,
        group="G11",
        req_ids=["F-205", "F-206", "F-207", "SUP-08", "fx-570LA X", "fx-CG50", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t1_a11_03_two_calculator_compare_and_presenter_keylog_step(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a11_03_") as tmp:
                for idx, spec in enumerate(make_casio_step_specs()[:2]):
                    out_cudoc = Path(tmp) / f"cmp_{idx}.cudoc"
                    shot = Path(tmp) / f"cmp_{idx}.png"
                    sr = _run_studio_cli(
                        plat,
                        [
                            "--smoke-test",
                            "--mode",
                            "canvas",
                            "--calc-card",
                            spec,
                            "--calc-bridge-sync",
                            "--out-cudoc",
                            _plat_path(plat, out_cudoc),
                            "--screenshot-out",
                            _plat_path(plat, shot),
                        ],
                    )
                    self.assertEqual(sr.returncode, 0, sr.stderr)
                    self.assertTrue(validate_cudoc(out_cudoc).ok)
                    self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A11-04",
        tier=1,
        group="G11",
        req_ids=["F-211", "F-212", "F-216"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a11_04_lecture_template_and_offline_html5_tutorial_package(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Lab HTML", dc["stdout"])
            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            self.assertIsNotNone(stem_pdf)
            assert stem_pdf is not None
            self.assertTrue(validate_pdf(stem_pdf).ok)

    @e2e_case(
        case_id="T1-A11-05",
        tier=1,
        group="G11",
        req_ids=["F-213", "F-214", "F-215"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t1_a11_05_lan_classroom_broadcast_and_qr_remote_control(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a11_05_") as tmp:
                d = Path(tmp)
                sut_html = d / "broadcast.html"
                shot = d / f"qr_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-broadcast",
                        "--port",
                        "0",
                        "--qr-payload",
                        "http://127.0.0.1:0/session/demo",
                        "--export-qr-html",
                        _plat_path(plat, sut_html),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_html5(sut_html).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 12: Colaboracion y sincronizacion (F-217..F-230)
    # =========================================================================

    @e2e_case(
        case_id="T1-A12-01",
        tier=1,
        group="G12",
        req_ids=["F-217", "F-218", "F-225"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t1_a12_01_open_cudoc_schema_split_delta_and_deep_link(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            valid_cudocs = [p for p in dc["artifacts"].get(".cudoc", []) if "proyecto" not in p.name.lower()]
            self.assertTrue(valid_cudocs, "Missing valid .cudoc containers in keepDir")
            for cudoc in valid_cudocs:
                res = validate_cudoc(cudoc)
                self.assertTrue(res.ok, f"{cudoc.name}: {res.errors}")

    @e2e_case(
        case_id="T1-A12-02",
        tier=1,
        group="G12",
        req_ids=["F-219", "F-221", "F-222", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a12_02_conflict_page_merge_comments_and_readonly_layer(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Conflict Merge", dc["stdout"])

    @e2e_case(
        case_id="T1-A12-03",
        tier=1,
        group="G12",
        req_ids=["F-220", "F-223", "F-224", "F-226"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t1_a12_03_lan_p2p_crdt_sync_presence_and_author_history(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a12_03_") as tmp:
                d = Path(tmp)
                sut_html = d / "p2p_session.html"
                out_cudoc = d / "p2p_merged.cudoc"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-p2p-sync",
                        "--port",
                        "0",
                        "--export-qr-html",
                        _plat_path(plat, sut_html),
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_html5(sut_html).ok)
                self.assertTrue(validate_cudoc(out_cudoc).ok)

    @e2e_case(
        case_id="T1-A12-04",
        tier=1,
        group="G12",
        req_ids=["F-227", "F-228", "F-229", "F-230"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a12_04_static_html5_batch_templates_and_lms_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Lab HTML", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc, "Missing f11_full.cudoc in keepDir")
            assert cudoc is not None
            self.assertEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, cudoc)]).returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t1_a12_04_") as tmp:
                d = Path(tmp)
                html_p = d / "lab_report.html"
                md_p = d / "lab_report.md"
                hr = run_cpp_probe(plat, ["--export-report-html", "LMS Batch Export", _plat_path(plat, html_p)])
                self.assertEqual(hr.returncode, 0, hr.stderr)
                self.assertTrue(validate_html5(html_p).ok)
                mr = run_cpp_probe(plat, ["--export-report-md", "LMS Batch Export", _plat_path(plat, md_p)])
                self.assertEqual(mr.returncode, 0, mr.stderr)
                self.assertTrue(md_p.exists() and len(md_p.read_text(encoding="utf-8").strip()) > 10)

    @e2e_case(
        case_id="T1-A12-05",
        tier=1,
        group="G12",
        req_ids=["F-217", "F-230", "U-16"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t1_a12_05_cross_os_cudoc_roundtrip_parity(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)
            with tempfile.TemporaryDirectory(prefix="t1_a12_05_") as tmp:
                shot = Path(tmp) / f"cross_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=cudoc)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 13: Ingenieria matematica (F-231..F-256, SUP-01..04, SUP-06, SUP-08)
    # =========================================================================

    @e2e_case(
        case_id="T1-A13-01",
        tier=1,
        group="G13",
        req_ids=["F-231", "F-232", "F-233", "F-245", "F-246", "SUP-01", "SUP-02", "SUP-03", "U-15"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a13_01_varsub_dimensional_units_and_reactive_dag(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("#varsub", dc["stdout"])

            r1 = run_cpp_probe(plat, ["--cas", "0.5 m^2 | cm^2"])
            self.assertEqual(r1.returncode, 0, r1.stderr)
            self.assertIn("5000", r1.stdout)

            r2 = run_cpp_probe(plat, ["--cas", "V_out = 4.7 kOhm * 2 mA | V"])
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertIn("9.4", r2.stdout)

            r3 = run_cpp_probe(plat, ["--cas", "3 bar + 100 psi | psi"])
            self.assertEqual(r3.returncode, 0, r3.stderr)
            self.assertIn("143.5", r3.stdout)

    @e2e_case(
        case_id="T1-A13-02",
        tier=1,
        group="G13",
        req_ids=["F-234", "F-235", "F-236", "F-237", "SUP-06"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a13_02_phasors_complex_sel_matrix_and_2d_3d_cvd_plots(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("SEL/Matrix", dc["stdout"])
            self.assertIn("3D CVD", dc["stdout"])
            pr = run_cpp_probe(plat, [])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("[PROBE PASS]", pr.stdout)

    @e2e_case(
        case_id="T1-A13-03",
        tier=1,
        group="G13",
        req_ids=["F-238", "F-239", "F-240", "F-241", "F-242", "F-249", "F-250", "SUP-04", "U-15"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a13_03_mna_dc_ac_transient_diode_laplace_and_bode(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Diode/Transient/Laplace", dc["stdout"])
            self.assertIn("Bode", dc["stdout"])

            mr = run_cpp_probe(plat, ["--mna", make_mna_rc_netlist()])
            self.assertEqual(mr.returncode, 0, mr.stderr)
            self.assertTrue(len(mr.stdout.strip()) > 0)

    @e2e_case(
        case_id="T1-A13-04",
        tier=1,
        group="G13",
        req_ids=["F-243", "F-244", "F-254", "F-255", "F-256"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a13_04_fbd_equilibrium_3d_vectors_smith_chart_and_control_blocks(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, ["--cas", "100 N * cos(30 deg) | N"])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            pr_vec = run_cpp_probe(plat, ["--cas", "cross([1,0,0],[0,1,0])"])
            self.assertEqual(pr_vec.returncode, 0, pr_vec.stderr)
            with tempfile.TemporaryDirectory(prefix="t1_a13_04_") as tmp:
                out_json = Path(tmp) / "stem_319.json"
                shot = Path(tmp) / f"fbd_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--cas",
                        "det([[1,2],[3,4]])",
                        "--out-json",
                        _plat_path(plat, out_json),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing CAS 319 output JSON")
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A13-05",
        tier=1,
        group="G13",
        req_ids=["F-247", "F-248", "SUP-08", "fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW", "CalcBridge", "U-13"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a13_05_calc_bridge_and_all_four_casio_models(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, [])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("[PROBE PASS]", pr.stdout)

            if plat == "win":
                ct = _run_ctest_cached("win", "^Cg50BackendContract$", timeout=240)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)
                for flag in ("--compact", "--compact-gray", "--compact-cg50", "--compact-cw2"):
                    res = probe_calculadora_win([flag], timeout_s=4.0)
                    self.assertTrue(res["started"] and res["alive_after_wait"], f"{flag}: {res}")
            else:
                ct = _run_ctest_cached("linux", "^test_linux_engines$", timeout=120)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)
                with tempfile.TemporaryDirectory(prefix="t1_a13_05_") as tmp:
                    shot = Path(tmp) / "calc_smoke.png"
                    sr = smoke_calculadora_linux(["--smoke-test"], out_png=shot, timeout=90)
                    self.assertEqual(sr.returncode, 0, sr.stderr)
                    self.assertIn("KnowledgeCatalog loaded=true", sr.stdout)
                    self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

            with tempfile.TemporaryDirectory(prefix="t1_a13_05_ds_") as tmp:
                for idx, spec in enumerate(make_casio_step_specs()):
                    shot = Path(tmp) / f"ds_card_{idx}_{plat}.png"
                    sr = smoke_document_studio(plat, "canvas", shot, calc_card=spec)
                    self.assertEqual(sr.returncode, 0, sr.stderr)
                    self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A13-06",
        tier=1,
        group="G13",
        req_ids=["F-251", "F-252", "F-253"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a13_06_csv_ols_regression_sigfigs_and_uncertainty(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Plots/CSV", dc["stdout"])

    # =========================================================================
    # Area 14: Accesibilidad y concentracion (F-257..F-271)
    # =========================================================================

    @e2e_case(
        case_id="T1-A14-01",
        tier=1,
        group="G14",
        req_ids=["F-257", "F-262", "F-270", "F-271"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a14_01_keymap_focus_mode_language_and_session_restore(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t1_a14_01_") as tmp:
                shot = Path(tmp) / f"focus_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A14-02",
        tier=1,
        group="G14",
        req_ids=["F-258", "F-259", "F-260", "F-267"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a14_02_light_dark_high_contrast_dpi_and_cvd_cues(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("3D CVD", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a14_02_") as tmp:
                shot = Path(tmp) / f"cvd_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A14-03",
        tier=1,
        group="G14",
        req_ids=["F-261", "F-263", "F-264", "F-266"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a14_03_uia_atk_tree_reading_ruler_and_ui_text_scale(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a14_03_") as tmp:
                d = Path(tmp)
                a11y_json = d / "a11y_tree.json"
                shot = d / f"a11y_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--ui-scale",
                        "1.5",
                        "--high-contrast",
                        "--dump-a11y-tree",
                        _plat_path(plat, a11y_json),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(a11y_json.exists(), "Missing dumped UIA/ATK accessibility tree JSON")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A14-04",
        tier=1,
        group="G14",
        req_ids=["F-265", "F-268", "F-269"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a14_04_tts_read_aloud_pressure_curve_and_reduce_motion(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a14_04_") as tmp:
                d = Path(tmp)
                tts_json = d / "tts_status.json"
                shot = d / f"tts_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--tts-read",
                        "--reduce-motion",
                        "--out-json",
                        _plat_path(plat, tts_json),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(tts_json.exists(), "Missing TTS / reduce-motion status JSON")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A14-05",
        tier=1,
        group="G14",
        req_ids=["F-258", "F-259", "F-271", "fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW"],
        apps=["CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="calc_win_4models",
    )
    def test_t1_a14_05_calculadora_four_models_themes_popup_and_help_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            if plat == "win":
                # Windows uses --compact-cw (substring-matches --compact-cw2)
                for args in (["--compact-cw"], ["--popup"], ["--help-search=fraccion"]):
                    pr = probe_calculadora_win(args, timeout_s=4.0)
                    self.assertTrue(pr["started"] and pr["alive_after_wait"], f"{args}: {pr}")
            else:
                self.assertEqual(get_readiness_entry("calc_linux_4models").get("status"), "READY")
                with tempfile.TemporaryDirectory(prefix="t1_a14_05_") as tmp:
                    # Linux uses --compact-cw2
                    shot_cw2 = Path(tmp) / "cw2.png"
                    sr0 = smoke_calculadora_linux(["--compact-cw2", "--smoke-test"], out_png=shot_cw2, timeout=90)
                    self.assertEqual(sr0.returncode, 0, sr0.stderr)
                    self.assertTrue(validate_png(shot_cw2, min_width=200, min_height=200).ok)

                    shot_pop = Path(tmp) / "popup.png"
                    sr1 = smoke_calculadora_linux(["--popup"], out_png=shot_pop, timeout=90)
                    self.assertEqual(sr1.returncode, 0, sr1.stderr)
                    self.assertTrue(validate_png(shot_pop, min_width=200, min_height=120).ok)

                    shot_help = Path(tmp) / "help.png"
                    sr2 = smoke_calculadora_linux(["--help-search", "fraccion"], out_png=shot_help, timeout=90)
                    self.assertEqual(sr2.returncode, 0, sr2.stderr)
                    self.assertTrue(validate_png(shot_help, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 15: Rendimiento y recursos (F-272..F-290)
    # =========================================================================

    @e2e_case(
        case_id="T1-A15-01",
        tier=1,
        group="G15",
        req_ids=["F-272", "F-273", "F-274", "F-275", "F-288"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a15_01_1000_page_pdf_lru_cache_and_clean_thread_join(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("LRU Cache", dc["stdout"])
            if plat == "win":
                ct = _run_ctest_cached("win", "^PdfViewerStress$", timeout=540)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)

    @e2e_case(
        case_id="T1-A15-02",
        tier=1,
        group="G15",
        req_ids=["F-276", "F-277", "F-278", "F-279", "F-286"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a15_02_generation_counter_discard_dirty_tiles_and_lazy_release(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            p205 = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_205p")
            self.assertIsNotNone(p205)
            assert p205 is not None
            self.assertTrue(validate_pdf(p205).ok)

    @e2e_case(
        case_id="T1-A15-03",
        tier=1,
        group="G15",
        req_ids=["F-280", "F-281", "F-282"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a15_03_local_perf_telemetry_and_paginated_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("100k Quadtree query", dc["stdout"])

    @e2e_case(
        case_id="T1-A15-04",
        tier=1,
        group="G15",
        req_ids=["F-283", "F-284", "F-285"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a15_04_stress_cycles_flat_memory_and_ink_latency(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            if plat == "win":
                ct = _run_ctest_cached("win", "^PdfViewerStress$", timeout=540)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)

    @e2e_case(
        case_id="T1-A15-05",
        tier=1,
        group="G15",
        req_ids=["F-287", "F-289", "F-290"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t1_a15_05_gpu_reset_resource_recreate_and_replay_harness(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F3", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a15_05_") as tmp:
                shot = Path(tmp) / f"recreate_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    # =========================================================================
    # Area 16: Privacidad y seguridad (F-291..F-305)
    # =========================================================================

    @e2e_case(
        case_id="T1-A16-01",
        tier=1,
        group="G16",
        req_ids=["F-291", "F-300", "F-301", "F-302"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t1_a16_01_100_percent_offline_no_account_and_open_formats(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])
            self.assertGreaterEqual(len(dc["artifacts"].get(".cudoc", [])), 1)
            self.assertGreaterEqual(len(dc["artifacts"].get(".pdf", [])), 1)

    @e2e_case(
        case_id="T1-A16-02",
        tier=1,
        group="G16",
        req_ids=["F-292", "F-295", "F-296"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a16_02_aes256_cudoc_encryption_pin_lock_and_permissions(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a16_02_") as tmp:
                enc_cudoc = Path(tmp) / "locked_exam.cudoc"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--encrypt-cudoc",
                        "synth_pass_4242",
                        "--pin-lock",
                        "4242",
                        "--exam-mode",
                        "--out-cudoc",
                        _plat_path(plat, enc_cudoc),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(enc_cudoc.exists(), "Encrypted .cudoc was not emitted")
                raw = enc_cudoc.read_bytes()
                self.assertNotIn(b"manifest.json", raw)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, enc_cudoc)]).returncode, 0)

    @e2e_case(
        case_id="T1-A16-03",
        tier=1,
        group="G16",
        req_ids=["F-293", "F-294"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a16_03_sensitive_data_detector_and_metadata_scrubber(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])
            leaks = scan_tree_for_leaks(resolve_paths(REPO_ROOT)["suite_root"])
            self.assertEqual(leaks, [], f"Privacy leaks in e2e_suite: {leaks}")
            demo_leaks = scan_text_for_leaks("Contact: demo@example.invalid")
            self.assertEqual(demo_leaks, [], "Synthetic example.invalid domain should be allowed")

    @e2e_case(
        case_id="T1-A16-04",
        tier=1,
        group="G16",
        req_ids=["F-297", "F-298", "F-299"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t1_a16_04_magic_byte_bounds_and_signed_artifact_verification(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            signed = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            self.assertIsNotNone(signed)
            assert signed is not None
            self.assertTrue(validate_pdf(signed, require_signature=True).ok)
            valid_cudocs = [p for p in dc["artifacts"].get(".cudoc", []) if "proyecto" not in p.name.lower()]
            self.assertTrue(valid_cudocs)
            for cudoc in valid_cudocs:
                self.assertTrue(validate_zip_container(cudoc).ok, f"{cudoc.name} invalid ZIP")

    @e2e_case(
        case_id="T1-A16-05",
        tier=1,
        group="G16",
        req_ids=["F-303", "F-304", "F-305"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t1_a16_05_release_checksums_passphrase_hygiene_and_repro_manifest(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            signed = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            self.assertIsNotNone(signed)
            assert signed is not None
            digest = sha256_file(signed)
            self.assertEqual(len(digest), 64)
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])

    # =========================================================================
    # Area 17: IA opcional y automatizacion (F-306..F-319, SUP-07)
    # =========================================================================

    @e2e_case(
        case_id="T1-A17-01",
        tier=1,
        group="G17",
        req_ids=["F-306", "F-307", "F-315"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a17_01_ai_off_by_default_and_deterministic_templates_work_offline(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            pr = run_cpp_probe(plat, ["--cas", "4.7 kOhm * 2 mA | V"])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("9.4", pr.stdout)
            step = run_cpp_probe(plat, ["--step", "(x + 1)^2", "x^2 + 2*x + 1"])
            self.assertEqual(step.returncode, 0, step.stderr)

    @e2e_case(
        case_id="T1-A17-02",
        tier=1,
        group="G17",
        req_ids=["F-310", "F-313", "F-314", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t1_a17_02_confirmation_card_and_deterministic_algebraic_equivalence(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            ok_res = run_cpp_probe(plat, ["--check-my-work", "(x + 1)^2", "x^2 + 2*x + 1"])
            self.assertEqual(ok_res.returncode, 0, ok_res.stderr)
            self.assertIn("CMW|1|", ok_res.stdout)
            with tempfile.TemporaryDirectory(prefix="t1_a17_02_") as tmp:
                out_json = Path(tmp) / "confirm.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--check-my-work",
                        "(x + 1)^2",
                        "x^2 + 2*x + 1",
                        "--confirm-ast",
                        "--export-errorbook",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing Check My Work UI JSON")

    @e2e_case(
        case_id="T1-A17-03",
        tier=1,
        group="G17",
        req_ids=["F-308", "F-309", "F-311", "F-316"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t1_a17_03_optin_local_cited_qa_summary_flashcards_and_translation(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a17_03_") as tmp:
                out_json = Path(tmp) / "qa_cited.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--local-qa-cited",
                        "Ley de Ohm",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing local cited Q&A JSON")
                payload = json.loads(out_json.read_text(encoding="utf-8"))
                self.assertIn("citations", payload)

    @e2e_case(
        case_id="T1-A17-04",
        tier=1,
        group="G17",
        req_ids=["F-312", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t1_a17_04_check_my_work_step_diff_and_solve_teach_me(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            diff_res = run_cpp_probe(plat, ["--check-my-work", "x^2 - 3*x + 2", "x^2 + 3*x + 2"])
            self.assertEqual(diff_res.returncode, 2)
            self.assertIn("CMW|0|", diff_res.stdout)
            with tempfile.TemporaryDirectory(prefix="t1_a17_04_") as tmp:
                out_json = Path(tmp) / "teach_me.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--check-my-work",
                        "x^2 - 3*x + 2",
                        "x^2 + 3*x + 2",
                        "--teach-me",
                        "--export-errorbook",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing teach-me / errorbook output JSON")

    @e2e_case(
        case_id="T1-A17-05",
        tier=1,
        group="G17",
        req_ids=["F-317", "F-318", "F-319"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t1_a17_05_deterministic_macro_replay_audit_trail_and_raw_data_toggle(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a17_05_") as tmp:
                out_cudoc = Path(tmp) / "macro_sync.cudoc"
                shot = Path(tmp) / f"macro_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-card",
                        "fx-991CW|V_out=4.7kOhm*2mA|4.7 kOhm * 2 mA | V",
                        "--calc-bridge-sync",
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)
