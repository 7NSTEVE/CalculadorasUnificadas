import CryptoKit
import Foundation

enum TipoElementoReutilizableIPad: String, Codable, CaseIterable, Hashable {
    case manuscrito
    case resaltado
    case figura
    case imagen
    case texto
    case mixto
    case formula
    case circuito
    case diagrama
    case tecnico

    var nombre: String {
        switch self {
        case .manuscrito: return "Manuscrito"
        case .resaltado: return "Resaltado"
        case .figura: return "Figura"
        case .imagen: return "Imagen"
        case .texto: return "Texto"
        case .mixto: return "Mixto"
        case .formula: return "Fórmula"
        case .circuito: return "Circuito"
        case .diagrama: return "Diagrama"
        case .tecnico: return "Técnico"
        }
    }
}

struct ReferenciaRecursoElementoIPad: Codable, Equatable, Hashable {
    var hashSHA256: String
    var extensionArchivo: String
    var bytes: Int64

    var nombreArchivo: String {
        extensionArchivo.isEmpty ? hashSHA256 : "\(hashSHA256).\(extensionArchivo)"
    }
}

struct ColeccionElementosReutilizablesIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var nombre: String
    var etiquetas: [String]
    var orden: Int
    var creada: Date
    var actualizada: Date
    var archivada: Bool

    init(
        id: UUID = UUID(),
        nombre: String,
        etiquetas: [String] = [],
        orden: Int = 0,
        creada: Date = Date(),
        actualizada: Date = Date(),
        archivada: Bool = false
    ) {
        self.id = id
        self.nombre = nombre
        self.etiquetas = etiquetas
        self.orden = orden
        self.creada = creada
        self.actualizada = actualizada
        self.archivada = archivada
    }
}

struct ElementoReutilizableIPad: Codable, Equatable, Identifiable {
    var id: UUID
    var coleccionId: UUID
    var tipo: TipoElementoReutilizableIPad
    var nombre: String
    var detalle: String
    var etiquetas: [String]
    var recurso: ReferenciaRecursoElementoIPad
    var previsualizacion: ReferenciaRecursoElementoIPad?
    var anchoNatural: Double
    var altoNatural: Double
    var documentoOrigen: DocumentoPersistenteID?
    var paginaOrigen: Int?
    var frameOrigen: UUID?
    var creado: Date
    var actualizado: Date
    var usos: Int
    var favorito: Bool

    init(
        id: UUID = UUID(),
        coleccionId: UUID,
        tipo: TipoElementoReutilizableIPad,
        nombre: String,
        detalle: String = "",
        etiquetas: [String] = [],
        recurso: ReferenciaRecursoElementoIPad,
        previsualizacion: ReferenciaRecursoElementoIPad? = nil,
        anchoNatural: Double,
        altoNatural: Double,
        documentoOrigen: DocumentoPersistenteID? = nil,
        paginaOrigen: Int? = nil,
        frameOrigen: UUID? = nil,
        creado: Date = Date(),
        actualizado: Date = Date(),
        usos: Int = 0,
        favorito: Bool = false
    ) {
        self.id = id
        self.coleccionId = coleccionId
        self.tipo = tipo
        self.nombre = nombre
        self.detalle = detalle
        self.etiquetas = etiquetas
        self.recurso = recurso
        self.previsualizacion = previsualizacion
        self.anchoNatural = anchoNatural
        self.altoNatural = altoNatural
        self.documentoOrigen = documentoOrigen
        self.paginaOrigen = paginaOrigen
        self.frameOrigen = frameOrigen
        self.creado = creado
        self.actualizado = actualizado
        self.usos = usos
        self.favorito = favorito
    }
}

struct InstantaneaBibliotecaElementosIPad {
    var colecciones: [ColeccionElementosReutilizablesIPad]
    var elementos: [ElementoReutilizableIPad]
}

private struct EstadoBibliotecaElementosIPad: Codable {
    var version = 1
    var colecciones: [ColeccionElementosReutilizablesIPad] = []
    var elementos: [ElementoReutilizableIPad] = []
}

