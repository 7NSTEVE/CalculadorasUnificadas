import Foundation

public enum AngleMode: String, CaseIterable, Sendable {
    case degrees = "D"
    case radians = "R"
    case gradians = "G"
}

/// Los mismos errores que muestra la calculadora real.
public enum CalcError: Error, Equatable, Sendable {
    /// Quedan huecos `▢` por rellenar.
    case incompleteExpression
    case syntaxError
    /// `Math ERROR`: dominio inválido, división por cero, etc.
    case mathError
    /// Fuera del rango ±9.999999999×10⁹⁹.
    case rangeError
}

/// Rango representable de la fx-570/991: ±9.999999999×10⁹⁹.
private let casioLimit = 1e100

/// Se define aquí en vez de usar `M_E`, que no está expuesto igual en todas las
/// plataformas donde se compila este paquete (Windows para las pruebas, iOS para la app).
private let eulerNumber = 2.718281828459045235360287471352662497757

public struct Evaluator: Sendable {
    public var angleMode: AngleMode
    public var variables: [Variable: Double]
    public var lastAnswer: Double

    public init(
        angleMode: AngleMode = .degrees,
        variables: [Variable: Double] = [:],
        lastAnswer: Double = 0
    ) {
        self.angleMode = angleMode
        self.variables = variables
        self.lastAnswer = lastAnswer
    }

    public func evaluate(_ expression: Expression) throws -> Double {
        guard !expression.containsPlaceholder else { throw CalcError.incompleteExpression }
        let value = try value(of: expression)
        return try checked(value)
    }

    // MARK: - Recorrido del árbol

    private func value(of expression: Expression) throws -> Double {
        switch expression {
        case .number:
            guard let literal = expression.numericLiteral else { throw CalcError.syntaxError }
            return literal

        case .placeholder:
            throw CalcError.incompleteExpression

        case let .constant(constant):
            switch constant {
            case .pi: return Double.pi
            case .eulerNumber: return eulerNumber
            case .answer: return lastAnswer
            case .randomNumber: return Double(Int.random(in: 0..<1000)) / 1000
            }

        case let .variable(variable):
            return variables[variable] ?? 0

        case let .negation(inner):
            return -(try value(of: inner))

        case let .binary(op, lhsExpression, rhsExpression):
            return try applyBinary(op, lhsExpression, rhsExpression)

        case let .implicitProduct(lhsExpression, rhsExpression):
            return try checked(try value(of: lhsExpression) * value(of: rhsExpression))

        case let .postfix(op, inner):
            return try applyPostfix(op, to: try value(of: inner))

        case let .function(function, argument):
            return try apply(function, to: try value(of: argument))

        case let .fraction(numerator, denominator):
            let bottom = try value(of: denominator)
            guard bottom != 0 else { throw CalcError.mathError }
            return try checked(try value(of: numerator) / bottom)

        case let .mixedFraction(whole, numerator, denominator):
            let bottom = try value(of: denominator)
            guard bottom != 0 else { throw CalcError.mathError }
            let wholePart = try value(of: whole)
            let fractionPart = try value(of: numerator) / bottom
            // El signo del entero arrastra a la parte fraccionaria: −2 1/2 es −2.5.
            let signed = wholePart < 0 ? wholePart - fractionPart : wholePart + fractionPart
            return try checked(signed)

        case let .root(index, radicand):
            return try applyRoot(index: index, radicand: radicand)

        case let .power(base, exponent):
            return try applyPower(base: try value(of: base), exponent: try value(of: exponent))

        case let .absoluteValue(inner):
            return abs(try value(of: inner))

        case let .logarithm(base, argument):
            let x = try value(of: argument)
            guard x > 0 else { throw CalcError.mathError }
            guard let base else { return log10(x) }
            let b = try value(of: base)
            guard b > 0, b != 1 else { throw CalcError.mathError }
            return try checked(log(x) / log(b))
        }
    }

    // MARK: - Operadores binarios

    private func applyBinary(
        _ op: BinaryOperator,
        _ lhsExpression: Expression,
        _ rhsExpression: Expression
    ) throws -> Double {
        let lhs = try value(of: lhsExpression)

        // El `%` de Casio es relativo al operando izquierdo cuando se suma o se resta:
        // `200+10%` son 220, no 200.1. En × y ÷ es simplemente una centésima parte.
        if case let .postfix(.percent, inner) = rhsExpression {
            let portion = try value(of: inner) / 100
            switch op {
            case .add: return try checked(lhs + lhs * portion)
            case .subtract: return try checked(lhs - lhs * portion)
            default: return try combine(op, lhs, portion)
            }
        }

        return try combine(op, lhs, try value(of: rhsExpression))
    }

    private func combine(_ op: BinaryOperator, _ lhs: Double, _ rhs: Double) throws -> Double {
        switch op {
        case .add: return try checked(lhs + rhs)
        case .subtract: return try checked(lhs - rhs)
        case .multiply: return try checked(lhs * rhs)
        case .divide:
            guard rhs != 0 else { throw CalcError.mathError }
            return try checked(lhs / rhs)
        case .permutations: return try permutations(n: lhs, r: rhs)
        case .combinations: return try combinations(n: lhs, r: rhs)
        }
    }

