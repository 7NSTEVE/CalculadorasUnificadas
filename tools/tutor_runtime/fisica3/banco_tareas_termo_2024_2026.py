"""Tareas oficiales de Termodinámica FI2103 (I-2024 a I-2026), corte estricto ≤ 20.5 (18 lecciones).

Recoge los 18 problemas vigentes (≤ sección 20.5) de las 5 tareas oficiales de cátedra:
  - T4-24I-1, T4-24I-2: Tarea 4 I-2024 (calorimetría 3 fases y esfera hueca radial).
  - T3-24II-1, T3-24II-2: Tarea 3 II-2024 (dilatación exacta exponencial y fracción de hielo).
  - T3-25I-1, T3-25I-2: Tarea 3 I-2025 (dilatación isotrópica/anisotrópica/catenaria y calorimetría).
  - T3-25II-1..T3-25II-4: Tarea 3 II-2025 (Al líquido en agua, ducto cerámico, 1.ª ley y refrigerador).
  - T3-26I-1, T3-26I-2, T3-26I-5..T3-26I-10: Tarea 3 I-2026 (calorimetría, ciclos, pistón con resorte,
    tubería compuesta 2 capas, 3 recipientes de hierro y cono truncado con k(T) = k₀T).

Exclusiones estrictas por corte ≤ 20.5:
  - Tarea 3 I-2026 #3 (Entropía, sección 20.7) y #4 (Ciclo de Carnot, sección 20.6) quedan fuera.

Toda solución parte de las 42 casillas de `documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf`
y cada errata encontrada en los solucionarios oficiales se documenta con total honestidad.
"""

from __future__ import annotations

import math
from typing import Tuple

from .ayudas import R_GAS, T0, Gas, tabla_estados, tabla_quw, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import partes, tex
from .registro import leccion

C = r"^\circ\text{C}"

# Casillas exactas del formulario oficial III Parcial II-2026
F_DL = r"\Delta L = \alpha L_0 \Delta T"
F_DV = r"\Delta V = \beta V_0 \Delta T"
F_ESF = r"\frac{F}{A} = -Y \alpha \Delta T"
F_COND = r"H = \frac{dQ}{dt} = kA\frac{T_H - T_C}{L}"
F_FOUR = r"H = -kA\frac{dT}{dx}"
F_FOUR_R = r"H = -kA\frac{dT}{dr}"
F_QS = r"Q = mc\Delta T = nC\Delta T"
F_QL = r"Q = \pm mL"
F_GAS = r"pV = nRT"
F_W_INT = r"W = \int_{V_1}^{V_2} p\,dV"
F_W_ISO = r"W = p(V_2 - V_1)"
F_W_T = r"W = nRT\ln\left(\frac{V_2}{V_1}\right)"
F_1LEY = r"\Delta U = Q - W"
F_DU = r"\Delta U = nC_V\Delta T"
F_MAYER = r"C_P = C_V + R"
F_CV_M = r"C_{V\text{mono}} = \frac{3}{2}R"
F_CV_D = r"C_{V\text{dia}} = \frac{5}{2}R"
F_GAM = r"\gamma = \frac{C_P}{C_V}"
F_W_AD = r"W = \frac{1}{\gamma - 1}(p_1 V_1 - p_2 V_2)"
F_PVG = r"pV^\gamma = \text{constante}"
F_TVG = r"TV^{\gamma - 1} = \text{constante}"
F_REF = r"K = \frac{|Q_c|}{|W|} = \frac{|Q_c|}{|Q_H| - |Q_c|}"
F_KEL = r"T_K = T_{^\circ\text{C}} + 273{,}15"
F_DQV = r"dQ = nC_V dT"
F_DQP = r"dQ = nC_p dT"


def _f(doc: str, nombre: str, num: str) -> dict:
    return {"doc": doc, "nombre": nombre, "num": num, "parcial": 3}


def _q(x: float, c: int = 4) -> float:
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ 1. T4-24I-1
@leccion("T4-24I-1")
def t4_24i_1() -> Leccion:
    mv, mh, mcu = 0.0350, 0.0950, 0.446
    ca, ccu = 4190.0, 390.0
    lf, lv = 3.340e5, 2.256e6
    q1 = mh * lf              # 31730 J
    q2 = mh * ca * 100.0      # 39805 J
    q3 = mcu * ccu * 100.0    # 17394 J
    q_subir = q1 + q2 + q3    # 88929 J
    q_cond = mv * lv          # 78960 J
    q_v_enf = mv * ca * 100.0 # 14665 J
    coef_t = mh * ca + mcu * ccu + mv * ca  # 718.64 J/°C
    num_t = q_cond + q_v_enf - q1           # 61895 J
    tf = num_t / coef_t                     # 86.128 °C
    m_tot = mv + mh                         # 0.1300 kg

    L = Leccion(
        "T4-24I-1",
        "Vapor a 100 °C, hielo a 0 °C y calorímetro de cobre (Tarea 4 I-2024, E1)",
        tema="4.8", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("24-I-T4", "Tarea 4 I-2024", "1"),
        enunciado=(
            "Se combinan $35{,}0\\ \\text{g}$ de vapor de agua a $100{,}0\\,^\\circ\\text{C}$ con $95{,}0\\ \\text{g}$ "
            "de hielo a $0{,}0\\,^\\circ\\text{C}$ en un calorímetro de cobre de $446\\ \\text{g}$ inicialmente a $0{,}0\\,^\\circ\\text{C}$. "
            "Use $c_a = 4190$, $c_{\\text{Cu}} = 390\\ \\text{J/(kg}\\cdot^\\circ\\text{C)}$, "
            "$L_f = 3{,}340\\times 10^5\\ \\text{J/kg}$ y $L_v = 2{,}256\\times 10^6\\ \\text{J/kg}$. "
            "Determine: A) la temperatura de equilibrio; B) la composición final de la mezcla."
        ),
        claves=["calorimetría", "vapor", "hielo", "cobre", "cambio de fase", "tarea"],
    )
    L.paso("Comparar umbrales antes de despejar")\
        .di("En calorimetría con cambios de fase nunca despejamos la temperatura a ciegas. Primero revisamos cuánta energía absorben hielo y cobre hasta cien grados.",
            DATO("35 g de vapor"),
            F(F_QL), F(F_QS),
            M(rf"Q_1 = m_h L_f = (0{{,}}0950)(3{{,}}340\times 10^5) = {tex(q1, 5)}\ \text{{J}}"),
            M(rf"Q_2 = m_h c_a (100-0) = (0{{,}}0950)(4190)(100) = {tex(q2, 5)}\ \text{{J}}"),
            M(rf"Q_3 = m_{{\text{{Cu}}}} c_{{\text{{Cu}}}} (100-0) = (0{{,}}446)(390)(100) = {tex(q3, 5)}\ \text{{J}}"))
    L.paso("Energía que libera el vapor al condensar")\
        .di("Ahora vemos cuánto libera todo el vapor solo al volverse agua líquida a cien grados. Lo comparamos con los dos umbrales anteriores.",
            M(rf"Q_1+Q_2+Q_3 = 31\,730+39\,805+17\,394 = {tex(q_subir, 5)}\ \text{{J}}"),
            M(rf"|Q_{{\text{{cond}}}}| = m_v L_v = (0{{,}}0350)(2{{,}}256\times 10^6) = {tex(q_cond, 5)}\ \text{{J}}"))\
        .di("Como setenta y ocho mil novecientos sesenta supera la fusión del hielo pero no llega a ochenta y ocho mil novecientos veintinueve, todo queda líquido entre cero y cien grados.",
            NOTA("Umbral verificado: $31\\,730\\ \\text{J} < 78\\,960\\ \\text{J} < 88\\,929\\ \\text{J} \\Rightarrow 0\\,^\\circ\\text{C} < T_f < 100\\,^\\circ\\text{C}$."))
    L.paso("A) Balance total de calor para T final")\
        .di("Sumamos todos los calores igual a cero: el hielo se funde y sube, el cobre sube, el vapor se condensa y su agua baja desde cien grados.",
            M(r"Q_{\text{hielo}} + Q_{\text{Cu}} + Q_{\text{vapor}} = 0"),
            M(r"m_h L_f + m_h c_a(T_f-0) + m_{\text{Cu}}c_{\text{Cu}}(T_f-0) - m_v L_v + m_v c_a(T_f-100) = 0"))\
        .di("Sustituimos cada coeficiente que multiplica a la temperatura final y los términos constantes.",
            M(r"31\,730 + 398{,}05\,T_f + 173{,}94\,T_f - 78\,960 + 146{,}65\,T_f - 14\,665 = 0"),
            M(rf"{tex(coef_t, 5)}\,T_f = {tex(num_t, 5)}"),
            M(rf"T_f = \frac{{{tex(num_t, 5)}}}{{{tex(coef_t, 5)}}} = {tex(tf, 4)}\ {C}"),
            R(rf"T_f = {tex(tf, 3)}\ {C}"))
    L.paso("B) Composición final de la mezcla")\
        .di("Como ochenta y seis coma uno grados está entre cero y cien, la hipótesis se confirma: todo el hielo se derritió y todo el vapor se condensó.",
            M(r"m_{\text{agua}} = m_v + m_h = 0{,}0350 + 0{,}0950 = 0{,}1300\ \text{kg}"),
            R(r"m_{\text{agua}} = 0{,}1300\ \text{kg}\ \text{(130 g de agua líquida, sin hielo ni vapor)}"))
    L.usa(F_QS, F_QL)
    L.responde("A", rf"T_f = {tex(tf, 3)}\ {C}", tf, "°C")
    L.responde("B", r"m_{\text{agua}} = 0{,}1300\ \text{kg}", m_tot, "kg")
    return L.oficial("Tarea 4 I-2024", 86.1, 0.1300)


# ============================================================================ 2. T4-24I-2
@leccion("T4-24I-2")
def t4_24i_2() -> Leccion:
    k_al = 205.0
    ri, r_ext = 0.05, 0.45
    h_w = 2000.0
    ti_c = 500.0
    ti_k = ti_c + T0
    inv_diff = 1.0 / ri - 1.0 / r_ext  # 17.7778 m^-1
    factor = h_w / (4.0 * math.pi * k_al)
    dT_rad = factor * inv_diff  # 13.802 K
    t_ext_c = ti_k - dT_rad     # 759.35 K
    ti_fus = 660.3 + T0         # 933.45 K
    t_ext_d = ti_fus - dT_rad   # 919.65 K
    rho_th = inv_diff / (4.0 * math.pi * k_al)  # 0.006901 K/W

    filas_c = []
    for cm in (5, 10, 15, 20, 25, 30, 35, 40, 45):
        rr = cm / 100.0
        tt = ti_k - factor * (1.0 / ri - 1.0 / rr)
        filas_c.append([f"${tex(rr, dec=2)}$", f"${tex(tt, dec=2)}$"])

    L = Leccion(
        "T4-24I-2",
        "Conducción radial en esfera hueca de aluminio (Tarea 4 I-2024, E2)",
        tema="5.4", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("24-I-T4", "Tarea 4 I-2024", "2"),
        enunciado=(
            "Considere conducción radial estacionaria por una esfera hueca de conductividad $k$ con radio interior $R_i$ "
            "a $T_i$ y exterior $R$ a $T$. a) Halle $dT/dr$ en función de $r$. b) Deduzca $H$ en términos de $T_i, T, R_i, R$ y $k$. "
            "c) Para aluminio ($k = 205\\ \\text{W/(m}\\cdot\\text{K)}$), $R_i = 5\\ \\text{cm}$, $R = 45\\ \\text{cm}$, "
            "$T_i = 500\\,^\\circ\\text{C}$ y $H = 2000\\ \\text{W}$, tabule $T(r)$ cada $5\\ \\text{cm}$. "
            "d) Si $T_i$ sube al punto de fusión del aluminio ($660{,}3\\,^\\circ\\text{C}$), halle $T(0{,}45\\ \\text{m})$. "
            "e) Halle la resistencia térmica global $\\rho = \\Delta T / H$."
        ),
        aviso=(
            "Errata en la solución oficial (inciso d): reportó 979,64 K para la superficie exterior, "
            "más caliente que el interior (933,45 K), violando la segunda ley. El valor exacto al restar "
            "la caída radial de 13,80 K es 919,65 K."
        ),
        claves=["esfera hueca", "Fourier", "integral", "gradiente", "resistencia térmica", "tarea"],
    )
    L.paso("a) Gradiente radial desde Fourier")\
        .di("Partimos de la ley diferencial de Fourier del formulario. En simetría esférica el área a radio erre es la superficie de una esfera: cuatro pi erre cuadrado.",
            DATO("esfera hueca"),
            F(F_FOUR_R),
            M(r"A(r) = 4\pi r^2"),
            M(r"H = -k\,(4\pi r^2)\frac{dT}{dr}"))\
        .di("Despejamos la derivada de la temperatura respecto al radio: el signo menos confirma que la temperatura baja al salir.",
            R(r"\frac{dT}{dr} = -\frac{H}{4\pi k r^2}"))
    L.paso("b) Separar variables e integrar para H")\
        .di("Como el flujo hache es constante en régimen estacionario, separamos erre a un lado y te al otro, e integramos desde el radio interno hasta el externo.",
            M(r"\frac{H}{4\pi}\int_{R_i}^{R}\frac{dr}{r^2} = -k\int_{T_i}^{T} dT'"),
            M(r"\frac{H}{4\pi}\left[-\frac{1}{r}\right]_{R_i}^{R} = -k\,(T - T_i)"),
            M(r"\frac{H}{4\pi}\left(\frac{1}{R_i}-\frac{1}{R}\right) = k\,(T_i - T)"))\
        .di("Sacamos común denominador en el paréntesis de radios para simplificar la expresión final del flujo hache.",
            M(r"\frac{1}{R_i} - \frac{1}{R} = \frac{R - R_i}{R R_i}"),
            M(r"H = \frac{4\pi k(T_i - T)}{(R - R_i)/(R R_i)}"),
            R(r"H = \frac{4\pi k R R_i (T_i - T)}{R - R_i}"))
    L.paso("c) Perfil radial T(r) cada 5 cm")\
        .di("Pasamos quinientos grados Celsius a kelvin con la casilla del formulario y despejamos te en función del radio.",
            F(F_KEL),
            M(r"T_i = 500 + 273{,}15 = 773{,}15\ \text{K}"),
            M(r"T(r) = T_i - \frac{H}{4\pi k}\left(\frac{1}{R_i}-\frac{1}{r}\right)"),
            M(rf"\frac{{H}}{{4\pi k}} = \frac{{2000}}{{4\pi(205)}} = {tex(factor, 4)}\ \text{{m}}\cdot\text{{K}}"))\
        .di("Al evaluar de cinco en cinco centímetros, la temperatura cae rápido cerca del centro y casi se aplana afuera.",
            TABLA(["$r$ (m)", "$T(r)$ (K)"], filas_c, resaltar=[[0, None], [8, None]]))
    L.paso("d) Temperatura externa con interior a fusión")\
        .di("El aluminio funde a seiscientos sesenta coma tres grados Celsius, es decir novecientos treinta y tres coma cuarenta y cinco kelvin.",
            M(r"T_i = 660{,}3 + 273{,}15 = 933{,}45\ \text{K}"),
            M(rf"\frac{{1}}{{0{{,}}05}} - \frac{{1}}{{0{{,}}45}} = 20 - 2{{,}}222 = {tex(inv_diff, 5)}\ \text{{m}}^{{-1}}"),
            M(rf"\Delta T = ({tex(factor, 4)})({tex(inv_diff, 5)}) = {tex(dT_rad, 4)}\ \text{{K}}"))\
        .di("Restamos esa caída de temperatura a la pared interna. Nunca puede dar mayor que novecientos treinta y tres kelvin.",
            M(rf"T(0{{,}}45\ \text{{m}}) = 933{{,}}45 - {tex(dT_rad, 4)} = {tex(t_ext_d, 5)}\ \text{{K}}"),
            R(rf"T(0{{,}}45\ \text{{m}}) = {tex(t_ext_d, 5)}\ \text{{K}}"),
            OJO("El solucionario imprimió 979,64 K por error aritmético; el exterior siempre debe estar más frío que el interior."))
    L.paso("e) Resistencia térmica global de la esfera")\
        .di("Comparando el flujo esférico con la conducción estacionaria del formulario, la razón cambio de temperatura entre flujo define la resistencia térmica global.",
            F(F_COND),
            M(r"\rho = \frac{\Delta T}{H} = \frac{1}{4\pi k}\left(\frac{1}{R_i}-\frac{1}{R}\right)"),
            M(rf"\rho = \frac{{{tex(inv_diff, 5)}}}{{4\pi(205)}} = {tex(rho_th, 4)}\ \text{{K/W}}"),
            R(rf"\rho = {tex(rho_th, 3)}\ \text{{K/W}} \approx 0{{,}}007\ \text{{K/W}}"))
    L.usa(F_FOUR_R, F_COND, F_KEL)
    L.responde("c-ext", rf"T(0{{,}}45) = {tex(t_ext_c, 5)}\ \text{{K}}", t_ext_c, "K")
    L.responde("d", rf"T = {tex(t_ext_d, 5)}\ \text{{K}}", t_ext_d, "K")
    L.responde("e", rf"\rho = {tex(rho_th, 3)}\ \text{{K/W}}", rho_th, "K/W")
    return L.oficial(
        "Tarea 4 I-2024", 759.34, 979.64, 0.007, tol=0.03,
        nota="Inciso (d) oficial tiene errata de signo/aritmética (979,64 K > T_interna 933,45 K); el físico exacto es 919,65 K.",
    )


