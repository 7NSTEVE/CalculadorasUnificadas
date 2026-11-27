"""I parcial II-2026 de Física General III (FI2103): MAS y ondas, ordinario y extraordinario (18 lecciones).

Fuentes: `I_Examen__Ord___2S_2026_Solucion.pdf` y `I_Examen__Ext___2S_2026_Solución.pdf` (cátedra,
«Documentos II - 2026»). Misma hoja del Parcial 1 que traerá la reposición (`formularios_p1_p2.py`).

Ojo con el extraordinario: la tabla de respuestas de su solución publicada no corresponde a este
enunciado en 6 de las 7 selecciones (parece la clave de otra versión del examen; el encabezado dice
«15/04/2023»). En esas lecciones la respuesta sale de la física y se avisa; no se coteja con esa clave.

IDs: P1O26-1..9 (ordinario) y P1E26-1..9 (extraordinario).
"""

from __future__ import annotations

import math
from typing import Tuple

from .dsl import DATO, F, LISTA, M, NOTA, OJO, OPCION, Leccion, R, TABLA
from .formato import partes, tex
from .registro import leccion

G = 9.80

_ORD = ("26-II-1O", "I parcial ordinario II-2026")
_EXT = ("26-II-1E", "I parcial extraordinario II-2026")

POS = r"x=A\cos(\omega t+\phi)"
VEL = r"v=-\omega A\sen(\omega t+\phi)"
W_RES = r"\omega^2=\frac{k}{m}"
PERIODO = r"T=\frac{2\pi}{\omega}"
W_F = r"\omega=2\pi f"
E_TOTAL = r"E=\frac{1}{2}kA^2"
E_SUMA = r"E=K+U=\frac{1}{2}mv^2+\frac{1}{2}kx^2"
W_SIMPLE = r"\omega^2=\frac{g}{L}"
W_FISICO = r"\omega^2=\frac{mgd}{I}"
EJES = r"I_{\text{eje}\parallel}=I_{\text{cm}}+md^2"
V_CUERDA = r"v=\sqrt{\frac{F}{\mu}}"
MU = r"\mu=\frac{m}{L}"
LAMBDA_N = r"\lambda_n=\frac{2L}{n}"
F_N = r"f_n=\frac{v}{\lambda_n}"
ARMONICOS = r"f_n=nf_1"
V_ONDA = r"v=\lambda f=\frac{\omega}{k}"
K_ONDA = r"k=\frac{2\pi}{\lambda}"
ESTACIONARIA = r"y(x,t)=A_{\text{OE}}\sen(kx)\sen(\omega t)"
P_MED = r"P_{\text{med}}=\frac{1}{2}\sqrt{\mu F}\,\omega^2A^2"

_CLAVE_OTRA = ("La tabla de respuestas de la solución publicada marca la {pub}, que no corresponde a este enunciado "
               "(parece la clave de otra versión del examen). La respuesta correcta sale de la física: {bien}.")


def _f(ex: Tuple[str, str], num: str) -> dict:
    return {"doc": ex[0], "nombre": ex[1], "num": num, "parcial": 1}


def _q(x: float, c: int = 4) -> float:
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ ordinario · selecciones

@leccion("P1O26-1")
def p1o26_1() -> Leccion:
    L = Leccion(
        "P1O26-1", "MAS con el doble de amplitud y el mismo período (I parcial ordinario II-2026, P1)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "1"),
        enunciado="Un movimiento oscilatorio tiene período y amplitud conocidos. Si se duplica la amplitud sin cambiar el período, podemos afirmar que:",
        opciones={
            "A": "La frecuencia angular es el doble del valor anterior.",
            "B": "El movimiento no corresponde a un movimiento armónico simple.",
            "C": "La fuerza de restitución máxima se cuadruplica.",
            "D": "La función que describe el movimiento final es el doble de la función inicial.",
        },
        correcta="D",
        claves=["MAS", "amplitud", "período", "fuerza de restitución", "selección"],
    )
    L.paso("Qué cambia y qué no")\
        .di("El período no cambia, así que omega tampoco: omega es dos pi entre te.",
            DATO("sin alterar su período"),
            F(PERIODO),
            M(r"T\ \text{igual}\;\Rightarrow\;\omega=\frac{2\pi}{T}\ \text{igual}"))\
        .di("En la posición, la amplitud multiplica a todo el coseno. Con el doble de a, toda la función se duplica.",
            F(POS),
            M(r"x_{\text{nueva}}=2A\cos(\omega t+\phi)=2\,x_{\text{vieja}}"))
    L.paso("La fuerza máxima solo se duplica")\
        .di("La fuerza del resorte es menos ka equis. Su valor máximo es en el extremo, ka por a: con el doble de a, el doble de fuerza, no el cuádruple.",
            F(r"F_x=-kx"),
            M(r"F_{\text{máx}}=kA\;\Rightarrow\;k(2A)=2kA"))
    L.paso("Qué opción es")\
        .di("Solo la de cambiar la función entera por el doble es verdadera.",
            R(r"x_{\text{nueva}}(t)=2\,x_{\text{vieja}}(t)"),
            OPCION("A", "descarta", "ω = 2π/T: si T no cambia, ω tampoco."),
            OPCION("B", "descarta", "Cambiar la amplitud no deja de ser MAS."),
            OPCION("C", "descarta", "F_máx = kA se duplica, no se cuadruplica."),
            OPCION("D", "correcta", "x = A cos(ωt + φ): con 2A toda la función se duplica."))
    L.usa(PERIODO, POS, r"F_x=-kx")
    L.oficial("I parcial ordinario II-2026", "D")
    return L


@leccion("P1O26-2")
def p1o26_2() -> Leccion:
    r1, I1, I2 = 7.5, 0.11, 1.0
    raiz = math.sqrt(I1 / I2)
    r2 = r1 * raiz
    L = Leccion(
        "P1O26-2", "El dron y la intensidad del sonido (I parcial ordinario II-2026, P2)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "2"),
        enunciado=(
            "Un dron emite sonido en todas direcciones como una fuente puntual. A $7{,}5\\ \\text{m}$ la intensidad es "
            "$0{,}11\\ \\text{W/m}^2$. ¿A qué distancia la intensidad llega al umbral de dolor ($1\\ \\text{W/m}^2$)?"
        ),
        opciones={"A": "0,8250 m", "B": "2,4875 m", "C": "6,2572 m", "D": "5,3636 m"},
        correcta="B",
        claves=["intensidad", "fuente puntual", "inverso del cuadrado", "casilla nueva", "selección"],
        aviso="Casillas nuevas del II-2026: I = P/(4πr²) e I₁/I₂ = r₂²/r₁² nunca estuvieron en una hoja de reposición.",
    )
    L.paso("La intensidad baja con la distancia al cuadrado")\
        .di("La potencia del dron se reparte en una esfera de área cuatro pi erre cuadrado. Por eso, más lejos, menos intensidad.",
            DATO("fuente puntual"),
            F(r"I=\frac{P}{4\pi r^2}"),
            F(r"\frac{I_1}{I_2}=\frac{r_2^2}{r_1^2}"))
    L.paso("Despejar la distancia nueva")\
        .di("Pasamos erre uno al cuadrado multiplicando y sacamos raíz.",
            M(r"r_2^2=r_1^2\,\frac{I_1}{I_2}\;\Rightarrow\;r_2=r_1\sqrt{\frac{I_1}{I_2}}"),
            M(rf"r_2=7{{,}}5\sqrt{{\frac{{0{{,}}11}}{{1}}}}=(7{{,}}5)({tex(raiz, 5)})={tex(r2, 5)}\ \text{{m}}"))\
        .di("Tiene sentido: para que la intensidad suba hay que acercarse.",
            R(rf"r_2={tex(r2, 5)}\ \text{{m}}"),
            OPCION("A", "descarta", "Usa 0,11 sin raíz: 7,5 × 0,11."),
            OPCION("B", "correcta", "7,5 × √0,11 = 2,4875 m."),
            OPCION("C", "descarta", "No sale de la relación del inverso del cuadrado."),
            OPCION("D", "descarta", "No sale de la relación del inverso del cuadrado."))
    L.usa(r"I=\frac{P}{4\pi r^2}", r"\frac{I_1}{I_2}=\frac{r_2^2}{r_1^2}")
    L.responde("r2", rf"r_2={tex(r2, 5)}\ \text{{m}}", r2, "m")
    L.oficial("I parcial ordinario II-2026", "B")
    return L


