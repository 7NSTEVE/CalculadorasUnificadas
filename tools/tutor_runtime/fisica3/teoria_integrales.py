"""Integrales desde cero, solo las que salen en la reposición (T-INT0, T-INT1, T-PARED, T-INT2).

Para quien no sabe integrar ni relacionar la geometría con la integral. Orden fijo:
  T-INT0  la idea, sin reglas: partir la pared en franjas y sumar (la suma se acerca al valor exacto);
  T-INT1  una sola regla (la de la potencia) y «arriba menos abajo»;
  T-PARED la receta de 5 pasos para la fuerza sobre una pared: rectángulo, trapecio y parábola;
  T-INT2  la integral del logaritmo para la conducción con k(y) = a + by (el posible 4.º problema).

Todos los números vienen de problemas reales: la represa de la Reposición II-2025, la compuerta de
la Reposición I-2024, la ventana del II parcial extraordinario II-2026 y la lámina de la Reposición II-2025.
Un movimiento por renglón.
"""

from __future__ import annotations

import math

from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V, VISUAL
from .formato import tex
from .registro import leccion

G = 9.80
RHO = 1000.0


def _teo(id: str, titulo: str, parcial: int, **k) -> Leccion:
    return Leccion(id, titulo, tema="teoria", tipo="teoria", nivel=1, parcial=parcial,
                   fuente={"texto": "Teoría · integrales de la reposición"}, **k)


# ============================================================================ T-INT0
@leccion("T-INT0")
def t_int0() -> Leccion:
    H, Lw = 4.00, 2.00
    k = RHO * G * Lw              # 19 600
    s4 = k * 1.00 * (0 + 1 + 2 + 3)                 # 117 600
    s10 = k * 0.400 * (0.4 * sum(range(10)))        # 141 120
    s100 = k * 0.0400 * (0.04 * sum(range(100)))    # 155 232
    exacto = 0.5 * RHO * G * Lw * H ** 2            # 156 800

    L = _teo("T-INT0", "Integrar es sumar franjas (sin reglas todavía)", 2,
             claves=["integral", "franjas", "sumar", "desde cero", "pared", "presión"],
             enunciado=(
                 "Una pared rectangular de 2,00 m de ancho tiene agua de un lado hasta 4,00 m de profundidad. "
                 "¿Con qué fuerza empuja el agua? La presión no es la misma arriba que abajo, así que no alcanza con "
                 "presión por área. Aquí se ve qué hace la integral, sin ninguna regla."),
             visual={"tipo": "pared", "forma": "rectangulo", "H": H, "ancho": Lw,
                     "titulo": "Pared de 2,00 m de ancho y 4,00 m de agua"})
    L.paso("1. El problema: la presión cambia con la profundidad")\
        .di("Arriba, en la superficie, el agua casi no empuja. Abajo empuja mucho. La presión crece con la profundidad hache.",
            F(r"p=\rho g h"))\
        .di("Por eso no podemos multiplicar una sola presión por toda el área: ¿cuál presión usaríamos, la de arriba o la de abajo?",
            OJO("Fuerza = presión × área solo vale si la presión es la misma en toda el área."))
    L.paso("2. La idea: partir la pared en franjas horizontales")\
        .di("Partimos la pared en franjas horizontales delgadas. Dentro de una franja, toda el agua está casi a la misma profundidad.",
            V(modo="franja"),
            NOTA("Mueva el control «Profundidad de la franja»: la franja baja y su presión sube."))\
        .di("Entonces cada franja sí tiene una sola presión, y su fuerza es presión por el área de la franja.",
            M(r"\text{fuerza de una franja}=p\cdot(\text{ancho}\cdot\text{alto})"),
            M(r"dF=p\cdot L\cdot dh"))
    L.paso("3. Sumar 4 franjas de 1 metro")\
        .di("Probamos con cuatro franjas de un metro. Para cada una usamos la presión de su borde de arriba: cero, uno, dos y tres metros de profundidad.",
            V(modo="suma", n=4),
            M(r"F\approx\rho g L\,\Delta h\,(h_1+h_2+h_3+h_4)"),
            M(rf"F\approx(1000)(9{{,}}80)(2{{,}}00)(1{{,}}00)(0+1+2+3)={tex(s4, 6)}\ \text{{N}}"))\
        .di("Se queda corto, porque el borde de arriba es la parte con menos presión de cada franja.")
    L.paso("4. Franjas más finas: la suma se acerca")\
        .di("Con diez franjas de cero coma cuarenta metros, la suma de las profundidades de arriba da dieciocho.",
            V(modo="suma", n=10),
            M(rf"F\approx(1000)(9{{,}}80)(2{{,}}00)(0{{,}}400)(18{{,}}0)={tex(s10, 6)}\ \text{{N}}"))\
        .di("Con cien franjas el error es casi nada. Mueva el control «Franjas para sumar» y mire cómo se acerca al valor exacto.",
            TABLA(["franjas", "4", "10", "100", "infinitas"],
                  [["suma (N)", tex(s4, 6), tex(s10, 6), tex(s100, 6), tex(exacto, 6)]]))
    L.paso("5. Eso es la integral")\
        .di("La integral es esa suma con franjas infinitamente delgadas. El símbolo es una ese alargada: significa sumar.",
            M(r"\int=\text{sumar todas las franjas}"),
            M(r"F=\int_0^H \rho g h\,L\,dh"))\
        .di("Los números de abajo y de arriba dicen desde dónde y hasta dónde se suma: desde la superficie hasta el fondo.",
            R(rf"F={tex(exacto, 6)}\ \text{{N}}"),
            NOTA("En la próxima lección se calcula ese valor exacto con una sola regla."))\
        .pregunta_rapida("¿Por qué la suma con 4 franjas da menos que el valor exacto?",
                         ["Porque usé la presión del borde de arriba, que es la menor de cada franja",
                          "Porque el agua pesa menos arriba", "Porque faltó una franja"], 0,
                         "En cada franja la presión crece hacia abajo; el borde de arriba es la presión más chica.")
    L.usa(r"p=\rho g h", r"F_\perp=\int p\,dA")
    return L


