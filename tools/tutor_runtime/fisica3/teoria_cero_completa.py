"""Teoría desde cero completa para FI2103: Prerrequisitos relámpago (T-BASE1..4),
Parcial 1 (T-MAS, T-PEND, T-ONDA, T-SON) y Parcial 2 (T-HIDRO, T-FLOT, T-BERN, T-VISC).

Diseñada con escalera pedagógica de 3 capas para comprensión rápida y pasos muy cortos:
  * Intuición concreta sin asumir conocimientos previos (enlazada al modelo 3D interactivo);
  * Traductor símbolo por símbolo con unidades SI y atajos de calculadora;
  * Receta exacta de examen con casillas del formulario oficial, pregunta rápida y «Te toca».
"""

from __future__ import annotations

import math

from .dsl import F, LISTA, M, NOTA, OJO, Leccion, R, T, TABLA, s
from .registro import leccion


def _teo(id: str, titulo: str, tema: str, parcial: int, **k) -> Leccion:
    return Leccion(
        id, titulo, tema=tema, tipo="teoria", nivel=1, parcial=parcial,
        fuente={"texto": f"Desde cero · Parcial {parcial}" if parcial in (1, 2) else "Bases rápidas · FI2103"},
        **k,
    )


# ============================================================================ PRERREQUISITOS RELÁMPAGO

@leccion("T-BASE1")
def t_base1() -> Leccion:
    L = _teo(
        "T-BASE1", "Bases rápidas (1): despejes y signos sin miedo", "teoria", 3,
        claves=["despeje", "signos", "álgebra", "desde cero", "bases"],
        enunciado="Cómo despejar cualquier letra de una fórmula del formulario en dos renglones y no perder puntos por signos.",
    )
    L.paso("La regla de la balanza: lo que haces a un lado, al otro")\
        .por_que("Despejar no es memorizar trucos: es deshacer una operación haciendo la contraria en ambos lados.")\
        .di("Una ecuación es una balanza equilibrada. Si una letra multiplica, pasa al otro lado dividiendo.",
            M(r"pV=nRT\;\Rightarrow\;T=\frac{pV}{nR}"))\
        .di("Si divide, pasa multiplicando. Si está al cuadrado, sacas raíz cuadrada a todo el otro lado.",
            M(r"E=\frac{1}{2}kA^2\;\Rightarrow\;A=\sqrt{\frac{2E}{k}}"))\
        .pregunta_rapida("Si F = p·A y quieres despejar A, ¿qué queda?", ["A = F / p", "A = F · p", "A = p / F"], 0,
                         "Como p multiplica a A, pasa dividiendo al otro lado: A = F/p.")
    L.paso("Despejar una letra que está abajo en una fracción")\
        .por_que("Uno de los errores más caros en física es despejar el denominador al revés.")\
        .di("Cuando la incógnita está abajo, como el volumen en densidad, primero súbela multiplicando.",
            M(r"\rho=\frac{m}{V}\;\Rightarrow\;\rho V=m"))\
        .di("Y ahora sí bajas la letra que estorba dividiendo. Dos renglones limpios, cero errores.",
            R(r"V=\frac{m}{\rho}"),
            OJO("Nunca dividas al revés: si 5 = 10/V, entonces V = 10/5 = 2."))
    L.paso("Regla de signos y restas de examen")\
        .por_que("En primera ley y manómetros un signo menos mal puesto invierte toda la respuesta.")\
        .di("Menos por menos da más. Y en cualquier delta, siempre es final menos inicial.",
            LISTA([
                "$\\Delta T = T_{\\text{final}} - T_{\\text{inicial}}$ (si se enfría, es negativo)",
                "$-(-W) = +W$ (comprimir tiene $W<0$, así que $-W > 0$)",
                "Dividir entre $0{,}5$ duplica el número: $-20/0{,}5 = -40$",
            ]))
    L.te_toca("Si $Q = m c \\Delta T$ con $Q = 4000\\ \\text{J}$, $m = 2{,}0\\ \\text{kg}$ y $c = 500\\ \\text{J/(kg}\\cdot\\text{K)}$, ¿cuánto vale $\\Delta T$?",
              4.0, "K", pista="Despeja ΔT = Q / (m·c) = 4000 / (2·500).", solucion="ΔT = 4000 / 1000 = 4,0 K.")
    return L


