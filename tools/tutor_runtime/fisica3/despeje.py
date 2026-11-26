"""Despejes sin saltos: una operación por renglón.

Muchas lecciones pasan de una ecuación a la incógnita despejada en un solo renglón, haciendo dos o
tres operaciones a la vez ("multiplicamos por πr₁r₂ y dividimos entre L"). Este módulo escribe los
renglones intermedios:

    \\frac{H}{\\pi}\\cdot\\frac{L}{r_1r_2}=k\\,(T_1-T_2)
    H\\,L=\\pi\\,r_1\\,r_2\\,k\\,(T_1-T_2)        «Multiplicamos los dos lados por lo que divide a H…»
    H=\\frac{\\pi k\\,r_1r_2\\,(T_1-T_2)}{L}      (el renglón que ya escribió el autor)

Trabaja con productos y cocientes de factores (letras con subíndice, griegas, números, paréntesis,
logaritmos) y con una suma o resta sencilla. Es conservador: solo inserta renglones si, al terminar
el despeje por su cuenta, llega EXACTAMENTE al mismo resultado que escribió el autor (mismos
factores arriba y abajo, mismo signo). Si no lo puede leer o no coincide, no toca nada.
"""

from __future__ import annotations

import re
from collections import Counter
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

from .dsl import Leccion, Paso, Tramo, s as simbolo


class NoSe(Exception):
    """No se puede leer como producto o suma sencilla."""


# ---------------------------------------------------------------- lectura

_ESP = ("\\,", "\\;", "\\!", "\\ ", "\\:", "~", " ")
_GRIEGAS = ("alpha", "beta", "gamma", "delta", "epsilon", "eta", "theta", "kappa", "lambda", "mu", "nu", "pi", "rho",
            "sigma", "tau", "phi", "omega", "Delta", "Sigma", "Omega")


def _cierra_llave(t: str, i: int) -> int:
    """t[i] == '{' -> índice de su '}'."""
    prof = 0
    for k in range(i, len(t)):
        if t[k] == "{":
            prof += 1
        elif t[k] == "}":
            prof -= 1
            if prof == 0:
                return k
    raise NoSe("llave sin cerrar")


def _cierra_par(t: str, i: int) -> Tuple[int, int]:
    """Desde un '(' (o '\\left(') devuelve (inicio del contenido, índice después del cierre)."""
    if t.startswith("\\left(", i):
        ini = i + len("\\left(")
    elif t[i] == "(":
        ini = i + 1
    else:
        raise NoSe("no abre paréntesis")
    prof = 1
    k = ini
    while k < len(t):
        if t.startswith("\\left(", k):
            prof += 1
            k += len("\\left(")
            continue
        if t.startswith("\\right)", k):
            prof -= 1
            if prof == 0:
                return ini, k + len("\\right)")
            k += len("\\right)")
            continue
        c = t[k]
        if c == "(":
            prof += 1
        elif c == ")":
            prof -= 1
            if prof == 0:
                return ini, k + 1
        k += 1
    raise NoSe("paréntesis sin cerrar")


def _sufijos(t: str, k: int) -> int:
    """Subíndice y exponente pegados a un átomo: _1, _{int}, ^2, ^{-1}."""
    while k < len(t) and t[k] in "_^":
        k += 1
        if k < len(t) and t[k] == "{":
            k = _cierra_llave(t, k) + 1
        elif k < len(t) and t[k] == "\\":
            m = re.match(r"\\[A-Za-z]+", t[k:])
            if not m:
                raise NoSe("sufijo raro")
            k += m.end()
        elif k < len(t):
            k += 1
    return k


def normal(a: str) -> str:
    """Forma comparable de un átomo o expresión."""
    x = a
    for e in _ESP:
        x = x.replace(e, "")
    x = x.replace("\\left(", "(").replace("\\right)", ")").replace("\\left[", "[").replace("\\right]", "]")
    x = x.replace("\\dfrac", "\\frac").replace("\\tfrac", "\\frac").replace("−", "-")
    x = re.sub(r"_\{(\w)\}", r"_\1", x)
    x = re.sub(r"\^\{(\w)\}", r"^\1", x)
    return x


