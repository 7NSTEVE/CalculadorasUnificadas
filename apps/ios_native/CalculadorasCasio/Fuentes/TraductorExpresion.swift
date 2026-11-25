import Foundation

/// Convierte una expresión escrita —la que copias de OneNote— en la secuencia de
/// teclas que hay que pulsar en la Casio.
///
/// No calcula nada. La cuenta la sigue haciendo el firmware original: esto solo
/// decide qué teclas se pulsan y en qué orden, que es justo la parte que uno no se
/// sabe de memoria.
///
/// Emite **nombres lógicos** («RAIZ», «PAREN_ABRE»), no códigos de matriz. Así el
/// fichero no depende de CoreGraphics ni de la disposición, y se puede probar fuera
/// de iOS; traducir nombre a código es cosa de `Disposiciones`.
enum TraductorExpresion {

    enum Modelo {
        case esPlus     // fx-570VN PLUS
        case classWiz   // fx-570/991LA X

        /// La LA X es el modelo latinoamericano y usa coma decimal: la misma tecla
        /// física, pero en la disposición se llama distinto.
        var separadorDecimal: String {
            switch self {
            case .esPlus: return "PUNTO"
            case .classWiz: return "COMA"
            }
        }

        /// Cómo se teclea `log` en base 10 en cada máquina. Comprobado sobre las
        /// dos ROM, no deducido del manual.
        ///
        /// En la ES PLUS `log` es una tecla que escribe `log(` y se cierra con `)`.
        ///
        /// En la ClassWiz esa misma posición es `log▫(▫)`: hay que poner la base en
        /// el subíndice, salir con la flecha derecha, escribir el argumento y volver
        /// a salir. El paréntesis de cierre **ya viene puesto**, así que añadirle un
        /// `)` da Syntax ERROR. Y `SHIFT` sobre esa tecla no es `log`, es `10^▫`.
        var logaritmoDecimal: (antes: [String], despues: [String]) {
            switch self {
            case .esPlus: return (["LOG"], ["PAREN_CIERRA"])
            case .classWiz: return (["LOG_B", "1", "0", "DERECHA"], ["DERECHA"])
            }
        }
    }

    struct Resultado {
        /// Nombres lógicos de tecla, en orden de pulsación.
        var teclas: [String] = []
        /// Lo que hubo que interpretar y conviene mirar antes de ejecutar.
        var avisos: [String] = []
    }

    enum Fallo: Error, CustomStringConvertible {
        case caracterInesperado(Character, en: Int)
        case nombreDesconocido(String, en: Int)
        case faltaParentesis
        case faltaOperando(en: Int)
        case expresionVacia

        var description: String {
            switch self {
            case .caracterInesperado(let c, let i):
                return "no entiendo «\(c)» en la posición \(i)"
            case .nombreDesconocido(let nombre, let i):
                return "no sé teclear «\(nombre)» (posición \(i))"
            case .faltaParentesis:
                return "falta cerrar un paréntesis"
            case .faltaOperando(let i):
                return "falta un número en la posición \(i)"
            case .expresionVacia:
                return "la expresión está vacía"
            }
        }
    }

    static func traducir(_ texto: String, para modelo: Modelo) throws -> Resultado {
        var analizador = Analizador(texto: texto, modelo: modelo)
        let arbol = try analizador.expresionCompleta()
        var resultado = Resultado()
        resultado.avisos = analizador.avisos
        emitir(arbol, modelo: modelo, en: &resultado.teclas)
        return resultado
    }

    // MARK: - Árbol

    indirect enum Nodo {
        case numero(String)
        case constante([String])            // π y demás: ya son teclas
        case binaria(String, Nodo, Nodo)    // + - * /
        case negativo(Nodo)
        case potencia(Nodo, Nodo)
        case cuadrado(Nodo)
        case raiz(Nodo)
        case funcion(String, Nodo)          // SIN, COS, TAN, LN, LOG
        case parentesis(Nodo)
    }

