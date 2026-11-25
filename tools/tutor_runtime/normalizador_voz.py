"""Normalizador independiente de matemática y física para voz natural en español.

Convierte ecuaciones, unidades del SI, notación científica y símbolos griegos/matemáticos en
español hablado fluido ANTES de enviar el texto a cualquier motor de voz (Edge Neural,
ZONOS3-Mini, VoxCPM2, Qwen3-TTS, Kokoro-82M o Gemini), sin alterar la prosa en español que ya
esté redactada para leerse en voz alta.
"""

from __future__ import annotations

import re
from typing import Tuple

from .verificador_numerico import limpiar_voz_latex

# Símbolos Unicode sueltos que pueden quedar fuera de bloques LaTeX
_UNICODE_FISICA: Tuple[Tuple[str, str], ...] = (
    ("Δ", " delta "),
    ("δ", " delta "),
    ("γ", " gama "),
    ("Γ", " gama "),
    ("σ", " sigma "),
    ("Σ", " suma "),
    ("η", " eta "),
    ("ε", " épsilon "),
    ("π", " pi "),
    ("α", " alfa "),
    ("β", " beta "),
    ("λ", " lambda "),
    ("μ", " micro "),
    ("ρ", " ro "),
    ("θ", " teta "),
    ("φ", " fi "),
    ("ω", " omega "),
    ("Ω", " ohm "),
    ("∮", " integral cerrada de "),
    ("∫", " integral de "),
    ("√", " raíz cuadrada de "),
    ("∞", " infinito "),
    ("≈", " aproximadamente "),
    ("≠", " distinto de "),
    ("≤", " menor o igual que "),
    ("≥", " mayor o igual que "),
    ("⟹", " entonces "),
    ("⇒", " entonces "),
    ("→", " a "),
    ("↔", " equivale a "),
    ("±", " más o menos "),
    ("∝", " proporcional a "),
    ("×", " por "),
)

# Unidades físicas: las compuestas primero; las simples exigen número previo para no tocar prosa
_UNIDADES_COMPUESTAS: Tuple[Tuple[str, str], ...] = (
    (r"(?<=[\d\s)])J\s*/\s*\(\s*mol\s*[·⋅*]\s*K\s*\)", " joules por mol kelvin"),
    (r"(?<=[\d\s)])J\s*/\s*\(\s*kg\s*[·⋅*]\s*(?:°C|K)\s*\)", " joules por kilogramo kelvin"),
    (r"(?<=[\d\s)])W\s*/\s*\(\s*m\s*[·⋅*]\s*(?:°C|K)\s*\)", " vatios por metro kelvin"),
    (r"(?<=[\d\s)])W\s*/\s*m\s*(?:²|\^2)\s*[·⋅*]?\s*K\s*(?:⁴|\^4)", " vatios por metro cuadrado kelvin a la cuarta"),
    (r"(?<=[\d\s)])W\s*/\s*m\s*(?:²|\^2)", " vatios por metro cuadrado"),
    (r"(?<=[\d\s)])cal\s*/\s*\(\s*g\s*[·⋅*]\s*°C\s*\)", " calorías por gramo grado Celsius"),
    (r"(?<=[\d\s)])cal\s*/\s*g\b", " calorías por gramo"),
    (r"(?<=[\d\s)])kg\s*/\s*m\s*(?:³|\^3)", " kilogramos por metro cúbico"),
    (r"(?<=[\d\s)])g\s*/\s*cm\s*(?:³|\^3)", " gramos por centímetro cúbico"),
    (r"(?<=[\d\s)])m\s*/\s*s\s*(?:²|\^2)", " metros por segundo al cuadrado"),
    (r"(?<=[\d\s)])m\s*/\s*s\b", " metros por segundo"),
    (r"(?<=[\d\s)])kW\s*/\s*m\b", " kilovatios por metro"),
)

_UNIDADES_TRAS_NUMERO: Tuple[Tuple[str, str], ...] = (
    (r"°\s*C\b", "grados Celsius"),
    (r"°\s*F\b", "grados Fahrenheit"),
    (r"°\b", "grados"),
    (r"cm\s*(?:³|\^3)", "centímetros cúbicos"),
    (r"cm\s*(?:²|\^2)", "centímetros cuadrados"),
    (r"mm\s*(?:²|\^2)", "milímetros cuadrados"),
    (r"m\s*(?:³|\^3)", "metros cúbicos"),
    (r"m\s*(?:²|\^2)", "metros cuadrados"),
    (r"m\s*(?:⁻¹|\^-1)", "por metro"),
    (r"K\s*(?:⁻¹|\^-1)", "por kelvin"),
    (r"kW\b", "kilovatios"),
    (r"MW\b", "megavatios"),
    (r"kJ\b", "kilojoules"),
    (r"MJ\b", "megajoules"),
    (r"GJ\b", "gigajoules"),
    (r"kPa\b", "kilopascales"),
    (r"MPa\b", "megapascales"),
    (r"Pa\b", "pascales"),
    (r"atm\b", "atmósferas"),
    (r"kcal\b", "kilocalorías"),
    (r"cm\b", "centímetros"),
    (r"mm\b", "milímetros"),
    (r"kg\b", "kilogramos"),
    (r"mol\b", "moles"),
    (r"min\b", "minutos"),
    (r"J\b", "joules"),
    (r"W\b", "vatios"),
    (r"K\b", "kelvin"),
)

