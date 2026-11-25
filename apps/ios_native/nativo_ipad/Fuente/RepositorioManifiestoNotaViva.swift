import Foundation

/// Único punto de lectura-modificación-escritura del manifiesto. La
/// transcripción termina en segundo plano y puede coincidir con mejoras de
/// audio, vídeo vinculado o marcadores; sin esta exclusión mutua, la última
/// escritura podía borrar silenciosamente campos guardados por la anterior.
enum RepositorioManifiestoNotaViva {
    private static let bloqueo = NSRecursiveLock()
    private static let nombreRespaldo = "manifest.previous.json"
    private static let nombreCheckpoint = "manifest.checkpoint.json"
    private static let avisoRecuperacion =
        "El manifiesto principal de esta Nota Viva no era legible. Se recuperó la última versión válida disponible; algunos metadatos recientes podrían no estar presentes."
    private static let avisoCheckpoint =
        "La Nota Viva no alcanzó a publicar su manifiesto final. Se abrió un checkpoint de recuperación; la copia final de tinta se conservará si llegó a guardarse."

    static func cargar(_ url: URL) throws -> ManifiestoNotaViva {
        bloqueo.lock()
        defer { bloqueo.unlock() }
        return try cargarSinBloqueo(url)
    }

    @discardableResult
    static func guardar(
        _ manifiesto: ManifiestoNotaViva,
        en url: URL
    ) throws -> ManifiestoNotaViva {
        bloqueo.lock()
        defer { bloqueo.unlock() }
        try escribirSinBloqueo(manifiesto, en: url)
        return manifiesto
    }

    /// Checkpoint mínimo y separado del manifiesto canónico. Se crea al
    /// comenzar una Nota Viva y solo sirve como ruta de rescate si el cierre
    /// alcanza a persistir `nota.tinta` pero falla antes de publicar
    /// `manifest.json`. Una publicación canónica correcta lo elimina.
    static func guardarCheckpoint(
        _ manifiesto: ManifiestoNotaViva,
        en urlCanonica: URL
    ) throws {
        bloqueo.lock()
        defer { bloqueo.unlock() }
        let datos = try codificar(manifiesto)
        try datos.write(to: urlCheckpoint(para: urlCanonica),
                        options: [.atomic, .completeFileProtection])
    }

    @discardableResult
    static func actualizar(
        _ url: URL,
        cambio: (inout ManifiestoNotaViva) throws -> Void
    ) throws -> ManifiestoNotaViva {
        bloqueo.lock()
        defer { bloqueo.unlock() }
        var manifiesto = try cargarSinBloqueo(url)
        try cambio(&manifiesto)
        try escribirSinBloqueo(manifiesto, en: url)
        return manifiesto
    }

    private static func cargarSinBloqueo(_ url: URL) throws -> ManifiestoNotaViva {
        do {
            return try decodificar(Data(contentsOf: url))
        } catch {
            let falloPrincipal = error
            let candidatos = [
                (url.deletingLastPathComponent()
                    .appendingPathComponent(nombreRespaldo), avisoRecuperacion),
                (urlCheckpoint(para: url), avisoCheckpoint)
            ]
            for (candidato, aviso) in candidatos
                where FileManager.default.fileExists(atPath: candidato.path) {
                guard let datos = try? Data(contentsOf: candidato),
                      var recuperado = try? decodificar(datos) else {
                    continue
                }
                if recuperado.advertencias?.contains(aviso) != true {
                    recuperado.advertencias = (recuperado.advertencias ?? [])
                        + [aviso]
                }
                return recuperado
            }
            throw falloPrincipal
        }
    }

    private static func urlCheckpoint(para urlCanonica: URL) -> URL {
        urlCanonica.deletingLastPathComponent()
            .appendingPathComponent(nombreCheckpoint)
    }

    private static func decodificar(_ datos: Data) throws -> ManifiestoNotaViva {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return try decoder.decode(ManifiestoNotaViva.self, from: datos)
    }

    private static func codificar(_ manifiesto: ManifiestoNotaViva) throws -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        return try encoder.encode(manifiesto)
    }

    private static func escribirSinBloqueo(
        _ manifiesto: ManifiestoNotaViva,
        en url: URL
    ) throws {
        let datos = try codificar(manifiesto)

        // Antes de reemplazar un manifiesto válido conservamos su última
        // versión legible. Si una escritura posterior o una corrupción deja el
        // JSON principal inutilizable, la Nota Viva aún puede abrir la copia
        // anterior sin tocar la tinta, el audio ni la cronología del paquete.
        if let anterior = try? Data(contentsOf: url),
           (try? decodificar(anterior)) != nil {
            let respaldo = url.deletingLastPathComponent()
                .appendingPathComponent(nombreRespaldo)
            try? anterior.write(to: respaldo,
                                options: [.atomic, .completeFileProtection])
        }

        try datos.write(to: url, options: [.atomic, .completeFileProtection])

        // Solo después de publicar correctamente el canónico deja de hacer
        // falta el checkpoint. Fallar al limpiarlo no invalida el manifiesto:
        // en una carga normal siempre gana `manifest.json`.
        try? FileManager.default.removeItem(at: urlCheckpoint(para: url))
    }
}