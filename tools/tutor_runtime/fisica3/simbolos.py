"""Qué significa cada letra: símbolo, significado en palabras y unidades.

Mientras la pizarra escribe una fórmula, el tutor muestra unos segundos qué es cada letra, símbolo,
variable o constante de ese renglón, con sus unidades. Aquí se arma ese diccionario por lección:

    {"por_accion": {"p3a17": [["T_1", "temperatura 1", "K o °C"], ["k", "conductividad térmica…", "W/(m·K)"]]}}

Reglas:
- Una fórmula de la hoja (rol "formula") muestra todas sus letras la primera vez que aparece en la lección.
- Un renglón de cuentas muestra solo las letras que todavía no se habían explicado.
- Es conservador: si una letra es ambigua en ese contexto (por ejemplo, A como estado y no como área),
  no se etiqueta. Mejor no decir nada que enseñar algo falso.
"""

from __future__ import annotations

import re
from typing import Any, Dict, List, Optional, Sequence, Tuple

Item = List[str]  # [símbolo a mostrar (con _ para subíndice), significado, unidades]

GRIEGAS = {"alpha": "α", "beta": "β", "gamma": "γ", "sigma": "σ", "rho": "ρ", "pi": "π", "Delta": "Δ", "lambda": "λ"}
DIFERENCIALES = set("TxyrVQSWULAtm")
CTX_GAS = {"gases", "primera_ley", "ciclos", "maquinas", "entropia"}
UNIDADES_SUELTAS = {"K", "J", "W", "m", "s", "g", "L", "N", "C"}

# Grupo de la lección (como en registro.grupo) a partir del código de tema
_GRUPO = {
    "4.1": "dilatacion", "4.2": "dilatacion", "4.3": "dilatacion", "4.4": "dilatacion",
    "4.5": "calorimetria", "4.6": "calorimetria", "4.7": "calorimetria", "4.8": "calorimetria",
    "5.1": "conduccion", "5.2": "conduccion", "5.3": "conduccion", "5.4": "conduccion", "5.5": "conduccion",
    "5.6": "conduccion", "5.7": "conduccion", "5.8": "radiacion", "5.9": "radiacion",
    "6.1": "gases", "6.2": "gases", "6.3": "gases", "6.4": "gases", "6.5": "gases", "6.6": "gases",
    "6.7": "primera_ley", "6.8": "primera_ley", "6.9": "primera_ley", "6.10": "primera_ley",
    "6.11": "ciclos", "6.12": "entropia", "6.13": "entropia", "6.14": "maquinas", "6.15": "maquinas",
    "6.16": "entropia", "6.17": "entropia",
    "ondas": "ondas", "fluidos": "fluidos",
}
_VISUAL_A_GRUPO = {"conduccion": "conduccion", "radiacion": "radiacion", "dilatacion": "dilatacion", "calor": "calorimetria",
                   "calentamiento": "calorimetria", "pv": "ciclos", "gas": "gases", "distribucion": "gases",
                   "maquina": "maquinas", "otto": "maquinas", "entropia": "entropia", "fluidos": "fluidos"}

# Subíndices con nombre propio
_SUB_PALABRA = {
    "neto": "neto (de todo el ciclo)", "ciclo": "de todo el ciclo", "eq": "de equilibrio", "med": "promedio",
    "rms": "cuadrática media", "total": "total", "máx": "máximo", "max": "máximo", "mín": "mínimo", "min": "mínimo",
    "agua": "del agua", "hielo": "del hielo", "vapor": "del vapor", "Cu": "del cobre", "Al": "del aluminio",
    "Fe": "del hierro", "cobre": "del cobre", "aluminio": "del aluminio", "acero": "del acero", "vidrio": "del vidrio",
    "otto": "del ciclo Otto", "Carnot": "de Carnot", "iso": "en la isoterma", "adi": "en la adiabática",
    "isot": "en el proceso isotérmico", "isob": "en el proceso isobárico", "adiab": "en el proceso adiabático",
    "gana": "que se gana", "pierde": "que se pierde", "cede": "que se cede", "fundir": "para fundir",
    "evaporar": "para evaporar", "frío": "del lado frío", "caliente": "del lado caliente", "universo": "del universo",
    "máquina": "de la máquina", "mono": "de un gas monoatómico", "dia": "de un gas diatómico",
    "int": "interior", "ext": "exterior", "tr": "de traslación", "lata": "de la lata", "esfera": "de la esfera",
    "univ": "del universo", "serie": "en serie", "paralelo": "en paralelo", "anillo": "del anillo",
    "plateada": "con la pared plateada", "negro": "del cuerpo negro", "negra": "del cuerpo negro", "extra": "extra",
    "O": "del oxígeno", "He": "del helio", "tubo": "del tubo", "sis": "del sistema", "prom": "promedio",
    "Otto": "del ciclo Otto", "tanque": "del tanque", "rev": "reversible", "irrev": "irreversible",
}


