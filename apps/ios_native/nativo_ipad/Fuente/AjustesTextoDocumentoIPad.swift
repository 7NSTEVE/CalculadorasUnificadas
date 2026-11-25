import Foundation

/// Namespace documental para los ajustes avanzados de las cajas de texto.
///
/// El repositorio histórico usa únicamente `texto.id` como clave. Una copia
/// completa conserva deliberadamente esos IDs dentro del blob `.tinta`, por lo
/// que dos documentos distintos podían compartir padding, Markdown, checkboxes,
/// continuación de listas y matemática inline. Este repositorio añade la
/// identidad durable del documento sin romper el formato legacy.
final class RepositorioAjustesTextoDocumentoIPad {
    static let compartido = RepositorioAjustesTextoDocumentoIPad()

    private struct Archivo: Codable {
        var formato = 1
        var porDocumento: [String: [String: AjustesCajaTextoIPad]] = [:]
    }

    private let cola = DispatchQueue(label: "tinta.texto.avanzado.documento")
    private let url: URL
    private var archivo: Archivo

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Texto", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("ajustes-cajas-documento.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(Archivo.self, from: $0)
        } ?? Archivo()
    }

    private func clave(_ documento: UInt64) -> String { String(documento) }

    private static func limpiarContexto(_ valor: AjustesCajaTextoIPad)
        -> AjustesCajaTextoIPad {
        var limpio = valor
        limpio.documentoContexto = nil
        return limpio
    }

    /// Lee el valor efectivo. Si solo existe la entrada global legacy, la
    /// materializa bajo este documento para que una copia futura pueda divergir
    /// sin modificar el original.
    func ajustes(documento: UInt64, id: String) -> AjustesCajaTextoIPad {
        cola.sync {
            let documentoClave = clave(documento)
            if let valor = archivo.porDocumento[documentoClave]?[id] {
                return Self.limpiarContexto(valor)
            }

            let heredado = Self.limpiarContexto(
                RepositorioAjustesTextoIPad.compartido.ajustesLegacy(id))
            var siguiente = archivo
            var valores = siguiente.porDocumento[documentoClave] ?? [:]
            valores[id] = heredado
            siguiente.porDocumento[documentoClave] = valores
            do {
                try persistir(siguiente)
                archivo = siguiente
            } catch {
                // Una lectura legacy puede continuar con el valor efectivo. Una
                // edición explícita sí devuelve `false` si no pudo persistirse.
            }
            return heredado
        }
    }

    @discardableResult
    func guardar(_ ajustes: AjustesCajaTextoIPad,
                 documento: UInt64,
                 id: String) -> Bool {
        cola.sync {
            let documentoClave = clave(documento)
            var siguiente = archivo
            var valores = siguiente.porDocumento[documentoClave] ?? [:]
            valores[id] = Self.limpiarContexto(ajustes)
            siguiente.porDocumento[documentoClave] = valores
            do {
                try persistir(siguiente)
                archivo = siguiente
                return true
            } catch {
                return false
            }
        }
    }

    /// Duplica una caja de texto dentro del mismo documento con una fotografía
    /// independiente de sus ajustes efectivos.
    @discardableResult
    func duplicarElemento(documento: UInt64,
                          origen: String,
                          destino: String) -> Bool {
        guard origen != destino else { return false }
        let valor = ajustes(documento: documento, id: origen)
        return guardar(valor, documento: documento, id: destino)
    }

    /// Copia el namespace materializado de un documento completo. Los IDs de
    /// texto no cambian porque el núcleo también los conserva al duplicar.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        return try cola.sync {
            let origenClave = clave(origen)
            let destinoClave = clave(destino)
            let existente = archivo.porDocumento[destinoClave] ?? [:]
            guard existente.isEmpty else { throw CocoaError(.fileWriteFileExists) }

            let fuente = archivo.porDocumento[origenClave] ?? [:]
            let copia = fuente.mapValues(Self.limpiarContexto)
            var siguiente = archivo
            siguiente.porDocumento[destinoClave] = copia
            try persistir(siguiente)
            archivo = siguiente
            return copia.count
        }
    }

    func eliminarDocumento(_ documento: UInt64) throws {
        try cola.sync {
            let documentoClave = clave(documento)
            guard archivo.porDocumento[documentoClave] != nil else { return }
            var siguiente = archivo
            siguiente.porDocumento.removeValue(forKey: documentoClave)
            try persistir(siguiente)
            archivo = siguiente
        }
    }

    func instantanea(documento: UInt64) -> [String: AjustesCajaTextoIPad] {
        cola.sync {
            (archivo.porDocumento[clave(documento)] ?? [:])
                .mapValues(Self.limpiarContexto)
        }
    }

    private func persistir(_ valor: Archivo) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