@leccion("T-BASE2")
def t_base2() -> Leccion:
    L = _teo(
        "T-BASE2", "Bases rápidas (2): unidades SI y calculadora Casio", "teoria", 3,
        claves=["unidades", "SI", "kelvin", "litros", "calculadora", "desde cero"],
        enunciado="Las 6 conversiones que aparecen en casi todos los problemas de la reposición y cómo evitar fallos de potencia de diez.",
    )
    L.paso("Por qué siempre pasamos primero al Sistema Internacional")\
        .por_que("Las constantes de la hoja (g = 9,8, R = 8,314, σ) están en SI: mezclar cm o litros rompe el número por factores de 100 o 1000.")\
        .di("Antes de sustituir un solo número, convierte todo a metros, kilogramos, segundos, pascales y kelvin.",
            TABLA(["Te dan", "En SI", "Operación rápida"],
                  [["cm", "m", "dividir entre 100 ($\\times 10^{-2}$)"],
                   ["mm", "m", "dividir entre 1000 ($\\times 10^{-3}$)"],
                   ["L (litros)", "m³", "dividir entre 1000 ($\\times 10^{-3}$)"],
                   ["atm", "Pa", "multiplicar por $101\\,325$"],
                   ["°C (temperatura)", "K", "sumar $273{,}15$"],
                   ["g (gramos)", "kg", "dividir entre 1000"]]))
    L.paso("Diámetro vs radio y área al cuadrado")\
        .por_que("En Bernoulli y Poiseuille casi siempre dan el diámetro en centímetros y la fórmula pide el radio en metros.")\
        .di("El radio es siempre la mitad del diámetro. Primero pasa el diámetro a metros y divide entre dos.",
            M(r"D=6{,}0\ \text{cm}=0{,}060\ \text{m}\;\Rightarrow\;r=\frac{D}{2}=0{,}030\ \text{m}"))\
        .di("Con el radio ya en metros, el área circular sale directa.",
            M(r"A=\pi r^2=\pi(0{,}030)^2=2{,}83\times 10^{-3}\ \text{m}^2"),
            OJO("En la calculadora Casio usa la tecla ×10ˣ (EXP) en vez de escribir *10^ para que nunca se rompan las divisiones."))
    L.paso("La trampa de los grados Celsius frente a delta T")\
        .por_que("Saber cuándo sumar 273,15 y cuándo NO ahorra errores en gas ideal y radiación.")\
        .di("Si la fórmula lleva T sola, como pV igual a nRT o T a la cuatro, usa kelvin obligatorio.",
            M(r"T(\text{K})=T(^\circ\text{C})+273{,}15"))\
        .di("Si lleva delta T, una resta, vale exactamente lo mismo en Celsius que en kelvin.",
            M(r"\Delta T=80-20=60\ ^\circ\text{C}=60\ \text{K}"))\
        .pregunta_rapida("¿Cuántos metros cúbicos son 9,00 litros?", ["0,00900 m³", "0,0900 m³", "9000 m³"], 0,
                         "1 m³ = 1000 L, así que divides entre 1000: 9,00 × 10⁻³ = 0,00900 m³.")
    L.te_toca("Convierte $4{,}0\\ \\text{cm}$ de diámetro a radio en metros.",
              0.02, "m", pista="Primero 4,0 cm = 0,040 m; luego divide entre 2.", solucion="r = 0,040 / 2 = 0,020 m.")
    return L


@leccion("T-BASE3")
def t_base3() -> Leccion:
    L = _teo(
        "T-BASE3", "Bases rápidas (3): radianes, coseno y ángulo de fase en 3D", "ondas", 1,
        claves=["radianes", "fase", "coseno", "seno", "cuadrante", "desde cero"],
        enunciado="Cómo leer el círculo fasorial 3D para poner la calculadora en RAD y elegir siempre la fase φ correcta sin adivinar.",
    )
    L.paso("Radianes: una vuelta completa son dos pi")\
        .por_que("En ondas y MAS, ωt está en radianes; si la Casio queda en DEG todo el resultado sale malo.")\
        .di("Una vuelta completa al círculo son trescientos sesenta grados, que en radianes equivale a dos pi.",
            M(r"360^\circ=2\pi\ \text{rad}\qquad 180^\circ=\pi\ \text{rad}\qquad 90^\circ=\frac{\pi}{2}\ \text{rad}"))\
        .di("Antes de empezar el Parcial 1, pon la calculadora en modo radianes.",
            OJO("Verifica que arriba en la pantalla de tu calculadora diga RAD (no DEG)."))
    L.paso("El círculo fasorial: por qué hay dos ángulos posibles")\
        .por_que("cos(φ) = cos(−φ) siempre da dos candidatos; ignorar el signo de la velocidad es el error #1 del Parcial 1.")\
        .di("Mira la flecha giratoria en el modelo 3D. Su sombra horizontal es la posición equis.",
            F(r"x=A\cos(\omega t+\phi)"))\
        .di("En t igual cero, el coseno inverso te da un ángulo positivo, pero su negativo tiene la misma sombra.",
            M(r"\cos\phi=\frac{x_0}{A}\;\Rightarrow\;\phi=+\alpha\quad\text{o}\quad\phi=-\alpha"))\
        .di("El signo de la velocidad inicial elige cuál de los dos es el verdadero.",
            F(r"v=-\omega A\sen(\omega t+\phi)"),
            LISTA([
                "Si $v(0) > 0$ (va hacia la derecha): necesitas $\\sen\\phi < 0 \\Rightarrow \\phi$ **negativo** (o cuadrante III/IV)",
                "Si $v(0) < 0$ (va hacia la izquierda): necesitas $\\sen\\phi > 0 \\Rightarrow \\phi$ **positivo** (cuadrante I/II)",
            ]))\
        .pregunta_rapida("Si x(0) = +A/2 y v(0) > 0, ¿cuál fase cumple ambas condiciones?", ["φ = −π/3 rad", "φ = +π/3 rad", "φ = 0 rad"], 0,
                         "cos(±π/3) = +1/2, pero v(0) = −ωA sen(φ) > 0 exige sen(φ) < 0, así que φ = −π/3 rad.")
    L.te_toca("Si un resorte tiene $A = 0{,}10\\ \\text{m}$, $x(0) = 0{,}05\\ \\text{m}$ y $v(0) < 0$, ¿cuánto vale $\\phi$ en radianes? (usa 3 decimales)",
              round(math.pi / 3, 3), "rad", tol=0.03,
              pista="cos(φ) = 0,05/0,10 = 0,5 → arccos(0,5) = π/3 ≈ 1,047 rad. Con v(0) < 0, φ es positivo.",
              solucion="φ = +π/3 ≈ +1,047 rad.")
    L.usa(r"x=A\cos(\omega t+\phi)", r"v=-\omega A\sen(\omega t+\phi)")
    return L


