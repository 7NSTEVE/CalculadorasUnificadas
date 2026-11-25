import Foundation

enum EnergiaDisponible: Int, Codable, CaseIterable {
    case agotado = 0, baja = 1, normal = 2, alta = 3
    var nombre: String { ["Estoy muerto", "Baja", "Normal", "Alta"][rawValue] }
}

enum PrioridadAcademica: Int, Codable, CaseIterable { case algunDia, baja, normal, alta, urgente }
enum EstadoTarea: String, Codable { case pendiente, enCurso, esperando, completada, archivada }
enum TipoCaptura: String, Codable { case texto, foto, archivo, audio, enlace, pensamiento, duda }
enum TipoFuente: String, Codable, Hashable {
    case apuntePropio, audioClase, transcripcion, diapositivaProfesor, pizarra
    case libro, web, solucion, inferenciaLocal, generadoLocal
}
enum TipoEventoAcademico: String, Codable {
    case clase, escritura, audio, diapositiva, foto, marcador, mePerdi, importante
    case examen, tarea, repaso, error, foco, captura, archivo, version, recuperacion
}
enum ModoRespuestaLocal: String, Codable { case estrictoFuentes, asistidoConInferencias }

struct CursoAcademico: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var codigo = ""
    var profesor = ""
    var colorARGB: UInt32 = 0xFF0A_84FF
    var semestre = ""
    var archivado = false
    var etiquetas: [String] = []
    var workspaceId: UUID?
}

struct BloqueHorario: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID
    var diaSemana: Int // Calendar: 1 domingo ... 7 sábado
    var horaInicio: Int
    var minutoInicio: Int
    var duracionMinutos: Int
    var aula = ""
}

struct SesionClase: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var titulo: String
    var profesor = ""
    var inicio = Date()
    var fin: Date?
    var documentoId: UInt64?
    var objetivo = ""
    var resumenPostClase = ""
    var revisada = false
    var nombresAutomaticos: [String] = []
    var archivoAudio: String?
    var archivoNotaViva: String?
}

struct CapturaBandeja: Codable, Identifiable, Equatable {
    var id = UUID()
    var fecha = Date()
    var tipo: TipoCaptura
    var contenido: String
    var hashAdjunto: String?
    var cursoSugerido: UUID?
    var cursoConfirmado: UUID?
    var procesada = false
    var etiquetasSugeridas: [String] = []
}

struct ReferenciaFuente: Codable, Identifiable, Equatable {
    var id = UUID()
    var tipo: TipoFuente
    var titulo: String
    var cursoId: UUID?
    var sesionId: UUID?
    var documentoId: UInt64?
    var elementoId: String?
    var hashArchivo: String?
    var pagina: Int?
    var segundoAudio: Double?
    var rangoTexto: RangeCodificable?
    var originalInmutable = true
}

struct RangeCodificable: Codable, Equatable { var inicio: Int; var longitud: Int }

struct TareaAcademica: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var detalle = ""
    var cursoId: UUID?
    var fechaCreacion = Date()
    var fechaLimite: Date?
    var estimadoMinutos = 25
    var energiaMinima: EnergiaDisponible = .baja
    var prioridad: PrioridadAcademica = .normal
    var estado: EstadoTarea = .pendiente
    var pasos: [PasoTarea] = []
    var referencia: ReferenciaFuente?
    var confianzaExtraccion: Float?
    var favoritaHasta: Date?
    var etiquetas: [String] = []
}

struct PasoTarea: Codable, Identifiable, Equatable {
    var id = UUID(); var titulo: String; var completado = false; var estimadoMinutos = 5
}

struct FlashcardAcademica: Codable, Identifiable, Equatable {
    var id = UUID()
    var pregunta: String
    var respuesta: String
    var cursoId: UUID?
    var conceptoId: UUID?
    var referencia: ReferenciaFuente?
    var creada = Date()
    var proximoRepaso = Date()
    var intervaloDias: Double = 0
    var facilidad: Double = 2.5
    var repeticiones = 0
    var fallos = 0
    var confianza: Double = 0.5
    var suspendida = false
}

struct RegistroRepaso: Codable, Identifiable, Equatable {
    var id = UUID(); var flashcardId: UUID; var fecha = Date(); var calidad: Int
    var confianza: Double; var segundos: Double
}

struct ErrorAprendizaje: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var conceptoId: UUID?
    var enunciado: String
    var respuestaDada: String
    var respuestaCorrecta: String
    var causa: String
    var fecha = Date()
    var repeticiones = 1
    var resuelto = false
    var proximoRepaso = Date()
    var referencia: ReferenciaFuente?
}

struct ConceptoDominio: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var nombre: String
    var dominio: Double = 0
    var confianzaDeclarada: Double = 0
    var intentos = 0
    var aciertos = 0
    var ultimoContacto: Date?
    var prerrequisitos: [UUID] = []
    var etiquetas: [String] = []
}

