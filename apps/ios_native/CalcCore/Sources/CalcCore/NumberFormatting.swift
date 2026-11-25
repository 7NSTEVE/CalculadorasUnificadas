import Foundation

/// Formato de presentación del resultado (menú `SETUP`).
public enum DisplayFormat: Sendable, Equatable {
    /// `Norm 1` pasa a notación científica por debajo de 10⁻², `Norm 2` por debajo de 10⁻⁹.
    case norm(Int)
    /// `Fix n`: n decimales fijos.
    case fixed(Int)
    /// `Sci n`: n cifras significativas.
    case scientific(Int)
    /// `Eng`: exponente múltiplo de 3.
    case engineering
}

/// Resultado ya partido en mantisa y exponente, para que el LCD pinte el `×10ⁿ`
/// con el exponente en superíndice igual que la calculadora.
public struct FormattedNumber: Sendable, Equatable {
    public let mantissa: String
    public let exponent: Int?

    public init(mantissa: String, exponent: Int? = nil) {
        self.mantissa = mantissa
        self.exponent = exponent
    }

    /// Versión en texto plano, sobre todo para tests y depuración.
    public var plainText: String {
        guard let exponent else { return mantissa }
        return "\(mantissa)×10^\(exponent)"
    }
}

public enum NumberFormatting {

    /// Dígitos significativos que muestra la fx-570/991 en pantalla.
    public static let displayDigits = 10

    public static func roundToSignificantDigits(_ value: Double, digits: Int) -> Double {
        guard value != 0, value.isFinite else { return value }
        let exponent = Int(floor(log10(abs(value))))
        let shift = digits - 1 - exponent
        // Se acota el desplazamiento para no perder el número al multiplicar.
        guard abs(shift) < 300 else { return value }
        let factor = pow(10.0, Double(shift))
        return (value * factor).rounded() / factor
    }

    public static func format(_ value: Double, as format: DisplayFormat) -> FormattedNumber {
        guard value.isFinite else { return FormattedNumber(mantissa: "Error") }
        if value == 0 {
            switch format {
            case let .fixed(decimals):
                return FormattedNumber(mantissa: fixedString(0, decimals: decimals))
            case let .scientific(digits):
                return FormattedNumber(mantissa: fixedString(0, decimals: max(0, digits - 1)),
                                       exponent: 0)
            default:
                return FormattedNumber(mantissa: "0")
            }
        }

        switch format {
        case let .norm(variant):
            let lowerBound = variant == 1 ? 1e-2 : 1e-9
            let magnitude = abs(value)
            if magnitude < lowerBound || magnitude >= 1e10 {
                return scientific(value, significantDigits: displayDigits)
            }
            return FormattedNumber(mantissa: plain(value, significantDigits: displayDigits))

        case let .fixed(decimals):
            return FormattedNumber(mantissa: fixedString(value, decimals: decimals))

        case let .scientific(digits):
            return scientific(value, significantDigits: max(1, digits))

        case .engineering:
            return engineering(value)
        }
    }

    // MARK: - Variantes

    private static func plain(_ value: Double, significantDigits: Int) -> String {
        let exponent = Int(floor(log10(abs(value))))
        let decimals = max(0, min(15, significantDigits - 1 - exponent))
        return stripTrailingZeros(fixedString(value, decimals: decimals))
    }

    private static func scientific(_ value: Double, significantDigits: Int) -> FormattedNumber {
        var exponent = Int(floor(log10(abs(value))))
        var mantissaValue = value / pow(10.0, Double(exponent))
        // Redondear puede empujar la mantisa a 10.0: se reajusta el exponente.
        mantissaValue = roundToSignificantDigits(mantissaValue, digits: significantDigits)
        if abs(mantissaValue) >= 10 {
            mantissaValue /= 10
            exponent += 1
        }
        let text = stripTrailingZeros(
            fixedString(mantissaValue, decimals: max(0, significantDigits - 1))
        )
        return FormattedNumber(mantissa: text, exponent: exponent)
    }

    private static func engineering(_ value: Double) -> FormattedNumber {
        let rawExponent = Int(floor(log10(abs(value))))
        // Se baja al múltiplo de 3 inmediatamente inferior, que es lo que hace la tecla ENG.
        let exponent = Int(floor(Double(rawExponent) / 3)) * 3
        let mantissaValue = value / pow(10.0, Double(exponent))
        let digitsBeforePoint = Int(floor(log10(abs(mantissaValue)))) + 1
        let decimals = max(0, displayDigits - digitsBeforePoint)
        let text = stripTrailingZeros(fixedString(mantissaValue, decimals: decimals))
        return FormattedNumber(mantissa: text, exponent: exponent)
    }

    // MARK: - Apoyo

    private static func fixedString(_ value: Double, decimals: Int) -> String {
        let text = String(format: "%.\(max(0, decimals))f", value)
        // "-0" y "-0.00" se muestran como cero a secas.
        if text.allSatisfy({ $0 == "-" || $0 == "0" || $0 == "." }) {
            return String(text.dropFirst(text.hasPrefix("-") ? 1 : 0))
        }
        return text
    }

    private static func stripTrailingZeros(_ text: String) -> String {
        guard text.contains(".") else { return text }
        var result = text
        while result.hasSuffix("0") { result.removeLast() }
        if result.hasSuffix(".") { result.removeLast() }
        return result.isEmpty ? "0" : result
    }
}
