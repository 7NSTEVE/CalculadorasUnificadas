"""Exámenes de Reposición FI2103 · Parcial 1 (Oscilaciones y Ondas · 7 lecciones).

Cubren los 5 exámenes históricos de reposición (`23-II-R`, `24-I-R`, `24-II-R`, `25-II-R`,
`26-I-R`). Cada desarrollo parte exclusivamente de la hoja oficial del I Parcial
(`documentos-ii-2026_Ecuaciones_Importantes_Parcial_1.pdf`) con aritmética calculada en Python.
"""

from __future__ import annotations

import math

from .dsl import DATO, F, LISTA, M, NOTA, OJO, OPCION, Leccion, R
from .formato import dice, tex
from .registro import leccion


def _fuente_r(doc: str, nombre: str, num: str) -> dict:
    return {"doc": doc, "nombre": nombre, "num": num, "parcial": 1}


# ============================================================================ RP-23II-1
@leccion("RP-23II-1")
def rp_23ii_1() -> Leccion:
    a_oe = 8.50e-3
    k_val = 10.0 * math.pi
    w_val = 240.0 * math.pi
    L_cuerda = 0.50
    lam = 2.0 * math.pi / k_val  # 0.200 m
    n_ant = round(2.0 * L_cuerda / lam)  # 5
    v_prop = w_val / k_val  # 24.0 m/s
    a_via = a_oe / 2.0  # 4.25 mm
    v_max = a_oe * w_val  # 6.4088 m/s

    L = Leccion(
        "RP-23II-1",
        "Onda estacionaria en cuerda tensa (Reposición II-2023 P1)",
        tema="ondas",
        tipo="desarrollo",
        nivel=3,
        parcial=1,
        fuente=_fuente_r("23-II-R", "Reposición II-2023", "1"),
        enunciado=(
            "Se genera una onda estacionaria en una cuerda delgada tensa y sujeta por ambos extremos: "
            "$y(x,t)=8{,}50\\ \\text{mm}\\,\\sen[(10\\pi\\ \\text{rad/m})x]\\,\\sen[(240\\pi\\ \\text{rad/s})t]$. "
            "La cuerda mide $50\\ \\text{cm}$. Calcule: "
            "A) el número de antinodos presentes; "
            "B) la rapidez de propagación de las ondas viajeras; "
            "C) las funciones de onda incidente y reflejada; "
            "D) la rapidez transversal máxima en un antinodo."
        ),
        claves=["onda estacionaria", "antinodos", "ondas viajeras", "rapidez transversal", "reposición"],
    )

    L.paso("Datos y comparación con la hoja")\
        .di("Comparamos la función dada con la ecuación de onda estacionaria del formulario del primer parcial para leer cada parámetro sin confundirnos.",
            DATO("50 cm"),
            F(r"y(x,t)=A_{\text{OE}}\sen(kx)\sen(\omega t)"),
            LISTA([
                r"$A_{\text{OE}}=8{,}50\ \text{mm}=8{,}50\times 10^{-3}\ \text{m}$",
                r"$k=10\pi\ \text{rad/m}$, $\omega=240\pi\ \text{rad/s}$, $L=0{,}50\ \text{m}$",
            ]))

    L.paso("A) Longitud de onda y número de antinodos")\
        .di("De la casilla del número de onda despejamos la longitud de onda lambda y sustituimos diez pi.",
            F(r"k=\frac{2\pi}{\lambda}"),
            M(r"\lambda=\frac{2\pi}{k}=\frac{2\pi}{10\pi}=0{,}200\ \text{m}"))\
        .di("Ahora tomamos la condición de cuerda fija en ambos extremos, despejamos el armónico ene y confirmamos cinco antinodos.",
            F(r"\lambda_n=\frac{2L}{n}"),
            M(r"n=\frac{2L}{\lambda}=\frac{2(0{,}50)}{0{,}200}=5"),
            R(r"n=5\ \text{antinodos}"))

    L.paso("B) Rapidez de propagación de las ondas viajeras")\
        .di("Usamos la relación entre rapidez, frecuencia angular y número de onda de la hoja. Pi se cancela directo.",
            F(r"v=\lambda f=\frac{\omega}{k}"),
            M(r"v=\frac{240\pi}{10\pi}=24{,}0\ \text{m/s}"),
            R(r"v=24{,}0\ \text{m/s}"))

    L.paso("C) Ondas viajeras incidente y reflejada")\
        .di("Partimos de la onda viajera sinusoidal de la hoja y aplicamos la identidad trigonométrica de resta de cosenos para reconstruir la onda estacionaria.",
            F(r"y(x,t)=A\cos(kx\pm \omega t)"),
            M(r"2A\sen(kx)\sen(\omega t)=A\cos(kx-\omega t)-A\cos(kx+\omega t)"))\
        .di("Como dos veces la amplitud viajera vale ocho coma cinco milímetros, cada onda viajera tiene amplitud cuatro coma veinticinco milímetros.",
            M(r"A=\frac{A_{\text{OE}}}{2}=\frac{8{,}50}{2}=4{,}25\ \text{mm}"),
            R(r"y_1(x,t)=4{,}25\ \text{mm}\,\cos(10\pi x-240\pi t)"),
            R(r"y_2(x,t)=-4{,}25\ \text{mm}\,\cos(10\pi x+240\pi t)"))

    L.paso("D) Rapidez transversal máxima en un antinodo")\
        .di("Derivamos la posición vertical respecto al tiempo manteniendo equis fijo. En un antinodo el seno de ka equis vale uno y el máximo ocurre cuando el coseno vale uno.",
            M(r"v_y(x,t)=\frac{\partial y}{\partial t}=A_{\text{OE}}\omega\sen(kx)\cos(\omega t)"),
            M(r"v_{y,\text{máx}}=A_{\text{OE}}\omega=(8{,}50\times 10^{-3})(240\pi)"),
            M(rf"v_{{y,\text{{máx}}}}=(8{{,}}50\times 10^{{-3}})(754{{,}}0)={tex(v_max, 3)}\ \text{{m/s}}"),
            R(rf"v_{{y,\text{{máx}}}}={tex(v_max, 3)}\ \text{{m/s}}"),
            NOTA("La solución publicada redondea 240π ≈ 754,9 o 755 y anota 6,42 m/s; el cálculo exacto con π es 6,41 m/s."))

    L.usa(
        r"y(x,t)=A_{\text{OE}}\sen(kx)\sen(\omega t)",
        r"k=\frac{2\pi}{\lambda}",
        r"\lambda_n=\frac{2L}{n}",
        r"v=\lambda f=\frac{\omega}{k}",
        r"y(x,t)=A\cos(kx\pm \omega t)",
    )
    L.responde("A", r"n=5", n_ant, "")
    L.responde("B", r"v=24{,}0\ \text{m/s}", v_prop, "m/s")
    L.responde("C", r"A=4{,}25\ \text{mm}", a_via * 1e3, "mm")
    L.responde("D", rf"v_{{y,\text{{máx}}}}={tex(v_max, 3)}\ \text{{m/s}}", v_max, "m/s")
    L.oficial("Reposición II-2023", 5, 24.0, 4.25, 6.42, tol=0.02)
    L.te_toca("Si ω fuera 120π rad/s con el mismo k = 10π rad/m, ¿cuál sería la rapidez v de la onda?", 12.0, "m/s",
              pista="Usa v = ω/k.", solucion="v = 120π / 10π = 12,0 m/s.")
    return L