struct SesionFoco: Codable, Identifiable, Equatable {
    var id = UUID()
    var objetivo: String
    var inicio = Date()
    var duracionMinutos: Int
    var fin: Date?
    var interrupciones = 0
    var tareaId: UUID?
    var activa = true
}

struct PensamientoParking: Codable, Identifiable, Equatable {
    var id = UUID(); var texto: String; var fecha = Date(); var resuelto = false
}

struct EstadoSesionExacto: Codable, Equatable {
    var fecha = Date()
    var documentoId: UInt64
    var pagina = 0
    var vista: EstadoVistaLienzo
    var herramienta = "pluma"
    var colorARGB: UInt32 = 0xFF1A_1A1A
    var grosor: Float = 5
    var segundoAudio: Double = 0
    var elementoSeleccionado: String?
    var panelAbierto: String?
    var consultaPendiente: String?
}

struct EventoAcademico: Codable, Identifiable, Equatable {
    var id = UUID()
    var fecha = Date()
    var tipo: TipoEventoAcademico
    var titulo: String
    var detalle = ""
    var cursoId: UUID?
    var sesionId: UUID?
    var referencia: ReferenciaFuente?
}

struct NodoConocimiento: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var texto: String
    var fuente: ReferenciaFuente
    var etiquetas: [String] = []
    var conceptos: [UUID] = []
    var fecha = Date()
}

struct ResultadoConFuente: Identifiable {
    var id = UUID(); var texto: String; var fuente: ReferenciaFuente; var puntuacion: Double
}

struct RespuestaLocal {
    var texto: String
    var citas: [ReferenciaFuente]
    var esInferencia: Bool
    var advertencias: [String]
}

struct ContradiccionLocal: Identifiable {
    var id = UUID()
    var afirmacionA: String
    var afirmacionB: String
    var fuenteA: ReferenciaFuente
    var fuenteB: ReferenciaFuente
    var motivo: String
}

struct EntradaIndice: Identifiable {
    var id = UUID(); var titulo: String; var cantidad: Int; var fuentes: [ReferenciaFuente]
}

struct CarpetaInteligente: Codable, Identifiable, Equatable {
    var id = UUID(); var nombre: String; var consulta: String; var etiquetas: [String] = []
    var soloPendientes = false; var soloFavoritos = false
}

struct ArchivoProfesor: Codable, Identifiable, Equatable {
    var id = UUID(); var nombre: String; var cursoId: UUID?; var hashActual: String
    var versiones: [VersionArchivo] = []; var fechaRevision = Date()
}

struct VersionArchivo: Codable, Identifiable, Equatable {
    var id = UUID(); var hash: String; var fecha = Date(); var notasCambio = ""
}

struct ScratchSpace: Codable, Identifiable, Equatable {
    var id = UUID(); var titulo: String; var documentoId: UInt64?; var global = false
    var fecha = Date(); var expira: Date?
}

struct BaseAcademica: Codable {
    var version = 1
    var cursos: [CursoAcademico] = []
    var horarios: [BloqueHorario] = []
    var sesiones: [SesionClase] = []
    var bandeja: [CapturaBandeja] = []
    var tareas: [TareaAcademica] = []
    var flashcards: [FlashcardAcademica] = []
    var repasos: [RegistroRepaso] = []
    var errores: [ErrorAprendizaje] = []
    var conceptos: [ConceptoDominio] = []
    var foco: [SesionFoco] = []
    var parking: [PensamientoParking] = []
    var estados: [UInt64: EstadoSesionExacto] = [:]
    var timeline: [EventoAcademico] = []
    var conocimiento: [NodoConocimiento] = []
    var carpetasInteligentes: [CarpetaInteligente] = []
    var archivosProfesor: [ArchivoProfesor] = []
    var scratch: [ScratchSpace] = []
    var presupuestoNotificacionesDiario = 3
    var notificacionesEmitidasHoy = 0
    var diaPresupuesto = Date.distantPast
}

struct ResumenHoy {
    var siguiente: TareaAcademica?
    var tareasVencidas: [TareaAcademica]
    var tareasHoy: [TareaAcademica]
    var tarjetasPendientes: [FlashcardAcademica]
    var erroresPendientes: [ErrorAprendizaje]
    var sesionesSinRevisar: [SesionClase]
}

