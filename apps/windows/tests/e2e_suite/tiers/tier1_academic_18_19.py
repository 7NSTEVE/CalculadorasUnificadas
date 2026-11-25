"""Tier 1 Opaque-Box E2E Tests: Academic Groups 18 (TEC Digital) & 19 (HorariosTEC)."""
from __future__ import annotations

import hashlib
import json
import shutil
import tempfile
import unittest
import urllib.request
from pathlib import Path
from typing import Any

from e2e_suite.adapters.artifact_validators import validate_csv, validate_pdf, validate_zip_container
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks, scan_tree_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.fixtures.mock_tec_server import MockTecServer
from e2e_suite.fixtures.synthetic_data import (
    make_horarios_catalog_canonical,
    make_minimal_docx,
    make_minimal_pdf,
    make_minimal_xlsx,
)


SYNTH_EMAIL = "student@example.invalid"
SYNTH_PASS = "SYNTH-TEST-PASS-0001"


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


def _find_academic_tec_cli(plat: str) -> Path | None:
    paths = resolve_paths(REPO_ROOT)
    key = "linux_tec_digital_cli" if plat == "linux" else "win_tec_digital_cli"
    cli = paths.get(key)
    if cli is not None and cli.exists():
        return cli
    return None


def _find_horarios_cli(plat: str) -> Path | None:
    paths = resolve_paths(REPO_ROOT)
    key = "linux_horarios_tec_cli" if plat == "linux" else "win_horarios_tec_cli"
    cli = paths.get(key)
    if cli is not None and cli.exists():
        return cli
    return None


def _run_native_tool(plat: str, exe: Path, args: list[str], env: dict[str, str] | None = None) -> RunResult:
    if plat == "linux":
        cmd = [win_to_wsl_path(exe), *[str(a) for a in args]]
        return run_wsl(cmd, env=env, timeout=45, cwd_linux=win_to_wsl_path(REPO_ROOT))
    return run_win([str(exe), *[str(a) for a in args]], env=env, timeout=45, cwd=REPO_ROOT)


def _canonical_oracle_count(catalog: dict[str, Any]) -> int:
    """Mathematical oracle for conflict-free combinations across all courses in `catalog`."""
    courses = catalog.get("courses", [])

    def _to_min(hhmm: str) -> int:
        hh, mm = hhmm.split(":")
        return int(hh) * 60 + int(mm)

    def _overlaps(s1: list[dict[str, str]], s2: list[dict[str, str]]) -> bool:
        for a in s1:
            for b in s2:
                if a["day"] == b["day"]:
                    a0, a1 = _to_min(a["start"]), _to_min(a["end"])
                    b0, b1 = _to_min(b["start"]), _to_min(b["end"])
                    if a0 < b1 and b0 < a1:
                        return True
        return False

    valid = 0

    def _dfs(idx: int, chosen_sessions: list[dict[str, str]]) -> None:
        nonlocal valid
        if idx == len(courses):
            valid += 1
            return
        for grp in courses[idx].get("groups", []):
            sess = grp.get("sessions", [])
            if not _overlaps(chosen_sessions, sess):
                _dfs(idx + 1, chosen_sessions + sess)

    _dfs(0, [])
    return valid


