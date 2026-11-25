import XCTest
@testable import CuadernUEstudio

final class ModoPresentacionIPadTests: XCTestCase {
    private func carpetaTemporal() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("presentacion-tests-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    func testSesionReabreConDocumentoYPagina() throws {
        let base = try carpetaTemporal()
        let documento = DocumentoPersistenteID(UUID())
        do {
            let coordinador = CoordinadorPresentacionIPad(base: base)
            _ = coordinador.iniciar(documentoId: documento, pagina: 7)
            _ = try coordinador.mutar { estado in
                estado.notasPrivadas = "No mostrar esta nota a la audiencia"
                estado.pantallaNegra = true
            }
        }
        let reabierto = CoordinadorPresentacionIPad(base: base).estado()
        XCTAssertEqual(reabierto?.documentoId, documento)
        XCTAssertEqual(reabierto?.pagina, 7)
        XCTAssertEqual(reabierto?.notasPrivadas, "No mostrar esta nota a la audiencia")
        XCTAssertEqual(reabierto?.pantallaNegra, true)
    }

    func testPunteroSeNormalizaYClampa() throws {
        var estado = EstadoPresentacionIPad(documentoId: DocumentoPersistenteID())
        estado.moverPuntero(a: CGPoint(x: 250, y: -20), en: CGSize(width: 200, height: 100))
        XCTAssertEqual(estado.puntero.xNormalizado, 1, accuracy: 0.0001)
        XCTAssertEqual(estado.puntero.yNormalizado, 0, accuracy: 0.0001)
        XCTAssertTrue(estado.puntero.visible)
    }

    func testNoPermiteConfundirDocumentos() throws {
        let base = try carpetaTemporal()
        let a = DocumentoPersistenteID()
        let b = DocumentoPersistenteID()
        let coordinador = CoordinadorPresentacionIPad(base: base)
        _ = coordinador.iniciar(documentoId: a)
        XCTAssertNoThrow(try coordinador.asegurar(documentoId: a))
        XCTAssertThrowsError(try coordinador.asegurar(documentoId: b)) { error in
            guard case ErrorPresentacionIPad.documentoDistinto = error else {
                return XCTFail("Error inesperado: \(error)")
            }
        }
    }

    func testFinalizarEliminaSesionPersistida() throws {
        let base = try carpetaTemporal()
        let coordinador = CoordinadorPresentacionIPad(base: base)
        _ = coordinador.iniciar(documentoId: DocumentoPersistenteID(), pagina: 1)
        coordinador.finalizar()
        XCTAssertNil(coordinador.estado())
        XCTAssertNil(CoordinadorPresentacionIPad(base: base).estado())
    }

    func testIrAPaginaNoAceptaIndicesNegativos() {
        var estado = EstadoPresentacionIPad(documentoId: DocumentoPersistenteID())
        estado.ir(pagina: -9)
        XCTAssertEqual(estado.pagina, 0)
    }
}
