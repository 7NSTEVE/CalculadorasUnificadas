import CryptoKit
import Foundation

enum EstadoMigracionMapaJSONIPad: Equatable {
    case sinClaveLegacy
    case yaMigrado
    case migrado
}

struct ResultadoMigracionMapaJSONIPad: Equatable {
    var estado: EstadoMigracionMapaJSONIPad
    var documentoId: DocumentoPersistenteID
    var archivo: String
    var claveLegacy: String
    var claveDurable: String
}

enum ErrorMigracionMapaJSONIPad: LocalizedError, Equatable {
    case claveLegacyNoDerivada
    case raizNoEsObjeto
    case conflictoValorDurable
    case jsonNoSerializable

    var errorDescription: String? {
        switch self {
        case .claveLegacyNoDerivada:
            return "La clave UInt64 no corresponde al UUID durable conocido."
        case .raizNoEsObjeto:
            return "El JSON global no tiene un objeto/diccionario en la raíz."
        case .conflictoValorDurable:
            return "La clave UUID ya contiene un valor diferente al legacy."
        case .jsonNoSerializable:
            return "El JSON migrado no puede serializarse de forma segura."
        }
    }
}

/// Migra diccionarios JSON globales del patrón `{ "<UInt64>": valor }` al
/// patrón `{ "<uuid>": valor }` sin necesitar conocer el tipo de `valor`.
/// El archivo completo se reemplaza mediante la transacción P0 y SHA precondición.
enum MigradorMapaJSONIdentidadIPad {
    static func migrar(
        documentoId: DocumentoPersistenteID,
        claveLegacy: UInt64,
        archivo: URL,
        transacciones: CoordinadorPersistenciaTransaccionalIPad = .compartido,
        archivos: FileManager = .default
    ) throws -> ResultadoMigracionMapaJSONIPad {
        guard claveLegacy == documentoId.claveCompatibilidadUInt64 else {
            throw ErrorMigracionMapaJSONIPad.claveLegacyNoDerivada
        }
        let ruta = archivo.standardizedFileURL
        guard archivos.fileExists(atPath: ruta.path) else {
            return .init(estado: .sinClaveLegacy,
                         documentoId: documentoId,
                         archivo: ruta.path,
                         claveLegacy: String(claveLegacy),
                         claveDurable: documentoId.description)
        }

        let original = try Data(contentsOf: ruta, options: [.mappedIfSafe])
        let objeto = try JSONSerialization.jsonObject(with: original)
        guard var mapa = objeto as? [String: Any] else {
            throw ErrorMigracionMapaJSONIPad.raizNoEsObjeto
        }

        let legacy = String(claveLegacy)
        let durable = documentoId.description
        guard let valorLegacy = mapa[legacy] else {
            return .init(estado: mapa[durable] == nil ? .sinClaveLegacy : .yaMigrado,
                         documentoId: documentoId,
                         archivo: ruta.path,
                         claveLegacy: legacy,
                         claveDurable: durable)
        }

        if let valorDurable = mapa[durable] {
            guard equivalentesJSON(valorLegacy, valorDurable) else {
                throw ErrorMigracionMapaJSONIPad.conflictoValorDurable
            }
            mapa.removeValue(forKey: legacy)
            let nuevos = try serializar(mapa)
            _ = try transacciones.ejecutar(
                documentoId: documentoId,
                cambios: [.init(
                    destino: ruta,
                    mutacion: .escribir(nuevos),
                    precondicion: .sha256(sha256(original)))])
            return .init(estado: .yaMigrado,
                         documentoId: documentoId,
                         archivo: ruta.path,
                         claveLegacy: legacy,
                         claveDurable: durable)
        }

        mapa[durable] = valorLegacy
        mapa.removeValue(forKey: legacy)
        let nuevos = try serializar(mapa)
        _ = try transacciones.ejecutar(
            documentoId: documentoId,
            cambios: [.init(
                destino: ruta,
                mutacion: .escribir(nuevos),
                precondicion: .sha256(sha256(original)))])
        return .init(estado: .migrado,
                     documentoId: documentoId,
                     archivo: ruta.path,
                     claveLegacy: legacy,
                     claveDurable: durable)
    }

    private static func equivalentesJSON(_ a: Any, _ b: Any) -> Bool {
        guard JSONSerialization.isValidJSONObject(["v": a]),
              JSONSerialization.isValidJSONObject(["v": b]),
              let da = try? JSONSerialization.data(
                withJSONObject: ["v": a], options: [.sortedKeys]),
              let db = try? JSONSerialization.data(
                withJSONObject: ["v": b], options: [.sortedKeys]) else {
            return false
        }
        return da == db
    }

    private static func serializar(_ mapa: [String: Any]) throws -> Data {
        guard JSONSerialization.isValidJSONObject(mapa) else {
            throw ErrorMigracionMapaJSONIPad.jsonNoSerializable
        }
        return try JSONSerialization.data(
            withJSONObject: mapa,
            options: [.prettyPrinted, .sortedKeys])
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
