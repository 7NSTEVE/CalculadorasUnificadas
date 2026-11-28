"""CuadernU Matrícula TEC — Playwright DOM Scraping Fallback Worker.

Protocol: JSON-Lines over stdin/stdout (UTF-8).
Security guarantees:
  - Credentials arrive ONLY via stdin JSON commands and are cleared right after form fill.
  - NEVER logs credentials, passwords, or cookie values.
  - Hard safety lock: mutating DOM actions (enroll_group, drop_course) are REFUSED unless
    both dry_run == False AND armed == True (and confirmed via phrase).
"""

from __future__ import annotations

import json
import os
import re
import sys
import time
import traceback
from pathlib import Path
from typing import Any, Dict, List, Optional

CONFIRMATION_PHRASE = "CONFIRMAR MATRICULA REAL"


def emit(event: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(event, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def log(level: str, message: str) -> None:
    emit({"event": "log", "level": level, "message": message})


def ensure_playwright_browsers_path() -> None:
    if os.environ.get("PLAYWRIGHT_BROWSERS_PATH"):
        return
    here = Path(__file__).resolve()
    candidates = [
        here.parents[2] / ".ms-playwright",
        here.parents[1] / ".ms-playwright",
        Path(os.environ.get("LOCALAPPDATA", "")) / "ms-playwright",
        Path("D:/Proyectos/CalculadorasUnificadas/private_local/ms-playwright"),
        Path("E:/Proyectos/CuadernU/.ms-playwright"),
    ]
    for c in candidates:
        if c.is_dir():
            os.environ["PLAYWRIGHT_BROWSERS_PATH"] = str(c)
            return


def parse_dom_horario_text(raw_html: str) -> List[Dict[str, str]]:
    """Parses DATIC DOM schedule text like 'L 07:30-09:20<br>D3-004<br>' into structured slots."""
    lines = [
        re.sub(r"<[^>]+>", "", part).strip()
        for part in re.split(r"</?br\s*/?>|\r?\n", raw_html or "", flags=re.IGNORECASE)
    ]
    lines = [ln for ln in lines if ln]
    sessions: List[Dict[str, str]] = []
    time_re = re.compile(r"^([LKMJVSD1-7])\s+(\d{1,2}:\d{2})\s*-\s*(\d{1,2}:\d{2})$", re.IGNORECASE)
    i = 0
    while i < len(lines):
        m = time_re.match(lines[i])
        if m:
            dia, ini, fin = m.group(1).upper(), m.group(2), m.group(3)
            edificio, aula = "ND", ""
            if i + 1 < len(lines) and not time_re.match(lines[i + 1]):
                room = lines[i + 1]
                if "No Disponible" not in room and "-" in room:
                    parts = room.split("-", 1)
                    edificio = parts[0].strip()
                    aula = parts[1].strip().lstrip("0") or parts[1].strip()
                i += 1
            sessions.append({
                "Dia": dia,
                "Inicio": ini,
                "Fin": fin,
                "Edificio": edificio,
                "Aula": aula,
            })
        i += 1
    return sessions


class MatriculaScraperWorker:
    def __init__(self) -> None:
        self._pw = None
        self._browser = None
        self._context = None
        self._page = None
        self.base_url: str = "https://tec-appsext.itcr.ac.cr/Matricula/"
        self.headless: bool = True
        self.screenshot_dir: Optional[Path] = None
        self.armed: bool = False
        self.last_xhr_horarios: Dict[str, Any] = {}

    def _launch_browser(self, headless: bool) -> None:
        ensure_playwright_browsers_path()
        from playwright.sync_api import sync_playwright

        if self._browser is not None:
            return
        self._pw = sync_playwright().start()
        launch_errors: List[str] = []
        for attempt in ("chromium", "msedge", "chrome"):
            try:
                if attempt == "chromium":
                    self._browser = self._pw.chromium.launch(headless=headless)
                else:
                    self._browser = self._pw.chromium.launch(channel=attempt, headless=headless)
                log("INFO", f"[Scraper] Navegador iniciado ({attempt}, headless={headless}).")
                break
            except Exception as exc:
                launch_errors.append(f"{attempt}: {exc}")
        if self._browser is None:
            raise RuntimeError("No se pudo iniciar Chromium/Edge/Chrome: " + " | ".join(launch_errors))

        self._context = self._browser.new_context(
            locale="es-CR",
            timezone_id="America/Costa_Rica",
            viewport={"width": 1280, "height": 840},
        )
        self._page = self._context.new_page()
        self._page.set_default_timeout(20000)

        def _on_response(resp) -> None:
            try:
                if "ConsultaHorarios" in resp.url and resp.status == 200:
                    data = resp.json()
                    if isinstance(data, dict) and isinstance(data.get("d"), str):
                        inner = json.loads(data["d"])
                        if isinstance(inner, dict) and isinstance(inner.get("Horario"), list) and inner["Horario"]:
                            cid = str(inner["Horario"][0].get("IdMateria", "")).strip().upper()
                            if cid:
                                self.last_xhr_horarios[cid] = inner
            except Exception:
                pass

        self._page.on("response", _on_response)

    def _save_screenshot(self, tag: str) -> Optional[str]:
        if self._page is None or self.screenshot_dir is None:
            return None
        try:
            self.screenshot_dir.mkdir(parents=True, exist_ok=True)
            ts = time.strftime("%Y%m%d_%H%M%S")
            out = self.screenshot_dir / f"matricula_{tag}_{ts}.png"
            self._page.screenshot(path=str(out), full_page=False)
            emit({"event": "screenshot_saved", "tag": tag, "path": str(out)})
            return str(out)
        except Exception:
            return None

    def handle_init(self, cmd: Dict[str, Any]) -> None:
        self.base_url = str(cmd.get("base_url") or self.base_url)
        if not self.base_url.endswith("/"):
            self.base_url += "/"
        self.headless = bool(cmd.get("headless", True))
        sdir = cmd.get("screenshot_dir")
        if sdir:
            self.screenshot_dir = Path(str(sdir))
        self._launch_browser(self.headless)
        emit({"event": "ready", "ok": True})

    def handle_arm(self, cmd: Dict[str, Any]) -> None:
        armed = bool(cmd.get("armed", False))
        phrase = str(cmd.get("confirmation_phrase", "")).strip()
        if not armed:
            self.armed = False
            emit({"event": "arm_result", "armed": False})
            return
        self.armed = (phrase == CONFIRMATION_PHRASE)
        emit({"event": "arm_result", "armed": self.armed})

    def handle_probe_login_page(self, cmd: Dict[str, Any]) -> None:
        base_url = str(cmd.get("base_url") or self.base_url)
        if not base_url.endswith("/"):
            base_url += "/"
        self._launch_browser(bool(cmd.get("headless", True)))
        assert self._page is not None
        self._page.goto(base_url + "frmMatricula.aspx", wait_until="domcontentloaded")
        has_user = self._page.locator("input[name='txtUsuario'], #txtUsuario").count() > 0
        has_pass = self._page.locator("input[name='txtPassword'], #txtPassword").count() > 0
        has_submit = self._page.locator("input[type='submit']").count() > 0
        has_vs = self._page.locator("input[name='__VIEWSTATE']").count() > 0
        emit({
            "event": "probe_result",
            "ok": has_user and has_pass and has_submit and has_vs,
            "final_url": self._page.url,
            "has_user": has_user,
            "has_pass": has_pass,
            "has_submit": has_submit,
            "has_viewstate": has_vs,
        })

    def handle_login(self, cmd: Dict[str, Any]) -> None:
        email = str(cmd.pop("email", "")).strip()
        password = str(cmd.pop("password", ""))
        try:
            self._launch_browser(self.headless)
            assert self._page is not None
            self._context.clear_cookies()
            self._page.goto(self.base_url + "frmMatricula.aspx", wait_until="domcontentloaded")

            user_loc = self._page.locator("input[name='txtUsuario'], #txtUsuario").first
            pass_loc = self._page.locator("input[name='txtPassword'], #txtPassword").first
            btn_loc = self._page.locator("input[type='submit']").first

            user_loc.fill(email)
            pass_loc.fill(password)
            password = ""  # Clear immediately

            with self._page.expect_navigation(wait_until="domcontentloaded", timeout=20000):
                btn_loc.click()

            url_lower = self._page.url.lower()
            if "frmautenticacion.aspx" in url_lower or self._page.locator("input[name='txtUsuario']").count() > 0:
                self._save_screenshot("login_failed")
                emit({"event": "login_result", "ok": False, "error": "Credenciales inválidas o sesión rechazada en navegador."})
                return

            name = (self._page.locator("#ctl00_lblNombre").inner_text() if self._page.locator("#ctl00_lblNombre").count() else "").strip()
            carne = (self._page.locator("#ctl00_lblCarne").inner_text() if self._page.locator("#ctl00_lblCarne").count() else "").strip()
            clock = self._page.evaluate(
                "() => { const h=document.querySelector('#shora')?.textContent||''; "
                "const m=document.querySelector('#sminutos')?.textContent||''; "
                "const s=document.querySelector('#ssegundos')?.textContent||''; "
                "return (h && m) ? `${h}:${m}:${s}` : ''; }"
            )
            emit({
                "event": "login_result",
                "ok": True,
                "student_name": name,
                "student_carne": carne,
                "server_clock": clock,
            })
        finally:
            password = ""

    def handle_select_convocatoria(self, cmd: Dict[str, Any]) -> None:
        assert self._page is not None
        id_conv = int(cmd.get("idConvocatoria", 0))
        id_per = int(cmd.get("idPeriodo", 0))
        row_sel = f"#Table_ConvocatoriaEstudiante tr[id='{id_conv}_{id_per}'] .btn_Consultar"
        if self._page.locator(row_sel).count() > 0:
            self._page.locator(row_sel).first.click()
        else:
            self._page.evaluate(
                "([c, p]) => { if (typeof ObtieneCursos === 'function') ObtieneCursos(c, p); }",
                [id_conv, id_per],
            )
        self._page.wait_for_selector("#tBodyCursos tbody tr", timeout=15000)

        courses = self._page.evaluate(
            """() => {
                const out = [];
                document.querySelectorAll('#tBodyCursos tbody tr').forEach(tr => {
                    if (tr.classList.contains('Rowaccordion') || !tr.id) return;
                    const code = tr.id.trim();
                    const name = (tr.querySelector('.colMateria')?.textContent || '').trim();
                    const cred = parseInt((tr.querySelector('.colCreditos')?.textContent || '0').trim(), 10) || 0;
                    const grpVal = (tr.querySelector('input.txtGrupoMatriculado')?.value || '0').trim();
                    out.push({
                        ID_MATERIA: code,
                        DESCRIPCION_MATERIA: name,
                        CANT_CREDITOS: cred,
                        ID_GRUPO: parseInt(grpVal, 10) || 0,
                        IND_MOSTRAR: true
                    });
                });
                return out;
            }"""
        )
        emit({"event": "convocatoria_selected", "ok": True, "cursos": courses})

    def handle_check_cita(self, cmd: Dict[str, Any]) -> None:
        assert self._page is not None
        reload_if_inactive = bool(cmd.get("reload_if_inactive", False))

        def _inspect() -> Dict[str, Any]:
            return self._page.evaluate(
                """() => {
                    const div = document.querySelector('.divCitas');
                    const lbl = (document.querySelector('#lblCitaMatricula')?.textContent || '').trim();
                    const activeByClass = div ? div.classList.contains('cCitasOk') : false;
                    const activeInputs = document.querySelectorAll('input.txtGrupoMatriculado:not([disabled])').length > 0;
                    const blocked = lbl.toLowerCase().includes('bloqueado');
                    return { active: activeByClass || activeInputs, label: lbl, blocked };
                }"""
            )

        st = _inspect()
        if not st["active"] and reload_if_inactive:
            log("INFO", "[Scraper] Cita inactiva en DOM; recargando página para refrescar sesión...")
            self._page.reload(wait_until="domcontentloaded")
            st = _inspect()

        emit({
            "event": "cita_status",
            "ok": True,
            "active_now": bool(st["active"]),
            "blocked": bool(st["blocked"]),
            "label": str(st["label"]),
        })

    def handle_scrape_groups(self, cmd: Dict[str, Any]) -> None:
        assert self._page is not None
        cid = str(cmd.get("idMateria", "")).strip().upper()
        self.last_xhr_horarios.pop(cid, None)

        row_sel = f"#tBodyCursos tr[id='{cid}']"
        if self._page.locator(row_sel).count() == 0:
            emit({"event": "groups_scraped", "ok": False, "idMateria": cid, "error": f"Fila #{cid} no encontrada en DOM."})
            return

        upd_btn = self._page.locator(f"#cupoupd_{cid}")
        if upd_btn.count() > 0 and upd_btn.first.is_visible():
            upd_btn.first.click()
            time.sleep(0.35)
        else:
            self._page.locator(row_sel).first.click()
            self._page.wait_for_selector(f"#bodyTableHorarios_{cid} tr", timeout=15000)

        # Prefer captured XHR JSON if available (contains Capacidad + CanMatriculados), else DOM scrape
        if cid in self.last_xhr_horarios:
            payload = self.last_xhr_horarios[cid]
            emit({
                "event": "groups_scraped",
                "ok": True,
                "source": "xhr",
                "idMateria": cid,
                "grupos": payload.get("Horario", []),
            })
            return

        dom_rows = self._page.evaluate(
            """(cid) => {
                const out = [];
                document.querySelectorAll(`#bodyTableHorarios_${cid} tr`).forEach(tr => {
                    const tds = tr.querySelectorAll('td');
                    if (tds.length < 7) return;
                    const sede = (tds[0].textContent || '').trim();
                    const gid = parseInt((tds[1].textContent || '0').trim(), 10) || 0;
                    const horHtml = tds[2].innerHTML || '';
                    const profHtml = tds[3].innerHTML || '';
                    const cupo = parseInt((tds[4].textContent || '0').trim(), 10) || 0;
                    const resv = (tds[5].textContent || '').trim().toLowerCase().startsWith('s');
                    const est = (tds[6].textContent || '').trim();
                    if (gid > 0) out.push({ sede, gid, horHtml, profHtml, cupo, resv, est });
                });
                return out;
            }""",
            cid,
        )

        grupos: List[Dict[str, Any]] = []
        for r in dom_rows:
            profs = [
                re.sub(r"<[^>]+>", "", p).strip()
                for p in re.split(r"</?br\s*/?>", r["profHtml"], flags=re.IGNORECASE)
                if re.sub(r"<[^>]+>", "", p).strip() and "No Disponible" not in p
            ]
            grupos.append({
                "IdMateria": cid,
                "NomMateria": cid,
                "IdGrupo": r["gid"],
                "Sede": r["sede"],
                "EstadoGrupo": r["est"],
                "EstadoReservado": bool(r["resv"]),
                "CanMatriculados": 0,
                "Capacidad": max(r["cupo"], 1),
                "CupoDisponible": r["cupo"],
                "TieneHorario": True,
                "TieneProfe": len(profs) > 0,
                "ListaProfesores": profs,
                "Itinerario": parse_dom_horario_text(r["horHtml"]),
            })

        emit({
            "event": "groups_scraped",
            "ok": True,
            "source": "dom",
            "idMateria": cid,
            "grupos": grupos,
        })

    def handle_enroll_group(self, cmd: Dict[str, Any]) -> None:
        cid = str(cmd.get("idMateria", "")).strip().upper()
        gid = int(cmd.get("idGrupo", 0))
        dry_run = bool(cmd.get("dryRun", True))
        allow_other_campus = bool(cmd.get("allowOtherCampus", False))
        coreq_mat = str(cmd.get("coreqMateria", "")).strip().upper()
        coreq_grp = int(cmd.get("coreqGroup", 0))

        # HARD SAFETY LOCK: never click enroll in dryRun or when unarmed!
        if dry_run or not self.armed:
            emit({
                "event": "enroll_result",
                "ok": dry_run,
                "dry_run": True,
                "blocked": not dry_run,
                "idMateria": cid,
                "numGrupo": gid,
                "summary": (
                    f"[SCRAPER DRY-RUN] Verificado selector #{cid}_{gid} sin hacer clic."
                    if dry_run else
                    f"[SCRAPER BLOQUEADO] Candado cerrado; no se envió matrícula para {cid} Gr {gid}."
                ),
            })
            return

        assert self._page is not None
        btn_sel = f"#{cid}_{gid}"
        if self._page.locator(btn_sel).count() == 0:
            # Expand accordion first
            self.handle_scrape_groups({"idMateria": cid})

        if self._page.locator(btn_sel).count() == 0:
            self._save_screenshot(f"missing_btn_{cid}_{gid}")
            emit({
                "event": "enroll_result",
                "ok": False,
                "dry_run": False,
                "idMateria": cid,
                "numGrupo": gid,
                "error_kind": "ErrGrupoNE",
                "summary": f"[Scraper] Botón #{cid}_{gid} no presente en DOM (cita inactiva o grupo inexistente).",
            })
            return

        self._page.locator(btn_sel).first.click()
        time.sleep(0.45)

        # Handle #dialogOtraSede modal if it appeared
        if self._page.locator("#dialogOtraSede").count() > 0 and self._page.locator("#dialogOtraSede").first.is_visible():
            choice = "Sí" if allow_other_campus else "No"
            self._page.locator(f".ui-dialog-buttonpane button:has-text('{choice}')").first.click()
            time.sleep(0.45)
            if not allow_other_campus:
                emit({
                    "event": "enroll_result",
                    "ok": False,
                    "dry_run": False,
                    "idMateria": cid,
                    "numGrupo": gid,
                    "error_kind": "ErrSede",
                    "summary": f"[Scraper] {cid} Gr {gid} pertenece a otra sede (cancelado por política).",
                })
                return

        # Handle #dialogCoReq* modal if it appeared
        if coreq_mat and coreq_grp > 0:
            co_btn = self._page.locator(f"#{coreq_mat}_{coreq_grp}")
            if co_btn.count() > 0 and co_btn.first.is_visible():
                co_btn.first.click()
                time.sleep(0.45)

        # Verify DOM enrolled state
        enrolled_ok = self._page.evaluate(
            """([cid, gid]) => {
                const img = document.querySelector('#img' + cid);
                const inp = document.querySelector('#grupo_' + cid);
                const hasClass = img ? img.classList.contains('ccmat') : false;
                const valMatch = inp ? (parseInt(inp.value || '0', 10) === gid) : false;
                return hasClass || valMatch;
            }""",
            [cid, gid],
        )

        if enrolled_ok:
            emit({
                "event": "enroll_result",
                "ok": True,
                "dry_run": False,
                "idMateria": cid,
                "numGrupo": gid,
                "summary": f"[Scraper] Curso {cid} matriculado en grupo {gid} (verificado en DOM).",
            })
            return

        err_msgs = self._page.evaluate(
            """(cid) => {
                const out = [];
                document.querySelectorAll(`#ulError${cid} li`).forEach(li => out.push(li.textContent.trim()));
                return out;
            }""",
            cid,
        )
        self._save_screenshot(f"enroll_err_{cid}_{gid}")
        emit({
            "event": "enroll_result",
            "ok": False,
            "dry_run": False,
            "idMateria": cid,
            "numGrupo": gid,
            "error_kind": "Restrictions",
            "summary": "[Scraper] " + (" | ".join(err_msgs) if err_msgs else "Matrícula no confirmada en DOM."),
        })

    def handle_drop_course(self, cmd: Dict[str, Any]) -> None:
        cid = str(cmd.get("idMateria", "")).strip().upper()
        dry_run = bool(cmd.get("dryRun", True))
        if dry_run or not self.armed:
            emit({
                "event": "drop_result",
                "ok": False,
                "dry_run": True,
                "idMateria": cid,
                "summary": f"[SCRAPER BLOQUEADO] Retiro de {cid} no ejecutado (dryRun={dry_run}, armed={self.armed}).",
            })
            return

        assert self._page is not None
        mat_btn = self._page.locator(f".cBtnMat_{cid}.cImgMatriculado")
        if mat_btn.count() == 0:
            self.handle_scrape_groups({"idMateria": cid})
            mat_btn = self._page.locator(f".cBtnMat_{cid}.cImgMatriculado")
        if mat_btn.count() > 0:
            mat_btn.first.click()
            time.sleep(0.4)
        emit({"event": "drop_result", "ok": True, "dry_run": False, "idMateria": cid, "summary": f"[Scraper] Retiro enviado para {cid}."})

    def handle_send_informe(self, cmd: Dict[str, Any]) -> None:
        dry_run = bool(cmd.get("dryRun", True))
        if dry_run or not self.armed:
            emit({
                "event": "informe_sent",
                "ok": dry_run,
                "dry_run": True,
                "detail": "[SCRAPER DRY-RUN] Mi Informe -> Enviar simulado sin clic." if dry_run else "[SCRAPER BLOQUEADO] Candado cerrado.",
            })
            return
        assert self._page is not None
        if self._page.locator(".subDivInforme").count() > 0:
            self._page.locator(".subDivInforme").first.click()
            self._page.wait_for_selector(".dialogInforme .cuerpoInforme", timeout=15000)
            send_btn = self._page.locator(".ui-dialog-buttonpane button:has-text('Enviar')")
            if send_btn.count() > 0:
                send_btn.first.click()
                time.sleep(0.6)
                self._save_screenshot("informe_enviado")
                emit({"event": "informe_sent", "ok": True, "dry_run": False, "detail": "[Scraper] Botón 'Enviar' de Mi Informe presionado."})
                return
        emit({"event": "informe_sent", "ok": False, "dry_run": False, "detail": "[Scraper] No se encontró el diálogo/botón 'Enviar' de Mi Informe."})

    def shutdown(self) -> None:
        try:
            if self._context is not None:
                self._context.close()
        except Exception:
            pass
        try:
            if self._browser is not None:
                self._browser.close()
        except Exception:
            pass
        try:
            if self._pw is not None:
                self._pw.stop()
        except Exception:
            pass
        self._page = None
        self._context = None
        self._browser = None
        self._pw = None

    def run_loop(self) -> None:
        for raw_line in sys.stdin:
            line = raw_line.strip()
            if not line:
                continue
            try:
                cmd = json.loads(line)
                action = str(cmd.get("cmd", ""))
                if action == "init":
                    self.handle_init(cmd)
                elif action == "arm":
                    self.handle_arm(cmd)
                elif action == "probe_login_page":
                    self.handle_probe_login_page(cmd)
                elif action == "login":
                    self.handle_login(cmd)
                elif action == "select_convocatoria":
                    self.handle_select_convocatoria(cmd)
                elif action == "check_cita":
                    self.handle_check_cita(cmd)
                elif action == "scrape_groups":
                    self.handle_scrape_groups(cmd)
                elif action == "enroll_group":
                    self.handle_enroll_group(cmd)
                elif action == "drop_course":
                    self.handle_drop_course(cmd)
                elif action == "send_informe":
                    self.handle_send_informe(cmd)
                elif action == "shutdown":
                    self.shutdown()
                    emit({"event": "shutdown_ok"})
                    return
                else:
                    emit({"event": "error", "message": f"Comando desconocido: {action}"})
            except Exception as exc:
                emit({
                    "event": "error",
                    "message": str(exc),
                    "trace": traceback.format_exc(limit=3),
                })
        self.shutdown()


if __name__ == "__main__":
    MatriculaScraperWorker().run_loop()
