import Foundation

enum TipoRecuperacionSimulacroIPad: String, Codable, CaseIterable {
    case corregirModeloMental
    case reconstruirProcedimiento
    case consolidarConfianza
    case ganarFluidez

    var nombre: String {
        switch self {
        case .corregirModeloMental: return "Corregir modelo mental"
        case .reconstruirProcedimiento: return "Reconstruir procedimiento"
        case .consolidarConfianza: return "Consolidar confianza"
        case .ganarFluidez: return "Ganar fluidez"
        }
    }
}

enum PrioridadRecuperacionSimulacroIPad: Int, Codable, Comparable {
    case critica = 0
    case alta = 1
    case media = 2
    case baja = 3

    static func < (
        izquierda: PrioridadRecuperacionSimulacroIPad,
        derecha: PrioridadRecuperacionSimulacroIPad
    ) -> Bool {
        izquierda.rawValue < derecha.rawValue
    }

    var nombre: String {
        switch self {
        case .critica: return "Crítica"
        case .alta: return "Alta"
        case .media: return "Media"
        case .baja: return "Baja"
        }
    }
}

struct AccionRecuperacionSimulacroIPad: Equatable, Identifiable {
    var id: UUID { frameId }
    var frameId: UUID
    var titulo: String
    var tipo: TipoRecuperacionSimulacroIPad
    var prioridad: PrioridadRecuperacionSimulacroIPad
    var senales: [String]
    var instruccion: String
    var criterioCierre: String
}

/// Convierte las señales ya explicables del simulacro en una secuencia concreta
/// de recuperación. No calcula una puntuación de dominio ni agenda el próximo
/// repaso: únicamente decide qué conviene revisar primero dentro de este informe.
enum PlanificadorRecuperacionSimulacroIPad {
    static func construir(
        _ informe: InformeSimulacroIPad,
        limite: Int = 6
    ) -> [AccionRecuperacionSimulacroIPad] {
        let limiteSeguro = max(limite, 0)
        guard limiteSeguro > 0, !informe.resultados.isEmpty else { return [] }

        let erroresAltaConfianza = Set(informe.erroresAltaConfianza.map(\.frameId))
        let aciertosInseguros = Set(informe.aciertosInseguros.map(\.frameId))
        let lentos = Set(informe.ejerciciosLentos.map(\.frameId))

        var vistos = Set<UUID>()
        var candidatos: [(indice: Int, accion: AccionRecuperacionSimulacroIPad)] = []

        for (indice, resultado) in informe.resultados.enumerated() {
            guard vistos.insert(resultado.frameId).inserted else { continue }

            let esErrorAltaConfianza = erroresAltaConfianza.contains(resultado.frameId)
            let esFallo = resultado.resultado == .repetir
            let esAciertoInseguro = aciertosInseguros.contains(resultado.frameId)
            let esLento = lentos.contains(resultado.frameId)
            guard esErrorAltaConfianza || esFallo || esAciertoInseguro || esLento else {
                continue
            }

            let estrategia: (
                TipoRecuperacionSimulacroIPad,
                PrioridadRecuperacionSimulacroIPad,
                String,
                String
            )

            if esErrorAltaConfianza {
                estrategia = (
                    .corregirModeloMental,
                    .critica,
                    "Antes de ver la solución, escriba qué ley, regla o supuesto aplicó y por qué. Luego compare y marque el primer paso donde su procedimiento diverge.",
                    "Puede explicar el error y la corrección con sus propias palabras sin consultar la solución."
                )
            } else if esFallo {
                estrategia = (
                    .reconstruirProcedimiento,
                    .alta,
                    "Rehaga el ejercicio desde cero y marque el primer paso que no puede justificar. Compare con la solución únicamente después de cerrar el intento.",
                    "Completa un nuevo intento correcto sin revelar la solución durante el intento."
                )
            } else if esAciertoInseguro {
                estrategia = (
                    .consolidarConfianza,
                    .media,
                    "Resuelva otra vez sin ayudas y justifique en una frase por qué cada transformación o ecuación usada es válida.",
                    "Vuelve a acertar y puede justificar el procedimiento con confianza de al menos 70 %."
                )
            } else {
                estrategia = (
                    .ganarFluidez,
                    .baja,
                    "Repita el procedimiento identificando el paso que consumió más tiempo y elimine trabajo mecánico innecesario sin saltarse justificaciones.",
                    "Completa un intento correcto e identifica explícitamente dónde estaba el cuello de botella."
                )
            }

            var senales: [String] = []
            if esErrorAltaConfianza {
                senales.append(
                    "Fallo con confianza \(porcentaje(resultado.confianza))"
                )
            } else if esFallo {
                senales.append("Intento incorrecto")
            }
            if esAciertoInseguro {
                senales.append(
                    "Acierto con confianza \(porcentaje(resultado.confianza))"
                )
            }
            if esLento {
                senales.append("Tiempo atípico dentro de esta sesión")
            }
            if !resultado.error.isEmpty {
                senales.append("Error registrado: \(resultado.error)")
            }

            candidatos.append((
                indice,
                AccionRecuperacionSimulacroIPad(
                    frameId: resultado.frameId,
                    titulo: resultado.titulo,
                    tipo: estrategia.0,
                    prioridad: estrategia.1,
                    senales: senales,
                    instruccion: estrategia.2,
                    criterioCierre: estrategia.3)
            ))
        }

        return candidatos.sorted { izquierda, derecha in
            if izquierda.accion.prioridad != derecha.accion.prioridad {
                return izquierda.accion.prioridad < derecha.accion.prioridad
            }
            if izquierda.indice != derecha.indice {
                return izquierda.indice < derecha.indice
            }
            return izquierda.accion.titulo.localizedCaseInsensitiveCompare(
                derecha.accion.titulo) == .orderedAscending
        }
        .prefix(limiteSeguro)
        .map(\.accion)
    }

    private static func porcentaje(_ valor: Double) -> String {
        "\(Int((min(max(valor, 0), 1) * 100).rounded())) %"
    }
}
