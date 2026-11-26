"""Formulario oficial exacto del III Parcial de Física General III (FI2103 · II-2026).

Fuente verificada visualmente página por página:
  `D:\\Descargas\\documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf`
  ("Ecuaciones y constantes importantes", 14 filas × 3 columnas = 42 celdas).

Regla académica del examen:
  Toda solución de desarrollo debe partir exclusivamente de estas 35 ecuaciones base y
  7 constantes/conversiones. Cualquier otra expresión (resistencia térmica, cilindro/esfera
  integrados, ΔA = 2αA₀ΔT, β = 3α, C_p = γR/(γ−1), C_V = R/(γ−1), pM = ρRT, eficiencia de
  Carnot, etc.) debe obtenerse explícitamente igualando, sustituyendo o despejando a partir
  de las ecuaciones base de esta hoja.
"""

from __future__ import annotations

import re
from typing import Any, Dict, List, Sequence, Tuple

# Recorte normalizado de la página 1 del PDF oficial (mide exacto la tabla + título)
CROP_NORM: Tuple[float, float, float, float] = (0.14332, 0.08099, 0.85936, 0.89339)
# Relación de aspecto de la hoja recortada (1339 × 1966 px → viewBox 1000 × 1468)
HOJA_ASPECTO: Tuple[int, int] = (1000, 1468)

# Paleta semántica armoniosa (coincide con las claves de `paleta.js` para Claro, Oscuro y Alto contraste)
COLORES_FORMULARIO: Tuple[str, ...] = ("azul", "rojo", "verde", "naranja", "morado", "turquesa")

# Las 35 ecuaciones base exactas impresas en la hoja oficial II-2026
ECUACIONES_BASE_II_2026: Tuple[Tuple[str, str], ...] = (
    # Dilatación y esfuerzo térmico (Fila 1)
    ("F1C1", r"\Delta L = \alpha L_0 \Delta T"),
    ("F1C2", r"\Delta V = \beta V_0 \Delta T"),
    ("F1C3", r"\frac{F}{A} = -Y \alpha \Delta T"),
    # Conducción y radiación (Filas 2 y 3)
    ("F2C1", r"H = \frac{dQ}{dt} = kA\frac{T_H - T_C}{L}"),
    ("F2C2", r"H = -kA\frac{dT}{dx}"),
    ("F2C3", r"H = Ae\sigma T^4"),
    ("F3C1", r"H = Ae\sigma (T^4 - T_s^4)"),
    # Calorimetría (Fila 3)
    ("F3C2", r"Q = mc\Delta T = nC\Delta T"),
    ("F3C3", r"Q = \pm mL"),
    # Gases ideales y teoría cinética (Filas 4 y 5)
    ("F4C1", r"pV = nRT"),
    ("F4C2", r"m_{\text{total}} = nM"),
    ("F4C3", r"K = \frac{3}{2}nRT"),
    ("F5C1", r"v_{\text{rms}} = \sqrt{\frac{3k_B T}{m}} = \sqrt{\frac{3RT}{M}}"),
    ("F5C2", r"\frac{1}{2}m(v^2)_{\text{med}} = \frac{3k_B T}{2}"),
    ("F5C3", r"M = N_A m"),
    # Trabajo termodinámico (Fila 6)
    ("F6C1", r"W = \int_{V_1}^{V_2} p\,dV"),
    ("F6C2", r"W = p(V_2 - V_1)"),
    ("F6C3", r"W = nRT\ln\left(\frac{V_2}{V_1}\right)"),
    # Primera ley y calores específicos (Filas 7 y 8)
    ("F7C1", r"\Delta U = Q - W"),
    ("F7C2", r"\Delta U = nC_V\Delta T"),
    ("F7C3", r"C_P = C_V + R"),
    ("F8C1", r"C_{V\text{mono}} = \frac{3}{2}R"),
    ("F8C2", r"C_{V\text{dia}} = \frac{5}{2}R"),
    ("F8C3", r"\gamma = \frac{C_P}{C_V}"),
    # Procesos adiabáticos (Fila 9)
    ("F9C1", r"W = \frac{1}{\gamma - 1}(p_1 V_1 - p_2 V_2)"),
    ("F9C2", r"pV^\gamma = \text{constante}"),
    ("F9C3", r"TV^{\gamma - 1} = \text{constante}"),
    # Máquinas térmicas, refrigeradores, calor diferencial y Otto (Filas 10, 11 y 12)
    ("F10C1", r"e = \frac{W}{Q_H} = 1 - \left|\frac{Q_c}{Q_H}\right|"),
    ("F10C2", r"K = \frac{|Q_c|}{|W|} = \frac{|Q_c|}{|Q_H| - |Q_c|}"),
    ("F10C3", r"T_K = T_{^\circ\text{C}} + 273{,}15"),
    ("F11C1", r"dQ = nC_V dT"),
    ("F11C2", r"dQ = nC_p dT"),
    ("F11C3", r"e_{\text{otto}} = 1 - \frac{1}{r^{\gamma - 1}}"),
    ("F12C1", r"r = \frac{V_{\text{máx}}}{V_{\text{mín}}}"),
    ("F12C2", r"T_{^\circ\text{F}} = \frac{9}{5}T_{^\circ\text{C}} + 32"),
)