@leccion("P1O26-3")
def p1o26_3() -> Leccion:
    L = Leccion(
        "P1O26-3", "Péndulo físico con d/4: qué pasa con el período (I parcial ordinario II-2026, P3)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "3"),
        enunciado=(
            "Un péndulo físico de masa $m$ oscila alrededor de un eje a distancia $d$ de su centro de gravedad, con momento "
            "de inercia $I$. Se cambia por otro de igual $m$ e igual $I$, pero con la distancia al centro de gravedad igual a $d/4$. "
            "¿Qué le pasa al período de pequeñas oscilaciones?"
        ),
        opciones={"A": "Aumenta al doble.", "B": "Permanece constante.", "C": "Se reduce a la mitad.", "D": "Aumenta al cuádruple."},
        correcta="A",
        claves=["péndulo físico", "período", "momento de inercia", "selección"],
    )
    L.paso("El período del péndulo físico desde la hoja")\
        .di("Juntamos dos casillas: omega al cuadrado del péndulo físico y te igual dos pi entre omega.",
            DATO("d/4"),
            F(W_FISICO),
            F(PERIODO),
            M(r"T=2\pi\sqrt{\frac{I}{mgd}}"))
    L.paso("Cambiar d por d/4")\
        .di("La de está abajo dentro de la raíz. Dividirla entre cuatro multiplica lo de adentro por cuatro, y la raíz de cuatro es dos.",
            M(r"T'=2\pi\sqrt{\frac{I}{mg\,(d/4)}}=2\pi\sqrt{\frac{4I}{mgd}}"),
            M(r"T'=\sqrt{4}\cdot 2\pi\sqrt{\frac{I}{mgd}}=2T"),
            R(r"T'=2T"),
            OPCION("A", "correcta", "d/4 dentro de la raíz, abajo: el período se multiplica por √4 = 2."),
            OPCION("B", "descarta", "d sí está en la fórmula del período."),
            OPCION("C", "descarta", "d está abajo: si baja d, T sube."),
            OPCION("D", "descarta", "Falta la raíz: √4 = 2, no 4."))
    L.usa(W_FISICO, PERIODO)
    L.oficial("I parcial ordinario II-2026", "A")
    return L


@leccion("P1O26-4")
def p1o26_4() -> Leccion:
    L = Leccion(
        "P1O26-4", "Dos ondas en la misma cuerda (I parcial ordinario II-2026, P4)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "4"),
        enunciado=(
            "Dos ondas viajan en la misma cuerda. I. Si tienen la misma frecuencia, pueden tener distinta longitud de onda. "
            "II. Pueden tener distinta amplitud, pero siempre deben tener la misma velocidad."
        ),
        opciones={
            "A": "Ambas afirmaciones son falsas.",
            "B": "Ambas afirmaciones son verdaderas.",
            "C": "La II es falsa, pero la I es verdadera.",
            "D": "La I es falsa, pero la II es verdadera.",
        },
        correcta="D",
        claves=["rapidez de onda", "longitud de onda", "frecuencia", "selección"],
    )
    L.paso("La rapidez depende solo de la cuerda")\
        .di("En la casilla de rapidez de una cuerda solo aparecen la tensión y mu. En la misma cuerda, las dos ondas van igual de rápido.",
            DATO("misma cuerda"),
            F(V_CUERDA),
            M(r"\text{misma cuerda}\;\Rightarrow\;v\ \text{igual}"))
    L.paso("Con igual frecuencia, igual longitud de onda")\
        .di("Como ve es lambda por efe, si ve y efe son iguales, lambda también lo es. La uno es falsa y la dos verdadera.",
            F(V_ONDA),
            M(r"\lambda=\frac{v}{f}\;\Rightarrow\;\lambda\ \text{igual}"),
            R(r"\text{I falsa, II verdadera}"),
            OPCION("A", "descarta", "La II es verdadera: v solo depende de F y μ."),
            OPCION("B", "descarta", "La I es falsa: igual v e igual f dan igual λ."),
            OPCION("C", "descarta", "Está al revés."),
            OPCION("D", "correcta", "λ = v/f: misma v y misma f dan la misma λ; la amplitud no cambia v."))
    L.usa(V_CUERDA, V_ONDA)
    L.oficial("I parcial ordinario II-2026", "D")
    return L


@leccion("P1O26-5")
def p1o26_5() -> Leccion:
    L = Leccion(
        "P1O26-5", "En x = A/2, ¿qué parte de la energía es cinética? (I parcial ordinario II-2026, P5)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "5"),
        enunciado="Un sistema masa-resorte en MAS tiene amplitud $A$. Cuando la masa está en $x=A/2$, la razón entre la energía cinética y la energía total es:",
        opciones={"A": "0,25", "B": "0,50", "C": "0,75", "D": "1,00"},
        correcta="C",
        claves=["energía del MAS", "energía cinética", "energía potencial", "selección"],
    )
    L.paso("Energía total y energía potencial en A/2")\
        .di("La energía total es un medio de ka a al cuadrado. La potencial en a medios es un medio de ka por a medios al cuadrado.",
            DATO("x = A/2"),
            F(E_TOTAL),
            F(E_SUMA),
            M(r"U=\frac{1}{2}k\left(\frac{A}{2}\right)^2=\frac{1}{4}\cdot\frac{1}{2}kA^2=\frac{E}{4}"))
    L.paso("La cinética es lo que falta")\
        .di("Ka es la total menos la potencial: tres cuartos de la total.",
            M(r"K=E-U=E-\frac{E}{4}=\frac{3}{4}E"),
            M(r"\frac{K}{E}=\frac{3}{4}=0{,}75"),
            R(r"\frac{K}{E}=0{,}75"),
            OPCION("A", "descarta", "0,25 es U/E, no K/E."),
            OPCION("B", "descarta", "La mitad sería en x = A/√2."),
            OPCION("C", "correcta", "K = E − E/4 = 3E/4."),
            OPCION("D", "descarta", "K = E solo en x = 0."))
    L.usa(E_TOTAL, E_SUMA)
    L.oficial("I parcial ordinario II-2026", "C")
    return L