/// Fuente de verdad académica local. JSON atómico y sin servicio de pago.
final class CentroAcademico {
    static let compartido = CentroAcademico()
    private let url: URL
    private let bloqueo = NSRecursiveLock()
    private(set) var base: BaseAcademica

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("centro-academico.json")
        if let datos = try? Data(contentsOf: url),
           let leida = try? JSONDecoder.academico.decode(BaseAcademica.self, from: datos) {
            base = leida
        } else {
            base = BaseAcademica()
        }
    }

    @discardableResult
    func transaccion<T>(_ cambios: (inout BaseAcademica) throws -> T) rethrows -> T {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let resultado = try cambios(&base)
        guardarSinBloqueo()
        return resultado
    }

    func instantanea() -> BaseAcademica {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return base
    }

    private func guardarSinBloqueo() {
        guard let datos = try? JSONEncoder.academico.encode(base) else { return }
        try? datos.write(to: url, options: .atomic)
    }

    @discardableResult
    func capturar(_ contenido: String, tipo: TipoCaptura,
                  hash: String? = nil, cursoSugerido: UUID? = nil) -> UUID {
        transaccion { base in
            let captura = CapturaBandeja(tipo: tipo, contenido: contenido,
                                         hashAdjunto: hash, cursoSugerido: cursoSugerido,
                                         etiquetasSugeridas: EtiquetadorLocal.etiquetas(contenido))
            base.bandeja.append(captura)
            base.timeline.append(EventoAcademico(tipo: .captura, titulo: contenido,
                                                  cursoId: cursoSugerido))
            return captura.id
        }
    }

    @discardableResult
    func convertir(_ referencia: ReferenciaElemento, a destino: DestinoFragmento,
                   cursoId: UUID? = nil) -> UUID {
        let fuente = ReferenciaFuente(
            tipo: .apuntePropio, titulo: referencia.titulo, cursoId: cursoId,
            documentoId: UInt64(referencia.paginaDocumento), elementoId: referencia.id)
        return transaccion { base in
            switch destino {
            case .tarea:
                let item = TareaAcademica(titulo: referencia.titulo, cursoId: cursoId,
                                          referencia: fuente)
                base.tareas.append(item); return item.id
            case .flashcard:
                let item = FlashcardAcademica(
                    pregunta: "Explique: \(referencia.titulo)", respuesta: "Pendiente de completar",
                    cursoId: cursoId, referencia: fuente)
                base.flashcards.append(item); return item.id
            case .pregunta:
                let captura = CapturaBandeja(tipo: .texto,
                                             contenido: "Pregunta: \(referencia.titulo)",
                                             cursoSugerido: cursoId)
                base.bandeja.append(captura); return captura.id
            case .duda:
                let captura = CapturaBandeja(tipo: .duda, contenido: referencia.titulo,
                                             cursoSugerido: cursoId)
                base.bandeja.append(captura); return captura.id
            case .nota:
                let nodo = NodoConocimiento(titulo: referencia.titulo,
                                            texto: referencia.titulo, fuente: fuente,
                                            etiquetas: EtiquetadorLocal.etiquetas(referencia.titulo))
                base.conocimiento.append(nodo); return nodo.id
            }
        }
    }

    func cursoEnHorario(fecha: Date = Date()) -> CursoAcademico? {
        let b = instantanea()
        let calendario = Calendar.current
        let dia = calendario.component(.weekday, from: fecha)
        let minutos = calendario.component(.hour, from: fecha) * 60
            + calendario.component(.minute, from: fecha)
        guard let bloque = b.horarios.first(where: {
            $0.diaSemana == dia && minutos >= $0.horaInicio * 60 + $0.minutoInicio - 20
                && minutos <= $0.horaInicio * 60 + $0.minutoInicio + $0.duracionMinutos
        }) else { return nil }
        return b.cursos.first { $0.id == bloque.cursoId && !$0.archivado }
    }

    @discardableResult
    func iniciarClase(documentoId: UInt64?, objetivo: String = "",
                      fecha: Date = Date()) -> SesionClase {
        let curso = cursoEnHorario(fecha: fecha)
        return transaccion { base in
            let titulo = [curso?.nombre, Formateadores.fechaCorta.string(from: fecha)]
                .compactMap { $0 }.joined(separator: " — ")
            let sesion = SesionClase(cursoId: curso?.id, titulo: titulo,
                                     profesor: curso?.profesor ?? "", inicio: fecha,
                                     documentoId: documentoId, objetivo: objetivo)
            base.sesiones.append(sesion)
            base.timeline.append(EventoAcademico(fecha: fecha, tipo: .clase,
                                                  titulo: titulo, cursoId: curso?.id,
                                                  sesionId: sesion.id))
            return sesion
        }
    }

    func guardarEstado(_ estado: EstadoSesionExacto) {
        transaccion { $0.estados[estado.documentoId] = estado }
    }

    func estado(documentoId: UInt64) -> EstadoSesionExacto? {
        instantanea().estados[documentoId]
    }

    func extraerTareas(texto: String, cursoId: UUID?, fuente: ReferenciaFuente?)
        -> [TareaAcademica] {
        let lineas = texto.components(separatedBy: .newlines)
        let indicios = ["tarea", "entregar", "deber", "ejercicio", "para el", "quiz",
                       "examen", "proyecto", "hacer ", "leer "]
        var extraidas: [TareaAcademica] = []
        for linea in lineas {
            let normal = linea.folding(options: [.diacriticInsensitive, .caseInsensitive],
                                       locale: .current)
            guard indicios.contains(where: normal.contains), linea.count > 3 else { continue }
            var tarea = TareaAcademica(titulo: linea.trimmingCharacters(in: .whitespaces),
                                       cursoId: cursoId, referencia: fuente,
                                       confianzaExtraccion: 0.72)
            tarea.fechaLimite = DetectorFechasLocal.fecha(en: linea)
            if normal.contains("examen") || normal.contains("entregar") { tarea.prioridad = .alta }
            tarea.pasos = DescomponedorTareas.pasos(para: tarea.titulo,
                                                    minutos: tarea.estimadoMinutos)
            extraidas.append(tarea)
        }
        return transaccion { base in
            let titulos = Set(base.tareas.map { Normalizador.texto($0.titulo) })
            let nuevas = extraidas.filter { !titulos.contains(Normalizador.texto($0.titulo)) }
            base.tareas.append(contentsOf: nuevas)
            for tarea in nuevas {
                base.timeline.append(EventoAcademico(tipo: .tarea, titulo: tarea.titulo,
                                                      cursoId: cursoId, referencia: fuente))
            }
            return nuevas
        }
    }

    func registrarRepaso(tarjetaId: UUID, calidad: Int, confianza: Double,
                         segundos: Double) {
        transaccion { base in
            guard let indice = base.flashcards.firstIndex(where: { $0.id == tarjetaId }) else { return }
            var tarjeta = base.flashcards[indice]
            let q = min(max(calidad, 0), 5)
            if q < 3 {
                tarjeta.repeticiones = 0
                tarjeta.intervaloDias = 1
                tarjeta.fallos += 1
            } else {
                tarjeta.intervaloDias = tarjeta.repeticiones == 0 ? 1
                    : (tarjeta.repeticiones == 1 ? 6
                       : tarjeta.intervaloDias * tarjeta.facilidad)
                tarjeta.repeticiones += 1
            }
            tarjeta.facilidad = max(1.3, tarjeta.facilidad
                + (0.1 - Double(5 - q) * (0.08 + Double(5 - q) * 0.02)))
            tarjeta.confianza = min(max(confianza, 0), 1)
            // Confianza baja adelanta el repaso aunque la respuesta haya sido correcta.
            let factorConfianza = 0.55 + tarjeta.confianza * 0.45
            tarjeta.proximoRepaso = Date().addingTimeInterval(
                tarjeta.intervaloDias * factorConfianza * 86_400)
            base.flashcards[indice] = tarjeta
            base.repasos.append(RegistroRepaso(flashcardId: tarjetaId, calidad: q,
                                                confianza: confianza, segundos: segundos))
        }
    }

    @discardableResult
    func registrarError(cursoId: UUID?, conceptoId: UUID?, enunciado: String,
                        respuestaDada: String, correcta: String, causa: String,
                        referencia: ReferenciaFuente? = nil) -> ErrorAprendizaje {
        transaccion { base in
            if let indice = base.errores.firstIndex(where: {
                !$0.resuelto && Normalizador.texto($0.enunciado) == Normalizador.texto(enunciado)
            }) {
                base.errores[indice].repeticiones += 1
                base.errores[indice].fecha = Date()
                base.errores[indice].proximoRepaso = Date()
                return base.errores[indice]
            }
            let error = ErrorAprendizaje(cursoId: cursoId, conceptoId: conceptoId,
                                         enunciado: enunciado, respuestaDada: respuestaDada,
                                         respuestaCorrecta: correcta, causa: causa,
                                         referencia: referencia)
            base.errores.append(error)
            base.timeline.append(EventoAcademico(tipo: .error, titulo: enunciado,
                                                  cursoId: cursoId, referencia: referencia))
            return error
        }
    }

    func actualizarDominio(conceptoId: UUID, correcto: Bool, confianza: Double) {
        transaccion { base in
            guard let i = base.conceptos.firstIndex(where: { $0.id == conceptoId }) else { return }
            base.conceptos[i].intentos += 1
            if correcto { base.conceptos[i].aciertos += 1 }
            base.conceptos[i].confianzaDeclarada = min(max(confianza, 0), 1)
            let precision = Double(base.conceptos[i].aciertos)
                / Double(max(base.conceptos[i].intentos, 1))
            // No confunde "lo vi" con "lo domino": combina desempeño y confianza calibrada.
            base.conceptos[i].dominio = precision * 0.75 + min(confianza, precision) * 0.25
            base.conceptos[i].ultimoContacto = Date()
        }
    }

    func siguienteAccion(minutos: Int, energia: EnergiaDisponible,
                         cursoId: UUID? = nil, ahora: Date = Date()) -> TareaAcademica? {
        let tareas = instantanea().tareas.filter {
            ($0.estado == .pendiente || $0.estado == .enCurso)
                && $0.energiaMinima.rawValue <= energia.rawValue
                && (cursoId == nil || $0.cursoId == cursoId)
        }
        return tareas.max { a, b in
            puntuacion(tarea: a, minutos: minutos, ahora: ahora)
                < puntuacion(tarea: b, minutos: minutos, ahora: ahora)
        }
    }

    private func puntuacion(tarea: TareaAcademica, minutos: Int, ahora: Date) -> Double {
        var puntos = Double(tarea.prioridad.rawValue) * 20
        if let limite = tarea.fechaLimite {
            let dias = limite.timeIntervalSince(ahora) / 86_400
            puntos += dias <= 0 ? 100 : max(0, 45 - dias * 7)
        }
        if tarea.estimadoMinutos <= minutos { puntos += 22 }
        else if minutos <= 15, let paso = tarea.pasos.first(where: { !$0.completado }),
                paso.estimadoMinutos <= minutos { puntos += 18 }
        if tarea.estado == .enCurso { puntos += 16 }
        return puntos
    }

    func hoy(minutos: Int = 45, energia: EnergiaDisponible = .normal,
             fecha: Date = Date()) -> ResumenHoy {
        let b = instantanea()
        let calendario = Calendar.current
        let pendientes = b.tareas.filter { $0.estado == .pendiente || $0.estado == .enCurso }
        return ResumenHoy(
            siguiente: siguienteAccion(minutos: minutos, energia: energia, ahora: fecha),
            tareasVencidas: pendientes.filter { ($0.fechaLimite ?? .distantFuture) < fecha },
            tareasHoy: pendientes.filter { fechaLimite in
                guard let limite = fechaLimite.fechaLimite else { return false }
                return calendario.isDate(limite, inSameDayAs: fecha)
            },
            tarjetasPendientes: b.flashcards.filter { !$0.suspendida && $0.proximoRepaso <= fecha },
            erroresPendientes: b.errores.filter { !$0.resuelto && $0.proximoRepaso <= fecha }
                .sorted { $0.repeticiones > $1.repeticiones },
            sesionesSinRevisar: b.sesiones.filter { !$0.revisada })
    }

    func planUrgencia(horas: Double, cursoId: UUID?) -> [TareaAcademica] {
        let limite = max(Int(horas * 60), 5)
        var restantes = limite
        var resultado: [TareaAcademica] = []
        let candidatas = instantanea().tareas.filter {
            $0.estado == .pendiente && (cursoId == nil || $0.cursoId == cursoId)
        }.sorted { puntuacion(tarea: $0, minutos: limite, ahora: Date())
            > puntuacion(tarea: $1, minutos: limite, ahora: Date()) }
        for tarea in candidatas where restantes > 0 {
            resultado.append(tarea)
            restantes -= min(tarea.estimadoMinutos, restantes)
        }
        return resultado
    }

    /// Reprograma sin rachas ni castigo: mantiene la prioridad real y reparte
    /// la carga desde hoy.
    func reprogramarAtrasos(maximoDiarioMinutos: Int = 90) {
        transaccion { base in
            var acumulado = 0
            var dia = Calendar.current.startOfDay(for: Date())
            let indices = base.tareas.indices.filter {
                guard base.tareas[$0].estado == .pendiente,
                      let limite = base.tareas[$0].fechaLimite else { return false }
                return limite < Date()
            }.sorted { (base.tareas[$0].fechaLimite ?? .distantPast)
                < (base.tareas[$1].fechaLimite ?? .distantPast) }
            for indice in indices {
                if acumulado + base.tareas[indice].estimadoMinutos > maximoDiarioMinutos {
                    dia = Calendar.current.date(byAdding: .day, value: 1, to: dia) ?? dia
                    acumulado = 0
                }
                base.tareas[indice].fechaLimite = Calendar.current.date(
                    bySettingHour: 18, minute: 0, second: 0, of: dia)
                acumulado += base.tareas[indice].estimadoMinutos
            }
        }
    }

    func agregarNodo(_ nodo: NodoConocimiento) {
        transaccion { base in
            if let indice = base.conocimiento.firstIndex(where: {
                $0.fuente.hashArchivo != nil && $0.fuente.hashArchivo == nodo.fuente.hashArchivo
                    && Normalizador.texto($0.texto) == Normalizador.texto(nodo.texto)
            }) {
                base.conocimiento[indice].fecha = nodo.fecha
            } else { base.conocimiento.append(nodo) }
        }
    }

    func buscar(_ consulta: String, cursoId: UUID? = nil, limite: Int = 30)
        -> [ResultadoConFuente] {
        let datos = instantanea()
        let analizada = ConsultaAcademicaIPad.analizar(consulta)
        let cursos: Set<UUID>? = cursoId.map { Set([$0]) }
            ?? analizada.idsCurso(en: datos.cursos)
        return MotorConocimientoLocal.buscar(
            analizada.texto, en: datos.conocimiento,
            cursoIds: cursos, limite: limite)
    }

    func responder(_ consulta: String, modo: ModoRespuestaLocal,
                   cursoId: UUID? = nil) -> RespuestaLocal {
        MotorConocimientoLocal.responder(consulta, resultados: buscar(
            consulta, cursoId: cursoId, limite: 8), modo: modo)
    }

    func modoSocratico(_ tema: String, cursoId: UUID? = nil) -> RespuestaLocal {
        let resultados = buscar(tema, cursoId: cursoId, limite: 6)
        guard !resultados.isEmpty else {
            return RespuestaLocal(texto: "No hay material local suficiente para preguntar sin inventar.",
                                  citas: [], esInferencia: false,
                                  advertencias: ["Agregue o indexe material del tema."])
        }
        let preguntas = resultados.prefix(5).enumerated().map { indice, resultado in
            "\(indice + 1). Sin mirar, ¿cómo explicaría \(resultado.fuente.titulo) y qué evidencia usaría?"
        }
        return RespuestaLocal(texto: preguntas.joined(separator: "\n\n"),
                              citas: resultados.prefix(5).map(\.fuente),
                              esInferencia: false, advertencias: [])
    }

    func loDijoElProfesor(_ consulta: String, cursoId: UUID? = nil) -> RespuestaLocal {
        let permitidas: Set<TipoFuente> = [.audioClase, .transcripcion, .diapositivaProfesor, .pizarra]
        let resultados = buscar(consulta, cursoId: cursoId, limite: 20)
            .filter { permitidas.contains($0.fuente.tipo) }
        guard !resultados.isEmpty else {
            return RespuestaLocal(
                texto: "No encontré esa afirmación en audio, transcripción, diapositivas o pizarra del profesor.",
                citas: [], esInferencia: false,
                advertencias: ["Ausencia de evidencia no demuestra que nunca se haya dicho."])
        }
        return MotorConocimientoLocal.responder(consulta, resultados: resultados,
                                                modo: .estrictoFuentes)
    }

    func detectarContradicciones(cursoId: UUID? = nil) -> [ContradiccionLocal] {
        MotorConocimientoLocal.contradicciones(
            instantanea().conocimiento.filter { cursoId == nil || $0.fuente.cursoId == cursoId })
    }

    func indiceAutomatico(cursoId: UUID? = nil) -> [EntradaIndice] {
        let nodos = instantanea().conocimiento.filter {
            cursoId == nil || $0.fuente.cursoId == cursoId
        }
        var grupos: [String: [NodoConocimiento]] = [:]
        for nodo in nodos {
            grupos[nodo.etiquetas.first ?? nodo.titulo, default: []].append(nodo)
        }
        return grupos.map { clave, valores in
            EntradaIndice(titulo: clave, cantidad: valores.count,
                          fuentes: valores.map(\.fuente))
        }.sorted { $0.titulo < $1.titulo }
    }

    func recuperarUltimosCincoMinutos(sesionId: UUID, ahora: Date = Date()) -> RespuestaLocal {
        let desde = ahora.addingTimeInterval(-300)
        let eventos = instantanea().timeline.filter {
            $0.sesionId == sesionId && $0.fecha >= desde && $0.fecha <= ahora
        }
        let citas = eventos.compactMap(\.referencia)
        let texto = eventos.isEmpty
            ? "No hay eventos locales suficientes en los últimos cinco minutos."
            : eventos.map { "• \($0.titulo)" }.joined(separator: "\n")
        return RespuestaLocal(texto: texto, citas: citas, esInferencia: false,
                              advertencias: eventos.isEmpty ? ["Contexto insuficiente"] : [])
    }

    func elementosHuerfanos() -> [CapturaBandeja] {
        instantanea().bandeja.filter { !$0.procesada && $0.cursoConfirmado == nil }
    }

    func materialNoRevisado(antesDe dias: Int = 3) -> [SesionClase] {
        let fecha = Calendar.current.date(byAdding: .day, value: -dias, to: Date()) ?? Date()
        return instantanea().sesiones.filter { !$0.revisada && $0.inicio <= fecha }
    }
}

