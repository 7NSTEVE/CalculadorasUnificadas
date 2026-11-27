"""Documentos del curso: encuentra cada problema dentro de su PDF y lo recorta exacto.

Reemplaza a la segmentación vieja, que pegaba la página entera cuando el problema se llamaba
"Ejercicio 5", perdía las figuras que caían en la página siguiente y no sabía trabajar con varios
PDF a la vez. El recorte nunca incluye la solución.
"""

from __future__ import annotations

import hashlib
import io
import os
import pathlib
import re
import threading
import unicodedata
import zipfile
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence, Tuple

import fitz

from .pdf_rag import fold_accents

ZOOM = 2.3
MAX_ANCHO = 1600
MAX_PDF = 60 * 1024 * 1024
MAX_ZIP_SUMA = 120 * 1024 * 1024
MAX_PAGINAS = 400

# id -> (patrones de nombre, título visible, tipo)
FUENTES: Dict[str, Tuple[Tuple[str, ...], str, str]] = {
    "superexamen": (("superexamen_tercer_parcial_fisica_general_iii.pdf",), "Superexamen", "superexamen"),
    "recomendados": (("ejercicios_recomendados_3_iis2026.pdf",), "Ejercicios Recomendados 3", "recomendados"),
    "recomendados1": (("ejercicios_recomendados_1_iis2026.pdf", "documentos ii - 2026/ejercicios_recomendados_1_iis2026.pdf"), "Ejercicios Recomendados 1 (II-2026)", "recomendados"),
    "recomendados2": (("ejercicios_recomendados_2_iis2026.pdf", "documentos ii - 2026/ejercicios_recomendados_2_iis2026.pdf"), "Ejercicios Recomendados 2 (II-2026)", "recomendados"),
    "24-I-T4": (("evaluaciones i-2024/tareas/tarea_4-enunciado.pdf", "tarea_4-enunciado.pdf"), "Tarea 4 I-2024", "tarea"),
    "24-II-T3": (("evaluaciones ii-2024/tareas/tarea 3/tarea_3_enunciado.pdf",), "Tarea 3 II-2024", "tarea"),
    "25-I-T3": (("evaluaciones i-2025/tarea 3/tarea_3_enunciado.pdf",), "Tarea 3 I-2025", "tarea"),
    "25-II-T3": (("evaluaciones ii-2025/tareas 2025-ii/tarea3_catedra_fg3_ii2025.pdf", "tarea3_catedra_fg3_ii2025.pdf"), "Tarea 3 II-2025", "tarea"),
    "26-I-T3": (("documentos i-2026/tareas/tarea3_catedra_fg3_i2026.pdf", "tarea3_catedra_fg3_i2026.pdf"), "Tarea 3 I-2026", "tarea"),
    "formulario": (("documentos-ii-2026_ecuaciones_tercer_parcial_fg3.pdf",), "Formulario del III Parcial", "hoja"),
    "formulario_p1": (("documentos-ii-2026_ecuaciones_importantes_parcial_1.pdf",), "Formulario del I Parcial", "hoja"),
    "formulario_p2": (("documentos-ii-2026_ecuaciones_segundo_parcial_fg3(1).pdf", "documentos-ii-2026_ecuaciones_segundo_parcial_fg3.pdf"), "Formulario del II Parcial", "hoja"),
    "programa": (("documentos-ii-2026_fi2103_fisica_general_iii_2026-ii (2).pdf", "documentos-ii-2026_fi2103_fisica_general_iii_2026-ii.pdf"), "Programa del curso FI2103 II-2026", "hoja"),
    "23-II-O": (("banco-de-examenes_ii-2023_ii_examen_parcial_ordinario.pdf",), "III Parcial ordinario II-2023", "examen"),
    "23-II-E": (("banco-de-examenes_ii-2023_ii_examen_extraordinario.pdf",), "III Parcial extraordinario II-2023", "examen"),
    "23-II-R": (("banco-de-examenes_ii-2023_examen_de_reposicion.pdf", "banco-de-examenes_ii-2023_examen_de_reposicion.zip"), "Reposición II-2023", "examen"),
    "24-I-O": (("banco-de-examenes_i-2024_iii_examen_parcial_ordinario.pdf",), "III Parcial ordinario I-2024", "examen"),
    "24-I-E": (("banco-de-examenes_i-2024_iii_examen_parcial_extraordinario.pdf",), "III Parcial extraordinario I-2024", "examen"),
    "24-I-R": (("banco-de-examenes_i-2024_examen_de_reposicion.pdf",), "Reposición I-2024", "examen"),
    "24-II-O": (("banco-de-examenes_ii-2024_iii_examen_parcial_ordinario_solucion.pdf",), "III Parcial ordinario II-2024", "examen"),
    "24-II-E": (("banco-de-examenes_ii-2024_iii_examen_parcial_exrtraordinario_solucion.pdf",), "III Parcial extraordinario II-2024", "examen"),
    "24-II-R": (("banco-de-examenes_ii-2024_examen_de_reposicion.pdf",), "Reposición II-2024", "examen"),
    "25-I-O": (("banco-de-examenes_i-2025_iii_examen_parcial_ordinario__solucion.pdf",), "III Parcial ordinario I-2025", "examen"),
    "25-I-E": (("banco-de-examenes_i-2025_iii_examen_parcial_extra_solucion.pdf", "banco-de-examenes_i-2025_iii_examen_parcial_extra_solucion (1).pdf"), "III Parcial extraordinario I-2025", "examen"),
    "25-II-O": (("evaluaciones-ii-2025_examenes_2025ii_fg3_solucion_exam3_ord.pdf",), "III Parcial ordinario II-2025", "examen"),
    "25-II-E": (("evaluaciones-ii-2025_examenes_2025ii_fg3_solucion_exam3_extraord.pdf",), "III Parcial extraordinario II-2025", "examen"),
    "25-II-R": (("evaluaciones-ii-2025_examenes_2025ii_fg3_solucion_examen_reposicion_ordinario.pdf",), "Reposición II-2025", "examen"),
    "26-I-O": (("documentos-i-2026_examenes_2026i_fg3_solucion_exam3_ordinario.pdf",), "III Parcial ordinario I-2026", "examen"),
    "26-I-E": (("documentos-i-2026_examenes_2026i_fg3_solucion_exam3_extraordinario.pdf",), "III Parcial extraordinario I-2026", "examen"),
    "26-I-R": (("documentos-i-2026_examenes_2026i_fg3_solucion_examen_reposicion_ordinario.pdf",), "Reposición I-2026", "examen"),
    # Reposición II-2026: I y II parcial del semestre (hechos en LaTeX, preguntas «1)», «2)»…) y la suficiencia I-2026
    "26-II-1O": (("documentos-ii-2026_i_examen__ord___2s_2026_solucion.pdf", "i_examen__ord___2s_2026_solucion.pdf"), "I parcial ordinario II-2026", "examen26"),
    "26-II-1E": (("documentos-ii-2026_i_examen__ext___2s_2026_solucion.pdf", "i_examen__ext___2s_2026_solucion.pdf"), "I parcial extraordinario II-2026", "examen26"),
    "26-II-2O": (("documentos-ii-2026_ii_examen__ord___2s_2026_solucion.pdf", "ii_examen__ord___2s_2026_solucion.pdf"), "II parcial ordinario II-2026", "examen26"),
    "26-II-2E": (("documentos-ii-2026_ii_examen__ext___2s_2026_solucion.pdf", "ii_examen__ext___2s_2026_solucion.pdf"), "II parcial extraordinario II-2026", "examen26"),
    "26-II-O": (("documentos-ii-2026_iii_examen__ord___2s_2026_solucion.pdf", "iii_examen__ord___2s_2026_solucion.pdf"), "III parcial ordinario II-2026", "examen26"),
    "26-I-SUF": (("documentos-i-2026_examenes_2026i_fg3_examen_suficiencia.pdf", "2026i_fg3_examen_suficiencia.pdf"), "Examen de suficiencia I-2026", "examen"),
    "26-F": (("documentos-i-2026_material-complementario_folleto-fisica3_(1).pdf", "documentos-i-2026_material-complementario_folleto-fisica3_.pdf"), "Folleto de Física III", "folleto"),
    "26-II-P1": (("practica del parcial 3_iis2026.pdf",), "Práctica del Parcial 3 (II-2026)", "practica"),
    "26-II-P2": (("practica tercer parcial fiii.pdf",), "Práctica del tercer examen (II-2026)", "practica"),
}