    // MARK: - Emisión

    /// Traduce el árbol a pulsaciones.
    ///
    /// Lo que no es evidente: en pantalla natural, la raíz y el exponente abren una
    /// **caja** y lo que se teclea cae dentro. Para salir hay que pulsar la flecha
    /// derecha. Sin eso, `√2+3` acabaría siendo `√(2+3)`, que es otra cuenta.
    private static func emitir(_ nodo: Nodo, modelo: Modelo, en teclas: inout [String]) {
        switch nodo {
        case .numero(let digitos):
            for caracter in digitos {
                if caracter == "." || caracter == "," {
                    teclas.append(modelo.separadorDecimal)
                } else {
                    teclas.append(String(caracter))
                }
            }

        case .constante(let secuencia):
            teclas.append(contentsOf: secuencia)

        case .binaria(let operador, let izquierda, let derecha):
            emitir(izquierda, modelo: modelo, en: &teclas)
            teclas.append(operador)
            emitir(derecha, modelo: modelo, en: &teclas)

        case .negativo(let operando):
            teclas.append("NEGATIVO")
            emitir(operando, modelo: modelo, en: &teclas)

        case .cuadrado(let base):
            emitir(base, modelo: modelo, en: &teclas)
            teclas.append("CUADRADO")

        case .potencia(let base, let exponente):
            emitir(base, modelo: modelo, en: &teclas)
            teclas.append("POTENCIA")
            emitir(exponente, modelo: modelo, en: &teclas)
            teclas.append("DERECHA")

        case .raiz(let dentro):
            teclas.append("RAIZ")
            emitir(dentro, modelo: modelo, en: &teclas)
            teclas.append("DERECHA")

        case .funcion(let nombre, let argumento):
            // `sin`, `cos`, `tan` y `ln` abren paréntesis y hay que cerrarlo.
            // `log` no se comporta igual en las dos máquinas.
            let (antes, despues) = nombre == "LOG"
                ? modelo.logaritmoDecimal
                : ([nombre], ["PAREN_CIERRA"])
            teclas.append(contentsOf: antes)
            emitir(argumento, modelo: modelo, en: &teclas)
            teclas.append(contentsOf: despues)

        case .parentesis(let dentro):
            teclas.append("PAREN_ABRE")
            emitir(dentro, modelo: modelo, en: &teclas)
            teclas.append("PAREN_CIERRA")
        }
    }

    // MARK: - Análisis

    private struct Analizador {
        let caracteres: [Character]
        let modelo: Modelo
        var posicion = 0
        var avisos: [String] = []

        init(texto: String, modelo: Modelo) {
            // `sqrt`, `√`, `×`, `÷`, `−` y los espacios se normalizan antes para que
            // el resto del análisis no tenga que conocer cada variante.
            var normalizado = texto
            for (de, a) in [("×", "*"), ("·", "*"), ("÷", "/"), ("−", "-"), ("–", "-"),
                            ("√", "sqrt"), ("π", "pi"), ("^", "^")] {
                normalizado = normalizado.replacingOccurrences(of: de, with: a)
            }
            self.caracteres = Array(normalizado)
            self.modelo = modelo
        }

        mutating func expresionCompleta() throws -> Nodo {
            saltarEspacios()
            guard posicion < caracteres.count else { throw Fallo.expresionVacia }
            let nodo = try expresion()
            saltarEspacios()
            if posicion < caracteres.count {
                throw Fallo.caracterInesperado(caracteres[posicion], en: posicion)
            }
            return nodo
        }

        mutating func expresion() throws -> Nodo {
            var izquierda = try termino()
            while true {
                saltarEspacios()
                guard let signo = actual, signo == "+" || signo == "-" else { break }
                posicion += 1
                let derecha = try termino()
                izquierda = .binaria(signo == "+" ? "MAS" : "MENOS", izquierda, derecha)
            }
            return izquierda
        }