enum MotorConocimientoLocal {
    static func buscar(_ consulta: String, en nodos: [NodoConocimiento],
                       cursoIds: Set<UUID>?, limite: Int) -> [ResultadoConFuente] {
        let terminos = Normalizador.tokens(consulta)
        if terminos.isEmpty {
            guard let cursoIds else { return [] }
            return nodos.filter { nodo in
                nodo.fuente.cursoId.map(cursoIds.contains) == true
            }.sorted { $0.fecha > $1.fecha }.prefix(limite).map {
                ResultadoConFuente(texto: $0.texto, fuente: $0.fuente,
                                   puntuacion: 1)
            }
        }
        return nodos.compactMap { nodo -> ResultadoConFuente? in
            if let cursoIds,
               nodo.fuente.cursoId.map(cursoIds.contains) != true { return nil }
            let titulo = Normalizador.tokens(nodo.titulo)
            let cuerpo = Normalizador.tokens(nodo.texto)
            let etiquetas = nodo.etiquetas.flatMap(Normalizador.tokens)
            var puntaje = 0.0
            for termino in terminos {
                puntaje += Double(titulo.filter { $0 == termino }.count) * 4
                puntaje += Double(cuerpo.filter { $0 == termino }.count)
                if etiquetas.contains(termino) { puntaje += 3 }

                let mejorTitulo = titulo.map {
                    SimilitudManuscritaIPad.puntuacion(
                        consulta: termino, candidato: $0)
                }.max() ?? 0
                let mejorCuerpo = cuerpo.map {
                    SimilitudManuscritaIPad.puntuacion(
                        consulta: termino, candidato: $0)
                }.max() ?? 0
                let mejorEtiqueta = etiquetas.map {
                    SimilitudManuscritaIPad.puntuacion(
                        consulta: termino, candidato: $0)
                }.max() ?? 0
                if mejorTitulo >= 0.66 { puntaje += mejorTitulo * 3.5 }
                if mejorCuerpo >= 0.66 { puntaje += mejorCuerpo * 0.9 }
                if mejorEtiqueta >= 0.66 { puntaje += mejorEtiqueta * 2.5 }
            }
            if !consulta.isEmpty,
               Normalizador.texto(nodo.texto).contains(
                Normalizador.texto(consulta)) { puntaje += 8 }
            guard puntaje > 0 else { return nil }
            let antiguedad = Date().timeIntervalSince(nodo.fecha) / 86_400
            puntaje += max(0, 2 - antiguedad / 180)
            return ResultadoConFuente(texto: nodo.texto, fuente: nodo.fuente,
                                      puntuacion: puntaje)
        }.sorted { $0.puntuacion > $1.puntuacion }.prefix(limite).map { $0 }
    }

