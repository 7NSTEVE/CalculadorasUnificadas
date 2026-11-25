import XCTest
@testable import CuadernUEstudio

final class LimpiezaTransaccionesHuerfanasIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("tx-huerfanas-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func carpeta(_ nombre: String, en base: URL,
                         fecha: Date, journal: Bool = false) throws -> URL {
        let url = base.appendingPathComponent(nombre, isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        try Data("staging".utf8).write(to: url.appendingPathComponent("staged-0.bin"))
        if journal { try Data("{}".utf8).write(to: url.appendingPathComponent("journal.json")) }
        try FileManager.default.setAttributes([.modificationDate: fecha], ofItemAtPath: url.path)
        return url
    }

    func testEliminaSoloHuerfanaAntigua() throws {
        let base = try base(), ahora = Date(timeIntervalSince1970: 10_000)
        let vieja = try carpeta("vieja", en: base,
                                fecha: ahora.addingTimeInterval(-3600))
        let reciente = try carpeta("reciente", en: base,
                                   fecha: ahora.addingTimeInterval(-30))
        let reporte = LimpiadorTransaccionesHuerfanasIPad.ejecutar(
            base: base, antiguedadMinima: 600, ahora: ahora)
        XCTAssertEqual(reporte.huerfanasEliminadas, ["vieja"])
        XCTAssertEqual(reporte.recientesConservadas, ["reciente"])
        XCTAssertFalse(FileManager.default.fileExists(atPath: vieja.path))
        XCTAssertTrue(FileManager.default.fileExists(atPath: reciente.path))
    }

    func testNuncaEliminaCarpetaConJournalAunqueSeaAntigua() throws {
        let base = try base(), ahora = Date(timeIntervalSince1970: 10_000)
        let activa = try carpeta("con-journal", en: base,
                                 fecha: ahora.addingTimeInterval(-86_400),
                                 journal: true)
        let reporte = LimpiadorTransaccionesHuerfanasIPad.ejecutar(
            base: base, antiguedadMinima: 0, ahora: ahora)
        XCTAssertEqual(reporte.conJournalConservadas, ["con-journal"])
        XCTAssertTrue(FileManager.default.fileExists(atPath: activa.path))
    }

    func testIgnoraArchivosSueltos() throws {
        let base = try base()
        try Data([1,2,3]).write(to: base.appendingPathComponent("no-directorio"))
        let reporte = LimpiadorTransaccionesHuerfanasIPad.ejecutar(
            base: base, antiguedadMinima: 0)
        XCTAssertEqual(reporte.carpetasInspeccionadas, 0)
    }
}