@leccion("P1O26-6")
def p1o26_6() -> Leccion:
    L = Leccion(
        "P1O26-6", "Nodos en el segundo armónico de una cuerda (I parcial ordinario II-2026, P6)",
        tema="ondas", tipo="seleccion", nivel=1, parcial=1, fuente=_f(_ORD, "6"),
        enunciado="Se produce una onda estacionaria en el segundo armónico en una cuerda de violín. ¿Cuántos nodos hay en la cuerda?",
        opciones={"A": "1", "B": "2", "C": "3", "D": "4"},
        correcta="C",
        claves=["onda estacionaria", "nodos", "armónico", "selección"],
    )
    L.paso("La longitud de onda del segundo armónico")\
        .di("Con ene igual dos, lambda es igual al largo de la cuerda: cabe una onda completa.",
            DATO("segundo armónico"),
            F(LAMBDA_N),
            M(r"\lambda_2=\frac{2L}{2}=L"))
    L.paso("Contar los nodos")\
        .di("Los nodos están cada media longitud de onda: en cero, en el medio y en el otro extremo. Son tres; en general, ene más uno.",
            M(r"x=0,\ \frac{L}{2},\ L\;\Rightarrow\;3\ \text{nodos}"),
            R(r"n+1=3\ \text{nodos}"),
            OPCION("A", "descarta", "Los dos extremos fijos ya son dos nodos."),
            OPCION("B", "descarta", "Eso es el primer armónico (solo los extremos)."),
            OPCION("C", "correcta", "Extremos + el centro: 3 nodos."),
            OPCION("D", "descarta", "Cuatro nodos es el tercer armónico."))
    L.usa(LAMBDA_N)
    L.oficial("I parcial ordinario II-2026", "C")
    return L


@leccion("P1O26-7")
def p1o26_7() -> Leccion:
    L = Leccion(
        "P1O26-7", "Resorte vertical: dónde es máxima la energía potencial (I parcial ordinario II-2026, P7)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_ORD, "7"),
        enunciado=(
            "Un sistema masa-resorte oscila verticalmente entre los puntos 1 y 3, con el equilibrio en el punto 2 "
            "(la distancia de 1 a 2 es igual a la de 2 a 3). Midiendo y desde el equilibrio, ¿dónde es máxima la "
            "energía potencial asociada al movimiento oscilatorio?"
        ),
        opciones={"A": "En 1.", "B": "En 2.", "C": "En 3.", "D": "Tanto en 1 como en 3."},
        correcta="D",
        claves=["energía potencial", "MAS vertical", "equilibrio", "selección"],
    )
    L.paso("La energía potencial del MAS se mide desde el equilibrio")\
        .di("En la casilla de energía, la potencial es un medio de ka por el desplazamiento al cuadrado, medido desde el equilibrio.",
            F(E_SUMA),
            M(r"U=\frac{1}{2}k\,y^2"))
    L.paso("Dónde es máxima")\
        .di("Ye al cuadrado es máxima en los dos extremos, porque arriba y abajo el desplazamiento tiene el mismo tamaño.",
            M(r"|y|=A\ \text{en 1 y en 3}\;\Rightarrow\;U_{\text{máx}}=\frac{1}{2}kA^2"),
            R(r"\text{en 1 y en 3}"),
            OPCION("A", "descarta", "En 3 el desplazamiento es igual de grande."),
            OPCION("B", "descarta", "En el equilibrio la energía potencial del MAS es cero."),
            OPCION("C", "descarta", "En 1 el desplazamiento es igual de grande."),
            OPCION("D", "correcta", "U = ½ky² es igual en los dos extremos."))
    L.usa(E_SUMA)
    L.oficial("I parcial ordinario II-2026", "D")
    return L


# ============================================================================ ordinario · desarrollos

