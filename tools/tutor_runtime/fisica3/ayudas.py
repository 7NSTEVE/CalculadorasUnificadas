"""Piezas reutilizables: constantes del formulario oficial y pasos que se repiten."""

from __future__ import annotations

import math
import re
from typing import Any, Dict, List, Optional, Sequence, Tuple

from .dsl import DATO, F, LISTA, M, MARCA, NOTA, OJO, OPCION, Leccion, Paso, R, T, TABLA, V
from .formato import _redondea, dice, n, nu, tex

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


def _num_tex(t: str) -> float:
    """Lee un número escrito en TeX por tex(): '1{,}013\\times 10^{5}', '20{,}785', '12\\,000'."""
    m = re.search(r"\\times\s*10\^\{?(-?\d+)\}?", t)
    base = t[: m.start()] if m else t
    base = base.replace("\\,", "").replace("{,}", ".").replace("{", "").replace("}", "").strip()
    return float(base) * (10.0 ** int(m.group(1)) if m else 1.0)


def _media_ulp_tex(t: str) -> float:
    """Media unidad de la última cifra con que se escribe un número (12{,}35 → 0,005; 9176 → 0,5)."""
    m = re.search(r"\\times\s*10\^\{?(-?\d+)\}?", t)
    exp = int(m.group(1)) if m else 0
    base = t[: m.start()] if m else t
    dec = re.search(r"\{,\}(\d+)", base)
    return 0.5 * 10.0 ** (exp - (len(dec.group(1)) if dec else 0))