    static func responder(_ consulta: String, resultados: [ResultadoConFuente],
                          modo: ModoRespuestaLocal) -> RespuestaLocal {
        guard !resultados.isEmpty else {
            return RespuestaLocal(
                texto: "No encontré evidencia en sus apuntes o materiales indexados.",
                citas: [], esInferencia: false,
                advertencias: ["La respuesta se cerró por falta de fuentes locales."])
        }
        let terminos = Set(Normalizador.tokens(consulta))
        var fragmentos: [String] = []
        var citas: [ReferenciaFuente] = []
        for resultado in resultados.prefix(5) {
            let oraciones = resultado.texto.components(separatedBy: CharacterSet(charactersIn: ".!?\n"))
            if let mejor = oraciones.max(by: {
                Set(Normalizador.tokens($0)).intersection(terminos).count
                    < Set(Normalizador.tokens($1)).intersection(terminos).count
            }), !mejor.trimmingCharacters(in: .whitespaces).isEmpty {
                fragmentos.append(mejor.trimmingCharacters(in: .whitespaces))
                citas.append(resultado.fuente)
            }
        }
        var texto = fragmentos.enumerated().map { "[\($0.offset + 1)] \($0.element)" }
            .joined(separator: "\n")
        var inferencia = false
        var avisos: [String] = []
        if modo == .asistidoConInferencias, fragmentos.count >= 2 {
            texto += "\n\nInferencia local: los fragmentos anteriores parecen describir partes relacionadas del mismo tema."
            inferencia = true
            avisos.append("La última oración es una inferencia, no una cita del profesor.")
        }
        return RespuestaLocal(texto: texto, citas: citas, esInferencia: inferencia,
                              advertencias: avisos)
    }

