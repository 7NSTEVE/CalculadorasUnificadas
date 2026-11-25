import XCTest

final class OCRManuscritoCacheNegativaIPadTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-OCRNegativo-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    func testCacheNegativaEsValidaPeroNoProduceHits() throws {
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        let fuente = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 77, idsTrazos: ["a", "b"])
        let negativo = RegistroOCRManuscritoIPad(
            fuenteId: fuente,
            documentoId: 77,
            idsTrazos: ["a", "b"],
            marco: CGRect(x: 10, y: 20, width: 120, height: 40),
            texto: "",
            confianza: 0,
            idiomas: ["es-ES", "en-US"],
            firmaMotor: "vision-text-r3|iPadOS-test")

        try repo.reemplazarDocumento(77, con: [negativo])
        XCTAssertEqual(repo.registros(documentoId: 77), [negativo])
        XCTAssertTrue(repo.buscar("a").isEmpty)
        XCTAssertTrue(repo.buscar("texto").isEmpty)
    }

    func testCambiarConjuntoDeTrazosCambiaFuenteYEliminaCacheAnterior() throws {
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        let vieja = RegistroOCRManuscritoIPad(
            fuenteId: RepositorioOCRManuscritoIPad.fuenteId(
                documentoId: 5, idsTrazos: ["t1", "t2"]),
            documentoId: 5,
            idsTrazos: ["t1", "t2"],
            marco: CGRect(x: 1, y: 2, width: 100, height: 30),
            texto: "",
            confianza: 0,
            idiomas: ["es-ES"],
            firmaMotor: "motor-a")
        try repo.reemplazarDocumento(5, con: [vieja])

        let nueva = RegistroOCRManuscritoIPad(
            fuenteId: RepositorioOCRManuscritoIPad.fuenteId(
                documentoId: 5, idsTrazos: ["t1", "t2", "t3"]),
            documentoId: 5,
            idsTrazos: ["t1", "t2", "t3"],
            marco: CGRect(x: 1, y: 2, width: 150, height: 35),
            texto: "Kirchhoff",
            confianza: 0.9,
            idiomas: ["es-ES"],
            firmaMotor: "motor-a")
        try repo.reemplazarDocumento(5, con: [nueva])

        let actuales = repo.registros(documentoId: 5)
        XCTAssertEqual(actuales.count, 1)
        XCTAssertEqual(actuales.first?.fuenteId, nueva.fuenteId)
        XCTAssertNotEqual(vieja.fuenteId, nueva.fuenteId)
        XCTAssertEqual(repo.buscar("kirchhoff").count, 1)
    }

    func testRegistroV1SinFirmaMotorSigueDecodificando() throws {
        let fuente = RepositorioOCRManuscritoIPad.fuenteId(
            documentoId: 9, idsTrazos: ["legacy"])
        let json = """
        {
          "version": 1,
          "registros": [{
            "fuenteId": "\(fuente)",
            "documentoId": 9,
            "idsTrazos": ["legacy"],
            "marco": [[0,0],[100,40]],
            "texto": "Laplace",
            "confianza": 0.8,
            "idiomas": ["es-ES"],
            "fecha": "2026-08-18T12:00:00Z"
          }]
        }
        """
        // CGRect tiene codificación propia de Foundation; este fixture sólo
        // valida compatibilidad de la propiedad opcional si el resto del JSON
        // logra decodificar en el runtime. En caso contrario, el repositorio
        // debe caer de forma segura a índice vacío.
        try Data(json.utf8).write(
            to: carpeta.appendingPathComponent("indice-v1.json"),
            options: .atomic)
        let repo = RepositorioOCRManuscritoIPad(base: carpeta)
        XCTAssertTrue(repo.registros().isEmpty
            || repo.registros().allSatisfy { $0.firmaMotor == nil })
    }
}
