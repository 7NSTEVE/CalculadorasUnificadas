import XCTest
@testable import CuadernUEstudio

final class GestorVersionesDocumentoTests: XCTestCase {
    private var carpeta: URL!

    override func setUp() {
        super.setUp()
        carpeta = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Versiones-\(UUID().uuidString)",
                                    isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
    }

    override func tearDown() {
        if let carpeta { try? FileManager.default.removeItem(at: carpeta) }
        super.tearDown()
    }

    private func crear(
        en gestor: GestorVersionesDocumento,
        documento: UInt64 = 42,
        datos: Data = Data([0x43, 0x55, 0x41, 0x44])
    ) -> InstantaneaDocumento? {
        gestor.crear(
            documentoId: documento,
            nombre: "Antes de editar",
            motivo: "prueba",
            datosTinta: datos,
            bloques: [],
            tecnicos: [],
            capas: EstadoCapasDocumento())
    }

    func testVersionExitosaPersisteSnapshotYManifiestoYReabre() throws {
        let gestor = GestorVersionesDocumento(base: carpeta)
        guard let version = crear(en: gestor) else {
            return XCTFail("La versión debía crearse")
        }

        XCTAssertTrue(FileManager.default.fileExists(
            atPath: carpeta.appendingPathComponent(version.archivoTinta).path))
        XCTAssertTrue(FileManager.default.fileExists(
            atPath: carpeta.appendingPathComponent("versiones.json").path))

        let reabierto = GestorVersionesDocumento(base: carpeta)
        XCTAssertEqual(reabierto.listar(42).map(\.id), [version.id])
    }

    func testFalloDeManifiestoHaceRollbackDelSnapshotYNoPublicaCache() throws {
        struct FalloEsperado: Error {}
        let gestor = GestorVersionesDocumento(
            base: carpeta,
            escribirManifiesto: { _, _ in throw FalloEsperado() })

        XCTAssertNil(crear(en: gestor))
        XCTAssertTrue(gestor.listar(42).isEmpty)

        let archivos = try FileManager.default.contentsOfDirectory(
            at: carpeta,
            includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])
        XCTAssertFalse(archivos.contains { $0.pathExtension.lowercased() == "tinta" })
    }

    func testArranqueLimpiaSnapshotHuerfanoCuandoManifestEsConfiable() throws {
        let gestor = GestorVersionesDocumento(base: carpeta)
        guard let version = crear(en: gestor) else {
            return XCTFail("La versión debía crearse")
        }
        let huerfano = carpeta.appendingPathComponent("huerfano.tinta")
        try Data([1, 2, 3]).write(to: huerfano, options: .atomic)
        XCTAssertTrue(FileManager.default.fileExists(atPath: huerfano.path))

        let reabierto = GestorVersionesDocumento(base: carpeta)
        XCTAssertFalse(FileManager.default.fileExists(atPath: huerfano.path))
        XCTAssertEqual(reabierto.listar(42).map(\.id), [version.id])
    }

    func testManifestCorruptoPreservaSnapshotsParaRecuperacionManual() throws {
        let snapshot = carpeta.appendingPathComponent("preservar.tinta")
        try Data([9, 9, 9]).write(to: snapshot, options: .atomic)
        try Data("{no-es-json".utf8).write(
            to: carpeta.appendingPathComponent("versiones.json"), options: .atomic)

        _ = GestorVersionesDocumento(base: carpeta)

        XCTAssertTrue(FileManager.default.fileExists(atPath: snapshot.path),
                      "Un índice corrupto no autoriza a borrar snapshots potencialmente recuperables")
    }

    func testDatosVaciosNoCreanVersionNiArchivos() throws {
        let gestor = GestorVersionesDocumento(base: carpeta)
        XCTAssertNil(crear(en: gestor, datos: Data()))
        XCTAssertTrue(gestor.listar(42).isEmpty)
        let archivos = try FileManager.default.contentsOfDirectory(
            at: carpeta, includingPropertiesForKeys: nil)
        XCTAssertTrue(archivos.isEmpty)
    }
}
