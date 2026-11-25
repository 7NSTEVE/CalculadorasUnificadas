import CryptoKit
import Foundation

struct ResultadoMigracionPrefijosJSONIPad: Equatable {
    var documentoId: DocumentoPersistenteID
    var archivo: String
    var camposProcesados: [String]
    var clavesMovidas: Int
    var aliasesIdenticosEliminados: Int
}

enum ErrorMigracionPrefijosJSONIPad: LocalizedError, Equatable {
    case claveLegacyNoDerivada
    case raizNoEsObjeto
    case campoNoEsObjeto(String)
    case conflictoDestino(String)
    case jsonNoSerializable

    var errorDescription: String? {
        switch self {
        case .claveLegacyNoDerivada: return "La clave legacy no corresponde al UUID durable conocido."
        case .raizNoEsObjeto: return "La raíz JSON no es un objeto."
        case .campoNoEsObjeto(let campo): return "El campo JSON «\(campo)» no es un diccionario."
        case .conflictoDestino(let clave): return "La clave durable ya existe con otro valor: \(clave)"
        case .jsonNoSerializable: return "El JSON migrado no se puede serializar."
        }
    }
}

/// Migra claves compuestas como `12345:elemento` → `uuid:elemento` dentro de
/// diccionarios anidados concretos. Útil para sidecars globales que mezclan
/// varios documentos en un mismo archivo sin conocer el tipo de sus valores.
enum MigradorPrefijosJSONIdentidadIPad {
    static func migrar(
        documentoId: DocumentoPersistenteID,
        claveLegacy: UInt64,
        archivo: URL,
        campos: [String],
        separador: String = ":",
        transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
        archivos: FileManager = .default
    ) throws -> ResultadoMigracionPrefijosJSONIPad {
        guard claveLegacy == documentoId.claveCompatibilidadUInt64 else {
            throw ErrorMigracionPrefijosJSONIPad.claveLegacyNoDerivada
        }
        let ruta = archivo.standardizedFileURL
        guard archivos.fileExists(atPath: ruta.path) else {
            return .init(documentoId: documentoId, archivo: ruta.path,
                         camposProcesados: campos, clavesMovidas: 0,
                         aliasesIdenticosEliminados: 0)
        }
        let original = try Data(contentsOf: ruta, options: [.mappedIfSafe])
        let objeto = try JSONSerialization.jsonObject(with: original)
        guard var raiz = objeto as? [String: Any] else {
            throw ErrorMigracionPrefijosJSONIPad.raizNoEsObjeto
        }

        let prefijoLegacy = String(claveLegacy) + separador
        let prefijoDurable = documentoId.description + separador
        var movidas = 0, aliases = 0

        for campo in campos {
            guard let existente = raiz[campo] else { continue }
            guard var mapa = existente as? [String: Any] else {
                throw ErrorMigracionPrefijosJSONIPad.campoNoEsObjeto(campo)
            }
            let clavesLegacy = mapa.keys.filter { $0.hasPrefix(prefijoLegacy) }.sorted()
            for claveVieja in clavesLegacy {
                let sufijo = String(claveVieja.dropFirst(prefijoLegacy.count))
                let claveNueva = prefijoDurable + sufijo
                guard let valorViejo = mapa[claveVieja] else { continue }
                if let valorNuevo = mapa[claveNueva] {
                    guard equivalentesJSON(valorViejo, valorNuevo) else {
                        throw ErrorMigracionPrefijosJSONIPad.conflictoDestino(
                            "\(campo).\(claveNueva)")
                    }
                    mapa.removeValue(forKey: claveVieja)
                    aliases += 1
                } else {
                    mapa[claveNueva] = valorViejo
                    mapa.removeValue(forKey: claveVieja)
                    movidas += 1
                }
            }
            raiz[campo] = mapa
        }

        guard movidas + aliases > 0 else {
            return .init(documentoId: documentoId, archivo: ruta.path,
                         camposProcesados: campos, clavesMovidas: 0,
                         aliasesIdenticosEliminados: 0)
        }
        guard JSONSerialization.isValidJSONObject(raiz) else {
            throw ErrorMigracionPrefijosJSONIPad.jsonNoSerializable
        }
        let nuevos = try JSONSerialization.data(
            withJSONObject: raiz, options: [.prettyPrinted, .sortedKeys])
        _ = try transacciones.ejecutar(
            documentoId: documentoId,
            cambios: [.init(destino: ruta,
                            mutacion: .escribir(nuevos),
                            precondicion: .sha256(sha256(original)))])
        return .init(documentoId: documentoId, archivo: ruta.path,
                     camposProcesados: campos, clavesMovidas: movidas,
                     aliasesIdenticosEliminados: aliases)
    }

    private static func equivalentesJSON(_ a: Any, _ b: Any) -> Bool {
        guard JSONSerialization.isValidJSONObject(["v": a]),
              JSONSerialization.isValidJSONObject(["v": b]),
              let da = try? JSONSerialization.data(withJSONObject: ["v": a], options: [.sortedKeys]),
              let db = try? JSONSerialization.data(withJSONObject: ["v": b], options: [.sortedKeys]) else {
            return false
        }
        return da == db
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
