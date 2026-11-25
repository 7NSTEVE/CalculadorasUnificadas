import Foundation

/// Análisis dimensional: qué es un número, no solo cuánto vale.
///
/// La Casio no sabe nada de esto y no debe saberlo: la capa 1 devuelve un número
/// pelado y así se queda. Esto vive encima y sirve para lo que de verdad cuesta en
/// un examen de circuitos: acordarse de si `0,0000023` eran amperios o faradios, y
/// darse cuenta a tiempo de que estás sumando una resistencia con una corriente.
///
/// El sistema es el SI con las siete dimensiones base. Todo lo demás —voltio, ohmio,
/// faradio— es una combinación de esas siete, así que la compatibilidad entre dos
/// magnitudes se decide comparando exponentes, no nombres. Eso evita el error clásico
/// de tratar `V/A` y `Ω` como cosas distintas cuando son la misma.

/// Exponentes de las siete dimensiones base del SI.
struct Dimension: Equatable, Hashable, Codable {
    var metro = 0
    var kilogramo = 0
    var segundo = 0
    var amperio = 0
    var kelvin = 0
    var mol = 0
    var candela = 0

    static let adimensional = Dimension()

    static func * (izquierda: Dimension, derecha: Dimension) -> Dimension {
        Dimension(metro: izquierda.metro + derecha.metro,
                  kilogramo: izquierda.kilogramo + derecha.kilogramo,
                  segundo: izquierda.segundo + derecha.segundo,
                  amperio: izquierda.amperio + derecha.amperio,
                  kelvin: izquierda.kelvin + derecha.kelvin,
                  mol: izquierda.mol + derecha.mol,
                  candela: izquierda.candela + derecha.candela)
    }

    static func / (izquierda: Dimension, derecha: Dimension) -> Dimension {
        izquierda * derecha.invertida
    }

    var invertida: Dimension {
        Dimension(metro: -metro, kilogramo: -kilogramo, segundo: -segundo,
                  amperio: -amperio, kelvin: -kelvin, mol: -mol, candela: -candela)
    }

    var esAdimensional: Bool { self == .adimensional }

    /// Cómo se escribe en unidades base, para cuando no hay nombre propio.
    /// Se usa al construir una magnitud a mano y en los mensajes de error.
    var formulaBase: String {
        let partes: [(String, Int)] = [
            ("m", metro), ("kg", kilogramo), ("s", segundo), ("A", amperio),
            ("K", kelvin), ("mol", mol), ("cd", candela)
        ]
        let arriba = partes.filter { $0.1 > 0 }.map { escribir($0.0, $0.1) }
        let abajo = partes.filter { $0.1 < 0 }.map { escribir($0.0, -$0.1) }

        if arriba.isEmpty && abajo.isEmpty { return "1" }
        let numerador = arriba.isEmpty ? "1" : arriba.joined(separator: "·")
        return abajo.isEmpty ? numerador : "\(numerador)/\(abajo.joined(separator: "·"))"
    }

    private func escribir(_ simbolo: String, _ exponente: Int) -> String {
        exponente == 1 ? simbolo : "\(simbolo)^\(exponente)"
    }
}

/// Una magnitud física con nombre: lo que se enseña junto al número.
struct Magnitud: Equatable, Codable {
    /// Símbolo de la unidad coherente del SI: V, A, Ω, F, H, W…
    let simbolo: String
    /// Nombre en castellano, para el panel de ayuda.
    let nombre: String
    let dimension: Dimension
    /// Categoría, que es de donde sale el color en la interfaz.
    let categoria: String

    static let adimensional = Magnitud(simbolo: "", nombre: "número",
                                       dimension: .adimensional, categoria: "número")
}

/// Catálogo de magnitudes. Sesgado a electrónica a propósito.
enum Magnitudes {
    private static func d(m: Int = 0, kg: Int = 0, s: Int = 0, A: Int = 0,
                          K: Int = 0, mol: Int = 0, cd: Int = 0) -> Dimension {
        Dimension(metro: m, kilogramo: kg, segundo: s, amperio: A,
                  kelvin: K, mol: mol, candela: cd)
    }