@leccion("P1O26-8")
def p1o26_8() -> Leccion:
    m, E, x0, U0, t1 = 0.50, 2.0, 0.141, 1.0, 0.1047
    k = 2.0 * U0 / x0 ** 2
    kv = _q(k)
    A2 = 2.0 * E / kv
    A = math.sqrt(A2)
    Av = _q(A)
    w2 = kv / m
    w = math.sqrt(w2)
    wv = _q(w)
    cphi = x0 / Av
    phi = math.acos(cphi)
    phiv = _q(phi, 3)
    wA = wv * Av
    w2A = _q(w2) * Av
    wt = wv * t1
    fase = _q(wt) + phiv
    s = math.sin(fase)
    c = math.cos(fase)
    v = -_q(wA) * _q(s)
    a = -_q(w2A) * _q(c)

    L = Leccion(
        "P1O26-8", "Bloque en MAS leído de las curvas de energía (I parcial ordinario II-2026, P8)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(_ORD, "8"),
        enunciado=(
            "Un bloque de $0{,}50\\ \\text{kg}$ en MAS horizontal. La gráfica muestra $U$, $K$ y $E$ contra $x$: "
            "$E=2{,}0\\ \\text{J}$ y las curvas $U$ y $K$ se cruzan en $x=0{,}141\\ \\text{m}$ a $1{,}0\\ \\text{J}$. "
            "En $t=0$ el bloque está en $x=0{,}141\\ \\text{m}$ y se mueve hacia la izquierda ($v_x<0$). "
            "a) $E$, $k$ y $A$. b) $\\omega$ y $\\phi$. c) $v_x(t)$ y $a_x(t)$. d) Rapidez y aceleración en $t=0{,}1047\\ \\text{s}$."
        ),
        claves=["MAS", "energía", "fase", "resorte", "gráfica"],
        aviso=(
            "La solución oficial redondea ω a √200 = 14,14 rad/s (con k = 100,6 da √201,2 = 14,18) y escribe v(t) y a(t) "
            "sin el signo menos de la hoja. Aquí se usan los signos de la hoja; las respuestas cambian menos de 1 %."
        ),
    )
    L.paso("a) Leer la gráfica")\
        .di("La energía total es la línea de arriba, plana: dos joules. Donde U y K se cruzan, cada una vale la mitad: un joule.",
            DATO("0,141"),
            F(E_SUMA),
            M(r"E=2{,}0\ \text{J}\qquad x=0{,}141\ \text{m}:\ U=K=\frac{E}{2}=1{,}0\ \text{J}"))
    L.paso("a) Constante del resorte")\
        .di("En ese punto la energía potencial es un medio de ka equis al cuadrado. Despejamos ka.",
            M(r"U=\frac{1}{2}kx^2\;\Rightarrow\;k=\frac{2U}{x^2}"),
            M(rf"k=\frac{{2(1{{,}}0)}}{{(0{{,}}141)^2}}=\frac{{2{{,}}0}}{{0{{,}}019881}}={tex(k, 4)}\ \text{{N/m}}"),
            R(rf"k={tex(k, 4)}\ \text{{N/m}}"))
    L.paso("a) Amplitud")\
        .di("En el extremo toda la energía es potencial: la casilla de energía total. Despejamos a.",
            F(E_TOTAL),
            M(r"A^2=\frac{2E}{k}"),
            M(rf"A^2=\frac{{2(2{{,}}0)}}{{{tex(kv, 4)}}}={tex(A2, 4)}\ \text{{m}}^2"),
            M(rf"A=\sqrt{{{tex(A2, 4)}}}={tex(A, 4)}\ \text{{m}}"),
            R(rf"A={tex(A, 4)}\ \text{{m}}"))
    L.paso("b) Frecuencia angular")\
        .di("Con la casilla del resorte: omega al cuadrado es ka entre eme. Dividimos la constante entre la masa del bloque y después sacamos la raíz.",
            F(W_RES),
            M(rf"\omega^2=\frac{{{tex(kv, 4)}}}{{0{{,}}50}}={tex(w2, 4)}\ \text{{rad}}^2/\text{{s}}^2"),
            M(rf"\omega=\sqrt{{{tex(w2, 4)}}}={tex(w, 4)}\ \text{{rad/s}}"),
            R(rf"\omega={tex(w, 4)}\ \text{{rad/s}}"))
    L.paso("b) Fase: dos ángulos posibles")\
        .di("En te igual cero, la posición es a por coseno de fi. Despejamos coseno de fi.",
            F(POS),
            M(rf"\cos\phi=\frac{{x(0)}}{{A}}=\frac{{0{{,}}141}}{{{tex(Av, 4)}}}={tex(cphi, 4)}"))\
        .di("Un coseno positivo sale con dos ángulos: más pi cuartos y menos pi cuartos, es decir más y menos cuarenta y cinco grados.",
            M(rf"\phi=\pm{tex(phi, 3)}\ \text{{rad}}"))
    L.paso("b) El signo de la velocidad elige el ángulo")\
        .di("En la velocidad de la hoja hay un menos. Si el bloque va hacia la izquierda, ve es negativa, y entonces seno de fi tiene que ser positivo.",
            F(VEL),
            M(r"v(0)=-\omega A\sen\phi<0\;\Rightarrow\;\sen\phi>0"),
            M(rf"\phi={tex(phiv, 3)}\ \text{{rad}}\approx\frac{{\pi}}{{4}}"),
            R(rf"\phi={tex(phiv, 3)}\ \text{{rad}}"))
    L.paso("c) Velocidad y aceleración en función del tiempo")\
        .di("La velocidad de la hoja es menos omega por a por el seno. Primero multiplicamos omega por a.",
            F(VEL),
            M(rf"\omega A=({tex(wv, 4)})({tex(Av, 4)})={tex(wA, 4)}\ \text{{m/s}}"),
            M(rf"v_x(t)=-{tex(wA, 4)}\sen({tex(wv, 4)}\,t+{tex(phiv, 3)})"))\
        .di("La aceleración es menos omega al cuadrado por equis, y equis es a por el coseno. Multiplicamos omega al cuadrado por a.",
            F(r"a=-\omega^2x"),
            M(r"a_x(t)=-\omega^2A\cos(\omega t+\phi)"),
            M(rf"\omega^2A=({tex(_q(w2), 4)})({tex(Av, 4)})={tex(w2A, 4)}\ \text{{m/s}}^2"),
            M(rf"a_x(t)=-{tex(w2A, 4)}\cos({tex(wv, 4)}\,t+{tex(phiv, 3)})"))\
        .di("Las dos funciones quedan así, con sus unidades.",
            R(rf"v_x(t)=-{tex(wA, 4)}\sen({tex(wv, 4)}\,t+{tex(phiv, 3)})\ \text{{m/s}}"),
            R(rf"a_x(t)=-{tex(w2A, 4)}\cos({tex(wv, 4)}\,t+{tex(phiv, 3)})\ \text{{m/s}}^2"))
    L.paso("d) En t = 0,1047 s")\
        .di("Primero el ángulo: omega por te, más fi, en radianes. La calculadora tiene que estar en radianes.",
            M(rf"\omega t+\phi=({tex(wv, 4)})(0{{,}}1047)+{tex(phiv, 3)}={tex(fase, 4)}\ \text{{rad}}"),
            OJO("Calculadora en RAD, no en DEG."))\
        .di("Después el seno y el coseno de ese ángulo, y los multiplicamos.",
            M(rf"v=-({tex(wA, 4)})\sen({tex(fase, 4)})=-({tex(wA, 4)})({tex(s, 4)})={tex(v, 4)}\ \text{{m/s}}"),
            M(rf"a=-({tex(w2A, 4)})\cos({tex(fase, 4)})=-({tex(w2A, 4)})({tex(c, 4)})={tex(a, 4)}\ \text{{m/s}}^2"),
            R(rf"|v|={tex(abs(v), 3)}\ \text{{m/s}}\qquad |a|={tex(abs(a), 3)}\ \text{{m/s}}^2"))
    L.usa(E_SUMA, E_TOTAL, W_RES, POS, VEL, r"a=-\omega^2x")
    L.responde("E", r"E=2{,}0\ \text{J}", 2.0, "J")
    L.responde("k", rf"k={tex(k, 4)}\ \text{{N/m}}", k, "N/m")
    L.responde("A", rf"A={tex(A, 4)}\ \text{{m}}", A, "m")
    L.responde("w", rf"\omega={tex(w, 4)}\ \text{{rad/s}}", w, "rad/s")
    L.responde("phi", rf"\phi={tex(phiv, 3)}\ \text{{rad}}", phiv, "rad")
    L.responde("v", rf"|v|={tex(abs(v), 3)}\ \text{{m/s}}", abs(v), "m/s")
    L.responde("a", rf"|a|={tex(abs(a), 3)}\ \text{{m/s}}^2", abs(a), "m/s^2")
    L.oficial("I parcial ordinario II-2026", 2.0, 100.6, 0.1994, 14.14, 0.785, 2.17, 25.59, tol=0.015,
              nota="La oficial usa ω = √200 y A = 0,20 redondeados.")
    L.te_toca("Con los mismos datos, ¿cuál es la rapidez máxima del bloque?", wA, "m/s", tol=0.01,
              pista="v_máx = ωA.", solucion=f"ωA ≈ {tex(wA, 3).replace('{,}', ',')} m/s.")
    return L


