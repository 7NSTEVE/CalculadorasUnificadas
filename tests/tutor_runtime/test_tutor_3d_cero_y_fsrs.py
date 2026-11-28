"""Suite integral de verificación del Tutor Desde Cero, Motor 3D, FSRS-5 Sprint,
Guías P1/P2/P3, 24 Variantes Paramétricas, Simulacro 3 Parciales y Minipruebas A/B.
"""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import unittest

from fastapi.testclient import TestClient

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
if str(REPO_ROOT / "tools" / "tutor_runtime") not in sys.path:
    sys.path.insert(0, str(REPO_ROOT / "tools" / "tutor_runtime"))

import auditoria_pasos  # noqa: E402
from tools.tutor_runtime.fisica3 import apoyos_reposicion, registro, variantes  # noqa: E402
from tools.tutor_runtime.fisica3.dsl import revisar  # noqa: E402
from tools.tutor_runtime.fisica3.guia_formulario_ii_2026 import guia_por_parcial  # noqa: E402
from tools.tutor_runtime.fisica3.preparacion_examenes import preparacion_completa  # noqa: E402
from tools.tutor_runtime.fisica3.reposicion_ii_2026 import ids_del_calendario  # noqa: E402
from tools.tutor_runtime.pedagogy_memory import PedagogyMemoryStore  # noqa: E402
from tools.tutor_runtime.server import create_app  # noqa: E402
from tools.tutor_runtime.verificador_fisico import auditar_leccion  # noqa: E402

NODE_EXE = pathlib.Path(r"D:\DevTools\node\node.exe")
TEORIA_CERO_NUEVAS = [
    "T-BASE1", "T-BASE2", "T-BASE3", "T-BASE4",
    "T-MAS", "T-PEND", "T-ONDA", "T-SON",
    "T-HIDRO", "T-FLOT", "T-BERN", "T-VISC",
]
CASOS_DORADOS = ["P1O26-8", "P2O26-8", "Q108", "O26-8"]


def _node() -> str:
    return str(NODE_EXE) if NODE_EXE.is_file() else "node"


class TestTeoriaDesdeCeroY3D(unittest.TestCase):
    def test_12_lecciones_teoria_cero_pasan_revisiones_y_tex(self) -> None:
        exprs = []
        faltas = {}
        for lid in TEORIA_CERO_NUEVAS:
            self.assertTrue(registro.existe(lid), f"Falta {lid}")
            lec = registro.obtener(lid)
            j = lec.a_json()
            prob = revisar(lec) + auditar_leccion(j)["alertas"] + auditoria_pasos.auditar(lid)["faltas"]
            if prob:
                faltas[lid] = prob
            self.assertIsNotNone(j["practica"], f"{lid} sin Te toca")
            for p in j["pasos"]:
                self.assertTrue(p.get("porque"), f"{lid} paso {p['n']} sin porque")
                for tr in p["tramos"]:
                    for acc in tr["acc"]:
                        if acc.get("a") == "mate" and acc.get("tex"):
                            exprs.append((f"{lid}:p{p['n']}", acc["tex"]))
        self.assertEqual(faltas, {}, f"Faltas en teoría cero: {faltas}")

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
        self.assertEqual(res["bad"], [], f"TeX desconocido en teoría cero: {res['bad'][:5]}")

    def test_cobertura_3d_total_76_reposicion_y_todas_las_lecciones(self) -> None:
        modelos_validos = set(apoyos_reposicion._ESCENAS)
        # 76/76 del calendario Reposición + 4 casos dorados (salvo las excepciones escritas:
        # lecciones de matemática pura que se ven mejor con su dibujo 2D, ver SIN_ESCENA_3D)
        for lid in ids_del_calendario() + CASOS_DORADOS:
            lec = registro.obtener(lid).a_json()
            ap = apoyos_reposicion.apoyo_leccion(lec)
            if lid in apoyos_reposicion.SIN_ESCENA_3D:
                self.assertIsNone(ap, lid)
                continue
            self.assertIsNotNone(ap, f"{lid} sin apoyo 3D")
            self.assertEqual(ap["motor"], "webgl2", lid)
            self.assertIn(ap["modelo"], modelos_validos, lid)
            self.assertTrue(ap["piden"], f"{lid} sin incisos/pregunta")
            self.assertEqual(len(ap["pasos"]), len(lec["pasos"]), lid)

        # Todas las lecciones del banco (~340) tienen escena 3D válida (o excepción escrita)
        for lid in registro.ids():
            if lid in apoyos_reposicion.SIN_ESCENA_3D:
                continue
            lec = registro.obtener(lid).a_json()
            ap = apoyos_reposicion.apoyo_leccion(lec)
            self.assertIsNotNone(ap, lid)
            self.assertIn(ap["modelo"], modelos_validos, lid)

    def test_24_generadores_de_variantes_p1_p2_p3(self) -> None:
        self.assertGreaterEqual(len(variantes.ARQUETIPOS), 24)
        for arq in variantes.ARQUETIPOS:
            for seed in (7, 42, 99):
                lec = variantes.generar_variante(arq, semilla=seed)
                self.assertEqual(revisar(lec), [], f"{arq} seed={seed}")
                j = lec.a_json()
                self.assertTrue(j["respuestas"], f"{arq} sin respuestas")
                ap = apoyos_reposicion.apoyo_leccion(j)
                self.assertIsNotNone(ap, f"{arq} sin apoyo 3D")

    def test_guias_formulario_tres_parciales(self) -> None:
        g1 = guia_por_parcial(1)
        g2 = guia_por_parcial(2)
        g3 = guia_por_parcial(3)
        self.assertEqual(g1["parcial"], 1)
        self.assertEqual(g2["parcial"], 2)
        self.assertEqual(g3["parcial"], 3)
        self.assertGreaterEqual(g1["total"], 37)
        self.assertGreaterEqual(g2["total"], 24)
        self.assertEqual(g3["total"], 42)
        self.assertTrue(g1["entrenador"] and g2["entrenador"] and g3["entrenador"])


