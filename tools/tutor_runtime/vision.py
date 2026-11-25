"""Reconocimiento de imágenes pegadas (Ctrl+V) y generador de lecciones IA recalculadas.

Orden de trabajo para Ctrl+V:
  1. Si la imagen coincide con uno de los 158 ejercicios verificados del banco (por OCR local
     de Windows, hash visual de recortes en caché, o transcripción rápida con Gemini Vision),
     devuelve el id verificado del banco para abrir la lección determinista al instante.
  2. Si es un problema nuevo o de otra materia, la IA arma los pasos y Python recalcula
     cada operación aritmética con verificador_numerico.recalcular_leccion_ia().
"""

from __future__ import annotations

import asyncio
import base64
import io
import json
import os
import pathlib
import re
import subprocess
import tempfile
import time
from typing import Any, Dict, List, Optional, Set, Tuple

from PIL import Image

from .fisica3 import registro
from .pdf_rag import fold_accents
from .verificador_numerico import limpiar_voz_latex, recalcular_leccion_ia

MAX_IMAGEN_BYTES = 12 * 1024 * 1024

_PS_OCR_SCRIPT = r"""
$ErrorActionPreference = 'Stop'
$ImgPath = $env:TUTOR_OCR_PATH
Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null
$null = [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime]
$null = [Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics.Imaging, ContentType = WindowsRuntime]
$null = [Windows.Media.Ocr.OcrEngine, Windows.Media.Ocr, ContentType = WindowsRuntime]
$null = [Windows.Globalization.Language, Windows.Globalization, ContentType = WindowsRuntime]
$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
})[0]
function Await($op, $t) {
    $m = $asTaskGeneric.MakeGenericMethod($t)
    $task = $m.Invoke($null, @($op))
    $task.Wait(6000) | Out-Null
    return $task.Result
}
$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($ImgPath)) ([Windows.Storage.StorageFile])
$stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
$decoder = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
$bmp = Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
if (-not $engine) {
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage([Windows.Globalization.Language]::new('es'))
}
if (-not $engine) {
    $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage([Windows.Globalization.Language]::new('en'))
}
if ($engine) {
    $res = Await ($engine.RecognizeAsync($bmp)) ([Windows.Media.Ocr.OcrResult])
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
    Write-Output $res.Text
}
$stream.Dispose()
"""

_STOPWORDS: Set[str] = {
    "que", "con", "para", "por", "una", "uno", "unos", "unas", "del", "las", "los",
    "como", "cual", "cuales", "donde", "cuando", "entre", "sobre", "desde", "hasta",
    "este", "esta", "estos", "estas", "ese", "esa", "pero", "mas", "muy", "sin",
    "sus", "son", "fue", "ser", "tiene", "figura", "muestra", "calcule", "determine",
    "encuentre", "problema", "ejercicio", "pregunta", "valor", "parte", "respuesta",
}


def dhash_imagen(img_bytes: bytes) -> Optional[int]:
    """Calcula un dHash perceptual de 64 bits (9x8 en escala de grises)."""
    try:
        im = Image.open(io.BytesIO(img_bytes)).convert("L").resize((9, 8), Image.Resampling.BILINEAR)
        px = list(im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata())
        bits = 0
        for fila in range(8):
            base = fila * 9
            for col in range(8):
                bits = (bits << 1) | (1 if px[base + col] > px[base + col + 1] else 0)
        return bits
    except Exception:
        return None


def _hamming(a: int, b: int) -> int:
    return (a ^ b).bit_count()


def ocr_local_windows(img_bytes: bytes, timeout_s: float = 7.0) -> str:
    """OCR 100% local con Windows.Media.Ocr (sin nube, sin dependencias externas)."""
    if os.name != "nt" or not img_bytes:
        return ""
    tmp_path = ""
    try:
        im = Image.open(io.BytesIO(img_bytes)).convert("RGB")
        # Asegura tamaño cómodo para OCR si es muy pequeña
        if max(im.size) < 420:
            escala = min(3.0, 600.0 / max(1, max(im.size)))
            im = im.resize((int(im.width * escala), int(im.height * escala)), Image.Resampling.BICUBIC)
        fd, tmp_path = tempfile.mkstemp(prefix="tutor_ocr_", suffix=".png")
        os.close(fd)
        im.save(tmp_path, "PNG")
        env = dict(os.environ, TUTOR_OCR_PATH=tmp_path)
        proc = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", _PS_OCR_SCRIPT],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
            timeout=timeout_s,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        if proc.returncode == 0:
            return " ".join((proc.stdout or "").split())
        return ""
    except Exception:
        return ""
    finally:
        if tmp_path:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass


