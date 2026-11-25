import Combine
import Foundation

/// Una operación ya cerrada: qué se tecleó y qué salió.
struct EntradaCinta: Identifiable, Codable {
    let id: UUID
    let teclas: String
    let resultado: String
    let modo: String
    let momento: Date

    /// Instantánea del LCD en el momento del `=`. Se guarda para poder copiar la
    /// pantalla auténtica de la Casio, no una recreación con tipografía del sistema.
    let pixeles: [UInt8]
    let ancho: Int
    let alto: Int

    init(teclas: String, resultado: String, modo: String, momento: Date,
         pixeles: [UInt8], ancho: Int, alto: Int) {
        self.id = UUID()
        self.teclas = teclas
        self.resultado = resultado
        self.modo = modo
        self.momento = momento
        self.pixeles = pixeles
        self.ancho = ancho
        self.alto = alto
    }

    // MARK: - Persistencia

    private enum Clave: String, CodingKey {
        case id, teclas, resultado, modo, momento, ancho, alto, puntos
    }

    /// En disco los píxeles van a bit por punto, no a byte.
    ///
    /// Un LCD de ClassWiz son 12.096 puntos. Como bytes ocupan 16 KB en base64 y
    /// como bits 2 KB; multiplicado por las entradas que se conservan, es la
    /// diferencia entre un fichero de 800 KB y uno de 100 KB.
    init(from decodificador: Decoder) throws {
        let caja = try decodificador.container(keyedBy: Clave.self)
        id = try caja.decode(UUID.self, forKey: .id)
        teclas = try caja.decode(String.self, forKey: .teclas)
        resultado = try caja.decode(String.self, forKey: .resultado)
        modo = try caja.decode(String.self, forKey: .modo)
        momento = try caja.decode(Date.self, forKey: .momento)
        ancho = try caja.decode(Int.self, forKey: .ancho)
        alto = try caja.decode(Int.self, forKey: .alto)
        let empaquetados = try caja.decode(Data.self, forKey: .puntos)
        pixeles = EntradaCinta.desempaquetar(empaquetados, cantidad: max(0, ancho * alto))
    }

    func encode(to codificador: Encoder) throws {
        var caja = codificador.container(keyedBy: Clave.self)
        try caja.encode(id, forKey: .id)
        try caja.encode(teclas, forKey: .teclas)
        try caja.encode(resultado, forKey: .resultado)
        try caja.encode(modo, forKey: .modo)
        try caja.encode(momento, forKey: .momento)
        try caja.encode(ancho, forKey: .ancho)
        try caja.encode(alto, forKey: .alto)
        try caja.encode(EntradaCinta.empaquetar(pixeles), forKey: .puntos)
    }

    static func empaquetar(_ pixeles: [UInt8]) -> Data {
        var datos = Data(repeating: 0, count: (pixeles.count + 7) / 8)
        for (indice, punto) in pixeles.enumerated() where punto != 0 {
            datos[indice / 8] |= UInt8(1 << (7 - indice % 8))
        }
        return datos
    }

    static func desempaquetar(_ datos: Data, cantidad: Int) -> [UInt8] {
        guard cantidad > 0 else { return [] }
        var pixeles = [UInt8](repeating: 0, count: cantidad)
        for indice in 0..<cantidad {
            let byte = indice / 8
            guard byte < datos.count else { break }
            if datos[datos.startIndex + byte] & UInt8(1 << (7 - indice % 8)) != 0 {
                pixeles[indice] = 1
            }
        }
        return pixeles
    }
}

/// Un valor del ejercicio, con el nombre que tiene en el enunciado.
///
/// La Casio solo tiene `A`…`F`, `X`, `Y`, `M`. Aquí al lado se apunta qué es cada
/// cosa, que es lo que de verdad se olvida: no el número, sino a cuál correspondía.
struct VariableEstudio: Identifiable, Codable, Equatable {
    var id = UUID()
    var nombre: String
    var valor: String
    var unidad: String

    var etiqueta: String {
        unidad.isEmpty ? "\(nombre) = \(valor)" : "\(nombre) = \(valor) \(unidad)"
    }
}

