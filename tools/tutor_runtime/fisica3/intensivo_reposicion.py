"""Itinerario de seis días y prácticas de reposición, evaluables en el motor local."""

from __future__ import annotations

import time
import unicodedata

from . import registro
from .preparacion_examenes import filtrar_leccion_corte_20_5


def _bloque(tipo, titulo, tarea, leccion="", paso=1, parte=None):
    return {"tipo": tipo, "titulo": titulo, "tarea": tarea, "leccion": leccion,
            "paso": paso, "parte": parte}


def _pizarra(id, titulo, tarea, paso=1):
    return _bloque("pizarra", titulo, tarea, id, paso)


def _evaluar(id, titulo, tarea, parte=None):
    return _bloque("evaluacion", titulo, tarea, id, parte=parte)


def _recuerdo(titulo, tarea):
    return _bloque("recuerdo", titulo, tarea)


def _miniprueba(forma):
    temas = ("Selecciones de los tres parciales", "Desarrollo de oscilaciones y ondas",
             "Desarrollo de fluidos", "Desarrollo de termodinámica")
    return tuple(_evaluar(forma, titulo, "Resolvé con el formulario y la calculadora.", i)
                 for i, titulo in enumerate(temas))


_DIAS = (
    ("2026-11-28", "Bases y primer ciclo", (
        _bloque("diagnostico", "Signos, unidades y despejes", "Contestá una pregunta a la vez.", parte=0),
        _bloque("diagnostico", "Gráficas y formulario", "Probá leer los datos y buscar la ecuación.", parte=1),
        _bloque("base", "Volvé a la primera base que te cuesta", "Seguí el ejemplo básico.", parte=0),
        _bloque("base", "Probá esa base con otro dato", "Hacé el intento antes de ver la corrección.", parte=1),
        _pizarra("Q108", "Ubicá los estados del ciclo", "En el inciso A, ubicá A, B y C y recorré el diagrama."),
        _pizarra("Q108", "Trabajo en el ciclo triangular", "En el inciso A, separá el trabajo de cada tramo y su signo.", 3),
        _evaluar("trabajo", "Trabajo neto por tu cuenta", "Resolvé el inciso A de Q106."),
        _bloque("correccion", "Revisá el trabajo neto", "Compará tu planteo, cuenta y unidades.", "trabajo"),
    )),
    ("2026-11-29", "Ciclos, gases y segunda ley", (
        _recuerdo("Recordá el ciclo triangular", "Dibujá el recorrido de Q108 de memoria y marcá dónde se expande."),
        _recuerdo("Ubicá el trabajo en la hoja", r"Buscá cómo obtener $W$ a partir de un diagrama $p$–$V$."),
        _pizarra("Q110", "Leé el refrigerador", "Ubicá los procesos y qué datos tenés para cada estado."),
        _pizarra("Q110", "Trabajo por proceso", "Elegí la ecuación del trabajo y revisá el signo.", 4),
        _pizarra("Q110", "Calor y energía interna", r"Usá $\Delta U=Q-W$ y distinguí calor que entra del que sale.", 5),
        _pizarra("Q110", "Coeficiente de rendimiento", "Identificá el calor extraído y el trabajo que consume el refrigerador.", 8),
        _evaluar("temperatura", "Temperatura de un ciclo nuevo", "Calculá la temperatura del estado A de Q109."),
        _bloque("correccion", "Revisá la tabla de estados", "Comprobá la ecuación, la constante y sus unidades.", "temperatura"),
    )),
    ("2026-11-30", "Presión, caudal y Bernoulli", (
        _recuerdo("Recordá la primera ley", "Escribí qué signo tiene el trabajo de una expansión y cómo se obtiene el calor."),
        _recuerdo("Ubicá las ecuaciones de fluidos", "Buscá en la hoja cuándo podés usar hidrostática, continuidad y Bernoulli."),
        _pizarra("P2O26-8", "Identificá las interfaces", "Marcá A y las interfaces de agua, mercurio y glicerina."),
        _pizarra("P2O26-8", "Sumá los cambios de presión", "En el inciso A, escribí una diferencia de alturas por cada líquido.", 4),
        _pizarra("RP-26I-8", "Del flujo másico a la rapidez", "Hacé los incisos A y B del tanque."),
        _pizarra("RP-26I-8", "Elegí los puntos de Bernoulli", "Ubicá la superficie libre y el orificio para el inciso C.", 4),
        _evaluar("venturi", "Venturi por tu cuenta", "Obtené la diferencia de presión entre ambas secciones."),
        _bloque("correccion", "Revisá continuidad y presión", "Cotejá los diámetros, sus áreas y el signo obtenido.", "venturi"),
    )),
    ("2026-12-01", "Oscilaciones y ondas", (
        _recuerdo("Recordá los signos en fluidos", "Anotá qué pasa con la presión al bajar en un mismo líquido en reposo."),
        _recuerdo("Buscá la casilla de continuidad", "Escribí qué magnitud se conserva si el tubo se angosta."),
        _pizarra("RP-26I-7", "Período y fase del resorte", "Empezá por el período y después ubicá la posición inicial."),
        _pizarra("P1O26-8", "Leé las curvas de energía", "Ubicá el cruce de energías y la dirección de la velocidad."),
        _pizarra("P1O26-8", "Relacioná A, x, U y K", "Compará la energía en el equilibrio y en los extremos.", 3),
        _pizarra("P1O26-6", "Seguí la cuerda", "Contá los nodos del segundo armónico y comprobá los extremos."),
        _evaluar("pendulo", "Péndulo por tu cuenta", "Decidí qué cambia al cuadruplicar el largo y reducir la masa."),
        _evaluar("ondas", "Dos ondas en la misma cuerda", "Elegí qué afirmaciones son correctas y revisá tu respuesta."),
    )),
    ("2026-12-02", "Conducción y fuerzas integradas", (
        _pizarra("O26-8", "Encontrá el disco de la esfera", "Mové el corte y relacioná su radio con la posición."),
        _pizarra("O26-8", "Elegí límites para conducción", "Ubicá los dos planos y escribí qué se integra.", 4),
        _pizarra("P2E26-8", "Ubicá la ventana en el tanque", "Marcá superficie, centro y bordes de la ventana."),
        _pizarra("P2E26-8", "De presión a fuerza", "Para el inciso C, anotá qué presión corresponde a cada franja.", 7),
        *_miniprueba("A"),
    )),
    ("2026-12-03", "Miniprueba y revisión", (
        *_miniprueba("B"),
        _bloque("correccion", "Corregí la miniprueba A", "Puntuá los tres desarrollos con la guía de corrección.", "A"),
        _bloque("correccion", "Corregí la miniprueba B", "Puntuá los desarrollos y revisá el resultado por parcial.", "B"),
        _bloque("reparacion", "Rehacé el error que más puntos costó", "Resolvelo otra vez e indicá qué corregiste."),
        _evaluar("retencion", "Comprobá qué recordás de conducción", "Intentá una pared compuesta y anotá lo que queda pendiente."),
    )),
)