# Prácticas del II-2026: cada problema tiene su lección PR1-N, PR2-SN (selección) o PR2-DN (desarrollo).
PRACTICAS = {"26-II-P1": "PR1", "26-II-P2": "PR2"}

# Parciales del II-2026 y suficiencia I-2026 (Reposición II-2026): la pregunta N es la lección <prefijo>-N.
PARCIALES_REPO26 = {"26-II-1O": "P1O26", "26-II-1E": "P1E26", "26-II-2O": "P2O26", "26-II-2E": "P2E26", "26-II-O": "O26", "26-I-SUF": "SUF26"}

# Recomendados P1/P2 II-2026 y Tareas Termo ≤ 20.5 (2024–2026): el problema N enlaza a <prefijo>-N.
TAREAS_Y_REC = {
    "recomendados1": "E1",
    "recomendados2": "E2",
    "24-I-T4": "T4-24I",
    "24-II-T3": "T3-24II",
    "25-I-T3": "T3-25I",
    "25-II-T3": "T3-25II",
    "26-I-T3": "T3-26I",
}

# Reposición Parcial 1 y Parcial 2: enlace directo (examen, número) -> ID de lección RP-*.
REPOSICION_MAPA: Dict[Tuple[str, str], str] = {
    ("23-II-R", "1"): "RP-23II-1",
    ("23-II-R", "2"): "RP-23II-2",
    ("24-I-R", "1"): "RP-24I-1",
    ("24-I-R", "2"): "RP-24I-2",
    ("24-II-R", "1"): "RP-24II-1",
    ("24-II-R", "2"): "RP-24II-2",
    ("25-II-R", "1"): "RP-25II-1",
    ("25-II-R", "2"): "RP-25II-2",
    ("26-I-R", "1"): "RP-26I-1",
    ("26-I-R", "2"): "RP-26I-2",
    ("26-I-R", "3"): "RP-26I-3",
    ("26-I-R", "4"): "RP-26I-4",
    ("26-I-R", "7"): "RP-26I-7",
    ("26-I-R", "8"): "RP-26I-8",
}

# Problemas del Superexamen cuya figura está cambiada en ese PDF: se recortan del examen original.
FIGURA_CAMBIADA = {"Q038", "Q048", "Q083", "Q091", "Q095"}


def _norma_archivo(s: str) -> str:
    return fold_accents(s).lower().strip()


@dataclass
class Linea:
    x0: float
    y0: float
    x1: float
    y1: float
    texto: str
    tam: float
    negrita: bool
    color: int = 0

    @property
    def azul(self) -> bool:
        """Las soluciones del folleto y de algunos exámenes vienen escritas en azul."""
        r, g, b = (self.color >> 16) & 255, (self.color >> 8) & 255, self.color & 255
        return b > 105 and r < 110 and g < 125 and b - max(r, g) > 40


@dataclass
class Problema:
    clave: str
    num: str
    titulo: str
    pagina: int
    tiras: List[Tuple[int, fitz.Rect]] = field(default_factory=list)
    texto: str = ""
    solucion: List[Tuple[int, fitz.Rect]] = field(default_factory=list)
    extra: Dict[str, Any] = field(default_factory=dict)
    tapas: List[Tuple[int, fitz.Rect]] = field(default_factory=list)  # lo que no debe verse (respuestas)


# ---------------------------------------------------------------------- utilidades de página

def _lineas(page: fitz.Page) -> List[Linea]:
    out: List[Linea] = []
    for b in page.get_text("dict")["blocks"]:
        for ln in b.get("lines", []):
            t = "".join(s["text"] for s in ln["spans"])
            if not t.strip():
                continue
            if re.fullmatch(r"\s*\d{1,3}\s*", t) and ln["bbox"][1] > page.rect.height - 78 and page.rect.height < 800:
                continue  # número de página
            s0 = next((s for s in ln["spans"] if s["text"].strip()), ln["spans"][0])
            x0, y0, x1, y1 = ln["bbox"]
            out.append(Linea(x0, y0, x1, y1, " ".join(t.split()), s0["size"], bool(s0["flags"] & 16), int(s0.get("color") or 0)))
    out.sort(key=lambda k: (round(k.y0, 0), k.x0))
    return out


def _graficos(page: fitz.Page) -> List[fitz.Rect]:
    """Imágenes y trazos, sin los adornos de página completa."""
    area = page.rect.get_area()
    rs: List[fitz.Rect] = []
    for inf in page.get_image_info():
        r = fitz.Rect(inf["bbox"])
        if 20 < r.get_area() < area * 0.85:
            rs.append(r)
    try:
        dib = page.get_drawings()
    except Exception:
        dib = []
    for d in dib[:4000]:
        r = fitz.Rect(d["rect"])
        if r.is_infinite or r.width < 0.2 and r.height < 0.2:
            continue
        if r.get_area() > area * 0.6 or r.height > page.rect.height * 0.75:
            continue
        if d.get("fill") == (1.0, 1.0, 1.0) and not d.get("color"):
            continue
        rs.append(r + (-0.6, -0.6, 0.6, 0.6))
    return rs


def _zona(tipo: str, page: fitz.Page) -> Tuple[float, float]:
    """Franja vertical útil (sin encabezado ni pie)."""
    h = page.rect.height
    if tipo == "folleto":
        return 127.0, 690.0
    if tipo == "superexamen":
        return 60.0, 784.0
    if tipo == "recomendados":
        return 60.0, h - 48.0
    if tipo == "practica":
        return 58.0, h - 50.0  # sin el encabezado del curso ni el pie "Material de estudio"
    return 50.0, h - 50.0


def _ajusta(page: fitz.Page, r: fitz.Rect, lineas: Sequence[Linea], graf: Sequence[fitz.Rect]) -> Optional[fitz.Rect]:
    """Recorta la franja al contenido real."""
    u: Optional[fitz.Rect] = None
    for k in lineas:
        c = (k.y0 + k.y1) / 2
        if r.y0 - 1 <= c <= r.y1 + 1 and k.x1 > r.x0 and k.x0 < r.x1:
            b = fitz.Rect(k.x0, k.y0, k.x1, k.y1)
            u = b if u is None else u | b
    for g in graf:
        c = (g.y0 + g.y1) / 2
        if r.y0 - 1 <= c <= r.y1 + 1 and g.x1 > r.x0 and g.x0 < r.x1:
            b = fitz.Rect(max(g.x0, r.x0), max(g.y0, r.y0), min(g.x1, r.x1), min(g.y1, r.y1))
            if b.is_empty:
                continue
            u = b if u is None else u | b
    if u is None:
        return None
    u = fitz.Rect(max(0, u.x0 - 7), max(r.y0 - 3, u.y0 - 4), min(page.rect.x1, u.x1 + 7), min(r.y1 + 5, u.y1 + 5))
    return u if u.width > 10 and u.height > 6 else None


# ---------------------------------------------------------------------- segmentadores

_RE_FIN = re.compile(r"^(soluci[o´]*n\b|soluci[oó]n|\(?primera parte|\(?segunda parte|\(?tercera parte|\(?cuarta parte|problemas de des|respuestas?$|pregunta$|resumen de la soluci)", re.I)
_RE_NUM = re.compile(r"^(\d{1,2})\s*\.(?!\d)\s*(.*)$")


