"""Verificación aritmética y simbólica determinista sin eval().

Recalcula en Python las igualdades numéricas que escribe la IA en la pizarra o en una lección
generada, detecta discrepancias reales (como 121,4 vs 121,6) y limpia LaTeX crudo de la voz.
"""

from __future__ import annotations

import ast
import math
import operator
import re
from typing import Any, Callable, Dict, List, Optional, Tuple

_CONSTANTES: Dict[str, float] = {
    "pi": math.pi,
    "e": math.e,
    "R": 8.314,
    "sigma": 5.670374419e-8,
    "k_B": 1.380649e-23,
    "N_A": 6.02214076e23,
    "g": 9.8,
    "atm": 101325.0,
}

_FUNCIONES: Dict[str, Callable[..., float]] = {
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "sinh": math.sinh,
    "cosh": math.cosh,
    "tanh": math.tanh,
    "sqrt": math.sqrt,
    "log": math.log,
    "ln": math.log,
    "log10": math.log10,
    "exp": math.exp,
    "abs": abs,
    "fabs": math.fabs,
    "pow": pow,
    "radians": math.radians,
    "degrees": math.degrees,
}

_BIN_OPS: Dict[type, Callable[[float, float], float]] = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
}

_UNARY_OPS: Dict[type, Callable[[float], float]] = {
    ast.UAdd: operator.pos,
    ast.USub: operator.neg,
}

_UNIDADES_FINAL_RE = re.compile(
    r"(?:\s*(?:\\text\s*\{[^{}]*\}|\\mathrm\s*\{[^{}]*\}|\\,|\s)*"
    r"(?:°\s*[CFK]|\^\{?\\circ\}?\s*[CFK]?|%|k?J|k?W|M?W|m?W|k?Pa|MPa|atm|mmHg|bar|"
    r"k?g|mg|mol|kmol|m\^?\{?[23]?\}?|cm\^?\{?[23]?\}?|mm\^?\{?[23]?\}?|L|mL|s|min|h|K|cal|kcal|BTU)"
    r"(?:\s*/\s*(?:s|h|min|K|°C|m\^?\{?[23]?\}?|cm\^?\{?[23]?\}?|kg|g|mol|K\^?4))*)+\s*$"
)


def _eval_ast_num(node: ast.AST, env: Dict[str, float], profundidad: int = 0) -> float:
    if profundidad > 40:
        raise ValueError("Expresión demasiado profunda")
    if isinstance(node, ast.Expression):
        return _eval_ast_num(node.body, env, profundidad + 1)
    if isinstance(node, ast.Constant):
        if isinstance(node.value, bool) or not isinstance(node.value, (int, float)):
            raise ValueError("Constante no numérica")
        val = float(node.value)
        if not math.isfinite(val):
            raise ValueError("Valor no finito")
        return val
    if isinstance(node, ast.Name):
        if node.id in env:
            return float(env[node.id])
        raise ValueError(f"Identificador no numérico: {node.id}")
    if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARY_OPS:
        return _UNARY_OPS[type(node.op)](_eval_ast_num(node.operand, env, profundidad + 1))
    if isinstance(node, ast.BinOp):
        izq = _eval_ast_num(node.left, env, profundidad + 1)
        der = _eval_ast_num(node.right, env, profundidad + 1)
        if isinstance(node.op, ast.Pow):
            if abs(der) > 400:
                raise ValueError("Exponente fuera de rango seguro")
            val = izq**der
            if isinstance(val, complex) or not math.isfinite(val):
                raise ValueError("Potencia no real o finita")
            return float(val)
        if type(node.op) in _BIN_OPS:
            if isinstance(node.op, ast.Div) and abs(der) < 1e-300:
                raise ValueError("División entre cero")
            val = _BIN_OPS[type(node.op)](izq, der)
            if not math.isfinite(val):
                raise ValueError("Resultado no finito")
            return float(val)
        raise ValueError(f"Operador no permitido: {type(node.op).__name__}")
    if isinstance(node, ast.Call):
        if not isinstance(node.func, ast.Name) or node.keywords:
            raise ValueError("Llamada no permitida")
        fn = _FUNCIONES.get(node.func.id)
        if fn is None:
            raise ValueError(f"Función no permitida: {node.func.id}")
        args = [_eval_ast_num(a, env, profundidad + 1) for a in node.args]
        val = float(fn(*args))
        if not math.isfinite(val):
            raise ValueError("Resultado de función no finito")
        return val
    raise ValueError(f"Nodo AST no permitido: {type(node).__name__}")


