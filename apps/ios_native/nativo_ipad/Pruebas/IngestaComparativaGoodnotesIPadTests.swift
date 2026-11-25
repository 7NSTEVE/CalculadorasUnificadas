import XCTest

final class IngestaComparativaGoodnotesIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-IngestaGoodnotes-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    private func condiciones(
        hz: Int = 120,
        thermal: EstadoTermicoComparativaIPad = .nominal,
        pencil: Bool = true,
        sha: String? = nil
    ) -> CondicionesCorridaGoodnotesIPad {
        .init(
            dispositivo: "iPad16,6",
            sistema: "iPadOS 27.0",
            refrescoHz: hz,
            estadoTermico: thermal,
            pencilFisico: pencil,
            sha256Corpus: sha)
    }

    func testPencilRechaza60Hz() {
        XCTAssertThrowsError(try IngestaComparativaGoodnotesIPad.validar(
            escenario: CatalogoEscenariosGoodnotesIPad.pencilP95,
            condiciones: condiciones(hz: 60),
            valor: 12)) { error in
                XCTAssertEqual(
                    error as? ErrorIngestaComparativaGoodnotesIPad,
                    .refrescoInsuficiente(60))
            }
    }

    func testPencilExigeHardwareFisico() {
        XCTAssertThrowsError(try IngestaComparativaGoodnotesIPad.validar(
            escenario: CatalogoEscenariosGoodnotesIPad.pencilP99,
            condiciones: condiciones(pencil: false),
            valor: 14)) { error in
                XCTAssertEqual(
                    error as? ErrorIngestaComparativaGoodnotesIPad,
                    .faltaPencilFisico)
            }
    }

    func testThermalSeriousNoSeAceptaComoComparacion() {
        XCTAssertThrowsError(try IngestaComparativaGoodnotesIPad.validar(
            escenario: CatalogoEscenariosGoodnotesIPad.searchTenThousand,
            condiciones: condiciones(thermal: .serious),
            valor: 30))
    }

    func testEscenario500PaginasExigeSHA256() {
        XCTAssertThrowsError(try IngestaComparativaGoodnotesIPad.validar(
            escenario: CatalogoEscenariosGoodnotesIPad.huge500Reopen,
            condiciones: condiciones(sha: nil),
            valor: 800)) { error in
                XCTAssertEqual(
                    error as? ErrorIngestaComparativaGoodnotesIPad,
                    .faltaCorpus)
            }
        XCTAssertThrowsError(try IngestaComparativaGoodnotesIPad.validar(
            escenario: CatalogoEscenariosGoodnotesIPad.huge500Reopen,
            condiciones: condiciones(sha: "1234"),
            valor: 800))
    }

    func testRegistroCanonicoDerivaMetricaDireccionYCorpus() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let sha = String(repeating: "a", count: 64)
        let escenario = CatalogoEscenariosGoodnotesIPad.huge500Reopen
        let serie = try repo.registrarMuestraCanonica(
            producto: .cuadernU,
            escenarioIdVersionado: escenario.idVersionado,
            condiciones: condiciones(sha: sha),
            valor: 720)

        XCTAssertEqual(serie.metrica, escenario.metrica)
        XCTAssertEqual(serie.direccion, .menorEsMejor)
        XCTAssertTrue(serie.escenario.contains(escenario.idVersionado))
        XCTAssertTrue(serie.escenario.contains("corpus=aaaaaaaaaaaaaaaa"))
        XCTAssertEqual(serie.cuadernU, [720])
        XCTAssertTrue(serie.goodnotes.isEmpty)
    }

    func testMismoProtocoloConSHAODispositivoDistintoNoSeMezcla() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        let escenario = CatalogoEscenariosGoodnotesIPad.export500PDF
        let shaA = String(repeating: "a", count: 64)
        let shaB = String(repeating: "b", count: 64)

        _ = try repo.registrarMuestraCanonica(
            producto: .cuadernU,
            escenarioIdVersionado: escenario.idVersionado,
            condiciones: condiciones(sha: shaA),
            valor: 20)
        _ = try repo.registrarMuestraCanonica(
            producto: .cuadernU,
            escenarioIdVersionado: escenario.idVersionado,
            condiciones: condiciones(sha: shaB),
            valor: 21)

        XCTAssertEqual(repo.snapshot().rendimiento.count, 2)
    }

    func testEscenarioInventadoNoSeRegistra() throws {
        let repo = RepositorioComparativaGoodnotesIPad(base: carpeta)
        XCTAssertThrowsError(try repo.registrarMuestraCanonica(
            producto: .cuadernU,
            escenarioIdVersionado: "inventado@v1",
            condiciones: condiciones(),
            valor: 1))
        XCTAssertTrue(repo.snapshot().rendimiento.isEmpty)
    }
}