/// El estado completo de un momento del ejercicio, para poder volver a él.
///
/// Guarda las dos cosas: el volcado del chipset, que devuelve la máquina a este
/// instante al momento, y la secuencia de teclas desde el encendido, que sirve de
/// respaldo si el volcado no vale (otra ROM, otra versión del formato) y además es
/// lo que se enseña cuando preguntas «¿cómo llegué aquí?».
struct PuntoGuardado: Identifiable, Codable {
    var id = UUID()
    var nota: String
    var momento: Date
    var objetivo: String
    var siguientePaso: String
    var teclas: [UInt8]
    var resumen: String
    var modo: String

    /// Volcado completo del chipset en ese instante, si se pudo tomar.
    ///
    /// Con él, volver al punto es inmediato (~0,3 ms medidos). Sin él hay que
    /// reproducir las teclas desde el encendido, a unos 20 ms cada una. Es opcional
    /// porque los puntos guardados antes de que existieran las instantáneas siguen
    /// siendo válidos: solo son lentos.
    var estado: Data?

    /// Cuánto tarda en restaurarse, para poder avisar antes de empezar.
    ///
    /// Con volcado no se avisa de nada: es imperceptible.
    var segundosEstimados: Double { estado != nil ? 0 : Double(teclas.count) * 0.02 }

    /// Si volver aquí es instantáneo o hay que esperar a la reproducción.
    var esInmediato: Bool { estado != nil }
}

/// Lo que se enseña al volver a la app después de un rato fuera.
struct Recordatorio {
    let objetivo: String
    let siguientePaso: String
    let ultimo: EntradaCinta?
    let ausencia: TimeInterval
}

/// La capa de estudio que envuelve a la calculadora.
///
/// No toca la emulación: solo observa las teclas que pasan y lee la pantalla. La
/// Casio de dentro sigue siendo la de siempre.
///
/// Existe por un motivo concreto: cuando saltas a OneNote y vuelves, lo caro no es
/// repetir el cálculo, es reconstruir en la cabeza qué estabas haciendo. Todo lo de
/// aquí va a que eso deje de pasar.
final class SesionEstudio: ObservableObject {

    /// Lo que persigues ahora mismo. Lo escribes tú; nadie lo adivina.
    @Published var objetivo: String = ""

    /// El paso concreto en el que estás. Se escribe en cinco segundos y evita el
    /// "¿y ahora qué tocaba?" al volver de otra app.
    @Published var siguientePaso: String = ""

    /// Cinta de cálculo, como la de una calculadora de impresora: la más reciente
    /// arriba. Evita el "¿de dónde saqué este número?".
    @Published private(set) var cinta: [EntradaCinta] = []

    /// Teclas de la operación en curso, todavía sin `=`.
    @Published private(set) var enCurso: [String] = []

    /// El estante de valores del ejercicio, siempre a la vista.
    @Published var variables: [VariableEstudio] = []

    /// Puntos a los que se puede volver, el más reciente primero.
    @Published private(set) var puntos: [PuntoGuardado] = []

    /// Resultados clavados a la vista mientras haces otro cálculo.
    ///
    /// Resuelve el caso de «necesito esta corriente para el siguiente paso pero la
    /// pantalla ya la borré». Se limitan a unos pocos a propósito: si caben veinte,
    /// deja de ser un recordatorio y vuelve a ser una lista que hay que leer.
    @Published private(set) var fijados: [EntradaCinta] = []

    static let maximoFijados = 4

    /// Resultados marcados con estrella: los que se van al cuaderno.
    ///
    /// Es distinto de `fijados`. Fijar es «lo necesito a la vista AHORA»; destacar es
    /// «esto es un resultado del ejercicio, guárdalo». Una cinta de una tarde tiene
    /// cincuenta líneas y al cuaderno van cuatro.
    @Published private(set) var destacados: Set<UUID> = []

    /// Materia en curso. De aquí salen el prefijo del código de referencia y, más
    /// adelante, el espacio de trabajo.
    @Published var materia: String = "General"

    /// Número de ejercicio dentro de la materia, para las referencias tipo CIR-04-R3.
    @Published var ejercicio: Int = 1

