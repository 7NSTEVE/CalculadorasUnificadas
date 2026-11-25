import Darwin
import Foundation
import ObjectiveC
import UIKit

struct SnapshotQAPencilFisicoIPad: Codable, Equatable {
    var fecha = Date()
    var p50GPUms: Double
    var p95GPUms: Double
    var p99GPUms: Double
    var maximaGPUms: Double
    var framesDiferidos: Int
    var porcentajeFramesPerdidos: Double
    var refrescoHz: Int
    var estadoTermico: String
}

/// Sesión física de QA de Pencil. Las sesiones nuevas usan el UUID durable del
/// documento. `documentoClaveLegacy` sólo existe para leer archivos v1 que
/// guardaban la clave UInt64 de compatibilidad; nunca se usa para identificar un
/// documento nuevo ni se intenta reconstruir un UUID desde ella.
struct SesionQAPencilFisicoIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var documentoId: DocumentoPersistenteID?
    var documentoClaveLegacy: UInt64?
    var dispositivo: String
    var sistema: String
    var inicio = Date()
    var fin: Date?
    var snapshots: [SnapshotQAPencilFisicoIPad] = []
    var actualizacionesRenderer = 0
    var ticksDisplay = 0
    var framesPerdidosEstimados = 0
    var notas: [String] = []

    init(id: UUID = UUID(),
         documentoId: DocumentoPersistenteID,
         dispositivo: String,
         sistema: String,
         inicio: Date = Date(),
         fin: Date? = nil,
         snapshots: [SnapshotQAPencilFisicoIPad] = [],
         actualizacionesRenderer: Int = 0,
         ticksDisplay: Int = 0,
         framesPerdidosEstimados: Int = 0,
         notas: [String] = []) {
        self.id = id
        self.documentoId = documentoId
        self.documentoClaveLegacy = nil
        self.dispositivo = dispositivo
        self.sistema = sistema
        self.inicio = inicio
        self.fin = fin
        self.snapshots = snapshots
        self.actualizacionesRenderer = actualizacionesRenderer
        self.ticksDisplay = ticksDisplay
        self.framesPerdidosEstimados = framesPerdidosEstimados
        self.notas = notas
    }

    private init(id: UUID,
                 documentoId: DocumentoPersistenteID?,
                 documentoClaveLegacy: UInt64?,
                 dispositivo: String,
                 sistema: String,
                 inicio: Date,
                 fin: Date?,
                 snapshots: [SnapshotQAPencilFisicoIPad],
                 actualizacionesRenderer: Int,
                 ticksDisplay: Int,
                 framesPerdidosEstimados: Int,
                 notas: [String]) {
        self.id = id
        self.documentoId = documentoId
        self.documentoClaveLegacy = documentoClaveLegacy
        self.dispositivo = dispositivo
        self.sistema = sistema
        self.inicio = inicio
        self.fin = fin
        self.snapshots = snapshots
        self.actualizacionesRenderer = actualizacionesRenderer
        self.ticksDisplay = ticksDisplay
        self.framesPerdidosEstimados = framesPerdidosEstimados
        self.notas = notas
    }

    var duracion: TimeInterval {
        max(0, (fin ?? Date()).timeIntervalSince(inicio))
    }

    var ultimo: SnapshotQAPencilFisicoIPad? { snapshots.last }

    /// La ventana interna del renderer conserva 240 muestras. Publicar una
    /// comparación antes de haberla renovado suficientemente mezclaría datos de
    /// una ráfaga anterior con la actual.
    var aptaParaBenchmark: Bool {
        actualizacionesRenderer >= 240 && duracion >= 3 && ultimo != nil
    }

    private enum CodingKeys: String, CodingKey {
        case id, documentoId, dispositivo, sistema, inicio, fin, snapshots
        case actualizacionesRenderer, ticksDisplay, framesPerdidosEstimados, notas
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        let id = try c.decodeIfPresent(UUID.self, forKey: .id) ?? UUID()
        let durable = try? c.decode(DocumentoPersistenteID.self, forKey: .documentoId)
        let legacy: UInt64?
        if durable == nil {
            legacy = try? c.decode(UInt64.self, forKey: .documentoId)
        } else {
            legacy = nil
        }
        self.init(
            id: id,
            documentoId: durable,
            documentoClaveLegacy: legacy,
            dispositivo: try c.decodeIfPresent(String.self, forKey: .dispositivo) ?? "desconocido",
            sistema: try c.decodeIfPresent(String.self, forKey: .sistema) ?? "desconocido",
            inicio: try c.decodeIfPresent(Date.self, forKey: .inicio) ?? Date(),
            fin: try c.decodeIfPresent(Date.self, forKey: .fin),
            snapshots: try c.decodeIfPresent([SnapshotQAPencilFisicoIPad].self, forKey: .snapshots) ?? [],
            actualizacionesRenderer: try c.decodeIfPresent(Int.self, forKey: .actualizacionesRenderer) ?? 0,
            ticksDisplay: try c.decodeIfPresent(Int.self, forKey: .ticksDisplay) ?? 0,
            framesPerdidosEstimados: try c.decodeIfPresent(Int.self, forKey: .framesPerdidosEstimados) ?? 0,
            notas: try c.decodeIfPresent([String].self, forKey: .notas) ?? [])
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(id, forKey: .id)
        if let documentoId {
            try c.encode(documentoId, forKey: .documentoId)
        } else if let documentoClaveLegacy {
            // Conserva una sesión v1 sin inventar una identidad UUID.
            try c.encode(documentoClaveLegacy, forKey: .documentoId)
        }
        try c.encode(dispositivo, forKey: .dispositivo)
        try c.encode(sistema, forKey: .sistema)
        try c.encode(inicio, forKey: .inicio)
        try c.encodeIfPresent(fin, forKey: .fin)
        try c.encode(snapshots, forKey: .snapshots)
        try c.encode(actualizacionesRenderer, forKey: .actualizacionesRenderer)
        try c.encode(ticksDisplay, forKey: .ticksDisplay)
        try c.encode(framesPerdidosEstimados, forKey: .framesPerdidosEstimados)
        try c.encode(notas, forKey: .notas)
    }
}