enum ErrorBibliotecaElementosIPad: LocalizedError {
    case nombreVacio
    case coleccionInexistente
    case dimensionesInvalidas
    case recursoInvalido
    case recursoCorrupto
    case demasiadosBytes(Int)

    var errorDescription: String? {
        switch self {
        case .nombreVacio: return "El elemento o la colección necesita un nombre."
        case .coleccionInexistente: return "La colección seleccionada ya no existe."
        case .dimensionesInvalidas: return "El elemento tiene dimensiones inválidas."
        case .recursoInvalido: return "El recurso reutilizable no tiene un identificador válido."
        case .recursoCorrupto: return "El recurso reutilizable no supera la verificación de integridad."
        case .demasiadosBytes(let bytes):
            return "El elemento ocupa \(ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)); excede el límite local."
        }
    }
}

/// Content-addressed store local para elementos. El contrato por SHA-256 permite
/// reemplazar este almacén por RepositorioAdjuntos en una integración posterior
/// sin cambiar el modelo persistido de la biblioteca.
final class AlmacenRecursosElementosIPad {
    static let compartido = AlmacenRecursosElementosIPad()
    static let maximoPorRecurso = 64 * 1_024 * 1_024

    private let base: URL
    private let archivos: FileManager
    private let bloqueo = NSLock()

    init(base: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        let raiz = base ?? archivos.urls(for: .applicationSupportDirectory,
                                         in: .userDomainMask)[0]
            .appendingPathComponent("ElementosReutilizables/Recursos", isDirectory: true)
        self.base = raiz
        try? archivos.createDirectory(at: raiz, withIntermediateDirectories: true)
    }

    func guardar(_ datos: Data, extensionArchivo: String = "bin") throws
        -> ReferenciaRecursoElementoIPad {
        guard datos.count <= Self.maximoPorRecurso else {
            throw ErrorBibliotecaElementosIPad.demasiadosBytes(datos.count)
        }
        let hash = Self.sha256(datos)
        let ext = Self.extensionSegura(extensionArchivo)
        let referencia = ReferenciaRecursoElementoIPad(
            hashSHA256: hash, extensionArchivo: ext, bytes: Int64(datos.count))
        let url = base.appendingPathComponent(referencia.nombreArchivo)

        bloqueo.lock(); defer { bloqueo.unlock() }
        if archivos.fileExists(atPath: url.path) {
            guard let existente = try? Data(contentsOf: url),
                  Self.sha256(existente) == hash else {
                throw ErrorBibliotecaElementosIPad.recursoCorrupto
            }
            return referencia
        }
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
        return referencia
    }

    func leer(_ referencia: ReferenciaRecursoElementoIPad) throws -> Data {
        guard referencia.hashSHA256.count == 64 else {
            throw ErrorBibliotecaElementosIPad.recursoInvalido
        }
        let url = base.appendingPathComponent(referencia.nombreArchivo)
        bloqueo.lock(); defer { bloqueo.unlock() }
        let datos = try Data(contentsOf: url)
        guard Int64(datos.count) == referencia.bytes,
              Self.sha256(datos) == referencia.hashSHA256 else {
            throw ErrorBibliotecaElementosIPad.recursoCorrupto
        }
        return datos
    }

    func eliminarHuerfanos(referenciasActivas: Set<ReferenciaRecursoElementoIPad>) {
        let activos = Set(referenciasActivas.map(\.nombreArchivo))
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let contenidos = try? archivos.contentsOfDirectory(
            at: base, includingPropertiesForKeys: nil) else { return }
        for url in contenidos where !activos.contains(url.lastPathComponent) {
            try? archivos.removeItem(at: url)
        }
    }

    private static func sha256(_ datos: Data) -> String {
        SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }

    private static func extensionSegura(_ valor: String) -> String {
        let limpia = valor.lowercased().filter { $0.isLetter || $0.isNumber }
        return String(limpia.prefix(12))
    }
}

final class BibliotecaElementosReutilizablesIPad {
    static let compartido = BibliotecaElementosReutilizablesIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private let almacen: AlmacenRecursosElementosIPad
    private var estado: EstadoBibliotecaElementosIPad

