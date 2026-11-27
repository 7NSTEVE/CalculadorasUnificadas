"""Ejercicios Recomendados de Cátedra 1 · FI2103 II-2026 (Parcial 1: MAS y ondas, 16 lecciones).

Fuentes: `Ejercicios_Recomendados_1_IIS2026.pdf` y `Ejercicios_Recomendados_1_IIS2026_Solucion.pdf`
(carpeta oficial «Documentos II - 2026»). Cada desarrollo parte estrictamente de las casillas
oficiales del formulario del Parcial 1 (`formularios_p1_p2.py`) y calcula todos los números en Python.

IDs: E1-1 .. E1-16 (`parcial=1`, `tema="ondas"`).
"""

from __future__ import annotations

import math

from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA
from .formato import partes, tex
from .registro import leccion

G = 9.80

# Casillas exactas del formulario oficial Parcial 1
HOOKE = r"F_x = -kx"
A_RES = r"a = -\frac{k}{m}x"
A_MAS = r"a = -\omega^2 x"
W_RES = r"\omega^2 = \frac{k}{m}"
POS = r"x = A\cos(\omega t + \phi)"
VEL = r"v = -\omega A\sen(\omega t + \phi)"
W_F = r"\omega = 2\pi f"
PERIODO = r"T = \frac{2\pi}{\omega}"
AMP = r"A = \sqrt{x^2 + \frac{v^2}{\omega^2}}"
FASE = r"\tan(\omega t + \phi) = -\frac{v}{\omega x}"
W_SIMPLE = r"\omega^2 = \frac{g}{L}"
W_FISICO = r"\omega^2 = \frac{mgd}{I}"
EJES = r"I_{\text{eje}\parallel} = I_{\text{cm}} + md^2"
INT_ESF = r"I = \frac{P}{4\pi r^2}"
INV_CUAD = r"\frac{I_1}{I_2} = \frac{r_2^2}{r_1^2}"
E_TOT = r"E = \frac{1}{2}kA^2"
E_SUMA = r"E = K + U = \frac{1}{2}mv^2 + \frac{1}{2}kx^2"
P_MED = r"P_{\text{med}} = \frac{1}{2}\sqrt{\mu F}\,\omega^2 A^2"
VIAJERA = r"y(x,t) = A\cos(kx - \omega t)"
VIAJERA_PM = r"y(x,t) = A\cos(kx \pm \omega t)"
ESTAC = r"y(x,t) = A_{\text{OE}}\sen(kx)\sen(\omega t)"
V_ONDA = r"v = \lambda f = \frac{\omega}{k}"
K_ONDA = r"k = \frac{2\pi}{\lambda}"
MU = r"\mu = \frac{m}{L}"
V_CUERDA = r"v = \sqrt{\frac{F}{\mu}}"
LAM_N = r"\lambda_n = \frac{2L}{n}"
F_N = r"f_n = \frac{v}{\lambda_n}"
ARM = r"f_n = n f_1"


def _f(num: int) -> dict:
    return {"doc": "recomendados1", "nombre": "Ejercicios Recomendados 1 (II-2026)", "num": str(num), "parcial": 1}


def _q(x: float, c: int = 4) -> float:
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ E1-1
@leccion("E1-1")
def e1_1() -> Leccion:
    m, k, x0, v0 = 0.500, 200.0, 0.080, -1.50
    w = math.sqrt(k / m)             # 20.0 rad/s
    f = w / (2.0 * math.pi)          # 3.183 Hz
    T = 2.0 * math.pi / w            # 0.3142 s
    A = math.sqrt(x0 ** 2 + (v0 / w) ** 2)  # 0.10966 m
    tan_phi = -v0 / (w * x0)         # 0.9375
    phi = math.atan(tan_phi)         # 0.7532 rad
    vmax = w * A                     # 2.193 m/s
    amax = w * w * A                 # 43.86 m/s^2

    L = Leccion(
        "E1-1",
        "Masa-resorte con condiciones iniciales: amplitud, fase y cinemática (Recomendados 1, E1)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(1),
        enunciado=(
            "Un bloque de masa $m = 0{,}500\\ \\text{kg}$ unido a un resorte ideal horizontal de $k = 200\\ \\text{N/m}$ "
            "parte en $t = 0\\ \\text{s}$ desde $x_0 = +8{,}0\\ \\text{cm}$ moviéndose hacia la izquierda con rapidez $1{,}50\\ \\text{m/s}$. "
            "Determine: a) la amplitud $A$; b) $\\omega$, $f$ y $T$; c) la ecuación $x(t)$; "
            "d) $v_{\\text{máx}}$ y $a_{\\text{máx}}$; e) las ecuaciones $v(t)$ y $a(t)$."
        ),
        claves=["MAS", "amplitud", "fase inicial", "resorte", "recomendados"],
    )
    L.paso("b) Frecuencia angular, frecuencia y período")\
        .di("Primero sacamos omega con la casilla del sistema masa-resorte, porque la necesitaremos también para hallar la amplitud y la fase.",
            DATO("200 N/m"),
            F(W_RES), F(W_F), F(PERIODO),
            M(rf"\omega = \sqrt{{\frac{{k}}{{m}}}} = \sqrt{{\frac{{200}}{{0{{,}}500}}}} = \sqrt{{400}} = {tex(w, 3)}\ \text{{rad/s}}"),
            M(rf"f = \frac{{\omega}}{{2\pi}} = \frac{{20{{,}}0}}{{2\pi}} = {tex(f, 3)}\ \text{{Hz}},\qquad T = \frac{{2\pi}}{{20{{,}}0}} = {tex(T, 3)}\ \text{{s}}"),
            R(rf"\omega = {tex(w, 3)}\ \text{{rad/s}},\quad f = {tex(f, 3)}\ \text{{Hz}},\quad T = {tex(T, 3)}\ \text{{s}}"))
    L.paso("a) Amplitud desde x₀ y v₀")\
        .di("Pasamos ocho centímetros a cero coma cero ocho metros y ponemos signo negativo a la velocidad por ir hacia la izquierda.",
            F(AMP),
            M(r"\frac{v_0}{\omega} = \frac{-1{,}50}{20{,}0} = -0{,}0750\ \text{m}"),
            M(rf"A = \sqrt{{(0{{,}}080)^2 + (-0{{,}}0750)^2}} = \sqrt{{0{{,}}012025}} = {tex(A, 4)}\ \text{{m}}"),
            R(rf"A = {tex(A, 3)}\ \text{{m}}"))
    L.paso("c) Fase inicial y ecuación de posición x(t)")\
        .di("Usamos la casilla de tangente de la fase en te igual a cero. Como equis es positivo y la velocidad es negativa, el coseno y el seno son positivos: primer cuadrante.",
            F(FASE), F(POS),
            M(rf"\tan\phi = -\frac{{v_0}}{{\omega x_0}} = -\frac{{-1{{,}}50}}{{(20{{,}}0)(0{{,}}080)}} = \frac{{1{{,}}50}}{{1{{,}}60}} = {tex(tan_phi, 4)}"),
            M(rf"\phi = \arctan(0{{,}}9375) = {tex(phi, 3)}\ \text{{rad}}"),
            R(rf"x(t) = 0{{,}}110\cos(20{{,}}0t + 0{{,}}753)\ \text{{m}}"))
    L.paso("d–e) Valores máximos y ecuaciones v(t) y a(t)")\
        .di("Con las casillas de velocidad y aceleración del movimiento armónico simple leemos los máximos y escribimos las funciones completas del tiempo.",
            F(VEL), F(A_MAS),
            M(rf"v_{{\text{{máx}}}} = \omega A = (20{{,}}0)(0{{,}}1097) = {tex(vmax, 3)}\ \text{{m/s}}"),
            M(rf"a_{{\text{{máx}}}} = \omega^2 A = (400)(0{{,}}1097) = {tex(amax, 3)}\ \text{{m/s}}^2"),
            R(rf"v(t) = -2{{,}}19\sen(20{{,}}0t + 0{{,}}753)\ \text{{m/s}},\quad a(t) = -43{{,}}9\cos(20{{,}}0t + 0{{,}}753)\ \text{{m/s}}^2"))
    L.usa(W_RES, W_F, PERIODO, AMP, FASE, POS, VEL, A_MAS)
    L.responde("a", rf"A = {tex(A, 3)}\ \text{{m}}", A, "m")
    L.responde("b-w", rf"\omega = {tex(w, 3)}\ \text{{rad/s}}", w, "rad/s")
    L.responde("c-phi", rf"\phi = {tex(phi, 3)}\ \text{{rad}}", phi, "rad")
    L.responde("d-v", rf"v_{{\text{{máx}}}} = {tex(vmax, 3)}\ \text{{m/s}}", vmax, "m/s")
    L.responde("d-a", rf"a_{{\text{{máx}}}} = {tex(amax, 3)}\ \text{{m/s}}^2", amax, "m/s²")
    return L.oficial("Recomendados 1 II-2026", 0.110, 20.0, 0.753, 2.19, 43.9)


