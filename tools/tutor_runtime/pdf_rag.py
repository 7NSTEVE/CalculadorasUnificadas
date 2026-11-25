"""Local PDF Rendering, Problem Indexing, Smart Visual Cropping, Duplicate Auditing & Hybrid FTS5 RAG (PyMuPDF)."""

from __future__ import annotations

import base64
import collections
import hashlib
import json
import pathlib
import re
import sqlite3
import unicodedata
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

import fitz  # PyMuPDF


_STOPWORDS_ES = {
    "de", "la", "el", "en", "y", "a", "los", "las", "un", "una", "unos", "unas",
    "del", "al", "se", "por", "para", "con", "su", "sus", "que", "es", "son",
    "como", "si", "no", "mas", "o", "u", "lo", "le", "les", "este", "esta",
    "estos", "estas", "ese", "esa", "cual", "donde", "cuando", "entre", "sobre",
    "desde", "hasta", "determine", "calcule", "encuentre", "considere", "muestra",
    "figura", "siguiente", "partir", "informacion", "anterior", "puntos", "valor",
    "inciso", "incisos", "pregunta", "problema", "ejercicio", "espacio", "apuntes",
}

_Q_HEADER_RE = re.compile(r"(?m)^\s*([Qq]\d{2,4})\b")
_GENERIC_PROB_RE = re.compile(
    r"(?mi)^\s*(?:Problema|Ejercicio|Pregunta)\s*#?\s*(\d{1,4})\b"
)


def fold_accents(text: str) -> str:
    """Lowercase and strip diacritics for accent-insensitive Spanish search."""
    if not text:
        return ""
    nfd = unicodedata.normalize("NFD", text.lower())
    return "".join(ch for ch in nfd if unicodedata.category(ch) != "Mn")


def normalize_problem_id(raw: str) -> str:
    """Normalize 'q57', 'Q057', 'problema 122' into canonical form ('Q057', 'Q122', etc.)."""
    s = (raw or "").strip()
    m = re.search(r"\b([qQ])\s*0*(\d{1,4})\b", s)
    if m:
        num = int(m.group(2))
        return f"Q{num:03d}"
    m2 = re.search(r"\b(?:problema|ejercicio|pregunta|p)\s*#?\s*0*(\d{1,4})\b", s, re.I)
    if m2:
        num = int(m2.group(1))
        return f"P{num:03d}"
    m3 = re.fullmatch(r"0*(\d{1,4})", s)
    if m3:
        return f"{int(m3.group(1)):03d}"
    return s.upper()


def _statement_fingerprint_tokens(text: str) -> List[str]:
    """Normalize a problem statement by stripping numbers/labels so numeric-variant clones match."""
    folded = fold_accents(text)
    # Remove metadata labels and numeric literals
    folded = re.sub(r"\[\s*\d+\s*puntos?\s*\]", " ", folded)
    folded = re.sub(r"\b\d+(?:[.,]\d+)?(?:\s*[x×]\s*10\^?[-+]?\d+)?\b", " ", folded)
    folded = re.sub(r"[^a-z\s]", " ", folded)
    toks = [t for t in folded.split() if len(t) >= 3 and t not in _STOPWORDS_ES]
    return toks


def _shingle_set(tokens: List[str]) -> set:
    if not tokens:
        return set()
    s = set(tokens)
    for i in range(len(tokens) - 1):
        s.add(f"{tokens[i]}_{tokens[i + 1]}")
    return s


def _normalize_match_token(raw: str) -> str:
    """Normalize a PDF word or query token for robust phrase/unit box matching."""
    f = fold_accents(raw or "").strip()
    # Normalize decimal commas like '1,50' -> '1.50'
    f = re.sub(r"(\d),(\d)", r"\1.\2", f)
    f = f.strip(".,;:()[]{}\"'¡!¿?")
    return f


def _tokens_match(query_tok: str, word_tok: str) -> bool:
    if not query_tok or not word_tok:
        return False
    if query_tok == word_tok:
        return True
    # Numeric equivalence: '1.5' matches '1.50', '3' matches '3.00'
    try:
        qf = float(query_tok)
        wf = float(word_tok)
        if abs(qf - wf) <= 1e-6:
            return True
    except ValueError:
        pass
    # Prefix tolerance for stemmed Spanish words (>=5 chars) or attached punctuation
    if len(query_tok) >= 5 and (word_tok.startswith(query_tok[:5]) or query_tok.startswith(word_tok[:5])):
        return True
    return False


