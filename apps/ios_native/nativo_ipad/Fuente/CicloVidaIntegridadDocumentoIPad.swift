import Foundation

struct ReporteArranqueIntegridadIPad: Equatable {
    var limpiezaHuerfanos: ReporteLimpiezaTransaccionesHuerfanasIPad
    var recovery: EstadoGateIntegridadIPad

    var permiteRestaurarBiblioteca: Bool {
        if case .bloqueado = recovery { return false }
        return true
    }
}

enum DecisionAperturaIntegridadIPad: Equatable {
    case abrir
    case requiereAceptarRecovery(EstadoReaperturaEstrictoIPad)
    case bloquearManifest(EstadoReaperturaEstrictoIPad)
    case bloquearContenido(ReporteRevisionDocumentoIPad)
    case errorSesion(String)
}

/// Fachada de orden de operaciones P0 para el shell:
/// 1) limpia sólo staging huérfano antiguo;
/// 2) recupera journals;
/// 3) antes de cada documento revisa sesión stale + manifest/contenido;
/// 4) sólo después registra la apertura de escena.
///
/// No contiene UI ni serialización de documento, por lo que puede integrarse en
/// AppPrincipal/SceneDelegate sin transferir ownership de esos archivos.
final class CoordinadorCicloVidaIntegridadIPad {
    private let transacciones: CoordinadorPersistenciaTransaccionalIPad
    private let sesiones: RepositorioSesionesPersistenciaDocumentoIPad
    private let revisiones: RepositorioRevisionesGuardadoIPad
    private let baseTransacciones: URL
    private let archivos: FileManager

    init(transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
         sesiones: RepositorioSesionesPersistenciaDocumentoIPad = .compartido,
         revisiones: RepositorioRevisionesGuardadoIPad = .compartido,
         baseTransacciones: URL? = nil,
         archivos: FileManager = .default) {
        self.transacciones = transacciones
        self.sesiones = sesiones
        self.revisiones = revisiones
        self.archivos = archivos
        self.baseTransacciones = baseTransacciones ?? archivos.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/Transacciones", isDirectory: true)
    }

    func prepararArranque(ahora: Date = Date()) -> ReporteArranqueIntegridadIPad {
        let limpieza = LimpiadorTransaccionesHuerfanasIPad.ejecutar(
            base: baseTransacciones,
            antiguedadMinima: 15 * 60,
            ahora: ahora,
            archivos: archivos)
        let recovery = GateIntegridadArranqueIPad(
            transacciones: transacciones).ejecutar()
        return .init(limpiezaHuerfanos: limpieza, recovery: recovery)
    }

    func evaluarApertura(_ documentoId: DocumentoPersistenteID)
        -> DecisionAperturaIntegridadIPad {
        let sesion: EstadoAntesDeAbrirDocumentoIPad
        do {
            sesion = try sesiones.evaluarAntesDeAbrir(documentoId)
        } catch {
            return .errorSesion(error.localizedDescription)
        }

        let estadoRevision = GateReaperturaEstrictoIPad(
            revisiones: revisiones).verificar(documentoId)

        switch estadoRevision {
        case .manifestCorrupto, .manifestDeOtroDocumento:
            return .bloquearManifest(estadoRevision)
        case .contenidoInconsistente(let reporte):
            return .bloquearContenido(reporte)
        case .sinManifest, .integro:
            break
        }

        if case .requiereRecovery = sesion {
            return .requiereAceptarRecovery(estadoRevision)
        }
        return .abrir
    }

    @discardableResult
    func abrir(_ documentoId: DocumentoPersistenteID,
               escenaId: String) throws -> AperturaPersistenciaDocumentoIPad {
        switch evaluarApertura(documentoId) {
        case .abrir:
            return try sesiones.abrir(documentoId, escenaId: escenaId)
        case .requiereAceptarRecovery:
            throw ErrorSesionPersistenciaDocumentoIPad.recoveryPendiente
        case .bloquearManifest:
            throw ErrorRevisionGuardadoDocumentoIPad.manifestCorrupto
        case .bloquearContenido:
            throw ErrorPersistenciaTransaccionalIPad.journalInvalido
        case .errorSesion:
            throw ErrorSesionPersistenciaDocumentoIPad.archivoSesionCorrupto
        }
    }

    /// El shell sólo debe invocarlo después de mostrar qué recovery ocurrió y
    /// comprobar que la revisión actual no está corrupta/inconsistente.
    func aceptarRecoveryYHabilitarApertura(
        _ documentoId: DocumentoPersistenteID
    ) throws {
        let estado = GateReaperturaEstrictoIPad(
            revisiones: revisiones).verificar(documentoId)
        switch estado {
        case .sinManifest, .integro:
            try sesiones.aceptarRecovery(documentoId)
        case .contenidoInconsistente:
            throw ErrorPersistenciaTransaccionalIPad.journalInvalido
        case .manifestCorrupto:
            throw ErrorRevisionGuardadoDocumentoIPad.manifestCorrupto
        case .manifestDeOtroDocumento:
            throw ErrorRevisionGuardadoDocumentoIPad.documentoDistinto
        }
    }

    func cerrar(_ documentoId: DocumentoPersistenteID,
                escenaId: String) throws {
        try sesiones.cerrar(documentoId, escenaId: escenaId)
    }

    func heartbeat(_ documentoId: DocumentoPersistenteID,
                   escenaId: String,
                   ahora: Date = Date()) throws {
        try sesiones.heartbeat(documentoId,
                               escenaId: escenaId,
                               ahora: ahora)
    }
}