private struct ArchivoQAPencilFisicoIPad: Codable {
    var version = 2
    var sesiones: [SesionQAPencilFisicoIPad] = []

    private enum CodingKeys: String, CodingKey { case version, sesiones }

    init() {}

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        sesiones = try c.decodeIfPresent(
            [SesionQAPencilFisicoIPad].self, forKey: .sesiones) ?? []
    }
}

final class RepositorioQAPencilFisicoIPad {
    static let compartido = RepositorioQAPencilFisicoIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoQAPencilFisicoIPad
    var maximoSesiones = 60

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(
            for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("QA/PencilFisico", isDirectory: true)
        try? archivos.createDirectory(at: carpeta,
                                      withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("sesiones-pencil-fisico.json")
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(
                ArchivoQAPencilFisicoIPad.self, from: datos) {
            archivo = leido
        } else {
            archivo = ArchivoQAPencilFisicoIPad()
        }
    }

    func guardar(_ sesion: SesionQAPencilFisicoIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.sesiones.removeAll { $0.id == sesion.id }
        archivo.sesiones.append(sesion)
        if archivo.sesiones.count > max(maximoSesiones, 1) {
            archivo.sesiones.removeFirst(
                archivo.sesiones.count - maximoSesiones)
        }
        persistirSinBloqueo()
    }

    func sesiones(documento: DocumentoPersistenteID? = nil) -> [SesionQAPencilFisicoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let valores = documento.map { id in
            archivo.sesiones.filter { $0.documentoId == id }
        } ?? archivo.sesiones
        return valores.sorted { $0.inicio > $1.inicio }
    }

    /// Consulta únicamente histórica para inspeccionar archivos v1. No debe
    /// usarse como identidad documental funcional.
    func sesiones(claveLegacyDocumento: UInt64) -> [SesionQAPencilFisicoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.sesiones
            .filter { $0.documentoId == nil && $0.documentoClaveLegacy == claveLegacyDocumento }
            .sorted { $0.inicio > $1.inicio }
    }

    private func persistirSinBloqueo() {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        guard let datos = try? encoder.encode(archivo) else { return }
        try? datos.write(to: url,
                         options: [.atomic, .completeFileProtection])
    }
}

private enum EstadoTermicoQAPencilIPad {
    static func texto(_ estado: ProcessInfo.ThermalState) -> String {
        switch estado {
        case .nominal: return "nominal"
        case .fair: return "fair"
        case .serious: return "serious"
        case .critical: return "critical"
        @unknown default: return "unknown"
        }
    }

    static func severidad(_ valor: String) -> Int {
        switch valor {
        case "nominal": return 0
        case "fair": return 1
        case "serious": return 2
        case "critical": return 3
        default: return -1
        }
    }
}

private final class CapturadorQAPencilFisicoIPad: NSObject {
    private weak var lienzo: LienzoCuadernU?
    private let repositorio: RepositorioQAPencilFisicoIPad
    private var sesion: SesionQAPencilFisicoIPad?
    private var displayLink: CADisplayLink?
    private var ultimoTick: CFTimeInterval?
    private var ultimoSnapshot = CFTimeInterval(0)
    private var framesDiferidosIniciales = 0
    private var cierrePendiente: DispatchWorkItem?
    private var callbackEstadisticasAnterior: (() -> Void)?
    private var callbackActividadAnterior: ((Bool) -> Void)?

