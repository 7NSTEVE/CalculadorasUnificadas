"""Exámenes de Reposición FI2103 · Parcial 2 (Fluidos e Hidrodinámica · 7 lecciones).

Cubren los 5 exámenes históricos de reposición (`23-II-R`, `24-I-R`, `24-II-R`, `25-II-R`,
`26-I-R`). Cada desarrollo parte exclusivamente de la hoja oficial del II Parcial
(`documentos-ii-2026_Ecuaciones_Segundo_Parcial_FG3(1).pdf`) con aritmética calculada en Python.
"""

from __future__ import annotations

import math

from .dsl import DATO, F, LISTA, M, NOTA, OJO, OPCION, Leccion, R
from .formato import tex
from .registro import leccion

G = 9.80
RHO_AGUA = 1000.0


def _fuente_r(doc: str, nombre: str, num: str) -> dict:
    return {"doc": doc, "nombre": nombre, "num": num, "parcial": 2}


# ============================================================================ RP-23II-2
@leccion("RP-23II-2")
def rp_23ii_2() -> Leccion:
    h = 0.5
    eta = 0.3
    rho = 670.0
    L_tubo = 0.200
    D = 0.002
    r = D / 2.0
    dp = rho * G * h  # 3283 Pa
    Q = dp * math.pi * (r ** 4) / (8.0 * eta * L_tubo)  # 2.1487e-8 m3/s
    V10 = Q * 10.0  # 2.1487e-7 m3
    area = math.pi * r ** 2
    v = Q / area  # 6.8396e-3 m/s
    nr = rho * v * D / eta  # 0.03055

    L = Leccion(
        "RP-23II-2",
        "Recipiente con aceite y flujo viscoso por tubo (Reposición II-2023 P2)",
        tema="fluidos",
        tipo="desarrollo",
        nivel=3,
        parcial=2,
        fuente=_fuente_r("23-II-R", "Reposición II-2023", "2"),
        enunciado=(
            "Un recipiente cilíndrico abierto ($R=1{,}0\\ \\text{m}$, altura $h=0{,}5\\ \\text{m}$) contiene aceite "
            "de viscosidad $\\eta=0{,}3\\ \\text{Pa}\\cdot\\text{s}$ y densidad $\\rho=670\\ \\text{kg/m}^3$. "
            "El aceite sale por un tubo horizontal en el fondo de largo $L=20{,}0\\ \\text{cm}$ y diámetro $D=0{,}2\\ \\text{cm}$. "
            "Determine: A) $\\Delta p$ entre los extremos del tubo; B) el caudal $Q$; C) el volumen en $10\\ \\text{s}$; "
            "D) la velocidad media de salida; E) si el flujo es laminar."
        ),
        claves=["Poiseuille", "viscosidad", "Reynolds", "laminar", "reposición"],
    )

    L.paso("Datos del tubo en metros")\
        .di("Cuidado con las unidades pequeñas del tubo: pasamos el diámetro y el largo a metros y dividimos el diámetro entre dos para sacar el radio.",
            DATO("0,2 cm"),
            LISTA([
                r"$h=0{,}5\ \text{m}$, $\rho=670\ \text{kg/m}^3$, $\eta=0{,}3\ \text{Pa}\cdot\text{s}$",
                r"$L=0{,}200\ \text{m}$, $D=2{,}0\times 10^{-3}\ \text{m}\;\Rightarrow\;R=1{,}0\times 10^{-3}\ \text{m}$",
            ]))

    L.paso("A) Diferencia de presiones en los extremos del tubo")\
        .di("En la entrada del tubo la presión es la atmosférica más la columna de aceite, y en la salida es solo la atmosférica. Restamos usando la hidrostática de la hoja.",
            F(r"p=p_0+\rho gh"),
            M(r"\Delta p=p_1-p_2=(p_0+\rho gh)-p_0=\rho gh"),
            M(rf"\Delta p=(670)(9{{,}}80)(0{{,}}5)={tex(dp, 4)}\ \text{{Pa}}"),
            R(rf"\Delta p={tex(dp, 4)}\ \text{{Pa}}"))

    L.paso("B) Caudal volumétrico por Poiseuille")\
        .di("Como el aceite es viscoso usamos la ley de Poiseuille del formulario del segundo parcial con el radio a la cuarta potencia.",
            F(r"\frac{dV}{dt}=\frac{(p_1-p_2)\pi R^4}{8\eta L}"),
            M(rf"Q=\frac{{(3283)\pi(1{{,}}0\times 10^{{-3}})^4}}{{8(0{{,}}3)(0{{,}}200)}}={tex(Q, 3)}\ \text{{m}}^3/\text{{s}}"),
            R(rf"Q={tex(Q, 3)}\ \text{{m}}^3/\text{{s}}"))

    L.paso("C) Volumen descargado en 10 s")\
        .di("Con caudal constante, el volumen total que sale es el caudal multiplicado por el intervalo de diez segundos.",
            M(rf"V=Q\,\Delta t=({tex(Q, 3)})(10)={tex(V10, 3)}\ \text{{m}}^3"),
            R(rf"V={tex(V10, 3)}\ \text{{m}}^3"))

    L.paso("D) Velocidad media de salida")\
        .di("Despejamos la rapidez desde la casilla de caudal de la hoja, usando el área circular pi por radio al cuadrado.",
            F(r"\frac{dV}{dt}=Av"),
            M(rf"A=\pi R^2=\pi(1{{,}}0\times 10^{{-3}})^2={tex(area, 4)}\ \text{{m}}^2"),
            M(rf"v=\frac{{Q}}{{A}}=\frac{{{tex(Q, 3)}}}{{{tex(area, 4)}}}={tex(v, 3)}\ \text{{m/s}}"),
            R(rf"v={tex(v, 3)}\ \text{{m/s}}"))

    L.paso("E) Verificación de régimen laminar")\
        .di("Evaluamos el número de Reynolds con la casilla de la hoja. Como resulta muchísimo menor que dos mil, confirmamos que el flujo es totalmente laminar.",
            F(r"N_R=\frac{\rho v D}{\eta}"),
            M(rf"N_R=\frac{{(670)({tex(v, 3)})(2{{,}}0\times 10^{{-3}})}}{{0{{,}}3}}={tex(nr, 3)}"),
            R(rf"N_R={tex(nr, 2)}\ll 2000\;\Rightarrow\;\text{{sí es laminar}}"))

    L.usa(r"p=p_0+\rho gh", r"\frac{dV}{dt}=\frac{(p_1-p_2)\pi R^4}{8\eta L}", r"\frac{dV}{dt}=Av", r"N_R=\frac{\rho v D}{\eta}")
    L.responde("A", rf"\Delta p={tex(dp, 4)}\ \text{{Pa}}", dp, "Pa")
    L.responde("B", rf"Q={tex(Q, 3)}\ \text{{m}}^3/\text{{s}}", Q, "m^3/s")
    L.responde("C", rf"V={tex(V10, 3)}\ \text{{m}}^3", V10, "m^3")
    L.responde("D", rf"v={tex(v, 3)}\ \text{{m/s}}", v, "m/s")
    L.responde("E", rf"N_R={tex(nr, 3)}", nr, "")
    L.oficial("Reposición II-2023", 3283, 2.15e-8, 2.15e-7, 6.84e-3, 0.0306, tol=0.03)
    L.te_toca("Si el radio del tubo se duplicara con igual Δp, ¿por qué factor crecería el caudal Q de Poiseuille?", 16.0, "",
              pista="Mira el exponente de R en Poiseuille.", solucion="2⁴ = 16 veces mayor.")
    return L


