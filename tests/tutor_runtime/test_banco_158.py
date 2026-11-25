"""Suite exhaustiva de verificación del Tutor Audiovisual de Física General III.

Cubre:
- Las 166 lecciones verificadas (21 Recomendados E1..E21 + 137 Superexamen Q001..Q137 + 8 teoría T-*).
- Revisión de calidad DSL (0 faltas, cero LaTeX en voz/subtítulos, claves de opción única).
- Analizador TeX real en Node.js (assets/cuadernu/tutor_web/js/mate/tex.js) sobre 100 % de expresiones.
- Cotejo oficial TEC (coincidencias exactas y 6 errores de solucionario documentados honestamente).
- P0 #1 (Q049 = 121,6 °C), P0 #2 (_normalize_beat no inventa sympy_verified), P0 #4 (sin LaTeX en voz),
  P0 #12 (demo_lessons dice 56 J), P0 #14 (avisos de figura cambiada en Q038/Q048/Q083/Q091/Q095).
- Verificador numérico AST, reconocimiento Ctrl+V (/api/imagen/reconocer), herramientas de banco en voz viva.
- Seguridad: AST whitelist sin eval/parse_expr, rechazo de Origin/Host externos (403/4403), sin CDN,
  persistencia de sesión WS tras reconexión y guardado en cuaderno de voz en vivo (cuaderno.js).
"""

from __future__ import annotations

import ast
import base64
import json
import pathlib
import re
import subprocess
import sys
import unittest

from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.demo_lessons import get_lesson_beats
from tools.tutor_runtime.documentos import Documentos
from tools.tutor_runtime.fisica3 import registro, rutas
from tools.tutor_runtime.fisica3.dsl import revisar
from tools.tutor_runtime.gemini_engines import GeminiLiveVoiceBridge, _normalize_beat
from tools.tutor_runtime.server import create_app
from tools.tutor_runtime.sympy_verifier import safe_parse_math
from tools.tutor_runtime.verificador_numerico import (
    evaluar_expresion_segura,
    limpiar_voz_latex,
    verificar_ecuacion_tex,
)
from tools.tutor_runtime.vision import emparejar_texto_con_banco, reconocer_imagen

NODE_EXE = pathlib.Path(r"D:\DevTools\node\node.exe")


def _node_cmd() -> str:
    return str(NODE_EXE) if NODE_EXE.is_file() else "node"


