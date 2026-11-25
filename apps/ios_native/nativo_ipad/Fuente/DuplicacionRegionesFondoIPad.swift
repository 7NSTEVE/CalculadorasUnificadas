import Foundation

/// Copia verificable de las regiones de papel/fondo que viven fuera del blob
/// `.tinta` en `Documents/RegionesFondo/regiones.json`.
///
/// El repositorio histórico actualiza su caché antes de intentar la escritura y
/// silencia errores de disco. Esta capa mantiene esa caché sincronizada usando la
/// API existente, pero relee el archivo canónico antes de confirmar la etapa de
/// duplicación. Así una región rayada/cuadriculada nunca se considera copiada
/// solo porque quedó presente en RAM.
final class DuplicadorRegionesFondoIPad {
    static let compartido = DuplicadorRegionesFondoIPad()

    enum Fallo: LocalizedError {
        case mismoDocumento
        case destinoOcupado
        case verificacion

        var errorDescription: String? {
            switch self {
            case .mismoDocumento:
                return "Las regiones de fondo requieren una identidad destino distinta."
            case .destinoOcupado:
                return "El documento destino ya contiene regiones de fondo."
            case .verificacion:
                return "Las regiones de fondo duplicadas no pudieron verificarse en disco."
            }
        }
    }

    private let bloqueo = NSRecursiveLock()
    private let url: URL

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        url = documentos.appendingPathComponent(
            "RegionesFondo/regiones.json", isDirectory: false)
    }

    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw Fallo.mismoDocumento }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let repo = RepositorioRegionesFondo.compartido
        let fuente = repo.regiones(origen)
        guard repo.regiones(destino).isEmpty else { throw Fallo.destinoOcupado }
        guard !fuente.isEmpty else { return 0 }

        // Los UUID de región se conservan: quedan aislados por documento y no
        // existe un registro global que necesite distinguirlos. Mantenerlos evita
        // romper referencias futuras a una zona concreta del papel.
        repo.guardar(fuente, documento: destino)
        guard try persistidas(documento: destino) == fuente else {
            repo.guardar([], documento: destino)
            throw Fallo.verificacion
        }
        return fuente.count
    }

    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let repo = RepositorioRegionesFondo.compartido
        guard !repo.regiones(documento).isEmpty else { return }
        repo.guardar([], documento: documento)
        guard try persistidas(documento: documento).isEmpty else {
            throw Fallo.verificacion
        }
    }

    func persistidas(documento: UInt64) throws -> [RegionFondoDocumento] {
        guard FileManager.default.fileExists(atPath: url.path) else { return [] }
        let datos = try Data(contentsOf: url)
        let todos = try JSONDecoder().decode(
            [String: [RegionFondoDocumento]].self, from: datos)
        return todos[String(documento)] ?? []
    }
}
