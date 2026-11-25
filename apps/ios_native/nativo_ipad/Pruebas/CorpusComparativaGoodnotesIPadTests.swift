import PDFKit
import XCTest

final class CorpusComparativaGoodnotesIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-CorpusGoodnotes-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testCorpusCortoGeneraPDFManifiestoHashYConsultas() throws {
        let resultado = try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta, paginas: 12, corpusId: "qa-12")

        XCTAssertTrue(FileManager.default.fileExists(atPath: resultado.pdf.path))
        XCTAssertTrue(FileManager.default.fileExists(atPath: resultado.manifiesto.path))
        XCTAssertEqual(resultado.metadata.paginas, 12)
        XCTAssertEqual(resultado.metadata.sha256PDF.count, 64)
        XCTAssertGreaterThan(resultado.metadata.bytesPDF, 0)
        XCTAssertEqual(PDFDocument(url: resultado.pdf)?.pageCount, 12)

        let validado = try GeneradorCorpusGoodnotesIPad.validar(
            pdf: resultado.pdf, manifiesto: resultado.manifiesto)
        XCTAssertEqual(validado, resultado.metadata)
        XCTAssertEqual(validado.consultas.count, 10)
        XCTAssertTrue(validado.consultas.allSatisfy { !$0.paginasEsperadas.isEmpty })
    }

    func testConsultasEsperadasUsanPaginasUnoBased() throws {
        let resultado = try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta, paginas: 20, corpusId: "qa-search")
        let primera = try XCTUnwrap(resultado.metadata.consultas.first)
        XCTAssertEqual(primera.termino, "KIRCHHOFF")
        XCTAssertEqual(primera.paginasEsperadas, [1, 11])
    }

    func testAlterarPDFRompeValidacionSinAceptarManifiestoViejo() throws {
        let resultado = try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta, paginas: 3, corpusId: "qa-tamper")
        let manejador = try FileHandle(forWritingTo: resultado.pdf)
        try manejador.seekToEnd()
        try manejador.write(contentsOf: Data("tamper".utf8))
        try manejador.close()

        XCTAssertThrowsError(try GeneradorCorpusGoodnotesIPad.validar(
            pdf: resultado.pdf, manifiesto: resultado.manifiesto))
    }

    func testCancelacionNoPublicaPDFNiManifiestoFinal() throws {
        var llamadas = 0
        XCTAssertThrowsError(try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta,
            paginas: 50,
            corpusId: "qa-cancel",
            cancelado: {
                llamadas += 1
                return llamadas > 3
            })) { error in
                XCTAssertEqual(error as? ErrorCorpusGoodnotesIPad, .cancelado)
            }
        XCTAssertFalse(FileManager.default.fileExists(
            atPath: carpeta.appendingPathComponent("qa-cancel.pdf").path))
        XCTAssertFalse(FileManager.default.fileExists(
            atPath: carpeta.appendingPathComponent("qa-cancel.json").path))
    }

    func testRechazaCantidadDePaginasFueraDePresupuesto() {
        XCTAssertThrowsError(try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta, paginas: 0, corpusId: "invalid"))
        XCTAssertThrowsError(try GeneradorCorpusGoodnotesIPad.generar(
            en: carpeta, paginas: 2_001, corpusId: "invalid"))
    }
}
