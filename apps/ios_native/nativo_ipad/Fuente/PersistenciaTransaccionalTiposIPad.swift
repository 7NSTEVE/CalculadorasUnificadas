import Foundation

enum PrecondicionArchivoTransaccionalIPad: Equatable {
    /// Captura el estado actual durante prepare y vuelve a comprobarlo justo antes de commit.
    case capturarActual
    case debeNoExistir
    case sha256(String)
}

enum MutacionArchivoTransaccionalIPad {
    case escribir(Data)
    case eliminar
}

struct CambioArchivoTransaccionalIPad {
    var destino: URL
    var mutacion: MutacionArchivoTransaccionalIPad
    var precondicion: PrecondicionArchivoTransaccionalIPad = .capturarActual

    init(destino: URL,
         mutacion: MutacionArchivoTransaccionalIPad,
         precondicion: PrecondicionArchivoTransaccionalIPad = .capturarActual) {
        self.destino = destino
        self.mutacion = mutacion
        self.precondicion = precondicion
    }
}

enum EstadoJournalPersistenciaIPad: String, Codable {
    case preparada
    case aplicando
    case confirmada
    case rollbackPendiente
    case conflictoRecovery
}

struct TransaccionPreparadaPersistenciaIPad: Equatable, Identifiable {
    var id: UUID
    var documentoId: DocumentoPersistenteID
    var cantidadCambios: Int
}

struct ResultadoRecoveryPersistenciaIPad: Equatable {
    var restauradas: [UUID] = []
    var limpiadasConfirmadas: [UUID] = []
    var conflictos: [UUID] = []
}

enum ErrorPersistenciaTransaccionalIPad: LocalizedError, Equatable {
    case sinCambios
    case destinoDuplicado(String)
    case rutaNoAbsoluta(String)
    case precondicionNoCumplida(String)
    case cambioConcurrente(String)
    case transaccionInexistente
    case journalInvalido
    case backupAusente(String)
    case recoveryEnConflicto(String)

    var errorDescription: String? {
        switch self {
        case .sinCambios:
            return "La transacción no contiene cambios."
        case .destinoDuplicado(let ruta):
            return "La misma ruta aparece más de una vez en la transacción: \(ruta)"
        case .rutaNoAbsoluta(let ruta):
            return "El destino de persistencia debe ser una URL local absoluta: \(ruta)"
        case .precondicionNoCumplida(let ruta):
            return "El archivo cambió antes de preparar la transacción: \(ruta)"
        case .cambioConcurrente(let ruta):
            return "El archivo cambió entre prepare y commit: \(ruta)"
        case .transaccionInexistente:
            return "La transacción preparada ya no existe."
        case .journalInvalido:
            return "El journal transaccional está dañado."
        case .backupAusente(let ruta):
            return "Falta el backup necesario para restaurar \(ruta)."
        case .recoveryEnConflicto(let ruta):
            return "Recovery encontró cambios externos y no sobreescribirá \(ruta)."
        }
    }
}

struct EntradaJournalPersistenciaIPad: Codable, Equatable {
    enum Operacion: String, Codable { case escribir, eliminar }

    var destinoPath: String
    var operacion: Operacion
    var originalExistia: Bool
    var originalSHA256: String?
    var nuevoSHA256: String?
    var stagedNombre: String?
    var backupNombre: String?
}

struct JournalPersistenciaIPad: Codable, Equatable {
    var version = 1
    var id: UUID
    var documentoId: DocumentoPersistenteID
    var estado: EstadoJournalPersistenciaIPad
    var creado: Date
    var actualizado: Date
    var aplicados: Int
    var entradas: [EntradaJournalPersistenciaIPad]
}
