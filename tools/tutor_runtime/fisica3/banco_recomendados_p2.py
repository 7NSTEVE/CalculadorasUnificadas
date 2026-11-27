"""Ejercicios Recomendados de Cátedra 2 · FI2103 II-2026 (Parcial 2: elasticidad y fluidos, 14 lecciones).

Fuentes: `Ejercicios_Recomendados_2_IIS2026.pdf` y `Ejercicios_Recomendados_2_IIS2026_Solucion.pdf`
(carpeta oficial «Documentos II - 2026»). Cada desarrollo parte estrictamente de las casillas
oficiales del formulario del Parcial 2 (`formularios_p1_p2.py`) y calcula todos los números en Python.

IDs: E2-1 .. E2-14 (`parcial=2`, `tema="fluidos"`).
"""

from __future__ import annotations

import math

from .dsl import DATO, F, M, NOTA, OJO, Leccion, R
from .formato import partes, tex
from .registro import leccion

G = 9.80

# Casillas exactas del formulario oficial Parcial 2
YOUNG = r"Y = \frac{F_\perp / A}{\Delta l / l_0}"
VOLUM = r"B = -\frac{F_\perp / A}{\Delta V / V_0}"
CORTE = r"S = \frac{F_\parallel / A}{x / h}"
DENS = r"\rho = \frac{m}{V}"
PRES = r"p = \frac{dF_\perp}{dA}"
MOD_EL = r"\text{módulo elástico} = \frac{\text{esfuerzo}}{\text{deformación}}"
F_INT = r"F_\perp = \int p\,dA"
TORQ = r"d\tau = r\sen\theta\,dF_\perp = r\sen\theta\,p\,dA"
MAN = r"p_{\text{man}} = p_{\text{abs}} - p_{\text{atm}}"
HIDRO = r"p = p_0 + \rho gh"
CAUDAL = r"\frac{dV}{dt} = Av"
CONT = r"A_1 v_1 = A_2 v_2"
BERN = r"p_1 + \rho g y_1 + \frac{1}{2}\rho v_1^2 = p_2 + \rho g y_2 + \frac{1}{2}\rho v_2^2"
REYN = r"N_R = \frac{\rho v D}{\eta}"
POIS = r"\frac{dV}{dt} = \frac{(p_1 - p_2)\pi R^4}{8\eta L}"


def _f(num: int) -> dict:
    return {"doc": "recomendados2", "nombre": "Ejercicios Recomendados 2 (II-2026)", "num": str(num), "parcial": 2}


def _q(x: float, c: int = 4) -> float:
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ E2-1
@leccion("E2-1")
def e2_1() -> Leccion:
    Y, sig, d, l0 = 1.50e10, 1.50e8, 0.0250, 0.250
    A = math.pi * (d / 2.0) ** 2    # 4.9087e-4 m^2
    Fmax = sig * A                  # 7.363e4 N
    dl = sig * l0 / Y               # 2.50e-3 m

    L = Leccion(
        "E2-1",
        "Fuerza de ruptura y acortamiento del fémur bajo compresión (Recomendados 2, E1)",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2, fuente=_f(1),
        enunciado=(
            "El módulo de Young del hueso es $Y = 1{,}50\\times 10^{10}\\ \\text{N/m}^2$ y se rompe si el esfuerzo "
            "supera $1{,}50\\times 10^8\\ \\text{N/m}^2$. a) Halle la fuerza máxima sobre un fémur de diámetro $2{,}50\\ \\text{cm}$. "
            "b) ¿Cuánto se acorta un fémur de $25{,}0\\ \\text{cm}$ bajo esa fuerza compresiva?"
        ),
        claves=["módulo de Young", "esfuerzo", "fémur", "compresión", "recomendados"],
    )
    L.paso("a) Área transversal y fuerza máxima de ruptura")\
        .di("Calculamos el área circular con radio cero coma cero ciento veinticinco metros y despejamos la fuerza desde el esfuerzo normal.",
            DATO("2,50 cm"),
            F(YOUNG),
            M(rf"A = \pi r^2 = \pi(0{{,}}0125)^2 = {tex(A, 4)}\ \text{{m}}^2"),
            M(rf"F_{{\text{{máx}}}} = \left(\frac{{F_\perp}}{{A}}\right)_{{\text{{máx}}}} A = (1{{,}}50\times 10^8)({tex(A, 4)}) = {tex(Fmax, 4)}\ \text{{N}}"),
            R(rf"F_{{\text{{máx}}}} = {tex(Fmax, 3)}\ \text{{N}}"))
    L.paso("b) Acortamiento del hueso de 25,0 cm")\
        .di("Despejamos delta ele de la casilla del módulo de Young usando el esfuerzo límite y la longitud inicial en metros.",
            M(r"\Delta l = \frac{(F_\perp / A)\,l_0}{Y}"),
            M(rf"\Delta l = \frac{{(1{{,}}50\times 10^8)(0{{,}}250)}}{{1{{,}}50\times 10^{{10}}}} = {tex(dl, 3)}\ \text{{m}} = {tex(dl * 1000, 3)}\ \text{{mm}}"),
            R(rf"\Delta l = {tex(dl * 1000, 3)}\ \text{{mm}}"))
    L.usa(YOUNG)
    L.responde("a", rf"F_{{\text{{máx}}}} = {tex(Fmax, 3)}\ \text{{N}}", Fmax, "N")
    L.responde("b", rf"\Delta l = {tex(dl * 1000, 3)}\ \text{{mm}}", dl * 1000, "mm")
    return L.oficial("Recomendados 2 II-2026", 7.36e4, 2.50)


# ============================================================================ E2-2
@leccion("E2-2")
def e2_2() -> Leccion:
    tau = 4.00e8
    d, t = 0.0100, 0.00500
    A_perno = math.pi * (d / 2.0) ** 2  # 7.854e-5 m^2
    F_a = tau * A_perno                 # 3.142e4 N
    A_placa = math.pi * d * t           # 1.5708e-4 m^2
    F_b = tau * A_placa                 # 6.283e4 N

    L = Leccion(
        "E2-2",
        "Esfuerzo cortante en un perno y punzonado de una placa de acero (Recomendados 2, E2)",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2, fuente=_f(2),
        enunciado=(
            "El acero se rompe cuando el esfuerzo cortante supera $4{,}00\\times 10^8\\ \\text{N/m}^2$. "
            "Halle la fuerza cortante necesaria para: a) cortar un perno de $1{,}00\\ \\text{cm}$ de diámetro; "
            "b) punzonar un agujero de $1{,}00\\ \\text{cm}$ de diámetro en una placa de $0{,}500\\ \\text{cm}$ de grosor."
        ),
        claves=["esfuerzo cortante", "punzonado", "acero", "área lateral", "recomendados"],
    )
    L.paso("a) Cortar el perno (área circular)")\
        .di("En el perno la fuerza paralela corta la sección transversal circular. Multiplicamos el esfuerzo cortante límite por pi erre cuadrado.",
            DATO("4,00"),
            F(CORTE),
            M(rf"A_a = \pi(0{{,}}00500)^2 = {tex(A_perno, 4)}\ \text{{m}}^2"),
            M(rf"F_a = (4{{,}}00\times 10^8)({tex(A_perno, 4)}) = {tex(F_a, 4)}\ \text{{N}}"),
            R(rf"F_a = {tex(F_a, 3)}\ \text{{N}}"))
    L.paso("b) Punzonar la placa (área lateral cilíndrica)")\
        .di("Al punzonar un disco, la superficie que se desgarra es el borde cilíndrico alrededor del agujero: perímetro pi por diámetro multiplicado por el grosor de la placa.",
            M(rf"A_b = \pi d\,t = \pi(0{{,}}0100)(0{{,}}00500) = {tex(A_placa, 4)}\ \text{{m}}^2"),
            M(rf"F_b = (4{{,}}00\times 10^8)({tex(A_placa, 4)}) = {tex(F_b, 4)}\ \text{{N}}"),
            R(rf"F_b = {tex(F_b, 3)}\ \text{{N}}"))
    L.usa(CORTE)
    L.responde("a", rf"F_a = {tex(F_a, 3)}\ \text{{N}}", F_a, "N")
    L.responde("b", rf"F_b = {tex(F_b, 3)}\ \text{{N}}", F_b, "N")
    return L.oficial("Recomendados 2 II-2026", 3.14e4, 6.28e4)


