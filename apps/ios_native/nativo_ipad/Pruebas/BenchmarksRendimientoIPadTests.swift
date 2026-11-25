import XCTest
@testable import CuadernUEstudio

final class BenchmarksRendimientoIPadTests: XCTestCase {
    private func muestra(_ metrica: MetricaRendimientoCuadernUIPad,
                         _ valor: Double,
                         escenario: String = "500p",
                         dispositivo: String = "iPad",
                         sistema: String = "iPadOS") -> MuestraBenchmarkCuadernUIPad {
        .init(metrica: metrica, valor: valor, escenario: escenario,
              dispositivo: dispositivo, sistema: sistema, commit: "abc")
    }

    func testMedianaEsRobustaAOutlier() {
        XCTAssertEqual(EvaluadorBenchmarksCuadernUIPad.mediana([10, 10, 11, 12, 500]), 11)
        XCTAssertEqual(EvaluadorBenchmarksCuadernUIPad.mediana([10, 12]), 11)
    }

    func testLatenciaMayorQueBaselineEsRegresion() {
        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [muestra(.reaperturaDocumentoMs, 130)],
            baselines: [.init(metrica: .reaperturaDocumentoMs,
                              escenario: "500p", valor: 100,
                              toleranciaRegresionPorcentaje: 10)])
        XCTAssertFalse(reporte.aprobado)
        XCTAssertEqual(reporte.regresiones.count, 1)
        XCTAssertEqual(reporte.regresiones[0].degradacionPorcentaje, 30, accuracy: 0.001)
    }

    func testThroughputMenorQueBaselineEsRegresion() {
        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [muestra(.importacionMBPorSegundo, 40)],
            baselines: [.init(metrica: .importacionMBPorSegundo,
                              escenario: "500p", valor: 50,
                              toleranciaRegresionPorcentaje: 10)])
        XCTAssertFalse(reporte.aprobado)
        XCTAssertEqual(reporte.regresiones[0].degradacionPorcentaje, 20, accuracy: 0.001)
    }

    func testMejoraNoFallaGate() {
        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [muestra(.reaperturaDocumentoMs, 80)],
            baselines: [.init(metrica: .reaperturaDocumentoMs,
                              escenario: "500p", valor: 100,
                              toleranciaRegresionPorcentaje: 5)])
        XCTAssertTrue(reporte.aprobado)
    }

    func testEscenarioSinBaselineSeReportaSinInventarFallo() {
        let muestra = muestra(.busquedaP95Ms, 12, escenario: "curso-ingenieria")
        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [muestra], baselines: [])
        XCTAssertTrue(reporte.aprobado)
        XCTAssertEqual(reporte.sinBaseline, [muestra])
    }

    func testNoMezclaHardwareDistintoEnUnaMismaMediana() {
        let lento = muestra(.reaperturaDocumentoMs, 180,
                           dispositivo: "iPad13,8", sistema: "iPadOS 27.0")
        let rapido = muestra(.reaperturaDocumentoMs, 70,
                            dispositivo: "iPad16,3", sistema: "iPadOS 27.0")
        let baselineRapido = BaselineBenchmarkCuadernUIPad(
            metrica: .reaperturaDocumentoMs, escenario: "500p", valor: 80,
            toleranciaRegresionPorcentaje: 10,
            dispositivo: "iPad16,3", sistema: "iPadOS 27.0")

        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [lento, rapido], baselines: [baselineRapido])

        XCTAssertTrue(reporte.aprobado)
        XCTAssertEqual(reporte.sinBaseline, [lento])
    }

    func testBaselineEspecificoGanaSobreGenerico() {
        let actual = muestra(.latenciaPencilP95Ms, 15,
                            dispositivo: "iPad16,3", sistema: "iPadOS 27.0")
        let generico = BaselineBenchmarkCuadernUIPad(
            metrica: .latenciaPencilP95Ms, escenario: "500p", valor: 20,
            toleranciaRegresionPorcentaje: 5)
        let exacto = BaselineBenchmarkCuadernUIPad(
            metrica: .latenciaPencilP95Ms, escenario: "500p", valor: 10,
            toleranciaRegresionPorcentaje: 5,
            dispositivo: "iPad16,3", sistema: "iPadOS 27.0")

        let reporte = EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: [actual], baselines: [generico, exacto])
        XCTAssertFalse(reporte.aprobado)
        XCTAssertEqual(reporte.regresiones.first?.baseline, 10)
    }

    func testPromocionBaselineExigeVariasMuestrasDelMismoEntorno() throws {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("bench-promote-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioBenchmarksCuadernUIPad(base: base)
        for valor in [90.0, 100, 110, 95, 105] {
            repo.registrar(muestra(.reaperturaDocumentoMs, valor,
                                  dispositivo: "iPad16,3", sistema: "iPadOS 27.0"))
        }
        repo.registrar(muestra(.reaperturaDocumentoMs, 1,
                              dispositivo: "otro", sistema: "iPadOS 27.0"))

        let baseline = repo.fijarBaselineDesdeMuestras(
            metrica: .reaperturaDocumentoMs,
            escenario: "500p",
            dispositivo: "iPad16,3",
            sistema: "iPadOS 27.0",
            toleranciaRegresionPorcentaje: 10)
        XCTAssertEqual(baseline?.valor, 100)
        XCTAssertEqual(baseline?.dispositivo, "iPad16,3")
    }

    func testThroughputSeRegistraEnMBPorSegundo() throws {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("bench-throughput-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        let repo = RepositorioBenchmarksCuadernUIPad(base: base)
        repo.registrarThroughput(
            metrica: .exportacionMBPorSegundo,
            bytes: 10 * 1_048_576,
            duracionSegundos: 2,
            escenario: "export-10mb",
            dispositivo: "iPad",
            sistema: "iPadOS")
        repo.fijarBaseline(.init(
            metrica: .exportacionMBPorSegundo,
            escenario: "export-10mb",
            valor: 6,
            toleranciaRegresionPorcentaje: 10,
            dispositivo: "iPad",
            sistema: "iPadOS"))
        let reporte = repo.reporte(dispositivo: "iPad", sistema: "iPadOS")
        XCTAssertFalse(reporte.aprobado)
        XCTAssertEqual(reporte.regresiones.first?.actual ?? .nan, 5, accuracy: 0.001)
    }

    func testRepositorioPersisteBaselineYMuestras() throws {
        let base = FileManager.default.temporaryDirectory
            .appendingPathComponent("bench-\(UUID().uuidString)", isDirectory: true)
        defer { try? FileManager.default.removeItem(at: base) }
        do {
            let repo = RepositorioBenchmarksCuadernUIPad(base: base)
            repo.fijarBaseline(.init(metrica: .reaperturaDocumentoMs,
                                     escenario: "500p", valor: 100,
                                     toleranciaRegresionPorcentaje: 10))
            repo.registrar(muestra(.reaperturaDocumentoMs, 150))
        }
        let reabierto = RepositorioBenchmarksCuadernUIPad(base: base)
        XCTAssertFalse(reabierto.reporte().aprobado)
    }
}