class TestBanco158YTeoria(unittest.TestCase):
    def test_todas_las_166_lecciones_existen_y_pasan_revision_dsl(self) -> None:
        ids = set(registro.ids())
        esperados_e = {f"E{i}" for i in range(1, 22)}
        esperados_q = {f"Q{i:03d}" for i in range(1, 138)}
        esperados_t = {"T-DIL", "T-CAL", "T-CON", "T-RAD", "T-GAS", "T-PV", "T-MAQ", "T-ENT"}

        self.assertTrue(esperados_e.issubset(ids), f"Faltan E: {sorted(esperados_e - ids)}")
        self.assertTrue(esperados_q.issubset(ids), f"Faltan Q: {sorted(esperados_q - ids)}")
        self.assertTrue(esperados_t.issubset(ids), f"Faltan T: {sorted(esperados_t - ids)}")
        self.assertEqual(len(ids), 166)

        faltas = {}
        for lid in sorted(ids):
            lec = registro.obtener(lid)
            f = revisar(lec)
            if f:
                faltas[lid] = f
        self.assertEqual(faltas, {}, f"Faltas de calidad en el banco: {faltas}")

    def test_ruta_del_examen_resuelve_todas_las_lecciones(self) -> None:
        pub = rutas.publicas()
        self.assertGreaterEqual(len(pub), 1)
        for ruta in pub:
            for item in ruta.get("items", []):
                lid = item["id"] if isinstance(item, dict) else str(item)
                self.assertTrue(registro.existe(lid), f"Ruta apunta a lección inexistente: {lid}")

    def test_p0_q049_exacto_121_6_y_avisos_p0_14_y_entropia(self) -> None:
        q49 = registro.obtener("Q049")
        resps = " ".join(r.get("tex", "") for r in q49.respuestas)
        self.assertIn("121,6", resps.replace("{,}", ","))
        self.assertNotIn("121,4", resps.replace("{,}", ","))

        # P0 #14: Las 5 figuras cambiadas del Superexamen deben llevar aviso explícito
        for qid in ("Q038", "Q048", "Q083", "Q091", "Q095"):
            lec = registro.obtener(qid)
            self.assertTrue(lec.aviso and len(lec.aviso) > 15, f"{qid} debe llevar aviso de figura cambiada")

        # Entropía (Q130..Q137): aviso explícito sobre formulario II-2026
        for i in range(130, 138):
            lec = registro.obtener(f"Q{i:03d}")
            self.assertTrue(lec.aviso, f"Q{i:03d} debe advertir sobre el formulario II-2026")

    def test_cotejo_oficial_honesto(self) -> None:
        coincide = 0
        difiere = []
        for lid in registro.ids():
            c = registro.obtener(lid).cotejo()
            if not c:
                continue
            if c.get("estado") == "coincide":
                coincide += 1
            elif c.get("estado") == "difiere":
                self.assertTrue(c.get("texto") and len(c["texto"]) > 20, f"{lid} difiere sin motivo explícito")
                difiere.append(lid)
        self.assertGreaterEqual(coincide, 110)
        for conocido in ("Q045", "Q050", "Q060", "Q061", "Q099", "Q116"):
            self.assertIn(conocido, difiere)

    def test_parser_tex_real_nodejs_cero_comandos_desconocidos(self) -> None:
        """P0 #3: Ejecuta analizarTex() real de js/mate/tex.js sobre TODAS las fórmulas de las 166 lecciones."""
        exprs = []
        for lid in registro.ids():
            j = registro.obtener(lid).a_json()
            for txt in (j["enunciado"], j["titulo"], j.get("aviso") or ""):
                for m in re.finditer(r"\$([^$]+)\$", txt or ""):
                    exprs.append((lid, m.group(1)))
            for f in j.get("formulas") or []:
                exprs.append((lid, f))
            for r in j.get("respuestas") or []:
                if r.get("tex"):
                    exprs.append((lid, r["tex"]))
            for p in j["pasos"]:
                for tr in p["tramos"]:
                    for a in tr["acc"]:
                        if a.get("a") == "mate" and a.get("tex"):
                            exprs.append((lid, a["tex"]))
                        elif a.get("a") == "texto" and a.get("txt"):
                            for m in re.finditer(r"\$([^$]+)\$", a["txt"]):
                                exprs.append((lid, m.group(1)))
                for o in p.get("origen") or []:
                    for m in re.finditer(r"\$([^$]+)\$", o.get("que", "")):
                        exprs.append((lid, m.group(1)))
                for ln in p.get("lento") or []:
                    if ln.get("tex"):
                        exprs.append((lid, ln["tex"]))

        self.assertGreater(len(exprs), 2500)
        tex_js_uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "mate" / "tex.js").as_uri()
        js_code = (
            f"import {{ analizarTex }} from {json.dumps(tex_js_uri)};\n"
            "import fs from 'node:fs';\n"
            "const items = JSON.parse(fs.readFileSync(0, 'utf8'));\n"
            "const bad = [];\n"
            "for (const [id, tex] of items) {\n"
            "  const r = analizarTex(tex);\n"
            "  if (r.desconocidos && r.desconocidos.length) bad.push({ id, tex, unk: r.desconocidos });\n"
            "}\n"
            "console.log(JSON.stringify({ total: items.length, bad }));\n"
        )
        proc = subprocess.run(
            [_node_cmd(), "--input-type=module", "-e", js_code],
            input=json.dumps(exprs, ensure_ascii=False),
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=30,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        res = json.loads(proc.stdout)
        self.assertEqual(res["bad"], [], f"Comandos TeX desconocidos: {res['bad'][:5]}")


class TestVerificadorVisionYRegresionesP0(unittest.TestCase):
    def test_verificador_numerico_detecta_error_q049_y_rechaza_inyeccion(self) -> None:
        bad = verificar_ecuacion_tex(r"T_2 = 120 + 1.6 = 121.4\,^\circ\text{C}")
        self.assertFalse(bad["ok"])
        self.assertEqual(len(bad["discrepancias"]), 1)
        self.assertAlmostEqual(bad["discrepancias"][0]["calculado"], 121.6, places=2)

        ok = verificar_ecuacion_tex(r"T_2 = 120 + 1.6 = 121.6\,^\circ\text{C}")
        self.assertTrue(ok["ok"])
        self.assertEqual(ok["comprobaciones"], 1)

        # Inyección debe devolverse como None sin ejecutarse
        self.assertIsNone(evaluar_expresion_segura("__import__('os').system('echo pwn')"))
        self.assertIsNone(evaluar_expresion_segura("2**99999"))

    def test_p0_2_y_p0_4_normalize_beat_nunca_inventa_verificacion_ni_deja_latex(self) -> None:
        beat = _normalize_beat(
            {
                "speech_markup": r"Aquí <cue id='c1'/> calculamos $\dfrac{T_C}{T_H}$ y da $0.5$.",
                "verification": {"status": "sympy_verified", "sympy_rule": "inventado"},
                "actions": [],
            },
            "ses_1",
            "turn_1",
            1,
            1,
        )
        self.assertEqual(beat["verification"]["status"], "conceptual")
        self.assertIsNone(beat["verification"]["sympy_rule"])
        self.assertIn("<cue id='c1'/>", beat["speech_markup"])
        self.assertNotIn("\\dfrac", beat["speech_markup"])
        self.assertNotIn("$", beat["speech_markup"])

        # Limpieza general de voz
        self.assertNotIn("\\", limpiar_voz_latex(r"La eficiencia es $\eta = 1 - \frac{T_C}{T_H}$."))

    def test_p0_12_demo_lesson_1_dice_56_joules(self) -> None:
        beats = get_lesson_beats("fis3_trabajo_cero", "ses_demo")
        self.assertIn("cincuenta y seis joules", beats[0]["speech_markup"])
        self.assertNotIn("cuarenta joules", beats[0]["speech_markup"])

    def test_herramientas_banco_en_voz_viva(self) -> None:
        tools = GeminiLiveVoiceBridge._build_live_tools()
        names = {fn.name for t in tools for fn in (t.function_declarations or [])}
        self.assertIn("buscar_leccion_banco", names)
        self.assertIn("abrir_leccion_banco", names)

    def test_vision_empareja_texto_e_imagen_real_api(self) -> None:
        m_q = emparejar_texto_con_banco("Q049 Dos reservorios térmicos 200 °C y 25 °C barra piramidal aluminio")
        self.assertEqual(m_q["id"], "Q049")
        self.assertTrue(m_q["seguro"])

        docs = Documentos(REPO_ROOT, cache=REPO_ROOT / "private_local" / "tutor_recortes")
        if "superexamen" in docs.rutas():
            png_bytes = docs.png("superexamen", "Q049")
            app = create_app({"session_token": ""})
            client = TestClient(app)
            r = client.post(
                "/api/imagen/reconocer",
                json={"imagen": base64.b64encode(png_bytes).decode("ascii"), "tipo": "image/png"},
            )
            self.assertEqual(r.status_code, 200)
            body = r.json()
            self.assertEqual(body["id"], "Q049")
            self.assertTrue(body["seguro"])

            # También verificar segmentación P0 #13 en Recomendados (5, 8, 16)
            if "recomendados" in docs.rutas():
                p5 = docs.problema("recomendados", "5")
                p8 = docs.problema("recomendados", "8")
                p16 = docs.problema("recomendados", "16")
                self.assertEqual(len(p5.tiras), 1)
                self.assertLess(p5.tiras[0][1].height, 360)  # no pega la página entera
                self.assertGreaterEqual(len(p8.tiras), 2)  # conserva figura en página siguiente
                self.assertGreaterEqual(len(p16.tiras), 2)


class TestSeguridadRobustezYCuaderno(unittest.TestCase):
    def test_sympy_verifier_ast_whitelist_sin_eval_ni_parse_expr(self) -> None:
        src = (REPO_ROOT / "tools" / "tutor_runtime" / "sympy_verifier.py").read_text(encoding="utf-8")
        tree = ast.parse(src)
        llamadas = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Call):
                if isinstance(node.func, ast.Name):
                    llamadas.add(node.func.id)
                elif isinstance(node.func, ast.Attribute):
                    llamadas.add(node.func.attr)
        self.assertNotIn("parse_expr", llamadas)
        self.assertNotIn("eval", llamadas)
        self.assertNotIn("exec", llamadas)

        # Expresiones legítimas con multiplicación implícita
        expr = safe_parse_math("2x + 3(x+1) - 1/r^2")
        self.assertIn("x", str(expr))

        # Payloads maliciosos rechazados estrictamente
        maliciosos = [
            "__import__('os').system('echo x')",
            "(1).__class__.__base__.__subclasses__()",
            "open('README.md').read()",
            "(lambda x: x)(5)",
            "[x for x in range(10)]",
            "2**9999",
        ]
        for payload in maliciosos:
            with self.assertRaises(ValueError, msg=f"No bloqueó: {payload}"):
                safe_parse_math(payload)

    def test_origin_host_y_cors_estrictos_en_http_y_ws(self) -> None:
        src = (REPO_ROOT / "tools" / "tutor_runtime" / "server.py").read_text(encoding="utf-8")
        self.assertNotIn('"*"', src.split("CORSMiddleware")[1].split(")")[0])

        app = create_app({"session_token": "tok_sec_test"})
        client = TestClient(app)

        # Origen externo -> 403
        r_evil = client.get("/api/health?token=tok_sec_test", headers={"Origin": "https://evil.example"})
        self.assertEqual(r_evil.status_code, 403)

        # Origin: null (iframe sandboxed) -> 403
        r_null = client.get("/api/health?token=tok_sec_test", headers={"Origin": "null"})
        self.assertEqual(r_null.status_code, 403)

        # Host externo (DNS rebinding) -> 403
        r_host = client.get("/api/health?token=tok_sec_test", headers={"Host": "evil.example:8765"})
        self.assertEqual(r_host.status_code, 403)

        # Loopback permitido -> 200
        r_ok = client.get("/api/health?token=tok_sec_test", headers={"Origin": "http://127.0.0.1:8765"})
        self.assertEqual(r_ok.status_code, 200)

        # WebSocket con Origin externo -> rechazado (4403)
        with self.assertRaises(WebSocketDisconnect) as ctx_ws:
            with client.websocket_connect(
                "/ws/tutor?token=tok_sec_test",
                headers={"Origin": "https://evil.example"},
            ):
                pass
        self.assertEqual(ctx_ws.exception.code, 4403)

    def test_ws_sobrevive_a_reconexion_con_sesion(self) -> None:
        app = create_app({"session_token": ""})
        client = TestClient(app)
        sid = "ses_recon_test_42"

        with client.websocket_connect(f"/ws/tutor?sesion={sid}") as ws1:
            r1 = ws1.receive_json()
            self.assertEqual(r1["session_id"], sid)
            self.assertFalse(r1["resumed"])
            ws1.send_json({"type": "UPDATE_SETTINGS", "speech_enabled": False})
            _ = ws1.receive_json()
            ws1.send_json({"type": "LOAD_LESSON", "lesson_id": "fis3_trabajo_cero"})
            _ = ws1.receive_json()  # BOARD_RESET
            _ = ws1.receive_json()  # BEAT_PLAN
            _ = ws1.receive_json()  # AUDIO_ALIGNMENT

        # Reconectar con la misma sesión: debe recordar el estado
        with client.websocket_connect(f"/ws/tutor?sesion={sid}") as ws2:
            r2 = ws2.receive_json()
            self.assertEqual(r2["session_id"], sid)
            self.assertTrue(r2["resumed"])
            self.assertGreaterEqual(r2["scene_count"], 1)

    def test_cero_dependencias_cdn_en_tutor_web(self) -> None:
        web_dir = REPO_ROOT / "assets" / "cuadernu" / "tutor_web"
        cdn_re = re.compile(r"https?://(?:cdn|fonts\.googleapis|fonts\.gstatic|unpkg|jsdelivr|cdnjs|polyfill)", re.I)
        hallados = []
        for p in web_dir.rglob("*"):
            if p.is_file() and p.suffix.lower() in (".html", ".css", ".js"):
                txt = p.read_text(encoding="utf-8", errors="ignore")
                if cdn_re.search(txt):
                    hallados.append(str(p.relative_to(web_dir)))
        self.assertEqual(hallados, [], f"Referencias CDN detectadas: {hallados}")

    def test_cuaderno_js_convierte_voz_en_vivo_a_beats(self) -> None:
        cuad_uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "ui" / "cuaderno.js").as_uri()
        js_code = (
            f"import {{ vozVivaABeats }} from {json.dumps(cuad_uri)};\n"
            "const beats = vozVivaABeats([\n"
            "  { tipo: 'habla', quien: 'Tú', texto: '¿Cómo se calcula el calor?' },\n"
            "  { tipo: 'habla', quien: 'Profesor', texto: 'Usamos masa por calor específico por delta T.' },\n"
            "  { tipo: 'accion', acc: { a: 'mate', tex: 'Q = m c \\\\Delta T', color: 'azul' } },\n"
            "]);\n"
            "console.log(JSON.stringify(beats));\n"
        )
        proc = subprocess.run(
            [_node_cmd(), "--input-type=module", "-e", js_code],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=15,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        beats = json.loads(proc.stdout)
        self.assertGreaterEqual(len(beats), 1)
        self.assertEqual(beats[0]["protocol"], "cuadernu.tutor.v1.1")
        tipos = [a["type"] for b in beats for a in b["actions"]]
        self.assertIn("write_math", tipos)
        self.assertIn("write_text", tipos)


if __name__ == "__main__":
    unittest.main()
