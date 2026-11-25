import Foundation

struct EntradaAnaliticaFrameIPad {
    var frameId: UUID
    var titulo: String
    var estudio: EstadoEstudioFrameIPad
}

struct ErrorRecurrenteFrameIPad: Identifiable, Equatable {
    var id: String { clave }
    var clave: String
    var descripcion: String
    var apariciones: Int
    var frameIds: [UUID]
    var titulosFrames: [String]
    var ultimaFecha: Date
    var confianzaPromedio: Double
}

/// Convierte errores que el usuario escribió durante intentos reales en un mapa
/// de debilidades explicable. No inventa categorías ni usa IA: agrupa textos
/// equivalentes ignorando mayúsculas, diacríticos y espacios repetidos.
enum AnalizadorErroresFramesIPad {
    static func recurrentes(
        _ entradas: [EntradaAnaliticaFrameIPad]
    ) -> [ErrorRecurrenteFrameIPad] {
        struct Acumulado {
            var descripcion: String
            var apariciones = 0
            var frames: [UUID: String] = [:]
            var ultimaFecha = Date.distantPast
            var sumaConfianza = 0.0
        }

        var grupos: [String: Acumulado] = [:]
        for entrada in entradas {
            for intento in entrada.estudio.intentosTerminados {
                guard intento.resultado == .repetir,
                      let fin = intento.fin else { continue }
                let descripcion = intento.error.trimmingCharacters(
                    in: .whitespacesAndNewlines)
                let clave = normalizar(descripcion)
                guard !clave.isEmpty else { continue }

                var grupo = grupos[clave] ?? Acumulado(descripcion: descripcion)
                grupo.apariciones += 1
                grupo.frames[entrada.frameId] = entrada.titulo
                grupo.ultimaFecha = max(grupo.ultimaFecha, fin)
                grupo.sumaConfianza += intento.confianza
                grupos[clave] = grupo
            }
        }

        return grupos.map { clave, grupo in
            let pares = grupo.frames.sorted {
                $0.value.localizedCaseInsensitiveCompare($1.value) == .orderedAscending
            }
            return ErrorRecurrenteFrameIPad(
                clave: clave,
                descripcion: grupo.descripcion,
                apariciones: grupo.apariciones,
                frameIds: pares.map(\.key),
                titulosFrames: pares.map(\.value),
                ultimaFecha: grupo.ultimaFecha,
                confianzaPromedio: grupo.sumaConfianza / Double(grupo.apariciones))
        }.sorted {
            if $0.apariciones != $1.apariciones { return $0.apariciones > $1.apariciones }
            if $0.ultimaFecha != $1.ultimaFecha { return $0.ultimaFecha > $1.ultimaFecha }
            return $0.descripcion.localizedCaseInsensitiveCompare($1.descripcion)
                == .orderedAscending
        }
    }

    static func normalizar(_ texto: String) -> String {
        texto
            .folding(options: [.diacriticInsensitive, .caseInsensitive], locale: .current)
            .split(whereSeparator: { $0.isWhitespace })
            .joined(separator: " ")
            .lowercased()
    }
}