# Subíndices cortos que solo se entienden dentro de un tema
_SUB_POR_GRUPO = {
    "calorimetria": {"v": "del vapor", "a": "del agua", "w": "del agua", "h": "del hielo", "i": "inicial", "f": "final",
                     "cond": "que se condensa", "fund": "que se funde", "sobra": "que sobra", "pide": "que se necesita",
                     "dan": "que se da", "cedeelagua": "que cede el agua", "absorbeelhielo": "que absorbe el hielo"},
    "conduccion": {"e": "exterior", "u": "(u)"},
    "ciclos": {"alta": "alta", "baja": "baja", "der": "de la derecha", "izq": "de la izquierda"},
    "gases": {"ideal": "si fuera gas ideal", "real": "real"},
    "entropia": {"h": "del hielo", "a": "del agua"},
}
MARCA_ESTADOS = "§estados-con-letra§"


def grupo_celda(cid: str) -> str:
    """Tema de una casilla de la hoja II-2026 (F1C1…F14C3), para leer bien sus letras."""
    fila = int(re.match(r"F(\d+)", cid).group(1)) if re.match(r"F\d+", cid or "") else 0
    if fila == 1:
        return "dilatacion"
    if cid in ("F2C1", "F2C2"):
        return "conduccion"
    if cid in ("F2C3", "F3C1"):
        return "radiacion"
    if cid in ("F3C2", "F3C3"):
        return "calorimetria"
    if fila in (4, 5) or fila >= 13 or cid == "F12C3":
        return "gases"
    if cid in ("F10C1", "F10C2", "F11C3", "F12C1"):
        return "maquinas"
    if cid in ("F10C3", "F12C2"):
        return "otros"
    return "ciclos"


def grupo_de(lec: Dict[str, Any]) -> str:
    g = _GRUPO.get(str(lec.get("tema") or ""))
    if g:
        return g
    if lec.get("parcial") == 1:
        return "ondas"
    if lec.get("parcial") == 2:
        return "fluidos"
    tipo = (lec.get("visual") or {}).get("tipo")
    return _VISUAL_A_GRUPO.get(tipo, "otros")


# ---------------------------------------------------------------- lectura del TeX

def _grupo_llaves(s: str, i: int) -> Tuple[str, int]:
    """s[i] == '{': devuelve el contenido y la posición después de la llave que cierra."""
    prof = 0
    for j in range(i, len(s)):
        if s[j] == "{":
            prof += 1
        elif s[j] == "}":
            prof -= 1
            if prof == 0:
                return s[i + 1:j], j + 1
    return s[i + 1:], len(s)


def _limpia_sub(sub: str) -> str:
    sub = re.sub(r"\{?\^\s*\\circ\}?\s*(?:\\text\s*\{\s*([CF])\s*\}|([CF]))", lambda m: "°" + (m.group(1) or m.group(2)), sub)
    sub = re.sub(r"\\(?:text|mathrm|operatorname)\s*\{([^{}]*)\}", r"\1", sub)
    sub = re.sub(r"\\([A-Za-z]+)", lambda m: GRIEGAS.get(m.group(1), ""), sub)
    sub = sub.replace("{", "").replace("}", "").replace(" ", "").replace("~", "")
    return sub.strip(",").strip()


def _lee_sub(s: str, i: int) -> Tuple[str, int]:
    """Si en s[i] empieza un subíndice (_x o _{...}), lo devuelve limpio con la nueva posición."""
    if i < len(s) and s[i] == "_":
        if i + 1 < len(s) and s[i + 1] == "{":
            cont, k = _grupo_llaves(s, i + 1)
            return _limpia_sub(cont), k
        if s.startswith("\\", i + 1):
            m = re.match(r"\\([A-Za-z]+)", s[i + 1:])
            if m:
                return _limpia_sub("\\" + m.group(1)), i + 1 + m.end()
        if i + 1 < len(s):
            return s[i + 1], i + 2
    return "", i


