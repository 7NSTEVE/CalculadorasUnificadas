import XCTest
@testable import CuadernUEstudio

private final class ParticipanteGuardadoPruebaIPad: ParticipanteGuardadoDocumentoIPad {
    let nombreParticipanteGuardado: String
    var cambios: [CambioArchivoTransaccionalIPad]
    var errorPreparacion: Error?
    var confirmados = 0
    var abortados = 0

    init(_ nombre: String, cambios: [CambioArchivoTransaccionalIPad] = []) {
        nombreParticipanteGuardado = nombre
        self.cambios = cambios
    }

    func cambiosParaGuardar(documentoId: DocumentoPersistenteID) throws
        -> [CambioArchivoTransaccionalIPad] {
        if let errorPreparacion { throw errorPreparacion }
        return cambios
    }

    func guardadoConfirmado(documentoId: DocumentoPersistenteID) {
        confirmados += 1
    }

    func guardadoAbortado(documentoId: DocumentoPersistenteID, error: Error) {
        abortados += 1
    }
}

final class GuardadoDocumentoAtomicoIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("guardar-doc-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func texto(_ url: URL) -> String? {
        (try? Data(contentsOf: url)).flatMap { String(data: $0, encoding: .utf8) }
    }

    func testParticipantesSeConfirmanSoloTrasCommitCompleto() throws {
        let raiz = try base(), a = raiz.appendingPathComponent("doc"), b = raiz.appendingPathComponent("side")
        let p1 = ParticipanteGuardadoPruebaIPad("documento", cambios: [
            .init(destino: a, mutacion: .escribir(Data("A".utf8)), precondicion: .debeNoExistir)
        ])
        let p2 = ParticipanteGuardadoPruebaIPad("sidecar", cambios: [
            .init(destino: b, mutacion: .escribir(Data("B".utf8)), precondicion: .debeNoExistir)
        ])
        let tx = CoordinadorPersistenciaTransaccionalIPad(
            base: raiz.appendingPathComponent("journals"))
        let resultado = try CoordinadorGuardadoDocumentoAtomicoIPad(transacciones: tx)
            .guardar(documentoId: DocumentoPersistenteID(), participantes: [p1, p2])

        XCTAssertTrue(resultado.huboCambios)
        XCTAssertEqual(resultado.archivos, 2)
        XCTAssertEqual(p1.confirmados, 1); XCTAssertEqual(p2.confirmados, 1)
        XCTAssertEqual(p1.abortados, 0); XCTAssertEqual(p2.abortados, 0)
        XCTAssertEqual(texto(a), "A"); XCTAssertEqual(texto(b), "B")
    }

    func testFalloPreparandoUnParticipanteNotificaAbortATodos() throws {
        enum ErrorPrueba: Error { case fallo }
        let p1 = ParticipanteGuardadoPruebaIPad("a")
        let p2 = ParticipanteGuardadoPruebaIPad("b")
        p2.errorPreparacion = ErrorPrueba.fallo
        let coordinador = CoordinadorGuardadoDocumentoAtomicoIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(
                base: try base()))

        XCTAssertThrowsError(try coordinador.guardar(
            documentoId: DocumentoPersistenteID(), participantes: [p1, p2]))
        XCTAssertEqual(p1.confirmados, 0); XCTAssertEqual(p2.confirmados, 0)
        XCTAssertEqual(p1.abortados, 1); XCTAssertEqual(p2.abortados, 1)
    }

    func testSinCambiosEsNoOpConfirmadoSinJournal() throws {
        let p = ParticipanteGuardadoPruebaIPad("metadata")
        let resultado = try CoordinadorGuardadoDocumentoAtomicoIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(base: try base()))
            .guardar(documentoId: DocumentoPersistenteID(), participantes: [p])
        XCTAssertFalse(resultado.huboCambios)
        XCTAssertNil(resultado.transaccionId)
        XCTAssertEqual(p.confirmados, 1)
    }

    func testRechazaParticipanteDuplicado() throws {
        let p1 = ParticipanteGuardadoPruebaIPad("audio")
        let p2 = ParticipanteGuardadoPruebaIPad("audio")
        XCTAssertThrowsError(try CoordinadorGuardadoDocumentoAtomicoIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(base: try base()))
            .guardar(documentoId: DocumentoPersistenteID(), participantes: [p1, p2])) {
                guard case ErrorGuardadoDocumentoAtomicoIPad.participanteDuplicado = $0 else {
                    return XCTFail("Error inesperado: \($0)")
                }
            }
    }

    func testGateBloqueaDocumentoConConflictoDeRecovery() throws {
        let raiz = try base(), journalBase = raiz.appendingPathComponent("journals")
        let destino = raiz.appendingPathComponent("doc")
        try Data("v1".utf8).write(to: destino)
        let docId = DocumentoPersistenteID()
        let tx = CoordinadorPersistenciaTransaccionalIPad(base: journalBase)
        let preparada = try tx.preparar(
            documentoId: docId,
            cambios: [.init(destino: destino,
                            mutacion: .escribir(Data("v2".utf8)))])
        try Data("edicion-externa".utf8).write(to: destino)

        let estado = GateIntegridadArranqueIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(base: journalBase))
            .ejecutar()
        guard case .bloqueado(let conflictos) = estado else {
            return XCTFail("El gate debió bloquear el documento")
        }
        XCTAssertEqual(conflictos.count, 1)
        XCTAssertEqual(conflictos[0].id, preparada.id)
        XCTAssertEqual(conflictos[0].documentoId, docId)
        XCTAssertEqual(conflictos[0].rutas, [destino.path])
        XCTAssertEqual(texto(destino), "edicion-externa")
    }
}
