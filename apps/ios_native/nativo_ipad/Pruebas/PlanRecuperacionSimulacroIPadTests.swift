import XCTest
@testable import CuadernUEstudio

final class PlanRecuperacionSimulacroIPadTests: XCTestCase {
    func testPriorizaErrorConAltaConfianzaAntesDeOtrosFallos() {
        let normal = resultado(.repetir, confianza: 0.4, segundos: 90,
                               titulo: "Fallo normal")
        let peligroso = resultado(.repetir, confianza: 0.9, segundos: 100,
                                  error: "Signo incorrecto", titulo: "Fallo seguro")
        let inseguro = resultado(.correcto, confianza: 0.2, segundos: 80,
                                 titulo: "Acierto dudoso")
        let informe = AnalizadorSimulacroIPad.analizar([normal, peligroso, inseguro])

        let plan = PlanificadorRecuperacionSimulacroIPad.construir(informe)

        XCTAssertEqual(plan.first?.frameId, peligroso.frameId)
        XCTAssertEqual(plan.first?.prioridad, .critica)
        XCTAssertEqual(plan.first?.tipo, .corregirModeloMental)
        XCTAssertEqual(plan.dropFirst().first?.frameId, normal.frameId)
    }

    func testUnaAccionCombinaFalloYLentitudSinDuplicarFrame() {
        let lento = resultado(.repetir, confianza: 0.8, segundos: 600,
                              error: "KVL", titulo: "Circuito")
        let informe = AnalizadorSimulacroIPad.analizar([
            resultado(.correcto, confianza: 0.8, segundos: 100),
            resultado(.correcto, confianza: 0.8, segundos: 110),
            lento,
            resultado(.correcto, confianza: 0.8, segundos: 120),
        ])

        let plan = PlanificadorRecuperacionSimulacroIPad.construir(informe)
        let accion = plan.first { $0.frameId == lento.frameId }

        XCTAssertEqual(plan.filter { $0.frameId == lento.frameId }.count, 1)
        XCTAssertEqual(accion?.tipo, .corregirModeloMental)
        XCTAssertTrue(accion?.senales.contains("Tiempo atípico dentro de esta sesión") == true)
        XCTAssertTrue(accion?.senales.contains { $0.contains("KVL") } == true)
    }

    func testNoIncluyeAciertoNormalSinSenales() {
        let estable = resultado(.correcto, confianza: 0.85, segundos: 100)
        let informe = AnalizadorSimulacroIPad.analizar([estable])

        XCTAssertTrue(
            PlanificadorRecuperacionSimulacroIPad.construir(informe).isEmpty)
    }

    func testAciertoInseguroProponeConsolidarConfianza() {
        let dudoso = resultado(.correcto, confianza: 0.3, segundos: 100)
        let informe = AnalizadorSimulacroIPad.analizar([dudoso])

        let accion = PlanificadorRecuperacionSimulacroIPad.construir(informe).first

        XCTAssertEqual(accion?.tipo, .consolidarConfianza)
        XCTAssertEqual(accion?.prioridad, .media)
        XCTAssertTrue(accion?.criterioCierre.contains("70 %") == true)
    }

    func testRespetaLimiteSinCambiarOrdenExplicable() {
        let peligroso = resultado(.repetir, confianza: 0.95, segundos: 100,
                                  titulo: "Crítico")
        let fallo = resultado(.repetir, confianza: 0.4, segundos: 100,
                              titulo: "Alto")
        let dudoso = resultado(.correcto, confianza: 0.2, segundos: 100,
                               titulo: "Medio")
        let informe = AnalizadorSimulacroIPad.analizar([fallo, dudoso, peligroso])

        let plan = PlanificadorRecuperacionSimulacroIPad.construir(informe, limite: 2)

        XCTAssertEqual(plan.count, 2)
        XCTAssertEqual(plan.map(\.titulo), ["Crítico", "Alto"])
        XCTAssertEqual(
            PlanificadorRecuperacionSimulacroIPad.construir(informe, limite: 0), [])
    }

    func testDeduplicaEntradaCorruptaConFrameRepetido() {
        let frame = UUID()
        let primero = ResultadoEjercicioSimulacroIPad(
            frameId: frame, titulo: "Primero", resultado: .repetir,
            confianza: 0.9, segundos: 100, error: "Error")
        let repetido = ResultadoEjercicioSimulacroIPad(
            frameId: frame, titulo: "Duplicado", resultado: .repetir,
            confianza: 0.9, segundos: 100, error: "Error")
        let informe = AnalizadorSimulacroIPad.analizar([primero, repetido])

        let plan = PlanificadorRecuperacionSimulacroIPad.construir(informe)

        XCTAssertEqual(plan.count, 1)
        XCTAssertEqual(plan.first?.titulo, "Primero")
    }

    private func resultado(
        _ resultado: ResultadoIntentoFrameIPad,
        confianza: Double,
        segundos: TimeInterval,
        error: String = "",
        titulo: String = "Ejercicio"
    ) -> ResultadoEjercicioSimulacroIPad {
        ResultadoEjercicioSimulacroIPad(
            frameId: UUID(), titulo: titulo,
            resultado: resultado, confianza: confianza,
            segundos: segundos, error: error)
    }
}
