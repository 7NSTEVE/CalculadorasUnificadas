"""III Parcial Ordinario real FI2103 II-2026 (9 ítems confirmados) + Cartago Extremo C26-1.

Todas las lecciones parten estrictamente de las 42 casillas de
`documentos-ii-2026_Ecuaciones_Tercer_Parcial_FG3.pdf` y cumplen el corte ≤ 20.5:
  - O26-1..O26-6: 6 ítems confirmados de selección única (págs. 1–2 del examen real II-2026;
    la pág. 3 con la pregunta 7 no se incluyó en las fotografías).
  - O26-8: Desarrollo 8 (16 pts) — Esfera seccionada con cortes planos en y = ±L (Fourier integral).
  - O26-9: Desarrollo 9 (20 pts) — Ciclo de 4 procesos (Q096 recalibrado a γ = 1,40 con n = 10,0 mol).
  - O26-10: Desarrollo 10 (10 pts) — Calorimetría agua + aluminio + hielo (fusión completa, 11,94 °C).
  - C26-1: Cartago Extremo (Nivel 5) — Esfuerzo térmico con holgura inicial + dilatación volumétrica.
"""

from __future__ import annotations

import math

from .ayudas import ATM, R_GAS, T0, Gas, revisar_opciones, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V, s
from .formato import dice, n, nu, tex
from .registro import leccion

C = r"^\circ\text{C}"
DEG_F = r"^\circ\text{F}"


def _fuente_o26(num: str, pag: int) -> dict:
    return {
        "doc": "26-II-O",
        "examen": "26-II-O",
        "nombre": "III Parcial ordinario II-2026",
        "num": num,
        "pag": pag,
        "texto": f"III Parcial ordinario II-2026 · pregunta {num}",
    }


def _fuente_c26(num: int) -> dict:
    return {
        "doc": "cartago-extremo-ii-2026",
        "examen": "26-II-CE",
        "nombre": "Escudo Cartago Extremo II-2026 (≤ 20.5)",
        "num": str(num),
        "texto": f"Escudo Cartago Extremo II-2026 · desafío {num}",
    }


# ============================================================================ O26-1
@leccion("O26-1")
def o26_1() -> Leccion:
    cp_f = 16.18
    cp_k = cp_f * 1.8
    L = Leccion(
        "O26-1",
        "Conversión del calor molar a presión constante del nitrógeno de °F a Kelvin",
        tema="4.3",
        tipo="seleccion",
        nivel=2,
        fuente=_fuente_o26("1", 1),
        enunciado=(
            "Una estudiante de termodinámica necesita la capacidad calorífica molar a presión constante ($C_p$) "
            "del nitrógeno ($\\text{N}_2$). En la biblioteca consulta un texto donde el valor reportado es "
            "$16{,}18\\ \\text{J}/(\\text{mol}\\cdot{}^\\circ\\text{F})$. Al realizar la conversión a unidades "
            "del Sistema Internacional, $\\text{J}/(\\text{mol}\\cdot\\text{K})$, el valor obtenido para $C_p$ es:"
        ),
        opciones={
            "A": "8,99 J/mol K",
            "B": "14,38 J/mol K",
            "C": "17,98 J/mol K",
            "D": "29,12 J/mol K",
        },
        correcta="D",
        claves=["escala Fahrenheit", "capacidad calorífica", "conversión de intervalos", "nitrógeno"],
    )
    L.paso("Qué nos dan y qué piden")\
        .di("Tenemos la capacidad calorífica molar del nitrógeno por grado Fahrenheit y la piden por kelvin.",
            DATO("16,18"),
            LISTA([
                f"$C_p = 16{{,}}18\\ \\text{{J}}/(\\text{{mol}}\\cdot {DEG_F})$",
                "Meta: convertir a $\\text{J}/(\\text{mol}\\cdot\\text{K})$",
                "Clave: el grado de temperatura está en el **denominador** y representa un **intervalo** $\\Delta T$",
            ]))\
        .di("Como el tamaño de un kelvin es mayor que el de un grado Fahrenheit, el resultado numérico debe subir.",
            OJO("No se suma ni se resta 32: en capacidades caloríficas se convierten intervalos $\\Delta T$, no lecturas."))
    L.paso("Relación entre intervalos desde el formulario")\
        .di("Partimos de la casilla de escala Fahrenheit de la hoja oficial.",
            F(r"T_{^\circ\text{F}} = \frac{9}{5}T_{^\circ\text{C}} + 32"))\
        .di("Al restar dos temperaturas el treinta y dos se cancela, y un cambio en Celsius iguala un cambio en kelvin.",
            M(r"\Delta T_{^\circ\text{F}} = \frac{9}{5}\,\Delta T_{^\circ\text{C}} = 1{,}8\,\Delta T_{\text{K}}"),
            M(r"1\ \text{K} = 1{,}8\ ^\circ\text{F}", "eq", "azul"))
    L.paso("Factor de conversión en el denominador")\
        .di("Multiplicamos para cancelar grado Fahrenheit abajo y dejar kelvin abajo.",
            M(rf"C_p = 16{{,}}18\,\frac{{\text{{J}}}}{{\text{{mol}}\cdot {DEG_F}}} \cdot \frac{{1{{,}}8\ {DEG_F}}}{{1\ \text{{K}}}}"))\
        .di("Calculamos el producto.",
            M(rf"C_p = (16{{,}}18)(1{{,}}8) = {tex(cp_k, 5)}"),
            R(r"C_p = 29{,}12\ \frac{\text{J}}{\text{mol}\cdot\text{K}}", "r1"))\
        .di("Comprobación física relámpago: el nitrógeno es diatómico, así que siete medios de erre da veintinueve coma diez.".replace("erre", s("R")),
            NOTA("Atajo físico: para $\\text{N}_2$ diatómico, $C_p=C_V+R=\\frac{7}{2}(8{,}314)=29{,}10\\ \\text{J}/(\\text{mol}\\cdot\\text{K})$, que confirma **D** al instante."))
    revisar_opciones(L, {
        "A": ("La A es la trampa clásica: divide entre uno coma ocho en lugar de multiplicar.", "$16{,}18 / 1{,}8 = 8{,}99$."),
        "B": ("La B multiplica por ocho novenos, que no corresponde al factor de escala.", "$16{,}18 \\times \\frac{8}{9} = 14{,}38$."),
        "C": ("La C multiplica por diez novenos en vez de nueve quintos.", "$16{,}18 \\times \\frac{10}{9} = 17{,}98$."),
        "D": ("La D es la correcta: veintinueve coma doce joules por mol kelvin.", "$16{,}18 \\times 1{,}8 = 29{,}124 \\approx 29{,}12$."),
    })
    L.responde("", r"C_p=29{,}12\ \text{J}/(\text{mol}\cdot\text{K})", cp_k, "J/(mol·K)")
    return L.oficial("III Parcial ordinario II-2026", "D").usa(r"T_{^\circ\text{F}}=\frac{9}{5}T_{^\circ\text{C}}+32")


