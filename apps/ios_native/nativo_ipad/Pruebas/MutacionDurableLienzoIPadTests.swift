import XCTest
@testable import CuadernUEstudio

final class MutacionDurableLienzoIPadTests: XCTestCase {
    func testCanalConservaElManejadorDeSesion() {
        let centro = NotificationCenter()
        let recibida = expectation(description: "mutación durable recibida")
        var manejador: UInt64?
        let observador = centro.addObserver(
            forName: CanalMutacionDurableLienzoIPad.nombre,
            object: nil,
            queue: nil) { nota in
                manejador = CanalMutacionDurableLienzoIPad.manejadorNucleo(de: nota)
                recibida.fulfill()
            }
        defer { centro.removeObserver(observador) }

        CanalMutacionDurableLienzoIPad.publicar(
            manejadorNucleo: 0xA11CE,
            centro: centro)

        wait(for: [recibida], timeout: 0.1)
        XCTAssertEqual(manejador, 0xA11CE)
    }

    func testManejadoresDistintosNoSeConfunden() {
        let nota = Notification(
            name: CanalMutacionDurableLienzoIPad.nombre,
            object: nil,
            userInfo: [
                CanalMutacionDurableLienzoIPad.claveManejadorNucleo:
                    NSNumber(value: UInt64(77)),
            ])
        let recibido = CanalMutacionDurableLienzoIPad.manejadorNucleo(de: nota)

        XCTAssertEqual(recibido, 77)
        XCTAssertNotEqual(recibido, 78)
    }

    func testArrastreConMuchosDeltasProduceUnaSolaMutacionAlTerminar() {
        var acumulador = AcumuladorMutacionDurableLienzoIPad()
        acumulador.comenzar()
        acumulador.registrarCambio()
        acumulador.registrarCambio()
        acumulador.registrarCambio()

        XCTAssertTrue(acumulador.terminar())
        XCTAssertFalse(acumulador.terminar(),
                       "la misma interacción no debe publicar dos generaciones")
    }

    func testArrastreSinCambioRealNoProgramaAutoguardado() {
        var acumulador = AcumuladorMutacionDurableLienzoIPad()
        acumulador.comenzar()

        XCTAssertFalse(acumulador.terminar())
    }

    func testNuevoArrastreDescartaEstadoDeLaInteraccionAnterior() {
        var acumulador = AcumuladorMutacionDurableLienzoIPad()
        acumulador.registrarCambio()
        acumulador.comenzar()

        XCTAssertFalse(acumulador.terminar())
    }
}
