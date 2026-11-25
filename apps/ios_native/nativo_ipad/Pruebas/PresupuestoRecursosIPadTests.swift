import XCTest
@testable import CuadernUEstudio

private final class RecursoSuspendiblePruebaIPad: RecursoSuspendibleDocumentoIPad {
    let documentoPersistenteId: DocumentoPersistenteID
    var bytesResidentesEstimados: Int64
    var bytesLiberablesEstimados: Int64
    var tieneCambiosSinGuardar: Bool
    var fijadoEnMemoria: Bool
    var ultimoUso: Date
    var persistencias = 0
    var liberaciones = 0
    var fallarPersistencia = false

    init(id: DocumentoPersistenteID = .init(), residentes: Int64,
         liberables: Int64, sucio: Bool = false, fijado: Bool = false,
         ultimoUso: Date = Date()) {
        documentoPersistenteId = id; bytesResidentesEstimados = residentes
        bytesLiberablesEstimados = liberables; tieneCambiosSinGuardar = sucio
        fijadoEnMemoria = fijado; self.ultimoUso = ultimoUso
    }
    func persistirAntesDeSuspender() throws {
        if fallarPersistencia { throw CocoaError(.fileWriteUnknown) }
        persistencias += 1; tieneCambiosSinGuardar = false
    }
    func liberarRecursosPesados() { liberaciones += 1; bytesResidentesEstimados -= bytesLiberablesEstimados }
}

final class PresupuestoRecursosIPadTests: XCTestCase {
    private var presupuesto: PresupuestoMemoriaCuadernUIPad {
        .init(porDocumento: 200, global: 500, reserva: 100)
    }

    func testLRUSuspendePrimeroElMasAntiguo() {
        let centro = CentroPresupuestoRecursosIPad(presupuesto: presupuesto, observarMemoria: false)
        let viejo = RecursoSuspendiblePruebaIPad(residentes: 250, liberables: 200,
                                                ultimoUso: Date(timeIntervalSince1970: 1))
        let nuevo = RecursoSuspendiblePruebaIPad(residentes: 250, liberables: 200,
                                                ultimoUso: Date(timeIntervalSince1970: 2))
        centro.registrar(viejo); centro.registrar(nuevo)
        let reporte = centro.liberar(objetivo: 150)
        XCTAssertEqual(reporte.documentosSuspendidos, [viejo.documentoPersistenteId])
        XCTAssertEqual(viejo.liberaciones, 1); XCTAssertEqual(nuevo.liberaciones, 0)
    }

    func testPersisteSucioAntesDeLiberar() {
        let centro = CentroPresupuestoRecursosIPad(presupuesto: presupuesto, observarMemoria: false)
        let recurso = RecursoSuspendiblePruebaIPad(residentes: 300, liberables: 220, sucio: true)
        centro.registrar(recurso)
        _ = centro.liberar(objetivo: 100)
        XCTAssertEqual(recurso.persistencias, 1)
        XCTAssertEqual(recurso.liberaciones, 1)
    }

    func testNoLiberaSiGuardarFalla() {
        let centro = CentroPresupuestoRecursosIPad(presupuesto: presupuesto, observarMemoria: false)
        let recurso = RecursoSuspendiblePruebaIPad(residentes: 300, liberables: 220, sucio: true)
        recurso.fallarPersistencia = true; centro.registrar(recurso)
        let reporte = centro.liberar(objetivo: 100)
        XCTAssertTrue(reporte.documentosSuspendidos.isEmpty)
        XCTAssertEqual(reporte.documentosOmitidosPorError, [recurso.documentoPersistenteId])
        XCTAssertEqual(recurso.liberaciones, 0)
    }

    func testProtegidoYFijadoNoSeSuspenden() {
        let centro = CentroPresupuestoRecursosIPad(presupuesto: presupuesto, observarMemoria: false)
        let protegido = RecursoSuspendiblePruebaIPad(residentes: 250, liberables: 200)
        let fijado = RecursoSuspendiblePruebaIPad(residentes: 250, liberables: 200, fijado: true)
        centro.registrar(protegido); centro.registrar(fijado)
        let reporte = centro.liberar(objetivo: 300, proteger: [protegido.documentoPersistenteId])
        XCTAssertTrue(reporte.documentosSuspendidos.isEmpty)
    }

    func testAsegurarCapacidadSoloLiberaExceso() {
        let centro = CentroPresupuestoRecursosIPad(presupuesto: presupuesto, observarMemoria: false)
        let r = RecursoSuspendiblePruebaIPad(residentes: 450, liberables: 200)
        centro.registrar(r)
        XCTAssertEqual(centro.asegurarCapacidad(bytesAdicionales: 25).solicitados, 0)
        let reporte = centro.asegurarCapacidad(bytesAdicionales: 100)
        XCTAssertEqual(reporte.solicitados, 50)
        XCTAssertEqual(r.liberaciones, 1)
    }

    func testPresupuestoRecomendadoMantieneLimites() {
        let p = PresupuestoMemoriaCuadernUIPad.recomendado(memoriaFisica: 8 * 1_024 * 1_024 * 1_024)
        XCTAssertLessThanOrEqual(p.porDocumento, 512 * 1_024 * 1_024)
        XCTAssertLessThanOrEqual(p.global, 2 * 1_024 * 1_024 * 1_024)
        XCTAssertGreaterThan(p.reserva, 0)
    }
}
