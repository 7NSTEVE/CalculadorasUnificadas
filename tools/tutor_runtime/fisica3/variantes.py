"""Generador determinista de variantes paramétricas para los arquetipos críticos del III Parcial.

Cambia los valores numéricos y condiciones conservando la estructura física, recalcula TODOS
los pasos en Python (cero improvisación de IA, cero cuota), cumple dsl.revisar() y pasa
la auditoría física de 10 niveles.
"""

from __future__ import annotations

import math
import random
from typing import Callable, Dict, Optional

from . import registro
from .dsl import Leccion

SIGMA = 5.670374419e-8
R_GAS = 8.314


def fmt(x: float, dec: int = 2) -> str:
    """Formatea un número en TeX con coma decimal ({,}) y sin -0."""
    v = 0.0 if abs(x) < 10.0 ** (-(dec + 3)) else x
    if dec <= 0:
        return f"{round(v):d}"
    return f"{v:.{dec}f}".replace(".", "{,}")


def fmt_txt(x: float, dec: int = 2) -> str:
    return fmt(x, dec).replace("{,}", ",")


def _v_conduccion_area(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo Q049: barra piramidal con área transversal variable A(y) = A0 - b y."""
    mat, k = rng.choice([("aluminio", 235.0), ("cobre", 385.0), ("latón", 109.0), ("acero", 50.2)])
    T1 = float(rng.choice([160, 180, 200, 220, 250]))
    T2 = float(rng.choice([15, 20, 25, 30]))
    L = rng.choice([0.36, 0.40, 0.48, 0.50, 0.60])
    A0 = rng.choice([0.160, 0.200, 0.240, 0.250])
    A_top = round(A0 * rng.choice([0.55, 0.60, 0.65]), 4)
    b = round((A0 - A_top) / L, 4)
    A_top = A0 - b * L

    s_inf = math.sqrt(A0)
    s_sup = math.sqrt(A_top)
    I_tot = (1.0 / b) * math.log(A0 / A_top)
    H = k * (T1 - T2) / I_tot
    y_mid = L / 2.0
    A_mid = A0 - b * y_mid
    I_mid = (1.0 / b) * math.log(A0 / A_mid)
    T_mid = T1 - (H / k) * I_mid

    pts = []
    for i in range(41):
        yi = L * i / 40.0
        Ai = A0 - b * yi
        Ii = (1.0 / b) * math.log(A0 / Ai)
        Ti = T1 - (H / k) * Ii
        pts.append([round(yi, 4), round(Ti, 2)])
    c_forma = round(b * L / A0, 4)

    lec = Leccion(
        f"VAR-AREA-{rng.randint(100,999)}",
        f"Variante · Barra de {mat} con área variable A(y)",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Dos reservorios térmicos, uno a ${fmt(T1,1)}\\,^\\circ\\text{{C}}$ y otro a ${fmt(T2,1)}\\,^\\circ\\text{{C}}$, "
            f"se conectan mediante una barra de {mat} ($k = {fmt(k,1)}\\text{{ W/(m\\cdot K)}}$) de altura $L = {fmt(L,2)}\\text{{ m}}$ "
            f"y área transversal cuadrada $A(y) = {fmt(A0,3)} - {fmt(b,4)}y$ (en $\\text{{m}}^2$). "
            f"Calcule: A) los lados de la base y el tope, B) el flujo de calor $H$, y C) la temperatura a la mitad ($y = {fmt(y_mid,2)}\\text{{ m}}$)."
        ),
        visual={
            "tipo": "conduccion",
            "geo": "pared",
            "L": L,
            "T1": T1,
            "T2": T2,
            "eje": "y",
            "uT": "°C",
            "forma": f"Math.sqrt(1 - {c_forma} * u)",
            "pts": pts,
            "xm": y_mid,
            "etA": f"{fmt_txt(A0,3)}−{fmt_txt(b,4)}y",
        },
    ).usa(r"H = -k A(y)\frac{dT}{dy}", r"\int \frac{dy}{A_0 - b y} = -\frac{1}{b}\ln(A_0 - b y)")

    (
        lec.paso("A) Lados de la base y el tope")
        .por_que("El área es cuadrada ($A = s^2$), así que evaluamos $A(0)$ y $A(L)$ y sacamos raíz cuadrada.")
        .di(
            "En la base, y vale cero, así que el área es A subcero y su lado es la raíz cuadrada.",
            {"a": "mate", "tex": rf"A(0) = {fmt(A0,3)}\,\text{{m}}^2 \implies s_{{\text{{base}}}} = \sqrt{{{fmt(A0,3)}}} = {fmt(s_inf,3)}\,\text{{m}}", "color": "azul"},
        )
        .di(
            f"En el tope, y vale {fmt_txt(L,2)} metros, sustituimos en A de y y sacamos la raíz.",
            {"a": "mate", "tex": rf"A({fmt(L,2)}) = {fmt(A0,3)} - {fmt(b,4)}({fmt(L,2)}) = {fmt(A_top,4)}\,\text{{m}}^2 \implies s_{{\text{{tope}}}} = {fmt(s_sup,3)}\,\text{{m}}", "color": "verde"},
        )
        .sale_de((f"$A_0 = {fmt(A0,3)}$", "del enunciado en $y=0$"), (f"$b = {fmt(b,4)}$", "coeficiente de $y$ en $A(y)$"))
    )
    (
        lec.paso("B) Separar variables e integrar para hallar H")
        .por_que("Como el área cambia con $y$, no aplica $H = kA\\Delta T/L$ constante; hay que separar variables e integrar.")
        .di(
            "En estado estacionario el flujo H es constante a lo largo de la barra. Separamos y y la temperatura.",
            {"a": "mate", "tex": r"H\int_0^L \frac{dy}{A_0 - b y} = -k\int_{T_1}^{T_2} dT = k(T_1 - T_2)", "color": "azul"},
        )
        .di(
            "Usamos cambio de variable u igual a A subcero menos b por y para evaluar la integral geométrica.",
            {"a": "mate", "tex": rf"I = \frac{{1}}{{{fmt(b,4)}}}\ln\!\left(\frac{{{fmt(A0,3)}}}{{{fmt(A_top,4)}}}\right) = {fmt(I_tot,3)}\,\text{{m}}^{{-1}}"},
            {"a": "mate", "tex": rf"H = \frac{{({fmt(k,1)})({fmt(T1,1)} - {fmt(T2,1)})}}{{{fmt(I_tot,3)}}} = {fmt(H/1000,2)}\,\text{{kW}}\;({fmt(H,0)}\,\text{{W}})", "color": "verde", "rol": "resultado"},
        )
        .despacio(
            (r"u = A_0 - b y \implies du = -b\,dy \implies dy = -\frac{du}{b}", "Cambio de variable paso a paso sin saltos"),
            (r"\int_0^L \frac{dy}{A_0 - b y} = -\frac{1}{b}\left[\ln(A_0 - b y)\right]_0^L = \frac{1}{b}\ln\!\left(\frac{A_0}{A(L)}\right)", "El signo menos invierte los límites del logaritmo"),
        )
    )
    (
        lec.paso(f"C) Temperatura a la mitad (y = {fmt_txt(y_mid,2)} m)")
        .por_que("Integramos la misma ecuación de Fourier solo desde la base hasta la mitad de la barra.")
        .di(
            "Calculamos primero el área y la integral geométrica hasta la mitad.",
            {"a": "mate", "tex": rf"A({fmt(y_mid,2)}) = {fmt(A0,3)} - {fmt(b,4)}({fmt(y_mid,2)}) = {fmt(A_mid,4)}\,\text{{m}}^2"},
            {"a": "mate", "tex": rf"I_{{1/2}} = \frac{{1}}{{{fmt(b,4)}}}\ln\!\left(\frac{{{fmt(A0,3)}}}{{{fmt(A_mid,4)}}}\right) = {fmt(I_mid,3)}\,\text{{m}}^{{-1}}"},
        )
        .di(
            "Despejamos la temperatura en la mitad; queda más cerca de la cara ancha.",
            {"a": "mate", "tex": rf"T({fmt(y_mid,2)}\,\text{{m}}) = {fmt(T1,1)} - \frac{{{fmt(H,0)}}}{{{fmt(k,1)}}}({fmt(I_mid,3)}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"s_{{\text{{base}}}} = {fmt(s_inf,3)}\,\text{{m}},\; s_{{\text{{tope}}}} = {fmt(s_sup,3)}\,\text{{m}}", valor=round(s_inf, 3))
    lec.responde("B)", rf"H = {fmt(H/1000,2)}\,\text{{kW}}", valor=round(H / 1000, 2))
    lec.responde("C)", rf"T({fmt(y_mid,2)}\,\text{{m}}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", valor=round(T_mid, 1))
    lec.te_toca(
        f"Si en lugar de {mat} ($k={fmt(k,1)}$) la conductividad se duplicara, ¿cuánto valdría el flujo $H$ en kW?",
        round(2 * H / 1000, 2),
        "kW",
        pista="H es directamente proporcional a k cuando la geometría y las temperaturas no cambian.",
        solucion=f"Al duplicar $k$, el flujo se duplica: $2 \\times {fmt(H/1000,2)} = {fmt(2*H/1000,2)}\\text{{ kW}}$.",
    )
    return lec


def _v_conduccion_k_radial(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo E8 / Q057: tubo cilíndrico con conductividad k(r) = k0 / r."""
    a = rng.choice([0.04, 0.05, 0.06, 0.08])
    b = round(a * rng.choice([2.0, 2.5, 3.0]), 3)
    L = rng.choice([1.0, 1.5, 2.0, 2.5])
    k0 = rng.choice([0.40, 0.60, 0.80, 1.20])
    Tint = float(rng.choice([90, 100, 120, 140]))
    Text = float(rng.choice([15, 20, 25, 30]))

    H = 2.0 * math.pi * k0 * L * (Tint - Text) / (b - a)
    r_mid = 0.5 * (a + b)
    T_mid = Tint - (Tint - Text) * (r_mid - a) / (b - a)

    lec = Leccion(
        f"VAR-KRAD-{rng.randint(100,999)}",
        "Variante · Tubo cilíndrico con conductividad k(r) = k₀/r",
        tema="conduccion",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un tubo de largo $L = {fmt(L,1)}\\text{{ m}}$, radio interno $a = {fmt(a,3)}\\text{{ m}}$ y radio externo $b = {fmt(b,3)}\\text{{ m}}$ "
            f"tiene conductividad variable $k(r) = k_0/r$ con $k_0 = {fmt(k0,2)}\\text{{ W/K}}$. "
            f"Si $T_{{\\text{{int}}}} = {fmt(Tint,1)}\\,^\\circ\\text{{C}}$ y $T_{{\\text{{ext}}}} = {fmt(Text,1)}\\,^\\circ\\text{{C}}$, "
            f"calcule el flujo radial de calor $H$ y la temperatura en $r = {fmt(r_mid,3)}\\text{{ m}}$."
        ),
        visual={"tipo": "conduccion", "caso": "cilindro", "r1": a, "r2": b, "T1": Tint, "T2": Text, "k0": k0, "L": L},
    ).usa(r"H = -k(r) A(r)\frac{dT}{dr}", r"A(r) = 2\pi r L")

    (
        lec.paso("Sustituir A(r) y k(r) en la ley de Fourier")
        .por_que("El radio $r$ se cancela exactamente al multiplicar $k_0/r$ por $2\\pi r L$.")
        .di(
            "El área lateral de un cilindro de radio r y largo L es dos pi por r por L. Al multiplicar, r se cancela.",
            {"a": "mate", "tex": r"H = -\left(\frac{k_0}{r}\right)(2\pi r L)\frac{dT}{dr} = -2\pi k_0 L \frac{dT}{dr}", "color": "azul"},
        )
        .despacio((r"\frac{k_0}{r}\cdot 2\pi r L = 2\pi k_0 L", "Se simplifica r arriba y abajo sin ningún salto"))
    )
    (
        lec.paso("Integrar entre a y b para hallar H")
        .por_que("Como $r$ se canceló, la integral de $dr$ es simplemente $b - a$.")
        .di(
            "Separamos variables e integramos desde el borde interno hasta el externo.",
            {"a": "mate", "tex": r"H\int_a^b dr = -2\pi k_0 L \int_{T_{\text{int}}}^{T_{\text{ext}}} dT \implies H(b-a) = 2\pi k_0 L(T_{\text{int}} - T_{\text{ext}})"},
            {"a": "mate", "tex": rf"H = \frac{{2\pi({fmt(k0,2)})({fmt(L,1)})({fmt(Tint,1)} - {fmt(Text,1)})}}{{{fmt(b,3)} - {fmt(a,3)}}} = {fmt(H,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso(f"Temperatura en el punto medio (r = {fmt_txt(r_mid,3)} m)")
        .por_que("Como $dT/dr$ es constante, la temperatura baja linealmente con el radio.")
        .di(
            "Integramos solo desde a hasta el radio medio y despejamos la temperatura.",
            {"a": "mate", "tex": rf"T({fmt(r_mid,3)}\,\text{{m}}) = {fmt(Tint,1)} - \frac{{{fmt(H,1)}({fmt(r_mid-a,3)})}}{{2\pi({fmt(k0,2)})({fmt(L,1)})}} = {fmt(T_mid,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,1)}\,\text{{W}}", valor=round(H, 1))
    lec.responde("B)", rf"T({fmt(r_mid,3)}\,\text{{m}}) = {fmt(T_mid,1)}\,^\circ\text{{C}}", valor=round(T_mid, 1))
    return lec


def _v_ciclo_pv(rng: random.Random) -> Leccion:
    """Arquetipo crítico tipo E16 / Q111: ciclo isotérmica + isobárica + isocórica (100% energía conservada)."""
    n = rng.choice([1.5, 2.0, 2.5, 3.0])
    es_diat = rng.choice([True, False])
    tipo_gas = "diatómico" if es_diat else "monoatómico"
    cv_coef = 2.5 if es_diat else 1.5
    cp_coef = cv_coef + 1.0
    cv_tex = r"\frac{5}{2}R" if es_diat else r"\frac{3}{2}R"
    cp_tex = r"\frac{7}{2}R" if es_diat else r"\frac{5}{2}R"

    TA = float(rng.choice([400, 480, 540, 600]))
    VA = rng.choice([0.020, 0.030, 0.040])
    factor_V = rng.choice([2.0, 2.5, 3.0])
    VB = VA * factor_V
    TB = TA
    pA = n * R_GAS * TA / VA
    pB = n * R_GAS * TB / VB
    pC = pB
    VC = VA
    TC = TB * (VC / VB)

    W_AB = n * R_GAS * TA * math.log(VB / VA)
    Q_AB = W_AB
    W_BC = pB * (VC - VB)
    dU_BC = n * cv_coef * R_GAS * (TC - TB)
    Q_BC = n * cp_coef * R_GAS * (TC - TB)
    dU_CA = n * cv_coef * R_GAS * (TA - TC)
    Q_CA = dU_CA

    W_neto = W_AB + W_BC
    Q_H = Q_AB + Q_CA
    efic = W_neto / Q_H

    lec = Leccion(
        f"VAR-PV-{rng.randint(100,999)}",
        f"Variante · Ciclo P–V (isotérmica · isobárica · isocórica, gas {tipo_gas})",
        tema="ciclos",
        tipo="desarrollo",
        nivel=4,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un gas ideal {tipo_gas} ($n = {fmt(n,1)}\\text{{ mol}}$) parte del estado A con $T_A = {fmt(TA,0)}\\text{{ K}}$ y "
            f"$V_A = {fmt(VA,3)}\\text{{ m}}^3$. Realiza: A→B expansión isotérmica hasta $V_B = {fmt(VB,3)}\\text{{ m}}^3$, "
            f"B→C compresión isobárica hasta $V_C = V_A$, y C→A calentamiento isocórico hasta A. "
            f"Calcule el trabajo de cada proceso, el trabajo neto $W_{{\\text{{neto}}}}$ y la eficiencia $e$."
        ),
        visual={
            "tipo": "pv",
            "estados": [
                {"n": "A", "p": round(pA), "V": VA, "T": TA},
                {"n": "B", "p": round(pB), "V": VB, "T": TB},
                {"n": "C", "p": round(pC), "V": VC, "T": TC},
            ],
            "tramos": [
                {"de": 0, "a": 1, "tipo": "isotermico", "Q": round(Q_AB), "W": round(W_AB), "dU": 0},
                {"de": 1, "a": 2, "tipo": "isobarico", "Q": round(Q_BC), "W": round(W_BC), "dU": round(dU_BC)},
                {"de": 2, "a": 0, "tipo": "isocorico", "Q": round(Q_CA), "W": 0, "dU": round(dU_CA)},
            ],
        },
    ).usa(r"pV = nRT", r"W_{\text{iso}} = nRT\ln\!\left(\frac{V_f}{V_i}\right)", r"W_{\text{isob}} = p\Delta V", r"e = \frac{W_{\text{neto}}}{Q_H}")

    (
        lec.paso("Estados A, B y C con la ley del gas ideal")
        .por_que("Primero fijamos presión, volumen y temperatura en los tres vértices en SI.")
        .di(
            "Despejamos la presión en A y en B con la temperatura en Kelvin.",
            {"a": "mate", "tex": rf"p_A = \frac{{nRT_A}}{{V_A}} = {fmt(pA/1000,1)}\,\text{{kPa}},\quad p_B = p_C = {fmt(pB/1000,1)}\,\text{{kPa}}", "color": "azul"},
        )
        .di(
            "Como B a C es a presión constante, la temperatura baja en la misma proporción que el volumen.",
            {"a": "mate", "tex": rf"T_C = T_B\frac{{V_C}}{{V_B}} = {fmt(TB,0)}\left(\frac{{{fmt(VC,3)}}}{{{fmt(VB,3)}}}\right) = {fmt(TC,1)}\,\text{{K}}"},
        )
    )
    (
        lec.paso("Proceso A → B (expansión isotérmica)")
        .por_que("La presión cambia todo el camino: NO se usa $p\\Delta V$, sino la integral $nRT\\ln(V_B/V_A)$.")
        .di(
            "A temperatura constante la energía interna no cambia, así que el calor absorbido es igual al trabajo.",
            {"a": "mate", "tex": rf"W_{{AB}} = Q_{{AB}} = ({fmt(n,1)})(8{{,}}314)({fmt(TA,0)})\ln\!\left(\frac{{{fmt(VB,3)}}}{{{fmt(VA,3)}}}\right) = {fmt(W_AB,0)}\,\text{{J}}", "color": "verde"},
        )
        .despacio((r"W_{AB} = \int_{V_A}^{V_B} \frac{nRT_A}{V}\,dV = nRT_A \ln\!\left(\frac{V_B}{V_A}\right)", "Por qué aparece el logaritmo natural"))
    )
    (
        lec.paso("Procesos B → C (isobárica) y C → A (isocórica)")
        .por_que("En B→C la presión es constante y el volumen baja (trabajo negativo); en C→A el volumen no cambia ($W = 0$).")
        .di(
            "En la compresión isobárica el trabajo es presión por cambio de volumen y sale negativo.",
            {"a": "mate", "tex": rf"W_{{BC}} = p_B(V_C - V_B) = ({fmt(pB,0)})({fmt(VC-VB,3)}) = {fmt(W_BC,0)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"Q_{{BC}} = n\left({cp_tex}\right)(T_C - T_B) = {fmt(Q_BC,0)}\,\text{{J}}"},
        )
        .di(
            "Al cerrar el ciclo a volumen constante no hay trabajo y entra calor.",
            {"a": "mate", "tex": rf"W_{{CA}} = 0\,\text{{J}},\quad Q_{{CA}} = n\left({cv_tex}\right)(T_A - T_C) = {fmt(Q_CA,0)}\,\text{{J}}"},
        )
    )
    (
        lec.paso("Trabajo neto y eficiencia térmica")
        .por_que("Solo los calores positivos forman $Q_H$; además $\\Delta U_{{\\text{{ciclo}}}} = 0$ garantiza $W_{{\\text{{neto}}}} = Q_{{\\text{{neto}}}}$.")
        .di(
            "Sumamos los trabajos y dividimos entre el calor total absorbido.",
            {"a": "mate", "tex": rf"W_{{\text{{neto}}}} = {fmt(W_AB,0)} + ({fmt(W_BC,0)}) + 0 = {fmt(W_neto,0)}\,\text{{J}}", "color": "azul"},
            {"a": "mate", "tex": rf"Q_H = Q_{{AB}} + Q_{{CA}} = {fmt(Q_AB,0)} + {fmt(Q_CA,0)} = {fmt(Q_H,0)}\,\text{{J}}"},
            {"a": "mate", "tex": rf"e = \frac{{{fmt(W_neto,0)}}}{{{fmt(Q_H,0)}}} = {fmt(efic*100,1)}\,\%", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("W", rf"W_{{\text{{neto}}}} = {fmt(W_neto,0)}\,\text{{J}}", valor=round(W_neto))
    lec.responde("e", rf"e = {fmt(efic*100,1)}\,\%", valor=round(efic * 100, 1))
    return lec


def _v_calorimetria_fase(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E5: metal caliente en mezcla de agua y hielo con chequeo de fusión."""
    m_agua = rng.choice([0.20, 0.25, 0.30, 0.40])
    m_hielo = rng.choice([0.04, 0.05, 0.06])
    m_metal = rng.choice([0.25, 0.35, 0.45, 0.50])
    mat, c_m = rng.choice([("cobre", 387.0), ("aluminio", 900.0), ("hierro", 448.0)])
    T_m = float(rng.choice([180, 220, 260, 300]))
    c_w = 4186.0
    Lf = 3.34e5

    Q_fundir = m_hielo * Lf
    Q_metal_max = m_metal * c_m * (T_m - 0.0)
    if Q_metal_max > Q_fundir:
        Tf = (Q_metal_max - Q_fundir) / ((m_agua + m_hielo) * c_w + m_metal * c_m)
        res_tex = rf"T_f = {fmt(Tf,1)}\,^\circ\text{{C}}"
        val_final = round(Tf, 1)
        nota_f = "Todo el hielo se derrite y la mezcla queda líquida sobre cero grados."
    else:
        m_derr = Q_metal_max / Lf
        Tf = 0.0
        res_tex = rf"T_f = 0{{,}}0\,^\circ\text{{C}},\; m_{{\text{{derretida}}}} = {fmt(m_derr*1000,1)}\,\text{{g}}"
        val_final = 0.0
        nota_f = "El metal no alcanza para derretir todo el hielo; el equilibrio queda en cero grados."

    lec = Leccion(
        f"VAR-CAL-{rng.randint(100,999)}",
        f"Variante · Bloque de {mat} en agua con hielo",
        tema="calorimetria",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un calorímetro aislado contiene ${fmt(m_agua,2)}\\text{{ kg}}$ de agua y ${fmt(m_hielo,2)}\\text{{ kg}}$ de hielo en equilibrio a $0{{,}}0\\,^\\circ\\text{{C}}$. "
            f"Se introduce un bloque de {mat} ($c = {fmt(c_m,0)}\\text{{ J/(kg\\cdot K)}}$) de masa ${fmt(m_metal,2)}\\text{{ kg}}$ a ${fmt(T_m,0)}\\,^\\circ\\text{{C}}$. "
            f"Determine la temperatura final de equilibrio."
        ),
        visual={"tipo": "calor", "caso": "curva", "T0": 0, "Tf": Tf},
    ).usa(r"\sum Q = 0", r"Q = m c \Delta T", r"Q_{\text{fusión}} = m L_f")

    (
        lec.paso("Comparar el calor máximo del metal con el calor para fundir el hielo")
        .por_que("Antes de despejar $T_f$ hay que comprobar si el hielo se derrite por completo o no.")
        .di(
            "Calculamos cuánto calor pide derretir todo el hielo y cuánto puede ceder el metal al enfriarse hasta cero.",
            {"a": "mate", "tex": rf"Q_{{\text{{fundir}}}} = m_h L_f = ({fmt(m_hielo,2)})(3{{,}}34\times 10^5) = {fmt(Q_fundir,0)}\,\text{{J}}", "color": "azul"},
            {"a": "mate", "tex": rf"Q_{{\text{{metal, máx}}}} = ({fmt(m_metal,2)})({fmt(c_m,0)})({fmt(T_m,0)} - 0) = {fmt(Q_metal_max,0)}\,\text{{J}}"},
        )
    )
    (
        lec.paso("Balance de energía y temperatura final")
        .por_que(nota_f)
        .di(nota_f, {"a": "mate", "tex": res_tex, "color": "verde", "rol": "resultado"})
    )
    lec.responde("", res_tex, valor=val_final)
    return lec


def _v_conduccion_serie_paralelo(rng: random.Random) -> Leccion:
    """Arquetipo muy alto tipo Q038: dos barras en serie (temperatura de unión y flujo)."""
    L1 = rng.choice([0.20, 0.25, 0.30])
    L2 = rng.choice([0.15, 0.20, 0.30])
    A = rng.choice([0.0020, 0.0040, 0.0050])
    k1, n1 = 385.0, "cobre"
    k2, n2 = rng.choice([(50.2, "acero"), (109.0, "latón"), (235.0, "aluminio")])
    T1 = float(rng.choice([100, 120, 150]))
    T2 = float(rng.choice([0, 10, 20]))

    R1 = L1 / (k1 * A)
    R2 = L2 / (k2 * A)
    H = (T1 - T2) / (R1 + R2)
    Tu = T1 - H * R1

    lec = Leccion(
        f"VAR-SER-{rng.randint(100,999)}",
        f"Variante · Barras de {n1} y {n2} en serie",
        tema="conduccion",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {n1} ($L_1 = {fmt(L1,2)}\\text{{ m}}, k_1 = {fmt(k1,1)}\\text{{ W/(m\\cdot K)}}$) se une en serie con una de {n2} "
            f"($L_2 = {fmt(L2,2)}\\text{{ m}}, k_2 = {fmt(k2,1)}\\text{{ W/(m\\cdot K)}}$), ambas de área $A = {fmt(A,4)}\\text{{ m}}^2$. "
            f"Los extremos se mantienen a ${fmt(T1,0)}\\,^\\circ\\text{{C}}$ y ${fmt(T2,0)}\\,^\\circ\\text{{C}}$. Calcule el flujo $H$ y la temperatura de la unión."
        ),
        visual={"tipo": "conduccion", "caso": "serie", "T1": T1, "T2": T2, "L1": L1, "L2": L2, "k1": k1, "k2": k2},
    ).usa(r"R_t = \frac{L}{kA}", r"H = \frac{T_1 - T_2}{R_1 + R_2}")

    (
        lec.paso("Resistencias térmicas en serie y flujo H")
        .por_que("En serie el flujo $H$ es igual en ambas barras y las resistencias térmicas se suman.")
        .di(
            "Calculamos cada resistencia térmica y dividimos la diferencia de temperatura entre su suma.",
            {"a": "mate", "tex": rf"R_1 = \frac{{{fmt(L1,2)}}}{{({fmt(k1,1)})({fmt(A,4)})}} = {fmt(R1,4)}\,\text{{K/W}},\quad R_2 = {fmt(R2,4)}\,\text{{K/W}}"},
            {"a": "mate", "tex": rf"H = \frac{{{fmt(T1,0)} - {fmt(T2,0)}}}{{{fmt(R1,4)} + {fmt(R2,4)}}} \approx {fmt(H,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    (
        lec.paso("Temperatura de la unión")
        .por_que("La caída de temperatura en la primera barra es simplemente $H\\cdot R_1$.")
        .di(
            "Restamos a la temperatura caliente la caída en el cobre.",
            {"a": "mate", "tex": rf"T_u = {fmt(T1,0)} - ({fmt(H,1)})({fmt(R1,4)}) \approx {fmt(Tu,1)}\,^\circ\text{{C}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("A)", rf"H = {fmt(H,1)}\,\text{{W}}", valor=round(H, 1))
    lec.responde("B)", rf"T_u = {fmt(Tu,1)}\,^\circ\text{{C}}", valor=round(Tu, 1))
    return lec


def _v_dilatacion_barras(rng: random.Random) -> Leccion:
    """Arquetipo tipo E1: dos barras metálicas que mantienen diferencia constante de largo."""
    dif = rng.choice([4.0, 5.0, 6.0, 8.0])
    a1, n1 = 1.7e-5, "cobre"
    a2, n2 = rng.choice([(1.1e-5, "acero"), (1.2e-5, "hierro")])
    L1 = dif * a2 / (a1 - a2)
    L2 = L1 + dif

    lec = Leccion(
        f"VAR-DIL-{rng.randint(100,999)}",
        f"Variante · Dos barras ({n1} y {n2}) con diferencia fija de {fmt_txt(dif,1)} cm",
        tema="dilatacion",
        tipo="desarrollo",
        nivel=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Una barra de {n1} ($\\alpha_1 = 1{{,}}7\\times 10^{{-5}}\\,^\\circ\\text{{C}}^{{-1}}$) y otra de {n2} "
            f"($\\alpha_2 = {fmt(a2*1e5,1)}\\times 10^{{-5}}\\,^\\circ\\text{{C}}^{{-1}}$) mantienen siempre una diferencia de largo de ${fmt(dif,1)}\\text{{ cm}}$ a cualquier temperatura. "
            f"Calcule el largo inicial de cada barra."
        ),
        visual={"tipo": "dilatacion", "caso": "barras", "u": "cm", "T0": 20, "dTmax": 250, "barras": [{"n": n1, "L": round(L1, 2), "a": a1, "color": "naranja"}, {"n": n2, "L": round(L2, 2), "a": a2, "color": "azul"}]},
    ).usa(r"\Delta L = \alpha L_0 \Delta T")

    (
        lec.paso("Igualar las dilataciones y despejar los largos")
        .por_que("Para que $L_2 - L_1$ nunca cambie, ambas barras deben crecer exactamente lo mismo.")
        .di(
            "Igualamos ambos crecimientos de largo y cancelamos el cambio de temperatura.",
            {"a": "mate", "tex": r"\alpha_1 L_1 \Delta T = \alpha_2 L_2 \Delta T \implies \alpha_1 L_1 = \alpha_2 (L_1 + d)", "color": "azul"},
            {"a": "mate", "tex": rf"L_1 = \frac{{\alpha_2 d}}{{\alpha_1 - \alpha_2}} = {fmt(L1,2)}\,\text{{cm}},\quad L_2 = {fmt(L2,2)}\,\text{{cm}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("", rf"L_1 = {fmt(L1,2)}\,\text{{cm}},\; L_2 = {fmt(L2,2)}\,\text{{cm}}", valor=round(L1, 2))
    return lec


def _v_radiacion_stefan(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E10 / Q063: radiación neta de Stefan-Boltzmann."""
    A = rng.choice([0.08, 0.12, 0.15, 0.20])
    eps = rng.choice([0.65, 0.75, 0.85, 0.90])
    T_c = float(rng.choice([127, 227, 327, 427]))
    Tamb_c = float(rng.choice([17, 20, 27]))
    T_K = T_c + 273.15
    Tamb_K = Tamb_c + 273.15
    P_neta = eps * SIGMA * A * (T_K**4 - Tamb_K**4)

    lec = Leccion(
        f"VAR-RAD-{rng.randint(100,999)}",
        "Variante · Radiación neta por ley de Stefan–Boltzmann",
        tema="radiacion",
        tipo="desarrollo",
        nivel=2,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un objeto de área $A = {fmt(A,2)}\\text{{ m}}^2$ y emisividad $\\varepsilon = {fmt(eps,2)}$ está a ${fmt(T_c,0)}\\,^\\circ\\text{{C}}$ "
            f"dentro de un cuarto a ${fmt(Tamb_c,0)}\\,^\\circ\\text{{C}}$. Calcule la potencia neta irradiada."
        ),
        visual={"tipo": "radiacion", "T": T_K, "e": eps},
    ).usa(r"P_{\text{neta}} = \varepsilon \sigma A (T^4 - T_{\text{amb}}^4)")

    (
        lec.paso("Convertir temperaturas a Kelvin y aplicar Stefan–Boltzmann")
        .por_que("En $T^4$ es obligatorio usar Kelvin; sustituir Celsius da un resultado totalmente erróneo.")
        .di(
            "Primero sumamos 273,15 a cada temperatura para pasar a Kelvin.",
            {"a": "mate", "tex": rf"T = {fmt(T_K,2)}\,\text{{K}},\quad T_{{\text{{amb}}}} = {fmt(Tamb_K,2)}\,\text{{K}}", "color": "azul"},
        )
        .di(
            "Sustituimos en la ley de Stefan Boltzmann con la constante sigma.",
            {"a": "mate", "tex": rf"P_{{\text{{neta}}}} = ({fmt(eps,2)})(5{{,}}67\times 10^{{-8}})({fmt(A,2)})({fmt(T_K,2)}^4 - {fmt(Tamb_K,2)}^4) = {fmt(P_neta,1)}\,\text{{W}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("", rf"P_{{\text{{neta}}}} = {fmt(P_neta,1)}\,\text{{W}}", valor=round(P_neta, 1))
    return lec


def _v_maquina_otto(rng: random.Random) -> Leccion:
    """Arquetipo alto tipo E17 / Q128: eficiencia de un ciclo Otto y potencia térmica."""
    r = rng.choice([7.5, 8.0, 8.5, 9.0, 9.5, 10.0])
    gamma = rng.choice([1.40, 1.35])
    QH = float(rng.choice([1200, 1500, 1800, 2400]))
    e = 1.0 - 1.0 / (r ** (gamma - 1.0))
    W = e * QH
    QC = QH - W

    lec = Leccion(
        f"VAR-OTTO-{rng.randint(100,999)}",
        f"Variante · Motor de ciclo Otto (r = {fmt_txt(r,1)})",
        tema="maquinas",
        tipo="desarrollo",
        nivel=3,
        fuente={"doc": "variante", "nota": "Generada con otros números en Python"},
        enunciado=(
            f"Un motor de gasolina idealizado como ciclo Otto tiene razón de compresión $r = {fmt(r,1)}$ y $\\gamma = {fmt(gamma,2)}$. "
            f"Si en cada ciclo absorbe $Q_H = {fmt(QH,0)}\\text{{ J}}$, calcule la eficiencia $e$, el trabajo $W$ y el calor desechado $|Q_C|$."
        ),
        visual={"tipo": "maquina", "modo": "motor", "QH": QH, "W": round(W), "QC": round(QC)},
    ).usa(r"e_{\text{Otto}} = 1 - \frac{1}{r^{\gamma - 1}}", r"W = e Q_H", r"|Q_C| = Q_H - W")

    (
        lec.paso("Eficiencia del ciclo Otto y balance de calor")
        .por_que("La eficiencia de Otto depende solo de la razón de compresión $r$ y $\\gamma$.")
        .di(
            "Evaluamos la eficiencia del ciclo Otto.",
            {"a": "mate", "tex": rf"e = 1 - \frac{{1}}{{{fmt(r,1)}^{{{fmt(gamma-1,2)}}}}} = {fmt(e*100,1)}\,\%", "color": "azul"},
        )
        .di(
            "Por conservación de la energía, el calor que no sale como trabajo se expulsa al foco frío.",
            {"a": "mate", "tex": rf"W = ({fmt(e,3)})({fmt(QH,0)}) = {fmt(W,0)}\,\text{{J}},\quad |Q_C| = {fmt(QH,0)} - {fmt(W,0)} = {fmt(QC,0)}\,\text{{J}}", "color": "verde", "rol": "resultado"},
        )
    )
    lec.responde("e", rf"e = {fmt(e*100,1)}\,\%", valor=round(e * 100, 1))
    lec.responde("W", rf"W = {fmt(W,0)}\,\text{{J}},\; |Q_C| = {fmt(QC,0)}\,\text{{J}}", valor=round(W))
    return lec


ARQUETIPOS: Dict[str, Callable[[random.Random], Leccion]] = {
    "conduccion_area": _v_conduccion_area,
    "conduccion_k_radial": _v_conduccion_k_radial,
    "ciclo_pv": _v_ciclo_pv,
    "calorimetria_fase": _v_calorimetria_fase,
    "conduccion_serie_paralelo": _v_conduccion_serie_paralelo,
    "dilatacion_barras": _v_dilatacion_barras,
    "radiacion_stefan": _v_radiacion_stefan,
    "maquina_otto_refrig": _v_maquina_otto,
}

_MAPA_DIRECTO: Dict[str, str] = {
    "Q049": "conduccion_area",
    "Q050": "conduccion_area",
    "E8": "conduccion_k_radial",
    "Q057": "conduccion_k_radial",
    "Q060": "conduccion_k_radial",
    "E16": "ciclo_pv",
    "Q111": "ciclo_pv",
    "E5": "calorimetria_fase",
    "Q038": "conduccion_serie_paralelo",
    "E1": "dilatacion_barras",
    "Q063": "radiacion_stefan",
    "E10": "radiacion_stefan",
    "E17": "maquina_otto_refrig",
    "E21": "maquina_otto_refrig",
    "Q117": "maquina_otto_refrig",
    "Q128": "maquina_otto_refrig",
}

_MAPA_TEMA: Dict[str, str] = {
    "dilatacion": "dilatacion_barras",
    "calorimetria": "calorimetria_fase",
    "conduccion": "conduccion_area",
    "radiacion": "radiacion_stefan",
    "gases": "ciclo_pv",
    "primera_ley": "ciclo_pv",
    "ciclos": "ciclo_pv",
    "maquinas": "maquina_otto_refrig",
    "entropia": "maquina_otto_refrig",
}


def arquetipo_de(id_o_tema: str) -> str:
    clave = (id_o_tema or "").strip()
    if clave in ARQUETIPOS:
        return clave
    up = clave.upper()
    if up in _MAPA_DIRECTO:
        return _MAPA_DIRECTO[up]
    if registro.existe(up):
        lec = registro.obtener(up)
        en = (lec.enunciado or "").lower()
        if lec.tema == "conduccion":
            if "k(" in en or "k_0" in en or "esfera" in en or "cilindr" in en:
                return "conduccion_k_radial"
            if "serie" in en or "paralelo" in en:
                return "conduccion_serie_paralelo"
            return "conduccion_area"
        return _MAPA_TEMA.get(lec.tema, "ciclo_pv")
    return _MAPA_TEMA.get(clave.lower(), "conduccion_area")


def generar_variante(id_o_tema: str, semilla: Optional[int] = None) -> Leccion:
    """Genera una variante paramétrica determinista lista para estudiar."""
    arq = arquetipo_de(id_o_tema)
    rng = random.Random(semilla) if semilla is not None else random.Random()
    return ARQUETIPOS[arq](rng)