@leccion("T-BASE4")
def t_base4() -> Leccion:
    L = _teo(
        "T-BASE4", "Bases rápidas (4): geometría de cortes 3D para integrales", "fluidos", 2,
        claves=["geometría", "Pitágoras", "trapecio", "disco", "cascarón", "desde cero"],
        enunciado="Las únicas 4 fórmulas geométricas que convierten un dibujo 3D en el ancho o el área del pedacito diferencial.",
    )
    L.paso("Rectángulo y trapecio: cómo varía el ancho con una recta")\
        .por_que("En una represa trapezoidal el ancho baja linealmente desde arriba hasta el fondo y entra directo en la integral hidrostática.")\
        .di("La fuerza sobre una pared sumergida se arma sumando presión por área en cada franja horizontal.",
            F(r"p = p_0 + \rho gh"),
            F(r"F_\perp = \int p\,dA"))\
        .di("En un rectángulo el ancho es fijo. En un trapecio, el ancho sigue una línea recta con la profundidad.",
            M(r"L(h)=b_{\text{sup}}+\frac{b_{\text{inf}}-b_{\text{sup}}}{H}\,h"))\
        .di("Comprueba los extremos: en h igual cero da el ancho de arriba; en h igual H da el del fondo.",
            NOTA("Siempre prueba h = 0 y h = H en tu recta: si coinciden con el dibujo, está perfecta."))
    L.paso("Círculo y esfera cortada: el triángulo de Pitágoras")\
        .por_que("Tanto la compuerta semicircular como la esfera cortada de conducción salen del mismo triángulo.")\
        .di("Desde el centro hasta el borde del corte hay un triángulo rectángulo con hipotenusa R.",
            M(r"r^2+y^2=R^2\;\Rightarrow\;r=\sqrt{R^2-y^2}"))\
        .di("Para un disco circular en la esfera, el área es pi por erre cuadrado: la raíz desaparece sola.",
            R(r"A(y)=\pi r^2=\pi\,(R^2-y^2)"))
    L.paso("Las tres áreas de examen en un vistazo")\
        .por_que("No hace falta memorizar fórmulas raras: solo reconocer cuál de los 3 cortes estás mirando en 3D.")\
        .di("Mira la tabla y compárala girando el modelo 3D en vista Corte.",
            TABLA(["Forma 3D", "Pedacito", "Área del pedacito"],
                  [["Pared / compuerta", "Franja horizontal $dh$", "$dA = L(h)\\,dh$"],
                   ["Cono o esfera cortada", "Disco perpendicular $dy$", "$A = \\pi r^2$"],
                   ["Tubo cilíndrico radial", "Cascarón de radio $r$", "$A(r) = 2\\pi r L$"]]))\
        .pregunta_rapida("En una esfera de radio R = 0,10 m cortada a altura y = 0,06 m, ¿cuánto vale el radio r del disco?",
                         ["0,08 m", "0,04 m", "0,16 m"], 0,
                         "Por Pitágoras: r = √(0,10² − 0,06²) = √(0,0100 − 0,0036) = √0,0064 = 0,08 m.")
    L.te_toca("Una represa trapezoidal mide $40\\ \\text{m}$ arriba, $20\\ \\text{m}$ abajo y tiene $10\\ \\text{m}$ de alto. ¿Cuánto mide su ancho a mitad de profundidad ($h = 5\\ \\text{m}$)?",
              30.0, "m", pista="A la mitad de un trapecio el ancho es el promedio: (40 + 20)/2.", solucion="L(5) = 40 − 2(5) = 30 m.")
    L.usa(r"p = p_0 + \rho gh", r"F_\perp = \int p\,dA")
    return L