# En diagnóstico se devuelve la solución del reactivo después del intento.
_BASES = (
    ("signos", "¿Cuánto da $-20/0{,}5$?", ("$-40$", "$-10$", "$40$"), 0,
     r"Dividir entre medio cuenta cuántas mitades caben. $-6/0{,}5=-12$.",
     "¿Cuánto da $-8/0{,}5$?", ("$-4$", "$-16$", "$16$"), 1),
    ("unidades", "¿Cuántos metros son 30 cm?", ("0,030 m", "0,30 m", "3,0 m"), 1,
     r"Un metro tiene 100 cm. Por ejemplo, $50\,\mathrm{cm}=50/100\,\mathrm{m}$.",
     "¿Cuántos metros son 8 cm?", ("0,80 m", "0,008 m", "0,08 m"), 2),
    ("despeje", r"Si $m=\rho V$, ¿cómo se obtiene $\rho$?", ("$mV$", "$V/m$", "$m/V$"), 2,
     r"Para aislar una cantidad que multiplica, dividí los dos lados por el otro factor. Si $6=2x$, entonces $x=6/2$.",
     r"Si $F=pA$, ¿cómo se obtiene $p$?", ("$F/A$", "$FA$", "$A/F$"), 0),
    ("grafica", "En la gráfica del ciclo, seguí A→B. ¿Qué pasa con el volumen?", ("Aumenta", "Disminuye", "Queda fijo"), 0,
     "En un gráfico presión–volumen, la coordenada horizontal es el volumen. Ir a la derecha aumenta el volumen.",
     "Si vas de B a A por ese tramo, ¿qué pasa con el volumen?", ("Queda fijo", "Disminuye", "Aumenta"), 1),
    ("formulario", r"Si conocés el período $T$, ¿qué ecuación te da la frecuencia $f$?",
     ("$f=T$", "$f=T^2$", "$f=1/T$"), 2,
     "Buscá primero la cantidad que piden. La frecuencia cuenta ciclos por segundo y el período son segundos por ciclo.",
     "Si cada ciclo tarda 2 s, ¿cuántos ciclos hay por segundo?", ("2", "0,5", "4"), 1),
    ("condicion", "En un mismo líquido en reposo, ¿en cuál punto es mayor la presión?", ("En el de arriba", "En el más profundo", "Siempre son iguales"), 1,
     "Al bajar en el mismo líquido en reposo, hay más líquido encima. La presión aumenta.",
     "¿Qué pasa con la presión al subir en ese líquido?", ("Disminuye", "Aumenta", "Queda igual"), 0),
)


