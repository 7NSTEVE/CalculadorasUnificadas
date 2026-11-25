import Combine
import Foundation

/// Espacios de trabajo por materia (puntos 35, 36, 92, 94, 120).
///
/// El problema que resuelve: Circuitos, Física y Cálculo no se estudian con la misma
/// calculadora ni con las mismas variables, y hoy todo cae en el mismo montón. Al
/// cambiar de materia hay que volver a poner el modo, volver a apuntar los datos y
/// buscar la sesión de la semana pasada entre todas.
///
/// Cada materia recuerda su calculadora, su cuaderno de sesiones y sus fórmulas. Lo
/// que NO recuerda es el estado de la máquina: eso son los puntos guardados, que ya
/// existen y llevan su propio volcado.

/// Una materia con lo suyo.
struct Materia: Identifiable, Codable, Equatable {
    var id: String { nombre }
    var nombre: String
    /// Identificador del modelo que se abre por defecto (punto 120).
    var modeloPreferido: String?
    /// Prefijo para los códigos de referencia. Si está vacío se deduce del nombre.
    var prefijo: String
    /// Número del último ejercicio, para que la numeración siga donde se quedó.
    var ultimoEjercicio: Int
    var creada: Date

    init(nombre: String, modeloPreferido: String? = nil, prefijo: String = "",
         ultimoEjercicio: Int = 1, creada: Date = Date()) {
        self.nombre = nombre
        self.modeloPreferido = modeloPreferido
        self.prefijo = prefijo.isEmpty ? Exportacion.prefijoMateria(nombre) : prefijo
        self.ultimoEjercicio = ultimoEjercicio
        self.creada = creada
    }
}

/// Una sesión archivada, con lo justo para encontrarla después.
///
/// No guarda la cinta entera: eso ya vive en el fichero de la sesión. Esto es el
/// índice, y tiene que ser pequeño para poder buscar en todo sin cargarlo todo.
struct SesionArchivada: Identifiable, Codable, Equatable {
    var id = UUID()
    var titulo: String
    var materia: String
    var modelo: String
    var fecha: Date
    /// Los resultados destacados, que es lo que se busca de verdad.
    var resultados: [String]
    /// Objetivo del ejercicio, que es como se recuerda una sesión.
    var objetivo: String
    var favorita: Bool

    /// Título automático cuando no se puso ninguno: fecha y objetivo.
    static func tituloAutomatico(objetivo: String, fecha: Date) -> String {
        let formato = DateFormatter()
        formato.dateFormat = "d MMM"
        formato.locale = Locale(identifier: "es")
        let dia = formato.string(from: fecha)
        return objetivo.isEmpty ? "Sesión del \(dia)" : "\(dia) · \(objetivo)"
    }
}

/// El cuaderno: materias y sesiones archivadas, con búsqueda sobre todo.
final class Cuaderno: ObservableObject {

    @Published private(set) var materias: [Materia] = []
    @Published private(set) var archivo: [SesionArchivada] = []
    /// Materia activa. Al cambiarla, la app ofrece su calculadora.
    @Published var materiaActiva: String = "General"

    private let claveMaterias = "cuaderno.materias"
    private let claveArchivo = "cuaderno.archivo"
    private let claveActiva = "cuaderno.activa"
    private let almacen: UserDefaults

    /// Tope de sesiones archivadas. Cada una son unos cientos de bytes, así que 200
    /// caben de sobra y cubren un semestre entero.
    static let maximoArchivo = 200

    init(almacen: UserDefaults = .standard) {
        self.almacen = almacen
        materias = Self.decodificar([Materia].self, almacen, claveMaterias) ?? []
        archivo = Self.decodificar([SesionArchivada].self, almacen, claveArchivo) ?? []
        materiaActiva = almacen.string(forKey: claveActiva) ?? "General"

        // Siempre tiene que haber una materia donde caer, o el primer ejercicio no
        // tendría dónde guardarse.
        if materias.isEmpty {
            materias = [Materia(nombre: "General")]
            guardar()
        }
    }