# ============================================================================ E1-2
@leccion("E1-2")
def e1_2() -> Leccion:
    T = 30.0 / 50.0                   # 0.600 s
    d = G * T * T / (4.0 * math.pi ** 2)  # 0.08937 m

    L = Leccion(
        "E1-2",
        "Estiramiento estático d de un resorte vertical desde su período (Recomendados 1, E2)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(2),
        enunciado=(
            "Cuando se cuelga una masa de un resorte vertical, este se estira una distancia $d$. "
            "Al jalar la masa hacia abajo desde el equilibrio y soltarla, realiza $50{,}0$ oscilaciones en $30{,}0\\ \\text{s}$. "
            "¿Cuál es la distancia $d$?"
        ),
        claves=["resorte vertical", "estiramiento estático", "período", "Hooke", "recomendados"],
    )
    L.paso("Equilibrio estático y período medido")\
        .di("En reposo el resorte equilibra el peso de la masa, así que el cociente eme entre ka es igual a de entre ge, sin necesidad de conocer la masa.",
            DATO("50,0 oscilaciones"),
            F(HOOKE),
            M(r"kd = mg \;\Rightarrow\; \frac{m}{k} = \frac{d}{g}"),
            M(rf"T = \frac{{30{{,}}0\ \text{{s}}}}{{50{{,}}0}} = {tex(T, 3)}\ \text{{s}}"))
    L.paso("Conectar con el período del MAS y despejar d")\
        .di("Combinamos las casillas de frecuencia angular y período, sustituimos eme entre ka por de entre ge y despejamos de.",
            F(W_RES), F(PERIODO),
            M(r"T = 2\pi\sqrt{\frac{m}{k}} = 2\pi\sqrt{\frac{d}{g}} \;\Rightarrow\; d = \frac{gT^2}{4\pi^2}"),
            M(rf"d = \frac{{(9{{,}}80)(0{{,}}600)^2}}{{4\pi^2}} = \frac{{3{{,}}528}}{{39{{,}}48}} = {tex(d, 3)}\ \text{{m}}"),
            R(rf"d = {tex(d, 3)}\ \text{{m}} = {tex(d * 100, 3)}\ \text{{cm}}"))
    L.usa(HOOKE, W_RES, PERIODO)
    L.responde("d", rf"d = {tex(d, 3)}\ \text{{m}}", d, "m")
    return L.oficial("Recomendados 1 II-2026", 0.0894)


# ============================================================================ E1-3
@leccion("E1-3")
def e1_3() -> Leccion:
    L = Leccion(
        "E1-3",
        "Bloque entre dos resortes en paralelo: frecuencia natural (Recomendados 1, E3)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(3),
        enunciado=(
            "Dos resortes ideales de constantes $k_1$ y $k_2$ sujetan un bloque de masa $m$ a paredes opuestas "
            "sobre una superficie horizontal sin fricción (Figura 1). Al desplazar el bloque a la derecha y soltarlo, "
            "halle la frecuencia natural $f$ en términos de $m$, $k_1$ y $k_2$."
        ),
        claves=["resortes en paralelo", "ley de Hooke", "frecuencia natural", "simbólico", "recomendados"],
    )
    L.paso("Fuerza neta restauradora y constante equivalente")\
        .di("Si el bloque se desplaza equis a la derecha, el resorte izquierdo se estira y hala hacia la izquierda, mientras el derecho se comprime y también empuja hacia la izquierda.",
            DATO("Dos resortes ideales"),
            F(HOOKE),
            M(r"F_1 = -k_1 x,\qquad F_2 = -k_2 x"),
            M(r"F_{\text{neta}} = F_1 + F_2 = -(k_1 + k_2)x \;\Rightarrow\; k_{\text{eq}} = k_1 + k_2"))
    L.paso("Frecuencia angular y frecuencia natural")\
        .di("Sustituimos la constante equivalente en la casilla del oscilador masa-resorte para obtener omega.",
            F(W_RES), F(W_F),
            M(r"\omega = \sqrt{\frac{k_{\text{eq}}}{m}} = \sqrt{\frac{k_1 + k_2}{m}}"))\
        .di("Dividimos omega entre dos pi y obtenemos la frecuencia natural del bloque en paralelo.",
            M(r"f = \frac{\omega}{2\pi} = \frac{1}{2\pi}\sqrt{\frac{k_1 + k_2}{m}}"),
            R(r"f = \frac{1}{2\pi}\sqrt{\frac{k_1 + k_2}{m}}"))
    L.usa(HOOKE, W_RES, W_F)
    L.responde("f", r"f = \frac{1}{2\pi}\sqrt{\frac{k_1 + k_2}{m}}")
    return L


# ============================================================================ E1-4
@leccion("E1-4")
def e1_4() -> Leccion:
    m_tot = 1.000
    v0 = 5.0 / 3.6              # 1.3889 m/s
    A = 0.200
    w = v0 / A                  # 6.9444 rad/s
    k = m_tot * w * w           # 48.225 N/m
    v_x = w * math.sqrt(A ** 2 - 0.100 ** 2)  # 1.2028 m/s

    L = Leccion(
        "E1-4",
        "Colisión magnética de dos carritos acoplados a un resorte (Recomendados 1, E4)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(4),
        enunciado=(
            "Un carrito de $0{,}500\\ \\text{kg}$ en reposo en $x = 0$ unido a un resorte ideal choca con otro igual "
            "y quedan unidos moviéndose juntos a $5{,}0\\ \\text{km/h}$, oscilando luego en MAS con $A = 0{,}200\\ \\text{m}$. "
            "Determine: a) $\\omega$ y $k$; b) $x(t)$ si en $t = 0$ pasan por $x = 0$ hacia $-x$; "
            "c) los valores posibles del vector velocidad en $x = 0{,}100\\ \\text{m}$; d) el efecto de un resorte real con masa."
        ),
        claves=["colisión", "velocidad máxima", "fase", "resorte real", "recomendados"],
    )
    L.paso("a) Frecuencia angular y constante k")\
        .di("Como el choque ocurre en el equilibrio, esa rapidez de cinco kilómetros por hora es la rapidez máxima del MAS. Pasamos a metros por segundo y despejamos omega y ka.",
            DATO("5,0 km/h"),
            F(VEL), F(W_RES),
            M(rf"v_{{\text{{máx}}}} = \frac{{5{{,}}0}}{{3{{,}}6}} = {tex(v0, 4)}\ \text{{m/s}}"),
            M(rf"\omega = \frac{{v_{{\text{{máx}}}}}}{{A}} = \frac{{{tex(v0, 4)}}}{{0{{,}}200}} = {tex(w, 4)}\ \text{{rad/s}}"),
            M(rf"k = M\omega^2 = (1{{,}}000)({tex(w, 4)})^2 = {tex(k, 3)}\ \text{{N/m}}"),
            R(rf"\omega = {tex(w, 3)}\ \text{{rad/s}},\qquad k = {tex(k, 3)}\ \text{{N/m}}"))
    L.paso("b) Ecuación x(t) con velocidad inicial negativa")\
        .di("En te igual cero el coseno es cero, lo que deja dos opciones para la fase: más o menos pi medios.",
            F(POS),
            M(r"x(0) = A\cos\phi = 0 \;\Rightarrow\; \phi = \pm\frac{\pi}{2}"))\
        .di("Como la velocidad inicial apunta hacia menos equis, el seno de fi debe ser positivo, así que elegimos más pi medios.",
            M(r"v(0) = -\omega A\sen\phi < 0 \;\Rightarrow\; \sen\phi > 0 \;\Rightarrow\; \phi = +\frac{\pi}{2} \approx 1{,}57\ \text{rad}"),
            R(r"x(t) = 0{,}200\cos(6{,}94t + 1{,}57)\ \text{m}"))
    L.paso("c–d) Velocidad en x = 0,100 m y resorte real")\
        .di("Desde la conservación de energía del formulario despejamos la velocidad en función de la posición, con doble signo porque el bloque pasa yendo y volviendo.",
            F(E_SUMA), F(E_TOT),
            M(r"\frac{1}{2}Mv^2 + \frac{1}{2}kx^2 = \frac{1}{2}kA^2 \;\Rightarrow\; v = \pm\omega\sqrt{A^2 - x^2}"),
            M(rf"v = \pm({tex(w, 4)})\sqrt{{0{{,}}200^2 - 0{{,}}100^2}} = \pm({tex(w, 4)})(0{{,}}1732) = \pm {tex(v_x, 3)}\ \text{{m/s}}"),
            R(rf"\vec{{v}} = \pm {tex(v_x, 3)}\,\hat{{i}}\ \text{{m/s}}"))\
        .di("En un resorte real hay que sumar su masa efectiva (un tercio de la masa del resorte) y además aparece amortiguamiento, por lo que omega baja y el período sube.",
            NOTA("Con resorte real: $M_{\\text{ef}} = M + m_{\\text{res}}/3 > M \\Rightarrow \\omega$ disminuye y $T$ aumenta."))
    L.usa(VEL, W_RES, POS, E_SUMA, E_TOT)
    L.responde("a-w", rf"\omega = {tex(w, 3)}\ \text{{rad/s}}", w, "rad/s")
    L.responde("a-k", rf"k = {tex(k, 3)}\ \text{{N/m}}", k, "N/m")
    L.responde("c", rf"|v| = {tex(v_x, 3)}\ \text{{m/s}}", v_x, "m/s")
    return L.oficial("Recomendados 1 II-2026", 6.94, 48.2, 1.20)


