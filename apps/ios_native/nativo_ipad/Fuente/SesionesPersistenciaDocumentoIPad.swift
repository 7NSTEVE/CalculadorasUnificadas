import Foundation

struct AperturaPersistenciaDocumentoIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var procesoId: UUID
    var escenaId: String
    var abierta: Date
    var ultimoHeartbeat: Date
}

private struct ArchivoSesionPersistenciaDocumentoIPad: Codable {
    var version = 1
    var documentoId: DocumentoPersistenteID
    var aperturas: [AperturaPersistenciaDocumentoIPad] = []
    var ultimaRevisionId: UUID?
    var ultimoCierreLimpio: Date?
    var ultimoRecoveryAceptado: Date?
}

enum EstadoAntesDeAbrirDocumentoIPad: Equatable {
    case primeraApertura
    case limpio(ultimaRevisionId: UUID?)
    case yaAbiertoEnEsteProceso(escenas: [String])
    case requiereRecovery(aperturasStale: Int, ultimaRevisionId: UUID?)
}

enum ErrorSesionPersistenciaDocumentoIPad: LocalizedError, Equatable {
    case recoveryPendiente
    case escenaVacia
    case archivoSesionCorrupto

    var errorDescription: String? {
        switch self {
        case .recoveryPendiente:
            return "El documento quedó abierto por un proceso anterior y debe superar recovery antes de abrirse."
        case .escenaVacia:
            return "La apertura necesita un identificador de escena."
        case .archivoSesionCorrupto:
            return "El marcador de sesión del documento está dañado."
        }
    }
}

final class RepositorioSesionesPersistenciaDocumentoIPad {
    static let procesoActual = UUID()
    static let compartido = RepositorioSesionesPersistenciaDocumentoIPad()

    private let base: URL
    private let procesoId: UUID
    private let archivos: FileManager
    private let bloqueo = NSRecursiveLock()

    init(base: URL? = nil,
         procesoId: UUID = RepositorioSesionesPersistenciaDocumentoIPad.procesoActual,
         archivos: FileManager = .default) {
        self.procesoId = procesoId
        self.archivos = archivos
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory,
                                           in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/SesionesDocumento", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    func evaluarAntesDeAbrir(_ documentoId: DocumentoPersistenteID) throws
        -> EstadoAntesDeAbrirDocumentoIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let archivo = try cargar(documentoId) else { return .primeraApertura }
        let stale = archivo.aperturas.filter { $0.procesoId != procesoId }
        if !stale.isEmpty {
            return .requiereRecovery(
                aperturasStale: stale.count,
                ultimaRevisionId: archivo.ultimaRevisionId)
        }
        let actuales = archivo.aperturas.filter { $0.procesoId == procesoId }
        if !actuales.isEmpty {
            return .yaAbiertoEnEsteProceso(
                escenas: actuales.map(\.escenaId).sorted())
        }
        return .limpio(ultimaRevisionId: archivo.ultimaRevisionId)
    }

    /// Sólo debe llamarse después de que journal recovery + gate de revisión
    /// hayan dado un resultado aceptable. No modifica archivos del documento.
    func aceptarRecovery(_ documentoId: DocumentoPersistenteID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard var archivo = try cargar(documentoId) else { return }
        archivo.aperturas.removeAll { $0.procesoId != procesoId }
        archivo.ultimoRecoveryAceptado = Date()
        try guardar(archivo)
    }

    @discardableResult
    func abrir(_ documentoId: DocumentoPersistenteID,
               escenaId: String) throws -> AperturaPersistenciaDocumentoIPad {
        let escena = escenaId.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !escena.isEmpty else { throw ErrorSesionPersistenciaDocumentoIPad.escenaVacia }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = try cargar(documentoId)
            ?? ArchivoSesionPersistenciaDocumentoIPad(documentoId: documentoId)
        guard !archivo.aperturas.contains(where: { $0.procesoId != procesoId }) else {
            throw ErrorSesionPersistenciaDocumentoIPad.recoveryPendiente
        }
        if let existente = archivo.aperturas.first(where: {
            $0.procesoId == procesoId && $0.escenaId == escena
        }) {
            return existente
        }
        let ahora = Date()
        let apertura = AperturaPersistenciaDocumentoIPad(
            procesoId: procesoId,
            escenaId: escena,
            abierta: ahora,
            ultimoHeartbeat: ahora)
        archivo.aperturas.append(apertura)
        try guardar(archivo)
        return apertura
    }

