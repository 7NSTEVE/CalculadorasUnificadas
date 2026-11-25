import XCTest

final class RepositorioComparativaGoodnotesIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-ComparativaGoodnotes-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testMismoEscenarioAcumulaAmbosProductosSinFabricarMuestras() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        _ = try repo.registrarMuestra(
            producto: .cuadernU,
            metrica: "reopen_ms",
            escenario: "500 paginas PDF",
            dispositivo: "iPad16,6",
            sistema: "iPadOS 27.0",
            direccion: .menorEsMejor,
            valor: 420)
        _ = try repo.registrarMuestra(
            producto: .goodnotes,
            metrica: "reopen_ms",
            escenario: "500 paginas PDF",
            dispositivo: "iPad16,6",
            sistema: "iPadOS 27.0",
            direccion: .menorEsMejor,
            valor: 510)

        let snapshot = repo.snapshot()
        XCTAssertEqual(snapshot.rendimiento.count, 1)
        XCTAssertEqual(snapshot.rendimiento[0].cuadernU, [420])
        XCTAssertEqual(snapshot.rendimiento[0].goodnotes, [510])
        XCTAssertEqual(repo.resumen().muestrasCuadernU, 1)
        XCTAssertEqual(repo.resumen().muestrasGoodnotes, 1)
        XCTAssertEqual(repo.resumen().seriesComparables, 0,
                       "Una muestra por producto no debe pasar el mínimo del gate")
    }

    func testHardwareOSOEscenarioDistintoNoSeMezclan() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        for (escenario, dispositivo) in [
            ("100 paginas", "iPad16,6"),
            ("500 paginas", "iPad16,6"),
            ("100 paginas", "iPad14,3"),
        ] {
            _ = try repo.registrarMuestra(
                producto: .cuadernU,
                metrica: "scroll_jank_pct",
                escenario: escenario,
                dispositivo: dispositivo,
                sistema: "iPadOS 27.0",
                direccion: .menorEsMejor,
                valor: 1)
        }
        XCTAssertEqual(repo.snapshot().rendimiento.count, 3)
    }

    func testCambiarDireccionDeMismaSerieSeRechaza() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        _ = try repo.registrarMuestra(
            producto: .cuadernU,
            metrica: "export_mb_s",
            escenario: "PDF 300 paginas",
            dispositivo: "iPad",
            sistema: "iPadOS",
            direccion: .mayorEsMejor,
            valor: 18)
        XCTAssertThrowsError(try repo.registrarMuestra(
            producto: .goodnotes,
            metrica: "export_mb_s",
            escenario: "PDF 300 paginas",
            dispositivo: "iPad",
            sistema: "iPadOS",
            direccion: .menorEsMejor,
            valor: 20))
        XCTAssertEqual(repo.snapshot().rendimiento.first?.goodnotes, [])
    }

    func testImportarInvalidoNoReemplazaEstadoBueno() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        _ = try repo.registrarMuestra(
            producto: .cuadernU,
            metrica: "latencia",
            escenario: "pencil",
            dispositivo: "iPad",
            sistema: "iPadOS",
            direccion: .menorEsMejor,
            valor: 10)
        let antes = repo.snapshot()

        var corrupto = antes
        corrupto.version = 99
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        let datos = try encoder.encode(corrupto)
        XCTAssertThrowsError(try repo.importar(datos))
        XCTAssertEqual(repo.snapshot(), antes)
    }

    func testPersistenciaRoundTripConFuncionYPolitica() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        var politica = PoliticaSuperioridadGoodnotesIPad()
        politica.minimoMuestrasPorProducto = 7
        repo.politica = politica
        try repo.registrarFuncion(EvidenciaFuncionComparativaIPad(
            id: "anchors-handwriting",
            nombre: "Anclas sobre handwriting",
            categoria: "Links",
            disponibleEnGoodnotes: false,
            estadoCuadernU: .verificada,
            diferenciadorCuadernU: true,
            evidencia: "QA e2e en mismo dispositivo"))

        let recargado = RepositorioComparativaGoodnotesIPad(base: carpeta)
        XCTAssertEqual(recargado.politica.minimoMuestrasPorProducto, 7)
        XCTAssertEqual(recargado.snapshot().funciones.count, 1)
        XCTAssertEqual(recargado.snapshot().funciones[0].id, "anchors-handwriting")
    }

    func testGateSoloSuperaConCoberturaVentajaYDosDiferenciadores() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        var politica = PoliticaSuperioridadGoodnotesIPad()
        politica.minimoMuestrasPorProducto = 2
        politica.minimoMetricasRendimiento = 1
        politica.minimoMetricasConVentaja = 1
        politica.minimoFuncionesGoodnotesComparadas = 1
        politica.minimoDiferenciadoresVerificados = 2
        repo.politica = politica

        for valor in [8.0, 9.0] {
            _ = try repo.registrarMuestra(
                producto: .cuadernU, metrica: "p95_ms",
                escenario: "Pencil", dispositivo: "iPad",
                sistema: "iPadOS", direccion: .menorEsMejor,
                valor: valor)
        }
        for valor in [12.0, 13.0] {
            _ = try repo.registrarMuestra(
                producto: .goodnotes, metrica: "p95_ms",
                escenario: "Pencil", dispositivo: "iPad",
                sistema: "iPadOS", direccion: .menorEsMejor,
                valor: valor)
        }
        try repo.registrarFuncion(.init(
            id: "lasso", nombre: "Lasso", categoria: "Ink",
            disponibleEnGoodnotes: true,
            estadoCuadernU: .verificada,
            diferenciadorCuadernU: false,
            evidencia: "QA"))
        try repo.registrarFuncion(.init(
            id: "anchors", nombre: "Anclas directas", categoria: "Links",
            disponibleEnGoodnotes: false,
            estadoCuadernU: .verificada,
            diferenciadorCuadernU: true,
            evidencia: "QA"))

        XCTAssertEqual(repo.reporte().veredicto, .evidenciaInsuficiente)

        try repo.registrarFuncion(.init(
            id: "engineering-solve", nombre: "Solve Mode", categoria: "Engineering",
            disponibleEnGoodnotes: false,
            estadoCuadernU: .verificada,
            diferenciadorCuadernU: true,
            evidencia: "QA"))
        XCTAssertEqual(repo.reporte().veredicto, .supera)
    }

    func testValoresNoFinitosONegativosSeRechazan() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        for valor in [Double.nan, Double.infinity, -0.1] {
            XCTAssertThrowsError(try repo.registrarMuestra(
                producto: .cuadernU,
                metrica: "m", escenario: "e", dispositivo: "d", sistema: "s",
                direccion: .menorEsMejor, valor: valor))
        }
        XCTAssertTrue(repo.snapshot().rendimiento.isEmpty)
    }
}