def _par_T(Ta: float, Tb: float, dec_T: int, factor: Optional[float] = None, cifras: int = 4) -> Tuple[str, str]:
    """Temperaturas para sustituir en (T_b - T_a): con los decimales justos para que la resta no pierda exactitud.

    Con `factor` (lo que multiplica a la resta, tal como se escribe) se exige además que la cuenta hecha con
    calculadora sobre lo escrito dé el resultado que se muestra, a sus cifras.
    """
    dT = Tb - Ta
    d = dec_T
    while d < 4:
        dTv = _rd(Tb, d) - _rd(Ta, d)
        ok = abs(dTv - dT) <= 5e-4 * max(abs(dT), 1e-9)
        if ok and factor is not None:
            res = factor * dT
            ok = abs(factor * dTv - res) <= _media_ulp_tex(tex(res, cifras))
        if ok:
            break
        d += 1
    return tex(Ta, dec=d), tex(Tb, dec=d)


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
    # Las temperaturas llevan los decimales necesarios para que n·C_V·(T_b − T_a), con lo escrito, dé lo que se muestra
    sTa, sTb = _par_T(Ta, Tb, dec_T, factor=_num_tex(ntx) * _num_tex(cv_num), cifras=cifras)
    if tipo == "isotermico":
        p.di(f"De {a} a {b} la temperatura no cambia, así que la energía interna tampoco.",
             V(foco=foco), M(rf"\Delta U_{{{ab}}} = nC_V\,\Delta T = 0"))
        p.di("Por la primera ley, todo el calor se vuelve trabajo.", F(rf"\Delta U = Q - W \;\Rightarrow\; Q_{{{ab}}} = W_{{{ab}}}"))
        p.di("El trabajo isotérmico es ene erre te por el logaritmo de los volúmenes: final entre inicial.".replace("ene erre te", dice("nRT", "ene erre te")),
             F(rf"W_{{{ab}}} = nRT\ln\left(\frac{{V_{b}}}{{V_{a}}}\right)", f"w{ab}"))
        p.di("Sustituimos.", M(rf"= ({ntx})(8{{,}}314)({tex(Ta, dec=dec_T, recorta=True)})\ln\left(\frac{{{tex(g.v(b), cifras, recorta=True)}}}{{{tex(g.v(a), cifras, recorta=True)}}}\right)"))
        nrt = g.n * R_GAS * Ta
        lnv = math.log(g.v(b) / g.v(a))
        # Cifras justas para que el producto, hecho con lo escrito, dé el trabajo que se muestra
        snrt, sln = _datos_justos([(nrt, "c"), (lnv, "c")], lambda x, y: x * y, lambda x: qtx(x, cifras=cifras))
        p.di("Multiplicamos adelante y sacamos el logaritmo.",
             M(rf"= ({snrt})\,({sln})"))
        p.di(f"Da {nu(r['W'], 'J', cifras)}, {'positivo porque el gas se expande' if r['W'] > 0 else 'negativo porque el gas se comprime'}.",
             M(rf"W_{{{ab}}} = Q_{{{ab}}} = {qtx(r['W'], cifras=cifras)}", color="verde"))
    elif tipo == "isobarico":
        p.di(f"De {a} a {b} la presión no cambia, entonces el trabajo es el área de un rectángulo.", V(foco=foco),
             F(rf"W_{{{ab}}} = p\,(V_{b}-V_{a})"))
        sp, svb, sva = _datos_justos([(g.p(a), "c"), (g.v(b), "cr"), (g.v(a), "cr")], lambda pp, vb, va: pp * (vb - va),
                                     lambda x: qtx(x, cifras=cifras))
        p.di("Sustituimos en pascales y metros cúbicos.",
             M(rf"= ({sp})({svb}-{sva}) = {qtx(r['W'], cifras=cifras)}"))
        p.di(f"El cambio de energía interna siempre lleva {dice('Cv', 'ce ve')}, aunque no sea a volumen constante.",
             F(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a})"),
             M(rf"= ({ntx})({cv_num})({sTb}-{sTa}) = {qtx(r['U'], cifras=cifras)}"))
        p.di("Y el calor sale de la primera ley: la suma de los dos.", F(r"\Delta U = Q - W \;\Rightarrow\; Q = \Delta U + W"),
             M(rf"Q_{{{ab}}} = " + _suma_tex([r["U"], r["W"]], cifras), color="verde"))
    elif tipo == "isocorico":
        p.di(f"De {a} a {b} el volumen no cambia: el gas no empuja nada, el trabajo es cero.", V(foco=foco), M(rf"W_{{{ab}}} = 0"))
        p.di("Entonces el calor es igual al cambio de energía interna.", F(rf"Q_{{{ab}}} = \Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a})"))
        p.di("Sustituimos.", M(rf"= ({ntx})({cv_num})({sTb}-{sTa})"))
        p.di(f"Sale {signo_voz(r['Q'])}: el gas {'se calienta y recibe calor' if r['Q'] > 0 else 'se enfría y entrega calor'}.",
             M(rf"Q_{{{ab}}} = \Delta U_{{{ab}}} = {qtx(r['Q'], cifras=cifras)}", color="verde"))
    elif tipo == "adiabatico":
        p.di(f"De {a} a {b} es adiabático: no hay intercambio de calor.", V(foco=foco), M(rf"Q_{{{ab}}} = 0"))
        p.di("Partimos de la primera ley del formulario con calor cero.", F(r"\Delta U = Q - W"), F(rf"\Delta U_{{{ab}}} = 0 - W_{{{ab}}} \;\Rightarrow\; W_{{{ab}}} = -\Delta U_{{{ab}}}"))
        p.di("Calculamos el cambio de energía interna.",
             M(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a}) = ({ntx})({cv_num})({sTb}-{sTa}) = {qtx(r['U'], cifras=cifras)}"))
        p.di(f"El trabajo es lo mismo con el signo cambiado: {'el gas se comprime' if r['W'] < 0 else 'el gas se expande a costa de su energía'}.",
             M(rf"W_{{{ab}}} = {qtx(r['W'], cifras=cifras)}", color="verde"))
    elif tipo == "recta":
        p.di(f"De {a} a {b} la presión cambia en línea recta. La integral del trabajo de la hoja es el área bajo la línea en el diagrama.", V(foco=foco),
             F(r"W = \int_{V_1}^{V_2} p\,dV"))
        p.di("Bajo una recta esa área es un trapecio: el promedio de las dos alturas por la base. No hace falta integrar.",
             M(rf"W_{{{ab}}} = \frac{{p_{a}+p_{b}}}{{2}}\,(V_{b}-V_{a})"))
        spa, spb, svb, sva = _datos_justos([(g.p(a), "c"), (g.p(b), "c"), (g.v(b), "cr"), (g.v(a), "cr")],
                                           lambda pa, pb, vb, va: (pa + pb) / 2 * (vb - va), lambda x: qtx(x, cifras=cifras))
        p.di("Sustituimos.", M(rf"= \frac{{{spa}+{spb}}}{{2}}\,({svb}-{sva}) = {qtx(r['W'], cifras=cifras)}"))
        p.di("La energía interna solo mira las temperaturas.", F(rf"\Delta U_{{{ab}}} = nC_V\,(T_{b}-T_{a})"),
             M(rf"= ({ntx})({cv_num})({sTb}-{sTa}) = {qtx(r['U'], cifras=cifras)}"))
        p.di("Y el calor sale de la primera ley.", F(r"\Delta U = Q - W \;\Rightarrow\; Q = \Delta U + W"),
             M(rf"Q_{{{ab}}} = " + _suma_tex([r["U"], r["W"]], cifras), color="verde"))
    return r


