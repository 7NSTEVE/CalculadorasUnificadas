import XCTest

final class EscenariosComparativaGoodnotesIPadTests: XCTestCase {
    func testCatalogoTieneIdsVersionadosUnicosYProtocolosCompletos() {
        XCTAssertTrue(CatalogoEscenariosGoodnotesIPad.validarCatalogo())
        XCTAssertEqual(
            Set(CatalogoEscenariosGoodnotesIPad.todos.map(\.idVersionado)).count,
            CatalogoEscenariosGoodnotesIPad.todos.count)
        XCTAssertTrue(CatalogoEscenariosGoodnotesIPad.todos.allSatisfy {
            $0.minimoMuestras > 0 && !$0.pasos.isEmpty
        })
    }

    func testPencilExigeHardwareY120Hz() {
        XCTAssertTrue(CatalogoEscenariosGoodnotesIPad.pencilP95.requierePencilFisico)
        XCTAssertTrue(CatalogoEscenariosGoodnotesIPad.pencilP95.requiere120Hz)
        XCTAssertTrue(CatalogoEscenariosGoodnotesIPad.pencilP99.requierePencilFisico)
        XCTAssertEqual(
            CatalogoEscenariosGoodnotesIPad.pencilP95.direccion,
            .menorEsMejor)
    }

    func testThroughputExportacionEsMayorEsMejor() {
        XCTAssertEqual(
            CatalogoEscenariosGoodnotesIPad.export500PDF.direccion,
            .mayorEsMejor)
        XCTAssertEqual(
            CatalogoEscenariosGoodnotesIPad.export500PDF.unidad,
            "MB/s")
    }

    func testResolverIdVersionadoNoAceptaVersionInventada() {
        let escenario = CatalogoEscenariosGoodnotesIPad.huge500Reopen
        XCTAssertEqual(
            CatalogoEscenariosGoodnotesIPad.escenario(
                idVersionado: escenario.idVersionado),
            escenario)
        XCTAssertNil(CatalogoEscenariosGoodnotesIPad.escenario(
            idVersionado: "\(escenario.id)@v999"))
    }

    func testEscenario500PaginasEspecificaCorpusComun() {
        let scroll = CatalogoEscenariosGoodnotesIPad.huge500ScrollJank
        let reopen = CatalogoEscenariosGoodnotesIPad.huge500Reopen
        XCTAssertTrue(scroll.descripcionCorpus.contains("500 páginas"))
        XCTAssertEqual(scroll.descripcionCorpus, reopen.descripcionCorpus)
        XCTAssertGreaterThanOrEqual(reopen.minimoMuestras, 5)
    }
}
