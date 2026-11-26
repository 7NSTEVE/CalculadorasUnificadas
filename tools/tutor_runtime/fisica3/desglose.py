"""Desglose automático de cuentas: ningún renglón hace varias operaciones de un golpe.

Cuando una lección escribe una cuenta con varias operaciones (por ejemplo
`T=200-\\frac{15\\,396}{235}(1{,}197)`) y salta directo al resultado, este módulo inserta los
renglones intermedios, resolviendo de adentro hacia afuera como se hace con la calculadora:

    =200-(65{,}51)(1{,}197)     «Dividimos 15 396 entre 235: da 65,51.»
    =200-78{,}42                «Multiplicamos 65,51 por 1,197: da 78,42.»

Cada renglón nuevo va en su propio tramo con su frase hablada, así la voz explica cada operación.
Se aplica a todas las lecciones al construirlas (registro.obtener) y a las variantes.

Es conservador: si un renglón tiene letras, unidades a la mitad, varias ecuaciones o el número que
sigue no coincide con la cuenta, no se toca. Los valores se llevan con toda la precisión de la
calculadora y se muestran con cuatro cifras significativas, igual que en el resto del tutor.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Tuple, Union

from .dsl import Leccion, Paso, Tramo
from .formato import n as num_voz
from .formato import separa, tex as num_tex

# ---------------------------------------------------------------- árbol de la cuenta


class NoNumerico(Exception):
    """El trozo no es una cuenta solo con números."""


@dataclass
class Num:
    v: float
    t: str  # cómo se escribe (el original del autor o el calculado)
    pi: bool = False
    calc: bool = False  # resultado intermedio calculado aquí (sus cifras se pueden ajustar)
    fijo: bool = False  # su texto es el que escribió el autor en el renglón siguiente: no se le cambian cifras


@dataclass
class Neg:
    x: "Nodo"


@dataclass
class Suma:
    partes: List[Tuple[int, "Nodo"]]  # (+1 | -1, término)


@dataclass
class Prod:
    fac: List["Nodo"]
    estilo: str  # 'juxt' | 'cdot' | 'times'


@dataclass
class Div:
    a: "Nodo"
    b: "Nodo"
    estilo: str  # 'frac' | 'slash'


@dataclass
class Pot:
    base: "Nodo"
    exp: "Nodo"


@dataclass
class Fun:
    nombre: str  # 'ln' | 'sqrt' | 'exp' | 'abs'
    x: "Nodo"


@dataclass
class Par:
    x: "Nodo"
    abre: str
    cierra: str


Nodo = Union[Num, Neg, Suma, Prod, Div, Pot, Fun, Par]


def _sin_par(x: Nodo) -> Nodo:
    while isinstance(x, Par):
        x = x.x
    return x


def _listo(x: Nodo) -> bool:
    return isinstance(_sin_par(x), Num)


def contar_ops(x: Nodo) -> int:
    x = _sin_par(x)
    if isinstance(x, Num):
        return 0
    if isinstance(x, Neg):
        y = _sin_par(x.x)
        # "-5" es un número escrito; "-(-5)" o "-\pi" todavía piden una operación (el cambio de signo)
        if isinstance(y, Num) and not y.pi and y.v >= 0:
            return 0
        return contar_ops(x.x) + 1
    if isinstance(x, Suma):
        return sum(contar_ops(t) for _, t in x.partes) + len(x.partes) - 1
    if isinstance(x, Prod):
        return sum(contar_ops(f) for f in x.fac) + len(x.fac) - 1
    if isinstance(x, Div):
        return contar_ops(x.a) + contar_ops(x.b) + 1
    if isinstance(x, Pot):
        return contar_ops(x.base) + contar_ops(x.exp) + 1
    if isinstance(x, Fun):
        return contar_ops(x.x) + 1
    return 0


def valor(x: Nodo) -> float:
    x = _sin_par(x)
    if isinstance(x, Num):
        return x.v
    if isinstance(x, Neg):
        return -valor(x.x)
    if isinstance(x, Suma):
        return sum(s * valor(t) for s, t in x.partes)
    if isinstance(x, Prod):
        r = 1.0
        for f in x.fac:
            r *= valor(f)
        return r
    if isinstance(x, Div):
        b = valor(x.b)
        if b == 0:
            raise NoNumerico("división entre cero")
        return valor(x.a) / b
    if isinstance(x, Pot):
        b, e = valor(x.base), valor(x.exp)
        if b < 0 and abs(e - round(e)) > 1e-12:
            raise NoNumerico("potencia no real")
        return b ** e
    if isinstance(x, Fun):
        a = valor(x.x)
        if x.nombre == "ln":
            if a <= 0:
                raise NoNumerico("logaritmo de no positivo")
            return math.log(a)
        if x.nombre == "sqrt":
            if a < 0:
                raise NoNumerico("raíz de negativo")
            return math.sqrt(a)
        if x.nombre == "exp":
            return math.exp(a)
        if x.nombre == "abs":
            return abs(a)
    raise NoNumerico(type(x).__name__)


# ---------------------------------------------------------------- lector de TeX numérico

_ESPACIOS = ("\\,", "\\;", "\\!", "\\ ", "\\:", "~", " ")
_DELIM_PREFIJOS = ("\\left", "\\right", "\\bigl", "\\bigr", "\\Bigl", "\\Bigr", "\\biggl", "\\biggr", "\\big", "\\Big", "\\bigg")


class _Lector:
    def __init__(self, s: str) -> None:
        self.s = s
        self.i = 0

    # -------------------------------------------------------- utilidades
    def _salta(self) -> None:
        while self.i < len(self.s):
            for e in _ESPACIOS:
                if self.s.startswith(e, self.i):
                    self.i += len(e)
                    break
            else:
                return

    def _ve(self, t: str) -> bool:
        self._salta()
        return self.s.startswith(t, self.i)

    def _come(self, t: str) -> bool:
        if self._ve(t):
            self.i += len(t)
            return True
        return False

    def _delim(self) -> Optional[Tuple[str, str]]:
        """Abre un delimitador: devuelve (como se escribió la apertura, cierre esperado)."""
        self._salta()
        j = self.i
        pref = ""
        for p in _DELIM_PREFIJOS:
            if self.s.startswith(p, j) and not self.s[j + len(p): j + len(p) + 1].isalpha():
                pref = p
                break
        k = j + len(pref)
        while k < len(self.s) and self.s[k] == " ":
            k += 1
        for a, c in (("(", ")"), ("[", "]"), ("|", "|")):
            if self.s.startswith(a, k):
                self.i = k + 1
                cierre_pref = pref.replace("left", "right").replace("bigl", "bigr").replace("Bigl", "Bigr").replace("biggl", "biggr")
                return (pref + a, cierre_pref + c)
        return None

    def _cierra(self, cierre: str) -> None:
        self._salta()
        # Acepta el cierre con o sin su prefijo de tamaño
        car = cierre[-1]
        j = self.i
        for p in _DELIM_PREFIJOS:
            if self.s.startswith(p, j) and not self.s[j + len(p): j + len(p) + 1].isalpha():
                j += len(p)
                break
        while j < len(self.s) and self.s[j] == " ":
            j += 1
        if not self.s.startswith(car, j):
            raise NoNumerico("falta cerrar")
        self.i = j + 1

    def _grupo(self) -> Nodo:
        """{ ... } obligatorio."""
        if not self._come("{"):
            raise NoNumerico("falta {")
        x = self.expr()
        if not self._come("}"):
            raise NoNumerico("falta }")
        return x

    # -------------------------------------------------------- gramática
    def todo(self) -> Nodo:
        x = self.expr()
        self._salta()
        if self.i != len(self.s):
            raise NoNumerico(f"sobra: {self.s[self.i:self.i + 20]}")
        return x

    def expr(self) -> Nodo:
        partes: List[Tuple[int, Nodo]] = []
        signo = 1
        if self._come("-") or self._come("−"):
            signo = -1
        elif self._come("+"):
            signo = 1
        t = self.termino()
        partes.append((signo, t))
        while True:
            if self._come("+"):
                partes.append((1, self.termino()))
            elif self._come("-") or self._come("−"):
                partes.append((-1, self.termino()))
            else:
                break
        if len(partes) == 1:
            s, t = partes[0]
            return Neg(t) if s < 0 else t
        if partes[0][0] < 0:
            partes[0] = (1, Neg(partes[0][1]))
        return Suma(partes)

    def _empieza_primario(self) -> bool:
        self._salta()
        c = self.s[self.i: self.i + 1]
        if not c:
            return False
        if c.isdigit() or c in "({[":
            return True
        for p in ("\\frac", "\\dfrac", "\\tfrac", "\\sqrt", "\\ln", "\\pi", "\\left(", "\\left[", "\\Big(", "\\big(", "\\Big[", "\\big[", "\\left|"):
            if self.s.startswith(p, self.i):
                return True
        if self.s.startswith("e^", self.i):
            return True
        return False

    def termino(self) -> Nodo:
        fac: List[Nodo] = [self.potencia()]
        estilo = "juxt"
        divs: List[Tuple[str, Nodo]] = []
        while True:
            if self._come("\\cdot"):
                fac.append(self.potencia())
                estilo = "cdot"
            elif self._ve("\\times") and not self._es_cientifica():
                self._come("\\times")
                fac.append(self.potencia())
                estilo = "times"
            elif self._come("/"):
                divs.append(("slash", self.potencia()))
            elif self._empieza_primario():
                fac.append(self.potencia())
            else:
                break
        x: Nodo = fac[0] if len(fac) == 1 else Prod(fac, estilo)
        for est, d in divs:
            x = Div(x, d, est)
        return x

    def _es_cientifica(self) -> bool:
        j = self.i
        m = re.match(r"\\times\s*10\s*\^", self.s[j:])
        return bool(m)

    def potencia(self) -> Nodo:
        b = self.primario()
        while self._ve("^"):
            self._come("^")
            self._salta()
            if self._ve("{"):
                if self.s.startswith("{\\circ", self.i) or self.s.startswith("\\circ", self.i):
                    raise NoNumerico("grados")
                e = self._grupo()
            elif self.i < len(self.s) and self.s[self.i].isdigit():
                e = Num(float(self.s[self.i]), self.s[self.i])
                self.i += 1
            else:
                raise NoNumerico("exponente raro")
            b = Pot(b, e)
        return b

    def _numero(self) -> Optional[Num]:
        self._salta()
        m = re.match(r"\d+", self.s[self.i:])
        if not m:
            return None
        j = self.i + m.end()
        ent = m.group(0)
        txt = ent
        # Miles separados con espacio fino: 41\,125 (grupos de tres cifras, sin decimales pegados raros)
        while True:
            mm = re.match(r"(\\,|\\;| )(\d{3})(?![\d])", self.s[j:])
            if not mm:
                break
            ent += mm.group(2)
            txt += "\\," + mm.group(2)
            j += mm.end()
        dec = ""
        md = re.match(r"\{,\}(\d+)|,(\d+)|\.(\d+)", self.s[j:])
        if md:
            dec = md.group(1) or md.group(2) or md.group(3)
            txt += "{,}" + dec
            j += md.end()
        v = float(ent + ("." + dec if dec else ""))
        # Notación científica pegada: \times 10^{-5}
        ms = re.match(r"\s*(?:\\,|\\;|\s)*\\times\s*10\s*\^\s*(?:\{\s*([+\-−]?\s*\d+)\s*\}|([+\-]?\d))", self.s[j:])
        if ms:
            e = int((ms.group(1) or ms.group(2)).replace("−", "-").replace(" ", ""))
            v *= 10.0 ** e
            txt += f"\\times 10^{{{e}}}"
            j += ms.end()
        self.i = j
        return Num(v, txt)

    def primario(self) -> Nodo:
        self._salta()
        if self.i >= len(self.s):
            raise NoNumerico("fin inesperado")
        n0 = self._numero()
        if n0 is not None:
            return n0
        if self._come("\\pi"):
            return Num(math.pi, "\\pi", pi=True)
        for f in ("\\frac", "\\dfrac", "\\tfrac"):
            if self.s.startswith(f, self.i) and not self.s[self.i + len(f): self.i + len(f) + 1].isalpha():
                self.i += len(f)
                a = self._grupo()
                b = self._grupo()
                return Div(a, b, "frac")
        if self.s.startswith("\\sqrt", self.i):
            self.i += len("\\sqrt")
            if self._ve("["):
                raise NoNumerico("raíz n-ésima")
            return Fun("sqrt", self._grupo())
        if self.s.startswith("\\ln", self.i) and not self.s[self.i + 3: self.i + 4].isalpha():
            self.i += 3
            self._salta()
            d = self._delim()
            if d is not None:
                x = self.expr()
                self._cierra(d[1])
                return Fun("ln", x)
            if self._ve("{"):
                return Fun("ln", self._grupo())
            return Fun("ln", self.potencia_simple())
        if self.s.startswith("e^", self.i):
            self.i += 2
            self._salta()
            if self._ve("{"):
                return Fun("exp", self._grupo())
            if self.i < len(self.s) and self.s[self.i].isdigit():
                d0 = self.s[self.i]
                self.i += 1
                return Fun("exp", Num(float(d0), d0))
            raise NoNumerico("exponente de e")
        if self._ve("{"):
            return self._grupo()
        d = self._delim()
        if d is not None:
            if d[0].endswith("|"):
                x = self.expr()
                self._cierra(d[1])
                return Fun("abs", x)
            x = self.expr()
            self._cierra(d[1])
            return Par(x, d[0], d[1])
        raise NoNumerico(f"no es número: {self.s[self.i:self.i + 12]}")

    def potencia_simple(self) -> Nodo:
        """Argumento sin paréntesis de \\ln: un número o una fracción."""
        self._salta()
        n0 = self._numero()
        if n0 is not None:
            return n0
        for f in ("\\frac", "\\dfrac", "\\tfrac"):
            if self.s.startswith(f, self.i):
                return self.primario()
        if self.s.startswith("\\pi", self.i):
            return self.primario()
        raise NoNumerico("argumento de ln")


# ---------------------------------------------------------------- unidades al final

_INICIO_UNIDAD = re.compile(r"\\text\s*\{|\\mathrm\s*\{|\^\s*\{?\s*\\circ|\\%|%|\\frac\s*\{\s*\\text|°")


def separar_unidad(seg: str) -> Tuple[str, str]:
    """'2{,}671\\ \\text{m}^{-1}' -> ('2{,}671', '\\ \\text{m}^{-1}')."""
    prof = 0
    for k in range(len(seg)):
        c = seg[k]
        if c == "{":
            prof += 1
        elif c == "}":
            prof -= 1
        if prof == 0 and _INICIO_UNIDAD.match(seg, k):
            j = k
            # retrocede sobre espacios de TeX que separan la unidad
            while True:
                movido = False
                for e in ("\\ ", "\\,", "\\;", " ", "~"):
                    if seg[:j].endswith(e):
                        j -= len(e)
                        movido = True
                        break
                if not movido:
                    break
            resto = seg[j:]
            # La unidad no puede llevar cuentas: solo letras, exponentes y /, ·, paréntesis
            sin_exp = re.sub(r"\^\s*\{[^{}]*\}|\^\s*-?\d", "", resto)
            if re.search(r"\d", re.sub(r"\\text\s*\{[^{}]*\}", "", sin_exp)):
                return seg, ""
            if re.search(r"[=+]", resto):
                return seg, ""
            return seg[:j], resto
    return seg, ""


_COMANDOS_OK = re.compile(
    r"\\(?:dfrac|tfrac|frac|sqrt|ln|pi|cdot|times|left|right|biggl|biggr|Biggl|Biggr|bigg|Bigg|bigl|bigr|Bigl|Bigr|big|Big)(?![A-Za-z])")


def leer(seg: str) -> Tuple[Nodo, str]:
    """Lee un trozo numérico de TeX. Devuelve (árbol, unidad). Lanza NoNumerico."""
    cuerpo, u = separar_unidad(seg.strip())
    cuerpo = cuerpo.strip()
    limpio = _COMANDOS_OK.sub("", cuerpo).replace("e^", "")
    if not cuerpo or re.search(r"[A-Za-z]", limpio):
        raise NoNumerico("tiene letras")
    return _Lector(cuerpo).todo(), u


# ---------------------------------------------------------------- escritura

def _fmt(v: float) -> str:
    """Resultado calculado: cuatro cifras significativas, sin ceros de relleno; notación científica solo si es muy grande o muy pequeño."""
    a = abs(v)
    if a == 0:
        return "0"
    if 1e-4 <= a < 1e5:
        return num_tex(v, 4, sci="no", recorta=True)
    return num_tex(v, 4, sci="si", recorta=True)


def _atomo(x: Nodo) -> bool:
    x = _sin_par(x)
    return isinstance(x, Num) and x.v >= 0 and "times" not in x.t


def _envolver(t: str) -> str:
    return f"({t})"


def escribir(x: Nodo, ctx: str = "") -> str:
    if isinstance(x, Par):
        interior = _sin_par(x)
        if isinstance(interior, Num):
            return escribir(interior, ctx)
        return f"{x.abre}{escribir(x.x)}{x.cierra}"
    if isinstance(x, Num):
        t = x.t
        if ctx in ("juxt", "pot") and not x.pi:
            return _envolver(t)
        if ctx in ("suma2", "cdot") and x.v < 0:
            return _envolver(t)
        return t
    if isinstance(x, Neg):
        inner = _sin_par(x.x)
        t = escribir(x.x)
        if isinstance(inner, (Suma, Neg)) or (isinstance(inner, Num) and inner.v < 0):
            t = _envolver(t)
        s = "-" + t
        return _envolver(s) if ctx in ("juxt", "suma2", "cdot", "pot") else s
    if isinstance(x, Suma):
        out = ""
        for k, (sg, t) in enumerate(x.partes):
            tt = escribir(t, "suma2" if k else "suma1")
            if k == 0:
                out = ("-" if sg < 0 else "") + tt
            else:
                out += ("+" if sg > 0 else "-") + tt
        return _envolver(out) if ctx in ("juxt", "pot", "cdot", "slash") else out
    if isinstance(x, Prod):
        if x.estilo == "juxt":
            partes = []
            for k, f in enumerate(x.fac):
                fi = _sin_par(f)
                if isinstance(fi, Num):
                    partes.append(fi.t if fi.pi else _envolver(fi.t))
                elif isinstance(fi, (Suma, Neg)):
                    partes.append(_envolver(escribir(fi)))
                else:
                    partes.append(escribir(f, "juxtf"))
            s = "".join(partes)
        else:
            sep = r"\cdot " if x.estilo == "cdot" else r"\times "
            s = sep.join(escribir(f, "cdot") for f in x.fac)
        return _envolver(s) if ctx == "pot" else s
    if isinstance(x, Div):
        if x.estilo == "frac":
            s = f"\\frac{{{escribir(x.a)}}}{{{escribir(x.b)}}}"
            return s
        s = f"{escribir(x.a, 'slash')}/{escribir(x.b, 'slash')}"
        return _envolver(s) if ctx in ("juxt", "pot") else s
    if isinstance(x, Pot):
        base = escribir(x.base, "pot")
        return f"{base}^{{{escribir(x.exp)}}}"
    if isinstance(x, Fun):
        a = escribir(x.x)
        if x.nombre == "ln":
            return f"\\ln\\left({a}\\right)"
        if x.nombre == "sqrt":
            return f"\\sqrt{{{a}}}"
        if x.nombre == "exp":
            return f"e^{{{a}}}"
        if x.nombre == "abs":
            return f"\\left|{a}\\right|"
    raise NoNumerico(type(x).__name__)


# ---------------------------------------------------------------- un paso de cuenta

# (tipo, datos, resultado). datos: lista de (signo, Num) con el número tal como está escrito
Op = Tuple[str, List[Tuple[int, "Num"]], "Num"]


def _hecho(v: float) -> Num:
    return Num(v, _fmt(v), calc=True)


def _decimales_txt(t: str) -> Optional[int]:
    m = re.fullmatch(r"-?[\d\\,]+(?:\{,\}(\d+))?", t)
    return len(m.group(1) or "") if m else None


def _suma_exacta(x: "Suma", v: float) -> Optional[Num]:
    """Una suma o resta de números que escribió el autor se muestra exacta, con los decimales del que más
    tenga (49,096 − 20,0 = 29,096, no 29,1): así la cuenta siguiente, hecha con calculadora sobre lo que se
    ve, da justo lo que dice la pizarra."""
    ds = []
    for _, t in x.partes:
        n = _sin_par(t)
        if not isinstance(n, Num) or n.pi or n.calc:
            return None
        d = _decimales_txt(n.t)
        if d is None:
            return None
        ds.append(d)
    d = max(ds) if ds else 0
    if d > 6:
        return None
    return Num(v, num_tex(v, dec=d), calc=False)


def _fmt_cifras(v: float, cifras: int) -> str:
    a = abs(v)
    if a == 0:
        return "0"
    if 1e-4 <= a < 1e5:
        return num_tex(v, cifras, sci="no", recorta=True)
    return num_tex(v, cifras, sci="si", recorta=True)


def _a_float(t: str) -> float:
    m = re.fullmatch(r"(-?)([\d\\,]+)(?:\{,\}(\d+))?(?:\\times 10\^\{(-?\d+)\})?", t)
    if not m:
        raise ValueError(t)
    ent = m.group(2).replace("\\,", "")
    v = float(ent + ("." + m.group(3) if m.group(3) else ""))
    if m.group(4):
        v *= 10.0 ** int(m.group(4))
    return -v if m.group(1) else v


def _hijos(x: Nodo) -> List[Nodo]:
    if isinstance(x, (Neg, Fun, Par)):
        return [x.x]
    if isinstance(x, Suma):
        return [t for _, t in x.partes]
    if isinstance(x, Prod):
        return list(x.fac)
    if isinstance(x, Div):
        return [x.a, x.b]
    if isinstance(x, Pot):
        return [x.base, x.exp]
    return []


def _cifras_de(nodo: Optional[Nodo]) -> int:
    """Cifras significativas con que el autor escribió un número (mínimo 4, como el resto del tutor)."""
    n = _sin_par(nodo) if nodo is not None else None
    if not isinstance(n, Num):
        return 4
    t = re.sub(r"\\times\s*10\^\{?-?\d+\}?", "", n.t)
    dig = re.sub(r"[^\d]", "", t).lstrip("0")
    return min(7, max(4, len(dig)))


def _afinar(x: Nodo, cifras_vista: int = 4) -> None:
    """Da más cifras a un resultado intermedio cuando la cuenta siguiente, hecha con lo que se ve escrito,
    no daría lo mismo (logaritmos de números cercanos a 1, restas de números parecidos)."""
    for h in _hijos(x):
        _afinar(h, cifras_vista)
    if isinstance(x, (Num, Par)):
        return
    try:
        verdad = valor(x)
    except (NoNumerico, ValueError, OverflowError, ZeroDivisionError):
        return
    for h in _hijos(x):
        n = _sin_par(h)
        if not (isinstance(n, Num) and n.calc):
            continue
        real = n.v
        for cifras in range(4, 10):
            n.t = _fmt_cifras(real, cifras)
            try:
                n.v = _a_float(n.t)
                visto = valor(x)
            except (NoNumerico, ValueError, OverflowError, ZeroDivisionError):
                visto = verdad
            finally:
                n.v = real
            # Basta cuando la cuenta hecha con lo que se ve da lo mismo a las cifras que se muestran
            # (antes se aceptaba un 0,02 % de error, y eso a veces cambiaba la última cifra visible)
            if _fmt_cifras(visto, cifras_vista) == _fmt_cifras(verdad, cifras_vista) or abs(visto - verdad) <= 1e-9 * max(abs(verdad), 1e-300):
                break


def _ids_nodos(x: Nodo, acc: set) -> None:
    acc.add(id(x))
    for h in _hijos(x):
        _ids_nodos(h, acc)


def _copiar_textos(e: Nodo, a: Nodo, vistos: set) -> None:
    """Recorre a la par el estado calculado y el renglón que escribió el autor: cada número calculado nuevo toma
    el texto del autor, para que la voz diga exactamente lo que se ve escrito."""
    e1, a1 = _sin_par(e), _sin_par(a)
    if isinstance(e1, Num) and isinstance(a1, Neg) and isinstance(_sin_par(a1.x), Num):
        # "-1{,}57" escrito por el autor se lee como menos un número
        dentro = _sin_par(a1.x)
        if e1.calc and id(e1) not in vistos and not dentro.pi and _parecido(e1.v, -_a_float_seguro(dentro), 0.004):
            e1.t = "-" + dentro.t.lstrip("-")
            e1.fijo = True
        return
    if isinstance(e1, Num) and isinstance(a1, Num):
        if e1.calc and id(e1) not in vistos and not a1.pi and _parecido(e1.v, _a_float_seguro(a1), 0.004):
            e1.t = a1.t
            e1.fijo = True
        return
    if type(e1) is not type(a1):
        return
    he, ha = _hijos(e1), _hijos(a1)
    if len(he) != len(ha):
        return
    for x, y in zip(he, ha):
        _copiar_textos(x, y, vistos)


def _a_float_seguro(n: "Num") -> float:
    try:
        return _a_float(n.t)
    except ValueError:
        return n.v


def _visto(n: "Num") -> Optional[float]:
    """El valor del número tal como se lee escrito (lo que alguien teclearía en la calculadora)."""
    if n.pi:
        return math.pi
    try:
        return _a_float(n.t)
    except ValueError:
        return None


def _media_ulp(t: str) -> float:
    m = re.search(r"\\times\s*10\^\{?(-?\d+)\}?", t)
    exp = int(m.group(1)) if m else 0
    base = t[: m.start()] if m else t
    dec = re.search(r"\{,\}(\d+)", base)
    return 0.5 * 10 ** (exp - (len(dec.group(1)) if dec else 0))


def _cifras_txt(t: str) -> int:
    t = re.sub(r"\\times\s*10\^\{?-?\d+\}?", "", t)
    dig = re.sub(r"[^\d]", "", t).lstrip("0")
    return max(1, len(dig))


def _cuenta_op(tipo: str, vals: List[Tuple[int, float]]) -> float:
    if tipo == "div":
        return vals[0][1] / vals[1][1]
    if tipo == "prod":
        p = 1.0
        for _, v in vals:
            p *= v
        return p
    if tipo == "suma":
        return sum(s * v for s, v in vals)
    if tipo == "neg":
        return -vals[0][1]
    if tipo == "pot":
        return vals[0][1] ** vals[1][1]
    if tipo == "ln":
        return math.log(vals[0][1])
    if tipo == "sqrt":
        return math.sqrt(vals[0][1])
    if tipo == "exp":
        return math.exp(vals[0][1])
    if tipo == "abs":
        return abs(vals[0][1])
    raise ValueError(tipo)


def _cuadrar(ops: List["Op"]) -> None:
    """Cada operación que se dicta, hecha con calculadora sobre los números tal como se ven, tiene que dar el
    resultado que se ve a sus cifras. Si no da, los datos calculados (no los del autor) ganan una cifra; se
    repasa de atrás hacia adelante porque un dato con más cifras pide más cifras a la cuenta que lo produjo."""
    for _ in range(5):
        cambio = False
        for tipo, datos, r in reversed(ops):
            rv = _visto(r)
            if rv is None:
                continue
            antes = [(x, x.t) for _, x in datos]
            cuadra = False
            for _intento in range(6):
                vals = [(s, _visto(x)) for s, x in datos]
                if any(v is None for _, v in vals):
                    break
                try:
                    c = _cuenta_op(tipo, vals)  # type: ignore[arg-type]
                except (ValueError, ZeroDivisionError, OverflowError):
                    break
                if abs(c - rv) <= _media_ulp(r.t) * (1 + 1e-7) + 1e-12 * abs(rv):
                    cuadra = True
                    break
                subio = False
                for _, x in datos:
                    if x.calc and not x.pi and not x.fijo and _cifras_txt(x.t) < 9:
                        # sin recortar ceros: si la cifra nueva es un 0, igual cuenta (1,5230 y luego 1,52295)
                        k = _cifras_txt(x.t) + 1
                        a = abs(x.v)
                        x.t = num_tex(x.v, k, sci="no" if (a == 0 or 1e-4 <= a < 1e5) else "si")
                        subio = True
                if not subio:
                    break
                cambio = True
            if not cuadra:
                # No se arregla con más cifras (el número del autor no sale de la cuenta): se deja como estaba
                for x, t in antes:
                    x.t = t
        if not cambio:
            break


def _como_num(x: Nodo) -> Num:
    y = _sin_par(x)
    if isinstance(y, Num):
        return y
    return _hecho(valor(x))


def reducir(x: Nodo) -> Tuple[Nodo, List[Op]]:
    """Resuelve, a la vez, todas las operaciones cuyos datos ya son números."""
    if isinstance(x, Par):
        y, ops = reducir(x.x)
        return Par(y, x.abre, x.cierra), ops
    if isinstance(x, Num):
        return x, []
    if isinstance(x, Neg):
        if _listo(x.x):
            a = valor(x.x)
            if isinstance(_sin_par(x.x), Num) and not _sin_par(x.x).pi and a >= 0:
                # "-5" es un número, no una operación pendiente
                return Num(-a, "-" + _sin_par(x.x).t), []
            r = _hecho(-a)
            return r, [("neg", [(1, _como_num(x.x))], r)]
        y, ops = reducir(x.x)
        return Neg(y), ops
    if isinstance(x, Suma):
        if all(_listo(t) for _, t in x.partes):
            v = sum(s * valor(t) for s, t in x.partes)
            r = _suma_exacta(x, v) or _hecho(v)
            return r, [("suma", [(s, _como_num(t)) for s, t in x.partes], r)]
        ops: List[Op] = []
        nuevas = []
        for s, t in x.partes:
            if _listo(t):
                nuevas.append((s, t))
            else:
                y, o = reducir(t)
                nuevas.append((s, y))
                ops += o
        return Suma(nuevas), ops
    if isinstance(x, Prod):
        if all(_listo(f) for f in x.fac):
            prod = 1.0
            for f in x.fac:
                prod *= valor(f)
            r = _hecho(prod)
            return r, [("prod", [(1, _como_num(f)) for f in x.fac], r)]
        ops = []
        nuevos = []
        for f in x.fac:
            if _listo(f):
                nuevos.append(f)
            else:
                y, o = reducir(f)
                nuevos.append(y)
                ops += o
        return Prod(nuevos, x.estilo), ops
    if isinstance(x, Div):
        if _listo(x.a) and _listo(x.b):
            r = _hecho(valor(x.a) / valor(x.b))
            return r, [("div", [(1, _como_num(x.a)), (1, _como_num(x.b))], r)]
        ops = []
        a2, b2 = x.a, x.b
        if not _listo(x.a):
            a2, o = reducir(x.a)
            ops += o
        if not _listo(x.b):
            b2, o = reducir(x.b)
            ops += o
        return Div(a2, b2, x.estilo), ops
    if isinstance(x, Pot):
        if _listo(x.base) and _listo(x.exp):
            r = _hecho(valor(x))
            return r, [("pot", [(1, _como_num(x.base)), (1, _como_num(x.exp))], r)]
        ops = []
        b2, e2 = x.base, x.exp
        if not _listo(x.base):
            b2, o = reducir(x.base)
            ops += o
        if not _listo(x.exp):
            e2, o = reducir(x.exp)
            ops += o
        return Pot(b2, e2), ops
    if isinstance(x, Fun):
        if _listo(x.x):
            r = _hecho(valor(x))
            return r, [(x.nombre, [(1, _como_num(x.x))], r)]
        y, ops = reducir(x.x)
        return Fun(x.nombre, y), ops
    raise NoNumerico(type(x).__name__)


def cadena(x: Nodo) -> List[Tuple[Nodo, List[Op]]]:
    """Estados sucesivos hasta llegar a un número: [(estado, operaciones que llevaron a él)]."""
    out = []
    cur = x
    for _ in range(40):
        if _listo(cur):
            break
        nuevo, ops = reducir(cur)
        if not ops:
            break
        out.append((nuevo, ops))
        cur = nuevo
    return out


# ---------------------------------------------------------------- voz de cada operación

def _nv(x: "Num") -> str:
    """El número dicho tal como está escrito en la pizarra."""
    if x.pi:
        return "pi"
    t = x.t
    m = re.fullmatch(r"(-?)(\d+(?:\\,\d{3})*)(?:\{,\}(\d+))?(?:\\times 10\^\{(-?\d+)\})?", t)
    if not m:
        return num_voz(x.v, 4)
    dec = len(m.group(3) or "")
    if m.group(4) is not None:
        return num_voz(x.v, dec + 1, sci="si")
    return num_voz(x.v, dec=dec)


_NUM_PALABRA = ["", "", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho"]


def _frase(op: Op) -> str:
    tipo, d, r = op
    nums = [x for _, x in d]
    if tipo == "div":
        return f"Dividimos {_nv(nums[0])} entre {_nv(nums[1])}: da {_nv(r)}."
    if tipo == "prod":
        if len(nums) <= 5:
            return "Multiplicamos " + " por ".join(_nv(x) for x in nums) + f": da {_nv(r)}."
        return f"Multiplicamos los {_NUM_PALABRA[min(len(nums), 8)]} factores: da {_nv(r)}."
    if tipo == "suma":
        signos = [sg for sg, _ in d]
        if len(d) == 2 and signos[1] < 0:
            if nums[1].v == 0:
                return "Restar cero no cambia nada."
            return f"Restamos {_nv(nums[0])} menos {_nv(nums[1])}: da {_nv(r)}."
        if all(sg > 0 for sg in signos):
            if len(d) == 2:
                return f"Sumamos {_nv(nums[0])} más {_nv(nums[1])}: da {_nv(r)}."
            return f"Sumamos todo: da {_nv(r)}."
        return f"Sumamos y restamos con cuidado de los signos: da {_nv(r)}."
    if tipo == "neg":
        return "Menos por menos da más." if nums[0].v < 0 else f"Le ponemos el signo menos: {_nv(r)}."
    if tipo == "pot":
        b, e = nums
        ent = int(round(e.v)) if abs(e.v - round(e.v)) < 1e-12 else None
        nom = {2: "al cuadrado", 3: "al cubo", 4: "a la cuarta"}.get(ent) if ent is not None else None
        if nom:
            return f"{_nv(b)} {nom} da {_nv(r)}."
        return f"{_nv(b)} elevado a {_nv(e)}, con la tecla de potencia: da {_nv(r)}."
    if tipo == "ln":
        return f"Con la tecla ln de la calculadora, logaritmo natural de {_nv(nums[0])}: da {_nv(r)}."
    if tipo == "sqrt":
        return f"Raíz cuadrada de {_nv(nums[0])}: da {_nv(r)}."
    if tipo == "exp":
        return f"e elevado a {_nv(nums[0])} da {_nv(r)}."
    if tipo == "abs":
        return f"El valor absoluto quita el signo: {_nv(r)}."
    return "Hacemos la cuenta."


def _palabras_visibles(txt: str) -> int:
    vis, _, _ = separa(txt)
    return len(vis.split())


def frase_paso(ops: List[Op]) -> str:
    frases = [_frase(o) for o in ops]
    if len(frases) == 1:
        return frases[0]
    if len(frases) == 2:
        t = "Primero las de adentro. " + frases[0] + " " + frases[1]
        if _palabras_visibles(t) <= 30:
            return t
    return "Resolvemos a la vez las operaciones de adentro, cada una con la calculadora."


# ---------------------------------------------------------------- partir un renglón

_SEPARADORES = re.compile(r"\\approx|≈|=")


def _trozos_nivel0(tex: str) -> List[Tuple[str, str]]:
    """Parte en '=' / '\\approx' de nivel 0. Devuelve [(operador_previo, trozo)]."""
    out: List[Tuple[str, str]] = []
    prof = 0
    ini = 0
    op = ""
    k = 0
    while k < len(tex):
        c = tex[k]
        if c == "{":
            prof += 1
        elif c == "}":
            prof -= 1
        if prof == 0:
            m = _SEPARADORES.match(tex, k)
            if m and not (c == "=" and k > 0 and tex[k - 1] in "<>!\\"):
                out.append((op, tex[ini:k]))
                op = m.group(0)
                k = m.end()
                ini = k
                continue
        k += 1
    out.append((op, tex[ini:]))
    return out


def _quitar_boxed(tex: str) -> Optional[str]:
    t = tex.strip()
    if t.startswith("\\boxed{") and t.endswith("}"):
        cuerpo = t[len("\\boxed{"):-1]
        # comprobar que la llave del boxed cierra al final
        prof = 0
        for c in cuerpo:
            if c == "{":
                prof += 1
            elif c == "}":
                prof -= 1
                if prof < 0:
                    return None
        return cuerpo if prof == 0 else None
    return None


def _num_de(seg: str) -> Optional[Tuple[Nodo, str]]:
    try:
        return leer(seg)
    except (NoNumerico, ValueError, OverflowError, ZeroDivisionError, RecursionError, IndexError):
        return None


def _parecido(a: float, b: float, rel: float = 0.004) -> bool:
    return abs(a - b) <= max(rel * max(abs(a), abs(b)), 1e-9)


@dataclass
class Plan:
    cabeza: str                    # renglón original recortado (termina en la cuenta inicial)
    filas: List[Tuple[str, str]]   # (tex del renglón nuevo, voz)
    final_caja: Optional[str] = None  # si el original era un \boxed con el resultado


def planear(tex: str, siguiente: Optional[str]) -> Optional[Plan]:
    """Decide qué renglones intermedios faltan entre una cuenta y su resultado."""
    if not tex or "\\begin" in tex or "\\quad" in tex or "\\text{y}" in tex or "\\;\\text" in tex:
        return None
    caja = _quitar_boxed(tex)
    cuerpo = caja if caja is not None else tex
    # Solo la última ecuación de un renglón con implicación
    pre = ""
    if "\\Rightarrow" in cuerpo:
        k = cuerpo.rfind("\\Rightarrow")
        pre = cuerpo[: k + len("\\Rightarrow")]
        cuerpo = cuerpo[k + len("\\Rightarrow"):]
        if caja is not None:
            return None
    trozos = _trozos_nivel0(cuerpo)
    if len(trozos) < 2 and not cuerpo.lstrip().startswith("="):
        # Un renglón que es solo una cuenta (sin '=') no se toca: no sabemos qué es
        return None
    # índice del primer trozo numérico a partir del cual todos son numéricos
    leidos = [(_num_de(t) if t.strip() else None) for _, t in trozos]
    i0 = None
    for i in range(len(trozos)):
        if all(leidos[j] is not None for j in range(i, len(trozos))):
            i0 = i
            break
    if i0 is None:
        return None
    if i0 == 0 and trozos[0][1].strip():
        # Toda la fila es numérica sin nombre a la izquierda ("a·b = c"): se trata igual
        pass
    base, unidad_base = leidos[i0]
    if contar_ops(base) < 2:
        return None
    try:
        estados = cadena(base)
    except (NoNumerico, ValueError, OverflowError, ZeroDivisionError):
        return None
    if len(estados) < 2:
        return None
    # Valores ya escritos por el autor después de la cuenta, en el mismo renglón
    dados = [(trozos[j][0], trozos[j][1], leidos[j]) for j in range(i0 + 1, len(trozos))]
    try:
        final = valor(estados[-1][0])
    except (NoNumerico, ValueError, OverflowError, ZeroDivisionError):
        return None
    # ¿Qué estado corresponde al primer valor dado (o al renglón siguiente)?
    objetivo = None
    objetivo_txt = None
    primero = None
    cif_obj = 4  # cifras con que se ve el número al que hay que llegar
    if dados:
        op, txt, (nodo, _u) = dados[0]
        try:
            v = valor(nodo)
        except NoNumerico:
            return None
        objetivo = (contar_ops(nodo), v)
        objetivo_txt = (op, txt)
        cif_obj = _cifras_de(dados[-1][2][0])
        if len(dados) > 1:
            # Si el autor ya escribió más de un paso, solo rellenamos el primer hueco
            pass
    elif siguiente:
        # El renglón siguiente: su PRIMER trozo numérico es el paso que el autor ya escribió
        sig = _quitar_boxed(siguiente) or siguiente
        tz = _trozos_nivel0(sig)
        leidos_sig = [(_num_de(t) if t.strip() else None) for _, t in tz]
        candidatos = [x for x in leidos_sig if x is not None]
        primero = candidatos[0] if candidatos else None
        ultimo = candidatos[-1] if candidatos else None
        objetivo_alt = None
        if primero is not None:
            cif_obj = _cifras_de((ultimo or primero)[0])
            try:
                objetivo = (contar_ops(primero[0]), valor(primero[0]))
                if ultimo is not None and ultimo is not primero:
                    objetivo_alt = (contar_ops(ultimo[0]), valor(ultimo[0]))
            except NoNumerico:
                objetivo = None
    # Dónde parar
    corte = len(estados)  # se insertan estados[0:corte]
    if objetivo is not None:
        ops_obj, v_obj = objetivo
        hallado = None
        for k, (e, _) in enumerate(estados):
            if contar_ops(e) == ops_obj and _parecido(valor(e), v_obj):
                hallado = k
                break
        if hallado is None:
            if dados:
                # El autor siguió otro orden, o escribió un valor que no sale de la cuenta: no se toca.
                # Solo se anota cuando el número final de verdad no coincide (para revisarlo a mano).
                if ops_obj == 0 and not _parecido(final, v_obj, 0.006):
                    DISCREPANCIAS.append((tex, f"la cuenta da {final:.6g}, el renglón dice {v_obj:.6g}"))
                return None
            if ops_obj > 0:
                return None  # el autor sigue con su propio camino de pasos: no se toca
            if _parecido(final, v_obj, 0.006):
                hallado = len(estados) - 1
            elif _parecido(final * 1e-3, v_obj, 0.006) or _parecido(final * 1e3, v_obj, 0.006) or _parecido(final * 100, v_obj, 0.006):
                hallado = len(estados)  # el renglón siguiente lo pasa a kW, kJ, % ...: se escribe el número completo
            else:
                hallado = len(estados)  # el renglón siguiente es otra cosa: se termina la cuenta aquí
        corte = hallado
    if corte <= 0:
        return None
    # Cifras suficientes en cada resultado intermedio (se ajusta antes de escribir y de dictar)
    for e, _ in estados:
        _afinar(e, cif_obj)
    # El renglón del autor al que se llega (en el mismo renglón o en el siguiente)
    autor = None
    if corte < len(estados):
        if dados:
            autor = dados[0][2][0]
        elif primero is not None:
            autor = primero[0]
    if autor is not None:
        # Lo que se dice al llegar al renglón del autor se dice con sus mismas cifras ("da 20,785", no "da 20,79").
        # Los números que ya salieron en renglones anteriores no se tocan: allí se ven con sus cifras.
        vistos: set = set()
        for e, _ in estados[:corte]:
            _ids_nodos(e, vistos)
        _copiar_textos(estados[corte][0], autor, vistos)
        _cuadrar([op for _, os_ in estados[: corte + 1] for op in os_])
    else:
        _cuadrar([op for _, os_ in estados[:corte] for op in os_])
    voz_final = frase_paso(estados[corte][1]) if dados and corte < len(estados) else "Y así llegamos al resultado."
    filas: List[Tuple[str, str]] = []
    for e, ops in estados[:corte]:
        filas.append(("=" + escribir(e), frase_paso(ops)))
    if not dados and corte == len(estados):
        # La cuenta termina aquí: el último renglón es el número; lleva la unidad si el autor la puso
        if unidad_base and filas:
            t, v = filas[-1]
            filas[-1] = (t + unidad_base, v)
    # Cabeza: el renglón hasta la cuenta inicial (sin los valores dados)
    cab_trozos = trozos[: i0 + 1]
    cabeza = ""
    for k, (op, t) in enumerate(cab_trozos):
        cabeza += (op if k else "") + t
    cabeza = pre + cabeza
    final_caja = None
    if dados:
        # El valor que dio el autor se conserva tal cual (con su unidad) como último renglón
        resto = "".join(o + t for o, t, _ in dados)
        if caja is not None:
            nombre = "".join((o if k else "") + t for k, (o, t) in enumerate(trozos[:1])) if i0 > 0 else ""
            if nombre.strip():
                final_caja = "\\boxed{" + nombre + resto + "}"
            else:
                primero_op = dados[0][0]
                final_caja = "\\boxed{" + resto[len(primero_op):] + "}"
        else:
            filas.append((resto, voz_final))
    elif caja is not None:
        return None
    return Plan(cabeza=cabeza, filas=filas, final_caja=final_caja)


# Renglones donde la cuenta escrita no da el número que el autor puso al lado (para revisar a mano)
DISCREPANCIAS: List[Tuple[str, str]] = []


# ---------------------------------------------------------------- aplicar a una lección

MAX_PAL_PASO = 86  # un poco por debajo del límite de la revisión (90)


def _palabras_habladas(tramos: Sequence[Tramo]) -> int:
    return sum(len(separa(t.texto)[1].split()) for t in tramos)


def _es_mate(a: Dict) -> bool:
    return isinstance(a, dict) and a.get("a") == "mate" and bool(a.get("tex"))


def _siguiente_tex(tramos: Sequence[Tramo], ti: int, ai: int) -> Optional[str]:
    t = tramos[ti]
    for a in t.acciones[ai + 1:]:
        if _es_mate(a):
            return a["tex"]
    for t2 in tramos[ti + 1:]:
        for a in t2.acciones:
            if _es_mate(a):
                return a["tex"]
    return None


def _expandir_paso(p: Paso) -> int:
    nuevos: List[Tramo] = []
    cambios = 0
    originales = list(p.tramos)
    for ti, t in enumerate(originales):
        voz_actual = t.texto
        acc_actual: List[Dict] = []
        for ai, a in enumerate(t.acciones):
            if not _es_mate(a) or a.get("rol") == "formula":
                acc_actual.append(a)
                continue
            es_caja = a.get("rol") == "resultado"
            plan = planear(a["tex"], _siguiente_tex(originales, ti, ai))
            if plan is None or (es_caja and plan.final_caja is None):
                acc_actual.append(a)
                continue
            cambios += 1
            if es_caja:
                cab = {"a": "mate", "tex": plan.cabeza, "id": "", "color": "", "rol": "cuenta"}
                acc_actual.append(cab)
            else:
                cab = dict(a)
                cab["tex"] = plan.cabeza
                acc_actual.append(cab)
            nuevos.append(Tramo(voz_actual, acc_actual))
            for fila, voz in plan.filas:
                nuevos.append(Tramo(voz or "Así queda.", [{"a": "mate", "tex": fila, "id": "", "color": "", "rol": "cuenta"}]))
            if es_caja:
                final = dict(a)
                final["tex"] = plan.final_caja
                nuevos.append(Tramo("Este es el resultado.", [final]))
            voz_actual = "Así queda."
            acc_actual = []
        if acc_actual:
            nuevos.append(Tramo(voz_actual, acc_actual))
        elif voz_actual is t.texto and not t.acciones:
            nuevos.append(t)
    if cambios:
        p.tramos = nuevos
    return cambios


def _inicia_con_resultado(t: Tramo) -> bool:
    for a in t.acciones:
        if _es_mate(a):
            return a.get("rol") == "resultado"
    return False


def _partir_paso(p: Paso) -> List[Paso]:
    if _palabras_habladas(p.tramos) <= MAX_PAL_PASO or len(p.tramos) < 2:
        return [p]
    grupos: List[List[Tramo]] = [[]]
    cuenta = 0
    for t in p.tramos:
        w = len(separa(t.texto)[1].split())
        if grupos[-1] and cuenta + w > MAX_PAL_PASO:
            # no empezar una parte con un resultado suelto: se lleva el tramo anterior
            if _inicia_con_resultado(t) and len(grupos[-1]) > 1:
                previo = grupos[-1].pop()
                grupos.append([previo])
                cuenta = len(separa(previo.texto)[1].split())
            else:
                grupos.append([])
                cuenta = 0
        grupos[-1].append(t)
        cuenta += w
    grupos = [g for g in grupos if g]
    if len(grupos) == 1:
        return [p]
    partes: List[Paso] = []
    for k, g in enumerate(grupos):
        titulo = p.titulo if k == 0 else f"{p.titulo} (sigue)"
        q = Paso(titulo)
        q.tramos = g
        if k == 0:
            q.porque = p.porque
            q.origen = list(p.origen)
        if k == len(grupos) - 1:
            q.lento = list(p.lento)
            q.pregunta = p.pregunta
        partes.append(q)
    return partes


def aplicar(lec: Leccion) -> int:
    """Inserta los renglones intermedios que falten y parte los pasos largos. Devuelve cuántas cuentas desglosó."""
    if getattr(lec, "_desglosada", False):
        return 0
    # Primero los despejes (una operación por renglón), luego las cuentas con números
    from . import despeje
    despeje.aplicar(lec)
    total = 0
    # Varias pasadas: si el autor dejó una cuenta a medio hacer después de su último valor,
    # la segunda pasada la termina (cada renglón nuevo ya es de una sola operación).
    for _ in range(3):
        hechos = 0
        for p in lec.pasos:
            try:
                hechos += _expandir_paso(p)
            except (NoNumerico, ValueError, OverflowError, ZeroDivisionError, RecursionError):
                continue
        total += hechos
        if not hechos:
            break
    # Las reglas de derivar e integrar se explican donde se usan (la persona no sabe cálculo)
    from . import calculo
    calculo.explicar(lec)
    nuevos: List[Paso] = []
    for p in lec.pasos:
        nuevos.extend(_partir_paso(p))
    lec.pasos = nuevos
    lec._desglosada = True  # type: ignore[attr-defined]
    return total
