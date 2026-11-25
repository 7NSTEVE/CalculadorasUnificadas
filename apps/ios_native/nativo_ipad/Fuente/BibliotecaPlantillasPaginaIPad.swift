import Foundation

enum TipoPautaPlantillaIPad: String, Codable, CaseIterable, Hashable {
    case lisa
    case lineas
    case cuadricula
    case puntos
    case cornell
    case isometrica
    case hexagonal
    case polar
    case semilog
    case ingenieria

    var nombre: String {
        switch self {
        case .lisa: return "Lisa"
        case .lineas: return "Rayada"
        case .cuadricula: return "Cuadrícula"
        case .puntos: return "Punteada"
        case .cornell: return "Cornell"
        case .isometrica: return "Isométrica"
        case .hexagonal: return "Hexagonal"
        case .polar: return "Polar"
        case .semilog: return "Semilog"
        case .ingenieria: return "Ingeniería"
        }
    }
}

struct TamanoPaginaPlantillaIPad: Codable, Equatable, Hashable {
    var anchoPuntos: Double
    var altoPuntos: Double

    var horizontal: Bool { anchoPuntos > altoPuntos }

    static let a4 = TamanoPaginaPlantillaIPad(anchoPuntos: 595.28, altoPuntos: 841.89)
    static let carta = TamanoPaginaPlantillaIPad(anchoPuntos: 612, altoPuntos: 792)

    func rotado() -> TamanoPaginaPlantillaIPad {
        .init(anchoPuntos: altoPuntos, altoPuntos: anchoPuntos)
    }
}

struct EstiloPlantillaPaginaIPad: Codable, Equatable, Hashable {
    var fondoARGB: UInt32 = 0xFFFFFFFF
    var lineaARGB: UInt32 = 0xFFB8C1CC
    var lineaSecundariaARGB: UInt32? = nil
    var separacionPuntos: Double = 14.17 // 5 mm a 72 dpi
    var grosorLinea: Double = 0.6
    var margenIzquierdoPuntos: Double = 36
    var margenDerechoPuntos: Double = 36
    var margenSuperiorPuntos: Double = 36
    var margenInferiorPuntos: Double = 36
    var opacidadPauta: Double = 0.55
}

struct PlantillaPaginaIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var nombre: String
    var categoria: String
    var pauta: TipoPautaPlantillaIPad
    var tamano: TamanoPaginaPlantillaIPad
    var estilo: EstiloPlantillaPaginaIPad
    var esPortada: Bool
    var incorporada: Bool
    var creada: Date
    var actualizada: Date
    var etiquetas: [String]

    init(
        id: UUID = UUID(),
        nombre: String,
        categoria: String = "General",
        pauta: TipoPautaPlantillaIPad,
        tamano: TamanoPaginaPlantillaIPad = .a4,
        estilo: EstiloPlantillaPaginaIPad = .init(),
        esPortada: Bool = false,
        incorporada: Bool = false,
        creada: Date = Date(),
        actualizada: Date = Date(),
        etiquetas: [String] = []
    ) {
        self.id = id
        self.nombre = nombre
        self.categoria = categoria
        self.pauta = pauta
        self.tamano = tamano
        self.estilo = estilo
        self.esPortada = esPortada
        self.incorporada = incorporada
        self.creada = creada
        self.actualizada = actualizada
        self.etiquetas = etiquetas
    }
}

struct PaquetePlantillaPaginaIPad: Codable, Equatable {
    var formato = 1
    var plantilla: PlantillaPaginaIPad
}

private struct EstadoBibliotecaPlantillasIPad: Codable {
    var version = 1
    var personalizadas: [PlantillaPaginaIPad] = []
    var favoritos: Set<UUID> = []
    var ocultas: Set<UUID> = []
}

enum ErrorBibliotecaPlantillasIPad: LocalizedError {
    case nombreVacio
    case dimensionesInvalidas
    case pautaInvalida
    case formatoNoCompatible
    case plantillaIncorporada

    var errorDescription: String? {
        switch self {
        case .nombreVacio: return "La plantilla necesita un nombre."
        case .dimensionesInvalidas: return "El tamaño de página no es válido."
        case .pautaInvalida: return "La pauta contiene medidas fuera de rango."
        case .formatoNoCompatible: return "El archivo de plantilla no es compatible."
        case .plantillaIncorporada: return "Las plantillas incorporadas no se eliminan; se pueden ocultar."
        }
    }
}

