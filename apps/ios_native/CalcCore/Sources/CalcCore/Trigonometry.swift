import Foundation

/// Trigonometría con los ángulos notables clavados.
///
/// `sin(30°)` calculado como `sin(30 · π/180)` da 0.49999999999999994, y una Casio
/// muestra 0.5. La diferencia importa: al encadenar operaciones ese error se propaga y
/// acaba saliendo en pantalla. Por eso los múltiplos de 30° y 45° se resuelven por tabla
/// y solo el resto pasa por `Foundation`.
enum Trigonometry {

    // MARK: - Conversión de unidades

    static func toRadians(_ value: Double, mode: AngleMode) -> Double {
        switch mode {
        case .radians: return value
        case .degrees: return value * .pi / 180
        case .gradians: return value * .pi / 200
        }
    }

    static func fromRadians(_ radians: Double, mode: AngleMode) -> Double {
        switch mode {
        case .radians: return radians
        case .degrees: return snap(radians * 180 / .pi)
        case .gradians: return snap(radians * 200 / .pi)
        }
    }

    /// Redondea resultados que se quedan a un pelo de un entero.
    ///
    /// `atan(1)` en grados da 45.00000000000001; la calculadora enseña 45.
    private static func snap(_ value: Double) -> Double {
        let rounded = value.rounded()
        let tolerance = 1e-9 * Swift.max(1, abs(value))
        return abs(value - rounded) < tolerance ? rounded : value
    }

    // MARK: - Directas

    static func sine(_ value: Double, mode: AngleMode) throws -> Double {
        switch mode {
        case .radians: return sin(value)
        case .degrees: return sineOfDegrees(value)
        case .gradians: return sineOfDegrees(value * 0.9)
        }
    }

    static func cosine(_ value: Double, mode: AngleMode) throws -> Double {
        switch mode {
        case .radians: return cos(value)
        case .degrees: return sineOfDegrees(value + 90)
        case .gradians: return sineOfDegrees(value * 0.9 + 90)
        }
    }

    static func tangent(_ value: Double, mode: AngleMode) throws -> Double {
        if mode == .radians { return tan(value) }
        let degrees = mode == .gradians ? value * 0.9 : value
        let cosine = sineOfDegrees(degrees + 90)
        // Con los ángulos notables exactos, cos(90°) vale 0 clavado y esto detecta
        // la asíntota igual que la calculadora real, que da Math ERROR.
        guard cosine != 0 else { throw CalcError.mathError }
        return sineOfDegrees(degrees) / cosine
    }

    // MARK: - Seno en grados

    private static func sineOfDegrees(_ degrees: Double) -> Double {
        if let exact = exactSineOfDegrees(degrees) { return exact }
        return sin(degrees * .pi / 180)
    }

    /// Valor exacto si el ángulo es un múltiplo entero de 30° o 45°.
    private static func exactSineOfDegrees(_ degrees: Double) -> Double? {
        guard abs(degrees) < 1e9 else { return nil }
        let rounded = degrees.rounded()
        guard abs(degrees - rounded) < 1e-9 else { return nil }

        let normalized = ((Int(rounded) % 360) + 360) % 360
        let quadrant = normalized / 90
        let within = normalized % 90

        switch quadrant {
        case 0: return quarterTurnSine(within)
        case 1: return quarterTurnSine(90 - within)
        case 2: return quarterTurnSine(within).map { -$0 }
        default: return quarterTurnSine(90 - within).map { -$0 }
        }
    }

    /// Seno de los ángulos notables del primer cuadrante, extremos incluidos.
    private static func quarterTurnSine(_ degrees: Int) -> Double? {
        switch degrees {
        case 0: return 0
        case 30: return 0.5
        case 45: return 2.0.squareRoot() / 2
        case 60: return 3.0.squareRoot() / 2
        case 90: return 1
        default: return nil
        }
    }
}