# Las 7 constantes y conversiones impresas al pie de la hoja oficial II-2026 (completan las 42 celdas)
CONSTANTES_II_2026: Tuple[Tuple[str, str], ...] = (
    ("F12C3", r"R = 8{,}314\ \frac{\text{J}}{\text{mol}\cdot\text{K}}"),
    ("F13C1", r"k_B = 1{,}38\times 10^{-23}\ \frac{\text{J}}{\text{K}}"),
    ("F13C2", r"N_A = 6{,}022\times 10^{23}\ \text{mol}^{-1}"),
    ("F13C3", r"\sigma = 5{,}67\times 10^{-8}\ \frac{\text{W}}{\text{m}^2\cdot\text{K}^4}"),
    ("F14C1", r"1\ \text{atm} = 101325\ \text{Pa}"),
    ("F14C2", r"1\ \text{L} = 1000\ \text{cm}^3"),
    ("F14C3", r"R = 0{,}08206\ \frac{\text{L}\cdot\text{atm}}{\text{mol}\cdot\text{K}}"),
)

# Cajas normalizadas [x0, y0, x1, y1] dentro de la hoja recortada, medidas sobre el PDF real
CAJAS_CELDAS: Dict[str, Tuple[float, float, float, float]] = {
    "F1C1": (0.04182, 0.06765, 0.35400, 0.13225),
    "F1C2": (0.35400, 0.06765, 0.65646, 0.13225),
    "F1C3": (0.65646, 0.06765, 0.95818, 0.13225),
    "F2C1": (0.04182, 0.13225, 0.35400, 0.19634),
    "F2C2": (0.35400, 0.13225, 0.65646, 0.19634),
    "F2C3": (0.65646, 0.13225, 0.95818, 0.19634),
    "F3C1": (0.04182, 0.19634, 0.35400, 0.26094),
    "F3C2": (0.35400, 0.19634, 0.65646, 0.26094),
    "F3C3": (0.65646, 0.19634, 0.95818, 0.26094),
    "F4C1": (0.04182, 0.26094, 0.35400, 0.32553),
    "F4C2": (0.35400, 0.26094, 0.65646, 0.32553),
    "F4C3": (0.65646, 0.26094, 0.95818, 0.32553),
    "F5C1": (0.04182, 0.32553, 0.35400, 0.38962),
    "F5C2": (0.35400, 0.32553, 0.65646, 0.38962),
    "F5C3": (0.65646, 0.32553, 0.95818, 0.38962),
    "F6C1": (0.04182, 0.38962, 0.35400, 0.45422),
    "F6C2": (0.35400, 0.38962, 0.65646, 0.45422),
    "F6C3": (0.65646, 0.38962, 0.95818, 0.45422),
    "F7C1": (0.04182, 0.45422, 0.35400, 0.51882),
    "F7C2": (0.35400, 0.45422, 0.65646, 0.51882),
    "F7C3": (0.65646, 0.45422, 0.95818, 0.51882),
    "F8C1": (0.04182, 0.51882, 0.35400, 0.58291),
    "F8C2": (0.35400, 0.51882, 0.65646, 0.58291),
    "F8C3": (0.65646, 0.51882, 0.95818, 0.58291),
    "F9C1": (0.04182, 0.58291, 0.35400, 0.64700),
    "F9C2": (0.35400, 0.58291, 0.65646, 0.64700),
    "F9C3": (0.65646, 0.58291, 0.95818, 0.64700),
    "F10C1": (0.04182, 0.64700, 0.35400, 0.71211),
    "F10C2": (0.35400, 0.64700, 0.65646, 0.71211),
    "F10C3": (0.65646, 0.64700, 0.95818, 0.71211),
    "F11C1": (0.04182, 0.71211, 0.35400, 0.77620),
    "F11C2": (0.35400, 0.71211, 0.65646, 0.77620),
    "F11C3": (0.65646, 0.71211, 0.95818, 0.77620),
    "F12C1": (0.04182, 0.77620, 0.35400, 0.84028),
    "F12C2": (0.35400, 0.77620, 0.65646, 0.84028),
    "F12C3": (0.65646, 0.77620, 0.95818, 0.84028),
    "F13C1": (0.04182, 0.84028, 0.35400, 0.90437),
    "F13C2": (0.35400, 0.84028, 0.65646, 0.90437),
    "F13C3": (0.65646, 0.84028, 0.95818, 0.90437),
    "F14C1": (0.04182, 0.90437, 0.35400, 0.96948),
    "F14C2": (0.35400, 0.90437, 0.65646, 0.96948),
    "F14C3": (0.65646, 0.90437, 0.95818, 0.96948),
}

NOMBRES_CELDAS: Dict[str, str] = {
    "F1C1": "Dilatación lineal",
    "F1C2": "Dilatación volumétrica",
    "F1C3": "Esfuerzo térmico",
    "F2C1": "Conducción estacionaria",
    "F2C2": "Ley de Fourier diferencial",
    "F2C3": "Radiación emitida",
    "F3C1": "Radiación neta",
    "F3C2": "Calor sensible",
    "F3C3": "Calor latente (cambio de fase)",
    "F4C1": "Ley del gas ideal",
    "F4C2": "Masa total y moles",
    "F4C3": "Energía cinética total",
    "F5C1": "Rapidez cuadrática media",
    "F5C2": "Energía cinética molecular",
    "F5C3": "Masa molar y Avogadro",
    "F6C1": "Trabajo termodinámico (integral)",
    "F6C2": "Trabajo isobárico",
    "F6C3": "Trabajo isotérmico",
    "F7C1": "Primera ley",
    "F7C2": "Energía interna (gas ideal)",
    "F7C3": "Relación de Mayer",
    "F8C1": "Calor molar monoatómico",
    "F8C2": "Calor molar diatómico",
    "F8C3": "Razón adiabática γ",
    "F9C1": "Trabajo adiabático",
    "F9C2": "Adiabática p–V",
    "F9C3": "Adiabática T–V",
    "F10C1": "Eficiencia térmica",
    "F10C2": "Rendimiento (refrigerador)",
    "F10C3": "Conversión a Kelvin",
    "F11C1": "Calor isocórico",
    "F11C2": "Calor isobárico",
    "F11C3": "Eficiencia ciclo Otto",
    "F12C1": "Razón de compresión",
    "F12C2": "Escala Fahrenheit",
    "F12C3": "Constante R (SI)",
    "F13C1": "Constante de Boltzmann",
    "F13C2": "Número de Avogadro",
    "F13C3": "Constante Stefan–Boltzmann",
    "F14C1": "Equivalencia atm → Pa",
    "F14C2": "Equivalencia L → cm³",
    "F14C3": "Constante R (L·atm)",
}