# ============================================================================ RP-24I-2
@leccion("RP-24I-2")
def rp_24i_2() -> Leccion:
    H = 4.00
    L_comp = 2.00
    ancho_rect = L_comp / 2.0  # 1.00 m
    f_rect = RHO_AGUA * G * ancho_rect * (H ** 2 / 2.0)  # 78400 N
    # Integral int_0^4 rho*g*(4-y)*sqrt(y/2) dy = rho*g/sqrt(2) * [4*(2/3)*4^(3/2) - (2/5)*4^(5/2)]
    int_parab = (4.0 * (2.0 / 3.0) * (H ** 1.5) - (2.0 / 5.0) * (H ** 2.5)) / math.sqrt(2.0)
    f_parab = RHO_AGUA * G * int_parab  # 59132.98 N
    f_tot = f_rect + f_parab  # 137532.98 N

    L = Leccion(
        "RP-24I-2",
        "Compuerta mixta rectangular y parabólica (Reposición I-2024 P2)",
        tema="fluidos",
        tipo="desarrollo",
        nivel=3,
        parcial=2,
        fuente=_fuente_r("24-I-R", "Reposición I-2024", "2"),
        enunciado=(
            "Un contenedor de agua tiene una compuerta de altura $H=4{,}00\\ \\text{m}$ y largo $L=2{,}00\\ \\text{m}$: "
            "su mitad izquierda ($x<0$) es rectangular de ancho $L/2$ y su mitad derecha ($x>0$) sigue la parábola $y=2x^2$. "
            "A) Escriba $dF_\\perp$ sobre la mitad rectangular. "
            "B) Escriba $dF_\\perp$ sobre la mitad parabólica. "
            "C) Calcule la fuerza hidrostática total sobre la compuerta."
        ),
        aviso=(
            "Nota sobre la tabla resumen de 24-I-R p.4: la columna A imprimió por error el valor total 1,38×10⁵ N "
            "en lugar de la expresión diferencial de la mitad rectangular ρg(H − y)(L/2) dy."
        ),
        claves=["fuerza hidrostática", "integral", "compuerta parabólica", "presión manométrica", "reposición"],
        visual={"tipo": "pared", "forma": "mixta", "H": 4.0, "rect": 1.0, "k": 2.0,
                "titulo": "Compuerta: mitad rectángulo (1,00 m) y mitad parábola y = 2x²"},
    )

    L.paso("Planteo con presión hidrostática y franjas horizontales")\
        .di("Colocamos el eje ye hacia arriba desde el fondo de la compuerta. Una franja a altura ye está sumergida una profundidad H menos ye.",
            DATO("H = 4,00 m"),
            F(r"p=p_0+\rho gh"),
            F(r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}"),
            M(r"h=H-y\;\Rightarrow\;p_{\text{agua}}(y)=\rho g(H-y)"))

    L.paso("A) Diferencial de fuerza en la mitad rectangular")\
        .di("Partimos de la definición de presión de la hoja. En el lado izquierdo toda franja horizontal tiene ancho constante ele medios y alto de ye.",
            F(r"p=\frac{dF_\perp}{dA}"),
            M(r"dA_{\text{rect}}=\frac{L}{2}\,dy"),
            R(r"dF_{\text{rect}}=\rho g(H-y)\frac{L}{2}\,dy"))

    L.paso("B) Diferencial de fuerza en la mitad parabólica")\
        .di("En el lado derecho despejamos equis de la curva ye igual dos equis cuadrado para obtener el ancho variable de cada franja horizontal.",
            M(r"y=2x^2\;\Rightarrow\;x=\sqrt{\frac{y}{2}}"),
            M(r"dA_{\text{parab}}=\sqrt{\frac{y}{2}}\,dy"),
            R(r"dF_{\text{parab}}=\rho g(H-y)\sqrt{\frac{y}{2}}\,dy"))

    L.paso("C) Fuerza en la mitad rectangular")\
        .di("Sumamos las franjas desde ye igual cero hasta cuatro metros. Ro, ge y ele medios no cambian de franja a franja: salen de la integral.",
            F(r"F_\perp=\int p\,dA"),
            M(r"F_{\text{rect}}=\int_0^H \rho g(H-y)\frac{L}{2}\,dy=\rho g\frac{L}{2}\int_0^H (H-y)\,dy"))\
        .di("Integramos cada parte: hache es un número fijo y da hache por ye; ye da ye al cuadrado entre dos.",
            M(r"\int (H-y)\,dy=H\,y-\frac{y^2}{2}"),
            M(r"\left[H\,y-\frac{y^2}{2}\right]_0^H=\left(H^2-\frac{H^2}{2}\right)-0=\frac{H^2}{2}"))\
        .di("Ahora sustituimos los números.",
            M(rf"F_{{\text{{rect}}}}=(1000)(9{{,}}80)(1{{,}}00)\frac{{4{{,}}00^2}}{{2}}={tex(f_rect, 4)}\ \text{{N}}"))

    L.paso("C) Fuerza en la mitad parabólica")\
        .di("Escribimos la raíz como potencia un medio. La raíz de dos no cambia: sale afuera.",
            M(r"\sqrt{\frac{y}{2}}=\frac{y^{1/2}}{\sqrt{2}}"),
            M(r"F_{\text{parab}}=\frac{\rho g}{\sqrt{2}}\int_0^4 (4-y)\,y^{1/2}\,dy"))\
        .di("Multiplicamos: ye por ye a la un medio suma exponentes, uno más un medio es tres medios.",
            M(r"(4-y)\,y^{1/2}=4\,y^{1/2}-y^{3/2}"),
            M(r"\int 4\,y^{1/2}\,dy=4\,\frac{y^{3/2}}{3/2}=\frac{8}{3}\,y^{3/2}"),
            M(r"\int y^{3/2}\,dy=\frac{y^{5/2}}{5/2}=\frac{2}{5}\,y^{5/2}"))\
        .di("Evaluamos en cuatro: la raíz de cuatro es dos, dos al cubo es ocho y dos a la quinta es treinta y dos. En cero todo vale cero.",
            M(r"\frac{8}{3}(4)^{3/2}=\frac{8}{3}(8)=\frac{64}{3}"),
            M(r"\frac{2}{5}(4)^{5/2}=\frac{2}{5}(32)=\frac{64}{5}"),
            M(rf"F_{{\text{{parab}}}}=\frac{{9800}}{{\sqrt{{2}}}}\left(\frac{{64}}{{3}}-\frac{{64}}{{5}}\right)={tex(f_parab, 4)}\ \text{{N}}"))

    L.paso("C) Fuerza total sobre la compuerta")\
        .di("Sumamos las dos mitades para obtener la fuerza hidrostática total sobre la compuerta.",
            M(rf"F_{{\text{{total}}}}=F_{{\text{{rect}}}}+F_{{\text{{parab}}}}={tex(f_rect, 4)}+{tex(f_parab, 4)}={tex(f_tot, 4)}\ \text{{N}}"),
            R(r"F_{\text{total}}=1{,}38\times 10^5\ \text{N}"))

    L.usa(r"p=p_0+\rho gh", r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}", r"p=\frac{dF_\perp}{dA}", r"F_\perp=\int p\,dA")
    L.responde("C-rect", rf"F_{{\text{{rect}}}}={tex(f_rect, 4)}\ \text{{N}}", f_rect, "N")
    L.responde("C-parab", rf"F_{{\text{{parab}}}}={tex(f_parab, 4)}\ \text{{N}}", f_parab, "N")
    L.responde("C-tot", r"F_{\text{total}}=1{,}38\times 10^5\ \text{N}", f_tot, "N")
    L.oficial("Reposición I-2024", 78400, 59133, 1.38e5, tol=0.015)
    L.te_toca("Si toda la compuerta fuera un rectángulo completo de 2,00 m × 4,00 m, ¿cuánto valdría F = ½ρgLH²?", 156800.0, "N",
              pista="Multiplica 0,5 × 1000 × 9,80 × 2,00 × 4,00².", solucion="F = 1,568 × 10⁵ N.")
    return L


