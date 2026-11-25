import Foundation
import CryptoKit

// MARK: - Dominio de importación OneNote

enum FormatoImportacionOneNote: String, Codable, CaseIterable, Sendable {
    case one
    case onepkg
    case onetoc2
    case desconocido

    init(url: URL) {
        switch url.pathExtension.lowercased() {
        case "one": self = .one
        case "onepkg": self = .onepkg
        case "onetoc2": self = .onetoc2
        default: self = .desconocido
        }
    }
}

enum NivelSoporteImportacionOneNote: String, Codable, Sendable {
    case revisionStoreDirecto
    case paqueteReconocido
    case tablaContenidosReconocida
    case noSoportado
}

extension FormatoImportacionOneNote {
    var nivelSoporte: NivelSoporteImportacionOneNote {
        switch self {
        case .one: return .revisionStoreDirecto
        case .onepkg: return .paqueteReconocido
        case .onetoc2: return .tablaContenidosReconocida
        case .desconocido: return .noSoportado
        }
    }
}

enum SeveridadAdvertenciaImportacionOneNote: String, Codable, Comparable, Sendable {
    case informacion
    case advertencia
    case critica

    private var orden: Int {
        switch self {
        case .informacion: return 0
        case .advertencia: return 1
        case .critica: return 2
        }
    }

    static func < (lhs: Self, rhs: Self) -> Bool { lhs.orden < rhs.orden }
}

struct AdvertenciaImportacionOneNote: Codable, Hashable, Sendable {
    var codigo: String
    var mensaje: String
    var severidad: SeveridadAdvertenciaImportacionOneNote
    var rutaLogica: String?
}

struct HuellaFuenteOneNote: Codable, Hashable, Sendable {
    var sha256: String
    var tamanoBytes: UInt64
    var fechaModificacion: Date?
    var nombreOriginal: String
    var extensionOriginal: String
}

struct InformeFidelidadImportacionOneNote: Codable, Hashable, Sendable {
    var elementosReconocidos: Int = 0
    var elementosConvertidos: Int = 0
    var elementosPreservadosSinConvertir: Int = 0
    var elementosNoSoportados: Int = 0
    var advertencias: [AdvertenciaImportacionOneNote] = []

    var requiereRevision: Bool {
        elementosPreservadosSinConvertir > 0 ||
        elementosNoSoportados > 0 ||
        advertencias.contains { $0.severidad >= .advertencia }
    }

    var conversionCompletaConocida: Bool {
        elementosNoSoportados == 0 &&
        elementosPreservadosSinConvertir == 0 &&
        !advertencias.contains { $0.severidad == .critica }
    }
}

// MARK: - Modelo intermedio lossless

struct CuadernoIntermedioOneNote: Codable, Sendable {
    var id: UUID
    var titulo: String
    var grupos: [GrupoSeccionesIntermedioOneNote]
    var secciones: [SeccionIntermediaOneNote]
    var recursos: [RecursoIntermedioOneNote]
    var metadatosDesconocidos: [String: String]
    var fuente: HuellaFuenteOneNote
    var fidelidad: InformeFidelidadImportacionOneNote
}

struct GrupoSeccionesIntermedioOneNote: Codable, Sendable, Identifiable {
    var id: UUID
    var titulo: String
    var grupos: [GrupoSeccionesIntermedioOneNote]
    var secciones: [SeccionIntermediaOneNote]
    var metadatosDesconocidos: [String: String]
}

struct SeccionIntermediaOneNote: Codable, Sendable, Identifiable {
    var id: UUID
    var titulo: String
    var colorARGB: UInt32?
    var paginas: [PaginaIntermediaOneNote]
    var metadatosDesconocidos: [String: String]
}

struct PaginaIntermediaOneNote: Codable, Sendable, Identifiable {
    var id: UUID
    var titulo: String
    var nivelSubpagina: Int
    var creadaEn: Date?
    var modificadaEn: Date?
    var bloques: [BloqueIntermedioOneNote]
    var metadatosDesconocidos: [String: String]
}

enum TipoBloqueIntermedioOneNote: String, Codable, Sendable {
    case texto
    case html
    case imagen
    case tinta
    case adjunto
    case tabla
    case ecuacion
    case audio
    case video
    case enlace
    case desconocido
}

struct BloqueIntermedioOneNote: Codable, Sendable, Identifiable {
    var id: UUID
    var tipo: TipoBloqueIntermedioOneNote
    var texto: String?
    var html: String?
    var recursoId: UUID?
    var posicionX: Double?
    var posicionY: Double?
    var ancho: Double?
    var alto: Double?
    var ordenZ: Int?
    var fragmentoCrudoRelativo: String?
    var metadatosDesconocidos: [String: String]
}

struct RecursoIntermedioOneNote: Codable, Sendable, Identifiable {
    var id: UUID
    var nombreOriginal: String?
    var mimeType: String?
    var extensionArchivo: String?
    var tamanoBytes: UInt64?
    var sha256: String?
    var payloadRelativo: String
    var metadatosDesconocidos: [String: String]
}

// MARK: - Identidad determinista