# ============================================================================ O26-2
@leccion("O26-2")
def o26_2() -> Leccion:
    L = Leccion(
        "O26-2",
        "Relación entre trabajo y calor recibido con 20 % de eficiencia",
        tema="6.14",
        tipo="seleccion",
        nivel=1,
        fuente=_fuente_o26("2", 1),
        enunciado=(
            "La eficiencia $e$ de una cierta máquina térmica es del $20\\,\\%$. A partir de esta información "
            "es posible concluir que la relación entre el calor brindado $Q_H$ y el trabajo realizado por "
            "la máquina térmica $W$ cumple que:"
        ),
        opciones={
            "A": "$W = Q_H$",
            "B": "$W = 5Q_H$",
            "C": "$W = \\dfrac{Q_H}{5}$",
            "D": "$W = e - Q_H$",
        },
        correcta="C",
        claves=["eficiencia térmica", "despeje", "máquina térmica"],
    )
    L.paso("Partir de la casilla de eficiencia térmica")\
        .di("Escribimos veinte por ciento como fracción decimal y como fracción exacta.",
            DATO("20 %"),
            F(r"e = \frac{W}{Q_H} = 1 - \left|\frac{Q_C}{Q_H}\right|"),
            M(r"e = 20\,\% = 0{,}20 = \frac{20}{100} = \frac{1}{5}", "ef", "azul"))
    L.paso("Despejar el trabajo W")\
        .di("El calor caliente está dividiendo, así que pasa a multiplicar al otro lado.",
            M(r"\frac{W}{Q_H} = \frac{1}{5}\;\Rightarrow\;W = e\,Q_H = \frac{1}{5}\,Q_H"),
            R(r"W = \frac{Q_H}{5}", "r2"))\
        .di("Sentido físico: una máquina real siempre entrega menos trabajo que el calor que absorbe.",
            OJO("Si $e=20\\,\\%$, solo un quinto del calor recibido se convierte en trabajo útil."))
    revisar_opciones(L, {
        "A": ("La A significaría cien por ciento de eficiencia, prohibido por la segunda ley.", "$e = 1 = 100\\,\\%$."),
        "B": ("La B invierte el despeje: produciría cinco veces más trabajo que calor recibido.", "Viola conservación de energía."),
        "D": ("La D resta una cantidad sin unidades menos energía en joules: es dimensionalmente imposible.", "No se puede restar $e$ y $Q_H$."),
        "C": ("La C es la correcta: el trabajo es la quinta parte del calor brindado.", "$W = 0{,}20\\,Q_H = Q_H/5$."),
    })
    return L.oficial("III Parcial ordinario II-2026", "C").usa(r"e=\frac{W}{Q_H}")


# ============================================================================ O26-3
@leccion("O26-3")
def o26_3() -> Leccion:
    L = Leccion(
        "O26-3",
        "Dos barras cilíndricas con el doble de coeficiente de dilatación",
        tema="4.4",
        tipo="seleccion",
        nivel=1,
        fuente=_fuente_o26("3", 1),
        enunciado=(
            "Dos barras cilíndricas, fabricadas de materiales diferentes (1 y 2), tienen inicialmente exactamente "
            "la misma longitud $L_0$. El coeficiente de dilatación lineal del material 2 es el doble que el del "
            "material 1. Al someter ambas barras a un mismo incremento de temperatura $\\Delta T$, ¿cómo se "
            "relaciona la variación de longitud de la barra 2 ($\\Delta L_2$) con la variación de longitud de "
            "la barra 1 ($\\Delta L_1$)?"
        ),
        opciones={
            "A": "$\\Delta L_2 = 2\\Delta L_1$",
            "B": "$\\Delta L_2 = \\Delta L_1$",
            "C": "$\\Delta L_2 = \\frac{1}{2}\\Delta L_1$",
            "D": "$\\Delta L_2 = 4\\Delta L_1$",
        },
        correcta="A",
        claves=["dilatación lineal", "proporcionalidad"],
        visual={
            "tipo": "dilatacion",
            "caso": "barras",
            "u": "m",
            "T0": 20,
            "dTmax": 100,
            "barras": [
                {"n": "barra 1 (α₁)", "L": 1.0, "a": 1.2e-5, "color": "azul"},
                {"n": "barra 2 (2α₁)", "L": 1.0, "a": 2.4e-5, "color": "naranja"},
            ],
        },
    )
    L.paso("Comparar ambas barras desde la dilatación lineal")\
        .di("Las dos barras parten con el mismo largo inicial y sufren el mismo cambio de temperatura.",
            V(foco="calentar"), DATO("misma longitud"),
            F(r"\Delta L = \alpha L_0\,\Delta T"),
            LISTA(["Barra 1: $\\Delta L_1 = \\alpha_1 L_0\\,\\Delta T$", "Barra 2: $\\alpha_2 = 2\\alpha_1$, mismo $L_0$ y mismo $\\Delta T$"]))\
        .di("Sustituimos alfa dos igual a dos alfa uno y reconocemos delta ele uno.",
            M(r"\Delta L_2 = \alpha_2 L_0\,\Delta T = (2\alpha_1)L_0\,\Delta T = 2(\alpha_1 L_0\,\Delta T)"),
            R(r"\Delta L_2 = 2\,\Delta L_1", "r3"))
    revisar_opciones(L, {
        "B": ("La B solo ocurriría si tuvieran el mismo coeficiente de dilatación.", "Aquí $\\alpha_2 = 2\\alpha_1$."),
        "C": ("La C está invertida: la barra dos tiene mayor coeficiente, así que se estira más.", "No se reduce a la mitad."),
        "D": ("La D eleva el factor dos al cuadrado, pero la dilatación lineal es de primer grado en alfa.", "$\\Delta L \\propto \\alpha$, no $\\alpha^2$."),
        "A": ("La A es la correcta: al duplicar alfa se duplica el alargamiento.", "$\\Delta L_2 = 2\\Delta L_1$."),
    })
    return L.oficial("III Parcial ordinario II-2026", "A").usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ O26-4