def _extraer_numeros(texto: str) -> Set[str]:
    """Extrae números físicos relevantes normalizados (coma->punto, sin ceros finales innecesarios)."""
    s = re.sub(r"(?<=\d),(?=\d)", ".", texto or "")
    s = s.replace("{,}", ".")
    nums: Set[str] = set()
    for m in re.finditer(r"(?<![A-Za-z])(\d+(?:\.\d+)?)(?![A-Za-z])", s):
        raw = m.group(1)
        try:
            v = float(raw)
        except ValueError:
            continue
        if v == 0:
            continue
        # Ignorar enteros muy pequeños y genéricos (1, 2, 3, 4) salvo que tengan decimales
        if "." not in raw and v in (1.0, 2.0, 3.0, 4.0, 5.0):
            continue
        norm = f"{v:g}"
        nums.add(norm)
    return nums


def _tokens_significativos(texto: str) -> Set[str]:
    limpio = re.sub(r"\\[A-Za-z]+|[{}$^_\\]", " ", texto or "")
    limpio = re.sub(r"[^a-z0-9 ]", " ", fold_accents(limpio).lower())
    out: Set[str] = set()
    for w in limpio.split():
        if len(w) >= 4 and w not in _STOPWORDS and not w.isdigit():
            out.add(w[:6])
    return out


def _trigramas(texto: str) -> Set[str]:
    limpio = re.sub(r"\\[A-Za-z]+|[{}$^_\\]", " ", texto or "")
    limpio = re.sub(r"[^a-z0-9 ]", " ", fold_accents(limpio).lower())
    pal = [w for w in limpio.split() if len(w) > 2 or w.isdigit()]
    return {" ".join(pal[i : i + 3]) for i in range(max(0, len(pal) - 2))}


_INDICE_BANCO: Optional[List[Dict[str, Any]]] = None


def _construir_indice_banco() -> List[Dict[str, Any]]:
    global _INDICE_BANCO
    if _INDICE_BANCO is not None:
        return _INDICE_BANCO
    filas: List[Dict[str, Any]] = []
    for lid in registro.ids():
        if lid.startswith("T-"):
            continue
        lec = registro.obtener(lid)
        base_txt = f"{lec.titulo} {lec.enunciado} {' '.join(lec.claves)}"
        filas.append({
            "id": lid,
            "titulo": lec.titulo,
            "etiqueta": lid,
            "tokens": _tokens_significativos(base_txt),
            "nums": _extraer_numeros(base_txt),
            "tri": _trigramas(base_txt),
        })
    _INDICE_BANCO = filas
    return filas