    // MARK: - Sufijos

    private func applyPostfix(_ op: PostfixOperator, to x: Double) throws -> Double {
        switch op {
        case .square: return try checked(x * x)
        case .cube: return try checked(x * x * x)
        case .reciprocal:
            guard x != 0 else { throw CalcError.mathError }
            return try checked(1 / x)
        case .factorial: return try factorial(x)
        case .percent: return x / 100
        case .degreesMinutesSeconds:
            // Pendiente: la entrada sexagesimal encadenada (1°30′0″) necesita su propia
            // estructura en el árbol. Por ahora el valor pasa tal cual.
            return x
        }
    }

    private func factorial(_ x: Double) throws -> Double {
        guard x >= 0, x == x.rounded(), x <= 69 else { throw CalcError.mathError }
        var result = 1.0
        var i = 2.0
        while i <= x {
            result *= i
            i += 1
        }
        return try checked(result)
    }

    private func permutations(n: Double, r: Double) throws -> Double {
        guard n == n.rounded(), r == r.rounded(), n >= 0, r >= 0, r <= n else {
            throw CalcError.mathError
        }
        var result = 1.0
        var i = 0.0
        while i < r {
            result *= (n - i)
            i += 1
        }
        return try checked(result)
    }

    private func combinations(n: Double, r: Double) throws -> Double {
        guard n == n.rounded(), r == r.rounded(), n >= 0, r >= 0, r <= n else {
            throw CalcError.mathError
        }
        // Se usa el menor de r y n−r, y se divide en cada paso para no desbordar antes de tiempo.
        let k = min(r, n - r)
        var result = 1.0
        var i = 1.0
        while i <= k {
            result = result * (n - k + i) / i
            i += 1
        }
        return try checked(result.rounded())
    }

    // MARK: - Potencias y raíces

    private func applyPower(base: Double, exponent: Double) throws -> Double {
        if base == 0 && exponent < 0 { throw CalcError.mathError }
        if base < 0 && exponent != exponent.rounded() { throw CalcError.mathError }
        return try checked(pow(base, exponent))
    }

    private func applyRoot(index: Expression?, radicand: Expression) throws -> Double {
        let x = try value(of: radicand)
        guard let index else {
            guard x >= 0 else { throw CalcError.mathError }
            return x.squareRoot()
        }
        let n = try value(of: index)
        guard n != 0 else { throw CalcError.mathError }
        if x < 0 {
            // Solo los índices impares admiten radicando negativo: ∛(−8) = −2.
            guard n == n.rounded(), Int(n) % 2 != 0 else { throw CalcError.mathError }
            return try checked(-pow(-x, 1 / n))
        }
        return try checked(pow(x, 1 / n))
    }

    // MARK: - Funciones

    private func apply(_ function: MathFunction, to x: Double) throws -> Double {
        switch function {
        case .sine: return try Trigonometry.sine(x, mode: angleMode)
        case .cosine: return try Trigonometry.cosine(x, mode: angleMode)
        case .tangent: return try Trigonometry.tangent(x, mode: angleMode)
        case .arcsine:
            guard (-1...1).contains(x) else { throw CalcError.mathError }
            return Trigonometry.fromRadians(asin(x), mode: angleMode)
        case .arccosine:
            guard (-1...1).contains(x) else { throw CalcError.mathError }
            return Trigonometry.fromRadians(acos(x), mode: angleMode)
        case .arctangent:
            return Trigonometry.fromRadians(atan(x), mode: angleMode)
        case .hyperbolicSine: return try checked(sinh(x))
        case .hyperbolicCosine: return try checked(cosh(x))
        case .hyperbolicTangent: return tanh(x)
        case .areaHyperbolicSine: return try checked(asinh(x))
        case .areaHyperbolicCosine:
            guard x >= 1 else { throw CalcError.mathError }
            return try checked(acosh(x))
        case .areaHyperbolicTangent:
            guard x > -1, x < 1 else { throw CalcError.mathError }
            return try checked(atanh(x))
        case .naturalLog:
            guard x > 0 else { throw CalcError.mathError }
            return log(x)
        case .commonLog:
            guard x > 0 else { throw CalcError.mathError }
            return log10(x)
        case .exponential: return try checked(exp(x))
        case .powerOfTen: return try checked(pow(10, x))
        case .absoluteValue: return abs(x)
        case .roundToDisplay: return NumberFormatting.roundToSignificantDigits(x, digits: 10)
        case .integerPart: return x < 0 ? x.rounded(.up) : x.rounded(.down)
        case .floorPart: return x.rounded(.down)
        }
    }

    // MARK: - Rango

    private func checked(_ value: Double) throws -> Double {
        guard value.isFinite else { throw CalcError.mathError }
        guard abs(value) < casioLimit else { throw CalcError.rangeError }
        return value
    }
}
