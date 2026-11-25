import Foundation

/// Registro estable de un cuaderno en la pantalla de inicio.
///
/// `id` también es el contexto de persistencia de sus pestañas. De esa forma,
/// abrir una tarjeta restaura exactamente los archivos de ese cuaderno en vez
/// de crear una sesión C++ distinta y abandonar la que se acababa de cargar.
struct CuadernoBibliotecaIPad: Codable, Equatable, Identifiable {
    let id: String
    var titulo: String
    let fechaCreacion: Date
    var fechaModificacion: Date
    var indiceColor: Int
}

enum ErrorBibliotecaCuadernU: LocalizedError, Equatable {
    case catalogoDanado
    case identificadorInvalido
    case cuadernoNoEncontrado
    case tituloVacio

    var errorDescription: String? {
        switch self {
        case .catalogoDanado:
            return "El catálogo de cuadernos está dañado. Se conservó el archivo original para poder recuperarlo."
        case .identificadorInvalido:
            return "El identificador del cuaderno no es válido."
        case .cuadernoNoEncontrado:
            return "El cuaderno ya no existe en la biblioteca."
        case .tituloVacio:
            return "Escriba un nombre para el cuaderno."
        }
    }
}

/// Persistencia pequeña, atómica y comprobable del catálogo.
///
/// Se mantiene separada del almacenamiento binario del lienzo: el catálogo
/// decide qué cuadernos existen y `RepositorioPestanasIPad` conserva el
/// contenido de cada uno. Un JSON ilegible nunca se interpreta como una lista
/// vacía porque eso permitiría que el siguiente guardado destruyera referencias
/// recuperables.
final class RepositorioBibliotecaCuadernU {
    static let cantidadColores = 8

    private let archivos: FileManager
    private let directorio: URL
    private let catalogo: URL
    private let marcadorMigracion: URL
    private let tombstones: URL
    private let cola = DispatchQueue(label: "cuadernu.biblioteca.persistencia")

    init(directorioBase: URL, archivos: FileManager = .default) {
        self.archivos = archivos
        directorio = directorioBase
        catalogo = directorioBase.appendingPathComponent("cuadernos-v1.json")
        marcadorMigracion = directorioBase.appendingPathComponent(
            ".biblioteca-inicializada-v1")
        tombstones = directorioBase.appendingPathComponent(
            "cuadernos-eliminados-v1.json")
    }

    func listar() throws -> [CuadernoBibliotecaIPad] {
        try cola.sync { try leerSinBloqueo() }
    }

    /// Crea una sola entrada que apunta al contexto usado antes de que existiera
    /// la biblioteca. El marcador impide recrearla si el usuario elimina luego
    /// todos sus cuadernos voluntariamente.
    @discardableResult
    func prepararPrimeraEjecucion(
        contextoAnterior: String,
        contextosPersistidos: Set<String> = []) throws
        -> CuadernoBibliotecaIPad? {
        try cola.sync {
            try crearDirectorioSinBloqueo()
            let primeraInicializacion = !archivos.fileExists(
                atPath: marcadorMigracion.path)
            var registros = try leerSinBloqueo()
            let eliminados = try leerTombstonesSinBloqueo()
            var candidatos: [String] = []
            if primeraInicializacion {
                candidatos = contextosPersistidos.sorted()
                candidatos.removeAll { $0 == contextoAnterior }
                candidatos.insert(contextoAnterior, at: 0)
            }
            var creados: [CuadernoBibliotecaIPad] = []
            var origenPorId: [String: String] = [:]
            for contexto in candidatos {
                let id = try Self.identificadorSeguro(contexto)
                if let anterior = origenPorId[id], anterior != contexto {
                    // La sustitución legacy no es inyectiva ("a/b" y "a_b").
                    // Nunca se descarta uno por adivinanza durante migración.
                    throw ErrorBibliotecaCuadernU.identificadorInvalido
                }
                origenPorId[id] = contexto
                guard !eliminados.contains(id),
                      !registros.contains(where: { $0.id == id }) else {
                    continue
                }
                let fecha = Date()
                let registro = CuadernoBibliotecaIPad(
                    id: id,
                    titulo: registros.isEmpty
                        ? "Mi cuaderno"
                        : "Cuaderno recuperado \(registros.count + 1)",
                    fechaCreacion: fecha,
                    fechaModificacion: fecha,
                    indiceColor: Self.colorDeterminista(para: id))
                registros.append(registro)
                creados.append(registro)
            }
            if !creados.isEmpty {
                try guardarSinBloqueo(registros)
            }
            if primeraInicializacion {
                try Data("1\n".utf8).write(
                    to: marcadorMigracion, options: .atomic)
            }
            return creados.first
        }
    }

