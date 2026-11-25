import CryptoKit
import Foundation

struct EntradaDiagnosticoPersistenciaIPad: Codable, Equatable {
    enum EstadoActual: String, Codable {
        case coincideOriginal
        case coincideNuevo
        case ausente
        case desconocido
    }

    var destinoPath: String
    var estadoActual: EstadoActual
    var sha256Actual: String?
    var bytesActuales: Int64
    var originalSHA256: String?
    var nuevoSHA256: String?
    var backupIncluido: Bool
    var stagedIncluido: Bool
}

struct ManifiestoDiagnosticoPersistenciaIPad: Codable, Equatable {
    var version = 1
    var transaccionId: UUID
    var documentoId: DocumentoPersistenteID
    var estadoJournal: EstadoJournalPersistenciaIPad
    var creado: Date
    var exportado: Date
    var entradas: [EntradaDiagnosticoPersistenciaIPad]
}

struct ConflictoPersistenciaListadoIPad: Equatable, Identifiable {
    var id: UUID
    var documentoId: DocumentoPersistenteID
    var rutas: [String]
    var actualizado: Date
}

enum ErrorDiagnosticoPersistenciaIPad: LocalizedError, Equatable {
    case noEsConflicto
    case paqueteYaExiste
    case carpetaTransaccionAusente
    case evidenciaIncompleta(String)

    var errorDescription: String? {
        switch self {
        case .noEsConflicto:
            return "La transacción no está marcada como conflicto de recovery."
        case .paqueteYaExiste:
            return "Ya existe un diagnóstico en ese destino."
        case .carpetaTransaccionAusente:
            return "No existe la carpeta de evidencia de la transacción."
        case .evidenciaIncompleta(let nombre):
            return "Falta evidencia transaccional requerida: \(nombre)."
        }
    }
}

/// Lector/resolutor conservador de conflictos. Nunca tiene una operación
/// "forzar original" porque eso podría pisar una edición externa desconocida.
/// La única resolución automática elimina el journal DESPUÉS de exportar toda
/// la evidencia y conserva los archivos actuales tal como están.
final class GestorDiagnosticosPersistenciaIPad {
    private let baseTransacciones: URL
    private let archivos: FileManager
    private let coordinador: CoordinadorPersistenciaTransaccionalIPad

    init(baseTransacciones: URL? = nil,
         coordinador: CoordinadorPersistenciaTransaccionalIPad = .compartido,
         archivos: FileManager = .default) {
        self.archivos = archivos
        self.coordinador = coordinador
        self.baseTransacciones = baseTransacciones ?? archivos.urls(
            for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/Transacciones", isDirectory: true)
    }

    func listarConflictos() -> [ConflictoPersistenciaListadoIPad] {
        let urls = (try? archivos.contentsOfDirectory(
            at: baseTransacciones,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles])) ?? []
        return urls.compactMap { carpeta in
            let idTexto = carpeta.lastPathComponent
            guard let id = UUID(uuidString: idTexto),
                  let journal = try? coordinador.journal(id),
                  journal.estado == .conflictoRecovery else { return nil }
            return .init(id: id,
                         documentoId: journal.documentoId,
                         rutas: journal.entradas.map(\.destinoPath),
                         actualizado: journal.actualizado)
        }.sorted { $0.actualizado > $1.actualizado }
    }

