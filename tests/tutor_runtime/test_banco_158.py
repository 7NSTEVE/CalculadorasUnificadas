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
from tools.tutor_runtime.fisica3 import registro, rutas, variantes
from tools.tutor_runtime.fisica3.dsl import revisar
from tools.tutor_runtime.gemini_engines import GeminiLiveVoiceBridge, _normalize_beat
from tools.tutor_runtime.server import create_app
from tools.tutor_runtime.sympy_verifier import safe_parse_math
from tools.tutor_runtime.verificador_fisico import auditar_leccion
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
        esperados_t = {"T-DIL", "T-CAL", "T-CON", "T-RAD", "T-GAS", "T-PV", "T-MAQ", "T-ENT", "T-MAT1", "T-MAT2", "T-MAT3", "T-CONO"}
        # Prácticas oficiales del II-2026: 6 de la primera, 14 de selección y 12 de desarrollo de la segunda
        esperados_p = ({f"PR1-{i}" for i in range(1, 7)} | {f"PR2-S{i}" for i in range(1, 15)}
                       | {f"PR2-D{i}" for i in range(1, 13)})
        # Blindaje II-2026 para las casillas nuevas del formulario (F1C3, F12C2, F5C3, F14C2)
        esperados_b = {"P26-1", "P26-2", "P26-3", "P26-4", "P26-5"}
        # Reposición Parcial 1 y Parcial 2 (7 ondas + 7 fluidos)
        esperados_rp = {
            "RP-23II-1", "RP-24I-1", "RP-24II-1", "RP-25II-1", "RP-26I-1", "RP-26I-2", "RP-26I-7",
            "RP-23II-2", "RP-24I-2", "RP-24II-2", "RP-25II-2", "RP-26I-3", "RP-26I-4", "RP-26I-8",
        }

        self.assertTrue(esperados_e.issubset(ids), f"Faltan E: {sorted(esperados_e - ids)}")
        self.assertTrue(esperados_q.issubset(ids), f"Faltan Q: {sorted(esperados_q - ids)}")
        self.assertTrue(esperados_t.issubset(ids), f"Faltan T: {sorted(esperados_t - ids)}")
        self.assertTrue(esperados_p.issubset(ids), f"Faltan prácticas: {sorted(esperados_p - ids)}")
        self.assertTrue(esperados_b.issubset(ids), f"Faltan blindaje II-2026: {sorted(esperados_b - ids)}")
        self.assertTrue(esperados_rp.issubset(ids), f"Faltan reposición P1/P2: {sorted(esperados_rp - ids)}")
        self.assertEqual(len(ids), 166 + 32 + 5 + 4 + 14)  # + T-MAT1..3, T-CONO; blindaje P26-1..5; reposición 14 RP-*

        faltas = {}
        for lid in sorted(ids):
            lec = registro.obtener(lid)
            f = revisar(lec)
            if f:
                faltas[lid] = f
        self.assertEqual(faltas, {}, f"Faltas de calidad en el banco: {faltas}")

    def test_ruta_del_examen_resuelve_todas_las_lecciones(self) -> None:
        pub = rutas.publicas()
        self.assertGreaterEqual(len(pub), 4)
        ids_rutas = {r["id"] for r in pub}
        self.assertIn("estrategia_ii_2026", ids_rutas)
        est = next(r for r in pub if r["id"] == "estrategia_ii_2026")
        est_ids = {it["id"] for it in est["items"]}
        for obligatorio in ("PR1-4", "PR2-D2", "PR2-D9", "P26-1", "P26-2", "P26-3", "PR2-S1", "PR2-S13"):
            self.assertIn(obligatorio, est_ids)
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


