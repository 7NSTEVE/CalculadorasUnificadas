import Foundation
import UIKit

enum EventoSoakCuadernUIPad: String, Codable, CaseIterable {
    case trazo
    case guardado
    case reapertura
    case importacion
    case exportacion
    case cambioPestana
    case memoriaBaja
    case error
}

struct MuestraSoakCuadernUIPad: Codable, Equatable {
    var fecha: Date
    var segundosDesdeInicio: TimeInterval
    var bytesResidentesEstimados: Int64?
    var bateria: Float?
    var lowPowerMode: Bool
    var thermal: String
    var contadores: [String: Int]
}

struct ReporteSoakCuadernUIPad: Codable, Equatable {
    var version = 1
    var id: UUID
    var nombre: String
    var inicio: Date
    var fin: Date
    var muestras: [MuestraSoakCuadernUIPad]
    var errores: [String]
    var notas: [String]

    var duracion: TimeInterval { fin.timeIntervalSince(inicio) }
    var maximoResidente: Int64? { muestras.compactMap(\.bytesResidentesEstimados).max() }
    var minimoResidente: Int64? { muestras.compactMap(\.bytesResidentesEstimados).min() }
    var crecimientoResidente: Int64? {
        guard let primero = muestras.compactMap(\.bytesResidentesEstimados).first,
              let ultimo = muestras.compactMap(\.bytesResidentesEstimados).last else { return nil }
        return ultimo - primero
    }
    var pasoSinErrores: Bool { errores.isEmpty }
}

protocol ProveedorMetricasSoakCuadernUIPad: AnyObject {
    func bytesResidentesEstimadosSoak() -> Int64?
}

final class MonitorSoakCuadernUIPad {
    static let compartido = MonitorSoakCuadernUIPad()
    static let actualizo = Notification.Name("CuadernU.Soak.Actualizo")

    private let bloqueo = NSRecursiveLock()
    private var timer: DispatchSourceTimer?
    private var id = UUID(), nombre = "", inicio = Date()
    private var muestras: [MuestraSoakCuadernUIPad] = []
    private var errores: [String] = [], notas: [String] = []
    private var contadores: [EventoSoakCuadernUIPad: Int] = [:]
    private weak var proveedor: ProveedorMetricasSoakCuadernUIPad?
    private var activo = false

    func iniciar(nombre: String,
                 intervalo: TimeInterval = 30,
                 proveedor: ProveedorMetricasSoakCuadernUIPad? = nil) {
        finalizarSinReporte()
        bloqueo.lock()
        id = UUID(); self.nombre = nombre.isEmpty ? "Soak CuadernU" : nombre
        inicio = Date(); muestras = []; errores = []; notas = []; contadores = [:]
        self.proveedor = proveedor; activo = true
        UIDevice.current.isBatteryMonitoringEnabled = true
        let t = DispatchSource.makeTimerSource(queue: DispatchQueue.global(qos: .utility))
        t.schedule(deadline: .now(), repeating: max(intervalo, 1))
        t.setEventHandler { [weak self] in self?.muestrear() }
        timer = t; bloqueo.unlock(); t.resume()
    }

    func registrar(_ evento: EventoSoakCuadernUIPad, cantidad: Int = 1) {
        bloqueo.lock(); contadores[evento, default: 0] += max(cantidad, 0); bloqueo.unlock()
    }
    func registrarError(_ texto: String) {
        bloqueo.lock(); errores.append(String(texto.prefix(1000))); contadores[.error, default: 0] += 1; bloqueo.unlock()
    }
    func anotar(_ texto: String) {
        bloqueo.lock(); notas.append(String(texto.prefix(1000))); bloqueo.unlock()
    }

    func instantanea() -> ReporteSoakCuadernUIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard activo else { return nil }
        return reporteSinBloqueo(fin: Date())
    }

    func finalizar() -> ReporteSoakCuadernUIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard activo else { return nil }
        timer?.cancel(); timer = nil; activo = false
        let reporte = reporteSinBloqueo(fin: Date())
        UIDevice.current.isBatteryMonitoringEnabled = false
        return reporte
    }

    @discardableResult
    func guardar(_ reporte: ReporteSoakCuadernUIPad,
                 base: URL? = nil,
                 archivos: FileManager = .default) -> URL? {
        let carpeta = base ?? archivos.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("QA/Soak", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        let url = carpeta.appendingPathComponent("soak-\(reporte.id.uuidString).json")
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? encoder.encode(reporte),
              (try? datos.write(to: url, options: [.atomic, .completeFileProtection])) != nil else { return nil }
        return url
    }

    private func muestrear() {
        bloqueo.lock(); guard activo else { bloqueo.unlock(); return }
        let ahora = Date(), bateria = UIDevice.current.batteryLevel
        let muestra = MuestraSoakCuadernUIPad(
            fecha: ahora,
            segundosDesdeInicio: ahora.timeIntervalSince(inicio),
            bytesResidentesEstimados: proveedor?.bytesResidentesEstimadosSoak(),
            bateria: bateria < 0 ? nil : bateria,
            lowPowerMode: ProcessInfo.processInfo.isLowPowerModeEnabled,
            thermal: Self.thermal(ProcessInfo.processInfo.thermalState),
            contadores: Dictionary(uniqueKeysWithValues: contadores.map { ($0.key.rawValue, $0.value) }))
        muestras.append(muestra)
        if muestras.count > 20_000 { muestras.removeFirst(muestras.count - 20_000) }
        bloqueo.unlock()
        DispatchQueue.main.async { NotificationCenter.default.post(name: Self.actualizo, object: self) }
    }

    private func finalizarSinReporte() {
        bloqueo.lock(); timer?.cancel(); timer = nil; activo = false; bloqueo.unlock()
    }
    private func reporteSinBloqueo(fin: Date) -> ReporteSoakCuadernUIPad {
        .init(id: id, nombre: nombre, inicio: inicio, fin: fin,
              muestras: muestras, errores: errores, notas: notas)
    }
    private static func thermal(_ estado: ProcessInfo.ThermalState) -> String {
        switch estado { case .nominal: return "nominal"; case .fair: return "fair"; case .serious: return "serious"; case .critical: return "critical"; @unknown default: return "unknown" }
    }
}

enum EvaluadorSoakCuadernUIPad {
    struct Criterios: Equatable {
        var duracionMinima: TimeInterval = 4 * 3600
        var maximoErrores = 0
        var crecimientoMemoriaMaximo: Int64 = 384 * 1_024 * 1_024
        var permitirThermalCritical = false
    }

    static func fallos(_ reporte: ReporteSoakCuadernUIPad,
                       criterios: Criterios = .init()) -> [String] {
        var fallos: [String] = []
        if reporte.duracion < criterios.duracionMinima { fallos.append("Duración insuficiente") }
        if reporte.errores.count > criterios.maximoErrores { fallos.append("Errores registrados: \(reporte.errores.count)") }
        if let crecimiento = reporte.crecimientoResidente,
           crecimiento > criterios.crecimientoMemoriaMaximo { fallos.append("Crecimiento de memoria excesivo") }
        if !criterios.permitirThermalCritical,
           reporte.muestras.contains(where: { $0.thermal == "critical" }) { fallos.append("Thermal critical") }
        return fallos
    }
}
