import Foundation

enum EstadoImportacionOneNote: String, Codable, Sendable {
    case preparada
    case inspeccionada
    case convertida
    case requiereCompatibilidad
    case fallida
}

struct RegistroImportacionOneNote: Codable, Identifiable, Sendable, Equatable {
    var id: UUID
    var huella: HuellaFuenteOneNote
    var formato: FormatoImportacionOneNote
    var estado: EstadoImportacionOneNote
    var creadaEn: Date
    var actualizadaEn: Date
    var cuadernosCuadernU: [String]
    var advertencias: [AdvertenciaImportacionOneNote]
    var ultimoError: String?

    init(
        id: UUID = UUID(),
        huella: HuellaFuenteOneNote,
        formato: FormatoImportacionOneNote,
        estado: EstadoImportacionOneNote,
        creadaEn: Date = Date(),
        actualizadaEn: Date = Date(),
        cuadernosCuadernU: [String] = [],
        advertencias: [AdvertenciaImportacionOneNote] = [],
        ultimoError: String? = nil
    ) {
        self.id = id
        self.huella = huella
        self.formato = formato
        self.estado = estado
        self.creadaEn = creadaEn
        self.actualizadaEn = actualizadaEn
        self.cuadernosCuadernU = cuadernosCuadernU
        self.advertencias = advertencias
        self.ultimoError = ultimoError
    }
}

private struct ArchivoRegistroImportacionesOneNote: Codable, Sendable {
    var version: Int = 1
    var registros: [RegistroImportacionOneNote]
}

enum ErrorRegistroImportacionesOneNote: Error, LocalizedError {
    case versionNoSoportada(Int)
    case sha256Vacio

    var errorDescription: String? {
        switch self {
        case .versionNoSoportada(let version):
            return "Versión de registro de importación OneNote no soportada: \(version)."
        case .sha256Vacio:
            return "No se puede registrar una fuente OneNote sin SHA-256."
        }
    }
}

/// Registro durable que hace que los reintentos sean idempotentes por SHA-256.
/// Todas las fechas usan ISO-8601 tanto al codificar como al decodificar.
actor RepositorioRegistroImportacionesOneNote {
    static let compartido = RepositorioRegistroImportacionesOneNote()

    private let url: URL
    private var porSHA256: [String: RegistroImportacionOneNote]

    init(url: URL? = nil) {
        let destino: URL
        if let url {
            destino = url
        } else {
            let base = (try? FileManager.default.url(
                for: .applicationSupportDirectory,
                in: .userDomainMask,
                appropriateFor: nil,
                create: true
            )) ?? FileManager.default.temporaryDirectory
            destino = base
                .appendingPathComponent("CuadernU", isDirectory: true)
                .appendingPathComponent("Importaciones", isDirectory: true)
                .appendingPathComponent("onenote-registro-v1.json", isDirectory: false)
        }
        self.url = destino
        self.porSHA256 = Self.cargar(url: destino)
    }

    func registro(sha256: String) -> RegistroImportacionOneNote? {
        porSHA256[sha256.lowercased()]
    }

    func todos() -> [RegistroImportacionOneNote] {
        porSHA256.values.sorted { $0.actualizadaEn > $1.actualizadaEn }
    }

    @discardableResult
    func guardar(_ registro: RegistroImportacionOneNote) throws -> RegistroImportacionOneNote {
        let clave = registro.huella.sha256.lowercased()
        guard !clave.isEmpty else { throw ErrorRegistroImportacionesOneNote.sha256Vacio }

        var candidato = porSHA256
        var actualizado = registro
        if let anterior = candidato[clave] {
            // La identidad del primer intento se conserva para que observabilidad y UX no
            // conviertan cada reintento en una importación distinta.
            actualizado.id = anterior.id
            actualizado.creadaEn = anterior.creadaEn
        }
        actualizado.actualizadaEn = Date()
        candidato[clave] = actualizado

        try persistir(candidato)
        porSHA256 = candidato
        return actualizado
    }

    @discardableResult
    func marcar(
        sha256: String,
        estado: EstadoImportacionOneNote,
        cuadernosCuadernU: [String]? = nil,
        advertencias: [AdvertenciaImportacionOneNote]? = nil,
        error: String? = nil
    ) throws -> RegistroImportacionOneNote? {
        let clave = sha256.lowercased()
        guard var registro = porSHA256[clave] else { return nil }
        registro.estado = estado
        if let cuadernosCuadernU { registro.cuadernosCuadernU = cuadernosCuadernU }
        if let advertencias { registro.advertencias = advertencias }
        registro.ultimoError = error
        return try guardar(registro)
    }

    func eliminar(sha256: String) throws {
        let clave = sha256.lowercased()
        guard porSHA256[clave] != nil else { return }
        var candidato = porSHA256
        candidato.removeValue(forKey: clave)
        try persistir(candidato)
        porSHA256 = candidato
    }

    private func persistir(_ estado: [String: RegistroImportacionOneNote]) throws {
        let carpeta = url.deletingLastPathComponent()
        try FileManager.default.createDirectory(at: carpeta, withIntermediateDirectories: true)

        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        let archivo = ArchivoRegistroImportacionesOneNote(
            registros: estado.values.sorted { $0.huella.sha256 < $1.huella.sha256 }
        )
        let datos = try encoder.encode(archivo)
        try datos.write(to: url, options: [.atomic])
    }

    private static func cargar(url: URL) -> [String: RegistroImportacionOneNote] {
        guard let datos = try? Data(contentsOf: url), !datos.isEmpty else { return [:] }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        guard let archivo = try? decoder.decode(ArchivoRegistroImportacionesOneNote.self, from: datos),
              archivo.version == 1 else {
            return [:]
        }
        return Dictionary(uniqueKeysWithValues: archivo.registros.map {
            ($0.huella.sha256.lowercased(), $0)
        })
    }
}
