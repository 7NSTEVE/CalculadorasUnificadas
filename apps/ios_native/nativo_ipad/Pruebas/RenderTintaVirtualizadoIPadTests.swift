import XCTest

final class RenderTintaVirtualizadoIPadTests: XCTestCase {
    func testPaginaNormalNoNecesitaVirtualizacion() {
        XCTAssertFalse(RenderizadorMetal.requiereVirtualizacion(
            tamanoDocumento: CGSize(width: 1_240, height: 1_754),
            escala: 2))
    }

    func testCuadernoDe500PaginasSiempreVirtualiza() {
        XCTAssertTrue(RenderizadorMetal.requiereVirtualizacion(
            tamanoDocumento: CGSize(width: 1_240, height: 1_754 * 500),
            escala: 2))
    }

    func testGeometriaInvalidaSeTrataComoInsegura() {
        XCTAssertTrue(RenderizadorMetal.requiereVirtualizacion(
            tamanoDocumento: .zero, escala: 2))
        XCTAssertTrue(RenderizadorMetal.requiereVirtualizacion(
            tamanoDocumento: CGSize(width: .infinity, height: 1_000),
            escala: 2))
    }

    func testPrefetchExpandeYClampaAlDocumento() {
        let documento = CGRect(x: 0, y: 0, width: 1_000, height: 2_000)
        let visible = CGRect(x: 0, y: 100, width: 400, height: 600)
        let region = PlanificadorViewportTintaIPad.regionRender(
            visible: visible, documento: documento)

        XCTAssertEqual(region.minX, 0, accuracy: 0.001)
        XCTAssertLessThan(region.minY, visible.minY)
        XCTAssertGreaterThan(region.maxX, visible.maxX)
        XCTAssertGreaterThan(region.maxY, visible.maxY)
        XCTAssertTrue(documento.contains(region))
    }

    func testHisteresisEvitaRerenderPorScrollPequeno() {
        let documento = CGRect(x: 0, y: 0, width: 1_200, height: 20_000)
        let visibleInicial = CGRect(x: 100, y: 1_000, width: 700, height: 900)
        let tile = PlanificadorViewportTintaIPad.regionRender(
            visible: visibleInicial, documento: documento)
        let pequeno = visibleInicial.offsetBy(dx: 8, dy: 10)

        XCTAssertFalse(PlanificadorViewportTintaIPad.necesitaMoverTile(
            actual: tile, visible: pequeno, documento: documento))
    }

    func testHisteresisMueveTileAntesDeSalirDelPrefetch() {
        let documento = CGRect(x: 0, y: 0, width: 1_200, height: 20_000)
        let visibleInicial = CGRect(x: 100, y: 1_000, width: 700, height: 900)
        let tile = PlanificadorViewportTintaIPad.regionRender(
            visible: visibleInicial, documento: documento)
        let lejano = visibleInicial.offsetBy(dx: 0, dy: 450)

        XCTAssertTrue(PlanificadorViewportTintaIPad.necesitaMoverTile(
            actual: tile, visible: lejano, documento: documento))
    }

    func testEscalaRenderSigueZoomSinSupermuestrearSinLimite() {
        XCTAssertEqual(PlanificadorViewportTintaIPad.escalaRender(
            escalaPantalla: 2, zoom: 1), 2, accuracy: 0.001)
        XCTAssertEqual(PlanificadorViewportTintaIPad.escalaRender(
            escalaPantalla: 2, zoom: 0.1), 0.2, accuracy: 0.001)
        XCTAssertEqual(PlanificadorViewportTintaIPad.escalaRender(
            escalaPantalla: 2, zoom: 0.01),
            RenderizadorMetal.escalaMinimaRegional,
            accuracy: 0.001)
        XCTAssertEqual(PlanificadorViewportTintaIPad.escalaRender(
            escalaPantalla: 3, zoom: 4), 4, accuracy: 0.001)
    }
}
