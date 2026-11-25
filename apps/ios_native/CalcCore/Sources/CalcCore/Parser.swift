import Foundation

/// Convierte la secuencia lineal de tokens en el árbol `Expression`.
///
/// Es deliberadamente tolerante: nunca lanza. Todo lo que falta se rellena con
/// `.placeholder`, porque la pantalla tiene que poder dibujar una expresión a medio
/// escribir. Quien decide si el resultado es calculable es el evaluador, mirando
/// `containsPlaceholder`.
public struct Parser {
    private let tokens: [Token]
    private var index = 0

    private init(_ tokens: [Token]) {
        self.tokens = tokens
    }

    public static func parse(_ tokens: [Token]) -> Expression {
        var parser = Parser(tokens)
        var result = parser.parseExpression(minPrecedence: 0)
        // Un `)` o un separador sueltos no deben colgar el bucle ni tragarse lo que venga
        // detrás: se descartan y se sigue analizando.
        while !parser.isAtEnd {
            parser.advance()
            guard !parser.isAtEnd else { break }
            let next = parser.parseExpression(minPrecedence: 0)
            if next != .placeholder {
                result = .implicitProduct(result, next)
            }
        }
        return result
    }

    // MARK: - Recorrido

    private var isAtEnd: Bool { index >= tokens.count }

    private var current: Token? { isAtEnd ? nil : tokens[index] }

    private mutating func advance() {
        index += 1
    }

    // MARK: - Niveles del análisis

    /// Operadores binarios explícitos, por precedencia ascendente.
    private mutating func parseExpression(minPrecedence: Int) -> Expression {
        var lhs = parseImplicitChain()
        while case let .binaryOperator(op)? = current, op.precedence >= minPrecedence {
            advance()
            let rhs = parseExpression(minPrecedence: op.precedence + 1)
            lhs = .binary(op, lhs, rhs)
        }
        return lhs
    }

    /// Multiplicación implícita (`2π`, `3(4+5)`).
    ///
    /// Liga más fuerte que `×` y `÷` explícitos, igual que en la Casio: por eso
    /// `1÷2π` vale `1÷(2π)` y no `(1÷2)π`.
    private mutating func parseImplicitChain() -> Expression {
        var lhs = parseUnary()
        while let token = current, startsOperand(token) {
            let rhs = parsePostfixChain()
            lhs = .implicitProduct(lhs, rhs)
        }
        return lhs
    }

    private mutating func parseUnary() -> Expression {
        if current == .unaryMinus {
            advance()
            return .negation(parseUnary())
        }
        return parsePostfixChain()
    }

    /// Sufijos y cajas que se aplican al operando ya leído: `x²`, `x!`, `x^▢`, `ⁿ√▢`.
    private mutating func parsePostfixChain() -> Expression {
        var expression = parsePrimary()
        while let token = current {
            switch token {
            case let .postfixOperator(op):
                advance()
                expression = .postfix(op, expression)
            case .groupStart(.power):
                advance()
                let parts = parseGroupBody(separatorCount: 0)
                expression = .power(base: expression, exponent: parts[0])
            case .groupStart(.nthRoot):
                advance()
                let parts = parseGroupBody(separatorCount: 0)
                expression = .root(index: expression, radicand: parts[0])
            default:
                return expression
            }
        }
        return expression
    }

    private mutating func parsePrimary() -> Expression {
        guard let token = current else { return .placeholder }
        switch token {
        case .digit, .decimalPoint, .exponentMark:
            return scanNumber()
        case let .constant(value):
            advance()
            return .constant(value)
        case let .variable(value):
            advance()
            return .variable(value)
        case let .groupStart(kind) where !kind.attachesToPrecedingOperand:
            advance()
            return parseGroup(kind)
        default:
            // `)`, separadores, operadores binarios sin operando izquierdo… no se consumen:
            // se devuelve un hueco y que decida el nivel de arriba.
            return .placeholder
        }
    }

    // MARK: - Cajas

    /// Lee el contenido de una caja ya abierta y la cierra si encuentra el `)`.
    private mutating func parseGroupBody(separatorCount: Int) -> [Expression] {
        var parts: [Expression] = []
        for position in 0...separatorCount {
            parts.append(parseExpression(minPrecedence: 0))
            if position < separatorCount, current == .argumentSeparator {
                advance()
            }
        }
        if current == .groupEnd {
            advance()
        }
        // Si el usuario aún no ha escrito todos los huecos, se completan.
        while parts.count <= separatorCount {
            parts.append(.placeholder)
        }
        return parts
    }

    private mutating func parseGroup(_ kind: GroupKind) -> Expression {
        let parts = parseGroupBody(separatorCount: kind.separatorCount)
        switch kind {
        case .parenthesis:
            return parts[0]
        case .fraction:
            return .fraction(numerator: parts[0], denominator: parts[1])
        case .mixedFraction:
            return .mixedFraction(whole: parts[0], numerator: parts[1], denominator: parts[2])
        case .squareRoot:
            return .root(index: nil, radicand: parts[0])
        case .cubeRoot:
            return .root(index: .number(text: "3", exponentText: nil, exponentNegative: false),
                         radicand: parts[0])
        case .absoluteValue:
            return .absoluteValue(parts[0])
        case let .function(function):
            return .function(function, parts[0])
        case .logarithmWithBase:
            return .logarithm(base: parts[0], argument: parts[1])
        case .power, .nthRoot:
            // Las gestiona `parsePostfixChain`; aquí solo puede llegar por entrada corrupta.
            return parts[0]
        }
    }

    // MARK: - Literales

    private mutating func scanNumber() -> Expression {
        var text = ""
        while case let .digit(value)? = current {
            text += String(value)
            advance()
        }
        if current == .decimalPoint {
            text += "."
            advance()
            while case let .digit(value)? = current {
                text += String(value)
                advance()
            }
        }

        var exponentText: String?
        var exponentNegative = false
        if current == .exponentMark {
            advance()
            if current == .unaryMinus {
                exponentNegative = true
                advance()
            }
            var digits = ""
            while case let .digit(value)? = current {
                digits += String(value)
                advance()
            }
            exponentText = digits
        }

        return .number(text: text, exponentText: exponentText, exponentNegative: exponentNegative)
    }

    // MARK: - Apoyo

    /// ¿Este token empieza un operando nuevo? Decide dónde hay producto implícito.
    private func startsOperand(_ token: Token) -> Bool {
        switch token {
        case .digit, .decimalPoint, .constant, .variable:
            return true
        case let .groupStart(kind):
            return !kind.attachesToPrecedingOperand
        default:
            return false
        }
    }
}