def emparejar_texto_con_banco(texto: str, documentos: Any = None) -> Dict[str, Any]:
    """Busca con alta precisión si un texto (de OCR o usuario) corresponde a uno de los 158 ejercicios."""
    txt = (texto or "").strip()
    if not txt:
        return {"id": "", "etiqueta": "", "titulo": "", "seguro": False, "texto": "", "metodo": "vacio", "puntaje": 0.0}

    # 1. Código explícito Q001..Q137
    mq = re.search(r"\bQ\s*0*(\d{1,3})\b", txt, re.IGNORECASE)
    if mq:
        qid = f"Q{int(mq.group(1)):03d}"
        if registro.existe(qid):
            lec = registro.obtener(qid)
            return {
                "id": qid,
                "etiqueta": qid,
                "titulo": lec.titulo,
                "seguro": True,
                "texto": txt[:1500],
                "metodo": "codigo_Q",
                "puntaje": 1.0,
            }

    # 2. Emparejamiento vía documentos.parecidos (cubre exactamente el texto PDF oficial)
    if documentos is not None:
        try:
            pares = documentos.parecidos(txt, minimo=0.26)
            con_lec = [p for p in pares if p.get("leccion") and registro.existe(p["leccion"])]
            if con_lec:
                mejor = con_lec[0]
                segundo = con_lec[1]["puntos"] if len(con_lec) > 1 else 0.0
                lec = registro.obtener(mejor["leccion"])
                seguro = mejor["puntos"] >= 0.42 or (mejor["puntos"] >= 0.30 and mejor["puntos"] - segundo >= 0.12)
                return {
                    "id": lec.id,
                    "etiqueta": mejor.get("etiqueta") or lec.id,
                    "titulo": lec.titulo,
                    "seguro": bool(seguro),
                    "texto": txt[:1500],
                    "metodo": "documentos_trigramas",
                    "puntaje": float(mejor["puntos"]),
                }
        except Exception:
            pass

    # 3. Emparejamiento determinista contra enunciados + números de los 158 ejercicios del banco
    toks_q = _tokens_significativos(txt)
    nums_q = _extraer_numeros(txt)
    tri_q = _trigramas(txt)

    # Bonus si dice "Ejercicio N" (1..21)
    m_ej = re.search(r"\bejercicio\s*#?\s*(\d{1,2})\b", fold_accents(txt).lower())
    ej_id = f"E{int(m_ej.group(1))}" if m_ej and 1 <= int(m_ej.group(1)) <= 21 else ""

    candidatos: List[Tuple[float, Dict[str, Any], int, int]] = []
    for fila in _construir_indice_banco():
        inter_tok = len(toks_q & fila["tokens"])
        inter_num = len(nums_q & fila["nums"])
        inter_tri = len(tri_q & fila["tri"])
        if inter_tok < 2 and inter_num < 2 and fila["id"] != ej_id:
            continue
        score_tok = inter_tok / max(1, min(len(toks_q), len(fila["tokens"])))
        score_num = inter_num / max(1, min(len(nums_q), len(fila["nums"]))) if nums_q and fila["nums"] else 0.0
        score_tri = inter_tri / max(1, min(len(tri_q), len(fila["tri"]))) if tri_q and fila["tri"] else 0.0
        puntaje = 0.45 * score_tok + 0.35 * score_num + 0.20 * score_tri
        if inter_num >= 3 and inter_tok >= 3:
            puntaje += 0.18
        if fila["id"] == ej_id and (inter_tok >= 2 or inter_num >= 1):
            puntaje += 0.35
        candidatos.append((puntaje, fila, inter_tok, inter_num))

    if not candidatos:
        return {"id": "", "etiqueta": "", "titulo": "", "seguro": False, "texto": txt[:1500], "metodo": "sin_coincidencia", "puntaje": 0.0}

    candidatos.sort(key=lambda x: (-x[0], -x[3], -x[2], x[1]["id"]))
    p1, f1, it1, in1 = candidatos[0]
    p2 = candidatos[1][0] if len(candidatos) > 1 else 0.0

    if p1 < 0.28 or (it1 < 2 and in1 < 2):
        return {"id": "", "etiqueta": "", "titulo": "", "seguro": False, "texto": txt[:1500], "metodo": "puntaje_bajo", "puntaje": round(p1, 3)}

    seguro = (p1 >= 0.50 and (p1 - p2) >= 0.08) or (p1 >= 0.65) or (in1 >= 3 and it1 >= 4 and (p1 - p2) >= 0.10)
    return {
        "id": f1["id"],
        "etiqueta": f1["etiqueta"],
        "titulo": f1["titulo"],
        "seguro": bool(seguro),
        "texto": txt[:1500],
        "metodo": "banco_texto_numeros",
        "puntaje": round(p1, 3),
    }


