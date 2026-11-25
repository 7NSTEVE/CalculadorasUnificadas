"""Lenguaje para escribir lecciones sincronizadas.

Una lección es una lista de pasos. Cada paso es una lista de TRAMOS: un pedacito de voz y lo
que la mano hace mientras se dice. El reproductor no usa tiempos fijos: avanza la escritura
según qué palabra se está pronunciando, así que voz y trazo no pueden desfasarse.

Reglas (las vigilan las pruebas):
  * un movimiento por renglón, sin saltarse álgebra;
  * la voz nunca lleva LaTeX;
  * tramos cortos: hasta 30 palabras; pasos de hasta 75.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from .formato import ABRE, CIERRA, MEDIO, separa

Accion = Dict[str, Any]

# ---------------------------------------------------------------- símbolos hablados

_VOZ_SIMBOLO = {
    "ΔU": "delta U", "ΔT": "delta T", "ΔL": "delta L", "ΔV": "delta V", "ΔS": "delta S", "Δp": "delta pe",
    "ΔA": "delta A", "Δx": "delta equis", "α": "alfa", "β": "beta", "γ": "gama", "σ": "sigma", "η": "eta", "π": "pi",
    "ρ": "ro", "ε": "épsilon", "W": "doble ve", "Q": "cu", "H": "hache", "k": "ka", "K": "ka", "R": "erre", "n": "ene",
    "T": "te", "V": "ve", "p": "pe", "A": "a", "B": "be", "C": "ce", "D": "de", "L": "ele", "e": "e", "r": "erre",
    "m": "eme", "c": "ce", "N": "ene", "M": "eme", "v": "ve", "x": "equis", "y": "ye", "h": "hache", "t": "te", "a": "a", "b": "be",
    "f": "efe", "d": "de", "s": "ese", "F": "efe", "Y": "ye",
}
_SUB = str.maketrans("0123456789aeoxhklmnpst+-", "₀₁₂₃₄₅₆₇₈₉ₐₑₒₓₕₖₗₘₙₚₛₜ₊₋")
_VOZ_SUB = {"1": "uno", "2": "dos", "3": "tres", "4": "cuatro", "0": "cero", "5": "cinco", "f": "final", "i": "inicial", "H": "caliente", "C": "fría",
            "V": "ve", "P": "pe", "s": "ese", "neto": "neto", "rms": "eficaz", "med": "media"}


def s(sim: str, voz: Optional[str] = None) -> str:
    """Símbolo para narrar. s('T_B') se muestra T_B (con subíndice si existe) y se dice 'te be'."""
    base, _, sub = sim.partition("_")
    if voz is None:
        vb = _VOZ_SIMBOLO.get(base, base)
        if sub:
            vs = _VOZ_SUB.get(sub) or " ".join(_VOZ_SIMBOLO.get(ch, ch) for ch in sub)
            voz = f"{vb} {vs}"
        else:
            voz = vb
    vis = base
    if sub:
        tr = sub.translate(_SUB)
        vis = base + (tr if tr != sub and len(tr) == len(sub) and all(ch not in "0123456789aeoxhklmnpst" for ch in tr) else sub if len(sub) == 1 else sub)
    return f"{ABRE}{vis}{MEDIO}{voz}{CIERRA}"


# ---------------------------------------------------------------- acciones

def M(tex: str, id: str = "", color: str = "", rol: str = "") -> Accion:
    """Renglón matemático. rol: formula | despeje | sustituye | cuenta | resultado."""
    return {"a": "mate", "tex": tex, "id": id, "color": color, "rol": rol}


def F(tex: str, id: str = "") -> Accion:
    """Fórmula del formulario oficial (azul)."""
    return M(tex, id, "azul", "formula")


def R(tex: str, id: str = "") -> Accion:
    """Resultado final recuadrado (verde)."""
    return M(tex if r"\boxed" in tex else r"\boxed{" + tex + "}", id, "verde", "resultado")


def T(txt: str, id: str = "", color: str = "", clase: str = "") -> Accion:
    return {"a": "texto", "txt": txt, "id": id, "color": color, "clase": clase}


def NOTA(txt: str, id: str = "") -> Accion:
    return T(txt, id, "", "nota")


def OJO(txt: str, id: str = "") -> Accion:
    """Advertencia de error típico."""
    return T(txt, id, "naranja", "ojo")


def LISTA(items: Sequence[str], id: str = "", color: str = "") -> Accion:
    return {"a": "lista", "items": list(items), "id": id, "color": color}


def TABLA(cab: Sequence[str], filas: Sequence[Sequence[str]], id: str = "", resaltar=(), pie: str = "") -> Accion:
    return {"a": "tabla", "cab": list(cab), "filas": [list(map(str, f)) for f in filas], "id": id,
            "resaltar": [list(x) for x in resaltar], "pie": pie}


def MARCA(tipo: str, que: str, en: str = "", color: str = "rojo", nota: str = "", a_que: str = "", a_en: str = "", todas: bool = False) -> Accion:
    """tipo: circulo | subraya | resalta | tacha | flecha."""
    return {"a": "marca", "tipo": tipo, "que": que, "en": en, "color": color, "nota": nota, "a_que": a_que, "a_en": a_en, "todas": todas}


def DATO(frase: str, color: str = "naranja", nota: str = "") -> Accion:
    """Subraya una frase en el recorte del enunciado."""
    return {"a": "dato", "frase": frase, "color": color, "nota": nota}


def V(**cambio: Any) -> Accion:
    """Cambia el visual animado (foco, parámetros, fase)."""
    return {"a": "visual", "cambio": cambio}


def VISUAL(tipo: str, **spec: Any) -> Accion:
    """Pone un visual nuevo en el panel."""
    return {"a": "visual", "tipo": tipo, "spec": spec}


def OPCION(letra: str, estado: str, motivo: str = "") -> Accion:
    return {"a": "opcion", "letra": letra, "estado": estado, "motivo": motivo}


# ---------------------------------------------------------------- estructura

@dataclass
class Tramo:
    texto: str
    acciones: List[Accion]


@dataclass
class Paso:
    titulo: str
    tramos: List[Tramo] = field(default_factory=list)
    porque: str = ""
    origen: List[Dict[str, str]] = field(default_factory=list)
    lento: List[Dict[str, str]] = field(default_factory=list)
    pregunta: Optional[Dict[str, Any]] = None

    def di(self, texto: str, *acciones: Accion) -> "Paso":
        self.tramos.append(Tramo(" ".join(texto.split()), list(acciones)))
        return self

    def por_que(self, txt: str) -> "Paso":
        self.porque = txt
        return self

    def sale_de(self, *pares: Tuple[str, str]) -> "Paso":
        """Origen de cada cosa: ('$\\tfrac52 R$', 'gas diatómico, del formulario')."""
        self.origen.extend({"que": a, "de": b} for a, b in pares)
        return self

    def despacio(self, *renglones: Union[str, Tuple[str, str]]) -> "Paso":
        """Pasos intermedios extra, uno por renglón: (tex, explicación)."""
        for r in renglones:
            if isinstance(r, tuple):
                self.lento.append({"tex": r[0], "nota": r[1]})
            else:
                self.lento.append({"tex": r, "nota": ""})
        return self

    def pregunta_rapida(self, texto: str, opciones: Sequence[str], correcta: int, explica: str) -> "Paso":
        self.pregunta = {"texto": texto, "opciones": list(opciones), "correcta": correcta, "explica": explica}
        return self


class Leccion:
    def __init__(self, id: str, titulo: str, *, tema: str, tipo: str = "desarrollo", nivel: int = 3,
                 fuente: Optional[Dict[str, Any]] = None, enunciado: str = "", opciones: Optional[Dict[str, str]] = None,
                 correcta: str = "", visual: Optional[Dict[str, Any]] = None, aviso: str = "", claves: Sequence[str] = (),
                 alias: Sequence[str] = ()) -> None:
        self.id = id
        self.titulo = titulo
        self.tema = tema
        self.tipo = tipo
        self.nivel = nivel
        self.fuente = fuente or {}
        self.enunciado = " ".join(enunciado.split()) if enunciado else ""
        self.opciones = opciones or {}
        self.correcta = correcta
        self.visual = visual
        self.aviso = aviso
        self.claves = list(claves)
        self.alias = list(alias)
        self.pasos: List[Paso] = []
        self.respuestas: List[Dict[str, Any]] = []
        self.practica: Optional[Dict[str, Any]] = None
        self.formulas: List[str] = []
        self._oficial: Optional[Tuple[str, Tuple[Any, ...], float, str]] = None

    def paso(self, titulo: str) -> Paso:
        p = Paso(titulo)
        self.pasos.append(p)
        return p

    def responde(self, inciso: str, tex: str, valor: Optional[float] = None, unidad: str = "", tol: float = 0.01) -> "Leccion":
        self.respuestas.append({"inciso": inciso, "tex": tex, "valor": valor, "unidad": unidad, "tol": tol})
        return self

    def usa(self, *formulas: str) -> "Leccion":
        self.formulas.extend(formulas)
        return self

    def te_toca(self, texto: str, valor: float, unidad: str, tol: float = 0.02, pista: str = "", solucion: str = "") -> "Leccion":
        """Mini ejercicio para que la persona lo haga sola."""
        self.practica = {"texto": texto, "valor": valor, "unidad": unidad, "tol": tol, "pista": pista, "solucion": solucion}
        return self

    def oficial(self, de: str, *valores: Any, tol: float = 0.015, nota: str = "") -> "Leccion":
        """Respuesta publicada por la cátedra: una letra, o los números en el mismo orden de responde()."""
        self._oficial = (de, valores, tol, nota)
        return self

    def cotejo(self) -> Optional[Dict[str, Any]]:
        if not self._oficial:
            return None
        de, vals, tol, nota = self._oficial
        fallas: List[str] = []
        if vals and isinstance(vals[0], str):
            if self.tipo == "seleccion" and vals[0] != self.correcta:
                fallas.append(f"la oficial marca {vals[0]}")
        else:
            mios = [r["valor"] for r in self.respuestas if r["valor"] is not None]
            for a, b in zip(mios, vals):
                if b is None:
                    continue
                if abs(a - b) > tol * max(abs(b), 1e-12):
                    fallas.append(f"{a:.5g} frente a {b:.5g}")
        if fallas:
            return {"estado": "difiere", "texto": f"Difiere de la solución publicada ({de}): {'; '.join(fallas)}. {nota}".strip(), "de": de}
        return {"estado": "coincide", "texto": f"Coincide con la solución publicada ({de}). {nota}".strip(), "de": de}

    # ------------------------------------------------------------ exportar

    def a_json(self) -> Dict[str, Any]:
        pasos = []
        cont = 0
        for i, p in enumerate(self.pasos):
            hablado_total = ""
            tramos = []
            for t in p.tramos:
                vis, hab, trozos = separa(t.texto)
                ini = len(hablado_total) + (1 if hablado_total else 0)
                hablado_total = (hablado_total + " " + hab) if hablado_total else hab
                # Cada palabra visible con el rango de caracteres hablados que ocupa
                pals = []
                cur = ini
                for v, h in trozos:
                    pals.append([v, cur, cur + len(h)])
                    cur += len(h) + 1
                accs = []
                for a in t.acciones:
                    cont += 1
                    b = {k: x for k, x in a.items() if x not in ("", None, [], False)}
                    b.setdefault("id", a.get("id") or f"p{i + 1}a{cont}")
                    accs.append(b)
                tramos.append({"v": vis, "c0": ini, "c1": len(hablado_total), "pal": pals, "acc": accs})
            d: Dict[str, Any] = {"n": i + 1, "titulo": p.titulo, "habla": hablado_total, "tramos": tramos}
            if p.porque:
                d["porque"] = p.porque
            if p.origen:
                d["origen"] = p.origen
            if p.lento:
                d["lento"] = p.lento
            if p.pregunta:
                d["pregunta"] = p.pregunta
            pasos.append(d)
        return {
            "id": self.id, "titulo": self.titulo, "tema": self.tema, "tipo": self.tipo, "nivel": self.nivel,
            "fuente": self.fuente, "enunciado": self.enunciado, "opciones": self.opciones, "correcta": self.correcta,
            "visual": self.visual, "aviso": self.aviso, "claves": self.claves, "alias": self.alias,
            "pasos": pasos, "respuestas": self.respuestas, "practica": self.practica, "formulas": self.formulas,
            "cotejo": self.cotejo(),
        }


# ---------------------------------------------------------------- vigilancia de calidad

_RE_LATEX_VOZ = re.compile(r"[\\$^_{}]|<cue")
MAX_PAL_TRAMO = 34
MAX_PAL_PASO = 90


def revisar(lec: Leccion) -> List[str]:
    """Devuelve la lista de faltas de una lección (vacía = bien)."""
    f: List[str] = []
    j = lec.a_json()
    if not j["pasos"]:
        f.append("sin pasos")
    ids = set()
    for p in j["pasos"]:
        if not p["titulo"]:
            f.append(f"paso {p['n']} sin título")
        if not p["tramos"]:
            f.append(f"paso {p['n']} vacío")
        n_pal = len(p["habla"].split())
        if n_pal > MAX_PAL_PASO:
            f.append(f"paso {p['n']}: {n_pal} palabras habladas (máx {MAX_PAL_PASO})")
        if _RE_LATEX_VOZ.search(p["habla"]):
            f.append(f"paso {p['n']}: LaTeX en la voz -> {p['habla'][:80]}")
        for t in p["tramos"]:
            if len(t["v"].split()) > MAX_PAL_TRAMO:
                f.append(f"paso {p['n']}: tramo de {len(t['v'].split())} palabras")
            if _RE_LATEX_VOZ.search(t["v"].replace("_", "")):
                f.append(f"paso {p['n']}: LaTeX en el subtítulo -> {t['v'][:60]}")
            for a in t["acc"]:
                if a["a"] in ("mate", "texto", "lista", "tabla"):
                    if a["id"] in ids:
                        f.append(f"id repetido {a['id']}")
                    ids.add(a["id"])
                if a["a"] == "marca" and a.get("en") and a["en"] not in ids:
                    f.append(f"paso {p['n']}: marca sobre id inexistente {a['en']}")
    if lec.tipo == "seleccion":
        if lec.correcta not in lec.opciones:
            f.append("selección sin letra correcta")
        dichas = {a["letra"]: a["estado"] for p in j["pasos"] for t in p["tramos"] for a in t["acc"] if a["a"] == "opcion"}
        if dichas.get(lec.correcta) != "correcta":
            f.append("la lección no marca la opción correcta")
    return f
