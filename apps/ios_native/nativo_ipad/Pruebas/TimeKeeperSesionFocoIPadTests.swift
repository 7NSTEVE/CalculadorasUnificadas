import XCTest
@testable import CuadernUEstudio

final class TimeKeeperSesionFocoIPadTests: XCTestCase {
    func testRestanteSeDerivaDeInicioPersistido() {
        let inicio = Date(timeIntervalSince1970: 1_000)
        let sesion = SesionFoco(objetivo: "Repasar", inicio: inicio, duracionMinutos: 25)
        XCTAssertEqual(sesion.segundosRestantesTimeKeeper(
            ahora: inicio.addingTimeInterval(600)), 900, accuracy: 0.001)
    }

    func testReconciliarExpiradaLaCierraEnFinProgramado() {
        let inicio = Date(timeIntervalSince1970: 1_000)
        var base = BaseAcademica()
        base.foco = [SesionFoco(objetivo: "A", inicio: inicio, duracionMinutos: 5)]

        TimeKeeperSesionFocoIPad.reconciliarExpiradas(
            &base, ahora: inicio.addingTimeInterval(900))

        XCTAssertFalse(base.foco[0].activa)
        XCTAssertEqual(base.foco[0].fin, inicio.addingTimeInterval(300))
    }

    func testIniciarCierraSesionActivaAnterior() {
        let t0 = Date(timeIntervalSince1970: 1_000)
        var base = BaseAcademica()
        let anterior = SesionFoco(objetivo: "Anterior", inicio: t0, duracionMinutos: 60)
        base.foco = [anterior]

        let nueva = TimeKeeperSesionFocoIPad.iniciar(
            en: &base, objetivo: "Nueva", minutos: 25,
            ahora: t0.addingTimeInterval(30))

        XCTAssertFalse(base.foco[0].activa)
        XCTAssertEqual(base.foco[0].fin, t0.addingTimeInterval(30))
        XCTAssertEqual(TimeKeeperSesionFocoIPad.activa(
            en: base, ahora: t0.addingTimeInterval(31))?.id, nueva.id)
    }

    func testDuracionSeAcota() {
        var base = BaseAcademica()
        let corta = TimeKeeperSesionFocoIPad.iniciar(
            en: &base, objetivo: "", minutos: 0,
            ahora: Date(timeIntervalSince1970: 100))
        XCTAssertEqual(corta.duracionMinutos, 1)

        let larga = TimeKeeperSesionFocoIPad.iniciar(
            en: &base, objetivo: "Larga", minutos: 50_000,
            ahora: Date(timeIntervalSince1970: 200))
        XCTAssertEqual(larga.duracionMinutos, 720)
    }

    func testRegistrarInterrupcionSoloEnActiva() {
        var base = BaseAcademica()
        let sesion = TimeKeeperSesionFocoIPad.iniciar(
            en: &base, objetivo: "A", minutos: 10,
            ahora: Date(timeIntervalSince1970: 100))
        XCTAssertEqual(TimeKeeperSesionFocoIPad.registrarInterrupcion(
            sesion.id, en: &base), 1)
        _ = TimeKeeperSesionFocoIPad.terminar(
            sesion.id, en: &base, ahora: Date(timeIntervalSince1970: 130))
        XCTAssertNil(TimeKeeperSesionFocoIPad.registrarInterrupcion(sesion.id, en: &base))
    }

    func testTerminarCongelaEnAhoraSiEsAntesDelFin() {
        let t0 = Date(timeIntervalSince1970: 1_000)
        var base = BaseAcademica()
        let sesion = TimeKeeperSesionFocoIPad.iniciar(
            en: &base, objetivo: "A", minutos: 25, ahora: t0)
        let terminada = TimeKeeperSesionFocoIPad.terminar(
            sesion.id, en: &base, ahora: t0.addingTimeInterval(90))
        XCTAssertEqual(terminada?.fin, t0.addingTimeInterval(90))
        XCTAssertFalse(terminada?.activa ?? true)
    }
}