@leccion("O26-4")
def o26_4() -> Leccion:
    ti_c = 25.0
    ti_k = ti_c + T0
    tf_k = 2.0 * ti_k
    tf_c = tf_k - T0
    L = Leccion(
        "O26-4",
        "Duplicar la presión de un gas ideal a volumen constante desde 25 °C",
        tema="6.1",
        tipo="seleccion",
        nivel=1,
        fuente=_fuente_o26("4", 2),
        enunciado=(
            "Un mol de un gas ideal tiene una temperatura de $25\\ ^\\circ\\text{C}$. Si el volumen se mantiene "
            "constante y la presión se duplica, la temperatura final (en $^\\circ\\text{C}$) será alrededor de:"
        ),
        opciones={"A": "174", "B": "596", "C": "50", "D": "323"},
        correcta="D",
        claves=["gas ideal", "kelvin", "proceso isocórico"],
        visual={"tipo": "gas", "modo": "temperatura"},
    )
    L.paso("Pasar primero a temperatura absoluta (Kelvin)")\
        .di("En la ley del gas ideal la temperatura siempre debe ir en kelvin, nunca en grados Celsius.",
            DATO("25 °C"),
            F(r"T_{\text{K}} = T_{^\circ\text{C}} + 273{,}15"),
            M(rf"T_1 = 25 + 273{{,}}15 = {tex(ti_k, 5)}\ \text{{K}}", "t1", "azul"))\
        .di("Cuidado con multiplicar veinticinco por dos directamente: esa es la trampa principal del ítem.",
            OJO("Nunca dupliques grados Celsius en $pV=nRT$: el cero absoluto es $-273{,}15\\ ^\\circ\\text{C}$."))
    L.paso("Duplicar la presión a volumen y moles constantes")\
        .di("De la ecuación del gas ideal despejamos la proporción entre presión y temperatura.",
            F(r"pV = nRT"),
            M(r"\frac{p_2}{T_2} = \frac{p_1}{T_1}\;\Rightarrow\;T_2 = T_1\,\frac{p_2}{p_1} = 2T_1"))\
        .di("Sustituimos te uno en kelvin.".replace("te uno", dice("T₁", "te uno")),
            M(rf"T_2 = 2({tex(ti_k, 5)}) = {tex(tf_k, 5)}\ \text{{K}}", "t2", "verde"))
    L.paso("Volver a grados Celsius")\
        .di("Restamos doscientos setenta y tres coma quince para pasar el resultado final a grados Celsius.",
            M(rf"T_{{2,^\circ\text{{C}}}} = {tex(tf_k, 5)} - 273{{,}}15 = {tex(tf_c, 5)}\ {C}"),
            R(rf"T_2 \approx 323\ {C}", "r4"))
    revisar_opciones(L, {
        "A": ("La A no coincide con la relación en kelvin.", "Valor distractor."),
        "B": ("La B es la temperatura final en kelvin, sin devolverla a grados Celsius.", "$596{,}3\\ \\text{K}$, falta restar $273{,}15$."),
        "C": ("La C es la trampa de duplicar directamente los veinticinco grados Celsius.", "$2 \\times 25 = 50$ ignora el cero absoluto."),
        "D": ("La D es la correcta: trescientos veintitrés grados Celsius.", "$596{,}30 - 273{,}15 = 323{,}15\\ ^\\circ\\text{C}$."),
    })
    L.responde("", rf"T_2 = {tex(tf_c, 5)}\ {C}", tf_c, "°C")
    return L.oficial("III Parcial ordinario II-2026", "D").usa(r"pV=nRT", r"T_K=T_{^\circ\text{C}}+273{,}15")


# ============================================================================ O26-5
@leccion("O26-5")
def o26_5() -> Leccion:
    L = Leccion(
        "O26-5",
        "Pared compuesta de varias capas en régimen estacionario",
        tema="5.4",
        tipo="seleccion",
        nivel=2,
        fuente=_fuente_o26("5", 2),
        enunciado=(
            "Una pared compuesta por varias capas de diferentes materiales separa dos espacios a distintas "
            "temperaturas, una mayor que la otra. Después de un tiempo, el sistema alcanza un régimen estacionario. "
            "Considere que es un sistema ideal. ¿Cuál de las siguientes afirmaciones es correcta con respecto a la pared?"
        ),
        opciones={
            "A": "La temperatura necesariamente disminuye de forma lineal a través de todas las capas.",
            "B": "La capa de mayor conductividad térmica siempre presenta la mayor caída de temperatura.",
            "C": "La energía interna de cada capa debe aumentar continuamente debido al flujo de calor.",
            "D": "La rapidez de transferencia de calor es la misma a través de todas las capas, aunque los gradientes de temperatura puedan ser diferentes.",
        },
        correcta="D",
        claves=["pared compuesta", "régimen estacionario", "gradiente térmico"],
        visual={
            "tipo": "conduccion",
            "geo": "serie",
            "uT": "°C",
            "T": [100, 90, 30, 20],
            "capas": [
                {"n": "Capa 1", "e": 0.05, "k": 1.0},
                {"n": "Capa 2", "e": 0.05, "k": 0.17},
                {"n": "Capa 3", "e": 0.05, "k": 1.0},
            ],
        },
    )
    L.paso("Qué significa régimen estacionario en capas en serie")\
        .di("En régimen estacionario ninguna capa acumula ni pierde energía neta: todo el calor que entra por una cara sale por la siguiente.",
            DATO("régimen estacionario"),
            F(r"H = -kA\frac{dT}{dx}"),
            M(r"H_1 = H_2 = H_3 = H = \text{constante}", "hc", "azul"))\
        .di("Despejamos la magnitud del gradiente térmico en cada capa.",
            M(r"\left|\frac{dT}{dx}\right| = \frac{H}{kA}"))\
        .di("Como cada material tiene distinta conductividad ka, cada capa tiene su propia pendiente de temperatura.".replace("ka", s("k")),
            R(r"H_1 = H_2 = H_3,\qquad \left|\frac{dT}{dx}\right|_i \propto \frac{1}{k_i}", "r5"))
    revisar_opciones(L, {
        "A": ("La A es falsa: la temperatura es lineal dentro de cada capa, pero cambia de pendiente en cada frontera, formando una quebrada.", "No es una sola recta continua."),
        "B": ("La B está al revés: mayor conductividad significa menor resistencia y menor caída de temperatura.", "$\\Delta T = H L / (kA)$ es inverso a $k$."),
        "C": ("La C contradice el régimen estacionario: si la temperatura no cambia con el tiempo, la energía interna de cada capa permanece constante.", "En estado estacionario $\\partial T/\\partial t = 0$."),
        "D": ("La D es la correcta: mismo flujo térmico en serie, con gradientes distintos según la conductividad de cada material.", "Conservación de $H$ en serie."),
    })
    return L.oficial("III Parcial ordinario II-2026", "D").usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}=kA\frac{T_H-T_C}{L}")


# ============================================================================ O26-6
@leccion("O26-6")
def o26_6() -> Leccion:
    L = Leccion(
        "O26-6",
        "Por qué aumenta la capacidad calorífica molar de un gas diatómico al calentarlo",
        tema="6.6",
        tipo="seleccion",
        nivel=2,
        fuente=_fuente_o26("6", 2),
        enunciado=(
            "Un gas diatómico se calienta a volumen constante desde una temperatura muy baja hasta una temperatura "
            "mucho mayor. Experimentalmente se observa que su capacidad calorífica molar aumenta. ¿Cuál es la "
            "explicación física que podría explicar este comportamiento?"
        ),
        opciones={
            "A": "A mayor temperatura aparecen nuevas moléculas y, por ello, aumenta el número de grados de libertad.",
            "B": "La masa de cada molécula aumenta con la temperatura, lo que eleva la energía necesaria para acelerar el gas.",
            "C": "La energía de traslación deja de depender de la temperatura y es sustituida completamente por energía rotacional y vibracional.",
            "D": "Los grados de libertad rotacionales y vibracionales comienzan a almacenar energía cuando las moléculas pueden acceder a los niveles energéticos correspondientes.",
        },
        correcta="D",
        claves=["grados de libertad", "gas diatómico", "C_V", "equipartición"],
    )
    L.paso("Escalones cuánticos del calor molar a volumen constante")\
        .di("Las dos casillas de la hoja muestran que añadir movimientos internos sube la capacidad calorífica molar.",
            DATO("gas diatómico"),
            F(r"C_{V\text{mono}} = \frac{3}{2}R"), F(r"C_{V\text{dia}} = \frac{5}{2}R"),
            TABLA(
                ["Régimen de temperatura", "Modos activos", "$C_V$"],
                [
                    ["Muy baja", "Solo traslación ($f=3$)", "$\\tfrac{3}{2}R$"],
                    ["Intermedia (ambiente)", "Traslación + rotación ($f=5$)", "$\\tfrac{5}{2}R$"],
                    ["Muy alta", "Traslación + rotación + vibración ($f=7$)", "$\\tfrac{7}{2}R$"],
                ],
                resaltar=[[1, 2], [2, 2]],
            ))\
        .di("A temperaturas bajas la rotación y vibración están congeladas cuánticamente; al calentar se desbloquean y absorben energía adicional.",
            M(r"C_V = \frac{f}{2}R \quad (f:\ 3 \to 5 \to 7)"),
            R(r"\frac{3}{2}R \;\longrightarrow\; \frac{5}{2}R \;\longrightarrow\; \frac{7}{2}R", "r6"))\
        .di("Nota histórica: esta pregunta adapta directamente la pregunta tres del ordinario uno de dos mil veinticinco.",
            NOTA("Gemelo histórico verificado: **Q082** (III Parcial ordinario I-2025, pregunta 3)."))
    revisar_opciones(L, {
        "A": ("La A es falsa: calentar un gas ideal encerrado no crea moléculas nuevas.", "El número de moles $n$ es constante."),
        "B": ("La B es falsa: la masa de cada molécula no cambia al calentar el gas.", "Masa molecular constante."),
        "C": ("La C es falsa: la energía traslacional sigue valiendo tres medios de ka be te; los otros modos se suman, no la sustituyen.", "Se suman modos, no se reemplaza la traslación."),
        "D": ("La D es la correcta: al subir la temperatura las moléculas acceden a niveles rotacionales y vibracionales.", "Activación progresiva de grados de libertad."),
    })
    return L.oficial("III Parcial ordinario II-2026", "D").usa(r"C_{V\text{mono}}=\frac{3}{2}R", r"C_{V\text{dia}}=\frac{5}{2}R")