    var catalogoExiste: Bool {
        archivos.fileExists(atPath: catalogo.path)
    }

    @discardableResult
    func crear(titulo: String, identificador: String? = nil,
               fecha: Date = Date()) throws -> CuadernoBibliotecaIPad {
        try cola.sync {
            var registros = try leerSinBloqueo()
            let eliminados = try leerTombstonesSinBloqueo()
            let tituloNormalizado = try Self.normalizarTitulo(titulo)
            var id = try Self.identificadorSeguro(
                identificador ?? UUID().uuidString)
            while eliminados.contains(id)
                    || registros.contains(where: { $0.id == id }) {
                id = UUID().uuidString
            }
            let registro = CuadernoBibliotecaIPad(
                id: id,
                titulo: tituloNormalizado,
                fechaCreacion: fecha,
                fechaModificacion: fecha,
                indiceColor: Self.colorDeterminista(para: id))
            registros.append(registro)
            try guardarSinBloqueo(registros)
            return registro
        }
    }

    @discardableResult
    func renombrar(id: String, titulo: String, fecha: Date = Date()) throws
        -> CuadernoBibliotecaIPad {
        try cola.sync {
            var registros = try leerSinBloqueo()
            guard let indice = registros.firstIndex(where: { $0.id == id }) else {
                throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
            }
            registros[indice].titulo = try Self.normalizarTitulo(titulo)
            registros[indice].fechaModificacion = fecha
            try guardarSinBloqueo(registros)
            return registros[indice]
        }
    }

    @discardableResult
    func cambiarColor(id: String, indiceColor: Int, fecha: Date = Date()) throws
        -> CuadernoBibliotecaIPad {
        try cola.sync {
            var registros = try leerSinBloqueo()
            guard let indice = registros.firstIndex(where: { $0.id == id }) else {
                throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
            }
            registros[indice].indiceColor = Self.normalizarColor(indiceColor)
            registros[indice].fechaModificacion = fecha
            try guardarSinBloqueo(registros)
            return registros[indice]
        }
    }

    func marcarModificado(id: String, fecha: Date = Date()) throws {
        try cola.sync {
            var registros = try leerSinBloqueo()
            guard let indice = registros.firstIndex(where: { $0.id == id }) else {
                // Los contextos heredados o de extensiones que todavía no son
                // cuadernos no deben convertir el autoguardado en un error.
                return
            }
            guard fecha > registros[indice].fechaModificacion else { return }
            registros[indice].fechaModificacion = fecha
            try guardarSinBloqueo(registros)
        }
    }

    @discardableResult
    func eliminar(id: String) throws -> CuadernoBibliotecaIPad {
        try cola.sync {
            var registros = try leerSinBloqueo()
            guard let indice = registros.firstIndex(where: { $0.id == id }) else {
                throw ErrorBibliotecaCuadernU.cuadernoNoEncontrado
            }
            var eliminados = try leerTombstonesSinBloqueo()
            eliminados.insert(id)
            // La intención durable va primero. Si la escritura posterior del
            // catálogo falla, la tarjeta original permanece; si el proceso se
            // corta después, una migración nunca resucita el cuaderno borrado.
            try guardarTombstonesSinBloqueo(eliminados)
            let eliminado = registros.remove(at: indice)
            try guardarSinBloqueo(registros)
            return eliminado
        }
    }

