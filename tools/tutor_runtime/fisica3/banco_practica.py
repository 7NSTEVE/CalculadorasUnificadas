"""Prácticas oficiales del III parcial II-2026.

PR1-*: «Práctica del Parcial 3_IIS2026.pdf» (6 problemas de desarrollo).
PR2-S*: «Práctica tercer parcial FIII.pdf», selección única (14 ítems).
PR2-D*: misma práctica, desarrollo y desafíos (12 problemas).

No traen solución publicada: cada número se calcula aquí y lo revisa el verificador aritmético.
Todo parte de las casillas del formulario II-2026; lo que no está en la hoja se deriva a la vista.
"""

from __future__ import annotations

import math

from .ayudas import Gas, cierre_ciclo, estado_T, explica_proceso, receta_cono, revisar_opciones, valores_visual, visual_pv
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, TABLA, V
from .formato import dice, n, nu, tex
from .registro import leccion

P1 = "26-II-P1"
P2 = "26-II-P2"
NOM1 = "Práctica del Parcial 3 (II-2026)"
NOM2 = "Práctica del tercer examen (II-2026)"
C = r"^\circ\text{C}"


def _f1(num: int, pag: int) -> dict:
    return {"doc": P1, "nombre": NOM1, "num": str(num), "pag": pag, "texto": f"{NOM1} · problema {num}"}


def _f2(clave: str, pag: int) -> dict:
    tipo = "selección" if clave.startswith("S") else "desarrollo"
    return {"doc": P2, "nombre": NOM2, "num": clave, "pag": pag, "texto": f"{NOM2} · {tipo} {clave[1:]}"}


def _des(id: str, titulo: str, tema: str, fuente: dict, enunciado: str, nivel: int = 2, **k) -> Leccion:
    return Leccion(id, titulo, tema=tema, nivel=nivel, fuente=fuente, enunciado=enunciado, **k)


def _sel(id: str, titulo: str, tema: str, fuente: dict, enunciado: str, opciones: dict, correcta: str, **k) -> Leccion:
    return Leccion(id, titulo, tema=tema, nivel=1, tipo="seleccion", fuente=fuente, enunciado=enunciado,
                   opciones=opciones, correcta=correcta, **k)


def _j(x: float, c: int = 5) -> str:
    """Joules sin notación científica cuando caben."""
    return tex(x, c, sci="no")


