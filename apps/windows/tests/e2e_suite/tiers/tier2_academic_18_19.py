"""Tier 2 Opaque-Box E2E Tests: Academic Groups 18 & 19 Boundaries, Negative & Security."""
from __future__ import annotations

import csv
import json
import shutil
import tempfile
import time
import unittest
from pathlib import Path

from e2e_suite.adapters.artifact_validators import validate_csv, validate_pdf
from e2e_suite.adapters.cli_runner import RunResult, run_win, run_wsl
from e2e_suite.adapters.config import REPO_ROOT, is_windows_host, resolve_paths, win_to_wsl_path
from e2e_suite.adapters.privacy_guard import scan_text_for_leaks
from e2e_suite.adapters.registry import e2e_case, get_runtime_context
from e2e_suite.fixtures.corrupt_generators import make_corrupt_horarios_catalogs
from e2e_suite.fixtures.mock_tec_server import MockTecServer
from e2e_suite.fixtures.synthetic_data import make_horarios_catalog_large


SYNTH_EMAIL = "student@example.invalid"
SYNTH_UNKNOWN_EMAIL = "unknown.user@example.invalid"
SYNTH_PASS = "SYNTH-TEST-PASS-0001"
SYNTH_WRONG_PASS = "SYNTH-WRONG-PASS-9999"


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