# ============================================================================ RP-24I-1
@leccion("RP-24I-1")
def rp_24i_1() -> Leccion:
    L_c = 0.350
    m1 = 8.00e-3
    f1 = 262.0
    f2 = 466.0
    f1_p = 277.0
    mu1 = m1 / L_c
    lam1 = 2.0 * L_c
    v1 = lam1 * f1
    tension = mu1 * v1 ** 2
    v2 = lam1 * f2
    mu2 = tension / (v2 ** 2)
    m2 = mu2 * L_c
    L_p = v1 / (2.0 * f1_p)
    x_traste = L_c - L_p
    f2_p = v2 / (2.0 * L_p)

    L = Leccion(
        "RP-24I-1",
        "Instrumento de dos cuerdas metálicas y traste (Reposición I-2024 P1)",
        tema="ondas",
        tipo="desarrollo",
        nivel=3,
        parcial=1,
        fuente=_fuente_r("24-I-R", "Reposición I-2024", "1"),
        enunciado=(
            "Dos cuerdas metálicas de $35{,}0\\ \\text{cm}$ están bajo la misma tensión. "
            "La cuerda $S_1$ tiene masa $8{,}00\\ \\text{g}$ y produce $262\\ \\text{Hz}$ (C) en su fundamental. "
            "A) Calcule la tensión en la cuerda. "
            "B) Calcule la masa de $S_2$ para que su fundamental sea $466\\ \\text{Hz}$ (A#). "
            "C) Al colocar un traste a distancia $x$ del extremo superior, $S_1$ produce $277\\ \\text{Hz}$ (C#); halle $x$. "
            "D) Si presiona $S_2$ contra el mismo traste, ¿qué frecuencia fundamental produce?"
        ),
        claves=["cuerda vibrante", "frecuencia fundamental", "densidad lineal", "traste", "reposición"],
    )

    L.paso("Datos en unidades del SI")\
        .di("Pasamos longitud a metros y masa a kilogramos antes de empezar. Ambas cuerdas comparten longitud inicial y tensión.",
            DATO("35,0 cm"),
            LISTA([
                r"$L=0{,}350\ \text{m}$, $m_1=8{,}00\times 10^{-3}\ \text{kg}$",
                r"Fundamental ($n=1$): $f_1=262\ \text{Hz}$ en $S_1$, $f_2=466\ \text{Hz}$ en $S_2$",
                r"Con traste en $x$: longitud vibrante $L'=L-x$, $f_1'=277\ \text{Hz}$",
            ]))

    L.paso("A) Tensión de las cuerdas")\
        .di("Calculamos la densidad lineal mu uno y la longitud de onda fundamental con las dos casillas de la hoja.",
            F(r"\mu=\frac{m}{L}"),
            M(rf"\mu_1=\frac{{8{{,}}00\times 10^{{-3}}}}{{0{{,}}350}}={tex(mu1, 4)}\ \text{{kg/m}}"),
            F(r"\lambda_n=\frac{2L}{n}"),
            M(r"\lambda_1=2(0{,}350)=0{,}700\ \text{m}"))\
        .di("Ahora hallamos la rapidez ve uno y despejamos la tensión efe elevando al cuadrado la casilla de rapidez transversal.",
            F(r"v=\lambda f"),
            M(rf"v_1=(0{{,}}700)(262)={tex(v1, 4)}\ \text{{m/s}}"),
            F(r"v=\sqrt{\frac{F}{\mu}}"),
            M(rf"F=\mu_1 v_1^2=({tex(mu1, 4)})({tex(v1, 4)})^2={tex(tension, 4)}\ \text{{N}}"),
            R(rf"F={tex(tension, 4)}\ \text{{N}}"))

    L.paso("B) Masa de la cuerda S2")\
        .di("La segunda cuerda mide igual, así que su longitud de onda fundamental también es cero coma siete metros. Obtenemos su rapidez y despejamos mu dos.",
            M(rf"v_2=\lambda_1 f_2=(0{{,}}700)(466)={tex(v2, 4)}\ \text{{m/s}}"),
            M(rf"\mu_2=\frac{{F}}{{v_2^2}}=\frac{{{tex(tension, 4)}}}{{({tex(v2, 4)})^2}}={tex(mu2, 4)}\ \text{{kg/m}}"))\
        .di("Multiplicamos la densidad lineal por la longitud para hallar la masa en gramos.",
            M(rf"m_2=\mu_2 L=({tex(mu2, 4)})(0{{,}}350)={tex(m2, 3)}\ \text{{kg}}"),
            R(rf"m_2={tex(m2 * 1e3, 3)}\ \text{{g}}"))

    L.paso("C) Distancia x del traste")\
        .di("Al pisar el traste no cambian ni la tensión ni la densidad de la cuerda uno, así que su rapidez sigue siendo ciento ochenta y tres coma cuatro metros por segundo.",
            F(r"f_n=\frac{v}{\lambda_n}"),
            M(r"f_1'=\frac{v_1}{2L'}\;\Rightarrow\;L'=\frac{v_1}{2f_1'}"),
            M(rf"L'=\frac{{{tex(v1, 4)}}}{{2(277)}}={tex(L_p, 4)}\ \text{{m}}"))\
        .di("Restamos la longitud vibrante nueva a la longitud total para encontrar la posición del traste desde arriba.",
            M(rf"x=L-L'=0{{,}}350-{tex(L_p, 4)}={tex(x_traste, 3)}\ \text{{m}}"),
            R(rf"x={tex(x_traste * 100, 3)}\ \text{{cm}}"))

    L.paso("D) Nueva frecuencia fundamental de S2")\
        .di("Al presionar la segunda cuerda contra el mismo traste, su longitud vibrante también es ele prima mientras conserva su rapidez ve dos.",
            M(rf"f_2'=\frac{{v_2}}{{2L'}}=\frac{{{tex(v2, 4)}}}{{2({tex(L_p, 4)})}}={tex(f2_p, 4)}\ \text{{Hz}}"),
            R(rf"f_2'={tex(f2_p, 4)}\ \text{{Hz}}"),
            NOTA("Con el redondeo intermedio L' = 0,331 m la hoja oficial reporta 492,6 Hz; sin truncar da 492,7 Hz."))

    L.usa(r"\mu=\frac{m}{L}", r"\lambda_n=\frac{2L}{n}", r"v=\lambda f", r"v=\sqrt{\frac{F}{\mu}}", r"f_n=\frac{v}{\lambda_n}")
    L.responde("A", rf"F={tex(tension, 4)}\ \text{{N}}", tension, "N")
    L.responde("B", rf"m_2={tex(m2 * 1e3, 3)}\ \text{{g}}", m2 * 1e3, "g")
    L.responde("C", rf"x={tex(x_traste * 100, 3)}\ \text{{cm}}", x_traste * 100, "cm")
    L.responde("D", rf"f_2'={tex(f2_p, 4)}\ \text{{Hz}}", f2_p, "Hz")
    L.oficial("Reposición I-2024", 768.8, 2.53, 1.90, 492.6, tol=0.015)
    L.te_toca("Si la longitud vibrante de S1 se redujera exactamente a la mitad, ¿qué frecuencia produciría?", 524.0, "Hz",
              pista="f₁ es inversamente proporcional a L.", solucion="f' = 2 × 262 = 524 Hz.")
    return L