# ============================================================================ E2-3
@leccion("E2-3")
def e2_3() -> Leccion:
    F_carga = 8500.0
    L1, d1, Y1 = 5.75, 0.0127, 2.0e11
    L2, dint, dext, Y2 = 3.25, 0.1614, 0.1624, 7.0e10
    A1 = math.pi * (d1 / 2.0) ** 2                             # 1.2668e-4 m^2
    A2 = math.pi * ((dext / 2.0) ** 2 - (dint / 2.0) ** 2)     # 2.5431e-4 m^2
    k1 = Y1 * A1 / L1                                          # 4.4062e6 N/m
    k2 = Y2 * A2 / L2                                          # 5.4775e6 N/m
    dy_par = F_carga / (k1 + k2)                               # 8.600e-4 m = 0.860 mm
    F1_par = k1 * dy_par                                       # 3789 N
    F2_par = k2 * dy_par                                       # 4711 N
    dl1_ind = F_carga * L1 / (A1 * Y1)                         # 1.929 mm
    dl2_ind = F_carga * L2 / (A2 * Y2)                         # 1.552 mm

    L = Leccion(
        "E2-3",
        "Pasadizo sostenido por cable de acero y columna hueca de aluminio (Recomendados 2, E3)",
        tema="fluidos", tipo="desarrollo", nivel=4, parcial=2, fuente=_f(3),
        enunciado=(
            "Un punto de apoyo de un pasadizo recibe $F = 8500\\ \\text{N}$ hacia abajo, sostenido arriba por un cable "
            "de acero ($d_1 = 1{,}27\\ \\text{cm}$, $L_1 = 5{,}75\\ \\text{m}$, $Y_1 = 2{,}0\\times 10^{11}\\ \\text{Pa}$) "
            "y abajo por una columna hueca de aluminio ($d_{\\text{int}} = 16{,}14\\ \\text{cm}$, $d_{\\text{ext}} = 16{,}24\\ \\text{cm}$, "
            "$L_2 = 3{,}25\\ \\text{m}$, $Y_2 = 7{,}0\\times 10^{10}\\ \\text{Pa}$). ¿Cuánto se desplaza hacia abajo ese punto?"
        ),
        aviso=(
            "Errata física en la solución publicada: aplicó la carga completa de 8500 N tanto al cable como a la columna "
            "(dando ΔL₁ = 1,93 mm y ΔL₂ = 1,55 mm) y luego los sumó (3,48 mm). Como el mismo punto se une a ambos soportes "
            "en paralelo, el cable se estira y la columna se comprime exactamente lo mismo (Δy) repartiendo la carga "
            "F₁ + F₂ = 8500 N → Δy = 0,860 mm."
        ),
        claves=["módulo de Young", "paralelo", "cable", "columna hueca", "compatibilidad", "recomendados"],
    )
    L.paso("Áreas transversales del cable y la columna hueca")\
        .di("Convertimos todos los diámetros a radios en metros. En la columna hueca restamos el círculo interior al círculo exterior.",
            DATO("8 500 N"),
            M(rf"A_1 = \pi(0{{,}}00635)^2 = {tex(A1, 4)}\ \text{{m}}^2"),
            M(rf"A_2 = \pi(0{{,}}0812^2 - 0{{,}}0807^2) = {tex(A2, 4)}\ \text{{m}}^2"))
    L.paso("Equilibrio en paralelo: mismo desplazamiento Δy")\
        .di("Como el pasadizo baja una única distancia delta ye, el cable arriba se estira delta ye y la columna abajo se comprime delta ye, y entre los dos sostienen ocho mil quinientos newtons.",
            F(YOUNG),
            M(r"F_1 = \frac{Y_1 A_1}{L_1}\,\Delta y,\qquad F_2 = \frac{Y_2 A_2}{L_2}\,\Delta y,\qquad F_1 + F_2 = F"),
            M(rf"k_1 = \frac{{(2{{,}}0\times 10^{{11}})({tex(A1, 4)})}}{{5{{,}}75}} = {tex(k1, 4)}\ \text{{N/m}}"),
            M(rf"k_2 = \frac{{(7{{,}}0\times 10^{{10}})({tex(A2, 4)})}}{{3{{,}}25}} = {tex(k2, 4)}\ \text{{N/m}}"))
    L.paso("Despejar el desplazamiento Δy")\
        .di("Dividimos la carga total entre la suma de las dos rigideces. Así el cable toma tres mil setecientos ochenta y nueve newtons y la columna cuatro mil setecientos once.",
            M(rf"\Delta y = \frac{{F}}{{k_1 + k_2}} = \frac{{8500}}{{4{{,}}406\times 10^6 + 5{{,}}478\times 10^6}} = \frac{{8500}}{{9{{,}}884\times 10^6}} = {tex(dy_par, 3)}\ \text{{m}}"),
            R(rf"\Delta y = {tex(dy_par * 1000, 3)}\ \text{{mm}}"),
            OJO("Comprobación: F₁ + F₂ = 3789 + 4711 = 8500 N. (La solución publicada sumó 1,93 + 1,55 = 3,48 mm tratando cada elemento con 8500 N por separado.)"))
    L.usa(YOUNG)
    L.responde("dy", rf"\Delta y = {tex(dy_par * 1000, 3)}\ \text{{mm}}", dy_par * 1000, "mm")
    return L.oficial(
        "Recomendados 2 II-2026", 3.48, tol=0.015,
        nota="La solución oficial aplicó los 8500 N completos a cada soporte y sumó ΔL₁ + ΔL₂ = 3,48 mm; en paralelo real Δy = F/(k₁+k₂) = 0,860 mm.",
    )