    static func contradicciones(_ nodos: [NodoConocimiento]) -> [ContradiccionLocal] {
        var salida: [ContradiccionLocal] = []
        for i in nodos.indices {
            for j in nodos.indices where j > i {
                let a = nodos[i], b = nodos[j]
                let comunes = Set(Normalizador.tokens(a.texto))
                    .intersection(Set(Normalizador.tokens(b.texto)))
                guard comunes.count >= 3 else { continue }
                let numerosA = valoresNumericos(a.texto)
                let numerosB = valoresNumericos(b.texto)
                let conflictoNumero = numerosA.contains { valorA in
                    numerosB.contains { valorB in valorA.unidad == valorB.unidad
                        && abs(valorA.valor - valorB.valor) > max(abs(valorA.valor) * 0.02, 0.0001) }
                }
                let normalA = " " + Normalizador.texto(a.texto) + " "
                let normalB = " " + Normalizador.texto(b.texto) + " "
                let conflictoNegacion = normalA.contains(" no ") != normalB.contains(" no ")
                guard conflictoNumero || conflictoNegacion else { continue }
                salida.append(ContradiccionLocal(
                    afirmacionA: a.texto, afirmacionB: b.texto,
                    fuenteA: a.fuente, fuenteB: b.fuente,
                    motivo: conflictoNumero
                        ? "Valores numéricos distintos con contexto y unidad compartidos"
                        : "Una fuente contiene negación y la otra no"))
                if salida.count >= 100 { return salida }
            }
        }
        return salida
    }