# ============================================================================ PR1-1
@leccion("PR1-1")
def pr1_1() -> Leccion:
    comp = [("Al", "aluminio", 0.150, 900.0, 20.0), ("agua", "agua", 0.800, 4186.0, 20.0),
            ("Fe", "hierro", 0.600, 450.0, 450.0), ("Cu", "cobre", 0.400, 390.0, 5.0)]
    mc = [m * c for _, _, m, c, _ in comp]
    smc = sum(mc)
    smct = sum(x * t for x, (_, _, _, _, t) in zip(mc, comp))
    tf = smct / smc
    qs = [x * (tf - t) for x, (_, _, _, _, t) in zip(mc, comp)]
    gana = (mc[0] + mc[1]) * (40.0 - 20.0) + mc[3] * (40.0 - 5.0)
    mfe = gana / (450.0 * (450.0 - 40.0))
    L = _des("PR1-1", "Calorimetría con aluminio, agua, hierro y cobre", "4.6", _f1(1, 2),
             "Un calorímetro de aluminio de $0{,}150$ kg contiene $0{,}800$ kg de agua a $20{,}0\\ ^\\circ$C. Se introducen una pieza de hierro de $0{,}600$ kg a $450\\ ^\\circ$C y un bloque de cobre de $0{,}400$ kg a $5{,}0\\ ^\\circ$C. Sistema aislado, sin evaporación. 1) ¿Quién cede y quién absorbe calor? 2) Temperatura final. 3) Calor de cada componente y verificación. 4) Masa de hierro a $450\\ ^\\circ$C para que la final sea $40{,}0\\ ^\\circ$C.",
             claves=["calorimetría", "varias sustancias", "práctica"],
             visual={"tipo": "calor", "Tf": round(tf, 1),
                     "componentes": [{"n": "agua", "m": 800, "T": 20, "fase": "agua"}, {"n": "aluminio", "m": 150, "T": 20, "fase": "solido"},
                                     {"n": "hierro", "m": 600, "T": 450, "fase": "caliente"}, {"n": "cobre", "m": 400, "T": 5, "fase": "solido"}],
                     "necesita": [{"n": "agua", "Q": qs[1]}, {"n": "aluminio", "Q": qs[0]}, {"n": "cobre", "Q": qs[3]}],
                     "disponible": [{"n": "hierro", "Q": -qs[2]}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro cuerpos que se mezclan en un recipiente aislado: el calor solo pasa entre ellos.", DATO("aislado"),
            TABLA(["Cuerpo", "$m$ (kg)", "$c$ (J/(kg·K))", "$T_i$ (°C)"],
                  [["Aluminio", "$0{,}150$", "$900$", "$20{,}0$"], ["Agua", "$0{,}800$", "$4186$", "$20{,}0$"],
                   ["Hierro", "$0{,}600$", "$450$", "$450$"], ["Cobre", "$0{,}400$", "$390$", "$5{,}0$"]]))\
        .di("Para cada cuerpo usamos el calor sensible de la hoja.", F(r"Q=mc\,\Delta T"))
    L.paso("1) Quién cede y quién absorbe")\
        .di("La temperatura final tiene que quedar entre la más fría y la más caliente.", M(r"5{,}0\ ^\circ\text{C}<T_f<450\ ^\circ\text{C}"))\
        .di("El hierro es el más caliente: seguro baja y cede calor. El cobre es el más frío: seguro sube y absorbe.",
            M(r"Q_{\text{Fe}}=m c\,(T_f-450)<0\qquad Q_{\text{Cu}}=mc\,(T_f-5{,}0)>0"))\
        .di("El agua y el aluminio empiezan a veinte grados: dependen de si la final queda arriba o abajo de veinte. Eso no se sabe sin calcular.",
            NOTA("Respuesta: no se puede saber con certeza para todos. Hierro cede y cobre absorbe; agua y aluminio dependen de $T_f$."))
    L.paso("2) Balance de energía")\
        .di("Recipiente aislado: la suma de todos los calores es cero.", M(r"Q_{\text{Al}}+Q_{\text{agua}}+Q_{\text{Fe}}+Q_{\text{Cu}}=0"))\
        .di("Cada uno es su eme ce por el cambio de temperatura.".replace("eme ce", dice("mc", "eme ce")), M(r"\sum m c\,(T_f-T_i)=0"))\
        .di("Abrimos el paréntesis de cada cuerpo: su eme ce multiplica a las dos temperaturas.".replace("eme ce", dice("mc", "eme ce")),
            M(r"\sum mc\,T_f-\sum mc\,T_i=0"))\
        .di("La temperatura final es la misma para todos: sale de la suma como factor común.", M(r"T_f\sum mc-\sum mc\,T_i=0"))\
        .di("La segunda suma pasa al otro lado sumando.", M(r"T_f\sum mc=\sum mc\,T_i"))\
        .di("Y la suma de los eme ce pasa a dividir.".replace("eme ce", dice("mc", "eme ce")), M(r"T_f=\frac{\sum mc\,T_i}{\sum mc}"))
    p = L.paso("2) Los productos mc de cada cuerpo")
    for (_, nom, m, c, _), x in zip(comp, mc):
        p.di(f"El {nom}.", M(rf"m c_{{\text{{{nom[:2]}}}}}=({tex(m, 3)})({tex(c, 4)})={tex(x, 5)}\ \text{{J/K}}"))
    p.di("Sumamos.", M(r"\sum mc=" + "+".join(tex(x, 5) for x in mc) + rf"={tex(smc, 5)}\ \text{{J/K}}"))
    p = L.paso("2) Los productos mc por la temperatura inicial")
    for (_, nom, _, _, t), x in zip(comp, mc):
        p.di(f"El {nom}.", M(rf"({tex(x, 5)})({tex(t, 3)})={tex(x * t, 6, sci='no')}"))
    p.di("Sumamos.", M(r"\sum mc\,T_i=" + "+".join(tex(x * t, 6, sci='no') for x, (_, _, _, _, t) in zip(mc, comp)) + rf"={tex(smct, 6, sci='no')}"))
    L.paso("2) Temperatura final")\
        .di("Dividimos.", M(rf"T_f=\frac{{{tex(smct, 6, sci='no')}}}{{{tex(smc, 5)}}}"))\
        .di(f"Queda en {nu(tf, '°C', 3)}.", R(rf"T_f={tex(tf, 3)}\ {C}", "ra"))\
        .di("Es menor que cien: no hierve nada, como dice el enunciado. Y es mayor que veinte: el agua y el aluminio absorben.",
            NOTA("Con $T_f>20\\ ^\\circ$C el agua y el aluminio también absorben calor."))
    p = L.paso("3) Calor de cada componente")
    for (_, nom, _, _, t), x, q in zip(comp, mc, qs):
        p.di(f"El {nom}.", M(rf"Q_{{\text{{{nom}}}}}=({tex(x, 5)})({tex(tf, 5)}-{tex(t, 3)})={_j(q)}\ \text{{J}}"))
    redondos = [round(q) for q in qs]
    s = sum(redondos)
    L.paso("3) Revisión: la suma es cero")\
        .di("Sumamos los cuatro calores: lo que cede el hierro lo reciben los otros tres.",
            M(r"\sum Q=" + "+".join(f"({_j(q)})" for q in redondos) + ("=0" if s == 0 else rf"={s}\approx0")),
            R(r"\sum Q=0\ \ (\text{verificado})", "rc"))
    L.paso("4) Masa de hierro para terminar en 40,0 °C")\
        .di("Ahora la final es dato y la masa del hierro es la incógnita. Lo que ganan aluminio, agua y cobre:",
            M(rf"Q_{{\text{{ganado}}}}=({tex(mc[0] + mc[1], 5)})(40{{,}}0-20{{,}}0)+({tex(mc[3], 3)})(40{{,}}0-5{{,}}0)"),
            M(rf"Q_{{\text{{ganado}}}}={_j((mc[0] + mc[1]) * 20.0)}+{_j(mc[3] * 35.0)}={_j(gana)}\ \text{{J}}"))\
        .di("Eso lo tiene que ceder el hierro al bajar de cuatrocientos cincuenta a cuarenta.",
            M(rf"m_{{\text{{Fe}}}}(450)(450-40{{,}}0)={_j(gana)}"), M(rf"m_{{\text{{Fe}}}}=\frac{{{_j(gana)}}}{{{_j(450.0 * 410.0)}}}"))\
        .di(f"Hacen falta {nu(mfe, 'kg', 3)} de hierro.", R(rf"m_{{\text{{Fe}}}}={tex(mfe, 3)}\ \text{{kg}}", "rd"))
    L.responde("2", rf"T_f={tex(tf, 3)}\ {C}", tf, "°C").responde("3", rf"Q_{{\text{{Fe}}}}={_j(qs[2], 4)}\ \text{{J}}", qs[2], "J")\
     .responde("4", rf"m_{{\text{{Fe}}}}={tex(mfe, 3)}\ \text{{kg}}", mfe, "kg")
    return L.usa(r"Q=mc\Delta T")


# ============================================================================ PR1-2
@leccion("PR1-2")
def pr1_2() -> Leccion:
    mh, mv, ch, cw, lf, lv = 0.250, 0.0400, 2100.0, 4186.0, 3.34e5, 2.256e6
    q1 = mh * ch * 20.0
    q2 = mh * lf
    qn = q1 + q2
    q3 = mv * lv
    q4 = mv * cw * 100.0
    qmax = q3 + q4
    a, b = mh * cw, mv * cw
    tf = (q3 + q4 - qn) / (a + b)
    mmin = qn / (lv + cw * 100.0)
    L = _des("PR1-2", "Hielo a −20 °C con vapor a 100 °C", "4.7", _f1(2, 2),
             "En un recipiente aislado de capacidad calorífica despreciable se colocan $0{,}250$ kg de hielo a $-20{,}0\\ ^\\circ$C y se inyectan $0{,}0400$ kg de vapor a $100\\ ^\\circ$C. 1) ¿Se funde todo el hielo? 2) Temperatura final y masas de cada fase. 3) Masa mínima de vapor para fundir exactamente todo el hielo.",
             aviso="Nota editorial del PDF: el encabezado indica (15 pts), pero los incisos impresos suman 6 + 4 + 2 = 12 pts. La física y todos los cálculos aquí están verificados.",
             claves=["cambio de fase", "vapor", "hielo", "práctica"],
             visual={"tipo": "calor", "Tf": round(tf, 1),
                     "componentes": [{"n": "hielo", "m": 250, "T": -20, "fase": "hielo"}, {"n": "vapor", "m": 40, "T": 100, "fase": "vapor"}],
                     "necesita": [{"n": "hielo de −20 a 0 °C", "Q": q1}, {"n": "fundir el hielo", "Q": q2}],
                     "disponible": [{"n": "condensar el vapor", "Q": q3}, {"n": "enfriar a 0 °C", "Q": q4}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Hielo muy frío y vapor muy caliente. Primero hay que ver qué cambios de fase alcanzan a ocurrir.", DATO("0,0400 kg de vapor"),
            LISTA(["Hielo: $0{,}250$ kg a $-20{,}0\\ ^\\circ$C, $c_h=2100$ J/(kg·K)", "Vapor: $0{,}0400$ kg a $100\\ ^\\circ$C", "$L_f=3{,}34\\times10^5$ J/kg, $L_v=2{,}256\\times10^6$ J/kg, $c_w=4186$ J/(kg·K)"]))\
        .di("Usamos las dos casillas de calorimetría de la hoja.", F(r"Q=mc\,\Delta T"), F(r"Q=\pm mL"))
    L.paso("1) Lo que necesita el hielo para ser agua a 0 °C")\
        .di("Calentar el hielo de menos veinte a cero.", M(rf"Q_1=m_hc_h\,\Delta T=(0{{,}}250)(2100)(20{{,}}0)={_j(q1)}\ \text{{J}}"))\
        .di("Fundirlo a cero grados.", M(rf"Q_2=m_hL_f=(0{{,}}250)(3{{,}}34\times10^5)={_j(q2)}\ \text{{J}}"))\
        .di("En total.", M(rf"Q_{{\text{{necesita}}}}={_j(q1)}+{_j(q2)}={_j(qn)}\ \text{{J}}"))
    L.paso("1) Lo máximo que puede dar el vapor hasta 0 °C")\
        .di("Condensarse a cien grados.", M(rf"Q_3=m_vL_v=(0{{,}}0400)(2{{,}}256\times10^6)={_j(q3)}\ \text{{J}}"))\
        .di("Y enfriarse como agua de cien a cero.", M(rf"Q_4=m_vc_w\,\Delta T=(0{{,}}0400)(4186)(100)={_j(q4)}\ \text{{J}}"))\
        .di("En total.", M(rf"Q_{{\text{{máx}}}}={_j(q3)}+{_j(q4)}={_j(qmax)}\ \text{{J}}"))
    L.paso("1) Comparar")\
        .di("El vapor puede dar más de lo que el hielo necesita: todo el hielo se funde y sobra energía.",
            M(rf"{_j(qmax)}\ \text{{J}}>{_j(qn)}\ \text{{J}}"), M(rf"\text{{sobra}}={_j(qmax)}-{_j(qn)}={_j(qmax - qn)}\ \text{{J}}"),
            R(r"\text{Sí, se funde todo el hielo}", "ra"))
    L.paso("2) Temperatura final: plantear")\
        .di("Al final todo es agua líquida a una temperatura te.".replace("te.", dice("T.", "te.")), M(r"Q_{\text{hielo}}+Q_{\text{vapor}}=0"))\
        .di("El hielo se calienta, se funde y el agua que resulta sube de cero a te.".replace("a te", "a " + dice("T", "te")),
            M(rf"Q_{{\text{{hielo}}}}={_j(qn)}+(0{{,}}250)(4186)(T-0)={_j(qn)}+{tex(a, 5)}\,T"))\
        .di("El vapor se condensa, por eso el signo menos, y el agua baja de cien a te.".replace("a te", "a " + dice("T", "te")),
            M(rf"Q_{{\text{{vapor}}}}=-{_j(q3)}+(0{{,}}0400)(4186)(T-100)=-{_j(q3)}+{tex(b, 5)}\,T-{_j(q4)}"))
    L.paso("2) Temperatura final: despejar")\
        .di("Sumamos e igualamos a cero.", M(rf"{_j(qn)}+{tex(a, 5)}\,T-{_j(q3)}+{tex(b, 5)}\,T-{_j(q4)}=0"))\
        .di("Juntamos las te de un lado.".replace("las te", "las " + dice("T", "te")), M(rf"({tex(a, 5)}+{tex(b, 5)})\,T={_j(q3)}+{_j(q4)}-{_j(qn)}"),
            M(rf"{tex(a + b, 6)}\,T={_j(q3 + q4 - qn)}"))\
        .di(f"Te final vale {nu(tf, '°C', 3)}.".replace("Te final", dice("T final", "Te final")), M(rf"T=\frac{{{_j(q3 + q4 - qn)}}}{{{tex(a + b, 6)}}}"), R(rf"T_f={tex(tf, 3)}\ {C}", "rb"))
    L.paso("2) Estado final")\
        .di("No queda hielo ni vapor: todo es agua líquida.", M(rf"m_{{\text{{agua}}}}=0{{,}}250+0{{,}}0400=0{{,}}290\ \text{{kg}}"),
            R(rf"0{{,}}290\ \text{{kg de agua líquida a }}{tex(tf, 3)}\ {C}", "rb2"))
    L.paso("3) Masa mínima de vapor")\
        .di("Lo justo para fundir todo el hielo: termina todo a cero grados, sin energía de sobra.",
            M(rf"m_v\left[L_v+c_w(100-0)\right]=Q_{{\text{{necesita}}}}={_j(qn)}\ \text{{J}}"))\
        .di("Sustituimos.", M(rf"m_v=\frac{{{_j(qn)}}}{{2{{,}}256\times10^6+(4186)(100)}}=\frac{{{_j(qn)}}}{{{_j(lv + cw * 100.0, 7)}}}"))\
        .di(f"Son {nu(mmin * 1000, 'g', 3)}.", R(rf"m_v={tex(mmin, 3)}\ \text{{kg}}={tex(mmin * 1000, 3)}\ \text{{g}}", "rc"))
    L.responde("2", rf"T_f={tex(tf, 3)}\ {C}", tf, "°C").responde("3", rf"m_v={tex(mmin, 3)}\ \text{{kg}}", mmin, "kg")
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ PR1-3
@leccion("PR1-3")
def pr1_3() -> Leccion:
    lc, r1, r2, r3, k1, k2, t1, t3 = 10.0, 0.0250, 0.0450, 0.0550, 0.040, 0.20, 150.0, 30.0
    l1, l2 = math.log(r2 / r1), math.log(r3 / r2)
    d1, d2 = 2 * math.pi * k1 * lc, 2 * math.pi * k2 * lc
    R1, R2 = l1 / d1, l2 / d2
    H = (t1 - t3) / (R1 + R2)
    ti = t1 - H * R1
    rm = 0.0350
    tm = t1 - (H / d1) * math.log(rm / r1)
    q = H * 3600.0
    mv = q / 2.256e6
    L = _des("PR1-3", "Tubería de vapor con dos capas de aislante", "5.4", _f1(3, 3),
             "Tubería de $10{,}0$ m, radio exterior $r_1=2{,}50$ cm a $150\\ ^\\circ$C, con dos capas: lana de vidrio de $2{,}50$ a $4{,}50$ cm ($k=0{,}040$) y recubrimiento de $4{,}50$ a $5{,}50$ cm ($k=0{,}20$). Afuera, $30{,}0\\ ^\\circ$C. 1) Demuestre $H=2\\pi kL(T_{int}-T_{ext})/\\ln(r_{ext}/r_{int})$. 2) Resistencia térmica y por qué van en serie. 3) $H$ y temperatura en la interfaz. 4) $T$ en $r=3{,}50$ cm; ¿es lineal en $r$? 5) Masa de vapor condensada por hora.",
             claves=["cilindro", "dos capas", "resistencia térmica", "práctica"],
             visual={"tipo": "conduccion", "geo": "cilindro2", "r1": r1, "r2": r2, "r3": r3, "L": lc, "T1": t1, "T2": round(ti, 2), "T3": t3,
                     "k1": k1, "k2": k2, "n1": "lana de vidrio", "n2": "recubrimiento", "r": rm, "uT": "°C"})
    L.paso("Qué nos dan y qué piden")\
        .di("El calor sale del vapor hacia afuera atravesando dos capas cilíndricas, una dentro de la otra.", DATO("dos capas cilíndricas"),
            TABLA(["Capa", "$r_{int}$ (cm)", "$r_{ext}$ (cm)", "$k$ (W/(m·K))"], [["1: lana de vidrio", "$2{,}50$", "$4{,}50$", "$0{,}040$"], ["2: recubrimiento", "$4{,}50$", "$5{,}50$", "$0{,}20$"]]),
            LISTA(["$L=10{,}0$ m, $T(2{,}50\\ \\text{cm})=150\\ ^\\circ$C, $T(5{,}50\\ \\text{cm})=30{,}0\\ ^\\circ$C"]))
    L.paso("1) Fourier en un cascarón cilíndrico")\
        .di("Partimos de la ley de Fourier de la hoja, en dirección radial.", V(foco="dr"), F(r"H=-kA\,\frac{dT}{dr}"))\
        .di("A un radio erre el calor cruza la pared lateral de un cilindro.".replace("radio erre", "radio " + dice("r", "erre")),
            M(r"A(r)=2\pi rL\;\Rightarrow\;H=-k\,(2\pi rL)\,\frac{dT}{dr}"))
    L.paso("1) Separar variables (desde cero)")\
        .di("Separar es dejar lo del radio a un lado y lo de la temperatura al otro. Paso uno: multiplicar los dos lados por de erre.".replace("de erre", dice("dr", "de erre")),
            M(r"H\,dr=-k\,(2\pi rL)\,dT"))\
        .di("Paso dos: dividir los dos lados entre erre. Hache y ka no cambian.".replace("entre erre", "entre " + dice("r", "erre")).replace("Hache y ka", dice("H y k", "Hache y ka")),
            M(r"H\,\frac{dr}{r}=-2\pi kL\,dT"), NOTA("La receta completa está en la lección **T-MAT3**."))
    L.paso("1) Integrar los dos lados")\
        .di("Integral a los dos lados, con los mismos puntos: en el radio interno está la temperatura interna; en el externo, la externa.", V(foco="barrido"),
            M(r"H\int_{r_{int}}^{r_{ext}}\frac{dr}{r}=-2\pi kL\int_{T_{int}}^{T_{ext}}dT"))\
        .di("Izquierda: uno entre erre da logaritmo, la regla dos. Arriba menos abajo; una resta de logaritmos es el logaritmo de la división.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"\int_{r_{int}}^{r_{ext}}\frac{dr}{r}=\Big[\ln r\Big]_{r_{int}}^{r_{ext}}=\ln r_{ext}-\ln r_{int}=\ln\frac{r_{ext}}{r_{int}}"))\
        .di("Derecha: sumar todos los pedacitos de temperatura da el cambio total.",
            M(r"-2\pi kL\int_{T_{int}}^{T_{ext}}dT=-2\pi kL\,(T_{ext}-T_{int})=2\pi kL\,(T_{int}-T_{ext})"))
    L.paso("1) Juntar y despejar H")\
        .di("Juntamos los dos lados.", M(r"H\ln\left(\frac{r_{ext}}{r_{int}}\right)=2\pi kL\,(T_{int}-T_{ext})"))\
        .di("Dividimos los dos lados entre el logaritmo: es lo que había que demostrar.", R(r"H=\frac{2\pi kL\,(T_{int}-T_{ext})}{\ln(r_{ext}/r_{int})}", "r1"))
    L.paso("2) Resistencia térmica y capas en serie")\
        .di("Despejamos la caída de temperatura: es hache por un factor que solo depende de la capa.".replace("hache", dice("H", "hache")),
            M(r"T_{int}-T_{ext}=H\,\frac{\ln(r_{ext}/r_{int})}{2\pi kL}"))\
        .di("A ese factor se le llama resistencia térmica.", M(r"R\equiv\frac{T_{int}-T_{ext}}{H}=\frac{\ln(r_{ext}/r_{int})}{2\pi kL}"))\
        .di("En estado estacionario no se acumula calor: el mismo hache cruza las dos capas. Escribimos la caída de cada una.".replace("hache", dice("H", "hache")),
            M(r"T_1-T_i=HR_1"), M(r"T_i-T_2=HR_2"))\
        .di("Sumamos las dos ecuaciones: la temperatura de la interfaz entra una vez restando y otra sumando, y se cancela.",
            M(r"(T_1-T_i)+(T_i-T_2)=HR_1+HR_2"), M(r"T_1-T_2=H\,(R_1+R_2)"),
            NOTA("Por eso se combinan **en serie**: mismo $H$ y caídas de temperatura que se suman."))
    L.paso("3) Las dos resistencias")\
        .di("Capa uno, lana de vidrio. Llevamos cinco cifras, para que la suma y la división de después cuadren con la calculadora.",
            M(rf"R_1=\frac{{\ln(4{{,}}50/2{{,}}50)}}{{2\pi(0{{,}}040)(10{{,}}0)}}=\frac{{{tex(l1, 5)}}}{{{tex(d1, 5)}}}={tex(R1, 5)}\ \text{{K/W}}"))\
        .di("Capa dos, recubrimiento.", M(rf"R_2=\frac{{\ln(5{{,}}50/4{{,}}50)}}{{2\pi(0{{,}}20)(10{{,}}0)}}=\frac{{{tex(l2, 5)}}}{{{tex(d2, 5)}}}={tex(R2, 5)}\ \text{{K/W}}"))\
        .di("La suma.", M(rf"R_1+R_2={tex(R1, 5)}+{tex(R2, 5)}={tex(R1 + R2, 5)}\ \text{{K/W}}"))
    L.paso("3) Corriente de calor y temperatura de la interfaz")\
        .di("La caída total entre la suma de resistencias.", V(foco="flujo"), M(rf"H=\frac{{150-30{{,}}0}}{{{tex(R1 + R2, 5)}}}=\frac{{120}}{{{tex(R1 + R2, 5)}}}"),
            R(rf"H={tex(H, 4)}\ \text{{W}}", "r3a"))\
        .di("En la interfaz restamos la caída de la capa uno.", M(rf"T_i=T_1-HR_1=150-({tex(H, 4)})({tex(R1, 5)})=150-{tex(H * R1, 4)}"),
            R(rf"T_i={tex(ti, 3)}\ {C}", "r3b"))\
        .di("Revisión con la capa dos.", M(rf"T_i-30{{,}}0=HR_2=({tex(H, 4)})({tex(R2, 5)})={tex(H * R2, 3)}"))
    L.paso("4) La temperatura a un radio r cualquiera")\
        .di("Repetimos la integral de la parte uno, pero solo desde el radio interno hasta un radio erre dentro de la capa uno.".replace("radio erre", "radio " + dice("r", "erre")), V(foco="punto"),
            M(r"H\int_{r_1}^{r}\frac{dr}{r}=-2\pi k_1L\int_{T_1}^{T}dT"))\
        .di("Las dos integrales salen igual que antes: un logaritmo y una resta.", M(r"H\ln\left(\frac{r}{r_1}\right)=-2\pi k_1L\,(T-T_1)"))\
        .di("Para despejar te, dividimos los dos lados entre menos dos pi ka uno ele.".replace("despejar te", "despejar " + dice("T", "te")),
            M(r"T-T_1=-\frac{H}{2\pi k_1L}\ln\left(\frac{r}{r_1}\right)"))\
        .di("Te uno está restando: pasa al otro lado sumando.".replace("Te uno", dice("T₁", "Te uno")),
            M(r"T(r)=T_1-\frac{H}{2\pi k_1L}\ln\left(\frac{r}{r_1}\right)"))
    L.paso("4) Temperatura en r = 3,50 cm")\
        .di("Sustituimos.", M(rf"T=150-\frac{{{tex(H, 4)}}}{{{tex(d1, 4)}}}\ln\left(\frac{{3{{,}}50}}{{2{{,}}50}}\right)=150-({tex(H / d1, 4)})({tex(math.log(rm / r1), 4)})"),
            R(rf"T(3{{,}}50\ \text{{cm}})={tex(tm, 3)}\ {C}", "r4"))\
        .di("¿Es una recta? De la ley de Fourier despejamos la pendiente: depende de uno entre erre, así que cambia con el radio. Una recta tendría pendiente fija.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"H=-k_1(2\pi rL)\,\frac{dT}{dr}\;\Rightarrow\;\frac{dT}{dr}=-\frac{H}{2\pi k_1L}\cdot\frac{1}{r}"),
            NOTA(f"No es lineal. Una recta daría ${tex((t1 + ti) / 2, 3)}\\ ^\\circ$C a mitad de la capa; el logaritmo da ${tex(tm, 3)}\\ ^\\circ$C."))
    L.paso("5) Vapor que se condensa en una hora")\
        .di("De la hoja, hache es calor por tiempo.".replace("hache", dice("H", "hache")), F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"),
            M(rf"Q=({tex(H, 4)})(3600)={tex(q, 4)}\ \text{{J}}"))\
        .di("Ese calor sale de condensar vapor.", F(r"Q=\pm mL"), M(rf"m=\frac{{Q}}{{L_v}}=\frac{{{tex(q, 4)}}}{{2{{,}}256\times10^6}}"),
            R(rf"m={tex(mv, 3)}\ \text{{kg por hora}}", "r5"))
    L.responde("3", rf"H={tex(H, 4)}\ \text{{W}}", H, "W").responde("3", rf"T_i={tex(ti, 3)}\ {C}", ti, "°C")\
     .responde("4", rf"T={tex(tm, 3)}\ {C}", tm, "°C").responde("5", rf"m={tex(mv, 3)}\ \text{{kg/h}}", mv, "kg/h")
    return L.usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}", r"Q=\pm mL")


# ============================================================================ PR1-4
@leccion("PR1-4")
def pr1_4() -> Leccion:
    k, lb, r1, r2, t1, t2 = 205.0, 0.200, 0.0100, 0.0300, 100.0, 0.0
    b = (r2 - r1) / lb
    num = math.pi * k * r1 * r2 * (t1 - t2)
    H = num / lb
    fac = H / (math.pi * k * b)
    q = H * 3600.0
    mh = q / 3.34e5
    rmid = r1 + b * lb / 2
    tmid = t1 - fac * (1 / r1 - 1 / rmid)
    inv50 = 1 / r1 - (t1 - 50.0) / fac
    r50 = 1 / inv50
    x50 = (r50 - r1) / b
    pts = [[round(lb * i / 40, 4), round(t1 - fac * (1 / r1 - 1 / (r1 + b * lb * i / 40)), 2)] for i in range(41)]
    L = _des("PR1-4", "Barra de aluminio en tronco de cono", "5.5", _f1(4, 3),
             "Barra de aluminio ($k=205$ W/(m·K)) en tronco de cono de $L=20{,}0$ cm; radio de $r_1=1{,}00$ cm en $x=0$ (vapor a $100\\ ^\\circ$C) a $r_2=3{,}00$ cm en $x=L$ (agua con hielo a $0\\ ^\\circ$C). Superficie lateral aislada. 1) $r(x)$ y $A(x)$. 2) Demuestre $H=\\pi k r_1r_2(T_1-T_2)/L$. 3) $H$ y hielo fundido por hora. 4) $T(L/2)$ y posición donde $T=50\\ ^\\circ$C. 5) Caso $r_1=r_2$; ¿cambia $H$ si se invierte la barra?",
             claves=["tronco de cono", "área variable", "integral", "práctica"],
             visual={"tipo": "conduccion", "geo": "pared", "L": lb, "T1": t1, "T2": t2, "eje": "x", "uT": "°C",
                     "forma": "0.33 + 0.67 * u", "pts": pts, "xm": lb / 2, "etA": "π r(x)²"})
    L.paso("Qué nos dan y qué piden")\
        .di("El extremo angosto está en vapor y el ancho en hielo: el calor va de lo angosto a lo ancho.", DATO("tronco de cono"),
            LISTA(["$k=205$ W/(m·K), $L=0{,}200$ m", "$x=0$: $r_1=0{,}0100$ m, $T_1=100\\ ^\\circ$C", "$x=L$: $r_2=0{,}0300$ m, $T_2=0\\ ^\\circ$C"]))
    L.paso("1) Radio y área a una distancia x")\
        .di("El radio crece en línea recta de erre uno a erre dos.".replace("erre uno", dice("r₁", "erre uno")).replace("erre dos", dice("r₂", "erre dos")), V(foco="dx"),
            M(r"r(x)=r_1+\frac{r_2-r_1}{L}\,x"), M(rf"r(x)=0{{,}}0100+\frac{{0{{,}}0300-0{{,}}0100}}{{0{{,}}200}}\,x=0{{,}}0100+{tex(b, 3)}\,x"))\
        .di("La sección es un círculo de ese radio.", R(r"A(x)=\pi\,r(x)^2=\pi\left(r_1+\frac{r_2-r_1}{L}x\right)^2", "r1"))
    L.paso("2) Fourier y separar variables")\
        .di("Partimos de la ley de Fourier de la hoja. Llamamos a al radio inicial y be a la pendiente.".replace("a al", dice("a", "a") + " al").replace("y be", "y " + dice("b", "be")),
            F(r"H=-kA\,\frac{dT}{dx}"), M(r"a=r_1\qquad b=\frac{r_2-r_1}{L}\;\Rightarrow\;H=-k\pi(a+bx)^2\,\frac{dT}{dx}"))\
        .di("Separamos variables. Paso uno: multiplicar los dos lados por de equis.".replace("de equis", dice("dx", "de equis")), M(r"H\,dx=-k\pi(a+bx)^2\,dT"))\
        .di("Paso dos: dividir los dos lados entre pi por el paréntesis al cuadrado. Hache es constante.".replace("Hache", dice("H", "Hache")), M(r"\frac{H}{\pi}\,\frac{dx}{(a+bx)^2}=-k\,dT"))\
        .di("Integramos toda la barra: equis va de cero a ele y la temperatura de te uno a te dos.".replace("equis", dice("x", "equis")).replace("a ele", "a " + dice("L", "ele")).replace("te uno a te dos", dice("T₁ a T₂", "te uno a te dos")), V(foco="barrido"),
            M(r"\frac{H}{\pi}\int_0^L\frac{dx}{(a+bx)^2}=-k\int_{T_1}^{T_2}dT"))
    L.paso("2) La integral de la izquierda: cambio de variable")\
        .di("Abajo está el paréntesis al cuadrado. A ese paréntesis le ponemos un nombre nuevo: u. Es justo el radio en cada punto.",
            M(r"u=a+bx=r(x)"))\
        .di("Si equis crece un pedacito, u crece be veces ese pedacito, porque be es la pendiente. El número a no cambia.".replace("equis", dice("x", "equis")),
            M(r"du=b\,dx\;\Rightarrow\;dx=\frac{du}{b}"))\
        .di("Los límites también cambian. Donde equis vale cero, u vale erre uno; donde equis vale ele, u vale erre dos.".replace("erre uno", dice("r₁", "erre uno")).replace("erre dos", dice("r₂", "erre dos")),
            M(r"x=0\;\Rightarrow\;u=a=r_1\qquad x=L\;\Rightarrow\;u=a+bL=r_2"))\
        .di("Cambiamos todo a la letra u: el paréntesis por u, y de equis por de u entre be.".replace("de equis", dice("dx", "de equis")).replace("de u entre be", dice("du/b", "de u entre be")),
            M(r"\int_0^L\frac{dx}{(a+bx)^2}=\int_{r_1}^{r_2}\frac{1}{u^2}\cdot\frac{du}{b}=\frac{1}{b}\int_{r_1}^{r_2}u^{-2}\,du"))
    L.paso("2) La integral de la izquierda: regla de la potencia")\
        .di("Uno entre u al cuadrado es u a la menos dos. Regla de la potencia: al exponente se le suma uno y se divide entre ese exponente nuevo.",
            M(r"\int u^{-2}\,du=\frac{u^{-2+1}}{-2+1}=\frac{u^{-1}}{-1}=-\frac{1}{u}"))\
        .di("Ahora los límites: lo de arriba menos lo de abajo.",
            M(r"\frac{1}{b}\Big[-\frac{1}{u}\Big]_{r_1}^{r_2}=\frac{1}{b}\left[\left(-\frac{1}{r_2}\right)-\left(-\frac{1}{r_1}\right)\right]"))\
        .di("Menos por menos da más, y ordenamos. Es lo mismo que dice la sugerencia del enunciado.",
            M(r"=\frac{1}{b}\left(\frac{1}{r_1}-\frac{1}{r_2}\right)"), NOTA("La sugerencia del enunciado, $\\int\\frac{dx}{(a+bx)^2}=-\\frac{1}{b(a+bx)}$, sale de este mismo cambio de variable."))
    L.paso("2) Simplificar la resta de fracciones")\
        .di("Restamos las fracciones con el denominador común erre uno por erre dos.".replace("erre uno por erre dos", dice("r₁r₂", "erre uno por erre dos")),
            M(r"\frac{1}{r_1}-\frac{1}{r_2}=\frac{r_2}{r_1r_2}-\frac{r_1}{r_1r_2}=\frac{r_2-r_1}{r_1r_2}"))\
        .di("Y uno entre be es ele entre la resta de radios: la resta se cancela arriba y abajo.".replace("uno entre be es ele", dice("1/b = L/(r₂ − r₁)", "uno entre be es ele")),
            M(r"\frac{1}{b}\cdot\frac{r_2-r_1}{r_1r_2}=\frac{L}{r_2-r_1}\cdot\frac{r_2-r_1}{r_1r_2}=\frac{L}{r_1r_2}"))
    L.paso("2) La derecha, juntar y despejar H")\
        .di("La derecha: sumar los pedacitos de temperatura da el cambio total.", M(r"-k\int_{T_1}^{T_2}dT=-k\,(T_2-T_1)=k\,(T_1-T_2)"))\
        .di("Juntamos los dos lados.", M(r"\frac{H}{\pi}\cdot\frac{L}{r_1r_2}=k\,(T_1-T_2)"))\
        .di("Despejamos hache: multiplicamos por pi erre uno erre dos y dividimos entre ele.".replace("hache", dice("H", "hache")).replace("pi erre uno erre dos", dice("πr₁r₂", "pi erre uno erre dos")).replace("entre ele", "entre " + dice("L", "ele")),
            R(r"H=\frac{\pi k\,r_1r_2\,(T_1-T_2)}{L}", "r2"))
    L.paso("3) Calcular H")\
        .di("Sustituimos los radios y el largo en metros, junto con la conductividad y la diferencia de temperaturas.", V(foco="perfil"), M(r"H=\frac{\pi(205)(0{,}0100)(0{,}0300)(100-0)}{0{,}200}"))\
        .di("Multiplicamos todos los factores del numerador y dividimos entre cero coma doscientos metros.", M(rf"H=\frac{{{tex(num, 4)}}}{{0{{,}}200}}"), R(rf"H={tex(H, 3)}\ \text{{W}}", "r3a"))
    L.paso("3) Hielo que se funde por hora")\
        .di("De la hoja, la corriente de calor es energía por unidad de tiempo; multiplicamos por tres mil seiscientos segundos de una hora.", F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"), M(rf"Q=({tex(H, 4)})(3600)={tex(q, 4)}\ \text{{J}}"))\
        .di("Esa energía entra al hielo a cero grados para fundirlo; dividimos entre el calor latente de fusión.", F(r"Q=\pm mL"), M(rf"m=\frac{{Q}}{{L_f}}=\frac{{{tex(q, 4)}}}{{3{{,}}34\times10^5}}"),
            R(rf"m={tex(mh, 3)}\ \text{{kg por hora}}", "r3b"))
    L.paso("4) La temperatura a lo largo de la barra")\
        .di("Repetimos la integral, pero solo desde cero hasta una equis cualquiera.".replace("equis", dice("x", "equis")),
            M(r"\frac{H}{\pi}\int_0^x\frac{dx}{(a+bx)^2}=-k\int_{T_1}^{T(x)}dT"))\
        .di("La izquierda da lo mismo que antes, pero arriba queda el radio en equis.".replace("en equis", "en " + dice("x", "equis")),
            M(r"\frac{H}{\pi b}\left(\frac{1}{r_1}-\frac{1}{r(x)}\right)=k\,(T_1-T(x))"))\
        .di("Despejamos te de equis: dividimos entre ka y pasamos restando.".replace("te de equis", dice("T(x)", "te de equis")).replace("entre ka", "entre " + dice("k", "ka")),
            M(r"T(x)=T_1-\frac{H}{\pi k b}\left(\frac{1}{r_1}-\frac{1}{r(x)}\right)"))\
        .di("Calculamos el factor de adelante.", M(rf"\frac{{H}}{{\pi kb}}=\frac{{{tex(H, 4)}}}{{\pi(205)({tex(b, 3)})}}={tex(fac, 4)}\ \text{{K}}\cdot\text{{m}}"))
    L.paso("4) Punto medio y dónde vale 50 °C")\
        .di("A la mitad de la barra el radio vale dos centímetros; sustituimos en la expresión de temperatura y restamos los inversos.", V(foco="punto"), M(rf"r(L/2)={tex(rmid, 3)}\ \text{{m}}"),
            M(rf"T=100-{tex(fac, 4)}\left(\frac{{1}}{{0{{,}}0100}}-\frac{{1}}{{{tex(rmid, 3)}}}\right)=100-{tex(fac, 4)}(100-50{{,}}0)"),
            R(rf"T(L/2)={tex(tmid, 3)}\ {C}", "r4a"))\
        .di("Ahora, dónde vale cincuenta grados. Igualamos la expresión a cincuenta. El cien de adentro es uno entre erre uno.".replace("erre uno", dice("r₁", "erre uno")),
            M(rf"50=100-{tex(fac, 4)}\left(100-\frac{{1}}{{r}}\right)"))\
        .di("Pasamos el paréntesis a la izquierda y el cincuenta a la derecha: cada uno cambia de signo.",
            M(rf"{tex(fac, 4)}\left(100-\frac{{1}}{{r}}\right)=100-50=50"))
    L.paso("4) Dónde vale 50 °C: despejar el radio y la posición")\
        .di(f"Dividimos los dos lados entre {n(fac, 4)}.", M(rf"100-\frac{{1}}{{r}}=\frac{{50}}{{{tex(fac, 4)}}}={tex((t1 - 50.0) / fac, 4)}"))\
        .di("Uno entre erre está restando: lo pasamos sumando, y el número pasa restando.".replace("erre", dice("r", "erre")),
            M(rf"\frac{{1}}{{r}}=100-{tex((t1 - 50.0) / fac, 4)}={tex(100.0 - (t1 - 50.0) / fac, 4)}"))\
        .di("El radio es uno entre ese número.", M(rf"r=\frac{{1}}{{{tex(100.0 - (t1 - 50.0) / fac, 4)}}}={tex(r50, 3)}\ \text{{m}}"))\
        .di("Por último la posición: del radio en línea recta despejamos equis.".replace("equis", dice("x", "equis")),
            M(r"r=r_1+b\,x\;\Rightarrow\;x=\frac{r-r_1}{b}"),
            M(rf"x=\frac{{{tex(r50, 3)}-0{{,}}0100}}{{{tex(b, 3)}}}={tex(x50, 3)}\ \text{{m}}"),
            R(rf"x={tex(x50 * 100, 3)}\ \text{{cm}}", "r4b"))
    L.paso("4) ¿Por qué no es en la mitad?")\
        .di("El gradiente es hache entre ka por el área: donde el área es chica, la temperatura cae rápido.".replace("hache entre ka", dice("H/(kA)", "hache entre ka")),
            M(r"\left|\frac{dT}{dx}\right|=\frac{H}{kA(x)}"))\
        .di("Cerca del extremo angosto está casi toda la caída: la mitad de la temperatura se pierde en el primer cuarto.",
            NOTA("El área en $x=0$ es nueve veces menor que en $x=L$: ahí está la mayor resistencia."))
    L.paso("5) Casos límite")\
        .di("Si los dos radios son iguales, sale la barra cilíndrica de la hoja.",
            M(r"r_1=r_2=r\;\Rightarrow\;H=\frac{\pi k r^2(T_1-T_2)}{L}=kA\,\frac{T_1-T_2}{L}"), F(r"H=kA\,\frac{T_H-T_C}{L}"))\
        .di("Si se invierte la barra, el producto erre uno por erre dos es el mismo: hache no cambia. Lo que cambia es el perfil de temperatura.".replace("erre uno por erre dos", dice("r₁r₂", "erre uno por erre dos")).replace("hache", dice("H", "hache")),
            R(r"H\ \text{no cambia}\ (r_1r_2=r_2r_1)", "r5"))
    receta_cono(L, r1, r2, lb)
    L.responde("3", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("3", rf"m={tex(mh, 3)}\ \text{{kg/h}}", mh, "kg/h")\
     .responde("4", rf"T(L/2)={tex(tmid, 3)}\ {C}", tmid, "°C").responde("4", rf"x={tex(x50 * 100, 3)}\ \text{{cm}}", x50 * 100, "cm")
    return L.usa(r"H=-kA\frac{dT}{dx}", r"H=\frac{dQ}{dt}", r"Q=\pm mL")


# ============================================================================ PR1-5
@leccion("PR1-5")
def pr1_5() -> Leccion:
    g = Gas(0.160, "mono")
    g.pone("A", p=100e3, v=4.00e-3)
    g.pone("B", p=200e3, v=8.00e-3)
    g.pone("C", p=100e3, v=8.00e-3)
    alfa = 100e3 / 4.00e-3
    rab = g.proceso("A", "B", "recta")
    rbc = g.proceso("B", "C", "isocorico")
    rca = g.proceso("C", "A", "isobarico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    L = _des("PR1-5", "Ciclo con P = αV, isocórico e isobárico (monoatómico)", "6.11", _f1(5, 4),
             "Un motor usa $0{,}160$ mol de gas monoatómico: $A\\to B$ con $P=\\alpha V$ (recta por el origen), $B\\to C$ isocórico, $C\\to A$ isobárico. $A$: $100$ kPa, $4{,}00$ L; $B$: $200$ kPa, $8{,}00$ L; $C$: $100$ kPa, $8{,}00$ L. 1) $\\alpha$ y temperaturas. 2) $W_{AB}$ con la integral. 3) Tabla de $Q$, $W$, $\\Delta U$. 4) Demuestre que en $A\\to B$ el gas absorbe calor en todo momento. 5) Eficiencia.",
             aviso="Nota editorial del PDF: el encabezado indica (20 pts), pero los incisos impresos suman 3 + 4 + 6 + 2 + 3 = 18 pts. Todos los cálculos aquí están verificados.",
             claves=["P = αV", "ciclo", "monoatómico", "práctica"],
             visual=visual_pv(g, [("A", "B", "recta"), ("B", "C", "isocorico"), ("C", "A", "isobarico")], uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un triángulo: una recta que pasa por el origen, una vertical y una horizontal.", DATO("0,160 mol"),
            TABLA(["Estado", "$P$ (kPa)", "$V$ (L)"], [["A", "$100$", "$4{,}00$"], ["B", "$200$", "$8{,}00$"], ["C", "$100$", "$8{,}00$"]]),
            LISTA(["Monoatómico: $C_V=\\frac{3}{2}R$"]))
    p = L.paso("1) α y las temperaturas")
    p.di("Alfa es presión entre volumen; sirve con A o con B.".replace("Alfa", dice("α", "Alfa")),
         M(rf"\alpha=\frac{{P_A}}{{V_A}}=\frac{{100\times10^3}}{{4{{,}}00\times10^{{-3}}}}={tex(alfa, 3)}\ \text{{Pa/m}}^3"), M(r"\frac{P_B}{V_B}=\frac{200\times10^3}{8{,}00\times10^{-3}}=2{,}50\times10^{7}\ \ \checkmark"))
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B")
    estado_T(p, g, "C")
    L.paso("2) Trabajo de A a B con la integral")\
        .di("Partimos del trabajo de la hoja, con pe igual a alfa ve.".replace("pe igual a alfa ve", dice("P = αV", "pe igual a alfa ve")), V(foco="AB"),
            F(r"W=\int_{V_1}^{V_2}p\,dV"), M(r"W_{AB}=\int_{V_A}^{V_B}\alpha V\,dV=\alpha\int_{V_A}^{V_B}V\,dV"))\
        .di("Alfa es constante y sale. Regla uno de la potencia: ve a la uno da ve al cuadrado entre dos.".replace("Alfa", dice("α", "Alfa")).replace("ve a la uno da ve al cuadrado entre dos", dice("V¹ → V²/2", "ve a la uno da ve al cuadrado entre dos")),
            M(r"\int V\,dV=\int V^1\,dV=\frac{V^{1+1}}{1+1}=\frac{V^2}{2}"))\
        .di("Arriba menos abajo.", M(r"W_{AB}=\alpha\Big[\frac{V^2}{2}\Big]_{V_A}^{V_B}=\frac{\alpha}{2}\left(V_B^2-V_A^2\right)"), NOTA("Regla 1 de la lección **T-MAT2**."))
    L.paso("2) Trabajo de A a B: números")\
        .di("Sustituimos.", M(rf"W_{{AB}}=\frac{{{tex(alfa, 3)}}}{{2}}\left[(8{{,}}00\times10^{{-3}})^2-(4{{,}}00\times10^{{-3}})^2\right]=({tex(alfa / 2, 3)})(4{{,}}80\times10^{{-5}})"))\
        .di(f"Da {nu(rab['W'], 'J', 3)}: es el área del trapecio bajo la recta.", M(rf"W_{{AB}}={tex(rab['W'], 3)}\ \text{{J}}", "wab", "verde"))
    L.paso("3) A→B: energía interna y calor")\
        .di("La energía interna con ce ve.".replace("ce ve", dice("C_V", "ce ve")), F(r"\Delta U=nC_V\,\Delta T"),
            M(rf"\Delta U_{{AB}}=(0{{,}}160)({tex(g.cv, 5)})({tex(g.t('B'), 5)}-{tex(g.t('A'), 4)})={tex(rab['U'], 4)}\ \text{{J}}"))\
        .di("El calor sale de la primera ley.", F(r"\Delta U=Q-W\;\Rightarrow\;Q=\Delta U+W"),
            M(rf"Q_{{AB}}={tex(rab['U'], 4)}+{tex(rab['W'], 3)}={tex(rab['Q'], 4)}\ \text{{J}}", color="verde"))
    explica_proceso(L.paso("3) B→C (volumen constante)"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("3) C→A (presión constante)"), g, "C", "A", "isobarico", foco="CA")
    L.paso("4) En A→B el gas absorbe calor todo el tiempo")\
        .di("Miramos un pedacito del proceso. El trabajo de ese pedacito:", V(foco="AB"), M(r"dW=P\,dV=\alpha V\,dV"))\
        .di("La energía interna de ese pedacito, con ce ve igual a tres medios de erre.".replace("ce ve", dice("C_V", "ce ve")),
            M(r"dU=nC_V\,dT=\frac{3}{2}R\,n\,dT=\frac{3}{2}\,d(nRT)"))\
        .di("Del gas ideal, ene erre te es pe ve; y en esta recta pe ve es alfa ve al cuadrado.".replace("ene erre te es pe ve", dice("nRT = PV", "ene erre te es pe ve")).replace("pe ve es alfa ve al cuadrado", dice("PV = αV²", "pe ve es alfa ve al cuadrado")),
            M(r"PV=(\alpha V)\,V=\alpha V^2\;\Rightarrow\;dU=\frac{3}{2}\,d\left(\alpha V^2\right)"))
    L.paso("4) Derivar y sumar")\
        .di("Derivada de una potencia: el dos baja a multiplicar y el exponente queda en uno.", M(r"d\left(V^2\right)=2V^{2-1}\,dV=2V\,dV"),
            NOTA("Derivada de una potencia: lección **T-MAT1**, último paso."))\
        .di("Alfa es un número fijo: sale afuera. Ponemos lo que acabamos de derivar.",
            M(r"dU=\frac{3}{2}\,d\left(\alpha V^2\right)=\frac{3}{2}\,\alpha\,d\left(V^2\right)=\frac{3}{2}\,\alpha\,(2V\,dV)"))\
        .di("Tres medios por dos da tres.", M(r"dU=\frac{3\cdot 2}{2}\,\alpha V\,dV=3\alpha V\,dV"))
    L.paso("4) Sumar con la primera ley")\
        .di("Despejamos el calor de la primera ley de la hoja, para un pedacito del proceso.", F(r"\Delta U=Q-W\;\Rightarrow\;dQ=dU+dW"))\
        .di("El trabajo de un pedacito es presión por el pedacito de volumen; aquí la presión es alfa por ve.".replace("alfa por ve", dice("αV", "alfa por ve")),
            M(r"dW=p\,dV=\alpha V\,dV"))\
        .di("Sumamos: tres alfa ve de ve más un alfa ve de ve dan cuatro. Es positivo mientras el volumen crece.".replace("tres alfa ve de ve más un alfa ve de ve", dice("3αV dV + αV dV", "tres alfa ve de ve más un alfa ve de ve")),
            M(r"dQ=3\alpha V\,dV+\alpha V\,dV=4\alpha V\,dV>0"),
            R(r"dQ=4\alpha V\,dV>0\ \text{en todo }A\to B", "r4"))
    c = cierre_ciclo(L, res, titulo="3) Tabla del ciclo y 5) eficiencia", id_e="r5")
    L.responde("1", rf"\alpha={tex(alfa, 3)}\ \text{{Pa/m}}^3", alfa, "Pa/m³").responde("1", rf"T_B={tex(g.t('B'), 4)}\ \text{{K}}", g.t("B"), "K")\
     .responde("2", rf"W_{{AB}}={tex(rab['W'], 3)}\ \text{{J}}", rab["W"], "J").responde("5", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.usa(r"pV=nRT", r"W=\int p\,dV", r"\Delta U=nC_V\Delta T", r"e=\frac{W}{Q_H}")


# ============================================================================ PR1-6
@leccion("PR1-6")
def pr1_6() -> Leccion:
    g = Gas(2.00, "dia")
    g.pone("A", v=0.0100, t=400.0)
    g.pone("B", v=0.0300, t=400.0)
    tc = 400.0 * 3.0 ** (g.g - 1.0)
    g.pone("C", v=0.0100, t=tc)
    pb, pc = g.p("B"), g.p("C")
    rab = g.proceso("A", "B", "isotermico")
    rbc = g.proceso("B", "C", "adiabatico")
    rca = g.proceso("C", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    wbc = (pb * 0.0300 - pc * 0.0100) / (g.g - 1.0)
    L = _des("PR1-6", "Ciclo isotérmico, adiabático e isocórico (2 mol diatómico)", "6.11", _f1(6, 5),
             "Dos moles de gas diatómico ($C_V=5R/2$, $\\gamma=1{,}40$): $A\\to B$ expansión isotérmica a $400$ K de $10{,}0$ L a $30{,}0$ L; $B\\to C$ compresión adiabática reversible hasta $10{,}0$ L; $C\\to A$ enfriamiento isocórico. 1) $P_A$, $P_B$, $T_C$, $P_C$ y sentido del ciclo. 2) $W_{AB}$ y $W_{BC}$ integrando; demuestre $W_{BC}=(P_BV_B-P_CV_C)/(\\gamma-1)$. 3) Tabla. 4) Verificación e interpretación de $W_{\\text{neto}}$.",
             claves=["isotérmico", "adiabático", "diatómico", "práctica"],
             visual=visual_pv(g, [("A", "B", "isotermico"), ("B", "C", "adiabatico"), ("C", "A", "isocorico")], uv="L", up="kPa", fv=1e3, fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Expandir a temperatura constante, comprimir sin calor y enfriar a volumen constante.", DATO("2,00 mol"),
            LISTA(["$n=2{,}00$ mol, $C_V=\\frac{5}{2}R$, $\\gamma=1{,}40$", "$A$: $10{,}0$ L, $400$ K · $B$: $30{,}0$ L, $400$ K", "$C$: $10{,}0$ L, sobre la adiabática desde $B$"]))
    p = L.paso("1) Presiones de A y B")
    from .ayudas import estado_p
    estado_p(p, g, "A", primero=True, voz="Despejamos la presión del gas ideal y la calculamos en A.")
    estado_p(p, g, "B")
    L.paso("1) Estado C con la adiabática: temperatura")\
        .di("La hoja da la relación de temperatura y volumen en la adiabática.", V(foco="BC"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("La igualamos entre B y C.", M(r"T_CV_C^{\gamma-1}=T_BV_B^{\gamma-1}"))\
        .di("Dividimos los dos lados entre el volumen de C elevado a gama menos uno, para dejar sola la temperatura de C.",
            M(r"T_C=T_B\,\frac{V_B^{\gamma-1}}{V_C^{\gamma-1}}"))\
        .di("Dos potencias con el mismo exponente se dividen dentro de un solo paréntesis.", M(r"T_C=T_B\left(\frac{V_B}{V_C}\right)^{\gamma-1}"))\
        .di("Sustituimos: treinta litros entre diez da tres, y gama menos uno es cero coma cuarenta.",
            M(rf"T_C=400\,(3)^{{0{{,}}40}}=400\,({tex(3.0 ** 0.4, 5)})={tex(tc, 4)}\ \text{{K}}"))
    L.paso("1) Estado C con la adiabática: presión")\
        .di("Ahora la presión, con la otra relación adiabática de la hoja. La igualamos entre B y C.", F(r"pV^\gamma=\text{cte}"),
            M(r"P_CV_C^{\gamma}=P_BV_B^{\gamma}"))\
        .di("Dividimos los dos lados entre el volumen de C a la gama, y juntamos las potencias en un paréntesis.",
            M(r"P_C=P_B\,\frac{V_B^{\gamma}}{V_C^{\gamma}}=P_B\left(\frac{V_B}{V_C}\right)^{\gamma}"))\
        .di("Sustituimos: tres elevado a uno coma cuarenta, en la calculadora.",
            M(rf"P_C=({tex(pb, 4)})(3)^{{1{{,}}40}}=({tex(pb, 4)})({tex(3.0 ** 1.4, 4)})={tex(pc, 4)}\ \text{{Pa}}"))
    L.paso("1) Sentido del ciclo")\
        .di("A y C tienen el mismo volumen, pero C tiene más presión: la adiabática pasa por encima de la isoterma.", V(foco="ciclo"),
            M(rf"P_C={tex(pc, 3)}\ \text{{Pa}}>P_A={tex(g.p('A'), 3)}\ \text{{Pa}}"))\
        .di("Se va por abajo hacia la derecha y se vuelve por arriba: el ciclo se recorre en sentido antihorario.", R(r"\text{Sentido antihorario}", "r1"))
    L.paso("2) Trabajo de A a B integrando")\
        .di("Partimos del trabajo de la hoja con pe igual a ene erre te entre ve.".replace("pe igual a ene erre te entre ve", dice("P = nRT/V", "pe igual a ene erre te entre ve")), V(foco="AB"),
            F(r"W=\int_{V_1}^{V_2}p\,dV"), M(r"W_{AB}=\int_{V_A}^{V_B}\frac{nRT}{V}\,dV=nRT\int_{V_A}^{V_B}\frac{dV}{V}"))\
        .di("La temperatura no cambia, así que ene erre te sale de la integral. Regla dos: uno entre ve da logaritmo.".replace("ene erre te", dice("nRT", "ene erre te")).replace("entre ve", "entre " + dice("V", "ve")),
            M(r"W_{AB}=nRT\Big[\ln V\Big]_{V_A}^{V_B}=nRT\left(\ln V_B-\ln V_A\right)=nRT\ln\left(\frac{V_B}{V_A}\right)"),
            NOTA("Regla 2 de **T-MAT2** y la resta de logaritmos de **T-MAT3**. Es la casilla isotérmica de la hoja."))\
        .di("Sustituimos. Con cinco cifras en cada factor, para que el producto salga justo.", M(rf"W_{{AB}}=(2{{,}}00)(8{{,}}314)(400)\ln 3=({tex(2.0 * 8.314 * 400.0, 5)})({tex(math.log(3.0), 5)})"),
            M(rf"W_{{AB}}={tex(rab['W'], 4)}\ \text{{J}}", "wab", "verde"))
    L.paso("2) Trabajo de B a C integrando")\
        .di("En la adiabática, la casilla de la hoja dice que pe por ve a la gama no cambia: vale lo mismo que en B.".replace("pe por ve a la gama", dice("PVᵞ", "pe por ve a la gama")), V(foco="BC"),
            F(r"pV^{\gamma}=\text{constante}"), M(r"PV^{\gamma}=P_BV_B^{\gamma}"))\
        .di("Dividimos los dos lados entre ve a la gama para dejar sola la presión.".replace("ve a la gama", dice("Vᵞ", "ve a la gama")),
            M(r"P=\frac{P_BV_B^{\gamma}}{V^{\gamma}}"))\
        .di("Dividir entre ve a la gama es multiplicar por ve a la menos gama; así se integra con la regla de la potencia.".replace("ve a la gama", dice("Vᵞ", "ve a la gama")).replace("ve a la menos gama", dice("V⁻ᵞ", "ve a la menos gama")),
            M(r"P=P_BV_B^{\gamma}\,V^{-\gamma}"))\
        .di("La ponemos en el trabajo de la hoja. La constante sale de la integral.", F(r"W=\int_{V_1}^{V_2}p\,dV"),
            M(r"W_{BC}=\int_{V_B}^{V_C}P_BV_B^{\gamma}\,V^{-\gamma}\,dV=P_BV_B^{\gamma}\int_{V_B}^{V_C}V^{-\gamma}\,dV"))\
        .di("Regla uno de la potencia con exponente menos gama: se suma uno y se divide entre el nuevo.".replace("gama", dice("γ", "gama")),
            M(r"\int V^{-\gamma}\,dV=\frac{V^{-\gamma+1}}{-\gamma+1}=\frac{V^{1-\gamma}}{1-\gamma}"))\
        .di("Arriba menos abajo.", M(r"W_{BC}=P_BV_B^{\gamma}\Big[\frac{V^{1-\gamma}}{1-\gamma}\Big]_{V_B}^{V_C}=\frac{P_BV_B^{\gamma}V_C^{1-\gamma}-P_BV_B^{\gamma}V_B^{1-\gamma}}{1-\gamma}"))
    L.paso("2) Trabajo de B a C: simplificar")\
        .di("Al multiplicar potencias de la misma base, los exponentes se suman.", M(r"V^{\gamma}\cdot V^{1-\gamma}=V^{\gamma+1-\gamma}=V^{1}=V"))\
        .di("La constante de la adiabática es la misma en B y en C, así que cada término queda como presión por volumen.",
            M(r"P_BV_B^{\gamma}V_C^{1-\gamma}=P_CV_C^{\gamma}V_C^{1-\gamma}=P_CV_C\qquad P_BV_B^{\gamma}V_B^{1-\gamma}=P_BV_B"))\
        .di("Queda la fórmula pedida, que es la casilla adiabática de la hoja.", M(r"W_{BC}=\frac{P_CV_C-P_BV_B}{1-\gamma}=\frac{P_BV_B-P_CV_C}{\gamma-1}"),
            F(r"W=\frac{1}{\gamma-1}\left(p_1V_1-p_2V_2\right)"))
    pbv, pcv = float(f"{pb * 0.03:.6g}"), float(f"{pc * 0.01:.6g}")
    L.paso("2) Trabajo de B a C: números")\
        .di("Sustituimos. Llevamos seis cifras en las presiones: la resta de arriba es de dos números parecidos y no debe perder exactitud.",
            M(rf"W_{{BC}}=\frac{{({tex(pb, 6)})(0{{,}}0300)-({tex(pc, 6)})(0{{,}}0100)}}{{0{{,}}40}}"))\
        .di("Multiplicamos cada presión por su volumen.", M(rf"=\frac{{{tex(pbv, 6)}-{tex(pcv, 6)}}}{{0{{,}}40}}"))\
        .di("Restamos arriba.", M(rf"=\frac{{{tex(pbv - pcv, 5)}}}{{0{{,}}40}}"))\
        .di(f"Dividimos entre cero coma cuarenta. Es negativo: el gas es comprimido. Da {nu(wbc, 'J', 4)}.", M(rf"W_{{BC}}={tex(wbc, 4)}\ \text{{J}}", "wbc", "verde"))
    L.paso("3) A→B y B→C en la tabla")\
        .di("En la isoterma no cambia la energía interna: el calor es el trabajo.", V(foco="AB"), F(r"\Delta U=Q-W"),
            M(rf"\Delta U_{{AB}}=0\;\Rightarrow\;Q_{{AB}}=W_{{AB}}={tex(rab['Q'], 4)}\ \text{{J}}"))\
        .di("En la adiabática no hay calor: la energía interna es menos el trabajo.", V(foco="BC"),
            M(rf"Q_{{BC}}=0\;\Rightarrow\;\Delta U_{{BC}}=-W_{{BC}}={tex(-wbc, 4)}\ \text{{J}}"))\
        .di("Revisión con ce ve.".replace("ce ve", dice("C_V", "ce ve")), F(r"\Delta U=nC_V\,\Delta T"),
            M(rf"\Delta U_{{BC}}=(2{{,}}00)({tex(g.cv, 5)})({tex(tc, dec=2)}-400)={tex(rbc['U'], 4)}\ \text{{J}}"))
    explica_proceso(L.paso("3) C→A (volumen constante)"), g, "C", "A", "isocorico", foco="CA")
    c = cierre_ciclo(L, res, titulo="3) y 4) Tabla y verificación del ciclo", solo_totales=True)
    L.paso("4) ¿Entrega trabajo o lo consume?")\
        .di("Como el ciclo se recorre en sentido antihorario, el trabajo neto sale negativo: consume trabajo y actúa como refrigerador o bomba de calor.", V(foco="ciclo"),
            M(rf"W_{{\text{{neto}}}}=W_{{AB}}+W_{{BC}}+W_{{CA}}={tex(c['W'], 4)}\ \text{{J}}<0"),
            R(rf"W_{{\text{{neto}}}}={tex(c['W'], 4)}\ \text{{J}}<0\;\Rightarrow\;\text{{consume trabajo}}", "r4"),
            OJO("No apliques $e=W/Q_H$ en un ciclo antihorario: este ciclo no es un motor, es un refrigerador o bomba de calor."))
    L.responde("1", rf"P_A={tex(g.p('A'), 4)}\ \text{{Pa}}", g.p("A"), "Pa").responde("1", rf"T_C={tex(tc, 4)}\ \text{{K}}", tc, "K")\
     .responde("1", rf"P_C={tex(pc, 4)}\ \text{{Pa}}", pc, "Pa").responde("2", rf"W_{{AB}}={tex(rab['W'], 4)}\ \text{{J}}", rab["W"], "J")\
     .responde("2", rf"W_{{BC}}={tex(wbc, 4)}\ \text{{J}}", wbc, "J").responde("4", rf"W_{{\text{{neto}}}}={tex(c['W'], 4)}\ \text{{J}}", c["W"], "J")
    return L.usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"pV^\gamma=\text{cte}", r"W=nRT\ln\frac{V_2}{V_1}", r"W=\frac{p_1V_1-p_2V_2}{\gamma-1}")


# ============================================================================ PR2-S1
@leccion("PR2-S1")
def pr2_s1() -> Leccion:
    L = _sel("PR2-S1", "Calor específico en J/(kg·°F)", "4.5", _f2("S1", 1),
             "Un líquido posee un calor específico de $1800$ J/(kg·K). Su valor en J/(kg·°F) es:",
             {"A": "3240", "B": "1000", "C": "1800", "D": "1768"}, "B", claves=["calor específico", "Fahrenheit"])
    L.paso("Cuánto vale un cambio de un grado")\
        .di("De la hoja, la conversión a Fahrenheit. Restamos la de dos temperaturas: el treinta y dos se cancela.", DATO("J/(kg·°F)"),
            F(r"T_{F}=\frac{9}{5}T_{C}+32"), M(r"T_{F2}-T_{F1}=\left(\frac{9}{5}T_{C2}+32\right)-\left(\frac{9}{5}T_{C1}+32\right)=\frac{9}{5}\left(T_{C2}-T_{C1}\right)"),
            M(r"\Delta T_{F}=\frac{9}{5}\,\Delta T_{C}=1{,}8\,\Delta T_{C}"))\
        .di("Y un cambio de un kelvin es un cambio de un grado Celsius.", F(r"T_K=T_C+273{,}15"),
            M(r"\Delta T_K=\Delta T_C\;\Rightarrow\;\Delta T=1\ \text{K}\ \text{equivale a}\ 1{,}8\ ^\circ\text{F}"))
    L.paso("Cambiar la unidad")\
        .di("En la unidad del calor específico, cambiamos el kelvin por los uno coma ocho grados Fahrenheit que vale.",
            M(r"c=1800\ \frac{\text{J}}{\text{kg}\cdot\text{K}}=1800\ \frac{\text{J}}{\text{kg}\cdot(1{,}8\ ^\circ\text{F})}"))\
        .di("El uno coma ocho de abajo divide al mil ochocientos.", M(r"c=\frac{1800}{1{,}8}\ \frac{\text{J}}{\text{kg}\cdot^\circ\text{F}}"), R(r"c=1000\ \text{J/(kg·°F)}"))
    revisar_opciones(L, {
        "A": ("La A multiplicó por uno coma ocho en vez de dividir.", "$1800\\times1{,}8=3240$: al revés."),
        "C": ("La C no convierte nada.", "Un kelvin no es un grado Fahrenheit."),
        "D": ("La D le restó treinta y dos, pero en un cambio de temperatura el treinta y dos se cancela.", "$1800-32$ no tiene sentido."),
        "B": ("La B es la correcta: mil.", "$1800/1{,}8=1000$."),
    })
    return L.usa(r"T_F=\frac{9}{5}T_C+32", r"T_K=T_C+273{,}15")


# ============================================================================ PR2-S2
@leccion("PR2-S2")
def pr2_s2() -> Leccion:
    L = _sel("PR2-S2", "Máquina que libera el 65 % del calor recibido", "6.14", _f2("S2", 1),
             "Una máquina térmica libera al depósito frío el $65\\ \\%$ del calor recibido. Su eficiencia es:",
             {"A": "65 %", "B": "35 %", "C": "100 %", "D": "53,8 %"}, "B", claves=["eficiencia", "máquina térmica"])
    L.paso("Eficiencia desde la hoja")\
        .di("Partimos de la eficiencia de la hoja, escrita con los calores.", DATO("65 %"), F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_C}{Q_H}\right|"))\
        .di("El calor que se tira es el sesenta y cinco por ciento del que entra.", M(r"|Q_C|=0{,}65\,Q_H\;\Rightarrow\;\left|\frac{Q_C}{Q_H}\right|=0{,}65"))\
        .di("Sustituimos.", M(r"e=1-0{,}65=0{,}35"), R(r"e=35\ \%"))
    revisar_opciones(L, {
        "A": ("La A es el calor que se tira, no la eficiencia.", "$0{,}65$ es $|Q_C|/Q_H$."),
        "C": ("La C es imposible: siempre se cede algo de calor al foco frío.", "Segunda ley: $e<1$."),
        "D": ("La D divide treinta y cinco entre sesenta y cinco: eso es trabajo entre calor cedido.", "$W/|Q_C|$, no $W/Q_H$."),
        "B": ("La B es la correcta: treinta y cinco por ciento.", "$e=1-0{,}65$."),
    })
    return L.usa(r"e=1-\left|\frac{Q_C}{Q_H}\right|")


# ============================================================================ PR2-S3
@leccion("PR2-S3")
def pr2_s3() -> Leccion:
    L = _sel("PR2-S3", "Orificio en una placa que se calienta", "4.2", _f2("S3", 1),
             "Una placa metálica con un orificio circular se calienta uniformemente. El diámetro del orificio:",
             {"A": "Disminuye", "B": "Permanece constante", "C": "Aumenta", "D": "Depende de la posición"}, "C",
             claves=["dilatación", "orificio"], visual={"tipo": "dilatacion", "caso": "anillo", "L0": 1.0, "a": 2e-5, "T0": 20, "dT": 300, "nombre": "placa con orificio"})
    L.paso("Toda distancia de la placa crece igual")\
        .di("La dilatación lineal de la hoja vale para cualquier distancia dentro del material, también el diámetro del hueco.", DATO("orificio circular"),
            F(r"\Delta L=\alpha L_0\,\Delta T"), M(r"\Delta D=\alpha D_0\,\Delta T"))\
        .di("Si la temperatura sube, el cambio es positivo: el hueco crece como si estuviera lleno del mismo metal.",
            M(r"\Delta T>0\;\Rightarrow\;\Delta D>0"), R(r"\text{El diámetro aumenta}"))
    revisar_opciones(L, {
        "A": ("La A es el error típico: se piensa que el metal invade el hueco, pero todas las distancias crecen.", "Es una ampliación fotográfica."),
        "B": ("La B olvida que el borde del hueco también es material que se dilata.", "$\\Delta D=\\alpha D_0\\Delta T\\neq0$."),
        "D": ("La D no aplica: el calentamiento es uniforme, todo crece en la misma proporción.", "Mismo $\\alpha$ y mismo $\\Delta T$."),
        "C": ("La C es la correcta: el diámetro aumenta.", "$\\Delta D>0$."),
    })
    return L.usa(r"\Delta L=\alpha L_0\Delta T")


# ============================================================================ PR2-S4
@leccion("PR2-S4")
def pr2_s4() -> Leccion:
    L = _sel("PR2-S4", "Triplicar el volumen y duplicar la temperatura", "6.3", _f2("S4", 1),
             "Un gas ideal de cantidad fija triplica su volumen y duplica su temperatura absoluta. La presión final es:",
             {"A": "2/3 de la inicial", "B": "6 veces la inicial", "C": "3/2 de la inicial", "D": "1/6 de la inicial"}, "A",
             claves=["gas ideal", "razones"])
    L.paso("Despejar la presión del gas ideal")\
        .di("Partimos de la ley del gas ideal de la hoja; ene y erre no cambian.", DATO("triplica su volumen"), F(r"pV=nRT\;\Rightarrow\;p=\frac{nRT}{V}"))\
        .di("Dividimos final entre inicial: se cancelan ene y erre.", M(r"\frac{p_f}{p_i}=\frac{nRT_f/V_f}{nRT_i/V_i}=\frac{T_f}{T_i}\cdot\frac{V_i}{V_f}"))\
        .di("La temperatura se duplica y el volumen se triplica.", M(r"\frac{p_f}{p_i}=(2)\left(\frac{1}{3}\right)=\frac{2}{3}"), R(r"p_f=\frac{2}{3}\,p_i"))
    revisar_opciones(L, {
        "B": ("La B multiplicó las dos razones en vez de dividir por el volumen.", "$2\\times3=6$: el volumen va abajo."),
        "C": ("La C invirtió la razón.", "$3/2$ es $p_i/p_f$."),
        "D": ("La D dividió entre los dos.", "La temperatura multiplica, el volumen divide."),
        "A": ("La A es la correcta: dos tercios.", "$p_f/p_i=2/3$."),
    })
    return L.usa(r"pV=nRT")


# ============================================================================ PR2-S5
@leccion("PR2-S5")
def pr2_s5() -> Leccion:
    L = _sel("PR2-S5", "Dos barras con k₁ = 4k₂ entre los mismos depósitos", "5.2", _f2("S5", 2),
             "Dos barras de igual longitud y sección conectan independientemente los mismos depósitos térmicos. La conductividad de la primera es cuatro veces la de la segunda. En régimen estacionario:",
             {"A": "Ambas transfieren igual potencia", "B": "La segunda transfiere cuatro veces más", "C": "La primera transfiere dieciséis veces más", "D": "La primera transfiere cuatro veces más."}, "D",
             claves=["conducción", "conductividad"])
    L.paso("Fourier en cada barra")\
        .di("Usamos la ley de Fourier de la hoja. Las dos barras tienen la misma área, el mismo largo y la misma diferencia de temperatura.", DATO("cuatro veces"),
            F(r"H=kA\,\frac{T_H-T_C}{L}"))\
        .di("Dividimos: todo se cancela menos las conductividades.", M(r"\frac{H_1}{H_2}=\frac{k_1A\,\Delta T/L}{k_2A\,\Delta T/L}=\frac{k_1}{k_2}=4"),
            R(r"H_1=4H_2"))
    revisar_opciones(L, {
        "A": ("La A ignora que el flujo es proporcional a la conductividad.", "$H\\propto k$."),
        "B": ("La B lo dice al revés.", "La primera conduce mejor."),
        "C": ("La C elevó al cuadrado sin motivo.", "$H$ es lineal en $k$."),
        "D": ("La D es la correcta: cuatro veces más.", "$H_1/H_2=k_1/k_2=4$."),
    })
    return L.usa(r"H=kA\frac{\Delta T}{L}")


# ============================================================================ PR2-S6
@leccion("PR2-S6")
def pr2_s6() -> Leccion:
    L = _sel("PR2-S6", "Rapidez cuadrática media con m₁ = 4m₂", "6.5", _f2("S6", 2),
             "Dos gases a igual temperatura tienen moléculas de masas $m_1=4m_2$. La relación de rapideces cuadráticas medias es:",
             {"A": "v₁ = 4v₂", "B": "v₁ = 2v₂", "C": "v₁ = v₂/2", "D": "v₁ = v₂"}, "C", claves=["v rms", "teoría cinética"])
    L.paso("La rapidez cuadrática media de la hoja")\
        .di("Partimos de la casilla de la hoja: depende de la temperatura y de la masa de cada molécula.", DATO("igual temperatura"),
            F(r"v_{\text{rms}}=\sqrt{\frac{3k_BT}{m}}"))\
        .di("Dividimos: la temperatura es la misma y se cancela.", M(r"\frac{v_1}{v_2}=\sqrt{\frac{3k_BT/m_1}{3k_BT/m_2}}=\sqrt{\frac{m_2}{m_1}}"))\
        .di("Con eme uno igual a cuatro eme dos.".replace("eme uno igual a cuatro eme dos", dice("m₁ = 4m₂", "eme uno igual a cuatro eme dos")),
            M(r"\frac{v_1}{v_2}=\sqrt{\frac{m_2}{4m_2}}=\sqrt{\frac{1}{4}}=\frac{1}{2}"), R(r"v_1=\frac{v_2}{2}"))
    revisar_opciones(L, {
        "A": ("La A olvida la raíz y además lo pone al revés.", "Más masa, menos rapidez."),
        "B": ("La B tiene la raíz pero al revés: la molécula pesada es la lenta.", "$v\\propto1/\\sqrt{m}$."),
        "D": ("La D confunde rapidez con energía cinética media, que sí es igual.", "$\\frac{1}{2}m(v^2)_{\\text{med}}=\\frac{3}{2}k_BT$ igual, $v$ no."),
        "C": ("La C es la correcta: la mitad.", "$\\sqrt{1/4}=1/2$."),
    })
    return L.usa(r"v_{rms}=\sqrt{\frac{3k_BT}{m}}")


# ============================================================================ PR2-S7
@leccion("PR2-S7")
def pr2_s7() -> Leccion:
    e8 = 1 - 1 / 8 ** 0.4
    e10 = 1 - 1 / 10 ** 0.4
    L = _sel("PR2-S7", "Otto con mayor razón de compresión", "6.14", _f2("S7", 2),
             "En un ciclo Otto ideal se aumenta la razón de compresión sin cambiar el gas. La eficiencia:",
             {"A": "Aumenta", "B": "Disminuye", "C": "No cambia", "D": "Alcanza necesariamente 100 %"}, "A", claves=["Otto", "razón de compresión"])
    L.paso("La eficiencia de Otto de la hoja")\
        .di("La hoja trae la eficiencia de Otto en función de la razón de compresión.", DATO("razón de compresión"), F(r"e_{\text{otto}}=1-\frac{1}{r^{\gamma-1}}"))\
        .di("Si erre crece, erre a la gama menos uno crece, la fracción se achica y la eficiencia sube.".replace("Si erre crece, erre a la gama menos uno crece", "Si la razón crece, su potencia crece"),
            M(r"r\uparrow\;\Rightarrow\;r^{\gamma-1}\uparrow\;\Rightarrow\;\frac{1}{r^{\gamma-1}}\downarrow\;\Rightarrow\;e\uparrow"))\
        .di("Un ejemplo con gama uno coma cuatro. Primero con erre igual a ocho.".replace("gama", dice("γ", "gama")).replace("erre igual a ocho", dice("r = 8", "erre igual a ocho")),
            M(rf"r=8:\ e=1-\frac{{1}}{{8^{{0{{,}}40}}}}={tex(e8, 3)}"))\
        .di("Ahora con erre igual a diez.".replace("erre igual a diez", dice("r = 10", "erre igual a diez")),
            M(rf"r=10:\ e=1-\frac{{1}}{{10^{{0{{,}}40}}}}={tex(e10, 3)}"))\
        .di("Con más compresión, la eficiencia sube.", R(r"\text{La eficiencia aumenta}"))
    revisar_opciones(L, {
        "B": ("La B lo dice al revés.", "Más compresión, más eficiencia."),
        "C": ("La C olvida que la eficiencia depende de la razón.", "$e=1-1/r^{\\gamma-1}$."),
        "D": ("La D es imposible: haría falta una razón infinita.", "$1/r^{\\gamma-1}>0$ siempre."),
        "A": ("La A es la correcta: aumenta.", "$r\\uparrow\\Rightarrow e\\uparrow$."),
    })
    return L.usa(r"e=1-\frac{1}{r^{\gamma-1}}")


# ============================================================================ PR2-S8
@leccion("PR2-S8")
def pr2_s8() -> Leccion:
    L = _sel("PR2-S8", "Compresión adiabática con trabajo sobre el gas", "6.8", _f2("S8", 2),
             "En una compresión adiabática de un gas ideal con trabajo positivo realizado sobre él:",
             {"A": "Disminuye su energía interna", "B": "La temperatura no cambia", "C": "Absorbe calor", "D": "Aumenta su energía interna"}, "D",
             claves=["adiabática", "primera ley"])
    L.paso("Primera ley sin calor")\
        .di("Partimos de la primera ley de la hoja. Adiabático: no entra calor. Se le hace trabajo al gas: el trabajo del gas es negativo.", DATO("compresión adiabática"),
            F(r"\Delta U=Q-W"), M(r"Q=0\qquad W<0"))\
        .di("Sustituimos: el cambio de energía interna es positivo.", M(r"\Delta U=0-W=-W>0"))\
        .di("Y como la energía interna va con la temperatura, el gas se calienta.", F(r"\Delta U=nC_V\,\Delta T"), M(r"\Delta U>0\;\Rightarrow\;\Delta T>0"),
            R(r"\text{Aumenta su energía interna}"))
    revisar_opciones(L, {
        "A": ("La A lo dice al revés: el trabajo que recibe se queda como energía interna.", "$\\Delta U=-W>0$."),
        "B": ("La B describe una isoterma, no una adiabática.", "$\\Delta T>0$."),
        "C": ("La C contradice la palabra adiabática.", "$Q=0$."),
        "D": ("La D es la correcta.", "$\\Delta U=-W>0$."),
    })
    return L.usa(r"\Delta U=Q-W")


# ============================================================================ PR2-S9
@leccion("PR2-S9")
def pr2_s9() -> Leccion:
    L = _sel("PR2-S9", "Dos cuerpos en equilibrio en un recipiente aislado", "4.6", _f2("S9", 2),
             "Dos cuerpos alcanzan equilibrio térmico en un recipiente aislado de capacidad calorífica despreciable, sin cambios de fase:",
             {"A": "Tienen iguales cambios de temperatura", "B": "El calor cedido por uno iguala el absorbido por el otro",
              "C": "Tienen igual energía interna final", "D": "El más masivo siempre cambia menos su temperatura"}, "B",
             claves=["equilibrio térmico", "conservación de energía"])
    L.paso("Conservación de la energía")\
        .di("Aislado: nada entra ni sale y no hay trabajo. Toda la energía que pierde uno la gana el otro.", DATO("aislado"), M(r"Q_1+Q_2=0\;\Rightarrow\;Q_1=-Q_2"))\
        .di("Con el calor sensible de la hoja, eso no obliga a cambios de temperatura iguales.", F(r"Q=mc\,\Delta T"),
            M(r"m_1c_1\,\Delta T_1=-m_2c_2\,\Delta T_2"), R(r"Q_{\text{cedido}}=Q_{\text{absorbido}}"))
    revisar_opciones(L, {
        "A": ("La A solo pasa si los dos tienen el mismo producto eme ce.".replace("eme ce", "masa por calor específico"), "Hace falta $m_1c_1=m_2c_2$."),
        "C": ("La C confunde temperatura con energía interna: igualan la temperatura, no la energía.", "$U$ depende de la masa y del material."),
        "D": ("La D olvida el calor específico: manda el producto masa por calor específico.", "Masivo con $c$ chico puede cambiar más."),
        "B": ("La B es la correcta.", "$Q_1=-Q_2$."),
    })
    return L.usa(r"Q=mc\Delta T")


# ============================================================================ PR2-S10
@leccion("PR2-S10")
def pr2_s10() -> Leccion:
    L = _sel("PR2-S10", "Radiación a 300 K y a 600 K", "5.8", _f2("S10", 3),
             "Dos superficies de igual área y emisividad están a $300$ K y $600$ K. La potencia emitida por la segunda es:",
             {"A": "2 veces la primera", "B": "4 veces la primera", "C": "16 veces la primera", "D": "8 veces la primera"}, "C",
             claves=["Stefan-Boltzmann", "cuarta potencia"])
    L.paso("Stefan-Boltzmann de la hoja")\
        .di("La potencia radiada va con la cuarta potencia de la temperatura absoluta.", DATO("600 K"), F(r"H=Ae\sigma T^4"))\
        .di("Dividimos: área, emisividad y sigma se cancelan.", M(r"\frac{H_2}{H_1}=\frac{Ae\sigma(600)^4}{Ae\sigma(300)^4}=\left(\frac{600}{300}\right)^4"))\
        .di("Dos a la cuarta es dieciséis.", M(r"\left(\frac{600}{300}\right)^4=2^4=16"), R(r"H_2=16\,H_1"))
    revisar_opciones(L, {
        "A": ("La A supone que es lineal en la temperatura.", "Es a la cuarta."),
        "B": ("La B elevó al cuadrado.", "$2^2=4$, falta."),
        "D": ("La D elevó al cubo.", "$2^3=8$, falta."),
        "C": ("La C es la correcta: dieciséis veces.", "$2^4=16$."),
    })
    return L.usa(r"H=Ae\sigma T^4")


# ============================================================================ PR2-S11
@leccion("PR2-S11")
def pr2_s11() -> Leccion:
    L = _sel("PR2-S11", "Mismo calor a volumen constante: mono o diatómico", "6.9", _f2("S11", 3),
             "Igual número de moles de gas monoatómico y diatómico reciben igual calor a volumen constante, con vibraciones inactivas y rotaciones activas:",
             {"A": "El monoatómico aumenta más su temperatura", "B": "El diatómico aumenta más su temperatura", "C": "Aumentan igual", "D": "Ninguno cambia"}, "A",
             claves=["capacidad calorífica", "C_V"])
    L.paso("Calor a volumen constante")\
        .di("A volumen constante el calor de la hoja va con ce ve. Despejamos el cambio de temperatura.".replace("ce ve", dice("C_V", "ce ve")), DATO("volumen constante"),
            F(r"Q=nC\,\Delta T"), M(r"\Delta T=\frac{Q}{nC_V}"))\
        .di("Las capacidades de la hoja.", F(r"C_V=\frac{3}{2}R"), F(r"C_V=\frac{5}{2}R"))\
        .di("Mismo calor y mismos moles: gana el de menor ce ve.".replace("ce ve", dice("C_V", "ce ve")),
            M(r"\frac{\Delta T_{\text{mono}}}{\Delta T_{\text{dia}}}=\frac{Q/(nC_{V,\text{mono}})}{Q/(nC_{V,\text{dia}})}=\frac{Q}{nC_{V,\text{mono}}}\cdot\frac{nC_{V,\text{dia}}}{Q}=\frac{C_{V,\text{dia}}}{C_{V,\text{mono}}}"),
            M(r"\frac{C_{V,\text{dia}}}{C_{V,\text{mono}}}=\frac{\frac{5}{2}R}{\frac{3}{2}R}=\frac{5}{3}>1"),
            R(r"\text{El monoatómico aumenta más su temperatura}"))
    revisar_opciones(L, {
        "B": ("La B lo dice al revés: el diatómico reparte energía en rotaciones.", "$C_{V,\\text{dia}}>C_{V,\\text{mono}}$."),
        "C": ("La C olvida que las capacidades son distintas.", "$3/2\\,R\\neq5/2\\,R$."),
        "D": ("La D es falsa: reciben calor a volumen constante, se calientan.", "$\\Delta T=Q/(nC_V)>0$."),
        "A": ("La A es la correcta.", "$\\Delta T$ cinco tercios mayor."),
    })
    return L.usa(r"C_V=\frac{3}{2}R", r"C_V=\frac{5}{2}R")


# ============================================================================ PR2-S12
@leccion("PR2-S12")
def pr2_s12() -> Leccion:
    L = _sel("PR2-S12", "Calor neto positivo en un ciclo", "6.11", _f2("S12", 3),
             "Un gas absorbe calor neto positivo en un ciclo completo. Entonces:",
             {"A": "Aumenta su energía interna", "B": "Aumenta su temperatura final", "C": "El trabajo neto es cero", "D": "Realiza trabajo neto positivo"}, "D",
             claves=["ciclo", "primera ley"])
    L.paso("Primera ley en un ciclo")\
        .di("Partimos de la primera ley de la hoja. En un ciclo el gas vuelve al estado inicial: su energía interna no cambia.", DATO("ciclo completo"),
            F(r"\Delta U=Q-W"), M(r"\Delta U_{\text{ciclo}}=0\;\Rightarrow\;W_{\text{neto}}=Q_{\text{neto}}"))\
        .di("Si el calor neto es positivo, el trabajo neto también.", M(r"Q_{\text{neto}}>0\;\Rightarrow\;W_{\text{neto}}>0"), R(r"\text{Realiza trabajo neto positivo}"))
    revisar_opciones(L, {
        "A": ("La A olvida que en un ciclo la energía interna vuelve a su valor.", "$\\Delta U_{\\text{ciclo}}=0$."),
        "B": ("La B igual: termina en el mismo estado, con la misma temperatura.", "Mismo estado inicial y final."),
        "C": ("La C contradice que el trabajo neto es igual al calor neto.", "$W=Q\\neq0$."),
        "D": ("La D es la correcta.", "$W_{\\text{neto}}=Q_{\\text{neto}}>0$."),
    })
    return L.usa(r"\Delta U=Q-W")


# ============================================================================ PR2-S13
@leccion("PR2-S13")
def pr2_s13() -> Leccion:
    L = _sel("PR2-S13", "Barra sujeta en ambos extremos que se enfría", "4.4", _f2("S13", 3),
             "Una barra inicialmente sin esfuerzo, rígidamente sujeta en ambos extremos, se enfría sin cambiar de longitud. Desarrolla:",
             {"A": "Compresión", "B": "Tensión", "C": "Solo esfuerzo cortante", "D": "Ningún esfuerzo"}, "B", claves=["esfuerzo térmico"])
    L.paso("Esfuerzo térmico de la hoja")\
        .di("La hoja trae el esfuerzo que aparece cuando no se deja dilatar o contraer a la barra.", DATO("se enfría"), F(r"\frac{F}{A}=-Y\alpha\,\Delta T"))\
        .di("Al enfriarse, delta te es negativo: el esfuerzo sale positivo.".replace("delta te", dice("ΔT", "delta te")), M(r"\Delta T<0\;\Rightarrow\;\frac{F}{A}=-Y\alpha\,(\text{negativo})>0"))\
        .di("Positivo es tracción: la barra quiere encogerse y los soportes la estiran.", R(r"\text{Tensión}"))
    revisar_opciones(L, {
        "A": ("La A pasa al calentarla: la barra quiere crecer y los soportes la empujan.", "$\\Delta T>0\\Rightarrow F/A<0$."),
        "C": ("La C no aplica: la fuerza es a lo largo de la barra.", "Esfuerzo normal, no cortante."),
        "D": ("La D olvida que los soportes impiden que se encoja.", "Sin poder contraerse, aparece esfuerzo."),
        "B": ("La B es la correcta: tensión.", "$F/A>0$."),
    })
    return L.usa(r"\frac{F}{A}=-Y\alpha\Delta T")


# ============================================================================ PR2-S14
@leccion("PR2-S14")
def pr2_s14() -> Leccion:
    L = _sel("PR2-S14", "Rendimiento de un refrigerador: 900 J con 300 J", "6.14", _f2("S14", 3),
             "Un refrigerador extrae $900$ J de un espacio frío con $300$ J de trabajo. Su coeficiente de rendimiento es:",
             {"A": "0,33", "B": "4,00", "C": "3,00", "D": "0,75"}, "C", claves=["refrigerador", "coeficiente de rendimiento"],
             visual={"tipo": "maquina", "modo": "refri", "QH": 1200.0, "QC": 900.0, "W": 300.0, "u": "J"})
    L.paso("Coeficiente de rendimiento de la hoja")\
        .di("Lo que sirve es el calor que se saca del espacio frío; lo que se paga es el trabajo.", DATO("900 J"), F(r"K=\frac{|Q_C|}{|W|}"))\
        .di("Sustituimos.", M(r"K=\frac{900}{300}=3{,}00"), R(r"K=3{,}00"))
    revisar_opciones(L, {
        "A": ("La A divide al revés.", "$300/900$."),
        "B": ("La B usa el calor que sale al ambiente, que es mil doscientos.", "$1200/300$ es otra cosa."),
        "D": ("La D divide el calor frío entre el caliente.", "$900/1200$."),
        "C": ("La C es la correcta: tres.", "$K=900/300$."),
    })
    return L.usa(r"K=\frac{|Q_C|}{|W|}")


# ============================================================================ PR2-D1
@leccion("PR2-D1")
def pr2_d1() -> Leccion:
    # Perfil ilustrativo (T₁ = 100, T₂ = 0, L = 1, R₂ = 0,4 R₁) con la T(x) de la parte c
    pts = [[round(i / 40, 4), round(100.0 - 100.0 * 0.4 * (i / 40) / (1.0 - 0.6 * (i / 40)), 2)] for i in range(41)]
    L = _des("PR2-D1", "Tronco de cono: A(x), H y T(x) en letras", "5.5", _f2("D1", 4),
             "Un sólido de longitud $L$, conductividad $k$ y radios extremos $R_1$ (en $x=0$, cara caliente a $T_1$) y $R_2$ (en $x=L$, a $T_2<T_1$), con superficie lateral aislada. a) Área transversal $A(x)$. b) Rapidez de transferencia de calor $H$. c) Distribución $T(x)$.",
             claves=["tronco de cono", "simbólico", "integral", "práctica"],
             visual={"tipo": "conduccion", "geo": "pared", "L": 1.0, "T1": 100, "T2": 0, "eje": "x", "uT": "°C", "forma": "1 - 0.6 * u", "pts": pts, "etA": "π R(x)²"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un cono truncado: la cara caliente es la ancha, con radio erre uno, y la fría es la angosta.".replace("erre uno", dice("R₁", "erre uno")), DATO("x = 0 en la cara caliente"),
            LISTA(["Largo $L$, conductividad $k$", "$x=0$: radio $R_1$, temperatura $T_1$", "$x=L$: radio $R_2$, temperatura $T_2<T_1$", "Lateral aislada: el calor va solo a lo largo de $x$"]))
    L.paso("a) Radio y área transversal")\
        .di("El radio cambia en línea recta de erre uno a erre dos.".replace("erre uno", dice("R₁", "erre uno")).replace("erre dos", dice("R₂", "erre dos")), V(foco="dx"),
            M(r"R(x)=R_1+\frac{R_2-R_1}{L}\,x"))\
        .di("La sección transversal en cada posición es un círculo de radio erre de equis.".replace("erre de equis", dice("R(x)", "erre de equis")),
            M(r"A(x)=\pi\,R(x)^2"), R(r"A(x)=\pi\left(R_1+\frac{R_2-R_1}{L}\,x\right)^2", "ra"))
    L.paso("b) Fourier y separar variables")\
        .di("Partimos de la ley de Fourier de la hoja. El área es la del círculo de radio erre de equis.".replace("erre de equis", dice("R(x)", "erre de equis")),
            F(r"H=-kA\,\frac{dT}{dx}"), M(r"H=-k\,\pi R(x)^2\,\frac{dT}{dx}"))\
        .di("Separamos, paso uno: multiplicamos los dos lados por de equis.".replace("de equis", dice("dx", "de equis")), M(r"H\,dx=-k\,\pi R^2\,dT"))\
        .di("Paso dos: dividimos los dos lados entre pi erre al cuadrado.".replace("pi erre al cuadrado", dice("πR²", "pi erre al cuadrado")),
            M(r"H\,\frac{dx}{\pi R^2}=-k\,dT"))\
        .di("Integramos toda la barra: equis de cero a ele, y la temperatura de te uno a te dos.".replace("equis de cero a ele", dice("x de 0 a L", "equis de cero a ele")).replace("te uno a te dos", dice("T₁ a T₂", "te uno a te dos")),
            M(r"H\int_0^L\frac{dx}{\pi R^2}=-k\int_{T_1}^{T_2}dT"))
    L.paso("b) Cambio de variable: de x al radio")\
        .di("El radio es una recta: si equis crece un pedacito, el radio crece la pendiente por ese pedacito.".replace("si equis", "si " + dice("x", "equis")),
            M(r"dR=\frac{R_2-R_1}{L}\,dx"))\
        .di("Despejamos de equis: lo que la multiplica pasa al otro lado dividiendo.".replace("de equis", dice("dx", "de equis")), M(r"dx=\frac{L}{R_2-R_1}\,dR"))\
        .di("Los límites también cambian: donde equis vale cero el radio es erre uno; donde vale ele, erre dos.".replace("equis vale cero", dice("x = 0", "equis vale cero")).replace("erre uno", dice("R₁", "erre uno")).replace("vale ele, erre dos", dice("x = L, R₂", "vale ele, erre dos")),
            M(r"x=0\;\Rightarrow\;R=R_1\qquad x=L\;\Rightarrow\;R=R_2"))
    L.paso("b) La integral de la izquierda")\
        .di("Cambiamos de equis por su valor en de erre, y lo que es constante sale de la integral.".replace("de equis", dice("dx", "de equis")).replace("de erre", dice("dR", "de erre")),
            M(r"\int_0^L\frac{dx}{\pi R^2}=\int_{R_1}^{R_2}\frac{1}{\pi R^2}\cdot\frac{L\,dR}{R_2-R_1}=\frac{L}{\pi(R_2-R_1)}\int_{R_1}^{R_2}R^{-2}\,dR"))\
        .di("Regla de la potencia con exponente menos dos: se le suma uno y se divide entre el exponente nuevo.",
            M(r"\int R^{-2}\,dR=\frac{R^{-2+1}}{-2+1}=\frac{R^{-1}}{-1}=-\frac{1}{R}"))\
        .di("Arriba menos abajo. Menos por menos da más.",
            M(r"\Big[-\frac{1}{R}\Big]_{R_1}^{R_2}=\left(-\frac{1}{R_2}\right)-\left(-\frac{1}{R_1}\right)=\frac{1}{R_1}-\frac{1}{R_2}"),
            NOTA("Reglas 1 y 3 de la lección **T-MAT2**."))
    L.paso("b) Simplificar la izquierda")\
        .di("Restamos las fracciones con el denominador común erre uno por erre dos.".replace("erre uno por erre dos", dice("R₁R₂", "erre uno por erre dos")),
            M(r"\frac{1}{R_1}-\frac{1}{R_2}=\frac{R_2}{R_1R_2}-\frac{R_1}{R_1R_2}=\frac{R_2-R_1}{R_1R_2}"))\
        .di("Lo ponemos en la integral: la resta de radios queda arriba y abajo, y se cancela.",
            M(r"\int_0^L\frac{dx}{\pi R^2}=\frac{L}{\pi(R_2-R_1)}\cdot\frac{R_2-R_1}{R_1R_2}=\frac{L}{\pi R_1R_2}"))
    L.paso("b) La derecha, juntar y despejar H")\
        .di("La de la derecha: sumar los pedacitos de temperatura da el cambio total.", M(r"-k\int_{T_1}^{T_2}dT=-k\,(T_2-T_1)=k\,(T_1-T_2)"))\
        .di("Juntamos los dos lados.", M(r"H\,\frac{L}{\pi R_1R_2}=k\,(T_1-T_2)"))\
        .di("Multiplicamos los dos lados por pi erre uno erre dos: así deja de dividir.".replace("pi erre uno erre dos", dice("πR₁R₂", "pi erre uno erre dos")),
            M(r"H\,L=\pi\,k\,R_1R_2\,(T_1-T_2)"))\
        .di("Y dividimos entre ele.".replace("ele", dice("L", "ele")), R(r"H=\frac{\pi k\,R_1R_2\,(T_1-T_2)}{L}", "rb"))
    L.paso("c) Temperatura a una distancia x")\
        .di("Ahora integramos solo desde cero hasta una equis cualquiera, donde la temperatura es te de equis.".replace("una equis", "una " + dice("x", "equis")).replace("te de equis", dice("T(x)", "te de equis")), V(foco="punto"),
            M(r"H\int_0^x\frac{dx}{\pi R^2}=-k\int_{T_1}^{T(x)}dT"))\
        .di("La izquierda sale igual que antes, pero arriba queda el radio en equis; la derecha es la resta de temperaturas.".replace("en equis", "en " + dice("x", "equis")),
            M(r"\frac{HL}{\pi(R_2-R_1)}\left(\frac{1}{R_1}-\frac{1}{R(x)}\right)=k\,(T_1-T(x))"))\
        .di("Dividimos los dos lados entre ka.".replace("ka", dice("k", "ka")), M(r"T_1-T(x)=\frac{HL}{\pi k(R_2-R_1)}\left(\frac{1}{R_1}-\frac{1}{R(x)}\right)"))\
        .di("Te de equis está restando: pasa sumando, y lo demás pasa restando.".replace("Te de equis", dice("T(x)", "Te de equis")),
            M(r"T(x)=T_1-\frac{HL}{\pi k(R_2-R_1)}\left(\frac{1}{R_1}-\frac{1}{R(x)}\right)"))
    L.paso("c) Poner el H de la parte b")\
        .di("Ponemos el hache de la parte be en el factor de adelante: pi, ka y ele se cancelan.".replace("hache", dice("H", "hache")).replace("parte be", dice("parte b", "parte be")),
            M(r"\frac{HL}{\pi k(R_2-R_1)}=\frac{\pi kR_1R_2(T_1-T_2)}{L}\cdot\frac{L}{\pi k(R_2-R_1)}=\frac{R_1R_2\,(T_1-T_2)}{R_2-R_1}"))\
        .di("El erre uno de arriba entra al paréntesis: erre uno por uno entre erre uno da uno.".replace("El erre uno", "El " + dice("R₁", "erre uno")),
            M(r"R_1\left(\frac{1}{R_1}-\frac{1}{R(x)}\right)=1-\frac{R_1}{R(x)}"))\
        .di("Así queda.", M(r"T(x)=T_1-(T_1-T_2)\,\frac{R_2}{R_2-R_1}\left(1-\frac{R_1}{R(x)}\right)"))
    L.paso("c) Simplificar y revisar")\
        .di("Juntamos el paréntesis en una fracción. Erre de equis menos erre uno es la pendiente por equis.".replace("Erre de equis menos erre uno", dice("R(x) − R₁", "Erre de equis menos erre uno")).replace("por equis", "por " + dice("x", "equis")),
            M(r"1-\frac{R_1}{R(x)}=\frac{R(x)-R_1}{R(x)}=\frac{(R_2-R_1)\,x}{L\,R(x)}"))\
        .di("Lo ponemos: la resta de radios se cancela otra vez.",
            M(r"T(x)=T_1-(T_1-T_2)\,\frac{R_2}{R_2-R_1}\cdot\frac{(R_2-R_1)\,x}{L\,R(x)}=T_1-(T_1-T_2)\,\frac{R_2\,x}{L\,R(x)}"))\
        .di("Y ele por erre de equis se abre con la recta del radio.".replace("ele por erre de equis", dice("L·R(x)", "ele por erre de equis")),
            M(r"L\,R(x)=L\left(R_1+\frac{R_2-R_1}{L}\,x\right)=R_1L+(R_2-R_1)\,x"),
            R(r"T(x)=T_1-(T_1-T_2)\,\frac{R_2\,x}{R_1L+(R_2-R_1)\,x}", "rc"))\
        .di("Revisión en los extremos.", M(r"x=0:\ T=T_1"), M(r"x=L:\ \frac{R_2L}{R_1L+R_2L-R_1L}=\frac{R_2L}{R_2L}=1\;\Rightarrow\;T=T_2"))
    receta_cono(L, nr=("R₁", "R₂"))
    L.responde("b", r"H=\frac{\pi kR_1R_2(T_1-T_2)}{L}", None, "").responde("c", r"T(x)=T_1-(T_1-T_2)\frac{R_2x}{R_1L+(R_2-R_1)x}", None, "")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ PR2-D2
@leccion("PR2-D2")
def pr2_d2() -> Leccion:
    lb, k, a0, t1, t2 = 0.80, 180.0, 0.0020, 150.0, 30.0
    g0 = lb / (k * a0)
    Rt = g0 * math.log(2.0)
    H = (t1 - t2) / Rt
    tm = t1 - H * g0 * math.log(1.5)
    gr0, grl = H / (k * a0), H / (k * 2 * a0)
    pts = [[round(lb * i / 40, 4), round(t1 - H * g0 * math.log(1 + i / 40), 2)] for i in range(41)]
    L = _des("PR2-D2", "Barra con área A(x) = A₀(1 + x/L)", "5.5", _f2("D2", 5),
             "Barra de $L=0{,}80$ m y $k=180$ W/(m·K) con área $A(x)=A_0(1+x/L)$, $A_0=0{,}0020\\ \\text{m}^2$. Extremos a $150\\ ^\\circ$C ($x=0$) y $30\\ ^\\circ$C. Lateral aislada, régimen estacionario. a) Resistencia térmica total. b) Rapidez de transferencia de calor. c) Temperatura en el punto medio. d) Extremo con mayor gradiente.",
             claves=["área variable", "resistencia térmica", "práctica"],
             visual={"tipo": "conduccion", "geo": "pared", "L": lb, "T1": t1, "T2": t2, "eje": "x", "uT": "°C", "forma": "0.5 + 0.5 * u", "pts": pts, "xm": lb / 2, "etA": "A(x)"})
    L.paso("Qué nos dan y qué piden")\
        .di("La barra se ensancha hacia el extremo frío: en equis igual a ele tiene el doble de área.".replace("equis igual a ele", dice("x = L", "equis igual a ele")), DATO("A₀ = 0,0020 m²"),
            LISTA(["$L=0{,}80$ m, $k=180$ W/(m·K)", "$A(x)=A_0(1+x/L)$, $A_0=0{,}0020\\ \\text{m}^2$", "$T(0)=150\\ ^\\circ$C, $T(L)=30\\ ^\\circ$C"]))
    L.paso("a) De Fourier a la resistencia térmica")\
        .di("Partimos de la ley de Fourier de la hoja, con el área que cambia.", F(r"H=-kA\,\frac{dT}{dx}"), M(r"H=-k\,A(x)\,\frac{dT}{dx}"))\
        .di("Separamos: multiplicamos los dos lados por de equis, y luego dividimos entre ka por A de equis.".replace("de equis,", dice("dx", "de equis") + ",").replace("A de equis", dice("A(x)", "A de equis")),
            M(r"H\,dx=-k\,A(x)\,dT"), M(r"dT=-\frac{H\,dx}{kA(x)}"))\
        .di("Integramos de un extremo al otro. Hache es constante y sale de la integral.".replace("Hache", dice("H", "Hache")),
            M(r"\int_{T_1}^{T_2}dT=-H\int_0^L\frac{dx}{kA(x)}"))\
        .di("La izquierda es el cambio total de temperatura. Luego cambiamos el signo de los dos lados.",
            M(r"T_2-T_1=-H\int_0^L\frac{dx}{kA(x)}"), M(r"T_1-T_2=H\int_0^L\frac{dx}{kA(x)}"))\
        .di("La resistencia térmica es la caída de temperatura por cada watt.", M(r"R\equiv\frac{T_1-T_2}{H}=\int_0^L\frac{dx}{kA(x)}"))
    L.paso("a) Resolver la integral (regla 3)")\
        .di("Ka y A cero son constantes: salen de la integral.".replace("Ka y A cero", dice("k y A₀", "Ka y A cero")), V(foco="barrido"),
            M(r"R=\frac{1}{kA_0}\int_0^L\frac{dx}{1+x/L}"))\
        .di("Cambio de variable: al paréntesis lo llamamos u.", M(r"u=1+\frac{x}{L}"))\
        .di("Si equis crece un pedacito, u crece ese pedacito entre ele. Despejamos de equis.".replace("Si equis", "Si " + dice("x", "equis")).replace("entre ele", "entre " + dice("L", "ele")).replace("de equis.", dice("dx", "de equis") + "."),
            M(r"du=\frac{dx}{L}"), M(r"dx=L\,du"))\
        .di("Los límites cambian: con equis cero, u vale uno; con equis igual a ele, u vale dos.".replace("equis cero", dice("x = 0", "equis cero")).replace("equis igual a ele", dice("x = L", "equis igual a ele")),
            M(r"x=0\Rightarrow u=1\qquad x=L\Rightarrow u=2"))
    L.paso("a) Resolver la integral: logaritmo")\
        .di("Cambiamos todo a la letra u. Ele es constante y sale.".replace("Ele", dice("L", "Ele")), M(r"\int_0^L\frac{dx}{1+x/L}=\int_1^2\frac{L\,du}{u}=L\int_1^2\frac{du}{u}"))\
        .di("Regla dos: uno entre u da logaritmo.", M(r"L\int_1^2\frac{du}{u}=L\Big[\ln u\Big]_1^2"))\
        .di("Arriba menos abajo. Logaritmo de uno es cero.", M(r"L\,(\ln 2-\ln 1)=L\,(\ln 2-0)=L\ln 2"))
    L.paso("a) Resistencia total con números")\
        .di("Juntamos.", M(r"R=\frac{L}{kA_0}\ln 2"))\
        .di("Sustituimos.", M(rf"R=\frac{{0{{,}}80}}{{(180)(0{{,}}0020)}}\ln 2=({tex(g0, 4)})({tex(math.log(2.0), 4)})"), R(rf"R={tex(Rt, 4)}\ \text{{K/W}}", "ra"))
    L.paso("b) Rapidez de transferencia de calor")\
        .di("La caída total entre la resistencia.", M(rf"H=\frac{{T_1-T_2}}{{R}}=\frac{{150-30}}{{{tex(Rt, 4)}}}"), R(rf"H={tex(H, 3)}\ \text{{W}}", "rb"))
    L.paso("c) Temperatura en el punto medio")\
        .di("Integramos solo desde cero hasta una equis cualquiera: u va de uno a uno más equis entre ele.".replace("una equis", "una " + dice("x", "equis")).replace("uno más equis entre ele", dice("1 + x/L", "uno más equis entre ele")), V(foco="punto"),
            M(r"T_1-T(x)=\frac{H}{kA_0}\int_0^x\frac{dx}{1+x/L}"))\
        .di("El mismo cambio de variable; ahora u llega hasta uno más equis entre ele.".replace("uno más equis entre ele", dice("1 + x/L", "uno más equis entre ele")),
            M(r"T_1-T(x)=\frac{HL}{kA_0}\Big[\ln u\Big]_1^{1+x/L}"))\
        .di("Arriba menos abajo; logaritmo de uno es cero.", M(r"T_1-T(x)=\frac{HL}{kA_0}\ln\left(1+\frac{x}{L}\right)"))\
        .di("Despejamos te de equis: pasa sumando, y lo demás pasa restando.".replace("te de equis", dice("T(x)", "te de equis")), M(r"T(x)=T_1-\frac{HL}{kA_0}\ln\left(1+\frac{x}{L}\right)"))
    _p1 = float(f"{H * g0:.4g}") * float(f"{math.log(1.5):.4g}")
    L.paso("c) Temperatura en el punto medio: números")\
        .di("En la mitad, equis entre ele es un medio: el logaritmo es de uno coma cinco.".replace("equis entre ele", dice("x/L", "equis entre ele")),
            M(rf"T\left(\tfrac{{L}}{{2}}\right)=150-({tex(H, 4)})({tex(g0, 4)})\ln 1{{,}}5=150-({tex(H * g0, 4)})({tex(math.log(1.5), 4)})"))\
        .di("Multiplicamos y restamos.", M(rf"=150-{tex(_p1, 4)}"), R(rf"T(L/2)={tex(tm, 3)}\ {C}", "rc"))
    L.paso("d) Dónde es mayor el gradiente")\
        .di("De Fourier, la pendiente es hache entre ka por el área: es mayor donde el área es menor.".replace("hache entre ka", dice("H/(kA)", "hache entre ka")),
            M(r"\left|\frac{dT}{dx}\right|=\frac{H}{kA(x)}"))\
        .di("Comparamos los dos extremos.", M(rf"x=0:\ \frac{{{tex(H, 4)}}}{{(180)(0{{,}}0020)}}={tex(gr0, 3)}\ \text{{K/m}}"), M(rf"x=L:\ \frac{{{tex(H, 4)}}}{{(180)(0{{,}}0040)}}={tex(grl, 3)}\ \text{{K/m}}"),
            R(r"\text{Mayor en }x=0\ \text{(extremo de }150\ ^\circ\text{C, el de menor área)}", "rd"))
    L.responde("a", rf"R={tex(Rt, 4)}\ \text{{K/W}}", Rt, "K/W").responde("b", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("c", rf"T={tex(tm, 3)}\ {C}", tm, "°C")
    return L.usa(r"H=-kA\frac{dT}{dx}")


# ============================================================================ PR2-D3
@leccion("PR2-D3")
def pr2_d3() -> Leccion:
    A = 2.50
    capas = [("A", 0.040, 0.80), ("B", 0.080, 0.040), ("C", 0.050, 1.20)]
    Rs = [l / (k * A) for _, l, k in capas]
    Rt = sum(Rs)
    H = (100.0 - 20.0) / Rt
    t1 = 100.0 - H * Rs[0]
    t2 = t1 - H * Rs[1]
    q = H * 3 * 3600.0
    L = _des("PR2-D3", "Pared compuesta de tres capas", "5.3", _f2("D3", 6),
             "Pared de área $2{,}50\\ \\text{m}^2$ con tres capas en serie: A ($4{,}0$ cm; $0{,}80$ W/(m·K)), B ($8{,}0$ cm; $0{,}040$ W/(m·K)) y C ($5{,}0$ cm; $1{,}20$ W/(m·K)). Caras externas a $100\\ ^\\circ$C y $20\\ ^\\circ$C. a) Potencia térmica. b) Temperaturas de las interfaces. c) Energía en tres horas. d) Gráfica $T(x)$.",
             claves=["pared compuesta", "serie", "práctica"],
             visual={"tipo": "conduccion", "geo": "serie", "T": [100.0, round(t1, 1), round(t2, 1), 20.0], "uT": "°C",
                     "capas": [{"n": "A", "e": 4.0, "k": 0.80}, {"n": "B", "e": 8.0, "k": 0.040}, {"n": "C", "e": 5.0, "k": 1.20}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Tres capas una detrás de otra: el mismo calor las cruza a todas.", DATO("tres capas en serie"),
            TABLA(["Capa", "Espesor", "$k$ (W/(m·K))"], [["A", "$0{,}040$ m", "$0{,}80$"], ["B", "$0{,}080$ m", "$0{,}040$"], ["C", "$0{,}050$ m", "$1{,}20$"]]),
            LISTA(["Área $2{,}50\\ \\text{m}^2$, caras a $100\\ ^\\circ$C y $20\\ ^\\circ$C"]))
    L.paso("Fourier en cada capa")\
        .di("Partimos de la ley de Fourier de la hoja para una capa.", F(r"H=kA\,\frac{T_H-T_C}{L}"))\
        .di("Despejamos su caída de temperatura: multiplicamos los dos lados por ele, y dividimos entre ka por A.".replace("por ele", "por " + dice("L", "ele")).replace("ka por A", dice("kA", "ka por A")),
            M(r"H\,L=kA\,(T_H-T_C)"), M(r"T_H-T_C=H\,\frac{L}{kA}"))\
        .di("En serie el mismo hache cruza las tres capas. Llamamos te uno y te dos a las interfaces y escribimos la caída de cada capa.".replace("hache", dice("H", "hache")).replace("te uno y te dos", dice("T₁ y T₂", "te uno y te dos")),
            M(r"100-T_1=H\,\frac{L_A}{k_AA}\qquad T_1-T_2=H\,\frac{L_B}{k_BA}\qquad T_2-20=H\,\frac{L_C}{k_CA}"))\
        .di("Sumamos las tres: las temperaturas de las interfaces entran sumando y restando, y se cancelan.",
            M(r"(100-T_1)+(T_1-T_2)+(T_2-20)=100-20"),
            M(r"100-20=H\left(\frac{L_A}{k_AA}+\frac{L_B}{k_BA}+\frac{L_C}{k_CA}\right)"))
    p = L.paso("Cada término L/(kA)")
    for (nom, l, k), r in zip(capas, Rs):
        p.di(f"Capa {nom}.", M(rf"\frac{{L_{nom}}}{{k_{nom}A}}=\frac{{{tex(l, 2)}}}{{({tex(k, 2)})(2{{,}}50)}}={tex(r, 4)}\ \text{{K/W}}"))
    p.di("Sumamos.", M(r"\sum\frac{L}{kA}=" + "+".join(tex(r, 4) for r in Rs) + rf"={tex(Rt, 4)}\ \text{{K/W}}"))
    L.paso("a) Potencia térmica")\
        .di("Con la suma de los tres términos, la ecuación queda así.", M(rf"80{{,}}0=H\,({tex(Rt, 4)})"))\
        .di("Despejamos hache: la suma pasa al otro lado dividiendo.".replace("hache", dice("H", "hache")), M(rf"H=\frac{{80{{,}}0}}{{{tex(Rt, 4)}}}"), R(rf"H={tex(H, 3)}\ \text{{W}}", "ra"))
    L.paso("b) Temperaturas de las interfaces")\
        .di("Entre A y B: restamos la caída de A.", V(foco="perfil"), M(rf"T_1=100-H\frac{{L_A}}{{k_AA}}=100-({tex(H, 4)})({tex(Rs[0], 4)})=100-{tex(H * Rs[0], 4)}"), R(rf"T_1={tex(t1, 3)}\ {C}", "rb1"))\
        .di("Entre B y C: restamos la caída de B.", M(rf"T_2=T_1-H\frac{{L_B}}{{k_BA}}={tex(t1, 4)}-({tex(H, 4)})({tex(Rs[1], 3)})={tex(t1, 4)}-{tex(H * Rs[1], 4)}"), R(rf"T_2={tex(t2, 3)}\ {C}", "rb2"))\
        .di("Revisión con la capa C.", M(rf"T_2-H\frac{{L_C}}{{k_CA}}={tex(t2, 4)}-{tex(H * Rs[2], 3)}={tex(t2 - H * Rs[2], 3)}\ {C}"))
    L.paso("c) Energía en tres horas")\
        .di("De la hoja, hache es energía por tiempo.".replace("hache", dice("H", "hache")), F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"),
            M(r"\Delta t=(3)(3600)=10\,800\ \text{s}"), M(rf"Q=({tex(H, 4)})(10\,800)"), R(rf"Q={tex(q, 3)}\ \text{{J}}", "rc"))
    L.paso("d) Gráfica de T(x)")\
        .di("Dentro de cada capa la temperatura baja en línea recta. Casi toda la caída ocurre en la capa B, la aislante.",
            TABLA(["$x$ (cm)", "$T$ (°C)"], [["$0$", "$100$"], ["$4{,}0$", f"${tex(t1, 3)}$"], ["$12{,}0$", f"${tex(t2, 3)}$"], ["$17{,}0$", "$20{,}0$"]]),
            NOTA("Pendiente $|dT/dx|=H/(kA)$: la menor $k$ da la recta más empinada."))
    L.responde("a", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("b", rf"T_1={tex(t1, 3)}\ {C}", t1, "°C").responde("b", rf"T_2={tex(t2, 3)}\ {C}", t2, "°C")\
     .responde("c", rf"Q={tex(q, 3)}\ \text{{J}}", q, "J")
    return L.usa(r"H=kA\frac{\Delta T}{L}", r"H=\frac{dQ}{dt}")


# ============================================================================ PR2-D4
@leccion("PR2-D4")
def pr2_d4() -> Leccion:
    g = Gas(5.0, "mono")
    g.pone("A", p=100e3, v=0.05)
    g.pone("B", p=400e3, v=0.05)
    g.pone("C", p=400e3, v=0.15)
    rab = g.proceso("A", "B", "isocorico")
    rbc = g.proceso("B", "C", "isobarico")
    rca = g.proceso("C", "A", "recta")
    res = [("A→B", rab), ("B→C", rbc), ("C→A", rca)]
    L = _des("PR2-D4", "Ciclo triangular de 5 mol monoatómico", "6.11", _f2("D4", 7),
             "Cinco moles de gas monoatómico: $A$ ($0{,}05\\ \\text{m}^3$; $100$ kPa), $B$ ($0{,}05\\ \\text{m}^3$; $400$ kPa), $C$ ($0{,}15\\ \\text{m}^3$; $400$ kPa); $C\\to A$ es una recta. a) Temperaturas. b) Trabajo de cada proceso. c) $\\Delta U$ y $Q$. d) Trabajo neto. e) Eficiencia.",
             claves=["ciclo triangular", "monoatómico", "práctica"],
             visual=visual_pv(g, [("A", "B", "isocorico"), ("B", "C", "isobarico"), ("C", "A", "recta")], uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un triángulo que se recorre en sentido horario: sube vertical, va a la derecha y vuelve en diagonal.", DATO("Cinco moles"),
            TABLA(["Estado", "$V$ (m³)", "$P$ (kPa)"], [["A", "$0{,}05$", "$100$"], ["B", "$0{,}05$", "$400$"], ["C", "$0{,}15$", "$400$"]]),
            LISTA(["Monoatómico: $C_V=\\frac{3}{2}R$"]))
    p = L.paso("a) Temperaturas")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B")
    estado_T(p, g, "C")
    explica_proceso(L.paso("b y c) A→B (volumen constante)"), g, "A", "B", "isocorico", foco="AB")
    explica_proceso(L.paso("b y c) B→C (presión constante)"), g, "B", "C", "isobarico", foco="BC")
    explica_proceso(L.paso("b y c) C→A (recta)"), g, "C", "A", "recta", foco="CA")
    c = cierre_ciclo(L, res, titulo="d y e) Trabajo neto y eficiencia", id_e="re")
    L.responde("a", rf"T_C={tex(g.t('C'), 4)}\ \text{{K}}", g.t("C"), "K").responde("d", rf"W_{{\text{{neto}}}}={tex(c['W'], 3)}\ \text{{J}}", c["W"], "J")\
     .responde("e", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.usa(r"pV=nRT", r"W=\int p\,dV", r"\Delta U=nC_V\Delta T", r"e=\frac{W}{Q_H}")


# ============================================================================ PR2-D5
@leccion("PR2-D5")
def pr2_d5() -> Leccion:
    nm, t1, v1, t2 = 3.0, 300.0, 0.0300, 227.4
    rt = t2 / t1
    gm1 = math.log(rt) / math.log(0.5)
    gamma = 1.0 + gm1
    cv = 8.314 / gm1
    cp = cv + 8.314
    w = -nm * cv * (t2 - t1)
    q = nm * cv * (t1 - t2)
    g = Gas(nm, cv=cv)
    g.pone("1", v=v1, t=t1)
    g.pone("2", v=2 * v1, t=t2)
    g.pone("3", v=2 * v1, t=t1)
    L = _des("PR2-D5", "Hallar γ con una expansión adiabática", "6.10", _f2("D5", 8),
             "Tres moles de gas ideal a $300$ K ocupan $0{,}0300\\ \\text{m}^3$. Se expanden adiabática y reversiblemente hasta duplicar el volumen, alcanzando $227{,}4$ K. Luego se calientan a volumen constante hasta $300$ K. a) $\\gamma$. b) $C_P$ y $C_V$. c) Trabajo de expansión. d) Calor del calentamiento. e) Tipo de gas.",
             claves=["gamma", "adiabática", "capacidades caloríficas", "práctica"],
             visual=visual_pv(g, [("1", "2", "adiabatico"), ("2", "3", "isocorico")], uv="L", up="kPa", fv=1e3, fp=1e-3))
    L.paso("Qué nos dan y qué piden")\
        .di("No dicen qué gas es: lo vamos a descubrir con la adiabática.", DATO("227,4 K"),
            LISTA(["$n=3$ mol", "$1$: $300$ K, $0{,}0300\\ \\text{m}^3$", "$2$: $227{,}4$ K, $0{,}0600\\ \\text{m}^3$ (adiabática)", "$3$: $300$ K, $0{,}0600\\ \\text{m}^3$ (volumen constante)"]))
    L.paso("a) γ con la adiabática")\
        .di("La hoja da la relación de temperatura y volumen en la adiabática.", V(foco="12"), F(r"TV^{\gamma-1}=\text{cte}"))\
        .di("La igualamos entre los dos estados.", M(r"T_2V_2^{\gamma-1}=T_1V_1^{\gamma-1}"))\
        .di("Dividimos los dos lados entre te uno y entre el volumen dos elevado a gama menos uno.".replace("te uno", dice("T₁", "te uno")),
            M(r"\frac{T_2}{T_1}=\frac{V_1^{\gamma-1}}{V_2^{\gamma-1}}"))\
        .di("Dos potencias con el mismo exponente se dividen dentro de un solo paréntesis.",
            M(r"\left(\frac{V_1}{V_2}\right)^{\gamma-1}=\frac{T_2}{T_1}"))\
        .di("El volumen se duplica: ve uno entre ve dos es un medio.".replace("ve uno entre ve dos", dice("V₁/V₂", "ve uno entre ve dos")), M(rf"\left(\frac{{1}}{{2}}\right)^{{\gamma-1}}=\frac{{227{{,}}4}}{{300}}={tex(rt, 4)}"))
    L.paso("a) Despejar γ con logaritmos")\
        .di("La incógnita está en el exponente. Sacamos logaritmo a los dos lados.", M(rf"\ln\left(0{{,}}5^{{\gamma-1}}\right)=\ln({tex(rt, 4)})"))\
        .di("Regla de logaritmos: el exponente baja a multiplicar.", M(rf"(\gamma-1)\ln 0{{,}}5=\ln {tex(rt, 4)}"), NOTA("Regla $\\ln(a^n)=n\\ln a$ de la lección **T-MAT3**."))\
        .di("Dividimos entre el logaritmo de un medio. Los dos logaritmos son negativos: menos entre menos da más.",
            M(rf"\gamma-1=\frac{{\ln {tex(rt, 4)}}}{{\ln 0{{,}}5}}=\frac{{{tex(math.log(rt), 5)}}}{{{tex(math.log(0.5), 5)}}}={tex(gm1, 4)}"))\
        .di("Le sumamos uno.", M(rf"\gamma={tex(gm1, 4)}+1"), R(rf"\gamma={tex(gamma, 3)}", "ra"))
    L.paso("b) C_V y C_P desde la hoja")\
        .di("Juntamos las dos casillas de la hoja.", F(r"C_P=C_V+R"), F(r"\gamma=\frac{C_P}{C_V}"))\
        .di("Como ce pe es gama por ce ve, lo ponemos en la primera casilla.".replace("ce pe", dice("C_P", "ce pe")).replace("gama por ce ve", dice("γC_V", "gama por ce ve")),
            M(r"\gamma C_V=C_V+R"))\
        .di("Pasamos ce ve restando al lado izquierdo.".replace("ce ve", dice("C_V", "ce ve")), M(r"\gamma C_V-C_V=R"))\
        .di("Sacamos ce ve de factor común.".replace("ce ve", dice("C_V", "ce ve")), M(r"C_V\,(\gamma-1)=R"))\
        .di("Dividimos los dos lados entre gama menos uno.".replace("gama menos uno", dice("γ − 1", "gama menos uno")), M(r"C_V=\frac{R}{\gamma-1}"))\
        .di("Sustituimos.", M(rf"C_V=\frac{{8{{,}}314}}{{{tex(gm1, 4)}}}={tex(cv, 4)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"), M(rf"C_P={tex(cv, 4)}+8{{,}}314={tex(cp, 4)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"),
            R(rf"C_V={tex(cv, 3)}\qquad C_P={tex(cp, 3)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}", "rb"))
    L.paso("c) Trabajo de la expansión")\
        .di("Adiabática: no hay calor. De la primera ley, el trabajo es menos el cambio de energía interna.", F(r"\Delta U=Q-W"), M(r"Q=0\;\Rightarrow\;W=-\Delta U"))\
        .di("La energía interna con ce ve.".replace("ce ve", dice("C_V", "ce ve")), F(r"\Delta U=nC_V\,\Delta T"),
            M(rf"W=-(3)({tex(cv, 4)})(227{{,}}4-300)"))\
        .di("Restamos adentro: doscientos veintisiete coma cuatro menos trescientos da menos setenta y dos coma seis.", M(rf"W=-(3)({tex(cv, 4)})(-72{{,}}6)"))\
        .di("Menos por menos da más.", M(rf"W=(3)({tex(cv, 4)})(72{{,}}6)"))\
        .di("Multiplicamos tres por ce ve, y luego por setenta y dos coma seis.".replace("ce ve", dice("C_V", "ce ve")),
            M(rf"W=({tex(nm * float(f'{cv:.4g}'), 4)})(72{{,}}6)"), R(rf"W={tex(w, 4)}\ \text{{J}}", "rc"))
    L.paso("c) Revisión con la casilla adiabática")\
        .di("Revisamos con la casilla adiabática de la hoja; con el gas ideal, pe ve es ene erre te.".replace("pe ve es ene erre te", dice("pV = nRT", "pe ve es ene erre te")),
            F(r"W=\frac{1}{\gamma-1}\left(p_1V_1-p_2V_2\right)"), M(r"W=\frac{nRT_1-nRT_2}{\gamma-1}"))\
        .di("Sustituimos, con un decimal para que la resta no pierda exactitud.",
            M(rf"W=\frac{{{tex(nm * 8.314 * t1, dec=1)}-{tex(nm * 8.314 * t2, dec=1)}}}{{{tex(gm1, 4)}}}=\frac{{{tex(round(nm * 8.314 * t1, 1) - round(nm * 8.314 * t2, 1), dec=1)}}}{{{tex(gm1, 4)}}}"))\
        .di("Dividimos: da lo mismo que antes.", M(rf"W={tex((round(nm * 8.314 * t1, 1) - round(nm * 8.314 * t2, 1)) / float(f'{gm1:.4g}'), 4)}\ \text{{J}}"))
    L.paso("d) Calor del calentamiento")\
        .di("A volumen constante el calor de la hoja va con ce ve.".replace("ce ve", dice("C_V", "ce ve")), V(foco="23"), F(r"Q=nC\,\Delta T"),
            M(rf"Q=(3)({tex(cv, 4)})(300-227{{,}}4)=({tex(nm * cv, 4)})(72{{,}}6)"), R(rf"Q={tex(q, 4)}\ \text{{J}}", "rd"))\
        .di("Es igual al trabajo: el gas vuelve a trescientos kelvin, así que recupera la energía que gastó al expandirse.", NOTA("$Q_{23}=-\\Delta U_{12}=W_{12}$."))
    L.paso("e) Qué gas es")\
        .di("Comparamos con las capacidades de la hoja.", F(r"C_V=\frac{5}{2}R"), M(rf"\frac{{5}}{{2}}R=\frac{{5}}{{2}}(8{{,}}314)=20{{,}}79\approx{tex(cv, 4)}"))\
        .di("Gama uno coma cuatro y ce ve cinco medios de erre: es diatómico, con rotaciones activas y sin vibraciones.".replace("Gama", dice("γ", "Gama")).replace("ce ve", dice("C_V", "ce ve")),
            R(r"\text{Gas diatómico}", "re"))
    L.responde("a", rf"\gamma={tex(gamma, 3)}", gamma, "").responde("b", rf"C_V={tex(cv, 3)}", cv, "J/(mol·K)").responde("b", rf"C_P={tex(cp, 3)}", cp, "J/(mol·K)")\
     .responde("c", rf"W={tex(w, 4)}\ \text{{J}}", w, "J").responde("d", rf"Q={tex(q, 4)}\ \text{{J}}", q, "J")
    return L.usa(r"TV^{\gamma-1}=\text{cte}", r"C_P=C_V+R", r"\gamma=\frac{C_P}{C_V}", r"\Delta U=nC_V\Delta T")


# ============================================================================ PR2-D6
@leccion("PR2-D6")
def pr2_d6() -> Leccion:
    g = Gas(2.0, "dia")
    g.pone("A", p=100e3, v=0.02)
    g.pone("B", p=600e3, v=0.02)
    g.pone("C", p=200e3, v=0.06)
    g.pone("D", p=100e3, v=0.06)
    rab = g.proceso("A", "B", "isocorico")
    rbc = g.proceso("B", "C", "isotermico")
    rcd = g.proceso("C", "D", "isocorico")
    rda = g.proceso("D", "A", "isobarico")
    res = [("A→B", rab), ("B→C", rbc), ("C→D", rcd), ("D→A", rda)]
    L = _des("PR2-D6", "Ciclo de cuatro procesos (2 mol diatómico)", "6.11", _f2("D6", 9),
             "Dos moles de gas diatómico: $A\\to B$ isocórico, $B\\to C$ isotérmico, $C\\to D$ isocórico y $D\\to A$ isobárico. $A$ ($0{,}02\\ \\text{m}^3$; $100$ kPa), $B$ ($0{,}02$; $600$ kPa), $C$ ($0{,}06$; $200$ kPa), $D$ ($0{,}06$; $100$ kPa). a) Temperaturas. b) $W$, $Q$ y $\\Delta U$. c) Calor total absorbido. d) Eficiencia. e) Calor liberado en 100 ciclos.",
             claves=["cuatro procesos", "diatómico", "práctica"],
             visual=visual_pv(g, [("A", "B", "isocorico"), ("B", "C", "isotermico"), ("C", "D", "isocorico"), ("D", "A", "isobarico")], uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro procesos que vuelven al inicio. B y C están en la misma isoterma: pe por ve vale lo mismo; como el gas es diatómico, tomamos su capacidad molar de la hoja.".replace("pe por ve", dice("PV", "pe por ve")), DATO("Dos moles"),
            TABLA(["Estado", "$V$ (m³)", "$P$ (kPa)"], [["A", "$0{,}02$", "$100$"], ["B", "$0{,}02$", "$600$"], ["C", "$0{,}06$", "$200$"], ["D", "$0{,}06$", "$100$"]]),
            F(r"C_{V\text{dia}}=\frac{5}{2}R"),
            M(r"P_BV_B=(600\times10^3)(0{,}02)=12\,000\ \text{J}=P_CV_C=(200\times10^3)(0{,}06)"))
    p = L.paso("a) Temperaturas")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B")
    estado_T(p, g, "C", voz="En C sale la misma que en B: la isoterma.")
    estado_T(p, g, "D")
    explica_proceso(L.paso("b) A→B (volumen constante)"), g, "A", "B", "isocorico", foco="AB")
    explica_proceso(L.paso("b) B→C (isoterma)"), g, "B", "C", "isotermico", foco="BC")
    explica_proceso(L.paso("b) C→D (volumen constante)"), g, "C", "D", "isocorico", foco="CD")
    explica_proceso(L.paso("b) D→A (presión constante)"), g, "D", "A", "isobarico", foco="DA")
    c = cierre_ciclo(L, res, titulo="c y d) Calor absorbido y eficiencia", id_e="rd")
    qc100 = 100 * abs(c["QC"])
    L.paso("e) Calor liberado en 100 ciclos")\
        .di("En cada ciclo el gas libera el valor absoluto de los calores negativos. En cien ciclos, cien veces eso.",
            M(rf"|Q_C|={tex(abs(c['QC']), 5, sci='no')}\ \text{{J por ciclo}}"), M(rf"Q_{{100}}=(100)({tex(abs(c['QC']), 5, sci='no')})"),
            R(rf"Q_{{100}}={tex(qc100, 3)}\ \text{{J}}", "re"))
    L.responde("a", rf"T_B={tex(g.t('B'), 4)}\ \text{{K}}", g.t("B"), "K").responde("c", rf"Q_H={tex(c['QH'], 4)}\ \text{{J}}", c["QH"], "J")\
     .responde("d", rf"e={tex(c['e'], 3)}", c["e"], "").responde("e", rf"Q={tex(qc100, 3)}\ \text{{J}}", qc100, "J")
    return L.usa(r"pV=nRT", r"W=nRT\ln\frac{V_2}{V_1}", r"W=p(V_2-V_1)", r"e=\frac{W}{Q_H}")


# ============================================================================ PR2-D7
@leccion("PR2-D7")
def pr2_d7() -> Leccion:
    mfe, cfe, tfe, mh, mw, lf = 0.600, 450.0, 180.0, 0.200, 0.400, 334000.0
    qf = mh * lf
    qfe = mfe * cfe * tfe
    mfund = qfe / lf
    L = _des("PR2-D7", "Fusión parcial: hierro caliente sobre hielo y agua", "4.7", _f2("D7", 10),
             "Un calorímetro de cobre de $0{,}150$ kg contiene $0{,}400$ kg de agua y $0{,}200$ kg de hielo en equilibrio a $0\\ ^\\circ$C. Se introduce hierro de $0{,}600$ kg a $180\\ ^\\circ$C. Sistema aislado. $c_{Fe}=450$, $c_{Cu}=387$, $c_w=4186$ J/(kg·K); $L_f=334\\,000$ J/kg. a) ¿Se funde todo el hielo? b) Temperatura final. c) Masas finales de agua y hielo. d) Energía cedida por el hierro.",
             claves=["fusión parcial", "calorimetría", "práctica"],
             visual={"tipo": "calor", "Tf": 0,
                     "componentes": [{"n": "hierro", "m": 600, "T": 180, "fase": "caliente"}, {"n": "hielo", "m": 200, "T": 0, "fase": "hielo"}, {"n": "agua", "m": 400, "T": 0, "fase": "agua"}],
                     "necesita": [{"n": "fundir todo el hielo", "Q": qf}], "disponible": [{"n": "hierro de 180 a 0 °C", "Q": qfe}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Agua, hielo y el calorímetro ya están a cero grados. El único que trae energía es el hierro.", DATO("en equilibrio a 0 °C"),
            LISTA(["Hierro: $0{,}600$ kg a $180\\ ^\\circ$C", "Hielo $0{,}200$ kg y agua $0{,}400$ kg a $0\\ ^\\circ$C", "Calorímetro de cobre a $0\\ ^\\circ$C"]))
    L.paso("a) ¿Alcanza para fundir todo el hielo?")\
        .di("Lo que cuesta fundir todo el hielo: calor latente de la hoja.", F(r"Q=\pm mL"), M(rf"Q_{{\text{{fundir}}}}=(0{{,}}200)(334\,000)={_j(qf)}\ \text{{J}}"))\
        .di("Lo máximo que puede dar el hierro, si baja hasta cero grados: calor sensible de la hoja.", F(r"Q=mc\,\Delta T"),
            M(rf"Q_{{\text{{Fe,máx}}}}=(0{{,}}600)(450)(180-0)={_j(qfe)}\ \text{{J}}"))\
        .di("No alcanza: queda hielo sin fundir.", M(rf"{_j(qfe)}\ \text{{J}}<{_j(qf)}\ \text{{J}}"), R(r"\text{No se funde todo el hielo}", "ra"))
    L.paso("b) Temperatura final")\
        .di("Como el calor del hierro no alcanza para fundir todo el hielo, coexisten hielo y agua líquida en equilibrio a cero grados. El agua inicial y el calorímetro no cambian de temperatura.",
            M(rf"Q_{{\text{{Fe,máx}}}}={_j(qfe)}\ \text{{J}}<Q_{{\text{{fundir}}}}={_j(qf)}\ \text{{J}}\;\Rightarrow\;\text{{coexisten hielo y agua}}"),
            R(rf"T_f=0\ {C}", "rb"), NOTA("Hielo y agua conviven solo a $0\\ ^\\circ$C (a 1 atm)."))
    L.paso("c) Masas finales")\
        .di("Todo lo que da el hierro se usa para fundir hielo. Del calor latente de la hoja despejamos la masa: ele pasa dividiendo.".replace("ele pasa", dice("L", "ele") + " pasa"),
            F(r"Q=\pm mL"), M(r"m=\frac{Q}{L_f}"))\
        .di("Sustituimos el calor que dio el hierro.", M(rf"m_{{\text{{fundida}}}}=\frac{{Q_{{\text{{Fe}}}}}}{{L_f}}=\frac{{{_j(qfe)}}}{{334\,000}}={tex(mfund, 4)}\ \text{{kg}}"))\
        .di("Hielo que queda y agua que hay.", M(rf"m_{{\text{{hielo}}}}=0{{,}}200-{tex(mfund, 4)}={tex(mh - mfund, 3)}\ \text{{kg}}"),
            M(rf"m_{{\text{{agua}}}}=0{{,}}400+{tex(mfund, 4)}={tex(mw + mfund, 4)}\ \text{{kg}}"),
            R(rf"m_{{\text{{hielo}}}}={tex(mh - mfund, 3)}\ \text{{kg}}\qquad m_{{\text{{agua}}}}={tex(mw + mfund, 3)}\ \text{{kg}}", "rc"))
    L.paso("d) Energía cedida por el hierro")\
        .di("El hierro baja de ciento ochenta a cero grados.", M(rf"Q_{{\text{{Fe}}}}=(0{{,}}600)(450)(0-180)=-{_j(qfe)}\ \text{{J}}"),
            R(rf"\text{{Cede }}{_j(qfe)}\ \text{{J}}", "rd"))
    L.responde("c", rf"m_{{\text{{hielo}}}}={tex(mh - mfund, 3)}\ \text{{kg}}", mh - mfund, "kg").responde("c", rf"m_{{\text{{agua}}}}={tex(mw + mfund, 3)}\ \text{{kg}}", mw + mfund, "kg")\
     .responde("d", rf"Q={_j(qfe)}\ \text{{J}}", qfe, "J")
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ PR2-D8
@leccion("PR2-D8")
def pr2_d8() -> Leccion:
    mw, cw, ti, tf, crec, lv = 0.750, 4186.0, 18.0, 45.0, 120.0, 2.26e6
    qa = mw * cw * (tf - ti)
    qr = crec * (tf - ti)
    por_kg = lv + cw * (100.0 - tf)
    mv = (qa + qr) / por_kg
    L = _des("PR2-D8", "Vapor que se condensa en agua a 18 °C", "4.7", _f2("D8", 11),
             "Un recipiente aislado ($C=120$ J/K, a $18{,}0\\ ^\\circ$C) contiene $0{,}750$ kg de agua a $18{,}0\\ ^\\circ$C. Se introduce vapor saturado a $100\\ ^\\circ$C hasta llegar a $45{,}0\\ ^\\circ$C. Condensación completa. $c_w=4186$ J/(kg·K), $L_v=2{,}26\\times10^6$ J/kg. a) Calor absorbido por el agua. b) Por el recipiente. c) Masa de vapor. d) Masa final de agua.",
             claves=["condensación", "calorimetría", "práctica"],
             visual={"tipo": "calor", "Tf": 45,
                     "componentes": [{"n": "agua", "m": 750, "T": 18, "fase": "agua"}, {"n": "vapor", "m": round(mv * 1000), "T": 100, "fase": "vapor"}],
                     "necesita": [{"n": "agua de 18 a 45 °C", "Q": qa}, {"n": "recipiente", "Q": qr}], "disponible": [{"n": "condensar y enfriar el vapor", "Q": qa + qr}]})
    L.paso("Qué nos dan y qué piden")\
        .di("El vapor se condensa y el agua que forma se enfría hasta cuarenta y cinco grados; el agua y el recipiente se calientan.", DATO("45,0 °C"),
            LISTA(["Agua: $0{,}750$ kg de $18{,}0$ a $45{,}0\\ ^\\circ$C", "Recipiente: $C=120$ J/K, de $18{,}0$ a $45{,}0\\ ^\\circ$C", "Vapor: $m$ desconocida a $100\\ ^\\circ$C"]))
    L.paso("a) Calor que absorbe el agua")\
        .di("Calor sensible de la hoja.", F(r"Q=mc\,\Delta T"), M(rf"Q_{{\text{{agua}}}}=(0{{,}}750)(4186)(45{{,}}0-18{{,}}0)"), R(rf"Q_{{\text{{agua}}}}={_j(qa, 6)}\ \text{{J}}", "ra"))
    L.paso("b) Calor que absorbe el recipiente")\
        .di("La capacidad calorífica es el producto de masa por calor específico: ya viene junta.", M(r"C=mc\;\Rightarrow\;Q=C\,\Delta T"),
            M(r"Q_{\text{rec}}=(120)(45{,}0-18{,}0)"), R(rf"Q_{{\text{{rec}}}}={_j(qr)}\ \text{{J}}", "rb"))
    L.paso("c) Calor que entrega el vapor")\
        .di("Cada kilogramo de vapor se condensa y luego, ya como agua, se enfría de cien a cuarenta y cinco. Sale calor: por eso el signo menos.",
            F(r"Q=\pm mL"), F(r"Q=mc\,\Delta T"), M(r"Q_{\text{vapor}}=-m\left[L_v+c_w(100-45{,}0)\right]"))\
        .di("Sustituimos. Cien menos cuarenta y cinco es cincuenta y cinco.", M(r"Q_{\text{vapor}}=-m\left[2{,}26\times10^6+(4186)(55{,}0)\right]"))\
        .di("Multiplicamos adentro y sumamos.", M(rf"Q_{{\text{{vapor}}}}=-m\left[2\,260\,000+{_j(cw * 55.0, 6)}\right]=-m\,({_j(por_kg, 7)})"))
    L.paso("c) Masa de vapor: balance")\
        .di("Recipiente aislado: la suma de todos los calores es cero.", F(r"\sum Q=0"), M(rf"{_j(qa, 6)}+{_j(qr)}-m\,({_j(por_kg, 7)})=0"))\
        .di("Pasamos el término del vapor al otro lado, sumando.", M(rf"m\,({_j(por_kg, 7)})={_j(qa, 6)}+{_j(qr)}={_j(qa + qr, 6)}"))\
        .di(f"Despejamos la masa: el número que la multiplica pasa dividiendo. Son {nu(mv * 1000, 'g', 3)}.", M(rf"m=\frac{{{_j(qa + qr, 6)}}}{{{_j(por_kg, 7)}}}"), R(rf"m={tex(mv, 3)}\ \text{{kg}}={tex(mv * 1000, 3)}\ \text{{g}}", "rc"))
    L.paso("d) Masa final de agua")\
        .di("El vapor condensado se suma al agua.", M(rf"m_f=0{{,}}750+{tex(mv, 4)}"), R(rf"m_f={tex(mw + mv, 3)}\ \text{{kg}}", "rd"))
    L.responde("a", rf"Q={_j(qa, 6)}\ \text{{J}}", qa, "J").responde("c", rf"m={tex(mv, 3)}\ \text{{kg}}", mv, "kg").responde("d", rf"m_f={tex(mw + mv, 3)}\ \text{{kg}}", mw + mv, "kg")
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ PR2-D9
@leccion("PR2-D9")
def pr2_d9() -> Leccion:
    nm, gam, t1, v1, r, qh = 0.0250, 1.40, 300.0, 500e-6, 9.00, 650.0
    cv = 8.314 / (gam - 1.0)
    ncv = nm * cv
    f = r ** (gam - 1.0)
    t2 = t1 * f
    t3 = t2 + qh / ncv
    t4 = t3 / f
    qc = ncv * (t1 - t4)
    w = qh + qc
    e = w / qh
    g = Gas(nm, cv=cv)
    g.pone("1", v=v1, t=t1)
    g.pone("2", v=v1 / r, t=t2)
    g.pone("3", v=v1 / r, t=t3)
    g.pone("4", v=v1, t=t4)
    L = _des("PR2-D9", "Ciclo Otto ideal: temperaturas, trabajo y eficiencia", "6.14", _f2("D9", 12),
             "Motor ideal con $0{,}0250$ mol de gas diatómico ($\\gamma=1{,}40$). Al iniciar la compresión $T_1=300$ K y $V_1=500\\ \\text{cm}^3$; razón de compresión $9{,}00$; en el calentamiento isocórico recibe $650$ J. a) Volumen mínimo. b) $T$ al terminar la compresión. c) $T$ máxima. d) $T$ al terminar la expansión. e) Trabajo neto. f) Eficiencia.",
             claves=["Otto", "razón de compresión", "práctica"],
             visual=visual_pv(g, [("1", "2", "adiabatico"), ("2", "3", "isocorico"), ("3", "4", "adiabatico"), ("4", "1", "isocorico")], uv="cm³", up="kPa", fv=1e6, fp=1e-3))
    L.paso("Qué nos dan y qué piden")\
        .di("El ciclo Otto: compresión sin calor, calentamiento a volumen constante, expansión sin calor y enfriamiento a volumen constante.", DATO("razón de compresión es 9,00"),
            LISTA(["$n=0{,}0250$ mol, $\\gamma=1{,}40$", "$T_1=300$ K, $V_1=500\\ \\text{cm}^3$", "$r=9{,}00$, $Q_H=650$ J"]))
    L.paso("a) Volumen mínimo")\
        .di("La razón de compresión de la hoja es el volumen máximo entre el mínimo; también convertimos el volumen inicial a litros con la equivalencia de la hoja.", F(r"r=\frac{V_{\text{máx}}}{V_{\text{mín}}}"),
            M(r"1\ \text{L}=1000\ \text{cm}^3\;\Rightarrow\;V_1=500\ \text{cm}^3=0{,}500\ \text{L}"))\
        .di("Despejamos el volumen mínimo: pasa multiplicando a la izquierda, y erre pasa dividiendo.".replace("y erre", "y " + dice("r", "erre")),
            M(r"r\,V_{\text{mín}}=V_{\text{máx}}"), M(r"V_{\text{mín}}=\frac{V_{\text{máx}}}{r}=\frac{V_1}{r}=\frac{500}{9{,}00}"),
            R(rf"V_{{\text{{mín}}}}={tex(v1 / r * 1e6, 3)}\ \text{{cm}}^3", "ra"))
    L.paso("b) Temperatura al final de la compresión")\
        .di("Compresión adiabática: la relación de temperatura y volumen de la hoja. La igualamos entre 1 y 2.", V(foco="12"), F(r"TV^{\gamma-1}=\text{cte}"),
            M(r"T_2V_2^{\gamma-1}=T_1V_1^{\gamma-1}"))\
        .di("Dividimos los dos lados entre el volumen dos elevado a gama menos uno, y juntamos las potencias en un paréntesis.",
            M(r"T_2=T_1\,\frac{V_1^{\gamma-1}}{V_2^{\gamma-1}}=T_1\left(\frac{V_1}{V_2}\right)^{\gamma-1}"))\
        .di("Ve uno entre ve dos es el máximo entre el mínimo: la razón de compresión.".replace("Ve uno entre ve dos", dice("V₁/V₂", "Ve uno entre ve dos")),
            M(r"T_2=T_1\,r^{\gamma-1}"))\
        .di("Sustituimos: nueve elevado a cero coma cuarenta, en la calculadora.", M(rf"T_2=300\,(9{{,}}00)^{{0{{,}}40}}=300\,({tex(f, 5)})"), R(rf"T_2={tex(t2, 4)}\ \text{{K}}", "rb"))
    L.paso("c) Temperatura máxima: C_V")\
        .di("Para un gas diatómico, la capacidad a volumen constante sale de la hoja.".replace("ce ve", dice("C_V", "ce ve")), F(r"C_{V\text{dia}}=\frac{5}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"),
            NOTA("Coincide con $C_V=\\frac{R}{\\gamma-1}=\\frac{8{,}314}{0{,}40}$, que sale de $C_P=C_V+R$ y $\\gamma=\\frac{C_P}{C_V}$ (despeje completo en PR2-D5)."))
    L.paso("c) Temperatura máxima: calentamiento")\
        .di("El calor a volumen constante sube la temperatura: calor de la hoja.", V(foco="23"), F(r"Q=nC\,\Delta T"), M(r"Q_H=nC_V\,(T_3-T_2)"))\
        .di("Dividimos los dos lados entre ene ce ve, y pasamos te dos sumando.".replace("ene ce ve", dice("nC_V", "ene ce ve")).replace("te dos", dice("T₂", "te dos")),
            M(r"T_3-T_2=\frac{Q_H}{nC_V}"), M(r"T_3=T_2+\frac{Q_H}{nC_V}"))\
        .di("Sustituimos.", M(rf"T_3={tex(t2, 4)}+\frac{{650}}{{(0{{,}}0250)({tex(cv, 5)})}}={tex(t2, 4)}+\frac{{650}}{{{tex(ncv, 6)}}}={tex(t2, 4)}+{tex(qh / ncv, 5)}"),
            R(rf"T_3={tex(t3, 4)}\ \text{{K}}", "rc"))
    # Cifras para que T₃ entre el factor, hecho con lo escrito, dé T₄ como se muestra
    for d3, cf in ((0, 4), (1, 5), (2, 6), (2, 7)):
        s3, sf = tex(t3, dec=d3), tex(f, cf)
        if tex(round(t3, d3) / float(f"{f:.{cf}g}"), 4) == tex(t4, 4):
            break
    L.paso("d) Temperatura al final de la expansión")\
        .di("Expansión adiabática de 3 a 4: la misma relación de la hoja. Ahora el volumen pasa del mínimo al máximo, así que la razón queda al revés.", V(foco="34"),
            M(r"T_4=T_3\left(\frac{V_3}{V_4}\right)^{\gamma-1}=T_3\left(\frac{1}{r}\right)^{\gamma-1}=\frac{T_3}{r^{\gamma-1}}"))\
        .di("Sustituimos y dividimos.", M(rf"T_4=\frac{{{s3}}}{{{sf}}}"), R(rf"T_4={tex(t4, 4)}\ \text{{K}}", "rd"))
    L.paso("e) Trabajo neto")\
        .di("El calor sale en el enfriamiento a volumen constante, de cuatro a uno.", V(foco="41"),
            M(rf"Q_C=nC_V(T_1-T_4)=({tex(ncv, 4)})(300-{tex(t4, 4)})={tex(qc, 4)}\ \text{{J}}"))\
        .di("En un ciclo la energía interna no cambia: el trabajo neto es el calor neto.", F(r"\Delta U=Q-W"), M(r"\Delta U_{\text{ciclo}}=0\;\Rightarrow\;W=Q_H+Q_C"),
            M(rf"W=650+({tex(qc, 4)})"), R(rf"W={tex(w, 4)}\ \text{{J}}", "re"))
    L.paso("f) Eficiencia y despeje inverso de r")\
        .di("Lo que se obtiene entre lo que se paga.", F(r"e=\frac{W}{Q_H}"), M(rf"e=\frac{{{tex(w, 4)}}}{{650}}"), R(rf"e={tex(e, 3)}={tex(e * 100, 3)}\ \%", "rf"))\
        .di("Revisión con la eficiencia de Otto de la hoja. Si en el examen te dan la eficiencia y piden la razón de compresión, recuerda el despeje inverso.", F(r"e_{\text{otto}}=1-\frac{1}{r^{\gamma-1}}"), M(rf"1-\frac{{1}}{{{tex(f, 4)}}}=1-{tex(1 / f, 4)}={tex(1 - 1 / f, 3)}"),
            OJO(r"Trampa inversa frecuente: si conoces $e_{\text{otto}}$ y piden $r$, despeja $r = \left(\frac{1}{1-e_{\text{otto}}}\right)^{1/(\gamma-1)}$ (paso siguiente, sin saltos)."))
    e4 = float(f"{1 - 1 / f:.4g}")
    inv = 1.0 / (1.0 - e4)
    L.paso("f) El despeje inverso: de la eficiencia a r")\
        .di("Si dan la eficiencia y piden la razón de compresión, partimos de la misma casilla de la hoja.", F(r"e_{\text{otto}}=1-\frac{1}{r^{\gamma-1}}"))\
        .di("Pasamos la fracción a la izquierda y la eficiencia a la derecha: cada una cambia de signo.", M(r"\frac{1}{r^{\gamma-1}}=1-e"))\
        .di("Damos vuelta a los dos lados: si dos números son iguales, sus inversos también.", M(r"r^{\gamma-1}=\frac{1}{1-e}"))\
        .di("Para quitar el exponente gama menos uno, elevamos los dos lados a uno sobre gama menos uno: al multiplicar los exponentes queda uno.",
            M(r"r=\left(\frac{1}{1-e}\right)^{1/(\gamma-1)}"))\
        .di("Comprobamos con nuestro motor. Uno entre cero coma cuarenta es dos coma cinco.",
            M(rf"r=\left(\frac{{1}}{{1-{tex(e4, 4)}}}\right)^{{2{{,}}5}}=({tex(inv, 5)})^{{2{{,}}5}}={tex(float(f'{inv:.5g}') ** 2.5, 3)}"))
    L.responde("a", rf"V={tex(v1 / r * 1e6, 3)}\ \text{{cm}}^3", v1 / r * 1e6, "cm³").responde("b", rf"T_2={tex(t2, 4)}\ \text{{K}}", t2, "K")\
     .responde("c", rf"T_3={tex(t3, 4)}\ \text{{K}}", t3, "K").responde("d", rf"T_4={tex(t4, 4)}\ \text{{K}}", t4, "K")\
     .responde("e", rf"W={tex(w, 4)}\ \text{{J}}", w, "J").responde("f", rf"e={tex(e, 3)}", e, "")
    return L.usa(r"r=\frac{V_{max}}{V_{min}}", r"TV^{\gamma-1}=\text{cte}", r"e_{otto}=1-\frac{1}{r^{\gamma-1}}")


# ============================================================================ PR2-D10
@leccion("PR2-D10")
def pr2_d10() -> Leccion:
    e1 = math.e - 1.0
    # Perfil ilustrativo (T₁ = 100, T₂ = 0, L = 1) con la T(x) de la parte c
    pts = [[round(i / 40, 4), round(100.0 - 100.0 * (math.exp(i / 40) - 1.0) / e1, 2)] for i in range(41)]
    L = _des("PR2-D10", "Barra con área A(x) = A₀ e^(−x/L)", "5.5", _f2("D10", 13),
             "Barra homogénea de longitud $L$ y conductividad $k$ con área $A(x)=A_0e^{-x/L}$. En $x=0$ está a $T_1$ y en $x=L$ a $T_2<T_1$; lateral aislada, régimen estacionario. a) Resistencia térmica total y $H$. b) Razón con una barra uniforme de área $A_0$. c) $T(x)$ y extremo con mayor $|dT/dx|$.",
             claves=["área exponencial", "resistencia térmica", "simbólico", "práctica"],
             visual={"tipo": "conduccion", "geo": "pared", "L": 1.0, "T1": 100, "T2": 0, "eje": "x", "uT": "°C", "forma": "exp(-u)", "pts": pts, "etA": "A₀e^(−x/L)"})
    L.paso("Qué nos dan y qué piden")\
        .di("El área se achica de forma exponencial: al final es la de inicio dividida entre e.", DATO("A0"),
            M(r"A(0)=A_0\qquad A(L)=A_0e^{-1}=\frac{A_0}{e}"))
    L.paso("a) Fourier y separar variables")\
        .di("Partimos de la ley de Fourier de la hoja, con el área que cambia.", F(r"H=-kA\,\frac{dT}{dx}"), M(r"H=-k\,A(x)\,\frac{dT}{dx}"))\
        .di("Separamos: multiplicamos los dos lados por de equis, y luego dividimos entre ka por el área.".replace("de equis", dice("dx", "de equis")).replace("entre ka", "entre " + dice("k", "ka")),
            M(r"H\,dx=-k\,A(x)\,dT"), M(r"dT=-\frac{H\,dx}{kA(x)}=-\frac{H\,dx}{kA_0e^{-x/L}}"))\
        .di("Dividir entre e a la menos algo es multiplicar por e a la más algo.", M(r"\frac{1}{e^{-x/L}}=e^{x/L}"), M(r"dT=-\frac{H}{kA_0}\,e^{x/L}\,dx"))
    L.paso("a) Integrar los dos lados")\
        .di("Integramos toda la barra: la temperatura va de te uno a te dos, y equis de cero a ele.".replace("te uno a te dos", dice("T₁ a T₂", "te uno a te dos")).replace("equis de cero a ele", dice("x de 0 a L", "equis de cero a ele")),
            M(r"\int_{T_1}^{T_2}dT=-\frac{H}{kA_0}\int_0^Le^{x/L}\,dx"))\
        .di("La izquierda es el cambio total de temperatura. Luego cambiamos el signo de los dos lados.",
            M(r"T_2-T_1=-\frac{H}{kA_0}\int_0^Le^{x/L}\,dx"), M(r"T_1-T_2=\frac{H}{kA_0}\int_0^Le^{x/L}\,dx"))
    L.paso("a) La integral de la exponencial (regla 4)")\
        .di("Para ver de dónde sale la ele: cambio de variable. Al exponente lo llamamos u.".replace("la ele", "la " + dice("L", "ele")),
            M(r"u=\frac{x}{L}"), M(r"du=\frac{dx}{L}\;\Rightarrow\;dx=L\,du"))\
        .di("La integral de e a la u es la misma e a la u. Por eso queda la exponencial multiplicada por ele.".replace("por ele", "por " + dice("L", "ele")),
            M(r"\int e^{x/L}dx=\int e^{u}\,L\,du=L\,e^{u}=L\,e^{x/L}"), NOTA("Regla 4 de la lección **T-MAT2**."))\
        .di("Arriba menos abajo.", M(r"\int_0^Le^{x/L}dx=L\Big[e^{x/L}\Big]_0^L=L\left(e^{L/L}-e^{0/L}\right)"))\
        .di("E a la uno es e; e a la cero es uno.", M(r"=L\left(e^{1}-e^{0}\right)=L\,(e-1)"))
    L.paso("a) Resistencia y despeje de H")\
        .di("Lo ponemos en la ecuación.", M(r"T_1-T_2=\frac{H}{kA_0}\,L\,(e-1)=\frac{H\,L\,(e-1)}{kA_0}"))\
        .di("La resistencia es la caída de temperatura entre el flujo: dividimos los dos lados entre hache.".replace("hache", dice("H", "hache")),
            M(r"R=\frac{T_1-T_2}{H}=\frac{L\,(e-1)}{kA_0}"))\
        .di("Para despejar hache, multiplicamos los dos lados por ka A cero, y dividimos entre ele por e menos uno.".replace("hache", dice("H", "hache")).replace("ka A cero", dice("kA₀", "ka A cero")).replace("ele por e menos uno", dice("L(e − 1)", "ele por e menos uno")),
            M(r"kA_0\,(T_1-T_2)=H\,L\,(e-1)"), R(r"H=\frac{kA_0\,(T_1-T_2)}{L\,(e-1)}", "ra"))
    L.paso("b) Comparar con la barra uniforme")\
        .di("La barra uniforme de área A cero sale directo de la hoja.".replace("A cero", dice("A₀", "A cero")), F(r"H=kA\,\frac{T_H-T_C}{L}"), M(r"H_0=\frac{kA_0\,(T_1-T_2)}{L}"))\
        .di("Dividimos: multiplicar por el inverso del de abajo. Todo se cancela menos e menos uno.".replace("e menos uno", dice("(e − 1)", "e menos uno")),
            M(r"\frac{H}{H_0}=\frac{kA_0(T_1-T_2)}{L\,(e-1)}\cdot\frac{L}{kA_0(T_1-T_2)}=\frac{1}{e-1}"))\
        .di("En la calculadora, e menos uno es uno coma siete uno ocho.", M(rf"\frac{{H}}{{H_0}}=\frac{{1}}{{{tex(e1, 4)}}}"), R(rf"\frac{{H}}{{H_0}}={tex(1 / e1, 3)}", "rb"))
    L.paso("c) Temperatura a lo largo")\
        .di("Integramos solo desde cero hasta una equis cualquiera, con la misma regla cuatro.".replace("una equis", "una " + dice("x", "equis")), V(foco="punto"),
            M(r"\int_0^xe^{x/L}dx=L\Big[e^{x/L}\Big]_0^x=L\left(e^{x/L}-1\right)"), M(r"T_1-T(x)=\frac{H}{kA_0}\,L\left(e^{x/L}-1\right)=\frac{HL}{kA_0}\left(e^{x/L}-1\right)"))\
        .di("De la parte a, hache por ele entre ka A cero es la caída total entre e menos uno.".replace("hache por ele entre ka A cero", dice("HL/(kA₀)", "hache por ele entre ka A cero")).replace("e menos uno", dice("(e − 1)", "e menos uno")),
            M(r"\frac{HL}{kA_0}=\frac{kA_0(T_1-T_2)}{L\,(e-1)}\cdot\frac{L}{kA_0}=\frac{T_1-T_2}{e-1}"))\
        .di("Lo ponemos, y te de equis pasa sumando.".replace("te de equis", dice("T(x)", "te de equis")),
            M(r"T_1-T(x)=(T_1-T_2)\,\frac{e^{x/L}-1}{e-1}"), R(r"T(x)=T_1-(T_1-T_2)\,\frac{e^{x/L}-1}{e-1}", "rc"))\
        .di("Revisión: en cero da te uno y en ele da te dos.".replace("te uno", dice("T₁", "te uno")).replace("en ele", "en " + dice("L", "ele")).replace("te dos", dice("T₂", "te dos")),
            M(r"x=0:\ e^0-1=0\qquad x=L:\ \frac{e-1}{e-1}=1"))
    L.paso("c) Dónde es mayor el gradiente")\
        .di("La pendiente es hache entre ka por el área, y el área es mínima al final.".replace("hache entre ka", dice("H/(kA)", "hache entre ka")),
            M(r"\left|\frac{dT}{dx}\right|=\frac{H}{kA(x)}=\frac{H}{kA_0}\,e^{x/L}"))\
        .di("En equis igual a ele es e veces mayor que al inicio.".replace("equis igual a ele", dice("x = L", "equis igual a ele")),
            M(rf"\frac{{|dT/dx|_{{x=L}}}}{{|dT/dx|_{{x=0}}}}=\exp(1)\approx{tex(math.e, 3)}"), R(r"\text{Mayor en }x=L\ \text{(el extremo más angosto)}", "rd"))
    L.responde("a", r"H=\frac{kA_0(T_1-T_2)}{L(e-1)}", None, "").responde("b", rf"H/H_0={tex(1 / e1, 3)}", 1 / e1, "")
    return L.usa(r"H=-kA\frac{dT}{dx}", r"H=kA\frac{\Delta T}{L}")


# ============================================================================ PR2-D11
@leccion("PR2-D11")
def pr2_d11() -> Leccion:
    nm, p1, p2, va, vb, qab = 3.0, 200e3, 80.0e3, 0.0300, 0.0750, 31.5e3
    nr = nm * 8.314
    dt_ab = p1 * (vb - va) / nr
    cp = qab / (nm * dt_ab)
    cv = cp - 8.314
    gamma = cp / cv
    g = Gas(nm, cv=cv)
    g.pone("A", p=p1, v=va)
    g.pone("B", p=p1, v=vb)
    g.pone("C", p=p2, v=vb)
    g.pone("D", p=p2, v=va)
    rab = g.proceso("A", "B", "isobarico")
    rbc = g.proceso("B", "C", "isocorico")
    rcd = g.proceso("C", "D", "isobarico")
    rda = g.proceso("D", "A", "isocorico")
    res = [("A→B", rab), ("B→C", rbc), ("C→D", rcd), ("D→A", rda)]
    wnet = (p1 - p2) * (vb - va)
    L = _des("PR2-D11", "Ciclo rectangular con información incompleta", "6.11", _f2("D11", 14),
             "Tres moles de gas ideal: $A\\to B$ expansión isobárica a $200$ kPa de $0{,}0300$ a $0{,}0750\\ \\text{m}^3$; $B\\to C$ enfriamiento isocórico hasta $80{,}0$ kPa; $C\\to D$ compresión isobárica hasta $V_A$; $D\\to A$ calentamiento isocórico. En $A\\to B$ absorbe $31{,}5$ kJ. a) Temperaturas y trabajo neto. b) $C_P$, $C_V$, $\\gamma$ y tipo de gas. c) Calor total absorbido y eficiencia.",
             claves=["ciclo rectangular", "hallar C_P", "práctica"],
             visual=visual_pv(g, [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "D", "isobarico"), ("D", "A", "isocorico")], uv="m³", up="kPa", fp=1e-3, valores=valores_visual(res)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un rectángulo en el diagrama: dos presiones y dos volúmenes. No sabemos el gas; el calor de A a B lo va a delatar.", DATO("31,5 kJ"),
            LISTA(["$n=3$ mol", "$P_1=200$ kPa, $P_2=80{,}0$ kPa", "$V_A=0{,}0300\\ \\text{m}^3$, $V_B=0{,}0750\\ \\text{m}^3$", "$Q_{AB}=31{,}5$ kJ"]))
    p = L.paso("a) Temperaturas de los cuatro estados")
    estado_T(p, g, "A", primero=True, voz="Despejamos la temperatura del gas ideal y la calculamos en A.")
    estado_T(p, g, "B")
    estado_T(p, g, "C")
    estado_T(p, g, "D")
    L.paso("a) Trabajo neto")\
        .di("Solo hay trabajo en los tramos a presión constante: el de la hoja. De A a B, a la presión alta.", F(r"W=p\,(V_2-V_1)"),
            M(rf"W_{{AB}}=(200\times10^3)(0{{,}}0750-0{{,}}0300)={_j(rab['W'])}\ \text{{J}}"))\
        .di("De C a D, a la presión baja: el volumen baja, así que el trabajo sale negativo.",
            M(rf"W_{{CD}}=(80{{,}}0\times10^3)(0{{,}}0300-0{{,}}0750)={_j(rcd['W'])}\ \text{{J}}"))\
        .di("Los de volumen constante no hacen trabajo. Sumamos: es el área del rectángulo.", M(r"W_{BC}=W_{DA}=0"),
            M(rf"W_{{\text{{neto}}}}={_j(rab['W'])}+({_j(rcd['W'])})={_j(wnet)}\ \text{{J}}"), R(rf"W_{{\text{{neto}}}}={_j(wnet)}\ \text{{J}}", "ra"))
    L.paso("b) C_P desde el calor de A a B")\
        .di("A presión constante el calor de la hoja va con ce pe.".replace("ce pe", dice("C_P", "ce pe")), F(r"Q=nC\,\Delta T"), M(r"Q_{AB}=nC_P\,(T_B-T_A)"))\
        .di("Despejamos ce pe: lo que lo multiplica pasa al otro lado dividiendo.".replace("ce pe", dice("C_P", "ce pe")), M(r"C_P=\frac{Q_{AB}}{n\,(T_B-T_A)}"))\
        .di("Sustituimos.", M(rf"C_P=\frac{{31\,500}}{{(3)({tex(dt_ab, 5)})}}={tex(cp, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))\
        .di("Con las otras dos casillas de la hoja sacamos ce ve y gama.".replace("ce ve y gama", dice("C_V y γ", "ce ve y gama")), F(r"C_P=C_V+R"), F(r"\gamma=\frac{C_P}{C_V}"),
            M(rf"C_V={tex(cp, 5)}-8{{,}}314={tex(cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"), M(rf"\gamma=\frac{{{tex(cp, 5)}}}{{{tex(cv, 5)}}}={tex(gamma, 3)}"))\
        .di("Cinco medios de erre y gama uno coma cuatro: es diatómico.".replace("gama", dice("γ", "gama")), F(r"C_V=\frac{5}{2}R"),
            R(rf"C_P={tex(cp, 3)}\quad C_V={tex(cv, 3)}\quad \gamma={tex(gamma, 3)}\ \Rightarrow\ \text{{diatómico}}", "rb"))
    explica_proceso(L.paso("c) B→C (volumen constante)"), g, "B", "C", "isocorico", foco="BC")
    explica_proceso(L.paso("c) C→D (presión constante)"), g, "C", "D", "isobarico", foco="CD")
    explica_proceso(L.paso("c) D→A (volumen constante)"), g, "D", "A", "isocorico", foco="DA")
    c = cierre_ciclo(L, res, titulo="c) Calor absorbido y eficiencia", id_e="rc")
    L.responde("a", rf"T_B={tex(g.t('B'), 4)}\ \text{{K}}", g.t("B"), "K").responde("a", rf"W={_j(wnet)}\ \text{{J}}", wnet, "J")\
     .responde("b", rf"C_P={tex(cp, 3)}", cp, "J/(mol·K)").responde("b", rf"\gamma={tex(gamma, 3)}", gamma, "")\
     .responde("c", rf"Q_H={_j(c['QH'])}\ \text{{J}}", c["QH"], "J").responde("c", rf"e={tex(c['e'], 3)}", c["e"], "")
    return L.usa(r"pV=nRT", r"W=p(V_2-V_1)", r"C_P=C_V+R", r"e=\frac{W}{Q_H}")


# ============================================================================ PR2-D12
@leccion("PR2-D12")
def pr2_d12() -> Leccion:
    ccal, mw, mh, pot, tmin, cw, lf = 150.0, 0.300, 0.120, 250.0, 3.50, 4186.0, 3.34e5
    dt = tmin * 60.0
    e = pot * dt
    q1 = mh * lf
    sobra = e - q1
    cap = (mw + mh) * cw + ccal
    q100 = cap * 100.0
    tf = sobra / cap
    L = _des("PR2-D12", "Calentador en un calorímetro con hielo: balance por etapas", "4.7", _f2("D12", 15),
             "Un calorímetro ($C_{cal}=150$ J/K) contiene $0{,}300$ kg de agua y $0{,}120$ kg de hielo a $0\\ ^\\circ$C. Un calentador entrega $250$ W durante $3{,}50$ min. $c_w=4186$ J/(kg·K), $L_f=3{,}34\\times10^5$ J/kg, $L_v=2{,}26\\times10^6$ J/kg. a) ¿Funde todo el hielo? ¿Llega a hervir? b) Temperatura final y masas de cada fase.",
             claves=["calentador", "por etapas", "fusión", "práctica"],
             visual={"tipo": "calor", "Tf": round(tf, 1),
                     "componentes": [{"n": "agua", "m": 300, "T": 0, "fase": "agua"}, {"n": "hielo", "m": 120, "T": 0, "fase": "hielo"}],
                     "necesita": [{"n": "fundir el hielo", "Q": q1}, {"n": "calentar de 0 a T", "Q": sobra}], "disponible": [{"n": "calentador", "Q": e}]})
    L.paso("Qué nos dan y qué piden")\
        .di("El calentador entrega energía a un ritmo fijo. Hay que gastarla por etapas: primero fundir, luego calentar.", DATO("250 W"),
            LISTA(["Calorímetro $C_{cal}=150$ J/K, a $0\\ ^\\circ$C", "Agua $0{,}300$ kg y hielo $0{,}120$ kg a $0\\ ^\\circ$C", "Calentador: $250$ W durante $3{,}50$ min"]))
    L.paso("Energía que entrega el calentador")\
        .di("Potencia es energía por tiempo, como la corriente de calor de la hoja.", F(r"H=\frac{dQ}{dt}\;\Rightarrow\;Q=H\,\Delta t"),
            M(r"\Delta t=(3{,}50)(60)=210\ \text{s}"), M(rf"Q=(250)(210)={_j(e)}\ \text{{J}}"))
    L.paso("a) Etapa 1: fundir todo el hielo")\
        .di("Calor latente de la hoja.", F(r"Q=\pm mL"), M(rf"Q_1=(0{{,}}120)(3{{,}}34\times10^5)={_j(q1)}\ \text{{J}}"))\
        .di("Alcanza y sobra energía.", M(rf"{_j(e)}>{_j(q1)}\;\Rightarrow\;\text{{sobra}}={_j(e)}-{_j(q1)}={_j(sobra)}\ \text{{J}}"))
    L.paso("a) Etapa 2: ¿llega a cien grados?")\
        .di("Ahora hay cero coma cuatro dos kilogramos de agua más el calorímetro. Calor sensible de la hoja.", F(r"Q=mc\,\Delta T"),
            M(rf"(m_w+m_h)c_w+C_{{cal}}=(0{{,}}420)(4186)+150={tex(cap, 6)}\ \text{{J/K}}"),
            M(rf"Q_{{100}}=({tex(cap, 6)})(100-0)={_j(q100, 6)}\ \text{{J}}"))\
        .di("Lo que sobra es mucho menos: no llega a hervir.", M(rf"{_j(sobra)}<{_j(q100, 6)}"), R(r"\text{Se funde todo el hielo y no llega a hervir}", "ra"))
    L.paso("b) Temperatura final y masas")\
        .di("La energía que sobró calienta el agua y el calorímetro desde cero.", M(rf"{_j(sobra)}=({tex(cap, 6)})(T_f-0)"),
            M(rf"T_f=\frac{{{_j(sobra)}}}{{{tex(cap, 6)}}}"), R(rf"T_f={tex(tf, 3)}\ {C}", "rb"))\
        .di("Al apagar el calentador no queda hielo ni vapor.", R(r"m_{\text{hielo}}=0\quad m_{\text{agua}}=0{,}420\ \text{kg}\quad m_{\text{vapor}}=0", "rb2"))
    L.responde("b", rf"T_f={tex(tf, 3)}\ {C}", tf, "°C")
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL", r"H=\frac{dQ}{dt}")