_TODAS_LAS_CELDAS: Tuple[Tuple[str, str, bool], ...] = tuple(
    [(cid, tex, False) for cid, tex in ECUACIONES_BASE_II_2026]
    + [(cid, tex, True) for cid, tex in CONSTANTES_II_2026]
)
_POR_ID: Dict[str, Tuple[str, bool, int, int]] = {}
for _cid, _tex, _es_c in _TODAS_LAS_CELDAS:
    _m = re.fullmatch(r"F(\d+)C(\d+)", _cid)
    _POR_ID[_cid] = (_tex, _es_c, int(_m.group(1)), int(_m.group(2)))

FORMULARIO_PROMPT_TXT = """REGLA OBLIGATORIA DEL FORMULARIO OFICIAL III PARCIAL FI2103 II-2026 (Ecuaciones_Tercer_Parcial_FG3.pdf):
En problemas de desarrollo de Física General III, SOLO puedes partir de las ecuaciones base impresas en el formulario oficial:
- Dilatación/esfuerzo: ΔL = α L₀ ΔT ; ΔV = β V₀ ΔT ; F/A = −Y α ΔT
- Conducción/radiación: H = dQ/dt = kA(T_H − T_C)/L ; H = −kA dT/dx ; H = AeσT⁴ ; H = Aeσ(T⁴ − T_s⁴)
- Calorimetría: Q = mcΔT = nCΔT ; Q = ±mL
- Gases ideales y cinética: pV = nRT ; m_total = nM ; K = (3/2)nRT ; v_rms = √(3k_B T/m) = √(3RT/M) ; (1/2)m(v²)_med = 3k_B T/2 ; M = N_A m
- Trabajo y 1.ª ley: W = ∫ p dV ; W = p(V₂ − V₁) ; W = nRT ln(V₂/V₁) ; ΔU = Q − W ; ΔU = n C_V ΔT ; C_P = C_V + R ; C_Vmono = (3/2)R ; C_Vdia = (5/2)R ; γ = C_P/C_V ; dQ = n C_V dT ; dQ = n C_p dT
- Adiabáticos: W = (1/(γ − 1))(p₁V₁ − p₂V₂) ; p V^γ = cte ; T V^(γ−1) = cte
- Máquinas/Otto: e = W/Q_H = 1 − |Q_c/Q_H| ; K = |Q_c|/|W| = |Q_c|/(|Q_H| − |Q_c|) ; e_otto = 1 − 1/r^(γ−1) ; r = V_máx/V_mín
- Temperaturas y constantes: T_K = T_°C + 273,15 ; T_°F = (9/5)T_°C + 32 ; R = 8,314 J/(mol·K) = 0,08206 L·atm/(mol·K) ; k_B = 1,38×10⁻²³ J/K ; N_A = 6,022×10²³ mol⁻¹ ; σ = 5,67×10⁻⁸ W/(m²·K⁴) ; 1 atm = 101325 Pa ; 1 L = 1000 cm³.

ESTÁ PROHIBIDO sacar de la nada atajos que NO están en esta hoja sin derivarlos antes desde las ecuaciones base:
1) Resistencia térmica (R_th = L/(kA), R_cil, R_esf): parte de H = kA(T_H − T_C)/L o H = −kA dT/dr, despeja ΔT o iguala H₁ = H₂ y suma caídas.
2) Conducción cilíndrica/esférica/cónica/área variable: parte SIEMPRE de H = −kA dT/dx (o dT/dr), sustituye A(r) o A(x), separa variables e integra.
3) ΔA = 2α A₀ ΔT y β = 3α: parte de ΔL = α L₀ ΔT (L = L₀(1 + αΔT)) y eleva al cuadrado o al cubo despreciando (αΔT)².
4) C_p = γR/(γ − 1) y C_V = R/(γ − 1): despeja a partir de C_P = C_V + R y γ = C_P/C_V.
5) ρ = pM/(RT): despeja a partir de pV = nRT y m_total = nM.
6) W = nRT ln(p₁/p₂): parte de W = nRT ln(V₂/V₁) y sustituye V₂/V₁ = p₁/p₂ desde pV = nRT.
7) Eficiencia o K de Carnot (1 − T_C/T_H, T_C/(T_H − T_C)): NO están impresas; parte de e = 1 − |Q_c/Q_H| o K = |Q_c|/(|Q_H| − |Q_c|) y deduce |Q_c/Q_H| = T_C/T_H con las isotérmicas y adiabáticas (o indica explícitamente que Carnot no figura en el formulario II-2026)."""

