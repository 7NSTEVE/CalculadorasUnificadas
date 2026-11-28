"""II parcial II-2026 de Física General III (FI2103): fluidos, ordinario y extraordinario (18 lecciones).

Fuentes: `II_Examen__Ord___2S_2026_Solución.pdf` y `II_Examen__Ext___2S_2026_Solución.pdf`
(cátedra, carpeta «Documentos II - 2026»). Son las preguntas más recientes hechas con la misma hoja
del Parcial 2 que traerá la reposición. Cada renglón parte de una casilla de esa hoja
(`formularios_p1_p2.py`) y cada número se calcula aquí, en Python.

IDs: P2O26-1..9 (ordinario: 7 selecciones + 2 desarrollos) y P2E26-1..9 (extraordinario).
"""

from __future__ import annotations

import math
from typing import Tuple

from .dsl import DATO, F, LISTA, M, NOTA, OJO, OPCION, Leccion, R
from .formato import partes, tex
from .registro import leccion

G = 9.80
P_ATM = 101325.0

_ORD = ("26-II-2O", "II parcial ordinario II-2026")
_EXT = ("26-II-2E", "II parcial extraordinario II-2026")

# Casillas de la hoja del Parcial 2 escritas igual que en la hoja
BERNOULLI = r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"
CONTINUIDAD = r"A_1 v_1=A_2 v_2"
CAUDAL = r"\frac{dV}{dt}=Av"
HIDRO = r"p=p_0+\rho gh"
HIDRO_DIF = r"p_2-p_1=-\rho g(y_2-y_1)"
MAN = r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}"
EMPUJE = r"F_B=\rho g V"
STOKES = r"F_S=6\pi r\eta v"
POISEUILLE = r"\frac{dV}{dt}=\frac{(p_1-p_2)\pi R^4}{8\eta L}"
PERFIL = r"v(r)=\frac{p_1-p_2}{4\eta L}(R^2-r^2)"
TERMINAL = r"v_T=\frac{2}{9}\frac{r^2 g}{\eta}(\rho-\rho')"
REYNOLDS = r"N_R=\frac{\rho v D}{\eta}"


def _f(ex: Tuple[str, str], num: str) -> dict:
    return {"doc": ex[0], "nombre": ex[1], "num": num, "parcial": 2}


def _q(x: float, c: int = 4) -> float:
    """El mismo número que muestra tex(x, c), como float (para seguir calculando con lo que se ve)."""
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ ordinario · selecciones

@leccion("P2O26-1")
def p2o26_1() -> Leccion:
    L = Leccion(
        "P2O26-1", "Módulo de Young y límite elástico (II parcial ordinario II-2026, P1)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "1"),
        enunciado=(
            "Con un esfuerzo de tracción $\\sigma$ un material se deforma $\\varepsilon$ y al quitar el esfuerzo recupera su forma. "
            "Con $2\\sigma$ se deforma $3\\varepsilon$ y ya no recupera completamente su longitud. Esto quiere decir que:"
        ),
        opciones={
            "A": "El módulo de Young es σ/ε.",
            "B": "El módulo de Young es 2σ/3ε.",
            "C": "El límite elástico está por debajo del esfuerzo σ.",
            "D": "El límite elástico está por arriba del esfuerzo 2σ.",
        },
        correcta="A",
        claves=["módulo de Young", "límite elástico", "esfuerzo", "deformación", "selección"],
    )
    L.paso("Qué dice la hoja del módulo elástico")\
        .di("La hoja define el módulo elástico como esfuerzo entre deformación. Ese cociente solo vale en la zona elástica, donde el material vuelve a su forma.",
            DATO("recupera su forma"),
            F(r"\text{módulo elástico}=\frac{\text{esfuerzo}}{\text{deformación}}"))
    L.paso("Con sigma el material vuelve: zona elástica")\
        .di("Con sigma el material recupera su forma, así que está en la zona elástica. Ahí el módulo de Young es sigma entre épsilon.",
            M(r"Y=\frac{\sigma}{\varepsilon}"))\
        .di("Con dos sigma queda deformado: pasó el límite elástico. Por eso dos sigma entre tres épsilon no es el módulo de Young.",
            M(r"\frac{2\sigma}{3\varepsilon}\neq\frac{\sigma}{\varepsilon}"))
    L.paso("Qué opción es")\
        .di("El límite elástico quedó entre sigma y dos sigma. La única opción verdadera es la A.",
            R(r"Y=\frac{\sigma}{\varepsilon}"),
            OPCION("A", "correcta", "En la zona elástica, Y = esfuerzo/deformación = σ/ε."),
            OPCION("B", "descarta", "Con 2σ ya no es zona elástica: ese cociente no es Y."),
            OPCION("C", "descarta", "Con σ todavía recupera su forma: el límite está arriba de σ."),
            OPCION("D", "descarta", "Con 2σ ya no recupera su forma: el límite está debajo de 2σ."))
    L.usa(r"\text{módulo elástico}=\frac{\text{esfuerzo}}{\text{deformación}}")
    L.oficial("II parcial ordinario II-2026", "A")
    return L


@leccion("P2O26-2")
def p2o26_2() -> Leccion:
    L = Leccion(
        "P2O26-2", "Madera que flota en agua y luego en aceite (II parcial ordinario II-2026, P2)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "2"),
        enunciado=(
            "Un bloque de madera de densidad $600\\ \\text{kg/m}^3$ flota en agua ($1000\\ \\text{kg/m}^3$). "
            "Luego se pasa a un aceite de densidad $800\\ \\text{kg/m}^3$, donde también flota. ¿Qué ocurre en el aceite?"
        ),
        opciones={
            "A": "La fuerza de flotación disminuye y el volumen sumergido aumenta.",
            "B": "La fuerza de flotación permanece igual y el volumen sumergido aumenta.",
            "C": "La fuerza de flotación aumenta y el volumen sumergido permanece igual.",
            "D": "La fuerza de flotación permanece igual y el volumen sumergido disminuye.",
        },
        correcta="B",
        claves=["flotación", "Arquímedes", "empuje", "volumen sumergido", "selección"],
    )
    L.paso("Si flota, el empuje es igual al peso")\
        .di("Un cuerpo que flota quieto tiene el empuje igual a su peso. El peso del bloque no cambia, así que el empuje es el mismo en el agua y en el aceite.",
            DATO("también flota"),
            F(EMPUJE),
            M(r"F_B=mg"))
    L.paso("Cuánto se hunde en cada líquido")\
        .di("En el empuje, la ve es solo el volumen hundido. Escribimos la masa con la densidad de la hoja.",
            F(r"\rho=\frac{m}{V}"),
            M(r"\rho_{\text{líquido}}\,g\,V_{\text{hundido}}=\rho_{\text{madera}}\,V\,g"))\
        .di("Se cancela ge y queda la fracción hundida: densidad de la madera entre densidad del líquido.",
            M(r"\frac{V_{\text{hundido}}}{V}=\frac{\rho_{\text{madera}}}{\rho_{\text{líquido}}}"),
            M(r"\text{agua: }\frac{600}{1000}=0{,}600"),
            M(r"\text{aceite: }\frac{600}{800}=0{,}750"))
    L.paso("Qué opción es")\
        .di("En el aceite se hunde el setenta y cinco por ciento en vez del sesenta: el volumen hundido aumenta y el empuje sigue igual.",
            R(r"F_B\ \text{igual},\quad V_{\text{hundido}}\ \text{aumenta}"),
            OPCION("A", "descarta", "El empuje no baja: sigue siendo igual al peso."),
            OPCION("B", "correcta", "Empuje = peso en los dos; en el aceite se hunde 75 % en vez de 60 %."),
            OPCION("C", "descarta", "El empuje no sube y el volumen hundido sí cambia."),
            OPCION("D", "descarta", "Con un líquido menos denso se hunde más, no menos."))
    L.usa(EMPUJE, r"\rho=\frac{m}{V}")
    L.oficial("II parcial ordinario II-2026", "B")
    return L