# ============================================================================ PARCIAL 1 DESDE CERO

@leccion("T-MAS")
def t_mas() -> Leccion:
    L = _teo(
        "T-MAS", "Teoría desde cero (P1): resorte, fase φ y energía del MAS", "ondas", 1,
        claves=["MAS", "resorte", "fase", "período", "energía", "desde cero"],
        enunciado="Entiende el movimiento armónico simple desde cero mirando el resorte 3D y domina la receta de 5 pasos de reposición.",
    )
    L.paso("La idea en 3D: ir y venir alrededor del equilibrio")\
        .por_que("Si entiendes qué pasa en los extremos y en el centro, la mitad de las selecciones del Parcial 1 salen sin calculadora.")\
        .di("Mira el bloque en el modelo 3D: el resorte siempre lo jala de vuelta hacia el centro, equis igual cero.",
            F(r"F_x = -kx"))\
        .di("En los extremos, el bloque se frena un instante: velocidad cero y fuerza máxima. En el centro, velocidad máxima y fuerza cero.",
            LISTA([
                "Extremos ($x = \\pm A$): $v = 0$, $U = \\text{máx}$, $K = 0$",
                "Centro ($x = 0$): $v = \\pm\\omega A$ (máx), $U = 0$, $K = \\text{máx}$",
            ]))
    L.paso("Qué rápido oscila: omega y período T")\
        .por_que("En un resorte ideal, el período depende solo de la masa y de k, nunca de qué tanto lo estires.")\
        .di("Un resorte más duro o una masa más liviana oscilan más rápido. Eso lo mide omega.",
            F(r"\omega^2 = \frac{k}{m}"),
            F(r"T = \frac{2\pi}{\omega}"))\
        .di("El período te es el tiempo de una vuelta completa ida y vuelta. La frecuencia efe es uno entre te.",
            M(r"T = 2\pi\sqrt{\frac{m}{k}}\qquad f = \frac{1}{T}"))\
        .pregunta_rapida("Si duplicas la amplitud A del resorte sin cambiar m ni k, ¿qué pasa con el período T?",
                         ["Queda exactamente igual", "Se duplica", "Se reduce a la mitad"], 0,
                         "T = 2π√(m/k) no contiene a la amplitud A.")
    L.paso("La ecuación completa y la energía total")\
        .por_que("En 3 de 5 reposiciones piden escribir x(t) con la fase correcta y calcular la energía total.")\
        .di("La posición sigue un coseno. Si te dan equis cero y ve cero, sacas amplitud y fase directamente de la hoja.",
            F(r"x = A\cos(\omega t + \phi)"),
            F(r"A = \sqrt{x^2 + \frac{v^2}{\omega^2}}"))\
        .di("La energía mecánica se reparte entre cinética y potencial, y su suma nunca cambia.",
            F(r"E = \frac{1}{2}kA^2"),
            F(r"E = K + U = \frac{1}{2}mv^2 + \frac{1}{2}kx^2"))
    L.te_toca("Un bloque de $2{,}0\\ \\text{kg}$ unido a un resorte de $k = 200\\ \\text{N/m}$ oscila con amplitud $A = 0{,}10\\ \\text{m}$. ¿Cuál es su energía mecánica total $E$?",
              1.0, "J", pista="Usa E = ½ k A² = 0,5 · 200 · (0,10)².", solucion="E = 0,5(200)(0,01) = 1,0 J.")
    L.usa(r"F_x = -kx", r"\omega^2 = \frac{k}{m}", r"T = \frac{2\pi}{\omega}", r"x = A\cos(\omega t + \phi)", r"E = \frac{1}{2}kA^2")
    return L


