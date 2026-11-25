import Foundation

enum DuracionSimulacroIPad: Int, CaseIterable, Codable {
    case rapido = 10
    case normal = 25
    case largo = 45

    var nombre: String { "\(rawValue) min" }
    var segundos: TimeInterval { TimeInterval(rawValue * 60) }
}

struct CandidatoSimulacroFrameIPad: Equatable, Identifiable {
    var id: UUID { frameId }
    var frameId: UUID
    var problemaId: UUID?
    var titulo: String
    var estudio: EstadoEstudioFrameIPad
}

struct PlanSimulacroFramesIPad: Codable, Equatable {
    var creado: Date
    var duracion: DuracionSimulacroIPad
    var frameIds: [UUID]

    var cantidad: Int { frameIds.count }
}

/// Selecciona qué resolver sin usar red ni IA. Reutiliza la prioridad explicable
/// del repaso adaptativo y evita meter dos copias del mismo problema en un
/// simulacro cuando varios frames apuntan al mismo `problemaId`.
enum PlanificadorSimulacroFramesIPad {
    static func construir(
        candidatos: [CandidatoSimulacroFrameIPad],
        cantidadSolicitada: Int,
        duracion: DuracionSimulacroIPad,
        ahora: Date = Date()
    ) -> PlanSimulacroFramesIPad {
        let cantidad = min(max(cantidadSolicitada, 1), 50)
        var unicos: [CandidatoSimulacroFrameIPad] = []
        var problemasVistos = Set<UUID>()
        var framesVistos = Set<UUID>()

        for candidato in candidatos {
            guard framesVistos.insert(candidato.frameId).inserted else { continue }
            if let problemaId = candidato.problemaId,
               !problemasVistos.insert(problemaId).inserted { continue }
            unicos.append(candidato)
        }

        let ordenados = unicos.sorted { izquierda, derecha in
            if PlanificadorFramesEstudioIPad.esMejor(
                izquierda.estudio, que: derecha.estudio, ahora: ahora) { return true }
            if PlanificadorFramesEstudioIPad.esMejor(
                derecha.estudio, que: izquierda.estudio, ahora: ahora) { return false }
            return izquierda.titulo.localizedCaseInsensitiveCompare(derecha.titulo)
                == .orderedAscending
        }

        return PlanSimulacroFramesIPad(
            creado: ahora,
            duracion: duracion,
            frameIds: Array(ordenados.prefix(cantidad).map(\.frameId)))
    }
}

struct EstadoSesionSimulacroIPad: Codable, Equatable {
    var plan: PlanSimulacroFramesIPad
    var inicio: Date
    /// Se rellena al persistir/cargar el sidecar. Es opcional para conservar
    /// compatibilidad con sesiones creadas antes de esta versión.
    private(set) var documentoId: UInt64?
    private(set) var indiceActual = 0
    private(set) var completados = Set<UUID>()
    private(set) var omitidos = Set<UUID>()
    private(set) var intentosContabilizados = Set<UUID>()
    /// Une cada frame con el intento exacto que lo dio por completado. Es
    /// opcional al decodificar para que sidecars creados por builds anteriores
    /// sigan abriendo sin migración obligatoria.
    private(set) var intentoPorFrame: [UUID: UUID] = [:]
    private(set) var finalizado: Date?

    init(
        plan: PlanSimulacroFramesIPad,
        inicio: Date = Date(),
        documentoId: UInt64? = nil
    ) {
        self.plan = plan
        self.inicio = inicio
        self.documentoId = documentoId
    }

    private enum CodingKeys: String, CodingKey {
        case plan, inicio, documentoId, indiceActual, completados, omitidos
        case intentosContabilizados, intentoPorFrame, finalizado
    }