@leccion("P2O26-3")
def p2o26_3() -> Leccion:
    L = Leccion(
        "P2O26-3", "Hipótesis del fluido ideal (II parcial ordinario II-2026, P3)",
        tema="fluidos", tipo="seleccion", nivel=1, parcial=2, fuente=_f(_ORD, "3"),
        enunciado=(
            "¿Cuáles corresponden a las hipótesis de un fluido ideal? "
            "I. Su densidad permanece constante, aunque cambie la presión. "
            "II. En cada punto del fluido, sus propiedades permanecen constantes con el tiempo. "
            "III. Las partículas pueden presentar rotación local, pero el fluido globalmente no debe rotar."
        ),
        opciones={"A": "I y II.", "B": "II y III.", "C": "I y III.", "D": "I, II y III."},
        correcta="A",
        claves=["fluido ideal", "incompresible", "flujo estable", "irrotacional", "selección"],
    )
    L.paso("Las cuatro condiciones del fluido ideal")\
        .di("Bernoulli, de la hoja, solo vale para un fluido ideal. Ese fluido cumple cuatro condiciones.",
            F(BERNOULLI),
            LISTA([
                "Incompresible: la densidad no cambia.",
                "Estable: en cada punto, nada cambia con el tiempo.",
                "No viscoso: no hay roce entre capas.",
                "Irrotacional: ninguna partícula gira sobre sí misma.",
            ]))
    L.paso("Revisar cada afirmación")\
        .di("La uno es incompresible: sí. La dos es flujo estable: sí. La tres permite que las partículas giren, y eso contradice irrotacional: no.",
            M(r"\text{I: sí}\qquad\text{II: sí}\qquad\text{III: no}"),
            R(r"\text{I y II}"),
            OPCION("A", "correcta", "Incompresible y estable son hipótesis del fluido ideal."),
            OPCION("B", "descarta", "La III permite rotación local: el fluido ideal es irrotacional."),
            OPCION("C", "descarta", "La III es falsa."),
            OPCION("D", "descarta", "La III es falsa."))
    L.usa(BERNOULLI)
    L.oficial("II parcial ordinario II-2026", "A")
    return L


@leccion("P2O26-4")
def p2o26_4() -> Leccion:
    d1, d2, v1 = 6.0, 3.0, 2.0
    razon = (d1 / d2) ** 2
    v2 = v1 * razon
    L = Leccion(
        "P2O26-4", "Tubería que se angosta de 6 cm a 3 cm (II parcial ordinario II-2026, P4)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "4"),
        enunciado=(
            "Por una tubería horizontal circula agua en régimen estable. Donde el diámetro interior es $6{,}0\\ \\text{cm}$ "
            "la rapidez es $2{,}0\\ \\text{m/s}$. Más adelante el diámetro baja a $3{,}0\\ \\text{cm}$. "
            "Si el agua es incompresible, la rapidez en la sección angosta es:"
        ),
        opciones={"A": "1,0 m/s", "B": "4,0 m/s", "C": "8,0 m/s", "D": "16,0 m/s"},
        correcta="C",
        claves=["continuidad", "caudal", "diámetro", "selección"],
    )
    L.paso("Continuidad: lo que entra es lo que sale")\
        .di("El mismo caudal pasa por las dos secciones. Usamos la continuidad de la hoja y despejamos ve dos.",
            DATO("3,0 cm"),
            F(CONTINUIDAD),
            M(r"v_2=v_1\,\frac{A_1}{A_2}"))
    L.paso("La razón de áreas sale del diámetro al cuadrado")\
        .di("El área de un círculo es pi por radio al cuadrado. Pi se cancela y el radio es medio diámetro, así que queda el cociente de diámetros al cuadrado.",
            M(r"\frac{A_1}{A_2}=\frac{\pi r_1^2}{\pi r_2^2}=\left(\frac{d_1}{d_2}\right)^2"),
            M(rf"\left(\frac{{6{{,}}0}}{{3{{,}}0}}\right)^2={tex(razon, 2)}"))
    L.paso("Sustituir y elegir")\
        .di("La mitad del diámetro da la cuarta parte del área, así que la rapidez se multiplica por cuatro.",
            M(rf"v_2=(2{{,}}0)({tex(razon, 2)})={tex(v2, 2)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 2)}\ \text{{m/s}}"),
            OPCION("A", "descarta", "En lo angosto el agua va más rápido, no más lento."),
            OPCION("B", "descarta", "Usa la razón de diámetros sin elevar al cuadrado."),
            OPCION("C", "correcta", "2,0 × (6/3)² = 8,0 m/s."),
            OPCION("D", "descarta", "Eleva a la cuarta en vez de al cuadrado."))
    L.usa(CONTINUIDAD)
    L.responde("v2", rf"v_2={tex(v2, 2)}\ \text{{m/s}}", v2, "m/s")
    L.oficial("II parcial ordinario II-2026", "C")
    return L


@leccion("P2O26-5")
def p2o26_5() -> Leccion:
    L = Leccion(
        "P2O26-5", "Tubería horizontal que se angosta: rapidez y presión (II parcial ordinario II-2026, P5)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "5"),
        enunciado=(
            "Un líquido ideal circula en régimen estable por una tubería horizontal cuya sección disminuye. "
            "En el punto 1 (sección ancha) la rapidez es $v_1$ y en el punto 2 (sección angosta) es $v_2$. ¿Qué ocurre?"
        ),
        opciones={"A": "v₂ > v₁ y p₂ < p₁.", "B": "v₂ > v₁ y p₂ > p₁.", "C": "v₂ < v₁ y p₂ < p₁.", "D": "v₂ = v₁ y p₂ = p₁."},
        correcta="A",
        claves=["continuidad", "Bernoulli", "presión", "rapidez", "selección"],
    )
    L.paso("Rapidez: continuidad")\
        .di("Con la continuidad de la hoja, si el área dos es menor que el área uno, la rapidez dos tiene que ser mayor.",
            F(CONTINUIDAD),
            M(r"A_2<A_1\;\Rightarrow\;v_2=v_1\frac{A_1}{A_2}>v_1"))
    L.paso("Presión: Bernoulli a la misma altura")\
        .di("La tubería es horizontal: ye uno es igual a ye dos y esos términos se cancelan en Bernoulli.",
            F(BERNOULLI),
            M(r"p_1+\frac{1}{2}\rho v_1^2=p_2+\frac{1}{2}\rho v_2^2"))\
        .di("Pasamos pe dos a la izquierda y un medio de ro ve uno al cuadrado a la derecha.",
            M(r"p_1-p_2=\frac{1}{2}\rho v_2^2-\frac{1}{2}\rho v_1^2"),
            M(r"p_1-p_2=\frac{1}{2}\rho\left(v_2^2-v_1^2\right)"))\
        .di("Como ve dos es mayor, el paréntesis es positivo: la presión baja donde el líquido va más rápido.",
            M(r"v_2>v_1\;\Rightarrow\;p_1-p_2>0"),
            R(r"v_2>v_1\quad\text{y}\quad p_2<p_1"),
            OPCION("A", "correcta", "Más rápido en lo angosto y, por Bernoulli, menos presión."),
            OPCION("B", "descarta", "Si sube la rapidez, la presión baja (misma altura)."),
            OPCION("C", "descarta", "En lo angosto el líquido va más rápido."),
            OPCION("D", "descarta", "Las áreas son distintas: la rapidez no puede ser igual."))
    L.usa(CONTINUIDAD, BERNOULLI)
    L.oficial("II parcial ordinario II-2026", "A")
    return L


@leccion("P2O26-6")
def p2o26_6() -> Leccion:
    L = Leccion(
        "P2O26-6", "Esfera con rapidez terminal en un líquido viscoso (II parcial ordinario II-2026, P6)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "6"),
        enunciado=(
            "Una esfera pequeña cae verticalmente en un líquido viscoso y alcanza su velocidad terminal (vale la ley de Stokes). "
            "En ese instante, ¿qué relación entre las fuerzas es correcta?"
        ),
        opciones={
            "A": "El peso es igual únicamente a la fuerza viscosa.",
            "B": "La fuerza de flotación es igual a la suma del peso y la fuerza viscosa.",
            "C": "La fuerza viscosa es mayor que el peso, por lo que la aceleración es nula.",
            "D": "El peso es igual a la suma de la fuerza de flotación y la fuerza viscosa.",
        },
        correcta="D",
        claves=["Stokes", "velocidad terminal", "empuje", "selección"],
    )
    L.paso("Qué fuerzas actúan")\
        .di("Hacia abajo actúa el peso. Hacia arriba actúan el empuje y la fuerza viscosa de Stokes, porque la esfera baja.",
            DATO("velocidad terminal"),
            F(EMPUJE),
            F(STOKES))
    L.paso("Velocidad terminal: aceleración cero")\
        .di("Con velocidad constante, la aceleración es cero y las fuerzas se equilibran: lo de abajo igual a lo de arriba.",
            M(r"mg=F_B+F_S"),
            R(r"\text{peso}=F_B+F_S"),
            OPCION("A", "descarta", "Olvida el empuje, que siempre está en un líquido."),
            OPCION("B", "descarta", "El empuje y la fuerza viscosa van hacia arriba, el peso hacia abajo."),
            OPCION("C", "descarta", "Si una fuerza fuera mayor, habría aceleración."),
            OPCION("D", "correcta", "Abajo: peso. Arriba: empuje + viscosa. Se equilibran."))
    L.usa(EMPUJE, STOKES)
    L.oficial("II parcial ordinario II-2026", "D")
    return L


