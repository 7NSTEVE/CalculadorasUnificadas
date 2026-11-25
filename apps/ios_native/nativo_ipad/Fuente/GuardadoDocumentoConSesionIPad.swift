import Foundation

struct ResultadoGuardadoConSesionIPad: Equatable {
    var guardado: ResultadoGuardadoDocumentoAtomicoIPad
    var revisionRegistradaEnSesion: Bool
    var advertenciaSesion: String?
}

/// Decorador del guardado P0. La persistencia documental sigue siendo la fuente
/// de verdad; el marcador de sesión sólo recuerda qué revisión vio el proceso.
/// Si ese marcador auxiliar falla DESPUÉS de un commit confirmado, no se revierte
/// ni se reporta como pérdida de datos: se expone una advertencia diagnosticable.
final class CoordinadorGuardadoConSesionIPad {
    private let guardado: CoordinadorGuardadoDocumentoAtomicoIPad
    private let sesiones: RepositorioSesionesPersistenciaDocumentoIPad

    init(guardado: CoordinadorGuardadoDocumentoAtomicoIPad = .init(),
         sesiones: RepositorioSesionesPersistenciaDocumentoIPad = .compartido) {
        self.guardado = guardado
        self.sesiones = sesiones
    }

    @discardableResult
    func guardar(documentoId: DocumentoPersistenteID,
                 participantes: [ParticipanteGuardadoDocumentoIPad]) throws
        -> ResultadoGuardadoConSesionIPad {
        let resultado = try guardado.guardar(
            documentoId: documentoId,
            participantes: participantes)

        guard let revisionId = resultado.revisionId else {
            return .init(
                guardado: resultado,
                revisionRegistradaEnSesion: false,
                advertenciaSesion: nil)
        }

        do {
            try sesiones.registrarRevision(documentoId, revisionId: revisionId)
            return .init(
                guardado: resultado,
                revisionRegistradaEnSesion: true,
                advertenciaSesion: nil)
        } catch {
            return .init(
                guardado: resultado,
                revisionRegistradaEnSesion: false,
                advertenciaSesion: error.localizedDescription)
        }
    }
}