# ============================================================================ E1-5
@leccion("E1-5")
def e1_5() -> Leccion:
    m1, m2, k, A1 = 4.00, 6.00, 100.0, 2.00
    M_tot = m1 + m2            # 10.0 kg
    w1 = math.sqrt(k / m1)     # 5.00 rad/s
    T1 = 2.0 * math.pi / w1    # 1.2566 s
    v0 = w1 * A1               # 10.0 m/s
    E1 = 0.5 * k * A1 ** 2     # 200 J
    vp = (m1 / M_tot) * v0     # 4.00 m/s
    w2 = math.sqrt(k / M_tot)  # 3.1623 rad/s
    T2 = 2.0 * math.pi / w2    # 1.9869 s
    A2 = vp / w2               # 1.2649 m
    dA = A2 - A1               # -0.7351 m
    dT = T2 - T1               # +0.7303 s
    E2 = 0.5 * k * A2 ** 2     # 80.0 J
    dE = E2 - E1               # -120.0 J

    L = Leccion(
        "E1-5",
        "Masa que cae sobre oscilador en el equilibrio: ΔA, ΔT y ΔE (Recomendados 1, E5)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(5),
        enunciado=(
            "Una partícula de $m_1 = 4{,}00\\ \\text{kg}$ unida a un resorte de $k = 100\\ \\text{N/m}$ oscila sin fricción "
            "con amplitud $A_1 = 2{,}00\\ \\text{m}$. Al pasar por $x = 0$, un bloque de $m_2 = 6{,}00\\ \\text{kg}$ cae verticalmente "
            "sobre ella y se adhiere. Determine: a) cuánto cambia la amplitud; b) cuánto cambia el período; "
            "c) cuánto cambia la energía mecánica; d) a qué se debe esa variación de energía."
        ),
        claves=["colisión inelástica", "conservación de momento", "amplitud", "energía mecánica", "recomendados"],
    )
    L.paso("Estado antes del impacto y velocidad tras adherirse")\
        .di("Antes del choque sacamos omega uno, el período inicial y la rapidez máxima al pasar por cero. En el impacto horizontal se conserva el momento lineal.",
            DATO("4,00 kg"),
            F(W_RES), F(PERIODO), F(VEL),
            M(rf"\omega_1 = \sqrt{{\frac{{100}}{{4{{,}}00}}}} = 5{{,}}00\ \text{{rad/s}},\qquad v_0 = \omega_1 A_1 = 10{{,}}0\ \text{{m/s}}"),
            M(rf"m_1 v_0 = (m_1 + m_2)v' \;\Rightarrow\; v' = \frac{{4{{,}}00}}{{10{{,}}0}}(10{{,}}0) = {tex(vp, 3)}\ \text{{m/s}}"))
    L.paso("a y b) Nueva amplitud, nuevo período y sus cambios")\
        .di("Con la masa total de diez kilogramos baja omega dos, sube el período y la nueva amplitud sale de ve prima entre omega dos.",
            M(rf"\omega_2 = \sqrt{{\frac{{100}}{{10{{,}}0}}}} = {tex(w2, 4)}\ \text{{rad/s}}"),
            M(rf"A_2 = \frac{{v'}}{{\omega_2}} = \frac{{4{{,}}00}}{{3{{,}}162}} = {tex(A2, 4)}\ \text{{m}} \;\Rightarrow\; \Delta A = 1{{,}}265 - 2{{,}}00 = {tex(dA, 3)}\ \text{{m}}"),
            M(rf"T_1 = \frac{{2\pi}}{{5{{,}}00}} = {tex(T1, 4)}\ \text{{s}},\qquad T_2 = \frac{{2\pi}}{{3{{,}}162}} = {tex(T2, 4)}\ \text{{s}} \;\Rightarrow\; \Delta T = +{tex(dT, 3)}\ \text{{s}}"),
            R(rf"\Delta A = {tex(dA, 3)}\ \text{{m}}\ (A_2 = 0{{,}}632A_1),\qquad \Delta T = +{tex(dT, 3)}\ \text{{s}}\ (T_2 = 1{{,}}581T_1)"))
    L.paso("c y d) Cambio de energía y explicación física")\
        .di("Evaluamos la energía total antes y después con la casilla un medio ka a cuadrado. Se pierde exactamente el sesenta por ciento de la energía mecánica.",
            F(E_TOT),
            M(rf"E_1 = \frac{{1}}{{2}}(100)(2{{,}}00)^2 = {tex(E1, 3)}\ \text{{J}},\qquad E_2 = \frac{{1}}{{2}}(100)(1{{,}}265)^2 = {tex(E2, 3)}\ \text{{J}}"),
            M(rf"\Delta E = 80{{,}}0 - 200 = {tex(dE, 3)}\ \text{{J}}\ \left(\frac{{\Delta E}}{{E_1}} = -60{{,}}0\,\%\right)"),
            R(r"\Delta E = -120\ \text{J}\ (-60{,}0\,\%)"))\
        .di("Como los dos bloques quedan pegados, la colisión es completamente inelástica y esa energía se disipa en calor, sonido y deformación.",
            NOTA("Razón física: choque perfectamente inelástico $\\Rightarrow$ se conserva $p_x$, pero no se conserva la energía mecánica."))
    L.usa(W_RES, PERIODO, VEL, E_TOT)
    L.responde("a", rf"A_2 = {tex(A2, 4)}\ \text{{m}}", A2, "m")
    L.responde("b", rf"T_2 = {tex(T2, 4)}\ \text{{s}}", T2, "s")
    L.responde("c", rf"\Delta E = {tex(dE, 3)}\ \text{{J}}", dE, "J")
    return L.oficial("Recomendados 1 II-2026", 1.265, 1.987, -120.0)


# ============================================================================ E1-6
@leccion("E1-6")
def e1_6() -> Leccion:
    L_m, m_kg, th_deg = 15.0, 110.0, 3.5
    T = 2.0 * math.pi * math.sqrt(L_m / G)              # 7.773 s
    h = L_m * (1.0 - math.cos(math.radians(th_deg)))    # 0.02798 m
    k_max = m_kg * G * h                                # 30.16 J
    v_max = math.sqrt(2.0 * G * h)                      # 0.7405 m/s

    L = Leccion(
        "E1-6",
        "Péndulo de Foucault de 15 m: período, energía cinética y rapidez máxima (Recomendados 1, E6)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(6),
        enunciado=(
            "Un péndulo de Foucault de longitud $L = 15\\ \\text{m}$ y masa $m = 110\\ \\text{kg}$ oscila en MAS "
            "con amplitud angular $\\theta_{\\text{máx}} = 3{,}5^\\circ$. Determine: a) el período $T$; "
            "b) la energía cinética máxima; c) la rapidez máxima en el punto más bajo."
        ),
        claves=["péndulo simple", "Foucault", "energía mecánica", "período", "recomendados"],
    )
    L.paso("a) Período del péndulo simple")\
        .di("Combinamos la frecuencia angular del péndulo simple y la casilla de período del formulario. El período no depende ni de la masa ni de la amplitud pequeña.",
            DATO("15 m de largo"),
            F(W_SIMPLE), F(PERIODO),
            M(r"T = 2\pi\sqrt{\frac{L}{g}}"),
            M(rf"T = 2\pi\sqrt{{\frac{{15}}{{9{{,}}80}}}} = 2\pi(1{{,}}237) = {tex(T, 3)}\ \text{{s}}"),
            R(rf"T = {tex(T, 3)}\ \text{{s}}"))
    L.paso("b y c) Altura máxima, energía cinética máxima y rapidez")\
        .di("En el extremo superior la masa sube una altura hache igual a ele por uno menos coseno de tres coma cinco grados. Por conservación, toda esa energía potencial se vuelve cinética abajo.",
            M(rf"h = L(1 - \cos 3{{,}}5^\circ) = 15(1 - 0{{,}}998135) = {tex(h, 4)}\ \text{{m}}"),
            M(rf"K_{{\text{{máx}}}} = mgh = (110)(9{{,}}80)({tex(h, 4)}) = {tex(k_max, 4)}\ \text{{J}}"),
            M(rf"v_{{\text{{máx}}}} = \sqrt{{2gh}} = \sqrt{{2(9{{,}}80)({tex(h, 4)})}} = {tex(v_max, 3)}\ \text{{m/s}}"),
            R(rf"K_{{\text{{máx}}}} = {tex(k_max, 3)}\ \text{{J}},\qquad v_{{\text{{máx}}}} = {tex(v_max, 3)}\ \text{{m/s}}"))
    L.usa(W_SIMPLE, PERIODO)
    L.responde("a", rf"T = {tex(T, 3)}\ \text{{s}}", T, "s")
    L.responde("b", rf"K_{{\text{{máx}}}} = {tex(k_max, 3)}\ \text{{J}}", k_max, "J")
    L.responde("c", rf"v_{{\text{{máx}}}} = {tex(v_max, 3)}\ \text{{m/s}}", v_max, "m/s")
    return L.oficial("Recomendados 1 II-2026", 7.77, 30.2, 0.741)