def find_phrase_norm_box(
    phrase: str,
    crop_words: List[Dict[str, Any]],
) -> Optional[Dict[str, Any]]:
    """Find the exact pixel-accurate normalized bounding box {x, y, w, h} of a phrase inside a cropped PDF problem."""
    if not phrase or not crop_words:
        return None

    # Split query into clean tokens keeping decimal numbers and units separate (e.g. '1.50atm' -> ['1.50', 'atm'])
    raw_q = re.sub(r"(\d),(\d)", r"\1.\2", fold_accents(phrase))
    q_tokens = [
        _normalize_match_token(t)
        for t in re.findall(r"\d+(?:\.\d+)?|[a-z°%μωδπ]+", raw_q)
    ]
    q_tokens = [t for t in q_tokens if t]
    if not q_tokens:
        return None

    # Build expanded token stream from crop_words (splitting glued number+unit like '1.50atm' or '(3.50' cleanly)
    expanded: List[Tuple[str, Dict[str, Any]]] = []
    for w in crop_words:
        wt = str(w.get("t") or "")
        wf = re.sub(r"(\d),(\d)", r"\1.\2", fold_accents(wt))
        parts = re.findall(r"\d+(?:\.\d+)?|[a-z°%μωδπ]+", wf)
        if not parts:
            norm = _normalize_match_token(wt)
            if norm:
                expanded.append((norm, w))
        else:
            for p in parts:
                expanded.append((p, w))

    if not expanded:
        return None

    def _merge_boxes(slice_items: List[Tuple[str, Dict[str, Any]]]) -> Dict[str, Any]:
        x0 = min(float(it[1]["x"]) for it in slice_items)
        y0 = min(float(it[1]["y"]) for it in slice_items)
        x1 = max(float(it[1]["x"]) + float(it[1]["w"]) for it in slice_items)
        y1 = max(float(it[1]["y"]) + float(it[1]["h"]) for it in slice_items)
        pad_x = 0.004
        pad_y = 0.004
        rx0 = max(0.0, x0 - pad_x)
        ry0 = max(0.0, y0 - pad_y)
        rx1 = min(1.0, x1 + pad_x)
        ry1 = min(1.0, y1 + pad_y)
        seen_words = []
        for _, wd in slice_items:
            wt = str(wd.get("t") or "")
            if wt and (not seen_words or seen_words[-1] != wt):
                seen_words.append(wt)
        return {
            "x": round(rx0, 4),
            "y": round(ry0, 4),
            "w": round(max(0.015, rx1 - rx0), 4),
            "h": round(max(0.015, ry1 - ry0), 4),
            "matched_text": " ".join(seen_words),
        }

    # Pass 1: Exact contiguous multi-token match
    qn = len(q_tokens)
    for i in range(len(expanded) - qn + 1):
        if all(_tokens_match(q_tokens[k], expanded[i + k][0]) for k in range(qn)):
            return _merge_boxes(expanded[i : i + qn])

    # Pass 2: Allow 1 intermediate filler word (e.g. 'presión 1.50 atm' matching 'presión de 1.50 atm')
    if qn >= 2:
        for i in range(len(expanded)):
            if not _tokens_match(q_tokens[0], expanded[i][0]):
                continue
            matched_idx = [i]
            cur = i
            ok = True
            for k in range(1, qn):
                found_next = -1
                for j in range(cur + 1, min(len(expanded), cur + 4)):
                    if _tokens_match(q_tokens[k], expanded[j][0]):
                        found_next = j
                        break
                if found_next < 0:
                    ok = False
                    break
                matched_idx.append(found_next)
                cur = found_next
            if ok:
                return _merge_boxes(expanded[matched_idx[0] : matched_idx[-1] + 1])

    # Pass 3: Single strongest numeric token match (e.g. '1.50' when unit differs in phrasing)
    num_tokens = [t for t in q_tokens if re.fullmatch(r"\d+(?:\.\d+)?", t)]
    for nt in num_tokens:
        for i, (et, _) in enumerate(expanded):
            if _tokens_match(nt, et):
                # Include immediate trailing unit word if present
                end_i = i + 2 if (i + 1 < len(expanded) and re.fullmatch(r"[a-z°%μωδπ]{1,4}", expanded[i + 1][0])) else i + 1
                return _merge_boxes(expanded[i:end_i])

    # Pass 4: Single keyword fallback
    for qt in sorted(q_tokens, key=len, reverse=True):
        if len(qt) < 3:
            continue
        for i, (et, _) in enumerate(expanded):
            if _tokens_match(qt, et):
                return _merge_boxes([expanded[i]])

    return None


def _stitch_pixmaps_vertical(pixmaps: List[fitz.Pixmap], gap_px: int = 12) -> bytes:
    """Combine multiple PyMuPDF RGB pixmaps vertically into one PNG without external dependencies."""
    if not pixmaps:
        return b""
    if len(pixmaps) == 1:
        return pixmaps[0].tobytes("png")
    total_w = max(p.width for p in pixmaps)
    total_h = sum(p.height for p in pixmaps) + gap_px * (len(pixmaps) - 1)
    canvas = fitz.Pixmap(fitz.csRGB, fitz.IRect(0, 0, total_w, total_h), False)
    canvas.set_rect(canvas.irect, (255, 255, 255))
    y = 0
    for p in pixmaps:
        src = p if p.colorspace == fitz.csRGB and not p.alpha else fitz.Pixmap(fitz.csRGB, p)
        dest_rect = fitz.IRect(0, y, src.width, y + src.height)
        src.set_origin(0, y)
        canvas.copy(src, dest_rect)
        y += src.height + gap_px
    return canvas.tobytes("png")


@dataclass
class IndexedPdfDoc:
    doc_id: str
    filename: str
    path: str
    page_count: int
    problem_count: int = 0


