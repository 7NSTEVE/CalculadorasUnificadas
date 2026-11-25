import XCTest
@testable import CuadernUEstudio

final class SimulacroAdaptativoIPadTests: XCTestCase {
    func testPriorizaIntentoActivoYRepasoVencido() {
        let ahora = Date(timeIntervalSince1970: 100_000)
        var activo = EstadoEstudioFrameIPad(objetivo: "Activo")
        _ = activo.iniciar(ahora: ahora)
        var vencido = EstadoEstudioFrameIPad(objetivo: "Vencido", estado: .repetir)
        vencido.proximoRepaso = ahora.addingTimeInterval(-60)
        let pendiente = EstadoEstudioFrameIPad(objetivo: "Nuevo")

        let candidatos = [
            candidato("Nuevo", pendiente),
            candidato("Vencido", vencido),
            candidato("Activo", activo),
        ]
        let plan = PlanificadorSimulacroFramesIPad.construir(
            candidatos: candidatos, cantidadSolicitada: 3,
            duracion: .normal, ahora: ahora)

        XCTAssertEqual(plan.frameIds, [candidatos[2].frameId,
                                       candidatos[1].frameId,
                                       candidatos[0].frameId])
    }

    func testNoDuplicaElMismoProblemaCanonico() {
        let problema = UUID()
        let primero = CandidatoSimulacroFrameIPad(
            frameId: UUID(), problemaId: problema, titulo: "A",
            estudio: EstadoEstudioFrameIPad(objetivo: "A"))
        let segundo = CandidatoSimulacroFrameIPad(
            frameId: UUID(), problemaId: problema, titulo: "B",
            estudio: EstadoEstudioFrameIPad(objetivo: "B"))

        let plan = PlanificadorSimulacroFramesIPad.construir(
            candidatos: [primero, segundo], cantidadSolicitada: 10,
            duracion: .rapido, ahora: Date(timeIntervalSince1970: 1))

        XCTAssertEqual(plan.cantidad, 1)
        XCTAssertEqual(plan.frameIds.first, primero.frameId)
    }

    func testCantidadSeLimitaALoDisponible() {
        let candidatos = (0..<3).map {
            candidato("E\($0)", EstadoEstudioFrameIPad(objetivo: "E\($0)"))
        }
        let plan = PlanificadorSimulacroFramesIPad.construir(
            candidatos: candidatos, cantidadSolicitada: 15,
            duracion: .largo, ahora: Date(timeIntervalSince1970: 2))

        XCTAssertEqual(plan.cantidad, 3)
    }

    func testCompletarAvanzaYSoloCuentaUnaVez() {
        let ids = [UUID(), UUID()]
        let plan = PlanSimulacroFramesIPad(
            creado: Date(timeIntervalSince1970: 0), duracion: .normal,
            frameIds: ids)
        var sesion = EstadoSesionSimulacroIPad(
            plan: plan, inicio: Date(timeIntervalSince1970: 10))

        XCTAssertEqual(sesion.frameActualId, ids[0])
        XCTAssertEqual(sesion.completarActual(ahora: Date(timeIntervalSince1970: 20)), ids[0])
        XCTAssertEqual(sesion.frameActualId, ids[1])
        XCTAssertEqual(sesion.terminados, 1)
    }

    func testOmitirNoSeCuentaComoCompletado() {
        let ids = [UUID(), UUID()]
        let plan = PlanSimulacroFramesIPad(
            creado: Date(timeIntervalSince1970: 0), duracion: .normal,
            frameIds: ids)
        var sesion = EstadoSesionSimulacroIPad(
            plan: plan, inicio: Date(timeIntervalSince1970: 10))

        XCTAssertEqual(sesion.omitirActual(ahora: Date(timeIntervalSince1970: 15)), ids[0])
        XCTAssertEqual(sesion.terminados, 0)
        XCTAssertEqual(sesion.frameActualId, ids[1])
        XCTAssertEqual(sesion.omitidosTotal, 1)
    }

    func testTiempoAgotadoFinalizaAlAvanzar() {
        let ids = [UUID(), UUID()]
        let plan = PlanSimulacroFramesIPad(
            creado: Date(timeIntervalSince1970: 0), duracion: .rapido,
            frameIds: ids)
        let inicio = Date(timeIntervalSince1970: 100)
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)

        _ = sesion.completarActual(ahora: inicio.addingTimeInterval(601))

        XCTAssertNil(sesion.frameActualId)
        XCTAssertTrue(sesion.agotado(ahora: inicio.addingTimeInterval(601)))
    }

    func testResumenCalculaPorcentajeCompletado() {
        let resumen = ResumenSimulacroIPad(
            total: 5, completados: 3, omitidos: 1, segundosUsados: 420)
        XCTAssertEqual(resumen.porcentajeCompletado, 0.6, accuracy: 0.001)
    }

    func testRepositorioRecuperaMismoEjercicioYProgreso() throws {
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: carpeta) }
        let repo = RepositorioSimulacroDocumentoIPad(base: carpeta)
        let ids = [UUID(), UUID(), UUID()]
        let inicio = Date(timeIntervalSince1970: 500)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .normal, frameIds: ids)
        var sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)
        _ = sesion.completarActual(ahora: inicio.addingTimeInterval(10))

        XCTAssertTrue(repo.guardar(sesion, documento: 77))
        let recuperada = repo.cargar(
            documento: 77, ahora: inicio.addingTimeInterval(20))

        XCTAssertEqual(recuperada?.frameActualId, ids[1])
        XCTAssertEqual(recuperada?.terminados, 1)
        XCTAssertEqual(recuperada?.plan.frameIds, ids)
    }

    func testRepositorioCierraSesionSiElTiempoVencioMientrasAppNoEstabaActiva() {
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: carpeta) }
        let repo = RepositorioSimulacroDocumentoIPad(base: carpeta)
        let inicio = Date(timeIntervalSince1970: 900)
        let plan = PlanSimulacroFramesIPad(
            creado: inicio, duracion: .rapido, frameIds: [UUID(), UUID()])
        let sesion = EstadoSesionSimulacroIPad(plan: plan, inicio: inicio)
        XCTAssertTrue(repo.guardar(sesion, documento: 88))

        let recuperada = repo.cargar(
            documento: 88, ahora: inicio.addingTimeInterval(700))

        XCTAssertNil(recuperada?.frameActualId)
        XCTAssertTrue(recuperada?.agotado(ahora: inicio.addingTimeInterval(700)) == true)
    }

    func testRepositorioEliminarDescartaSesion() {
        let carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: carpeta) }
        let repo = RepositorioSimulacroDocumentoIPad(base: carpeta)
        let plan = PlanSimulacroFramesIPad(
            creado: Date(), duracion: .rapido, frameIds: [UUID()])
        XCTAssertTrue(repo.guardar(EstadoSesionSimulacroIPad(plan: plan), documento: 99))

        XCTAssertTrue(repo.eliminar(documento: 99))
        XCTAssertNil(repo.cargar(documento: 99))
    }

    private func candidato(
        _ titulo: String,
        _ estudio: EstadoEstudioFrameIPad
    ) -> CandidatoSimulacroFrameIPad {
        CandidatoSimulacroFrameIPad(
            frameId: UUID(), problemaId: nil, titulo: titulo, estudio: estudio)
    }
}
