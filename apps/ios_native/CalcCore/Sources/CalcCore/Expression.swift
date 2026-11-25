import Foundation

/// Árbol sintáctico resultante de analizar la línea de entrada.
///
/// Sirve para dos cosas a la vez: evaluarlo y dibujarlo en pantalla natural.
/// Por eso conserva el texto literal de los números (`"1.50"` no se colapsa a `1.5`
/// mientras se está escribiendo).
public indirect enum Expression: Sendable, Equatable {
    /// Literal numérico. `text` es lo tecleado; `exponentText` es la parte tras `×10ˣ`.
    case number(text: String, exponentText: String?, exponentNegative: Bool)
    case constant(Constant)
    case variable(Variable)
    /// Hueco vacío (`▢`): aparece cuando falta un operando por escribir.
    case placeholder
    case negation(Expression)
    case binary(BinaryOperator, Expression, Expression)
    /// Multiplicación implícita: `2π`. Se distingue de la explícita porque liga más fuerte.
    case implicitProduct(Expression, Expression)
    case postfix(PostfixOperator, Expression)
    case function(MathFunction, Expression)
    case fraction(numerator: Expression, denominator: Expression)
    case mixedFraction(whole: Expression, numerator: Expression, denominator: Expression)
    /// `index` a `nil` significa raíz cuadrada.
    case root(index: Expression?, radicand: Expression)
    case power(base: Expression, exponent: Expression)
    case absoluteValue(Expression)
    /// `base` a `nil` significa logaritmo decimal.
    case logarithm(base: Expression?, argument: Expression)

    /// `true` si en algún punto del árbol falta algo por escribir.
    public var containsPlaceholder: Bool {
        switch self {
        case .placeholder:
            return true
        case .number, .constant, .variable:
            return false
        case let .negation(inner),
             let .postfix(_, inner),
             let .function(_, inner),
             let .absoluteValue(inner):
            return inner.containsPlaceholder
        case let .binary(_, lhs, rhs), let .implicitProduct(lhs, rhs):
            return lhs.containsPlaceholder || rhs.containsPlaceholder
        case let .fraction(numerator, denominator):
            return numerator.containsPlaceholder || denominator.containsPlaceholder
        case let .mixedFraction(whole, numerator, denominator):
            return whole.containsPlaceholder || numerator.containsPlaceholder
                || denominator.containsPlaceholder
        case let .root(index, radicand):
            return (index?.containsPlaceholder ?? false) || radicand.containsPlaceholder
        case let .power(base, exponent):
            return base.containsPlaceholder || exponent.containsPlaceholder
        case let .logarithm(base, argument):
            return (base?.containsPlaceholder ?? false) || argument.containsPlaceholder
        }
    }
}

extension Expression {
    /// Construye el literal a partir de los trozos tecleados, ya listo para `Double`.
    var numericLiteral: Double? {
        guard case let .number(text, exponentText, exponentNegative) = self else { return nil }
        // Se normaliza lo tecleado a medias: ".5" → "0.5", "5." → "5.0". La mantisa vacía
        // vale 0, salvo que ya haya exponente escrito, donde lo natural es 1×10ⁿ.
        var mantissa = text
        if mantissa.hasPrefix(".") { mantissa = "0" + mantissa }
        if mantissa.hasSuffix(".") { mantissa += "0" }
        if mantissa.isEmpty || mantissa == "0." {
            mantissa = (exponentText?.isEmpty == false) ? "1" : "0"
        }
        guard let base = Double(mantissa) else { return nil }
        guard let exponentText, !exponentText.isEmpty else { return base }
        guard let exponent = Double(exponentText) else { return nil }
        return base * pow(10, exponentNegative ? -exponent : exponent)
    }
}
