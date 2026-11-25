import Foundation

enum IdentidadCopiaDocumentoIPad {
    /// Extrae la identidad durable del blob sin cargarlo en la sesión destino.
    /// Los documentos antiguos que no la expongan devuelven `nil`: pueden
    /// seguir importándose como tinta, pero no se inventa un origen para copiar
    /// sidecars que quizá no existan localmente.
    static func origen(de datos: Data) -> DocumentoPersistenteID? {
        var buffer = [CChar](repeating: 0, count: 128)
        let leida = buffer.withUnsafeMutableBufferPointer { salida in
            datos.withUnsafeBytes { crudo in
                guard let base = crudo.bindMemory(to: UInt8.self).baseAddress,
                      let destino = salida.baseAddress else { return false }
                return tinta_documento_identidad_serializada(
                    base, crudo.count, destino, salida.count)
            }
        }
        guard leida else { return nil }
        let texto = buffer.withUnsafeBufferPointer { entrada -> String in
            guard let base = entrada.baseAddress else { return "" }
            return String(cString: base)
        }
        guard let uuid = UUID(uuidString: texto) else { return nil }
        return DocumentoPersistenteID(uuid)
    }

    /// Completa la parte Swift de una copia después de que el núcleo haya
    /// adoptado la identidad nueva. Un archivo importado sin identidad durable
    /// no tiene un agregado local demostrable y por eso no copia sidecars.
    static func copiarAgregadoSiExiste(
        origen: DocumentoPersistenteID?,
        destino: DocumentoPersistenteID
    ) -> Bool {
        guard let origen else { return true }
        guard origen != destino else { return false }
        do {
            try CoordinadorDuplicacionAgregadoIPad.compartido.duplicar(
                desde: origen.claveCompatibilidadUInt64,
                hacia: destino.claveCompatibilidadUInt64)
            return true
        } catch {
            print("Error al duplicar el agregado documental: \(error.localizedDescription)")
            return false
        }
    }
}