# ============================================================================ T-INT1
@leccion("T-INT1")
def t_int1() -> Leccion:
    H, Lw = 4.00, 2.00
    exacto = RHO * G * Lw * 8.0
    L = _teo("T-INT1", "Una sola regla para integrar: la potencia, y «arriba menos abajo»", 2,
             claves=["regla de la potencia", "integral", "límites", "arriba menos abajo", "desde cero"],
             enunciado=(
                 "La única regla que hace falta para la fuerza del agua sobre paredes y compuertas. "
                 "Primero solo con números; al final, la pared de la lección anterior."),
             visual={"tipo": "funcion", "expr": "x", "x_min": 0, "x_max": 4, "riemann": True, "exact_value": 8,
                     "x_label": "h", "y_label": "h", "title": "Área bajo h, de 0 a 4 = 8"})
    L.paso("1. La regla de la potencia")\
        .di("Para integrar una potencia: al exponente se le suma uno, y se divide entre ese exponente nuevo.",
            M(r"\int h^n\,dh=\frac{h^{n+1}}{n+1}"))\
        .di("Hache sola es hache a la uno. El exponente pasa a dos y se divide entre dos.",
            M(r"\int h\,dh=\int h^1\,dh=\frac{h^{1+1}}{1+1}=\frac{h^2}{2}"))\
        .di("Un número solo es como hache a la cero: da ese número por hache.",
            M(r"\int dh=\int h^0\,dh=\frac{h^{0+1}}{0+1}=h"))
    L.paso("2. Más ejemplos, uno por renglón")\
        .di("Con hache al cuadrado, el exponente pasa a tres y se divide entre tres.",
            M(r"\int h^2\,dh=\frac{h^{2+1}}{2+1}=\frac{h^3}{3}"))\
        .di("La raíz es la potencia un medio. Un medio más uno es tres medios.",
            M(r"\sqrt{h}=h^{1/2}"),
            M(r"\int h^{1/2}\,dh=\frac{h^{3/2}}{3/2}=\frac{2}{3}h^{3/2}"),
            NOTA("Dividir entre 3/2 es lo mismo que multiplicar por 2/3."))
    L.paso("3. Los números fijos salen afuera")\
        .di("Lo que no cambia de franja a franja, como ro, ge o un ancho fijo, sale de la integral y solo multiplica.",
            M(r"\int 5\,h\,dh=5\int h\,dh=5\,\frac{h^2}{2}"))\
        .di("Si hay una suma o una resta, se integra cada parte por separado.",
            M(r"\int(5{,}45-y)\,dy=\int 5{,}45\,dy-\int y\,dy"),
            M(r"\int(5{,}45-y)\,dy=5{,}45\,y-\frac{y^2}{2}"))
    L.paso("4. Con límites: lo de arriba menos lo de abajo")\
        .di("Se escribe el resultado entre corchetes, se pone el número de arriba, después el de abajo, y se restan.",
            M(r"\int_0^4 h\,dh=\left[\frac{h^2}{2}\right]_0^4"),
            M(r"\frac{4^2}{2}-\frac{0^2}{2}=8-0=8"))\
        .di("En el dibujo, ese ocho es el área bajo la recta, de cero a cuatro. Mueva el control de rectángulos.",
            V(foco="area"))
    L.paso("5. La pared de la lección anterior, exacta")\
        .di("Ro, ge y el ancho salen afuera. Lo que queda es justo la integral que acabamos de hacer.",
            F(r"F_\perp=\int p\,dA"),
            M(r"F=\int_0^4\rho g h\,L\,dh=\rho g L\int_0^4 h\,dh"),
            M(rf"F=(1000)(9{{,}}80)(2{{,}}00)(8)={tex(exacto, 6)}\ \text{{N}}"),
            R(rf"F={tex(exacto, 6)}\ \text{{N}}"))\
        .pregunta_rapida("¿Cuánto da la integral de 0 a 3 de h dh?", ["4,5", "9", "3"], 0,
                         "$\\frac{3^2}{2}-\\frac{0^2}{2}=\\frac{9}{2}=4{,}5$.")
    L.usa(r"F_\perp=\int p\,dA")
    return L


