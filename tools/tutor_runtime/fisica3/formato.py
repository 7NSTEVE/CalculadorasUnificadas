"""Números y unidades del tutor: la misma cifra en la pizarra, en el subtítulo y en la voz.

Cada número se redondea UNA sola vez y de esa cadena salen las tres formas. Así la voz no
puede decir una cosa y la pizarra otra (que fue justo el fallo de la lección vieja: decía
"cuarenta joules" con 56 J escrito al lado).
"""

from __future__ import annotations

import math
import re
from decimal import ROUND_HALF_UP, Decimal
from typing import Optional, Tuple

# Marcadores de "se muestra así ¦ se dice asá" dentro de la narración.
ABRE, MEDIO, CIERRA = "⟦", "¦", "⟧"

_UNIDADES = [
    "cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve", "diez", "once",
    "doce", "trece", "catorce", "quince", "dieciséis", "diecisiete", "dieciocho", "diecinueve", "veinte",
    "veintiuno", "veintidós", "veintitrés", "veinticuatro", "veinticinco", "veintiséis", "veintisiete",
    "veintiocho", "veintinueve",
]
_DECENAS = ["", "", "veinte", "treinta", "cuarenta", "cincuenta", "sesenta", "setenta", "ochenta", "noventa"]
_CENTENAS = [
    "", "ciento", "doscientos", "trescientos", "cuatrocientos", "quinientos", "seiscientos", "setecientos",
    "ochocientos", "novecientos",
]


def entero_en_palabras(n: int) -> str:
    if n < 0:
        return "menos " + entero_en_palabras(-n)
    if n < 30:
        return _UNIDADES[n]
    if n < 100:
        d, u = divmod(n, 10)
        return _DECENAS[d] if u == 0 else f"{_DECENAS[d]} y {_UNIDADES[u]}"
    if n == 100:
        return "cien"
    if n < 1000:
        c, r = divmod(n, 100)
        return _CENTENAS[c] + ("" if r == 0 else " " + entero_en_palabras(r))
    if n < 1_000_000:
        m, r = divmod(n, 1000)
        cab = "mil" if m == 1 else _apocopa(entero_en_palabras(m)) + " mil"
        return cab + ("" if r == 0 else " " + entero_en_palabras(r))
    m, r = divmod(n, 1_000_000)
    cab = "un millón" if m == 1 else _apocopa(entero_en_palabras(m)) + " millones"
    return cab + ("" if r == 0 else " " + entero_en_palabras(r))


def _apocopa(s: str) -> str:
    if s.endswith("veintiuno"):
        return s[:-9] + "veintiún"
    if s.endswith("uno"):
        return s[:-1]
    return s


def _decimales_en_palabras(dec: str) -> str:
    """'064' -> 'cero sesenta y cuatro'; '314' -> 'trescientos catorce'."""
    ceros = len(dec) - len(dec.lstrip("0"))
    resto = dec.lstrip("0")
    partes = ["cero"] * ceros
    if resto:
        if len(resto) <= 3:
            partes.append(entero_en_palabras(int(resto)))
        else:
            # Más de tres cifras: se dicen de dos en dos, como en clase
            trozos = [resto[i : i + 2] for i in range(0, len(resto), 2)]
            for t in trozos:
                if len(t) == 2 and t[0] == "0":
                    partes.extend(["cero", _UNIDADES[int(t[1])]])
                else:
                    partes.append(entero_en_palabras(int(t)))
    return " ".join(partes)


def decimal_en_palabras(txt: str) -> str:
    """Convierte '-2.83' o '0.064' en palabras."""
    neg = txt.startswith("-")
    t = txt.lstrip("+-")
    ent, _, dec = t.partition(".")
    s = entero_en_palabras(int(ent or "0"))
    if dec:
        s += " coma " + _decimales_en_palabras(dec)
    return ("menos " if neg else "") + s


def _redondea(x: float, dec: int) -> Decimal:
    q = Decimal(1).scaleb(-dec)
    return Decimal(repr(float(x))).quantize(q, rounding=ROUND_HALF_UP)