    func heartbeat(_ documentoId: DocumentoPersistenteID,
                   escenaId: String,
                   ahora: Date = Date()) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard var archivo = try cargar(documentoId),
              let i = archivo.aperturas.firstIndex(where: {
                  $0.procesoId == procesoId && $0.escenaId == escenaId
              }) else { return }
        archivo.aperturas[i].ultimoHeartbeat = ahora
        try guardar(archivo)
    }

    func registrarRevision(_ documentoId: DocumentoPersistenteID,
                           revisionId: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = try cargar(documentoId)
            ?? ArchivoSesionPersistenciaDocumentoIPad(documentoId: documentoId)
        archivo.ultimaRevisionId = revisionId
        try guardar(archivo)
    }

    func cerrar(_ documentoId: DocumentoPersistenteID,
                escenaId: String,
                ahora: Date = Date()) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard var archivo = try cargar(documentoId) else { return }
        archivo.aperturas.removeAll {
            $0.procesoId == procesoId && $0.escenaId == escenaId
        }
        if archivo.aperturas.isEmpty {
            archivo.ultimoCierreLimpio = ahora
        }
        try guardar(archivo)
    }

    private func url(_ documentoId: DocumentoPersistenteID) -> URL {
        base.appendingPathComponent("\(documentoId.description).json")
    }

    private func cargar(_ documentoId: DocumentoPersistenteID) throws
        -> ArchivoSesionPersistenciaDocumentoIPad? {
        let ruta = url(documentoId)
        guard archivos.fileExists(atPath: ruta.path) else { return nil }
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        do {
            let archivo = try decoder.decode(
                ArchivoSesionPersistenciaDocumentoIPad.self,
                from: Data(contentsOf: ruta))
            guard archivo.documentoId == documentoId else {
                throw ErrorSesionPersistenciaDocumentoIPad.archivoSesionCorrupto
            }
            return archivo
        } catch let error as ErrorSesionPersistenciaDocumentoIPad {
            throw error
        } catch {
            throw ErrorSesionPersistenciaDocumentoIPad.archivoSesionCorrupto
        }
    }

    private func guardar(_ archivo: ArchivoSesionPersistenciaDocumentoIPad) throws {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        try encoder.encode(archivo).write(
            to: url(archivo.documentoId),
            options: [.atomic, .completeFileProtection])
    }
}

enum EstadoReaperturaDocumentoIPad: Equatable {
    case sinManifest
    case integro(revisionId: UUID, archivosVerificados: Int)
    case inconsistente(ReporteRevisionDocumentoIPad?)
}

final class GateReaperturaDocumentoIPad {
    private let estricto: GateReaperturaEstrictoIPad

    init(revisiones: RepositorioRevisionesGuardadoIPad = .compartido) {
        self.estricto = GateReaperturaEstrictoIPad(revisiones: revisiones)
    }

    func verificar(_ documentoId: DocumentoPersistenteID) -> EstadoReaperturaDocumentoIPad {
        switch estricto.verificar(documentoId) {
        case .sinManifest:
            return .sinManifest
        case .integro(let revisionId, let archivosVerificados):
            return .integro(revisionId: revisionId, archivosVerificados: archivosVerificados)
        case .contenidoInconsistente(let reporte):
            return .inconsistente(reporte)
        case .manifestCorrupto, .manifestDeOtroDocumento:
            return .inconsistente(nil)
        }
    }
}

struct ResultadoPreaperturaSeguraIPad: Equatable {
    enum Decision: Equatable {
        case abrir
        case abrirTrasRecovery
        case bloquearRecovery
        case bloquearIntegridad
    }

    var decision: Decision
    var estadoSesion: EstadoAntesDeAbrirDocumentoIPad
    var estadoRevision: EstadoReaperturaDocumentoIPad?
}

/// Une el marcador de cierre limpio con el manifest de revisión. El recovery de
/// journals global se ejecuta antes en `GateIntegridadArranqueIPad`.
final class GatePreaperturaSeguraIPad {
    private let sesiones: RepositorioSesionesPersistenciaDocumentoIPad
    private let revisiones: GateReaperturaDocumentoIPad

    init(sesiones: RepositorioSesionesPersistenciaDocumentoIPad = .compartido,
         revisiones: GateReaperturaDocumentoIPad = .init()) {
        self.sesiones = sesiones
        self.revisiones = revisiones
    }

    func evaluar(_ documentoId: DocumentoPersistenteID) -> ResultadoPreaperturaSeguraIPad {
        let sesion: EstadoAntesDeAbrirDocumentoIPad
        do { sesion = try sesiones.evaluarAntesDeAbrir(documentoId) }
        catch {
            return .init(decision: .bloquearRecovery,
                         estadoSesion: .requiereRecovery(
                            aperturasStale: 1, ultimaRevisionId: nil),
                         estadoRevision: nil)
        }

        let revision = revisiones.verificar(documentoId)
        if case .inconsistente = revision {
            return .init(decision: .bloquearIntegridad,
                         estadoSesion: sesion,
                         estadoRevision: revision)
        }

        switch sesion {
        case .requiereRecovery:
            return .init(decision: .abrirTrasRecovery,
                         estadoSesion: sesion,
                         estadoRevision: revision)
        default:
            return .init(decision: .abrir,
                         estadoSesion: sesion,
                         estadoRevision: revision)
        }
    }
}