def fichas(tex: str) -> List[Tuple[str, str]]:
    """Las letras y símbolos de un renglón, en orden: (base, subíndice). Sin unidades ni palabras."""
    s = tex or ""
    out: List[Tuple[str, str]] = []
    i = 0
    n = len(s)
    while i < n:
        ch = s[i]
        if ch == "\\":
            m = re.match(r"\\([A-Za-z]+)", s[i:])
            if not m:
                i += 2
                continue
            nom = m.group(1)
            k = i + m.end()
            if nom in ("text", "mathrm", "operatorname", "textbf", "textit"):
                while k < n and s[k] == " ":
                    k += 1
                if k < n and s[k] == "{":
                    _, k = _grupo_llaves(s, k)
                i = k
                continue
            if nom == "Delta":
                while k < n and s[k] in " ~":
                    k += 1
                if s.startswith("\\,", k):
                    k += 2
                if k < n and s[k].isalpha():
                    base = "Δ" + s[k]
                    sub, k2 = _lee_sub(s, k + 1)
                    out.append((base, sub))
                    i = k2
                    continue
                i = k
                continue
            if nom in GRIEGAS:
                sub, k2 = _lee_sub(s, k)
                out.append((GRIEGAS[nom], sub))
                i = k2
                continue
            if nom == "int":
                out.append(("∫", ""))
            elif nom == "ln":
                out.append(("ln", ""))
            elif nom == "exp":
                out.append(("e^", ""))
            i = k
            continue
        if ch.isalpha():
            # Diferencial: dT, dx, dr, dV, dQ…
            if ch == "d" and i + 1 < n and s[i + 1] in DIFERENCIALES and (i == 0 or not s[i - 1].isalpha()) \
                    and (i + 2 >= n or not s[i + 2].isalpha()):
                out.append(("d" + s[i + 1], ""))
                i += 2
                continue
            # Unidad suelta pegada a un número (300 K): no es una variable
            antes = s[max(0, i - 4):i]
            if ch in UNIDADES_SUELTAS and re.search(r"\d(?:\s|\\ |\\,|~)*$", antes) and (i + 1 >= n or not s[i + 1].isalpha()):
                i += 1
                continue
            sub, k = _lee_sub(s, i + 1)
            if ch == "e" and k < n and s[k] == "^":
                out.append(("e^", ""))
            else:
                out.append((ch, sub))
            i = k
            continue
        i += 1
    return out


# ---------------------------------------------------------------- significados

def _cual(sub: str, gas: bool, grupo: str = "", contexto: str = "") -> str:
    """Texto que agrega un subíndice: 'en el estado A', 'del foco caliente', 'inicial'…"""
    if not sub:
        return ""
    if "," in sub:
        return "".join(_cual(p, gas, grupo, contexto) for p in sub.split(",") if p)
    estados_letra = MARCA_ESTADOS in contexto
    if grupo in ("conduccion", "radiacion") and sub in ("H", "C", "c"):
        return " del lado caliente" if sub == "H" else " del lado frío"
    if grupo == "maquinas" and re.fullmatch(r"[AB]", sub) and not estados_letra:
        return f" de la máquina {sub}"
    if sub in ("C", "c") and gas and not estados_letra:
        return " del foco frío"
    if sub == "°C":
        return " en grados Celsius"
    if sub == "°F":
        return " en grados Fahrenheit"
    if sub == "i" and grupo == "conduccion":
        return " de la interfaz (donde se juntan las capas)" if "interfaz" in contexto else " interior"
    propios = _SUB_POR_GRUPO.get(grupo, {})
    if sub in propios:
        return " " + propios[sub]
    if sub in _SUB_PALABRA:
        return " " + _SUB_PALABRA[sub]
    if sub == "0":
        return " inicial"
    if sub.isdigit():
        if gas and len(sub) == 2:
            return f" en el proceso de {sub[0]} a {sub[1]}"
        return f" en el estado {sub}" if gas else f" {sub}"
    if re.fullmatch(r"[A-E]", sub):
        return f" en el estado {sub}" if gas else f" {sub}"
    if re.fullmatch(r"[A-E]{2}|[a-e]{2}", sub):
        return f" en el proceso de {sub[0]} a {sub[1]}"
    if re.fullmatch(r"[a-e]", sub) and gas:
        return f" en el estado {sub}"
    if sub == "H":
        return " del lado caliente" if grupo in ("conduccion", "radiacion") else " del foco caliente"
    if sub in ("C", "c"):
        return " del lado frío" if grupo in ("conduccion", "radiacion") else " del foco frío"
    if sub == "i":
        return " inicial"
    if sub == "f":
        return " final"
    if sub == "s":
        return " de los alrededores"
    return f" ({sub})"