@leccion("P2O26-7")
def p2o26_7() -> Leccion:
    V0 = 1.5  # L
    B = 5460e6
    rho, h = 1025.0, 2000.0
    dp = rho * G * h
    dv = V0 * dp / B
    L = Leccion(
        "P2O26-7", "Cuerpo a 2000 m de profundidad: cuánto se comprime (II parcial ordinario II-2026, P7)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_ORD, "7"),
        enunciado=(
            "Un cuerpo de $1{,}5\\ \\text{L}$ está a presión atmosférica y tiene módulo de compresibilidad $5460\\ \\text{MPa}$. "
            "Se sumerge a $2000\\ \\text{m}$ en agua de mar ($\\rho=1025\\ \\text{kg/m}^3$). El cambio de volumen del cuerpo será:"
        ),
        opciones={"A": "0,552 L", "B": "0,0552 L", "C": "0,00552 L", "D": "0,000552 L"},
        correcta="C",
        claves=["módulo de volumen", "compresibilidad", "presión hidrostática", "selección"],
    )
    L.paso("Cuánta presión de más hay a 2000 m")\
        .di("La presión extra sobre el cuerpo es lo que el agua agrega sobre la presión atmosférica. Sale de la hidrostática de la hoja.",
            DATO("2000 m"),
            F(HIDRO),
            M(r"\Delta p=p-p_0=\rho g h"),
            M(rf"\Delta p=(1025)(9{{,}}80)(2000)={tex(dp, 4)}\ \text{{Pa}}"))
    L.paso("Módulo de volumen: despejar el cambio de volumen")\
        .di("En la casilla del módulo de volumen, la fuerza por área es esa presión extra. Despejamos delta ve.",
            F(r"B=-\frac{F_\perp/A}{\Delta V/V_0}"),
            M(r"\frac{F_\perp}{A}=\Delta p\;\Rightarrow\;\Delta V=-\frac{V_0\,\Delta p}{B}"))\
        .di("Dejamos ve cero en litros: el cociente de presiones no tiene unidades, así que delta ve sale en litros.",
            M(rf"\Delta V=-\frac{{(1{{,}}5)({tex(dp, 4)})}}{{5460\times 10^6}}=-{tex(dv, 3)}\ \text{{L}}"))
    L.paso("Qué opción es")\
        .di("El signo menos dice que el volumen baja. Su tamaño es cero coma cero cero cinco cinco dos litros.",
            R(rf"|\Delta V|={tex(dv, 3)}\ \text{{L}}"),
            OPCION("A", "descarta", "Cien veces más grande: error de potencias de diez."),
            OPCION("B", "descarta", "Diez veces más grande."),
            OPCION("C", "correcta", "1,5 × 2,009×10⁷ / 5,46×10⁹ = 0,00552 L."),
            OPCION("D", "descarta", "Diez veces más pequeño."))
    L.usa(HIDRO, r"B=-\frac{F_\perp/A}{\Delta V/V_0}")
    L.responde("dV", rf"|\Delta V|={tex(dv, 3)}\ \text{{L}}", dv, "L")
    L.oficial("II parcial ordinario II-2026", "C")
    return L


# ============================================================================ ordinario · desarrollos