    init(from decoder: Decoder) throws {
        let contenedor = try decoder.container(keyedBy: CodingKeys.self)
        plan = try contenedor.decode(PlanSimulacroFramesIPad.self, forKey: .plan)
        inicio = try contenedor.decode(Date.self, forKey: .inicio)
        documentoId = try contenedor.decodeIfPresent(UInt64.self, forKey: .documentoId)
        indiceActual = try contenedor.decodeIfPresent(Int.self, forKey: .indiceActual) ?? 0
        completados = try contenedor.decodeIfPresent(Set<UUID>.self, forKey: .completados) ?? []
        omitidos = try contenedor.decodeIfPresent(Set<UUID>.self, forKey: .omitidos) ?? []
        intentosContabilizados = try contenedor.decodeIfPresent(
            Set<UUID>.self, forKey: .intentosContabilizados) ?? []
        intentoPorFrame = try contenedor.decodeIfPresent(
            [UUID: UUID].self, forKey: .intentoPorFrame) ?? [:]
        finalizado = try contenedor.decodeIfPresent(Date.self, forKey: .finalizado)
    }

    func encode(to encoder: Encoder) throws {
        var contenedor = encoder.container(keyedBy: CodingKeys.self)
        try contenedor.encode(plan, forKey: .plan)
        try contenedor.encode(inicio, forKey: .inicio)
        try contenedor.encodeIfPresent(documentoId, forKey: .documentoId)
        try contenedor.encode(indiceActual, forKey: .indiceActual)
        try contenedor.encode(completados, forKey: .completados)
        try contenedor.encode(omitidos, forKey: .omitidos)
        try contenedor.encode(intentosContabilizados, forKey: .intentosContabilizados)
        try contenedor.encode(intentoPorFrame, forKey: .intentoPorFrame)
        try contenedor.encodeIfPresent(finalizado, forKey: .finalizado)
    }

    mutating func asignarDocumentoSiFalta(_ documento: UInt64) {
        if documentoId == nil { documentoId = documento }
    }

    var frameActualId: UUID? {
        guard finalizado == nil, plan.frameIds.indices.contains(indiceActual) else { return nil }
        return plan.frameIds[indiceActual]
    }

    var terminados: Int { completados.count }
    var omitidosTotal: Int { omitidos.count }
    var restantes: Int { max(plan.cantidad - indiceActual, 0) }
    var progreso: Double {
        guard plan.cantidad > 0 else { return 1 }
        return min(max(Double(indiceActual) / Double(plan.cantidad), 0), 1)
    }

    func intentoId(para frameId: UUID) -> UUID? { intentoPorFrame[frameId] }

    func segundosRestantes(ahora: Date = Date()) -> TimeInterval {
        max(0, plan.duracion.segundos - ahora.timeIntervalSince(inicio))
    }

    func agotado(ahora: Date = Date()) -> Bool {
        segundosRestantes(ahora: ahora) <= 0
    }

    /// Avanza solo si el intento pertenece al frame actual, terminó después de
    /// iniciar el simulacro y todavía no había sido contabilizado. Esto permite
    /// reparar un cierre después de una caída sin adelantar dos veces.
    @discardableResult
    mutating func registrarCierre(
        frameId: UUID,
        intentoId: UUID,
        fin: Date,
        ahora: Date = Date()
    ) -> Bool {
        guard frameActualId == frameId,
              fin >= inicio,
              !intentosContabilizados.contains(intentoId) else { return false }
        intentosContabilizados.insert(intentoId)
        intentoPorFrame[frameId] = intentoId
        completados.insert(frameId)
        avanzar(ahora: ahora)
        return true
    }

    @discardableResult
    mutating func completarActual(ahora: Date = Date()) -> UUID? {
        guard let actual = frameActualId else { return nil }
        completados.insert(actual)
        avanzar(ahora: ahora)
        return actual
    }

    @discardableResult
    mutating func omitirActual(ahora: Date = Date()) -> UUID? {
        guard let actual = frameActualId else { return nil }
        omitidos.insert(actual)
        avanzar(ahora: ahora)
        return actual
    }

    mutating func finalizar(ahora: Date = Date()) {
        guard finalizado == nil else { return }
        finalizado = max(ahora, inicio)
    }

    mutating func finalizarSiAgotado(ahora: Date = Date()) {
        if agotado(ahora: ahora) { finalizar(ahora: ahora) }
    }

    private mutating func avanzar(ahora: Date) {
        indiceActual += 1
        if indiceActual >= plan.frameIds.count || agotado(ahora: ahora) {
            finalizar(ahora: ahora)
        }
    }

    func resumen(ahora: Date = Date()) -> ResumenSimulacroIPad {
        ResumenSimulacroIPad(
            total: plan.cantidad,
            completados: completados.count,
            omitidos: omitidos.count,
            segundosUsados: max(0, (finalizado ?? ahora).timeIntervalSince(inicio)))
    }
}