    private static func valoresNumericos(_ texto: String) -> [(valor: Double, unidad: String)] {
        let patron = #"([-+]?\d+(?:[\.,]\d+)?)\s*([a-zA-ZΩµ°/%]+)"#
        guard let regex = try? NSRegularExpression(pattern: patron) else { return [] }
        let rango = NSRange(texto.startIndex..<texto.endIndex, in: texto)
        return regex.matches(in: texto, range: rango).compactMap { coincidencia in
            guard let rv = Range(coincidencia.range(at: 1), in: texto),
                  let ru = Range(coincidencia.range(at: 2), in: texto),
                  let valor = Double(texto[rv].replacingOccurrences(of: ",", with: "."))
            else { return nil }
            return (valor, String(texto[ru]).lowercased())
        }
    }
}

enum EtiquetadorLocal {
    static func etiquetas(_ texto: String) -> [String] {
        let tokens = Normalizador.tokens(texto).filter { $0.count >= 5 }
        var frecuencia: [String: Int] = [:]
        tokens.forEach { frecuencia[$0, default: 0] += 1 }
        return frecuencia.sorted { a, b in a.value == b.value ? a.key < b.key : a.value > b.value }
            .prefix(6).map(\.key)
    }
}

enum DescomponedorTareas {
    static func pasos(para titulo: String, minutos: Int) -> [PasoTarea] {
        let base = max(minutos / 5, 1)
        let sugeridos = ["Abrir el material y ubicar el punto exacto",
                         "Resolver el primer bloque sin consultar la solución",
                         "Comparar, registrar errores y corregir",
                         "Entregar o programar el siguiente repaso"]
        return sugeridos.enumerated().map { indice, texto in
            PasoTarea(titulo: texto + (indice == 0 ? ": \(titulo)" : ""),
                      estimadoMinutos: max(base, 3))
        }
    }
}

