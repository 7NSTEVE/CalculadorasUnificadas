import Foundation

/// Identidad durable de un documento de Tinta.
///
/// El UUID proviene de `RegistroPestanaIPad.id` y sobrevive cierres, reinicios,
/// cambios de orden y restauración de pestañas. Nunca debe confundirse con el
/// handle `TintaDocumentoId` que el núcleo C++ asigna solo durante un proceso.
struct DocumentoPersistenteID: Hashable, Sendable, Codable, CustomStringConvertible {
    let rawValue: UUID

    init(_ rawValue: UUID = UUID()) {
        self.rawValue = rawValue
    }

    var description: String { rawValue.uuidString.lowercased() }

    /// Puente versionado para los repositorios históricos que todavía usan
    /// UInt64. El bit alto separa estas claves de los handles legacy 1, 2, 3…
    /// La fuente de verdad sigue siendo el UUID completo.
    var claveCompatibilidadUInt64: UInt64 {
        var bytes = rawValue.uuid
        var hash: UInt64 = 0xcbf2_9ce4_8422_2325
        withUnsafeBytes(of: &bytes) { buffer in
            for byte in buffer {
                hash ^= UInt64(byte)
                hash &*= 0x0000_0100_0000_01B3
            }
        }
        return hash | (UInt64(1) << 63)
    }

    init(from decoder: Decoder) throws {
        let contenedor = try decoder.singleValueContainer()
        let texto = try contenedor.decode(String.self)
        guard let uuid = UUID(uuidString: texto) else {
            throw DecodingError.dataCorruptedError(
                in: contenedor,
                debugDescription: "Identidad documental UUID inválida")
        }
        rawValue = uuid
    }

    func encode(to encoder: Encoder) throws {
        var contenedor = encoder.singleValueContainer()
        try contenedor.encode(description)
    }
}