def _transcribir_con_gemini(img_bytes: bytes, mime: str, visual_director: Any) -> str:
    """Usa Gemini Vision SOLO para transcribir el texto/números de una imagen si el OCR local fue pobre."""
    if visual_director is None or not getattr(visual_director, "configured", False):
        return ""
    client = getattr(visual_director, "_client", None)
    if client is None:
        return ""
    try:
        from google.genai import types

        prompt = (
            "Transcribe únicamente el texto, código de problema (ej. Q057, Ejercicio 8), números, "
            "unidades y fórmulas que aparecen en esta imagen. No resuelvas el ejercicio ni agregues comentarios."
        )
        resp = client.models.generate_content(
            model=getattr(visual_director, "primary_model", "gemini-2.5-flash"),
            contents=[prompt, types.Part.from_bytes(data=img_bytes, mime_type=mime or "image/png")],
            config=types.GenerateContentConfig(temperature=0.0, max_output_tokens=600),
        )
        return " ".join((resp.text or "").split())
    except Exception:
        return ""


def _emparejar_dhash_cache(img_bytes: bytes, documentos: Any) -> Optional[Dict[str, Any]]:
    """Si el usuario pegó una captura exacta de un recorte ya en caché, lo reconoce sin OCR."""
    if documentos is None:
        return None
    cache_dir = getattr(documentos, "cache", None)
    if not cache_dir or not pathlib.Path(cache_dir).is_dir():
        return None
    h_in = dhash_imagen(img_bytes)
    if h_in is None:
        return None
    # Comprueba correspondencia contra recortes de lecciones que ya tengan PNG en caché
    try:
        for lid in registro.ids():
            if not (lid.startswith("Q") or lid.startswith("E")):
                continue
            doc_id = "superexamen" if lid.startswith("Q") else "recomendados"
            clave = lid if lid.startswith("Q") else lid[1:]
            if doc_id not in documentos.rutas():
                continue
            try:
                p = documentos.problema(doc_id, clave)
            except KeyError:
                continue
            f = pathlib.Path(cache_dir) / f"{documentos._clave_cache(doc_id, p)}.png"
            if not f.is_file():
                continue
            h_ref = p.extra.get("_dhash")
            if h_ref is None:
                h_ref = dhash_imagen(f.read_bytes())
                p.extra["_dhash"] = h_ref
            if h_ref is not None and _hamming(h_in, h_ref) <= 5:
                lec = registro.obtener(lid)
                return {
                    "id": lid,
                    "etiqueta": documentos.etiqueta(doc_id, clave),
                    "titulo": lec.titulo,
                    "seguro": True,
                    "texto": p.texto[:1500],
                    "metodo": "dhash_recorte",
                    "puntaje": 1.0,
                }
    except Exception:
        return None
    return None


def reconocer_imagen(
    img_bytes: bytes,
    mime: str = "image/png",
    documentos: Any = None,
    visual_director: Any = None,
) -> Dict[str, Any]:
    """Reconoce una imagen pegada con Ctrl+V: primero busca en el banco de 158 ejercicios."""
    if not img_bytes:
        return {"id": "", "etiqueta": "", "titulo": "", "seguro": False, "texto": "", "metodo": "vacio"}
    if len(img_bytes) > MAX_IMAGEN_BYTES:
        raise ValueError("La imagen pasa de 12 MB")

    # 1. Coincidencia perceptual rápida contra recortes cacheados
    hit_hash = _emparejar_dhash_cache(img_bytes, documentos)
    if hit_hash:
        return hit_hash

    # 2. OCR local de Windows (rápido, sin cuota ni internet)
    ocr_txt = ocr_local_windows(img_bytes)
    if len(ocr_txt) >= 12:
        res = emparejar_texto_con_banco(ocr_txt, documentos=documentos)
        if res.get("id") and res.get("seguro"):
            return res
        # Si no fue seguro y tenemos Gemini Vision, probamos transcripción más fiel
        if not res.get("id") and len(ocr_txt) < 40 and visual_director is not None and getattr(visual_director, "configured", False):
            gem_txt = _transcribir_con_gemini(img_bytes, mime, visual_director)
            if gem_txt:
                res2 = emparejar_texto_con_banco(gem_txt, documentos=documentos)
                if res2.get("id"):
                    return res2
                res2["texto"] = gem_txt[:1500]
                return res2
        return res

    # 3. Respaldo Gemini Vision si OCR local devolvió poco texto (ej. foto manuscrita o sin paquete de idioma)
    gem_txt = _transcribir_con_gemini(img_bytes, mime, visual_director)
    if gem_txt:
        res = emparejar_texto_con_banco(gem_txt, documentos=documentos)
        if not res.get("texto"):
            res["texto"] = gem_txt[:1500]
        return res

    return {"id": "", "etiqueta": "", "titulo": "", "seguro": False, "texto": ocr_txt[:1500], "metodo": "sin_texto"}