def tabla_quw(res: Sequence[Tuple[str, Dict[str, float]]], *, cifras: int = 4, u: str = "J", f: float = 1.0,
              d: Optional[int] = None) -> Dict[str, Any]:
    """Tabla Q, W, ΔU de cada proceso y del ciclo.

    Con d (decimales fijos) cada casilla se escribe ya redondeada y la fila del ciclo es la suma exacta de lo
    escrito: así coincide con las sumas que se hacen debajo."""
    if d is not None:
        def cas(v: float) -> str:
            r = _rd(v * f, d)
            return "$0$" if r == 0 else f"${tex(r, dec=d)}$"
        filas = [[k, cas(r["Q"]), cas(r["W"]), cas(r["U"])] for k, r in res]
        tq = sum(_rd(r["Q"] * f, d) for _, r in res)
        tw = sum(_rd(r["W"] * f, d) for _, r in res)
        filas.append(["**Ciclo**", f"${tex(tq, dec=d)}$", f"${tex(tw, dec=d)}$", "$0$"])
        return TABLA(["Proceso", f"$Q$ ({u})", f"$W$ ({u})", rf"$\Delta U$ ({u})"], filas, resaltar=[[len(filas) - 1, None]])
    filas = [[k, f"${tex(r['Q'] * f, cifras)}$", f"${tex(r['W'] * f, cifras)}$", f"${tex(r['U'] * f, cifras)}$"] for k, r in res]
    tq = sum(r["Q"] for _, r in res)
    tw = sum(r["W"] for _, r in res)
    tu = sum(r["U"] for _, r in res)
    filas.append(["**Ciclo**", f"${tex(tq * f, cifras)}$", f"${tex(tw * f, cifras)}$", "$0$" if abs(tu) < 1e-3 * max(1.0, abs(tq)) else f"${tex(tu * f, cifras)}$"])
    return TABLA(["Proceso", f"$Q$ ({u})", f"$W$ ({u})", rf"$\Delta U$ ({u})"], filas, resaltar=[[len(filas) - 1, None]])


def valores_visual(res: Sequence[Tuple[str, Dict[str, float]]]) -> Dict[str, Dict[str, float]]:
    return {k.replace("→", "").replace(" ", ""): {"Q": r["Q"], "W": r["W"], "U": r["U"]} for k, r in res}


def revisar_opciones(L: Leccion, motivos: Dict[str, Tuple[str, str]], titulo: str = "Revisar las opciones") -> Paso:
    """Último paso de una selección única: se descarta cada opción con su motivo y al final se marca la buena.

    motivos: letra -> (lo que se dice, lo que se escribe).
    """
    p = L.paso(titulo)
    for letra in sorted(L.opciones, key=lambda k: (k == L.correcta, k)):
        voz_, txt = motivos[letra]
        p.di(voz_, OPCION(letra, "correcta" if letra == L.correcta else "descartada", txt))
    return p


# ---------------------------------------------------------------- estados, cierre de ciclo y derivaciones desde la hoja

def _datos_justos(vals: Sequence[Tuple[float, str]], cuenta, muestra) -> List[str]:
    """Escribe los datos de una cuenta con las cifras justas (desde las de siempre, subiendo de una en una) para
    que la cuenta hecha con calculadora sobre lo escrito dé el resultado que se muestra.

    vals: (valor, 'c' para cifras significativas | 'd<n>' para n decimales fijos)."""
    objetivo = muestra(cuenta(*[v for v, _ in vals]))
    txts: List[str] = []
    for extra in range(0, 4):
        txts = []
        for v, modo in vals:
            if modo == "c":
                txts.append(tex(v, 4 + extra))
            elif modo == "cr":
                txts.append(tex(v, 4 + extra, recorta=True))
            else:
                txts.append(tex(v, dec=int(modo[1:]) + extra, recorta=extra == 0))
        try:
            if muestra(cuenta(*[_num_tex(t) for t in txts])) == objetivo:
                break
        except (ValueError, ZeroDivisionError):
            break
    return txts


