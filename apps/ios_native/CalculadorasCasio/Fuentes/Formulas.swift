import Combine
import Foundation

/// Biblioteca de fórmulas, con despeje y sustitución.
///
/// La calculadora de dentro no cambia: esto genera la EXPRESIÓN que luego se teclea
/// con el traductor que ya existe. Es decir, la fórmula no la resuelve nadie por su
/// cuenta; la resuelve la Casio, que es de lo que se trataba.
///
/// El punto de todo el bloque es que en un examen no se falla por no saber la ley de
/// Ohm, se falla por sustituir mal: poner los 4,7 kΩ como 4,7 o olvidar una variable
/// y no darse cuenta hasta el final.

/// Una variable de una fórmula, con lo que hace falta para pedirla bien.
struct VariableFormula: Identifiable, Codable, Equatable {
    var id: String { simbolo }
    /// Cómo aparece en la fórmula: `V`, `R`, `I`, `f`…
    let simbolo: String
    /// Qué es, en palabras. Es el punto 75: nadie recuerda qué era `Q` en `f = 1/(2π√LC)`.
    let descripcion: String
    /// Símbolo de la unidad SI esperada, vacío si es adimensional.
    let unidad: String
}

/// Una fórmula despejable.
struct Formula: Identifiable, Codable, Equatable {
    let id: String
    let nombre: String
    let materia: String
    /// Cómo se escribe, para enseñarla arriba mientras se rellenan los huecos.
    let expresion: String
    let variables: [VariableFormula]

    /// Para cada variable despejable, la expresión que la calcula.
    ///
    /// Las claves son símbolos y los valores llevan los demás símbolos entre llaves:
    /// `"V": "{I}*{R}"`. Sustituir es reemplazo de texto, y por eso no hay que
    /// escribir un motor algebraico que además habría que verificar.
    let despejes: [String: String]

    /// Palabras por las que se busca además del nombre.
    let sinonimos: [String]

    /// Qué variables se pueden pedir como incógnita.
    var despejables: [VariableFormula] {
        variables.filter { despejes[$0.simbolo] != nil }
    }

    func descripcion(de simbolo: String) -> String? {
        variables.first { $0.simbolo == simbolo }?.descripcion
    }

    func unidad(de simbolo: String) -> String? {
        variables.first { $0.simbolo == simbolo }?.unidad
    }
}

/// Resultado de intentar sustituir valores en una fórmula.
enum Sustitucion: Equatable {
    /// Lista para teclear. `expresion` va en la sintaxis del traductor.
    case lista(expresion: String, pasos: [String])
    /// Faltan datos. Es el punto 77: se dice CUÁLES, no un "revisa los datos".
    case faltan([VariableFormula])
    /// La incógnita pedida no se puede despejar de esta fórmula.
    case noDespejable(String)
}

enum Formulas {

    // MARK: - Catálogo

