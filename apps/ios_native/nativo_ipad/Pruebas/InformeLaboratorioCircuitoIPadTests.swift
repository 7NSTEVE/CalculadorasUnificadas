import XCTest
@testable import CuadernUEstudio

final class InformeLaboratorioCircuitoIPadTests: XCTestCase {
    private func circuitoFuenteResistor() -> EsquemaEditable {
        var e = EsquemaEditable()
        e.titulo = "Fuente y resistor"
        e.nodos = [
            NodoEsquema(numero: 0, posicion: .zero, nombre: "GND"),
            NodoEsquema(numero: 1, posicion: CGPoint(x: 100, y: 0), nombre: "Vout"),
        ]
        e.componentes = [
            ComponenteEsquema(
                referencia: "V1", tipo: .fuenteVoltaje,
                nodoA: 1, nodoB: 0, posicion: CGPoint(x: 20, y: 40),
                valor: 10),
            ComponenteEsquema(
                referencia: "R1", tipo: .resistor,
                nodoA: 1, nodoB: 0, posicion: CGPoint(x: 150, y: 40),
                valor: 1_000),
        ]
        return e
    }

    func testInformeDeducCorrienteFuenteCierraKCLYPotencia() throws {
        let informe = GeneradorInformeLaboratorioCircuitoIPad.generar(
            circuitoFuenteResistor())

        XCTAssertNil(informe.errorSimulacion)
        XCTAssertEqual(informe.voltajes[1] ?? .nan, 10, accuracy: 1e-9)

        let r = try XCTUnwrap(informe.componentes.first { $0.referencia == "R1" })
        XCTAssertEqual(try XCTUnwrap(r.corrienteAB), 0.01, accuracy: 1e-9)
        XCTAssertEqual(try XCTUnwrap(r.potenciaAbsorbida), 0.1, accuracy: 1e-9)

        let fuente = try XCTUnwrap(informe.componentes.first { $0.referencia == "V1" })
        XCTAssertEqual(try XCTUnwrap(fuente.corrienteAB), -0.01, accuracy: 1e-9)
        XCTAssertEqual(try XCTUnwrap(fuente.potenciaAbsorbida), -0.1, accuracy: 1e-9)

        let nodo = try XCTUnwrap(informe.kcl.first { $0.nodo == 1 })
        XCTAssertTrue(nodo.completa)
        XCTAssertEqual(try XCTUnwrap(nodo.residuoAmperios), 0, accuracy: 1e-9)
        XCTAssertTrue(informe.balancePotenciaCompleto)
        XCTAssertEqual(try XCTUnwrap(informe.sumaPotencias), 0, accuracy: 1e-9)
        XCTAssertTrue(informe.aprobadoNumericamente)
    }

    func testFuenteCorrienteUsaLaMismaOrientacionQueMNA() throws {
        var e = EsquemaEditable()
        e.titulo = "Norton"
        e.nodos = [
            NodoEsquema(numero: 0, posicion: .zero, nombre: "GND"),
            NodoEsquema(numero: 1, posicion: CGPoint(x: 100, y: 0), nombre: "N1"),
        ]
        e.componentes = [
            ComponenteEsquema(
                referencia: "I1", tipo: .fuenteCorriente,
                nodoA: 1, nodoB: 0, posicion: .zero, valor: 0.001),
            ComponenteEsquema(
                referencia: "R1", tipo: .resistor,
                nodoA: 1, nodoB: 0, posicion: .zero, valor: 1_000),
        ]

        let informe = GeneradorInformeLaboratorioCircuitoIPad.generar(e)
        XCTAssertNil(informe.errorSimulacion)
        XCTAssertEqual(informe.voltajes[1] ?? .nan, -1, accuracy: 1e-9)
        let kcl = try XCTUnwrap(informe.kcl.first)
        XCTAssertEqual(try XCTUnwrap(kcl.residuoAmperios), 0, accuracy: 1e-9)
    }

    func testCapacitorDCSeReportaAbiertoConCorrienteCero() throws {
        var e = circuitoFuenteResistor()
        e.componentes.append(ComponenteEsquema(
            referencia: "C1", tipo: .capacitor,
            nodoA: 1, nodoB: 0, posicion: .zero, valor: 1e-6))

        let informe = GeneradorInformeLaboratorioCircuitoIPad.generar(e)
        let c = try XCTUnwrap(informe.componentes.first { $0.referencia == "C1" })
        XCTAssertEqual(try XCTUnwrap(c.corrienteAB), 0, accuracy: 0)
        XCTAssertEqual(try XCTUnwrap(c.potenciaAbsorbida), 0, accuracy: 0)
        XCTAssertTrue(c.nota?.contains("abierto") == true)
    }

    func testComponenteSinModeloBloqueaInformeNumericoSinInventarParcial() {
        var e = EsquemaEditable()
        e.nodos.append(NodoEsquema(numero: 1, posicion: CGPoint(x: 50, y: 0)))
        e.componentes = [
            ComponenteEsquema(
                referencia: "D1", tipo: .diodo,
                nodoA: 1, nodoB: 0, posicion: .zero, valor: 0),
        ]

        let informe = GeneradorInformeLaboratorioCircuitoIPad.generar(e)
        XCTAssertNotNil(informe.errorSimulacion)
        XCTAssertTrue(informe.componentes.isEmpty)
        XCTAssertTrue(informe.kcl.isEmpty)
        XCTAssertFalse(informe.aprobadoNumericamente)
        XCTAssertTrue(informe.advertencias.first?.contains("no se generó un resultado parcial") == true)
    }

    func testTextoIncluyeEvidenciaAcademicaYNetlist() {
        let informe = GeneradorInformeLaboratorioCircuitoIPad.generar(
            circuitoFuenteResistor())
        let texto = informe.texto()
        XCTAssertTrue(texto.contains("Comprobación KCL"))
        XCTAssertTrue(texto.contains("Balance de potencia"))
        XCTAssertTrue(texto.contains("Pabs"))
        XCTAssertTrue(texto.contains("Netlist SPICE"))
        XCTAssertTrue(texto.contains("R1"))
        XCTAssertTrue(texto.contains("V1"))
    }
}