# ============================================================================ RP-24II-1
@leccion("RP-24II-1")
def rp_24ii_1() -> Leccion:
    m = 0.600
    w = 12.0 * math.pi
    A = 0.20
    T = 2.0 * math.pi / w  # 1/6 = 0.1667 s
    k_res = m * w ** 2  # 852.73 N/m
    x18 = A * math.cos(w * 1.8 + math.pi)
    x24 = A * math.cos(w * 2.4 + math.pi)
    dx = x24 - x18

    L = Leccion(
        "RP-24II-1",
        "Oscilador masa-resorte horizontal con aceleración inicial positiva (Reposición II-2024 P1)",
        tema="ondas",
        tipo="desarrollo",
        nivel=3,
        parcial=1,
        fuente=_fuente_r("24-II-R", "Reposición II-2024", "1"),
        enunciado=(
            "Un bloque de $600\\ \\text{g}$ oscila en un resorte horizontal con $\\omega=12\\pi\\ \\text{rad/s}$ y "
            "amplitud $A=0{,}20\\ \\text{m}$. En $t=0$ tiene aceleración positiva ($a_x>0$) y velocidad mínima. "
            "Encuentre: A) el período; B) la constante del resorte; C) la posición $x(t)$; "
            "D) el desplazamiento entre $1{,}8\\ \\text{s}$ y $2{,}4\\ \\text{s}$."
        ),
        claves=["masa-resorte", "MAS", "fase inicial", "desplazamiento", "reposición"],
    )

    L.paso("Datos e interpretación de t = 0")\
        .di("Convertimos la masa a cero coma seis kilogramos. Que la rapidez sea mínima en t igual cero significa velocidad instantánea cero, en un extremo del recorrido.",
            DATO("600 g"),
            LISTA([
                r"$m=0{,}600\ \text{kg}$, $\omega=12\pi\ \text{rad/s}$, $A=0{,}20\ \text{m}$",
                r"En $t=0$: $v(0)=0$ y $a_x(0)>0$",
            ]))

    L.paso("A) Período del movimiento")\
        .di("Aplicamos directo la ecuación del período de la hoja oficial del primer parcial y simplificamos pi.",
            F(r"T=\frac{2\pi}{\omega}"),
            M(rf"T=\frac{{2\pi}}{{12\pi}}=\frac{{1}}{{6}}={tex(T, 4)}\ \text{{s}}"),
            R(rf"T={tex(T, 4)}\ \text{{s}}"))

    L.paso("B) Constante elástica del resorte")\
        .di("Partimos de la relación entre frecuencia angular, constante y masa de la hoja y despejamos ka multiplicando por la masa.",
            F(r"\omega^2=\frac{k}{m}"),
            M(r"k=m\omega^2=(0{,}600)(12\pi)^2"),
            M(rf"k=(0{{,}}600)(1\,421{{,}}2)={tex(k_res, 4)}\ \text{{N/m}}"),
            R(rf"k={tex(k_res, 4)}\ \text{{N/m}}"))

    L.paso("C) Ecuación de la posición x(t)")\
        .di("Evaluamos la velocidad de la hoja en t igual cero para hallar los candidatos de fase fi.",
            F(r"v=-\omega A\sen(\omega t+\phi)"),
            M(r"v(0)=-\omega A\sen(\phi)=0\;\Rightarrow\;\sen(\phi)=0\;\Rightarrow\;\phi=0\ \text{o}\ \phi=\pi"))\
        .di("Para elegir entre cero y pi usamos la ecuación de aceleración de la hoja. Si la aceleración es positiva, la posición inicial debe ser negativa, así que fi vale pi.",
            F(r"a=-\omega^2 x"),
            M(r"a_x(0)>0\;\Rightarrow\;x(0)<0\;\Rightarrow\;\cos(\phi)<0\;\Rightarrow\;\phi=\pi\ \text{rad}"),
            F(r"x=A\cos(\omega t+\phi)"),
            M(r"x(t)=0{,}20\cos\left(12\pi t+\pi\right)\ \text{m}"),
            R(r"x(t)=0{,}20\cos(12\pi t+\pi)\ \text{m}"))

    L.paso("D) Desplazamiento entre 1,8 s y 2,4 s")\
        .di("Evaluamos la posición en ambos instantes con la calculadora en radianes y restamos posición final menos posición inicial.",
            M(rf"x(1{{,}}8)=0{{,}}20\cos(12\pi\cdot 1{{,}}8+\pi)={tex(x18, 4)}\ \text{{m}}"),
            M(rf"x(2{{,}}4)=0{{,}}20\cos(12\pi\cdot 2{{,}}4+\pi)={tex(x24, 4)}\ \text{{m}}"),
            M(rf"\Delta x=x(2{{,}}4)-x(1{{,}}8)={tex(x24, 4)}-({tex(x18, 4)})={tex(dx, 3)}\ \text{{m}}"),
            R(r"\Delta x=+0{,}22\ \text{m}"))

    L.usa(r"T=\frac{2\pi}{\omega}", r"\omega^2=\frac{k}{m}", r"v=-\omega A\sen(\omega t+\phi)", r"a=-\omega^2 x", r"x=A\cos(\omega t+\phi)")
    L.responde("A", rf"T={tex(T, 4)}\ \text{{s}}", T, "s")
    L.responde("B", rf"k={tex(k_res, 4)}\ \text{{N/m}}", k_res, "N/m")
    L.responde("C", r"\phi=\pi\ \text{rad}", math.pi, "rad")
    L.responde("D", rf"\Delta x=+{tex(dx, 2)}\ \text{{m}}", dx, "m")
    L.oficial("Reposición II-2024", 0.1667, 852.7, math.pi, 0.22, tol=0.02)
    L.te_toca("Con la misma ω = 12π rad/s y A = 0,20 m, ¿cuánto vale la rapidez máxima ωA?", 7.54, "m/s",
              pista="Multiplica 12π por 0,20.", solucion="v_máx = 12π(0,20) = 7,54 m/s.")
    return L


