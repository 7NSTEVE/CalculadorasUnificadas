import XCTest
@testable import CuadernUEstudio

final class MigracionIdentidadSidecarsIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("migracion-sidecar-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func datos(_ texto: String) -> Data { Data(texto.utf8) }

    func testMigraLegacyADurableYEliminaOrigenComoUnaTransaccion() throws {
        let raiz = try base(), carpeta = raiz.appendingPathComponent("sidecars", isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta, withIntermediateDirectories: true)
        let doc = DocumentoPersistenteID()
        let descriptor = DescriptorSidecarFilePerDocumentIPad(nombre: "Notas", carpeta: carpeta)
        let rutas = descriptor.rutas(documentoId: doc)
        try datos("contenido").write(to: rutas.legacy)

        let migrador = MigradorIdentidadSidecarsIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(
                base: raiz.appendingPathComponent("journals")))
        let resultado = try migrador.migrarArchivo(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            repositorio: descriptor.nombre,
            origenLegacy: rutas.legacy,
            destinoDurable: rutas.durable)

        XCTAssertEqual(resultado.estado, .migrado)
        XCTAssertFalse(FileManager.default.fileExists(atPath: rutas.legacy.path))
        XCTAssertEqual(try Data(contentsOf: rutas.durable), datos("contenido"))
    }

    func testNuncaAceptaHandleQueNoSeaDerivadoDelUUID() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let migrador = MigradorIdentidadSidecarsIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(
                base: raiz.appendingPathComponent("journals")))
        XCTAssertThrowsError(try migrador.migrarArchivo(
            documentoId: doc,
            claveLegacy: 7,
            repositorio: "legacy",
            origenLegacy: raiz.appendingPathComponent("doc-7.json"),
            destinoDurable: raiz.appendingPathComponent("doc-uuid.json"))) {
                XCTAssertEqual($0 as? ErrorMigracionSidecarIPad,
                               .claveLegacyNoDerivada)
            }
    }

    func testDurableIdenticoRetiraSoloAliasLegacy() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let legacy = raiz.appendingPathComponent("legacy"), durable = raiz.appendingPathComponent("durable")
        try datos("mismo").write(to: legacy); try datos("mismo").write(to: durable)
        let migrador = MigradorIdentidadSidecarsIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(
                base: raiz.appendingPathComponent("journals")))
        let resultado = try migrador.migrarArchivo(
            documentoId: doc, claveLegacy: doc.claveCompatibilidadUInt64,
            repositorio: "x", origenLegacy: legacy, destinoDurable: durable)
        XCTAssertEqual(resultado.estado, .yaMigrado)
        XCTAssertFalse(FileManager.default.fileExists(atPath: legacy.path))
        XCTAssertEqual(try Data(contentsOf: durable), datos("mismo"))
    }

    func testDurableDiferenteNoSobrescribeNinguno() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let legacy = raiz.appendingPathComponent("legacy"), durable = raiz.appendingPathComponent("durable")
        try datos("viejo").write(to: legacy); try datos("nuevo-distinto").write(to: durable)
        let migrador = MigradorIdentidadSidecarsIPad(
            transacciones: CoordinadorPersistenciaTransaccionalIPad(
                base: raiz.appendingPathComponent("journals")))
        XCTAssertThrowsError(try migrador.migrarArchivo(
            documentoId: doc, claveLegacy: doc.claveCompatibilidadUInt64,
            repositorio: "x", origenLegacy: legacy, destinoDurable: durable)) {
                XCTAssertEqual($0 as? ErrorMigracionSidecarIPad,
                               .conflictoDestinoDurable)
            }
        XCTAssertEqual(try Data(contentsOf: legacy), datos("viejo"))
        XCTAssertEqual(try Data(contentsOf: durable), datos("nuevo-distinto"))
    }

    func testSinLegacyDistingueNoHabiaDeYaMigrado() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let tx = CoordinadorPersistenciaTransaccionalIPad(
            base: raiz.appendingPathComponent("journals"))
        let migrador = MigradorIdentidadSidecarsIPad(transacciones: tx)
        let legacy = raiz.appendingPathComponent("legacy"), durable = raiz.appendingPathComponent("durable")

        XCTAssertEqual(try migrador.migrarArchivo(
            documentoId: doc, claveLegacy: doc.claveCompatibilidadUInt64,
            repositorio: "x", origenLegacy: legacy, destinoDurable: durable).estado,
                       .noHabiaLegacy)
        try datos("uuid").write(to: durable)
        XCTAssertEqual(try migrador.migrarArchivo(
            documentoId: doc, claveLegacy: doc.claveCompatibilidadUInt64,
            repositorio: "x", origenLegacy: legacy, destinoDurable: durable).estado,
                       .yaMigrado)
    }

    func testNombresDurablesNuncaUsanHandleRuntime() {
        let doc = DocumentoPersistenteID()
        XCTAssertEqual(
            MigradorIdentidadSidecarsIPad.nombreDurable(documentoId: doc),
            "doc-\(doc.description).json")
        XCTAssertNotEqual(
            MigradorIdentidadSidecarsIPad.nombreDurable(documentoId: doc),
            "doc-7.json")
    }
}