@leccion("T-PEND")
def t_pend() -> Leccion:
    L = _teo(
        "T-PEND", "Teoría desde cero (P1): péndulo simple, físico y Steiner", "ondas", 1,
        claves=["péndulo", "péndulo físico", "inercia", "Steiner", "desde cero"],
        enunciado="Diferencia entre un péndulo simple y un cuerpo rígido colgado (péndulo físico), con comparación animada en 3D.",
    )
    L.paso("Péndulo simple: solo importan el largo y la gravedad")\
        .por_que("En selección suelen cambiar la masa para distraerte; la fórmula muestra que la masa se cancela.")\
        .di("Un péndulo simple es una bolita pequeña al final de una cuerda liviana. Su frecuencia angular depende solo de ge y ele.",
            F(r"\omega^2 = \frac{g}{L}"),
            F(r"T = \frac{2\pi}{\omega}"))\
        .di("Más largo significa más lento. Si el largo se cuadruplica, la raíz de cuatro es dos: el período se duplica.",
            M(r"L' = 4L \;\Rightarrow\; T' = \sqrt{4}\,T = 2T"))\
        .pregunta_rapida("Si cuelgas el doble de masa en la misma cuerda, ¿qué ocurre con el período T?",
                         ["No cambia", "Se duplica", "Se reduce a la mitad"], 0,
                         "En T = 2π√(L/g) la masa no aparece.")
    L.paso("Péndulo físico: cuando el objeto tiene forma y tamaño")\
        .por_que("Si cuelgas una varilla, un aro o una esfera con varilla, necesitas el momento de inercia I respecto al pivote.")\
        .di("Mira el punto violeta en el modelo 3D: de es la distancia desde el pivote hasta el centro de masa.",
            F(r"\omega^2 = \frac{mgd}{I}"))\
        .di("Si conoces la inercia en el centro de masa, el teorema de ejes paralelos la traslada al pivote.",
            F(r"I_{\text{eje}\parallel} = I_{\text{cm}} + md^2"),
            OJO("Recuerda siempre sumar m·d² al trasladar el eje desde el centro de masa hasta el pivote."))
    L.te_toca("Un péndulo simple tiene período $T = 2{,}0\\ \\text{s}$. Si su largo se reduce a la cuarta parte ($L/4$), ¿cuál es su nuevo período?",
              1.0, "s", pista="T es proporcional a √L; √(1/4) = 1/2.", solucion="T' = 2,0 · √(1/4) = 1,0 s.")
    L.usa(r"\omega^2 = \frac{g}{L}", r"\omega^2 = \frac{mgd}{I}", r"I_{\text{eje}\parallel} = I_{\text{cm}} + md^2")
    return L


@leccion("T-ONDA")
def t_onda() -> Leccion:
    L = _teo(
        "T-ONDA", "Teoría desde cero (P1): cuerdas, armónicos y ondas estacionarias", "ondas", 1,
        claves=["cuerda", "armónicos", "nodos", "onda estacionaria", "desde cero"],
        enunciado="Cómo viaja una onda por una cuerda, qué significan los nodos y vientres en 3D y cómo sacar cualquier armónico.",
    )
    L.paso("Qué tan rápido viaja la onda en la cuerda")\
        .por_que("Toda pregunta de guitarras, violines o trastes arranca calculando la densidad lineal μ y la rapidez v.")\
        .di("Primero calculamos cuánta masa hay por cada metro de cuerda: eso es mu.",
            F(r"\mu = \frac{m}{L}"))\
        .di("Más tensión efe hace la onda más rápida; una cuerda más pesada la frena.",
            F(r"v = \sqrt{\frac{F}{\mu}}"),
            F(r"v = \lambda f = \frac{\omega}{k}"))
    L.paso("Armónicos, nodos y vientres en el modelo 3D")\
        .por_que("Contar los vientres en el dibujo te da de inmediato el número de armónico n.")\
        .di("En una cuerda fija en ambos extremos solo caben mitades exactas de onda. Cada vientre mide media longitud de onda.",
            F(r"\lambda_n = \frac{2L}{n}"),
            F(r"f_n = \frac{v}{\lambda_n}"),
            F(r"f_n = n f_1"))\
        .di("Mueve el control Armónico n en el 3D: el armónico ene tiene ene vientres y ene más un nodos.",
            NOTA("Distancia entre dos nodos consecutivos = $\\lambda/2$. Al pisar un traste se acorta $L$ y sube la frecuencia."))\
        .pregunta_rapida("En el 3.er armónico (n = 3) de una cuerda fija en ambos extremos, ¿cuántos nodos hay en total (incluyendo extremos)?",
                         ["4 nodos", "3 nodos", "2 nodos"], 0,
                         "Hay n vientres y n + 1 nodos: para n = 3 son 4 nodos.")
    L.paso("Ecuación de onda estacionaria y rapidez transversal")\
        .por_que("En reposición suelen darte y(x,t) = A_OE sen(kx) sen(ωt) y pedir k, ω, λ, v y la amplitud de cada viajera.")\
        .di("De la ecuación lees directamente ka delante de equis y omega delante de te.",
            F(r"y(x,t) = A_{\text{OE}}\sen(kx)\sen(\omega t)"),
            F(r"k = \frac{2\pi}{\lambda}"))\
        .di("Una estacionaria es la suma de dos ondas viajeras opuestas, cada una con la mitad de amplitud.",
            M(r"A_{\text{viajera}} = \frac{A_{\text{OE}}}{2}\qquad v_{y,\text{máx}} = \omega A_{\text{OE}}"))
    L.te_toca("Una cuerda de $L = 1{,}0\\ \\text{m}$ tiene rapidez de onda $v = 200\\ \\text{m/s}$. ¿Cuál es su frecuencia fundamental $f_1$?",
              100.0, "Hz", pista="Para n=1, λ₁ = 2L = 2,0 m y f₁ = v / λ₁.", solucion="f₁ = 200 / 2,0 = 100 Hz.")
    L.usa(r"\mu = \frac{m}{L}", r"v = \sqrt{\frac{F}{\mu}}", r"\lambda_n = \frac{2L}{n}", r"f_n = n f_1", r"y(x,t) = A_{\text{OE}}\sen(kx)\sen(\omega t)")
    return L