def _tiene_suma(t: str) -> bool:
    prof = 0
    k = 0
    while k < len(t):
        if t.startswith("\\left(", k):
            prof += 1
            k += 6
            continue
        if t.startswith("\\right)", k):
            prof -= 1
            k += 7
            continue
        c = t[k]
        if c in "({":
            prof += 1
        elif c in ")}":
            prof -= 1
        elif c in "+-−" and prof == 0 and t[:k].strip() != "":
            return True
        k += 1
    return False


@dataclass
class Prod:
    signo: int = 1
    num: List[str] = field(default_factory=list)
    den: List[str] = field(default_factory=list)

    def clave(self) -> Tuple[int, Tuple[Tuple[str, int], ...], Tuple[Tuple[str, int], ...]]:
        return (self.signo, tuple(sorted(Counter(normal(a) for a in self.num if normal(a) != "1").items())),
                tuple(sorted(Counter(normal(a) for a in self.den if normal(a) != "1").items())))


def leer_prod(t: str) -> Prod:
    t = t.strip()
    for e in _ESP:
        if t.startswith(e):
            t = t[len(e):].strip()
    if not t:
        raise NoSe("vacío")
    if _tiene_suma(t):
        raise NoSe("es suma")
    p = Prod()
    k = 0
    if t[0] in "-−":
        p.signo = -1
        k = 1
    while k < len(t):
        # espacios y signos de producto
        movido = False
        for e in _ESP + ("\\cdot", "\\times"):
            if t.startswith(e, k):
                k += len(e)
                movido = True
                break
        if movido:
            continue
        c = t[k]
        if t.startswith("\\frac", k) or t.startswith("\\dfrac", k) or t.startswith("\\tfrac", k):
            j = t.index("{", k)
            fa = _cierra_llave(t, j)
            arriba = t[j + 1: fa]
            if fa + 1 >= len(t) or t[fa + 1] != "{":
                raise NoSe("frac sin denominador")
            fb = _cierra_llave(t, fa + 1)
            abajo = t[fa + 2: fb]
            k = fb + 1
            if k < len(t) and t[k] in "^_":
                raise NoSe("frac con exponente")
            a = leer_prod(arriba) if not _tiene_suma(arriba) else Prod(1, [f"({arriba})"], [])
            b = leer_prod(abajo) if not _tiene_suma(abajo) else Prod(1, [f"({abajo})"], [])
            p.signo *= a.signo * b.signo
            p.num += a.num + b.den
            p.den += a.den + b.num
            continue
        if t.startswith("\\left(", k) or c == "(":
            ini, fin = _cierra_par(t, k)
            fin = _sufijos(t, fin)
            p.num.append(t[k:fin])
            k = fin
            continue
        if t.startswith("\\ln", k) and not t[k + 3: k + 4].isalpha():
            j = k + 3
            while j < len(t) and t[j] == " ":
                j += 1
            if t.startswith("\\left(", j) or t[j: j + 1] == "(":
                _, fin = _cierra_par(t, j)
            elif t[j: j + 1] == "{":
                fin = _cierra_llave(t, j) + 1
            elif t.startswith("\\frac{", j):
                fa = _cierra_llave(t, j + 5)
                fin = _cierra_llave(t, fa + 1) + 1
            else:
                raise NoSe("ln sin paréntesis")
            p.num.append(t[k:fin])
            k = fin
            continue
        if t.startswith("\\sqrt{", k):
            fin = _cierra_llave(t, k + 5) + 1
            fin = _sufijos(t, fin)
            p.num.append(t[k:fin])
            k = fin
            continue
        if t.startswith("e^{", k):
            fin = _cierra_llave(t, k + 2) + 1
            p.num.append(t[k:fin])
            k = fin
            continue
        if c == "\\":
            m = re.match(r"\\([A-Za-z]+)", t[k:])
            if not m or m.group(1) not in _GRIEGAS:
                raise NoSe(f"comando {m.group(0) if m else c}")
            fin = k + m.end()
            if m.group(1) == "Delta":
                while fin < len(t) and t[fin] == " ":
                    fin += 1
                if fin < len(t) and t[fin].isalpha():
                    fin += 1
                elif t.startswith("\\", fin):
                    m2 = re.match(r"\\([A-Za-z]+)", t[fin:])
                    if not m2 or m2.group(1) not in _GRIEGAS:
                        raise NoSe("delta raro")
                    fin += m2.end()
            fin = _sufijos(t, fin)
            p.num.append(t[k:fin])
            k = fin
            continue
        if c.isalpha():
            fin = _sufijos(t, k + 1)
            # Notación de función: T(x), A(y), r(x), T(L/2)
            mf = re.match(r"\((?:x|y|r|t|z|L/2|h/2|d/2)\)", t[fin:])
            if mf:
                fin += mf.end()
            p.num.append(t[k:fin])
            k = fin
            continue
        if c.isdigit():
            m = re.match(r"\d+(?:\{,\}\d+)?(?:\\times\s*10\^\{-?\d+\})?", t[k:])
            fin = _sufijos(t, k + m.end())
            p.num.append(t[k:fin])
            k = fin
            continue
        raise NoSe(f"no sé leer «{t[k:k + 10]}»")
    if not p.num:
        p.num = ["1"]
    return p


