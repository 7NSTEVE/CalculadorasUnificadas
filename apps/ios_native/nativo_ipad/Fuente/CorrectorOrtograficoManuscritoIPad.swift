import Foundation
import UIKit

struct IncidenciaOrtograficaIPad: Equatable, Identifiable {
    var id: String { "\(rango.location):\(rango.length):\(palabra)" }
    var palabra: String
    var rango: NSRange
    var sugerencias: [String]
    var idioma: String
}

private struct ArchivoDiccionarioPersonalIPad: Codable {
    var version = 1
    var palabras: Set<String> = []
}

final class RepositorioDiccionarioPersonalIPad {
    static let compartido = RepositorioDiccionarioPersonalIPad()
    private let url: URL
    private let cola = DispatchQueue(label: "cuadernu.diccionario-personal")
    private var palabras: Set<String>

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("Academico/Ortografia", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("diccionario-personal.json")
        if let datos = try? Data(contentsOf: url),
           let leido = try? JSONDecoder().decode(ArchivoDiccionarioPersonalIPad.self, from: datos) {
            palabras = leido.palabras
        } else { palabras = [] }
    }

    func contiene(_ palabra: String) -> Bool {
        cola.sync { palabras.contains(Self.normalizar(palabra)) }
    }

    func todas() -> [String] {
        cola.sync { palabras.sorted() }
    }

    @discardableResult
    func aprender(_ palabra: String) -> Bool {
        let valor = Self.normalizar(palabra)
        guard !valor.isEmpty else { return false }
        return cola.sync {
            palabras.insert(valor)
            UITextChecker.learnWord(palabra)
            return guardar()
        }
    }

    @discardableResult
    func olvidar(_ palabra: String) -> Bool {
        let valor = Self.normalizar(palabra)
        return cola.sync {
            palabras.remove(valor)
            UITextChecker.unlearnWord(palabra)
            return guardar()
        }
    }

    private func guardar() -> Bool {
        do {
            let encoder = JSONEncoder(); encoder.outputFormatting = [.sortedKeys]
            let datos = try encoder.encode(ArchivoDiccionarioPersonalIPad(palabras: palabras))
            try datos.write(to: url, options: [.atomic, .completeFileProtection])
            return true
        } catch { return false }
    }

    private static func normalizar(_ palabra: String) -> String {
        palabra.folding(options: [.diacriticInsensitive, .caseInsensitive], locale: .current)
            .lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

enum CorrectorOrtograficoManuscritoIPad {
    static func incidencias(
        en texto: String,
        idioma: String = "es_ES",
        diccionario: RepositorioDiccionarioPersonalIPad = .compartido,
        maximo: Int = 100
    ) -> [IncidenciaOrtograficaIPad] {
        guard !texto.isEmpty, maximo > 0 else { return [] }
        let checker = UITextChecker()
        let ns = texto as NSString
        let total = NSRange(location: 0, length: ns.length)
        var inicio = 0
        var salida: [IncidenciaOrtograficaIPad] = []

        while inicio < total.length, salida.count < maximo {
            let rango = checker.rangeOfMisspelledWord(
                in: texto, range: NSRange(location: inicio, length: total.length - inicio),
                startingAt: inicio, wrap: false, language: idioma)
            guard rango.location != NSNotFound, rango.length > 0 else { break }
            let palabra = ns.substring(with: rango)
            if !diccionario.contiene(palabra) {
                let sugerencias = checker.guesses(
                    forWordRange: rango, in: texto, language: idioma) ?? []
                salida.append(.init(
                    palabra: palabra, rango: rango,
                    sugerencias: Array(sugerencias.prefix(8)), idioma: idioma))
            }
            inicio = max(rango.location + rango.length, inicio + 1)
        }
        return salida
    }

    /// Reemplazo determinista sobre la capa de texto reconocido. La capa de Ink
    /// puede usar el mismo rango para redibujar/preservar estilo en un PR propio.
    static func reemplazar(
        en texto: String,
        rangoUTF16: NSRange,
        con correccion: String
    ) -> String? {
        let ns = texto as NSString
        guard rangoUTF16.location >= 0, rangoUTF16.length >= 0,
              NSMaxRange(rangoUTF16) <= ns.length else { return nil }
        return ns.replacingCharacters(in: rangoUTF16, with: correccion)
    }
}
