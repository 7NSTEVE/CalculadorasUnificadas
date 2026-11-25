import XCTest
@testable import CuadernUEstudio

final class FrameEstudioIPadTests: XCTestCase {
    func testIniciarCreaUnSoloIntentoActivo() {
        var estudio = EstadoEstudioFrameIPad(objetivo: "Resolver sin mirar apuntes")
        let inicio = Date(timeIntervalSince1970: 100)

        let primero = estudio.iniciar(ahora: inicio)
        let segundo = estudio.iniciar(ahora: inicio.addingTimeInterval(30))

        XCTAssertEqual(primero, segundo)
        XCTAssertEqual(estudio.intentos.count, 1)
        XCTAssertEqual(estudio.estado, .intentando)
        XCTAssertTrue(estudio.intentos[0].activo)
    }

    func testCerrarCorrectoMarcaDominadoYCalculaDuracion() {
        var estudio = EstadoEstudioFrameIPad()
        let inicio = Date(timeIntervalSince1970: 1_000)
        let intento = estudio.iniciar(ahora: inicio)

        XCTAssertTrue(estudio.cerrar(
            intentoId: intento,
            resultado: .correcto,
            confianza: 0.82,
            ahora: inicio.addingTimeInterval(95)))

        XCTAssertEqual(estudio.estado, .dominado)
        XCTAssertEqual(estudio.aciertos, 1)
        XCTAssertEqual(estudio.intentos[0].duracion, 95, accuracy: 0.001)
        XCTAssertEqual(estudio.intentos[0].confianza, 0.82, accuracy: 0.001)
    }

    func testCerrarParaRepetirConservaErrorExacto() {
        var estudio = EstadoEstudioFrameIPad()
        let inicio = Date(timeIntervalSince1970: 2_000)
        let intento = estudio.iniciar(ahora: inicio)

        XCTAssertTrue(estudio.cerrar(
            intentoId: intento,
            resultado: .repetir,
            confianza: 0.31,
            error: "  Cambié el signo de la corriente  ",
            ahora: inicio.addingTimeInterval(140)))

        XCTAssertEqual(estudio.estado, .repetir)
        XCTAssertEqual(estudio.intentos[0].error, "Cambié el signo de la corriente")
        XCTAssertEqual(estudio.intentos[0].resultado, .repetir)
    }

    func testConfianzaSeLimitaEntreCeroYUno() {
        var estudio = EstadoEstudioFrameIPad()
        let inicio = Date(timeIntervalSince1970: 3_000)
        let intento = estudio.iniciar(ahora: inicio)
        _ = estudio.cerrar(
            intentoId: intento, resultado: .correcto,
            confianza: 4.2, ahora: inicio)

        XCTAssertEqual(estudio.intentos[0].confianza, 1, accuracy: 0.001)
    }

    func testNoCierraUnIntentoDistinto() {
        var estudio = EstadoEstudioFrameIPad()
        let intento = estudio.iniciar(ahora: Date(timeIntervalSince1970: 4_000))

        XCTAssertFalse(estudio.cerrar(
            intentoId: UUID(), resultado: .correcto, confianza: 1))
        XCTAssertEqual(estudio.intentoActivo?.id, intento)
        XCTAssertEqual(estudio.estado, .intentando)
    }

    func testConfianzaPromedioUsaSoloIntentosTerminados() {
        var estudio = EstadoEstudioFrameIPad()
        let base = Date(timeIntervalSince1970: 5_000)
        let uno = estudio.iniciar(ahora: base)
        _ = estudio.cerrar(
            intentoId: uno, resultado: .repetir, confianza: 0.2,
            ahora: base.addingTimeInterval(10))
        let dos = estudio.iniciar(ahora: base.addingTimeInterval(20))
        _ = estudio.cerrar(
            intentoId: dos, resultado: .correcto, confianza: 0.8,
            ahora: base.addingTimeInterval(30))
        _ = estudio.iniciar(ahora: base.addingTimeInterval(40))

        XCTAssertEqual(estudio.confianzaPromedio, 0.5, accuracy: 0.001)
        XCTAssertEqual(estudio.intentosTerminados.count, 2)
    }

    func testCopiaDocumentalConservaObjetivoPeroNoHistorialNiAgenda() {
        var original = EstadoEstudioFrameIPad(objetivo: "Resolver por superposición")
        let base = Date(timeIntervalSince1970: 6_000)
        let intento = original.iniciar(ahora: base)
        _ = original.cerrar(
            intentoId: intento, resultado: .repetir, confianza: 0.4,
            error: "Olvidé apagar una fuente", ahora: base.addingTimeInterval(60))

        let copia = original.reiniciadoParaCopiaDocumental()

        XCTAssertEqual(copia.objetivo, original.objetivo)
        XCTAssertEqual(copia.estado, .pendiente)
        XCTAssertTrue(copia.intentos.isEmpty)
        XCTAssertNil(copia.proximoRepaso)
        XCTAssertNil(copia.ultimoRepaso)
        XCTAssertNil(copia.rachaCorrectos)
    }

    func testFalloConBajaConfianzaProgramaRepasoEnVeinteMinutos() {
        let ahora = Date(timeIntervalSince1970: 10_000)
        let programacion = ProgramadorRepasoFrameIPad.programar(
            resultado: .repetir, confianza: 0.2, rachaPrevia: 4, ahora: ahora)

        XCTAssertEqual(programacion.rachaCorrectos, 0)
        XCTAssertEqual(
            programacion.proximoRepaso.timeIntervalSince(ahora),
            20 * 60,
            accuracy: 0.001)
    }

