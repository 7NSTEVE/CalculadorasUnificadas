"""Superexamen Q063–Q070: radiación térmica y ley de Stefan-Boltzmann."""

from __future__ import annotations

from .ayudas import SIGMA, T0, revisar_opciones
from .banco_recomendados import _filamento
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, V
from .formato import n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q063
@leccion("Q063")
def q063() -> Leccion:
    L = _sel("Q063", "Duplicar la temperatura Kelvin de un radiador",
             "Si se duplica la temperatura Kelvin de un objeto, su potencia radiada se incrementa:",
             {"A": "dos veces.", "B": "cuatro veces.", "C": "ocho veces.", "D": "dieciséis veces."}, "D",
             claves=["Stefan-Boltzmann", "potencia cuarta"],
             visual={"tipo": "radiacion", "T": 300, "Tmax": 700, "e": 1.0, "objeto": "cuerpo"})
    L.paso("Ley de Stefan-Boltzmann")\
        .di("La potencia radiada crece con la cuarta potencia de la temperatura absoluta.", DATO("duplica la temperatura Kelvin"),
            F(r"H = A e \sigma T^4"))\
        .di("Si la temperatura se multiplica por dos, la potencia se multiplica por dos a la cuarta.",
            M(r"H' = A e \sigma (2T)^4 = 2^4\,(A e \sigma T^4) = 16\,H"),
            R(r"H' = 16\,H"))\
        .por_que("Como el enunciado ya dice temperatura en kelvin, no hay que sumar 273: el factor entra directo a la cuarta potencia.")
    revisar_opciones(L, {
        "A": ("La A sería si fuera lineal con la temperatura.", "$2^1=2$, incorrecto."),
        "B": ("La B sería al cuadrado.", "$2^2=4$, incorrecto."),
        "C": ("La C sería al cubo.", "$2^3=8$, incorrecto."),
        "D": ("La D es la correcta: dos a la cuarta es dieciséis.", "$2^4 = 16$."),
    })
    return L.oficial("II Parcial extraordinario II-2023", "D").usa(r"H=Ae\sigma T^4")


# ============================================================================ Q064
@leccion("Q064")
def q064() -> Leccion:
    L = _sel("Q064", "Emisividad de 0,30 frente a un cuerpo negro",
             "La emisividad de una superficie de cobre pulida es de $0{,}30$. Eso significa que con respecto a un cuerpo negro dicha superficie emite:",
             {"A": "un 30 % más de energía por unidad de tiempo.", "B": "un 30 % menos de energía por unidad de tiempo.",
              "C": "un 70 % más de energía por unidad de tiempo.", "D": "un 70 % menos de energía por unidad de tiempo."}, "D",
             claves=["emisividad", "cuerpo negro"],
             visual={"tipo": "radiacion", "T": 400, "e": 0.30, "objeto": "cobre"})
    L.paso("Comparar con el cuerpo negro (e = 1)")\
        .di("Un cuerpo negro ideal tiene emisividad uno, el máximo posible.", DATO("0,30"),
            F(r"H = A e \sigma T^4"))\
        .di("Con emisividad cero coma treinta, el cobre solo emite el treinta por ciento de lo que emite un cuerpo negro.",
            M(r"\frac{H_{\text{Cu}}}{H_{\text{negro}}} = \frac{0{,}30}{1} = 30\,\%"))\
        .di("Emitir treinta por ciento significa emitir setenta por ciento menos.",
            M(r"100\,\% - 30\,\% = 70\,\%\ \text{menos}"), R(r"\text{Emite } 70\,\%\ \text{menos}"),
            OJO("Cuidado con la redacción: emite **el 30 %**, es decir, **70 % menos**."))
    revisar_opciones(L, {
        "A": ("La A no: ninguna superficie emite más que un cuerpo negro.", "$e\\le 1$."),
        "B": ("La B es la trampa clásica: confunde emitir el treinta por ciento con treinta por ciento menos.", "$30\\,\\%$ del total es $70\\,\\%$ menos."),
        "C": ("La C no: no puede emitir más que el cuerpo negro.", "$e=0{,}30 < 1$."),
        "D": ("La D es la correcta: emite setenta por ciento menos.", "$1-0{,}30 = 0{,}70 = 70\\,\\%$ menos."),
    })
    return L.oficial("II Parcial ordinario II-2023", "D").usa(r"H=Ae\sigma T^4")