class TestGroup18TecDigitalTier2(unittest.TestCase):
    """Tier 2 boundary, negative, security, and fault-tolerance tests for Group 18."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_g18_t2_"))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="G18-T2-01",
        tier=2,
        group="G18",
        req_ids=["U-01", "U-02"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_01_invalid_auth_and_session_expired_vs_file_403(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                prof_bad = self.tmpdir / f"prof_bad_{plat}"
                prof_arg = _cli_path(plat, prof_bad)
                r_no_acct = _run_native_tool(
                    plat,
                    cli,
                    ["login", "--base-url", srv.base_url, "--profile", prof_arg],
                    env={"TEC_EMAIL": SYNTH_UNKNOWN_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertNotEqual(r_no_acct.returncode, 0)
                self.assertIn("no_account", r_no_acct.stdout + r_no_acct.stderr)
                self.assertFalse((prof_bad / "session.dpapi").exists())

                r_bad_pw = _run_native_tool(
                    plat,
                    cli,
                    ["login", "--base-url", srv.base_url, "--profile", prof_arg],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_WRONG_PASS},
                )
                self.assertNotEqual(r_bad_pw.returncode, 0)
                self.assertIn("bad_password", r_bad_pw.stdout + r_bad_pw.stderr)

                # Single-file 403 must NOT trip session_expired while /dotlrn/ returns 200
                r_403 = _run_native_tool(
                    plat,
                    cli,
                    ["download-file", "--base-url", srv.base_url, "--object-id", "900004", "--out", _cli_path(plat, self.tmpdir / f"f403_{plat}")],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertNotIn("session_expired", r_403.stdout + r_403.stderr)

                # Mid-batch /register/ redirect must trip session_expired latch
                srv.set_session_invalidated(True)
                r_exp = _run_native_tool(
                    plat,
                    cli,
                    ["sync", "--base-url", srv.base_url, "--folder-id", "153108817", "--out", _cli_path(plat, self.tmpdir / f"exp_{plat}")],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertNotEqual(r_exp.returncode, 0)
                self.assertIn("session_expired", r_exp.stdout + r_exp.stderr)

    @e2e_case(
        case_id="G18-T2-02",
        tier=2,
        group="G18",
        req_ids=["U-05"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_02_strict_readonly_policy_blocks_mutations(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                blocked_cases = [
                    ("POST", f"{srv.base_url}/tda-notifications/ajax/delete_all_user_notifications", "{}"),
                    ("POST", f"{srv.base_url}/tda-notifications/ajax/update_notification_state?notification_id=1&state=1", "{}"),
                    ("GET", f"{srv.base_url}/dotlrn/calendar/cal-item-new?date=2026-09-01", ""),
                    ("GET", f"{srv.base_url}/dotlrn/x/member-confirm-2", ""),
                    ("GET", f"{srv.base_url}/dotlrn/x?action=delete", ""),
                    ("POST", f"{srv.base_url}/tda-prematricula/ajax/prematricula_est", '{"tipo":3}'),
                    ("POST", f"{srv.base_url}/graphql", '{"query":"mutation { enrollCourse { code } }"}'),
                ]
                for method, url, body in blocked_cases:
                    res = _run_native_tool(
                        plat,
                        cli,
                        ["policy-request", "--method", method, "--url", url, "--body", body],
                    )
                    self.assertNotEqual(res.returncode, 0, f"Policy failed to block {method} {url}")

                self.assertEqual(
                    srv.mutation_received_count,
                    0,
                    f"Mutating requests reached mock server: {srv.request_log}",
                )

                # Allowlisted GraphQL query & read-only prematricula tipo=1 succeed
                ok_gql = _run_native_tool(
                    plat,
                    cli,
                    ["policy-request", "--method", "POST", "--url", f"{srv.base_url}/graphql", "--body", '{"query":"query { courses { code } }"}'],
                )
                self.assertEqual(ok_gql.returncode, 0, ok_gql.stderr or ok_gql.stdout)

                ok_premat = _run_native_tool(
                    plat,
                    cli,
                    [
                        "policy-request",
                        "--method",
                        "POST",
                        "--url",
                        f"{srv.base_url}/tda-prematricula/ajax/prematricula_est",
                        "--body",
                        '{"tipo":1}',
                    ],
                )
                self.assertEqual(ok_premat.returncode, 0, ok_premat.stderr or ok_premat.stdout)
                self.assertIn("EL2113", ok_premat.stdout)
                self.assertEqual(
                    srv.mutation_received_count,
                    0,
                    f"Allowlisted read-only requests triggered mock server mutation counter: {srv.request_log}",
                )

    @e2e_case(
        case_id="G18-T2-03",
        tier=2,
        group="G18",
        req_ids=["U-04"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_03_bug059_calendar_url_collapse_and_canonical_determinism(self) -> None:
        urls = [
            f"https://tecdigital.tec.ac.cr/dotlrn/calendar/view?date=2026-09-{d:02d}&view=list&period_days={(7 if d % 2 else 30)}"
            for d in range(1, 31)
        ] + [
            "https://tecdigital.tec.ac.cr/dotlrn/calendar/view?date=2026-10-05&view=list&period_days=7",
            "https://tecdigital.tec.ac.cr/dotlrn/catedras.tec/one-community?date=2024-09-25&page_num=1",
            "https://tecdigital.tec.ac.cr/dotlrn/catedras.tec/one-community?page_num=1",
            "https://tecdigital.tec.ac.cr/dotlrn/catedras.tec/one-community?page_num=2",
            "https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/x?date=2026-09-02&period_days=30",
            "https://tecdigital.tec.ac.cr/dotlrn/file-storage/view/x?date=2026-09-02&period_days=7",
        ]

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            in_file = self.tmpdir / f"urls_{plat}.json"
            out_file = self.tmpdir / f"canon_{plat}.json"
            in_file.write_text(json.dumps(urls), encoding="utf-8")

            res = _run_native_tool(
                plat,
                cli,
                ["canonicalize-urls", "--in", _cli_path(plat, in_file), "--out", _cli_path(plat, out_file)],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            mapped = json.loads(out_file.read_text(encoding="utf-8"))

            # All 30 September calendar days collapse to 1 canonical entry
            sept_keys = {mapped[u]["page_key"] for u in urls[:30]}
            self.assertEqual(len(sept_keys), 1)
            self.assertNotEqual(mapped[urls[0]]["page_key"], mapped[urls[30]]["page_key"])
            # File-storage preserves period_days
            self.assertNotEqual(mapped[urls[-1]]["canonical_url"], mapped[urls[-2]]["canonical_url"])

    @e2e_case(
        case_id="G18-T2-04",
        tier=2,
        group="G18",
        req_ids=["U-07"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_04_interrupted_part_range_resume_and_bad_quarantine(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                out_dir = self.tmpdir / f"dl_faults_{plat}"
                out_arg = _cli_path(plat, out_dir)
                r_resume = _run_native_tool(
                    plat,
                    cli,
                    ["download-file", "--base-url", srv.base_url, "--object-id", "900001", "--out", out_arg],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(r_resume.returncode, 0, r_resume.stderr or r_resume.stdout)
                pdf_out = out_dir / "Drop_Resume_900001.pdf"
                self.assertTrue(validate_pdf(pdf_out).ok)
                self.assertFalse((out_dir / "Drop_Resume_900001.pdf.part").exists())

                r_bad = _run_native_tool(
                    plat,
                    cli,
                    ["download-file", "--base-url", srv.base_url, "--object-id", "900003", "--mode", "badmagic", "--out", out_arg],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertNotEqual(r_bad.returncode, 0)
                self.assertTrue(any(p.name.endswith(".bad") for p in out_dir.rglob("*")))

    @e2e_case(
        case_id="G18-T2-05",
        tier=2,
        group="G18",
        req_ids=["U-03"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_05_offline_disconnection_and_fail_closed_vault_decryption(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            prof = self.tmpdir / f"corrupt_vault_{plat}"
            prof.mkdir(parents=True, exist_ok=True)
            (prof / "session.dpapi").write_bytes(b"CORRUPTED_CIPHERTEXT_TAG_MISMATCH_0000")
            prof_arg = _cli_path(plat, prof)
            r_vault = _run_native_tool(
                plat,
                cli,
                ["vault-check", "--profile", prof_arg],
                env={"LOCALAPPDATA": prof_arg, "XDG_DATA_HOME": prof_arg},
            )
            self.assertNotEqual(r_vault.returncode, 0)

            # Offline closed port must return structured network_offline without crash
            r_off = _run_native_tool(
                plat,
                cli,
                ["sync", "--base-url", "http://127.0.0.1:9", "--folder-id", "153108817", "--out", _cli_path(plat, self.tmpdir / f"off_{plat}")],
                env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
            )
            self.assertNotEqual(r_off.returncode, 0)
            self.assertIn("network", (r_off.stdout + r_off.stderr).lower())

    @e2e_case(
        case_id="G18-T2-06",
        tier=2,
        group="G18",
        req_ids=["U-06"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_06_zero_credential_cookie_pii_and_zoom_pwd_leak(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            dirty = self.tmpdir / f"dirty_{plat}.har"
            ephem_cookie = "raw_leaked_val_1234"
            ephem_zoom = "ZOOM_SECRET_77"
            ephem_csrf = "csrf_synth_99"
            dirty.write_text(
                json.dumps({
                    "email": SYNTH_EMAIL,
                    "password": SYNTH_PASS,
                    "cookie": "ad_session_id" + "=" + ephem_cookie,
                    "zoom": "https://zoom.example.invalid/j/999?pwd=" + ephem_zoom,
                    "csrf_token": ephem_csrf,
                }),
                encoding="utf-8",
            )
            clean = self.tmpdir / f"clean_{plat}.har"
            res = _run_native_tool(
                plat,
                cli,
                ["sanitize", "--in", _cli_path(plat, dirty), "--out", _cli_path(plat, clean)],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            leaks = scan_text_for_leaks(
                clean.read_text(encoding="utf-8"),
                extra_secrets=[SYNTH_PASS, ephem_cookie, ephem_zoom, ephem_csrf],
            )
            self.assertEqual(leaks, [], f"Sanitizer left secrets: {leaks}")

    @e2e_case(
        case_id="G18-T2-07",
        tier=2,
        group="G18",
        req_ids=["U-07"],
        apps=["DocumentStudio", "AcademicTecDigital"],
        platforms=["win", "linux"],
        readiness_key="academic_tec_digital",
    )
    def test_g18_t2_07_upstream_404_max_folder_depth_and_concurrent_lock(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_academic_tec_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled tec_digital_cli binary for {plat}")

            with MockTecServer() as srv:
                res = _run_native_tool(
                    plat,
                    cli,
                    [
                        "sync",
                        "--base-url",
                        srv.base_url,
                        "--folder-id",
                        "99999001",
                        "--max-depth",
                        "10",
                        "--out",
                        _cli_path(plat, self.tmpdir / f"deep_{plat}"),
                    ],
                    env={"TEC_EMAIL": SYNTH_EMAIL, "TEC_PASSWORD": SYNTH_PASS},
                )
                self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
                folder_hits = [r for r in srv.request_log if "/api/folder/" in r["path"]]
                self.assertLessEqual(len(folder_hits), 11, "Folder depth guard exceeded <=10")


class TestGroup19HorariosTecTier2(unittest.TestCase):
    """Tier 2 boundary, negative, stress, and injection tests for Group 19 (HorariosTEC)."""

    def setUp(self) -> None:
        self.tmpdir = Path(tempfile.mkdtemp(prefix="e2e_g19_t2_"))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    @e2e_case(
        case_id="G19-T2-01",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_01_zero_conflict_free_schedules_warning(self) -> None:
        cat = {
            "semester": "S-2-2026",
            "courses": [
                {"code": "AA1", "name": "C1", "groups": [{"id": "G1", "sessions": [{"day": "L", "start": "08:00", "end": "10:00"}]}]},
                {"code": "BB1", "name": "C2", "groups": [{"id": "G1", "sessions": [{"day": "L", "start": "08:00", "end": "10:00"}]}]},
            ],
        }
        cp = self.tmpdir / "conflict.json"
        cp.write_text(json.dumps(cat), encoding="utf-8")

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            out_p = self.tmpdir / f"out_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, cp), "--out-json", _cli_path(plat, out_p)],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            data = json.loads(out_p.read_text(encoding="utf-8"))
            self.assertEqual(data.get("schedules"), [])
            self.assertIn("NO_CONFLICT_FREE_SCHEDULES", data.get("status", "") + res.stdout + res.stderr)

    @e2e_case(
        case_id="G19-T2-02",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_02_empty_and_malformed_catalog_json_handling(self) -> None:
        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            for label, raw in make_corrupt_horarios_catalogs():
                if label == "empty_groups":
                    continue
                bad_p = self.tmpdir / f"{label}_{plat}.json"
                out_p = self.tmpdir / f"{label}_{plat}_out.json"
                bad_p.write_text(raw, encoding="utf-8")
                res = _run_native_tool(
                    plat,
                    cli,
                    ["solve", "--catalog", _cli_path(plat, bad_p), "--out-json", _cli_path(plat, out_p)],
                )
                self.assertNotEqual(res.returncode, 0, f"Malformed catalog '{label}' must fail validation")

    @e2e_case(
        case_id="G19-T2-03",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_03_boundary_adjacent_vs_1min_overlap_and_midnight_slots(self) -> None:
        adjacent_cat = {
            "courses": [
                {"code": "A1", "name": "A", "groups": [{"id": "1", "sessions": [{"day": "L", "start": "07:55", "end": "09:40"}]}]},
                {"code": "B1", "name": "B", "groups": [{"id": "1", "sessions": [{"day": "L", "start": "09:40", "end": "11:30"}]}]},
                {"code": "C1", "name": "C", "groups": [{"id": "1", "sessions": [{"day": "S", "start": "00:00", "end": "00:50"}]}]},
                {"code": "D1", "name": "D", "groups": [{"id": "1", "sessions": [{"day": "S", "start": "22:00", "end": "23:59"}]}]},
            ]
        }
        overlap_cat = {
            "courses": [
                {"code": "A1", "name": "A", "groups": [{"id": "1", "sessions": [{"day": "L", "start": "07:55", "end": "09:41"}]}]},
                {"code": "B1", "name": "B", "groups": [{"id": "1", "sessions": [{"day": "L", "start": "09:40", "end": "11:30"}]}]},
            ]
        }
        p_adj = self.tmpdir / "adj.json"
        p_ov = self.tmpdir / "ov.json"
        p_adj.write_text(json.dumps(adjacent_cat), encoding="utf-8")
        p_ov.write_text(json.dumps(overlap_cat), encoding="utf-8")

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            o_adj = self.tmpdir / f"adj_out_{plat}.json"
            r_adj = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, p_adj), "--out-json", _cli_path(plat, o_adj)],
            )
            self.assertEqual(r_adj.returncode, 0, r_adj.stderr or r_adj.stdout)
            self.assertEqual(len(json.loads(o_adj.read_text(encoding="utf-8")).get("schedules", [])), 1)

            o_ov = self.tmpdir / f"ov_out_{plat}.json"
            r_ov = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, p_ov), "--out-json", _cli_path(plat, o_ov)],
            )
            self.assertEqual(r_ov.returncode, 0, r_ov.stderr or r_ov.stdout)
            self.assertEqual(len(json.loads(o_ov.read_text(encoding="utf-8")).get("schedules", [])), 0)

    @e2e_case(
        case_id="G19-T2-04",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_04_duplicate_courses_duplicate_groups_and_empty_groups(self) -> None:
        cat = {
            "courses": [
                {"code": "EL2113", "name": "C1", "groups": [{"id": "Gr 50", "sessions": [{"day": "L", "start": "08:00", "end": "09:00"}]}]},
                {"code": "EL2113", "name": "C1 dup", "groups": [{"id": "Gr 50", "sessions": [{"day": "M", "start": "08:00", "end": "09:00"}]}]},
                {"code": "CS0000", "name": "Empty", "groups": []},
            ]
        }
        p = self.tmpdir / "dup_empty.json"
        p.write_text(json.dumps(cat), encoding="utf-8")

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            o = self.tmpdir / f"dup_empty_out_{plat}.json"
            res = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, p), "--out-json", _cli_path(plat, o)],
            )
            combined = res.stdout + res.stderr + (o.read_text(encoding="utf-8") if o.exists() else "")
            self.assertIn("CS0000", combined)

    @e2e_case(
        case_id="G19-T2-05",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_05_large_combinatorial_catalog_performance_and_cap(self) -> None:
        p = self.tmpdir / "large.json"
        p.write_text(json.dumps(make_horarios_catalog_large(8, 6)), encoding="utf-8")

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            o = self.tmpdir / f"large_out_{plat}.json"
            t0 = time.perf_counter()
            res = _run_native_tool(
                plat,
                cli,
                ["solve", "--catalog", _cli_path(plat, p), "--max-results", "50", "--out-json", _cli_path(plat, o)],
            )
            elapsed_ms = (time.perf_counter() - t0) * 1000.0
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            self.assertLess(elapsed_ms, 5000.0, f"Large catalog solve took {elapsed_ms:.1f} ms")
            schedules = json.loads(o.read_text(encoding="utf-8")).get("schedules", [])
            self.assertLessEqual(len(schedules), 50)

    @e2e_case(
        case_id="G19-T2-06",
        tier=2,
        group="G19",
        req_ids=["U-12"],
        apps=["DocumentStudio", "HorariosTEC"],
        platforms=["win", "linux"],
        readiness_key="horarios_tec",
    )
    def test_g19_t2_06_csv_formula_injection_and_special_character_escaping(self) -> None:
        cat = {
            "courses": [
                {
                    "code": "INJ101",
                    "name": 'Circuitos, "AC/DC"',
                    "groups": [{"id": "G1", "sessions": [{"day": "L", "start": "08:00", "end": "09:30"}]}],
                },
                {
                    "code": "INJ102",
                    "name": "=CMD|' /C calc'!A0",
                    "groups": [{"id": "G1", "sessions": [{"day": "K", "start": "08:00", "end": "09:30"}]}],
                },
            ]
        }
        p = self.tmpdir / "inj.json"
        p.write_text(json.dumps(cat), encoding="utf-8")

        for plat in _target_platforms(["win", "linux"]):
            cli = _find_horarios_cli(plat)
            if cli is None:
                self.fail(f"Missing compiled horarios_tec_cli binary for {plat}")

            o_json = self.tmpdir / f"inj_out_{plat}.json"
            o_csv = self.tmpdir / f"inj_out_{plat}.csv"
            res = _run_native_tool(
                plat,
                cli,
                [
                    "solve",
                    "--catalog",
                    _cli_path(plat, p),
                    "--out-json",
                    _cli_path(plat, o_json),
                    "--out-csv",
                    _cli_path(plat, o_csv),
                ],
            )
            self.assertEqual(res.returncode, 0, res.stderr or res.stdout)
            self.assertTrue(validate_csv(o_csv, min_rows=2).ok)
            with o_csv.open("r", encoding="utf-8", newline="") as fh:
                rows = list(csv.reader(fh))
            flat_cells = [cell for row in rows[1:] for cell in row]
            self.assertFalse(any(c.startswith(("=", "+", "-", "@")) for c in flat_cells), f"Unescaped formula cell in {flat_cells}")
