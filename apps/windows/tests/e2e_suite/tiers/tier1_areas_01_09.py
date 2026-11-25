"""Tier 1 E2E Tests: Areas 01..09 (49 happy-path isolated feature cases)."""
from __future__ import annotations

import json
import os
import shutil
import struct
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import (
    sha256_file,
    validate_cudoc,
    validate_html5,
    validate_ooxml_docx,
    validate_ooxml_pptx,
    validate_ooxml_xlsx,
    validate_pdf,
    validate_png,
    validate_svg,
    validate_wav_pcm,
)
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.cpp_probe_channel import ensure_cpp_probe, run_cpp_probe
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_ctest_target, run_doc_core_keepdir
from e2e_suite.adapters.linux_gui_channel import smoke_calculadora_linux
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.adapters.win_gui_channel import probe_calculadora_win, smoke_document_studio
from e2e_suite.fixtures.synthetic_data import (
    make_minimal_docx,
    make_minimal_pdf,
    make_minimal_png,
    make_minimal_wav,
)


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


def _find_artifact(artifacts: dict[str, list[Path]], ext: str, *name_fragments: str) -> Path | None:
    for p in artifacts.get(ext, []):
        low = p.name.lower()
        if all(frag.lower() in low for frag in name_fragments):
            return p
    return None


