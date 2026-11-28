"""Anclas de origen y pasos intermedios para el reproductor de la lección."""

from __future__ import annotations

import re
from collections import defaultdict
from typing import Any, Dict

from . import desglose
from .reposicion_ii_2026 import ids_del_calendario

_ESPACIOS_TEX = re.compile(r"\\(?:[,;:! ]|left\b|right\b)")
_TEXTO_TEX = re.compile(r"\\(?:text|mathrm|operatorname)\{([^{}]*)\}")
_GRECIA = {
    "Delta": "Δ", "alpha": "α", "beta": "β", "gamma": "γ", "rho": "ρ",
    "omega": "ω", "lambda": "λ", "theta": "θ", "eta": "η", "mu": "μ",
}
_COMANDO_GRIEGO = re.compile(r"\\(" + "|".join(_GRECIA) + r")\b")
_VARIABLE = re.compile(r"(?:Δ[A-Za-z]|[A-Za-zαβγρηωλθμ])(?:_(?:\{[^{}]+\}|[A-Za-z0-9]))?")
_CON_SUBINDICE = re.compile(r"(?:Δ[A-Za-z]|[A-Za-zαβγρηωλθμ])_(?:\{[^{}]+\}|[A-Za-z0-9])")
_NUMERO = re.compile(r"(?<![\w.,^])[-−+]?\d+(?:[.,]\d+)?(?![\d.,])")
_ECUACIONES = re.compile(
    r"(?:\\[;,:]|\s)*\\(?:Longleftrightarrow|Leftrightarrow|Longrightarrow|Rightarrow|qquad)(?![A-Za-z])(?:\\[;,:]|\s)*"
)
_CON_DESGLOSE = frozenset(ids_del_calendario()) | {"PR2-D10"}
_RENGLONES_PREVIOS = {
    ("O26-9", "gamma-sustitucion"): ("duda", "Esta energía interna se obtuvo", ("ΔU_DA", "-253275")),
    ("PR2-D10", "pr2-e-menos-uno"): ("pr2-valor-e", "El valor de e aparece", ("e", "2.718")),
    ("PR2-D10", "pr2-cociente"): ("pr2-e-menos-uno", "El denominador se calculó", ("1.718",)),
}


def _limpia(tex: str) -> str:
    tex = _TEXTO_TEX.sub(lambda m: m[1].replace(" ", ""), tex)
    tex = _COMANDO_GRIEGO.sub(lambda m: _GRECIA[m[1]], tex)
    return _ESPACIOS_TEX.sub("", tex).replace("Δ ", "Δ").replace("{,}", ",").replace("−", "-")


def _variable(tex: str) -> str:
    return tex.replace("{", "").replace("}", "").replace("|", "/")


def _numeros(tex: str) -> set[str]:
    return {m[0].replace(",", ".").lstrip("+") for m in _NUMERO.finditer(tex)}


def _registra(filas: list, destino: str, fuente: dict, que: str, de: str, terminos: list[str]) -> None:
    if any(o.get("destino_id") == destino and o.get("fuente") == fuente for o in filas):
        return
    filas.append({"destino_id": destino, "fuente": fuente, "que": que, "de": de,
                  "terminos": [t for t in terminos if t]})


def _detalle(a: dict) -> list[dict]:
    """Separa igualdades y evalúa cuentas con el mismo motor aritmético de la pizarra."""
    if a.get("rol") == "formula":
        return []
    tex = desglose._quitar_boxed(a["tex"]) or a["tex"]
    if r"\begin" in tex:
        return []
    grupos = _ECUACIONES.split(tex)
    renglones = []
    ultimo = ""
    for g in grupos:
        partes = desglose._trozos_nivel0(g)
        cabeza = partes[0][1].strip()
        for op, parte in partes[1:]:
            if not parte.strip():
                continue
            ultimo = parte
            renglones.append((f"{cabeza} {op} {parte.strip()}".strip(), ""))
            cabeza = ""
    if len(renglones) >= 2 and len(grupos) == 1:
        plan = desglose.planear(tex, None)
        if plan and plan.filas:
            cierre = renglones[-1:] if plan.filas[-1][0].replace(" ", "") != renglones[-1][0].replace(" ", "") else []
            renglones = [(plan.cabeza, ""), *plan.filas, *cierre]
    elif len(renglones) == 1:
        leido = desglose._num_de(ultimo)
        if not leido or not desglose.contar_ops(leido[0]):
            return []
        try:
            estados = desglose.cadena(leido[0])
            for nodo, ops in estados:
                renglones.append(("= " + desglose.escribir(nodo), desglose.frase_paso(ops)))
            if leido[1] and len(renglones) > 1:
                valor, nota = renglones[-1]
                renglones[-1] = (f"{valor}{leido[1]}", nota)
        except (desglose.NoNumerico, ValueError, OverflowError, ZeroDivisionError):
            return []
    return [{"tex": tex, "nota": nota, "destino_id": a["id"]} for tex, nota in renglones]


