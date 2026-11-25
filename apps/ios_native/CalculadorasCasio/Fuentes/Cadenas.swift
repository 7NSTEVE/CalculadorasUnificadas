import Foundation

/// De dónde salió cada número.
///
/// El agujero que tapa esto es concreto: a mitad de un ejercicio largo tienes en
/// pantalla un `2,553191489E-03` y no te acuerdas de si era la corriente total o la
/// de la rama de arriba, ni de qué datos salió. La cinta ya guarda las teclas; lo que
/// faltaba es enlazar unas entradas con otras y poder ponerles nombre.
///
/// Todo esto se deduce del texto de la operación, sin tocar la emulación: se mira qué
/// nombres aparecen escritos. No adivina nada que no esté literalmente ahí.

/// Un enlace entre una operación y algo que usó.
struct Dependencia: Equatable, Identifiable {
    var id: String { "\(origen)->\(destino)" }
    /// De dónde viene el valor: `Ans`, `A`, `M`, o el nombre de una variable tuya.
    let origen: String
    /// Identificador de la entrada que lo usa, en texto para poder compararlo.
    let destino: String
    let clase: Clase

    enum Clase: String {
        /// `Ans`: el resultado inmediatamente anterior.
        case anterior
        /// Una memoria de la Casio: A…F, X, Y, M.
        case memoria
        /// Una variable del enunciado, de las que se apuntan en el panel.
        case variable
    }
}

/// Un eslabón de la cadena que llevó a un resultado.
struct Eslabon: Equatable, Identifiable {
    var id: String { entrada }
    /// Identificador de la entrada de cinta.
    let entrada: String
    /// Cómo se llama, si se le puso nombre: `Req`, `Itotal`…
    let etiqueta: String
    let expresion: String
    let resultado: String
    /// A qué profundidad está: 0 es el resultado final, 1 lo que lo alimentó, etc.
    let nivel: Int
}

enum Cadenas {

    /// Memorias que tiene la Casio. `Ans` va aparte porque significa otra cosa.
    static let memorias: Set<String> = ["A", "B", "C", "D", "E", "F", "X", "Y", "M"]

    /// Qué nombres aparecen escritos en una expresión.
    ///
    /// La regla es la de un identificador de toda la vida: **empieza por letra** y
    /// sigue con letras o dígitos. Eso resuelve los dos casos de golpe:
    ///
    ///   - `R1`, `R2`, `Vcc`, `I3` son nombres enteros. Un enunciado de circuitos no
    ///     tiene una sola resistencia, así que cortar en el dígito los partía todos.
    ///   - `1E-3` empieza por dígito, así que la `E` del exponente **no** se confunde
    ///     con la memoria `E` de la Casio. Sin esto, la app diría que un cálculo
    ///     depende de una memoria que nadie tocó.
    static func nombresEn(_ expresion: String) -> [String] {
        var nombres: [String] = []
        var actual = ""
        // Si el trozo que se está leyendo arrancó con un dígito, es un número.
        var esNumero = false

        func cerrar() {
            defer { actual = ""; esNumero = false }
            guard !actual.isEmpty, !esNumero else { return }
            if !nombres.contains(actual) { nombres.append(actual) }
        }

        for caracter in expresion {
            if caracter.isLetter || caracter.isNumber {
                if actual.isEmpty { esNumero = caracter.isNumber }
                actual.append(caracter)
            } else if caracter == "." || caracter == "," {
                // El separador decimal no parte el número: 2,55E-03 sigue siendo uno.
                if esNumero { actual.append(caracter) } else { cerrar() }
            } else {
                cerrar()
            }
        }
        cerrar()
        return nombres
    }

    /// Las dependencias de una entrada, dadas las variables que existen.
    static func dependencias(expresion: String, entrada: String,
                             variables: [String]) -> [Dependencia] {
        nombresEn(expresion).compactMap { nombre in
            if nombre == "Ans" {
                return Dependencia(origen: nombre, destino: entrada, clase: .anterior)
            }
            if memorias.contains(nombre) {
                return Dependencia(origen: nombre, destino: entrada, clase: .memoria)
            }
            if variables.contains(nombre) {
                return Dependencia(origen: nombre, destino: entrada, clase: .variable)
            }
            return nil
        }
    }

