import Foundation

enum EstadoPlanRecoveryAudioIPad: String, Codable, Equatable {
    case integro
    case parcialRecuperable
    case requiereEvidenciaManual
    case sinAudioConfirmado
}

struct IntervaloAudioRecuperableIPad: Codable, Equatable {
    var segmentoId: UUID
    var archivo: String
    var inicio: Double
    var fin: Double
    var sha256: String

    var duracion: Double { max(0, fin - inicio) }
}

struct HuecoAudioRecoveryIPad: Codable, Equatable {
    var inicio: Double
    var fin: Double
    var motivo: String

    var duracion: Double { max(0, fin - inicio) }
}

struct PlanRecoveryAudioIPad: Codable, Equatable {
    var sesionId: UUID
    var documentoId: UInt64
    var estado: EstadoPlanRecoveryAudioIPad
    var intervalosConfiables: [IntervaloAudioRecuperableIPad]
    var huecos: [HuecoAudioRecoveryIPad]
    var archivosFaltantes: [String]
    var archivosAlterados: [String]
    var archivosHuerfanos: [String]
    var duracionConfiable: Double
    var duracionTimeline: Double
    var puedeReproducirParcialmente: Bool
    var requiereDecisionUsuario: Bool
}

enum ConstructorPlanRecoveryAudioIPad {
    static func construir(
        _ diagnostico: DiagnosticoSesionAudioResilienteIPad,
        toleranciaSolape: Double = 0.020
    ) -> PlanRecoveryAudioIPad {
        let faltantes = Set(diagnostico.segmentosFaltantes)
        let alterados = Set(diagnostico.segmentosAlterados)
        let huerfanos = diagnostico.archivosHuerfanos.sorted()
        let tolerancia = max(toleranciaSolape, 0)

        let confiables = diagnostico.manifiesto.segmentos
            .filter {
                !faltantes.contains($0.archivo)
                    && !alterados.contains($0.archivo)
                    && $0.inicioSegundos.isFinite
                    && $0.duracionSegundos.isFinite
                    && $0.duracionSegundos > 0
            }
            .sorted {
                if $0.inicioSegundos != $1.inicioSegundos {
                    return $0.inicioSegundos < $1.inicioSegundos
                }
                return $0.indice < $1.indice
            }

        var intervalos: [IntervaloAudioRecuperableIPad] = []
        var huecos: [HuecoAudioRecoveryIPad] = []
        var cursor: Double = 0

        for segmento in confiables {
            let inicio = max(segmento.inicioSegundos, 0)
            let fin = inicio + segmento.duracionSegundos
            guard fin > inicio else { continue }

            if inicio > cursor + tolerancia {
                huecos.append(HuecoAudioRecoveryIPad(
                    inicio: cursor,
                    fin: inicio,
                    motivo: "No existe un segmento confirmado para este intervalo."))
            }
            intervalos.append(IntervaloAudioRecuperableIPad(
                segmentoId: segmento.id,
                archivo: segmento.archivo,
                inicio: inicio,
                fin: fin,
                sha256: segmento.sha256))
            cursor = max(cursor, fin)
        }

        // Un segmento listado pero faltante/alterado conserva su intervalo como
        // hueco explícito. No se sustituye por un huérfano que casualmente tenga
        // un nombre o duración parecida.
        let noConfiables = diagnostico.manifiesto.segmentos
            .filter { faltantes.contains($0.archivo) || alterados.contains($0.archivo) }
            .sorted { $0.inicioSegundos < $1.inicioSegundos }
        for segmento in noConfiables {
            guard segmento.inicioSegundos.isFinite,
                  segmento.duracionSegundos.isFinite,
                  segmento.duracionSegundos > 0 else { continue }
            let motivo = alterados.contains(segmento.archivo)
                ? "El segmento existe pero no coincide con su SHA-256/bytes confirmados."
                : "El segmento confirmado ya no está disponible."
            huecos.append(HuecoAudioRecoveryIPad(
                inicio: max(segmento.inicioSegundos, 0),
                fin: max(segmento.inicioSegundos, 0) + segmento.duracionSegundos,
                motivo: motivo))
        }

        huecos = normalizarHuecos(huecos, tolerancia: tolerancia)
        let duracionConfiable = intervalos.reduce(0) { $0 + $1.duracion }
        let duracionTimeline = max(
            diagnostico.manifiesto.duracionConfirmada,
            intervalos.map(\.fin).max() ?? 0,
            huecos.map(\.fin).max() ?? 0)

        let estado: EstadoPlanRecoveryAudioIPad
        if intervalos.isEmpty {
            estado = huerfanos.isEmpty && faltantes.isEmpty && alterados.isEmpty
                ? .sinAudioConfirmado : .requiereEvidenciaManual
        } else if faltantes.isEmpty && alterados.isEmpty && huerfanos.isEmpty
                    && huecos.isEmpty {
            estado = .integro
        } else if alterados.isEmpty && huerfanos.isEmpty {
            estado = .parcialRecuperable
        } else {
            estado = .requiereEvidenciaManual
        }

        return PlanRecoveryAudioIPad(
            sesionId: diagnostico.manifiesto.id,
            documentoId: diagnostico.manifiesto.documentoId,
            estado: estado,
            intervalosConfiables: intervalos,
            huecos: huecos,
            archivosFaltantes: diagnostico.segmentosFaltantes.sorted(),
            archivosAlterados: diagnostico.segmentosAlterados.sorted(),
            archivosHuerfanos: huerfanos,
            duracionConfiable: duracionConfiable,
            duracionTimeline: duracionTimeline,
            puedeReproducirParcialmente: !intervalos.isEmpty,
            requiereDecisionUsuario: !alterados.isEmpty || !huerfanos.isEmpty)
    }

    private static func normalizarHuecos(
        _ entrada: [HuecoAudioRecoveryIPad],
        tolerancia: Double
    ) -> [HuecoAudioRecoveryIPad] {
        let ordenados = entrada
            .filter { $0.fin > $0.inicio }
            .sorted { $0.inicio < $1.inicio }
        guard var actual = ordenados.first else { return [] }
        var salida: [HuecoAudioRecoveryIPad] = []
        for siguiente in ordenados.dropFirst() {
            if siguiente.inicio <= actual.fin + tolerancia {
                actual.fin = max(actual.fin, siguiente.fin)
                if siguiente.motivo != actual.motivo {
                    actual.motivo = "Hay uno o más segmentos faltantes o no confiables en este intervalo."
                }
            } else {
                salida.append(actual)
                actual = siguiente
            }
        }
        salida.append(actual)
        return salida
    }
}