# ============================================================================ T-PARED
@leccion("T-PARED")
def t_pared() -> Leccion:
    # Trapecio: represa de la Reposición II-2025 (45 m arriba, 30 m abajo, 50 m de agua)
    Hd, bs, bi = 50.0, 45.0, 30.0
    baja = (bs - bi) / Hd                            # 0,30 m por metro
    t1 = 22.5 * Hd ** 2                              # 56 250
    t2 = 0.1 * Hd ** 3                               # 12 500
    F_trap = RHO * G * (t1 - t2)                     # 428 750 000
    # Parábola: compuerta de la Reposición I-2024 (y = 2x², H = 4 m), mitad parabólica
    a1 = (8.0 / 3.0) * 8.0                           # 64/3
    a2 = (2.0 / 5.0) * 32.0                          # 64/5
    F_par = RHO * G / math.sqrt(2.0) * (a1 - a2)     # 59 133

    L = _teo("T-PARED", "La fuerza del agua sobre una pared en 5 pasos: rectángulo, trapecio y parábola", 2,
             claves=["fuerza hidrostática", "franja", "trapecio", "parábola", "represa", "compuerta", "receta"],
             enunciado=(
                 "La receta fija para el problema de la pared, que salió en las reposiciones I-2024 (compuerta con parábola) "
                 "y II-2025 (represa trapezoidal). Siempre los mismos 5 pasos; lo único que cambia es el ancho de la franja."),
             visual={"tipo": "pared", "forma": "trapecio", "H": Hd, "ancho_sup": bs, "ancho_inf": bi,
                     "titulo": "Represa: 45 m arriba, 30 m abajo, 50 m de agua"})
    L.paso("La receta de 5 pasos")\
        .di("Estos cinco pasos sirven para cualquier pared. Escríbalos siempre en el mismo orden.",
            LISTA([
                "1. Dibujar una franja horizontal a profundidad h, de alto dh.",
                "2. Escribir su ancho L(h).",
                "3. Escribir su presión: p = ρgh.",
                "4. Armar dF = p·L(h)·dh.",
                "5. Sumar de arriba (h = 0) a abajo (h = H) con la regla de la potencia.",
            ]),
            F(r"p=\frac{dF_\perp}{dA}"),
            F(r"F_\perp=\int p\,dA"))
    L.paso("Trapecio, paso 2: el ancho de la franja")\
        .di("La represa mide cuarenta y cinco metros arriba y treinta abajo. En cincuenta metros de bajada pierde quince metros de ancho.",
            V(modo="franja"),
            M(r"45{,}0-30{,}0=15{,}0\ \text{m}"),
            M(rf"\frac{{15{{,}}0}}{{50{{,}}0}}={tex(baja, 2)}\ \text{{m por cada metro de profundidad}}"))\
        .di("El ancho arranca en cuarenta y cinco y baja cero coma treinta por cada metro.",
            M(rf"L(h)=45{{,}}0-{tex(baja, 2)}\,h"),
            M(rf"L(50)=45{{,}}0-({tex(baja, 2)})(50)=30{{,}}0\ \text{{m}}"))
    L.paso("Trapecio, pasos 3 y 4: presión y fuerza de la franja")\
        .di("La presión neta es ro ge hache: la atmosférica empuja igual del otro lado y se cancela.",
            F(r"p=p_0+\rho g h"),
            M(r"dF=\rho g h\,(45-0{,}30\,h)\,dh"))\
        .di("Multiplicamos hache por el paréntesis para que queden potencias sueltas.",
            M(r"h\,(45-0{,}30\,h)=45\,h-0{,}30\,h^2"))
    L.paso("Trapecio, paso 5: integrar cada potencia")\
        .di("Hache da hache al cuadrado entre dos. Hache al cuadrado da hache al cubo entre tres.",
            M(r"\int 45\,h\,dh=45\,\frac{h^2}{2}=22{,}5\,h^2"),
            M(r"\int 0{,}30\,h^2\,dh=0{,}30\,\frac{h^3}{3}=0{,}10\,h^3"))\
        .di("Arriba menos abajo, de cero a cincuenta. En cero todo vale cero.",
            M(rf"22{{,}}5\,(50)^2={tex(t1, 6)}"),
            M(rf"0{{,}}10\,(50)^3={tex(t2, 6)}"),
            M(rf"F=(1000)(9{{,}}80)({tex(t1, 6)}-{tex(t2, 6)})={tex(F_trap, 4)}\ \text{{N}}"),
            R(rf"F={tex(F_trap, 4)}\ \text{{N}}"))
    L.paso("Parábola: el ancho sale de la curva")\
        .di("En la compuerta de I-2024 la curva es ye igual dos equis cuadrado, con ye medida desde el fondo. El ancho de la franja es equis: se despeja.",
            VISUAL("pared", forma="mixta", H=4.0, rect=1.0, k=2.0, titulo="Compuerta: mitad rectángulo y mitad parábola y = 2x²"),
            M(r"y=2x^2\;\Rightarrow\;x^2=\frac{y}{2}\;\Rightarrow\;x=\sqrt{\frac{y}{2}}"))\
        .di("Midiendo desde el fondo, la profundidad de la franja es cuatro menos ye.",
            M(r"dF=\rho g\,(4-y)\sqrt{\frac{y}{2}}\,dy"),
            M(r"\sqrt{\frac{y}{2}}=\frac{y^{1/2}}{\sqrt{2}}"))
    L.paso("Parábola: potencias sueltas e integral")\
        .di("Multiplicamos: ye por ye a la un medio suma exponentes, uno más un medio es tres medios.",
            M(r"(4-y)\,y^{1/2}=4\,y^{1/2}-y^{3/2}"),
            M(r"\int 4\,y^{1/2}\,dy=4\,\frac{y^{3/2}}{3/2}=\frac{8}{3}\,y^{3/2}"),
            M(r"\int y^{3/2}\,dy=\frac{y^{5/2}}{5/2}=\frac{2}{5}\,y^{5/2}"))\
        .di("En cuatro: la raíz de cuatro es dos, y dos al cubo es ocho y dos a la quinta es treinta y dos.",
            M(r"4^{3/2}=2^3=8\qquad 4^{5/2}=2^5=32"),
            M(rf"F_{{\text{{parábola}}}}=\frac{{9800}}{{\sqrt{{2}}}}\left(\frac{{8}}{{3}}\cdot 8-\frac{{2}}{{5}}\cdot 32\right)={tex(F_par, 5)}\ \text{{N}}"),
            R(rf"F_{{\text{{parábola}}}}={tex(F_par, 5)}\ \text{{N}}"))\
        .pregunta_rapida("En la represa, ¿cuánto mide la franja a 20 m de profundidad?", ["39 m", "45 m", "30 m"], 0,
                         "$L(20)=45-0{,}30\\cdot 20=45-6=39$ m.")
    L.usa(r"p=\frac{dF_\perp}{dA}", r"F_\perp=\int p\,dA", r"p=p_0+\rho g h")
    return L