def _seleccion(id):
    return id, "", "", "", "", ()


def _desarrollo(id, inciso, pide, planteo, desarrollo, *claves):
    return id, inciso, pide, planteo, desarrollo, claves


_PRUEBAS = {
    "A": (
        _seleccion("P1E26-1"), _seleccion("P2O26-6"), _seleccion("O26-3"),
        _desarrollo("P1E26-9", "a", "Determiná el armónico y el largo de la cuerda.",
                    "Relaciona el número de antinodos con el armónico.", r"Usa $\lambda/2$ entre nodos consecutivos y $L=n\lambda/2$.", "a-n", "a-L"),
        _desarrollo("P2O26-9", "a", "Obtené el número de Reynolds de la tubería principal e indicá su régimen.",
                    r"Plantea $\mathrm{Re}=\rho vd/\eta$.", "Convierte el diámetro y justifica la clasificación como laminar.", "a"),
        _desarrollo("O26-9", "a", r"Calculá el cambio de energía interna $\Delta U_{AC}$.",
                    "Identifica el tramo isotérmico y usa el cierre del ciclo.", r"Aplica $\Delta U=Q-W$ con los signos y la conversión de atm·m³ a kJ.", "a"),
    ),
    "B": (
        _seleccion("P1E26-7"), _seleccion("P2O26-1"), _seleccion("O26-1"),
        _desarrollo("P1O26-9", "a", r"Calculá la densidad lineal $\mu$ y la rapidez inicial $v_1$.",
                    "Distingue largo, masa y frecuencia fundamental.", r"Usa $\mu=m/L_1$ y $\lambda_1=2L_1$, seguida de $v_1=f_1\lambda_1$.", "a-mu", "a-v1"),
        _desarrollo("SUF26-4", "B", "Calculá la rapidez en la parte baja de la tubería.",
                    "Escribe Bernoulli entre las dos alturas.", "Usa la diferencia de presión y el desnivel con signos correctos y despeja la raíz positiva.", "B"),
        _desarrollo("Q107", "C", "Calculá la eficiencia del ciclo.",
                    "Identifica el calor entrante como el denominador.", r"Sustituye las dos expresiones y cancela $p_0V_0$.", "C"),
    ),
    "trabajo": (_desarrollo("Q106", "a", "Calculá el trabajo neto del ciclo.",
                              "Identifica el área encerrada y el sentido del ciclo.", "Multiplica la diferencia de presiones por la de volúmenes.", "a"),),
    "temperatura": (_desarrollo("Q109", "B", "Calculá solo la temperatura del estado A.",
                                  "Elige la ecuación del gas ideal.", "Usa unidades compatibles con el valor de la constante y despeja T.", "TA"),),
    "venturi": (_desarrollo("RP-24II-2", "B", "Determiná la diferencia de presión entre las dos secciones.",
                              "Usa continuidad y Bernoulli a altura constante.", "Convierte el caudal, calcula ambas rapideces y sustituye.", "B"),),
    "pendulo": (_seleccion("RP-26I-2"),),
    "ondas": (_seleccion("P1O26-4"),),
    "retencion": (_seleccion("O26-5"),),
}


