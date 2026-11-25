import XCTest
@testable import CuadernUEstudio

final class GateComparativoGoodnotesIPadTests: XCTestCase {
    private let ahora = Date(timeIntervalSince1970: 1_800_000_000)

    private func serie(
        _ nombre: String,
        cuadernU: [Double],
        goodnotes: [Double],
        direccion: DireccionMetricaComparativaIPad = .menorEsMejor,
        fecha: Date? = nil
    ) -> SerieComparativaRendimientoIPad {
        .init(
            metrica: nombre,
            escenario: "cuaderno-500-paginas",
            dispositivo: "iPad-Pro-M4",
            sistema: "iPadOS-27",
            direccion: direccion,
            cuadernU: cuadernU,
            goodnotes: goodnotes,
            fecha: fecha ?? ahora)
    }

    private func funcion(
        _ id: String,
        goodnotes: Bool = true,
        estado: EstadoFuncionComparativaIPad = .verificada,
        diferenciador: Bool = false,
        fecha: Date? = nil
    ) -> EvidenciaFuncionComparativaIPad {
        .init(
            id: id,
            nombre: id,
            categoria: "producto",
            disponibleEnGoodnotes: goodnotes,
            estadoCuadernU: estado,
            diferenciadorCuadernU: diferenciador,
            evidencia: "XCTest/e2e",
            fecha: fecha ?? ahora)
    }

    private func politica(minimoFunciones: Int) -> PoliticaSuperioridadGoodnotesIPad {
        var p = PoliticaSuperioridadGoodnotesIPad()
        p.minimoFuncionesGoodnotesComparadas = minimoFunciones
        return p
    }

    func testNoPermiteSuperioridadConCodigoParcialAunqueRendimientoSeaMejor() {
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71, 69, 72, 70],
                  goodnotes: [100, 102, 98, 101, 99]),
            serie("busqueda", cuadernU: [30, 31, 29, 30, 32],
                  goodnotes: [40, 42, 41, 39, 40]),
            serie("memoria", cuadernU: [400, 405, 398, 402, 399],
                  goodnotes: [500, 510, 490, 505, 495]),
        ]
        let funciones = [
            funcion("tinta"),
            funcion("pdf", estado: .parcial),
            funcion("circuitos", goodnotes: false, diferenciador: true),
            funcion("casio", goodnotes: false, diferenciador: true),
        ]

        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento,
            funciones: funciones,
            politica: politica(minimoFunciones: 2),
            ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .noSupera)
        XCTAssertFalse(reporte.permiteAfirmarSuperioridad)
        XCTAssertEqual(reporte.funcionesGoodnotesNoIgualadas.map(\.id), ["pdf"])
    }

    func testEvidenciaInsuficienteSiFaltanMuestrasOComparaciones() {
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71],
                  goodnotes: [100, 101]),
        ]
        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento,
            funciones: [funcion("tinta")],
            ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .evidenciaInsuficiente)
        XCTAssertTrue(reporte.faltantes.contains { $0.contains("faltan muestras") })
        XCTAssertTrue(reporte.faltantes.contains { $0.contains("Rendimiento") })
    }

    func testUnaRegresionImportanteImpideAfirmarSuperioridad() {
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71, 69, 72, 70],
                  goodnotes: [100, 102, 98, 101, 99]),
            serie("busqueda", cuadernU: [30, 31, 29, 30, 32],
                  goodnotes: [40, 42, 41, 39, 40]),
            serie("memoria", cuadernU: [550, 555, 545, 552, 548],
                  goodnotes: [500, 510, 490, 505, 495]),
        ]
        let funciones = [
            funcion("tinta"), funcion("pdf"),
            funcion("circuitos", goodnotes: false, diferenciador: true),
            funcion("casio", goodnotes: false, diferenciador: true),
        ]

        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento,
            funciones: funciones,
            politica: politica(minimoFunciones: 2),
            ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .noSupera)
        XCTAssertTrue(reporte.comparaciones.contains(where: \.regresionInaceptable))
    }

    func testSoloSuperaConParidadVentajaYDiferenciadoresExclusivosVerificados() {
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71, 69, 72, 70],
                  goodnotes: [100, 102, 98, 101, 99]),
            serie("busqueda", cuadernU: [30, 31, 29, 30, 32],
                  goodnotes: [40, 42, 41, 39, 40]),
            serie("memoria", cuadernU: [450, 451, 449, 452, 448],
                  goodnotes: [455, 456, 454, 457, 453]),
        ]
        let funciones = [
            funcion("tinta"), funcion("pdf"), funcion("audio"),
            funcion("circuitos", goodnotes: false, diferenciador: true),
            funcion("casio", goodnotes: false, diferenciador: true),
            funcion("busqueda", goodnotes: true, diferenciador: true),
        ]

        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento,
            funciones: funciones,
            politica: politica(minimoFunciones: 3),
            ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .supera)
        XCTAssertTrue(reporte.permiteAfirmarSuperioridad)
        XCTAssertTrue(reporte.funcionesGoodnotesNoIgualadas.isEmpty)
        XCTAssertEqual(reporte.diferenciadoresVerificados.map(\.id),
                       ["casio", "circuitos"])
    }

    func testMatrizConvenientementePequenaNoPuedeProbarSuperioridad() {
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71, 69, 72, 70],
                  goodnotes: [100, 102, 98, 101, 99]),
            serie("busqueda", cuadernU: [30, 31, 29, 30, 32],
                  goodnotes: [40, 42, 41, 39, 40]),
            serie("memoria", cuadernU: [400, 405, 398, 402, 399],
                  goodnotes: [500, 510, 490, 505, 495]),
        ]
        let funciones = [
            funcion("tinta"), funcion("pdf"),
            funcion("circuitos", goodnotes: false, diferenciador: true),
            funcion("casio", goodnotes: false, diferenciador: true),
        ]

        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento, funciones: funciones, ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .evidenciaInsuficiente)
        XCTAssertTrue(reporte.faltantes.contains {
            $0.contains("capacidades Goodnotes comparadas")
        })
    }

    func testEvidenciaVencidaNoCuentaParaClaim() {
        let vieja = ahora.addingTimeInterval(-100 * 86_400)
        let rendimiento = [
            serie("apertura", cuadernU: [70, 71, 69, 72, 70],
                  goodnotes: [100, 102, 98, 101, 99], fecha: vieja),
            serie("busqueda", cuadernU: [30, 31, 29, 30, 32],
                  goodnotes: [40, 42, 41, 39, 40], fecha: vieja),
            serie("memoria", cuadernU: [400, 405, 398, 402, 399],
                  goodnotes: [500, 510, 490, 505, 495], fecha: vieja),
        ]
        let reporte = GateSuperioridadGoodnotesIPad.evaluar(
            rendimiento: rendimiento,
            funciones: [
                funcion("tinta", fecha: vieja),
                funcion("circuitos", goodnotes: false,
                        diferenciador: true, fecha: vieja),
                funcion("casio", goodnotes: false,
                        diferenciador: true, fecha: vieja),
            ],
            ahora: ahora)

        XCTAssertEqual(reporte.veredicto, .evidenciaInsuficiente)
        XCTAssertTrue(reporte.faltantes.contains { $0.contains("vencid") })
    }
}
