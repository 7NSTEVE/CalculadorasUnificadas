import XCTest
@testable import CuadernUEstudio

final class SoakTestingIPadTests: XCTestCase {
    private func r(_ dur: TimeInterval = 14_400,
                   _ ini: Int64 = 100,
                   _ fin: Int64 = 200,
                   errores: [String] = [], thermal: String = "nominal") -> ReporteSoakCuadernUIPad {
        let t = Date(timeIntervalSince1970: 1000)
        return .init(id: UUID(), nombre: "test", inicio: t, fin: t.addingTimeInterval(dur), muestras: [
            .init(fecha: t, segundosDesdeInicio: 0, bytesResidentesEstimados: ini, bateria: 1,
                  lowPowerMode: false, thermal: "nominal", contadores: [:]),
            .init(fecha: t.addingTimeInterval(dur), segundosDesdeInicio: dur,
                  bytesResidentesEstimados: fin, bateria: 0.8,
                  lowPowerMode: false, thermal: thermal, contadores: [:])], errores: errores, notas: [])
    }

    func testCrecimientoYSoakLimpio() {
        let reporte = r()
        XCTAssertEqual(reporte.crecimientoResidente, 100)
        XCTAssertTrue(EvaluadorSoakCuadernUIPad.fallos(reporte).isEmpty)
    }

    func testDetectaDuracionErrorMemoriaYThermal() {
        let reporte = r(60, 0, 500 * 1_024 * 1_024, errores: ["save"], thermal: "critical")
        let f = EvaluadorSoakCuadernUIPad.fallos(reporte)
        XCTAssertTrue(f.contains("Duración insuficiente"))
        XCTAssertTrue(f.contains(where: { $0.contains("Errores registrados") }))
        XCTAssertTrue(f.contains("Crecimiento de memoria excesivo"))
        XCTAssertTrue(f.contains("Thermal critical"))
    }

    func testGuardarProduceJSONDecodificable() throws {
        let base = FileManager.default.temporaryDirectory.appendingPathComponent("soak-\(UUID())", isDirectory: true)
        guard let url = MonitorSoakCuadernUIPad().guardar(r(), base: base) else { return XCTFail("No guardó") }
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        XCTAssertNoThrow(try d.decode(ReporteSoakCuadernUIPad.self, from: Data(contentsOf: url)))
    }
}