class TestGroup18TecDigitalTier1(unittest.TestCase):
    """Tier 1 core feature coverage for Group 18 (TEC Digital HTTP/Offline/Sync)."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_g18_t1_"))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="G18-T1-01",
        tier=1,
        group="G18",
        req_ids=["U-01", "U-02"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_01_openacs_login_and_dotlrn_session(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            profile_dir = self.tmpdir / f"profile_{plat}"
            profile_dir.mkdir(parents=True, exist_ok=True)
            with MockTecServer() as srv:
                env = {
                    "TEC_BASE_URL": srv.base_url,
                    "TEC_EMAIL": SYNTH_EMAIL,
                    "TEC_PASSWORD": SYNTH_PASS,
                    "TEC_PROFILE_DIR": _cli_path(plat, profile_dir),
                }
                res = _run_native_tool(
                    plat,
                    cli,
                    ["login", "--base-url", srv.base_url, "--check-session"],
                    env=env,
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                self.assertIn("sessionActive", res.stdout)

                # Verify opaque-box OpenACS login + cookie-jar contract directly
                login_req = urllib.request.Request(
                    f"{srv.base_url}/api/login/new-form/",
                    data=json.dumps({"email": SYNTH_EMAIL, "password": SYNTH_PASS, "returnUrl": "/dotlrn/"}).encode("utf-8"),
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with urllib.request.urlopen(login_req, timeout=10) as resp:
                    self.assertEqual(resp.status, 200)
                    body = json.loads(resp.read().decode("utf-8"))
                    self.assertEqual(body.get("status"), "ok")
                    set_cookies = resp.headers.get_all("Set-Cookie") or []
                    joined = "; ".join(set_cookies)
                    self.assertIn("ad_session_id=", joined)
                    self.assertIn("ad_user_login=", joined)

                self.assertEqual(srv.mutation_received_count, 0)

    @e2e_case(
        case_id="G18-T1-02",
        tier=1,
        group="G18",
        req_ids=["U-03"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_02_encrypted_vault_roundtrip(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            profile_dir = self.tmpdir / f"vault_profile_{plat}"
            profile_dir.mkdir(parents=True, exist_ok=True)
            prof_arg = _cli_path(plat, profile_dir)
            with MockTecServer() as srv:
                env = {
                    "TEC_BASE_URL": srv.base_url,
                    "TEC_EMAIL": SYNTH_EMAIL,
                    "TEC_PASSWORD": SYNTH_PASS,
                    "LOCALAPPDATA": prof_arg,
                    "XDG_DATA_HOME": prof_arg,
                }
                r1 = _run_native_tool(
                    plat,
                    cli,
                    ["vault-save", "--profile", prof_arg, "--base-url", srv.base_url],
                    env=env,
                )
                self.assertEqual(r1.returncode, 0, r1.stderr or r1.stdout)
                r2 = _run_native_tool(
                    plat,
                    cli,
                    ["vault-check", "--profile", prof_arg, "--base-url", srv.base_url],
                    env={"LOCALAPPDATA": prof_arg, "XDG_DATA_HOME": prof_arg},
                )
                self.assertEqual(r2.returncode, 0, r2.stderr or r2.stdout)
                self.assertEqual(scan_tree_for_leaks(profile_dir, extra_secrets=[SYNTH_PASS, "synth_sess_0001"]), [])

    @e2e_case(
        case_id="G18-T1-03",
        tier=1,
        group="G18",
        req_ids=["U-07"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_03_recursive_file_storage_cas_blobs(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            mirror_dir = self.tmpdir / f"mirror_{plat}"
            with MockTecServer() as srv:
                res = _run_native_tool(
                    plat,
                    cli,
                    ["sync", "--base-url", srv.base_url, "--folder-id", "153108817", "--out", _cli_path(plat, mirror_dir)],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                blobs_dir = mirror_dir / "downloads" / "_blobs"
                self.assertTrue(blobs_dir.exists(), f"Missing _blobs CAS directory: {blobs_dir}")
                blob_files = [p for p in blobs_dir.rglob("*") if p.is_file()]
                self.assertGreaterEqual(len(blob_files), 3)
                for bf in blob_files:
                    digest = hashlib.sha256(bf.read_bytes()).hexdigest()
                    self.assertEqual(bf.name, digest)
                    self.assertEqual(bf.parent.name, digest[:2])

    @e2e_case(
        case_id="G18-T1-04",
        tier=1,
        group="G18",
        req_ids=["U-08"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_04_offline_material_index_and_search(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            mirror_dir = self.tmpdir / f"mirror_idx_{plat}"
            blobs_dir = mirror_dir / "downloads" / "_blobs"
            for payload in (
                make_minimal_pdf(["Ecuacion de Maxwell"], use_flate=True),
                make_minimal_docx(["Transformada de Laplace"]),
                make_minimal_xlsx([["Rubro", "Valor"], ["Nota_Final", 95]]),
            ):
                digest = hashlib.sha256(payload).hexdigest()
                sub = blobs_dir / digest[:2]
                sub.mkdir(parents=True, exist_ok=True)
                (sub / digest).write_bytes(payload)

            mirror_arg = _cli_path(plat, mirror_dir)
            r_idx = _run_native_tool(plat, cli, ["index-materials", "--mirror", mirror_arg])
            self.assertEqual(r_idx.returncode, 0, r_idx.stderr or r_idx.stdout)
            stats_path = mirror_dir / "material_index" / "stats.json"
            self.assertTrue(stats_path.exists())
            stats = json.loads(stats_path.read_text(encoding="utf-8"))
            self.assertEqual(stats.get("missing_blobs"), 0)
            self.assertGreater(stats.get("extracted_chars", 0), 0)

            r_q = _run_native_tool(plat, cli, ["search-materials", "--mirror", mirror_arg, "--query", "Laplace"])
            self.assertEqual(r_q.returncode, 0, r_q.stderr or r_q.stdout)
            self.assertIn("Laplace", r_q.stdout)

    @e2e_case(
        case_id="G18-T1-05",
        tier=1,
        group="G18",
        req_ids=["U-09"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_05_tda_tds_curriculum_and_sedes_non_null(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                out_json = self.tmpdir / f"curriculum_{plat}.json"
                res = _run_native_tool(
                    plat,
                    cli,
                    ["fetch-curriculum", "--base-url", srv.base_url, "--out", _cli_path(plat, out_json)],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                data = json.loads(out_json.read_text(encoding="utf-8"))
                sedes = data.get("sedes", [])
                self.assertGreaterEqual(len(sedes), 2)
                for sede in sedes:
                    self.assertTrue(sede.get("id"))
                    self.assertTrue(sede.get("nombre"))
                self.assertGreaterEqual(len(data.get("carreras", [])), 1)
                self.assertGreaterEqual(len(data.get("planes", [])), 1)

    @e2e_case(
        case_id="G18-T1-06",
        tier=1,
        group="G18",
        req_ids=["U-09"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_06_expediente_notifications_evaluations_readonly(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                out_json = self.tmpdir / f"expediente_{plat}.json"
                res = _run_native_tool(
                    plat,
                    cli,
                    ["fetch-expediente", "--base-url", srv.base_url, "--out", _cli_path(plat, out_json)],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                data = json.loads(out_json.read_text(encoding="utf-8"))
                self.assertGreater(data.get("unread_notifications", 0), 0)
                self.assertGreaterEqual(len(data.get("notas", [])), 1)
                self.assertEqual(srv.mutation_received_count, 0)

    @e2e_case(
        case_id="G18-T1-07",
        tier=1,
        group="G18",
        req_ids=["U-04", "U-05", "U-06", "U-10", "U-11", "U-17"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t1_07_pure_https_headless_and_consolidation_parity(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            mirror_dir = self.tmpdir / f"headless_mirror_{plat}"
            mirror_arg = _cli_path(plat, mirror_dir)
            with MockTecServer() as srv:
                res = _run_native_tool(
                    plat,
                    cli,
                    ["http-crawl", "--base-url", srv.base_url, "--out", mirror_arg],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS, "PLAYWRIGHT_BROWSERS_PATH": "0"},
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                self.assertEqual(srv.mutation_received_count, 0)
                v_res = _run_native_tool(plat, cli, ["verify-offline", "--mirror", mirror_arg])
                self.assertEqual(v_res.returncode, 0, v_res.stderr or v_res.stdout)
                self.assertEqual(scan_text_for_leaks(res.stdout + "\n" + res.stderr, extra_secrets=[SYNTH_PASS]), [])
                self.assertEqual(scan_tree_for_leaks(mirror_dir, extra_secrets=[SYNTH_PASS]), [])


class TestGroup19HorariosTecTier1(unittest.TestCase):
    """Tier 1 core feature coverage for Group 19 (HorariosTEC University Scheduler)."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_g19_t1_"))
        self.catalog_path = self.tmpdir / "catalog.json"
        self.catalog_path.write_text(
            json.dumps(make_horarios_catalog_canonical(), ensure_ascii=False, indent=2),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="G19-T1-01",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_01_catalog_json_parsing_and_normalization(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_json = self.tmpdir / f"parsed_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                ["parse", "--catalog", _cli_path(plat, self.catalog_path), "--out", _cli_path(plat, out_json)],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            parsed = json.loads(out_json.read_text(encoding="utf-8"))
            self.assertEqual(len(parsed.get("courses", [])), 4)
            names = [c["name"] for c in parsed["courses"]]
            self.assertTrue(any("Física" in n for n in names))
            self.assertTrue(any("Cálculo" in n for n in names))

    @e2e_case(
        case_id="G19-T1-02",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_02_exhaustive_conflict_free_backtracker(self) -> None:
        expected_combos = _canonical_oracle_count(make_horarios_catalog_canonical())
        self.assertEqual(expected_combos, 7)

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_json = self.tmpdir / f"schedules_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, self.catalog_path), "--out-json", _cli_path(plat, out_json)],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            data = json.loads(out_json.read_text(encoding="utf-8"))
            schedules = data.get("schedules", [])
            self.assertEqual(len(schedules), expected_combos)
            for sched in schedules:
                self.assertEqual(len(sched.get("courses", [])), 4)

    @e2e_case(
        case_id="G19-T1-03",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_03_multi_heuristic_scoring_and_ranking(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_json = self.tmpdir / f"ranked_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                [
                    "solve",
                    "--catalog",
                    _cli_path(plat, self.catalog_path),
                    "--prefer-morning",
                    "--compactness",
                    "--out-json",
                    _cli_path(plat, out_json),
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            schedules = json.loads(out_json.read_text(encoding="utf-8")).get("schedules", [])
            self.assertGreaterEqual(len(schedules), 3)
            scores = [float(s["score"]) for s in schedules]
            self.assertEqual(scores, sorted(scores, reverse=True))

    @e2e_case(
        case_id="G19-T1-04",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_04_blocked_busy_slots_and_filters(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_json = self.tmpdir / f"filtered_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                [
                    "solve",
                    "--catalog",
                    _cli_path(plat, self.catalog_path),
                    "--busy",
                    "K:09:30-12:00",
                    "--include",
                    "MA1103:Gr 50,Gr 52",
                    "--out-json",
                    _cli_path(plat, out_json),
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            schedules = json.loads(out_json.read_text(encoding="utf-8")).get("schedules", [])
            for sched in schedules:
                for course in sched.get("courses", []):
                    if course["code"] == "MA1103":
                        self.assertIn(course["group_id"], ("Gr 50", "Gr 52"))
                    for s in course.get("sessions", []):
                        if s["day"] == "K":
                            self.assertFalse(s["start"] < "12:00" and "09:30" < s["end"])

    @e2e_case(
        case_id="G19-T1-05",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_05_json_and_csv_schedule_export_roundtrip(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_json = self.tmpdir / f"horario_export_test_{plat}.json"
            out_csv = self.tmpdir / f"horario_export_test_{plat}.csv"
            res = _run_native_tool(
                plat,
                cli,
                [
                    "solve",
                    "--catalog",
                    _cli_path(plat, self.catalog_path),
                    "--out-json",
                    _cli_path(plat, out_json),
                    "--out-csv",
                    _cli_path(plat, out_csv),
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            csv_res = validate_csv(out_csv, min_rows=2)
            self.assertTrue(csv_res.ok, f"CSV validation failed: {csv_res.errors}")
            parsed = json.loads(out_json.read_text(encoding="utf-8"))
            self.assertGreaterEqual(len(parsed.get("schedules", [])), 1)
            self.assertIn("rank", parsed["schedules"][0])
            self.assertIn("score", parsed["schedules"][0])

    @e2e_case(
        case_id="G19-T1-06",
        tier=1,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t1_06_seat_availability_monitor_update(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            with MockTecServer() as srv:
                out_before = self.tmpdir / f"before_seats_{plat}.json"
                r1 = _run_native_tool(
                    plat,
                    cli,
                    [
                        "solve",
                        "--catalog",
                        _cli_path(plat, self.catalog_path),
                        "--seats-url",
                        f"{srv.base_url}/api/horarios/seats",
                        "--require-available-seats",
                        "--out-json",
                        _cli_path(plat, out_before),
                    ],
                )
                self.assertEqual(r1.returncode, 0, r1.stderr or r1.stdout)
                self.assertEqual(len(json.loads(out_before.read_text(encoding="utf-8")).get("schedules", [])), 7)

                srv.set_seat_count("EL2113:Gr 50", 0)
                out_after = self.tmpdir / f"after_seats_{plat}.json"
                r2 = _run_native_tool(
                    plat,
                    cli,
                    [
                        "solve",
                        "--catalog",
                        _cli_path(plat, self.catalog_path),
                        "--seats-url",
                        f"{srv.base_url}/api/horarios/seats",
                        "--require-available-seats",
                        "--out-json",
                        _cli_path(plat, out_after),
                    ],
                )
                self.assertEqual(r2.returncode, 0, r2.stderr or r2.stdout)
                self.assertEqual(len(json.loads(out_after.read_text(encoding="utf-8")).get("schedules", [])), 0)
