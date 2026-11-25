import CryptoKit
import Foundation

enum EstadoSesionAudioResilienteIPad: String, Codable, Equatable {
    case grabando
    case pausada
    case finalizada
    case requiereRecuperacion
}

struct SegmentoAudioResilienteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var indice: Int
    var archivo: String
    var sha256: String
    var bytes: Int64
    var inicioSegundos: Double
    var duracionSegundos: Double
    var codec: String
    var sampleRate: Double?
    var canales: Int?
    var creado = Date()

    var finSegundos: Double { inicioSegundos + duracionSegundos }
}

struct ManifiestoAudioResilienteIPad: Codable, Identifiable, Equatable {
    var version = 1
    var id = UUID()
    var documentoId: UInt64
    var estado: EstadoSesionAudioResilienteIPad = .grabando
    var creada = Date()
    var actualizada = Date()
    var segmentos: [SegmentoAudioResilienteIPad] = []
    var notaRecovery: String?

    var duracionConfirmada: Double {
        segmentos.map(\.finSegundos).max() ?? 0
    }
}

struct DiagnosticoSesionAudioResilienteIPad: Equatable {
    var manifiesto: ManifiestoAudioResilienteIPad
    var segmentosFaltantes: [String]
    var segmentosAlterados: [String]
    var archivosHuerfanos: [String]

    var integro: Bool {
        segmentosFaltantes.isEmpty && segmentosAlterados.isEmpty
    }
}

enum ErrorAudioResilienteIPad: LocalizedError, Equatable {
    case sesionNoExiste
    case manifiestoDanado
    case sesionFinalizada
    case archivoInvalido
    case archivoDemasiadoGrande
    case segmentoDuplicado
    case duracionInvalida
    case formatoInvalido
    case sesionInconsistente

    var errorDescription: String? {
        switch self {
        case .sesionNoExiste: return "La sesión de audio no existe."
        case .manifiestoDanado: return "El manifiesto de audio está dañado; se conservaron sus archivos."
        case .sesionFinalizada: return "La sesión de audio ya fue finalizada."
        case .archivoInvalido: return "El segmento de audio está vacío o no se puede leer."
        case .archivoDemasiadoGrande: return "El segmento supera el límite seguro de 1 GB."
        case .segmentoDuplicado: return "Ese segmento ya fue confirmado en la sesión."
        case .duracionInvalida: return "La duración o el tiempo inicial del segmento no son válidos."
        case .formatoInvalido: return "El codec o la extensión del segmento no son válidos."
        case .sesionInconsistente: return "La sesión contiene archivos faltantes o alterados y no puede finalizarse silenciosamente."
        }
    }
}

/// Sidecar durable para grabaciones largas. El capturador de audio puede rotar
/// archivos y confirmar cada chunk aquí sin depender de un único archivo que se
/// corrompa por completo ante terminación abrupta.
final class RepositorioAudioResilienteIPad {
    static let compartido = RepositorioAudioResilienteIPad()

    private static let limiteSegmentoBytes: Int64 = 1_073_741_824
    private static let nombreManifiesto = "manifest.json"
    private static let prefijoSegmento = "segmento-"

