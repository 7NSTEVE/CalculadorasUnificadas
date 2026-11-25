import XCTest
@testable import CuadernUEstudio

final class VirtualizacionElementosIPadTests: XCTestCase {
    private func elemento(_ id: String, x: CGFloat, y: CGFloat,
                          w: CGFloat = 200, h: CGFloat = 120,
                          anclado: Bool = false) -> DescriptorVirtualizableIPad {
        .init(id: id, marco: CGRect(x: x, y: y, width: w, height: h),
              anclado: anclado)
    }

    func testDocumentoPequenoMaterializaTodo() {
        var politica = PoliticaVirtualizacionElementosIPad()
        politica.umbralElementos = 5
        let elementos = (0..<5).map {
            elemento("e\($0)", x: 0, y: CGFloat($0) * 10_000)
        }
        let plan = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 0, width: 1_000, height: 1_000),
            regionAnterior: nil,
            politica: politica)

        XCTAssertFalse(plan.virtualizacionActiva)
        XCTAssertEqual(plan.idsMaterializados, Set(elementos.map(\.id)))
        XCTAssertNil(plan.regionMaterializacion)
    }

    func testDocumentoGrandeSoloMaterializaViewportMasPrefetch() {
        var politica = PoliticaVirtualizacionElementosIPad()
        politica.umbralElementos = 3
        politica.prefetchHorizontal = 0
        politica.prefetchVertical = 0
        politica.margenMinimoHorizontal = 100
        politica.margenMinimoVertical = 200
        let elementos = [
            elemento("visible", x: 50, y: 50),
            elemento("prefetch", x: 50, y: 1_050),
            elemento("lejos", x: 50, y: 10_000),
            elemento("lateral", x: 1_500, y: 50),
        ]

        let plan = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 0, width: 1_000, height: 900),
            regionAnterior: nil,
            politica: politica)

        XCTAssertTrue(plan.virtualizacionActiva)
        XCTAssertTrue(plan.idsMaterializados.contains("visible"))
        XCTAssertTrue(plan.idsMaterializados.contains("prefetch"))
        XCTAssertFalse(plan.idsMaterializados.contains("lejos"))
        XCTAssertFalse(plan.idsMaterializados.contains("lateral"))
    }

    func testElementoAncladoPermaneceAunqueEsteMuyLejos() {
        var politica = PoliticaVirtualizacionElementosIPad()
        politica.umbralElementos = 1
        let elementos = [
            elemento("visible", x: 0, y: 0),
            elemento("editando", x: 0, y: 1_000_000, anclado: true),
        ]
        let plan = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 0, width: 800, height: 1_000),
            regionAnterior: nil,
            politica: politica)

        XCTAssertTrue(plan.idsMaterializados.contains("editando"))
    }

    func testHisteresisReutilizaRegionMientrasViewportSigueEnZonaSegura() {
        var politica = PoliticaVirtualizacionElementosIPad()
        politica.umbralElementos = 1
        politica.margenMinimoHorizontal = 1_000
        politica.margenMinimoVertical = 2_000
        let elementos = [
            elemento("a", x: 100, y: 100),
            elemento("b", x: 100, y: 5_000),
        ]
        let inicial = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 0, width: 1_000, height: 1_000),
            regionAnterior: nil,
            politica: politica)
        let movidoPoco = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 120, width: 1_000, height: 1_000),
            regionAnterior: inicial.regionMaterializacion,
            politica: politica)

        XCTAssertTrue(movidoPoco.reutilizaRegionAnterior)
        XCTAssertEqual(movidoPoco.regionMaterializacion,
                       inicial.regionMaterializacion)
    }

    func testSalirseDeZonaSeguraCalculaNuevaRegion() {
        var politica = PoliticaVirtualizacionElementosIPad()
        politica.umbralElementos = 1
        let elementos = [
            elemento("a", x: 0, y: 0),
            elemento("b", x: 0, y: 20_000),
        ]
        let inicial = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 0, width: 1_000, height: 1_000),
            regionAnterior: nil,
            politica: politica)
        let lejano = PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: CGRect(x: 0, y: 19_500, width: 1_000, height: 1_000),
            regionAnterior: inicial.regionMaterializacion,
            politica: politica)

        XCTAssertFalse(lejano.reutilizaRegionAnterior)
        XCTAssertNotEqual(lejano.regionMaterializacion,
                          inicial.regionMaterializacion)
        XCTAssertTrue(lejano.idsMaterializados.contains("b"))
    }
}
