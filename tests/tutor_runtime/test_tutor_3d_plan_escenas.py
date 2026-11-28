"""Las escenas 3D del plan de la reposición no mienten ni faltan.

1. Cada lección del calendario (76) tiene escena 3D o una excepción escrita en SIN_ESCENA_3D.
2. Cada número físico escrito a mano para el 3D (apoyos_reposicion._PARAMETROS) sale del enunciado
   de su lección (con un cambio de unidad 10^k), o se deriva de él con la cuenta escrita abajo, o la
   lección no trae números en el enunciado y la escena lo dice («Valores de ejemplo»).
3. La suma de pedacitos del 3D llega a la respuesta oficial en las lecciones con integral.

Se escribió después de encontrar números inventados en el 3D (27-nov-2026): T-CONO decía 20 °C
donde el enunciado dice 0 °C, Q051 decía 25 °C (son 30 °C), P2E26-8 y RP-24I-2 tenían otra
profundidad, RP-24II-1 y RP-25II-1 otra masa y otra amplitud, y RP-23II-2 un tubo 6 veces más grueso.
"""

from __future__ import annotations

import math
import pathlib
import re
import sys
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.tutor_runtime.fisica3 import apoyos_reposicion as ar  # noqa: E402
from tools.tutor_runtime.fisica3 import procedencia, registro  # noqa: E402
from tools.tutor_runtime.fisica3 import reposicion_ii_2026 as reposicion  # noqa: E402

# Claves que no son datos físicos: modos, conteos, sentidos, nombres.
NO_FISICAS = {"armonico", "sentido", "fraccion", "comparar", "modo", "forma", "geo", "onda", "traste",
              "vertical", "igual", "nombres", "n", "eje", "caso", "tipo", "ejemplo"}

# Valores que no están escritos tal cual en el enunciado pero salen de él con una cuenta.
DERIVADOS = {
    ("RP-24II-1", "k"): "k = m·ω² con m = 0,600 kg y ω = 12π rad/s",
    ("RP-25II-1", "k"): "k = m·ω² con ω = 2π/T y T = 4,8 s (dos ceros seguidos de v a 1,1 s y 3,5 s)",
    ("RP-25II-1", "amplitud"): "A = v_máx/ω = 1,80/(2π/4,8)",
    ("RP-25II-1", "x0"): "x0 = A·cos(π/2 − ω·2,3 s): v es mínima en t = 2,3 s",
    ("P1E26-3", "amplitudes"): "«la mitad de amplitud»: proporción 0,5 a 1",
    ("P2O26-2", "hundido"): "fracción sumergida = ρ_madera/ρ_líquido = 600/1000 y 600/800",
    ("P2E26-9", "diametros"): "D = 2R con R = 2,00 mm",
    ("SUF26-3", "e"): "alto de cada capa = volumen/base: 12,87/3,46 y 8,58/3,46",
    ("T-INT1", "H"): "la pared de T-INT0 (4,00 m de agua), que T-INT1 integra al final",
    ("T-INT1", "ancho"): "la pared de T-INT0 (2,00 m de ancho)",
    ("Q051", "A"): "A = 4,00 m × 3,00 m",
}

# Respuesta oficial de cada lección con integral en el 3D (valor y unidad que escribe la escena).
TOTALES_OFICIALES = {
    "T-INT0": 156_800.0,      # F = ρg·L·H²/2 con L = 2,00 m y H = 4,00 m
    "RP-25II-2": 4.2875e8,    # F_neta (respuesta A)
    "P2E26-8": 1896.3,        # F sobre la ventana (respuesta c)
    "E2-8": 529.69,           # F⊥ sobre la puerta semicircular
    "RP-24I-2": 78_400.0 + 59_132.98,  # F_rect + F_parab (respuesta C)
}


def _numeros(texto: str):
    s = texto.replace("{,}", ",").replace("\\,", "").replace("\\ ", " ").replace("−", "-")
    s = re.sub(r"(\d)\s+(\d{3})(?!\d)", r"\1\2", s)
    for m in re.finditer(r"(-?\d+(?:,\d+)?)(?:\s*(?:\\times|×|x)\s*10\^?\{?(-?\d+)\}?)?", s):
        x = float(m.group(1).replace(",", "."))
        if m.group(2):
            x *= 10 ** int(m.group(2))
        yield x


def _coincide(v: float, nums) -> bool:
    if v == 0:
        return True
    return any(x and math.isclose(abs(v), abs(x) * 10 ** k, rel_tol=2e-3) for x in nums for k in range(-6, 7))


def _hojas(obj, clave=""):
    if isinstance(obj, dict):
        for k, v in obj.items():
            yield from _hojas(v, k)
    elif isinstance(obj, (list, tuple)):
        for v in obj:
            yield from _hojas(v, clave)
    elif isinstance(obj, (int, float)) and not isinstance(obj, bool):
        yield clave, float(obj)