@leccion("T-SON")
def t_son() -> Leccion:
    L = _teo(
        "T-SON", "Teoría desde cero (P1): intensidad del sonido en 3D", "ondas", 1,
        claves=["sonido", "intensidad", "esfera", "inverso del cuadrado", "desde cero"],
        enunciado="Por qué el sonido se debilita con el cuadrado de la distancia y cómo usar las casillas nuevas del II-2026.",
    )
    L.paso("La potencia se reparte en una esfera cada vez más grande")\
        .por_que("Estas dos casillas son nuevas en el II-2026 y ya salieron en el I parcial ordinario (el problema del dron).")\
        .di("Mira la esfera 3D alrededor de la fuente: toda la potencia sonora pe cruza la superficie cuatro pi erre cuadrado.",
            F(r"I = \frac{P}{4\pi r^2}"))\
        .di("Si te alejas al doble de distancia, el área se cuadruplica y la intensidad cae a la cuarta parte.",
            F(r"\frac{I_1}{I_2} = \frac{r_2^2}{r_1^2}"))\
        .pregunta_rapida("Si te acercas a la mitad de la distancia a un parlante puntual, la intensidad sonora se vuelve…",
                         ["4 veces mayor", "2 veces mayor", "La mitad"], 0,
                         "I es inversamente proporcional a r²: (1/2)² = 1/4 abajo ⇒ 4 veces mayor.")
    L.paso("Despejar una distancia desconocida sin hallar P")\
        .por_que("Cuando te dan dos intensidades y una distancia, la razón elimina P y 4π de golpe.")\
        .di("Despejas erre dos de la razón y sacas raíz cuadrada a ambos lados.",
            R(r"r_2 = r_1\sqrt{\frac{I_1}{I_2}}"))
    L.te_toca("A $6{,}0\\ \\text{m}$ de un dron la intensidad es $0{,}25\\ \\text{W/m}^2$. ¿A qué distancia la intensidad sube a $1{,}0\\ \\text{W/m}^2$?",
              3.0, "m", pista="r₂ = r₁·√(I₁/I₂) = 6,0·√(0,25/1,0) = 6,0·0,5.", solucion="r₂ = 3,0 m.")
    L.usa(r"I = \frac{P}{4\pi r^2}", r"\frac{I_1}{I_2} = \frac{r_2^2}{r_1^2}")
    return L


# ============================================================================ PARCIAL 2 DESDE CERO

@leccion("T-HIDRO")
def t_hidro() -> Leccion:
    L = _teo(
        "T-HIDRO", "Teoría desde cero (P2): presión hidrostática y manómetros 3D", "fluidos", 2,
        claves=["presión", "hidrostática", "manómetro", "densidad", "desde cero"],
        enunciado="Cómo crece la presión con la profundidad y el método infalible de caminar paso a paso por un manómetro multilíquido.",
    )
    L.paso("Más hondo significa más peso de líquido encima")\
        .por_que("La presión en reposo no depende de la forma del recipiente: solo de la densidad y de la profundidad vertical.")\
        .di("La densidad dice cuánta masa cabe en un metro cúbico. Al bajar una profundidad hache, la presión sube ro ge hache.",
            F(r"\rho = \frac{m}{V}"),
            F(r"p = p_0 + \rho gh"))\
        .di("La presión manométrica es simplemente la presión absoluta menos la atmosférica.",
            F(r"p_{\text{man}} = p_{\text{abs}} - p_{\text{atm}}"))
    L.paso("Cómo recorrer cualquier manómetro 3D sin equivocarte de signo")\
        .por_que("En el II parcial ordinario II-2026 un manómetro de 4 líquidos valió 20 puntos completos.")\
        .di("Empieza en un punto conocido y camina por el tubo siguiendo el modelo 3D.",
            F(r"p_2 - p_1 = -\rho g(y_2 - y_1)"),
            LISTA([
                "Si **bajas** una altura $\\Delta h$ en un líquido: **sumas** $+\\rho g\\Delta h$",
                "Si **subes** una altura $\\Delta h$ en un líquido: **restas** $-\\rho g\\Delta h$",
                "A la misma altura dentro del **mismo líquido continuo**: la presión es idéntica",
            ]))\
        .pregunta_rapida("Al subir 0,50 m dentro de un tubo lleno de mercurio, ¿qué le ocurre a la presión?",
                         ["Disminuye en ρ·g·0,50", "Aumenta en ρ·g·0,50", "Queda igual"], 0,
                         "Subir dentro de un líquido en reposo siempre disminuye la presión.")
    L.te_toca("¿Cuál es la presión manométrica (en kPa) a $5{,}0\\ \\text{m}$ de profundidad en agua ($\\rho = 1000\\ \\text{kg/m}^3$, $g = 9{,}8\\ \\text{m/s}^2$)?",
              49.0, "kPa", pista="p_man = ρ g h = 1000 · 9,8 · 5,0 = 49 000 Pa = 49 kPa.", solucion="p_man = 49,0 kPa.")
    L.usa(r"\rho = \frac{m}{V}", r"p = p_0 + \rho gh", r"p_{\text{man}} = p_{\text{abs}} - p_{\text{atm}}", r"p_2 - p_1 = -\rho g(y_2 - y_1)")
    return L


