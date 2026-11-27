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

# Lo que anunció el profesor para el III parcial II-2026: "un ejercicio de calorimetría o radiación y uno de ciclos"
_PROFE: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("1. Calorimetría (desarrollo)", [
        ("T-CAL", "Teoría en cinco minutos"),
        ("PR1-2", "Hielo a −20 °C + vapor: ¿se funde todo?"),
        ("PR2-D7", "Fusión parcial: queda hielo y T = 0 °C"),
        ("PR2-D12", "Calentador eléctrico: balance por etapas"),
        ("PR1-1", "Cuatro cuerpos: ΣQ = 0 y masa de hierro"),
        ("Q022", "Examen II-2023: hielo + vapor + hierro"),
        ("PR2-D8", "Vapor que se condensa en agua"),
        ("E6", "Hielo, agua y vapor juntos"),
    ]),
    ("2. Radiación (desarrollo)", [
        ("T-RAD", "Teoría en cinco minutos"),
        ("P26-4", "Nuevo: esfera negra que se enfría (neta + calor)"),
        ("P26-5", "Nuevo: despejar T con raíz cuarta + fundir hielo"),
        ("Q068", "Examen I-2026: radiación neta a 97 °C"),
        ("Q070", "Cubo de agua que se enfría por radiación"),
        ("E10", "Filamento de un bombillo"),
        ("PR2-S10", "Selección: 300 K contra 600 K"),
    ]),
    ("3. Ciclos (desarrollo)", [
        ("T-PV", "Teoría en cinco minutos"),
        ("Q105", "Examen I-2026 ordinario: helio, 3 procesos"),
        ("Q103", "Examen I-2026 extraordinario: N₂, refrigerador"),
        ("Q110", "Reposición I-2026: refrigerador de helio"),
        ("PR1-5", "Recta P = αV con integral"),
        ("PR1-6", "Antihorario: consume trabajo"),
        ("PR2-D6", "Cuatro procesos, diatómico"),
        ("PR2-D4", "Triangular monoatómico"),
        ("PR2-D11", "Información incompleta: hallar Cp y γ"),
        ("PR2-D9", "Ciclo Otto completo"),
        ("Q100", "Examen II-2025: dos isotermas y dos isocoras"),
    ]),
]

# Las dos prácticas oficiales del II-2026, en el orden de los PDF
_PRACTICAS: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("Antes de empezar: cálculo desde cero", [("T-CONO", "El tronco de cono desde cero"), ("T-MAT1", "Pendiente e integral"), ("T-MAT2", "Las cinco integrales del parcial"), ("T-MAT3", "Logaritmos y separar variables")]),
    ("Práctica del Parcial 3", [(f"PR1-{i}", "") for i in range(1, 7)]),
    ("Práctica del tercer examen · selección única", [(f"PR2-S{i}", "") for i in range(1, 15)]),
    ("Práctica del tercer examen · desarrollo y desafíos", [(f"PR2-D{i}", "") for i in range(1, 13)]),
]


# Top Estrategia II-2026: evidencia cruzada de Patrones.txt + auditoría de 17 exámenes + fórmulas nuevas
_ESTRATEGIA_II_2026: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("1. Conducción con integrales (alta probabilidad)", [
        ("T-CONO", "El tronco de cono desde cero (empiece aquí)"),
        ("T-MAT2", "Las cinco integrales del parcial"),
        ("PR1-4", "Cono truncado + fusión de hielo"),
        ("PR2-D2", "Área lineal A₀(1+x/L) y gradiente"),
        ("PR2-D10", "Área exponencial A₀ e^(−x/L)"),
        ("PR1-3", "Cilindro 2 capas + condensación"),
        ("Q049", "Barra piramidal A(y) variable"),
    ]),
    ("2. Ciclos con integrales e información incompleta", [
        ("PR1-5", "Recta P = αV con integral y dQ > 0"),
        ("PR1-6", "Ciclo antihorario (W < 0, trampa)"),
        ("PR2-D11", "Rectángulo con información incompleta"),
        ("PR2-D4", "Ciclo triangular monoatómico"),
        ("PR2-D5", "Hallar γ y tipo de gas en adiabática"),
    ]),
    ("3. Calorimetría con trampa de fusión parcial", [
        ("PR1-2", "Hielo −20 °C + vapor (fusión total)"),
        ("PR2-D7", "Hierro en hielo + agua (fusión parcial)"),
        ("PR2-D12", "Calentador eléctrico por etapas"),
        ("PR1-1", "4 cuerpos y masa de hierro"),
    ]),
    ("4. Fórmulas nuevas del formulario II-2026", [
        ("PR2-D9", "Ciclo Otto completo + trampa inversa r"),
        ("P26-1", "Esfuerzo térmico F/A = −YαΔT (nueva)"),
        ("P26-2", "Escala Fahrenheit y ΔT (nueva)"),
        ("P26-3", "Teoría cinética con M = N_A m y cm³"),
        ("PR2-S1", "Selección · calor específico en °F"),
        ("PR2-S13", "Selección · barra enfriada sujeta"),
    ]),
]


