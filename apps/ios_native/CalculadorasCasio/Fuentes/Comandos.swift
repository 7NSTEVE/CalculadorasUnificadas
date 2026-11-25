import Foundation

/// Catálogo de acciones con nombre, para los atajos y la paleta (puntos 137 y 138).
///
/// Existe como catálogo y no como una lista de botones sueltos por un motivo
/// práctico: cada acción tiene que estar en tres sitios a la vez —el menú, el atajo
/// de teclado y la paleta `Cmd+K`— y mantener tres listas paralelas es cómo acaban
/// desincronizadas. Aquí se declara una vez y los tres la leen.
///
/// El teclado Bluetooth importa de verdad en el iPad con funda: escribir números con
/// las teclas físicas y no con el cristal es más rápido y no tapa la pantalla.

/// Una acción invocable por nombre.
struct Comando: Identifiable, Equatable {
    var id: String { clave }
    /// Identificador estable, para guardar la reasignación de atajos.
    let clave: String
    let titulo: String
    /// Palabras por las que se encuentra en la paleta.
    let sinonimos: [String]
    /// Tecla del atajo por defecto, sin modificadores. Vacío si no tiene.
    let atajo: String
    /// Si el atajo lleva Cmd. Casi todos sí; los de teclado numérico no.
    let conComando: Bool
    let simbolo: String

    /// Cómo se escribe el atajo para enseñarlo: `⌘R`.
    var atajoLegible: String {
        guard !atajo.isEmpty else { return "" }
        return (conComando ? "⌘" : "") + atajo.uppercased()
    }
}

enum Comandos {

    /// Todo lo que se puede pedir por nombre.
    ///
    /// Los atajos evitan a propósito los que iOS ya usa (⌘C, ⌘V, ⌘Z, ⌘W): pisarlos
    /// haría que copiar dejara de funcionar, y eso se nota mucho más de lo que aporta
    /// el atajo nuevo.
    static let todos: [Comando] = [
        Comando(clave: "grafica", titulo: "Graficar",
                sinonimos: ["grafica", "curva", "dibujar", "plot"],
                atajo: "G", conComando: true, simbolo: "waveform.path.ecg"),
        Comando(clave: "formulas", titulo: "Fórmulas",
                sinonimos: ["formula", "ecuacion", "despejar", "ohm"],
                atajo: "F", conComando: true, simbolo: "function"),
        Comando(clave: "exportar", titulo: "Exportar",
                sinonimos: ["exportar", "copiar", "onenote", "markdown", "latex"],
                atajo: "E", conComando: true, simbolo: "square.and.arrow.up"),
        Comando(clave: "rebobinar", titulo: "Rebobinar",
                sinonimos: ["rebobinar", "atras", "deshacer", "undo", "historial"],
                atajo: "R", conComando: true, simbolo: "backward.end.alt"),
        Comando(clave: "punto", titulo: "Guardar punto",
                sinonimos: ["punto", "guardar", "checkpoint", "marcar"],
                atajo: "S", conComando: true, simbolo: "bookmark"),
        Comando(clave: "meperdi", titulo: "Me perdí",
                sinonimos: ["perdido", "donde estaba", "ayuda", "contexto"],
                atajo: "?", conComando: true, simbolo: "questionmark.bubble"),
        Comando(clave: "cinta", titulo: "Cinta de cálculo",
                sinonimos: ["cinta", "historial", "operaciones"],
                atajo: "L", conComando: true, simbolo: "list.bullet.rectangle"),
        Comando(clave: "procedimientos", titulo: "¿Cómo hago…?",
                sinonimos: ["como", "procedimiento", "matriz", "estadistica", "menu"],
                atajo: "H", conComando: true, simbolo: "questionmark.circle"),
        Comando(clave: "plano", titulo: "Plano complejo",
                sinonimos: ["complejo", "fasor", "polar", "imaginario"],
                atajo: "I", conComando: true, simbolo: "chart.dots.scatter"),
        Comando(clave: "intentos", titulo: "Intentos",
                sinonimos: ["intento", "rama", "arbol", "bifurcar"],
                atajo: "T", conComando: true, simbolo: "arrow.triangle.branch"),
        Comando(clave: "nuevo", titulo: "Nuevo ejercicio",
                sinonimos: ["nuevo", "limpiar", "empezar"],
                atajo: "N", conComando: true, simbolo: "square.and.pencil"),
        Comando(clave: "diagnostico", titulo: "Qué máquina es esta",
                sinonimos: ["diagnostico", "rom", "fidelidad", "version", "bug"],
                atajo: "D", conComando: true, simbolo: "checkmark.seal"),
    ]

    static func por(clave: String) -> Comando? {
        todos.first { $0.clave == clave }
    }

    /// Búsqueda de la paleta: por título o por sinónimo, sin tildes ni mayúsculas.
    static func buscar(_ texto: String) -> [Comando] {
        let aguja = Formulas.normalizar(texto)
        guard !aguja.isEmpty else { return todos }
        return todos.filter { comando in
            Formulas.normalizar(comando.titulo).contains(aguja)
                || comando.sinonimos.contains { Formulas.normalizar($0).contains(aguja) }
        }
    }

    /// Qué comando corresponde a una tecla pulsada con ⌘.
    static func porAtajo(_ tecla: String) -> Comando? {
        let buscada = tecla.uppercased()
        return todos.first { $0.conComando && $0.atajo.uppercased() == buscada }
    }

    /// Teclas físicas que escriben directamente en la calculadora.
    ///
    /// Es el mapa del teclado Bluetooth: lo que se teclea sin ⌘ va a la Casio, que es
    /// lo que se espera al escribir números con una funda-teclado.
    static let mapaTeclado: [String: String] = [
        "0": "0", "1": "1", "2": "2", "3": "3", "4": "4",
        "5": "5", "6": "6", "7": "7", "8": "8", "9": "9",
        "+": "MAS", "-": "MENOS", "*": "POR", "/": "ENTRE",
        ".": "COMA", ",": "COMA",
        "(": "PAREN_ABRE", ")": "PAREN_CIERRA",
        "=": "IGUAL", "\r": "IGUAL", "\n": "IGUAL",
        "\u{8}": "DEL",       // retroceso
        "\u{1B}": "AC",       // escape
        "^": "POTENCIA",
    ]

    /// Nombre de tecla de la calculadora para un carácter del teclado físico.
    static func teclaPara(_ caracter: String) -> String? {
        mapaTeclado[caracter]
    }
}
