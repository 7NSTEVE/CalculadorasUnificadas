"""CuadernU — TEC Costa Rica Matrícula MCP Server (Stdio).

Exposes the exact same official ITCR enrollment protocol (fast ASP.NET JSON WebMethods +
automatic Playwright Chromium DOM scraping fallback), Windows DPAPI encrypted credential vault
(binary-compatible with CuadernU's ProtectedCredentialVault), hierarchical Plan A/B/C scenario
engine, Mi Informe -> Enviar receipt sender, and background Seat Sniper daemon to AI agents.

Security invariants:
  - Starts ALWAYS in safe Dry-Run mode with armed=False.
  - Mutating actions (AgregarMatricula, AgregarMatricularCorequisito, EliminarMatricula,
    CancelaOtraSede, EnviaInforme) require explicit arming with exact confirmation phrase
    "CONFIRMAR MATRICULA REAL" AND dry_run=False.
  - NEVER logs, prints, or returns passwords, cookies, ViewState tokens, or DPAPI plaintext.
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wintypes
import email.utils
import html as html_lib
import json
import os
import re
import subprocess
import sys
import threading
import time
import urllib.parse
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import requests

CONFIRMATION_PHRASE = "CONFIRMAR MATRICULA REAL"
DEFAULT_BASE_URL = "https://tec-appsext.itcr.ac.cr/Matricula/"
USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
)

DAY_MAP = {
    "L": 0, "1": 0, "LUN": 0, "LUNES": 0,
    "K": 1, "2": 1, "MAR": 1, "MARTES": 1,
    "M": 2, "3": 2, "MIE": 2, "MIERCOLES": 2, "MIÉRCOLES": 2,
    "J": 3, "4": 3, "JUE": 3, "JUEVES": 3,
    "V": 4, "5": 4, "VIE": 4, "VIERNES": 4,
    "S": 5, "6": 5, "SAB": 5, "SABADO": 5, "SÁBADO": 5,
}


def cuadernu_appdata_dir() -> Path:
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    return Path(base) / "CuadernU"


def default_vault_path() -> Path:
    return cuadernu_appdata_dir() / "matricula_vault.dpapi"


def default_scenarios_path() -> Path:
    return cuadernu_appdata_dir() / "matricula_scenarios.json"


def default_audit_log_path() -> Path:
    return cuadernu_appdata_dir() / "matricula_logs" / "agent_audit.jsonl"


def append_audit(event: str, details: Dict[str, Any]) -> None:
    try:
        p = default_audit_log_path()
        p.parent.mkdir(parents=True, exist_ok=True)
        safe_details = {
            k: v for k, v in details.items()
            if k.lower() not in ("password", "pwd", "secret", "cookie", "cookies", "viewstate")
        }
        rec = {
            "ts_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "event": event,
            "details": safe_details,
        }
        with p.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
    except Exception:
        pass


# ==============================================================================
# 1. Windows DPAPI Vault (100% binary compatible with CuadernU C++ ProtectedCredentialVault)
# ==============================================================================

class _DATA_BLOB(ctypes.Structure):
    _fields_ = [("cbData", wintypes.DWORD), ("pbData", ctypes.POINTER(ctypes.c_char))]


class DpapiVault:
    CRYPTPROTECT_UI_FORBIDDEN = 0x1

    @staticmethod
    def _crypt32():
        if sys.platform != "win32":
            raise RuntimeError("DPAPI solo disponible en Windows.")
        c32 = ctypes.windll.crypt32
        k32 = ctypes.windll.kernel32
        c32.CryptProtectData.argtypes = [
            ctypes.POINTER(_DATA_BLOB), wintypes.LPCWSTR, ctypes.POINTER(_DATA_BLOB),
            wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(_DATA_BLOB),
        ]
        c32.CryptProtectData.restype = wintypes.BOOL
        c32.CryptUnprotectData.argtypes = [
            ctypes.POINTER(_DATA_BLOB), ctypes.POINTER(wintypes.LPWSTR), ctypes.POINTER(_DATA_BLOB),
            wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(_DATA_BLOB),
        ]
        c32.CryptUnprotectData.restype = wintypes.BOOL
        k32.LocalFree.argtypes = [wintypes.HLOCAL]
        return c32, k32

    @classmethod
    def exists(cls, path: Optional[Path] = None) -> bool:
        return (path or default_vault_path()).is_file()

    @classmethod
    def save(cls, email: str, password: str, path: Optional[Path] = None) -> Dict[str, Any]:
        email = email.strip()
        if not email or not password:
            return {"ok": False, "error": "Correo o contraseña vacíos."}
        c32, k32 = cls._crypt32()
        plain = json.dumps({"u": email, "p": password}, separators=(",", ":")).encode("utf-8")
        buf = ctypes.create_string_buffer(plain, len(plain))
        in_blob = _DATA_BLOB(len(plain), buf)
        out_blob = _DATA_BLOB()
        ok = c32.CryptProtectData(
            ctypes.byref(in_blob), "CuadernU_TecMatricula_Vault", None, None, None,
            cls.CRYPTPROTECT_UI_FORBIDDEN, ctypes.byref(out_blob),
        )
        ctypes.memset(ctypes.addressof(buf), 0, len(plain))
        if not ok:
            return {"ok": False, "error": "CryptProtectData falló."}
        try:
            cipher = ctypes.string_at(out_blob.pbData, out_blob.cbData)
            target = path or default_vault_path()
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(cipher)
            return {"ok": True, "path": str(target), "bytes": len(cipher)}
        finally:
            k32.LocalFree(out_blob.pbData)

    @classmethod
    def load(cls, path: Optional[Path] = None) -> Tuple[bool, str, str, str]:
        target = path or default_vault_path()
        if not target.is_file():
            return False, "", "", f"Bóveda DPAPI no encontrada en {target}"
        c32, k32 = cls._crypt32()
        cipher = target.read_bytes()
        buf = ctypes.create_string_buffer(cipher, len(cipher))
        in_blob = _DATA_BLOB(len(cipher), buf)
        out_blob = _DATA_BLOB()
        ok = c32.CryptUnprotectData(
            ctypes.byref(in_blob), None, None, None, None,
            cls.CRYPTPROTECT_UI_FORBIDDEN, ctypes.byref(out_blob),
        )
        if not ok:
            return False, "", "", "CryptUnprotectData falló (usuario Windows distinto o archivo corrupto)."
        try:
            raw = ctypes.string_at(out_blob.pbData, out_blob.cbData)
            ctypes.memset(out_blob.pbData, 0, out_blob.cbData)
            data = json.loads(raw.decode("utf-8"))
            u = str(data.get("u", "")).strip()
            p = str(data.get("p", ""))
            if not u or not p:
                return False, "", "", "Bóveda DPAPI sin credenciales válidas."
            return True, u, p, ""
        finally:
            k32.LocalFree(out_blob.pbData)

    @classmethod
    def delete(cls, path: Optional[Path] = None) -> Dict[str, Any]:
        target = path or default_vault_path()
        if target.exists():
            target.unlink()
        return {"ok": True, "deleted": True, "path": str(target)}


# ==============================================================================
# 2. Protocol Parsing & Schedule/Scenario Helpers
# ==============================================================================

def _parse_time_min(hhmm: str) -> Optional[int]:
    parts = (hhmm or "").strip().split(":")
    if len(parts) < 2:
        return None
    try:
        h, m = int(parts[0]), int(parts[1])
        if 0 <= h <= 23 and 0 <= m <= 59:
            return h * 60 + m
    except ValueError:
        return None
    return None


def _parse_day(raw: Any) -> Optional[int]:
    key = str(raw or "").strip().upper()
    if key in DAY_MAP:
        return DAY_MAP[key]
    for prefix, idx in (("LUN", 0), ("MAR", 1), ("MI", 2), ("JUE", 3), ("VIE", 4), ("SAB", 5), ("SÁB", 5)):
        if key.startswith(prefix):
            return idx
    return None


def _sessions_overlap(sess_a: List[Dict[str, Any]], sess_b: List[Dict[str, Any]]) -> bool:
    for a in sess_a:
        da = _parse_day(a.get("Dia"))
        sa, ea = _parse_time_min(a.get("Inicio", "")), _parse_time_min(a.get("Fin", ""))
        if da is None or sa is None or ea is None:
            continue
        for b in sess_b:
            db = _parse_day(b.get("Dia"))
            sb, eb = _parse_time_min(b.get("Inicio", "")), _parse_time_min(b.get("Fin", ""))
            if db is None or sb is None or eb is None or da != db:
                continue
            if sa < eb and sb < ea:
                return True
    return False


def _extract_webforms_fields(page_html: str) -> Dict[str, str]:
    fields: Dict[str, str] = {"submit_name": "btnEntrar", "submit_value": "Ingresar"}
    for m in re.finditer(r"<input\b([^>]+)>", page_html, flags=re.IGNORECASE):
        attrs = m.group(1)
        tm = re.search(r"\btype\s*=\s*[\"']?([^\"'\s>]+)", attrs, re.I)
        nm = re.search(r"\bname\s*=\s*[\"']([^\"']+)[\"']", attrs, re.I)
        vm = re.search(r"\bvalue\s*=\s*[\"']([^\"']*)[\"']", attrs, re.I)
        itype = (tm.group(1).lower() if tm else "")
        name = (nm.group(1) if nm else "")
        val = html_lib.unescape(vm.group(1)) if vm else ""
        if name in ("__VIEWSTATE", "__VIEWSTATEGENERATOR", "__EVENTVALIDATION"):
            fields[name] = val
        elif itype == "submit" and name:
            fields["submit_name"] = name
            if val:
                fields["submit_value"] = val
    return fields


def _encode_form(pairs: List[Tuple[str, str]]) -> bytes:
    return "&".join(
        f"{urllib.parse.quote(k, safe='')}={urllib.parse.quote(v, safe='')}"
        for k, v in pairs
    ).encode("utf-8")


def _unwrap_d(resp_json: Any) -> Any:
    if not isinstance(resp_json, dict) or "d" not in resp_json:
        raise ValueError(f"Sobre ASP.NET inválido: {str(resp_json)[:180]}")
    d = resp_json["d"]
    if isinstance(d, str):
        s = d.strip()
        if s in ("True", "true", "False", "false", "M", "ModH", "Error", ""):
            return s
        try:
            return json.loads(s)
        except Exception:
            return s
    return d


def match_rule_groups(
    rule: Dict[str, Any],
    live_groups: List[Dict[str, Any]],
    fixed_sessions: List[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    filt = rule.get("filter") or {}
    require_open = bool(filt.get("requireOpenSeats", True))
    allow_reserved = bool(filt.get("allowReservedGroups", True))
    campus = str(filt.get("campusFilter") or "").strip().lower()
    allowed_days = set(filt.get("allowedDays") or [])
    forbidden_days = set(filt.get("forbiddenDays") or [])
    earliest = _parse_time_min(str(filt.get("earliestStart") or "06:00")) or 360
    latest = _parse_time_min(str(filt.get("latestEnd") or "22:00")) or 1320
    prof_needle = str(filt.get("preferredProfessorContains") or "").strip().lower()
    explicit = [int(x) for x in (rule.get("explicitGroupPriority") or []) if int(x) > 0]
    allow_dyn = bool(rule.get("allowDynamicMatching", True))

    candidates: List[Dict[str, Any]] = []
    seen: set[int] = set()

    def _score(g: Dict[str, Any], exp_idx: int) -> int:
        sc = (10000 - exp_idx * 200) if exp_idx >= 0 else 2000
        if prof_needle and any(prof_needle in str(p).lower() for p in (g.get("ListaProfesores") or [])):
            sc += 400
        if not g.get("EstadoReservado"):
            sc += 100
        sc += min(int(g.get("CupoDisponible") or 0), 50) * 3
        return sc

    # 1. Explicit priority groups
    for idx, gid in enumerate(explicit):
        for g in live_groups:
            if int(g.get("IdGrupo") or 0) != gid:
                continue
            if require_open and int(g.get("CupoDisponible") or 0) <= 0:
                continue
            if _sessions_overlap(fixed_sessions, g.get("Itinerario") or []):
                continue
            candidates.append({"group": g, "score": _score(g, idx), "explicit": True})
            seen.add(gid)
            break

    # 2. Dynamic schedule matching
    if allow_dyn:
        has_time_or_day = bool(allowed_days or forbidden_days or earliest > 360 or latest < 1320)
        for g in live_groups:
            gid = int(g.get("IdGrupo") or 0)
            if gid <= 0 or gid in seen:
                continue
            if require_open and int(g.get("CupoDisponible") or 0) <= 0:
                continue
            if not allow_reserved and bool(g.get("EstadoReservado")):
                continue
            if campus and campus not in str(g.get("Sede") or "").lower():
                continue
            itin = g.get("Itinerario") or []
            if has_time_or_day:
                valid_any = False
                ok_slots = True
                for s in itin:
                    d = _parse_day(s.get("Dia"))
                    t1, t2 = _parse_time_min(s.get("Inicio", "")), _parse_time_min(s.get("Fin", ""))
                    if d is None or t1 is None or t2 is None:
                        continue
                    valid_any = True
                    if allowed_days and d not in allowed_days:
                        ok_slots = False
                        break
                    if d in forbidden_days:
                        ok_slots = False
                        break
                    if t1 < earliest or t2 > latest:
                        ok_slots = False
                        break
                if not valid_any or not ok_slots:
                    continue
            if _sessions_overlap(fixed_sessions, itin):
                continue
            candidates.append({"group": g, "score": _score(g, -1), "explicit": False})
            seen.add(gid)

    candidates.sort(key=lambda c: c["score"], reverse=True)
    return candidates


def evaluate_scenarios(
    scenarios: List[Dict[str, Any]],
    groups_by_course: Dict[str, List[Dict[str, Any]]],
    already_enrolled_sessions: List[Dict[str, Any]],
    enable_plan_z: bool = False,
) -> Dict[str, Any]:
    ordered = sorted(
        [s for s in scenarios if s.get("enabled", True)],
        key=lambda s: int(s.get("priority", 1)),
    )

    def _eval_one(scen: Dict[str, Any]) -> Dict[str, Any]:
        rules = scen.get("courses") or []
        if not rules:
            return {"scenario": scen.get("name", ""), "feasible": False, "steps": [], "diagnostics": ["Escenario vacío"]}
        cands_per_rule: List[List[Dict[str, Any]]] = []
        diags: List[str] = []
        for r in rules:
            cid = str(r.get("idMateria", "")).strip().upper()
            live = groups_by_course.get(cid) or []
            cands = match_rule_groups(r, live, already_enrolled_sessions)
            if not cands:
                diags.append(f"{cid}: sin grupos viables con cupo/filtro.")
                if r.get("required", True):
                    return {"scenario": scen.get("name", ""), "feasible": False, "steps": [], "diagnostics": diags}
            cands_per_rule.append(cands)

        best: Dict[str, Any] = {"found": False, "opt": -1, "score": -1, "steps": []}

        def _bt(idx: int, used: List[Dict[str, Any]], chosen: List[Dict[str, Any]], sc: int, opt_cnt: int) -> None:
            if idx >= len(rules):
                if (not best["found"]) or opt_cnt > best["opt"] or (opt_cnt == best["opt"] and sc > best["score"]):
                    best["found"] = True
                    best["opt"] = opt_cnt
                    best["score"] = sc
                    best["steps"] = list(chosen)
                return
            r = rules[idx]
            for c in cands_per_rule[idx]:
                g = c["group"]
                itin = g.get("Itinerario") or []
                if _sessions_overlap(used, itin):
                    continue
                step = {
                    "idMateria": str(r.get("idMateria", "")).upper(),
                    "chosenGroup": int(g.get("IdGrupo", 0)),
                    "availableSeats": int(g.get("CupoDisponible", 0)),
                    "backupGroups": [
                        int(o["group"].get("IdGrupo", 0))
                        for o in cands_per_rule[idx]
                        if int(o["group"].get("IdGrupo", 0)) != int(g.get("IdGrupo", 0))
                    ][:3],
                    "required": bool(r.get("required", True)),
                    "allowOtherCampus": bool(r.get("allowOtherCampus", False)),
                    "coreqMateria": str(r.get("coreqMateria", "")).upper(),
                    "coreqPreferredGroups": r.get("coreqPreferredGroups") or [],
                    "score": c["score"],
                }
                _bt(idx + 1, used + itin, chosen + [step], sc + c["score"], opt_cnt + (0 if step["required"] else 1))
            if not r.get("required", True):
                _bt(idx + 1, used, chosen, sc, opt_cnt)

        _bt(0, list(already_enrolled_sessions), [], 0, 0)
        if not best["found"]:
            diags.append("Sin combinación libre de choques.")
            return {"scenario": scen.get("name", ""), "feasible": False, "steps": [], "diagnostics": diags}

        # MRV ordering: required first, scarcest seats first
        steps = sorted(best["steps"], key=lambda st: (not st["required"], st["availableSeats"], -st["score"]))
        return {"scenario": scen.get("name", ""), "feasible": True, "steps": steps, "diagnostics": diags}

    evals = []
    selected = None
    for s in ordered:
        ev = _eval_one(s)
        evals.append(ev)
        if ev["feasible"] and selected is None:
            selected = ev

    if selected is None and enable_plan_z:
        seen_c: set[str] = set()
        pooled: List[Dict[str, Any]] = []
        for s in ordered:
            for r in (s.get("courses") or []):
                cid = str(r.get("idMateria", "")).upper()
                if cid and cid not in seen_c:
                    seen_c.add(cid)
                    nr = dict(r)
                    nr["required"] = False
                    nr["allowDynamicMatching"] = True
                    pooled.append(nr)
        z_ev = _eval_one({"name": "Plan Z (Rescate Automático)", "courses": pooled})
        z_ev["feasible"] = len(z_ev["steps"]) > 0
        z_ev["isPlanZRescue"] = True
        evals.append(z_ev)
        if z_ev["feasible"]:
            selected = z_ev

    return {
        "hasViableScenario": selected is not None,
        "selected": selected,
        "evaluations": evals,
    }


# ==============================================================================
# 3. Dual-Transport Engine (Fast API + Playwright Worker Bridge + Seat Sniper)
# ==============================================================================

class PlaywrightBridge:
    """Thin persistent JSON-Lines bridge to tools/matricula_scraper_worker.py."""

    def __init__(self, base_url: str, headless: bool = True) -> None:
        self.base_url = base_url
        self.headless = headless
        self.proc: Optional[subprocess.Popen] = None
        self._lock = threading.Lock()

    def _worker_script(self) -> Path:
        return Path(__file__).resolve().parent / "matricula_scraper_worker.py"

    def _python_exe(self) -> str:
        candidates = [
            Path("E:/Proyectos/CuadernU/.venv/Scripts/python.exe"),
            Path(sys.executable),
        ]
        for c in candidates:
            if c.is_file():
                return str(c)
        return sys.executable

    def start(self) -> Dict[str, Any]:
        with self._lock:
            if self.proc and self.proc.poll() is None:
                return {"ok": True}
            env = dict(os.environ)
            env["PYTHONIOENCODING"] = "utf-8"
            env["PYTHONUNBUFFERED"] = "1"
            self.proc = subprocess.Popen(
                [self._python_exe(), "-u", str(self._worker_script())],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                env=env,
            )
            return self._call_unlocked({
                "cmd": "init",
                "base_url": self.base_url,
                "headless": self.headless,
                "screenshot_dir": str(cuadernu_appdata_dir() / "matricula_logs"),
            }, expected_events=("ready", "error"))

    def call(self, cmd: Dict[str, Any], expected_events: Tuple[str, ...]) -> Dict[str, Any]:
        with self._lock:
            if not self.proc or self.proc.poll() is not None:
                self.start()
            return self._call_unlocked(cmd, expected_events)

    def _call_unlocked(self, cmd: Dict[str, Any], expected_events: Tuple[str, ...]) -> Dict[str, Any]:
        assert self.proc is not None and self.proc.stdin is not None and self.proc.stdout is not None
        self.proc.stdin.write(json.dumps(cmd, ensure_ascii=False) + "\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                return {"event": "error", "ok": False, "message": "Playwright worker cerró stdout inesperadamente."}
            try:
                ev = json.loads(line)
            except Exception:
                continue
            if ev.get("event") in expected_events or ev.get("event") == "error":
                return ev

    def stop(self) -> None:
        with self._lock:
            if self.proc and self.proc.poll() is None:
                try:
                    self._call_unlocked({"cmd": "shutdown"}, ("shutdown_ok",))
                except Exception:
                    pass
                try:
                    self.proc.terminate()
                    self.proc.wait(timeout=2)
                except Exception:
                    self.proc.kill()
            self.proc = None


class TecMatriculaEngine:
    def __init__(self, base_url: str = DEFAULT_BASE_URL) -> None:
        self.base_url = base_url if base_url.endswith("/") else base_url + "/"
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": USER_AGENT,
            "Accept-Language": "es-CR,es;q=0.9,en;q=0.8",
        })
        self.authenticated = False
        self.armed = False
        self.student_name = ""
        self.student_carne = ""
        self.server_clock_raw = ""
        self.server_clock_offset_ms = 0
        self.active_convocatoria = 0
        self.active_periodo = 0
        self.convocatorias: List[Dict[str, Any]] = []
        self.course_offers: List[Dict[str, Any]] = []
        self.groups_cache: Dict[str, List[Dict[str, Any]]] = {}
        self.scraper: Optional[PlaywrightBridge] = None
        self._lock = threading.RLock()

        # Background Seat Sniper state
        self._sniper_stop = threading.Event()
        self._sniper_thread: Optional[threading.Thread] = None
        self._sniper_state: Dict[str, Any] = {
            "active": False,
            "targets": [],
            "polls": 0,
            "detected": [],
            "enrolled": [],
            "logs": [],
            "last_error": "",
        }

    def _is_loopback(self) -> bool:
        host = (urllib.parse.urlparse(self.base_url).hostname or "").lower()
        return host in ("127.0.0.1", "localhost", "::1")

    def _url(self, rel: str) -> str:
        return urllib.parse.urljoin(self.base_url, rel)

    def _update_clock(self, resp: requests.Response, rtt_ms: int) -> None:
        hdr = resp.headers.get("Date")
        if not hdr:
            return
        try:
            dt = email.utils.parsedate_to_datetime(hdr)
            srv_ms = int(dt.timestamp() * 1000) + rtt_ms // 2
            self.server_clock_offset_ms = srv_ms - int(time.time() * 1000)
        except Exception:
            pass

    def _post_wm(self, rel: str, payload: Optional[Dict[str, Any]] = None) -> Tuple[bool, Any, str]:
        t0 = time.monotonic()
        origin = f"{urllib.parse.urlparse(self.base_url).scheme}://{urllib.parse.urlparse(self.base_url).netloc}"
        try:
            resp = self.session.post(
                self._url(rel),
                data=json.dumps(payload or {}),
                headers={
                    "Content-Type": "application/json; charset=utf-8",
                    "Accept": "application/json, text/javascript, */*; q=0.01",
                    "X-Requested-With": "XMLHttpRequest",
                    "Origin": origin,
                    "Referer": self._url("frmMatricula.aspx"),
                },
                timeout=25,
                allow_redirects=True,
            )
            self._update_clock(resp, int((time.monotonic() - t0) * 1000))
            if resp.status_code == 401 or "frmautenticacion.aspx" in resp.url.lower():
                self.authenticated = False
                return False, None, f"Sesión expirada en {rel} (HTTP {resp.status_code})."
            if resp.status_code != 200:
                return False, None, f"{rel} HTTP {resp.status_code}"
            return True, _unwrap_d(resp.json()), ""
        except Exception as exc:
            return False, None, f"{rel} error: {exc}"

    def arm_live(self, armed: bool, confirmation_phrase: str = "") -> Dict[str, Any]:
        with self._lock:
            if not armed:
                self.armed = False
                if self.scraper:
                    self.scraper.call({"cmd": "arm", "armed": False}, ("arm_result",))
                append_audit("arm_live", {"armed": False})
                return {"ok": True, "armed": False, "message": "Candado cerrado (solo simulación Dry-Run)."}
            if confirmation_phrase.strip() != CONFIRMATION_PHRASE:
                self.armed = False
                append_audit("arm_live_rejected", {"armed": False})
                return {
                    "ok": False,
                    "armed": False,
                    "error": f"Frase incorrecta. Se requiere exactamente: {CONFIRMATION_PHRASE}",
                }
            self.armed = True
            if self.scraper:
                self.scraper.call(
                    {"cmd": "arm", "armed": True, "confirmation_phrase": CONFIRMATION_PHRASE},
                    ("arm_result",),
                )
            append_audit("arm_live", {"armed": True})
            return {"ok": True, "armed": True, "message": "Candado abierto: mutaciones reales habilitadas."}

    def _resolve_creds(self, use_vault: bool, email: str, password: str, vault_path: Optional[Path] = None) -> Tuple[bool, str, str, str]:
        if email.strip() and password:
            return True, email.strip(), password, ""
        if use_vault:
            return DpapiVault.load(vault_path)
        return False, "", "", "No se proporcionaron credenciales ni bóveda DPAPI."

    def login(
        self,
        use_vault: bool = True,
        email: str = "",
        password: str = "",
        id_convocatoria: int = 0,
        id_periodo: int = 0,
        transport: str = "auto",
        headless: bool = True,
        vault_path: Optional[Path] = None,
    ) -> Dict[str, Any]:
        ok_c, user, pwd, c_err = self._resolve_creds(use_vault, email, password, vault_path)
        if not ok_c:
            return {"ok": False, "error": c_err}

        with self._lock:
            if transport == "scraping":
                return self._login_via_scraper(user, pwd, id_convocatoria, id_periodo, headless)

            # Fast HTTP/WebForms login
            self.session.cookies.clear()
            self.authenticated = False
            try:
                t0 = time.monotonic()
                r0 = self.session.get(self._url("frmMatricula.aspx"), timeout=25, allow_redirects=True)
                self._update_clock(r0, int((time.monotonic() - t0) * 1000))
                fields = _extract_webforms_fields(r0.text)
                if "__VIEWSTATE" not in fields or "__EVENTVALIDATION" not in fields:
                    raise RuntimeError("No se encontraron __VIEWSTATE/__EVENTVALIDATION.")

                pairs = [
                    ("__EVENTTARGET", ""),
                    ("__EVENTARGUMENT", ""),
                    ("__VIEWSTATE", fields["__VIEWSTATE"]),
                ]
                if "__VIEWSTATEGENERATOR" in fields:
                    pairs.append(("__VIEWSTATEGENERATOR", fields["__VIEWSTATEGENERATOR"]))
                pairs.extend([
                    ("__EVENTVALIDATION", fields["__EVENTVALIDATION"]),
                    ("txtUsuario", user),
                    ("txtPassword", pwd),
                    (fields["submit_name"], fields["submit_value"]),
                ])
                origin = f"{urllib.parse.urlparse(self.base_url).scheme}://{urllib.parse.urlparse(self.base_url).netloc}"
                r1 = self.session.post(
                    r0.url,
                    data=_encode_form(pairs),
                    headers={
                        "Content-Type": "application/x-www-form-urlencoded",
                        "Origin": origin,
                        "Referer": r0.url,
                    },
                    timeout=25,
                    allow_redirects=True,
                )
                if r1.status_code != 200 or "frmautenticacion.aspx" in r1.url.lower() or "txtUsuario" in r1.text:
                    return {"ok": False, "error": "Credenciales rechazadas por frmAutenticacion.aspx."}

                self.authenticated = True
                m_nom = re.search(r"id=[\"']ctl00_lblNombre[\"'][^>]*>(.*?)</span>", r1.text, re.I | re.S)
                m_car = re.search(r"id=[\"']ctl00_lblCarne[\"'][^>]*>(.*?)</span>", r1.text, re.I | re.S)
                m_clk = re.search(r"ObtieneHora\s*\(\s*[\"']([^\"']+)[\"']\s*\)", r1.text)
                m_con = re.search(r"ValidaConvocatoria\s*\(\s*(\[.*?\])\s*\)", r1.text, re.S)

                self.student_name = html_lib.unescape(re.sub(r"<[^>]+>", "", m_nom.group(1))).strip() if m_nom else ""
                self.student_carne = html_lib.unescape(re.sub(r"<[^>]+>", "", m_car.group(1))).strip() if m_car else ""
                self.server_clock_raw = m_clk.group(1).strip() if m_clk else ""
                self.convocatorias = json.loads(m_con.group(1)) if m_con else []

                if not self.convocatorias:
                    ok_cv, cv_data, _ = self._post_wm("frmMatricula.aspx/ConsultaConvocatorias")
                    if ok_cv and isinstance(cv_data, list):
                        self.convocatorias = cv_data

                # Bind active convocatoria (mandatory ASP.NET Session initialization)
                chosen_c, chosen_p = id_convocatoria, id_periodo
                if chosen_c <= 0 and self.convocatorias:
                    active = [c for c in self.convocatorias if c.get("DENTRO_FECHA")]
                    pick = active[0] if active else self.convocatorias[0]
                    chosen_c = int(pick.get("ID_CONVOCATORIA") or 0)
                    chosen_p = int(pick.get("ID_PERIODO") or 0)

                courses_info: Dict[str, Any] = {}
                if chosen_c > 0:
                    ok_co, co_data, co_err = self._post_wm(
                        "frmMatricula.aspx/ObtieneCursos",
                        {"idConvocatoria": str(chosen_c), "idPeriodo": str(chosen_p)},
                    )
                    if ok_co and isinstance(co_data, dict):
                        self.active_convocatoria = chosen_c
                        self.active_periodo = chosen_p
                        self.course_offers = co_data.get("Cursos") if isinstance(co_data.get("Cursos"), list) else []
                        courses_info = {
                            "estadoCita": bool(co_data.get("EstadoCita")),
                            "esBloqueado": bool(co_data.get("EsBloqueado")),
                            "fechaCita": co_data.get("FechaCita", ""),
                            "coursesCount": len(self.course_offers),
                        }
                    else:
                        courses_info = {"error": co_err}

                append_audit("login_api", {"ok": True, "conv": chosen_c, "periodo": chosen_p})
                return {
                    "ok": True,
                    "transport": "api",
                    "studentName": self.student_name,
                    "studentCarne": self.student_carne,
                    "serverClockRaw": self.server_clock_raw,
                    "clockOffsetMs": self.server_clock_offset_ms,
                    "activeConvocatoria": self.active_convocatoria,
                    "activePeriodo": self.active_periodo,
                    "convocatorias": self.convocatorias,
                    "coursesSummary": courses_info,
                    "courses": self.course_offers,
                }
            except Exception as exc:
                if transport == "auto":
                    return self._login_via_scraper(user, pwd, id_convocatoria, id_periodo, headless)
                return {"ok": False, "error": str(exc)}

    def _ensure_scraper(self, headless: bool = True) -> PlaywrightBridge:
        if self.scraper is None:
            self.scraper = PlaywrightBridge(self.base_url, headless=headless)
            self.scraper.start()
            if self.armed:
                self.scraper.call(
                    {"cmd": "arm", "armed": True, "confirmation_phrase": CONFIRMATION_PHRASE},
                    ("arm_result",),
                )
        return self.scraper

    def _login_via_scraper(self, user: str, pwd: str, id_conv: int, id_per: int, headless: bool) -> Dict[str, Any]:
        br = self._ensure_scraper(headless=headless)
        ev = br.call({"cmd": "login", "email": user, "password": pwd}, ("login_result",))
        if not ev.get("ok"):
            return {"ok": False, "transport": "scraping", "error": ev.get("error") or ev.get("message")}
        self.authenticated = True
        self.student_name = str(ev.get("student_name") or "")
        self.student_carne = str(ev.get("student_carne") or "")
        if id_conv > 0:
            sel = br.call(
                {"cmd": "select_convocatoria", "idConvocatoria": id_conv, "idPeriodo": id_per},
                ("convocatoria_selected",),
            )
            if sel.get("ok"):
                self.active_convocatoria = id_conv
                self.active_periodo = id_per
                self.course_offers = sel.get("cursos") or []
        append_audit("login_scraping", {"ok": True, "conv": self.active_convocatoria})
        return {
            "ok": True,
            "transport": "scraping",
            "studentName": self.student_name,
            "studentCarne": self.student_carne,
            "courses": self.course_offers,
        }

    def check_cita_and_morosidad(self, auto_relogin_if_inactive: bool = False, vault_path: Optional[Path] = None) -> Dict[str, Any]:
        with self._lock:
            ok_c, d_cita, err_c = self._post_wm("frmMatricula.aspx/ConsultaCitaMatriculaEstudiante")
            active_now = (d_cita in ("True", "true", True))
            if not active_now and auto_relogin_if_inactive:
                # Step 1: refresh session via ObtieneCursos; Step 2: full clean relogin via DPAPI vault
                if self.active_convocatoria > 0:
                    self._post_wm(
                        "frmMatricula.aspx/ObtieneCursos",
                        {"idConvocatoria": str(self.active_convocatoria), "idPeriodo": str(self.active_periodo)},
                    )
                    ok_c, d_cita, err_c = self._post_wm("frmMatricula.aspx/ConsultaCitaMatriculaEstudiante")
                    active_now = (d_cita in ("True", "true", True))
                if not active_now and DpapiVault.exists(vault_path):
                    self.login(use_vault=True, id_convocatoria=self.active_convocatoria, id_periodo=self.active_periodo, vault_path=vault_path)
                    ok_c, d_cita, err_c = self._post_wm("frmMatricula.aspx/ConsultaCitaMatriculaEstudiante")
                    active_now = (d_cita in ("True", "true", True))

            _, d_det, _ = self._post_wm("frmMatricula.aspx/ObtieneCitaMatricula")
            _, d_mor, _ = self._post_wm("frmMatricula.aspx/ConsultaMorosidadEstudiante")
            _, d_sch, _ = self._post_wm("frmMatricula.aspx/ConsultaHorarioMatriculado")
            return {
                "ok": ok_c,
                "activeNow": active_now,
                "blockedByMorosidad": (d_cita == "M"),
                "citaRaw": d_cita,
                "citaDetalle": d_det,
                "morosidad": d_mor,
                "horarioMatriculado": d_sch,
                "error": err_c,
            }

    def get_groups(self, course_ids: List[str], transport: str = "auto", headless: bool = True) -> Dict[str, Any]:
        spacing = 0.02 if self._is_loopback() else 1.15
        result: Dict[str, Any] = {}
        used_transport = "api"
        with self._lock:
            for idx, raw_cid in enumerate(course_ids):
                cid = raw_cid.strip().upper()
                if not cid:
                    continue
                if idx > 0:
                    time.sleep(spacing)
                if transport != "scraping":
                    ok, data, err = self._post_wm("frmMatricula.aspx/ConsultaHorarios", {"idMateria": cid})
                    if ok and isinstance(data, dict) and isinstance(data.get("Horario"), list):
                        self.groups_cache[cid] = data["Horario"]
                        result[cid] = data["Horario"]
                        continue
                    if transport == "api":
                        result[cid] = {"error": err}
                        continue
                # Automatic Playwright DOM scraping fallback
                used_transport = "scraping"
                br = self._ensure_scraper(headless=headless)
                ev = br.call({"cmd": "scrape_groups", "idMateria": cid}, ("groups_scraped",))
                if ev.get("ok"):
                    self.groups_cache[cid] = ev.get("grupos") or []
                    result[cid] = self.groups_cache[cid]
                else:
                    result[cid] = {"error": ev.get("error") or ev.get("message")}
        return {"ok": True, "transport": used_transport, "groupsByCourse": result}

    def enroll_course(
        self,
        id_materia: str,
        id_grupo: int,
        dry_run: bool = True,
        allow_other_campus: bool = False,
        coreq_materia: str = "",
        coreq_group: int = 0,
        transport: str = "auto",
        headless: bool = True,
    ) -> Dict[str, Any]:
        cid = id_materia.strip().upper()
        gid = int(id_grupo)
        with self._lock:
            if dry_run or not self.armed:
                msg = (
                    f"[DRY-RUN] Simulado {cid} Grupo {gid} sin enviar mutación."
                    if dry_run else
                    f"[BLOQUEADO] Candado cerrado: llame tec_matricula_arm_live con '{CONFIRMATION_PHRASE}' primero."
                )
                append_audit("enroll_blocked_or_dry", {"course": cid, "group": gid, "dry_run": dry_run, "armed": self.armed})
                return {"ok": dry_run, "dryRun": True, "blocked": not dry_run, "idMateria": cid, "idGrupo": gid, "message": msg}

            if transport == "scraping":
                br = self._ensure_scraper(headless=headless)
                ev = br.call({
                    "cmd": "enroll_group", "idMateria": cid, "idGrupo": gid, "dryRun": False,
                    "allowOtherCampus": allow_other_campus, "coreqMateria": coreq_materia.upper(), "coreqGroup": int(coreq_group),
                }, ("enroll_result",))
                append_audit("enroll_scraping", {"course": cid, "group": gid, "ok": bool(ev.get("ok"))})
                return ev

            ok, data, err = self._post_wm("frmMatricula.aspx/AgregarMatricula", {"idMateria": cid, "idGrupo": str(gid)})
            if not ok:
                if transport == "auto":
                    br = self._ensure_scraper(headless=headless)
                    ev = br.call({
                        "cmd": "enroll_group", "idMateria": cid, "idGrupo": gid, "dryRun": False,
                        "allowOtherCampus": allow_other_campus, "coreqMateria": coreq_materia.upper(), "coreqGroup": int(coreq_group),
                    }, ("enroll_result",))
                    append_audit("enroll_failover_scraping", {"course": cid, "group": gid, "ok": bool(ev.get("ok"))})
                    return ev
                return {"ok": False, "idMateria": cid, "idGrupo": gid, "error": err}

            if isinstance(data, dict) and data.get("Estado") is True:
                append_audit("enroll_api_ok", {"course": cid, "group": data.get("NumGrupo", gid)})
                return {"ok": True, "transport": "api", "idMateria": cid, "numGrupo": data.get("NumGrupo", gid), "raw": data}

            err_str = str((data or {}).get("ErrorString") or "")
            # Handle ErrSede (confirm 2nd call or cancel)
            if err_str == "ErrSede":
                if allow_other_campus:
                    ok2, data2, err2 = self._post_wm("frmMatricula.aspx/AgregarMatricula", {"idMateria": cid, "idGrupo": str(gid)})
                    return {"ok": bool(ok2 and isinstance(data2, dict) and data2.get("Estado")), "idMateria": cid, "raw": data2, "error": err2}
                self._post_wm("frmMatricula.aspx/CancelaOtraSede", {"idMateria": cid, "idGrupo": str(gid)})
                return {"ok": False, "idMateria": cid, "errorKind": "ErrSede", "message": "Otra sede cancelada por política."}

            # Handle single corequisite
            if err_str.startswith("ErrCoRequisito_"):
                co_id = err_str.split("_", 1)[1]
                co_g = int(coreq_group)
                if co_g <= 0:
                    for g in self.groups_cache.get(co_id, []):
                        if int(g.get("CupoDisponible") or 0) > 0:
                            co_g = int(g.get("IdGrupo") or 0)
                            break
                if co_g > 0:
                    ok_co, d_co, err_co = self._post_wm(
                        "frmMatricula.aspx/AgregarMatricularCorequisito",
                        {"idMateria": co_id, "idGrupo": str(co_g)},
                    )
                    return {
                        "ok": bool(ok_co and isinstance(d_co, dict) and d_co.get("Estado")),
                        "idMateria": cid, "coreqMateria": co_id, "coreqGroup": co_g, "raw": d_co, "error": err_co,
                    }

            append_audit("enroll_api_rejected", {"course": cid, "group": gid, "errorString": err_str})
            return {"ok": False, "transport": "api", "idMateria": cid, "idGrupo": gid, "errorString": err_str, "raw": data}

    def send_informe(self, dry_run: bool = True, transport: str = "auto", headless: bool = True) -> Dict[str, Any]:
        with self._lock:
            if dry_run or not self.armed:
                return {
                    "ok": dry_run,
                    "dryRun": True,
                    "blocked": not dry_run,
                    "message": "[DRY-RUN] Mi Informe -> Enviar simulado sin envío de correo." if dry_run else "[BLOQUEADO] Candado cerrado para EnviaInforme.",
                }
            if transport == "scraping":
                br = self._ensure_scraper(headless=headless)
                ev = br.call({"cmd": "send_informe", "dryRun": False}, ("informe_sent",))
                append_audit("send_informe_scraping", {"ok": bool(ev.get("ok"))})
                return ev

            ok_g, d_g, err_g = self._post_wm(
                "frmInformeMatricula.aspx/GeneraInforme",
                {"idEstudiante": "", "idAno": "", "idModalidad": "", "idPeriodo": "", "tipo": "E", "topeCursos": "5"},
            )
            if not ok_g or not isinstance(d_g, dict):
                return {"ok": False, "error": err_g or f"GeneraInforme devolvió: {d_g}"}

            inf_g = d_g.get("objInformeG") or {}
            mats = inf_g.get("DetalleMaterias") or []
            per = d_g.get("objInfoPersonal") or {}
            items_html = "".join(
                f"<li>{html_lib.escape(str(m.get('IdMateria','')))} - {html_lib.escape(str(m.get('NomMateria','')))} (Gr {m.get('IdGrupo','')})</li>"
                for m in mats
            )
            html_body = (
                f"<h3>Informe de Matricula TEC</h3>"
                f"<p><b>Carrera:</b> {html_lib.escape(str(per.get('Plan','')))} | "
                f"<b>Sede:</b> {html_lib.escape(str(per.get('Sede','')))}</p>"
                f"<ul>{items_html}</ul>"
            )
            ok_s, d_s, err_s = self._post_wm("frmInformeMatricula.aspx/EnviaInforme", {"htmlInforme": html_body})
            sent_ok = bool(ok_s and d_s and str(d_s).lower() != "false")
            append_audit("send_informe_api", {"ok": sent_ok, "coursesCount": len(mats)})
            return {"ok": sent_ok, "destination": str(d_s or ""), "coursesCount": len(mats), "error": err_s}

    def verify_enrolled(self) -> Dict[str, Any]:
        with self._lock:
            ok_h, d_h, err_h = self._post_wm("frmMatricula.aspx/ConsultaHorarioMatriculado")
            ok_i, d_i, err_i = self._post_wm(
                "frmInformeMatricula.aspx/GeneraInforme",
                {"idEstudiante": "", "idAno": "", "idModalidad": "", "idPeriodo": "", "tipo": "E", "topeCursos": "5"},
            )
            return {
                "ok": ok_h or ok_i,
                "horarioMatriculado": d_h if ok_h else {"error": err_h},
                "informe": d_i if ok_i else {"error": err_i},
            }

    def execute_scenarios(
        self,
        scenarios: Optional[List[Dict[str, Any]]] = None,
        dry_run: bool = True,
        enable_plan_z: bool = False,
        send_informe_on_success: bool = True,
        transport: str = "auto",
        headless: bool = True,
    ) -> Dict[str, Any]:
        if scenarios is None:
            p = default_scenarios_path()
            if p.is_file():
                cfg = json.loads(p.read_text(encoding="utf-8"))
                scenarios = cfg.get("scenarios") or []
            else:
                scenarios = []
        if not scenarios:
            return {"ok": False, "error": "No hay escenarios configurados."}

        course_ids = []
        for s in scenarios:
            if not s.get("enabled", True):
                continue
            for r in (s.get("courses") or []):
                cid = str(r.get("idMateria", "")).upper()
                if cid and cid not in course_ids:
                    course_ids.append(cid)

        self.get_groups(course_ids, transport=transport, headless=headless)
        decision = evaluate_scenarios(scenarios, self.groups_cache, [], enable_plan_z=enable_plan_z)
        if not decision["hasViableScenario"]:
            return {"ok": False, "dryRun": dry_run, "decision": decision}

        results = []
        enrolled_ok = 0
        for st in decision["selected"]["steps"]:
            res = self.enroll_course(
                st["idMateria"],
                st["chosenGroup"],
                dry_run=dry_run,
                allow_other_campus=st.get("allowOtherCampus", False),
                coreq_materia=st.get("coreqMateria", ""),
                coreq_group=(st.get("coreqPreferredGroups") or [0])[0],
                transport=transport,
                headless=headless,
            )
            # Backup group retry within same rule if live & failed
            if not res.get("ok") and not dry_run:
                for bg in st.get("backupGroups") or []:
                    res = self.enroll_course(
                        st["idMateria"], bg, dry_run=False,
                        allow_other_campus=st.get("allowOtherCampus", False),
                        transport=transport, headless=headless,
                    )
                    if res.get("ok"):
                        break
            if res.get("ok"):
                enrolled_ok += 1
            results.append(res)

        informe_res = None
        if send_informe_on_success and enrolled_ok > 0:
            informe_res = self.send_informe(dry_run=dry_run, transport=transport, headless=headless)

        # Auto re-lock after live scenario batch
        if not dry_run:
            self.arm_live(False)

        return {
            "ok": enrolled_ok > 0,
            "dryRun": dry_run,
            "selectedScenario": decision["selected"]["scenario"],
            "enrolledCount": enrolled_ok,
            "results": results,
            "informe": informe_res,
        }

    # --- Background Seat Sniper (Watches entire enrollment period for dropped seats!) ---
    def start_watch_and_snipe(
        self,
        targets: List[Dict[str, Any]],
        interval_sec: float = 35.0,
        max_duration_minutes: float = 120.0,
        auto_enroll: bool = False,
        send_informe_on_snipe: bool = True,
        auto_relogin_from_vault: bool = True,
        vault_path: Optional[Path] = None,
    ) -> Dict[str, Any]:
        if not targets:
            return {"ok": False, "error": "Debe indicar al menos un curso objetivo en targets."}
        if auto_enroll and not self.armed:
            return {
                "ok": False,
                "error": f"auto_enroll=True bloqueado: arme primero con tec_matricula_arm_live('{CONFIRMATION_PHRASE}').",
            }
        self.stop_watch_and_snipe()
        min_iv = 0.08 if self._is_loopback() else 30.0
        eff_interval = max(min_iv, float(interval_sec))
        deadline = time.monotonic() + max(0.05, float(max_duration_minutes)) * 60.0

        self._sniper_stop.clear()
        self._sniper_state = {
            "active": True,
            "targets": targets,
            "intervalSec": eff_interval,
            "autoEnroll": bool(auto_enroll),
            "polls": 0,
            "detected": [],
            "enrolled": [],
            "logs": [f"Seat Sniper iniciado ({len(targets)} curso(s), cada {eff_interval:.1f}s, autoEnroll={auto_enroll})."],
            "last_error": "",
        }

        def _loop() -> None:
            remaining = [dict(t) for t in targets]
            while not self._sniper_stop.is_set() and remaining and time.monotonic() < deadline:
                self._sniper_state["polls"] += 1
                next_remaining = []
                for rule in remaining:
                    if self._sniper_stop.is_set():
                        break
                    cid = str(rule.get("idMateria", "")).strip().upper()
                    ok, data, err = self._post_wm("frmMatricula.aspx/ConsultaHorarios", {"idMateria": cid})
                    if not ok and auto_relogin_from_vault and DpapiVault.exists(vault_path):
                        self._sniper_state["logs"].append(f"Sesión expirada consultando {cid}; re-autenticando desde bóveda DPAPI...")
                        self.login(use_vault=True, id_convocatoria=self.active_convocatoria, id_periodo=self.active_periodo, vault_path=vault_path)
                        ok, data, err = self._post_wm("frmMatricula.aspx/ConsultaHorarios", {"idMateria": cid})

                    if not ok or not isinstance(data, dict) or not isinstance(data.get("Horario"), list):
                        self._sniper_state["last_error"] = err
                        next_remaining.append(rule)
                        continue

                    live_groups = data["Horario"]
                    self.groups_cache[cid] = live_groups
                    cands = match_rule_groups(rule, live_groups, [])
                    if not cands:
                        next_remaining.append(rule)
                        continue

                    best_g = cands[0]["group"]
                    gid = int(best_g.get("IdGrupo") or 0)
                    seats = int(best_g.get("CupoDisponible") or 0)
                    self._sniper_state["detected"].append({"idMateria": cid, "idGrupo": gid, "cupo": seats})
                    self._sniper_state["logs"].append(f"CUPO DETECTADO: {cid} Gr {gid} ({seats} cupos).")

                    if auto_enroll and self.armed:
                        eres = self.enroll_course(
                            cid, gid, dry_run=False,
                            allow_other_campus=bool(rule.get("allowOtherCampus", False)),
                            coreq_materia=str(rule.get("coreqMateria") or ""),
                            coreq_group=int((rule.get("coreqPreferredGroups") or [0])[0]),
                        )
                        if eres.get("ok"):
                            self._sniper_state["enrolled"].append({"idMateria": cid, "idGrupo": gid})
                            self._sniper_state["logs"].append(f"MATRICULADO AUTOMATICAMENTE: {cid} Gr {gid}.")
                            if send_informe_on_snipe:
                                self.send_informe(dry_run=False)
                            continue  # Done with this course!
                        else:
                            self._sniper_state["logs"].append(f"Intento en {cid} Gr {gid} rechazado: {eres}")
                            next_remaining.append(rule)
                    else:
                        # Alert-only mode: record once and stop nagging for same course
                        continue

                remaining = next_remaining
                if remaining:
                    self._sniper_stop.wait(eff_interval)

            self._sniper_state["active"] = False
            if auto_enroll:
                self.arm_live(False)
            self._sniper_state["logs"].append("Seat Sniper finalizado.")

        self._sniper_thread = threading.Thread(target=_loop, daemon=True, name="TecSeatSniper")
        self._sniper_thread.start()
        return {"ok": True, "state": self._sniper_state}

    def watch_and_snipe_status(self) -> Dict[str, Any]:
        return dict(self._sniper_state)

    def stop_watch_and_snipe(self) -> Dict[str, Any]:
        self._sniper_stop.set()
        if self._sniper_thread and self._sniper_thread.is_alive():
            self._sniper_thread.join(timeout=3.0)
        self._sniper_state["active"] = False
        return {"ok": True, "stopped": True}

    def close(self) -> None:
        self.stop_watch_and_snipe()
        if self.scraper:
            self.scraper.stop()
            self.scraper = None


# ==============================================================================
# 4. FastMCP Server Wiring (14 Tools)
# ==============================================================================

def create_mcp_server():
    from mcp.server.fastmcp import FastMCP

    mcp = FastMCP("tec-matricula")
    engine = TecMatriculaEngine()

    @mcp.tool()
    def tec_matricula_status() -> Dict[str, Any]:
        """Estado actual del motor TEC Matrícula: sesión, reloj IIS, bóveda DPAPI, candado y Seat Sniper."""
        return {
            "authenticated": engine.authenticated,
            "armedForLiveMutation": engine.armed,
            "studentName": engine.student_name,
            "studentCarne": engine.student_carne,
            "activeConvocatoria": engine.active_convocatoria,
            "activePeriodo": engine.active_periodo,
            "serverClockRaw": engine.server_clock_raw,
            "clockOffsetMs": engine.server_clock_offset_ms,
            "dpapiVaultExists": DpapiVault.exists(),
            "scenariosFileExists": default_scenarios_path().is_file(),
            "sniper": engine.watch_and_snipe_status(),
        }

    @mcp.tool()
    def tec_matricula_vault_manage(action: str = "status", email: str = "", password: str = "") -> Dict[str, Any]:
        """Gestiona la bóveda cifrada Windows DPAPI compartida con CuadernU (action: status | save | delete)."""
        act = action.strip().lower()
        if act == "status":
            return {"ok": True, "exists": DpapiVault.exists(), "path": str(default_vault_path())}
        if act == "save":
            return DpapiVault.save(email, password)
        if act == "delete":
            return DpapiVault.delete()
        return {"ok": False, "error": "action debe ser status, save o delete."}

    @mcp.tool()
    def tec_matricula_login(
        use_vault: bool = True,
        email: str = "",
        password: str = "",
        id_convocatoria: int = 0,
        id_periodo: int = 0,
        transport: str = "auto",
        headless: bool = True,
    ) -> Dict[str, Any]:
        """Inicia sesión en el portal de Matrícula del TEC (usando bóveda DPAPI por defecto) y vincula convocatoria."""
        return engine.login(use_vault, email, password, id_convocatoria, id_periodo, transport, headless)

    @mcp.tool()
    def tec_matricula_check_cita_and_morosidad(auto_relogin_if_inactive: bool = False) -> Dict[str, Any]:
        """Consulta cita activa, detalle de cita, morosidad y horario matriculado actual (con refresh/re-login opcional)."""
        return engine.check_cita_and_morosidad(auto_relogin_if_inactive=auto_relogin_if_inactive)

    @mcp.tool()
    def tec_matricula_get_groups(course_ids: List[str], transport: str = "auto", headless: bool = True) -> Dict[str, Any]:
        """Consulta grupos y cupos reales en vivo (CupoDisponible/Capacidad/Reservado/Horario) con fallback scraping."""
        return engine.get_groups(course_ids, transport=transport, headless=headless)

    @mcp.tool()
    def tec_matricula_scenarios_manage(
        action: str = "read",
        scenarios_json: Optional[List[Dict[str, Any]]] = None,
        enable_plan_z: bool = False,
    ) -> Dict[str, Any]:
        """Lee, guarda o evalúa (dry_run_evaluate) los escenarios Plan A/B/C compartidos con CuadernU."""
        p = default_scenarios_path()
        act = action.strip().lower()
        if act == "read":
            if not p.is_file():
                return {"ok": True, "exists": False, "scenarios": []}
            return {"ok": True, "exists": True, "config": json.loads(p.read_text(encoding="utf-8"))}
        if act == "save":
            if scenarios_json is None:
                return {"ok": False, "error": "scenarios_json es requerido para save."}
            cfg = json.loads(p.read_text(encoding="utf-8")) if p.is_file() else {"version": 1}
            cfg["scenarios"] = scenarios_json
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")
            return {"ok": True, "savedPath": str(p), "count": len(scenarios_json)}
        if act == "dry_run_evaluate":
            scens = scenarios_json
            if scens is None and p.is_file():
                scens = json.loads(p.read_text(encoding="utf-8")).get("scenarios") or []
            return evaluate_scenarios(scens or [], engine.groups_cache, [], enable_plan_z=enable_plan_z)
        return {"ok": False, "error": "action debe ser read, save o dry_run_evaluate."}

    @mcp.tool()
    def tec_matricula_arm_live(armed: bool, confirmation_phrase: str = "") -> Dict[str, Any]:
        """Abre o cierra el candado duro de matrícula real. Requiere confirmation_phrase='CONFIRMAR MATRICULA REAL'."""
        return engine.arm_live(armed, confirmation_phrase)

    @mcp.tool()
    def tec_matricula_enroll_course(
        id_materia: str,
        id_grupo: int,
        dry_run: bool = True,
        allow_other_campus: bool = False,
        coreq_materia: str = "",
        coreq_group: int = 0,
        transport: str = "auto",
        headless: bool = True,
    ) -> Dict[str, Any]:
        """Matricula un curso y grupo específicos (por defecto dry_run=True; exige candado armado para matrícula real)."""
        return engine.enroll_course(
            id_materia, id_grupo, dry_run, allow_other_campus, coreq_materia, coreq_group, transport, headless
        )

    @mcp.tool()
    def tec_matricula_execute_scenarios(
        scenarios_json: Optional[List[Dict[str, Any]]] = None,
        dry_run: bool = True,
        enable_plan_z: bool = False,
        send_informe_on_success: bool = True,
        transport: str = "auto",
        headless: bool = True,
    ) -> Dict[str, Any]:
        """Evalúa y ejecuta el árbol Plan A -> B -> C (+ Plan Z y EnviaInforme oficial al terminar)."""
        return engine.execute_scenarios(scenarios_json, dry_run, enable_plan_z, send_informe_on_success, transport, headless)

    @mcp.tool()
    def tec_matricula_send_informe(dry_run: bool = True, transport: str = "auto", headless: bool = True) -> Dict[str, Any]:
        """Ejecuta 'Mi Informe -> Enviar' (GeneraInforme + EnviaInforme) para enviar el comprobante oficial al correo TEC."""
        return engine.send_informe(dry_run=dry_run, transport=transport, headless=headless)

    @mcp.tool()
    def tec_matricula_verify_enrolled() -> Dict[str, Any]:
        """Verifica directamente contra el servidor qué cursos y grupos quedaron matriculados en la BD del TEC."""
        return engine.verify_enrolled()

    @mcp.tool()
    def tec_matricula_watch_and_snipe_start(
        targets: List[Dict[str, Any]],
        interval_sec: float = 35.0,
        max_duration_minutes: float = 120.0,
        auto_enroll: bool = False,
        send_informe_on_snipe: bool = True,
    ) -> Dict[str, Any]:
        """Vigila en segundo plano durante el periodo de matrícula si se libera un cupo y matricula automáticamente al detectarlo."""
        return engine.start_watch_and_snipe(
            targets, interval_sec, max_duration_minutes, auto_enroll, send_informe_on_snipe
        )

    @mcp.tool()
    def tec_matricula_watch_and_snipe_status() -> Dict[str, Any]:
        """Consulta el estado, cupos detectados y cursos matriculados por el vigilante Seat Sniper."""
        return engine.watch_and_snipe_status()

    @mcp.tool()
    def tec_matricula_watch_and_snipe_stop() -> Dict[str, Any]:
        """Detiene el vigilante Seat Sniper en segundo plano."""
        return engine.stop_watch_and_snipe()

    return mcp


if __name__ == "__main__":
    create_mcp_server().run(transport="stdio")
