"""Regresión del pedido "paso a paso sin saltos, desde la hoja, para alguien que no sabe derivar ni integrar".

Cubre los motores de desglose (cuentas, despejes y explicaciones de cálculo), las preguntas
conceptuales razonadas desde la hoja, la lección T-CONO, los problemas nuevos de radiación
(P26-4 y P26-5), la corrección de Q122 y la ruta con lo que anunció el profesor.
"""

from __future__ import annotations

import math
import pathlib
import sys
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.fisica3 import calculo, conceptos, desglose, despeje, registro, rutas  # noqa: E402
from tools.tutor_runtime.fisica3.formato import separa  # noqa: E402


def _filas(lid: str):
    j = registro.obtener(lid).a_json()
    return [a for p in j["pasos"] for t in p["tramos"] for a in t["acc"] if a.get("a") == "mate"]


def _texto(lid: str) -> str:
    return "\n".join(a["tex"] for a in _filas(lid))


class TestDesgloseDeCuentas(unittest.TestCase):
    def test_una_operacion_por_renglon_en_q049(self) -> None:
        t = _texto("Q049")
        self.assertIn(r"=200-(65{,}51)(1{,}197)", t)
        self.assertIn(r"=200-78{,}42", t)
        # cinco cifras: con I=2,671 el flujo saldría 15 397 W y la pizarra dice 15 396 W
        self.assertIn(r"=(6{,}3694)\ln\left(1{,}52102\right)", t)

    def test_planear_desglosa_de_adentro_hacia_afuera(self) -> None:
        p = desglose.planear(r"T=200-\frac{15\,396}{235}(1{,}197)", r"\boxed{T=121{,}6\ ^\circ\text{C}}")
        self.assertIsNotNone(p)
        self.assertEqual([f for f, _ in p.filas], [r"=200-(65{,}51)(1{,}197)", r"=200-78{,}42"])
        self.assertIn("Dividimos", separa(p.filas[0][1])[0])

    def test_doble_signo_no_miente_en_la_voz(self) -> None:
        p = desglose.planear(r"=-(235)(25{,}0-200)=41\,125", None)
        self.assertIsNotNone(p)
        voces = " ".join(separa(v)[0] for _, v in p.filas)
        self.assertIn("Menos por menos", voces)

    def test_ninguna_cuenta_expandible_queda_comprimida(self) -> None:
        """Tras aplicar el motor, ningún renglón del banco admite otro desglose."""
        pendientes = []
        for lid in registro.ids():
            filas = _filas(lid)
            for i, a in enumerate(filas):
                if a.get("rol") == "formula":
                    continue  # las casillas de la hoja se copian tal cual
                sig = filas[i + 1]["tex"] if i + 1 < len(filas) else None
                if desglose.planear(a["tex"], sig) is not None:
                    pendientes.append((lid, a["tex"][:80]))
        self.assertEqual(pendientes, [], f"Cuentas comprimidas: {pendientes[:5]}")


class TestDespejesYCalculo(unittest.TestCase):
    def test_despeje_del_cono_en_dos_renglones(self) -> None:
        filas = despeje.planear(r"\frac{H}{\pi}\cdot\frac{L}{r_1r_2}=k\,(T_1-T_2)", r"H=\frac{\pi k\,r_1r_2\,(T_1-T_2)}{L}")
        self.assertEqual([f for f, _ in filas], [r"H\,L=\pi\,r_1\,r_2\,k\,(T_1-T_2)"])

    def test_despeje_no_inventa_si_no_coincide(self) -> None:
        self.assertIsNone(despeje.planear(r"\frac{H}{\pi}\cdot\frac{L}{r_1r_2}=k\,(T_1-T_2)", r"H=\frac{k\,r_1r_2\,(T_1-T_2)}{L}"))

    def test_integrales_explicadas_donde_se_usan(self) -> None:
        notas = [a.get("txt", "") for p in registro.obtener("PR1-4").a_json()["pasos"] for t in p["tramos"] for a in t["acc"] if a.get("a") == "texto"]
        blob = " ".join(notas)
        self.assertIn("arriba", blob)          # cómo se leen los corchetes
        self.assertIn("du=b", blob.replace("\\,", "").replace(" ", ""))
        t = _texto("PR1-4")
        self.assertIn(r"u=a+bx=r(x)", t)
        self.assertIn(r"du=b\,dx\;\Rightarrow\;dx=\frac{du}{b}", t)

    def test_explicador_no_toca_la_teoria(self) -> None:
        self.assertEqual(calculo.explicar(registro.obtener("T-MAT2")), 0)