@leccion("P2O26-8")
def p2o26_8() -> Leccion:
    pA = 64000.0
    yA, y1, y2, y3, yX, yB, yC = 2.40, 0.850, 1.35, 0.700, 2.65, 0.200, 1.80
    r_w, r_hg, r_gl = 1000.0, 13600.0, 1260.0
    d1 = r_w * G * (yA - y1)          # 15190
    p1 = pA + d1                       # 79190
    d2 = r_hg * G * (y2 - y1)          # 66640
    p2 = p1 - d2                       # 12550
    d3 = r_gl * G * (y2 - y3)          # 8026,2
    p3 = p2 + d3                       # 20576,2
    altura_x = yX - y3                 # 1,95
    g_alt = G * altura_x               # 19,11
    rho_x = p3 / g_alt                 # 1076,7
    dB = r_hg * G * (y1 - yB)          # 86632
    pB = p1 + dB                       # 165822
    pB_abs = pB + P_ATM                # 267147
    dC = r_gl * G * (yC - y2)          # 5556,6
    pC = p2 - dC                       # 6993,4

    L = Leccion(
        "P2O26-8", "Manómetro con agua, mercurio, glicerina y un líquido X (II parcial ordinario II-2026, P8)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(_ORD, "8"),
        enunciado=(
            "Un sistema de líquidos inmiscibles en reposo: en el depósito A (agua y aire) la presión manométrica en el punto A, "
            "a $y_A=2{,}40\\ \\text{m}$, es $64{,}0\\ \\text{kPa}$. El agua toca el mercurio a $0{,}850\\ \\text{m}$; "
            "en otra rama el mercurio toca la glicerina a $1{,}35\\ \\text{m}$; la glicerina toca un líquido X a $0{,}700\\ \\text{m}$; "
            "X está abierto a la atmósfera con su superficie a $2{,}65\\ \\text{m}$. "
            "($\\rho_{\\text{agua}}=1000$, $\\rho_{\\text{Hg}}=13\\,600$, $\\rho_{\\text{gli}}=1260\\ \\text{kg/m}^3$.) "
            "a) Presiones manométricas en las interfaces agua-mercurio y mercurio-glicerina. b) Densidad de X. "
            "c) Presión absoluta en B (mercurio, $y_B=0{,}200\\ \\text{m}$). d) Presión manométrica en C (glicerina, $y_C=1{,}80\\ \\text{m}$)."
        ),
        claves=["manómetro", "presión hidrostática", "presión manométrica", "líquidos inmiscibles"],
    )
    L.paso("La regla de oro de la hoja")\
        .di("Dentro de un mismo líquido en reposo, la presión cambia solo con la altura. Bajar suma presión y subir la resta.",
            DATO("64,0 kPa"),
            F(HIDRO_DIF),
            NOTA("Solo se resta altura dentro del mismo líquido. Al cruzar una interfaz, la presión es la misma de los dos lados."))\
        .di("Vamos de interfaz en interfaz, con presiones manométricas: en la superficie abierta de X la manométrica es cero.",
            LISTA([
                r"$y_A=2{,}40$ · agua/Hg $0{,}850$ · Hg/glicerina $1{,}35$ · glicerina/X $0{,}700$ · superficie de X $2{,}65$ (m)",
                r"$\rho_{\text{agua}}=1000$ · $\rho_{\text{Hg}}=13\,600$ · $\rho_{\text{gli}}=1260\ \text{kg/m}^3$",
            ]))
    L.paso("a) Interfaz agua-mercurio: bajar 1,55 m en agua")\
        .di("Desde A bajamos en el agua de dos coma cuarenta a cero coma ochocientos cincuenta metros. La diferencia de alturas es uno coma cincuenta y cinco.",
            M(r"p_{\text{agua/Hg}}=p_A+\rho_{\text{agua}}\,g\,(y_A-y_{\text{agua/Hg}})"),
            M(r"y_A-y_{\text{agua/Hg}}=2{,}40-0{,}850=1{,}55\ \text{m}"),
            M(rf"\rho_{{\text{{agua}}}}\,g\,(1{{,}}55)=(1000)(9{{,}}80)(1{{,}}55)={tex(d1, 5)}\ \text{{Pa}}"))\
        .di("Multiplicamos los kilopascales por mil para sumar ambas presiones en pascales.",
            M(r"p_A=64{,}0\ \text{kPa}\frac{1000\ \text{Pa}}{1\ \text{kPa}}=64\,000\ \text{Pa}", "presion-a-pascales"),
            M(rf"p_{{\text{{agua/Hg}}}}=64\,000+{tex(d1, 5)}={tex(p1, 5)}\ \text{{Pa}}"),
            R(rf"p_{{\text{{agua/Hg}}}}={tex(p1, 5)}\ \text{{Pa}}"))
    L.paso("a) Interfaz mercurio-glicerina: subir 0,50 m en mercurio")\
        .di("En el mercurio subimos de cero coma ochocientos cincuenta a uno coma treinta y cinco metros. La altura final menos la inicial da cero coma quinientos metros. Subir resta presión.",
            M(r"p_{\text{Hg/gli}}=p_{\text{agua/Hg}}-\rho_{\text{Hg}}\,g\,(y_{\text{Hg/gli}}-y_{\text{agua/Hg}})"),
            M(r"\Delta h_{\text{Hg}}=y_{\text{Hg/gli}}-y_{\text{agua/Hg}}=1{,}35\ \text{m}-0{,}850\ \text{m}=0{,}500\ \text{m}", "subida-mercurio"),
            M(rf"\rho_{{\text{{Hg}}}}\,g\,(1{{,}}35-0{{,}}850)=(13\,600)(9{{,}}80)(0{{,}}50)={tex(d2, 5)}\ \text{{Pa}}"),
            M(rf"p_{{\text{{Hg/gli}}}}={tex(p1, 5)}-{tex(d2, 5)}={tex(p2, 5)}\ \text{{Pa}}"),
            R(rf"p_{{\text{{Hg/gli}}}}={tex(p2, 5)}\ \text{{Pa}}"))
    L.paso("b) Interfaz glicerina-X: bajar 0,65 m en glicerina")\
        .di("La glicerina sube y vuelve a bajar, pero solo importa la diferencia de alturas: de uno coma treinta y cinco a cero coma setecientos.",
            M(r"p_{\text{gli/X}}=p_{\text{Hg/gli}}+\rho_{\text{gli}}\,g\,(y_{\text{Hg/gli}}-y_{\text{gli/X}})"),
            M(rf"\rho_{{\text{{gli}}}}\,g\,(1{{,}}35-0{{,}}700)=(1260)(9{{,}}80)(0{{,}}65)={tex(d3, 5)}\ \text{{Pa}}"),
            M(rf"p_{{\text{{gli/X}}}}={tex(p2, 5)}+{tex(d3, 5)}={tex(p3, 6)}\ \text{{Pa}}"))
    L.paso("b) Columna de X hasta la superficie abierta")\
        .di("De la interfaz subimos en X hasta la superficie, donde la manométrica es cero. Subir uno coma noventa y cinco metros resta ro equis ge por esa altura.",
            M(r"0=p_{\text{gli/X}}-\rho_X\,g\,(2{,}65-0{,}700)"),
            M(r"\rho_X=\frac{p_{\text{gli/X}}}{g\,(1{,}95)}"),
            M(rf"\rho_X=\frac{{{tex(p3, 6)}}}{{(9{{,}}80)(1{{,}}95)}}=\frac{{{tex(p3, 6)}}}{{{tex(g_alt, 4)}}}={tex(rho_x, 5)}\ \text{{kg/m}}^3"),
            R(rf"\rho_X\approx {tex(rho_x, 4)}\ \text{{kg/m}}^3"))
    L.paso("c) Punto B, dentro del mercurio")\
        .di("B está en el mercurio, cero coma sesenta y cinco metros por debajo de la interfaz con el agua. Bajar suma presión.",
            M(rf"p_{{B,\text{{man}}}}=p_{{\text{{agua/Hg}}}}+\rho_{{\text{{Hg}}}}\,g\,(0{{,}}850-0{{,}}200)"),
            M(rf"\rho_{{\text{{Hg}}}}\,g\,(0{{,}}650)=(13\,600)(9{{,}}80)(0{{,}}650)={tex(dB, 5)}\ \text{{Pa}}"),
            M(rf"p_{{B,\text{{man}}}}={tex(p1, 5)}+{tex(dB, 5)}={tex(pB, 6)}\ \text{{Pa}}"))\
        .di("La absoluta es la manométrica más la atmosférica, según la casilla de presión manométrica.",
            F(MAN),
            M(rf"p_{{B,\text{{abs}}}}={tex(pB, 6)}+101\,325={tex(pB_abs, 6)}\ \text{{Pa}}"),
            R(rf"p_{{B,\text{{abs}}}}\approx {tex(pB_abs / 1000.0, 4)}\ \text{{kPa}}"))
    L.paso("d) Punto C, en la glicerina")\
        .di("C está en la glicerina, cero coma cuarenta y cinco metros por encima de la interfaz con el mercurio. Subir resta presión.",
            M(rf"p_{{C}}=p_{{\text{{Hg/gli}}}}-\rho_{{\text{{gli}}}}\,g\,(1{{,}}80-1{{,}}35)"),
            M(rf"\rho_{{\text{{gli}}}}\,g\,(0{{,}}45)=(1260)(9{{,}}80)(0{{,}}45)={tex(dC, 5)}\ \text{{Pa}}"),
            M(rf"p_{{C}}={tex(p2, 5)}-{tex(dC, 5)}={tex(pC, 5)}\ \text{{Pa}}"),
            R(rf"p_{{C,\text{{man}}}}\approx {tex(pC / 1000.0, 3)}\ \text{{kPa}}"))
    L.usa(HIDRO_DIF, MAN)
    L.responde("a-agua/Hg", rf"p={tex(p1, 5)}\ \text{{Pa}}", p1, "Pa")
    L.responde("a-Hg/gli", rf"p={tex(p2, 5)}\ \text{{Pa}}", p2, "Pa")
    L.responde("b", rf"\rho_X={tex(rho_x, 4)}\ \text{{kg/m}}^3", rho_x, "kg/m^3")
    L.responde("c", rf"p_{{B,\text{{abs}}}}={tex(pB_abs, 6)}\ \text{{Pa}}", pB_abs, "Pa")
    L.responde("d", rf"p_{{C}}={tex(pC, 5)}\ \text{{Pa}}", pC, "Pa")
    L.oficial("II parcial ordinario II-2026", 79190, 12550, 1077, 267147, 6993.4, tol=0.01)
    L.te_toca("Si el punto A estuviera a 2,90 m en vez de 2,40 m (misma presión), ¿cuánto valdría la presión en la interfaz agua-mercurio?",
              pA + r_w * G * (2.90 - y1), "Pa", tol=0.01,
              pista="Ahora se bajan 2,05 m en el agua.", solucion="64 000 + (1000)(9,80)(2,05) = 84 090 Pa.")
    return L