# ============================================================================ E1-7
@leccion("E1-7")
def e1_7() -> Leccion:
    L_m = 2.00
    T = 2.0 * math.pi * math.sqrt(8.0 * L_m / (9.0 * G))  # 2.676 s

    L = Leccion(
        "E1-7",
        "Varilla uniforme con esfera en el extremo: tensiones y péndulo físico (Recomendados 1, E7)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(7),
        enunciado=(
            "Una pequeña esfera de masa $M$ está unida al extremo inferior de una varilla uniforme de igual masa $M$ "
            "y longitud $L$, articulada en su extremo superior (Figura 4). a) Halle las tensiones en el pivote y en "
            "un punto $P$ a distancia $y$ del extremo inferior en reposo. b) Deduzca el período de pequeñas oscilaciones "
            "y evalúelo para $L = 2{,}00\\ \\text{m}$."
        ),
        claves=["péndulo físico", "ejes paralelos", "centro de masa", "tensión", "recomendados"],
    )
    L.paso("a) Tensiones estáticas en el pivote y en P")\
        .di("El pivote superior carga la varilla más la esfera, o sea dos eme ge. Si cortamos a altura ye desde abajo, debajo cuelga la esfera más una fracción ye entre ele de la varilla.",
            DATO("extremo superior"),
            M(r"T_{\text{pivote}} = (M + M)g = 2Mg"),
            M(r"m_{\text{debajo}} = M + M\frac{y}{L} = M\left(1 + \frac{y}{L}\right)"),
            R(r"T_{\text{pivote}} = 2Mg,\qquad T(y) = Mg\left(1 + \frac{y}{L}\right)"))
    L.paso("b) Momento de inercia y centro de masa")\
        .di("Aplicamos el teorema de ejes paralelos del formulario a la varilla y sumamos la partícula en ele. Luego ubicamos el centro de masa total medido desde el pivote.",
            F(EJES),
            M(r"I_{\text{varilla}} = \frac{1}{12}ML^2 + M\left(\frac{L}{2}\right)^2 = \frac{1}{3}ML^2 \;\Rightarrow\; I = \frac{1}{3}ML^2 + ML^2 = \frac{4}{3}ML^2"),
            M(r"d_{\text{cm}} = \frac{M(L/2) + M(L)}{2M} = \frac{3}{4}L"))
    L.paso("b) Período del péndulo físico y cálculo para L = 2,00 m")\
        .di("Sustituimos masa total dos eme, inercia y centro de masa en la casilla de péndulo físico del formulario. La masa eme se cancela por completo.",
            F(W_FISICO), F(PERIODO),
            M(r"T = 2\pi\sqrt{\frac{I}{m_{\text{tot}}g d_{\text{cm}}}} = 2\pi\sqrt{\frac{\frac{4}{3}ML^2}{(2M)g\left(\frac{3}{4}L\right)}} = 2\pi\sqrt{\frac{8L}{9g}}"),
            M(rf"T = 2\pi\sqrt{{\frac{{8(2{{,}}00)}}{{9(9{{,}}80)}}}} = 2\pi\sqrt{{\frac{{16}}{{88{{,}}2}}}} = {tex(T, 3)}\ \text{{s}}"),
            R(rf"T = 2\pi\sqrt{{\frac{{8L}}{{9g}}}} = {tex(T, 3)}\ \text{{s}}"))
    L.usa(EJES, W_FISICO, PERIODO)
    L.responde("b", rf"T = {tex(T, 3)}\ \text{{s}}", T, "s")
    return L.oficial("Recomendados 1 II-2026", 2.68)


# ============================================================================ E1-8
@leccion("E1-8")
def e1_8() -> Leccion:
    A, k, w, mu = 0.150, 0.800, 50.0, 0.0120
    a_max = w * w * A              # 375 m/s^2
    dm = mu * 0.0100               # 1.20e-4 kg
    fy_max = dm * a_max            # 0.0450 N
    v = w / k                      # 62.5 m/s
    F_tens = mu * v * v            # 46.875 N
    razon = fy_max / F_tens        # 9.60e-4

    L = Leccion(
        "E1-8",
        "Onda viajera en cuerda: forma coseno, aceleración y fuerza transversal (Recomendados 1, E8)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(8),
        enunciado=(
            "Una onda sinusoidal en una cuerda con $\\mu = 12{,}0\\ \\text{g/m}$ sigue "
            "$y(x,t) = (0{,}150\\ \\text{m})\\sen(0{,}800x - 50{,}0t)$. "
            "a) Exprésela con coseno. b) Halle la aceleración transversal máxima. "
            "c) Halle la fuerza transversal máxima sobre un tramo de $1{,}00\\ \\text{cm}$ y compárela con la tensión de la cuerda."
        ),
        claves=["onda viajera", "aceleración transversal", "tensión", "cuerda", "recomendados"],
    )
    L.paso("a y b) Forma coseno y aceleración transversal máxima")\
        .di("Como seno de un ángulo equivale al coseno restando pi medios, reescribimos la onda al formato del formulario. Derivando dos veces respecto al tiempo sale la aceleración máxima.",
            DATO("0,150 m"),
            F(VIAJERA), F(A_MAS),
            M(r"y(x,t) = (0{,}150\ \text{m})\cos\left(0{,}800x - 50{,}0t - \frac{\pi}{2}\right)"),
            M(rf"a_{{y,\text{{máx}}}} = \omega^2 A = (50{{,}}0)^2(0{{,}}150) = {tex(a_max, 3)}\ \text{{m/s}}^2"),
            R(rf"a_{{y,\text{{máx}}}} = {tex(a_max, 3)}\ \text{{m/s}}^2"))
    L.paso("c) Fuerza transversal sobre 1,00 cm y tensión de la cuerda")\
        .di("Con la densidad lineal hallamos la masa de un centímetro de cuerda y multiplicamos por la aceleración máxima. Luego sacamos la rapidez de onda y la tensión.",
            F(MU), F(V_ONDA), F(V_CUERDA),
            M(rf"\Delta m = \mu\Delta x = (0{{,}}0120)(0{{,}}0100) = {tex(dm, 3)}\ \text{{kg}} \;\Rightarrow\; F_{{y,\text{{máx}}}} = \Delta m\,a_{{y,\text{{máx}}}} = {tex(fy_max, 3)}\ \text{{N}}"),
            M(rf"v = \frac{{\omega}}{{k}} = \frac{{50{{,}}0}}{{0{{,}}800}} = {tex(v, 3)}\ \text{{m/s}} \;\Rightarrow\; F = \mu v^2 = (0{{,}}0120)(62{{,}}5)^2 = {tex(F_tens, 4)}\ \text{{N}}"),
            M(rf"\frac{{F_{{y,\text{{máx}}}}}}{{F}} = \frac{{0{{,}}0450}}{{46{{,}}88}} = {tex(razon, 3)} = 0{{,}}096\,\%"),
            R(rf"F_{{y,\text{{máx}}}} = {tex(fy_max, 3)}\ \text{{N}}\quad (0{{,}}096\,\%\ \text{{de}}\ F = {tex(F_tens, 4)}\ \text{{N}})"))
    L.usa(VIAJERA, A_MAS, MU, V_ONDA, V_CUERDA)
    L.responde("b", rf"a_{{y,\text{{máx}}}} = {tex(a_max, 3)}\ \text{{m/s}}^2", a_max, "m/s²")
    L.responde("c-F", rf"F_{{y,\text{{máx}}}} = {tex(fy_max, 3)}\ \text{{N}}", fy_max, "N")
    L.responde("c-T", rf"F = {tex(F_tens, 4)}\ \text{{N}}", F_tens, "N")
    return L.oficial("Recomendados 1 II-2026", 375.0, 0.0450, 46.875)