# ============================================================================ 3. T3-24II-1
@leccion("T3-24II-1")
def t3_24ii_1() -> Leccion:
    l0 = 2.0
    alfa = 12e-6
    dt = 60.0
    x_exp = alfa * dt
    l_exact = l0 * math.exp(x_exp)
    l_aprox = l0 * (1.0 + x_exp)
    diff_rel = abs(l_exact - l_aprox) / l_exact

    filas_e = []
    for mat, a_val in (
        ("Vidrio", 9e-6), ("Acero", 12e-6), ("Cobre", 16e-6),
        ("Aluminio", 23e-6), ("Polipropileno", 150e-6),
    ):
        le = 1.0 * math.exp(a_val * 200.0)
        la = 1.0 * (1.0 + a_val * 200.0)
        dif_mm = (le - la) * 1000.0
        filas_e.append([mat, f"${tex(a_val * 1e6, 3)}\\times 10^{{-6}}$", f"${tex(le, 6)}$", f"${tex(la, 6)}$", f"${tex(dif_mm, 3)}$"])

    L = Leccion(
        "T3-24II-1",
        "Dilatación exacta exponencial vs aproximación lineal (Tarea 3 II-2024, E1)",
        tema="4.4", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("24-II-T3", "Tarea 3 II-2024", "1"),
        enunciado=(
            "Un cilindro de acero ($\\alpha = 12\\times 10^{-6}\\ ^\\circ\\text{C}^{-1}$) tiene longitud inicial "
            "$L_0 = 2{,}0\\ \\text{m}$ a $20\\,^\\circ\\text{C}$ y se calienta hasta $80\\,^\\circ\\text{C}$. "
            "A) Integre $dL/dT = \\alpha L$ para obtener la expresión exacta $L(T)$. "
            "B) Deduzca la fórmula lineal con la serie de Maclaurin de $e^x$. "
            "C) Calcule $L_f$ con ambos métodos. D) Compare y justifique si la aproximación es válida. "
            "E–F) Compare vidrio, acero, cobre, aluminio y polipropileno ($\\alpha = 150\\times 10^{-6}\\ ^\\circ\\text{C}^{-1}$) hasta $\\Delta T = 200\\,^\\circ\\text{C}$."
        ),
        claves=["dilatación lineal", "exponencial", "Taylor", "integral", "polipropileno", "tarea"],
    )
    L.paso("A) Ecuación exacta integrando dL/dT = αL")\
        .di("La dilatación lineal del formulario surge de una tasa instantánea proporcional a la longitud actual. Separamos variables e integramos desde longitud inicial hasta longitud final.",
            DATO("integrar la relación"),
            F(F_DL),
            M(r"\frac{dL}{dT} = \alpha L \;\Rightarrow\; \int_{L_0}^{L}\frac{dL'}{L'} = \int_{T_0}^{T}\alpha\,dT'"),
            M(r"\ln\left(\frac{L}{L_0}\right) = \alpha(T - T_0) = \alpha\Delta T"))\
        .di("Aplicamos exponencial a ambos lados para despejar la longitud exacta.",
            R(r"L = L_0\,e^{\alpha\Delta T}"))
    L.paso("B) Deducción de la fórmula lineal con Maclaurin")\
        .di("Expandimos la exponencial en serie de Maclaurin alrededor de cero. Cuando alfa por delta te es mucho menor que uno, el término cuadrático es despreciable.",
            M(r"e^{x} = 1 + x + \frac{x^2}{2!} + \frac{x^3}{3!} + \cdots \approx 1 + x"),
            M(r"L \approx L_0(1 + \alpha\Delta T) = L_0 + \alpha L_0 \Delta T"),
            R(r"\Delta L \approx \alpha L_0 \Delta T"))
    L.paso("C) Cálculo para el acero de 20 °C a 80 °C")\
        .di("El cambio de temperatura es sesenta grados y el producto alfa por delta te vale siete coma dos por diez a la menos cuatro.",
            M(r"\Delta T = 80 - 20 = 60\ ^\circ\text{C}"),
            M(rf"\alpha\Delta T = (12\times 10^{{-6}})(60) = {tex(x_exp, 3)}"),
            M(rf"L_{{\text{{exacta}}}} = 2{{,}}0\,e^{{{tex(x_exp, 3)}}} = {tex(l_exact, 7)}\ \text{{m}}"),
            M(rf"L_{{\text{{aprox}}}} = 2{{,}}0(1 + {tex(x_exp, 3)}) = {tex(l_aprox, 7)}\ \text{{m}}"),
            R(r"L_f = 2{,}00144\ \text{m}\ \text{(ambos métodos)}"))
    L.paso("D–F) Comparación entre metales y polímeros")\
        .di("En el acero el error relativo es de dos coma seis en diez millones, así que la lineal es excelente.",
            M(rf"\frac{{|L_{{\text{{exacta}}}} - L_{{\text{{aprox}}}}|}}{{L_{{\text{{exacta}}}}}} = {tex(diff_rel, 2)}"))\
        .di("En cambio, en plásticos con alfa grande y doscientos grados de cambio sí se nota la separación entre ambas curvas.",
            TABLA(["Material", r"$\alpha$ ($^\circ\text{C}^{-1}$)", "$L_{\\text{exacta}}$ (m)", "$L_{\\text{aprox}}$ (m)", "Dif. (mm)"], filas_e, resaltar=[[4, None]]),
            NOTA("Para $L_0 = 1\\ \\text{m}$ y $\\Delta T = 200\\,^\\circ\\text{C}$, la fórmula exacta siempre crece más que la lineal por los términos positivos $\\frac{1}{2}(\\alpha\\Delta T)^2$."))
    L.usa(F_DL)
    L.responde("C-exacta", r"L_{\text{exacta}} = 2{,}00144\ \text{m}", l_exact, "m")
    L.responde("C-aprox", r"L_{\text{aprox}} = 2{,}00144\ \text{m}", l_aprox, "m")
    return L.oficial("Tarea 3 II-2024", 2.00144, 2.00144)


# ============================================================================ 4. T3-24II-2
@leccion("T3-24II-2")
def t3_24ii_2() -> Leccion:
    ca = 4190.0
    lf = 333e3
    t_crit = lf / ca           # 79.475 °C
    a90_raw = ca * 90.0 / lf   # 1.1324 -> 1
    t90 = (ca * 90.0 - lf) / (2.0 * ca)  # 5.2625 °C
    a70 = ca * 70.0 / lf       # 0.88078 < 1
    t70 = 0.0

    L = Leccion(
        "T3-24II-2",
        "Fracción de hielo derretido al preparar té verde frío (Tarea 3 II-2024, E2)",
        tema="4.8", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("24-II-T3", "Tarea 3 II-2024", "2"),
        enunciado=(
            "Se mezclan $595\\ \\text{g}$ de té verde caliente ($c = c_a = 4190\\ \\text{J/(kg}\\cdot\\text{K)}$) con "
            "la misma masa de hielo a $0\\,^\\circ\\text{C}$ ($L_f = 333\\times 10^3\\ \\text{J/kg}$). "
            "A) Deduzca la fracción de hielo derretido $\\alpha$ en función de la temperatura inicial del té $T_{i,\\text{té}}$. "
            "B) Halle e interprete el punto donde $\\alpha = 1$. "
            "C) Calcule la temperatura final de equilibrio para $T_{i,\\text{té}} = 90\\,^\\circ\\text{C}$ y $70\\,^\\circ\\text{C}$."
        ),
        aviso=(
            "Errata menor de redondeo en la solución oficial (inciso C a 70 °C): sustituyó α redondeado a 0,88 "
            "en la fórmula de T y obtuvo 0,03 °C. Físicamente, mientras queda hielo sin fundir (α = 0,881 < 1), "
            "la temperatura de equilibrio exacta es 0,0 °C."
        ),
        claves=["calorimetría", "fracción derretida", "fusión parcial", "umbral", "tarea"],
    )
    L.paso("A) Balance general con fracción derretida α")\
        .di("Como no sabemos si se derrite todo el hielo, llamamos alfa a la fracción que funde, entre cero y uno. Como las masas son iguales, se cancelan.",
            DATO("595 g de té verde"),
            F(F_QL), F(F_QS),
            M(r"\alpha m L_f + \alpha m c_a(T - 0) + m c_a(T - T_{i,\text{té}}) = 0"))\
        .di("Dividimos entre la masa eme y despejamos la temperatura de equilibrio te.",
            M(r"\alpha L_f + c_a(\alpha + 1)T - c_a T_{i,\text{té}} = 0"),
            M(r"T = \frac{c_a T_{i,\text{té}} - \alpha L_f}{c_a(\alpha + 1)}"))
    L.paso("Condición de fusión parcial y fórmula de α")\
        .di("Mientras haya hielo sin fundir, el equilibrio está exactamente en cero grados. Igualando el numerador a cero sale alfa en función de la temperatura inicial.",
            M(r"T = 0 \;\Rightarrow\; c_a T_{i,\text{té}} - \alpha L_f = 0"),
            R(r"\alpha(T_{i,\text{té}}) = \frac{c_a}{L_f}\,T_{i,\text{té}} \quad (0 \le \alpha \le 1)"))
    L.paso("B) Punto crítico donde α = 1")\
        .di("Igualamos alfa a uno para saber desde qué temperatura del té se derrite hasta el último gramo de hielo.",
            M(rf"T_{{\text{{crit}}}} = \frac{{L_f}}{{c_a}} = \frac{{333\times 10^3}}{{4190}} = {tex(t_crit, 4)}\ {C}"),
            R(rf"\alpha = 1\ \text{{en}}\ T_{{i,\text{{té}}}} = {tex(t_crit, 4)}\ {C}"))\
        .di("Por debajo de setenta y nueve coma cinco grados sobra hielo y la mezcla queda en cero grados; por encima, todo se funde y la temperatura sube.",
            NOTA("Si $T_{i,\\text{té}} > 79{,}47\\,^\\circ\\text{C}$, $\\alpha$ se satura físicamente en $1$ (no puede fundirse más del 100 % del hielo)."))
    L.paso("C) Temperaturas finales para 90 °C y 70 °C")\
        .di("Para noventa grados, la fórmula da uno coma trece, así que se funde el ciento por ciento del hielo y usamos alfa igual a uno.",
            M(rf"\alpha_{{90}} = \frac{{(4190)(90)}}{{333\times 10^3}} = {tex(a90_raw, 4)} > 1 \;\Rightarrow\; \alpha = 1"),
            M(rf"T_{{90}} = \frac{{(4190)(90) - 333\times 10^3}}{{(4190)(2)}} = \frac{{44\,100}}{{8380}} = {tex(t90, 3)}\ {C}"),
            R(rf"T_{{90}} = {tex(t90, 2)}\ {C}"))\
        .di("Para setenta grados solo se funde el ochenta y ocho coma uno por ciento del hielo, así que coexisten agua y hielo a cero grados exactos.",
            M(rf"\alpha_{{70}} = \frac{{(4190)(70)}}{{333\times 10^3}} = {tex(a70, 4)} < 1"),
            R(rf"T_{{70}} = 0{{,}}0\ {C}"))
    L.usa(F_QS, F_QL)
    L.responde("B", rf"T_{{\text{{crit}}}} = {tex(t_crit, 4)}\ {C}", t_crit, "°C")
    L.responde("C-90", rf"T_{{90}} = {tex(t90, 2)}\ {C}", t90, "°C")
    L.responde("C-70", rf"T_{{70}} = 0{{,}}0\ {C}", t70, "°C")
    return L.oficial(
        "Tarea 3 II-2024", t_crit, 5.3, 0.03, tol=0.02,
        nota="Con 70 °C la solución oficial puso 0,03 °C por arrastrar α redondeado a 0,88; el físico exacto es 0,0 °C.",
    )