def estado_T(p: Paso, g: Gas, k: str, *, voz: str = "", primero: bool = False, dec: int = 1) -> float:
    """T_k = p_k V_k / (nR), con la sustitución escrita (p y V en SI)."""
    t = g.t(k)
    ntx = tex(g.n, 4, recorta=True)
    nR = _num_tex(ntx) * R_GAS
    sp, sv = _datos_justos([(g.p(k), "c"), (g.v(k), "c")], lambda a, b: a * b / nR, lambda x: tex(x, dec=dec))
    acc = [F(r"pV=nRT\;\Rightarrow\;T=\frac{pV}{nR}")] if primero else []
    p.di(voz or f"Temperatura de {k} con el gas ideal.", *acc,
         M(rf"T_{k}=\frac{{p_{k}V_{k}}}{{nR}}=\frac{{({sp})({sv})}}{{({ntx})(8{{,}}314)}}={tex(t, dec=dec, u='K')}"))
    return t


def estado_V(p: Paso, g: Gas, k: str, *, voz: str = "", primero: bool = False, dec_T: int = 1) -> float:
    """V_k = nRT_k / p_k, con la sustitución escrita."""
    v = g.v(k)
    ntx = tex(g.n, 4, recorta=True)
    nR = _num_tex(ntx) * R_GAS
    sT, sp = _datos_justos([(g.t(k), f"d{dec_T}"), (g.p(k), "c")], lambda T, pp: nR * T / pp, lambda x: tex(x, 4))
    acc = [F(r"pV=nRT\;\Rightarrow\;V=\frac{nRT}{p}")] if primero else []
    p.di(voz or f"Volumen de {k} con el gas ideal.", *acc,
         M(rf"V_{k}=\frac{{nRT_{k}}}{{p_{k}}}=\frac{{({ntx})(8{{,}}314)({sT})}}{{{sp}}}={tex(v, 4, u='m^3')}"))
    return v


def estado_p(p: Paso, g: Gas, k: str, *, voz: str = "", primero: bool = False, dec_T: int = 1) -> float:
    """p_k = nRT_k / V_k, con la sustitución escrita."""
    pk = g.p(k)
    ntx = tex(g.n, 4, recorta=True)
    nR = _num_tex(ntx) * R_GAS
    sT, sv = _datos_justos([(g.t(k), f"d{dec_T}"), (g.v(k), "c")], lambda T, vv: nR * T / vv, lambda x: tex(x, 4))
    acc = [F(r"pV=nRT\;\Rightarrow\;p=\frac{nRT}{V}")] if primero else []
    p.di(voz or f"Presión de {k} con el gas ideal.", *acc,
         M(rf"p_{k}=\frac{{nRT_{k}}}{{V_{k}}}=\frac{{({ntx})(8{{,}}314)({sT})}}{{{sv}}}={tex(pk, 4, u='Pa')}"))
    return pk


def _sub(k: str) -> str:
    return k.replace("→", "").replace(" ", "")


def _dec_suma(vals: Sequence[float], total: float, cifras: int) -> int:
    """Decimales para escribir una suma: los sumandos y el total con los mismos decimales, sin perder cifras del total."""
    m = max([abs(v) for v in vals] + [abs(total), 1e-12])
    d1 = cifras - 1 - int(math.floor(math.log10(m)))
    d2 = cifras - 1 - int(math.floor(math.log10(max(abs(total), 1e-12))))
    return min(4, max(0, d1, d2))


def _rd(v: float, d: int) -> float:
    """Redondeo igual al de tex (mitad hacia arriba), para que las sumas escritas cuadren con lo que se ve."""
    return float(_redondea(v, d))


def _suma_tex(vals: Sequence[float], cifras: int, u: str = "J", d: Optional[int] = None) -> str:
    """'(a)+(b)+(c)=total' con decimales coherentes (la suma escrita da exactamente el total escrito)."""
    if d is None:
        d = _dec_suma(vals, sum(vals), cifras)
    redondeados = [_rd(v, d) for v in vals]
    total = sum(redondeados)
    return "+".join("(0)" if v == 0 else f"({tex(v, dec=d)})" for v in redondeados) + f"={tex(total, dec=d, u=u)}"