def _arma(doc: fitz.Document, tipo: str, anclas: List[Dict[str, Any]], fines: List[Tuple[int, float]], cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]]) -> List[Problema]:
    """Convierte anclas (página, y, clave…) en problemas con sus tiras recortadas."""
    def pag(i: int) -> Tuple[List[Linea], List[fitz.Rect]]:
        if i not in cache:
            cache[i] = (_lineas(doc[i]), _graficos(doc[i]))
        return cache[i]

    out: List[Problema] = []
    for k, a in enumerate(anclas):
        sig = (anclas[k + 1]["p"], anclas[k + 1]["y"]) if k + 1 < len(anclas) else (len(doc) - 1, doc[-1].rect.height)
        if tipo == "superexamen" and k + 1 < len(anclas):
            sig = (anclas[k + 1]["p"], 0.0)  # cada problema abre página
        fin = sig
        for f in fines:
            if (a["p"], a["y"] + 4) < f < fin:
                fin = f
                break
        prob = Problema(a["clave"], a["num"], a.get("titulo", ""), a["p"] + 1, extra=a.get("extra", {}))
        tope_pag = a.get("max_pag", 3)
        for i in range(a["p"], min(fin[0], a["p"] + tope_pag) + 1):
            page = doc[i]
            z0, z1 = _zona(tipo, page)
            y0 = a["y"] - 2 if i == a["p"] else z0
            y1 = fin[1] - 3 if i == fin[0] else z1
            if y1 - y0 < 8:
                continue
            ls, gs = pag(i)
            r = _ajusta(page, fitz.Rect(0, y0, page.rect.width, min(y1, z1)), ls, gs)
            if r is not None:
                prob.tiras.append((i, r))
        # Solución: de donde termina el enunciado hasta el siguiente problema
        if fin != sig:
            for i in range(fin[0], min(sig[0], fin[0] + 6) + 1):
                page = doc[i]
                z0, z1 = _zona(tipo, page)
                y0 = fin[1] if i == fin[0] else z0
                y1 = sig[1] - 3 if i == sig[0] else z1
                if y1 - y0 > 10:
                    prob.solucion.append((i, fitz.Rect(0, y0, page.rect.width, y1)))
        if not prob.tiras:
            continue
        prob.texto = _texto(doc, prob.tiras)
        if not prob.titulo:
            t = re.sub(r"^\s*\d{1,2}\s*\.\s*(\[[^\]]*\]|\([^)]*puntos\))?\s*", "", prob.texto)
            prob.titulo = (t[:96] + "…") if len(t) > 98 else t
        out.append(prob)
    return out


def _texto(doc: fitz.Document, tiras: Sequence[Tuple[int, fitz.Rect]]) -> str:
    s = " ".join(doc[i].get_text("text", clip=r) for i, r in tiras)
    s = s.replace("ﬁ", "fi").replace("ﬂ", "fl")
    s = re.sub(r"[´`¨˜]\s*([aeiounAEIOUN])", lambda m: m.group(1), s)
    s = re.sub(r"(\w)-\s+([a-záéíóúñ])", r"\1\2", s)
    return " ".join(s.split())


