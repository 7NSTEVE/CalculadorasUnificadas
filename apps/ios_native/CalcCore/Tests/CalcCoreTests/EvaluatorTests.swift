import XCTest
@testable import CalcCore

/// Comprobaciones de comportamiento contra lo que hace la calculadora real.
///
/// Los valores esperados salen del manual de la fx-570 y de cálculo exacto, no de
/// ejecutar este mismo código: si el motor se equivoca, estos tests deben caerse.
final class EvaluatorTests: XCTestCase {

    private func evaluate(
        _ source: String,
        angleMode: AngleMode = .degrees,
        variables: [Variable: Double] = [:],
        lastAnswer: Double = 0
    ) throws -> Double {
        let expression = try SourceTokenizer.parse(source)
        let evaluator = Evaluator(
            angleMode: angleMode, variables: variables, lastAnswer: lastAnswer
        )
        return try evaluator.evaluate(expression)
    }

    private func assertClose(
        _ source: String,
        _ expected: Double,
        angleMode: AngleMode = .degrees,
        accuracy: Double = 1e-10,
        file: StaticString = #filePath,
        line: UInt = #line
    ) throws {
        let actual = try evaluate(source, angleMode: angleMode)
        XCTAssertEqual(actual, expected, accuracy: accuracy * Swift.max(1, abs(expected)),
                       "\(source)", file: file, line: line)
    }

    // MARK: - Prioridad de operadores

    func testMultiplicationBeforeAddition() throws {
        try assertClose("1+2*3", 7)
        try assertClose("2*3+1", 7)
        try assertClose("8-2*3", 2)
    }

    func testDivisionIsLeftAssociative() throws {
        try assertClose("100/5/2", 10)
    }

    /// En la Casio la multiplicación implícita liga más que `÷`: `1÷2π` es `1÷(2π)`.
    func testImplicitProductBindsTighterThanExplicitDivision() throws {
        try assertClose("1/2pi", 1 / (2 * Double.pi))
        try assertClose("1/2*pi", Double.pi / 2)
    }

    func testImplicitProductWithParentheses() throws {
        try assertClose("2(3+4)", 14)
        try assertClose("3sqrt(4)", 6)
    }

    /// `−2²` son `−4`: el cuadrado se aplica antes que el signo.
    func testUnaryMinusBindsLooserThanPostfix() throws {
        try assertClose("~2sq", -4)
        try assertClose("~2^(2)", -4)
    }

    func testPermutationsAndCombinationsBindTighterThanTimes() throws {
        try assertClose("10nPr3", 720)
        try assertClose("10nCr3", 120)
        try assertClose("2*10nCr3", 240)
    }

    // MARK: - Potencias, raíces y sufijos

    func testPowersAndRoots() throws {
        try assertClose("2^(10)", 1024)
        try assertClose("sqrt(9)", 3)
        try assertClose("3rt(8)", 2)
        try assertClose("cbrt(27)", 3)
        try assertClose("2^(0.5)", 2.0.squareRoot())
    }

    /// Índice impar admite radicando negativo; índice par, no.
    func testOddRootOfNegativeNumber() throws {
        try assertClose("cbrt(~8)", -2)
        XCTAssertThrowsError(try evaluate("sqrt(~4)"))
    }

    func testPostfixOperators() throws {
        try assertClose("5!", 120)
        try assertClose("0!", 1)
        try assertClose("4inv", 0.25)
        try assertClose("3cb", 27)
    }

    func testFactorialRejectsNonIntegerAndTooLarge() throws {
        XCTAssertThrowsError(try evaluate("2.5!"))
        XCTAssertThrowsError(try evaluate("70!"))
    }

    // MARK: - Porcentaje

    /// El `%` de Casio es relativo cuando se suma o se resta.
    func testPercentIsRelativeForAdditionAndSubtraction() throws {
        try assertClose("200+10%", 220)
        try assertClose("200-10%", 180)
    }

    func testPercentIsPlainFractionElsewhere() throws {
        try assertClose("200*10%", 20)
        try assertClose("50%", 0.5)
    }

    // MARK: - Trigonometría

