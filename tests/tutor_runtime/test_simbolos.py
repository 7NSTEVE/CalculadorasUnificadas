"""Qué es cada letra: el diccionario de símbolos que acompaña a cada fórmula."""

from __future__ import annotations

import pathlib
import re
import sys
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.fisica3 import registro, simbolos  # noqa: E402
from tools.tutor_runtime.fisica3.formulario_ii_2026 import catalogo_formulario  # noqa: E402


def _todos(lid: str):
    m = simbolos.mapa_leccion(registro.obtener(lid).a_json())
    return {s: (que, u) for items in m["por_accion"].values() for s, que, u in items}


class TestSimbolos(unittest.TestCase):
    def test_todas_las_lecciones_tienen_letras_explicadas_sin_latex(self) -> None:
        sin = []
        for lid in registro.ids():
            j = registro.obtener(lid).a_json()
            m = simbolos.mapa_leccion(j)
            items = [x for v in m["por_accion"].values() for x in v]
            if not items and j.get("tipo") != "seleccion":
                sin.append(lid)
            for s, que, u in items:
                self.assertTrue(s and que, (lid, s))
                self.assertNotRegex(que + u, r"[\\{}$]", f"{lid}: LaTeX crudo en '{que}' / '{u}'")
        self.assertEqual(sin, [], f"Lecciones sin ninguna letra explicada: {sin}")

    def test_letras_ambiguas_se_leen_segun_el_tema(self) -> None:
        e18 = _todos("E18")  # refrigerador con estados A-D: Q_C es el foco frío, T_C el estado C
        self.assertIn("foco frío", e18["Q_C"][0])
        self.assertIn("estado C", e18["T_C"][0])
        pr13 = _todos("PR1-3")  # tubería con dos capas
        self.assertIn("interfaz", pr13["T_i"][0])
        self.assertIn("resistencia térmica", pr13["R_1"][0])
        self.assertIn("calor latente", simbolos.simbolos_de(r"Q=\pm mL", "conduccion")[2][1])
        e20 = _todos("E20")  # dos máquinas A y B
        self.assertIn("máquina A", e20["e_A"][0])
        self.assertIn("foco frío de la máquina B", e20["Q_C,B"][0])

    def test_unidades_quedan_fuera_y_diferenciales_se_reconocen(self) -> None:
        f = simbolos.fichas(r"H=-kA\frac{dT}{dx}=300\ \text{K}\cdot 5\ \text{W}")
        self.assertIn(("dT", ""), f)
        self.assertIn(("dx", ""), f)
        self.assertNotIn(("K", ""), f)
        self.assertNotIn(("W", ""), f)

    def test_las_42_casillas_de_la_hoja(self) -> None:
        cel = {c["id"]: simbolos.simbolos_de(c["tex"], simbolos.grupo_celda(c["id"])) for c in catalogo_formulario()["celdas"]}
        self.assertEqual(len(cel), 42)
        f2c1 = {s: q for s, q, _ in cel["F2C1"]}
        self.assertIn("lado frío", f2c1["T_C"])
        self.assertIn("conductividad", f2c1["k"])
        f10c2 = {s: q for s, q, _ in cel["F10C2"]}
        self.assertIn("foco frío", f10c2["Q_c"])
        self.assertTrue(any("Stefan" in q for _, q, _ in cel["F13C3"]))
        vacias = [k for k, v in cel.items() if not v and k not in ("F14C1", "F14C2")]
        self.assertEqual(vacias, [])


if __name__ == "__main__":
    unittest.main()