def segmenta_examen(doc: fitz.Document) -> List[Problema]:
    anclas: List[Dict[str, Any]] = []
    fines: List[Tuple[int, float]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    esperado = 1
    for i in range(len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        z0, z1 = _zona("examen", doc[i])
        for k in ls:
            if k.y0 < z0 or k.y0 > z1:
                continue
            t = k.texto
            mp = re.match(r"^Problema\s+(\d{1,2})\s*\[", t)
            if mp:
                anclas.append({"p": i, "y": k.y0, "num": mp.group(1), "clave": mp.group(1)})
                continue
            m = _RE_NUM.match(t)
            if m and k.x0 < 95 and k.tam > 10.4 and int(m.group(1)) == esperado:
                # En portada y resumen hay tablas numeradas con letra pequeña: se exige texto o renglón propio
                anclas.append({"p": i, "y": k.y0, "num": m.group(1), "clave": m.group(1)})
                esperado += 1
                continue
            if esperado > 1 and k.x0 < 60 and re.match(r"^\(\d{1,2} puntos\)\s+\S", t):
                # El número quedó como viñeta automática y no está en el texto
                anclas.append({"p": i, "y": k.y0, "num": str(esperado), "clave": str(esperado)})
                esperado += 1
                continue
            if k.x0 < 360 and _RE_FIN.match(fold_accents(t)) and len(t) < 110:
                fines.append((i, k.y0))
            elif k.azul and len(t) > 3:
                fines.append((i, k.y0))
    return _arma(doc, "examen", anclas, sorted(fines), cache)


_RE_NUM_PAREN = re.compile(r"^(\d{1,2})\)\s+\S")


def segmenta_examen_latex(doc: fitz.Document) -> List[Problema]:
    """Parciales del II-2026, hechos en LaTeX: cada pregunta empieza con «1)», «2)», … en el margen izquierdo.

    La solución viene al final, después de «Solución …», que corta la última pregunta.
    """
    anclas: List[Dict[str, Any]] = []
    fines: List[Tuple[int, float]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    esperado = 1
    for i in range(len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        z0, z1 = _zona("examen", doc[i])
        for k in ls:
            if k.y0 < z0 or k.y0 > z1:
                continue
            t = k.texto
            m = _RE_NUM_PAREN.match(t)
            if m and k.x0 < 95 and k.tam > 10.4 and int(m.group(1)) == esperado:
                anclas.append({"p": i, "y": k.y0, "num": m.group(1), "clave": m.group(1)})
                esperado += 1
                continue
            if k.x0 < 360 and _RE_FIN.match(fold_accents(t)) and len(t) < 110:
                fines.append((i, k.y0))
    return _arma(doc, "examen", anclas, sorted(fines), cache)


def segmenta_recomendados(doc: fitz.Document) -> List[Problema]:
    anclas: List[Dict[str, Any]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    for i in range(len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        for k in ls:
            m = re.match(r"^Ejercicio\s+(\d{1,3})\b", k.texto)
            if m and k.negrita:
                anclas.append({"p": i, "y": k.y0, "num": m.group(1), "clave": m.group(1)})
    return _arma(doc, "recomendados", anclas, [], cache)


_RE_META = re.compile(r"^(Examen|Folleto|Material|Ejercicios)[^|]*\|\s*([0-9]{2}-[IVF]+(?:-[A-Z])?|[0-9]{2}-F)\s*\|\s*(.+?)\s*\|\s*p\.\s*(\d+)", re.I)


def segmenta_superexamen(doc: fitz.Document) -> List[Problema]:
    anclas: List[Dict[str, Any]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    for i in range(len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        q = next((k for k in ls if re.fullmatch(r"Q\d{3}", k.texto) and k.x0 < 100), None)
        if q is None:
            continue
        meta: Dict[str, Any] = {}
        y = q.y1
        for k in ls:
            if k.tam < 9 and k.y0 > q.y0 and k.y0 < q.y0 + 110 and k.x0 < 240:
                y = max(y, k.y1)
                for campo in ("Subtema", "Procedimiento", "Tema", "Familia"):
                    if k.texto.startswith(campo + ":"):
                        meta[campo.lower()] = k.texto.split(":", 1)[1].strip()
                m = re.match(r"^[^|]+\|\s*(\d{2}-[A-Z0-9.\-]+)\s*\|\s*(?:pregunta|problema|ejercicio|item)?\s*([0-9A-Za-z.\- ]+?)\s*\|\s*p\.\s*(\d+)", k.texto)
                if m:
                    ex, preg = m.group(1), m.group(2).strip()
                    mf = re.fullmatch(r"(\d{2}-F)-F(\d\.\d)", ex)
                    if mf:
                        ex, preg = mf.group(1), f"{mf.group(2)}-{preg}"
                    meta.update(examen=ex, preg=preg, pag_examen=int(m.group(3)))
                elif "|" in k.texto and "examen" not in meta:
                    meta["origen"] = k.texto
            mm = re.match(r"^Nivel\s+(\d)", k.texto)
            if mm:
                meta["nivel"] = int(mm.group(1))
        anclas.append({"p": i, "y": y + 4, "num": q.texto, "clave": q.texto, "extra": meta, "max_pag": 1})
    probs = _arma(doc, "superexamen", anclas, [], cache)
    # Cada problema vive en su página (o la siguiente si no abre otro): nunca en más
    for p in probs:
        p.tiras = p.tiras[:2]
    return probs


def segmenta_folleto(doc: fitz.Document) -> List[Problema]:
    """Ejemplos resueltos y propuestos. En los resueltos solo se toma el enunciado."""
    anclas: List[Dict[str, Any]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    sec = ""
    prop = False
    esperado = 1
    cortes: List[Tuple[int, float]] = []
    for i in range(4, len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        for k in ls:
            if k.tam > 20:
                cortes.append((i, k.y0))  # portada de capítulo
                continue
            if k.tam > 15 and re.fullmatch(r"\d\.\d", k.texto):
                sec = k.texto
                prop = False
                esperado = 1
                cortes.append((i, k.y0))
                continue
            if k.tam > 15 and "ropuestos" in k.texto:
                prop = True
                continue
            m = _RE_NUM.match(k.texto)
            if m and sec and 90 < k.x0 < 152 and 11 < k.tam < 13 and int(m.group(1)) == esperado:
                anclas.append({
                    "p": i, "y": k.y0, "num": f"{sec}-{m.group(1)}", "clave": f"{sec}-{m.group(1)}",
                    "extra": {"seccion": sec, "propuesto": prop, "x": k.x0}, "max_pag": 4,
                })
                esperado += 1
    probs = _arma(doc, "folleto", anclas, sorted(cortes), cache)
    for p in probs:
        completas = list(p.tiras)
        p.tiras = _solo_enunciado(doc, p, cache)
        p.solucion = completas
        p.texto = _texto(doc, p.tiras)
        t = re.sub(r"^\s*\d{1,2}\s*\.\s*", "", p.texto)
        p.titulo = (t[:96] + "…") if len(t) > 98 else t
    return probs


def _solo_enunciado(doc: fitz.Document, p: Problema, cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]]) -> List[Tuple[int, fitz.Rect]]:
    """Solo lo escrito en negro (y las figuras con su rótulo): la solución del folleto va en azul."""
    tiras: List[Tuple[int, fitz.Rect]] = []
    for i, r in p.tiras:
        ls = [k for k in cache[i][0] if r.y0 - 1 <= (k.y0 + k.y1) / 2 <= r.y1 + 1]
        azules = [fitz.Rect(k.x0, k.y0, k.x1, k.y1) for k in ls if k.azul]
        fig: Optional[fitz.Rect] = None
        for g in cache[i][1]:
            if g.y0 < r.y0 - 2 or g.y1 > r.y1 + 4 or g.height < 10 or g.width < 10:
                continue
            if any(not (g & a).is_empty and (g & a).get_area() > 0.4 * a.get_area() for a in azules):
                continue
            fig = fitz.Rect(g) if fig is None else fig | g
        bloques: List[List[float]] = []
        abierto = False
        for k in ls:
            dentro = fig is not None and fig.y0 - 14 <= (k.y0 + k.y1) / 2 <= fig.y1 + 22
            rojo = ((k.color >> 16) & 255) > 180 and (k.color & 255) < 90
            if not dentro and (k.azul or rojo or re.match(r"^Tabla\s+\d", k.texto)):
                abierto = False
                continue
            if abierto and k.y0 - bloques[-1][1] < 40:
                bloques[-1][0] = min(bloques[-1][0], k.y0)
                bloques[-1][1] = max(bloques[-1][1], k.y1)
            else:
                bloques.append([k.y0, k.y1])
                abierto = True
        if fig is not None and fig.height > 24:
            bloques.append([fig.y0, fig.y1])
        bloques.sort()
        unidos: List[List[float]] = []
        for a, b in bloques:
            if unidos and a <= unidos[-1][1] + 10:
                unidos[-1][1] = max(unidos[-1][1], b)
            else:
                unidos.append([a, b])
        for a, b in unidos:
            if b - a >= 7:
                tiras.append((i, fitz.Rect(r.x0, a - 2.5, r.x1, b + 3.5)))
        for inf in doc[i].get_text("dict", clip=r)["blocks"]:
            for ln in inf.get("lines", []):
                todo = fold_accents("".join(x["text"] for x in ln["spans"])).lower()
                cita = "figura" in todo or "tabla" in todo
                for sp in ln["spans"]:
                    c = int(sp.get("color") or 0)
                    rr, gg, bb = (c >> 16) & 255, (c >> 8) & 255, c & 255
                    azul = bb > 130 and rr < 110 and gg < 125
                    rojo = rr > 180 and gg < 90 and bb < 90 and not (cita and re.fullmatch(r"\s*\d\.\d{1,2}\s*", sp["text"]))
                    if (azul or rojo) and sp["text"].strip():
                        p.tapas.append((i, fitz.Rect(sp["bbox"]) + (-1, -1, 1, 1)))
    return tiras or p.tiras[:1]


_RE_ENCABEZADO_TAREA = re.compile(
    r"^(?:fi2103\b|fisica\s+general\s+iii|tarea\s+\d+\s*[-–]|solucion\s+tarea\s+\d+|(?:primer|segundo|i+)\s+semestre\s+(?:de\s+)?20\d\d|\d{1,2}$)",
    re.I,
)


def segmenta_general(doc: fitz.Document, tipo: str = "general") -> List[Problema]:
    """PDF cualquiera que suba la persona: Ejercicio N, Problema N, Q001, o listas numeradas."""
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    pats = [
        re.compile(r"^(Q\d{2,4})\b"),
        re.compile(r"^(?:Ejercicio|Exercise|Actividad)\s+(?:No\.?\s*)?(\d{1,3}(?:\.\d{1,2})?)\b", re.I),
        re.compile(r"^(?:Problema|Problem|Pregunta|Question|Item|Ítem)\s+(?:No\.?\s*)?(\d{1,3}(?:\.\d{1,2})?)\b", re.I),
        re.compile(r"^(\d{1,3})\s*[.)](?!\d)\s*\S"),
    ]
    todas: List[List[Dict[str, Any]]] = [[] for _ in pats]
    fines: List[Tuple[int, float]] = []
    for i in range(min(len(doc), MAX_PAGINAS)):
        ls = _lineas(doc[i])
        if tipo == "tarea":
            ls = [k for k in ls if not (k.y0 < 105 and _RE_ENCABEZADO_TAREA.match(fold_accents(k.texto).strip()))]
        cache[i] = (ls, _graficos(doc[i]))
        margen = sorted(k.x0 for k in ls)[max(0, len(ls) // 12)] if ls else 0
        for k in ls:
            if _RE_FIN.match(fold_accents(k.texto)) and len(k.texto) < 60:
                fines.append((i, k.y0))
            for n, pat in enumerate(pats):
                m = pat.match(k.texto)
                if m and k.x0 < margen + 60:
                    todas[n].append({"p": i, "y": k.y0, "num": m.group(1), "clave": m.group(1)})
                    break
    # La lista numerada simple exige secuencia
    sec: List[Dict[str, Any]] = []
    esp = 1
    for a in todas[3]:
        if int(a["num"]) == esp:
            sec.append(a)
            esp += 1
    todas[3] = sec
    for grupo in todas:
        if len(grupo) >= 2:
            vistos: Dict[str, int] = {}
            for a in grupo:
                c = a["clave"]
                vistos[c] = vistos.get(c, 0) + 1
                if vistos[c] > 1:
                    a["clave"] = f"{c}~{vistos[c]}"
            return _arma(doc, tipo, grupo, sorted(fines), cache)
    return []


def segmenta_practica(doc: fitz.Document) -> List[Problema]:
    """Prácticas del II-2026: «Problema N — tema» o secciones I (selección) y II/III (desarrollo) con ítems «N.»."""
    anclas: List[Dict[str, Any]] = []
    fines: List[Tuple[int, float]] = []
    cache: Dict[int, Tuple[List[Linea], List[fitz.Rect]]] = {}
    seccion = ""
    for i in range(len(doc)):
        ls = _lineas(doc[i])
        cache[i] = (ls, _graficos(doc[i]))
        z0, z1 = _zona("practica", doc[i])
        for k in ls:
            if k.y0 < z0 or k.y0 > z1:
                continue
            t = k.texto
            mp = re.match(r"^Problema\s+(\d{1,2})\s*[—–-]\s*(.+)$", t)
            if mp:
                tit = re.sub(r"\s*\(\d+\s*$", "", mp.group(2)).strip()
                anclas.append({"p": i, "y": k.y0, "num": mp.group(1), "clave": mp.group(1), "titulo": tit})
                continue
            if re.match(r"^I\.\s*Selecci", t):
                seccion = "S"
                fines.append((i, k.y0))
                continue
            if re.match(r"^(?:II|III)\.\s*(?:Desarrollo|Desaf)", t):
                seccion = "D"
                fines.append((i, k.y0))  # el título de sección no pertenece al problema anterior
                continue
            m = _RE_NUM.match(t)
            if seccion and m and k.negrita and k.x0 < 100:
                tit = m.group(2).strip() if seccion == "D" else ""
                anclas.append({"p": i, "y": k.y0, "num": m.group(1), "clave": f"{seccion}{m.group(1)}", "titulo": tit})
    return _arma(doc, "practica", anclas, sorted(fines), cache)


SEGMENTADORES = {
    "superexamen": segmenta_superexamen, "recomendados": segmenta_recomendados, "examen": segmenta_examen,
    "examen26": segmenta_examen_latex,
    "folleto": segmenta_folleto, "general": segmenta_general,
    "tarea": lambda d: segmenta_general(d, tipo="tarea"),
    "hoja": lambda d: [], "practica": segmenta_practica,
}


# ---------------------------------------------------------------------- datos subrayados en el recorte
#
# La pizarra subraya un dato buscando su frase entre las palabras del recorte (cajaDeFrase en
# leccion/enunciado.js). Los PDF hechos en LaTeX parten los números: «0,141» sale como «0,» y «141»,
# y «m³» como «m» y «3». Antes de mandar la lección, cada frase se cambia por las palabras exactas del
# PDF para que el subrayado caiga donde debe.

_RE_MARCAS = re.compile("[̀-ͯ]")
_RE_GUIONES = re.compile("[‐-―−]")
_RE_BORDES = re.compile(r"^[(\[{\"'¿¡]+|[)\]}\"'.,;:!?]+$")


def _norma_frase(s: str) -> str:
    """Igual que norma() de enunciado.js: sin tildes, en minúscula y con un solo tipo de guion."""
    s = unicodedata.normalize("NFD", str(s or ""))
    s = _RE_MARCAS.sub("", s).lower()
    s = _RE_GUIONES.sub("-", s)
    return re.sub(r"\s+", " ", s).strip()


def _limpia_palabra(t: str) -> str:
    return _RE_BORDES.sub("", _norma_frase(t))


def caja_de_frase(frase: str, palabras: Sequence[Dict[str, Any]]) -> Optional[int]:
    """Dónde empieza la frase entre las palabras del recorte, con la misma regla que la pizarra."""
    if not frase or not palabras:
        return None
    q = [x for x in (_limpia_palabra(t) for t in _norma_frase(frase).split(" ")) if x]
    if not q:
        return None
    ws = [_limpia_palabra(w.get("t", "")) for w in palabras]
    for i in range(len(ws) - len(q) + 1):
        if all(ws[i + k] == q[k] or (len(q[k]) > 3 and ws[i + k].startswith(q[k])) for k in range(len(q))):
            return i
    return None


def _aplasta(s: str) -> str:
    """Sin espacios y con los superíndices como dígitos comunes: «1,26 m³» y «1,26 m 3» quedan iguales."""
    return re.sub(r"\s+", "", unicodedata.normalize("NFKC", _norma_frase(s)))


def ajustar_frase(frase: str, palabras: Sequence[Dict[str, Any]]) -> str:
    """La frase escrita con las palabras del PDF. Si ya calza o no aparece, se devuelve igual."""
    if not frase or not palabras or caja_de_frase(frase, palabras) is not None:
        return frase
    meta = _RE_BORDES.sub("", _aplasta(frase))
    if not meta:
        return frase
    crudas = [_aplasta(w.get("t", "")) for w in palabras]
    for i in range(len(crudas)):
        junto = ""
        for j in range(i, min(len(crudas), i + 12)):
            # Una palabra que es solo un signo («(», «=») no la puede encontrar la pizarra
            if not _limpia_palabra(palabras[j].get("t", "")):
                break
            junto += crudas[j]
            limpio = _RE_BORDES.sub("", junto)
            if limpio == meta:
                nueva = " ".join(str(w.get("t", "")) for w in palabras[i:j + 1])
                if caja_de_frase(nueva, palabras) is not None:
                    return nueva
                break
            if len(limpio) > len(meta) + 2:
                break
    return frase


def ajustar_datos(j: Dict[str, Any], palabras: Sequence[Dict[str, Any]]) -> int:
    """Pasa todas las frases de subrayado de una lección a las palabras del PDF. Devuelve cuántas cambió."""
    n = 0
    for p in j.get("pasos") or []:
        for t in p.get("tramos") or []:
            for a in t.get("acc") or []:
                if a.get("a") == "dato" and a.get("frase"):
                    nueva = ajustar_frase(a["frase"], palabras)
                    if nueva != a["frase"]:
                        a["frase"] = nueva
                        n += 1
    return n


# ---------------------------------------------------------------------- servicio

def carpetas_por_defecto(raiz: pathlib.Path) -> List[pathlib.Path]:
    cs = [raiz / "private_local" / "tutor_documentos"]
    extra = os.environ.get("TUTOR_DOCUMENTOS", "")
    cs += [pathlib.Path(x) for x in extra.split(os.pathsep) if x.strip()]
    casa = pathlib.Path.home()
    cs += [
        casa / "Downloads",
        casa / "Descargas",
        pathlib.Path("D:/Descargas"),
        pathlib.Path(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Fisica_III"),
        pathlib.Path(r"D:\OneDrive\Desktop\Calculadoras (blanca y gris)\Fisica_III\Documentos II - 2026"),
    ]
    vistos: List[pathlib.Path] = []
    for c in cs:
        try:
            if c.is_dir() and c not in vistos:
                vistos.append(c)
        except OSError:
            continue
    return vistos


class Documentos:
    def __init__(self, raiz: pathlib.Path, carpetas: Optional[Sequence[pathlib.Path]] = None, cache: Optional[pathlib.Path] = None) -> None:
        self.raiz = raiz
        self.carpetas = list(carpetas) if carpetas is not None else carpetas_por_defecto(raiz)
        self.cache = cache
        self._c = threading.RLock()
        self._rutas: Optional[Dict[str, pathlib.Path]] = None
        self._docs: Dict[str, fitz.Document] = {}
        self._probs: Dict[str, List[Problema]] = {}
        self._subidos: Dict[str, Tuple[str, bytes]] = {}
        self._tipo: Dict[str, str] = {k: v[2] for k, v in FUENTES.items()}
        self._titulo: Dict[str, str] = {k: v[1] for k, v in FUENTES.items()}
        self._cruce: Optional[Dict[Tuple[str, str], str]] = None
        self._formulario_png: Optional[bytes] = None
        self._formularios_parcial_png: Dict[int, bytes] = {}

    # ------------------------------------------------------------ archivos

    def rutas(self) -> Dict[str, pathlib.Path]:
        with self._c:
            if self._rutas is None:
                indice: Dict[str, pathlib.Path] = {}
                subindice: Dict[str, pathlib.Path] = {}
                for c in self.carpetas:
                    try:
                        for f in c.iterdir():
                            if f.suffix.lower() in (".pdf", ".zip") and f.is_file():
                                indice.setdefault(_norma_archivo(f.name), f)
                    except OSError:
                        continue
                    # Índice por subruta normalizada solo dentro del repositorio del curso (rápido, sin recorrer Descargas)
                    if _norma_archivo(c.name) in ("fisica_iii", "tutor_documentos"):
                        try:
                            for f in c.rglob("*.pdf"):
                                if f.is_file():
                                    rel = _norma_archivo(f.relative_to(c).as_posix())
                                    subindice.setdefault(rel, f)
                        except OSError:
                            continue
                self._rutas = {}
                for id, (pats, _, _) in FUENTES.items():
                    for p in pats:
                        np = _norma_archivo(p)
                        if "/" in np and np in subindice:
                            self._rutas[id] = subindice[np]
                            break
                        if np in indice:
                            self._rutas[id] = indice[np]
                            break
            return self._rutas

    @staticmethod
    def _pdf_de_zip(ruta: pathlib.Path) -> bytes:
        with zipfile.ZipFile(ruta) as z:
            total = 0
            for inf in z.infolist():
                if inf.is_dir() or not inf.filename.lower().endswith(".pdf"):
                    continue
                total += inf.file_size
                if inf.file_size > MAX_PDF or total > MAX_ZIP_SUMA or (inf.compress_size and inf.file_size / inf.compress_size > 200):
                    raise ValueError("El ZIP es demasiado grande")
                with z.open(inf) as fh:
                    return fh.read(MAX_PDF + 1)[:MAX_PDF]
        raise ValueError("El ZIP no trae ningún PDF")

    def _abre(self, id: str) -> fitz.Document:
        with self._c:
            if id in self._docs:
                return self._docs[id]
            if id in self._subidos:
                d = fitz.open(stream=self._subidos[id][1], filetype="pdf")
            else:
                ruta = self.rutas().get(id)
                if ruta is None:
                    raise KeyError(id)
                if ruta.suffix.lower() == ".zip":
                    d = fitz.open(stream=self._pdf_de_zip(ruta), filetype="pdf")
                else:
                    d = fitz.open(str(ruta))
            self._docs[id] = d
            return d

    def problemas(self, id: str) -> List[Problema]:
        with self._c:
            if id not in self._probs:
                d = self._abre(id)
                tipo = self._tipo.get(id, "general")
                probs = SEGMENTADORES[tipo](d)
                if not probs and tipo == "examen":
                    probs = segmenta_general(d)
                self._probs[id] = probs
            return self._probs[id]

    def problema(self, id: str, clave: str) -> Problema:
        for p in self.problemas(id):
            if p.clave.lower() == str(clave).lower():
                return p
        raise KeyError(f"{id} {clave}")

    def titulo(self, id: str) -> str:
        return self._titulo.get(id, id)

    def etiqueta(self, id: str, clave: str) -> str:
        t = self._tipo.get(id, "general")
        if t == "superexamen":
            return f"Superexamen · {clave}"
        if t == "recomendados":
            return f"{self.titulo(id)} · ejercicio {clave}"
        if t == "folleto":
            return f"Folleto · sección {clave.split('-')[0]}, ejercicio {clave.split('-')[-1]}"
        return f"{self.titulo(id)} · {'problema' if t == 'examen' else 'ejercicio'} {clave.split('~')[0]}"

    def lista(self) -> List[Dict[str, Any]]:
        out = []
        for id in list(self.rutas()) + list(self._subidos):
            out.append({"doc": id, "titulo": self.titulo(id), "tipo": self._tipo.get(id, "general")})
        return out

    def resumen(self, id: str, leccion_de=None) -> Dict[str, Any]:
        ps = []
        for p in self.problemas(id):
            ps.append({
                "clave": p.clave, "num": p.num, "titulo": p.titulo, "pagina": p.pagina,
                "leccion": (leccion_de(id, p.clave) if leccion_de else None) or self.leccion_de(id, p.clave),
            })
        return {"doc": id, "titulo": self.titulo(id), "paginas": len(self._abre(id)), "problemas": ps}

    # ------------------------------------------------------------ recortes

    def _clave_cache(self, id: str, p: Problema) -> str:
        base = f"{id}|{p.clave}|{ZOOM}|sin-resaltador-v2|{[(i, [round(v, 1) for v in r]) for i, r in p.tiras]}"
        return hashlib.sha256(base.encode()).hexdigest()[:20]

    def png(self, id: str, clave: str) -> bytes:
        p = self.problema(id, clave)
        if self.cache is not None:
            f = self.cache / f"{self._clave_cache(id, p)}.png"
            if f.is_file():
                return f.read_bytes()
        b = self._pinta(id, p)
        if self.cache is not None:
            try:
                self.cache.mkdir(parents=True, exist_ok=True)
                tmp = f.with_suffix(".parcial")
                tmp.write_bytes(b)
                tmp.replace(f)
            except OSError:
                pass
        return b

    def hoja_formulario_png(self) -> bytes:
        """Renderiza la hoja oficial del formulario II-2026 recortada exactamente a CAJAS_CELDAS."""
        with self._c:
            if self._formulario_png is not None:
                return self._formulario_png
            cache_f = self.cache / "formulario_ii_2026_hoja_v1.png" if self.cache is not None else None
            if cache_f is not None and cache_f.is_file():
                self._formulario_png = cache_f.read_bytes()
                return self._formulario_png

            from PIL import Image, ImageDraw
            from .fisica3.formulario_ii_2026 import CAJAS_CELDAS, CROP_NORM, HOJA_ASPECTO, NOMBRES_CELDAS

            b: bytes
            try:
                doc = self._abre("formulario")
                page = doc[0]
                r = page.rect
                x0, y0, x1, y1 = CROP_NORM
                clip = fitz.Rect(r.width * x0, r.height * y0, r.width * x1, r.height * y1)
                px = page.get_pixmap(matrix=fitz.Matrix(200.0 / 72.0, 200.0 / 72.0), clip=clip, alpha=False)
                im = Image.frombytes("RGB", (px.width, px.height), px.samples)
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                b = buf.getvalue()
            except (KeyError, OSError, RuntimeError):
                # Respaldo limpio si el PDF no está en la carpeta de descargas
                w, h = HOJA_ASPECTO
                im = Image.new("RGB", (w, h), "white")
                dr = ImageDraw.Draw(im)
                dr.text((42, 24), "Ecuaciones y constantes importantes · FI2103 II-2026", fill=(22, 33, 63))
                for cid, (cx0, cy0, cx1, cy1) in CAJAS_CELDAS.items():
                    box = [int(cx0 * w), int(cy0 * h), int(cx1 * w), int(cy1 * h)]
                    dr.rectangle(box, outline=(180, 190, 205), width=2)
                    dr.text((box[0] + 10, box[1] + 14), f"{cid}: {NOMBRES_CELDAS.get(cid, '')}", fill=(35, 45, 70))
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                b = buf.getvalue()

            self._formulario_png = b
            if cache_f is not None:
                try:
                    cache_f.parent.mkdir(parents=True, exist_ok=True)
                    tmp = cache_f.with_suffix(".parcial")
                    tmp.write_bytes(b)
                    tmp.replace(cache_f)
                except OSError:
                    pass
            return b

    def hoja_formulario_parcial_png(self, parcial: int) -> bytes:
        """Renderiza la hoja oficial del Parcial 1, 2 o 3 recortada a sus celdas."""
        if parcial == 3:
            return self.hoja_formulario_png()
        if parcial not in (1, 2):
            raise KeyError(f"parcial {parcial}")
        with self._c:
            if parcial in self._formularios_parcial_png:
                return self._formularios_parcial_png[parcial]
            cache_f = self.cache / f"formulario_ii_2026_p{parcial}_v1.png" if self.cache is not None else None
            if cache_f is not None and cache_f.is_file():
                data = cache_f.read_bytes()
                self._formularios_parcial_png[parcial] = data
                return data

            from PIL import Image, ImageDraw
            from .fisica3.formularios_p1_p2 import _HOJAS_PARCIAL, catalogo_formulario_parcial

            info = _HOJAS_PARCIAL[parcial]
            doc_id = f"formulario_p{parcial}"
            b: bytes
            try:
                doc = self._abre(doc_id)
                page = doc[0]
                r = page.rect
                x0, y0, x1, y1 = info["crop_norm"]
                clip = fitz.Rect(r.width * x0, r.height * y0, r.width * x1, r.height * y1)
                px = page.get_pixmap(matrix=fitz.Matrix(200.0 / 72.0, 200.0 / 72.0), clip=clip, alpha=False)
                im = Image.frombytes("RGB", (px.width, px.height), px.samples)
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                b = buf.getvalue()
            except (KeyError, OSError, RuntimeError):
                w, h = info["aspecto"]
                im = Image.new("RGB", (w, h), "white")
                dr = ImageDraw.Draw(im)
                dr.text((36, 20), info["titulo"], fill=(22, 33, 63))
                for c in catalogo_formulario_parcial(parcial)["celdas"]:
                    cx0, cy0, cx1, cy1 = c["caja"]
                    box = [int(cx0 * w), int(cy0 * h), int(cx1 * w), int(cy1 * h)]
                    dr.rectangle(box, outline=(180, 190, 205), width=2)
                    dr.text((box[0] + 8, box[1] + 10), f"{c['id']}: {c['nombre']}", fill=(35, 45, 70))
                buf = io.BytesIO()
                im.save(buf, "PNG", optimize=True)
                b = buf.getvalue()

            self._formularios_parcial_png[parcial] = b
            if cache_f is not None:
                try:
                    cache_f.parent.mkdir(parents=True, exist_ok=True)
                    tmp = cache_f.with_suffix(".parcial")
                    tmp.write_bytes(b)
                    tmp.replace(cache_f)
                except OSError:
                    pass
            return b

    def _pinta(self, id: str, p: Problema) -> bytes:
        from PIL import Image, ImageDraw

        with self._c:
            d = self._abre(id)
            ims = []
            for i, r in p.tiras:
                z = min(ZOOM, MAX_ANCHO / max(1.0, r.width))
                px = d[i].get_pixmap(matrix=fitz.Matrix(z, z), clip=r, alpha=False)
                im = Image.frombytes("RGB", (px.width, px.height), px.samples)
                lienzo = ImageDraw.Draw(im)
                for t in [t for j, t in p.tapas if j == i] + self._numeros_de_pagina(d[i]):
                    c = t & r
                    if not c.is_empty:
                        lienzo.rectangle([(c.x0 - r.x0) * z, (c.y0 - r.y0) * z, (c.x1 - r.x0) * z, (c.y1 - r.y0) * z], fill="white")
                for t, fondo in self._resaltes(d[i]):
                    c = (t + (-0.9, -0.9, 0.9, 0.9)) & r  # el borde suavizado también se limpia
                    if not c.is_empty:
                        self._blanquear(im, [(c.x0 - r.x0) * z, (c.y0 - r.y0) * z, (c.x1 - r.x0) * z, (c.y1 - r.y0) * z], fondo)
                ims.append(im)
        w = max(im.width for im in ims)
        sep = 10
        h = sum(im.height for im in ims) + sep * (len(ims) - 1)
        tela = Image.new("RGB", (w, h), "white")
        y = 0
        for im in ims:
            tela.paste(im, (0, y))
            y += im.height + sep
        buf = io.BytesIO()
        tela.save(buf, "PNG", optimize=True)
        return buf.getvalue()

    @staticmethod
    def _resaltes(page: fitz.Page) -> List[Tuple[fitz.Rect, Tuple[int, int, int]]]:
        """Rectángulos de resaltador (amarillo, cian o verde puros) que los solucionarios ponen sobre la respuesta."""
        out = []
        try:
            dib = page.get_drawings()
        except Exception:
            return out
        for d in dib:
            f = d.get("fill")
            if not f or len(f) != 3:
                continue
            altos = sorted(f)
            if altos[0] < 0.35 and altos[1] > 0.8 and altos[2] > 0.85:
                r = fitz.Rect(d["rect"])
                if 6 < r.height < 60 and r.width > 8:
                    out.append((r, tuple(int(round(x * 255)) for x in f)))
        return out

    @staticmethod
    def _blanquear(im: Any, caja: List[float], fondo: Tuple[int, int, int]) -> None:
        """Vuelve blanco el fondo de color de una caja y deja el texto negro, con su suavizado."""
        x0, y0, x1, y1 = (int(round(v)) for v in caja)
        x0, y0 = max(0, x0), max(0, y0)
        x1, y1 = min(im.width, x1), min(im.height, y1)
        if x1 - x0 < 2 or y1 - y0 < 2:
            return
        zona = im.crop((x0, y0, x1, y1))
        lum_f = max(1.0, 0.299 * fondo[0] + 0.587 * fondo[1] + 0.114 * fondo[2])
        datos = []
        for r, g, b in zona.getdata():
            lum = 0.299 * r + 0.587 * g + 0.114 * b
            v = max(0, min(255, int(round(255.0 * lum / lum_f))))
            datos.append((v, v, v))
        zona.putdata(datos)
        im.paste(zona, (x0, y0))

    @staticmethod
    def _numeros_de_pagina(page: fitz.Page) -> List[fitz.Rect]:
        out = []
        for x0, y0, x1, y1, t, *_ in page.get_text("words"):
            if re.fullmatch(r"\d{1,3}", t) and y0 > page.rect.height - 78 and page.rect.height < 800 and x0 > page.rect.width - 80:
                out.append(fitz.Rect(x0 - 1, y0 - 1, x1 + 1, y1 + 1))
        return out

    def resaltado(self, id: str, clave: str) -> bool:
        """¿El recorte trae la respuesta marcada con resaltador? (varios exámenes resueltos lo hacen)."""
        from PIL import Image

        p = self.problema(id, clave)
        if "_resaltado" not in p.extra:
            with self._c:
                d = self._abre(id)
                p.extra["_resaltado"] = any(not (t & r).is_empty for i, r in p.tiras for t, _ in self._resaltes(d[i]))
        return bool(p.extra["_resaltado"])

    def recorte(self, id: str, clave: str) -> Dict[str, Any]:
        """Datos del recorte para la pizarra: dirección, palabras con su caja y pie."""
        p = self.problema(id, clave)
        with self._c:
            d = self._abre(id)
            anchos = [r.width for _, r in p.tiras]
            zs = [min(ZOOM, MAX_ANCHO / max(1.0, w)) for w in anchos]
            W = max(w * z for w, z in zip(anchos, zs))
            sep = 10
            H = sum(r.height * z for (_, r), z in zip(p.tiras, zs)) + sep * (len(p.tiras) - 1)
            pal = []
            y = 0.0
            for (i, r), z in zip(p.tiras, zs):
                for x0, y0, x1, y1, t, *_ in d[i].get_text("words", clip=r):
                    if not t.strip():
                        continue
                    pal.append({
                        "t": t, "x": round((x0 - r.x0) * z / W, 4), "y": round((y + (y0 - r.y0) * z) / H, 4),
                        "w": round((x1 - x0) * z / W, 4), "h": round((y1 - y0) * z / H, 4),
                    })
                y += r.height * z + sep
        return {
            "url": f"/api/documentos/{id}/recorte/{clave}.png?v={self._clave_cache(id, p)[:8]}",
            "palabras": pal[:420], "etiqueta": self.etiqueta(id, clave), "pie": f"{self.titulo(id)}, página {p.pagina}",
            "ancho": round(W), "alto": round(H),
        }

    # ------------------------------------------------------------ cruces con el banco

    def cruce(self) -> Dict[Tuple[str, str], str]:
        """(examen, pregunta) -> Q###, leído del propio Superexamen."""
        with self._c:
            if self._cruce is None:
                self._cruce = {}
                try:
                    for p in self.problemas("superexamen"):
                        e, q = p.extra.get("examen"), str(p.extra.get("preg") or "")
                        if e and q:
                            self._cruce.setdefault((e, q.lower()), p.clave)
                except KeyError:
                    pass
            return self._cruce

    def leccion_de(self, id: str, clave: str) -> Optional[str]:
        from .fisica3 import registro

        if id == "superexamen":
            return clave if registro.existe(clave) else None
        if id == "recomendados":
            return f"E{clave}" if registro.existe(f"E{clave}") else None
        if id in PRACTICAS:
            lid = f"{PRACTICAS[id]}-{clave}"
            return lid if registro.existe(lid) else None
        if id in PARCIALES_REPO26:
            lid = f"{PARCIALES_REPO26[id]}-{clave}"
            return lid if registro.existe(lid) else None
        if id in TAREAS_Y_REC:
            lid = f"{TAREAS_Y_REC[id]}-{clave}"
            return lid if registro.existe(lid) else None
        rp = REPOSICION_MAPA.get((id, str(clave)))
        if rp and registro.existe(rp):
            return rp
        q = self.cruce().get((id, str(clave).lower()))
        return q if q and registro.existe(q) else None

    def recorte_de_leccion(self, lec: Any) -> Optional[Dict[str, Any]]:
        f = lec.fuente or {}
        doc = f.get("doc")
        try:
            if re.fullmatch(r"Q\d{3}", lec.id):
                p = self.problema("superexamen", lec.id)
                e, q = p.extra.get("examen"), str(p.extra.get("preg") or "")
                orig = f.get("original")  # (examen, pregunta) cuando se fuerza el original
                if orig:
                    e, q = str(orig[0]), str(orig[1])
                # El original está mejor compuesto y trae la figura correcta. Si el solucionario marca la
                # respuesta con resaltador, el recorte sale con ese fondo borrado (ver _blanquear).
                if e in self.rutas() and q:
                    try:
                        if True:
                            rec = self.recorte(e, q)
                            rec["pie"] = f"{lec.id} del Superexamen · {rec['etiqueta']}"
                            rec["etiqueta"] = f"{lec.id} · {self.titulo(e)}"
                            return rec
                    except KeyError:
                        pass
                rec = self.recorte("superexamen", lec.id)
                if e:
                    rec["pie"] = f"{rec['pie']} · sale en {self.titulo(e)}, pregunta {q}"
                return rec
            if re.fullmatch(r"E\d{1,2}", lec.id):
                return self.recorte("recomendados", lec.id[1:])
            if doc and f.get("num") is not None:
                return self.recorte(str(doc), str(f["num"]))
        except (KeyError, ValueError, RuntimeError):
            return None
        return None

    # ------------------------------------------------------------ subir y buscar

    def subir(self, nombre: str, datos: bytes) -> Dict[str, Any]:
        if len(datos) > MAX_PDF:
            raise ValueError("El PDF pasa de 60 MB")
        if not datos[:1024].lstrip().startswith(b"%PDF"):
            raise ValueError("El archivo no es un PDF")
        # ¿Es uno de los documentos conocidos?
        for id, ruta in self.rutas().items():
            if _norma_archivo(ruta.name) == _norma_archivo(nombre):
                return self.resumen(id)
        id = "doc" + hashlib.sha256(datos).hexdigest()[:10]
        with self._c:
            if id not in self._subidos:
                d = fitz.open(stream=datos, filetype="pdf")
                if d.needs_pass:
                    raise ValueError("El PDF tiene contraseña")
                if len(d) > MAX_PAGINAS:
                    raise ValueError(f"El PDF pasa de {MAX_PAGINAS} páginas")
                if len(self._subidos) >= 8:
                    viejo = next(iter(self._subidos))
                    self._subidos.pop(viejo)
                    self._docs.pop(viejo, None)
                    self._probs.pop(viejo, None)
                self._subidos[id] = (nombre[:120], datos)
                self._docs[id] = d
                self._tipo[id] = "general"
                self._titulo[id] = pathlib.PurePath(nombre).stem[:80]
        return self.resumen(id)

    def texto_solucion(self, id: str, clave: str, limite: int = 6000) -> str:
        """Solución oficial en texto (guía para la IA; nunca se muestra ni se copia)."""
        p = self.problema(id, clave)
        if not p.solucion:
            return ""
        with self._c:
            return _texto(self._abre(id), p.solucion)[:limite]

    def resolver(self, q: str) -> Optional[Dict[str, Any]]:
        """'problema 9 del ordinario I-2024' -> {doc, clave, …} o None."""
        t = fold_accents(q).lower()
        m = re.search(r"\bq\s*0*(\d{1,3})\b", t)
        if m:
            return self._hallado("superexamen", f"Q{int(m.group(1)):03d}")
        # Prácticas del II-2026: «PR2-D4», «práctica 2 desarrollo 4», «práctica del parcial problema 3»
        m = re.search(r"\bpr\s*([12])\s*-?\s*([sd]?)\s*(\d{1,2})\b", t)
        if m:
            return self._hallado(f"26-II-P{m.group(1)}", f"{m.group(2).upper()}{m.group(3)}")
        if "practica" in t:
            num = re.search(r"(?:problema|pregunta|ejercicio|item|desarrollo|desafio|seleccion|numero)\s*#?\s*(\d{1,2})", t) or re.search(r"\b(\d{1,2})\b(?!.*\b\d{1,2}\b)", re.sub(r"20\d\d|\bparcial\s*3\b|\bpractica\s*[12]\b", " ", t))
            if num:
                if re.search(r"seleccion|unica", t):
                    return self._hallado("26-II-P2", f"S{num.group(1)}")
                if re.search(r"desarrollo|desafio|tercer examen|fiii|practica\s*2|segunda practica", t):
                    return self._hallado("26-II-P2", f"D{num.group(1)}")
                return self._hallado("26-II-P1", num.group(1))
        doc = None
        if re.search(r"recomendad", t):
            doc = "recomendados"
        elif "superexamen" in t:
            doc = "superexamen"
        elif "folleto" in t:
            doc = "26-F"
        else:
            sem = re.search(r"\b(ii|i|1|2)\s*[- ]?\s*(?:semestre)?\s*(?:de|del)?\s*[- ]?\s*(20\d\d)\b", t) or re.search(r"\b(20\d\d)\s*[- ]\s*(ii|i|1|2)\b", t)
            if sem:
                a, b = sem.group(1), sem.group(2)
                if a.startswith("20"):
                    a, b = b, a
                s = {"1": "I", "2": "II"}.get(a, a.upper())
                clase = "E" if "extra" in t else "R" if "repos" in t else "O"
                cand = f"{b[2:]}-{s}-{clase}"
                if cand in self.rutas():
                    doc = cand
        if doc is None:
            return None
        sin = re.sub(r"20\d\d", " ", t)
        sin = re.sub(r"\b(iii|ii|i|3er|tercer)\b", " ", sin)
        if doc == "26-F":
            m = re.search(r"\b(\d\.\d)\b.*?\b(\d{1,2})\b", sin)
            if m:
                return self._hallado(doc, f"{m.group(1)}-{m.group(2)}")
        m = re.search(r"(?:problema|pregunta|ejercicio|item|numero|n[o°º]\.?)\s*#?\s*(\d{1,3})", sin) or re.search(r"\b(\d{1,3})\b", sin)
        if not m:
            return None
        clave = m.group(1)
        if doc == "superexamen":
            clave = f"Q{int(clave):03d}"
        return self._hallado(doc, clave)

    def _hallado(self, doc: str, clave: str) -> Optional[Dict[str, Any]]:
        try:
            p = self.problema(doc, clave)
        except KeyError:
            return None
        lec = self.leccion_de(doc, p.clave)
        out: Dict[str, Any] = {"doc": doc, "clave": p.clave, "etiqueta": self.etiqueta(doc, p.clave), "texto": p.texto[:1500], "leccion": lec}
        if not lec:
            out["recorte"] = self.recorte(doc, p.clave)
        return out

    # ------------------------------------------------------------ reconocer texto pegado

    def huella(self, texto: str) -> set:
        t = re.sub(r"[^a-z0-9 ]", " ", fold_accents(texto).lower())
        pal = [w for w in t.split() if len(w) > 2 or w.isdigit()]
        return {" ".join(pal[i : i + 3]) for i in range(max(0, len(pal) - 2))}

    def parecidos(self, texto: str, minimo: float = 0.28) -> List[Dict[str, Any]]:
        """Problemas conocidos cuyo texto se parece (para imágenes pegadas)."""
        h = self.huella(texto)
        if len(h) < 4:
            return []
        out = []
        for id in self.rutas():
            if self._tipo[id] == "hoja":
                continue
            try:
                ps = self.problemas(id)
            except Exception:
                continue
            for p in ps:
                g = p.extra.setdefault("_huella", self.huella(p.texto))
                if not g:
                    continue
                inter = len(h & g)
                s = inter / max(1, min(len(h), len(g)))
                if s >= minimo and inter >= 4:
                    out.append({"doc": id, "clave": p.clave, "puntos": round(s, 3), "leccion": self.leccion_de(id, p.clave),
                                "etiqueta": self.etiqueta(id, p.clave), "texto": p.texto[:400]})
        out.sort(key=lambda x: (-x["puntos"], x["leccion"] is None))
        return out[:6]