# ============================================================================ RP-24II-2
@leccion("RP-24II-2")
def rp_24ii_2() -> Leccion:
    Q = 0.800e-3  # m3/s
    ra = 0.0200
    rb = 0.0100
    Aa = math.pi * ra ** 2
    Ab = math.pi * rb ** 2
    va = Q / Aa  # 0.6366 m/s
    vb = Q / Ab  # 2.5465 m/s
    dp = 0.5 * RHO_AGUA * (vb ** 2 - va ** 2)  # 3039.64 Pa
    dh = dp / (RHO_AGUA * G)  # 0.3102 m

    L = Leccion(
        "RP-24II-2",
        "Tubo Venturi con manómetros verticales (Reposición II-2024 P2)",
        tema="fluidos",
        tipo="desarrollo",
        nivel=3,
        parcial=2,
        fuente=_fuente_r("24-II-R", "Reposición II-2024", "2"),
        enunciado=(
            "Agua fluye con caudal $0{,}800\\ \\text{L/s}$ por un tubo horizontal con dos tubos piezométricos verticales abiertos. "
            "Los diámetros son $D_a=4{,}00\\ \\text{cm}$ y $D_b=2{,}00\\ \\text{cm}$. "
            "A) Calcule $v_a$ y $v_b$. B) Determine $p_a-p_b$. C) ¿Cuánto vale $h_a-h_b$?"
        ),
        claves=["Venturi", "continuidad", "Bernoulli", "manómetro", "reposición"],
    )

    L.paso("Conversión del caudal y radios")\
        .di("Pasamos el caudal a metros cúbicos por segundo dividiendo entre mil y dividimos cada diámetro entre dos para obtener los radios.",
            DATO("0,800 L/s"),
            LISTA([
                r"$Q=0{,}800\ \text{L/s}=8{,}00\times 10^{-4}\ \text{m}^3/\text{s}$",
                r"$r_a=2{,}00\times 10^{-2}\ \text{m}$, $r_b=1{,}00\times 10^{-2}\ \text{m}$",
            ]))

    L.paso("A) Rapidez del agua en a y en b")\
        .di("Aplicamos la ecuación de caudal de la hoja en ambas secciones. Como el radio en be es la mitad, su área es la cuarta parte y la rapidez se cuadruplica.",
            F(r"\frac{dV}{dt}=Av"),
            M(rf"v_a=\frac{{8{{,}}00\times 10^{{-4}}}}{{\pi(2{{,}}00\times 10^{{-2}})^2}}={tex(va, 3)}\ \text{{m/s}}"),
            M(rf"v_b=\frac{{8{{,}}00\times 10^{{-4}}}}{{\pi(1{{,}}00\times 10^{{-2}})^2}}={tex(vb, 3)}\ \text{{m/s}}"),
            R(rf"v_a={tex(va, 3)}\ \text{{m/s}}\qquad v_b={tex(vb, 3)}\ \text{{m/s}}"))

    L.paso("B) Diferencia de presiones por Bernoulli")\
        .di("Escribimos Bernoulli de la hoja entre a y be. Al estar a la misma altura, los términos ro ge ye se cancelan y despejamos pe a menos pe be.",
            F(r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"),
            M(r"p_a-p_b=\frac{1}{2}\rho\left(v_b^2-v_a^2\right)"),
            M(rf"p_a-p_b=\frac{{1}}{{2}}(1000)\left({tex(vb, 4)}^2-{tex(va, 4)}^2\right)={tex(dp, 4)}\ \text{{Pa}}"),
            R(rf"p_a-p_b={tex(dp, 4)}\ \text{{Pa}}"),
            NOTA("La solución oficial usa v_b = 2,55 redondeado y obtiene 3048,4 Pa; sin redondeo intermedio da 3040 Pa."))

    L.paso("C) Diferencia de alturas en los tubos verticales")\
        .di("En cada tubo abierto vertical la presión en la base es la atmosférica más ro ge hache. Al restar ambas ecuaciones se cancela la presión atmosférica.",
            F(r"p=p_0+\rho gh"),
            M(r"p_a-p_b=(p_{\text{atm}}+\rho gh_a)-(p_{\text{atm}}+\rho gh_b)=\rho g(h_a-h_b)"),
            M(rf"h_a-h_b=\frac{{p_a-p_b}}{{\rho g}}=\frac{{{tex(dp, 4)}}}{{(1000)(9{{,}}80)}}={tex(dh, 3)}\ \text{{m}}"),
            R(rf"h_a-h_b={tex(dh, 3)}\ \text{{m}}"))

    L.usa(r"\frac{dV}{dt}=Av", r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2", r"p=p_0+\rho gh")
    L.responde("A-va", rf"v_a={tex(va, 3)}\ \text{{m/s}}", va, "m/s")
    L.responde("A-vb", rf"v_b={tex(vb, 3)}\ \text{{m/s}}", vb, "m/s")
    L.responde("B", rf"p_a-p_b={tex(dp, 4)}\ \text{{Pa}}", dp, "Pa")
    L.responde("C", rf"h_a-h_b={tex(dh, 3)}\ \text{{m}}", dh, "m")
    L.oficial("Reposición II-2024", 0.637, 2.55, 3048.4, 0.31, tol=0.015)
    L.te_toca("Si D_b fuera igual a D_a, ¿cuánto valdría la diferencia de presiones p_a − p_b?", 0.0, "Pa",
              pista="Con igual diámetro, v_a = v_b.", solucion="p_a − p_b = 0 Pa.")
    return L


# ============================================================================ RP-25II-2
@leccion("RP-25II-2")
def rp_25ii_2() -> Leccion:
    H = 50.0
    L0 = 45.0
    L1 = 30.0
    D1 = 0.840
    D2 = 0.700
    dz = 8.00
    f_neta = RHO_AGUA * G * (22.5 * H ** 2 - (H ** 3) / 10.0)  # 428.75e6 N
    v2 = math.sqrt(2.0 * G * (H + dz))  # 33.716 m/s
    v1 = ((D2 / D1) ** 2) * v2  # 23.414 m/s
    Q = (math.pi * D2 ** 2 / 4.0) * v2  # 12.976 m3/s

    L = Leccion(
        "RP-25II-2",
        "Represa trapezoidal y drenaje con Bernoulli (Reposición II-2025 P2)",
        tema="fluidos",
        tipo="desarrollo",
        nivel=3,
        parcial=2,
        fuente=_fuente_r("25-II-R", "Reposición II-2025", "2"),
        enunciado=(
            "Una represa de pared vertical trapezoidal acumula agua hasta $H=50{,}0\\ \\text{m}$ "
            "(ancho superior $45{,}0\\ \\text{m}$, ancho inferior $30{,}0\\ \\text{m}$). "
            "En la base sale una tubería de $D_1=84{,}0\\ \\text{cm}$ que se reduce a $D_2=70{,}0\\ \\text{cm}$, "
            "ubicada $8{,}00\\ \\text{m}$ por debajo de la primera y abierta a la atmósfera. Determine: "
            "A) la fuerza neta del agua sobre la pared; "
            "B) la rapidez $v_2$ en la salida; "
            "C) la rapidez $v_1$ y el caudal en $D_1$."
        ),
        claves=["represa trapezoidal", "integral hidrostática", "Bernoulli", "continuidad", "reposición"],
        visual={"tipo": "pared", "forma": "trapecio", "H": 50.0, "ancho_sup": 45.0, "ancho_inf": 30.0,
                "titulo": "Represa: 45 m arriba, 30 m abajo, 50 m de agua"},
    )

    L.paso("A) Ancho lineal de la pared trapezoidal")\
        .di("Como el aire empuja con presión atmosférica por afuera, la presión neta a profundidad hache es solo ro ge hache.",
            DATO("50,0 m"),
            F(r"p=p_0+\rho gh"),
            F(r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}"),
            M(r"p_{\text{neta}}(h)=(p_0+\rho gh)-p_0=\rho gh"))\
        .di("El ancho baja linealmente de cuarenta y cinco metros arriba a treinta metros en el fondo, quince metros en cincuenta metros de bajada.",
            M(r"L(h)=45{,}0-\frac{45{,}0-30{,}0}{50{,}0}\,h=45{,}0-0{,}30\,h"))

    L.paso("A) La fuerza de una franja")\
        .di("Cada franja horizontal a profundidad hache tiene ancho ele de hache y alto de hache. Su fuerza es la presión por su área.",
            F(r"p=\frac{dF_\perp}{dA}"),
            M(r"dA=L(h)\,dh=(45{,}0-0{,}30\,h)\,dh"),
            M(r"dF=\rho g h\,(45{,}0-0{,}30\,h)\,dh"))\
        .di("Multiplicamos hache por el paréntesis para que queden potencias sueltas.",
            M(r"h\,(45{,}0-0{,}30\,h)=45{,}0\,h-0{,}30\,h^2"))

    L.paso("A) Sumar las franjas: la integral")\
        .di("Sumamos de la superficie hasta el fondo. Ro y ge salen afuera, y cada potencia se integra con la regla.",
            F(r"F_\perp=\int p\,dA"),
            M(r"F_{\text{neta}}=\rho g\int_0^{50}\left(45{,}0\,h-0{,}30\,h^2\right)dh"),
            M(r"\int 45{,}0\,h\,dh=45{,}0\,\frac{h^2}{2}=22{,}5\,h^2"),
            M(r"\int 0{,}30\,h^2\,dh=0{,}30\,\frac{h^3}{3}=0{,}10\,h^3"))\
        .di("Arriba menos abajo: en cincuenta, y en cero todo vale cero.",
            M(r"22{,}5\,(50)^2=56\,250"),
            M(r"0{,}10\,(50)^3=12\,500"),
            M(rf"F_{{\text{{neta}}}}=(1000)(9{{,}}80)(56\,250-12\,500)={tex(f_neta, 4)}\ \text{{N}}"),
            R(r"F_{\text{neta}}=4{,}288\times 10^8\ \text{N}=428{,}75\ \text{MN}"))

    L.paso("B) Rapidez de salida v2 por Bernoulli")\
        .di("Aplicamos Bernoulli entre la superficie del embalse y la salida final, cincuenta y ocho metros más abajo. Ambas están a presión atmosférica y la superficie baja con rapidez despreciable.",
            F(r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"),
            M(r"\rho g(y_0-y_2)=\frac{1}{2}\rho v_2^2\;\Rightarrow\;v_2=\sqrt{2g(y_0-y_2)}"),
            M(rf"v_2=\sqrt{{2(9{{,}}80)(50{{,}}0+8{{,}}00)}}=\sqrt{{1136{{,}}8}}={tex(v2, 4)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 4)}\ \text{{m/s}}"))

    L.paso("C) Rapidez v1 y caudal por continuidad")\
        .di("Usamos la ecuación de continuidad entre las dos secciones de la tubería. Al sustituir áreas circulares se cancela pi cuartos.",
            F(r"A_1 v_1=A_2 v_2"),
            M(rf"v_1=\left(\frac{{D_2}}{{D_1}}\right)^2 v_2=\left(\frac{{0{{,}}700}}{{0{{,}}840}}\right)^2({tex(v2, 4)})={tex(v1, 4)}\ \text{{m/s}}"),
            R(rf"v_1={tex(v1, 4)}\ \text{{m/s}}"))\
        .di("Con la casilla de caudal calculamos el flujo volumétrico usando cualquiera de las dos secciones.",
            F(r"\frac{dV}{dt}=Av"),
            M(rf"Q=\frac{{\pi D_2^2}}{{4}}v_2=\frac{{\pi(0{{,}}700)^2}}{{4}}({tex(v2, 4)})={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"),
            R(rf"Q={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"))

    L.usa(r"p=p_0+\rho gh", r"F_\perp=\int p\,dA", r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2", r"A_1 v_1=A_2 v_2", r"\frac{dV}{dt}=Av")
    L.responde("A", r"F_{\text{neta}}=4{,}288\times 10^8\ \text{N}", f_neta, "N")
    L.responde("B", rf"v_2={tex(v2, 4)}\ \text{{m/s}}", v2, "m/s")
    L.responde("C-v1", rf"v_1={tex(v1, 4)}\ \text{{m/s}}", v1, "m/s")
    L.responde("C-Q", rf"Q={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}", Q, "m^3/s")
    L.oficial("Reposición II-2025", 4.288e8, 33.72, 23.41, 12.98, tol=0.015)
    L.te_toca("Si la tubería saliera justo al nivel de la base (h = 50,0 m en vez de 58,0 m), ¿cuál sería v₂?", 31.30, "m/s",
              pista="Calcula √(2 × 9,80 × 50,0).", solucion="v₂ = √980 = 31,30 m/s.")
    return L


# ============================================================================ RP-26I-3
@leccion("RP-26I-3")
def rp_26i_3() -> Leccion:
    L = Leccion(
        "RP-26I-3",
        "Esferas flotando al 40 % y 60 % en dos fluidos (Reposición I-2026 P3)",
        tema="fluidos",
        tipo="seleccion",
        nivel=2,
        parcial=2,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "3"),
        enunciado=(
            "Dos esferas sólidas idénticas se posan en dos fluidos: en el fluido A se hunde el $40\\ \\%$ de su volumen "
            "y en el fluido B el $60\\ \\%$. Esto implica que la densidad del fluido A es:"
        ),
        opciones={
            "A": "20 % mayor que la densidad del fluido B.",
            "B": "40 % mayor que la densidad del fluido B.",
            "C": "50 % mayor que la densidad del fluido B.",
            "D": "60 % mayor que la densidad del fluido B.",
        },
        correcta="C",
        claves=["flotación", "Arquímedes", "empuje", "selección", "reposición"],
    )

    L.paso("Equilibrio de flotación desde el empuje")\
        .di("Como ambas esferas son idénticas tienen el mismo peso. Al flotar, el empuje de Arquímedes de la hoja iguala al peso en ambos fluidos.",
            DATO("40 %"),
            F(r"F_B=\rho g V"),
            M(r"\rho_A g(0{,}40\,V_{\text{esf}})=\rho_B g(0{,}60\,V_{\text{esf}})"))

    L.paso("Despejar la razón de densidades y elegir la opción")\
        .di("Cancelamos ge y el volumen total de la esfera, y dividimos cero coma seis entre cero coma cuatro: da uno coma cinco, es decir cincuenta por ciento mayor.",
            M(r"\frac{\rho_A}{\rho_B}=\frac{0{,}60}{0{,}40}=1{,}50\;\Rightarrow\;\rho_A=\rho_B+0{,}50\,\rho_B"),
            R(r"\rho_A\ \text{es}\ 50\ \%\ \text{mayor que}\ \rho_B"),
            OPCION("A", "descarta", "Confunde restar 60 % − 40 % con el cociente relativo."),
            OPCION("B", "descarta", "No corresponde al cociente 0,60 / 0,40."),
            OPCION("C", "correcta", "1,50 significa 50 % mayor."),
            OPCION("D", "descarta", "Sobreestima el aumento relativo."))

    L.usa(r"F_B=\rho g V")
    L.oficial("Reposición I-2026", "C")
    return L


# ============================================================================ RP-26I-4
@leccion("RP-26I-4")
def rp_26i_4() -> Leccion:
    dm_dt = 20.0
    rho = 1040.0
    area = 0.0015
    Q = dm_dt / rho
    v = Q / area  # 12.82 m/s

    L = Leccion(
        "RP-26I-4",
        "Rapidez de salida desde flujo másico (Reposición I-2026 P4)",
        tema="fluidos",
        tipo="seleccion",
        nivel=2,
        parcial=2,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "4"),
        enunciado=(
            "Un gran tanque se vacía a tasa constante de $20\\ \\text{kg/s}$, con densidad $1040\\ \\text{kg/m}^3$ "
            "y sección transversal $0{,}0015\\ \\text{m}^2$. La rapidez del flujo de salida es:"
        ),
        opciones={"A": "31,2 m/s", "B": "30,0 m/s", "C": "13,3 m/s", "D": "12,8 m/s"},
        correcta="D",
        claves=["flujo másico", "caudal", "rapidez", "selección", "reposición"],
    )

    L.paso("Combinar flujo másico y caudal desde la hoja")\
        .di("Unimos las dos casillas de flujo de la hoja oficial del segundo parcial para despejar la rapidez ve.",
            DATO("20 kg/s"),
            F(r"\frac{dm}{dt}=\rho\frac{dV}{dt}"),
            F(r"\frac{dV}{dt}=Av"),
            M(r"\frac{dm}{dt}=\rho Av\;\Rightarrow\;v=\frac{dm/dt}{\rho A}"))

    L.paso("Sustituir y comprobar opciones")\
        .di("Multiplicamos la densidad por el área en el denominador y dividimos veinte entre ese producto.",
            M(rf"v=\frac{{20}}{{(1040)(0{{,}}0015)}}=\frac{{20}}{{1{{,}}56}}={tex(v, 3)}\ \text{{m/s}}"),
            R(r"v=12{,}8\ \text{m/s}"),
            OPCION("A", "descarta", "Multiplica en lugar de dividir."),
            OPCION("B", "descarta", "No corresponde a 20 / 1,56."),
            OPCION("C", "descarta", "Aparece si se usa ρ = 1000 en vez de 1040."),
            OPCION("D", "correcta", "Coincide exacto con 12,8 m/s."))

    L.usa(r"\frac{dm}{dt}=\rho\frac{dV}{dt}", r"\frac{dV}{dt}=Av")
    L.responde("A", r"v=12{,}8\ \text{m/s}", v, "m/s")
    L.oficial("Reposición I-2026", "D")
    return L


# ============================================================================ RP-26I-8
@leccion("RP-26I-8")
def rp_26i_8() -> Leccion:
    dm_dt = 60.0
    rho = 1030.0
    D = 0.140
    Q = dm_dt / rho  # 0.058252 m3/s
    area = math.pi * (D ** 2) / 4.0
    v2 = Q / area  # 3.7842 m/s
    h = (v2 ** 2) / (2.0 * G)  # 0.7306 m

    L = Leccion(
        "RP-26I-8",
        "Tanque de agua salada en estado estacionario (Reposición I-2026 P8)",
        tema="fluidos",
        tipo="desarrollo",
        nivel=3,
        parcial=2,
        fuente=_fuente_r("26-I-R", "Reposición I-2026", "8"),
        enunciado=(
            "Un gran tanque de agua salada ($\\rho=1030\\ \\text{kg/m}^3$) abierto a la atmósfera mantiene su nivel "
            "constante con flujo másico de entrada $60{,}0\\ \\text{kg/s}$ y un orificio de salida de diámetro $0{,}140\\ \\text{m}$. "
            "Determine: A) el caudal a la salida; B) la velocidad por el orificio; "
            "C) la profundidad del orificio bajo la superficie."
        ),
        claves=["flujo másico", "caudal", "Bernoulli", "Torricelli", "reposición"],
    )

    L.paso("A) Caudal volumétrico a la salida")\
        .di("Como el nivel del tanque no cambia, el caudal de salida es igual al de entrada. Despejamos el caudal de la casilla de flujo másico de la hoja.",
            DATO("60,0 kg/s"),
            F(r"\frac{dm}{dt}=\rho\frac{dV}{dt}"),
            M(rf"Q=\frac{{60{{,}}0}}{{1030}}={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"),
            R(rf"Q={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"))

    L.paso("B) Velocidad de salida por el orificio")\
        .di("Calculamos el área circular del orificio con el radio cero coma cero siete metros y despejamos ve dos desde la casilla de caudal.",
            F(r"\frac{dV}{dt}=Av"),
            M(rf"A_2=\pi\left(\frac{{0{{,}}140}}{{2}}\right)^2={tex(area, 4)}\ \text{{m}}^2"),
            M(rf"v_2=\frac{{Q}}{{A_2}}=\frac{{{tex(Q, 4)}}}{{{tex(area, 4)}}}={tex(v2, 4)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 4)}\ \text{{m/s}}"))

    L.paso("C) Profundidad del orificio por Bernoulli")\
        .di("Aplicamos Bernoulli entre la superficie del tanque y el chorro saliente. Ambos están a presión atmosférica y la superficie baja casi a velocidad cero.",
            F(r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"),
            M(r"\rho g(y_1-y_2)=\frac{1}{2}\rho v_2^2\;\Rightarrow\;h=\frac{v_2^2}{2g}"),
            M(rf"h=\frac{{({tex(v2, 4)})^2}}{{2(9{{,}}80)}}={tex(h, 4)}\ \text{{m}}"),
            R(rf"h={tex(h, 4)}\ \text{{m}}"))

    L.usa(r"\frac{dm}{dt}=\rho\frac{dV}{dt}", r"\frac{dV}{dt}=Av", r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2")
    L.responde("A", rf"Q={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}", Q, "m^3/s")
    L.responde("B", rf"v_2={tex(v2, 4)}\ \text{{m/s}}", v2, "m/s")
    L.responde("C", rf"h={tex(h, 4)}\ \text{{m}}", h, "m")
    L.oficial("Reposición I-2026", 0.05825, 3.784, 0.7305, tol=0.015)
    L.te_toca("¿Depende la profundidad h = v₂²/(2g) de la densidad del agua salada una vez conocida v₂?", 0.7306, "m",
              pista="ρ se cancela en Bernoulli.", solucion="No depende de ρ: sigue siendo 0,7306 m.")
    return L
