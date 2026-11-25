import Foundation

enum ModoCapturaAudioResilienteIPad: String, Codable, Equatable {
    case soloAudio
    case notaViva
}

enum EstadoVinculoCapturaAudioIPad: String, Codable, Equatable {
    case preparando
    case capturando
    case pausado
    case confirmado
    case cancelado
    case requiereRecuperacion
}

struct VinculoCapturaAudioIPad: Codable, Identifiable, Equatable {
    var version = 1
    var id: UUID { sesionAudioId }
    var sesionAudioId: UUID
    var documentoId: UInt64
    var modo: ModoCapturaAudioResilienteIPad
    var estado: EstadoVinculoCapturaAudioIPad = .preparando
    /// Ruta relativa al data container actual. Nunca se persiste un path
    /// absoluto ni se permite escapar mediante `..`.
    var fuenteRelativa: String?
    /// En Nota Viva la fuente durante captura es el paquete/manifest; el audio
    /// definitivo se resuelve desde el manifest al detener correctamente.
    var esManifiestoNotaViva = false
    var creada = Date()
    var actualizada = Date()
    var nota: String?
}

enum ErrorVinculoCapturaAudioIPad: LocalizedError, Equatable {
    case vinculoNoExiste
    case versionNoSoportada
    case rutaFueraDelContenedor
    case rutaInvalida
    case archivoFinalNoDisponible
    case manifiestoNotaVivaInvalido

    var errorDescription: String? {
        switch self {
        case .vinculoNoExiste:
            return "No existe el journal de recuperación de esta grabación."
        case .versionNoSoportada:
            return "El journal de recuperación usa una versión no soportada."
        case .rutaFueraDelContenedor:
            return "La fuente de audio no pertenece al contenedor de CuadernU."
        case .rutaInvalida:
            return "La ruta de recuperación de audio no es válida."
        case .archivoFinalNoDisponible:
            return "La grabación terminó sin un archivo de audio verificable."
        case .manifiestoNotaVivaInvalido:
            return "Nota Viva terminó sin un manifiesto de audio verificable."
        }
    }
}

/// Journal pequeño que enlaza el capturador real con `RepositorioAudioResilienteIPad`.
/// No contiene audio ni sustituye al manifest SHA de segmentos: únicamente
/// permite saber qué archivo estaba escribiendo el proceso si termina abruptamente.
final class RepositorioVinculosCapturaAudioIPad {
    static let compartido = RepositorioVinculosCapturaAudioIPad()

    private let archivos: FileManager
    private let base: URL
    private let raizContenedor: URL
    private let cola = DispatchQueue(label: "cuadernu.audio.vinculo-captura")