    static let todas: [Magnitud] = [
        Magnitud(simbolo: "m", nombre: "longitud", dimension: d(m: 1), categoria: "mecánica"),
        Magnitud(simbolo: "kg", nombre: "masa", dimension: d(kg: 1), categoria: "mecánica"),
        Magnitud(simbolo: "s", nombre: "tiempo", dimension: d(s: 1), categoria: "tiempo"),
        Magnitud(simbolo: "A", nombre: "corriente", dimension: d(A: 1), categoria: "eléctrica"),
        Magnitud(simbolo: "K", nombre: "temperatura", dimension: d(K: 1), categoria: "térmica"),

        Magnitud(simbolo: "Hz", nombre: "frecuencia", dimension: d(s: -1), categoria: "tiempo"),
        Magnitud(simbolo: "N", nombre: "fuerza", dimension: d(m: 1, kg: 1, s: -2),
                 categoria: "mecánica"),
        Magnitud(simbolo: "J", nombre: "energía", dimension: d(m: 2, kg: 1, s: -2),
                 categoria: "energía"),
        Magnitud(simbolo: "W", nombre: "potencia", dimension: d(m: 2, kg: 1, s: -3),
                 categoria: "energía"),
        Magnitud(simbolo: "C", nombre: "carga", dimension: d(s: 1, A: 1), categoria: "eléctrica"),
        Magnitud(simbolo: "V", nombre: "tensión", dimension: d(m: 2, kg: 1, s: -3, A: -1),
                 categoria: "eléctrica"),
        Magnitud(simbolo: "Ω", nombre: "resistencia", dimension: d(m: 2, kg: 1, s: -3, A: -2),
                 categoria: "eléctrica"),
        Magnitud(simbolo: "S", nombre: "conductancia", dimension: d(m: -2, kg: -1, s: 3, A: 2),
                 categoria: "eléctrica"),
        Magnitud(simbolo: "F", nombre: "capacidad", dimension: d(m: -2, kg: -1, s: 4, A: 2),
                 categoria: "eléctrica"),
        Magnitud(simbolo: "H", nombre: "inductancia", dimension: d(m: 2, kg: 1, s: -2, A: -2),
                 categoria: "eléctrica"),
        Magnitud(simbolo: "Wb", nombre: "flujo magnético", dimension: d(m: 2, kg: 1, s: -2, A: -1),
                 categoria: "magnética"),
        Magnitud(simbolo: "T", nombre: "densidad de flujo", dimension: d(kg: 1, s: -2, A: -1),
                 categoria: "magnética"),
    ]

    /// Busca por símbolo exacto. Distingue mayúsculas: `S` (siemens) no es `s`.
    static func porSimbolo(_ simbolo: String) -> Magnitud? {
        todas.first { $0.simbolo == simbolo }
    }

    /// Qué magnitud con nombre propio tiene esta dimensión, si alguna.
    ///
    /// Es lo que convierte un `V/A` calculado a mano en «Ω», que es lo que hace que
    /// el resultado se entienda de un vistazo.
    static func porDimension(_ dimension: Dimension) -> Magnitud? {
        todas.first { $0.dimension == dimension }
    }
}

/// Prefijos del SI. Solo los que se usan de verdad en electrónica.
struct PrefijoSI {
    let simbolo: String
    let exponente: Int

    static let todos: [PrefijoSI] = [
        PrefijoSI(simbolo: "T", exponente: 12),
        PrefijoSI(simbolo: "G", exponente: 9),
        PrefijoSI(simbolo: "M", exponente: 6),
        PrefijoSI(simbolo: "k", exponente: 3),
        PrefijoSI(simbolo: "", exponente: 0),
        PrefijoSI(simbolo: "m", exponente: -3),
        PrefijoSI(simbolo: "µ", exponente: -6),
        PrefijoSI(simbolo: "n", exponente: -9),
        PrefijoSI(simbolo: "p", exponente: -12),
        PrefijoSI(simbolo: "f", exponente: -15),
    ]