# Reglas de verificación: (nombre, regex_atajo_no_en_hoja, regex_derivacion_desde_hoja)
_ATAJOS_SIN_DERIVAR: Tuple[Tuple[str, re.Pattern[str], re.Pattern[str]], ...] = (
    (
        "Resistencia térmica (R_th / R_cil / R_esf) usada sin partir de H = kAΔT/L o H = −kA dT/dr",
        re.compile(r"R_\{?(?:th|t|cil|esf|eq|tot)\}?|\\frac\{L_?\d?\}\{k_?\d?A"),
        re.compile(r"H\s*=\s*-?\s*k\s*A|H\s*=\s*k_?\d?\s*A|H_1\s*=\s*H_2|\\frac\{dT\}\{d[xr]\}"),
    ),
    (
        "Fórmula cilíndrica 2πkLΔT/ln(r₂/r₁) sin derivar integrando H = −kA dT/dr",
        re.compile(r"2\\pi\s*k\s*L.*\\ln"),
        re.compile(r"\\int|\\frac\{dT\}\{dr\}"),
    ),
    (
        "Fórmula esférica 4πkr₁r₂ΔT/(r₂−r₁) sin derivar integrando H = −kA dT/dr",
        re.compile(r"4\\pi\s*k.*(?:\\frac\{1\}\{r|r_1\s*r_2)"),
        re.compile(r"\\int|\\frac\{dT\}\{dr\}"),
    ),
    (
        "Cono truncado πkR₀R₁ΔT/L sin derivar integrando H = −kA(x) dT/dx",
        re.compile(r"\\pi\s*k_?[A-Za-z]?\s*R_0\s*R_1"),
        re.compile(r"\\int|\\frac\{dT\}\{dx\}"),
    ),
    (
        "C_p = γR/(γ−1) o C_V = R/(γ−1) sin despejar desde C_P = C_V + R y γ = C_P/C_V",
        re.compile(r"C_[pP]\s*=\s*\\frac\{\\gamma\s*R\}\{\\gamma\s*[-−]\s*1\}|C_V\s*=\s*\\frac\{R\}\{\\gamma\s*[-−]\s*1\}"),
        re.compile(r"C_[pP]\s*=\s*C_V\s*\+\s*R|C_[pP]\s*[-−]\s*C_V\s*=\s*R"),
    ),
    (
        "ΔA = 2αA₀ΔT usada sin derivarla desde ΔL = αL₀ΔT",
        re.compile(r"\\Delta\s*A\s*=\s*2\\alpha"),
        re.compile(r"\(1\s*\+\s*\\alpha\s*(?:\\,)?\s*\\Delta\s*T\)\^2|\\Delta\s*L\s*=\s*\\alpha\s*L_0"),
    ),
    (
        "β = 3α usada sin derivarla desde ΔL = αL₀ΔT",
        re.compile(r"\\beta(?:_\{?[A-Za-z]+\}?)?\s*=\s*3\\alpha"),
        re.compile(r"\(1\s*\+\s*\\alpha\s*(?:\\,)?\s*\\Delta\s*T\)\^3|L_0\^3|\\Delta\s*L\s*=\s*\\alpha\s*L_0"),
    ),
    (
        "ρ = pM/(RT) usada sin despejar desde pV = nRT y m_total = nM",
        re.compile(r"\\rho\s*=\s*\\frac\{p\s*M\}\{R\s*T\}|p\s*M\s*=\s*\\rho\s*R\s*T"),
        re.compile(r"p\s*V\s*=\s*n\s*R\s*T"),
    ),
    (
        "W = nRT ln(p₁/p₂) sin pasar por W = nRT ln(V₂/V₁) y pV = nRT",
        re.compile(r"n\s*R\s*T\s*\\ln\s*(?:\\left)?\(\s*\\frac\{p_1\}\{p_2\}"),
        re.compile(r"\\frac\{V_2\}\{V_1\}|p_1\s*V_1\s*=\s*p_2\s*V_2"),
    ),
    (
        "Eficiencia de Carnot e = 1 − T_C/T_H usada sin enlazarla con e = 1 − |Q_c/Q_H|",
        re.compile(r"e_\{?(?:\\text\{)?Carnot\}?\}?\s*=\s*1\s*[-−]\s*\\frac\{T"),
        re.compile(r"\\left\|\\frac\{Q_[cC]\}\{Q_H\}\s*\\right\||\|Q_[cC]/Q_H\||\\frac\{\|Q_[cC]\|\}\{Q_H\}|e\s*=\s*\\frac\{W"),
    ),
)


def verificar_origen_formulario(lec: Dict[str, Any]) -> List[str]:
    """Comprueba que toda lección de desarrollo parta de las ecuaciones base del formulario II-2026."""
    if lec.get("tipo") in ("seleccion", "teoria"):
        return []
    mates: List[str] = []
    for p in lec.get("pasos") or []:
        for tr in p.get("tramos") or []:
            for acc in tr.get("acc") or []:
                if isinstance(acc, dict) and acc.get("a") == "mate" and acc.get("tex"):
                    mates.append(str(acc["tex"]))
        for l in p.get("lento") or []:
            if isinstance(l, dict) and l.get("tex"):
                mates.append(str(l["tex"]))
    if not mates:
        return []
    blob = "\n".join(mates)
    faltas: List[str] = []
    for desc, pat_atajo, pat_base in _ATAJOS_SIN_DERIVAR:
        if pat_atajo.search(blob) and not pat_base.search(blob):
            faltas.append(desc)
    return faltas


# ------------------------------------------------------------------ mapeo fino a las 42 celdas