# ============================================================================ 5. T3-25I-1
@leccion("T3-25I-1")
def t3_25i_1() -> Leccion:
    l0, w0, h0 = 1.0, 0.5, 0.2
    a0, k_t, dt = 1.0e-5, 0.01, 20.0
    l_f = l0 * (1.0 + a0 * dt)   # 1.0002 m
    w_f = w0 * (1.0 + a0 * dt)   # 0.5001 m
    expo = a0 * (dt + 0.5 * k_t * dt * dt)  # 2.2e-4
    h_f = h0 * math.exp(expo)    # 0.2000440048 m
    v_f = l_f * w_f * h_f        # 0.100062013 m^3
    sh = math.sinh(20.0 / 30.0)  # 0.717914
    l_arco = 60.0 * sh           # 43.0748 m
    dl_arco = a0 * l_arco * 40.0 # 0.017230 m = 1.72 cm

    L = Leccion(
        "T3-25I-1",
        "Dilatación isotrópica (β = 3α), anisotrópica con α(T) y arco catenaria (Tarea 3 I-2025, E1)",
        tema="4.4", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("25-I-T3", "Tarea 3 I-2025", "1"),
        enunciado=(
            "I Parte: A partir de $r(T) = r_0(1 + \\alpha\\Delta T)$ para una esfera, deduzca $V(T)$ y demuestre $\\beta = 3\\alpha$. "
            "II Parte: Un bloque anisotrópico de $L_0 = 1{,}0\\ \\text{m}$, $W_0 = 0{,}5\\ \\text{m}$, $H_0 = 0{,}2\\ \\text{m}$ a $T_0 = 300\\ \\text{K}$ "
            "tiene $\\alpha_0 = 1{,}0\\times 10^{-5}\\ \\text{K}^{-1}$ en $x, y$ y $\\alpha(T) = \\alpha_0[1 + k(T - T_0)]$ con $k = 0{,}01\\ \\text{K}^{-1}$ en $z$. "
            "Deduzca $H(T)$ y calcule $V(320\\ \\text{K})$. "
            "III Parte: El arco catenaria $y(x) = 30\\cosh(x/30)$ de $x = -20\\ \\text{m}$ a $+20\\ \\text{m}$ ($\\alpha = 1{,}0\\times 10^{-5}\\ \\text{K}^{-1}$) "
            "sufre $\\Delta T = 40\\,^\\circ\\text{C}$. Halle la longitud de arco $L$, su expansión $\\Delta L$ y las implicaciones estructurales."
        ),
        aviso=(
            "Errata aritmética en la solución oficial (Parte II): obtuvo H(320 K) ≈ 0,200066 m y V ≈ 0,100053 m³ "
            "al evaluar mal el exponente y el producto. El exponente exacto es 10⁻⁵·(20 + 0,005·400) = 2,2×10⁻⁴, "
            "lo que da H = 0,200044 m y V = 0,100062 m³."
        ),
        claves=["dilatación volumétrica", "beta igual 3 alfa", "anisotrópico", "catenaria", "integral", "tarea"],
    )
    L.paso("I) Demostración de β = 3α en una esfera")\
        .di("Partimos de la dilatación lineal del formulario aplicada al radio de la esfera y elevamos al cubo usando el volumen de una esfera.",
            DATO("objeto isotrópico"),
            F(F_DL),
            M(r"V(T) = \frac{4}{3}\pi r(T)^3 = \frac{4}{3}\pi r_0^3(1 + \alpha\Delta T)^3 = V_0(1 + \alpha\Delta T)^3"))\
        .di("Al desarrollar el cubo y despreciar los términos cuadrático y cúbico en alfa delta te, igualamos directamente con la dilatación volumétrica del formulario.",
            F(F_DV),
            M(r"(1 + \alpha\Delta T)^3 \approx 1 + 3\alpha\Delta T \;\Rightarrow\; \Delta V \approx 3\alpha V_0 \Delta T"),
            M(r"\beta V_0 \Delta T = 3\alpha V_0 \Delta T \;\Rightarrow\; \beta = 3\alpha"),
            R(r"\beta = 3\alpha"))
    L.paso("II-A) Integración de H(T) en el eje z")\
        .di("En la dirección zeta el coeficiente varía con la temperatura. Separamos variables e integramos desde te cero hasta te.",
            M(r"\frac{dH}{H} = \alpha_0\bigl[1 + k(T - T_0)\bigr]\,dT"),
            M(r"\int_{H_0}^{H}\frac{dH'}{H'} = \alpha_0\int_{T_0}^{T}\bigl[1 + k(T' - T_0)\bigr]\,dT'"))\
        .di("Evaluamos las antiderivadas y aplicamos exponencial a ambos lados para despejar la altura.",
            M(r"\ln\left(\frac{H}{H_0}\right) = \alpha_0\left[(T - T_0) + \frac{k}{2}(T - T_0)^2\right]"),
            M(r"H(T) = H_0 e^{I(T)} = H_0\,\exp\left(\alpha_0\left[(T-T_0) + \frac{k}{2}(T-T_0)^2\right]\right)"),
            R(r"H(T) = H_0\,\exp\left(\alpha_0\left[(T-T_0) + \frac{k}{2}(T-T_0)^2\right]\right)"))
    L.paso("II-B) Dimensiones y volumen a 320 K")\
        .di("Con un aumento de veinte kelvin, calculamos primero largo y ancho con dilatación lineal constante, y luego el exponente exacto para la altura.",
            M(rf"L = 1{{,}}0\bigl(1 + 1{{,}}0\times 10^{{-5}}(20)\bigr) = {tex(l_f, 5)}\ \text{{m}}"),
            M(rf"W = 0{{,}}5\bigl(1 + 1{{,}}0\times 10^{{-5}}(20)\bigr) = {tex(w_f, 4)}\ \text{{m}}"),
            M(rf"\alpha_0\left[20 + \frac{{0{{,}}01}}{{2}}(20)^2\right] = 1{{,}}0\times 10^{{-5}}(20 + 2) = {tex(expo, 2)}"))\
        .di("Evaluamos la altura exacta y multiplicamos las tres dimensiones finales para hallar el volumen del bloque.",
            M(rf"H(320\ \text{{K}}) = 0{{,}}2\,e^{{{tex(expo, 2)}}} = {tex(h_f, 6)}\ \text{{m}}"),
            M(rf"V = ({tex(l_f, 5)})({tex(w_f, 4)})({tex(h_f, 6)}) = {tex(v_f, 6)}\ \text{{m}}^3"),
            R(rf"V(320\ \text{{K}}) = {tex(v_f, 6)}\ \text{{m}}^3"))
    L.paso("III) Longitud del arco catenaria y expansión")\
        .di("La derivada de treinta coseno hiperbólico de equis entre treinta es seno hiperbólico. Usamos la identidad hiperbólica en la integral de longitud de arco.",
            M(r"L = \int_{-20}^{20}\sqrt{1 + \left(\frac{dy}{dx}\right)^2}\,dx = \int_{-20}^{20}\sqrt{1 + \sinh^2\left(\frac{x}{30}\right)}\,dx"),
            M(r"L = \int_{-20}^{20}\cosh\left(\frac{x}{30}\right)\,dx = 2(30)\sinh\left(\frac{20}{30}\right)"),
            M(rf"L = 60({tex(sh, 4)}) = {tex(l_arco, 4)}\ \text{{m}}"))\
        .di("Sustituimos esta longitud en la casilla de dilatación lineal para cuarenta grados. Si los extremos están rígidos aparecería esfuerzo térmico destructivo, por eso se diseñan juntas.",
            M(rf"\Delta L = (1{{,}}0\times 10^{{-5}})({tex(l_arco, 4)})(40) = {tex(dl_arco, 3)}\ \text{{m}} = {tex(dl_arco * 100, 3)}\ \text{{cm}}"),
            F(F_ESF),
            R(rf"L = {tex(l_arco, 4)}\ \text{{m}},\qquad \Delta L = {tex(dl_arco * 100, 3)}\ \text{{cm}}"))
    L.usa(F_DL, F_DV, F_ESF)
    L.responde("II-H", rf"H = {tex(h_f, 6)}\ \text{{m}}", h_f, "m")
    L.responde("II-V", rf"V = {tex(v_f, 6)}\ \text{{m}}^3", v_f, "m³")
    L.responde("III-L", rf"L = {tex(l_arco, 4)}\ \text{{m}}", l_arco, "m")
    L.responde("III-dL", rf"\Delta L = {tex(dl_arco * 100, 3)}\ \text{{cm}}", dl_arco * 100, "cm")
    return L.oficial(
        "Tarea 3 I-2025", 0.200066, 0.100053, 43.07, 1.72, tol=0.005,
        nota="En Parte II, H(320 K) oficial dio 0,200066 m por error al calcular 20 + 0,005·400 = 22 (dando 0,200044 m exacto).",
    )


# ============================================================================ 6. T3-25I-2
@leccion("T3-25I-2")
def t3_25i_2() -> Leccion:
    ca, ccu, cet = 4.184, 0.385, 2.46
    tf_ab = (100 * ca * 50 + 100 * ca * 20) / (200 * ca)          # 35.0 °C
    tf_cd = (50 * ccu * 200 + 50 * ca * 20) / (50 * ccu + 50 * ca)  # 35.165 °C
    tf_et = (50 * ccu * 200 + 50 * cet * 20) / (50 * ccu + 50 * cet) # 44.362 °C
    m_cu_e = (50 * ca * (70 - 20)) / (ccu * (200 - 70))           # 208.991 g
    c_unk = (50 * ca * (35.28 - 20)) / (50 * (200 - 35.28))       # 0.38812 J/(g°C)
    c_unk_q = _q(c_unk, 3)                                        # 0.388
    num_h = 50 * c_unk_q * 200 + 50 * ca * 20 + 70 * cet * 40     # 14952
    den_h = 50 * c_unk_q + 50 * ca + 70 * cet                     # 400.8
    tf_h = num_h / den_h                                          # 37.305 °C

    L = Leccion(
        "T3-25I-2",
        "Calorimetría teórica de mezclas binarias y ternarias (Tarea 3 I-2025, E2)",
        tema="4.5", tipo="desarrollo", nivel=2, parcial=3,
        fuente=_f("25-I-T3", "Tarea 3 I-2025", "2"),
        enunciado=(
            "En un calorímetro ideal ($c_{\\text{agua}} = 4{,}184$, $c_{\\text{Cu}} = 0{,}385$, $c_{\\text{etanol}} = 2{,}46\\ \\text{J/(g}\\cdot^\\circ\\text{C)}$): "
            "A–B) $100\\ \\text{g}$ de agua a $50\\,^\\circ\\text{C}$ con $100\\ \\text{g}$ de agua a $20\\,^\\circ\\text{C}$: halle $T_f$. "
            "C–D) $50\\ \\text{g}$ de cobre a $200\\,^\\circ\\text{C}$ con $50\\ \\text{g}$ de agua a $20\\,^\\circ\\text{C}$: halle $T_f$ y compare si se cambia el agua por etanol. "
            "E) ¿Cuántos gramos de cobre a $200\\,^\\circ\\text{C}$ llevan esos $50\\ \\text{g}$ de agua a $70\\,^\\circ\\text{C}$? "
            "F–G) $50\\ \\text{g}$ de un metal desconocido a $200\\,^\\circ\\text{C}$ con $50\\ \\text{g}$ de agua a $20\\,^\\circ\\text{C}$ llegan a $35{,}28\\,^\\circ\\text{C}$: halle $c_x$. "
            "H) Si a la mezcla F se añaden $70\\ \\text{g}$ de etanol a $40\\,^\\circ\\text{C}$, halle la nueva $T_f$."
        ),
        claves=["calorimetría", "calor específico", "cobre", "etanol", "mezcla ternaria", "tarea"],
    )
    L.paso("A–B) Dos masas iguales de agua")\
        .di("Como el calorímetro es ideal, la suma de calores es cero y el calor perdido por el cuerpo caliente iguala en magnitud al ganado por el frío.",
            DATO("calorímetro ideal"),
            F(F_QS),
            M(r"m_1 c_1(T_f - T_{i1}) + m_2 c_2(T_f - T_{i2}) = 0"),
            M(rf"T_f = \frac{{(100)(4{{,}}184)(50) + (100)(4{{,}}184)(20)}}{{(100)(4{{,}}184) + (100)(4{{,}}184)}} = \frac{{50 + 20}}{{2}} = {tex(tf_ab, 3)}\ {C}"),
            R(rf"T_{{f,\text{{AB}}}} = {tex(tf_ab, 3)}\ {C}"))
    L.paso("C–D) Cobre en agua vs cobre en etanol")\
        .di("Sustituimos cincuenta gramos de cobre a doscientos grados con cincuenta gramos de agua a veinte grados. El agua domina por su gran calor específico.",
            M(rf"T_f = \frac{{(50)(0{{,}}385)(200) + (50)(4{{,}}184)(20)}}{{(50)(0{{,}}385) + (50)(4{{,}}184)}} = \frac{{8034}}{{228{{,}}45}} = {tex(tf_cd, 4)}\ {C}"),
            R(rf"T_{{f,\text{{Cu+agua}}}} = {tex(tf_cd, 4)}\ {C}"))\
        .di("Si cambiamos el agua por etanol, como su calor específico es menor, necesita menos energía por grado y el equilibrio sube.",
            M(rf"T_{{f,\text{{Cu+etanol}}}} = \frac{{3850 + 2460}}{{19{{,}}25 + 123}} = \frac{{6310}}{{142{{,}}25}} = {tex(tf_et, 4)}\ {C}"))
    L.paso("E) Masa de cobre para llegar a 70 °C")\
        .di("Fijamos la temperatura final en setenta grados y despejamos la masa de cobre del balance de calor.",
            M(r"m_{\text{Cu}} c_{\text{Cu}}(70 - 200) + m_a c_a(70 - 20) = 0"),
            M(rf"m_{{\text{{Cu}}}} = \frac{{(50)(4{{,}}184)(50)}}{{(0{{,}}385)(130)}} = \frac{{10\,460}}{{50{{,}}05}} = {tex(m_cu_e, 4)}\ \text{{g}}"),
            R(rf"m_{{\text{{Cu}}}} = {tex(m_cu_e, 4)}\ \text{{g}}"))
    L.paso("F–G) Calor específico del metal desconocido")\
        .di("Con la temperatura final medida de treinta y cinco coma veintiocho grados, despejamos el calor específico del metal.",
            M(r"c_x = \frac{-m_a c_a(T_f - 20)}{m_x(T_f - 200)}"),
            M(rf"c_x = \frac{{(50)(4{{,}}184)(15{{,}}28)}}{{(50)(164{{,}}72)}} = \frac{{63{{,}}93}}{{164{{,}}72}} = {tex(c_unk, 3)}\ \frac{{\text{{J}}}}{{\text{{g}}\cdot {C}}}"),
            R(rf"c_x = {tex(c_unk_q, 3)}\ \frac{{\text{{J}}}}{{\text{{g}}\cdot {C}}}"))\
        .di("Es ligeramente mayor que el cobre y mucho menor que el aluminio, así que requiere más energía por gramo que el cobre pero menos que el aluminio.",
            NOTA("Comparación: $c_{\\text{Cu}} = 0{,}385 < c_x = 0{,}388 < c_{\\text{Al}} = 0{,}903\\ \\text{J/(g}\\cdot^\\circ\\text{C)}$."))
    L.paso("H) Mezcla de tres sustancias con etanol")\
        .di("Añadimos setenta gramos de etanol a cuarenta grados al balance. Sumamos los tres productos masa por calor específico por temperatura inicial y dividimos entre la suma de capacidades.",
            M(r"m_1 c_1(T_f - 200) + m_2 c_2(T_f - 20) + m_3 c_3(T_f - 40) = 0"),
            M(rf"T_f = \frac{{(50)(0{{,}}388)(200) + (50)(4{{,}}184)(20) + (70)(2{{,}}46)(40)}}{{(50)(0{{,}}388) + (50)(4{{,}}184) + (70)(2{{,}}46)}}"),
            M(rf"T_f = \frac{{3880 + 4184 + 6888}}{{19{{,}}4 + 209{{,}}2 + 172{{,}}2}} = \frac{{{tex(num_h, 5)}}}{{{tex(den_h, 4)}}} = {tex(tf_h, 4)}\ {C}"),
            R(rf"T_{{f,\text{{H}}}} = {tex(tf_h, 4)}\ {C}"))
    L.usa(F_QS)
    L.responde("AB", rf"T_f = {tex(tf_ab, 3)}\ {C}", tf_ab, "°C")
    L.responde("CD", rf"T_f = {tex(tf_cd, 4)}\ {C}", tf_cd, "°C")
    L.responde("E", rf"m_{{\text{{Cu}}}} = {tex(m_cu_e, 4)}\ \text{{g}}", m_cu_e, "g")
    L.responde("G", rf"c_x = {tex(c_unk_q, 3)}\ \text{{J/(g}}\cdot {C}\text{{)}}", c_unk, "J/(g·°C)")
    L.responde("H", rf"T_f = {tex(tf_h, 4)}\ {C}", tf_h, "°C")
    return L.oficial("Tarea 3 I-2025", 35.0, 35.17, 209.0, 0.388, 37.30)


