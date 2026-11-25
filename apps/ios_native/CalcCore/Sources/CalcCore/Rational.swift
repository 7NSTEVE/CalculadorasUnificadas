import Foundation

/// Racional exacto con denominador siempre positivo y fracción siempre reducida.
///
/// Se usa como soporte de la conversión S⇔D (mostrar 0.5 como 1/2, 1.047… como π/3,
/// 1.414… como √2). Todas las operaciones detectan desbordamiento y devuelven `nil`
/// en lugar de envolver silenciosamente: si un resultado no cabe, la calculadora
/// simplemente no ofrece forma exacta y se queda en decimal.
public struct Rational: Hashable, Sendable {
    /// Numerador. Lleva el signo del racional.
    public let num: Int
    /// Denominador. Siempre estrictamente positivo.
    public let den: Int

    public init?(_ num: Int, _ den: Int) {
        // Int.min no tiene opuesto representable: se descarta de entrada.
        guard den != 0, num != Int.min, den != Int.min else { return nil }
        var n = num
        var d = den
        if d < 0 {
            n = -n
            d = -d
        }
        let g = Rational.gcd(n.magnitude, d.magnitude)
        if g > 1 {
            n /= Int(g)
            d /= Int(g)
        }
        self.num = n
        self.den = d
    }

    public init(integer: Int) {
        self.num = integer
        self.den = 1
    }

    public static let zero = Rational(integer: 0)
    public static let one = Rational(integer: 1)

    public var isInteger: Bool { den == 1 }
    public var isZero: Bool { num == 0 }
    public var isNegative: Bool { num < 0 }
    public var doubleValue: Double { Double(num) / Double(den) }

    /// Valor absoluto. `nil` solo en el caso patológico de `Int.min`, ya excluido en `init`.
    public var magnitudeValue: Rational? { Rational(abs(num), den) }

    public var negated: Rational? { Rational(-num, den) }

    /// Parte entera truncada hacia cero y resto, para mostrar fracciones mixtas (a b/c).
    public var mixedParts: (whole: Int, remainder: Rational)? {
        let whole = num / den
        guard let rem = Rational(num - whole * den, den) else { return nil }
        return (whole, rem)
    }

    // MARK: - Aritmética

    public static func + (lhs: Rational, rhs: Rational) -> Rational? {
        // Se reduce por el mcd de los denominadores antes de multiplicar para que
        // sumas como 1/3 + 1/3 no inflen el denominador innecesariamente.
        let g = gcd(lhs.den.magnitude, rhs.den.magnitude)
        guard g > 0 else { return nil }
        let lhsScale = rhs.den / Int(g)
        guard let a = mul(lhs.num, lhsScale),
              let b = mul(rhs.num, lhs.den / Int(g)),
              let n = add(a, b),
              let d = mul(lhs.den, lhsScale)
        else { return nil }
        return Rational(n, d)
    }

    public static func - (lhs: Rational, rhs: Rational) -> Rational? {
        guard let negated = rhs.negated else { return nil }
        return lhs + negated
    }

    public static func * (lhs: Rational, rhs: Rational) -> Rational? {
        // Cruzar los mcd antes de multiplicar reduce mucho el riesgo de desbordar.
        guard let a = Rational(lhs.num, rhs.den), let b = Rational(rhs.num, lhs.den) else { return nil }
        guard let n = mul(a.num, b.num), let d = mul(a.den, b.den) else { return nil }
        return Rational(n, d)
    }

    public static func / (lhs: Rational, rhs: Rational) -> Rational? {
        guard !rhs.isZero else { return nil }
        guard let inverted = Rational(rhs.den, rhs.num) else { return nil }
        return lhs * inverted
    }

    /// Potencia entera. Exponentes negativos invierten la base.
    public func power(_ exponent: Int) -> Rational? {
        if exponent == 0 { return isZero ? nil : .one }
        if exponent < 0 {
            guard !isZero, exponent != Int.min, let inverted = Rational(den, num) else { return nil }
            return inverted.power(-exponent)
        }
        var result = Rational.one
        var base = self
        var e = exponent
        while e > 0 {
            if e & 1 == 1 {
                guard let next = result * base else { return nil }
                result = next
            }
            e >>= 1
            if e > 0 {
                guard let squared = base * base else { return nil }
                base = squared
            }
        }
        return result
    }

    // MARK: - Aritmética protegida

    private static func add(_ a: Int, _ b: Int) -> Int? {
        let (value, overflow) = a.addingReportingOverflow(b)
        return overflow ? nil : value
    }

    private static func mul(_ a: Int, _ b: Int) -> Int? {
        let (value, overflow) = a.multipliedReportingOverflow(by: b)
        return overflow ? nil : value
    }

    static func gcd(_ a: UInt, _ b: UInt) -> UInt {
        var a = a
        var b = b
        while b != 0 {
            (a, b) = (b, a % b)
        }
        return a
    }

    /// Aproxima un `Double` por fracciones continuas, con el denominador acotado.
    ///
    /// Devuelve `nil` si no hay ninguna fracción dentro de `tolerance` cuyo
    /// denominador no pase de `maxDenominator`.
    public static func approximating(
        _ value: Double,
        maxDenominator: Int,
        tolerance: Double
    ) -> Rational? {
        guard value.isFinite, abs(value) < 1e15 else { return nil }
        let negative = value < 0
        var x = abs(value)

        // Convergentes sucesivos h/k del desarrollo en fracción continua de x.
        var hPrev = 0, h = 1
        var kPrev = 1, k = 0

        for _ in 0..<40 {
            let a = x.rounded(.down)
            guard a < 1e15 else { break }
            let ai = Int(a)

            let (h1, overflowH) = ai.multipliedReportingOverflow(by: h)
            let (k1, overflowK) = ai.multipliedReportingOverflow(by: k)
            guard !overflowH, !overflowK else { break }
            let (hNext, oh) = h1.addingReportingOverflow(hPrev)
            let (kNext, ok) = k1.addingReportingOverflow(kPrev)
            guard !oh, !ok else { break }

            hPrev = h; h = hNext
            kPrev = k; k = kNext

            if k > maxDenominator { break }
            if k > 0 {
                let candidate = Double(h) / Double(k)
                if abs(candidate - abs(value)) <= tolerance * max(1, abs(value)) {
                    return Rational(negative ? -h : h, k)
                }
            }

            let frac = x - a
            if frac < 1e-15 { break }
            x = 1 / frac
        }
        return nil
    }
}

extension Rational: CustomStringConvertible {
    public var description: String { den == 1 ? "\(num)" : "\(num)/\(den)" }
}