# ============================================================================ E2-4
@leccion("E2-4")
def e2_4() -> Leccion:
    p0, p_f, B, v0, rho0 = 1.0e5, 1.16e8, 2.2e9, 1.0, 1.03e3
    dp = p_f - p0                  # 1.159e8 Pa
    dv = -dp * v0 / B              # -0.05268 m^3
    vf = v0 + dv                   # 0.94732 m^3
    rho_f = rho0 * v0 / vf         # 1087.3 kg/m^3

    L = Leccion(
        "E2-4",
        "Compresión y densidad del agua de mar en la Fosa de las Marianas (Recomendados 2, E4)",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2, fuente=_f(4),
        enunciado=(
            "En la Fosa de las Marianas ($10{,}9\\ \\text{km}$) la presión es $1{,}16\\times 10^8\\ \\text{Pa}$. "
            "Con $p_0 = 1{,}0\\times 10^5\\ \\text{Pa}$, $B = 2{,}2\\times 10^9\\ \\text{Pa}$ y "
            "$\\rho_0 = 1{,}03\\times 10^3\\ \\text{kg/m}^3$, halle: a) el cambio de volumen de $1{,}0\\ \\text{m}^3$ de agua; "
            "b) la densidad a esa profundidad."
        ),
        claves=["módulo de volumen", "Fosa de las Marianas", "compresibilidad", "densidad", "recomendados"],
    )
    L.paso("a) Cambio de volumen con el módulo de volumen B")\
        .di("Hallamos el aumento de presión respecto a la superficie y despejamos delta ve desde la casilla de módulo de volumen.",
            DATO("1,16"),
            F(VOLUM),
            M(rf"\Delta p = 1{{,}}16\times 10^8 - 1{{,}}0\times 10^5 = {tex(dp, 4)}\ \text{{Pa}}"),
            M(rf"\Delta V = -\frac{{\Delta p\,V_0}}{{B}} = -\frac{{({tex(dp, 4)})(1{{,}}0)}}{{2{{,}}2\times 10^9}} = {tex(dv, 4)}\ \text{{m}}^3"),
            R(rf"\Delta V = {tex(dv, 2)}\ \text{{m}}^3\ (-5{{,}}3\,\%)"))
    L.paso("b) Densidad del agua de mar en el fondo")\
        .di("La masa de agua no cambia, pero ocupa un volumen menor de cero coma novecientos cuarenta y siete metros cúbicos, así que la densidad sube.",
            F(DENS),
            M(rf"V = 1{{,}}0 - 0{{,}}05268 = {tex(vf, 5)}\ \text{{m}}^3"),
            M(rf"\rho = \frac{{\rho_0 V_0}}{{V}} = \frac{{(1{{,}}03\times 10^3)(1{{,}}0)}}{{{tex(vf, 5)}}} = {tex(rho_f, 4)}\ \text{{kg/m}}^3"),
            R(rf"\rho = {tex(rho_f, 3)}\ \text{{kg/m}}^3"))
    L.usa(VOLUM, DENS)
    L.responde("a", rf"\Delta V = {tex(dv, 2)}\ \text{{m}}^3", dv, "m³")
    L.responde("b", rf"\rho = {tex(rho_f, 3)}\ \text{{kg/m}}^3", rho_f, "kg/m³")
    return L.oficial("Recomendados 2 II-2026", -0.053, 1.09e3)


# ============================================================================ E2-5
@leccion("E2-5")
def e2_5() -> Leccion:
    A_tot = 300.0 * 300.0    # 90000 mm^2
    A_s = 1820.0             # mm^2
    A_c = A_tot - A_s        # 88180 mm^2
    P_n = 400000.0           # N
    den = A_c + 18.0 * A_s   # 120940 mm^2
    sig_c = P_n / den        # 3.3074 N/mm^2
    sig_s = 18.0 * sig_c     # 59.534 N/mm^2
    P_s = sig_s * A_s        # 108351 N
    P_viga = P_s / 8.0       # 13544 N

    L = Leccion(
        "E2-5",
        "Columna de concreto reforzada con 8 vigas de acero (Recomendados 2, E5)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(5),
        enunciado=(
            "Una columna cuadrada de $300\\ \\text{mm}$ de lado consta de concreto reforzado con $8$ vigas de acero "
            "(área total de acero $A_s = 1820\\ \\text{mm}^2$), soportando $400\\ \\text{kN}$ con $Y_s = 18 Y_c$. "
            "Halle: a) el esfuerzo en el concreto ($\\text{N/mm}^2$); b) el esfuerzo en el acero; c) la carga en cada viga de acero."
        ),
        claves=["columna compuesta", "módulo de Young", "concreto", "acero", "recomendados"],
    )
    L.paso("a) Compatibilidad de deformaciones y esfuerzo en el concreto")\
        .di("Concreto y acero se acortan exactamente lo mismo, así que sus deformaciones unitarias son iguales y el esfuerzo del acero es dieciocho veces el del concreto.",
            DATO("400 kN"),
            F(YOUNG), F(MOD_EL),
            M(r"\frac{\sigma_s}{Y_s} = \frac{\sigma_c}{Y_c} \;\Rightarrow\; \sigma_s = 18\sigma_c,\qquad A_c = 300^2 - 1820 = 88\,180\ \text{mm}^2"),
            M(rf"\sigma_c A_c + 18\sigma_c A_s = P \;\Rightarrow\; \sigma_c = \frac{{400\,000}}{{88\,180 + 18(1820)}} = \frac{{400\,000}}{{120\,940}} = {tex(sig_c, 4)}\ \text{{N/mm}}^2"),
            R(rf"\sigma_c = {tex(sig_c, 3)}\ \text{{N/mm}}^2"))
    L.paso("b y c) Esfuerzo del acero y carga por viga")\
        .di("Multiplicamos el esfuerzo del concreto por dieciocho para obtener el del acero, hallamos la carga total del acero y dividimos entre las ocho vigas.",
            M(rf"\sigma_s = 18({tex(sig_c, 4)}) = {tex(sig_s, 4)}\ \text{{N/mm}}^2"),
            M(rf"P_s = \sigma_s A_s = ({tex(sig_s, 4)})(1820) = {tex(P_s, 6)}\ \text{{N}} \;\Rightarrow\; P_i = \frac{{108\,351}}{{8}} = {tex(P_viga, 5)}\ \text{{N}}"),
            R(rf"\sigma_s = {tex(sig_s, 4)}\ \text{{N/mm}}^2,\qquad P_i = {tex(P_viga / 1000, 4)}\ \text{{kN}}"))
    L.usa(YOUNG, MOD_EL)
    L.responde("a", rf"\sigma_c = {tex(sig_c, 3)}\ \text{{N/mm}}^2", sig_c, "N/mm²")
    L.responde("b", rf"\sigma_s = {tex(sig_s, 4)}\ \text{{N/mm}}^2", sig_s, "N/mm²")
    L.responde("c", rf"P_i = {tex(P_viga / 1000, 4)}\ \text{{kN}}", P_viga / 1000, "kN")
    return L.oficial("Recomendados 2 II-2026", 3.31, 59.53, 13.54)