# ============================================================================ 7. T3-25II-1
@leccion("T3-25II-1")
def t3_25ii_1() -> Leccion:
    m_al, m_a = 0.1000, 1.000
    l_al, c_al, c_a = 396.0, 0.896, 4.186  # kJ/kg y kJ/(kg·K)
    t_fus = 659.0
    q_al1 = -m_al * l_al                   # -39.6 kJ
    q_al2 = m_al * c_al * (100.0 - t_fus)  # -50.0864 kJ
    q_al_tot = q_al1 + q_al2               # -89.6864 kJ
    q_a_100 = m_a * c_a * (100.0 - 22.0)   # 326.508 kJ
    num_eq = -q_al1 + m_al * c_al * t_fus + m_a * c_a * 22.0  # 190.738 kJ
    den_eq = m_al * c_al + m_a * c_a                           # 4.2756 kJ/°C
    t_eq = num_eq / den_eq                 # 44.611 °C
    m_min = (-q_al_tot) / (c_a * 78.0)     # 0.27468 kg

    L = Leccion(
        "T3-25II-1",
        "Aluminio líquido en agua: prueba de ebullición y masa mínima (Tarea 3 II-2025, P1)",
        tema="4.8", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("25-II-T3", "Tarea 3 II-2025", "1"),
        enunciado=(
            "Se vierten $100{,}0\\ \\text{g}$ de aluminio líquido a su temperatura de fusión ($659\\,^\\circ\\text{C}$) "
            "dentro de $1{,}000\\ \\text{L}$ ($1{,}000\\ \\text{kg}$) de agua a $22\\,^\\circ\\text{C}$. "
            "Considere $L_{\\text{Al},f} = 396\\ \\text{kJ/kg}$, $L_{a,v} = 2257\\ \\text{kJ/kg}$, "
            "$c_{\\text{Al}} = 0{,}896\\ \\text{kJ/(kg}\\cdot\\text{K)}$ y $c_a = 4{,}186\\ \\text{kJ/(kg}\\cdot\\text{K)}$. "
            "Determine: A) cuánta agua hierve y cuánto aluminio se solidifica; B) la temperatura final del sistema; "
            "C) la masa mínima de agua para que el equilibrio quede en $100\\,^\\circ\\text{C}$."
        ),
        claves=["calorimetría", "solidificación", "aluminio fundido", "ebullición", "umbral", "tarea"],
    )
    L.paso("A) Energía máxima cedida por el aluminio hasta 100 °C")\
        .di("Calculamos todo el calor que suelta el aluminio al solidificarse y bajar hasta cien grados, usando kilojoules en todo el problema.",
            DATO("100,0 g de aluminio líquido"),
            F(F_QL), F(F_QS),
            M(rf"Q_{{\text{{Al}},1}} = -m_{{\text{{Al}}}} L_{{\text{{Al}},f}} = -(0{{,}}100)(396) = {tex(q_al1, 3)}\ \text{{kJ}}"),
            M(rf"Q_{{\text{{Al}},2}} = (0{{,}}100)(0{{,}}896)(100 - 659) = {tex(q_al2, 4)}\ \text{{kJ}}"),
            M(rf"Q_{{\text{{Al,cedido}}}} = -39{{,}}6 - 50{{,}}09 = {tex(q_al_tot, 4)}\ \text{{kJ}}"))
    L.paso("Comparar con el calor para llevar el agua a 100 °C")\
        .di("Ahora calculamos cuánto calor necesitaría el kilogramo de agua solo para llegar a cien grados sin hervir todavía.",
            M(rf"Q_{{\text{{agua}}\to 100}} = (1{{,}}000)(4{{,}}186)(100 - 22) = {tex(q_a_100, 4)}\ \text{{kJ}}"))\
        .di("Ochenta y nueve coma siete kilojoules no alcanzan ni de lejos para trescientos veintiséis coma cinco: no hierve ni una gota y todo el aluminio solidifica.",
            R(r"m_{\text{evap}} = 0\ \text{g},\qquad m_{\text{Al,sólido}} = 100{,}0\ \text{g}"))
    L.paso("B) Temperatura final de equilibrio")\
        .di("Como el agua sigue líquida y el aluminio se solidifica por completo, planteamos la suma de calores igual a cero y despejamos la temperatura de equilibrio.",
            M(r"-m_{\text{Al}}L_{\text{Al},f} + m_{\text{Al}}c_{\text{Al}}(T_{\text{eq}} - 659) + m_a c_a(T_{\text{eq}} - 22) = 0"),
            M(r"-39{,}6 + 0{,}0896(T_{\text{eq}} - 659) + 4{,}186(T_{\text{eq}} - 22) = 0"),
            M(rf"{tex(den_eq, 4)}\,T_{{\text{{eq}}}} = 39{{,}}6 + 59{{,}}05 + 92{{,}}09 = {tex(num_eq, 4)}"),
            M(rf"T_{{\text{{eq}}}} = \frac{{{tex(num_eq, 4)}}}{{{tex(den_eq, 4)}}} = {tex(t_eq, 4)}\ {C}"),
            R(rf"T_{{\text{{eq}}}} = {tex(t_eq, 3)}\ {C}"))
    L.paso("C) Masa mínima de agua para llegar a 100 °C")\
        .di("Para que el aluminio solidifique y el equilibrio quede justo en cien grados, esa masa de agua debe absorber exactamente los ochenta y nueve coma sesenta y nueve kilojoules cedidos.",
            M(r"-89{,}69 + m_{\text{mín}}(4{,}186)(100 - 22) = 0"),
            M(rf"m_{{\text{{mín}}}} = \frac{{89{{,}}69}}{{(4{{,}}186)(78)}} = \frac{{89{{,}}69}}{{326{{,}}5}} = {tex(m_min, 4)}\ \text{{kg}}"),
            R(rf"m_{{\text{{mín}}}} = {tex(m_min, 3)}\ \text{{kg}}"))
    L.usa(F_QS, F_QL)
    L.responde("B", rf"T_{{\text{{eq}}}} = {tex(t_eq, 3)}\ {C}", t_eq, "°C")
    L.responde("C", rf"m_{{\text{{mín}}}} = {tex(m_min, 3)}\ \text{{kg}}", m_min, "kg")
    return L.oficial("Tarea 3 II-2025", 44.6, 0.275)


# ============================================================================ 8. T3-25II-2
@leccion("T3-25II-2")
def t3_25ii_2() -> Leccion:
    l_m, r1, r2, rin = 1.000, 0.0100, 0.0300, 0.0200
    k_c, t1, t2 = 1.000, 150.0, 30.0
    ln3 = math.log(r2 / r1)
    h_val = 2.0 * math.pi * k_c * l_m * (t1 - t2) / ln3   # 686.30 W
    t_in = t1 - (h_val / (2.0 * math.pi * k_c * l_m)) * math.log(rin / r1)  # 74.29 °C

    L = Leccion(
        "T3-25II-2",
        "Ducto cilíndrico de cerámica y perfil logarítmico T(r) (Tarea 3 II-2025, P2)",
        tema="5.4", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("25-II-T3", "Tarea 3 II-2025", "2"),
        enunciado=(
            "Un ducto tubular cerámico de longitud $L = 1{,}000\\ \\text{m}$, radio interior $r_1 = 0{,}0100\\ \\text{m}$, "
            "radio exterior $r_2 = 0{,}0300\\ \\text{m}$ y conductividad $k = 1{,}000\\ \\text{W/(m}\\cdot\\text{K)}$ "
            "conduce calor radialmente en régimen estacionario entre $T_1 = 150{,}0\\,^\\circ\\text{C}$ y $T_2 = 30{,}0\\,^\\circ\\text{C}$. "
            "A) Deduzca $H$ partiendo de Fourier diferencial y calcule su valor. "
            "B) Calcule la temperatura en el radio intermedio $r_{\\text{in}} = 0{,}020\\ \\text{m}$ y compárela con el promedio."
        ),
        claves=["cilindro hueco", "Fourier", "integral", "perfil logarítmico", "conducción radial", "tarea"],
    )
    L.paso("A) Derivar el flujo cilíndrico desde Fourier")\
        .di("Partimos de la ley diferencial de Fourier del formulario. El área que atraviesa el calor a radio erre es la cara lateral de un cilindro: dos pi erre por ele.",
            DATO("conduce calor radialmente"),
            F(F_FOUR_R),
            M(r"A(r) = 2\pi r L \;\Rightarrow\; H = -k(2\pi r L)\frac{dT}{dr}"))\
        .di("Separamos variables e integramos desde el radio interno hasta el externo.",
            M(r"\frac{H}{2\pi k L}\int_{r_1}^{r_2}\frac{dr}{r} = -\int_{T_1}^{T_2} dT"),
            M(r"\frac{H}{2\pi k L}\ln\left(\frac{r_2}{r_1}\right) = T_1 - T_2 \;\Rightarrow\; H = \frac{2\pi k L(T_1 - T_2)}{\ln(r_2/r_1)}"))
    L.paso("Sustituir para hallar H")\
        .di("Sustituimos conductividad, longitud, salto de ciento veinte grados y el logaritmo natural de tres.",
            M(rf"H = \frac{{2\pi(1{{,}}000)(1{{,}}000)(150{{,}}0 - 30{{,}}0)}}{{\ln(0{{,}}0300/0{{,}}0100)}} = \frac{{754{{,}}0}}{{{tex(ln3, 4)}}} = {tex(h_val, 4)}\ \text{{W}}"),
            R(rf"H = {tex(h_val, 4)}\ \text{{W}}"))
    L.paso("B) Temperatura en el punto medio r = 0,020 m")\
        .di("Como el flujo hache es idéntico en toda capa cilíndrica, integramos de cero coma cero uno a cero coma cero dos metros y despejamos la temperatura intermedia.",
            M(r"T_{\text{in}} = T_1 - \frac{H}{2\pi k L}\ln\left(\frac{r_{\text{in}}}{r_1}\right)"),
            M(rf"T_{{\text{{in}}}} = 150{{,}}0 - \frac{{{tex(h_val, 4)}}}{{2\pi(1{{,}}000)(1{{,}}000)}}\ln(2) = 150{{,}}0 - 75{{,}}71 = {tex(t_in, 3)}\ {C}"),
            R(rf"T(0{{,}}020\ \text{{m}}) = {tex(t_in, 3)}\ {C}"))\
        .di("El promedio lineal sería noventa grados, pero aquí da setenta y cuatro coma tres: como el área crece con el radio, la temperatura cae más empinada cerca del centro.",
            NOTA("No coincide con $T_{\\text{prom}} = \\frac{150 + 30}{2} = 90{,}0\\,^\\circ\\text{C}$ porque el perfil radial es logarítmico, no lineal."))
    L.usa(F_FOUR_R)
    L.responde("A", rf"H = {tex(h_val, 4)}\ \text{{W}}", h_val, "W")
    L.responde("B", rf"T_{{\text{{in}}}} = {tex(t_in, 3)}\ {C}", t_in, "°C")
    return L.oficial("Tarea 3 II-2025", 686.3, 74.3)


# ============================================================================ 9. T3-25II-3
@leccion("T3-25II-3")
def t3_25ii_3() -> Leccion:
    q_acb, w_acb = 90.0, 60.0
    du_ab = q_acb - w_acb    # +30.0 J
    w_adb = 15.0
    q_adb = du_ab + w_adb    # +45.0 J
    w_ba = -35.0
    du_ba = -du_ab           # -30.0 J
    q_ba = du_ba + w_ba      # -65.0 J

    L = Leccion(
        "T3-25II-3",
        "Primera ley e independencia de trayectoria entre estados a y b (Tarea 3 II-2025, P3)",
        tema="6.8", tipo="desarrollo", nivel=2, parcial=3,
        fuente=_f("25-II-T3", "Tarea 3 II-2025", "3"),
        enunciado=(
            "En un diagrama $p$–$V$ entre los estados $a$ y $b$: en la trayectoria $acb$ entran $Q_{acb} = 90{,}0\\ \\text{J}$ "
            "y el sistema realiza $W_{acb} = 60{,}0\\ \\text{J}$; en la trayectoria $adb$ el trabajo realizado es "
            "$W_{adb} = 15{,}0\\ \\text{J}$; en el retorno $b \\to a$ (compresión) el valor absoluto del trabajo es $|W_{ba}| = 35{,}0\\ \\text{J}$. "
            "Determine: A) $\\Delta U_{a\\to b}$ y si sube o baja la temperatura; B) $Q_{adb}$ y por qué depende de la trayectoria; "
            "C) el signo de $W_{ba}$, el valor de $Q_{ba}$ y si absorbe o libera calor."
        ),
        claves=["primera ley", "función de estado", "trayectoria", "compresión", "tarea"],
    )
    L.paso("A) Variación de energía interna por acb")\
        .di("Aplicamos la primera ley del formulario al camino a ce be. Como entran noventa joules y salen sesenta en trabajo, la energía interna sube treinta joules.",
            DATO("Q = 90,0 J"),
            F(F_1LEY),
            M(rf"\Delta U_{{a\to b}} = Q_{{acb}} - W_{{acb}} = 90{{,}}0 - 60{{,}}0 = +{tex(du_ab, 3)}\ \text{{J}}"),
            R(r"\Delta U_{a\to b} = +30{,}0\ \text{J}"))\
        .di("Para un gas ideal la energía interna solo depende de la temperatura, así que si delta U es positiva, la temperatura aumenta.",
            F(F_DU))
    L.paso("B) Calor en el camino adb")\
        .di("La energía interna es una función de estado: depende solo de los extremos a y be, nunca del camino. Por eso en a de be vale los mismos treinta joules.",
            M(r"\Delta U_{adb} = \Delta U_{a\to b} = +30{,}0\ \text{J}"),
            M(rf"Q_{{adb}} = \Delta U_{{adb}} + W_{{adb}} = 30{{,}}0 + 15{{,}}0 = +{tex(q_adb, 3)}\ \text{{J}}"),
            R(r"Q_{adb} = +45{,}0\ \text{J}"))\
        .di("Aunque el punto de partida y llegada son idénticos, el trabajo bajo cada curva es distinto, y por eso el calor cambia de noventa a cuarenta y cinco joules.",
            NOTA("Tanto $Q$ como $W$ dependen de la trayectoria; únicamente su diferencia $\\Delta U = Q - W$ es independiente del camino."))
    L.paso("C) Retorno b → a por compresión")\
        .di("El retorno de be hacia a reduce el volumen, así que es una compresión y el trabajo lleva signo negativo. Además el cambio de energía interna cambia de signo.",
            M(r"W_{ba} = -35{,}0\ \text{J}"),
            M(r"\Delta U_{b\to a} = -\Delta U_{a\to b} = -30{,}0\ \text{J}"),
            M(rf"Q_{{ba}} = \Delta U_{{b\to a}} + W_{{ba}} = -30{{,}}0 + (-35{{,}}0) = {tex(q_ba, 3)}\ \text{{J}}"),
            R(r"W_{ba} = -35{,}0\ \text{J},\qquad Q_{ba} = -65{,}0\ \text{J}\ \text{(libera calor)}"))
    L.usa(F_1LEY, F_DU)
    L.responde("A", r"\Delta U_{a\to b} = +30{,}0\ \text{J}", du_ab, "J")
    L.responde("B", r"Q_{adb} = +45{,}0\ \text{J}", q_adb, "J")
    L.responde("C", r"Q_{ba} = -65{,}0\ \text{J}", q_ba, "J")
    return L.oficial("Tarea 3 II-2025", 30.0, 45.0, -65.0)


