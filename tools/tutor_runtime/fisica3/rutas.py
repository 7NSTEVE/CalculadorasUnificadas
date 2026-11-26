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
    ("3. Conducción y área/k variable", [("T-CON", ""), ("E9", ""), ("E8", ""), ("Q049", "Prioridad crítica · A(y) variable")]),
    ("4. Radiación", [("T-RAD", ""), ("E10", ""), ("E11", ""), ("E12", "")]),
    ("5. Gases", [("T-GAS", ""), ("E13", ""), ("E14", "")]),
    ("6. Ciclos P–V y primera ley", [("T-PV", ""), ("E15", ""), ("E16", ""), ("Q111", "Prioridad crítica · ciclo integrador")]),
    ("7. Máquinas, refrigeradores y Otto", [("T-MAQ", ""), ("E17", ""), ("E18", ""), ("E19", ""), ("E20", ""), ("E21", "")]),
]

_CRITICOS: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("Conducción variable e integrales", [("T-CON", ""), ("E8", ""), ("Q049", ""), ("Q050", ""), ("Q057", "")]),
    ("Ciclos P–V integradores", [("T-PV", ""), ("E16", ""), ("Q111", ""), ("Q109", ""), ("Q110", "")]),
    ("Calorimetría y máquinas clave", [("E5", ""), ("Q038", ""), ("E17", ""), ("E21", ""), ("Q128", "")]),
]

# Las dos prácticas oficiales del II-2026, en el orden de los PDF
_PRACTICAS: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("Práctica del Parcial 3", [(f"PR1-{i}", "") for i in range(1, 7)]),
    ("Práctica del tercer examen · selección única", [(f"PR2-S{i}", "") for i in range(1, 15)]),
    ("Práctica del tercer examen · desarrollo y desafíos", [(f"PR2-D{i}", "") for i in range(1, 13)]),
]


def _construye(bloques: List[Tuple[str, List[Tuple[str, str]]]]) -> List[Dict[str, Any]]:
    items = []
    for bloque, ls in bloques:
        for id, nota in ls:
            if registro.existe(id):
                items.append({"id": id, "nota": nota, "bloque": bloque})
    return items


def publicas() -> List[Dict[str, Any]]:
    return [
        {
            "id": "practicas",
            "titulo": "Prácticas del III parcial (II-2026)",
            "descripcion": "Los 32 problemas de las dos prácticas oficiales de este semestre, resueltos paso a paso desde la hoja.",
            "items": _construye(_PRACTICAS),
        },
        {
            "id": "manana",
            "titulo": "Ruta del examen",
            "descripcion": "De lo más sencillo a lo más largo, con los 21 Recomendados, Q049 y Q111 (sin entropía).",
            "items": _construye(_MANANA),
        },
        {
            "id": "criticos",
            "titulo": "Prioridad crítica (conducción variable y ciclos P–V)",
            "descripcion": "Selección concentrada en integrales de Fourier, área/k variable y ciclos termodinámicos completos.",
            "items": _construye(_CRITICOS),
        },
    ]