    init(
        base: URL? = nil,
        archivos: FileManager = .default,
        almacen: AlmacenRecursosElementosIPad? = nil
    ) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("ElementosReutilizables", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("biblioteca.json")
        self.almacen = almacen ?? AlmacenRecursosElementosIPad(
            base: carpeta.appendingPathComponent("Recursos", isDirectory: true),
            archivos: archivos)

        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(EstadoBibliotecaElementosIPad.self, from: datos) {
            estado = leido
        } else {
            estado = EstadoBibliotecaElementosIPad()
        }
    }

    func instantanea() -> InstantaneaBibliotecaElementosIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return .init(
            colecciones: estado.colecciones.sorted {
                if $0.orden != $1.orden { return $0.orden < $1.orden }
                return $0.nombre.localizedCaseInsensitiveCompare($1.nombre) == .orderedAscending
            },
            elementos: estado.elementos)
    }

    @discardableResult
    func crearColeccion(nombre: String, etiquetas: [String] = []) throws
        -> ColeccionElementosReutilizablesIPad {
        let nombre = Self.limpiar(nombre)
        guard !nombre.isEmpty else { throw ErrorBibliotecaElementosIPad.nombreVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let siguiente = (estado.colecciones.map(\.orden).max() ?? -1) + 1
        let coleccion = ColeccionElementosReutilizablesIPad(
            nombre: nombre, etiquetas: Self.etiquetas(etiquetas), orden: siguiente)
        estado.colecciones.append(coleccion)
        guardarSinBloqueo()
        return coleccion
    }

    func renombrarColeccion(id: UUID, nombre: String) throws {
        let nombre = Self.limpiar(nombre)
        guard !nombre.isEmpty else { throw ErrorBibliotecaElementosIPad.nombreVacio }
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.colecciones.firstIndex(where: { $0.id == id }) else {
            throw ErrorBibliotecaElementosIPad.coleccionInexistente
        }
        estado.colecciones[i].nombre = nombre
        estado.colecciones[i].actualizada = Date()
        guardarSinBloqueo()
    }

    @discardableResult
    func guardarElemento(
        coleccionId: UUID,
        tipo: TipoElementoReutilizableIPad,
        nombre: String,
        detalle: String = "",
        etiquetas: [String] = [],
        payload: Data,
        extensionArchivo: String = "cuitem",
        preview: Data? = nil,
        extensionPreview: String = "png",
        anchoNatural: Double,
        altoNatural: Double,
        documentoOrigen: DocumentoPersistenteID? = nil,
        paginaOrigen: Int? = nil,
        frameOrigen: UUID? = nil
    ) throws -> ElementoReutilizableIPad {
        let nombre = Self.limpiar(nombre)
        guard !nombre.isEmpty else { throw ErrorBibliotecaElementosIPad.nombreVacio }
        guard anchoNatural.isFinite, altoNatural.isFinite,
              anchoNatural > 0, altoNatural > 0 else {
            throw ErrorBibliotecaElementosIPad.dimensionesInvalidas
        }

        bloqueo.lock(); defer { bloqueo.unlock() }
        guard estado.colecciones.contains(where: { $0.id == coleccionId && !$0.archivada }) else {
            throw ErrorBibliotecaElementosIPad.coleccionInexistente
        }
        let recurso = try almacen.guardar(payload, extensionArchivo: extensionArchivo)
        let referenciaPreview = try preview.map {
            try almacen.guardar($0, extensionArchivo: extensionPreview)
        }
        let elemento = ElementoReutilizableIPad(
            coleccionId: coleccionId,
            tipo: tipo,
            nombre: nombre,
            detalle: Self.limpiar(detalle),
            etiquetas: Self.etiquetas(etiquetas),
            recurso: recurso,
            previsualizacion: referenciaPreview,
            anchoNatural: anchoNatural,
            altoNatural: altoNatural,
            documentoOrigen: documentoOrigen,
            paginaOrigen: paginaOrigen,
            frameOrigen: frameOrigen)
        estado.elementos.append(elemento)
        guardarSinBloqueo()
        return elemento
    }

    func payload(elementoId: UUID) throws -> Data? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let elemento = estado.elementos.first(where: { $0.id == elementoId }) else {
            return nil
        }
        return try almacen.leer(elemento.recurso)
    }

    func mover(elementoId: UUID, a coleccionId: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard estado.colecciones.contains(where: { $0.id == coleccionId && !$0.archivada }) else {
            throw ErrorBibliotecaElementosIPad.coleccionInexistente
        }
        guard let i = estado.elementos.firstIndex(where: { $0.id == elementoId }) else { return }
        estado.elementos[i].coleccionId = coleccionId
        estado.elementos[i].actualizado = Date()
        guardarSinBloqueo()
    }

    func favorito(elementoId: UUID, valor: Bool) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.elementos.firstIndex(where: { $0.id == elementoId }) else { return }
        estado.elementos[i].favorito = valor
        estado.elementos[i].actualizado = Date()
        guardarSinBloqueo()
    }

    func registrarUso(elementoId: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.elementos.firstIndex(where: { $0.id == elementoId }) else { return }
        estado.elementos[i].usos += 1
        estado.elementos[i].actualizado = Date()
        guardarSinBloqueo()
    }

    func eliminar(elementoId: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.elementos.removeAll { $0.id == elementoId }
        guardarSinBloqueo()
        limpiarRecursosSinBloqueo()
    }

    func archivarColeccion(id: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = estado.colecciones.firstIndex(where: { $0.id == id }) else { return }
        estado.colecciones[i].archivada = true
        estado.colecciones[i].actualizada = Date()
        guardarSinBloqueo()
    }

    func buscar(_ consulta: String, coleccionId: UUID? = nil, limite: Int = 100)
        -> [ElementoReutilizableIPad] {
        let terminos = Normalizador.tokens(consulta)
        bloqueo.lock(); defer { bloqueo.unlock() }
        let coleccionesActivas = Set(estado.colecciones.filter { !$0.archivada }.map(\.id))
        let candidatos = estado.elementos.filter {
            coleccionesActivas.contains($0.coleccionId)
                && (coleccionId == nil || $0.coleccionId == coleccionId)
        }
        guard !terminos.isEmpty else {
            return candidatos.sorted {
                if $0.favorito != $1.favorito { return $0.favorito && !$1.favorito }
                if $0.usos != $1.usos { return $0.usos > $1.usos }
                return $0.actualizado > $1.actualizado
            }.prefix(max(1, limite)).map { $0 }
        }
        return candidatos.compactMap { elemento -> (ElementoReutilizableIPad, Double)? in
            let titulo = Normalizador.tokens(elemento.nombre)
            let detalle = Normalizador.tokens(elemento.detalle)
            let etiquetas = elemento.etiquetas.flatMap(Normalizador.tokens)
            var puntos = 0.0
            for termino in terminos {
                var local = 0.0
                if titulo.contains(termino) { local = max(local, 5) }
                if etiquetas.contains(termino) { local = max(local, 4) }
                if detalle.contains(termino) { local = max(local, 2) }
                guard local > 0 else { return nil }
                puntos += local
            }
            if elemento.favorito { puntos += 0.5 }
            puntos += min(Double(elemento.usos) * 0.02, 0.5)
            return (elemento, puntos)
        }
        .sorted {
            if abs($0.1 - $1.1) > 0.000_001 { return $0.1 > $1.1 }
            return $0.0.actualizado > $1.0.actualizado
        }
        .prefix(max(1, limite)).map(\.0)
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? encoder.encode(estado) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    private func limpiarRecursosSinBloqueo() {
        var activas = Set(estado.elementos.map(\.recurso))
        activas.formUnion(estado.elementos.compactMap(\.previsualizacion))
        almacen.eliminarHuerfanos(referenciasActivas: activas)
    }

    private static func limpiar(_ valor: String) -> String {
        valor.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private static func etiquetas(_ valores: [String]) -> [String] {
        Array(Set(valores.map(limpiar).filter { !$0.isEmpty })).sorted {
            $0.localizedCaseInsensitiveCompare($1) == .orderedAscending
        }
    }
}
