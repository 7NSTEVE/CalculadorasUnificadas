import CryptoKit
import Foundation

struct ComponenteRespaldoCuadernUIPad {
    var rutaRelativa: String
    var datos: Data
}

struct EntradaManifiestoRespaldoIPad: Codable, Equatable {
    var rutaRelativa: String
    var bytes: Int64
    var sha256: String
}

struct ManifiestoRespaldoCuadernUIPad: Codable, Equatable {
    var version = 1
    var respaldoId: UUID
    var documentoId: DocumentoPersistenteID
    var creado: Date
    var appVersion: String?
    var componentes: [EntradaManifiestoRespaldoIPad]
}

struct ResultadoValidacionRespaldoIPad: Equatable {
    var manifiesto: ManifiestoRespaldoCuadernUIPad
    var componentesValidos: Int
    var bytesValidados: Int64
}

enum ErrorRespaldoVerificableIPad: LocalizedError, Equatable {
    case rutaInvalida(String)
    case componenteDuplicado(String)
    case respaldoYaExiste
    case manifiestoAusente
    case manifiestoInvalido
    case versionNoCompatible(Int)
    case documentoDistinto
    case componenteAusente(String)
    case tamanoNoCoincide(String)
    case hashNoCoincide(String)
    case destinoNoVacio

    var errorDescription: String? {
        switch self {
        case .rutaInvalida(let ruta): return "Ruta inválida dentro del respaldo: \(ruta)"
        case .componenteDuplicado(let ruta): return "El respaldo contiene dos componentes con la misma ruta: \(ruta)"
        case .respaldoYaExiste: return "Ya existe un respaldo en ese destino."
        case .manifiestoAusente: return "El respaldo no contiene manifiesto."
        case .manifiestoInvalido: return "El manifiesto del respaldo está dañado."
        case .versionNoCompatible(let version): return "La versión \(version) del respaldo no es compatible."
        case .documentoDistinto: return "El respaldo pertenece a otro documento."
        case .componenteAusente(let ruta): return "Falta el componente \(ruta)."
        case .tamanoNoCoincide(let ruta): return "El tamaño de \(ruta) no coincide con el manifiesto."
        case .hashNoCoincide(let ruta): return "La integridad SHA-256 de \(ruta) no coincide."
        case .destinoNoVacio: return "La restauración sólo puede prepararse en un directorio staging vacío."
        }
    }
}

enum RespaldoVerificableIPad {
    static let nombreManifiesto = "manifest.json"

    @discardableResult
    static func crear(
        documentoId: DocumentoPersistenteID,
        componentes: [ComponenteRespaldoCuadernUIPad],
        destino: URL,
        appVersion: String? = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String,
        archivos: FileManager = .default
    ) throws -> ManifiestoRespaldoCuadernUIPad {
        guard !archivos.fileExists(atPath: destino.path) else {
            throw ErrorRespaldoVerificableIPad.respaldoYaExiste
        }
        var rutas = Set<String>()
        let normalizados = try componentes.map { componente -> ComponenteRespaldoCuadernUIPad in
            let ruta = try rutaSegura(componente.rutaRelativa)
            guard rutas.insert(ruta).inserted else {
                throw ErrorRespaldoVerificableIPad.componenteDuplicado(ruta)
            }
            return .init(rutaRelativa: ruta, datos: componente.datos)
        }
        let entradas = normalizados.map {
            EntradaManifiestoRespaldoIPad(
                rutaRelativa: $0.rutaRelativa,
                bytes: Int64($0.datos.count),
                sha256: sha256($0.datos))
        }.sorted { $0.rutaRelativa < $1.rutaRelativa }
        let manifiesto = ManifiestoRespaldoCuadernUIPad(
            respaldoId: UUID(), documentoId: documentoId, creado: Date(),
            appVersion: appVersion, componentes: entradas)

        let padre = destino.deletingLastPathComponent()
        try archivos.createDirectory(at: padre, withIntermediateDirectories: true)
        let temporal = padre.appendingPathComponent(".backup-\(UUID().uuidString)", isDirectory: true)
        do {
            try archivos.createDirectory(at: temporal, withIntermediateDirectories: true)
            for componente in normalizados {
                let url = temporal.appendingPathComponent(componente.rutaRelativa)
                try archivos.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                try componente.datos.write(to: url, options: [.atomic, .completeFileProtection])
            }
            let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            try encoder.encode(manifiesto).write(
                to: temporal.appendingPathComponent(nombreManifiesto),
                options: [.atomic, .completeFileProtection])
            _ = try validar(temporal, documentoEsperado: documentoId, archivos: archivos)
            try archivos.moveItem(at: temporal, to: destino)
            return manifiesto
        } catch {
            try? archivos.removeItem(at: temporal)
            throw error
        }
    }