# ============================================================================ Q065
@leccion("Q065")
def q065() -> Leccion:
    L = _sel("Q065", "Afirmaciones sobre radiación térmica y emisividad",
             "Sobre la radiación térmica: I. Un cuerpo negro es un radiador ideal que no absorbe toda la energía que incide sobre él. II. Un buen reflector (como una superficie plateada) es un mal radiador. III. La emisividad es adimensional entre 0 y 1. ¿Cuáles son correctas?",
             {"A": "Solo II", "B": "Solo I y III", "C": "Solo II y III", "D": "Todas"}, "C",
             claves=["cuerpo negro", "reflector", "emisividad"])
    L.paso("Evaluar cada afirmación")\
        .di("Uno es falsa: un cuerpo negro sí absorbe el cien por ciento de la radiación que le llega.", DATO("no absorbe"),
            LISTA(["**I. Falsa**: el cuerpo negro absorbe **toda** la radiación incidente ($e=1$)",
                   "**II. Verdadera**: buen reflector absorbe poco y, por Kirchhoff, emite poco",
                   "**III. Verdadera**: $e$ es una razón sin unidades con $0\\le e\\le 1$"]))\
        .di("Solo la dos y la tres son verdaderas.", R(r"\text{Solo II y III}"))
    revisar_opciones(L, {
        "A": ("La A deja fuera la tres, que también es cierta.", "III es verdadera."),
        "B": ("La B incluye la uno, pero un cuerpo negro sí absorbe todo.", "I es falsa."),
        "D": ("La D incluye la uno, que contradice la definición de cuerpo negro.", "I es falsa."),
        "C": ("La C es la correcta: solo dos y tres.", "II y III verdaderas."),
    })
    return L.oficial("III Parcial ordinario II-2024", "C").usa(r"H=Ae\sigma T^4")


# ============================================================================ Q066
@leccion("Q066")
def q066() -> Leccion:
    razon = 1.0 / 0.3
    L = _sel("Q066", "Cuerpo negro frente a cobre pulido (e = 0,3)",
             "Un cuerpo negro ideal y una superficie de cobre pulido ($e\\approx 0{,}3$) tienen igual área y misma temperatura absoluta. ¿Cuál es la relación entre sus emisiones radiantes?",
             {"A": "El cuerpo negro emite menos energía porque su emisividad es 1.",
              "B": "El cobre emite más energía debido a su baja reflectividad.",
              "C": "El cuerpo negro emite aproximadamente 3,3 veces más energía que el cobre.",
              "D": "Ambos emiten igual energía porque comparten temperatura."}, "C",
             claves=["emisividad", "razón de potencias"],
             visual={"tipo": "radiacion", "T": 400, "e": 0.30, "objeto": "cobre"})
    L.paso("Potencia radiada de cada superficie")\
        .di("Partimos de la ley de Stefan-Boltzmann de la hoja.", DATO("emisividad"), F(r"H=Ae\sigma T^4"))\
        .di("El cuerpo negro tiene emisividad uno; el cobre, cero coma tres. Área y temperatura son iguales.",
            M(r"H_{\text{negro}}=A(1)\sigma T^4\qquad H_{\text{Cu}}=A(0{,}3)\sigma T^4"))
    L.paso("Razón de potencias")\
        .di("Dividimos una entre la otra: se cancelan el área, sigma y la temperatura a la cuarta.",
            M(r"\frac{H_{\text{negro}}}{H_{\text{Cu}}}=\frac{A(1)\sigma T^4}{A(0{,}3)\sigma T^4}=\frac{1}{0{,}3}"))\
        .di(f"Uno entre cero coma tres da aproximadamente {n(razon, 2)}.",
            R(rf"\frac{{H_{{\text{{negro}}}}}}{{H_{{\text{{Cu}}}}}} \approx {tex(razon, 2)}"))
    revisar_opciones(L, {
        "A": ("La A dice al revés: emisividad uno es el máximo.", "$e=1$ emite más."),
        "B": ("La B contradice los datos: el cobre pulido refleja mucho y emite poco.", "$e_{\\text{Cu}}=0{,}3 < 1$."),
        "D": ("La D olvida multiplicar por la emisividad.", "Depende de $e$."),
        "C": ("La C es la correcta: unas tres coma tres veces más.", "$1/0{,}3\\approx 3{,}3$."),
    })
    return L.oficial("III Parcial extraordinario I-2025", "C").usa(r"H=Ae\sigma T^4")

