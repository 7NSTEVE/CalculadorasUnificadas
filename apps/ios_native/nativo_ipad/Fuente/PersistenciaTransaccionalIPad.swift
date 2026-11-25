import CryptoKit
import Foundation

/// Coordinador de transacciones multiarchivo para documento + sidecars.
///
/// Un filesystem no puede hacer atómico un conjunto arbitrario de renames. Este
/// coordinador usa prepare + backups + journal durable: cualquier commit no
/// confirmado se revierte en el siguiente arranque. Si recovery encuentra un
/// cambio externo desconocido, no lo sobreescribe y marca conflicto explícito.
final class CoordinadorPersistenciaTransaccionalIPad {
    static let compartido = CoordinadorPersistenciaTransaccionalIPad()

    private let bloqueo = NSRecursiveLock()
    private let base: URL
    private let archivos: FileManager

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory,
                                           in: .userDomainMask)[0]
            .appendingPathComponent("Persistencia/Transacciones", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
        try? archivos.setAttributes(
            [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication],
            ofItemAtPath: self.base.path)
    }

    @discardableResult
    func ejecutar(documentoId: DocumentoPersistenteID,
                  cambios: [CambioArchivoTransaccionalIPad]) throws
        -> TransaccionPreparadaPersistenciaIPad {
        let preparada = try preparar(documentoId: documentoId, cambios: cambios)
        do {
            try confirmar(preparada.id)
            return preparada
        } catch {
            try? rollback(preparada.id)
            throw error
        }
    }

    func preparar(documentoId: DocumentoPersistenteID,
                  cambios: [CambioArchivoTransaccionalIPad]) throws
        -> TransaccionPreparadaPersistenciaIPad {
        guard !cambios.isEmpty else { throw ErrorPersistenciaTransaccionalIPad.sinCambios }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let normalizados = try validarDestinos(cambios)
        let id = UUID()
        let carpeta = carpetaTransaccion(id)
        try archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)

        do {
            var entradas: [EntradaJournalPersistenciaIPad] = []
            for (indice, cambio) in normalizados.enumerated() {
                let destino = cambio.destino
                let original = try datosActuales(destino)
                try verificarPrecondicion(cambio.precondicion,
                                          actual: original,
                                          ruta: destino.path)

                let backupNombre: String?
                if let original {
                    let nombre = "backup-\(indice).bin"
                    try original.write(to: carpeta.appendingPathComponent(nombre),
                                       options: [.atomic, .completeFileProtection])
                    backupNombre = nombre
                } else {
                    backupNombre = nil
                }

                switch cambio.mutacion {
                case .escribir(let nuevos):
                    let staged = "staged-\(indice).bin"
                    try nuevos.write(to: carpeta.appendingPathComponent(staged),
                                     options: [.atomic, .completeFileProtection])
                    entradas.append(.init(
                        destinoPath: destino.path,
                        operacion: .escribir,
                        originalExistia: original != nil,
                        originalSHA256: original.map(Self.sha256),
                        nuevoSHA256: Self.sha256(nuevos),
                        stagedNombre: staged,
                        backupNombre: backupNombre))

                case .eliminar:
                    entradas.append(.init(
                        destinoPath: destino.path,
                        operacion: .eliminar,
                        originalExistia: original != nil,
                        originalSHA256: original.map(Self.sha256),
                        nuevoSHA256: nil,
                        stagedNombre: nil,
                        backupNombre: backupNombre))
                }
            }

            let ahora = Date()
            let journal = JournalPersistenciaIPad(
                id: id, documentoId: documentoId,
                estado: .preparada,
                creado: ahora, actualizado: ahora,
                aplicados: 0, entradas: entradas)
            try guardarJournal(journal, en: carpeta)
            return .init(id: id, documentoId: documentoId,
                         cantidadCambios: entradas.count)
        } catch {
            try? archivos.removeItem(at: carpeta)
            throw error
        }
    }

    func confirmar(_ id: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let carpeta = carpetaTransaccion(id)
        var journal = try cargarJournal(carpeta)
        guard journal.estado == .preparada || journal.estado == .aplicando else {
            throw ErrorPersistenciaTransaccionalIPad.journalInvalido
        }

        journal.estado = .aplicando
        journal.actualizado = Date()
        try guardarJournal(journal, en: carpeta)

        do {
            for indice in journal.aplicados..<journal.entradas.count {
                let entrada = journal.entradas[indice]
                let destino = URL(fileURLWithPath: entrada.destinoPath)
                try verificarEstadoOriginal(entrada, destino: destino)
                try aplicar(entrada, carpeta: carpeta, destino: destino)
                journal.aplicados = indice + 1
                journal.actualizado = Date()
                try guardarJournal(journal, en: carpeta)
            }
            journal.estado = .confirmada
            journal.actualizado = Date()
            try guardarJournal(journal, en: carpeta)
            try archivos.removeItem(at: carpeta)
        } catch {
            journal.estado = .rollbackPendiente
            journal.actualizado = Date()
            try? guardarJournal(journal, en: carpeta)
            throw error
        }
    }

    func rollback(_ id: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let carpeta = carpetaTransaccion(id)
        guard archivos.fileExists(atPath: carpeta.path) else {
            throw ErrorPersistenciaTransaccionalIPad.transaccionInexistente
        }
        let journal = try cargarJournal(carpeta)
        try restaurar(journal: journal, carpeta: carpeta)
        try archivos.removeItem(at: carpeta)
    }

    @discardableResult
    func recuperarPendientes() -> ResultadoRecoveryPersistenciaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var resultado = ResultadoRecoveryPersistenciaIPad()
        let candidatos = (try? archivos.contentsOfDirectory(
            at: base, includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles])) ?? []

        for carpeta in candidatos {
            let esDirectorio = (try? carpeta.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory == true
            guard esDirectorio, let journal = try? cargarJournal(carpeta) else {
                continue
            }
            if journal.estado == .confirmada {
                try? archivos.removeItem(at: carpeta)
                resultado.limpiadasConfirmadas.append(journal.id)
                continue
            }
            do {
                try restaurar(journal: journal, carpeta: carpeta)
                try archivos.removeItem(at: carpeta)
                resultado.restauradas.append(journal.id)
            } catch {
                var conflicto = journal
                conflicto.estado = .conflictoRecovery
                conflicto.actualizado = Date()
                try? guardarJournal(conflicto, en: carpeta)
                resultado.conflictos.append(journal.id)
            }
        }
        return resultado
    }

    func journal(_ id: UUID) throws -> JournalPersistenciaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return try cargarJournal(carpetaTransaccion(id))
    }

    // MARK: - Commit / rollback

    private func aplicar(_ entrada: EntradaJournalPersistenciaIPad,
                         carpeta: URL,
                         destino: URL) throws {
        try archivos.createDirectory(at: destino.deletingLastPathComponent(),
                                     withIntermediateDirectories: true)
        switch entrada.operacion {
        case .escribir:
            guard let staged = entrada.stagedNombre else {
                throw ErrorPersistenciaTransaccionalIPad.journalInvalido
            }
            let datos = try Data(contentsOf: carpeta.appendingPathComponent(staged),
                                 options: [.mappedIfSafe])
            guard Self.sha256(datos) == entrada.nuevoSHA256 else {
                throw ErrorPersistenciaTransaccionalIPad.journalInvalido
            }
            try datos.write(to: destino, options: [.atomic, .completeFileProtection])
        case .eliminar:
            if archivos.fileExists(atPath: destino.path) {
                try archivos.removeItem(at: destino)
            }
        }
    }

    /// Restaura todas las entradas cuyo estado sigue siendo reconocible. Un
    /// conflicto en un archivo no impide recuperar los demás; al final se
    /// devuelve error para conservar el journal y exigir intervención explícita.
    private func restaurar(journal: JournalPersistenciaIPad,
                           carpeta: URL) throws {
        var primerConflicto: String?

        for entrada in journal.entradas.reversed() {
            let destino = URL(fileURLWithPath: entrada.destinoPath)
            let actual = try datosActuales(destino)
            let actualHash = actual.map(Self.sha256)
            let estadoEsperable: Bool

            switch entrada.operacion {
            case .escribir:
                estadoEsperable = actualHash == entrada.nuevoSHA256 ||
                    actualHash == entrada.originalSHA256 ||
                    (!entrada.originalExistia && actual == nil)
            case .eliminar:
                estadoEsperable = actual == nil || actualHash == entrada.originalSHA256
            }

            guard estadoEsperable else {
                if primerConflicto == nil { primerConflicto = entrada.destinoPath }
                continue
            }

            if entrada.originalExistia {
                guard let backupNombre = entrada.backupNombre else {
                    throw ErrorPersistenciaTransaccionalIPad.backupAusente(
                        entrada.destinoPath)
                }
                let backupURL = carpeta.appendingPathComponent(backupNombre)
                guard archivos.fileExists(atPath: backupURL.path) else {
                    throw ErrorPersistenciaTransaccionalIPad.backupAusente(
                        entrada.destinoPath)
                }
                let backup = try Data(contentsOf: backupURL, options: [.mappedIfSafe])
                guard Self.sha256(backup) == entrada.originalSHA256 else {
                    throw ErrorPersistenciaTransaccionalIPad.journalInvalido
                }
                try archivos.createDirectory(at: destino.deletingLastPathComponent(),
                                             withIntermediateDirectories: true)
                try backup.write(to: destino,
                                 options: [.atomic, .completeFileProtection])
            } else if actual != nil {
                try archivos.removeItem(at: destino)
            }
        }

        if let primerConflicto {
            throw ErrorPersistenciaTransaccionalIPad.recoveryEnConflicto(
                primerConflicto)
        }
    }

    private func verificarEstadoOriginal(_ entrada: EntradaJournalPersistenciaIPad,
                                         destino: URL) throws {
        let actual = try datosActuales(destino)
        if entrada.originalExistia {
            guard let actual,
                  Self.sha256(actual) == entrada.originalSHA256 else {
                throw ErrorPersistenciaTransaccionalIPad.cambioConcurrente(
                    entrada.destinoPath)
            }
        } else if actual != nil {
            throw ErrorPersistenciaTransaccionalIPad.cambioConcurrente(
                entrada.destinoPath)
        }
    }

    // MARK: - Prepare helpers

    private func validarDestinos(_ cambios: [CambioArchivoTransaccionalIPad]) throws
        -> [CambioArchivoTransaccionalIPad] {
        var vistos = Set<String>()
        return try cambios.map { cambio in
            guard cambio.destino.isFileURL,
                  cambio.destino.path.hasPrefix("/") else {
                throw ErrorPersistenciaTransaccionalIPad.rutaNoAbsoluta(
                    cambio.destino.path)
            }
            let estandar = cambio.destino.standardizedFileURL
            guard vistos.insert(estandar.path).inserted else {
                throw ErrorPersistenciaTransaccionalIPad.destinoDuplicado(estandar.path)
            }
            var copia = cambio
            copia.destino = estandar
            return copia
        }
    }

    private func verificarPrecondicion(_ precondicion: PrecondicionArchivoTransaccionalIPad,
                                       actual: Data?,
                                       ruta: String) throws {
        switch precondicion {
        case .capturarActual:
            return
        case .debeNoExistir:
            guard actual == nil else {
                throw ErrorPersistenciaTransaccionalIPad.precondicionNoCumplida(ruta)
            }
        case .sha256(let esperado):
            guard let actual, Self.sha256(actual) == esperado.lowercased() else {
                throw ErrorPersistenciaTransaccionalIPad.precondicionNoCumplida(ruta)
            }
        }
    }

    private func datosActuales(_ url: URL) throws -> Data? {
        guard archivos.fileExists(atPath: url.path) else { return nil }
        return try Data(contentsOf: url, options: [.mappedIfSafe])
    }

    // MARK: - Journal

    private func carpetaTransaccion(_ id: UUID) -> URL {
        base.appendingPathComponent(id.uuidString.lowercased(), isDirectory: true)
    }

    private func cargarJournal(_ carpeta: URL) throws -> JournalPersistenciaIPad {
        let url = carpeta.appendingPathComponent("journal.json")
        guard archivos.fileExists(atPath: url.path) else {
            throw ErrorPersistenciaTransaccionalIPad.transaccionInexistente
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        do {
            return try decoder.decode(JournalPersistenciaIPad.self,
                                      from: Data(contentsOf: url))
        } catch {
            throw ErrorPersistenciaTransaccionalIPad.journalInvalido
        }
    }

    private func guardarJournal(_ journal: JournalPersistenciaIPad,
                                en carpeta: URL) throws {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        try encoder.encode(journal).write(
            to: carpeta.appendingPathComponent("journal.json"),
            options: [.atomic, .completeFileProtection])
    }

    static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}
