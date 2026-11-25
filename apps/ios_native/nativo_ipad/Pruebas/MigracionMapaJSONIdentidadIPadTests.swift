import XCTest
@testable import CuadernUEstudio

final class MigracionMapaJSONIdentidadIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("mapa-json-id-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    private func guardar(_ objeto: Any, en url: URL) throws {
        let datos = try JSONSerialization.data(withJSONObject: objeto, options: [.sortedKeys])
        try datos.write(to: url)
    }

    private func mapa(_ url: URL) throws -> [String: Any] {
        try JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
    }

    func testMueveClaveLegacyAUUIDSinCambiarValor() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("global.json")
        let doc = DocumentoPersistenteID(), legacy = String(doc.claveCompatibilidadUInt64)
        try guardar([legacy: ["texto": "hola", "n": 2]], en: archivo)
        let tx = CoordinadorPersistenciaTransaccionalIPad(base: raiz.appendingPathComponent("tx"))
        let resultado = try MigradorMapaJSONIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            archivo: archivo,
            transacciones: tx)
        XCTAssertEqual(resultado.estado, .migrado)
        let leido = try mapa(archivo)
        XCTAssertNil(leido[legacy])
        XCTAssertNotNil(leido[doc.description])
    }

    func testDurableIdenticoEliminaAliasLegacy() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("global.json")
        let doc = DocumentoPersistenteID(), legacy = String(doc.claveCompatibilidadUInt64)
        let valor: [String: Any] = ["x": 1, "y": [1,2,3]]
        try guardar([legacy: valor, doc.description: valor], en: archivo)
        let resultado = try MigradorMapaJSONIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            archivo: archivo,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))
        XCTAssertEqual(resultado.estado, .yaMigrado)
        let leido = try mapa(archivo)
        XCTAssertNil(leido[legacy])
        XCTAssertNotNil(leido[doc.description])
    }

    func testDurableDistintoEsConflictoYNoTocaArchivo() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("global.json")
        let doc = DocumentoPersistenteID(), legacy = String(doc.claveCompatibilidadUInt64)
        try guardar([legacy: ["v": 1], doc.description: ["v": 2]], en: archivo)
        let antes = try Data(contentsOf: archivo)
        XCTAssertThrowsError(try MigradorMapaJSONIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            archivo: archivo,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                XCTAssertEqual($0 as? ErrorMigracionMapaJSONIPad,
                               .conflictoValorDurable)
            }
        XCTAssertEqual(try Data(contentsOf: archivo), antes)
    }

    func testNoAceptaHandleRuntime() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("global.json")
        try guardar(["7": ["v": 1]], en: archivo)
        XCTAssertThrowsError(try MigradorMapaJSONIdentidadIPad.migrar(
            documentoId: DocumentoPersistenteID(),
            claveLegacy: 7,
            archivo: archivo,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                XCTAssertEqual($0 as? ErrorMigracionMapaJSONIPad,
                               .claveLegacyNoDerivada)
            }
    }

    func testRaizArraySeRechaza() throws {
        let raiz = try base(), archivo = raiz.appendingPathComponent("global.json")
        let doc = DocumentoPersistenteID()
        try guardar([["x":1]], en: archivo)
        XCTAssertThrowsError(try MigradorMapaJSONIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            archivo: archivo,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                XCTAssertEqual($0 as? ErrorMigracionMapaJSONIPad, .raizNoEsObjeto)
            }
    }
}