# ============================================================================ 10. T3-25II-4
@leccion("T3-25II-4")
def t3_25ii_4() -> Leccion:
    g = Gas(0.500, "mono")
    pA, TA = 1.00e5, 300.0
    g.pone("A", p=pA, t=TA)
    VA = g.v("A")
    VB = 2.0 * VA
    g.pone("B", p=pA, v=VB)
    TB = g.t("B")  # 600 K
    TC = TB
    VC = VA * (TA / TC) ** (1.0 / (g.g - 1.0))  # 4.4092e-3 m^3
    g.pone("C", v=VC, t=TC)
    pC = g.p("C")  # 565.69 kPa

    rAB = g.proceso("A", "B", "isobarico")
    rBC = g.proceso("B", "C", "isotermico")
    rCA = g.proceso("C", "A", "adiabatico")
    w_neto = rAB["W"] + rBC["W"] + rCA["W"]     # -1204.37 J
    q_neto = rAB["Q"] + rBC["Q"] + rCA["Q"]     # -1204.37 J
    k_ref = abs(rAB["Q"]) / abs(w_neto)         # 2.5887

    procs = [("A", "B", "isobarico"), ("B", "C", "isotermico"), ("C", "A", "adiabatico")]
    L = Leccion(
        "T3-25II-4",
        "Ciclo isobárico-isotérmico-adiabático (refrigerador monoatómico) (Tarea 3 II-2025, P4)",
        tema="6.11", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("25-II-T3", "Tarea 3 II-2025", "4"),
        enunciado=(
            "Un gas ideal monoatómico ($n = 0{,}500\\ \\text{mol}$) describe el ciclo $A \\to B \\to C \\to A$: "
            "$A \\to B$ expansión isobárica desde $p_A = 100\\ \\text{kPa}$, $T_A = 300\\ \\text{K}$ hasta $V_B = 2V_A$; "
            "$B \\to C$ compresión isotérmica; $C \\to A$ expansión adiabática que regresa a $A$. "
            "A) Dibuje el diagrama $p$–$V$. B) Halle $p, V, T$ en $A, B, C$. C) Calcule $Q, W, \\Delta U$ por etapa. "
            "D) Halle los totales del ciclo. E) Indique si es motor o refrigerador y calcule su $e$ o $K$."
        ),
        visual=visual_pv(g, procs, uv="L", up="kPa", fv=1000.0, fp=1e-3),
        aviso=(
            "Errata física en la solución oficial (inciso E): usó |Q_BC| = 4322,1 J (calor expulsado al foco caliente, Q_BC < 0) "
            "como si fuera el calor absorbido del foco frío, obteniendo K = 3,59 (que es el COP de bomba de calor K + 1). "
            "El calor que entra al refrigerador es Q_AB = +3117,8 J, por lo que K = |Q_C|/|W| = 2,59. "
            "También hubo un error de exponente en su tabla (4,4092×10⁻² en vez de 10⁻³ m³)."
        ),
        claves=["ciclo", "refrigerador", "monoatómico", "adiabática", "isotérmica", "COP", "tarea"],
    )
    L.paso("A–B) Constantes del gas y estados A y B")\
        .di("Para un gas ideal monoatómico tomamos ce ve, ce pe y gama del formulario, y hallamos el volumen en A con la ley del gas ideal.",
            DATO("0,500 mol"),
            F(F_CV_M), F(F_MAYER), F(F_GAM), F(F_GAS),
            M(rf"C_P = \frac{{5}}{{2}}R,\qquad \gamma = \frac{{5}}{{3}}"),
            M(rf"V_A = \frac{{nRT_A}}{{p_A}} = \frac{{(0{{,}}500)(8{{,}}314)(300)}}{{1{{,}}00\times 10^5}} = {tex(VA, 5)}\ \text{{m}}^3"))\
        .di("Como A hacia B es isobárica y duplica el volumen, la temperatura también se duplica a seiscientos kelvin.",
            V(foco="AB"),
            M(rf"V_B = 2V_A = {tex(VB, 5)}\ \text{{m}}^3,\qquad T_B = 2T_A = 600\ \text{{K}}"))
    L.paso("B) Estado C usando la adiabática C → A")\
        .di("De B a C es isotérmica, así que te C vale seiscientos kelvin. Como C a A es adiabática, usamos la casilla te por ve a la gama menos uno constante.",
            V(foco="CA"),
            F(F_TVG),
            M(r"T_C V_C^{\gamma - 1} = T_A V_A^{\gamma - 1} \;\Rightarrow\; V_C = V_A\left(\frac{T_A}{T_C}\right)^{\frac{1}{\gamma - 1}}"),
            M(rf"V_C = ({tex(VA, 5)})\left(\frac{{300}}{{600}}\right)^{{3/2}} = {tex(VC, 5)}\ \text{{m}}^3"),
            M(rf"p_C = \frac{{nRT_C}}{{V_C}} = \frac{{(0{{,}}500)(8{{,}}314)(600)}}{{{tex(VC, 5)}}} = {tex(pC / 1e3, 4)}\ \text{{kPa}}"),
            tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1000.0))
    L.paso("C) Trabajo, ΔU y calor por etapa")\
        .di("Calculamos la etapa isobárica A hacia B con trabajo de rectángulo, cambio de energía interna y primera ley.",
            V(foco="AB"), F(F_W_ISO), F(F_DU), F(F_1LEY),
            M(rf"W_{{AB}} = (1{{,}}00\times 10^5)({tex(VB - VA, 4)}) = +{tex(rAB['W'], 5)}\ \text{{J}}"),
            M(rf"\Delta U_{{AB}} = (0{{,}}500)\left(\frac{{3}}{{2}}\right)(8{{,}}314)(300) = +{tex(rAB['U'], 5)}\ \text{{J}}"),
            M(rf"Q_{{AB}} = 1871 + 1247 = +{tex(rAB['Q'], 5)}\ \text{{J}}"))\
        .di("En la compresión isotérmica B hacia C delta U es cero; en la expansión adiabática C hacia A el calor es cero.",
            V(foco="BC"), F(F_W_T),
            M(rf"W_{{BC}} = Q_{{BC}} = (0{{,}}500)(8{{,}}314)(600)\ln\left(\frac{{{tex(VC, 5)}}}{{{tex(VB, 5)}}}\right) = {tex(rBC['W'], 5)}\ \text{{J}}"),
            M(rf"Q_{{CA}} = 0,\quad \Delta U_{{CA}} = -{tex(abs(rCA['U']), 5)}\ \text{{J}},\quad W_{{CA}} = +{tex(rCA['W'], 5)}\ \text{{J}}"))
    L.paso("D) Totales del ciclo")\
        .di("Sumamos las tres etapas. La variación total de energía interna da exactamente cero y el calor neto coincide con el trabajo neto negativo.",
            V(foco="ciclo"),
            tabla_quw([("A→B", rAB), ("B→C", rBC), ("C→A", rCA)], d=1),
            M(rf"W_{{\text{{neto}}}} = 1247{{,}}1 - 4322{{,}}1 + 1870{{,}}7 = {tex(w_neto, 5)}\ \text{{J}}"),
            R(rf"W_{{\text{{neto}}}} = Q_{{\text{{neto}}}} = {tex(w_neto, 5)}\ \text{{J}},\qquad \Delta U_{{\text{{ciclo}}}} = 0"))
    L.paso("E) Refrigerador y rendimiento K correcto")\
        .di("Como el trabajo neto es negativo y el recorrido es antihorario, el ciclo opera como refrigerador. El calor absorbido del foco frío es el único positivo: Q A B.",
            F(F_REF),
            M(rf"|Q_c| = Q_{{AB}} = {tex(rAB['Q'], 5)}\ \text{{J}},\qquad |Q_H| = |Q_{{BC}}| = {tex(abs(rBC['Q']), 5)}\ \text{{J}}"),
            M(rf"K = \frac{{|Q_c|}}{{|W|}} = \frac{{{tex(rAB['Q'], 5)}}}{{{tex(abs(w_neto), 5)}}} = {tex(k_ref, 3)}"),
            R(rf"\text{{Refrigerador:}}\quad K = {tex(k_ref, 3)}"),
            OJO("Comprobación cruzada: |Q_H|/|W| = 4322,1/1204,4 = 3,59 = K + 1 (bomba de calor, no refrigerador)."))
    L.usa(F_CV_M, F_MAYER, F_GAM, F_GAS, F_TVG, F_W_ISO, F_W_T, F_DU, F_1LEY, F_REF)
    L.responde("B-pC", rf"p_C = {tex(pC / 1e3, 4)}\ \text{{kPa}}", pC / 1e3, "kPa")
    L.responde("D-W", rf"W_{{\text{{neto}}}} = {tex(w_neto, 5)}\ \text{{J}}", w_neto, "J")
    L.responde("E-K", rf"K = {tex(k_ref, 3)}", k_ref, "")
    return L.oficial(
        "Tarea 3 II-2025", 565.7, -1204.4, 3.59, tol=0.015,
        nota="Inciso E oficial tomó |Q_BC| (calor cedido al foco caliente) y obtuvo K_bomba = 3,59; el K real de refrigerador es |Q_AB|/|W| = 2,59.",
    )


# ============================================================================ 11. T3-26I-1
@leccion("T3-26I-1")
def t3_26i_1() -> Leccion:
    m_al, m_h, m_a = 0.300, 0.200, 0.100
    c_h, c_a, c_al, lf = 2100.0, 4190.0, 910.0, 334e3
    q_h1 = m_h * c_h * 10.0      # 4200 J
    q_al1 = m_al * c_al * 10.0   # 2730 J
    q_h2 = m_h * lf              # 66800 J
    q_sub0 = q_h1 + q_al1        # 6930 J
    q_tot_f = q_sub0 + q_h2      # 73730 J
    q_a1 = m_a * c_a * (-50.0)   # -20950 J
    q_sobra = abs(q_a1) - q_sub0 # 14020 J
    m_funde = q_sobra / lf       # 0.041976 kg
    m_a_tot = m_a + m_funde      # 0.14198 kg
    m_h_tot = m_h - m_funde      # 0.15802 kg
    q_h3 = m_h * c_a * 25.0      # 20950 J
    q_al2 = m_al * c_al * 25.0   # 6825 J
    q_req_25 = q_tot_f + q_h3 + q_al2  # 101505 J
    m_a_25 = q_req_25 / (c_a * 25.0)   # 0.96902 kg

    L = Leccion(
        "T3-26I-1",
        "Hielo y calorímetro de aluminio a −10 °C con agua a 50 °C (Tarea 3 I-2026, P1)",
        tema="4.8", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "1"),
        enunciado=(
            "Un calorímetro aislado de aluminio ($300{,}0\\ \\text{g}$) contiene $200{,}0\\ \\text{g}$ de hielo en equilibrio "
            "a $-10{,}0\\,^\\circ\\text{C}$. Se agregan $100{,}0\\ \\text{g}$ de agua a $50{,}0\\,^\\circ\\text{C}$ "
            "($c_h = 2100$, $c_a = 4190$, $c_{\\text{Al}} = 910\\ \\text{J/(kg}\\cdot\\text{K)}$, $L_f = 334\\times 10^3\\ \\text{J/kg}$). "
            "A) Halle la temperatura de equilibrio. B) Determine las masas finales de agua y hielo. "
            "C) ¿Cuánta agua a $50{,}0\\,^\\circ\\text{C}$ se requiere para que el equilibrio sea $25{,}0\\,^\\circ\\text{C}$?"
        ),
        claves=["calorimetría", "fusión parcial", "hielo", "aluminio", "equilibrio", "tarea"],
    )
    L.paso("A) Comprobar si el agua logra fundir el hielo")\
        .di("Calculamos la energía para llevar hielo y aluminio hasta cero grados, y aparte la energía para fundir todo el hielo.",
            DATO("-10,0 °C"),
            F(F_QS), F(F_QL),
            M(rf"Q_{{h1}} = (0{{,}}200)(2100)(10) = {tex(q_h1, 4)}\ \text{{J}}"),
            M(rf"Q_{{\text{{Al}}1}} = (0{{,}}300)(910)(10) = {tex(q_al1, 4)}\ \text{{J}}"),
            M(rf"Q_{{h2}} = (0{{,}}200)(334\times 10^3) = {tex(q_h2, 4)}\ \text{{J}}"))\
        .di("El agua al enfriarse hasta cero entrega veinte mil novecientos cincuenta joules: alcanza para llegar a cero pero no para fundir todo el hielo.",
            M(rf"|Q_{{a1}}| = (0{{,}}100)(4190)(50) = {tex(abs(q_a1), 5)}\ \text{{J}}"),
            M(r"6930\ \text{J} < 20\,950\ \text{J} < 73\,730\ \text{J}"),
            R(rf"T_{{\text{{eq}}}} = 0{{,}}0\ {C}"))
    L.paso("B) Masa fundida y composición final")\
        .di("El excedente que sobra tras llevar hielo y aluminio a cero grados se dedica a fundir una fracción del hielo.",
            M(rf"Q_{{\text{{funde}}}} = 20\,950 - (4200 + 2730) = {tex(q_sobra, 5)}\ \text{{J}}"),
            M(rf"m_{{\text{{funde}}}} = \frac{{{tex(q_sobra, 5)}}}{{334\times 10^3}} = {tex(m_funde, 3)}\ \text{{kg}}"))\
        .di("Sumamos la masa fundida al agua inicial y la restamos del hielo inicial.",
            M(rf"m_{{\text{{agua}}}} = 0{{,}}100 + 0{{,}}0420 = {tex(m_a_tot, 4)}\ \text{{kg}}"),
            M(rf"m_{{\text{{hielo}}}} = 0{{,}}200 - 0{{,}}0420 = {tex(m_h_tot, 4)}\ \text{{kg}}"),
            R(rf"m_{{\text{{agua}}}} = {tex(m_a_tot, 4)}\ \text{{kg}},\qquad m_{{\text{{hielo}}}} = {tex(m_h_tot, 4)}\ \text{{kg}}"))
    L.paso("C) Masa de agua a 50 °C para equilibrar en 25 °C")\
        .di("Sumamos todo el calor necesario para fundir el hielo y calentar esa agua derretida junto con el aluminio hasta veinticinco grados.",
            M(rf"Q_{{h3}} = (0{{,}}200)(4190)(25) = {tex(q_h3, 5)}\ \text{{J}}"),
            M(rf"Q_{{\text{{Al}}2}} = (0{{,}}300)(910)(25) = {tex(q_al2, 4)}\ \text{{J}}"),
            M(rf"Q_{{\text{{req}}}} = 73\,730 + 20\,950 + 6825 = {tex(q_req_25, 6)}\ \text{{J}}"))\
        .di("Ese calor debe cederlo el agua nueva al bajar de cincuenta a veinticinco grados.",
            M(rf"m_a = \frac{{{tex(q_req_25, 6)}}}{{(4190)(50 - 25)}} = \frac{{101\,505}}{{104\,750}} = {tex(m_a_25, 4)}\ \text{{kg}}"),
            R(rf"m_a = {tex(m_a_25, 3)}\ \text{{kg}}"),
            NOTA("Nota: el solucionario imprimió la errata tipográfica 674930 J en un renglón, pero en la suma siguiente usó 73730 J correctamente."))
    L.usa(F_QS, F_QL)
    L.responde("A", rf"T_{{\text{{eq}}}} = 0{{,}}0\ {C}", 0.0, "°C")
    L.responde("B-agua", rf"m_{{\text{{agua}}}} = {tex(m_a_tot, 4)}\ \text{{kg}}", m_a_tot, "kg")
    L.responde("B-hielo", rf"m_{{\text{{hielo}}}} = {tex(m_h_tot, 4)}\ \text{{kg}}", m_h_tot, "kg")
    L.responde("C", rf"m_a = {tex(m_a_25, 3)}\ \text{{kg}}", m_a_25, "kg")
    return L.oficial("Tarea 3 I-2026", 0.0, 0.1420, 0.1580, 0.969)


# ============================================================================ 12. T3-26I-2
@leccion("T3-26I-2")
def t3_26i_2() -> Leccion:
    pA, VA, TA = 1.00e5, 2.00e-2, 300.0
    n_mol = pA * VA / (R_GAS * TA)  # 0.80186 mol
    g = Gas(n_mol, "mono")
    g.pone("A", p=pA, v=VA)
    VB = 2.0 * VA
    g.pone("B", v=VB, t=TA)
    pB = g.p("B")  # 5.00e4 Pa
    pC = pA
    VC = VB * (pB / pC) ** (1.0 / g.g)  # 2.6390e-2 m^3
    g.pone("C", p=pC, v=VC)
    TC = g.t("C")  # 395.85 K

    rAB = g.proceso("A", "B", "isotermico")
    rBC = g.proceso("B", "C", "adiabatico")
    rCA = g.proceso("C", "A", "isobarico")
    procs = [("A", "B", "isotermico"), ("B", "C", "adiabatico"), ("C", "A", "isobarico")]

    L = Leccion(
        "T3-26I-2",
        "Ciclo isotérmico-adiabático-isobárico monoatómico (Tarea 3 I-2026, P2)",
        tema="6.11", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "2"),
        enunciado=(
            "Un gas ideal monoatómico parte del estado $A$ ($p_A = 1{,}00\\times 10^5\\ \\text{Pa}$, "
            "$V_A = 2{,}00\\times 10^{-2}\\ \\text{m}^3$, $T_A = 300\\ \\text{K}$). Realiza: "
            "$A \\to B$ expansión isotérmica hasta duplicar su volumen; $B \\to C$ compresión adiabática hasta "
            "$p_C = 1{,}00\\times 10^5\\ \\text{Pa}$; $C \\to A$ compresión isobárica de regreso a $A$. "
            "A) Halle $n$. B) Determine $p, V, T$ en $A, B, C$. C) Construya el diagrama $p$–$V$. "
            "D) Calcule $W, Q$ y $\\Delta U$ en cada etapa."
        ),
        visual=visual_pv(g, procs, uv="L", up="kPa", fv=1000.0, fp=1e-3),
        aviso=(
            "La solución publicada calculó A → B y B → C pero omitió la etapa de cierre C → A en el inciso D. "
            "Aquí se completan las 3 etapas y se comprueba ΔU_ciclo = 0."
        ),
        claves=["ciclo", "monoatómico", "isotérmica", "adiabática", "isobárica", "tarea"],
    )
    L.paso("A) Número de moles del gas")\
        .di("Despejamos los moles directamente de la ley del gas ideal en el estado A con todo en unidades del sistema internacional.",
            DATO("300 K"),
            F(F_GAS),
            M(rf"n = \frac{{p_A V_A}}{{RT_A}} = \frac{{(1{{,}}00\times 10^5)(2{{,}}00\times 10^{{-2}})}}{{(8{{,}}314)(300)}} = \frac{{2000}}{{2494{{,}}2}} = {tex(n_mol, 4)}\ \text{{mol}}"),
            R(rf"n = {tex(n_mol, 3)}\ \text{{mol}}"))
    L.paso("B–C) Estados B y C en el diagrama p–V")\
        .di("En la expansión isotérmica al doble de volumen la presión cae a la mitad. Para la adiabática usamos gama igual a cinco tercios.",
            V(foco="AB"), F(F_CV_M), F(F_MAYER), F(F_GAM), F(F_PVG),
            M(r"V_B = 4{,}00\times 10^{-2}\ \text{m}^3,\quad T_B = 300\ \text{K},\quad p_B = \frac{p_A V_A}{V_B} = 5{,}00\times 10^4\ \text{Pa}"),
            M(rf"V_C = V_B\left(\frac{{p_B}}{{p_C}}\right)^{{3/5}} = (4{{,}}00\times 10^{{-2}})(0{{,}}500)^{{0{{,}}600}} = {tex(VC, 4)}\ \text{{m}}^3"))\
        .di("Con la casilla adiabática de temperatura y volumen hallamos la temperatura en C y completamos la tabla.",
            V(foco="BC"), F(F_TVG),
            M(rf"T_C = T_B\left(\frac{{V_B}}{{V_C}}\right)^{{2/3}} = 300\left(\frac{{4{{,}}00}}{{2{{,}}639}}\right)^{{2/3}} = {tex(TC, 4)}\ \text{{K}}"),
            tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1000.0))
    L.paso("D) Etapas A → B (isotérmica) y B → C (adiabática)")\
        .di("En la isotérmica delta U es cero y el trabajo iguala al calor. Usamos p A por V A en lugar de n R T para no arrastrar redondeos.",
            V(foco="AB"), F(F_W_T), F(F_1LEY),
            M(rf"\Delta U_{{AB}} = 0,\qquad W_{{AB}} = Q_{{AB}} = (2000)\ln(2) = +{tex(rAB['W'], 4)}\ \text{{J}}"))\
        .di("En la compresión adiabática B hacia C el calor es cero y usamos la fórmula del trabajo adiabático del formulario.",
            V(foco="BC"), F(F_W_AD),
            M(rf"W_{{BC}} = \frac{{p_B V_B - p_C V_C}}{{\gamma - 1}} = \frac{{2000 - 2639}}{{2/3}} = {tex(rBC['W'], 4)}\ \text{{J}}"),
            M(rf"Q_{{BC}} = 0,\qquad \Delta U_{{BC}} = -W_{{BC}} = +{tex(rBC['U'], 4)}\ \text{{J}}"))
    L.paso("D) Etapa de cierre C → A (isobárica) y verificación")\
        .di("Completamos la compresión isobárica C hacia A que faltaba en la solución publicada, para cerrar el ciclo con delta U total igual a cero.",
            V(foco="CA"), F(F_W_ISO), F(F_DU),
            M(rf"W_{{CA}} = p_A(V_A - V_C) = (1{{,}}00\times 10^5)(-6{{,}}39\times 10^{{-3}}) = {tex(rCA['W'], 4)}\ \text{{J}}"),
            M(rf"\Delta U_{{CA}} = -\Delta U_{{BC}} = {tex(rCA['U'], 4)}\ \text{{J}},\qquad Q_{{CA}} = \Delta U_{{CA}} + W_{{CA}} = {tex(rCA['Q'], 4)}\ \text{{J}}"),
            tabla_quw([("A→B", rAB), ("B→C", rBC), ("C→A", rCA)], d=1),
            R(rf"W_{{AB}} = {tex(rAB['W'], 3)}\ \text{{J}},\quad W_{{BC}} = {tex(rBC['W'], 3)}\ \text{{J}},\quad W_{{CA}} = {tex(rCA['W'], 3)}\ \text{{J}}"))
    L.usa(F_GAS, F_CV_M, F_MAYER, F_GAM, F_PVG, F_TVG, F_W_T, F_W_AD, F_W_ISO, F_1LEY, F_DU)
    L.responde("A", rf"n = {tex(n_mol, 3)}\ \text{{mol}}", n_mol, "mol")
    L.responde("B-TC", rf"T_C = {tex(TC, 3)}\ \text{{K}}", TC, "K")
    L.responde("D-WAB", rf"W_{{AB}} = {tex(rAB['W'], 3)}\ \text{{J}}", rAB["W"], "J")
    L.responde("D-WBC", rf"W_{{BC}} = {tex(rBC['W'], 3)}\ \text{{J}}", rBC["W"], "J")
    L.responde("D-QCA", rf"Q_{{CA}} = {tex(rCA['Q'], 4)}\ \text{{J}}", rCA["Q"], "J")
    return L.oficial("Tarea 3 I-2026", 0.802, 396.0, 1390.0, -959.0, None)


