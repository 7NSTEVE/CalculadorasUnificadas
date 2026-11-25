import Darwin
import Foundation

struct EntornoBenchmarkFisicoIPad: Equatable {
    var dispositivo: String
    var sistema: String

    static var actual: EntornoBenchmarkFisicoIPad {
        var info = utsname()
        let resultado = uname(&info)
        let maquina: String
        if resultado == 0 {
            maquina = withUnsafePointer(to: &info.machine) { puntero in
                puntero.withMemoryRebound(to: CChar.self, capacity: 1) {
                    String(cString: $0)
                }
            }
        } else {
            maquina = "iPad-desconocido"
        }
        return .init(
            dispositivo: maquina.isEmpty ? "iPad-desconocido" : maquina,
            sistema: ProcessInfo.processInfo.operatingSystemVersionString)
    }
}

/// Agrupa latencias interactivas en ventanas antes de escribir al repositorio de
/// benchmarks. La métrica es P95 real de una ventana, no una muestra individual
/// etiquetada engañosamente como P95. Tampoco escribe al disco por cada tecla.
final class TelemetriaBusquedaUniversalIPad {
    static let compartido = TelemetriaBusquedaUniversalIPad()

    private let bloqueo = NSLock()
    private let repositorio: RepositorioBenchmarksCuadernUIPad
    private let entorno: () -> EntornoBenchmarkFisicoIPad
    private var ventanas: [String: [Double]] = [:]
    let muestrasPorVentana: Int

    init(
        repositorio: RepositorioBenchmarksCuadernUIPad = .compartido,
        muestrasPorVentana: Int = 20,
        entorno: @escaping () -> EntornoBenchmarkFisicoIPad = {
            EntornoBenchmarkFisicoIPad.actual
        }
    ) {
        self.repositorio = repositorio
        self.muestrasPorVentana = max(muestrasPorVentana, 5)
        self.entorno = entorno
    }

    func registrar(duracionMs: Double, elementosIndexados: Int) {
        guard duracionMs.isFinite, duracionMs >= 0 else { return }
        let escenario = Self.escenario(elementosIndexados: elementosIndexados)
        bloqueo.lock()
        var valores = ventanas[escenario] ?? []
        valores.append(duracionMs)
        guard valores.count >= muestrasPorVentana else {
            ventanas[escenario] = valores
            bloqueo.unlock()
            return
        }
        ventanas[escenario] = []
        bloqueo.unlock()

        let p95 = Self.percentil(valores, 0.95)
        guard p95.isFinite else { return }
        let actual = entorno()
        repositorio.registrar(.init(
            metrica: .busquedaP95Ms,
            valor: p95,
            escenario: escenario,
            dispositivo: actual.dispositivo,
            sistema: actual.sistema,
            commit: nil))
    }

    static func escenario(elementosIndexados: Int) -> String {
        let cantidad = max(elementosIndexados, 0)
        switch cantidad {
        case 0...100: return "busqueda-universal-0-100"
        case 101...1_000: return "busqueda-universal-101-1000"
        case 1_001...10_000: return "busqueda-universal-1001-10000"
        default: return "busqueda-universal-10000+"
        }
    }

    static func percentil(_ valores: [Double], _ fraccion: Double) -> Double {
        let ordenados = valores.filter { $0.isFinite && $0 >= 0 }.sorted()
        guard !ordenados.isEmpty else { return .nan }
        if ordenados.count == 1 { return ordenados[0] }
        let p = min(max(fraccion, 0), 1)
        let posicion = p * Double(ordenados.count - 1)
        let inferior = Int(floor(posicion))
        let superior = Int(ceil(posicion))
        guard inferior != superior else { return ordenados[inferior] }
        let peso = posicion - Double(inferior)
        return ordenados[inferior] * (1 - peso) + ordenados[superior] * peso
    }
}
