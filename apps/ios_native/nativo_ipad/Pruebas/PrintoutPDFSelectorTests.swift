import XCTest
@testable import CuadernUEstudio

final class PrintoutPDFSelectorTests: XCTestCase {
    func testTodasSeleccionaElDocumentoCompleto() {
        XCTAssertEqual(
            SelectorPaginasPrintoutIPad.seleccionar(.todas, total: 7),
            IndexSet(integersIn: 0..<7))
    }

    func testPrimerasCincoYDiezSeLimitanAlTotalReal() {
        XCTAssertEqual(
            SelectorPaginasPrintoutIPad.seleccionar(.primeras5, total: 3),
            IndexSet(integersIn: 0..<3))
        XCTAssertEqual(
            SelectorPaginasPrintoutIPad.seleccionar(.primeras10, total: 13),
            IndexSet(integersIn: 0..<10))
    }

    func testImparesYParesUsanNumeracionVisibleUnoBased() {
        XCTAssertEqual(
            SelectorPaginasPrintoutIPad.seleccionar(.impares, total: 7),
            IndexSet([0, 2, 4, 6]),
            "Páginas visibles 1, 3, 5 y 7 deben mapear a índices 0, 2, 4 y 6")
        XCTAssertEqual(
            SelectorPaginasPrintoutIPad.seleccionar(.pares, total: 7),
            IndexSet([1, 3, 5]),
            "Páginas visibles 2, 4 y 6 deben mapear a índices 1, 3 y 5")
    }

    func testRangoAceptaListasRangosInvertidosYGuionesTipograficos() {
        let seleccion = SelectorPaginasPrintoutIPad.seleccionar(
            .rango,
            total: 15,
            rangoPersonalizado: "1–3, 8, 12-10; 15")
        XCTAssertEqual(seleccion, IndexSet([0, 1, 2, 7, 9, 10, 11, 14]))
    }

    func testRangoRecortaFueraDeLimitesEIgnoraBasura() {
        let seleccion = SelectorPaginasPrintoutIPad.parsear(
            "0-2, texto, 4, 99, 6—8",
            total: 7)
        XCTAssertEqual(seleccion, IndexSet([0, 1, 3, 5, 6]))
    }

    func testDocumentoVacioNuncaProduceSeleccion() {
        for preset in PresetPaginasPrintoutIPad.allCases {
            XCTAssertTrue(
                SelectorPaginasPrintoutIPad.seleccionar(
                    preset, total: 0, rangoPersonalizado: "1-99").isEmpty)
        }
    }

    func testPresetsDeEspacioParaNotasSonMonotonicos() {
        let valores = EspacioNotasPrintoutIPad.allCases.map(\.puntos)
        XCTAssertEqual(valores, [0, 180, 320, 520])
        XCTAssertEqual(valores, valores.sorted())
    }
}
