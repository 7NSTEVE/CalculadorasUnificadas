import Foundation

/// Duplica los objetos editables que pertenecen al lienzo pero todavía viven
/// fuera del blob `.tinta`: bloques estructurados y objetos técnicos/vectoriales.
///
/// Ambos repositorios legacy mantienen una caché en memoria y silencian errores
/// de escritura. Para no desincronizar esa caché, esta capa publica mediante su
/// API existente y después relee el JSON canónico para confirmar que el disco
/// contiene exactamente la copia esperada. Un fallo de persistencia se convierte
/// así en un error real de la transacción superior.
final class DuplicadorObjetosExternosTintaIPad {
    static let compartido = DuplicadorObjetosExternosTintaIPad()

    enum Fallo: LocalizedError {
        case mismoDocumento
        case destinoBloquesOcupado
        case destinoTecnicoOcupado
        case verificacionBloques
        case verificacionTecnicos

        var errorDescription: String? {
            switch self {
            case .mismoDocumento:
                return "La copia de objetos externos requiere otra identidad documental."
            case .destinoBloquesOcupado:
                return "El documento destino ya contiene bloques estructurados."
            case .destinoTecnicoOcupado:
                return "El documento destino ya contiene objetos técnicos."
            case .verificacionBloques:
                return "Los bloques duplicados no pudieron verificarse en disco."
            case .verificacionTecnicos:
                return "Los objetos técnicos duplicados no pudieron verificarse en disco."
            }
        }
    }

    private let bloqueo = NSRecursiveLock()
    private let urlBloques: URL
    private let urlTecnicos: URL

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        urlBloques = documentos.appendingPathComponent(
            "Bloques/bloques.json", isDirectory: false)
        urlTecnicos = documentos.appendingPathComponent(
            "ObjetosTecnicos/objetos.json", isDirectory: false)
    }

    @discardableResult
    func duplicarBloques(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw Fallo.mismoDocumento }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let repo = RepositorioBloquesLienzo.compartido
        let fuente = repo.bloques(documento: origen)
        guard repo.bloques(documento: destino).isEmpty else {
            throw Fallo.destinoBloquesOcupado
        }
        guard !fuente.isEmpty else { return 0 }

        // Los UUID de bloque se conservan deliberadamente: están dentro de un
        // namespace documental y pueden ser la clave de procedencia PPTX u otras
        // asociaciones semánticas. Regenerarlos aquí rompería esas referencias.
        repo.guardar(fuente, documento: destino)
        guard try bloquesPersistidos(documento: destino) == fuente else {
            // Mantener la caché legacy coherente con el intento de rollback.
            repo.guardar([], documento: destino)
            throw Fallo.verificacionBloques
        }
        return fuente.count
    }

    @discardableResult
    func duplicarTecnicos(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw Fallo.mismoDocumento }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let repo = RepositorioObjetosTecnicos.compartido
        let fuente = repo.objetos(documento: origen)
        guard repo.objetos(documento: destino).isEmpty else {
            throw Fallo.destinoTecnicoOcupado
        }
        guard !fuente.isEmpty else { return 0 }

        // Igual que los bloques, los UUID de objeto permanecen estables dentro
        // de la copia para que la procedencia PPTX siga resolviendo el mismo
        // elemento lógico bajo otra identidad de documento.
        repo.guardar(fuente, documento: destino)
        guard try tecnicosPersistidos(documento: destino) == fuente else {
            repo.guardar([], documento: destino)
            throw Fallo.verificacionTecnicos
        }
        return fuente.count
    }

    func eliminarBloques(documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let repo = RepositorioBloquesLienzo.compartido
        guard !repo.bloques(documento: documento).isEmpty else { return }
        repo.guardar([], documento: documento)
        guard try bloquesPersistidos(documento: documento).isEmpty else {
            throw Fallo.verificacionBloques
        }
    }

    func eliminarTecnicos(documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let repo = RepositorioObjetosTecnicos.compartido
        guard !repo.objetos(documento: documento).isEmpty else { return }
        repo.guardar([], documento: documento)
        guard try tecnicosPersistidos(documento: documento).isEmpty else {
            throw Fallo.verificacionTecnicos
        }
    }

    func bloquesPersistidos(documento: UInt64) throws -> [BloqueLienzo] {
        guard FileManager.default.fileExists(atPath: urlBloques.path) else { return [] }
        let datos = try Data(contentsOf: urlBloques)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let todos = try decoder.decode([String: [BloqueLienzo]].self, from: datos)
        return todos[String(documento)] ?? []
    }

    func tecnicosPersistidos(documento: UInt64) throws -> [ObjetoTecnico] {
        guard FileManager.default.fileExists(atPath: urlTecnicos.path) else { return [] }
        let datos = try Data(contentsOf: urlTecnicos)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let todos = try decoder.decode([String: [ObjetoTecnico]].self, from: datos)
        return todos[String(documento)] ?? []
    }
}