    static let todas: [Formula] = [
        Formula(
            id: "ohm",
            nombre: "Ley de Ohm",
            materia: "Circuitos",
            expresion: "V = I · R",
            variables: [
                VariableFormula(simbolo: "V", descripcion: "tensión entre los extremos", unidad: "V"),
                VariableFormula(simbolo: "I", descripcion: "corriente que lo atraviesa", unidad: "A"),
                VariableFormula(simbolo: "R", descripcion: "resistencia", unidad: "Ω"),
            ],
            despejes: ["V": "{I}*{R}", "I": "{V}/{R}", "R": "{V}/{I}"],
            sinonimos: ["ohm", "voltaje", "tension", "corriente", "resistencia", "vir"]),

        Formula(
            id: "potencia_dc",
            nombre: "Potencia en continua",
            materia: "Circuitos",
            expresion: "P = V · I",
            variables: [
                VariableFormula(simbolo: "P", descripcion: "potencia disipada", unidad: "W"),
                VariableFormula(simbolo: "V", descripcion: "tensión", unidad: "V"),
                VariableFormula(simbolo: "I", descripcion: "corriente", unidad: "A"),
            ],
            despejes: ["P": "{V}*{I}", "V": "{P}/{I}", "I": "{P}/{V}"],
            sinonimos: ["potencia", "watt", "vatios", "disipacion"]),

        Formula(
            id: "divisor_tension",
            nombre: "Divisor de tensión",
            materia: "Circuitos",
            expresion: "Vout = Vin · R2 / (R1 + R2)",
            variables: [
                VariableFormula(simbolo: "Vout", descripcion: "tensión de salida", unidad: "V"),
                VariableFormula(simbolo: "Vin", descripcion: "tensión de entrada", unidad: "V"),
                VariableFormula(simbolo: "R1", descripcion: "resistencia de arriba", unidad: "Ω"),
                VariableFormula(simbolo: "R2", descripcion: "resistencia de abajo, la que mide", unidad: "Ω"),
            ],
            despejes: ["Vout": "{Vin}*{R2}/({R1}+{R2})"],
            sinonimos: ["divisor", "divisor de voltaje", "vout", "salida"]),

        Formula(
            id: "serie",
            nombre: "Resistencias en serie",
            materia: "Circuitos",
            expresion: "Req = R1 + R2",
            variables: [
                VariableFormula(simbolo: "Req", descripcion: "resistencia equivalente", unidad: "Ω"),
                VariableFormula(simbolo: "R1", descripcion: "primera resistencia", unidad: "Ω"),
                VariableFormula(simbolo: "R2", descripcion: "segunda resistencia", unidad: "Ω"),
            ],
            despejes: ["Req": "{R1}+{R2}"],
            sinonimos: ["serie", "equivalente", "suma de resistencias"]),

        Formula(
            id: "paralelo",
            nombre: "Resistencias en paralelo",
            materia: "Circuitos",
            expresion: "Req = R1 · R2 / (R1 + R2)",
            variables: [
                VariableFormula(simbolo: "Req", descripcion: "resistencia equivalente", unidad: "Ω"),
                VariableFormula(simbolo: "R1", descripcion: "primera resistencia", unidad: "Ω"),
                VariableFormula(simbolo: "R2", descripcion: "segunda resistencia", unidad: "Ω"),
            ],
            despejes: ["Req": "{R1}*{R2}/({R1}+{R2})"],
            sinonimos: ["paralelo", "equivalente", "producto sobre suma"]),

        Formula(
            id: "reactancia_c",
            nombre: "Reactancia capacitiva",
            materia: "Corriente alterna",
            expresion: "Xc = 1 / (2π · f · C)",
            variables: [
                VariableFormula(simbolo: "Xc", descripcion: "reactancia del condensador", unidad: "Ω"),
                VariableFormula(simbolo: "f", descripcion: "frecuencia", unidad: "Hz"),
                VariableFormula(simbolo: "C", descripcion: "capacidad", unidad: "F"),
            ],
            despejes: ["Xc": "1/(2*pi*{f}*{C})", "f": "1/(2*pi*{Xc}*{C})", "C": "1/(2*pi*{f}*{Xc})"],
            sinonimos: ["reactancia", "capacitiva", "condensador", "xc"]),

        Formula(
            id: "reactancia_l",
            nombre: "Reactancia inductiva",
            materia: "Corriente alterna",
            expresion: "Xl = 2π · f · L",
            variables: [
                VariableFormula(simbolo: "Xl", descripcion: "reactancia de la bobina", unidad: "Ω"),
                VariableFormula(simbolo: "f", descripcion: "frecuencia", unidad: "Hz"),
                VariableFormula(simbolo: "L", descripcion: "inductancia", unidad: "H"),
            ],
            despejes: ["Xl": "2*pi*{f}*{L}", "f": "{Xl}/(2*pi*{L})", "L": "{Xl}/(2*pi*{f})"],
            sinonimos: ["reactancia", "inductiva", "bobina", "xl"]),

        Formula(
            id: "resonancia",
            nombre: "Frecuencia de resonancia",
            materia: "Corriente alterna",
            expresion: "f = 1 / (2π · √(L·C))",
            variables: [
                VariableFormula(simbolo: "f", descripcion: "frecuencia de resonancia", unidad: "Hz"),
                VariableFormula(simbolo: "L", descripcion: "inductancia", unidad: "H"),
                VariableFormula(simbolo: "C", descripcion: "capacidad", unidad: "F"),
            ],
            despejes: ["f": "1/(2*pi*sqrt({L}*{C}))"],
            sinonimos: ["resonancia", "lc", "tanque", "frecuencia natural"]),

        Formula(
            id: "constante_rc",
            nombre: "Constante de tiempo RC",
            materia: "Transitorios",
            expresion: "τ = R · C",
            variables: [
                VariableFormula(simbolo: "T", descripcion: "constante de tiempo (tau)", unidad: "s"),
                VariableFormula(simbolo: "R", descripcion: "resistencia", unidad: "Ω"),
                VariableFormula(simbolo: "C", descripcion: "capacidad", unidad: "F"),
            ],
            despejes: ["T": "{R}*{C}", "R": "{T}/{C}", "C": "{T}/{R}"],
            sinonimos: ["tau", "constante de tiempo", "rc", "transitorio", "carga"]),
    ]

