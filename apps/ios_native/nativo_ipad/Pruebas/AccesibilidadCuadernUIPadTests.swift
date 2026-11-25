import XCTest
import UIKit
@testable import CuadernUEstudio

final class AccesibilidadCuadernUIPadTests: XCTestCase {
    func testAuditorDetectaBotonSinLabelYPequeno() {
        let raiz=UIView(frame:CGRect(x:0,y:0,width:300,height:300));let boton=UIButton(type:.system);boton.frame=CGRect(x:10,y:10,width:30,height:30);raiz.addSubview(boton)
        let i=AuditorAccesibilidadIPad.auditar(raiz)
        XCTAssertTrue(i.contains{$0.tipo == .etiquetaFaltante});XCTAssertTrue(i.contains{$0.tipo == .objetivoTactilPequeno})
    }
    func testAplicadorConfiguraSemanticaYObjetivoMinimo() {
        let b=UIButton(type:.system);_=AplicadorAccesibilidadIPad.control(b,etiqueta:"Buscar",ayuda:"Busca en todo CuadernU",traits:.button)
        XCTAssertTrue(b.isAccessibilityElement);XCTAssertEqual(b.accessibilityLabel,"Buscar");XCTAssertEqual(b.accessibilityHint,"Busca en todo CuadernU")
        XCTAssertTrue(b.constraints.contains{$0.firstAttribute == .height && $0.constant>=44});XCTAssertTrue(b.constraints.contains{$0.firstAttribute == .width && $0.constant>=44})
    }
    func testTextoDinamicoActivaEscalado(){let l=UILabel();AplicadorAccesibilidadIPad.textoDinamico(l,estilo:.body);XCTAssertTrue(l.adjustsFontForContentSizeCategory)}
    func testAtajosPredeterminadosNoColisionan(){XCTAssertTrue(AuditorAtajosIPad.colisiones().isEmpty)}
    func testAuditorDetectaColisionSinCrearOtraFuenteDeVerdad(){
        let a=AtajoIPad(accion:.importar,entrada:"x",modificadores:1),b=AtajoIPad(accion:.exportar,entrada:"X",modificadores:1)
        let c=AuditorAtajosIPad.colisiones([a,b]);XCTAssertEqual(c.count,1)
        XCTAssertEqual(c[0].acciones.map(\.rawValue).sorted(),[AccionIPad.exportar.rawValue,AccionIPad.importar.rawValue].sorted())
    }
}