@leccion("P1O26-9")
def p1o26_9() -> Leccion:
    L1, m, f1 = 0.900, 0.01215, 180.0
    mu = m / L1
    lam1 = 2.0 * L1
    v1 = lam1 * f1
    raiz = math.sqrt(1.04)
    v2 = raiz * v1
    f3n = 842.4
    f1n = f3n / 3.0
    L2 = v2 / (2.0 * f1n)
    L2v = _q(L2)
    dL = L1 - L2v
    lam3 = 2.0 * L2v / 3.0

    L = Leccion(
        "P1O26-9", "Cuerda de instrumento presionada contra un traste (I parcial ordinario II-2026, P9)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(_ORD, "9"),
        enunciado=(
            "Una cuerda de $L_1=0{,}900\\ \\text{m}$ y $12{,}15\\ \\text{g}$ vibra en su fundamental a $180\\ \\text{Hz}$. "
            "Al presionarla contra un traste, su longitud vibrante baja a $L_2$ y la tensión sube un $4{,}00\\ \\%$ ($\\mu$ igual). "
            "Así, el tercer armónico suena a $842{,}4\\ \\text{Hz}$. a) $\\mu$ y $v_1$. b) $v_2$. c) Nueva fundamental $f_1'$ y $L_2$. "
            "d) Distancia $\\Delta L$ del extremo al traste. e) $\\lambda_3'$."
        ),
        claves=["cuerda", "armónicos", "tensión", "rapidez de onda", "traste"],
    )
    L.paso("a) Densidad lineal y rapidez inicial")\
        .di("Mu es masa entre largo, con la masa en kilogramos.",
            DATO("12,15 g"),
            F(MU),
            M(rf"\mu=\frac{{0{{,}}01215}}{{0{{,}}900}}={tex(mu, 3)}\ \text{{kg/m}}"))\
        .di("En la fundamental, ene igual uno: lambda es dos veces el largo. Después ve es lambda por efe.",
            F(LAMBDA_N),
            F(V_ONDA),
            M(rf"\lambda_1=\frac{{2(0{{,}}900)}}{{1}}={tex(lam1, 4)}\ \text{{m}}"),
            M(rf"v_1=({tex(lam1, 4)})(180)={tex(v1, 3)}\ \text{{m/s}}"),
            R(rf"\mu={tex(mu, 3)}\ \text{{kg/m}}\qquad v_1={tex(v1, 3)}\ \text{{m/s}}"))
    L.paso("b) La tensión sube 4 %: nueva rapidez")\
        .di("Con la casilla de rapidez en una cuerda hacemos el cociente: mu se cancela y queda la raíz del cociente de tensiones.",
            F(V_CUERDA),
            M(r"\frac{v_2}{v_1}=\sqrt{\frac{F_2/\mu}{F_1/\mu}}=\sqrt{\frac{F_2}{F_1}}=\sqrt{1{,}04}"),
            M(rf"v_2=\sqrt{{1{{,}}04}}\,(324)=({tex(raiz, 5)})(324)={tex(v2, 5)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 5)}\ \text{{m/s}}"))
    L.paso("c) Nueva fundamental y nueva longitud")\
        .di("El tercer armónico es tres veces la fundamental, así que dividimos entre tres.",
            F(ARMONICOS),
            M(rf"f_1'=\frac{{842{{,}}4}}{{3}}={tex(f1n, 4)}\ \text{{Hz}}"))\
        .di("En la fundamental, efe es ve entre dos ele. Despejamos ele dos.",
            F(F_N),
            M(r"f_1'=\frac{v_2}{2L_2}\;\Rightarrow\;L_2=\frac{v_2}{2f_1'}"),
            M(rf"L_2=\frac{{{tex(v2, 5)}}}{{2({tex(f1n, 4)})}}={tex(L2, 4)}\ \text{{m}}"),
            R(rf"f_1'={tex(f1n, 4)}\ \text{{Hz}}\qquad L_2={tex(L2, 4)}\ \text{{m}}"))
    L.paso("d) Dónde está el traste")\
        .di("La parte que ya no vibra es el largo viejo menos el nuevo.",
            M(rf"\Delta L=0{{,}}900-{tex(L2v, 4)}={tex(dL, 4)}\ \text{{m}}"),
            R(rf"\Delta L={tex(dL, 3)}\ \text{{m}}"))
    L.paso("e) Longitud de onda del tercer armónico")\
        .di("Con ene igual tres y el largo nuevo.",
            M(rf"\lambda_3'=\frac{{2L_2}}{{3}}=\frac{{2({tex(L2v, 4)})}}{{3}}={tex(lam3, 4)}\ \text{{m}}"),
            R(rf"\lambda_3'={tex(lam3, 3)}\ \text{{m}}"))
    L.usa(MU, LAMBDA_N, V_ONDA, V_CUERDA, ARMONICOS, F_N)
    L.responde("a-mu", rf"\mu={tex(mu, 3)}\ \text{{kg/m}}", mu, "kg/m")
    L.responde("a-v1", rf"v_1={tex(v1, 3)}\ \text{{m/s}}", v1, "m/s")
    L.responde("b", rf"v_2={tex(v2, 5)}\ \text{{m/s}}", v2, "m/s")
    L.responde("c-f", rf"f_1'={tex(f1n, 4)}\ \text{{Hz}}", f1n, "Hz")
    L.responde("c-L", rf"L_2={tex(L2, 4)}\ \text{{m}}", L2, "m")
    L.responde("d", rf"\Delta L={tex(dL, 3)}\ \text{{m}}", dL, "m")
    L.responde("e", rf"\lambda_3'={tex(lam3, 3)}\ \text{{m}}", lam3, "m")
    L.oficial("I parcial ordinario II-2026", 0.0135, 324, 330.42, 280.8, 0.588, 0.312, 0.392, tol=0.01)
    L.te_toca("Sin traste y con la tensión original, ¿en qué frecuencia suena el tercer armónico?", 3 * f1, "Hz", tol=0.01,
              pista="f_n = n·f₁.", solucion="3 × 180 = 540 Hz.")
    return L


# ============================================================================ extraordinario · selecciones

@leccion("P1E26-1")
def p1e26_1() -> Leccion:
    L = Leccion(
        "P1E26-1", "Péndulo simple con la cuerda cuatro veces más corta (I parcial extraordinario II-2026, P1)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "1"),
        enunciado="Un péndulo simple de longitud $L$ oscila con período $T$ (pequeñas amplitudes). Si la longitud se reduce a la cuarta parte, ¿qué le ocurre al período?",
        opciones={"A": "Se reduce a la mitad.", "B": "Permanece constante.", "C": "Aumenta al doble.", "D": "Aumenta al cuádruple."},
        correcta="A",
        claves=["péndulo simple", "período", "longitud", "selección"],
        aviso=_CLAVE_OTRA.format(pub="C", bien="A"),
    )
    L.paso("El período del péndulo simple desde la hoja")\
        .di("Juntamos omega al cuadrado igual ge entre ele con te igual dos pi entre omega.",
            DATO("cuarta parte"),
            F(W_SIMPLE),
            F(PERIODO),
            M(r"T=2\pi\sqrt{\frac{L}{g}}"))
    L.paso("Cambiar L por L/4")\
        .di("Ele está arriba dentro de la raíz. Dividirla entre cuatro divide el período entre raíz de cuatro, que es dos.",
            M(r"T'=2\pi\sqrt{\frac{L/4}{g}}=\frac{1}{\sqrt{4}}\cdot 2\pi\sqrt{\frac{L}{g}}=\frac{T}{2}"),
            R(r"T'=\frac{T}{2}"),
            OPCION("A", "correcta", "L/4 arriba en la raíz: T se divide entre √4 = 2."),
            OPCION("B", "descarta", "L sí está en la fórmula del período."),
            OPCION("C", "descarta", "Más corto oscila más rápido: T baja."),
            OPCION("D", "descarta", "Además de subir, olvida la raíz."))
    L.usa(W_SIMPLE, PERIODO)
    return L


@leccion("P1E26-2")
def p1e26_2() -> Leccion:
    k, w = 4.0, 50.0
    lam = 2.0 * math.pi / k
    v = w / k
    L = Leccion(
        "P1E26-2", "Leer λ y v de y = 0,025 cos(4,0x − 50t) (I parcial extraordinario II-2026, P2)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "2"),
        enunciado="Una onda transversal va hacia $+x$: $y(x,t)=0{,}025\\cos(4{,}0x-50t)$ (SI). ¿Cuál afirmación es correcta?",
        opciones={
            "A": "λ = 0,7854 m y v = 80,00 m/s.",
            "B": "λ = 1,571 m y v = 12,50 m/s.",
            "C": "λ = 1,571 m y v = 5,000 m/s.",
            "D": "λ = 6,283 m y v = 12,50 m/s.",
        },
        correcta="B",
        claves=["onda sinusoidal", "número de onda", "longitud de onda", "rapidez", "selección"],
        aviso=_CLAVE_OTRA.format(pub="D", bien="B"),
    )
    L.paso("Comparar con la casilla de la onda viajera")\
        .di("El número que multiplica a equis es ka y el que multiplica a te es omega.",
            F(r"y(x,t)=A\cos(kx-\omega t)"),
            M(r"k=4{,}0\ \text{rad/m}\qquad \omega=50\ \text{rad/s}"))
    L.paso("Longitud de onda y rapidez")\
        .di("Lambda sale del número de onda, y la rapidez es omega entre ka.",
            F(K_ONDA),
            M(rf"\lambda=\frac{{2\pi}}{{4{{,}}0}}={tex(lam, 4)}\ \text{{m}}"),
            F(V_ONDA),
            M(rf"v=\frac{{50}}{{4{{,}}0}}={tex(v, 4)}\ \text{{m/s}}"),
            R(rf"\lambda={tex(lam, 4)}\ \text{{m}}\qquad v={tex(v, 4)}\ \text{{m/s}}"),
            OPCION("A", "descarta", "Ni λ ni v salen de 2π/k y ω/k."),
            OPCION("B", "correcta", "λ = 2π/k = 1,571 m y v = ω/k = 12,50 m/s."),
            OPCION("C", "descarta", "La rapidez está mal: ω/k = 12,5."),
            OPCION("D", "descarta", "6,283 = 2π: olvida dividir entre k."))
    L.usa(K_ONDA, V_ONDA)
    L.responde("lambda", rf"\lambda={tex(lam, 4)}\ \text{{m}}", lam, "m")
    L.responde("v", rf"v={tex(v, 4)}\ \text{{m/s}}", v, "m/s")
    return L


