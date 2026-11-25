import XCTest

final class NavegacionProfundaDocumentosIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-NavegacionProfunda-\(UUID().uuidString)",
            isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testClavesEstablesDistinguenTintaTextoMedioYRegion() throws {
        XCTAssertEqual(ClaveDestinoProfundoIPad.leer("t:abc"), .trazo("abc"))
        XCTAssertEqual(ClaveDestinoProfundoIPad.leer("x:def"), .texto("def"))
        XCTAssertEqual(ClaveDestinoProfundoIPad.leer("m:ghi"), .medio("ghi"))

        let original = CGRect(x: -12.3456, y: 98.7654,
                              width: 420.1254, height: 310.8756)
        let clave = try XCTUnwrap(CodecAnclaRegionIPad.codificar(original))
        guard case .region(let region) = ClaveDestinoProfundoIPad.leer(clave) else {
            return XCTFail("La clave r: debe resolverse como región")
        }
        XCTAssertEqual(region.minX, original.minX, accuracy: 0.001)
        XCTAssertEqual(region.minY, original.minY, accuracy: 0.001)
        XCTAssertEqual(region.width, original.width, accuracy: 0.001)
        XCTAssertEqual(region.height, original.height, accuracy: 0.001)

        XCTAssertEqual(ClaveDestinoProfundoIPad.leer("legacy-id"), .legado("legacy-id"))
    }

    func testCodecRegionRechazaGeometriaNoFinitaOVacia() {
        XCTAssertNil(CodecAnclaRegionIPad.codificar(.zero))
        XCTAssertNil(CodecAnclaRegionIPad.codificar(
            CGRect(x: 0, y: 0, width: .infinity, height: 20)))
        XCTAssertNil(CodecAnclaRegionIPad.decodificar("r:1,2,0,20"))
        XCTAssertNil(CodecAnclaRegionIPad.decodificar("r:1,2,nan,20"))
        XCTAssertNil(CodecAnclaRegionIPad.decodificar("r:1,2,3"))
    }

    func testReabrirPestanaMueveSoloManifiestoYConservaBlob() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let activa = UUID()
        let cerrada = UUID()
        let registroActivo = RegistroPestanaIPad(
            id: activa, titulo: "Activa", colorARGB: 0,
            archivo: "principal-\(activa.uuidString).tinta")
        let registroCerrado = RegistroPestanaIPad(
            id: cerrada, titulo: "Destino", colorARGB: 0,
            archivo: "principal-\(cerrada.uuidString).tinta")
        try repo.guardar(
            EstadoPestanasIPad(
                abiertas: [registroActivo],
                cerradas: [registroCerrado], activa: activa),
            panel: .principal, contexto: "curso")
        let bytes = Data("contenido-durable".utf8)
        try repo.guardarContenido(bytes, registro: registroCerrado)

        let resultado = try repo.prepararPestanaParaNavegacion(
            id: cerrada, panel: .principal, contexto: "curso")
        XCTAssertEqual(resultado, .reabierta)

        guard case .cargado(let estado) = repo.leer(
            .principal, contexto: "curso") else {
            return XCTFail("El manifiesto debe seguir siendo legible")
        }
        XCTAssertEqual(estado.activa, cerrada)
        XCTAssertTrue(estado.abiertas.contains { $0.id == cerrada })
        XCTAssertFalse(estado.cerradas.contains { $0.id == cerrada })
        guard case .cargado(let despues) = repo.contenido(registroCerrado) else {
            return XCTFail("El blob original no debe desaparecer")
        }
        XCTAssertEqual(despues, bytes)
    }

    func testPestanaYaAbiertaSoloSeActivaSinDuplicarse() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let a = UUID(), b = UUID()
        let ra = RegistroPestanaIPad(
            id: a, titulo: "A", colorARGB: 0,
            archivo: "secundario-\(a.uuidString).tinta")
        let rb = RegistroPestanaIPad(
            id: b, titulo: "B", colorARGB: 0,
            archivo: "secundario-\(b.uuidString).tinta")
        try repo.guardar(
            EstadoPestanasIPad(abiertas: [ra, rb], activa: a),
            panel: .secundario, contexto: "ref")

        XCTAssertEqual(
            try repo.prepararPestanaParaNavegacion(
                id: b, panel: .secundario, contexto: "ref"),
            .yaAbierta)
        guard case .cargado(let estado) = repo.leer(
            .secundario, contexto: "ref") else {
            return XCTFail("El manifiesto debe seguir siendo legible")
        }
        XCTAssertEqual(estado.activa, b)
        XCTAssertEqual(estado.abiertas.map(\.id), [a, b])
    }

    func testDestinoInexistenteNoReescribeManifiesto() throws {
        let repo = RepositorioPestanasIPad(directorioBase: carpeta)
        let id = UUID()
        let registro = RegistroPestanaIPad(
            id: id, titulo: "Única", colorARGB: 0,
            archivo: "principal-\(id.uuidString).tinta")
        let original = EstadoPestanasIPad(abiertas: [registro], activa: id)
        try repo.guardar(original, panel: .principal, contexto: "uno")

        XCTAssertEqual(
            try repo.prepararPestanaParaNavegacion(
                id: UUID(), panel: .principal, contexto: "uno"),
            .noEncontrada)
        guard case .cargado(let despues) = repo.leer(
            .principal, contexto: "uno") else {
            return XCTFail("El manifiesto debe permanecer disponible")
        }
        XCTAssertEqual(despues, original)
    }
}