# ============================================================================ E2-6
@leccion("E2-6")
def e2_6() -> Leccion:
    m_kg = 1800.0
    T = m_kg * G                           # 17640 N
    LA, dA, YA = 1.80, 0.0180, 2.00e11
    LB, dB, YB = 1.20, 0.0240, 7.00e10
    AA = math.pi * dA ** 2 / 4.0           # 2.5447e-4 m^2
    AB = math.pi * dB ** 2 / 4.0           # 4.5239e-4 m^2
    sigA, sigB = T / AA, T / AB            # 69.32 MPa, 38.99 MPa
    epsA, epsB = sigA / YA, sigB / YB      # 3.466e-4, 5.570e-4
    dlA, dlB = epsA * LA, epsB * LB        # 0.6239 mm, 0.6684 mm
    dl_tot = dlA + dlB                     # 1.2923 mm

    L = Leccion(
        "E2-6",
        "Dos barras cilíndricas en serie (acero y aluminio) con masa suspendida (Recomendados 2, E6)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(6),
        enunciado=(
            "Dos barras verticales en serie (A acero: $L_A = 1{,}80\\ \\text{m}$, $d_A = 18{,}0\\ \\text{mm}$, $Y_A = 2{,}00\\times 10^{11}\\ \\text{Pa}$; "
            "B aluminio: $L_B = 1{,}20\\ \\text{m}$, $d_B = 24{,}0\\ \\text{mm}$, $Y_B = 7{,}00\\times 10^{10}\\ \\text{Pa}$) "
            "sostienen $1800\\ \\text{kg}$ ($g = 9{,}80\\ \\text{m/s}^2$). Halle: a) tensión y esfuerzo normal en cada barra; "
            "b) deformación unitaria; c) alargamiento individual y total; d) nuevo $d_B$ para igualar ambos esfuerzos."
        ),
        claves=["barras en serie", "módulo de Young", "deformación unitaria", "acero", "aluminio", "recomendados"],
    )
    L.paso("a) Tensión común y esfuerzos normales")\
        .di("Como las barras cuelgan una debajo de la otra y su peso se desprecia, ambas soportan la misma tensión igual al peso de mil ochocientos kilogramos.",
            DATO("1800 kg"),
            F(YOUNG),
            M(rf"T = mg = (1800)(9{{,}}80) = {tex(T, 5)}\ \text{{N}}"),
            M(rf"A_A = \frac{{\pi(0{{,}}0180)^2}}{{4}} = {tex(AA, 4)}\ \text{{m}}^2 \;\Rightarrow\; \sigma_A = \frac{{17\,640}}{{{tex(AA, 4)}}} = {tex(sigA / 1e6, 4)}\ \text{{MPa}}"),
            M(rf"A_B = \frac{{\pi(0{{,}}0240)^2}}{{4}} = {tex(AB, 4)}\ \text{{m}}^2 \;\Rightarrow\; \sigma_B = \frac{{17\,640}}{{{tex(AB, 4)}}} = {tex(sigB / 1e6, 4)}\ \text{{MPa}}"),
            R(rf"\sigma_A = {tex(sigA / 1e6, 4)}\ \text{{MPa}},\qquad \sigma_B = {tex(sigB / 1e6, 4)}\ \text{{MPa}}"))
    L.paso("b y c) Deformaciones unitarias y alargamiento total")\
        .di("Dividimos cada esfuerzo entre su módulo de Young para hallar la deformación unitaria y luego multiplicamos por la longitud de cada barra.",
            M(rf"\varepsilon_A = \frac{{{tex(sigA, 4)}}}{{2{{,}}00\times 10^{{11}}}} = {tex(epsA, 4)},\qquad \varepsilon_B = \frac{{{tex(sigB, 4)}}}{{7{{,}}00\times 10^{{10}}}} = {tex(epsB, 4)}"),
            M(rf"\Delta l_A = ({tex(epsA, 4)})(1{{,}}80) = {tex(dlA * 1000, 3)}\ \text{{mm}},\qquad \Delta l_B = ({tex(epsB, 4)})(1{{,}}20) = {tex(dlB * 1000, 3)}\ \text{{mm}}"),
            M(rf"\Delta l_{{\text{{total}}}} = 0{{,}}624 + 0{{,}}668 = {tex(dl_tot * 1000, 4)}\ \text{{mm}}"),
            R(rf"\Delta l_{{\text{{total}}}} = {tex(dl_tot * 1000, 4)}\ \text{{mm}}"))
    L.paso("d) Nuevo diámetro de B para igualar esfuerzos")\
        .di("Como ambas soportan la misma tensión, igualar esfuerzos exige igualar áreas transversales, así que el diámetro de B debe bajar de veinticuatro a dieciocho milímetros.",
            M(r"\frac{T}{A'_B} = \frac{T}{A_A} \;\Rightarrow\; A'_B = A_A \;\Rightarrow\; d'_B = d_A = 18{,}0\ \text{mm}"),
            R(r"d'_B = 18{,}0\ \text{mm}\ \text{(menor que } 24{,}0\ \text{mm)}"))
    L.usa(YOUNG)
    L.responde("a-sigA", rf"\sigma_A = {tex(sigA / 1e6, 4)}\ \text{{MPa}}", sigA / 1e6, "MPa")
    L.responde("a-sigB", rf"\sigma_B = {tex(sigB / 1e6, 4)}\ \text{{MPa}}", sigB / 1e6, "MPa")
    L.responde("c", rf"\Delta l_{{\text{{total}}}} = {tex(dl_tot * 1000, 4)}\ \text{{mm}}", dl_tot * 1000, "mm")
    L.responde("d", r"d'_B = 18{,}0\ \text{mm}", 18.0, "mm")
    return L.oficial("Recomendados 2 II-2026", 69.32, 38.99, 1.292, 18.0)


# ============================================================================ E2-7
@leccion("E2-7")
def e2_7() -> Leccion:
    rho_a, ha, rho_w, b, h = 820.0, 0.80, 1000.0, 0.60, 1.20
    p_sup = rho_a * G * ha                     # 6428.8 Pa
    p_inf = p_sup + rho_w * G * h              # 18188.8 Pa
    f_net = b * (p_sup * h + 0.5 * rho_w * G * h ** 2)   # 8862.34 N
    tau0 = b * (0.5 * p_sup * h ** 2 + (1.0 / 3.0) * rho_w * G * h ** 3)  # 6164.12 N·m
    y_cp = tau0 / f_net                        # 0.69554 m

    L = Leccion(
        "E2-7",
        "Compuerta rectangular sumergida bajo aceite y agua: F y centro de presión (Recomendados 2, E7)",
        tema="fluidos", tipo="desarrollo", nivel=4, parcial=2, fuente=_f(7),
        enunciado=(
            "Un tanque abierto contiene $0{,}80\\ \\text{m}$ de aceite ($\\rho_a = 820\\ \\text{kg/m}^3$) sobre agua "
            "($\\rho_w = 1000\\ \\text{kg/m}^3$). Una compuerta rectangular ($b = 0{,}60\\ \\text{m}$, $h = 1{,}20\\ \\text{m}$) "
            "comienza justo en la interfaz aceite–agua, con su cara exterior al aire. Determine: "
            "a) presión manométrica superior e inferior; b) $p(y)$ sobre la compuerta; c) fuerza hidrostática neta; "
            "d) profundidad $y_{cp}$ desde el borde superior para el mismo torque."
        ),
        claves=["hidrostática", "integral", "compuerta", "dos líquidos", "torque", "centro de presión", "recomendados"],
    )
    L.paso("a y b) Presiones manométricas y perfil p(y)")\
        .di("Como afuera hay aire, la presión atmosférica se cancela y trabajamos con presión manométrica usando la gravedad de la hoja: nueve coma ochenta.",
            DATO("820 kg/m"),
            F(HIDRO), F(MAN),
            M(rf"p_{{\text{{sup}}}} = \rho_a g h_a = (820)(9{{,}}80)(0{{,}}80) = {tex(p_sup, 5)}\ \text{{Pa}} = {tex(p_sup / 1e3, 3)}\ \text{{kPa}}"),
            M(rf"p_{{\text{{inf}}}} = 6429 + (1000)(9{{,}}80)(1{{,}}20) = 6429 + 11\,760 = {tex(p_inf, 5)}\ \text{{Pa}} = {tex(p_inf / 1e3, 4)}\ \text{{kPa}}"),
            R(rf"p(y) = {tex(p_sup, 5)} + 9800\,y\ \text{{Pa}}\quad (0 \le y \le 1{{,}}20\ \text{{m}})"))
    L.paso("c) Fuerza hidrostática neta por integración")\
        .di("Tomamos una franja horizontal de ancho cero coma sesenta y alto de ye e integramos presión por área desde cero hasta uno coma veinte metros.",
            F(F_INT), F(PRES),
            M(r"F_\perp = \int_{0}^{1{,}20} p(y)\,b\,dy = 0{,}60\int_{0}^{1{,}20}(6428{,}8 + 9800y)\,dy"),
            M(r"F_\perp = 0{,}60\Bigl[6428{,}8y + 4900y^2\Bigr]_{0}^{1{,}20} = 0{,}60(7714{,}6 + 7056{,}0)"),
            M(rf"F_\perp = 0{{,}}60(14\,770{{,}}6) = {tex(f_net, 5)}\ \text{{N}}"),
            R(rf"F_\perp = {tex(f_net, 4)}\ \text{{N}} = {tex(f_net / 1e3, 3)}\ \text{{kN}}"))
    L.paso("d) Torque respecto al borde superior y profundidad y_cp")\
        .di("Integramos el brazo ye por cada fuerza diferencial desde la casilla de torque y dividimos el torque total entre la fuerza neta.",
            F(TORQ),
            M(r"\tau_0 = \int_{0}^{1{,}20} y\,p(y)\,b\,dy = 0{,}60\int_{0}^{1{,}20}(6428{,}8y + 9800y^2)\,dy"),
            M(rf"\tau_0 = 0{{,}}60\left[3214{{,}}4(1{{,}}20)^2 + \frac{{9800}}{{3}}(1{{,}}20)^3\right] = 0{{,}}60(4628{{,}}7 + 5644{{,}}8) = {tex(tau0, 5)}\ \text{{N}}\cdot\text{{m}}"),
            M(rf"y_{{cp}} = \frac{{\tau_0}}{{F_\perp}} = \frac{{{tex(tau0, 5)}}}{{{tex(f_net, 5)}}} = {tex(y_cp, 4)}\ \text{{m}}"),
            R(rf"y_{{cp}} = {tex(y_cp, 3)}\ \text{{m}}"),
            NOTA("Con $g = 9{,}80\\ \\text{m/s}^2$ (hoja oficial) da $8862\\ \\text{N}$; con $9{,}81$ (solucionario) da $8871\\ \\text{N}$ ($0{,}1\\,\\%$ de diferencia), y $y_{cp} = 0{,}696\\ \\text{m}$ es idéntico."))
    L.usa(HIDRO, MAN, F_INT, PRES, TORQ)
    L.responde("a-sup", rf"p_{{\text{{sup}}}} = {tex(p_sup / 1e3, 3)}\ \text{{kPa}}", p_sup / 1e3, "kPa")
    L.responde("a-inf", rf"p_{{\text{{inf}}}} = {tex(p_inf / 1e3, 4)}\ \text{{kPa}}", p_inf / 1e3, "kPa")
    L.responde("c", rf"F_\perp = {tex(f_net, 4)}\ \text{{N}}", f_net, "N")
    L.responde("d", rf"y_{{cp}} = {tex(y_cp, 3)}\ \text{{m}}", y_cp, "m")
    return L.oficial("Recomendados 2 II-2026", 6.44, 18.21, 8871.38, 0.696)