@leccion("P1E26-3")
def p1e26_3() -> Leccion:
    L = Leccion(
        "P1E26-3", "Potencia media con el doble de frecuencia y la mitad de amplitud (I parcial extraordinario II-2026, P3)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "3"),
        enunciado=(
            "Dos ondas viajan por cuerdas idénticas con la misma tensión. La onda 1 tiene el doble de frecuencia y la mitad "
            "de amplitud que la onda 2. Sobre la potencia media que transportan:"
        ),
        opciones={
            "A": "La onda 1 transporta la cuarta parte de la potencia de la onda 2.",
            "B": "La onda 1 transporta la mitad de la potencia de la onda 2.",
            "C": "Ambas ondas transportan la misma potencia media.",
            "D": "La onda 1 transporta el doble de la potencia de la onda 2.",
        },
        correcta="C",
        claves=["potencia media", "onda en cuerda", "frecuencia", "amplitud", "selección"],
        aviso=_CLAVE_OTRA.format(pub="B", bien="C"),
    )
    L.paso("La casilla de potencia media")\
        .di("Mu y la tensión son iguales en las dos cuerdas. Lo que cambia es omega al cuadrado por a al cuadrado.",
            F(P_MED),
            M(r"\frac{P_1}{P_2}=\frac{\omega_1^2A_1^2}{\omega_2^2A_2^2}"))
    L.paso("Doble de frecuencia y mitad de amplitud")\
        .di("El doble de omega da cuatro veces más; la mitad de a da cuatro veces menos. Se compensan.",
            M(r"\frac{P_1}{P_2}=\frac{(2\omega)^2\left(\frac{A}{2}\right)^2}{\omega^2A^2}=\frac{4\cdot\frac{1}{4}\,\omega^2A^2}{\omega^2A^2}=1"),
            R(r"P_1=P_2"),
            OPCION("A", "descarta", "Olvida que ω está al cuadrado."),
            OPCION("B", "descarta", "Olvida los cuadrados."),
            OPCION("C", "correcta", "(2ω)²(A/2)² = ω²A²: la misma potencia."),
            OPCION("D", "descarta", "Olvida que A también está al cuadrado."))
    L.usa(P_MED)
    return L


@leccion("P1E26-4")
def p1e26_4() -> Leccion:
    L = Leccion(
        "P1E26-4", "Qué es cierto en un oscilador masa-resorte (I parcial extraordinario II-2026, P4)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "4"),
        enunciado="Un bloque unido a un resorte ideal oscila horizontalmente sin fricción con amplitud $A$, constante $k$ y período $T$. ¿Cuál afirmación es correcta?",
        opciones={
            "A": "La aceleración es máxima en x = 0, donde la velocidad es nula.",
            "B": "Si la amplitud se duplica, el período también se duplica.",
            "C": "En todo instante la velocidad y la aceleración apuntan en la misma dirección.",
            "D": "La fuerza restauradora es proporcional al desplazamiento y opuesta a él.",
        },
        correcta="D",
        claves=["MAS", "ley de Hooke", "período", "aceleración", "selección"],
        aviso=_CLAVE_OTRA.format(pub="C", bien="D"),
    )
    L.paso("Revisar cada afirmación con la hoja")\
        .di("La aceleración es menos omega al cuadrado por equis: en equis cero es cero, no máxima. Y ahí la velocidad es máxima.",
            F(r"a=-\omega^2x"),
            M(r"x=0\;\Rightarrow\;a=0"))\
        .di("El período depende de ka y eme, no de la amplitud.",
            F(W_RES),
            M(r"T=\frac{2\pi}{\omega}=2\pi\sqrt{\frac{m}{k}}\ \text{(sin A)}"))
    L.paso("La ley de Hooke")\
        .di("La fuerza es menos ka equis: su tamaño crece con el desplazamiento y el signo menos dice que apunta al revés.",
            F(r"F_x=-kx"),
            R(r"F_x=-kx"),
            OPCION("A", "descarta", "En x = 0 la aceleración es cero y la velocidad es máxima."),
            OPCION("B", "descarta", "T = 2π√(m/k) no depende de A."),
            OPCION("C", "descarta", "Al alejarse del centro, la velocidad y la aceleración apuntan al revés."),
            OPCION("D", "correcta", "F = −kx: proporcional y opuesta al desplazamiento."))
    L.usa(r"a=-\omega^2x", W_RES, r"F_x=-kx")
    return L


@leccion("P1E26-5")
def p1e26_5() -> Leccion:
    L = Leccion(
        "P1E26-5", "Dos pulsos opuestos que se cruzan (I parcial extraordinario II-2026, P5)",
        tema="ondas", tipo="seleccion", nivel=1, parcial=1, fuente=_f(_EXT, "5"),
        enunciado=(
            "Dos pulsos de igual forma y amplitud viajan por la misma cuerda en sentidos opuestos; uno hacia arriba y otro hacia abajo. "
            "Durante la superposición, ¿qué ocurre?"
        ),
        opciones={
            "A": "La cuerda queda un instante sin desplazamiento, pero los pulsos siguen después.",
            "B": "Los pulsos desaparecen para siempre porque sus energías se anulan.",
            "C": "La rapidez de los pulsos se hace cero durante la superposición.",
            "D": "La tensión de la cuerda se anula cuando los pulsos coinciden.",
        },
        correcta="A",
        claves=["superposición", "pulsos", "interferencia destructiva", "selección"],
        aviso=_CLAVE_OTRA.format(pub="D", bien="A"),
    )
    L.paso("Superposición: los desplazamientos se suman")\
        .di("Cuando se cruzan, el desplazamiento de la cuerda es la suma de los dos. Uno es más ye y el otro menos ye: suman cero.",
            M(r"y=y_1+y_2=y_1+(-y_1)=0"))\
        .di("La rapidez de cada pulso depende solo de la cuerda, no de que se crucen. Por eso siguen igual de rápido.",
            F(V_CUERDA),
            M(r"F\ \text{y}\ \mu\ \text{no cambian}\;\Rightarrow\;v\ \text{no cambia}"))
    L.paso("Qué opción es")\
        .di("La energía no desaparece: en ese instante la cuerda está plana pero se mueve. Después cada pulso sigue su camino.",
            R(r"y=0\ \text{un instante; los pulsos siguen}"),
            OPCION("A", "correcta", "Se suman a cero un instante y siguen viajando."),
            OPCION("B", "descarta", "La energía está en el movimiento de la cuerda: no se pierde."),
            OPCION("C", "descarta", "v = √(F/μ) no cambia."),
            OPCION("D", "descarta", "La tensión sigue siendo la misma."))
    L.usa(V_CUERDA)
    return L


@leccion("P1E26-6")
def p1e26_6() -> Leccion:
    L = Leccion(
        "P1E26-6", "En x = A/2: relación entre K y U (I parcial extraordinario II-2026, P6)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "6"),
        enunciado="Una masa oscila con amplitud $A$ en un resorte. En $x=\\tfrac{1}{2}A$, ¿qué relación hay entre la energía cinética $K$ y la potencial $U$?",
        opciones={"A": "K = U", "B": "K = 2U", "C": "K = 3U", "D": "K = 4U"},
        correcta="C",
        claves=["energía del MAS", "energía cinética", "energía potencial", "selección"],
    )
    L.paso("U en A/2 es un cuarto de la total")\
        .di("Un medio de ka por a medios al cuadrado es un cuarto de la energía total.",
            F(E_TOTAL),
            F(E_SUMA),
            M(r"U=\frac{1}{2}k\left(\frac{A}{2}\right)^2=\frac{E}{4}"))
    L.paso("K es el resto")\
        .di("La cinética es tres cuartos de la total: tres veces la potencial.",
            M(r"K=E-\frac{E}{4}=\frac{3E}{4}"),
            M(r"\frac{K}{U}=\frac{3E/4}{E/4}=3"),
            R(r"K=3U"),
            OPCION("A", "descarta", "K = U pasa en x = A/√2."),
            OPCION("B", "descarta", "No sale de E/4 y 3E/4."),
            OPCION("C", "correcta", "K = 3E/4 y U = E/4: K = 3U."),
            OPCION("D", "descarta", "No sale de E/4 y 3E/4."))
    L.usa(E_TOTAL, E_SUMA)
    L.oficial("I parcial extraordinario II-2026", "C")
    return L