# ============================================================================ E1-9
@leccion("E1-9")
def e1_9() -> Leccion:
    L = Leccion(
        "E1-9",
        "Potencia de una onda atenuada exponencialmente en una cuerda (Recomendados 1, E9)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(9),
        enunciado=(
            "Un pulso se propaga por una cuerda de densidad lineal $\\mu$ con función "
            "$y(x,t) = [A_0 e^{-bx}]\\sen(kx - \\omega t)$. Determine: a) la potencia media $P(x)$ en la posición $x$; "
            "b) $P(0)$ en el origen; c) la razón $P(x)/P(0)$."
        ),
        claves=["potencia de onda", "amplitud exponencial", "atenuación", "simbólico", "recomendados"],
    )
    L.paso("a) Sustituir A(x) en la potencia media del formulario")\
        .di("Partimos de la potencia media en una cuerda del formulario y expresamos raíz de mu por tensión como mu por ve usando la rapidez de onda.",
            DATO("densidad lineal de masa"),
            F(P_MED), F(V_CUERDA),
            M(r"\sqrt{\mu F} = \mu\sqrt{\frac{F}{\mu}} = \mu v \;\Rightarrow\; P_{\text{med}} = \frac{1}{2}\mu v\omega^2 A^2"),
            M(r"P(x) = \frac{1}{2}\mu v\omega^2 \left(A_0 e^{-bx}\right)^2 = \frac{1}{2}\mu v\omega^2 A_0^2 e^{-2bx}"),
            R(r"P(x) = \frac{1}{2}\mu v\omega^2 A_0^2 e^{-2bx}"))
    L.paso("b y c) Potencia en el origen y razón P(x)/P(0)")\
        .di("Evaluamos en equis igual a cero, donde la exponencial vale uno, para obtener la potencia en el origen.",
            M(r"P(0) = \frac{1}{2}\mu v\omega^2 A_0^2 e^{0} = \frac{1}{2}\mu v\omega^2 A_0^2"))\
        .di("Dividimos ambas expresiones: como la potencia va con la amplitud al cuadrado, cae al doble de ritmo exponencial.",
            M(r"\frac{P(x)}{P(0)} = \frac{\frac{1}{2}\mu v\omega^2 A_0^2 e^{-2bx}}{\frac{1}{2}\mu v\omega^2 A_0^2} = e^{-2bx}"),
            R(r"P(0) = \frac{1}{2}\mu v\omega^2 A_0^2,\qquad \frac{P(x)}{P(0)} = e^{-2bx}"))
    L.usa(P_MED, V_CUERDA)
    L.responde("c", r"P(x)/P(0) = e^{-2bx}")
    return L


# ============================================================================ E1-10
@leccion("E1-10")
def e1_10() -> Leccion:
    T, v = 0.0250, 30.0
    w = 2.0 * math.pi / T           # 251.33 rad/s
    k = w / v                       # 8.3776 rad/m
    y0, vy0 = 0.0200, -2.00
    a_sen = -vy0 / w                # 0.0079577 m
    A = math.sqrt(y0 ** 2 + a_sen ** 2)  # 0.021525 m
    phi = math.atan2(a_sen, y0)     # 0.3787 rad
    vy_max = w * A                  # 5.410 m/s

    L = Leccion(
        "E1-10",
        "Onda viajando hacia −x desde condiciones en (0, 0) (Recomendados 1, E10)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(10),
        enunciado=(
            "Una onda transversal de período $T = 25{,}0\\ \\text{ms}$ viaja hacia $-x$ a $30{,}0\\ \\text{m/s}$. "
            "En $x = 0,\\ t = 0$, la cuerda está en $y = +2{,}00\\ \\text{cm}$ bajando a $2{,}00\\ \\text{m/s}$. "
            "Determine: a) la amplitud $A$; b) la fase inicial $\\phi$; c) $v_{y,\\text{máx}}$; d) la función $y(x,t)$."
        ),
        claves=["onda viajera", "fase inicial", "dirección negativa", "amplitud", "recomendados"],
    )
    L.paso("Parámetros ω y k para onda hacia −x")\
        .di("Como la onda viaja en sentido negativo, usamos signo más entre ka equis y omega te. Calculamos omega desde el período y ka desde la rapidez.",
            DATO("25,0 ms"),
            F(PERIODO), F(V_ONDA), F(VIAJERA_PM),
            M(rf"\omega = \frac{{2\pi}}{{0{{,}}0250}} = 80\pi = {tex(w, 4)}\ \text{{rad/s}},\qquad k = \frac{{\omega}}{{v}} = \frac{{251{{,}}3}}{{30{{,}}0}} = {tex(k, 3)}\ \text{{rad/m}}"))
    L.paso("a y b) Amplitud y constante de fase")\
        .di("Evaluando ye y su derivada temporal en cero coma cero, obtenemos A coseno de fi y A seno de fi. Los dos son positivos: primer cuadrante.",
            F(AMP),
            M(rf"A\cos\phi = 0{{,}}0200\ \text{{m}},\qquad v_y(0,0) = -\omega A\sen\phi = -2{{,}}00 \;\Rightarrow\; A\sen\phi = \frac{{2{{,}}00}}{{80\pi}} = {tex(a_sen, 4)}\ \text{{m}}"),
            M(rf"A = \sqrt{{(0{{,}}0200)^2 + ({tex(a_sen, 4)})^2}} = {tex(A, 4)}\ \text{{m}} = {tex(A * 100, 3)}\ \text{{cm}}"),
            M(rf"\tan\phi = \frac{{{tex(a_sen, 4)}}}{{0{{,}}0200}} = 0{{,}}3979 \;\Rightarrow\; \phi = \arctan(0{{,}}3979) = {tex(phi, 3)}\ \text{{rad}}"),
            R(rf"A = {tex(A, 3)}\ \text{{m}},\qquad \phi = {tex(phi, 3)}\ \text{{rad}}"))
    L.paso("c y d) Rapidez transversal máxima y función y(x,t)")\
        .di("La rapidez transversal máxima es omega por A, y armamos la función completa sustituyendo todos los valores en unidades del sistema internacional.",
            M(rf"v_{{y,\text{{máx}}}} = \omega A = (251{{,}}3)(0{{,}}02153) = {tex(vy_max, 3)}\ \text{{m/s}}"),
            R(rf"v_{{y,\text{{máx}}}} = {tex(vy_max, 3)}\ \text{{m/s}},\qquad y(x,t) = (0{{,}}0215\ \text{{m}})\cos(8{{,}}38x + 251t + 0{{,}}379)"))
    L.usa(PERIODO, V_ONDA, VIAJERA_PM, AMP)
    L.responde("a", rf"A = {tex(A, 3)}\ \text{{m}}", A, "m")
    L.responde("b", rf"\phi = {tex(phi, 3)}\ \text{{rad}}", phi, "rad")
    L.responde("c", rf"v_{{y,\text{{máx}}}} = {tex(vy_max, 3)}\ \text{{m/s}}", vy_max, "m/s")
    return L.oficial("Recomendados 1 II-2026", 0.0215, 0.379, 5.41)


