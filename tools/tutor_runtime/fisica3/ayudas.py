"""Piezas reutilizables: constantes del formulario oficial y pasos que se repiten."""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Sequence, Tuple

from .dsl import DATO, F, LISTA, M, MARCA, NOTA, OJO, Leccion, Paso, R, T, TABLA, V
from .formato import dice, n, nu, tex

# ---------------------------------------------------------------- constantes (formulario II-2026)
R_GAS = 8.314
R_LATM = 0.08206
SIGMA = 5.67e-8
KB = 1.38e-23
NA = 6.022e23
ATM = 101325.0
T0 = 273.15

TX_R = r"8{,}314\ \frac{\text{J}}{\text{mol}\cdot\text{K}}"


def cv_de(gas: str) -> float:
    return {"mono": 1.5, "dia": 2.5}[gas] * R_GAS


def gamma_de(gas: str) -> float:
    return {"mono": 5.0 / 3.0, "dia": 1.4}[gas]


NOMBRE_GAS = {"mono": "monoatómico", "dia": "diatómico"}
TX_CV = {"mono": r"\tfrac{3}{2}R", "dia": r"\tfrac{5}{2}R"}
TX_CP = {"mono": r"\tfrac{5}{2}R", "dia": r"\tfrac{7}{2}R"}
VOZ_CV = {"mono": "tres medios de erre", "dia": "cinco medios de erre"}
VOZ_CP = {"mono": "cinco medios de erre", "dia": "siete medios de erre"}
TX_GAMMA = {"mono": r"\tfrac{5}{3}", "dia": r"1{,}40"}
VOZ_GAMMA = {"mono": "cinco tercios", "dia": "uno coma cuatro"}


def qtx(x: float, u: str = "J", cifras: int = 4, **k: Any) -> str:
    return tex(x, cifras, u=u, **k)


class Gas:
    """Estados y procesos de gas ideal con todo en SI (los datos pueden venir en atm/L)."""

    def __init__(self, n_mol: float, gas: str = "dia", cv: Optional[float] = None) -> None:
        self.n = n_mol
        self.gas = gas
        self.cv = cv if cv is not None else cv_de(gas)
        self.cp = self.cv + R_GAS
        self.g = self.cp / self.cv
        self.e: Dict[str, Tuple[float, float, float]] = {}

    def pone(self, nombre: str, p: Optional[float] = None, v: Optional[float] = None, t: Optional[float] = None) -> Tuple[float, float, float]:
        nR = self.n * R_GAS
        if p is None:
            p = nR * t / v
        elif v is None:
            v = nR * t / p
        elif t is None:
            t = p * v / nR
        self.e[nombre] = (p, v, t)
        return self.e[nombre]

    def p(self, a: str) -> float:
        return self.e[a][0]

    def v(self, a: str) -> float:
        return self.e[a][1]

    def t(self, a: str) -> float:
        return self.e[a][2]

    def proceso(self, a: str, b: str, tipo: str) -> Dict[str, float]:
        pa, va, ta = self.e[a]
        pb, vb, tb = self.e[b]
        dU = self.n * self.cv * (tb - ta)
        if tipo == "isotermico":
            w = self.n * R_GAS * ta * math.log(vb / va)
        elif tipo == "isobarico":
            w = pa * (vb - va)
        elif tipo == "isocorico":
            w = 0.0
        elif tipo == "adiabatico":
            w = -dU
        elif tipo == "recta":
            w = (pa + pb) / 2 * (vb - va)
        else:
            raise ValueError(tipo)
        q = 0.0 if tipo == "adiabatico" else dU + w
        return {"Q": q, "W": w, "U": dU}


PROC_NOMBRE = {
    "isotermico": "isotérmico", "isobarico": "isobárico", "isocorico": "isocórico",
    "adiabatico": "adiabático", "recta": "en línea recta",
}
PROC_REGLA = {
    "isotermico": ("la temperatura no cambia", r"\Delta T=0"),
    "isobarico": ("la presión no cambia", r"p=\text{cte}"),
    "isocorico": ("el volumen no cambia", r"V=\text{cte}"),
    "adiabatico": ("no entra ni sale calor", r"Q=0"),
    "recta": ("la presión cambia en línea recta", r"W=\text{área del trapecio}"),
}