@leccion("P1E26-7")
def p1e26_7() -> Leccion:
    L = Leccion(
        "P1E26-7", "Dos cuerdas: 4F con 2μ, ¿qué pasa con la rapidez? (I parcial extraordinario II-2026, P7)",
        tema="ondas", tipo="seleccion", nivel=2, parcial=1, fuente=_f(_EXT, "7"),
        enunciado="Dos cuerdas de igual longitud: la 1 con tensión $F$ y densidad $\\mu$; la 2 con tensión $4F$ y densidad $2\\mu$. La relación entre sus rapideces es:",
        opciones={"A": "v₂ = v₁/√2", "B": "v₂ = √2·v₁", "C": "v₂ = 2v₁", "D": "v₂ = 4v₁"},
        correcta="B",
        claves=["rapidez de onda", "tensión", "densidad lineal", "selección"],
        aviso=_CLAVE_OTRA.format(pub="A", bien="B"),
    )
    L.paso("Cociente de rapideces")\
        .di("Con la casilla de rapidez en una cuerda, dividimos la dos entre la uno.",
            DATO("4F"),
            F(V_CUERDA),
            M(r"\frac{v_2}{v_1}=\sqrt{\frac{4F/(2\mu)}{F/\mu}}=\sqrt{\frac{4}{2}}=\sqrt{2}"),
            R(r"v_2=\sqrt{2}\,v_1"),
            OPCION("A", "descarta", "Está invertido."),
            OPCION("B", "correcta", "√(4/2) = √2."),
            OPCION("C", "descarta", "Olvida dividir entre el doble de μ."),
            OPCION("D", "descarta", "Olvida la raíz y la μ."))
    L.usa(V_CUERDA)
    return L


# ============================================================================ extraordinario · desarrollos

@leccion("P1E26-8")
def p1e26_8() -> Leccion:
    Lp, m, mv, me, d = 2.00, 1.20, 0.60, 0.60, 1.50
    I_cm_v = mv * Lp ** 2 / 12.0
    I_v = I_cm_v + mv * (Lp / 2.0) ** 2
    I_e = me * Lp ** 2
    I_B = I_v + I_e
    TA = 2.0 * math.pi * math.sqrt(Lp / G)
    mgd = m * G * d
    TB = 2.0 * math.pi * math.sqrt(I_B / mgd)
    NA = 300.0 / _q(TA)
    NB = 300.0 / _q(TB)
    Leq = I_B / (m * d)

    L = Leccion(
        "P1E26-8", "Péndulo simple contra péndulo físico de 2,00 m (I parcial extraordinario II-2026, P8)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(_EXT, "8"),
        enunciado=(
            "Dos péndulos de $L=2{,}00\\ \\text{m}$ y $1{,}20\\ \\text{kg}$. A: esfera pequeña en un alambre sin masa (péndulo simple). "
            "B: varilla uniforme de $0{,}60\\ \\text{kg}$ con una esfera de $0{,}60\\ \\text{kg}$ en el extremo; su centro de masa está a "
            "$d=1{,}50\\ \\text{m}$ del pivote. Para la varilla, $I_{\\text{cm}}=\\tfrac{1}{12}m_vL^2$. a) $I_B$ respecto al pivote. "
            "b) $T_A$. c) $T_B$. d) Oscilaciones completas de cada uno en 5 min. e) Longitud de A para que tenga el período de B."
        ),
        claves=["péndulo físico", "péndulo simple", "momento de inercia", "ejes paralelos", "período"],
        aviso="El encabezado de la solución oficial dice «15/04/2023» y su clave de selección no corresponde a este examen; los resultados de los desarrollos sí coinciden.",
    )
    L.paso("a) Inercia de la varilla respecto al pivote")\
        .di("El centro de la varilla está a un metro del pivote. Con el teorema de ejes paralelos de la hoja sumamos eme de al cuadrado.",
            DATO("d = 1,50 m"),
            F(EJES),
            M(rf"I_{{\text{{cm}},v}}=\frac{{1}}{{12}}(0{{,}}60)(2{{,}}00)^2={tex(I_cm_v, 3)}\ \text{{kg}}\cdot\text{{m}}^2"),
            M(rf"I_v={tex(I_cm_v, 3)}+(0{{,}}60)(1{{,}}00)^2={tex(I_v, 3)}\ \text{{kg}}\cdot\text{{m}}^2"))
    L.paso("a) Inercia de la esfera y total")\
        .di("La esfera es pequeña: es como una masa puntual a dos metros. Su inercia es eme por ele al cuadrado. Después sumamos.",
            M(rf"I_e=(0{{,}}60)(2{{,}}00)^2={tex(I_e, 3)}\ \text{{kg}}\cdot\text{{m}}^2"),
            M(rf"I_B={tex(I_v, 3)}+{tex(I_e, 3)}={tex(I_B, 3)}\ \text{{kg}}\cdot\text{{m}}^2"),
            R(rf"I_B={tex(I_B, 3)}\ \text{{kg}}\cdot\text{{m}}^2"))
    L.paso("b) Período del péndulo simple")\
        .di("Juntamos omega al cuadrado igual ge entre ele con te igual dos pi entre omega.",
            F(W_SIMPLE),
            F(PERIODO),
            M(r"T_A=2\pi\sqrt{\frac{L}{g}}"),
            M(rf"T_A=2\pi\sqrt{{\frac{{2{{,}}00}}{{9{{,}}80}}}}={tex(TA, 4)}\ \text{{s}}"),
            R(rf"T_A={tex(TA, 3)}\ \text{{s}}"))
    L.paso("c) Período del péndulo físico")\
        .di("Ahora con la casilla del péndulo físico: omega al cuadrado es eme ge de entre i.",
            F(W_FISICO),
            M(r"T_B=2\pi\sqrt{\frac{I_B}{mgd}}"),
            M(rf"mgd=(1{{,}}20)(9{{,}}80)(1{{,}}50)={tex(mgd, 4)}\ \text{{N}}\cdot\text{{m}}"),
            M(rf"T_B=2\pi\sqrt{{\frac{{{tex(I_B, 3)}}}{{{tex(mgd, 4)}}}}}={tex(TB, 4)}\ \text{{s}}"),
            R(rf"T_B={tex(TB, 3)}\ \text{{s}}"))
    L.paso("d) Oscilaciones completas en 5 minutos")\
        .di("Cinco minutos son trescientos segundos. Dividimos entre cada período y nos quedamos con la parte entera.",
            M(rf"N_A=\frac{{300}}{{{tex(TA, 4)}}}={tex(NA, 4)}\;\Rightarrow\;105"),
            M(rf"N_B=\frac{{300}}{{{tex(TB, 4)}}}={tex(NB, 4)}\;\Rightarrow\;112"),
            R(r"N_A=105\qquad N_B=112"))
    L.paso("e) Largo del péndulo simple equivalente")\
        .di("Igualamos las dos omegas al cuadrado y despejamos ele.",
            M(r"\frac{g}{L}=\frac{mgd}{I_B}\;\Rightarrow\;L=\frac{I_B}{md}"),
            M(rf"L=\frac{{{tex(I_B, 3)}}}{{(1{{,}}20)(1{{,}}50)}}={tex(Leq, 4)}\ \text{{m}}"),
            R(rf"L={tex(Leq, 3)}\ \text{{m}}"))
    L.usa(EJES, W_SIMPLE, W_FISICO, PERIODO)
    L.responde("a", rf"I_B={tex(I_B, 3)}\ \text{{kg}}\cdot\text{{m}}^2", I_B, "kg·m^2")
    L.responde("b", rf"T_A={tex(TA, 3)}\ \text{{s}}", TA, "s")
    L.responde("c", rf"T_B={tex(TB, 3)}\ \text{{s}}", TB, "s")
    L.responde("d-A", r"N_A=105", 105.0, "")
    L.responde("d-B", r"N_B=112", 112.0, "")
    L.responde("e", rf"L={tex(Leq, 3)}\ \text{{m}}", Leq, "m")
    L.oficial("I parcial extraordinario II-2026 (desarrollo)", 3.20, 2.84, 2.68, 105, 112, 1.78, tol=0.01)
    L.te_toca("¿Cuál sería el período del péndulo A en la Luna (g = 1,62 m/s²)?",
              2.0 * math.pi * math.sqrt(Lp / 1.62), "s", tol=0.01, pista="T = 2π√(L/g).", solucion="2π√(2,00/1,62) ≈ 6,98 s.")
    return L