# ============================================================================ RP-25II-1
@leccion("RP-25II-1")
def rp_25ii_1() -> Leccion:
    m = 1.50
    T = 5.9 - 1.1  # 4.80 s
    f = 1.0 / T
    w = 2.0 * math.pi * f
    v_max = 1.80
    A = v_max / w
    phi = math.pi / 2.0 - w * 2.3  # -1.440 rad
    k_res = m * w ** 2
    a_max = w ** 2 * A

    L = Leccion(
        "RP-25II-1",
        "Masa-resorte a partir de gráfica de velocidad v(t) (Reposición II-2025 P1)",
        tema="ondas",
        tipo="desarrollo",
        nivel=3,
        parcial=1,
        fuente=_fuente_r("25-II-R", "Reposición II-2025", "1"),
        enunciado=(
            "Un bloque de $1{,}50\\ \\text{kg}$ atado a un resorte realiza MAS sobre un riel sin fricción. "
            "La gráfica $v(t)$ tiene amplitud $1{,}80\\ \\text{m/s}$, cruza cero en $1{,}1;\\ 3{,}5;\\ 5{,}9;\\ 8{,}3;\\ 10{,}7\\ \\text{s}$ "
            "y alcanza su mínimo $-1{,}80\\ \\text{m/s}$ en $t=2{,}3\\ \\text{s}$. Determine: "
            "A) período $T$, frecuencia $f$ y frecuencia angular $\\omega$; "
            "B) amplitud $A$, ángulo de fase $\\phi$ y constante $k$; "
            "C) ecuaciones $x(t)$, $v(t)$ y $a(t)$."
        ),
        aviso=(
            "Nota sobre errata del enunciado oficial: el texto menciona x₀ = 0,100 m, pero la gráfica v(t) implica "
            "x(0) = 0,1795 m (la propia solución oficial p.5 reconoce esta discrepancia y obtiene φ desde la gráfica). "
            "Además, en p.6 imprimió 1,307 rad/s por error tipográfico en lugar de 1,309 rad/s."
        ),
        claves=["gráfica v(t)", "desfase", "amplitud", "MAS", "reposición"],
    )

    L.paso("A) Período, frecuencia y frecuencia angular")\
        .di("Leemos un ciclo completo en la gráfica entre dos ceros con igual pendiente: de uno coma uno segundos a cinco coma nueve segundos.",
            DATO("1,50 kg"),
            M(r"T=5{,}9-1{,}1=4{,}80\ \text{s}"),
            R(r"T=4{,}80\ \text{s}"))\
        .di("Con las dos casillas de frecuencia de la hoja sacamos primero omega y luego la frecuencia lineal efe.",
            F(r"T=\frac{2\pi}{\omega}"),
            M(rf"\omega=\frac{{2\pi}}{{4{{,}}80}}={tex(w, 4)}\ \text{{rad/s}}"),
            F(r"\omega=2\pi f"),
            M(rf"f=\frac{{1}}{{4{{,}}80}}={tex(f, 4)}\ \text{{Hz}}"),
            R(rf"f={tex(f, 4)}\ \text{{Hz}}\qquad \omega={tex(w, 4)}\ \text{{rad/s}}"))

    L.paso("B) Amplitud, fase y constante del resorte")\
        .di("De la ecuación de velocidad de la hoja vemos que la velocidad máxima es omega por A. Despejamos la amplitud.",
            F(r"v=-\omega A\sen(\omega t+\phi)"),
            M(rf"v_{{\text{{máx}}}}=\omega A\;\Rightarrow\;A=\frac{{1{{,}}80}}{{{tex(w, 4)}}}={tex(A, 4)}\ \text{{m}}"),
            R(rf"A={tex(A, 4)}\ \text{{m}}"))\
        .di("Para no confundir los signos de la fase evaluamos en el mínimo de la gráfica, donde t vale dos coma tres segundos y el seno debe valer uno positivo.",
            M(r"v(2{,}3)=-1{,}80\sen(\omega\cdot 2{,}3+\phi)=-1{,}80\;\Rightarrow\;\sen(\omega\cdot 2{,}3+\phi)=1"),
            M(rf"\omega(2{{,}}3)+\phi=\frac{{\pi}}{{2}}\;\Rightarrow\;\phi=\frac{{\pi}}{{2}}-({tex(w, 4)})(2{{,}}3)={tex(phi, 4)}\ \text{{rad}}"),
            R(rf"\phi={tex(phi, 4)}\ \text{{rad}}"))\
        .di("Finalmente despejamos la constante del resorte desde omega al cuadrado igual ka entre eme.",
            F(r"\omega^2=\frac{k}{m}"),
            M(rf"k=m\omega^2=(1{{,}}50)({tex(w, 4)})^2={tex(k_res, 3)}\ \text{{N/m}}"),
            R(rf"k={tex(k_res, 3)}\ \text{{N/m}}"))

    L.paso("C) Ecuaciones de posición, velocidad y aceleración")\
        .di("Calculamos la aceleración máxima con la casilla de aceleración del movimiento armónico simple y escribimos las tres funciones del tiempo.",
            F(r"x=A\cos(\omega t+\phi)"),
            F(r"a=-\omega^2 x"),
            M(rf"a_{{\text{{máx}}}}=\omega^2 A=({tex(w, 4)})^2({tex(A, 4)})={tex(a_max, 4)}\ \text{{m/s}}^2"),
            R(rf"x(t)={tex(A, 4)}\cos({tex(w, 4)}t{tex(phi, 4)})\ \text{{m}}"),
            R(rf"v(t)=-1{{,}}80\sen({tex(w, 4)}t{tex(phi, 4)})\ \text{{m/s}}"),
            R(rf"a(t)=-{tex(a_max, 4)}\cos({tex(w, 4)}t{tex(phi, 4)})\ \text{{m/s}}^2"))

    L.usa(r"T=\frac{2\pi}{\omega}", r"\omega=2\pi f", r"v=-\omega A\sen(\omega t+\phi)", r"\omega^2=\frac{k}{m}", r"x=A\cos(\omega t+\phi)", r"a=-\omega^2 x")
    L.responde("A-T", r"T=4{,}80\ \text{s}", T, "s")
    L.responde("A-f", rf"f={tex(f, 4)}\ \text{{Hz}}", f, "Hz")
    L.responde("A-w", rf"\omega={tex(w, 4)}\ \text{{rad/s}}", w, "rad/s")
    L.responde("B-A", rf"A={tex(A, 4)}\ \text{{m}}", A, "m")
    L.responde("B-phi", rf"\phi={tex(phi, 4)}\ \text{{rad}}", phi, "rad")
    L.responde("B-k", rf"k={tex(k_res, 3)}\ \text{{N/m}}", k_res, "N/m")
    L.oficial("Reposición II-2025", 4.80, 0.2083, 1.309, 1.375, -1.440, 2.57, tol=0.015)
    L.te_toca("Con k = 2,57 N/m y A = 1,375 m, ¿cuál es la energía total E = ½kA²?", 2.43, "J",
              pista="Multiplica 0,5 × 2,57 × 1,375².", solucion="E = 0,5(2,57)(1,375)² = 2,43 J.")
    return L


