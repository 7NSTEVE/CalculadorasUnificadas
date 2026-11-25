import XCTest
@testable import CuadernUEstudio

private final class AdaptadorAccionIAPrueba: AdaptadorObjetivoAccionIAIPad {
    var datos: Data
    init(_ texto:String){datos=Data(texto.utf8)}
    func leer(_ objetivo:ObjetivoAccionIAIPad)throws->Data{datos}
    func escribir(_ datos:Data,en objetivo:ObjetivoAccionIAIPad)throws{self.datos=datos}
}

final class AccionesIAReversiblesIPadTests:XCTestCase {
    private func base()throws->URL{let u=FileManager.default.temporaryDirectory.appendingPathComponent("ia-rev-\(UUID())",isDirectory:true);try FileManager.default.createDirectory(at:u,withIntermediateDirectories:true);return u}
    private func propuesta(_ doc:DocumentoPersistenteID=DocumentoPersistenteID())->AccionIAReversibleIPad{.propuesta(objetivo:.init(documentoId:doc,tipo:.seleccionTexto,objetoId:"x"),titulo:"Corregir",explicacion:"Mejorar redacción",antes:Data("antes".utf8),despues:Data("despues".utf8),fuentes:["fuente:1"])}
    func testAplicarYRevertirRoundTrip()throws{
        let r=RepositorioAccionesIAReversiblesIPad(base:try base()),a=try r.guardarPropuesta(propuesta()),adaptador=AdaptadorAccionIAPrueba("antes")
        XCTAssertEqual(try r.aplicar(a.id,adaptador:adaptador).estado,.aplicada);XCTAssertEqual(String(data:adaptador.datos,encoding:.utf8),"despues")
        XCTAssertEqual(try r.revertir(a.id,adaptador:adaptador).estado,.revertida);XCTAssertEqual(String(data:adaptador.datos,encoding:.utf8),"antes")
    }
    func testNoAplicaSiObjetivoCambio()throws{
        let r=RepositorioAccionesIAReversiblesIPad(base:try base()),a=try r.guardarPropuesta(propuesta()),adaptador=AdaptadorAccionIAPrueba("edición humana nueva")
        XCTAssertThrowsError(try r.aplicar(a.id,adaptador:adaptador)){XCTAssertEqual($0 as? ErrorAccionIAReversibleIPad,.objetivoCambioDesdePropuesta)}
        XCTAssertEqual(String(data:adaptador.datos,encoding:.utf8),"edición humana nueva")
    }
    func testNoRevierteSiUsuarioEditoDespues()throws{
        let r=RepositorioAccionesIAReversiblesIPad(base:try base()),a=try r.guardarPropuesta(propuesta()),adaptador=AdaptadorAccionIAPrueba("antes")
        _=try r.aplicar(a.id,adaptador:adaptador);adaptador.datos=Data("cambio posterior".utf8)
        XCTAssertThrowsError(try r.revertir(a.id,adaptador:adaptador)){XCTAssertEqual($0 as? ErrorAccionIAReversibleIPad,.contenidoAplicadoCambio)}
    }
    func testReaperturaConservaPropuestaYFuentes()throws{
        let b=try base(),doc=DocumentoPersistenteID();let id:UUID
        do{let r=RepositorioAccionesIAReversiblesIPad(base:b);id=try r.guardarPropuesta(propuesta(doc)).id}
        let a=RepositorioAccionesIAReversiblesIPad(base:b).acciones(documentoId:doc).first
        XCTAssertEqual(a?.id,id);XCTAssertEqual(a?.fuentes,["fuente:1"]);XCTAssertEqual(a?.estado,.propuesta)
    }
    func testDescartarNoMutaObjetivo()throws{
        let r=RepositorioAccionesIAReversiblesIPad(base:try base()),a=try r.guardarPropuesta(propuesta()),adaptador=AdaptadorAccionIAPrueba("antes")
        try r.descartar(a.id);XCTAssertEqual(String(data:adaptador.datos,encoding:.utf8),"antes");XCTAssertTrue(r.acciones().isEmpty)
    }
}