def _tex_a_aritmetica(expr: str) -> str:
    """Convierte una expresión LaTeX numérica (sin texto de unidades) a sintaxis aritmética Python."""
    s = (expr or "").strip()
    if not s or len(s) > 500:
        return ""
    s = s.replace("$$", "").replace("$", "")
    s = re.sub(r"\\class\{[^{}]*\}\{([^{}]*)\}", r"\1", s)
    s = re.sub(r"\\boxed\{([^{}]*)\}", r"\1", s)
    s = s.replace("{,}", ".")
    # Coma decimal europea/latina: 121,6 -> 121.6
    s = re.sub(r"(?<=\d),(?=\d)", ".", s)
    # Separador de miles con espacio fino: 10\,000 -> 10000
    s = re.sub(r"(?<=\d)(?:\\,|\\;|~|\s)+(?=\d{3}\b)", "", s)
    # Quitar unidades explícitas \text{m}^2, \text{m}^{-1}, \mathrm{...}, °C, %
    s = re.sub(r"\\(?:text|mathrm|operatorname|mathbf)\s*\{[^{}]*\}(?:\s*\^\s*(?:\{[^{}]*\}|-?\d+))?", " ", s)
    s = re.sub(r"\^\s*\{?\s*\\circ\s*\}?\s*[CFK]?", " ", s)
    s = re.sub(r"°\s*[CFK]?", " ", s)
    s = s.replace(r"\%", " ").replace("%", " ")
    s = s.replace(r"\left", "").replace(r"\right", "")
    s = s.replace(r"\,", " ").replace(r"\;", " ").replace(r"\!", " ").replace(r"\ ", " ").replace(r"\quad", " ").replace(r"\qquad", " ")
    s = s.replace(r"\cdot", "*").replace(r"\times", "*").replace("·", "*").replace("×", "*")
    s = s.replace("−", "-").replace("–", "-").replace("—", "-")

    # Fracciones anidadas (\frac, \dfrac, \tfrac)
    frac_re = re.compile(r"\\[dt]?frac\s*\{([^{}]+)\}\s*\{([^{}]+)\}")
    for _ in range(6):
        ns = frac_re.sub(r"((\1)/(\2))", s)
        if ns == s:
            break
        s = ns

    # Raíces (\sqrt[n]{a} y \sqrt{a})
    s = re.sub(r"\\sqrt\s*\[([^\]]+)\]\s*\{([^{}]+)\}", r"((\2)**(1/(\1)))", s)
    sqrt_re = re.compile(r"\\sqrt\s*\{([^{}]+)\}")
    for _ in range(4):
        ns = sqrt_re.sub(r"sqrt(\1)", s)
        if ns == s:
            break
        s = ns

    # Funciones y constantes
    for viejo, nuevo in (
        (r"\ln", "ln"),
        (r"\log_{10}", "log10"),
        (r"\log", "log"),
        (r"\sin", "sin"),
        (r"\cos", "cos"),
        (r"\tan", "tan"),
        (r"\exp", "exp"),
        (r"\pi", "pi"),
        ("π", "pi"),
        (r"\sigma", "sigma"),
    ):
        s = s.replace(viejo, nuevo)

    # Exponentes ^{...} y ^d
    for _ in range(4):
        ns = re.sub(r"\^\s*\{([^{}]+)\}", r"**(\1)", s)
        if ns == s:
            break
        s = ns
    s = re.sub(r"\^\s*(-?\d+(?:\.\d+)?)", r"**(\1)", s)

    # Quitar unidades sueltas al final de un número (ej. "121.6 C", "56 J", "7.80 W")
    s = _UNIDADES_FINAL_RE.sub("", s).strip()
    # Quitar llaves sobrantes simples
    s = s.replace("{", "(").replace("}", ")")
    # Multiplicación implícita segura entre números/paréntesis/constantes/funciones
    s = re.sub(r"(\d|\))\s*\(", r"\1*(", s)
    s = re.sub(r"\)\s*(\d|(?:pi|e|ln|log|log10|sin|cos|tan|sqrt|exp)\b)", r")*\1", s)
    s = re.sub(r"(\d)\s*((?:pi|ln|log|log10|sin|cos|tan|sqrt|exp)\b)", r"\1*\2", s)
    s = re.sub(r"\b(ln|log|log10|sin|cos|tan|sqrt|exp)\s+([0-9.]+)", r"\1(\2)", s)
    return s.strip()


