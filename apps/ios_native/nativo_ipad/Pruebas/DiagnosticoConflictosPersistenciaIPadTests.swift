import XCTest
@testable import CuadernUEstudio

final class DiagnosticoConflictosPersistenciaIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("diag-tx-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    private func conflicto() throws -> (
        raiz: URL,
        journals: URL,
        archivo: URL,
        preparada: TransaccionPreparadaPersistenciaIPad,
        coordinador: CoordinadorPersistenciaTransaccionalIPad
    ) {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let archivo = raiz.appendingPathComponent("documento")
        try Data("original".utf8).write(to: archivo)
        let c = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let p = try c.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: archivo,
                            mutacion: .escribir(Data("propuesto".utf8)))])
        try Data("edicion-externa".utf8).write(to: archivo)
        XCTAssertEqual(CoordinadorPersistenciaTransaccionalIPad(base: journals)
            .recuperarPendientes().conflictos, [p.id])
        return (raiz, journals, archivo, p, c)
    }

    func testListaConflictoConDocumentoYRuta() throws {
        let x = try conflicto()
        let gestor = GestorDiagnosticosPersistenciaIPad(
            baseTransacciones: x.journals,
            coordinador: x.coordinador)
        let lista = gestor.listarConflictos()
        XCTAssertEqual(lista.count, 1)
        XCTAssertEqual(lista[0].id, x.preparada.id)
        XCTAssertEqual(lista[0].rutas, [x.archivo.path])
    }

    func testExportIncluyeOriginalPropuestoYActual() throws {
        let x = try conflicto()
        let gestor = GestorDiagnosticosPersistenciaIPad(
            baseTransacciones: x.journals,
            coordinador: x.coordinador)
        let destino = x.raiz.appendingPathComponent("diagnostico", isDirectory: true)
        let manifiesto = try gestor.exportar(
            transaccionId: x.preparada.id, destino: destino)

        XCTAssertEqual(manifiesto.estadoJournal, .conflictoRecovery)
        XCTAssertEqual(manifiesto.entradas.first?.estadoActual, .desconocido)
        XCTAssertTrue(manifiesto.entradas.first?.backupIncluido == true)
        XCTAssertTrue(manifiesto.entradas.first?.stagedIncluido == true)
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("backup-0.bin")),
                       Data("original".utf8))
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("staged-0.bin")),
                       Data("propuesto".utf8))
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("actual-0.bin")),
                       Data("edicion-externa".utf8))
        XCTAssertTrue(FileManager.default.fileExists(
            atPath: destino.appendingPathComponent("journal.json").path))
    }

    func testResolverConservandoActualNoModificaArchivo() throws {
        let x = try conflicto()
        let gestor = GestorDiagnosticosPersistenciaIPad(
            baseTransacciones: x.journals,
            coordinador: x.coordinador)
        let destino = x.raiz.appendingPathComponent("evidencia", isDirectory: true)
        _ = try gestor.conservarActualesYResolver(
            transaccionId: x.preparada.id,
            diagnosticoDestino: destino)

        XCTAssertEqual(try Data(contentsOf: x.archivo), Data("edicion-externa".utf8))
        XCTAssertThrowsError(try x.coordinador.journal(x.preparada.id))
        XCTAssertTrue(gestor.listarConflictos().isEmpty)
        XCTAssertTrue(FileManager.default.fileExists(
            atPath: destino.appendingPathComponent("diagnostico.json").path))
    }

    func testNoExportaTransaccionQueAunNoEsConflicto() throws {
        let raiz = try base(), journals = raiz.appendingPathComponent("journals")
        let destino = raiz.appendingPathComponent("doc")
        let c = CoordinadorPersistenciaTransaccionalIPad(base: journals)
        let p = try c.preparar(
            documentoId: DocumentoPersistenteID(),
            cambios: [.init(destino: destino,
                            mutacion: .escribir(Data("nuevo".utf8)),
                            precondicion: .debeNoExistir)])
        let gestor = GestorDiagnosticosPersistenciaIPad(
            baseTransacciones: journals, coordinador: c)
        XCTAssertThrowsError(try gestor.exportar(
            transaccionId: p.id,
            destino: raiz.appendingPathComponent("diag"))) {
                XCTAssertEqual($0 as? ErrorDiagnosticoPersistenciaIPad, .noEsConflicto)
            }
    }
}