class TestAuditoriaFisicaVariantesY21PDFs(unittest.TestCase):
    def test_auditoria_fisica_10_niveles_banco_y_trampas_fisicas(self) -> None:
        # 1. Todo el banco de 166 lecciones debe pasar sin alertas físicas
        con_alerta = {}
        for lid in registro.ids():
            aud = auditar_leccion(registro.obtener(lid).a_json())
            self.assertEqual(len(aud["niveles"]), 10)
            if aud["alertas"]:
                con_alerta[lid] = aud["alertas"]
        self.assertEqual(con_alerta, {}, f"Alertas físicas inesperadas en el banco: {con_alerta}")

        # 2. Trampa física nivel 6: usar W = P(V_f - V_i) en una expansión isotérmica
        falsa_iso = {
            "titulo": "Expansión isotérmica mal resuelta",
            "tema": "ciclos",
            "enunciado": "Un gas ideal experimenta una expansión isotérmica desde V_i hasta V_f.",
            "pasos": [
                {
                    "titulo": "Cálculo erróneo",
                    "habla": "Multiplicamos presión por cambio de volumen.",
                    "tramos": [{"v": "Trabajo", "acc": [{"a": "mate", "tex": r"W = P(V_f - V_i)"}]}],
                }
            ],
            "respuestas": [],
        }
        aud_iso = auditar_leccion(falsa_iso)
        self.assertTrue(any("Nivel 6" in a and "isotérmica" in a for a in aud_iso["alertas"]), aud_iso["alertas"])

        # 3. Trampa física nivel 5: °C dentro de T^4 en radiación
        falsa_rad = {
            "titulo": "Radiación en Celsius",
            "tema": "radiacion",
            "enunciado": "Calcule la potencia radiada a 100 °C.",
            "pasos": [{"titulo": "Paso 1", "habla": "Elevamos a la cuatro.", "tramos": [{"v": "P", "acc": [{"a": "mate", "tex": r"P = \sigma A (100^\circ C)^4"}]}]}],
            "respuestas": [],
        }
        aud_rad = auditar_leccion(falsa_rad)
        self.assertTrue(any("Nivel 5" in a for a in aud_rad["alertas"]), aud_rad["alertas"])

        # 4. Trampa física nivel 6: conducción con A(y) variable sin integral
        falsa_cond = {
            "titulo": "Barra piramidal sin integral",
            "tema": "conduccion",
            "enunciado": "Barra con área transversal variable A(y) = 0,2 - 0,1y entre 200 °C y 25 °C.",
            "pasos": [{"titulo": "Paso 1", "habla": "Usamos Fourier constante.", "tramos": [{"v": "H", "acc": [{"a": "mate", "tex": r"H = \frac{k A \Delta T}{L}"}]}]}],
            "respuestas": [],
        }
        aud_cond = auditar_leccion(falsa_cond)
        self.assertTrue(any("Nivel 6" in a for a in aud_cond["alertas"]), aud_cond["alertas"])

        # 5. Regla estricta del formulario oficial II-2026: rechazar atajos no impresos sin derivación
        falsa_rth = {
            "titulo": "Barras en serie con atajo no derivado",
            "tema": "conduccion",
            "tipo": "desarrollo",
            "enunciado": "Dos barras en serie entre 100 °C y 0 °C.",
            "pasos": [{"titulo": "Paso 1", "habla": "Usamos resistencia térmica.", "tramos": [{"v": "R", "acc": [{"a": "mate", "tex": r"R_{\text{th}} = \frac{L}{kA}"}]}]}],
            "respuestas": [],
        }
        aud_rth = auditar_leccion(falsa_rth)
        self.assertTrue(any("Formulario II-2026" in a for a in aud_rth["alertas"]), aud_rth["alertas"])

        falsa_cp = {
            "titulo": "Cp con atajo sin derivar",
            "tema": "maquinas",
            "tipo": "desarrollo",
            "enunciado": "Halle Cp a partir de gamma.",
            "pasos": [{"titulo": "Paso 1", "habla": "Aplicamos la fórmula directa.", "tramos": [{"v": "Cp", "acc": [{"a": "mate", "tex": r"C_p = \frac{\gamma R}{\gamma - 1}"}]}]}],
            "respuestas": [],
        }
        aud_cp = auditar_leccion(falsa_cp)
        self.assertTrue(any("Formulario II-2026" in a for a in aud_cp["alertas"]), aud_cp["alertas"])

    def test_variantes_parametricas_los_8_arquetipos_y_api(self) -> None:
        self.assertEqual(len(variantes.ARQUETIPOS), 15)
        exprs = []
        for arq in variantes.ARQUETIPOS:
            for sem in (1, 7, 42):
                lec = variantes.generar_variante(arq, semilla=sem)
                self.assertEqual(revisar(lec), [], f"Variante {arq} (semilla {sem}) falló dsl.revisar")
                j = lec.a_json()
                aud = auditar_leccion(j)
                self.assertEqual(aud["alertas"], [], f"Variante {arq} (semilla {sem}) tuvo alertas: {aud['alertas']}")
                for p in j["pasos"]:
                    for tr in p["tramos"]:
                        for a in tr["acc"]:
                            if a.get("a") == "mate" and a.get("tex"):
                                exprs.append((arq, a["tex"]))

        # Validar 100% del TeX generado en variantes con Node.js real
        tex_js_uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "mate" / "tex.js").as_uri()
        js_code = (
            f"import {{ analizarTex }} from {json.dumps(tex_js_uri)};\n"
            "import fs from 'node:fs';\n"
            "const items = JSON.parse(fs.readFileSync(0, 'utf8'));\n"
            "const bad = items.filter(([_, t]) => (analizarTex(t).desconocidos || []).length > 0);\n"
            "console.log(JSON.stringify({ total: items.length, bad }));\n"
        )
        proc = subprocess.run(
            [_node_cmd(), "--input-type=module", "-e", js_code],
            input=json.dumps(exprs, ensure_ascii=False),
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout)["bad"], [])

        # Endpoint HTTP /api/banco/variante/Q049
        app = create_app({"session_token": ""})
        client = TestClient(app)
        rv = client.get("/api/banco/variante/Q049?semilla=7")
        self.assertEqual(rv.status_code, 200)
        body = rv.json()
        self.assertTrue(body.get("esVariante"))
        self.assertEqual(body["auditoria_fisica"]["alertas"], [])

    def test_documentos_21_pdfs_catalogo_y_recorte_extra(self) -> None:
        docs = Documentos(REPO_ROOT, cache=REPO_ROOT / "private_local" / "tutor_recortes")
        rutas_enc = docs.rutas()
        if "23-II-R" in rutas_enc and "26-F" in rutas_enc:
            # Programa FI2103 II-2026 registrado junto a los 17 exámenes + folleto + superexamen + recomendados + formulario
            self.assertGreaterEqual(len(rutas_enc), 21)
            self.assertIn("programa", rutas_enc)
            # Problema extra de examen de reposición (no está en el banco de 158) -> recorte PNG limpio
            r_rep = docs.resolver("problema 2 de reposicion II-2023")
            self.assertIsNotNone(r_rep)
            self.assertEqual(r_rep["doc"], "23-II-R")
            self.assertTrue(docs.png("23-II-R", "2").startswith(b"\x89PNG"))
            # Problema extra del folleto
            r_fol = docs.resolver("folleto 3.4-5")
            self.assertIsNotNone(r_fol)
            self.assertTrue(docs.png("26-F", r_fol["clave"]).startswith(b"\x89PNG"))

    def test_practicas_ii_2026_segmentadas_y_enlazadas(self) -> None:
        docs = Documentos(REPO_ROOT, cache=REPO_ROOT / "private_local" / "tutor_recortes")
        rutas_enc = docs.rutas()
        if "26-II-P1" not in rutas_enc or "26-II-P2" not in rutas_enc:
            self.skipTest("Las prácticas del II-2026 no están en esta máquina")
        p1 = [p.clave for p in docs.problemas("26-II-P1")]
        p2 = [p.clave for p in docs.problemas("26-II-P2")]
        self.assertEqual(p1, [str(i) for i in range(1, 7)])
        self.assertEqual(p2, [f"S{i}" for i in range(1, 15)] + [f"D{i}" for i in range(1, 13)])
        for doc, claves, pre in (("26-II-P1", p1, "PR1"), ("26-II-P2", p2, "PR2")):
            for c in claves:
                self.assertEqual(docs.leccion_de(doc, c), f"{pre}-{c}")
        self.assertTrue(docs.png("26-II-P2", "D10").startswith(b"\x89PNG"))
        self.assertEqual(docs.resolver("práctica 2 desarrollo 4")["clave"], "D4")
        self.assertEqual(docs.resolver("PR1-3")["doc"], "26-II-P1")

    def test_voz_edge_neural_mapa_y_audio_mp3(self) -> None:
        from tools.tutor_runtime.voz import VOCES_NEURALES, _mapa_desde_palabras, normalizar_voz

        self.assertEqual(normalizar_voz("Kore"), "es-MX-DaliaNeural")
        self.assertEqual(normalizar_voz("Puck"), "es-CR-JuanNeural")
        self.assertEqual(normalizar_voz("es-CR-MariaNeural"), "es-CR-MariaNeural")
        self.assertGreaterEqual(len(VOCES_NEURALES), 8)

        # Mapa tiempo->carácter estrictamente monótono a partir de WordBoundary
        txt = "Separamos variables e integramos desde cero hasta la longitud total."
        palabras = [
            (100, 350, "Separamos"),
            (480, 420, "variables"),
            (920, 80, "e"),
            (1020, 480, "integramos"),
            (1520, 260, "desde"),
            (1800, 240, "cero"),
            (2060, 260, "hasta"),
            (2340, 120, "la"),
            (2480, 410, "longitud"),
            (2910, 360, "total"),
        ]
        al = _mapa_desde_palabras(txt, palabras, [0, 22, 46], bytes_mp3=22000)
        mapa = al["mapa"]
        self.assertGreaterEqual(len(mapa), 10)
        for i in range(1, len(mapa)):
            self.assertGreater(mapa[i][0], mapa[i - 1][0])
            self.assertGreaterEqual(mapa[i][1], mapa[i - 1][1])
        self.assertEqual(mapa[-1][1], len(txt))
        self.assertEqual(len(al["cortes_ms"]), 3)
        self.assertLess(al["cortes_ms"][0], al["cortes_ms"][1])
        self.assertLess(al["cortes_ms"][1], al["cortes_ms"][2])

        # Endpoint /api/voz + /api/voz/audio/{clave} con Edge Neural TTS real y caché en disco
        app = create_app({"session_token": ""})
        client = TestClient(app)
        st = client.get("/api/voz/estado")
        self.assertEqual(st.status_code, 200)
        self.assertEqual(st.json().get("motor_primario"), "edge-tts")

        r1 = client.post("/api/voz", json={"texto": "Prueba de voz natural en física.", "cortes": [0, 15], "voz": "es-MX-DaliaNeural", "urgente": True})
        self.assertEqual(r1.status_code, 200)
        b1 = r1.json()
        self.assertTrue(str(b1.get("motor") or "").startswith("edge:"), b1)
        self.assertEqual(b1.get("mime"), "audio/mpeg")
        self.assertTrue(b1.get("clave"))
        self.assertGreaterEqual(len(b1.get("mapa") or []), 4)

        ra = client.get(f"/api/voz/audio/{b1['clave']}")
        self.assertEqual(ra.status_code, 200)
        self.assertEqual(ra.headers.get("content-type"), "audio/mpeg")
        self.assertGreater(len(ra.content), 2000)

        # Segunda llamada idéntica: golpe de caché inmediato
        r2 = client.post("/api/voz", json={"texto": "Prueba de voz natural en física.", "cortes": [0, 15], "voz": "es-MX-DaliaNeural", "urgente": True})
        self.assertEqual(r2.status_code, 200)
        self.assertTrue(r2.json().get("cache"))

    def test_normalizador_voz_y_motores_locales_intercambiables(self) -> None:
        import asyncio
        import http.server
        import io
        import math
        import struct
        import threading
        import wave
        from tools.tutor_runtime.normalizador_voz import detectar_estilo_pedagogico, normalizar_para_voz
        from tools.tutor_runtime.voz import ServicioVoz, validar_url_loopback

        # 1. Normalizador matemático y de unidades del SI independiente del motor
        n1 = normalizar_para_voz(r"$Q = n C_V (T_f - T_i)$ da 15,40 kW a 121,6 °C y 3,6×10^6 Pa con 28,97 J/(mol·K).")
        self.assertIn("kilovatios", n1)
        self.assertIn("grados Celsius", n1)
        self.assertIn("por diez a la 6", n1)
        self.assertIn("pascales", n1)
        self.assertIn("joules por mol kelvin", n1)
        self.assertNotIn("$", n1)
        self.assertNotIn("\\", n1)

        # Preserva prosa natural sin dañarla
        prosa = "Separamos variables e integramos desde cero hasta la longitud total."
        self.assertEqual(normalizar_para_voz(prosa), prosa)

        # 2. Detección automática de prosodia pedagógica
        self.assertEqual(detectar_estilo_pedagogico("Ojo: aquí hay una trampa común con los grados."), "alerta")
        self.assertEqual(detectar_estilo_pedagogico("Hagamos el cambio de variable paso a paso."), "despacio")
        self.assertEqual(detectar_estilo_pedagogico("Calculamos el volumen del cilindro."), "normal")
        self.assertEqual(detectar_estilo_pedagogico("Texto cualquiera.", "enfasis"), "enfasis")

        # 3. Guardia SSRF: rechaza URLs externas no-loopback
        self.assertEqual(validar_url_loopback("http://127.0.0.1:8880/v1/audio/speech"), "http://127.0.0.1:8880/v1/audio/speech")
        with self.assertRaises(ValueError):
            validar_url_loopback("https://evil.example.com/v1/audio/speech")

        # 4. Servidor local OpenAI-compatible simulado (ej. ZONOS3-Mini / VoxCPM2 / Qwen3-TTS Vulkan)
        buf = io.BytesIO()
        sr = 24000
        with wave.open(buf, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sr)
            frames = bytearray()
            for i in range(int(sr * 1.1)):
                env = 12000 if (i // 4000) % 2 == 0 else 400
                s = int(env * math.sin(2 * math.pi * 220 * i / sr))
                frames.extend(struct.pack("<h", s))
            wf.writeframes(bytes(frames))
        wav_bytes = buf.getvalue()
        recibidos = []

        class _FakeLocalTts(http.server.BaseHTTPRequestHandler):
            def do_POST(self) -> None:
                n = int(self.headers.get("Content-Length") or "0")
                recibidos.append(json.loads(self.rfile.read(n).decode("utf-8")))
                self.send_response(200)
                self.send_header("Content-Type", "audio/wav")
                self.send_header("Content-Length", str(len(wav_bytes)))
                self.end_headers()
                self.wfile.write(wav_bytes)

            def log_message(self, fmt: str, *args: object) -> None:
                return

        srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _FakeLocalTts)
        port = srv.server_address[1]
        th = threading.Thread(target=srv.serve_forever, daemon=True)
        th.start()
        try:
            url = f"http://127.0.0.1:{port}/v1/audio/speech"
            sv = ServicioVoz(api_key="", carpeta=None, local_tts_url=url)
            res = asyncio.run(sv.pedir("Ojo: $Q = 15,40$ kW a 121,6 °C.", [0, 12], voz="profesor_cr", motor="local_http"))
            self.assertTrue(str(res.get("motor") or "").startswith("local_http:"), res)
            self.assertEqual(res.get("mime"), "audio/wav")
            self.assertEqual(res.get("estilo"), "alerta")
            self.assertTrue(res.get("clave"))
            self.assertGreaterEqual(len(res.get("mapa") or []), 2)
            # Verifica que el texto llegó ya normalizado al modelo local
            self.assertEqual(len(recibidos), 1)
            self.assertIn("kilovatios", recibidos[0]["input"])
            self.assertIn("grados Celsius", recibidos[0]["input"])
            self.assertAlmostEqual(recibidos[0]["speed"], 0.94, places=2)
            # Segunda llamada -> golpe de caché en memoria sin repetir POST
            res2 = asyncio.run(sv.pedir("Ojo: $Q = 15,40$ kW a 121,6 °C.", [0, 12], voz="profesor_cr", motor="local_http"))
            self.assertTrue(res2.get("cache"))
            self.assertEqual(len(recibidos), 1)
        finally:
            srv.shutdown()
            srv.server_close()

    def test_formulario_ii_2026_mapa_166_lecciones_variantes_y_hoja_png(self) -> None:
        from tools.tutor_runtime.fisica3 import variantes
        from tools.tutor_runtime.fisica3.formulario_ii_2026 import (
            CAJAS_CELDAS,
            COLORES_FORMULARIO,
            catalogo_formulario,
            mapear_formulario_leccion,
            verificar_origen_formulario,
        )
        from tools.tutor_runtime.fisica3.formularios_p1_p2 import (
            catalogo_formulario_parcial,
            mapear_formulario_por_parcial,
        )
        from tools.tutor_runtime.verificador_fisico import auditar_leccion

        # 1. Catálogo oficial exacto de 42 celdas (14×3) con geometría normalizada
        cat = catalogo_formulario()
        self.assertEqual(cat["total"], 42)
        self.assertEqual(len(cat["celdas"]), 42)
        for c in cat["celdas"]:
            x0, y0, x1, y1 = c["caja"]
            self.assertTrue(0.0 <= x0 < x1 <= 1.0 and 0.0 <= y0 < y1 <= 1.0, c)

        cajas_por_parcial = {
            1: {c["id"] for c in catalogo_formulario_parcial(1)["celdas"]},
            2: {c["id"] for c in catalogo_formulario_parcial(2)["celdas"]},
            3: set(CAJAS_CELDAS.keys()),
        }

        # 2. Auditoría estricta de todas las lecciones del banco (0 violaciones, 0 alertas físicas, 42/42 casillas P3)
        desarrollos = 0
        cobertura: dict[str, list[str]] = {c["id"]: [] for c in cat["celdas"]}
        cobertura_des: dict[str, list[str]] = {c["id"]: [] for c in cat["celdas"]}
        for lid in registro.ids():
            j = registro.obtener(lid).a_json()
            parcial = int(j.get("parcial") or 3)
            aud = auditar_leccion(j)
            self.assertEqual(aud["alertas"], [], f"{lid} tiene alertas físicas: {aud['alertas']}")
            if parcial in (1, 2):
                mapa = mapear_formulario_por_parcial(j)
                self.assertEqual(len(mapa["por_paso"]), len(j.get("pasos") or []), lid)
                self.assertGreaterEqual(len(mapa["celdas"]), 1, f"{lid} (P{parcial}) sin celdas")
                for c in mapa["celdas"]:
                    self.assertIn(c["id"], cajas_por_parcial[parcial])
                    self.assertIn(c["color"], COLORES_FORMULARIO)
                if j.get("tipo") not in ("seleccion", "teoria"):
                    desarrollos += 1
                continue
            self.assertEqual(verificar_origen_formulario(j), [], f"{lid} usa atajo sin derivar")
            mapa = mapear_formulario_leccion(j)
            self.assertEqual(len(mapa["por_paso"]), len(j.get("pasos") or []), lid)
            cids_lec = {c["id"] for c in mapa["celdas"]}
            for c in mapa["celdas"]:
                self.assertIn(c["id"], CAJAS_CELDAS)
                self.assertIn(c["color"], COLORES_FORMULARIO)
                cobertura[c["id"]].append(lid)
                if j.get("tipo") not in ("seleccion", "teoria"):
                    cobertura_des[c["id"]].append(lid)
            if j.get("tipo") not in ("seleccion", "teoria"):
                desarrollos += 1
                self.assertGreaterEqual(len(mapa["celdas"]), 1, f"Desarrollo {lid} sin celdas mapeadas")
            # Cero falsos positivos F13C1 (Boltzmann) en conducción
            if registro.grupo(j.get("tema") or "") == "conduccion":
                self.assertNotIn("F13C1", cids_lec, f"Falso positivo Boltzmann F13C1 en conducción {lid}")
            # Cero falsos positivos F10C3 (conversión a Kelvin) sin conversión real 273 / T_K = T
            if "F10C3" in cids_lec:
                blob_k = json.dumps(j.get("pasos") or [], ensure_ascii=False) + " " + " ".join(j.get("formulas") or [])
                self.assertRegex(
                    blob_k,
                    r"273|T_K\s*=\s*T",
                    f"Falso positivo F10C3 (conversión a Kelvin) en {lid}",
                )
        self.assertGreaterEqual(desarrollos, 100)
        huerfanas = [cid for cid, ls in cobertura.items() if not ls]
        self.assertEqual(huerfanas, [], f"Casillas del formulario II-2026 sin cobertura: {huerfanas}")
        for cid_nuevo in ("F1C3", "F12C2", "F5C3", "F14C2"):
            self.assertGreaterEqual(len(cobertura_des[cid_nuevo]), 1, f"Casilla {cid_nuevo} sin desarrollo")

        # 3. Los 15 arquetipos paramétricos × 3 semillas = 45 variantes limpias
        for arq in variantes.ARQUETIPOS:
            for sem in (101, 202, 303):
                j = variantes.generar_variante(arq, semilla=sem).a_json()
                self.assertEqual(verificar_origen_formulario(j), [], f"Variante {arq}/{sem}")
                aud = auditar_leccion(j)
                self.assertEqual(aud["alertas"], [], f"Variante {arq}/{sem}: {aud['alertas']}")
                mapa = mapear_formulario_leccion(j)
                self.assertGreaterEqual(len(mapa["celdas"]), 1, f"Variante {arq}/{sem} sin celdas")

        # 4. Prueba negativa: rechaza atajos sin ecuación base
        trampa = {
            "id": "FAKE1",
            "tipo": "desarrollo",
            "pasos": [
                {
                    "titulo": "Atajo prohibido",
                    "tramos": [
                        {
                            "v": "Usamos C_p directo.",
                            "acc": [
                                {"a": "mate", "tex": r"C_p = \frac{\gamma R}{\gamma - 1}"},
                                {"a": "mate", "tex": r"R_{th} = \frac{L}{kA}"},
                            ],
                        }
                    ],
                }
            ],
        }
        faltas_neg = verificar_origen_formulario(trampa)
        self.assertGreaterEqual(len(faltas_neg), 2, faltas_neg)

        # 5. Endpoints HTTP: /api/formulario, /api/formulario/hoja.png y formulario_mapa en /api/banco/{id}
        app = create_app({"session_token": ""})
        client = TestClient(app)

        rf = client.get("/api/formulario")
        self.assertEqual(rf.status_code, 200)
        self.assertEqual(rf.json()["total"], 42)

        rp = client.get("/api/formulario/hoja.png")
        self.assertEqual(rp.status_code, 200)
        self.assertEqual(rp.headers.get("content-type"), "image/png")
        self.assertTrue(rp.content.startswith(b"\x89PNG\r\n\x1a\n"))
        self.assertGreater(len(rp.content), 10000)

        for qid, esperadas in (
            ("Q049", {"F2C2", "F2C1"}),
            ("Q122", {"F4C1", "F7C3", "F8C3", "F9C2", "F10C2"}),
            ("Q014", {"F1C1", "F1C2"}),
            ("PR2-D3", {"F2C1"}),
            ("PR2-D6", {"F4C1", "F8C2"}),
            ("PR2-D9", {"F8C2", "F11C3", "F12C1", "F14C2"}),
            ("P26-1", {"F1C1", "F1C2", "F1C3"}),
            ("P26-2", {"F12C2", "F10C3", "F3C2"}),
            ("P26-3", {"F4C1", "F4C2", "F4C3", "F5C1", "F5C2", "F5C3", "F13C1", "F13C2", "F14C1", "F14C2"}),
        ):
            r = client.get(f"/api/banco/{qid}")
            self.assertEqual(r.status_code, 200)
            fm = r.json().get("formulario_mapa") or {}
            ids_hallados = {c["id"] for c in fm.get("celdas") or []}
            self.assertTrue(esperadas.issubset(ids_hallados), f"{qid}: faltan {esperadas - ids_hallados} en {ids_hallados}")
            if qid == "PR2-D3":
                self.assertNotIn("F13C1", ids_hallados)

    def test_simulacro_js_filtra_corte_ii_2026_sin_entropia_carnot_ni_vdw(self) -> None:
        sim_src = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "ui" / "simulacro.js").read_text(encoding="utf-8")
        # Verifica la función pura esDelCorteII2026 sin importar dependencias DOM
        self.assertIn("FUERA_II_2026", sim_src)
        for fuera in ("Q071", "Q072", "Q073", "Q118", "Q125"):
            self.assertIn(f"'{fuera}'", sim_src)
        self.assertIn("tema !== 'entropia'", sim_src)
        self.assertIn("Simulacro predictivo II-2026", sim_src)
        self.assertIn("Incluir histórico completo", sim_src)

    def test_guia_estudio_formulario_42_celdas_patrones_y_api(self) -> None:
        from tools.tutor_runtime.fisica3.formulario_ii_2026 import CAJAS_CELDAS
        from tools.tutor_runtime.fisica3.guia_formulario_ii_2026 import guia_completa

        g = guia_completa()
        self.assertEqual(g["total"], 42)
        self.assertEqual(len(g["celdas"]), 42)
        ids_guia = {c["id"] for c in g["celdas"]}
        self.assertEqual(ids_guia, set(CAJAS_CELDAS.keys()))

        lecciones_validas = set(registro.ids())
        exprs: list[tuple[str, str]] = []

        for c in g["celdas"]:
            cid = c["id"]
            self.assertTrue(c.get("que_es"), f"{cid} sin que_es")
            self.assertTrue(c.get("cuando_si"), f"{cid} sin cuando_si")
            self.assertTrue(c.get("cuando_no"), f"{cid} sin cuando_no")
            self.assertTrue(c.get("unidades"), f"{cid} sin unidades")
            self.assertTrue(c.get("errores"), f"{cid} sin errores")
            self.assertTrue(c.get("pistas_enunciado"), f"{cid} sin pistas_enunciado")
            pat = c.get("patron_examen") or {}
            self.assertTrue(pat.get("en_formularios"), f"{cid} sin en_formularios")
            self.assertTrue(pat.get("parte"), f"{cid} sin parte")
            self.assertTrue(pat.get("historial"), f"{cid} sin historial")
            self.assertTrue(c.get("simbolos"), f"{cid} sin simbolos")
            self.assertGreaterEqual(len(c.get("ejemplos") or []), 1, f"{cid} sin ejemplos en el banco")
            for ej in c["ejemplos"]:
                self.assertIn(ej["id"], lecciones_validas, f"{cid} refiere a lección inexistente {ej['id']}")
            for conf in c.get("confundir_con") or []:
                self.assertIn(conf["id"], CAJAS_CELDAS, f"{cid} refiere a gemela inexistente {conf['id']}")
            exprs.append((cid, c["tex"]))
            for d in c.get("se_deriva") or []:
                exprs.append((f"{cid}:deriv", d["tex"]))

        # Decisiones rápidas y entrenador apuntan únicamente a celdas y lecciones reales
        self.assertGreaterEqual(len(g["decisiones"]), 12)
        for d in g["decisiones"]:
            self.assertIn(d["leccion"], lecciones_validas, d)
            for cid in d["celdas"]:
                self.assertIn(cid, CAJAS_CELDAS, d)

        self.assertGreaterEqual(len(g["fuera_de_hoja"]), 4)
        self.assertGreaterEqual(len(g["entrenador"]), 30)
        for ent in g["entrenador"]:
            self.assertIn(ent["leccion"], lecciones_validas, ent)
            self.assertIn(ent["distractor"], CAJAS_CELDAS, ent)
            self.assertGreaterEqual(len(ent["celdas_validas"]), 1, ent)
            for cid in ent["celdas_validas"]:
                self.assertIn(cid, CAJAS_CELDAS, ent)

        # Validar 100 % del TeX (42 casillas + derivaciones) con analizarTex en Node.js
        tex_js_uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "mate" / "tex.js").as_uri()
        js_code = (
            f"import {{ analizarTex }} from {json.dumps(tex_js_uri)};\n"
            "import fs from 'node:fs';\n"
            "const items = JSON.parse(fs.readFileSync(0, 'utf8'));\n"
            "const bad = items\n"
            "  .map(([id, t]) => [id, t, analizarTex(t).desconocidos || []])\n"
            "  .filter(([, , d]) => d.length > 0);\n"
            "console.log(JSON.stringify({ total: items.length, bad }));\n"
        )
        proc = subprocess.run(
            [_node_cmd(), "--input-type=module", "-e", js_code],
            input=json.dumps(exprs, ensure_ascii=False),
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        res_tex = json.loads(proc.stdout)
        self.assertEqual(res_tex["bad"], [], f"TeX con comandos desconocidos: {res_tex['bad']}")

        # Endpoint HTTP /api/formulario/guia + validación de token/Origin
        app = create_app({"session_token": "tok-guia-test"})
        client = TestClient(app)

        r_ok = client.get("/api/formulario/guia?token=tok-guia-test", headers={"Origin": "http://127.0.0.1:8765"})
        self.assertEqual(r_ok.status_code, 200)
        body = r_ok.json()
        self.assertEqual(body["total"], 42)
        self.assertEqual(len(body["celdas"]), 42)

        # Pruebas negativas: sin token (401) y Origin externo (403)
        r_no_tok = client.get("/api/formulario/guia")
        self.assertEqual(r_no_tok.status_code, 401)
        r_bad_ori = client.get(
            "/api/formulario/guia?token=tok-guia-test",
            headers={"Origin": "https://evil.example"},
        )
        self.assertEqual(r_bad_ori.status_code, 403)

    def test_catalogo_modelos_gemini_4_y_failover_sin_legado_2_5(self) -> None:
        from tools.tutor_runtime.gemini_engines import (
            LIVE_AUDIO_MODEL_PRIMARY,
            TTS_MODEL_CHAIN,
            VISUAL_MODEL_CHAIN,
            VISUAL_MODEL_DEFAULT,
            normalizar_modelo_visual,
        )

        self.assertEqual(VISUAL_MODEL_DEFAULT, "gemini-4-hydrogen")
        self.assertEqual(VISUAL_MODEL_CHAIN[0], "gemini-4-hydrogen")
        for esperado in ("gemini-3.8-flash", "gemini-3.6-flash", "gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemma-4-31b-it"):
            self.assertIn(esperado, VISUAL_MODEL_CHAIN)
        for m in VISUAL_MODEL_CHAIN + TTS_MODEL_CHAIN:
            self.assertFalse(m.startswith("gemini-2.5"), f"Modelo legado 2.5 en cadena: {m}")
        self.assertEqual(LIVE_AUDIO_MODEL_PRIMARY, "gemini-3.8-live")

        # Migración automática de modelos obsoletos y rechazo de entradas inválidas
        self.assertEqual(normalizar_modelo_visual("gemini-2.5-flash"), "gemini-4-hydrogen")
        self.assertEqual(normalizar_modelo_visual("models/gemini-3.8-flash"), "gemini-3.8-flash")
        self.assertEqual(normalizar_modelo_visual("  "), "gemini-4-hydrogen")
        self.assertEqual(normalizar_modelo_visual("../../evil"), "gemini-4-hydrogen")

        app_js = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "app.js").read_text(encoding="utf-8")
        api_js = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "nucleo" / "api.js").read_text(encoding="utf-8")
        for esperado in ("gemini-4-hydrogen", "gemini-3.8-flash", "gemini-3.6-flash", "gemini-3.5-flash-lite", "gemma-4-31b-it", "gemini-4-carbon"):
            self.assertIn(esperado, app_js)
        self.assertIn("modelo: 'gemini-4-hydrogen'", api_js)
        self.assertNotIn("gemini-2.5-flash", app_js)

    def test_preparacion_extraordinario_42_reposicion_22_y_corte_20_5(self) -> None:
        from tools.tutor_runtime.fisica3.formularios_p1_p2 import (
            catalogo_formulario_parcial,
            mapear_formulario_por_parcial,
        )
        from tools.tutor_runtime.fisica3.preparacion_examenes import preparacion_completa
        from tools.tutor_runtime.verificador_fisico import auditar_leccion

        # 1. Catálogos de formularios P1 y P2 con cajas normalizadas válidas
        cat1 = catalogo_formulario_parcial(1)
        cat2 = catalogo_formulario_parcial(2)
        self.assertEqual(cat1["total"], 38)
        self.assertEqual(cat2["total"], 27)
        for cat in (cat1, cat2):
            for c in cat["celdas"]:
                x0, y0, x1, y1 = c["caja"]
                self.assertTrue(0.0 <= x0 < x1 <= 1.0 and 0.0 <= y0 < y1 <= 1.0, c)

        # 2. Estructura exacta Extraordinario (42 activos + 4 excluidos) y Reposición (22 = 7 P1 + 7 P2 + 8 P3)
        prep = preparacion_completa()
        extra = prep["extraordinario"]
        repos = prep["reposicion"]
        self.assertEqual(extra["total_activos"], 42)
        self.assertEqual(extra["total_excluidos"], 4)
        self.assertEqual(extra["total_historico"], 46)

        ids_extra = [it["id"] for b in extra["bloques"] for it in b["items"]]
        self.assertEqual(len(ids_extra), 42)
        self.assertEqual(len(set(ids_extra)), 42)
        excl_ids = {e["id"] for e in extra["excluidos_corte"]}
        self.assertEqual(excl_ids, {"Q132", "Q023", "Q130", "Q125"})
        self.assertTrue(excl_ids.isdisjoint(ids_extra))

        for b in extra["bloques"]:
            self.assertTrue(3 <= len(b["items"]) <= 5, f"Bloque {b['id']} fuera de tamaño pasos cortos: {len(b['items'])}")
            for it in b["items"]:
                self.assertTrue(it["meta_1_linea"])
                cal = it["calentamiento"]
                self.assertEqual(len(cal["opciones"]), 3)
                self.assertIn(cal["correcta"], (0, 1, 2))
                self.assertTrue(cal["explica"])

        ids_repos = [it["id"] for b in repos["bloques"] for it in b["items"]]
        self.assertEqual(repos["total"], 22)
        self.assertEqual(len(ids_repos), 22)
        self.assertEqual(len(set(ids_repos)), 22)
        por_parcial = {1: [], 2: [], 3: []}
        for b in repos["bloques"]:
            for it in b["items"]:
                por_parcial[it["parcial"]].append(it["id"])
        self.assertEqual(len(por_parcial[1]), 7)
        self.assertEqual(len(por_parcial[2]), 7)
        self.assertEqual(len(por_parcial[3]), 8)

        # 3. Las 14 lecciones nuevas RP-* pasan DSL, auditoría física, cotejo oficial, mapeo P1/P2 y TeX en Node.js
        rp_ids = por_parcial[1] + por_parcial[2]
        self.assertEqual(len(rp_ids), 14)
        exprs: list[tuple[str, str]] = []
        for lid in rp_ids:
            lec = registro.obtener(lid)
            self.assertEqual(revisar(lec), [], f"Faltas DSL en {lid}")
            j = lec.a_json()
            aud = auditar_leccion(j)
            self.assertEqual(aud["alertas"], [], f"Alertas físicas en {lid}: {aud['alertas']}")
            self.assertIsNotNone(j["cotejo"], f"{lid} sin cotejo oficial")
            self.assertEqual(j["cotejo"]["estado"], "coincide", f"{lid} difiere de la oficial: {j['cotejo']}")
            mapa = mapear_formulario_por_parcial(j)
            self.assertEqual(mapa["parcial"], j["parcial"])
            self.assertGreaterEqual(len([c for c in mapa["celdas"] if not c["es_constante"]]), 1, lid)
            for p in j["pasos"]:
                for tr in p["tramos"]:
                    for acc in tr["acc"]:
                        if acc.get("a") == "mate" and acc.get("tex"):
                            exprs.append((f"{lid}:p{p['n']}", acc["tex"]))
            for r_item in j["respuestas"]:
                if r_item.get("tex"):
                    exprs.append((f"{lid}:resp", r_item["tex"]))

        tex_js_uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "mate" / "tex.js").as_uri()
        js_code = (
            f"import {{ analizarTex }} from {json.dumps(tex_js_uri)};\n"
            "import fs from 'node:fs';\n"
            "const items = JSON.parse(fs.readFileSync(0, 'utf8'));\n"
            "const bad = items.map(([id, t]) => [id, t, analizarTex(t).desconocidos || []]).filter(([, , d]) => d.length > 0);\n"
            "console.log(JSON.stringify({ total: items.length, bad }));\n"
        )
        proc = subprocess.run(
            [_node_cmd(), "--input-type=module", "-e", js_code],
            input=json.dumps(exprs, ensure_ascii=False),
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=20,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        res_tex = json.loads(proc.stdout)
        self.assertEqual(res_tex["bad"], [], f"TeX desconocido en RP-*: {res_tex['bad']}")

        # 4. Endpoints HTTP: /api/preparacion, /api/banco/{id}?corte=20.5, /api/formulario/p1|p2/hoja.png + seguridad
        app = create_app({"session_token": "tok-prep-test"})
        client = TestClient(app)
        hdrs = {"Origin": "http://127.0.0.1:8765"}

        self.assertEqual(client.get("/api/preparacion").status_code, 401)
        self.assertEqual(client.get("/api/preparacion?token=tok-prep-test", headers={"Origin": "https://evil.example"}).status_code, 403)
        r_prep = client.get("/api/preparacion?token=tok-prep-test", headers=hdrs)
        self.assertEqual(r_prep.status_code, 200)
        self.assertEqual(r_prep.json()["extraordinario"]["total_activos"], 42)
        self.assertEqual(r_prep.json()["reposicion"]["total"], 22)

        # Corte quirúrgico ?corte=20.5 en los 5 problemas mixtos (Q092, Q097, Q128, Q108, Q127)
        for q_mix in ("Q092", "Q097", "Q128", "Q108"):
            r_cut = client.get(f"/api/banco/{q_mix}?corte=20.5&token=tok-prep-test", headers=hdrs)
            self.assertEqual(r_cut.status_code, 200)
            body = r_cut.json()
            self.assertEqual(body.get("corte_aplicado"), "20.5")
            self.assertTrue(body.get("inciso_omitido"))
            self.assertEqual((body.get("formulario_mapa") or {}).get("fuera_de_hoja"), [], f"{q_mix} aún tiene fuera_de_hoja")
            pasos_txt = " ".join((p.get("titulo") or "") + " " + (p.get("habla") or "") for p in body["pasos"])
            self.assertNotRegex(pasos_txt, r"Carnot|entrop[íi]a|\\Delta\s*S", f"{q_mix} filtró incompleto")
            resp_txt = " ".join(str(x.get("tex") or "") for x in body["respuestas"])
            self.assertNotRegex(resp_txt, r"Carnot|\\Delta\s*S", f"{q_mix} mantuvo respuesta > 20.5")
            self.assertGreaterEqual(len(body["pasos"]), 8, f"{q_mix} perdió pasos <= 20.5")

        r_127 = client.get("/api/banco/Q127?corte=20.5&token=tok-prep-test", headers=hdrs)
        self.assertEqual(r_127.status_code, 200)
        b127 = r_127.json()
        self.assertEqual(len(b127["pasos"]), 2)
        self.assertNotRegex(b127["pasos"][0]["titulo"] + " " + b127["pasos"][1]["titulo"], r"entrop[íi]a")

        # Hojas PNG oficiales de Parcial 1 y Parcial 2 + seguridad + 404 en parcial inválido
        for pnum in (1, 2):
            self.assertEqual(client.get(f"/api/formulario/p{pnum}/hoja.png").status_code, 401)
            r_png = client.get(f"/api/formulario/p{pnum}/hoja.png?token=tok-prep-test", headers=hdrs)
            self.assertEqual(r_png.status_code, 200)
            self.assertEqual(r_png.headers.get("content-type"), "image/png")
            self.assertTrue(r_png.content.startswith(b"\x89PNG\r\n\x1a\n"))
            self.assertGreater(len(r_png.content), 10000)
        self.assertEqual(client.get("/api/formulario/p9/hoja.png?token=tok-prep-test", headers=hdrs).status_code, 404)

        # Recorte real y hoja P1/P2 al abrir RP-23II-1 y RP-24I-2 vía HTTP
        for rid, p_exp in (("RP-23II-1", 1), ("RP-24I-2", 2)):
            r_rp = client.get(f"/api/banco/{rid}?token=tok-prep-test", headers=hdrs)
            self.assertEqual(r_rp.status_code, 200)
            jb = r_rp.json()
            self.assertEqual(jb["formulario_mapa"]["parcial"], p_exp)
            self.assertEqual(jb["formulario_mapa"]["hoja_url"], f"/api/formulario/p{p_exp}/hoja.png")
            self.assertTrue(jb.get("recorte") and jb["recorte"].get("url"), f"{rid} sin recorte PDF")


if __name__ == "__main__":
    unittest.main()