# ============================================================================ 13. T3-26I-5
@leccion("T3-26I-5")
def t3_26i_5() -> Leccion:
    n_mol = 0.200
    g = Gas(n_mol, "dia")
    pA, VA = 2.00e5, 1.00e-3
    g.pone("A", p=pA, v=VA)
    TA = g.t("A")       # 120.28 K
    g.pone("B", p=pA, v=2.0 * VA)
    TB = g.t("B")       # 240.56 K
    g.pone("C", p=0.5 * pA, v=2.0 * VA)
    TC = g.t("C")       # 120.28 K
    rAB = g.proceso("A", "B", "isobarico")
    rBC = g.proceso("B", "C", "isocorico")
    w_tot = rAB["W"] + rBC["W"]  # 200.0 J
    q_tot = rAB["Q"] + rBC["Q"]  # 200.0 J
    du_tot = rAB["U"] + rBC["U"] # 0.0 J
    procs = [("A", "B", "isobarico"), ("B", "C", "isocorico")]

    L = Leccion(
        "T3-26I-5",
        "Expansión isobárica y enfriamiento isocórico de gas diatómico (Tarea 3 I-2026, P5)",
        tema="6.8", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "5"),
        enunciado=(
            "Un cilindro contiene $0{,}200\\ \\text{mol}$ de gas ideal diatómico inicialmente en $A$ con "
            "$p_A = 200{,}0\\ \\text{kPa}$ y $V_A = 1{,}00\\times 10^{-3}\\ \\text{m}^3$. Realiza: "
            "$A \\to B$ expansión isobárica hasta $V_B = 2V_A$, y $B \\to C$ enfriamiento isocórico hasta $p_C = p_B/2$. "
            "Calcule: A) $T_A$ y $T_B$; B) el trabajo total; C) el calor total e indique si absorbe o libera calor; D) $\\Delta U_{A\\to C}$."
        ),
        visual=visual_pv(g, procs, uv="L", up="kPa", fv=1000.0, fp=1e-3),
        claves=["gas diatómico", "isobárico", "isocórico", "primera ley", "tarea"],
    )
    L.paso("A) Temperaturas en A y B")\
        .di("Despejamos la temperatura en A desde la ley del gas ideal. Como la etapa A hacia B mantiene presión constante y duplica volumen, la temperatura también se duplica.",
            DATO("0,200 mol"),
            F(F_GAS),
            M(rf"T_A = \frac{{p_A V_A}}{{nR}} = \frac{{(2{{,}}00\times 10^5)(1{{,}}00\times 10^{{-3}})}}{{(0{{,}}200)(8{{,}}314)}} = \frac{{200}}{{1{{,}}6628}} = {tex(TA, 4)}\ \text{{K}}"),
            M(rf"T_B = 2T_A = {tex(TB, 4)}\ \text{{K}}"),
            R(rf"T_A = {tex(TA, 4)}\ \text{{K}},\qquad T_B = {tex(TB, 4)}\ \text{{K}}"))
    L.paso("B) Trabajo total del recorrido A → B → C")\
        .di("En la expansión isobárica el trabajo es presión por cambio de volumen; en la etapa isocórica el volumen no cambia y el trabajo es cero.",
            V(foco="AB"), F(F_W_ISO),
            M(r"W_{AB} = p_A(V_B - V_A) = (2{,}00\times 10^5)(1{,}00\times 10^{-3}) = +200\ \text{J}"),
            M(r"W_{BC} = 0 \;\Rightarrow\; W_{\text{total}} = 200 + 0 = 200\ \text{J}"),
            R(r"W_{\text{total}} = +200\ \text{J}"))
    L.paso("C) Calor total en ambas etapas")\
        .di("Para un gas diatómico tomamos ce ve y ce pe del formulario. Como en C la presión baja a la mitad a volumen constante, la temperatura regresa a la de A.",
            V(foco="BC"), F(F_CV_D), F(F_MAYER), F(F_DQP), F(F_DQV),
            M(rf"C_P = \frac{{7}}{{2}}R,\qquad T_C = \frac{{T_B}}{{2}} = {tex(TC, 4)}\ \text{{K}}"),
            M(rf"Q_{{AB}} = nC_P(T_B - T_A) = (0{{,}}200)(3{{,}}5)(8{{,}}314)(120{{,}}28) = +{tex(rAB['Q'], 4)}\ \text{{J}}"),
            M(rf"Q_{{BC}} = nC_V(T_C - T_B) = (0{{,}}200)(2{{,}}5)(8{{,}}314)(-120{{,}}28) = {tex(rBC['Q'], 4)}\ \text{{J}}"))\
        .di("Sumamos ambos calores: da doscientos joules positivos, así que en el balance neto el gas absorbe calor.",
            M(r"Q_{\text{total}} = 700{,}0 - 500{,}0 = +200{,}0\ \text{J}"),
            R(r"Q_{\text{total}} = +200\ \text{J}\ \text{(absorbe calor)}"))
    L.paso("D) Cambio total de energía interna A → C")\
        .di("Como el estado C queda a la misma temperatura que el estado A, la energía interna vuelve a su valor inicial, lo que confirma la primera ley.",
            F(F_DU), F(F_1LEY),
            M(r"\Delta U_{AC} = nC_V(T_C - T_A) = nC_V(0) = 0\ \text{J}"),
            M(r"\Delta U_{AC} = Q_{\text{total}} - W_{\text{total}} = 200 - 200 = 0\ \text{J}"),
            R(r"\Delta U_{AC} = 0\ \text{J}"))
    L.usa(F_GAS, F_W_ISO, F_CV_D, F_MAYER, F_DQP, F_DQV, F_DU, F_1LEY)
    L.responde("A-TA", rf"T_A = {tex(TA, 4)}\ \text{{K}}", TA, "K")
    L.responde("A-TB", rf"T_B = {tex(TB, 4)}\ \text{{K}}", TB, "K")
    L.responde("B", r"W_{\text{total}} = 200\ \text{J}", w_tot, "J")
    L.responde("C", r"Q_{\text{total}} = 200\ \text{J}", q_tot, "J")
    L.responde("D", r"\Delta U_{AC} = 0\ \text{J}", du_tot, "J")
    return L.oficial("Tarea 3 I-2026", 120.3, 240.6, 200.0, 200.0, 0.0)


# ============================================================================ 14. T3-26I-6
@leccion("T3-26I-6")
def t3_26i_6() -> Leccion:
    k_res = 1.500e4
    area = 3.000e-3
    mp, g_acc = 6.000, 9.800
    vi, ti = 5.000e-3, 290.0
    p_atm = 1.000e5
    dy = 0.120

    p_pist = mp * g_acc / area          # 1.96e4 Pa
    pi = p_atm + p_pist                 # 1.196e5 Pa
    n_mol = pi * vi / (R_GAS * ti)      # 0.24802 mol
    ui = 2.5 * pi * vi                  # 1495.0 J
    dv = area * dy                      # 3.60e-4 m^3
    vf = vi + dv                        # 5.36e-3 m^3
    p_res = k_res * dy / area           # 6.00e5 Pa
    pf = pi + p_res                     # 7.196e5 Pa
    tf = pf * vf / (n_mol * R_GAS)      # 1870.5 K
    uf = 2.5 * pf * vf                  # 9642.64 J
    du = uf - ui                        # 8147.64 J
    w_atm = p_atm * dv                  # 36.0 J
    w_p = mp * g_acc * dy               # 7.056 J
    w_r = 0.5 * k_res * dy * dy         # 108.0 J
    w_tot = w_atm + w_p + w_r           # 151.056 J
    q_tot = du + w_tot                  # 8298.7 J

    L = Leccion(
        "T3-26I-6",
        "Expansión de nitrógeno contra pistón con resorte vertical (Tarea 3 I-2026, P6)",
        tema="6.8", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "6"),
        enunciado=(
            "Un cilindro vertical contiene nitrógeno (gas diatómico) bajo un pistón sin fricción de masa $m_p = 6{,}000\\ \\text{kg}$ "
            "y área $A = 3{,}000\\times 10^{-3}\\ \\text{m}^2$, unido a un resorte de $k = 1{,}500\\times 10^4\\ \\text{N/m}$. "
            "Inicialmente $V_i = 5{,}000\\times 10^{-3}\\ \\text{m}^3$, $T_i = 290{,}0\\ \\text{K}$, "
            "$p_{\\text{atm}} = 1{,}000\\times 10^5\\ \\text{Pa}$ y $g = 9{,}800\\ \\text{m/s}^2$. Al calentar el gas, el pistón sube $\\Delta y = 0{,}120\\ \\text{m}$. "
            "Determine: A) $p_i$, $n$ y $U_i$; B) $V_f$, $p_f$ y $T_f$; C) $\\Delta U$; "
            "D) el trabajo contra la atmósfera, el peso y el resorte; E) $Q$; F) por qué $W_r \\neq F\\Delta y$."
        ),
        claves=["pistón con resorte", "nitrógeno", "diatómico", "integral de trabajo", "primera ley", "tarea"],
    )
    L.paso("A) Presión inicial, moles y energía interna inicial")\
        .di("Al inicio el resorte está sin comprimir, así que el gas sostiene la atmósfera más el peso del pistón por unidad de área.",
            DATO("6,000 kg"),
            F(F_GAS), F(F_CV_D),
            M(rf"p_i = p_{{\text{{atm}}}} + \frac{{m_p g}}{{A}} = 1{{,}}000\times 10^5 + \frac{{(6{{,}}000)(9{{,}}800)}}{{3{{,}}000\times 10^{{-3}}}} = {tex(pi, 4)}\ \text{{Pa}}"),
            M(rf"n = \frac{{p_i V_i}}{{RT_i}} = \frac{{(1{{,}}196\times 10^5)(5{{,}}000\times 10^{{-3}})}}{{(8{{,}}314)(290{{,}}0)}} = {tex(n_mol, 4)}\ \text{{mol}}"))\
        .di("Para un gas diatómico ce ve es cinco medios de erre, así que la energía interna inicial es cinco medios de n R T.",
            M(rf"U_i = \frac{{5}}{{2}}nRT_i = \frac{{5}}{{2}}(0{{,}}2480)(8{{,}}314)(290{{,}}0) = {tex(ui, 4)}\ \text{{J}}"),
            R(rf"p_i = {tex(pi, 4)}\ \text{{Pa}},\quad n = {tex(n_mol, 3)}\ \text{{mol}},\quad U_i = {tex(ui, 4)}\ \text{{J}}"))
    L.paso("B) Volumen, presión y temperatura finales")\
        .di("Al subir cero coma doce metros, el volumen crece en A por delta ye y el resorte añade una presión extra igual a ka delta ye entre A.",
            M(rf"\Delta V = A\Delta y = (3{{,}}000\times 10^{{-3}})(0{{,}}120) = {tex(dv, 3)}\ \text{{m}}^3 \;\Rightarrow\; V_f = {tex(vf, 3)}\ \text{{m}}^3"),
            M(rf"p_f = p_i + \frac{{k\Delta y}}{{A}} = 1{{,}}196\times 10^5 + \frac{{(1{{,}}500\times 10^4)(0{{,}}120)}}{{3{{,}}000\times 10^{{-3}}}} = {tex(pf, 4)}\ \text{{Pa}}"),
            M(rf"T_f = \frac{{p_f V_f}}{{nR}} = \frac{{(7{{,}}196\times 10^5)(5{{,}}36\times 10^{{-3}})}}{{(0{{,}}2480)(8{{,}}314)}} = {tex(tf, 4)}\ \text{{K}}"),
            R(rf"V_f = {tex(vf, 3)}\ \text{{m}}^3,\quad p_f = {tex(pf, 4)}\ \text{{Pa}},\quad T_f = {tex(tf, 4)}\ \text{{K}}"))
    L.paso("C) Cambio de energía interna ΔU")\
        .di("Aplicamos la casilla de energía interna del formulario con cinco medios de erre para gas diatómico.",
            F(F_DU),
            M(rf"\Delta U = \frac{{5}}{{2}}nR(T_f - T_i) = 2{{,}}5(0{{,}}2480)(8{{,}}314)(1870{{,}}5 - 290{{,}}0)"),
            M(rf"\Delta U = (5{{,}}155)(1580{{,}}5) = {tex(du, 4)}\ \text{{J}}"),
            R(rf"\Delta U = {tex(du, 4)}\ \text{{J}}"))
    L.paso("D y F) Trabajo total desglosado e integral del resorte")\
        .di("Partimos de la integral del trabajo del formulario. Como la presión crece linealmente con el desplazamiento, el trabajo se separa en tres aportes: atmósfera, peso y resorte.",
            F(F_W_INT), F(F_W_ISO),
            M(rf"W_{{\text{{atm}}}} = p_{{\text{{atm}}}}\Delta V = (1{{,}}000\times 10^5)(3{{,}}60\times 10^{{-4}}) = {tex(w_atm, 3)}\ \text{{J}}"),
            M(rf"W_p = m_p g \Delta y = (6{{,}}000)(9{{,}}800)(0{{,}}120) = {tex(w_p, 4)}\ \text{{J}}"),
            M(rf"W_r = \int_0^{{\Delta y}} kx\,dx = \frac{{1}}{{2}}k(\Delta y)^2 = \frac{{1}}{{2}}(1{{,}}500\times 10^4)(0{{,}}120)^2 = {tex(w_r, 4)}\ \text{{J}}"))\
        .di("La fuerza del resorte arranca en cero y crece linealmente hasta ka delta ye, por eso su integral vale la mitad de fuerza máxima por desplazamiento.",
            M(rf"W = 36{{,}}0 + 7{{,}}056 + 108{{,}}0 = {tex(w_tot, 4)}\ \text{{J}}"),
            R(rf"W = {tex(w_tot, 4)}\ \text{{J}}"))
    L.paso("E) Calor total transferido al gas")\
        .di("Con la primera ley sumamos el aumento de energía interna y el trabajo total realizado por el gas.",
            F(F_1LEY),
            M(rf"Q = \Delta U + W = {tex(du, 4)} + {tex(w_tot, 4)} = {tex(q_tot, 4)}\ \text{{J}}"),
            R(rf"Q = {tex(q_tot, 4)}\ \text{{J}}"))
    L.usa(F_GAS, F_CV_D, F_DU, F_W_INT, F_W_ISO, F_1LEY)
    L.responde("A-pi", rf"p_i = {tex(pi, 4)}\ \text{{Pa}}", pi, "Pa")
    L.responde("A-n", rf"n = {tex(n_mol, 3)}\ \text{{mol}}", n_mol, "mol")
    L.responde("B-Tf", rf"T_f = {tex(tf, 4)}\ \text{{K}}", tf, "K")
    L.responde("C", rf"\Delta U = {tex(du, 4)}\ \text{{J}}", du, "J")
    L.responde("D", rf"W = {tex(w_tot, 4)}\ \text{{J}}", w_tot, "J")
    L.responde("E", rf"Q = {tex(q_tot, 4)}\ \text{{J}}", q_tot, "J")
    return L.oficial("Tarea 3 I-2026", 1.196e5, 0.248, 1870.0, 8144.0, 151.1, 8300.0)