struct ResumenSimulacroIPad: Equatable {
    var total: Int
    var completados: Int
    var omitidos: Int
    var segundosUsados: TimeInterval

    var porcentajeCompletado: Double {
        guard total > 0 else { return 1 }
        return min(max(Double(completados) / Double(total), 0), 1)
    }
}

/// Sidecar pequeño por documento. El simulacro es estado de estudio, no tinta;
/// aun así debe sobrevivir cierres de escena o terminaciones del proceso para
/// que una interrupción no haga perder el progreso del usuario.
final class RepositorioSimulacroDocumentoIPad {
    static let compartido = RepositorioSimulacroDocumentoIPad()

    private let base: URL
    private let archivos: FileManager
    private let cola = DispatchQueue(label: "cuadernu.simulacro.documento")

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        if let base {
            self.base = base
        } else {
            let soporte = archivos.urls(for: .applicationSupportDirectory,
                                        in: .userDomainMask)[0]
            self.base = soporte.appendingPathComponent(
                "Tinta/Simulacros", isDirectory: true)
        }
        try? archivos.createDirectory(at: self.base,
                                      withIntermediateDirectories: true)
    }

    func cargar(documento: UInt64, ahora: Date = Date()) -> EstadoSesionSimulacroIPad? {
        cola.sync {
            guard let datos = try? Data(contentsOf: url(documento)),
                  var sesion = try? JSONDecoder().decode(
                    EstadoSesionSimulacroIPad.self, from: datos) else { return nil }
            sesion.asignarDocumentoSiFalta(documento)
            let estabaActiva = sesion.frameActualId != nil
            sesion.finalizarSiAgotado(ahora: ahora)
            if estabaActiva && sesion.frameActualId == nil {
                try? persistir(sesion, documento: documento)
            }
            return sesion
        }
    }

    @discardableResult
    func guardar(_ sesion: EstadoSesionSimulacroIPad, documento: UInt64) -> Bool {
        cola.sync {
            do {
                var copia = sesion
                copia.asignarDocumentoSiFalta(documento)
                try persistir(copia, documento: documento)
                return true
            } catch {
                return false
            }
        }
    }

    @discardableResult
    func eliminar(documento: UInt64) -> Bool {
        cola.sync {
            let destino = url(documento)
            guard archivos.fileExists(atPath: destino.path) else { return true }
            do {
                try archivos.removeItem(at: destino)
                return true
            } catch {
                return false
            }
        }
    }

    private func url(_ documento: UInt64) -> URL {
        base.appendingPathComponent("\(documento).json")
    }

    private func persistir(
        _ sesion: EstadoSesionSimulacroIPad,
        documento: UInt64
    ) throws {
        let datos = try JSONEncoder().encode(sesion)
        try datos.write(to: url(documento), options: .atomic)
    }
}

/// Reconciliación oportunista entre el intento ya persistido en el frame y el
/// sidecar del simulacro. Puede llamarse varias veces después de un cierre.
enum ReconciliadorSimulacroFramesIPad {
    @discardableResult
    static func registrarTerminados(
        estudio: EstadoEstudioFrameIPad,
        documentoId: UInt64,
        frameId: UUID,
        ahora: Date = Date(),
        repositorio: RepositorioSimulacroDocumentoIPad = .compartido
    ) -> EstadoSesionSimulacroIPad? {
        guard var sesion = repositorio.cargar(documento: documentoId, ahora: ahora),
              sesion.frameActualId == frameId else { return nil }

        let candidatos = estudio.intentosTerminados
            .compactMap { intento -> (UUID, Date)? in
                guard let fin = intento.fin, fin >= sesion.inicio else { return nil }
                return (intento.id, fin)
            }
            .sorted { $0.1 < $1.1 }

        var cambio = false
        for (intentoId, fin) in candidatos {
            if sesion.registrarCierre(
                frameId: frameId, intentoId: intentoId,
                fin: fin, ahora: ahora) {
                cambio = true
                break
            }
        }
        guard cambio else { return sesion }
        return repositorio.guardar(sesion, documento: documentoId) ? sesion : nil
    }
}