class LocalPdfRagStore:
    """Indexes local PDFs (formularios, bancos de examen, guías TEC) with SQLite FTS5,
    automatic problem segmentation, smart visual problem+figure cropping, and duplicate auditing.
    """

    def __init__(self, db_path: str = ":memory:") -> None:
        self._conn = sqlite3.connect(db_path, check_same_thread=False)
        self._conn.execute("PRAGMA journal_mode=WAL;")
        self._conn.execute(
            """
            CREATE TABLE IF NOT EXISTS pdf_docs (
                doc_id TEXT PRIMARY KEY,
                filename TEXT NOT NULL,
                file_path TEXT NOT NULL,
                page_count INTEGER NOT NULL,
                problem_count INTEGER NOT NULL DEFAULT 0
            )
            """
        )
        self._conn.execute(
            """
            CREATE VIRTUAL TABLE IF NOT EXISTS pdf_pages_fts
            USING fts5(doc_id, filename, page_num UNINDEXED, content)
            """
        )
        self._conn.execute(
            """
            CREATE TABLE IF NOT EXISTS pdf_problems (
                doc_id TEXT NOT NULL,
                problem_id TEXT NOT NULL,
                filename TEXT NOT NULL,
                page_start INTEGER NOT NULL,
                page_end INTEGER NOT NULL,
                level TEXT NOT NULL DEFAULT '',
                source_ref TEXT NOT NULL DEFAULT '',
                family TEXT NOT NULL DEFAULT '',
                topic_code TEXT NOT NULL DEFAULT '',
                subtema TEXT NOT NULL DEFAULT '',
                procedimiento TEXT NOT NULL DEFAULT '',
                body_text TEXT NOT NULL,
                has_figure INTEGER NOT NULL DEFAULT 0,
                figure_count INTEGER NOT NULL DEFAULT 0,
                crop_rects_json TEXT NOT NULL DEFAULT '[]',
                figure_rects_json TEXT NOT NULL DEFAULT '[]',
                PRIMARY KEY (doc_id, problem_id)
            )
            """
        )
        self._conn.execute(
            """
            CREATE VIRTUAL TABLE IF NOT EXISTS pdf_problems_fts
            USING fts5(doc_id UNINDEXED, problem_id, folded_meta, folded_body)
            """
        )
        self._conn.commit()
        self._docs: Dict[str, IndexedPdfDoc] = {}
        self._raw_bytes: Dict[str, bytes] = {}
        self._doc_order: List[str] = []

    def close(self) -> None:
        self._conn.close()

    def latest_doc_id(self) -> Optional[str]:
        return self._doc_order[-1] if self._doc_order else None

    def _extract_page_visual_regions(self, page: fitz.Page) -> Tuple[List[float], List[List[float]], int]:
        """Compute tight [x0, y0, x1, y1] content bbox excluding running header/footer, plus image bboxes."""
        pr = page.rect
        top_cut = pr.height * 0.032
        bot_cut = pr.height * 0.905

        x0, y0, x1, y1 = pr.width, pr.height, 0.0, 0.0
        has_any = False
        fig_rects: List[List[float]] = []

        pdict = page.get_text("dict")
        for b in pdict.get("blocks", []):
            bx0, by0, bx1, by1 = b["bbox"]
            btype = b.get("type", 0)
            if btype == 1:
                # Embedded raster image / diagram
                if (bx1 - bx0) >= 24 and (by1 - by0) >= 24:
                    fig_rects.append([float(bx0), float(by0), float(bx1), float(by1)])
                    x0 = min(x0, bx0)
                    y0 = min(y0, by0)
                    x1 = max(x1, bx1)
                    y1 = max(y1, by1)
                    has_any = True
                continue

            # Text block
            txt = " ".join(
                "".join(sp.get("text", "") for sp in ln.get("spans", []))
                for ln in b.get("lines", [])
            ).strip()
            if not txt:
                continue
            low = fold_accents(txt)
            is_problem_hdr = bool(_Q_HEADER_RE.search(txt)) or low.startswith("nivel ")
            # Exclude running page header ("Física General III ... Banco de estudio") or standard study-bank footer
            if by1 < pr.height * 0.085 and not is_problem_hdr and (
                "banco de estudio" in low or ("fisica general" in low and "parcial" in low)
            ):
                continue
            if by1 < top_cut and not is_problem_hdr:
                continue
            if by0 > bot_cut:
                continue
            if "espacio para procedimiento y apuntes" in low:
                continue
            if by0 > pr.height * 0.88 and re.fullmatch(r"\d{1,4}", txt):
                continue

            x0 = min(x0, bx0)
            y0 = min(y0, by0)
            x1 = max(x1, bx1)
            y1 = max(y1, by1)
            has_any = True

        # Include meaningful vector drawings (diagrams/axes) inside the content band
        drawings = page.get_drawings() or []
        vector_diag_rects: List[fitz.Rect] = []
        for dr in drawings:
            r = dr.get("rect")
            if not r:
                continue
            if r.y1 < top_cut or r.y0 > bot_cut:
                continue
            # Ignore pure full-width thin divider lines
            if r.height < 3.0 and r.width > pr.width * 0.65:
                continue
            if r.width >= 18 and r.height >= 18:
                vector_diag_rects.append(r)
                x0 = min(x0, r.x0)
                y0 = min(y0, r.y0)
                x1 = max(x1, r.x1)
                y1 = max(y1, r.y1)
                has_any = True

        # If vector diagrams exist with no raster image, record clustered vector diagram bbox
        if not fig_rects and len(vector_diag_rects) >= 4:
            vx0 = min(r.x0 for r in vector_diag_rects)
            vy0 = min(r.y0 for r in vector_diag_rects)
            vx1 = max(r.x1 for r in vector_diag_rects)
            vy1 = max(r.y1 for r in vector_diag_rects)
            if (vx1 - vx0) >= 60 and (vy1 - vy0) >= 50:
                fig_rects.append([float(vx0), float(vy0), float(vx1), float(vy1)])

        if not has_any:
            return [float(pr.x0 + 24), float(pr.y0 + 36), float(pr.x1 - 24), float(pr.y1 - 48)], fig_rects, len(fig_rects)

        pad_x = 14.0
        pad_top = 18.0
        pad_bot = 14.0
        bbox = [
            max(float(pr.x0), float(x0) - pad_x),
            max(float(pr.y0), float(y0) - pad_top),
            min(float(pr.x1), float(x1) + pad_x),
            min(float(pr.y1), float(y1) + pad_bot),
        ]
        return bbox, fig_rects, len(fig_rects)

    def _parse_problems_in_doc(self, doc: fitz.Document, doc_id: str, filename: str) -> int:
        """Segment and index problems across the PDF (works for Superexamen Q### and generic exam PDFs)."""
        problems: List[Dict[str, Any]] = []

        # Pass 1: Look for Q### structured problem cards (e.g. Superexamen Q001..Q137)
        q_pages: List[Tuple[int, str, str]] = []
        for pno in range(doc.page_count):
            txt = (doc.load_page(pno).get_text("text") or "").strip()
            m = _Q_HEADER_RE.search(txt[:420])
            if m:
                q_pages.append((pno, normalize_problem_id(m.group(1)), txt))

        if q_pages:
            for idx, (pno, pid, txt) in enumerate(q_pages):
                next_pno = q_pages[idx + 1][0] if idx + 1 < len(q_pages) else doc.page_count
                # Usually 1 page per problem, allow continuation up to 2 pages if next page isn't another Q###
                page_end_0 = min(next_pno - 1, pno + 1)
                if page_end_0 > pno:
                    # Only include next page if it looks like continuation (not index/blank)
                    cont_txt = (doc.load_page(page_end_0).get_text("text") or "").strip()
                    if len(cont_txt) < 40:
                        page_end_0 = pno

                level = ""
                source_ref = ""
                family = ""
                topic_code = ""
                subtema = ""
                procedimiento = ""

                ml = re.search(r"Nivel\s*([0-9]+/[0-9]+)", txt)
                if ml:
                    level = f"Nivel {ml.group(1)}"
                ms = re.search(r"(?m)^((?:Examen|Folleto|Pr[aá]ctica|Gu[ií]a)[^\n]{4,120})$", txt)
                if ms:
                    source_ref = ms.group(1).strip()
                mf = re.search(r"Familia:\s*([^\n]+)", txt)
                if mf:
                    fam_line = mf.group(1).strip()
                    mt = re.search(r"\bTema:\s*([0-9]+(?:\.[0-9]+)?)", fam_line)
                    if mt:
                        topic_code = mt.group(1).strip()
                        family = fam_line[: mt.start()].strip(" |·-")
                    else:
                        family = fam_line
                if not topic_code:
                    mt2 = re.search(r"Tema:\s*([0-9]+(?:\.[0-9]+)?)", txt)
                    if mt2:
                        topic_code = mt2.group(1).strip()
                msub = re.search(r"Subtema:\s*([^\n]+)", txt)
                if msub:
                    subtema = msub.group(1).strip()
                mproc = re.search(r"Procedimiento:\s*([^\n]+)", txt)
                if mproc:
                    procedimiento = mproc.group(1).strip()

                # Clean body text (remove header/metadata/footer lines)
                body_lines = []
                for ln in txt.splitlines():
                    s_ln = ln.strip()
                    if not s_ln:
                        continue
                    low = fold_accents(s_ln)
                    if low.startswith(("fisica general iii |", "banco de estudio", "familia:", "tema:", "subtema:", "procedimiento:", "espacio para procedimiento")):
                        continue
                    if re.fullmatch(r"[Qq]\d{2,4}", s_ln) or re.fullmatch(r"Nivel\s*\d+/\d+", s_ln):
                        continue
                    if s_ln == source_ref or re.fullmatch(r"\d{1,4}", s_ln):
                        continue
                    body_lines.append(s_ln)
                for extra_p in range(pno + 1, page_end_0 + 1):
                    body_lines.append((doc.load_page(extra_p).get_text("text") or "").strip())
                body_text = "\n".join(body_lines).strip()

                crop_rects: List[Dict[str, Any]] = []
                fig_rects_all: List[Dict[str, Any]] = []
                fig_total = 0
                for cp in range(pno, page_end_0 + 1):
                    bbox, frects, fcnt = self._extract_page_visual_regions(doc.load_page(cp))
                    crop_rects.append({"page": cp + 1, "rect": bbox})
                    for fr in frects:
                        fig_rects_all.append({"page": cp + 1, "rect": fr})
                    fig_total += fcnt

                problems.append({
                    "problem_id": pid,
                    "page_start": pno + 1,
                    "page_end": page_end_0 + 1,
                    "level": level,
                    "source_ref": source_ref,
                    "family": family,
                    "topic_code": topic_code,
                    "subtema": subtema,
                    "procedimiento": procedimiento,
                    "body_text": body_text,
                    "has_figure": 1 if fig_total > 0 else 0,
                    "figure_count": fig_total,
                    "crop_rects_json": json.dumps(crop_rects),
                    "figure_rects_json": json.dumps(fig_rects_all),
                })
        else:
            # Pass 2: Generic exam/homework PDF segmentation ("Problema 1", "Ejercicio 2", or 1 entry/page)
            for pno in range(doc.page_count):
                page = doc.load_page(pno)
                txt = (page.get_text("text") or "").strip()
                if not txt:
                    continue
                bbox, frects, fcnt = self._extract_page_visual_regions(page)
                matches = list(_GENERIC_PROB_RE.finditer(txt))
                if matches:
                    for mi, m in enumerate(matches):
                        pid = f"P{int(m.group(1)):03d}"
                        start_pos = m.start()
                        end_pos = matches[mi + 1].start() if mi + 1 < len(matches) else len(txt)
                        chunk = txt[start_pos:end_pos].strip()
                        problems.append({
                            "problem_id": pid,
                            "page_start": pno + 1,
                            "page_end": pno + 1,
                            "level": "",
                            "source_ref": f"{filename} p.{pno + 1}",
                            "family": "",
                            "topic_code": "",
                            "subtema": "",
                            "procedimiento": "",
                            "body_text": chunk,
                            "has_figure": 1 if fcnt > 0 else 0,
                            "figure_count": fcnt,
                            "crop_rects_json": json.dumps([{"page": pno + 1, "rect": bbox}]),
                            "figure_rects_json": json.dumps([{"page": pno + 1, "rect": fr} for fr in frects]),
                        })
                else:
                    problems.append({
                        "problem_id": f"PAG{pno + 1:03d}",
                        "page_start": pno + 1,
                        "page_end": pno + 1,
                        "level": "",
                        "source_ref": f"{filename} p.{pno + 1}",
                        "family": "",
                        "topic_code": "",
                        "subtema": "",
                        "procedimiento": "",
                        "body_text": txt,
                        "has_figure": 1 if fcnt > 0 else 0,
                        "figure_count": fcnt,
                        "crop_rects_json": json.dumps([{"page": pno + 1, "rect": bbox}]),
                        "figure_rects_json": json.dumps([{"page": pno + 1, "rect": fr} for fr in frects]),
                    })

        # Deduplicate problem_id collisions safely
        seen_ids: Dict[str, int] = {}
        self._conn.execute("DELETE FROM pdf_problems WHERE doc_id = ?", (doc_id,))
        self._conn.execute("DELETE FROM pdf_problems_fts WHERE doc_id = ?", (doc_id,))

        for prob in problems:
            pid = prob["problem_id"]
            if pid in seen_ids:
                seen_ids[pid] += 1
                pid = f"{pid}_{seen_ids[pid]}"
            else:
                seen_ids[pid] = 1
            prob["problem_id"] = pid

            self._conn.execute(
                """
                INSERT INTO pdf_problems(
                    doc_id, problem_id, filename, page_start, page_end,
                    level, source_ref, family, topic_code, subtema,
                    procedimiento, body_text, has_figure, figure_count,
                    crop_rects_json, figure_rects_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    doc_id, pid, filename, prob["page_start"], prob["page_end"],
                    prob["level"], prob["source_ref"], prob["family"], prob["topic_code"],
                    prob["subtema"], prob["procedimiento"], prob["body_text"],
                    prob["has_figure"], prob["figure_count"],
                    prob["crop_rects_json"], prob["figure_rects_json"],
                ),
            )
            meta_blob = " ".join([
                pid, prob["family"], prob["topic_code"],
                prob["subtema"], prob["procedimiento"], prob["source_ref"],
            ])
            self._conn.execute(
                "INSERT INTO pdf_problems_fts(doc_id, problem_id, folded_meta, folded_body) VALUES (?, ?, ?, ?)",
                (doc_id, pid, fold_accents(meta_blob), fold_accents(prob["body_text"])),
            )
        return len(problems)

    def index_pdf_bytes(self, filename: str, pdf_bytes: bytes, file_path: str = "") -> IndexedPdfDoc:
        if not pdf_bytes:
            raise ValueError("El archivo PDF está vacío.")
        try:
            doc = fitz.open(stream=pdf_bytes, filetype="pdf")
        except Exception as exc:
            raise ValueError(f"Archivo PDF inválido o corrupto: {exc}") from exc

        if doc.page_count == 0:
            doc.close()
            raise ValueError("El PDF no contiene páginas.")

        doc_id = hashlib.sha256(pdf_bytes).hexdigest()[:12]
        clean_name = pathlib.Path(filename or "documento.pdf").name
        self._raw_bytes[doc_id] = pdf_bytes

        self._conn.execute("DELETE FROM pdf_docs WHERE doc_id = ?", (doc_id,))
        self._conn.execute("DELETE FROM pdf_pages_fts WHERE doc_id = ?", (doc_id,))
        for pno in range(doc.page_count):
            page = doc.load_page(pno)
            text = (page.get_text("text") or "").strip()
            self._conn.execute(
                "INSERT INTO pdf_pages_fts(doc_id, filename, page_num, content) VALUES (?, ?, ?, ?)",
                (doc_id, clean_name, pno + 1, fold_accents(text) + "\n" + text),
            )

        prob_count = self._parse_problems_in_doc(doc, doc_id, clean_name)

        meta = IndexedPdfDoc(
            doc_id=doc_id,
            filename=clean_name,
            path=file_path,
            page_count=doc.page_count,
            problem_count=prob_count,
        )
        self._docs[doc_id] = meta
        if doc_id in self._doc_order:
            self._doc_order.remove(doc_id)
        self._doc_order.append(doc_id)

        self._conn.execute(
            "INSERT INTO pdf_docs(doc_id, filename, file_path, page_count, problem_count) VALUES (?, ?, ?, ?, ?)",
            (doc_id, clean_name, file_path, doc.page_count, prob_count),
        )
        self._conn.commit()
        doc.close()
        return meta

    def index_pdf_file(self, path: str | pathlib.Path) -> IndexedPdfDoc:
        p = pathlib.Path(path)
        if not p.is_file():
            raise FileNotFoundError(f"No se encontró el PDF: {p}")
        if p.suffix.lower() != ".pdf":
            raise ValueError("El archivo indicado no tiene extensión .pdf.")
        return self.index_pdf_bytes(p.name, p.read_bytes(), str(p))

    def list_docs(self) -> List[Dict[str, Any]]:
        return [
            {
                "doc_id": d.doc_id,
                "filename": d.filename,
                "page_count": d.page_count,
                "problem_count": d.problem_count,
            }
            for d in self._docs.values()
        ]

    def _row_to_problem_dict(self, r: Tuple[Any, ...]) -> Dict[str, Any]:
        return {
            "doc_id": r[0],
            "problem_id": r[1],
            "filename": r[2],
            "page_start": int(r[3]),
            "page_end": int(r[4]),
            "level": r[5],
            "source_ref": r[6],
            "family": r[7],
            "topic_code": r[8],
            "subtema": r[9],
            "procedimiento": r[10],
            "body_text": r[11],
            "has_figure": bool(r[12]),
            "figure_count": int(r[13]),
            "crop_rects": json.loads(r[14] or "[]"),
            "figure_rects": json.loads(r[15] or "[]"),
        }

    def get_problem_by_id(
        self,
        problem_id_query: str,
        doc_id: Optional[str] = None,
    ) -> Optional[Dict[str, Any]]:
        """Lookup a problem by exact or flexible ID ('q057', 'Q57', '122', 'Problema 5')."""
        target_doc = doc_id or self.latest_doc_id()
        if not target_doc:
            return None

        norm = normalize_problem_id(problem_id_query)
        candidates = [norm]
        digits = re.sub(r"\D+", "", problem_id_query or "")
        if digits:
            num = int(digits)
            for c in (f"Q{num:03d}", f"Q{num:02d}", f"Q{num}", f"P{num:03d}", f"PAG{num:03d}"):
                if c not in candidates:
                    candidates.append(c)

        cols = (
            "doc_id, problem_id, filename, page_start, page_end, level, source_ref, "
            "family, topic_code, subtema, procedimiento, body_text, has_figure, "
            "figure_count, crop_rects_json, figure_rects_json"
        )
        for cand in candidates:
            cur = self._conn.execute(
                f"SELECT {cols} FROM pdf_problems WHERE doc_id = ? AND UPPER(problem_id) = ? LIMIT 1",
                (target_doc, cand.upper()),
            )
            row = cur.fetchone()
            if row:
                return self._row_to_problem_dict(row)
        return None

    def crop_problem_visual(
        self,
        doc_id: Optional[str],
        problem_id: str,
        zoom: float = 2.0,
    ) -> Dict[str, Any]:
        """Crop the exact statement + attached figures of a problem from the PDF as a high-res PNG."""
        prob = self.get_problem_by_id(problem_id, doc_id=doc_id)
        if not prob:
            raise KeyError(f"Problema no encontrado en el PDF: {problem_id}")

        actual_doc_id = prob["doc_id"]
        raw = self._raw_bytes.get(actual_doc_id)
        if not raw:
            raise KeyError(f"Documento PDF no disponible en memoria: {actual_doc_id}")

        doc = fitz.open(stream=raw, filetype="pdf")
        try:
            mat = fitz.Matrix(zoom, zoom)
            gap_px = 12
            pixmaps: List[fitz.Pixmap] = []
            raw_word_boxes: List[Tuple[str, float, float, float, float]] = []
            y_px_offset = 0.0
            for item in prob["crop_rects"] or [{"page": prob["page_start"], "rect": None}]:
                pidx = max(0, min(int(item["page"]) - 1, doc.page_count - 1))
                page = doc.load_page(pidx)
                rect_arr = item.get("rect")
                clip = fitz.Rect(*rect_arr) if rect_arr and len(rect_arr) == 4 else page.rect
                clip = clip & page.rect
                if clip.width < 20 or clip.height < 20:
                    clip = page.rect
                pix = page.get_pixmap(matrix=mat, clip=clip, alpha=False)
                pixmaps.append(pix)

                # Extract exact word bounding boxes in stitched-PNG pixel coordinates
                words = page.get_text("words", clip=clip) or []
                words.sort(key=lambda w: (int(w[5]), int(w[6]), int(w[7])))
                for w in words:
                    wx0, wy0, wx1, wy1, wtxt = float(w[0]), float(w[1]), float(w[2]), float(w[3]), str(w[4])
                    if not wtxt.strip():
                        continue
                    px0 = (wx0 - clip.x0) * zoom
                    py0 = y_px_offset + (wy0 - clip.y0) * zoom
                    px1 = (wx1 - clip.x0) * zoom
                    py1 = y_px_offset + (wy1 - clip.y0) * zoom
                    if px1 > px0 and py1 > py0:
                        raw_word_boxes.append((wtxt, px0, py0, px1, py1))
                y_px_offset += pix.height + gap_px

            problem_png = _stitch_pixmaps_vertical(pixmaps, gap_px=gap_px)
            out_w = max(p.width for p in pixmaps) if pixmaps else 0
            out_h = (sum(p.height for p in pixmaps) + gap_px * max(0, len(pixmaps) - 1)) if pixmaps else 0

            crop_words: List[Dict[str, Any]] = []
            if out_w > 0 and out_h > 0:
                fw = float(out_w)
                fh = float(out_h)
                for wtxt, px0, py0, px1, py1 in raw_word_boxes:
                    nx0 = max(0.0, min(1.0, px0 / fw))
                    ny0 = max(0.0, min(1.0, py0 / fh))
                    nx1 = max(nx0, min(1.0, px1 / fw))
                    ny1 = max(ny0, min(1.0, py1 / fh))
                    crop_words.append({
                        "t": wtxt,
                        "x": round(nx0, 4),
                        "y": round(ny0, 4),
                        "w": round(nx1 - nx0, 4),
                        "h": round(ny1 - ny0, 4),
                    })

            # Also render primary figure crop separately if present
            figure_png_b64 = ""
            if prob["figure_rects"]:
                f0 = prob["figure_rects"][0]
                fp = doc.load_page(max(0, min(int(f0["page"]) - 1, doc.page_count - 1)))
                frect = fitz.Rect(*f0["rect"])
                frect = fitz.Rect(
                    max(fp.rect.x0, frect.x0 - 12),
                    max(fp.rect.y0, frect.y0 - 12),
                    min(fp.rect.x1, frect.x1 + 12),
                    min(fp.rect.y1, frect.y1 + 12),
                )
                if frect.width >= 20 and frect.height >= 20:
                    fpix = fp.get_pixmap(matrix=fitz.Matrix(2.2, 2.2), clip=frect, alpha=False)
                    figure_png_b64 = base64.b64encode(fpix.tobytes("png")).decode("ascii")

            return {
                **{k: v for k, v in prob.items() if k not in ("crop_rects", "figure_rects")},
                "width": out_w,
                "height": out_h,
                "problem_png_b64": base64.b64encode(problem_png).decode("ascii"),
                "figure_png_b64": figure_png_b64,
                "crop_words": crop_words,
            }
        finally:
            doc.close()

    def search_problems(
        self,
        query: str,
        doc_id: Optional[str] = None,
        limit: int = 8,
        require_figure: bool = False,
    ) -> List[Dict[str, Any]]:
        """Search indexed PDF problems by topic, subtema, procedure, or statement keywords."""
        target_doc = doc_id or self.latest_doc_id()
        if not target_doc:
            return []

        folded = fold_accents(query)
        raw_tokens = [t for t in re.findall(r"[a-z0-9.]{2,}", folded) if t not in _STOPWORDS_ES]
        if not raw_tokens:
            return []

        # Build stem-friendly prefix FTS query so 'esfera' matches 'esferica', 'conduccion' matches 'conduccion'
        terms = []
        for t in raw_tokens[:8]:
            stem = t[:-1] if len(t) >= 6 and t.endswith(("a", "o", "s", "e")) else t
            terms.append(f'"{stem}"*')
        fts_q = " OR ".join(terms)

        cols = (
            "p.doc_id, p.problem_id, p.filename, p.page_start, p.page_end, p.level, p.source_ref, "
            "p.family, p.topic_code, p.subtema, p.procedimiento, p.body_text, p.has_figure, "
            "p.figure_count, p.crop_rects_json, p.figure_rects_json"
        )
        rows = []
        try:
            sql = f"""
                SELECT {cols}, bm25(pdf_problems_fts, 10.0, 4.0, 1.5) AS score
                FROM pdf_problems_fts f
                JOIN pdf_problems p
                  ON p.doc_id = f.doc_id AND p.problem_id = f.problem_id
                WHERE f.doc_id = ? AND pdf_problems_fts MATCH ?
                {"AND p.has_figure = 1" if require_figure else ""}
                ORDER BY score
                LIMIT ?
            """
            rows = self._conn.execute(sql, (target_doc, fts_q, max(1, min(limit * 2, 24)))).fetchall()
        except sqlite3.OperationalError:
            rows = []

        results: List[Dict[str, Any]] = []
        for r in rows:
            item = self._row_to_problem_dict(r[:-1])
            hay = fold_accents(f"{item['subtema']} {item['procedimiento']} {item['family']} {item['body_text']}")
            hits = sum(1 for t in raw_tokens if t[:5] in hay)
            item["match_score"] = round(hits / max(1, len(raw_tokens)), 3)
            item["excerpt"] = item["body_text"][:220].replace("\n", " ")
            results.append(item)

        results.sort(key=lambda x: (x["match_score"], 1 if x["has_figure"] else 0), reverse=True)
        return results[:limit]

    def audit_repeated_problems(
        self,
        doc_id: Optional[str] = None,
        top_k: int = 8,
    ) -> Dict[str, Any]:
        """Visually and structurally audit a PDF to find:
        1. Most-repeated problem archetypes / subtopics / procedures (frequency table)
        2. Near-duplicate / cloned problem statements (same physics problem with changed numbers)
        """
        target_doc = doc_id or self.latest_doc_id()
        if not target_doc or target_doc not in self._docs:
            return {
                "ok": False,
                "error": "No hay ningún PDF cargado todavía.",
                "total_problems": 0,
                "top_archetypes": [],
                "near_duplicate_clusters": [],
            }

        meta = self._docs[target_doc]
        rows = self._conn.execute(
            """
            SELECT problem_id, page_start, family, topic_code, subtema, procedimiento, body_text, has_figure
            FROM pdf_problems
            WHERE doc_id = ?
            ORDER BY page_start, problem_id
            """,
            (target_doc,),
        ).fetchall()

        total = len(rows)
        with_figs = sum(1 for r in rows if r[7])

        # 1. Frequency distribution by procedure / subtema archetype
        arch_groups: Dict[Tuple[str, str, str], List[Dict[str, Any]]] = collections.defaultdict(list)
        shingles: List[Tuple[str, int, str, str, set]] = []

        for pid, pstart, fam, tcode, sub, proc, body, hfig in rows:
            key_sub = sub or fam or "Tema general"
            key_proc = proc or (f"Tema {tcode}" if tcode else "Procedimiento de desarrollo")
            arch_groups[(fam or "Física", key_sub, key_proc)].append({
                "problem_id": pid,
                "page": int(pstart),
                "has_figure": bool(hfig),
            })
            toks = _statement_fingerprint_tokens(body)
            shingles.append((pid, int(pstart), key_sub, body[:180].replace("\n", " "), _shingle_set(toks)))

        top_archetypes = []
        for (fam, sub, proc), items in sorted(arch_groups.items(), key=lambda kv: len(kv[1]), reverse=True)[:top_k]:
            fig_cnt = sum(1 for it in items if it["has_figure"])
            top_archetypes.append({
                "family": fam,
                "subtema": sub,
                "procedimiento": proc,
                "count": len(items),
                "pct": round((len(items) * 100.0) / max(1, total), 1),
                "with_figures": fig_cnt,
                "sample_ids": [it["problem_id"] for it in items[:8]],
            })

        # 2. Near-duplicate / cloned problem detection (Jaccard over number-stripped tokens+bigrams)
        n = len(shingles)
        parent = list(range(n))

        def find(i: int) -> int:
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        def union(a: int, b: int) -> None:
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[rb] = ra

        pair_sims: Dict[Tuple[int, int], float] = {}
        for i in range(n):
            si = shingles[i][4]
            if len(si) < 8:
                continue
            for j in range(i + 1, n):
                sj = shingles[j][4]
                if len(sj) < 8:
                    continue
                inter = len(si & sj)
                if inter < 6:
                    continue
                union_sz = len(si | sj)
                sim = inter / float(union_sz)
                if sim >= 0.36:
                    union(i, j)
                    pair_sims[(i, j)] = sim

        cluster_map: Dict[int, List[int]] = collections.defaultdict(list)
        for i in range(n):
            cluster_map[find(i)].append(i)

        near_clusters = []
        for members in cluster_map.values():
            if len(members) < 2:
                continue
            sims = [
                pair_sims[(a, b)]
                for idx_a, a in enumerate(members)
                for b in members[idx_a + 1 :]
                if (a, b) in pair_sims
            ]
            avg_sim = round((sum(sims) / max(1, len(sims))) * 100.0, 1)
            near_clusters.append({
                "cluster_size": len(members),
                "similarity_pct": avg_sim,
                "subtema": shingles[members[0]][2],
                "problem_ids": [shingles[m][0] for m in members],
                "pages": [shingles[m][1] for m in members],
                "sample_excerpt": shingles[members[0]][3],
            })

        near_clusters.sort(key=lambda c: (c["cluster_size"], c["similarity_pct"]), reverse=True)

        return {
            "ok": True,
            "doc_id": target_doc,
            "filename": meta.filename,
            "page_count": meta.page_count,
            "total_problems": total,
            "problems_with_figures": with_figs,
            "top_archetypes": top_archetypes,
            "near_duplicate_clusters": near_clusters[:top_k],
            "sample_problem_ids": [r[0] for r in rows[:12]],
        }

    def render_page_png(self, doc_id: str, page_num_1based: int, zoom: float = 1.65) -> Dict[str, Any]:
        raw = self._raw_bytes.get(doc_id)
        if not raw:
            raise KeyError(f"Documento PDF no cargado: {doc_id}")
        doc = fitz.open(stream=raw, filetype="pdf")
        try:
            idx = max(0, min(page_num_1based - 1, doc.page_count - 1))
            page = doc.load_page(idx)
            mat = fitz.Matrix(zoom, zoom)
            pix = page.get_pixmap(matrix=mat, alpha=False)
            png_bytes = pix.tobytes("png")
            return {
                "doc_id": doc_id,
                "filename": self._docs[doc_id].filename,
                "page": idx + 1,
                "page_count": doc.page_count,
                "width": pix.width,
                "height": pix.height,
                "png_b64": base64.b64encode(png_bytes).decode("ascii"),
            }
        finally:
            doc.close()

    def extract_region_text_and_crop(
        self,
        doc_id: str,
        page_num_1based: int,
        rect_norm: Dict[str, float],
        zoom: float = 2.2,
    ) -> Dict[str, Any]:
        """Extract exact vector text and a high-res PNG crop from a circled normalized region on a PDF page."""
        raw = self._raw_bytes.get(doc_id)
        if not raw:
            raise KeyError(f"Documento PDF no cargado: {doc_id}")
        doc = fitz.open(stream=raw, filetype="pdf")
        try:
            idx = max(0, min(page_num_1based - 1, doc.page_count - 1))
            page = doc.load_page(idx)
            pr = page.rect
            x0 = max(0.0, min(1.0, float(rect_norm.get("x", 0.0))))
            y0 = max(0.0, min(1.0, float(rect_norm.get("y", 0.0))))
            w = max(0.02, min(1.0 - x0, float(rect_norm.get("w", 0.25))))
            h = max(0.02, min(1.0 - y0, float(rect_norm.get("h", 0.15))))
            clip = fitz.Rect(
                pr.x0 + x0 * pr.width,
                pr.y0 + y0 * pr.height,
                pr.x0 + (x0 + w) * pr.width,
                pr.y0 + (y0 + h) * pr.height,
            )
            region_text = (page.get_text("text", clip=clip) or "").strip()
            ctx_clip = fitz.Rect(
                max(pr.x0, clip.x0 - 24),
                max(pr.y0, clip.y0 - 24),
                min(pr.x1, clip.x1 + 24),
                min(pr.y1, clip.y1 + 24),
            )
            pix = page.get_pixmap(matrix=fitz.Matrix(zoom, zoom), clip=ctx_clip, alpha=False)
            return {
                "doc_id": doc_id,
                "filename": self._docs[doc_id].filename,
                "page": idx + 1,
                "region_text": region_text,
                "crop_png_b64": base64.b64encode(pix.tobytes("png")).decode("ascii"),
            }
        finally:
            doc.close()

    def search(self, query: str, limit: int = 5) -> List[Dict[str, Any]]:
        tokens = [t.strip() for t in fold_accents(query).replace('"', " ").split() if len(t.strip()) >= 2]
        if not tokens:
            return []
        fts_q = " OR ".join(f'"{t}"' for t in tokens[:8])
        try:
            cur = self._conn.execute(
                """
                SELECT doc_id, filename, page_num,
                       snippet(pdf_pages_fts, 3, '', '', ' … ', 28) AS excerpt
                FROM pdf_pages_fts
                WHERE pdf_pages_fts MATCH ?
                ORDER BY bm25(pdf_pages_fts)
                LIMIT ?
                """,
                (fts_q, max(1, min(limit, 12))),
            )
            rows = cur.fetchall()
        except sqlite3.OperationalError:
            return []
        return [
            {
                "doc_id": r[0],
                "doc_name": r[1],
                "page": int(r[2]),
                "excerpt": (r[3] or "").strip(),
            }
            for r in rows
        ]
