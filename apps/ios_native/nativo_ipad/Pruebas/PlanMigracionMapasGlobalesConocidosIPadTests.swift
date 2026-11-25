import XCTest
@testable import CuadernUEstudio

final class PlanMigracionMapasGlobalesConocidosIPadTests: XCTestCase {
    func testMigraObjetosBloquesYFondosConElMismoUUID() throws {
        let fm = FileManager.default, doc = DocumentoPersistenteID()
        let documentos = fm.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let soporte = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let objetos = documentos.appendingPathComponent("ObjetosTecnicos/objetos.json")
        let bloques = documentos.appendingPathComponent("Bloques/bloques.json")
        let fondos = soporte.appendingPathComponent("Tinta/FondosObjeto/fondos-objeto.json")
        let legacy = String(doc.claveCompatibilidadUInt64)
        let durable = doc.description
        let fondoLegacy = "\(legacy):media-1", fondoDurable = "\(durable):media-1"
        defer {
            for u in [objetos, bloques, fondos] { try? fm.removeItem(at: u) }
        }
        for u in [objetos, bloques, fondos] {
            try fm.createDirectory(at: u.deletingLastPathComponent(), withIntermediateDirectories: true)
        }
        let escribir: (Any, URL) throws -> Void = { obj, url in
            try JSONSerialization.data(withJSONObject: obj, options: [.sortedKeys]).write(to: url)
        }
        try escribir([legacy: [["id":"o1"]]], objetos)
        try escribir([legacy: [["id":"b1"]]], bloques)
        try escribir([
            "formato": 1,
            "valores": [fondoLegacy: ["bloqueado": true]],
            "nombres": [fondoLegacy: "Printout"]
        ], fondos)

        let txBase = fm.temporaryDirectory.appendingPathComponent("global-known-\(UUID())", isDirectory: true)
        defer { try? fm.removeItem(at: txBase) }
        let resultado = try PlanMigracionMapasGlobalesConocidosIPad.ejecutar(
            documentoId: doc,
            transacciones: .init(base: txBase))
        XCTAssertEqual(resultado.objetosTecnicos?.estado, .migrado)
        XCTAssertEqual(resultado.bloques?.estado, .migrado)
        XCTAssertEqual(resultado.fondosObjeto?.clavesMovidas, 2)

        let objetosJSON = try JSONSerialization.jsonObject(with: Data(contentsOf: objetos)) as! [String: Any]
        let bloquesJSON = try JSONSerialization.jsonObject(with: Data(contentsOf: bloques)) as! [String: Any]
        let fondosJSON = try JSONSerialization.jsonObject(with: Data(contentsOf: fondos)) as! [String: Any]
        XCTAssertNil(objetosJSON[legacy]); XCTAssertNotNil(objetosJSON[durable])
        XCTAssertNil(bloquesJSON[legacy]); XCTAssertNotNil(bloquesJSON[durable])
        let valores = fondosJSON["valores"] as! [String: Any]
        let nombres = fondosJSON["nombres"] as! [String: Any]
        XCTAssertNil(valores[fondoLegacy]); XCTAssertNotNil(valores[fondoDurable])
        XCTAssertNil(nombres[fondoLegacy]); XCTAssertNotNil(nombres[fondoDurable])
    }
}
