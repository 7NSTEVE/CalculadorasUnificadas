import Foundation

struct RegionNormalizadaComentarioIPad: Codable, Equatable, Hashable {
    var x: Double
    var y: Double
    var ancho: Double
    var alto: Double

    var valida: Bool {
        [x, y, ancho, alto].allSatisfy(\.isFinite)
            && x >= 0 && y >= 0 && ancho >= 0 && alto >= 0
            && x <= 1 && y <= 1 && x + ancho <= 1.000_001 && y + alto <= 1.000_001
    }
}

struct AnclajeComentarioDocumentoIPad: Codable, Equatable, Hashable {
    var documentoId: DocumentoPersistenteID
    var pagina: Int?
    var frameId: UUID?
    var elementoId: String?
    var regionNormalizada: RegionNormalizadaComentarioIPad?
    var segundoAudio: Double?
    var contexto: String?
}

struct MensajeComentarioIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var autorId: String
    var autorNombre: String
    var texto: String
    var creado: Date
    var editado: Date?

    init(
        id: UUID = UUID(), autorId: String, autorNombre: String,
        texto: String, creado: Date = Date(), editado: Date? = nil
    ) {
        self.id = id
        self.autorId = autorId
        self.autorNombre = autorNombre
        self.texto = texto
        self.creado = creado
        self.editado = editado
    }
}

struct HiloComentarioDocumentoIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var anclaje: AnclajeComentarioDocumentoIPad
    var mensajes: [MensajeComentarioIPad]
    var resuelto: Bool
    var creado: Date
    var actualizado: Date

    init(
        id: UUID = UUID(), anclaje: AnclajeComentarioDocumentoIPad,
        mensajes: [MensajeComentarioIPad], resuelto: Bool = false,
        creado: Date = Date(), actualizado: Date = Date()
    ) {
        self.id = id
        self.anclaje = anclaje
        self.mensajes = mensajes
        self.resuelto = resuelto
        self.creado = creado
        self.actualizado = actualizado
    }

    var ultimoMensaje: MensajeComentarioIPad? { mensajes.max { $0.creado < $1.creado } }
}

private struct ArchivoComentariosDocumentoIPad: Codable {
    var version = 1
    var documentoId: DocumentoPersistenteID
    var hilos: [HiloComentarioDocumentoIPad] = []
}

enum ErrorComentariosDocumentoIPad: LocalizedError {
    case textoVacio
    case hiloInexistente
    case mensajeInexistente
    case anclajeInvalido
    case documentoDistinto

    var errorDescription: String? {
        switch self {
        case .textoVacio: return "El comentario no puede estar vacío."
        case .hiloInexistente: return "El hilo de comentarios ya no existe."
        case .mensajeInexistente: return "El mensaje ya no existe."
        case .anclajeInvalido: return "El anclaje del comentario no es válido."
        case .documentoDistinto: return "El comentario pertenece a otro documento."
        }
    }
}

final class RepositorioComentariosDocumentoIPad {
    static let compartido = RepositorioComentariosDocumentoIPad()

    private let base: URL
    private let archivos: FileManager
    private let bloqueo = NSRecursiveLock()
    private var cache: [DocumentoPersistenteID: ArchivoComentariosDocumentoIPad] = [:]

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        self.base = base ?? archivos.urls(for: .applicationSupportDirectory,
                                           in: .userDomainMask)[0]
            .appendingPathComponent("ComentariosDocumento", isDirectory: true)
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    func hilos(documentoId: DocumentoPersistenteID,
               incluirResueltos: Bool = true) -> [HiloComentarioDocumentoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return cargar(documentoId).hilos.filter { incluirResueltos || !$0.resuelto }
            .sorted { $0.actualizado > $1.actualizado }
    }

