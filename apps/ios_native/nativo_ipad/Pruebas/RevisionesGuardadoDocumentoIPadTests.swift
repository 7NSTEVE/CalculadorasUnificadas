import XCTest
@testable import CuadernUEstudio

final class RevisionesGuardadoDocumentoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("revision-doc-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    func testManifestAcumulaArchivosNoTocados() throws {
        let raiz = try base(), manifests = raiz.appendingPathComponent("manifests")
        let repo = RepositorioRevisionesGuardadoIPad(base: manifests)
        let doc = DocumentoPersistenteID()
        let a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")

        let primera = try repo.preparar(documentoId: doc, cambios: [
            .init(destino: a, mutacion: .escribir(Data("a1".utf8))),
            .init(destino: b, mutacion: .escribir(Data("b1".utf8)))
        ])
        try Data("a1".utf8).write(to: a); try Data("b1".utf8).write(to: b)
        if case .escribir(let datos) = primera.cambioManifest.mutacion {
            try FileManager.default.createDirectory(at: manifests, withIntermediateDirectories: true)
            try datos.write(to: repo.url(doc))
        }

        let segunda = try repo.preparar(documentoId: doc, cambios: [
            .init(destino: a, mutacion: .escribir(Data("a2".utf8)))
        ])
        XCTAssertEqual(segunda.revision.entradas.count, 2)
        XCTAssertEqual(segunda.revision.revisionAnteriorId, primera.revision.revisionId)
        XCTAssertNotNil(segunda.revision.hashManifestAnterior)
        XCTAssertEqual(segunda.revision.entradas.first { $0.destinoPath == b.path }?.sha256,
                       primera.revision.entradas.first { $0.destinoPath == b.path }?.sha256)
    }

    func testVerificacionDetectaHashAlterado() throws {
        let raiz = try base(), repo = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("manifests"))
        let doc = DocumentoPersistenteID(), archivo = raiz.appendingPathComponent("doc")
        let prep = try repo.preparar(documentoId: doc, cambios: [
            .init(destino: archivo, mutacion: .escribir(Data("correcto".utf8)))
        ])
        try Data("correcto".utf8).write(to: archivo)
        try FileManager.default.createDirectory(at: repo.url(doc).deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        if case .escribir(let datos) = prep.cambioManifest.mutacion {
            try datos.write(to: repo.url(doc))
        }
        XCTAssertTrue(try repo.verificar(doc).integra)

        try Data("alterado".utf8).write(to: archivo)
        let reporte = try repo.verificar(doc)
        XCTAssertFalse(reporte.integra)
        XCTAssertTrue(reporte.inconsistencias.contains {
            $0.destinoPath == archivo.path &&
            ($0.tipo == .hashDistinto || $0.tipo == .tamanoDistinto)
        })
    }

    func testVerificaArchivoQueDebiaEstarBorrado() throws {
        let raiz = try base(), repo = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("manifests"))
        let doc = DocumentoPersistenteID(), archivo = raiz.appendingPathComponent("sidecar")
        let prep = try repo.preparar(documentoId: doc, cambios: [
            .init(destino: archivo, mutacion: .eliminar)
        ])
        try FileManager.default.createDirectory(at: repo.url(doc).deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        if case .escribir(let datos) = prep.cambioManifest.mutacion {
            try datos.write(to: repo.url(doc))
        }
        XCTAssertTrue(try repo.verificar(doc).integra)
        try Data("fantasma".utf8).write(to: archivo)
        XCTAssertEqual(try repo.verificar(doc).inconsistencias.first?.tipo,
                       .presentePeroDebiaEstarBorrado)
    }

    func testNoPermiteQueParticipanteEscribaManifestReservado() throws {
        let raiz = try base(), repo = RepositorioRevisionesGuardadoIPad(
            base: raiz.appendingPathComponent("manifests"))
        let doc = DocumentoPersistenteID()
        XCTAssertThrowsError(try repo.preparar(documentoId: doc, cambios: [
            .init(destino: repo.url(doc), mutacion: .escribir(Data()))
        ])) {
            XCTAssertEqual($0 as? ErrorRevisionGuardadoDocumentoIPad,
                           .manifestIncluidoComoDestino)
        }
    }
}
