import XCTest
@testable import CuadernUEstudio

final class OperacionesLargasIPadTests: XCTestCase {
    func testOperacionExitosaTerminaEnCienPorCiento() {
        let coordinador = CoordinadorOperacionesLargasIPad()
        let fin = expectation(description: "fin")
        let id = coordinador.ejecutar(titulo: "Importar", bytesTotales: 100) { contexto in
            try contexto.reportar(fase: "Leyendo", progreso: 0.5,
                                  bytesProcesados: 50, bytesTotales: 100)
        } completion: { resultado in
            if case .failure(let error) = resultado { XCTFail("\(error)") }
            fin.fulfill()
        }
        wait(for: [fin], timeout: 2)
        let estado = coordinador.instantaneas(incluirTerminadas: true).first { $0.id == id }
        XCTAssertEqual(estado?.estado, .completada)
        XCTAssertEqual(estado?.progreso, 1)
        XCTAssertFalse(estado?.puedeCancelar ?? true)
    }

    func testCancelacionEjecutaRollbackYLimpieza() {
        let coordinador = CoordinadorOperacionesLargasIPad()
        let entro = expectation(description: "entro")
        let fin = expectation(description: "fin")
        let lock = NSLock()
        var rollback = false
        var limpieza = false

        let id = coordinador.ejecutar(titulo: "Exportar") { contexto in
            contexto.registrarRollback { lock.lock(); rollback = true; lock.unlock() }
            contexto.registrarLimpieza { lock.lock(); limpieza = true; lock.unlock() }
            entro.fulfill()
            for i in 0..<100 {
                Thread.sleep(forTimeInterval: 0.005)
                try contexto.reportar(fase: "Escribiendo", progreso: Double(i) / 100)
            }
        } completion: { _ in fin.fulfill() }

        wait(for: [entro], timeout: 1)
        coordinador.cancelar(id)
        wait(for: [fin], timeout: 2)
        lock.lock(); let r = rollback, l = limpieza; lock.unlock()
        XCTAssertTrue(r)
        XCTAssertTrue(l)
        XCTAssertEqual(coordinador.instantaneas(incluirTerminadas: true)
            .first { $0.id == id }?.estado, .cancelada)
    }

    func testFalloEjecutaRollbackYConservaMensaje() {
        enum PruebaError: Error { case fallo }
        let coordinador = CoordinadorOperacionesLargasIPad()
        let fin = expectation(description: "fin")
        var rollback = false
        let id = coordinador.ejecutar(titulo: "PDF") { contexto in
            contexto.registrarRollback { rollback = true }
            throw PruebaError.fallo
        } completion: { _ in fin.fulfill() }
        wait(for: [fin], timeout: 2)
        let estado = coordinador.instantaneas(incluirTerminadas: true).first { $0.id == id }
        XCTAssertTrue(rollback)
        XCTAssertEqual(estado?.estado, .fallida)
        XCTAssertNotNil(estado?.mensajeError)
    }

    func testReportarClampaProgreso() throws {
        let token = TokenCancelacionOperacionIPad()
        var ultimo = -1.0
        let contexto = ContextoOperacionLargaIPad(token: token) { _, valor, _, _ in ultimo = valor }
        try contexto.reportar(fase: "x", progreso: 9)
        XCTAssertEqual(ultimo, 1)
        try contexto.reportar(fase: "x", progreso: -2)
        XCTAssertEqual(ultimo, 0)
    }

    func testPreflightRechazaArchivoSobreLimite() throws {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("preflight-\(UUID().uuidString).bin")
        try Data(repeating: 1, count: 32).write(to: url)
        defer { try? FileManager.default.removeItem(at: url) }
        XCTAssertThrowsError(try PreflightOperacionLargaIPad.validarArchivo(url, maximoBytes: 16)) {
            guard case ErrorOperacionLargaIPad.archivoDemasiadoGrande = $0 else {
                return XCTFail("Error inesperado: \($0)")
            }
        }
    }
}
