import XCTest
@testable import CuadernUEstudio

final class WhiteboardDocumentoIPadTests:XCTestCase{
    private func base()throws->URL{let u=FileManager.default.temporaryDirectory.appendingPathComponent("whiteboard-\(UUID())",isDirectory:true);try FileManager.default.createDirectory(at:u,withIntermediateDirectories:true);return u}
    func testCrearSiempreTieneBoardYReabre()throws{
        let b=try base();let repo=RepositorioWhiteboardIPad(base:b);let doc=try repo.crear(titulo:"Circuitos")
        XCTAssertEqual(doc.boards.count,1);XCTAssertEqual(try repo.cargar(doc.id)?.boards.count,1);XCTAssertEqual(doc.boards[0].documentoCanvasId,DocumentoPersistenteID(doc.boards[0].id))
    }
    func testAgregarMoverYReabrirConOrdenEstable()throws{
        let b=try base();let repo=RepositorioWhiteboardIPad(base:b);let doc=try repo.crear(titulo:"W");let segundo=try repo.agregarBoard(documentoId:doc.id,titulo:"Segundo");_=try repo.agregarBoard(documentoId:doc.id,titulo:"Tercero");try repo.moverBoard(documentoId:doc.id,desde:2,hasta:0)
        let r=try RepositorioWhiteboardIPad(base:b).cargar(doc.id)!;XCTAssertEqual(r.boards.map(\.titulo),["Tercero","Board 1","Segundo"]);XCTAssertEqual(r.boards.map(\.orden),[0,1,2]);XCTAssertTrue(r.boards.contains{$0.id==segundo.id})
    }
    func testNoPermiteEliminarUltimoBoard()throws{
        let repo=RepositorioWhiteboardIPad(base:try base());let doc=try repo.crear(titulo:"W")
        XCTAssertThrowsError(try repo.eliminarBoard(documentoId:doc.id,boardId:doc.boards[0].id)){XCTAssertEqual($0 as? ErrorWhiteboardDocumentoIPad,.minimoUnBoard)}
    }
    func testBoardConservaPlantillaYColor()throws{
        let repo=RepositorioWhiteboardIPad(base:try base()),plantilla=UUID();let doc=try repo.crear(titulo:"W");let id=doc.boards[0].id
        try repo.configurarBoard(documentoId:doc.id,boardId:id,plantillaId:plantilla,colorARGB:0xFF112233)
        let b=try repo.cargar(doc.id)!.boards[0];XCTAssertEqual(b.plantillaId,plantilla);XCTAssertEqual(b.colorFondoARGB,0xFF112233)
    }
    func testTituloVacioRechazado()throws{XCTAssertThrowsError(try RepositorioWhiteboardIPad(base:base()).crear(titulo:"   "))}
}