# ============================================================================ RP-26I-1
@leccion("RP-26I-1")
def rp_26i_1() -> Leccion:
    L_c = 1.056
    m = 0.0337
    tension = 280.0
    mu = m / L_c
    v = math.sqrt(tension / mu)
    f1 = v / (2.0 * L_c)

    L = Leccion(
        "RP-26I-1",
        "Frecuencia fundamental de cuerda de contrabajo (Reposición I-2026 P1)",
        tema="ondas",
        tipo="seleccion",
        nivel=2,
        parcial=1,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "1"),
        enunciado=(
            "La cuerda de un contrabajo tiene longitud $1{,}056\\ \\text{m}$ y masa $0{,}0337\\ \\text{kg}$. "
            "Si la tensión es $280\\ \\text{N}$, su frecuencia fundamental es:"
        ),
        opciones={"A": "13,64 Hz", "B": "27,27 Hz", "C": "41,91 Hz", "D": "44,36 Hz"},
        correcta="D",
        claves=["contrabajo", "frecuencia fundamental", "tensión", "selección", "reposición"],
    )

    L.paso("Densidad lineal y rapidez transversal")\
        .di("Calculamos primero la densidad lineal mu con la casilla de la hoja y luego la rapidez transversal de la cuerda.",
            DATO("1,056 m"),
            F(r"\mu=\frac{m}{L}"),
            M(rf"\mu=\frac{{0{{,}}0337}}{{1{{,}}056}}={tex(mu, 4)}\ \text{{kg/m}}"),
            F(r"v=\sqrt{\frac{F}{\mu}}"),
            M(rf"v=\sqrt{{\frac{{280}}{{{tex(mu, 4)}}}}}={tex(v, 4)}\ \text{{m/s}}"))

    L.paso("Frecuencia fundamental y revisión de opciones")\
        .di("En el modo fundamental ene vale uno, así que lambda uno es dos veces el largo de la cuerda. Dividimos la rapidez entre dos ele.",
            F(r"\lambda_n=\frac{2L}{n}"),
            F(r"f_n=\frac{v}{\lambda_n}"),
            M(rf"f_1=\frac{{{tex(v, 4)}}}{{2(1{{,}}056)}}={tex(f1, 4)}\ \text{{Hz}}"),
            R(rf"f_1={tex(f1, 4)}\ \text{{Hz}}"),
            OPCION("A", "descarta", "Queda muy por debajo del valor real."),
            OPCION("B", "descarta", "Falta considerar la densidad lineal exacta."),
            OPCION("C", "descarta", "No coincide con √(FL/m)/(2L)."),
            OPCION("D", "correcta", "Coincide exacto con 44,36 Hz."))

    L.usa(r"\mu=\frac{m}{L}", r"v=\sqrt{\frac{F}{\mu}}", r"\lambda_n=\frac{2L}{n}", r"f_n=\frac{v}{\lambda_n}")
    L.responde("A", rf"f_1={tex(f1, 4)}\ \text{{Hz}}", f1, "Hz")
    L.oficial("Reposición I-2026", "D")
    return L