enum IdentidadImportacionOneNote {
    static func uuidDeterminista(sha256Fuente: String, rutaLogica: String) -> UUID {
        let semilla = Data((sha256Fuente.lowercased() + "\u{0}" + rutaLogica).utf8)
        var bytes = Array(SHA256.hash(data: semilla).prefix(16))
        bytes[6] = (bytes[6] & 0x0F) | 0x50
        bytes[8] = (bytes[8] & 0x3F) | 0x80
        return UUID(uuid: (
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11],
            bytes[12], bytes[13], bytes[14], bytes[15]
        ))
    }
}

// MARK: - Seguridad de rutas/nombres

enum SanitizadorNombresImportacionOneNote {
    static func nombreSeguro(_ nombre: String, fallback: String = "OneNote") -> String {
        let prohibidos = CharacterSet(charactersIn: "/\\:\u{0000}").union(.controlCharacters)
        let piezas = nombre.components(separatedBy: prohibidos)
        var limpio = piezas.joined(separator: "_")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        while limpio.contains("..") {
            limpio = limpio.replacingOccurrences(of: "..", with: "_")
        }
        limpio = limpio.trimmingCharacters(in: CharacterSet(charactersIn: ". "))
        if limpio.isEmpty { limpio = fallback }
        return String(limpio.prefix(180))
    }
}

// MARK: - Staging seguro y streaming hash

struct FuenteOneNotePreparada: Sendable {
    var id: UUID
    var formato: FormatoImportacionOneNote
    var carpetaTrabajo: URL
    var archivoPreparado: URL
    var huella: HuellaFuenteOneNote
}

enum ErrorPreparacionOneNote: Error, LocalizedError, Sendable {
    case formatoNoSoportado
    case archivoNoRegular
    case archivoVacio
    case archivoDemasiadoGrande(UInt64)
    case noSePudoPreparar(String)

    var errorDescription: String? {
        switch self {
        case .formatoNoSoportado:
            return "El archivo no tiene una extensión OneNote reconocida."
        case .archivoNoRegular:
            return "La fuente seleccionada no es un archivo regular."
        case .archivoVacio:
            return "El archivo OneNote está vacío."
        case .archivoDemasiadoGrande(let bytes):
            return "El archivo OneNote excede el límite de seguridad configurado (\(bytes) bytes)."
        case .noSePudoPreparar(let detalle):
            return "No se pudo preparar la importación: \(detalle)"
        }
    }
}

actor AlmacenTemporalImportacionOneNote {
    static let compartido = AlmacenTemporalImportacionOneNote()

    private let maximoBytes: UInt64 = 32 * 1024 * 1024 * 1024
    private let fileManager = FileManager.default

    func preparar(urlOriginal: URL) throws -> FuenteOneNotePreparada {
        let formato = FormatoImportacionOneNote(url: urlOriginal)
        guard formato != .desconocido else { throw ErrorPreparacionOneNote.formatoNoSoportado }

        let accesoSeguro = urlOriginal.startAccessingSecurityScopedResource()
        defer {
            if accesoSeguro { urlOriginal.stopAccessingSecurityScopedResource() }
        }

        let valores = try urlOriginal.resourceValues(forKeys: [
            .isRegularFileKey,
            .fileSizeKey,
            .contentModificationDateKey
        ])
        guard valores.isRegularFile == true else { throw ErrorPreparacionOneNote.archivoNoRegular }

        let tamano = UInt64(max(0, valores.fileSize ?? 0))
        guard tamano > 0 else { throw ErrorPreparacionOneNote.archivoVacio }
        guard tamano <= maximoBytes else { throw ErrorPreparacionOneNote.archivoDemasiadoGrande(tamano) }

        let id = UUID()
        let raiz = fileManager.temporaryDirectory
            .appendingPathComponent("CuadernU-Import-OneNote", isDirectory: true)
            .appendingPathComponent(id.uuidString, isDirectory: true)
        try fileManager.createDirectory(at: raiz, withIntermediateDirectories: true)

        do {
            let nombre = SanitizadorNombresImportacionOneNote.nombreSeguro(urlOriginal.lastPathComponent)
            let destino = raiz.appendingPathComponent(nombre, isDirectory: false)
            try fileManager.copyItem(at: urlOriginal, to: destino)

            let sha = try Self.sha256Streaming(url: destino)
            let huella = HuellaFuenteOneNote(
                sha256: sha,
                tamanoBytes: tamano,
                fechaModificacion: valores.contentModificationDate,
                nombreOriginal: urlOriginal.lastPathComponent,
                extensionOriginal: urlOriginal.pathExtension.lowercased()
            )
            return FuenteOneNotePreparada(
                id: id,
                formato: formato,
                carpetaTrabajo: raiz,
                archivoPreparado: destino,
                huella: huella
            )
        } catch {
            try? fileManager.removeItem(at: raiz)
            throw ErrorPreparacionOneNote.noSePudoPreparar(error.localizedDescription)
        }
    }

    func limpiar(_ fuente: FuenteOneNotePreparada) {
        try? fileManager.removeItem(at: fuente.carpetaTrabajo)
    }

    static func sha256Streaming(url: URL, tamanoBloque: Int = 1_048_576) throws -> String {
        precondition(tamanoBloque > 0)
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }

        var hash = SHA256()
        while true {
            let datos = try handle.read(upToCount: tamanoBloque) ?? Data()
            guard !datos.isEmpty else { break }
            hash.update(data: datos)
        }
        return hash.finalize().map { String(format: "%02x", $0) }.joined()
    }
}
