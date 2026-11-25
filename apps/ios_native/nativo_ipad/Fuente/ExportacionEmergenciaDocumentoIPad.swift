import CryptoKit
import Foundation

struct EntradaExportacionEmergenciaIPad: Codable, Equatable {
    enum Estado: String, Codable {
        case coincideEsperado
        case alterado
        case faltante
        case presenteNoInventariado
        case correctamenteAusente
    }

    var rutaOriginal: String
    var nombreCopia: String?
    var estado: Estado
    var bytesActuales: Int64
    var sha256Actual: String?
    var sha256Esperado: String?
}

struct ManifiestoExportacionEmergenciaIPad: Codable, Equatable {
    var version = 1
    var exportacionId: UUID
    var documentoId: DocumentoPersistenteID
    var fecha: Date
    var revisionId: UUID?
    var revisionDisponible: Bool
    var conflictosRecovery: [UUID]
    var entradas: [EntradaExportacionEmergenciaIPad]

    var faltantes: Int { entradas.filter { $0.estado == .faltante }.count }
    var alterados: Int { entradas.filter { $0.estado == .alterado }.count }
}

enum ErrorExportacionEmergenciaIPad: LocalizedError, Equatable {
    case destinoYaExiste
    case rutaAdicionalNoLocal(String)

    var errorDescription: String? {
        switch self {
        case .destinoYaExiste: return "Ya existe un paquete en el destino solicitado."
        case .rutaAdicionalNoLocal(let ruta): return "La ruta adicional no es un archivo local: \(ruta)."
        }
    }
}

/// Exporta el estado ACTUAL sin intentar repararlo. Es deliberadamente más
/// tolerante que un backup normal: archivos faltantes/alterados se registran en
/// el manifest, pero todo lo que sí puede leerse se copia para rescate manual.
enum ExportadorEmergenciaDocumentoIPad {
    @discardableResult
    static func exportar(
        documentoId: DocumentoPersistenteID,
        destino: URL,
        revisiones: RepositorioRevisionesGuardadoIPad = .compartido,
        conflictos: GestorDiagnosticosPersistenciaIPad = .init(),
        rutasAdicionales: [URL] = [],
        archivos: FileManager = .default
    ) throws -> ManifiestoExportacionEmergenciaIPad {
        guard !archivos.fileExists(atPath: destino.path) else {
            throw ErrorExportacionEmergenciaIPad.destinoYaExiste
        }
        for ruta in rutasAdicionales where !ruta.isFileURL {
            throw ErrorExportacionEmergenciaIPad.rutaAdicionalNoLocal(ruta.absoluteString)
        }

        let revision = try? revisiones.cargar(documentoId)
        var inventario: [String: EntradaRevisionGuardadoIPad] = [:]
        for entrada in revision?.entradas ?? [] {
            inventario[entrada.destinoPath] = entrada
        }
        for extra in rutasAdicionales {
            let path = extra.standardizedFileURL.path
            if inventario[path] == nil {
                inventario[path] = .init(
                    destinoPath: path,
                    presente: true,
                    bytes: -1,
                    sha256: nil)
            }
        }

        let padre = destino.deletingLastPathComponent()
        try archivos.createDirectory(at: padre, withIntermediateDirectories: true)
        let temporal = padre.appendingPathComponent(
            ".emergencia-\(UUID().uuidString)", isDirectory: true)
        try archivos.createDirectory(at: temporal, withIntermediateDirectories: true)
        let copias = temporal.appendingPathComponent("archivos", isDirectory: true)
        try archivos.createDirectory(at: copias, withIntermediateDirectories: true)

        do {
            var entradasSalida: [EntradaExportacionEmergenciaIPad] = []
            for (indice, esperado) in inventario.values
                .sorted(by: { $0.destinoPath < $1.destinoPath }).enumerated() {
                let original = URL(fileURLWithPath: esperado.destinoPath)
                let existe = archivos.fileExists(atPath: original.path)

                if !esperado.presente {
                    if existe {
                        let datos = try Data(contentsOf: original, options: [.mappedIfSafe])
                        let nombre = "\(indice)-actual.bin"
                        try datos.write(to: copias.appendingPathComponent(nombre),
                                        options: [.atomic, .completeFileProtection])
                        entradasSalida.append(.init(
                            rutaOriginal: original.path,
                            nombreCopia: "archivos/\(nombre)",
                            estado: .alterado,
                            bytesActuales: Int64(datos.count),
                            sha256Actual: sha256(datos),
                            sha256Esperado: nil))
                    } else {
                        entradasSalida.append(.init(
                            rutaOriginal: original.path,
                            nombreCopia: nil,
                            estado: .correctamenteAusente,
                            bytesActuales: 0,
                            sha256Actual: nil,
                            sha256Esperado: nil))
                    }
                    continue
                }

                guard existe else {
                    entradasSalida.append(.init(
                        rutaOriginal: original.path,
                        nombreCopia: nil,
                        estado: .faltante,
                        bytesActuales: 0,
                        sha256Actual: nil,
                        sha256Esperado: esperado.sha256))
                    continue
                }

                let datos = try Data(contentsOf: original, options: [.mappedIfSafe])
                let hash = sha256(datos)
                let nombre = "\(indice)-actual.bin"
                try datos.write(to: copias.appendingPathComponent(nombre),
                                options: [.atomic, .completeFileProtection])
                let estado: EntradaExportacionEmergenciaIPad.Estado
                if esperado.sha256 == nil {
                    estado = .presenteNoInventariado
                } else if hash == esperado.sha256 &&
                            (esperado.bytes < 0 || Int64(datos.count) == esperado.bytes) {
                    estado = .coincideEsperado
                } else {
                    estado = .alterado
                }
                entradasSalida.append(.init(
                    rutaOriginal: original.path,
                    nombreCopia: "archivos/\(nombre)",
                    estado: estado,
                    bytesActuales: Int64(datos.count),
                    sha256Actual: hash,
                    sha256Esperado: esperado.sha256))
            }

            if let revision {
                let e = JSONEncoder(); e.dateEncodingStrategy = .iso8601
                e.outputFormatting = [.prettyPrinted, .sortedKeys]
                try e.encode(revision).write(
                    to: temporal.appendingPathComponent("revision.json"),
                    options: [.atomic, .completeFileProtection])
            }

            let idsConflicto = conflictos.listarConflictos()
                .filter { $0.documentoId == documentoId }
                .map(\.id)
            let manifiesto = ManifiestoExportacionEmergenciaIPad(
                exportacionId: UUID(),
                documentoId: documentoId,
                fecha: Date(),
                revisionId: revision?.revisionId,
                revisionDisponible: revision != nil,
                conflictosRecovery: idsConflicto,
                entradas: entradasSalida)
            let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            try encoder.encode(manifiesto).write(
                to: temporal.appendingPathComponent("emergencia.json"),
                options: [.atomic, .completeFileProtection])

            try archivos.moveItem(at: temporal, to: destino)
            return manifiesto
        } catch {
            try? archivos.removeItem(at: temporal)
            throw error
        }
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
