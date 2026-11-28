"""Correspondencias 3D interactivas entre enunciados, incisos, datos y pasos de TODAS las lecciones FI2103.

Mapea tanto las 76 lecciones del plan «Reposición II-2026» como el resto del banco (~340 lecciones)
a una de las 12 escenas 3D WebGL2 interactivas, con:
  * parámetros numéricos reales del problema;
  * cámara inicial óptima (`perspectiva`, `frontal`, `corte`);
  * focos sincronizados por paso, por dato subrayado y por cada inciso (A, B, C...).
"""

from __future__ import annotations

import re
import unicodedata
from typing import Any, Dict, List, Optional

_FAMILIAS = {
    "resorte": "RP-26I-7 P1O26-8 P1O26-1 P1O26-7 P1E26-4 P1E26-6 P1O26-5 RP-24II-1 RP-25II-1 T-MAS T-BASE3",
    "pendulo": "P1E26-8 RP-26I-2 P1O26-3 P1E26-1 T-PEND",
    "cuerda": "P1O26-6 P1E26-9 P1O26-4 P1E26-2 P1E26-3 P1E26-5 RP-26I-1 P1E26-7 P1O26-9 RP-23II-1 RP-24I-1 T-ONDA",
    "sonido": "P1O26-2 T-SON",
    "flotacion": "RP-26I-3 P2O26-2 P2E26-1 P2O26-7 P2O26-6 T-FLOT",
    "tuberia": "RP-26I-4 P2O26-4 P2O26-3 RP-26I-8 RP-24II-2 P2O26-5 P2E26-3 P2E26-6 P2E26-7 "
               "P2E26-5 P2O26-9 P2E26-9 P2E26-4 SUF26-4 RP-23II-2 T-BERN T-VISC",
    "compuerta": "E2-8 SUF26-3 P2E26-8 RP-24I-2 RP-25II-2 T-PARED T-INT0 T-INT1 T-PLANTEO T-BASE4",
    "manometro": "P2O26-8 T-HIDRO",
    "piston": "O26-4 O26-6 O26-7 O26-2 O26-1 T-GAS T-PV T-MAQ T-ENT",
    "solido": "Q045 O26-8 Q051 Q050 T-CONO O26-5 T-CON T-INT2 T-MAT1 T-MAT2 T-MAT3",
    "dilatacion": "O26-3 T-DIL",
    "calor": "O26-10 Q020 T-CAL",
    "radiacion": "T-RAD",
    "tension": "P2E26-2 P2O26-1",
}
_POR_ID = {lid: familia for familia, ids in _FAMILIAS.items() for lid in ids.split()}

_ESCENAS = {
    "resorte": ("Resorte 3D, fase φ y energía", "Gira el modelo 3D y mira cómo el círculo fasorial proyecta x(t)."),
    "pendulo": ("Péndulo 3D: pivote y centro de masa", "Compara largos y ubica la distancia d del pivote al centro de masa."),
    "cuerda": ("Cuerda vibrante y ondas 3D", "Cambia el armónico n y ubica nodos, antinodos y el traste."),
    "sonido": ("Frentes de onda sonora 3D", "Observa cómo la potencia P se reparte sobre esferas 4πr²."),
    "flotacion": ("Tanque de flotación 3D", "Compara líquidos y observa Empuje F_B frente al Peso mg."),
    "tuberia": ("Tubería, Venturi y Poiseuille 3D", "Activa «Corte» para ver las partículas acelerar al angostarse el tubo."),
    "compuerta": ("Represa y compuerta 3D (integral)", "Mueve la franja dA = L(h)dh o suma N franjas en 3D."),
    "manometro": ("Manómetro multilíquido 3D", "Recorre cada líquido: bajar suma +ρgΔh y subir resta −ρgΔh."),
    "piston": ("Cilindro-Pistón 3D ↔ Diagrama p–V", "Observa el pistón moverse en sincronía con el ciclo p–V."),
    "solido": ("Sólido en conducción 3D (corte)", "Activa «Corte» y desliza el disco o cascarón diferencial."),
    "dilatacion": ("Dilatación térmica 3D", "Calienta el sólido y observa cómo crece cada dimensión (y los huecos)."),
    "calor": ("Calorimetría 3D (mezcla aislada)", "Sigue el flujo de calor desde el cuerpo caliente al frío hasta T_f."),
    "radiacion": ("Radiación térmica 3D (T⁴)", "Sube la temperatura en Kelvin y mira el crecimiento a la cuarta potencia."),
    "tension": ("Tensión superficial y elasticidad 3D", "Observa las dos caras de la película o la deformación elástica."),
}

