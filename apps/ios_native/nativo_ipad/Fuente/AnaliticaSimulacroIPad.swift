import Foundation

struct ResultadoEjercicioSimulacroIPad: Equatable, Identifiable {
    var id: UUID { frameId }
    var frameId: UUID
    var titulo: String
    var resultado: ResultadoIntentoFrameIPad
    var confianza: Double
    var segundos: TimeInterval
    var error: String
}

struct InformeSimulacroIPad: Equatable {
    var resultados: [ResultadoEjercicioSimulacroIPad]
    var aciertos: Int
    var fallos: Int
    var confianzaPromedio: Double
    var segundosPromedio: TimeInterval
    var aciertosInseguros: [ResultadoEjercicioSimulacroIPad]
    var erroresAltaConfianza: [ResultadoEjercicioSimulacroIPad]
    var ejerciciosLentos: [ResultadoEjercicioSimulacroIPad]

    var precision: Double {
        guard !resultados.isEmpty else { return 0 }
        return Double(aciertos) / Double(resultados.count)
    }

    var requiereRevisionPrioritaria: Bool {
        !erroresAltaConfianza.isEmpty || !aciertosInseguros.isEmpty
    }
}

/// Reconstruye el resultado usando el UUID de intento guardado en el sidecar.
/// Así un repaso posterior del mismo frame no reescribe retrospectivamente el
/// informe del simulacro ya terminado.
enum RecopiladorResultadosSimulacroIPad {
    static func recopilar(
        sesion: EstadoSesionSimulacroIPad,
        estudiosPorFrame: [UUID: EstadoEstudioFrameIPad],
        titulosPorFrame: [UUID: String]
    ) -> [ResultadoEjercicioSimulacroIPad] {
        sesion.plan.frameIds.compactMap { frameId in
            guard let intentoId = sesion.intentoId(para: frameId),
                  let estudio = estudiosPorFrame[frameId],
                  let intento = estudio.intentos.first(where: { $0.id == intentoId }),
                  let resultado = intento.resultado,
                  intento.fin != nil else { return nil }
            return ResultadoEjercicioSimulacroIPad(
                frameId: frameId,
                titulo: titulosPorFrame[frameId] ?? "Ejercicio",
                resultado: resultado,
                confianza: intento.confianza,
                segundos: intento.duracion ?? 0,
                error: intento.error)
        }
    }
}

/// No intenta diagnosticar dominio ni usar una puntuación opaca. Separa señales
/// que requieren estrategias distintas: acertar dudando, fallar estando seguro
/// y resolver mucho más lento que el resto de la propia sesión.
enum AnalizadorSimulacroIPad {
    static func analizar(
        _ resultados: [ResultadoEjercicioSimulacroIPad]
    ) -> InformeSimulacroIPad {
        guard !resultados.isEmpty else {
            return InformeSimulacroIPad(
                resultados: [], aciertos: 0, fallos: 0,
                confianzaPromedio: 0, segundosPromedio: 0,
                aciertosInseguros: [], erroresAltaConfianza: [],
                ejerciciosLentos: [])
        }

        let normalizados = resultados.map { original -> ResultadoEjercicioSimulacroIPad in
            var valor = original
            valor.confianza = min(max(valor.confianza, 0), 1)
            valor.segundos = max(valor.segundos, 0)
            valor.error = valor.error.trimmingCharacters(in: .whitespacesAndNewlines)
            return valor
        }
        let aciertos = normalizados.filter { $0.resultado == .correcto }
        let fallos = normalizados.filter { $0.resultado == .repetir }
        let confianza = normalizados.reduce(0) { $0 + $1.confianza }
            / Double(normalizados.count)
        let promedioSegundos = normalizados.reduce(0) { $0 + $1.segundos }
            / Double(normalizados.count)
        let mediana = medianaSegundos(normalizados.map(\.segundos))
        let umbralLento = max(mediana * 1.6, mediana + 90)

        return InformeSimulacroIPad(
            resultados: normalizados,
            aciertos: aciertos.count,
            fallos: fallos.count,
            confianzaPromedio: confianza,
            segundosPromedio: promedioSegundos,
            aciertosInseguros: aciertos.filter { $0.confianza < 0.45 },
            erroresAltaConfianza: fallos.filter { $0.confianza >= 0.75 },
            ejerciciosLentos: normalizados.filter {
                normalizados.count >= 3 && $0.segundos > umbralLento
            })
    }

    static func prioridadRevision(
        _ informe: InformeSimulacroIPad
    ) -> [ResultadoEjercicioSimulacroIPad] {
        var vistos = Set<UUID>()
        var salida: [ResultadoEjercicioSimulacroIPad] = []
        let grupos = [
            informe.erroresAltaConfianza,
            informe.aciertosInseguros,
            informe.resultados.filter { $0.resultado == .repetir },
            informe.ejerciciosLentos,
        ]
        for grupo in grupos {
            for resultado in grupo where vistos.insert(resultado.frameId).inserted {
                salida.append(resultado)
            }
        }
        return salida
    }

    private static func medianaSegundos(_ valores: [TimeInterval]) -> TimeInterval {
        let ordenados = valores.sorted()
        guard !ordenados.isEmpty else { return 0 }
        let mitad = ordenados.count / 2
        if ordenados.count.isMultiple(of: 2) {
            return (ordenados[mitad - 1] + ordenados[mitad]) / 2
        }
        return ordenados[mitad]
    }
}