    // MARK: - Búsqueda

    /// Busca por nombre, materia, sinónimo o SÍMBOLO de variable.
    ///
    /// Lo de buscar por variable es el punto 74 y es el caso real: te dan `Xc` y no
    /// te acuerdas de cómo se llamaba la fórmula, pero el símbolo sí lo tienes
    /// delante en el enunciado.
    static func buscar(_ texto: String) -> [Formula] {
        let aguja = normalizar(texto)
        guard !aguja.isEmpty else { return todas }

        return todas.filter { formula in
            if normalizar(formula.nombre).contains(aguja) { return true }
            if normalizar(formula.materia).contains(aguja) { return true }
            if formula.sinonimos.contains(where: { normalizar($0).contains(aguja) }) { return true }
            return formula.variables.contains { normalizar($0.simbolo) == aguja }
        }
    }

    static func por(id: String) -> Formula? {
        todas.first { $0.id == id }
    }

    /// Sin tildes y en minúsculas: nadie escribe «tensión» con tilde en un buscador.
    static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive], locale: nil)
            .trimmingCharacters(in: .whitespaces)
    }

    // MARK: - Sustitución

    /// Sustituye los valores conocidos en el despeje de `incognita`.
    ///
    /// Devuelve además los pasos en texto —fórmula, sustitución— que es lo que se
    /// enseña encima del resultado para poder copiarlo al cuaderno.
    static func sustituir(_ formula: Formula, incognita: String,
                          valores: [String: String]) -> Sustitucion {
        guard let plantilla = formula.despejes[incognita] else {
            return .noDespejable(incognita)
        }

        // Se necesitan todas las variables que aparecen en el despeje, no todas las
        // de la fórmula: para `I = V/R` sobra saber cuánto vale `P`.
        let necesarias = simbolosEn(plantilla)
        let faltan = formula.variables.filter { variable in
            necesarias.contains(variable.simbolo) && vacio(valores[variable.simbolo])
        }
        guard faltan.isEmpty else { return .faltan(faltan) }

        var expresion = plantilla
        var sustituido = plantilla
        for simbolo in necesarias {
            guard let bruto = valores[simbolo] else { continue }
            // Se envuelve en paréntesis a propósito: sin ellos, un valor negativo o
            // en notación científica cambia la precedencia y da otro resultado.
            let limpio = normalizarNumero(bruto)
            expresion = expresion.replacingOccurrences(of: "{\(simbolo)}", with: "(\(limpio))")
            sustituido = sustituido.replacingOccurrences(of: "{\(simbolo)}", with: limpio)
        }

        // El despeje sin llaves, para enseñarlo tal cual se escribe a mano.
        let despejeLegible = plantilla
            .replacingOccurrences(of: "{", with: "")
            .replacingOccurrences(of: "}", with: "")

        let pasos = [
            formula.expresion,
            "\(incognita) = \(despejeLegible)",
            "\(incognita) = \(sustituido)",
        ]
        return .lista(expresion: expresion, pasos: pasos)
    }

    /// Acepta lo que escribe la gente: coma decimal y sufijos de ingeniería.
    ///
    /// `"4,7k"` entra como `4.7E3`. Sin esto, el error más común del mundo —meter
    /// 4,7 donde iban 4700— sigue estando a un descuido de distancia.
    static func normalizarNumero(_ texto: String) -> String {
        let limpio = texto.trimmingCharacters(in: .whitespaces)
        if let cantidad = Unidades.interpretar(limpio), cantidad.valor.isFinite {
            // Notación científica sin coma: es lo que entiende el traductor.
            return String(format: "%.12g", cantidad.valor)
        }
        return limpio.replacingOccurrences(of: ",", with: ".")
    }

    /// Símbolos entre llaves que aparecen en una plantilla de despeje.
    static func simbolosEn(_ plantilla: String) -> [String] {
        var encontrados: [String] = []
        var actual = ""
        var dentro = false
        for caracter in plantilla {
            if caracter == "{" { dentro = true; actual = ""; continue }
            if caracter == "}" {
                dentro = false
                if !actual.isEmpty && !encontrados.contains(actual) { encontrados.append(actual) }
                continue
            }
            if dentro { actual.append(caracter) }
        }
        return encontrados
    }

    private static func vacio(_ texto: String?) -> Bool {
        (texto ?? "").trimmingCharacters(in: .whitespaces).isEmpty
    }
}