@leccion("P2O26-9")
def p2o26_9() -> Leccion:
    rho, eta = 1000.0, 0.00100
    v1, d1, L1 = 0.200, 0.00860, 6.00
    d2, L2 = 0.00890, 10.00
    NR = rho * v1 * d1 / eta
    A1 = math.pi * d1 ** 2 / 4.0
    Q1 = A1 * v1
    R1 = d1 / 2.0
    R2 = d2 / 2.0
    Q1v = _q(Q1)
    num = 8.0 * eta * L1 * Q1v
    den = math.pi * R1 ** 4
    dp = num / den
    dpv = _q(dp)
    v2 = dpv * R2 ** 2 / (8.0 * eta * L2)

    L = Leccion(
        "P2O26-9", "Fuente con dos tuberías: Reynolds y Poiseuille (II parcial ordinario II-2026, P9)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(_ORD, "9"),
        enunciado=(
            "Una bomba sube la presión manométrica del agua a $P$ y divide el flujo en dos tuberías horizontales que salen al aire: "
            "la principal ($d_1=0{,}860\\ \\text{cm}$, $L_1=6{,}00\\ \\text{m}$) y la secundaria ($d_2=0{,}890\\ \\text{cm}$, $L_2=10{,}00\\ \\text{m}$). "
            "En la principal $v_1=0{,}200\\ \\text{m/s}$; $\\rho=1000\\ \\text{kg/m}^3$ y $\\eta=0{,}00100\\ \\text{Pa}\\cdot\\text{s}$. "
            "a) Número de Reynolds de la principal: ¿laminar o turbulento? b) Caudal de la principal. "
            "c) Diferencia de presión entre los extremos de la principal. d) Rapidez media $v_2$ en la secundaria."
        ),
        claves=["Reynolds", "Poiseuille", "caudal", "flujo viscoso", "laminar"],
    )
    L.paso("a) Número de Reynolds")\
        .di("Usamos la casilla de Reynolds con el diámetro de la tubería principal en metros.",
            DATO("0,860 cm"),
            F(REYNOLDS),
            M(rf"N_R=\frac{{(1000)(0{{,}}200)(0{{,}}00860)}}{{0{{,}}00100}}={tex(NR, 4)}"))\
        .di("Mil setecientos veinte es menor que dos mil, así que el flujo es laminar y vale Poiseuille.",
            R(rf"N_R={tex(NR, 4)}<2000\;\Rightarrow\;\text{{laminar}}"))
    L.paso("b) Caudal de la tubería principal")\
        .di("El área es pi por el diámetro al cuadrado entre cuatro, porque el radio es medio diámetro. Después multiplicamos por la rapidez.",
            F(CAUDAL),
            M(rf"A_1=\frac{{\pi d_1^2}}{{4}}=\frac{{\pi(0{{,}}00860)^2}}{{4}}={tex(A1, 4)}\ \text{{m}}^2"),
            M(rf"\frac{{dV}}{{dt}}=({tex(A1, 4)})(0{{,}}200)={tex(Q1, 4)}\ \text{{m}}^3/\text{{s}}"),
            R(rf"\frac{{dV}}{{dt}}={tex(Q1, 4)}\ \text{{m}}^3/\text{{s}}"))
    L.paso("c) Caída de presión: Poiseuille despejada")\
        .di("La casilla de Poiseuille relaciona el caudal con la diferencia de presión. Despejamos la diferencia de presión.",
            F(POISEUILLE),
            M(r"p_1-p_2=\frac{8\eta L\,(dV/dt)}{\pi R^4}"))\
        .di("El radio es la mitad de cero coma ochocientos sesenta centímetros: cuatro coma treinta milímetros.",
            M(r"R_1=\frac{0{,}00860}{2}=4{,}30\times 10^{-3}\ \text{m}"),
            M(rf"p_1-p_2=\frac{{8(0{{,}}00100)(6{{,}}00)({tex(Q1, 4)})}}{{\pi(4{{,}}30\times 10^{{-3}})^4}}={tex(dp, 4)}\ \text{{Pa}}"),
            R(rf"\Delta p_1={tex(dp, 4)}\ \text{{Pa}}"),
            NOTA("La solución oficial usa la forma equivalente Δp = 32ηLv/d² y obtiene 519,2 Pa."))
    L.paso("d) Rapidez en la tubería secundaria")\
        .di("Las dos tuberías arrancan en la misma bomba y salen al aire a la misma altura: tienen la misma caída de presión.",
            M(rf"\Delta p_2=\Delta p_1={tex(dpv, 4)}\ \text{{Pa}}"))\
        .di("En Poiseuille cambiamos el caudal por área por rapidez. Pi y un erre al cuadrado se cancelan.",
            M(r"\pi R_2^2\,v_2=\frac{\Delta p\,\pi R_2^4}{8\eta L_2}\;\Rightarrow\;v_2=\frac{\Delta p\,R_2^2}{8\eta L_2}"),
            M(rf"v_2=\frac{{({tex(dpv, 4)})(4{{,}}45\times 10^{{-3}})^2}}{{8(0{{,}}00100)(10{{,}}00)}}={tex(v2, 4)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 4)}\ \text{{m/s}}"))
    L.usa(REYNOLDS, CAUDAL, POISEUILLE)
    L.responde("a", rf"N_R={tex(NR, 4)}", NR, "")
    L.responde("b", rf"\frac{{dV}}{{dt}}={tex(Q1, 4)}\ \text{{m}}^3/\text{{s}}", Q1, "m^3/s")
    L.responde("c", rf"\Delta p={tex(dp, 4)}\ \text{{Pa}}", dp, "Pa")
    L.responde("d", rf"v_2={tex(v2, 4)}\ \text{{m/s}}", v2, "m/s")
    L.oficial("II parcial ordinario II-2026", 1720, 1.162e-5, 519.2, 0.1285, tol=0.01)
    L.te_toca("Si la tubería principal midiera 12,0 m (mismo caudal), ¿cuál sería la caída de presión?",
              2.0 * dp, "Pa", tol=0.01, pista="En Poiseuille, Δp es proporcional a L.", solucion="El doble: 1038 Pa.")
    return L


# ============================================================================ extraordinario · selecciones

@leccion("P2E26-1")
def p2e26_1() -> Leccion:
    L = Leccion(
        "P2E26-1", "Escultura sacada del mar: tensión del cable (II parcial extraordinario II-2026, P1)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "1"),
        enunciado=(
            "Una escultura de mármol de masa $m$ se saca verticalmente del fondo del mar, a velocidad constante, "
            "mientras sigue sumergida. ¿Cuál es la tensión $T$ del cable en términos del empuje $F_B$, $m$ y $g$?"
        ),
        opciones={"A": "T = −F_B − mg", "B": "T = −F_B + mg", "C": "T = F_B − mg", "D": "T = F_B + mg"},
        correcta="B",
        claves=["empuje", "tensión", "equilibrio", "Arquímedes", "selección"],
    )
    L.paso("Velocidad constante: fuerzas en equilibrio")\
        .di("Si sube a velocidad constante, la aceleración es cero. Hacia arriba tiran el cable y el empuje; hacia abajo, el peso.",
            DATO("velocidad constante"),
            F(EMPUJE),
            M(r"T+F_B-mg=0"))
    L.paso("Despejar la tensión")\
        .di("Pasamos el empuje restando al otro lado. El cable solo tiene que poner lo que el empuje no alcanza.",
            M(r"T=-F_B+mg"),
            R(r"T=mg-F_B"),
            OPCION("A", "descarta", "Daría una tensión negativa: el cable no empuja."),
            OPCION("B", "correcta", "T + F_B = mg, entonces T = −F_B + mg."),
            OPCION("C", "descarta", "Tiene los signos al revés: sería negativa porque el mármol es más denso que el agua."),
            OPCION("D", "descarta", "El empuje ayuda a subir: se resta, no se suma."))
    L.usa(EMPUJE)
    L.oficial("II parcial extraordinario II-2026", "B")
    return L


@leccion("P2E26-2")
def p2e26_2() -> Leccion:
    L = Leccion(
        "P2E26-2", "Tensión superficial con el doble de γ (II parcial extraordinario II-2026, P2)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "2"),
        enunciado=(
            "Con un marco de alambre de lado móvil $l$ se mide la tensión superficial. Para el líquido A se necesita una fuerza $F$. "
            "Si el líquido B tiene el doble de tensión superficial ($\\gamma_B=2\\gamma_A$), ¿qué fuerza mantiene en equilibrio el mismo alambre?"
        ),
        opciones={"A": "F/4", "B": "F/2", "C": "4F", "D": "2F"},
        correcta="D",
        claves=["tensión superficial", "casilla nueva", "proporcionalidad", "selección"],
        aviso="Casilla nueva del II-2026: γ = F/(2l) nunca estuvo en una hoja de reposición.",
    )
    L.paso("La casilla de tensión superficial")\
        .di("La hoja define la tensión superficial como la fuerza entre dos veces el largo del alambre. El dos aparece porque la película tiene dos caras.",
            DATO("doble de tensión superficial"),
            F(r"\gamma=\frac{F}{2l}"),
            M(r"F=2\gamma l"))
    L.paso("Mismo alambre, el doble de γ")\
        .di("El largo ele no cambia. Si gama se duplica, la fuerza se duplica.",
            M(r"F_A=2\gamma_A l"),
            M(r"F_B=2\gamma_B l=2(2\gamma_A)l=2F_A"),
            R(r"F_B=2F"),
            OPCION("A", "descarta", "La fuerza crece con γ, no baja."),
            OPCION("B", "descarta", "La fuerza crece con γ, no baja."),
            OPCION("C", "descarta", "γ aparece a la primera potencia, no al cuadrado."),
            OPCION("D", "correcta", "F = 2γl: el doble de γ da el doble de fuerza."))
    L.usa(r"\gamma=\frac{F}{2l}")
    L.oficial("II parcial extraordinario II-2026", "D")
    return L


@leccion("P2E26-3")
def p2e26_3() -> Leccion:
    L = Leccion(
        "P2E26-3", "Tanque con orificio: de qué depende la rapidez de salida (II parcial extraordinario II-2026, P3)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "3"),
        enunciado=(
            "Un tanque abierto tiene un orificio en el fondo. El área del tanque es mucho mayor que la del orificio y ambos están "
            "a presión atmosférica. ¿De qué depende la rapidez de salida?"
        ),
        opciones={
            "A": "Área del tanque.",
            "B": "Densidad del líquido.",
            "C": "Valor de la presión atmosférica.",
            "D": "Altura del líquido sobre el orificio (profundidad de la columna).",
        },
        correcta="D",
        claves=["Torricelli", "Bernoulli", "tanque", "selección"],
    )
    L.paso("Bernoulli entre la superficie y el orificio")\
        .di("La fórmula de Torricelli no está en la hoja: hay que sacarla de Bernoulli. Punto uno, la superficie; punto dos, el orificio.",
            F(BERNOULLI),
            NOTA("Arriba y abajo hay presión atmosférica, y la superficie baja tan lento que su rapidez es casi cero."))\
        .di("Ponemos pe atmosférica en los dos lados y ve uno igual a cero.",
            M(r"p_{\text{atm}}+\rho g y_1+0=p_{\text{atm}}+\rho g y_2+\frac{1}{2}\rho v_2^2"))
    L.paso("Tachar y despejar")\
        .di("La presión atmosférica se cancela y también la densidad, porque está en todos los términos.",
            M(r"\rho g(y_1-y_2)=\frac{1}{2}\rho v_2^2"),
            M(r"v_2^2=2g(y_1-y_2)=2gh"),
            M(r"v_2=\sqrt{2gh}"))\
        .di("Solo queda la altura hache del líquido sobre el orificio.",
            R(r"v_2=\sqrt{2gh}\ \text{depende solo de }h"),
            OPCION("A", "descarta", "El área del tanque solo sirvió para decir que la superficie casi no se mueve."),
            OPCION("B", "descarta", "La densidad se cancela en Bernoulli."),
            OPCION("C", "descarta", "La presión atmosférica está en los dos lados y se cancela."),
            OPCION("D", "correcta", "v = √(2gh): solo depende de la profundidad h."))
    L.usa(BERNOULLI)
    L.oficial("II parcial extraordinario II-2026", "D")
    return L


