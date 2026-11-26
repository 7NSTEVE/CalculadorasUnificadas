"""Superexamen Q016–Q035: calor específico, calorimetría y cambios de fase."""

from __future__ import annotations

import math

from .ayudas import qtx, revisar_opciones
from .banco_dilatacion import _des, _sel
from .banco_recomendados import _cubo_frio
from .dsl import DATO, F, LISTA, M, NOTA, OJO, Leccion, R, T, TABLA, V, VISUAL, s
from .formato import dice, n, nu, tex
from .indice_superexamen import fuente
from .registro import igual_que, leccion

C = r"^\circ\text{C}"


def _j(x: float, c: int = 5) -> str:
    return tex(x, c, u="J", sci="no")


# ============================================================================ Q016
@leccion("Q016")
def q016() -> Leccion:
    L = _sel("Q016", "Mismo calor, distinta temperatura",
             "Dos sustancias A y B tienen la misma masa y reciben la misma cantidad de calor, pero la temperatura de A aumenta más que la de B. ¿Cuál afirmación es verdadera?",
             {"A": "A tiene un calor específico mayor que B", "B": "B tiene un calor específico mayor que A", "C": "A y B tienen el mismo calor específico", "D": "El calor específico de A y B son inversamente proporcionales"}, "B",
             claves=["calor específico"],
             visual={"tipo": "calor", "componentes": [{"n": "A", "m": 100, "T": 80, "fase": "caliente"}, {"n": "B", "m": 100, "T": 40, "fase": "frio"}], "necesita": [{"n": "calor a A", "Q": 100}, {"n": "calor a B", "Q": 100}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Misma masa y mismo calor. A se calienta más.", DATO("misma masa"), LISTA(["$m_A=m_B$", "$Q_A=Q_B$", "$\\Delta T_A>\\Delta T_B$"]))\
        .di("Piden comparar el calor específico.", M(r"c_A\ \;?\ \;c_B"))
    L.paso("La fórmula")\
        .di("El calor y la temperatura se conectan así.", F(r"Q=mc\,\Delta T"))\
        .di("Despejamos el calor específico.", M(r"c=\frac{Q}{m\,\Delta T}"))\
        .di("Con el mismo calor y la misma masa, el que sube más tiene el específico más pequeño.", M(r"\Delta T_A>\Delta T_B \;\Rightarrow\; c_A<c_B", "r", "verde"))\
        .por_que("El calor específico dice cuánto cuesta calentar algo. Si sube mucho con poco, es fácil de calentar: $c$ pequeño.")
    revisar_opciones(L, {
        "A": ("La A está al revés.", "El que sube más tiene $c$ menor."),
        "C": ("La C no: con el mismo específico subirían igual.", "Subirían lo mismo."),
        "D": ("La D no: no se relacionan así entre ellos.", "Cada $c$ es propio del material."),
        "B": ("La B es la correcta.", "$c_B>c_A$."),
    })
    return L.oficial("III Parcial ordinario II-2023", "B").usa(r"Q=mc\Delta T")


# ============================================================================ Q017
@leccion("Q017")
def q017() -> Leccion:
    L = _sel("Q017", "¿Qué es el calor específico?",
             "¿Cuál de las siguientes afirmaciones es verdadera sobre el calor específico?",
             {"A": "Es una medida de la cantidad total de calor que puede almacenar un material.", "B": "Es la cantidad de calor necesaria para elevar la temperatura de una unidad de masa de un material en una unidad de temperatura.",
              "C": "No depende de la composición química ni de la estructura del material.", "D": "Es inversamente proporcional a la capacidad del material para conducir el calor."}, "B", claves=["calor específico", "definición"])
    L.paso("La definición sale de la fórmula")\
        .di("No hay que memorizar: sale de la fórmula del calor.", F(r"Q=mc\,\Delta T"))\
        .di("Despejamos.", M(r"c=\frac{Q}{m\,\Delta T}", "c", "azul"))\
        .di("Se lee: calor, por cada unidad de masa y por cada grado.", M(r"[c]=\frac{\text{J}}{\text{kg}\cdot\text{K}}"), NOTA("Es el calor que cuesta subir **1 grado** a **1 kilogramo**."))
    revisar_opciones(L, {
        "A": ("La A no: el calor total depende también de la masa y del cambio de temperatura.", "Eso sería $Q$, no $c$."),
        "C": ("La C no: cada material tiene el suyo, por su composición.", "Cada material tiene su $c$."),
        "D": ("La D mezcla con la conductividad, que es otra cosa: la letra ka.".replace("letra ka", "letra " + dice("k", "ka")), "Conducir es $k$, no $c$."),
        "B": ("La B es exactamente lo que dice la fórmula.", "Calor por unidad de masa y por grado."),
    })
    return L.oficial("III Parcial ordinario I-2024", "B").usa(r"Q=mc\Delta T")


# ============================================================================ Q018
@leccion("Q018")
def q018() -> Leccion:
    L = _sel("Q018", "¿Conductividad o calor específico?",
             "Dos materiales A y B, de igual masa y temperatura inicial, reciben la misma cantidad de calor en el mismo tiempo. A alcanza una temperatura más alta que B. ¿Cuál afirmación lo explica mejor?",
             {"A": "El material A tiene mayor conductividad térmica que el material B", "B": "El material A tiene un calor específico menor que el material B", "C": "El material B tiene mayor conductividad térmica que el material A", "D": "El material B tiene un calor específico menor que el material A"}, "B",
             claves=["calor específico", "conductividad"],
             visual={"tipo": "calor", "componentes": [{"n": "A", "m": 100, "T": 80, "fase": "caliente"}, {"n": "B", "m": 100, "T": 40, "fase": "frio"}], "necesita": [{"n": "calor a A", "Q": 100}, {"n": "calor a B", "Q": 100}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Igual masa, igual calor, pero A llega más arriba.", DATO("igual masa"), LISTA(["$m_A=m_B$", "$Q_A=Q_B$", "$\\Delta T_A>\\Delta T_B$"]))\
        .di("Hay una trampa: meten la conductividad para confundir.", OJO("Conductividad $k$: qué tan rápido **pasa** el calor. Calor específico $c$: cuánto **sube** la temperatura."))
    L.paso("La fórmula que relaciona calor y temperatura")\
        .di("Solo aparece el calor específico.", F(r"Q=mc\,\Delta T"), M(r"\Delta T=\frac{Q}{mc}", "d", "azul"))\
        .di(f"Para que el cambio sea mayor, {s('c')} tiene que ser menor, porque está dividiendo.", M(r"\Delta T_A>\Delta T_B \;\Rightarrow\; c_A<c_B", "r", "verde"))
    revisar_opciones(L, {
        "A": ("La A no: ya recibieron el mismo calor, la conductividad no importa.", "El calor recibido ya es el mismo."),
        "C": ("La C no, por la misma razón.", "El calor recibido ya es el mismo."),
        "D": ("La D está al revés.", "Al que sube menos le corresponde $c$ mayor."),
        "B": ("La B es la correcta.", "$c_A<c_B$."),
    })
    return L.oficial("III Parcial ordinario II-2024", "B").usa(r"Q=mc\Delta T")


# ============================================================================ Q019 / Q020
def _bloque_al(q: str) -> Leccion:
    L = _sel(q, "Temperatura final de un bloque de aluminio",
             "Un bloque de aluminio de 0,500 kg está a 20,0 °C y absorbe $9{,}00\\times10^3$ J. Si $c=900$ J/(kg·K), la temperatura final será",
             {"A": "30 °C", "B": "40 °C", "C": "50 °C", "D": "60 °C"}, "B", claves=["calor específico", "temperatura final"],
             visual={"tipo": "calor", "componentes": [{"n": "aluminio", "m": 500, "T": 20, "fase": "solido"}], "necesita": [{"n": "calor absorbido", "Q": 9000}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Un solo bloque que recibe calor.", DATO("0,500 kg"), LISTA(["$m=0{,}500$ kg", "$T_i=20{,}0\\ ^\\circ\\text{C}$", "$Q=9{,}00\\times10^3$ J (lo absorbe, positivo)", "$c=900\\ \\frac{\\text{J}}{\\text{kg}\\cdot\\text{K}}$"]))\
        .di("Piden la temperatura final.", M(r"T_f=\;?"))
    L.paso("Despejar el cambio de temperatura")\
        .di("No hay cambio de fase, entonces.", F(r"Q=mc\,\Delta T"))\
        .di("Lo que multiplica pasa a dividir.", M(r"\Delta T=\frac{Q}{mc}"))\
        .di("Sustituimos.", M(r"\Delta T=\frac{9000}{(0{,}500)(900)}=\frac{9000}{450}"))\
        .di("Son veinte grados de subida.", M(r"\Delta T=20\ ^\circ\text{C}", "dt", "verde"))\
        .sale_de(("grados", "un cambio de 20 K es igual a un cambio de 20 °C"))
    L.paso("Temperatura final")\
        .di("Le sumamos a la inicial.", M(r"T_f=T_i+\Delta T=20{,}0+20"), R(r"T_f=40\ ^\circ\text{C}"))\
        .di("Cuidado con contestar el cambio en lugar del final.", OJO("$\\Delta T=20$ °C no es la respuesta: piden $T_f$."))
    revisar_opciones(L, {
        "A": ("La A no.", "Saldría con $\\Delta T=10$."),
        "C": ("La C no.", "Saldría con $\\Delta T=30$."),
        "D": ("La D tampoco.", "Saldría con $\\Delta T=40$."),
        "B": ("La B es la correcta: cuarenta grados.", "$20+20=40\\ ^\\circ\\text{C}$."),
    })
    L.responde("", r"T_f=40\ ^\circ\text{C}", 40.0, "°C").usa(r"Q=mc\Delta T")
    L.te_toca("Un bloque de cobre ($c=390$) de 0,200 kg a 25 °C absorbe 3900 J. ¿A qué temperatura llega, en °C?", 75.0, "°C", pista="$\\Delta T=\\frac{Q}{mc}$", solucion="$\\Delta T=\\frac{3900}{(0{,}200)(390)}=50$, entonces $T_f=75\\ ^\\circ\\text{C}$")
    return L


@leccion("Q019")
def q019() -> Leccion:
    return _bloque_al("Q019").oficial("III Parcial ordinario I-2026", "B")


@leccion("Q020")
def q020() -> Leccion:
    return _bloque_al("Q020")


# ============================================================================ Q021
@leccion("Q021")
def q021() -> Leccion:
    L = _sel("Q021", "Fusión, condensación y calor latente",
             "I. Para fundir un sólido se necesita energía para romper las fuerzas de atracción y luego para aumentar su temperatura. II. La condensación se produce al transferir energía al sistema, lo que permite que las moléculas permanezcan más juntas. III. El calor para un cambio de fase es proporcional al calor latente y a la masa. Son correctas:",
             {"A": "III", "B": "Todas", "C": "II y III", "D": "I"}, "A", claves=["cambio de fase", "calor latente"],
             visual={"tipo": "calentamiento", "m": 1.0, "Ti": -20, "Tf": 120})
    L.paso("Analizar la I")\
        .di("Durante la fusión la temperatura no cambia: toda la energía rompe enlaces.", V(etapa=1), M(r"Q=mL_f\qquad \Delta T=0"))\
        .di("La I dice que además sube la temperatura. Eso es falso.", T("**I es falsa**: mientras se funde, la temperatura no sube.", color="rojo"))
    L.paso("Analizar la II")\
        .di("Condensar es pasar de gas a líquido. Para eso el gas tiene que soltar energía, no recibirla.", V(etapa=3), M(r"Q_{\text{condensar}}=-mL_v"))\
        .di("La II dice que se le transfiere energía. Falsa.", T("**II es falsa**: al condensar, el sistema **entrega** calor.", color="rojo"))
    L.paso("Analizar la III")\
        .di("La fórmula dice masa por calor latente.", F(r"Q=\pm mL"))\
        .di("Es proporcional a las dos cosas. Verdadera.", T("**III es verdadera**.", color="verde"))
    revisar_opciones(L, {
        "B": ("La B no.", "I y II son falsas."),
        "C": ("La C no, porque la dos es falsa.", "II es falsa."),
        "D": ("La D no, la uno es falsa.", "I es falsa."),
        "A": ("La A: solo la tres.", "Solo III."),
    })
    return L.oficial("III Parcial extraordinario II-2023", "A").usa(r"Q=\pm mL")


# ============================================================================ Q022
@leccion("Q022")
def q022() -> Leccion:
    mf, mh, mv = 0.392, 0.190, 0.0700
    cf, ca, Lf, Lv = 470.0, 4190.0, 3.33e5, 2.256e6
    q1 = mh * Lf
    q2 = mh * ca * 100
    q3 = mf * cf * 100
    tot = q1 + q2 + q3
    disp = mv * Lv
    izq = mh * ca + mf * cf
    der = mv * ca
    Tf = (disp + der * 100 - q1) / (izq + der)
    L = _des("Q022", "Hielo en recipiente de hierro más vapor",
             "Recipiente de hierro de 0,392 kg con 0,190 kg de hielo, todo a 0 °C. Se agregan 0,0700 kg de vapor a 100 °C. A) Temperatura de equilibrio. B) Composición final. ($c_{Fe}=470$, $c_{agua}=4190$, $L_f=3{,}33\\times10^5$, $L_v=2{,}256\\times10^6$, SI).",
             claves=["calorimetría", "condensación", "equilibrio"],
             visual={"tipo": "calor", "Tf": round(Tf, 1), "componentes": [{"n": "hielo", "m": 190, "T": 0, "fase": "hielo"}, {"n": "hierro", "m": 392, "T": 0, "fase": "solido"}, {"n": "vapor", "m": 70, "T": 100, "fase": "vapor"}],
                     "necesita": [{"n": "fundir el hielo", "Q": q1}, {"n": "esa agua hasta 100 °C", "Q": q2}, {"n": "hierro hasta 100 °C", "Q": q3}], "disponible": [{"n": "condensar todo el vapor", "Q": disp}],
                     "final": [{"n": "agua", "m": 260}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Hielo y hierro a cero grados, y les llega vapor a cien.", DATO("0,392 kg"), DATO("0,190 kg"), LISTA(["Hierro: $0{,}392$ kg a $0\\ ^\\circ\\text{C}$", "Hielo: $0{,}190$ kg a $0\\ ^\\circ\\text{C}$", "Vapor: $0{,}0700$ kg a $100\\ ^\\circ\\text{C}$"]))\
        .di("No sabemos si al final queda vapor, hielo o solo agua. Primero se averigua eso.", M(r"T_{eq}=\;?"))
    L.paso("La idea: ¿alcanza el vapor para llegar a 100 °C?")\
        .di("Sumamos lo que costaría llevar todo a cien grados y lo comparamos con lo que da el vapor al condensarse.", V(foco="flujo"), F(r"Q=mc\,\Delta T\qquad Q=mL"))\
        .por_que("Si sobra vapor, la temperatura es 100 °C y ya. Si no alcanza, todo el vapor se condensa y la temperatura queda entre 0 y 100 °C.")
    L.paso("Lo que pide cada uno")\
        .di("Fundir el hielo.", M(rf"Q_1=(0{{,}}190)(3{{,}}33\times10^5)={_j(q1)}", "q1", "rojo"), V(etapa=0))\
        .di("Calentar esa agua de cero a cien.", M(rf"Q_2=(0{{,}}190)(4190)(100)={_j(q2)}", "q2", "rojo"), V(etapa=1))\
        .di("Calentar el hierro de cero a cien.", M(rf"Q_3=(0{{,}}392)(470)(100)={_j(q3)}", "q3", "rojo"), V(etapa=2))\
        .di("En total, sumamos los tres.", M(rf"Q_{{\text{{pide}}}}={_j(q1)}+{_j(q2)}+{_j(q3)}={_j(tot, 6)}", "tot"))
    L.paso("Comparar con el vapor")\
        .di("Lo máximo que puede dar condensándose todo.", M(rf"Q_{{\text{{vapor}}}}=(0{{,}}0700)(2{{,}}256\times10^6)={_j(disp, 6)}", "da"), V(foco="comparar"))\
        .di("No alcanza por poquito: se condensa todo y la temperatura queda un poco abajo de cien.", M(rf"{tex(disp, 6, sci='no')}<{tex(tot, 6, sci='no')} \;\Rightarrow\; 0<T_{{eq}}<100\ {C}", "cmp", "azul"))
    L.paso("Plantear la conservación de la energía")\
        .di("Lo que cede el vapor, condensarse y luego enfriarse, lo ganan el hielo y el hierro.", M(r"\underbrace{m_vL_v+m_vc_a(100-T)}_{\text{cede}}=\underbrace{m_hL_f+m_hc_aT+m_{Fe}c_{Fe}T}_{\text{ganan}}", "bal"))\
        .di("Primero los productos de masa por calor específico.", M(rf"m_vc_a=(0{{,}}0700)(4190)={tex(der, 4)}"), M(rf"m_hc_a=(0{{,}}190)(4190)={tex(mh * ca, 4)}"), M(rf"m_{{Fe}}c_{{Fe}}=(0{{,}}392)(470)={tex(mf * cf, 5)}"))\
        .di("Ponemos números, sin prisa.", M(rf"{tex(disp, 6, sci='no')}+{tex(der, 4)}\,(100-T)={tex(q1, 5, sci='no')}+{tex(mh * ca, 4)}\,T+{tex(mf * cf, 5)}\,T"))
    L.paso("Despejar la temperatura")\
        .di("Abrimos el paréntesis y juntamos las dos te de la derecha.".replace("dos te", "dos " + dice("T", "te")), M(rf"{tex(disp, 6, sci='no')}+{tex(der * 100, 5, sci='no')}-{tex(der, 4)}\,T={tex(q1, 5, sci='no')}+{tex(izq, 5)}\,T"))\
        .di("Las te a un lado sumando, los números al otro restando.".replace("Las te", "Las " + dice("T", "te")),
            M(rf"{tex(disp, 6, sci='no')}+{tex(der * 100, 5, sci='no')}-{tex(q1, 5, sci='no')}={tex(izq, 5)}\,T+{tex(der, 4)}\,T"), M(rf"{tex(disp + der * 100 - q1, 6, sci='no')}={tex(izq + der, 6)}\,T"))\
        .di("Dividimos.", M(rf"T=\frac{{{tex(disp + der * 100 - q1, 6, sci='no')}}}{{{tex(izq + der, 6)}}}"), R(rf"T_{{eq}}={tex(Tf, 3)}\ {C}", "ra"))\
        .di("Dio menos de cien y más de cero: el caso que supusimos era el correcto.", NOTA("Si hubiera salido más de 100 o menos de 0, el caso estaría mal escogido."))
    L.paso("Composición final")\
        .di("Todo el hielo se fundió y todo el vapor se condensó: solo queda agua.", M(r"m_{\text{agua}}=0{,}190+0{,}0700"), R(r"m_{\text{agua}}=0{,}260\ \text{kg}\quad(\text{y }0{,}392\ \text{kg de hierro})", "rb"), V(foco="final"))
    L.responde("A", rf"T_{{eq}}={tex(Tf, 3)}\ {C}", Tf, "°C").responde("B", r"0{,}260\ \text{kg de agua}", 0.260, "kg")
    return L.oficial("III Parcial extraordinario II-2023", 97.0, 0.26, tol=0.02).usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ Q023
@leccion("Q023")
def q023() -> Leccion:
    mh, ma = 0.045, 0.500
    ca, ch, Lf = 4190.0, 2100.0, 3.33e5
    T0, Th, Ta, Tf = 273.15, 258.15, 318.15, 307.23
    s1 = mh * ch * math.log(T0 / Th)
    s2 = mh * Lf / T0
    s3 = mh * ca * math.log(Tf / T0)
    s4 = ma * ca * math.log(Tf / Ta)
    tot = s1 + s2 + s3 + s4
    L = _des("Q023", "Entropía al derretir un cubo de hielo",
             "Un cubo de hielo de 0,045 kg a −15,00 °C se coloca en 0,500 kg de agua a 45,00 °C, en un recipiente aislado. El equilibrio es 34,08 °C. A) Cambio de entropía de cada componente. B) Del sistema. C) ¿Reversible o irreversible? ($c_a=4190$, $c_h=2100$, $L_f=3{,}33\\times10^5$).",
             claves=["entropía", "cambio de fase", "irreversible"],
             visual={"tipo": "entropia", "partes": [{"n": "hielo: calentar", "dS": s1}, {"n": "hielo: fundir", "dS": s2}, {"n": "agua fundida", "dS": s3}, {"n": "agua del vaso", "dS": s4}]})
    L.paso("Qué nos dan y qué piden")\
        .di("El hielo se calienta, se funde y luego se calienta como agua. El agua del vaso se enfría.", DATO("34,08"), LISTA(["Hielo: $0{,}045$ kg a $-15{,}00\\ ^\\circ\\text{C}$", "Agua: $0{,}500$ kg a $45{,}00\\ ^\\circ\\text{C}$", "Equilibrio: $34{,}08\\ ^\\circ\\text{C}$"]))\
        .di("Para entropía, todo en kelvin.", TABLA(["", "°C", "K"], [["hielo", "$-15{,}00$", "$258{,}15$"], ["fusión", "$0$", "$273{,}15$"], ["agua", "$45{,}00$", "$318{,}15$"], ["final", "$34{,}08$", "$307{,}23$"]]))
    L.paso("Las dos fórmulas de entropía")\
        .di("La definición: calor entre temperatura.", M(r"dS=\frac{dQ}{T}", "d", "azul"))\
        .di("Si se calienta sin cambiar de fase, se integra y sale un logaritmo.", M(r"\Delta S=\int_{T_i}^{T_f}\frac{mc\,dT}{T}=mc\,\ln\frac{T_f}{T_i}", "f1"))\
        .di("En el cambio de fase la temperatura es constante: es solo dividir.", M(r"\Delta S=\frac{Q}{T}=\frac{mL}{T}", "f2"))\
        .por_que("Estas fórmulas no están en el formulario: se deducen de $dS=dQ/T$. Si $T$ no cambia, sale de la integral; si cambia, aparece $\\ln$.")\
        .despacio((r"dQ=mc\,dT", "calor para un cambio pequeño"), (r"\Delta S=\int\frac{mc\,dT}{T}", "se sustituye"), (r"\int\frac{dT}{T}=\ln T", "integral conocida"), (r"mc(\ln T_f-\ln T_i)=mc\ln\frac{T_f}{T_i}", "resta de logaritmos"))
    L.paso("A) Hielo: calentarlo hasta 0 °C")\
        .di("Del hielo a la fusión, con el específico del hielo.", V(foco=0), M(r"\Delta S_1=(0{,}045)(2100)\ln\frac{273{,}15}{258{,}15}"), M(rf"\Delta S_1={tex(s1, 3)}\ \text{{J/K}}", "s1", "verde"))
    L.paso("A) Hielo: fundirlo")\
        .di("A temperatura constante, doscientos setenta y tres coma quince kelvin.", V(foco=1), M(r"\Delta S_2=\frac{(0{,}045)(3{,}33\times10^5)}{273{,}15}"), M(rf"\Delta S_2={tex(s2, 4)}\ \text{{J/K}}", "s2", "verde"))\
        .di("El hielo, hasta fundirse, suma esto.", R(rf"\Delta S_{{\text{{hielo}}}}={tex(s1 + s2, 4)}\ \text{{J/K}}", "rh"))
    L.paso("A) El agua que salió del hielo")\
        .di("Esa agua sube de cero a treinta y cuatro grados.", V(foco=2), M(r"\Delta S_3=(0{,}045)(4190)\ln\frac{307{,}23}{273{,}15}"), M(rf"\Delta S_3={tex(s3, 4)}\ \text{{J/K}}", "s3", "verde"))
    L.paso("A) El agua del vaso")\
        .di("Se enfría: el logaritmo sale negativo.", V(foco=3), M(r"\Delta S_4=(0{,}500)(4190)\ln\frac{307{,}23}{318{,}15}"), M(rf"\Delta S_4={tex(s4, 4)}\ \text{{J/K}}", "s4", "rojo"))\
        .di("La cátedra junta las dos aguas en un solo dato.", R(rf"\Delta S_{{\text{{agua}}}}={tex(s3, 4)}{tex(s4, 4)}={tex(s3 + s4, 4)}\ \text{{J/K}}", "rw"))
    L.paso("B) y C) Total")\
        .di("Sumamos todo.", M(rf"\Delta S_{{\text{{sistema}}}}={tex(s1 + s2, 4)}+({tex(s3 + s4, 4)})"), R(rf"\Delta S_{{\text{{sistema}}}}={tex(tot, 3, signo=True)}\ \text{{J/K}}", "rb"), V(foco="total"))\
        .di("Salió positivo: el proceso es irreversible.", T("**C)** Como $\\Delta S>0$, el proceso es **irreversible**: el hielo no se vuelve a formar solo."))
    L.responde("A", rf"\Delta S_{{\text{{hielo}}}}={tex(s1 + s2, 4)}\ \text{{J/K}}", s1 + s2, "J/K").responde("A", rf"\Delta S_{{\text{{agua}}}}={tex(s3 + s4, 4)}\ \text{{J/K}}", s3 + s4, "J/K").responde("B", rf"\Delta S={tex(tot, 3, signo=True)}\ \text{{J/K}}", tot, "J/K")
    return L.oficial("III Parcial extraordinario I-2024", 60.20, -51.00, 9.20).usa(r"dS=\frac{dQ}{T}", r"\Delta S=mc\ln\frac{T_f}{T_i}")


# ============================================================================ Q024
@leccion("Q024")
def q024() -> Leccion:
    L = _sel("Q024", "¿Por qué tarda más en evaporarse?",
             "Un cubo de hielo a 0 °C en una olla se derrite rápido; pero cuando el agua llega a 100 °C tarda mucho más en evaporarse por completo. ¿Cuál afirmación lo explica?",
             {"A": "La temperatura del agua aumenta durante la vaporización, por lo que se requiere más energía", "B": "La temperatura del hielo aumenta durante la fusión, por lo que se requiere más energía",
              "C": "El calor latente de fusión es mayor que el calor latente de vaporización para el agua", "D": "El calor latente de fusión es menor que el calor latente de vaporización para el agua"}, "D",
             claves=["calor latente", "curva de calentamiento"], visual={"tipo": "calentamiento", "m": 1.0, "Ti": -10, "Tf": 110})
    L.paso("Qué preguntan")\
        .di("La misma masa de agua: derretirla es rápido, evaporarla es lento.", DATO("tarda mucho más"), M(r"Q_{\text{fundir}}=mL_f\qquad Q_{\text{evaporar}}=mL_v"))\
        .di("En los dos tramos la temperatura no cambia: son los planos de la curva.", V(etapa=1))
    L.paso("Comparar los calores latentes")\
        .di("Los valores del agua.", M(r"L_f=3{,}34\times10^5\ \frac{\text{J}}{\text{kg}}"), M(r"L_v=2{,}256\times10^6\ \frac{\text{J}}{\text{kg}}"))\
        .di("Evaporar cuesta casi siete veces más.", M(rf"\frac{{L_v}}{{L_f}}\approx {tex(2.256e6 / 3.34e5, 2)}", "raz", "verde"), V(etapa=3))\
        .por_que("Para fundir solo se aflojan los enlaces. Para evaporar hay que separar las moléculas del todo, y eso cuesta mucho más.")
    revisar_opciones(L, {
        "A": ("La A no: en la ebullición la temperatura se queda en cien.", "La temperatura no cambia al hervir."),
        "B": ("La B no: en la fusión se queda en cero.", "La temperatura no cambia al fundir."),
        "C": ("La C está al revés.", "$L_f<L_v$."),
        "D": ("La D es la correcta.", "$L_f$ es menor que $L_v$."),
    })
    return L.oficial("III Parcial ordinario II-2024", "D").usa(r"Q=\pm mL")


# ============================================================================ Q025
@leccion("Q025")
def q025() -> Leccion:
    L = _sel("Q025", "¿Por qué hay tramos de temperatura constante?",
             "Un bloque de hielo a −10 °C se calienta con flujo de calor constante hasta ser vapor a 110 °C. La temperatura solo se mantiene constante en ciertos lapsos. ¿Cuál es la causa?",
             {"A": "La energía se convierte en trabajo mecánico para expandir el sistema, sin afectar la energía interna.", "B": "La energía compensa la pérdida de calor al ambiente.",
              "C": "La energía se emplea en modificar la estructura microscópica de la materia, sin cambiar la energía cinética promedio de las moléculas.", "D": "El sistema alcanzó el equilibrio térmico con el ambiente."}, "C",
             claves=["cambio de fase", "energía cinética"], visual={"tipo": "calentamiento", "m": 1.0, "Ti": -10, "Tf": 110})
    L.paso("Los tramos planos")\
        .di("Miremos la curva: hay dos tramos planos, en cero y en cien grados.", V(etapa=1), LISTA(["$0\\ ^\\circ\\text{C}$: se funde", "$100\\ ^\\circ\\text{C}$: hierve"]))\
        .di("Sigue entrando calor, pero la temperatura no sube.", V(etapa=3), M(r"Q=mL\qquad \Delta T=0"))
    L.paso("¿A dónde se va esa energía?")\
        .di("La temperatura mide la energía cinética promedio de las moléculas.", F(r"K_{\text{prom}}=\frac{3}{2}k_BT"))\
        .di("Si la temperatura no cambia, esa energía cinética tampoco.", M(r"T\ \text{constante} \;\Rightarrow\; K_{\text{prom}}\ \text{constante}"))\
        .di("Entonces la energía se usa en romper enlaces: cambiar la estructura.", T("La energía separa las moléculas (energía potencial), no las acelera."))
    revisar_opciones(L, {
        "A": ("La A no: la energía interna sí cambia, porque se rompen enlaces.", "La energía interna sí aumenta."),
        "B": ("La B no: no se habla de pérdidas, y la temperatura sube en los otros tramos.", "No hay pérdidas en el problema."),
        "D": ("La D no: si fuera equilibrio, nunca llegaría a vapor.", "El calentamiento sigue."),
        "C": ("La C es la correcta: cambia la estructura, no la rapidez.", "Cambia la estructura; $K_{\\text{prom}}$ no."),
    })
    return L.oficial("III Parcial extraordinario II-2025", "C").usa(r"Q=\pm mL", r"K_{prom}=\frac32 k_BT")


# ============================================================================ Q026 / Q028
def _mezcla_aguas(q: str, m1: float, T1: float, m2: float, T2: float, ops: dict, corr: str, motivos: dict, of: str) -> Leccion:
    Tf = (m1 * T1 + m2 * T2) / (m1 + m2)
    prom = (T1 + T2) / 2
    L = _sel(q, "Mezcla de agua fría y caliente", f"Se mezclan {m1:g} g de agua a {T1:g} °C con {m2:g} g de agua a {T2:g} °C en un calorímetro ideal. Un estudiante predice {prom:g} °C por ser el promedio. La temperatura final estará", ops, corr,
             claves=["calorimetría", "mezcla"],
             visual={"tipo": "calor", "Tf": round(Tf, 2), "componentes": [{"n": f"{m1:g} g", "m": m1, "T": T1, "fase": "caliente" if T1 > T2 else "frio"}, {"n": f"{m2:g} g", "m": m2, "T": T2, "fase": "caliente" if T2 > T1 else "frio"}],
                     "necesita": [{"n": "gana la fría", "Q": 100}], "disponible": [{"n": "cede la caliente", "Q": 100}]})
    fria, cal = (m1, m2) if T1 < T2 else (m2, m1)
    mas = "fría" if fria > cal else "caliente"
    L.paso("Qué nos dan y qué piden")\
        .di("La misma sustancia, agua, pero con un poquito más de una que de la otra.", LISTA([f"${m1:g}$ g a ${T1:g}\\ ^\\circ\\text{{C}}$", f"${m2:g}$ g a ${T2:g}\\ ^\\circ\\text{{C}}$", "Sin pérdidas"]))\
        .di(f"El promedio simple da {n(prom, dec=0)}, pero solo vale con masas iguales.", M(rf"\frac{{{T1:g}+{T2:g}}}{{2}}={prom:g}\ {C}"))
    L.paso("La idea, sin cuentas")\
        .di(f"Hay más agua {mas}, así que la mezcla se inclina hacia ese lado, pero poquito.", T(f"Más masa de agua **{mas}** → la temperatura queda un poco hacia ese lado de {prom:g} °C."))\
        .por_que("La que tiene más masa cambia menos su temperatura: jala la mezcla hacia ella.")
    L.paso("Comprobarlo con la ecuación")\
        .di("Partimos de la casilla de calor sensible de la hoja. En un calorímetro ideal la suma de calores es cero y el calor específico se cancela.", F(r"Q=mc\Delta T"), M(r"\sum Q=0\;\Rightarrow\;m_1\cancel{c}(T-T_1)+m_2\cancel{c}(T-T_2)=0"))\
        .di("Abrimos los paréntesis multiplicando cada masa por la diferencia de temperaturas.", M(r"m_1T-m_1T_1+m_2T-m_2T_2=0"))\
        .di("Dejamos a la izquierda lo que tiene te, y la sacamos de factor común.".replace("tiene te", "tiene " + dice("T", "te")),
            M(r"m_1T+m_2T=m_1T_1+m_2T_2"), M(r"T(m_1+m_2)=m_1T_1+m_2T_2"))\
        .di("La suma de las masas pasa a dividir.", M(r"T=\frac{m_1T_1+m_2T_2}{m_1+m_2}", "f", "azul"))
    L.paso("Comprobarlo con números")\
        .di("Sustituimos cada masa y su temperatura inicial en el promedio ponderado que acabamos de despejar.", M(rf"T=\frac{{({m1:g})({T1:g})+({m2:g})({T2:g})}}{{{m1:g}+{m2:g}}}"))\
        .di("Multiplicamos cada pareja en el numerador y sumamos las dos masas en el denominador.", M(rf"T=\frac{{{m1 * T1:g}+{m2 * T2:g}}}{{{m1 + m2:g}}}=\frac{{{tex(m1 * T1 + m2 * T2, 5, sci='no', recorta=True)}}}{{{m1 + m2:g}}}"))\
        .di("Dividimos ambos resultados para confirmar el valor exacto de la temperatura de equilibrio.", R(rf"T={tex(Tf, 3)}\ {C}"))
    revisar_opciones(L, motivos)
    return L.oficial(of, corr).usa(r"\sum Q=0", r"Q=mc\Delta T")


@leccion("Q026")
def q026() -> Leccion:
    return _mezcla_aguas("Q026", 100, 80, 110, 20,
                         {"A": "Entre 50 °C y 80 °C, pero más cerca de 50 °C.", "B": "Entre 50 °C y 80 °C, pero más cerca de 80 °C.", "C": "Entre 20 °C y 50 °C, pero más cerca de 50 °C.", "D": "Entre 20 °C y 50 °C, pero más cerca de 20 °C."}, "C",
                         {"A": ("La A no: hay más agua fría, baja de cincuenta.", "Hay más agua fría."), "B": ("La B tampoco.", "Hay más agua fría."), "D": ("La D no: la diferencia de masas es muy poca.", "Diez gramos de más jalan poquito."), "C": ("La C: un poquito abajo de cincuenta.", "48,6 °C.")}, "III Parcial ordinario II-2025")


@leccion("Q028")
def q028() -> Leccion:
    return _mezcla_aguas("Q028", 100, 40, 109, 60,
                         {"A": "Entre 50 °C y 60 °C, pero más cerca de 50 °C.", "B": "Entre 50 °C y 60 °C, pero más cerca de 60 °C.", "C": "Entre 40 °C y 50 °C, pero más cerca de 50 °C.", "D": "Entre 40 °C y 50 °C, pero más cerca de 40 °C."}, "A",
                         {"B": ("La B no: nueve gramos de más no alcanzan para tanto.", "La diferencia de masas es poca."), "C": ("La C no: hay más agua caliente.", "Hay más agua caliente."), "D": ("La D tampoco.", "Hay más agua caliente."), "A": ("La A: un poquito arriba de cincuenta.", "50,4 °C.")}, "III Parcial extraordinario I-2026")


# ============================================================================ Q027
@leccion("Q027")
def q027() -> Leccion:
    L = _sel("Q027", "Hielo con agua: ¿sirve el promedio?",
             "Cuando se mezclan la misma masa de hielo y agua en un recipiente aislado, ¿se puede afirmar que la temperatura de equilibrio es simplemente el promedio de las temperaturas iniciales?",
             {"A": "Sí, la temperatura de equilibrio es el promedio de las temperaturas iniciales.", "B": "Casi siempre, porque la temperatura es una medida de la energía de los cuerpos.", "C": "Casi nunca, porque la energía que el hielo absorbe del agua produce cambios de estado.",
              "D": "No, la temperatura final es un parámetro que garantiza que la energía absorbida por el hielo sea igual que la cedida por el agua."}, "D",
             claves=["calorimetría", "cambio de fase"],
             visual={"tipo": "calor", "componentes": [{"n": "hielo", "m": 100, "T": 0, "fase": "hielo"}, {"n": "agua", "m": 100, "T": 60, "fase": "caliente"}], "necesita": [{"n": "fundir y calentar el hielo", "Q": 100}], "disponible": [{"n": "enfriar el agua", "Q": 100}]})
    L.paso("Qué preguntan")\
        .di("Aunque las masas son iguales, el hielo y el agua no son lo mismo.", DATO("misma masa"), LISTA(["$c_{\\text{hielo}}\\neq c_{\\text{agua}}$", "El hielo además puede **fundirse**"]))\
        .di("El promedio simple pide mismo material y sin cambio de fase.", OJO("El promedio solo sirve con masas iguales, mismo $c$ y sin cambio de fase."))
    L.paso("Qué determina la temperatura final")\
        .di("En un sistema aislado el calor ganado y el cedido se compensan usando las dos casillas de calorimetría de la hoja.", F(r"Q=mc\,\Delta T"), F(r"Q=\pm mL"), M(r"\sum Q=0\;\Rightarrow\;Q_{\text{absorbe el hielo}}=Q_{\text{cede el agua}}", "c", "azul"))\
        .di("La temperatura final es la que hace cierta esa igualdad.", T("$T_f$ es el valor que cumple esa igualdad."))
    revisar_opciones(L, {
        "A": ("La A no, por lo que vimos.", "Distinto $c$ y cambio de fase."),
        "B": ("La B no: temperatura no es lo mismo que energía.", "Temperatura no es energía total."),
        "C": ("La C suena bien, pero ‘casi nunca’ no explica qué determina la temperatura, y no siempre hay cambio de estado.".replace("‘casi nunca’", "casi nunca"), "No siempre hay cambio de estado; no dice qué fija $T_f$."),
        "D": ("La D es la correcta: la temperatura final sale de igualar energías.", "Sale de igualar energías."),
    })
    return L.oficial("III Parcial extraordinario I-2026", "D").usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ Q029
@leccion("Q029")
def q029() -> Leccion:
    mt, ma, mc, mv = 0.1460, 0.2230, 0.3140, 0.0047
    ca, cc, Lv, dT = 4190.0, 390.0, 2.256e6, 79.0
    qa1, qa2 = ma * ca * dT, mv * Lv
    qa = qa1 + qa2
    qt = mt * cc * dT
    # Lo que se suma es lo escrito (84 418 + 4498): de ahí sale la cuenta del cilindro
    qc_vis = round(qa) + round(qt)
    dTc = qc_vis / (mc * cc)
    Tc = 100 + dTc
    L = _des("Q029", "Cilindro de cobre al rojo en agua",
             "Un tazón de cobre de 146,0 g contiene 223,0 g de agua, a 21,0 °C. Se deja caer un cilindro de cobre muy caliente de 314,0 g; el agua hierve y 4,7 g se convierten en vapor. a) Temperatura de equilibrio. b) Calor transferido al agua. c) Al tazón. d) Temperatura original del cilindro.",
             claves=["calorimetría", "vaporización"], aviso="El enunciado no da las constantes. Se usan las del folleto: agua 4190, cobre 390 J/(kg·°C) y vaporización 2,256×10⁶ J/kg.",
             visual={"tipo": "calor", "Tf": 100, "componentes": [{"n": "agua", "m": 223, "T": 21, "fase": "agua"}, {"n": "tazón", "m": 146, "T": 21, "fase": "solido"}, {"n": "cilindro", "m": 314, "T": round(Tc), "fase": "caliente"}],
                     "necesita": [{"n": "agua de 21 a 100 °C", "Q": qa1}, {"n": "evaporar 4,7 g", "Q": qa2}, {"n": "tazón de 21 a 100 °C", "Q": qt}], "disponible": [{"n": "enfriar el cilindro", "Q": qa + qt}]})
    L.paso("Qué nos dan y qué piden")\
        .di("El cilindro calienta el agua hasta hervirla, y una parte se va como vapor.", DATO("4,7 g"), LISTA(["Tazón Cu: $0{,}1460$ kg a $21{,}0\\ ^\\circ\\text{C}$", "Agua: $0{,}2230$ kg a $21{,}0\\ ^\\circ\\text{C}$", "Cilindro Cu: $0{,}3140$ kg, $T=?$", "Se evaporan $0{,}0047$ kg"]))\
        .di("Todo en kilogramos, para que cuadre con las constantes.", M(r"\Delta T=100-21{,}0=79{,}0\ ^\circ\text{C}", "dt"))
    L.paso("a) Temperatura de equilibrio")\
        .di("Al final hay agua líquida y también vapor: los dos a la vez.", M(r"\text{agua líquida}+\text{vapor juntos al final}"))\
        .di("A una atmósfera, el agua solo hierve a cien grados: líquido y vapor solo pueden convivir a esa temperatura.",
            M(r"\text{líquido y vapor a }1\ \text{atm}\;\Rightarrow\;T=100\ ^\circ\text{C}"), R(r"T_{eq}=100\ ^\circ\text{C}", "ra"), V(foco="flujo"))\
        .por_que("A 1 atm, agua líquida y vapor coexisten únicamente a 100 °C. Es el dato escondido del problema.")
    L.paso("b) Calor que recibió el agua")\
        .di("Partimos de las dos ecuaciones de calorimetría del formulario: cambio de temperatura y cambio de fase.",
            F(r"Q=mc\Delta T"), F(r"Q=\pm mL"),
            M(r"Q_A=m_Ac_A\,\Delta T+m_VL_V", "f", "azul"))\
        .di("Calentarse.", M(rf"(0{{,}}2230)(4190)(79{{,}}0)={_j(qa1)}", "qa1"), V(etapa=0))\
        .di("Evaporar cuatro coma siete gramos.", M(rf"(0{{,}}0047)(2{{,}}256\times10^6)={_j(qa2)}", "qa2"), V(etapa=1))\
        .di("Sumamos los dos calores.", M(rf"Q_A={tex(qa1, 5, sci='no')}+{tex(qa2, 5, sci='no')}={tex(qa, 5, sci='no')}\ \text{{J}}"), R(rf"Q_A={tex(qa, 3, u='J')}", "rb"))
    L.paso("c) Calor que recibió el tazón")\
        .di("El tazón es cobre y también sube setenta y nueve grados.", M(r"Q_T=(0{,}1460)(390)(79{,}0)"), R(rf"Q_T={tex(qt, 3, u='J', sci='no')}", "rc"), V(etapa=2))\
        .di("Es poquito comparado con el agua, pero se cuenta.", NOTA("Olvidar el recipiente es el error más común."))
    L.paso("d) Conservación de la energía")\
        .di("Lo que ganaron el agua y el tazón lo perdió el cilindro.", F(r"\sum Q=0"), M(r"Q_A+Q_T+Q_C=0"), V(foco="comparar"))\
        .di("Despejamos el calor del cilindro: es negativo, porque lo entrega.", M(rf"Q_C=-({tex(qa, 5, sci='no')}+{tex(qt, 4, sci='no')})=-{tex(qc_vis, 5, sci='no')}\ \text{{J}}", "qc", "rojo"))
    L.paso("d) Temperatura original")\
        .di("El cilindro baja de su temperatura original a cien grados.", M(r"Q_C=m_Cc_C\,(100-T_0)"))\
        .di("Despejamos la diferencia de temperatura.", M(rf"T_0-100=\frac{{{tex(qc_vis, 5, sci='no')}}}{{(0{{,}}3140)(390)}}={tex(dTc, 4)}\ {C}"))\
        .di("Sumamos cien.", R(rf"T_0=100+{tex(dTc, 4)}={tex(Tc, dec=1)}\ {C}", "rd"))\
        .di("Más de ochocientos grados: por eso hace hervir el agua.", NOTA("El folleto escribe $-88{,}9$ J; es una errata, son $-88{,}9$ **kJ**."))
    L.responde("a", r"T_{eq}=100\ ^\circ\text{C}", 100.0, "°C").responde("b", rf"Q_A={tex(qa, 3)}\ \text{{J}}", qa, "J").responde("c", rf"Q_T={tex(qt, 3, sci='no')}\ \text{{J}}", qt, "J").responde("d", rf"T_0={tex(Tc, dec=1)}\ {C}", Tc, "°C")
    return L.oficial("folleto, sección 3.2", 100.0, 84.4e3, 4.50e3, 826.1).usa(r"Q=mc\Delta T", r"Q=\pm mL")


# ============================================================================ Q030 / Q033
@leccion("Q030")
def q030() -> Leccion:
    return _cubo_frio("Q030", fuente("Q030"), cagua=4190.0, ccu=390.0, Lv=2.256e6).oficial("folleto, sección 3.2", 100.0, 201.55, 298.45, tol=0.005)


@leccion("Q033")
def q033() -> Leccion:
    return _cubo_frio("Q033", fuente("Q033")).oficial("folleto, sección 3.4", 100.0, 201.5, 298.5, tol=0.005)


# ============================================================================ prueba de los 0 °C (Q031, Q032, Q034)
def _prueba_cero(q: str, titulo: str, enunciado: str, *, hielo, agua=None, vapor=None, cobre=None, ch=2090.0, ca=4190.0, cv=2010.0, Lf=3.34e5, Lv=2.256e6,
                 aviso: str = "", claves=()) -> Leccion:
    """Mezclas que terminan en 0 °C: se compara el calor hasta llegar a 0 °C y luego se funde lo que alcance.

    hielo=(kg, °C), agua=(kg, °C), vapor=(kg, °C), cobre=kg (ya a 0 °C, no participa)
    """
    mh, Th = hielo
    qh = mh * ch * (0 - Th)
    partes = []
    da = 0.0
    if vapor:
        mv, Tv = vapor
        q1 = mv * cv * (Tv - 100) if Tv > 100 else 0.0
        q2 = mv * Lv
        q3 = mv * ca * 100
        da += q1 + q2 + q3
        partes.append(("vapor", mv, Tv, q1, q2, q3))
    if agua:
        ma, Ta = agua
        qa = ma * ca * Ta
        da += qa
        partes.append(("agua", ma, Ta, qa))
    sobra = da - qh
    fund = mh * Lf
    x = sobra / Lf
    maq = x + (vapor[0] if vapor else 0) + (agua[0] if agua else 0)
    queda = mh - x
    comp = [{"n": "hielo", "m": mh * 1000, "T": Th, "fase": "hielo"}]
    if agua:
        comp.append({"n": "agua", "m": agua[0] * 1000, "T": agua[1], "fase": "agua"})
    if vapor:
        comp.append({"n": "vapor", "m": vapor[0] * 1000, "T": vapor[1], "fase": "vapor"})
    if cobre:
        comp.append({"n": "cobre", "m": cobre * 1000, "T": 0, "fase": "solido"})
    nec = ([{"n": "hielo hasta 0 °C", "Q": qh}] if qh > 0 else []) + [{"n": "fundir todo el hielo", "Q": fund}]
    disp = []
    if vapor:
        disp.append({"n": "vapor hasta agua a 0 °C", "Q": sum(partes[0][3:])})
    if agua:
        disp.append({"n": "agua hasta 0 °C", "Q": partes[-1][3]})
    L = _des(q, titulo, enunciado, claves=list(claves) or ["calorimetría", "cambio de fase", "0 °C"], aviso=aviso,
             visual={"tipo": "calor", "Tf": 0, "componentes": comp, "necesita": nec, "disponible": disp, "tDisp": "Calor que se puede dar hasta 0 °C",
                     "final": [{"n": "hielo", "m": round(queda * 1000, 1)}, {"n": "agua", "m": round(maq * 1000, 1)}]})
    datos = [f"Hielo: ${tex(mh * 1000, 4)}$ g a ${tex(Th, 3)}\\ ^\\circ\\text{{C}}$" if Th else f"Hielo: ${tex(mh * 1000, 4)}$ g a $0\\ ^\\circ\\text{{C}}$"]
    if agua:
        datos.append(f"Agua: ${tex(agua[0] * 1000, 4)}$ g a ${tex(agua[1], 4)}\\ ^\\circ\\text{{C}}$")
    if vapor:
        datos.append(f"Vapor: ${tex(vapor[0] * 1000, 3)}$ g a ${tex(vapor[1], 4)}\\ ^\\circ\\text{{C}}$")
    if cobre:
        datos.append(f"Calorímetro de cobre: ${tex(cobre * 1000, 4)}$ g a $0\\ ^\\circ\\text{{C}}$")
    L.paso("Qué nos dan y qué piden")\
        .di("Hay mucho hielo y poco vapor: sospechamos que el hielo no se derrite completo.", LISTA(datos))\
        .di("Los datos del agua que se usan.", M(rf"c_h={tex(ch, 4, sci='no')}\quad c_a={tex(ca, 4, sci='no')}" + (rf"\quad c_v={tex(cv, 4, sci='no')}" if vapor and vapor[1] > 100 else "") + rf"\quad L_f={tex(Lf, 3)}\quad L_v={tex(Lv, 4)}"))
    p = L.paso("La idea: la prueba de los 0 °C")
    p.di("Llevamos todo a cero grados y vemos quién gana: lo que se da o lo que se pide.", V(foco="flujo"), T("Suponemos $T_f=0\\ ^\\circ\\text{C}$ y comparamos **dar** contra **pedir**."))
    if cobre:
        p.di("El calorímetro ya está a cero grados. Si termina en cero, no da ni pide nada.", NOTA("Cobre: $\\Delta T=0$, entonces $Q=0$."))
    p.por_que("Con mucho hielo, casi siempre el final es 0 °C con hielo y agua juntos. Se prueba, y si las cuentas no se contradicen, era cierto.")
    if vapor:
        nom, mv, Tv, q1, q2, q3 = partes[0]
        p = L.paso("Lo que da el vapor")
        if q1 > 0:
            p.di("Primero el vapor se enfría hasta cien grados.", M(rf"q_1=({tex(mv, 3, sci='no')})({tex(cv, 4, sci='no')})({tex(Tv - 100, 3, recorta=True)})={_j(q1, 4)}", "q1"))
        p.di("Se condensa a cien grados." if q1 <= 0 else "Luego se condensa.", M(rf"q_2=({tex(mv, 3, sci='no')})({tex(Lv, 4)})={_j(q2)}", "q2"))
        p.di("Y esa agua baja de cien a cero.", M(rf"q_3=({tex(mv, 3, sci='no')})({tex(ca, 4, sci='no')})(100)={_j(q3, 4)}", "q3"))
        sumandos = "+".join(_j(x, 5) for x in (q1, q2, q3) if x > 0)
        p.di("En total, sumamos lo que da el vapor.", M(rf"Q_{{\text{{vapor}}}}={sumandos}={_j(q1 + q2 + q3)}", "qv", "rojo"))
    if agua:
        p = L.paso("Lo que da el agua")
        p.di("El agua baja de su temperatura hasta cero.", M(rf"Q_{{\text{{agua}}}}=({tex(agua[0], 4, sci='no')})({tex(ca, 4, sci='no')})({tex(agua[1], 4, recorta=True)})={_j(partes[-1][3])}", "qa", "rojo"))
        if vapor:
            p.di("Sumado con el vapor.", M(rf"Q_{{\text{{dan}}}}=Q_{{\text{{vapor}}}}+Q_{{\text{{agua}}}}={_j(q1 + q2 + q3)}+{_j(partes[-1][3])}={_j(da)}", "qd"))
    p = L.paso("Lo que pide el hielo")
    if qh > 0:
        p.di("Llegar a cero grados.", M(rf"Q_h=({tex(mh, 4, sci='no')})({tex(ch, 4, sci='no')})({tex(-Th, 3, recorta=True)})={_j(qh, 4)}", "qh"), V(etapa=0))
        p.di("Y fundirse completo costaría esto.", M(rf"mL_f=({tex(mh, 4, sci='no')})({tex(Lf, 3)})={_j(fund)}", "ff"), V(etapa=1))
        p.di("Calentarlo y fundirlo todo costaría la suma de los dos.", M(rf"Q_h+mL_f={tex(qh, 4, sci='no')}+{_j(fund)}={tex(qh + fund, 5, sci='no')}\ \text{{J}}"))
        p.di("Lo que se da alcanza para calentarlo, pero no para fundirlo todo.", M(rf"{tex(qh, 4, sci='no')}<{tex(da, 5, sci='no')}<{tex(qh + fund, 5, sci='no')}", "cmp", "azul"), V(foco="comparar"))
    else:
        p.di("El hielo ya está a cero: solo falta fundirlo. Fundirlo todo costaría esto.", M(rf"mL_f=({tex(mh, 4, sci='no')})({tex(Lf, 3)})={_j(fund)}", "ff"), V(etapa=0))
        p.di("No alcanza: queda hielo.", M(rf"{tex(da, 5, sci='no')}<{tex(fund, 5, sci='no')}", "cmp", "azul"), V(foco="comparar"))
    p.di("Entonces sí termina en cero grados, con hielo y agua juntos.", R(r"T_f=0\ ^\circ\text{C}", "ra"))
    p = L.paso("Cuánto hielo se funde")
    if qh > 0:
        p.di("Lo que sobra, después de calentar el hielo, se usa en fundir.", M(rf"Q_{{\text{{sobra}}}}={tex(da, 5, sci='no')}-{tex(qh, 4, sci='no')}={_j(sobra)}", "qs"))
    else:
        p.di("Todo lo que se da se usa en fundir.", M(rf"Q_{{\text{{sobra}}}}={_j(sobra)}", "qs"))
    p.di("Masa fundida: calor entre calor latente.", M(rf"m_{{\text{{fund}}}}=\frac{{Q_{{\text{{sobra}}}}}}{{L_f}}=\frac{{{tex(sobra, 5, sci='no')}}}{{{tex(Lf, 3)}}}"), M(rf"m_{{\text{{fund}}}}={tex(x, 3, sci='no')}\ \text{{kg}}={tex(x * 1000, 3)}\ \text{{g}}", "mf", "verde"))
    suma = " + ".join([f"{tex(x * 1000, dec=1)}"] + ([f"{tex(vapor[0] * 1000, dec=1)}"] if vapor else []) + ([f"{tex(agua[0] * 1000, dec=1)}"] if agua else [])).replace(" + ", "+")
    L.paso("Composición final")\
        .di("El hielo que queda.", M(rf"m_{{\text{{hielo}}}}={tex(mh * 1000, dec=1)}-{tex(x * 1000, dec=1)}"), R(rf"m_{{\text{{hielo}}}}={tex(queda * 1000, dec=1)}\ \text{{g}}", "rb"))\
        .di("El agua: lo que se fundió, más lo que ya era o se volvió agua.", M(rf"m_{{\text{{agua}}}}={suma}"), R(rf"m_{{\text{{agua}}}}={tex(maq * 1000, dec=1)}\ \text{{g}}", "rc"), V(foco="final"))\
        .di("Y cero gramos de vapor.", NOTA("Comprobación: la masa total es la misma del inicio."))
    L.responde("a", r"T_f=0\ ^\circ\text{C}").responde("b", rf"\text{{hielo }}{tex(queda * 1000, dec=1)}\ \text{{g}}", queda * 1000, "g").responde("b", rf"\text{{agua }}{tex(maq * 1000, dec=1)}\ \text{{g}}", maq * 1000, "g")
    return L.usa(r"Q=mc\Delta T", r"Q=\pm mL", r"\sum Q=0")


@leccion("Q031")
def q031() -> Leccion:
    L = _prueba_cero("Q031", "200 g de hielo con 10 g de vapor",
                     "Se mezclan 200 g de hielo a −20,0 °C con 10,0 g de vapor de agua a 105 °C. Hallar la temperatura final de equilibrio y la cantidad final de vapor, agua y hielo.",
                     hielo=(0.200, -20.0), vapor=(0.0100, 105.0), aviso="El enunciado no da las constantes; se usan las del folleto.")
    return L.oficial("folleto, sección 3.2", 144.6, 65.4, tol=0.01, nota="El folleto escribe 26 890,5 J por una errata; la suma correcta es 26 850,5 J.")


@leccion("Q032")
def q032() -> Leccion:
    L = _prueba_cero("Q032", "Hielo, agua y vapor a 200 °C",
                     "En un calorímetro se mezclan 30,00 g de hielo a −40,00 °C con 30,00 g de agua a 25,00 °C y 2,00 g de vapor a 200,00 °C. a) Temperatura final. b) Composición final.",
                     hielo=(0.0300, -40.0), agua=(0.0300, 25.0), vapor=(0.00200, 200.0), aviso="El enunciado no da las constantes; se usan las del folleto.")
    return L


@leccion("Q034")
def q034() -> Leccion:
    L = _prueba_cero("Q034", "Calorímetro de cobre con mucho hielo",
                     "Un calorímetro de cobre de 750,0 g está en equilibrio con 250,0 g de hielo a 0,00 °C. Se agregan 68,0 g de agua a 75,00 °C y 17,0 g de vapor a 100,00 °C. a) Temperatura final. b) Composición final.",
                     hielo=(0.2500, 0.0), agua=(0.0680, 75.0), vapor=(0.0170, 100.0), cobre=0.750, ca=4186.0, Lf=3.33e5, Lv=2.26e6)
    return L.oficial("folleto, sección 3.4", 50.0, 285.0, tol=0.025, nota="El folleto redondea a 50 g y 285 g.")


# ============================================================================ Q035
@leccion("Q035")
def q035() -> Leccion:
    mc, mh, mv = 0.322, 0.0420, 0.0120
    ca, cc, Lf, Lv = 4186.0, 387.0, 3.33e5, 2.26e6
    cond, fu = mv * Lv, mh * Lf
    k1 = mv * ca
    k2 = mh * ca + mc * cc
    Tf = (cond + k1 * 100 - fu) / (k1 + k2)
    L = _des("Q035", "Se licua vapor sobre poco hielo",
             "Un calorímetro de cobre de 0,322 kg contiene 0,0420 kg de hielo, todo a 0,0 °C. Dentro se licuan 0,0120 kg de vapor a 100 °C y 1,00 atm. a) Temperatura final. b) Kilogramos de hielo, agua y vapor presentes.",
             claves=["calorimetría", "condensación"], aviso="El enunciado no da las constantes; se usan las que el folleto da en esa misma sección (agua 4186, cobre 387, fusión 3,33×10⁵ y vaporización 22,6×10⁵).",
             visual={"tipo": "calor", "Tf": round(Tf, 1), "componentes": [{"n": "hielo", "m": 42, "T": 0, "fase": "hielo"}, {"n": "cobre", "m": 322, "T": 0, "fase": "solido"}, {"n": "vapor", "m": 12, "T": 100, "fase": "vapor"}],
                     "necesita": [{"n": "fundir el hielo", "Q": fu}, {"n": f"agua y cobre a {Tf:.1f} °C", "Q": k2 * Tf}], "disponible": [{"n": "condensar", "Q": cond}, {"n": "enfriar esa agua", "Q": k1 * (100 - Tf)}], "final": [{"n": "agua", "m": 54}]})
    L.paso("Qué nos dan y qué piden")\
        .di("Poco hielo y bastante vapor. La palabra licua dice que todo el vapor se vuelve líquido.", DATO("licua"), LISTA(["Cobre: $0{,}322$ kg a $0\\ ^\\circ\\text{C}$", "Hielo: $0{,}0420$ kg a $0\\ ^\\circ\\text{C}$", "Vapor: $0{,}0120$ kg a $100\\ ^\\circ\\text{C}$"]))\
        .di("Piden la temperatura final y la composición.", M(r"T_f=\;?"))
    L.paso("¿Se funde todo el hielo?")\
        .di("Fundir todo el hielo cuesta esto.", M(rf"m_hL_f=(0{{,}}0420)(3{{,}}33\times10^5)={_j(fu)}", "fu"), V(etapa=0))\
        .di("Y solo condensar el vapor ya da mucho más.", M(rf"m_vL_v=(0{{,}}0120)(2{{,}}26\times10^6)={_j(cond)}", "co"), V(foco="comparar"))\
        .di("Se funde todo y sobra: la temperatura queda arriba de cero.", M(rf"{tex(cond, 5, sci='no')}>{tex(fu, 5, sci='no')} \;\Rightarrow\; T_f>0", "c", "azul"))
    L.paso("Conservación de la energía")\
        .di("Cede el vapor: se condensa y luego se enfría como agua. Ganan el hielo y el cobre.", M(r"m_vL_v+m_vc_a(100-T)=m_hL_f+m_hc_aT+m_{Cu}c_{Cu}T", "bal", "azul"))\
        .di("Primero los productos de masa por calor específico.", M(rf"m_vc_a=(0{{,}}0120)(4186)={tex(k1, 4)}"), M(rf"m_hc_a=(0{{,}}0420)(4186)={tex(mh * ca, 4)}"), M(rf"m_{{Cu}}c_{{Cu}}=(0{{,}}322)(387)={tex(mc * cc, 4)}"))\
        .di("Con números.", M(rf"{tex(cond, 5, sci='no')}+{tex(k1, 4)}\,(100-T)={tex(fu, 5, sci='no')}+{tex(mh * ca, 4)}\,T+{tex(mc * cc, 4)}\,T"))
    L.paso("a) Despejar la temperatura")\
        .di("Abrimos el paréntesis y juntamos las dos te de la derecha.".replace("dos te", "dos " + dice("T", "te")),
            M(rf"{tex(cond, 5, sci='no')}+{tex(k1 * 100, 4, sci='no')}-{tex(k1, 4)}\,T={tex(fu, 5, sci='no')}+{tex(k2, 4)}\,T"))\
        .di("Las te a un lado sumando y los números al otro restando.".replace("Las te", "Las " + dice("T", "te")),
            M(rf"{tex(cond, 5, sci='no')}+{tex(k1 * 100, 4, sci='no')}-{tex(fu, 5, sci='no')}=({tex(k1, 4)}+{tex(k2, 4)})\,T"), M(rf"{tex(cond + k1 * 100 - fu, 5, sci='no')}={tex(k1 + k2, 4)}\,T"))\
        .di("Dividimos.", R(rf"T_f=\frac{{{tex(cond + k1 * 100 - fu, 5, sci='no')}}}{{{tex(k1 + k2, 4)}}}={tex(Tf, 3)}\ {C}", "ra"))
    L.paso("b) Composición")\
        .di("Cincuenta y tantos grados: no puede haber hielo ni vapor. Todo es agua.", M(r"m_{\text{agua}}=0{,}0420+0{,}0120"), R(r"m_{\text{agua}}=0{,}0540\ \text{kg}\qquad m_{\text{hielo}}=m_{\text{vapor}}=0", "rb"), V(foco="final"))
    L.responde("a", rf"T_f={tex(Tf, 3)}\ {C}", Tf, "°C").responde("b", r"0{,}0540\ \text{kg de agua}", 0.0540, "kg")
    return L.oficial("folleto, sección 3.4", 51.8, 0.054).usa(r"Q=mc\Delta T", r"Q=\pm mL")