# ============================================================================ E2-8
@leccion("E2-8")
def e2_8() -> Leccion:
    rho1, h1, rho2, R_m = 850.0, 0.30, 1000.0, 0.30
    p0 = rho1 * G * h1                      # 2499.0 Pa
    A_semi = 0.5 * math.pi * R_m ** 2       # 0.14137 m^2
    F1 = p0 * A_semi                        # 353.29 N
    F2 = (2.0 / 3.0) * rho2 * G * R_m ** 3  # 176.40 N
    F_tot = F1 + F2                         # 529.69 N

    L = Leccion(
        "E2-8",
        "Puerta semicircular sumergida bajo aceite y agua (Recomendados 2, E8)",
        tema="fluidos", tipo="desarrollo", nivel=4, parcial=2, fuente=_f(8),
        enunciado=(
            "Un tanque contiene $0{,}30\\ \\text{m}$ de aceite ($\\rho_1 = 850\\ \\text{kg/m}^3$) sobre $0{,}60\\ \\text{m}$ de agua "
            "($\\rho_2 = 1000\\ \\text{kg/m}^3$). En la pared sumergida en agua hay una puerta semicircular de radio $R = 0{,}30\\ \\text{m}$ "
            "con borde recto horizontal en la interfaz aceite–agua y arco hacia abajo. Determine la fuerza hidrostática total."
        ),
        claves=["puerta semicircular", "integral", "sustitución u", "dos líquidos", "hidrostática", "recomendados"],
    )
    L.paso("Presión manométrica y ancho de franja en el semicírculo")\
        .di("Medimos ye hacia abajo desde la interfaz. Por la ecuación del círculo, una franja a profundidad ye tiene ancho dos por raíz de erre cuadrado menos ye cuadrado.",
            DATO("semicírculo"),
            F(HIDRO), F(PRES),
            M(rf"p_0 = \rho_1 g h_1 = (850)(9{{,}}80)(0{{,}}30) = {tex(p0, 4)}\ \text{{Pa}}"),
            M(r"p(y) = p_0 + \rho_2 g y,\qquad dA = 2\sqrt{R^2 - y^2}\,dy"))
    L.paso("Separar la integral en dos aportes F₁ y F₂")\
        .di("Planteamos la fuerza integral del formulario y la dividimos en dos partes: la presión constante por el área del semicírculo y la integral del agua por sustitución.",
            F(F_INT),
            M(r"F_\perp = \int_{0}^{R}(p_0 + \rho_2 gy)\,2\sqrt{R^2 - y^2}\,dy = F_1 + F_2"),
            M(rf"F_1 = p_0\left(\frac{{\pi R^2}}{{2}}\right) = (2499)\left(\frac{{\pi\cdot 0{{,}}30^2}}{{2}}\right) = (2499)({tex(A_semi, 4)}) = {tex(F1, 4)}\ \text{{N}}"))\
        .di("En la segunda integral hacemos u igual a erre cuadrado menos ye cuadrado, cuyo diferencial trae justo menos dos ye de ye.",
            M(r"F_2 = 2\rho_2 g\int_{0}^{R} y\sqrt{R^2 - y^2}\,dy = \rho_2 g\left[-\frac{2}{3}(R^2 - y^2)^{3/2}\right]_{0}^{R} = \frac{2}{3}\rho_2 g R^3"),
            M(rf"F_2 = \frac{{2}}{{3}}(1000)(9{{,}}80)(0{{,}}30)^3 = {tex(F2, 4)}\ \text{{N}}"))
    L.paso("Suma total")\
        .di("Sumamos la contribución de la columna de aceite más la contribución propia del agua sobre el semicírculo.",
            M(rf"F_\perp = {tex(F1, 4)} + {tex(F2, 4)} = {tex(F_tot, 4)}\ \text{{N}}"),
            R(rf"F_\perp = {tex(F_tot, 3)}\ \text{{N}}"))
    L.usa(HIDRO, PRES, F_INT)
    L.responde("F", rf"F_\perp = {tex(F_tot, 3)}\ \text{{N}}", F_tot, "N")
    return L.oficial("Recomendados 2 II-2026", 530.2)