# ============================================================================ E1-11
@leccion("E1-11")
def e1_11() -> Leccion:
    L_m, m_kg, F_t, f_hz, A = 20.0, 0.400, 200.0, 10.0, 0.0500
    mu = m_kg / L_m              # 0.0200 kg/m
    v = math.sqrt(F_t / mu)      # 100 m/s
    lam = v / f_hz               # 10.0 m
    k = 2.0 * math.pi / lam      # 0.200*pi = 0.6283 rad/m
    w = 2.0 * math.pi * f_hz     # 20.0*pi = 62.83 rad/s
    phi = -math.pi / 3.0         # -1.0472 rad
    th = k * 7.50 - w * 0.325 + phi  # -16*pi/3
    y_val = A * math.cos(th)         # -0.0250 m
    vy_val = A * w * math.sin(th)    # +2.7207 m/s
    ay_val = -w * w * y_val          # +98.696 m/s^2

    L = Leccion(
        "E1-11",
        "Cuerda de 20 m: parámetros de onda incidente y evaluación en (7,50 m; 0,325 s) (Recomendados 1, E11)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(11),
        enunciado=(
            "Una cuerda horizontal ($L = 20{,}0\\ \\text{m}$, $m = 0{,}400\\ \\text{kg}$, $F = 200\\ \\text{N}$) lleva hacia $+x$ "
            "una onda de $f = 10{,}0\\ \\text{Hz}$ y $A = 5{,}00\\ \\text{cm}$. En $x = 0,\\ t = 0$, $y = +2{,}50\\ \\text{cm}$ bajando. "
            "Halle: a) $\\mu$; b) $v$; c) $\\lambda$ y $k$; d) $\\omega$; e) $\\phi$; f) $y(x,t)$; "
            "g) $y$, $v_y$ y $a_y$ en $x = 7{,}50\\ \\text{m}$, $t = 0{,}325\\ \\text{s}$."
        ),
        claves=["onda viajera", "cuerda", "velocidad transversal", "fase", "recomendados"],
    )
    L.paso("a–d) Densidad lineal, rapidez, λ, k y ω")\
        .di("Calculamos primero densidad lineal y rapidez transversal en la cuerda con las casillas de la hoja.",
            DATO("20,0 m"),
            F(MU), F(V_CUERDA), F(V_ONDA), F(K_ONDA), F(W_F),
            M(rf"\mu = \frac{{0{{,}}400}}{{20{{,}}0}} = {tex(mu, 3)}\ \text{{kg/m}},\qquad v = \sqrt{{\frac{{200}}{{0{{,}}0200}}}} = {tex(v, 3)}\ \text{{m/s}}"))\
        .di("Con rapidez cien metros por segundo y diez hercios sacamos longitud de onda, número de onda y frecuencia angular.",
            M(rf"\lambda = \frac{{100}}{{10{{,}}0}} = 10{{,}}0\ \text{{m}},\quad k = \frac{{2\pi}}{{10{{,}}0}} = 0{{,}}200\pi = {tex(k, 3)}\ \text{{rad/m}},\quad \omega = 20{{,}}0\pi = {tex(w, 3)}\ \text{{rad/s}}"),
            R(rf"\mu = 0{{,}}0200\ \text{{kg/m}},\quad v = 100\ \text{{m/s}},\quad \lambda = 10{{,}}0\ \text{{m}}"))
    L.paso("e–f) Fase inicial y ecuación completa de la onda")\
        .di("Como viaja hacia más equis usamos coseno de ka equis menos omega te más fi. La posición inicial deja más o menos pi tercios.",
            F(VIAJERA),
            M(r"0{,}0250 = 0{,}0500\cos\phi \;\Rightarrow\; \cos\phi = \frac{1}{2} \;\Rightarrow\; \phi = \pm\frac{\pi}{3}"))\
        .di("Al derivar en el tiempo sale signo más delante del seno, así que la velocidad negativa hacia abajo fija fi en menos pi tercios.",
            M(r"v_y(0,0) = +\omega A\sen\phi < 0 \;\Rightarrow\; \sen\phi < 0 \;\Rightarrow\; \phi = -\frac{\pi}{3}\ \text{rad}"),
            R(r"y(x,t) = (0{,}0500\ \text{m})\cos\left(0{,}200\pi x - 20{,}0\pi t - \frac{\pi}{3}\right)"))
    L.paso("g) Evaluación en x = 7,50 m y t = 0,325 s")\
        .di("Evaluamos el argumento exacto en múltiplos de pi y sumamos seis pi para caer en dos pi tercios, donde el coseno es menos un medio y el seno es raíz de tres medios.",
            F(A_MAS),
            M(r"\theta = (0{,}200\pi)(7{,}50) - (20{,}0\pi)(0{,}325) - \frac{\pi}{3} = 1{,}50\pi - 6{,}50\pi - \frac{\pi}{3} = -\frac{16\pi}{3} \equiv \frac{2\pi}{3}"),
            M(rf"y = (0{{,}}0500)(-0{{,}}500) = {tex(y_val, 3)}\ \text{{m}}"),
            M(rf"v_y = (0{{,}}0500)(20{{,}}0\pi)\frac{{\sqrt{{3}}}}{{2}} = \frac{{\pi\sqrt{{3}}}}{{2}} = +{tex(vy_val, 3)}\ \text{{m/s}}"),
            M(rf"a_y = -\omega^2 y = -(20{{,}}0\pi)^2(-0{{,}}0250) = 10\pi^2 = +{tex(ay_val, 3)}\ \text{{m/s}}^2"),
            R(rf"y = {tex(y_val, 3)}\ \text{{m}},\quad v_y = +{tex(vy_val, 3)}\ \text{{m/s}},\quad a_y = +{tex(ay_val, 3)}\ \text{{m/s}}^2"))
    L.usa(MU, V_CUERDA, V_ONDA, K_ONDA, W_F, VIAJERA, A_MAS)
    L.responde("a", r"\mu = 0{,}0200\ \text{kg/m}", mu, "kg/m")
    L.responde("b", r"v = 100\ \text{m/s}", v, "m/s")
    L.responde("g-y", rf"y = {tex(y_val, 3)}\ \text{{m}}", y_val, "m")
    L.responde("g-vy", rf"v_y = {tex(vy_val, 3)}\ \text{{m/s}}", vy_val, "m/s")
    L.responde("g-ay", rf"a_y = {tex(ay_val, 3)}\ \text{{m/s}}^2", ay_val, "m/s²")
    return L.oficial("Recomendados 1 II-2026", 0.0200, 100.0, -0.0250, 2.72, 98.7)


# ============================================================================ E1-12
@leccion("E1-12")
def e1_12() -> Leccion:
    L_m, F_t, mu, A = 0.80, 120.0, 5.0e-4, 0.0020
    v = math.sqrt(F_t / mu)          # 489.898 m/s
    lam3 = 2.0 * L_m / 3.0           # 0.53333 m
    f3 = v / lam3                    # 918.56 Hz
    w3 = 2.0 * math.pi * f3          # 5771.47 rad/s
    k3 = 2.0 * math.pi / lam3        # 3.75*pi = 11.781 rad/m

    L = Leccion(
        "E1-12",
        "Superposición de dos ondas opuestas y tercer armónico (Recomendados 1, E12)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(12),
        enunciado=(
            "Una cuerda de $L = 0{,}80\\ \\text{m}$ fija en ambos extremos tiene $F = 120\\ \\text{N}$ y "
            "$\\mu = 5{,}0\\times 10^{-4}\\ \\text{kg/m}$. Se superponen $y_1 = A\\sen(kx - \\omega t)$ y "
            "$y_2 = A\\sen(kx + \\omega t)$ ($A = 2{,}0\\ \\text{mm}$) en el tercer modo normal ($n = 3$). "
            "Halle: a) la onda resultante; b) $v$; c) $\\lambda_3$; d) $f_3$ y $\\omega_3$; e) $y(x,t)$; f) nodos y antinodos."
        ),
        claves=["onda estacionaria", "tercer armónico", "nodos", "antinodos", "superposición", "recomendados"],
    )
    L.paso("a y b) Onda estacionaria por superposición y rapidez v")\
        .di("Sumamos ambos senos con la identidad de suma a producto. Posición y tiempo quedan separados en factores distintos: esa es la firma de una onda estacionaria.",
            DATO("tercer modo normal"),
            F(ESTAC), F(V_CUERDA),
            M(r"y(x,t) = A\sen(kx - \omega t) + A\sen(kx + \omega t) = 2A\sen(kx)\cos(\omega t)"),
            M(rf"v = \sqrt{{\frac{{120}}{{5{{,}}0\times 10^{{-4}}}}}} = \sqrt{{240\,000}} = {tex(v, 5)}\ \text{{m/s}}"),
            R(rf"y = 2A\sen(kx)\cos(\omega t),\qquad v = {tex(v, 5)}\ \text{{m/s}}"))
    L.paso("c–e) Tercer armónico: λ₃, f₃, ω₃ y ecuación completa")\
        .di("Con los extremos fijos en cero y ele, seno de ka ele vale cero, lo que da la casilla de longitudes estacionarias. Evaluamos para ene igual a tres.",
            F(LAM_N), F(F_N), F(W_F), F(K_ONDA),
            M(rf"\lambda_3 = \frac{{2(0{{,}}80)}}{{3}} = {tex(lam3, 3)}\ \text{{m}},\qquad k_3 = \frac{{2\pi}}{{\lambda_3}} = 3{{,}}75\pi = {tex(k3, 4)}\ \text{{rad/m}}"),
            M(rf"f_3 = \frac{{489{{,}}90}}{{0{{,}}5333}} = {tex(f3, 5)}\ \text{{Hz}},\qquad \omega_3 = 2\pi(918{{,}}56) = {tex(w3, 6)}\ \text{{rad/s}}"),
            R(rf"y(x,t) = 0{{,}}0040\sen(3{{,}}75\pi x)\cos(5771t)\ \text{{m}}"))
    L.paso("f) Posiciones de nodos y antinodos")\
        .di("En el tercer armónico hay cuatro nodos espaciados media longitud de onda y tres antinodos justo a la mitad entre cada par de nodos.",
            M(r"\frac{\lambda_3}{2} = 0{,}267\ \text{m}"),
            M(r"x_{\text{nodos}} = \{0;\ 0{,}267;\ 0{,}533;\ 0{,}800\}\ \text{m}"),
            R(r"x_{\text{antinodos}} = \{0{,}133;\ 0{,}400;\ 0{,}667\}\ \text{m}"))
    L.usa(ESTAC, V_CUERDA, LAM_N, F_N, W_F, K_ONDA)
    L.responde("b", rf"v = {tex(v, 5)}\ \text{{m/s}}", v, "m/s")
    L.responde("c", rf"\lambda_3 = {tex(lam3, 3)}\ \text{{m}}", lam3, "m")
    L.responde("d-f", rf"f_3 = {tex(f3, 5)}\ \text{{Hz}}", f3, "Hz")
    return L.oficial("Recomendados 1 II-2026", 489.90, 0.533, 918.56)