class TestTier1Areas01To09(unittest.TestCase):
    """Tier 1 isolated feature checks covering Areas 01..09."""

    # =========================================================================
    # Area 01: Integridad y fiabilidad (F-001..F-015, GN-12)
    # =========================================================================

    @e2e_case(
        case_id="T1-A01-01",
        tier=1,
        group="G01",
        req_ids=["F-001", "F-003", "F-010", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t1_a01_01_cudoc_storage_and_rotating_backup(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F1", dc["stdout"])

            cudocs = dc["artifacts"].get(".cudoc", [])
            baks = dc["artifacts"].get(".bak", [])
            self.assertTrue(cudocs, "No .cudoc files produced in keepDir")
            self.assertTrue(baks, "No rotating .bak files produced in keepDir")

            primary = _find_artifact(dc["artifacts"], ".cudoc", "f11_full") or _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(primary, "No valid primary .cudoc produced in keepDir")
            assert primary is not None
            res = validate_cudoc(primary)
            self.assertTrue(res.ok, f"Invalid primary .cudoc: {res.errors}")

            bak_res = validate_cudoc(baks[0])
            self.assertTrue(bak_res.ok, f"Invalid backup .cudoc.bak: {bak_res.errors}")

            with tempfile.TemporaryDirectory(prefix="t1_a01_01_") as tmp:
                restored = Path(tmp) / "restored.cudoc"
                shutil.copy2(baks[0], restored)
                self.assertEqual(sha256_file(baks[0]), sha256_file(restored))
                shot = Path(tmp) / f"open_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=restored)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])

    @e2e_case(
        case_id="T1-A01-02",
        tier=1,
        group="G01",
        req_ids=["F-002", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t1_a01_02_wal_atomic_crash_recovery(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            wal = _find_artifact(dc["artifacts"], ".cudoc-wal", "f9_notebook")
            self.assertIsNotNone(cudoc, "Missing f9_notebook.cudoc")
            self.assertIsNotNone(wal, "Missing f9_notebook.cudoc-wal")
            assert cudoc is not None and wal is not None

            self.assertTrue(validate_cudoc(cudoc).ok, "f9_notebook.cudoc invalid")

            raw = wal.read_bytes()
            self.assertTrue(raw.startswith(b"CWAL"))
            body_len, body_crc = struct.unpack("<II", raw[4:12])
            body = raw[12 : 12 + body_len]
            self.assertEqual(zlib.crc32(body) & 0xFFFFFFFF, body_crc)
            parsed = json.loads(body.decode("utf-8"))
            self.assertIn("op", parsed)
            vres = validate_cudoc(cudoc, wal_path=wal)
            self.assertFalse(vres.ok)
            self.assertGreaterEqual(vres.details.get("wal_frames", 0), 2)

    @e2e_case(
        case_id="T1-A01-03",
        tier=1,
        group="G01",
        req_ids=["F-004"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_command_stack",
    )
    def test_t1_a01_03_persisted_command_stack_undo_boundary(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("CommandStack", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])
            pr = run_cpp_probe(plat, [])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("[PROBE PASS]", pr.stdout)

    @e2e_case(
        case_id="T1-A01-04",
        tier=1,
        group="G01",
        req_ids=["F-005", "F-007"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a01_04_unicode_bmp_smp_roundtrip_and_asset_hashes(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            uni_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            rec_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_recovered")
            self.assertIsNotNone(uni_pdf, "Missing f8_unicode_math.pdf")
            self.assertIsNotNone(rec_pdf, "Missing f8_recovered_from_cudoc.pdf")
            assert uni_pdf is not None and rec_pdf is not None

            p_res = validate_pdf(uni_pdf, require_tounicode=True)
            self.assertTrue(p_res.ok, p_res.errors)
            self.assertTrue(validate_pdf(rec_pdf).ok)
            self.assertNotIn("\ufffd", dc["stdout"])

            with tempfile.TemporaryDirectory(prefix="t1_a01_04_") as tmp:
                shot = Path(tmp) / f"uni_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=rec_pdf, page=1)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A01-05",
        tier=1,
        group="G01",
        req_ids=["F-006", "F-008", "F-011", "F-012", "F-013"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a01_05_incremental_pdf_preserve_and_docx_warning(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            merged = _find_artifact(dc["artifacts"], ".pdf", "merged")
            flattened = _find_artifact(dc["artifacts"], ".pdf", "flattened")
            docx = _find_artifact(dc["artifacts"], ".docx", "exportado")
            self.assertIsNotNone(merged)
            self.assertIsNotNone(flattened)
            self.assertIsNotNone(docx)
            assert merged is not None and flattened is not None and docx is not None
            self.assertTrue(validate_pdf(merged).ok)
            self.assertTrue(validate_pdf(flattened).ok)
            self.assertTrue(validate_ooxml_docx(docx).ok)

    @e2e_case(
        case_id="T1-A01-06",
        tier=1,
        group="G01",
        req_ids=["F-009", "F-014", "F-015"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t1_a01_06_valid_container_bounds_and_render_parity(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            docx = _find_artifact(dc["artifacts"], ".docx", "exportado")
            pptx = _find_artifact(dc["artifacts"], ".pptx", "exportado")
            self.assertIsNotNone(docx)
            self.assertIsNotNone(pptx)
            assert docx is not None and pptx is not None
            self.assertTrue(validate_ooxml_docx(docx).ok)
            self.assertTrue(validate_ooxml_pptx(pptx).ok)
            self.assertEqual(run_cpp_probe(plat, ["--check-docx", _plat_path(plat, docx)]).returncode, 0)
            self.assertEqual(run_cpp_probe(plat, ["--check-pptx", _plat_path(plat, pptx)]).returncode, 0)

            with tempfile.TemporaryDirectory(prefix="t1_a01_06_") as tmp:
                pdf_path = Path(tmp) / "synthetic.pdf"
                pdf_path.write_bytes(
                    make_minimal_pdf(["MissingMathFont fallback check", "Line 2 integral"], use_flate=True)
                )
                self.assertTrue(validate_pdf(pdf_path).ok)
                pr_syn = run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, pdf_path), "--require-text"])
                self.assertEqual(pr_syn.returncode, 0, pr_syn.stderr)

                merged = _find_artifact(dc["artifacts"], ".pdf", "merged")
                self.assertIsNotNone(merged)
                assert merged is not None
                pr_merged = run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, merged)])
                self.assertEqual(pr_merged.returncode, 0, pr_merged.stderr)
                shot = Path(tmp) / f"render_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=merged, page=1)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 02: Escritura digital y tinta (F-016..F-045, GN-02..GN-07)
    # =========================================================================

    @e2e_case(
        case_id="T1-A02-01",
        tier=1,
        group="G02",
        req_ids=["F-016", "F-017", "F-018", "F-019", "GN-02", "GN-03", "U-14"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t1_a02_01_pressure_tilt_stabilizer_and_predictive_ink(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F3", dc["stdout"])
            self.assertIn("Ink Smoothing", dc["stdout"])

            with tempfile.TemporaryDirectory(prefix="t1_a02_01_") as tmp:
                shot = Path(tmp) / f"ink_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                pres = validate_png(shot, min_width=600, min_height=400, require_non_uniform=True)
                self.assertTrue(pres.ok, pres.errors)

    @e2e_case(
        case_id="T1-A02-02",
        tier=1,
        group="G02",
        req_ids=["F-028", "F-029", "F-030", "F-031", "F-032", "F-033", "F-034", "F-042", "GN-03"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t1_a02_02_brushes_multiply_highlighter_hex_and_eyedropper(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)
            with zipfile.ZipFile(cudoc, "r") as zf:
                objs = json.loads(zf.read("objects.json").decode("utf-8"))
                pages = json.loads(zf.read("pages.json").decode("utf-8"))
            all_objs = list(objs.get("canvasObjects", []))
            for pg in pages:
                all_objs.extend(pg.get("objects", []))
            self.assertGreater(len(all_objs), 0, "Expected persisted ink/highlighter objects in f9_notebook.cudoc")

    @e2e_case(
        case_id="T1-A02-03",
        tier=1,
        group="G02",
        req_ids=["F-020", "F-021", "F-022", "F-023", "F-037", "GN-04", "GN-06"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a02_03_pixel_whole_selective_scribble_erasers(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Selective Eraser", dc["stdout"])
            self.assertIn("Scribble", dc["stdout"])

    @e2e_case(
        case_id="T1-A02-04",
        tier=1,
        group="G02",
        req_ids=["F-024", "F-025", "F-026", "F-027", "F-036", "F-038", "GN-04"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a02_04_hold_to_shape_circle_lasso_and_palm_rejection(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Lasso", dc["stdout"])
            self.assertIn("Snap", dc["stdout"])

    @e2e_case(
        case_id="T1-A02-05",
        tier=1,
        group="G02",
        req_ids=["F-035", "F-039", "F-040", "F-041", "GN-06", "GN-07", "U-14"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a02_05_ruler_layers_anchored_ink_and_stroke_replay(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Layers", dc["stdout"])
            self.assertIn("Anchors", dc["stdout"])
            self.assertIn("Ruler", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)

    @e2e_case(
        case_id="T1-A02-06",
        tier=1,
        group="G02",
        req_ids=["F-043", "F-044", "F-045", "GN-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t1_a02_06_hwr_confirmation_card_and_smart_ink_restyle(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, ["--check-my-work", "(x + 1)^2", "x^2 + 2*x + 1"])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertIn("CMW|1|", pr.stdout)
            with tempfile.TemporaryDirectory(prefix="t1_a02_06_") as tmp:
                out_json = Path(tmp) / "confirm_card.json"
                shot = Path(tmp) / f"hwr_{plat}.png"
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
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing Check My Work confirmation output JSON")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 03: Cuadernos y organizacion (F-046..F-068, GN-01, GN-09)
    # =========================================================================

    @e2e_case(
        case_id="T1-A03-01",
        tier=1,
        group="G03",
        req_ids=["F-046", "F-047", "F-058", "F-060", "GN-01", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a03_01_library_hierarchy_and_trash_restore(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            vault = _find_artifact(dc["artifacts"], ".json", "vault")
            self.assertIsNotNone(vault, "Missing vault.json in keepDir")
            assert vault is not None
            data = json.loads(vault.read_text(encoding="utf-8"))
            self.assertIsInstance(data, (dict, list))
            self.assertTrue(len(json.dumps(data)) > 20)

    @e2e_case(
        case_id="T1-A03-02",
        tier=1,
        group="G03",
        req_ids=["F-048", "F-049", "F-050", "GN-01"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a03_02_mixed_page_sizes_and_vector_paper_templates(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F2", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a03_02_") as tmp:
                shot = Path(tmp) / f"grid_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A03-03",
        tier=1,
        group="G03",
        req_ids=["F-051", "F-052", "F-057", "F-062", "F-063", "F-064", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a03_03_tags_favorites_smart_collections_and_metadata(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Vault/WAL/FTS/SM-2", dc["stdout"])
            vault = _find_artifact(dc["artifacts"], ".json", "vault")
            self.assertIsNotNone(vault)

    @e2e_case(
        case_id="T1-A03-04",
        tier=1,
        group="G03",
        req_ids=["F-053", "F-054", "F-055", "F-056", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a03_04_outlines_bookmarks_hyperlinks_and_backlinks(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("failed=0", dc["stdout"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)

    @e2e_case(
        case_id="T1-A03-05",
        tier=1,
        group="G03",
        req_ids=["F-059", "F-061", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a03_05_page_version_history_and_global_fts_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("FTS", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])

    @e2e_case(
        case_id="T1-A03-06",
        tier=1,
        group="G03",
        req_ids=["F-065", "F-066", "F-067", "F-068", "GN-01"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a03_06_multitab_split_parallel_and_quick_note(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t1_a03_06_") as tmp:
                shot = Path(tmp) / f"multiview_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 04: Lectura y edicion PDF (F-069..F-098, GN-10, SUP-05)
    # =========================================================================

    @e2e_case(
        case_id="T1-A04-01",
        tier=1,
        group="G04",
        req_ids=["F-069", "F-070", "F-071", "F-072", "GN-10"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t1_a04_01_virtualized_pdf_zoom_anchor_and_lru(self) -> None:
        paths = resolve_paths(REPO_ROOT)
        manual_pdf = paths["manuales_dir"] / "fx-570_991LA_X_ES.pdf"
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("1,200-Page Layout", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a04_01_") as tmp:
                shot = Path(tmp) / f"pdf_p5_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=manual_pdf, page=5)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A04-02",
        tier=1,
        group="G04",
        req_ids=["F-073", "F-074", "F-075", "F-076", "F-077", "F-078"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a04_02_pdf_unicode_search_ocr_and_annotations(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F5", dc["stdout"])
            self.assertIn("OCR output", dc["stdout"])

    @e2e_case(
        case_id="T1-A04-03",
        tier=1,
        group="G04",
        req_ids=["F-079", "F-080", "F-081", "F-082", "F-083"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a04_03_pdf_insert_delete_rotate_crop_split_merge(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            split_pdf = _find_artifact(dc["artifacts"], ".pdf", "split2")
            merged_pdf = _find_artifact(dc["artifacts"], ".pdf", "merged")
            self.assertIsNotNone(split_pdf)
            self.assertIsNotNone(merged_pdf)
            assert split_pdf is not None and merged_pdf is not None
            self.assertTrue(validate_pdf(split_pdf).ok)
            self.assertTrue(validate_pdf(merged_pdf).ok)

    @e2e_case(
        case_id="T1-A04-04",
        tier=1,
        group="G04",
        req_ids=["F-084", "F-085", "F-086", "F-087", "F-088", "F-089"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a04_04_acroform_attachments_metadata_redaction_and_edit(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F8", dc["stdout"])
            card_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_pdfcard")
            self.assertIsNotNone(card_pdf)
            assert card_pdf is not None
            self.assertTrue(validate_pdf(card_pdf).ok)

    @e2e_case(
        case_id="T1-A04-05",
        tier=1,
        group="G04",
        req_ids=["F-090", "F-091", "F-092", "F-096", "F-097", "F-098"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a04_05_encrypted_pdf_tounicode_export_and_visual_diff(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            flat = _find_artifact(dc["artifacts"], ".pdf", "flattened")
            uni = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(flat)
            self.assertIsNotNone(uni)
            assert flat is not None and uni is not None
            self.assertTrue(validate_pdf(flat).ok)
            self.assertTrue(validate_pdf(uni, require_tounicode=True).ok)

    @e2e_case(
        case_id="T1-A04-06",
        tier=1,
        group="G04",
        req_ids=["F-093", "F-094", "F-095", "SUP-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t1_a04_06_pkcs7_signature_incremental_and_pinch_ink_link(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            signed = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            self.assertIsNotNone(signed, "Missing signed_test.pdf in keepDir")
            assert signed is not None
            sres = validate_pdf(signed, require_signature=True)
            self.assertTrue(sres.ok, f"PKCS#7 validation failed: {sres.errors}")
            self.assertEqual(len(sres.details.get("byte_range", [])), 4)
            self.assertIn("cryptoValid=1", dc["stdout"])

    # =========================================================================
    # Area 05: Imagenes y elementos (F-099..F-114, GN-08)
    # =========================================================================

    @e2e_case(
        case_id="T1-A05-01",
        tier=1,
        group="G05",
        req_ids=["F-099", "F-100", "F-114"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a05_01_clipboard_and_image_format_import(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full")
            self.assertIsNotNone(cudoc, "Missing f11_full.cudoc")
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)
            with tempfile.TemporaryDirectory(prefix="t1_a05_01_") as tmp:
                sample_png = Path(tmp) / "sample.png"
                sample_png.write_bytes(make_minimal_png(128, 96, pattern="gradient"))
                shot = Path(tmp) / f"img_open_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=sample_png)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                pres = validate_png(shot, min_width=320, min_height=200, require_non_uniform=True)
                self.assertTrue(pres.ok, pres.errors)

    @e2e_case(
        case_id="T1-A05-02",
        tier=1,
        group="G05",
        req_ids=["F-101", "F-102", "F-103"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a05_02_nondestructive_crop_transform_and_anchored_ink(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            self.assertTrue(validate_cudoc(cudoc).ok)

    @e2e_case(
        case_id="T1-A05-03",
        tier=1,
        group="G05",
        req_ids=["F-104", "F-105", "F-106"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t1_a05_03_whiteboard_perspective_warp_and_background_cleanup(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a05_03_") as tmp:
                img_p = Path(tmp) / "whiteboard_sample.png"
                img_p.write_bytes(make_minimal_png(128, 96, pattern="gradient"))
                out_cudoc = Path(tmp) / "warped.cudoc"
                shot = Path(tmp) / f"canvas_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--warp-scan-ocr",
                        _plat_path(plat, img_p),
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                with zipfile.ZipFile(out_cudoc, "r") as zf:
                    objs = json.loads(zf.read("objects.json").decode("utf-8")).get("canvasObjects", [])
                    pages = json.loads(zf.read("pages.json").decode("utf-8"))
                    all_objs = list(objs) + [o for pg in pages for o in pg.get("objects", [])]
                    has_ocr = any(
                        str(o.get("ocrTextUtf8", "") or o.get("textUtf8", "")).strip()
                        for o in all_objs
                    )
                    self.assertTrue(has_ocr, "Expected OCR text object in warped .cudoc")
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A05-04",
        tier=1,
        group="G05",
        req_ids=["F-107", "F-108", "F-109", "F-110", "GN-08"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a05_04_quick_diagramming_connectors_grouping_and_stickers(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t1_a05_04_") as tmp:
                shot = Path(tmp) / f"diagram_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A05-05",
        tier=1,
        group="G05",
        req_ids=["F-111", "F-112", "F-113"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="win_table_interop",
    )
    def test_t1_a05_05_reactive_canvas_table_media_and_region_screenshot(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a05_05_") as tmp:
                in_csv = Path(tmp) / "table_input.csv"
                in_csv.write_text("col_a,col_b\n10,20\n30,40\n", encoding="utf-8")
                sut_xlsx = Path(tmp) / "table_export.xlsx"
                shot = Path(tmp) / f"table_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-csv",
                        _plat_path(plat, in_csv),
                        "--export-xlsx",
                        _plat_path(plat, sut_xlsx),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_ooxml_xlsx(sut_xlsx).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 06: Lienzo infinito (F-115..F-134, GN-01, GN-08, SUP-05, SUP-08)
    # =========================================================================

    @e2e_case(
        case_id="T1-A06-01",
        tier=1,
        group="G06",
        req_ids=["F-115", "F-116", "F-117", "F-118", "F-119", "GN-01"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_quadtree_100k",
    )
    def test_t1_a06_01_infinite_canvas_100k_quadtree_and_minimap(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("100k Quadtree query returned 25 matches", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t1_a06_01_") as tmp:
                shot = Path(tmp) / f"quad_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A06-02",
        tier=1,
        group="G06",
        req_ids=["F-120", "F-121", "F-125", "F-126", "F-127", "GN-08"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_quadtree_100k",
    )
    def test_t1_a06_02_frames_sections_snapping_and_board_conversion(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            region_pdf = _find_artifact(dc["artifacts"], ".pdf", "canvas_region")
            self.assertIsNotNone(region_pdf)
            assert region_pdf is not None
            self.assertTrue(validate_pdf(region_pdf).ok)

    @e2e_case(
        case_id="T1-A06-03",
        tier=1,
        group="G06",
        req_ids=["F-122", "F-123", "F-124", "F-132", "SUP-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a06_03_pdf_on_canvas_pinch_to_collapse_and_ink_link(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            p205 = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_205p")
            pcard = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_pdfcard")
            self.assertIsNotNone(p205)
            self.assertIsNotNone(pcard)
            assert p205 is not None and pcard is not None
            self.assertTrue(validate_pdf(p205).ok)
            self.assertTrue(validate_pdf(pcard).ok)

    @e2e_case(
        case_id="T1-A06-04",
        tier=1,
        group="G06",
        req_ids=["F-128", "F-129", "F-130", "F-131"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a06_04_iec_real_scale_a4_grid_and_frame_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            self.assertIsNotNone(stem_pdf)
            assert stem_pdf is not None
            self.assertTrue(validate_pdf(stem_pdf).ok)
            with tempfile.TemporaryDirectory(prefix="t1_a06_04_") as tmp:
                shot = Path(tmp) / f"stem_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, showcase_stem=True)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=400, min_height=300).ok)

    @e2e_case(
        case_id="T1-A06-05",
        tier=1,
        group="G06",
        req_ids=["F-133", "F-134", "SUP-08", "fx-570LA X", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t1_a06_05_formula_card_and_live_calc_bridge_card(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a06_05_") as tmp:
                out_cudoc = Path(tmp) / "bridge_sync.cudoc"
                shot = Path(tmp) / f"card_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-card",
                        "fx-991LAX|2,+,3,=|2+3",
                        "--calc-bridge-sync",
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                with zipfile.ZipFile(out_cudoc, "r") as zf:
                    objs = json.loads(zf.read("objects.json").decode("utf-8")).get("canvasObjects", [])
                    self.assertTrue(
                        any(o.get("kind") == "calc_card" and "5" in json.dumps(o) for o in objs),
                        f"Synced CalcSessionCard missing result in {objs}",
                    )
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    # =========================================================================
    # Area 07: Texto y documentos (F-135..F-152)
    # =========================================================================

    @e2e_case(
        case_id="T1-A07-01",
        tier=1,
        group="G07",
        req_ids=["F-135", "F-136", "F-137", "F-138", "F-139"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t1_a07_01_unicode_bidi_math_typography_and_fractions(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            uni_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(uni_pdf)
            assert uni_pdf is not None
            self.assertTrue(validate_pdf(uni_pdf, require_tounicode=True).ok)

    @e2e_case(
        case_id="T1-A07-02",
        tier=1,
        group="G07",
        req_ids=["F-140", "F-141", "F-142", "F-143"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a07_02_lists_checkboxes_tables_and_inline_ink_reflow(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Anchors", dc["stdout"])

    @e2e_case(
        case_id="T1-A07-03",
        tier=1,
        group="G07",
        req_ids=["F-144", "F-145"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a07_03_paragraph_styles_outline_and_cross_references(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])

    @e2e_case(
        case_id="T1-A07-04",
        tier=1,
        group="G07",
        req_ids=["F-146", "F-147", "F-148", "F-149"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t1_a07_04_docx_pptx_ooxml_roundtrip_and_fidelity_report(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            docx = _find_artifact(dc["artifacts"], ".docx", "exportado")
            pptx = _find_artifact(dc["artifacts"], ".pptx", "exportado")
            self.assertIsNotNone(docx)
            self.assertIsNotNone(pptx)
            assert docx is not None and pptx is not None
            self.assertTrue(validate_ooxml_docx(docx).ok)
            self.assertTrue(validate_ooxml_pptx(pptx).ok)
            self.assertEqual(run_cpp_probe(plat, ["--check-docx", _plat_path(plat, docx)]).returncode, 0)
            self.assertEqual(run_cpp_probe(plat, ["--check-pptx", _plat_path(plat, pptx)]).returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t1_a07_04_") as tmp:
                syn_docx = Path(tmp) / "syn.docx"
                syn_docx.write_bytes(make_minimal_docx(["Ecuacion Maxwell", "Laplace F(s)"]))
                self.assertTrue(validate_ooxml_docx(syn_docx).ok)
                pr = run_cpp_probe(plat, ["--check-docx", _plat_path(plat, syn_docx)])
                self.assertEqual(pr.returncode, 0, pr.stderr)

    @e2e_case(
        case_id="T1-A07-05",
        tier=1,
        group="G07",
        req_ids=["F-150", "F-151", "F-152"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a07_05_vector_svg_markdown_latex_and_mathml(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t1_a07_05_") as tmp:
                d = Path(tmp)
                svg_p = d / "vec.svg"
                sr_svg = run_cpp_probe(
                    plat,
                    ["--mna-svg", "V1 1 0 12\nR1 1 2 1000\nR2 2 0 2000", _plat_path(plat, svg_p)],
                )
                self.assertEqual(sr_svg.returncode, 0, sr_svg.stderr)
                self.assertTrue(validate_svg(svg_p).ok)

                html_p = d / "report.html"
                sr_html = run_cpp_probe(
                    plat,
                    ["--export-report-html", "V_out = 12 V / 3", _plat_path(plat, html_p)],
                )
                self.assertEqual(sr_html.returncode, 0, sr_html.stderr)
                self.assertTrue(validate_html5(html_p).ok)

                md_p = d / "report.md"
                sr_md = run_cpp_probe(
                    plat,
                    ["--export-report-md", "V_out = 12 V / 3", _plat_path(plat, md_p)],
                )
                self.assertEqual(sr_md.returncode, 0, sr_md.stderr)
                self.assertTrue(md_p.exists() and len(md_p.read_text(encoding="utf-8").strip()) > 10)

    # =========================================================================
    # Area 08: Audio video captura (F-153..F-164, GN-13)
    # =========================================================================

    @e2e_case(
        case_id="T1-A08-01",
        tier=1,
        group="G08",
        req_ids=["F-153", "F-154", "F-155", "GN-13", "U-16"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t1_a08_01_audio_recording_and_synced_ink_seek(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a08_01_") as tmp:
                d = Path(tmp)
                sut_wav = d / "recorded.wav"
                out_cudoc = d / "recorded.cudoc"
                shot = d / f"audio_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--record-audio-smoke",
                        "--out-wav",
                        _plat_path(plat, sut_wav),
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                wres = validate_wav_pcm(sut_wav, min_duration_ms=200.0)
                self.assertTrue(wres.ok, wres.errors)
                self.assertTrue(validate_cudoc(out_cudoc).ok)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T1-A08-02",
        tier=1,
        group="G08",
        req_ids=["F-156", "F-159", "F-164"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t1_a08_02_audio_bookmarks_import_offset_and_chapters(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a08_02_") as tmp:
                d = Path(tmp)
                wav = d / "external_300ms.wav"
                wav.write_bytes(make_minimal_wav(duration_ms=300))
                out_cudoc = d / "audio_chapters.cudoc"
                shot = d / f"chapters_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-audio",
                        _plat_path(plat, wav),
                        "--audio-offset-ms",
                        "150",
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
        case_id="T1-A08-03",
        tier=1,
        group="G08",
        req_ids=["F-157", "F-158"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t1_a08_03_local_offline_transcription_and_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a08_03_") as tmp:
                d = Path(tmp)
                wav = d / "speech_sample.wav"
                wav.write_bytes(make_minimal_wav(duration_ms=400))
                out_json = d / "transcript.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-audio",
                        _plat_path(plat, wav),
                        "--transcribe-audio",
                        "--out-json",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(out_json.exists(), "Missing offline transcription JSON")
                payload = json.loads(out_json.read_text(encoding="utf-8"))
                self.assertIn("segments", payload)

    @e2e_case(
        case_id="T1-A08-04",
        tier=1,
        group="G08",
        req_ids=["F-160", "F-161", "F-163"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t1_a08_04_synced_voice_ink_keylog_html5_tutorial_export(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t1_a08_04_") as tmp:
                d = Path(tmp)
                wav = d / "tutorial_voice.wav"
                wav.write_bytes(make_minimal_wav(duration_ms=300))
                sut_html = d / "tutorial.html"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-audio",
                        _plat_path(plat, wav),
                        "--calc-card",
                        "fx-991CW|2,+,2,=|4",
                        "--export-keylog-html",
                        _plat_path(plat, sut_html),
                    ],
                )
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_html5(sut_html).ok)

    @e2e_case(
        case_id="T1-A08-05",
        tier=1,
        group="G08",
        req_ids=["F-162", "SUP-08", "fx-570LA X", "fx-570VN PLUS", "fx-CG50", "fx-991CW", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t1_a08_05_four_casio_models_lcd_capture_via_calc_bridge(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            if plat == "win":
                for flag in ("--compact", "--compact-gray", "--compact-cg50", "--compact-cw"):
                    pr = probe_calculadora_win([flag], timeout_s=4.0)
                    self.assertTrue(pr["started"] and pr["alive_after_wait"], f"{flag}: {pr}")
            else:
                with tempfile.TemporaryDirectory(prefix="t1_a08_05_") as tmp:
                    for idx, flag in enumerate(("--compact", "--compact-gray", "--compact-cg50", "--compact-cw2")):
                        shot = Path(tmp) / f"calc_{idx}.png"
                        sr = smoke_calculadora_linux([flag, "--smoke-test"], out_png=shot, timeout=90)
                        self.assertEqual(sr.returncode, 0, sr.stderr)
                        self.assertTrue(validate_png(shot, min_width=200, min_height=200).ok)
            with tempfile.TemporaryDirectory(prefix="t1_a08_05_ds_") as tmp:
                out_cudoc = Path(tmp) / "lcd_sync.cudoc"
                shot = Path(tmp) / f"lcd_sync_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-card",
                        "fx-CG50|SIN,3,0,=|0.5",
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

    # =========================================================================
    # Area 09: Busqueda y conocimiento (F-165..F-178, GN-11)
    # =========================================================================

    @e2e_case(
        case_id="T1-A09-01",
        tier=1,
        group="G09",
        req_ids=["F-165", "F-166", "F-167", "GN-11"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a09_01_universal_unicode_pdf_ligature_and_tounicode_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F5", dc["stdout"])
            uni = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(uni)
            assert uni is not None
            self.assertTrue(validate_pdf(uni, require_tounicode=True).ok)

    @e2e_case(
        case_id="T1-A09-02",
        tier=1,
        group="G09",
        req_ids=["F-168", "F-169", "GN-11"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t1_a09_02_background_ocr_and_hwr_index_query(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("OCR output", dc["stdout"])

    @e2e_case(
        case_id="T1-A09-03",
        tier=1,
        group="G09",
        req_ids=["F-170", "F-171"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a09_03_accent_folded_phrase_regex_and_proximity_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("FTS", dc["stdout"])

    @e2e_case(
        case_id="T1-A09-04",
        tier=1,
        group="G09",
        req_ids=["F-172", "F-173", "F-174"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t1_a09_04_structural_math_and_circuit_component_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            pr = run_cpp_probe(plat, ["--mna", "V1 1 0 12\nR1 1 2 1k\nR2 2 0 2k"])
            self.assertEqual(pr.returncode, 0, pr.stderr)
            self.assertTrue(len(pr.stdout.strip()) > 0)

    @e2e_case(
        case_id="T1-A09-05",
        tier=1,
        group="G09",
        req_ids=["F-175", "F-176", "F-177", "F-178"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t1_a09_05_cross_course_search_citations_and_backlink_graph(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            vault = _find_artifact(dc["artifacts"], ".json", "vault")
            self.assertIsNotNone(vault)
            if plat == "win":
                ct = run_ctest_target("win", "^UiCoreModules$", timeout=90)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)