def leer_suma(t: str) -> List[Tuple[int, Prod]]:
    """Términos con su signo. Un solo término también vale."""
    t = t.strip()
    terms: List[Tuple[int, str]] = []
    prof = 0
    ini = 0
    signo = 1
    k = 0
    if t[:1] in "+-−":
        signo = -1 if t[0] in "-−" else 1
        k = ini = 1
    while k < len(t):
        if t.startswith("\\left(", k):
            prof += 1
            k += 6
            continue
        if t.startswith("\\right)", k):
            prof -= 1
            k += 7
            continue
        c = t[k]
        if c in "({":
            prof += 1
        elif c in ")}":
            prof -= 1
        elif c in "+-−" and prof == 0 and t[ini:k].strip():
            # un '-' pegado a un exponente o subíndice no separa términos
            if t[k - 1] in "^_{":
                k += 1
                continue
            terms.append((signo, t[ini:k]))
            signo = -1 if c in "-−" else 1
            ini = k + 1
        k += 1
    terms.append((signo, t[ini:]))
    out = []
    for sg, tt in terms:
        p = leer_prod(tt)
        out.append((sg * p.signo, Prod(1, p.num, p.den)))
    return out


# ---------------------------------------------------------------- escritura

def escribir_prod(p: Prod) -> str:
    num = [a for a in p.num if normal(a) != "1"] or ["1"]
    den = [a for a in p.den if normal(a) != "1"]
    sep = "\\,"
    arr = sep.join(num)
    s = arr if not den else "\\frac{" + arr + "}{" + sep.join(den) + "}"
    return ("-" if p.signo < 0 else "") + s


def escribir_suma(ts: Sequence[Tuple[int, Prod]]) -> str:
    out = ""
    for k, (sg, p) in enumerate(ts):
        t = escribir_prod(Prod(1, p.num, p.den))
        if k == 0:
            out = ("-" if sg < 0 else "") + t
        else:
            out += ("+" if sg > 0 else "-") + t
    return out


def _como_atomo(t: str) -> str:
    return t if not _tiene_suma(t) else f"\\left({t}\\right)"


# ---------------------------------------------------------------- el despeje

def _partir_igualdad(t: str) -> Optional[Tuple[str, str]]:
    prof = 0
    pos = []
    for k, c in enumerate(t):
        if c == "{":
            prof += 1
        elif c == "}":
            prof -= 1
        elif c == "=" and prof == 0:
            pos.append(k)
    if len(pos) != 1:
        return None
    return t[: pos[0]], t[pos[0] + 1:]


def _voz_inc(x: str) -> str:
    if re.fullmatch(r"[A-Za-z]", x):
        return simbolo(x)
    m = re.fullmatch(r"([A-Za-z])_\{?([A-Za-z0-9]{1,4})\}?", x)
    if m:
        return simbolo(f"{m.group(1)}_{m.group(2)}")
    if re.fullmatch(r"[A-Za-z]\((?:x|y|r)\)", x):
        return simbolo(x[0], {"T": "te", "A": "a", "r": "erre", "R": "erre"}.get(x[0], "la incógnita")) + " de " + {"x": "equis", "y": "ye", "r": "erre"}[x[2]]
    return "la incógnita"


Fila = Tuple[str, str]  # (TeX, voz)


