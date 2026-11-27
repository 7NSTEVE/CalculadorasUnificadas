"""Pruebas de la preparación para la Reposición II-2026 (FI2103).

Cubre: las 47 lecciones nuevas (I y II parcial II-2026, integrales desde cero, suficiencia I-2026),
las dos lecciones de integrales corregidas (RP-24I-2, RP-25II-2), la ruta por días de
`fisica3/reposicion_ii_2026.py`, la hoja esperada, los PDF nuevos y la API.
"""

from __future__ import annotations

import json
import pathlib
import re
import subprocess
import sys
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(REPO_ROOT / "tools" / "tutor_runtime") not in sys.path:
    sys.path.insert(0, str(REPO_ROOT / "tools" / "tutor_runtime"))

import auditoria_pasos  # noqa: E402
from tools.tutor_runtime.documentos import FUENTES, PARCIALES_REPO26, Documentos  # noqa: E402
from tools.tutor_runtime.fisica3 import registro  # noqa: E402
from tools.tutor_runtime.fisica3.dsl import revisar  # noqa: E402
from tools.tutor_runtime.fisica3.formularios_p1_p2 import mapear_formulario_por_parcial  # noqa: E402
from tools.tutor_runtime.fisica3.reposicion_ii_2026 import (  # noqa: E402
    CALENDARIO, CASILLAS_NUEVAS, REPOSICIONES, hoja_predicha, ids_del_calendario, prediccion)
from tools.tutor_runtime.verificador_fisico import auditar_leccion  # noqa: E402

NODE_EXE = pathlib.Path(r"D:\DevTools\node\node.exe")

PARCIALES = [f"P{p}{t}26-{i}" for p in (1, 2) for t in ("O", "E") for i in range(1, 10)]
TEORIA = ["T-INT0", "T-INT1", "T-PARED", "T-INT2"]
SUFICIENCIA = [f"SUF26-{i}" for i in range(1, 8)]
CORREGIDAS = ["RP-24I-2", "RP-25II-2"]
NUEVAS = PARCIALES + TEORIA + SUFICIENCIA

# Selecciones del I parcial extraordinario cuya clave publicada no corresponde al enunciado
CLAVE_DE_OTRA_VERSION = {"P1E26-1", "P1E26-2", "P1E26-3", "P1E26-4", "P1E26-5", "P1E26-7"}


def _node() -> str:
    return str(NODE_EXE) if NODE_EXE.is_file() else "node"


