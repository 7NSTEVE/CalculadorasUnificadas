import XCTest
@testable import CuadernUEstudio

final class TapeRecallIPadTests: XCTestCase {
    func testRoundTripConUUIDDurable() throws {
        let repo = RepositorioTapeRecallIPad(base: temporal())
        let documento = DocumentoPersistenteID(UUID())
        let cinta = TapeRecallIPad(documentoId: documento, pagina: 2,
                                   region: .init(x: 10, y: 20, ancho: 300, alto: 80),
                                   titulo: "Definición")
        XCTAssertTrue(repo.guardar([cinta], documento: documento))
        let leidas = try repo.cargar(documento: documento)
        XCTAssertEqual(leidas.count, 1)
        XCTAssertEqual(leidas.first?.documentoId, documento)
        XCTAssertEqual(leidas.first?.region, cinta.region)
    }

    func testDocumentosNoCruzanSidecars() throws {
        let repo = RepositorioTapeRecallIPad(base: temporal())
        let a = DocumentoPersistenteID(UUID()), b = DocumentoPersistenteID(UUID())
        XCTAssertTrue(repo.guardar([
            TapeRecallIPad(documentoId: a,
                           region: .init(x: 0, y: 0, ancho: 10, alto: 10))
        ], documento: a))
        XCTAssertEqual(try repo.cargar(documento: a).count, 1)
        XCTAssertTrue(try repo.cargar(documento: b).isEmpty)
    }

    func testAlternarPersiste() throws {
        let repo = RepositorioTapeRecallIPad(base: temporal())
        let documento = DocumentoPersistenteID(UUID())
        let cinta = TapeRecallIPad(documentoId: documento,
                                   region: .init(x: 0, y: 0, ancho: 20, alto: 20))
        XCTAssertTrue(repo.upsert(cinta))
        XCTAssertEqual(repo.alternar(id: cinta.id, documento: documento), .revelado)
        XCTAssertEqual(try repo.cargar(documento: documento).first?.estado, .revelado)
    }

    func testExportacionOcultaPorDefecto() {
        let documento = DocumentoPersistenteID(UUID())
        let cinta = TapeRecallIPad(documentoId: documento,
                                   region: .init(x: 0, y: 0, ancho: 20, alto: 20),
                                   estado: .revelado)
        XCTAssertEqual(cinta.paraExportacion().estado, .cubierto)
        XCTAssertEqual(cinta.paraExportacion(incluirRevelados: true).estado, .revelado)
    }

    func testUpsertNoDuplica() throws {
        let repo = RepositorioTapeRecallIPad(base: temporal())
        let documento = DocumentoPersistenteID(UUID())
        var cinta = TapeRecallIPad(documentoId: documento,
                                   region: .init(x: 0, y: 0, ancho: 20, alto: 20), titulo: "A")
        XCTAssertTrue(repo.upsert(cinta)); cinta.titulo = "B"; XCTAssertTrue(repo.upsert(cinta))
        let leidas = try repo.cargar(documento: documento)
        XCTAssertEqual(leidas.count, 1); XCTAssertEqual(leidas.first?.titulo, "B")
    }

    func testRechazaOtroDocumento() {
        let repo = RepositorioTapeRecallIPad(base: temporal())
        let a = DocumentoPersistenteID(UUID()), b = DocumentoPersistenteID(UUID())
        let cinta = TapeRecallIPad(documentoId: b,
                                   region: .init(x: 0, y: 0, ancho: 20, alto: 20))
        XCTAssertFalse(repo.guardar([cinta], documento: a))
    }

    private func temporal() -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Tape-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }
}