_CAMARA_INICIAL = {
    "solido": "corte",
    "tuberia": "perspectiva",
    "piston": "corte",
    "compuerta": "perspectiva",
    "manometro": "frontal",
    "cuerda": "perspectiva",
}

_FOCOS = {
    "resorte": (("masa", "Masa", "masa peso bloque"), ("resorte", "Resorte", "constante resorte elastica"),
                ("equilibrio", "Equilibrio y fase φ", "equilibrio amplitud extrema fase posicion"),
                ("energia", "Energía", "energia cinetica potencial")),
    "pendulo": (("pivote", "Pivote", "pivote eje inercia"), ("centro", "Centro de masa", "centro masa"),
                ("largo", "Largo y período", "longitud largo periodo")),
    "cuerda": (("extremos", "Extremos y traste", "largo longitud extremo traste"),
               ("nodos", "Nodos y antinodos", "nodo antinodo armonico fundamental"),
               ("onda", "Onda y viajeras", "velocidad rapidez potencia superposicion pulso frecuencia lambda tension")),
    "sonido": (("fuente", "Fuente sonora", "fuente potencia"), ("onda", "Distancia r e intensidad", "umbral intensidad distancia")),
    "flotacion": (("superficie", "Superficie y volumen", "hundido volumen superficie profundidad"),
                  ("fuerzas", "Empuje y peso", "empuje peso equilibrio resistencia terminal tension")),
    "tuberia": (("entrada", "Sección 1", "principal entrada ancha superior caudal flujo"),
                ("salida", "Sección 2", "secundaria angosta salida orificio baja inferior"),
                ("presion", "Presión y alturas", "presion altura nivel bernoulli remanso")),
    "compuerta": (("superficie", "Superficie del agua", "superficie profundidad altura presion"),
                  ("franja", "Franja dA = L(h)dh", "franja ancho integral fuerza area borde")),
    "manometro": (("A", "Punto A", "presion dada punto a"), ("agua", "Agua/Hg", "agua mercurio"),
                  ("mercurio", "Hg/Glicerina", "mercurio glicerina"),
                  ("aceite", "Glicerina/X", "glicerina liquido densidad"),
                  ("B", "Punto B", "absoluta punto b"), ("C", "Punto C", "punto c")),
    "piston": (("volumen", "Volumen y trabajo W", "volumen trabajo expansion compresion"),
               ("presion", "Estados p-V-T", "estado tabla presion temperatura"),
               ("energia", "Calor y eficiencia", "calor trabajo eficiencia neto cop rendimiento")),
    "solido": (("limites", "Caras T₁ y T₂", "limites planos extremos caras temperaturas"),
               ("corte", "Disco / Cascarón", "radio area disco franja cascaron integral"),
               ("calor", "Flujo de calor H", "flujo resistencia potencia temperatura calor")),
    "dilatacion": (("calentar", "Expansión ΔL / ΔV", "dilatacion calienta largo volumen hueco alfa beta"),),
    "calor": (("equilibrio", "Equilibrio térmico", "equilibrio mezcla hielo calor temperatura"),),
    "radiacion": (("calentar", "Radiación T⁴", "radiacion emisividad potencia kelvin"),),
    "tension": (("fuerza", "Fuerza aplicada", "fuerza esfuerzo traccion tension"),
                ("deforma", "Deformación", "deformacion longitud elastico")),
}

