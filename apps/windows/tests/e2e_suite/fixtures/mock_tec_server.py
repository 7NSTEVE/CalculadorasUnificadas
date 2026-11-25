"""Loopback HTTP/HTTPS TEC Digital mock server (Mode A) per spec_academic_18_19.md §1."""
from __future__ import annotations

import http.server
import json
import re
import shutil
import socketserver
import ssl
import subprocess
import tempfile
import threading
import time
import urllib.parse
import zipfile
from pathlib import Path
from typing import Any

from .synthetic_data import make_minimal_docx, make_minimal_pdf, make_minimal_xlsx


_SYNTH_USER_EMAIL = "student@example.invalid"
_SYNTH_USER_PASS = "SYNTH-TEST-PASS-0001"
_SYNTH_SESS_ID = "synth_sess_0001"
_SYNTH_LOGIN_COOKIE = "synth_login_0001"
_SYNTH_LB_COOKIE = "synth_lb_0001"


def _make_zip_bytes(entries: dict[str, bytes]) -> bytes:
    import io

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for name, data in entries.items():
            zf.writestr(name, data)
    return buf.getvalue()


def _parse_cookies(cookie_header: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for part in cookie_header.split(";"):
        if "=" in part:
            k, v = part.split("=", 1)
            out[k.strip()] = v.strip()
    return out


class _MockServerState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.request_log: list[dict[str, Any]] = []
        self.mutation_received_count: int = 0
        self.session_invalidated: bool = False
        self.drop_attempts: dict[str, int] = {}
        self.seat_counts: dict[str, int] = {"EL2113:Gr 50": 12}

        pdf_bytes = make_minimal_pdf(
            ["Ecuacion de Maxwell: div(E) = rho / eps0", "Guia de Estudio Sintetica"],
            title="Guia_Estudio_Synth",
            use_flate=True,
        )
        docx_bytes = make_minimal_docx(["Transformada de Laplace L{f(t)} = F(s)"])
        xlsx_bytes = make_minimal_xlsx([["Rubro", "Valor"], ["Nota_Final", 95]])
        drop_pdf_bytes = make_minimal_pdf(
            ["Archivo Resumido 900001", "Contenido completo despues de Range 206"],
            title="Resumable_900001",
            use_flate=False,
        ) * 3

        self.files: dict[str, dict[str, Any]] = {
            "214778985": {
                "object_id": 214778985,
                "name": "Guia_Estudio_Synth.pdf",
                "mime_type": "application/pdf",
                "revision_id": 1001,
                "last_modified": "2026-11-17T10:00:00Z",
                "bytes": pdf_bytes,
            },
            "214778986": {
                "object_id": 214778986,
                "name": "Resumen_Lab.docx",
                "mime_type": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                "revision_id": 1002,
                "last_modified": "2026-11-17T10:05:00Z",
                "bytes": docx_bytes,
            },
            "214778987": {
                "object_id": 214778987,
                "name": "Datos_Exp.xlsx",
                "mime_type": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                "revision_id": 1003,
                "last_modified": "2026-11-17T10:10:00Z",
                "bytes": xlsx_bytes,
            },
            "900001": {
                "object_id": 900001,
                "name": "Drop_Resume_900001.pdf",
                "mime_type": "application/pdf",
                "revision_id": 9001,
                "last_modified": "2026-11-17T11:00:00Z",
                "bytes": drop_pdf_bytes,
            },
            "900002": {
                "object_id": 900002,
                "name": "Throttled_900002.docx",
                "mime_type": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                "revision_id": 9002,
                "last_modified": "2026-11-17T11:01:00Z",
                "bytes": docx_bytes,
            },
            "900003": {
                "object_id": 900003,
                "name": "Corrupt_900003.pdf",
                "mime_type": "application/pdf",
                "revision_id": 9003,
                "last_modified": "2026-11-17T11:02:00Z",
                "bytes": b"<!DOCTYPE html><html><body><form id='loginForm'>CORRUPT_BYTES</form></body></html>",
            },
        }


class _ThreadedHttpServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, server_address: tuple[str, int], handler_cls: type, state: _MockServerState) -> None:
        self.mock_state = state
        super().__init__(server_address, handler_cls)


