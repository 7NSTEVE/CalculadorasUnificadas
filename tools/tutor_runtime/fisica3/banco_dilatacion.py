"""Superexamen Q001–Q015: equilibrio térmico y dilatación."""

from __future__ import annotations

import math

from .ayudas import revisar_opciones
from .banco_recomendados import _cinta, _lata
from .dsl import DATO, F, LISTA, M, MARCA, NOTA, OJO, Leccion, R, T, TABLA, V, VISUAL, s
from .formato import dice, n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion

AL = s("α")


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q001
@leccion("Q001")
def q001() -> Leccion:
    L = _sel("Q001", "¿Por qué el calor ganado es igual al perdido?",
             "Cuando dos cuerpos a distinta temperatura se ponen en contacto dentro de un calorímetro aislado, ¿por qué razón el calor ganado por uno es igual al calor perdido por el otro?",
             {"A": "Porque ambos cuerpos poseen el mismo calor específico.",
              "B": "Porque la temperatura final es el promedio aritmético de las temperaturas iniciales.",
              "C": "Porque en un sistema aislado la energía se conserva y el calor transferido entre los cuerpos no se pierde al entorno.",
              "D": "Porque el cuerpo con mayor masa siempre cede exactamente la misma cantidad de energía que absorbe el de menor masa."}, "C",
             claves=["calorímetro", "conservación de la energía"],
             visual={"tipo": "calor", "componentes": [{"n": "caliente", "m": 100, "T": 80, "fase": "caliente"}, {"n": "frío", "m": 100, "T": 20, "fase": "frio"}],
                     "necesita": [{"n": "gana el frío", "Q": 100}], "disponible": [{"n": "cede el caliente", "Q": 100}]})
    L.paso("Qué preguntan")\
        .di("La palabra importante es aislado: nada de energía entra ni sale del calorímetro.", DATO("aislado"), LISTA(["Dos cuerpos a distinta temperatura", "Calorímetro **aislado**: no se pierde calor"]))\
        .di("Preguntan por qué lo que uno gana es justo lo que el otro pierde.", M(r"Q_{\text{gana}} = Q_{\text{pierde}}\;?"))
    L.paso("La idea: la energía se conserva")\
        .di("Si el sistema está aislado, la suma de todos los calores es cero.", V(foco="comparar"), F(r"\sum Q = 0"))\
        .di("Con dos cuerpos, eso se escribe así.", M(r"Q_{\text{frío}}+Q_{\text{caliente}}=0"))\
        .di("Y despejando: uno gana exactamente lo que el otro pierde.", R(r"Q_{\text{frío}}=-Q_{\text{caliente}}"))\
        .por_que("El signo menos dice que uno recibe y el otro entrega. No importa la masa ni el material: importa que nada se escape.")
    revisar_opciones(L, {
        "A": ("La A no: los materiales pueden ser distintos y aun así se cumple.", "No hace falta el mismo $c$."),
        "B": ("La B no: el promedio solo sale si las masas y los materiales son iguales.", "El promedio es un caso especial."),
        "D": ("La D no: la masa no decide quién cede. Cede el que está más caliente.", "Cede el más caliente, no el más pesado."),
        "C": ("La C es la correcta: sistema aislado, la energía se conserva.", "Es la conservación de la energía."),
    })
    return L.oficial("III Parcial ordinario I-2026", "C").usa(r"\sum Q=0", r"Q=mc\Delta T")


# ============================================================================ Q002
@leccion("Q002")
def q002() -> Leccion:
    L = _sel("Q002", "Fundir dos cilindros de oro",
             "Dos cilindros de oro tienen la misma área transversal y temperatura, pero uno es el doble de alto. Una vez que ambos llegan al punto de fusión, el calor absorbido por cada cilindro para fundirse:",
             {"A": "Es el mismo dado que están en las mismas condiciones.", "B": "Es mayor en el cilindro más largo porque contiene más materia.",
              "C": "Es el mismo porque tienen la misma área y son del mismo material.", "D": "Es mayor en el cilindro corto porque tiene menos área lateral."}, "B",
             claves=["calor latente", "masa"])
    L.paso("Qué preguntan")\
        .di("Mismo material y misma área, pero uno mide el doble de alto.", DATO("doble de alto"), LISTA(["Los dos son de oro", "Misma área $A$", "Alturas: $h$ y $2h$"]))\
        .di("Ya están en el punto de fusión: solo falta fundirlos.", M(r"Q_{\text{fundir}} = \;?"))
    L.paso("La fórmula")\
        .di("Para fundir no cambia la temperatura: se usa el calor latente.", F(r"Q = mL_f"))\
        .di(f"{s('L')} efe es igual en los dos, porque ambos son de oro. Lo que cambia es la masa.", M(r"L_f\ \text{igual}\qquad m=\rho A h"))\
        .di("El doble de alto tiene el doble de masa, y pide el doble de calor.", M(r"m_2=\rho A(2h)=2m_1"), R(r"Q_2 = 2Q_1"))\
        .sale_de(("$m=\\rho A h$", "masa = densidad por volumen; el volumen del cilindro es área por altura"))
    revisar_opciones(L, {
        "A": ("La A no: mismas condiciones, pero distinta cantidad de oro.", "Distinta masa."),
        "C": ("La C no: misma área no significa misma masa.", "La altura es distinta."),
        "D": ("La D no: el área lateral no aparece en la fórmula.", "$Q=mL_f$ no depende del área lateral."),
        "B": ("La B es correcta: más materia, más calor.", "Doble masa, doble calor."),
    })
    return L.oficial("III Parcial extraordinario II-2023", "B").usa(r"Q=mL_f")