def completar(j: Dict[str, Any]) -> None:
    """Vincula renglones con acciones existentes en el orden de la pizarra."""
    mapa = j.get("formulario_mapa") or {}
    celdas = {c["id"]: c for c in mapa.get("celdas", ())}
    por_accion = mapa.get("por_accion", {})
    lid = j["id"]
    revisar_detalle = lid in _CON_DESGLOSE
    definiciones: Dict[str, tuple[int, str, str, str | None]] = {}
    valores: Dict[str, set[str]] = defaultdict(set)
    dependencias: Dict[str, set[str]] = defaultdict(set)
    renglones = {}
    anterior = None
    for p in j["pasos"]:
        origen = list(p.get("origen", ()))
        lento = list(p.get("lento", ()))
        for tramo in p["tramos"]:
            dato = None
            for a in tramo["acc"]:
                if a["a"] == "dato":
                    dato = a
                    continue
                if a["a"] != "mate":
                    continue
                tex = a["tex"]
                destino = a["id"]
                sin_caja = desglose._quitar_boxed(tex) or tex
                limpio = _limpia(sin_caja)
                grupos = [t.strip() for _, t in desglose._trozos_nivel0(limpio)]
                izquierda = grupos[0]
                variable = _variable(izquierda) if len(grupos) > 1 and _VARIABLE.fullmatch(izquierda) else ""
                operandos = grupos[1:] if variable else grupos
                if len(operandos) > 1:
                    operandos = operandos[:-1]

                if dato is not None:
                    frase = dato["frase"]
                    numeros_del_enunciado = _numeros(dato.get("frase_original") or frase)
                    _registra(origen, destino, {"tipo": "enunciado", "dato_id": dato["id"]},
                              "$" + tex + "$", f"El enunciado indica «{frase}».",
                              [frase, variable, *sorted(numeros_del_enunciado & _numeros(limpio))])
                    dato = None

                # Las igualdades sin lado izquierdo continúan el cálculo inmediatamente anterior.
                if not izquierda and len(grupos) > 1 and anterior is not None:
                    paso, id_anterior, ecuacion = anterior
                    _registra(origen, destino, {"tipo": "renglon", "accion_id": id_anterior},
                              "$" + ecuacion + "$", f"Continuamos el cálculo señalado del paso {paso}.",
                              sorted(_numeros(limpio)))

                # Los subíndices identifican magnitudes como ΔU_DA sin confundirlas con ΔU_AC.
                parte = " ".join(operandos)
                nombres = {_variable(m[0]) for m in _CON_SUBINDICE.finditer(parte)}
                numeros = _numeros(parte)
                definicion = definiciones.get(variable)
                if definicion and definicion[3] in numeros:
                    paso, fuente, ecuacion, numero = definicion
                    _registra(origen, destino, {"tipo": "renglon", "accion_id": fuente},
                              "$" + ecuacion + "$", f"Usamos el valor del renglón señalado del paso {paso}.",
                              [variable, numero])
                fuentes = set(nombres)
                for n in numeros:
                    fuentes.update(valores.get(n, set()) & dependencias.get(variable, set()))
                for nombre in sorted(fuentes):
                    if nombre not in definiciones or nombre == variable:
                        continue
                    paso, fuente, ecuacion, numero = definiciones[nombre]
                    _registra(origen, destino, {"tipo": "renglon", "accion_id": fuente},
                              "$" + ecuacion + "$", f"Se obtiene en el paso {paso}, en el renglón señalado.",
                              [nombre, *([numero] if numero and numero in numeros else [])])
                if variable and nombres:
                    dependencias[variable] = nombres
                previo = _RENGLONES_PREVIOS.get((lid, destino))
                if previo and previo[0] in renglones:
                    fuente, explicacion, terminos = previo
                    paso, ecuacion = renglones[fuente]
                    _registra(origen, destino, {"tipo": "renglon", "accion_id": fuente},
                              "$" + ecuacion + "$", f"{explicacion} en el paso {paso}.", list(terminos))

                coincidencia = por_accion.get(destino, {})
                ids = [] if coincidencia.get("hereda") else list(coincidencia.get("celdas", ()))
                for id in ids:
                    if id not in celdas:
                        continue
                    c = celdas[id]
                    referencia = (f"La ecuación de {c['nombre']} está en el catálogo, entrada {id}."
                                  if mapa.get("hoja_real") is False else f"Buscá {c['nombre']} en la casilla {id}.")
                    _registra(origen, destino, {"tipo": "formulario", "celda_id": id},
                              "$" + c["tex"] + "$", referencia,
                              [id, variable, *sorted(_numeros(_limpia(c["tex"])))])

                if variable and not re.search(r"\\qquad|\\Rightarrow", grupos[-1]):
                    final = desglose._num_de(desglose._trozos_nivel0(sin_caja)[-1][1])
                    if final and not desglose.contar_ops(final[0]):
                        numeros_final = _numeros(grupos[-1])
                        numero = next(iter(numeros_final)) if len(numeros_final) == 1 else None
                        anterior_var = definiciones.get(variable)
                        if anterior_var and anterior_var[3]:
                            valores[anterior_var[3]].discard(variable)
                            if numero == anterior_var[3] and a.get("rol") == "resultado":
                                paso, id_origen, ecuacion, _ = anterior_var
                                _registra(origen, destino, {"tipo": "renglon", "accion_id": id_origen},
                                          "$" + ecuacion + "$", f"El valor se calculó en el renglón señalado del paso {paso}.",
                                          [variable, numero])
                        definiciones[variable] = (p["n"], destino, tex, numero)
                        if numero:
                            valores[numero].add(variable)
                renglones[destino] = (p["n"], tex)
                anterior = (p["n"], destino, tex)
                if revisar_detalle and not p.get("lento"):
                    lento.extend(_detalle(a))
        if origen:
            p["origen"] = origen
        if lento:
            p["lento"] = lento
