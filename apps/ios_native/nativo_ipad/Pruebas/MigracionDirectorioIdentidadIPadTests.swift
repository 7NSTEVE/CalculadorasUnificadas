import XCTest
@testable import CuadernUEstudio

final class MigracionDirectorioIdentidadIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("dir-id-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    func testMigraArbolCompletoYEliminaOrigen() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let origen = raiz.appendingPathComponent(String(doc.claveCompatibilidadUInt64), isDirectory: true)
        let destino = raiz.appendingPathComponent(doc.description, isDirectory: true)
        try FileManager.default.createDirectory(
            at: origen.appendingPathComponent("sub"), withIntermediateDirectories: true)
        try Data("a".utf8).write(to: origen.appendingPathComponent("a.bin"))
        try Data("b".utf8).write(to: origen.appendingPathComponent("sub/b.bin"))

        let resultado = try MigradorDirectorioIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            origenLegacy: origen,
            destinoDurable: destino,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))

        XCTAssertEqual(resultado.archivosMigrados, 2)
        XCTAssertTrue(resultado.origenEliminado)
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("a.bin")),
                       Data("a".utf8))
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("sub/b.bin")),
                       Data("b".utf8))
    }

    func testDestinoIdenticoSoloRetiraAliasLegacy() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let origen = raiz.appendingPathComponent("legacy", isDirectory: true)
        let destino = raiz.appendingPathComponent("durable", isDirectory: true)
        try FileManager.default.createDirectory(at: origen, withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: destino, withIntermediateDirectories: true)
        try Data("mismo".utf8).write(to: origen.appendingPathComponent("x"))
        try Data("mismo".utf8).write(to: destino.appendingPathComponent("x"))

        let resultado = try MigradorDirectorioIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            origenLegacy: origen,
            destinoDurable: destino,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))
        XCTAssertEqual(resultado.aliasIdenticosEliminados, 1)
        XCTAssertTrue(resultado.origenEliminado)
        XCTAssertEqual(try Data(contentsOf: destino.appendingPathComponent("x")),
                       Data("mismo".utf8))
    }

    func testDestinoDistintoBloqueaSinTocarOrigen() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let origen = raiz.appendingPathComponent("legacy", isDirectory: true)
        let destino = raiz.appendingPathComponent("durable", isDirectory: true)
        try FileManager.default.createDirectory(at: origen, withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: destino, withIntermediateDirectories: true)
        try Data("viejo".utf8).write(to: origen.appendingPathComponent("x"))
        try Data("otro".utf8).write(to: destino.appendingPathComponent("x"))

        XCTAssertThrowsError(try MigradorDirectorioIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            origenLegacy: origen,
            destinoDurable: destino,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                guard case ErrorMigracionDirectorioIPad.conflictoDestino = $0 else {
                    return XCTFail("Error inesperado: \($0)")
                }
            }
        XCTAssertEqual(try Data(contentsOf: origen.appendingPathComponent("x")),
                       Data("viejo".utf8))
    }

    func testLimiteArchivosSeAplicaAntesDeCommit() throws {
        let raiz = try base(), doc = DocumentoPersistenteID()
        let origen = raiz.appendingPathComponent("legacy", isDirectory: true)
        try FileManager.default.createDirectory(at: origen, withIntermediateDirectories: true)
        try Data([1]).write(to: origen.appendingPathComponent("a"))
        try Data([2]).write(to: origen.appendingPathComponent("b"))
        XCTAssertThrowsError(try MigradorDirectorioIdentidadIPad.migrar(
            documentoId: doc,
            claveLegacy: doc.claveCompatibilidadUInt64,
            origenLegacy: origen,
            destinoDurable: raiz.appendingPathComponent("durable"),
            maximoArchivos: 1,
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                guard case ErrorMigracionDirectorioIPad.demasiadosArchivos = $0 else {
                    return XCTFail("Error inesperado")
                }
            }
        XCTAssertTrue(FileManager.default.fileExists(atPath: origen.appendingPathComponent("a").path))
        XCTAssertTrue(FileManager.default.fileExists(atPath: origen.appendingPathComponent("b").path))
    }

    func testNuncaAceptaHandleRuntime() throws {
        let raiz = try base(), origen = raiz.appendingPathComponent("7", isDirectory: true)
        try FileManager.default.createDirectory(at: origen, withIntermediateDirectories: true)
        XCTAssertThrowsError(try MigradorDirectorioIdentidadIPad.migrar(
            documentoId: DocumentoPersistenteID(),
            claveLegacy: 7,
            origenLegacy: origen,
            destinoDurable: raiz.appendingPathComponent("uuid"),
            transacciones: .init(base: raiz.appendingPathComponent("tx")))) {
                XCTAssertEqual($0 as? ErrorMigracionDirectorioIPad, .claveLegacyNoDerivada)
            }
    }
}