# ============================================================================ E2-9
@leccion("E2-9")
def e2_9() -> Leccion:
    h, L_m, th = 10.0, 2.00, 30.0
    yB = L_m * math.sin(math.radians(th))      # 1.00 m
    vB = math.sqrt(2.0 * G * (h - yB))         # 13.2816 m/s
    vBy = vB * math.sin(math.radians(th))      # 6.6408 m/s
    dy = vBy ** 2 / (2.0 * G)                  # 2.25 m
    y_max = yB + dy                            # 3.25 m

    L = Leccion(
        "E2-9",
        "Chorro inclinado a 30° desde un tanque: Bernoulli y proyectil (Recomendados 2, E9)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(9),
        enunciado=(
            "Un tanque de agua de altura $h = 10{,}0\\ \\text{m}$ tiene abajo un tubo inclinado de largo $L = 2{,}00\\ \\text{m}$ "
            "a $\\theta = 30{,}0^\\circ$ (Figura 5), con área en la superficie $A$ mucho mayor que la boquilla $B$. "
            "¿Cuál es la altura máxima respecto a la base que alcanza el chorro?"
        ),
        claves=["Bernoulli", "Torricelli", "proyectil", "chorro inclinado", "recomendados"],
    )
    L.paso("Demostrar la rapidez de salida v_B desde Bernoulli")\
        .di("Aplicamos Bernoulli entre la superficie A (rapidez casi cero) y la boquilla B, ambas abiertas a la atmósfera. Así demostramos Torricelli sin sacarlo de memoria.",
            DATO("10,0 m"),
            F(BERN),
            M(rf"y_B = L\sen 30{{,}}0^\circ = (2{{,}}00)(0{{,}}500) = {tex(yB, 3)}\ \text{{m}}"),
            M(r"\rho g h = \rho g y_B + \frac{1}{2}\rho v_B^2 \;\Rightarrow\; v_B = \sqrt{2g(h - y_B)}"),
            M(rf"v_B = \sqrt{{2(9{{,}}80)(10{{,}}0 - 1{{,}}00)}} = \sqrt{{176{{,}}4}} = {tex(vB, 4)}\ \text{{m/s}}"))
    L.paso("Subida del chorro como proyectil y altura total")\
        .di("La componente vertical al salir es ve B por seno de treinta grados. En el punto más alto la velocidad vertical es cero, y sumamos la altura inicial de la boquilla.",
            M(rf"v_{{By}} = v_B\sen 30{{,}}0^\circ = ({tex(vB, 4)})(0{{,}}500) = {tex(vBy, 3)}\ \text{{m/s}}"),
            M(rf"\Delta y_{{\text{{máx}}}} = \frac{{v_{{By}}^2}}{{2g}} = \frac{{({tex(vBy, 3)})^2}}{{2(9{{,}}80)}} = {tex(dy, 3)}\ \text{{m}}"),
            M(rf"y_{{\text{{máx}}}} = y_B + \Delta y_{{\text{{máx}}}} = 1{{,}}00 + 2{{,}}25 = {tex(y_max, 3)}\ \text{{m}}"),
            R(rf"y_{{\text{{máx}}}} = {tex(y_max, 3)}\ \text{{m}}"))
    L.usa(BERN)
    L.responde("y_max", rf"y_{{\text{{máx}}}} = {tex(y_max, 3)}\ \text{{m}}", y_max, "m")
    return L.oficial("Recomendados 2 II-2026", 3.25)


# ============================================================================ E2-10
@leccion("E2-10")
def e2_10() -> Leccion:
    rho, Q = 1000.0, 0.0250
    y_pozo = -2.00
    r_tub = 0.0500
    A_tub = math.pi * r_tub ** 2         # 7.854e-3 m^2
    v_tub = Q / A_tub                    # 3.1831 m/s
    p_suc = rho * (G * y_pozo - 0.5 * v_tub ** 2)  # -24666 Pa
    V_tanq, r_tanq = 5.00, 1.00
    h_tanq = V_tanq / (math.pi * r_tanq ** 2)      # 1.5915 m
    y_tanq = 2.50 + h_tanq                         # 4.0915 m
    p_imp = rho * G * y_tanq                       # 40097 Pa
    dt_ll = V_tanq / Q                             # 200 s
    v_sal = math.sqrt(2.0 * G * y_tanq)            # 8.955 m/s

    L = Leccion(
        "E2-10",
        "Sistema de riego con pozo, bomba hidráulica y tanque en torre (Recomendados 2, E10)",
        tema="fluidos", tipo="desarrollo", nivel=4, parcial=2, fuente=_f(10),
        enunciado=(
            "Una bomba al nivel del suelo extrae $Q = 25{,}0\\ \\text{L/s}$ de un pozo abierto de $7{,}00\\ \\text{m}$ de profundidad "
            "(nivel del agua a $5{,}00\\ \\text{m}$ del fondo, es decir $2{,}00\\ \\text{m}$ bajo el suelo) por tubería de $D = 10{,}0\\ \\text{cm}$ "
            "para llenar un tanque abierto ($V = 5{,}00\\ \\text{m}^3$, $D = 2{,}00\\ \\text{m}$) sobre una torre de $2{,}50\\ \\text{m}$. "
            "Determine: a) presión manométrica de succión; b) presión manométrica de impulsión; "
            "c) tiempo de llenado; d) velocidad de salida a nivel del suelo con el tanque lleno."
        ),
        claves=["bomba", "Bernoulli", "succión", "impulsión", "caudal", "Torricelli", "recomendados"],
    )
    L.paso("a) Rapidez en tubería y presión manométrica de succión")\
        .di("Sacamos la rapidez desde el caudal en la tubería de radio cinco centímetros; la superficie del pozo queda dos metros bajo el suelo.",
            DATO("25,0 L/s"),
            F(CAUDAL), F(BERN), F(MAN),
            M(rf"v = \frac{{Q}}{{\pi r^2}} = \frac{{0{{,}}0250}}{{\pi(0{{,}}0500)^2}} = {tex(v_tub, 4)}\ \text{{m/s}},\qquad y_{{\text{{pozo}}}} = -2{{,}}00\ \text{{m}}"))\
        .di("Aplicando Bernoulli entre la superficie del pozo y la entrada de la bomba, la presión manométrica resulta negativa por vacío de succión.",
            M(r"p_{\text{suc}} - p_0 = \rho\left(gy_{\text{pozo}} - \frac{1}{2}v^2\right)"),
            M(rf"p_{{\text{{suc}}}} - p_0 = 1000\bigl((9{{,}}80)(-2{{,}}00) - 0{{,}}5(3{{,}}183)^2\bigr) = 1000(-19{{,}}60 - 5{{,}}07) = {tex(p_suc, 4)}\ \text{{Pa}}"),
            R(rf"p_{{\text{{man,suc}}}} = {tex(p_suc / 1e3, 3)}\ \text{{kPa}}"))
    L.paso("b y c) Presión manométrica de impulsión y tiempo de llenado")\
        .di("Hallamos la altura del tanque dividiendo volumen entre su área circular, sumamos los dos coma cinco metros de la torre y aplicamos Bernoulli desde la salida de la bomba hasta arriba.",
            M(rf"h_{{\text{{tanque}}}} = \frac{{5{{,}}00}}{{\pi(1{{,}}00)^2}} = {tex(h_tanq, 4)}\ \text{{m}} \;\Rightarrow\; y_{{\text{{tanque}}}} = 2{{,}}50 + 1{{,}}592 = {tex(y_tanq, 4)}\ \text{{m}}"),
            M(rf"p_{{\text{{man,imp}}}} = \rho g y_{{\text{{tanque}}}} = (1000)(9{{,}}80)({tex(y_tanq, 4)}) = {tex(p_imp, 4)}\ \text{{Pa}}"),
            M(rf"\Delta t = \frac{{V}}{{Q}} = \frac{{5{{,}}00}}{{0{{,}}0250}} = {tex(dt_ll, 3)}\ \text{{s}}"),
            R(rf"p_{{\text{{man,imp}}}} = {tex(p_imp / 1e3, 3)}\ \text{{kPa}},\qquad \Delta t = {tex(dt_ll, 3)}\ \text{{s}}"))
    L.paso("d) Rapidez de salida al nivel del suelo")\
        .di("Con el tanque lleno, la superficie está a cuatro coma cero nueve metros sobre la salida. Por Bernoulli demostramos raíz de dos ge por altura total.",
            M(r"\rho g y_{\text{tanque}} = \frac{1}{2}\rho v_{\text{salida}}^2 \;\Rightarrow\; v_{\text{salida}} = \sqrt{2g y_{\text{tanque}}}"),
            M(rf"v_{{\text{{salida}}}} = \sqrt{{2(9{{,}}80)({tex(y_tanq, 4)})}} = \sqrt{{80{{,}}19}} = {tex(v_sal, 3)}\ \text{{m/s}}"),
            R(rf"v_{{\text{{salida}}}} = {tex(v_sal, 3)}\ \text{{m/s}}"),
            NOTA("Nota: el solucionario imprimió $2{,}50\\ \\text{m}$ dentro de la raíz por errata de escritura, pero su resultado $8{,}96\\ \\text{m/s}$ usa la altura total correcta $4{,}09\\ \\text{m}$."))
    L.usa(CAUDAL, BERN, MAN)
    L.responde("a", rf"p_{{\text{{man,suc}}}} = {tex(p_suc / 1e3, 3)}\ \text{{kPa}}", p_suc / 1e3, "kPa")
    L.responde("b", rf"p_{{\text{{man,imp}}}} = {tex(p_imp / 1e3, 3)}\ \text{{kPa}}", p_imp / 1e3, "kPa")
    L.responde("c", rf"\Delta t = {tex(dt_ll, 3)}\ \text{{s}}", dt_ll, "s")
    L.responde("d", rf"v_{{\text{{salida}}}} = {tex(v_sal, 3)}\ \text{{m/s}}", v_sal, "m/s")
    return L.oficial("Recomendados 2 II-2026", -24.7, 40.1, 200.0, 8.96)