_TAREAS_TERMO: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("Tarea 4 · I-2024 (calorimetría y esfera hueca)", [
        ("T4-24I-1", "Vapor + hielo + cobre (3 fases)"),
        ("T4-24I-2", "Esfera hueca radial (corrige errata oficial d)"),
    ]),
    ("Tarea 3 · II-2024 (dilatación exacta y té verde)", [
        ("T3-24II-1", "Dilatación exponencial vs lineal (Maclaurin)"),
        ("T3-24II-2", "Fracción derretida α(T) y umbral 79,5 °C"),
    ]),
    ("Tarea 3 · I-2025 (dilatación 3D/catenaria y calorimetría)", [
        ("T3-25I-1", "β = 3α, bloque anisotrópico α(T) y catenaria"),
        ("T3-25I-2", "Mezclas binarias y ternaria con etanol"),
    ]),
    ("Tarea 3 · II-2025 (4 problemas ≤ 20.5)", [
        ("T3-25II-1", "Aluminio fundido en agua + masa mínima"),
        ("T3-25II-2", "Ducto cerámico cilíndrico y perfil logarítmico"),
        ("T3-25II-3", "Primera ley e independencia de trayectoria"),
        ("T3-25II-4", "Ciclo refrigerador monoatómico (corrige K = 2,59)"),
    ]),
    ("Tarea 3 · I-2026 (8 problemas ≤ 20.5; #3 y #4 excluidos)", [
        ("T3-26I-1", "Fusión parcial a −10 °C + masa para 25 °C"),
        ("T3-26I-2", "Ciclo completo con cierre isobárico C→A"),
        ("T3-26I-5", "Isobárica + isocórica en gas diatómico"),
        ("T3-26I-6", "Pistón vertical con resorte (trabajo integral)"),
        ("T3-26I-7", "Ciclo triangular antihorario (Q = W = −300 J)"),
        ("T3-26I-8", "Tubería 2 capas acero + poliestireno"),
        ("T3-26I-9", "Esfera, cilindro y cubo de hierro (corrige SI)"),
        ("T3-26I-10", "Cono truncado doble variable A(x) y k(T) = k₀T"),
    ]),
]

_RECOMENDADOS_P1_P2: List[Tuple[str, List[Tuple[str, str]]]] = [
    ("Ejercicios Recomendados 1 · Parcial 1 (MAS y ondas)", [(f"E1-{i}", "") for i in range(1, 17)]),
    ("Ejercicios Recomendados 2 · Parcial 2 (elasticidad y fluidos)", [(f"E2-{i}", "") for i in range(1, 15)]),
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
            "id": "profe_ii_2026",
            "titulo": "Lo que dijo el profe: calorimetría o radiación + un ciclo",
            "descripcion": "El profesor anunció un ejercicio de calorimetría o radiación y uno de ciclos. Aquí están los mejores de cada tema, del más directo al más largo.",
            "items": _construye(_PROFE),
        },
        {
            "id": "practicas",
            "titulo": "Prácticas del III parcial (II-2026)",
            "descripcion": "Los 32 problemas de las dos prácticas oficiales de este semestre, resueltos paso a paso desde la hoja.",
            "items": _construye(_PRACTICAS),
        },
        {
            "id": "estrategia_ii_2026",
            "titulo": "Top 12 Estrategia II-2026 (Patrones + Fórmulas nuevas)",
            "descripcion": "Ruta priorizada con la auditoría de los 17 exámenes históricos, las 2 prácticas y las 4 casillas nuevas de la hoja II-2026.",
            "items": _construye(_ESTRATEGIA_II_2026),
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
        {
            "id": "tareas_termo_2024_2026",
            "titulo": "Tareas de Termodinámica ≤ 20.5 (2024–2026)",
            "descripcion": "Los 18 problemas vigentes (≤ 20.5) de las 5 tareas oficiales de cátedra, con corrección documentada de erratas.",
            "items": _construye(_TAREAS_TERMO),
        },
        {
            "id": "recomendados_p1_p2_ii_2026",
            "titulo": "Recomendados 1 y 2 II-2026 (Parciales 1 y 2 · Reposición)",
            "descripcion": "Los 16 ejercicios de MAS/ondas y 14 de elasticidad/fluidos de Documentos II-2026, derivados desde sus hojas oficiales.",
            "items": _construye(_RECOMENDADOS_P1_P2),
        },
    ]
