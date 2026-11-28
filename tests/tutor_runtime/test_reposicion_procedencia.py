"""Regresiones de origen, corte y formularios del recorrido de reposición."""

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

import fitz

from tools.tutor_runtime.documentos import Documentos, Problema
from tools.tutor_runtime.fisica3 import procedencia, registro
from tools.tutor_runtime.fisica3.formularios_p1_p2 import mapear_formulario_por_parcial
from tools.tutor_runtime.fisica3.preparacion_examenes import filtrar_leccion_corte_20_5
from tools.tutor_runtime.fisica3.reposicion_ii_2026 import ids_del_calendario
from tools.tutor_runtime.verificador_fisico import auditar_leccion


def leccion(lid):
    j = filtrar_leccion_corte_20_5(registro.obtener(lid).a_json())
    j["formulario_mapa"] = mapear_formulario_por_parcial(j)
    procedencia.completar(j)
    return j


def acciones(paso):
    return [a for t in paso["tramos"] for a in t["acc"]]


class ProcedenciaReposicionTest(unittest.TestCase):
    def test_dato_del_manometro_y_desglose_de_la_altura(self):
        j = leccion("P2O26-8")
        p = j["pasos"][0]
        a = next(a for a in acciones(p) if a.get("frase") == "64,0 kPa")
        origen = next(o for o in p["origen"] if o["fuente"] == {"tipo": "enunciado", "dato_id": a["id"]})
        self.assertIn("64.0", origen["terminos"])
        conversion = next(p for p in j["pasos"] if any(a["id"] == "presion-a-pascales" for a in acciones(p)))
        self.assertTrue(any(o["destino_id"] == "presion-a-pascales" and "64.0" in o["terminos"]
                            and o["fuente"] == {"tipo": "renglon", "accion_id": origen["destino_id"]}
                            for o in conversion["origen"]))
        fila = next(p for p in j["pasos"] if any(a["id"] == "subida-mercurio" for a in acciones(p)))
        detalle = [r["tex"] for r in fila["lento"] if r.get("destino_id") == "subida-mercurio"]
        self.assertGreaterEqual(len(detalle), 3)
        self.assertTrue(any("0{,}500" in r for r in detalle))

    def test_no_atribuye_los_numeros_de_la_formula_al_enunciado(self):
        j = leccion("O26-2")
        p = j["pasos"][1]
        anclas = [o for o in p["origen"] if (o.get("fuente") or {}).get("tipo") == "enunciado"]
        self.assertTrue(anclas)
        self.assertFalse(any("1" in o["terminos"] for o in anclas))

    def test_explicacion_de_logaritmos_con_ecuaciones_independientes(self):
        j = leccion("T-MAT3")
        self.assertEqual([], auditar_leccion(j)["alertas"])
        lento = [r["tex"] for r in j["pasos"][0]["lento"]]
        self.assertEqual([r"\ln x = y", "e^{y} = x"], lento)

    def test_practica_de_conduccion_desglosa_y_rastrea_e(self):
        j = leccion("PR2-D10")
        self.assertEqual(10, len(j["pasos"]))
        self.assertEqual([], auditar_leccion(j)["alertas"])
        previo = j["pasos"][5]
        self.assertIn("pr2-valor-e", [a["id"] for a in acciones(previo)])
        p = j["pasos"][7]
        self.assertTrue(any(r["destino_id"] == "pr2-e-menos-uno" and "2{,}718-1" in r["tex"] for r in p["lento"]))
        self.assertTrue(any(o["destino_id"] == "pr2-e-menos-uno"
                            and o["fuente"] == {"tipo": "renglon", "accion_id": "pr2-valor-e"} for o in p["origen"]))

    def test_origen_renglon_anterior_y_celdas_de_otto(self):
        ciclo = leccion("O26-9")
        for destino in ("gam", "gamma-sustitucion"):
            origenes = [o for p in ciclo["pasos"] for o in p.get("origen", []) if o.get("destino_id") == destino]
            self.assertIn({"tipo": "renglon", "accion_id": "duda"}, [o["fuente"] for o in origenes])
        otto = leccion("O26-7")["pasos"][1]
        celdas = {o["fuente"].get("celda_id") for o in otto["origen"]}
        self.assertTrue({"F11C3", "F12C1"}.issubset(celdas))
        razon = [o for o in otto["origen"] if o["fuente"].get("celda_id") == "F12C1"]
        self.assertEqual({o["destino_id"] for o in razon}, {"razon-otto"})

    def test_76_lecciones_con_anclas_validas_y_pasos_estables(self):
        ids = list(dict.fromkeys(ids_del_calendario()))
        self.assertEqual(76, len(ids))
        num_pasos = 0
        num_lentos = 0
        for lid in ids:
            j = leccion(lid)
            numeros = {a["id"]: p["n"] for p in j["pasos"] for a in acciones(p)}
            celdas = {c["id"] for c in j["formulario_mapa"]["celdas"]}
            num_pasos += len(j["pasos"])
            for p in j["pasos"]:
                num_lentos += bool(p.get("lento"))
                for o in p.get("origen", []):
                    f = o.get("fuente")
                    if f is None:
                        continue
                    self.assertEqual(p["n"], numeros[o["destino_id"]], lid)
                    if f["tipo"] == "formulario":
                        self.assertIn(f["celda_id"], celdas, lid)
                    else:
                        self.assertLessEqual(numeros[f.get("dato_id") or f.get("accion_id")], p["n"], lid)
        self.assertEqual(443, num_pasos)
        self.assertGreater(num_lentos, 100)

    def test_inciso_fuera_del_recorte_original(self):
        doc = Documentos(Path.cwd(), carpetas=[])
        with fitz.open() as pdf:
            pagina = pdf.new_page()
            pagina.insert_text((70, 70), "A) Trabajo")
            pagina.insert_text((70, 125), "B) Calor")
            pagina.insert_text((70, 140), "C) Eficiencia")
            pagina.insert_text((70, 165), "D) Entropia")
            doc._docs["examen"] = pdf
            doc._probs["examen"] = [Problema("3", "3", "Problema 3", 1, [(0, fitz.Rect(0, 45, 550, 180))])]
            completo = doc.recorte("examen", "3")
            recortado = doc.recorte("examen", "3", "D")
            self.assertNotEqual(completo["url"], recortado["url"])
            self.assertIn("D)", [w["t"] for w in completo["palabras"]])
            self.assertIn("C)", [w["t"] for w in recortado["palabras"]])
            self.assertNotIn("D)", [w["t"] for w in recortado["palabras"]])
            self.assertLess(recortado["alto"], completo["alto"])
            self.assertNotEqual(doc.png("examen", "3"), doc.png("examen", "3", "D"))
            with self.assertRaises(KeyError):
                doc.png("examen", "3", "E")
        self.assertIn("11 puntos", leccion("Q108")["aviso"])

    def test_hoja_aparece_y_cambia_en_la_misma_instancia(self):
        base = Path("build/tutor-reposicion-prioritaria").resolve()
        base.mkdir(parents=True, exist_ok=True)
        with TemporaryDirectory(dir=base) as tmp:
            raiz = Path(tmp)
            curso = raiz / "Fisica_III"
            semestre = curso / "Documentos II - 2026"
            semestre.mkdir(parents=True)
            hoja = semestre / "Ecuaciones_Importantes_Parcial_1.pdf"
            carpeta_cache = raiz / "cache"
            docs = Documentos(raiz, carpetas=[curso], cache=carpeta_cache)
            respaldo, version = docs.hoja_formulario(1)
            self.assertIsNone(version)
            self.assertFalse(carpeta_cache.exists())
            versiones = []
            for texto in ("Primera hoja", "Segunda hoja actualizada"):
                with fitz.open() as pdf:
                    pagina = pdf.new_page()
                    pagina.insert_text((110, 245), texto)
                    pdf.save(hoja)
                imagen, version = docs.hoja_formulario(1)
                self.assertTrue(version)
                self.assertNotEqual(respaldo, imagen)
                versiones.append((imagen, version))
            self.assertNotEqual(versiones[0][1], versiones[1][1])
            self.assertNotEqual(versiones[0][0], versiones[1][0])
            self.assertEqual(2, len(list(carpeta_cache.glob("*.png"))))