# ============================================================================ O26-8
@leccion("O26-8")
def o26_8() -> Leccion:
    # Parámetros de demostración para la vista interactiva (0 < L < R)
    r_demo = 0.10
    l_demo = 0.08
    t1_demo = 100.0
    t2_demo = 20.0
    k_demo = 200.0
    ln_tot = math.log((r_demo + l_demo) / (r_demo - l_demo))
    pts = []
    for i in range(61):
        yi = -l_demo + (2.0 * l_demo * i) / 60.0
        # T(y) = T1 - (T1 - T2) * [ln((R+y)/(R-y)) - ln((R-L)/(R+L))] / [2 ln((R+L)/(R-L))]
        frac = (math.log((r_demo + yi) / (r_demo - yi)) + ln_tot) / (2.0 * ln_tot)
        pts.append([round(yi, 5), round(t1_demo - (t1_demo - t2_demo) * frac, 2)])

    L = Leccion(
        "O26-8",
        "Conducción en una esfera seccionada con dos cortes planos simétricos",
        tema="5.4",
        tipo="desarrollo",
        nivel=4,
        fuente=_fuente_o26("8", 4),
        enunciado=(
            "Una esfera, de radio $R$, hecha de un material con conductividad térmica $k$, tiene dos cortes planos "
            "simétricos a una distancia $L$ de su centro ($0<L<R$). La esfera seccionada se coloca de modo tal que "
            "sus caras planas están en contacto con dos depósitos térmicos de temperaturas constantes $T_1$ (en $y=-L$) "
            "y $T_2$ (en $y=+L$), tales que $T_1 > T_2$. La superficie curva de la esfera está cubierta de un material "
            "aislante. Para el estado estacionario, calcule el flujo de calor resultante $H$ en términos de $k$, $L$, "
            "$T_1$, $T_2$ y $R$. "
            "*(Integrales provistas: $\\int \\frac{du}{a^2-u^2}=\\frac{1}{2a}\\ln\\left|\\frac{a+u}{a-u}\\right|+C$, "
            "$\\int \\frac{du}{\\sqrt{a^2-u^2}}=\\text{arcsen}(u/a)+C$, $\\int \\frac{du}{(a^2-u^2)^{3/2}}=\\frac{u}{a^2\\sqrt{a^2-u^2}}+C$.)*"
        ),
        claves=["esfera seccionada", "área variable", "Fourier diferencial", "integral logarítmica", "simbólico"],
        visual={
            "tipo": "conduccion",
            "geo": "esfera_seccionada",
            "R": r_demo,
            "L": l_demo,
            "T1": t1_demo,
            "T2": t2_demo,
            "k": k_demo,
            "eje": "y",
            "uT": "°C",
            "pts": pts,
            "xm": 0.036,
            "etA": "A(y) = π(R² − y²)",
        },
    )
    L.paso("Qué nos dan y qué piden")\
        .di("El calor sube verticalmente desde la cara plana inferior a temperatura te uno hasta la superior a temperatura te dos.".replace("te uno", dice("T₁", "te uno")).replace("te dos", dice("T₂", "te dos")),
            DATO("cortes planos"),
            LISTA([
                "Esfera de radio $R$ y conductividad térmica constante $k$",
                "Cara inferior caliente en $y=-L$ a temperatura $T_1$; cara superior fría en $y=+L$ a $T_2$ ($T_1>T_2$)",
                "Superficie curva lateral aislada: en estado estacionario el mismo flujo $H$ cruza toda rebanada horizontal",
                "Meta: expresar $H$ en términos de $k, L, T_1, T_2, R$ (con $0<L<R$)",
            ]))\
        .di("No es una esfera hueca con flujo radial cuatro pi erre cuadrado: aquí las caras son planas y el flujo sigue el eje vertical.",
            OJO("No uses $A=4\\pi r^2$: el calor cruza discos horizontales perpendiculares al eje $y$, no cascarones esféricos."))
    L.paso("Paso 1 · Área de una rebanada horizontal con Pitágoras")\
        .di("A una altura ye, la sección transversal es un círculo horizontal de radio equis.".replace("ye", dice("y", "ye")).replace("equis", dice("x", "equis")),
            V(foco="dx"),
            M(r"x^2 + y^2 = R^2 \;\Rightarrow\; x^2 = R^2 - y^2"))\
        .di("El área del disco circular es pi por su radio al cuadrado.",
            M(r"A(y) = \pi x^2"),
            M(r"A(y) = \pi\left(R^2 - y^2\right)", "ay", "azul"))\
        .por_que("En el ecuador ($y=0$) el área es máxima ($\\pi R^2$); en las caras planas ($y=\\pm L$) es mínima ($\\pi(R^2-L^2)$).")
    L.paso("Paso 2 · Ley de Fourier diferencial desde el formulario")\
        .di("Como el área cambia con la altura, partimos de la casilla de Fourier diferencial adaptada al eje vertical.",
            F(r"H = -kA\frac{dT}{dx}"),
            M(r"H = -k\,A(y)\,\frac{dT}{dy}"))\
        .di("Sustituimos la expresión del área en función de ye.".replace("ye", dice("y", "ye")),
            M(r"H = -k\pi\left(R^2 - y^2\right)\frac{dT}{dy}"))
    L.paso("Paso 3 · Separar variables y elegir la integral correcta")\
        .di("Dejamos todo lo de la posición a la izquierda y el diferencial de temperatura a la derecha.",
            M(r"\frac{H}{\pi k}\,\frac{dy}{R^2 - y^2} = -dT"))\
        .di("Integramos desde la cara inferior caliente hasta la cara superior fría.",
            M(r"\frac{H}{\pi k}\int_{-L}^{L}\frac{dy}{R^2 - y^2} = -\int_{T_1}^{T_2}dT", "int", "azul"))\
        .di("De las tres integrales dadas en el examen, solo la primera coincide exactamente con uno entre a cuadrado menos u cuadrado.",
            NOTA("Las integrales con $\\sqrt{a^2-u^2}$ y $(a^2-u^2)^{3/2}$ son señuelos para quien olvida elevar al cuadrado el radio $x=\\sqrt{R^2-y^2}$."))
    L.paso("Paso 4 · Evaluar ambas integrales definidas")\
        .di("Lado derecho: el menos invierte el orden de las temperaturas.",
            M(r"-\int_{T_1}^{T_2}dT = -(T_2 - T_1) = T_1 - T_2"))\
        .di("Lado izquierdo: aplicamos la integral dada con a igual a erre y u igual a ye.".replace("erre", s("R")).replace("ye", dice("y", "ye")),
            M(r"\int_{-L}^{L}\frac{dy}{R^2 - y^2} = \left[\frac{1}{2R}\ln\left|\frac{R+y}{R-y}\right|\right]_{-L}^{L}"))\
        .di("Como cero es menor que ele y menor que erre, ambos cocientes son positivos y podemos quitar el valor absoluto.".replace("ele", s("L")).replace("erre", s("R")),
            M(r"= \frac{1}{2R}\left[\ln\left(\frac{R+L}{R-L}\right) - \ln\left(\frac{R-L}{R+L}\right)\right]"))\
        .di("Invertir la fracción del segundo logaritmo cambia su signo, así que los dos términos se suman.",
            M(r"\ln\left(\frac{R-L}{R+L}\right) = -\ln\left(\frac{R+L}{R-L}\right)"),
            M(r"\int_{-L}^{L}\frac{dy}{R^2 - y^2} = \frac{1}{2R}\cdot 2\ln\left(\frac{R+L}{R-L}\right) = \frac{1}{R}\ln\left(\frac{R+L}{R-L}\right)", "iv", "verde"))
    L.paso("Paso 5 · Despejar H y comprobar el límite físico")\
        .di("Igualamos ambos lados y despejamos el flujo de calor.",
            V(foco="listo"),
            M(r"\frac{H}{\pi k R}\ln\left(\frac{R+L}{R-L}\right) = T_1 - T_2"),
            R(r"H = \frac{\pi k R\,(T_1 - T_2)}{\ln\left(\frac{R+L}{R-L}\right)}", "rh"))\
        .di("Prueba de sanidad: si el corte es muy delgado respecto al radio, el logaritmo vale dos ele entre erre y recuperamos la placa plana de grosor dos ele.".replace("ele", s("L")).replace("erre", s("R")),
            M(r"L \ll R \;\Rightarrow\; \ln\left(\frac{R+L}{R-L}\right) \approx \frac{2L}{R} \;\Rightarrow\; H \approx k(\pi R^2)\frac{T_1 - T_2}{2L}"))
    L.responde("H", r"H = \frac{\pi k R(T_1 - T_2)}{\ln\left(\frac{R+L}{R-L}\right)}")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ O26-9
