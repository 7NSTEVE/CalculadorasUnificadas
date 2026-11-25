"""Rutas HTTP del banco verificado, la voz de lecciones, el progreso y los documentos."""

from __future__ import annotations

import asyncio
import base64
import json
import pathlib
import threading
import time
from typing import Any, Callable, Dict, List, Optional

from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import Response

from .fisica3 import registro, rutas, variantes
from .fisica3.dsl import revisar
from .documentos import MAX_PDF
from .pdf_rag import fold_accents
from .verificador_fisico import auditar_leccion
from .vision import MAX_IMAGEN_BYTES, reconocer_imagen
from .voz import ServicioVoz


class Progreso:
    """Qué lecciones ha visto la persona y en qué paso quedó. Local, en JSON, sin nube."""

    def __init__(self, ruta: Optional[pathlib.Path]) -> None:
        self.ruta = ruta
        self._c = threading.Lock()
        self.datos: Dict[str, Any] = {"lecciones": {}, "practicas": {}, "ajustes": {}}
        if ruta is not None and ruta.is_file():
            try:
                d = json.loads(ruta.read_text(encoding="utf-8"))
                if isinstance(d, dict):
                    for k in self.datos:
                        if isinstance(d.get(k), dict):
                            self.datos[k] = d[k]
            except (OSError, ValueError):
                pass

    def _escribe(self) -> None:
        if self.ruta is None:
            return
        try:
            self.ruta.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.ruta.with_suffix(".parcial")
            tmp.write_text(json.dumps(self.datos, ensure_ascii=False, indent=1), encoding="utf-8")
            tmp.replace(self.ruta)
        except OSError:
            pass

    def anota(self, id: str, paso: int, total: int, hecha: bool) -> Dict[str, Any]:
        with self._c:
            e = self.datos["lecciones"].setdefault(id, {"paso": 0, "total": total, "hecha": False, "veces": 0})
            e["paso"] = max(0, min(int(paso), max(1, int(total))))
            e["total"] = int(total)
            e["cuando"] = int(time.time())
            if hecha:
                e["hecha"] = True
                e["veces"] = int(e.get("veces", 0)) + 1
            self._escribe()
            return dict(e)

    def practica(self, id: str, bien: bool) -> Dict[str, Any]:
        with self._c:
            e = self.datos["practicas"].setdefault(id, {"bien": 0, "mal": 0})
            e["bien" if bien else "mal"] += 1
            e["cuando"] = int(time.time())
            self._escribe()
            return dict(e)

    def ajuste(self, cambios: Dict[str, Any]) -> Dict[str, Any]:
        with self._c:
            for k, v in cambios.items():
                if isinstance(k, str) and len(k) < 40 and isinstance(v, (str, int, float, bool)):
                    self.datos["ajustes"][k] = v
            self._escribe()
            return dict(self.datos["ajustes"])


def _plano(s: str) -> str:
    import re

    s = re.sub(r"\\[A-Za-z]+|[{}$^_\\]", " ", s or "")
    return fold_accents(s)


def catalogo() -> List[Dict[str, Any]]:
    out = []
    for i in registro.ids():
        r = registro.resumen(i)
        if r:
            out.append(r)
    return out


def buscar(q: str, limite: int = 12) -> List[Dict[str, Any]]:
    toks = [t for t in _plano(q).split() if len(t) > 1]
    if not toks:
        return []
    puntos = []
    for i in registro.ids():
        lec = registro.obtener(i)
        titulo = _plano(lec.titulo)
        claves = _plano(" ".join(lec.claves))
        cuerpo = _plano(lec.enunciado)
        p = 0.0
        for t in toks:
            raiz = t[:5] if len(t) > 5 else t
            if t == i.lower():
                p += 20
            if raiz in titulo:
                p += 3
            if raiz in claves:
                p += 2
            if raiz in cuerpo:
                p += 1
        if p > 0:
            puntos.append((p, i))
    puntos.sort(key=lambda x: (-x[0], x[1]))
    return [r for r in (registro.resumen(i) for _, i in puntos[:limite]) if r]


