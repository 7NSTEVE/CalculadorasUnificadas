"""Examen de Suficiencia I-2026 de Física General III (SUF26-1..7): todo el curso, nivel de reposición.

Fuente: `2026I_FG3_Examen_Suficiencia.pdf` (cátedra, «Documentos I-2026/Exámenes»). Tiene 8 problemas en
4 grupos (MAS y ondas, fluidos, propiedades térmicas, gases y termodinámica). El 8 es de entropía (sección
20.7) y queda fuera del corte ≤ 20.5. El 7 es el mismo problema del III parcial extraordinario I-2024 (Q096).

La cátedra no publicó solución de este examen: estas soluciones son propias, con cada número calculado
aquí y comprobado de dos maneras cuando se puede. Cada lección lo avisa.
"""

from __future__ import annotations

import math
from typing import Tuple

from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R
from .formato import partes, tex
from .registro import igual_que, leccion

G = 9.80
P_ATM = 1.013e5  # el valor de la hoja de esta suficiencia

_SUF = ("26-I-SUF", "Examen de suficiencia I-2026")
_PROPIA = "La cátedra no publicó la solución de esta suficiencia: la solución es propia y cada número está comprobado."


def _f(num: str, parcial: int) -> dict:
    return {"doc": _SUF[0], "nombre": _SUF[1], "num": num, "parcial": parcial}


def _q(x: float, c: int = 4) -> float:
    m, e = partes(x, c)
    return float(m) * (10.0 ** e if e is not None else 1.0)


# ============================================================================ grupo 1 · MAS y ondas