# ============================================================================ E2-11
@leccion("E2-11")
def e2_11() -> Leccion:
    p1, p2, v1, rho = 1.20e4, 1.15e4, 0.300, 1060.0
    dp = p1 - p2                            # 500 Pa
    v2 = math.sqrt(v1 ** 2 + 2.0 * dp / rho)  # 1.0166 m/s
    frac_libre = v1 / v2                    # 0.2951
    pct_bloq = (1.0 - frac_libre) * 100.0   # 70.49 %

    L = Leccion(
        "E2-11",
        "Porcentaje de arteria bloqueada por una placa (Bernoulli y continuidad) (Recomendados 2, E11)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(11),
        enunciado=(
            "Antes de un bloqueo en una arteria horizontal, la presión sanguínea es $1{,}20\\times 10^4\\ \\text{Pa}$ "
            "y la rapidez es $30{,}0\\ \\text{cm/s}$; en la zona bloqueada la presión baja a $1{,}15\\times 10^4\\ \\text{Pa}$. "
            "Con $\\rho = 1060\\ \\text{kg/m}^3$ (fluido ideal), ¿qué porcentaje del área transversal está bloqueado?"
        ),
        claves=["Bernoulli", "continuidad", "arteria", "estenosis", "recomendados"],
    )
    L.paso("Rapidez en el estrechamiento con Bernoulli")\
        .di("Como la arteria es horizontal los términos de altura se cancelan. La caída de presión es quinientos pascales, y de Bernoulli despejamos ve dos.",
            DATO("30,0 cm/s"),
            F(BERN),
            M(r"p_1 - p_2 = 1{,}20\times 10^4 - 1{,}15\times 10^4 = 500\ \text{Pa}"),
            M(rf"v_2 = \sqrt{{v_1^2 + \frac{{2(p_1 - p_2)}}{{\rho}}}} = \sqrt{{0{{,}}300^2 + \frac{{2(500)}}{{1060}}}} = \sqrt{{0{{,}}0900 + 0{{,}}9434}} = {tex(v2, 5)}\ \text{{m/s}}"))
    L.paso("Fracción libre y porcentaje bloqueado")\
        .di("Por continuidad, la razón de áreas es inversa a la razón de rapideces. El área libre es veintinueve coma cinco por ciento, así que el resto está bloqueado.",
            F(CONT),
            M(rf"\frac{{A_2}}{{A_1}} = \frac{{v_1}}{{v_2}} = \frac{{0{{,}}300}}{{{tex(v2, 5)}}} = {tex(frac_libre, 4)} = 29{{,}}51\,\%"),
            M(rf"\text{{Bloqueado}} = 100\,\% - 29{{,}}51\,\% = {tex(pct_bloq, 4)}\,\%"),
            R(rf"\text{{Área bloqueada}} = {tex(pct_bloq, 4)}\,\%"),
            NOTA("Nota: el solucionario escribió por errata $1{,}05\\times 10^4\\ \\text{Pa}$ en vez de $500\\ \\text{Pa}$, pero calculó con $500\\ \\text{Pa}$ llegando al mismo $70{,}49\\,\\%$."))
    L.usa(BERN, CONT)
    L.responde("bloqueo", rf"\text{{Bloqueado}} = {tex(pct_bloq, 4)}\,\%", pct_bloq, "%")
    return L.oficial("Recomendados 2 II-2026", 70.49)


# ============================================================================ E2-12
@leccion("E2-12")
def e2_12() -> Leccion:
    factor = 1.0 / (0.90 ** 4)          # 1.52416
    pct = (factor - 1.0) * 100.0        # 52.42 %

    L = Leccion(
        "E2-12",
        "Reducción del 10 % en el radio arterial y aumento de presión por Poiseuille (Recomendados 2, E12)",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2, fuente=_f(12),
        enunciado=(
            "El radio de un segmento de $100\\ \\text{mm}$ de una arteria radial (diámetro original $3\\ \\text{mm}$) "
            "se redujo un $10\\,\\%$ a lo largo de todo el tramo. a) ¿Debe usarse Bernoulli o Poiseuille? "
            "b) ¿En qué porcentaje debe aumentar la caída de presión $\\Delta p$ para mantener el mismo caudal?"
        ),
        claves=["Poiseuille", "viscosidad", "cuarta potencia", "arteria", "recomendados"],
    )
    L.paso("a) Por qué aplica la ley de Poiseuille")\
        .di("En una arteria real de milímetros de diámetro la sangre es viscosa y disipa energía por fricción, así que Bernoulli no aplica y debemos usar Poiseuille.",
            DATO("reducido un 10 %"),
            F(POIS),
            R(r"\text{Debe usarse la ley de Poiseuille (fluido real viscoso)}"))
    L.paso("b) Incremento porcentual de Δp al reducir el radio 10 %")\
        .di("Despejamos la caída de presión con caudal constante: es inversamente proporcional al radio a la cuarta potencia, así que bajar diez por ciento el radio exige más de cincuenta por ciento extra de presión.",
            M(r"\Delta p = \frac{8\eta L Q}{\pi R^4} \;\Rightarrow\; \frac{\Delta p'}{\Delta p} = \left(\frac{R}{0{,}90 R}\right)^4 = \frac{1}{0{,}90^4}"),
            M(rf"\frac{{\Delta p'}}{{\Delta p}} = \frac{{1}}{{0{{,}}6561}} = {tex(factor, 5)} \;\Rightarrow\; \text{{incremento}} = ({tex(factor, 5)} - 1)\times 100\,\% = {tex(pct, 4)}\,\%"),
            R(rf"\text{{Incremento de }}\Delta p = +{tex(pct, 4)}\,\%"))
    L.usa(POIS)
    L.responde("b", rf"+{tex(pct, 4)}\,\%", pct, "%")
    return L.oficial("Recomendados 2 II-2026", 52.42)


