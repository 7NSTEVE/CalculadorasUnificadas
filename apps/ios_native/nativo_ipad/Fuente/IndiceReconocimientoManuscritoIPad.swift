import CryptoKit
import Foundation
import Vision

struct RegistroOCRManuscritoIPad: Codable, Equatable, Identifiable {
    var id: String { fuenteId }
    var fuenteId: String
    var documentoId: UInt64
    var idsTrazos: [String]
    var marco: CGRect
    /// Vacío significa "Vision evaluó esta fuente y no encontró texto". Se
    /// conserva como caché negativa reconstruible para no reintentar en bucle.
    var texto: String
    var confianza: Float
    var idiomas: [String]
    /// Opcional para compatibilidad con sidecars v1 anteriores. Si falta o no
    /// coincide con el motor actual, la región se vuelve a reconocer.
    var firmaMotor: String? = nil
    var fecha = Date()
}

struct ResultadoBusquedaManuscritaIPad: Equatable, Identifiable {
    var id: String {
        "\(documentoId)|\(marco.minX)|\(marco.minY)|\(fragmento)"
    }
    var documentoId: UInt64
    var marco: CGRect
    var puntuacion: Float
    var fragmento: String
}

private struct ArchivoOCRManuscritoIPad: Codable {
    var version = 1
    var registros: [RegistroOCRManuscritoIPad] = []
}

enum ErrorIndiceOCRManuscritoIPad: LocalizedError {
    case demasiadosRegistros
    case registroInvalido(String)
    case fuenteDuplicada(String)
    case persistencia(String)

    var errorDescription: String? {
        switch self {
        case .demasiadosRegistros:
            return "El índice manuscrito supera el límite seguro de registros."
        case .registroInvalido(let id):
            return "El registro OCR manuscrito no es válido: \(id)."
        case .fuenteDuplicada(let id):
            return "El índice manuscrito contiene una fuente duplicada: \(id)."
        case .persistencia(let detalle):
            return "No se pudo persistir el índice manuscrito: \(detalle)"
        }
    }
}

