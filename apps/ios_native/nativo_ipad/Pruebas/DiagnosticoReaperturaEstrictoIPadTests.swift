import XCTest
@testable import CuadernUEstudio

final class DiagnosticoReaperturaEstrictoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("reopen-strict-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    func testDistingueSinManifest() throws {
        let repo = RepositorioRevisionesGuardadoIPad(base: try base())
        XCTAssertEqual(GateReaperturaEstrictoIPad(revisiones: repo)
            .verificar(DocumentoPersistenteID()), .sinManifest)
    }

    func testDistingueManifestCorrupto() throws {
        let base = try base(), repo = RepositorioRevisionesGuardadoIPad(base: base)
        let doc = DocumentoPersistenteID()
        try Data("no-json".utf8).write(to: repo.url(doc))
        XCTAssertEqual(
            GateReaperturaEstrictoIPad(revisiones: repo).verificar(doc),
            .manifestCorrupto(ruta: repo.url(doc).path))
    }

    func testContenidoAlteradoNoSeConfundeConManifestCorrupto() throws {
        let base = try base(), repo = RepositorioRevisionesGuardadoIPad(
            base: base.appendingPathComponent("revisiones"))
        let doc = DocumentoPersistenteID(), archivo = base.appendingPathComponent("doc")
        let prep = try repo.preparar(documentoId: doc, cambios: [
            .init(destino: archivo, mutacion: .escribir(Data("ok".utf8)))
        ])
        try Data("ok".utf8).write(to: archivo)
        if case .escribir(let manifest) = prep.cambioManifest.mutacion {
            try manifest.write(to: repo.url(doc))
        }
        try Data("cambio".utf8).write(to: archivo)
        guard case .contenidoInconsistente(let reporte) =
                GateReaperturaEstrictoIPad(revisiones: repo).verificar(doc) else {
            return XCTFail("Debió ser inconsistencia de contenido")
        }
        XCTAssertEqual(reporte.documentoId, doc)
        XCTAssertFalse(reporte.integra)
    }
}