def significado(base: str, sub: str, grupo: str, linea: str = "", contexto: str = "") -> Optional[Tuple[str, str]]:
    """(significado, unidades) o None si no se sabe con seguridad."""
    gas = grupo in CTX_GAS
    q = _cual(sub, gas, grupo, contexto)
    # El calor no es de un estado: Q_C y Q_H siempre son los focos, aunque los estados se llamen A, B, C…
    if base == "Q" and gas and sub in ("C", "c"):
        q = " del foco frío"
    # Constantes y símbolos con subíndice propio
    if base == "k" and sub == "B":
        return "constante de Boltzmann", "J/K"
    if base == "N" and sub == "A":
        return "número de Avogadro: moléculas en un mol", "1/mol"
    if base == "v" and sub.startswith("rms"):
        resto = _cual(sub[3:].strip(","), gas, grupo, contexto) if len(sub) > 3 else ""
        return "rapidez cuadrática media (rapidez eficaz) de las moléculas" + resto, "m/s"
    if base == "L" and not sub and re.search(r"m\s*L(?![A-Za-z_])|\\pm\s*mL", linea):
        return "calor latente: calor para cambiar de fase 1 kg", "J/kg"
    if base == "P" and not sub and gas:
        return "presión", "Pa"
    if base == "C" and sub.startswith("V"):
        extra = _SUB_PALABRA.get(sub[1:], "") if len(sub) > 1 else ""
        return ("capacidad calorífica molar a volumen constante" + (" " + extra if extra else "")).strip(), "J/(mol·K)"
    if base == "C" and sub in ("P", "p"):
        return "capacidad calorífica molar a presión constante", "J/(mol·K)"
    if base == "L" and sub == "f":
        return "calor latente de fusión: calor para fundir 1 kg", "J/kg"
    if base == "L" and sub == "v":
        return "calor latente de vaporización: calor para evaporar 1 kg", "J/kg"
    if base == "e" and sub == "otto":
        return "eficiencia del ciclo Otto", "sin unidades"
    if base == "T" and sub == "K":
        return "temperatura en kelvin", "K"
    if base == "T" and sub in ("°C", "°F"):
        return f"temperatura en grados {'Celsius' if sub == '°C' else 'Fahrenheit'}", sub
    # Diferenciales, integral y funciones
    dif = {
        "dT": ("cambio pequeñito de temperatura", "K"),
        "dx": ("rebanada delgadita: un pedacito de largo", "m"),
        "dy": ("rebanada delgadita: un pedacito de largo", "m"),
        "dr": ("capa delgadita: un pedacito de radio", "m"),
        "dV": ("cambio pequeñito de volumen", "m³"),
        "dQ": ("calor pequeñito que entra en un pedacito del proceso", "J"),
        "dS": ("cambio pequeñito de entropía", "J/K"),
        "dW": ("trabajo pequeñito", "J"),
        "dU": ("cambio pequeñito de energía interna", "J"),
        "dt": ("instante pequeñito de tiempo", "s"),
        "dA": ("pedacito de área", "m²"),
        "dL": ("pedacito de largo", "m"),
        "dm": ("pedacito de masa", "kg"),
    }
    if base == "dQ" and grupo in ("conduccion", "radiacion"):
        return "calor pequeñito que pasa en un instante", "J"
    if base in dif:
        return dif[base]
    if base == "∫":
        return "integral: suma de todos los pedacitos, desde el límite de abajo hasta el de arriba", ""
    if base == "ln":
        return "logaritmo natural (tecla ln de la calculadora)", ""
    if base == "e^":
        return "número e ≈ 2,718 (base del logaritmo natural)", ""
    # Cambios (Δ)
    if base.startswith("Δ"):
        x = base[1:]
        que = {"T": ("temperatura", "K o °C (en una resta es lo mismo)"), "L": ("largo", "m"), "A": ("área", "m²"),
               "V": ("volumen", "m³"), "U": ("energía interna", "J"), "S": ("entropía", "J/K"), "p": ("presión", "Pa"),
               "x": ("posición", "m"), "r": ("radio", "m"), "t": ("tiempo", "s")}.get(x)
        if not que:
            return None
        return f"cambio de {que[0]}{q}: final menos inicial", que[1]
    # Griegas
    termo = grupo not in ("ondas", "fluidos")
    if base == "λ" and grupo == "ondas":
        return f"longitud de onda{q}", "m"
    if base == "σ" and termo:
        return "constante de Stefan-Boltzmann", "W/(m²·K⁴)"
    if base == "α" and termo:
        return "coeficiente de dilatación lineal: cuánto crece cada metro por cada grado", "1/K o 1/°C"
    if base == "β" and termo:
        return "coeficiente de dilatación de volumen", "1/K o 1/°C"
    if base == "γ" and termo:
        return "gama: C_P entre C_V del gas", "sin unidades"
    if base == "ρ":
        return f"densidad{q}: masa entre volumen", "kg/m³"
    if base == "π":
        return "pi ≈ 3,1416", "sin unidades"
    # Letras que dependen del tema
    if grupo == "ondas":
        if base == "n" and not sub:
            return "número de armónico (1, 2, 3, …)", "entero positivo"
        if base == "T" and not sub and re.search(r"2\\pi|\\omega|1/f|\\frac\{1\}\{f\}", linea):
            return "período: tiempo de una oscilación completa", "s"
        if base == "m":
            return f"masa{q}", "kg"
        if base == "t" and not sub:
            return "tiempo", "s"
        if base == "L" and not sub:
            return "longitud de la cuerda o tubo", "m"
        return None
    if grupo == "fluidos":
        if base == "Q" and not sub:
            return "caudal volumétrico: volumen por segundo", "m³/s"
        if base == "p":
            return f"presión{q}", "Pa"
        if base == "V":
            return f"volumen{q}", "m³"
        if base == "m":
            return f"masa{q}", "kg"
        if base == "A":
            return f"área{q}", "m²"
        if base == "Y" and not sub:
            return "módulo de Young: rigidez del material", "Pa"
        if base == "h" and not sub:
            return "profundidad o altura", "m"
        if base == "t" and not sub:
            return "tiempo", "s"
        return None
    if base == "T":
        return f"temperatura{q}", "K (o °C en una resta)"
    if base == "Q":
        return f"calor{q}", "J"
    if base == "m":
        return f"masa{q}", "kg"
    if base == "n" and not sub:
        return "cantidad de gas en moles", "mol"
    if base == "c" and grupo in ("calorimetria", "otros") and not gas:
        return f"calor específico{q}: calor para subir 1 grado a 1 kg", "J/(kg·K)"
    if base == "p":
        return f"presión{q}", "Pa"
    if base == "V" and grupo not in ("conduccion", "radiacion"):
        return f"volumen{q}", "m³"
    if base == "W" and grupo == "maquinas":
        return f"trabajo{q} en un ciclo (el que entrega el motor o el que se le paga al refrigerador)", "J"
    if base == "W":
        return f"trabajo{q}" + (" que hace el gas" if gas and not sub else ""), "J"
    if base == "U":
        return f"energía interna del gas{q}", "J"
    if base == "H" and not sub and grupo in ("conduccion", "radiacion"):
        return "flujo de calor: cuánto calor pasa cada segundo", "W = J/s"
    if base == "k" and not sub and grupo == "conduccion":
        return "conductividad térmica: qué tan bien conduce el calor el material", "W/(m·K)"
    if base == "A" and grupo in ("conduccion", "radiacion", "dilatacion"):
        return f"área{q}", "m²"
    if base == "x" and grupo in ("conduccion", "dilatacion"):
        return "posición a lo largo del material", "m"
    if base == "y" and grupo == "conduccion":
        return "posición a lo largo del material", "m"
    if base == "r" and grupo in ("conduccion", "radiacion", "dilatacion"):
        return f"radio{q}: distancia desde el centro", "m"
    if base == "r" and grupo == "maquinas" and not sub:
        return "razón de compresión: volumen máximo entre volumen mínimo", "sin unidades"
    if base == "L" and not sub and grupo == "calorimetria":
        return "calor latente: calor para cambiar de fase 1 kg", "J/kg"
    if base == "L" and grupo in ("conduccion", "dilatacion"):
        return f"largo{q}", "m"
    if base == "e" and not sub and grupo == "radiacion":
        return "emisividad: qué tanto radia comparado con un cuerpo negro", "sin unidades (de 0 a 1)"
    if base == "e" and grupo in ("maquinas", "ciclos"):
        return f"eficiencia{q}: trabajo que se obtiene entre calor que entra", "sin unidades"
    if base == "C" and not sub and gas:
        return "capacidad calorífica molar", "J/(mol·K)"
    if base == "R" and not sub and (gas or grupo == "conduccion" and "nRT" in linea):
        return "constante de los gases", "J/(mol·K) (o 0,08206 L·atm/(mol·K))"
    if base == "R" and grupo == "conduccion" and sub.isdigit() and re.search(r"H\s*\(?\s*R_|R_\{?\d\}?\s*\+\s*R_|R_\{?\d\}?\s*=\s*\\frac", linea):
        return f"resistencia térmica de la capa {sub}: cuánto le cuesta al calor cruzarla", "K/W"
    if base == "K" and not sub and grupo == "maquinas":
        return "coeficiente de rendimiento del refrigerador: calor que se saca entre trabajo", "sin unidades"
    if base == "K" and grupo == "gases":
        return f"energía cinética{q}", "J"
    if base == "N" and not sub and grupo == "gases":
        return "número de moléculas", "sin unidades"
    if base == "M" and not sub and grupo == "gases":
        return "masa molar: masa de un mol", "kg/mol"
    if base == "v" and grupo == "gases":
        return f"rapidez de una molécula{q}", "m/s"
    if base == "S" and grupo == "entropia":
        return f"entropía{q}", "J/K"
    if base == "t" and not sub:
        return "tiempo", "s"
    if base == "f" and not sub and grupo == "gases":
        return "formas de moverse de la molécula (grados de libertad)", "sin unidades"
    if base == "h" and not sub and grupo in ("conduccion", "dilatacion"):
        return "altura", "m"
    if base == "F" and not sub and grupo == "dilatacion":
        return "fuerza que aparece cuando no se deja dilatar el material", "N"
    if base == "Y" and not sub and grupo == "dilatacion":
        return "módulo de Young: qué tan rígido es el material", "Pa"
    if base == "M" and not sub and grupo in ("gases", "calorimetria"):
        return "masa molar: masa de un mol", "kg/mol"
    return None