# (celda_id, regex_en_tex_o_texto)
_REGLAS_CELDAS: Tuple[Tuple[str, re.Pattern[str]], ...] = (
    # Fila 1: Dilatación y esfuerzo
    ("F1C1", re.compile(r"\\Delta\s*[LRD]\s*=\s*\\alpha|\\alpha\s*(?:\\,)?\s*[LRD]_?0?\s*(?:\\,)?\s*\\Delta\s*T|\(1\s*\+\s*\\alpha\s*(?:\\,)?\s*\\Delta\s*T\)|\\frac\{\\Delta\s*[LRD]\}\{[LRD]_0\}|dL\s*=\s*\\alpha|\\int\s*\\alpha\s*(?:\\,)?L_0|\\Delta\s*A\s*=\s*2\\alpha|\\bar\\alpha", re.I)),
    ("F1C2", re.compile(r"\\Delta\s*V(?:_\{?[A-Za-z0-9,]+\}?)?\s*=\s*(?:\\beta|3\\alpha)|\(?1\s*\+\s*\\beta\s*(?:\\,)?\s*\\Delta\s*T\)?|\\beta_?\{?[A-Za-z0-9]*\}?\s*V_?0|\\beta\s*=\s*3\\alpha", re.I)),
    ("F1C3", re.compile(r"\\frac\{F\}\{A\}\s*=\s*-?\s*Y\s*\\alpha|Y\s*\\alpha\s*(?:\\,)?\s*\\Delta\s*T|esfuerzo\s+t[ée]rmic", re.I)),
    # Fila 2: Conducción y radiación
    ("F2C1", re.compile(r"\\frac\{dQ\}\{dt\}|H(?:_[A-Za-z0-9]+)?\s*=\s*k(?:_\{?[A-Za-z0-9]+\}?)?\s*A(?:_\{?[A-Za-z0-9]+\}?)?\s*\\frac\{[^}]*(?:T|Delta)|Q\s*=\s*H\s*(?:\\,)?\s*\\Delta\s*t|kA\\frac\{\\Delta\s*T\}\{L\}", re.I)),
    ("F2C2", re.compile(r"-\s*k\s*(?:A|\(r\)|\\pi|2\\pi|4\\pi)[^=\n]{0,40}\\frac\{dT\}\{d[xry]\}|\\frac\{dT\}\{d[xry]\}|H\s*=\s*-kA", re.I)),
    ("F2C3", re.compile(r"(?:H|P)(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*(?:A\s*e|e\s*A|\d*\\pi\s*r\^2\s*e)\s*\\sigma\s*T(?:_?[A-Za-z0-9])?\^4|Ae\\sigma\s*T\^4|Stefan[\-\u2013 ]Boltzmann", re.I)),
    # Fila 3: Radiación neta y calorimetría
    ("F3C1", re.compile(r"\\sigma\s*(?:\\left)?\(\s*T(?:_?[A-Za-z0-9])?\^4\s*[-−]\s*T_?[sa0]\^4|T\^4\s*[-−]\s*T_?[sa0]\^4|H_\{\\text\{neto\}\}", re.I)),
    ("F3C2", re.compile(r"Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*[-+]?\s*(?:m(?:_\{?[A-Za-z0-9]+\}?)?\s*c(?:_\{?[A-Za-z0-9]+\}?)?|n\s*C)\s*(?:\\,)?\s*(?:\\Delta\s*T|\(T|\(\d)|m_[A-Za-z]\s*c_[A-Za-z]\s*(?:\\,)?\s*\\Delta\s*T|mc\\Delta\s*T|nC\\Delta\s*T", re.I)),
    ("F3C3", re.compile(r"Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*(?:\\pm|[+-])?\s*m(?:_\{?[A-Za-z0-9]+\}?)?\s*L(?:_\{?[fFvV]\})?|m_?[fFvVhH]?\s*L_?[fFvV]|\\pm\s*mL|calor\s+latente", re.I)),
    # Fila 4: Gases ideales
    ("F4C1", re.compile(r"p(?:_\{?[A-Za-z0-9]+\}?)?\s*V(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*n\s*R\s*T|\\frac\{n\s*R\s*T(?:_?[A-Za-z0-9])?\}\{[pV]|\\frac\{p(?:_?[A-Za-z0-9])?\s*V(?:_?[A-Za-z0-9])?\}\{n\s*R\}|\\frac\{p_1\s*V_1\}\{T_1\}|p_1\s*V_1\s*=\s*p_2\s*V_2|ley\s+de\s+gas(?:es)?\s+ideal", re.I)),
    ("F4C2", re.compile(r"m_\{\\text\{total\}\}\s*=\s*n\s*M|n\s*=\s*\\frac\{m\}\{M\}|m\s*=\s*n\s*M|\\rho\s*=\s*\\frac\{p\s*M\}\{R\s*T\}", re.I)),
    ("F4C3", re.compile(r"K(?:_\{?(?:\\text\{)?tr\}?\}?)?\s*=\s*\\frac\{3\}\{2\}\s*n\s*R\s*T|\\frac\{3\}\{2\}\s*n\s*R\s*T", re.I)),
    # Fila 5: Teoría cinética
    ("F5C1", re.compile(r"v_\{?(?:\\text\{)?rms\}?\}?|\\sqrt\{\\frac\{3\s*(?:k_B|R)\s*T\}\{[mM]\}\}", re.I)),
    ("F5C2", re.compile(r"\(v\^2\)_\{?(?:\\text\{)?med\}?\}?|\\frac\{3\s*k_B\s*T\}\{2\}|\\frac\{3\}\{2\}\s*k_B\s*T|K_\{?med\}?", re.I)),
    ("F5C3", re.compile(r"M\s*=\s*N_A\s*m|m\s*=\s*\\frac\{M\}\{N_A\}|N\s*=\s*n\s*N_A", re.I)),
    # Fila 6: Trabajo termodinámico
    ("F6C1", re.compile(r"W\s*=\s*\\int|\\int_\{?V_?1\}?\^\{?V_?2\}?\s*p\s*(?:\\,)?\s*dV|\\frac\{p_[A-Za-z0-9]\s*\+\s*p_[A-Za-z0-9]\}\{2\}\s*(?:\\,)?\(V", re.I)),
    ("F6C2", re.compile(r"W(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*p(?:_\{?[A-Za-z0-9]+\}?)?\s*(?:\\,)?\s*\(\s*V_?[A-Za-z0-9]\s*[-−]\s*V_?[A-Za-z0-9]\s*\)|p\s*(?:\\,)?\s*\\Delta\s*V", re.I)),
    ("F6C3", re.compile(r"n\s*R\s*T(?:_?[A-Za-z0-9])?\s*\\ln\s*(?:\\left)?\(", re.I)),
    # Fila 7: Primera ley y C_P = C_V + R
    ("F7C1", re.compile(r"\\Delta\s*U(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*(?:Q(?:_\{?[A-Za-z0-9]+\}?)?|0)\s*[-−]\s*W|Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*\\Delta\s*U(?:_\{?[A-Za-z0-9]+\}?)?\s*\+\s*W|W(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*Q(?:_\{?[A-Za-z0-9]+\}?)?\s*[-−]\s*\\Delta\s*U|primera\s+ley", re.I)),
    ("F7C2", re.compile(r"\\Delta\s*U(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*(?:Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*)?n\s*C_V\s*(?:\\,)?(?:\\Delta\s*T|\(T)|n\s*C_V\s*(?:\\,)?\(T_[A-Za-z0-9]\s*[-−]\s*T_[A-Za-z0-9]\)", re.I)),
    ("F7C3", re.compile(r"C_[pP]\s*=\s*C_V\s*\+\s*R|C_[pP]\s*[-−]\s*C_V\s*=\s*R", re.I)),
    # Fila 8: C_V mono/dia y gamma
    ("F8C1", re.compile(r"C_\{?V(?:\\text\{mono\}|mono)?\}?\s*=\s*(?:\\frac\{3\}\{2\}|\\tfrac\{3\}\{2\}|3/2)\s*R|monoat[óo]mic", re.I)),
    ("F8C2", re.compile(r"C_\{?V(?:\\text\{dia\}|dia)?\}?\s*=\s*(?:\\frac\{5\}\{2\}|\\tfrac\{5\}\{2\}|5/2)\s*R|diat[óo]mic", re.I)),
    ("F8C3", re.compile(r"\\gamma\s*=\s*\\frac\{C_[pP]\}\{C_V\}|\\gamma\s*=\s*(?:\\frac\{5\}\{3\}|\\frac\{7\}\{5\}|5/3|7/5|1\{?,\}?4|1\{?,\}?67)", re.I)),
    # Fila 9: Procesos adiabáticos
    ("F9C1", re.compile(r"\\frac\{1\}\{\\gamma\s*[-−]\s*1\}\s*\(\s*p_?1\s*V_?1|p_1\s*V_1\s*[-−]\s*p_2\s*V_2", re.I)),
    ("F9C2", re.compile(r"p\s*V\^\\gamma|p_[A-Za-z0-9]\s*V_[A-Za-z0-9]\^\{?(?:\\gamma(?!\s*[-−]\s*1)|1\{?,\}?4)|\\left\(\\frac\{V_[A-Za-z0-9]\}\{V_[A-Za-z0-9]\}\\right\)\^\{?(?:\\gamma(?!\s*[-−]\s*1)|1\{?,\}?4)|\\left\(\\frac\{p_[A-Za-z0-9]\}\{p_[A-Za-z0-9]\}\\right\)\^\{?1/\\gamma", re.I)),
    ("F9C3", re.compile(r"T\s*V\^\{\\gamma\s*[-−]\s*1\}|T_[A-Za-z0-9]\s*V_[A-Za-z0-9]\^\{\\gamma|\\left\(\\frac\{T_[A-Za-z0-9]\}\{T_[A-Za-z0-9]\}\\right\)\^\{?(?:1/\(\\gamma|2\{?,\}?5|1\{?,\}?5)", re.I)),
    # Fila 10: Máquinas, refrigeradores, Kelvin
    ("F10C1", re.compile(r"e\s*=\s*\\frac\{W|e\s*=\s*1\s*[-−]\s*(?:\\left)?\||e\s*=\s*1\s*[-−]\s*\\frac\{\|Q_[cC]\||e\s*=\s*W/Q_H|e\s*=\s*\\frac\{[\d{,}]+", re.I)),
    ("F10C2", re.compile(r"K\s*=\s*\\frac\{\|Q_[cC]\|\}\{\|W|K\s*=\s*\|Q_[cC]\|/\|W\||K\s*=\s*\\frac\{\|Q_[cC]\|\}\{\|Q_H\|\s*[-−]", re.I)),
    ("F10C3", re.compile(r"273\{?,\}?15|T_K\s*=\s*T|kelvin|\+\s*273", re.I)),
    # Fila 11: Calor diferencial / Otto
    ("F11C1", re.compile(r"dQ\s*=\s*n\s*C_V\s*dT|Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*(?:\\Delta\s*U(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*)?n\s*C_V\s*(?:\\,)?(?:\\Delta\s*T|\(T)", re.I)),
    ("F11C2", re.compile(r"dQ\s*=\s*n\s*C_[pP]\s*dT|Q(?:_\{?[A-Za-z0-9]+\}?)?\s*=\s*n\s*C_[pP]\s*(?:\\,)?(?:\\Delta\s*T|\(T)", re.I)),
    ("F11C3", re.compile(r"e_\{?(?:\\text\{)?otto\}?\}?|1\s*[-−]\s*\\frac\{1\}\{r\^\{\\gamma|1\s*[-−]\s*1/r\^", re.I)),
    # Fila 12: Compresión, Fahrenheit, R (SI)
    ("F12C1", re.compile(r"r\s*=\s*\\frac\{V_\{?(?:\\text\{)?m[áa]x\}?\}?\}\{V_\{?(?:\\text\{)?m[íi]n|raz[óo]n\s+de\s+compresi[óo]n", re.I)),
    ("F12C2", re.compile(r"\\frac\{9\}\{5\}\s*T|T_\{?\^?\\circ\s*\\text\{F\}|Fahrenheit|\^\\circ\s*F", re.I)),
    ("F12C3", re.compile(r"8\{?,\}?314", re.I)),
    # Fila 13: k_B, N_A, sigma
    ("F13C1", re.compile(r"1\{?,\}?38\s*\\?times\s*10\^\{?-23\}?|1[.,]38×10|k_B", re.I)),
    ("F13C2", re.compile(r"6\{?,\}?022\s*\\?times\s*10\^\{?23\}?|6[.,]022×10|N_A", re.I)),
    ("F13C3", re.compile(r"5\{?,\}?67\s*\\?times\s*10\^\{?-8\}?|5[.,]67×10|\\sigma\s*=", re.I)),
    # Fila 14: atm->Pa, L->cm3, R (L*atm)
    ("F14C1", re.compile(r"101\s*,?325|1\{?,\}?013\s*\\?times\s*10\^5|101325", re.I)),
    ("F14C2", re.compile(r"1000\s*(?:\\text\{)?cm\^3|10\^\{?-3\}\s*(?:\\text\{)?m\^3|1\s*(?:\\ )?\s*\\text\{L\}\s*=\s*1000", re.I)),
    ("F14C3", re.compile(r"0\{?,\}?08206", re.I)),
)

