import XCTest
@testable import CuadernUEstudio

final class RespaldoVerificableIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u=FileManager.default.temporaryDirectory.appendingPathComponent("backup-\(UUID())",isDirectory:true)
        try FileManager.default.createDirectory(at:u,withIntermediateDirectories:true);return u
    }
    func testRoundTrip() throws {
        let r=try base(),d=DocumentoPersistenteID(),b=r.appendingPathComponent("a",isDirectory:true)
        _=try RespaldoVerificableIPad.crear(documentoId:d,componentes:[.init(rutaRelativa:"doc/a",datos:Data("abc".utf8)),.init(rutaRelativa:"side/b",datos:Data("{}".utf8))],destino:b)
        XCTAssertEqual(try RespaldoVerificableIPad.validar(b,documentoEsperado:d).componentesValidos,2)
        let s=r.appendingPathComponent("staging",isDirectory:true);_=try RespaldoVerificableIPad.prepararRestauracion(b,documentoEsperado:d,staging:s)
        XCTAssertEqual(try Data(contentsOf:s.appendingPathComponent("doc/a")),Data("abc".utf8))
    }
    func testCorrupcionSeDetecta() throws {
        let r=try base(),d=DocumentoPersistenteID(),b=r.appendingPathComponent("b",isDirectory:true)
        _=try RespaldoVerificableIPad.crear(documentoId:d,componentes:[.init(rutaRelativa:"x",datos:Data("bien".utf8))],destino:b)
        try Data("mal".utf8).write(to:b.appendingPathComponent("x"))
        XCTAssertThrowsError(try RespaldoVerificableIPad.validar(b,documentoEsperado:d))
    }
    func testDocumentoDistinto() throws {
        let r=try base(),a=DocumentoPersistenteID(),b=DocumentoPersistenteID(),u=r.appendingPathComponent("c",isDirectory:true)
        _=try RespaldoVerificableIPad.crear(documentoId:a,componentes:[.init(rutaRelativa:"x",datos:Data())],destino:u)
        XCTAssertThrowsError(try RespaldoVerificableIPad.validar(u,documentoEsperado:b)){XCTAssertEqual($0 as? ErrorRespaldoVerificableIPad,.documentoDistinto)}
    }
    func testPathTraversalYDuplicado() throws {
        let r=try base(),d=DocumentoPersistenteID()
        XCTAssertThrowsError(try RespaldoVerificableIPad.crear(documentoId:d,componentes:[.init(rutaRelativa:"../x",datos:Data())],destino:r.appendingPathComponent("d")))
        XCTAssertThrowsError(try RespaldoVerificableIPad.crear(documentoId:d,componentes:[.init(rutaRelativa:"a/x",datos:Data()),.init(rutaRelativa:"/a/x",datos:Data())],destino:r.appendingPathComponent("e")))
    }
    func testStagingNoSobrescribe() throws {
        let r=try base(),d=DocumentoPersistenteID(),b=r.appendingPathComponent("f",isDirectory:true)
        _=try RespaldoVerificableIPad.crear(documentoId:d,componentes:[.init(rutaRelativa:"x",datos:Data())],destino:b)
        let s=r.appendingPathComponent("s",isDirectory:true);try FileManager.default.createDirectory(at:s,withIntermediateDirectories:true);try Data([1]).write(to:s.appendingPathComponent("existente"))
        XCTAssertThrowsError(try RespaldoVerificableIPad.prepararRestauracion(b,documentoEsperado:d,staging:s)){XCTAssertEqual($0 as? ErrorRespaldoVerificableIPad,.destinoNoVacio)}
    }
}