def cierre_ciclo(L: Leccion, res: Sequence[Tuple[str, Dict[str, float]]], *, refri: bool = False, cifras: int = 4,
                 titulo: str = "", id_e: str = "re", solo_totales: bool = False) -> Dict[str, float]:
    """Paso final de un ciclo: suma de trabajos, calores que entran y salen, y e (o K), todo sustituido.

    Todas las sumas usan los mismos decimales, y se suben si hace falta para que el trabajo neto escrito
    sea exactamente el calor neto escrito (si no, el redondeo haría que no cuadren por una unidad).
    """
    ws = [(_sub(k), r["W"]) for k, r in res]
    wnet = sum(w for _, w in ws)
    qin = [(_sub(k), r["Q"]) for k, r in res if r["Q"] > 1e-9]
    qout = [(_sub(k), r["Q"]) for k, r in res if r["Q"] < -1e-9]
    qh = sum(q for _, q in qin)
    qc = sum(q for _, q in qout)
    vw, vin, vout = [w for _, w in ws], [q for _, q in qin], [q for _, q in qout]
    d0 = d = _dec_suma(vw + vin + vout, wnet, cifras)

    def _cuadra(d: int) -> bool:
        """Con d decimales, ¿las sumas escritas cuadran entre sí y dan lo mismo que la respuesta (y la e)?"""
        wr = sum(_rd(w, d) for w in vw)
        qhr = sum(_rd(q, d) for q in vin)
        qcr = sum(_rd(q, d) for q in vout)
        if abs(wr - (qhr + qcr)) >= 0.5 * 10 ** (-d):
            return False
        if tex(wr, cifras) != tex(wnet, cifras) or (vin and tex(qhr, cifras) != tex(qh, cifras)) or (vout and tex(qcr, cifras) != tex(qc, cifras)):
            return False
        if abs(wr) < 1e-12 or abs(qhr) < 1e-12:
            return True
        if refri:
            return tex(abs(qhr) / abs(wr), 3) == tex(abs(qh) / abs(wnet), 3)
        return tex(wr / qhr, 3) == tex(wnet / qh, 3) and tex(1 - abs(qcr) / qhr, 3) == tex(1 - abs(qc) / qh, 3)

    while not _cuadra(d) and d < 4:
        d += 1
    wr = sum(_rd(w, d) for w in vw)
    qhr = sum(_rd(q, d) for q in vin)
    qcr = sum(_rd(q, d) for q in vout)
    p = L.paso(titulo or ("Coeficiente de rendimiento" if refri else "Trabajo neto, calores y eficiencia"))
    p.di("Primero la tabla de los procesos, para ver los signos.", tabla_quw(res, cifras=cifras, d=d), V(foco="ciclo"))
    extra = [NOTA("Aquí sumamos con " + ("un decimal más" if d - d0 == 1 else f"{d - d0} decimales más") +
                  " que en los pasos de arriba: con los números redondeados la suma se correría una unidad en la última cifra.")] if d > d0 else []
    p.di("El trabajo neto es la suma de los trabajos de cada proceso.",
         M(r"W_{\text{neto}}=" + "+".join(f"W_{{{k}}}" for k, _ in ws)),
         M("=" + _suma_tex(vw, cifras, d=d)), *extra)
    ent, sal = ("Q_C", "Q_H") if refri else ("Q_H", "Q_C")
    p.di("Los calores positivos son los que entran al gas; los negativos, los que salen." + (" En el refrigerador, el que entra viene del foco frío." if refri else ""),
         M(ent + "=" + "+".join(f"Q_{{{k}}}" for k, _ in qin) + "=" + _suma_tex(vin, cifras, d=d)),
         M(sal + "=" + "+".join(f"Q_{{{k}}}" for k, _ in qout) + "=" + _suma_tex(vout, cifras, d=d)))
    p.di("Revisión con la primera ley: en un ciclo la energía interna vuelve a su valor, así que el trabajo neto es el calor neto.",
         F(r"\Delta U=Q-W"), M(r"\Delta U_{\text{ciclo}}=0\;\Rightarrow\;W_{\text{neto}}=Q_{\text{neto}}"),
         M(r"Q_{\text{neto}}=" + _suma_tex([qhr, qcr], cifras, d=d)))
    out = {"W": wnet, "QH": qh, "QC": qc}
    if solo_totales:
        return out
    if refri:
        K = abs(qh) / abs(wnet)
        p.di("En el refrigerador sirve el calor que se le saca al foco frío; lo que se paga es el trabajo.",
             F(r"K=\frac{|Q_C|}{|W|}"),
             M(rf"K=\frac{{{tex(abs(qhr), dec=d)}}}{{{tex(abs(wr), dec=d)}}}"),
             R(rf"K={tex(K, 3)}", id_e))
        out["K"] = K
    else:
        e = wnet / qh
        p.di("La eficiencia es lo que se obtiene entre lo que se paga.",
             F(r"e=\frac{W}{Q_H}=1-\left|\frac{Q_C}{Q_H}\right|"),
             M(rf"e=\frac{{{tex(wr, dec=d)}}}{{{tex(qhr, dec=d)}}}"),
             R(rf"e={tex(e, 3)}={tex(e * 100, 3)}\,\%", id_e))
        p.di("Y la misma cuenta con los calores, para comprobar.",
             M(rf"e=1-\frac{{{tex(abs(qcr), dec=d)}}}{{{tex(qhr, dec=d)}}}={tex(1 - abs(qc) / qh, 3)}"))
        out["e"] = e
    return out


