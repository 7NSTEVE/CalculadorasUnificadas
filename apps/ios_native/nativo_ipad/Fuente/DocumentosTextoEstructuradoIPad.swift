import Foundation

// MARK: - Documento de texto de primera clase

enum TipoBloqueDocumentoTextoIPad: String, Codable, CaseIterable, Sendable {
    case titulo1, titulo2, titulo3
    case parrafo
    case listaVinetas
    case listaNumerada
    case tarea
    case cita
    case codigo
    case ecuacion
    case tabla
    case imagen
    case archivo
    case divisor
}

struct CeldaDocumentoTextoIPad: Codable, Equatable, Identifiable, Sendable {
    var id = UUID()
    var texto = ""
}

struct TablaDocumentoTextoIPad: Codable, Equatable, Sendable {
    var filas: [[CeldaDocumentoTextoIPad]]

    init(filas: Int = 2, columnas: Int = 2) {
        let f = min(max(filas, 1), 100)
        let c = min(max(columnas, 1), 30)
        self.filas = (0..<f).map { _ in (0..<c).map { _ in CeldaDocumentoTextoIPad() } }
    }

    var columnas: Int { filas.map(\.count).max() ?? 0 }

    mutating func normalizar() {
        guard !filas.isEmpty else { filas = [[CeldaDocumentoTextoIPad()]]; return }
        let c = min(max(columnas, 1), 30)
        filas = Array(filas.prefix(100)).map { fila in
            var copia = Array(fila.prefix(c))
            while copia.count < c { copia.append(CeldaDocumentoTextoIPad()) }
            return copia
        }
    }
}

struct AdjuntoDocumentoTextoIPad: Codable, Equatable, Sendable {
    /// Hash o identificador estable de un recurso gestionado por otro almacén.
    /// El documento no incrusta blobs grandes dentro del JSON.
    var recursoId: String
    var nombre: String
    var tipoMIME: String?
    var descripcionAccesible: String?
}

struct BloqueDocumentoTextoIPad: Codable, Equatable, Identifiable, Sendable {
    var id = UUID()
    var tipo: TipoBloqueDocumentoTextoIPad
    var texto = ""
    var completado = false
    var lenguajeCodigo: String?
    var tabla: TablaDocumentoTextoIPad?
    var adjunto: AdjuntoDocumentoTextoIPad?
    var creado = Date()
    var modificado = Date()

    init(tipo: TipoBloqueDocumentoTextoIPad = .parrafo, texto: String = "") {
        self.tipo = tipo
        self.texto = texto
        if tipo == .tabla { tabla = TablaDocumentoTextoIPad() }
    }
}

struct DocumentoTextoEstructuradoIPad: Codable, Equatable, Identifiable, Sendable {
    var version = 1
    var id = DocumentoPersistenteID()
    var titulo: String
    var bloques: [BloqueDocumentoTextoIPad] = []
    var etiquetas: [String] = []
    var cursoId: UUID?
    var creado = Date()
    var modificado = Date()
    var revision: UInt64 = 0

    init(titulo: String = "Documento de texto", cursoId: UUID? = nil) {
        self.titulo = titulo
        self.cursoId = cursoId
        self.bloques = [BloqueDocumentoTextoIPad()]
    }

    mutating func insertar(_ bloque: BloqueDocumentoTextoIPad, en indice: Int? = nil) {
        let destino = min(max(indice ?? bloques.count, 0), bloques.count)
        bloques.insert(bloque, at: destino)
        tocar()
    }

    mutating func actualizar(_ bloque: BloqueDocumentoTextoIPad) -> Bool {
        guard let i = bloques.firstIndex(where: { $0.id == bloque.id }) else { return false }
        var copia = bloque
        copia.modificado = Date()
        bloques[i] = copia
        tocar()
        return true
    }