# ============================================================================ Q067
@leccion("Q067")
def q067() -> Leccion:
    L = _sel("Q067", "Recubrimiento plateado en un termo Dewar",
             "En un termo Dewar, platear las paredes reduce la emisividad de $1$ a $0{,}05$. ¿En qué proporción queda la tasa de pérdida por radiación?",
             {"A": "Se reduce a aproximadamente 5 % de la original.", "B": "Se reduce a aproximadamente 50 % de la original.",
              "C": "Se reduce a aproximadamente 95 % de la original.", "D": "No se reduce, permanece igual."}, "A",
             claves=["termo Dewar", "emisividad"],
             visual={"tipo": "radiacion", "T": 360, "e": 0.05, "objeto": "termo"})
    L.paso("Comparar antes y después de platear")\
        .di("Como el flujo es proporcional a la emisividad, la nueva pérdida es cero coma cero cinco de la original.", DATO("0,05"),
            M(r"\frac{H_{\text{plateada}}}{H_{\text{negra}}} = \frac{0{,}05}{1} = 0{,}05 = 5\,\%"),
            R(r"H_{\text{plateada}} = 5\,\%\ \text{de } H_{\text{negra}}"))\
        .di("Ojo con la preposición: se reduce a cinco por ciento, que es reducir en un noventa y cinco por ciento.",
            OJO("«Se reduce **a** 5 %» significa que queda en 5 % (baja **en** 95 %)."))
    revisar_opciones(L, {
        "B": ("La B correspondería a emisividad cero coma cinco.", "$0{,}05 \\neq 0{,}50$."),
        "C": ("La C confunde «se reduce a» con «se reduce en».", "Queda **en** 5 % de la original, no en 95 %."),
        "D": ("La D ignora que el plateado cambia la emisividad.", "La radiación sí depende de $e$."),
        "A": ("La A es la correcta: se reduce a cinco por ciento de la original.", "$0{,}05/1 = 5\\,\\%$."),
    })
    return L.oficial("III Parcial ordinario I-2025", "A").usa(r"H=Ae\sigma T^4")


# ============================================================================ Q068
@leccion("Q068")
def q068() -> Leccion:
    e, A = 0.850, 0.200
    T = 97.0 + T0
    Ts = 27.0 + T0
    pre = A * e * SIGMA
    d4 = T ** 4 - Ts ** 4
    H = pre * d4
    H_273 = e * SIGMA * A * (370.0 ** 4 - 300.0 ** 4)
    L = _sel("Q068", "Radiación neta de un recipiente a 97 °C en habitación a 27 °C",
             "En una habitación a $27{,}0\\ ^\\circ$C hay un recipiente metálico a $97{,}0\\ ^\\circ$C con $e=0{,}850$ y $A=0{,}200\\ \\text{m}^2$. El flujo neto de calor por radiación es:",
             {"A": "84,82 mW", "B": "3,817 W", "C": "102,6 W", "D": "3,319 MW"}, "C",
             claves=["radiación neta", "kelvin"],
             visual={"tipo": "radiacion", "T": 370.15, "Ts": 300.15, "e": e, "A": A, "H": round(H, 1), "objeto": "recipiente"})
    L.paso("Pasar las temperaturas a kelvin")\
        .di("En radiación es obligatorio usar kelvin antes de elevar a la cuarta. La hoja da la conversión.", DATO("97,0 °C"),
            F(r"T_K=T_C+273{,}15"), M(r"T = 97{,}0 + 273{,}15 = 370{,}15\ \text{K}"), M(r"T_s = 27{,}0 + 273{,}15 = 300{,}15\ \text{K}"),
            OJO("Nunca calcules $(97-27)^4$: cada temperatura va a la cuarta **en kelvin** y luego se restan."))
    L.paso("Calcular el flujo neto")\
        .di("Partimos de la radiación neta de la hoja.",
            F(r"H_{\text{neto}} = A e \sigma\,(T^4 - T_s^4)"),
            M(r"H_{\text{neto}} = (0{,}200)(0{,}850)(5{,}67\times 10^{-8})(370{,}15^4 - 300{,}15^4)"))\
        .di("Elevamos cada temperatura a la cuarta y restamos.", M(rf"370{{,}}15^4={tex(T ** 4, 4)}"), M(rf"300{{,}}15^4={tex(Ts ** 4, 4)}"),
            M(rf"370{{,}}15^4-300{{,}}15^4={tex(d4, 4)}"))\
        .di("Multiplicamos los tres factores de adelante.", M(rf"(0{{,}}200)(0{{,}}850)(5{{,}}67\times 10^{{-8}})={tex(pre, 4)}"))\
        .di(f"Y el producto final: {nu(H, 'W', 4)}.", M(rf"H_{{\text{{neto}}}}=({tex(pre, 4)})({tex(d4, 4)})"),
            R(rf"H_{{\text{{neto}}}} = {tex(H, 4, u='W')}"),
            NOTA(f"Con $T_0=273$ K sale ${tex(H_273, 4, u='W')}$, exacto con la opción C."))
    revisar_opciones(L, {
        "A": ("La A sale de olvidar pasar a kelvin o restar antes de elevar.", "Error de escala."),
        "B": ("La B es casi treinta veces menor.", "No coincide."),
        "D": ("La D es millones de watts, absurdo para una olla.", "Error de potencia de diez."),
        "C": ("La C es la correcta: ciento dos coma seis watts.", "Coincide con $H_{\\text{neto}}$. "),
    })
    return L.oficial("III Parcial extraordinario I-2026", "C").usa(r"H_{\text{neto}}=Ae\sigma(T^4-T_s^4)")