_SUBINDICES_FISICA: Tuple[Tuple[str, str], ...] = (
    (r"\bC_\{?V\}?\b", "C sub V"),
    (r"\bC_\{?p\}?\b", "C sub p"),
    (r"\bT_\{?f\}?\b", "T final"),
    (r"\bT_\{?i\}?\b", "T inicial"),
    (r"\bT_\{?H\}?\b", "T caliente"),
    (r"\bT_\{?C\}?\b", "T fría"),
    (r"\bQ_\{?H\}?\b", "Q caliente"),
    (r"\bQ_\{?C\}?\b", "Q frío"),
    (r"\bV_\{?f\}?\b", "V final"),
    (r"\bV_\{?i\}?\b", "V inicial"),
)

# Patrones de alerta o trampa física para ajustar la prosodia automáticamente
_RE_ALERTA = re.compile(
    r"\b(?:ojo|cuidado|trampa|viola|no es posible|error com[uú]n|nunca uses|no confundas|atenci[oó]n)\b",
    re.I,
)
_RE_DESPACIO = re.compile(
    r"\b(?:paso a paso|despejamos|cambio de variable|separamos variables|sustituimos con calma)\b",
    re.I,
)


def _expandir_notacion_cientifica(texto: str) -> str:
    def _rep(m: re.Match[str]) -> str:
        mant = m.group(1)
        sig = "menos " if m.group(2) in ("-", "−") else ""
        exp = m.group(3)
        return f"{mant} por diez a la {sig}{exp}"

    return re.sub(
        r"(\d+(?:[.,]\d+)?)\s*[×xX*]\s*10\s*\^?\s*\{?([-−+]?)(\d+)\}?",
        _rep,
        texto,
    )


def _expandir_potencias_unicode(texto: str) -> str:
    return (
        texto.replace("²", " al cuadrado ")
        .replace("³", " al cubo ")
        .replace("⁴", " a la cuarta ")
    )


def normalizar_para_voz(texto: str) -> str:
    """Normaliza ecuaciones, símbolos y unidades a español hablado claro para cualquier motor TTS."""
    s = str(texto or "").strip()
    if not s:
        return ""

    # 1. Subíndices frecuentes en termodinámica antes de limpiar LaTeX
    for patron, reemplazo in _SUBINDICES_FISICA:
        s = re.sub(patron, reemplazo, s)

    # 2. Si hay delimitadores o comandos LaTeX, conviértelos a español sin código
    if re.search(r"[$\\]|[_^]\{", s):
        s = limpiar_voz_latex(s, preservar_cues=False)

    # 3. Notación científica explícita (3,6×10^6 -> 3,6 por diez a la 6)
    s = _expandir_notacion_cientifica(s)

    # 4. Unidades físicas compuestas (p. ej. J/(mol·K)) primero
    for patron, reemplazo in _UNIDADES_COMPUESTAS:
        s = re.sub(patron, reemplazo, s)

    # 5. Unidades físicas simples únicamente cuando siguen a un número o potencia
    for patron, reemplazo in _UNIDADES_TRAS_NUMERO:
        s = re.sub(rf"(\d)\s*(?:{patron})", rf"\1 {reemplazo}", s)

    # 6. Símbolos griegos y operadores matemáticos Unicode
    for sim, pal in _UNICODE_FISICA:
        if sim in s:
            s = s.replace(sim, pal)

    # 7. Potencias residuales Unicode (T⁴ -> T a la cuarta)
    s = _expandir_potencias_unicode(s)

    # 8. Operadores sueltos en ecuaciones en texto plano
    s = re.sub(r"\s*=\s*", " es igual a ", s)
    s = re.sub(r"(?<=\S)\s+[-−]\s+(?=\S)", " menos ", s)

    # 9. Limpieza tipográfica y pausas limpias
    s = s.replace("·", " ").replace("⋅", " ")
    s = re.sub(r"\s+([.,;:!?])", r"\1", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s


def detectar_estilo_pedagogico(texto: str, estilo_explicito: str = "") -> str:
    """Elige el estilo prosódico (`normal`, `despacio`, `alerta`, `enfasis`) para un tramo."""
    ex = (estilo_explicito or "").strip().lower()
    if ex in ("normal", "despacio", "alerta", "enfasis"):
        return ex
    t = texto or ""
    if _RE_ALERTA.search(t):
        return "alerta"
    if _RE_DESPACIO.search(t):
        return "despacio"
    return "normal"
