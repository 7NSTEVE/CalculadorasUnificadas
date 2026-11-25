import XCTest
@testable import CuadernUEstudio

final class FocoPaginaActualIPadTests: XCTestCase {
    func testUsaElFrameQueContieneElCentroVisible() {
        let visible = CGRect(x: 900, y: 1200, width: 500, height: 700)
        let pagina = CGRect(x: 620, y: 900, width: 1240, height: 1754)

        XCTAssertEqual(
            GeometriaFocoPaginaActualIPad.region(
                visible: visible, marcosPagina: [pagina]),
            pagina)
    }

    func testSiHayFramesAnidadosPrefiereElMasEspecifico() {
        let visible = CGRect(x: 1000, y: 1000, width: 400, height: 500)
        let grande = CGRect(x: 0, y: 0, width: 3000, height: 4000)
        let pagina = CGRect(x: 700, y: 500, width: 1240, height: 1754)

        XCTAssertEqual(
            GeometriaFocoPaginaActualIPad.region(
                visible: visible, marcosPagina: [grande, pagina]),
            pagina)
    }

    func testFallbackVerticalUsaA4LogicoACientoCincuentaPPP() {
        let visible = CGRect(x: 100, y: 200, width: 700, height: 1000)
        let region = GeometriaFocoPaginaActualIPad.region(visible: visible)

        XCTAssertEqual(region.size, GeometriaFocoPaginaActualIPad.a4Vertical)
        XCTAssertEqual(region.midX, visible.midX, accuracy: 0.001)
        XCTAssertEqual(region.midY, visible.midY, accuracy: 0.001)
    }

    func testFallbackHorizontalRespetaOrientacionDeTrabajo() {
        let visible = CGRect(x: 300, y: 500, width: 1200, height: 700)
        let region = GeometriaFocoPaginaActualIPad.region(visible: visible)

        XCTAssertEqual(region.size, GeometriaFocoPaginaActualIPad.a4Horizontal)
        XCTAssertEqual(region.midX, visible.midX, accuracy: 0.001)
        XCTAssertEqual(region.midY, visible.midY, accuracy: 0.001)
    }

    func testFrameQueNoContieneElCentroNoSecuestraLaPaginaActual() {
        let visible = CGRect(x: 2000, y: 2000, width: 600, height: 900)
        let lejano = CGRect(x: 0, y: 0, width: 1240, height: 1754)
        let region = GeometriaFocoPaginaActualIPad.region(
            visible: visible, marcosPagina: [lejano])

        XCTAssertEqual(region.size, GeometriaFocoPaginaActualIPad.a4Vertical)
        XCTAssertEqual(region.midX, visible.midX, accuracy: 0.001)
        XCTAssertEqual(region.midY, visible.midY, accuracy: 0.001)
    }

    func testRectangulosInvalidosSeIgnoran() {
        let visible = CGRect(x: 20, y: 40, width: 700, height: 1000)
        let invalido = CGRect(x: .infinity, y: 0, width: 100, height: 100)

        XCTAssertEqual(
            GeometriaFocoPaginaActualIPad.region(
                visible: visible, marcosPagina: [invalido]).size,
            GeometriaFocoPaginaActualIPad.a4Vertical)
    }

    func testContextoVisualDejaMargenAlrededorDeLaPagina() {
        let pagina = CGRect(x: 100, y: 200, width: 1000, height: 1500)
        let contexto = GeometriaFocoPaginaActualIPad.contextoVisual(
            para: pagina, margenRelativo: 0.06)

        XCTAssertEqual(contexto.midX, pagina.midX, accuracy: 0.001)
        XCTAssertEqual(contexto.midY, pagina.midY, accuracy: 0.001)
        XCTAssertEqual(contexto.width, 1120, accuracy: 0.001)
        XCTAssertEqual(contexto.height, 1680, accuracy: 0.001)
        XCTAssertTrue(contexto.contains(pagina))
    }

    func testCanalPublicaDocumentoRegionYGuia() {
        let centro = NotificationCenter()
        let recibida = expectation(description: "foco recibido")
        let pagina = CGRect(x: 10, y: 20, width: 1240, height: 1754)
        var evento: EventoFocoPaginaActualIPad?
        let observador = centro.addObserver(
            forName: CanalFocoPaginaActualIPad.nombre,
            object: nil,
            queue: nil) { nota in
                evento = CanalFocoPaginaActualIPad.evento(de: nota)
                recibida.fulfill()
            }
        defer { centro.removeObserver(observador) }

        CanalFocoPaginaActualIPad.activar(
            documento: 42,
            region: pagina,
            guia: "  Calcule la corriente de Norton  ",
            centro: centro)

        wait(for: [recibida], timeout: 0.1)
        XCTAssertEqual(evento, EventoFocoPaginaActualIPad(
            documento: 42,
            region: pagina,
            guia: "Calcule la corriente de Norton"))
    }

    func testCanalSinGuiaMantieneCompatibilidad() {
        let centro = NotificationCenter()
        let recibida = expectation(description: "foco recibido")
        let pagina = CGRect(x: 10, y: 20, width: 1240, height: 1754)
        var evento: EventoFocoPaginaActualIPad?
        let observador = centro.addObserver(
            forName: CanalFocoPaginaActualIPad.nombre,
            object: nil,
            queue: nil) { nota in
                evento = CanalFocoPaginaActualIPad.evento(de: nota)
                recibida.fulfill()
            }
        defer { centro.removeObserver(observador) }

        CanalFocoPaginaActualIPad.activar(
            documento: 42, region: pagina, centro: centro)

        wait(for: [recibida], timeout: 0.1)
        XCTAssertEqual(evento, EventoFocoPaginaActualIPad(
            documento: 42, region: pagina))
    }

    func testDesactivarPublicaRegionYGuiaNulas() {
        let centro = NotificationCenter()
        let recibida = expectation(description: "foco retirado")
        var evento: EventoFocoPaginaActualIPad?
        let observador = centro.addObserver(
            forName: CanalFocoPaginaActualIPad.nombre,
            object: nil,
            queue: nil) { nota in
                evento = CanalFocoPaginaActualIPad.evento(de: nota)
                recibida.fulfill()
            }
        defer { centro.removeObserver(observador) }

        CanalFocoPaginaActualIPad.desactivar(documento: 91, centro: centro)

        wait(for: [recibida], timeout: 0.1)
        XCTAssertEqual(evento?.documento, 91)
        XCTAssertNil(evento?.region)
        XCTAssertNil(evento?.guia)
    }
}