# Notas pedagógicas breves cuando una celda oficial da origen a un resultado derivado en el problema
_NOTAS_DERIVACION: Tuple[Tuple[str, re.Pattern[str], str], ...] = (
    ("F1C1", re.compile(r"\\Delta\s*A\s*=\s*2\\alpha|\(1\s*\+\s*\\alpha.*?\)\^2"), "Desde ΔL = α L₀ ΔT se obtiene ΔA = 2α A₀ ΔT"),
    ("F1C1", re.compile(r"\\beta(?:_\{?[A-Za-z]+\}?)?\s*=\s*3\\alpha|\(1\s*\+\s*\\alpha.*?\)\^3"), "Desde ΔL = α L₀ ΔT se obtiene β = 3α"),
    ("F2C2", re.compile(r"2\\pi\s*k\s*L.*\\ln|4\\pi\s*k|R_\{?(?:th|cil|esf)|\\int"), "Se integra o despeja desde H = −kA dT/dx"),
    ("F2C1", re.compile(r"R_\{?(?:th|t|eq|tot)|Q\s*=\s*H\s*(?:\\,)?\s*\\Delta\s*t"), "Se despeja directamente de H = dQ/dt = kAΔT/L"),
    ("F6C1", re.compile(r"\\frac\{p_[A-Za-z0-9]\s*\+\s*p_[A-Za-z0-9]\}\{2\}"), "Área del trapecio evaluada con W = ∫ p dV"),
    ("F7C3", re.compile(r"C_[pP]\s*=\s*\\frac\{\\gamma\s*R\}|C_V\s*=\s*\\frac\{R\}\{\\gamma"), "Combinada con γ = C_P/C_V da C_V y C_P en términos de γ"),
    ("F8C3", re.compile(r"C_[pP]\s*=\s*\\frac\{\\gamma\s*R\}|C_V\s*=\s*\\frac\{R\}\{\\gamma"), "Combinada con C_P = C_V + R despeja C_V y C_P"),
    ("F10C1", re.compile(r"e_\{?(?:\\text\{)?Carnot\}?\}?"), "Base oficial; Carnot sustituye |Q_c/Q_H| = T_C/T_H"),
)