# ============================================================================ E1-13
@leccion("E1-13")
def e1_13() -> Leccion:
    mu = 1.05 / 30.0               # 0.0350 kg/m
    L_act = 1.50
    n_arm = 5
    f = 180.0 / 3.00               # 60.0 Hz
    lam5 = 2.0 * L_act / n_arm     # 0.600 m
    v = f * lam5                   # 36.0 m/s
    F_t = mu * v * v               # 45.36 N
    a_oe = 2.50e-3
    vy_max = a_oe * (2.0 * math.pi * f)  # 0.9425 m/s

    L = Leccion(
        "E1-13",
        "Patrón estacionario de 5 antinodos: tensión, viajeras y v_y central (Recomendados 1, E13)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(13),
        enunciado=(
            "De una carrucha de $30{,}0\\ \\text{m}$ y $1{,}05\\ \\text{kg}$ se tensan $1{,}50\\ \\text{m}$ con extremos fijos. "
            "La Figura 5 muestra $5$ antinodos con recorrido pico a pico de $5{,}00\\ \\text{mm}$ en el centro, "
            "que completa $180$ oscilaciones en $3{,}00\\ \\text{s}$. Halle: a) la tensión $F$; b) las ondas viajeras "
            "incidente, reflejada y estacionaria; c) $v_y(t)$ del punto central."
        ),
        claves=["onda estacionaria", "5 antinodos", "tensión", "ondas viajeras", "recomendados"],
    )
    L.paso("a) Densidad lineal, rapidez y tensión F")\
        .di("Sacamos mu de toda la carrucha, la frecuencia de las ciento ochenta oscilaciones y la longitud de onda del quinto armónico. De ahí despejamos la tensión.",
            DATO("180 oscilaciones"),
            F(MU), F(LAM_N), F(V_ONDA), F(V_CUERDA),
            M(rf"\mu = \frac{{1{{,}}05}}{{30{{,}}0}} = {tex(mu, 3)}\ \text{{kg/m}},\qquad f = \frac{{180}}{{3{{,}}00}} = 60{{,}}0\ \text{{Hz}}"),
            M(rf"\lambda_5 = \frac{{2(1{{,}}50)}}{{5}} = 0{{,}}600\ \text{{m}},\qquad v = (60{{,}}0)(0{{,}}600) = 36{{,}}0\ \text{{m/s}}"),
            M(rf"F = \mu v^2 = (0{{,}}0350)(36{{,}}0)^2 = {tex(F_t, 4)}\ \text{{N}}"),
            R(rf"F = {tex(F_t, 3)}\ \text{{N}}"))
    L.paso("b) Ondas viajeras y estacionaria")\
        .di("Cinco milímetros de pico a pico implican dos coma cinco milímetros de amplitud estacionaria y la mitad para cada onda viajera.",
            F(K_ONDA), F(W_F), F(VIAJERA), F(ESTAC),
            M(r"A_{\text{OE}} = 2{,}50\times 10^{-3}\ \text{m},\quad A = 1{,}25\times 10^{-3}\ \text{m},\quad k = \frac{10\pi}{3}\ \text{rad/m},\quad \omega = 120\pi\ \text{rad/s}"))\
        .di("La onda reflejada invierte signo en el extremo fijo; al sumarlas aparece la onda estacionaria de la hoja.",
            M(r"y_i = 1{,}25\times 10^{-3}\cos\left(\frac{10\pi}{3}x - 120\pi t\right),\qquad y_r = -1{,}25\times 10^{-3}\cos\left(\frac{10\pi}{3}x + 120\pi t\right)"),
            R(r"y(x,t) = (2{,}50\times 10^{-3}\ \text{m})\sen\left(\frac{10\pi}{3}x\right)\sen(120\pi t)"))
    L.paso("c) Velocidad transversal del punto central")\
        .di("Derivamos respecto al tiempo y evaluamos en el centro de la cuerda, equis igual cero coma setenta y cinco metros, donde el seno vale uno.",
            M(r"v_y(x,t) = \frac{\partial y}{\partial t} = (2{,}50\times 10^{-3})(120\pi)\sen\left(\frac{10\pi}{3}x\right)\cos(120\pi t)"),
            M(rf"\sen\left(\frac{{10\pi}}{{3}}\cdot 0{{,}}750\right) = \sen\left(\frac{{5\pi}}{{2}}\right) = 1,\qquad (2{{,}}50\times 10^{{-3}})(120\pi) = {tex(vy_max, 3)}\ \text{{m/s}}"),
            R(rf"v_y(t) = ({tex(vy_max, 3)}\ \text{{m/s}})\cos(120\pi t)"))
    L.usa(MU, LAM_N, V_ONDA, V_CUERDA, K_ONDA, W_F, VIAJERA, ESTAC)
    L.responde("a", rf"F = {tex(F_t, 3)}\ \text{{N}}", F_t, "N")
    L.responde("c", rf"v_{{y,\text{{máx}}}} = {tex(vy_max, 3)}\ \text{{m/s}}", vy_max, "m/s")
    return L.oficial("Recomendados 1 II-2026", 45.4, 0.942)


# ============================================================================ E1-14
@leccion("E1-14")
def e1_14() -> Leccion:
    f, L_m, m_c = 60.0, 1.5, 1.0e-4
    mu = m_c / L_m                            # 6.667e-5 kg/m
    M1 = 4.0 * mu * (L_m ** 2) * (f ** 2) / G # 0.22041 kg = 220.4 g
    M2, M3, M4 = M1 / 4.0, M1 / 9.0, M1 / 16.0

    L = Leccion(
        "E1-14",
        "Vibrador de 60 Hz y masas colgantes para los primeros 4 armónicos (Recomendados 1, E14)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(14),
        enunciado=(
            "Un vibrador de $60\\ \\text{Hz}$ excita un tramo activo de $L = 1{,}5\\ \\text{m}$ de cuerda "
            "(masa de ese tramo $0{,}10\\ \\text{g}$) que pasa por una polea con un gancho para masas colgantes (Figura 6). "
            "¿Qué masa $M_n$ debe colgarse para formar los primeros cuatro armónicos ($n = 1, 2, 3, 4$)?"
        ),
        claves=["armónicos", "masa colgante", "tensión", "cuerda", "recomendados"],
    )
    L.paso("Relación entre la masa colgante Mₙ y el armónico n")\
        .di("Igualamos la rapidez transversal desde tensión eme ge con la rapidez exigida por el armónico ene. Así despejamos la masa en función de ene al cuadrado.",
            DATO("60 Hz"),
            F(MU), F(V_CUERDA), F(LAM_N), F(F_N),
            M(rf"\mu = \frac{{1{{,}}0\times 10^{{-4}}}}{{1{{,}}5}} = {tex(mu, 3)}\ \text{{kg/m}}"),
            M(r"\sqrt{\frac{M_n g}{\mu}} = \frac{2Lf}{n} \;\Rightarrow\; M_n = \frac{4\mu L^2 f^2}{g\,n^2} = \frac{M_1}{n^2}"))
    L.paso("Cálculo de las cuatro masas")\
        .di("Evaluamos primero la masa fundamental para ene igual a uno y luego dividimos entre cuatro, nueve y dieciséis.",
            M(rf"M_1 = \frac{{4({tex(mu, 3)})(1{{,}}5)^2(60)^2}}{{9{{,}}80}} = {tex(M1, 4)}\ \text{{kg}} = {tex(M1 * 1000, 4)}\ \text{{g}}"),
            M(rf"M_2 = \frac{{220{{,}}4}}{{4}} = {tex(M2 * 1000, 3)}\ \text{{g}},\quad M_3 = \frac{{220{{,}}4}}{{9}} = {tex(M3 * 1000, 3)}\ \text{{g}},\quad M_4 = \frac{{220{{,}}4}}{{16}} = {tex(M4 * 1000, 3)}\ \text{{g}}"),
            R(r"M_1 = 220\ \text{g},\quad M_2 = 55\ \text{g},\quad M_3 = 25\ \text{g},\quad M_4 = 14\ \text{g}"))
    L.usa(MU, V_CUERDA, LAM_N, F_N)
    L.responde("M1", rf"M_1 = {tex(M1 * 1000, 4)}\ \text{{g}}", M1 * 1000, "g")
    L.responde("M2", rf"M_2 = {tex(M2 * 1000, 3)}\ \text{{g}}", M2 * 1000, "g")
    L.responde("M3", rf"M_3 = {tex(M3 * 1000, 3)}\ \text{{g}}", M3 * 1000, "g")
    L.responde("M4", rf"M_4 = {tex(M4 * 1000, 3)}\ \text{{g}}", M4 * 1000, "g")
    return L.oficial("Recomendados 1 II-2026", 220.4, 55.1, 24.5, 13.8)