    func estaDestacado(_ entrada: EntradaCinta) -> Bool {
        destacados.contains(entrada.id)
    }

    func alternarDestacado(_ entrada: EntradaCinta) {
        if destacados.contains(entrada.id) {
            destacados.remove(entrada.id)
        } else {
            destacados.insert(entrada.id)
        }
        guardar()
    }

    /// Las líneas listas para exportar, con su numeración y su código corto.
    func lineasExportables(alcance: AlcanceExportacion) -> [LineaExportable] {
        Exportacion.preparar(cinta: cinta, destacadas: destacados, alcance: alcance,
                             materia: materia, ejercicio: ejercicio)
    }

    /// Se enseña al volver a la app tras un rato fuera.
    @Published var recordatorio: Recordatorio?

    /// Cómo estaba la calculadora la última vez que se salió de la app.
    ///
    /// Se guarda siempre, aunque no se aplique solo: reproducirlo cuesta segundos y
    /// arrancar la app con una espera obligatoria sería peor que el problema. La
    /// vista lo ofrece y decide quien la usa.
    @Published private(set) var reanudacion: PuntoGuardado?

    private let modelo: String
    private var ultimaSalida: Date?

    /// Cuántas entradas de cinta sobreviven al cierre de la app. Cada una son ~2 KB
    /// de píxeles empaquetados, así que 60 son unos 120 KB de fichero.
    private static let entradasPersistidas = 60
    private static let entradasEnMemoria = 200

    /// Cola **serie** a propósito. Con la cola global concurrente, dos guardados
    /// seguidos —pulsar `=` y salir a OneNote acto seguido— podían terminar en
    /// orden inverso y dejar en disco el estado viejo pisando al nuevo.
    private static let colaDisco = DispatchQueue(label: "estudio.disco", qos: .utility)

    init(modelo: String) {
        self.modelo = modelo
        cargar()
    }

    // MARK: - Registro

    func registrar(tecla: String) {
        // AC arranca de cero: lo tecleado hasta ahora ya no lleva a ningún sitio.
        if tecla == "AC" {
            enCurso.removeAll()
            return
        }
        if tecla == "DEL" {
            if !enCurso.isEmpty { enCurso.removeLast() }
            return
        }
        enCurso.append(simbolo(para: tecla))
    }

    /// Cierra la operación en curso cuando se pulsa `=` y ya se pudo leer la pantalla.
    func cerrar(resultado: String, modo: String,
                pixeles: [UInt8], ancho: Int, alto: Int) {
        guard !enCurso.isEmpty || !resultado.isEmpty else { return }
        let entrada = EntradaCinta(
            teclas: enCurso.joined(separator: " "),
            resultado: resultado,
            modo: modo,
            momento: Date(),
            pixeles: pixeles,
            ancho: ancho,
            alto: alto
        )
        cinta.insert(entrada, at: 0)
        enCurso.removeAll()
        if cinta.count > Self.entradasEnMemoria { cinta.removeLast() }
        guardar()
    }

    func deshacerUltimaTecla() {
        if !enCurso.isEmpty { enCurso.removeLast() }
    }

    /// Cierra el ejercicio actual sin perderlo: antes de limpiar deja un punto al
    /// que volver. Vaciar el espacio de trabajo no puede costar información.
    func nuevoEjercicio(teclas: [UInt8]) {
        if !objetivo.isEmpty || !cinta.isEmpty {
            anotarPunto(nota: objetivo.isEmpty ? "Ejercicio anterior" : objetivo,
                        teclas: teclas)
        }
        enCurso.removeAll()
        objetivo = ""
        siguientePaso = ""
        cinta.removeAll()
        variables.removeAll()
        fijados.removeAll()
        recordatorio = nil
        reanudacion = nil
        guardar()
    }

    // MARK: - Puntos guardados

    func anotarPunto(nota: String, teclas: [UInt8], estado: Data? = nil) {
        let punto = PuntoGuardado(
            nota: nota.isEmpty ? "Punto sin nombre" : nota,
            momento: Date(),
            objetivo: objetivo,
            siguientePaso: siguientePaso,
            teclas: teclas,
            resumen: cinta.first?.resultado ?? "",
            modo: cinta.first?.modo ?? "",
            estado: estado
        )
        puntos.insert(punto, at: 0)
        if puntos.count > 30 { puntos.removeLast() }
        guardar()
    }