def _despeje_producto(lx: Prod, otro: Prod, x: str, otro_tex: str, x_izq: bool) -> Optional[Tuple[List[Fila], Prod]]:
    """x es factor (arriba) de lx. Devuelve (renglones, resultado)."""
    nx = normal(x)
    idx = [i for i, a in enumerate(lx.num) if normal(a) == nx]
    if len(idx) != 1 or any(normal(a) == nx for a in lx.den):
        return None
    otros_num = [a for i, a in enumerate(lx.num) if i != idx[0] and normal(a) != "1"]
    dens = [a for a in lx.den if normal(a) != "1"]
    filas: List[Fila] = []
    vx = _voz_inc(x)
    lado = Prod(lx.signo, list(lx.num), list(lx.den))
    der = Prod(otro.signo, list(otro.num), list(otro.den))

    def fila() -> str:
        a, b = escribir_prod(lado), escribir_prod(der)
        return f"{a}={b}" if x_izq else f"{b}={a}"

    if dens:
        lado = Prod(lado.signo, list(lado.num), [])
        der = Prod(der.signo, list(dens) + list(der.num), list(der.den))
        filas.append((fila(), f"Para despejar {vx}, primero multiplicamos los dos lados por lo que está dividiendo. Así deja de dividir."))
    if otros_num:
        lado = Prod(lado.signo, [x], [])
        der = Prod(der.signo, list(der.num), list(der.den) + otros_num)
        filas.append((fila(), ("Ahora" if filas else "Primero") + f" dividimos los dos lados entre lo que multiplica a {vx}. Así queda sola."))
    if lado.signo < 0:
        lado = Prod(1, [x], [])
        der = Prod(-der.signo, list(der.num), list(der.den))
        filas.append((fila(), "Cambiamos el signo a los dos lados."))
    return filas, der


_SUB = r"(?:_\{(?:[^{}]|\{[^{}]*\})*\}|_[A-Za-z0-9])"
_ES_INCOGNITA = re.compile(
    r"(?:[A-Za-z]|\\(?:alpha|beta|gamma|rho|sigma|eta|lambda))" + _SUB + r"?(?:\((?:x|y|r)\))?"
    r"|\\Delta\s?[A-Za-z]" + _SUB + r"?")


def _primer_tramo(final: str) -> str:
    """'X = F = 3{,}2 = …' -> 'X = F' (el despeje; lo demás es la sustitución)."""
    prof = 0
    pos = []
    for k, c in enumerate(final):
        if c == "{":
            prof += 1
        elif c == "}":
            prof -= 1
        elif c == "=" and prof == 0:
            pos.append(k)
    if len(pos) >= 2:
        return final[: pos[1]]
    return final