# ============================================================================ E1-15
@leccion("E1-15")
def e1_15() -> Leccion:
    rA, rB, rC = 150.0, 200.0, 300.0
    rD2 = rB ** 2 + rC ** 2       # 130000 m^2
    rD = math.sqrt(rD2)           # 360.555 m
    ib = (rA / rB) ** 2           # 0.5625
    ic = (rA / rC) ** 2           # 0.2500
    id_ = (rA ** 2) / rD2         # 0.17308

    L = Leccion(
        "E1-15",
        "Intensidades sonoras relativas de un cohete en puntos A, B, C y D (Recomendados 1, E15)",
        tema="ondas", tipo="desarrollo", nivel=2, parcial=1, fuente=_f(15),
        enunciado=(
            "Un cohete explota como fuente puntual en el origen (Figura 7). Los observadores están en: "
            "$A$ a $150\\ \\text{m}$ al oeste, $B$ a $200\\ \\text{m}$ al este, $C$ a $300\\ \\text{m}$ al sur "
            "y $D$ en $(200\\ \\text{m},\\ -300\\ \\text{m})$. Halle las intensidades en $B$, $C$ y $D$ respecto a $I_A$."
        ),
        claves=["intensidad", "inverso del cuadrado", "fuente puntual", "casilla nueva", "recomendados"],
    )
    L.paso("Distancias al origen y ley del inverso del cuadrado")\
        .di("Usamos las dos casillas nuevas de intensidad de onda esférica. Solo falta hallar la distancia de D por Pitágoras.",
            DATO("fuente puntual"),
            F(INT_ESF), F(INV_CUAD),
            M(rf"r_A = 150\ \text{{m}},\quad r_B = 200\ \text{{m}},\quad r_C = 300\ \text{{m}},\quad r_D = \sqrt{{200^2 + 300^2}} = \sqrt{{130\,000}} = {tex(rD, 5)}\ \text{{m}}"))
    L.paso("Razones de intensidad en B, C y D respecto a A")\
        .di("Dividimos el cuadrado de la distancia de A entre el cuadrado de cada distancia. Cuanto más lejos esté el observador, menor es la fracción de intensidad.",
            M(rf"\frac{{I_B}}{{I_A}} = \left(\frac{{150}}{{200}}\right)^2 = \frac{{9}}{{16}} = {tex(ib, 4)}"),
            M(rf"\frac{{I_C}}{{I_A}} = \left(\frac{{150}}{{300}}\right)^2 = \frac{{1}}{{4}} = {tex(ic, 2)}"),
            M(rf"\frac{{I_D}}{{I_A}} = \frac{{150^2}}{{130\,000}} = \frac{{9}}{{52}} = {tex(id_, 4)}"),
            R(r"I_B = 0{,}5625\,I_A,\qquad I_C = 0{,}25\,I_A,\qquad I_D = 0{,}1731\,I_A"))
    L.usa(INT_ESF, INV_CUAD)
    L.responde("B", r"I_B/I_A = 0{,}5625", ib, "")
    L.responde("C", r"I_C/I_A = 0{,}25", ic, "")
    L.responde("D", r"I_D/I_A = 0{,}1731", id_, "")
    return L.oficial("Recomendados 1 II-2026", 0.5625, 0.25, 0.1731)


# ============================================================================ E1-16
@leccion("E1-16")
def e1_16() -> Leccion:
    m_kg, L_tot, mu, d_pol = 12.0, 5.00, 0.00100, 2.00
    ell = (L_tot - d_pol) / 2.0        # 1.50 m
    cos_t = (d_pol / 2.0) / ell        # 2/3
    sen_t = math.sqrt(1.0 - cos_t ** 2)  # sqrt(5)/3
    F_t = m_kg * G / (2.0 * sen_t)     # 78.8885 N
    v = math.sqrt(F_t / mu)            # 280.87 m/s
    lam3 = 2.0 * d_pol / 3.0           # 1.3333 m
    f3 = v / lam3                      # 210.65 Hz

    L = Leccion(
        "E1-16",
        "Masa colgante entre dos poleas y tercer armónico (Recomendados 1, E16)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(16),
        enunciado=(
            "Un objeto de $12{,}0\\ \\text{kg}$ cuelga simétricamente en el punto medio de una cuerda de longitud total "
            "$L = 5{,}00\\ \\text{m}$ y $\\mu = 0{,}00100\\ \\text{kg/m}$ que pasa por dos poleas sin fricción separadas "
            "$d = 2{,}00\\ \\text{m}$ (Figura 8, mostrando $3$ antinodos entre poleas). Determine: a) la tensión $F$; "
            "b) la frecuencia de vibración para ese patrón."
        ),
        claves=["poleas", "equilibrio", "tensión", "tercer armónico", "onda estacionaria", "recomendados"],
    )
    L.paso("a) Geometría triangular y tensión en la cuerda")\
        .di("Entre las poleas hay dos metros, así que quedan tres metros repartidos en dos tramos diagonales de uno coma cinco metros cada uno. Por equilibrio vertical despejamos la tensión.",
            DATO("12,0 kg"),
            M(r"\ell = \frac{5{,}00 - 2{,}00}{2} = 1{,}50\ \text{m},\qquad \cos\theta = \frac{1{,}00}{1{,}50} = \frac{2}{3} \;\Rightarrow\; \sen\theta = \frac{\sqrt{5}}{3}"),
            M(rf"2F\sen\theta = mg \;\Rightarrow\; F = \frac{{(12{{,}}0)(9{{,}}80)}}{{2(\sqrt{{5}}/3)}} = \frac{{176{{,}}4}}{{\sqrt{{5}}}} = {tex(F_t, 4)}\ \text{{N}}"),
            R(rf"F = {tex(F_t, 3)}\ \text{{N}}"))
    L.paso("b) Rapidez y frecuencia del tercer armónico entre poleas")\
        .di("Calculamos la rapidez de onda con la tensión recién hallada. Como la figura muestra tres bucles sobre el tramo de dos metros entre poleas, aplicamos ene igual a tres.",
            F(V_CUERDA), F(LAM_N), F(F_N),
            M(rf"v = \sqrt{{\frac{{{tex(F_t, 4)}}}{{0{{,}}00100}}}} = {tex(v, 5)}\ \text{{m/s}}"),
            M(rf"\lambda_3 = \frac{{2(2{{,}}00)}}{{3}} = {tex(lam3, 4)}\ \text{{m}},\qquad f_3 = \frac{{{tex(v, 5)}}}{{{tex(lam3, 4)}}} = {tex(f3, 4)}\ \text{{Hz}}"),
            R(rf"f_3 = {tex(f3, 3)}\ \text{{Hz}}"))
    L.usa(V_CUERDA, LAM_N, F_N)
    L.responde("a", rf"F = {tex(F_t, 3)}\ \text{{N}}", F_t, "N")
    L.responde("b", rf"f_3 = {tex(f3, 3)}\ \text{{Hz}}", f3, "Hz")
    return L.oficial("Recomendados 1 II-2026", 78.9, 211.0)