def _texto_de_paso(paso: Dict[str, Any]) -> str:
    partes: List[str] = [str(paso.get("titulo") or ""), str(paso.get("porque") or "")]
    for tr in paso.get("tramos") or []:
        if tr.get("v"):
            partes.append(str(tr["v"]))
        for acc in tr.get("acc") or []:
            if not isinstance(acc, dict):
                continue
            for k in ("tex", "txt", "nota"):
                if acc.get(k):
                    partes.append(str(acc[k]))
            if acc.get("items"):
                partes.extend(str(x) for x in acc["items"])
    for ln in paso.get("lento") or []:
        if isinstance(ln, dict):
            partes.append(str(ln.get("tex") or ""))
            partes.append(str(ln.get("nota") or ""))
    for og in paso.get("origen") or []:
        if isinstance(og, dict):
            partes.append(str(og.get("que") or ""))
            partes.append(str(og.get("de") or ""))
    return "\n".join(partes)


_RE_TEXTO = re.compile(r"\\(?:text|mathrm)\{([^{}]*)\}")


def _normaliza_tex(blob: str) -> str:
    r"""Quita envoltorios que no cambian la fórmula: \text{...} y espacios finos."""
    return _RE_TEXTO.sub(r"\1", blob).replace("\\,", "").replace("\\!", "")


def _celdas_en_texto(blob: str) -> List[str]:
    halladas: List[str] = []
    blob = blob + "\n" + _normaliza_tex(blob)
    for cid, pat in _REGLAS_CELDAS:
        if pat.search(blob):
            halladas.append(cid)
    return halladas


def catalogo_formulario() -> Dict[str, Any]:
    """Catálogo completo de las 42 celdas oficiales con sus coordenadas exactas en la hoja."""
    celdas = []
    for cid, tex, es_c in _TODAS_LAS_CELDAS:
        _, _, fila, col = _POR_ID[cid]
        celdas.append({
            "id": cid,
            "fila": fila,
            "col": col,
            "etiqueta": f"Fila {fila} · Col {col}",
            "nombre": NOMBRES_CELDAS[cid],
            "tex": tex,
            "es_constante": es_c,
            "caja": list(CAJAS_CELDAS[cid]),
        })
    return {
        "titulo": "Formulario oficial · III Parcial FI2103 II-2026",
        "hoja_url": "/api/formulario/hoja.png",
        "aspecto": list(HOJA_ASPECTO),
        "total": len(celdas),
        "celdas": celdas,
    }


