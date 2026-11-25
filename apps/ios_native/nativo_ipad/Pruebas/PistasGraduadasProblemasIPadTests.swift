import XCTest
@testable import CuadernUEstudio

final class PistasGraduadasProblemasIPadTests:XCTestCase {
    private func problema(activo:Bool=false,cerrado:Bool=false,solucion:String?="Paso 1: use KCL\nPaso 2: resuelva") -> ProblemaBancoIPad {
        var p=ProblemaBancoIPad(tema:"Circuitos",etiquetas:["kcl"],dificultad:.practica,enunciado:"Calcule la corriente I")
        if activo{p.intentos=[IntentoProblemaIPad(correcto:nil)]}
        else if cerrado{p.intentos=[IntentoProblemaIPad(correcto:false)]}
        if let solucion{p.solucion=SolucionVinculadaIPad(texto:solucion)}
        return p
    }
    func testPrimerosNivelesNuncaRevelanSolucion()throws{
        let p=problema(activo:true)
        for n in [NivelPistaProblemaIPad.orientar,.recordarConcepto,.sugerirMetodo,.diagnosticarError]{let pista=try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:n,formulas:[]);XCTAssertFalse(pista.revelaSolucion);XCTAssertFalse(pista.texto.contains("Paso 1"))}
    }
    func testReveladoParcialBloqueadoDuranteIntentoActivo(){let p=problema(activo:true);XCTAssertThrowsError(try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:.reveladoParcial,formulas:[])){XCTAssertEqual($0 as? ErrorPistaProblemaIPad,.nivelBloqueado)}}
    func testReveladoParcialBloqueadoSinIntentoCerrado(){let p=problema();XCTAssertFalse(GeneradorPistasGraduadasIPad.disponibles(problema:p).contains(.reveladoParcial));XCTAssertThrowsError(try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:.reveladoParcial,formulas:[]))}
    func testReveladoParcialSoloMuestraPrimeraLinea()throws{
        let p=problema(cerrado:true),pista=try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:.reveladoParcial,formulas:[])
        XCTAssertTrue(pista.revelaSolucion);XCTAssertTrue(pista.texto.contains("Paso 1"));XCTAssertFalse(pista.texto.contains("Paso 2"))
    }
    func testConceptoPriorizaFormulaFavoritaDelTema()throws{
        let p=problema(),f=FormulaTecnicaIPad(expresion:"ΣI = 0",nombre:"KCL",tema:"Circuitos",frecuenciaUso:2,favorita:true)
        let pista=try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:.recordarConcepto,formulas:[f]);XCTAssertTrue(pista.texto.contains("KCL"));XCTAssertTrue(pista.evidencia.contains("formula:\(f.id.uuidString)"))
    }
    func testDiagnosticoUsaDescripcionPeroNoCorreccion()throws{
        var p=problema();p.erroresDePaso=[PasoErrorProblemaIPad(descripcion:"Signo de corriente",respuestaDada:"-2",correccion:"+2")]
        let pista=try GeneradorPistasGraduadasIPad.pista(problema:p,nivel:.diagnosticarError,formulas:[]);XCTAssertTrue(pista.texto.contains("Signo de corriente"));XCTAssertFalse(pista.texto.contains("+2"))
    }
}
