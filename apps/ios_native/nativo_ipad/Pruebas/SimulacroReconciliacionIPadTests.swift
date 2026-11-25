import XCTest
@testable import CuadernUEstudio

final class SimulacroReconciliacionIPadTests: XCTestCase {
    func testIntentoAnteriorAlSimulacroNoAvanza() {
        let frame = UUID()
        let inicio = Date(timeIntervalSince1970: 1_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: [frame])
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)

        XCTAssertFalse(sesion.registrarCierre(
            frameId: frame,
            intentoId: UUID(),
            fin: inicio.addingTimeInterval(-1),
            ahora: inicio))
        XCTAssertEqual(sesion.frameActualId, frame)
        XCTAssertEqual(sesion.terminados, 0)
    }

    func testMismoIntentoNoSeContabilizaDosVeces() {
        let frames = [UUID(), UUID()]
        let inicio = Date(timeIntervalSince1970: 2_000)
        let intento = UUID()
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: frames)
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)

        XCTAssertTrue(sesion.registrarCierre(
            frameId: frames[0], intentoId: intento,
            fin: inicio.addingTimeInterval(30), ahora: inicio.addingTimeInterval(30)))
        XCTAssertFalse(sesion.registrarCierre(
            frameId: frames[0], intentoId: intento,
            fin: inicio.addingTimeInterval(30), ahora: inicio.addingTimeInterval(31)))
        XCTAssertEqual(sesion.frameActualId, frames[1])
        XCTAssertEqual(sesion.terminados, 1)
    }

    func testReconciliadorReparaCierrePersistidoTrasInterrupcion() {
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: carpeta) }
        let repo = RepositorioSimulacroDocumentoIPad(base: carpeta)
        let frames = [UUID(), UUID()]
        let inicio = Date(timeIntervalSince1970: 3_000)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: frames)
        XCTAssertTrue(repo.guardar(
            EstadoSesionSimulacroIPad(plan: plan, inicio: inicio), documento: 123))

        var estudio = EstadoEstudioFrameIPad(objetivo: "Resolver")
        let intento = estudio.iniciar(ahora: inicio.addingTimeInterval(10))
        XCTAssertTrue(estudio.cerrar(
            intentoId: intento,
            resultado: .correcto,
            confianza: 0.8,
            ahora: inicio.addingTimeInterval(70)))

        let reparada = ReconciliadorSimulacroFramesIPad.registrarTerminados(
            estudio: estudio,
            documentoId: 123,
            frameId: frames[0],
            ahora: inicio.addingTimeInterval(80),
            repositorio: repo)

        XCTAssertEqual(reparada?.frameActualId, frames[1])
        XCTAssertEqual(reparada?.terminados, 1)
        XCTAssertEqual(repo.cargar(
            documento: 123, ahora: inicio.addingTimeInterval(81))?.frameActualId,
            frames[1])
    }
}