# ============================================================================ Q003
@leccion("Q003")
def q003() -> Leccion:
    L = _sel("Q003", "Barra compuesta: ¿cuál crece más?",
             "Barras de igual longitud: cerámicas $\\alpha_1$, metálicas $2\\alpha_1$, plásticas $3\\alpha_1$. Se forma una barra con dos segmentos alineados de materiales distintos y se calienta de 20,0 °C a 70,0 °C. ¿Cuál combinación aumenta más y cuál menos?",
             {"A": "Más: metal-cerámico; menos: plástico-cerámico.", "B": "Más: metal-plástico; menos: metal-cerámico.",
              "C": "Más: metal-plástico; menos: plástico-cerámico.", "D": "Más: plástico-cerámico; menos: metal-plástico."}, "B",
             claves=["dilatación lineal", "barra compuesta"],
             visual={"tipo": "dilatacion", "caso": "compuesta", "T0": 20, "T1": 70, "combos": [
                 {"n": "metal + plástico", "partes": [{"n": "metal", "a": 2, "color": "azul"}, {"n": "plástico", "a": 3, "color": "verde"}]},
                 {"n": "plástico + cerámica", "partes": [{"n": "plástico", "a": 3, "color": "verde"}, {"n": "cerámica", "a": 1, "color": "naranja"}]},
                 {"n": "metal + cerámica", "partes": [{"n": "metal", "a": 2, "color": "azul"}, {"n": "cerámica", "a": 1, "color": "naranja"}]}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Tres materiales. Entre más grande el alfa, más crece la barra.", DATO("igual longitud"), LISTA(["cerámica: $\\alpha_1$", "metal: $2\\alpha_1$", "plástico: $3\\alpha_1$", "Cada segmento mide lo mismo: $L$"]))\
        .di("Se juntan dos segmentos y se calientan cincuenta grados.", M(r"\Delta T = 70{,}0-20{,}0 = 50{,}0\ ^\circ\text{C}"))
    L.paso("La idea: se suman los crecimientos")\
        .di("Cada segmento crece por su cuenta y el total es la suma.", V(foco="calentar"), F(r"\Delta L=\alpha L\,\Delta T"))\
        .di("Como el largo y el cambio de temperatura son iguales, se saca factor común.", M(r"\Delta L_{\text{total}} = \alpha_a L\Delta T+\alpha_b L\Delta T"), M(r"\Delta L_{\text{total}} = (\alpha_a+\alpha_b)\,L\,\Delta T", "sum", "azul"))\
        .di("Entonces solo hay que sumar los alfas.")\
        .por_que("$L$ y $\\Delta T$ son iguales para todas las combinaciones, así que no influyen en la comparación.")
    L.paso("Sumar los alfas")\
        .di("Hacemos las tres combinaciones.", TABLA(["Combinación", "Suma"], [["metal + plástico", "$2\\alpha_1+3\\alpha_1=5\\alpha_1$"], ["plástico + cerámica", "$3\\alpha_1+\\alpha_1=4\\alpha_1$"], ["metal + cerámica", "$2\\alpha_1+\\alpha_1=3\\alpha_1$"]], resaltar=[[0, 1], [2, 1]]))\
        .di("La mayor es metal con plástico y la menor es metal con cerámica.", M(r"5\alpha_1 > 4\alpha_1 > 3\alpha_1"))
    revisar_opciones(L, {
        "A": ("La A no: metal cerámico es la que menos crece.", "Metal-cerámico suma $3\\alpha_1$, la menor."),
        "C": ("La C acierta la mayor, pero plástico cerámico no es la menor.", "Plástico-cerámico suma $4\\alpha_1$."),
        "D": ("La D está al revés.", "Metal-plástico es la mayor."),
        "B": ("La B es la correcta.", "$5\\alpha_1$ la mayor, $3\\alpha_1$ la menor."),
    })
    return L.oficial("III Parcial extraordinario II-2025", "B").usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ Q004
@leccion("Q004")
def q004() -> Leccion:
    L = _sel("Q004", "Que la esfera pase por el anillo",
             "Los coeficientes de dilatación son mayores en los polímeros, luego los metales y por último los cerámicos. ¿Para cuál combinación, con el menor incremento de temperatura, es más probable que una esfera pase por un anillo al calentarlas juntas, si el diámetro de la esfera es 0,1 % mayor?",
             {"A": "Esfera metálica y anillo cerámico.", "B": "Esfera polimérica y anillo metálico.", "C": "Esfera polimérica y anillo cerámico.", "D": "Esfera cerámica y anillo polimérico."}, "D",
             claves=["dilatación", "anillo y esfera"],
             visual={"tipo": "dilatacion", "caso": "anillo", "D": 10.01, "d": 10.0, "a": 8e-5, "T0": 20, "dTmax": 40, "modo": "anillo", "nb": "esfera"})
    L.paso("Qué nos dan y qué piden")\
        .di("La esfera es un poquito más grande que el anillo: no pasa.", DATO("0,1 % mayor"), LISTA(["$\\alpha_{\\text{polímero}} > \\alpha_{\\text{metal}} > \\alpha_{\\text{cerámica}}$", "Esfera $0{,}1\\,\\%$ más grande que el hueco"]))\
        .di("Se calientan las dos juntas. Queremos que pase con el menor calentamiento.", M(r"\Delta T\ \text{mínimo}"))
    L.paso("La idea: el hueco debe crecer más que la esfera")\
        .di("Las dos crecen. Para que pase, el hueco tiene que ganarle a la esfera.", V(foco="calentar"), M(r"\Delta d_{\text{anillo}} > \Delta d_{\text{esfera}}", "cond", "azul"))\
        .di("Lo que decide es la diferencia de los alfas.", M(r"\Delta d = \alpha\,d_0\,\Delta T"), M(r"(\alpha_{\text{anillo}}-\alpha_{\text{esfera}})\,\Delta T \approx 0{,}001"))\
        .di("Entre más grande esa diferencia, menos hay que calentar.", M(r"\Delta T \approx \frac{0{,}001}{\alpha_{\text{anillo}}-\alpha_{\text{esfera}}}", "dt", "verde"))\
        .sale_de(("$0{,}001$", "el 0,1 % que le falta al hueco"), ("$d_0$", "los dos diámetros son casi iguales, por eso se cancela"))
    L.paso("Escoger materiales")\
        .di("Conviene el anillo que más crece, polímero, y la esfera que menos crece, cerámica.", M(r"\alpha_{\text{anillo}}\ \text{máximo}:\ \text{polímero}"), M(r"\alpha_{\text{esfera}}\ \text{mínimo}:\ \text{cerámica}"))\
        .di("Recuerda: el hueco del anillo crece como si estuviera relleno.", OJO("El hueco no se cierra al calentar: se agranda."))
    revisar_opciones(L, {
        "A": ("La A no: la esfera crece más que el anillo, se traba peor.", "Crece más la esfera."),
        "B": ("La B tampoco: la esfera de polímero crece más que el anillo.", "Crece más la esfera."),
        "C": ("La C es la peor de todas: esfera que más crece, anillo que menos.", "La peor combinación."),
        "D": ("La D es la correcta: el anillo crece mucho y la esfera casi nada.", "Diferencia de $\\alpha$ máxima."),
    })
    return L.oficial("III Parcial ordinario II-2025", "D").usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ Q005
@leccion("Q005")
def q005() -> Leccion:
    L = _sel("Q005", "Dos barras que crecen lo mismo (distinto ΔT)",
             "Dos barras de distinto material con $L_1<L_2$ se calientan $\\Delta T_1<\\Delta T_2$ y resulta $\\Delta L_1=\\Delta L_2$. Se puede afirmar que",
             {"A": "$\\alpha_2>\\alpha_1$", "B": "$\\alpha_2=\\alpha_1$", "C": "$\\alpha_2<\\alpha_1$", "D": "es imposible comparar los coeficientes de dilatación lineal de las barras."}, "C",
             claves=["dilatación lineal", "comparar coeficientes"],
             visual={"tipo": "dilatacion", "caso": "barras", "u": "cm", "T0": 20, "dTmax": 100, "barras": [{"n": "barra 1", "L": 60, "a": 3e-5, "color": "naranja"}, {"n": "barra 2", "L": 100, "a": 1.2e-5, "color": "azul"}]})
    L.paso("Qué nos dan y qué piden")\
        .di("La barra uno es más corta y se calienta menos, pero crece igual que la otra.", DATO("L1"), LISTA(["$L_1<L_2$", "$\\Delta T_1<\\Delta T_2$", "$\\Delta L_1=\\Delta L_2$"]))\
        .di("Piden comparar los alfas.", M(r"\alpha_1\ \;?\ \;\alpha_2"))
    L.paso("Igualar los crecimientos")\
        .di("Usamos la dilatación lineal en las dos.", F(r"\Delta L=\alpha L\,\Delta T"), M(r"\alpha_1L_1\Delta T_1=\alpha_2L_2\Delta T_2"))\
        .di("Despejamos el cociente de alfas.", M(r"\frac{\alpha_2}{\alpha_1}=\frac{L_1\,\Delta T_1}{L_2\,\Delta T_2}", "coc", "azul"))\
        .di("Arriba hay dos cosas más pequeñas, abajo dos más grandes: el cociente es menor que uno.", M(r"\frac{L_1}{L_2}<1\qquad \frac{\Delta T_1}{\Delta T_2}<1"), R(r"\frac{\alpha_2}{\alpha_1}<1 \;\Rightarrow\; \alpha_2<\alpha_1"))\
        .despacio((r"\alpha_1L_1\Delta T_1=\alpha_2L_2\Delta T_2", "partimos de la igualdad"), (r"\frac{\alpha_1L_1\Delta T_1}{\alpha_1L_2\Delta T_2}=\frac{\alpha_2L_2\Delta T_2}{\alpha_1L_2\Delta T_2}", "dividimos los dos lados entre lo mismo"), (r"\frac{L_1\Delta T_1}{L_2\Delta T_2}=\frac{\alpha_2}{\alpha_1}", "se cancela y queda el cociente"))\
        .por_que("La barra 1 tiene menos largo y menos calentamiento, pero crece igual. Eso solo puede pasar si su material se dilata más.")
    revisar_opciones(L, {
        "A": ("La A no: es al revés.", "El cociente $\\alpha_2/\\alpha_1$ es menor que 1."),
        "B": ("La B no: con alfas iguales, la barra uno crecería menos.", "Con $\\alpha$ iguales no crecerían igual."),
        "D": ("La D no: sí se pudo comparar, porque las dos desigualdades empujan hacia el mismo lado.", "Las dos desigualdades apuntan igual."),
        "C": ("La C es la correcta.", "$\\alpha_2<\\alpha_1$."),
    })
    return L.oficial("III Parcial extraordinario I-2026", "C").usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ Q006
@leccion("Q006")
def q006() -> Leccion:
    D, d, a, T0 = 2.46, 2.45, 12e-6, 25.0
    dA = (d - D) / (D * a)
    Ta = T0 + dA
    dB = (D - d) / (d * a)
    Tb = T0 + dB
    L = _des("Q006", "Meter una barra en un anillo de hierro",
             "Se quiere introducir una barra de hierro de 2,46 cm de diámetro en un anillo de hierro de 2,45 cm de diámetro interior, ambos a 25,0 °C ($\\alpha=12{,}0\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$). A) Temperatura a la que habría que enfriar la barra. B) Temperatura a la que habría que calentar el anillo. C) ¿Cuál proceso recomendaría?",
             claves=["dilatación lineal", "anillo", "cero absoluto"],
             visual={"tipo": "dilatacion", "caso": "anillo", "D": D, "d": d, "a": a, "T0": 25, "dTmax": 360, "modo": "anillo", "nb": "barra"})
    L.paso("Qué nos dan y qué piden")\
        .di("La barra es una décima de milímetro más ancha que el hueco: no entra.", DATO("2,46 cm"), DATO("2,45 cm"), LISTA(["Barra: $D_0=2{,}46$ cm", "Hueco del anillo: $d_0=2{,}45$ cm", "$\\alpha=12{,}0\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$", "Los dos a $25{,}0\\ ^\\circ\\text{C}$"]))\
        .di("Hay dos caminos: encoger la barra o agrandar el anillo.", M(r"|\Delta D| = 2{,}46-2{,}45 = 0{,}01\ \text{cm}", "falta"))\
        .sale_de(("$0{,}01$ cm", "lo que sobra de barra"))
    L.paso("A) Enfriar la barra: despejar ΔT")\
        .di("El diámetro se dilata como cualquier longitud.", F(r"\Delta D=\alpha D_0\,\Delta T"))\
        .di("Despejamos el cambio de temperatura.", M(r"\Delta T=\frac{\Delta D}{\alpha D_0}"))\
        .di("La barra debe perder cero coma cero uno centímetros, por eso el signo menos.", M(r"\Delta T=\frac{-0{,}01}{(12{,}0\times10^{-6})(2{,}46)}"), M(rf"\Delta T={tex(dA, dec=1)}\ ^\circ\text{{C}}", "dta"))\
        .por_que("Aquí $D_0$ es el diámetro de lo que cambia: la barra, 2,46 cm.")
    L.paso("A) Temperatura final y la trampa")\
        .di("Sumamos a los veinticinco grados del inicio.", M(rf"T_f=25{{,}}0+({tex(dA, dec=1)})"), R(rf"T_f={tex(Ta, dec=1)}\ ^\circ\text{{C}}", "ra"))\
        .di("Pero el cero absoluto está en menos doscientos setenta y tres grados. Eso no existe: es imposible.", OJO("Nada puede estar por debajo de $-273{,}15\\ ^\\circ\\text{C}$. El inciso A da un proceso **imposible**."))
    L.paso("B) Calentar el anillo")\
        .di("Ahora cambia el anillo, así que se usa su diámetro.", V(foco="calentar"), M(r"\Delta T=\frac{\Delta d}{\alpha d_0}=\frac{0{,}01}{(12{,}0\times10^{-6})(2{,}45)}"))\
        .di(f"Son {n(dB, dec=1)} grados de calentamiento.", M(rf"\Delta T={tex(dB, dec=1)}\ ^\circ\text{{C}}"))\
        .di("Más los veinticinco iniciales.", R(rf"T_f=25{{,}}0+{tex(dB, dec=1)}={tex(Tb, dec=1)}\ ^\circ\text{{C}}", "rb"))\
        .pregunta_rapida("Al calentar el anillo, ¿qué le pasa al hueco?", ["Se hace más grande", "Se hace más pequeño"], 0, "El hueco se dilata igual que si estuviera lleno de hierro.")
    L.paso("C) ¿Cuál se recomienda?")\
        .di("Calentar el anillo: se puede con un horno o un mechero.", T("**Calentar el anillo.** 365 °C se alcanza con un mechero u horno."))\
        .di("Enfriar la barra pediría bajar del cero absoluto, y eso es imposible.", T("Enfriar la barra exigiría bajar de $-273{,}15\\ ^\\circ\\text{C}$: **imposible**."))
    L.responde("A", rf"T_f={tex(Ta, dec=1)}\ ^\circ\text{{C}}\ (\text{{imposible}})", Ta, "°C").responde("B", rf"T_f={tex(Tb, dec=1)}\ ^\circ\text{{C}}", Tb, "°C")
    L.oficial("III Parcial extraordinario I-2026", -313.8, 365.1).usa(r"\Delta L=\alpha L_0\Delta T")
    x = 0.005 / (2.0 * 17e-6)
    L.te_toca("Un anillo de cobre ($\\alpha=17\\times10^{-6}$) de 2,000 cm de hueco debe dejar pasar una esfera de 2,005 cm. ¿Cuánto hay que calentarlo, en °C?", x, "°C", pista="$\\Delta T=\\frac{\\Delta d}{\\alpha d_0}$ con $d_0=2{,}000$ cm.", solucion="$\\Delta T=\\frac{0{,}005}{(17\\times10^{-6})(2{,}000)}=147\\ ^\\circ\\text{C}$")
    return L


# ============================================================================ Q007
@leccion("Q007")
def q007() -> Leccion:
    L = _sel("Q007", "Dos barras que crecen lo mismo (igual ΔT)",
             "Dos barras de distinto material con $L_1>L_2$ se calientan el mismo $\\Delta T$ y resulta $\\Delta L_1=\\Delta L_2$. Se puede afirmar que",
             {"A": "$\\alpha_2>\\alpha_1$", "B": "$\\alpha_2=\\alpha_1$", "C": "$\\alpha_2<\\alpha_1$", "D": "es imposible comparar los coeficientes de dilatación lineal de las barras."}, "A",
             claves=["dilatación lineal", "comparar coeficientes"],
             visual={"tipo": "dilatacion", "caso": "barras", "u": "cm", "T0": 20, "dTmax": 100, "barras": [{"n": "barra 1", "L": 100, "a": 1.2e-5, "color": "azul"}, {"n": "barra 2", "L": 60, "a": 2e-5, "color": "naranja"}]})
    L.paso("Qué nos dan y qué piden")\
        .di("La barra uno es más larga. Se calientan igual y crecen igual.", DATO("L1"), LISTA(["$L_1>L_2$", "Mismo $\\Delta T$", "$\\Delta L_1=\\Delta L_2$"]))\
        .di("Piden comparar los alfas.", M(r"\alpha_1\ \;?\ \;\alpha_2"))
    L.paso("Igualar los crecimientos")\
        .di("Escribimos la dilatación de cada una y las igualamos.", F(r"\Delta L=\alpha L\,\Delta T"), M(r"\alpha_1L_1\,\Delta T=\alpha_2L_2\,\Delta T", "ig"))\
        .di("El cambio de temperatura es el mismo: se cancela.", M(r"\alpha_1L_1\,\cancel{\Delta T}=\alpha_2L_2\,\cancel{\Delta T}"), M(r"\frac{\alpha_2}{\alpha_1}=\frac{L_1}{L_2}", "coc", "azul"))\
        .di("Como la uno es más larga, ese cociente es mayor que uno.", R(r"\frac{L_1}{L_2}>1 \;\Rightarrow\; \alpha_2>\alpha_1"))\
        .por_que("La barra corta necesita un material que se dilate más para crecer lo mismo que la larga.")
    revisar_opciones(L, {
        "B": ("La B no: con alfas iguales, la larga crecería más.", "Con $\\alpha$ iguales no crecerían igual."),
        "C": ("La C está al revés.", "El cociente es mayor que 1."),
        "D": ("La D no: sí se pudo comparar.", "Se comparó con la igualdad."),
        "A": ("La A es la correcta.", "$\\alpha_2>\\alpha_1$."),
    })
    return L.oficial("III Parcial ordinario I-2026", "A").usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ Q008
@leccion("Q008")
def q008() -> Leccion:
    L0, dLm, aal, dT = 1060.0, 0.152, 2.30e-5, 30.0
    am = dLm / (L0 * dT)
    dLal = aal * L0 * dT
    dif = dLal - dLm
    dTc = 0.10 / (L0 * (aal - am))
    Tc = 20.0 - dTc
    L = _des("Q008", "Dos bates de béisbol: madera y aluminio",
             "Dos bates de 106,00 cm a 20,0 °C, uno de madera y otro de aluminio, se calientan hasta 50,0 °C. El de madera aumenta 0,152 mm; el de aluminio tiene $\\alpha=2{,}30\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$. A) $\\alpha$ de la madera. B) Diferencia de tamaño a 50,0 °C. C) Temperatura a la que deben enfriarse (desde 20,0 °C) para que uno sea 0,10 mm más pequeño que el otro.",
             claves=["dilatación lineal", "diferencia de longitudes"],
             visual={"tipo": "dilatacion", "caso": "barras", "u": "mm", "T0": 20, "dTmax": 30, "barras": [{"n": "madera", "L": L0, "a": am, "color": "naranja"}, {"n": "aluminio", "L": L0, "a": aal, "color": "azul"}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Los dos bates miden lo mismo al inicio, pero son de distinto material.", DATO("106,00 cm"), DATO("0,152 mm"), LISTA(["$L_0=106{,}00$ cm $=1060{,}0$ mm", "De $20{,}0$ a $50{,}0\\ ^\\circ\\text{C}$: $\\Delta T=30{,}0\\ ^\\circ\\text{C}$", "Madera: $\\Delta L_m=0{,}152$ mm", "Aluminio: $\\alpha_{Al}=2{,}30\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$"]))\
        .di("Trabajamos todo en milímetros para no equivocarnos.", M(r"\alpha_m=\;?\qquad \Delta L_{Al}-\Delta L_m=\;?\qquad T_f=\;?"))\
        .sale_de(("$1060{,}0$ mm", "106,00 cm por 10 mm en cada centímetro"))
    L.paso("A) Coeficiente de la madera")\
        .di("Dilatación lineal.", F(r"\Delta L=\alpha L_0\,\Delta T"))\
        .di(f"Despejamos {AL}: lo que multiplica pasa a dividir.", M(r"\alpha_m=\frac{\Delta L_m}{L_0\,\Delta T}"))\
        .di("Ponemos los números, todo en milímetros.", M(r"\alpha_m=\frac{0{,}152}{(1060{,}0)(30{,}0)}"), R(rf"\alpha_m={tex(am, 3)}\ ^\circ\text{{C}}^{{-1}}", "ra"))
    L.paso("B) Cuánto crece el aluminio")\
        .di("Con el alfa del aluminio.", V(foco="calentar"), M(r"\Delta L_{Al}=(2{,}30\times10^{-5})(1060{,}0)(30{,}0)"), M(rf"\Delta L_{{Al}}={tex(dLal, 4)}\ \text{{mm}}", "dal"))\
        .di("La diferencia de tamaño es la diferencia de los crecimientos, porque empezaron iguales.", M(r"L_{Al}-L_m=(L_0+\Delta L_{Al})-(L_0+\Delta L_m)"), M(r"L_{Al}-L_m=\Delta L_{Al}-\Delta L_m", "dif"))\
        .di("Restamos.", M(rf"={tex(dLal, 4)}-0{{,}}152"), R(rf"L_{{Al}}-L_m={tex(dif, 3)}\ \text{{mm}}", "rb"))
    L.paso("C) Plantear: ahora se enfrían")\
        .di("Al enfriarse, los dos se encogen; el aluminio se encoge más.", V(foco="enfriar"), M(r"|\Delta L_{Al}|-|\Delta L_m| = 0{,}10\ \text{mm}", "pl", "azul"))\
        .di("Escribimos cada uno con su alfa y sacamos factor común.", M(r"\alpha_{Al}L_0|\Delta T|-\alpha_mL_0|\Delta T|=0{,}10"), M(r"(\alpha_{Al}-\alpha_m)\,L_0\,|\Delta T|=0{,}10"))\
        .por_que("El que tiene el alfa mayor cambia más, tanto al calentar como al enfriar.")
    L.paso("C) Despejar la temperatura")\
        .di("Despejamos el cambio de temperatura.", M(r"|\Delta T|=\frac{0{,}10}{(\alpha_{Al}-\alpha_m)\,L_0}"))\
        .di("Primero la resta de los alfas.", M(rf"\alpha_{{Al}}-\alpha_m=2{{,}}30\times10^{{-5}}-0{{,}}478\times10^{{-5}}={tex(aal - am, 4)}"))\
        .di("Sustituimos.", M(rf"|\Delta T|=\frac{{0{{,}}10}}{{({tex(aal - am, 4)})(1060{{,}}0)}}={tex(dTc, 3)}\ ^\circ\text{{C}}"))\
        .di("Como se enfría, se resta de los veinte grados.", R(rf"T_f=20{{,}}0-{tex(dTc, 3)}={tex(Tc, dec=1)}\ ^\circ\text{{C}}", "rc"))\
        .despacio((r"0{,}478\times10^{-5}=4{,}78\times10^{-6}", "mismo número con el exponente igualado para poder restar"),)\
        .pregunta_rapida("¿Cuál bate queda más corto al enfriarse?", ["El de aluminio", "El de madera"], 0, "Tiene el $\\alpha$ mayor: se encoge más.")
    L.responde("A", rf"\alpha_m={tex(am, 3)}\ ^\circ\text{{C}}^{{-1}}", am, "1/°C").responde("B", rf"{tex(dif, 3)}\ \text{{mm}}", dif, "mm").responde("C", rf"T_f={tex(Tc, dec=1)}\ ^\circ\text{{C}}", Tc, "°C")
    L.oficial("III Parcial ordinario I-2026", 4.78e-6, 0.579, 14.8).usa(r"\Delta L=\alpha L_0\Delta T")
    L.te_toca("Una barra de 800,0 mm crece 0,480 mm al subir 50,0 °C. ¿Cuánto vale $\\alpha$ en $10^{-6}\\ ^\\circ\\text{C}^{-1}$?", 12.0, "×10⁻⁶ °C⁻¹", pista="$\\alpha=\\frac{\\Delta L}{L_0\\Delta T}$", solucion="$\\alpha=\\frac{0{,}480}{(800{,}0)(50{,}0)}=1{,}20\\times10^{-5}=12{,}0\\times10^{-6}$")
    return L


# ============================================================================ Q009
@leccion("Q009")
def q009() -> Leccion:
    V0, be, ba, dT = 2.80, 75e-5, 3.6e-5, -14.0
    dVe, dVa = be * V0 * dT, ba * V0 * dT
    ad = dVa - dVe
    L = _des("Q009", "Tanque de acero lleno de etanol",
             "Un tanque de acero se llena totalmente con 2,80 m³ de etanol a 32,0 °C. Al enfriarse ambos a 18,0 °C, ¿qué volumen adicional de etanol podrá meterse?",
             claves=["dilatación volumétrica", "líquido en recipiente"],
             aviso="El enunciado no da los coeficientes. Se usan los del folleto: etanol 75×10⁻⁵ °C⁻¹ y acero 3,6×10⁻⁵ °C⁻¹ (volumétricos).",
             visual={"tipo": "dilatacion", "caso": "lata", "V": V0, "bc": ba, "bl": be, "T0": 32, "T1": 18, "Vliq": V0, "u": "m³", "nc": "tanque", "nl": "etanol"})
    L.paso("Qué nos dan y qué piden")\
        .di("El tanque está lleno hasta el borde. Al enfriarse se encogen los dos, pero el etanol se encoge mucho más.", DATO("2.80 m3"), LISTA(["$V_0=2{,}80\\ \\text{m}^3$ (tanque y etanol)", "De $32{,}0$ a $18{,}0\\ ^\\circ\\text{C}$", "$\\beta_{et}=75\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$", "$\\beta_{ac}=3{,}6\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$"]))\
        .di("Se enfría: el cambio es negativo.", M(r"\Delta T=18{,}0-32{,}0=-14{,}0\ ^\circ\text{C}", "dt"))
    L.paso("La idea: el espacio que queda")\
        .di("El hueco que aparece es la diferencia entre lo que mide el tanque y lo que mide el etanol.", V(foco="enfriar"), M(r"V_{\text{extra}}=V_{\text{tanque}}-V_{\text{etanol}}", "idea", "azul"))\
        .di("Cada volumen final es el inicial más su cambio.", M(r"V_{\text{extra}}=(V_0+\Delta V_{ac})-(V_0+\Delta V_{et})"), M(r"V_{\text{extra}}=\Delta V_{ac}-\Delta V_{et}", "f"))\
        .por_que("El volumen inicial es el mismo, por eso se cancela y solo quedan los cambios.")
    L.paso("Cuánto se encoge el etanol")\
        .di("Dilatación volumétrica.", F(r"\Delta V=\beta V_0\,\Delta T"), M(r"\Delta V_{et}=(75\times10^{-5})(2{,}80)(-14{,}0)"))\
        .di("Se encoge casi tres centésimas de metro cúbico.", M(rf"\Delta V_{{et}}={tex(dVe, 3, sci='no')}\ \text{{m}}^3", "ve", "rojo"))
    L.paso("Cuánto se encoge el tanque")\
        .di("Igual, pero con el beta del acero.", M(r"\Delta V_{ac}=(3{,}6\times10^{-5})(2{,}80)(-14{,}0)"), M(rf"\Delta V_{{ac}}={tex(dVa, 3, sci='no')}\ \text{{m}}^3", "va", "rojo"))\
        .di("El hueco de un tanque se encoge como si fuera macizo.", NOTA("El hueco cambia igual que el material."))
    L.paso("El volumen extra")\
        .di("Restamos, con mucho cuidado con los signos.", M(rf"V_{{\text{{extra}}}}=({tex(dVa, 3, sci='no')})-({tex(dVe, 3, sci='no')})"), V(foco="vacio"))\
        .di(f"Caben {nu(ad, 'm3', 3, sci='no')} más.", R(rf"V_{{\text{{extra}}}}={tex(ad, 3, sci='no')}\ \text{{m}}^3"))\
        .di("Son unos veintiocho litros.", NOTA(rf"${tex(ad, 3, sci='no')}\ \text{{m}}^3 = {tex(ad * 1000, 3)}$ litros."), OJO("Menos por menos da más: $-(-0{,}0294)=+0{,}0294$."))
    L.responde("", rf"V_{{\text{{extra}}}}={tex(ad, 3, sci='no')}\ \text{{m}}^3", ad, "m3").oficial("folleto, sección 3.1", 0.02799).usa(r"\Delta V=\beta V_0\Delta T")
    L.te_toca("Un recipiente de vidrio ($\\beta=2{,}7\\times10^{-5}$) de 500 mL está lleno de agua ($\\beta=2{,}1\\times10^{-4}$) a 20 °C. ¿Cuántos mL se derraman al llegar a 60 °C?", 500 * (2.1e-4 - 2.7e-5) * 40, "mL", pista="$\\Delta V_{agua}-\\Delta V_{vidrio}$ con $\\Delta T=40$.", solucion="$500(2{,}1\\times10^{-4}-2{,}7\\times10^{-5})(40)=3{,}66$ mL")
    return L


# ============================================================================ Q010
@leccion("Q010")
def q010() -> Leccion:
    R0, Lc, f, dT = 2.000, 20.000, 0.0020, 75.0
    al = f / dT
    V0 = math.pi * R0 ** 2 * Lc
    dV = 3 * al * V0 * dT
    Vf = V0 + dV
    L = _des("Q010", "Cilindro cuyo radio crece 0,20 %",
             "Un cilindro isotrópico ($V=\\pi R^2L$) con $R=2{,}000$ cm y $L=20{,}000$ cm a 25,00 °C se calienta 75,00 °C y su radio aumenta 0,20 %. a) Coeficiente lineal. b) Porcentaje de dilatación del largo. c) Porcentaje de dilatación del volumen. d) Volumen a 100,00 °C.",
             claves=["dilatación", "porcentajes", "volumen"],
             visual={"tipo": "dilatacion", "caso": "cilindro", "R": R0, "L": Lc, "a": al, "T0": 25, "T1": 100})
    L.paso("Qué nos dan y qué piden")\
        .di("Un cilindro macizo del mismo material por todos lados: eso significa isotrópico.", DATO("0,20"), LISTA(["$R_0=2{,}000$ cm, $L_0=20{,}000$ cm", "$\\Delta T=75{,}00\\ ^\\circ\\text{C}$", "El radio crece $0{,}20\\,\\%$"]))\
        .di("Un porcentaje se usa como decimal.", M(r"0{,}20\,\%=\frac{0{,}20}{100}=0{,}0020"), M(r"\Delta R=0{,}0020\,R_0", "dr"))
    L.paso("a) Coeficiente lineal")\
        .di("El radio es una longitud, así que usa la dilatación lineal.", F(r"\Delta R=\alpha R_0\,\Delta T"))\
        .di("Ponemos lo que sabemos y cancelamos el radio.", M(r"0{,}0020\,\cancel{R_0}=\alpha\,\cancel{R_0}\,(75{,}00)"), M(r"\alpha=\frac{0{,}0020}{75{,}00}"))\
        .di("Queda.", R(rf"\alpha={tex(al, 3)}\ ^\circ\text{{C}}^{{-1}}", "ra"), V(foco="calentar"))
    L.paso("b) Porcentaje del largo")\
        .di("El largo usa el mismo alfa y el mismo cambio de temperatura.", M(r"\frac{\Delta L}{L_0}=\alpha\,\Delta T"), M(r"\frac{\Delta L}{L_0}=(2{,}67\times10^{-5})(75)=0{,}0020"))\
        .di("Igual que el radio: cero coma veinte por ciento.", R(r"\frac{\Delta L}{L_0}=0{,}20\,\%", "rb"))\
        .por_que("En un material isotrópico todas las longitudes crecen en el mismo porcentaje.")
    L.paso("c) Porcentaje del volumen")\
        .di("Partimos de la dilatación lineal del formulario en las tres dimensiones del cilindro: dos veces el radio y una vez el largo.",
            F(r"\Delta L = \alpha L_0\,\Delta T \;\Rightarrow\; V = \pi R^2 L = \pi R_0^2 L_0(1+\alpha\Delta T)^3 \approx V_0(1+3\alpha\Delta T)"),
            F(r"\Delta V = \beta V_0\,\Delta T \quad\text{con}\quad \beta = 3\alpha"))\
        .di("Dividimos entre el volumen inicial.", M(r"\frac{\Delta V}{V_0}=3\alpha\,\Delta T=3(0{,}0020)"), R(r"\frac{\Delta V}{V_0}=0{,}0060=0{,}60\,\%", "rc"))\
        .sale_de(("$3\\alpha$", "al expandir $(1+\\alpha\\Delta T)^3 \\approx 1+3\\alpha\\Delta T$ desde $\\Delta L=\\alpha L_0\\Delta T$"))
    L.paso("d) Volumen a 100 °C")\
        .di("Primero el volumen inicial.", M(r"V_0=\pi R_0^2L_0=\pi(2{,}000)^2(20{,}000)"), M(rf"V_0={tex(V0, 5)}\ \text{{cm}}^3", "v0"))\
        .di("Le sumamos el cero coma sesenta por ciento.", M(rf"V=V_0(1+0{{,}}0060)={tex(V0, 5)}(1{{,}}0060)"))\
        .di("Queda.", R(rf"V={tex(Vf, 4)}\ \text{{cm}}^3={tex(Vf * 1e-6, 4)}\ \text{{m}}^3", "rd"), NOTA("El folleto escribe $2{,}515\\times10^{-4}\\ \\text{m}^3$ porque redondeó $V_0$ a $2{,}5\\times10^{-4}$ antes de sumar."))
    L.responde("a", rf"\alpha={tex(al, 3)}\ ^\circ\text{{C}}^{{-1}}", al, "1/°C").responde("b", r"0{,}20\,\%", 0.20, "%").responde("c", r"0{,}60\,\%", 0.60, "%").responde("d", rf"V={tex(Vf * 1e-6, 4)}\ \text{{m}}^3", Vf * 1e-6, "m3")
    L.oficial("folleto, sección 3.1", 2.67e-5, 0.20, 0.60, 2.515e-4, tol=0.01, nota="El folleto redondeó el volumen inicial antes de sumar.").usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T")
    return L


# ============================================================================ Q011
@leccion("Q011")
def q011() -> Leccion:
    L = _cinta("Q011", fuente("Q011"))
    return L.oficial("folleto, sección 3.1", 79.95, 0.99967, 79.98, tol=0.001)


# ============================================================================ Q012
@leccion("Q012")
def q012() -> Leccion:
    L = _des("Q012", "Coeficiente de dilatación que depende de T",
             "Un material de longitud $L_0$ tiene $\\alpha_1=\\beta+\\gamma T^3$ en $[T,2T]$ y $\\alpha_2=3\\beta+2\\gamma T^3$ en $[2T,3T]$. a) Longitud al pasar de $T$ a $2T$. b) Longitud al pasar de $2T$ a $3T$. c) Coeficiente promedio de $T$ a $3T$.",
             claves=["dilatación", "integral", "coeficiente variable"],
             aviso="El folleto resuelve con 2β en el segundo tramo, pero el enunciado dice 3β. Aquí se resuelve con lo que dice el enunciado.")
    L.paso("Qué nos dan y qué piden")\
        .di(f"El coeficiente {AL} cambia con la temperatura, y además tiene una fórmula distinta en cada tramo.", LISTA(["Tramo 1, de $T$ a $2T$: $\\alpha_1=\\beta+\\gamma T^3$", "Tramo 2, de $2T$ a $3T$: $\\alpha_2=3\\beta+2\\gamma T^3$", "Largo inicial: $L_0$"]))\
        .di("Como alfa no es constante, hay que integrar.", M(r"\Delta L=\int \alpha\,L_0\,dT"))\
        .di("Para no confundir, adentro de la integral uso te prima.".replace("te prima", dice("T'", "te prima")), NOTA("$T'$ es la temperatura que va cambiando; $T$ es un número fijo."))
    L.paso("a) Tramo 1: plantear la integral")\
        .di("Cada pedacito de temperatura produce un pedacito de largo.", F(r"dL=\alpha\,L_0\,dT'"))\
        .di("Sumamos desde te hasta dos te.", M(r"\Delta L_1=L_0\int_{T}^{2T}(\beta+\gamma T'^3)\,dT'", "i1"))\
        .por_que("La fórmula del formulario $\\Delta L=\\alpha L_0\\Delta T$ solo vale si $\\alpha$ es constante. Si cambia, se suma tramo por tramo, o sea, se integra.")
    L.paso("a) Tramo 1: resolver")\
        .di("La integral de beta es beta por te; la de te al cubo es te a la cuarta entre cuatro.", M(r"\int(\beta+\gamma T'^3)\,dT'=\beta T'+\frac{\gamma T'^4}{4}"))\
        .di("Evaluamos: arriba dos te, abajo te.", M(r"=\left[\beta(2T)+\frac{\gamma(2T)^4}{4}\right]-\left[\beta T+\frac{\gamma T^4}{4}\right]"))\
        .di("Dos a la cuarta es dieciséis: dieciséis cuartos menos un cuarto son quince cuartos.", M(r"=\beta T+\left(\frac{16}{4}-\frac{1}{4}\right)\gamma T^4=\beta T+\frac{15}{4}\gamma T^4", "r1", "azul"))\
        .despacio((r"\beta(2T)-\beta T=\beta T", "primer término"), (r"(2T)^4=16T^4", "se eleva el 2 y la T"), (r"\frac{16\gamma T^4}{4}-\frac{\gamma T^4}{4}=\frac{15}{4}\gamma T^4", "segundo término"))
    L.paso("a) Longitud a 2T")\
        .di("El largo nuevo es el inicial más lo que creció.", M(r"L_1=L_0+\Delta L_1"))\
        .di("Ponemos lo que creció, que acabamos de integrar.", M(r"L_1=L_0+L_0\left(\beta T+\frac{15}{4}\gamma T^4\right)"))\
        .di("Sacamos ele cero de factor común.".replace("ele cero", dice("L₀", "ele cero")), R(r"L_1=L_0\left(1+\beta T+\frac{15}{4}\gamma T^4\right)", "ra"))
    L.paso("b) Tramo 2: ahora se parte de L₁")\
        .di("La barra ya mide ele uno, y de ahí sigue creciendo.".replace("ele uno", dice("L₁", "ele uno")), M(r"\Delta L_2=L_1\int_{2T}^{3T}(3\beta+2\gamma T'^3)\,dT'"))\
        .di("Integramos: tres beta te, más dos gama te a la cuarta entre cuatro.", M(r"=L_1\left[3\beta T'+\frac{2\gamma T'^4}{4}\right]_{2T}^{3T}"))\
        .di("Evaluamos. Tres a la cuarta es ochenta y uno; dos a la cuarta, dieciséis.", M(r"=L_1\left[3\beta(3T-2T)+\frac{\gamma}{2}(81T^4-16T^4)\right]"))\
        .di("Ochenta y uno menos dieciséis es sesenta y cinco.", M(r"\Delta L_2=L_1\left(3\beta T+\frac{65}{2}\gamma T^4\right)", "dl2", "azul"))
    L.paso("b) Longitud a 3T")\
        .di("Sumamos a ele uno.".replace("ele uno", dice("L₁", "ele uno")), M(r"L_2=L_1\left(1+3\beta T+\frac{65}{2}\gamma T^4\right)"))\
        .di("Y cambiamos ele uno por lo del inciso a.".replace("ele uno", dice("L₁", "ele uno")), R(r"L_2=L_0\left(1+3\beta T+\frac{65}{2}\gamma T^4\right)\left(1+\beta T+\frac{15}{4}\gamma T^4\right)", "rb"))
    L.paso("b) Multiplicar los paréntesis")\
        .di("Multiplicamos cada término del primero por cada uno del segundo.", TABLA(["por", "$1$", "$\\beta T$", "$\\frac{15}{4}\\gamma T^4$"], [["$1$", "$1$", "$\\beta T$", "$\\frac{15}{4}\\gamma T^4$"], ["$3\\beta T$", "$3\\beta T$", "$3\\beta^2T^2$", "$\\frac{45}{4}\\beta\\gamma T^5$"], ["$\\frac{65}{2}\\gamma T^4$", "$\\frac{65}{2}\\gamma T^4$", "$\\frac{65}{2}\\beta\\gamma T^5$", "$\\frac{975}{8}\\gamma^2T^8$"]]))\
        .di("Juntamos los que se parecen.", M(r"\frac{L_2}{L_0}=1+4\beta T+3\beta^2T^2+\frac{145}{4}\gamma T^4+\frac{175}{4}\beta\gamma T^5+\frac{975}{8}\gamma^2T^8", "ex"))\
        .despacio((r"\beta T+3\beta T=4\beta T", ""), (r"\frac{15}{4}+\frac{65}{2}=\frac{15}{4}+\frac{130}{4}=\frac{145}{4}", "se iguala el denominador"), (r"\frac{45}{4}+\frac{65}{2}=\frac{45}{4}+\frac{130}{4}=\frac{175}{4}", ""), (r"\frac{65}{2}\cdot\frac{15}{4}=\frac{975}{8}", ""))
    L.paso("c) Coeficiente promedio")\
        .di("El promedio es el alfa constante que daría el mismo crecimiento total.", M(r"\Delta L_{\text{total}}=\bar\alpha\,L_0\,(3T-T)"), M(r"\bar\alpha=\frac{L_2-L_0}{L_0\,(2T)}", "fa", "azul"))\
        .di("Restamos el uno y dividimos todo entre dos te.", R(r"\bar\alpha=2\beta+\frac{3}{2}\beta^2T+\frac{145}{8}\gamma T^3+\frac{175}{8}\beta\gamma T^4+\frac{975}{16}\gamma^2T^7", "rc"))\
        .di("Ojo: en el folleto aparece dos beta en el segundo tramo y una suma distinta. Con el enunciado dado, sale esto.", OJO("El folleto usa $2\\beta$ y suma $\\frac{15}{4}+\\frac{65}{2}$ como $20$; la suma correcta es $\\frac{145}{4}$."))
    L.responde("a", r"L_1=L_0\left(1+\beta T+\tfrac{15}{4}\gamma T^4\right)").responde("b", r"L_2=L_1\left(1+3\beta T+\tfrac{65}{2}\gamma T^4\right)").responde("c", r"\bar\alpha=\frac{L_2-L_0}{2TL_0}")
    return L.usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ Q013
@leccion("Q013")
def q013() -> Leccion:
    al, dT = 30e-6, 30.0
    e = al * dT
    x = 0.5 * math.sqrt((1 + e) ** 2 - 1)
    L = _des("Q013", "Barra que se pandea entre dos paredes",
             "Una barra fija entre dos paredes inamovibles (separadas $L_0$), con una grieta en el centro, se pandea hacia arriba al aumentar 30 °C la temperatura. $\\alpha=30\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$. Hallar $x$, la altura del centro, en términos de $L_0$.",
             claves=["dilatación lineal", "Pitágoras", "pandeo"],
             visual={"tipo": "dilatacion", "caso": "pandeo", "L": 1.0, "a": al, "dT": dT, "y": round(x, 4)})
    L.paso("Qué nos dan y qué piden")\
        .di("Las paredes no se mueven. La barra crece, no cabe y se levanta en el centro.", DATO("paredes inamovibles"), LISTA(["Separación fija: $L_0$", "$\\Delta T=30\\ ^\\circ\\text{C}$", "$\\alpha=30\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$"]))\
        .di("Piden la altura del centro.", M(r"x=\;?\ \ \text{(en términos de } L_0)"))
    L.paso("La idea: un triángulo rectángulo")\
        .di("Media barra es la hipotenusa. La base es la mitad de la separación.", V(foco="triangulo"), M(r"\text{base}=\frac{L_0}{2}"))\
        .di("La hipotenusa es esa mitad, ya dilatada.", M(r"\text{hipotenusa}=\frac{L_0}{2}(1+\alpha\,\Delta T)"))\
        .di("Pitágoras.", M(r"\left[\frac{L_0}{2}(1+\alpha\,\Delta T)\right]^2=\left(\frac{L_0}{2}\right)^2+x^2", "pit", "azul"))\
        .por_que("La pared no deja que la base crezca, así que lo que creció se va en diagonal.")
    L.paso("Calcular αΔT")\
        .di("Primero el producto.", M(r"\alpha\,\Delta T=(30\times10^{-6})(30)=9{,}0\times10^{-4}"), M(r"1+\alpha\,\Delta T=1{,}0009", "k"))
    L.paso("Despejar x")\
        .di("Pasamos la base al otro lado.", M(r"x^2=\left(\frac{L_0}{2}\right)^2(1{,}0009)^2-\left(\frac{L_0}{2}\right)^2"))\
        .di("Factor común: ele cero medios al cuadrado.".replace("ele cero", dice("L₀", "ele cero")), M(r"x^2=\left(\frac{L_0}{2}\right)^2\left[(1{,}0009)^2-1\right]"))\
        .di("Sacamos raíz.", M(r"x=\frac{L_0}{2}\sqrt{(1{,}0009)^2-1}"))\
        .despacio((r"(1{,}0009)^2=1{,}00180081", ""), (r"1{,}00180081-1=0{,}00180081", ""), (r"\sqrt{0{,}00180081}=0{,}042436", ""))
    L.paso("Resultado")\
        .di("La raíz da cero coma cero cuarenta y dos.", M(r"x=\frac{L_0}{2}(0{,}04244)"), R(rf"x={tex(x, 3, sci='no')}\,L_0"), V(foco="altura"))\
        .di("Sube dos por ciento del largo, aunque la barra creció menos de una milésima.", NOTA("Un crecimiento pequeñito levanta mucho el centro."))
    L.responde("", rf"x={tex(x, 2, sci='no')}\,L_0", x, "L0").oficial("folleto, sección 3.1", 0.021, tol=0.03).usa(r"\Delta L=\alpha L_0\Delta T")
    return L


# ============================================================================ Q014
@leccion("Q014")
def q014() -> Leccion:
    f = 0.092
    L = _des("Q014", "Cilindro: cambio de área y de densidad",
             "Al elevar la temperatura de un cilindro metálico isotrópico de 60 °C a 100 °C, su longitud aumenta 0,092 %. a) Cambio porcentual del área transversal. b) Cambio porcentual de la densidad.",
             claves=["dilatación", "densidad", "porcentajes"],
             visual={"tipo": "dilatacion", "caso": "cilindro", "R": 2.0, "L": 10.0, "a": 0.00092 / 40, "T0": 60, "T1": 100})
    L.paso("Qué nos dan y qué piden")\
        .di("Nos dan el cambio de largo en porcentaje.", DATO("0.092"), LISTA(["$\\Delta T=100-60=40\\ ^\\circ\\text{C}$", "$\\frac{\\Delta L}{L_0}=0{,}092\\,\\%=0{,}00092$"]))\
        .di("No piden alfa: con alfa por delta T basta.".replace("alfa por delta T", dice("αΔT", "alfa por delta te")), M(r"\alpha\,\Delta T=\frac{\Delta L}{L_0}=0{,}00092", "adt", "azul"))
    L.paso("a) Cambio de área")\
        .di("Partimos de la dilatación lineal del formulario: el radio crece como uno más alfa por delta te, así que el área transversal queda al cuadrado.",
            F(r"\Delta L = \alpha L_0\,\Delta T \;\Rightarrow\; A = \pi R_0^2(1+\alpha\Delta T)^2 \approx A_0(1+2\alpha\Delta T)"),
            M(r"\Delta A = 2\alpha A_0\,\Delta T"), V(foco="calentar"))\
        .di("Dividimos entre el área inicial.", M(r"\frac{\Delta A}{A_0}=2\,\alpha\,\Delta T=2(0{,}00092)"), R(r"\frac{\Delta A}{A_0}=0{,}00184=0{,}184\,\%", "ra"))\
        .sale_de(("$2\\alpha$", "despreciando $(\\alpha\\Delta T)^2 \\ll \\alpha\\Delta T$ en $(1+\\alpha\\Delta T)^2$ desde $\\Delta L=\\alpha L_0\\Delta T$"))
    L.paso("b) Densidad: qué cambia y qué no")\
        .di("La densidad es masa entre volumen. La masa no cambia al calentar.", M(r"\rho=\frac{m}{V}"))\
        .di("Para el volumen multiplicamos área por largo desde la dilatación lineal: queda al cubo y aparece tres alfa delta te.".replace("alfa delta te", dice("αΔT", "alfa delta te")),
            M(r"V = \pi R_0^2 L_0(1+\alpha\Delta T)^3 \approx V_0(1+3\alpha\,\Delta T)"),
            M(r"V=V_0(1+3\times0{,}00092)=V_0(1{,}00276)", "v"))\
        .por_que("El metal no gana ni pierde átomos, solo se separan más; el factor $3\\alpha$ sale de expandir $(1+\\alpha\\Delta T)^3$.")
    L.paso("b) Cambio de densidad")\
        .di("La densidad nueva.", M(r"\rho=\frac{m}{V_0(1{,}00276)}=\frac{\rho_0}{1{,}00276}"))\
        .di("Uno entre uno coma cero cero dos setenta y seis.", M(rf"\frac{{\rho}}{{\rho_0}}={tex(1 / 1.00276, 6)}"))\
        .di("El cambio es lo que le falta para llegar a uno: disminuye.", M(rf"\frac{{\Delta\rho}}{{\rho_0}}={tex(1 / 1.00276, 6)}-1={tex(1 / 1.00276 - 1, 3, sci='no')}"), R(rf"\frac{{\Delta\rho}}{{\rho_0}}=-{tex(100 * (1 - 1 / 1.00276), 3)}\,\%", "rb"))\
        .di("Baja, porque el mismo metal ocupa más espacio.", OJO("El signo importa: la densidad **disminuye**."))
    L.responde("a", r"+0{,}184\,\%", 0.184, "%").responde("b", rf"-{tex(100 * (1 - 1 / 1.00276), 3)}\,\%", 100 * (1 - 1 / 1.00276), "%")
    L.oficial("folleto, sección 3.4", 0.184, 0.275).usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T")
    L.te_toca("Una lámina crece 0,10 % de largo. ¿En qué porcentaje crece su área?", 0.20, "%", pista="El área tiene dos direcciones.", solucion="$2(0{,}10\\,\\%)=0{,}20\\,\\%$")
    return L


# ============================================================================ Q015
@leccion("Q015")
def q015() -> Leccion:
    return _lata("Q015", fuente("Q015")).oficial("folleto, sección 3.4", 3.271)
