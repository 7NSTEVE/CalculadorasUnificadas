"""Superexamen Q036–Q062: conducción, resistencias térmicas, geometrías radiales, k variable y convección."""

from __future__ import annotations

import math

from .ayudas import receta_cono, revisar_opciones
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


# ============================================================================ moldes de conducción
def _nm(x: float, c: int = 3) -> str:
    """Número con las cifras del enunciado (0,220 y no 0,22)."""
    return tex(x, c)


def _area_lineal(q: str, titulo: str, enunciado: str, incisos, *, k: float, h: float, a0: float, m: float, t1: float, t2: float,
                 dt_s: float = 0.0, dt_txt: str = "", oficial=None, aviso: str = "", et_a: str = "") -> Leccion:
    """Barra con área en línea recta, A(y) = a0 + m·y: Fourier, separar, cambio de variable y despejar.

    incisos: lista en el orden del examen con 'lados', 'H', 'Q', 'T' o 'inter'.
    """
    ah = a0 + m * h
    ym = h / 2.0
    am = a0 + m * ym
    ab = abs(m)
    neg = m < 0
    I = math.log(ah / a0) / m
    H = k * (t1 - t2) / I
    Im = math.log(am / a0) / m
    tm = t1 - (H / k) * Im
    Q = H * dt_s
    sg = "-" if neg else "+"
    A_tex = rf"{_nm(a0)}{sg}{'' if abs(ab - 1.0) < 1e-12 else _nm(ab)}y"
    # La integral se escribe con las cifras justas para que k(T1-T2) entre ella dé el H que se muestra
    from .ayudas import _datos_justos
    (sI,) = _datos_justos([(I, "c")], lambda i: k * (t1 - t2) / i, lambda x: tex(x, 5, sci="no"))

    def cociente(a_fin: float) -> str:
        return rf"\frac{{{_nm(a0)}}}{{{tex(a_fin, 5)}}}" if neg else rf"\frac{{{tex(a_fin, 5)}}}{{{_nm(a0)}}}"

    pts = [[round(h * i / 40, 5), round(t1 - (t1 - t2) * math.log((a0 + m * h * i / 40) / a0) / math.log(ah / a0), 2)] for i in range(41)]
    L = _des(q, titulo, enunciado, aviso=aviso, claves=["área variable", "integral", "logaritmo", "Fourier"],
             visual={"tipo": "conduccion", "geo": "pared", "L": h, "T1": t1, "T2": t2, "eje": "y", "uT": "°C",
                     "forma": f"({a0} + ({m}) * {h} * u) / {max(a0, ah)}", "pts": pts, "xm": ym, "etA": et_a or A_tex.replace("{,}", ",")})
    L.paso("Qué nos dan y qué piden")\
        .di("Un bloque cuya área cambia con la altura: el calor sube por él.", DATO("A(y)"),
            LISTA([f"$k={_nm(k)}$ W/(m·K)", f"Altura: $h={_nm(h)}$ m", f"$A(y)={A_tex}$ en m²", f"Abajo ($y=0$): $T_1={_nm(t1)}\\ ^\\circ\\text{{C}}$", f"Arriba ($y=h$): $T_2={_nm(t2)}\\ ^\\circ\\text{{C}}$"]))\
        .di("Como el área cambia, la fórmula de pared plana no sirve: hay que integrar.", OJO("$H=kA\\frac{\\Delta T}{L}$ solo vale con área constante."))
    letras = iter("ABCDEFG")
    resp = []
    for inc in incisos:
        lt = next(letras)
        if inc == "lados":
            L.paso(f"{lt}) Lado de cada cara cuadrada")\
                .di("Abajo, en ye igual a cero.".replace("ye igual a cero", dice("y = 0", "ye igual a cero")), M(rf"A(0)={_nm(a0)}\ \text{{m}}^2"), M(rf"L_{{\text{{inf}}}}=\sqrt{{{_nm(a0)}}}={tex(math.sqrt(a0), 3, u='m')}", "li", "verde"))\
                .di("Arriba, en la altura completa.", M(rf"A({_nm(h)})={_nm(a0)}{sg}{_nm(ab)}({_nm(h)})={tex(ah, 4, u='m^2')}"), M(rf"L_{{\text{{sup}}}}=\sqrt{{{tex(ah, 4)}}}={tex(math.sqrt(ah), 3, u='m')}", "ls", "verde"))\
                .por_que("Las caras son cuadradas: el área es lado por lado, así que el lado es la raíz del área.")
            resp += [(lt, rf"L_{{\text{{inf}}}}={tex(math.sqrt(a0), 3)}\ \text{{m}}", math.sqrt(a0), "m"), (lt, rf"L_{{\text{{sup}}}}={tex(math.sqrt(ah), 3)}\ \text{{m}}", math.sqrt(ah), "m")]
        elif inc == "H":
            L.paso(f"{lt}) Fourier en una rebanada delgadita")\
                .di("Tomamos una rebanada de grosor de ye: ahí el área casi no cambia.".replace("de ye", dice("dy", "de ye")), V(foco="dx"), F(r"H=-kA\frac{dT}{dy}"))\
                .di("Ponemos el área de esa altura.", M(rf"H=-k\,({A_tex})\,\frac{{dT}}{{dy}}"))\
                .por_que("En estado estacionario el mismo $H$ atraviesa todas las rebanadas; lo que cambia es cuánto cae la temperatura en cada una.")
            L.paso(f"{lt}) Separar variables")\
                .di("Separar es dejar lo de ye a un lado y lo de la temperatura al otro. Paso uno: multiplicar los dos lados por de ye.".replace("lo de ye", "lo de " + dice("y", "ye")).replace("por de ye", "por " + dice("dy", "de ye")),
                    M(rf"H\,dy=-k\,({A_tex})\,dT"))\
                .di("Paso dos: dividir los dos lados entre el área.", M(rf"\frac{{H\,dy}}{{{A_tex}}}=-k\,dT"), NOTA("Receta en la lección **T-MAT3**."))\
                .di("Integral a los dos lados, de abajo hacia arriba: en ye cero está te uno; arriba, te dos.".replace("ye cero", dice("y = 0", "ye cero")).replace("te uno", dice("T₁", "te uno")).replace("te dos", dice("T₂", "te dos")),
                    M(rf"H\int_0^{{{_nm(h)}}}\frac{{dy}}{{{A_tex}}}=-k\int_{{{_nm(t1)}}}^{{{_nm(t2)}}}dT", "int", "azul"))\
                .di("El lado de la temperatura: sumar todos los pedacitos da el cambio total.",
                    M(rf"-k\int_{{{_nm(t1)}}}^{{{_nm(t2)}}}dT=-k\,(T_2-T_1)=-({_nm(k)})({_nm(t2)}-{_nm(t1)})={tex(k * (t1 - t2), 5, sci='no')}"))
            L.paso(f"{lt}) La integral del área: cambio de variable")\
                .di("Regla tres: al área le ponemos un nombre, u. Su pedacito de u es la pendiente por de ye.".replace("de u", dice("du", "de u")).replace("de ye", dice("dy", "de ye")),
                    M(rf"u={A_tex}\qquad du={sg if neg else ''}{_nm(ab)}\,dy\;\Rightarrow\;dy={'-' if neg else ''}\frac{{du}}{{{_nm(ab)}}}"))\
                .di("Los límites cambian: pasan a ser las áreas de abajo y de arriba.", M(rf"y=0\Rightarrow u={_nm(a0)}\qquad y={_nm(h)}\Rightarrow u={tex(ah, 5)}"))\
                .di("Queda la regla dos: uno entre u da logaritmo.", M(rf"\int\frac{{dy}}{{{A_tex}}}={'-' if neg else ''}\frac{{1}}{{{_nm(ab)}}}\int\frac{{du}}{{u}}={'-' if neg else ''}\frac{{1}}{{{_nm(ab)}}}\ln u"),
                    NOTA("Reglas 2 y 3 de la lección **T-MAT2**."))
            L.paso(f"{lt}) La integral del área: arriba menos abajo")\
                .di("Evaluamos el logaritmo arriba menos abajo.", M(rf"I={'-' if neg else ''}\frac{{1}}{{{_nm(ab)}}}\Big[\ln u\Big]_{{{_nm(a0)}}}^{{{tex(ah, 5)}}}={'-' if neg else ''}\frac{{1}}{{{_nm(ab)}}}\left(\ln {tex(ah, 5)}-\ln {_nm(a0)}\right)"))\
                .di(("El signo menos da vuelta a la resta, y una resta de logaritmos es el logaritmo de la división." if neg else "Una resta de logaritmos es el logaritmo de la división."),
                    M(rf"I=\frac{{1}}{{{_nm(ab)}}}\ln\left({cociente(ah)}\right)={sI}\ \text{{m}}^{{-1}}", "ii", "verde"))
            L.paso(f"{lt}) Despejar el flujo")\
                .di("Juntamos las dos integrales.", M(rf"H\,({sI})={tex(k * (t1 - t2), 5, sci='no')}"))\
                .di("Despejamos.", M(rf"H=\frac{{{tex(k * (t1 - t2), 5, sci='no')}}}{{{sI}}}"))\
                .di(f"Son {nu(H / 1e3, 'kW', 4)}.", R(rf"H={tex(H, 5, u='W', sci='no')}={tex(H / 1e3, 4, u='kW')}", "rh"))
            resp.append((lt, rf"H={tex(H / 1e3, 4)}\ \text{{kW}}", H, "W"))
        elif inc == "Q":
            L.paso(f"{lt}) Energía en {dt_txt}")\
                .di("El flujo es energía por segundo: se multiplica por el tiempo en segundos.", F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"), M(rf"\Delta t={dt_txt}={_nm(dt_s)}\ \text{{s}}"))\
                .di("Multiplicamos.", M(rf"Q=({tex(H, 5, sci='no')})({_nm(dt_s)})"), R(rf"Q={tex(Q, 3, u='J')}", "rq"))
            resp.append((lt, rf"Q={tex(Q, 3)}\ \text{{J}}", Q, "J"))
        elif inc == "T":
            L.paso(f"{lt}) Temperatura a la mitad de la altura")\
                .di("Integramos solo hasta la mitad.", V(foco="punto"), M(rf"H\int_0^{{{_nm(ym)}}}\frac{{dy}}{{{A_tex}}}=-k\,(T-{_nm(t1)})"))\
                .di("El área a media altura.", M(rf"A({_nm(ym)})={_nm(a0)}{sg}{_nm(ab)}({_nm(ym)})={tex(am, 5)}\ \text{{m}}^2"))\
                .di("La integral hasta ahí.", M(rf"I_{{1/2}}=\frac{{1}}{{{_nm(ab)}}}\ln\left({cociente(am)}\right)={tex(Im, 4)}\ \text{{m}}^{{-1}}"))\
                .di("Despejamos la temperatura.", M(rf"T={_nm(t1)}-\frac{{H}}{{k}}\,I_{{1/2}}={_nm(t1)}-\frac{{{tex(H, 5, sci='no')}}}{{{_nm(k)}}}({tex(Im, 4)})"))\
                .di(f"Da {nu(tm, '°C', dec=1)}.", R(rf"T(h/2)={tex(tm, dec=1)}\ ^\circ\text{{C}}", "rt"))\
                .por_que("No da el promedio: donde el área es menor, la resistencia es mayor y ahí cae más temperatura.")
            resp.append((lt, rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C"))
        elif inc == "inter":
            L.paso(f"{lt}) ¿Cambia el flujo si se intercambian los focos?")\
                .di("La integral del área depende solo de la forma del bloque, no de las temperaturas.", M(rf"I={sI}\ \text{{m}}^{{-1}}\ \text{{igual}}"))\
                .di("Y la diferencia de temperatura tiene el mismo tamaño.", M(r"|T_1-T_2|=\text{igual}\;\Rightarrow\;|H|=\frac{k\,|T_1-T_2|}{I}"))\
                .di("El flujo tiene el mismo tamaño; solo cambia de sentido.", R(r"|H|\ \text{se mantiene igual}", "ri"))
    for r in resp:
        L.responde(*r)
    if oficial:
        de, vals, nota = oficial
        L.oficial(de, *vals, nota=nota)
    return L.usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}")


def _tubo(q: str, titulo: str, enunciado: str, *, k: float, lc: float, r1: float, r2: float, t1: float, t2: float, rm: float,
          dt_s: float, dt_txt: str, por_longitud: bool = True, oficial=None, aviso: str = "") -> Leccion:
    """Tubo de una capa: Fourier con el área lateral 2πrL, separar e integrar."""
    lnr = math.log(r2 / r1)
    H = 2.0 * math.pi * k * lc * (t1 - t2) / lnr
    HL = H / lc
    Q = H * dt_s
    tm = t1 - (H / (2.0 * math.pi * k * lc)) * math.log(rm / r1)
    L = _des(q, titulo, enunciado, aviso=aviso, claves=["cilindro", "flujo radial", "logaritmo"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": r1, "r2": r2, "L": lc, "T1": t1, "T2": t2, "k": k, "r": rm, "uT": "°C"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un tubo: el calor sale del centro hacia afuera, en dirección radial.", DATO("radio"),
            LISTA([f"$k={_nm(k)}$ W/(m·K), largo $L={_nm(lc)}$ m", f"Radio interno $r_1={_nm(r1)}$ m a ${_nm(t1)}\\ ^\\circ\\text{{C}}$", f"Radio externo $r_2={_nm(r2)}$ m a ${_nm(t2)}\\ ^\\circ\\text{{C}}$"]))\
        .di("El área por donde pasa el calor crece con el radio: hay que integrar.", OJO("El área es la pared lateral $2\\pi rL$, **no** el volumen $\\pi r^2L$."))
    L.paso("A) Fourier en un cascarón cilíndrico")\
        .di("Tomamos un cascarón de radio erre y grosor de erre.".replace("radio erre", "radio " + dice("r", "erre")).replace("grosor de erre", "grosor " + dice("dr", "de erre")), V(foco="dr"), F(r"H=-kA\frac{dT}{dr}"))\
        .di("Su área es la pared del cilindro.", M(r"A=2\pi rL"), M(r"H=-k\,(2\pi rL)\,\frac{dT}{dr}", "f", "azul"))
    L.paso("A) Separar variables")\
        .di("Separar es dejar lo del radio a un lado y lo de la temperatura al otro. Paso uno: multiplicar los dos lados por de erre.".replace("de erre", dice("dr", "de erre")),
            M(r"H\,dr=-k\,(2\pi rL)\,dT"))\
        .di("Paso dos: dividir los dos lados entre erre.".replace("entre erre", "entre " + dice("r", "erre")), M(r"H\,\frac{dr}{r}=-2\pi kL\,dT"), NOTA("Receta en la lección **T-MAT3**."))\
        .di("Integral a los dos lados, del radio interno al externo.", M(rf"H\int_{{{_nm(r1)}}}^{{{_nm(r2)}}}\frac{{dr}}{{r}}=-2\pi kL\int_{{{_nm(t1)}}}^{{{_nm(t2)}}}dT"))
    L.paso("A) Resolver las dos integrales")\
        .di("Izquierda: regla dos, uno entre erre da logaritmo. Arriba menos abajo, y la resta de logaritmos es el logaritmo de la división.".replace("entre erre", "entre " + dice("r", "erre")),
            M(rf"\int_{{{_nm(r1)}}}^{{{_nm(r2)}}}\frac{{dr}}{{r}}=\Big[\ln r\Big]_{{{_nm(r1)}}}^{{{_nm(r2)}}}=\ln {_nm(r2)}-\ln {_nm(r1)}=\ln\frac{{{_nm(r2)}}}{{{_nm(r1)}}}={tex(lnr, 4)}"))\
        .di("Derecha: sumar los pedacitos de temperatura da el cambio total.", M(rf"-2\pi kL\int_{{{_nm(t1)}}}^{{{_nm(t2)}}}dT=-2\pi kL\,(T_2-T_1)=2\pi({_nm(k)})({_nm(lc)})({_nm(t1 - t2)})"))
    L.paso("A) Despejar el flujo")\
        .di("Despejamos hache.".replace("hache", s("H", "hache")), M(rf"H=\frac{{2\pi kL\,(T_1-T_2)}}{{\ln(r_2/r_1)}}=\frac{{2\pi({_nm(k)})({_nm(lc)})({_nm(t1 - t2)})}}{{{tex(lnr, 4)}}}"))\
        .di(f"En todo el tubo: {nu(H / 1e3, 'kW', 4)}. Lo guardamos con cinco cifras, porque se sigue usando.", M(rf"H={tex(H, 5, u='W')}"))
    if por_longitud:
        L.paso("A) Por cada metro de tubo")\
            .di("Como nos piden el flujo de calor por unidad de longitud, dividimos el flujo total entre el largo del tubo.", M(rf"\frac{{H}}{{L}}=\frac{{{tex(H, 5)}}}{{{_nm(lc)}}}"), R(rf"\frac{{H}}{{L}}={tex(HL, 4, u='W/m')}={tex(HL / 1e3, 4, u='kW/m')}", "ra"))
    L.paso(f"B) Energía en {dt_txt}")\
        .di("Partimos de la definición de flujo de calor de la hoja y despejamos la energía multiplicando por el tiempo en segundos.", F(r"H=\frac{dQ}{dt}"), M(rf"Q=H\,\Delta t=({tex(H, 5)})({_nm(dt_s)})"))\
        .di("El producto nos da la energía total transferida en joules durante ese intervalo.", R(rf"Q={tex(Q, 4, u='J')}", "rb"))
    L.paso(f"C) Temperatura en r = {_nm(rm * 100, 3)} cm")\
        .di("Repetimos la integral, pero solo hasta ese radio.", V(foco="punto"), M(rf"H\int_{{{_nm(r1)}}}^{{{_nm(rm)}}}\frac{{dr}}{{r}}=-2\pi kL\int_{{{_nm(t1)}}}^{{T}}dT"))\
        .di("Las dos integrales salen igual que antes: un logaritmo y una resta.", M(rf"H\ln\frac{{{_nm(rm)}}}{{{_nm(r1)}}}=-2\pi kL\,(T-{_nm(t1)})"))\
        .di("Despejamos la temperatura: dividimos entre menos dos pi ka ele y pasamos sumando.", M(rf"T={_nm(t1)}-\frac{{H}}{{2\pi kL}}\ln\frac{{{_nm(rm)}}}{{{_nm(r1)}}}"))\
        .di("Sustituimos.", M(rf"T={_nm(t1)}-\frac{{{tex(H, 5)}}}{{2\pi({_nm(k)})({_nm(lc)})}}\ln({tex(rm / r1, 3)})"))\
        .di(f"Da {nu(tm, '°C', dec=1)}.", R(rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", "rc"))
    L.responde("A", rf"H/L={tex(HL / 1e3, 4)}\ \text{{kW/m}}", HL, "W/m").responde("B", rf"Q={tex(Q, 4)}\ \text{{J}}", Q, "J").responde("C", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
    if oficial:
        de, vals, nota = oficial
        L.oficial(de, *vals, nota=nota)
    return L.usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}")


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
    lnr = math.log(r2 / r1)
    num = 2.0 * math.pi * k_ac * lc * (t1 - t2)
    h_ac = num / lnr
    k_cu = 385.0
    razon = k_cu / k_ac
    L = _des("Q037", "Tubería cilíndrica de acero y comparación con cobre",
             "Una tubería cilíndrica de conductividad $k$, radios $R_1$ y $R_2$, longitud $L$, lleva agua a $T_1$ y afuera está a $T_2<T_1$. A) Expresión de $H$. B) Para acero ($k=50{,}2$ W/m·K, $R_1=3{,}3$ cm, $R_2=4{,}2$ cm, $T_1=80\\ ^\\circ$C, $T_2=20\\ ^\\circ$C, $L=1{,}5$ m), calcule $H$. C) Compare cualitativamente con cobre ($k_{\\text{Cu}}=385$ W/m·K).",
             claves=["conducción cilíndrica", "integral", "tubería"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": r1, "r2": r2, "L": lc, "T1": t1, "T2": t2, "k": k_ac, "uT": "°C", "etq1": "R₁", "etq2": "R₂"})
    L.paso("A) Fourier en un cascarón cilíndrico")\
        .di("El calor sale radialmente desde el radio interior al exterior. Partimos de la ley de Fourier de la hoja.", V(foco="dr"), DATO("tubería cilíndrica"),
            F(r"H = -kA\,\frac{dT}{dr}", "f"))\
        .di("El área que cruza el calor a un radio erre es la pared lateral del cilindro.".replace("erre", dice("r", "erre")),
            M(r"A(r) = 2\pi r L"), M(r"H = -k\,(2\pi rL)\,\frac{dT}{dr}"))\
        .por_que("En estado estacionario la misma corriente $H$ cruza cada capa cilíndrica, aunque el área crezca con $r$.")
    L.paso("A) Separar variables")\
        .di("Dejamos el radio de un lado y la temperatura del otro. Paso uno: multiplicar por de erre.".replace("de erre", dice("dr", "de erre")), M(r"H\,dr=-k\,(2\pi rL)\,dT"))\
        .di("Paso dos: dividir entre erre.".replace("entre erre", "entre " + dice("r", "erre")), M(r"H\,\frac{dr}{r} = -2\pi kL\,dT"), NOTA("Receta en la lección **T-MAT3**."))\
        .di("Integral a los dos lados, desde el radio interior hasta el exterior.", V(foco="barrido"),
            M(r"H\int_{R_1}^{R_2}\frac{dr}{r} = -2\pi k L\int_{T_1}^{T_2}dT"))
    L.paso("A) Resolver las integrales")\
        .di("Izquierda: regla dos, uno entre erre da logaritmo; la resta de logaritmos es el logaritmo de la división.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"\int_{R_1}^{R_2}\frac{dr}{r}=\Big[\ln r\Big]_{R_1}^{R_2}=\ln R_2-\ln R_1=\ln\left(\frac{R_2}{R_1}\right)"))\
        .di("Derecha: la suma de los pedacitos de temperatura; el menos voltea la resta.", M(r"-2\pi kL\int_{T_1}^{T_2}dT=-2\pi kL\,(T_2-T_1)=2\pi kL\,(T_1-T_2)"))\
        .di("Juntamos y despejamos hache.".replace("hache", dice("H", "hache")),
            M(r"H\ln\left(\frac{R_2}{R_1}\right) = 2\pi k L\,(T_1-T_2)"),
            R(r"H = \frac{2\pi k L\,(T_1-T_2)}{\ln(R_2/R_1)}", "ra"))
    L.paso("B) Calcular el flujo en el tubo de acero")\
        .di("Los datos; los radios pueden quedar en centímetros porque en el logaritmo se cancelan.",
            LISTA(["$k=50{,}2\\ \\text{W/(m}\\cdot\\text{K)}$, $L=1{,}5$ m", "$R_1=3{,}3$ cm, $R_2=4{,}2$ cm", "$T_1-T_2 = 80-20 = 60\\ ^\\circ\\text{C}$"]))\
        .di("Sustituimos en la expresión de la parte A.", V(foco="flujo"),
            M(r"H = \frac{2\pi(50{,}2)(1{,}5)(80-20)}{\ln(4{,}2/3{,}3)}"))\
        .di("Arriba multiplicamos todo.", M(rf"2\pi(50{{,}}2)(1{{,}}5)(60)={tex(num, 5)}"))\
        .di("Abajo, el logaritmo de la razón de radios. Guardamos seis cifras del cociente: el logaritmo es muy sensible.", M(rf"\ln\left(\frac{{4{{,}}2}}{{3{{,}}3}}\right)=\ln({tex(r2 / r1, 6)})={tex(lnr, 4)}"))\
        .di(f"Dividimos: el flujo sale {nu(h_ac / 1e3, 'kW', 4)}.", M(rf"H=\frac{{{tex(num, 5)}}}{{{tex(lnr, 4)}}}"),
            R(rf"H = {tex(h_ac, 4, u='W', sci='si')} = {tex(h_ac / 1e3, 4, u='kW')}", "rb"))
    L.paso("C) Comparación con el cobre")\
        .di("Con la misma geometría y temperaturas, el flujo es proporcional a la conductividad.", V(foco="perfil"),
            M(r"\frac{H_{\text{Cu}}}{H_{\text{acero}}}=\frac{2\pi k_{\text{Cu}}L\,\Delta T/\ln(R_2/R_1)}{2\pi k_{\text{acero}}L\,\Delta T/\ln(R_2/R_1)}=\frac{k_{\text{Cu}}}{k_{\text{acero}}}"))\
        .di("Sustituimos las conductividades.", M(rf"\frac{{k_{{\text{{Cu}}}}}}{{k_{{\text{{acero}}}}}} = \frac{{385}}{{50{{,}}2}} = {tex(razon, 3)}"))\
        .di(f"El tubo de cobre perdería calor unas {n(razon, 2)} veces más rápido que el de acero.",
            R(rf"H_{{\text{{Cu}}}} \approx {tex(razon, 3)}\,H_{{\text{{acero}}}}\ \ (\text{{mucho mayor}})", "rc"))
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
        .di("Partimos de Fourier del formulario. En serie el mismo flujo cruza ambas barras; sumamos las dos caídas de temperatura para eliminar la temperatura intermedia.", DATO("CONFIGURACIÓN 1"),
            F(r"H = kA\frac{T_H - T_C}{L} \;\Rightarrow\; T_h - T_{\text{int}} = H\frac{L}{k_1 A},\quad T_{\text{int}} - T_c = H\frac{L}{k_2 A}"),
            M(r"T_h - T_c = H\left(\frac{L}{k_1 A}+\frac{L}{k_2 A}\right) \;\Rightarrow\; H_{\text{serie}} = \frac{k_1 k_2}{k_1+k_2}\frac{A\Delta T}{L}"))\
        .di("En la configuración dos están en paralelo: cada barra conecta directo ambos depósitos y sumamos sus flujos.",
            M(r"H_{\text{paralelo}} = H_1 + H_2 = k_1 A\frac{\Delta T}{L} + k_2 A\frac{\Delta T}{L} = (k_1+k_2)\frac{A\Delta T}{L}"))\
        .di("Dividimos los dos flujos: se cancelan el área, la diferencia de temperatura y el largo.",
            M(r"\frac{H_{\text{serie}}}{H_{\text{paralelo}}}=\frac{\frac{k_1k_2}{k_1+k_2}}{k_1+k_2}=\frac{k_1k_2}{(k_1+k_2)^2}"))\
        .di("Sustituimos las conductividades del cobre y la plata.",
            M(rf"\frac{{H_{{\text{{serie}}}}}}{{H_{{\text{{paralelo}}}}}}=\frac{{(385)(406)}}{{(385+406)^2}}=\frac{{156\,310}}{{625\,681}}={tex(razon_sp, 3)}"))\
        .di("En serie el flujo es aproximadamente la cuarta parte que en paralelo: siempre es menor.",
            R(r"H_1 < H_2 \quad \text{(menor)}", "ra"))\
        .por_que("En serie el flujo es el mismo y se suman las caídas de temperatura; en paralelo hay dos rutas directas entre $T_h$ y $T_c$.")
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
    return L.oficial("III Parcial extraordinario I-2024", 1.0 / 3.0, 1.0 / 3.0, nota="A) menor y B) mayor, igual que la respuesta publicada.").usa(r"H=kA\frac{\Delta T}{L}")


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
        .di("Partimos de la ley de Fourier de la hoja. Como equis va del foco frío al caliente, la temperatura sube con equis y el signo solo indica el sentido.".replace("equis", dice("x", "equis")), V(foco="dx"),
            F(r"H=-kA\,\frac{dT}{dx}"), M(r"H = k\,A(x)\,\frac{dT}{dx}"))\
        .di("Separamos: multiplicamos por de equis y dividimos entre ka por el área.".replace("de equis", dice("dx", "de equis")).replace("entre ka", "entre " + dice("k", "ka")),
            M(r"H\,dx=k\,A(x)\,dT\;\Rightarrow\;\frac{H}{k}\,\frac{dx}{A(x)} = dT"))\
        .di("Uno entre el área: la fracción se da vuelta y queda un polinomio, fácil de integrar.",
            M(r"\frac{1}{A(x)} = \frac{1 + x/2{,}0}{0{,}01} = 100\left(1 + \frac{x}{2{,}0}\right)"),
            NOTA("Nota: aunque el texto dice 'se ensancha', la fórmula dada disminuye de $0{,}01$ a $0{,}005\\ \\text{m}^2$."))
    L.paso("B) Integrar en toda la barra")\
        .di("Integral a los dos lados: equis de cero a dos metros y la temperatura de veinte a ochenta.".replace("equis", dice("x", "equis")), V(foco="barrido"),
            M(r"\frac{H}{k}\int_0^{2{,}00}100\left(1+\frac{x}{2{,}0}\right)dx=\int_{20}^{80}dT=80-20=60"))\
        .di("Un polinomio se integra término por término con la regla de la potencia: uno da equis; equis entre dos da equis cuadrado entre cuatro.".replace("da equis;", "da " + dice("x", "equis") + ";").replace("equis entre dos da equis cuadrado entre cuatro", dice("x/2 → x²/4", "equis entre dos da equis cuadrado entre cuatro")),
            M(r"\int\left(1+\frac{x}{2}\right)dx=x+\frac{1}{2}\cdot\frac{x^2}{2}=x+\frac{x^2}{4}"), NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Arriba menos abajo.", M(rf"I(2{{,}}00)=100\left[x+\frac{{x^2}}{{4}}\right]_0^2=100\left[\left(2+\frac{{4}}{{4}}\right)-0\right]={tex(I_tot, 3)}\ \text{{m}}^{{-1}}"))
    L.paso("B) Hasta la mitad y comparar")\
        .di("Ahora solo hasta un metro, con la misma regla.", V(foco="punto"),
            M(rf"I(1{{,}}00)=100\left[x+\frac{{x^2}}{{4}}\right]_0^1=100\left(1 + \frac{{1^2}}{{4}}\right) = {tex(I_mid, 3)}\ \text{{m}}^{{-1}}"),
            M(r"\frac{H}{k}\,(125)=T(1)-20\qquad \frac{H}{k}\,(300)=80-20"))\
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
        .di("Como están en serie, el flujo de calor es idéntico en las cuatro placas. Partimos de la casilla de conducción estacionaria de la hoja.", DATO("igual espesor"),
            F(r"H = kA\frac{T_H - T_C}{L}"), M(r"H = \frac{k A\,\Delta T}{d}"))\
        .di("Despejamos ka. El flujo, el área y el espesor son iguales en las cuatro, así que ka va al revés de la caída de temperatura.".replace("ka", dice("k", "ka")),
            M(r"k = \frac{H d}{A\,\Delta T} \;\Rightarrow\; k \propto \frac{1}{\Delta T}"))\
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
    num = math.pi * k * a * b * (t1 - t2)
    H = num / h
    Q = H * (5.0 * 60.0)
    pts = [[round(h * i / 40, 4), round(t1 - (t1 - t2) * (1 / a - 1 / (a + (b - a) * i / 40)) / (1 / a - 1 / b), 2)] for i in range(41)]
    L = _des("Q041", "Conducción en un cono truncado sólido",
             "Un cono truncado sólido ($k=50{,}0$ W/m·K, altura $h=2{,}00$ m, radios $a=0{,}100$ m a $T_1=100\\ ^\\circ$C y $b=0{,}450$ m a $T_2=0\\ ^\\circ$C) tiene paredes laterales aisladas. A) Corriente de calor $H$. B) Calor transferido en $5{,}00$ min.",
             claves=["cono truncado", "área variable", "integral"],
             visual={"tipo": "conduccion", "geo": "pared", "L": h, "T1": t1, "T2": t2, "eje": "y", "uT": "°C",
                     "forma": "0.22 + 0.78 * u", "pts": pts, "etA": "π r(y)²"})
    L.paso("A) Fourier con el área del cono")\
        .di("El calor sube por el cono; partimos de la ley de Fourier de la hoja.", V(foco="dx"), DATO("cono truncado"), F(r"H = -kA\,\frac{dT}{dy}"))\
        .di("El radio crece en línea recta desde a abajo hasta be arriba.".replace("be arriba", dice("b arriba", "be arriba")), M(r"r(y) = a + \frac{b-a}{h}\,y"))\
        .di("El área transversal es un círculo de ese radio.", M(r"A = \pi r^2"), M(r"H = -k\,\pi r^2\,\frac{dT}{dy}"))
    L.paso("A) Cambiar la variable a r y separar")\
        .di("Derivamos el radio: así cambiamos de ye a erre.".replace("ye a erre", dice("y a r", "ye a erre")), M(r"dr=\frac{b-a}{h}\,dy\;\Rightarrow\;dy=\frac{h}{b-a}\,dr"))\
        .di("Separamos: multiplicamos por de ye y dividimos entre pi erre cuadrado.".replace("de ye", dice("dy", "de ye")).replace("pi erre cuadrado", dice("πr²", "pi erre cuadrado")), M(r"H\,\frac{dy}{\pi r^2}=-k\,dT"))\
        .di("Ponemos de ye en términos de de erre.".replace("de ye", dice("dy", "de ye")).replace("de erre", dice("dr", "de erre")), M(r"\frac{Hh}{\pi(b-a)}\,\frac{dr}{r^2}=-k\,dT"))
    L.paso("A) La integral de 1/r² (regla 1)")\
        .di("Integramos de abajo hacia arriba: el radio va de a a be.".replace("a a be", dice("a a b", "a a be")), V(foco="barrido"), M(r"\frac{Hh}{\pi(b-a)}\int_a^b\frac{dr}{r^2}=-k\int_{T_1}^{T_2}dT"))\
        .di("Uno entre erre cuadrado es erre a la menos dos; regla de la potencia: se suma uno y se divide.".replace("erre cuadrado", dice("r²", "erre cuadrado")).replace("erre a la menos dos", dice("r⁻²", "erre a la menos dos")),
            M(r"\int r^{-2}\,dr=\frac{r^{-2+1}}{-2+1}=\frac{r^{-1}}{-1}=-\frac{1}{r}"), NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Arriba menos abajo; menos por menos da más. Luego, la resta con denominador común.", M(r"\int_a^b\frac{dr}{r^2}=\Big[-\frac{1}{r}\Big]_a^b=\left(-\frac{1}{b}\right)-\left(-\frac{1}{a}\right)=\frac{1}{a}-\frac{1}{b}=\frac{b-a}{ab}"))
    L.paso("A) La derecha, juntar y despejar H")\
        .di("La de la temperatura: la suma de los pedacitos.", M(r"-k\int_{T_1}^{T_2}dT=-k\,(T_2-T_1)=k\,(T_1-T_2)"))\
        .di("Juntamos: el factor be menos a se cancela.".replace("be menos a", dice("(b − a)", "be menos a")), M(r"\frac{Hh}{\pi(b-a)}\cdot\frac{b-a}{ab}=k\,(T_1-T_2)\;\Rightarrow\;\frac{Hh}{\pi ab}=k\,(T_1-T_2)"))\
        .di("Despejamos hache.".replace("hache", dice("H", "hache")), M(r"H = \frac{\pi k a b\,(T_1-T_2)}{h}", "fh", "azul"))
    L.paso("A) Sustituir los datos")\
        .di("Sustituimos.", V(foco="perfil"), M(r"H = \frac{\pi(50{,}0)(0{,}100)(0{,}450)(100-0)}{2{,}00}"))\
        .di("Multiplicamos arriba.", M(rf"H=\frac{{{tex(num, 4)}}}{{2{{,}}00}}"))\
        .di(f"Y dividimos: {nu(H, 'W', 4)}.", R(rf"H = {tex(H, 4, u='W')}", "ra"))
    L.paso("B) Calor en 5,00 minutos")\
        .di("De la hoja, la corriente de calor es calor por tiempo; con hache constante se despeja Q.".replace("hache", dice("H", "hache")),
            F(r"H = \frac{dQ}{dt} \;\Rightarrow\; Q = H\,\Delta t"))\
        .di("El tiempo en segundos.", M(r"\Delta t=(5{,}00)(60)=300\ \text{s}"))\
        .di("Multiplicamos.", M(rf"Q = ({tex(H, 4)})(300)"), R(rf"Q = {tex(Q, 3, u='J', sci='si')}", "rb"))
    receta_cono(L, a, b, h, eje="y", nr=("a", "b"), despues_de=0)
    L.responde("A", rf"H={tex(H, 4)}\ \text{{W}}", H, "W").responde("B", rf"Q={tex(Q, 3, sci='si')}\ \text{{J}}", Q, "J")
    return L.oficial("III Parcial extraordinario II-2024", 353.4, 1.06e5).usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}")

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
            M(rf"A = \frac{{\pi d^2}}{{4}} = \frac{{\pi(0{{,}}15)^2}}{{4}} = {tex(A, 5, u='m^2', sci='si')}"),
            NOTA("El área se guarda con cinco cifras: se usa en las tres cuentas que siguen."))\
        .di("Usamos la ley de Fourier de la hoja en la plata.", F(r"H = kA\,\frac{T_H-T_C}{L}"), M(r"H = \frac{k_3 A\,(T_{23}-T_C)}{L_3}"))\
        .di("Sustituimos; arriba multiplicamos y luego dividimos.",
            M(rf"H = \frac{{(406{{,}}0)({tex(A, 5, sci='si')})(22{{,}}2-0)}}{{0{{,}}20}}"))\
        .di(f"Da {nu(H, 'W', 4)}.", M(rf"H = {tex(H, 4, u='W')}", "h", "verde"))\
        .por_que("Están en serie y aisladas lateralmente, así que ese mismo flujo cruza las tres varillas.")
    L.paso("A) Temperatura en la unión aluminio-acero")\
        .di("Usamos el mismo flujo en el tramo de aluminio.", V(foco="perfil"), M(r"H = \frac{k_1 A\,(T_H - T_{12})}{L_1}"))\
        .di("Despejamos: multiplicamos por ele uno, dividimos entre ka uno por A, y pasamos te doce.".replace("ele uno", dice("L₁", "ele uno")).replace("ka uno por A", dice("k₁A", "ka uno por A")).replace("te doce", dice("T₁₂", "te doce")),
            M(r"\frac{H L_1}{k_1 A}=T_H - T_{12}\;\Rightarrow\; T_{12} = T_H - \frac{H L_1}{k_1 A}"))\
        .di("Sustituimos; primero la fracción y luego la resta.",
            M(rf"T_{{12}} = 100{{,}}0 - \frac{{({tex(H, 4)})(0{{,}}15)}}{{(205{{,}}0)({tex(A, 5, sci='si')})}}"))\
        .di(f"Resulta {nu(t12, '°C', dec=1)}.", R(rf"T_{{12}} = {tex(t12, dec=1, u='°C')}", "ra"))
    L.paso("B) Largo de la varilla de acero")\
        .di("El mismo flujo cruza el acero; ahí la incógnita es el largo.", M(r"H = \frac{k_2 A\,(T_{12}-T_{23})}{L_2}"))\
        .di("Despejamos: multiplicamos por ele dos y dividimos entre hache.".replace("ele dos", dice("L₂", "ele dos")).replace("hache", dice("H", "hache")),
            M(r"L_2 = \frac{k_2 A\,(T_{12}-T_{23})}{H}"))\
        .di("Sustituimos con el salto entre las dos uniones.",
            M(rf"L_2 = \frac{{(50{{,}}2)({tex(A, 5, sci='si')})({tex(t12, dec=1)}-22{{,}}2)}}{{{tex(H, 4)}}}"))\
        .di(f"Obtenemos {nu(L2, 'm', 3)}.", R(rf"L_2 = {tex(L2, 3, u='m')}\ \ (\approx 5{{,}}0\ \text{{cm}})", "rb"))
    L.responde("A", rf"T_{{12}}={tex(t12, dec=1)}\ ^\circ\text{{C}}", t12, "°C").responde("B", rf"L_2={tex(L2, 3)}\ \text{{m}}", L2, "m")
    return L.oficial("III Parcial ordinario II-2024", 67.0, 0.050).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q043
@leccion("Q043")
def q043() -> Leccion:
    th, tc, L_b = 80.0, 20.0, 0.20
    r0, r1, ka, kb = 0.0120, 0.0060, 385.0, 50.0
    num_a = math.pi * ka * r0 * r1 * (th - tc)
    ha = num_a / L_b
    ab = math.pi * r1 ** 2
    hb = kb * ab * (th - tc) / L_b
    ht = ha + hb
    L = _des("Q043", "Dos barras en paralelo: cobre cónico y acero cilíndrico",
             "Dos barras de largo $L=0{,}20$ m conectan en paralelo $T_H=80{,}0\\ ^\\circ$C y $T_C=20{,}0\\ ^\\circ$C. Barra A (cobre, $k_A=385$): radio lineal de $R_0=1{,}20$ cm a $R_1=0{,}60$ cm. Barra B (acero, $k_B=50$): radio constante $R_1=0{,}60$ cm. A) $H_A$. B) $H_B$. C) $H_{\\text{total}}$.",
             claves=["paralelo", "cono truncado", "cobre y acero"],
             visual={"tipo": "conduccion", "geo": "paralelo", "L": L_b, "T1": th, "T2": tc, "uT": "°C",
                     "barras": [{"n": "A: Cu cónica", "A": 2, "forma": "1 - 0.5 * u"}, {"n": "B: Acero", "A": 1}]})
    L.paso("A) Fourier en la barra cónica")\
        .di("El radio de la barra A cambia a lo largo de ella; partimos de la forma diferencial de Fourier de la hoja.", V(foco=0), DATO("radio varía linealmente"),
            F(r"H_A = -k_A A(x)\,\frac{dT}{dx}"))\
        .di("El radio baja en línea recta de erre cero a erre uno, y el área es un círculo.".replace("erre cero", dice("R₀", "erre cero")).replace("erre uno", dice("R₁", "erre uno")),
            M(r"R(x) = R_0 - \frac{R_0-R_1}{L}\,x \qquad A(x) = \pi R(x)^2"))
    L.paso("A) Cambio de variable al radio")\
        .di("Derivamos el radio para pasar de equis a erre.".replace("equis a erre", dice("x a R", "equis a erre")), M(r"dR=-\frac{R_0-R_1}{L}\,dx\;\Rightarrow\;dx=-\frac{L}{R_0-R_1}\,dR"))\
        .di("Separamos: multiplicamos por de equis y dividimos entre pi erre cuadrado.".replace("de equis", dice("dx", "de equis")).replace("pi erre cuadrado", dice("πR²", "pi erre cuadrado")), M(r"H_A\,\frac{dx}{\pi R^2}=-k_A\,dT"))\
        .di("Integramos toda la barra: cuando equis vale cero el radio es erre cero, y al final es erre uno.".replace("equis", dice("x", "equis")).replace("erre cero", dice("R₀", "erre cero")).replace("erre uno", dice("R₁", "erre uno")),
            M(r"H_A\int_0^L\frac{dx}{\pi R^2}=-k_A\int_{T_H}^{T_C}dT"))
    L.paso("A) La integral de la izquierda")\
        .di("Ponemos de equis en términos de de erre. Los límites cambian: en equis cero el radio es erre cero; en equis ele, erre uno.".replace("de equis", dice("dx", "de equis")).replace("de erre", dice("dR", "de erre")).replace("equis cero", dice("x = 0", "equis cero")).replace("erre cero", dice("R₀", "erre cero")).replace("equis ele", dice("x = L", "equis ele")).replace("erre uno", dice("R₁", "erre uno")),
            M(r"\int_0^L\frac{dx}{\pi R^2}=-\frac{L}{\pi(R_0-R_1)}\int_{R_0}^{R_1}\frac{dR}{R^2}"))\
        .di("Un signo menos delante se quita dando vuelta a los límites.", M(r"=\frac{L}{\pi(R_0-R_1)}\int_{R_1}^{R_0}\frac{dR}{R^2}"))\
        .di("Regla uno: uno entre erre cuadrado da menos uno entre erre. Arriba menos abajo.".replace("erre cuadrado", dice("R²", "erre cuadrado")).replace("entre erre.", "entre " + dice("R", "erre") + "."),
            M(r"\int_{R_1}^{R_0}\frac{dR}{R^2}=\Big[-\frac{1}{R}\Big]_{R_1}^{R_0}=-\frac{1}{R_0}+\frac{1}{R_1}=\frac{1}{R_1}-\frac{1}{R_0}"), NOTA("Reglas 1 y 3 de la lección **T-MAT2**."))
    L.paso("A) Simplificar, la derecha y despejar")\
        .di("Juntamos lo anterior.", M(r"\int_0^L\frac{dx}{\pi R^2}=\frac{L}{\pi(R_0-R_1)}\left(\frac{1}{R_1}-\frac{1}{R_0}\right)"))\
        .di("Como uno entre erre uno menos uno entre erre cero es la resta sobre el producto, se cancela.".replace("erre uno", dice("R₁", "erre uno")).replace("erre cero", dice("R₀", "erre cero")),
            M(r"\frac{1}{R_1}-\frac{1}{R_0}=\frac{R_0-R_1}{R_0R_1}\;\Rightarrow\;\int_0^L\frac{dx}{\pi R^2}=\frac{L}{\pi R_0R_1}"))\
        .di("La de la derecha.", M(r"-k_A\int_{T_H}^{T_C}dT=k_A\,(T_H-T_C)"))\
        .di("Igualamos y despejamos.", M(r"H_A\,\frac{L}{\pi R_0R_1}=k_A(T_H-T_C)\;\Rightarrow\;H_A = \frac{\pi k_A R_0 R_1\,(T_H-T_C)}{L}"))
    L.paso("A) Sustituir")\
        .di("Radios en metros.", M(rf"H_A = \frac{{\pi(385)(0{{,}}0120)(0{{,}}0060)(80{{,}}0-20{{,}}0)}}{{0{{,}}20}}"))\
        .di("Multiplicamos arriba.", M(rf"H_A=\frac{{{tex(num_a, 4)}}}{{0{{,}}20}}"))\
        .di(f"Da {nu(ha, 'W', 3)}.", R(rf"H_A = {tex(ha, 3, u='W')}", "ra"))
    L.paso("B) Flujo por la barra cilíndrica de acero")\
        .di("La barra B tiene área constante: la ley de Fourier de la hoja para una barra uniforme.", V(foco=1),
            F(r"H = kA\,\frac{T_H-T_C}{L}"))\
        .di("Su área es un círculo de radio erre uno.".replace("erre uno", dice("R₁", "erre uno")), M(rf"A_B=\pi R_1^2=\pi(0{{,}}0060)^2={tex(ab, 4, u='m^2')}"))\
        .di("Sustituimos.", M(rf"H_B = \frac{{(50)({tex(ab, 4)})(60{{,}}0)}}{{0{{,}}20}}"))\
        .di(f"Da {nu(hb, 'W', 3)}.", R(rf"H_B = {tex(hb, 3, u='W')}", "rb"))
    L.paso("C) Flujo total en paralelo")\
        .di("Las dos barras unen los mismos depósitos: el calor que sale del caliente se reparte entre ellas, así que los flujos se suman.", V(foco="total"),
            M(r"H_T = H_A + H_B"), M(rf"H_T = {tex(ha, 3)} + {tex(hb, 3)}"),
            R(rf"H_T = {tex(ht, 3, u='W')}", "rc"))
    receta_cono(L, r0, r1, L_b, nr=("R₀", "R₁"), despues_de=0)
    L.responde("A", rf"H_A={tex(ha, 3)}\ \text{{W}}", ha, "W").responde("B", rf"H_B={tex(hb, 3)}\ \text{{W}}", hb, "W").responde("C", rf"H_T={tex(ht, 3)}\ \text{{W}}", ht, "W")
    return L.oficial("III Parcial extraordinario I-2025", 26.1, 1.70, 27.8).usa(r"H=-kA\frac{dT}{dx}", r"H=kA\frac{\Delta T}{L}")

# ============================================================================ Q044
@leccion("Q044")
def q044() -> Leccion:
    r1, r2, r3 = 0.0500, 0.0800, 0.1000
    t1, t3, k1, k2 = 125.0, 40.0, 50.0, 1.50
    g1 = 1.0 / r1 - 1.0 / r2
    g2 = 1.0 / r2 - 1.0 / r3
    R1 = g1 / (4.0 * math.pi * k1)
    R2 = g2 / (4.0 * math.pi * k2)
    H = (t1 - t3) / (R1 + R2)
    t2 = t1 - H * R1
    L = _des("Q044", "Reactor esférico de acero con revestimiento cerámico",
             "Reactor esférico: acero ($r_1=5{,}00$ cm a $r_2=8{,}00$ cm, $k_1=50{,}0$ W/m·K) cubierto por cerámica ($r_2$ a $r_3=10{,}00$ cm, $k_2=1{,}50$ W/m·K). Interior a $T_1=125{,}0\\ ^\\circ$C, exterior a $T_3=40{,}0\\ ^\\circ$C. A) Temperatura $T_2$ en la interfaz. B) Flujo estacionario $H$.",
             claves=["esfera compuesta", "dos capas", "reactor"],
             visual={"tipo": "conduccion", "geo": "esfera2", "r1": r1, "r2": r2, "r3": r3,
                     "T1": t1, "T2": round(t2, 1), "T3": t3, "k1": k1, "k2": k2, "n1": "acero", "n2": "cerámica", "uT": "°C"})
    L.paso("Fourier en un cascarón esférico")\
        .di("El calor sale en dirección radial. Partimos de la ley de Fourier de la hoja.", V(foco="dr"), DATO("cascarón esférico"), F(r"H = -kA\,\frac{dT}{dr}", "f"))\
        .di("El área de una esfera de radio erre es cuatro pi erre cuadrado.".replace("radio erre", "radio " + dice("r", "erre")).replace("pi erre cuadrado", dice("4πr²", "pi erre cuadrado")),
            M(r"A=4\pi r^2\;\Rightarrow\;H = -k\,(4\pi r^2)\,\frac{dT}{dr}"))\
        .di("Separamos: multiplicamos por de erre y dividimos entre cuatro pi erre cuadrado.".replace("de erre", dice("dr", "de erre")).replace("cuatro pi erre cuadrado", dice("4πr²", "cuatro pi erre cuadrado")),
            M(r"H\,dr=-4\pi k\,r^2\,dT\;\Rightarrow\;H\,\frac{dr}{r^2}=-4\pi k\,dT"))
    L.paso("Integrar una capa")\
        .di("Integral a los dos lados, del radio interno al externo de la capa.",
            M(r"H\int_{r_{\text{int}}}^{r_{\text{ext}}}\frac{dr}{r^2} = -4\pi k\int_{T_{\text{int}}}^{T_{\text{ext}}} dT"))\
        .di("Regla uno: uno entre erre cuadrado da menos uno entre erre. Arriba menos abajo.".replace("erre cuadrado", dice("r²", "erre cuadrado")).replace("entre erre.", "entre " + dice("r", "erre") + "."),
            M(r"\int_{r_{\text{int}}}^{r_{\text{ext}}}\frac{dr}{r^2}=\Big[-\frac{1}{r}\Big]_{r_{\text{int}}}^{r_{\text{ext}}}=\frac{1}{r_{\text{int}}}-\frac{1}{r_{\text{ext}}}"), NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Derecha: la suma de pedacitos de temperatura; el menos voltea la resta.", M(r"-4\pi k\,(T_{\text{ext}}-T_{\text{int}})=4\pi k\,(T_{\text{int}}-T_{\text{ext}})"))\
        .di("Juntamos.", M(r"H\left(\frac{1}{r_{\text{int}}}-\frac{1}{r_{\text{ext}}}\right)=4\pi k\,(T_{\text{int}}-T_{\text{ext}})"))
    L.paso("Factor geométrico de cada capa")\
        .di("Despejamos la caída de temperatura de una capa.", M(r"T_{\text{int}} - T_{\text{ext}} = H\,\frac{\frac{1}{r_{\text{int}}}-\frac{1}{r_{\text{ext}}}}{4\pi k}"))\
        .di("Para no arrastrar la fracción, llamamos R uno y R dos a ese factor en cada capa.",
            M(rf"R_1 = \frac{{\frac{{1}}{{0{{,}}05}}-\frac{{1}}{{0{{,}}08}}}}{{4\pi(50{{,}}0)}} = \frac{{{tex(g1, 4)}}}{{{tex(4 * math.pi * k1, 4)}}} = {tex(R1, 4, u='K/W')}"),
            M(rf"R_2 = \frac{{\frac{{1}}{{0{{,}}08}}-\frac{{1}}{{0{{,}}10}}}}{{4\pi(1{{,}}50)}} = \frac{{{tex(g2, 4)}}}{{{tex(4 * math.pi * k2, 4)}}} = {tex(R2, 4, u='K/W')}"))
    L.paso("A) Temperatura en la interfaz acero-cerámica")\
        .di("Las capas están en serie: el mismo flujo cruza las dos. Igualamos.", V(foco="perfil"),
            M(r"H=\frac{T_1-T_2}{R_1} = \frac{T_2-T_3}{R_2}"))\
        .di("Multiplicamos en cruz: cada resistencia pasa a multiplicar al otro lado.",
            M(r"R_2(T_1-T_2)=R_1(T_2-T_3)"))\
        .di("Abrimos los paréntesis: cada resistencia multiplica a las dos temperaturas.",
            M(r"R_2T_1-R_2T_2=R_1T_2-R_1T_3"))\
        .di("Juntamos lo que tiene te dos a la derecha, y lo que no, a la izquierda. Cada término cambia de signo al pasar.".replace("te dos", dice("T₂", "te dos")),
            M(r"R_2T_1+R_1T_3=R_1T_2+R_2T_2"))\
        .di("Sacamos te dos de factor común.".replace("te dos", dice("T₂", "te dos")),
            M(r"R_2T_1+R_1T_3=T_2\,(R_1+R_2)"))\
        .di("Dividimos los dos lados entre la suma de resistencias.",
            M(r"T_2 = \frac{R_2 T_1 + R_1 T_3}{R_1+R_2}"))\
        .di("Sustituimos.", M(rf"T_2 = \frac{{({tex(R2, 4)})(125{{,}}0)+({tex(R1, 4)})(40{{,}}0)}}{{{tex(R1, 4)}+{tex(R2, 4)}}}"))\
        .di("Arriba y abajo por separado.", M(rf"T_2=\frac{{{tex(R2 * t1, 4)}+{tex(R1 * t3, 4)}}}{{{tex(R1 + R2, 4)}}}=\frac{{{tex(R2 * t1 + R1 * t3, 4)}}}{{{tex(R1 + R2, 4)}}}"))\
        .di(f"La cerámica aísla mucho más, así que casi toda la caída ocurre en ella: {nu(t2, '°C', dec=1)}.",
            R(rf"T_2 = {tex(t2, dec=1, u='°C')}", "ra"))
    L.paso("B) Flujo estacionario total")\
        .di("Sumamos las dos caídas: el salto total entre la suma de los factores.", V(foco="listo"),
            M(r"T_1-T_3=H(R_1+R_2)\;\Rightarrow\;H = \frac{T_1-T_3}{R_1+R_2}"))\
        .di("Sustituimos.", M(rf"H = \frac{{125{{,}}0-40{{,}}0}}{{{tex(R1 + R2, 4)}}} = \frac{{85{{,}}0}}{{{tex(R1 + R2, 4)}}}"))\
        .di(f"Da {nu(H, 'W', 3)}.", R(rf"H = {tex(H, 3, u='W')}", "rb"))
    L.responde("A", rf"T_2={tex(t2, dec=1)}\ ^\circ\text{{C}}", t2, "°C").responde("B", rf"H={tex(H, 3)}\ \text{{W}}", H, "W")
    return L.oficial("III Parcial ordinario I-2025", 118.0, 588.0).usa(r"H=-kA\frac{dT}{dx}")

# ============================================================================ Q045
@leccion("Q045")
def q045() -> Leccion:
    return _tubo("Q045", "Tubería cilíndrica de aluminio: flujo por metro y T a 6 cm",
                 "Tubería cilíndrica de aluminio ($k=235$ W/m·K) de $5{,}00$ m de largo, radio interno $4{,}00$ cm y externo $8{,}00$ cm. Adentro hay un fluido a $150\\ ^\\circ\\text{C}$ y afuera la tubería está a $45\\ ^\\circ\\text{C}$, en estado estacionario. A) Flujo de calor por unidad de longitud. B) Calor transferido en $10{,}0$ min. C) Temperatura a $6{,}00$ cm del eje.",
                 k=235.0, lc=5.00, r1=0.0400, r2=0.0800, t1=150.0, t2=45.0, rm=0.0600, dt_s=600.0, dt_txt="10,0 min",
                 aviso="El solucionario oficial de 25-II-E usó por error πr²L (un volumen) en vez del área lateral 2πrL. Aquí se resuelve con el área correcta.",
                 oficial=("III Parcial extraordinario II-2025", (6202.0, 18.61e6, 80.0), "El solucionario oficial usó A = πr²L (volumen) en vez del área lateral 2πrL; con 2πrL da 223,7 kW/m, 671,0 MJ y 88,6 °C."))

# ============================================================================ Q046
@leccion("Q046")
def q046() -> Leccion:
    return _area_lineal("Q046", "Barra trapezoidal de plata en lámina aislante",
                        "Una barra de plata ($k=429$ W/m·K) atraviesa una lámina aislante de grosor $h=0{,}100$ m. Su área cambia en línea recta de $A_{inf}=0{,}100\\ \\text{m}^2$ (abajo, $T_1=100{,}0\\ ^\\circ\\text{C}$) a $A_{sup}=0{,}200\\ \\text{m}^2$ (arriba, $T_2=20{,}0\\ ^\\circ\\text{C}$): $A(y)=A_{inf}+\\frac{A_{sup}-A_{inf}}{h}y$. A) Flujo de calor. B) Energía en 1 minuto. C) Temperatura a la mitad del grosor.",
                        ["H", "Q", "T"], k=429.0, h=0.100, a0=0.100, m=1.0, t1=100.0, t2=20.0, dt_s=60.0, dt_txt="1 minuto",
                        oficial=("III Parcial ordinario II-2025", (49510.0, 2.971e6, 53.21), ""))

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
        .di("Con el vidrio del lado exterior: ley de Fourier de la hoja en el vidrio.", V(foco="perfil"),
            F(r"H = kA\,\frac{T_H-T_C}{L}"), M(r"H = \frac{k_v A\,(T_f - T_{\text{ext}})}{\Delta x_v}"))\
        .di("Despejamos: multiplicamos por el espesor, dividimos entre ka A y pasamos la temperatura exterior sumando.".replace("ka A", dice("kA", "ka A")),
            M(r"\frac{H\,\Delta x_v}{k_v A}=T_f - T_{\text{ext}}\;\Rightarrow\; T_f = T_{\text{ext}} + \frac{H\,\Delta x_v}{k_v A}"))\
        .di(f"Sustituimos; la fracción primero. Obtenemos {nu(tf, '°C', dec=1)}.",
            M(rf"T_f = 5{{,}}0 + \frac{{(1{{,}}46\times 10^4)(0{{,}}00400)}}{{(0{{,}}960)(11{{,}}0)}} = 5{{,}}0+\frac{{{tex(H * dv, 4)}}}{{{tex(kv * A, 4)}}} = 5{{,}}0+{tex(dt_v, 4)} = {tex(tf, dec=2, u='°C')}"),
            R(rf"T_f = {tex(tf, dec=1, u='°C')}", "ra"),
            NOTA("Si el vidrio se colocara del lado interior, $T_f=24{,}5\\ ^\\circ\\text{C}$; $k_{pc}$ y ambos gradientes quedan iguales."))
    L.paso("B) Conductividad del policarbonato")\
        .di("En el policarbonato cae el resto de la temperatura.",
            M(rf"\Delta T_{{pc}} = 30{{,}}0 - {tex(tf, dec=2)} = {tex(dt_p, dec=2, u='°C')}"))\
        .di(f"De Fourier despejamos la conductividad: multiplicamos por el espesor y dividimos entre A por delta te. Sale {nu(kp, 'W/(m·K)', 3)}.".replace("A por delta te", dice("AΔT", "A por delta te")),
            M(r"H=\frac{k_{pc}A\,\Delta T_{pc}}{\Delta x_{pc}}\;\Rightarrow\;k_{pc} = \frac{H\,\Delta x_{pc}}{A\,\Delta T_{pc}}"),
            M(rf"k_{{pc}} = \frac{{(1{{,}}46\times 10^4)(0{{,}}00300)}}{{(11{{,}}0)({tex(dt_p, dec=2)})}}=\frac{{{tex(H * dp, 4)}}}{{{tex(A * dt_p, 5)}}}"),
            R(rf"k_{{pc}} = {tex(kp, 3, u='W/(m·K)')}", "rb"))
    L.paso("C) Gradientes de temperatura")\
        .di("El gradiente en cada lámina es el cambio de temperatura entre su espesor; de Fourier, es hache entre ka A.".replace("hache entre ka A", dice("H/(kA)", "hache entre ka A")),
            M(r"H=kA\,\frac{\Delta T}{\Delta x}\;\Rightarrow\;\left|\frac{\Delta T}{\Delta x}\right|=\frac{H}{kA}"),
            M(rf"\left|\frac{{\Delta T}}{{\Delta x}}\right|_v = \frac{{H}}{{k_v A}} = \frac{{1{{,}}46\times10^4}}{{(0{{,}}960)(11{{,}}0)}} = {tex(gv, 4, u='K/m')}"),
            M(rf"\left|\frac{{\Delta T}}{{\Delta x}}\right|_{{pc}} = \frac{{H}}{{k_{{pc}} A}} = \frac{{1{{,}}46\times10^4}}{{({tex(kp, 4)})(11{{,}}0)}} = {tex(gp, 4, u='K/m')}"),
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
            M(rf"H = \frac{{(0{{,}}800)(12{{,}}0)(20{{,}}0)}}{{0{{,}}200}}"))\
        .di("Multiplicamos arriba: da ciento noventa y dos. Luego dividimos entre cero coma dos.", M(r"H = \frac{192}{0{,}200}"),
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
    return _area_lineal("Q049", "Barra piramidal de aluminio A(y) = 0,220 − 0,157y",
                        "Dos reservorios, uno a $200{,}0\\ ^\\circ\\text{C}$ (abajo) y otro a $25{,}0\\ ^\\circ\\text{C}$ (arriba), se conectan con un bloque de aluminio ($k=235$ W/m·K) de $0{,}480$ m de altura, con caras cuadradas y área $A(y)=0{,}220-0{,}157y$ (m²). A) Lado de la cara inferior y superior. B) Flujo de calor. C) Temperatura a la mitad de la altura. D) Energía en 2 min. E) ¿Cambia el flujo si se intercambian los reservorios?",
                        ["lados", "H", "T", "Q", "inter"], k=235.0, h=0.480, a0=0.220, m=-0.157, t1=200.0, t2=25.0, dt_s=120.0, dt_txt="2 min",
                        oficial=("III Parcial ordinario I-2026", (0.469, 0.380, 15397.0, 121.6, 1.85e6), ""), et_a="0,220−0,157y")

# ============================================================================ Q050
@leccion("Q050")
def q050() -> Leccion:
    A, d, t1, t2 = 4.00, 0.0200, 300.0, 100.0
    I_tot = math.log(220.0 / 200.0) / 1000.0
    H = A * (t1 - t2) / I_tot
    Q = H * (5.0 * 60.0)
    I_mid = math.log(210.0 / 200.0) / 1000.0
    tm = t1 - H * I_mid / A
    pts = [[round(d * i / 40, 5), round(t1 - (t1 - t2) * math.log((200.0 + 1000.0 * d * i / 40) / 200.0) / math.log(1.1), 2)] for i in range(41)]
    L = _des("Q050", "Lámina con conductividad variable k(y) = 200 + 1000y",
             "Una lámina de $4{,}00\\ \\text{m}^2$ y $2{,}00$ cm de grosor conduce calor entre $T_1=300\\ ^\\circ\\text{C}$ (cara inferior, $y=0$) y $T_2=100\\ ^\\circ\\text{C}$ (cara superior, $y=d$). La conductividad cambia con la altura: $k(y)=200+1000y$ en W/(m·K) con $y$ en metros. A) Flujo de calor. B) Energía en 5 minutos. C) Temperatura en $y=d/2$.",
             aviso="El solucionario oficial de la Reposición II-2025 olvidó dividir entre 1000 al cambiar de variable y dio el flujo mil veces más pequeño. La temperatura del inciso C sí coincide, porque ese factor se cancela.",
             claves=["conductividad variable", "cambio de variable", "integral", "logaritmo"],
             visual={"tipo": "conduccion", "geo": "pared", "L": d, "T1": t1, "T2": t2, "eje": "y", "uT": "°C", "pts": pts, "xm": d / 2})
    L.paso("Qué nos dan y qué piden")\
        .di("Una lámina caliente abajo y fría arriba. Lo raro es que conduce mejor mientras más arriba.", DATO("k(y) = 200 + 1000 y"),
            LISTA(["$A=4{,}00\\ \\text{m}^2$", "$d=2{,}00\\ \\text{cm}=0{,}0200$ m", "$T_1=300\\ ^\\circ\\text{C}$ en $y=0$, $T_2=100\\ ^\\circ\\text{C}$ en $y=d$", "$k(y)=200+1000y$"]))\
        .di("Como ka cambia, la fórmula con delta te entre ele no sirve: hay que integrar.".replace("ka cambia", dice("k", "ka") + " cambia").replace("delta te entre ele", dice("ΔT/L", "delta te entre ele")),
            OJO("$H=kA\\frac{\\Delta T}{L}$ solo vale si $k$ es constante."))
    L.paso("A) Fourier en una rebanada delgadita")\
        .di("Tomamos una capa de grosor de ye. El área no cambia: es la de la lámina.".replace("de ye", dice("dy", "de ye")), V(foco="dx"), F(r"H=-kA\frac{dT}{dy}"))\
        .di("Ponemos la conductividad de esa altura.", M(r"H=-(200+1000y)\,A\,\frac{dT}{dy}"))\
        .por_que("En estado estacionario el mismo flujo $H$ cruza todas las capas; lo que cambia de capa en capa es la caída de temperatura.")
    L.paso("A) Separar variables")\
        .di("Separamos: multiplicamos por de ye y dividimos entre el área y entre la conductividad de esa altura.".replace("de ye", dice("dy", "de ye")),
            M(r"H\,dy=-(200+1000y)\,A\,dT\;\Rightarrow\;\frac{H}{A}\cdot\frac{dy}{200+1000y}=-dT"), NOTA("Receta en la lección **T-MAT3**; la integral de la izquierda es la regla 3 de **T-MAT2**."))\
        .di("Integramos de cero a de, y de trescientos a cien grados.".replace("a de,", "a " + dice("d", "de") + ","), M(r"\frac{H}{A}\int_0^{0{,}0200}\frac{dy}{200+1000y}=-\int_{300}^{100}dT", "int", "azul"))\
        .di("La de la derecha es fácil: menos cien menos trescientos, con el signo de afuera da doscientos.", M(r"-\int_{300}^{100}dT=-(100-300)=200"))
    L.paso("A) La integral de la izquierda: cambio de variable")\
        .di("Llamamos u a lo de abajo.", M(r"u=200+1000y\qquad du=1000\,dy\;\Rightarrow\;dy=\frac{du}{1000}"))\
        .di("Los límites también cambian.", M(r"y=0\Rightarrow u=200\qquad y=0{,}0200\Rightarrow u=220"))\
        .di("Queda un logaritmo, dividido entre mil.", M(r"\int_0^{0{,}0200}\frac{dy}{200+1000y}=\frac{1}{1000}\int_{200}^{220}\frac{du}{u}=\frac{1}{1000}\ln\frac{220}{200}"))\
        .di("Ese uno entre mil es justo lo que se le olvidó al solucionario.", OJO("No se olvide el $\\frac{1}{1000}$ que sale de $dy=\\frac{du}{1000}$."))
    L.paso("A) Despejar el flujo")\
        .di("Juntamos las dos integrales.", M(r"\frac{H}{A}\cdot\frac{\ln(1{,}1)}{1000}=200"))\
        .di("Despejamos.", M(r"H=\frac{1000\,A\,(200)}{\ln(1{,}1)}=\frac{1000(4{,}00)(200)}{0{,}09531}"))\
        .di(f"Son {nu(H / 1e6, 'MW', 3)}: muchísimo, porque la lámina es delgada y conduce muy bien.", R(rf"H={tex(H, 3, u='W')}={tex(H / 1e6, 3, u='MW')}", "ra"))
    L.paso("B) Energía en cinco minutos")\
        .di("Partimos de la definición de flujo de calor de la hoja: despejamos el calor multiplicando por los cinco minutos pasados a segundos.", F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"), M(r"\Delta t=5\ \text{min}=300\ \text{s}"))\
        .di("Multiplicamos la potencia estacionaria en watts por trescientos segundos para obtener la energía total en gigajoules.", M(rf"Q=({tex(H, 4)})(300)"), R(rf"Q={tex(Q, 3, u='J')}={tex(Q / 1e9, 3, u='GJ')}", "rb"))
    L.paso("C) Temperatura a la mitad del grosor")\
        .di("Integramos solo hasta la mitad, y igual a cero coma cero cien.".replace("y igual a", dice("y =", "ye igual a")), V(foco="punto"), M(r"\frac{H}{A}\int_0^{0{,}0100}\frac{dy}{200+1000y}=-\int_{300}^{T}dT"))\
        .di("A la mitad, u llega a doscientos diez.", M(r"\frac{H}{A}\cdot\frac{1}{1000}\ln\frac{210}{200}=300-T"))\
        .di("Despejamos la temperatura.", M(rf"T=300-\frac{{H\ln(1{{,}}05)}}{{1000\,A}}=300-\frac{{({tex(H, 4)})(0{{,}}04879)}}{{4000}}"))\
        .di(f"Queda en {nu(tm, '°C', dec=1)}: un poco más abajo del promedio, porque arriba conduce mejor.", R(rf"T(d/2)={tex(tm, dec=1)}\ ^\circ\text{{C}}", "rc"))
    L.responde("A", rf"H={tex(H / 1e6, 3)}\ \text{{MW}}", H, "W").responde("B", rf"Q={tex(Q / 1e9, 3)}\ \text{{GJ}}", Q, "J").responde("C", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
    return L.oficial("Reposición II-2025", 8394.0, 2.518e6, 197.6,
                     nota="El solucionario oficial omitió el factor 1/1000 de du = 1000 dy (obtuvo kW en vez de MW); T(d/2) = 197,6 °C sí coincide porque el factor se cancela.").usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}")

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
        .di("El área es alto por ancho, y el grosor pasa a metros.", DATO("20,0 cm"),
            LISTA(["$A = (4{,}00)(3{,}00) = 12{,}0\\ \\text{m}^2$", "$L = 20{,}0\\ \\text{cm} = 0{,}200$ m", "$T_1-T_2 = 30{,}0-10{,}0 = 20{,}0\\ ^\\circ\\text{C}$"]))\
        .di("Sustituimos el área, el grosor y el salto de temperatura en la casilla de conducción estacionaria de la hoja.", F(r"H = kA\frac{T_H-T_C}{L}"),
            M(rf"H = \frac{{(0{{,}}800)(12{{,}}0)(30{{,}}0-10{{,}}0)}}{{0{{,}}200}}=\frac{{(0{{,}}800)(12{{,}}0)(20{{,}}0)}}{{0{{,}}200}}"))\
        .di("Multiplicamos arriba: da ciento noventa y dos. Luego dividimos entre cero coma dos.", M(r"H = \frac{192}{0{,}200}"),
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
        .di("Convertimos el área a metros cuadrados: un centímetro cuadrado es diez a la menos cuatro metros cuadrados.", DATO("4.00 cm2"),
            M(r"A = 4{,}00\ \text{cm}^2=4{,}00\times 10^{-4}\ \text{m}^2"))\
        .di("Del cobre conocemos todo: sus dos temperaturas y su largo. Usamos la ley de Fourier de la hoja.", V(foco="flujo"),
            F(r"H = kA\,\frac{T_H - T_C}{L}"))\
        .di("En el cobre, los extremos están a cien y a sesenta y cinco grados.", M(r"H = k_{\text{Cu}} A\,\frac{T_H - T_u}{L_1}"),
            M(rf"H = (385)(4{{,}}00\times 10^{{-4}})\,\frac{{100-65{{,}}0}}{{1{{,}}00}}=(0{{,}}154)(35{{,}}0)"))\
        .di(f"Da {nu(H, 'W', 3)}.", R(rf"H = {tex(H, 3, u='W')}", "ra"))
    L.paso("(b) Longitud del tramo de acero")\
        .di("En serie, ese mismo flujo atraviesa el acero.", V(foco="perfil"),
            M(r"H = k_{\text{ac}} A\,\frac{T_u - T_C}{L_2}"))\
        .di("Despejamos el largo.", M(r"L_2 = \frac{k_{\text{ac}} A\,(T_u - T_C)}{H}"))\
        .di("Sustituimos.", M(rf"L_2 = \frac{{(50{{,}}2)(4{{,}}00\times 10^{{-4}})(65{{,}}0-0)}}{{{tex(H, 3)}}}=\frac{{{tex(k_ac * A * (tu - tc), 4)}}}{{{tex(H, 3)}}}"))\
        .di(f"Da {nu(l_ac, 'm', 3)}.", R(rf"L_2 = {tex(l_ac, 3, u='m')}", "rb"))
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
    L.paso("Qué nos dan y qué piden")\
        .di("Dos barras una al lado de la otra, entre los mismos dos depósitos. Todo queda en letras.", V(foco="total"), DATO("una al lado de la otra"),
            LISTA(["Barra 1: $k_1$, área $A_1=2A$, largo $L$", "Barra 2: $k_2$, área $A_2=A$, largo $L$", "Extremos: $T_1=2T$ y $T_2=T$"]))\
        .di("Las dos barras ven la misma diferencia de temperatura.", M(r"\Delta T = T_1 - T_2 = 2T - T = T"))
    L.paso("Fourier en cada barra")\
        .di("Cada barra tiene área constante: usamos la ley de Fourier de la hoja.", F(r"H = kA\,\frac{T_H-T_C}{L}"))\
        .di("Barra uno, con área dos A.", V(foco=0), M(r"H_1 = k_1(2A)\,\frac{2T-T}{L}=\frac{2k_1AT}{L}"))\
        .di("Barra dos, con área A.", V(foco=1), M(r"H_2 = k_2(A)\,\frac{2T-T}{L}=\frac{k_2AT}{L}"))
    L.paso("Flujo total en paralelo")\
        .di("El calor que sale del depósito caliente se reparte entre las dos barras: los flujos se suman.", V(foco="total"),
            M(r"H_{\text{total}} = H_1 + H_2"))\
        .di("Sustituimos.", M(r"H_{\text{total}} = \frac{2k_1AT}{L}+\frac{k_2AT}{L}"))\
        .di("Sacamos factor común A te entre ele.".replace("A te entre ele", dice("AT/L", "A te entre ele")),
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
        .di("Cancelamos ka, A y ele, que están en los dos lados, y abrimos el paréntesis.",
            M(r"2T - T_u = 2(T_u - T)=2T_u - 2T"))\
        .di("Las te u a un lado y las te al otro: sumamos te u y dos te a los dos lados.".replace("te u", dice("Tᵤ", "te u")).replace("dos te", dice("2T", "dos te")),
            M(r"2T+2T = 2T_u+T_u \;\Rightarrow\; 4T = 3T_u"), R(r"T_u = \frac{4}{3}\,T", "ra"))
    L.paso("(b) Flujo total sumando las dos ramas")\
        .di("Flujo de la rama superior, con ka uno igual a un tercio de ka; los dos de arriba y de abajo se cancelan.", V(foco=0),
            M(r"H_1 = \frac{(k/3)(2A)(2T-T)}{2L} = \frac{2}{3\cdot2}\cdot\frac{kAT}{L} = \frac{1}{3}\frac{kAT}{L}"))\
        .di("En la rama inferior sustituimos la temperatura de unión en la barra dos. Dos te menos cuatro tercios de te son dos tercios de te.".replace("Dos te menos cuatro tercios de te son dos tercios de te", dice("2T − 4T/3 = 2T/3", "Dos te menos cuatro tercios de te son dos tercios de te")),
            M(r"2T-\frac{4}{3}T=\frac{6}{3}T-\frac{4}{3}T=\frac{2}{3}T"), M(r"H_{23} = \frac{kA\,(2T - \frac{4}{3}T)}{L} = \frac{2}{3}\frac{kAT}{L}"))\
        .di("Sumamos las dos ramas en paralelo y sale uno exacto.", V(foco="total"),
            M(r"H_{\text{total}} = H_1 + H_{23} = \left(\frac{1}{3}+\frac{2}{3}\right)\frac{kAT}{L}"),
            R(r"H_{\text{total}} = \frac{k A T}{L}", "rb"))
    L.responde("a", r"T_u=\frac{4}{3}T", 4.0 / 3.0, "T").responde("b", r"H=\frac{kAT}{L}", 1.0, "kAT/L")
    return L.oficial("Folleto resuelto 3.3-3", 4.0 / 3.0, 1.0).usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ Q055
@leccion("Q055")
def q055() -> Leccion:
    k, lc, a, b, ta, tb = 238.0, 0.1, 0.2, 1.0, 80.0, 20.0
    lnr = math.log(b / a)
    num = math.pi * k * lc * (ta - tb)
    H = num / lnr
    L = _des("Q055", "Flujo radial en un semicilindro hueco de aluminio",
             "Calcule el flujo de calor entre las caras curvas de un medio cilindro hueco de aluminio ($k=238{,}0$ W/m·°C, espesor axial $L=0{,}1$ m, radio interior $a=0{,}2$ m a $80\\ ^\\circ$C y exterior $b=1{,}0$ m a $20\\ ^\\circ$C).",
             claves=["semicilindro", "conducción radial", "aluminio"],
             visual={"tipo": "conduccion", "geo": "cilindro", "r1": a, "r2": b, "L": lc, "T1": ta, "T2": tb, "k": k, "uT": "°C", "etq1": "a", "etq2": "b"})
    L.paso("Qué nos dan y qué piden")\
        .di("Medio tubo de aluminio: el calor va de la cara curva interna, a ochenta grados, a la externa, a veinte.", DATO("caras curvas"),
            LISTA(["$k=238{,}0$ W/(m·°C), espesor axial $L=0{,}1$ m", "Radio interno $a=0{,}2$ m a $80\\ ^\\circ$C", "Radio externo $b=1{,}0$ m a $20\\ ^\\circ$C"]))
    L.paso("Fourier con el área de medio cilindro")\
        .di("Partimos de la ley de Fourier de la hoja, en dirección radial.", V(foco="dr"), F(r"H = -kA\,\frac{dT}{dr}"))\
        .di("Es la mitad de un tubo: el área a un radio erre es la mitad de dos pi erre ele.".replace("radio erre", "radio " + dice("r", "erre")).replace("dos pi erre ele", dice("2πrL", "dos pi erre ele")),
            M(r"A(r) = \frac{1}{2}(2\pi r L) = \pi r L"), M(r"H = -k(\pi r L)\,\frac{dT}{dr}"))
    L.paso("Separar variables")\
        .di("El radio a un lado, la temperatura al otro. Paso uno: multiplicar por de erre.".replace("de erre", dice("dr", "de erre")), M(r"H\,dr=-k\,(\pi rL)\,dT"))\
        .di("Paso dos: dividir entre erre.".replace("entre erre", "entre " + dice("r", "erre")), M(r"H\,\frac{dr}{r} = -\pi kL\,dT"), NOTA("Receta en la lección **T-MAT3**."))\
        .di("Integral a los dos lados, del radio interno al externo.", V(foco="barrido"), M(r"H\int_a^b \frac{dr}{r} = -\pi k L\int_{T_a}^{T_b}dT"))
    L.paso("Resolver las integrales")\
        .di("Izquierda: regla dos, uno entre erre da logaritmo; la resta de logaritmos es el logaritmo de la división.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"\int_a^b\frac{dr}{r}=\Big[\ln r\Big]_a^b=\ln b-\ln a=\ln\frac{b}{a}"))\
        .di("Derecha: la suma de los pedacitos de temperatura; el menos voltea la resta.", M(r"-\pi kL\int_{T_a}^{T_b}dT=-\pi kL\,(T_b-T_a)=\pi kL\,(T_a-T_b)"))\
        .di("Juntamos.", M(r"H\ln\left(\frac{b}{a}\right)=\pi kL\,(T_a-T_b)"))\
        .di("Despejamos hache.".replace("hache", dice("H", "hache")), M(r"H = \frac{\pi k L\,(T_a-T_b)}{\ln(b/a)}"))
    L.paso("Sustituir los datos")\
        .di("Sustituimos.", V(foco="flujo"), M(r"H = \frac{\pi(238{,}0)(0{,}1)(80-20)}{\ln(1{,}0/0{,}2)}"))\
        .di("Arriba multiplicamos; abajo, el logaritmo de cinco.", M(rf"\pi(238{{,}}0)(0{{,}}1)(60)={tex(num, 5)}"), M(rf"\ln\left(\frac{{1{{,}}0}}{{0{{,}}2}}\right)=\ln 5={tex(lnr, 6)}"))\
        .di(f"Dividimos: {nu(H, 'W', 4)}.", M(rf"H=\frac{{{tex(num, 5)}}}{{{tex(lnr, 6)}}}"), R(rf"H = {tex(H, 4, u='W')}", "ra"))
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
    denom = 1.0 / r1 ** 2 - 1.0 / r2 ** 2
    H = 8.0 * math.pi * k0 * (t2 - t1) / denom
    tm = t1 + H * (1.0 / r1 ** 2 - 1.0 / rm ** 2) / (8.0 * math.pi * k0)
    pts = [[round(r1 + (r2 - r1) * i / 40, 4), round(t1 + (t2 - t1) * (1.0 / r1 ** 2 - 1.0 / (r1 + (r2 - r1) * i / 40) ** 2) / denom, 2)] for i in range(41)]
    L = _des(q, "Esfera hueca con conductividad k = k₀r",
             "Esfera hueca de radio interno $0{,}150$ m (a $20\\ ^\\circ\\text{C}$) y externo $0{,}600$ m (a $100\\ ^\\circ\\text{C}$). Su conductividad es $k=k_0r$ con $k_0=1{,}50\\ \\text{W/(m}^2\\cdot\\text{K)}$. (a) Flujo calórico radial. (b) Temperatura a $30$ cm del centro.",
             aviso=aviso, claves=["esfera", "k variable", "integral"],
             visual={"tipo": "conduccion", "geo": "esfera", "r1": r1, "r2": r2, "T1": t1, "T2": t2, "k": "k₀ r", "r": rm, "pts": pts, "uT": "°C"})
    L.paso("Qué nos dan y qué piden")\
        .di("Una esfera hueca: adentro está a veinte grados y afuera a cien. El calor entra, de afuera hacia el centro.", DATO("k = k0r"),
            LISTA(["$a=0{,}150$ m a $20\\ ^\\circ\\text{C}$ (adentro)", "$b=0{,}600$ m a $100\\ ^\\circ\\text{C}$ (afuera)", "$k=k_0r$ con $k_0=1{,}50$"]))\
        .di("Dos cosas cambian con el radio: el área de cada capa y la conductividad. Hay que integrar.", OJO("La fórmula de pared plana no sirve: aquí cambian $A$ y $k$."))
    L.paso("(a) Fourier en un cascarón delgadito")\
        .di("Tomamos un cascarón de radio erre y grosor de erre.".replace("radio erre", "radio " + dice("r", "erre")).replace("grosor de erre", "grosor " + dice("dr", "de erre")), V(foco="dr"), F(r"H=-kA\frac{dT}{dr}"))\
        .di("El área de una esfera es cuatro pi erre cuadrado.", M(r"A=4\pi r^2"))\
        .di("Ponemos el área y la conductividad.", M(r"H=-(k_0r)(4\pi r^2)\frac{dT}{dr}"), M(r"H=-4\pi k_0\,r^3\,\frac{dT}{dr}", "f", "azul"))
    L.paso("(a) Separar variables")\
        .di("Separar: paso uno, multiplicar los dos lados por de erre.".replace("de erre", dice("dr", "de erre")), M(r"H\,dr=-4\pi k_0\,r^3\,dT"))\
        .di("Paso dos: dividir los dos lados entre erre al cubo.".replace("erre al cubo", dice("r³", "erre al cubo")), M(r"H\,\frac{dr}{r^3}=-4\pi k_0\,dT"), NOTA("Receta en la lección **T-MAT3**."))\
        .di("Integral a los dos lados, de adentro hacia afuera.", M(r"H\int_{0{,}150}^{0{,}600}\frac{dr}{r^3}=-4\pi k_0\int_{20}^{100}dT"))
    L.paso("(a) La integral de 1/r³ (regla 1)")\
        .di("Uno entre erre al cubo es erre a la menos tres.".replace("erre al cubo", dice("r³", "erre al cubo")).replace("erre a la menos tres", dice("r⁻³", "erre a la menos tres")), M(r"\frac{1}{r^3}=r^{-3}"))\
        .di("Regla de la potencia: se suma uno al exponente, menos tres más uno es menos dos, y se divide entre menos dos.",
            M(r"\int r^{-3}\,dr=\frac{r^{-3+1}}{-3+1}=\frac{r^{-2}}{-2}"))\
        .di("Se escribe con el exponente positivo.", M(r"\frac{r^{-2}}{-2}=-\frac{1}{2r^2}"), NOTA("Regla 1 de la lección **T-MAT2**."))
    L.paso("(a) Evaluar y despejar H")\
        .di("Arriba menos abajo. Menos por menos da más.", M(r"\Big[-\frac{1}{2r^2}\Big]_{0{,}150}^{0{,}600}=\left(-\frac{1}{2(0{,}600)^2}\right)-\left(-\frac{1}{2(0{,}150)^2}\right)=\frac{1}{2}\left(\frac{1}{0{,}150^2}-\frac{1}{0{,}600^2}\right)"),
            M(rf"=\frac{{1}}{{2}}(44{{,}}44-2{{,}}78)={tex(denom / 2, 4)}\ \text{{m}}^{{-2}}"))\
        .di("Del otro lado, la suma de pedacitos de temperatura: ochenta grados.", M(r"-4\pi k_0\int_{20}^{100}dT=-4\pi k_0(100-20)=-4\pi(1{,}50)(80)"))\
        .di("Despejamos.", M(rf"H=\frac{{-4\pi(1{{,}}50)(80)}}{{{tex(denom / 2, 4)}}}"), R(rf"H={tex(-H, 3)}\ \text{{W}}\quad(|H|={tex(H, 3, u='W')})", "ra"))\
        .di("Sale negativo porque el calor va hacia el centro, contra el sentido de erre.", NOTA("El signo menos solo dice la dirección: el calor entra."))
    L.paso("(b) Temperatura a 30 cm")\
        .di("Repetimos la integral desde adentro hasta treinta centímetros.", V(foco="punto"), M(r"H\int_{0{,}150}^{0{,}300}\frac{dr}{r^3}=-4\pi k_0\int_{20}^{T}dT=-4\pi k_0\,(T-20)"))\
        .di("La integral, con la misma regla, arriba menos abajo.", M(r"\Big[-\frac{1}{2r^2}\Big]_{0{,}150}^{0{,}300}=\frac{1}{2}\left(\frac{1}{0{,}150^2}-\frac{1}{0{,}300^2}\right)=\frac{1}{2}(44{,}44-11{,}11)=16{,}67"))\
        .di("Despejamos te.".replace("te", s("T")), M(rf"T=20-\frac{{H(16{{,}}67)}}{{4\pi(1{{,}}50)}}=20-\frac{{({tex(-H, 3)})(16{{,}}67)}}{{18{{,}}85}}"))\
        .di(f"Da {nu(tm, '°C', dec=1)}.", R(rf"T(0{{,}}300\ \text{{m}})={tex(tm, dec=1)}\ ^\circ\text{{C}}={tex(tm + 273.15, dec=2)}\ \text{{K}}", "rb"))
    L.responde("a", rf"|H|={tex(H, 3)}\ \text{{W}}", H, "W").responde("b", rf"T={tex(tm, dec=1)}\ ^\circ\text{{C}}", tm, "°C")
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
    L.paso("Qué nos dan y qué piden")\
        .di("Una cuña: su altura cambia a lo largo, así que el área que cruza el calor también cambia.", DATO("conductividad térmica k0"),
            LISTA(["Conductividad $k_0$, largo $L$, profundidad $C$", "En $x=0$: altura $B$ y $T=100\\ ^\\circ$C", "En $x=L$: altura $A$ y $T=0\\ ^\\circ$C"]))
    L.paso("Área transversal en función de x")\
        .di("La altura crece en línea recta de B a A a lo largo de ele.".replace("ele", dice("L", "ele")), V(foco="dx"), M(r"y(x) = B + \frac{A-B}{L}\,x"))\
        .di("El área transversal es la profundidad por la altura.", M(r"A_{\text{trans}} = C\,y"))\
        .di("Derivamos la altura para cambiar de variable.", M(r"dy=\frac{A-B}{L}\,dx\;\Rightarrow\;dx = \frac{L}{A-B}\,dy"))
    L.paso("Fourier y separar variables")\
        .di("Partimos de la ley de Fourier de la hoja.", F(r"H = -kA\,\frac{dT}{dx}"), M(r"H = -k_0\,(C y)\,\frac{dT}{dx}"))\
        .di("Separamos: multiplicamos por de equis y dividimos entre la profundidad por la altura.".replace("de equis", dice("dx", "de equis")), M(r"H\,dx=-k_0\,(C\,y)\,dT\;\Rightarrow\;H\,\frac{dx}{C\,y}=-k_0\,dT"))\
        .di("Ponemos de equis en términos de de ye.".replace("de equis", dice("dx", "de equis")).replace("de ye", dice("dy", "de ye")), M(r"\frac{HL}{C(A-B)}\,\frac{dy}{y}=-k_0\,dT"))
    L.paso("Integrar y despejar H")\
        .di("Integramos de un extremo al otro: la altura va de B a A y la temperatura de cien a cero.", V(foco="barrido"),
            M(r"\frac{HL}{C(A-B)}\int_B^A\frac{dy}{y}=-k_0\int_{100}^{0}dT"))\
        .di("Izquierda: regla dos, uno entre ye da logaritmo; la resta de logaritmos es el logaritmo de la división.".replace("entre ye", "entre " + dice("y", "ye")),
            M(r"\int_B^A\frac{dy}{y}=\Big[\ln y\Big]_B^A=\ln A-\ln B=\ln\left(\frac{A}{B}\right)"))\
        .di("Derecha: la suma de los pedacitos de temperatura.", M(r"-k_0\int_{100}^{0}dT=-k_0(0-100)=100\,k_0"))\
        .di("Juntamos.", M(r"\frac{HL}{C(A-B)}\ln\left(\frac{A}{B}\right) = 100\,k_0"))\
        .di("Despejamos el flujo de calor.", R(r"H = \frac{100\,k_0 C\,(A-B)}{L\ln(A/B)}", "ra"))
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
    L.paso("Integrar Fourier en las dos capas cilíndricas")\
        .di("Los radios son cinco, cinco coma cinco y ocho coma cinco centímetros.", DATO("5.00 cm"),
            LISTA(["$r_1=0{,}0500$ m, $r_2=0{,}0550$ m, $r_3=0{,}0850$ m", "$k_{\\text{acero}}=50{,}2$, $k_{\\text{caucho}}=0{,}160$ W/(m·K)"]))\
        .di("Partimos de la ley de Fourier de la hoja con el área lateral del cilindro.", V(foco="dr"), F(r"H=-kA\,\frac{dT}{dr}"), M(r"H = -k\,(2\pi r L)\,\frac{dT}{dr}"))\
        .di("Separamos: multiplicamos por de erre y dividimos entre dos pi erre ele.".replace("de erre", dice("dr", "de erre")).replace("dos pi erre ele", dice("2πrL", "dos pi erre ele")),
            M(r"H\,\frac{dr}{2\pi rL}=-k\,dT"), NOTA("Receta en la lección **T-MAT3**."))
    L.paso("Integrar una capa")\
        .di("Integral a los dos lados, del radio interno al externo de la capa.", M(r"\frac{H}{2\pi L}\int_{r_{\text{int}}}^{r_{\text{ext}}}\frac{dr}{r} = -k\int_{T_{\text{int}}}^{T_{\text{ext}}} dT"))\
        .di("Izquierda: regla dos, logaritmo. Derecha: la suma de los pedacitos de temperatura.",
            M(r"\Big[\ln r\Big]_{r_{\text{int}}}^{r_{\text{ext}}}=\ln\frac{r_{\text{ext}}}{r_{\text{int}}}\qquad -k\,(T_{\text{ext}}-T_{\text{int}})=k\,(T_{\text{int}}-T_{\text{ext}})"))\
        .di("Despejamos la caída de temperatura de la capa: hache por un factor que llamamos erre.".replace("hache", dice("H", "hache")).replace("llamamos erre", "llamamos " + dice("R", "erre")),
            M(r"T_{\text{int}} - T_{\text{ext}} = H\,\frac{\ln(r_{\text{ext}}/r_{\text{int}})}{2\pi k L}=H\,R"))
    L.paso("El factor de cada capa")\
        .di("Acero y caucho.", M(rf"R_1 = \frac{{\ln(5{{,}}5/5{{,}}0)}}{{2\pi(50{{,}}2)(1{{,}}0)}} = {tex(R1, 4, u='K/W', sci='si')}"),
            M(rf"R_2 = \frac{{\ln(8{{,}}5/5{{,}}5)}}{{2\pi(0{{,}}160)(1{{,}}0)}} = {tex(R2, 4, u='K/W')}"))\
        .di("En serie el mismo hache cruza las dos capas y las caídas se suman.".replace("hache", dice("H", "hache")), M(r"T_1-T_3=H\,(R_1+R_2)"))
    L.paso("(a) y (b) Flujo y temperatura en la interfaz")\
        .di(f"El flujo total sale {nu(H, 'W', 4)}.", V(foco="perfil"),
            M(rf"H = \frac{{T_1-T_3}}{{R_1+R_2}} = \frac{{125{{,}}0-40{{,}}0}}{{{tex(R1 + R2, 4)}}}"),
            R(rf"H = {tex(H, 4, u='W')}", "rb"))\
        .di(f"Como el acero casi no resiste, en él solo caen seis centésimas de grado: {nu(t2, '°C', dec=2)}.",
            M(rf"T_2 = 125{{,}}0 - H R_1 = 125{{,}}0 - {tex(H * R1, 3)}"),
            R(rf"T_2 = {tex(t2, dec=2, u='°C')}", "ra"))
    L.paso("(c) Perfil T(r) dentro del caucho")\
        .di("Repetimos la integral dentro del caucho, desde la interfaz hasta un radio erre cualquiera.".replace("radio erre", "radio " + dice("r", "erre")), V(foco="listo"),
            M(r"\frac{H}{2\pi L}\int_{r_2}^{r}\frac{dr}{r}=-k_2\int_{T_2}^{T}dT\;\Rightarrow\;\frac{H}{2\pi L}\ln\frac{r}{r_2}=-k_2\,(T-T_2)"))\
        .di("Despejamos te de erre.".replace("te de erre", dice("T(r)", "te de erre")),
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
        .di("Partimos de la ley de Fourier de la hoja; ahora ka depende de la temperatura.".replace("ka depende", dice("k", "ka") + " depende"), V(foco="barrido"),
            F(r"H=-kA\,\frac{dT}{dx}"), M(r"H=-k(T)\,A\,\frac{dT}{dx}"))\
        .di("Separamos: multiplicamos por de equis. Ka queda del lado de la temperatura, porque depende de te.".replace("de equis", dice("dx", "de equis")).replace("Ka queda", dice("k", "Ka") + " queda").replace("de te", "de " + dice("T", "te")),
            M(r"H\,dx=-A\,k(T)\,dT"))\
        .di("Integramos: equis de cero a ele, la temperatura del extremo caliente al frío. El menos da vuelta a los límites.".replace("equis", dice("x", "equis")).replace("a ele", "a " + dice("L", "ele")),
            M(r"H\int_0^L dx=-A\int_{573}^{293}(mT+b)\,dT\;\Rightarrow\;H\,L = A\int_{293}^{573}(mT+b)\,dT"))
    L.paso("(b) La integral de una recta (regla 1)")\
        .di("Término por término con la regla de la potencia: eme te da eme te cuadrado entre dos; be da be te.".replace("eme te da eme te cuadrado entre dos", dice("mT → mT²/2", "eme te da eme te cuadrado entre dos")).replace("be da be te", dice("b → bT", "be da be te")),
            M(r"\int(mT+b)\,dT=\frac{m\,T^2}{2}+b\,T"), NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Arriba menos abajo, y sacamos la resta de temperaturas como factor común.",
            M(r"\Big[\frac{mT^2}{2}+bT\Big]_{293}^{573}=\frac{m}{2}\left(573^2-293^2\right)+b\,(573-293)=(573-293)\left[m\,\frac{573+293}{2}+b\right]"))\
        .di("El corchete es la conductividad a la temperatura del medio, que es el promedio de los dos extremos porque k es una recta.".replace("k es", dice("k", "ka") + " es"),
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