def partes(x: float, cifras: int = 4, dec: Optional[int] = None, sci: str = "auto") -> Tuple[str, Optional[int]]:
    """Devuelve (mantisa como texto con punto, exponente o None)."""
    x = float(x)
    if not math.isfinite(x):
        raise ValueError(f"Número no finito: {x}")
    if abs(x) < 1e-300:
        return (f"{_redondea(0.0, dec or 0)}", None)
    if dec is not None and sci != "si":
        s = str(_redondea(x, dec))
        return (s if float(s) != 0 else s.lstrip("-"), None)
    exp = math.floor(math.log10(abs(x)))
    usar_sci = sci == "si" or (sci == "auto" and (exp >= 5 or exp <= -3))
    if usar_sci:
        mant = x / 10.0 ** exp
        m = _redondea(mant, max(0, cifras - 1))
        if abs(m) >= 10:
            m = _redondea(float(m) / 10, max(0, cifras - 1))
            exp += 1
        return (str(m), exp)
    d = max(0, cifras - 1 - exp)
    m = _redondea(x, d)
    # 999.96 con 4 cifras -> 1000.0: se corrige la cifra de más
    if m != 0 and math.floor(math.log10(abs(float(m)))) > exp and d > 0:
        m = _redondea(x, d - 1)
    return (str(m), None)


def _agrupa(ent: str) -> str:
    neg = ent.startswith("-")
    e = ent.lstrip("-")
    if len(e) < 5:
        return ent
    g = []
    while e:
        g.insert(0, e[-3:])
        e = e[:-3]
    return ("-" if neg else "") + r"\,".join(g)


def _limpia(m: str, recorta: bool) -> str:
    if recorta and "." in m:
        m = m.rstrip("0").rstrip(".")
    return m


def tex(x: float, cifras: int = 4, dec: Optional[int] = None, u: str = "", sci: str = "auto",
        signo: bool = False, par: bool = False, recorta: bool = False) -> str:
    """Número en TeX con coma decimal. `u` es una unidad ya escrita ('J', 'W/(m·K)')."""
    m, e = partes(x, cifras, dec, sci)
    m = _limpia(m, recorta)
    ent, _, d = m.partition(".")
    s = _agrupa(ent) + (("{,}" + d) if d else "")
    if e is not None:
        s = f"{s}\\times 10^{{{e}}}"
    if signo and not s.startswith("-"):
        s = "+" + s
    if u:
        s = f"{s}\\ {unidad(u)}"
    if par and (s.startswith("-") or par == "si"):
        s = f"({s})"
    return s


_SUPER = str.maketrans("0123456789-+", "⁰¹²³⁴⁵⁶⁷⁸⁹⁻⁺")


def plano(x: float, cifras: int = 4, dec: Optional[int] = None, sci: str = "auto", recorta: bool = False) -> str:
    """Número legible en texto corrido: 2,83 × 10⁵."""
    m, e = partes(x, cifras, dec, sci)
    m = _limpia(m, recorta)
    ent, _, d = m.partition(".")
    s = _agrupa(ent).replace(r"\,", " ") + (("," + d) if d else "")
    s = s.replace("-", "−")
    if e is not None:
        s += " × 10" + str(e).translate(_SUPER)
    return s


def voz(x: float, cifras: int = 4, dec: Optional[int] = None, sci: str = "auto", recorta: bool = False) -> str:
    m, e = partes(x, cifras, dec, sci)
    s = decimal_en_palabras(_limpia(m, recorta))
    if e is not None:
        s += " por diez a la " + ("menos " if e < 0 else "") + entero_en_palabras(abs(e))
    return s


def n(x: float, cifras: int = 4, dec: Optional[int] = None, sci: str = "auto", recorta: bool = False) -> str:
    """Número para narrar: se muestra con dígitos y se dice con palabras."""
    return f"{ABRE}{plano(x, cifras, dec, sci, recorta)}{MEDIO}{voz(x, cifras, dec, sci, recorta)}{CIERRA}"


def dice(mostrar: str, decir: str) -> str:
    """Texto que se muestra de una forma y se pronuncia de otra."""
    return f"{ABRE}{mostrar}{MEDIO}{decir}{CIERRA}"


# ---------------------------------------------------------------- unidades

