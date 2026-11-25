import Foundation

/// Copia verificable del esquema eléctrico editable asociado a un documento.
///
/// `RepositorioEsquemas` mantiene una caché global y actualmente silencia
/// errores de escritura. Para una duplicación no basta con ver el esquema en
/// RAM: la etapa solo se confirma después de releer `Ingenieria/esquemas.json`.
final class DuplicadorEsquemaCircuitoIPad {
    static let compartido = DuplicadorEsquemaCircuitoIPad()

    enum Fallo: LocalizedError {
        case mismoDocumento
        case destinoOcupado
        case verificacion

        var errorDescription: String? {
            switch self {
            case .mismoDocumento:
                return "El esquema requiere una identidad destino distinta."
            case .destinoOcupado:
                return "El documento destino ya contiene un esquema eléctrico."
            case .verificacion:
                return "El esquema duplicado no pudo verificarse en disco."
            }
        }
    }

    private let bloqueo = NSRecursiveLock()
    private let url: URL

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        url = documentos.appendingPathComponent(
            "Ingenieria/esquemas.json", isDirectory: false)
    }

    /// Devuelve 1 cuando había esquema canónico que copiar y 0 cuando el
    /// documento fuente nunca persistió uno.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw Fallo.mismoDocumento }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let todos = try cargarTodos()
        guard let fuente = todos[String(origen)] else { return 0 }
        guard todos[String(destino)] == nil else { throw Fallo.destinoOcupado }

        RepositorioEsquemas.compartido.guardar(fuente, documento: destino)
        let verificacion = try cargarTodos()[String(destino)]
        guard verificacion == fuente else {
            try retirarClaveDestino(destino)
            throw Fallo.verificacion
        }
        return 1
    }

    /// Retira la clave canónica del documento. Primero deja la caché legacy en
    /// un estado vacío equivalente a “sin esquema” y luego elimina la entrada
    /// del JSON, de modo que un reintento con el mismo UUID no choque con basura
    /// de una copia abortada.
    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard try cargarTodos()[String(documento)] != nil else { return }
        RepositorioEsquemas.compartido.guardar(
            EsquemaEditable(), documento: documento)
        try retirarClaveDestino(documento)
    }

    func persistido(documento: UInt64) throws -> EsquemaEditable? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return try cargarTodos()[String(documento)]
    }

    private func retirarClaveDestino(_ documento: UInt64) throws {
        var todos = try cargarTodos()
        todos.removeValue(forKey: String(documento))
        try persistir(todos)
        guard try cargarTodos()[String(documento)] == nil else {
            throw Fallo.verificacion
        }
    }

    private func cargarTodos() throws -> [String: EsquemaEditable] {
        guard FileManager.default.fileExists(atPath: url.path) else { return [:] }
        return try JSONDecoder().decode(
            [String: EsquemaEditable].self,
            from: Data(contentsOf: url))
    }

    private func persistir(_ valores: [String: EsquemaEditable]) throws {
        let datos = try JSONEncoder().encode(valores)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}