    /// Devuelve el objetivo y el paso al estado que tenían en ese punto. Reproducir
    /// las teclas en la calculadora es cosa del emulador, no de aquí.
    func adoptar(_ punto: PuntoGuardado) {
        objetivo = punto.objetivo
        siguientePaso = punto.siguientePaso
        enCurso.removeAll()
        guardar()
    }

    func borrarPunto(_ punto: PuntoGuardado) {
        puntos.removeAll { $0.id == punto.id }
        guardar()
    }

    // MARK: - Variables

    func anadirVariable() {
        variables.append(VariableEstudio(nombre: "", valor: "", unidad: ""))
    }

    func borrarVariable(_ variable: VariableEstudio) {
        variables.removeAll { $0.id == variable.id }
        guardar()
    }

    // MARK: - Continuidad

    func salioDeLaApp(teclas: [UInt8], estado: Data? = nil) {
        ultimaSalida = Date()
        if !teclas.isEmpty {
            reanudacion = PuntoGuardado(
                nota: "Como lo dejaste",
                momento: Date(),
                objetivo: objetivo,
                siguientePaso: siguientePaso,
                teclas: teclas,
                resumen: cinta.first?.resultado ?? "",
                modo: cinta.first?.modo ?? "",
                estado: estado
            )
        }
        guardar()
    }

    /// Se llama en cuanto se acepta o se descarta la oferta de reanudar, para que
    /// no vuelva a aparecer en la misma sesión.
    func descartarReanudacion() {
        reanudacion = nil
        guardar()
    }

    /// Al volver, si estuviste fuera lo suficiente, se recuerda dónde ibas.
    func volvioALaApp() {
        guard let salida = ultimaSalida else { return }
        let fuera = Date().timeIntervalSince(salida)
        ultimaSalida = nil
        guard fuera > 20 else { return }
        guard !objetivo.isEmpty || !siguientePaso.isEmpty || !cinta.isEmpty else { return }
        recordatorio = Recordatorio(objetivo: objetivo,
                                    siguientePaso: siguientePaso,
                                    ultimo: cinta.first,
                                    ausencia: fuera)
    }

    // MARK: - Persistencia

    /// Todo lo que sobrevive a que el sistema mate el proceso.
    private struct Estado: Codable {
        /// Sube cuando el formato deja de ser compatible. Sirve para no confundir
        /// «fichero de una versión que no entiendo» con «fichero roto».
        ///
        /// Opcional porque Swift **no** aplica el valor por defecto de una propiedad
        /// cuando la clave falta en el JSON: si no lo fuera, los ficheros escritos
        /// antes de que existiera este campo dejarían de decodificar enteros.
        var version: Int?
        static let actual = 1

        var objetivo: String
        var siguientePaso: String
        var cinta: [EntradaCinta]
        var variables: [VariableEstudio]
        var puntos: [PuntoGuardado]
        /// Opcional a propósito: así los ficheros escritos antes de que existiera
        /// siguen decodificando en vez de perderse enteros.
        var reanudacion: PuntoGuardado?
        /// Opcional por lo mismo que `reanudacion`.
        var fijados: [EntradaCinta]?
        /// Resultados marcados con estrella. Opcional por lo mismo.
        var destacados: [UUID]?
        /// Materia activa, para el código de referencia y los espacios de trabajo.
        var materia: String?
        /// Cuántos ejercicios se llevan en esta materia, para numerar las referencias.
        var ejercicio: Int?
    }

    private var fichero: URL? {
        let gestor = FileManager.default
        guard let base = try? gestor.url(for: .applicationSupportDirectory,
                                         in: .userDomainMask,
                                         appropriateFor: nil, create: true) else { return nil }
        let carpeta = base.appendingPathComponent("Estudio", isDirectory: true)
        if !gestor.fileExists(atPath: carpeta.path) {
            try? gestor.createDirectory(at: carpeta, withIntermediateDirectories: true)
        }
        return carpeta.appendingPathComponent("\(modelo).json")
    }