def mapear_formulario_leccion(lec: Dict[str, Any]) -> Dict[str, Any]:
    """Mapea cada paso de una lección a las celdas exactas (`F1C1`..`F14C3`) de la hoja II-2026.

    Asigna un color distinto y un número secuencial a cada celda usada, sincronizado entre
    la pizarra (etiquetas clicables por paso) y la imagen del formulario (círculos de color).
    """
    pasos = lec.get("pasos") or []
    por_paso: List[List[str]] = []
    pasos_por_celda: Dict[str, List[int]] = {}
    blobs_paso: List[str] = []

    for idx, p in enumerate(pasos):
        blob = _texto_de_paso(p)
        blobs_paso.append(blob)
        cids = _celdas_en_texto(blob)
        por_paso.append(cids)
        for cid in cids:
            pasos_por_celda.setdefault(cid, []).append(idx)

    # Respaldo con las fórmulas declaradas a nivel de lección (y enunciado si faltaran)
    formulas_blob = "\n".join(str(x) for x in (lec.get("formulas") or []))
    for cid in _celdas_en_texto(formulas_blob):
        if cid not in pasos_por_celda:
            pasos_por_celda[cid] = [0] if pasos else []
            if por_paso and cid not in por_paso[0]:
                por_paso[0].append(cid)

    # Si ninguna celda se activó aún (ej. lección conceptual corta), revisar enunciado + respuestas
    if not pasos_por_celda:
        extra = "\n".join([
            str(lec.get("enunciado") or ""),
            " ".join(str(x) for x in (lec.get("claves") or [])),
            "\n".join(str(r.get("tex") or "") for r in (lec.get("respuestas") or []) if isinstance(r, dict)),
        ])
        for cid in _celdas_en_texto(extra):
            pasos_por_celda[cid] = [0] if pasos else []
            if por_paso and cid not in por_paso[0]:
                por_paso[0].append(cid)

    blob_total = "\n".join(blobs_paso + [formulas_blob, str(lec.get("enunciado") or "")])

    # Orden: ecuaciones base primero (en orden de primera aparición), constantes al final
    ordenadas = sorted(
        pasos_por_celda.keys(),
        key=lambda cid: (
            1 if _POR_ID[cid][1] else 0,
            min(pasos_por_celda[cid]) if pasos_por_celda[cid] else 999,
            _POR_ID[cid][2],
            _POR_ID[cid][3],
        ),
    )

    celdas_out: List[Dict[str, Any]] = []
    for pos, cid in enumerate(ordenadas):
        tex, es_c, fila, col = _POR_ID[cid]
        nota_der = ""
        for target_cid, pat_der, texto_der in _NOTAS_DERIVACION:
            if target_cid == cid and pat_der.search(blob_total):
                nota_der = texto_der
                break
        celdas_out.append({
            "id": cid,
            "num": pos + 1,
            "color": COLORES_FORMULARIO[pos % len(COLORES_FORMULARIO)],
            "fila": fila,
            "col": col,
            "etiqueta": f"Fila {fila} · Col {col}",
            "nombre": NOMBRES_CELDAS[cid],
            "tex": tex,
            "es_constante": es_c,
            "caja": list(CAJAS_CELDAS[cid]),
            "pasos": sorted(set(pasos_por_celda[cid])),
            "derivado": nota_der,
        })

    # Reordenar `por_paso[i]` en el mismo orden numerado de `celdas_out`
    orden_num = {c["id"]: c["num"] for c in celdas_out}
    por_paso_ordenado = [
        sorted(set(cids), key=lambda cid: orden_num.get(cid, 999))
        for cids in por_paso
    ]

    fuera_de_hoja: List[Dict[str, str]] = []
    if re.search(r"\\Delta\s*S|dS\s*=|\\frac\{dQ\}\{T\}|entrop[íi]a", blob_total, re.I):
        fuera_de_hoja.append({
            "tema": "Entropía (ΔS)",
            "nota": "Las expresiones de entropía no están impresas en el formulario oficial II-2026 (corresponden a exámenes de semestres anteriores).",
        })
    if re.search(r"e_\{?(?:\\text\{)?Carnot|K_\{?(?:\\text\{)?Carnot", blob_total, re.I):
        fuera_de_hoja.append({
            "tema": "Relación de Carnot (T_C / T_H)",
            "nota": "No figura impresa como ecuación directa; parte de F10C1 o F10C2 sustituyendo |Q_c/Q_H| = T_C/T_H en ciclo reversible.",
        })

    # Renglón por renglón: qué celdas usa cada línea matemática (para citar la casilla en la pizarra)
    # Cada renglón escrito: de qué casillas sale. Los renglones que siguen a una ecuación del
    # formulario dentro del mismo paso (igualar, despejar, sustituir, resultado) heredan su color.
    por_accion: Dict[str, Dict[str, Any]] = {}
    for p in pasos:
        ultima: List[str] = []
        for tr in p.get("tramos") or []:
            for acc in tr.get("acc") or []:
                if not isinstance(acc, dict) or acc.get("a") != "mate" or not acc.get("id"):
                    continue
                cids = [c for c in _celdas_en_texto(str(acc.get("tex") or "")) if c in orden_num]
                if not cids:
                    if ultima:
                        por_accion[str(acc["id"])] = {"celdas": list(ultima), "formula": False, "hereda": True}
                    continue
                cids.sort(key=lambda c: (1 if _POR_ID[c][1] else 0, orden_num[c]))
                por_accion[str(acc["id"])] = {"celdas": cids, "formula": acc.get("rol") == "formula"}
                base = [c for c in cids if not _POR_ID[c][1]]
                if base:
                    ultima = base

    return {
        "titulo": "Formulario oficial · III Parcial FI2103 II-2026",
        "hoja_url": "/api/formulario/hoja.png",
        "aspecto": list(HOJA_ASPECTO),
        "celdas": celdas_out,
        "por_paso": por_paso_ordenado,
        "por_accion": por_accion,
        "fuera_de_hoja": fuera_de_hoja,
    }