@leccion("P2E26-4")
def p2e26_4() -> Leccion:
    L = Leccion(
        "P2E26-4", "Aceite viscoso en un tubo horizontal: alturas en los manómetros (II parcial extraordinario II-2026, P4)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "4"),
        enunciado=(
            "Por una tubería horizontal de sección constante circula aceite de viscosidad $\\eta$ en régimen laminar. "
            "Tubos verticales abiertos e iguales funcionan como manómetros. Sobre la altura del aceite en esos tubos es correcto que:"
        ),
        opciones={
            "A": "Es igual en todos los tubos, porque la rapidez es la misma en todas las secciones.",
            "B": "Es decreciente en la dirección del flujo, porque la viscosidad provoca una caída de presión.",
            "C": "Es creciente en la dirección del flujo, porque el fluido acumula presión.",
            "D": "Es igual en todos los tubos, excepto cerca de la salida.",
        },
        correcta="B",
        claves=["Poiseuille", "viscosidad", "caída de presión", "manómetro", "selección"],
    )
    L.paso("Para que un fluido viscoso avance, la presión tiene que bajar")\
        .di("Según Poiseuille, el caudal es proporcional a pe uno menos pe dos. Si hay flujo hacia adelante, pe uno es mayor que pe dos.",
            F(POISEUILLE),
            M(r"\frac{dV}{dt}>0\;\Rightarrow\;p_1>p_2"))
    L.paso("La altura en cada tubo mide la presión")\
        .di("En cada tubo abierto, la presión del tubo es la atmosférica más ro ge hache. Menos presión da menos altura.",
            F(HIDRO),
            M(r"h=\frac{p-p_0}{\rho g}"),
            R(r"h\ \text{baja en la dirección del flujo}"),
            OPCION("A", "descarta", "Con viscosidad, igual rapidez no significa igual presión."),
            OPCION("B", "correcta", "La viscosidad hace caer la presión a lo largo del tubo."),
            OPCION("C", "descarta", "La presión baja, no sube."),
            OPCION("D", "descarta", "La caída es pareja a lo largo de todo el tubo, no solo al final."))
    L.usa(POISEUILLE, HIDRO)
    L.oficial("II parcial extraordinario II-2026", "B")
    return L


@leccion("P2E26-5")
def p2e26_5() -> Leccion:
    L = Leccion(
        "P2E26-5", "Tubo que se angosta con dos tubos verticales (II parcial extraordinario II-2026, P5)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "5"),
        enunciado=(
            "Un fluido ideal pasa de una sección ancha (tubo vertical con altura $H$) a una sección angosta (tubo vertical con altura $L$), "
            "a la misma altura. ¿Qué relación hay entre $H$ y $L$?"
        ),
        opciones={"A": "H = L", "B": "H > L", "C": "H < L", "D": "Depende de la distancia horizontal entre los tubos."},
        correcta="B",
        claves=["Venturi", "Bernoulli", "continuidad", "selección"],
    )
    L.paso("Dónde va más rápido")\
        .di("Por continuidad, en la sección angosta el fluido va más rápido que en la ancha.",
            F(CONTINUIDAD),
            M(r"v_{\text{angosta}}>v_{\text{ancha}}"))
    L.paso("Bernoulli: más rapidez, menos presión")\
        .di("A la misma altura, Bernoulli dice que donde va más rápido la presión es menor.",
            F(BERNOULLI),
            M(r"p_{\text{ancha}}-p_{\text{angosta}}=\frac{1}{2}\rho\left(v_{\text{angosta}}^2-v_{\text{ancha}}^2\right)>0"))\
        .di("Cada tubo vertical sube hasta que ro ge por su altura iguala la presión manométrica. Más presión da más altura.",
            F(HIDRO),
            R(r"H>L"),
            OPCION("A", "descarta", "Las presiones son distintas porque las rapideces lo son."),
            OPCION("B", "correcta", "En lo ancho hay más presión, así que su tubo sube más."),
            OPCION("C", "descarta", "Está al revés: la sección angosta tiene menos presión."),
            OPCION("D", "descarta", "Un fluido ideal no pierde presión por la distancia."))
    L.usa(CONTINUIDAD, BERNOULLI, HIDRO)
    L.oficial("II parcial extraordinario II-2026", "B")
    return L


@leccion("P2E26-6")
def p2e26_6() -> Leccion:
    p, p0, rho = 1.00e5, 1.006e5, 1.20
    dp = p0 - p
    v = math.sqrt(2.0 * dp / rho)
    L = Leccion(
        "P2E26-6", "Tubo de Pitot: rapidez del aire (II parcial extraordinario II-2026, P6)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "6"),
        enunciado=(
            "Un tubo de Pitot mide la presión estática $p=1{,}00\\times 10^5\\ \\text{Pa}$ y la presión de remanso "
            "$p_0=1{,}006\\times 10^5\\ \\text{Pa}$. Si $\\rho_a=1{,}20\\ \\text{kg/m}^3$, ¿cuál es aproximadamente la rapidez del aire?"
        ),
        opciones={"A": "15,8 m/s", "B": "22,4 m/s", "C": "31,6 m/s", "D": "44,7 m/s"},
        correcta="C",
        claves=["Pitot", "Bernoulli", "presión de remanso", "selección"],
    )
    L.paso("Bernoulli entre la corriente y la punta del tubo")\
        .di("En la punta del tubo el aire se detiene: ahí la rapidez es cero y la presión es la de remanso. Las dos posiciones están a la misma altura.",
            DATO("presión de remanso"),
            F(BERNOULLI),
            M(r"p+\frac{1}{2}\rho_a v^2=p_0+0"))
    L.paso("Despejar la rapidez")\
        .di("Pasamos pe restando, multiplicamos por dos, dividimos entre ro y sacamos raíz.",
            M(r"v=\sqrt{\frac{2(p_0-p)}{\rho_a}}"),
            M(rf"p_0-p=1{{,}}006\times 10^5-1{{,}}00\times 10^5={tex(dp, 3)}\ \text{{Pa}}"),
            M(rf"v=\sqrt{{\frac{{2({tex(dp, 3)})}}{{1{{,}}20}}}}=\sqrt{{1000}}={tex(v, 3)}\ \text{{m/s}}"),
            R(rf"v\approx {tex(v, 3)}\ \text{{m/s}}"),
            OPCION("A", "descarta", "Olvida el 2 dentro de la raíz."),
            OPCION("B", "descarta", "No sale de √(2·600/1,20)."),
            OPCION("C", "correcta", "√(2 × 600 / 1,20) = √1000 = 31,6 m/s."),
            OPCION("D", "descarta", "Usa ρ = 0,60 en vez de 1,20."))
    L.usa(BERNOULLI)
    L.responde("v", rf"v={tex(v, 3)}\ \text{{m/s}}", v, "m/s")
    L.oficial("II parcial extraordinario II-2026", "C")
    return L