    mutating func eliminar(_ id: UUID) -> Bool {
        guard let i = bloques.firstIndex(where: { $0.id == id }) else { return false }
        bloques.remove(at: i)
        if bloques.isEmpty { bloques.append(BloqueDocumentoTextoIPad()) }
        tocar()
        return true
    }

    mutating func mover(desde origen: Int, hacia destino: Int) -> Bool {
        guard bloques.indices.contains(origen) else { return false }
        let bloque = bloques.remove(at: origen)
        let d = min(max(destino, 0), bloques.count)
        bloques.insert(bloque, at: d)
        tocar()
        return true
    }

    mutating func alternarTarea(_ id: UUID) -> Bool {
        guard let i = bloques.firstIndex(where: { $0.id == id && $0.tipo == .tarea }) else { return false }
        bloques[i].completado.toggle()
        bloques[i].modificado = Date()
        tocar()
        return true
    }

    private mutating func tocar() {
        revision &+= 1
        modificado = Date()
    }
}

enum ErrorDocumentoTextoIPad: LocalizedError {
    case noExiste
    case corrupto
    case versionNoCompatible(Int)
    case documentoDistinto
    case demasiadoGrande

    var errorDescription: String? {
        switch self {
        case .noExiste: return "El documento de texto no existe."
        case .corrupto: return "El documento de texto está dañado o incompleto."
        case .versionNoCompatible(let v): return "La versión \(v) del documento todavía no es compatible."
        case .documentoDistinto: return "El archivo abierto pertenece a otro documento."
        case .demasiadoGrande: return "El documento excede el límite seguro de almacenamiento."
        }
    }
}

final class RepositorioDocumentosTextoIPad: @unchecked Sendable {
    static let compartido = RepositorioDocumentosTextoIPad()

    private let cola = DispatchQueue(label: "cuadernu.documentos-texto", qos: .utility)
    private let carpeta: URL
    private let fm: FileManager
    private let limiteBytes = 32 * 1_024 * 1_024

    init(base: URL? = nil, fileManager: FileManager = .default) {
        fm = fileManager
        let soporte = base ?? fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        carpeta = soporte.appendingPathComponent("CuadernU/DocumentosTexto", isDirectory: true)
        try? fileManager.createDirectory(at: carpeta, withIntermediateDirectories: true)
    }

    private func url(_ id: DocumentoPersistenteID) -> URL {
        carpeta.appendingPathComponent("\(id.description).json")
    }

    func crear(titulo: String = "Documento de texto", cursoId: UUID? = nil) throws -> DocumentoTextoEstructuradoIPad {
        let doc = DocumentoTextoEstructuradoIPad(titulo: titulo, cursoId: cursoId)
        try guardar(doc)
        return doc
    }

    func guardar(_ documento: DocumentoTextoEstructuradoIPad) throws {
        try cola.sync {
            var copia = documento
            copia.titulo = String(copia.titulo.prefix(500))
            copia.etiquetas = Array(copia.etiquetas.prefix(100)).map { String($0.prefix(100)) }
            for i in copia.bloques.indices {
                copia.bloques[i].texto = String(copia.bloques[i].texto.prefix(1_000_000))
                copia.bloques[i].tabla?.normalizar()
            }
            let encoder = JSONEncoder()
            encoder.dateEncodingStrategy = .iso8601
            encoder.outputFormatting = [.sortedKeys]
            let datos = try encoder.encode(copia)
            guard datos.count <= limiteBytes else { throw ErrorDocumentoTextoIPad.demasiadoGrande }
            try datos.write(to: url(copia.id), options: [.atomic, .completeFileProtection])
        }
    }