    @discardableResult
    func crearHilo(
        anclaje: AnclajeComentarioDocumentoIPad,
        autorId: String,
        autorNombre: String,
        texto: String
    ) throws -> HiloComentarioDocumentoIPad {
        try Self.validar(anclaje)
        let texto = Self.limpiar(texto)
        guard !texto.isEmpty else { throw ErrorComentariosDocumentoIPad.textoVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargar(anclaje.documentoId)
        let ahora = Date()
        let hilo = HiloComentarioDocumentoIPad(
            anclaje: anclaje,
            mensajes: [.init(autorId: autorId, autorNombre: autorNombre,
                             texto: texto, creado: ahora)],
            creado: ahora, actualizado: ahora)
        archivo.hilos.append(hilo)
        try persistir(archivo)
        cache[anclaje.documentoId] = archivo
        return hilo
    }

    @discardableResult
    func responder(
        hiloId: UUID,
        documentoId: DocumentoPersistenteID,
        autorId: String,
        autorNombre: String,
        texto: String
    ) throws -> MensajeComentarioIPad {
        let texto = Self.limpiar(texto)
        guard !texto.isEmpty else { throw ErrorComentariosDocumentoIPad.textoVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargar(documentoId)
        guard let i = archivo.hilos.firstIndex(where: { $0.id == hiloId }) else {
            throw ErrorComentariosDocumentoIPad.hiloInexistente
        }
        let mensaje = MensajeComentarioIPad(
            autorId: autorId, autorNombre: autorNombre, texto: texto)
        archivo.hilos[i].mensajes.append(mensaje)
        archivo.hilos[i].actualizado = mensaje.creado
        try persistir(archivo); cache[documentoId] = archivo
        return mensaje
    }

    func editarMensaje(
        mensajeId: UUID,
        hiloId: UUID,
        documentoId: DocumentoPersistenteID,
        texto: String
    ) throws {
        let texto = Self.limpiar(texto)
        guard !texto.isEmpty else { throw ErrorComentariosDocumentoIPad.textoVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargar(documentoId)
        guard let h = archivo.hilos.firstIndex(where: { $0.id == hiloId }) else {
            throw ErrorComentariosDocumentoIPad.hiloInexistente
        }
        guard let m = archivo.hilos[h].mensajes.firstIndex(where: { $0.id == mensajeId }) else {
            throw ErrorComentariosDocumentoIPad.mensajeInexistente
        }
        let ahora = Date()
        archivo.hilos[h].mensajes[m].texto = texto
        archivo.hilos[h].mensajes[m].editado = ahora
        archivo.hilos[h].actualizado = ahora
        try persistir(archivo); cache[documentoId] = archivo
    }

    func resolver(hiloId: UUID, documentoId: DocumentoPersistenteID, valor: Bool) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargar(documentoId)
        guard let i = archivo.hilos.firstIndex(where: { $0.id == hiloId }) else {
            throw ErrorComentariosDocumentoIPad.hiloInexistente
        }
        archivo.hilos[i].resuelto = valor
        archivo.hilos[i].actualizado = Date()
        try persistir(archivo); cache[documentoId] = archivo
    }

    func eliminarHilo(hiloId: UUID, documentoId: DocumentoPersistenteID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var archivo = cargar(documentoId)
        let anterior = archivo.hilos.count
        archivo.hilos.removeAll { $0.id == hiloId }
        guard archivo.hilos.count != anterior else {
            throw ErrorComentariosDocumentoIPad.hiloInexistente
        }
        try persistir(archivo); cache[documentoId] = archivo
    }

    func buscar(_ consulta: String, documentoId: DocumentoPersistenteID? = nil,
                incluirResueltos: Bool = false) -> [HiloComentarioDocumentoIPad] {
        let terminos = Normalizador.tokens(consulta)
        bloqueo.lock(); defer { bloqueo.unlock() }
        let archivosCandidatos: [ArchivoComentariosDocumentoIPad]
        if let documentoId { archivosCandidatos = [cargar(documentoId)] }
        else {
            cargarTodosEnCache()
            archivosCandidatos = Array(cache.values)
        }
        return archivosCandidatos.flatMap(\.hilos).filter { hilo in
            guard incluirResueltos || !hilo.resuelto else { return false }
            if terminos.isEmpty { return true }
            let texto = hilo.mensajes.map { $0.texto + " " + $0.autorNombre }
                .joined(separator: " ") + " " + (hilo.anclaje.contexto ?? "")
            let tokens = Normalizador.tokens(texto)
            return terminos.allSatisfy(tokens.contains)
        }.sorted { $0.actualizado > $1.actualizado }
    }

    private func cargar(_ documentoId: DocumentoPersistenteID) -> ArchivoComentariosDocumentoIPad {
        if let cached = cache[documentoId] { return cached }
        let url = urlArchivo(documentoId)
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        let archivo: ArchivoComentariosDocumentoIPad
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(ArchivoComentariosDocumentoIPad.self, from: datos),
           leido.documentoId == documentoId {
            archivo = leido
        } else {
            archivo = ArchivoComentariosDocumentoIPad(documentoId: documentoId)
        }
        cache[documentoId] = archivo
        return archivo
    }

    private func cargarTodosEnCache() {
        guard let urls = try? archivos.contentsOfDirectory(
            at: base, includingPropertiesForKeys: nil) else { return }
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        for url in urls where url.pathExtension == "json" {
            guard let datos = try? Data(contentsOf: url),
                  let archivo = try? decoder.decode(ArchivoComentariosDocumentoIPad.self, from: datos)
            else { continue }
            cache[archivo.documentoId] = archivo
        }
    }

    private func persistir(_ archivo: ArchivoComentariosDocumentoIPad) throws {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let datos = try encoder.encode(archivo)
        try datos.write(to: urlArchivo(archivo.documentoId),
                        options: [.atomic, .completeFileProtection])
    }

    private func urlArchivo(_ documentoId: DocumentoPersistenteID) -> URL {
        base.appendingPathComponent("doc-\(documentoId.description).json")
    }

    private static func validar(_ anclaje: AnclajeComentarioDocumentoIPad) throws {
        if let pagina = anclaje.pagina, pagina < 0 { throw ErrorComentariosDocumentoIPad.anclajeInvalido }
        if let segundo = anclaje.segundoAudio, (!segundo.isFinite || segundo < 0) {
            throw ErrorComentariosDocumentoIPad.anclajeInvalido
        }
        if let region = anclaje.regionNormalizada, !region.valida {
            throw ErrorComentariosDocumentoIPad.anclajeInvalido
        }
    }

    private static func limpiar(_ texto: String) -> String {
        texto.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