def deriva_carnot(p: Paso, *, refri: bool = False) -> None:
    """Escribe, desde la hoja, por qué |Q_C|/|Q_H| = T_C/T_H en un ciclo de Carnot (no viene impreso)."""
    p.di("Carnot no viene en la hoja; lo armamos desde lo que sí viene.",
         F(r"e=1-\left|\frac{Q_C}{Q_H}\right|"))
    p.di("El calor entra y sale solo en las isotermas, donde es igual al trabajo isotérmico.",
         F(r"\Delta U=Q-W=0\;\Rightarrow\;Q=W=nRT\ln\left(\frac{V_2}{V_1}\right)"),
         M(r"Q_H=nRT_H\ln\left(\frac{V_b}{V_a}\right)\qquad |Q_C|=nRT_C\ln\left(\frac{V_c}{V_d}\right)"))
    p.di("Las otras dos son adiabáticas y cumplen la relación de la hoja.",
         F(r"TV^{\gamma-1}=\text{cte}"),
         M(r"T_HV_b^{\gamma-1}=T_CV_c^{\gamma-1}\qquad T_HV_a^{\gamma-1}=T_CV_d^{\gamma-1}"))
    p.di("Dividimos una entre otra: se cancelan las temperaturas.",
         M(r"\left(\frac{V_b}{V_a}\right)^{\gamma-1}=\left(\frac{V_c}{V_d}\right)^{\gamma-1}\;\Rightarrow\;\frac{V_b}{V_a}=\frac{V_c}{V_d}"))
    p.di("Los logaritmos son iguales: la razón de calores es la razón de temperaturas.",
         M(r"\frac{|Q_C|}{Q_H}=\frac{nRT_C\ln(V_c/V_d)}{nRT_H\ln(V_b/V_a)}=\frac{T_C}{T_H}"))
    if refri:
        p.di("Para el refrigerador, el rendimiento de la hoja con el trabajo de la primera ley del ciclo.",
             F(r"K=\frac{|Q_C|}{|W|}"), M(r"|W|=|Q_H|-|Q_C|\;\Rightarrow\;K=\frac{|Q_C|}{|Q_H|-|Q_C|}"),
             M(r"K_{\text{Carnot}}=\frac{T_C}{T_H-T_C}"))
    else:
        p.di("Sustituimos en la eficiencia general.", M(r"e_{\text{Carnot}}=1-\frac{T_C}{T_H}"))