def evaluar_expresion_segura(expr_str: str, variables: Optional[Dict[str, float]] = None) -> Optional[float]:
    """Evalúa de forma segura una expresión numérica/LaTeX mediante AST whitelist (sin eval)."""
    arit = _tex_a_aritmetica(expr_str)
    if not arit or len(arit) > 400:
        return None
    env = dict(_CONSTANTES)
    if variables:
        for k, v in variables.items():
            if isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(float(v)):
                env[str(k)] = float(v)
    try:
        tree = ast.parse(arit, mode="eval")
        return _eval_ast_num(tree, env)
    except Exception:
        return None


def _es_numero_puro(tex_segment: str) -> bool:
    arit = _tex_a_aritmetica(tex_segment)
    if not arit:
        return False
    # Acepta número simple o notación científica 1.2*10**(-3)
    if re.fullmatch(r"[+\-]?\s*\d+(?:\.\d+)?(?:[eE][+\-]?\d+)?", arit):
        return True
    if re.fullmatch(r"[+\-]?\s*\d+(?:\.\d+)?\s*\*\s*10\s*\*\*\s*\(?[+\-]?\d+\)?", arit):
        return True
    return False


def _tiene_operacion(tex_segment: str) -> bool:
    arit = _tex_a_aritmetica(tex_segment)
    if not arit or _es_numero_puro(tex_segment):
        return False
    return bool(re.search(r"[\+\-\*/]|\*\*|\b(?:sqrt|ln|log|log10|sin|cos|tan|exp)\b", arit))


def _tolerancia_redondeo(der_str: str, val_calc: float, es_aprox: bool) -> float:
    """Calcula la tolerancia justa según las cifras decimales escritas (half-ULP)."""
    arit = _tex_a_aritmetica(der_str)
    m_sci = re.search(r"(\d+)(?:\.(\d+))?\s*\*\s*10\s*\*\*\s*\(?([+\-]?\d+)\)?", arit)
    if m_sci:
        dec = len(m_sci.group(2) or "")
        exp = int(m_sci.group(3))
        half_ulp = 0.5 * (10.0 ** (exp - dec))
    else:
        m_num = re.search(r"(\d+)(?:\.(\d+))?(?:[eE]([+\-]?\d+))?", arit)
        if m_num:
            dec = len(m_num.group(2) or "")
            exp = int(m_num.group(3) or 0)
            half_ulp = 0.5 * (10.0 ** (exp - dec))
        else:
            half_ulp = 0.005 * max(abs(val_calc), 1.0)
    if es_aprox:
        return max(half_ulp * 1.6, 0.005 * max(abs(val_calc), 1e-12), 1e-4)
    return max(half_ulp * 1.2, 0.0008 * max(abs(val_calc), 1e-12), 1e-4)


def _quitar_unidad_parentesis_final(der_str: str) -> str:
    """Si el lado derecho trae unidad equivalente entre paréntesis ej. '23,62 kW (23617 W)', toma el valor principal."""
    return re.sub(
        r"(?:\\;|\\quad|\s)+\(\s*[-+]?\d[\d.,{}\s\\]*\\?(?:text|mathrm)\{[^{}]+\}\s*\)\s*$",
        "",
        der_str.strip(),
    ).strip()


def _factor_prefijo_si(s: str) -> float:
    """Devuelve factor SI si el término termina en prefijo/unidad (ej. kW->1e3, L->1e-3 m³, cm->1e-2 m)."""
    st = s.strip()
    m = re.search(r"(?:\\text|\\mathrm)\{\s*([kMmGc])?(W|J|Pa|Hz|N|V|A|L|m)\s*\}\s*(?:\^\s*\{?([23])\}?)?\s*$", st)
    if not m:
        return 1.0
    pref, unit, exp_s = m.group(1) or "", m.group(2), int(m.group(3) or 1)
    if unit == "L":
        return {"m": 1e-6, "": 1e-3}.get(pref, 1.0)
    f = {"k": 1e3, "M": 1e6, "G": 1e9, "m": 1e-3, "c": 1e-2}.get(pref, 1.0)
    return f**exp_s