_LECCION_IA_PROMPT = """Eres un profesor particular audiovisual que explica paso a paso en español natural de Costa Rica para alguien que necesita pasos cortos.
Resuelve el problema dado (de CUALQUIER materia: física, cálculo, química, biología, historia, programación, etc.) devolviendo EXCLUSIVAMENTE un JSON válido con este formato:
{
  "titulo": "Título claro y breve del ejercicio",
  "tema": "materia_o_tema",
  "enunciado": "Resumen corto del enunciado (en español claro; fórmulas entre $...$ si hace falta)",
  "pasos": [
    {
      "titulo": "Título corto del paso",
      "porque": "Razón breve de por qué hacemos este paso",
      "tramos": [
        {
          "habla": "Explicación hablada corta en español natural (SIN NINGÚN LaTeX, sin barras invertidas ni signos de dólar)",
          "acc": [
            {"a": "mate", "tex": "ecuación LaTeX limpia", "color": "azul"},
            {"a": "texto", "txt": "Nota breve en la pizarra", "clase": "nota"}
          ]
        }
      ]
    }
  ],
  "respuestas": [
    {"inciso": "", "tex": "resultado final", "nota": "explicación breve"}
  ]
}
REGLAS ESTRICTAS:
1. PROHIBIDO escribir LaTeX crudo (\\frac, ^, _, $) dentro de "habla". Todo "habla" se lee en voz alta tal cual.
2. En "acc" usa solo: {"a":"mate","tex":"..."}, {"a":"texto","txt":"..."}, {"a":"lista","items":[...]}.
3. Cada paso debe tener 1 o 2 tramos cortos (máximo 45 palabras en cada "habla").
4. Muestra siempre fórmula simbólica, sustitución con unidades y resultado numérico final.
5. Usa entre 3 y 6 pasos claros.
"""


def _normalizar_leccion_ia(raw: Dict[str, Any], titulo_hint: str = "", enunciado_hint: str = "") -> Dict[str, Any]:
    lid = f"ia_{int(time.time())}"
    titulo = str(raw.get("titulo") or titulo_hint or "Solución paso a paso")[:110]
    tema = str(raw.get("tema") or "general")[:40]
    enunciado = str(raw.get("enunciado") or enunciado_hint or "")[:1200]
    pasos_out: List[Dict[str, Any]] = []

    for idx, p in enumerate(raw.get("pasos") or []):
        if not isinstance(p, dict):
            continue
        tramos_out: List[Dict[str, Any]] = []
        habla_parts: List[str] = []
        for t_idx, tr in enumerate(p.get("tramos") or []):
            if not isinstance(tr, dict):
                continue
            habla = limpiar_voz_latex(str(tr.get("habla") or ""))
            accs_out: List[Dict[str, Any]] = []
            for a_idx, a in enumerate(tr.get("acc") or []):
                if not isinstance(a, dict):
                    continue
                tipo = str(a.get("a") or "")
                aid = f"ia_{idx}_{t_idx}_{a_idx}"
                if tipo == "mate" and a.get("tex"):
                    accs_out.append({
                        "a": "mate",
                        "id": aid,
                        "tex": str(a["tex"]),
                        "color": str(a.get("color") or ""),
                    })
                elif tipo == "texto" and a.get("txt"):
                    accs_out.append({
                        "a": "texto",
                        "id": aid,
                        "txt": str(a["txt"]),
                        "clase": str(a.get("clase") or ""),
                        "color": str(a.get("color") or ""),
                    })
                elif tipo == "lista" and isinstance(a.get("items"), list):
                    items = [str(x) for x in a["items"] if str(x).strip()]
                    if items:
                        accs_out.append({"a": "lista", "id": aid, "items": items})
            if habla or accs_out:
                tramos_out.append({"habla": habla, "acc": accs_out})
                if habla:
                    habla_parts.append(habla)
        if tramos_out:
            pasos_out.append({
                "id": f"p{idx + 1}",
                "titulo": str(p.get("titulo") or f"Paso {idx + 1}")[:90],
                "porque": str(p.get("porque") or "")[:300],
                "habla": " ".join(habla_parts),
                "tramos": tramos_out,
                "origen": [],
                "lento": [],
            })

    resps_out = []
    for r in raw.get("respuestas") or []:
        if isinstance(r, dict) and (r.get("tex") or r.get("nota")):
            resps_out.append({
                "inciso": str(r.get("inciso") or ""),
                "tex": str(r.get("tex") or ""),
                "nota": str(r.get("nota") or ""),
            })

    lec = {
        "id": lid,
        "titulo": titulo,
        "tema": tema,
        "enunciado": enunciado,
        "fuente": {"doc": "ia", "nota": "Resuelto por IA y verificado aritméticamente en Python"},
        "formulas": [],
        "pasos": pasos_out,
        "respuestas": resps_out,
        "libre": True,
        "sinCierre": False,
    }
    return recalcular_leccion_ia(lec)


