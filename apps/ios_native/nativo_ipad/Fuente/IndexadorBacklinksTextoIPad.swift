import Foundation

/// Reconstruye el grafo saliente de un documento desde su snapshot textual.
/// No se invoca por pulsación: la capa de edición lo llama al confirmar cambios
/// estructurales o finalizar edición, evitando I/O dentro del camino caliente de
/// teclado/Pencil.
enum IndexadorBacklinksTextoIPad {
    @discardableResult
    static func reconstruir(
        sesion: Sesion,
        repositorio: RepositorioBacklinksIPad = .compartido
    ) -> Bool {
        let origen = sesion.idPersistente
        let altoPagina = max(CGFloat(sesion.altoPagina), 1)
        var backlinks: [BacklinkCuadernUIPad] = []

        for elemento in sesion.textos {
            let pagina = Int(max(0, floor(elemento.marco.midY / altoPagina)))
            let enlaces = DetectorEnlacesAutomaticosIPad.detectar(en: elemento.texto)
            for enlace in enlaces {
                // Los vínculos web y el contrato UInt64 legacy siguen siendo
                // interactivos, pero sólo el codec UUID v1 tiene identidad
                // suficiente para formar aristas durables del grafo nuevo.
                guard let destino = try? CodecEnlacesInternosIPad.decodificar(enlace.url)
                else { continue }
                backlinks.append(BacklinkCuadernUIPad(
                    destino: destino,
                    documentoOrigen: origen,
                    paginaOrigen: pagina,
                    frameOrigen: nil,
                    etiqueta: etiqueta(elemento.texto, enlace: enlace),
                    contexto: contexto(elemento.texto, enlace: enlace)))
            }
        }

        return repositorio.reemplazarSalientes(de: origen, con: backlinks)
    }

    private static func etiqueta(_ texto: String,
                                 enlace: EnlaceAutomaticoIPad) -> String {
        let limpio = texto
            .replacingOccurrences(of: "\n", with: " ")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        if !limpio.isEmpty {
            return String(limpio.prefix(96))
        }
        return enlace.texto
    }

    private static func contexto(_ texto: String,
                                 enlace: EnlaceAutomaticoIPad) -> String {
        let ns = texto as NSString
        let inicio = max(0, enlace.rango.localizacionUTF16 - 72)
        let finEnlace = enlace.rango.localizacionUTF16 + enlace.rango.longitudUTF16
        let fin = min(ns.length, finEnlace + 72)
        guard fin >= inicio else { return "" }
        return ns.substring(with: NSRange(location: inicio, length: fin - inicio))
            .replacingOccurrences(of: "\n", with: " ")
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