/// Favoritos, recientes e historial por materia (puntos 72, 73, 78, 79).
///
/// Se guarda en `UserDefaults` y no en el fichero de sesión a propósito: la sesión es
/// de un ejercicio, y los favoritos son tuyos para todo el curso. Meterlos ahí los
/// borraría cada vez que se empieza un ejercicio nuevo.
final class MemoriaFormulas: ObservableObject {

    @Published private(set) var favoritas: [String] = []
    @Published private(set) var recientes: [String] = []
    /// Materia -> identificadores usados, para la hoja de «qué usé hoy».
    @Published private(set) var porMateria: [String: [String]] = [:]

    /// Cuántas recientes se recuerdan. Más de seis deja de ser una lista corta y
    /// vuelve a ser el catálogo entero, que ya está al lado.
    static let maximoRecientes = 6

    private let claveFavoritas = "formulas.favoritas"
    private let claveRecientes = "formulas.recientes"
    private let claveMateria = "formulas.porMateria"
    private let almacen: UserDefaults

    init(almacen: UserDefaults = .standard) {
        self.almacen = almacen
        favoritas = almacen.stringArray(forKey: claveFavoritas) ?? []
        recientes = almacen.stringArray(forKey: claveRecientes) ?? []
        porMateria = (almacen.dictionary(forKey: claveMateria) as? [String: [String]]) ?? [:]
    }

    func esFavorita(_ id: String) -> Bool { favoritas.contains(id) }

    func alternarFavorita(_ id: String) {
        if let indice = favoritas.firstIndex(of: id) {
            favoritas.remove(at: indice)
        } else {
            favoritas.append(id)
        }
        almacen.set(favoritas, forKey: claveFavoritas)
    }

    /// Se llama al usar una fórmula de verdad, no al mirarla: una lista de recientes
    /// llena de cosas que solo abriste por curiosidad no sirve para nada.
    func registrarUso(_ id: String, materia: String) {
        recientes.removeAll { $0 == id }
        recientes.insert(id, at: 0)
        if recientes.count > Self.maximoRecientes { recientes.removeLast() }
        almacen.set(recientes, forKey: claveRecientes)

        var deLaMateria = porMateria[materia] ?? []
        deLaMateria.removeAll { $0 == id }
        deLaMateria.insert(id, at: 0)
        porMateria[materia] = deLaMateria
        almacen.set(porMateria, forKey: claveMateria)
    }

    /// Las fórmulas usadas en una materia, resueltas a objetos.
    func usadasEn(_ materia: String) -> [Formula] {
        (porMateria[materia] ?? []).compactMap(Formulas.por(id:))
    }

    /// Hoja de «qué fórmulas usé» para pegar en el cuaderno (punto 79).
    func resumen(materia: String) -> String {
        let usadas = usadasEn(materia)
        guard !usadas.isEmpty else { return "Todavía no has usado ninguna fórmula en \(materia)." }
        var lineas = ["Fórmulas usadas en \(materia):"]
        lineas.append(contentsOf: usadas.map { "- \($0.nombre): \($0.expresion)" })
        return lineas.joined(separator: "\n")
    }

    /// Borra todo. Existe para poder probarlo sin dejar basura en las preferencias.
    func limpiar() {
        favoritas = []; recientes = []; porMateria = [:]
        almacen.removeObject(forKey: claveFavoritas)
        almacen.removeObject(forKey: claveRecientes)
        almacen.removeObject(forKey: claveMateria)
    }
}