class TestEscenas3DDelPlan(unittest.TestCase):
    def test_cada_leccion_del_plan_tiene_escena_o_excepcion(self) -> None:
        faltan = []
        for lid in reposicion.ids_del_calendario():
            j = registro.obtener(lid).a_json()
            procedencia.completar(j)
            apoyo = ar.apoyo_leccion(j)
            if apoyo is None:
                if not ar.SIN_ESCENA_3D.get(lid, "").strip():
                    faltan.append(lid)
                continue
            self.assertIn(apoyo["modelo"], ar._ESCENAS, lid)
        self.assertEqual(faltan, [], f"Lecciones del plan sin escena 3D ni excepción escrita: {faltan}")
        for lid, razon in ar.SIN_ESCENA_3D.items():
            self.assertGreater(len(razon), 20, f"{lid}: la excepción necesita una razón escrita")

    def test_numeros_del_3d_salen_del_enunciado(self) -> None:
        problemas = []
        for lid, params in ar._PARAMETROS.items():
            lec = registro.obtener(lid).a_json()
            nums = list(_numeros(f"{lec.get('enunciado') or ''} {lec.get('titulo') or ''}"))
            for clave, valor in _hojas(params):
                if clave in NO_FISICAS or params.get("ejemplo") or (lid, clave) in DERIVADOS:
                    continue
                if not _coincide(valor, nums):
                    problemas.append(f"{lid}.{clave} = {valor}")
        self.assertEqual(problemas, [], "Números del 3D que no salen del enunciado (ni de una cuenta escrita en DERIVADOS)")

    def test_sin_numeros_en_el_enunciado_la_escena_dice_ejemplo(self) -> None:
        for lid, params in ar._PARAMETROS.items():
            lec = registro.obtener(lid).a_json()
            claves = {k for k, _ in _hojas(params) if k not in NO_FISICAS}
            # Con la procedencia escrita en DERIVADOS (p. ej. T-INT1 usa la pared de T-INT0) no es «ejemplo».
            if claves and all((lid, k) in DERIVADOS for k in claves):
                continue
            if claves and not list(_numeros(lec.get("enunciado") or "")):
                self.assertTrue(params.get("ejemplo"), f"{lid}: enunciado sin números → la escena debe decir «Valores de ejemplo»")

    def test_suma_de_franjas_llega_a_la_respuesta_oficial(self) -> None:
        """La misma geometría que usa el 3D (geometria_pared.js), en Python: si una cambia, se nota acá."""
        def modelo(p):
            G = 9.8
            capas = p.get("capas")
            H = sum(c["e"] for c in capas) if capas else float(p.get("H") or p.get("alto") or 4)
            rho = float(p.get("rho") or 1000)

            def presion(h):
                if not capas:
                    return rho * G * h
                resto, pr = h, 0.0
                for c in capas:
                    tramo = min(resto, c["e"])
                    pr += c["rho"] * G * tramo
                    resto -= tramo
                    if resto <= 0:
                        break
                return pr

            forma = p.get("forma", "rectangulo")
            h0, h1 = 0.0, H
            if forma == "ventana":
                h0, h1 = H - float(p.get("y_sup", H)), H - float(p.get("y_inf", 0))
                L = lambda h: float(p.get("ancho", 1))  # noqa: E731
            elif forma == "semicirculo":
                R = float(p.get("R") or p.get("alto") or 0.3)
                h0 = float(p.get("h_borde", capas[0]["e"] if capas and len(capas) > 1 else 0))
                h1 = min(H, h0 + R)
                L = lambda h: 2 * math.sqrt(max(0.0, R * R - (h - h0) ** 2))  # noqa: E731
            elif forma == "trapecio":
                bs, bi = float(p["ancho_sup"]), float(p["ancho_inf"])
                L = lambda h: bs + (bi - bs) * h / H  # noqa: E731
            elif forma == "mixta":
                r, k = float(p.get("rect", 1)), float(p.get("k", 2))
                L = lambda h: r + math.sqrt(max(0.0, (H - h) / k))  # noqa: E731
            else:
                L = lambda h: float(p.get("ancho", 1))  # noqa: E731
            n = 2000
            d = (h1 - h0) / n
            f = lambda h: presion(h) * L(h)  # noqa: E731
            s = f(h0) + f(h1) + sum(f(h0 + i * d) * (4 if i % 2 else 2) for i in range(1, n))
            return s * d / 3

        for lid, oficial in TOTALES_OFICIALES.items():
            j = registro.obtener(lid).a_json()
            procedencia.completar(j)
            apoyo = ar.apoyo_leccion(j)
            self.assertEqual(apoyo["modelo"], "compuerta", lid)
            total = modelo(apoyo["parametros"])
            self.assertAlmostEqual(total / oficial, 1.0, delta=0.005, msg=f"{lid}: el 3D suma {total:.1f} y la respuesta oficial es {oficial}")


if __name__ == "__main__":
    unittest.main()
