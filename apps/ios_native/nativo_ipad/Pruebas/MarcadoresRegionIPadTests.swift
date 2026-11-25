import XCTest

final class MarcadoresRegionIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-MarcadoresRegion-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testPersistenciaYAislamientoPorDocumento() throws {
        let repo = RepositorioMarcadoresRegionIPad(base: carpeta)
        let a = MarcadorRegionIPad(
            documentoId: 1, titulo: "Nodo A",
            x: 100, y: 200, zoom: 1.25)
        let b = MarcadorRegionIPad(
            documentoId: 2, titulo: "Bode",
            x: 800, y: 1200, zoom: 0.8)
        try repo.guardar(a)
        try repo.guardar(b)

        XCTAssertEqual(repo.todos().count, 2)
        XCTAssertEqual(repo.todos(documentoId: 1).map(\.id), [a.id])
        XCTAssertEqual(repo.todos(documentoId: 2).map(\.id), [b.id])

        let reabierto = RepositorioMarcadoresRegionIPad(base: carpeta)
        XCTAssertEqual(Set(reabierto.todos().map(\.id)), Set([a.id, b.id]))
    }

    func testCercanoNoCruzaDocumentos() throws {
        let repo = RepositorioMarcadoresRegionIPad(base: carpeta)
        let a = MarcadorRegionIPad(
            documentoId: 10, titulo: "A", x: 100, y: 100, zoom: 1)
        let b = MarcadorRegionIPad(
            documentoId: 20, titulo: "B", x: 102, y: 101, zoom: 1)
        try repo.guardar(a)
        try repo.guardar(b)

        XCTAssertEqual(
            repo.cercano(documentoId: 10, x: 101, y: 101, radio: 10)?.id,
            a.id)
        XCTAssertEqual(
            repo.cercano(documentoId: 20, x: 101, y: 101, radio: 10)?.id,
            b.id)
        XCTAssertNil(repo.cercano(
            documentoId: 30, x: 101, y: 101, radio: 10))
    }

    func testEliminarDocumentoNoTocaOtrosMarcadores() throws {
        let repo = RepositorioMarcadoresRegionIPad(base: carpeta)
        try repo.guardar(.init(
            documentoId: 1, titulo: "Uno", x: 1, y: 2, zoom: 1))
        try repo.guardar(.init(
            documentoId: 2, titulo: "Dos", x: 3, y: 4, zoom: 1))

        try repo.eliminarDocumento(1)
        XCTAssertTrue(repo.todos(documentoId: 1).isEmpty)
        XCTAssertEqual(repo.todos(documentoId: 2).count, 1)
    }

    func testRechazaZoomYCoordenadasNoFinitas() {
        let repo = RepositorioMarcadoresRegionIPad(base: carpeta)
        XCTAssertThrowsError(try repo.guardar(.init(
            documentoId: 1, titulo: "NaN",
            x: .nan, y: 0, zoom: 1)))
        XCTAssertThrowsError(try repo.guardar(.init(
            documentoId: 1, titulo: "Zoom",
            x: 0, y: 0, zoom: 0)))
        XCTAssertTrue(repo.todos().isEmpty)
    }

    func testDestinoConservaDocumentoCentroZoomYTitulo() {
        let marcador = MarcadorRegionIPad(
            documentoId: 55,
            titulo: "Resonancia serie",
            x: 320.25,
            y: 880.75,
            zoom: 1.6)
        let url = marcador.destino.enlace()
        let leido = DestinoEnlaceTinta.leer(url)

        XCTAssertEqual(leido?.documentoId, 55)
        XCTAssertEqual(leido?.x ?? 0, 320.25, accuracy: 0.0001)
        XCTAssertEqual(leido?.y ?? 0, 880.75, accuracy: 0.0001)
        XCTAssertEqual(leido?.zoom ?? 0, 1.6, accuracy: 0.0001)
        XCTAssertEqual(leido?.titulo, "Resonancia serie")
    }
}