    init(lienzo: LienzoCuadernU,
         repositorio: RepositorioQAPencilFisicoIPad = .compartido) {
        self.lienzo = lienzo
        self.repositorio = repositorio
        super.init()
        callbackEstadisticasAnterior = lienzo.alActualizarEstadisticas
        callbackActividadAnterior = lienzo.alCambiarActividad

        lienzo.alActualizarEstadisticas = { [weak self] in
            self?.callbackEstadisticasAnterior?()
            self?.rendererActualizado()
        }
        lienzo.alCambiarActividad = { [weak self] activo in
            self?.callbackActividadAnterior?(activo)
            self?.cambioActividad(activo)
        }
    }

    deinit {
        cierrePendiente?.cancel()
        displayLink?.invalidate()
    }

    private func cambioActividad(_ activa: Bool) {
        if activa {
            cierrePendiente?.cancel()
            cierrePendiente = nil
            if sesion == nil { comenzar() }
        } else {
            programarCierreConGracia()
        }
    }

    private func comenzar() {
        guard let lienzo else { return }
        framesDiferidosIniciales = lienzo.framesDiferidos
        ultimoTick = nil
        ultimoSnapshot = 0
        sesion = SesionQAPencilFisicoIPad(
            documentoId: lienzo.sesion.idPersistente,
            dispositivo: Self.identificadorDispositivo(),
            sistema: "iPadOS \(UIDevice.current.systemVersion)")
        iniciarDisplayLink()
    }

    private func programarCierreConGracia() {
        cierrePendiente?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in self?.finalizar() }
        cierrePendiente = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + 2, execute: trabajo)
    }

    private func rendererActualizado() {
        guard var actual = sesion, let lienzo else { return }
        actual.actualizacionesRenderer += 1
        let ahora = CACurrentMediaTime()
        if ahora - ultimoSnapshot >= 0.20 {
            ultimoSnapshot = ahora
            actual.snapshots.append(snapshot(lienzo: lienzo, sesion: actual))
            if actual.snapshots.count > 2_000 {
                actual.snapshots.removeFirst(actual.snapshots.count - 2_000)
            }
        }
        sesion = actual
    }

    private func iniciarDisplayLink() {
        displayLink?.invalidate()
        let enlace = CADisplayLink(target: self, selector: #selector(tick(_:)))
        enlace.preferredFrameRateRange = CAFrameRateRange(
            minimum: 60, maximum: 120, preferred: 120)
        enlace.add(to: .main, forMode: .common)
        displayLink = enlace
    }

    @objc private func tick(_ enlace: CADisplayLink) {
        guard var actual = sesion else { return }
        let hz = max(UIScreen.main.maximumFramesPerSecond, 60)
        let esperado = 1.0 / Double(hz)
        if let anterior = ultimoTick {
            let delta = max(enlace.timestamp - anterior, 0)
            if delta > esperado * 1.45 {
                let equivalentes = max(Int((delta / esperado).rounded()) - 1, 0)
                actual.framesPerdidosEstimados += equivalentes
            }
        }
        ultimoTick = enlace.timestamp
        actual.ticksDisplay += 1
        sesion = actual
    }

    private func snapshot(
        lienzo: LienzoCuadernU,
        sesion: SesionQAPencilFisicoIPad
    ) -> SnapshotQAPencilFisicoIPad {
        let totalEsperado = sesion.ticksDisplay + sesion.framesPerdidosEstimados
        let perdidos = totalEsperado > 0
            ? Double(sesion.framesPerdidosEstimados) / Double(totalEsperado) * 100
            : 0
        return SnapshotQAPencilFisicoIPad(
            p50GPUms: lienzo.latenciaMs,
            p95GPUms: lienzo.latenciaP95Ms,
            p99GPUms: lienzo.latenciaP99Ms,
            maximaGPUms: lienzo.latenciaMaximaMs,
            framesDiferidos: max(
                lienzo.framesDiferidos - framesDiferidosIniciales, 0),
            porcentajeFramesPerdidos: perdidos,
            refrescoHz: UIScreen.main.maximumFramesPerSecond,
            estadoTermico: EstadoTermicoQAPencilIPad.texto(
                ProcessInfo.processInfo.thermalState))
    }

    private func finalizar() {
        cierrePendiente?.cancel()
        cierrePendiente = nil
        displayLink?.invalidate()
        displayLink = nil
        guard var actual = sesion else { return }
        if let lienzo {
            actual.snapshots.append(snapshot(lienzo: lienzo, sesion: actual))
        }
        actual.fin = Date()
        if let peor = actual.snapshots.max(by: {
            EstadoTermicoQAPencilIPad.severidad($0.estadoTermico)
                < EstadoTermicoQAPencilIPad.severidad($1.estadoTermico)
        })?.estadoTermico {
            actual.notas.append("thermal_peor=\(peor)")
        }
        if !actual.aptaParaBenchmark {
            actual.notas.append(
                "No publicada como benchmark: requiere ≥240 actualizaciones renderer y ≥3 s.")
        }
        repositorio.guardar(actual)
        if actual.aptaParaBenchmark, let ultimo = actual.ultimo {
            publicarBenchmarks(actual, snapshot: ultimo)
        }
        sesion = nil
    }

    private func publicarBenchmarks(
        _ sesion: SesionQAPencilFisicoIPad,
        snapshot: SnapshotQAPencilFisicoIPad
    ) {
        let repo = RepositorioBenchmarksCuadernUIPad.compartido
        let escenario = "Pencil fisico · ventana GPU 240 · \(snapshot.refrescoHz)Hz"
        repo.registrar(.init(
            metrica: .latenciaPencilP95Ms,
            valor: snapshot.p95GPUms,
            escenario: escenario,
            dispositivo: sesion.dispositivo,
            sistema: sesion.sistema))
        repo.registrar(.init(
            metrica: .latenciaPencilP99Ms,
            valor: snapshot.p99GPUms,
            escenario: escenario,
            dispositivo: sesion.dispositivo,
            sistema: sesion.sistema))
        repo.registrar(.init(
            metrica: .framesPerdidosPorcentaje,
            valor: snapshot.porcentajeFramesPerdidos,
            escenario: escenario,
            dispositivo: sesion.dispositivo,
            sistema: sesion.sistema))
    }

    private static func identificadorDispositivo() -> String {
        var sistema = utsname()
        uname(&sistema)
        let espejo = Mirror(reflecting: sistema.machine)
        let valor = espejo.children.reduce(into: "") { salida, elemento in
            guard let byte = elemento.value as? Int8, byte != 0 else { return }
            salida.append(Character(
                UnicodeScalar(UInt8(bitPattern: byte))))
        }
        return valor.isEmpty ? UIDevice.current.model : valor
    }
}