@leccion("O26-9")
def o26_9() -> Leccion:
    n_mol = 10.0
    pa_atm, pb_atm = 1.0, 3.0
    va, vb, vc, vd = 0.20, 0.09, 0.40, 1.20
    pa, pb = pa_atm * ATM, pb_atm * ATM
    qbc, qda = 328500.0, -354600.0

    w_da = pa * (va - vd)           # -101325.0 J
    du_da = qda - w_da              # -253275.0 J
    du_ac = -du_da                  # +253275.0 J
    gamma_da = qda / du_da          # 1.4000592...

    ta = pa * va / (n_mol * R_GAS)  # 243.7455 K
    td = pa * vd / (n_mol * R_GAS)  # 1462.4729 K
    cp_da = qda / (n_mol * (ta - td))
    cv_da = cp_da - R_GAS

    g = Gas(n_mol, "dia")
    g.pone("A", p=pa, v=va)
    g.pone("B", p=pb, v=vb)
    g.pone("C", p=pb, v=vc)
    g.pone("D", p=pa, v=vd)

    L = Leccion(
        "O26-9",
        "Ciclo de cuatro procesos con calores dados: cambio de energía interna y γ (II-2026)",
        tema="6.11",
        tipo="desarrollo",
        nivel=3,
        fuente=_fuente_o26("9", 5),
        enunciado=(
            "$10{,}0$ moles de un gas ideal pasan por el proceso cíclico $A(0{,}20\\ \\text{m}^3;\\ 1{,}0\\ \\text{atm})\\to "
            "B(0{,}09\\ \\text{m}^3;\\ 3{,}0\\ \\text{atm})\\to C(0{,}40\\ \\text{m}^3;\\ 3{,}0\\ \\text{atm})\\to "
            "D(1{,}20\\ \\text{m}^3;\\ 1{,}0\\ \\text{atm})\\to A$. Entre $A$ y $B$ el proceso es adiabático; entre "
            "$B$ y $C$ es isobárico con $328{,}5$ kJ entrando como calor; entre $C$ y $D$ es isotérmico; y entre $D$ y $A$ "
            "es isobárico con $354{,}6$ kJ saliendo como calor. "
            "(a) Calcule el cambio de energía interna entre los estados $A$ y $C$. "
            "(b) Calcule la razón $\\gamma$ entre las capacidades caloríficas del gas."
        ),
        aviso=(
            "Versión recalibrada de Q096 (24-I-E P8): el autor calculó 328,5 kJ usando el volumen adiabático exacto "
            "en B (0,09125 m³) para γ = 1,40, pero mantuvo la etiqueta redondeada 0,09 en la gráfica. Por eso la ruta "
            "D→A da γ = 1,400 exacto, mientras la relación adiabática con 0,09 da 1,376."
        ),
        claves=["ciclo termodinámico", "función de estado", "primera ley", "razón gamma", "reciclaje Q096"],
        visual=visual_pv(
            g,
            [("A", "B", "adiabatico"), ("B", "C", "isobarico"), ("C", "D", "isotermico"), ("D", "A", "isobarico")],
            uv="m³",
            up="atm",
            fp=1.0 / ATM,
        ),
    )
    L.paso("Qué nos dan y el atajo de función de estado")\
        .di("No hace falta calcular el trabajo en la curva adiabática ni en la isoterma: la energía interna es función de estado.",
            DATO("354,6 kJ"),
            LISTA([
                "$n = 10{,}0$ mol; $1{,}0\\ \\text{atm} = 101\\,325$ Pa; $3{,}0\\ \\text{atm} = 303\\,975$ Pa",
                "$A(0{,}20\\ \\text{m}^3;\\ 1{,}0\\ \\text{atm})$, $B(0{,}09;\\ 3{,}0)$, $C(0{,}40;\\ 3{,}0)$, $D(1{,}20;\\ 1{,}0)$",
                "$A\\to B$ adiabático ($Q=0$); $B\\to C$ isobárico ($Q_{BC}=+328{,}5$ kJ); $C\\to D$ isotérmico; $D\\to A$ isobárico ($Q_{DA}=-354{,}6$ kJ)",
            ]))\
        .di("Atención: nunca aproximes el trabajo de una curva adiabática con la presión promedio por delta volumen.",
            OJO("El área de un trapecio $P_{\\text{media}}(V_f-V_i)$ solo sirve si el tramo es una **recta**, jamás en una curva adiabática."))
    L.paso("a) Cerrar el ciclo con energía interna")\
        .di("Como el gas regresa a su estado inicial A, la suma de cambios de energía interna en el ciclo vale cero.",
            V(foco="ciclo"),
            M(r"\Delta U_{AC} + \Delta U_{CD} + \Delta U_{DA} = 0"))\
        .di("El tramo C a D es isotérmico, así que su cambio de temperatura es cero y su cambio de energía interna también es cero.",
            V(foco="CD"), F(r"\Delta U = nC_V\Delta T"),
            M(r"\Delta U_{CD} = nC_V(0) = 0"))\
        .di("Despejamos delta U entre A y C: es simplemente el negativo de delta U entre D y A.",
            M(r"\Delta U_{AC} = -\Delta U_{DA}", "atajo", "azul"))
    L.paso("a) Trabajo y primera ley en el tramo isobárico D→A")\
        .di("A presión constante usamos la casilla de trabajo isobárico del formulario, pasando atmósferas a pascales.",
            V(foco="DA"), F(r"W = p(V_2 - V_1)"),
            M(r"W_{DA} = p_A(V_A - V_D) = (101\,325)(0{,}20 - 1{,}20)"),
            M(rf"W_{{DA}} = {tex(w_da, 6, u='J', sci='no')}", "wda"))\
        .di("Aplicamos la primera ley con el calor saliente negativo.",
            F(r"\Delta U = Q - W"),
            M(rf"\Delta U_{{DA}} = Q_{{DA}} - W_{{DA}} = -354\,600 - ({tex(w_da, 6, sci='no')})"),
            M(rf"\Delta U_{{DA}} = {tex(du_da, 6, u='J', sci='no')}", "duda"))\
        .di("Cambiamos el signo y obtenemos el cambio de energía interna entre A y C.",
            R(rf"\Delta U_{{AC}} = -\Delta U_{{DA}} = {tex(du_ac, 6, u='J', sci='no')} \approx {tex(du_ac / 1e3, 4, u='kJ')}", "ra"))
    L.paso("b) Deducción de γ desde el formulario")\
        .di("En el proceso isobárico D a A integramos la casilla de calor isobárico con capacidad constante.",
            F(r"dQ = nC_p\,dT"),
            M(r"Q_{DA} = \int_{T_D}^{T_A} nC_p\,dT = nC_p\,\Delta T_{DA}"))\
        .di("Dividimos ese calor entre la casilla de energía interna para cancelar moles y delta temperatura.",
            F(r"\Delta U = nC_V\Delta T"), F(r"\gamma = \frac{C_P}{C_V}"),
            M(r"\frac{Q_{DA}}{\Delta U_{DA}} = \frac{nC_P\,\Delta T_{DA}}{nC_V\,\Delta T_{DA}} = \frac{C_P}{C_V} = \gamma", "gam", "azul"))
    L.paso("b) Sustitución y verificación con los 10,0 moles")\
        .di("Sustituimos los dos valores del tramo D a A.",
            M(rf"\gamma = \frac{{-354\,600}}{{-253\,275}} = {tex(gamma_da, 5)}"),
            R(r"\gamma = 1{,}40", "rb"))\
        .di("Comprobación usando los diez moles y la relación de Mayer: las temperaturas de A y D llevan al mismo ce pe de gas diatómico.".replace("ce pe", dice("C_P", "ce pe")),
            F(r"pV = nRT"), F(r"C_P = C_V + R"),
            M(rf"T_A = \frac{{(101\,325)(0{{,}}20)}}{{(10{{,}}0)(8{{,}}314)}} = {tex(ta, 5)}\ \text{{K}}\qquad T_D = \frac{{(101\,325)(1{{,}}20)}}{{(10{{,}}0)(8{{,}}314)}} = {tex(td, 6)}\ \text{{K}}"),
            M(rf"C_P = \frac{{-354\,600}}{{(10{{,}}0)({tex(ta, 5)}-{tex(td, 6)})}} = {tex(cp_da, 4)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}\;\Rightarrow\;C_V = C_P - R = {tex(cv_da, 4)}\;\Rightarrow\;\gamma = 1{{,}}40"))
    L.responde("a", rf"\Delta U_{{AC}}={tex(du_ac / 1e3, 4)}\ \text{{kJ}}", du_ac / 1e3, "kJ")\
     .responde("b", r"\gamma=1{,}40", gamma_da, "")
    return L.oficial("III Parcial ordinario II-2026", 253.3, 1.40, tol=0.01).usa(
        r"W=p(V_2-V_1)", r"\Delta U=Q-W", r"\Delta U=nC_V\Delta T",
        r"dQ=nC_p\,dT", r"\gamma=\frac{C_P}{C_V}", r"pV=nRT", r"C_P=C_V+R",
    )


