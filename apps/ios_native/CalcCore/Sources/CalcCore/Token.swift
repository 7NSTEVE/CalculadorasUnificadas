import Foundation

/// Constantes que ocupan una sola pulsación.
public enum Constant: String, CaseIterable, Sendable {
    case pi
    case eulerNumber
    /// Resultado del último cálculo (tecla `Ans`).
    case answer
    /// `Ran#`: aleatorio en [0,1) con tres decimales, como en la Casio.
    case randomNumber
}

/// Registros de memoria direccionables desde el teclado.
public enum Variable: String, CaseIterable, Sendable {
    case a = "A", b = "B", c = "C", d = "D", e = "E", f = "F"
    case x = "x", y = "y"
    /// Memoria independiente (`M+`, `M-`, `M`).
    case independentMemory = "M"
}

public enum BinaryOperator: Sendable, Equatable, Hashable {
    case add
    case subtract
    case multiply
    case divide
    /// `nPr`
    case permutations
    /// `nCr`
    case combinations

    /// Precedencia al estilo Casio: nPr/nCr se agrupan antes que × y ÷.
    var precedence: Int {
        switch self {
        case .add, .subtract: return 10
        case .multiply, .divide: return 20
        case .permutations, .combinations: return 30
        }
    }
}

/// Operadores que se escriben *después* del operando.
public enum PostfixOperator: Sendable, Equatable, Hashable {
    case square
    case cube
    case reciprocal
    case factorial
    case percent
    /// Sexagesimal: convierte el número precedente a grados (tecla `°′″`).
    case degreesMinutesSeconds
}

/// Funciones que se escriben con paréntesis: `sin(`, `ln(`, …
public enum MathFunction: String, CaseIterable, Sendable {
    case sine, cosine, tangent
    case arcsine, arccosine, arctangent
    case hyperbolicSine, hyperbolicCosine, hyperbolicTangent
    case areaHyperbolicSine, areaHyperbolicCosine, areaHyperbolicTangent
    case naturalLog
    case commonLog
    case exponential          // e^x
    case powerOfTen           // 10^x
    case absoluteValue
    case roundToDisplay       // Rnd(
    case integerPart          // Int(
    case floorPart            // Intg(

    /// Etiqueta tal y como aparece serigrafiada en la calculadora.
    public var displayName: String {
        switch self {
        case .sine: return "sin"
        case .cosine: return "cos"
        case .tangent: return "tan"
        case .arcsine: return "sin⁻¹"
        case .arccosine: return "cos⁻¹"
        case .arctangent: return "tan⁻¹"
        case .hyperbolicSine: return "sinh"
        case .hyperbolicCosine: return "cosh"
        case .hyperbolicTangent: return "tanh"
        case .areaHyperbolicSine: return "sinh⁻¹"
        case .areaHyperbolicCosine: return "cosh⁻¹"
        case .areaHyperbolicTangent: return "tanh⁻¹"
        case .naturalLog: return "ln"
        case .commonLog: return "log"
        case .exponential: return "e^"
        case .powerOfTen: return "10^"
        case .absoluteValue: return "Abs"
        case .roundToDisplay: return "Rnd"
        case .integerPart: return "Int"
        case .floorPart: return "Intg"
        }
    }
}

/// Estructuras que abren una "caja" en la pantalla natural y se cierran con `groupEnd`.
public enum GroupKind: Sendable, Equatable, Hashable {
    case parenthesis
    /// Numerador y denominador, separados por un `argumentSeparator`.
    case fraction
    /// Entero, numerador y denominador: dos separadores.
    case mixedFraction
    case squareRoot
    case cubeRoot
    case absoluteValue
    case function(MathFunction)
    /// `log_b(x)`: base y argumento, un separador.
    case logarithmWithBase
    /// `x^▢`, se aplica al operando que le precede.
    case power
    /// `ⁿ√▢`, el índice es el operando que le precede.
    case nthRoot

    /// Las cajas que modifican al operando anterior en lugar de empezar uno nuevo.
    var attachesToPrecedingOperand: Bool {
        switch self {
        case .power, .nthRoot: return true
        default: return false
        }
    }

    /// Cuántos `argumentSeparator` espera esta caja.
    var separatorCount: Int {
        switch self {
        case .fraction, .logarithmWithBase: return 1
        case .mixedFraction: return 2
        default: return 0
        }
    }
}

/// Unidad mínima de entrada: cada pulsación de tecla produce uno o varios tokens.
///
/// La secuencia es lineal (para que mover el cursor sea trivial) pero las cajas
/// `groupStart`/`groupEnd` le dan la estructura de árbol que necesita la pantalla natural.
public enum Token: Sendable, Equatable, Hashable {
    case digit(Int)
    case decimalPoint
    /// Tecla `×10ˣ`.
    case exponentMark
    case constant(Constant)
    case variable(Variable)
    case binaryOperator(BinaryOperator)
    case postfixOperator(PostfixOperator)
    case groupStart(GroupKind)
    case groupEnd
    case argumentSeparator
    /// Tecla `(−)`: signo negativo, no resta.
    case unaryMinus

    /// Si el token puede cerrar un operando. Se usa para decidir dónde hace falta
    /// una multiplicación implícita (`2π`, `3(4+5)`).
    var terminatesOperand: Bool {
        switch self {
        case .digit, .decimalPoint, .constant, .variable, .groupEnd, .postfixOperator:
            return true
        case .exponentMark, .binaryOperator, .groupStart, .argumentSeparator, .unaryMinus:
            return false
        }
    }
}