@leccion("P2E26-7")
def p2e26_7() -> Leccion:
    L = Leccion(
        "P2E26-7", "Agua que baja por un tubo que se ensancha: P_A frente a P_B (II parcial extraordinario II-2026, P7)",
        tema="fluidos", tipo="seleccion", nivel=2, parcial=2, fuente=_f(_EXT, "7"),
        enunciado=(
            "Agua fluye de forma estacionaria hacia abajo por un tubo vertical. El punto A está en la parte angosta, "
            "una altura $h$ por encima del punto B, que está en la parte ancha. ¿Cuál es la relación entre $P_A$ y $P_B$?"
        ),
        opciones={"A": "P_A = P_B", "B": "P_A < P_B", "C": "P_A > P_B", "D": "P_A − P_B = ρgh"},
        correcta="B",
        claves=["Bernoulli", "continuidad", "tubo vertical", "selección"],
    )
    L.paso("Rapideces: continuidad")\
        .di("A está en la parte angosta y B en la ancha. Por continuidad, en A el agua va más rápido.",
            F(CONTINUIDAD),
            M(r"A_A<A_B\;\Rightarrow\;v_A>v_B"))
    L.paso("Bernoulli con B como altura cero")\
        .di("Ponemos ye de B igual a cero y ye de A igual a hache.",
            F(BERNOULLI),
            M(r"p_A+\rho g h+\frac{1}{2}\rho v_A^2=p_B+0+\frac{1}{2}\rho v_B^2"))\
        .di("Pasamos pe a a la derecha y un medio de ro ve be al cuadrado a la izquierda.",
            M(r"p_B-p_A=\rho g h+\frac{1}{2}\rho v_A^2-\frac{1}{2}\rho v_B^2"),
            M(r"p_B-p_A=\rho g h+\frac{1}{2}\rho\left(v_A^2-v_B^2\right)"))\
        .di("Los dos términos de la derecha son positivos, así que pe be es mayor.",
            M(r"\rho g h>0\quad\text{y}\quad v_A>v_B\;\Rightarrow\;p_B-p_A>0"),
            R(r"P_A<P_B"),
            OPCION("A", "descarta", "Hay diferencia de altura y de rapidez."),
            OPCION("B", "correcta", "Abajo hay más presión: por la altura y porque el agua frena."),
            OPCION("C", "descarta", "Está al revés."),
            OPCION("D", "descarta", "Falta el término de rapidez, que no es cero; además el signo está al revés."))
    L.usa(CONTINUIDAD, BERNOULLI)
    L.oficial("II parcial extraordinario II-2026", "B")
    return L


# ============================================================================ extraordinario · desarrollos

@leccion("P2E26-8")
def p2e26_8() -> Leccion:
    rho, H, a = 1000.0, 5.45, 0.300
    y_sup, y_inf = 3.45, 3.15
    p_fondo = rho * G * H
    y_c = y_sup - a / 2.0
    h_c = H - y_c
    p_c = rho * G * h_c
    k = rho * G * a
    arriba_1, arriba_2 = H * y_sup, y_sup ** 2 / 2.0
    abajo_1, abajo_2 = H * y_inf, y_inf ** 2 / 2.0
    arriba = arriba_1 - arriba_2
    abajo = abajo_1 - abajo_2
    dif = arriba - abajo
    F_tot = k * dif
    A_cuad = a ** 2
    A_circ = math.pi * (a / 2.0) ** 2

    L = Leccion(
        "P2E26-8", "Ventana cuadrada en un tanque: presión y fuerza con integral (II parcial extraordinario II-2026, P8)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(_EXT, "8"),
        enunciado=(
            "Un tanque cúbico de $10{,}0\\ \\text{m}$ de lado, abierto arriba, tiene agua hasta $5{,}45\\ \\text{m}$ sobre el fondo. "
            "En una pared hay una ventana cuadrada de $30{,}0\\ \\text{cm}$ de lado cuyo borde superior está a $3{,}45\\ \\text{m}$ sobre el fondo. "
            "a) Presión manométrica en el fondo. b) Presión manométrica en el centro de la ventana. "
            "c) Fuerza hidrostática total sobre la ventana. d) Con una ventana circular de $30{,}0\\ \\text{cm}$ de diámetro en el mismo sitio, "
            "¿sobre cuál sería mayor la fuerza? (solo argumentar)"
        ),
        visual={"tipo": "pared", "forma": "ventana", "H": 5.45, "y_inf": 3.15, "y_sup": 3.45, "ancho": 0.300,
                "titulo": "Ventana de 0,300 m entre 3,15 m y 3,45 m sobre el fondo"},
        claves=["presión manométrica", "fuerza hidrostática", "integral", "franja", "ventana"],
    )
    L.paso("Dibujo y medidas")\
        .di("Medimos ye hacia arriba desde el fondo. La ventana va de tres coma quince a tres coma cuarenta y cinco metros, porque mide cero coma treinta.",
            DATO("3,45 m"),
            LISTA([
                r"agua hasta $5{,}45\ \text{m}$ · ventana de $3{,}15$ a $3{,}45\ \text{m}$ · lado $a=0{,}300\ \text{m}$",
                r"profundidad de un punto a altura $y$: $h=5{,}45-y$",
            ]))
    L.paso("a) Presión manométrica en el fondo")\
        .di("La manométrica no cuenta el aire de arriba: es solo ro ge hache. En el fondo, hache es cinco coma cuarenta y cinco metros.",
            F(HIDRO),
            F(MAN),
            M(r"p_{\text{man}}=p-p_0=\rho g h"),
            M(rf"p_{{\text{{fondo}}}}=(1000)(9{{,}}80)(5{{,}}45)={tex(p_fondo, 5)}\ \text{{Pa}}"),
            R(rf"p_{{\text{{fondo}}}}={tex(p_fondo / 1000.0, 4)}\ \text{{kPa}}"))
    L.paso("b) Presión en el centro de la ventana")\
        .di("El centro está media ventana por debajo del borde de arriba. Después restamos para tener la profundidad.",
            M(rf"y_c=3{{,}}45-\frac{{0{{,}}300}}{{2}}={tex(y_c, 3)}\ \text{{m}}"),
            M(rf"h_c=5{{,}}45-{tex(y_c, 3)}={tex(h_c, 3)}\ \text{{m}}"),
            M(rf"p_c=(1000)(9{{,}}80)({tex(h_c, 3)})={tex(p_c, 5)}\ \text{{Pa}}"),
            R(rf"p_c={tex(p_c / 1000.0, 4)}\ \text{{kPa}}"))
    L.paso("c) Una franja horizontal de la ventana")\
        .di("Partimos la ventana en franjas horizontales delgadas. En una franja toda el agua está a la misma profundidad, así que la presión es una sola.",
            F(r"p=\frac{dF_\perp}{dA}"),
            M(r"dA=a\,dy"),
            M(r"p(y)=\rho g\,(5{,}45-y)"))\
        .di("La fuerza de la franja es su presión por su área.",
            M(r"dF=p\,dA=\rho g\,(5{,}45-y)\,a\,dy"))
    L.paso("c) Sumar todas las franjas: la integral")\
        .di("Sumar todas las franjas, de tres coma quince a tres coma cuarenta y cinco, es integrar. Ro, ge y a no cambian: salen de la suma.",
            F(r"F_\perp=\int p\,dA"),
            M(r"F=\int_{3{,}15}^{3{,}45}\rho g a\,(5{,}45-y)\,dy=\rho g a\int_{3{,}15}^{3{,}45}(5{,}45-y)\,dy"),
            M(rf"\rho g a=(1000)(9{{,}}80)(0{{,}}300)={tex(k, 4)}\ \text{{N/m}}^2"))
    L.paso("c) Integrar 5,45 − y, término por término")\
        .di("Un número solo, al integrarlo, queda multiplicado por ye. Y ye, que es ye a la uno, da ye al cuadrado entre dos.",
            M(r"\int 5{,}45\,dy=5{,}45\,y"),
            M(r"\int y\,dy=\frac{y^{1+1}}{1+1}=\frac{y^2}{2}"),
            M(r"\int(5{,}45-y)\,dy=5{,}45\,y-\frac{y^2}{2}"))
    L.paso("c) Arriba menos abajo")\
        .di("Evaluamos en tres coma cuarenta y cinco, después en tres coma quince, y restamos.",
            M(rf"(5{{,}}45)(3{{,}}45)-\frac{{3{{,}}45^2}}{{2}}={tex(arriba_1, 6)}-{tex(arriba_2, 6)}={tex(arriba, 7)}"),
            M(rf"(5{{,}}45)(3{{,}}15)-\frac{{3{{,}}15^2}}{{2}}={tex(abajo_1, 6)}-{tex(abajo_2, 6)}={tex(abajo, 7)}"),
            M(rf"{tex(arriba, 7)}-{tex(abajo, 7)}={tex(dif, 3)}\ \text{{m}}^2"))\
        .di("Multiplicamos por ro ge a.",
            M(rf"F=({tex(k, 4)})({tex(dif, 3)})={tex(F_tot, 5)}\ \text{{N}}"),
            R(rf"F={tex(F_tot, 5)}\ \text{{N}}"))
    L.paso("Comprobación rápida")\
        .di("La presión crece en línea recta con la profundidad. Por eso la fuerza total es la presión del centro por el área, y da lo mismo.",
            M(rf"p_c\,A=({tex(p_c, 5)})(0{{,}}0900)={tex(p_c * A_cuad, 5)}\ \text{{N}}"),
            OJO("Este atajo sirve para comprobar. Si el ancho cambia con la altura (trapecio o parábola), hay que integrar."))
    L.paso("d) Ventana circular en el mismo sitio")\
        .di("El círculo tiene el centro a la misma profundidad, así que la presión del centro es la misma. Su área es menor que la del cuadrado.",
            M(rf"A_{{\text{{círculo}}}}=\pi(0{{,}}150)^2={tex(A_circ, 3)}\ \text{{m}}^2"),
            M(rf"{tex(A_circ, 3)}\ \text{{m}}^2<0{{,}}0900\ \text{{m}}^2"),
            R(r"\text{mayor fuerza sobre la ventana cuadrada}"))
    L.usa(HIDRO, MAN, r"p=\frac{dF_\perp}{dA}", r"F_\perp=\int p\,dA")
    L.responde("a", rf"p_{{\text{{fondo}}}}={tex(p_fondo, 5)}\ \text{{Pa}}", p_fondo, "Pa")
    L.responde("b", rf"p_c={tex(p_c, 5)}\ \text{{Pa}}", p_c, "Pa")
    L.responde("c", rf"F={tex(F_tot, 5)}\ \text{{N}}", F_tot, "N")
    L.oficial("II parcial extraordinario II-2026", 53410, 21070, 1896.3, tol=0.01)
    L.te_toca("Si el agua subiera hasta 6,45 m (un metro más), ¿cuál sería la fuerza sobre la ventana?",
              rho * G * (h_c + 1.0) * A_cuad, "N", tol=0.01,
              pista="El centro queda 1 m más hondo: usa p_c·A con h_c = 3,15 m.", solucion="(1000)(9,80)(3,15)(0,0900) = 2778 N.")
    return L


