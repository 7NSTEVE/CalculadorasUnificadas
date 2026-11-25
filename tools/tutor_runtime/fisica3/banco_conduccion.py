"""Superexamen Q036–Q062: conducción, resistencias térmicas, geometrías radiales, k variable y convección."""

from __future__ import annotations

import math

from .ayudas import revisar_opciones
from .banco_recomendados import _esfera_hierro
from .dsl import DATO, F, LISTA, M, MARCA, NOTA, OJO, Leccion, R, T, TABLA, V, s
from .formato import dice, n, nu, tex
from .indice_superexamen import fuente, nivel, tema
from .registro import leccion


def _sel(q: str, titulo: str, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), tipo="seleccion", fuente=fuente(q), enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _des(q: str, titulo: str, enunciado: str, **k) -> Leccion:
    return Leccion(q, titulo, tema=tema(q), nivel=nivel(q), fuente=fuente(q), enunciado=enunciado, **k)


# ============================================================================ Q036
@leccion("Q036")
def q036() -> Leccion:
    L = _sel("Q036", "Cuatro láminas del mismo material al calentarse",
             "Cuatro láminas sólidas del mismo material tienen dimensiones: A ($L\\times L$), B ($L\\times 2L$), C ($2L\\times 2L$) y D ($3L\\times L$). Se someten al mismo incremento de temperatura. ¿Cuál lámina experimenta el mayor incremento de área?",
             {"A": "D experimenta el mayor incremento.", "B": "B experimenta el mayor incremento.",
              "C": "A experimenta el mayor incremento.", "D": "C experimenta el mayor incremento."}, "D",
             claves=["dilatación superficial", "área inicial"],
             visual={"tipo": "dilatacion", "caso": "barra", "L0": 2.0, "a": 2e-5, "T0": 20, "dT": 60, "nombre": "lámina C (4L²)"})
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro láminas hechas del mismo material reciben el mismo aumento de temperatura.", DATO("mismo incremento de temperatura"),
            LISTA(["Lámina A: $L\\times L = L^2$", "Lámina B: $L\\times 2L = 2L^2$", "Lámina C: $2L\\times 2L = 4L^2$", "Lámina D: $3L\\times L = 3L^2$"]))\
        .di("Preguntan cuál área crece más.", M(r"\Delta A = \;?"))
    L.paso("Dilatación superficial")\
        .di("El incremento de área es proporcional al área inicial.", V(foco="calentar"), F(r"\Delta A = 2\alpha A_0 \Delta T", "f"))\
        .di("Como el material y el cambio de temperatura son iguales, manda el área inicial.", M(r"\Delta A \propto A_0"))\
        .di("La lámina C tiene área cuatro ele cuadrado, la mayor de todas.",
            TABLA(["Lámina", "Dimensiones", "Área $A_0$"], [["A", "$L\\times L$", "$L^2$"], ["B", "$L\\times 2L$", "$2L^2$"], ["C", "$2L\\times 2L$", "$4L^2$"], ["D", "$3L\\times L$", "$3L^2$"]], resaltar=[[2, 2]]),
            R(r"A_C = 4L^2 > A_D = 3L^2 > A_B = 2L^2 > A_A = L^2"))\
        .por_que("Dos veces alfa y delta T son idénticos para las cuatro; gana la de mayor área inicial.")
    revisar_opciones(L, {
        "A": ("La A no: D tiene área tres ele cuadrado, menos que C.", "$A_D=3L^2<4L^2$."),
        "B": ("La B no: B tiene área dos ele cuadrado.", "$A_B=2L^2$."),
        "C": ("La C no: A es la más pequeña, solo un ele cuadrado.", "$A_A=L^2$, la menor."),
        "D": ("La D es correcta: la lámina C tiene el mayor área inicial.", "$A_C=4L^2$ es la mayor."),
    })
    return L.oficial("II Parcial ordinario II-2023", "D").usa(r"\Delta A=2\alpha A_0\Delta T")


