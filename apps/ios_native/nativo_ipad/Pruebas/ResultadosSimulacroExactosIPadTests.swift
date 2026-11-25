import XCTest
@testable import CuadernUEstudio

final class ResultadosSimulacroExactosIPadTests: XCTestCase {
    func testRecopiladorUsaIntentoExactoAunqueExistaUnoPosterior() {
        let frameId = UUID()
        let inicio = Date(timeIntervalSince1970: 10_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: [frameId])
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)

        var estudio = EstadoEstudioFrameIPad(objetivo: "Superposición")
        let intentoSimulacro = estudio.iniciar(ahora: inicio.addingTimeInterval(5))
        XCTAssertTrue(estudio.cerrar(
            intentoId: intentoSimulacro,
            resultado: .repetir,
            confianza: 0.9,
            error: "Cambié el signo",
            ahora: inicio.addingTimeInterval(65)))
        XCTAssertTrue(sesion.registrarCierre(
            frameId: frameId,
            intentoId: intentoSimulacro,
            fin: inicio.addingTimeInterval(65),
            ahora: inicio.addingTimeInterval(65)))

        let posterior = estudio.iniciar(ahora: inicio.addingTimeInterval(100))
        XCTAssertTrue(estudio.cerrar(
            intentoId: posterior,
            resultado: .correcto,
            confianza: 1,
            ahora: inicio.addingTimeInterval(130)))

        let resultados = RecopiladorResultadosSimulacroIPad.recopilar(
            sesion: sesion,
            estudiosPorFrame: [frameId: estudio],
            titulosPorFrame: [frameId: "Problema 1"])

        XCTAssertEqual(resultados.count, 1)
        XCTAssertEqual(resultados[0].resultado, .repetir)
        XCTAssertEqual(resultados[0].confianza, 0.9, accuracy: 0.001)
        XCTAssertEqual(resultados[0].error, "Cambié el signo")
        XCTAssertEqual(resultados[0].segundos, 60, accuracy: 0.001)
    }

    func testMapaFrameIntentoSobreviveCodificacion() throws {
        let frameId = UUID()
        let intentoId = UUID()
        let inicio = Date(timeIntervalSince1970: 20_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .rapido, frameIds: [frameId])
        var original = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)
        XCTAssertTrue(original.registrarCierre(
            frameId: frameId,
            intentoId: intentoId,
            fin: inicio.addingTimeInterval(20),
            ahora: inicio.addingTimeInterval(20)))

        let datos = try JSONEncoder().encode(original)
        let restaurada = try JSONDecoder().decode(
            EstadoSesionSimulacroIPad.self, from: datos)

        XCTAssertEqual(restaurada.intentoId(para: frameId), intentoId)
        XCTAssertEqual(restaurada.terminados, 1)
    }

    func testDecodificaSidecarLegacySinMapaDeIntentos() throws {
        let frameId = UUID()
        let inicio = Date(timeIntervalSince1970: 30_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: [frameId])
        let planData = try JSONEncoder().encode(plan)
        let planObject = try XCTUnwrap(
            JSONSerialization.jsonObject(with: planData) as? [String: Any])
        let objeto: [String: Any] = [
            "plan": planObject,
            "inicio": inicio.timeIntervalSinceReferenceDate,
            "indiceActual": 0,
            "completados": [],
            "omitidos": [],
            "intentosContabilizados": [],
        ]
        let legacy = try JSONSerialization.data(withJSONObject: objeto)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .deferredToDate

        let sesion = try decoder.decode(EstadoSesionSimulacroIPad.self, from: legacy)

        XCTAssertNil(sesion.intentoId(para: frameId))
        XCTAssertEqual(sesion.frameActualId, frameId)
    }

    func testRecopiladorIgnoraFrameOmitidoSinIntentoContabilizado() {
        let frameId = UUID()
        let inicio = Date(timeIntervalSince1970: 40_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .rapido, frameIds: [frameId])
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)
        _ = sesion.omitirActual(ahora: inicio.addingTimeInterval(5))

        let resultados = RecopiladorResultadosSimulacroIPad.recopilar(
            sesion: sesion,
            estudiosPorFrame: [frameId: EstadoEstudioFrameIPad(objetivo: "X")],
            titulosPorFrame: [frameId: "Omitido"])

        XCTAssertTrue(resultados.isEmpty)
    }
}