_ENTROPIA_CASOS = {
    "cp": ("A presión constante, la hoja da el calor de cada pedacito.",
           r"dQ=nC_p\,dT", r"\Delta S=\int_{T_1}^{T_2}\frac{nC_p\,dT}{T}=nC_p\ln\left(\frac{T_2}{T_1}\right)"),
    "cv": ("A volumen constante, también viene en la hoja.",
           r"dQ=nC_V\,dT", r"\Delta S=\int_{T_1}^{T_2}\frac{nC_V\,dT}{T}=nC_V\ln\left(\frac{T_2}{T_1}\right)"),
    "mc": ("Para un sólido o un líquido, el calor sensible de la hoja en un pedacito.",
           r"Q=mc\,\Delta T\;\Rightarrow\;dQ=mc\,dT", r"\Delta S=\int_{T_1}^{T_2}\frac{mc\,dT}{T}=mc\ln\left(\frac{T_2}{T_1}\right)"),
    "lat": ("En un cambio de fase la temperatura no cambia; el calor es el latente.",
            r"Q=\pm mL", r"\Delta S=\frac{1}{T}\int dQ=\frac{Q}{T}=\pm\frac{mL}{T}"),
    "iso": ("En una isoterma el calor es el trabajo isotérmico.",
            r"Q=W=nRT\ln\left(\frac{V_2}{V_1}\right)", r"\Delta S=\frac{Q}{T}=nR\ln\left(\frac{V_2}{V_1}\right)"),
    "adi": ("En una adiabática reversible no entra calor.",
            r"Q=0", r"dQ=0\;\Rightarrow\;\Delta S=0"),
    "foco": ("Un depósito enorme no cambia su temperatura: su calor entre esa temperatura.",
             "", r"\Delta S_{\text{dep}}=\frac{Q_{\text{dep}}}{T_{\text{dep}}}"),
}


def deriva_entropia(p: Paso, *casos: str, con_definicion: bool = True) -> None:
    """Escribe de dónde sale cada fórmula de ΔS: la definición dS = dQ/T más el dQ que sí da la hoja."""
    if con_definicion:
        p.di("La entropía no viene en la hoja: se parte de su definición, calor entre temperatura en cada pedacito.",
             M(r"dS=\frac{dQ}{T}\qquad \Delta S=\int\frac{dQ}{T}"),
             NOTA("$dS=dQ/T$ es la definición de entropía para un proceso reversible. El $dQ$ de cada caso sí sale de la hoja."))
    for c in casos:
        voz_, base, res = _ENTROPIA_CASOS[c]
        if base:
            p.di(voz_, F(base), M(res))
        else:
            p.di(voz_, M(res))


def receta_cono(L: Leccion, r1: Optional[float] = None, r2: Optional[float] = None, largo: Optional[float] = None, *,
                eje: str = "x", nr: Tuple[str, str] = ("r₁", "r₂"), u: str = "m", despues_de: int = 1) -> Paso:
    """Paso 'Antes de calcular' para los conos truncados: geometría con números y la receta de siempre.

    Para quien no entiende la geometría: el radio crece parejo (una recta), cada rebanada es un círculo
    y la receta es siempre la misma. Remite a la lección T-CONO, que lo explica desde cero.
    """
    p = Paso("Antes de calcular: cómo se ve y la receta")
    p.di("Si la forma no está clara, la lección T-CONO la explica desde cero con un dibujo y una tabla.",
         NOTA("Lección **T-CONO**: el tronco de cono desde cero (geometría, receta y la integral sin saltos)."))
    if r1 is not None and r2 is not None and largo is not None:
        b = (r2 - r1) / largo
        verbo = "crece" if b > 0 else "baja"
        p.di(f"El radio {verbo} parejo, como una rampa: va de {nu(r1, u, 3, recorta=True)} a {nu(r2, u, 3, recorta=True)} en un largo de {nu(largo, u, 3, recorta=True)}.",
             M(rf"\text{{cambio por cada {u}}}=\frac{{{tex(r2, 3)}-{tex(r1, 3)}}}{{{tex(largo, 3)}}}={tex(b, 3)}"))
        cab = [f"{eje} ({u})", tex(0, dec=0), plano_corto(largo / 2), plano_corto(largo)]
        fila = [f"radio ({u})", plano_corto(r1), plano_corto(r1 + b * largo / 2), plano_corto(r2)]
        p.di("En el inicio, en la mitad y al final:", TABLA(cab, [fila]))
    p.di(f"Cada rebanada es un círculo, así que el área cambia con {eje}. La receta es siempre la misma:".replace(f"con {eje}", "con " + dice(eje, {"x": "equis", "y": "ye", "r": "erre"}.get(eje, eje))),
         LISTA([f"1. Radio: recta de {nr[0]} a {nr[1]}", r"2. Área: $A=\pi r^2$", r"3. Fourier de la hoja: $H=-kA\frac{dT}{d" + eje + "}$",
                "4. Separar variables", "5. Integrar los dos lados", "6. Despejar $H$"]))
    L.pasos.insert(min(despues_de, len(L.pasos)), p)
    return p


def plano_corto(x: float) -> str:
    """Número corto para tablas de texto: 0,0275 · 0,1 · 1,5."""
    s = f"{x:.4g}".replace(".", ",")
    return s
