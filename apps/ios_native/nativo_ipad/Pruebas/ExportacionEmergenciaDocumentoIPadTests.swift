import XCTest
@testable import CuadernUEstudio

final class ExportacionEmergenciaDocumentoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("emergencia-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    private func instalarRevision(
        doc: DocumentoPersistenteID,
        repo: RepositorioRevisionesGuardadoIPad,
        cambios: [CambioArchivoTransaccionalIPad]
    ) throws -> RevisionGuardadoDocumentoIPad {
        let prep = try repo.preparar(documentoId: doc, cambios: cambios)
        try FileManager.default.createDirectory(
            at: repo.url(doc).deletingLastPathComponent(),
            withIntermediateDirectories: true)
        if case .escribir(let datos) = prep.cambioManifest.mutacion {
            try datos.write(to: repo.url(doc))
        }
        return prep.revision
    }

    func testExportaArchivosAunqueUnoEsteAlteradoYOtroFalte() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")
        try Data("a-ok".utf8).write(to: a); try Data("b-ok".utf8).write(to: b)
        let revisiones = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("revisiones"))
        _ = try instalarRevision(doc: doc, repo: revisiones, cambios: [
            .init(destino: a, mutacion: .escribir(Data("a-ok".utf8))),
            .init(destino: b, mutacion: .escribir(Data("b-ok".utf8)))
        ])

        try Data("a-alterado".utf8).write(to: a)
        try FileManager.default.removeItem(at: b)
        let destino = raiz.appendingPathComponent("paquete", isDirectory: true)
        let manifiesto = try ExportadorEmergenciaDocumentoIPad.exportar(
            documentoId: doc,
            destino: destino,
            revisiones: revisiones,
            conflictos: .init(baseTransacciones: raiz.appendingPathComponent("journals"),
                              coordinador: .init(base: raiz.appendingPathComponent("journals"))))

        XCTAssertEqual(manifiesto.alterados, 1)
        XCTAssertEqual(manifiesto.faltantes, 1)
        XCTAssertEqual(manifiesto.entradas.first { $0.rutaOriginal == a.path }?.estado,
                       .alterado)
        XCTAssertEqual(manifiesto.entradas.first { $0.rutaOriginal == b.path }?.estado,
                       .faltante)
        let copia = manifiesto.entradas.first { $0.rutaOriginal == a.path }?.nombreCopia
        XCTAssertNotNil(copia)
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent(copia!)),
                       Data("a-alterado".utf8))
    }

    func testSinManifestPuedeRescatarRutasAdicionales() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let extra = raiz.appendingPathComponent("legacy.bin")
        try Data("rescatable".utf8).write(to: extra)
        let destino = raiz.appendingPathComponent("paquete")
        let manifiesto = try ExportadorEmergenciaDocumentoIPad.exportar(
            documentoId: doc,
            destino: destino,
            revisiones: .init(base: raiz.appendingPathComponent("revisiones")),
            conflictos: .init(baseTransacciones: raiz.appendingPathComponent("journals"),
                              coordinador: .init(base: raiz.appendingPathComponent("journals"))),
            rutasAdicionales: [extra])
        XCTAssertFalse(manifiesto.revisionDisponible)
        XCTAssertEqual(manifiesto.entradas.count, 1)
        XCTAssertEqual(manifiesto.entradas[0].estado, .presenteNoInventariado)
    }

    func testArchivoQueDebiaEstarBorradoSeRescataComoAlterado() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let fantasma = raiz.appendingPathComponent("fantasma")
        let revisiones = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("revisiones"))
        _ = try instalarRevision(doc: doc, repo: revisiones, cambios: [
            .init(destino: fantasma, mutacion: .eliminar)
        ])
        try Data("aparecio".utf8).write(to: fantasma)
        let manifiesto = try ExportadorEmergenciaDocumentoIPad.exportar(
            documentoId: doc,
            destino: raiz.appendingPathComponent("salida"),
            revisiones: revisiones,
            conflictos: .init(baseTransacciones: raiz.appendingPathComponent("journals"),
                              coordinador: .init(base: raiz.appendingPathComponent("journals"))))
        XCTAssertEqual(manifiesto.entradas.first?.estado, .alterado)
        XCTAssertNotNil(manifiesto.entradas.first?.nombreCopia)
    }

    func testNoSobrescribePaqueteExistente() throws {
        let raiz = try base(), destino = raiz.appendingPathComponent("ya-existe")
        try FileManager.default.createDirectory(at: destino, withIntermediateDirectories: true)
        XCTAssertThrowsError(try ExportadorEmergenciaDocumentoIPad.exportar(
            documentoId: DocumentoPersistenteID(),
            destino: destino,
            revisiones: .init(base: raiz.appendingPathComponent("revisiones")),
            conflictos: .init(baseTransacciones: raiz.appendingPathComponent("journals"),
                              coordinador: .init(base: raiz.appendingPathComponent("journals"))))) {
                XCTAssertEqual($0 as? ErrorExportacionEmergenciaIPad, .destinoYaExiste)
            }
    }
}