def verificar_ecuacion_tex(
    tex_str: str,
    rel_tol: Optional[float] = None,
    abs_tol: float = 1e-4,
) -> Dict[str, Any]:
    """Verifica igualdades aritméticas encadenadas en una ecuación LaTeX (ej. '120 + 1.6 = 121.4')."""
    s = (tex_str or "").strip()
    if not s:
        return {"comprobaciones": 0, "ok": True, "detalles": [], "discrepancias": []}
    # Ignorar entorno cases o matrices complejas
    if r"\begin{cases}" in s or r"\begin{matrix}" in s:
        return {"comprobaciones": 0, "ok": True, "detalles": [], "discrepancias": []}

    # Dividir por líneas \\ si es aligned
    lineas = re.split(r"\\\\", s)
    comprobaciones = 0
    detalles: List[Dict[str, Any]] = []
    discrepancias: List[Dict[str, Any]] = []

    for linea in lineas:
        linea_limpia = linea.replace("&", " ").strip()
        tokens = re.split(r"(\\approx|\\simeq|≈|=)", linea_limpia)
        if len(tokens) < 3:
            continue
        for idx in range(0, len(tokens) - 2, 2):
            izq = tokens[idx].strip()
            op = tokens[idx + 1].strip()
            der_raw = tokens[idx + 2].strip()
            der = _quitar_unidad_parentesis_final(der_raw)
            if not izq or not der or not _tiene_operacion(izq):
                continue
            val_izq = evaluar_expresion_segura(izq)
            val_der = evaluar_expresion_segura(der)
            if val_izq is None or val_der is None:
                continue
            # Si un lado está expresado en porcentaje (ej. 1 - 350/700 = 50 %), convertir % -> fracción
            der_es_pct = bool(re.search(r"(?:\\%|%)\s*$", der))
            izq_es_pct = bool(re.search(r"(?:\\%|%)\s*$", izq))
            if der_es_pct and not izq_es_pct and abs(val_izq) <= 1.05 and abs(val_der) > 1.0:
                val_der = val_der / 100.0
            elif izq_es_pct and not der_es_pct and abs(val_der) <= 1.05 and abs(val_izq) > 1.0:
                val_izq = val_izq / 100.0
            # Si el lado derecho expresa el resultado con prefijo SI (ej. 23,62 kW frente a 23617 W en el cálculo)
            f_der = _factor_prefijo_si(der)
            if f_der != 1.0 and _factor_prefijo_si(izq) == 1.0:
                if abs(val_izq - val_der * f_der) < abs(val_izq - val_der):
                    val_der = val_der * f_der
            comprobaciones += 1
            diff = abs(val_izq - val_der)
            denom = max(abs(val_izq), abs(val_der), 1e-12)
            rel_err = diff / denom
            es_aprox = op != "="
            if rel_tol is not None:
                coincide = (diff <= abs_tol) or (rel_err <= rel_tol)
            else:
                tol_ulp = _tolerancia_redondeo(der, val_izq, es_aprox)
                coincide = diff <= max(abs_tol, tol_ulp)
            item = {
                "expresion": izq[:120],
                "afirmado_str": der[:60],
                "calculado": round(val_izq, 6),
                "afirmado": round(val_der, 6),
                "error_rel": round(rel_err, 5),
                "ok": coincide,
            }
            detalles.append(item)
            if not coincide:
                discrepancias.append({
                    **item,
                    "mensaje": f"Cálculo revisado: {izq[:60]} da {val_izq:.4g}, no {val_der:.4g}.",
                })

    return {
        "comprobaciones": comprobaciones,
        "ok": len(discrepancias) == 0,
        "detalles": detalles,
        "discrepancias": discrepancias,
    }