# ============================================================================ O26-10
@leccion("O26-10")
def o26_10() -> Leccion:
    mw, cw, tw = 0.500, 4186.0, 38.3
    mal, cal = 0.300, 900.0
    mh, ch, th = 0.150, 2090.0, -15.0
    lf = 3.34e5
    q_w0 = mw * cw * tw         # 80161.9 J
    q_al0 = mal * cal * tw      # 10341.0 J
    q_disp = q_w0 + q_al0       # 90502.9 J
    q1 = mh * ch * (0.0 - th)   # 4702.5 J
    q2 = mh * lf                # 50100.0 J
    q_nec = q1 + q2             # 54802.5 J
    q_rest = q_disp - q_nec     # 35700.4 J
    c_tot = mw * cw + mal * cal + mh * cw  # 2990.9 J/C
    tf = q_rest / c_tot         # 11.93634 C

    L = Leccion(
        "O26-10",
        "Equilibrio térmico de agua y aluminio a 38,3 °C con hielo a −15,0 °C",
        tema="4.8",
        tipo="desarrollo",
        nivel=3,
        fuente=_fuente_o26("10", 5),
        enunciado=(
            "Un recipiente térmicamente aislado contiene $0{,}500$ kg de agua y un bloque de aluminio de $0{,}300$ kg "
            "en equilibrio térmico a $38{,}3\\ ^\\circ$C. Posteriormente se introduce una muestra de $0{,}150$ kg de hielo "
            "a $-15{,}0\\ ^\\circ$C y el sistema alcanza el equilibrio térmico (capacidad del recipiente despreciable). "
            "Determine la temperatura final del sistema. "
            "*(Datos: $c_{\\text{agua}}=4186$, $c_{\\text{Al}}=900$, $c_{\\text{hielo}}=2090\\ \\text{J}/(\\text{kg}\\cdot{}^\\circ\\text{C})$, "
            "$L_f=3{,}34\\times 10^5$ J/kg.)*"
        ),
        claves=["calorimetría", "fusión completa", "balance por etapas", "aluminio"],
        visual={
            "tipo": "calor",
            "componentes": [
                {"n": "agua", "m": 500, "T": 38.3, "fase": "liquido"},
                {"n": "aluminio", "m": 300, "T": 38.3, "fase": "solido"},
                {"n": "hielo", "m": 150, "T": -15.0, "fase": "hielo"},
            ],
            "necesita": [{"n": "calentar hielo", "Q": q1}, {"n": "fundir hielo", "Q": q2}],
            "disponible": [{"n": "agua a 0 °C", "Q": q_w0}, {"n": "aluminio a 0 °C", "Q": q_al0}],
        },
    )
    L.paso("Qué nos dan y regla de etapas")\
        .di("Antes de despejar la temperatura final siempre verificamos si el hielo alcanza a derretirse por completo.",
            DATO("0,150 kg"),
            LISTA([
                "Calientes ($38{,}3\\ ^\\circ\\text{C}$): $0{,}500$ kg de agua y $0{,}300$ kg de aluminio",
                "Frío ($-15{,}0\\ ^\\circ\\text{C}$): $0{,}150$ kg de hielo",
                "Recipiente aislado de capacidad despreciable: $\\sum Q = 0$",
            ]))
    L.paso("Paso 1 · Calor máximo disponible hasta 0 °C")\
        .di("Calculamos cuánta energía cederían el agua y el aluminio si bajaran hasta cero grados.",
            V(foco="disponible"), F(r"Q = mc\Delta T"),
            M(rf"Q_{{\text{{agua}}\to 0}} = (0{{,}}500)(4186)(38{{,}}3) = {tex(q_w0, 6, u='J', sci='no')}"),
            M(rf"Q_{{\text{{Al}}\to 0}} = (0{{,}}300)(900)(38{{,}}3) = {tex(q_al0, 6, u='J', sci='no')}"),
            M(rf"Q_{{\text{{disp}}}} = {tex(q_w0, 6, sci='no')} + {tex(q_al0, 6, sci='no')} = {tex(q_disp, 6, u='J', sci='no')}", "qd", "azul"))
    L.paso("Paso 2 · Calor necesario para calentar y fundir todo el hielo")\
        .di("Primero subimos el hielo desde menos quince hasta cero grados con su calor específico de sólido.",
            V(foco="necesita"),
            M(rf"Q_1 = m_h c_h \Delta T = (0{{,}}150)(2090)(0 - (-15{{,}}0)) = {tex(q1, 5, u='J', sci='no')}"))\
        .di("Luego lo derretimos por completo con la casilla de calor latente.",
            F(r"Q = \pm mL"),
            M(rf"Q_2 = m_h L_f = (0{{,}}150)(3{{,}}34\times 10^5) = {tex(q2, 5, u='J', sci='no')}"))\
        .di("Sumamos ambos y comparamos con lo disponible: sobra energía, así que se funde todo el hielo y la temperatura final queda sobre cero.",
            V(foco="comparar"),
            M(rf"Q_{{\text{{nec}}}} = {tex(q1, 5, sci='no')} + {tex(q2, 5, sci='no')} = {tex(q_nec, 6, u='J', sci='no')} < {tex(q_disp, 6, sci='no')}\ \text{{J}}", "cmp", "verde"))
    L.paso("Paso 3 · Balance térmico final y despeje de la temperatura final")\
        .di("Planteamos suma de calores igual a cero incluyendo el agua derretida que ahora se calienta desde cero hasta la temperatura final.",
            M(r"\sum Q = 0"),
            M(r"(0{,}500)(4186)(T_f - 38{,}3) + (0{,}300)(900)(T_f - 38{,}3) + 4702{,}5 + 50\,100 + (0{,}150)(4186)(T_f - 0) = 0"))\
        .di("Agrupamos las capacidades caloríficas que multiplican a la incógnita y restamos el calor usado al disponible.",
            M(r"C_{\text{tot}} = 2093 + 270 + 627{,}9 = 2990{,}9\ \text{J}/^\circ\text{C}"),
            M(rf"2990{{,}}9\,T_f = {tex(q_disp, 6, sci='no')} - {tex(q_nec, 6, sci='no')} = {tex(q_rest, 6, sci='no')}"))\
        .di("Dividimos y obtenemos la temperatura final de equilibrio.",
            M(rf"T_f = \frac{{{tex(q_rest, 6, sci='no')}}}{{2990{{,}}9}}"),
            R(rf"T_f = {tex(tf, 4)}\ {C}", "rf"))\
        .di("Al final quedan cero coma seiscientos cincuenta kilogramos de agua líquida, el aluminio y nada de hielo.",
            NOTA("Composición final: $0{,}650$ kg de agua líquida + $0{,}300$ kg de aluminio a $11{,}94\\ ^\\circ\\text{C}$."))
    L.responde("", rf"T_f = {tex(tf, 4)}\ {C}", tf, "°C")
    return L.oficial("III Parcial ordinario II-2026", 11.94, tol=0.02).usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ C26-1