# ============================================================================ 15. T3-26I-7
@leccion("T3-26I-7")
def t3_26i_7() -> Leccion:
    g = Gas(1.00, "mono")
    g.pone("A", p=3.00e5, v=2.00e-3)
    g.pone("B", p=5.00e5, v=5.00e-3)
    g.pone("C", p=5.00e5, v=2.00e-3)
    rAB = g.proceso("A", "B", "recta")
    rBC = g.proceso("B", "C", "isobarico")
    rCA = g.proceso("C", "A", "isocorico")
    w_neto = rAB["W"] + rBC["W"] + rCA["W"]  # -300.0 J
    q_neto = rAB["Q"] + rBC["Q"] + rCA["Q"]  # -300.0 J
    procs = [("A", "B", "recta"), ("B", "C", "isobarico"), ("C", "A", "isocorico")]

    L = Leccion(
        "T3-26I-7",
        "Ciclo triangular antihorario de 1 mol monoatómico (Tarea 3 I-2026, P7)",
        tema="6.11", tipo="desarrollo", nivel=3, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "7"),
        enunciado=(
            "Un mol de gas ideal monoatómico sigue el ciclo $A \\to B \\to C \\to A$: "
            "$A \\to B$ expansión en línea recta desde $(300{,}0\\ \\text{kPa},\\ 2{,}00\\times 10^{-3}\\ \\text{m}^3)$ "
            "hasta $(500{,}0\\ \\text{kPa},\\ 5{,}00\\times 10^{-3}\\ \\text{m}^3)$; $B \\to C$ compresión isobárica hasta "
            "$V_C = 2{,}00\\times 10^{-3}\\ \\text{m}^3$; $C \\to A$ enfriamiento isocórico. "
            "A) Halle $T_A, T_B, T_C$. B) Indique el sentido del ciclo y su significado físico. "
            "C) Calcule $W_{\\text{neto}}$. D) Calcule $Q_{\\text{neto}}$. E) Explique el área encerrada."
        ),
        visual=visual_pv(g, procs, uv="L", up="kPa", fv=1000.0, fp=1e-3),
        aviso=(
            "Nota de redondeo: la solución oficial obtuvo Q_neto = −305 J al arrastrar temperaturas redondeadas "
            "(72,2 K y 120,3 K) en Q_CA. Usando valores exactos, Q_neto = W_neto = −300,0 J exactamente (ΔU_ciclo = 0)."
        ),
        claves=["ciclo triangular", "antihorario", "trapecio", "monoatómico", "tarea"],
    )
    L.paso("A) Temperaturas en A, B y C")\
        .di("Con un mol de gas aplicamos la ley del gas ideal en cada vértice usando pascales y metros cúbicos.",
            DATO("300,0 kPa"),
            F(F_GAS),
            M(rf"T_A = \frac{{(3{{,}}00\times 10^5)(2{{,}}00\times 10^{{-3}})}}{{8{{,}}314}} = {tex(g.t('A'), 4)}\ \text{{K}}"),
            M(rf"T_B = \frac{{(5{{,}}00\times 10^5)(5{{,}}00\times 10^{{-3}})}}{{8{{,}}314}} = {tex(g.t('B'), 4)}\ \text{{K}}"),
            M(rf"T_C = \frac{{(5{{,}}00\times 10^5)(2{{,}}00\times 10^{{-3}})}}{{8{{,}}314}} = {tex(g.t('C'), 4)}\ \text{{K}}"),
            tabla_estados(g, ["A", "B", "C"], up="kPa", uv="L", fp=1e-3, fv=1000.0))
    L.paso("B y E) Sentido antihorario y significado del área")\
        .di("El tramo superior B hacia C va hacia la izquierda (compresión a presión alta) y el inferior A hacia B va a la derecha (expansión a presión menor).",
            V(foco="ciclo"),
            M(r"\text{Recorrido antihorario} \;\Rightarrow\; W_{\text{neto}} = -\text{Área}_{\triangle} < 0"),
            NOTA("Un ciclo antihorario consume trabajo neto del exterior (como un refrigerador); el área encerrada del triángulo mide ese trabajo neto."))
    L.paso("C) Trabajo en cada etapa y trabajo neto")\
        .di("De A a B integramos la recta como área de un trapecio; de B a C usamos trabajo isobárico y de C a A el trabajo es cero.",
            V(foco="AB"), F(F_W_INT), F(F_W_ISO),
            M(r"W_{AB} = \frac{p_A + p_B}{2}(V_B - V_A) = (4{,}00\times 10^5)(3{,}00\times 10^{-3}) = +1200\ \text{J}"),
            M(r"W_{BC} = p_B(V_C - V_B) = (5{,}00\times 10^5)(-3{,}00\times 10^{-3}) = -1500\ \text{J}"),
            M(r"W_{\text{neto}} = 1200 - 1500 + 0 = -300\ \text{J}"),
            R(r"W_{\text{neto}} = -300\ \text{J}"))
    L.paso("D) Calor por etapa y calor neto exacto")\
        .di("Usamos ce ve igual a tres medios de erre y ce pe igual a cinco medios de erre. En cada etapa calculamos el calor sin redondeos prematuros.",
            F(F_CV_M), F(F_MAYER), F(F_DU), F(F_1LEY), F(F_DQP), F(F_DQV),
            M(r"\Delta U_{AB} = \frac{3}{2}(p_B V_B - p_A V_A) = \frac{3}{2}(2500 - 600) = +2850\ \text{J} \;\Rightarrow\; Q_{AB} = 2850 + 1200 = +4050\ \text{J}"),
            M(r"Q_{BC} = \frac{5}{2}(p_C V_C - p_B V_B) = \frac{5}{2}(1000 - 2500) = -3750\ \text{J}"),
            M(r"Q_{CA} = \frac{3}{2}(p_A V_A - p_C V_C) = \frac{3}{2}(600 - 1000) = -600\ \text{J}"))\
        .di("Al sumar los tres calores obtenemos exactamente menos trescientos joules, idéntico al trabajo neto porque la energía interna del ciclo es cero.",
            tabla_quw([("A→B", rAB), ("B→C", rBC), ("C→A", rCA)], d=1),
            R(r"Q_{\text{neto}} = 4050 - 3750 - 600 = -300\ \text{J}"))
    L.usa(F_GAS, F_W_INT, F_W_ISO, F_CV_M, F_MAYER, F_DU, F_1LEY, F_DQP, F_DQV)
    L.responde("A-TB", rf"T_B = {tex(g.t('B'), 4)}\ \text{{K}}", g.t("B"), "K")
    L.responde("C", r"W_{\text{neto}} = -300\ \text{J}", w_neto, "J")
    L.responde("D", r"Q_{\text{neto}} = -300\ \text{J}", q_neto, "J")
    return L.oficial(
        "Tarea 3 I-2026", 300.7, -300.0, -305.0, tol=0.02,
        nota="La solución oficial redondeó T_A=72,2 y T_C=120,3 K dando Q_neto=-305 J; el exacto por conservación es -300,0 J.",
    )


# ============================================================================ 16. T3-26I-8
@leccion("T3-26I-8")
def t3_26i_8() -> Leccion:
    r1, r2, r3 = 4.50, 5.50, 8.50
    t1, t3 = 200.0, 35.0
    k_ac, k_pol = 53.0, 0.0340
    ln21 = math.log(r2 / r1)
    ln32 = math.log(r3 / r2)
    t2 = (k_ac * t1 * ln32 + k_pol * t3 * ln21) / (k_ac * ln32 + k_pol * ln21)  # 199.951 °C
    hl = 2.0 * math.pi * k_pol * (t2 - t3) / ln32                                # 80.950 W/m
    t_5 = t1 - (hl / (2.0 * math.pi * k_ac)) * math.log(5.00 / r1)              # 199.974 °C
    t_7 = t3 + (hl / (2.0 * math.pi * k_pol)) * math.log(r3 / 7.00)             # 108.58 °C
    r_100 = r3 * math.exp(2.0 * math.pi * k_pol * (t3 - 100.0) / hl)            # 7.160 cm

    L = Leccion(
        "T3-26I-8",
        "Tubería compuesta de acero y poliestireno (2 capas cilíndricas) (Tarea 3 I-2026, P8)",
        tema="5.4", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "8"),
        enunciado=(
            "Una tubería de acero ($r_1 = 4{,}50\\ \\text{cm}$, $r_2 = 5{,}50\\ \\text{cm}$, $k_{\\text{ac}} = 53{,}0\\ \\text{W/(m}\\cdot\\text{K)}$) "
            "está recubierta con poliestireno ($r_2 = 5{,}50\\ \\text{cm}$, $r_3 = 8{,}50\\ \\text{cm}$, $k_{\\text{pol}} = 0{,}0340\\ \\text{W/(m}\\cdot\\text{K)}$). "
            "Transporta gas a $T_1 = 200{,}0\\,^\\circ\\text{C}$ en ambiente a $T_3 = 35{,}0\\,^\\circ\\text{C}$. "
            "A) Halle la pérdida $H/L$. B) Halle la temperatura $T_2$ en la interfaz acero–poliestireno. "
            "C) Halle $T(5{,}00\\ \\text{cm})$. D) Halle $T(7{,}00\\ \\text{cm})$. E) Halle el radio donde $T = 100{,}0\\,^\\circ\\text{C}$."
        ),
        claves=["dos capas", "cilindro compuesto", "temperatura interfaz", "aislante", "Fourier", "tarea"],
    )
    L.paso("Integrar Fourier en cada capa cilíndrica")\
        .di("Partimos de Fourier diferencial con el área lateral del cilindro y separamos variables. En estado estacionario hache entre ele es igual en el acero y en el aislante.",
            DATO("53,0 W/(m·K)"),
            F(F_FOUR_R),
            M(r"H = -k(2\pi r L)\frac{dT}{dr} \;\Rightarrow\; \ln r = -\frac{2\pi k}{H/L}\,T + C"))\
        .di("Al evaluar entre las dos fronteras de cada capa se elimina la constante ce de integración.",
            M(r"T_1 - T_2 = \frac{H/L}{2\pi k_{\text{ac}}}\ln\left(\frac{r_2}{r_1}\right),\qquad T_2 - T_3 = \frac{H/L}{2\pi k_{\text{pol}}}\ln\left(\frac{r_3}{r_2}\right)"))
    L.paso("A y B) Temperatura de interfaz T₂ y pérdida H/L")\
        .di("Igualamos hache entre ele en ambas capas para despejar la temperatura de la unión. Como los cocientes de radios son adimensionales, podemos dejarlos en centímetros.",
            M(r"T_2 = \frac{k_{\text{ac}}T_1\ln(r_3/r_2) + k_{\text{pol}}T_3\ln(r_2/r_1)}{k_{\text{ac}}\ln(r_3/r_2) + k_{\text{pol}}\ln(r_2/r_1)}"),
            M(rf"T_2 = \frac{{(53{{,}}0)(200{{,}}0)({tex(ln32, 4)}) + (0{{,}}0340)(35{{,}}0)({tex(ln21, 4)})}}{{(53{{,}}0)({tex(ln32, 4)}) + (0{{,}}0340)({tex(ln21, 4)})}} = {tex(t2, 5)}\ {C}"),
            M(rf"\frac{{H}}{{L}} = \frac{{2\pi(0{{,}}0340)({tex(t2, 5)} - 35{{,}}0)}}{{{tex(ln32, 4)}}} = \frac{{35{{,}}24}}{{0{{,}}4353}} = {tex(hl, 4)}\ \text{{W/m}}"),
            R(rf"\frac{{H}}{{L}} = {tex(hl, 4)}\ \text{{W/m}},\qquad T_2 = {tex(t2, 5)}\ {C}"))
    L.paso("C y D) Temperaturas a 5,00 cm y a 7,00 cm")\
        .di("Cinco centímetros está dentro del acero, donde casi no cae la temperatura porque es muy buen conductor; siete centímetros cae en el aislante.",
            M(rf"T(5{{,}}00\ \text{{cm}}) = 200{{,}}0 - \frac{{{tex(hl, 4)}}}{{2\pi(53{{,}}0)}}\ln\left(\frac{{5{{,}}00}}{{4{{,}}50}}\right) = 200{{,}}0 - 0{{,}}026 = {tex(t_5, 5)}\ {C}"),
            M(rf"T(7{{,}}00\ \text{{cm}}) = 35{{,}}0 + \frac{{{tex(hl, 4)}}}{{2\pi(0{{,}}0340)}}\ln\left(\frac{{8{{,}}50}}{{7{{,}}00}}\right) = 35{{,}}0 + 73{{,}}58 = {tex(t_7, 5)}\ {C}"),
            R(rf"T(5{{,}}00\ \text{{cm}}) = {tex(t_5, 5)}\ {C},\qquad T(7{{,}}00\ \text{{cm}}) = {tex(t_7, 5)}\ {C}"))
    L.paso("E) Radio donde la temperatura es 100,0 °C")\
        .di("Como el acero entero está cerca de doscientos grados, cien grados ocurre necesariamente dentro del poliestireno. Despejamos el radio aplicando exponencial.",
            M(r"\ln\left(\frac{r}{r_3}\right) = \frac{2\pi k_{\text{pol}}(T_3 - T)}{H/L}"),
            M(rf"r = 8{{,}}50\,\exp\left(\frac{{2\pi(0{{,}}0340)(35{{,}}0 - 100{{,}}0)}}{{{tex(hl, 4)}}}\right) = 8{{,}}50\,e^{{-0{{,}}1716}} = {tex(r_100, 3)}\ \text{{cm}}"),
            R(rf"r = {tex(r_100, 3)}\ \text{{cm}}"))
    L.usa(F_FOUR_R)
    L.responde("A", rf"H/L = {tex(hl, 4)}\ \text{{W/m}}", hl, "W/m")
    L.responde("B", rf"T_2 = {tex(t2, 5)}\ {C}", t2, "°C")
    L.responde("C", rf"T(5) = {tex(t_5, 5)}\ {C}", t_5, "°C")
    L.responde("D", rf"T(7) = {tex(t_7, 5)}\ {C}", t_7, "°C")
    L.responde("E", rf"r = {tex(r_100, 3)}\ \text{{cm}}", r_100, "cm")
    return L.oficial("Tarea 3 I-2026", 80.95, 199.96, 199.97, 108.57, 7.16)