def visual_pv(g: Gas, procesos: Sequence[Tuple[str, str, str]], *, uv: str = "m³", up: str = "Pa", fv: float = 1.0, fp: float = 1.0, **extra: Any) -> Dict[str, Any]:
    """Especificación del diagrama p-V. fv/fp convierten de SI a la unidad que se rotula."""
    return {
        "tipo": "pv", "uV": uv, "up": up, "gamma": g.g,
        "estados": {k: [v[1] * fv, v[0] * fp] for k, v in g.e.items()},
        "procesos": [list(p) for p in procesos], **extra,
    }


def tabla_estados(g: Gas, orden: Sequence[str], *, up: str = "Pa", uv: str = "m³", fp: float = 1.0, fv: float = 1.0,
                  cp: int = 4, cv: int = 4, dt: int = 1, resaltar=(), pie: str = "") -> Dict[str, Any]:
    filas = [[k, f"${tex(g.p(k) * fp, cp)}$", f"${tex(g.v(k) * fv, cv)}$", f"${tex(g.t(k), dec=dt)}$"] for k in orden]
    return TABLA(["Estado", f"$p$ ({up})", f"$V$ ({uv})", "$T$ (K)"], filas, resaltar=resaltar, pie=pie)


def signo_voz(x: float) -> str:
    return "positivo" if x > 0 else "negativo"


