import XCTest
@testable import CuadernUEstudio

final class AnaliticaSimulacroIPadTests: XCTestCase {
    func testSeparaAciertoInseguroDeErrorConAltaConfianza() {
        let a = resultado(.correcto, confianza: 0.3, segundos: 100)
        let b = resultado(.repetir, confianza: 0.9, segundos: 110,
                          error: "Olvidé el signo")
        let c = resultado(.correcto, confianza: 0.8, segundos: 120)

        let informe = AnalizadorSimulacroIPad.analizar([a, b, c])

        XCTAssertEqual(informe.aciertos, 2)
        XCTAssertEqual(informe.fallos, 1)
        XCTAssertEqual(informe.aciertosInseguros.map(\.frameId), [a.frameId])
        XCTAssertEqual(informe.erroresAltaConfianza.map(\.frameId), [b.frameId])
        XCTAssertTrue(informe.requiereRevisionPrioritaria)
    }

    func testDetectaEjercicioMuyLentoRespectoALaMediana() {
        let resultados = [
            resultado(.correcto, confianza: 0.8, segundos: 100),
            resultado(.correcto, confianza: 0.8, segundos: 110),
            resultado(.correcto, confianza: 0.8, segundos: 500),
            resultado(.correcto, confianza: 0.8, segundos: 120),
        ]

        let informe = AnalizadorSimulacroIPad.analizar(resultados)

        XCTAssertEqual(informe.ejerciciosLentos.count, 1)
        XCTAssertEqual(informe.ejerciciosLentos.first?.segundos, 500)
    }

    func testNoMarcaLentosConSoloDosEjercicios() {
        let informe = AnalizadorSimulacroIPad.analizar([
            resultado(.correcto, confianza: 0.8, segundos: 30),
            resultado(.correcto, confianza: 0.8, segundos: 500),
        ])

        XCTAssertTrue(informe.ejerciciosLentos.isEmpty)
    }

    func testNormalizaConfianzaTiempoYError() {
        let informe = AnalizadorSimulacroIPad.analizar([
            resultado(.repetir, confianza: 4.0, segundos: -10,
                      error: "  Error de signo  ")
        ])
        let valor = try! XCTUnwrap(informe.resultados.first)

        XCTAssertEqual(valor.confianza, 1, accuracy: 0.001)
        XCTAssertEqual(valor.segundos, 0, accuracy: 0.001)
        XCTAssertEqual(valor.error, "Error de signo")
    }

    func testPrioridadRevisionNoDuplicaElMismoFrame() {
        let peligroso = resultado(.repetir, confianza: 0.9, segundos: 500,
                                  error: "Ley de signos")
        let inseguro = resultado(.correcto, confianza: 0.2, segundos: 100)
        let normal = resultado(.repetir, confianza: 0.4, segundos: 110,
                               error: "Unidad")
        let informe = AnalizadorSimulacroIPad.analizar(
            [peligroso, inseguro, normal])

        let prioridad = AnalizadorSimulacroIPad.prioridadRevision(informe)

        XCTAssertEqual(prioridad.first?.frameId, peligroso.frameId)
        XCTAssertEqual(Set(prioridad.map(\.frameId)).count, prioridad.count)
        XCTAssertTrue(prioridad.contains { $0.frameId == inseguro.frameId })
        XCTAssertTrue(prioridad.contains { $0.frameId == normal.frameId })
    }

    func testInformeVacioEsSeguro() {
        let informe = AnalizadorSimulacroIPad.analizar([])
        XCTAssertEqual(informe.precision, 0)
        XCTAssertEqual(informe.confianzaPromedio, 0)
        XCTAssertFalse(informe.requiereRevisionPrioritaria)
    }

    private func resultado(
        _ resultado: ResultadoIntentoFrameIPad,
        confianza: Double,
        segundos: TimeInterval,
        error: String = ""
    ) -> ResultadoEjercicioSimulacroIPad {
        ResultadoEjercicioSimulacroIPad(
            frameId: UUID(), titulo: "Ejercicio",
            resultado: resultado, confianza: confianza,
            segundos: segundos, error: error)
    }
}