def planear(ecuacion: str, final: str) -> Optional[List[Fila]]:
    """Renglones que faltan entre `ecuacion` y `final` (sin incluir ninguno de los dos)."""
    try:
        ig = _partir_igualdad(ecuacion)
        fg = _partir_igualdad(_primer_tramo(final))
        if not ig or not fg:
            return None
        x = fg[0].strip()
        if not _ES_INCOGNITA.fullmatch(x):
            return None
        L, R = ig
        nx = normal(x)
        if nx not in normal(L) and nx not in normal(R):
            return None
        if normal(L).count(nx) + normal(R).count(nx) != 1:
            return None
        x_izq = nx in normal(L)
        lado_x, otro = (L, R) if x_izq else (R, L)
        objetivo_suma = leer_suma(fg[1])
        # Caso 0: la incógnita está dividiendo. Se multiplica por ella y queda arriba, del otro lado.
        if not _tiene_suma(lado_x):
            lx0 = leer_prod(lado_x)
            if any(normal(a) == nx for a in lx0.den) and not any(normal(a) == nx for a in lx0.num):
                ot0 = leer_prod(_como_atomo(otro)) if _tiene_suma(otro) else leer_prod(otro)
                sin_x = Prod(lx0.signo, list(lx0.num), [a for a in lx0.den if normal(a) != nx])
                con_x = Prod(ot0.signo, list(ot0.num) + [x], list(ot0.den))
                fila0 = (f"{escribir_prod(sin_x)}={escribir_prod(con_x)}" if x_izq
                         else f"{escribir_prod(con_x)}={escribir_prod(sin_x)}")
                voz0 = f"{_voz_inc(x)} está dividiendo: multiplicamos los dos lados por ella y pasa arriba, al otro lado."
                if voz0.startswith("la incógnita"):
                    voz0 = "L" + voz0[1:]
                r = _despeje_producto(con_x, sin_x, x, "", not x_izq)
                if r is None:
                    return None
                filas, res = r
                if len(objetivo_suma) != 1:
                    return None
                sg, obj = objetivo_suma[0]
                if res.clave() != Prod(sg, obj.num, obj.den).clave():
                    return None
                todas = [(fila0, voz0)] + filas
                return todas[:-1] if len(todas) >= 2 else None
        # Caso 1: la incógnita es un factor de un producto
        if not _tiene_suma(lado_x):
            lx = leer_prod(lado_x)
            if any(normal(a) == nx for a in lx.num):
                ot = leer_prod(_como_atomo(otro)) if _tiene_suma(otro) else leer_prod(otro)
                r = _despeje_producto(lx, ot, x, otro, x_izq)
                if r is None:
                    return None
                filas, res = r
                if len(objetivo_suma) != 1:
                    return None
                sg, obj = objetivo_suma[0]
                obj = Prod(sg, obj.num, obj.den)
                if res.clave() != obj.clave():
                    return None
                return filas[:-1] if len(filas) >= 2 else None
            # Caso 2: la incógnita está dentro de un paréntesis que multiplica: primero se aísla el paréntesis
            par = [a for a in lx.num if nx in normal(a) and normal(a).startswith("(")]
            if len(par) != 1:
                return None
            interior = par[0]
            ini, fin = _cierra_par(interior, 0)
            dentro = interior[ini: fin - (len("\\right)") if interior.rstrip().endswith("\\right)") else 1)]
            if fin != len(interior):
                return None
            ot = leer_prod(_como_atomo(otro)) if _tiene_suma(otro) else leer_prod(otro)
            r = _despeje_producto(lx, ot, par[0], otro, x_izq)
            if r is None:
                return None
            filas, res = r
            # Ahora: dentro = res  -> despeje aditivo
            filas_ad = _despeje_suma(dentro, res, x, x_izq)
            if filas_ad is None:
                return None
            filas2, terminos = filas_ad
            if not _sumas_iguales(terminos, objetivo_suma):
                return None
            # el renglón de aislar el paréntesis se escribe ya sin paréntesis
            todas = [(f.replace(escribir_prod(Prod(1, [par[0]], [])), dentro, 1) if k == len(filas) - 1 else f, v)
                     for k, (f, v) in enumerate(filas)] + filas2
            return todas[:-1] if len(todas) >= 2 else None
        # Caso 3: el lado de la incógnita es una suma
        ot = leer_prod(_como_atomo(otro)) if _tiene_suma(otro) else leer_prod(otro)
        r = _despeje_suma(lado_x, ot, x, x_izq)
        if r is None:
            return None
        filas, terminos = r
        if not _sumas_iguales(terminos, objetivo_suma):
            return None
        return filas[:-1] if len(filas) >= 2 else None
    except (NoSe, ValueError, IndexError):
        return None


def _sumas_iguales(a: Sequence[Tuple[int, Prod]], b: Sequence[Tuple[int, Prod]]) -> bool:
    ka = sorted((sg * p.signo, p.clave()[1], p.clave()[2]) for sg, p in a)
    kb = sorted((sg * p.signo, p.clave()[1], p.clave()[2]) for sg, p in b)
    return ka == kb


