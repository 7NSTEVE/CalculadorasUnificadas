import Foundation

/// Traduce una expresión escrita en texto a la secuencia de tokens que produciría el teclado.
///
/// No es lo que usa la app —ahí cada tecla inserta sus tokens directamente— sino la puerta de
/// entrada para las pruebas: permite escribir miles de casos como cadenas y compararlos contra
/// una referencia de precisión arbitraria.
///
/// Sintaxis:
/// - números `12.5`, exponente con `E` mayúscula: `2E3` = 2×10³
/// - operadores `+ - * /`, y `nPr` / `nCr`
/// - `~` es el signo negativo de la tecla `(−)`, distinto de la resta
/// - cajas: `(x)`, `sqrt(x)`, `cbrt(x)`, `abs(x)`, `frac(a,b)`, `mixed(a,b,c)`, `logb(b,x)`
/// - potencia `2^(3)` y raíz n-ésima `3rt(8)`, ambas pegadas al operando anterior
/// - sufijos: `sq` (x²), `cb` (x³), `inv` (x⁻¹), `!`, `%`
/// - constantes `pi`, `e`, `Ans`; variables con `$`: `$A`, `$M`, `$X`
public enum SourceTokenizer {

    public enum TokenizerError: Error, Equatable {
        case unexpectedCharacter(Character, at: Int)
        case unknownVariable(String, at: Int)
    }

    /// Palabras clave que abren caja. Se prueban por orden de longitud descendente.
    private static let groupKeywords: [(String, GroupKind)] = [
        ("asinh(", .function(.areaHyperbolicSine)),
        ("acosh(", .function(.areaHyperbolicCosine)),
        ("atanh(", .function(.areaHyperbolicTangent)),
        ("mixed(", .mixedFraction),
        ("sinh(", .function(.hyperbolicSine)),
        ("cosh(", .function(.hyperbolicCosine)),
        ("tanh(", .function(.hyperbolicTangent)),
        ("asin(", .function(.arcsine)),
        ("acos(", .function(.arccosine)),
        ("atan(", .function(.arctangent)),
        ("sqrt(", .squareRoot),
        ("cbrt(", .cubeRoot),
        ("frac(", .fraction),
        ("logb(", .logarithmWithBase),
        ("intg(", .function(.floorPart)),
        ("pow10(", .function(.powerOfTen)),
        ("sin(", .function(.sine)),
        ("cos(", .function(.cosine)),
        ("tan(", .function(.tangent)),
        ("exp(", .function(.exponential)),
        ("abs(", .absoluteValue),
        ("rnd(", .function(.roundToDisplay)),
        ("int(", .function(.integerPart)),
        ("log(", .function(.commonLog)),
        ("ln(", .function(.naturalLog)),
        ("rt(", .nthRoot),
        ("^(", .power),
        ("(", .parenthesis),
    ]

    private static let wordKeywords: [(String, [Token])] = [
        ("nPr", [.binaryOperator(.permutations)]),
        ("nCr", [.binaryOperator(.combinations)]),
        ("Ans", [.constant(.answer)]),
        ("Ran", [.constant(.randomNumber)]),
        ("inv", [.postfixOperator(.reciprocal)]),
        ("sq", [.postfixOperator(.square)]),
        ("cb", [.postfixOperator(.cube)]),
        ("pi", [.constant(.pi)]),
        ("e", [.constant(.eulerNumber)]),
    ]

    public static func tokenize(_ source: String) throws -> [Token] {
        var tokens: [Token] = []
        let characters = Array(source)
        var position = 0

        func matches(_ keyword: String) -> Bool {
            let letters = Array(keyword)
            guard position + letters.count <= characters.count else { return false }
            return Array(characters[position..<(position + letters.count)]) == letters
        }

        outer: while position < characters.count {
            let character = characters[position]

            if character == " " || character == "\t" {
                position += 1
                continue
            }

            if let digit = character.wholeNumberValue, character.isNumber {
                tokens.append(.digit(digit))
                position += 1
                continue
            }

            switch character {
            case ".":
                tokens.append(.decimalPoint); position += 1; continue
            case "E":
                tokens.append(.exponentMark); position += 1; continue
            case "+":
                tokens.append(.binaryOperator(.add)); position += 1; continue
            case "-":
                tokens.append(.binaryOperator(.subtract)); position += 1; continue
            case "*":
                tokens.append(.binaryOperator(.multiply)); position += 1; continue
            case "/":
                tokens.append(.binaryOperator(.divide)); position += 1; continue
            case "~":
                tokens.append(.unaryMinus); position += 1; continue
            case ")":
                tokens.append(.groupEnd); position += 1; continue
            case ",":
                tokens.append(.argumentSeparator); position += 1; continue
            case "!":
                tokens.append(.postfixOperator(.factorial)); position += 1; continue
            case "%":
                tokens.append(.postfixOperator(.percent)); position += 1; continue
            case "$":
                position += 1
                guard position < characters.count else {
                    throw TokenizerError.unknownVariable("", at: position)
                }
                let name = String(characters[position])
                guard let variable = Variable(rawValue: name) else {
                    throw TokenizerError.unknownVariable(name, at: position)
                }
                tokens.append(.variable(variable))
                position += 1
                continue
            default:
                break
            }

            for (keyword, kind) in groupKeywords where matches(keyword) {
                tokens.append(.groupStart(kind))
                position += keyword.count
                continue outer
            }
            for (keyword, produced) in wordKeywords where matches(keyword) {
                tokens.append(contentsOf: produced)
                position += keyword.count
                continue outer
            }

            throw TokenizerError.unexpectedCharacter(character, at: position)
        }

        return tokens
    }

    /// Atajo para pruebas: texto → árbol.
    public static func parse(_ source: String) throws -> Expression {
        Parser.parse(try tokenize(source))
    }
}