    private static func decodificar<T: Decodable>(_ tipo: T.Type, _ almacen: UserDefaults,
                                                  _ clave: String) -> T? {
        guard let datos = almacen.data(forKey: clave) else { return nil }
        return try? JSONDecoder().decode(tipo, from: datos)
    }

    private func guardar() {
        if let datos = try? JSONEncoder().encode(materias) {
            almacen.set(datos, forKey: claveMaterias)
        }
        if let datos = try? JSONEncoder().encode(archivo) {
            almacen.set(datos, forKey: claveArchivo)
        }
        almacen.set(materiaActiva, forKey: claveActiva)
    }

    // MARK: - Materias

    func materia(_ nombre: String) -> Materia? {
        materias.first { $0.nombre == nombre }
    }

    @discardableResult
    func crearMateria(_ nombre: String) -> Materia? {
        let limpio = nombre.trimmingCharacters(in: .whitespaces)
        guard !limpio.isEmpty, materia(limpio) == nil else { return nil }
        let nueva = Materia(nombre: limpio)
        materias.append(nueva)
        guardar()
        return nueva
    }

    func borrarMateria(_ nombre: String) {
        // No se puede quedar sin ninguna: sin materia no hay dónde archivar.
        guard materias.count > 1 else { return }
        materias.removeAll { $0.nombre == nombre }
        if materiaActiva == nombre { materiaActiva = materias.first?.nombre ?? "General" }
        guardar()
    }

    /// Recuerda con qué calculadora se trabaja esta materia (punto 120).
    func recordarModelo(_ modelo: String, para materia: String) {
        guard let indice = materias.firstIndex(where: { $0.nombre == materia }) else { return }
        materias[indice].modeloPreferido = modelo
        guardar()
    }

    func modeloPreferido(de materia: String) -> String? {
        self.materia(materia)?.modeloPreferido
    }

    /// Sube el contador de ejercicios de la materia y devuelve el nuevo número.
    @discardableResult
    func siguienteEjercicio(en materia: String) -> Int {
        guard let indice = materias.firstIndex(where: { $0.nombre == materia }) else { return 1 }
        materias[indice].ultimoEjercicio += 1
        guardar()
        return materias[indice].ultimoEjercicio
    }

    // MARK: - Archivo

    func archivar(_ sesion: SesionArchivada) {
        archivo.insert(sesion, at: 0)
        if archivo.count > Self.maximoArchivo { archivo.removeLast() }
        guardar()
    }

    func alternarFavorita(_ id: UUID) {
        guard let indice = archivo.firstIndex(where: { $0.id == id }) else { return }
        archivo[indice].favorita.toggle()
        guardar()
    }

    /// Búsqueda global: título, objetivo, materia y resultados (punto 93).
    ///
    /// Buscar por RESULTADO es el caso que de verdad pasa: te acuerdas del número
    /// —«aquello que daba 4,7 kΩ»— y no de cómo llamaste al ejercicio.
    func buscar(_ texto: String) -> [SesionArchivada] {
        let aguja = Formulas.normalizar(texto)
        guard !aguja.isEmpty else { return ordenadas(archivo) }

        return ordenadas(archivo.filter { sesion in
            Formulas.normalizar(sesion.titulo).contains(aguja)
                || Formulas.normalizar(sesion.objetivo).contains(aguja)
                || Formulas.normalizar(sesion.materia).contains(aguja)
                || sesion.resultados.contains { Formulas.normalizar($0).contains(aguja) }
        })
    }

    /// Favoritas arriba, y dentro de cada grupo la más reciente primero (punto 94).
    private func ordenadas(_ lista: [SesionArchivada]) -> [SesionArchivada] {
        lista.sorted { primera, segunda in
            if primera.favorita != segunda.favorita { return primera.favorita }
            return primera.fecha > segunda.fecha
        }
    }

    func sesionesDe(_ materia: String) -> [SesionArchivada] {
        ordenadas(archivo.filter { $0.materia == materia })
    }

    func limpiar() {
        materias = [Materia(nombre: "General")]
        archivo = []
        materiaActiva = "General"
        almacen.removeObject(forKey: claveMaterias)
        almacen.removeObject(forKey: claveArchivo)
        almacen.removeObject(forKey: claveActiva)
    }
}
