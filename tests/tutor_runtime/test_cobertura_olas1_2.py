"""Pruebas de cobertura total Olas 1B + 2A + 2B (48 lecciones nuevas, banco de 326).

Valida:
  - Las 18 lecciones de Tareas de Termodinámica ≤ 20.5 (2024–2026): T4-24I-1..2, T3-24II-1..2,
    T3-25I-1..2, T3-25II-1..4, T3-26I-1,2,5..10 (excluyendo #3 Entropía y #4 Carnot).
  - Las 16 lecciones de Ejercicios Recomendados 1 II-2026 (Parcial 1: E1-1..E1-16).
  - Las 14 lecciones de Ejercicios Recomendados 2 II-2026 (Parcial 2: E2-1..E2-14).
  - Cero faltas y cero avisos en `dsl.revisar`, `verificador_fisico.auditar_leccion` y `auditoria_pasos`.
  - Cero comandos TeX desconocidos con `assets/cuadernu/tutor_web/js/mate/tex.js` real en Node.js.
  - Mapeo exacto a las 3 hojas oficiales (Parcial 1, 2 y 3) con `fuera_de_hoja == []`.
  - Valores numéricos clave, cotejo honesto de erratas oficiales y recortes PDF sin spoilers.
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
from tools.tutor_runtime.documentos import FUENTES, TAREAS_Y_REC, Documentos  # noqa: E402
from tools.tutor_runtime.fisica3 import registro, rutas  # noqa: E402
from tools.tutor_runtime.fisica3.dsl import revisar  # noqa: E402
from tools.tutor_runtime.fisica3.formularios_p1_p2 import mapear_formulario_por_parcial  # noqa: E402
from tools.tutor_runtime.verificador_fisico import auditar_leccion  # noqa: E402

NODE_EXE = pathlib.Path(r"D:\DevTools\node\node.exe")

TAREAS_TERMO = [
    "T4-24I-1", "T4-24I-2",
    "T3-24II-1", "T3-24II-2",
    "T3-25I-1", "T3-25I-2",
    "T3-25II-1", "T3-25II-2", "T3-25II-3", "T3-25II-4",
    "T3-26I-1", "T3-26I-2", "T3-26I-5", "T3-26I-6", "T3-26I-7", "T3-26I-8", "T3-26I-9", "T3-26I-10",
]
REC_P1 = [f"E1-{i}" for i in range(1, 17)]
REC_P2 = [f"E2-{i}" for i in range(1, 15)]
TODAS_48 = TAREAS_TERMO + REC_P1 + REC_P2

ERRATAS_OFICIALES_DOCUMENTADAS = {"T4-24I-2", "T3-24II-2", "T3-25II-4", "T3-26I-9", "E2-3"}
SOLO_SIMBOLICAS = {"E1-3", "E1-9"}


def _node() -> str:
    return str(NODE_EXE) if NODE_EXE.is_file() else "node"


class TestCoberturaOlas1Y2(unittest.TestCase):
    def test_todas_48_existen_y_cero_faltas_o_avisos(self) -> None:
        self.assertEqual(len(TODAS_48), 48)
        # Corte estricto ≤ 20.5: #3 (Entropía) y #4 (Carnot) de Tarea 3 I-2026 NO existen en el banco
        self.assertFalse(registro.existe("T3-26I-3"))
        self.assertFalse(registro.existe("T3-26I-4"))

        faltas = {}
        for lid in TODAS_48:
            self.assertTrue(registro.existe(lid), lid)
            lec = registro.obtener(lid)
            j = lec.a_json()
            probs = []
            probs += revisar(lec)
            probs += auditar_leccion(j)["alertas"]
            aud = auditoria_pasos.auditar(lid)
            probs += aud["faltas"] + aud["avisos"]
            if probs:
                faltas[lid] = probs
        self.assertEqual(faltas, {}, f"Faltas/avisos en Olas 1+2: {faltas}")

    def test_corte_20_5_y_hoja_correcta_en_las_48(self) -> None:
        for lid in TODAS_48:
            j = registro.obtener(lid).a_json()
            esperado_p = 3 if lid in TAREAS_TERMO else (1 if lid in REC_P1 else 2)
            self.assertEqual(j["parcial"], esperado_p, lid)
            mapa = mapear_formulario_por_parcial(j)
            self.assertEqual(mapa["parcial"], esperado_p, lid)
            base = [c for c in mapa["celdas"] if not c["es_constante"]]
            self.assertGreaterEqual(len(base), 1, f"{lid} sin celda oficial")
            self.assertEqual(mapa.get("fuera_de_hoja") or [], [], lid)
            if esperado_p == 3:
                blob = json.dumps(j, ensure_ascii=False)
                self.assertNotRegex(blob, r"Carnot|\\Delta\s*S|entrop", f"Fuga > 20.5 en {lid}")

    def test_cotejos_y_erratas_oficiales_honestas(self) -> None:
        for lid in TODAS_48:
            lec = registro.obtener(lid)
            c = lec.cotejo()
            if lid in SOLO_SIMBOLICAS:
                self.assertIsNone(c, lid)
                continue
            self.assertIsNotNone(c, f"{lid} sin cotejo")
            if lid in ERRATAS_OFICIALES_DOCUMENTADAS:
                self.assertEqual(c["estado"], "difiere", lid)
                self.assertTrue(lec.aviso and len(lec.aviso) > 40, f"{lid} debe explicar errata en aviso")
            else:
                self.assertEqual(c["estado"], "coincide", f"{lid}: {c}")

    def test_valores_numericos_clave_y_erratas_corregidas(self) -> None:
        def val(lid: str, inciso: str) -> float:
            return next(r["valor"] for r in registro.obtener(lid).respuestas if r["inciso"] == inciso)

        # Erratas oficiales corregidas con física exacta
        self.assertAlmostEqual(val("T4-24I-2", "d"), 919.65, delta=0.1)
        self.assertAlmostEqual(val("T3-24II-2", "C-70"), 0.0, delta=1e-6)
        self.assertAlmostEqual(val("T3-25I-1", "II-H"), 0.200044, delta=1e-5)
        self.assertAlmostEqual(val("T3-25II-4", "E-K"), 2.589, delta=0.01)
        self.assertAlmostEqual(val("T3-26I-9", "C-esf"), 1.980e5, delta=500.0)
        self.assertAlmostEqual(val("T3-26I-9", "C-cil"), 4.537e5, delta=1000.0)
        self.assertAlmostEqual(val("E2-3", "dy"), 0.860, delta=0.005)
        # Otros valores clave de Olas 1B, 2A y 2B
        self.assertAlmostEqual(val("T3-26I-10", "A"), 35180.0, delta=50.0)
        self.assertAlmostEqual(val("T3-26I-6", "E"), 8299.0, delta=10.0)
        self.assertAlmostEqual(val("E1-1", "a"), 0.1097, delta=0.001)
        self.assertAlmostEqual(val("E1-7", "b"), 2.676, delta=0.01)
        self.assertAlmostEqual(val("E1-16", "b"), 210.7, delta=0.5)
        self.assertAlmostEqual(val("E2-7", "d"), 0.6955, delta=0.002)
        self.assertAlmostEqual(val("E2-10", "d"), 8.955, delta=0.02)
        self.assertAlmostEqual(val("E2-11", "bloqueo"), 70.49, delta=0.1)
        self.assertAlmostEqual(val("E2-14", "d"), 0.1082, delta=0.001)

    def test_tex_nodejs_cero_desconocidos_en_48(self) -> None:
        exprs = []
        for lid in TODAS_48:
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
                    for acc in tr["acc"]:
                        if acc.get("a") == "mate" and acc.get("tex"):
                            exprs.append((f"{lid}:p{p['n']}", acc["tex"]))
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
        self.assertGreater(res["total"], 1000)
        self.assertEqual(res["bad"], [], f"TeX desconocido: {res['bad'][:5]}")

    def test_rutas_y_documentos_enlazados(self) -> None:
        pub = {r["id"]: r for r in rutas.publicas()}
        self.assertIn("tareas_termo_2024_2026", pub)
        self.assertIn("recomendados_p1_p2_ii_2026", pub)
        self.assertEqual(len(pub["tareas_termo_2024_2026"]["items"]), 18)
        self.assertEqual(len(pub["recomendados_p1_p2_ii_2026"]["items"]), 30)

        for doc_id in TAREAS_Y_REC:
            self.assertIn(doc_id, FUENTES)
        docs = Documentos(REPO_ROOT, cache=None)
        self.assertEqual(docs.leccion_de("recomendados1", "16"), "E1-16")
        self.assertEqual(docs.leccion_de("recomendados2", "14"), "E2-14")
        self.assertEqual(docs.leccion_de("26-I-T3", "10"), "T3-26I-10")
        self.assertIsNone(docs.leccion_de("26-I-T3", "3"))  # Entropía excluida
        self.assertIsNone(docs.leccion_de("26-I-T3", "4"))  # Carnot excluido

        rutas_pdf = docs.rutas()
        if all(d in rutas_pdf for d in TAREAS_Y_REC):
            self.assertEqual(len(docs.problemas("recomendados1")), 16)
            self.assertEqual(len(docs.problemas("recomendados2")), 14)
            self.assertTrue(docs.png("recomendados1", "1").startswith(b"\x89PNG"))
            self.assertTrue(docs.png("26-I-T3", "10").startswith(b"\x89PNG"))

        # Verificar sintaxis JS de biblioteca.js
        bib = REPO_ROOT / "assets" / "cuadernu" / "tutor_web" / "js" / "ui" / "biblioteca.js"
        proc = subprocess.run([_node(), "--check", str(bib)], capture_output=True, text=True, timeout=30)
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_simbolos_contexto_p1_p2(self) -> None:
        from tools.tutor_runtime.fisica3 import simbolos
        m1 = simbolos.mapa_leccion(registro.obtener("E1-16").a_json())
        self.assertEqual(m1["grupo"], "ondas")
        items1 = [it for lst in m1["por_accion"].values() for it in lst]
        n_items = [it for it in items1 if it[0] == "n"]
        self.assertTrue(n_items, "E1-16 debe explicar n (número de armónico)")
        self.assertIn("armónico", n_items[0][1].lower())
        self.assertNotIn("mol", n_items[0][1].lower())

        m2 = simbolos.mapa_leccion(registro.obtener("E2-10").a_json())
        self.assertEqual(m2["grupo"], "fluidos")
        items2 = [it for lst in m2["por_accion"].values() for it in lst]
        q_items = [it for it in items2 if it[0] == "Q"]
        self.assertTrue(q_items, "E2-10 debe explicar Q (caudal volumétrico)")
        self.assertIn("caudal", q_items[0][1].lower())
        self.assertNotIn("calor", q_items[0][1].lower())


if __name__ == "__main__":
    unittest.main()
