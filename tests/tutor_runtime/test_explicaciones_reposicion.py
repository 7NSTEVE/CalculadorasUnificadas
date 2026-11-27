"""Pruebas del paso «Datos» y del «¿Por qué?» de las lecciones del plan Reposición II-2026.

Lo que se vigila:
  * toda lección del plan (68) tiene «¿Por qué?» en cada paso, y las que no son teoría empiezan con
    el paso «Datos»;
  * cada frase subrayada se encuentra en el recorte del PDF (o en el enunciado, si la lección no
    tiene recorte), con la misma regla que usa la pizarra;
  * las lecciones siguen pasando las revisiones del tutor y su TeX lo entiende la pizarra;
  * la API cambia las frases por las palabras exactas del PDF («6,0 cm» -> «6, 0 cm»).
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
from tools.tutor_runtime.documentos import Documentos, _norma_frase, ajustar_datos, ajustar_frase, caja_de_frase  # noqa: E402
from tools.tutor_runtime.fisica3 import explicaciones_reposicion as E  # noqa: E402
from tools.tutor_runtime.fisica3 import registro  # noqa: E402
from tools.tutor_runtime.fisica3.dsl import revisar  # noqa: E402
from tools.tutor_runtime.fisica3.reposicion_ii_2026 import ids_del_calendario  # noqa: E402
from tools.tutor_runtime.verificador_fisico import auditar_leccion  # noqa: E402

NODE_EXE = pathlib.Path(r"D:\DevTools\node\node.exe")
PLAN = ids_del_calendario()


def _node() -> str:
    return str(NODE_EXE) if NODE_EXE.is_file() else "node"


def _calza_en_enunciado(frase: str, enunciado: str) -> bool:
    """Como marcarDato de enunciado.js sin recorte: cada $…$ es una unidad aparte y no calza con texto."""
    unidades = []
    for trozo in re.split(r"(\$[^$]*\$)", enunciado):
        if len(trozo) > 1 and trozo.startswith("$") and trozo.endswith("$"):
            unidades.append("\x00")
        else:
            unidades += trozo.split()
    limpia = lambda t: re.sub(r"[.,;:()]+$", "", _norma_frase(t))  # noqa: E731
    us = [limpia(u) for u in unidades]
    q = [limpia(x) for x in _norma_frase(frase).split(" ")]
    return any(us[i:i + len(q)] == q for i in range(len(us) - len(q) + 1))


class TestPlanCompleto(unittest.TestCase):
    def test_el_plan_tiene_explicaciones_para_todas(self) -> None:
        self.assertEqual(len(PLAN), 68)
        self.assertEqual(set(E.lecciones_con_explicaciones()), set(PLAN))

    def test_porque_en_cada_paso_y_datos_al_empezar(self) -> None:
        sin_porque, sin_datos = [], []
        for lid in PLAN:
            lec = registro.obtener(lid)
            sin_porque += [f"{lid}: {p.titulo}" for p in lec.pasos if not p.porque.strip()]
            if lec.tipo != "teoria" and not lec.pasos[0].titulo.startswith(E.TITULO_DATOS):
                sin_datos.append(lid)
        self.assertEqual(sin_porque, [])
        self.assertEqual(sin_datos, [])

    def test_el_paso_datos_subraya_y_escribe_cada_dato(self) -> None:
        for lid, datos in E.DATOS.items():
            j = registro.obtener(lid).a_json()
            pasos = [p for p in j["pasos"] if p["titulo"].startswith(E.TITULO_DATOS)]
            accs = [a for p in pasos for t in p["tramos"] for a in t["acc"]]
            frases = [a["frase"] for a in accs if a["a"] == "dato"]
            mates = [a["tex"] for a in accs if a["a"] == "mate"]
            self.assertEqual(frases, [f for f, _, _ in datos if f], lid)
            self.assertEqual(mates, [t for _, t, _ in datos], lid)
            # Un tramo por dato (más la introducción), cada uno con su explicación hablada
            n_tramos = sum(len(p["tramos"]) for p in pasos)
            self.assertEqual(n_tramos, len(datos) + 1, lid)
            self.assertTrue(all(len(p["habla"].split()) <= 90 for p in pasos), lid)

    def test_no_pisa_porques_que_ya_existian(self) -> None:
        # Q106 («La idea»), O26-8 (Paso 1) y Q050 (A) traían su propio porqué desde antes
        for lid, prefijo in (("Q106", "La idea"), ("O26-8", "Paso 1"), ("Q050", "A) Fourier")):
            paso = next(p for p in registro.obtener(lid).pasos if p.titulo.startswith(prefijo))
            self.assertTrue(paso.porque.strip(), lid)
            self.assertNotIn(paso.porque, {t for _, t in E._GENERICOS}, lid)
            self.assertNotEqual(E.porque_de(lid, paso.titulo), paso.porque, lid)

    def test_porque_de_por_prefijo_y_generico(self) -> None:
        self.assertEqual(E.base_del_titulo("A) Proceso B→C (recta) (sigue) (sigue)"), "A) Proceso B→C (recta)")
        self.assertTrue(E.porque_de("Q108", "A) Proceso B→C (recta) (sigue)").startswith("En el diagrama p-V"))
        self.assertTrue(E.porque_de("Q110", "D) Coeficiente de rendimiento del refrigerador").startswith("El enunciado dice"))
        self.assertEqual(E.porque_de("NO-EXISTE", "Algo cualquiera"), "")


class TestFrasesYRevisiones(unittest.TestCase):
    def test_cada_frase_se_encuentra(self) -> None:
        docs = Documentos(REPO_ROOT, cache=None)
        faltan = []
        sin_pdf = []
        for lid, datos in E.DATOS.items():
            lec = registro.obtener(lid)
            rec = docs.recorte_de_leccion(lec)
            pal = (rec or {}).get("palabras") or []
            if rec is None and lec.fuente.get("doc") and lec.fuente.get("doc") not in ("26-II-O",):
                sin_pdf.append(lid)
                continue
            for frase, _, _ in datos:
                if not frase:
                    continue
                if pal:
                    if caja_de_frase(ajustar_frase(frase, pal), pal) is None:
                        faltan.append(f"{lid}: «{frase}» no está en el PDF")
                elif not _calza_en_enunciado(frase, lec.enunciado):
                    faltan.append(f"{lid}: «{frase}» no está en el enunciado")
        if sin_pdf and len(sin_pdf) == len(E.DATOS):
            self.skipTest("PDF no presentes en esta máquina")
        self.assertEqual(faltan, [])

    def test_siguen_pasando_las_revisiones(self) -> None:
        problemas = {}
        for lid in PLAN:
            lec = registro.obtener(lid)
            p = revisar(lec) + auditar_leccion(lec.a_json())["alertas"] + auditoria_pasos.auditar(lid)["faltas"]
            if p:
                problemas[lid] = p
        self.assertEqual(problemas, {})

    def test_tex_nuevo_que_entiende_la_pizarra(self) -> None:
        exprs = [(f"{lid}:dato{i}", t) for lid, ds in E.DATOS.items() for i, (_, t, _) in enumerate(ds)]
        textos = [(lid, t) for lid, d in E.PORQUE.items() for t in d.values()] + [("comun", t) for _, t in E._GENERICOS]
        exprs += [(f"{lid}:porque", m) for lid, t in textos for m in re.findall(r"\$([^$]+)\$", t)]
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
        self.assertGreater(res["total"], 700)
        self.assertEqual(res["bad"], [], f"TeX desconocido: {res['bad'][:5]}")

    def test_la_voz_no_lleva_latex_ni_dolares(self) -> None:
        for lid, datos in E.DATOS.items():
            for _, _, para in datos:
                self.assertIsNone(re.search(r"[\\$^_{}]", re.sub(r"\[\[[^\]]+\]\]", "", para)), f"{lid}: {para}")
        # Los porqués sí llevan $…$ (el asistente los narra con texAVoz), pero balanceados
        for lid, d in E.PORQUE.items():
            for k, t in d.items():
                self.assertEqual(t.count("$") % 2, 0, f"{lid} · {k}")


class TestAjusteDeFrases(unittest.TestCase):
    @staticmethod
    def _pal(texto: str):
        return [{"t": w} for w in texto.split()]

    def test_numeros_partidos_por_latex(self) -> None:
        pal = self._pal("el diámetro interior es de 6, 0 cm y la rapidez")
        self.assertIsNone(caja_de_frase("6,0 cm", pal))
        self.assertEqual(ajustar_frase("6,0 cm", pal), "6, 0 cm")
        self.assertIsNotNone(caja_de_frase(ajustar_frase("6,0 cm", pal), pal))

    def test_ligaduras_y_superindices(self) -> None:
        pal = self._pal("donde también ﬂota y tiene 1,26 m³")
        self.assertEqual(ajustar_frase("también flota", pal), "también ﬂota")
        self.assertEqual(ajustar_frase("1,26 m3", pal), "1,26 m³")

    def test_lo_que_ya_calza_o_no_existe_queda_igual(self) -> None:
        pal = self._pal("una masa de 0,500 kg")
        self.assertEqual(ajustar_frase("0,500 kg", pal), "0,500 kg")
        self.assertEqual(ajustar_frase("no aparece", pal), "no aparece")

    def test_ajustar_datos_cambia_solo_los_subrayados(self) -> None:
        j = {"pasos": [{"tramos": [{"acc": [{"a": "dato", "frase": "6,0 cm"}, {"a": "mate", "tex": "6,0"}]}]}]}
        self.assertEqual(ajustar_datos(j, self._pal("es de 6, 0 cm")), 1)
        self.assertEqual(j["pasos"][0]["tramos"][0]["acc"][0]["frase"], "6, 0 cm")
        self.assertEqual(j["pasos"][0]["tramos"][0]["acc"][1]["tex"], "6,0")

    def test_la_api_manda_las_frases_del_pdf(self) -> None:
        docs = Documentos(REPO_ROOT, cache=None)
        if "26-II-2O" not in docs.rutas():
            self.skipTest("PDF del II parcial II-2026 no presente en esta máquina")
        from fastapi.testclient import TestClient

        from tools.tutor_runtime.server import create_app

        client = TestClient(create_app({"session_token": "tok-exp"}))
        r = client.get("/api/banco/P2O26-4?token=tok-exp", headers={"Origin": "http://127.0.0.1:8765"})
        self.assertEqual(r.status_code, 200)
        j = r.json()
        self.assertTrue(j["pasos"][0]["titulo"].startswith(E.TITULO_DATOS))
        self.assertTrue(j["pasos"][0].get("porque"))
        frases = [a["frase"] for p in j["pasos"] for t in p["tramos"] for a in t["acc"] if a["a"] == "dato"]
        self.assertIn("6, 0 cm", frases)
        pal = j["recorte"]["palabras"]
        self.assertTrue(all(caja_de_frase(f, pal) is not None for f in frases), frases)


if __name__ == "__main__":
    unittest.main()
