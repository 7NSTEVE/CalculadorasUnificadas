"""Cálculo explicado en el mismo lugar donde se usa.

La persona que estudia no sabe derivar ni integrar. Las lecciones usan pocas reglas (las de la
lección T-MAT2), pero antes solo las nombraban ("regla tres de la tabla"). Aquí, la primera vez
que una lección usa cada regla, se agrega justo después de ese renglón una explicación hablada y
una nota en la pizarra que dice qué se hizo y por qué, con palabras de alguien que empieza de cero.

No inventa matemática nueva: solo explica el renglón que ya está escrito. Se aplica al construir
cada lección (desglose.aplicar) y no toca la teoría (T-*), que ya enseña las reglas.
"""

from __future__ import annotations

import re
from typing import Dict, List, Optional, Sequence, Tuple

from .dsl import NOTA, Leccion, Paso, Tramo
from .formato import separa

# (clave, patrón sobre el TeX del renglón, voz, nota en la pizarra, frases que indican que ya se explicó en el paso)
Regla = Tuple[str, "re.Pattern[str]", str, str, Tuple[str, ...]]

REGLAS: Sequence[Regla] = (
    ("pendiente",
     re.compile(r"\\frac\{d\s*T\}\{d\s*[xyr]\}"),
     "Qué significa de te entre de equis: cuánto cambia la temperatura por cada metro que avanzamos. Es la pendiente del perfil de temperatura.",
     "$\\frac{dT}{dx}$ se lee *de te entre de equis*: cuánto cambia $T$ por cada metro, justo en ese punto. "
     "Si la temperatura baja al avanzar, es negativa; por eso la ley de Fourier lleva un signo menos y $H$ sale positivo.",
     ("pendiente", "cada metro")),
    ("separar",
     re.compile(r"^\s*H\s*(?:\\,)?\s*d[xyr]\s*=|^\s*\\frac\{H\s*(?:\\,)?\s*d[xyr]\}"),
     "Por qué se puede mover de equis: de equis y de te son pedacitos pequeñísimos de distancia y de temperatura. Se multiplican y dividen como números normales.",
     "*Separar variables*: dejar todo lo que tiene $x$ (con su $dx$) de un lado y todo lo de la temperatura (con su $dT$) del otro. "
     "$dx$ y $dT$ son pedacitos: se pasan multiplicando o dividiendo como cualquier número.",
     ("pedacito",)),
    ("integral_dT",
     re.compile(r"\\int_\{?[^=]*?\}?\^\{?[^=]*?\}?\s*(?:\\,)?\s*dT(?![A-Za-z_])"),
     "Qué es integrar de te: sumar todos los pedacitos de cambio de temperatura desde el inicio hasta el final. La suma de todos los cambios es el cambio total: final menos inicial.",
     "$\\int_{T_1}^{T_2}dT$ = sumar todos los pedacitos $dT$ = cambio total de temperatura = $T_2-T_1$ (final menos inicial).",
     ("pedacitos", "cambio total")),
    ("du_recta",
     re.compile(r"\bd(?:u|R|r|z|w)\s*=\s*-?\s*(?:\d|\\frac|[a-z]\b|\\,|\()[^=]*?\bd[xyrV]\b"),
     "De dónde sale este pedacito: la nueva letra es una recta, un número fijo más la pendiente por la variable. Si la variable crece un pedacito, la nueva letra crece la pendiente por ese pedacito; el número fijo no cambia.",
     "Si $u=a+b\\,x$ (una recta), entonces $du=b\\,dx$: la pendiente $b$ por el pedacito $dx$. El número $a$ no cambia, así que no aporta. "
     "Luego se despeja $dx=\\frac{du}{b}$ para cambiar la integral a la letra $u$.",
     ("pendiente por",)),
    ("ln",
     re.compile(r"\\int[^=]*?\\frac\{(?:[^{}]*\s)?d\s*([a-zA-Z])\}\{\s*\1\s*\}|\\int[^=]*?\\frac\{d([a-zA-Z])\}\{\2\}|\\int\s*\\frac\{du\}\{u\}"),
     "La integral de uno entre u es una regla de la tabla: da el logaritmo natural de u. Se usa como una fórmula, igual que las de la hoja; no hay que deducirla.",
     "Regla de la tabla (lección T-MAT2): $\\int\\frac{du}{u}=\\ln u$. Con límites: $\\int_a^b\\frac{du}{u}=\\ln b-\\ln a=\\ln\\frac{b}{a}$. "
     "El logaritmo natural es la tecla **ln** de la calculadora.",
     ("logaritmo natural de u", "regla dos")),
    ("potencia",
     re.compile(r"u\^\{-[23]\}|\\frac\{d[a-zA-Z]\}\{\([^()]*\)\^\{?[23]\}?\}|\\frac\{d[a-zA-Z]\}\{[a-zA-Z]\^\{?[23]\}?\}|\\int[^=]*?[a-zA-Z]\^\{?-?\d\}?\s*(?:\\,)?\s*d[a-zA-Z]\b|\\int[^=]*?\\alpha\s*V\s*(?:\\,)?\s*dV"),
     "Regla de la potencia: al exponente se le suma uno y se divide entre ese exponente nuevo. Por ejemplo, u a la menos dos pasa a u a la menos uno, dividido entre menos uno.",
     "Regla de la potencia (tabla, lección T-MAT2): $\\int u^{n}\\,du=\\frac{u^{n+1}}{n+1}$. "
     "Ejemplos: $\\int u^{-2}du=\\frac{u^{-1}}{-1}=-\\frac{1}{u}$ · $\\int V\\,dV=\\frac{V^{2}}{2}$.",
     ("se le suma uno", "regla de la potencia")),
    ("evaluar",
     re.compile(r"\\Big\[[^\]]*\\Big\]_|\\Big\|_|\\right\]_\{|\\bigg\[[^\]]*\\bigg\]_"),
     "Cómo se leen los corchetes con dos números: se pone el número de arriba, luego el de abajo, y se restan. Siempre arriba menos abajo.",
     "$\\Big[F\\Big]_a^b=F(b)-F(a)$: se sustituye el límite de **arriba**, se sustituye el de **abajo** y se restan, en ese orden.",
     ("arriba menos abajo",)),
    ("resta_ln",
     re.compile(r"\\ln\s*[^=]*?-\s*\\ln[^=]*=\s*[^=]*\\ln\s*(?:\\left)?\(?\s*\\frac|\\ln\s*[0-9{},\\]+\s*-\s*\\ln\s*[0-9{},\\]+"),
     "Una resta de logaritmos es el logaritmo de la división: logaritmo de a menos logaritmo de b es logaritmo de a entre b.",
     "Propiedad de los logaritmos: $\\ln a-\\ln b=\\ln\\frac{a}{b}$. Así se hace una sola cuenta en la calculadora.",
     ("logaritmo de la división",)),
    ("exponencial",
     re.compile(r"\\int[^=]*?e\^\{"),
     "Regla de la exponencial: e a la equis entre ele, al integrarla, queda igual, multiplicada por ele.",
     "Regla de la tabla: $\\int e^{x/L}\\,dx=L\\,e^{x/L}$ y $\\int e^{-x/L}\\,dx=-L\\,e^{-x/L}$. Recuerde: $e^{0}=1$ y $e^{1}=e\\approx 2{,}718$.",
     ("queda igual",)),
    ("area_pv",
     re.compile(r"W\s*=\s*\\int_?\{?[^=]*?p\s*(?:\\,)?\s*dV"),
     "El trabajo es la integral de pe de ve: el área bajo la curva en el diagrama p-V, entre el volumen inicial y el final.",
     "$W=\\int_{V_1}^{V_2}p\\,dV$ es el **área bajo la curva** del proceso en el diagrama $p$-$V$. "
     "Si el gas se expande el área cuenta positiva; si se comprime, negativa.",
     ("área bajo",)),
)