@leccion("T-FLOT")
def t_flot() -> Leccion:
    L = _teo(
        "T-FLOT", "Teoría desde cero (P2): flotación y empuje de Arquímedes", "fluidos", 2,
        claves=["flotación", "empuje", "Arquímedes", "volumen sumergido", "desde cero"],
        enunciado="Por qué flotan los objetos, qué volumen entra en F_B = ρgV y cómo resolver cables y dos líquidos en 3D.",
    )
    L.paso("El empuje es el peso del líquido que desplazaste")\
        .por_que("Confundir la densidad del objeto con la densidad del líquido es el error clásico en flotación.")\
        .di("Mira las flechas verde y roja en el tanque 3D: el líquido empuja hacia arriba con una fuerza igual al peso del líquido desplazado.",
            F(r"F_B = \rho g V"),
            OJO("Aquí $\\rho$ es SIEMPRE la densidad del **líquido**, y $V$ es SOLO el volumen **sumergido**."))
    L.paso("Si flota quieto: el empuje iguala al peso")\
        .por_que("De esa sola igualdad sale la fracción hundida y por qué el empuje no cambia al pasar de agua a aceite.")\
        .di("Si el cuerpo flota, la suma de fuerzas es cero: empuje igual a peso.",
            M(r"F_B = mg \;\Rightarrow\; \rho_{\text{líq}}\,g\,V_{\text{sum}} = \rho_{\text{obj}}\,V_{\text{tot}}\,g"))\
        .di("Se cancela ge y queda la fracción sumergida.",
            R(r"\frac{V_{\text{sum}}}{V_{\text{tot}}} = \frac{\rho_{\text{obj}}}{\rho_{\text{líq}}}"))
    L.paso("Cuando además hay un cable jalando hacia arriba")\
        .por_que("Si el objeto no flota solo (es más denso que el agua), el cable ayuda al empuje a sostener el peso.")\
        .di("Sumas las dos fuerzas que apuntan arriba e igualas al peso.",
            M(r"T + F_B = mg \;\Rightarrow\; T = mg - F_B"))\
        .pregunta_rapida("Un bloque de madera flota en agua y luego flota en aceite menos denso. ¿Qué pasa con el empuje F_B?",
                         ["Permanece igual (siempre iguala al peso del bloque)", "Disminuye", "Aumenta"], 0,
                         "Como en ambos líquidos flota en equilibrio, F_B = mg no cambia; lo que aumenta es el volumen sumergido.")
    L.te_toca("Un cubo de densidad $700\\ \\text{kg/m}^3$ flota en agua ($1000\\ \\text{kg/m}^3$). ¿Qué porcentaje de su volumen queda sumergido?",
              70.0, "%", pista="Fracción sumergida = ρ_obj / ρ_líq = 700 / 1000 = 0,70 = 70 %.", solucion="70 % sumergido.")
    L.usa(r"F_B = \rho g V")
    return L