    func restaurar(_ registro: CuadernoBibliotecaIPad) throws {
        try cola.sync {
            var registros = try leerSinBloqueo()
            guard !registros.contains(where: { $0.id == registro.id }) else {
                return
            }
            registros.append(registro)
            try guardarSinBloqueo(registros)
            var eliminados = try leerTombstonesSinBloqueo()
            if eliminados.remove(registro.id) != nil {
                try guardarTombstonesSinBloqueo(eliminados)
            }
        }
    }

    static func normalizarTitulo(_ titulo: String) throws -> String {
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { throw ErrorBibliotecaCuadernU.tituloVacio }
        return String(limpio.prefix(120))
    }

    static func normalizarColor(_ indice: Int) -> Int {
        let resto = indice % cantidadColores
        return resto >= 0 ? resto : resto + cantidadColores
    }

    static func colorDeterminista(para id: String) -> Int {
        // FNV-1a es estable entre lanzamientos; `String.hashValue` no lo es y
        // `abs(Int.min)` puede provocar una trampa de ejecución.
        var hash: UInt64 = 14_695_981_039_346_656_037
        for byte in id.utf8 {
            hash ^= UInt64(byte)
            hash &*= 1_099_511_628_211
        }
        return Int(hash % UInt64(cantidadColores))
    }

    private static func identificadorSeguro(_ valor: String) throws -> String {
        let convertido = valor.map { caracter -> Character in
            if caracter.isLetter || caracter.isNumber || caracter == "-"
                || caracter == "_" {
                return caracter
            }
            return "_"
        }
        let resultado = String(convertido.prefix(128))
        guard !resultado.isEmpty else {
            throw ErrorBibliotecaCuadernU.identificadorInvalido
        }
        return resultado
    }

    private func crearDirectorioSinBloqueo() throws {
        try archivos.createDirectory(at: directorio,
                                     withIntermediateDirectories: true)
    }

    private func leerSinBloqueo() throws -> [CuadernoBibliotecaIPad] {
        guard archivos.fileExists(atPath: catalogo.path) else { return [] }
        do {
            let datos = try Data(contentsOf: catalogo)
            let decodificador = JSONDecoder()
            decodificador.dateDecodingStrategy = .millisecondsSince1970
            let registros = try decodificador.decode(
                [CuadernoBibliotecaIPad].self, from: datos)
            let ids = registros.map(\.id)
            guard Set(ids).count == ids.count,
                  registros.allSatisfy({
                      !$0.id.isEmpty && !$0.titulo.isEmpty
                          && (0..<Self.cantidadColores).contains($0.indiceColor)
                  }) else {
                throw ErrorBibliotecaCuadernU.catalogoDanado
            }
            return registros.sorted { $0.fechaModificacion > $1.fechaModificacion }
        } catch let error as ErrorBibliotecaCuadernU {
            throw error
        } catch {
            throw ErrorBibliotecaCuadernU.catalogoDanado
        }
    }

    private func guardarSinBloqueo(_ registros: [CuadernoBibliotecaIPad]) throws {
        try crearDirectorioSinBloqueo()
        let codificador = JSONEncoder()
        codificador.dateEncodingStrategy = .millisecondsSince1970
        codificador.outputFormatting = [.prettyPrinted, .sortedKeys]
        let ordenados = registros.sorted {
            $0.fechaModificacion > $1.fechaModificacion
        }
        let datos = try codificador.encode(ordenados)
        try datos.write(to: catalogo, options: .atomic)
    }

    private func leerTombstonesSinBloqueo() throws -> Set<String> {
        guard archivos.fileExists(atPath: tombstones.path) else { return [] }
        do {
            let datos = try Data(contentsOf: tombstones)
            let ids = try JSONDecoder().decode([String].self, from: datos)
            guard ids.count <= 100_000,
                  ids.allSatisfy({ !$0.isEmpty && $0.count <= 128 }) else {
                throw ErrorBibliotecaCuadernU.catalogoDanado
            }
            return Set(ids)
        } catch let error as ErrorBibliotecaCuadernU {
            throw error
        } catch {
            throw ErrorBibliotecaCuadernU.catalogoDanado
        }
    }

