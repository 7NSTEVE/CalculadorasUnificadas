import XCTest
@testable import CuadernUEstudio

final class RevisionSolucionPostIntentoIPadTests: XCTestCase {
    func testBloqueaSolucionMientrasHayIntentoActivo() {
        var estudio = EstadoEstudioFrameIPad(
            objetivo: "Resolver",
            problemaId: UUID())
        _ = estudio.iniciar(ahora: Date(timeIntervalSince1970: 10))
        let problema = problemaConSolucion(id: estudio.problemaId!)

        XCTAssertEqual(
            PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio, problema: problema),
            .intentoActivo)
    }

    func testNoPermiteSolucionAntesDelPrimerIntentoTerminado() {
        let id = UUID()
        let estudio = EstadoEstudioFrameIPad(objetivo: "Resolver", problemaId: id)

        XCTAssertEqual(
            PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio, problema: problemaConSolucion(id: id)),
            .sinIntentoTerminado)
    }

    func testPermiteSolucionDespuesDeCerrarIntento() {
        let id = UUID()
        var estudio = EstadoEstudioFrameIPad(objetivo: "Resolver", problemaId: id)
        let intento = estudio.iniciar(ahora: Date(timeIntervalSince1970: 10))
        XCTAssertTrue(estudio.cerrar(
            intentoId: intento,
            resultado: .repetir,
            confianza: 0.8,
            error: "signo",
            ahora: Date(timeIntervalSince1970: 30)))

        XCTAssertEqual(
            PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio, problema: problemaConSolucion(id: id)),
            .permitido(intentoId: intento))
    }

    func testSolucionVaciaNoCuentaComoDisponible() {
        XCTAssertFalse(
            PoliticaAccesoSolucionPostIntentoIPad.solucionDisponible(
                SolucionVinculadaIPad()))
        XCTAssertTrue(
            PoliticaAccesoSolucionPostIntentoIPad.solucionDisponible(
                SolucionVinculadaIPad(texto: "  x = 4  ")))
    }

    func testRepositorioActualizaMismaRevisionSinDuplicarla() throws {
        let temporal = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: temporal) }
        let repo = RepositorioRevisionesSolucionIPad(base: temporal)
        let documento: UInt64 = 99
        let frame = UUID()
        let problema = UUID()
        let intento = UUID()
        var revision = RevisionSolucionPostIntentoIPad(
            documentoId: documento,
            frameId: frame,
            problemaId: problema,
            intentoId: intento,
            resultado: .diferencia,
            paso: "  cambié el signo  ",
            correccion: " usar convención pasiva ")

        XCTAssertTrue(repo.guardar(revision))
        revision.nota = " revisar KVL "
        XCTAssertTrue(repo.guardar(revision))

        let filas = repo.revisiones(documento: documento)
        XCTAssertEqual(filas.count, 1)
        XCTAssertEqual(filas.first?.paso, "cambié el signo")
        XCTAssertEqual(filas.first?.correccion, "usar convención pasiva")
        XCTAssertEqual(filas.first?.nota, "revisar KVL")
    }

    func testMarcadorDeErrorEsDeterministaPorIntento() {
        let intento = UUID()
        XCTAssertEqual(
            ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: intento),
            ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: intento))
        XCTAssertNotEqual(
            ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: intento),
            ServicioRevisionSolucionPostIntentoIPad.marcador(intentoId: UUID()))
    }

    private func problemaConSolucion(id: UUID) -> ProblemaBancoIPad {
        ProblemaBancoIPad(
            id: id,
            enunciado: "Calcule la corriente",
            solucion: SolucionVinculadaIPad(texto: "I = 2 A"))
    }
}