    /// El prefijo que deja la mantisa entre 1 y 1000.
    ///
    /// Es la regla de la notación de ingeniería, y es la que hace que `0,0000023 A`
    /// se lea `2,3 µA` en vez de tener que contar ceros.
    static func para(_ valor: Double) -> PrefijoSI {
        let magnitud = abs(valor)
        guard magnitud > 0, magnitud.isFinite else { return PrefijoSI(simbolo: "", exponente: 0) }
        // Se busca de mayor a menor y se coge el primero que no deje la mantisa por
        // debajo de 1. `todos` está ordenado de T a f, así que basta recorrerlo.
        for prefijo in todos where magnitud >= pow(10, Double(prefijo.exponente)) {
            return prefijo
        }
        return todos.last ?? PrefijoSI(simbolo: "", exponente: 0)
    }
}

/// Un número con su unidad. Es lo que la capa de estudio pega al resultado.
struct CantidadFisica: Equatable {
    var valor: Double
    var magnitud: Magnitud

    init(_ valor: Double, _ magnitud: Magnitud) {
        self.valor = valor
        self.magnitud = magnitud
    }

    /// Con prefijo del SI y el número de cifras significativas que se pida.
    ///
    /// Ejemplo: `CantidadFisica(0.0000023, corriente).formateada()` → `"2,3 µA"`.
    func formateada(significativas: Int = 4, separadorDecimal: String = ",") -> String {
        guard valor.isFinite else { return "—" }
        guard magnitud.simbolo.isEmpty == false else {
            return numero(valor, significativas: significativas,
                          separadorDecimal: separadorDecimal)
        }
        if valor == 0 {
            return "0 \(magnitud.simbolo)"
        }

        let prefijo = PrefijoSI.para(valor)
        let mantisa = valor / pow(10, Double(prefijo.exponente))
        let texto = numero(mantisa, significativas: significativas,
                           separadorDecimal: separadorDecimal)
        return "\(texto) \(prefijo.simbolo)\(magnitud.simbolo)"
    }

    private func numero(_ valor: Double, significativas: Int,
                        separadorDecimal: String) -> String {
        // Se redondea a cifras significativas, no a decimales fijos: 2,3 µA y
        // 947 kΩ tienen que salir los dos con la misma "precisión aparente".
        let magnitudDecimal = valor == 0 ? 0 : Int(floor(log10(abs(valor))))
        let decimales = max(0, significativas - 1 - magnitudDecimal)
        var texto = String(format: "%.\(min(decimales, 12))f", valor)

        // Se quitan los ceros de relleno: "2,300" molesta más de lo que informa.
        if texto.contains(".") {
            while texto.hasSuffix("0") { texto.removeLast() }
            if texto.hasSuffix(".") { texto.removeLast() }
        }
        return texto.replacingOccurrences(of: ".", with: separadorDecimal)
    }

    // MARK: - Aritmética con control dimensional

    /// Suma. Nil si las dimensiones no cuadran: sumar ohmios con amperios no es un
    /// número raro, es que el planteamiento está mal, y hay que decirlo antes.
    static func + (izquierda: CantidadFisica, derecha: CantidadFisica) -> CantidadFisica? {
        guard izquierda.magnitud.dimension == derecha.magnitud.dimension else { return nil }
        return CantidadFisica(izquierda.valor + derecha.valor, izquierda.magnitud)
    }

    static func - (izquierda: CantidadFisica, derecha: CantidadFisica) -> CantidadFisica? {
        guard izquierda.magnitud.dimension == derecha.magnitud.dimension else { return nil }
        return CantidadFisica(izquierda.valor - derecha.valor, izquierda.magnitud)
    }

    /// Multiplicar y dividir siempre valen: el resultado cambia de magnitud.
    static func * (izquierda: CantidadFisica, derecha: CantidadFisica) -> CantidadFisica {
        CantidadFisica(izquierda.valor * derecha.valor,
                       Unidades.magnitudPara(izquierda.magnitud.dimension * derecha.magnitud.dimension))
    }

    static func / (izquierda: CantidadFisica, derecha: CantidadFisica) -> CantidadFisica {
        CantidadFisica(izquierda.valor / derecha.valor,
                       Unidades.magnitudPara(izquierda.magnitud.dimension / derecha.magnitud.dimension))
    }
}

/// Operaciones sobre magnitudes que no pertenecen a una sola cantidad.
enum Unidades {

