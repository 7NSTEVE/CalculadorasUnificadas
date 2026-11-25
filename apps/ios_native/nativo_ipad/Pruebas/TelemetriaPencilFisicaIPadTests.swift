import XCTest

final class TelemetriaPencilFisicaIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-QAPencil-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    private func documento(_ literal: String) -> DocumentoPersistenteID {
        DocumentoPersistenteID(UUID(uuidString: literal)!)
    }

    func testBenchmarkExigeRenovarVentanaDe240YDuracionMinima() {
        let ahora = Date()
        let muestra = SnapshotQAPencilFisicoIPad(
            p50GPUms: 7, p95GPUms: 13, p99GPUms: 18,
            maximaGPUms: 24, framesDiferidos: 0,
            porcentajeFramesPerdidos: 0.2, refrescoHz: 120,
            estadoTermico: "nominal")

        var insuficiente = SesionQAPencilFisicoIPad(
            documentoId: documento("00000000-0000-0000-0000-000000000007"),
            dispositivo: "iPad-test", sistema: "iPadOS test",
            inicio: ahora.addingTimeInterval(-5), fin: ahora,
            snapshots: [muestra], actualizacionesRenderer: 239)
        XCTAssertFalse(insuficiente.aptaParaBenchmark)

        insuficiente.actualizacionesRenderer = 240
        XCTAssertTrue(insuficiente.aptaParaBenchmark)

        insuficiente.inicio = ahora.addingTimeInterval(-2)
        XCTAssertFalse(insuficiente.aptaParaBenchmark)
    }

    func testRepositorioPodaSesionesYFiltraPorDocumentoDurable() {
        let repo = RepositorioQAPencilFisicoIPad(base: carpeta)
        repo.maximoSesiones = 2
        let docA = documento("00000000-0000-0000-0000-000000000001")
        let docB = documento("00000000-0000-0000-0000-000000000002")

        var primera = SesionQAPencilFisicoIPad(
            documentoId: docA, dispositivo: "A", sistema: "S")
        primera.inicio = Date(timeIntervalSince1970: 1)
        var segunda = SesionQAPencilFisicoIPad(
            documentoId: docB, dispositivo: "B", sistema: "S")
        segunda.inicio = Date(timeIntervalSince1970: 2)
        var tercera = SesionQAPencilFisicoIPad(
            documentoId: docA, dispositivo: "C", sistema: "S")
        tercera.inicio = Date(timeIntervalSince1970: 3)

        repo.guardar(primera)
        repo.guardar(segunda)
        repo.guardar(tercera)

        XCTAssertEqual(repo.sesiones().map(\.id), [tercera.id, segunda.id])
        XCTAssertEqual(repo.sesiones(documento: docA).map(\.id), [tercera.id])
        XCTAssertTrue(repo.sesiones(documento: documento(
            "00000000-0000-0000-0000-000000000099")).isEmpty)
    }

    func testLeeSesionLegacySinInventarUUID() throws {
        let archivo = carpeta.appendingPathComponent("sesiones-pencil-fisico.json")
        let json = """
        {
          "version": 1,
          "sesiones": [
            {
              "id": "00000000-0000-0000-0000-000000000123",
              "documentoId": 123,
              "dispositivo": "iPad-legacy",
              "sistema": "iPadOS legacy",
              "inicio": "2026-08-18T12:00:00Z",
              "snapshots": [],
              "actualizacionesRenderer": 0,
              "ticksDisplay": 0,
              "framesPerdidosEstimados": 0,
              "notas": []
            }
          ]
        }
        """
        try Data(json.utf8).write(to: archivo, options: .atomic)

        let repo = RepositorioQAPencilFisicoIPad(base: carpeta)
        let legacy = repo.sesiones(claveLegacyDocumento: 123)
        XCTAssertEqual(legacy.count, 1)
        XCTAssertNil(legacy.first?.documentoId)
        XCTAssertEqual(legacy.first?.documentoClaveLegacy, 123)
        XCTAssertTrue(repo.sesiones(documento: documento(
            "00000000-0000-0000-0000-000000000123")).isEmpty)
    }

    func testSesionNuevaPersisteUUIDCompleto() throws {
        let doc = documento("9F38E776-7D4A-4D64-AEE8-0B13087FCB36")
        let repo = RepositorioQAPencilFisicoIPad(base: carpeta)
        let sesion = SesionQAPencilFisicoIPad(
            documentoId: doc, dispositivo: "iPad16,3", sistema: "iPadOS 27.0")
        repo.guardar(sesion)

        let reabierto = RepositorioQAPencilFisicoIPad(base: carpeta)
        let cargadas = reabierto.sesiones(documento: doc)
        XCTAssertEqual(cargadas.map(\.id), [sesion.id])
        XCTAssertNil(cargadas.first?.documentoClaveLegacy)
    }

    func testBenchmarkReconoceNuevasMetricasPencilComoMenorEsMejor() {
        XCTAssertTrue(MetricaRendimientoCuadernUIPad.latenciaPencilP95Ms.menorEsMejor)
        XCTAssertTrue(MetricaRendimientoCuadernUIPad.latenciaPencilP99Ms.menorEsMejor)
        XCTAssertTrue(MetricaRendimientoCuadernUIPad.framesPerdidosPorcentaje.menorEsMejor)
    }
}
