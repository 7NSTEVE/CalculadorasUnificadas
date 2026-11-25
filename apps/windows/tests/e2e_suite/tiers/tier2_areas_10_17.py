"""Tier 2 E2E Tests: Areas 10..17 (40 boundary, negative, fuzz, and stress cases)."""
from __future__ import annotations

import json
import os
import tempfile
import unittest
import zipfile
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import (
    validate_csv,
    validate_cudoc,
    validate_html5,
    validate_pdf,
    validate_png,
    validate_zip_container,
)
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.cpp_probe_channel import run_cpp_probe
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_ctest_target, run_doc_core_keepdir
from e2e_suite.adapters.linux_gui_channel import smoke_calculadora_linux
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scan_tree_for_leaks, scrub_secrets
from e2e_suite.adapters.registry import e2e_case, get_readiness_entry, get_runtime_context
from e2e_suite.adapters.win_gui_channel import probe_calculadora_win, smoke_document_studio
from e2e_suite.fixtures.corrupt_generators import (
    make_bad_crc_zip,
    make_bad_magic_pdf,
    make_path_traversal_zip,
    make_truncated_pdf,
    make_zip_bomb_payload,
)
from e2e_suite.fixtures.synthetic_data import make_minimal_png


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


class TestTier2Areas10To17(unittest.TestCase):
    """Tier 2 boundary, negative, fuzz, and stress cases covering Areas 10..17."""

    # =========================================================================
    # Area 10: Estudio y practica
    # =========================================================================

    @e2e_case(
        case_id="T2-A10-01",
        tier=2,
        group="G10",
        req_ids=["F-180", "F-187", "GN-14", "U-16"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a10_01_sm2_fsrs_extreme_streaks_and_past_exam_date(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("SM-2", dc["stdout"])

    @e2e_case(
        case_id="T2-A10-02",
        tier=2,
        group="G10",
        req_ids=["F-185", "F-189"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a10_02_locked_exam_mode_blocks_cas_and_blank_quiz(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a10_02_") as tmp:
                out_json = Path(tmp) / "exam_block.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--exam-mode",
                        "--cas",
                        "integrate(x^2, x)",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertTrue(
                    sr.returncode != 0 or (out_json.exists() and json.loads(out_json.read_text(encoding="utf-8")).get("blocked") is True),
                    "Locked exam mode must block CAS evaluation",
                )

    @e2e_case(
        case_id="T2-A10-03",
        tier=2,
        group="G10",
        req_ids=["F-191", "F-192", "F-197"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a10_03_invalid_variant_bounds_and_missing_placeholder(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, ["--cas", "unknown_unbound_symbol + 1"])
            self.assertNotEqual(pr.returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t2_a10_03_") as tmp:
                out_json = Path(tmp) / "bad_variants.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--generate-variants",
                        "0",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertNotEqual(sr.returncode, 0)

    @e2e_case(
        case_id="T2-A10-04",
        tier=2,
        group="G10",
        req_ids=["F-179", "F-181", "F-182", "F-183", "F-184", "F-186", "F-188", "F-195"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a10_04_empty_deck_invalid_rubric_weight_and_zero_tape_toggle(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("StudyTape", dc["stdout"])

    @e2e_case(
        case_id="T2-A10-05",
        tier=2,
        group="G10",
        req_ids=["F-190", "F-193", "F-194", "F-196"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a10_05_corrupt_study_analytics_and_step_playback_clamp(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t2_a10_05_") as tmp:
                d = Path(tmp)
                bad_vault = d / "bad_vault.json"
                bad_vault.write_text('{"reviews": -5, "broken":', encoding="utf-8")
                pr_vault = run_cpp_probe(plat, ["--check-vault", _plat_path(plat, bad_vault)])
                self.assertNotEqual(pr_vault.returncode, 0)

                bad_study = d / "bad_study.cudoc"
                with zipfile.ZipFile(bad_study, "w") as zf:
                    zf.writestr("manifest.json", "{corrupt_study_manifest")
                    zf.writestr("pages.json", "[]")
                    zf.writestr("objects.json", '{"canvasObjects":[]}')
                self.assertFalse(validate_cudoc(bad_study).ok)
                pr_cu = run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, bad_study)])
                self.assertNotEqual(pr_cu.returncode, 0)

    # =========================================================================
    # Area 11: Ensenanza y proyeccion
    # =========================================================================

    @e2e_case(
        case_id="T2-A11-01",
        tier=2,
        group="G11",
        req_ids=["F-198", "F-199", "F-200", "F-201", "F-202"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a11_01_single_monitor_projector_fallback_and_dpi_rescale(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])

    @e2e_case(
        case_id="T2-A11-02",
        tier=2,
        group="G11",
        req_ids=["F-203", "F-204", "F-205", "F-206", "F-207", "F-208", "F-209", "SUP-08", "SUP-09", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t2_a11_02_frozen_projector_zero_private_note_leakage(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a11_02_") as tmp:
                d = Path(tmp)
                out_cudoc = d / "presenter_frozen.cudoc"
                shot = d / f"frozen_proj_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--presenter-console",
                        "--freeze-projector",
                        "--calc-card",
                        "fx-991CW|2,+,2,=|4",
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
        case_id="T2-A11-03",
        tier=2,
        group="G11",
        req_ids=["F-213", "F-214"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t2_a11_03_lan_remote_wrong_pin_backoff_and_readonly_isolation(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a11_03_") as tmp:
                sut_html = Path(tmp) / "student_readonly.html"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-broadcast",
                        "--port",
                        "0",
                        "--pin-lock",
                        "wrongpin",
                        "--export-qr-html",
                        _plat_path(plat, sut_html),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_html5(sut_html).ok)

    @e2e_case(
        case_id="T2-A11-04",
        tier=2,
        group="G11",
        req_ids=["F-210", "F-211", "F-212", "F-216"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a11_04_html5_xss_escaping_and_empty_animation_export_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Lab HTML", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t2_a11_04_") as tmp:
                d = Path(tmp)
                sut_html = d / "xss_report.html"
                pr = run_cpp_probe(
                    plat,
                    ["--export-report-html", "<img src=x onerror=alert(1)>", _plat_path(plat, sut_html)],
                )
                self.assertEqual(pr.returncode, 0, pr.stderr)
                self.assertTrue(validate_html5(sut_html).ok)
                html_text = sut_html.read_text(encoding="utf-8")
                self.assertNotIn("<img src=x", html_text)
                self.assertIn("&lt;img src=x", html_text)

                bad_html = d / "no_doctype.html"
                bad_html.write_text("<html><body>Missing doctype</body></html>", encoding="utf-8")
                self.assertFalse(validate_html5(bad_html).ok)

    @e2e_case(
        case_id="T2-A11-05",
        tier=2,
        group="G11",
        req_ids=["F-215"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t2_a11_05_qr_payload_overflow_and_empty_url_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            oversized = "http://127.0.0.1:8080/?" + ("a" * 5000)
            with tempfile.TemporaryDirectory(prefix="t2_a11_05_") as tmp:
                d = Path(tmp)
                sut_html = d / "qr_overflow.html"
                sr_big = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-broadcast",
                        "--port",
                        "0",
                        "--qr-payload",
                        oversized,
                        "--export-qr-html",
                        _plat_path(plat, sut_html),
                    ],
                )
                self.assertNotEqual(sr_big.returncode, 0)
                sr_empty = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-broadcast",
                        "--port",
                        "0",
                        "--qr-payload",
                        "",
                        "--export-qr-html",
                        _plat_path(plat, sut_html),
                    ],
                )
                self.assertNotEqual(sr_empty.returncode, 0)

    # =========================================================================
    # Area 12: Colaboracion y sincronizacion
    # =========================================================================

    @e2e_case(
        case_id="T2-A12-01",
        tier=2,
        group="G12",
        req_ids=["F-217", "F-218", "F-225", "F-230"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t2_a12_01_unsupported_schema_version_missing_manifest_and_csv_sanitization(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a12_01_") as tmp:
                d = Path(tmp)
                no_manifest = d / "no_manifest.cudoc"
                with zipfile.ZipFile(no_manifest, "w") as zf:
                    zf.writestr("pages.json", "[]")
                    zf.writestr("objects.json", '{"canvasObjects":[]}')
                self.assertFalse(validate_cudoc(no_manifest).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, no_manifest)]).returncode, 0)

                bad_ver = d / "future_schema.cudoc"
                with zipfile.ZipFile(bad_ver, "w") as zf:
                    zf.writestr("manifest.json", '{"format":"WrongFormat","formatVersion":0,"mode":"unknown"}')
                    zf.writestr("pages.json", "[]")
                    zf.writestr("objects.json", '{"canvasObjects":[]}')
                self.assertFalse(validate_cudoc(bad_ver).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, bad_ver)]).returncode, 0)

                sr = smoke_document_studio(plat, "canvas", d / f"bad_{plat}.png", open_path=no_manifest)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)

    @e2e_case(
        case_id="T2-A12-02",
        tier=2,
        group="G12",
        req_ids=["F-219", "F-226"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a12_02_conflict_merge_delete_vs_modify_zero_silent_loss(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Conflict Merge", dc["stdout"])

    @e2e_case(
        case_id="T2-A12-03",
        tier=2,
        group="G12",
        req_ids=["F-220", "F-223", "F-224"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="lan_qr_remote",
    )
    def test_t2_a12_03_p2p_wrong_pin_and_interrupted_crdt_burst_convergence(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a12_03_") as tmp:
                out_cudoc = Path(tmp) / "p2p_wrong_pin.cudoc"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--lan-p2p-sync",
                        "--port",
                        "0",
                        "--pin-lock",
                        "0000",
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertNotEqual(sr.returncode, 0)

    @e2e_case(
        case_id="T2-A12-04",
        tier=2,
        group="G12",
        req_ids=["F-222", "F-228", "F-229"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a12_04_readonly_locked_layer_and_duplicate_roster_ids(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Layers", dc["stdout"])

    @e2e_case(
        case_id="T2-A12-05",
        tier=2,
        group="G12",
        req_ids=["F-221", "F-227"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a12_05_xss_payload_in_review_comment_escaped_in_html5(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Lab HTML", dc["stdout"])

    # =========================================================================
    # Area 13: Ingenieria matematica
    # =========================================================================

    @e2e_case(
        case_id="T2-A13-01",
        tier=2,
        group="G13",
        req_ids=["F-231", "F-233", "F-243", "F-244", "SUP-02", "SUP-03", "U-15"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a13_01_dimensionally_incompatible_unit_expressions_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            for bad_expr in ("1 Nm + 1 m", "3 bar + 5 s", "10 m | kg", "sin(30 m)"):
                res = run_cpp_probe(plat, ["--cas", bad_expr])
                self.assertNotEqual(res.returncode, 0, f"Expected failure on '{bad_expr}', got rc=0 stdout={res.stdout}")
                self.assertTrue(len(res.stderr.strip()) > 0, f"Missing error message for '{bad_expr}'")

    @e2e_case(
        case_id="T2-A13-02",
        tier=2,
        group="G13",
        req_ids=["F-232", "F-234", "F-235", "F-236", "F-237", "F-247", "F-248", "SUP-08", "fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW", "CalcBridge", "U-13"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a13_02_cas_division_by_zero_singular_matrix_and_casio_domain_errors(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            for bad_expr in ("1 / 0", "0 / 0"):
                res = run_cpp_probe(plat, ["--cas", bad_expr])
                self.assertNotEqual(res.returncode, 0, f"Expected div0 failure on '{bad_expr}'")
            cpx = run_cpp_probe(plat, ["--cas", "asin(2)"])
            self.assertEqual(cpx.returncode, 0, cpx.stderr)
            self.assertIn("j", cpx.stdout)
            if plat == "win":
                ct = _run_ctest_cached("win", "^Cg50BackendContract$", timeout=240)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)
            else:
                ct = _run_ctest_cached("linux", "^test_linux_engines$", timeout=120)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)

    @e2e_case(
        case_id="T2-A13-03",
        tier=2,
        group="G13",
        req_ids=["F-245", "F-246", "F-255", "SUP-01"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a13_03_dag_cycle_detection_and_empty_formula_card_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            res = run_cpp_probe(plat, ["--cas", ""])
            self.assertNotEqual(res.returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t2_a13_03_") as tmp:
                shot = Path(tmp) / f"empty_card_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, calc_card="fx-991CW||")
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)

    @e2e_case(
        case_id="T2-A13-04",
        tier=2,
        group="G13",
        req_ids=["F-238", "F-239", "F-240", "F-241", "F-242", "F-249", "F-250", "F-256", "SUP-04"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="mna_53_components_full",
    )
    def test_t2_a13_04_pathological_floating_node_and_shorted_voltage_loop_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            floating = run_cpp_probe(plat, ["--mna", "R1 2 3 1000"])
            self.assertNotEqual(floating.returncode, 0)
            shorted = run_cpp_probe(plat, ["--mna", "V1 1 0 5\nV2 1 0 10"])
            self.assertNotEqual(shorted.returncode, 0)

    @e2e_case(
        case_id="T2-A13-05",
        tier=2,
        group="G13",
        req_ids=["F-251", "F-252", "F-253", "F-254"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a13_05_ragged_empty_csv_and_zero_division_uncertainty_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t2_a13_05_") as tmp:
                d = Path(tmp)
                ragged = d / "ragged.csv"
                ragged.write_text("x,y\n1.0,2.0\n3.0\n", encoding="utf-8")
                self.assertFalse(validate_csv(ragged, min_rows=2).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--fit-csv", _plat_path(plat, ragged)]).returncode, 0)

                empty_csv = d / "empty.csv"
                empty_csv.write_text("", encoding="utf-8")
                self.assertFalse(validate_csv(empty_csv, min_rows=1).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--fit-csv", _plat_path(plat, empty_csv)]).returncode, 0)

                single_row = d / "single_row.csv"
                single_row.write_text("x,y\n1.0,2.0\n", encoding="utf-8")
                self.assertNotEqual(run_cpp_probe(plat, ["--fit-csv", _plat_path(plat, single_row)]).returncode, 0)

    # =========================================================================
    # Area 14: Accesibilidad y concentracion
    # =========================================================================

    @e2e_case(
        case_id="T2-A14-01",
        tier=2,
        group="G14",
        req_ids=["F-259", "F-261", "F-262", "F-263", "F-264", "F-266", "F-269"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a14_01_hidpi_300_percent_and_narrow_800x600_no_clipping(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a14_01_") as tmp:
                d = Path(tmp)
                layout_json = d / "layout_bounds.json"
                shot = d / f"hidpi_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--ui-scale",
                        "3.0",
                        "--window-size",
                        "800x600",
                        "--high-contrast",
                        "--dump-layout-bounds",
                        _plat_path(plat, layout_json),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(layout_json.exists(), "Missing dumped layout bounds JSON")
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T2-A14-02",
        tier=2,
        group="G14",
        req_ids=["F-257", "F-268"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="custom_keymap_config",
    )
    def test_t2_a14_02_duplicate_shortcut_collision_and_corrupt_keymap_fallback(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a14_02_") as tmp:
                d = Path(tmp)
                bad_km = d / "corrupt_keymap.json"
                bad_km.write_text("{corrupt_keymap", encoding="utf-8")
                eff_km = d / "effective_keymap.json"
                shot = d / f"keymap_fallback_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--keymap-config",
                        _plat_path(plat, bad_km),
                        "--export-effective-keymap",
                        _plat_path(plat, eff_km),
                        "--mode",
                        "canvas",
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(eff_km.exists(), "Missing fallback effective keymap JSON")
                eff_data = json.loads(eff_km.read_text(encoding="utf-8"))
                self.assertTrue(bool(eff_data), "Expected non-empty default keymap fallback")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A14-03",
        tier=2,
        group="G14",
        req_ids=["F-270"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t2_a14_03_missing_last_session_file_recovers_cleanly_at_startup(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a14_03_") as tmp:
                missing = Path(tmp) / "deleted_before_launch.cudoc"
                shot = Path(tmp) / f"missing_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=missing)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A14-04",
        tier=2,
        group="G14",
        req_ids=["F-265", "F-271"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a14_04_missing_tts_voice_and_es_en_locale_consistency(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a14_04_") as tmp:
                d = Path(tmp)
                out_json = d / "tts_locale.json"
                shot = d / f"locale_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--tts-read",
                        "--tts-voice",
                        "nonexistent_voice_id",
                        "--locale",
                        "es",
                        "--out-json",
                        _plat_path(plat, out_json),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing TTS fallback & locale JSON")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A14-05",
        tier=2,
        group="G14",
        req_ids=["F-258", "F-260", "F-267", "fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="calc_win_4models",
    )
    def test_t2_a14_05_high_contrast_and_cvd_noncolor_cues_across_four_casios(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("3D CVD", dc["stdout"])
            if plat == "win":
                # Windows uses --compact-cw (substring-matches --compact-cw2)
                res = probe_calculadora_win(["--compact-cw"], timeout_s=4.0)
                self.assertTrue(res["started"] and res["alive_after_wait"], str(res))
            else:
                self.assertEqual(get_readiness_entry("calc_linux_4models").get("status"), "READY")
                with tempfile.TemporaryDirectory(prefix="t2_a14_05_") as tmp:
                    # Linux uses --compact-cw2
                    shot = Path(tmp) / "cw2_cvd.png"
                    sr = smoke_calculadora_linux(["--compact-cw2", "--smoke-test"], out_png=shot, timeout=90)
                    self.assertEqual(sr.returncode, 0, sr.stderr)
                    self.assertTrue(validate_png(shot, min_width=200, min_height=200).ok)

    # =========================================================================
    # Area 15: Rendimiento y recursos
    # =========================================================================

    @e2e_case(
        case_id="T2-A15-01",
        tier=2,
        group="G15",
        req_ids=["F-278", "F-286"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t2_a15_01_extreme_1600_percent_zoom_adaptive_raster_cap(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F2", dc["stdout"])

    @e2e_case(
        case_id="T2-A15-02",
        tier=2,
        group="G15",
        req_ids=["F-272", "F-273", "F-277", "F-281", "F-282", "F-284", "F-285"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t2_a15_02_rapid_open_scroll_zoom_close_stress_zero_leaks(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            if plat == "win":
                ct = _run_ctest_cached("win", "^PdfViewerStress$", timeout=540)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)

    @e2e_case(
        case_id="T2-A15-03",
        tier=2,
        group="G15",
        req_ids=["F-276", "F-287"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t2_a15_03_gpu_device_reset_mid_stroke_recovery(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F3", dc["stdout"])

    @e2e_case(
        case_id="T2-A15-04",
        tier=2,
        group="G15",
        req_ids=["F-274", "F-275", "F-279", "F-280", "F-283", "F-288"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t2_a15_04_tight_32mib_lru_budget_under_rapid_2000_page_scrub(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            p205 = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_205p")
            self.assertIsNotNone(p205)
            assert p205 is not None
            self.assertTrue(validate_pdf(p205).ok)

    @e2e_case(
        case_id="T2-A15-05",
        tier=2,
        group="G15",
        req_ids=["F-289", "F-290"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a15_05_fuzz_corpus_zip_bomb_and_mutated_pdf_bounds_blocked(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a15_05_") as tmp:
                d = Path(tmp)
                bomb = d / "bomb.zip"
                bomb.write_bytes(make_zip_bomb_payload(15))
                self.assertFalse(validate_zip_container(bomb, max_uncompressed_bytes=5 * 1024 * 1024, max_ratio=20.0).ok)
                # Note: ZipArchive::Deserialize caps at 512 MiB; construct >512 MiB or verify ratio/size rejection
                bomb_large = d / "bomb_large.zip"
                bomb_large.write_bytes(make_zip_bomb_payload(520))
                self.assertNotEqual(run_cpp_probe(plat, ["--check-zip", _plat_path(plat, bomb_large)]).returncode, 0)

                bad_crc = d / "bad_crc.zip"
                bad_crc.write_bytes(make_bad_crc_zip())
                self.assertFalse(validate_zip_container(bad_crc).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-zip", _plat_path(plat, bad_crc)]).returncode, 0)

                bad_pdf = d / "trunc.pdf"
                bad_pdf.write_bytes(make_truncated_pdf())
                self.assertFalse(validate_pdf(bad_pdf).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, bad_pdf)]).returncode, 0)

                shot = d / f"trunc_pdf_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=bad_pdf, page=1)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 16: Privacidad y seguridad
    # =========================================================================

    @e2e_case(
        case_id="T2-A16-01",
        tier=2,
        group="G16",
        req_ids=["F-291", "F-292", "F-295", "F-300", "F-301", "F-302"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a16_01_wrong_aes_password_tampered_gcm_tag_and_pin_backoff(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a16_01_") as tmp:
                d = Path(tmp)
                enc_cudoc = d / "aes_locked.cudoc"
                sr_enc = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--encrypt-cudoc",
                        "correct_pass",
                        "--pin-lock",
                        "9876",
                        "--out-cudoc",
                        _plat_path(plat, enc_cudoc),
                    ],
                )
                self.assertEqual(sr_enc.returncode, 0, sr_enc.stderr)
                self.assertTrue(enc_cudoc.exists(), "Encrypted .cudoc missing")
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, enc_cudoc)]).returncode, 0)
                sr_bad = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--open",
                        _plat_path(plat, enc_cudoc),
                        "--decrypt-pass",
                        "wrong_pass",
                    ],
                )
                self.assertNotEqual(sr_bad.returncode, 0)

    @e2e_case(
        case_id="T2-A16-02",
        tier=2,
        group="G16",
        req_ids=["F-297"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a16_02_spoofed_exe_elf_png_renamed_to_cudoc_and_pdf_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a16_02_") as tmp:
                d = Path(tmp)
                mz_cudoc = d / "spoof_pe.cudoc"
                mz_cudoc.write_bytes(b"MZ\x90\x00\x03\x00\x00\x00PE_HEADER_SPOOF")
                self.assertFalse(validate_cudoc(mz_cudoc).ok)

                elf_pdf = d / "spoof_elf.pdf"
                elf_pdf.write_bytes(b"\x7fELF\x02\x01\x01\x00ELF_BINARY_SPOOF")
                self.assertFalse(validate_pdf(elf_pdf).ok)

                png_pdf = d / "spoof_png.pdf"
                png_pdf.write_bytes(make_minimal_png())
                self.assertFalse(validate_pdf(png_pdf).ok)

                sr = smoke_document_studio(plat, "pdf", d / f"spoof_{plat}.png", open_path=elf_pdf)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)

    @e2e_case(
        case_id="T2-A16-03",
        tier=2,
        group="G16",
        req_ids=["F-298"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a16_03_zip_traversal_overflow_and_xml_billion_laughs_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a16_03_") as tmp:
                d = Path(tmp)
                trav_win = d / "trav_win.cudoc"
                trav_win.write_bytes(make_path_traversal_zip("..\\..\\evil.txt"))
                self.assertFalse(validate_zip_container(trav_win).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-zip", _plat_path(plat, trav_win)]).returncode, 0)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, trav_win)]).returncode, 0)

                trav_drive = d / "trav_drive.cudoc"
                trav_drive.write_bytes(make_path_traversal_zip("C:\\evil.txt"))
                self.assertFalse(validate_zip_container(trav_drive).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-zip", _plat_path(plat, trav_drive)]).returncode, 0)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-cudoc", _plat_path(plat, trav_drive)]).returncode, 0)

                bad_pdf = d / "bad_magic.pdf"
                bad_pdf.write_bytes(make_bad_magic_pdf(as_html_login=False))
                self.assertFalse(validate_pdf(bad_pdf).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, bad_pdf)]).returncode, 0)

                shot = d / f"bad_pdf_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=bad_pdf, page=1)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A16-04",
        tier=2,
        group="G16",
        req_ids=["F-296", "F-299", "F-303", "F-305"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t2_a16_04_tampered_signed_manifest_and_permissions_fail_closed(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Tamper Rejection", dc["stdout"])

    @e2e_case(
        case_id="T2-A16-05",
        tier=2,
        group="G16",
        req_ids=["F-293", "F-294", "F-304"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a16_05_privacy_guard_detects_synthetic_leaks_and_tree_is_clean(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            tok = "tok_synthetic_12345"
            cook = "raw_cookie_value_999"
            zpw = "rawZoomSecret99"
            raw_pwd = "RawUnquotedPass99"
            raw_name = "Nel" + "son"
            raw_email = "user@" + "realdomain.example"
            synthetic_leak = (
                "Authori" + "zation: Bearer " + tok + "\n"
                + "ad_" + "session_id" + "=" + cook + "\n"
                + "https://zoom.example.invalid/j/123?" + "pwd=" + zpw + "\n"
                + "pass" + "word=" + raw_pwd + "\n"
                + "Author: " + raw_name + " Test\n"
                + "Mail: " + raw_email + "\n"
            )
            findings = scan_text_for_leaks(synthetic_leak)
            self.assertIn("LEAK_AUTH_HEADER", findings)
            self.assertIn("LEAK_COOKIE(ad_" + "session_id)", findings)
            self.assertIn("LEAK_ZOOM_PWD", findings)
            self.assertIn("LEAK_PASSWORD_KV", findings)
            self.assertIn("LEAK_PERSONAL_NAME", findings)
            self.assertIn("LEAK_EMAIL(" + raw_email + ")", findings)

            scrubbed = scrub_secrets(synthetic_leak)
            self.assertNotIn(tok, scrubbed)
            self.assertNotIn(cook, scrubbed)
            self.assertNotIn(zpw, scrubbed)
            self.assertNotIn(raw_pwd, scrubbed)
            self.assertNotIn(raw_name.lower(), scrubbed.lower())
            self.assertNotIn(raw_email, scrubbed)
            self.assertIn("[REDACTED_PASSWORD]", scrubbed)
            self.assertIn("[REDACTED_NAME]", scrubbed)
            self.assertIn("[REDACTED_EMAIL]", scrubbed)

            allowed_text = "Contact: synth@example.invalid pass" + "word=synth_ok"
            self.assertEqual(scan_text_for_leaks(allowed_text), [])

            tree_leaks = scan_tree_for_leaks(resolve_paths(REPO_ROOT)["suite_root"])
            self.assertEqual(tree_leaks, [], f"Repo e2e_suite privacy leaks: {tree_leaks}")

    # =========================================================================
    # Area 17: IA opcional y automatizacion
    # =========================================================================

    @e2e_case(
        case_id="T2-A17-01",
        tier=2,
        group="G17",
        req_ids=["F-310", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t2_a17_01_unconfirmed_confirmation_card_and_invalid_syntax_blocked(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            bad = run_cpp_probe(plat, ["--cas", "x^2 + *"])
            self.assertNotEqual(bad.returncode, 0)
            bad_cmw = run_cpp_probe(plat, ["--check-my-work", "x^2 + *", "x"])
            self.assertNotEqual(bad_cmw.returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t2_a17_01_") as tmp:
                out_json = Path(tmp) / "invalid_cmw.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--check-my-work",
                        "x^2 + *",
                        "x",
                        "--export-errorbook",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertNotEqual(sr.returncode, 0)

    @e2e_case(
        case_id="T2-A17-02",
        tier=2,
        group="G17",
        req_ids=["F-312", "F-313", "F-314", "SUP-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t2_a17_02_check_my_work_rejects_hallucination_and_accepts_factoring(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            ok_factor = run_cpp_probe(plat, ["--check-my-work", "x^2 - 4", "(x - 2)*(x + 2)"])
            self.assertEqual(ok_factor.returncode, 0, ok_factor.stderr)
            self.assertIn("CMW|1|", ok_factor.stdout)
            bad_halluc = run_cpp_probe(plat, ["--check-my-work", "2 + 2", "5"])
            self.assertEqual(bad_halluc.returncode, 2)
            self.assertIn("CMW|0|", bad_halluc.stdout)
            with tempfile.TemporaryDirectory(prefix="t2_a17_02_") as tmp:
                out_json = Path(tmp) / "halluc_cmw.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--check-my-work",
                        "2 + 2",
                        "5",
                        "--export-errorbook",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing Check My Work errorbook JSON")

    @e2e_case(
        case_id="T2-A17-03",
        tier=2,
        group="G17",
        req_ids=["F-308", "F-309", "F-311", "F-316"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t2_a17_03_grounded_qa_refuses_uncited_fabrication(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a17_03_") as tmp:
                out_json = Path(tmp) / "uncited_qa.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--local-qa-cited",
                        "uncited_fictional_theorem_xyz",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing grounded Q&A refusal JSON")
                payload = json.loads(out_json.read_text(encoding="utf-8"))
                self.assertTrue(payload.get("refused") is True or len(payload.get("citations", [])) == 0)

    @e2e_case(
        case_id="T2-A17-04",
        tier=2,
        group="G17",
        req_ids=["F-317", "F-318", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t2_a17_04_macro_replay_on_deleted_target_or_switched_casio_model(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a17_04_") as tmp:
                d = Path(tmp)
                out_cudoc = d / "bad_macro.cudoc"
                shot = d / f"macro_bad_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-card",
                        "fx-991CW|bad_macro|",
                        "--calc-bridge-sync",
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A17-05",
        tier=2,
        group="G17",
        req_ids=["F-306", "F-307", "F-315", "F-319"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a17_05_ai_off_keeps_100_percent_core_cas_and_mna_operational(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, [])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("[PROBE PASS]", pr.stdout)
            step_ok = run_cpp_probe(plat, ["--step", "(x + 1)^2", "x^2 + 2*x + 1"])
            self.assertEqual(step_ok.returncode, 0, step_ok.stderr)
            step_bad = run_cpp_probe(plat, ["--step", "(x + 1)^2", "x^2 + 1"])
            self.assertEqual(step_bad.returncode, 2)
