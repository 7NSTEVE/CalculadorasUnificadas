import XCTest

final class IndiceReconocimientoManuscritoIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-OCRManuscrito-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    private func trazo(_ sesion: Sesion, x: Float, y: Float) -> String? {
        sesion.comenzarTrazo(ancho: 3, color: 0xFF101010, resaltador: false)
        sesion.anadirMuestra(
            x: x, y: y, presion: 0.6,
            altitud: 1, azimut: 0, tiempo: Double(x + y) / 100)
        sesion.anadirMuestra(
            x: x + 35, y: y + 8, presion: 0.7,
            altitud: 1, azimut: 0, tiempo: Double(x + y) / 100 + 0.01)
        return sesion.terminarTrazo()
    }

    func testPlanificadorAgrupaMismaLineaYSeparaRegionLejana() throws {
        let sesion = try XCTUnwrap(Sesion())
        XCTAssertNotNil(trazo(sesion, x: 20, y: 40))
        XCTAssertNotNil(trazo(sesion, x: 72, y: 42))
        XCTAssertNotNil(trazo(sesion, x: 22, y: 520))

        let regiones = PlanificadorOCRManuscritoIPad.regiones(sesion: sesion)
        XCTAssertEqual(regiones.count, 2)
        XCTAssertEqual(regiones.map { $0.idsTrazos.count }.sorted(), [1, 2])
        XCTAssertTrue(regiones.allSatisfy { $0.fuenteId.count == 64 })
    }

    func testFuenteIdNoDependeDelOrdenDeIds() {
        let a = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 7, idsTrazos: ["c", "a", "b"])
        let b = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 7, idsTrazos: ["b", "c", "a"])
        let otroDocumento = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 8, idsTrazos: ["a", "b", "c"])
        XCTAssertEqual(a, b)
        XCTAssertNotEqual(a, otroDocumento)
    }

    func testBusquedaCplusplusEsInsensibleAAcentosYNye() throws {
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        let id = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 11, idsTrazos: ["t1", "t2"])
        try repo.reemplazarDocumento(11, con: [
            RegistroOCRManuscritoIPad(
                fuenteId: id,
                documentoId: 11,
                idsTrazos: ["t1", "t2"],
                marco: CGRect(x: 100, y: 220, width: 180, height: 48),
                texto: "Tensión de Thévenin y señal española",
                confianza: 0.91,
                idiomas: ["es-ES", "en-US"]),
        ])

        let tension = repo.buscar("tension")
        XCTAssertEqual(tension.count, 1)
        XCTAssertEqual(tension.first?.documentoId, 11)
        XCTAssertTrue(tension.first?.fragmento.contains("Tensión") == true)

        let thevenin = repo.buscar("thevenin")
        XCTAssertEqual(thevenin.count, 1)

        let espanola = repo.buscar("espanola")
        XCTAssertEqual(espanola.count, 1)
        XCTAssertEqual(espanola.first?.marco.minY, 220, accuracy: 0.001)
    }

    func testReemplazarDocumentoEliminaOCRStaleSinTocarOtroDocumento() throws {
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        func registro(_ documento: UInt64, _ idTrazo: String,
                      _ texto: String) -> RegistroOCRManuscritoIPad {
            RegistroOCRManuscritoIPad(
                fuenteId: RepositorioOCRManuscritoIPad.fuenteId(
                    documentoId: documento, idsTrazos: [idTrazo]),
                documentoId: documento,
                idsTrazos: [idTrazo],
                marco: CGRect(x: 10, y: 20, width: 100, height: 40),
                texto: texto,
                confianza: 0.8,
                idiomas: ["es-ES"])
        }
        try repo.reemplazarDocumento(1, con: [registro(1, "a", "resistencia")])
        try repo.reemplazarDocumento(2, con: [registro(2, "b", "capacitancia")])
        XCTAssertEqual(repo.buscar("resistencia").count, 1)
        XCTAssertEqual(repo.buscar("capacitancia").count, 1)

        try repo.reemplazarDocumento(1, con: [registro(1, "c", "inductancia")])
        XCTAssertTrue(repo.buscar("resistencia").isEmpty)
        XCTAssertEqual(repo.buscar("inductancia").count, 1)
        XCTAssertEqual(repo.buscar("capacitancia").count, 1)
    }

    func testJSONCorruptoSeDescartaComoIndiceReconstruible() throws {
        let url = carpeta.appendingPathComponent("indice-v1.json")
        try Data("{esto-no-es-json".utf8).write(to: url)
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        XCTAssertTrue(repo.registros().isEmpty)
        XCTAssertTrue(repo.buscar("cualquier cosa").isEmpty)
    }
}