private enum AsociadosQAPencilFisicoIPad {
    static var capturador: UInt8 = 0
}

@MainActor
final class CoordinadorTelemetriaPencilFisicaIPad {
    static let compartido = CoordinadorTelemetriaPencilFisicaIPad()

    private var observadorEscena: NSObjectProtocol?
    private var observadorMemoria: NSObjectProtocol?
    private var temporizadorEscaneo: Timer?

    var habilitado: Bool {
#if DEBUG
        return true
#else
        return UserDefaults.standard.bool(forKey: "Tinta.QA.PencilFisico")
#endif
    }

    func instalar() {
        guard observadorEscena == nil else {
            escanearEscenasActivas()
            return
        }
        observadorEscena = NotificationCenter.default.addObserver(
            forName: UIScene.didActivateNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.escanearEscenasActivas() }
            }
        observadorMemoria = NotificationCenter.default.addObserver(
            forName: UIApplication.didReceiveMemoryWarningNotification,
            object: nil,
            queue: .main) { _ in
                // La telemetría se conserva en RAM sólo durante la ráfaga actual;
                // el repositorio persiste al cerrarla. No hacemos escrituras de
                // emergencia dentro del memory-warning del sistema.
            }
#if DEBUG
        temporizadorEscaneo = Timer.scheduledTimer(
            withTimeInterval: 2.0, repeats: true) { [weak self] _ in
                Task { @MainActor in self?.escanearEscenasActivas() }
            }
#endif
        escanearEscenasActivas()
    }

    private func escanearEscenasActivas() {
        guard habilitado else { return }
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            for ventana in escena.windows where !ventana.isHidden {
                instalarCapturadores(en: ventana)
            }
        }
    }

    private func instalarCapturadores(en vista: UIView) {
        if let lienzo = vista as? LienzoCuadernU {
            guard objc_getAssociatedObject(
                lienzo, &AsociadosQAPencilFisicoIPad.capturador) == nil else {
                return
            }
            let capturador = CapturadorQAPencilFisicoIPad(lienzo: lienzo)
            objc_setAssociatedObject(
                lienzo, &AsociadosQAPencilFisicoIPad.capturador,
                capturador, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
            return
        }
        vista.subviews.forEach(instalarCapturadores)
    }
}