/// Sidecar reconstruible. Su pérdida o corrupción jamás impide abrir el
/// documento; a lo sumo obliga a volver a ejecutar Vision sobre la tinta.
final class RepositorioOCRManuscritoIPad {
    static let compartido = RepositorioOCRManuscritoIPad()
    static let maximoRegistros = 100_000

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var registros: [RegistroOCRManuscritoIPad]
    private var buscadores: [UInt64: OpaquePointer] = [:]

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(
            for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Tinta/OCRManuscrito", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("indice-v1.json")

        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url, options: .mappedIfSafe),
           let archivo = try? decoder.decode(ArchivoOCRManuscritoIPad.self,
                                             from: datos),
           archivo.version == 1,
           (try? Self.validar(archivo.registros)) != nil {
            registros = archivo.registros
        } else {
            registros = []
        }
        reconstruirTodosSinBloqueo()
    }

    deinit {
        for buscador in buscadores.values {
            tinta_buscador_liberar(buscador)
        }
    }

    func registros(documentoId: UInt64? = nil) -> [RegistroOCRManuscritoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let valores = documentoId.map { id in
            registros.filter { $0.documentoId == id }
        } ?? registros
        return valores.sorted { ($0.documentoId, $0.marco.minY, $0.marco.minX)
            < ($1.documentoId, $1.marco.minY, $1.marco.minX) }
    }

    /// Reemplaza atómicamente TODO el conjunto derivado de un documento. Si
    /// Vision se cancela a mitad, el caller no invoca esta función y el índice
    /// anterior sigue disponible.
    func reemplazarDocumento(
        _ documentoId: UInt64,
        con nuevos: [RegistroOCRManuscritoIPad]
    ) throws {
        guard nuevos.allSatisfy({ $0.documentoId == documentoId }) else {
            throw ErrorIndiceOCRManuscritoIPad.registroInvalido("documento")
        }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = registros.filter { $0.documentoId != documentoId }
        candidato.append(contentsOf: nuevos)
        try Self.validar(candidato)
        try persistir(candidato)
        registros = candidato
        reconstruirDocumentoSinBloqueo(documentoId)
    }

    func eliminarDocumento(_ documentoId: UInt64) throws {
        try reemplazarDocumento(documentoId, con: [])
    }

    func buscar(_ consulta: String,
                limite: Int = 100) -> [ResultadoBusquedaManuscritaIPad] {
        let consulta = consulta.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !consulta.isEmpty else { return [] }
        bloqueo.lock(); defer { bloqueo.unlock() }
        var salida: [ResultadoBusquedaManuscritaIPad] = []
        for (documento, buscador) in buscadores {
            let cantidad: Int = consulta.withCString {
                Int(tinta_buscador_buscar(buscador, $0))
            }
            for indice in 0..<cantidad {
                var resultado = TintaResultadoBusquedaC()
                guard tinta_buscador_resultado(
                    buscador, indice, &resultado) else { continue }
                let fragmento = resultado.fragmento.map {
                    String(cString: $0)
                } ?? ""
                salida.append(.init(
                    documentoId: documento,
                    marco: CGRect(
                        x: CGFloat(resultado.x), y: CGFloat(resultado.y),
                        width: CGFloat(resultado.ancho),
                        height: CGFloat(resultado.alto)),
                    puntuacion: resultado.puntuacion,
                    fragmento: fragmento))
            }
        }
        salida.sort {
            if $0.puntuacion != $1.puntuacion {
                return $0.puntuacion > $1.puntuacion
            }
            return ($0.documentoId, $0.marco.minY, $0.marco.minX)
                < ($1.documentoId, $1.marco.minY, $1.marco.minX)
        }
        return Array(salida.prefix(max(1, min(limite, 1_000))))
    }

    private func persistir(_ valores: [RegistroOCRManuscritoIPad]) throws {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        do {
            let datos = try encoder.encode(
                ArchivoOCRManuscritoIPad(registros: valores))
            try datos.write(to: url,
                            options: [.atomic, .completeFileProtection])
        } catch {
            throw ErrorIndiceOCRManuscritoIPad.persistencia(
                error.localizedDescription)
        }
    }

    private func reconstruirTodosSinBloqueo() {
        let documentos = Set(registros.map(\.documentoId))
        for documento in documentos {
            reconstruirDocumentoSinBloqueo(documento)
        }
    }

    private func reconstruirDocumentoSinBloqueo(_ documentoId: UInt64) {
        if let anterior = buscadores.removeValue(forKey: documentoId) {
            tinta_buscador_liberar(anterior)
        }
        let valores = registros.filter { $0.documentoId == documentoId }
        guard !valores.isEmpty, let buscador = tinta_buscador_crear() else {
            return
        }
        for registro in valores {
            let ok = registro.fuenteId.withCString { fuente in
                registro.texto.withCString { texto in
                    registro.texto.withCString { fragmento in
                        tinta_buscador_reemplazar_ocr(
                            buscador,
                            fuente,
                            0, 0, 0,
                            Float(registro.marco.minX),
                            Float(registro.marco.minY),
                            Float(registro.marco.width),
                            Float(registro.marco.height),
                            texto,
                            fragmento)
                    }
                }
            }
            if !ok {
                // El JSON sigue siendo la autoridad derivada. Si una entrada no
                // cabe en el motor, no se inventa un resultado parcial.
                continue
            }
        }
        buscadores[documentoId] = buscador
    }

    static func fuenteId(documentoId: UInt64, idsTrazos: [String]) -> String {
        let ids = idsTrazos.sorted()
        let entrada = "ocr-v1|\(documentoId)|" + ids.joined(separator: "|")
        let digest = SHA256.hash(data: Data(entrada.utf8))
        return digest.map { String(format: "%02x", $0) }.joined()
    }

    private static func validar(_ valores: [RegistroOCRManuscritoIPad]) throws {
        guard valores.count <= maximoRegistros else {
            throw ErrorIndiceOCRManuscritoIPad.demasiadosRegistros
        }
        var ids = Set<String>()
        for valor in valores {
            guard ids.insert(valor.fuenteId).inserted else {
                throw ErrorIndiceOCRManuscritoIPad.fuenteDuplicada(valor.fuenteId)
            }
            let marco = valor.marco
            guard valor.fuenteId.count == 64,
                  !valor.idsTrazos.isEmpty,
                  valor.idsTrazos.count <= 256,
                  Set(valor.idsTrazos).count == valor.idsTrazos.count,
                  valor.texto.count <= 8_000,
                  valor.confianza.isFinite,
                  (0...1).contains(valor.confianza),
                  marco.minX.isFinite, marco.minY.isFinite,
                  marco.width.isFinite, marco.height.isFinite,
                  marco.width > 0, marco.height > 0,
                  marco.width <= 100_000, marco.height <= 100_000,
                  valor.idiomas.count <= 16,
                  (valor.firmaMotor?.count ?? 0) <= 160 else {
                throw ErrorIndiceOCRManuscritoIPad.registroInvalido(valor.fuenteId)
            }
        }
    }
}

struct PreferenciasOCRManuscritoIPad: Codable, Equatable {
    var idiomasPrioritarios: [String] = ["es-ES", "en-US"]
    var correccionIdioma = true
    var deteccionAutomaticaIdioma = true

    static var actual: PreferenciasOCRManuscritoIPad {
        get {
            guard let datos = UserDefaults.standard.data(
                forKey: "Tinta.OCRManuscrito.Preferencias"),
                  let valor = try? JSONDecoder().decode(
                    PreferenciasOCRManuscritoIPad.self, from: datos) else {
                return .init()
            }
            return valor.normalizada()
        }
        set {
            let valor = newValue.normalizada()
            if let datos = try? JSONEncoder().encode(valor) {
                UserDefaults.standard.set(
                    datos, forKey: "Tinta.OCRManuscrito.Preferencias")
            }
        }
    }

    func normalizada() -> PreferenciasOCRManuscritoIPad {
        var copia = self
        var vistos = Set<String>()
        copia.idiomasPrioritarios = idiomasPrioritarios.compactMap { idioma in
            let limpio = idioma.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !limpio.isEmpty, limpio.count <= 32,
                  vistos.insert(limpio).inserted else { return nil }
            return limpio
        }
        if copia.idiomasPrioritarios.isEmpty {
            copia.idiomasPrioritarios = ["es-ES", "en-US"]
        }
        copia.idiomasPrioritarios = Array(copia.idiomasPrioritarios.prefix(8))
        return copia
    }

    func idiomasSoportados(por solicitud: VNRecognizeTextRequest) -> [String] {
        let soportados = Set((try? solicitud.supportedRecognitionLanguages()) ?? [])
        let preferidos = normalizada().idiomasPrioritarios.filter {
            soportados.contains($0)
        }
        if !preferidos.isEmpty { return preferidos }
        if soportados.contains("es-ES") { return ["es-ES"] }
        if soportados.contains("en-US") { return ["en-US"] }
        return Array(soportados.sorted().prefix(2))
    }
}
