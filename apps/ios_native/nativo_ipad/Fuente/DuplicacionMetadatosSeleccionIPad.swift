import Foundation

/// Copia transaccional del sidecar privado que usa
/// `RepositorioMetadatosSeleccionIPad` para grupos, bloqueos, etiquetas y
/// descripciones accesibles.
///
/// Se mantiene fuera de `SeleccionAvanzadaIPad.swift` para que la operación de
/// duplicación no dependa de mutadores legacy que actualmente silencian errores
/// de disco. Las estructuras reflejan únicamente el formato persistido.
final class DuplicadorMetadatosSeleccionIPad {
    static let compartido = DuplicadorMetadatosSeleccionIPad()

    private struct Metadato: Codable, Equatable {
        var grupo: String?
        var bloqueado = false
        var etiquetas: [String] = []
        var descripcionAccesible: String?
    }

    private struct Archivo: Codable {
        var formato = 1
        var elementos: [String: Metadato] = [:]
    }

    private let bloqueo = NSRecursiveLock()
    private let base: URL

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        base = soporte.appendingPathComponent("Tinta/MetadatosSeleccion",
                                              isDirectory: true)
        try? FileManager.default.createDirectory(at: base,
                                                 withIntermediateDirectories: true)
    }

    private func url(_ documento: UInt64) -> URL {
        base.appendingPathComponent("\(documento).json")
    }

    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let destinoURL = url(destino)
        if FileManager.default.fileExists(atPath: destinoURL.path) {
            let actual = try cargar(destinoURL)
            guard actual.elementos.isEmpty else { throw CocoaError(.fileWriteFileExists) }
        }

        let origenURL = url(origen)
        guard FileManager.default.fileExists(atPath: origenURL.path) else { return 0 }
        var fuente = try cargar(origenURL)
        guard !fuente.elementos.isEmpty else { return 0 }

        // Los grupos son identidades internas del documento. Las claves de
        // trazo/texto/medio se conservan porque el núcleo también las conserva;
        // los UUID de agrupación se regeneran para que nunca crucen documentos.
        let grupos = Set(fuente.elementos.values.compactMap(\.grupo))
        let remapeo = Dictionary(uniqueKeysWithValues: grupos.map {
            ($0, UUID().uuidString)
        })
        for clave in Array(fuente.elementos.keys) {
            guard let grupo = fuente.elementos[clave]?.grupo else { continue }
            fuente.elementos[clave]?.grupo = remapeo[grupo]
        }

        try persistir(fuente, en: destinoURL)
        return fuente.elementos.count
    }

    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let destino = url(documento)
        guard FileManager.default.fileExists(atPath: destino.path) else { return }
        try FileManager.default.removeItem(at: destino)
    }

    /// Inspección sin mutar usada por regresiones de duplicación.
    func instantanea(documento: UInt64) throws
        -> [String: (grupo: String?, bloqueado: Bool,
                     etiquetas: [String], descripcion: String?)] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let destino = url(documento)
        guard FileManager.default.fileExists(atPath: destino.path) else { return [:] }
        let archivo = try cargar(destino)
        return archivo.elementos.mapValues {
            (grupo: $0.grupo, bloqueado: $0.bloqueado,
             etiquetas: $0.etiquetas, descripcion: $0.descripcionAccesible)
        }
    }

    private func cargar(_ url: URL) throws -> Archivo {
        let datos = try Data(contentsOf: url)
        return try JSONDecoder().decode(Archivo.self, from: datos)
    }

    private func persistir(_ archivo: Archivo, en url: URL) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(archivo)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