def calendario():
    return [{"fecha": fecha, "titulo": titulo,
             "bloques": [{**b, "id": f"{i + 1}-{k + 1}"} for k, b in enumerate(bloques)]}
            for i, (fecha, titulo, bloques) in enumerate(_DIAS)]


def formulario_repo(raiz):
    carpeta = raiz / "Fisica_III" / "Documentos II - 2026"
    if not carpeta.is_dir():
        return None
    candidatos = []
    for p in carpeta.iterdir():
        nombre = unicodedata.normalize("NFKD", p.stem.casefold())
        if p.is_file() and p.suffix.lower() == ".pdf" and "repo" in nombre and (
            "formul" in nombre or "ecuacio" in nombre or "instrucc" in nombre
        ) and "solucio" not in nombre:
            candidatos.append(p)
    return candidatos[0] if len(candidatos) == 1 else None


def _publicar_base(i, refuerzo, intentos):
    id, pregunta, opciones, clave, ejemplo, pregunta2, opciones2, clave2 = _BASES[i]
    if refuerzo:
        pregunta, opciones, clave = pregunta2, opciones2, clave2
    marca = f"{id}-refuerzo" if refuerzo else id
    d = {"id": marca, "pregunta": pregunta, "opciones": opciones, "grafica": id == "grafica"}
    if refuerzo:
        d["ejemplo"] = ejemplo
    if marca in intentos:
        d.update(elegida=intentos[marca], correcta=clave, explicacion=ejemplo)
    return d


def diagnostico(estado):
    intentos = estado.get("diagnostico", {})
    i = next((k for k, (id, _, _, clave, *_) in enumerate(_BASES)
              if id in intentos and intentos[id] != clave), 3)
    return {"reactivos": [_publicar_base(k, False, intentos) for k in range(len(_BASES))],
            "refuerzo": _publicar_base(i, True, intentos)}


def _pregunta(prueba, i, datos, documentos):
    id, inciso, pide, planteo, desarrollo, claves = _PRUEBAS[prueba][i]
    lec = registro.obtener(id)
    j = filtrar_leccion_corte_20_5(lec.a_json())
    texto = j["enunciado"]
    if id == "Q107":
        texto = r"Para el ciclo rectangular, el trabajo neto es $p_0V_0$ y el calor entrante es $9{,}5\,p_0V_0$."
    r = {"id": id, "titulo": j["titulo"], "enunciado": texto, "opciones": j.get("opciones", {}),
         "inciso": inciso, "pide": pide, "parcial": j.get("parcial", 3)}
    if documentos and id != "Q107":
        rec = documentos.recorte_de_leccion(lec, j.get("recorte_omite_desde", ""))
        if rec:
            r["recorte"] = {"url": rec["url"], "etiqueta": rec.get("etiqueta", "")}
    r["entrada"] = datos.get("respuestas", {}).get(str(i), "")
    if datos.get("entregada"):
        if inciso:
            respuestas = [q["tex"] for q in j["respuestas"] if q["inciso"] in claves]
            r["rubrica"] = {"planteo": planteo, "desarrollo": desarrollo, "resultado": respuestas}
            r["criterios"] = datos.get("rubrica", {}).get(str(i))
        else:
            r["correcta"] = j["correcta"]
    return r