def verificar_beat_numerico(beat: Dict[str, Any]) -> Dict[str, Any]:
    """Audita todas las acciones matemáticas de un beat de protocolo v1.1."""
    total_comp = 0
    discrepancias: List[Dict[str, Any]] = []

    for act in beat.get("actions") or []:
        if not isinstance(act, dict):
            continue
        tipo = str(act.get("type") or act.get("a") or "")
        payload = act.get("payload") if isinstance(act.get("payload"), dict) else act
        exprs: List[str] = []
        if tipo in ("write_math", "mate"):
            lx = str(payload.get("latex") or payload.get("tex") or "")
            if lx:
                exprs.append(lx)
        elif tipo == "expand_intermediate_steps":
            for st in payload.get("substeps") or payload.get("steps") or []:
                if isinstance(st, dict) and (st.get("latex") or st.get("tex")):
                    exprs.append(str(st.get("latex") or st.get("tex")))
                elif isinstance(st, str):
                    exprs.append(st)
        for lx in exprs:
            r = verificar_ecuacion_tex(lx)
            total_comp += r["comprobaciones"]
            discrepancias.extend(r["discrepancias"])

    verificado = total_comp > 0 and len(discrepancias) == 0
    if discrepancias:
        resumen = f"Se detectaron {len(discrepancias)} error(es) aritmético(s) al recalcular en Python."
    elif verificado:
        resumen = f"Verificado numéricamente en Python ({total_comp} cálculo(s))."
    else:
        resumen = "Sin operaciones aritméticas directas verificables."

    return {
        "verificado": verificado,
        "comprobaciones": total_comp,
        "discrepancias": discrepancias,
        "resumen": resumen,
    }


def recalcular_leccion_ia(leccion: Dict[str, Any]) -> Dict[str, Any]:
    """Verifica toda ecuación en una lección generada por IA; marca cualquier discrepancia visiblemente."""
    total_comp = 0
    discrepancias: List[Dict[str, Any]] = []
    for paso in leccion.get("pasos") or []:
        if not isinstance(paso, dict):
            continue
        for tramo in paso.get("tramos") or []:
            if not isinstance(tramo, dict):
                continue
            for acc in list(tramo.get("acc") or []):
                if isinstance(acc, dict) and acc.get("a") == "mate" and acc.get("tex"):
                    r = verificar_ecuacion_tex(str(acc["tex"]))
                    total_comp += r["comprobaciones"]
                    if r["discrepancias"]:
                        discrepancias.extend(r["discrepancias"])
                        aviso = " ".join(d["mensaje"] for d in r["discrepancias"])
                        tramo["acc"].append({
                            "a": "texto",
                            "txt": f"Aviso del verificador numérico: {aviso}",
                            "clase": "nota",
                            "color": "rojo",
                        })
    leccion["verificacion_numerica"] = {
        "verificado": total_comp > 0 and len(discrepancias) == 0,
        "comprobaciones": total_comp,
        "discrepancias": discrepancias,
    }
    if discrepancias:
        leccion["aviso"] = (
            "El verificador numérico en Python detectó al menos una diferencia aritmética en la "
            "respuesta generada; revisa el aviso en rojo en el paso correspondiente."
        )
    return leccion


_GRIEGAS_VOZ = {
    "alpha": "alfa", "beta": "beta", "gamma": "gama", "delta": "delta",
    "epsilon": "épsilon", "varepsilon": "épsilon", "eta": "eta", "theta": "teta",
    "kappa": "kapa", "lambda": "lambda", "mu": "mu", "nu": "nu", "pi": "pi",
    "rho": "ro", "sigma": "sigma", "tau": "tau", "phi": "fi", "varphi": "fi",
    "chi": "ji", "psi": "psi", "omega": "omega", "Gamma": "gama", "Delta": "delta",
    "Theta": "teta", "Lambda": "lambda", "Sigma": "sigma", "Phi": "fi", "Psi": "psi",
    "Omega": "omega",
}