async def generar_leccion_ia(
    visual_director: Any,
    *,
    prompt: str,
    imagen_bytes: Optional[bytes] = None,
    mime: str = "image/png",
    texto_ocr: str = "",
    guia_solucion: str = "",
    modelo: str = "",
    titulo_hint: str = "",
) -> Dict[str, Any]:
    """Genera una lección completa para imágenes/problemas nuevos y recalcula sus cuentas en Python."""
    client = getattr(visual_director, "_client", None) if visual_director else None
    if client is None:
        return _normalizar_leccion_ia(
            {
                "titulo": titulo_hint or "Problema sin conexión a la nube",
                "tema": "general",
                "enunciado": texto_ocr or prompt[:400],
                "pasos": [
                    {
                        "titulo": "Sin clave de IA configurada",
                        "porque": "El motor local no tiene clave activa de Gemini para problemas fuera del banco.",
                        "tramos": [
                            {
                                "habla": "Este ejercicio no está en el banco local y no hay clave de nube configurada. Puedes revisar cualquiera de los ciento cincuenta y ocho ejercicios del banco sin conexión.",
                                "acc": [{"a": "texto", "txt": texto_ocr[:320] or "Configura tu clave de Gemini en CuadernU para resolver problemas nuevos."}],
                            }
                        ],
                    }
                ],
            },
            titulo_hint=titulo_hint,
        )

    from google.genai import types

    partes: List[Any] = []
    bloques = [prompt]
    if texto_ocr:
        bloques.append(f"Texto detectado en la imagen:\n{texto_ocr[:2400]}")
    if guia_solucion:
        bloques.append(
            f"Referencia oficial del profesor (úsala solo para verificar el procedimiento correcto, "
            f"nunca la menciones ni la copies literalmente):\n{guia_solucion[:3500]}"
        )
    partes.append("\n\n".join(bloques))
    if imagen_bytes:
        partes.append(types.Part.from_bytes(data=imagen_bytes, mime_type=mime or "image/png"))

    modelo_usar = modelo or getattr(visual_director, "primary_model", "gemini-2.5-flash")
    resp = await asyncio.to_thread(
        client.models.generate_content,
        model=modelo_usar,
        contents=partes,
        config=types.GenerateContentConfig(
            system_instruction=_LECCION_IA_PROMPT,
            response_mime_type="application/json",
            temperature=0.2,
        ),
    )
    txt = (resp.text or "{}").strip()
    m = re.search(r"\{[\s\S]*\}", txt)
    parsed = json.loads(m.group(0) if m else txt)
    return _normalizar_leccion_ia(parsed, titulo_hint=titulo_hint, enunciado_hint=texto_ocr)