        mutating func termino() throws -> Nodo {
            var izquierda = try factor()
            while true {
                saltarEspacios()
                guard let signo = actual, signo == "*" || signo == "/" else { break }
                posicion += 1
                let derecha = try factor()
                izquierda = .binaria(signo == "*" ? "POR" : "ENTRE", izquierda, derecha)
            }
            return izquierda
        }

        /// La potencia asocia por la derecha: `2^3^2` es `2^(3^2)`.
        mutating func factor() throws -> Nodo {
            let base = try unario()
            saltarEspacios()
            guard actual == "^" else { return base }
            posicion += 1
            let exponente = try factor()
            // `x^2` tiene tecla propia, y ademas evita tener que salir de la caja.
            if case .numero(let digitos) = exponente, digitos == "2" {
                return .cuadrado(base)
            }
            return .potencia(base, exponente)
        }

        mutating func unario() throws -> Nodo {
            saltarEspacios()
            if actual == "-" {
                posicion += 1
                return .negativo(try unario())
            }
            if actual == "+" {
                posicion += 1
                return try unario()
            }
            return try primaria()
        }

        mutating func primaria() throws -> Nodo {
            saltarEspacios()
            guard let caracter = actual else { throw Fallo.faltaOperando(en: posicion) }

            if caracter.isNumber || caracter == "." || caracter == "," {
                return .numero(leerNumero())
            }

            if caracter == "(" {
                posicion += 1
                let dentro = try expresion()
                saltarEspacios()
                guard actual == ")" else { throw Fallo.faltaParentesis }
                posicion += 1
                return .parentesis(dentro)
            }

            if caracter.isLetter {
                let nombre = leerPalabra().lowercased()
                return try aplicar(nombre: nombre)
            }

            throw Fallo.caracterInesperado(caracter, en: posicion)
        }

        /// Nombres que reconocemos, y el argumento entre paréntesis que llevan.
        mutating func aplicar(nombre: String) throws -> Nodo {
            if nombre == "pi" { return .constante(["SHIFT", "EXP"]) }

            let funciones = ["sin": "SIN", "cos": "COS", "tan": "TAN",
                             "ln": "LN", "log": "LOG"]

            saltarEspacios()
            if nombre == "sqrt" || nombre == "raiz" {
                let dentro = try argumento()
                return .raiz(dentro)
            }
            if let tecla = funciones[nombre] {
                let dentro = try argumento()
                return .funcion(tecla, dentro)
            }

            // Una variable suelta. No se traduce sola porque cada modelo la pone
            // detrás de ALPHA y de una tecla distinta; mejor decirlo que acertar
            // por casualidad.
            avisos.append("«\(nombre)» no se pudo traducir a teclas: sustitúyelo por "
                          + "su valor antes de enviarlo")
            throw Fallo.nombreDesconocido(nombre, en: posicion)
        }

        /// El argumento de una función, con o sin paréntesis: `sin 30` vale igual.
        mutating func argumento() throws -> Nodo {
            saltarEspacios()
            if actual == "(" {
                posicion += 1
                let dentro = try expresion()
                saltarEspacios()
                guard actual == ")" else { throw Fallo.faltaParentesis }
                posicion += 1
                return dentro
            }
            return try unario()
        }

        // MARK: Léxico

        var actual: Character? {
            posicion < caracteres.count ? caracteres[posicion] : nil
        }

        mutating func saltarEspacios() {
            while let caracter = actual, caracter == " " || caracter == "\t" {
                posicion += 1
            }
        }

        mutating func leerNumero() -> String {
            var digitos = ""
            while let caracter = actual,
                  caracter.isNumber || caracter == "." || caracter == "," {
                digitos.append(caracter)
                posicion += 1
            }
            return digitos
        }

        mutating func leerPalabra() -> String {
            var palabra = ""
            while let caracter = actual, caracter.isLetter {
                palabra.append(caracter)
                posicion += 1
            }
            return palabra
        }
    }
}
