import Foundation

/// Duplica la *política* de protección de un cuaderno, nunca su material
/// criptográfico ni su historial de recuperación.
///
/// `AlmacenClavesCuaderno` deriva la cuenta del Keychain a partir del
/// `documentoId`; por tanto el destino obtendrá automáticamente una clave
/// aleatoria distinta la primera vez que cifre. Aquí solo viajan nivel de
/// privacidad, previsualizaciones, política de respaldos, compresión y número de
/// generaciones. Los journals/autoguardados permanecen en el documento fuente.
final class DuplicadorPrivacidadCuadernoIPad {
    static let compartido = DuplicadorPrivacidadCuadernoIPad()

    enum Fallo: LocalizedError {
        case mismoDocumento
        case destinoOcupado
        case verificacion

        var errorDescription: String? {
            switch self {
            case .mismoDocumento:
                return "La privacidad de una copia requiere otra identidad documental."
            case .destinoOcupado:
                return "El documento destino ya tiene una política de privacidad explícita."
            case .verificacion:
                return "La política de privacidad de la copia no pudo verificarse en disco."
            }
        }
    }

    private let bloqueo = NSRecursiveLock()
    private let url: URL

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        url = documentos.appendingPathComponent(
            "Seguridad/privacidad-cuadernos.json", isDirectory: false)
    }

    /// Devuelve 1 si existía una configuración explícita y 0 cuando el origen
    /// usa simplemente los valores por defecto del sistema.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw Fallo.mismoDocumento }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let persistidas = try cargarTodas()
        guard let fuente = persistidas[String(origen)] else { return 0 }
        guard persistidas[String(destino)] == nil else { throw Fallo.destinoOcupado }

        var copia = fuente
        copia.documentoId = destino
        copia.actualizado = Date()
        RepositorioPrivacidadCuadernos.compartido.guardar(copia)

        guard let escrita = try cargarTodas()[String(destino)],
              equivalentes(escrita, copia) else {
            try? limpiarDestino(destino)
            throw Fallo.verificacion
        }
        return 1
    }

    /// Restablece el caché público a los valores normales y retira después la
    /// clave explícita del JSON. No toca Keychain ni `Recuperacion/<documento>`:
    /// una copia abortada todavía no debería haber generado ninguno de ambos.
    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let actual = try cargarTodas()
        guard actual[String(documento)] != nil else { return }
        RepositorioPrivacidadCuadernos.compartido.guardar(
            ConfiguracionProteccionCuaderno(documentoId: documento))
        try limpiarDestino(documento)
    }

    func persistida(documento: UInt64) throws -> ConfiguracionProteccionCuaderno? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return try cargarTodas()[String(documento)]
    }

    private func limpiarDestino(_ documento: UInt64) throws {
        var todas = try cargarTodas()
        todas.removeValue(forKey: String(documento))
        try persistir(todas)
        guard try cargarTodas()[String(documento)] == nil else {
            throw Fallo.verificacion
        }
    }

    private func equivalentes(_ a: ConfiguracionProteccionCuaderno,
                              _ b: ConfiguracionProteccionCuaderno) -> Bool {
        a.documentoId == b.documentoId
            && a.nivel == b.nivel
            && a.ocultarPrevisualizaciones == b.ocultarPrevisualizaciones
            && a.cifrarRespaldos == b.cifrarRespaldos
            && a.compresion == b.compresion
            && a.generacionesAutoguardado == b.generacionesAutoguardado
    }

    private func cargarTodas() throws -> [String: ConfiguracionProteccionCuaderno] {
        guard FileManager.default.fileExists(atPath: url.path) else { return [:] }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return try decoder.decode(
            [String: ConfiguracionProteccionCuaderno].self,
            from: Data(contentsOf: url))
    }

    private func persistir(_ valores: [String: ConfiguracionProteccionCuaderno]) throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        let datos = try encoder.encode(valores)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