# ============================================================================ Q037
@leccion("Q037")
def q037() -> Leccion:
    k_ac, r1, r2, t1, t2, lc = 50.2, 0.033, 0.042, 80.0, 20.0, 1.5
    h_ac = 2.0 * math.pi * k_ac * lc * (t1 - t2) / math.log(r2 / r1)
    k_cu = 385.0
    razon = k_cu / k_ac
    L = _des("Q037", "Tubería cilíndrica de acero y comparación con cobre",
             "Una tubería cilíndrica de conductividad $k$, radios $R_1$ y $R_2$, longitud $L$, lleva agua a $T_1$ y afuera está a $T_2<T_1$. A) Expresión de $H$. B) Para acero ($k=50{,}2$ W/m·K, $R_1=3{,}3$ cm, $R_2=4{,}2$ cm, $T_1=80\\ ^\\circ$C, $T_2=20\\ ^\\circ$C, $L=1{,}5$ m), calcule $H$. C) Compare cualitativamente con cobre ($k_{\\text{Cu}}=385$ W/m·K).",
             claves=["conducción cilíndrica", "integral", "tubería"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": r1, "r2": r2, "L": lc, "T1": t1, "T2": t2, "k": k_ac, "uT": "°C", "etq1": "R₁", "etq2": "R₂"})
    L.paso("A) Ley de Fourier en un cascarón cilíndrico")\
        .di("El calor sale radialmente desde el radio interior al exterior.", V(foco="dr"), DATO("tubería cilíndrica"),
            F(r"H = -k A(r)\,\frac{dT}{dr}", "f"))\
        .di("El área lateral de un cilindro de radio erre y largo ele es dos pi erre ele.".replace("erre", dice("r", "erre")).replace("ele", dice("L", "ele")),
            M(r"A(r) = 2\pi r L"))\
        .di("Separamos variables e integramos desde el interior hasta el exterior.", V(foco="barrido"),
            M(r"H\int_{R_1}^{R_2}\frac{dr}{r} = -2\pi k L\int_{T_1}^{T_2}dT"))\
        .di("La integral de uno entre erre es el logaritmo natural.",
            M(r"H\ln\left(\frac{R_2}{R_1}\right) = 2\pi k L\,(T_1-T_2)"),
            R(r"H = \frac{2\pi k L\,(T_1-T_2)}{\ln(R_2/R_1)}", "ra"))\
        .por_que("En estado estacionario la misma corriente $H$ cruza cada capa cilíndrica, aunque el área crezca con $r$.")
    L.paso("B) Calcular el flujo en el tubo de acero")\
        .di("Pasamos los radios a metros o usamos la razón directo, porque las unidades se cancelan.",
            LISTA(["$k=50{,}2\\ \\text{W/(m}\\cdot\\text{K)}$, $L=1{,}5$ m", "$R_1=3{,}3$ cm, $R_2=4{,}2$ cm", "$\\Delta T = 80-20 = 60\\ ^\\circ\\text{C}$"]))\
        .di("Sustituimos en la expresión de la parte A.", V(foco="flujo"),
            M(r"H = \frac{2\pi(50{,}2)(1{,}5)(80-20)}{\ln(4{,}2/3{,}3)}"))\
        .di(f"El logaritmo da {n(math.log(r2 / r1), 4)} y el flujo sale {nu(h_ac / 1e3, 'kW', 4)}.",
            R(rf"H = {tex(h_ac, 4, u='W', sci='si')} = {tex(h_ac / 1e3, 4, u='kW')}", "rb"))
    L.paso("C) Comparación con el cobre")\
        .di("Como el flujo es proporcional a la conductividad, solo comparamos las conductividades.", V(foco="perfil"),
            M(rf"\frac{{H_{{\text{{Cu}}}}}}{{H_{{\text{{acero}}}}}} = \frac{{k_{{\text{{Cu}}}}}}{{k_{{\text{{acero}}}}}} = \frac{{385}}{{50{{,}}2}} \approx {tex(razon, 3)}"))\
        .di(f"El tubo de cobre perdería calor casi {n(razon, 2)} veces más rápido que el de acero.",
            R(rf"H_{{\text{{Cu}}}} \approx {tex(razon, 3)}\,H_{{\text{{acero}}}}\ \ (\text{{mucho mayor}})"))
    L.responde("B", rf"H={tex(h_ac / 1e3, 4)}\ \text{{kW}}", h_ac, "W").responde("C", rf"H_{{\text{{Cu}}}}/H_{{\text{{acero}}}}={tex(razon, 3)}", razon, "")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q038
@leccion("Q038")
def q038() -> Leccion:
    k1, k2 = 385.0, 406.0
    razon_sp = (k1 * k2) / ((k1 + k2) ** 2)
    razon_dt = k2 / k1
    L = _des("Q038", "Barras de cobre y plata: serie vs paralelo",
             "Barra 1 de cobre ($k_1=385$) y barra 2 de plata ($k_2=406$), con igual área $A$ y largo $L$, se conectan entre $T_h$ y $T_c$ en serie (Config. 1) y en paralelo (Config. 2). A) Compare $H_1$ y $H_2$. B) En serie, compare $\\Delta T_1$ y $\\Delta T_2$. C) Razón $H_2/H'_2$ al triplicar $k$. D) Razón $H_1/H'_1$ al triplicar $k$.",
             aviso="En el Superexamen se pegó por error un diagrama p-V ajeno; la figura original de 24-I-E muestra las barras en serie (Configuración 1) y en paralelo (Configuración 2).",
             claves=["serie y paralelo", "resistencia térmica", "cobre y plata"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [100, 48.7, 0], "uT": "°C",
                     "capas": [{"n": "Barra 1 (Cu)", "e": 1.0, "k": 385}, {"n": "Barra 2 (Ag)", "e": 1.0, "k": 406}]})
    L.paso("A) Serie frente a paralelo")\
        .di("En la configuración uno las barras están en serie: el calor debe cruzar el doble de distancia.", DATO("CONFIGURACIÓN 1"),
            M(r"H_{\text{serie}} = \frac{A\Delta T}{\frac{L}{k_1}+\frac{L}{k_2}} = \frac{k_1 k_2}{k_1+k_2}\frac{A\Delta T}{L}"))\
        .di("En la configuración dos están en paralelo: el calor viaja por dos caminos a la vez.",
            M(r"H_{\text{paralelo}} = H_1 + H_2 = (k_1+k_2)\frac{A\Delta T}{L}"))\
        .di("En serie el flujo es aproximadamente la cuarta parte que en paralelo: siempre es menor.",
            R(r"H_1 < H_2 \quad \text{(menor)}", "ra"))\
        .por_que("En serie las resistencias térmicas se suman; en paralelo hay dos rutas directas entre $T_h$ y $T_c$.")
    L.paso("B) Caída de temperatura en serie")\
        .di("En serie pasa exactamente la misma corriente de calor por ambas barras.", V(foco="perfil"),
            M(r"H = \frac{k_1 A \Delta T_1}{L} = \frac{k_2 A \Delta T_2}{L}"))\
        .di("Por eso la caída de temperatura es inversamente proporcional a la conductividad.",
            M(rf"\frac{{\Delta T_1}}{{\Delta T_2}} = \frac{{k_2}}{{k_1}} = \frac{{406}}{{385}} \approx {tex(razon_dt, 3)} > 1"))\
        .di("El cobre conduce un poco menos que la plata, así que cae más temperatura en la barra uno.",
            R(r"\Delta T_1 > \Delta T_2 \quad \text{(mayor)}", "rb"))
    L.paso("C y D) Al triplicar las conductividades")\
        .di("Tanto en paralelo como en serie, si ambas conductividades se triplican, el flujo se triplica.",
            M(r"H'_{\text{paralelo}} = (3k_1+3k_2)\frac{A\Delta T}{L} = 3H_{\text{paralelo}}"),
            M(r"H'_{\text{serie}} = \frac{(3k_1)(3k_2)}{3k_1+3k_2}\frac{A\Delta T}{L} = 3H_{\text{serie}}"))\
        .di("La razón entre el flujo original y el nuevo es un tercio en los dos casos.",
            R(r"\frac{H_2}{H'_2} = \frac{1}{3} \approx 0{,}333 \qquad \frac{H_1}{H'_1} = \frac{1}{3} \approx 0{,}333", "rcd"))
    L.responde("C", r"\frac{1}{3}", 1.0 / 3.0, "").responde("D", r"\frac{1}{3}", 1.0 / 3.0, "")
    return L.usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q039
@leccion("Q039")
def q039() -> Leccion:
    k, L_b, th, tc = 385.0, 2.00, 80.0, 20.0
    a1 = 0.10 ** 2
    h1 = k * a1 * (th - tc) / L_b
    t_500 = 500.0 / h1
    # B: A(x) = 0.01 / (1 + x/2.0), x=0 frío (20°C), x=2.0 caliente (80°C)
    # I(x) = int_0^x (1 + u/2)/0.01 du = 100*(x + x^2/4)
    I_tot = 100.0 * (L_b + L_b ** 2 / 4.0)
    I_mid = 100.0 * (1.0 + 1.0 ** 2 / 4.0)
    t_mid = tc + (th - tc) * (I_mid / I_tot)
    pts = [[round(2.0 * i / 40, 4), round(tc + (th - tc) * (100.0 * (2.0 * i / 40 + (2.0 * i / 40) ** 2 / 4.0)) / I_tot, 2)] for i in range(41)]
    L = _des("Q039", "Barra de cobre uniforme y de área variable A(x)",
             "Una barra de cobre ($k=385$ W/m·K, $L=2{,}00$ m) une reservorios a $80\\ ^\\circ$C y $20\\ ^\\circ$C. A) Sección cuadrada constante de $10$ cm de lado: tiempo para transmitir $500$ J. B) Área $A(x)=\\frac{0{,}01\\ \\text{m}^2}{1+x/2{,}0\\ \\text{m}}$ desde el reservorio frío ($x=0$) al cálido ($x=2{,}00$ m): temperatura en la mitad.",
             claves=["área variable", "integral", "barra de cobre"],
             visual={"tipo": "conduccion", "geo": "pared", "L": L_b, "T1": tc, "T2": th, "eje": "x", "uT": "°C",
                     "forma": "1 / (1 + u)", "pts": pts, "xm": 1.0, "etA": "A(x)"})
    L.paso("A) Sección constante de 10 cm de lado")\
        .di("El área es un cuadrado de cero coma diez metros de lado.", DATO("10 cm de lado"),
            M(r"A = (0{,}10\ \text{m})^2 = 0{,}0100\ \text{m}^2"))\
        .di("Calculamos el flujo de calor por Fourier.",
            F(r"H = \frac{k A (T_H-T_C)}{L}"),
            M(rf"H = \frac{{(385)(0{{,}}0100)(80-20)}}{{2{{,}}00}} = {tex(h1, 4, u='W')}"))\
        .di(f"El tiempo para quinientos joules es calor entre flujo: {nu(t_500, 's', 3)}.",
            M(rf"t = \frac{{Q}}{{H}} = \frac{{500}}{{{tex(h1, 4)}}}"), R(rf"t = {tex(t_500, 3, u='s')}", "ra"))
    L.paso("B) Separar variables con área variable")\
        .di("Como el eje equis va del foco frío al caliente, la temperatura sube con equis.", V(foco="dx"),
            F(r"H = k A(x)\,\frac{dT}{dx} \;\Rightarrow\; \frac{H}{k}\frac{dx}{A(x)} = dT"))\
        .di("Al invertir el área, el denominador pasa arriba y la integral queda polinomial.",
            M(r"\frac{1}{A(x)} = \frac{1 + x/2{,}0}{0{,}01} = 100\left(1 + \frac{x}{2{,}0}\right)"),
            NOTA("Nota: aunque el texto dice 'se ensancha', la fórmula dada disminuye de $0{,}01$ a $0{,}005\\ \\text{m}^2$."))
    L.paso("Integrar hasta el final y hasta la mitad")\
        .di("Integramos en toda la barra para obtener la resistencia geométrica total.", V(foco="barrido"),
            M(rf"I(2{{,}}00) = 100\int_0^{{2{{,}}00}}\left(1+\frac{{x}}{{2{{,}}0}}\right)dx = 100\left[x+\frac{{x^2}}{{4}}\right]_0^2 = {tex(I_tot, 3)}\ \text{{m}}^{{-1}}"))\
        .di("Ahora integramos solo hasta la mitad, en un metro.", V(foco="punto"),
            M(rf"I(1{{,}}00) = 100\left(1 + \frac{{1^2}}{{4}}\right) = {tex(I_mid, 3)}\ \text{{m}}^{{-1}}"))\
        .di("Dividiendo las dos integrales se cancelan hache y ka.",
            M(rf"\frac{{T(1)-20}}{{80-20}} = \frac{{125}}{{300}} = \frac{{5}}{{12}} \;\Rightarrow\; T(1) = 20 + 60\left(\frac{{5}}{{12}}\right)"),
            R(rf"T(1{{,}}00\ \text{{m}}) = {tex(t_mid, dec=1, u='°C')}", "rb"))\
        .por_que("Como el tramo de $x=1$ a $x=2$ es más angosto, ofrece más resistencia y concentra más caída de temperatura.")
    L.responde("A", rf"t={tex(t_500, 3)}\ \text{{s}}", t_500, "s").responde("B", rf"T={tex(t_mid, dec=1)}\ ^\circ\text{{C}}", t_mid, "°C")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q040
@leccion("Q040")
def q040() -> Leccion:
    L = _sel("Q040", "Ordenar cuatro placas en serie por su conductividad",
             "Cuatro placas de igual espesor $d$ en serie tienen temperaturas en sus interfaces de $35\\ ^\\circ$C, $30\\ ^\\circ$C, $20\\ ^\\circ$C, $0\\ ^\\circ$C y $-15\\ ^\\circ$C de izquierda a derecha. Ordene los materiales de menor a mayor conductividad térmica.",
             {"A": "1, 2, 3, 4", "B": "2, 1, 3, 4", "C": "3, 4, 1, 2", "D": "3, 4, 2, 1"}, "D",
             claves=["placas en serie", "conductividad térmica"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [35, 30, 20, 0, -15], "uT": "°C",
                     "capas": [{"n": "Placa 1", "e": 1, "k": 4, "kt": "k₁"}, {"n": "Placa 2", "e": 1, "k": 2, "kt": "k₂"},
                               {"n": "Placa 3", "e": 1, "k": 1, "kt": "k₃"}, {"n": "Placa 4", "e": 1, "k": 1.33, "kt": "k₄"}]})
    L.paso("Caída de temperatura en cada placa")\
        .di("Como están en serie, el flujo de calor es idéntico en las cuatro placas.", DATO("igual espesor"),
            F(r"H = \frac{k A\,\Delta T}{d} \;\Rightarrow\; k = \frac{H d}{A\,\Delta T} \propto \frac{1}{\Delta T}"))\
        .di("A mayor caída de temperatura, peor conductor es el material.",
            OJO("No te confundas: **más** caída de temperatura significa **menor** conductividad $k$."))\
        .di("Restamos las temperaturas de cada interfaz.", V(foco="perfil"),
            TABLA(["Placa", "Extremos", "$\\Delta T$"], [["1", "$35\\to 30$", "$5\\ ^\\circ\\text{C}$"], ["2", "$30\\to 20$", "$10\\ ^\\circ\\text{C}$"], ["3", "$20\\to 0$", "$20\\ ^\\circ\\text{C}$"], ["4", "$0\\to -15$", "$15\\ ^\\circ\\text{C}$"]], resaltar=[[2, 2], [0, 2]]))\
        .di("De mayor a menor caída va tres, cuatro, dos, uno; ese es el orden de menor a mayor conductividad.",
            R(r"k_3 < k_4 < k_2 < k_1"))
    revisar_opciones(L, {
        "A": ("La A ordena por número de placa, al revés de la conductividad.", "La 1 tiene la menor caída, es la mayor $k$."),
        "B": ("La B pone la 1 y 2 como menores, pero son las más conductoras.", "Menor $\\Delta T$ implica mayor $k$."),
        "C": ("La C intercambia el orden final entre uno y dos.", "$\\Delta T_2=10 > \\Delta T_1=5$, así que $k_2 < k_1$."),
        "D": ("La D es la correcta: tres, cuatro, dos, uno.", "$20 > 15 > 10 > 5 \\Rightarrow k_3 < k_4 < k_2 < k_1$."),
    })
    return L.oficial("III Parcial extraordinario II-2024", "D").usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q041
@leccion("Q041")
def q041() -> Leccion:
    k, h, a, b, t1, t2 = 50.0, 2.00, 0.100, 0.450, 100.0, 0.0
    H = math.pi * k * a * b * (t1 - t2) / h
    Q = H * (5.0 * 60.0)
    pts = [[round(h * i / 40, 4), round(t1 - (t1 - t2) * (1 / a - 1 / (a + (b - a) * i / 40)) / (1 / a - 1 / b), 2)] for i in range(41)]
    L = _des("Q041", "Conducción en un cono truncado sólido",
             "Un cono truncado sólido ($k=50{,}0$ W/m·K, altura $h=2{,}00$ m, radios $a=0{,}100$ m a $T_1=100\\ ^\\circ$C y $b=0{,}450$ m a $T_2=0\\ ^\\circ$C) tiene paredes laterales aisladas. A) Corriente de calor $H$. B) Calor transferido en $5{,}00$ min.",
             claves=["cono truncado", "área variable", "integral"],
             visual={"tipo": "conduccion", "geo": "pared", "L": h, "T1": t1, "T2": t2, "eje": "y", "uT": "°C",
                     "forma": "0.22 + 0.78 * u", "pts": pts, "etA": "π r(y)²"})
    L.paso("A) Radio y área a altura y")\
        .di("El radio crece linealmente desde a en la base hasta be arriba.", V(foco="dx"), DATO("cono truncado"),
            M(r"r(y) = a + \frac{b-a}{h}\,y \;\Rightarrow\; dy = \frac{h}{b-a}\,dr"))\
        .di("El área transversal es un círculo de radio erre.", M(r"A(r) = \pi r^2"))\
        .di("Metemos todo en la ley de Fourier y separamos variables.", V(foco="barrido"),
            F(r"H = -k\pi r^2 \frac{dT}{dy}"),
            M(r"\frac{H h}{\pi(b-a)}\int_a^b \frac{dr}{r^2} = -k\int_{T_1}^{T_2}dT"))
    L.paso("Resolver la integral y calcular H")\
        .di("La integral de uno entre erre cuadrado da la resta de inversos.",
            M(r"\frac{H h}{\pi(b-a)}\left(\frac{1}{a}-\frac{1}{b}\right) = k\,(T_1-T_2)"))\
        .di("El factor be menos a se cancela y queda una fórmula muy limpia.",
            M(r"\frac{1}{a}-\frac{1}{b} = \frac{b-a}{ab} \;\Rightarrow\; H = \frac{\pi k a b\,(T_1-T_2)}{h}", "fh", "azul"))\
        .di(f"Sustituimos los datos y sale {nu(H, 'W', 4)}.", V(foco="perfil"),
            M(rf"H = \frac{{\pi(50{{,}}0)(0{{,}}100)(0{{,}}450)(100-0)}}{{2{{,}}00}}"),
            R(rf"H = {tex(H, 4, u='W')}", "ra"))
    L.paso("B) Calor en 5,00 minutos")\
        .di("Cinco minutos son trescientos segundos; en estado estacionario solo multiplicamos.",
            M(r"Q = H\,\Delta t = (353{,}4\ \text{W})(5{,}00 \times 60\ \text{s})"),
            R(rf"Q = {tex(Q, 3, u='J', sci='si')}", "rb"))
    L.responde("A", rf"H={tex(H, 4)}\ \text{{W}}", H, "W").responde("B", rf"Q={tex(Q, 3, sci='si')}\ \text{{J}}", Q, "J")
    return L.oficial("III Parcial extraordinario II-2024", 353.4, 1.06e5).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q042
@leccion("Q042")
def q042() -> Leccion:
    d, k1, L1, k2, k3, L3 = 0.15, 205.0, 0.15, 50.2, 406.0, 0.20
    th, tc, t23 = 100.0, 0.0, 22.2
    A = math.pi * d ** 2 / 4.0
    H = k3 * A * (t23 - tc) / L3
    t12 = th - H * L1 / (k1 * A)
    L2 = k2 * A * (t12 - t23) / H
    L = _des("Q042", "Tres varillas soldadas en serie (Al, acero y Ag)",
             "Tres varillas circulares ($d=0{,}15$ m) en serie: aluminio ($k_1=205{,}0$, $L_1=0{,}15$ m), acero ($k_2=50{,}2$, $L_2=?$) y plata ($k_3=406{,}0$, $L_3=0{,}20$ m). Extremos a $100\\ ^\\circ$C y $0\\ ^\\circ$C; unión acero-plata a $22{,}2\\ ^\\circ$C. A) Temperatura en la unión Al-acero. B) Largo $L_2$ del acero.",
             claves=["tres varillas", "serie", "temperatura de unión"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [th, round(t12, 1), t23, tc], "uT": "°C", "incognitas": [1],
                     "capas": [{"n": "Al", "e": L1, "k": k1}, {"n": "Acero", "e": L2, "k": k2}, {"n": "Ag", "e": L3, "k": k3}]})
    L.paso("Flujo de calor desde la varilla de plata")\
        .di("De la plata conocemos todo: conductividad, largo y sus dos temperaturas.", DATO("22,2 °C"),
            M(rf"A = \frac{{\pi d^2}}{{4}} = \frac{{\pi(0{{,}}15)^2}}{{4}} = {tex(A, 3, u='m^2', sci='si')}"))\
        .di(f"Calculamos el flujo de calor: da {nu(H, 'W', 3)}.",
            F(r"H = \frac{k_3 A\,(T_{23}-T_C)}{L_3}"),
            M(rf"H = \frac{{(406{{,}}0)({tex(A, 3, sci='si')})(22{{,}}2-0)}}{{0{{,}}20}} = {tex(H, 3, u='W')}"))\
        .por_que("Están en serie y aisladas lateralmente, así que ese mismo flujo cruza las tres varillas.")
    L.paso("A) Temperatura en la unión aluminio-acero")\
        .di("Usamos el mismo flujo en el tramo de aluminio y despejamos la temperatura de unión.", V(foco="perfil"),
            M(r"H = \frac{k_1 A\,(T_H - T_{12})}{L_1} \;\Rightarrow\; T_{12} = T_H - \frac{H L_1}{k_1 A}"))\
        .di(f"Sustituimos y resulta {nu(t12, '°C', dec=1)}.",
            M(rf"T_{{12}} = 100{{,}}0 - \frac{{({tex(H, 3)})(0{{,}}15)}}{{(205{{,}}0)({tex(A, 3, sci='si')})}}"),
            R(rf"T_{{12}} = {tex(t12, dec=1, u='°C')}", "ra"))
    L.paso("B) Largo de la varilla de acero")\
        .di("Ahora despejamos la longitud de la barra central de acero.",
            M(r"L_2 = \frac{k_2 A\,(T_{12}-T_{23})}{H}"))\
        .di(f"Con el salto entre las dos uniones obtenemos {nu(L2, 'm', 3)}.",
            M(rf"L_2 = \frac{{(50{{,}}2)({tex(A, 3, sci='si')})({tex(t12, dec=1)}-22{{,}}2)}}{{{tex(H, 3)}}}"),
            R(rf"L_2 = {tex(L2, 3, u='m')}\ \ (\approx 5{{,}}0\ \text{{cm}})", "rb"))
    L.responde("A", rf"T_{{12}}={tex(t12, dec=1)}\ ^\circ\text{{C}}", t12, "°C").responde("B", rf"L_2={tex(L2, 3)}\ \text{{m}}", L2, "m")
    return L.oficial("III Parcial ordinario II-2024", 67.0, 0.050).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q043
@leccion("Q043")
def q043() -> Leccion:
    th, tc, L_b = 80.0, 20.0, 0.20
    r0, r1, ka, kb = 0.0120, 0.0060, 385.0, 50.0
    ha = math.pi * ka * r0 * r1 * (th - tc) / L_b
    hb = kb * math.pi * r1 ** 2 * (th - tc) / L_b
    ht = ha + hb
    L = _des("Q043", "Dos barras en paralelo: cobre cónico y acero cilíndrico",
             "Dos barras de largo $L=0{,}20$ m conectan en paralelo $T_H=80{,}0\\ ^\\circ$C y $T_C=20{,}0\\ ^\\circ$C. Barra A (cobre, $k_A=385$): radio lineal de $R_0=1{,}20$ cm a $R_1=0{,}60$ cm. Barra B (acero, $k_B=50$): radio constante $R_1=0{,}60$ cm. A) $H_A$. B) $H_B$. C) $H_{\\text{total}}$.",
             claves=["paralelo", "cono truncado", "cobre y acero"],
             visual={"tipo": "conduccion", "geo": "paralelo", "L": L_b, "T1": th, "T2": tc, "uT": "°C",
                     "barras": [{"n": "A: Cu cónica", "A": 2, "forma": "1 - 0.5 * u"}, {"n": "B: Acero", "A": 1}]})
    L.paso("A) Flujo por la barra cónica de cobre")\
        .di("Como el radio varía linealmente de erre cero a erre uno, es un cono truncado.", V(foco=0), DATO("radio varía linealmente"),
            M(r"R(x) = 0{,}0120 - 0{,}030\,x \qquad A(x) = \pi R(x)^2"))\
        .di("Integrando Fourier en un cono truncado sale la misma expresión con el producto de radios.",
            F(r"H_A = \frac{\pi k_A R_0 R_1\,(T_H-T_C)}{L}"))\
        .di(f"Sustituimos en metros y da {nu(ha, 'W', 3)}.",
            M(rf"H_A = \frac{{\pi(385)(0{{,}}0120)(0{{,}}0060)(80{{,}}0-20{{,}}0)}}{{0{{,}}20}}"),
            R(rf"H_A = {tex(ha, 3, u='W')}", "ra"))
    L.paso("B) Flujo por la barra cilíndrica de acero")\
        .di("La barra B tiene área constante: usamos directamente la ley de Fourier.", V(foco=1),
            F(r"H_B = \frac{k_B\,\pi R_1^2\,(T_H-T_C)}{L}"))\
        .di(f"Sustituimos y da {nu(hb, 'W', 3)}.",
            M(rf"H_B = \frac{{(50)\,\pi(0{{,}}0060)^2(60{{,}}0)}}{{0{{,}}20}}"),
            R(rf"H_B = {tex(hb, 3, u='W')}", "rb"))
    L.paso("C) Flujo total en paralelo")\
        .di("Como las dos barras unen los mismos depósitos en paralelo, los flujos se suman.", V(foco="total"),
            M(rf"H_T = H_A + H_B = {tex(ha, 3)} + {tex(hb, 3)}"),
            R(rf"H_T = {tex(ht, 3, u='W')}", "rc"))
    L.responde("A", rf"H_A={tex(ha, 3)}\ \text{{W}}", ha, "W").responde("B", rf"H_B={tex(hb, 3)}\ \text{{W}}", hb, "W").responde("C", rf"H_T={tex(ht, 3)}\ \text{{W}}", ht, "W")
    return L.oficial("III Parcial extraordinario I-2025", 26.1, 1.70, 27.8).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q044
@leccion("Q044")
def q044() -> Leccion:
    r1, r2, r3 = 0.0500, 0.0800, 0.1000
    t1, t3, k1, k2 = 125.0, 40.0, 50.0, 1.50
    R1 = (1.0 / r1 - 1.0 / r2) / (4.0 * math.pi * k1)
    R2 = (1.0 / r2 - 1.0 / r3) / (4.0 * math.pi * k2)
    H = (t1 - t3) / (R1 + R2)
    t2 = t1 - H * R1
    L = _des("Q044", "Reactor esférico de acero con revestimiento cerámico",
             "Reactor esférico: acero ($r_1=5{,}00$ cm a $r_2=8{,}00$ cm, $k_1=50{,}0$ W/m·K) cubierto por cerámica ($r_2$ a $r_3=10{,}00$ cm, $k_2=1{,}50$ W/m·K). Interior a $T_1=125{,}0\\ ^\\circ$C, exterior a $T_3=40{,}0\\ ^\\circ$C. A) Temperatura $T_2$ en la interfaz. B) Flujo estacionario $H$.",
             claves=["esfera compuesta", "dos capas", "reactor"],
             visual={"tipo": "conduccion", "geo": "esfera2", "r1": r1, "r2": r2, "r3": r3,
                     "T1": t1, "T2": round(t2, 1), "T3": t3, "k1": k1, "k2": k2, "n1": "acero", "n2": "cerámica", "uT": "°C"})
    L.paso("Fourier en cada cascarón esférico")\
        .di("Para una capa esférica, integrar uno entre erre cuadrado da la resistencia esférica.", V(foco="dr"), DATO("cascarón esférico"),
            F(r"H = \frac{4\pi k\,(T_{\text{int}}-T_{\text{ext}})}{\frac{1}{r_{\text{int}}}-\frac{1}{r_{\text{ext}}}}", "f"))\
        .di("Escribimos las resistencias del acero y de la cerámica.",
            M(rf"R_1 = \frac{{\frac{{1}}{{0{{,}}05}}-\frac{{1}}{{0{{,}}08}}}}{{4\pi(50{{,}}0)}} = {tex(R1, 4, u='K/W')}"),
            M(rf"R_2 = \frac{{\frac{{1}}{{0{{,}}08}}-\frac{{1}}{{0{{,}}10}}}}{{4\pi(1{{,}}50)}} = {tex(R2, 4, u='K/W')}"))
    L.paso("A) Temperatura en la interfaz acero-cerámica")\
        .di("Como el flujo es el mismo en ambas capas en serie, igualamos las expresiones de hache.", V(foco="perfil"),
            M(r"\frac{T_1-T_2}{R_1} = \frac{T_2-T_3}{R_2} \;\Rightarrow\; T_2 = \frac{R_2 T_1 + R_1 T_3}{R_1+R_2}"))\
        .di(f"La cerámica aísla mucho más, así que casi toda la caída ocurre en ella: {nu(t2, '°C', dec=1)}.",
            M(rf"T_2 = \frac{{({tex(R2, 4)})(125{{,}}0)+({tex(R1, 4)})(40{{,}}0)}}{{{tex(R1, 4)}+{tex(R2, 4)}}}"),
            R(rf"T_2 = {tex(t2, dec=1, u='°C')}", "ra"))
    L.paso("B) Flujo estacionario total")\
        .di(f"Dividimos el salto total entre la resistencia total: sale {nu(H, 'W', 3)}.", V(foco="listo"),
            M(rf"H = \frac{{T_1-T_3}}{{R_1+R_2}} = \frac{{125{{,}}0-40{{,}}0}}{{{tex(R1 + R2, 4)}}}"),
            R(rf"H = {tex(H, 3, u='W')}", "rb"))
    L.responde("A", rf"T_2={tex(t2, dec=1)}\ ^\circ\text{{C}}", t2, "°C").responde("B", rf"H={tex(H, 3)}\ \text{{W}}", H, "W")
    return L.oficial("III Parcial ordinario I-2025", 118.0, 588.0).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q045
@leccion("Q045")
def q045() -> Leccion:
    k, lc, r1, r2, t1, t2, rm = 235.0, 5.00, 0.0400, 0.0800, 150.0, 45.0, 0.0600
    h_por_l = 2.0 * math.pi * k * (t1 - t2) / math.log(r2 / r1)
    Q = h_por_l * lc * (10.0 * 60.0)
    tm = t1 - (t1 - t2) * math.log(rm / r1) / math.log(r2 / r1)
    L = _des("Q045", "Tubería cilíndrica de aluminio: flujo por longitud y T(6 cm)",
             "Tubería cilíndrica de aluminio ($k=235$ W/m·K, $L=5{,}00$ m, $r_1=4{,}00$ cm a $150\\ ^\\circ$C, $r_2=8{,}00$ cm a $45\\ ^\\circ$C). A) Flujo por unidad de longitud $H/L$. B) Calor total en $10{,}0$ min. C) Temperatura a $r=6{,}00$ cm.",
             aviso="Ojo: el solucionario oficial de 25-II-E usó por error el volumen πr²L en lugar del área lateral cilíndrica 2πrL. Aquí resolvemos con el área correcta 2πrL.",
             claves=["cilindro", "flujo por longitud", "logaritmo"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": r1, "r2": r2, "L": lc, "T1": t1, "T2": t2, "k": k, "r": rm, "uT": "°C"})
    L.paso("A) Flujo por unidad de longitud")\
        .di("El flujo es radial a través de la pared lateral del cilindro: área dos pi erre ele.", V(foco="dr"), DATO("flujo radial"),
            F(r"H = -k(2\pi r L)\frac{dT}{dr}"),
            OJO("El área lateral es $2\\pi r L$, **no** el volumen $\\pi r^2 L$."))\
        .di("Integramos de cuatro a ocho centímetros.", V(foco="barrido"),
            M(r"\frac{H}{L} = \frac{2\pi k\,(T_1-T_2)}{\ln(r_2/r_1)} = \frac{2\pi(235)(150-45)}{\ln(8{,}00/4{,}00)}"))\
        .di(f"Da {nu(h_por_l / 1e3, 'kW/m', 4)}.", R(rf"\frac{{H}}{{L}} = {tex(h_por_l / 1e3, 4, u='kW/m')}", "ra"))
    L.paso("B) Calor transferido por toda la tubería en 10 min")\
        .di("Multiplicamos por los cinco metros de tubo y por seiscientos segundos.", V(foco="flujo"),
            M(rf"Q = \left(\frac{{H}}{{L}}\right)L\,\Delta t = ({tex(h_por_l, 4, sci='si')})(5{{,}}00)(600)"))\
        .di(f"Resultan {nu(Q / 1e6, 'MJ', 4)}.", R(rf"Q = {tex(Q, 4, u='J', sci='si')} = {tex(Q / 1e6, 4, u='MJ')}", "rb"))
    L.paso("C) Temperatura en r = 6,00 cm")\
        .di("Integrando solo hasta seis centímetros, la fracción del salto de temperatura sigue el logaritmo.", V(foco="punto"),
            M(r"T(r) = T_1 - (T_1-T_2)\frac{\ln(r/r_1)}{\ln(r_2/r_1)}"))\
        .di(f"Sustituimos y da {nu(tm, '°C', dec=1)}.",
            M(rf"T(6{{,}}00\ \text{{cm}}) = 150 - 105\,\frac{{\ln(6/4)}}{{\ln(8/4)}}"),
            R(rf"T(6{{,}}00\ \text{{cm}}) = {tex(tm, dec=1, u='°C')}", "rc"))
    L.responde("A", rf"H/L={tex(h_por_l / 1e3, 4)}\ \text{{kW/m}}", h_por_l, "W/m")\
     .responde("B", rf"Q={tex(Q, 4, sci='si')}\ \text{{J}}", Q, "J")\
     .responde("C", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
    return L.oficial("III Parcial extraordinario II-2025", 6202.0, 18.61e6, 80.0,
                     nota="El solucionario oficial usó A = πr²L (volumen) en vez del área lateral 2πrL; con 2πrL da 223,7 kW/m, 671,1 MJ y 88,6 °C.").usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q046
@leccion("Q046")
def q046() -> Leccion:
    k, h, a_inf, a_sup, t1, t2 = 429.0, 0.100, 0.100, 0.200, 100.0, 20.0
    # A(y) = 0.100 + y -> int_0^0.1 dy/(0.1+y) = ln(0.2/0.1) = ln(2)
    H = k * (t1 - t2) / math.log(a_sup / a_inf)
    Q = H * 60.0
    tm = t1 - (t1 - t2) * math.log(0.150 / a_inf) / math.log(a_sup / a_inf)
    pts = [[round(h * i / 40, 4), round(t1 - (t1 - t2) * math.log((a_inf + h * i / 40) / a_inf) / math.log(a_sup / a_inf), 2)] for i in range(41)]
    L = _des("Q046", "Barra trapezoidal de plata en lámina aislante",
             "Barra de plata ($k=429$ W/m·K) de grosor $h=0{,}100$ m con área lineal $A(y)=0{,}100+y$ (de $0{,}100$ a $0{,}200\\ \\text{m}^2$) entre $T_1=100{,}0\\ ^\\circ$C (abajo) y $T_2=20{,}0\\ ^\\circ$C (arriba). A) Flujo $H$. B) Energía en $1$ min. C) Temperatura en $y=h/2$.",
             claves=["barra trapezoidal", "área lineal", "plata"],
             visual={"tipo": "conduccion", "geo": "pared", "L": h, "T1": t1, "T2": t2, "eje": "y", "uT": "°C",
                     "forma": "0.5 + 0.5 * u", "pts": pts, "xm": h / 2, "etA": "0,100+y"})
    L.paso("A) Flujo de calor integrando 1/A(y)")\
        .di("Sustituimos el área lineal en Fourier y separamos variables.", V(foco="dx"), DATO("A(y)"),
            M(r"A(y) = 0{,}100 + \frac{0{,}200-0{,}100}{0{,}100}\,y = 0{,}100 + y"),
            F(r"\frac{H}{k}\int_0^{0{,}100}\frac{dy}{0{,}100+y} = -\int_{T_1}^{T_2}dT"))\
        .di("Con el cambio u igual a cero coma uno más ye sale el logaritmo de dos.", V(foco="barrido"),
            M(r"\frac{H}{k}\ln\left(\frac{0{,}200}{0{,}100}\right) = T_1 - T_2 \;\Rightarrow\; H = \frac{429(100{,}0-20{,}0)}{\ln 2}"))\
        .di(f"Resultan {nu(H / 1e3, 'kW', 4)}.", R(rf"H = {tex(H / 1e3, 4, u='kW')}", "ra"))
    L.paso("B) Energía en 1 minuto")\
        .di(f"Multiplicamos el flujo por sesenta segundos: da {nu(Q / 1e6, 'MJ', 4)}.",
            M(rf"Q = H\,\Delta t = ({tex(H, 4, sci='si')})(60)"),
            R(rf"Q = {tex(Q, 4, u='J', sci='si')} = {tex(Q / 1e6, 4, u='MJ')}", "rb"))
    L.paso("C) Temperatura a la mitad (y = 0,050 m)")\
        .di("Integramos solo hasta la mitad del grosor, donde el área vale cero coma quince.", V(foco="punto"),
            M(r"T(h/2) = T_1 - \frac{H}{k}\ln\left(\frac{0{,}150}{0{,}100}\right) = 100{,}0 - 80{,}0\,\frac{\ln(1{,}5)}{\ln 2}"))\
        .di(f"La temperatura a media altura es {nu(tm, '°C', dec=2)}.", R(rf"T(h/2) = {tex(tm, dec=2, u='°C')}", "rc"))
    L.responde("A", rf"H={tex(H / 1e3, 4)}\ \text{{kW}}", H, "W").responde("B", rf"Q={tex(Q / 1e6, 4)}\ \text{{MJ}}", Q, "J").responde("C", rf"T={tex(tm, dec=2)}\ ^\circ\text{{C}}", tm, "°C")
    return L.oficial("III Parcial ordinario II-2025", 49510.0, 2.971e6, 53.21).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q047
@leccion("Q047")
def q047() -> Leccion:
    dv, dp, A, ti, te, kv, H = 0.00400, 0.00300, 5.00 * 2.20, 30.0, 5.0, 0.960, 1.46e4
    # Criterio oficial 26-I-E: vidrio del lado exterior (5,0 °C)
    dt_v = H * dv / (kv * A)
    tf = te + dt_v
    dt_p = ti - tf
    kp = H * dp / (A * dt_p)
    gv = dt_v / dv
    gp = dt_p / dp
    L = _des("Q047", "Ventana compuesta de vidrio y policarbonato",
             "Una ventana de $5{,}00\\ \\text{m}\\times 2{,}20\\ \\text{m}$ une vidrio ($4{,}00$ mm, $k_v=0{,}960$ W/m·K) y policarbonato ($3{,}00$ mm) entre $30{,}0\\ ^\\circ$C (interior) y $5{,}0\\ ^\\circ$C (exterior), con $H=1{,}46\\times 10^4$ W. A) Temperatura en la frontera. B) Conductividad del policarbonato. C) Gradientes de temperatura.",
             claves=["ventana compuesta", "gradiente", "serie"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [ti, round(tf, 1), te], "uT": "°C", "incognitas": [1],
                     "capas": [{"n": "Policarbonato", "e": dp, "k": round(kp, 3)}, {"n": "Vidrio", "e": dv, "k": kv}]})
    L.paso("A) Temperatura en la frontera")\
        .di("El área de la ventana es once metros cuadrados y el flujo es igual en ambas láminas.", DATO("1,46 x 104 W"),
            M(r"A = (5{,}00)(2{,}20) = 11{,}0\ \text{m}^2"))\
        .di("Con el vidrio del lado exterior, despejamos la temperatura de la unión.", V(foco="perfil"),
            F(r"H = \frac{k_v A\,(T_f - T_{\text{ext}})}{\Delta x_v} \;\Rightarrow\; T_f = T_{\text{ext}} + \frac{H\,\Delta x_v}{k_v A}"))\
        .di(f"Sustituimos y obtenemos {nu(tf, '°C', dec=1)}.",
            M(rf"T_f = 5{{,}}0 + \frac{{(1{{,}}46\times 10^4)(0{{,}}00400)}}{{(0{{,}}960)(11{{,}}0)}} = {tex(tf, dec=2, u='°C')}"),
            R(rf"T_f = {tex(tf, dec=1, u='°C')}", "ra"),
            NOTA("Si el vidrio se colocara del lado interior, $T_f=24{,}5\\ ^\\circ\\text{C}$; $k_{pc}$ y ambos gradientes quedan iguales."))
    L.paso("B) Conductividad del policarbonato")\
        .di("En el policarbonato cae el resto de la temperatura.",
            M(rf"\Delta T_{{pc}} = 30{{,}}0 - {tex(tf, dec=2)} = {tex(dt_p, dec=2, u='°C')}"))\
        .di(f"Despejamos su conductividad: sale {nu(kp, 'W/(m·K)', 3)}.",
            M(rf"k_{{pc}} = \frac{{H\,\Delta x_{{pc}}}}{{A\,\Delta T_{{pc}}}} = \frac{{(1{{,}}46\times 10^4)(0{{,}}00300)}}{{(11{{,}}0)({tex(dt_p, dec=2)})}}"),
            R(rf"k_{{pc}} = {tex(kp, 3, u='W/(m·K)')}", "rb"))
    L.paso("C) Gradientes de temperatura")\
        .di("El gradiente en cada lámina es el cambio de temperatura entre su espesor.",
            M(rf"\left|\frac{{\Delta T}}{{\Delta x}}\right|_v = \frac{{H}}{{k_v A}} = {tex(gv, 4, u='K/m')}"),
            M(rf"\left|\frac{{\Delta T}}{{\Delta x}}\right|_{{pc}} = \frac{{H}}{{k_{{pc}} A}} = {tex(gp, 4, u='K/m')}"),
            R(rf"\nabla T_v = {tex(gv, 4, u='K/m')}\qquad \nabla T_{{pc}} = {tex(gp, 4, u='K/m')}", "rc"))
    L.responde("A", rf"T_f={tex(tf, dec=1)}\ ^\circ\text{{C}}", tf, "°C")\
     .responde("B", rf"k_{{pc}}={tex(kp, 3)}\ \text{{W/(m}}\cdot\text{{K)}}", kp, "W/(m·K)")\
     .responde("C1", rf"{tex(gv, 4)}\ \text{{K/m}}", gv, "K/m")\
     .responde("C2", rf"{tex(gp, 4)}\ \text{{K/m}}", gp, "K/m")
    return L.oficial("III Parcial extraordinario I-2026", 10.5, 0.2042, 1383.0, 6500.0).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q048
@leccion("Q048")
def q048() -> Leccion:
    A, d, k, t1, t2 = 3.00 * 4.00, 0.200, 0.800, 25.0, 5.0
    H = k * A * (t1 - t2) / d
    L = _sel("Q048", "Conducción a través de una pared de ladrillo",
             "Una pared de ladrillo de $3{,}00$ m de alto, $4{,}00$ m de ancho y $20{,}0$ cm de grosor ($k=0{,}800$ W/m·K) separa $25{,}00\\ ^\\circ$C (interior) de $5{,}00\\ ^\\circ$C (exterior). La tasa de transferencia de calor es:",
             {"A": "96 W", "B": "192 W", "C": "480 W", "D": "960 W"}, "D",
             aviso="En el Superexamen se pegó por error el diagrama p-V de la pregunta 8; este problema es de una pared plana y no lleva gráfica.",
             claves=["pared plana", "ley de Fourier"],
             visual={"tipo": "conduccion", "geo": "pared", "L": d, "T1": t1, "T2": t2, "uT": "°C"})
    L.paso("Aplicar Fourier a la pared plana")\
        .di("Calculamos el área y pasamos el grosor a metros.", DATO("20,0 cm"),
            LISTA(["$A = (3{,}00)(4{,}00) = 12{,}0\\ \\text{m}^2$", "$L = 0{,}200$ m", "$\\Delta T = 25{,}00-5{,}00 = 20{,}0\\ ^\\circ\\text{C}$"]))\
        .di("Sustituimos en la fórmula de conducción plana.", V(foco="flujo"),
            F(r"H = \frac{k A\,\Delta T}{L}"),
            M(rf"H = \frac{{(0{{,}}800)(12{{,}}0)(20{{,}}0)}}{{0{{,}}200}}"),
            R(rf"H = {tex(H, 3, u='W')}"))
    revisar_opciones(L, {
        "A": ("La A es diez veces menor por un fallo de potencia de diez.", "Error de decimal."),
        "B": ("La B olvida dividir bien entre cero coma dos.", "Usó mal el grosor."),
        "C": ("La C es la mitad del resultado correcto.", "Falta un factor dos."),
        "D": ("La D es la correcta: novecientos sesenta watts.", "$H=960$ W exacto."),
    })
    return L.oficial("III Parcial ordinario I-2026", "D").usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q049
@leccion("Q049")
def q049() -> Leccion:
    k, h, t1, t2 = 235.0, 0.480, 200.0, 25.0
    a0 = 0.220
    m_c = 0.157
    ah = a0 - m_c * h
    am = a0 - m_c * (h / 2.0)
    l_inf = math.sqrt(a0)
    l_sup = math.sqrt(ah)
    I_tot = math.log(a0 / ah) / m_c
    H = k * (t1 - t2) / I_tot
    I_mid = math.log(a0 / am) / m_c
    t_mid = t1 - (H / k) * I_mid
    Q = H * 120.0
    pts = [[round(h * i / 40, 4), round(t1 - (t1 - t2) * math.log(a0 / (a0 - m_c * h * i / 40)) / math.log(a0 / ah), 2)] for i in range(41)]
    L = _des("Q049", "Barra piramidal de aluminio A(y) = 0,220 − 0,157y",
             "Bloque piramidal de aluminio ($k=235$ W/m·K, altura $h=0{,}480$ m) entre $200{,}0\\ ^\\circ$C (base $y=0$) y $25{,}0\\ ^\\circ$C (tope $y=h$), con área cuadrada $A(y)=0{,}220-0{,}157y$. A) Lados inferior y superior. B) Flujo $H$. C) Temperatura a $y=h/2$. D) Energía en $2$ min. E) ¿Cambia $H$ si se intercambian los reservorios?",
             claves=["barra piramidal", "área variable", "logaritmo"],
             visual={"tipo": "conduccion", "geo": "pared", "L": h, "T1": t1, "T2": t2, "eje": "y", "uT": "°C",
                     "forma": "Math.sqrt(1 - 0.3425 * u)", "pts": pts, "xm": h / 2, "etA": "0,220−0,157y"})
    L.paso("A) Lados de las caras cuadradas")\
        .di("Evaluamos el área en la base y en el tope, y sacamos la raíz cuadrada.", DATO("A(y) = 0,220"),
            M(rf"A(0) = 0{{,}}220\ \text{{m}}^2 \;\Rightarrow\; L_{{\text{{inf}}}} = \sqrt{{0{{,}}220}} = {tex(l_inf, 3, u='m')}"),
            M(rf"A(0{{,}}480) = 0{{,}}220 - 0{{,}}157(0{{,}}480) = {tex(ah, 4, u='m^2')} \;\Rightarrow\; L_{{\text{{sup}}}} = {tex(l_sup, 3, u='m')}"),
            R(rf"L_{{\text{{inf}}}} = {tex(l_inf, 3, u='m')}\qquad L_{{\text{{sup}}}} = {tex(l_sup, 3, u='m')}", "ra"))
    L.paso("B) Flujo calorífico integrando 1/A(y)")\
        .di("Separamos variables en la ley de Fourier.", V(foco="dx"),
            F(r"H = -k A(y)\frac{dT}{dy} \;\Rightarrow\; \frac{H}{k}\int_0^{0{,}480}\frac{dy}{0{,}220-0{,}157y} = -\int_{200{,}0}^{25{,}0}dT"))\
        .di(f"La integral con logaritmo vale {n(I_tot, 4)} metros a la menos uno.", V(foco="barrido"),
            M(rf"I = \frac{{1}}{{0{{,}}157}}\ln\left(\frac{{0{{,}}220}}{{{tex(ah, 5)}}}\right) = {tex(I_tot, 4)}\ \text{{m}}^{{-1}}"))\
        .di(f"Despejamos hache y sale {nu(H / 1e3, 'kW', 4)}.",
            M(rf"H = \frac{{(235)(200{{,}}0-25{{,}}0)}}{{{tex(I_tot, 4)}}}"),
            R(rf"H = {tex(H / 1e3, 4, u='kW')}\ \ ({tex(H, 5, u='W', sci='no')})", "rb"))
    L.paso("C) Temperatura a la mitad de la altura (y = 0,240 m)")\
        .di("Integramos solo desde la base hasta cero coma veinticuatro metros.", V(foco="punto"),
            M(rf"A(0{{,}}240) = 0{{,}}220 - 0{{,}}157(0{{,}}240) = {tex(am, 5, u='m^2')}"),
            M(rf"I_{{1/2}} = \frac{{1}}{{0{{,}}157}}\ln\left(\frac{{0{{,}}220}}{{{tex(am, 5)}}}\right) = {tex(I_mid, 4)}\ \text{{m}}^{{-1}}"))\
        .di(f"Restamos la caída desde doscientos grados: da exactamente {nu(t_mid, '°C', dec=1)}.",
            M(rf"T(0{{,}}240) = 200{{,}}0 - \frac{{{tex(H, 5, sci='no')}}}{{235}}({tex(I_mid, 4)})"),
            R(rf"T(0{{,}}240\ \text{{m}}) = {tex(t_mid, dec=1, u='°C')}", "rc"))\
        .por_que("Como la barra se angosta arriba, la mitad superior tiene más resistencia y ahí cae más temperatura: $121{,}6\\ ^\\circ\\text{C} > 112{,}5\\ ^\\circ\\text{C}$.")
    L.paso("D y E) Energía en 2 minutos e intercambio de focos")\
        .di(f"En ciento veinte segundos pasan {nu(Q / 1e6, 'MJ', 3)}.",
            M(rf"Q = H\,\Delta t = ({tex(H, 4, sci='si')})(120)"), R(rf"Q = {tex(Q, 3, u='J', sci='si')}", "rd"))\
        .di("Si se intercambian las temperaturas, la integral geométrica y la diferencia de temperaturas no cambian: la magnitud del flujo se mantiene igual.",
            R(r"|H|\ \text{se mantiene igual (solo se invierte el sentido)}", "re"))
    L.responde("A1", rf"L_{{\text{{inf}}}}={tex(l_inf, 3)}\ \text{{m}}", l_inf, "m")\
     .responde("A2", rf"L_{{\text{{sup}}}}={tex(l_sup, 3)}\ \text{{m}}", l_sup, "m")\
     .responde("B", rf"H={tex(H / 1e3, 4)}\ \text{{kW}}", H, "W")\
     .responde("C", rf"T={tex(t_mid, dec=1)}\ ^\circ\text{{C}}", t_mid, "°C")\
     .responde("D", rf"Q={tex(Q / 1e6, 3)}\ \text{{MJ}}", Q, "J")
    return L.oficial("III Parcial ordinario I-2026", 0.469, 0.380, 15397.0, 121.6, 1.85e6).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q050
@leccion("Q050")
def q050() -> Leccion:
    A, d, t1, t2 = 4.00, 0.0200, 300.0, 100.0
    # k(y) = 200 + 1000y -> int_0^d dy/(200+1000y) = (1/1000)*ln(220/200)
    I_tot = math.log(220.0 / 200.0) / 1000.0
    H = A * (t1 - t2) / I_tot
    Q = H * (5.0 * 60.0)
    tm = t1 - (t1 - t2) * math.log(210.0 / 200.0) / math.log(220.0 / 200.0)
    pts = [[round(d * i / 40, 5), round(t1 - (t1 - t2) * math.log((200.0 + 1000.0 * d * i / 40) / 200.0) / math.log(1.1), 2)] for i in range(41)]
    L = _des("Q050", "Lámina con conductividad variable k(y) = 200 + 1000y",
             "Una lámina ($A=4{,}00\\ \\text{m}^2$, grosor $d=2{,}00$ cm) conduce calor entre $T_1=300\\ ^\\circ$C (abajo) y $T_2=100\\ ^\\circ$C (arriba), con $k(y)=200+1000y$ (W/m·K). A) Flujo $H$. B) Energía en $5$ min. C) Temperatura en $y=d/2$.",
             aviso="Ojo: el solucionario oficial de 25-II-R omitió el factor 1/1000 al sustituir du = 1000 dy (obtuvo kW en vez de MW). Aquí resolvemos con la integral exacta; T(d/2) = 197,6 °C coincide en ambos.",
             claves=["conductividad variable", "cambio de variable", "integral"],
             visual={"tipo": "conduccion", "geo": "pared", "L": d, "T1": t1, "T2": t2, "eje": "y", "uT": "°C", "pts": pts, "xm": d / 2})
    L.paso("A) Integrar dy / k(y) sin olvidar el factor 1000")\
        .di("Separamos variables dejando la conductividad con de ye.", V(foco="dx"), DATO("k(y) = 200 + 1000 y"),
            F(r"\frac{H}{A}\int_0^{0{,}020}\frac{dy}{200+1000y} = -\int_{300}^{100}dT"))\
        .di("Con u igual a doscientos más mil ye, de u es mil de ye: hay que dividir entre mil.", V(foco="barrido"),
            M(r"\int_0^{0{,}020}\frac{dy}{200+1000y} = \frac{1}{1000}\ln\left(\frac{220}{200}\right) = \frac{\ln(1{,}1)}{1000}"),
            OJO("No olvides el factor $\\frac{1}{1000}$ de $dy=du/1000$."))\
        .di(f"Despejamos el flujo: da {nu(H / 1e6, 'MW', 4)}.",
            M(rf"H = \frac{{1000\,A\,(T_1-T_2)}}{{\ln(1{{,}}1)}} = \frac{{1000(4{{,}}00)(200)}}{{\ln(1{{,}}1)}}"),
            R(rf"H = {tex(H / 1e6, 4, u='MW')}", "ra"))
    L.paso("B) Energía en 5 minutos")\
        .di(f"Multiplicamos por trescientos segundos: da {nu(Q / 1e9, 'GJ', 4)}.",
            M(rf"Q = H\,\Delta t = ({tex(H, 4, sci='si')})(300)"),
            R(rf"Q = {tex(Q, 4, u='J', sci='si')} = {tex(Q / 1e9, 4, u='GJ')}", "rb"))
    L.paso("C) Temperatura a la mitad del grosor (y = 0,010 m)")\
        .di("Al dividir las dos integrales, el factor mil se cancela.", V(foco="punto"),
            M(r"T(d/2) = T_1 - (T_1-T_2)\frac{\ln(210/200)}{\ln(220/200)} = 300 - 200\,\frac{\ln(1{,}05)}{\ln(1{,}10)}"))\
        .di(f"Resultan {nu(tm, '°C', dec=1)}.", R(rf"T(d/2) = {tex(tm, dec=1, u='°C')}", "rc"))
    L.responde("A", rf"H={tex(H / 1e6, 4)}\ \text{{MW}}", H, "W").responde("B", rf"Q={tex(Q / 1e9, 4)}\ \text{{GJ}}", Q, "J").responde("C", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
    return L.oficial("Reposición II-2025", 8394.0, 2.518e6, 197.6,
                     nota="El solucionario oficial omitió el factor 1/1000 de du = 1000 dy (obtuvo kW en vez de MW); T(d/2) = 197,6 °C sí coincide porque el factor se cancela.").usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q051
@leccion("Q051")
def q051() -> Leccion:
    A, d, k, t1, t2 = 4.00 * 3.00, 0.200, 0.800, 30.0, 10.0
    H = k * A * (t1 - t2) / d
    L = _sel("Q051", "Conducción en pared de oficina (30 °C a 10 °C)",
             "Una pared de ladrillo de $4{,}00$ m de alto, $3{,}00$ m de ancho y $20{,}0$ cm de grosor ($k=0{,}800$ W/m·K) separa $30{,}0\\ ^\\circ$C de $10{,}0\\ ^\\circ$C. La tasa de transferencia de calor es:",
             {"A": "96 W", "B": "192 W", "C": "480 W", "D": "960 W"}, "D",
             claves=["pared plana", "ley de Fourier"],
             visual={"tipo": "conduccion", "geo": "pared", "L": d, "T1": t1, "T2": t2, "uT": "°C"})
    L.paso("Aplicar Fourier")\
        .di("Igual que en Q048: el área es doce metros cuadrados y el salto térmico es veinte grados.", DATO("20,0 cm"),
            F(r"H = \frac{k A\,(T_1-T_2)}{L}"),
            M(rf"H = \frac{{(0{{,}}800)(12{{,}}0)(30{{,}}0-10{{,}}0)}}{{0{{,}}200}}"),
            R(rf"H = {tex(H, 3, u='W')}"))
    revisar_opciones(L, {
        "A": ("La A es diez veces menor.", "Error de decimal."),
        "B": ("La B tiene un error al dividir por cero coma dos.", "$12\\times 0{,}8\\times 20 = 192$, faltó dividir entre $0{,}2$."),
        "C": ("La C es la mitad del valor real.", "Falta un factor dos."),
        "D": ("La D es la correcta: novecientos sesenta watts.", "$H=960$ W."),
    })
    return L.oficial("Reposición I-2026", "D").usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q052
@leccion("Q052")
def q052() -> Leccion:
    k_cu, l_cu, k_ac, A = 385.0, 1.00, 50.2, 4.00e-4
    th, tu, tc = 100.0, 65.0, 0.0
    H = k_cu * A * (th - tu) / l_cu
    l_ac = k_ac * A * (tu - tc) / H
    L = _des("Q052", "Varilla compuesta cobre-acero entre vapor e hielo",
             "Una varilla aislada ($A=4{,}00\\ \\text{cm}^2$) une $1{,}00$ m de cobre (en vapor a $100\\ ^\\circ$C) con un tramo $L_2$ de acero (en agua-hielo a $0\\ ^\\circ$C). La unión está a $65{,}0\\ ^\\circ$C. (a) Flujo de calor $H$. (b) Longitud $L_2$ del acero.",
             claves=["serie", "cobre y acero", "folleto"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [th, tu, tc], "uT": "°C",
                     "capas": [{"n": "Cobre", "e": l_cu, "k": k_cu}, {"n": "Acero", "e": l_ac, "k": k_ac}]})
    L.paso("(a) Flujo de calor desde el tramo de cobre")\
        .di("Convertimos el área a metros cuadrados: cuatro centímetros cuadrados son cuatro por diez a la menos cuatro.", DATO("4.00 cm2"),
            M(r"A = 4{,}00\times 10^{-4}\ \text{m}^2"))\
        .di(f"Aplicamos Fourier en el cobre y obtenemos {nu(H, 'W', 3)}.", V(foco="flujo"),
            F(r"H = \frac{k_{\text{Cu}} A\,(T_H - T_u)}{L_1}"),
            M(rf"H = \frac{{(385)(4{{,}}00\times 10^{{-4}})(100-65{{,}}0)}}{{1{{,}}00}}"),
            R(rf"H = {tex(H, 3, u='W')}", "ra"))
    L.paso("(b) Longitud del tramo de acero")\
        .di("En serie, ese mismo flujo atraviesa el acero.", V(foco="perfil"),
            M(r"H = \frac{k_{\text{ac}} A\,(T_u - T_C)}{L_2} \;\Rightarrow\; L_2 = \frac{k_{\text{ac}} A\,(T_u - T_C)}{H}"))\
        .di(f"Sustituimos y da {nu(l_ac, 'm', 3)}.",
            M(rf"L_2 = \frac{{(50{{,}}2)(4{{,}}00\times 10^{{-4}})(65{{,}}0-0)}}{{5{{,}}39}}"),
            R(rf"L_2 = {tex(l_ac, 3, u='m')}", "rb"))
    L.responde("a", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("b", rf"L_2={tex(l_ac, 3)}\ \text{{m}}", l_ac, "m")
    return L.oficial("Folleto resuelto 3.3-1", 5.39, 0.242).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q053
@leccion("Q053")
def q053() -> Leccion:
    L = _des("Q053", "Dos barras en paralelo (solución simbólica)",
             "Dos barras de largo $L$, conductividades $k_1$ y $k_2$, y áreas $A_1=2A$ y $A_2=A$ (errata $2A$ en enunciado del folleto) se colocan en paralelo entre $T_1=2T$ y $T_2=T$. Determine el flujo total $H_{\\text{total}}$.",
             aviso="El enunciado del folleto imprime A₂ = 2A por errata, mientras la solución oficial usa A₂ = A. Mostramos el resultado oficial y el del enunciado literal.",
             claves=["paralelo", "simbólico", "folleto"],
             visual={"tipo": "conduccion", "geo": "paralelo", "L": 1.0, "T1": 200, "T2": 100, "uT": "K",
                     "barras": [{"n": "k₁ · A₁=2A", "A": 2}, {"n": "k₂ · A₂=A", "A": 1}]})
    L.paso("Flujo por cada barra en paralelo")\
        .di("Ambas barras ven la misma diferencia de temperatura: dos te menos te igual a te.", V(foco="total"), DATO("una al lado de la otra"),
            M(r"\Delta T = T_1 - T_2 = 2T - T = T"))\
        .di("En paralelo los flujos se suman.", F(r"H_{\text{total}} = H_1 + H_2"))\
        .di("Con el criterio de la solución del folleto, área uno es dos A y área dos es A.",
            M(r"H_1 = \frac{k_1(2A)\,T}{L} \qquad H_2 = \frac{k_2(A)\,T}{L}"))\
        .di("Sumamos y sacamos factor común.",
            R(r"H_{\text{total}} = \frac{(2k_1+k_2)\,A T}{L}", "ra"),
            NOTA("Si se usa literalmente $A_2=2A$, entonces $H_{\\text{total}}=\\frac{2(k_1+k_2)AT}{L}$."))
    L.responde("", r"H_{\text{total}}=\frac{(2k_1+k_2)AT}{L}", None, "")
    return L.usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q054
@leccion("Q054")
def q054() -> Leccion:
    L = _des("Q054", "Arreglo mixto de tres barras (paralelo y serie)",
             "Barra 1 ($L_1=2L$, $A_1=2A$, $k_1=\\tfrac{1}{3}k$) está en paralelo con la rama en serie de barra 2 ($L$, $A$, $k$) y barra 3 ($L$, $A$, $2k$), entre $T_1=2T$ y $T_2=T$. (a) Temperatura en la unión 2-3. (b) Flujo total en términos de $k,A,L,T$.",
             aviso="El Superexamen transcribió k₁ = 3k por error; el folleto original (p. 105) usa k₁ = (1/3)k, que es con el que se obtiene H = kAT/L.",
             claves=["mixto", "serie y paralelo", "simbólico"],
             visual={"tipo": "conduccion", "geo": "paralelo", "L": 2.0, "T1": 200, "T2": 100, "uT": "K",
                     "barras": [{"n": "k₁ = k/3 (2A, 2L)", "A": 2},
                                {"A": 1, "partes": [{"n": "k₂=k", "e": 1}, {"n": "k₃=2k", "e": 1}]}]})
    L.paso("(a) Temperatura en la unión entre las barras 2 y 3")\
        .di("La rama inferior tiene las barras dos y tres en serie, así que comparten el mismo flujo.", V(foco=1), DATO("unión entre las barras 2 y 3"),
            M(r"H_2 = H_3 \;\Rightarrow\; \frac{k A\,(2T - T_u)}{L} = \frac{2k A\,(T_u - T)}{L}"))\
        .di("Cancelamos ka, A y ele, y despejamos la temperatura de unión.",
            M(r"2T - T_u = 2T_u - 2T \;\Rightarrow\; 3T_u = 4T"),
            R(r"T_u = \frac{4}{3}\,T", "ra"))
    L.paso("(b) Flujo total sumando las dos ramas")\
        .di("Calculamos el flujo de la rama superior con ka uno igual a un tercio de ka.", V(foco=0),
            M(r"H_1 = \frac{(k/3)(2A)(2T-T)}{2L} = \frac{1}{3}\frac{kAT}{L}"))\
        .di("En la rama inferior sustituimos la temperatura de unión en la barra dos.",
            M(r"H_{23} = \frac{kA\,(2T - \frac{4}{3}T)}{L} = \frac{2}{3}\frac{kAT}{L}"))\
        .di("Sumamos las dos ramas en paralelo y sale uno exacto.", V(foco="total"),
            M(r"H_{\text{total}} = H_1 + H_{23} = \left(\frac{1}{3}+\frac{2}{3}\right)\frac{kAT}{L}"),
            R(r"H_{\text{total}} = \frac{k A T}{L}", "rb"))
    L.responde("a", r"T_u=\frac{4}{3}T", 4.0 / 3.0, "T").responde("b", r"H=\frac{kAT}{L}", 1.0, "kAT/L")
    return L.oficial("Folleto resuelto 3.3-3", 4.0 / 3.0, 1.0).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q055
@leccion("Q055")
def q055() -> Leccion:
    k, lc, a, b, ta, tb = 238.0, 0.1, 0.2, 1.0, 80.0, 20.0
    H = math.pi * k * lc * (ta - tb) / math.log(b / a)
    L = _des("Q055", "Flujo radial en un semicilindro hueco de aluminio",
             "Calcule el flujo de calor entre las caras curvas de un medio cilindro hueco de aluminio ($k=238{,}0$ W/m·°C, espesor axial $L=0{,}1$ m, radio interior $a=0{,}2$ m a $80\\ ^\\circ$C y exterior $b=1{,}0$ m a $20\\ ^\\circ$C).",
             claves=["semicilindro", "conducción radial", "aluminio"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": a, "r2": b, "L": lc, "T1": ta, "T2": tb, "k": k, "uT": "°C", "etq1": "a", "etq2": "b"})
    L.paso("Área de medio cilindro y ley de Fourier")\
        .di("Es la mitad de un tubo cilíndrico, así que el área radial es pi erre ele, no dos pi.", V(foco="dr"), DATO("caras curvas"),
            M(r"A(r) = \frac{1}{2}(2\pi r L) = \pi r L"),
            F(r"H = -k(\pi r L)\frac{dT}{dr}"))\
        .di("Separamos variables e integramos del radio interno al externo.", V(foco="barrido"),
            M(r"H\int_a^b \frac{dr}{r} = -\pi k L\int_{T_a}^{T_b}dT \;\Rightarrow\; H = \frac{\pi k L\,(T_a-T_b)}{\ln(b/a)}"))\
        .di(f"Sustituimos y obtenemos {nu(H, 'W', 4)}.", V(foco="flujo"),
            M(r"H = \frac{\pi(238{,}0)(0{,}1)(80-20)}{\ln(1{,}0/0{,}2)}"),
            R(rf"H = {tex(H, 4, u='W')}", "ra"))
    L.responde("", rf"H={tex(H, 4)}\ \text{{W}}", H, "W")
    return L.oficial("Folleto resuelto 3.3-4", 2787.0).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q056
@leccion("Q056")
def q056() -> Leccion:
    L = _esfera_hierro("Q056", fuente("Q056"))
    return L.oficial("Folleto resuelto 3.3-5", 340.4,
                     nota="En la sustitución impresa del folleto aparece 238,0 por errata de copia, pero el resultado 340,4 °C sí usa k=73.")


# ============================================================================ Q057 y Q060
def _esfera_kr(q: str, aviso: str = "") -> Leccion:
    r1, r2, k0, t1, t2, rm = 0.150, 0.600, 1.50, 20.0, 100.0, 0.300
    # Calor fluye de afuera (100°C) hacia adentro (20°C): |H| = 8*pi*k0*(t2-t1)/(1/r1^2 - 1/r2^2)
    denom = 1.0 / r1 ** 2 - 1.0 / r2 ** 2
    H = 8.0 * math.pi * k0 * (t2 - t1) / denom
    tm = t2 - H * (1.0 / rm ** 2 - 1.0 / r2 ** 2) / (8.0 * math.pi * k0)
    pts = [[round(r1 + (r2 - r1) * i / 40, 4), round(t2 - (t2 - t1) * (1.0 / (r1 + (r2 - r1) * i / 40) ** 2 - 1.0 / r2 ** 2) / denom, 2)] for i in range(41)]
    L = _des(q, "Esfera hueca con conductividad k = k₀r",
             "Esfera hueca ($r_1=0{,}150$ m a $20\\ ^\\circ$C, $r_2=0{,}600$ m a $100\\ ^\\circ$C) con $k(r)=k_0 r$ y $k_0=1{,}50\\ \\text{W/(m}^2\\cdot\\text{K)}$. (a) Flujo calórico radial $H$. (b) Temperatura a $r=30$ cm.",
             aviso=aviso, claves=["esfera", "k variable", "integral"],
             visual={"tipo": "conduccion", "geo": "esfera", "r1": r1, "r2": r2, "T1": t1, "T2": t2, "k": "k0 r", "r": rm, "pts": pts, "uT": "°C"})
    L.paso("(a) Integrar Fourier con k = k₀r en esfera")\
        .di("Sustituimos la conductividad y el área esférica cuatro pi erre cuadrado.", V(foco="dr"), DATO("k = k0r"),
            F(r"H = -k(r)A(r)\frac{dT}{dr} = -(k_0 r)(4\pi r^2)\frac{dT}{dr} = -4\pi k_0 r^3\frac{dT}{dr}"))\
        .di("La integral de uno entre erre al cubo trae un factor de un medio.", V(foco="barrido"),
            M(r"\int_{r_1}^{r_2}\frac{dr}{r^3} = \left[-\frac{1}{2r^2}\right]_{r_1}^{r_2} = \frac{1}{2}\left(\frac{1}{r_1^2}-\frac{1}{r_2^2}\right)"),
            OJO("El factor $\\frac{1}{2}$ sube multiplicando el $4\\pi$ y lo convierte en **$8\\pi$**."))\
        .di(f"Despejamos la magnitud del flujo hacia adentro: da {nu(H, 'W', 3)}.",
            M(r"|H| = \frac{8\pi k_0\,(T_2-T_1)}{\frac{1}{r_1^2}-\frac{1}{r_2^2}} = \frac{8\pi(1{,}50)(100-20)}{\frac{1}{0{,}150^2}-\frac{1}{0{,}600^2}}"),
            R(rf"|H| = {tex(H, 3, u='W')}\ \ (\text{{hacia adentro}})", "ra"))
    L.paso("(b) Temperatura a r = 30 cm")\
        .di("Integramos desde treinta centímetros hasta la cara externa de sesenta centímetros.", V(foco="punto"),
            M(r"T(r) = T_2 - \frac{|H|}{8\pi k_0}\left(\frac{1}{r^2}-\frac{1}{r_2^2}\right)"))\
        .di(f"Sustituimos y sale exactamente {nu(tm, '°C', dec=1)}, es decir trescientos cincuenta y siete coma quince kelvin.",
            M(rf"T(0{{,}}300) = 100 - \frac{{{tex(H, 4)}}}{{8\pi(1{{,}}50)}}\left(\frac{{1}}{{0{{,}}300^2}}-\frac{{1}}{{0{{,}}600^2}}\right)"),
            R(rf"T(0{{,}}300\ \text{{m}}) = {tex(tm, dec=1, u='°C')} = {tex(tm + 273.15, dec=2, u='K')}", "rb"))
    L.responde("a", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("b", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
    return L.usa(r"H=-kA\frac{dT}{dx}")


@leccion("Q057")
def q057() -> Leccion:
    return _esfera_kr("Q057").oficial("Folleto resuelto 3.3-6", 72.4, 84.0)


# ============================================================================ Q058
@leccion("Q058")
def q058() -> Leccion:
    L = _des("Q058", "Conducción a través de una cuña trapezoidal",
             "Un bloque en forma de cuña de conductividad $k_0$, largo $L$ y profundidad $C$ tiene altura $B$ en $x=0$ ($T=100\\ ^\\circ$C) y altura $A$ en $x=L$ ($T=0\\ ^\\circ$C). Determine el flujo de calor $H$.",
             claves=["cuña", "área variable", "simbólico"],
             visual={"tipo": "conduccion", "geo": "pared", "L": 1.0, "T1": 100, "T2": 0, "eje": "x", "uT": "°C",
                     "forma": "0.35 + 0.65 * u", "etA": "C·y(x)"})
    L.paso("Área transversal en función de x")\
        .di("La altura crece linealmente de B a A a lo largo de la distancia ele.", V(foco="dx"), DATO("conductividad térmica k0"),
            M(r"y(x) = B + \frac{A-B}{L}\,x \;\Rightarrow\; dx = \frac{L}{A-B}\,dy"))\
        .di("El área transversal es la profundidad C por la altura ye.", M(r"A_{\text{trans}}(y) = C\,y"))
    L.paso("Integrar Fourier y obtener H")\
        .di("Sustituimos en Fourier y pasamos las variables de geometría a la izquierda.", V(foco="barrido"),
            F(r"H = -k_0 (C y)\frac{dT}{dx} \;\Rightarrow\; \frac{H L}{C(A-B)}\int_B^A \frac{dy}{y} = -k_0\int_{100}^{0}dT"))\
        .di("La integral de uno entre ye da el logaritmo de A entre B.",
            M(r"\frac{H L}{C(A-B)}\ln\left(\frac{A}{B}\right) = 100\,k_0"))\
        .di("Despejamos el flujo de calor.",
            R(r"H = \frac{100\,k_0 C\,(A-B)}{L\ln(A/B)}", "ra"))
    L.responde("", r"H=\frac{100\,k_0 C(A-B)}{L\ln(A/B)}", None, "")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q059
@leccion("Q059")
def q059() -> Leccion:
    r1, r2, r3 = 0.0500, 0.0550, 0.0850
    t1, t3, lc = 125.0, 40.0, 1.0
    k1, k2 = 50.2, 0.160
    R1 = math.log(r2 / r1) / (2.0 * math.pi * k1 * lc)
    R2 = math.log(r3 / r2) / (2.0 * math.pi * k2 * lc)
    H = (t1 - t3) / (R1 + R2)
    t2 = t1 - H * R1
    coef_ln = H / (2.0 * math.pi * k2 * lc)
    L = _des("Q059", "Tubería de acero con recubrimiento de caucho",
             "Tubería cilíndrica de acero ($r_1=5{,}00$ cm, espesor $0{,}50$ cm, $k_{\\text{ac}}=50{,}2$) forrada con $3{,}00$ cm de caucho ($k_{\\text{ca}}=0{,}160$), entre $125{,}0\\ ^\\circ$C (interior) y $40{,}0\\ ^\\circ$C (exterior). Para $L=1{,}0$ m: (a) Temperatura en la interfaz. (b) Flujo $H$. (c) $T(r)$ dentro del caucho.",
             claves=["cilindro compuesto", "acero y caucho", "aislante"],
             visual={"tipo": "conduccion", "geo": "cilindro2", "r1": r1, "r2": r2, "r3": r3, "exagera": True,
                     "T1": t1, "T2": round(t2, 2), "T3": t3, "k1": k1, "k2": k2, "n1": "acero", "n2": "caucho", "uT": "°C"})
    L.paso("Resistencias cilíndricas en serie")\
        .di("Los radios son cinco, cinco coma cinco y ocho coma cinco centímetros.", DATO("5.00 cm"),
            LISTA(["$r_1=0{,}0500$ m, $r_2=0{,}0550$ m, $r_3=0{,}0850$ m", "$k_{\\text{acero}}=50{,}2$, $k_{\\text{caucho}}=0{,}160$ W/(m·K)"]))\
        .di("Calculamos la resistencia de cada capa cilíndrica.", V(foco="dr"),
            F(r"R_{\text{cil}} = \frac{\ln(r_{\text{ext}}/r_{\text{int}})}{2\pi k L}"),
            M(rf"R_1 = \frac{{\ln(5{{,}}5/5{{,}}0)}}{{2\pi(50{{,}}2)(1{{,}}0)}} = {tex(R1, 4, u='K/W', sci='si')}"),
            M(rf"R_2 = \frac{{\ln(8{{,}}5/5{{,}}5)}}{{2\pi(0{{,}}160)(1{{,}}0)}} = {tex(R2, 4, u='K/W')}"))
    L.paso("(a) y (b) Flujo y temperatura en la interfaz")\
        .di(f"El flujo total sale {nu(H, 'W', 4)}.", V(foco="perfil"),
            M(rf"H = \frac{{T_1-T_3}}{{R_1+R_2}} = \frac{{125{{,}}0-40{{,}}0}}{{{tex(R1 + R2, 4)}}}"),
            R(rf"H = {tex(H, 4, u='W')}", "rb"))\
        .di(f"Como el acero casi no resiste, en él solo caen seis centésimas de grado: {nu(t2, '°C', dec=2)}.",
            M(rf"T_2 = 125{{,}}0 - H R_1 = 125{{,}}0 - {tex(H * R1, 3)}"),
            R(rf"T_2 = {tex(t2, dec=2, u='°C')}", "ra"))
    L.paso("(c) Perfil T(r) dentro del caucho")\
        .di("Integramos dentro del caucho desde la interfaz hasta un radio erre cualquiera.", V(foco="listo"),
            M(r"T(r) = T_2 - \frac{H}{2\pi k_2 L}\ln\left(\frac{r}{r_2}\right)"),
            R(rf"T(r) = {tex(t2, dec=2)} - {tex(coef_ln, 4)}\ln\left(\frac{{r}}{{0{{,}}0550\ \text{{m}}}}\right)\ ^\circ\text{{C}}", "rc"))
    L.responde("a", rf"T_2={tex(t2, dec=2)}\ ^\circ\text{{C}}", t2, "°C").responde("b", rf"H={tex(H, 4)}\ \text{{W}}", H, "W")
    return L.oficial("Folleto propuesto 3.4-6", 124.94, 196.2).usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q060
@leccion("Q060")
def q060() -> Leccion:
    L = _esfera_kr("Q060", aviso="Es el mismo problema que Q057 (en el folleto propuesto se imprimió 200C y 1000C por 20°C y 100°C, y la respuesta 36,2 W omitió un factor 2 de la integral ∫ r⁻³ dr).")
    return L.oficial("Folleto propuesto 3.4-7", 36.2, 84.0,
                     nota="El folleto propuesto imprimió 36,2 W al omitir el factor 2 de ∫ r⁻³ dr = -1/(2r²); el mismo problema resuelto en 3.3-6 (Q057) da 72,4 W. La temperatura 84 °C sí coincide.")


# ============================================================================ Q061
@leccion("Q061")
def q061() -> Leccion:
    k1, T1, k2, T2 = 73.2, 293.0, 54.0, 573.0
    Lc, A = 0.742, 3.25e-4
    m = (k2 - k1) / (T2 - T1)
    b = k1 - m * T1
    k_prom = 0.5 * (k1 + k2)
    H = (A / Lc) * k_prom * (T2 - T1)
    L = _des("Q061", "Barra de hierro con conductividad k(T) lineal",
             "Para el hierro, $k$ varía linealmente de $73{,}2$ W/(K·m) a $293$ K hasta $54{,}0$ W/(K·m) a $573$ K. Una barra aislada de $74{,}2$ cm y $3{,}25\\ \\text{cm}^2$ une depósitos a $20\\ ^\\circ$C y $300\\ ^\\circ$C. (a) Relación $k(T)$. (b) Flujo de calor $H$.",
             aviso="Ojo: la solución impresa del folleto 3.3-9 escribió A = 3,25 m² en vez de 3,25 cm² (error de un factor 10⁴, dando 78 220 W). Aquí calculamos con el área real 3,25×10⁻⁴ m².",
             claves=["k dependiente de T", "hierro", "integral"],
             visual={"tipo": "conduccion", "geo": "pared", "L": Lc, "T1": 300, "T2": 20, "uT": "°C"})
    L.paso("(a) Recta k(T) = mT + b")\
        .di("Calculamos la pendiente y la intersección con los dos puntos dados.", DATO("varía linealmente"),
            M(rf"m = \frac{{54{{,}}0 - 73{{,}}2}}{{573 - 293}} = {tex(m, 3, u='W/(m·K^2)')}"),
            M(rf"b = 73{{,}}2 - ({tex(m, 4)})(293) = {tex(b, 3, u='W/(m·K)')}"),
            R(rf"k(T) = {tex(m, 3)}\,T + {tex(b, 3)}\ \ \text{{W/(m}}\cdot\text{{K)}}", "ra"))
    L.paso("(b) Integrar k(T) dT con el área en m²")\
        .di("Separamos variables: el área es tres coma veinticinco por diez a la menos cuatro metros cuadrados.", V(foco="barrido"),
            F(r"H\int_0^L dx = A\int_{293}^{573}(mT+b)\,dT"))\
        .di("Para una función lineal, la integral es simplemente la conductividad promedio por delta te.",
            M(rf"k_{{\text{{prom}}}} = \frac{{73{{,}}2 + 54{{,}}0}}{{2}} = {tex(k_prom, 3)}\ \text{{W/(m}}\cdot\text{{K)}}"),
            M(rf"H = \frac{{A\,k_{{\text{{prom}}}}\,\Delta T}}{{L}} = \frac{{(3{{,}}25\times 10^{{-4}})({tex(k_prom, 3)})(280)}}{{0{{,}}742}}"))\
        .di(f"El flujo real es {nu(H, 'W', 3)}.", R(rf"H = {tex(H, 3, u='W')}", "rb"))
    L.responde("b", rf"H={tex(H, 3)}\ \text{{W}}", H, "W")
    return L.oficial("Folleto resuelto 3.3-9", 78220.0,
                     nota="El folleto usó A = 3,25 m² en lugar de 3,25 cm² = 3,25×10⁻⁴ m² (error de 10⁴). Con el área correcta da 7,80 W.").usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ Q062
@leccion("Q062")
def q062() -> Leccion:
    L = _sel("Q062", "Por qué asciende el agua caliente (convección)",
             "Al calentar agua en una olla sobre una cocina, se observa que el fluido caliente asciende desde el fondo hacia la superficie. Según la convección térmica, ¿cuál es la explicación física más adecuada?",
             {"A": "El agua caliente disuelve mejor los gases, generando burbujas.",
              "B": "El agua caliente en el fondo se dilata, reduce su densidad y asciende por flotabilidad.",
              "C": "El vapor de agua formado en el fondo empuja el líquido hacia arriba.",
              "D": "La radiación térmica calienta preferentemente las capas superiores."}, "B",
             claves=["convección", "densidad", "flotabilidad"])
    L.paso("Mecanismo de la convección natural")\
        .di("El fondo de la olla calienta primero la capa de agua que está abajo.", DATO("convección térmica"),
            LISTA(["Al calentarse, el agua se dilata ($V$ aumenta)", "Su densidad baja ($\\rho = m/V$ disminuye)", "El agua más fría y densa de arriba baja y empuja hacia arriba a la caliente"]))\
        .di("Ese ciclo continuo impulsado por la flotabilidad es la convección natural.",
            R(r"\text{Más }T \;\Rightarrow\; \text{menor }\rho \;\Rightarrow\; \text{asciende por flotabilidad}"))
    revisar_opciones(L, {
        "A": ("La A no: de hecho los gases son menos solubles en agua caliente.", "No explica la convección."),
        "C": ("La C no: la convección ocurre mucho antes de que haya ebullición.", "No requiere vapor."),
        "D": ("La D no: la cocina calienta desde el fondo, no desde arriba.", "El calor entra por la base."),
        "B": ("La B es la correcta: dilatación, menor densidad y flotabilidad.", "Definición exacta de convección natural."),
    })
    return L.oficial("III Parcial extraordinario I-2025", "B")
