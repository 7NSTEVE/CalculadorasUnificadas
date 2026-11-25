import XCTest

final class SincronizacionComparativaPencilIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-SyncPencil-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    private func sesion(
        hz: Int = 120,
        thermal: String = "nominal",
        finalizada: Bool = true,
        id: UUID = UUID()
    ) -> SesionQAPencilFisicoIPad {
        let inicio = Date().addingTimeInterval(-12)
        let snapshot = SnapshotQAPencilFisicoIPad(
            fecha: inicio.addingTimeInterval(10),
            p50GPUms: 8,
            p95GPUms: 14,
            p99GPUms: 21,
            maximaGPUms: 28,
            framesDiferidos: 1,
            porcentajeFramesPerdidos: 0.4,
            refrescoHz: hz,
            estadoTermico: thermal)
        return SesionQAPencilFisicoIPad(
            id: id,
            documentoId: 42,
            dispositivo: "iPad16,6",
            sistema: "iPadOS 27.0",
            inicio: inicio,
            fin: finalizada ? Date() : nil,
            snapshots: [snapshot],
            actualizacionesRenderer: 260,
            ticksDisplay: 900,
            framesPerdidosEstimados: 3,
            notas: [])
    }

    func testMismaSesionSincronizadaDosVecesNoDuplicaP95NiP99() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let fija = sesion(id: UUID())

        _ = SincronizadorComparativaPencilIPad.sincronizar(
            sesiones: [fija], repositorio: repo)
        _ = SincronizadorComparativaPencilIPad.sincronizar(
            sesiones: [fija], repositorio: repo)

        let archivo = repo.snapshot()
        XCTAssertEqual(archivo.rendimiento.count, 2)
        XCTAssertEqual(archivo.rendimiento.reduce(0) {
            $0 + $1.cuadernU.count
        }, 2)
        XCTAssertEqual(archivo.provenienciaMuestras?.count, 2)
        XCTAssertTrue(archivo.rendimiento.allSatisfy { $0.goodnotes.isEmpty })
    }

    func testThermalSeriousNoEntraAlCorpusComparativo() {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let resultado = SincronizadorComparativaPencilIPad.sincronizar(
            sesiones: [sesion(thermal: "serious")], repositorio: repo)

        XCTAssertTrue(repo.snapshot().rendimiento.isEmpty)
        XCTAssertEqual(resultado.errores.count, 2)
        XCTAssertTrue(resultado.errores.allSatisfy {
            $0.contains("thermal serious")
        })
    }

    func testSesentaHzNoSeAceptaComoProtocoloPencil120Hz() {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let resultado = SincronizadorComparativaPencilIPad.sincronizar(
            sesiones: [sesion(hz: 60)], repositorio: repo)

        XCTAssertTrue(repo.snapshot().rendimiento.isEmpty)
        XCTAssertEqual(resultado.errores.count, 2)
        XCTAssertTrue(resultado.errores.allSatisfy { $0.contains("60 Hz") })
    }

    func testSesionNoFinalizadaSeOmiteSinCrearEvidencia() {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let resultado = SincronizadorComparativaPencilIPad.sincronizar(
            sesiones: [sesion(finalizada: false)], repositorio: repo)

        XCTAssertEqual(resultado.sesionesEvaluadas, 0)
        XCTAssertEqual(resultado.sesionesOmitidas, 1)
        XCTAssertTrue(resultado.errores.isEmpty)
        XCTAssertTrue(repo.snapshot().rendimiento.isEmpty)
    }

    func testJSONVersionUnoSinCampoProvenienciaSigueImportando() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let json = """
        {
          "version": 1,
          "politica": {
            "minimoMuestrasPorProducto": 5,
            "ventajaRendimientoMinimaPorcentaje": 5,
            "regresionRendimientoMaximaPorcentaje": 2,
            "minimoMetricasRendimiento": 3,
            "minimoMetricasConVentaja": 1,
            "minimoFuncionesGoodnotesComparadas": 20,
            "minimoDiferenciadoresVerificados": 2,
            "antiguedadMaximaDias": 90
          },
          "rendimiento": [],
          "funciones": [],
          "actualizado": "2026-08-18T12:00:00Z"
        }
        """
        try repo.importar(Data(json.utf8))
        XCTAssertNil(repo.snapshot().provenienciaMuestras)
    }
}