class TestLeccionesReposicion(unittest.TestCase):
    def test_todas_existen_y_pasan_las_revisiones(self) -> None:
        faltas = {}
        for lid in NUEVAS + CORREGIDAS:
            self.assertTrue(registro.existe(lid), lid)
            lec = registro.obtener(lid)
            j = lec.a_json()
            problemas = []
            problemas += revisar(lec)
            problemas += auditar_leccion(j)["alertas"]
            aud = auditoria_pasos.auditar(lid)
            problemas += aud["faltas"] + aud["avisos"]
            if problemas:
                faltas[lid] = problemas
        self.assertEqual(faltas, {}, f"Lecciones con faltas: {faltas}")

    def test_cotejo_con_las_soluciones_publicadas(self) -> None:
        for lid in PARCIALES + CORREGIDAS:
            j = registro.obtener(lid).a_json()
            if lid in CLAVE_DE_OTRA_VERSION:
                # Sin cotejo, y el aviso lo explica
                self.assertIsNone(j["cotejo"], lid)
                self.assertIn("otra versión del examen", j["aviso"], lid)
                continue
            self.assertIsNotNone(j["cotejo"], f"{lid} sin cotejo")
            self.assertEqual(j["cotejo"]["estado"], "coincide", f"{lid}: {j['cotejo']}")

    def test_suficiencia_avisa_que_la_solucion_es_propia(self) -> None:
        for lid in SUFICIENCIA[:-1]:
            self.assertIn("no publicó la solución", registro.obtener(lid).aviso, lid)
        # El 7 es el mismo problema del III parcial extraordinario I-2024
        s7 = registro.obtener("SUF26-7")
        self.assertIn("Q096", s7.alias)
        self.assertEqual([r["valor"] for r in s7.respuestas], [r["valor"] for r in registro.obtener("Q096").respuestas])

    def test_casillas_de_la_hoja_correcta(self) -> None:
        for lid in PARCIALES + SUFICIENCIA[:4] + CORREGIDAS:
            j = registro.obtener(lid).a_json()
            mapa = mapear_formulario_por_parcial(j)
            self.assertEqual(mapa["parcial"], j["parcial"], lid)
            cel = [c for c in mapa["celdas"] if not c["es_constante"]]
            self.assertGreaterEqual(len(cel), 1, f"{lid} no usa ninguna casilla de su hoja")
            self.assertEqual(mapa.get("fuera_de_hoja") or [], [], lid)

    def test_numeros_clave(self) -> None:
        def resp(lid: str, inciso: str) -> float:
            return next(r["valor"] for r in registro.obtener(lid).respuestas if r["inciso"] == inciso)

        self.assertAlmostEqual(resp("P2O26-8", "b"), 1076.7, delta=0.5)
        self.assertAlmostEqual(resp("P2O26-9", "d"), 0.1285, delta=0.0005)
        self.assertAlmostEqual(resp("P2E26-8", "c"), 1896.3, delta=0.5)
        self.assertAlmostEqual(resp("P2E26-9", "a"), 0.01508, delta=0.00002)
        self.assertAlmostEqual(resp("P1O26-9", "c-L"), 0.5883, delta=0.0002)
        self.assertAlmostEqual(resp("P1E26-8", "c"), 2.676, delta=0.002)
        self.assertAlmostEqual(resp("SUF26-6", "B"), 0.7874, delta=0.0005)
        self.assertAlmostEqual(resp("SUF26-5", "C"), -73.31, delta=0.02)
        # La integral de la ventana, el atajo p_c·A y la represa
        self.assertAlmostEqual(resp("RP-25II-2", "A"), 4.2875e8, delta=1e5)

    def test_tex_que_entiende_la_pizarra(self) -> None:
        exprs = []
        for lid in NUEVAS + CORREGIDAS:
            j = registro.obtener(lid).a_json()
            for p in j["pasos"]:
                for tr in p["tramos"]:
                    for acc in tr["acc"]:
                        if acc.get("a") == "mate" and acc.get("tex"):
                            exprs.append((f"{lid}:p{p['n']}", acc["tex"]))
            exprs += [(f"{lid}:resp", r["tex"]) for r in j["respuestas"] if r.get("tex")]
        uri = (REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "mate" / "tex.js").as_uri()
        js = (
            f"import {{ analizarTex }} from {json.dumps(uri)};\n"
            "import fs from 'node:fs';\n"
            "const items = JSON.parse(fs.readFileSync(0, 'utf8'));\n"
            "const bad = items.map(([id, t]) => [id, t, analizarTex(t).desconocidos || []]).filter(([, , d]) => d.length > 0);\n"
            "console.log(JSON.stringify({ total: items.length, bad }));\n"
        )
        proc = subprocess.run([_node(), "--input-type=module", "-e", js], input=json.dumps(exprs, ensure_ascii=False),
                              capture_output=True, text=True, encoding="utf-8", timeout=30)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        res = json.loads(proc.stdout)
        self.assertGreater(res["total"], 500)
        self.assertEqual(res["bad"], [], f"TeX desconocido: {res['bad'][:5]}")