    @discardableResult
    func exportar(transaccionId: UUID, destino: URL) throws
        -> ManifiestoDiagnosticoPersistenciaIPad {
        guard !archivos.fileExists(atPath: destino.path) else {
            throw ErrorDiagnosticoPersistenciaIPad.paqueteYaExiste
        }
        let journal = try coordinador.journal(transaccionId)
        guard journal.estado == .conflictoRecovery else {
            throw ErrorDiagnosticoPersistenciaIPad.noEsConflicto
        }
        let fuente = carpetaTransaccion(transaccionId)
        guard archivos.fileExists(atPath: fuente.path) else {
            throw ErrorDiagnosticoPersistenciaIPad.carpetaTransaccionAusente
        }

        let padre = destino.deletingLastPathComponent()
        try archivos.createDirectory(at: padre, withIntermediateDirectories: true)
        let temporal = padre.appendingPathComponent(
            ".diagnostico-\(UUID().uuidString)", isDirectory: true)
        try archivos.createDirectory(at: temporal, withIntermediateDirectories: true)

        do {
            var entradas: [EntradaDiagnosticoPersistenciaIPad] = []
            for (indice, entrada) in journal.entradas.enumerated() {
                let destinoActual = URL(fileURLWithPath: entrada.destinoPath)
                let actual = archivos.fileExists(atPath: destinoActual.path)
                    ? try Data(contentsOf: destinoActual, options: [.mappedIfSafe])
                    : nil
                let actualHash = actual.map(Self.sha256)
                let estado: EntradaDiagnosticoPersistenciaIPad.EstadoActual
                if actual == nil {
                    estado = .ausente
                } else if actualHash == entrada.originalSHA256 {
                    estado = .coincideOriginal
                } else if actualHash == entrada.nuevoSHA256 {
                    estado = .coincideNuevo
                } else {
                    estado = .desconocido
                }

                var backupIncluido = false
                if let backupNombre = entrada.backupNombre {
                    let origen = fuente.appendingPathComponent(backupNombre)
                    guard archivos.fileExists(atPath: origen.path) else {
                        throw ErrorDiagnosticoPersistenciaIPad.evidenciaIncompleta(
                            backupNombre)
                    }
                    try archivos.copyItem(
                        at: origen,
                        to: temporal.appendingPathComponent("backup-\(indice).bin"))
                    backupIncluido = true
                }

                var stagedIncluido = false
                if let stagedNombre = entrada.stagedNombre {
                    let origen = fuente.appendingPathComponent(stagedNombre)
                    guard archivos.fileExists(atPath: origen.path) else {
                        throw ErrorDiagnosticoPersistenciaIPad.evidenciaIncompleta(
                            stagedNombre)
                    }
                    try archivos.copyItem(
                        at: origen,
                        to: temporal.appendingPathComponent("staged-\(indice).bin"))
                    stagedIncluido = true
                }

                if let actual {
                    try actual.write(
                        to: temporal.appendingPathComponent("actual-\(indice).bin"),
                        options: [.atomic, .completeFileProtection])
                }

                entradas.append(.init(
                    destinoPath: entrada.destinoPath,
                    estadoActual: estado,
                    sha256Actual: actualHash,
                    bytesActuales: Int64(actual?.count ?? 0),
                    originalSHA256: entrada.originalSHA256,
                    nuevoSHA256: entrada.nuevoSHA256,
                    backupIncluido: backupIncluido,
                    stagedIncluido: stagedIncluido))
            }

            let manifiesto = ManifiestoDiagnosticoPersistenciaIPad(
                transaccionId: journal.id,
                documentoId: journal.documentoId,
                estadoJournal: journal.estado,
                creado: journal.creado,
                exportado: Date(),
                entradas: entradas)
            let encoder = JSONEncoder()
            encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            try encoder.encode(journal).write(
                to: temporal.appendingPathComponent("journal.json"),
                options: [.atomic, .completeFileProtection])
            try encoder.encode(manifiesto).write(
                to: temporal.appendingPathComponent("diagnostico.json"),
                options: [.atomic, .completeFileProtection])

            try archivos.moveItem(at: temporal, to: destino)
            return manifiesto
        } catch {
            try? archivos.removeItem(at: temporal)
            throw error
        }
    }

    /// Resolución no destructiva: primero exporta evidencia completa y sólo
    /// después retira el journal. Los archivos actuales no se modifican.
    @discardableResult
    func conservarActualesYResolver(transaccionId: UUID,
                                    diagnosticoDestino: URL) throws
        -> ManifiestoDiagnosticoPersistenciaIPad {
        let manifiesto = try exportar(
            transaccionId: transaccionId,
            destino: diagnosticoDestino)
        let journal = try coordinador.journal(transaccionId)
        guard journal.estado == .conflictoRecovery else {
            throw ErrorDiagnosticoPersistenciaIPad.noEsConflicto
        }
        try archivos.removeItem(at: carpetaTransaccion(transaccionId))
        return manifiesto
    }

    func destinoDiagnosticoAutomatico(transaccionId: UUID) -> URL {
        let documentos = archivos.urls(for: .documentDirectory,
                                       in: .userDomainMask)[0]
        return documentos
            .appendingPathComponent("RecuperacionConflictos", isDirectory: true)
            .appendingPathComponent(
                "conflicto-\(transaccionId.uuidString.lowercased())",
                isDirectory: true)
    }

    private func carpetaTransaccion(_ id: UUID) -> URL {
        baseTransacciones.appendingPathComponent(
            id.uuidString.lowercased(), isDirectory: true)
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