final class BibliotecaPlantillasPaginaIPad {
    static let compartido = BibliotecaPlantillasPaginaIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoBibliotecaPlantillasIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("PlantillasPagina", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("biblioteca-plantillas.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(EstadoBibliotecaPlantillasIPad.self, from: datos) {
            estado = leido
        } else { estado = EstadoBibliotecaPlantillasIPad() }
    }

    func todas(incluirOcultas: Bool = false) -> [PlantillaPaginaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return (Self.incorporadas + estado.personalizadas)
            .filter { incluirOcultas || !estado.ocultas.contains($0.id) }
            .sorted { a, b in
                let fa = estado.favoritos.contains(a.id), fb = estado.favoritos.contains(b.id)
                if fa != fb { return fa && !fb }
                if a.categoria != b.categoria { return a.categoria < b.categoria }
                return a.nombre.localizedCaseInsensitiveCompare(b.nombre) == .orderedAscending
            }
    }

    func esFavorita(_ id: UUID) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.favoritos.contains(id)
    }

    func buscar(_ consulta: String, categoria: String? = nil) -> [PlantillaPaginaIPad] {
        let terminos = Normalizador.tokens(consulta)
        let candidatas = todas().filter {
            categoria == nil || Normalizador.texto($0.categoria) == Normalizador.texto(categoria ?? "")
        }
        guard !terminos.isEmpty else { return candidatas }
        return candidatas.filter { plantilla in
            let tokens = Normalizador.tokens(
                ([plantilla.nombre, plantilla.categoria, plantilla.pauta.nombre]
                    + plantilla.etiquetas).joined(separator: " "))
            return terminos.allSatisfy(tokens.contains)
        }
    }

    @discardableResult
    func crear(_ plantilla: PlantillaPaginaIPad) throws -> PlantillaPaginaIPad {
        var copia = plantilla
        try Self.validar(copia)
        copia.id = UUID()
        copia.incorporada = false
        copia.nombre = copia.nombre.trimmingCharacters(in: .whitespacesAndNewlines)
        copia.categoria = copia.categoria.trimmingCharacters(in: .whitespacesAndNewlines)
        copia.etiquetas = Self.limpiarEtiquetas(copia.etiquetas)
        copia.creada = Date(); copia.actualizada = copia.creada
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.personalizadas.append(copia)
        guardarSinBloqueo()
        return copia
    }

    @discardableResult
    func duplicar(_ id: UUID) throws -> PlantillaPaginaIPad? {
        guard var original = todas(incluirOcultas: true).first(where: { $0.id == id }) else {
            return nil
        }
        original.nombre += " copia"
        return try crear(original)
    }

    func favorito(_ id: UUID, valor: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if valor { estado.favoritos.insert(id) } else { estado.favoritos.remove(id) }
        guardarSinBloqueo()
    }

    func ocultar(_ id: UUID, valor: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if valor { estado.ocultas.insert(id) } else { estado.ocultas.remove(id) }
        guardarSinBloqueo()
    }

    func eliminar(_ id: UUID) throws {
        if Self.incorporadas.contains(where: { $0.id == id }) {
            throw ErrorBibliotecaPlantillasIPad.plantillaIncorporada
        }
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.personalizadas.removeAll { $0.id == id }
        estado.favoritos.remove(id); estado.ocultas.remove(id)
        guardarSinBloqueo()
    }

    func exportar(_ id: UUID) throws -> Data? {
        guard let plantilla = todas(incluirOcultas: true).first(where: { $0.id == id }) else {
            return nil
        }
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return try encoder.encode(PaquetePlantillaPaginaIPad(plantilla: plantilla))
    }

    @discardableResult
    func importar(_ datos: Data) throws -> PlantillaPaginaIPad {
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        guard let paquete = try? decoder.decode(PaquetePlantillaPaginaIPad.self, from: datos),
              paquete.formato == 1 else {
            throw ErrorBibliotecaPlantillasIPad.formatoNoCompatible
        }
        return try crear(paquete.plantilla)
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? encoder.encode(estado) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    private static func validar(_ plantilla: PlantillaPaginaIPad) throws {
        guard !plantilla.nombre.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            throw ErrorBibliotecaPlantillasIPad.nombreVacio
        }
        guard plantilla.tamano.anchoPuntos.isFinite, plantilla.tamano.altoPuntos.isFinite,
              plantilla.tamano.anchoPuntos >= 72, plantilla.tamano.altoPuntos >= 72,
              plantilla.tamano.anchoPuntos <= 20_000, plantilla.tamano.altoPuntos <= 20_000 else {
            throw ErrorBibliotecaPlantillasIPad.dimensionesInvalidas
        }
        guard plantilla.estilo.separacionPuntos.isFinite,
              plantilla.estilo.separacionPuntos >= 2,
              plantilla.estilo.separacionPuntos <= 500,
              plantilla.estilo.grosorLinea >= 0.1,
              plantilla.estilo.grosorLinea <= 20,
              (0...1).contains(plantilla.estilo.opacidadPauta) else {
            throw ErrorBibliotecaPlantillasIPad.pautaInvalida
        }
    }

    private static func limpiarEtiquetas(_ valores: [String]) -> [String] {
        Array(Set(valores.map {
            $0.trimmingCharacters(in: .whitespacesAndNewlines)
        }.filter { !$0.isEmpty })).sorted()
    }

    static let incorporadas: [PlantillaPaginaIPad] = {
        func uuid(_ valor: String) -> UUID { UUID(uuidString: valor)! }
        let azul: UInt32 = 0xFFB8C7D9
        let gris: UInt32 = 0xFFC6CCD4
        var cuadricula = EstiloPlantillaPaginaIPad(lineaARGB: azul)
        cuadricula.separacionPuntos = 14.17
        var puntos = EstiloPlantillaPaginaIPad(lineaARGB: gris)
        puntos.separacionPuntos = 14.17; puntos.grosorLinea = 0.8
        var cornell = EstiloPlantillaPaginaIPad(lineaARGB: azul)
        cornell.margenIzquierdoPuntos = 150; cornell.margenSuperiorPuntos = 90
        var iso = EstiloPlantillaPaginaIPad(lineaARGB: 0xFFBFC8D2)
        iso.separacionPuntos = 18
        var polar = EstiloPlantillaPaginaIPad(lineaARGB: 0xFFBFC8D2)
        polar.separacionPuntos = 24
        var semilog = EstiloPlantillaPaginaIPad(lineaARGB: 0xFFB4C3D3)
        semilog.separacionPuntos = 18
        return [
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000001"),
                nombre: "Blanca A4", pauta: .lisa, incorporada: true,
                etiquetas: ["papel", "blanco"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000002"),
                nombre: "Cuadrícula 5 mm", categoria: "Papel",
                pauta: .cuadricula, estilo: cuadricula, incorporada: true,
                etiquetas: ["5mm", "matemática"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000003"),
                nombre: "Puntos 5 mm", categoria: "Papel", pauta: .puntos,
                estilo: puntos, incorporada: true, etiquetas: ["dot grid"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000004"),
                nombre: "Cornell", categoria: "Estudio", pauta: .cornell,
                estilo: cornell, incorporada: true,
                etiquetas: ["resumen", "preguntas", "apuntes"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000005"),
                nombre: "Isométrica", categoria: "Ingeniería", pauta: .isometrica,
                estilo: iso, incorporada: true, etiquetas: ["3d", "dibujo técnico"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000006"),
                nombre: "Polar", categoria: "Ingeniería", pauta: .polar,
                estilo: polar, incorporada: true,
                etiquetas: ["fasores", "ángulo", "vectores"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000007"),
                nombre: "Semilog Bode", categoria: "Ingeniería", pauta: .semilog,
                estilo: semilog, incorporada: true,
                etiquetas: ["bode", "frecuencia", "ganancia"]),
            PlantillaPaginaIPad(id: uuid("10000000-0000-4000-8000-000000000008"),
                nombre: "Cuadrícula de ingeniería", categoria: "Ingeniería",
                pauta: .ingenieria, tamano: .carta, estilo: cuadricula,
                incorporada: true, etiquetas: ["circuitos", "cálculo", "electrónica"]),
        ]
    }()
}