    init(base: URL? = nil,
         raizContenedor: URL? = nil,
         archivos: FileManager = .default) {
        self.archivos = archivos
        self.raizContenedor = (raizContenedor
            ?? URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true))
            .standardizedFileURL
        if let base {
            self.base = base
        } else {
            self.base = archivos.urls(
                for: .applicationSupportDirectory,
                in: .userDomainMask)[0]
                .appendingPathComponent("Tinta/AudioResilienteVinculos",
                                        isDirectory: true)
        }
        try? archivos.createDirectory(at: self.base,
                                      withIntermediateDirectories: true)
    }

    @discardableResult
    func crear(documentoId: UInt64,
               modo: ModoCapturaAudioResilienteIPad,
               repositorioAudio: RepositorioAudioResilienteIPad = .compartido)
        throws -> VinculoCapturaAudioIPad {
        let sesion = try repositorioAudio.crearSesion(documentoId: documentoId)
        let vinculo = VinculoCapturaAudioIPad(
            sesionAudioId: sesion.id,
            documentoId: documentoId,
            modo: modo)
        try cola.sync { try guardarSinCola(vinculo) }
        return vinculo
    }

    func leer(_ id: UUID) throws -> VinculoCapturaAudioIPad {
        try cola.sync { try leerSinCola(id) }
    }

    @discardableResult
    func vincularFuente(_ url: URL,
                        sesion id: UUID,
                        esManifiestoNotaViva: Bool) throws -> VinculoCapturaAudioIPad {
        let relativa = try rutaRelativaSegura(url)
        return try modificar(id) { valor in
            valor.fuenteRelativa = relativa
            valor.esManifiestoNotaViva = esManifiestoNotaViva
            valor.estado = .capturando
            valor.nota = nil
        }
    }

    @discardableResult
    func marcarPausa(_ id: UUID) throws -> VinculoCapturaAudioIPad {
        try modificar(id) { $0.estado = .pausado }
    }

    @discardableResult
    func marcarReanudacion(_ id: UUID) throws -> VinculoCapturaAudioIPad {
        try modificar(id) { $0.estado = .capturando }
    }

    @discardableResult
    func marcarConfirmado(_ id: UUID) throws -> VinculoCapturaAudioIPad {
        try modificar(id) {
            $0.estado = .confirmado
            $0.nota = nil
        }
    }

    @discardableResult
    func marcarCancelado(_ id: UUID, motivo: String) throws -> VinculoCapturaAudioIPad {
        try modificar(id) {
            $0.estado = .cancelado
            $0.nota = String(motivo.prefix(1_000))
        }
    }

    @discardableResult
    func marcarRecovery(_ id: UUID, motivo: String) throws -> VinculoCapturaAudioIPad {
        try modificar(id) {
            $0.estado = .requiereRecuperacion
            $0.nota = String(motivo.prefix(1_000))
        }
    }

    /// Resuelve únicamente rutas que fueron creadas dentro del data container.
    /// Si el contenedor cambia entre procesos, se reconstruye desde la raíz actual.
    func fuente(_ vinculo: VinculoCapturaAudioIPad) -> URL? {
        guard let relativa = vinculo.fuenteRelativa,
              Self.relativaValida(relativa) else { return nil }
        let candidata = raizContenedor.appendingPathComponent(relativa)
            .standardizedFileURL
        guard Self.estaDentro(candidata, de: raizContenedor) else { return nil }
        return candidata
    }

    func pendientesRecovery() -> [VinculoCapturaAudioIPad] {
        cola.sync {
            let urls = (try? archivos.contentsOfDirectory(
                at: base,
                includingPropertiesForKeys: [.isRegularFileKey],
                options: [.skipsHiddenFiles])) ?? []
            return urls.compactMap { url -> VinculoCapturaAudioIPad? in
                guard url.pathExtension.lowercased() == "json",
                      let id = UUID(uuidString: url.deletingPathExtension().lastPathComponent),
                      let valor = try? leerSinCola(id),
                      valor.estado == .requiereRecuperacion else { return nil }
                return valor
            }.sorted { $0.creada > $1.creada }
        }
    }

    /// Al iniciar un proceso nuevo, cualquier journal que todavía figure
    /// preparando/capturando/pausado pertenece al proceso anterior.
    @discardableResult
    func marcarInterrumpidosParaRecovery() throws -> [UUID] {
        try cola.sync {
            let urls = try archivos.contentsOfDirectory(
                at: base,
                includingPropertiesForKeys: [.isRegularFileKey],
                options: [.skipsHiddenFiles])
            var ids: [UUID] = []
            for url in urls where url.pathExtension.lowercased() == "json" {
                guard let id = UUID(uuidString:
                    url.deletingPathExtension().lastPathComponent),
                      var valor = try? leerSinCola(id),
                      [.preparando, .capturando, .pausado].contains(valor.estado)
                else { continue }
                valor.estado = .requiereRecuperacion
                valor.nota = "El proceso anterior terminó antes de confirmar el audio final. La fuente se conserva sólo como evidencia pendiente; no se adjunta automáticamente."
                valor.actualizada = Date()
                try guardarSinCola(valor)
                ids.append(id)
            }
            return ids
        }
    }

    private func modificar(_ id: UUID,
                           _ cuerpo: (inout VinculoCapturaAudioIPad) -> Void)
        throws -> VinculoCapturaAudioIPad {
        try cola.sync {
            var valor = try leerSinCola(id)
            cuerpo(&valor)
            valor.actualizada = Date()
            try guardarSinCola(valor)
            return valor
        }
    }

    private func leerSinCola(_ id: UUID) throws -> VinculoCapturaAudioIPad {
        let url = ruta(id)
        guard archivos.fileExists(atPath: url.path) else {
            throw ErrorVinculoCapturaAudioIPad.vinculoNoExiste
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let valor = try decoder.decode(
            VinculoCapturaAudioIPad.self,
            from: Data(contentsOf: url, options: .mappedIfSafe))
        guard valor.version == 1 else {
            throw ErrorVinculoCapturaAudioIPad.versionNoSoportada
        }
        guard valor.sesionAudioId == id else {
            throw ErrorVinculoCapturaAudioIPad.rutaInvalida
        }
        if let relativa = valor.fuenteRelativa,
           !Self.relativaValida(relativa) {
            throw ErrorVinculoCapturaAudioIPad.rutaInvalida
        }
        return valor
    }

    private func guardarSinCola(_ valor: VinculoCapturaAudioIPad) throws {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: ruta(valor.sesionAudioId),
                        options: [.atomic, .completeFileProtection])
    }

    private func ruta(_ id: UUID) -> URL {
        base.appendingPathComponent(id.uuidString.lowercased())
            .appendingPathExtension("json")
    }

    private func rutaRelativaSegura(_ url: URL) throws -> String {
        let absoluta = url.standardizedFileURL
        guard Self.estaDentro(absoluta, de: raizContenedor) else {
            throw ErrorVinculoCapturaAudioIPad.rutaFueraDelContenedor
        }
        let raiz = raizContenedor.path.hasSuffix("/")
            ? raizContenedor.path : raizContenedor.path + "/"
        guard absoluta.path.hasPrefix(raiz) else {
            throw ErrorVinculoCapturaAudioIPad.rutaFueraDelContenedor
        }
        let relativa = String(absoluta.path.dropFirst(raiz.count))
        guard Self.relativaValida(relativa) else {
            throw ErrorVinculoCapturaAudioIPad.rutaInvalida
        }
        return relativa
    }

    private static func relativaValida(_ valor: String) -> Bool {
        guard !valor.isEmpty, valor.count <= 2_048,
              !valor.hasPrefix("/"), !valor.contains("\0") else { return false }
        let componentes = valor.split(separator: "/", omittingEmptySubsequences: false)
        return !componentes.contains(where: { $0.isEmpty || $0 == "." || $0 == ".." })
    }

    private static func estaDentro(_ hija: URL, de raiz: URL) -> Bool {
        let base = raiz.standardizedFileURL.path
        let path = hija.standardizedFileURL.path
        if path == base { return true }
        let prefijo = base.hasSuffix("/") ? base : base + "/"
        return path.hasPrefix(prefijo)
    }
}