    /// Escribe en disco fuera del hilo principal: son ~120 KB de JSON y esto se
    /// llama al pulsar `=` y al salir de la app, donde una pausa se nota.
    ///
    /// Lo que se teclea en «objetivo» y «ahora» no se guarda letra a letra: se
    /// persiste en la siguiente operación o al salir de la app, que es cuando
    /// importa que esté a salvo.
    func guardar() {
        let estado = Estado(version: Estado.actual,
                            objetivo: objetivo,
                            siguientePaso: siguientePaso,
                            cinta: Array(cinta.prefix(Self.entradasPersistidas)),
                            variables: variables,
                            puntos: puntos,
                            reanudacion: reanudacion,
                            fijados: fijados,
                            destacados: Array(destacados),
                            materia: materia,
                            ejercicio: ejercicio)
        guard let destino = fichero else { return }
        Self.colaDisco.async {
            guard let datos = try? JSONEncoder().encode(estado) else { return }
            try? datos.write(to: destino, options: .atomic)
        }
    }

    /// `true` si al abrir había un fichero que no se pudo leer. La vista lo dice en
    /// vez de dejar creer que nunca hubo nada.
    private(set) var sesionIlegible = false

    private func cargar() {
        guard let origen = fichero, let datos = try? Data(contentsOf: origen) else { return }

        guard let estado = try? JSONDecoder().decode(Estado.self, from: datos) else {
            // Empezar en blanco vale; borrar el fichero, no. Se aparta con otro
            // nombre por si algún día se puede rescatar a mano.
            sesionIlegible = true
            let apartado = origen.appendingPathExtension("ilegible")
            try? FileManager.default.removeItem(at: apartado)
            try? FileManager.default.moveItem(at: origen, to: apartado)
            return
        }

        guard estado.version ?? 1 <= Estado.actual else {
            // Fichero escrito por una versión más nueva de la app. Cargarlo a
            // medias sería peor que no cargarlo.
            sesionIlegible = true
            return
        }

        objetivo = estado.objetivo
        siguientePaso = estado.siguientePaso
        cinta = estado.cinta
        variables = estado.variables
        puntos = estado.puntos
        reanudacion = estado.reanudacion
        fijados = estado.fijados ?? []
        destacados = Set(estado.destacados ?? [])
        materia = estado.materia ?? "General"
        ejercicio = estado.ejercicio ?? 1
    }

    // MARK: - Resultados fijados

    func estaFijado(_ entrada: EntradaCinta) -> Bool {
        fijados.contains { $0.id == entrada.id }
    }

    /// Clava o desclava un resultado. Al llegar al tope se cae el más antiguo, en vez
    /// de negarse: quien fija uno nuevo quiere ver ese, no que le digan que no cabe.
    func alternarFijado(_ entrada: EntradaCinta) {
        if let indice = fijados.firstIndex(where: { $0.id == entrada.id }) {
            fijados.remove(at: indice)
        } else {
            fijados.insert(entrada, at: 0)
            if fijados.count > Self.maximoFijados { fijados.removeLast() }
        }
        guardar()
    }

    // MARK: - Presentación

    /// Traduce el nombre interno de la tecla a algo legible en la cinta.
    private func simbolo(para tecla: String) -> String {
        switch tecla {
        case "MAS": return "+"
        case "MENOS": return "−"
        case "POR": return "×"
        case "ENTRE": return "÷"
        case "IGUAL": return "="
        case "PUNTO": return "."
        case "COMA": return ","
        case "PAREN_ABRE": return "("
        case "PAREN_CIERRA": return ")"
        case "NEGATIVO": return "(−)"
        case "FRACCION": return "▭/▭"
        case "RAIZ": return "√"
        case "CUADRADO": return "x²"
        case "POTENCIA": return "^"
        case "INVERSO": return "x⁻¹"
        case "EXP": return "×10^"
        case "SIN": return "sin"
        case "COS": return "cos"
        case "TAN": return "tan"
        case "LOG": return "log"
        case "LN": return "ln"
        case "LOG_B": return "log_"
        case "ANS": return "Ans"
        default: return tecla
        }
    }
}
