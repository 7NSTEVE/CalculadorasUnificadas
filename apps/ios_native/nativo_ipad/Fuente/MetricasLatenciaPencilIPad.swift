import Foundation

struct MuestraLatenciaPencilIPad: Codable, Equatable {
    var eventoId: UUID
    var timestampEntrada: TimeInterval
    var timestampPresentado: TimeInterval
    var coalesced: Bool
    var predicted: Bool
    var corregidaPrediccion: Bool
    var descartada: Bool
    var thermal: String?

    var milisegundos: Double {
        max(0, (timestampPresentado - timestampEntrada) * 1000)
    }
}

struct ResumenLatenciaPencilIPad: Codable, Equatable {
    var muestrasValidas: Int
    var muestrasDescartadas: Int
    var p50Ms: Double?
    var p95Ms: Double?
    var p99Ms: Double?
    var maximaMs: Double?
    var promedioMs: Double?
    var porcentajePredicted: Double
    var porcentajeCorreccionPredicted: Double
    var porcentajeDescartadas: Double
}

struct CriteriosLatenciaPencilIPad: Codable, Equatable {
    var minimoMuestras = 500
    var p50MaxMs: Double = 12
    var p95MaxMs: Double = 20
    var p99MaxMs: Double = 35
    var descartadasMaxPorcentaje: Double = 0.1
}

struct ResultadoGateLatenciaPencilIPad: Equatable {
    var aprobado: Bool
    var fallos: [String]
    var resumen: ResumenLatenciaPencilIPad
}

enum CalculadorMetricasPencilIPad {
    static func resumir(_ muestras: [MuestraLatenciaPencilIPad]) -> ResumenLatenciaPencilIPad {
        let validas = muestras.filter { !$0.descartada && $0.timestampPresentado >= $0.timestampEntrada }
        let latencias = validas.map(\.milisegundos).sorted()
        let descartadas = muestras.count - validas.count
        let predicted = validas.filter(\.predicted).count
        let corregidas = validas.filter { $0.predicted && $0.corregidaPrediccion }.count
        return .init(
            muestrasValidas: validas.count,
            muestrasDescartadas: descartadas,
            p50Ms: percentil(latencias, 0.50),
            p95Ms: percentil(latencias, 0.95),
            p99Ms: percentil(latencias, 0.99),
            maximaMs: latencias.last,
            promedioMs: latencias.isEmpty ? nil : latencias.reduce(0, +) / Double(latencias.count),
            porcentajePredicted: porcentaje(predicted, sobre: validas.count),
            porcentajeCorreccionPredicted: porcentaje(corregidas, sobre: predicted),
            porcentajeDescartadas: porcentaje(descartadas, sobre: muestras.count))
    }

    static func evaluar(_ muestras: [MuestraLatenciaPencilIPad],
                        criterios: CriteriosLatenciaPencilIPad = .init())
        -> ResultadoGateLatenciaPencilIPad {
        let resumen = resumir(muestras)
        var fallos: [String] = []
        if resumen.muestrasValidas < criterios.minimoMuestras {
            fallos.append("Muestras válidas insuficientes: \(resumen.muestrasValidas)/\(criterios.minimoMuestras)")
        }
        if let p50 = resumen.p50Ms, p50 > criterios.p50MaxMs {
            fallos.append(String(format: "P50 %.2f ms > %.2f ms", p50, criterios.p50MaxMs))
        }
        if let p95 = resumen.p95Ms, p95 > criterios.p95MaxMs {
            fallos.append(String(format: "P95 %.2f ms > %.2f ms", p95, criterios.p95MaxMs))
        }
        if let p99 = resumen.p99Ms, p99 > criterios.p99MaxMs {
            fallos.append(String(format: "P99 %.2f ms > %.2f ms", p99, criterios.p99MaxMs))
        }
        if resumen.porcentajeDescartadas > criterios.descartadasMaxPorcentaje {
            fallos.append(String(format: "Muestras descartadas %.3f%% > %.3f%%",
                                 resumen.porcentajeDescartadas,
                                 criterios.descartadasMaxPorcentaje))
        }
        return .init(aprobado: fallos.isEmpty, fallos: fallos, resumen: resumen)
    }