    private func guardarTombstonesSinBloqueo(_ ids: Set<String>) throws {
        try crearDirectorioSinBloqueo()
        let datos = try JSONEncoder().encode(ids.sorted())
        try datos.write(to: tombstones, options: .atomic)
    }
}

final class GestorArchivosCuadernU {
    static let compartido = GestorArchivosCuadernU()

    private let repositorio: RepositorioBibliotecaCuadernU

    private init() {
        let archivos = FileManager.default
        let soporte = archivos.urls(for: .applicationSupportDirectory,
                                    in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true)
                .appendingPathComponent("Library/Application Support",
                                        isDirectory: true)
        repositorio = RepositorioBibliotecaCuadernU(
            directorioBase: soporte
                .appendingPathComponent("CuadernU/Biblioteca", isDirectory: true))
    }

    /// Conserva configurada la ruta del núcleo C++ para compatibilidad con los
    /// documentos binarios existentes. La biblioteca nueva no usa su listado,
    /// porque aquel listado no estaba conectado con las pestañas que muestra el
    /// editor.
    func configurarRutaBase() {
        let archivos = FileManager.default
        let documentos = archivos.urls(for: .documentDirectory,
                                       in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true)
                .appendingPathComponent("Documents", isDirectory: true)
        documentos.path.withCString { tinta_almacen_configurar_ruta($0) }
    }

    func prepararBiblioteca(contextoAnterior: String) throws {
        let contextosAntesDeRecuperar: Set<String>?
        do {
            let existentes = try repositorio.listar()
            contextosAntesDeRecuperar = repositorio.catalogoExiste
                ? Set(existentes.map(\.id)) : nil
        } catch {
            // Con un catálogo ilegible no se puede decidir que un borrado ya
            // estaba confirmado. Se restaura todo el contenido transitorio.
            try recuperarEliminacionesPestanasIPad(contextosExistentes: nil)
            throw error
        }
        try recuperarEliminacionesPestanasIPad(
            contextosExistentes: contextosAntesDeRecuperar)
        try repositorio.prepararPrimeraEjecucion(
            contextoAnterior: contextoAnterior,
            contextosPersistidos: try contextosPersistidosPestanasIPad())
    }

    func listarCuadernos() throws -> [CuadernoBibliotecaIPad] {
        try repositorio.listar()
    }

    func crearCuaderno(titulo: String) throws -> CuadernoBibliotecaIPad {
        try repositorio.crear(titulo: titulo)
    }

    func renombrarCuaderno(id: String, nombre: String) throws {
        try repositorio.renombrar(id: id, titulo: nombre)
    }

    func actualizarColor(id: String, indiceColor: Int) throws {
        try repositorio.cambiarColor(id: id, indiceColor: indiceColor)
    }

    func marcarModificado(id: String) {
        do {
            try repositorio.marcarModificado(id: id)
        } catch {
            // No se interrumpe la escritura del lienzo por un fallo del índice;
            // se informa y el catálogo original permanece intacto.
            print("No se pudo actualizar la fecha del cuaderno: \(error.localizedDescription)")
        }
    }

    func eliminarCuaderno(id: String) throws {
        let transaccion = try prepararEliminacionPestanasIPad(contexto: id)
        do {
            _ = try repositorio.eliminar(id: id)
            do {
                try transaccion.confirmar()
            } catch {
                // Si no se pudo vaciar la carpeta transitoria, el contenido no
                // se pierde: queda aislado para recuperación en el siguiente
                // arranque. La tarjeta ya fue eliminada por solicitud expresa.
                print("La limpieza final del cuaderno quedó pendiente: \(error.localizedDescription)")
            }
        } catch {
            try? transaccion.revertir()
            throw error
        }
    }
}
