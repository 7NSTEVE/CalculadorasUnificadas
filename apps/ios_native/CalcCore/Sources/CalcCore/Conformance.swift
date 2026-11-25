import Foundation

/// Un caso de conformidad: una expresión y lo que la calculadora real contesta.
///
/// Los casos son *datos*, no código, a propósito. Así los ejecutan por igual el runner
/// local de Windows (`swiftc`, porque SwiftPM está roto en esa máquina) y XCTest en el
/// CI de macOS, y encima se pueden generar por millares desde una referencia externa
/// de precisión arbitraria.
public struct ConformanceCase: Sendable {

    public enum Expectation: Sendable, Equatable {
        /// Valor esperado. `tolerance == 0` exige igualdad binaria exacta.
        case value(Double)
        case error(CalcError)
        case anyError
    }

    public let source: String
    public let expectation: Expectation
    public let angleMode: AngleMode
    /// Tolerancia relativa. Cero significa "clavado", para los ángulos notables.
    public let relativeTolerance: Double
    public let note: String

    public init(
        _ source: String,
        _ expectation: Expectation,
        angleMode: AngleMode = .degrees,
        relativeTolerance: Double = 1e-10,
        note: String = ""
    ) {
        self.source = source
        self.expectation = expectation
        self.angleMode = angleMode
        self.relativeTolerance = relativeTolerance
        self.note = note
    }
}

public struct ConformanceFailure: Sendable {
    public let source: String
    public let detail: String
    public let note: String
}

public enum ConformanceRunner {

    /// Evalúa un caso y devuelve la descripción del fallo, o `nil` si pasa.
    public static func check(_ testCase: ConformanceCase) -> ConformanceFailure? {
        func failure(_ detail: String) -> ConformanceFailure {
            ConformanceFailure(source: testCase.source, detail: detail, note: testCase.note)
        }

        let expression: Expression
        do {
            expression = try SourceTokenizer.parse(testCase.source)
        } catch {
            return failure("no se pudo tokenizar: \(error)")
        }

        let evaluator = Evaluator(angleMode: testCase.angleMode)
        let result = Result { try evaluator.evaluate(expression) }

        switch (testCase.expectation, result) {
        case let (.value(expected), .success(actual)):
            if testCase.relativeTolerance == 0 {
                guard actual == expected else {
                    return failure("esperaba exactamente \(expected), obtuve \(actual)")
                }
                return nil
            }
            let allowed = testCase.relativeTolerance * Swift.max(1, abs(expected))
            guard abs(actual - expected) <= allowed else {
                return failure("esperaba \(expected), obtuve \(actual) (dif \(abs(actual - expected)))")
            }
            return nil

        case let (.value(expected), .failure(error)):
            return failure("esperaba \(expected), pero dio error \(error)")

        case let (.error(expected), .failure(error)):
            guard let calcError = error as? CalcError, calcError == expected else {
                return failure("esperaba \(expected), obtuve otro error: \(error)")
            }
            return nil

        case (.anyError, .failure):
            return nil

        case let (.error(expected), .success(actual)):
            return failure("esperaba error \(expected), pero devolvió \(actual)")

        case let (.anyError, .success(actual)):
            return failure("esperaba un error, pero devolvió \(actual)")
        }
    }

    public static func run(_ cases: [ConformanceCase]) -> [ConformanceFailure] {
        cases.compactMap(check)
    }
}
