"""Automated end-to-end verification suite for the TEC Matrícula MCP Server, DPAPI Vault,
Seat Sniper daemon, EnviaInforme receipt sender, Playwright failover, and Lifecycle Hook.

Runs strictly against a local HTTP mock server on 127.0.0.1 (ZERO mutations against live ITCR).
"""

from __future__ import annotations

import http.server
import json
import os
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from typing import Any, Dict, List

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools" / "tec_matricula"
sys.path.insert(0, str(TOOLS_DIR))

import tec_matricula_hook  # noqa: E402
import tec_matricula_mcp_server as srv  # noqa: E402


def _wrap_d(obj: Any) -> bytes:
    inner = obj if isinstance(obj, str) else json.dumps(obj, ensure_ascii=False)
    return json.dumps({"d": inner}, ensure_ascii=False).encode("utf-8")


class _MockState:
    def __init__(self) -> None:
        self.login_count = 0
        self.mutating_calls = 0
        self.informe_sent_count = 0
        self.dom_enroll_hits = 0
        self.horarios_poll_count = 0
        self.release_seat_after_poll = 0  # If >0, MA1102 seat opens at this poll #
        self.fail_horarios_500 = False
        self.enrolled: List[str] = []


def _make_handler(state: _MockState):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: Any) -> None:
            pass

        def _send(self, code: int, ctype: str, body: bytes, extra_headers: List[tuple[str, str]] | None = None) -> None:
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            for k, v in (extra_headers or []):
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            path = self.path.split("?", 1)[0]
            cookies = self.headers.get("Cookie", "")
            if path == "/Matricula/frmMatricula.aspx":
                if ".ASPXAUTH=MCPAUTH" not in cookies:
                    self._send(302, "text/html", b"", [
                        ("Location", "/Matricula/frmAutenticacion.aspx"),
                        ("Set-Cookie", "AlteonP=MCP1; path=/"),
                    ])
                    return
                html = (
                    '<!doctype html><html><body>'
                    '<span id="ctl00_lblNombre">ESTUDIANTE MCP</span>'
                    '<span id="ctl00_lblCarne">2026111111</span>'
                    '<div class="divCitas cCitasOk"><label id="lblCitaMatricula">Cita activa</label></div>'
                    '<table id="Table_ConvocatoriaEstudiante"><tbody>'
                    '<tr id="42_1"><td>Semestre</td><td>1</td><td>Ordinaria</td><td>01/01</td><td>12/31</td>'
                    '<td><button type="button" class="btn_Consultar" onclick="ObtieneCursos(42,1)">Ir</button></td></tr>'
                    '</tbody></table><table id="tBodyCursos"><tbody></tbody></table>'
                    '<script>'
                    'ValidaConvocatoria([{"ID_CONVOCATORIA":42,"ID_PERIODO":1,"MODALIDAD":"Semestre","PERIODO":"1","TIPOMATRICULA":"Ordinaria","DENTRO_FECHA":true,"ESTADO":true}]);'
                    'ObtieneHora("11/27/2026 08:00:00");'
                    'function ObtieneCursos(c,p){'
                    '  document.querySelector("#tBodyCursos tbody").innerHTML='
                    '  \'<tr id="CI1231" onclick="toggleH(\\\'CI1231\\\')"><td></td><td class="colCodigo">CI1231</td><td class="colMateria">INGLES II</td><td class="colCreditos">2</td><td><input id="grupo_CI1231" class="txtGrupoMatriculado" value=""/></td><td></td><td><img id="imgCI1231" class="imghide"/></td></tr>\'+'
                    '  \'<tr class="Rowaccordion" id="trHCI1231" style="display:none"><td colspan="7"><div id="divHCI1231"></div></td></tr>\';'
                    '}'
                    'function toggleH(cid){'
                    '  document.getElementById("trH"+cid).style.display="table-row";'
                    '  document.getElementById("divH"+cid).innerHTML='
                    '  \'<table><tbody id="bodyTableHorarios_\'+cid+\'"><tr><td>Cartago</td><td>2</td><td>K 09:30-11:20<br>D3-04<br></td><td>PROF DOM<br></td><td><span id="cupo\'+cid+\'2">5</span></td><td>No</td><td>Normal</td>\'+'
                    '  \'<td><button type="button" id="\'+cid+\'_2" class="cBtnMat_\'+cid+\' cImgDesMatriculado" onclick="doMat(\\\'\'+cid+\'\\\',2)">Mat</button></td></tr></tbody></table>\';'
                    '}'
                    'function doMat(cid,gid){'
                    '  fetch("/Matricula/dom_enroll",{method:"POST"}).then(()=>{'
                    '    document.getElementById("grupo_"+cid).value=String(gid);'
                    '    document.getElementById("img"+cid).className="ccmat";'
                    '  });'
                    '}'
                    '</script></body></html>'
                ).encode("utf-8")
                self._send(200, "text/html; charset=utf-8", html)
                return

            if path == "/Matricula/frmAutenticacion.aspx":
                html = (
                    '<form method="post" action="/Matricula/frmAutenticacion.aspx">'
                    '<input type="hidden" name="__VIEWSTATE" value="VS+mcp/token==" />'
                    '<input type="hidden" name="__VIEWSTATEGENERATOR" value="9999" />'
                    '<input type="hidden" name="__EVENTVALIDATION" value="EV+mcp/valid==" />'
                    '<input type="text" id="txtUsuario" name="txtUsuario" />'
                    '<input type="password" id="txtPassword" name="txtPassword" />'
                    '<input type="submit" id="btnEntrar" name="btnEntrar" value="Ingresar" /></form>'
                ).encode("utf-8")
                self._send(200, "text/html; charset=utf-8", html)
                return

            self._send(404, "text/plain", b"not found")

        def do_POST(self) -> None:
            path = self.path.split("?", 1)[0]
            clen = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(clen) if clen > 0 else b""

            if path == "/Matricula/frmAutenticacion.aspx":
                state.login_count += 1
                self._send(302, "text/html", b"", [
                    ("Location", "/Matricula/frmMatricula.aspx"),
                    ("Set-Cookie", "ASP.NET_SessionId=MCPSESS; path=/"),
                    ("Set-Cookie", ".ASPXAUTH=MCPAUTH; path=/"),
                ])
                return

            if path == "/Matricula/dom_enroll":
                state.dom_enroll_hits += 1
                self._send(200, "application/json", b'{"ok":true}')
                return

            payload = json.loads(raw.decode("utf-8")) if raw else {}

            if path == "/Matricula/frmMatricula.aspx/ObtieneCursos":
                self._send(200, "application/json; charset=utf-8", _wrap_d({
                    "EsBloqueado": False, "EstadoCita": True,
                    "Cursos": [
                        {"ID_MATERIA": "MA1102", "DESCRIPCION_MATERIA": "CALCULO", "CANT_CREDITOS": 4, "ID_GRUPO": 0, "IND_MOSTRAR": True},
                        {"ID_MATERIA": "CI1231", "DESCRIPCION_MATERIA": "INGLES II", "CANT_CREDITOS": 2, "ID_GRUPO": 0, "IND_MOSTRAR": True},
                    ],
                }))
                return

            if path == "/Matricula/frmMatricula.aspx/ConsultaCitaMatriculaEstudiante":
                self._send(200, "application/json; charset=utf-8", _wrap_d("True"))
                return

            if path == "/Matricula/frmMatricula.aspx/ObtieneCitaMatricula":
                self._send(200, "application/json; charset=utf-8", _wrap_d({"ID_CITA": 7, "POSICION": 12}))
                return

            if path == "/Matricula/frmMatricula.aspx/ConsultaMorosidadEstudiante":
                self._send(200, "application/json; charset=utf-8", _wrap_d("False"))
                return

            if path == "/Matricula/frmMatricula.aspx/ConsultaHorarioMatriculado":
                self._send(200, "application/json; charset=utf-8", _wrap_d({
                    "Periodo": "1-2026",
                    "Horario": [{"IdMateria": m, "NomMateria": m, "IdGrupo": 1} for m in state.enrolled],
                }))
                return

            if path == "/Matricula/frmMatricula.aspx/ConsultaHorarios":
                if state.fail_horarios_500:
                    self._send(500, "application/json", b'{"Message":"Simulated 500"}')
                    return
                state.horarios_poll_count += 1
                cid = str(payload.get("idMateria", "")).upper()
                seats = 5
                if cid == "MA1102" and state.release_seat_after_poll > 0:
                    seats = 1 if state.horarios_poll_count >= state.release_seat_after_poll else 0
                self._send(200, "application/json; charset=utf-8", _wrap_d({
                    "EstadoCita": True, "EsBloqueado": False,
                    "Horario": [{
                        "IdMateria": cid, "NomMateria": cid, "IdGrupo": 2 if cid == "CI1231" else 1,
                        "Sede": "Cartago", "EstadoGrupo": "Normal", "EstadoReservado": False,
                        "CanMatriculados": 20, "Capacidad": 25, "CupoDisponible": seats,
                        "TieneHorario": True, "ListaProfesores": ["PROF TEST"],
                        "Itinerario": [{"Dia": "L" if cid == "MA1102" else "K", "Inicio": "07:30", "Fin": "09:20", "Edificio": "D3", "Aula": "1"}],
                    }],
                }))
                return

            if path == "/Matricula/frmMatricula.aspx/AgregarMatricula":
                state.mutating_calls += 1
                cid = str(payload.get("idMateria", "")).upper()
                gid = int(payload.get("idGrupo", 0))
                if cid not in state.enrolled:
                    state.enrolled.append(cid)
                self._send(200, "application/json; charset=utf-8", _wrap_d({
                    "Estado": True, "NumGrupo": gid, "ErrorString": "", "Error": [],
                }))
                return

            if path == "/Matricula/frmInformeMatricula.aspx/GeneraInforme":
                self._send(200, "application/json; charset=utf-8", _wrap_d({
                    "idPeriodo": "1", "idAno": "2026",
                    "objInfoPersonal": {"Plan": "Computacion", "Sede": "Cartago", "Jornada": "Diurna"},
                    "objInformeG": {
                        "DetalleMaterias": [{"IdMateria": m, "NomMateria": m, "IdGrupo": 1, "Creditos": 3} for m in state.enrolled],
                        "DetallePago": [],
                    },
                }))
                return

            if path == "/Matricula/frmInformeMatricula.aspx/EnviaInforme":
                state.mutating_calls += 1
                state.informe_sent_count += 1
                self._send(200, "application/json; charset=utf-8", _wrap_d("demo@estudiantec.cr"))
                return

            self._send(404, "text/plain", b"not found")

    return Handler


