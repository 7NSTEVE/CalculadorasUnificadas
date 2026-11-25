import XCTest
@testable import CuadernUEstudio

final class StudySetsCSVIPadTests: XCTestCase {
    func testCSVConCabecera() throws {
        let filas = try ParserStudySetCSVIPad.analizar(
            "pregunta,respuesta,tags\nLey de Ohm,V=IR,circuitos;basico\n")
        XCTAssertEqual(filas.count, 1)
        XCTAssertEqual(filas[0].pregunta, "Ley de Ohm")
        XCTAssertEqual(filas[0].respuesta, "V=IR")
        XCTAssertEqual(filas[0].etiquetas, ["circuitos", "basico"])
    }

    func testTSVSeDetectaAutomaticamente() throws {
        let filas = try ParserStudySetCSVIPad.analizar(
            "question\tanswer\nKCL\tSuma algebraica de corrientes\n")
        XCTAssertEqual(filas.first?.pregunta, "KCL")
    }

    func testCamposConComasYComillasEscapadas() throws {
        let filas = try ParserStudySetCSVIPad.analizar(
            "front,back\n\"R, L y C\",\"Dice \"\"fasor\"\"\"\n")
        XCTAssertEqual(filas.first?.pregunta, "R, L y C")
        XCTAssertEqual(filas.first?.respuesta, "Dice \"fasor\"")
    }

    func testSaltoDeLineaDentroDeCampo() throws {
        let filas = try ParserStudySetCSVIPad.analizar(
            "front,back\nPregunta,\"línea 1\nlínea 2\"\n")
        XCTAssertTrue(filas.first?.respuesta.contains("línea 2") == true)
    }

    func testSinCabeceraUsaDosPrimerasColumnas() throws {
        let filas = try ParserStudySetCSVIPad.analizar("KVL,Suma de voltajes\nKCL,Suma de corrientes")
        XCTAssertEqual(filas.count, 2)
    }

    func testFilaIncompletaSeOmite() throws {
        let filas = try ParserStudySetCSVIPad.analizar(
            "front,back\nOK,Respuesta\nSolo pregunta,\n")
        XCTAssertEqual(filas.count, 1)
    }
}