# ============================================================================ RP-26I-2
@leccion("RP-26I-2")
def rp_26i_2() -> Leccion:
    L = Leccion(
        "RP-26I-2",
        "Período de péndulo simple al cambiar L y m (Reposición I-2026 P2)",
        tema="ondas",
        tipo="seleccion",
        nivel=1,
        parcial=1,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "2"),
        enunciado=(
            "¿Qué ocurre con el período del péndulo simple que oscila con pequeña amplitud "
            "si la longitud se cuadruplica y la masa se disminuye a la mitad?"
        ),
        opciones={
            "A": "El período se duplica.",
            "B": "El período no cambia.",
            "C": "El período se cuadruplica.",
            "D": "El período disminuye a la mitad.",
        },
        correcta="A",
        claves=["péndulo simple", "período", "escalamiento", "selección", "reposición"],
    )

    L.paso("Obtener T en función de L desde la hoja")\
        .di("Combinamos las dos casillas del formulario del primer parcial: el período con omega y la frecuencia angular del péndulo simple.",
            DATO("cuadruplica"),
            F(r"T=\frac{2\pi}{\omega}"),
            F(r"\omega^2=\frac{g}{L}"),
            M(r"T=2\pi\sqrt{\frac{L}{g}}"))\
        .di("Observa que la masa no aparece en la fórmula: cambiar la masa a la mitad no afecta el período.",
            NOTA("El período de un péndulo simple es independiente de la masa."))

    L.paso("Sustituir L' = 4L y elegir la opción")\
        .di("Ponemos cuatro ele dentro de la raíz. La raíz de cuatro es dos, así que el período nuevo es exactamente el doble del original.",
            M(r"T'=2\pi\sqrt{\frac{4L}{g}}=2\left(2\pi\sqrt{\frac{L}{g}}\right)=2T"),
            R(r"T'=2T"),
            OPCION("A", "correcta", "√4 = 2 y la masa no influye."),
            OPCION("B", "descarta", "Cuadruplicar L sí aumenta T."),
            OPCION("C", "descarta", "Faltó sacar raíz cuadrada al factor 4."),
            OPCION("D", "descarta", "La masa reducida no divide al período."))

    L.usa(r"T=\frac{2\pi}{\omega}", r"\omega^2=\frac{g}{L}")
    L.oficial("Reposición I-2026", "A")
    return L


