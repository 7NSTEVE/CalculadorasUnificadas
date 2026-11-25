import XCTest

final class TransaccionHistorialSesionIPadTests: XCTestCase {
    private func anadirTrazo(_ sesion: Sesion, x: Float = 20) -> Bool {
        sesion.comenzarTrazo(ancho: 3, color: 0xFF102030, resaltador: false)
        sesion.anadirMuestra(
            x: x, y: 30, presion: 0.6,
            altitud: 1, azimut: 0, tiempo: 1)
        sesion.anadirMuestra(
            x: x + 50, y: 55, presion: 0.7,
            altitud: 1, azimut: 0, tiempo: 1.01)
        return sesion.terminarTrazo() != nil
    }

    func testCommitTrazoTextoYMedioEsUnaSolaEntradaUndo() throws {
        let sesion = try XCTUnwrap(Sesion())

        let confirmada = sesion.conTransaccionHistorial {
            guard anadirTrazo(sesion),
                  sesion.anadirTexto(
                    "RLC",
                    marco: CGRect(x: 100, y: 120, width: 140, height: 52),
                    id: "texto-tx") != nil,
                  sesion.anadirMedio(
                    hash: "hash-tx", nombre: "diagrama.png", tipo: .imagen,
                    marco: CGRect(x: 280, y: 160, width: 180, height: 120),
                    id: "medio-tx") != nil else { return false }
            return true
        }

        XCTAssertTrue(confirmada)
        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertEqual(sesion.textos.count, 1)
        XCTAssertEqual(sesion.medios.count, 1)

        sesion.deshacer()
        XCTAssertEqual(sesion.cantidadTrazos, 0)
        XCTAssertEqual(sesion.textos.count, 0)
        XCTAssertEqual(sesion.medios.count, 0)

        sesion.rehacer()
        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertEqual(sesion.textos.count, 1)
        XCTAssertEqual(sesion.medios.count, 1)
        XCTAssertEqual(sesion.textos.first?.id, "texto-tx")
        XCTAssertEqual(sesion.medios.first?.id, "medio-tx")
    }

    func testRollbackRestauraPaginaYUndoAnterior() throws {
        let sesion = try XCTUnwrap(Sesion())
        XCTAssertTrue(anadirTrazo(sesion))
        XCTAssertEqual(sesion.cantidadTrazos, 1)

        let confirmada = sesion.conTransaccionHistorial {
            guard sesion.anadirTexto(
                "temporal",
                marco: CGRect(x: 80, y: 90, width: 120, height: 44),
                id: "texto-temporal") != nil else { return false }
            return false
        }

        XCTAssertFalse(confirmada)
        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertTrue(sesion.textos.isEmpty)

        // Si el rollback hubiera destruido la pila anterior, este Undo no
        // quitaría el trazo creado antes de abrir la transacción.
        sesion.deshacer()
        XCTAssertEqual(sesion.cantidadTrazos, 0)
        XCTAssertTrue(sesion.textos.isEmpty)
    }

    func testRollbackConservaRamaRedoAnterior() throws {
        let sesion = try XCTUnwrap(Sesion())
        XCTAssertTrue(anadirTrazo(sesion))
        sesion.deshacer()
        XCTAssertEqual(sesion.cantidadTrazos, 0)

        let confirmada = sesion.conTransaccionHistorial {
            guard sesion.anadirTexto(
                "no debe quedar",
                marco: CGRect(x: 10, y: 10, width: 120, height: 40),
                id: "rollback-redo") != nil else { return false }
            return false
        }
        XCTAssertFalse(confirmada)
        XCTAssertTrue(sesion.textos.isEmpty)
        XCTAssertEqual(sesion.cantidadTrazos, 0)

        // La rama Redo que existía antes de la transacción debe sobrevivir.
        sesion.rehacer()
        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertTrue(sesion.textos.isEmpty)
    }

    func testTransaccionAnidadaSeRechazaSinRomperLaExterna() throws {
        let sesion = try XCTUnwrap(Sesion())

        let externa = sesion.conTransaccionHistorial {
            let anidada = sesion.conTransaccionHistorial { true }
            XCTAssertFalse(anidada)
            return sesion.anadirTexto(
                "externa",
                marco: CGRect(x: 40, y: 50, width: 100, height: 40),
                id: "tx-externa") != nil
        }

        XCTAssertTrue(externa)
        XCTAssertEqual(sesion.textos.count, 1)
        sesion.deshacer()
        XCTAssertTrue(sesion.textos.isEmpty)
    }
}