_PARAMETROS: Dict[str, Dict[str, Any]] = {
    "P1O26-8": {"masa": 0.50, "energia": 2, "cruce": 0.141, "x0": 0.141, "sentido": -1},
    "RP-26I-7": {"masa": 4.53, "k": 4300, "amplitud": 0.0624, "x0": 0.0300, "sentido": 1},
    "RP-24II-1": {"masa": 0.50, "k": 200, "amplitud": 0.10, "x0": -0.10, "sentido": 1},
    "RP-25II-1": {"masa": 0.40, "k": 160, "amplitud": 0.08, "x0": 0.0, "sentido": 1},
    "P1O26-7": {"vertical": True},
    "P1O26-1": {"comparar": "doble_amplitud"},
    "P1O26-5": {"fraccion": 0.5},
    "P1E26-6": {"fraccion": 0.5},
    "P1E26-8": {"largo": 2, "centro": 1.5, "comparar": "fisico"},
    "RP-26I-2": {"comparar": "largo_cuatro"},
    "P1E26-1": {"comparar": "largo_cuarto"},
    "P1O26-3": {"comparar": "centro_cuarto"},
    "P1O26-6": {"armonico": 2},
    "P1E26-9": {"armonico": 4, "nodos_distancia": 0.24, "amplitud": 0.015},
    "P1O26-9": {"largo": 0.9, "frecuencia": 180, "armonico": 3, "traste": True},
    "P1E26-2": {"onda": "viajera", "amplitud": 0.025, "k": 4, "omega": 50},
    "P1E26-5": {"onda": "pulsos"},
    "P1E26-3": {"onda": "comparacion", "frecuencias": [2, 1], "amplitudes": [0.5, 1]},
    "P1O26-4": {"onda": "comparacion"},
    "P1E26-7": {"onda": "comparacion", "tensiones": [1, 4], "densidades": [1, 2]},
    "RP-26I-1": {"armonico": 1, "largo": 1.056},
    "RP-23II-1": {"armonico": 3, "amplitud": 0.0425},
    "RP-24I-1": {"armonico": 1, "traste": True},
    "P1O26-2": {"distancia": 7.5, "intensidad": 0.11, "umbral": 1},
    "RP-26I-3": {"hundido": [0.40, 0.60], "forma": "esfera", "nombres": ["Fluido A", "Fluido B"]},
    "P2O26-2": {"hundido": [0.60, 0.75], "nombres": ["Agua", "Aceite"]},
    "P2E26-1": {"modo": "cable", "forma": "esfera"},
    "P2O26-6": {"modo": "terminal", "forma": "esfera"},
    "P2O26-7": {"modo": "compresion", "profundidad": 2000},
    "P2O26-4": {"diametros": [0.06, 0.03], "rapidez": 2, "modo": "venturi"},
    "RP-24II-2": {"diametros": [0.04, 0.02], "modo": "venturi", "caudal": 0.0008},
    "P2E26-5": {"modo": "venturi"},
    "P2E26-4": {"modo": "viscoso", "igual": True},
    "P2E26-6": {"modo": "pitot", "presiones": [100000, 100600]},
    "P2E26-7": {"modo": "vertical"},
    "SUF26-4": {"modo": "descenso", "desnivel": 2.50, "diametros": [0.04], "rapidez": 2.52},
    "P2E26-9": {"modo": "perfil", "diametros": [0.004, 0.004], "caudal": 1.50e-5},
    "RP-23II-2": {"modo": "perfil", "diametros": [0.012, 0.012]},
    "P2O26-9": {"modo": "ramas", "diametros": [0.0086, 0.0089], "rapidez": 0.2},
    "RP-26I-4": {"modo": "tanque", "caudal_masico": 20, "area": 0.0015},
    "RP-26I-8": {"modo": "tanque", "caudal_masico": 60, "diametros": [0.14]},
    "P2E26-3": {"modo": "tanque"},
    "P2O26-3": {"igual": True, "modo": "venturi"},
    "P2O26-8": {"alturas": {"A": 2.40, "agua": 0.850, "mercurio": 1.35, "aceite": 0.700, "B": 0.200, "C": 1.80, "libre": 2.65},
                "presion": 64, "densidades": [1000, 13600, 1260]},
    "E2-8": {"forma": "semicirculo", "alto": 0.30, "H": 0.60},
    "SUF26-3": {"forma": "rectangulo", "alto": 0.40, "H": 0.80},
    "P2E26-8": {"forma": "ventana", "H": 3.0},
    "RP-24I-2": {"forma": "mixta", "H": 2.0},
    "RP-25II-2": {"forma": "trapecio", "H": 50.0},
    "T-CONO": {"geo": "cono", "r1": 0.01, "r2": 0.03, "L": 0.20, "T1": 100, "T2": 20},
    "O26-8": {"geo": "esfera_seccionada", "R": 0.10, "L": 0.08, "T1": 100, "T2": 20},
    "Q045": {"geo": "cilindro", "r1": 0.04, "r2": 0.08, "T1": 150, "T2": 45},
    "O26-3": {"caso": "barra"},
    "Q051": {"geo": "pared", "T1": 25, "T2": 10},
    "Q050": {"geo": "pared", "T1": 100, "T2": 20},
    "O26-4": {"modo": "isocorico"},
    "O26-6": {"modo": "modos"},
    "O26-7": {"modo": "otto"},
    "P2E26-2": {"modo": "pelicula"},
    "P2O26-1": {"modo": "traccion"},
    "T-MAS": {"masa": 1.0, "k": 100, "amplitud": 0.12, "x0": 0.06, "sentido": 1},
    "T-PEND": {"largo": 1.5, "centro": 1.1, "comparar": "fisico"},
    "T-ONDA": {"armonico": 3},
    "T-SON": {"distancia": 8.0, "intensidad": 0.10, "umbral": 1.0},
    "T-HIDRO": {"alturas": {"A": 2.2, "agua": 0.9, "mercurio": 1.4, "aceite": 0.8, "B": 0.3, "C": 1.7, "libre": 2.5}, "presion": 50},
    "T-FLOT": {"hundido": [0.60, 0.75], "nombres": ["Agua", "Aceite"]},
    "T-BERN": {"diametros": [0.06, 0.03], "modo": "venturi"},
    "T-VISC": {"modo": "perfil", "diametros": [0.01, 0.01]},
}

