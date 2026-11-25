import XCTest
@testable import CuadernUEstudio

final class CorrectorOrtograficoManuscritoIPadTests: XCTestCase {
    func testDiccionarioPersonalPersisteYNormaliza() {
        let base = temporal()
        let repo = RepositorioDiccionarioPersonalIPad(base: base)
        XCTAssertTrue(repo.aprender("Thévenin"))
        XCTAssertTrue(repo.contiene("thevenin"))
        XCTAssertTrue(RepositorioDiccionarioPersonalIPad(base: base).contiene("THÉVENIN"))
    }

    func testOlvidarEliminaPalabra() {
        let repo = RepositorioDiccionarioPersonalIPad(base: temporal())
        XCTAssertTrue(repo.aprender("CuadernUzz"))
        XCTAssertTrue(repo.olvidar("CuadernUzz"))
        XCTAssertFalse(repo.contiene("CuadernUzz"))
    }

    func testReemplazoUsaRangoUTF16() throws {
        let texto = "⚡ kirchof aquí"
        let ns = texto as NSString
        let rango = ns.range(of: "kirchof")
        let corregido = try XCTUnwrap(CorrectorOrtograficoManuscritoIPad.reemplazar(
            en: texto, rangoUTF16: rango, con: "Kirchhoff"))
        XCTAssertEqual(corregido, "⚡ Kirchhoff aquí")
    }

    func testReemplazoRechazaRangoFueraDeTexto() {
        XCTAssertNil(CorrectorOrtograficoManuscritoIPad.reemplazar(
            en: "abc", rangoUTF16: NSRange(location: 50, length: 2), con: "x"))
    }

    func testPalabraPersonalNoSeReportaAunqueCheckerLaDesconozca() {
        let repo = RepositorioDiccionarioPersonalIPad(base: temporal())
        _ = repo.aprender("CuadernUzzword")
        let errores = CorrectorOrtograficoManuscritoIPad.incidencias(
            en: "CuadernUzzword", idioma: "es_ES", diccionario: repo)
        XCTAssertFalse(errores.contains { $0.palabra.caseInsensitiveCompare("CuadernUzzword") == .orderedSame })
    }

    private func temporal() -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Ortografia-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }
}
