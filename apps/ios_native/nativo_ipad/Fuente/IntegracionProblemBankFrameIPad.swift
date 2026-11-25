import Foundation

enum ResultadoSincronizacionProblemaFrameIPad: Equatable {
    case sincronizado
    case yaSincronizado
    case problemaNoDisponible
    case intentoIncompleto
    case noSePudoIniciarIntentoCentral
}

/// Une la memoria espacial del frame con el historial canónico del Problem Bank
/// sin convertirlos en dos copias independientes del problema.
///
/// La sincronización es idempotente: el UUID del intento del frame viaja dentro
/// de la nota del intento central y también identifica cualquier error de paso.
/// Repetir esta función después de una interrupción no crea otro intento ni otro
/// error para el mismo evento.
enum SincronizadorProblemaFrameIPad {
    static func marcador(intentoId: UUID) -> String {
        "[frame-intento:\(intentoId.uuidString)]"
    }

    @discardableResult
    static func sincronizar(
        intento: IntentoFrameEstudioIPad,
        problemaId: UUID,
        documentoId: UInt64,
        frameId: UUID
    ) -> ResultadoSincronizacionProblemaFrameIPad {
        guard let fin = intento.fin,
              let resultado = intento.resultado,
              !intento.activo else { return .intentoIncompleto }

        let centro = CentroEstudioTecnicoIPad.compartido
        guard let problemaInicial = centro.instantanea().problemas.first(where: {
            $0.id == problemaId
        }) else { return .problemaNoDisponible }

        let marca = marcador(intentoId: intento.id)
        let yaExiste = problemaInicial.intentos.contains { $0.nota.contains(marca) }

        if !yaExiste {
            guard let intentoCentral = centro.iniciarIntento(
                problemaId, documentoId: documentoId) else {
                return .noSePudoIniciarIntentoCentral
            }
            var nota = "\(marca) · frame \(frameId.uuidString)"
            if !intento.error.isEmpty { nota += " · error: \(intento.error)" }
            centro.cerrarIntento(
                problemaId,
                intentoId: intentoCentral.id,
                correcto: resultado == .correcto,
                confianza: intento.confianza,
                segundos: max(0, fin.timeIntervalSince(intento.inicio)),
                nota: nota)
        }

        if resultado == .repetir && !intento.error.isEmpty {
            // El intento puede haberse confirmado justo antes de una caída de la
            // app. Se vuelve a leer el banco y solo se crea el error que falte.
            let problemaActual = centro.instantanea().problemas.first { $0.id == problemaId }
            let errorYaExiste = problemaActual?.erroresDePaso.contains {
                $0.elementoId == marca
            } ?? false
            if !errorYaExiste {
                centro.registrarError(
                    PasoErrorProblemaIPad(
                        fecha: fin,
                        descripcion: intento.error,
                        respuestaDada: "",
                        correccion: "",
                        elementoId: marca),
                    problemaId: problemaId)
            }
        }

        return yaExiste ? .yaSincronizado : .sincronizado
    }

    /// Repara de forma oportunista cualquier intento local que todavía no haya
    /// llegado al banco. Es seguro llamarlo al abrir el frame o tras un cierre.
    /// La misma pasada reconcilia el progreso de un simulacro activo; ese avance
    /// depende del UUID/fecha del intento local, no de que Problem Bank estuviera
    /// disponible, por lo que una caída o un vínculo roto no pierde el progreso.
    @discardableResult
    static func sincronizarTerminados(
        estudio: EstadoEstudioFrameIPad,
        documentoId: UInt64,
        frameId: UUID
    ) -> [ResultadoSincronizacionProblemaFrameIPad] {
        let resultados: [ResultadoSincronizacionProblemaFrameIPad]
        if let problemaId = estudio.problemaId {
            resultados = estudio.intentosTerminados.map {
                sincronizar(
                    intento: $0,
                    problemaId: problemaId,
                    documentoId: documentoId,
                    frameId: frameId)
            }
        } else {
            resultados = []
        }

        _ = ReconciliadorSimulacroFramesIPad.registrarTerminados(
            estudio: estudio,
            documentoId: documentoId,
            frameId: frameId)
        return resultados
    }
}