def explica_proceso(p: Paso, g: Gas, a: str, b: str, tipo: str, *, foco: Optional[str] = None, dec_T: int = 1,
                    datos_pv: bool = False, cifras: int = 4, corto: bool = False) -> Dict[str, float]:
    """Escribe el desarrollo completo de un proceso: ΔU, W y Q, un movimiento por renglón."""
    r = g.proceso(a, b, tipo)
    ab = a + b
    foco = foco or ab
    nT, nV = tex(g.n, 3, recorta=False), None
    Ta, Tb = g.t(a), g.t(b)
    ntx = tex(g.n, 4, recorta=True)
    cvx, cpx = TX_CV.get(g.gas, r"C_V"), TX_CP.get(g.gas, r"C_P")
    cv_num = tex(g.cv, 5)
    cp_num = tex(g.cp, 5)
    if tipo == "isotermico":
        p.di(f"De {a} a {b} la temperatura no cambia, así que la energía interna tampoco.",
             V(foco=foco), M(rf"\Delta U_{{{ab}}} = nC_V\,\Delta T = 0"))
        p.di("Por la primera ley, todo el calor se vuelve trabajo.", F(rf"\Delta U = Q - W \;\Rightarrow\; Q_{{{ab}}} = W_{{{ab}}}"))
        p.di("El trabajo isotérmico es ene erre te por el logaritmo de los volúmenes: final entre inicial.".replace("ene erre te", dice("nRT", "ene erre te")),
             F(rf"W_{{{ab}}} = nRT\ln\left(\frac{{V_{b}}}{{V_{a}}}\right)", f"w{ab}"))
        p.di("Sustituimos.", M(rf"= ({ntx})(8{{,}}314)({tex(Ta, dec=dec_T, recorta=True)})\ln\left(\frac{{{tex(g.v(b), cifras, recorta=True)}}}{{{tex(g.v(a), cifras, recorta=True)}}}\right)"))
        p.di(f"Da {nu(r['W'], 'J', cifras)}, {'positivo porque el gas se expande' if r['W'] > 0 else 'negativo porque el gas se comprime'}.",
             M(rf"W_{{{ab}}} = Q_{{{ab}}} = {qtx(r['W'], cifras=cifras)}", color="verde"))
    elif tipo == "isobarico":
        p.di(f"De {a} a {b} la presión no cambia, entonces el trabajo es el área de un rectángulo.", V(foco=foco),
             F(rf"W_{{{ab}}} = p\,(V_{b}-V_{a})"))
        p.di("Sustituimos en pascales y metros cúbicos.",
             M(rf"= ({tex(g.p(a), cifras)})({tex(g.v(b), cifras, recorta=True)}-{tex(g.v(a), cifras, recorta=True)}) = {qtx(r['W'], cifras=cifras)}"))
        p.di(f"El cambio de energía interna siempre lleva {dice('Cv', 'ce ve')}, aunque no sea a volumen constante.",
             F(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a})"),
             M(rf"= ({ntx})({cv_num})({tex(Tb, dec=dec_T)}-{tex(Ta, dec=dec_T)}) = {qtx(r['U'], cifras=cifras)}"))
        p.di("Y el calor es la suma de los dos.", M(rf"Q_{{{ab}}} = \Delta U_{{{ab}}}+W_{{{ab}}} = {qtx(r['Q'], cifras=cifras)}", color="verde"))
    elif tipo == "isocorico":
        p.di(f"De {a} a {b} el volumen no cambia: el gas no empuja nada, el trabajo es cero.", V(foco=foco), M(rf"W_{{{ab}}} = 0"))
        p.di("Entonces el calor es igual al cambio de energía interna.", F(rf"Q_{{{ab}}} = \Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a})"))
        p.di("Sustituimos.", M(rf"= ({ntx})({cv_num})({tex(Tb, dec=dec_T)}-{tex(Ta, dec=dec_T)})"))
        p.di(f"Sale {signo_voz(r['Q'])}: el gas {'se calienta y recibe calor' if r['Q'] > 0 else 'se enfría y entrega calor'}.",
             M(rf"Q_{{{ab}}} = \Delta U_{{{ab}}} = {qtx(r['Q'], cifras=cifras)}", color="verde"))
    elif tipo == "adiabatico":
        p.di(f"De {a} a {b} es adiabático: no hay intercambio de calor.", V(foco=foco), M(rf"Q_{{{ab}}} = 0"))
        p.di("La primera ley queda así.", F(rf"\Delta U_{{{ab}}} = 0 - W_{{{ab}}} \;\Rightarrow\; W_{{{ab}}} = -\Delta U_{{{ab}}}"))
        p.di("Calculamos el cambio de energía interna.",
             M(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a}) = ({ntx})({cv_num})({tex(Tb, dec=dec_T)}-{tex(Ta, dec=dec_T)}) = {qtx(r['U'], cifras=cifras)}"))
        p.di(f"El trabajo es lo mismo con el signo cambiado: {'el gas se comprime' if r['W'] < 0 else 'el gas se expande a costa de su energía'}.",
             M(rf"W_{{{ab}}} = {qtx(r['W'], cifras=cifras)}", color="verde"))
    elif tipo == "recta":
        p.di(f"De {a} a {b} la presión baja o sube en línea recta: el trabajo es el área del trapecio.", V(foco=foco),
             F(rf"W_{{{ab}}} = \frac{{p_{a}+p_{b}}}{{2}}\,(V_{b}-V_{a})"))
        p.di("Sustituimos.", M(rf"= \frac{{{tex(g.p(a), cifras)}+{tex(g.p(b), cifras)}}}{{2}}\,({tex(g.v(b), cifras, recorta=True)}-{tex(g.v(a), cifras, recorta=True)}) = {qtx(r['W'], cifras=cifras)}"))
        p.di("La energía interna solo mira las temperaturas.", M(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a}) = {qtx(r['U'], cifras=cifras)}"))
        p.di("Y el calor sale de la primera ley.", M(rf"Q_{{{ab}}} = \Delta U_{{{ab}}}+W_{{{ab}}} = {qtx(r['Q'], cifras=cifras)}", color="verde"))
    return r


def tabla_quw(res: Sequence[Tuple[str, Dict[str, float]]], *, cifras: int = 4, u: str = "J", f: float = 1.0) -> Dict[str, Any]:
    filas = [[k, f"${tex(r['Q'] * f, cifras)}$", f"${tex(r['W'] * f, cifras)}$", f"${tex(r['U'] * f, cifras)}$"] for k, r in res]
    tq = sum(r["Q"] for _, r in res)
    tw = sum(r["W"] for _, r in res)
    tu = sum(r["U"] for _, r in res)
    filas.append(["**Ciclo**", f"${tex(tq * f, cifras)}$", f"${tex(tw * f, cifras)}$", "$0$" if abs(tu) < 1e-3 * max(1.0, abs(tq)) else f"${tex(tu * f, cifras)}$"])
    return TABLA(["Proceso", f"$Q$ ({u})", f"$W$ ({u})", rf"$\Delta U$ ({u})"], filas, resaltar=[[len(filas) - 1, None]])


def valores_visual(res: Sequence[Tuple[str, Dict[str, float]]]) -> Dict[str, Dict[str, float]]:
    return {k.replace("→", "").replace(" ", ""): {"Q": r["Q"], "W": r["W"], "U": r["U"]} for k, r in res}
