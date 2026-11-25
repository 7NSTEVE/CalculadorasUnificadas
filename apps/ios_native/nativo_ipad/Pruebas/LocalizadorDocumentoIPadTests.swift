import XCTest

final class LocalizadorDocumentoIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-Localizador-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testLocalizaPestanaAbiertaEnPanelPrincipal() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let id = UUID()
        let registro = RegistroPestanaIPad(
            id: id, titulo: "Circuitos",
            colorARGB: 0xFF3366CC,
            archivo: "principal-\(id.uuidString).tinta")
        try repo.guardar(
            EstadoPestanasIPad(abiertas: [registro], activa: id),
            panel: .principal, contexto: "cuaderno-A")

        let documento = DocumentoPersistenteID(id).claveCompatibilidadUInt64
        let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: documento,
            repositorio: repo,
            contextos: ["cuaderno-A"])

        XCTAssertEqual(ubicacion?.contextoCuaderno, "cuaderno-A")
        XCTAssertEqual(ubicacion?.panel, .principal)
        XCTAssertEqual(ubicacion?.pestanaId, id)
        XCTAssertEqual(ubicacion?.titulo, "Circuitos")
        XCTAssertEqual(ubicacion?.abierta, true)
    }

    func testLocalizaPestanaCerradaEnPanelSecundario() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let id = UUID()
        let registro = RegistroPestanaIPad(
            id: id, titulo: "Referencia PDF",
            colorARGB: 0xFF999999,
            archivo: "secundario-\(id.uuidString).tinta")
        try repo.guardar(
            EstadoPestanasIPad(cerradas: [registro]),
            panel: .secundario, contexto: "cuaderno-B")

        let documento = DocumentoPersistenteID(id).claveCompatibilidadUInt64
        let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: documento,
            repositorio: repo,
            contextos: ["cuaderno-B"])

        XCTAssertEqual(ubicacion?.panel, .secundario)
        XCTAssertEqual(ubicacion?.abierta, false)
        XCTAssertEqual(ubicacion?.titulo, "Referencia PDF")
    }

    func testBuscaEntreVariosContextosSinConfundirUUID() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let a = UUID(), b = UUID()
        try repo.guardar(
            EstadoPestanasIPad(abiertas: [RegistroPestanaIPad(
                id: a, titulo: "A", colorARGB: 0,
                archivo: "principal-\(a.uuidString).tinta")], activa: a),
            panel: .principal, contexto: "libro-A")
        try repo.guardar(
            EstadoPestanasIPad(abiertas: [RegistroPestanaIPad(
                id: b, titulo: "B", colorARGB: 0,
                archivo: "principal-\(b.uuidString).tinta")], activa: b),
            panel: .principal, contexto: "libro-B")

        let destino = DocumentoPersistenteID(b).claveCompatibilidadUInt64
        let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: destino, repositorio: repo)
        XCTAssertEqual(ubicacion?.contextoCuaderno, "libro-B")
        XCTAssertEqual(ubicacion?.pestanaId, b)
    }

    func testDocumentoDesconocidoNoProduceFallbackFalso() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let id = UUID()
        try repo.guardar(
            EstadoPestanasIPad(abiertas: [RegistroPestanaIPad(
                id: id, titulo: "Único", colorARGB: 0,
                archivo: "principal-\(id.uuidString).tinta")], activa: id),
            panel: .principal, contexto: "uno")

        XCTAssertNil(LocalizadorDocumentoIPad.localizar(
            documentoId: UInt64.max,
            repositorio: repo,
            contextos: ["uno"]))
        XCTAssertNil(LocalizadorDocumentoIPad.localizar(
            documentoId: 0,
            repositorio: repo,
            contextos: ["uno"]))
    }
}