    private static func percentil(_ valores: [Double], _ q: Double) -> Double? {
        guard !valores.isEmpty else { return nil }
        if valores.count == 1 { return valores[0] }
        let posicion = min(max(q, 0), 1) * Double(valores.count - 1)
        let inferior = Int(floor(posicion)), superior = Int(ceil(posicion))
        guard inferior != superior else { return valores[inferior] }
        let fraccion = posicion - Double(inferior)
        return valores[inferior] + (valores[superior] - valores[inferior]) * fraccion
    }

    private static func porcentaje(_ cantidad: Int, sobre total: Int) -> Double {
        total > 0 ? Double(cantidad) / Double(total) * 100 : 0
    }
}

private struct ArchivoMetricasPencilIPad: Codable {
    var version = 1
    var sesiones: [SesionMetricasPencilIPad] = []
}

struct SesionMetricasPencilIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var dispositivo: String
    var sistema: String
    var inicio = Date()
    var fin: Date?
    var muestras: [MuestraLatenciaPencilIPad] = []
    var notas: [String] = []
}

/// Colector desacoplado del renderer. Ink sólo reporta entrada/presentación; no
/// conoce persistencia ni percentiles. Limita muestras para que QA no pueda
/// crecer sin límite durante sesiones de horas.
final class RepositorioMetricasPencilIPad {
    static let compartido = RepositorioMetricasPencilIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoMetricasPencilIPad
    var maximoMuestrasPorSesion = 100_000
    var maximoSesiones = 30

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .documentDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("QA/Pencil", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("latencia-pencil.json")
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? d.decode(ArchivoMetricasPencilIPad.self, from: datos) {
            archivo = leido
        } else { archivo = ArchivoMetricasPencilIPad() }
    }

    @discardableResult
    func iniciar(dispositivo: String, sistema: String) -> UUID {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var sesion = SesionMetricasPencilIPad(
            dispositivo: dispositivo, sistema: sistema)
        if sesion.dispositivo.isEmpty { sesion.dispositivo = "desconocido" }
        archivo.sesiones.append(sesion)
        podarSinBloqueo(); persistirSinBloqueo()
        return sesion.id
    }

    func registrar(_ muestra: MuestraLatenciaPencilIPad, sesionId: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = archivo.sesiones.firstIndex(where: { $0.id == sesionId }),
              archivo.sesiones[i].muestras.count < max(maximoMuestrasPorSesion, 1) else { return }
        archivo.sesiones[i].muestras.append(muestra)
    }

    func finalizar(_ sesionId: UUID, notas: [String] = []) -> SesionMetricasPencilIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let i = archivo.sesiones.firstIndex(where: { $0.id == sesionId }) else { return nil }
        archivo.sesiones[i].fin = Date()
        archivo.sesiones[i].notas.append(contentsOf: notas.map { String($0.prefix(500)) })
        persistirSinBloqueo()
        return archivo.sesiones[i]
    }

    func sesion(_ id: UUID) -> SesionMetricasPencilIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.sesiones.first { $0.id == id }
    }

    func resumen(_ id: UUID) -> ResumenLatenciaPencilIPad? {
        sesion(id).map { CalculadorMetricasPencilIPad.resumir($0.muestras) }
    }

    private func podarSinBloqueo() {
        if archivo.sesiones.count > maximoSesiones {
            archivo.sesiones = Array(archivo.sesiones.suffix(maximoSesiones))
        }
    }

    private func persistirSinBloqueo() {
        let e = JSONEncoder(); e.dateEncodingStrategy = .iso8601
        e.outputFormatting = [.sortedKeys]
        if let datos = try? e.encode(archivo) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }
}