/// Resuelve el archivo de audio final de un paquete Nota Viva sin inventar un
/// candidato. El manifest final es la única autoridad; durante recovery sólo se
/// usa esta ruta cuando `archivoAudio` quedó confirmado.
enum ResolutorAudioFinalNotaVivaIPad {
    static func resolver(manifiestoURL: URL) throws -> URL? {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let datos = try Data(contentsOf: manifiestoURL, options: .mappedIfSafe)
        let manifiesto: ManifiestoNotaViva
        do {
            manifiesto = try decoder.decode(ManifiestoNotaViva.self, from: datos)
        } catch {
            throw ErrorVinculoCapturaAudioIPad.manifiestoNotaVivaInvalido
        }
        guard let nombre = manifiesto.archivoAudio else { return nil }
        guard !nombre.isEmpty,
              (nombre as NSString).lastPathComponent == nombre,
              nombre != ".", nombre != ".." else {
            throw ErrorVinculoCapturaAudioIPad.manifiestoNotaVivaInvalido
        }
        let audio = manifiestoURL.deletingLastPathComponent()
            .appendingPathComponent(nombre)
            .standardizedFileURL
        let carpeta = manifiestoURL.deletingLastPathComponent().standardizedFileURL
        let prefijo = carpeta.path.hasSuffix("/") ? carpeta.path : carpeta.path + "/"
        guard audio.path.hasPrefix(prefijo) else {
            throw ErrorVinculoCapturaAudioIPad.manifiestoNotaVivaInvalido
        }
        return audio
    }
}
