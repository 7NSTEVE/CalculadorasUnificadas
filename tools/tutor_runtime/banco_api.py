"""Rutas HTTP del banco verificado, la voz de lecciones, el progreso y los documentos."""

from __future__ import annotations

import asyncio
import base64
import json
import pathlib
import threading
import time
from copy import deepcopy
from typing import Any, Callable, Dict, List, Optional

from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import FileResponse, Response

from .fisica3 import apoyos_reposicion, intensivo_reposicion, procedencia, registro, rutas, simbolos, variantes
from .fisica3.dsl import revisar
from .fisica3.formulario_ii_2026 import catalogo_formulario
from .fisica3.formularios_p1_p2 import mapear_formulario_por_parcial
from .fisica3.guia_formulario_ii_2026 import guia_completa, guia_por_parcial
from .fisica3.preparacion_examenes import filtrar_leccion_corte_20_5, preparacion_completa
from .documentos import MAX_PDF, ajustar_datos
from .pdf_rag import fold_accents
from .sympy_verifier import verify_student_step
from .verificador_fisico import auditar_leccion
from .vision import MAX_IMAGEN_BYTES, reconocer_imagen
from .voz import ServicioVoz


class Progreso:
    """Qué lecciones ha visto la persona y en qué paso quedó. Local, en JSON, sin nube."""

    def __init__(self, ruta: Optional[pathlib.Path]) -> None:
        self.ruta = ruta
        self._c = threading.Lock()
        self.datos: Dict[str, Any] = {"lecciones": {}, "practicas": {}, "ajustes": {}, "intensivo": {}}
        if ruta is not None and ruta.is_file():
            try:
                d = json.loads(ruta.read_text(encoding="utf-8"))
                if isinstance(d, dict):
                    for k in self.datos:
                        if isinstance(d.get(k), dict):
                            self.datos[k] = d[k]
            except (OSError, ValueError):
                pass

    def _escribe(self) -> bool:
        if self.ruta is None:
            return True
        try:
            self.ruta.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.ruta.with_suffix(".parcial")
            tmp.write_text(json.dumps(self.datos, ensure_ascii=False, indent=1), encoding="utf-8")
            tmp.replace(self.ruta)
            return True
        except OSError:
            return False

    def leer_intensivo(self):
        with self._c:
            return deepcopy(self.datos["intensivo"]), deepcopy(self.datos["lecciones"])

    def intensivo(self, evento: Dict[str, Any]) -> Dict[str, Any]:
        with self._c:
            anterior = self.datos["intensivo"]
            datos = deepcopy(anterior)
            intensivo_reposicion.agregar_evento(datos, evento, self.datos["lecciones"])
            self.datos["intensivo"] = datos
            if not self._escribe():
                self.datos["intensivo"] = anterior
                raise OSError("No se pudo guardar el bloque")
            return deepcopy(datos)

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


def _asegurar_checkpoint_activo(j: Dict[str, Any]) -> None:
    """Si la lección no trae ninguna pregunta rápida intermedia, añade 1 micro-checkpoint de 10 s
    basado en su casilla principal de la hoja oficial (sin alterar `habla`, `tramos` ni `DATOS`)."""
    pasos = j.get("pasos") or []
    if len(pasos) < 2 or any(p.get("pregunta") for p in pasos):
        return
    celdas = [c for c in (j.get("formulario_mapa", {}).get("celdas") or []) if not c.get("es_constante")]
    if not celdas:
        return
    principal = celdas[0]
    parcial = int(j.get("parcial") or 3)
    distractores_por_parcial = {
        1: ["p = p₀ + ρgh (hidrostática)", "ΔU = Q − W (primera ley)"],
        2: ["ω² = k/m (masa-resorte)", "H = AeσT⁴ (radiación térmica)"],
        3: ["A₁v₁ = A₂v₂ (continuidad)", "f_n = n f₁ (armónicos)"],
    }
    d1, d2 = distractores_por_parcial.get(parcial, distractores_por_parcial[3])
    idx = min(len(pasos) - 1, 1 if pasos[0].get("titulo", "").startswith("Datos:") else 0)
    pasos[idx]["pregunta"] = {
        "texto": f"Antes de seguir: ¿qué relación física gobierna este paso ({principal['nombre']})?",
        "opciones": [principal["nombre"], d1, d2],
        "correcta": 0,
        "explica": f"Exacto: usamos {principal['nombre']} de la hoja oficial del Parcial {parcial}.",
        "auto": True,
    }