@leccion("SUF26-1")
def suf26_1() -> Leccion:
    Lc, mu, Ft = 12.0, 0.0452, 54.3
    medio = 2.0
    lam = 2.0 * medio
    n = round(2.0 * Lc / lam)
    v2 = Ft / mu
    v = math.sqrt(v2)
    vv = _q(v)
    k = 2.0 * math.pi / lam
    f = vv / lam
    w = 2.0 * math.pi * _q(f)
    L = Leccion(
        "SUF26-1", "Onda estacionaria leída de una gráfica: armónico, v, k, ω y función (Suficiencia I-2026, P1)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f("1", 1),
        enunciado=(
            "La gráfica muestra un tramo de una onda estacionaria en una cuerda de $12{,}0\\ \\text{m}$, con $\\mu=45{,}2\\ \\text{g/m}$ "
            "y tensión $54{,}3\\ \\text{N}$, en el instante de desplazamiento máximo: los nodos están en $0$, $2$, $4$, $6$ y $8\\ \\text{m}$ "
            "y los picos llegan a $1{,}0\\ \\text{mm}$. A) Número del armónico. B) Rapidez de las ondas viajeras. "
            "C) Número de onda y frecuencia angular. D) Función de onda de la onda estacionaria."
        ),
        claves=["onda estacionaria", "armónico", "número de onda", "gráfica", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("A) Leer la gráfica: la distancia entre nodos")\
        .di("Los nodos están cada dos metros. De un nodo al siguiente hay media longitud de onda, así que lambda es cuatro metros.",
            DATO("12,0 m"),
            M(r"\frac{\lambda}{2}=2{,}0\ \text{m}\;\Rightarrow\;\lambda=4{,}0\ \text{m}"))\
        .di("Con la casilla de las longitudes de onda estacionarias despejamos ene con el largo total de doce metros.",
            F(r"\lambda_n=\frac{2L}{n}"),
            M(rf"n=\frac{{2L}}{{\lambda}}=\frac{{2(12{{,}}0)}}{{4{{,}}0}}={n}"),
            R(rf"n={n}\ \text{{(sexto armónico)}}"))
    L.paso("B) Rapidez de las ondas viajeras")\
        .di("La rapidez solo depende de la tensión y de mu. Pasamos mu a kilogramos por metro.",
            F(r"v=\sqrt{\frac{F}{\mu}}"),
            M(r"\mu=45{,}2\ \text{g/m}=0{,}0452\ \text{kg/m}"),
            M(rf"v=\sqrt{{\frac{{54{{,}}3}}{{0{{,}}0452}}}}=\sqrt{{{tex(v2, 5)}}}={tex(v, 4)}\ \text{{m/s}}"),
            R(rf"v={tex(v, 4)}\ \text{{m/s}}"))
    L.paso("C) Número de onda y frecuencia angular")\
        .di("Ka sale de lambda. Para omega, primero la frecuencia con ve igual lambda por efe.",
            F(r"k=\frac{2\pi}{\lambda}"),
            M(rf"k=\frac{{2\pi}}{{4{{,}}0}}={tex(k, 4)}\ \text{{rad/m}}"),
            F(r"v=\lambda f=\frac{\omega}{k}"),
            M(rf"f=\frac{{{tex(vv, 4)}}}{{4{{,}}0}}={tex(f, 4)}\ \text{{Hz}}"))\
        .di("Omega es dos pi por efe.",
            F(r"\omega=2\pi f"),
            M(rf"\omega=2\pi({tex(_q(f), 4)})={tex(w, 4)}\ \text{{rad/s}}"),
            R(rf"k={tex(k, 4)}\ \text{{rad/m}}\qquad \omega={tex(w, 4)}\ \text{{rad/s}}"))
    L.paso("D) La función de onda")\
        .di("Usamos la forma de la hoja con la amplitud de los antinodos: un milímetro, en metros.",
            F(r"y(x,t)=A_{\text{OE}}\sen(kx)\sen(\omega t)"),
            M(r"A_{\text{OE}}=1{,}0\ \text{mm}=1{,}0\times 10^{-3}\ \text{m}"),
            R(rf"y(x,t)=(1{{,}}0\times 10^{{-3}}\ \text{{m}})\sen({tex(k, 4)}\,x)\sen({tex(w, 4)}\,t)"),
            NOTA("La gráfica es el instante en que sen(ωt) = 1. Si se toma ese instante como t = 0, se escribe cos(ωt) en vez de sen(ωt)."))
    L.usa(r"\lambda_n=\frac{2L}{n}", r"v=\sqrt{\frac{F}{\mu}}", r"k=\frac{2\pi}{\lambda}", r"\omega=2\pi f")
    L.responde("A", rf"n={n}", float(n), "")
    L.responde("B", rf"v={tex(v, 4)}\ \text{{m/s}}", v, "m/s")
    L.responde("C-k", rf"k={tex(k, 4)}\ \text{{rad/m}}", k, "rad/m")
    L.responde("C-w", rf"\omega={tex(w, 4)}\ \text{{rad/s}}", w, "rad/s")
    L.te_toca("¿Cuántos nodos tiene toda la cuerda de 12,0 m en este armónico?", float(n + 1), "", tol=0.01,
              pista="En el armónico n hay n + 1 nodos.", solucion="6 + 1 = 7 nodos.")
    return L


@leccion("SUF26-2")
def suf26_2() -> Leccion:
    mu, Lc, m1, m2, A = 0.00238, 2.20, 16.0, 25.0, 0.0200
    T1, T2 = m1 * G, m2 * G
    v1 = math.sqrt(T1 / mu)
    v2 = math.sqrt(T2 / mu)
    v1v, v2v = _q(v1), _q(v2)
    n1 = 5
    f = n1 * v1v / (2.0 * Lc)
    m_max = m1 * n1 ** 2
    lam1 = 2.0 * Lc
    k1 = 2.0 * math.pi / lam1
    w = 2.0 * math.pi * _q(f)
    L = Leccion(
        "SUF26-2", "Cuerda con un vibrador y masas colgadas: frecuencia y masa máxima (Suficiencia I-2026, P2)",
        tema="ondas", tipo="desarrollo", nivel=3, parcial=1, fuente=_f("2", 1),
        enunciado=(
            "Una cuerda ($\\mu=0{,}00238\\ \\text{kg/m}$) pasa por una polea con un bloque colgado; entre la polea y el vibrador hay "
            "$2{,}20\\ \\text{m}$. Con bloques de $16{,}0\\ \\text{kg}$ y de $25{,}0\\ \\text{kg}$ hay ondas estacionarias, pero no con masas "
            "intermedias. A) Rapidez de las ondas con cada tensión. B) Frecuencia del vibrador (más tensión, menos nodos). "
            "C) La mayor masa con la que habría ondas estacionarias. D) Función de onda en el primer armónico con desplazamiento máximo de $2{,}00\\ \\text{cm}$."
        ),
        claves=["onda estacionaria", "tensión", "vibrador", "armónicos consecutivos", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("A) Tensión y rapidez con cada bloque")\
        .di("La tensión es el peso del bloque. Después la casilla de rapidez en una cuerda.",
            DATO("16,0 kg"),
            F(r"v=\sqrt{\frac{F}{\mu}}"),
            M(rf"F_1=(16{{,}}0)(9{{,}}80)={tex(T1, 4)}\ \text{{N}}\qquad F_2=(25{{,}}0)(9{{,}}80)={tex(T2, 3)}\ \text{{N}}"),
            M(rf"v_1=\sqrt{{\frac{{{tex(T1, 4)}}}{{0{{,}}00238}}}}={tex(v1, 5)}\ \text{{m/s}}"),
            M(rf"v_2=\sqrt{{\frac{{{tex(T2, 3)}}}{{0{{,}}00238}}}}={tex(v2, 5)}\ \text{{m/s}}"))
    L.paso("B) Dos armónicos seguidos con la misma frecuencia")\
        .di("El vibrador no cambia su frecuencia. Con más tensión hay menos nodos: si con dieciséis kilos es el armónico ene, con veinticinco es ene menos uno.",
            F(r"f_n=\frac{v}{\lambda_n}"),
            F(r"\lambda_n=\frac{2L}{n}"),
            M(r"f=\frac{n\,v_1}{2L}=\frac{(n-1)\,v_2}{2L}\;\Rightarrow\;n\,v_1=(n-1)\,v_2"))\
        .di("Como la rapidez va con la raíz de la masa, ve dos entre ve uno es raíz de veinticinco entre dieciséis: cinco cuartos.",
            M(r"\frac{v_2}{v_1}=\sqrt{\frac{25{,}0}{16{,}0}}=\frac{5}{4}"),
            M(r"n=\frac{5}{4}(n-1)\;\Rightarrow\;4n=5n-5\;\Rightarrow\;n=5"))\
        .di("Con dieciséis kilos es el quinto armónico y con veinticinco, el cuarto. Calculamos efe.",
            M(rf"f=\frac{{5({tex(v1v, 5)})}}{{2(2{{,}}20)}}={tex(f, 5)}\ \text{{Hz}}"),
            R(rf"f={tex(f, 5)}\ \text{{Hz}}"))
    L.paso("C) La mayor masa posible")\
        .di("La mayor tensión da el menor número de nodos: el primer armónico.",
            M(r"n=1"))\
        .di("Como ene por ve es constante y ve va con la raíz de la masa, la masa va con uno entre ene al cuadrado.",
            M(r"n\sqrt{m}=\text{constante}\;\Rightarrow\;m_1=m_5\left(\frac{5}{1}\right)^2"),
            M(rf"m_{{\text{{máx}}}}=(16{{,}}0)(25)={tex(m_max, 3)}\ \text{{kg}}"),
            R(rf"m_{{\text{{máx}}}}={tex(m_max, 3)}\ \text{{kg}}"))
    L.paso("D) Función de onda en el primer armónico")\
        .di("En el primer armónico lambda es dos veces el largo. La frecuencia es la del vibrador.",
            F(r"k=\frac{2\pi}{\lambda}"),
            M(rf"\lambda_1=2(2{{,}}20)={tex(lam1, 3)}\ \text{{m}}\;\Rightarrow\;k=\frac{{2\pi}}{{{tex(lam1, 3)}}}={tex(k1, 4)}\ \text{{rad/m}}"),
            F(r"\omega=2\pi f"),
            M(rf"\omega=2\pi({tex(_q(f), 5)})={tex(w, 4)}\ \text{{rad/s}}"))\
        .di("Con la forma de la hoja y dos centímetros de amplitud en el antinodo, pasados a metros.",
            F(r"y(x,t)=A_{\text{OE}}\sen(kx)\sen(\omega t)"),
            M(r"A_{\text{OE}}=2{,}00\ \text{cm}=0{,}0200\ \text{m}"),
            M(rf"y(x,t)=(0{{,}}0200)\sen({tex(k1, 4)}\,x)\sen({tex(w, 4)}\,t)"),
            R(rf"y(x,t)=(0{{,}}0200\ \text{{m}})\sen({tex(k1, 4)}\,x)\sen({tex(w, 4)}\,t)"))
    L.usa(r"v=\sqrt{\frac{F}{\mu}}", r"f_n=\frac{v}{\lambda_n}", r"\lambda_n=\frac{2L}{n}", r"k=\frac{2\pi}{\lambda}", r"\omega=2\pi f")
    L.responde("A-v1", rf"v_1={tex(v1, 5)}\ \text{{m/s}}", v1, "m/s")
    L.responde("A-v2", rf"v_2={tex(v2, 5)}\ \text{{m/s}}", v2, "m/s")
    L.responde("B", rf"f={tex(f, 5)}\ \text{{Hz}}", f, "Hz")
    L.responde("C", rf"m_{{\text{{máx}}}}={tex(m_max, 3)}\ \text{{kg}}", m_max, "kg")
    L.te_toca("¿Con qué masa se vería el tercer armónico?", m1 * (5 / 3) ** 2, "kg", tol=0.01,
              pista="m va con 1/n²: m₃ = 16,0 · (5/3)².", solucion="16,0 × 25/9 ≈ 44,4 kg.")
    return L


# ============================================================================ grupo 2 · fluidos

@leccion("SUF26-3")
def suf26_3() -> Leccion:
    Ab, Vw, Vo = 3.46, 12.87, 8.58
    rw, ro = 1020.0, 768.0
    b, alto = 0.100, 0.400
    hw = Vw / Ab
    ho = Vo / Ab
    hwv, hov = _q(hw), _q(ho)
    po = ro * G * hov
    p_int = P_ATM + po
    rwg = rw * G
    pw = rwg * hwv
    p_fondo = po + pw
    term1 = p_fondo * alto
    term2 = rwg * alto ** 2 / 2.0
    integral = term1 - term2
    Fg = b * integral
    y_c = alto / 2.0
    p_c = p_fondo - rwg * y_c
    v = math.sqrt(2.0 * p_c / rw)

    L = Leccion(
        "SUF26-3", "Tanque con agua de mar y aceite: compuerta y chorro por un orificio (Suficiencia I-2026, P3)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f("3", 2),
        enunciado=(
            "Un depósito cilíndrico abierto de $3{,}46\\ \\text{m}^2$ de base tiene $12{,}87\\ \\text{m}^3$ de agua de mar "
            "($1{,}02\\ \\text{g/cm}^3$) y encima $8{,}58\\ \\text{m}^3$ de aceite ($0{,}768\\ \\text{g/cm}^3$). Abajo, en la pared, hay una compuerta "
            "rectangular de $10{,}00\\ \\text{cm}$ de ancho y $40{,}0\\ \\text{cm}$ de alto con un orificio de $1{,}40\\ \\text{cm}^2$ en su centro, cerrado. "
            "A) Altura de cada capa. B) Presión absoluta en la interfaz. C) Fuerza hidrostática neta sobre la compuerta. "
            "D) Rapidez de salida del agua al abrir el orificio."
        ),
        claves=["dos líquidos", "presión hidrostática", "compuerta", "integral", "Torricelli", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("A) Altura de cada capa")\
        .di("El depósito es un cilindro: volumen es área de la base por altura. Despejamos la altura de cada líquido.",
            DATO("3,46 m"),
            M(rf"h_{{\text{{agua}}}}=\frac{{12{{,}}87}}{{3{{,}}46}}={tex(hw, 4)}\ \text{{m}}"),
            M(rf"h_{{\text{{aceite}}}}=\frac{{8{{,}}58}}{{3{{,}}46}}={tex(ho, 4)}\ \text{{m}}"),
            R(rf"h_{{\text{{agua}}}}={tex(hw, 3)}\ \text{{m}}\qquad h_{{\text{{aceite}}}}={tex(ho, 3)}\ \text{{m}}"))
    L.paso("B) Presión en la interfaz")\
        .di("Arriba del aceite está el aire. Bajamos por todo el aceite hasta tocar el agua. Pasamos las densidades a kilogramos por metro cúbico.",
            F(r"p=p_0+\rho g h"),
            M(r"\rho_{\text{aceite}}=0{,}768\ \text{g/cm}^3=768\ \text{kg/m}^3"),
            M(rf"\rho_{{\text{{aceite}}}}\,g\,h_{{\text{{aceite}}}}=(768)(9{{,}}80)({tex(hov, 4)})={tex(po, 5)}\ \text{{Pa}}"),
            M(rf"p_{{\text{{interfaz}}}}=1{{,}}013\times 10^5+{tex(po, 5)}={tex(p_int, 5)}\ \text{{Pa}}"),
            R(rf"p_{{\text{{interfaz}}}}={tex(p_int, 4)}\ \text{{Pa}}"))
    L.paso("C) Presión manométrica en el fondo")\
        .di("Afuera de la compuerta hay aire, así que la presión atmosférica se cancela: usamos presiones manométricas. En el fondo se suman las dos capas.",
            F(r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}"),
            M(rf"\rho_{{\text{{agua}}}}\,g=(1020)(9{{,}}80)={tex(rwg, 4)}\ \text{{N/m}}^3"),
            M(rf"\rho_{{\text{{agua}}}}\,g\,h_{{\text{{agua}}}}=({tex(rwg, 4)})({tex(hwv, 4)})={tex(pw, 5)}\ \text{{Pa}}"),
            M(rf"p_{{\text{{fondo}}}}={tex(po, 5)}+{tex(pw, 5)}={tex(p_fondo, 5)}\ \text{{Pa}}"))
    L.paso("C) Una franja de la compuerta")\
        .di("Medimos ye hacia arriba desde el fondo. Subir dentro del agua resta ro ge ye. La franja tiene diez centímetros de ancho.",
            F(r"p=\frac{dF_\perp}{dA}"),
            M(rf"p(y)={tex(p_fondo, 5)}-{tex(rwg, 4)}\,y"),
            M(r"dF=p(y)\,(0{,}100)\,dy"))
    L.paso("C) Sumar las franjas de 0 a 0,40 m")\
        .di("Un número solo da número por ye. Ye da ye al cuadrado entre dos. Arriba menos abajo, y en cero todo vale cero.",
            F(r"F_\perp=\int p\,dA"),
            M(rf"F=0{{,}}100\int_0^{{0{{,}}400}}\left({tex(p_fondo, 5)}-{tex(rwg, 4)}\,y\right)dy"),
            M(rf"\int_0^{{0{{,}}400}}(\dots)\,dy=({tex(p_fondo, 5)})(0{{,}}400)-{tex(rwg, 4)}\,\frac{{(0{{,}}400)^2}}{{2}}"),
            M(rf"({tex(p_fondo, 5)})(0{{,}}400)-({tex(rwg, 4)})(0{{,}}0800)={tex(term1, 5)}-{tex(term2, 4)}={tex(integral, 5)}"),
            M(rf"F=(0{{,}}100)({tex(integral, 5)})={tex(Fg, 4)}\ \text{{N}}"),
            R(rf"F={tex(Fg, 4)}\ \text{{N}}"))\
        .di("Comprobación: la presión en el centro de la compuerta por su área da lo mismo.",
            M(rf"p_c\,A=({tex(p_c, 5)})(0{{,}}100)(0{{,}}400)={tex(p_c * b * alto, 4)}\ \text{{N}}"))
    L.paso("D) Rapidez de salida por el orificio")\
        .di("Bernoulli se usa dentro de un solo líquido. Dentro del agua, de la superficie al orificio: arriba quieto y abajo sale al aire.",
            F(r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"),
            M(r"\frac{1}{2}\rho_{\text{agua}}v^2=p_{c,\text{man}}\;\Rightarrow\;v=\sqrt{\frac{2\,p_{c,\text{man}}}{\rho_{\text{agua}}}}"),
            NOTA("p_c,man es la presión manométrica en el centro de la compuerta: el aceite y el agua que hay encima del orificio."))\
        .di("Sustituimos la presión del centro que ya calculamos.",
            M(rf"v=\sqrt{{\frac{{2({tex(p_c, 5)})}}{{1020}}}}={tex(v, 4)}\ \text{{m/s}}"),
            R(rf"v={tex(v, 4)}\ \text{{m/s}}"))
    L.usa(r"p=p_0+\rho g h", r"p_{\text{man}}=p_{\text{abs}}-p_{\text{atm}}", r"p=\frac{dF_\perp}{dA}", r"F_\perp=\int p\,dA",
          r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2")
    L.responde("A-agua", rf"h={tex(hw, 3)}\ \text{{m}}", hw, "m")
    L.responde("A-aceite", rf"h={tex(ho, 3)}\ \text{{m}}", ho, "m")
    L.responde("B", rf"p={tex(p_int, 4)}\ \text{{Pa}}", p_int, "Pa")
    L.responde("C", rf"F={tex(Fg, 4)}\ \text{{N}}", Fg, "N")
    L.responde("D", rf"v={tex(v, 4)}\ \text{{m/s}}", v, "m/s")
    return L


@leccion("SUF26-4")
def suf26_4() -> Leccion:
    rho, d1, v1, dy, dp = 1180.0, 0.0400, 2.52, 2.50, 10133.0
    A1 = math.pi * (d1 / 2.0) ** 2
    Q = A1 * v1
    mdot = rho * Q
    t_v1 = v1 ** 2
    t_g = 2.0 * G * dy
    t_p = 2.0 * dp / rho
    v2sq = t_v1 + t_g - t_p
    v2 = math.sqrt(v2sq)
    A2 = _q(Q) / _q(v2)
    d2 = math.sqrt(4.0 * A2 / math.pi)
    L = Leccion(
        "SUF26-4", "Tubería que baja 2,50 m y cambia de diámetro (Suficiencia I-2026, P4)",
        tema="fluidos", tipo="desarrollo", nivel=3, parcial=2, fuente=_f("4", 2),
        enunciado=(
            "Un fluido incompresible ($1180\\ \\text{kg/m}^3$) va por una tubería horizontal de $4{,}00\\ \\text{cm}$ de diámetro a "
            "$2{,}52\\ \\text{m/s}$. Más adelante la tubería baja $2{,}50\\ \\text{m}$ y cambia de diámetro; ahí la presión absoluta es "
            "$10\\,133\\ \\text{Pa}$ mayor. A) Flujo másico y caudal. B) Rapidez en el nivel bajo. C) Diámetro en el nivel bajo."
        ),
        claves=["Bernoulli", "continuidad", "caudal", "flujo másico", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("A) Caudal y flujo másico")\
        .di("El área es pi por el radio al cuadrado; el radio es dos centímetros. El caudal es área por rapidez.",
            DATO("4,00 cm"),
            F(r"\frac{dV}{dt}=Av"),
            M(rf"A_1=\pi(0{{,}}0200)^2={tex(A1, 4)}\ \text{{m}}^2"),
            M(rf"\frac{{dV}}{{dt}}=({tex(A1, 4)})(2{{,}}52)={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}"))\
        .di("El flujo másico es la densidad por el caudal.",
            F(r"\frac{dm}{dt}=\rho\frac{dV}{dt}"),
            M(rf"\frac{{dm}}{{dt}}=(1180)({tex(Q, 4)})={tex(mdot, 4)}\ \text{{kg/s}}"),
            R(rf"\frac{{dV}}{{dt}}={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}\qquad \frac{{dm}}{{dt}}={tex(mdot, 4)}\ \text{{kg/s}}"))
    L.paso("B) Bernoulli entre el nivel alto y el bajo")\
        .di("Tomamos el nivel bajo como altura cero: el alto está a dos coma cincuenta metros. Despejamos ve dos al cuadrado.",
            F(r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2"),
            M(r"\frac{1}{2}\rho v_2^2=\frac{1}{2}\rho v_1^2+\rho g(y_1-y_2)-(p_2-p_1)"),
            M(r"v_2^2=v_1^2+2g(y_1-y_2)-\frac{2(p_2-p_1)}{\rho}"))\
        .di("Calculamos cada término por separado y después sumamos y restamos.",
            M(rf"v_1^2=(2{{,}}52)^2={tex(t_v1, 5)}\ \text{{m}}^2/\text{{s}}^2"),
            M(rf"2g(y_1-y_2)=2(9{{,}}80)(2{{,}}50)={tex(t_g, 3)}\ \text{{m}}^2/\text{{s}}^2"),
            M(rf"\frac{{2(10\,133)}}{{1180}}={tex(t_p, 5)}\ \text{{m}}^2/\text{{s}}^2"),
            M(rf"v_2^2={tex(t_v1, 5)}+{tex(t_g, 3)}-{tex(t_p, 5)}={tex(v2sq, 5)}\ \text{{m}}^2/\text{{s}}^2"),
            M(rf"v_2=\sqrt{{{tex(v2sq, 5)}}}={tex(v2, 4)}\ \text{{m/s}}"),
            R(rf"v_2={tex(v2, 4)}\ \text{{m/s}}"))
    L.paso("C) Diámetro en el nivel bajo")\
        .di("El caudal es el mismo. Despejamos el área nueva y de ahí el diámetro.",
            F(r"A_1 v_1=A_2 v_2"),
            M(rf"A_2=\frac{{{tex(Q, 4)}}}{{{tex(v2, 4)}}}={tex(A2, 4)}\ \text{{m}}^2"),
            M(rf"d_2=\sqrt{{\frac{{4A_2}}{{\pi}}}}=\sqrt{{\frac{{4({tex(A2, 4)})}}{{\pi}}}}={tex(d2, 4)}\ \text{{m}}"),
            R(rf"d_2={tex(d2 * 100.0, 3)}\ \text{{cm}}"))
    L.usa(r"\frac{dV}{dt}=Av", r"\frac{dm}{dt}=\rho\frac{dV}{dt}",
          r"p_1+\rho g y_1+\frac{1}{2}\rho v_1^2=p_2+\rho g y_2+\frac{1}{2}\rho v_2^2", r"A_1 v_1=A_2 v_2")
    L.responde("A-Q", rf"Q={tex(Q, 4)}\ \text{{m}}^3/\text{{s}}", Q, "m^3/s")
    L.responde("A-m", rf"\dot m={tex(mdot, 4)}\ \text{{kg/s}}", mdot, "kg/s")
    L.responde("B", rf"v_2={tex(v2, 4)}\ \text{{m/s}}", v2, "m/s")
    L.responde("C", rf"d_2={tex(d2 * 100.0, 3)}\ \text{{cm}}", d2 * 100.0, "cm")
    return L


# ============================================================================ grupo 3 · propiedades térmicas

@leccion("SUF26-5")
def suf26_5() -> Leccion:
    D_al, D_h, a_al, a_ac, T0 = 2.540, 2.537, 2.40e-5, 1.20e-5, 25.0
    dD = D_h - D_al                       # −0,003 cm
    den_a = a_al * D_al                    # 6,096e-5
    dT_a = dD / den_a                      # −49,21
    den_b = a_ac * D_h                     # 3,0444e-5
    dT_b = (D_al - D_h) / den_b            # +98,54
    den_c = den_b - den_a                  # −3,0516e-5
    dT_c = (D_al - D_h) / den_c            # −98,31
    L = Leccion(
        "SUF26-5", "Esfera de aluminio por un agujero de acero: tres maneras de que pase (Suficiencia I-2026, P5)",
        tema="dilatacion", tipo="desarrollo", nivel=3, parcial=3, fuente=_f("5", 3),
        enunciado=(
            "Una esfera de aluminio de $2{,}540\\ \\text{cm}$ debe pasar por un agujero de $2{,}537\\ \\text{cm}$ en una lámina de acero. "
            "$\\alpha_{\\text{Al}}=2{,}40\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$ y $\\alpha_{\\text{acero}}=1{,}20\\times 10^{-5}\\ ^\\circ\\text{C}^{-1}$; "
            "ambas a $25{,}0\\ ^\\circ\\text{C}$. A) Temperatura de la esfera para que pase. B) Temperatura de la lámina. "
            "C) Temperatura del conjunto esfera-lámina."
        ),
        claves=["dilatación lineal", "agujero", "esfera", "temperatura", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("La casilla y una idea clave")\
        .di("El diámetro de la esfera y el del agujero cambian como cualquier largo. El agujero se agranda al calentar la lámina, como si fuera de acero.",
            DATO("2,537 cm"),
            F(r"\Delta L=\alpha L_0\Delta T"),
            M(r"D=D_0\,(1+\alpha\,\Delta T)"))
    L.paso("A) Enfriar solo la esfera")\
        .di("La esfera tiene que encoger tres milésimas de centímetro, hasta medir lo mismo que el agujero.",
            M(rf"\Delta D=2{{,}}537-2{{,}}540={tex(dD, 2)}\ \text{{cm}}"),
            M(rf"\Delta T=\frac{{\Delta D}}{{\alpha_{{\text{{Al}}}}D_0}}=\frac{{{tex(dD, 2)}}}{{(2{{,}}40\times 10^{{-5}})(2{{,}}540)}}={tex(dT_a, 4)}\ ^\circ\text{{C}}"),
            M(rf"T=25{{,}}0+({tex(dT_a, 4)})={tex(T0 + dT_a, 3)}\ ^\circ\text{{C}}"),
            R(rf"T_{{\text{{esfera}}}}={tex(T0 + dT_a, 3)}\ ^\circ\text{{C}}"))
    L.paso("B) Calentar solo la lámina")\
        .di("Ahora el agujero tiene que crecer tres milésimas, con el coeficiente del acero y su diámetro inicial.",
            M(rf"\Delta T=\frac{{2{{,}}540-2{{,}}537}}{{(1{{,}}20\times 10^{{-5}})(2{{,}}537)}}={tex(dT_b, 4)}\ ^\circ\text{{C}}"),
            M(rf"T=25{{,}}0+{tex(dT_b, 4)}={tex(T0 + dT_b, 4)}\ ^\circ\text{{C}}"),
            R(rf"T_{{\text{{lámina}}}}={tex(T0 + dT_b, 4)}\ ^\circ\text{{C}}"))
    L.paso("C) Las dos a la misma temperatura")\
        .di("Si las dos cambian igual de temperatura, pedimos que al final midan lo mismo. Como el aluminio cambia el doble, hay que enfriar.",
            M(r"2{,}540\,(1+\alpha_{\text{Al}}\Delta T)=2{,}537\,(1+\alpha_{\text{ac}}\Delta T)"),
            M(r"2{,}540-2{,}537=\Delta T\left(2{,}537\,\alpha_{\text{ac}}-2{,}540\,\alpha_{\text{Al}}\right)"))\
        .di("Calculamos el paréntesis y despejamos delta te.",
            M(rf"2{{,}}537(1{{,}}20\times 10^{{-5}})-2{{,}}540(2{{,}}40\times 10^{{-5}})={tex(den_c, 5)}"),
            M(rf"\Delta T=\frac{{0{{,}}003}}{{{tex(den_c, 5)}}}={tex(dT_c, 4)}\ ^\circ\text{{C}}"),
            M(rf"T=25{{,}}0+({tex(dT_c, 4)})={tex(T0 + dT_c, 3)}\ ^\circ\text{{C}}"),
            R(rf"T_{{\text{{conjunto}}}}={tex(T0 + dT_c, 3)}\ ^\circ\text{{C}}"))
    L.usa(r"\Delta L=\alpha L_0\Delta T")
    L.responde("A", rf"T={tex(T0 + dT_a, 3)}", T0 + dT_a, "°C")
    L.responde("B", rf"T={tex(T0 + dT_b, 4)}", T0 + dT_b, "°C")
    L.responde("C", rf"T={tex(T0 + dT_c, 3)}", T0 + dT_c, "°C")
    return L


@leccion("SUF26-6")
def suf26_6() -> Leccion:
    m_h, m_v, ch, cw, Lf, Lv = 0.9500, 0.0500, 2100.0, 4190.0, 3.33e5, 2.26e6
    Q_cond = m_v * Lv
    Q_enf = m_v * cw * 100.0
    Q_disp = Q_cond + Q_enf
    Q_cal = m_h * ch * 40.0
    Q_fun = m_h * Lf
    Q_nec = Q_cal + Q_fun
    sobra = Q_disp - Q_cal
    m_fund = sobra / Lf
    m_queda = m_h - m_fund
    Q_agua50 = m_h * cw * 50.0
    Q_abs = Q_cal + Q_fun + Q_agua50
    por_kg = Lv + cw * 50.0
    m_tot = Q_abs / por_kg
    m_extra = m_tot - m_v
    L = Leccion(
        "SUF26-6", "Hielo a −40 °C y vapor a 100 °C en un calorímetro (Suficiencia I-2026, P6)",
        tema="calorimetria", tipo="desarrollo", nivel=3, parcial=3, fuente=_f("6", 3),
        enunciado=(
            "En un calorímetro ideal se ponen $950{,}0\\ \\text{g}$ de hielo a $-40{,}00\\ ^\\circ\\text{C}$ y $50{,}0\\ \\text{g}$ de vapor a $100{,}00\\ ^\\circ\\text{C}$ "
            "($c_{\\text{hielo}}=2100$, $c_{\\text{agua}}=4190\\ \\text{J/(kg·K)}$, $L_f=3{,}33\\times 10^5$, $L_v=2{,}26\\times 10^6\\ \\text{J/kg}$). "
            "A) Temperatura final, justificando si se funde todo el hielo. B) Masa de hielo sin fundir (o masa total de agua). "
            "C) Masa adicional de vapor para que la temperatura final sea $50{,}00\\ ^\\circ\\text{C}$."
        ),
        claves=["calorimetría", "cambio de fase", "fusión parcial", "vapor", "suficiencia"],
        aviso=_PROPIA,
    )
    L.paso("A) Cuánto calor puede dar el vapor hasta 0 °C")\
        .di("No se despeja la temperatura de una vez: primero se compara. El vapor se condensa y el agua que forma se enfría hasta cero grados.",
            DATO("950,0 g"),
            F(r"Q=\pm mL"),
            F(r"Q=mc\Delta T"),
            M(rf"Q_{{\text{{cond}}}}=(0{{,}}0500)(2{{,}}26\times 10^6)={tex(Q_cond, 5)}\ \text{{J}}"),
            M(rf"Q_{{\text{{enfr}}}}=(0{{,}}0500)(4190)(100)={tex(Q_enf, 5)}\ \text{{J}}"),
            M(rf"Q_{{\text{{disp}}}}={tex(Q_cond, 5)}+{tex(Q_enf, 5)}={tex(Q_disp, 6)}\ \text{{J}}"))
    L.paso("A) Cuánto calor necesita el hielo")\
        .di("El hielo primero sube de menos cuarenta a cero, y después se funde. Comparamos con lo disponible.",
            M(rf"Q_{{\text{{calentar}}}}=(0{{,}}9500)(2100)(40)={tex(Q_cal, 5)}\ \text{{J}}"),
            M(rf"Q_{{\text{{fundir}}}}=(0{{,}}9500)(3{{,}}33\times 10^5)={tex(Q_fun, 6)}\ \text{{J}}"),
            M(rf"Q_{{\text{{nec}}}}={tex(Q_cal, 5)}+{tex(Q_fun, 6)}={tex(Q_nec, 6)}\ \text{{J}}"))\
        .di("Lo disponible no alcanza para fundir todo el hielo. Entonces queda hielo y la mezcla termina a cero grados.",
            M(rf"{tex(Q_disp, 6)}\ \text{{J}}<{tex(Q_nec, 6)}\ \text{{J}}"),
            R(r"T_f=0\ ^\circ\text{C}\ \text{(no se funde todo)}"))
    L.paso("B) Cuánto hielo queda")\
        .di("Del calor disponible, una parte calienta el hielo hasta cero. Lo que sobra funde una parte.",
            M(rf"Q_{{\text{{sobra}}}}={tex(Q_disp, 6)}-{tex(Q_cal, 5)}={tex(sobra, 5)}\ \text{{J}}"),
            M(rf"m_{{\text{{fundida}}}}=\frac{{{tex(sobra, 5)}}}{{3{{,}}33\times 10^5}}={tex(m_fund, 4)}\ \text{{kg}}"),
            M(rf"m_{{\text{{hielo}}}}=0{{,}}9500-{tex(m_fund, 4)}={tex(m_queda, 4)}\ \text{{kg}}"),
            R(rf"m_{{\text{{hielo}}}}={tex(m_queda, 3)}\ \text{{kg}}"))
    L.paso("C) Lo que absorbe el hielo hasta 50 °C")\
        .di("Ahora el hielo sube a cero, se funde todo, y esa agua sube hasta cincuenta grados.",
            M(rf"Q_{{\text{{agua}}}}=(0{{,}}9500)(4190)(50)={tex(Q_agua50, 6)}\ \text{{J}}"),
            M(rf"Q_{{\text{{abs}}}}={tex(Q_cal, 5)}+{tex(Q_fun, 6)}+{tex(Q_agua50, 6)}={tex(Q_abs, 6)}\ \text{{J}}"))
    L.paso("C) Lo que da cada kilogramo de vapor y la masa extra")\
        .di("Cada kilogramo de vapor se condensa y después el agua baja de cien a cincuenta grados.",
            M(rf"\text{{por kg}}=2{{,}}26\times 10^6+(4190)(50)={tex(por_kg, 6)}\ \text{{J/kg}}"),
            M(rf"m_{{\text{{vapor}}}}=\frac{{{tex(Q_abs, 6)}}}{{{tex(por_kg, 6)}}}={tex(m_tot, 4)}\ \text{{kg}}"))\
        .di("Ya había cincuenta gramos de vapor. La diferencia es lo que hay que agregar.",
            M(rf"m_{{\text{{extra}}}}={tex(m_tot, 4)}-0{{,}}0500={tex(m_extra, 4)}\ \text{{kg}}"),
            R(rf"m_{{\text{{extra}}}}={tex(m_extra, 3)}\ \text{{kg}}"))
    L.usa(r"Q=\pm mL", r"Q=mc\Delta T")
    L.responde("A", r"T_f=0\ ^\circ\text{C}", 0.0, "°C")
    L.responde("B", rf"m={tex(m_queda, 3)}\ \text{{kg}}", m_queda, "kg")
    L.responde("C", rf"m={tex(m_extra, 3)}\ \text{{kg}}", m_extra, "kg")
    return L


# ============================================================================ grupo 4 · termodinámica

# El problema 7 es el mismo del III parcial extraordinario I-2024 (Q096), ya resuelto en el banco.
igual_que(
    "SUF26-7", "Q096",
    fuente=_f("7", 3),
    titulo="Ciclo de cuatro procesos con calores dados y γ (Suficiencia I-2026, P7 = Q096)",
    aviso=("Es el mismo problema del III parcial extraordinario I-2024 (Q096). Los datos de la figura no son del todo "
           "coherentes: la adiabática con 0,09 m³ da γ ≈ 1,376, mientras que el camino de la energía da γ ≈ 1,165."),
)