    /// Los ángulos notables deben salir exactos, no 0.49999999999999994.
    func testExactTrigonometryInDegrees() throws {
        XCTAssertEqual(try evaluate("sin(30)"), 0.5)
        XCTAssertEqual(try evaluate("cos(60)"), 0.5)
        XCTAssertEqual(try evaluate("sin(90)"), 1)
        XCTAssertEqual(try evaluate("cos(90)"), 0)
        XCTAssertEqual(try evaluate("sin(180)"), 0)
        XCTAssertEqual(try evaluate("tan(45)"), 1)
        XCTAssertEqual(try evaluate("sin(~30)"), -0.5)
    }

    func testTangentAsymptoteIsMathError() throws {
        XCTAssertThrowsError(try evaluate("tan(90)")) { error in
            XCTAssertEqual(error as? CalcError, .mathError)
        }
        XCTAssertThrowsError(try evaluate("tan(270)"))
    }

    func testAngleModes() throws {
        try assertClose("sin(pi/2)", 1, angleMode: .radians)
        try assertClose("sin(100)", 1, angleMode: .gradians)
        try assertClose("cos(0)", 1, angleMode: .radians)
    }

    func testInverseTrigonometrySnapsToWholeDegrees() throws {
        XCTAssertEqual(try evaluate("asin(0.5)"), 30)
        XCTAssertEqual(try evaluate("atan(1)"), 45)
        XCTAssertEqual(try evaluate("acos(0)"), 90)
    }

    func testInverseTrigonometryDomain() throws {
        XCTAssertThrowsError(try evaluate("asin(2)"))
        XCTAssertThrowsError(try evaluate("acos(~2)"))
    }

    // MARK: - Logaritmos y exponenciales

    func testLogarithms() throws {
        try assertClose("log(100)", 2)
        try assertClose("ln(e)", 1)
        try assertClose("logb(2,8)", 3)
        try assertClose("logb(3,81)", 4)
    }

    func testLogarithmDomain() throws {
        XCTAssertThrowsError(try evaluate("ln(0)"))
        XCTAssertThrowsError(try evaluate("log(~5)"))
        XCTAssertThrowsError(try evaluate("logb(1,8)"))
    }

    // MARK: - Fracciones

    func testFractions() throws {
        try assertClose("frac(1,2)", 0.5)
        try assertClose("frac(1,3)+frac(1,6)", 0.5)
        try assertClose("mixed(2,1,2)", 2.5)
    }

    /// En una fracción mixta negativa el signo afecta al conjunto: −2 1/2 es −2.5.
    func testNegativeMixedFraction() throws {
        try assertClose("mixed(~2,1,2)", -2.5)
    }

    func testDivisionByZeroIsMathError() throws {
        XCTAssertThrowsError(try evaluate("1/0")) { error in
            XCTAssertEqual(error as? CalcError, .mathError)
        }
        XCTAssertThrowsError(try evaluate("frac(1,0)"))
    }

    // MARK: - Literales y notación científica

    func testExponentLiterals() throws {
        try assertClose("2E3", 2000)
        try assertClose("2E~3", 0.002)
        try assertClose("1.5E2", 150)
    }

    // MARK: - Variables y Ans

    func testVariablesAndAnswer() throws {
        XCTAssertEqual(try evaluate("$A+$B", variables: [.a: 3, .b: 4]), 7)
        XCTAssertEqual(try evaluate("Ans*2", lastAnswer: 21), 42)
        // Una variable sin asignar vale cero, igual que tras un RESET.
        XCTAssertEqual(try evaluate("$X"), 0)
    }

    // MARK: - Errores

    func testIncompleteExpressionIsReported() throws {
        XCTAssertThrowsError(try evaluate("1+")) { error in
            XCTAssertEqual(error as? CalcError, .incompleteExpression)
        }
        XCTAssertThrowsError(try evaluate("sqrt()"))
    }

    func testRangeOverflow() throws {
        XCTAssertThrowsError(try evaluate("9E99*10")) { error in
            XCTAssertEqual(error as? CalcError, .rangeError)
        }
    }

    /// Una entrada rara no debe colgar el analizador ni petar.
    func testParserToleratesGarbage() throws {
        XCTAssertNoThrow(try SourceTokenizer.parse("2)"))
        XCTAssertNoThrow(try SourceTokenizer.parse(")))"))
        XCTAssertNoThrow(try SourceTokenizer.parse("sqrt(((("))
        XCTAssertNoThrow(try SourceTokenizer.parse("+++"))
    }
}
