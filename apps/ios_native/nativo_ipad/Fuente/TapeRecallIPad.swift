import Foundation

enum EstadoTapeRecallIPad: String, Codable, Equatable {
    case cubierto
    case revelado
}

struct RegionTapeRecallIPad: Codable, Equatable, Hashable {
    var x: Double
    var y: Double
    var ancho: Double
    var alto: Double

    init(x: Double, y: Double, ancho: Double, alto: Double) {
        self.x = x; self.y = y
        self.ancho = max(ancho, 1); self.alto = max(alto, 1)
    }
}

struct TapeRecallIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var documentoId: DocumentoPersistenteID
    var pagina: Int?
    var frameId: UUID?
    var region: RegionTapeRecallIPad
    var titulo: String
    var estado: EstadoTapeRecallIPad
    var creado: Date
    var actualizado: Date

    init(
        id: UUID = UUID(),
        documentoId: DocumentoPersistenteID,
        pagina: Int? = nil,
        frameId: UUID? = nil,
        region: RegionTapeRecallIPad,
        titulo: String = "",
        estado: EstadoTapeRecallIPad = .cubierto,
        creado: Date = Date(),
        actualizado: Date = Date()
    ) {
        self.id = id; self.documentoId = documentoId; self.pagina = pagina
        self.frameId = frameId; self.region = region; self.titulo = titulo
        self.estado = estado; self.creado = creado; self.actualizado = actualizado
    }

    /// Por seguridad de active recall, una exportación normal nunca revela
    /// contenido solo porque el usuario lo hubiera destapado temporalmente.
    func paraExportacion(incluirRevelados: Bool = false) -> TapeRecallIPad {
        var copia = self
        if !incluirRevelados { copia.estado = .cubierto }
        return copia
    }
}

private struct ArchivoTapeRecallIPad: Codable, Equatable {
    var version = 1
    var documentoId: DocumentoPersistenteID
    var cintas: [TapeRecallIPad]
}

enum ErrorTapeRecallIPad: Error, Equatable {
    case documentoInvalido
    case datosCorruptos
}

/// Sidecar por UUID durable. El repositorio acepta un `base` inyectable para
/// tests y escribe de forma atómica para no dejar JSON parcial tras una caída.
final class RepositorioTapeRecallIPad {
    static let compartido = RepositorioTapeRecallIPad()

    private let base: URL
    private let archivos: FileManager
    private let cola = DispatchQueue(label: "cuadernu.tape-recall")

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        if let base {
            self.base = base
        } else {
            let soporte = archivos.urls(for: .applicationSupportDirectory,
                                        in: .userDomainMask)[0]
            self.base = soporte.appendingPathComponent("Tinta/TapeRecall", isDirectory: true)
        }
        try? archivos.createDirectory(at: self.base, withIntermediateDirectories: true)
    }

    func cargar(documento: DocumentoPersistenteID) throws -> [TapeRecallIPad] {
        try cola.sync {
            let destino = url(documento)
            guard archivos.fileExists(atPath: destino.path) else { return [] }
            guard let datos = try? Data(contentsOf: destino),
                  let archivo = try? JSONDecoder().decode(ArchivoTapeRecallIPad.self, from: datos)
            else { throw ErrorTapeRecallIPad.datosCorruptos }
            guard archivo.documentoId == documento,
                  archivo.cintas.allSatisfy({ $0.documentoId == documento })
            else { throw ErrorTapeRecallIPad.documentoInvalido }
            return archivo.cintas.sorted { $0.creado < $1.creado }
        }
    }

    @discardableResult
    func guardar(_ cintas: [TapeRecallIPad], documento: DocumentoPersistenteID) -> Bool {
        cola.sync {
            guard cintas.allSatisfy({ $0.documentoId == documento }) else { return false }
            do {
                let archivo = ArchivoTapeRecallIPad(documentoId: documento, cintas: cintas)
                let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
                encoder.outputFormatting = [.sortedKeys]
                let datos = try encoder.encode(archivo)
                try datos.write(to: url(documento), options: [.atomic, .completeFileProtection])
                return true
            } catch { return false }
        }
    }

    @discardableResult
    func upsert(_ cinta: TapeRecallIPad) -> Bool {
        var cintas = (try? cargar(documento: cinta.documentoId)) ?? []
        var copia = cinta; copia.actualizado = Date()
        if let indice = cintas.firstIndex(where: { $0.id == copia.id }) {
            cintas[indice] = copia
        } else {
            cintas.append(copia)
        }
        return guardar(cintas, documento: copia.documentoId)
    }

    @discardableResult
    func alternar(id: UUID, documento: DocumentoPersistenteID) -> EstadoTapeRecallIPad? {
        var cintas = (try? cargar(documento: documento)) ?? []
        guard let indice = cintas.firstIndex(where: { $0.id == id }) else { return nil }
        cintas[indice].estado = cintas[indice].estado == .cubierto ? .revelado : .cubierto
        cintas[indice].actualizado = Date()
        guard guardar(cintas, documento: documento) else { return nil }
        return cintas[indice].estado
    }

    @discardableResult
    func eliminar(id: UUID, documento: DocumentoPersistenteID) -> Bool {
        var cintas = (try? cargar(documento: documento)) ?? []
        let cantidad = cintas.count
        cintas.removeAll { $0.id == id }
        guard cintas.count != cantidad else { return true }
        return guardar(cintas, documento: documento)
    }

    func paraExportacion(
        documento: DocumentoPersistenteID,
        incluirRevelados: Bool = false
    ) -> [TapeRecallIPad] {
        ((try? cargar(documento: documento)) ?? []).map {
            $0.paraExportacion(incluirRevelados: incluirRevelados)
        }
    }

    private func url(_ documento: DocumentoPersistenteID) -> URL {
        base.appendingPathComponent("\(documento.description).json")
    }
}