# ============================================================================ T-INT2
@leccion("T-INT2")
def t_int2() -> Leccion:
    A, d, a, b = 4.00, 0.0200, 200.0, 1000.0
    T1, T2 = 300.0, 100.0
    u1 = a + b * d                                   # 220
    ln_ = math.log(u1 / a)                           # 0,09531
    integral = ln_ / b                               # 9,531e-5
    Hc = A * (T1 - T2) / integral                    # 8,394e6
    L = _teo("T-INT2", "Conducción con k(y) = a + b·y: la integral del logaritmo y el 1/b que se olvida", 3,
             claves=["conductividad variable", "logaritmo", "cambio de variable", "Fourier", "desde cero"],
             enunciado=(
                 "Lámina de la Reposición II-2025: 4,00 m², 2,00 cm de grosor, 300 °C abajo y 100 °C arriba, "
                 "con k(y) = 200 + 1000y. Solo los pasos de la integral; la lección completa es Q050."),
             visual={"tipo": "funcion", "expr": "1/x", "x_min": 1, "x_max": 4, "shade_from": 1, "shade_to": 2,
                     "x_label": "u", "y_label": "1/u", "title": "Área bajo 1/u de 1 a 2 = ln 2"})
    L.paso("1. Fourier de la hoja con k que cambia")\
        .di("Como ka depende de ye, no sirve la fórmula de pared. Se usa la ley de Fourier con ka escrita adentro.",
            DATO("k(y) = 200 + 1000y"),
            F(r"H=-kA\frac{dT}{dy}"),
            M(r"H=-(200+1000\,y)\,A\,\frac{dT}{dy}"))\
        .di("Separamos: todo lo de ye con de ye, todo lo de te con de te.",
            M(r"\frac{H\,dy}{200+1000\,y}=-A\,dT"))
    L.paso("2. La regla del logaritmo")\
        .di("Uno entre algo da logaritmo natural. Es la única integral que no sale con la regla de la potencia.",
            M(r"\int\frac{du}{u}=\ln u"))\
        .di("Para usarla, al paréntesis le ponemos un nombre: u. Y miramos cuánto cambia u cuando ye cambia un poquito.",
            M(r"u=200+1000\,y"),
            M(r"du=1000\,dy\;\Rightarrow\;dy=\frac{du}{1000}"),
            OJO("Ese 1/1000 es el que se olvida. El solucionario oficial de II-2025 lo olvidó y dio un flujo mil veces menor."))
    L.paso("3. Cambian los límites")\
        .di("Si ye va de cero a cero coma cero dos, u va de doscientos a doscientos veinte.",
            M(r"y=0\;\Rightarrow\;u=200"),
            M(rf"y=0{{,}}0200\;\Rightarrow\;u=200+(1000)(0{{,}}0200)={tex(u1, 3)}"))\
        .di("Ahora la integral de la izquierda queda con u.",
            M(r"\int_0^{0{,}0200}\frac{dy}{200+1000\,y}=\frac{1}{1000}\int_{200}^{220}\frac{du}{u}"),
            M(rf"\frac{{1}}{{1000}}\ln\frac{{220}}{{200}}=\frac{{{tex(ln_, 4)}}}{{1000}}={tex(integral, 4)}\ \text{{m}}^{{2}}\cdot\text{{K/W}}"))
    L.paso("4. Despejar el flujo")\
        .di("La derecha es menos a por el cambio de temperatura: de trescientos a cien grados.",
            M(r"-A\int_{300}^{100}dT=-A\,(100-300)=200\,A"),
            M(rf"H\,({tex(integral, 4)})=(200)(4{{,}}00)"),
            M(rf"H=\frac{{800}}{{{tex(integral, 4)}}}={tex(Hc, 4)}\ \text{{W}}"),
            R(rf"H={tex(Hc, 4)}\ \text{{W}}"))\
        .pregunta_rapida("Si u = 5 + 2y, ¿cuánto vale dy?", ["du/2", "2 du", "du"], 0,
                         "du = 2 dy, entonces dy = du/2.")
    L.usa(r"H=-kA\frac{dT}{dx}")
    return L