@leccion("P1E26-9")
def p1e26_9() -> Leccion:
    m, n, medio, F_t, A = 0.0480, 4, 0.240, 96.0, 0.0150
    lam = 2.0 * medio
    Lc = n * medio
    mu = m / Lc
    v2 = F_t / mu
    v = math.sqrt(v2)
    vv = _q(v)
    f = vv / lam
    w = 2.0 * math.pi * _q(f)
    wv = _q(w)
    amax = wv ** 2 * A
    vn = 1.20 * vv
    f1n = vn / (2.0 * Lc)

    L = Leccion(
        "P1E26-9", "Cuerda en el banco de pruebas: armónico, v, aceleración y nueva tensión (I parcial extraordinario II-2026, P9)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f(_EXT, "9"),
        enunciado=(
            "Una cuerda de $48{,}0\\ \\text{g}$ fija en sus dos extremos vibra con 4 antinodos. Entre el primer y el segundo nodo interior "
            "hay $24{,}0\\ \\text{cm}$. Tensión $96{,}0\\ \\text{N}$, amplitud en los antinodos $1{,}50\\ \\text{cm}$. a) Armónico $n$ y largo $L$. "
            "b) Rapidez $v$ y $\\mu$. c) Aceleración transversal máxima en un antinodo. d) Si la tensión sube 44 %, nueva fundamental."
        ),
        claves=["onda estacionaria", "armónico", "nodos", "antinodos", "rapidez de onda"],
    )
    L.paso("a) Qué armónico y qué largo")\
        .di("Con los dos extremos fijos, el número de antinodos es el número del armónico: cuatro.",
            DATO("4 antinodos"),
            M(r"n=4"))\
        .di("De un nodo al siguiente hay media longitud de onda. Y el largo tiene cuatro medias longitudes, una por antinodo.",
            F(LAMBDA_N),
            M(rf"\frac{{\lambda}}{{2}}=0{{,}}240\ \text{{m}}\;\Rightarrow\;\lambda={tex(lam, 3)}\ \text{{m}}"),
            M(rf"L=\frac{{n\lambda}}{{2}}=(4)(0{{,}}240)={tex(Lc, 3)}\ \text{{m}}"),
            R(rf"n=4\qquad L={tex(Lc, 3)}\ \text{{m}}"))
    L.paso("b) Densidad lineal y rapidez")\
        .di("Mu es masa entre largo, con la masa en kilogramos. Después usamos la casilla de rapidez en una cuerda: raíz de la tensión entre mu.",
            F(MU),
            M(rf"\mu=\frac{{0{{,}}0480}}{{{tex(Lc, 3)}}}={tex(mu, 3)}\ \text{{kg/m}}"),
            F(V_CUERDA),
            M(rf"v=\sqrt{{\frac{{96{{,}}0}}{{{tex(mu, 3)}}}}}=\sqrt{{{tex(v2, 4)}}}={tex(v, 4)}\ \text{{m/s}}"),
            R(rf"\mu={tex(mu, 3)}\ \text{{kg/m}}\qquad v={tex(v, 4)}\ \text{{m/s}}"))
    L.paso("c) Aceleración máxima en un antinodo")\
        .di("Cada punto del antinodo hace un MAS con la frecuencia de la onda. Primero efe y omega.",
            F(F_N),
            M(rf"f=\frac{{v}}{{\lambda}}=\frac{{{tex(vv, 4)}}}{{{tex(lam, 3)}}}={tex(f, 4)}\ \text{{Hz}}"),
            F(W_F),
            M(rf"\omega=2\pi({tex(_q(f), 4)})={tex(w, 4)}\ \text{{rad/s}}"))\
        .di("La aceleración máxima de un MAS es omega al cuadrado por la amplitud, que aquí es la del antinodo.",
            F(r"a=-\omega^2x"),
            M(rf"a_{{\text{{máx}}}}=\omega^2A=({tex(wv, 4)})^2(0{{,}}0150)={tex(amax, 4)}\ \text{{m/s}}^2"),
            R(rf"a_{{\text{{máx}}}}={tex(amax, 4)}\ \text{{m/s}}^2"))
    L.paso("d) Tensión 44 % mayor")\
        .di("La rapidez va con la raíz de la tensión: raíz de uno coma cuarenta y cuatro es uno coma veinte.",
            M(r"\frac{v'}{v}=\sqrt{1{,}44}=1{,}20"),
            M(rf"v'=1{{,}}20({tex(vv, 4)})={tex(vn, 4)}\ \text{{m/s}}"))\
        .di("En la fundamental, la longitud de onda es dos veces el largo.",
            M(rf"f_1'=\frac{{v'}}{{2L}}=\frac{{{tex(vn, 4)}}}{{2({tex(Lc, 3)})}}={tex(f1n, 4)}\ \text{{Hz}}"),
            R(rf"f_1'={tex(f1n, 4)}\ \text{{Hz}}"))
    L.usa(LAMBDA_N, MU, V_CUERDA, F_N, W_F, r"a=-\omega^2x")
    L.responde("a-n", r"n=4", 4.0, "")
    L.responde("a-L", rf"L={tex(Lc, 3)}\ \text{{m}}", Lc, "m")
    L.responde("b-v", rf"v={tex(v, 4)}\ \text{{m/s}}", v, "m/s")
    L.responde("b-mu", rf"\mu={tex(mu, 3)}\ \text{{kg/m}}", mu, "kg/m")
    L.responde("c", rf"a_{{\text{{máx}}}}={tex(amax, 4)}\ \text{{m/s}}^2", amax, "m/s^2")
    L.responde("d", rf"f_1'={tex(f1n, 4)}\ \text{{Hz}}", f1n, "Hz")
    L.oficial("I parcial extraordinario II-2026 (desarrollo)", 4, 0.960, 43.82, 0.050, 4935, 27.39, tol=0.01)
    L.te_toca("Con la tensión original, ¿cuál es la frecuencia fundamental de esta cuerda?", vv / (2.0 * Lc), "Hz", tol=0.01,
              pista="f₁ = v/(2L).", solucion="43,82 / 1,92 ≈ 22,82 Hz.")
    return L
