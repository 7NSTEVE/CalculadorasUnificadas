import Foundation

protocol ParticipanteGuardadoDocumentoIPad: AnyObject {
    var nombreParticipanteGuardado: String { get }
    func cambiosParaGuardar(documentoId: DocumentoPersistenteID) throws
        -> [CambioArchivoTransaccionalIPad]
    func guardadoConfirmado(documentoId: DocumentoPersistenteID)
    func guardadoAbortado(documentoId: DocumentoPersistenteID, error: Error)
}

extension ParticipanteGuardadoDocumentoIPad {
    func guardadoConfirmado(documentoId: DocumentoPersistenteID) {}
    func guardadoAbortado(documentoId: DocumentoPersistenteID, error: Error) {}
}

struct ResultadoGuardadoDocumentoAtomicoIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var transaccionId: UUID?
    var participantes: [String]
    var archivos: Int
    var huboCambios: Bool { archivos > 0 }
}

enum ErrorGuardadoDocumentoAtomicoIPad: LocalizedError, Equatable {
    case participanteDuplicado(String)

    var errorDescription: String? {
        switch self {
        case .participanteDuplicado(let nombre):
            return "El participante de guardado «\(nombre)» fue registrado más de una vez."
        }
    }
}

/// Punto único para un save lógico de documento. Los subsistemas siguen siendo
/// dueños de su serialización; sólo entregan `CambioArchivoTransaccionalIPad`.
/// Así no se copia estado ni se crea un repositorio paralelo.
final class CoordinadorGuardadoDocumentoAtomicoIPad {
    private let transacciones: CoordinadorPersistenciaTransaccionalIPad

    init(transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido) {
        self.transacciones = transacciones
    }

    @discardableResult
    func guardar(documentoId: DocumentoPersistenteID,
                 participantes: [ParticipanteGuardadoDocumentoIPad]) throws
        -> ResultadoGuardadoDocumentoAtomicoIPad {
        var nombres = Set<String>()
        for participante in participantes {
            guard nombres.insert(participante.nombreParticipanteGuardado).inserted else {
                throw ErrorGuardadoDocumentoAtomicoIPad.participanteDuplicado(
                    participante.nombreParticipanteGuardado)
            }
        }

        var cambios: [CambioArchivoTransaccionalIPad] = []
        do {
            for participante in participantes {
                cambios.append(contentsOf: try participante.cambiosParaGuardar(
                    documentoId: documentoId))
            }
        } catch {
            participantes.forEach {
                $0.guardadoAbortado(documentoId: documentoId, error: error)
            }
            throw error
        }

        guard !cambios.isEmpty else {
            participantes.forEach { $0.guardadoConfirmado(documentoId: documentoId) }
            return .init(documentoId: documentoId,
                         transaccionId: nil,
                         participantes: participantes.map(\.nombreParticipanteGuardado),
                         archivos: 0)
        }

        do {
            let preparada = try transacciones.ejecutar(
                documentoId: documentoId,
                cambios: cambios)
            participantes.forEach { $0.guardadoConfirmado(documentoId: documentoId) }
            return .init(documentoId: documentoId,
                         transaccionId: preparada.id,
                         participantes: participantes.map(\.nombreParticipanteGuardado),
                         archivos: cambios.count)
        } catch {
            participantes.forEach {
                $0.guardadoAbortado(documentoId: documentoId, error: error)
            }
            throw error
        }
    }
}

struct ConflictoRecoveryDocumentoIPad: Equatable, Identifiable {
    var id: UUID
    var documentoId: DocumentoPersistenteID
    var rutas: [String]
    var actualizado: Date
}

enum EstadoGateIntegridadIPad: Equatable {
    case limpio
    case recuperado(transacciones: Int)
    case bloqueado(conflictos: [ConflictoRecoveryDocumentoIPad])
}

/// Debe ejecutarse antes de restaurar pestañas/documentos. Si recovery detecta
/// contenido externo inesperado, el shell puede bloquear sólo esos documentos
/// y ofrecer diagnóstico en vez de continuar con datos ambiguos.
final class GateIntegridadArranqueIPad {
    private let transacciones: CoordinadorPersistenciaTransaccionalIPad

    init(transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido) {
        self.transacciones = transacciones
    }

    func ejecutar() -> EstadoGateIntegridadIPad {
        let reporte = transacciones.recuperarPendientes()
        let conflictos: [ConflictoRecoveryDocumentoIPad] = reporte.conflictos.compactMap { id in
            guard let journal = try? transacciones.journal(id) else { return nil }
            return .init(
                id: id,
                documentoId: journal.documentoId,
                rutas: journal.entradas.map(\.destinoPath),
                actualizado: journal.actualizado)
        }
        if !conflictos.isEmpty { return .bloqueado(conflictos: conflictos) }
        if !reporte.restauradas.isEmpty {
            return .recuperado(transacciones: reporte.restauradas.count)
        }
        return .limpio
    }
}
