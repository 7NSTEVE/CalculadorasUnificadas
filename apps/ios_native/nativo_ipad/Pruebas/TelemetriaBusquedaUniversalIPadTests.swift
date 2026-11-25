import XCTest
@testable import CuadernUEstudio

final class TelemetriaBusquedaUniversalIPadTests: XCTestCase {
    func testPercentilInterpoladoEsDeterminista() {
        XCTAssertEqual(
            TelemetriaBusquedaUniversalIPad.percentil([1, 2, 3, 4, 5], 0.95),
            4.8,
            accuracy: 0.001)
        XCTAssertTrue(
            TelemetriaBusquedaUniversalIPad.percentil([], 0.95).isNaN)
    }

    func testBucketsNoFragmentanPorCadaTamanoExacto() {
        XCTAssertEqual(
            TelemetriaBusquedaUniversalIPad.escenario(elementosIndexados: 10),
            "busqueda-universal-0-100")
        XCTAssertEqual(
            TelemetriaBusquedaUniversalIPad.escenario(elementosIndexados: 999),
            "busqueda-universal-101-1000")
        XCTAssertEqual(
            TelemetriaBusquedaUniversalIPad.escenario(elementosIndexados: 9_999),
            "busqueda-universal-1001-10000")
        XCTAssertEqual(
            TelemetriaBusquedaUniversalIPad.escenario(elementosIndexados: 25_000),
            "busqueda-universal-10000+")
    }

    func testVentanaRegistraUnP95RealNoCincoMuestrasIndividuales() throws {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("busqueda-p95-\(UUID().uuidString)",
                                    isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioBenchmarksCuadernUIPad(base: base)
        let telemetria = TelemetriaBusquedaUniversalIPad(
            repositorio: repo,
            muestrasPorVentana: 5,
            entorno: {
                EntornoBenchmarkFisicoIPad(
                    dispositivo: "iPad-test",
                    sistema: "iPadOS-test")
            })

        [10.0, 20, 30, 40, 50].forEach {
            telemetria.registrar(duracionMs: $0, elementosIndexados: 850)
        }

        let reporte = repo.reporte(
            dispositivo: "iPad-test",
            sistema: "iPadOS-test")
        XCTAssertEqual(reporte.sinBaseline.count, 1)
        XCTAssertEqual(reporte.sinBaseline.first?.metrica, .busquedaP95Ms)
        XCTAssertEqual(
            reporte.sinBaseline.first?.escenario,
            "busqueda-universal-101-1000")
        XCTAssertEqual(reporte.sinBaseline.first?.valor ?? .nan,
                       48, accuracy: 0.001)
    }
}
