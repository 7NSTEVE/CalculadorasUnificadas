import XCTest
@testable import CuadernUEstudio

final class AnaliticaErroresFramesIPadTests: XCTestCase {
    func testAgrupaMayusculasAcentosYEspacios() {
        let base = Date(timeIntervalSince1970: 100)
        var uno = EstadoEstudioFrameIPad()
        let a = uno.iniciar(ahora: base)
        _ = uno.cerrar(
            intentoId: a, resultado: .repetir, confianza: 0.2,
            error: "Cambié   el signo de la corriente",
            ahora: base.addingTimeInterval(10))

        var dos = EstadoEstudioFrameIPad()
        let b = dos.iniciar(ahora: base.addingTimeInterval(20))
        _ = dos.cerrar(
            intentoId: b, resultado: .repetir, confianza: 0.4,
            error: "cambie el SIGNO de la corriente",
            ahora: base.addingTimeInterval(30))

        let salida = AnalizadorErroresFramesIPad.recurrentes([
            EntradaAnaliticaFrameIPad(frameId: UUID(), titulo: "Ejercicio 1", estudio: uno),
            EntradaAnaliticaFrameIPad(frameId: UUID(), titulo: "Ejercicio 2", estudio: dos),
        ])

        XCTAssertEqual(salida.count, 1)
        XCTAssertEqual(salida[0].apariciones, 2)
        XCTAssertEqual(salida[0].frameIds.count, 2)
        XCTAssertEqual(salida[0].confianzaPromedio, 0.3, accuracy: 0.001)
    }

    func testIgnoraAciertosYErroresVacios() {
        let base = Date(timeIntervalSince1970: 200)
        var estudio = EstadoEstudioFrameIPad()
        let correcto = estudio.iniciar(ahora: base)
        _ = estudio.cerrar(
            intentoId: correcto, resultado: .correcto, confianza: 0.9,
            error: "No debería contar", ahora: base.addingTimeInterval(10))
        let vacio = estudio.iniciar(ahora: base.addingTimeInterval(20))
        _ = estudio.cerrar(
            intentoId: vacio, resultado: .repetir, confianza: 0.1,
            error: "   ", ahora: base.addingTimeInterval(30))

        XCTAssertTrue(AnalizadorErroresFramesIPad.recurrentes([
            EntradaAnaliticaFrameIPad(frameId: UUID(), titulo: "Frame", estudio: estudio)
        ]).isEmpty)
    }

    func testOrdenaPorFrecuenciaAntesQueRecencia() {
        let base = Date(timeIntervalSince1970: 300)
        var frecuente = EstadoEstudioFrameIPad()
        for indice in 0..<2 {
            let intento = frecuente.iniciar(
                ahora: base.addingTimeInterval(Double(indice * 20)))
            _ = frecuente.cerrar(
                intentoId: intento, resultado: .repetir, confianza: 0.5,
                error: "Olvidé la polaridad",
                ahora: base.addingTimeInterval(Double(indice * 20 + 10)))
        }

        var reciente = EstadoEstudioFrameIPad()
        let intento = reciente.iniciar(ahora: base.addingTimeInterval(1_000))
        _ = reciente.cerrar(
            intentoId: intento, resultado: .repetir, confianza: 0.5,
            error: "Usé mal la unidad", ahora: base.addingTimeInterval(1_010))

        let salida = AnalizadorErroresFramesIPad.recurrentes([
            EntradaAnaliticaFrameIPad(frameId: UUID(), titulo: "A", estudio: frecuente),
            EntradaAnaliticaFrameIPad(frameId: UUID(), titulo: "B", estudio: reciente),
        ])

        XCTAssertEqual(salida.first?.descripcion, "Olvidé la polaridad")
        XCTAssertEqual(salida.first?.apariciones, 2)
    }
}