# Estas reglas se explican donde la integral se calcula, no donde solo se plantea
_SOLO_AL_RESOLVER = {"ln", "potencia", "exponencial", "integral_dT"}


def _resuelve_integral(tex: str) -> bool:
    """Una sola integral y, a su derecha, ya su resultado (no otra integral)."""
    if tex.count(r"\int") != 1:
        return False
    k = tex.index(r"\int")
    return "=" in tex[k:]


def _voz_paso(p: Paso) -> str:
    return " ".join(separa(t.texto)[1] for t in p.tramos).lower()


def _ya_dicho(p: Paso, frases: Sequence[str]) -> bool:
    v = _voz_paso(p)
    return any(f.lower() in v for f in frases)


def _tramos_de(voz: str, nota: str, max_pal: int = 30) -> List[Tramo]:
    """La explicación en uno o dos tramos (cada tramo hablado tiene un máximo de palabras)."""
    voz = " ".join(voz.split())
    if len(voz.split()) <= max_pal:
        return [Tramo(voz, [NOTA(nota)])]
    frases = re.split(r"(?<=[.])\s+", voz)
    partes: List[str] = []
    for f in frases:
        if partes and len((partes[-1] + " " + f).split()) <= max_pal:
            partes[-1] = partes[-1] + " " + f
        else:
            partes.append(f)
    return [Tramo(partes[0], [NOTA(nota)])] + [Tramo(x, []) for x in partes[1:]]


def explicar(lec: Leccion) -> int:
    """Agrega, la primera vez que aparece cada regla de cálculo, su explicación. Devuelve cuántas agregó."""
    if lec.tipo == "teoria" or getattr(lec, "_calculo_explicado", False):
        return 0
    usadas: set = set()
    total = 0
    for p in lec.pasos:
        nuevos: List[Tramo] = []
        for t in p.tramos:
            nuevos.append(t)
            agregar: List[Tramo] = []
            for a in t.acciones:
                if not (isinstance(a, dict) and a.get("a") == "mate" and a.get("tex")):
                    continue
                tex = a["tex"]
                for clave, pat, voz, nota, ya in REGLAS:
                    if clave in usadas or not pat.search(tex):
                        continue
                    if clave in _SOLO_AL_RESOLVER and not _resuelve_integral(tex):
                        continue
                    usadas.add(clave)
                    if _ya_dicho(p, ya):
                        continue
                    agregar.extend(_tramos_de(voz, nota))
            nuevos.extend(agregar)
            total += len(agregar)
        p.tramos = nuevos
    lec._calculo_explicado = True  # type: ignore[attr-defined]
    return total