class TestFSRS5SprintYEndpointsAPI(unittest.TestCase):
    def setUp(self) -> None:
        self.token = "test-token-tutor-3d"
        self.app = create_app({
            "session_token": self.token,
            "repo_root": str(REPO_ROOT),
            "telemetry_enabled": False,
            "memory_db_path": ":memory:",
            "progreso_path": None,
        })
        self.client = TestClient(self.app)
        self.hdr = {"X-CuadernU-Token": self.token}

    def test_fsrs5_sprint_misconception_y_resolucion_tras_2_ciegas(self) -> None:
        mem = PedagogyMemoryStore(":memory:")
        t0 = 1_700_000_000
        # Fallo inicial -> 15 min de intervalo y error registrado
        r_fail = mem.record_practice_outcome("RP-26I-7", "Resorte con fase", False, now_sec=t0, sprint_mode=True, blind=True)
        self.assertEqual(r_fail["interval_min"], 15.0)
        self.assertEqual(r_fail["consecutive_blind_passes"], 0)
        mem.log_misconception("RP-26I-7", "SignError", "1.047", "-1.047", "Signo invertido de fase", now_sec=t0)

        q1 = mem.get_due_review_queue(now_sec=t0 + 1000)
        self.assertEqual(q1["resumen"]["errores_abiertos"], 1)
        self.assertEqual(q1["cola"][0]["skill_id"], "RP-26I-7")

        # 1.er acierto ciego -> todavía abierto
        r1 = mem.record_practice_outcome("RP-26I-7", "Resorte con fase", True, now_sec=t0 + 1200, sprint_mode=True, blind=True)
        self.assertEqual(r1["consecutive_blind_passes"], 1)
        self.assertEqual(r1["misconceptions_resolved"], 0)

        # 2.º acierto ciego -> resuelve el error automáticamente
        r2 = mem.record_practice_outcome("RP-26I-7", "Resorte con fase", True, now_sec=t0 + 2000, sprint_mode=True, blind=True)
        self.assertEqual(r2["consecutive_blind_passes"], 2)
        self.assertEqual(r2["misconceptions_resolved"], 1)
        q2 = mem.get_due_review_queue(now_sec=t0 + 2100)
        self.assertEqual(q2["resumen"]["errores_abiertos"], 0)
        mem.close()

    def test_api_practica_verificar_diagnostico_errores_y_repaso_hoy(self) -> None:
        # Signo invertido
        r_sig = self.client.post("/api/practica/verificar", headers=self.hdr, json={
            "id": "RP-26I-7:B", "titulo": "Fase del resorte", "esperado": -1.047, "respuesta": "1,047", "ciega": True,
        })
        self.assertEqual(r_sig.status_code, 200)
        self.assertFalse(r_sig.json()["ok"])
        self.assertEqual(r_sig.json()["categoria"], "SignError")

        # Error de potencia de 10 (unidades)
        r_u = self.client.post("/api/practica/verificar", headers=self.hdr, json={
            "id": "RP-24II-2:A", "titulo": "Venturi", "esperado": 0.02, "respuesta": "2.0", "ciega": True,
        })
        self.assertEqual(r_u.status_code, 200)
        self.assertEqual(r_u.json()["categoria"], "UnitPowerOfTen")

        # Cola /api/repaso/hoy incluye los errores
        rep = self.client.get("/api/repaso/hoy", headers=self.hdr)
        self.assertEqual(rep.status_code, 200)
        ids_cola = [x["skill_id"] for x in rep.json()["cola"]]
        self.assertIn("RP-26I-7:B", ids_cola)

        # Acierto exacto
        r_ok = self.client.post("/api/practica/verificar", headers=self.hdr, json={
            "id": "RP-26I-7:B", "titulo": "Fase del resorte", "esperado": -1.047, "respuesta": "-1,047", "ciega": True,
        })
        self.assertTrue(r_ok.json()["ok"])

    def test_api_casos_dorados_checkpoint_y_capas_cero(self) -> None:
        for lid in CASOS_DORADOS:
            qs = "?corte=20.5" if lid == "Q108" else ""
            r = self.client.get(f"/api/banco/{lid}{qs}", headers=self.hdr)
            self.assertEqual(r.status_code, 200, lid)
            j = r.json()
            self.assertIn("apoyo_visual", j, lid)
            self.assertIn("capas_cero", j, lid)
            self.assertTrue(j["capas_cero"]["capa0_analogia"], lid)
            self.assertTrue(any(p.get("pregunta") for p in j["pasos"]), f"{lid} sin checkpoint activo")
            if lid == "Q108":
                self.assertEqual(j.get("corte_aplicado"), "20.5")

    def test_api_simulacro_reposicion_3_parciales_y_sprint_6_dias(self) -> None:
        r1 = self.client.get("/api/simulacro/reposicion?formato=i2026", headers=self.hdr)
        self.assertEqual(r1.status_code, 200)
        d1 = r1.json()
        self.assertEqual(len(d1["items"]), 9)
        sels = [x for x in d1["items"] if x["lec"]["tipo"] == "seleccion"]
        dess = [x for x in d1["items"] if x["lec"]["tipo"] == "desarrollo"]
        self.assertEqual(len(sels), 6)
        self.assertEqual(len(dess), 3)
        self.assertEqual(sorted(x["lec"]["parcial"] for x in sels), [1, 1, 2, 2, 3, 3])
        self.assertEqual(sorted(x["lec"]["parcial"] for x in dess), [1, 2, 3])

        r2 = self.client.get("/api/simulacro/reposicion?formato=ii2025", headers=self.hdr)
        self.assertEqual(r2.status_code, 200)
        self.assertEqual(len(r2.json()["items"]), 4)
        self.assertEqual(r2.json()["puntos"], 60)

        prep = preparacion_completa()
        s6 = prep["sprint_6_dias"]
        self.assertEqual(len(s6["bloques"]), 6)
        self.assertGreaterEqual(s6["total"], 45)

    def test_api_miniprueba_oculta_soluciones_hasta_entregar(self) -> None:
        ev0 = self.client.get("/api/intensivo/evaluacion/A", headers=self.hdr).json()
        self.assertFalse(ev0["entregada"])
        for it in ev0["items"]:
            self.assertNotIn("correcta", it)
            self.assertNotIn("rubrica", it)

        # Entregar -> se abren claves y rúbricas 2/4/2
        r_ent = self.client.post("/api/progreso", headers=self.hdr, json={
            "tipo": "intensivo", "evento": "entregar", "id": "A",
        })
        self.assertEqual(r_ent.status_code, 200)
        ev1 = self.client.get("/api/intensivo/evaluacion/A", headers=self.hdr).json()
        self.assertTrue(ev1["entregada"])
        self.assertTrue(any("rubrica" in it for it in ev1["items"]))
        self.assertTrue(any("correcta" in it for it in ev1["items"]))


if __name__ == "__main__":
    unittest.main()