class _ThreadedServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


class TestTecMatriculaMcpAndHook(unittest.TestCase):
    def _start_mock(self, state: _MockState):
        httpd = _ThreadedServer(("127.0.0.1", 0), _make_handler(state))
        t = threading.Thread(target=httpd.serve_forever, daemon=True)
        t.start()
        return httpd, f"http://127.0.0.1:{httpd.server_address[1]}/Matricula/"

    def test_1_mcp_server_registers_all_14_tools(self) -> None:
        mcp = srv.create_mcp_server()
        tools = list(mcp._tool_manager._tools.keys())
        expected = {
            "tec_matricula_status",
            "tec_matricula_vault_manage",
            "tec_matricula_login",
            "tec_matricula_check_cita_and_morosidad",
            "tec_matricula_get_groups",
            "tec_matricula_scenarios_manage",
            "tec_matricula_arm_live",
            "tec_matricula_enroll_course",
            "tec_matricula_execute_scenarios",
            "tec_matricula_send_informe",
            "tec_matricula_verify_enrolled",
            "tec_matricula_watch_and_snipe_start",
            "tec_matricula_watch_and_snipe_status",
            "tec_matricula_watch_and_snipe_stop",
        }
        self.assertEqual(set(tools), expected)

    def test_2_dpapi_vault_roundtrip_and_zero_plaintext(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            vpath = Path(td) / "vault.dpapi"
            res = srv.DpapiVault.save("agent@estudiantec.cr", "My$ecret+42", vpath)
            self.assertTrue(res["ok"])
            raw = vpath.read_bytes()
            self.assertNotIn(b"My$ecret+42", raw)
            ok, u, p, err = srv.DpapiVault.load(vpath)
            self.assertTrue(ok, err)
            self.assertEqual(u, "agent@estudiantec.cr")
            self.assertEqual(p, "My$ecret+42")
            srv.DpapiVault.delete(vpath)
            self.assertFalse(vpath.exists())

    def test_3_engine_vault_login_scenarios_informe_and_safety_lock(self) -> None:
        state = _MockState()
        httpd, url = self._start_mock(state)
        try:
            with tempfile.TemporaryDirectory() as td:
                vpath = Path(td) / "vault.dpapi"
                srv.DpapiVault.save("demo@estudiantec.cr", "Pass+123$", vpath)

                eng = srv.TecMatriculaEngine(base_url=url)
                lres = eng.login(use_vault=True, vault_path=vpath)
                self.assertTrue(lres["ok"])
                self.assertEqual(lres["studentName"], "ESTUDIANTE MCP")

                scenarios = [{
                    "name": "Plan A MCP",
                    "priority": 1,
                    "enabled": True,
                    "courses": [
                        {"idMateria": "MA1102", "required": True, "explicitGroupPriority": [1]},
                        {"idMateria": "CI1231", "required": True, "explicitGroupPriority": [2]},
                    ],
                }]

                # Safety lock: Dry-Run and Unarmed MUST cause 0 mutations!
                dres = eng.execute_scenarios(scenarios, dry_run=True, send_informe_on_success=True)
                self.assertTrue(dres["ok"])
                self.assertEqual(state.mutating_calls, 0)

                unarmed = eng.enroll_course("MA1102", 1, dry_run=False)
                self.assertTrue(unarmed.get("blocked"))
                self.assertEqual(state.mutating_calls, 0)

                # Wrong phrase rejected
                self.assertFalse(eng.arm_live(True, "incorrecta")["ok"])
                # Exact phrase unlocks -> executes scenarios + sends official EnviaInforme + auto re-locks!
                self.assertTrue(eng.arm_live(True, srv.CONFIRMATION_PHRASE)["ok"])
                live = eng.execute_scenarios(scenarios, dry_run=False, send_informe_on_success=True)
                self.assertTrue(live["ok"])
                self.assertEqual(live["enrolledCount"], 2)
                self.assertEqual(state.informe_sent_count, 1)
                self.assertFalse(eng.armed)  # Auto re-locked!

                ver = eng.verify_enrolled()
                self.assertTrue(ver["ok"])
                self.assertEqual(len(ver["informe"]["objInformeG"]["DetalleMaterias"]), 2)
                eng.close()
        finally:
            httpd.shutdown()

    def test_4_seat_sniper_catches_dropped_seat_and_auto_enrolls(self) -> None:
        state = _MockState()
        state.release_seat_after_poll = 3  # Seat is 0 on polls #1 and #2, opens on poll #3!
        httpd, url = self._start_mock(state)
        try:
            eng = srv.TecMatriculaEngine(base_url=url)
            self.assertTrue(eng.login(use_vault=False, email="demo@estudiantec.cr", password="x")["ok"])

            # Refused while unarmed
            refused = eng.start_watch_and_snipe([{"idMateria": "MA1102", "explicitGroupPriority": [1]}], interval_sec=0.08, auto_enroll=True)
            self.assertFalse(refused["ok"])

            # Arm and snipe!
            self.assertTrue(eng.arm_live(True, srv.CONFIRMATION_PHRASE)["ok"])
            started = eng.start_watch_and_snipe(
                [{"idMateria": "MA1102", "explicitGroupPriority": [1]}],
                interval_sec=0.08,
                max_duration_minutes=0.2,
                auto_enroll=True,
                send_informe_on_snipe=True,
            )
            self.assertTrue(started["ok"])

            for _ in range(40):
                st = eng.watch_and_snipe_status()
                if not st["active"]:
                    break
                time.sleep(0.08)

            st = eng.watch_and_snipe_status()
            self.assertGreaterEqual(st["polls"], 3)
            self.assertEqual(len(st["enrolled"]), 1)
            self.assertEqual(st["enrolled"][0]["idMateria"], "MA1102")
            self.assertEqual(state.informe_sent_count, 1)
            self.assertFalse(eng.armed)
            eng.close()
        finally:
            httpd.shutdown()

    def test_5_mcp_engine_playwright_failover_on_http_500(self) -> None:
        state = _MockState()
        state.fail_horarios_500 = True
        httpd, url = self._start_mock(state)
        try:
            eng = srv.TecMatriculaEngine(base_url=url)
            # Login via scraping so browser context is authenticated, then test failover scraping
            lres = eng.login(use_vault=False, email="demo@estudiantec.cr", password="x", id_convocatoria=42, id_periodo=1, transport="scraping", headless=True)
            self.assertTrue(lres["ok"])
            gres = eng.get_groups(["CI1231"], transport="auto", headless=True)
            self.assertEqual(gres["transport"], "scraping")
            self.assertEqual(len(gres["groupsByCourse"]["CI1231"]), 1)
            self.assertEqual(gres["groupsByCourse"]["CI1231"][0]["CupoDisponible"], 5)
            eng.close()
        finally:
            httpd.shutdown()

    def test_6_lifecycle_hook_pre_and_post_audit_redaction(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            audit_file = Path(td) / "audit.jsonl"
            env = dict(os.environ)
            env["CUADERNU_AUDIT_LOG_PATH"] = str(audit_file)

            def _run_hook(payload: Dict[str, Any]) -> Dict[str, Any]:
                proc = subprocess.run(
                    [sys.executable, str(TOOLS_DIR / "tec_matricula_hook.py")],
                    input=json.dumps(payload),
                    text=True,
                    capture_output=True,
                    env=env,
                    timeout=10,
                )
                self.assertEqual(proc.returncode, 0, proc.stderr)
                return json.loads(proc.stdout)

            # PreToolUse denies wrong phrase on arm_live
            r1 = _run_hook({
                "hookEventName": "PreToolUse",
                "toolCall": {"name": "tec_matricula_arm_live", "args": {"armed": True, "confirmation_phrase": "nope"}},
            })
            self.assertEqual(r1["decision"], "deny")

            # PreToolUse asks before live dry_run=False enrollment
            r2 = _run_hook({
                "hookEventName": "PreToolUse",
                "toolCall": {"name": "tec_matricula_enroll_course", "args": {"id_materia": "MA1102", "id_grupo": 1, "dry_run": False}},
            })
            self.assertEqual(r2["decision"], "ask")

            # PreToolUse allows read-only / dry_run calls
            r3 = _run_hook({
                "hookEventName": "PreToolUse",
                "toolCall": {"name": "tec_matricula_get_groups", "args": {"course_ids": ["MA1102"]}},
            })
            self.assertEqual(r3["decision"], "allow")

            # PostToolUse writes sanitized audit log with password REDACTED
            r4 = _run_hook({
                "hookEventName": "PostToolUse",
                "toolCall": {"name": "tec_matricula_login", "args": {"email": "demo@estudiantec.cr", "password": "SUPER_SECRET_PASSWORD"}},
            })
            self.assertEqual(r4, {})
            log_text = audit_file.read_text(encoding="utf-8")
            self.assertNotIn("SUPER_SECRET_PASSWORD", log_text)
            self.assertIn("<REDACTED>", log_text)

    def test_7_mcp_stdio_jsonrpc_handshake_and_tools_list(self) -> None:
        proc = subprocess.Popen(
            [sys.executable, "-u", str(TOOLS_DIR / "tec_matricula_mcp_server.py")],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
        try:
            assert proc.stdin is not None and proc.stdout is not None
            init_req = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "clientInfo": {"name": "cuadernu-test", "version": "1.0"},
                },
            }
            proc.stdin.write(json.dumps(init_req) + "\n")
            proc.stdin.flush()
            init_resp = json.loads(proc.stdout.readline())
            self.assertEqual(init_resp.get("id"), 1)
            self.assertIn("result", init_resp)

            proc.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
            proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}) + "\n")
            proc.stdin.flush()
            list_resp = json.loads(proc.stdout.readline())
            self.assertEqual(list_resp.get("id"), 2)
            names = [t["name"] for t in list_resp["result"]["tools"]]
            self.assertEqual(len(names), 14)
            self.assertIn("tec_matricula_send_informe", names)
            self.assertIn("tec_matricula_watch_and_snipe_start", names)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except Exception:
                proc.kill()


if __name__ == "__main__":
    unittest.main(verbosity=2)