    static func validar(
        _ respaldo: URL,
        documentoEsperado: DocumentoPersistenteID? = nil,
        archivos: FileManager = .default
    ) throws -> ResultadoValidacionRespaldoIPad {
        let manifestURL = respaldo.appendingPathComponent(nombreManifiesto)
        guard archivos.fileExists(atPath: manifestURL.path) else {
            throw ErrorRespaldoVerificableIPad.manifiestoAusente
        }
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        guard let datosManifiesto = try? Data(contentsOf: manifestURL),
              let manifiesto = try? decoder.decode(ManifiestoRespaldoCuadernUIPad.self,
                                                    from: datosManifiesto) else {
            throw ErrorRespaldoVerificableIPad.manifiestoInvalido
        }
        guard manifiesto.version == 1 else {
            throw ErrorRespaldoVerificableIPad.versionNoCompatible(manifiesto.version)
        }
        if let documentoEsperado, manifiesto.documentoId != documentoEsperado {
            throw ErrorRespaldoVerificableIPad.documentoDistinto
        }
        var vistos = Set<String>(), bytes: Int64 = 0
        for entrada in manifiesto.componentes {
            let ruta = try rutaSegura(entrada.rutaRelativa)
            guard vistos.insert(ruta).inserted else {
                throw ErrorRespaldoVerificableIPad.componenteDuplicado(ruta)
            }
            let url = respaldo.appendingPathComponent(ruta)
            guard archivos.fileExists(atPath: url.path) else {
                throw ErrorRespaldoVerificableIPad.componenteAusente(ruta)
            }
            let datos = try Data(contentsOf: url, options: [.mappedIfSafe])
            guard Int64(datos.count) == entrada.bytes else {
                throw ErrorRespaldoVerificableIPad.tamanoNoCoincide(ruta)
            }
            guard sha256(datos) == entrada.sha256 else {
                throw ErrorRespaldoVerificableIPad.hashNoCoincide(ruta)
            }
            bytes += Int64(datos.count)
        }
        return .init(manifiesto: manifiesto,
                     componentesValidos: manifiesto.componentes.count,
                     bytesValidados: bytes)
    }

    /// Copia un respaldo ya validado a staging. El llamador decide después cómo
    /// hacer el commit atómico sobre los repositorios canónicos de cada subsistema.
    @discardableResult
    static func prepararRestauracion(
        _ respaldo: URL,
        documentoEsperado: DocumentoPersistenteID,
        staging: URL,
        archivos: FileManager = .default
    ) throws -> ResultadoValidacionRespaldoIPad {
        let validacion = try validar(respaldo,
                                     documentoEsperado: documentoEsperado,
                                     archivos: archivos)
        if archivos.fileExists(atPath: staging.path) {
            let contenido = try archivos.contentsOfDirectory(atPath: staging.path)
            guard contenido.isEmpty else { throw ErrorRespaldoVerificableIPad.destinoNoVacio }
        } else {
            try archivos.createDirectory(at: staging, withIntermediateDirectories: true)
        }
        do {
            for entrada in validacion.manifiesto.componentes {
                let origen = respaldo.appendingPathComponent(entrada.rutaRelativa)
                let destino = staging.appendingPathComponent(entrada.rutaRelativa)
                try archivos.createDirectory(at: destino.deletingLastPathComponent(),
                                             withIntermediateDirectories: true)
                try archivos.copyItem(at: origen, to: destino)
            }
            _ = try validarStaging(staging, manifiesto: validacion.manifiesto, archivos: archivos)
            return validacion
        } catch {
            try? archivos.removeItem(at: staging)
            throw error
        }
    }

    private static func validarStaging(_ staging: URL,
                                       manifiesto: ManifiestoRespaldoCuadernUIPad,
                                       archivos: FileManager) throws -> Int64 {
        var bytes: Int64 = 0
        for entrada in manifiesto.componentes {
            let datos = try Data(contentsOf: staging.appendingPathComponent(entrada.rutaRelativa),
                                 options: [.mappedIfSafe])
            guard Int64(datos.count) == entrada.bytes else {
                throw ErrorRespaldoVerificableIPad.tamanoNoCoincide(entrada.rutaRelativa)
            }
            guard sha256(datos) == entrada.sha256 else {
                throw ErrorRespaldoVerificableIPad.hashNoCoincide(entrada.rutaRelativa)
            }
            bytes += Int64(datos.count)
        }
        return bytes
    }

    private static func rutaSegura(_ ruta: String) throws -> String {
        let limpia = ruta.replacingOccurrences(of: "\\", with: "/")
            .trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        let partes = limpia.split(separator: "/", omittingEmptySubsequences: true)
        guard !partes.isEmpty,
              partes.allSatisfy({ $0 != "." && $0 != ".." && !$0.contains(":") }),
              !limpia.hasPrefix("~") else {
            throw ErrorRespaldoVerificableIPad.rutaInvalida(ruta)
        }
        return partes.joined(separator: "/")
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
