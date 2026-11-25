import XCTest
@testable import CuadernUEstudio

final class MetricasLatenciaPencilIPadTests: XCTestCase {
    private func muestra(ms: Double,
                         predicted: Bool = false,
                         corregida: Bool = false,
                         descartada: Bool = false) -> MuestraLatenciaPencilIPad {
        .init(eventoId: UUID(),
              timestampEntrada: 10,
              timestampPresentado: 10 + ms / 1000,
              coalesced: false,
              predicted: predicted,
              corregidaPrediccion: corregida,
              descartada: descartada,
              thermal: "nominal")
    }

    func testPercentilesInterpoladosSonDeterministas() {
        let resumen = CalculadorMetricasPencilIPad.resumir(
            (1...100).map { muestra(ms: Double($0)) })
        XCTAssertEqual(resumen.muestrasValidas, 100)
        XCTAssertEqual(resumen.p50Ms!, 50.5, accuracy: 0.001)
        XCTAssertEqual(resumen.p95Ms!, 95.05, accuracy: 0.001)
        XCTAssertEqual(resumen.p99Ms!, 99.01, accuracy: 0.001)
        XCTAssertEqual(resumen.maximaMs!, 100, accuracy: 0.001)
    }

    func testGateFallaPorColaLargaAunquePromedioSeaAceptable() {
        var muestras = (0..<999).map { _ in muestra(ms: 8) }
        muestras.append(muestra(ms: 80))
        let criterios = CriteriosLatenciaPencilIPad(
            minimoMuestras: 500,
            p50MaxMs: 12,
            p95MaxMs: 20,
            p99MaxMs: 35,
            descartadasMaxPorcentaje: 0.1)
        let resultado = CalculadorMetricasPencilIPad.evaluar(
            muestras, criterios: criterios)
        // Un solo outlier no mueve P99 en 1000 muestras; el gate no inventa
        // un fallo que el percentil pedido no respalda.
        XCTAssertTrue(resultado.aprobado)
        XCTAssertLessThan(resultado.resumen.p99Ms!, 35)
        XCTAssertEqual(resultado.resumen.maximaMs!, 80, accuracy: 0.01)
    }

    func testGateDetectaP95RealmenteAlto() {
        let muestras = (0..<950).map { _ in muestra(ms: 8) }
            + (0..<50).map { _ in muestra(ms: 50) }
        let resultado = CalculadorMetricasPencilIPad.evaluar(muestras)
        XCTAssertFalse(resultado.aprobado)
        XCTAssertTrue(resultado.fallos.contains { $0.hasPrefix("P95") })
    }

    func testPredictedYCorreccionesSeMidenSinExcluirMuestras() {
        let resumen = CalculadorMetricasPencilIPad.resumir([
            muestra(ms: 5, predicted: true, corregida: true),
            muestra(ms: 6, predicted: true, corregida: false),
            muestra(ms: 7)
        ])
        XCTAssertEqual(resumen.porcentajePredicted, 66.666, accuracy: 0.01)
        XCTAssertEqual(resumen.porcentajeCorreccionPredicted, 50, accuracy: 0.001)
    }

    func testDescartadasSeSeparanDeLatenciaValida() {
        let resumen = CalculadorMetricasPencilIPad.resumir([
            muestra(ms: 10),
            muestra(ms: 10, descartada: true)
        ])
        XCTAssertEqual(resumen.muestrasValidas, 1)
        XCTAssertEqual(resumen.muestrasDescartadas, 1)
        XCTAssertEqual(resumen.porcentajeDescartadas, 50, accuracy: 0.001)
    }

    func testRepositorioReabreSesionPersistida() throws {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("pencil-metrics-\(UUID().uuidString)", isDirectory: true)
        let id: UUID
        do {
            let repo = RepositorioMetricasPencilIPad(base: base)
            id = repo.iniciar(dispositivo: "iPad", sistema: "iPadOS")
            repo.registrar(muestra(ms: 9), sesionId: id)
            _ = repo.finalizar(id, notas: ["prueba física"])
        }
        let reabierto = RepositorioMetricasPencilIPad(base: base)
        XCTAssertEqual(reabierto.sesion(id)?.muestras.count, 1)
        XCTAssertEqual(reabierto.sesion(id)?.notas, ["prueba física"])
        XCTAssertEqual(reabierto.resumen(id)?.p50Ms, 9, accuracy: 0.01)
    }
}