    /// La magnitud con nombre de una dimensión, o una construida al vuelo.
    ///
    /// Si `V/A` da la dimensión del ohmio, sale «Ω». Si no hay nombre propio, se
    /// escribe en unidades base (`kg·m^2/s^3·A^2`) en vez de mentir con un símbolo.
    static func magnitudPara(_ dimension: Dimension) -> Magnitud {
        if dimension.esAdimensional { return .adimensional }
        if let conocida = Magnitudes.porDimension(dimension) { return conocida }
        return Magnitud(simbolo: dimension.formulaBase, nombre: "magnitud derivada",
                        dimension: dimension, categoria: "derivada")
    }

    /// Interpreta un texto como `"4.7k"`, `"2,3 µA"`, `"1M"` o `"330"`.
    ///
    /// Acepta la coma decimal porque es lo que escribe la LA X y lo que escribe
    /// el dueño. Si no trae unidad, sale adimensional y quien llame decide qué es.
    static func interpretar(_ texto: String) -> CantidadFisica? {
        var resto = texto.trimmingCharacters(in: .whitespaces)
            .replacingOccurrences(of: ",", with: ".")
        guard !resto.isEmpty else { return nil }

        // Número por delante: signo, dígitos, punto y exponente.
        var numero = ""
        while let primero = resto.first,
              primero.isNumber || primero == "." || primero == "-" || primero == "+"
                || ((primero == "e" || primero == "E") && !numero.isEmpty) {
            numero.append(primero)
            resto.removeFirst()
        }
        guard let valor = Double(numero) else { return nil }

        resto = resto.trimmingCharacters(in: .whitespaces)
        if resto.isEmpty { return CantidadFisica(valor, .adimensional) }

        // Lo que queda es prefijo + símbolo, o solo símbolo. Se prueba primero con
        // el símbolo entero: "m" solo es metro, no "mili" sin unidad.
        if let magnitud = Magnitudes.porSimbolo(resto) {
            return CantidadFisica(valor, magnitud)
        }
        for prefijo in PrefijoSI.todos where !prefijo.simbolo.isEmpty {
            guard resto.hasPrefix(prefijo.simbolo) else { continue }
            let cola = String(resto.dropFirst(prefijo.simbolo.count))
            if cola.isEmpty {
                // "4.7k" sin unidad: se guarda el factor y ya dirá quien llame qué es.
                return CantidadFisica(valor * pow(10, Double(prefijo.exponente)), .adimensional)
            }
            if let magnitud = Magnitudes.porSimbolo(cola) {
                return CantidadFisica(valor * pow(10, Double(prefijo.exponente)), magnitud)
            }
        }
        return nil
    }

    /// Todas las formas útiles de escribir la misma cantidad, para el conversor.
    ///
    /// No convierte entre magnitudes distintas —eso no existe— sino entre prefijos,
    /// que es la conversión que de verdad se hace veinte veces por ejercicio.
    static func equivalencias(_ cantidad: CantidadFisica) -> [String] {
        guard cantidad.valor.isFinite, cantidad.valor != 0,
              !cantidad.magnitud.simbolo.isEmpty else { return [] }

        return PrefijoSI.todos.compactMap { prefijo in
            let mantisa = cantidad.valor / pow(10, Double(prefijo.exponente))
            // Solo las que se leen bien: por debajo de 0,001 o por encima de un
            // millón vuelve a ser una fila de ceros y no ayuda a nadie.
            guard abs(mantisa) >= 0.001, abs(mantisa) < 1_000_000 else { return nil }
            let texto = CantidadFisica(mantisa, .adimensional).formateada(significativas: 6)
            return "\(texto) \(prefijo.simbolo)\(cantidad.magnitud.simbolo)"
        }
    }

    /// Comprueba si dos cantidades se pueden sumar o comparar.
    ///
    /// Devuelve el aviso ya redactado, no un booleano: el mensaje es la mitad del
    /// valor de la comprobación.
    static func avisoIncompatibilidad(_ izquierda: CantidadFisica,
                                      _ derecha: CantidadFisica) -> String? {
        guard izquierda.magnitud.dimension != derecha.magnitud.dimension else { return nil }
        let uno = izquierda.magnitud.nombre
        let otro = derecha.magnitud.nombre
        return "No se puede sumar \(uno) (\(izquierda.magnitud.simbolo)) con "
             + "\(otro) (\(derecha.magnitud.simbolo)): son magnitudes distintas."
    }
}