class TestRutaReposicion(unittest.TestCase):
    def test_calendario_de_10_dias_sin_pendientes(self) -> None:
        from tools.tutor_runtime.fisica3.preparacion_examenes import preparacion_completa

        r26 = preparacion_completa()["reposicion_ii_2026"]
        self.assertEqual(len(r26["bloques"]), 10)
        self.assertEqual(r26["pendientes"], [])
        self.assertEqual(r26["total"], len(ids_del_calendario()))
        fechas = [b["fecha"] for b in r26["bloques"]]
        self.assertEqual(fechas[0], "2026-11-27")
        self.assertEqual(fechas[-1], "2026-12-06")
        self.assertEqual(fechas, sorted(fechas))
        for b in r26["bloques"]:
            for it in b["items"]:
                self.assertTrue(registro.existe(it["id"]), it["id"])
                cal = it["calentamiento"]
                self.assertEqual(len(cal["opciones"]), 3)
                self.assertTrue(cal["explica"])

    def test_simulacros_con_el_formato_de_i_2026(self) -> None:
        from tools.tutor_runtime.fisica3.preparacion_examenes import preparacion_completa

        sims = [b for b in preparacion_completa()["reposicion_ii_2026"]["bloques"] if b["simulacro"]]
        self.assertEqual([b["id"] for b in sims], ["D8", "D10"])
        for b in sims:
            sel = [it for it in b["items"] if it["tipo"] == "seleccion"]
            des = [it for it in b["items"] if it["tipo"] == "desarrollo"]
            self.assertEqual(len(sel), 6, b["id"])
            self.assertEqual(len(des), 3, b["id"])
            self.assertEqual(sorted(it["parcial"] for it in sel), [1, 1, 2, 2, 3, 3], b["id"])
            self.assertEqual(sorted(it["parcial"] for it in des), [1, 2, 3], b["id"])

    def test_dias_del_calendario_coinciden_con_la_fecha(self) -> None:
        import datetime

        dias = ["lun", "mar", "mié", "jue", "vie", "sáb", "dom"]
        for _, _, fecha, titulo, *_ in CALENDARIO:
            d = datetime.date.fromisoformat(fecha)
            self.assertIn(f"{dias[d.weekday()]} {d.day}-", titulo, f"{fecha}: {titulo}")

    def test_prediccion_y_hoja_esperada(self) -> None:
        pred = prediccion()
        arq = {a["clave"]: a for a in pred["arquetipos"]}
        self.assertEqual((arq["ciclo"]["veces"], arq["ciclo"]["de"]), (5, 5))
        self.assertEqual(arq["resorte_fase"]["veces"], 3)
        self.assertEqual(sum(1 for r in REPOSICIONES if r["items"]), 5)
        hoja = hoja_predicha()
        self.assertEqual([p["casillas"] for p in hoja["partes"]], [38, 27, 42])
        marcadas = {c["id"] for p in hoja["partes"] for c in p["celdas"] if c["marca"] == "nueva"}
        self.assertEqual(marcadas, {c["id"] for c in CASILLAS_NUEVAS})
        # Carnot y entropía no están en la hoja esperada
        texto = " ".join(c["tex"] for p in hoja["partes"] for c in p["celdas"])
        self.assertNotRegex(texto, r"Carnot|\\Delta\s*S")

    def test_api_preparacion_trae_la_reposicion(self) -> None:
        from fastapi.testclient import TestClient

        from tools.tutor_runtime.server import create_app

        client = TestClient(create_app({"session_token": "tok-r26"}))
        r = client.get("/api/preparacion?token=tok-r26", headers={"Origin": "http://127.0.0.1:8765"})
        self.assertEqual(r.status_code, 200)
        r26 = r.json()["reposicion_ii_2026"]
        self.assertEqual(len(r26["bloques"]), 10)
        self.assertIn("prediccion", r26)


class TestDocumentosYPantalla(unittest.TestCase):
    def test_fuentes_y_enlace_pregunta_leccion(self) -> None:
        for doc in ("26-II-1O", "26-II-1E", "26-II-2O", "26-II-2E", "26-I-SUF"):
            self.assertIn(doc, FUENTES)
            self.assertIn(doc, PARCIALES_REPO26)
        docs = Documentos(REPO_ROOT, cache=None)
        self.assertEqual(docs.leccion_de("26-II-2O", "8"), "P2O26-8")
        self.assertEqual(docs.leccion_de("26-I-SUF", "3"), "SUF26-3")

    def test_recorte_de_los_pdf_nuevos(self) -> None:
        docs = Documentos(REPO_ROOT, cache=None)
        rutas = docs.rutas()
        faltan = [d for d in PARCIALES_REPO26 if d not in rutas]
        if faltan:
            self.skipTest(f"PDF no presentes en esta máquina: {faltan}")
        for doc in ("26-II-1O", "26-II-1E", "26-II-2O", "26-II-2E"):
            self.assertEqual([p.clave for p in docs.problemas(doc)], [str(i) for i in range(1, 10)], doc)
        self.assertEqual([p.clave for p in docs.problemas("26-I-SUF")], [str(i) for i in range(1, 9)])
        self.assertTrue(docs.png("26-II-2E", "8").startswith(b"\x89PNG"))

    def test_pantalla_conoce_el_plan_y_el_dibujo_de_la_pared(self) -> None:
        js = REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js"
        prep = (js / "ui" / "preparacion.js").read_text(encoding="utf-8")
        self.assertIn("'repo26'", prep)
        self.assertIn("reposicion_ii_2026", prep)
        self.assertIn("['repo26', 'Reposición II-2026']", (js / "ui" / "inicio.js").read_text(encoding="utf-8"))
        idx = (js / "visuales" / "index.js").read_text(encoding="utf-8")
        self.assertRegex(idx, r"pared:\s*VisualPared")
        for f in (js / "ui" / "preparacion.js", js / "ui" / "inicio.js", js / "visuales" / "fluidos.js"):
            proc = subprocess.run([_node(), "--check", str(f)], capture_output=True, text=True, timeout=30)
            self.assertEqual(proc.returncode, 0, f"{f.name}: {proc.stderr}")
        # Sin emojis en el texto nuevo de la pantalla
        self.assertIsNone(re.search(r"[\U0001F300-\U0001FAFF]", prep))


if __name__ == "__main__":
    unittest.main()