class TestConoDesdeCero(unittest.TestCase):
    def test_t_cono_completa(self) -> None:
        lec = registro.obtener("T-CONO")
        self.assertEqual(lec.tipo, "teoria")
        titulos = [p.titulo for p in lec.pasos]
        self.assertGreaterEqual(len(titulos), 15)
        self.assertTrue(any("receta" in t.lower() for t in titulos))
        t = _texto("T-CONO")
        self.assertIn(r"H=\frac{\pi k\,r_1r_2\,(T_1-T_2)}{L}", t)
        self.assertIn(r"du=b\,dx", t)

    def test_problemas_de_cono_abren_con_la_receta(self) -> None:
        for lid in ("PR1-4", "PR2-D1", "Q041", "Q043"):
            titulos = [p.titulo for p in registro.obtener(lid).pasos]
            self.assertTrue(any(t.startswith("Antes de calcular: cómo se ve y la receta") for t in titulos), lid)


class TestRadiacionNueva(unittest.TestCase):
    def test_p26_4_esfera_que_se_enfria(self) -> None:
        sigma, r, e = 5.67e-8, 0.05, 0.9
        A = 4 * math.pi * r ** 2
        he, ha = A * e * sigma * 473.15 ** 4, A * e * sigma * 298.15 ** 4
        dT = (he - ha) * 60.0 / (1.41 * 900.0)
        vals = [x["valor"] for x in registro.obtener("P26-4").respuestas]
        for mio, ref in zip(vals, [A, he, ha, he - ha, dT]):
            self.assertAlmostEqual(mio, ref, delta=abs(ref) * 1e-6)

    def test_p26_5_raiz_cuarta(self) -> None:
        sigma = 5.67e-8
        A = 2 * math.pi * 0.01 * 0.30
        T = (1500.0 / (A * 0.85 * sigma) + 293.15 ** 4) ** 0.25
        vals = {x["inciso"]: x["valor"] for x in registro.obtener("P26-5").respuestas}
        self.assertAlmostEqual(vals["b"], T, delta=0.01)
        self.assertAlmostEqual(vals["c"], 2.00 * 3.34e5 / 1500.0, delta=1e-6)
        self.assertEqual(vals["d"], 16.0)


class TestCorreccionesYRutas(unittest.TestCase):
    def test_q122_el_ciclo_si_es_posible(self) -> None:
        t = _texto("Q122")
        self.assertIn("sí es posible", t)
        self.assertNotIn("viola la 2.ª ley", t)

    def test_conceptuales_razonadas_desde_la_hoja(self) -> None:
        for lid in conceptos.AGREGADOS:
            lec = registro.obtener(lid)
            pasos = [p for p in lec.pasos if p.titulo == "Desde la hoja, paso a paso"]
            self.assertEqual(len(pasos), 1, lid)
            formulas = [a for t in pasos[0].tramos for a in t.acciones if isinstance(a, dict) and a.get("rol") == "formula"]
            self.assertGreaterEqual(len(formulas), 1, lid)

    def test_ruta_del_profe_primero(self) -> None:
        pub = rutas.publicas()
        self.assertEqual(pub[0]["id"], "profe_ii_2026")
        ids = {it["id"] for it in pub[0]["items"]}
        for obligatorio in ("P26-4", "P26-5", "PR1-2", "PR2-D7", "Q105", "Q103"):
            self.assertIn(obligatorio, ids)
        practicas = next(r for r in pub if r["id"] == "practicas")
        self.assertEqual(practicas["items"][0]["id"], "T-CONO")


if __name__ == "__main__":
    unittest.main()
