import XCTest
@testable import CuadernUEstudio

final class FrameProblemBankVinculoTests: XCTestCase {
    func testCopiaConservaProblemaIdPeroReiniciaHistorial() {
        let problemaId = UUID()
        var original = EstadoEstudioFrameIPad(
            objetivo: "Resolver circuito equivalente",
            problemaId: problemaId)
        let inicio = Date(timeIntervalSince1970: 100)
        let intento = original.iniciar(ahora: inicio)
        _ = original.cerrar(
            intentoId: intento,
            resultado: .repetir,
            confianza: 0.25,
            error: "Confundí serie con paralelo",
            ahora: inicio.addingTimeInterval(90))

        let copia = original.reiniciadoParaCopiaDocumental()

        XCTAssertEqual(copia.problemaId, problemaId)
        XCTAssertEqual(copia.objetivo, original.objetivo)
        XCTAssertEqual(copia.estado, .pendiente)
        XCTAssertTrue(copia.intentos.isEmpty)
        XCTAssertNil(copia.proximoRepaso)
    }

    func testEstadoLegacySinProblemaIdSigueDecodificando() throws {
        let json = "{\"objetivo\":\"Legacy\",\"estado\":\"pendiente\",\"intentos\":[]}"
        let estudio = try JSONDecoder().decode(
            EstadoEstudioFrameIPad.self, from: Data(json.utf8))

        XCTAssertEqual(estudio.objetivo, "Legacy")
        XCTAssertNil(estudio.problemaId)
    }

    func testMarcadorDeSincronizacionEsDeterministaYUnicoPorIntento() {
        let uno = UUID(uuidString: "00000000-0000-0000-0000-000000000001")!
        let dos = UUID(uuidString: "00000000-0000-0000-0000-000000000002")!

        XCTAssertEqual(
            SincronizadorProblemaFrameIPad.marcador(intentoId: uno),
            "[frame-intento:00000000-0000-0000-0000-000000000001]")
        XCTAssertNotEqual(
            SincronizadorProblemaFrameIPad.marcador(intentoId: uno),
            SincronizadorProblemaFrameIPad.marcador(intentoId: dos))
    }
}