# ============================================================================ 17. T3-26I-9
@leccion("T3-26I-9")
def t3_26i_9() -> Leccion:
    rho_fe, k_fe = 7870.0, 80.0
    dt = 105.0  # 100 - (-5)
    d_esf, d_cil, d_cubo = 5.33e-3, 3.49e-3, 2.72e-3
    # Masas de hierro con esos grosores (en cm^3)
    v_fe_esf = (4.0 / 3.0) * math.pi * (10.0 ** 3 - 9.467 ** 3)
    v_fe_cil = math.pi * (20.0 * 10.0 ** 2 - 19.302 * 9.651 ** 2)
    v_fe_cubo = 20.0 ** 3 - 19.456 ** 3
    m_fe_esf = 7.870e-3 * v_fe_esf   # 4.995 kg
    m_fe_cil = 7.870e-3 * v_fe_cil   # 4.999 kg
    m_fe_cubo = 7.870e-3 * v_fe_cubo # 4.999 kg

    # Áreas superficiales exteriores exactas en m^2
    a_esf = 4.0 * math.pi * (0.100 ** 2)       # 0.12566 m^2
    a_cil = 2.0 * math.pi * 0.100 * (0.100 + 0.200)  # 0.18850 m^2 (2 tapas + lateral)
    a_cubo = 6.0 * (0.200 ** 2)                # 0.2400 m^2

    h_esf = k_fe * a_esf * dt / d_esf    # 1.980e5 W
    h_cil = k_fe * a_cil * dt / d_cil    # 4.537e5 W
    h_cubo = k_fe * a_cubo * dt / d_cubo # 7.412e5 W

    # Masas de hielo interior (rho_hielo = 917 kg/m^3, c_hielo = 2100 J/(kg·K))
    m_h_esf = 0.917e-3 * (4.0 / 3.0) * math.pi * (9.467 ** 3)  # 3.259 kg
    m_h_cil = 0.917e-3 * math.pi * 19.302 * (9.651 ** 2)       # 5.179 kg
    m_h_cubo = 0.917e-3 * (19.456 ** 3)                        # 6.754 kg
    q_esf = m_h_esf * 2100.0   # 6844 J
    q_cil = m_h_cil * 2100.0   # 10876 J
    q_cubo = m_h_cubo * 2100.0 # 14183 J
    t_esf = q_esf / h_esf      # 0.03456 s = 34.56 ms
    t_cil = q_cil / h_cil      # 0.02397 s = 23.97 ms
    t_cubo = q_cubo / h_cubo   # 0.01914 s = 19.14 ms

    L = Leccion(
        "T3-26I-9",
        "Tres recipientes de hierro (esfera, cilindro y cubo) con hielo (Tarea 3 I-2026, P9)",
        tema="5.3", tipo="desarrollo", nivel=4, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "9"),
        enunciado=(
            "Con $5{,}00\\ \\text{kg}$ de hierro ($\\rho_{\\text{Fe}} = 7870\\ \\text{kg/m}^3$, $k_{\\text{Fe}} = 80{,}0\\ \\text{W/(m}\\cdot\\text{K)}$) "
            "se fabrican tres recipientes de $20{,}0\\ \\text{cm}$ de dimensión exterior (esfera de diámetro $20\\ \\text{cm}$, "
            "cilindro de diámetro y altura $20\\ \\text{cm}$, y cubo de arista $20\\ \\text{cm}$), llenos de hielo a $-5{,}00\\,^\\circ\\text{C}$ "
            "y sumergidos en agua a $100{,}0\\,^\\circ\\text{C}$. A) Compruebe los grosores $5{,}33\\ \\text{mm}$, $3{,}49\\ \\text{mm}$ y $2{,}72\\ \\text{mm}$. "
            "B) Explique por qué la esfera es más gruesa. C) Halle el flujo inicial $H$ en cada recipiente. "
            "D) Halle la energía $Q$ y el tiempo $\\Delta t$ para elevar $1\\,^\\circ\\text{C}$ el hielo interior."
        ),
        aviso=(
            "Errata mayor en la solución oficial (incisos C y D): sustituyó radios y aristas en cm² sin convertir a m² "
            "(error de factor 10 000 en H y Δt: reportó 10⁹ W y microsegundos en vez de 10⁵ W y milisegundos). "
            "Además escribió πR(2R + h) para el área del cilindro en vez de 2πR(R + h). Aquí se corrige todo a SI."
        ),
        claves=["conducción", "geometría", "esfera", "cilindro", "cubo", "unidades SI", "tarea"],
    )
    L.paso("A–B) Comprobación de grosores y relación con el área")\
        .di("El volumen de metal en cada recipiente debe producir cinco kilogramos al multiplicar por la densidad del hierro.",
            DATO("5,00 kg de hierro"),
            M(rf"V_{{\text{{Fe,esf}}}} = \frac{{4}}{{3}}\pi(10{{,}}00^3 - 9{{,}}467^3) = {tex(v_fe_esf, 4)}\ \text{{cm}}^3 \;\Rightarrow\; m = {tex(m_fe_esf, 4)}\ \text{{kg}}"),
            M(rf"V_{{\text{{Fe,cil}}}} = \pi(20{{,}}00\cdot 10{{,}}00^2 - 19{{,}}302\cdot 9{{,}}651^2) = {tex(v_fe_cil, 4)}\ \text{{cm}}^3 \;\Rightarrow\; m = {tex(m_fe_cil, 4)}\ \text{{kg}}"),
            M(rf"V_{{\text{{Fe,cubo}}}} = 20{{,}}00^3 - 19{{,}}456^3 = {tex(v_fe_cubo, 4)}\ \text{{cm}}^3 \;\Rightarrow\; m = {tex(m_fe_cubo, 4)}\ \text{{kg}}"))\
        .di("Como todos usan el mismo volumen de hierro, el grosor es inversamente proporcional al área superficial: la esfera tiene menor superficie, por eso su pared es la más gruesa.",
            NOTA("Al mismo volumen metálico $V \\approx A\\cdot d$: $A_{\\text{esf}} < A_{\\text{cil}} < A_{\\text{cubo}} \\Rightarrow d_{\\text{esf}} > d_{\\text{cil}} > d_{\\text{cubo}}$."))
    L.paso("C) Flujos iniciales de calor en unidades SI")\
        .di("Usamos la conducción estacionaria del formulario convirtiendo todas las áreas a metros cuadrados y los grosores a metros, con salto de ciento cinco kelvin.",
            F(F_COND),
            M(rf"H_{{\text{{esf}}}} = \frac{{(80{{,}}0)(4\pi\cdot 0{{,}}100^2)(105)}}{{0{{,}}00533}} = \frac{{1056}}{{0{{,}}00533}} = {tex(h_esf, 4)}\ \text{{W}}"),
            M(rf"H_{{\text{{cil}}}} = \frac{{(80{{,}}0)[2\pi(0{{,}}100)(0{{,}}300)](105)}}{{0{{,}}00349}} = \frac{{1583}}{{0{{,}}00349}} = {tex(h_cil, 4)}\ \text{{W}}"),
            M(rf"H_{{\text{{cubo}}}} = \frac{{(80{{,}}0)(6\cdot 0{{,}}200^2)(105)}}{{0{{,}}00272}} = \frac{{2016}}{{0{{,}}00272}} = {tex(h_cubo, 4)}\ \text{{W}}"),
            R(rf"H_{{\text{{esf}}}} = {tex(h_esf, 4)}\ \text{{W}} < H_{{\text{{cil}}}} = {tex(h_cil, 4)}\ \text{{W}} < H_{{\text{{cubo}}}} = {tex(h_cubo, 4)}\ \text{{W}}"))
    L.paso("D) Energía para subir 1 °C el hielo y tiempo Δt")\
        .di("Calculamos la masa de hielo en el volumen interior con novecientos diecisiete kilogramos por metro cúbico y el calor para subir un grado.",
            F(F_QS),
            M(rf"Q_{{\text{{esf}}}} = (3{{,}}259)(2100)(1) = {tex(q_esf, 4)}\ \text{{J}},\quad Q_{{\text{{cil}}}} = (5{{,}}179)(2100)(1) = {tex(q_cil, 5)}\ \text{{J}},\quad Q_{{\text{{cubo}}}} = (6{{,}}754)(2100)(1) = {tex(q_cubo, 5)}\ \text{{J}}"))\
        .di("Dividimos cada calor entre su flujo inicial para hallar el tiempo en segundos y lo convertimos a milisegundos.",
            M(rf"\Delta t_{{\text{{esf}}}} = \frac{{6844}}{{1{{,}}980\times 10^5}} = {tex(t_esf, 4)}\ \text{{s}} = {tex(t_esf * 1000, 4)}\ \text{{ms}}"),
            M(rf"\Delta t_{{\text{{cil}}}} = \frac{{10\,876}}{{4{{,}}537\times 10^5}} = {tex(t_cil, 4)}\ \text{{s}} = {tex(t_cil * 1000, 4)}\ \text{{ms}}"),
            M(rf"\Delta t_{{\text{{cubo}}}} = \frac{{14\,183}}{{7{{,}}412\times 10^5}} = {tex(t_cubo, 4)}\ \text{{s}} = {tex(t_cubo * 1000, 4)}\ \text{{ms}}"),
            R(rf"\Delta t_{{\text{{esf}}}} = {tex(t_esf * 1000, 4)}\ \text{{ms}},\quad \Delta t_{{\text{{cil}}}} = {tex(t_cil * 1000, 4)}\ \text{{ms}},\quad \Delta t_{{\text{{cubo}}}} = {tex(t_cubo * 1000, 4)}\ \text{{ms}}"))
    L.usa(F_COND, F_QS)
    L.responde("C-esf", rf"H_{{\text{{esf}}}} = {tex(h_esf, 4)}\ \text{{W}}", h_esf, "W")
    L.responde("C-cil", rf"H_{{\text{{cil}}}} = {tex(h_cil, 4)}\ \text{{W}}", h_cil, "W")
    L.responde("C-cubo", rf"H_{{\text{{cubo}}}} = {tex(h_cubo, 4)}\ \text{{W}}", h_cubo, "W")
    L.responde("D-Qesf", rf"Q_{{\text{{esf}}}} = {tex(q_esf, 4)}\ \text{{J}}", q_esf, "J")
    L.responde("D-Qcubo", rf"Q_{{\text{{cubo}}}} = {tex(q_cubo, 5)}\ \text{{J}}", q_cubo, "J")
    return L.oficial(
        "Tarea 3 I-2026", 1.980e9, 3.025e9, 7.412e9, 6844.0, 14183.0, tol=0.015,
        nota="La solución oficial olvidó convertir cm² a m² (error 10⁴ en H y Δt) y usó πR(2R+h) en el cilindro; las energías Q sí coinciden.",
    )


# ============================================================================ 18. T3-26I-10
@leccion("T3-26I-10")
def t3_26i_10() -> Leccion:
    a_r, b_r, l_m = 0.150, 0.750, 1.50
    m_pend = (b_r - a_r) / l_m  # 0.40
    k0 = 6.50
    t1 = 90.0 + T0              # 363.15 K
    t2 = 20.0 + T0              # 293.15 K
    coef_h = (1.0 / a_r - 1.0 / b_r) / (m_pend * math.pi)  # 4.24413 m^-1
    rhs = 0.5 * k0 * (t1 * t1 - t2 * t2)                   # 149308.25 W/m
    h_val = rhs / coef_h                                   # 35180 W
    u_m = a_r + m_pend * (l_m / 2.0)                       # 0.450 m
    lhs_m = (h_val / (m_pend * math.pi)) * (1.0 / a_r - 1.0 / u_m)
    tm_sq = t1 * t1 - (2.0 / k0) * lhs_m
    tm = math.sqrt(tm_sq)                                  # 305.94 K
    dt_750 = 750.0 / h_val                                 # 0.02132 s

    L = Leccion(
        "T3-26I-10",
        "Cono truncado con doble variable: área A(x) y k(T) = k₀T (Tarea 3 I-2026, P10)",
        tema="5.4", tipo="desarrollo", nivel=5, parcial=3,
        fuente=_f("26-I-T3", "Tarea 3 I-2026", "10"),
        enunciado=(
            "Una barra metálica con forma de cono truncado une un reservorio caliente a $T_1 = 90{,}0\\,^\\circ\\text{C}$ "
            "con uno frío a $T_2 = 20{,}0\\,^\\circ\\text{C}$ (Figura 2). Su radio crece de $a = 15{,}0\\ \\text{cm}$ a "
            "$b = 75{,}0\\ \\text{cm}$ en un largo $L = 1{,}50\\ \\text{m}$, y su conductividad depende de la temperatura "
            "como $k(T) = k_0 T$ con $k_0 = 6{,}50\\ \\text{W/(m}\\cdot\\text{K}^2)$. "
            "Calcule: A) el flujo de calor $H$; B) la temperatura a la mitad del cono ($x = L/2$); "
            "C) el tiempo para transmitir $750\\ \\text{J}$."
        ),
        claves=["cono truncado", "doble variable", "k(T)", "Fourier", "integral", "Cartago extremo", "tarea"],
    )
    L.paso("A) Plantear A(x), k(T) y separar variables")\
        .di("Aquí cambian dos cosas a la vez: el área con equis y la conductividad con te. Partimos de Fourier diferencial, pasamos las temperaturas a kelvin y separamos variables.",
            DATO("k = k₀·T"),
            F(F_FOUR), F(F_KEL),
            M(r"T_1 = 90{,}0 + 273{,}15 = 363{,}15\ \text{K},\qquad T_2 = 20{,}0 + 273{,}15 = 293{,}15\ \text{K}"),
            M(r"r(x) = a + \frac{b-a}{L}x = 0{,}15 + 0{,}40x \;\Rightarrow\; A(x) = \pi(0{,}15 + 0{,}40x)^2"),
            M(r"H\int_{0}^{L}\frac{dx}{\pi(0{,}15 + 0{,}40x)^2} = -k_0\int_{T_1}^{T_2} T\,dT"))
    L.paso("A) Integrar por sustitución y despejar H")\
        .di("Hacemos el cambio u igual a cero coma quince más cero coma cuatro equis, integramos ambos lados y evaluamos en los límites.",
            M(r"u = 0{,}15 + 0{,}40x,\qquad du = 0{,}40\,dx"),
            M(r"\frac{H}{0{,}40\pi}\left[\frac{1}{0{,}15}-\frac{1}{0{,}75}\right] = \frac{k_0}{2}(T_1^2 - T_2^2)"),
            M(rf"{tex(coef_h, 4)}\,H = \frac{{6{{,}}50}}{{2}}(363{{,}}15^2 - 293{{,}}15^2) = {tex(rhs, 6)}"),
            M(rf"H = \frac{{{tex(rhs, 6)}}}{{{tex(coef_h, 4)}}} = {tex(h_val, 5)}\ \text{{W}}"),
            R(rf"H = {tex(h_val, 4)}\ \text{{W}}"))
    L.paso("B) Temperatura en la mitad del cono (x = L/2)")\
        .di("Como el flujo estacionario hache es el mismo en cualquier corte, integramos solo hasta la mitad del cono, donde el radio vale cero coma cuarenta y cinco metros.",
            M(r"u_m = 0{,}15 + 0{,}40(0{,}75) = 0{,}45\ \text{m}"),
            M(rf"\frac{{{tex(h_val, 5)}}}{{0{{,}}40\pi}}\left(\frac{{1}}{{0{{,}}15}} - \frac{{1}}{{0{{,}}45}}\right) = \frac{{6{{,}}50}}{{2}}(363{{,}}15^2 - T_m^2)"),
            M(rf"124\,420 = 3{{,}}25(131\,878 - T_m^2) \;\Rightarrow\; T_m^2 = 131\,878 - 38\,284 = {tex(tm_sq, 5)}"),
            M(rf"T_m = \sqrt{{{tex(tm_sq, 5)}}} = {tex(tm, 4)}\ \text{{K}}"),
            R(rf"T(L/2) = {tex(tm, 4)}\ \text{{K}}"))
    L.paso("C) Tiempo para transmitir 750 J")\
        .di("En régimen estacionario el flujo es energía entre tiempo, así que despejamos delta te directamente desde la casilla de conducción.",
            F(F_COND),
            M(rf"\Delta t = \frac{{Q}}{{H}} = \frac{{750}}{{{tex(h_val, 5)}}} = {tex(dt_750, 4)}\ \text{{s}} = {tex(dt_750 * 1000, 4)}\ \text{{ms}}"),
            R(rf"\Delta t = {tex(dt_750 * 1000, 4)}\ \text{{ms}}"))
    L.usa(F_FOUR, F_COND, F_KEL)
    L.responde("A", rf"H = {tex(h_val, 4)}\ \text{{W}}", h_val, "W")
    L.responde("B", rf"T_m = {tex(tm, 4)}\ \text{{K}}", tm, "K")
    L.responde("C", rf"\Delta t = {tex(dt_750 * 1000, 4)}\ \text{{ms}}", dt_750, "s")
    return L.oficial("Tarea 3 I-2026", 35165.0, 305.8, 0.02133)