# ============================================================================ Q069
@leccion("Q069")
def q069() -> Leccion:
    return _filamento("Q069", fuente("Q069")).oficial("Folleto resuelto 3.3-8", 1.26e-4)


# ============================================================================ Q070
@leccion("Q070")
def q070() -> Leccion:
    a = 0.20
    A = 6.0 * a ** 2
    V_cub = a ** 3
    m = 1000.0 * V_cub
    c_agua = 4190.0
    T = 100.0 + T0
    Ts = 25.0 + T0
    H = 1.0 * SIGMA * A * (T ** 4 - Ts ** 4)
    tasa = H / (m * c_agua)
    L = _des("Q070", "Tasa de enfriamiento inicial de un cubo de agua por radiación",
             "Un recipiente cúbico de $20$ cm de arista, paredes negras ($e=1$) y masa despreciable, lleno de agua a $100\\ ^\\circ$C está en una habitación a $25\\ ^\\circ$C. Calcule la tasa de enfriamiento inicial $|dT/dt|$.",
             claves=["enfriamiento", "Stefan-Boltzmann", "derivada"],
             visual={"tipo": "radiacion", "T": 373.15, "Ts": 298.15, "e": 1.0, "A": A, "H": round(H, 1), "objeto": "cubo agua"})
    L.paso("Área superficial y masa de agua")\
        .di("Un cubo tiene seis caras cuadradas; su volumen nos da la masa de agua.", DATO("20 cm de arista"),
            M(rf"A = 6a^2 = 6(0{{,}}20)^2 = {tex(A, 3, u='m^2')}"),
            M(rf"m = \rho a^3 = (1000)(0{{,}}20)^3 = {tex(m, 3, u='kg')}"))\
        .di("Pasamos las temperaturas a kelvin.",
            M(r"T = 373{,}15\ \text{K}\qquad T_s = 298{,}15\ \text{K}"))
    L.paso("Flujo neto por radiación")\
        .di(f"Como las paredes son negras, la emisividad es uno y el flujo sale {nu(H, 'W', 4)}.",
            F(r"H_{\text{neto}} = A e \sigma\,(T^4 - T_s^4)"),
            M(rf"H_{{\text{{neto}}}} = ({tex(A, 3)})(1)(5{{,}}67\times 10^{{-8}})(373{{,}}15^4 - 298{{,}}15^4) = {tex(H, 4, u='W')}"))
    L.paso("Relacionar H con dT/dt")\
        .di("El calor perdido en cada segundo enfría el agua según eme ce de te en de te.",
            F(r"H_{\text{neto}} = -m c\frac{dT}{dt} \;\Rightarrow\; \left|\frac{dT}{dt}\right| = \frac{H_{\text{neto}}}{m c}"))\
        .di(f"Dividimos entre ocho kilos por cuatro mil ciento noventa: da {nu(tasa, 'K/s', 3)}.",
            M(rf"\left|\frac{{dT}}{{dt}}\right| = \frac{{{tex(H, 4)}}}{{(8{{,}}00)(4190)}}"),
            R(rf"\left|\frac{{dT}}{{dt}}\right| = {tex(tasa, 3, u='K/s', sci='si')}", "ra"))
    L.responde("", rf"\left|\frac{{dT}}{{dt}}\right|={tex(tasa, 3, sci='si')}\ \text{{K/s}}", tasa, "K/s")
    return L.oficial("Folleto propuesto 3.4-8", 4.66e-3).usa(r"H_{\text{neto}}=Ae\sigma(T^4-T_s^4)", r"Q=mc\Delta T")