def unidad(u: str) -> str:
    """'J/(mol·K)' -> TeX con fracción; 'm3' -> m³; '°C' -> grados."""
    u = u.strip()
    if not u:
        return ""
    if u in ("°C", "C°", "oC"):
        return r"^\circ\text{C}"
    if u in ("1/°C", "°C-1", "°C^-1"):
        return r"^\circ\text{C}^{-1}"
    if u == "%":
        return r"\%"
    if "/" in u:
        a, b = u.split("/", 1)
        b = b.strip()
        if b.startswith("(") and b.endswith(")"):
            b = b[1:-1]
        return f"\\frac{{{unidad(a) if a and a != '1' else '1'}}}{{{unidad(b)}}}"
    if "·" in u:
        return r"\cdot".join(unidad(p) for p in u.split("·"))
    m = re.fullmatch(r"([A-Za-zµμΩÅ°%]+?)(?:\^?(-?\d+))?", u)
    if m:
        base = m.group(1)
        t = r"^\circ\text{C}" if base == "°C" else f"\\text{{{base}}}"
        return t + (f"^{{{m.group(2)}}}" if m.group(2) else "")
    return f"\\text{{{u}}}"


VOZ_UNIDAD = {
    "J": "joules", "kJ": "kilojoules", "W": "watts", "kW": "kilowatts", "Pa": "pascales", "kPa": "kilopascales",
    "atm": "atmósferas", "L": "litros", "mL": "mililitros", "m": "metros", "cm": "centímetros", "mm": "milímetros",
    "m2": "metros cuadrados", "cm2": "centímetros cuadrados", "m3": "metros cúbicos", "K": "kelvin", "°C": "grados Celsius",
    "kg": "kilogramos", "g": "gramos", "mol": "moles", "s": "segundos", "min": "minutos", "%": "por ciento",
    "J/K": "joules por kelvin", "MJ": "megajoules", "W/m": "watts por metro", "K/m": "kelvin por metro",
    "W/(m·K)": "watts por metro kelvin", "J/kg": "joules por kilogramo", "J/(kg·K)": "joules por kilogramo kelvin",
    "J/(mol·K)": "joules por mol kelvin", "m/s": "metros por segundo",
}


def nu(x: float, u: str, cifras: int = 4, dec: Optional[int] = None, sci: str = "auto", recorta: bool = False) -> str:
    """Número con unidad para narrar: '25,1 kJ' ¦ 'veinticinco coma uno kilojoules'."""
    m = plano(x, cifras, dec, sci, recorta)
    v = voz(x, cifras, dec, sci, recorta)
    uv = VOZ_UNIDAD.get(u, u)
    if v == "uno":
        v = "un" if u not in ("atm",) else "una"
        uv = {"J": "joule", "atm": "atmósfera", "m": "metro", "kg": "kilogramo", "s": "segundo", "min": "minuto", "mol": "mol", "W": "watt", "L": "litro", "K": "kelvin"}.get(u, uv)
    visible = u.replace("m3", "m³").replace("m2", "m²")
    return f"{ABRE}{m} {visible}{MEDIO}{v} {uv}{CIERRA}"


# ---------------------------------------------------------------- narración

_TOK = re.compile(re.escape(ABRE) + r"(.*?)" + re.escape(MEDIO) + r"(.*?)" + re.escape(CIERRA), re.S)


def separa(narracion: str):
    """Devuelve (texto_visible, texto_hablado, trozos) donde trozos = [(visible, hablado)] por palabra."""
    trozos = []
    i = 0
    for m in _TOK.finditer(narracion):
        for p in narracion[i : m.start()].split():
            trozos.append((p, p))
        trozos.append((m.group(1).strip(), m.group(2).strip()))
        i = m.end()
    for p in narracion[i:].split():
        trozos.append((p, p))
    # La puntuación pegada a un número marcado se une a él ("⟦5¦cinco⟧," -> "5,")
    unidos = []
    for vis, hab in trozos:
        if unidos and re.fullmatch(r"[.,;:!?)»]+", vis):
            a, b = unidos[-1]
            unidos[-1] = (a + vis, b + vis)
        else:
            unidos.append((vis, hab))
    visible = " ".join(t[0] for t in unidos)
    hablado = " ".join(t[1] for t in unidos)
    return visible, hablado, unidos