enum DetectorFechasLocal {
    static func fecha(en texto: String, ahora: Date = Date()) -> Date? {
        let detector = try? NSDataDetector(types: NSTextCheckingResult.CheckingType.date.rawValue)
        let rango = NSRange(texto.startIndex..<texto.endIndex, in: texto)
        if let fecha = detector?.firstMatch(in: texto, range: rango)?.date { return fecha }
        let normal = Normalizador.texto(texto)
        if normal.contains("manana") { return Calendar.current.date(byAdding: .day, value: 1, to: ahora) }
        if normal.contains("hoy") { return ahora }
        if normal.contains("proxima semana") { return Calendar.current.date(byAdding: .day, value: 7, to: ahora) }
        return nil
    }
}

enum Normalizador {
    private static let vacias: Set<String> = ["para", "como", "este", "esta", "esto", "desde",
                                               "sobre", "entre", "porque", "pero", "cuando", "donde",
                                               "unos", "unas", "del", "las", "los", "una", "que"]
    static func texto(_ valor: String) -> String {
        valor.folding(options: [.diacriticInsensitive, .caseInsensitive], locale: .current)
            .lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
    }
    static func tokens(_ valor: String) -> [String] {
        let expandido = texto(valor)
            .replacingOccurrences(of: "Ω", with: " ohm ")
            .replacingOccurrences(of: "ω", with: " ohm ")
            .replacingOccurrences(of: "µ", with: " micro ")
            .replacingOccurrences(of: "μ", with: " micro ")
            .replacingOccurrences(of: "Δ", with: " delta ")
            .replacingOccurrences(of: "δ", with: " delta ")
            .replacingOccurrences(of: "∑", with: " sumatoria ")
            .replacingOccurrences(of: "∫", with: " integral ")
            .replacingOccurrences(of: "∂", with: " derivada parcial ")
            .replacingOccurrences(of: "θ", with: " theta ")
            .replacingOccurrences(of: "φ", with: " phi ")
            .replacingOccurrences(of: "τ", with: " tau ")
        return expandido.components(
            separatedBy: CharacterSet.alphanumerics.inverted)
            .filter { token in
                (token.count > 2 || token.contains(where: { $0.isNumber }))
                    && !vacias.contains(token)
            }
    }
}

private enum Formateadores {
    static let fechaCorta: DateFormatter = {
        let f = DateFormatter(); f.dateStyle = .medium; f.timeStyle = .short; return f
    }()
}

private extension JSONEncoder {
    static var academico: JSONEncoder {
        let e = JSONEncoder(); e.outputFormatting = [.sortedKeys]; e.dateEncodingStrategy = .iso8601
        return e
    }
}

private extension JSONDecoder {
    static var academico: JSONDecoder {
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601; return d
    }
}
