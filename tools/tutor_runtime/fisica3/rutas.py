"""Rutas de estudio: en qué orden conviene ver las lecciones y por qué.

Solo se publican las lecciones que existen; así una ruta nunca enlaza a algo vacío.
"""

from __future__ import annotations

from typing import Any, Dict, List, Tuple

from . import registro

# (bloque, [(id, nota)])
_MANANA: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("1. Dilatación", [("T-DIL", ""), ("E2", ""), ("E1", ""), ("E3", ""), ("E4", "")]),
    ("2. Calor y cambios de fase", [("T-CAL", ""), ("E7", ""), ("E6", ""), ("E5", "")]),
    ("3. Conducción", [("T-CON", ""), ("E9", ""), ("E8", "")]),
    ("4. Radiación", [("T-RAD", ""), ("E10", ""), ("E11", ""), ("E12", "")]),
    ("5. Gases", [("T-GAS", ""), ("E13", ""), ("E14", "")]),
    ("6. Ciclos y primera ley", [("T-PV", ""), ("E15", ""), ("E16", "")]),
    ("7. Máquinas, refrigeradores y Otto", [("T-MAQ", ""), ("E17", ""), ("E18", ""), ("E19", ""), ("E20", ""), ("E21", "")]),
]


def publicas() -> List[Dict[str, Any]]:
    items = []
    for bloque, ls in _MANANA:
        for id, nota in ls:
            if registro.existe(id):
                items.append({"id": id, "nota": nota, "bloque": bloque})
    return [{
        "id": "manana", "titulo": "Ruta del examen",
        "descripcion": "De lo más sencillo a lo más largo, con lo que más sale en el III Parcial.",
        "items": items,
    }]
