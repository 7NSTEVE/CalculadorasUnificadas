import XCTest
@testable import CuadernUEstudio

final class ColaSincronizacionDurableIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("sync-cola-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func operacion(documento: DocumentoPersistenteID,
                           contador: UInt64 = 1,
                           objeto: String = "obj") -> OperacionColaborativa {
        OperacionColaborativa(
            sesionId: UUID(),
            documentoId: documento.claveCompatibilidadUInt64,
            marca: MarcaLamport(contador: contador, autor: "test"),
            tipo: .actualizarTexto,
            objetoId: objeto,
            carga: Data("x".utf8),
            tumba: false,
            rolAutor: .editor)
    }

    func testEncolaYReabreConUUIDCompleto() throws {
        let base = try base(), doc = DocumentoPersistenteID()
        let op = operacion(documento: doc)
        do { _ = try ColaSincronizacionDurableIPad(base: base).encolar(documentoId: doc, operacion: op) }
        let reabierta = ColaSincronizacionDurableIPad(base: base)
        XCTAssertEqual(reabierta.pendientes(documentoId: doc).count, 1)
        XCTAssertEqual(reabierta.pendientes(documentoId: doc).first?.operacion.id, op.id)
    }

    func testRechazaCruceEntreUUIDYClaveLegacy() throws {
        let a = DocumentoPersistenteID(), b = DocumentoPersistenteID()
        let cola = ColaSincronizacionDurableIPad(base: try base())
        XCTAssertThrowsError(try cola.encolar(documentoId: b, operacion: operacion(documento: a))) {
            XCTAssertEqual($0 as? ErrorColaSyncIPad, .identidadLegacyNoCoincide)
        }
    }

    func testRetryAplicaBackoffYLuegoReintentarAhora() throws {
        let doc = DocumentoPersistenteID(), cola = ColaSincronizacionDurableIPad(base: try base())
        let op = operacion(documento: doc)
        _ = try cola.encolar(documentoId: doc, operacion: op)
        cola.marcarEnviando([op.id])
        let ahora = Date(timeIntervalSince1970: 1000)
        cola.fallar([op.id], error: "offline", ahora: ahora)
        XCTAssertTrue(cola.listos(ahora: ahora).isEmpty)
        XCTAssertFalse(cola.listos(ahora: ahora.addingTimeInterval(3601)).isEmpty)
        cola.reintentarAhora(documentoId: doc)
        XCTAssertEqual(cola.pendientes(documentoId: doc).first?.estado, .pendiente)
    }

    func testConfirmadoNoVuelveAListos() throws {
        let doc = DocumentoPersistenteID(), cola = ColaSincronizacionDurableIPad(base: try base())
        let op = operacion(documento: doc)
        _ = try cola.encolar(documentoId: doc, operacion: op)
        cola.confirmar([op.id])
        XCTAssertTrue(cola.listos(ahora: Date.distantFuture).isEmpty)
        XCTAssertTrue(cola.pendientes(documentoId: doc).isEmpty)
    }

    func testOrdenListosRespetaLamport() throws {
        let doc = DocumentoPersistenteID(), cola = ColaSincronizacionDurableIPad(base: try base())
        let tarde = operacion(documento: doc, contador: 9, objeto: "b")
        let temprano = operacion(documento: doc, contador: 2, objeto: "a")
        _ = try cola.encolar(documentoId: doc, operacion: tarde)
        _ = try cola.encolar(documentoId: doc, operacion: temprano)
        XCTAssertEqual(cola.listos().map { $0.operacion.marca.contador }, [2, 9])
    }
}