@leccion("P2E26-9")
def p2e26_9() -> Leccion:
    rho, R_t, L_t, Q, dp = 850.0, 2.00e-3, 5.00, 1.50e-5, 180e3
    rho_e, r_e = 2200.0, 0.500e-3
    eta = math.pi * R_t ** 4 * dp / (8.0 * Q * L_t)
    eta_v = _q(eta)
    den = 4.0 * eta_v * L_t
    vmax = dp * R_t ** 2 / den
    r = 1.00e-3
    v_r = dp * (R_t ** 2 - r ** 2) / den
    vT = (2.0 / 9.0) * r_e ** 2 * G * (rho_e - rho) / eta_v
    vT_v = _q(vT)
    FS = 6.0 * math.pi * r_e * eta_v * vT_v

    L = Leccion(
        "P2E26-9", "Aceite mineral en un tubo: viscosidad, perfil de velocidad y rapidez terminal (II parcial extraordinario II-2026, P9)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f(_EXT, "9"),
        enunciado=(
            "Un aceite ($\\rho=850\\ \\text{kg/m}^3$) pasa por un tubo horizontal de $L=5{,}00\\ \\text{m}$ y radio $R=2{,}00\\ \\text{mm}$ "
            "con caudal $1{,}50\\times 10^{-5}\\ \\text{m}^3/\\text{s}$. a) Si la diferencia de presión es $180\\ \\text{kPa}$, halle la viscosidad. "
            "b) Velocidad máxima y velocidad a $r=1{,}00\\ \\text{mm}$ del eje. c) Velocidad terminal de una esfera de teflón "
            "($r_e=0{,}500\\ \\text{mm}$, $2200\\ \\text{kg/m}^3$) en ese aceite. d) Fuerza viscosa sobre la esfera a esa velocidad."
        ),
        claves=["Poiseuille", "viscosidad", "perfil de velocidad", "Stokes", "velocidad terminal"],
        aviso="El encabezado de la solución oficial dice «Primera Prueba Parcial Ordinaria (07/09/2026)» por error: es el II parcial extraordinario.",
    )
    L.paso("a) Viscosidad: despejar η de Poiseuille")\
        .di("La casilla de Poiseuille tiene el caudal y la diferencia de presión. Despejamos eta pasando el ocho, la ele y el caudal.",
            DATO("180 kPa"),
            F(POISEUILLE),
            M(r"\eta=\frac{(p_1-p_2)\pi R^4}{8L\,(dV/dt)}"),
            M(rf"\eta=\frac{{(180\times 10^3)\pi(2{{,}}00\times 10^{{-3}})^4}}{{8(5{{,}}00)(1{{,}}50\times 10^{{-5}})}}={tex(eta, 4)}\ \text{{Pa}}\cdot\text{{s}}"),
            R(rf"\eta={tex(eta, 4)}\ \text{{Pa}}\cdot\text{{s}}"))
    L.paso("b) Velocidad máxima: en el eje del tubo")\
        .di("La casilla del perfil de velocidad da la rapidez a una distancia erre del eje. La máxima está en el eje, donde erre vale cero.",
            F(PERFIL),
            M(r"v_{\text{máx}}=v(0)=\frac{(p_1-p_2)R^2}{4\eta L}"),
            M(rf"4\eta L=4({tex(eta_v, 4)})(5{{,}}00)={tex(den, 4)}"),
            M(rf"v_{{\text{{máx}}}}=\frac{{(180\times 10^3)(2{{,}}00\times 10^{{-3}})^2}}{{{tex(den, 4)}}}={tex(vmax, 4)}\ \text{{m/s}}"),
            R(rf"v_{{\text{{máx}}}}={tex(vmax, 3)}\ \text{{m/s}}"))
    L.paso("b) Velocidad a 1,00 mm del eje")\
        .di("Ahora erre vale un milímetro. Restamos los cuadrados dentro del paréntesis.",
            M(r"R^2-r^2=(2{,}00\times 10^{-3})^2-(1{,}00\times 10^{-3})^2=3{,}00\times 10^{-6}\ \text{m}^2"),
            M(rf"v=\frac{{(180\times 10^3)(3{{,}}00\times 10^{{-6}})}}{{{tex(den, 4)}}}={tex(v_r, 4)}\ \text{{m/s}}"),
            R(rf"v(1\ \text{{mm}})={tex(v_r, 3)}\ \text{{m/s}}"))
    L.paso("c) Velocidad terminal de la esfera")\
        .di("La casilla de velocidad terminal usa el radio de la esfera y la diferencia de densidades: teflón menos aceite.",
            F(TERMINAL),
            M(rf"\rho-\rho'=2200-850=1350\ \text{{kg/m}}^3"),
            M(rf"v_T=\frac{{2}}{{9}}\frac{{(5{{,}}00\times 10^{{-4}})^2(9{{,}}80)}}{{{tex(eta_v, 4)}}}(1350)={tex(vT, 4)}\ \text{{m/s}}"),
            R(rf"v_T={tex(vT, 3)}\ \text{{m/s}}"))
    L.paso("d) Fuerza viscosa a la velocidad terminal")\
        .di("La ley de Stokes de la hoja da la fuerza viscosa con el radio, la viscosidad y la velocidad terminal.",
            F(STOKES),
            M(rf"F_S=6\pi(5{{,}}00\times 10^{{-4}})({tex(eta_v, 4)})({tex(vT_v, 4)})={tex(FS, 3)}\ \text{{N}}"),
            R(rf"F_S={tex(FS, 3)}\ \text{{N}}"),
            NOTA("Comprobación: a velocidad terminal, la fuerza viscosa iguala peso menos empuje, (ρ − ρ′)·g·(4/3)πr³ ≈ 6,93 × 10⁻⁶ N."))
    L.usa(POISEUILLE, PERFIL, TERMINAL, STOKES)
    L.responde("a", rf"\eta={tex(eta, 4)}\ \text{{Pa}}\cdot\text{{s}}", eta, "Pa·s")
    L.responde("b-max", rf"v_{{\text{{máx}}}}={tex(vmax, 3)}\ \text{{m/s}}", vmax, "m/s")
    L.responde("b-r", rf"v={tex(v_r, 3)}\ \text{{m/s}}", v_r, "m/s")
    L.responde("c", rf"v_T={tex(vT, 3)}\ \text{{m/s}}", vT, "m/s")
    L.responde("d", rf"F_S={tex(FS, 3)}\ \text{{N}}", FS, "N")
    L.oficial("II parcial extraordinario II-2026", 0.0151, 2.39, 1.79, 0.0487, 6.93e-6, tol=0.015)
    L.te_toca("Si el radio del tubo fuera el doble (mismo Δp, misma η y L), ¿por cuánto se multiplica el caudal?",
              16.0, "", tol=0.01, pista="Mirá el exponente de R en Poiseuille.", solucion="2⁴ = 16 veces.")
    return L
