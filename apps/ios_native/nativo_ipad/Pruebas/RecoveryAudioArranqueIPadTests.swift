import XCTest

final class RecoveryAudioArranqueIPadTests: XCTestCase {
    func testEjecutaUnaSolaVezPorProceso() {
        var llamadasManifest = 0
        var llamadasVinculos = 0
        let id = UUID()
        let vinculo = UUID()
        let coordinador = CoordinadorRecoveryAudioArranqueIPad(
            ejecutar: {
                llamadasManifest += 1
                return [id]
            },
            ejecutarVinculos: {
                llamadasVinculos += 1
                return [vinculo]
            })

        let primero = coordinador.ejecutarUnaVez()
        let segundo = coordinador.ejecutarUnaVez()

        XCTAssertEqual(llamadasManifest, 1)
        XCTAssertEqual(llamadasVinculos, 1)
        XCTAssertEqual(primero, segundo)
        XCTAssertEqual(primero.sesionesMarcadas, [id])
        XCTAssertEqual(primero.vinculosMarcados, [vinculo])
        XCTAssertNil(primero.error)
        XCTAssertTrue(primero.requiereAtencion)
    }

    func testFalloManifestNoDescartaVinculosProtegidos() {
        struct Fallo: LocalizedError {
            var errorDescription: String? { "manifest ilegible" }
        }
        let vinculo = UUID()
        let coordinador = CoordinadorRecoveryAudioArranqueIPad(
            ejecutar: { throw Fallo() },
            ejecutarVinculos: { [vinculo] })

        let resultado = coordinador.ejecutarUnaVez()
        XCTAssertTrue(resultado.sesionesMarcadas.isEmpty)
        XCTAssertEqual(resultado.vinculosMarcados, [vinculo])
        XCTAssertEqual(resultado.error, "manifest: manifest ilegible")
        XCTAssertTrue(resultado.requiereAtencion)
    }

    func testFalloJournalNoDescartaManifestProtegido() {
        struct Fallo: LocalizedError {
            var errorDescription: String? { "journal ilegible" }
        }
        let sesion = UUID()
        let coordinador = CoordinadorRecoveryAudioArranqueIPad(
            ejecutar: { [sesion] },
            ejecutarVinculos: { throw Fallo() })

        let resultado = coordinador.ejecutarUnaVez()
        XCTAssertEqual(resultado.sesionesMarcadas, [sesion])
        XCTAssertTrue(resultado.vinculosMarcados.isEmpty)
        XCTAssertEqual(resultado.error, "journal: journal ilegible")
        XCTAssertTrue(resultado.requiereAtencion)
    }

    func testSinSesionesNiErrorNoRequiereAtencion() {
        let coordinador = CoordinadorRecoveryAudioArranqueIPad(
            ejecutar: { [] }, ejecutarVinculos: { [] })
        let resultado = coordinador.ejecutarUnaVez()
        XCTAssertFalse(resultado.requiereAtencion)
        XCTAssertTrue(resultado.sesionesMarcadas.isEmpty)
        XCTAssertTrue(resultado.vinculosMarcados.isEmpty)
        XCTAssertNil(resultado.error)
    }

    func testIdsSeOrdenanParaResultadoDeterminista() {
        let a = UUID(uuidString: "00000000-0000-0000-0000-000000000002")!
        let b = UUID(uuidString: "00000000-0000-0000-0000-000000000001")!
        let c = UUID(uuidString: "00000000-0000-0000-0000-000000000004")!
        let d = UUID(uuidString: "00000000-0000-0000-0000-000000000003")!
        let coordinador = CoordinadorRecoveryAudioArranqueIPad(
            ejecutar: { [a, b] }, ejecutarVinculos: { [c, d] })
        let resultado = coordinador.ejecutarUnaVez()
        XCTAssertEqual(resultado.sesionesMarcadas, [b, a])
        XCTAssertEqual(resultado.vinculosMarcados, [d, c])
    }
}