@leccion("T-BERN")
def t_bern() -> Leccion:
    L = _teo(
        "T-BERN", "Teoría desde cero (P2): caudal, continuidad y Bernoulli en 3D", "fluidos", 2,
        claves=["caudal", "continuidad", "Bernoulli", "Venturi", "Torricelli", "desde cero"],
        enunciado="Entiende el tubo Venturi y los tanques con orificio en 3D: donde el tubo se angosta, el fluido acelera y la presión baja.",
    )
    L.paso("Caudal y continuidad: lo que entra tiene que salir")\
        .por_que("Antes de aplicar Bernoulli casi siempre necesitas sacar las rapideces v₁ y v₂ con continuidad.")\
        .di("El caudal mide cuántos metros cúbicos pasan por segundo. Si dan kilogramos por segundo, divides entre densidad.",
            F(r"\frac{dV}{dt} = Av"),
            F(r"\frac{dm}{dt} = \rho \frac{dV}{dt}"))\
        .di("En un tubo lleno, si el área se reduce, las partículas aceleran para que pase el mismo caudal.",
            F(r"A_1 v_1 = A_2 v_2"),
            NOTA("Si el diámetro se parte a la mitad, el área $A=\\pi r^2$ cae a la **cuarta parte**, así que la rapidez se **cuadruplica**."))
    L.paso("Bernoulli: conservación de energía por unidad de volumen")\
        .por_que("Elegir bien los puntos 1 y 2 y tachar lo que es cero o igual resuelve cualquier tanque o Venturi.")\
        .di("La suma de presión, energía potencial y energía cinética vale lo mismo en el punto uno y en el dos.",
            F(r"p_1 + \rho g y_1 + \frac{1}{2}\rho v_1^2 = p_2 + \rho g y_2 + \frac{1}{2}\rho v_2^2"))\
        .di("En un Venturi horizontal, ye uno igual a ye dos: donde la rapidez sube, la presión tiene que bajar.",
            OJO("En la superficie de un tanque grande, $v_1 \\approx 0$ y $p_1 = p_{\\text{atm}}$. Si usas $v=\\sqrt{2gh}$, demuéstralo desde Bernoulli."))\
        .pregunta_rapida("En un tubo horizontal que se angosta de diámetro 6 cm a 3 cm, ¿cómo cambia la rapidez?",
                         ["Se multiplica por 4", "Se multiplica por 2", "Se divide entre 2"], 0,
                         "A ∝ D²: al dividir D entre 2, A se divide entre 4, así que v₂ = 4 v₁.")
    L.te_toca("Por un tubo de área $A_1 = 0{,}04\\ \\text{m}^2$ circula agua a $v_1 = 1{,}5\\ \\text{m/s}$. Si se angosta a $A_2 = 0{,}01\\ \\text{m}^2$, ¿cuánto vale $v_2$?",
              6.0, "m/s", pista="A₁v₁ = A₂v₂ ⇒ v₂ = (0,04 / 0,01) · 1,5.", solucion="v₂ = 4 · 1,5 = 6,0 m/s.")
    L.usa(r"\frac{dV}{dt} = Av", r"\frac{dm}{dt} = \rho \frac{dV}{dt}", r"A_1 v_1 = A_2 v_2",
          r"p_1 + \rho g y_1 + \frac{1}{2}\rho v_1^2 = p_2 + \rho g y_2 + \frac{1}{2}\rho v_2^2")
    return L


@leccion("T-VISC")
def t_visc() -> Leccion:
    L = _teo(
        "T-VISC", "Teoría desde cero (P2): Poiseuille, Reynolds, Stokes y tensión", "fluidos", 2,
        claves=["viscosidad", "Poiseuille", "Reynolds", "Stokes", "tensión superficial", "desde cero"],
        enunciado="Fluidos reales con rozamiento interno (perfil parabólico en 3D), rapidez terminal y la casilla nueva de tensión superficial.",
    )
    L.paso("Poiseuille y Reynolds: fluidos viscosos en tubos")\
        .por_que("Cuando el enunciado menciona viscosidad η o aceite, ya NO aplica Bernoulli: se usa Poiseuille.")\
        .di("Mira el perfil parabólico en el tubo 3D: el fluido se pega a la pared y corre más rápido en el centro.",
            F(r"\frac{dV}{dt} = \frac{(p_1 - p_2)\pi R^4}{8\eta L}"),
            F(r"v(r) = \frac{p_1 - p_2}{4\eta L}(R^2 - r^2)"))\
        .di("El número de Reynolds confirma si el flujo es laminar, por debajo de dos mil, o turbulento.",
            F(r"N_R = \frac{\rho v D}{\eta}"),
            OJO("Ojo con el exponente $R^4$: usa siempre el **radio** en metros, nunca el diámetro."))
    L.paso("Esferas cayendo (Stokes) y tensión superficial")\
        .por_que("La casilla γ = F/(2l) es nueva en II-2026 y ya apareció en el II parcial extraordinario.")\
        .di("Una bolita cayendo en aceite alcanza rapidez terminal constante cuando empuje más arrastre igualan al peso.",
            F(r"F_S = 6\pi r \eta v"),
            F(r"v_T = \frac{2}{9}\frac{r^2 g}{\eta}(\rho - \rho')"))\
        .di("En un marco de alambre con película jabonosa, hay dos superficies jalando el alambre: por eso va un dos abajo.",
            F(r"\gamma = \frac{F}{2l}"))\
        .pregunta_rapida("En la ley de Poiseuille, si el radio R del tubo se duplica con igual Δp y L, el caudal se multiplica por…",
                         ["16", "4", "2"], 0,
                         "El caudal depende de R⁴, y 2⁴ = 16.")
    L.te_toca("Si una película jabonosa de ancho $l = 0{,}050\\ \\text{m}$ necesita una fuerza $F = 0{,}0040\\ \\text{N}$ para sostener el alambre, ¿cuánto vale $\\gamma$?",
              0.04, "N/m", pista="Usa γ = F / (2l) = 0,0040 / (2 · 0,050).", solucion="γ = 0,0040 / 0,10 = 0,040 N/m.")
    L.usa(r"\frac{dV}{dt} = \frac{(p_1 - p_2)\pi R^4}{8\eta L}", r"N_R = \frac{\rho v D}{\eta}",
          r"F_S = 6\pi r \eta v", r"\gamma = \frac{F}{2l}")
    return L