_CMD_VOZ = {
    "cdot": " por ", "times": " por ", "div": " entre ", "pm": " más o menos ",
    "le": " menor o igual que ", "leq": " menor o igual que ",
    "ge": " mayor o igual que ", "geq": " mayor o igual que ",
    "ne": " distinto de ", "neq": " distinto de ",
    "approx": " aproximadamente ", "simeq": " aproximadamente ",
    "propto": " proporcional a ", "to": " tiende a ",
    "rightarrow": " lleva a ", "Rightarrow": " entonces ", "implies": " entonces ",
    "int": " integral de ", "oint": " integral cerrada de ", "sum": " suma de ",
    "infty": " infinito ", "partial": " derivada parcial ",
    "ln": " logaritmo natural de ", "log": " logaritmo de ",
    "sin": " seno de ", "cos": " coseno de ", "tan": " tangente de ",
    "exp": " exponencial de ", "degree": " grados ", "circ": " grados ",
    "quad": " ", "qquad": " ", "left": " ", "right": " ",
    "displaystyle": " ", "text": "", "mathrm": "", "mathbf": "", "boxed": "",
}


def _tex_a_voz_py(tex: str) -> str:
    s = tex or ""
    s = re.sub(r"\\\\|&", " ", s)
    s = re.sub(r"\\[,;:!]|~", " ", s)
    s = s.replace("{,}", ",").replace(r"\%", " por ciento ")
    for _ in range(5):
        s = re.sub(r"\\[dt]?frac\s*\{([^{}]*)\}\s*\{([^{}]*)\}", r" \1 sobre \2 ", s)
        s = re.sub(r"\\sqrt\s*\{([^{}]*)\}", r" raíz de \1 ", s)
        s = re.sub(r"\^\s*\{?\\circ\}?\s*(?:\\text\{C\}|C)", " grados Celsius ", s)
        s = re.sub(r"\^\s*\{?\\circ\}?", " grados ", s)
        s = re.sub(r"\^\s*\{?2\}?(?![0-9])", " al cuadrado ", s)
        s = re.sub(r"\^\s*\{?3\}?(?![0-9])", " al cubo ", s)
        s = re.sub(r"\^\s*\{([^{}]*)\}", r" a la \1 ", s)
        s = re.sub(r"_\s*\{([^{}]*)\}", r" \1 ", s)
        s = re.sub(r"\\(?:text|mathrm|mathbf|boxed|operatorname)\s*\{([^{}]*)\}", r" \1 ", s)
    s = re.sub(r"\\([A-Za-z]+)", lambda m: f" {_GRIEGAS_VOZ[m.group(1)]} " if m.group(1) in _GRIEGAS_VOZ else _CMD_VOZ.get(m.group(1), " "), s)
    s = re.sub(r"[{}$\\]", " ", s)
    s = s.replace("=", " es igual a ").replace("+", " más ")
    return re.sub(r"\s+", " ", s).strip()


def limpiar_voz_latex(texto: str, preservar_cues: bool = False) -> str:
    """Elimina cualquier LaTeX crudo de un guion hablado (opcionalmente conservando <cue id='...'/>)."""
    s = str(texto or "")
    cues: List[str] = []
    if preservar_cues:
        def _guarda_cue(m: re.Match[str]) -> str:
            idx = len(cues)
            cues.append(m.group(0))
            return f" XCUEMARK{idx}X "
        s = re.sub(r"<cue\s+id=['\"][^'\"]+['\"]\s*/?>", _guarda_cue, s)
    else:
        s = re.sub(r"<[^>]+>", " ", s)

    s = re.sub(r"\$\$([\s\S]+?)\$\$", lambda m: f" {_tex_a_voz_py(m.group(1))} ", s)
    s = re.sub(r"\$([^$]+)\$", lambda m: f" {_tex_a_voz_py(m.group(1))} ", s)
    s = re.sub(r"\\\(([\s\S]+?)\\\)", lambda m: f" {_tex_a_voz_py(m.group(1))} ", s)
    s = re.sub(r"\\\[([\s\S]+?)\\\]", lambda m: f" {_tex_a_voz_py(m.group(1))} ", s)
    if re.search(r"\\[A-Za-z]+|[_^]\{", s):
        s = _tex_a_voz_py(s)
    s = re.sub(r"[*_`#>]+", " ", s)
    s = re.sub(r"\s+([.,;:!?])", r"\1", s)
    s = re.sub(r"\s+", " ", s).strip()
    if preservar_cues:
        for idx, cue_tag in enumerate(cues):
            s = s.replace(f"XCUEMARK{idx}X", cue_tag)
        s = re.sub(r"\s+", " ", s).strip()
    return s