class _MockTecHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
        return  # keep test stdout clean

    @property
    def state(self) -> _MockServerState:
        return self.server.mock_state  # type: ignore[attr-defined]

    def _read_body(self) -> bytes:
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            return b""
        return self.rfile.read(length)

    def _record_request(self, method: str, parsed: urllib.parse.ParseResult, body: bytes) -> None:
        with self.state.lock:
            self.state.request_log.append({
                "method": method,
                "path": parsed.path,
                "query": parsed.query,
                "body": body.decode("utf-8", errors="replace"),
            })

    def _has_valid_session(self) -> bool:
        if self.state.session_invalidated:
            return False
        cookies = _parse_cookies(self.headers.get("Cookie", ""))
        return (
            cookies.get("ad_session_id") == _SYNTH_SESS_ID
            and cookies.get("ad_user_login") == _SYNTH_LOGIN_COOKIE
        )

    def _send_json(self, code: int, obj: Any, extra_headers: list[tuple[str, str]] | None = None) -> None:
        raw = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        if extra_headers:
            for k, v in extra_headers:
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(raw)

    def _send_html(self, code: int, html_text: str) -> None:
        raw = html_text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _send_bytes(
        self,
        code: int,
        data: bytes,
        content_type: str = "application/octet-stream",
        extra_headers: list[tuple[str, str]] | None = None,
    ) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        if extra_headers:
            for k, v in extra_headers:
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(data)

    def _serve_file_payload(self, obj_id: str, query: dict[str, list[str]]) -> None:
        if obj_id == "900004":
            self._send_json(403, {"error": "forbidden_file"})
            return
        if obj_id == "900005":
            self._send_json(404, {"error": "ORPHAN_404"})
            return
        if obj_id == "900003":
            if query.get("mode", [""])[0] == "badmagic":
                self._send_bytes(200, b"BADMAGIC_NOT_PDF_PAYLOAD", "application/pdf")
            else:
                self._send_bytes(
                    200,
                    self.state.files["900003"]["bytes"],
                    "text/html; charset=utf-8",
                )
            return

        meta = self.state.files.get(obj_id)
        if meta is None:
            self._send_json(404, {"error": "file_not_found"})
            return

        data: bytes = meta["bytes"]
        total = len(data)
        range_hdr = self.headers.get("Range", "")

        # Fault 900001: drop connection mid-transfer on first non-Range request
        if obj_id == "900001" and not range_hdr:
            with self.state.lock:
                seen = self.state.drop_attempts.get("900001", 0)
                self.state.drop_attempts["900001"] = seen + 1
            if seen == 0:
                half = max(1, total // 2)
                self.send_response(200)
                self.send_header("Content-Type", meta["mime_type"])
                self.send_header("Accept-Ranges", "bytes")
                self.send_header("Content-Length", str(total))
                self.end_headers()
                self.wfile.write(data[:half])
                self.wfile.flush()
                self.close_connection = True
                return

        if range_hdr:
            m = re.match(r"bytes=(\d+)-(\d*)", range_hdr.strip())
            if not m:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{total}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            start = int(m.group(1))
            end = int(m.group(2)) if m.group(2) else total - 1
            if start >= total or end >= total or start > end:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{total}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            slice_bytes = data[start : end + 1]
            self._send_bytes(
                206,
                slice_bytes,
                meta["mime_type"],
                extra_headers=[
                    ("Accept-Ranges", "bytes"),
                    ("Content-Range", f"bytes {start}-{end}/{total}"),
                ],
            )
            return

        # Fault 900002: simulated throttle (bounded delay so test suite stays fast)
        if obj_id == "900002":
            self.send_response(200)
            self.send_header("Content-Type", meta["mime_type"])
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Length", str(total))
            self.end_headers()
            chunk_sz = max(1, total // 4)
            for off in range(0, total, chunk_sz):
                time.sleep(0.05)
                self.wfile.write(data[off : off + chunk_sz])
                self.wfile.flush()
            return

        self._send_bytes(
            200,
            data,
            meta["mime_type"],
            extra_headers=[("Accept-Ranges", "bytes")],
        )

    def do_GET(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        query = urllib.parse.parse_qs(parsed.query)
        self._record_request("GET", parsed, b"")
        path = parsed.path

        # Mutating GET paths that must never be sent by read-only clients
        if (
            "cal-item-new" in path
            or "member-confirm-2" in path
            or query.get("action", [""])[0].lower() == "delete"
        ):
            with self.state.lock:
                self.state.mutation_received_count += 1
            self._send_json(403, {"error": "mutation-blocked"})
            return

        if path in ("/", "/dotlrn", "/dotlrn/", "/dotlrn/index"):
            if not self._has_valid_session():
                if query.get("no_redirect", [""])[0] == "1":
                    self._send_html(
                        200,
                        '<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">'
                        '<title>TEC Digital - Login</title></head><body>'
                        '<form id="loginForm"></form></body></html>',
                    )
                else:
                    self.send_response(302)
                    self.send_header("Location", "/register/?return_url=/dotlrn/index")
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                return
            self._send_html(
                200,
                '<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">'
                "<title>TEC Digital Portal</title></head><body>"
                "<h1>TEC Digital Portal - Estudiante Sintetico</h1>"
                '<a href="/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50/">EL2113</a>'
                '<span data-root-folder-id="153108817">root_folder_id = 153108817</span>'
                "</body></html>",
            )
            return

        if path in ("/register", "/register/"):
            self._send_html(
                200,
                '<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">'
                "<title>TEC Digital - Registro</title></head><body>"
                '<form id="loginForm" action="/api/login/new-form/">'
                '<script data-scan="/api/login/scanning/" data-reg="/api/login/register-login-method/"></script>'
                "</form></body></html>",
            )
            return

        if path == "/api/login/register-login-method/":
            self._send_json(200, {"methods": ["email_password"]})
            return

        if path.startswith("/file-storage/xmlhttp/"):
            self._send_json(404, {"error": "xmlhttp_unavailable"})
            return

        if path.startswith("/dotlrn/file-storage/api/folder/"):
            fid = path.rstrip("/").split("/")[-1]
            if fid == "153108817":
                self._send_json(200, {
                    "folder_id": 153108817,
                    "elements": [
                        {"fs_type": "folder", "object_id": 153108818, "name": "Laboratorios"},
                        {
                            "fs_type": "file",
                            "object_id": 214778985,
                            "name": "Guia_Estudio_Synth.pdf",
                            "mime_type": "application/pdf",
                        },
                        {
                            "fs_type": "url",
                            "object_id": 214778990,
                            "name": "Enlace Externo",
                            "url": "https://www.tec.ac.cr/synthetic-doc",
                        },
                    ],
                })
                return
            if fid == "153108818":
                self._send_json(200, {
                    "folder_id": 153108818,
                    "elements": [
                        {
                            "fs_type": "file",
                            "object_id": 214778986,
                            "name": "Resumen_Lab.docx",
                            "mime_type": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                        },
                        {
                            "fs_type": "file",
                            "object_id": 214778987,
                            "name": "Datos_Exp.xlsx",
                            "mime_type": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                        },
                    ],
                })
                return
            if fid.isdigit() and 99999001 <= int(fid) <= 99999015:
                nxt = int(fid) + 1 if int(fid) < 99999015 else 99999001
                self._send_json(200, {
                    "folder_id": int(fid),
                    "elements": [{"fs_type": "folder", "object_id": nxt, "name": f"Nested_{nxt}"}],
                })
                return
            self._send_json(404, {"error": "folder_not_found"})
            return

        if path.startswith("/dotlrn/file-storage/api/file/"):
            oid = path.rstrip("/").split("/")[-1]
            meta = self.state.files.get(oid)
            if not meta:
                self._send_json(404, {"error": "file_not_found"})
                return
            self._send_json(200, {
                "object_id": meta["object_id"],
                "name": meta["name"],
                "mime_type": meta["mime_type"],
                "content_size": len(meta["bytes"]),
                "revision_id": meta["revision_id"],
                "last_modified": meta["last_modified"],
            })
            return

        if path.startswith("/dotlrn/file-storage/view/"):
            oid = path.rstrip("/").split("/")[-1]
            self._serve_file_payload(oid, query)
            return

        if path.startswith("/dotlrn/file-storage/download/"):
            oid = query.get("file_id", [""])[0]
            self._serve_file_payload(oid, query)
            return

        if path.startswith("/dotlrn/file-storage/download-zip"):
            zip_raw = _make_zip_bytes({
                "Guia_Estudio_Synth.pdf": self.state.files["214778985"]["bytes"],
                "Resumen_Lab.docx": self.state.files["214778986"]["bytes"],
            })
            self._send_bytes(200, zip_raw, "application/zip")
            return

        # TDA / TDS Academic endpoints
        if path == "/theme-td/ajax/get_date_server":
            raw = b'"2026-11-17 17:30:00"'
            self._send_bytes(200, raw, "application/json; charset=utf-8")
            return

        if path == "/tds-curriculum-exp/ajax/carga_sedes_json":
            self._send_json(200, [
                {"id": "CA", "nombre": "Cartago"},
                {"id": "SJ", "nombre": "San Jose"},
            ])
            return

        if path == "/tds-curriculum-exp/ajax/carga_carreras_json":
            self._send_json(200, [
                {"id": "EL", "nombre": "Ingenieria Electronica", "id_sede": "CA"},
                {"id": "CE", "nombre": "Ingenieria en Computadores", "id_sede": "CA"},
            ])
            return

        if path == "/tds-curriculum-exp/ajax/carga_planes_json":
            self._send_json(200, [
                {"id_plan": 101, "nombre": "Plan 4102", "id_sede": "CA"},
            ])
            return

        if path == "/tds-curriculum-exp/ajax/color_codes_json":
            self._send_json(200, {
                "Aprobado": "#22c55e",
                "Cursando": "#3b82f6",
                "Pendiente": "#94a3b8",
            })
            return

        if path == "/tds-curriculum-exp/ajax/json_draw_angular":
            self._send_json(200, {
                "id_plan": 101,
                "courses": [
                    {"code": "EL2113", "name": "Sistemas Digitales", "credits": 4},
                    {"code": "FI1102", "name": "Fisica General II", "credits": 4},
                ],
            })
            return

        if path == "/tds-curriculum-exp/ajax/json_student_profile_angular":
            self._send_json(200, {
                "carne": "2026000001",
                "nombre": "Estudiante Sintetico",
                "sede": "CA",
                "id_plan": 101,
            })
            return

        if path == "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante":
            self._send_json(200, {
                "carneOficial": "2026000001",
                "carrera": "Ingenieria Electronica",
                "estado": "Activo",
            })
            return

        if path == "/tda-expediente-estudiantil/ajax/i18n":
            self._send_json(200, {"locale": "es", "grade_label": "Nota"})
            return

        if path == "/tda-expediente-estudiantil/ajax/detalle_notas":
            self._send_json(200, [
                {"code": "EL2113", "name": "Sistemas Digitales", "nota": 92, "estado": "Aprobado"},
                {"code": "MA1103", "name": "Calculo", "nota": 88, "estado": "Aprobado"},
            ])
            return

        if path in (
            "/tda-expediente-estudiantil/ajax/detalle_promedio_ponderado",
            "/tda-expediente-estudiantil/ajax/generar_tabla_promedio_ponderado",
        ):
            self._send_json(200, {"promedio_ponderado": 90.0, "creditos_aprobados": 8})
            return

        if path == "/tda-notifications/ajax/has_unread_notifications":
            self._send_json(200, {"unread": 2})
            return

        if path == "/tda-notifications/ajax/get_user_notifications":
            self._send_json(200, [
                {
                    "id": 501,
                    "title": "Entrega Laboratorio 1",
                    "course": "EL2113",
                    "deadline_hours": 48,
                    "read": False,
                },
                {
                    "id": 502,
                    "title": "Material Disponible",
                    "course": "FI1102",
                    "deadline_hours": 72,
                    "read": False,
                },
            ])
            return

        if path == "/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50/evaluation/tda-ce-estudiante/ajax/Internationalization":
            self._send_json(200, {"submit": "Evaluar", "title": "Evaluacion Docente"})
            return

        if path == "/tda-performance-evaluation/api/evaluations/":
            self._send_json(200, {"evaluations": [{"id": 1, "course": "EL2113", "title": "Examen 1"}]})
            return

        if path == "/tda-performance-evaluation/api/forms/":
            self._send_json(200, {"forms": [{"id": 10, "name": "Encuesta Curso"}]})
            return

        if path == "/dotlrn/calendar/view":
            links = "\n".join(
                f'<a href="/dotlrn/calendar/view?date=2026-09-{d:02d}&view=list&period_days=7">Dia {d}</a>'
                for d in range(1, 31)
            )
            self._send_html(
                200,
                '<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">'
                "<title>TEC Digital - Calendario</title></head><body>"
                f"{links}"
                '<a href="/dotlrn/calendar/cal-item-new?date=2026-09-01">Nuevo</a>'
                '<a href="https://zoom.example.invalid/j/123456?pwd=SYNTH_ZOOM_PWD">Clase</a>'
                "</body></html>",
            )
            return

        if path == "/api/horarios/seats":
            with self.state.lock:
                snapshot = dict(self.state.seat_counts)
            self._send_json(200, snapshot)
            return

        self._send_json(404, {"error": "unknown_mock_endpoint", "path": path})

    def do_POST(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        body_bytes = self._read_body()
        self._record_request("POST", parsed, body_bytes)
        path = parsed.path

        if path == "/api/login/new-form/":
            try:
                payload = json.loads(body_bytes.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                self._send_json(400, {"status": "bad_request"})
                return
            submitted_email = payload.get("email", "")
            submitted_pwd = payload.get("password", "")
            if submitted_email == _SYNTH_USER_EMAIL and submitted_pwd == _SYNTH_USER_PASS:
                self._send_json(
                    200,
                    {"status": "ok", "returnUrl": "/dotlrn/"},
                    extra_headers=[
                        (
                            "Set-Cookie",
                            f"ad_session_id={_SYNTH_SESS_ID}; Path=/; HttpOnly; SameSite=Lax; Max-Age=31536000",
                        ),
                        (
                            "Set-Cookie",
                            f"ad_user_login={_SYNTH_LOGIN_COOKIE}; Path=/; HttpOnly; SameSite=Lax; Max-Age=31536000",
                        ),
                        (
                            "Set-Cookie",
                            f"AlteonP={_SYNTH_LB_COOKIE}; Path=/; SameSite=Lax",
                        ),
                    ],
                )
            elif submitted_email == _SYNTH_USER_EMAIL:
                self._send_json(200, {"status": "bad_password", "returnUrl": "/register"})
            else:
                self._send_json(200, {"status": "no_account", "returnUrl": "/register"})
            return

        if path == "/api/login/scanning/":
            self._send_json(200, {"status": "ok", "scanning": False})
            return

        if path == "/dotlrn/classes/E/EL2113/S-2-2026.SC.EL2113.50/ajax/load_members":
            self._send_json(200, {
                "data": [
                    {"carne": "2026000001", "name": "Estudiante Sintetico", "role": "student"}
                ]
            })
            return

        if path == "/tda-prematricula/ajax/prematricula_est":
            try:
                payload = json.loads(body_bytes.decode("utf-8")) if body_bytes else {}
            except (UnicodeDecodeError, json.JSONDecodeError):
                payload = {}
            allowed_keys = {"tipo", "id_plan", "carne"}
            tipo = payload.get("tipo")
            if tipo not in (1, 2) or not set(payload.keys()).issubset(allowed_keys):
                with self.state.lock:
                    self.state.mutation_received_count += 1
                self._send_json(403, {"error": "prematricula-operation-not-proven-read-only"})
                return
            self._send_json(200, {"status": "ok", "tipo": tipo, "courses": ["EL2113", "FI1102"]})
            return

        if path == "/graphql":
            try:
                payload = json.loads(body_bytes.decode("utf-8")) if body_bytes else {}
            except (UnicodeDecodeError, json.JSONDecodeError):
                self._send_json(400, {"errors": [{"message": "invalid_json"}]})
                return
            raw_query = str(payload.get("query", ""))
            q_clean = re.sub(r"#[^\r\n]*", " ", raw_query).lstrip()
            if re.match(r"^(?:mutation|subscription)\b", q_clean, re.IGNORECASE) or re.search(
                r"\b(?:mutation|subscription)\b(?:\s+[A-Za-z_][A-Za-z0-9_]*)?(?:\s*\([^)]*\))?\s*\{",
                q_clean,
                re.IGNORECASE,
            ):
                with self.state.lock:
                    self.state.mutation_received_count += 1
                self._send_json(403, {"errors": [{"message": "graphql-not-proven-query"}]})
                return
            self._send_json(200, {
                "data": {
                    "courses": [
                        {"code": "EL2113", "name": "Sistemas Digitales"},
                        {"code": "FI1102", "name": "Fisica General II"},
                    ]
                }
            })
            return

        # Any other POST (e.g. delete_all_user_notifications, update_notification_state) is a mutation
        with self.state.lock:
            self.state.mutation_received_count += 1
        self._send_json(403, {"error": "post-not-proven-read-only", "path": path})

    def _handle_blocked_method(self, method: str) -> None:
        parsed = urllib.parse.urlparse(self.path)
        body_bytes = self._read_body()
        self._record_request(method, parsed, body_bytes)
        with self.state.lock:
            self.state.mutation_received_count += 1
        self._send_json(
            403,
            {
                "error": "method-not-proven-read-only",
                "method": method,
                "path": parsed.path,
            },
        )

    def do_PUT(self) -> None:  # noqa: N802
        self._handle_blocked_method("PUT")

    def do_DELETE(self) -> None:  # noqa: N802
        self._handle_blocked_method("DELETE")

    def do_PATCH(self) -> None:  # noqa: N802
        self._handle_blocked_method("PATCH")


class MockTecServer:
    """Thread-safe loopback HTTP/HTTPS TEC Digital mock server context manager."""

    def __init__(self, use_tls: bool = False) -> None:
        self.use_tls = use_tls
        self.state = _MockServerState()
        self._server: _ThreadedHttpServer | None = None
        self._thread: threading.Thread | None = None
        self._tls_tmpdir: Path | None = None

    @property
    def port(self) -> int:
        if self._server is None:
            raise RuntimeError("MockTecServer is not running")
        return int(self._server.server_address[1])

    @property
    def base_url(self) -> str:
        scheme = "https" if self.use_tls else "http"
        return f"{scheme}://127.0.0.1:{self.port}"

    @property
    def graphql_url(self) -> str:
        return f"{self.base_url}/graphql"

    @property
    def request_log(self) -> list[dict[str, Any]]:
        with self.state.lock:
            return list(self.state.request_log)

    @property
    def mutation_received_count(self) -> int:
        with self.state.lock:
            return self.state.mutation_received_count

    def set_session_invalidated(self, invalidated: bool) -> None:
        self.state.session_invalidated = invalidated

    def set_seat_count(self, course_group_key: str, seats: int) -> None:
        with self.state.lock:
            self.state.seat_counts[course_group_key] = int(seats)

    def _wrap_tls(self, srv: _ThreadedHttpServer) -> None:
        openssl = shutil.which("openssl")
        if not openssl:
            raise RuntimeError("openssl CLI not available to generate ephemeral self-signed TLS cert")
        tmpdir = Path(tempfile.mkdtemp(prefix="e2e_mock_tls_"))
        self._tls_tmpdir = tmpdir
        cert = tmpdir / "cert.pem"
        key = tmpdir / "key.pem"
        subprocess.run(
            [
                openssl,
                "req",
                "-x509",
                "-newkey",
                "rsa:2048",
                "-keyout",
                str(key),
                "-out",
                str(cert),
                "-days",
                "1",
                "-nodes",
                "-subj",
                "/CN=127.0.0.1",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
            timeout=15,
        )
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=str(cert), keyfile=str(key))
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)

    def start(self) -> "MockTecServer":
        srv = _ThreadedHttpServer(("127.0.0.1", 0), _MockTecHandler, self.state)
        if self.use_tls:
            self._wrap_tls(srv)
        self._server = srv
        self._thread = threading.Thread(target=srv.serve_forever, daemon=True)
        self._thread.start()
        return self

    def stop(self) -> None:
        if self._server is not None:
            self._server.shutdown()
            self._server.server_close()
            self._server = None
        if self._thread is not None:
            self._thread.join(timeout=3)
            self._thread = None
        if self._tls_tmpdir is not None:
            shutil.rmtree(self._tls_tmpdir, ignore_errors=True)
            self._tls_tmpdir = None

    def __enter__(self) -> "MockTecServer":
        return self.start()

    def __exit__(self, exc_type: Any, exc: Any, tb: Any) -> None:
        self.stop()