    private let archivos: FileManager
    private let base: URL
    private let cola = DispatchQueue(label: "cuadernu.audio.resiliente")

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        if let base {
            self.base = base
        } else {
            let soporte = archivos.urls(
                for: .applicationSupportDirectory,
                in: .userDomainMask)[0]
            self.base = soporte.appendingPathComponent(
                "Tinta/AudioResiliente", isDirectory: true)
        }
        try? archivos.createDirectory(
            at: self.base, withIntermediateDirectories: true)
    }

    @discardableResult
    func crearSesion(documentoId: UInt64) throws -> ManifiestoAudioResilienteIPad {
        try cola.sync {
            let manifiesto = ManifiestoAudioResilienteIPad(documentoId: documentoId)
            let carpeta = rutaSesion(manifiesto.id)
            guard !archivos.fileExists(atPath: carpeta.path) else {
                throw ErrorAudioResilienteIPad.segmentoDuplicado
            }
            try archivos.createDirectory(at: carpeta, withIntermediateDirectories: false)
            do {
                try guardar(manifiesto, en: carpeta)
            } catch {
                try? archivos.removeItem(at: carpeta)
                throw error
            }
            return manifiesto
        }
    }

    func leer(_ id: UUID) throws -> ManifiestoAudioResilienteIPad {
        try cola.sync { try leerSinCola(id) }
    }

    @discardableResult
    func confirmarSegmento(
        sesion id: UUID,
        archivoTemporal: URL,
        inicioSegundos: Double,
        duracionSegundos: Double,
        codec: String,
        extensionArchivo: String,
        sampleRate: Double? = nil,
        canales: Int? = nil
    ) throws -> SegmentoAudioResilienteIPad {
        try cola.sync {
            guard inicioSegundos.isFinite, inicioSegundos >= 0,
                  duracionSegundos.isFinite, duracionSegundos > 0 else {
                throw ErrorAudioResilienteIPad.duracionInvalida
            }
            let codecLimpio = codec.trimmingCharacters(in: .whitespacesAndNewlines)
            let ext = extensionArchivo.lowercased()
                .trimmingCharacters(in: CharacterSet(charactersIn: ". "))
            guard textoSeguro(codecLimpio, maximo: 64),
                  textoSeguro(ext, maximo: 12),
                  ext.allSatisfy({ $0.isLetter || $0.isNumber }) else {
                throw ErrorAudioResilienteIPad.formatoInvalido
            }
            if let sampleRate {
                guard sampleRate.isFinite, sampleRate >= 8_000,
                      sampleRate <= 384_000 else {
                    throw ErrorAudioResilienteIPad.formatoInvalido
                }
            }
            if let canales, !(1...16).contains(canales) {
                throw ErrorAudioResilienteIPad.formatoInvalido
            }

            var manifiesto = try leerSinCola(id)
            guard manifiesto.estado != .finalizada else {
                throw ErrorAudioResilienteIPad.sesionFinalizada
            }
            let valores = try archivoTemporal.resourceValues(
                forKeys: [.isRegularFileKey, .fileSizeKey])
            guard valores.isRegularFile == true,
                  let tamano = valores.fileSize, tamano > 0 else {
                throw ErrorAudioResilienteIPad.archivoInvalido
            }
            guard Int64(tamano) <= Self.limiteSegmentoBytes else {
                throw ErrorAudioResilienteIPad.archivoDemasiadoGrande
            }

            let sha = try sha256(archivoTemporal)
            guard !manifiesto.segmentos.contains(where: { $0.sha256 == sha }) else {
                throw ErrorAudioResilienteIPad.segmentoDuplicado
            }
            let indice = (manifiesto.segmentos.map(\.indice).max() ?? -1) + 1
            let nombre = String(format: "%@%06d.%@",
                                Self.prefijoSegmento, indice, ext)
            let carpeta = rutaSesion(id)
            let destino = carpeta.appendingPathComponent(nombre)
            guard !archivos.fileExists(atPath: destino.path) else {
                throw ErrorAudioResilienteIPad.segmentoDuplicado
            }

            // Primero se publica el archivo durable. Si el manifest write falla,
            // queda un huérfano detectable y preservado por recovery; nunca una
            // entrada de manifiesto apuntando a bytes que todavía no existen.
            try archivos.copyItem(at: archivoTemporal, to: destino)
            do {
                let segmento = SegmentoAudioResilienteIPad(
                    indice: indice,
                    archivo: nombre,
                    sha256: sha,
                    bytes: Int64(tamano),
                    inicioSegundos: inicioSegundos,
                    duracionSegundos: duracionSegundos,
                    codec: codecLimpio,
                    sampleRate: sampleRate,
                    canales: canales)
                manifiesto.segmentos.append(segmento)
                manifiesto.estado = .grabando
                manifiesto.actualizada = Date()
                try validar(manifiesto)
                try guardar(manifiesto, en: carpeta)
                return segmento
            } catch {
                // No se borra el archivo copiado: si el proceso cae o el disco
                // falla justo aquí, recovery lo identifica como huérfano y lo
                // preserva para diagnóstico/reensamblado manual.
                throw error
            }
        }
    }

    func pausar(_ id: UUID) throws {
        try cambiarEstado(id, a: .pausada)
    }

    func reanudar(_ id: UUID) throws {
        try cambiarEstado(id, a: .grabando)
    }

    func finalizar(_ id: UUID) throws {
        try cola.sync {
            var manifiesto = try leerSinCola(id)
            guard manifiesto.estado != .finalizada else { return }
            let diagnostico = try diagnosticarSinCola(manifiesto)
            guard diagnostico.integro, diagnostico.archivosHuerfanos.isEmpty else {
                throw ErrorAudioResilienteIPad.sesionInconsistente
            }
            manifiesto.estado = .finalizada
            manifiesto.actualizada = Date()
            try guardar(manifiesto, en: rutaSesion(id))
        }
    }

    /// Al arrancar un proceso nuevo, ninguna sesión marcada todavía como
    /// `grabando` puede seguir teniendo un capturador vivo del proceso anterior.
    /// Se marca recovery; no se adjuntan ni eliminan huérfanos automáticamente.
    @discardableResult
    func marcarInterrumpidasParaRecovery() throws -> [UUID] {
        try cola.sync {
            var afectadas: [UUID] = []
            for carpeta in try carpetasSesion() {
                let idTexto = carpeta.lastPathComponent
                guard let id = UUID(uuidString: idTexto) else { continue }
                let manifiesto: ManifiestoAudioResilienteIPad
                do { manifiesto = try leerSinCola(id) }
                catch { continue }
                guard manifiesto.estado == .grabando else { continue }
                var actualizado = manifiesto
                actualizado.estado = .requiereRecuperacion
                actualizado.notaRecovery =
                    "La aplicación terminó mientras esta sesión figuraba como grabando. Los segmentos confirmados se conservaron y los huérfanos no se adjuntaron automáticamente."
                actualizado.actualizada = Date()
                try guardar(actualizado, en: carpeta)
                afectadas.append(id)
            }
            return afectadas
        }
    }

    func diagnosticar(_ id: UUID) throws -> DiagnosticoSesionAudioResilienteIPad {
        try cola.sync {
            let manifiesto = try leerSinCola(id)
            return try diagnosticarSinCola(manifiesto)
        }
    }

    func sesiones(documentoId: UInt64? = nil) -> [ManifiestoAudioResilienteIPad] {
        cola.sync {
            let resultados = (try? carpetasSesion())?.compactMap { carpeta in
                UUID(uuidString: carpeta.lastPathComponent)
                    .flatMap { try? leerSinCola($0) }
            } ?? []
            return resultados.filter {
                documentoId.map { $0 == $0 } == nil
                    || documentoId == $0.documentoId
            }.sorted { $0.creada > $1.creada }
        }
    }

    private func cambiarEstado(
        _ id: UUID,
        a estado: EstadoSesionAudioResilienteIPad
    ) throws {
        try cola.sync {
            var manifiesto = try leerSinCola(id)
            guard manifiesto.estado != .finalizada else {
                throw ErrorAudioResilienteIPad.sesionFinalizada
            }
            manifiesto.estado = estado
            manifiesto.actualizada = Date()
            try guardar(manifiesto, en: rutaSesion(id))
        }
    }

    private func diagnosticarSinCola(
        _ manifiesto: ManifiestoAudioResilienteIPad
    ) throws -> DiagnosticoSesionAudioResilienteIPad {
        let carpeta = rutaSesion(manifiesto.id)
        let declarados = Set(manifiesto.segmentos.map(\.archivo))
        var faltantes: [String] = []
        var alterados: [String] = []
        for segmento in manifiesto.segmentos {
            let url = carpeta.appendingPathComponent(segmento.archivo)
            guard archivos.fileExists(atPath: url.path) else {
                faltantes.append(segmento.archivo)
                continue
            }
            let valores = try url.resourceValues(forKeys: [.fileSizeKey])
            let hashActual = try sha256(url)
            if Int64(valores.fileSize ?? -1) != segmento.bytes
                || hashActual != segmento.sha256 {
                alterados.append(segmento.archivo)
            }
        }
        let huerfanos = try archivos.contentsOfDirectory(
            at: carpeta,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles])
            .filter {
                $0.lastPathComponent.hasPrefix(Self.prefijoSegmento)
                    && !declarados.contains($0.lastPathComponent)
            }
            .map(\.lastPathComponent)
            .sorted()
        return DiagnosticoSesionAudioResilienteIPad(
            manifiesto: manifiesto,
            segmentosFaltantes: faltantes.sorted(),
            segmentosAlterados: alterados.sorted(),
            archivosHuerfanos: huerfanos)
    }

    private func leerSinCola(_ id: UUID) throws -> ManifiestoAudioResilienteIPad {
        let carpeta = rutaSesion(id)
        guard archivos.fileExists(atPath: carpeta.path) else {
            throw ErrorAudioResilienteIPad.sesionNoExiste
        }
        let url = carpeta.appendingPathComponent(Self.nombreManifiesto)
        do {
            let datos = try Data(contentsOf: url, options: .mappedIfSafe)
            let decoder = JSONDecoder()
            decoder.dateDecodingStrategy = .iso8601
            let manifiesto = try decoder.decode(
                ManifiestoAudioResilienteIPad.self, from: datos)
            try validar(manifiesto)
            guard manifiesto.id == id else {
                throw ErrorAudioResilienteIPad.manifiestoDanado
            }
            return manifiesto
        } catch let error as ErrorAudioResilienteIPad {
            throw error
        } catch {
            throw ErrorAudioResilienteIPad.manifiestoDanado
        }
    }

    private func validar(_ manifiesto: ManifiestoAudioResilienteIPad) throws {
        guard manifiesto.version == 1,
              manifiesto.segmentos.count <= 100_000,
              Set(manifiesto.segmentos.map(\.id)).count == manifiesto.segmentos.count,
              Set(manifiesto.segmentos.map(\.indice)).count == manifiesto.segmentos.count,
              Set(manifiesto.segmentos.map(\.archivo)).count == manifiesto.segmentos.count
        else { throw ErrorAudioResilienteIPad.manifiestoDanado }
        for segmento in manifiesto.segmentos {
            guard segmento.indice >= 0,
                  nombreSeguro(segmento.archivo),
                  segmento.archivo.hasPrefix(Self.prefijoSegmento),
                  segmento.sha256.count == 64,
                  segmento.bytes > 0,
                  segmento.inicioSegundos.isFinite,
                  segmento.inicioSegundos >= 0,
                  segmento.duracionSegundos.isFinite,
                  segmento.duracionSegundos > 0,
                  textoSeguro(segmento.codec, maximo: 64)
            else { throw ErrorAudioResilienteIPad.manifiestoDanado }
        }
    }

    private func guardar(
        _ manifiesto: ManifiestoAudioResilienteIPad,
        en carpeta: URL
    ) throws {
        try validar(manifiesto)
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let datos = try encoder.encode(manifiesto)
        try datos.write(
            to: carpeta.appendingPathComponent(Self.nombreManifiesto),
            options: [.atomic, .completeFileProtection])
    }

    private func rutaSesion(_ id: UUID) -> URL {
        base.appendingPathComponent(id.uuidString, isDirectory: true)
    }

    private func carpetasSesion() throws -> [URL] {
        try archivos.contentsOfDirectory(
            at: base,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles])
            .filter { (try? $0.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true }
    }

    private func sha256(_ url: URL) throws -> String {
        let manejador = try FileHandle(forReadingFrom: url)
        defer { try? manejador.close() }
        var hasher = SHA256()
        while true {
            let datos = try manejador.read(upToCount: 1_048_576) ?? Data()
            if datos.isEmpty { break }
            hasher.update(data: datos)
        }
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }

    private func nombreSeguro(_ nombre: String) -> Bool {
        !nombre.isEmpty
            && nombre.count <= 180
            && (nombre as NSString).lastPathComponent == nombre
            && nombre != "." && nombre != ".."
    }

    private func textoSeguro(_ texto: String, maximo: Int) -> Bool {
        !texto.isEmpty && texto.count <= maximo
            && !texto.unicodeScalars.contains(where: { $0.value == 0 })
    }
}
