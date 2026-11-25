import XCTest
@testable import CuadernUEstudio

final class RevisionSolucionHistoricaIPadTests: XCTestCase {
    func testIntentoHistoricoSigueSiendoComparableTrasIntentoPosterior() {
        let problemaId = UUID()
        var estudio = EstadoEstudioFrameIPad(
            objetivo: "Resolver",
            problemaId: problemaId)
        let primero = estudio.iniciar(ahora: Date(timeIntervalSince1970: 10))
        XCTAssertTrue(estudio.cerrar(
            intentoId: primero,
            resultado: .repetir,
            confianza: 0.8,
            ahora: Date(timeIntervalSince1970: 20)))
        let segundo = estudio.iniciar(ahora: Date(timeIntervalSince1970: 30))
        XCTAssertTrue(estudio.cerrar(
            intentoId: segundo,
            resultado: .correcto,
            confianza: 0.9,
            ahora: Date(timeIntervalSince1970: 40)))

        let problema = ProblemaBancoIPad(
            id: problemaId,
            enunciado: "Resolver",
            solucion: SolucionVinculadaIPad(texto: "Solución"))

        XCTAssertEqual(
            PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio,
                problema: problema,
                intentoId: primero),
            .permitido(intentoId: primero))
    }

    func testIntentoActivoBloqueaInclusoRevisionHistorica() {
        let problemaId = UUID()
        var estudio = EstadoEstudioFrameIPad(objetivo: "Resolver", problemaId: problemaId)
        let cerrado = estudio.iniciar(ahora: Date(timeIntervalSince1970: 10))
        XCTAssertTrue(estudio.cerrar(
            intentoId: cerrado,
            resultado: .correcto,
            confianza: 0.8,
            ahora: Date(timeIntervalSince1970: 20)))
        _ = estudio.iniciar(ahora: Date(timeIntervalSince1970: 30))
        let problema = ProblemaBancoIPad(
            id: problemaId,
            enunciado: "Resolver",
            solucion: SolucionVinculadaIPad(texto: "Solución"))

        XCTAssertEqual(
            PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio,
                problema: problema,
                intentoId: cerrado),
            .intentoActivo)
    }

    func testRepositorioSimulacroInyectaDocumentoAlCargar() throws {
        let temporal = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: temporal) }
        let repo = RepositorioSimulacroDocumentoIPad(base: temporal)
        let frame = UUID()
        let plan = PlanSimulacroFramesIPad(
            creado: Date(timeIntervalSince1970: 1),
            duracion: .normal,
            frameIds: [frame])
        let sesion = EstadoSesionSimulacroIPad(
            plan: plan,
            inicio: Date(timeIntervalSince1970: 2))

        XCTAssertNil(sesion.documentoId)
        XCTAssertTrue(repo.guardar(sesion, documento: 812))
        XCTAssertEqual(repo.cargar(documento: 812)?.documentoId, 812)
    }

    func testDocumentoIdEsOpcionalEnJSONAnterior() throws {
        let frame = UUID()
        let plan = PlanSimulacroFramesIPad(
            creado: Date(timeIntervalSince1970: 1),
            duracion: .rapido,
            frameIds: [frame])
        let datosPlan = try JSONEncoder().encode(plan)
        let jsonPlan = try XCTUnwrap(String(data: datosPlan, encoding: .utf8))
        let json = """
        {
          "plan": \(jsonPlan),
          "inicio": 10,
          "indiceActual": 0,
          "completados": [],
          "omitidos": [],
          "intentosContabilizados": [],
          "intentoPorFrame": {}
        }
        """
        let decoder = JSONDecoder()
        let sesion = try decoder.decode(
            EstadoSesionSimulacroIPad.self,
            from: Data(json.utf8))
        XCTAssertNil(sesion.documentoId)
    }
}