_INCISOS = re.compile(r"(?<!\S)\(?([a-hA-H])\)\s*")
_SIN_TEX = re.compile(r"\$[^$]*\$")


def _normaliza(s: str) -> str:
    return "".join(c for c in unicodedata.normalize("NFD", s.casefold()) if not unicodedata.combining(c))


def _foco(modelo: str, texto: str) -> Optional[Dict[str, str]]:
    palabras = set(re.findall(r"[a-z]+", _normaliza(_SIN_TEX.sub(" ", texto))))
    temas = _FOCOS.get(modelo, ())
    elegido = max(temas, default=None, key=lambda t: len(palabras.intersection(t[2].split())))
    if elegido is not None and palabras.intersection(elegido[2].split()):
        return {"id": elegido[0], "texto": elegido[1]}
    return None


def _inferir_modelo(lec: Dict[str, Any]) -> str:
    lid = str(lec.get("id") or "")
    if lid in _POR_ID:
        return _POR_ID[lid]
    v = lec.get("visual") or {}
    vtipo = v.get("tipo") if isinstance(v, dict) else ""
    por_visual = {
        "pv": "piston",
        "maquina": "piston",
        "otto": "piston",
        "gas": "piston",
        "distribucion": "piston",
        "entropia": "piston",
        "conduccion": "solido",
        "pared": "compuerta",
        "dilatacion": "dilatacion",
        "calor": "calor",
        "calentamiento": "calor",
        "radiacion": "radiacion",
    }
    if vtipo in por_visual:
        return por_visual[vtipo]

    txt = _normaliza(f"{lec.get('titulo', '')} {lec.get('enunciado', '')} {' '.join(lec.get('claves') or [])}")
    parcial = int(lec.get("parcial") or 3)
    if parcial == 1:
        if any(w in txt for w in ("pendul", "pivote", "varilla", "inercia")):
            return "pendulo"
        if any(w in txt for w in ("sonid", "dron", "intensidad", "decibel", "esferic")):
            return "sonido"
        if any(w in txt for w in ("cuerda", "armonico", "nodo", "estacionar", "pulso", "viajera", "traste")):
            return "cuerda"
        return "resorte"
    if parcial == 2:
        if any(w in txt for w in ("compuerta", "represa", "pared", "ventana", "franja")):
            return "compuerta"
        if any(w in txt for w in ("manometr", "mercurio", "glicerina", "tubo en u")):
            return "manometro"
        if any(w in txt for w in ("flota", "empuje", "arquimedes", "hundid", "stokes", "terminal", "sumerg")):
            return "flotacion"
        if any(w in txt for w in ("superficial", "pelicula", "alambre", "young", "elastico", "esfuerzo")):
            return "tension"
        return "tuberia"

    # Parcial 3 fallback
    tema = str(lec.get("tema") or "")
    if "dilat" in tema or "dilat" in txt:
        return "dilatacion"
    if "radiac" in tema or "stefan" in txt:
        return "radiacion"
    if "calor" in tema or "hielo" in txt or "mezcla" in txt:
        return "calor"
    if "conduc" in tema or "fourier" in txt or "esfera" in txt or "cono" in txt:
        return "solido"
    return "piston"


