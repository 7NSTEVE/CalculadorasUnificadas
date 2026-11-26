"""Registro de lecciones: id -> constructor, con construcción perezosa y validación."""

from __future__ import annotations

import importlib
import threading
from typing import Any, Callable, Dict, List, Optional

from .dsl import Leccion

_CONSTRUCTORES: Dict[str, Callable[[], Leccion]] = {}
_CACHE: Dict[str, Leccion] = {}
_ALIAS: Dict[str, str] = {}
_CANDADO = threading.RLock()
_CARGADO = False

MODULOS = [
    "banco_recomendados",
    "banco_dilatacion",
    "banco_calorimetria",
    "banco_conduccion",
    "banco_radiacion",
    "banco_gases",
    "banco_primera_ley",
    "banco_ciclos",
    "banco_ciclos2",
    "banco_maquinas",
    "banco_entropia",
    "banco_practica",
    "banco_blindaje_ii_2026",
    "teoria",
]


def leccion(id: str) -> Callable[[Callable[[], Leccion]], Callable[[], Leccion]]:
    """Decorador: registra la función que construye la lección `id`."""

    def deco(fn: Callable[[], Leccion]) -> Callable[[], Leccion]:
        if id in _CONSTRUCTORES:
            raise RuntimeError(f"Lección duplicada: {id}")
        _CONSTRUCTORES[id] = fn
        return fn

    return deco


def igual_que(id: str, original: str, **cambios: Any) -> None:
    """Registra un ejercicio repetido (mismo enunciado en otro documento)."""

    def construye() -> Leccion:
        import copy

        base = copy.deepcopy(obtener(original))
        base.id = id
        for k, v in cambios.items():
            setattr(base, k, v)
        if original not in base.alias:
            base.alias.append(original)
        return base

    if id in _CONSTRUCTORES:
        raise RuntimeError(f"Lección duplicada: {id}")
    _CONSTRUCTORES[id] = construye


def cargar() -> None:
    global _CARGADO
    with _CANDADO:
        if _CARGADO:
            return
        for m in MODULOS:
            try:
                importlib.import_module(f"{__package__}.{m}")
            except ModuleNotFoundError as exc:
                if exc.name and exc.name.endswith(m):
                    continue
                raise
        _CARGADO = True


def obtener(id: str) -> Leccion:
    cargar()
    with _CANDADO:
        clave = id if id in _CONSTRUCTORES else _ALIAS.get(id.upper(), id.upper())
        if clave not in _CONSTRUCTORES:
            raise KeyError(id)
        if clave not in _CACHE:
            lec = _CONSTRUCTORES[clave]()
            if lec.id != clave:
                raise RuntimeError(f"La lección {clave} se llama {lec.id}")
            _CACHE[clave] = lec
        return _CACHE[clave]


def ids() -> List[str]:
    cargar()
    return list(_CONSTRUCTORES)


def existe(id: str) -> bool:
    cargar()
    return id in _CONSTRUCTORES


def registrar_alias(alias: str, id: str) -> None:
    _ALIAS[alias.upper()] = id


# Código del programa del curso -> grupo con el que se muestra
_GRUPO_POR_CODIGO = {
    "4.1": "dilatacion", "4.2": "dilatacion", "4.3": "dilatacion", "4.4": "dilatacion",
    "4.5": "calorimetria", "4.6": "calorimetria", "4.7": "calorimetria", "4.8": "calorimetria",
    "5.1": "conduccion", "5.2": "conduccion", "5.3": "conduccion", "5.4": "conduccion", "5.5": "conduccion",
    "5.6": "conduccion", "5.7": "conduccion", "5.8": "radiacion", "5.9": "radiacion",
    "6.1": "gases", "6.2": "gases", "6.3": "gases", "6.4": "gases", "6.5": "gases", "6.6": "gases",
    "6.7": "primera_ley", "6.8": "primera_ley", "6.9": "primera_ley", "6.10": "primera_ley",
    "6.11": "ciclos", "6.12": "entropia", "6.13": "entropia", "6.14": "maquinas", "6.15": "maquinas",
    "6.16": "entropia", "6.17": "entropia",
}


def grupo(tema: str) -> str:
    return _GRUPO_POR_CODIGO.get(tema, tema or "otros")


def resumen(id: str) -> Optional[Dict[str, Any]]:
    try:
        lec = obtener(id)
    except KeyError:
        return None
    return {
        "id": lec.id, "titulo": lec.titulo, "tema": grupo(lec.tema), "codigo": lec.tema, "tipo": lec.tipo, "nivel": lec.nivel,
        "fuente": lec.fuente, "pasos": len(lec.pasos), "claves": lec.claves, "aviso": bool(lec.aviso),
        "min": minutos(lec),
    }


_MINUTOS: Dict[str, int] = {}


def minutos(lec: Leccion) -> int:
    """Duración aproximada de la lección hablada, en minutos."""
    if lec.id not in _MINUTOS:
        car = sum(len(p["habla"]) for p in lec.a_json()["pasos"])
        _MINUTOS[lec.id] = max(1, round(car / 14.5 / 60 + 0.12 * len(lec.pasos)))
    return _MINUTOS[lec.id]
