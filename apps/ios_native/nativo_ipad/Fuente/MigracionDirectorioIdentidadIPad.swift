import CryptoKit
import Foundation

struct ResultadoMigracionDirectorioIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var archivosMigrados: Int
    var bytesMigrados: Int64
    var aliasIdenticosEliminados: Int
    var origenEliminado: Bool
}

enum ErrorMigracionDirectorioIPad: LocalizedError, Equatable {
    case claveLegacyNoDerivada
    case origenNoExiste
    case origenNoEsDirectorio
    case destinoEsArchivo
    case demasiadosArchivos(Int)
    case demasiadosBytes(Int64)
    case conflictoDestino(String)
    case rutaRelativaInvalida(String)

    var errorDescription: String? {
        switch self {
        case .claveLegacyNoDerivada:
            return "La clave legacy no corresponde al UUID durable conocido."
        case .origenNoExiste:
            return "No existe el directorio legacy que se intentó migrar."
        case .origenNoEsDirectorio:
            return "La ruta legacy no es un directorio."
        case .destinoEsArchivo:
            return "La ruta durable existe pero no es un directorio."
        case .demasiadosArchivos(let cantidad):
            return "El directorio contiene \(cantidad) archivos y excede el límite preventivo."
        case .demasiadosBytes(let bytes):
            return "El directorio requiere migrar \(bytes) bytes y excede el límite preventivo."
        case .conflictoDestino(let ruta):
            return "El destino durable ya contiene contenido diferente: \(ruta)"
        case .rutaRelativaInvalida(let ruta):
            return "La ruta relativa no es segura para migración: \(ruta)"
        }
    }
}

/// Migra repositorios directory-per-document (`<UInt64>/...`) a
/// `<uuid>/...`. Cada archivo nuevo y cada eliminación legacy forman una sola
/// transacción P0. Los directorios vacíos se retiran sólo después del commit.
enum MigradorDirectorioIdentidadIPad {
    static func migrar(
        documentoId: DocumentoPersistenteID,
        claveLegacy: UInt64,
        origenLegacy: URL,
        destinoDurable: URL,
        maximoArchivos: Int = 10_000,
        maximoBytes: Int64 = 1_024 * 1_024 * 1_024,
        transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
        archivos: FileManager = .default
    ) throws -> ResultadoMigracionDirectorioIPad {
        guard claveLegacy == documentoId.claveCompatibilidadUInt64 else {
            throw ErrorMigracionDirectorioIPad.claveLegacyNoDerivada
        }
        let origen = origenLegacy.standardizedFileURL
        let destino = destinoDurable.standardizedFileURL
        var esDir: ObjCBool = false
        guard archivos.fileExists(atPath: origen.path, isDirectory: &esDir) else {
            throw ErrorMigracionDirectorioIPad.origenNoExiste
        }
        guard esDir.boolValue else { throw ErrorMigracionDirectorioIPad.origenNoEsDirectorio }

        if archivos.fileExists(atPath: destino.path, isDirectory: &esDir), !esDir.boolValue {
            throw ErrorMigracionDirectorioIPad.destinoEsArchivo
        }

        let keys: Set<URLResourceKey> = [.isRegularFileKey, .fileSizeKey]
        guard let enumerador = archivos.enumerator(
            at: origen,
            includingPropertiesForKeys: Array(keys),
            options: [.skipsHiddenFiles]) else {
            throw ErrorMigracionDirectorioIPad.origenNoExiste
        }

        var entradas: [(origen: URL, destino: URL, datos: Data, hash: String)] = []
        var totalBytes: Int64 = 0
        for case let url as URL in enumerador {
            let valores = try url.resourceValues(forKeys: keys)
            guard valores.isRegularFile == true else { continue }
            if entradas.count + 1 > max(maximoArchivos, 1) {
                throw ErrorMigracionDirectorioIPad.demasiadosArchivos(entradas.count + 1)
            }
            let relativa = String(url.path.dropFirst(origen.path.count))
                .trimmingCharacters(in: CharacterSet(charactersIn: "/"))
            guard !relativa.isEmpty,
                  !relativa.split(separator: "/").contains(where: { $0 == ".." }) else {
                throw ErrorMigracionDirectorioIPad.rutaRelativaInvalida(relativa)
            }
            let datos = try Data(contentsOf: url, options: [.mappedIfSafe])
            totalBytes += Int64(datos.count)
            if totalBytes > max(maximoBytes, 1) {
                throw ErrorMigracionDirectorioIPad.demasiadosBytes(totalBytes)
            }
            entradas.append((url,
                             destino.appendingPathComponent(relativa),
                             datos,
                             sha256(datos)))
        }

        var cambios: [CambioArchivoTransaccionalIPad] = []
        var aliases = 0
        for entrada in entradas {
            if archivos.fileExists(atPath: entrada.destino.path) {
                let actual = try Data(contentsOf: entrada.destino, options: [.mappedIfSafe])
                guard sha256(actual) == entrada.hash else {
                    throw ErrorMigracionDirectorioIPad.conflictoDestino(
                        entrada.destino.path)
                }
                aliases += 1
            } else {
                cambios.append(.init(
                    destino: entrada.destino,
                    mutacion: .escribir(entrada.datos),
                    precondicion: .debeNoExistir))
            }
            cambios.append(.init(
                destino: entrada.origen,
                mutacion: .eliminar,
                precondicion: .sha256(entrada.hash)))
        }

        if !cambios.isEmpty {
            _ = try transacciones.ejecutar(
                documentoId: documentoId,
                cambios: cambios)
        }

        // El commit ya hizo durable la migración. Borrar carpetas vacías no
        // cambia contenido y puede reintentarse en un próximo mantenimiento.
        removerDirectoriosVacios(origen, archivos: archivos)
        let origenSigue = archivos.fileExists(atPath: origen.path)
        return .init(
            documentoId: documentoId,
            archivosMigrados: entradas.count,
            bytesMigrados: totalBytes,
            aliasIdenticosEliminados: aliases,
            origenEliminado: !origenSigue)
    }

    private static func removerDirectoriosVacios(_ raiz: URL,
                                                  archivos: FileManager) {
        let subdirectorios = ((try? archivos.subpathsOfDirectory(atPath: raiz.path)) ?? [])
            .map { raiz.appendingPathComponent($0) }
            .sorted { $0.path.count > $1.path.count }
        for url in subdirectorios + [raiz] {
            var esDir: ObjCBool = false
            guard archivos.fileExists(atPath: url.path, isDirectory: &esDir),
                  esDir.boolValue,
                  ((try? archivos.contentsOfDirectory(atPath: url.path).isEmpty) == true) else { continue }
            try? archivos.removeItem(at: url)
        }
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