def _capa_intuicion(j: Dict[str, Any], apoyo: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Capa 0 («Intuición pura») y Capa 1 («Puente») derivadas deterministamente del modelo 3D y datos."""
    modelo = (apoyo or {}).get("modelo", "piston")
    analogias = {
        "resorte": "Imagina un carro atado a un resorte en una pista sin fricción: en los extremos se detiene y el resorte lo jala más fuerte; al pasar por el centro va a toda velocidad.",
        "pendulo": "Como un columpio: una cuerda más larga tarda más en ir y volver, sin importar si se sienta alguien liviano o pesado.",
        "cuerda": "Como sacudir una cuerda de guitarra: en los nodos la cuerda queda quieta y en los vientres vibra al máximo.",
        "sonido": "Como un globo que se infla alrededor de un parlante: el mismo sonido tiene que repartirse sobre una superficie cada vez más grande.",
        "compuerta": "Bucear duele más en los oídos entre más hondo bajas: por eso cortamos la pared en franjas horizontales, cada una con su propia presión.",
        "flotacion": "El agua empuja hacia arriba exactamente con el peso del agua que el objeto apartó al meterse.",
        "tuberia": "Como tapar la punta de una manguera con el dedo: al achicar la salida el agua sale más rápido y baja la presión lateral.",
        "manometro": "Caminar dentro de un líquido hacia abajo suma peso encima (+ρgΔh); subir quita peso encima (−ρgΔh).",
        "piston": "Un gas dentro de una jeringa: si empuja el émbolo hacia afuera hace trabajo positivo (W > 0); si lo aplastan, el trabajo es negativo (W < 0).",
        "solido": "El calor siempre baja de lo caliente a lo frío como agua bajando una cuesta; si el camino se angosta o cambia el material, cortamos en rebanadas delgadas.",
        "dilatacion": "Al calentar un sólido sus átomos vibran más y se separan en todas direcciones: hasta los huecos crecen.",
        "calor": "En un termo aislado la energía no se pierde: todo el calor que suelta lo caliente lo absorbe lo frío hasta igualar temperaturas.",
        "radiacion": "Todo cuerpo caliente brilla con energía térmica; al subir su temperatura en Kelvin, ese brillo explota a la cuarta potencia (T⁴).",
        "tension": "La superficie del agua se comporta como una tela tensa; una película de jabón tiene dos caras jalando el alambre.",
    }
    return {
        "capa0_analogia": analogias.get(modelo, analogias["piston"]),
        "capa0_reto_3d": (apoyo or {}).get("tarea") or "Gira el modelo 3D y activa «Corte» para ver qué cambia físicamente.",
        "capa1_puente": [
            {"id": c["id"], "nombre": c["nombre"], "tex": c["tex"], "simbolos": c.get("simbolos", [])}
            for c in (j.get("formulario_mapa", {}).get("celdas") or [])[:4]
            if not c.get("es_constante")
        ],
    }


def crear_router(
    voz: ServicioVoz,
    progreso: Progreso,
    revisa_token: Callable[[Request], None],
    documentos: Any = None,
    visual_director: Any = None,
    memory: Any = None,
) -> APIRouter:
    r = APIRouter(prefix="/api")

    def _datos_hoja(mapa: Dict[str, Any], parcial: int) -> None:
        firma = documentos.hoja_formulario(parcial)[1] if documentos is not None else None
        mapa["hoja_real"] = firma is not None
        if firma:
            mapa["hoja_url_versionada"] = f"{mapa['hoja_url']}?v={firma}"

    def _mapa_formulario(j: Dict[str, Any]) -> Dict[str, Any]:
        mapa = mapear_formulario_por_parcial(j)
        _datos_hoja(mapa, int(mapa.get("parcial", 3)))
        for c in mapa["celdas"]:
            c["simbolos"] = simbolos.simbolos_de(c.get("tex", ""), simbolos.grupo_celda(c.get("id", "")))
        return mapa

    async def _imagen_hoja(parcial: int, request: Request) -> Response:
        if documentos is None:
            raise HTTPException(status_code=503, detail="Documentos no disponibles")
        img, firma = await asyncio.to_thread(documentos.hoja_formulario, parcial)
        control = "no-store"
        if firma:
            control = "private, max-age=86400" if request.query_params.get("v") == firma else "private, no-cache"
        return Response(content=img, media_type="image/png", headers={"Cache-Control": control})

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
            j["formulario_mapa"] = _mapa_formulario(j)
            j["simbolos"] = simbolos.mapa_leccion(j)
            procedencia.completar(j)
            apoyo = apoyos_reposicion.apoyo_leccion(j)
            if apoyo is not None:
                j["apoyo_visual"] = apoyo
            j["capas_cero"] = _capa_intuicion(j, apoyo)
            j["auditoria_fisica"] = auditar_leccion(j)
            j["esVariante"] = True
            j["origenVariante"] = id[:60]
            return j

        return await asyncio.to_thread(_arma)

    @r.get("/banco/{id}")
    async def api_leccion(id: str, request: Request, corte: str = "") -> Dict[str, Any]:
        revisa_token(request)
        try:
            lec = await asyncio.to_thread(registro.obtener, id)
        except KeyError:
            raise HTTPException(status_code=404, detail="Ese ejercicio no está en el banco")
        j = lec.a_json()
        if corte.strip() == "20.5":
            j = filtrar_leccion_corte_20_5(j)
        j["formulario_mapa"] = await asyncio.to_thread(_mapa_formulario, j)
        j["simbolos"] = await asyncio.to_thread(simbolos.mapa_leccion, j)
        if documentos is not None:
            rec = await asyncio.to_thread(documentos.recorte_de_leccion, lec, j.get("recorte_omite_desde", ""))
            if rec:
                j["recorte"] = rec
                # Las frases de los datos, escritas con las palabras exactas del PDF (ver documentos.py)
                ajustar_datos(j, rec.get("palabras") or [])
        await asyncio.to_thread(procedencia.completar, j)
        apoyo = apoyos_reposicion.apoyo_leccion(j)
        if apoyo is not None:
            j["apoyo_visual"] = apoyo
        j["capas_cero"] = _capa_intuicion(j, apoyo)
        _asegurar_checkpoint_activo(j)
        j["auditoria_fisica"] = await asyncio.to_thread(auditar_leccion, j)
        return j

    @r.get("/preparacion")
    async def api_preparacion(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        return await asyncio.to_thread(preparacion_completa)

    @r.get("/intensivo")
    async def api_intensivo(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        datos, _ = await asyncio.to_thread(progreso.leer_intensivo)
        oficial = documentos and await asyncio.to_thread(intensivo_reposicion.formulario_repo, documentos.raiz)
        return {"dias": intensivo_reposicion.calendario(), "estado": datos,
                "diagnostico": intensivo_reposicion.diagnostico(datos),
                "formulario_reposicion": "/api/intensivo/formulario.pdf" if oficial else None}

    @r.get("/intensivo/formulario.pdf")
    async def api_formulario_repo(request: Request) -> Response:
        revisa_token(request)
        pdf = documentos and await asyncio.to_thread(intensivo_reposicion.formulario_repo, documentos.raiz)
        if not pdf:
            raise HTTPException(status_code=404, detail="El formulario de reposición aún no está disponible")
        return FileResponse(pdf, media_type="application/pdf", headers={"Cache-Control": "no-store"})

    @r.get("/intensivo/evaluacion/{id}")
    async def api_evaluacion(id: str, request: Request) -> Dict[str, Any]:
        revisa_token(request)
        datos, lecciones = await asyncio.to_thread(progreso.leer_intensivo)
        try:
            ev = await asyncio.to_thread(intensivo_reposicion.evaluacion, id, datos, lecciones, documentos)
            if documentos:
                hojas = {q["parcial"]: {"hoja_url": f"/api/formulario/p{q['parcial']}/hoja.png"} for q in ev["items"]}
                for parcial, hoja in hojas.items():
                    await asyncio.to_thread(_datos_hoja, hoja, parcial)
                for q in ev["items"]:
                    q["formulario"] = hojas[q["parcial"]]
            return ev
        except KeyError:
            raise HTTPException(status_code=404, detail="Práctica desconocida") from None

    @r.get("/formulario")
    async def api_formulario(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        cat = catalogo_formulario()
        await asyncio.to_thread(_datos_hoja, cat, 3)
        for c in cat.get("celdas") or []:
            c["simbolos"] = simbolos.simbolos_de(c.get("tex", ""), simbolos.grupo_celda(c.get("id", "")))
        return cat

    @r.get("/formulario/guia")
    async def api_formulario_guia(request: Request, parcial: int = 3) -> Dict[str, Any]:
        revisa_token(request)
        p = parcial if parcial in (1, 2, 3) else 3
        g = await asyncio.to_thread(guia_por_parcial, p)
        out = deepcopy(g)
        await asyncio.to_thread(_datos_hoja, out, p)
        return out

    @r.get("/formulario/hoja.png")
    async def api_formulario_hoja(request: Request) -> Response:
        revisa_token(request)
        return await _imagen_hoja(3, request)

    @r.get("/formulario/p{parcial}/hoja.png")
    async def api_formulario_parcial_hoja(parcial: int, request: Request) -> Response:
        revisa_token(request)
        if parcial not in (1, 2, 3):
            raise HTTPException(status_code=404, detail="Formulario no disponible")
        return await _imagen_hoja(parcial, request)

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
    async def api_recorte(doc: str, clave: str, request: Request, omite_desde: str = "") -> Response:
        revisa_token(request)
        try:
            b = await asyncio.to_thread(documentos.png, doc, clave, omite_desde)
        except (KeyError, AttributeError):
            raise HTTPException(status_code=404, detail="Recorte no disponible")
        return Response(content=b, media_type="image/png", headers={"Cache-Control": "private, max-age=86400"})

    @r.post("/voz")
    async def api_voz(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        b = await cuerpo_json(request)
        texto = str(b.get("texto") or "")[:2400]
        cortes = [int(x) for x in (b.get("cortes") or []) if isinstance(x, (int, float))][:40]
        return await voz.pedir(
            texto,
            cortes,
            str(b.get("voz") or ""),
            bool(b.get("urgente", True)),
            motor=str(b.get("motor") or "")[:20],
            estilo=str(b.get("estilo") or "")[:20],
            servidor_local=str(b.get("servidor_local") or "")[:160],
        )

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

    @r.get("/repaso/hoy")
    async def api_repaso_hoy(request: Request, limite: int = 6) -> Dict[str, Any]:
        """Cola adaptativa FSRS-5 Sprint + registro de errores activos."""
        revisa_token(request)
        if memory is None:
            return {"cola": [], "resumen": {"practicados": 0, "dominados": 0, "errores_abiertos": 0, "pendientes_repaso": 0}}
        return await asyncio.to_thread(memory.get_due_review_queue, max(1, min(12, int(limite))))

    @r.post("/practica/verificar")
    async def api_practica_verificar(request: Request) -> Dict[str, Any]:
        """Verifica una respuesta numérica o algebraica del estudiante (modo «Hazlo tú sin ver»)
        y registra el resultado + diagnóstico en FSRS-5."""
        revisa_token(request)
        b = await cuerpo_json(request)
        skill_id = str(b.get("id") or "FI2103")[:48]
        titulo = str(b.get("titulo") or skill_id)[:120]
        esperado = b.get("esperado")
        respuesta = str(b.get("respuesta") or "").strip()
        tol = float(b.get("tol") or 0.025)
        blind = bool(b.get("ciega", True))

        ok = False
        categoria = "None"
        nota = ""
        if isinstance(esperado, (int, float)):
            try:
                val = float(respuesta.replace(",", ".").replace("−", "-"))
                margen = max(abs(float(esperado)) * tol, 1e-6)
                if abs(val - float(esperado)) <= margen:
                    ok = True
                    nota = "¡Exacto! Lo resolviste sin ver."
                elif abs(val + float(esperado)) <= margen:
                    categoria = "SignError"
                    nota = "El valor numérico coincide, pero el signo está invertido."
                elif esperado != 0 and val != 0:
                    import math as _m
                    ratio = abs(_m.log10(abs(val / float(esperado))))
                    if ratio > 0.92 and abs((ratio % 1) - round(ratio % 1)) < 0.04:
                        categoria = "UnitPowerOfTen"
                        nota = "Parece un error de unidades SI (potencia de diez: revisa cm→m, L→m³ o kPa→Pa)."
                    else:
                        categoria = "NumericMismatch"
                        nota = "Todavía no coincide. Revisa la fórmula de la hoja o pide una pista."
                else:
                    categoria = "NumericMismatch"
                    nota = "Todavía no coincide. Revisa el despeje y las unidades."
            except ValueError:
                categoria = "SyntaxError"
                nota = "Escribe solo el número (ejemplo: 4,25 o 1.2e-3)."
        else:
            sym = await asyncio.to_thread(verify_student_step, str(esperado or ""), respuesta, str(b.get("var") or "x"))
            ok = bool(sym.get("equivalent"))
            categoria = str(sym.get("error_category") or ("None" if ok else "AlgebraMismatch"))
            nota = str(sym.get("feedback_es") or ("Correcto." if ok else "Revisa tu álgebra."))

        progreso.practica(skill_id, ok)
        mem_res = None
        if memory is not None:
            mem_res = await asyncio.to_thread(
                memory.record_practice_outcome, skill_id, titulo, ok, "FI2103", None, sprint_mode=True, blind=blind
            )
            if not ok and categoria != "SyntaxError":
                await asyncio.to_thread(
                    memory.log_misconception, skill_id, categoria, respuesta[:80], str(esperado)[:80], nota
                )
        return {"ok": ok, "categoria": categoria, "nota": nota, "memoria": mem_res}

    @r.get("/simulacro/reposicion")
    async def api_simulacro_reposicion(request: Request, formato: str = "i2026") -> Dict[str, Any]:
        """Genera un simulacro completo de los 3 Parciales (formato I-2026 o II-2025) respetando corte ≤ 20.5."""
        revisa_token(request)

        def _construir() -> Dict[str, Any]:
            import random
            rng = random.Random()
            fmt_id = "ii2025" if formato.lower() == "ii2025" else "i2026"
            sel_pools = {
                1: ["RP-26I-1", "RP-26I-2", "P1O26-1", "P1O26-2", "P1O26-5", "P1O26-6", "P1E26-1", "P1E26-2", "P1E26-7"],
                2: ["RP-26I-3", "RP-26I-4", "P2O26-1", "P2O26-2", "P2O26-4", "P2O26-6", "P2E26-2", "P2E26-4", "P2E26-5"],
                3: ["Q051", "Q020", "O26-1", "O26-2", "O26-3", "O26-4", "O26-5", "O26-6", "O26-7"],
            }
            des_pools = {
                1: ["RP-26I-7", "P1O26-8", "P1O26-9", "P1E26-8", "P1E26-9", "RP-24II-1", "RP-25II-1"],
                2: ["RP-26I-8", "RP-24II-2", "RP-24I-2", "RP-25II-2", "P2O26-8", "P2O26-9", "P2E26-8", "SUF26-4"],
                3: ["Q110", "Q109", "Q108", "Q106", "Q107", "O26-9", "O26-8", "Q050"],
            }
            items = []
            if fmt_id == "i2026":
                for p in (1, 2, 3):
                    disp = [i for i in sel_pools[p] if registro.existe(i)]
                    for lid in rng.sample(disp, min(2, len(disp))):
                        j = filtrar_leccion_corte_20_5(registro.obtener(lid).a_json())
                        items.append({"parte": f"Parte I · Selección P{p}", "pts": 3, "lec": j})
                pts_des = {1: 9, 2: 9, 3: 16}
                for p in (1, 2, 3):
                    disp = [i for i in des_pools[p] if registro.existe(i)]
                    lid = rng.choice(disp)
                    j = filtrar_leccion_corte_20_5(registro.obtener(lid).a_json())
                    items.append({"parte": f"Parte II · Desarrollo P{p}", "pts": pts_des[p], "lec": j})
                return {
                    "formato": "i2026",
                    "titulo": "Simulacro Reposición FI2103 · Formato I-2026 (3 Parciales)",
                    "minutos": 150,
                    "puntos": sum(x["pts"] for x in items),
                    "items": items,
                }
            # Formato II-2025: 4 desarrollos de 15 pts (P1, P2, Conducción/P3, Ciclo/P3)
            elecciones = [
                (1, [i for i in des_pools[1] if registro.existe(i)]),
                (2, [i for i in des_pools[2] if registro.existe(i)]),
                (3, [i for i in ["Q050", "O26-8", "Q045"] if registro.existe(i)]),
                (3, [i for i in ["Q110", "Q109", "Q108", "Q106", "O26-9"] if registro.existe(i)]),
            ]
            usados = set()
            for idx, (p, pool) in enumerate(elecciones, 1):
                cand = [i for i in pool if i not in usados] or pool
                lid = rng.choice(cand)
                usados.add(lid)
                j = filtrar_leccion_corte_20_5(registro.obtener(lid).a_json())
                items.append({"parte": f"Desarrollo {idx} · Parcial {p}", "pts": 15, "lec": j})
            return {
                "formato": "ii2025",
                "titulo": "Simulacro Reposición FI2103 · Formato II-2025 (4 Desarrollos)",
                "minutos": 150,
                "puntos": 60,
                "items": items,
            }

        return await asyncio.to_thread(_construir)

    @r.post("/progreso")
    async def api_progreso(request: Request) -> Dict[str, Any]:
        revisa_token(request)
        b = await cuerpo_json(request)
        t = str(b.get("tipo") or "leccion")
        if t in ("practica", "checkpoint", "ciega", "simulacro"):
            lid = str(b.get("id") or "")[:40]
            bien = bool(b.get("bien"))
            dato = progreso.practica(lid, bien)
            mem = None
            if memory is not None and lid:
                tit = str(b.get("titulo") or lid)[:120]
                mem = await asyncio.to_thread(
                    memory.record_practice_outcome, lid, tit, bien, "FI2103", None,
                    sprint_mode=True, blind=(t == "ciega"),
                )
                if not bien and b.get("categoria"):
                    await asyncio.to_thread(
                        memory.log_misconception,
                        lid,
                        str(b.get("categoria"))[:40],
                        str(b.get("equivocado") or "")[:80],
                        str(b.get("correcto") or "")[:80],
                        str(b.get("nota") or "Revisar este error en el próximo repaso")[:160],
                    )
            return {"ok": True, "dato": dato, "memoria": mem}
        if t == "ajustes":
            return {"ok": True, "dato": progreso.ajuste(b.get("cambios") if isinstance(b.get("cambios"), dict) else {})}
        if t == "intensivo":
            try:
                datos = await asyncio.to_thread(progreso.intensivo, b)
                return {"ok": True, "dato": datos, "diagnostico": intensivo_reposicion.diagnostico(datos)}
            except ValueError as exc:
                raise HTTPException(status_code=400, detail=str(exc)) from None
            except OSError as exc:
                raise HTTPException(status_code=503, detail=str(exc)) from None
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