def evaluacion(id, estado, lecciones, documentos=None):
    if id not in _PRUEBAS:
        raise KeyError(id)
    datos = estado.get("evaluaciones", {}).get(id, {})
    items = [_pregunta(id, i, datos, documentos) for i in range(len(_PRUEBAS[id]))]
    puntos, maximo, corregidas = 0, 0, 0
    for q in items:
        maximo += 8 if q["inciso"] else 2
        if "correcta" in q:
            puntos += 2 * (q["entrada"] == q["correcta"])
            corregidas += 1
        if q.get("criterios") is not None:
            puntos += sum(q["criterios"])
            corregidas += 1
    vistas = datos.get("vistas")
    if vistas is None:
        vistas = [q["id"] for q in items if (lecciones.get(q["id"]) or {}).get("hecha")
                  or (lecciones.get(q["id"]) or {}).get("cuando")]
    return {"id": id, "items": items, "entregada": bool(datos.get("entregada")), "vistas": vistas,
            "puntos": puntos, "maximo": maximo, "corregida": corregidas == len(items)}


def agregar_evento(estado, b, lecciones):
    """Actualiza solo el bloque, intento o rúbrica indicado. Las entregas son idempotentes."""
    evento = b.get("evento")
    if evento == "bloque":
        clave = b.get("id")
        if not isinstance(clave, str) or clave not in {f"{i}-{k}" for i in range(1, 7) for k in range(1, 9)}:
            raise ValueError("Bloque desconocido")
        anterior = estado.setdefault("bloques", {}).setdefault(clave, {})
        segundos = b.get("segundos", 0)
        if type(segundos) is not int or not 0 <= segundos <= 900:
            raise ValueError("Duración inválida")
        anterior["segundos"] = max(anterior.get("segundos", 0), segundos)
        if b.get("hecho") is True:
            anterior["hecho"] = True
        if b.get("pendiente") is True:
            anterior["pendiente"] = True
        if isinstance(b.get("nota"), str):
            anterior["nota"] = b["nota"][:2400]
    elif evento == "diagnostico":
        diagnosticos = diagnostico(estado)
        reactivo = next((d for d in [*diagnosticos["reactivos"], diagnosticos["refuerzo"]]
                         if d["id"] == b.get("id")), None)
        opcion = b.get("opcion")
        if reactivo is None or type(opcion) is not int or opcion not in range(len(reactivo["opciones"])):
            raise ValueError("Respuesta inválida")
        estado.setdefault("diagnostico", {}).setdefault(reactivo["id"], opcion)
    elif evento in ("respuesta", "entregar", "rubrica", "conocida"):
        id = b.get("id")
        if not isinstance(id, str) or id not in _PRUEBAS:
            raise ValueError("Práctica desconocida")
        ev = estado.setdefault("evaluaciones", {}).setdefault(id, {})
        if evento == "entregar":
            if not ev.get("entregada"):
                vistas = evaluacion(id, estado, lecciones)["vistas"]
                ev.update(entregada=int(time.time()), vistas=vistas)
            return
        i = b.get("i")
        if type(i) is not int or i not in range(len(_PRUEBAS[id])):
            raise ValueError("Pregunta inexistente")
        lec_id, inciso, *_ = _PRUEBAS[id][i]
        if evento == "respuesta" and not ev.get("entregada"):
            respuesta = b.get("entrada")
            if not isinstance(respuesta, str):
                raise ValueError("Respuesta inválida")
            if not inciso and respuesta not in (*registro.obtener(lec_id).opciones, ""):
                raise ValueError("Opción inválida")
            ev.setdefault("respuestas", {})[str(i)] = respuesta[:2400]
        elif evento == "rubrica" and ev.get("entregada") and inciso:
            valores = b.get("criterios")
            if not isinstance(valores, list) or len(valores) != 3 or any(
                type(v) is not int or v < 0 or v > tope for v, tope in zip(valores, (2, 4, 2))
            ):
                raise ValueError("Puntaje inválido")
            ev.setdefault("rubrica", {})[str(i)] = valores
        elif evento == "conocida" and not ev.get("entregada"):
            ev["vistas"] = list(dict.fromkeys([*evaluacion(id, estado, lecciones)["vistas"], lec_id]))
        else:
            raise ValueError("La práctica ya está entregada o falta entregarla")
    else:
        raise ValueError("Evento de estudio desconocido")
