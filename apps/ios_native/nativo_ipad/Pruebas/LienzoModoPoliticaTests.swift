import XCTest
@testable import CuadernUEstudio

final class LienzoModoPoliticaTests: XCTestCase {
    func testModosFijosMantienenDosEjesFinitos() {
        for modo in [ModoPaginaLienzo.libro, .paginadoFijo] {
            let politica = PoliticaModoLienzoIPad.para(modo)
            XCTAssertFalse(politica.anchoInfinito)
            XCTAssertFalse(politica.altoInfinito)
            XCTAssertTrue(politica.tieneEjeFinito)
        }
    }

    func testHibridoYPresentacionConservanAnchoFijoYAltoInfinito() {
        for modo in [ModoPaginaLienzo.hibrido, .presentacion] {
            let politica = PoliticaModoLienzoIPad.para(modo)
            XCTAssertFalse(politica.anchoInfinito)
            XCTAssertTrue(politica.altoInfinito)
            XCTAssertTrue(politica.tieneEjeFinito)
        }
    }

    func testHorizontalInfinitoConservaSoloElAltoFijo() {
        let politica = PoliticaModoLienzoIPad.para(.horizontalInfinito)
        XCTAssertTrue(politica.anchoInfinito)
        XCTAssertFalse(politica.altoInfinito)
        XCTAssertTrue(politica.tieneEjeFinito)
    }

    func testPizarraEInfinitoXYNoExponenTiradoresDeTamano() {
        for modo in [ModoPaginaLienzo.infinitoXY, .pizarra] {
            let politica = PoliticaModoLienzoIPad.para(modo)
            XCTAssertTrue(politica.anchoInfinito)
            XCTAssertTrue(politica.altoInfinito)
            XCTAssertFalse(politica.tieneEjeFinito)
        }
    }

    func testPresetsInfinitosEligenElModoEsperado() {
        XCTAssertEqual(PreajusteLienzo.a4InfinitoVertical.modoSugerido, .hibrido)
        XCTAssertEqual(PreajusteLienzo.cartaInfinitaVertical.modoSugerido, .hibrido)
        XCTAssertEqual(PreajusteLienzo.pizarraInfinita.modoSugerido, .infinitoXY)
        XCTAssertEqual(PreajusteLienzo.circuitosPanoramicos.modoSugerido,
                       .horizontalInfinito)
    }

    func testPresetsClasicosNoCambianElModoPorSorpresa() {
        let clasicos: [PreajusteLienzo] = [
            .a4, .a3, .a5, .carta, .oficio, .dieciseisNueve, .dieciseisDiez,
            .cuatroTres, .ipad, .pantallaDividida, .ingenieria, .pdfNotas, .pptNotas,
        ]
        for preset in clasicos {
            XCTAssertNil(preset.modoSugerido, "\(preset.nombre) no debe cambiar el modo")
        }
    }

    func testA4EnMilimetrosCoincideConPixelesDelNucleoA150PPP() {
        let ancho = ConversorUnidadLienzoIPad.convertir(
            210, de: .milimetros, a: .pixeles)
        let alto = ConversorUnidadLienzoIPad.convertir(
            297, de: .milimetros, a: .pixeles)
        XCTAssertEqual(ancho, 1240.1575, accuracy: 0.01)
        XCTAssertEqual(alto, 1753.937, accuracy: 0.01)
    }

    func testDosPuntoCincuentaYCuatroCentimetrosSonUnaPulgada() {
        let pulgadas = ConversorUnidadLienzoIPad.convertir(
            2.54, de: .centimetros, a: .pulgadas)
        XCTAssertEqual(pulgadas, 1, accuracy: 0.0001)
    }

    func testCambiarPxAMmNoConservaElNumeroCrudo() {
        let milimetros = ConversorUnidadLienzoIPad.convertir(
            1240, de: .pixeles, a: .milimetros)
        XCTAssertEqual(milimetros, 209.9733, accuracy: 0.01)
        XCTAssertNotEqual(milimetros, 1240)
    }

    func testConversionDeIdaYVueltaConservaLaDimensionFisica() {
        let original: Float = 237
        let pixeles = ConversorUnidadLienzoIPad.convertir(
            original, de: .milimetros, a: .pixeles)
        let regreso = ConversorUnidadLienzoIPad.convertir(
            pixeles, de: .pixeles, a: .milimetros)
        XCTAssertEqual(regreso, original, accuracy: 0.001)
    }

    func testEspaciosRapidosMantienenOrdenFisico() {
        let centimetros = EspacioRapidoLienzoIPad.allCases.map(\.centimetros)
        XCTAssertEqual(centimetros, [1, 3, 5, 10])
        XCTAssertEqual(centimetros, centimetros.sorted())
    }

    func testTresCentimetrosUsanLaMismaEscalaDelNucleo() {
        XCTAssertEqual(EspacioRapidoLienzoIPad.cm3.pixeles,
                       177.16536, accuracy: 0.001)
        XCTAssertEqual(EspacioRapidoLienzoIPad.cm10.pixeles,
                       590.5512, accuracy: 0.001)
    }
}