def _muestra(base: str, sub: str) -> str:
    return base + ("_" + sub if sub else "")


def simbolos_de(tex: str, grupo: str, contexto: str = "") -> List[Item]:
    """Lista sin repetir de [símbolo, significado, unidades] de un renglón."""
    vistos = set()
    out: List[Item] = []
    for base, sub in fichas(tex):
        clave = _muestra(base, sub)
        if clave in vistos:
            continue
        vistos.add(clave)
        r = significado(base, sub, grupo, tex, contexto)
        if r:
            out.append([clave, r[0], r[1]])
    return out


def mapa_leccion(lec: Dict[str, Any]) -> Dict[str, Any]:
    """Qué mostrar con cada renglón de matemática de la lección (por id de acción)."""
    grupo = grupo_de(lec)
    partes = [str(lec.get("enunciado") or ""), " ".join(lec.get("claves") or [])]
    partes += [f"{p.get('titulo', '')} {p.get('habla', '')}" for p in lec.get("pasos") or []]
    todo_tex = " ".join(a.get("tex", "") for p in lec.get("pasos") or [] for t in p.get("tramos") or [] for a in t.get("acc") or [])
    if re.search(r"[pVT]_\{?[abdABD]\b", todo_tex):
        partes.append(MARCA_ESTADOS)
    contexto = " ".join(partes).lower().replace(MARCA_ESTADOS.lower(), MARCA_ESTADOS)
    explicados: set = set()
    formulas_vistas: set = set()
    por_accion: Dict[str, List[Item]] = {}
    for p in lec.get("pasos") or []:
        for t in p.get("tramos") or []:
            for a in t.get("acc") or []:
                if a.get("a") != "mate" or not a.get("tex") or not a.get("id"):
                    continue
                items = simbolos_de(a["tex"], grupo, contexto)
                if a.get("rol") == "formula":
                    clave_f = re.sub(r"\s+", "", a["tex"])
                    mostrar = items if clave_f not in formulas_vistas else [x for x in items if x[0] not in explicados]
                    formulas_vistas.add(clave_f)
                else:
                    mostrar = [x for x in items if x[0] not in explicados]
                explicados.update(x[0] for x in items)
                if mostrar:
                    por_accion[a["id"]] = mostrar
    return {"grupo": grupo, "por_accion": por_accion}
