import XCTest

final class EdicionManuscritaSemanticaIPadTests: XCTestCase {
    func testAgrupaDosPalabrasEnUnaMismaLinea() {
        let trazos = [
            t(0, 10, 10, 8, 22), t(1, 21, 11, 7, 21),
            t(2, 31, 10, 8, 22),
            t(3, 63, 10, 9, 22), t(4, 75, 11, 8, 21),
        ]
        let palabras = AnalizadorEstructuraManuscritaIPad.grupos(
            trazos, nivel: .palabra)
        XCTAssertEqual(palabras.count, 2)
        XCTAssertEqual(palabras[0].trazos, [0, 1, 2])
        XCTAssertEqual(palabras[1].trazos, [3, 4])
    }

    func testSeparaLineasAunqueSusRangosXCoincidan() {
        let trazos = [
            t(0, 10, 10, 12, 20), t(1, 25, 11, 10, 19),
            t(2, 11, 58, 10, 20), t(3, 24, 59, 12, 19),
        ]
        let lineas = AnalizadorEstructuraManuscritaIPad.grupos(
            trazos, nivel: .linea)
        XCTAssertEqual(lineas.count, 2)
        XCTAssertEqual(lineas[0].trazos, [0, 1])
        XCTAssertEqual(lineas[1].trazos, [2, 3])
    }

    func testHitTestEligePalabraTocada() {
        let trazos = [
            t(0, 0, 0, 10, 20), t(1, 13, 0, 10, 20),
            t(2, 55, 0, 10, 20), t(3, 68, 0, 10, 20),
        ]
        let grupo = AnalizadorEstructuraManuscritaIPad.grupo(
            cerca: CGPoint(x: 62, y: 10), trazos: trazos, nivel: .palabra)
        XCTAssertEqual(grupo?.trazos, [2, 3])
    }

    func testHitTestNoSeleccionaAlgoMuyLejano() {
        let trazos = [t(0, 0, 0, 10, 20)]
        XCTAssertNil(AnalizadorEstructuraManuscritaIPad.grupo(
            cerca: CGPoint(x: 500, y: 500), trazos: trazos, nivel: .palabra))
    }

    func testDescartaMarcosInvalidosSinContaminarResultado() {
        let trazos = [
            t(0, 0, 0, 10, 20),
            TrazoManuscritoIPad(indice: 1,
                marco: CGRect(x: .infinity, y: 0, width: 10, height: 10)),
            TrazoManuscritoIPad(indice: 2,
                marco: CGRect(x: 0, y: 0, width: 0, height: 10)),
        ]
        let lineas = AnalizadorEstructuraManuscritaIPad.grupos(
            trazos, nivel: .linea)
        XCTAssertEqual(lineas.count, 1)
        XCTAssertEqual(lineas[0].trazos, [0])
    }

    func testEscalaLinealNoFusionaCienLineas() {
        var trazos: [TrazoManuscritoIPad] = []
        for linea in 0..<100 {
            for columna in 0..<12 {
                trazos.append(t(
                    linea * 12 + columna,
                    CGFloat(columna * 12), CGFloat(linea * 42), 8, 18))
            }
        }
        let lineas = AnalizadorEstructuraManuscritaIPad.grupos(
            trazos, nivel: .linea)
        XCTAssertEqual(lineas.count, 100)
        XCTAssertTrue(lineas.allSatisfy { $0.trazos.count == 12 })
    }

    private func t(_ indice: Int, _ x: CGFloat, _ y: CGFloat,
                   _ w: CGFloat, _ h: CGFloat) -> TrazoManuscritoIPad {
        TrazoManuscritoIPad(
            indice: indice, marco: CGRect(x: x, y: y, width: w, height: h))
    }
}