def _despeje_suma(lado_suma: str, otro: Prod, x: str, x_izq: bool) -> Optional[Tuple[List[Fila], List[Tuple[int, Prod]]]]:
    """La incógnita es un término (con signo) de una suma; el otro lado es un producto."""
    ts = leer_suma(lado_suma)
    nx = normal(x)
    pos = [i for i, (sg, p) in enumerate(ts) if len(p.num) == 1 and not p.den and normal(p.num[0]) == nx]
    if len(pos) != 1 or len(ts) > 3:
        return None
    i = pos[0]
    sgx = ts[i][0]
    resto = [(sg, p) for k, (sg, p) in enumerate(ts) if k != i]
    vx = _voz_inc(x)
    filas: List[Fila] = []
    otro_t = (otro.signo if otro.signo else 1, Prod(1, otro.num, otro.den))
    if sgx > 0:
        # A + x = O  ->  x = O - A
        terminos = [otro_t] + [(-sg, p) for sg, p in resto]
        filas.append((f"{escribir_prod(Prod(1, [x], []))}={escribir_suma(terminos)}" if True else "",
                      f"Pasamos lo que suma o resta junto a {vx} al otro lado, con el signo cambiado."))
        return filas, terminos
    # A - x = O  ->  A = O + x  ->  x = A - O
    paso1 = [otro_t, (1, Prod(1, [x], []))]
    izq1 = escribir_suma(resto)
    f1 = f"{izq1}={escribir_suma(paso1)}" if x_izq else f"{escribir_suma(paso1)}={izq1}"
    filas.append((f1, f"{vx.capitalize() if vx == 'la incógnita' else vx} está restando: la pasamos al otro lado sumando, para que quede positiva."))
    terminos = list(resto) + [(-otro_t[0], otro_t[1])]
    filas.append((f"{escribir_prod(Prod(1, [x], []))}={escribir_suma(terminos)}", "Y pasamos lo demás restando al otro lado."))
    return filas, terminos


# ---------------------------------------------------------------- aplicar a una lección

def _es_mate(a: Dict) -> bool:
    return isinstance(a, dict) and a.get("a") == "mate" and bool(a.get("tex"))


def _sin_caja(t: str) -> str:
    t = t.strip()
    if t.startswith("\\boxed{") and t.endswith("}"):
        return t[len("\\boxed{"):-1]
    return t


def _partes_implicacion(t: str) -> List[str]:
    return [x for x in re.split(r"\\;?\\Rightarrow\\;?|\\quad\\Rightarrow\\quad", t)]


def _expandir_paso(p: Paso) -> int:
    nuevos: List[Tramo] = []
    cambios = 0
    tramos = list(p.tramos)
    previo: Optional[str] = None  # último renglón (ecuación) visto
    for t in tramos:
        acc: List[Dict] = []
        voz = t.texto
        hablo_despeje = bool(re.search(r"[Dd]espej", t.texto))
        for a in t.acciones:
            if not _es_mate(a) or a.get("rol") == "formula":
                acc.append(a)
                if _es_mate(a):
                    previo = _partes_implicacion(a["tex"])[-1]
                continue
            tex = a["tex"]
            partes = _partes_implicacion(_sin_caja(tex))
            # (a) dentro del mismo renglón: ecuación ⇒ incógnita = ...
            if len(partes) == 2 and a.get("rol") != "resultado":
                filas = planear(partes[0], partes[1])
                if filas:
                    cambios += 1
                    primera = dict(a)
                    primera["tex"] = partes[0]
                    acc.append(primera)
                    nuevos.append(Tramo(voz, acc))
                    for f, v in filas:
                        nuevos.append(Tramo(v, [{"a": "mate", "tex": f, "id": "", "color": "", "rol": "despeje"}]))
                    nuevos.append(Tramo("Y así queda despejada.", [{"a": "mate", "tex": partes[1], "id": "", "color": a.get("color", ""), "rol": "despeje"}]))
                    voz, acc = "Seguimos.", []
                    previo = partes[1]
                    continue
            # (b) entre renglones: el anterior es la ecuación y este la incógnita despejada
            if previo and hablo_despeje and len(partes) == 1:
                filas = planear(previo, partes[0])
                if filas:
                    cambios += 1
                    if acc:
                        nuevos.append(Tramo(voz, acc))
                        voz, acc = "Seguimos.", []
                    for f, v in filas:
                        nuevos.append(Tramo(v, [{"a": "mate", "tex": f, "id": "", "color": "", "rol": "despeje"}]))
                    acc.append(a)
                    previo = partes[-1]
                    continue
            acc.append(a)
            previo = partes[-1]
        if acc or not t.acciones:
            nuevos.append(Tramo(voz, acc))
    if cambios:
        p.tramos = nuevos
    return cambios


def aplicar(lec: Leccion) -> int:
    if getattr(lec, "_despejada", False):
        return 0
    total = 0
    for p in lec.pasos:
        try:
            total += _expandir_paso(p)
        except (NoSe, ValueError, IndexError, RecursionError):
            continue
    lec._despejada = True  # type: ignore[attr-defined]
    return total
