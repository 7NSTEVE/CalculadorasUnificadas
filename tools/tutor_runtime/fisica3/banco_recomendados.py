"""Los 21 ejercicios recomendados de cátedra (III parcial, II semestre 2026)."""

from __future__ import annotations

import math

from .ayudas import (ATM, R_GAS, R_LATM, SIGMA, TX_R, Gas, cierre_ciclo, deriva_carnot, estado_T, explica_proceso, qtx, tabla_estados,
                     tabla_quw, valores_visual, visual_pv)
from .dsl import DATO, F, LISTA, M, MARCA, NOTA, OJO, Leccion, R, T, TABLA, V, s
from .formato import dice, n, nu, tex
from .registro import leccion


def _f(num: int, pag: int) -> dict:
    return {"doc": "recomendados", "nombre": "Ejercicios Recomendados 3", "num": num, "pag": pag}


# ============================================================================ E1
@leccion("E1")
def e1() -> Leccion:
    ac, aa, dif = 1.7e-5, 1.1e-5, 5.00
    Lc = dif / (ac / aa - 1)
    La = Lc + dif
    L = Leccion("E1", "Dos barras que siempre se llevan 5 cm", tema="4.4", nivel=3, fuente=_f(1, 1),
                claves=["dilatación lineal", "sistema de ecuaciones"],
                enunciado="Una barra de cobre mide $L_c$ y una de acero $L_a$, ambas a 10,0 °C. A cualquier temperatura la diferencia entre sus longitudes es 5,00 cm. Halle $L_c$ y $L_a$. $\\alpha_{acero}=1{,}1\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$, $\\alpha_{cobre}=1{,}7\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$.",
                visual={"tipo": "dilatacion", "caso": "barras", "u": "cm", "T0": 10, "dTmax": 300,
                        "barras": [{"n": "cobre", "L": round(Lc, 3), "a": ac, "color": "naranja"}, {"n": "acero", "L": round(La, 3), "a": aa, "color": "azul"}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Tenemos dos barras de distinto material. Lo raro es que su diferencia de largo nunca cambia, aunque las calentemos.",
            DATO("mantienen una diferencia"), LISTA(["Diferencia fija: $5{,}00$ cm a **cualquier** temperatura", "$\\alpha_c = 1{,}7\\times10^{-5}$ (cobre, se dilata más)", "$\\alpha_a = 1{,}1\\times10^{-5}$ (acero, se dilata menos)"]))\
        .di("Buscamos los dos largos a diez grados.", M(r"L_c = \;?\qquad L_a = \;?"))
    L.paso("La idea: las dos crecen lo mismo")\
        .di("Si la diferencia no cambia, es porque las dos barras se alargan exactamente lo mismo.",
            V(foco="calentar"), M(r"\Delta L_c = \Delta L_a", "ig"))\
        .di("Usamos la dilatación lineal del formulario para cada barra.", F(r"\Delta L = \alpha L_0\,\Delta T"))\
        .por_que("Si una creciera más que la otra, la distancia entre las puntas cambiaría con la temperatura. Como el enunciado dice que no cambia nunca, los alargamientos tienen que ser iguales.")\
        .pregunta_rapida("¿Cuál barra tiene que ser la más larga?", ["La de cobre", "La de acero"], 1, "El acero se dilata menos por cada centímetro; para crecer lo mismo necesita más centímetros.")
    L.paso("Escribir la igualdad")\
        .di("Cambiamos cada alargamiento por su fórmula.", M(r"\alpha_c L_c\,\Delta T = \alpha_a L_a\,\Delta T", "e1"))\
        .di(f"El cambio de temperatura {s('ΔT')} es el mismo en los dos lados, se cancela.",
            M(r"\alpha_c L_c\,\cancel{\Delta T} = \alpha_a L_a\,\cancel{\Delta T}"))\
        .di("Queda una relación entre los largos, sin temperatura.", M(r"\alpha_c L_c = \alpha_a L_a", "rel", "azul"))\
        .sale_de(("$\\Delta T$ se va", "está multiplicando a ambos lados y es el mismo para las dos barras"))
    L.paso("Despejar el largo del acero")\
        .di("Despejamos el largo del acero: alfa del acero pasa a dividir.", M(r"L_a = \frac{\alpha_c}{\alpha_a}\,L_c"))\
        .di("Ponemos los números; las potencias de diez se cancelan.", M(r"L_a = \frac{1{,}7\times10^{-5}}{1{,}1\times10^{-5}}\,L_c"))\
        .di(f"Diecisiete onceavos, o sea {n(ac / aa, 4)}.", M(rf"L_a = \frac{{17}}{{11}}\,L_c = {tex(ac / aa, 4)}\,L_c", "la"))\
        .despacio((r"\frac{1{,}7\times10^{-5}}{1{,}1\times10^{-5}}=\frac{1{,}7}{1{,}1}=\frac{17}{11}", "arriba y abajo tienen el mismo $10^{-5}$"))
    L.paso("Usar la diferencia de 5 cm")\
        .di("La de acero es la más larga, así que la resta es acero menos cobre.", M(r"L_a - L_c = 5{,}00\ \text{cm}", "dif"))\
        .di("Sustituimos lo que acabamos de hallar.", M(r"\frac{17}{11}L_c - L_c = 5{,}00\ \text{cm}"))\
        .di("Diecisiete onceavos menos once onceavos son seis onceavos.", M(r"\frac{6}{11}L_c = 5{,}00\ \text{cm}"))\
        .di("Multiplicamos por once y dividimos entre seis.", M(r"L_c = \frac{11\,(5{,}00)}{6}"))\
        .despacio((r"\frac{17}{11}L_c-\frac{11}{11}L_c=\frac{17-11}{11}L_c", "un $L_c$ entero es $\\frac{11}{11}L_c$"))
    L.paso("Resultado")\
        .di(f"El cobre mide {nu(Lc, 'cm', 3)}.", R(rf"L_c = {tex(Lc, 3, u='cm')}"))\
        .di(f"Y el acero, cinco más: {nu(La, 'cm', 3)}.", R(rf"L_a = {tex(Lc, 3)}+5{{,}}00 = {tex(La, 3, u='cm')}"))\
        .di("Comprobación rápida: alfa por largo da lo mismo en las dos.",
            NOTA(rf"$\alpha_cL_c = {tex(ac * Lc * 1e5, 4)}\times10^{{-5}}$ y $\alpha_aL_a = {tex(aa * La * 1e5, 4)}\times10^{{-5}}$. Coinciden."))
    L.responde("", rf"L_c={tex(Lc, 3)}\ \text{{cm}}", Lc, "cm").responde("", rf"L_a={tex(La, 3)}\ \text{{cm}}", La, "cm")
    L.usa(r"\Delta L=\alpha L_0\Delta T")
    L.te_toca("Dos barras de aluminio ($\\alpha=2{,}4\\times10^{-5}$) y acero ($\\alpha=1{,}2\\times10^{-5}$) mantienen siempre 6,00 cm de diferencia. ¿Cuánto mide la de aluminio?", 6.0, "cm", pista="La razón entre los alfas es 2, así que el acero mide el doble.", solucion="$L_{ac}=2L_{al}$ y $2L_{al}-L_{al}=6$, entonces $L_{al}=6{,}00$ cm.")
    return L


# ============================================================================ E2
def _lata(id: str, fuente: dict) -> Leccion:
    Vl, aal, bw, dT = 400.0, 2.4e-5, 2.1e-4, 60.0
    bal = 3 * aal
    Vcan = Vl * (1 + bal * dT)
    Vag = Vcan / (1 + bw * dT)
    vac = Vl - Vag
    aprox = Vl * (bw - bal) * dT
    L = Leccion(id, "Espacio vacío en una lata de gaseosa", tema="4.4", nivel=4, fuente=fuente, claves=["dilatación volumétrica", "líquido en recipiente"],
                enunciado="Una lata de aluminio de 400,000 mL se llena a 5,0 °C con un líquido que se comporta como agua. ¿Qué volumen hay que dejar vacío para que no explote a 65,0 °C? $\\alpha_{Al}=2{,}4\\times10^{-5}\\ \\text{K}^{-1}$, $\\beta_{agua}=2{,}1\\times10^{-4}\\ \\text{K}^{-1}$.",
                visual={"tipo": "dilatacion", "caso": "lata", "V": Vl, "bc": bal, "bl": bw, "T0": 5, "T1": 65, "Vliq": round(Vag, 3), "u": "mL"})
    L.paso("Qué nos dan y qué piden")\
        .di("Una lata de aluminio y agua adentro. Al calentarse crecen las dos, pero el agua crece más.",
            DATO("400"), LISTA(["Lata: $V_0=400{,}000$ mL a $5{,}0\\ ^\\circ\\text{C}$", "$\\alpha_{Al}=2{,}4\\times10^{-5}\\ \\text{K}^{-1}$", "$\\beta_{agua}=2{,}1\\times10^{-4}\\ \\text{K}^{-1}$", "Temperatura máxima: $65{,}0\\ ^\\circ\\text{C}$"]))\
        .di("Hay que hallar cuánto espacio dejar vacío al enlatar.", M(r"V_{\text{vacío}} = V_{\text{lata}} - V_{\text{agua}} = \;?", "pide"))
    L.paso("La idea: a 65 °C el agua llena justo la lata")\
        .di("El caso límite es que a sesenta y cinco grados el agua llene la lata exactamente, sin sobrar ni faltar.",
            V(foco="calentar"), M(r"V_{\text{agua}}(65\,^\circ\text{C}) = V_{\text{lata}}(65\,^\circ\text{C})", "cond", "azul"))\
        .di("Primero, el cambio de temperatura.", M(r"\Delta T = 65{,}0-5{,}0 = 60{,}0\ \text{K}"))\
        .por_que("Si dejamos menos espacio, el agua no cabe y revienta la lata. Si dejamos más, sobra. El punto justo es que se igualen a la temperatura máxima.")
    L.paso("Beta de la lata")\
        .di(f"Nos dieron {s('α')} del aluminio, pero el volumen usa {s('β')}. Partimos de las dos ecuaciones de dilatación del formulario.",
            F(r"\Delta L = \alpha L_0\,\Delta T \;\Rightarrow\; V = V_0(1+\alpha\Delta T)^3 \approx V_0(1+3\alpha\Delta T)"),
            F(r"\Delta V=\beta V_0\,\Delta T"))\
        .di("¿De dónde sale el tres? Se abre el cubo: los términos con alfa delta te al cuadrado o al cubo son pequeñísimos y se desprecian.".replace("alfa delta te", dice("αΔT", "alfa delta te")),
            M(r"(1+\alpha\Delta T)^3=1+3\alpha\Delta T+3(\alpha\Delta T)^2+(\alpha\Delta T)^3\approx 1+3\alpha\Delta T"),
            M(r"\alpha\Delta T=(2{,}4\times10^{-5})(60)=1{,}44\times10^{-3}\;\Rightarrow\;(\alpha\Delta T)^2\approx 2\times10^{-6}\ \text{(despreciable)}"))\
        .di("Comparando las dos expresiones, en un sólido isotrópico beta es tres veces alfa.", M(r"\beta_{Al}=3\alpha_{Al}=3\,(2{,}4\times10^{-5})"), M(rf"\beta_{{Al}}={tex(bal, 2)}\ \text{{K}}^{{-1}}", "bal"))\
        .di("La lata es un hueco, pero el hueco se dilata como si fuera macizo.", NOTA("El hueco crece igual que el material: mismo $\\beta$ del aluminio."))\
        .por_que("Un volumen tiene largo, ancho y alto; cada uno crece con $\\Delta L=\\alpha L_0\\Delta T$. Al expandir $(1+\\alpha\\Delta T)^3\\approx 1+3\\alpha\\Delta T$ sale $\\beta=3\\alpha$.")\
        .sale_de(("$3\\alpha$", "al expandir $(1+\\alpha\\Delta T)^3 \\approx 1+3\\alpha\\Delta T$ desde $\\Delta L=\\alpha L_0\\Delta T$"))
    L.paso("Volumen de la lata caliente")\
        .di("Volumen final: el inicial más lo que creció.", M(r"V = V_0\,(1+\beta\,\Delta T)"))\
        .di("Para la lata.", M(r"V_{\text{lata}} = 400\,\left[1+(7{,}2\times10^{-5})(60)\right]"))\
        .di(f"Da {nu(Vcan, 'mL', 6)}.", M(rf"V_{{\text{{lata}}}} = 400\,({tex(1 + bal * dT, dec=5)}) = {tex(Vcan, 6, u='mL')}", "vlata"))
    L.paso("Volumen de agua que puede entrar")\
        .di("Esa misma cantidad es el agua ya caliente. La escribimos con su volumen frío, que es la incógnita.", M(r"V_{\text{agua},0}\,(1+\beta_{\text{agua}}\,\Delta T) = V_{\text{lata}}"))\
        .di("Despejamos dividiendo.", M(r"V_{\text{agua},0} = \frac{V_{\text{lata}}}{1+\beta_{\text{agua}}\,\Delta T}"))\
        .di("Sustituimos.", M(rf"V_{{\text{{agua}},0}} = \frac{{{tex(Vcan, 6)}}}{{1+(2{{,}}1\times10^{{-4}})(60)}} = \frac{{{tex(Vcan, 6)}}}{{{tex(1 + bw * dT, dec=4)}}}"))\
        .di(f"Caben {nu(Vag, 'mL', 6)} de agua fría.", M(rf"V_{{\text{{agua}},0}} = {tex(Vag, 6, u='mL')}", "vag"))
    L.paso("El espacio vacío")\
        .di("Restamos.", M(rf"V_{{\text{{vacío}}}} = 400{{,}}000-{tex(Vag, 6)}"), V(foco="vacio"))\
        .di(f"Hay que dejar {nu(vac, 'mL', 3)} sin llenar.", R(rf"V_{{\text{{vacío}}}} = {tex(vac, 3, u='mL')}"))\
        .di("Cuidado con el atajo: restar las dilataciones de cuatrocientos da un poquito más.",
            OJO(rf"Atajo aproximado: $400(\beta_{{agua}}-\beta_{{Al}})\Delta T = {tex(aprox, 3)}$ mL. Supone la lata llena de agua desde el inicio; el valor exacto es ${tex(vac, 3)}$ mL."))
    L.responde("", rf"V_{{\text{{vacío}}}}={tex(vac, 3)}\ \text{{mL}}", vac, "mL")
    L.usa(r"\Delta L=\alpha L_0\Delta T", r"\Delta V=\beta V_0\Delta T")
    return L


@leccion("E2")
def e2() -> Leccion:
    return _lata("E2", _f(2, 1))


# ============================================================================ E3
@leccion("E3")
def e3() -> Leccion:
    Lt, al, dT = 250.0, 12e-6, 20.0
    l0 = Lt / 2
    l1 = l0 * (1 + al * dT)
    y = math.sqrt(l1 * l1 - l0 * l0)
    L = Leccion("E3", "Puente que se pandea por el calor", tema="4.4", nivel=3, fuente=_f(3, 1), claves=["dilatación lineal", "Pitágoras", "pandeo"],
                enunciado="Dos losas idénticas de concreto de un puente de 250 m se colocan unidas, sin espacio para expandirse. Si la temperatura sube 20,0 °C, ¿a qué altura $y$ se elevan al pandearse? $\\alpha=12\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$.",
                visual={"tipo": "dilatacion", "caso": "pandeo", "L": Lt, "a": al, "dT": dT, "y": round(y, 3)})
    L.paso("Qué nos dan y qué piden")\
        .di("Dos losas iguales, juntas, entre paredes que no se mueven. Al calentarse no tienen para dónde crecer y se levantan.",
            DATO("250 m"), LISTA(["Largo total: $250$ m, dos losas de $125$ m", "$\\Delta T = 20{,}0\\ ^\\circ\\text{C}$", "$\\alpha = 12\\times10^{-6}\\ ^\\circ\\text{C}^{-1}$"]))\
        .di("Piden la altura del pico.", M(r"y = \;?"))
    L.paso("La idea: media figura es un triángulo")\
        .di("Miramos solo una losa. La base sigue midiendo ciento veinticinco metros, porque las paredes no se mueven.",
            V(foco="triangulo"), M(r"\text{base} = \frac{250}{2} = 125\ \text{m}", "base"))\
        .di("La losa dilatada es la hipotenusa, y la altura es el otro cateto.", M(r"L^2 = 125^2 + y^2", "pit", "azul"))\
        .por_que("La losa se inclinó: su largo nuevo va en diagonal, la base quedó igual y la altura completa el triángulo rectángulo. Por eso sirve Pitágoras.")
    L.paso("Largo nuevo de una losa")\
        .di("Dilatación lineal de una losa.", F(r"\Delta L=\alpha L_0\,\Delta T"))\
        .di("Sustituimos.", M(r"\Delta L=(12\times10^{-6})(125)(20) = 0{,}030\ \text{m}", "dl"))\
        .di("Creció apenas tres centímetros.", M(r"L = 125+0{,}030 = 125{,}030\ \text{m}", "ln"), V(foco="crece"))
    L.paso("Despejar la altura")\
        .di("De Pitágoras, despejamos ye.", M(r"y^2 = L^2-125^2"), M(r"y = \sqrt{L^2-125^2}"))\
        .di("Ponemos los números.", M(r"y = \sqrt{125{,}030^2-125^2}"))\
        .di("Adentro de la raíz queda siete coma cinco.", M(rf"y = \sqrt{{{tex(l1 * l1, dec=4)}-15\,625}} = \sqrt{{{tex(l1 * l1 - l0 * l0, dec=4)}}}"))\
        .despacio((rf"125{{,}}030^2 = {tex(l1 * l1, dec=4)}", "no redondees antes de restar: la diferencia es pequeña"), (r"125^2 = 15\,625", ""))
    L.paso("Resultado")\
        .di(f"El puente se levanta {nu(y, 'm', 3)}.", R(rf"y = {tex(y, 3, u='m')}"), V(foco="altura"))\
        .di("Sorprende: creció tres centímetros y subió casi tres metros. Es porque el triángulo es muy aplastado.",
            OJO("Error típico: usar 250 m como base o redondear $125{,}030^2$ antes de restar."))
    L.responde("", rf"y={tex(y, 3)}\ \text{{m}}", y, "m").usa(r"\Delta L=\alpha L_0\Delta T")
    L.te_toca("El mismo puente, pero la temperatura sube solo 10,0 °C. ¿Cuánto mide $y$?", math.sqrt((125 * (1 + al * 10)) ** 2 - 125 ** 2), "m", pista="$\\Delta L=0{,}015$ m por losa.", solucion="$y=\\sqrt{125{,}015^2-125^2}=1{,}94$ m")
    return L


# ============================================================================ E4
def _cinta(id: str, fuente: dict, aviso: str = "") -> Leccion:
    Lb, ab, aa, dT = 80.00, 1.9e-5, 1.1e-5, -30.0
    La = Lb * (1 + ab * dT)
    cm = 1.0 * (1 + aa * dT)
    Lm = La / cm
    L = Leccion(id, "Medir una barra fría con una cinta fría", tema="4.4", nivel=4, fuente=fuente, aviso=aviso, claves=["dilatación lineal", "cinta métrica"],
                enunciado="Una barra de latón mide 80,00 cm con una cinta de acero, ambas a 20,00 °C (calibración de la cinta). a) Longitud de la barra a −10,00 °C medida con la cinta a 20,00 °C. b) Separación de las marcas de 1 cm si la cinta está a −10,00 °C. c) Longitud de la barra a −10,00 °C medida con la cinta a −10,00 °C. ($\\alpha_{La}=1{,}9\\times10^{-5}$, $\\alpha_{ac}=1{,}1\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$)",
                visual={"tipo": "dilatacion", "caso": "cinta", "L": Lb, "ab": ab, "aa": aa, "T0": 20, "T1": -10})
    L.paso("Qué nos dan y qué piden")\
        .di("Hay dos cosas que se encogen con el frío: la barra de latón y la cinta de acero. La cinta mide bien solo a veinte grados.",
            DATO("80,00 cm"), LISTA(["Barra: $80{,}00$ cm a $20{,}00\\ ^\\circ\\text{C}$", "Latón: $\\alpha_{La}=1{,}9\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$", "Acero: $\\alpha_{ac}=1{,}1\\times10^{-5}\\ ^\\circ\\text{C}^{-1}$"]))\
        .di("El cambio de temperatura es de veinte a menos diez: baja treinta grados.", M(r"\Delta T = -10-20 = -30\ ^\circ\text{C}", "dt"))
    L.paso("a) Solo se enfría la barra")\
        .di("La cinta sigue a veinte grados, o sea que mide correctamente. Calculamos cuánto se encoge el latón.", V(foco="barra"), F(r"\Delta L=\alpha L_0\,\Delta T"))\
        .di("Sustituimos, con el menos del frío.", M(rf"\Delta L=(1{{,}}9\times10^{{-5}})(80{{,}}00)(-30) = {tex(Lb * ab * dT, dec=4, u='cm')}"))\
        .di(f"La barra queda en {nu(La, 'cm', dec=2)}.", R(rf"L = 80{{,}}00-{tex(-Lb * ab * dT, dec=4)} = {tex(La, dec=2, u='cm')}", "ra"))
    L.paso("b) Cuánto se juntan las marcas de la cinta")\
        .di("Ahora la cinta también se enfría: cada marca de un centímetro se encoge como acero.", V(foco="cinta"), M(r"\ell = \ell_0\,(1+\alpha_{ac}\,\Delta T)"))\
        .di("Un centímetro se convierte en esto.", M(r"\ell = 1{,}00\,\left[1+(1{,}1\times10^{-5})(-30)\right]"))\
        .di("Primero el producto: uno coma uno por diez a la menos cinco, por menos treinta, da menos cero coma cero cero cero treinta y tres.", M(r"(1{,}1\times10^{-5})(-30)=-3{,}3\times10^{-4}=-0{,}00033"), M(r"\ell=1{,}00\,(1-0{,}00033)"))\
        .di("Un pelito menos que un centímetro.", R(rf"\ell = {tex(cm, dec=5, u='cm')}", "rb"))
    L.paso("c) Barra fría medida con cinta fría")\
        .di("La lectura es cuántas marcas caben en la barra: largo real entre separación real.", V(foco="lectura"), M(r"\text{lectura} = \frac{L}{\ell}", "lec", "azul"))\
        .di("Dividimos los dos resultados anteriores.", M(rf"\text{{lectura}} = \frac{{{tex(La, dec=4)}\ \text{{cm}}}}{{{tex(cm, dec=5)}}}"))\
        .di(f"La cinta fría marca {nu(Lm, 'cm', dec=2)}.", R(rf"\text{{lectura}} = {tex(Lm, dec=2, u='cm')}", "rc"))\
        .por_que("La cinta está encogida, sus marcas están más juntas, entonces caben más marcas y la lectura sube un poquito respecto al largo real.")
    L.paso("Cómo recordarlo")\
        .di("Objeto y regla se encogen los dos: se divide el largo real entre lo que mide de verdad cada división.",
            TABLA(["Inciso", "Qué se enfría", "Resultado"], [["a", "barra", f"${tex(La, dec=2)}$ cm"], ["b", "cinta", f"${tex(cm, dec=5)}$ cm"], ["c", "las dos", f"${tex(Lm, dec=2)}$ cm"]]),
            OJO("Error típico: en c) multiplicar por $\\ell$ en vez de dividir."))
    L.responde("a", rf"{tex(La, dec=2)}\ \text{{cm}}", La, "cm", 0.0005).responde("b", rf"{tex(cm, dec=5)}\ \text{{cm}}", cm, "cm", 0.0001).responde("c", rf"{tex(Lm, dec=2)}\ \text{{cm}}", Lm, "cm", 0.0005)
    L.usa(r"\Delta L=\alpha L_0\Delta T")
    return L


@leccion("E4")
def e4() -> Leccion:
    return _cinta("E4", _f(4, 1), "El PDF de recomendados no trae los coeficientes. Se usan los del folleto, donde aparece este mismo ejercicio: latón 1,9×10⁻⁵ y acero 1,1×10⁻⁵ °C⁻¹.")


# ============================================================================ E5
def _cubo_frio(id: str, fuente: dict, aviso: str = "", cagua=4186.0, ccu=387.0, Lv=2.26e6) -> Leccion:
    mc, mv, ma, Tc = 0.0500, 0.3000, 0.2000, -79.0
    Q = mc * ccu * (100 - Tc)
    mcond = Q / Lv
    disp = mv * Lv
    L = Leccion(id, "Cubo de cobre helado dentro de vapor y agua", tema="4.8", nivel=4, fuente=fuente, aviso=aviso, claves=["calorimetría", "condensación", "calor latente"],
                enunciado="Un recipiente de aluminio de 50,0 g, aislado, contiene en equilibrio 200,0 g de agua y 300,0 g de vapor de agua. Se introduce un cubo de cobre de 50,0 g a −79,0 °C. a) Temperatura final. b) Composición final.",
                visual={"tipo": "calor", "Tf": 100, "titulo": "¿Quién cede y quién recibe?",
                        "componentes": [{"n": "vapor", "m": 300, "T": 100, "fase": "vapor"}, {"n": "agua", "m": 200, "T": 100, "fase": "agua"}, {"n": "aluminio", "m": 50, "T": 100, "fase": "solido"}, {"n": "cobre", "m": 50, "T": -79, "fase": "frio"}],
                        "necesita": [{"n": "calentar el cobre a 100 °C", "Q": Q}], "disponible": [{"n": "condensar todo el vapor", "Q": disp}], "final": [{"n": "agua", "m": round(200 + mcond * 1000, 2)}, {"n": "vapor", "m": round(300 - mcond * 1000, 2)}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Agua líquida y vapor conviviendo en equilibrio. Eso solo pasa a cien grados: es el dato escondido.",
            DATO("equilibrio"), DATO("200,0 g de agua"), LISTA(["Recipiente Al: $50{,}0$ g", "Agua: $200{,}0$ g y vapor: $300{,}0$ g, **juntos en equilibrio**", "Cobre: $50{,}0$ g a $-79{,}0\\ ^\\circ\\text{C}$"]))\
        .di("Como conviven líquido y vapor, recipiente, agua y vapor están a cien grados.", M(r"T_{\text{agua}}=T_{\text{vapor}}=T_{\text{Al}}=100\ ^\circ\text{C}", "t100", "azul"))\
        .sale_de(("$100\\ ^\\circ\\text{C}$", "a 1 atm, agua líquida y vapor solo coexisten a la temperatura de ebullición"))
    L.paso("La idea: ¿alcanza el vapor?")\
        .di("El cobre está helado y pide calor. Se lo da el vapor al condensarse, sin cambiar de temperatura.", V(foco="flujo"), F(r"Q=mc\,\Delta T\qquad Q=\pm mL"))\
        .di("La pregunta clave es si todo el vapor alcanza, o sobra.", T("Comparamos: calor que **pide** el cobre contra calor que **puede dar** el vapor."))\
        .por_que("Antes de plantear una ecuación hay que saber si el vapor se acaba. Si sobra vapor, la temperatura final se queda en 100 °C y no hay nada que despejar.")
    L.paso("Calor que pide el cobre")\
        .di("Para subir de menos setenta y nueve a cien grados.", M(r"\Delta T = 100-(-79) = 179\ ^\circ\text{C}"))\
        .di("Sustituimos en kilogramos.", M(rf"Q_{{Cu}} = (0{{,}}0500)({tex(ccu, 3)})(179)"))\
        .di(f"El cobre pide {nu(Q, 'J', 4)}.", M(rf"Q_{{Cu}} = {qtx(Q)}", "qcu", "rojo"))\
        .despacio((r"m_{Cu}=50{,}0\ \text{g}=0{,}0500\ \text{kg}", "el calor específico está por kilogramo"))
    L.paso("Calor que puede dar el vapor")\
        .di("Si se condensara todo.", M(rf"Q_{{\text{{máx}}}} = m_vL_v = (0{{,}}3000)({tex(Lv, 4, recorta=True)})"))\
        .di("Puede dar muchísimo más de lo que se pide.", M(rf"Q_{{\text{{máx}}}} = {tex(disp, 3, u='J')} \gg {tex(Q, 4)}\ \text{{J}}", "qm"), V(foco="comparar"))\
        .di("Sobra vapor, entonces la temperatura final es cien grados.", R(r"T_f = 100\ ^\circ\text{C}", "ra"))\
        .pregunta_rapida("¿Por qué el recipiente de aluminio no entra en la cuenta?", ["Porque ya está a 100 °C y termina a 100 °C", "Porque el aluminio no absorbe calor"], 0, "Su temperatura no cambia, entonces $\\Delta T=0$ y su calor es cero.")
    L.paso("Cuánto vapor se condensa")\
        .di("Solo se condensa lo necesario para cubrir lo del cobre.", M(r"m_{\text{cond}}L_v = Q_{Cu}"))\
        .di("Despejamos la masa.", M(rf"m_{{\text{{cond}}}} = \frac{{Q_{{Cu}}}}{{L_v}} = \frac{{{tex(Q, 4)}}}{{{tex(Lv, 4, recorta=True)}}}"))\
        .di(f"Es poquito: {nu(mcond * 1000, 'g', 3)}.", M(rf"m_{{\text{{cond}}}} = {tex(mcond, 4, u='kg')} = {tex(mcond * 1000, 3, u='g')}", "mc"))
    L.paso("Composición final")\
        .di("Ese vapor ahora es agua: lo sumamos al agua y lo quitamos del vapor.",
            M(rf"m_{{\text{{agua}}}}=200{{,}}0+{tex(mcond * 1000, 3)}={tex(200 + mcond * 1000, dec=2)}\ \text{{g}}\qquad m_{{\text{{vapor}}}}=300{{,}}0-{tex(mcond * 1000, 3)}={tex(300 - mcond * 1000, dec=2)}\ \text{{g}}"),
            TABLA(["Sustancia", "Antes", "Después"], [["agua", "$200{,}0$ g", f"${tex(200 + mcond * 1000, dec=2)}$ g"], ["vapor", "$300{,}0$ g", f"${tex(300 - mcond * 1000, dec=2)}$ g"]], resaltar=[[0, 2], [1, 2]]), V(foco="final"))\
        .di("No queda hielo: nada llegó a cero grados.", OJO("Error típico: plantear $\\sum Q=0$ con $T_f$ de incógnita sin revisar si sobra vapor."))
    L.responde("a", r"T_f=100\ ^\circ\text{C}", 100.0, "°C").responde("b", rf"\text{{agua }}{tex(200 + mcond * 1000, dec=2)}\ \text{{g}}", 200 + mcond * 1000, "g").responde("b", rf"\text{{vapor }}{tex(300 - mcond * 1000, dec=2)}\ \text{{g}}", 300 - mcond * 1000, "g")
    L.usa(r"Q=mc\Delta T", r"Q=\pm mL")
    return L


@leccion("E5")
def e5() -> Leccion:
    return _cubo_frio("E5", _f(5, 2))


# ============================================================================ E6
@leccion("E6")
def e6() -> Leccion:
    mh, ma, mv, Ta = 0.100, 0.150, 0.125, 45.0
    Lf, Lv, c = 3.34e5, 2.256e6, 4190.0
    q1, q2, q3 = mh * Lf, mh * c * 100, ma * c * (100 - Ta)
    tot = q1 + q2 + q3
    disp = mv * Lv
    mcond = tot / Lv
    L = Leccion("E6", "Hielo, agua y vapor en un mismo recipiente", tema="4.8", nivel=4, fuente=_f(6, 2), claves=["calorimetría", "cambio de fase"],
                enunciado="En un sistema aislado se mezclan 100,0 g de hielo a 0,0 °C, 150,0 g de agua a 45 °C y 125,0 g de vapor a 100,0 °C. a) Temperatura de equilibrio. b) Cantidad final de hielo, agua y vapor. $L_f=3{,}34\\times10^5$, $L_v=2{,}256\\times10^6$ J/kg, $c=4190$ J/(kg·K).",
                visual={"tipo": "calor", "Tf": 100, "componentes": [{"n": "hielo", "m": 100, "T": 0, "fase": "hielo"}, {"n": "agua", "m": 150, "T": 45, "fase": "agua"}, {"n": "vapor", "m": 125, "T": 100, "fase": "vapor"}],
                        "necesita": [{"n": "fundir el hielo", "Q": q1}, {"n": "esa agua de 0 a 100 °C", "Q": q2}, {"n": "agua de 45 a 100 °C", "Q": q3}], "disponible": [{"n": "condensar el vapor", "Q": disp}],
                        "final": [{"n": "agua", "m": round((mh + ma + mcond) * 1000, 1)}, {"n": "vapor", "m": round((mv - mcond) * 1000, 1)}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Tres cosas a distinta temperatura dentro de un sistema aislado.",
            DATO("100,0 g de hielo"), LISTA(["Hielo: $100{,}0$ g a $0\\ ^\\circ\\text{C}$", "Agua: $150{,}0$ g a $45\\ ^\\circ\\text{C}$", "Vapor: $125{,}0$ g a $100\\ ^\\circ\\text{C}$"]))\
        .di("No sabemos si al final queda hielo, agua, vapor o mezcla. Eso se averigua comparando energías.", M(r"T_f=\;?"))
    L.paso("La idea: la prueba de los 100 °C")\
        .di("Suponemos que todo llega a cien grados y preguntamos si el vapor alcanza a pagarlo.", V(foco="flujo"), T("Pide calor: el hielo y el agua. Da calor: el vapor al condensarse."))\
        .por_que("El vapor tiene un calor latente enorme. Casi siempre conviene probar primero si alcanza para llevar todo a 100 °C; si sobra vapor, el problema se acaba ahí.")
    L.paso("1) Fundir el hielo")\
        .di("El hielo ya está a cero grados, así que solo hay que fundirlo.", F(r"Q=mL_f"), V(etapa=0))\
        .di("Sustituimos.", M(r"Q_1=(0{,}1000)(3{,}34\times10^{5})"), M(rf"Q_1={tex(q1, 3, u='J', sci='no')}", "q1", "rojo"))
    L.paso("2) Calentar esa agua de 0 a 100 °C")\
        .di("El hielo ya es agua a cero grados; ahora sube a cien.", M(r"Q_2=(0{,}1000)(4190)(100-0)"), V(etapa=1))\
        .di("Da esto.", M(rf"Q_2={tex(q2, 3, u='J', sci='no')}", "q2", "rojo"))
    L.paso("3) Calentar el agua de 45 a 100 °C")\
        .di("El agua que ya era líquida sube cincuenta y cinco grados.", M(r"Q_3=(0{,}1500)(4190)(100-45)"), V(etapa=2))\
        .di("Da esto.", M(rf"Q_3={tex(q3, 5, u='J', sci='no')}", "q3", "rojo"))
    L.paso("Comparar con lo que da el vapor")\
        .di("Sumamos todo lo que se pide.", M(rf"Q_{{\text{{pide}}}}=Q_1+Q_2+Q_3"), M(rf"Q_{{\text{{pide}}}}={tex(q1, 5, sci='no')}+{tex(q2, 5, sci='no')}+{tex(q3, 5, sci='no')}={tex(tot, 6, u='J', sci='no')}", "pide"))\
        .di("Y lo máximo que da el vapor.", M(rf"Q_{{\text{{vapor}}}}=m_vL_v=(0{{,}}1250)(2{{,}}256\times10^6)={tex(disp, 4, u='J')}", "da"), V(foco="comparar"))\
        .di("El vapor puede dar más de lo que se pide: sobra vapor y todo queda a cien grados.", R(r"T_f=100\ ^\circ\text{C}", "ra"))
    L.paso("Cuánto vapor se condensa")\
        .di("Lo justo para pagar lo que se pidió.", M(r"m_{\text{cond}}=\frac{Q_{\text{pide}}}{L_v}"))\
        .di("Dividimos.", M(rf"m_{{\text{{cond}}}}=\frac{{{tex(tot, 6, sci='no')}}}{{2{{,}}256\times10^6}}={tex(mcond, 4, u='kg')}={tex(mcond * 1000, 4, u='g')}", "mc"))
    L.paso("Composición final")\
        .di("El agua final es el hielo fundido, más el agua que había, más el vapor condensado.",
            M(rf"m_{{\text{{agua}}}}=100{{,}}0+150{{,}}0+{tex(mcond * 1000, dec=1)}={tex((mh + ma + mcond) * 1000, dec=1, u='g')}", "fa"))\
        .di("Y el vapor que sobró.", M(rf"m_{{\text{{vapor}}}}=125{{,}}0-{tex(mcond * 1000, dec=1)}={tex((mv - mcond) * 1000, dec=1, u='g')}", "fv"), V(foco="final"))\
        .di("Queda resumido.", TABLA(["Hielo", "Agua", "Vapor"], [["$0$ g", f"${tex((mh + ma + mcond) * 1000, dec=1)}$ g", f"${tex((mv - mcond) * 1000, dec=1)}$ g"]], resaltar=[[0, None]]),
            NOTA(rf"Comprobación: $0+{tex((mh + ma + mcond) * 1000, dec=1)}+{tex((mv - mcond) * 1000, dec=1)}=375{{,}}0$ g, la misma masa del inicio."))
    L.responde("a", r"T_f=100\ ^\circ\text{C}", 100.0, "°C").responde("b", rf"\text{{agua }}{tex((mh + ma + mcond) * 1000, dec=1)}\ \text{{g}}", (mh + ma + mcond) * 1000, "g").responde("b", rf"\text{{vapor }}{tex((mv - mcond) * 1000, dec=1)}\ \text{{g}}", (mv - mcond) * 1000, "g")
    L.usa(r"Q=mc\Delta T", r"Q=\pm mL")
    return L


# ============================================================================ E7
@leccion("E7")
def e7() -> Leccion:
    mh, mal, Tf, Ta = 0.200, 0.150, 10.0, 95.0
    ca, cal, Lf = 4190.0, 900.0, 3.35e5
    q1, q2, q3 = mh * Lf, mh * ca * Tf, mal * cal * Tf
    tot = q1 + q2 + q3
    m = tot / (ca * (Ta - Tf))
    L = Leccion("E7", "¿Cuánta agua caliente derrite el hielo?", tema="4.8", nivel=3, fuente=_f(7, 2), claves=["calorimetría", "masa incógnita"],
                enunciado="¿Cuánta masa de agua a 95 °C se necesita para llevar 0,200 kg de hielo de 0 °C a 10 °C, si está en un recipiente de aluminio de 0,150 kg? $c_{agua}=4190$, $c_{Al}=900$ J/(kg·K), $L_f=3{,}35\\times10^5$ J/kg.",
                visual={"tipo": "calor", "Tf": 10, "componentes": [{"n": "hielo", "m": 200, "T": 0, "fase": "hielo"}, {"n": "aluminio", "m": 150, "T": 0, "fase": "solido"}, {"n": "agua", "m": round(m * 1000, 1), "T": 95, "fase": "caliente"}],
                        "necesita": [{"n": "fundir el hielo", "Q": q1}, {"n": "esa agua de 0 a 10 °C", "Q": q2}, {"n": "aluminio de 0 a 10 °C", "Q": q3}], "disponible": [{"n": "agua de 95 a 10 °C", "Q": tot}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Hielo a cero grados en un recipiente de aluminio, también a cero. Queremos terminar a diez grados.",
            DATO("0,200 kg de hielo"), LISTA(["Hielo: $0{,}200$ kg a $0\\ ^\\circ\\text{C}$", "Aluminio: $0{,}150$ kg a $0\\ ^\\circ\\text{C}$", "Agua caliente a $95\\ ^\\circ\\text{C}$, masa desconocida", "Final: $T_f=10\\ ^\\circ\\text{C}$"]))\
        .di("La incógnita es la masa de agua caliente.", M(r"m=\;?"))\
        .sale_de(("aluminio a $0\\ ^\\circ\\text{C}$", "está en contacto con el hielo desde el inicio"))
    L.paso("La idea: lo que uno cede, los otros lo ganan")\
        .di("Conservación de la energía: todo el calor que suelta el agua caliente lo reciben el hielo y el aluminio.", V(foco="flujo"),
            M(r"Q_{\text{cede el agua}} = Q_{\text{ganan hielo y Al}}", "cons", "azul"))\
        .di("El hielo gana en dos etapas: primero se funde y luego se calienta.", LISTA(["fundir: $m_hL_f$", "calentar de 0 a 10: $m_hc_{agua}\\Delta T$", "aluminio de 0 a 10: $m_{Al}c_{Al}\\Delta T$"]))
    L.paso("Calor que se necesita")\
        .di("Fundir el hielo.", M(rf"Q_1=(0{{,}}200)(3{{,}}35\times10^5)={tex(q1, 5, u='J', sci='no')}", "q1"), V(etapa=0))\
        .di("Calentar esa agua diez grados.", M(rf"Q_2=(0{{,}}200)(4190)(10)={tex(q2, 4, u='J', sci='no')}", "q2"), V(etapa=1))\
        .di("Calentar el aluminio diez grados.", M(rf"Q_3=(0{{,}}150)(900)(10)={tex(q3, 4, u='J', sci='no')}", "q3"), V(etapa=2))\
        .di("En total, sumamos los tres.", M(rf"Q_{{\text{{gana}}}}=Q_1+Q_2+Q_3={tex(q1, 5, sci='no')}+{tex(q2, 4, sci='no')}+{tex(q3, 4, sci='no')}={tex(tot, 5, u='J', sci='no')}", "tot", "rojo"))\
        .pregunta_rapida("¿Se usa el calor específico del hielo (2090)?", ["Sí, para calentarlo", "No, el hielo ya está a 0 °C"], 1, "El hielo no sube de temperatura como hielo: de una vez se funde.")
    L.paso("Calor que cede el agua caliente")\
        .di("Baja de noventa y cinco a diez: ochenta y cinco grados.", M(r"Q_{\text{cede}}=m\,c_{\text{agua}}\,(95-10)"))\
        .di("Igualamos con lo que se necesita.", M(rf"m\,(4190)(85)={tex(tot, 5, sci='no')}", "ig"))
    L.paso("Despejar la masa")\
        .di("Lo que multiplica pasa a dividir. Abajo multiplicamos primero.", M(rf"m=\frac{{{tex(tot, 5, sci='no')}}}{{(4190)(85)}}=\frac{{{tex(tot, 5, sci='no')}}}{{356\,150}}"))\
        .di(f"Se necesitan {nu(m, 'kg', 3)} de agua.", R(rf"m={tex(m, 3, u='kg')}"))\
        .di("Son unos doscientos quince gramos, razonable: parecido a la masa de hielo.", OJO("Error típico: olvidar el aluminio o usar $c_{hielo}$ en vez de $L_f$."))
    L.responde("", rf"m={tex(m, 3)}\ \text{{kg}}", m, "kg").usa(r"Q=mc\Delta T", r"Q=mL_f")
    L.te_toca("Mismo problema pero con 0,100 kg de hielo y sin recipiente. ¿Cuántos kg de agua a 95 °C?", (0.1 * Lf + 0.1 * ca * 10) / (ca * 85), "kg", pista="Solo dos calores: fundir y calentar el hielo fundido.", solucion="$m=\\frac{33\\,500+4190}{4190\\cdot85}=0{,}106$ kg")
    return L


# ============================================================================ E8
@leccion("E8")
def e8() -> Leccion:
    a, b, Lc, Ti, Te, r = 0.100, 0.200, 2.00, 100.0, 25.0, 0.15
    coef = 2 * math.pi * Lc * (Ti - Te) / (b - a)
    Tr = Ti - (Ti - Te) * (r - a) / (b - a)
    L = Leccion("E8", "Tubo con aislante de conductividad k₀/r", tema="5.5", nivel=4, fuente=_f(8, 2), claves=["conducción radial", "k variable", "integral"],
                aviso="El enunciado no da el valor de k₀: el flujo queda en función de k₀. La temperatura no depende de k₀.",
                enunciado="Aislante cilíndrico alrededor de un tubo con vapor: $a=0{,}100$ m, $b=0{,}200$ m, $L=2{,}00$ m, $k=k_0/r$, $T_{int}=100\\ ^\\circ\\text{C}$, $T_{ext}=25{,}0\\ ^\\circ\\text{C}$. a) Flujo calórico radial. b) Temperatura a 15 cm del centro.",
                visual={"tipo": "conduccion", "geo": "cilindro", "r1": a, "r2": b, "L": Lc, "T1": Ti, "T2": Te, "k": "k0/r", "perfil": "lineal", "r": r, "Tr": Tr, "uT": "°C", "etq1": "a", "etq2": "b"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un tubo caliente envuelto en aislante. El calor sale hacia afuera, en dirección radial.",
            DATO("longitud de esta sección"), LISTA(["$a=0{,}100$ m, $b=0{,}200$ m, $L=2{,}00$ m", "Adentro $100\\ ^\\circ\\text{C}$, afuera $25{,}0\\ ^\\circ\\text{C}$ (de la figura)", "La conductividad **cambia con el radio**: $k=k_0/r$"]))\
        .di("Como ka no es constante, la fórmula corta no sirve: hay que integrar.".replace("ka no", dice("k", "ka") + " no"), OJO("La fórmula $H=kA\\frac{T_H-T_C}{L}$ solo vale para pared plana con $k$ constante."))
    L.paso("La idea: ley de Fourier en un cascarón")\
        .di("Cortamos un cascarón delgadito de radio erre y grosor de erre.".replace("radio erre", "radio " + dice("r", "erre")).replace("grosor de erre", "grosor " + dice("dr", "de erre")), V(foco="dr"), F(r"H=-kA\frac{dT}{dr}", "f"))\
        .di("El área por donde pasa el calor es la pared lateral de ese cilindro.", M(r"A=2\pi rL", "area"))\
        .di("El signo menos dice que el calor va de caliente a frío.", MARCA("circulo", "-", "f", "rojo", "de caliente a frío"))\
        .por_que("En estado estacionario por cada cascarón pasa la misma cantidad de calor por segundo. Por eso $H$ es constante y puede salir de la integral.")\
        .sale_de(("$2\\pi rL$", "perímetro del círculo $2\\pi r$ por el largo $L$"), ("$H$ constante", "estado estacionario: no se acumula calor en ningún lado"))
    L.paso("Meter k y A en la ecuación")\
        .di("Sustituimos la conductividad y el área.", M(r"H=-\left(\frac{k_0}{r}\right)(2\pi rL)\frac{dT}{dr}", "e1"))\
        .di("Mira la magia: la erre de abajo se cancela con la erre de arriba.", M(r"H=-\left(\frac{k_0}{\cancel{r}}\right)(2\pi \cancel{r}L)\frac{dT}{dr}"))\
        .di("Queda mucho más simple.", M(r"H=-2\pi k_0L\,\frac{dT}{dr}", "e2", "azul"))
    L.paso("Separar variables")\
        .di("Pasamos de erre para la izquierda: las erres a un lado, la temperatura al otro.".replace("de erre", dice("dr", "de erre")), M(r"H\,dr=-2\pi k_0L\,dT"))\
        .di("Integramos desde adentro hasta afuera.", M(r"H\int_{a}^{b}dr=-2\pi k_0L\int_{T_{int}}^{T_{ext}}dT", "int"), V(foco="barrido"))\
        .di("Los límites van en parejas: en el radio a está cien grados, en el radio be, veinticinco.".replace("radio a ", "radio " + dice("a", "a") + " ").replace("radio be", "radio " + dice("b", "be")), NOTA("$r=a \\leftrightarrow T=100$ y $r=b \\leftrightarrow T=25$"))
    L.paso("Resolver las integrales")\
        .di("Las dos son directas: sumar todos los pedacitos de radio da el radio, y los de temperatura, la temperatura.", M(r"\int dr=r\qquad \int dT=T"), NOTA("Regla 1 con exponente cero, lección **T-MAT2**."))\
        .di("Con sus límites.", M(r"H\,\Big[\,r\,\Big]_a^b=-2\pi k_0L\,\Big[\,T\,\Big]_{T_{int}}^{T_{ext}}"))\
        .di("Superior menos inferior.", M(r"H\,(b-a)=-2\pi k_0L\,(T_{ext}-T_{int})"))\
        .di("Metemos el menos en el paréntesis y se voltea la resta.", M(r"H\,(b-a)=2\pi k_0L\,(T_{int}-T_{ext})", "e3"))\
        .despacio((r"-(T_{ext}-T_{int})=-T_{ext}+T_{int}=T_{int}-T_{ext}", "el menos cambia los dos signos"))
    L.paso("a) El flujo de calor")\
        .di("Despejamos hache.".replace("hache", dice("H", "hache")), M(r"H=\frac{2\pi k_0L\,(T_{int}-T_{ext})}{b-a}", "h", "azul"))\
        .di("Sustituimos.", M(r"H=\frac{2\pi k_0(2{,}00)(100-25{,}0)}{0{,}200-0{,}100}"))\
        .di("Queda en función de ka cero.".replace("ka cero", dice("k₀", "ka cero")), R(rf"H={tex(coef, 4, sci='si')}\,k_0\ \ (\text{{W, con }}k_0\text{{ en W/K}})", "ra"), V(foco="flujo"))
    L.paso("b) Temperatura a 15 cm")\
        .di("Repetimos la misma integral, pero solo hasta erre igual a quince centímetros.".replace("erre igual", dice("r", "erre") + " igual"), M(r"H\,(r-a)=2\pi k_0L\,(T_{int}-T)"))\
        .di("Dividimos esta entre la anterior: hache, ka cero, pi y ele se cancelan.".replace("hache, ka cero", dice("H, k₀", "hache, ka cero")), M(r"\frac{r-a}{b-a}=\frac{T_{int}-T}{T_{int}-T_{ext}}", "raz", "azul"), V(foco="punto"))\
        .di("Ponemos números.", M(r"\frac{0{,}15-0{,}10}{0{,}20-0{,}10}=\frac{100-T}{75}"), M(r"0{,}5=\frac{100-T}{75}"))\
        .di(f"Despejamos: cien menos treinta y siete y medio.", M(r"T=100-0{,}5\,(75)"), R(rf"T={tex(Tr, dec=1, u='°C')}", "rb"))
    L.paso("Cómo recordarlo")\
        .di("Con esta conductividad rara, la temperatura baja en línea recta: por eso a medio camino está justo a la mitad.", V(foco="perfil"),
            LISTA(["Escribí Fourier con $A(r)$ y $k(r)$", "Separá: $r$ de un lado, $T$ del otro", "Integrá con los límites en parejas", "Para otra temperatura: misma integral, otro límite"]))
    L.responde("a", rf"H={tex(coef, 4, sci='si')}\,k_0", coef, "W/k0").responde("b", rf"T={tex(Tr, dec=1)}\ ^\circ\text{{C}}", Tr, "°C")
    L.usa(r"H=-kA\frac{dT}{dx}")
    return L


# ============================================================================ E9
def _esfera_hierro(id: str, fuente: dict) -> Leccion:
    r2, r1, H, T2, k = 0.44, 0.61, 3700.0, 343.0, 73.0
    f = H / (4 * math.pi * k)
    g = 1 / r2 - 1 / r1
    T1 = T2 - f * g
    L = Leccion(id, "Cascarón esférico de un reactor", tema="5.4", nivel=3, fuente=fuente, claves=["conducción esférica", "integral"],
                enunciado="Un reactor está revestido con un cascarón esférico de hierro ($k=73$ W/m°C). Radio interior $r_2=0{,}44$ m y exterior $r_1=0{,}61$ m. Sale una corriente de calor $H=3700$ W y la temperatura interior es $T_2=343\\ ^\\circ\\text{C}$. Halle la temperatura exterior $T_1$.",
                visual={"tipo": "conduccion", "geo": "esfera", "r1": r2, "r2": r1, "T1": T2, "T2": round(T1, 2), "k": k, "H": H, "uT": "°C", "etq1": "r₂", "etq2": "r₁"})
    L.paso("Qué nos dan y qué piden")\
        .di("Una esfera hueca de hierro. El calor sale del centro hacia afuera y ya sabemos cuánto: tres mil setecientos watts.",
            DATO("3700 W"), LISTA(["$r_2=0{,}44$ m (adentro), $r_1=0{,}61$ m (afuera)", "$H=3700$ W, $k=73\\ \\frac{\\text{W}}{\\text{m}\\cdot{}^\\circ\\text{C}}$", "$T_2=343\\ ^\\circ\\text{C}$ adentro"]))\
        .di("Nos falta la temperatura de afuera.", M(r"T_1=\;?"), OJO("Ojo con los nombres: aquí $r_2$ es el de **adentro** y $r_1$ el de **afuera**."))
    L.paso("La idea: Fourier en una cáscara de esfera")\
        .di("Cortamos una cáscara delgadita de radio erre.".replace("erre.", dice("r", "erre") + "."), V(foco="dr"), F(r"H=-kA\frac{dT}{dr}", "f"))\
        .di("El área de una esfera es cuatro pi erre cuadrado.", M(r"A=4\pi r^2", "ar"))\
        .di("La metemos en Fourier.", M(r"H=-k\,(4\pi r^2)\frac{dT}{dr}", "e1"))\
        .por_que("El área crece con el radio: las cáscaras de afuera son más grandes. Por eso no se puede usar la fórmula de pared plana.")\
        .sale_de(("$4\\pi r^2$", "área de la superficie de una esfera"))
    L.paso("Separar variables")\
        .di("Las erres a un lado y la temperatura al otro. Paso uno: multiplicar los dos lados por de erre.".replace("de erre", dice("dr", "de erre")), M(r"H\,dr=-4\pi k\,r^2\,dT"))\
        .di("Paso dos: dividir entre erre cuadrado. Paso tres: dividir entre hache.".replace("erre cuadrado", dice("r²", "erre cuadrado")).replace("hache", dice("H", "hache")),
            M(r"\frac{H\,dr}{r^2}=-4\pi k\,dT\;\Rightarrow\;\frac{dr}{r^2}=-\frac{4\pi k}{H}\,dT", "sep"), NOTA("Receta en la lección **T-MAT3**."))\
        .di("Integral a los dos lados, de adentro hacia afuera.", M(r"\int_{r_2}^{r_1}\frac{dr}{r^2}=-\frac{4\pi k}{H}\int_{T_2}^{T_1}dT", "int"), V(foco="barrido"))
    L.paso("La integral de 1/r²")\
        .di("Uno entre erre cuadrado es erre a la menos dos; regla de la potencia: sumamos uno al exponente y dividimos entre el nuevo.", M(r"\frac{1}{r^2}=r^{-2}\;\Rightarrow\;\int r^{-2}dr=\frac{r^{-2+1}}{-2+1}=\frac{r^{-1}}{-1}=-\frac{1}{r}", "prim", "azul"),
            NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Evaluamos: arriba menos abajo.", M(r"\left[-\frac{1}{r}\right]_{r_2}^{r_1}=-\frac{1}{r_1}-\left(-\frac{1}{r_2}\right)"))\
        .di("Queda la resta de los inversos.", M(r"=\frac{1}{r_2}-\frac{1}{r_1}", "res"))\
        .di("El otro lado: sumar los pedacitos de temperatura da el cambio total.", M(r"-\frac{4\pi k}{H}\int_{T_2}^{T_1}dT=-\frac{4\pi k}{H}\,(T_1-T_2)=\frac{4\pi k}{H}\,(T_2-T_1)", "der"))
    L.paso("Despejar la temperatura exterior")\
        .di("Igualamos los dos lados.", M(r"\frac{1}{r_2}-\frac{1}{r_1}=\frac{4\pi k}{H}\,(T_2-T_1)"))\
        .di("Pasamos el factor al otro lado, volteado.", M(r"T_2-T_1=\frac{H}{4\pi k}\left(\frac{1}{r_2}-\frac{1}{r_1}\right)"))\
        .di("Y despejamos te uno.".replace("te uno", s("T_1")), M(r"T_1=T_2-\frac{H}{4\pi k}\left(\frac{1}{r_2}-\frac{1}{r_1}\right)", "desp", "azul"))
    L.paso("Poner los números")\
        .di("Primero el factor de adelante.", M(rf"\frac{{H}}{{4\pi k}}=\frac{{3700}}{{4\pi(73)}}={tex(f, 4)}"))\
        .di("Luego el paréntesis.", M(rf"\frac{{1}}{{0{{,}}44}}-\frac{{1}}{{0{{,}}61}}={tex(1 / r2, dec=4)}-{tex(1 / r1, dec=4)}={tex(g, 4)}\ \text{{m}}^{{-1}}"))\
        .di("Multiplicamos.", M(rf"T_1=343-({tex(f, 4)})({tex(g, 4)})=343-{tex(f * g, 3)}"))\
        .di(f"La superficie exterior está a {nu(T1, '°C', dec=1)}.", R(rf"T_1={tex(T1, dec=1, u='°C')}"), V(foco="perfil"))\
        .di("Solo baja dos grados y medio: el hierro conduce muy bien.", NOTA("Con tres cifras, $T_1\\approx 340\\ ^\\circ\\text{C}$."))
    L.responde("", rf"T_1={tex(T1, dec=1)}\ ^\circ\text{{C}}", T1, "°C", 0.005).usa(r"H=-kA\frac{dT}{dx}")
    return L


@leccion("E9")
def e9() -> Leccion:
    return _esfera_hierro("E9", _f(9, 3))


# ============================================================================ E10
def _filamento(id: str, fuente: dict) -> Leccion:
    P, f, e, Tt = 100.0, 0.90, 0.35, 2450.0
    H = P * f
    A = H / (e * SIGMA * Tt ** 4)
    L = Leccion(id, "Área del filamento de un bombillo", tema="5.9", nivel=2, fuente=fuente, claves=["radiación", "Stefan-Boltzmann"],
                enunciado="El filamento de tungsteno de un bombillo ($e=0{,}35$) trabaja a 2450 K. Calcule su área si la lámpara es de 100 W y el 90 % de la energía se radia como ondas electromagnéticas.",
                visual={"tipo": "radiacion", "T": Tt, "e": e, "A": A * 1e4, "uA": "cm²", "H": H, "objeto": "filamento"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un filamento que brilla porque está muy caliente: radia energía.",
            DATO("2450 K"), LISTA(["Potencia eléctrica: $100$ W", "Radia el $90\\,\\%$", "$e=0{,}35$, $T=2450$ K (ya está en kelvin)"]))\
        .di("Queremos el área de su superficie.", M(r"A=\;?"))
    L.paso("La potencia que sí se radia")\
        .di("Solo el noventa por ciento sale como radiación.", M(r"H=0{,}90\,(100\ \text{W})"), M(r"H=90\ \text{W}", "h"))\
        .por_que("El 10 % restante se va por el alambre y el gas. La ley de Stefan-Boltzmann solo cuenta lo radiado.")
    L.paso("La idea: Stefan-Boltzmann")\
        .di("La potencia radiada depende del área, de la emisividad y de la temperatura a la cuarta.", V(foco="radia"), F(r"H=Ae\sigma T^4", "f"))\
        .di("Despejamos el área.", M(r"A=\frac{H}{e\sigma T^4}", "d", "azul"))\
        .sale_de(("$\\sigma=5{,}67\\times10^{-8}$", "constante de Stefan-Boltzmann, está en el formulario"))
    L.paso("Poner los números")\
        .di("Cuidado: la cuarta potencia es solo de la temperatura.", M(r"A=\frac{90}{(0{,}35)(5{,}67\times10^{-8})(2450)^4}"))\
        .di("Primero la cuarta potencia.", M(rf"(2450)^4={tex(Tt ** 4, 4)}\ \text{{K}}^4", "t4"))\
        .di("Luego todo el denominador.", M(rf"(0{{,}}35)(5{{,}}67\times10^{{-8}})({tex(Tt ** 4, 4)})={tex(e * SIGMA * Tt ** 4, 4)}\ \frac{{\text{{W}}}}{{\text{{m}}^2}}", "den"))\
        .di("Dividimos.", M(rf"A=\frac{{90}}{{{tex(e * SIGMA * Tt ** 4, 4)}}}={tex(A, 3, u='m2')}"))\
        .di(f"En centímetros cuadrados son {n(A * 1e4, 3)}.", R(rf"A={tex(A, 3)}\ \text{{m}}^2={tex(A * 1e4, 3)}\ \text{{cm}}^2"))\
        .di("Muy chiquito, como corresponde a un alambre.", OJO("Error típico: usar los 100 W completos, o la temperatura en Celsius."))
    L.responde("", rf"A={tex(A, 3)}\ \text{{m}}^2", A, "m2").usa(r"H=Ae\sigma T^4")
    L.te_toca("Un bombillo de 60 W radia el 90 % a 2600 K con $e=0{,}35$. ¿Área en cm²?", 54 / (0.35 * SIGMA * 2600 ** 4) * 1e4, "cm²", pista="$H=54$ W.", solucion="$A=\\frac{54}{0{,}35\\,\\sigma\\,2600^4}$")
    return L


@leccion("E10")
def e10() -> Leccion:
    return _filamento("E10", _f(10, 3))


# ============================================================================ E11
@leccion("E11")
def e11() -> Leccion:
    Lc, r1, esp, r3, Tm, Tu, km, ka = 0.700, 0.0294, 0.0001, 0.178, 360.0, 469.0, 101.5, 0.024
    r2 = r1 + esp
    l1, l2 = math.log(r2 / r1), math.log(r3 / r2)
    Ra, Rm = l1 / ka, l2 / km
    H = 2 * math.pi * Lc * (Tu - Tm) / (Ra + Rm)
    T2 = Tm + H * l2 / (2 * math.pi * Lc * km)
    L = Leccion("E11", "Uranio, aire y magnox: dos capas cilíndricas", tema="5.4", nivel=5, fuente=_f(11, 3), claves=["conducción cilíndrica", "dos capas", "serie"],
                enunciado="Un cilindro de uranio (radio 29,4 mm, largo 70,0 cm) va dentro de un tubo de magnox de diámetro externo 356,0 mm, con una capa de aire de 0,10 mm entre ambos. $T$ externa del magnox: 360,0 °C; del combustible: 469,0 °C. $k_{magnox}=101{,}5$, $k_{aire}=0{,}024$ W/(m·K). a) Temperatura en el radio interno del magnox. b) Flujo de calor a través del magnox.",
                visual={"tipo": "conduccion", "geo": "cilindro2", "r1": r1, "r2": r2, "r3": r3, "T1": Tu, "T2": round(T2, 1), "T3": Tm, "n1": "aire", "n2": "magnox", "k1": ka, "k2": km, "uT": "°C", "exagera": True})
    L.paso("Qué nos dan y qué piden")\
        .di("Tres cosas una dentro de otra: uranio, una película de aire finísima y el tubo de magnox.",
            DATO("0,10 mm"), LISTA(["Uranio: $r_1=29{,}4$ mm", "Aire: $0{,}10$ mm, entonces $r_2=29{,}5$ mm", "Magnox: diámetro externo $356{,}0$ mm, entonces $r_3=178{,}0$ mm", "$L=0{,}700$ m, $T_1=469\\ ^\\circ\\text{C}$, $T_3=360\\ ^\\circ\\text{C}$"]))\
        .di("Cuidado: dieron el diámetro, hay que sacarle la mitad.", M(r"r_3=\frac{356{,}0}{2}=178{,}0\ \text{mm}", "r3"))\
        .di("Piden la temperatura entre el aire y el magnox.", M(r"T_2=\;?\qquad H=\;?"))\
        .sale_de(("$r_2=29{,}5$ mm", "radio del uranio más el grosor del aire: $29{,}4+0{,}10$"))
    L.paso("La idea: el mismo calor cruza las dos capas")\
        .di("En estado estacionario, el calor que pasa por el aire es el mismo que pasa por el magnox.", V(foco="flujo"), M(r"H_{\text{aire}}=H_{\text{magnox}}=H", "ig", "azul"))\
        .por_que("Si pasara más calor por una capa que por la otra, la energía se acumularía en el medio y la temperatura cambiaría. Estacionario significa que nada cambia.")
    L.paso("Fourier en un cilindro")\
        .di("Partimos de la ley de Fourier con el área lateral.", F(r"H=-kA\frac{dT}{dr}"), M(r"H=-k\,(2\pi rL)\frac{dT}{dr}", "e1"), V(foco="dr"))\
        .di("Separamos: multiplicamos por de erre y dividimos entre erre y entre hache.".replace("de erre", dice("dr", "de erre")).replace("entre erre", "entre " + dice("r", "erre")).replace("hache", dice("H", "hache")),
            M(r"H\,dr=-k\,(2\pi rL)\,dT\;\Rightarrow\;\frac{dr}{r}=-\frac{2\pi kL}{H}\,dT"), NOTA("Receta en la lección **T-MAT3**."))
    L.paso("Integrar una capa")\
        .di("Integral a los dos lados, del radio interno al externo de la capa.", M(r"\int_{r_i}^{r_e}\frac{dr}{r}=-\frac{2\pi kL}{H}\int_{T_i}^{T_e}dT"))\
        .di("Izquierda: regla dos, uno entre erre da logaritmo; la resta de logaritmos es el logaritmo de la división.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"\int_{r_i}^{r_e}\frac{dr}{r}=\Big[\ln r\Big]_{r_i}^{r_e}=\ln r_e-\ln r_i=\ln\frac{r_e}{r_i}"))\
        .di("Derecha: la suma de pedacitos de temperatura; el menos voltea la resta.", M(r"-\frac{2\pi kL}{H}\int_{T_i}^{T_e}dT=-\frac{2\pi kL}{H}(T_e-T_i)=\frac{2\pi kL}{H}(T_i-T_e)"))\
        .di("Juntamos y despejamos hache. Sirve para cualquier capa.".replace("hache", dice("H", "hache")), M(r"\ln\left(\frac{r_e}{r_i}\right)=\frac{2\pi kL}{H}\,(T_i-T_e)\;\Rightarrow\;H=\frac{2\pi kL\,(T_i-T_e)}{\ln(r_e/r_i)}", "cap", "azul"))
    L.paso("Escribirla para cada capa")\
        .di("Para el aire.", M(r"H=\frac{2\pi k_aL\,(T_1-T_2)}{\ln(r_2/r_1)}", "ha"))\
        .di("Para el magnox.", M(r"H=\frac{2\pi k_mL\,(T_2-T_3)}{\ln(r_3/r_2)}", "hm"))\
        .di("De cada una sacamos la caída de temperatura.", M(r"T_1-T_2=\frac{H}{2\pi L}\cdot\frac{\ln(r_2/r_1)}{k_a}"), M(r"T_2-T_3=\frac{H}{2\pi L}\cdot\frac{\ln(r_3/r_2)}{k_m}"))
    L.paso("Sumar las caídas")\
        .di("Al sumarlas, te dos desaparece: es el truco de las capas en serie.".replace("te dos", s("T_2")), M(r"T_1-T_3=\frac{H}{2\pi L}\left[\frac{\ln(r_2/r_1)}{k_a}+\frac{\ln(r_3/r_2)}{k_m}\right]", "sum", "azul"))\
        .di("Calculamos cada término del corchete, con cinco cifras: el flujo da justo en el límite y con menos se correría una unidad.",
            M(rf"\frac{{\ln(29{{,}}5/29{{,}}4)}}{{0{{,}}024}}=\frac{{{tex(l1, 5)}}}{{0{{,}}024}}={tex(Ra, 5)}"), M(rf"\frac{{\ln(178{{,}}0/29{{,}}5)}}{{101{{,}}5}}=\frac{{{tex(l2, 5)}}}{{101{{,}}5}}={tex(Rm, 5)}"))\
        .di("La capa de aire, aunque es finísima, frena mucho más que todo el magnox.", NOTA("El aire conduce unas 4000 veces peor que el magnox."), V(foco="comparar"))
    L.paso("b) El flujo de calor")\
        .di("Despejamos hache.".replace("hache", dice("H", "hache")), M(r"H=\frac{2\pi L\,(T_1-T_3)}{\text{corchete}}"))\
        .di("Sustituimos.", M(rf"H=\frac{{2\pi(0{{,}}700)(469-360)}}{{{tex(Ra, 5)}+{tex(Rm, 5)}}}"))\
        .di(f"Pasan {nu(H, 'W', 4)}.", R(rf"H={tex(H, 4, u='W')}", "rb"))
    L.paso("a) La temperatura interna del magnox")\
        .di("Usamos la capa de magnox, donde ya solo falta te dos.".replace("te dos", s("T_2")), M(r"T_2=T_3+\frac{H\ln(r_3/r_2)}{2\pi k_mL}"))\
        .di("Sustituimos.", M(rf"T_2=360+\frac{{({tex(H, 4)})({tex(l2, 5)})}}{{2\pi(101{{,}}5)(0{{,}}700)}}"))\
        .di(f"Queda en {nu(T2, '°C', dec=1)}.", R(rf"T_2=360+{tex(T2 - Tm, 4)}={tex(T2, dec=1, u='°C')}", "ra"), V(foco="perfil"))\
        .di("Casi toda la caída de temperatura ocurre en la película de aire.", OJO("Error típico: usar el diámetro como radio, o restar los radios en vez de dividirlos dentro del logaritmo."))
    L.responde("a", rf"T_2={tex(T2, dec=1)}\ ^\circ\text{{C}}", T2, "°C", 0.005).responde("b", rf"H={tex(H, 4)}\ \text{{W}}", H, "W").usa(r"H=-kA\frac{dT}{dx}")
    return L


# ============================================================================ E12
@leccion("E12")
def e12() -> Leccion:
    a, b, Lc, Ti, Te, k = 0.03, 0.08, 6.2, 100.0, 11.0, 0.024
    ln1 = math.log(b / a)
    H = 2 * math.pi * k * Lc * (Ti - Te) / ln1
    c = b * b / a
    L = Leccion("E12", "Tubería de vapor forrada con espuma", tema="5.4", nivel=4, fuente=_f(12, 3), claves=["conducción cilíndrica", "espesor adicional"],
                aviso="El enunciado no da la k de la espuma de poliuretano. Se usa 0,024 W/(m·K), el valor de tabla habitual. El inciso b) no depende de k.",
                enunciado="Una tubería metálica delgada lleva vapor a 100 °C. Radio $a=3$ cm, forrada con 5 cm de espuma de poliuretano hasta $b=8$ cm. Largo 6,2 m, temperatura externa 11 °C. a) Tasa de calor que se pierde. b) ¿Cuánto aislante adicional reduce la pérdida a la mitad?",
                visual={"tipo": "conduccion", "geo": "cilindro", "r1": a, "r2": b, "L": Lc, "T1": Ti, "T2": Te, "k": k, "uT": "°C", "etq1": "a", "etq2": "b"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un tubo con vapor y un forro de espuma. El metal es delgado: lo ignoramos, todo el freno lo pone la espuma.",
            DATO("5cm de espuma"), LISTA(["$a=0{,}03$ m, $b=0{,}08$ m, $L=6{,}2$ m", "Adentro $100\\ ^\\circ\\text{C}$, afuera $11\\ ^\\circ\\text{C}$", "Espuma: $k=0{,}024\\ \\frac{\\text{W}}{\\text{m}\\cdot\\text{K}}$ (de tabla)"]))\
        .di("Piden la pérdida de calor y cuánto forro extra la baja a la mitad.", M(r"H=\;?\qquad \text{grosor extra}=\;?"))
    L.paso("La idea: Fourier radial")\
        .di("El calor cruza cilindros cada vez más grandes.", V(foco="dr"), F(r"H=-kA\frac{dT}{dr}"), M(r"A=2\pi rL"))\
        .di("Sustituimos el área.", M(r"H=-k\,(2\pi rL)\,\frac{dT}{dr}"))\
        .di("Separamos: multiplicamos por de erre y dividimos entre erre y entre hache.".replace("de erre", dice("dr", "de erre")).replace("entre erre", "entre " + dice("r", "erre")).replace("hache", dice("H", "hache")),
            M(r"H\,dr=-2\pi kL\,r\,dT\;\Rightarrow\;\frac{dr}{r}=-\frac{2\pi kL}{H}\,dT", "sep"), NOTA("Receta en la lección **T-MAT3**."))
    L.paso("Integrar")\
        .di("Del radio interno al externo, con sus temperaturas en parejas.", M(r"\int_a^b\frac{dr}{r}=-\frac{2\pi kL}{H}\int_{T_i}^{T_e}dT"), V(foco="barrido"))\
        .di("Izquierda: regla dos, uno entre erre da logaritmo. Derecha: la suma de pedacitos de temperatura.".replace("entre erre", "entre " + dice("r", "erre")),
            M(r"\Big[\ln r\Big]_a^b=\ln b-\ln a=\ln\frac{b}{a}"), M(r"-\frac{2\pi kL}{H}\,(T_e-T_i)=\frac{2\pi kL}{H}\,(T_i-T_e)"))\
        .di("Juntamos.", M(r"\ln\left(\frac{b}{a}\right)=\frac{2\pi kL}{H}\,(T_i-T_e)"))\
        .di("Despejamos.", M(r"H=\frac{2\pi kL\,(T_i-T_e)}{\ln(b/a)}", "h", "azul"))
    L.paso("a) La pérdida de calor")\
        .di("Ponemos números.", M(r"H=\frac{2\pi(0{,}024)(6{,}2)(100-11)}{\ln(0{,}08/0{,}03)}"))\
        .di("Arriba y abajo por separado.", M(rf"H=\frac{{{tex(2 * math.pi * k * Lc * 89, 4)}}}{{{tex(ln1, 4)}}}"))\
        .di(f"Se pierden {nu(H, 'W', 3)}.", R(rf"H={tex(H, 3, u='W')}", "ra"), V(foco="flujo"))
    L.paso("b) La mitad de la pérdida")\
        .di("Nuevo radio exterior ce. Todo lo demás queda igual, solo cambia el logaritmo.".replace("exterior ce", "exterior " + dice("c", "ce")), M(r"\frac{H}{2}=\frac{2\pi kL\,(T_i-T_e)}{\ln(c/a)}"))\
        .di("Para que hache baje a la mitad, el logaritmo tiene que ser el doble.".replace("hache", dice("H", "hache")), M(r"\ln\left(\frac{c}{a}\right)=2\ln\left(\frac{b}{a}\right)", "dob", "azul"))\
        .di("Dos por un logaritmo es el logaritmo del cuadrado.", M(r"\ln\left(\frac{c}{a}\right)=\ln\left(\frac{b}{a}\right)^{2}"))\
        .di("Quitamos logaritmos.", M(r"\frac{c}{a}=\frac{b^2}{a^2}\;\Rightarrow\;c=\frac{b^2}{a}"))\
        .por_que("$H$ está dividido entre el logaritmo. Si $H$ se hace la mitad, el logaritmo se hace el doble. La conductividad ni aparece.")
    L.paso("Resultado")\
        .di("Sustituimos en centímetros.", M(rf"c=\frac{{8^2}}{{3}}=\frac{{64}}{{3}}={tex(c * 100, 4, u='cm')}"))\
        .di("El forro extra es ce menos be.".replace("ce menos be", dice("c − b", "ce menos be")), R(rf"c-b={tex(c * 100, 4)}-8={tex((c - b) * 100, 3, u='cm')}", "rb"), V(foco="perfil"))\
        .di("Más del doble de forro para bajar solo a la mitad: en cilindros el aislante rinde cada vez menos.", OJO("Error típico: duplicar el grosor. Lo que se duplica es el logaritmo, no el espesor."))
    L.responde("a", rf"H={tex(H, 3)}\ \text{{W}}", H, "W").responde("b", rf"{tex((c - b) * 100, 3)}\ \text{{cm}}", (c - b) * 100, "cm").usa(r"H=-kA\frac{dT}{dx}")
    return L


# ============================================================================ E13
@leccion("E13")
def e13() -> Leccion:
    g = [(1, 2), (2, 3), (3, 5), (4, 4), (5, 3), (6, 2), (7, 1)]
    N = sum(k for _, k in g)
    s1 = sum(v * k for v, k in g)
    s2 = sum(v * v * k for v, k in g)
    vm, v2 = s1 / N, s2 / N
    L = Leccion("E13", "Veinte partículas con distintas rapideces", tema="6.3", nivel=3, fuente=_f(13, 4), claves=["rapidez promedio", "rms", "presión", "energía cinética"],
                enunciado="Veinte partículas de masa $m_0$ en un volumen $V$: dos con rapidez $v$, tres con $2v$, cinco con $3v$, cuatro con $4v$, tres con $5v$, dos con $6v$ y una con $7v$. Halle a) la rapidez promedio, b) la rms, c) la presión y d) la energía cinética promedio por partícula.",
                visual={"tipo": "gas", "modo": "rapideces", "grupos": [list(x) for x in g], "vm": round(vm, 3), "vrms": round(math.sqrt(v2), 3)})
    L.paso("Qué nos dan y qué piden")\
        .di("Nos dan un inventario: cuántas partículas van a cada rapidez.", DATO("Veinte partículas"),
            TABLA(["rapidez", "$v$", "$2v$", "$3v$", "$4v$", "$5v$", "$6v$", "$7v$"], [["cuántas"] + [str(k) for _, k in g]]), V(foco="barras"))\
        .di("Suman veinte. Siempre conviene revisarlo.", M(r"N=2+3+5+4+3+2+1=20"))
    L.paso("a) Rapidez promedio")\
        .di("Promedio con pesos: cada rapidez por cuántas la tienen, entre el total.", M(r"\overline{v}=\frac{\sum N_i\,v_i}{N}", "f", "azul"), V(foco="promedio"))\
        .di("Multiplicamos cada pareja.", M(r"\sum N_iv_i=\left(2\cdot1+3\cdot2+5\cdot3+4\cdot4+3\cdot5+2\cdot6+1\cdot7\right)v"))\
        .di("Sumamos.", M(r"=\left(2+6+15+16+15+12+7\right)v=73\,v"))\
        .di(f"Entre veinte: {n(vm, 3)} veces ve.".replace("veces ve", "veces " + dice("v", "ve")), R(rf"\overline{{v}}=\frac{{73v}}{{20}}={tex(vm, 3)}\,v", "ra"))
    L.paso("b) Rapidez cuadrática media")\
        .di("Orden: primero se elevan al cuadrado, luego se promedian y al final la raíz.", M(r"v_{rms}=\sqrt{\frac{\sum N_i\,v_i^2}{N}}", "f2", "azul"), V(foco="rms"))\
        .di("Cada cuadrado por su cantidad.", M(r"\sum N_iv_i^2=\left(2\cdot1+3\cdot4+5\cdot9+4\cdot16+3\cdot25+2\cdot36+1\cdot49\right)v^2"))\
        .di("Sumamos.", M(r"=\left(2+12+45+64+75+72+49\right)v^2=319\,v^2"))\
        .di("Promedio de los cuadrados.", M(rf"(v^2)_{{med}}=\frac{{319\,v^2}}{{20}}={tex(v2, 4)}\,v^2", "v2m"))\
        .di("Y la raíz.", R(rf"v_{{rms}}=\sqrt{{{tex(v2, 4)}}}\,v={tex(math.sqrt(v2), 3)}\,v", "rb"))\
        .por_que("La rms siempre es mayor que el promedio porque al elevar al cuadrado las rápidas pesan más.")
    L.paso("c) De dónde sale la presión (desde la hoja)")\
        .di("La fórmula de presión con las rapideces no viene en la hoja: la armamos con dos casillas que sí vienen. Primero, el gas ideal.", F(r"pV=nRT"), V(foco="presion"))\
        .di("Los moles son partículas entre el número de Avogadro, y erre es Avogadro por ka be. Por eso ene erre es ene por ka be.".replace("ka be", dice("k_B", "ka be")).replace("ene erre es ene por", dice("nR = N k_B", "ene erre es ene por")),
            M(r"n=\frac{N}{N_A}\qquad R=N_Ak_B\;\Rightarrow\;nR=Nk_B\;\Rightarrow\;pV=Nk_BT"),
            NOTA("Se comprueba con las constantes de la hoja: $(6{,}022\\times10^{23})(1{,}38\\times10^{-23})=8{,}31$."))\
        .di("Segunda casilla: la energía cinética media de una partícula. Despejamos ka be te.".replace("ka be te", dice("k_B T", "ka be te")),
            F(r"\frac{1}{2}m(v^2)_{\text{med}}=\frac{3k_BT}{2}"), M(r"k_BT=\frac{1}{3}m\,(v^2)_{med}"))\
        .di("Lo ponemos en la primera: presión por volumen es un tercio de ene por la masa por el promedio de los cuadrados.".replace("ene por", dice("N", "ene") + " por"),
            M(r"pV=N\,k_BT=\frac{1}{3}N\,m_0\,(v^2)_{med}", "pv", "azul"))
    L.paso("c) Presión sobre las paredes")\
        .di("Despejamos la presión.", M(r"p=\frac{N\,m_0\,(v^2)_{med}}{3V}"))\
        .di("Sustituimos: veinte por quince coma noventa y cinco es trescientos diecinueve.", M(rf"p=\frac{{20\,m_0\,({tex(v2, 4)}\,v^2)}}{{3V}}=\frac{{319\,m_0v^2}}{{3V}}"))\
        .di("Dividimos entre tres.", R(rf"p={tex(s2 / 3, 4)}\,\frac{{m_0v^2}}{{V}}", "rc"))\
        .sale_de(("$\\frac13$", "las partículas se mueven en tres direcciones; contra cada pared cuenta una"), ("formulario", "sale de $\\tfrac12m(v^2)_{med}=\\tfrac32k_BT$ con $pV=Nk_BT$"))\
        .despacio((r"k_BT=\tfrac13 m_0(v^2)_{med}", "de la fórmula de energía cinética del formulario"), (r"pV=Nk_BT=\tfrac13Nm_0(v^2)_{med}", "lo metemos en el gas ideal"))
    L.paso("d) Energía cinética promedio")\
        .di("Un medio de la masa por el promedio de los cuadrados.", F(r"K_{med}=\frac{1}{2}m_0\,(v^2)_{med}"))\
        .di("Sustituimos.", M(rf"K_{{med}}=\frac{{1}}{{2}}m_0({tex(v2, 4)}\,v^2)"))\
        .di("Listo.", R(rf"K_{{med}}={tex(v2 / 2, 4)}\,m_0v^2", "rd"))\
        .di("Nunca se usa la rapidez promedio para la energía: se usa el promedio de los cuadrados.", OJO("Error típico: poner $\\overline{v}^2$ en lugar de $(v^2)_{med}$."))
    L.responde("a", rf"{tex(vm, 3)}\,v", vm, "v").responde("b", rf"{tex(math.sqrt(v2), 3)}\,v", math.sqrt(v2), "v").responde("c", rf"{tex(s2 / 3, 4)}\,m_0v^2/V", s2 / 3, "m0v2/V").responde("d", rf"{tex(v2 / 2, 4)}\,m_0v^2", v2 / 2, "m0v2")
    L.usa(r"v_{rms}=\sqrt{3k_BT/m}", r"\tfrac12m(v^2)_{med}=\tfrac32k_BT")
    return L


# ============================================================================ E14
@leccion("E14")
def e14() -> Leccion:
    N = 1 / 3 + 1 + 1.5 + 1
    S = 0.25 + 1.5 + (4.5 - (8 / 3 - 2)) + 2 * ((32 - 64 / 3) - (18 - 9))
    vm = S / N
    L = Leccion("E14", "Distribución de rapideces por tramos", tema="6.3", nivel=5, fuente=_f(14, 4), claves=["distribución", "área bajo la curva", "rapidez media"],
                enunciado="La figura muestra $N(v)$ para un gas hipotético, con $N_1=2N_0$, $v_1=2v_0$, $v_2=3v_0$, $v_3=4v_0$. En $[0,v_0]$ es la parábola $N(v)=\\frac{N_0}{v_0^2}v^2$. En función de $N_0$ y $v_0$: a) número total de moléculas, b) rapidez más probable, c) rapidez media.",
                visual={"tipo": "distribucion", "tramos": [{"a": 0, "b": 1, "f": "x^2", "n": "parábola"}, {"a": 1, "b": 2, "f": "1", "n": "constante"}, {"a": 2, "b": 3, "f": "x-1", "n": "sube"}, {"a": 3, "b": 4, "f": "8-2*x", "n": "baja"}], "vm": round(vm, 3), "vp": 3})
    L.paso("Qué nos dan y qué piden")\
        .di("Una curva que dice cuántas moléculas hay en cada rapidez. Tiene cuatro tramos.",
            LISTA(["De $0$ a $v_0$: parábola $\\frac{N_0}{v_0^2}v^2$", "De $v_0$ a $2v_0$: plano en $N_0$", "De $2v_0$ a $3v_0$: sube recto de $N_0$ a $2N_0$", "De $3v_0$ a $4v_0$: baja recto de $2N_0$ a $0$"]), V(foco="todo"))\
        .di("El área bajo la curva es el número de moléculas.", M(r"N=\int N(v)\,dv", "ar", "azul"))\
        .por_que("$N(v)\\,dv$ es cuántas moléculas tienen rapidez entre $v$ y $v+dv$. Sumarlas todas da el total, y sumar es integrar.")
    L.paso("a) Tramo 1: la parábola")\
        .di("Este sí lleva integral.", M(r"A_1=\int_0^{v_0}\frac{N_0}{v_0^2}\,v^2\,dv"), V(foco=0))\
        .di("La constante sale. Regla de la potencia: ve cuadrado da ve cubo entre tres.".replace("ve cuadrado", dice("v²", "ve cuadrado")).replace("ve cubo", dice("v³", "ve cubo")),
            M(r"\int v^2\,dv=\frac{v^{2+1}}{2+1}=\frac{v^3}{3}"), NOTA("Regla 1 de la lección **T-MAT2**."))\
        .di("Arriba menos abajo: en cero da cero.", M(r"A_1=\frac{N_0}{v_0^2}\left[\frac{v^3}{3}\right]_0^{v_0}=\frac{N_0}{v_0^2}\left(\frac{v_0^3}{3}-0\right)=\frac{N_0}{v_0^2}\cdot\frac{v_0^3}{3}"))\
        .di("Se simplifica.", M(r"A_1=\frac{1}{3}N_0v_0", "a1", "verde"))
    L.paso("a) Tramos 2, 3 y 4: figuras conocidas")\
        .di("El tramo plano es un rectángulo de base ve cero y altura ene cero.".replace("ve cero", s("v_0")).replace("ene cero", s("N_0")), M(r"A_2=N_0\,v_0", "a2", "verde"), V(foco=1))\
        .di("El que sube es un trapecio: promedio de alturas por la base.", M(r"A_3=\frac{N_0+2N_0}{2}\,v_0=\frac{3}{2}N_0v_0", "a3", "verde"), V(foco=2))\
        .di("El que baja es un triángulo.", M(r"A_4=\frac{1}{2}(v_0)(2N_0)=N_0v_0", "a4", "verde"), V(foco=3))
    L.paso("a) Total de moléculas")\
        .di("Sumamos las cuatro áreas.", M(r"N=\left(\frac{1}{3}+1+\frac{3}{2}+1\right)N_0v_0"))\
        .di("Con denominador seis.", M(r"N=\frac{2+6+9+6}{6}\,N_0v_0"))\
        .di("Veintitrés sextos.", R(rf"N=\frac{{23}}{{6}}N_0v_0={tex(N, 4)}\,N_0v_0", "ra"), V(foco="todo"))
    L.paso("b) La rapidez más probable")\
        .di("Es donde la curva está más alta, porque ahí hay más moléculas.", V(foco="pico"), R(r"v_{mp}=v_2=3v_0", "rb"))\
        .di("El procedimiento es buscar el máximo: la curva sube hasta tres ve cero y después baja.".replace("tres ve cero", dice("3v₀", "tres ve cero")), NOTA("En $3v_0$ hay un pico: antes la pendiente es positiva y después negativa."))
    L.paso("c) Rapidez media: la fórmula")\
        .di("Promedio con pesos, pero continuo: ve por ene de ve, integrado, entre el total.".replace("ve por ene de ve", dice("v·N(v)", "ve por ene de ve")), M(r"\overline{v}=\frac{1}{N}\int_0^{4v_0}v\,N(v)\,dv", "vm", "azul"))\
        .di("Hay que hacerlo tramo por tramo. Necesitamos las rectas escritas.", M(r"N_3(v)=\frac{N_0}{v_0}(v-v_0)"), M(r"N_4(v)=\frac{2N_0}{v_0}(4v_0-v)"))\
        .despacio((r"N_3:\ \text{pendiente }\frac{2N_0-N_0}{3v_0-2v_0}=\frac{N_0}{v_0}", "pasa por $(2v_0,N_0)$"), (r"N_0+\frac{N_0}{v_0}(v-2v_0)=\frac{N_0}{v_0}(v-v_0)", "se simplifica"), (r"N_4:\ \text{pendiente }\frac{0-2N_0}{v_0}=-\frac{2N_0}{v_0}", "llega a cero en $4v_0$"))
    L.paso("c) Integrar los dos primeros")\
        .di("Parábola: queda ve cubo.".replace("ve cubo", dice("v³", "ve cubo")), M(r"I_1=\frac{N_0}{v_0^2}\int_0^{v_0}v^3dv=\frac{N_0}{v_0^2}\cdot\frac{v_0^4}{4}=\frac{1}{4}N_0v_0^2", "i1"), V(foco=0))\
        .di("Plano: integral de ve.".replace("integral de ve", "integral de " + dice("v", "ve")), M(r"I_2=N_0\int_{v_0}^{2v_0}v\,dv=N_0\,\frac{4v_0^2-v_0^2}{2}=\frac{3}{2}N_0v_0^2", "i2"), V(foco=1))
    L.paso("c) El tramo que sube")\
        .di("Primero multiplicamos ve por la recta de ese tramo.".replace("ve por", dice("v", "ve") + " por"), V(foco=2),
            M(r"v\,N_3(v)=v\cdot\frac{N_0}{v_0}(v-v_0)=\frac{N_0}{v_0}\left(v^2-v_0v\right)"))\
        .di("Regla de la potencia término por término.", M(r"\int\left(v^2-v_0v\right)dv=\frac{v^3}{3}-v_0\,\frac{v^2}{2}"))\
        .di("Arriba, en tres ve cero; abajo, en dos ve cero.".replace("tres ve cero", dice("3v₀", "tres ve cero")).replace("dos ve cero", dice("2v₀", "dos ve cero")),
            M(r"\left[\tfrac{v^3}{3}-\tfrac{v_0v^2}{2}\right]_{3v_0}=9v_0^3-\tfrac92v_0^3=\tfrac92v_0^3\qquad \left[\tfrac{v^3}{3}-\tfrac{v_0v^2}{2}\right]_{2v_0}=\tfrac83v_0^3-2v_0^3=\tfrac23v_0^3"))\
        .di("Restamos y multiplicamos por la constante de afuera.", M(r"I_3=\frac{N_0}{v_0}\left(\frac{9}{2}-\frac{2}{3}\right)v_0^3=\frac{23}{6}N_0v_0^2", "i3"))
    L.paso("c) El tramo que baja")\
        .di("Multiplicamos ve por la recta que baja.".replace("ve por", dice("v", "ve") + " por"), V(foco=3),
            M(r"v\,N_4(v)=v\cdot\frac{2N_0}{v_0}(4v_0-v)=\frac{2N_0}{v_0}\left(4v_0v-v^2\right)"))\
        .di("Regla de la potencia término por término.", M(r"\int\left(4v_0v-v^2\right)dv=4v_0\,\frac{v^2}{2}-\frac{v^3}{3}=2v_0v^2-\frac{v^3}{3}"))\
        .di("Arriba, en cuatro ve cero; abajo, en tres ve cero.".replace("cuatro ve cero", dice("4v₀", "cuatro ve cero")).replace("tres ve cero", dice("3v₀", "tres ve cero")),
            M(r"\left[2v_0v^2-\tfrac{v^3}{3}\right]_{4v_0}=32v_0^3-\tfrac{64}{3}v_0^3=\tfrac{32}{3}v_0^3\qquad \left[\,\cdot\,\right]_{3v_0}=18v_0^3-9v_0^3=9v_0^3"))\
        .di("Restamos y multiplicamos por la constante de afuera.", M(r"I_4=\frac{2N_0}{v_0}\left(\frac{32}{3}-9\right)v_0^3=\frac{2N_0}{v_0}\cdot\frac{5}{3}v_0^3=\frac{10}{3}N_0v_0^2", "i4"))
    L.paso("c) Rapidez media")\
        .di("Sumamos las cuatro.", M(r"\int vN\,dv=\left(\frac14+\frac32+\frac{23}{6}+\frac{10}{3}\right)N_0v_0^2=\frac{107}{12}N_0v_0^2"))\
        .di("Dividimos entre el total de moléculas.", M(r"\overline{v}=\frac{\frac{107}{12}N_0v_0^2}{\frac{23}{6}N_0v_0}=\frac{107}{46}\,v_0"))\
        .di(f"Da {n(vm, 3)} veces ve cero.".replace("ve cero", s("v_0")), R(rf"\overline{{v}}={tex(vm, 3)}\,v_0", "rc"), V(foco="media"))
    L.responde("a", r"N=\tfrac{23}{6}N_0v_0", N, "N0v0").responde("b", r"v_{mp}=3v_0", 3.0, "v0").responde("c", rf"\overline{{v}}={tex(vm, 3)}\,v_0", vm, "v0")
    L.usa(r"v_{\text{rms}}=\sqrt{\frac{3k_B T}{m}}=\sqrt{\frac{3RT}{M}}", r"\frac{1}{2}m(v^2)_{\text{med}}=\frac{3k_B T}{2}")
    return L


# ============================================================================ E15
def _rectangulo(id: str, fuente: dict, incisos: str = "recomendado") -> Leccion:
    nm = 2.0
    g = Gas(nm, "dia")
    g.pone("A", 1.22e5, 9.00e-2)
    g.pone("B", 1.22e5, 7.00e-2)
    g.pone("C", 2.03e5, 7.00e-2)
    g.pone("D", 2.03e5, 9.00e-2)
    pr = [("A", "B", "isobarico"), ("B", "C", "isocorico"), ("C", "D", "isobarico"), ("D", "A", "isocorico")]
    res = {a + b: g.proceso(a, b, t) for a, b, t in pr}
    W = sum(r["W"] for r in res.values())
    QH = res["BC"]["Q"] + res["CD"]["Q"]
    QC = res["DA"]["Q"] + res["AB"]["Q"]
    e = W / QH
    lista = [(a + "→" + b, res[a + b]) for a, b, _ in pr]
    L = Leccion(id, "Ciclo rectangular de una máquina térmica", tema="6.11", nivel=3, fuente=fuente, claves=["ciclo", "trabajo neto", "eficiencia", "diatómico"],
                enunciado="Dos moles de gas ideal diatómico (5 grados de libertad) recorren el ciclo rectangular ABCD: $p_{baja}=1{,}22\\times10^5$ Pa, $p_{alta}=2{,}03\\times10^5$ Pa, $V$ entre $7{,}00\\times10^{-2}$ y $9{,}00\\times10^{-2}$ m³. a) Trabajo neto, b) calor total absorbido, c) calor total liberado, d) eficiencia.",
                visual=visual_pv(g, pr, valores=valores_visual(lista)))
    L.paso("Qué nos dan y qué piden")\
        .di("Un ciclo que en el diagrama es un rectángulo, recorrido en el sentido del reloj.", DATO("dos moles"),
            LISTA(["$n=2$ mol, gas **diatómico**", "Presiones: $1{,}22\\times10^5$ y $2{,}03\\times10^5$ Pa", "Volúmenes: $0{,}0700$ y $0{,}0900$ m³"]), V(foco="ciclo"))\
        .di("Sentido del reloj quiere decir que es una máquina: entrega trabajo.", NOTA("Horario: máquina térmica ($W>0$). Antihorario: refrigerador ($W<0$)."))\
        .di("Diatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{5}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    L.paso("La idea")\
        .di("El trabajo neto es el área encerrada. El calor absorbido es la suma de los calores positivos.", V(foco="neto"), M(r"W_{neto}=\text{área del rectángulo}"), M(r"e=\frac{W_{neto}}{Q_H}", "ef", "azul"))\
        .por_que("En un ciclo el gas vuelve a su estado inicial: $\\Delta U=0$ y por la primera ley $W_{neto}=Q_{neto}$.")
    L.paso("a) Trabajo neto")\
        .di("Sumando el trabajo isobárico de expansión y el de compresión desde la hoja queda el área del rectángulo: base por altura.", F(r"W=p(V_2-V_1)"), M(r"W_{neto}=(p_{alta}-p_{baja})(V_{der}-V_{izq})"))\
        .di("Sustituimos las dos presiones en pascales y los dos volúmenes en metros cúbicos.", M(r"=(2{,}03\times10^5-1{,}22\times10^5)(0{,}0900-0{,}0700)"))\
        .di("Resolvemos primero las dos restas dentro de cada paréntesis.", M(r"=(8{,}1\times10^4)(0{,}0200)"))\
        .di(f"Al multiplicar ambos factores obtenemos un trabajo neto de {nu(W, 'J', 4)}.", R(rf"W_{{neto}}={tex(W, 4, u='J')}", "ra"))
    p = L.paso("Temperaturas de los cuatro estados")
    estado_T(p, g, "A", primero=True, voz="Las necesitamos para el calor. Despejamos la temperatura del gas ideal y la calculamos en A.")
    for k in "BCD":
        estado_T(p, g, k)
    p.di("Las cuatro juntas.", tabla_estados(g, "ABCD"), V(foco="estados"))
    p = L.paso("Proceso A → B (isobárico)")
    explica_proceso(p, g, "A", "B", "isobarico")
    p = L.paso("Proceso B → C (isocórico)")
    explica_proceso(p, g, "B", "C", "isocorico")
    p = L.paso("Proceso C → D (isobárico)")
    explica_proceso(p, g, "C", "D", "isobarico")
    p = L.paso("Proceso D → A (isocórico)")
    explica_proceso(p, g, "D", "A", "isocorico")
    L.paso("Juntar todo en una tabla")\
        .di("Ponemos los tres valores de cada proceso. La suma de delta U tiene que dar cero: es la revisión.".replace("delta U", s("ΔU")), tabla_quw(lista), V(foco="barras"))\
        .di("Da cero, vamos bien.", NOTA("Si $\\sum\\Delta U\\neq0$, hay un error en alguna temperatura."))
    L.paso("b) y c) Calor absorbido y liberado")\
        .di("Absorbido: los calores positivos, B a C y C a D.", M(rf"Q_H={tex(res['BC']['Q'], 5)}+{tex(res['CD']['Q'], 5)}"), R(rf"Q_H={tex(QH, 5, u='J', sci='no')}", "rb"))\
        .di("Liberado: los negativos, D a A y A a B.", M(rf"Q_C={tex(res['DA']['Q'], 5)}{tex(res['AB']['Q'], 4, signo=True)}"), R(rf"Q_C={tex(QC, 5, u='J', sci='no')}", "rc"))\
        .di("Comprobación: la suma de los dos es el trabajo neto.", NOTA(rf"${tex(QH, 5, sci='no')}{tex(QC, 5, sci='no')}={tex(QH + QC, 4)}$ J"))
    L.paso("d) Eficiencia")\
        .di("Trabajo entre calor absorbido.", F(r"e=\frac{W}{Q_H}"), M(rf"e=\frac{{{tex(W, 4)}}}{{{tex(QH, 5, sci='no')}}}={tex(e, 3)}"))\
        .di(f"Muy baja: {n(e * 100, 3)} por ciento.", R(rf"e={tex(e * 100, 3)}\,\%", "rd"))\
        .di("Un error común es dividir entre el calor neto o entre el liberado.", OJO("Abajo va solo el calor **absorbido**, no el neto."))
    L.responde("a", rf"W={tex(W, 4)}\ \text{{J}}", W, "J").responde("b", rf"Q_H={tex(QH, 5, sci='no')}\ \text{{J}}", QH, "J").responde("c", rf"Q_C={tex(QC, 5, sci='no')}\ \text{{J}}", QC, "J").responde("d", rf"e={tex(e * 100, 3)}\,\%", e * 100, "%", 0.02)
    L.usa(r"pV=nRT", r"W=p(V_2-V_1)", r"\Delta U=nC_V\Delta T", r"e=W/Q_H")
    return L


@leccion("E15")
def e15() -> Leccion:
    return _rectangulo("E15", _f(15, 5))

# ============================================================================ E16
@leccion("E16")
def e16() -> Leccion:
    nm, T, Va, Vb = 3.20, 680.0, 0.064, 0.256
    g = Gas(nm, "dia")
    g.pone("A", v=Va, t=T)
    g.pone("B", v=Vb, t=T)
    Tc = T * (Va / Vb) ** (g.g - 1)
    g.pone("C", v=Vb, t=Tc)
    pr = [("A", "B", "isotermico"), ("B", "C", "isocorico"), ("C", "A", "adiabatico")]
    res = {a + b: g.proceso(a, b, t) for a, b, t in pr}
    lista = [(a + "→" + b, res[a + b]) for a, b, _ in pr]
    L = Leccion("E16", "Ciclo isotérmica · isocórica · adiabática", tema="6.11", nivel=4, fuente=_f(16, 5), claves=["ciclo", "isotérmico", "adiabático", "eficiencia"],
                enunciado="3,20 mol de gas ideal diatómico recorren: A→B expansión isotérmica a 680 K de 0,064 a 0,256 m³; B→C isocórico con presión que disminuye; C→A compresión adiabática. a) $p$, $V$, $T$ en A, B y C. b) $\\Delta U$, $W$ y $Q$ de cada proceso y del ciclo. c) Eficiencia.",
                visual=visual_pv(g, pr, valores=valores_visual(lista)))
    L.paso("Qué nos dan y qué piden")\
        .di("Tres procesos que forman un ciclo. Anotamos lo que sabemos de cada punto.", DATO("680 K"),
            TABLA(["", "$p$", "$V$ (m³)", "$T$ (K)"], [["A", "?", "$0{,}064$", "$680$"], ["B", "?", "$0{,}256$", "$680$"], ["C", "?", "$0{,}256$", "?"]]), V(foco="ciclo"))\
        .di("B y C tienen el mismo volumen, y A y B la misma temperatura. Con eso se arma todo.")\
        .sale_de(("$V_C=0{,}256$", "B→C es isocórico: mismo volumen que B"))
    L.paso("Constantes del gas diatómico")\
        .di("De la hoja: la capacidad a volumen constante del diatómico, y la de presión constante, que es una erre más.", F(r"C_V=\frac{5}{2}R"), F(r"C_P=C_V+R"))\
        .di("Sumamos y ponemos el número de ce ve.".replace("ce ve", s("C_V", "ce ve")), M(r"C_P=\frac{5}{2}R+R=\frac{7}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))\
        .di("Gama es el cociente de las dos: las erres se cancelan.", F(r"\gamma=\frac{C_P}{C_V}"), M(r"\gamma=\frac{\frac{7}{2}R}{\frac{5}{2}R}=\frac{7}{5}=1{,}40"))
    L.paso("Presiones en A y en B")\
        .di("Gas ideal en el punto A.", F(r"pV=nRT\;\Rightarrow\;p=\frac{nRT}{V}"), V(foco="A"))\
        .di("Sustituimos.", M(r"p_A=\frac{(3{,}20)(8{,}314)(680)}{0{,}064}"), M(rf"p_A={tex(g.p('A'), 4, u='Pa')}", "pa", "verde"))\
        .di("En B cambia solo el volumen, que es cuatro veces mayor: la presión es cuatro veces menor.", M(rf"p_B=\frac{{(3{{,}}20)(8{{,}}314)(680)}}{{0{{,}}256}}={tex(g.p('B'), 4, u='Pa')}", "pb", "verde"), V(foco="B"))
    L.paso("Temperatura en C: despeje con la adiabática")\
        .di("De C a A es adiabático: usamos la relación de temperatura y volumen de la hoja.", F(r"TV^{\gamma-1}=\text{cte}"), V(foco="CA"))\
        .di("La escribimos entre C y A.", M(r"T_CV_C^{\gamma-1}=T_AV_A^{\gamma-1}"))\
        .di("Dividimos los dos lados entre el volumen de C elevado a gama menos uno, para dejar sola la temperatura de C.", M(r"T_C=T_A\,\frac{V_A^{\gamma-1}}{V_C^{\gamma-1}}"))\
        .di("Dos potencias con el mismo exponente se pueden dividir dentro de un solo paréntesis.", M(r"T_C=T_A\left(\frac{V_A}{V_C}\right)^{\gamma-1}", "tc", "azul"))
    L.paso("Temperatura en C: los números")\
        .di("Sustituimos: gama menos uno es cero coma cuatro.", M(r"T_C=680\left(\frac{0{,}064}{0{,}256}\right)^{0{,}40}=680\,(0{,}25)^{0{,}40}"))\
        .di("En la calculadora, cero coma veinticinco elevado a cero coma cuatro.", M(rf"T_C=680\,({tex(0.25 ** 0.4, 5)})"))\
        .di(f"Da {nu(Tc, 'K', dec=2)}. Guardamos los dos decimales: con ellos sale justa la presión de C.", M(rf"T_C={tex(Tc, dec=2, u='K')}", "rtc", "verde"))
    L.paso("Presión en C y tabla de estados")\
        .di("Gas ideal en C.", M(rf"p_C=\frac{{(3{{,}}20)(8{{,}}314)({tex(Tc, dec=2)})}}{{0{{,}}256}}={tex(g.p('C'), 4, u='Pa')}", "pc", "verde"), V(foco="C"))\
        .di("Ya está el inciso a.", tabla_estados(g, "ABC", resaltar=[[0, 1], [1, 1], [2, 1], [2, 3]]), V(foco="estados"))
    p = L.paso("b) Proceso A → B (isotérmico)")
    explica_proceso(p, g, "A", "B", "isotermico")
    p = L.paso("b) Proceso B → C (isocórico)")
    explica_proceso(p, g, "B", "C", "isocorico")
    p = L.paso("b) Proceso C → A (adiabático)")
    explica_proceso(p, g, "C", "A", "adiabatico")
    c = cierre_ciclo(L, lista, titulo="b) y c) Ciclo completo y eficiencia", id_e="rc")
    W, e = c["W"], c["e"]
    for k in "ABC":
        L.responde("a", rf"p_{k}={tex(g.p(k), 4)}\ \text{{Pa}}", g.p(k), "Pa")
    L.responde("a", rf"T_C={tex(Tc, dec=2)}\ \text{{K}}", Tc, "K").responde("b", rf"W_{{neto}}={tex(W, 4)}\ \text{{J}}", W, "J").responde("c", rf"e={tex(e * 100, 3)}\,\%", e * 100, "%")
    L.usa(r"pV=nRT", r"TV^{\gamma-1}=\text{cte}", r"W=nRT\ln(V_2/V_1)", r"\Delta U=nC_V\Delta T", r"e=W/Q_H")
    return L

# ============================================================================ E17
@leccion("E17")
def e17() -> Leccion:
    W, Qc, Th, Tc = 120.0, 180.0, 630.0, 300.0
    Qh = W + Qc
    e, ec = W / Qh, 1 - Tc / Th
    L = Leccion("E17", "Motor entre 630 K y 300 K", tema="6.14", nivel=2, fuente=_f(17, 6), claves=["eficiencia", "Carnot", "máquina térmica"],
                aviso="El inciso c) es el ciclo de Carnot (sección 20.6): no entra en el III parcial, que llega hasta 20.5. Los incisos a) y b) sí entran.",
                enunciado="Un motor opera entre 630 K y 300 K. En cada ciclo produce 120 J de trabajo y rechaza 180 J a la fuente fría. a) Calor absorbido, b) eficiencia, c) eficiencia máxima posible.",
                visual={"tipo": "maquina", "modo": "motor", "QH": Qh, "QC": Qc, "W": W, "TH": Th, "TC": Tc, "u": "J"})
    L.paso("Qué nos dan y qué piden")\
        .di("Un motor: entra calor de la fuente caliente, sale trabajo y lo que sobra va a la fría.", DATO("120 J"),
            LISTA(["$W=120$ J", "$|Q_C|=180$ J", "$T_H=630$ K, $T_C=300$ K"]), V(foco="todo"))
    L.paso("a) Calor absorbido")\
        .di("Partimos de la primera ley de la hoja.", V(foco="balance"), F(r"\Delta U=Q-W"))\
        .di("En un ciclo el motor vuelve a su estado inicial: la energía interna no cambia y el calor neto es el que entra menos el que sale.", M(r"\Delta U_{\text{ciclo}}=0\;\Rightarrow\;W=Q_{\text{neto}}=|Q_H|-|Q_C|"))\
        .di("Despejamos el calor que entra.", M(r"|Q_H|=W+|Q_C|", "b", "azul"))\
        .di("Sumamos.", M(r"|Q_H|=120+180"), R(r"Q_H=300\ \text{J}", "ra"))\
        .por_que("En un ciclo el motor vuelve a su estado inicial, no guarda energía: $\\Delta U=0$.")
    L.paso("b) Eficiencia")\
        .di("Lo que obtengo entre lo que pago: la eficiencia de la hoja.", F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_C}{Q_H}\right|"), M(r"e=\frac{120}{300}"))\
        .di("Cuarenta por ciento.", R(r"e=0{,}40=40\,\%", "rb"), V(foco="eficiencia"))
    deriva_carnot(L.paso("c) De dónde sale el máximo: Carnot desde la hoja (no entra en el parcial)"))
    L.paso("c) Lo máximo posible: Carnot (no entra en el parcial)")\
        .di("Este inciso es de la sección veinte punto seis y no entra en el tercer parcial. Ninguna máquina entre esas dos temperaturas le gana a la de Carnot.", M(r"e_{\text{máx}}=1-\frac{T_C}{T_H}", "c", "azul"), V(foco="carnot"))\
        .di("Con temperaturas en kelvin.", M(r"e_{\text{máx}}=1-\frac{300}{630}=1-0{,}476"))\
        .di(f"El tope es {n(ec * 100, 3)} por ciento.", R(rf"e_{{\text{{máx}}}}={tex(ec, 3)}={tex(ec * 100, 3)}\,\%", "rc"))\
        .di("Cuarenta es menor que cincuenta y dos: el motor es posible.", OJO("Carnot **solo** con kelvin. No viene en la hoja y no entra en el III parcial (corte en la sección 20.5)."))\
        .sale_de(("$1-\\frac{T_C}{T_H}$", "eficiencia de Carnot, viene de la segunda ley"))
    L.responde("a", r"Q_H=300\ \text{J}", 300.0, "J").responde("b", r"e=40\,\%", 40.0, "%").responde("c", rf"e_{{\text{{máx}}}}={tex(ec * 100, 3)}\,\%", ec * 100, "%")
    L.usa(r"e=W/Q_H=1-|Q_C/Q_H|")
    L.te_toca("Un motor absorbe 500 J y rechaza 350 J por ciclo. ¿Eficiencia en %?", 30.0, "%", pista="$W=500-350$", solucion="$e=\\frac{150}{500}=30\\,\\%$")
    return L


# ============================================================================ E18
@leccion("E18")
def e18() -> Leccion:
    nm, T1, Va = 2.5, 800.0, 0.160
    g = Gas(nm, "dia")
    g.pone("A", v=Va, t=T1)
    g.pone("B", v=Va / 2, t=T1)
    g.pone("C", p=g.p("B"), v=0.060)
    g.pone("D", v=Va, t=g.t("C"))
    pr = [("A", "B", "isotermico"), ("B", "C", "isobarico"), ("C", "D", "isotermico"), ("D", "A", "isocorico")]
    res = {a + b: g.proceso(a, b, t) for a, b, t in pr}
    lista = [(a + "→" + b, res[a + b]) for a, b, _ in pr]
    L = Leccion("E18", "Refrigerador de cuatro etapas", tema="6.14", nivel=5, fuente=_f(18, 6), claves=["refrigerador", "coeficiente de desempeño", "ciclo"],
                enunciado="Un refrigerador usa 2,5 mol de gas diatómico: compresión isotérmica a 800 K de 0,160 m³ a la mitad; compresión isobárica hasta 0,060 m³; expansión isotérmica hasta el volumen inicial; proceso isocórico hasta la presión inicial. a) Diagrama p-V, b) tabla de $T$, $p$, $V$, c) $W$ y $Q$ de cada etapa, d) coeficiente de desempeño.",
                visual=visual_pv(g, pr, valores=valores_visual(lista)))
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro etapas. Le ponemos una letra al inicio de cada una, en orden.", DATO("800 K"),
            LISTA(["A→B: isotérmica a $800$ K, de $0{,}160$ a $0{,}080$ m³", "B→C: isobárica hasta $0{,}060$ m³", "C→D: isotérmica hasta $0{,}160$ m³", "D→A: isocórica, cierra el ciclo"]))\
        .di("La mitad de cero coma ciento sesenta es cero coma cero ochenta.", M(r"V_B=\frac{0{,}160}{2}=0{,}080\ \text{m}^3"))\
        .di("Diatómico: de la hoja, la capacidad a volumen constante.", F(r"C_V=\frac{5}{2}R"),
            M(rf"C_V=\frac{{5}}{{2}}(8{{,}}314)={tex(g.cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))
    L.paso("a) El diagrama p-V")\
        .di("Primero sube comprimiéndose por una isoterma.", V(foco="AB"))\
        .di("Luego camina hacia la izquierda a presión constante.", V(foco="BC"))\
        .di("Baja por otra isoterma, más fría, hasta el volumen inicial.", V(foco="CD"))\
        .di("Y sube derecho, a volumen constante, hasta cerrar.", V(foco="DA"), NOTA("Se recorre **antihorario**: es un refrigerador."))
    L.paso("b) Estado A")\
        .di("Volumen y temperatura ya están; falta la presión. La despejamos del gas ideal.", F(r"pV=nRT\;\Rightarrow\;p=\frac{nRT}{V}"), V(foco="A"))\
        .di("Sustituimos.", M(rf"p_A=\frac{{(2{{,}}5)(8{{,}}314)(800)}}{{0{{,}}160}}={tex(g.p('A'), 4, u='Pa')}", "pa", "verde"))
    L.paso("b) Estado B")\
        .di("Misma temperatura y la mitad del volumen: la presión se duplica.", M(rf"p_B=\frac{{(2{{,}}5)(8{{,}}314)(800)}}{{0{{,}}080}}={tex(g.p('B'), 4, u='Pa')}", "pb", "verde"), V(foco="B"))
    L.paso("b) Estado C")\
        .di("De B a C la presión no cambia.", M(rf"p_C=p_B={tex(g.p('C'), 4, u='Pa')}"), V(foco="C"))\
        .di("Del gas ideal, a presión constante la temperatura baja igual que el volumen.", F(r"pV=nRT"), M(r"\frac{V_B}{T_B}=\frac{V_C}{T_C}\;\Rightarrow\;T_C=T_B\,\frac{V_C}{V_B}", "ch"))\
        .di("Sustituimos.", M(r"T_C=800\left(\frac{0{,}060}{0{,}080}\right)=800\,(0{,}75)=600\ \text{K}", "tc", "verde"))
    L.paso("b) Estado D y tabla")\
        .di("De C a D la temperatura no cambia, y el volumen vuelve al inicial.", M(r"T_D=T_C=600\ \text{K}"), M(r"V_D=V_A=0{,}160\ \text{m}^3"), V(foco="D"))\
        .di("La presión con el gas ideal.", M(rf"p_D=\frac{{(2{{,}}5)(8{{,}}314)(600)}}{{0{{,}}160}}={tex(g.p('D'), 4, u='Pa')}", "pd", "verde"))\
        .di("Resumen.", tabla_estados(g, "ABCD", dt=0), V(foco="estados"))
    p = L.paso("c) Etapa A → B (isotérmica)")
    explica_proceso(p, g, "A", "B", "isotermico", dec_T=0)
    p = L.paso("c) Etapa B → C (isobárica)")
    explica_proceso(p, g, "B", "C", "isobarico", dec_T=0)
    p = L.paso("c) Etapa C → D (isotérmica)")
    explica_proceso(p, g, "C", "D", "isotermico", dec_T=0)
    p = L.paso("c) Etapa D → A (isocórica)")
    explica_proceso(p, g, "D", "A", "isocorico", dec_T=0)
    c = cierre_ciclo(L, lista, refri=True, titulo="c) y d) Ciclo completo y coeficiente de desempeño", id_e="rd")
    W, K = c["W"], c["K"]
    L.paso("d) Ojo con los nombres")\
        .di("Puede ser mayor que uno: no es una eficiencia.", OJO("En un refrigerador $Q_C$ es lo que el gas **absorbe**, y $Q_H$ lo que entrega."))
    for k in "ABCD":
        L.responde("b", rf"p_{k}={tex(g.p(k), 4)}\ \text{{Pa}}", g.p(k), "Pa")
    L.responde("c", rf"W_{{neto}}={tex(W, 4)}\ \text{{J}}", W, "J").responde("d", rf"K={tex(K, 3)}", K, "")
    L.usa(r"pV=nRT", r"W=nRT\ln(V_2/V_1)", r"W=p(V_2-V_1)", r"K=|Q_C|/|W|")
    return L

# ============================================================================ E19
@leccion("E19")
def e19() -> Leccion:
    from .ayudas import _rd  # redondeo igual al de la pizarra (mitad hacia arriba)
    nm, Ta = 5.0, 20.0 + 273.15
    pa, pb = 1.5, 30.0
    # Cadena como en el papel: cada número se redondea como se escribe y el siguiente se calcula con lo escrito.
    # Así cada cuenta, hecha con calculadora sobre lo que se ve, da justo lo que dice la pizarra.
    Va = _rd(nm * R_LATM * Ta / pa, 2)         # 80,19 L
    f06 = _rd(0.05 ** 0.6, 4)                  # 0,1657
    Vb = _rd(Va * f06, 2)                       # 13,29 L
    Tb = _rd(pb * Vb / (nm * R_LATM), 1)        # 971,7 K
    Vc = 2 * Vb
    Tc = 2 * Tb
    g = Gas(nm, "mono")
    for k, (p_, v_, t_) in {"a": (pa, Va, Ta), "b": (pb, Vb, Tb), "c": (pb, Vc, Tc), "d": (nm * R_LATM * Tc / Va, Va, Tc)}.items():
        g.e[k] = (nm * R_GAS * t_ / (v_ * 1e-3), v_ * 1e-3, t_)
    pd = nm * R_LATM * Tc / Va
    cv, cp = 1.5 * R_GAS, 2.5 * R_GAS
    dab, dbc, dda = _rd(Tb - Ta, 2), _rd(Tc - Tb, 1), _rd(Ta - Tc, 2)
    nrt = nm * R_GAS * Tc
    lnv = _rd(math.log(Va / Vc), 5)
    # Energías en joules enteros, como se escriben; W de b→c sale de restar lo escrito
    rab = {"Q": 0.0, "U": _rd(nm * cv * dab, 0)}
    rab["W"] = -rab["U"]
    rbc = {"Q": _rd(nm * cp * dbc, 0), "U": _rd(nm * cv * dbc, 0)}
    rbc["W"] = rbc["Q"] - rbc["U"]
    rcd = {"U": 0.0, "W": _rd(_rd(nrt, 0) * lnv, 0)}
    rcd["Q"] = rcd["W"]
    rda = {"W": 0.0, "U": _rd(nm * cv * dda, 0)}
    rda["Q"] = rda["U"]
    lista = [("a→b", rab), ("b→c", rbc), ("c→d", rcd), ("d→a", rda)]
    sTa, sTb, sTc = tex(Ta, dec=2), tex(Tb, dec=1), tex(Tc, dec=1)
    pr = [("a", "b", "adiabatico"), ("b", "c", "isobarico"), ("c", "d", "isotermico"), ("d", "a", "isocorico")]
    vis = visual_pv(g, pr, valores=valores_visual(lista), uv="L", up="atm", fv=1000, fp=1 / ATM)
    vis["estados"] = {"a": [Va, pa], "b": [Vb, pb], "c": [Vc, pb], "d": [Va, pd]}
    L = Leccion("E19", "Ciclo abcda monoatómico (con atm y litros)", tema="6.11", nivel=5, fuente=_f(19, 7), claves=["ciclo", "adiabático", "isobárico", "isotérmico", "monoatómico"],
                enunciado="Una máquina trabaja con 5 moles de gas monoatómico en el ciclo abcda: a→b adiabático de 1,5 a 30,0 atm; b→c isobárico; c→d isotérmico; d→a isocórico. $V_c=2V_b$ y $T_a=20{,}0\\ ^\\circ\\text{C}$. a) $V$ y $T$ en a, b, c, d. b) $Q$, $W$ y $\\Delta U$ de cada proceso. c) Eficiencia.",
                visual=vis)
    L.paso("Qué nos dan y qué piden")\
        .di("Del dibujo leemos presiones y tipos de proceso.", DATO("5 moles"),
            TABLA(["", "$p$ (atm)", "$V$", "$T$"], [["a", "$1{,}5$", "?", "$293{,}15$ K"], ["b", "$30{,}0$", "?", "?"], ["c", "$30{,}0$", "$2V_b$", "?"], ["d", "?", "$=V_a$", "$=T_c$"]]), V(foco="ciclo"))\
        .di("La temperatura de a se pasa a kelvin con la hoja.", F(r"T_K=T_{^\circ\text{C}}+273{,}15"), M(r"T_a=20{,}0+273{,}15=293{,}15\ \text{K}"))\
        .sale_de(("$V_d=V_a$", "d→a es vertical: isocórico"), ("$T_d=T_c$", "c→d es isotérmico"))
    L.paso("Constantes del gas monoatómico")\
        .di("De la hoja: la capacidad a volumen constante del monoatómico, y la de presión constante, que es una erre más.", F(r"C_V=\frac{3}{2}R"), F(r"C_P=C_V+R"))\
        .di("Con erre igual a ocho coma tres uno cuatro.",
            M(rf"C_V=\frac{{3}}{{2}}(8{{,}}314)={tex(cv, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"),
            M(rf"C_P=\frac{{3}}{{2}}R+R=\frac{{5}}{{2}}R=\frac{{5}}{{2}}(8{{,}}314)={tex(cp, 5)}\ \frac{{\text{{J}}}}{{\text{{mol}}\cdot\text{{K}}}}"))\
        .di("Gama es el cociente de las dos: las erres se cancelan.", F(r"\gamma=\frac{C_P}{C_V}"), M(r"\gamma=\frac{\frac{5}{2}R}{\frac{3}{2}R}=\frac{5}{3}"))
    L.paso("a) Volumen en a")\
        .di("Presión en atmósferas: conviene la erre de litro atmósfera de la hoja.".replace("erre de litro atmósfera", dice("R en L·atm", "erre de litro atmósfera")), F(r"pV=nRT\;\Rightarrow\;V=\frac{nRT}{p}"), V(foco="a"))\
        .di("Sustituimos.", M(r"V_a=\frac{(5)(0{,}08206)(293{,}15)}{1{,}5}"), M(rf"V_a={tex(Va, 4, u='L')}", "va", "verde"))
    L.paso("a) Volumen en b: despeje con la adiabática")\
        .di("De a a b es adiabático y conocemos las dos presiones.", F(r"pV^\gamma=\text{cte}"), M(r"p_aV_a^\gamma=p_bV_b^\gamma", "ad"), V(foco="ab"))\
        .di("Dividimos los dos lados entre la presión de b.", M(r"V_b^\gamma=V_a^\gamma\,\frac{p_a}{p_b}"))\
        .di("Para quitar el exponente gama, elevamos los dos lados a uno sobre gama: al multiplicar los exponentes queda uno.",
            M(r"\left(V_b^{\gamma}\right)^{1/\gamma}=V_b^{\gamma\cdot\frac{1}{\gamma}}=V_b^{1}=V_b"),
            M(r"V_b=V_a\left(\frac{p_a}{p_b}\right)^{1/\gamma}", "vb", "azul"))
    L.paso("a) Volumen en b: los números")\
        .di("Uno sobre gama es tres quintos, o sea cero coma seis.", M(r"\frac{1}{\gamma}=\frac{3}{5}=0{,}6"),
            M(r"V_b=V_a\left(\frac{1{,}5}{30{,}0}\right)^{0{,}6}=V_a\,(0{,}050)^{0{,}6}"))\
        .di("En la calculadora, cero coma cero cinco elevado a cero coma seis.", M(rf"V_b=({tex(Va, 4)})({tex(0.05 ** 0.6, 4)})={tex(Vb, 4, u='L')}", "vbn", "verde"))
    L.paso("a) Temperatura en b")\
        .di("Ya tenemos presión y volumen: despejamos la temperatura del gas ideal.", F(r"pV=nRT\;\Rightarrow\;T=\frac{pV}{nR}"))\
        .di("Sustituimos.", M(rf"T_b=\frac{{(30{{,}}0)({tex(Vb, 4)})}}{{(5)(0{{,}}08206)}}={tex(Tb, dec=1, u='K')}", "tb", "verde"), V(foco="b"))
    L.paso("a) Estados c y d")\
        .di("De b a c la presión no cambia y el volumen se duplica.", M(rf"V_c=2V_b=2\,({tex(Vb, 4)})={tex(Vc, 4, u='L')}"), V(foco="c"))\
        .di("Del gas ideal, a presión constante el volumen y la temperatura van juntos: la temperatura también se duplica.", F(r"pV=nRT"),
            M(r"\frac{V_c}{T_c}=\frac{V_b}{T_b}\;\Rightarrow\;T_c=T_b\,\frac{V_c}{V_b}=2T_b"),
            M(rf"T_c=2\,({tex(Tb, dec=1)})={tex(Tc, dec=1, u='K')}", "tc", "verde"))\
        .di("En d: misma temperatura que ce y mismo volumen que a.".replace("que ce", "que " + dice("c", "ce")),
            M(rf"T_d=T_c={tex(Tc, dec=1, u='K')}"), M(rf"V_d=V_a={tex(Va, 4, u='L')}"),
            M(rf"p_d=\frac{{nRT_d}}{{V_d}}=\frac{{(5)(0{{,}}08206)({tex(Tc, dec=1)})}}{{{tex(Va, 4)}}}={tex(pd, 4, u='atm')}", "pd"), V(foco="d"))\
        .di("Tabla de estados.", TABLA(["", "$p$ (atm)", "$V$ (L)", "$T$ (K)"], [["a", "$1{,}5$", f"${tex(Va, 4)}$", f"${tex(Ta, dec=1)}$"], ["b", "$30{,}0$", f"${tex(Vb, 4)}$", f"${tex(Tb, dec=1)}$"], ["c", "$30{,}0$", f"${tex(Vc, 4)}$", f"${tex(Tc, dec=1)}$"], ["d", f"${tex(pd, 4)}$", f"${tex(Va, 4)}$", f"${tex(Tc, dec=1)}$"]]), V(foco="estados"))
    L.paso("b) a → b, adiabático")\
        .di("No hay calor, así que partimos de la primera ley del formulario con Q igual a cero.", F(r"\Delta U = Q - W"), M(r"Q_{ab}=0"), V(foco="ab"))\
        .di("Delta U con las temperaturas.".replace("Delta U", s("ΔU")), F(r"\Delta U = nC_V\Delta T"), M(rf"\Delta U_{{ab}}=nC_V(T_b-T_a)=(5)(12{{,}}471)({sTb}-{sTa})"))\
        .di("Multiplicamos los dos primeros y restamos las temperaturas.", M(rf"\Delta U_{{ab}}=({tex(nm * cv, 5)})({tex(dab, dec=2)})"), M(rf"\Delta U_{{ab}}={tex(rab['U'], 5, u='J')}", "uab", "verde"))\
        .di("Despejando de la primera ley, el trabajo es lo mismo con signo contrario: lo comprimen.", M(rf"W_{{ab}}=0-\Delta U_{{ab}}=-\Delta U_{{ab}}={tex(rab['W'], 5, u='J')}", "wab", "verde"))
    L.paso("b) b → c, isobárico: calor")\
        .di("El calor a presión constante sale de la hoja con ce pe.".replace("ce pe", s("C_P")), F(r"dQ=nC_p\,dT"), V(foco="bc"))\
        .di("Sumamos todos los pedacitos: la integral de de te es el cambio total de la temperatura.".replace("de de te", "de " + s("dT", "de te"), 1),M(r"Q_{bc}=nC_P\int_{T_b}^{T_c}dT=nC_P(T_c-T_b)"))\
        .di("Sustituimos.", M(rf"Q_{{bc}}=(5)(20{{,}}785)({sTc}-{sTb})=({tex(nm * cp, 6)})({tex(dbc, dec=1)})"))\
        .di("Da casi cien mil joules.", M(rf"Q_{{bc}}={tex(rbc['Q'], 6, u='J', sci='no')}", "qbc", "verde"))
    L.paso("b) b → c, isobárico: energía y trabajo")\
        .di("Delta U, siempre con ce ve.".replace("Delta U", s("ΔU")).replace("ce ve", s("C_V")), F(r"\Delta U = nC_V\Delta T"),
            M(rf"\Delta U_{{bc}}=(5)(12{{,}}471)({tex(dbc, dec=1)})={tex(rbc['U'], 5, u='J')}", "ubc"))\
        .di("El trabajo se despeja de la primera ley: el calor menos el cambio de energía.", F(r"\Delta U = Q - W\;\Rightarrow\;W=Q-\Delta U"),
            M(rf"W_{{bc}}={tex(rbc['Q'], 6, sci='no')}-{tex(rbc['U'], 5)}={tex(rbc['W'], 5, u='J')}", "wbc", "verde"))
    L.paso("b) c → d, isotérmico")\
        .di("Temperatura constante: delta U es cero y el calor es igual al trabajo.".replace("delta U", s("ΔU")), M(r"\Delta U_{cd}=0\;\Rightarrow\;Q_{cd}=W_{cd}"), V(foco="cd"))\
        .di("Los volúmenes pueden ir en litros porque se dividen.", F(r"W_{cd}=nRT_c\ln\left(\frac{V_d}{V_c}\right)"))\
        .di("Sustituimos con la erre en joules.".replace("erre en joules", dice("R en joules", "erre en joules")), M(rf"=(5)(8{{,}}314)({sTc})\ln\left(\frac{{{tex(Va, 4)}}}{{{tex(Vc, 4)}}}\right)"))\
        .di("Multiplicamos adelante y sacamos el logaritmo.", M(rf"=({tex(nrt, 5)})({tex(lnv, 6)})"), M(rf"W_{{cd}}=Q_{{cd}}={tex(rcd['W'], 5, u='J')}", "wcd", "verde"))
    L.paso("b) d → a, isocórico")\
        .di("Volumen constante: trabajo cero.", M(r"W_{da}=0"), V(foco="da"))\
        .di("El calor es el cambio de energía interna; se enfría, sale negativo.", F(r"\Delta U = nC_V\Delta T"),
            M(rf"Q_{{da}}=\Delta U_{{da}}=(5)(12{{,}}471)({sTa}-{sTc})=({tex(nm * cv, 5)})({tex(dda, dec=2)})"))\
        .di("Da unos ciento tres mil joules que salen.", M(rf"Q_{{da}}={tex(rda['Q'], 6, u='J', sci='no')}", "qda", "verde"))
    c = cierre_ciclo(L, lista, titulo="c) Ciclo completo y eficiencia", id_e="rc")
    W, e = c["W"], c["e"]
    L.paso("Cuidado con las unidades")\
        .di("Litros y atmósferas solo en los estados; las energías, en joules.", OJO("$R=0{,}08206$ para hallar $V$ y $T$; $R=8{,}314$ para energías."))
    L.responde("a", rf"V_a={tex(Va, 4)}\ \text{{L}}", Va, "L").responde("a", rf"V_b={tex(Vb, 4)}\ \text{{L}}", Vb, "L").responde("a", rf"T_b={tex(Tb, dec=1)}\ \text{{K}}", Tb, "K").responde("a", rf"T_c={tex(Tc, dec=1)}\ \text{{K}}", Tc, "K")
    L.responde("b", rf"W_{{neto}}={tex(W, 4)}\ \text{{J}}", W, "J").responde("c", rf"e={tex(e * 100, 3)}\,\%", e * 100, "%")
    L.usa(r"pV=nRT", r"pV^\gamma=\text{cte}", r"W=nRT\ln(V_2/V_1)", r"\Delta U=Q-W", r"\Delta U=nC_V\Delta T", r"e=\frac{W}{Q_H}")
    return L

# ============================================================================ E20
@leccion("E20")
def e20() -> Leccion:
    eb, ea = 1 / 3, 5 / 9
    L = Leccion("E20", "Comparar dos máquinas sin números", tema="6.14", nivel=3, fuente=_f(20, 7), claves=["eficiencia", "álgebra", "máquinas"],
                enunciado="La máquina A, comparada con la B, produce por ciclo cinco veces el trabajo, recibe el triple de calor y descarga el doble. Determine la eficiencia de cada una.",
                visual={"tipo": "maquina", "modo": "dos", "B": {"QH": 3, "QC": 2, "W": 1}, "A": {"QH": 9, "QC": 4, "W": 5}})
    L.paso("Qué nos dan y qué piden")\
        .di("No hay números, solo comparaciones. Las escribimos en símbolos.", DATO("cinco veces el trabajo"),
            M(r"W_A=5W_B", "w"), M(r"Q_{H,A}=3Q_{H,B}", "h"), M(r"|Q_{C,A}|=2|Q_{C,B}|", "c"), V(foco="dos"))\
        .di("Piden las dos eficiencias.", M(r"e_A=\;?\qquad e_B=\;?"))
    L.paso("La idea: balance en cada máquina")\
        .di("La hoja da la eficiencia de dos formas. Las igualamos.", F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_C}{Q_H}\right|"), M(r"\frac{W}{Q_H}=1-\frac{|Q_C|}{Q_H}"))\
        .di("Multiplicamos los dos lados por el calor que entra: el trabajo es lo que entra menos lo que se descarga.", M(r"W=Q_H-|Q_C|"))\
        .di("Lo escribimos para cada máquina.", M(r"W_B=Q_{H,B}-|Q_{C,B}|", "bb", "azul"), M(r"W_A=Q_{H,A}-|Q_{C,A}|", "ba", "azul"))
    L.paso("Meter las comparaciones en A")\
        .di("Cambiamos todo lo de A por lo de B.", M(r"5W_B=3Q_{H,B}-2|Q_{C,B}|", "e1"))\
        .di("Y el trabajo de B por su balance.", M(r"5\left(Q_{H,B}-|Q_{C,B}|\right)=3Q_{H,B}-2|Q_{C,B}|"))\
        .di("Abrimos el paréntesis.", M(r"5Q_{H,B}-5|Q_{C,B}|=3Q_{H,B}-2|Q_{C,B}|"))\
        .di("Juntamos: calores que entran a la izquierda, descargados a la derecha.", M(r"5Q_{H,B}-3Q_{H,B}=5|Q_{C,B}|-2|Q_{C,B}|"), M(r"2Q_{H,B}=3|Q_{C,B}|", "rel"))\
        .di("Despejamos el cociente: dividimos los dos lados entre tres y entre el calor que entra.", M(r"\frac{|Q_{C,B}|}{Q_{H,B}}=\frac{2}{3}", "co", "verde"))
    L.paso("Eficiencia de B")\
        .di("La segunda forma de la hoja.", F(r"e=1-\left|\frac{Q_C}{Q_H}\right|"), M(r"e_B=1-\frac{2}{3}"))\
        .di("Un tercio.", R(r"e_B=\frac{1}{3}=33{,}3\,\%", "rb"))
    L.paso("Eficiencia de A")\
        .di("Cinco veces el trabajo entre tres veces el calor.", M(r"e_A=\frac{W_A}{Q_{H,A}}=\frac{5W_B}{3Q_{H,B}}=\frac{5}{3}\,e_B"))\
        .di("Cinco tercios de un tercio son cinco novenos.", M(r"e_A=\frac{5}{3}\cdot\frac{1}{3}"), R(r"e_A=\frac{5}{9}=55{,}6\,\%", "ra"), V(foco="dos"))\
        .di("Revisión: con entrada tres y descarga dos, B da uno. A recibe nueve, descarga cuatro y da cinco.", NOTA("$W_A=9-4=5$, que es $5\\times1$. Cuadra."))
    L.responde("", r"e_A=55{,}6\,\%", ea * 100, "%").responde("", r"e_B=33{,}3\,\%", eb * 100, "%").usa(r"e=1-|Q_C/Q_H|")
    return L

# ============================================================================ E21
@leccion("E21")
def e21() -> Leccion:
    g = 1.4
    rs = [(91, 8.5), (93, 9.0), (95, 9.8), (97, 10.5)]
    es = {o: 1 - r ** (-(g - 1)) for o, r in rs}
    inc = (es[97] - es[91]) / es[91] * 100
    rtx = {8.5: "8{,}5", 9.0: "9{,}0", 9.8: "9{,}8", 10.5: "10{,}5"}
    rvoz = {8.5: "ocho coma cinco", 9.0: "nueve", 9.8: "nueve coma ocho", 10.5: "diez coma cinco"}
    L = Leccion("E21", "Motor Otto y octanaje de la gasolina", tema="6.14", nivel=2, fuente=_f(21, 8), claves=["ciclo Otto", "relación de compresión"],
                enunciado="Relación de compresión máxima según octanaje: 91→8,5; 93→9,0; 95→9,8; 97→10,5. Calcule la eficiencia teórica máxima (Otto) con cada gasolina y el incremento porcentual entre usar 91 y usar 97.",
                visual={"tipo": "otto", "gamma": g, "puntos": [[r, round(es[o], 4), str(o)] for o, r in rs]})
    L.paso("Qué nos dan y qué piden")\
        .di("Cuatro gasolinas, cada una aguanta una compresión máxima.", TABLA(["Octano", "$r$"], [["91", "$8{,}5$"], ["93", "$9{,}0$"], ["95", "$9{,}8$"], ["97", "$10{,}5$"]]))\
        .di("La fórmula del ciclo Otto está en el formulario.", F(r"e_{otto}=1-\frac{1}{r^{\gamma-1}}", "f"), V(foco="curva"))
    L.paso("Cuánto vale gama")\
        .di("La mezcla es casi aire, que es diatómico. De la hoja, la capacidad a volumen constante y la de presión constante, que es una erre más.",
            F(r"C_V=\frac{5}{2}R"), F(r"C_P=C_V+R"), M(r"C_P=\frac{5}{2}R+R=\frac{7}{2}R"))\
        .di("Gama es el cociente: las erres se cancelan.", F(r"\gamma=\frac{C_P}{C_V}"), M(r"\gamma=\frac{\frac{7}{2}R}{\frac{5}{2}R}=\frac{7}{5}=1{,}40"))\
        .di("El exponente de la fórmula es gama menos uno.", M(r"\gamma-1=1{,}40-1=0{,}40", "gm"))
    for i, (o, r) in enumerate(rs):
        pot = r ** (g - 1)
        inv = 1 / pot
        L.paso(f"Octano {o}")\
            .di(f"Ponemos {rvoz[r]}.", M(rf"e_{{{o}}}=1-\frac{{1}}{{{rtx[r]}^{{0{{,}}40}}}}"), V(foco=i))\
            .di("La potencia primero, en la calculadora.", M(rf"{rtx[r]}^{{0{{,}}40}}={tex(pot, 5)}"))\
            .di("Luego uno entre ese número.", M(rf"\frac{{1}}{{{tex(pot, 5)}}}={tex(inv, 4)}"))\
            .di(f"Y se lo restamos a uno: {n(es[o] * 100, 3)} por ciento.", R(rf"e_{{{o}}}=1-{tex(inv, 4)}={tex(es[o], 4)}", f"r{o}"))
    L.paso("Las cuatro en tabla")\
        .di("Más compresión, más eficiencia.", TABLA(["Octano", "$r$", "$e$"], [[str(o), f"${tex(r, 3, recorta=True)}$", f"${tex(es[o] * 100, 3)}\\,\\%$"] for o, r in rs], resaltar=[[0, 2], [3, 2]]))
    L.paso("Incremento porcentual de 91 a 97")\
        .di("Porcentual significa comparado con el valor inicial.", M(r"\%=\frac{e_{97}-e_{91}}{e_{91}}\times100"))\
        .di("Sustituimos.", M(rf"=\frac{{{tex(es[97], 4)}-{tex(es[91], 4)}}}{{{tex(es[91], 4)}}}\times100"))\
        .di("Primero la resta de arriba.", M(rf"=\frac{{{tex(round(es[97], 4) - round(es[91], 4), 3)}}}{{{tex(es[91], 4)}}}\times100"))\
        .di(f"Aumenta {n(inc, 3)} por ciento.", R(rf"{tex(inc, 3)}\,\%", "ri"), V(foco="todo"))\
        .di("No confundas con la resta directa, que son puntos porcentuales.", OJO(rf"La resta simple da ${tex((es[97] - es[91]) * 100, 3)}$ puntos porcentuales; el incremento **relativo** es ${tex(inc, 3)}\,\%$."))
    for o, _ in rs:
        L.responde(str(o), rf"e={tex(es[o] * 100, 3)}\,\%", es[o] * 100, "%", 0.005)
    L.responde("inc", rf"{tex(inc, 3)}\,\%", inc, "%", 0.03).usa(r"e_{otto}=1-1/r^{\gamma-1}")
    L.te_toca("¿Qué eficiencia tiene un Otto con $r=12$? (en %)", (1 - 12 ** -0.4) * 100, "%", pista="$12^{0,4}$ primero.", solucion="$e=1-\\frac{1}{12^{0{,}4}}$")
    return L
