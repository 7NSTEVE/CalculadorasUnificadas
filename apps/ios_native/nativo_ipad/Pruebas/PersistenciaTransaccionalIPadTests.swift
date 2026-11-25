import XCTest
@testable import CuadernUEstudio

final class PersistenciaTransaccionalIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("tx-persistencia-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func escribir(_ texto: String, en url: URL) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        try Data(texto.utf8).write(to: url)
    }

    private func leer(_ url: URL) -> String? {
        (try? Data(contentsOf: url)).flatMap { String(data: $0, encoding: .utf8) }
    }

    func testCommitCambiaDocumentoYSidecarComoUnidad() throws {
        let raiz = try base()
        let journals = raiz.appendingPathComponent("journals", isDirectory: true)
        let documento = raiz.appendingPathComponent("doc.bin")
        let sidecar = raiz.appendingPathComponent("audio.json")
        try escribir("doc-v1", en: documento)
        try escribir("audio-v1", en: sidecar)

        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.ejecutar(
            documentoId: DocumentoPersistenteID(),
            cambios: [
                .init(destino: documento, mutacion: .escribir(Data("doc-v2".utf8))),
                .init(destino: sidecar, mutacion: .escribir(Data("audio-v2".utf8)))
            ])

        XCTAssertEqual(preparada.cantidadCambios, 2)
        XCTAssertEqual(leer(documento), "doc-v2")
        XCTAssertEqual(leer(sidecar), "audio-v2")
        XCTAssertThrowsError(try coordinador.journal(preparada.id))
    }

    func testPrecondicionSHAIncorrectaNoTocaNada() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let archivo = raiz.appendingPathComponent("doc")
        try escribir("original", en: archivo)
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)

        XCTAssertThrowsError(try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("nuevo".utf8)),
                            precondicion: .sha256(String(repeating: "0", count: 64)))])
        ) { error in
            guard case ErrorPersistenciaTransaccionalIPad.precondicionNoCumplida = error else {
                return XCTFail("Error inesperado: \(error)")
            }
        }
        XCTAssertEqual(leer(archivo), "original")
    }

    func testCambioConcurrenteRestauraLoSeguroSinPisarEdicionExterna() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")
        try escribir("a1", en: a); try escribir("b1", en: b)
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [
                .init(destino: a, mutacion: .escribir(Data("a2".utf8))),
                .init(destino: b, mutacion: .escribir(Data("b2".utf8)))
            ])

        try escribir("edicion-externa", en: b)
        XCTAssertThrowsError(try coordinador.confirmar(preparada.id))
        XCTAssertThrowsError(try coordinador.rollback(preparada.id)) { error in
            guard case ErrorPersistenciaTransaccionalIPad.recoveryEnConflicto = error else {
                return XCTFail("Error inesperado: \(error)")
            }
        }

        XCTAssertEqual(leer(a), "a1")
        XCTAssertEqual(leer(b), "edicion-externa")
    }

    func testRecoveryRestauraCrashDespuesDeWriteParcial() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")
        try escribir("a1", en: a); try escribir("b1", en: b)
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [
                .init(destino: a, mutacion: .escribir(Data("a2".utf8))),
                .init(destino: b, mutacion: .escribir(Data("b2".utf8)))
            ])

        // Simula muerte del proceso después de que el primer destino recibió
        // los bytes nuevos pero antes de actualizar/confirmar el journal.
        try escribir("a2", en: a)

        let trasRelaunch = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let reporte = trasRelaunch.recuperarPendientes()
        XCTAssertEqual(reporte.restauradas, [preparada.id])
        XCTAssertTrue(reporte.conflictos.isEmpty)
        XCTAssertEqual(leer(a), "a1")
        XCTAssertEqual(leer(b), "b1")
    }

    func testRecoveryEliminaArchivoNuevoQueQuedoParcial() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let nuevo = raiz.appendingPathComponent("nuevo-sidecar")
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: nuevo,
                            mutacion: .escribir(Data("parcial".utf8)),
                            precondicion: .debeNoExistir)])
        try escribir("parcial", en: nuevo)

        let reporte = CoordinadorPersistenciaTransaccionalIPad(base: journals)
            .recuperarPendientes()
        XCTAssertEqual(reporte.restauradas, [preparada.id])
        XCTAssertFalse(FileManager.default.fileExists(atPath: nuevo.path))
    }

    func testRecoveryRestauraArchivoEliminadoParcialmente() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let sidecar = raiz.appendingPathComponent("sidecar")
        try escribir("contenido", en: sidecar)
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: sidecar, mutacion: .eliminar)])
        try FileManager.default.removeItem(at: sidecar)

        let reporte = CoordinadorPersistenciaTransaccionalIPad(base: journals)
            .recuperarPendientes()
        XCTAssertEqual(reporte.restauradas, [preparada.id])
        XCTAssertEqual(leer(sidecar), "contenido")
    }

    func testRecoveryNoPisaCambioExternoDesconocido() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let archivo = raiz.appendingPathComponent("doc")
        try escribir("v1", en: archivo)
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let preparada = try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("v2".utf8)))])
        try escribir("edicion-ajena", en: archivo)

        let reporte = CoordinadorPersistenciaTransaccionalIPad(base: journals)
            .recuperarPendientes()
        XCTAssertEqual(reporte.conflictos, [preparada.id])
        XCTAssertEqual(leer(archivo), "edicion-ajena")
        XCTAssertEqual(try coordinador.journal(preparada.id).estado,
                       .conflictoRecovery)
    }

    func testRechazaDestinoDuplicado() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("x")
        let coordinador = CoordinadorPersistenciaTransaccionalIPad(
            base: raiz.appendingPathComponent("journals"))
        XCTAssertThrowsError(try coordinador.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [
                .init(destino: archivo, mutacion: .escribir(Data([1]))),
                .init(destino: archivo, mutacion: .escribir(Data([2])))
            ])) { error in
                guard case ErrorPersistenciaTransaccionalIPad.destinoDuplicado = error else {
                    return XCTFail("Error inesperado: \(error)")
                }
            }
    }
}
