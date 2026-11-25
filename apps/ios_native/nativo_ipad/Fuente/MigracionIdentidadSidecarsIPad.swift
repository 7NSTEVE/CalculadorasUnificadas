import CryptoKit
import Foundation

struct ResultadoMigracionSidecarIPad: Equatable {
    enum Estado: Equatable {
        case noHabiaLegacy
        case yaMigrado
        case migrado
    }

    var estado: Estado
    var documentoId: DocumentoPersistenteID
    var repositorio: String
    var bytes: Int64
    var sha256: String?
}

enum ErrorMigracionSidecarIPad: LocalizedError, Equatable {
    case claveLegacyNoDerivada
    case destinosIguales
    case conflictoDestinoDurable
    case archivoLegacyCambioDuranteMigracion

    var errorDescription: String? {
        switch self {
        case .claveLegacyNoDerivada:
            return "La clave legacy no coincide con la clave derivada del UUID durable; no se migrará un handle ambiguo."
        case .destinosIguales:
            return "La ruta legacy y la durable son la misma."
        case .conflictoDestinoDurable:
            return "Ya existe un sidecar durable con contenido diferente al legacy."
        case .archivoLegacyCambioDuranteMigracion:
            return "El sidecar legacy cambió mientras se preparaba la migración."
        }
    }
}

/// Migra sidecars file-per-document usando la transacción P0. El llamador debe
/// proporcionar el UUID canónico; jamás se intenta reconstruirlo desde UInt64.
final class MigradorIdentidadSidecarsIPad {
    private let transacciones: CoordinadorPersistenciaTransaccionalIPad
    private let archivos: FileManager

    init(transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
         archivos: FileManager = .default) {
        self.transacciones = transacciones
        self.archivos = archivos
    }

    @discardableResult
    func migrarArchivo(
        documentoId: DocumentoPersistenteID,
        claveLegacy: UInt64,
        repositorio: String,
        origenLegacy: URL,
        destinoDurable: URL
    ) throws -> ResultadoMigracionSidecarIPad {
        guard claveLegacy == documentoId.claveCompatibilidadUInt64 else {
            throw ErrorMigracionSidecarIPad.claveLegacyNoDerivada
        }
        let origen = origenLegacy.standardizedFileURL
        let destino = destinoDurable.standardizedFileURL
        guard origen != destino else { throw ErrorMigracionSidecarIPad.destinosIguales }

        let legacyExiste = archivos.fileExists(atPath: origen.path)
        let durableExiste = archivos.fileExists(atPath: destino.path)

        guard legacyExiste else {
            return .init(
                estado: durableExiste ? .yaMigrado : .noHabiaLegacy,
                documentoId: documentoId,
                repositorio: repositorio,
                bytes: durableExiste ? Int64((try? Data(contentsOf: destino).count) ?? 0) : 0,
                sha256: durableExiste ? (try? Data(contentsOf: destino)).map(Self.sha256) : nil)
        }

        let legacy = try Data(contentsOf: origen, options: [.mappedIfSafe])
        let legacyHash = Self.sha256(legacy)

        if durableExiste {
            let durable = try Data(contentsOf: destino, options: [.mappedIfSafe])
            guard Self.sha256(durable) == legacyHash else {
                throw ErrorMigracionSidecarIPad.conflictoDestinoDurable
            }
            // Mismo contenido: sólo retirar el alias legacy con precondición hash.
            _ = try transacciones.ejecutar(
                documentoId: documentoId,
                cambios: [
                    .init(destino: origen,
                          mutacion: .eliminar,
                          precondicion: .sha256(legacyHash))
                ])
            return .init(estado: .yaMigrado,
                         documentoId: documentoId,
                         repositorio: repositorio,
                         bytes: Int64(legacy.count),
                         sha256: legacyHash)
        }

        // Write UUID + delete legacy forman una sola unidad recuperable.
        _ = try transacciones.ejecutar(
            documentoId: documentoId,
            cambios: [
                .init(destino: destino,
                      mutacion: .escribir(legacy),
                      precondicion: .debeNoExistir),
                .init(destino: origen,
                      mutacion: .eliminar,
                      precondicion: .sha256(legacyHash))
            ])

        return .init(estado: .migrado,
                     documentoId: documentoId,
                     repositorio: repositorio,
                     bytes: Int64(legacy.count),
                     sha256: legacyHash)
    }

    /// Convención recomendada para repositorios file-per-document nuevos.
    static func nombreDurable(documentoId: DocumentoPersistenteID,
                              extensionArchivo: String = "json") -> String {
        let ext = extensionArchivo.trimmingCharacters(in: CharacterSet(charactersIn: "."))
        return ext.isEmpty
            ? "doc-\(documentoId.description)"
            : "doc-\(documentoId.description).\(ext)"
    }

    static func nombreLegacy(clave: UInt64,
                             extensionArchivo: String = "json") -> String {
        let ext = extensionArchivo.trimmingCharacters(in: CharacterSet(charactersIn: "."))
        return ext.isEmpty ? "doc-\(clave)" : "doc-\(clave).\(ext)"
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}

/// Describe un repositorio legacy sin acoplar el migrador a su modelo interno.
/// Los owners pueden declarar sus rutas sin entregar serialización ni estado.
struct DescriptorSidecarFilePerDocumentIPad: Equatable {
    var nombre: String
    var carpeta: URL
    var prefijo = "doc-"
    var extensionArchivo = "json"

    func rutas(documentoId: DocumentoPersistenteID) -> (legacy: URL, durable: URL) {
        let legacyNombre = prefijo + String(documentoId.claveCompatibilidadUInt64) +
            (extensionArchivo.isEmpty ? "" : ".\(extensionArchivo)")
        let durableNombre = prefijo + documentoId.description +
            (extensionArchivo.isEmpty ? "" : ".\(extensionArchivo)")
        return (carpeta.appendingPathComponent(legacyNombre),
                carpeta.appendingPathComponent(durableNombre))
    }
}