# ============================================================================ E2-13
@leccion("E2-13")
def e2_13() -> Leccion:
    r, rho, eta = 2.0e-3, 1060.0, 2.1e-3
    D = 2.0 * r                         # 4.0e-3 m
    v_max = 2000.0 * eta / (rho * D)    # 0.99057 m/s
    A = math.pi * r ** 2                # 1.2566e-5 m^2
    Q = A * v_max                       # 1.2448e-5 m^3/s

    L = Leccion(
        "E2-13",
        "Rapidez máxima laminar y caudal en un vaso sanguíneo (Recomendados 2, E13)",
        tema="fluidos", tipo="desarrollo", nivel=2, parcial=2, fuente=_f(13),
        enunciado=(
            "La sangre ($\\eta = 2{,}1\\times 10^{-3}\\ \\text{Pa}\\cdot\\text{s}$, $\\rho = 1060\\ \\text{kg/m}^3$) "
            "fluye por un vaso de radio $2\\ \\text{mm}$. Determine: a) la rapidez máxima para que el flujo siga siendo laminar "
            "($N_R = 2000$); b) el caudal correspondiente."
        ),
        claves=["Reynolds", "flujo laminar", "caudal", "sangre", "recomendados"],
    )
    L.paso("a) Velocidad límite laminar desde el número de Reynolds")\
        .di("El límite laminar se fija en Reynolds igual a dos mil. Ojo con el diámetro: es el doble del radio, o sea cuatro milímetros.",
            DATO("2 mm de radio"),
            F(REYN),
            M(r"D = 2r = 4{,}0\times 10^{-3}\ \text{m} \;\Rightarrow\; v_{\text{máx}} = \frac{N_R\,\eta}{\rho D}"),
            M(rf"v_{{\text{{máx}}}} = \frac{{(2000)(2{{,}}1\times 10^{{-3}})}}{{(1060)(4{{,}}0\times 10^{{-3}})}} = \frac{{4{{,}}20}}{{4{{,}}24}} = {tex(v_max, 4)}\ \text{{m/s}}"),
            R(rf"v_{{\text{{máx}}}} = {tex(v_max, 3)}\ \text{{m/s}}"))
    L.paso("b) Caudal volumétrico correspondiente")\
        .di("Multiplicamos el área transversal circular del vaso por la rapidez máxima recién calculada.",
            F(CAUDAL),
            M(rf"A = \pi(2{{,}}0\times 10^{{-3}})^2 = {tex(A, 4)}\ \text{{m}}^2"),
            M(rf"Q = Av_{{\text{{máx}}}} = ({tex(A, 4)})({tex(v_max, 4)}) = {tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"),
            R(rf"Q = {tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"))
    L.usa(REYN, CAUDAL)
    L.responde("a", rf"v_{{\text{{máx}}}} = {tex(v_max, 3)}\ \text{{m/s}}", v_max, "m/s")
    L.responde("b", rf"Q = {tex(Q, 4)}\ \text{{m}}^3/\text{{s}}", Q, "m³/s")
    return L.oficial("Recomendados 2 II-2026", 0.991, 1.245e-5)


# ============================================================================ E2-14
@leccion("E2-14")
def e2_14() -> Leccion:
    V_air, dt = 0.50e-3, 2.0
    Q = V_air / dt                      # 2.5e-4 m^3/s
    D, L_m = 0.020, 0.10
    r = D / 2.0                         # 0.010 m
    A = math.pi * r ** 2                # 3.1416e-4 m^2
    v = Q / A                           # 0.7958 m/s
    rho_a, eta_a = 1.0, 1.7e-5
    nr = rho_a * v * D / eta_a          # 936.2
    dp = 8.0 * eta_a * L_m * Q / (math.pi * r ** 4)  # 0.1082 Pa

    L = Leccion(
        "E2-14",
        "Flujo de aire en la tráquea: caudal, rapidez, Reynolds y Poiseuille (Recomendados 2, E14)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(14),
        enunciado=(
            "En reposo se inhalan $0{,}50\\ \\text{L}$ de aire en $2{,}0\\ \\text{s}$ por una tráquea cilíndrica de "
            "$D = 2{,}0\\ \\text{cm}$ y $L = 0{,}10\\ \\text{m}$ ($\\rho_a = 1{,}0\\ \\text{kg/m}^3$, "
            "$\\eta_a = 1{,}7\\times 10^{-5}\\ \\text{Pa}\\cdot\\text{s}$). Halle: a) el caudal; b) la rapidez media; "
            "c) el número de Reynolds y el régimen; d) la caída de presión necesaria."
        ),
        claves=["tráquea", "caudal", "Reynolds", "Poiseuille", "flujo laminar", "recomendados"],
    )
    L.paso("a y b) Caudal volumétrico y velocidad media")\
        .di("Convertimos medio litro a cinco por diez a la menos cuatro metros cúbicos y dividimos entre dos segundos. Luego dividimos el caudal entre el área de la tráquea.",
            DATO("0,50 L"),
            F(CAUDAL),
            M(rf"Q = \frac{{0{{,}}50\times 10^{{-3}}}}{{2{{,}}0}} = {tex(Q, 2)}\ \text{{m}}^3/\text{{s}}"),
            M(rf"A = \pi(0{{,}}010)^2 = {tex(A, 4)}\ \text{{m}}^2 \;\Rightarrow\; v = \frac{{{tex(Q, 2)}}}{{{tex(A, 4)}}} = {tex(v, 4)}\ \text{{m/s}}"),
            R(rf"Q = {tex(Q, 2)}\ \text{{m}}^3/\text{{s}},\qquad v = {tex(v, 4)}\ \text{{m/s}}"))
    L.paso("c y d) Número de Reynolds y caída de presión por Poiseuille")\
        .di("Calculamos Reynolds con diámetro cero coma cero dos metros: da novecientos treinta y seis, menor que dos mil, por lo que el flujo es laminar y podemos aplicar Poiseuille.",
            F(REYN), F(POIS),
            M(rf"N_R = \frac{{(1{{,}}0)(0{{,}}7958)(0{{,}}020)}}{{1{{,}}7\times 10^{{-5}}}} = {tex(nr, 3)} < 2000\ \text{{(laminar)}}"),
            M(rf"\Delta p = \frac{{8\eta L Q}}{{\pi R^4}} = \frac{{8(1{{,}}7\times 10^{{-5}})(0{{,}}10)(2{{,}}5\times 10^{{-4}})}}{{\pi(0{{,}}010)^4}} = \frac{{3{{,}}4\times 10^{{-9}}}}{{3{{,}}142\times 10^{{-8}}}} = {tex(dp, 3)}\ \text{{Pa}}"),
            R(rf"N_R = {tex(nr, 3)}\ \text{{(laminar)}},\qquad \Delta p = {tex(dp, 3)}\ \text{{Pa}}"))
    L.usa(CAUDAL, REYN, POIS)
    L.responde("a", rf"Q = {tex(Q, 2)}\ \text{{m}}^3/\text{{s}}", Q, "m³/s")
    L.responde("b", rf"v = {tex(v, 4)}\ \text{{m/s}}", v, "m/s")
    L.responde("c", rf"N_R = {tex(nr, 3)}", nr, "")
    L.responde("d", rf"\Delta p = {tex(dp, 3)}\ \text{{Pa}}", dp, "Pa")
    return L.oficial("Recomendados 2 II-2026", 2.5e-4, 0.7958, 936.0, 0.108)