def _pide(lec: Dict[str, Any], modelo: str, pasos: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    txt = str(lec.get("enunciado") or "")
    plano = _SIN_TEX.sub(lambda m: " " * len(m[0]), txt)
    partes = list(_INCISOS.finditer(plano)) if lec.get("tipo") != "seleccion" else []
    out: List[Dict[str, Any]] = []
    for i, parte in enumerate(partes):
        clave = parte[1].lower()
        fin = partes[i + 1].start() if i + 1 < len(partes) else len(txt)
        texto = txt[parte.end():fin].strip()
        texto = re.split(r"\s*\[Inciso > 20\.5 omitido", texto)[0].strip()
        if not texto or clave == str(lec.get("recorte_omite_desde", "")).lower():
            continue
        paso = next((p for p in pasos if re.match(rf"\(?{clave}\)", p["titulo"], re.I)), pasos[0])
        out.append({
            "id": clave,
            "titulo": f"Inciso {clave.upper()}",
            "texto": texto,
            "paso": paso["n"],
            "foco": _foco(modelo, texto) or paso["foco"],
        })
    if out:
        return out
    if lec.get("tipo") == "teoria":
        return [{
            "id": str(p["n"]),
            "titulo": p["titulo"][:26],
            "texto": p["titulo"],
            "paso": p["n"],
            "foco": p["foco"],
        } for p in pasos[:5]]
    pregunta = re.findall(r"¿[^?]+\?", txt)
    return [{
        "id": "problema",
        "titulo": "Qué piden",
        "texto": pregunta[-1] if pregunta else lec.get("titulo", ""),
        "paso": pasos[0]["n"],
        "foco": _foco(modelo, str(lec.get("titulo") or "")),
    }]


def apoyo_leccion(lec: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Construye la especificación 3D interactiva para cualquier lección del banco FI2103."""
    if not lec or not lec.get("pasos"):
        return None
    lid = str(lec.get("id") or "")
    v = lec.get("visual") if isinstance(lec.get("visual"), dict) else {}
    modelo = _inferir_modelo(lec)
    if modelo not in _ESCENAS:
        modelo = "piston"

    titulo, tarea = _ESCENAS[modelo]
    pasos = [{"n": p["n"], "titulo": p["titulo"], "foco": _foco(modelo, p["titulo"])} for p in lec["pasos"]]
    datos = []
    vistas = set()
    for p in lec["pasos"]:
        for tramo in p.get("tramos") or []:
            for a in tramo.get("acc") or []:
                if a.get("a") != "dato":
                    continue
                texto = a.get("frase_original") or a.get("frase") or ""
                if texto and texto not in vistas:
                    vistas.add(texto)
                    datos.append({"id": a.get("id", ""), "texto": texto, "foco": _foco(modelo, texto)})

    params: Dict[str, Any] = {**v, **_PARAMETROS.get(lid, {})}
    if modelo == "manometro":
        focos = {1: "A", 4: "agua", 5: "mercurio", 6: "aceite", 7: "aceite", 8: "B", 9: "C"}
        nombres = {fid: t for fid, t, _ in _FOCOS[modelo]}
        for p in pasos:
            if p["n"] in focos:
                f = focos[p["n"]]
                p["foco"] = {"id": f, "texto": nombres[f]}

    return {
        "modelo": modelo,
        "motor": "webgl2",
        "titulo": titulo,
        "tarea": tarea,
        "camara_inicial": _CAMARA_INICIAL.get(modelo, "perspectiva"),
        "parametros": params,
        "datos": datos,
        "piden": _pide(lec, modelo, pasos),
        "pasos": pasos,
    }