    func cargar(_ id: DocumentoPersistenteID) throws -> DocumentoTextoEstructuradoIPad {
        try cola.sync {
            let ruta = url(id)
            guard fm.fileExists(atPath: ruta.path) else { throw ErrorDocumentoTextoIPad.noExiste }
            let datos = try Data(contentsOf: ruta)
            guard datos.count <= limiteBytes else { throw ErrorDocumentoTextoIPad.demasiadoGrande }
            let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
            guard let doc = try? decoder.decode(DocumentoTextoEstructuradoIPad.self, from: datos) else {
                throw ErrorDocumentoTextoIPad.corrupto
            }
            guard doc.version == 1 else { throw ErrorDocumentoTextoIPad.versionNoCompatible(doc.version) }
            guard doc.id == id else { throw ErrorDocumentoTextoIPad.documentoDistinto }
            return doc
        }
    }

    func listar() -> [DocumentoTextoEstructuradoIPad] {
        cola.sync {
            guard let urls = try? fm.contentsOfDirectory(at: carpeta, includingPropertiesForKeys: nil) else { return [] }
            return urls.filter { $0.pathExtension == "json" }.compactMap { ruta in
                guard let datos = try? Data(contentsOf: ruta), datos.count <= limiteBytes else { return nil }
                let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
                return try? d.decode(DocumentoTextoEstructuradoIPad.self, from: datos)
            }.filter { $0.version == 1 }.sorted { $0.modificado > $1.modificado }
        }
    }

    func eliminar(_ id: DocumentoPersistenteID) throws {
        try cola.sync {
            let ruta = url(id)
            if fm.fileExists(atPath: ruta.path) { try fm.removeItem(at: ruta) }
        }
    }
}

// MARK: - Exportaciones deterministas

enum ExportadorDocumentoTextoIPad {
    static func textoPlano(_ documento: DocumentoTextoEstructuradoIPad) -> String {
        documento.bloques.compactMap { bloque in
            switch bloque.tipo {
            case .divisor: return "────────"
            case .tabla:
                return bloque.tabla?.filas.map { $0.map(\.texto).joined(separator: "\t") }.joined(separator: "\n")
            case .imagen, .archivo:
                return bloque.adjunto.map { "[\($0.nombre)]" } ?? nil
            case .tarea: return "[\(bloque.completado ? "x" : " ")] \(bloque.texto)"
            default: return bloque.texto
            }
        }.joined(separator: "\n\n")
    }

    static func markdown(_ documento: DocumentoTextoEstructuradoIPad) -> String {
        var numerada = 1
        return documento.bloques.compactMap { bloque in
            switch bloque.tipo {
            case .titulo1: return "# \(bloque.texto)"
            case .titulo2: return "## \(bloque.texto)"
            case .titulo3: return "### \(bloque.texto)"
            case .parrafo: return bloque.texto
            case .listaVinetas: return "- \(bloque.texto)"
            case .listaNumerada:
                defer { numerada += 1 }
                return "\(numerada). \(bloque.texto)"
            case .tarea: return "- [\(bloque.completado ? "x" : " ")] \(bloque.texto)"
            case .cita: return bloque.texto.split(separator: "\n").map { "> \($0)" }.joined(separator: "\n")
            case .codigo: return "```\(bloque.lenguajeCodigo ?? "")\n\(bloque.texto)\n```"
            case .ecuacion: return "$$\n\(bloque.texto)\n$$"
            case .tabla:
                guard let tabla = bloque.tabla, let primera = tabla.filas.first else { return nil }
                let cab = "| " + primera.map(\.texto).joined(separator: " | ") + " |"
                let sep = "| " + primera.map { _ in "---" }.joined(separator: " | ") + " |"
                let cuerpo = tabla.filas.dropFirst().map { "| " + $0.map(\.texto).joined(separator: " | ") + " |" }.joined(separator: "\n")
                return [cab, sep, cuerpo].filter { !$0.isEmpty }.joined(separator: "\n")
            case .imagen: return bloque.adjunto.map { "![\($0.descripcionAccesible ?? $0.nombre)](cuadernu://resource/\($0.recursoId))" }
            case .archivo: return bloque.adjunto.map { "[\($0.nombre)](cuadernu://resource/\($0.recursoId))" }
            case .divisor: return "---"
            }
        }.joined(separator: "\n\n")
    }
}