@leccion("C26-1")
def c26_1() -> Leccion:
    l0 = 2.00
    area = 5.00e-4
    v0 = l0 * area           # 1.00e-3 m^3 = 1.00 L = 1000 cm^3
    alpha = 1.20e-5
    beta = 3.0 * alpha       # 3.60e-5 /C
    young = 2.00e11
    t0 = 20.0
    delta = 6.00e-4          # 0.600 mm
    dt_c = delta / (alpha * l0)   # 25.0 C
    tc = t0 + dt_c           # 45.0 C
    tf = 105.0
    dt_tot = tf - t0         # 85.0 C
    dv = beta * v0 * dt_tot  # 3.06e-6 m^3 = 3.06 cm^3
    dl_libre = alpha * l0 * dt_tot  # 2.04e-3 m
    dl_imp = dl_libre - delta       # 1.44e-3 m
    dt_bloq = tf - tc        # 60.0 C
    esf = -young * alpha * dt_bloq  # -1.44e8 Pa
    fuerza = esf * area      # -7.20e4 N
    esf_lim = 2.40e8
    dt_lim = esf_lim / (young * alpha)  # 100.0 C
    t_max = tc + dt_lim      # 145.0 C

    L = Leccion(
        "C26-1",
        "Cartago Extremo · Esfuerzo térmico con holgura inicial y dilatación volumétrica",
        tema="4.4",
        tipo="desarrollo",
        nivel=5,
        fuente=_fuente_c26(1),
        enunciado=(
            "Una barra de acero ($L_0=2{,}00$ m, sección $A=5{,}00\\times 10^{-4}\\ \\text{m}^2$, "
            "$\\alpha=1{,}20\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$, módulo de Young $Y=2{,}00\\times 10^{11}$ Pa) "
            "se instala a $T_0=20{,}0\\ ^\\circ$C entre dos muros rígidos dejando una holgura inicial "
            "$\\delta=0{,}600$ mm en uno de sus extremos. "
            "a) Determine la temperatura $T_c$ a la cual la barra cierra la holgura y toca el muro. "
            "b) Si no existieran muros y se calentara hasta $T_f=105{,}0\\ ^\\circ$C, calcule el cambio de volumen "
            "$\\Delta V$ en $\\text{cm}^3$. "
            "c) Con los muros rígidos presentes a $T_f=105{,}0\\ ^\\circ$C, calcule el esfuerzo térmico $F/A$ y la "
            "fuerza $F$ ejercida por los muros. "
            "d) Si el límite de esfuerzo a compresión es $2{,}40\\times 10^8$ Pa, halle la temperatura máxima $T_{\\text{máx}}$."
        ),
        claves=["esfuerzo térmico", "holgura inicial", "dilatación volumétrica", "fórmula virgen II-2026", "Cartago Extremo"],
        visual={
            "tipo": "dilatacion",
            "caso": "barras",
            "u": "m",
            "T0": t0,
            "dTmax": t_max - t0,
            "barras": [{"n": "barra de acero (holgura 0,60 mm)", "L": l0, "a": alpha, "color": "rojo"}],
            "pie": "Primero cierra la holgura δ hasta 45,0 °C; solo el calentamiento extra genera esfuerzo térmico",
        },
    )
    L.paso("Qué nos dan y la trampa de la holgura")\
        .di("Este es el problema estrella para las dos casillas vírgenes de la primera fila: esfuerzo térmico y dilatación volumétrica.",
            DATO("holgura"),
            LISTA([
                "$L_0=2{,}00$ m, $A=5{,}00\\times 10^{-4}\\ \\text{m}^2$, $V_0=A L_0=1{,}00\\times 10^{-3}\\ \\text{m}^3=1{,}00$ L",
                "$\\alpha=1{,}20\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$, $Y=2{,}00\\times 10^{11}$ Pa",
                "$T_0=20{,}0\\ ^\\circ$C, holgura $\\delta=0{,}600\\ \\text{mm}=6{,}00\\times 10^{-4}$ m, $T_f=105{,}0\\ ^\\circ$C",
            ]))\
        .di("Mientras la barra recorre la holgura se dilata libremente con esfuerzo cero; el esfuerzo nace solo después de tocar el muro.",
            OJO("Error clásico: meter $\\Delta T=85{,}0\\ ^\\circ\\text{C}$ directo en $F/A=-Y\\alpha\\Delta T$ ignora la holgura y sobreestima el esfuerzo."))
    L.paso("a) Temperatura de contacto con el muro")\
        .di("Usamos la casilla de dilatación lineal para hallar qué incremento de temperatura cierra exactamente la brecha.",
            V(foco="calentar"), F(r"\Delta L = \alpha L_0\,\Delta T"),
            M(r"\delta = \alpha L_0\,\Delta T_c \;\Rightarrow\; \Delta T_c = \frac{\delta}{\alpha L_0}"),
            M(r"\Delta T_c = \frac{6{,}00\times 10^{-4}}{(1{,}20\times 10^{-5})(2{,}00)} = \frac{6{,}00\times 10^{-4}}{2{,}40\times 10^{-5}} = 25{,}0\ ^\circ\text{C}"))\
        .di("Sumamos a la temperatura inicial y obtenemos la temperatura de contacto.",
            R(rf"T_c = 20{{,}}0 + 25{{,}}0 = {tex(tc, 3)}\ {C}", "ra"))
    L.paso("b) Cambio de volumen libre hasta 105,0 °C")\
        .di("Primero derivamos beta igual a tres alfa desde la dilatación lineal para un sólido isótropo.",
            M(r"V = L_0^3(1+\alpha\Delta T)^3 \approx V_0(1+3\alpha\Delta T) \;\Rightarrow\; \beta = 3\alpha = 3{,}60\times 10^{-5}\ ^\circ\text{C}^{-1}"))\
        .di("Aplicamos la casilla de dilatación volumétrica con el salto total de ochenta y cinco grados.",
            F(r"\Delta V = \beta V_0\,\Delta T"),
            M(r"\Delta T_{\text{tot}} = 105{,}0 - 20{,}0 = 85{,}0\ ^\circ\text{C}"),
            M(rf"\Delta V = (3{{,}}60\times 10^{{-5}})(1{{,}}00\times 10^{{-3}})(85{{,}}0) = {tex(dv, 3, u='m^3')}"))\
        .di("Pasamos a centímetros cúbicos usando la equivalencia del formulario.",
            M(r"1\ \text{m}^3 = 10^6\ \text{cm}^3 \quad (1\ \text{L} = 1000\ \text{cm}^3)"),
            R(rf"\Delta V = {tex(dv * 1e6, 3)}\ \text{{cm}}^3", "rb"))
    L.paso("c) Esfuerzo térmico y fuerza con muros rígidos")\
        .di("A partir de cuarenta y cinco grados la barra ya no puede crecer: el salto térmico bloqueado es sesenta grados.",
            M(rf"\Delta L_{{\text{{libre}}}} = (1{{,}}20\times 10^{{-5}})(2{{,}}00)(85{{,}}0) = 2{{,}}04\ \text{{mm}} > 0{{,}}600\ \text{{mm}}"),
            M(rf"\Delta T_{{\text{{bloq}}}} = T_f - T_c = 105{{,}}0 - 45{{,}}0 = {tex(dt_bloq, 3)}\ {C}", "dtb", "azul"))\
        .di("Sustituimos ese incremento bloqueado en la casilla nueva de esfuerzo térmico.",
            F(r"\frac{F}{A} = -Y\alpha\,\Delta T"),
            M(rf"\frac{{F}}{{A}} = -(2{{,}}00\times 10^{{11}})(1{{,}}20\times 10^{{-5}})({tex(dt_bloq, 3)})"),
            R(rf"\frac{{F}}{{A}} = {tex(esf, 3, u='Pa')} = -144\ \text{{MPa}}\ (\text{{compresión}})", "rc1"))\
        .di("Multiplicamos por el área transversal para hallar la fuerza que ejercen los muros.",
            M(rf"F = \left(\frac{{F}}{{A}}\right)A = ({tex(esf, 3)})(5{{,}}00\times 10^{{-4}})"),
            R(rf"F = {tex(fuerza, 3, u='N')} = -72{{,}}0\ \text{{kN}}", "rc2"))\
        .por_que("Comprobación equivalente por deformación impedida: $\\Delta L_{\\text{imp}}=2{,}04-0{,}60=1{,}44$ mm y $F/A=-Y(\\Delta L_{\\text{imp}}/L_0)=-1{,}44\\times 10^8$ Pa.")
    L.paso("d) Temperatura máxima antes de superar el límite elástico")\
        .di("Despejamos el incremento bloqueado máximo que permite el límite de dos coma cuarenta por diez a la ocho pascales.",
            M(r"\left|\frac{F}{A}\right|_{\text{máx}} = Y\alpha\,\Delta T_{\text{bloq,máx}} \;\Rightarrow\; \Delta T_{\text{bloq,máx}} = \frac{|F/A|_{\text{máx}}}{Y\alpha}"),
            M(r"\Delta T_{\text{bloq,máx}} = \frac{2{,}40\times 10^8}{(2{,}00\times 10^{11})(1{,}20\times 10^{-5})} = 100{,}0\ ^\circ\text{C}"))\
        .di("Sumamos a la temperatura de contacto, porque hasta cuarenta y cinco grados no había esfuerzo.",
            R(rf"T_{{\text{{máx}}}} = T_c + 100{{,}}0 = 45{{,}}0 + 100{{,}}0 = {tex(t_max, 4)}\ {C}", "rd"))
    L.responde("a", rf"T_c={tex(tc, 3)}\ {C}", tc, "°C")\
     .responde("b", rf"\Delta V={tex(dv * 1e6, 3)}\ \text{{cm}}^3", dv * 1e6, "cm³")\
     .responde("c", rf"\frac{{F}}{{A}}={tex(esf, 3)}\ \text{{Pa}}", esf, "Pa")\
     .responde("d", rf"T_{{\text{{máx}}}}={tex(t_max, 4)}\ {C}", t_max, "°C")
    return L.oficial("Escudo Cartago Extremo II-2026 (SymPy)", 45.0, 3.06, -1.44e8, 145.0, tol=0.01).usa(
        r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T", r"\frac{F}{A}=-Y\alpha\Delta T",
    )