async def cuerpo_json(request: Request) -> Dict[str, Any]:
    """Cuerpo JSON de la petición; si viene mal formado responde 400 (nunca un error interno)."""
    try:
        d = await request.json()
    except (ValueError, UnicodeDecodeError):
        raise HTTPException(status_code=400, detail="Petición mal formada")
    if not isinstance(d, dict):
        raise HTTPException(status_code=400, detail="Petición mal formada")
    return d


def _entero(x: Any, por_defecto: int = 0) -> int:
    try:
        return int(x)
    except (TypeError, ValueError):
        return por_defecto


def crear_router(
    voz: ServicioVoz,
    progreso: Progreso,
    revisa_token: Callable[[Request], None],
    documentos: Any = None,
    visual_director: Any = None,
) -> APIRouter:
    r = APIRouter(prefix="/api")

    @r.post("/imagen/reconocer")
    async def api_imagen_reconocer(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        b = await cuerpo_json(request)
        b64 = str(b.get("imagen") or "")
        mime = str(b.get("tipo") or "image/png")[:40]
        if not b64:
            raise HTTPException(status_code=400, detail="Falta la imagen")
        if len(b64) > (MAX_IMAGEN_BYTES * 4) // 3 + 128:
            raise HTTPException(status_code=413, detail="La imagen pasa de 12 MB")
        try:
            raw = base64.b64decode(b64, validate=False)
        except Exception:
            raise HTTPException(status_code=400, detail="Imagen en base64 inválida")
        if len(raw) > MAX_IMAGEN_BYTES:
            raise HTTPException(status_code=413, detail="La imagen pasa de 12 MB")
        try:
            return await asyncio.to_thread(reconocer_imagen, raw, mime, documentos, visual_director)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)[:200])

    @r.get("/banco")
    async def api_banco(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        return {
            "lecciones": await asyncio.to_thread(catalogo),
            "rutas": await asyncio.to_thread(rutas.publicas),
            "progreso": progreso.datos,
        }

    @r.get("/banco/buscar")
    async def api_buscar(request: Request, q: str = "") -> Dict[str, Any]:
        revisa_token(request)
        return {"resultados": await asyncio.to_thread(buscar, q[:200])}

    @r.get("/banco/variante/{id}")
    async def api_variante(id: str, request: Request, semilla: Optional[int] = None) -> Dict[str, Any]:
        revisa_token(request)

        def _arma() -> Dict[str, Any]:
            lec = variantes.generar_variante(id[:60], semilla=semilla)
            j = lec.a_json()
            j["auditoria_fisica"] = auditar_leccion(j)
            j["esVariante"] = True
            j["origenVariante"] = id[:60]
            return j

        return await asyncio.to_thread(_arma)

    @r.get("/banco/{id}")
    async def api_leccion(id: str, request: Request) -> Dict[str, Any]:
        revisa_token(request)
        try:
            lec = await asyncio.to_thread(registro.obtener, id)
        except KeyError:
            raise HTTPException(status_code=404, detail="Ese ejercicio no está en el banco")
        j = lec.a_json()
        j["auditoria_fisica"] = await asyncio.to_thread(auditar_leccion, j)
        if documentos is not None:
            rec = await asyncio.to_thread(documentos.recorte_de_leccion, lec)
            if rec:
                j["recorte"] = rec
        return j

    # -------------------------------------------------------- documentos

    @r.get("/documentos")
    async def api_documentos(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        return {"documentos": await asyncio.to_thread(documentos.lista) if documentos else []}

    @r.get("/documentos/resolver")
    async def api_resolver(request: Request, q: str = "") -> Dict[str, Any]:
        revisa_token(request)
        d = await asyncio.to_thread(documentos.resolver, q[:200]) if documentos else None
        return d or {}

    @r.post("/documentos/subir")
    async def api_subir(request: Request, nombre: str = "documento.pdf") -> Dict[str, Any]:
        revisa_token(request)
        if documentos is None:
            raise HTTPException(status_code=503, detail="Documentos no disponibles")
        largo = _entero(request.headers.get("content-length"))
        if largo > MAX_PDF:
            raise HTTPException(status_code=413, detail="El PDF pasa de 60 MB")
        datos = bytearray()
        async for trozo in request.stream():
            datos.extend(trozo)
            if len(datos) > MAX_PDF:
                raise HTTPException(status_code=413, detail="El PDF pasa de 60 MB")
        try:
            return await asyncio.to_thread(documentos.subir, nombre[:120], bytes(datos))
        except (ValueError, RuntimeError) as exc:
            raise HTTPException(status_code=400, detail=str(exc)[:200])

    @r.get("/documentos/{doc}")
    async def api_documento(doc: str, request: Request) -> Dict[str, Any]:
        revisa_token(request)
        try:
            return await asyncio.to_thread(documentos.resumen, doc)
        except (KeyError, AttributeError):
            raise HTTPException(status_code=404, detail="Ese documento no está disponible")

    @r.get("/documentos/{doc}/problema/{clave}")
    async def api_problema(doc: str, clave: str, request: Request) -> Dict[str, Any]:
        revisa_token(request)

        def _arma() -> Dict[str, Any]:
            p = documentos.problema(doc, clave)
            return {"doc": doc, "clave": p.clave, "etiqueta": documentos.etiqueta(doc, p.clave), "texto": p.texto[:1500],
                    "leccion": documentos.leccion_de(doc, p.clave), "recorte": documentos.recorte(doc, p.clave)}

        try:
            return await asyncio.to_thread(_arma)
        except (KeyError, AttributeError):
            raise HTTPException(status_code=404, detail="Ese problema no está en el documento")

    @r.get("/documentos/{doc}/recorte/{clave}.png")
    async def api_recorte(doc: str, clave: str, request: Request) -> Response:
        revisa_token(request)
        try:
            b = await asyncio.to_thread(documentos.png, doc, clave)
        except (KeyError, AttributeError):
            raise HTTPException(status_code=404, detail="Recorte no disponible")
        return Response(content=b, media_type="image/png", headers={"Cache-Control": "private, max-age=86400"})

    @r.post("/voz")
    async def api_voz(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        b = await cuerpo_json(request)
        texto = str(b.get("texto") or "")[:2400]
        cortes = [int(x) for x in (b.get("cortes") or []) if isinstance(x, (int, float))][:40]
        return await voz.pedir(texto, cortes, str(b.get("voz") or ""), bool(b.get("urgente", True)))

    @r.get("/voz/audio/{clave}")
    async def api_audio(clave: str, request: Request) -> Response:
        revisa_token(request)
        d = await asyncio.to_thread(voz.leer, clave)
        if not d:
            raise HTTPException(status_code=404, detail="Audio no disponible")
        mime = str((d[1] or {}).get("mime") or "audio/wav")
        return Response(content=d[0], media_type=mime, headers={"Cache-Control": "private, max-age=86400"})

    @r.get("/voz/estado")
    async def api_voz_estado(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        return voz.estado()

    @r.post("/progreso")
    async def api_progreso(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        b = await cuerpo_json(request)
        t = str(b.get("tipo") or "leccion")
        if t == "practica":
            return {"ok": True, "dato": progreso.practica(str(b.get("id") or "")[:40], bool(b.get("bien")))}
        if t == "ajustes":
            return {"ok": True, "dato": progreso.ajuste(b.get("cambios") if isinstance(b.get("cambios"), dict) else {})}
        return {"ok": True, "dato": progreso.anota(str(b.get("id") or "")[:40], _entero(b.get("paso")), max(1, _entero(b.get("total"), 1)), bool(b.get("hecha")))}

    @r.get("/banco-revision")
    async def api_revision(request: Request) -> Dict[str, Any]:
        """Autodiagnóstico: faltas de calidad de todas las lecciones."""
        revisa_token(request)

        def _todo() -> Dict[str, List[str]]:
            return {i: f for i in registro.ids() for f in [revisar(registro.obtener(i))] if f}

        return {"faltas": await asyncio.to_thread(_todo)}

    @r.get("/banco-auditoria")
    async def api_auditoria(request: Request) -> Dict[str, Any]:
        """Auditoría física de 10 niveles sobre todas las lecciones del banco."""
        revisa_token(request)

        def _todo() -> Dict[str, Any]:
            alertas: Dict[str, List[str]] = {}
            total = 0
            for i in registro.ids():
                total += 1
                aud = auditar_leccion(registro.obtener(i).a_json())
                if aud["alertas"]:
                    alertas[i] = aud["alertas"]
            return {"total": total, "alertas": alertas}

        return await asyncio.to_thread(_todo)

    return r
