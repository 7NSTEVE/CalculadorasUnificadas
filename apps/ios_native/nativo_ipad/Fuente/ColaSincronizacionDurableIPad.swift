import Foundation

enum EstadoEnvioSyncIPad: String, Codable { case pendiente, enviando, confirmado, fallido }

struct SobreOperacionSyncIPad: Codable, Equatable, Identifiable {
    var id: UUID { operacion.id }
    var documentoId: DocumentoPersistenteID
    var operacion: OperacionColaborativa
    var estado: EstadoEnvioSyncIPad = .pendiente
    var intentos = 0
    var proximoIntento = Date()
    var ultimoError: String?
    var creada = Date()
    var actualizada = Date()
}

private struct ArchivoColaSyncIPad: Codable {
    var version = 1
    var operaciones: [SobreOperacionSyncIPad] = []
}

enum ErrorColaSyncIPad: LocalizedError, Equatable {
    case identidadLegacyNoCoincide
    var errorDescription: String? { "La operación legacy no corresponde al UUID durable del documento." }
}

final class ColaSincronizacionDurableIPad {
    static let compartida = ColaSincronizacionDurableIPad()
    static let cambio = Notification.Name("CuadernU.Sync.Cola.Cambio")
    private let bloqueo = NSRecursiveLock(); private let url: URL
    private var archivo: ArchivoColaSyncIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Sync/Cola", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("operaciones-v1.json")
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url), let leido = try? d.decode(ArchivoColaSyncIPad.self, from: datos) { archivo = leido }
        else { archivo = ArchivoColaSyncIPad() }
    }

    @discardableResult
    func encolar(documentoId: DocumentoPersistenteID,
                 operacion: OperacionColaborativa) throws -> SobreOperacionSyncIPad {
        guard operacion.documentoId == documentoId.claveCompatibilidadUInt64 else { throw ErrorColaSyncIPad.identidadLegacyNoCoincide }
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let existente = archivo.operaciones.first(where: { $0.operacion.id == operacion.id }) { return existente }
        let sobre = SobreOperacionSyncIPad(documentoId: documentoId, operacion: operacion)
        archivo.operaciones.append(sobre); persistir(); publicar(); return sobre
    }

    func listos(ahora: Date = Date(), limite: Int = 100) -> [SobreOperacionSyncIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.operaciones.filter { $0.estado != .confirmado && $0.proximoIntento <= ahora }
            .sorted { $0.operacion.marca == $1.operacion.marca ? $0.creada < $1.creada : $0.operacion.marca < $1.operacion.marca }
            .prefix(max(1, limite)).map { $0 }
    }

    func pendientes(documentoId: DocumentoPersistenteID? = nil) -> [SobreOperacionSyncIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.operaciones.filter { $0.estado != .confirmado && (documentoId == nil || $0.documentoId == documentoId) }
    }

    func marcarEnviando(_ ids: Set<UUID>) { mutar(ids) { if $0.estado != .confirmado { $0.estado = .enviando; $0.intentos += 1; $0.ultimoError = nil } } }
    func confirmar(_ ids: Set<UUID>) { mutar(ids) { $0.estado = .confirmado; $0.ultimoError = nil } }

    func fallar(_ ids: Set<UUID>, error: String, ahora: Date = Date()) {
        mutar(ids) { item in
            guard item.estado != .confirmado else { return }
            item.estado = .fallido; item.ultimoError = String(error.prefix(500))
            let e = min(max(item.intentos, 1), 10)
            item.proximoIntento = ahora.addingTimeInterval(min(pow(2, Double(e)) * 5, 3600))
        }
    }

    func reintentarAhora(documentoId: DocumentoPersistenteID? = nil) {
        bloqueo.lock(); defer { bloqueo.unlock() }; let ahora = Date()
        for i in archivo.operaciones.indices where archivo.operaciones[i].estado != .confirmado && (documentoId == nil || archivo.operaciones[i].documentoId == documentoId) {
            archivo.operaciones[i].estado = .pendiente; archivo.operaciones[i].proximoIntento = ahora; archivo.operaciones[i].actualizada = ahora
        }
        persistir(); publicar()
    }

    func compactarConfirmadas(anterioresA fecha: Date) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.operaciones.removeAll { $0.estado == .confirmado && $0.actualizada < fecha }
        persistir(); publicar()
    }

    private func mutar(_ ids: Set<UUID>, _ cambios: (inout SobreOperacionSyncIPad) -> Void) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for i in archivo.operaciones.indices where ids.contains(archivo.operaciones[i].operacion.id) { cambios(&archivo.operaciones[i]); archivo.operaciones[i].actualizada = Date() }
        persistir(); publicar()
    }

    private func persistir() {
        let e = JSONEncoder(); e.dateEncodingStrategy = .iso8601; e.outputFormatting = [.sortedKeys]
        if let datos = try? e.encode(archivo) { try? datos.write(to: url, options: [.atomic, .completeFileProtection]) }
    }
    private func publicar() { DispatchQueue.main.async { NotificationCenter.default.post(name: Self.cambio, object: self) } }
}