# ============================================================================ RP-26I-7
@leccion("RP-26I-7")
def rp_26i_7() -> Leccion:
    k_res = 4300.0
    m = 4.53
    A = 0.0624
    x0 = 0.0300
    w = math.sqrt(k_res / m)
    T = 2.0 * math.pi / w
    cos_phi = x0 / A
    phi_rad = -math.acos(cos_phi)  # -1.069 rad = -61.26 deg (equiv 298.74 deg)
    phi_deg = math.degrees(phi_rad)
    E = 0.5 * k_res * A ** 2

    L = Leccion(
        "RP-26I-7",
        "Masa-resorte con x(0) = 3,00 cm y v(0) > 0 (Reposición I-2026 P7)",
        tema="ondas",
        tipo="desarrollo",
        nivel=3,
        parcial=1,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "7"),
        enunciado=(
            "Un sistema masa-resorte tiene $k=4300\\ \\text{N/m}$, $m=4{,}53\\ \\text{kg}$ y amplitud $6{,}24\\ \\text{cm}$. "
            "En $t=0$ la posición es $3{,}00\\ \\text{cm}$ y la velocidad es positiva. Determine: "
            "A) el período de oscilación; "
            "B) la ecuación de la posición $x(t)$ en unidades del SI; "
            "C) la energía mecánica total del sistema."
        ),
        aviso=(
            "ERRATA EN EL SOLUCIONARIO OFICIAL I-2026 (p.6–7): al derivar x(t) = A cos(ωt + φ) omitieron el signo menos "
            "y escribieron v(t) = +Aω sen(ωt + φ), eligiendo φ = +61,26°. Con la fórmula oficial v = −ωA sen(ωt + φ), "
            "para que v(0) > 0 se requiere sen(φ) < 0, es decir φ = −61,26° = −1,069 rad (equivalente a 298,74° = 5,214 rad)."
        ),
        claves=["masa-resorte", "fase inicial", "signo velocidad", "energía mecánica", "reposición"],
    )

    L.paso("A) Frecuencia angular y período")\
        .di("De las dos casillas de masa-resorte y período de la hoja sacamos primero omega y luego el período en segundos.",
            DATO("4300 N/m"),
            F(r"\omega^2=\frac{k}{m}"),
            M(rf"\omega=\sqrt{{\frac{{4300}}{{4{{,}}53}}}}={tex(w, 4)}\ \text{{rad/s}}"),
            F(r"T=\frac{2\pi}{\omega}"),
            M(rf"T=\frac{{2\pi}}{{{tex(w, 4)}}}={tex(T, 4)}\ \text{{s}}"),
            R(rf"T={tex(T, 4)}\ \text{{s}}"))

    L.paso("B) Fase inicial y ecuación de posición x(t)")\
        .di("Evaluamos la ecuación de posición de la hoja en t igual cero con metros. El coseno positivo da dos posibles ángulos opuestos.",
            F(r"x=A\cos(\omega t+\phi)"),
            M(rf"\cos\phi=\frac{{0{{,}}0300}}{{0{{,}}0624}}={tex(cos_phi, 4)}"),
            M(r"\phi=\pm 1{,}069\ \text{rad}\quad(\pm 61{,}26^\circ\ \text{o}\ 298{,}74^\circ)"))\
        .di("Para elegir el signo correcto miramos la ecuación de velocidad de la hoja: lleva un menos adelante. Para que v cero sea positiva, el seno debe ser negativo.",
            F(r"v=-\omega A\sen(\omega t+\phi)"),
            OJO("El signo menos en v = −ωA sen(ωt + φ) obliga a que sen(φ) < 0 cuando v(0) > 0."),
            M(r"v(0)=-\omega A\sen\phi>0\;\Rightarrow\;\sen\phi<0\;\Rightarrow\;\phi=-1{,}069\ \text{rad}"),
            R(rf"x(t)=0{{,}}0624\cos({tex(w, 4)}t-1{{,}}069)\ \text{{m}}"))

    L.paso("C) Energía mecánica del sistema")\
        .di("Aplicamos la casilla de energía total del oscilador con la amplitud en metros.",
            F(r"E=\frac{1}{2}kA^2"),
            M(rf"E=\frac{{1}}{{2}}(4300)(0{{,}}0624)^2={tex(E, 4)}\ \text{{J}}"),
            R(rf"E={tex(E, 4)}\ \text{{J}}"))

    L.usa(r"\omega^2=\frac{k}{m}", r"T=\frac{2\pi}{\omega}", r"x=A\cos(\omega t+\phi)", r"v=-\omega A\sen(\omega t+\phi)", r"E=\frac{1}{2}kA^2")
    L.responde("A", rf"T={tex(T, 4)}\ \text{{s}}", T, "s")
    L.responde("B", rf"\phi={tex(phi_rad, 4)}\ \text{{rad}}", phi_rad, "rad")
    L.responde("C", rf"E={tex(E, 4)}\ \text{{J}}", E, "J")
    L.oficial("Reposición I-2026", 0.2039, None, 8.372, tol=0.015,
              nota="En B se corrige el signo de φ (−1,069 rad = −61,26°) porque el solucionario omitió el menos de v = −ωA sen(ωt+φ).")
    L.te_toca("Si la amplitud se duplicara manteniendo k = 4300 N/m, ¿a cuántos joules subiría la energía E?", 33.49, "J",
              pista="E es proporcional a A², así que se multiplica por 4.", solucion="E' = 4 × 8,372 = 33,49 J.")
    return L
