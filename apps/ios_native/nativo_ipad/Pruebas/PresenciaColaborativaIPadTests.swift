import XCTest
@testable import CuadernUEstudio

final class PresenciaColaborativaIPadTests:XCTestCase{
    private func op(doc:DocumentoPersistenteID,autor:String="a",x:CGFloat=50,y:CGFloat=25)->OperacionColaborativa{
        let cursor=CursorRemotoCuadernUIPad(autorId:autor,nombre:"el dueño",documentoId:doc,punto:CGPoint(x:x,y:y),tamano:CGSize(width:100,height:100),colorARGB:0xFFFF0000)
        let e=JSONEncoder();e.dateEncodingStrategy = .iso8601
        return OperacionColaborativa(sesionId:UUID(),documentoId:doc.claveCompatibilidadUInt64,marca:.init(contador:1,autor:autor),tipo:.cursor,objetoId:"cursor:\(autor)",carga:try! e.encode(cursor),rolAutor:.editor)
    }
    func testProcesaCursorYNormalizaPunto(){let d=DocumentoPersistenteID(),c=CoordinadorPresenciaColaborativaIPad(documentoId:d,observar:false);let t=Date();c.procesar(op(doc:d),ahora:t);let r=c.cursores(ahora:t);XCTAssertEqual(r.count,1);XCTAssertEqual(r[0].xNormalizado,0.5,accuracy:0.001);XCTAssertEqual(r[0].yNormalizado,0.25,accuracy:0.001)}
    func testIgnoraDocumentoDistinto(){let a=DocumentoPersistenteID(),b=DocumentoPersistenteID(),c=CoordinadorPresenciaColaborativaIPad(documentoId:a,observar:false);c.procesar(op(doc:b));XCTAssertTrue(c.cursores().isEmpty)}
    func testIgnoraPayloadConAutorQueNoCoincide()throws{let d=DocumentoPersistenteID(),c=CoordinadorPresenciaColaborativaIPad(documentoId:d,observar:false);var o=op(doc:d,autor:"payload");o.marca.autor="firma-diferente";c.procesar(o);XCTAssertTrue(c.cursores().isEmpty)}
    func testTTLRetiraCursorViejo(){let d=DocumentoPersistenteID(),c=CoordinadorPresenciaColaborativaIPad(documentoId:d,observar:false);c.ttl=5;let t=Date(timeIntervalSince1970:100);c.procesar(op(doc:d),ahora:t);XCTAssertEqual(c.cursores(ahora:t.addingTimeInterval(4)).count,1);XCTAssertTrue(c.cursores(ahora:t.addingTimeInterval(6)).isEmpty)}
    func testCoordenadasSeClampan(){let d=DocumentoPersistenteID();let c=CursorRemotoCuadernUIPad(autorId:"a",nombre:"x",documentoId:d,punto:CGPoint(x:-5,y:500),tamano:CGSize(width:100,height:100),colorARGB:0);XCTAssertEqual(c.xNormalizado,0);XCTAssertEqual(c.yNormalizado,1)}
}