    /// Reconstruye la cadena que llevó hasta una entrada.
    ///
    /// `cinta` viene con la más reciente primero, igual que en pantalla. Una entrada
    /// que usa `Ans` depende de la que la precede; las que usan una memoria dependen
    /// de la última que escribió en esa memoria.
    ///
    /// Corta a `profundidadMaxima` a propósito: una cadena de veinte pasos no se lee,
    /// y el punto 19 del backlog pide justo lo contrario, tres pasos a la vista.
    static func cadena(hasta indice: Int, cinta: [(id: String, expresion: String,
                                                   resultado: String, etiqueta: String)],
                       profundidadMaxima: Int = 5) -> [Eslabon] {
        guard cinta.indices.contains(indice) else { return [] }

        var eslabones: [Eslabon] = []
        var actual = indice
        var nivel = 0
        var vistos: Set<String> = []

        while cinta.indices.contains(actual), nivel < profundidadMaxima {
            let entrada = cinta[actual]
            // Un ciclo no debería poder pasar, pero si la cinta viniera rara sería un
            // bucle infinito dentro de la interfaz.
            if vistos.contains(entrada.id) { break }
            vistos.insert(entrada.id)

            eslabones.append(Eslabon(entrada: entrada.id, etiqueta: entrada.etiqueta,
                                     expresion: entrada.expresion,
                                     resultado: entrada.resultado, nivel: nivel))

            // Solo se sigue hacia atrás si esta operación usó algo anterior. Si es
            // toda números, la cadena termina aquí: no vino de ningún sitio.
            let nombres = nombresEn(entrada.expresion)
            guard !nombres.isEmpty else { break }

            if nombres.contains("Ans") {
                actual += 1   // la anterior en el tiempo está una posición más abajo
            } else if let memoria = nombres.first(where: { memorias.contains($0) }) {
                guard let anterior = ultimaQueEscribio(memoria, cinta: cinta,
                                                       desde: actual + 1) else { break }
                actual = anterior
            } else {
                break
            }
            nivel += 1
        }
        return eslabones
    }

    /// La entrada más reciente (hacia atrás) cuya etiqueta es esa memoria.
    ///
    /// Se usa la etiqueta y no la expresión: guardar en `A` se hace con `STO A`, que
    /// en la cinta aparece como parte de las teclas, no como un nombre en la fórmula.
    private static func ultimaQueEscribio(_ memoria: String,
                                          cinta: [(id: String, expresion: String,
                                                   resultado: String, etiqueta: String)],
                                          desde: Int) -> Int? {
        guard desde < cinta.count else { return nil }
        for indice in desde..<cinta.count where cinta[indice].etiqueta == memoria {
            return indice
        }
        return nil
    }

    /// Qué datos del enunciado se han usado ya y cuáles no (punto 18).
    ///
    /// Un dato sin usar al final del ejercicio casi siempre significa una de dos:
    /// o sobraba, o te falta un paso. Las dos merecen mirarse.
    static func variablesSinUsar(variables: [String], expresiones: [String]) -> [String] {
        let usados = Set(expresiones.flatMap(nombresEn))
        return variables.filter { !usados.contains($0) }
    }

    /// Resumen de una línea para el botón «me perdí» (punto 25).
    static func resumenDeSituacion(objetivo: String, siguientePaso: String,
                                   ultimoResultado: String,
                                   sinUsar: [String]) -> [String] {
        var lineas: [String] = []
        if !objetivo.isEmpty { lineas.append("Buscas: \(objetivo)") }
        if !siguientePaso.isEmpty { lineas.append("Ibas por: \(siguientePaso)") }
        if !ultimoResultado.isEmpty { lineas.append("Lo último que salió: \(ultimoResultado)") }
        if !sinUsar.isEmpty {
            lineas.append("Todavía no has usado: \(sinUsar.joined(separator: ", "))")
        }
        if lineas.isEmpty {
            lineas.append("Todavía no hay nada apuntado. Escribe qué buscas y sigue.")
        }
        return lineas
    }
}