    func testAciertoSeguroProgramaSieteDiasYAumentaRacha() {
        let ahora = Date(timeIntervalSince1970: 20_000)
        let programacion = ProgramadorRepasoFrameIPad.programar(
            resultado: .correcto, confianza: 0.9, rachaPrevia: 0, ahora: ahora)

        XCTAssertEqual(programacion.rachaCorrectos, 1)
        XCTAssertEqual(
            programacion.proximoRepaso.timeIntervalSince(ahora),
            7 * 86_400,
            accuracy: 0.001)
    }

    func testRachaDeAciertosSeparaProgresivamenteElRepaso() {
        let ahora = Date(timeIntervalSince1970: 30_000)
        let primera = ProgramadorRepasoFrameIPad.programar(
            resultado: .correcto, confianza: 0.9, rachaPrevia: 0, ahora: ahora)
        let segunda = ProgramadorRepasoFrameIPad.programar(
            resultado: .correcto, confianza: 0.9, rachaPrevia: 1, ahora: ahora)

        XCTAssertGreaterThan(segunda.proximoRepaso, primera.proximoRepaso)
        XCTAssertEqual(segunda.rachaCorrectos, 2)
    }

    func testDescripcionRepasoDistingueAhoraHorasYDias() {
        let ahora = Date(timeIntervalSince1970: 40_000)
        var estudio = EstadoEstudioFrameIPad()

        estudio.proximoRepaso = ahora.addingTimeInterval(-1)
        XCTAssertEqual(estudio.descripcionRepaso(ahora: ahora), "repasar ahora")
        estudio.proximoRepaso = ahora.addingTimeInterval(2 * 3_600)
        XCTAssertEqual(estudio.descripcionRepaso(ahora: ahora), "repasar en 2 h")
        estudio.proximoRepaso = ahora.addingTimeInterval(3 * 86_400)
        XCTAssertEqual(estudio.descripcionRepaso(ahora: ahora), "repasar en 3 días")
    }

    func testDecodificaEstadoLegacySinClavesDeAgenda() throws {
        let json = "{\"objetivo\":\"Legacy\",\"estado\":\"pendiente\",\"intentos\":[]}"
        let estudio = try JSONDecoder().decode(
            EstadoEstudioFrameIPad.self, from: Data(json.utf8))

        XCTAssertEqual(estudio.objetivo, "Legacy")
        XCTAssertNil(estudio.proximoRepaso)
        XCTAssertNil(estudio.ultimoRepaso)
        XCTAssertNil(estudio.rachaCorrectos)
    }

    func testPlanificadorContinuaIntentoActivoAntesQueTodo() {
        let ahora = Date(timeIntervalSince1970: 50_000)
        var activo = EstadoEstudioFrameIPad(objetivo: "Activo")
        _ = activo.iniciar(ahora: ahora)
        let repetir = EstadoEstudioFrameIPad(
            objetivo: "Repetir", estado: .repetir)
        let pendiente = EstadoEstudioFrameIPad(objetivo: "Pendiente")

        let indice = PlanificadorFramesEstudioIPad.indiceSiguiente(
            [repetir, pendiente, activo], ahora: ahora)

        XCTAssertEqual(indice, 2)
    }

    func testPlanificadorPriorizaRepetirVencidoSobrePendienteYDominado() {
        let ahora = Date(timeIntervalSince1970: 60_000)
        let pendiente = EstadoEstudioFrameIPad(objetivo: "Pendiente")
        var repetir = EstadoEstudioFrameIPad(objetivo: "Repetir", estado: .repetir)
        repetir.proximoRepaso = ahora.addingTimeInterval(-1)
        let dominado = EstadoEstudioFrameIPad(objetivo: "Dominado", estado: .dominado)

        XCTAssertEqual(
            PlanificadorFramesEstudioIPad.indiceSiguiente(
                [pendiente, dominado, repetir], ahora: ahora),
            2)
    }

    func testPlanificadorRespetaDescansoTrasFalloYEligePendiente() {
        let ahora = Date(timeIntervalSince1970: 70_000)
        let pendiente = EstadoEstudioFrameIPad(objetivo: "Nuevo")
        var repetirLuego = EstadoEstudioFrameIPad(
            objetivo: "Acabo de fallar", estado: .repetir)
        repetirLuego.proximoRepaso = ahora.addingTimeInterval(20 * 60)

        XCTAssertEqual(
            PlanificadorFramesEstudioIPad.indiceSiguiente(
                [repetirLuego, pendiente], ahora: ahora),
            1)
    }

    func testPlanificadorDentroDelMismoNivelEligeMenorConfianza() {
        let ahora = Date(timeIntervalSince1970: 80_000)
        var alta = EstadoEstudioFrameIPad(objetivo: "Alta", estado: .repetir)
        var baja = EstadoEstudioFrameIPad(objetivo: "Baja", estado: .repetir)
        let base = Date(timeIntervalSince1970: 20)

        let a = alta.iniciar(ahora: base)
        _ = alta.cerrar(
            intentoId: a, resultado: .repetir, confianza: 0.9,
            ahora: base.addingTimeInterval(5))
        let b = baja.iniciar(ahora: base)
        _ = baja.cerrar(
            intentoId: b, resultado: .repetir, confianza: 0.2,
            ahora: base.addingTimeInterval(5))
        alta.proximoRepaso = ahora.addingTimeInterval(-10)
        baja.proximoRepaso = ahora.addingTimeInterval(-10)

        XCTAssertEqual(
            PlanificadorFramesEstudioIPad.indiceSiguiente(
                [alta, baja], ahora: ahora),
            1)
    }
}
