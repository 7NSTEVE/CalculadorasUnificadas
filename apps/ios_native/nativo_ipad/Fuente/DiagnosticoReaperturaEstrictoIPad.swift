import Foundation

enum EstadoReaperturaEstrictoIPad: Equatable {
    case sinManifest
    case integro(revisionId: UUID, archivosVerificados: Int)
    case contenidoInconsistente(ReporteRevisionDocumentoIPad)
    case manifestCorrupto(ruta: String)
    case manifestDeOtroDocumento(ruta: String)
}

/// Variante diagnóstica del gate de reapertura. Mantiene separados los fallos
/// del inventario de contenido y los del propio manifest; así recovery puede
/// elegir una estrategia segura en vez de presentar todos como "hash distinto".
final class GateReaperturaEstrictoIPad {
    private let revisiones: RepositorioRevisionesGuardadoIPad

    init(revisiones: RepositorioRevisionesGuardadoIPad = .compartido) {
        self.revisiones = revisiones
    }

    func verificar(_ documentoId: DocumentoPersistenteID) -> EstadoReaperturaEstrictoIPad {
        do {
            let reporte = try revisiones.verificar(documentoId)
            return reporte.integra
                ? .integro(revisionId: reporte.revisionId,
                           archivosVerificados: reporte.archivosVerificados)
                : .contenidoInconsistente(reporte)
        } catch ErrorRevisionGuardadoDocumentoIPad.manifestNoExiste {
            return .sinManifest
        } catch ErrorRevisionGuardadoDocumentoIPad.documentoDistinto {
            return .manifestDeOtroDocumento(ruta: revisiones.url(documentoId).path)
        } catch {
            return .manifestCorrupto(ruta: revisiones.url(documentoId).path)
        }
    }
}
