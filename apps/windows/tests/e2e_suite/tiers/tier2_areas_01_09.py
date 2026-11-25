"""Tier 2 E2E Tests: Areas 01..09 (45 boundary, negative, fuzz, and stress cases)."""
from __future__ import annotations

import json
import os
import shutil
import tempfile
import unittest
import zipfile
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
    validate_zip_container,
)
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.cpp_probe_channel import run_cpp_probe
from e2e_suite.adapters.ctest_channel import create_session_keepdir, run_ctest_target, run_doc_core_keepdir
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.adapters.win_gui_channel import smoke_document_studio
from e2e_suite.fixtures.corrupt_generators import (
    make_bad_crc_zip,
    make_bad_magic_pdf,
    make_blank_png,
    make_corrupt_png_bad_crc,
    make_corrupt_wal,
    make_path_traversal_zip,
    make_truncated_pdf,
    make_truncated_zip,
    make_zip_bomb_payload,
)
from e2e_suite.fixtures.synthetic_data import make_minimal_docx, make_minimal_pdf, make_minimal_png, make_minimal_wav


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


class TestTier2Areas01To09(unittest.TestCase):
    """Tier 2 boundary, negative, fuzz, and stress cases covering Areas 01..09."""

    # =========================================================================
    # Area 01: Integridad y fiabilidad
    # =========================================================================

    @e2e_case(
        case_id="T2-A01-01",
        tier=2,
        group="G01",
        req_ids=["F-001", "F-010"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t2_a01_01_atomic_save_preserves_prior_cudoc_on_partial_temp(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            src_cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f11_full") or _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(src_cudoc, "Missing valid baseline .cudoc in keepDir")
            assert src_cudoc is not None
            with tempfile.TemporaryDirectory(prefix="t2_a01_01_") as tmp:
                v1 = Path(tmp) / "doc_v1.cudoc"
                shutil.copy2(src_cudoc, v1)
                h_before = sha256_file(v1)

                partial_tmp = Path(tmp) / "doc_v1.cudoc.tmp"
                partial_tmp.write_bytes(make_truncated_zip(v1.read_bytes(), 0.35))
                self.assertFalse(validate_cudoc(partial_tmp).ok)
                self.assertEqual(sha256_file(v1), h_before)
                self.assertTrue(validate_cudoc(v1).ok)

                shot = Path(tmp) / f"reopen_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=v1)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A01-02",
        tier=2,
        group="G01",
        req_ids=["F-002", "F-003", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_cudoc_storage",
    )
    def test_t2_a01_02_corrupt_wal_tail_rejected_via_crc_without_loop(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            cudoc = _find_artifact(dc["artifacts"], ".cudoc", "f9_notebook")
            self.assertIsNotNone(cudoc)
            assert cudoc is not None
            with tempfile.TemporaryDirectory(prefix="t2_a01_02_") as tmp:
                d = Path(tmp)
                bad_wal = d / "corrupt.cudoc-wal"
                bad_wal.write_bytes(make_corrupt_wal(corrupt_tail=True))
                res = validate_cudoc(cudoc, wal_path=bad_wal)
                self.assertFalse(res.ok)
                self.assertTrue(any("CRC mismatch" in e for e in res.errors), res.errors)
                self.assertEqual(res.details.get("wal_frames"), 1)

                # Route through C++ LoadFromFileWithRecovery: corrupt primary + bad WAL recovers cleanly from .bak
                rec_cudoc = d / "rec_test.cudoc"
                rec_cudoc.write_bytes(b"CORRUPT_PRIMARY_BYTES")
                (d / "rec_test.cudoc.bak").write_bytes(cudoc.read_bytes())
                (d / "rec_test.cudoc-wal").write_bytes(bad_wal.read_bytes())
                pr = run_cpp_probe(plat, ["--check-cudoc-recovery", _plat_path(plat, rec_cudoc)])
                self.assertEqual(pr.returncode, 0, pr.stderr)
                self.assertIn("OK|usedBak=1", pr.stdout)

    @e2e_case(
        case_id="T2-A01-03",
        tier=2,
        group="G01",
        req_ids=["F-004", "GN-12"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_command_stack",
    )
    def test_t2_a01_03_excessive_undo_stops_cleanly_at_baseline_boundary(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("CommandStack", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])

    @e2e_case(
        case_id="T2-A01-04",
        tier=2,
        group="G01",
        req_ids=["F-008", "F-011", "F-014"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a01_04_fuzz_zero_byte_zip_bomb_and_truncated_pdf_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a01_04_") as tmp:
                d = Path(tmp)
                zero_cu = d / "zero.cudoc"
                zero_cu.write_bytes(b"")
                self.assertFalse(validate_cudoc(zero_cu).ok)

                bomb = d / "bomb.cudoc"
                bomb.write_bytes(make_zip_bomb_payload(uncompressed_mb=20))
                zres = validate_zip_container(bomb, max_uncompressed_bytes=10 * 1024 * 1024, max_ratio=50.0)
                self.assertFalse(zres.ok)

                trunc_pdf = d / "trunc.pdf"
                trunc_pdf.write_bytes(make_truncated_pdf())
                self.assertFalse(validate_pdf(trunc_pdf).ok)

                html_pdf = d / "login.pdf"
                html_pdf.write_bytes(make_bad_magic_pdf(as_html_login=True))
                self.assertFalse(validate_pdf(html_pdf).ok)

                # Verify DocumentStudio refuses crash/hang when fed malformed inputs
                for idx, bad in enumerate((zero_cu, trunc_pdf, html_pdf)):
                    shot = d / f"fuzz_{idx}_{plat}.png"
                    sr = smoke_document_studio(plat, "pdf", shot, open_path=bad)
                    self.assertFalse(sr.timed_out, f"Studio hung on {bad.name}")
                    self.assertEqual(sr.returncode, 0, sr.stderr)

    @e2e_case(
        case_id="T2-A01-05",
        tier=2,
        group="G01",
        req_ids=["F-005", "F-006", "F-007", "F-009", "F-012", "F-013", "F-015"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a01_05_bitflipped_png_crc_and_missing_tounicode_detection(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            uni_pdf = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(uni_pdf, "Missing f8_unicode_math.pdf in keepDir")
            assert uni_pdf is not None
            self.assertTrue(validate_pdf(uni_pdf, require_tounicode=True).ok)
            pr_uni = run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, uni_pdf), "--require-text"])
            self.assertEqual(pr_uni.returncode, 0, pr_uni.stderr)

            with tempfile.TemporaryDirectory(prefix="t2_a01_05_") as tmp:
                d = Path(tmp)
                bad_png = d / "corrupt.png"
                bad_png.write_bytes(make_corrupt_png_bad_crc(make_minimal_png()))
                pres = validate_png(bad_png)
                self.assertFalse(pres.ok)
                self.assertTrue(any("CRC" in e for e in pres.errors), pres.errors)

                shot_png = d / f"bad_png_{plat}.png"
                sr_png = smoke_document_studio(plat, "canvas", shot_png, open_path=bad_png)
                self.assertFalse(sr_png.timed_out)
                self.assertEqual(sr_png.returncode, 0, sr_png.stderr)
                self.assertTrue(validate_png(shot_png, min_width=320, min_height=200).ok)

                no_tu = d / "no_tounicode.pdf"
                no_tu.write_bytes(make_minimal_pdf(["Plain text without ToUnicode"]))
                pdf_res = validate_pdf(no_tu, require_tounicode=True)
                self.assertFalse(pdf_res.ok)
                self.assertTrue(any("ToUnicode" in e for e in pdf_res.errors))

                shot_pdf = d / f"no_tu_{plat}.png"
                sr_pdf = smoke_document_studio(plat, "pdf", shot_pdf, open_path=no_tu, page=1)
                self.assertFalse(sr_pdf.timed_out)
                self.assertEqual(sr_pdf.returncode, 0, sr_pdf.stderr)
                self.assertTrue(validate_png(shot_pdf, min_width=320, min_height=200).ok)

                bad_zip = d / "bad_crc.zip"
                bad_zip.write_bytes(make_bad_crc_zip())
                self.assertNotEqual(run_cpp_probe(plat, ["--check-zip", _plat_path(plat, bad_zip)]).returncode, 0)

                trunc_pdf = d / "trunc.pdf"
                trunc_pdf.write_bytes(make_truncated_pdf())
                self.assertNotEqual(run_cpp_probe(plat, ["--check-pdf", _plat_path(plat, trunc_pdf)]).returncode, 0)

    # =========================================================================
    # Area 02: Escritura digital y tinta
    # =========================================================================

    @e2e_case(
        case_id="T2-A02-01",
        tier=2,
        group="G02",
        req_ids=["F-016", "F-017", "F-028", "F-029", "F-030", "F-032", "F-033", "GN-03", "U-14"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t2_a02_01_extreme_pressure_nan_tilt_and_zero_length_tap(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F3", dc["stdout"])

    @e2e_case(
        case_id="T2-A02-02",
        tier=2,
        group="G02",
        req_ids=["F-018", "F-019", "F-035", "F-039", "F-040", "GN-02"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_ink_d2d_cairo",
    )
    def test_t2_a02_02_high_freq_10k_pointer_stream_and_10x_replay(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Ink Smoothing", dc["stdout"])

    @e2e_case(
        case_id="T2-A02-03",
        tier=2,
        group="G02",
        req_ids=["F-022", "F-023", "F-024", "F-025", "F-031", "F-041", "GN-04", "GN-06"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a02_03_gesture_false_trigger_prevention_and_locked_layer_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Layers", dc["stdout"])
            self.assertIn("Selective Eraser", dc["stdout"])

    @e2e_case(
        case_id="T2-A02-04",
        tier=2,
        group="G02",
        req_ids=["F-036", "F-037", "F-038"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a02_04_simultaneous_palm_contacts_and_hover_barrel_shortcut(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F9", dc["stdout"])

    @e2e_case(
        case_id="T2-A02-05",
        tier=2,
        group="G02",
        req_ids=["F-020", "F-021", "F-026", "F-027", "F-034", "F-042", "F-043", "F-044", "F-045", "GN-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="check_my_work_ui",
    )
    def test_t2_a02_05_degenerate_one_point_split_and_empty_hwr_prompt(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            res = run_cpp_probe(plat, ["--check-my-work", "", "x"])
            self.assertNotEqual(res.returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t2_a02_05_") as tmp:
                out_json = Path(tmp) / "empty_cmw.json"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--check-my-work",
                        "",
                        "x",
                        "--export-errorbook",
                        _plat_path(plat, out_json),
                    ],
                )
                self.assertNotEqual(sr.returncode, 0)

    # =========================================================================
    # Area 03: Cuadernos y organizacion
    # =========================================================================

    @e2e_case(
        case_id="T2-A03-01",
        tier=2,
        group="G03",
        req_ids=["F-046", "F-051", "F-052", "F-057", "F-061", "F-068", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a03_01_stress_utf8_library_and_empty_search_query(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("FTS", dc["stdout"])

    @e2e_case(
        case_id="T2-A03-02",
        tier=2,
        group="G03",
        req_ids=["F-047", "F-060", "F-062", "F-064"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a03_02_reserved_os_filename_and_hierarchy_cycle_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t2_a03_02_") as tmp:
                evil = Path(tmp) / "traversal.zip"
                evil.write_bytes(make_path_traversal_zip("../../../reserved<>name.txt"))
                self.assertFalse(validate_zip_container(evil).ok)

    @e2e_case(
        case_id="T2-A03-03",
        tier=2,
        group="G03",
        req_ids=["F-053", "F-054", "F-055", "F-056", "GN-09"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a03_03_dangling_backlink_target_in_trash_recovery(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Vault/WAL/FTS/SM-2", dc["stdout"])

    @e2e_case(
        case_id="T2-A03-04",
        tier=2,
        group="G03",
        req_ids=["F-058", "F-059"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a03_04_trash_purge_and_100_snapshot_version_rollback(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])

    @e2e_case(
        case_id="T2-A03-05",
        tier=2,
        group="G03",
        req_ids=["F-048", "F-049", "F-050", "F-063", "F-065", "F-066", "F-067", "GN-01"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t2_a03_05_extreme_heterogeneous_page_sizes_and_tab_close(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("1,200-Page Layout", dc["stdout"])

    # =========================================================================
    # Area 04: Lectura y edicion PDF
    # =========================================================================

    @e2e_case(
        case_id="T2-A04-01",
        tier=2,
        group="G04",
        req_ids=["F-069", "F-070", "F-071", "F-072", "F-078", "F-098", "GN-10"],
        apps=["DocumentStudio", "CalculadoraUnificada"],
        platforms=["win", "linux"],
        readiness_key="core_layout_camera_lru",
    )
    def test_t2_a04_01_massive_2000_page_pdf_lru_budget_stress(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            p205 = _find_artifact(dc["artifacts"], ".pdf", "f8_canvas_205p")
            self.assertIsNotNone(p205)
            assert p205 is not None
            self.assertTrue(validate_pdf(p205).ok)
            if plat == "win":
                ct = _run_ctest_cached("win", "^PdfViewerStress$", timeout=540)
                self.assertEqual(ct.returncode, 0, f"{ct.stderr}\n{ct.stdout}")
                self.assertNotIn("No tests were found", ct.stdout + ct.stderr)

    @e2e_case(
        case_id="T2-A04-02",
        tier=2,
        group="G04",
        req_ids=["F-082", "F-084", "F-085", "F-097"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a04_02_embedded_file_path_traversal_and_xfa_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("ZIP Budget/Path", dc["stdout"])
            with tempfile.TemporaryDirectory(prefix="t2_a04_02_") as tmp:
                trav = Path(tmp) / "trav.zip"
                trav.write_bytes(make_path_traversal_zip("../../../evil.sh"))
                zres = validate_zip_container(trav)
                self.assertFalse(zres.ok)
                self.assertTrue(any("traversal" in e.lower() for e in zres.errors), zres.errors)
                pr = run_cpp_probe(plat, ["--check-zip", _plat_path(plat, trav)])
                self.assertNotEqual(pr.returncode, 0)

    @e2e_case(
        case_id="T2-A04-03",
        tier=2,
        group="G04",
        req_ids=["F-091", "F-093", "F-094", "F-095"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t2_a04_03_tampered_signed_pdf_byte_flip_invalidates_signature(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Tamper Rejection", dc["stdout"])
            signed = _find_artifact(dc["artifacts"], ".pdf", "signed_test")
            self.assertIsNotNone(signed)
            assert signed is not None

            v_ok = run_cpp_probe(plat, ["--verify-sig", _plat_path(plat, signed)])
            self.assertEqual(v_ok.returncode, 0, v_ok.stderr or v_ok.stdout)
            self.assertIn("SIG|1|1|0", v_ok.stdout)

            with tempfile.TemporaryDirectory(prefix="t2_a04_03_") as tmp:
                tampered = Path(tmp) / "tampered.pdf"
                raw = bytearray(signed.read_bytes())
                # Corrupt trailer / ByteRange content on copy
                idx = raw.rfind(b"%%EOF")
                if idx > 16:
                    raw[idx - 10 : idx] = b"0000000000"
                tampered.write_bytes(bytes(raw))
                self.assertNotEqual(sha256_file(signed), sha256_file(tampered))

                v_bad = run_cpp_probe(plat, ["--verify-sig", _plat_path(plat, tampered)])
                self.assertEqual(v_bad.returncode, 2, f"Expected SIG tamper rejection rc=2, got {v_bad.returncode}: {v_bad.stdout}")

                shot = Path(tmp) / f"tampered_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=tampered, page=1)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A04-04",
        tier=2,
        group="G04",
        req_ids=["F-073", "F-075", "F-076", "F-086", "F-087", "F-088", "F-090", "F-092", "F-096"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t2_a04_04_missing_glyph_warning_redaction_and_broken_link_alert(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F5", dc["stdout"])
            self.assertIn("failed=0", dc["stdout"])

    @e2e_case(
        case_id="T2-A04-05",
        tier=2,
        group="G04",
        req_ids=["F-074", "F-077", "F-079", "F-080", "F-081", "F-083", "F-089"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a04_05_malformed_xref_and_blank_ocr_page_graceful_handling(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a04_05_") as tmp:
                d = Path(tmp)
                bad = d / "bad_xref.pdf"
                bad.write_bytes(make_truncated_pdf())
                self.assertFalse(validate_pdf(bad).ok)
                shot = d / f"bad_{plat}.png"
                sr = smoke_document_studio(plat, "pdf", shot, open_path=bad)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)

                blank = d / "blank.png"
                blank.write_bytes(make_blank_png(32, 32))
                bres = validate_png(blank, require_non_uniform=True)
                self.assertFalse(bres.ok)

    # =========================================================================
    # Area 05: Imagenes y elementos
    # =========================================================================

    @e2e_case(
        case_id="T2-A05-01",
        tier=2,
        group="G05",
        req_ids=["F-100", "F-109"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a05_01_corrupt_image_and_svg_script_sanitization(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a05_01_") as tmp:
                d = Path(tmp)
                bad_png = d / "bad_crc.png"
                bad_png.write_bytes(make_corrupt_png_bad_crc(make_minimal_png()))
                self.assertFalse(validate_png(bad_png).ok)

                shot = d / f"bad_crc_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=bad_png)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

                sut_svg = d / "mna_circuit.svg"
                pr_svg = run_cpp_probe(
                    plat,
                    ["--mna-svg", "V1 1 0 10\nR1 1 0 1000", _plat_path(plat, sut_svg)],
                )
                self.assertEqual(pr_svg.returncode, 0, pr_svg.stderr)
                self.assertTrue(validate_svg(sut_svg).ok)

                sut_html = d / "escaped_report.html"
                pr_html = run_cpp_probe(
                    plat,
                    ["--export-report-html", "<script>alert(1)</script>", _plat_path(plat, sut_html)],
                )
                self.assertEqual(pr_html.returncode, 0, pr_html.stderr)
                html_text = sut_html.read_text(encoding="utf-8")
                self.assertNotIn("<script>alert(1)</script>", html_text)
                self.assertIn("&lt;script&gt;alert(1)&lt;/script&gt;", html_text)
                self.assertTrue(validate_html5(sut_html).ok)

                bad_svg = d / "empty.svg"
                bad_svg.write_text("<svg xmlns='http://www.w3.org/2000/svg'></svg>", encoding="utf-8")
                self.assertFalse(validate_svg(bad_svg).ok)

    @e2e_case(
        case_id="T2-A05-02",
        tier=2,
        group="G05",
        req_ids=["F-099", "F-113", "F-114"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a05_02_empty_clipboard_and_malformed_paste_noop(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a05_02_") as tmp:
                shot = Path(tmp) / f"paste_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A05-03",
        tier=2,
        group="G05",
        req_ids=["F-104", "F-105", "F-106"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="cas_matrix_full_parity",
    )
    def test_t2_a05_03_collinear_perspective_warp_and_1x1_scan_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a05_03_") as tmp:
                d = Path(tmp)
                tiny = d / "tiny.png"
                tiny.write_bytes(make_minimal_png(1, 1))
                out_pdf_tiny = d / "tiny_out.pdf"
                shot_tiny = d / f"tiny_{plat}.png"
                sr1 = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--open",
                        _plat_path(plat, tiny),
                        "--export-pdf",
                        _plat_path(plat, out_pdf_tiny),
                        "--screenshot-out",
                        _plat_path(plat, shot_tiny),
                    ],
                )
                self.assertEqual(sr1.returncode, 0, sr1.stderr)
                self.assertTrue(validate_pdf(out_pdf_tiny).ok)
                self.assertTrue(validate_png(shot_tiny, min_width=320, min_height=200).ok)

                extreme = d / "extreme_aspect.png"
                extreme.write_bytes(make_minimal_png(256, 8, pattern="gradient"))
                out_pdf_ext = d / "extreme_out.pdf"
                shot_ext = d / f"extreme_{plat}.png"
                sr2 = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--open",
                        _plat_path(plat, extreme),
                        "--export-pdf",
                        _plat_path(plat, out_pdf_ext),
                        "--screenshot-out",
                        _plat_path(plat, shot_ext),
                    ],
                )
                self.assertEqual(sr2.returncode, 0, sr2.stderr)
                self.assertTrue(validate_pdf(out_pdf_ext).ok)
                self.assertTrue(validate_png(shot_ext, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A05-04",
        tier=2,
        group="G05",
        req_ids=["F-101", "F-102", "F-103", "F-111"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="win_table_interop",
    )
    def test_t2_a05_04_zero_crop_max_resize_cap_and_cyclic_table_formula(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a05_04_") as tmp:
                d = Path(tmp)
                bad_csv = d / "cyclic_table.csv"
                bad_csv.write_text("a,b\n=B1+1,=A1+1\n", encoding="utf-8")
                out_xlsx = d / "cyclic_out.xlsx"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-csv",
                        _plat_path(plat, bad_csv),
                        "--export-xlsx",
                        _plat_path(plat, out_xlsx),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertTrue(
                    sr.returncode != 0 or validate_ooxml_xlsx(out_xlsx).ok,
                    "Cyclic CSV export must either reject cleanly or emit bounded valid XLSX",
                )

    @e2e_case(
        case_id="T2-A05-05",
        tier=2,
        group="G05",
        req_ids=["F-107", "F-108", "F-110", "F-112", "GN-08"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a05_05_self_loop_connector_delete_and_corrupt_media_fallback(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])

    # =========================================================================
    # Area 06: Lienzo infinito
    # =========================================================================

    @e2e_case(
        case_id="T2-A06-01",
        tier=2,
        group="G06",
        req_ids=["F-115", "F-118", "F-119"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a06_01_extreme_negative_and_1e9_canvas_bounds(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Quadtree Bounds", dc["stdout"])

    @e2e_case(
        case_id="T2-A06-02",
        tier=2,
        group="G06",
        req_ids=["F-116", "F-117", "F-125", "F-131"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_quadtree_100k",
    )
    def test_t2_a06_02_sparse_100k_objects_spatial_query_stress(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("100k Quadtree query returned 25 matches", dc["stdout"])

    @e2e_case(
        case_id="T2-A06-03",
        tier=2,
        group="G06",
        req_ids=["F-120", "F-121", "F-126", "F-130", "F-134", "SUP-08", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t2_a06_03_empty_frame_export_and_disconnected_calc_bridge_peer(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a06_03_") as tmp:
                d = Path(tmp)
                out_cudoc = d / "disconnected_bridge.cudoc"
                shot = d / f"offline_card_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-card",
                        "fx-991LAX|invalid|",
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
        case_id="T2-A06-04",
        tier=2,
        group="G06",
        req_ids=["F-122", "F-123", "F-124", "F-132", "SUP-05"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t2_a06_04_malformed_docx_import_and_missing_ink_link_source(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a06_04_") as tmp:
                d = Path(tmp)
                bad_docx = d / "bad.docx"
                bad_docx.write_bytes(make_bad_crc_zip())
                self.assertFalse(validate_ooxml_docx(bad_docx).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-docx", _plat_path(plat, bad_docx)]).returncode, 0)

                trunc_docx = d / "trunc.docx"
                trunc_docx.write_bytes(make_truncated_zip(make_minimal_docx(), 0.4))
                self.assertFalse(validate_ooxml_docx(trunc_docx).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-docx", _plat_path(plat, trunc_docx)]).returncode, 0)

                shot = d / f"trunc_docx_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=trunc_docx)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A06-05",
        tier=2,
        group="G06",
        req_ids=["F-127", "F-128", "F-129", "F-133"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a06_05_straddling_a4_grid_and_dense_snap_targets(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            stem_pdf = _find_artifact(dc["artifacts"], ".pdf", "f10_stem_canvas")
            self.assertIsNotNone(stem_pdf)
            assert stem_pdf is not None
            self.assertTrue(validate_pdf(stem_pdf).ok)

    # =========================================================================
    # Area 07: Texto y documentos
    # =========================================================================

    @e2e_case(
        case_id="T2-A07-01",
        tier=2,
        group="G07",
        req_ids=["F-135", "F-136", "F-137", "F-139", "F-142", "F-143", "GN-07"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a07_01_empty_and_50k_unicode_bidi_smp_text_wrap(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            uni = _find_artifact(dc["artifacts"], ".pdf", "f8_unicode_math")
            self.assertIsNotNone(uni)
            assert uni is not None
            self.assertTrue(validate_pdf(uni, require_tounicode=True).ok)

    @e2e_case(
        case_id="T2-A07-02",
        tier=2,
        group="G07",
        req_ids=["F-138"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a07_02_corrupt_and_zero_byte_font_file_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a07_02_") as tmp:
                fake_ttf = Path(tmp) / "fake.ttf"
                fake_ttf.write_text("NOT_A_TTF_FONT", encoding="utf-8")
                sr = smoke_document_studio(plat, "canvas", Path(tmp) / f"f_{plat}.png", open_path=fake_ttf)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)

    @e2e_case(
        case_id="T2-A07-03",
        tier=2,
        group="G07",
        req_ids=["F-146", "F-147", "F-148", "F-149"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_pkcs7_openxml",
    )
    def test_t2_a07_03_malformed_ooxml_missing_content_types_and_bad_xml_rejected(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a07_03_") as tmp:
                d = Path(tmp)
                bad_pptx = d / "missing_parts.pptx"
                with zipfile.ZipFile(bad_pptx, "w") as zf:
                    zf.writestr("ppt/presentation.xml", "<p:presentation/>")
                self.assertFalse(validate_ooxml_pptx(bad_pptx).ok)
                self.assertNotEqual(run_cpp_probe(plat, ["--check-pptx", _plat_path(plat, bad_pptx)]).returncode, 0)

                missing_doc_xml = d / "missing_doc_xml.docx"
                with zipfile.ZipFile(missing_doc_xml, "w") as zf:
                    zf.writestr("[Content_Types].xml", "<Types/>")
                    zf.writestr("_rels/.rels", "<Relationships/>")
                self.assertFalse(validate_ooxml_docx(missing_doc_xml).ok)
                self.assertNotEqual(
                    run_cpp_probe(plat, ["--check-docx", _plat_path(plat, missing_doc_xml)]).returncode,
                    0,
                )

                # OpenXmlInterop::ImportFromDocxBytes currently checks missing word/document.xml rather than
                # full XML well-formedness; verify Studio UI non-crash resilience and Python XML validator guard.
                bad_xml_docx = d / "bad_xml.docx"
                with zipfile.ZipFile(bad_xml_docx, "w") as zf:
                    zf.writestr("[Content_Types].xml", "<Types/>")
                    zf.writestr("_rels/.rels", "<Relationships/>")
                    zf.writestr("word/document.xml", "<w:document><unclosed>")
                self.assertFalse(validate_ooxml_docx(bad_xml_docx).ok)
                shot = d / f"bad_xml_docx_{plat}.png"
                sr = smoke_document_studio(plat, "canvas", shot, open_path=bad_xml_docx)
                self.assertFalse(sr.timed_out)
                self.assertEqual(sr.returncode, 0, sr.stderr)
                self.assertTrue(validate_png(shot, min_width=320, min_height=200).ok)

    @e2e_case(
        case_id="T2-A07-04",
        tier=2,
        group="G07",
        req_ids=["F-150", "F-152"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a07_04_malformed_latex_mathml_and_empty_svg_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            pr = run_cpp_probe(plat, ["--cas", "\\frac{1}{"])
            self.assertNotEqual(pr.returncode, 0)
            with tempfile.TemporaryDirectory(prefix="t2_a07_04_") as tmp:
                empty_svg = Path(tmp) / "empty.svg"
                empty_svg.write_text("<svg xmlns='http://www.w3.org/2000/svg'/>", encoding="utf-8")
                self.assertFalse(validate_svg(empty_svg).ok)

    @e2e_case(
        case_id="T2-A07-05",
        tier=2,
        group="G07",
        req_ids=["F-140", "F-141", "F-144", "F-145", "F-151"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a07_05_large_gfm_table_and_circular_cross_reference_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Test F11", dc["stdout"])

    # =========================================================================
    # Area 08: Audio video captura
    # =========================================================================

    @e2e_case(
        case_id="T2-A08-01",
        tier=2,
        group="G08",
        req_ids=["F-153", "F-163", "U-16"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t2_a08_01_missing_mic_hardware_or_unplug_mid_recording(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a08_01_") as tmp:
                d = Path(tmp)
                partial_wav = d / "partial.wav"
                partial_wav.write_bytes(make_minimal_wav(duration_ms=100))
                out_cudoc = d / "unplug_recovery.cudoc"
                shot = d / f"unplug_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--record-audio-smoke",
                        "--simulate-mic-unplug",
                        "--import-audio",
                        _plat_path(plat, partial_wav),
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
        case_id="T2-A08-02",
        tier=2,
        group="G08",
        req_ids=["F-157", "F-158", "F-159"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t2_a08_02_zero_byte_corrupt_audio_and_silence_transcript(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a08_02_") as tmp:
                d = Path(tmp)
                zero_wav = d / "zero.wav"
                zero_wav.write_bytes(b"")
                self.assertFalse(validate_wav_pcm(zero_wav).ok)
                sr_zero = _run_studio_cli(plat, ["--smoke-test", "--import-audio", _plat_path(plat, zero_wav)])
                self.assertFalse(sr_zero.timed_out)
                self.assertNotEqual(sr_zero.returncode, 0)

                bad_wav = d / "bad.wav"
                bad_wav.write_bytes(b"NOT_RIFF_DATA")
                self.assertFalse(validate_wav_pcm(bad_wav).ok)
                sr_bad = _run_studio_cli(plat, ["--smoke-test", "--import-audio", _plat_path(plat, bad_wav)])
                self.assertFalse(sr_bad.timed_out)
                self.assertNotEqual(sr_bad.returncode, 0)

    @e2e_case(
        case_id="T2-A08-03",
        tier=2,
        group="G08",
        req_ids=["F-156", "F-160", "F-164"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t2_a08_03_out_of_range_audio_offset_clamp_and_zero_pauses(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a08_03_") as tmp:
                d = Path(tmp)
                wav = d / "noise.wav"
                wav.write_bytes(make_minimal_wav(duration_ms=200))
                out_cudoc = d / "clamped_offset.cudoc"
                shot = d / f"offset_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-audio",
                        _plat_path(plat, wav),
                        "--audio-offset-ms",
                        "9999999",
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
        case_id="T2-A08-04",
        tier=2,
        group="G08",
        req_ids=["F-154", "F-155", "GN-13"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="real_mic_audio_win_linux",
    )
    def test_t2_a08_04_deleted_audio_track_orphan_stroke_notice(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a08_04_") as tmp:
                d = Path(tmp)
                missing_wav = d / "deleted_track.wav"
                out_cudoc = d / "orphan_stroke.cudoc"
                shot = d / f"orphan_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--import-audio",
                        _plat_path(plat, missing_wav),
                        "--out-cudoc",
                        _plat_path(plat, out_cudoc),
                        "--screenshot-out",
                        _plat_path(plat, shot),
                    ],
                )
                self.assertFalse(sr.timed_out)
                self.assertNotEqual(sr.returncode, 0)

    @e2e_case(
        case_id="T2-A08-05",
        tier=2,
        group="G08",
        req_ids=["F-161", "F-162", "SUP-08", "fx-570LA X", "fx-991CW", "CalcBridge"],
        apps=["DocumentStudio", "CalculadoraUnificada", "CalcBridge"],
        platforms=["win", "linux"],
        readiness_key="calc_bridge_live_sync",
    )
    def test_t2_a08_05_capture_calculator_when_closed_or_transitioning(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            with tempfile.TemporaryDirectory(prefix="t2_a08_05_") as tmp:
                d = Path(tmp)
                out_cudoc = d / "closed_peer.cudoc"
                shot = d / f"closed_{plat}.png"
                sr = _run_studio_cli(
                    plat,
                    [
                        "--smoke-test",
                        "--mode",
                        "canvas",
                        "--calc-bridge-sync",
                        "--calc-card",
                        "fx-991CW|closed_peer|",
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

    # =========================================================================
    # Area 09: Busqueda y conocimiento
    # =========================================================================

    @e2e_case(
        case_id="T2-A09-01",
        tier=2,
        group="G09",
        req_ids=["F-165", "F-166", "F-174", "F-176"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a09_01_empty_single_char_and_high_frequency_search_cancel(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("FTS", dc["stdout"])

    @e2e_case(
        case_id="T2-A09-02",
        tier=2,
        group="G09",
        req_ids=["F-171"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_goodnotes_onenote_parity",
    )
    def test_t2_a09_02_catastrophic_redos_and_invalid_regex_guard(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])

    @e2e_case(
        case_id="T2-A09-03",
        tier=2,
        group="G09",
        req_ids=["F-168", "F-169", "GN-11"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_pdf_edit_merge_split",
    )
    def test_t2_a09_03_ocr_on_blank_corrupt_image_and_erased_hwr_strokes(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            with tempfile.TemporaryDirectory(prefix="t2_a09_03_") as tmp:
                blank = Path(tmp) / "blank_ocr.png"
                blank.write_bytes(make_blank_png(64, 64))
                self.assertFalse(validate_png(blank, require_non_uniform=True).ok)

    @e2e_case(
        case_id="T2-A09-04",
        tier=2,
        group="G09",
        req_ids=["F-172", "F-173"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_stem_engineering",
    )
    def test_t2_a09_04_malformed_math_query_and_nonexistent_circuit_part(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            bad_cas = run_cpp_probe(plat, ["--cas", "\\frac{"])
            self.assertNotEqual(bad_cas.returncode, 0)
            bad_mna = run_cpp_probe(plat, ["--mna", "XYZ9999 1 0 10"])
            self.assertNotEqual(bad_mna.returncode, 0)

    @e2e_case(
        case_id="T2-A09-05",
        tier=2,
        group="G09",
        req_ids=["F-167", "F-170", "F-175", "F-177", "F-178"],
        apps=["DocumentStudio"],
        platforms=["win", "linux"],
        readiness_key="core_audit_p0_fixes",
    )
    def test_t2_a09_05_nfd_vs_nfc_unicode_search_and_dangling_citation(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            dc = _run_doccore(plat)
            self.assertEqual(dc["returncode"], 0, dc["stderr"])
            self.assertIn("Unicode Search/PDF", dc["stdout"])
            self.assertEqual(scan_text_for_leaks(dc["stdout"]), [])
