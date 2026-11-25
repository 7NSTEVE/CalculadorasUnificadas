import Foundation

enum UnidadBenchmarkCuadernUIPad: String, Codable {
    case milisegundos
    case megabytesPorSegundo
    case bytes
    case porcentaje
}

enum MetricaRendimientoCuadernUIPad: String, Codable, CaseIterable {
    case reaperturaDocumentoMs
    case primeraTintaVisibleMs
    case busquedaP95Ms
    case latenciaPencilP95Ms
    case latenciaPencilP99Ms
    case importacionMBPorSegundo
    case exportacionMBPorSegundo
    case memoriaDocumentoBytes
    case memoriaGlobalBytes
    case framesPerdidosPorcentaje

    var menorEsMejor: Bool {
        switch self {
        case .importacionMBPorSegundo, .exportacionMBPorSegundo:
            return false
        default:
            return true
        }
    }

    var unidad: UnidadBenchmarkCuadernUIPad {
        switch self {
        case .reaperturaDocumentoMs, .primeraTintaVisibleMs, .busquedaP95Ms,
             .latenciaPencilP95Ms, .latenciaPencilP99Ms:
            return .milisegundos
        case .importacionMBPorSegundo, .exportacionMBPorSegundo:
            return .megabytesPorSegundo
        case .memoriaDocumentoBytes, .memoriaGlobalBytes:
            return .bytes
        case .framesPerdidosPorcentaje:
            return .porcentaje
        }
    }
}

struct MuestraBenchmarkCuadernUIPad: Codable, Equatable {
    var metrica: MetricaRendimientoCuadernUIPad
    var valor: Double
    var escenario: String
    var dispositivo: String
    var sistema: String
    var commit: String?
    var fecha = Date()
}

/// Baseline de una métrica. Cuando `dispositivo`/`sistema` son nil el baseline
/// es deliberadamente genérico. Para comparaciones de superioridad se deben
/// preferir baselines acotados al mismo hardware y versión de iPadOS.
struct BaselineBenchmarkCuadernUIPad: Codable, Equatable {
    var metrica: MetricaRendimientoCuadernUIPad
    var escenario: String
    var valor: Double
    var toleranciaRegresionPorcentaje: Double
    var dispositivo: String?
    var sistema: String?

    init(metrica: MetricaRendimientoCuadernUIPad,
         escenario: String,
         valor: Double,
         toleranciaRegresionPorcentaje: Double,
         dispositivo: String? = nil,
         sistema: String? = nil) {
        self.metrica = metrica
        self.escenario = escenario
        self.valor = valor
        self.toleranciaRegresionPorcentaje = toleranciaRegresionPorcentaje
        self.dispositivo = Self.normalizar(dispositivo)
        self.sistema = Self.normalizar(sistema)
    }

    private static func normalizar(_ valor: String?) -> String? {
        guard let limpio = valor?.trimmingCharacters(in: .whitespacesAndNewlines),
              !limpio.isEmpty else { return nil }
        return limpio
    }
}

struct RegresionBenchmarkCuadernUIPad: Equatable, Identifiable {
    var id: String { "\(metrica.rawValue):\(escenario)" }
    var metrica: MetricaRendimientoCuadernUIPad
    var escenario: String
    var baseline: Double
    var actual: Double
    var degradacionPorcentaje: Double
    var toleranciaPorcentaje: Double
}

struct ReporteBenchmarkCuadernUIPad: Equatable {
    var regresiones: [RegresionBenchmarkCuadernUIPad]
    var sinBaseline: [MuestraBenchmarkCuadernUIPad]
    var aprobado: Bool { regresiones.isEmpty }
}

enum EvaluadorBenchmarksCuadernUIPad {
    /// Evalúa cada combinación métrica+escenario+hardware+OS por separado.
    /// La implementación anterior podía mezclar en una misma mediana un iPad
    /// Pro 120 Hz y un iPad 60 Hz, produciendo un gate estadísticamente inválido.
    static func evaluar(
        muestras: [MuestraBenchmarkCuadernUIPad],
        baselines: [BaselineBenchmarkCuadernUIPad]
    ) -> ReporteBenchmarkCuadernUIPad {
        let limpias = muestras.filter { $0.valor.isFinite && $0.valor >= 0 }
        let agrupadas = Dictionary(grouping: limpias) {
            ClaveMuestraBenchmarkIPad(
                metrica: $0.metrica,
                escenario: $0.escenario,
                dispositivo: $0.dispositivo,
                sistema: $0.sistema)
        }
        var regresiones: [RegresionBenchmarkCuadernUIPad] = []
        var sinBaseline: [MuestraBenchmarkCuadernUIPad] = []

        for (clave, valores) in agrupadas {
            let actual = mediana(valores.map(\.valor))
            guard actual.isFinite,
                  let baseline = baselineMasEspecifico(
                    para: clave, baselines: baselines) else {
                sinBaseline.append(contentsOf: valores)
                continue
            }
            guard baseline.valor.isFinite, baseline.valor > 0 else {
                sinBaseline.append(contentsOf: valores)
                continue
            }

            let degradacion: Double
            if clave.metrica.menorEsMejor {
                degradacion = (actual - baseline.valor) / baseline.valor * 100
            } else {
                degradacion = (baseline.valor - actual) / baseline.valor * 100
            }
            if degradacion > max(baseline.toleranciaRegresionPorcentaje, 0) {
                regresiones.append(.init(
                    metrica: clave.metrica,
                    escenario: clave.escenario,
                    baseline: baseline.valor,
                    actual: actual,
                    degradacionPorcentaje: degradacion,
                    toleranciaPorcentaje: baseline.toleranciaRegresionPorcentaje))
            }
        }

        regresiones.sort {
            if $0.degradacionPorcentaje != $1.degradacionPorcentaje {
                return $0.degradacionPorcentaje > $1.degradacionPorcentaje
            }
            return $0.id < $1.id
        }
        sinBaseline.sort {
            ($0.metrica.rawValue, $0.escenario, $0.dispositivo, $0.sistema, $0.fecha)
                < ($1.metrica.rawValue, $1.escenario, $1.dispositivo, $1.sistema, $1.fecha)
        }
        return .init(regresiones: regresiones, sinBaseline: sinBaseline)
    }

    static func mediana(_ valores: [Double]) -> Double {
        let v = valores.filter { $0.isFinite }.sorted()
        guard !v.isEmpty else { return .nan }
        let mitad = v.count / 2
        return v.count.isMultiple(of: 2)
            ? (v[mitad - 1] + v[mitad]) / 2
            : v[mitad]
    }

    private static func baselineMasEspecifico(
        para clave: ClaveMuestraBenchmarkIPad,
        baselines: [BaselineBenchmarkCuadernUIPad]
    ) -> BaselineBenchmarkCuadernUIPad? {
        baselines
            .filter {
                $0.metrica == clave.metrica &&
                $0.escenario == clave.escenario &&
                ($0.dispositivo == nil || $0.dispositivo == clave.dispositivo) &&
                ($0.sistema == nil || $0.sistema == clave.sistema)
            }
            .max { izquierda, derecha in
                especificidad(izquierda) < especificidad(derecha)
            }
    }

    private static func especificidad(_ baseline: BaselineBenchmarkCuadernUIPad) -> Int {
        (baseline.dispositivo == nil ? 0 : 1) + (baseline.sistema == nil ? 0 : 1)
    }
}

private struct ClaveMuestraBenchmarkIPad: Hashable {
    var metrica: MetricaRendimientoCuadernUIPad
    var escenario: String
    var dispositivo: String
    var sistema: String
}

private struct ArchivoBenchmarksCuadernUIPad: Codable {
    var version = 2
    var muestras: [MuestraBenchmarkCuadernUIPad] = []
    var baselines: [BaselineBenchmarkCuadernUIPad] = []

    private enum CodingKeys: String, CodingKey {
        case version, muestras, baselines
    }

    init() {}

    init(from decoder: Decoder) throws {
        let contenedor = try decoder.container(keyedBy: CodingKeys.self)
        version = try contenedor.decodeIfPresent(Int.self, forKey: .version) ?? 1
        muestras = try contenedor.decodeIfPresent(
            [MuestraBenchmarkCuadernUIPad].self, forKey: .muestras) ?? []
        baselines = try contenedor.decodeIfPresent(
            [BaselineBenchmarkCuadernUIPad].self, forKey: .baselines) ?? []
    }
}

final class RepositorioBenchmarksCuadernUIPad {
    static let compartido = RepositorioBenchmarksCuadernUIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoBenchmarksCuadernUIPad
    var maximoMuestras = 10_000

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .documentDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("QA/Benchmarks", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("rendimiento.json")
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? d.decode(ArchivoBenchmarksCuadernUIPad.self, from: datos) {
            archivo = leido
        } else { archivo = ArchivoBenchmarksCuadernUIPad() }
    }

    func registrar(_ muestra: MuestraBenchmarkCuadernUIPad) {
        guard muestra.valor.isFinite, muestra.valor >= 0,
              !muestra.escenario.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              !muestra.dispositivo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              !muestra.sistema.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return
        }
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.muestras.append(muestra)
        if archivo.muestras.count > max(maximoMuestras, 1) {
            archivo.muestras.removeFirst(archivo.muestras.count - maximoMuestras)
        }
        persistir()
    }

    func fijarBaseline(_ baseline: BaselineBenchmarkCuadernUIPad) {
        guard baseline.valor.isFinite, baseline.valor > 0 else { return }
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.baselines.removeAll {
            $0.metrica == baseline.metrica &&
            $0.escenario == baseline.escenario &&
            $0.dispositivo == baseline.dispositivo &&
            $0.sistema == baseline.sistema
        }
        archivo.baselines.append(baseline)
        persistir()
    }

    /// Promueve la mediana de varias ejecuciones reales a baseline específico
    /// del hardware/OS. No permite fijar un baseline con una única corrida
    /// accidental salvo que el llamador lo pida explícitamente.
    @discardableResult
    func fijarBaselineDesdeMuestras(
        metrica: MetricaRendimientoCuadernUIPad,
        escenario: String,
        dispositivo: String,
        sistema: String,
        toleranciaRegresionPorcentaje: Double,
        minimoMuestras: Int = 5,
        ultimas: Int = 15
    ) -> BaselineBenchmarkCuadernUIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let candidatas = archivo.muestras.filter {
            $0.metrica == metrica && $0.escenario == escenario &&
            $0.dispositivo == dispositivo && $0.sistema == sistema
        }.suffix(max(ultimas, 1))
        guard candidatas.count >= max(minimoMuestras, 1) else { return nil }
        let valor = EvaluadorBenchmarksCuadernUIPad.mediana(candidatas.map(\.valor))
        guard valor.isFinite, valor > 0 else { return nil }
        let baseline = BaselineBenchmarkCuadernUIPad(
            metrica: metrica,
            escenario: escenario,
            valor: valor,
            toleranciaRegresionPorcentaje: max(toleranciaRegresionPorcentaje, 0),
            dispositivo: dispositivo,
            sistema: sistema)
        archivo.baselines.removeAll {
            $0.metrica == baseline.metrica &&
            $0.escenario == baseline.escenario &&
            $0.dispositivo == baseline.dispositivo &&
            $0.sistema == baseline.sistema
        }
        archivo.baselines.append(baseline)
        persistir()
        return baseline
    }

    func reporte(dispositivo: String? = nil,
                 sistema: String? = nil,
                 ultimas: Int = 500) -> ReporteBenchmarkCuadernUIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var muestras = archivo.muestras
        if let dispositivo { muestras = muestras.filter { $0.dispositivo == dispositivo } }
        if let sistema { muestras = muestras.filter { $0.sistema == sistema } }
        muestras = Array(muestras.suffix(max(ultimas, 1)))
        return EvaluadorBenchmarksCuadernUIPad.evaluar(
            muestras: muestras,
            baselines: archivo.baselines)
    }

    /// Mide sólo métricas cuya unidad es tiempo. Para throughput/memoria se usan
    /// los métodos específicos, evitando almacenar milisegundos bajo una unidad
    /// incompatible por error de cableado.
    func medir<T>(metrica: MetricaRendimientoCuadernUIPad,
                  escenario: String,
                  dispositivo: String,
                  sistema: String,
                  commit: String? = nil,
                  bloque: () throws -> T) rethrows -> T {
        let inicio = ContinuousClock.now
        let resultado = try bloque()
        let duracion = inicio.duration(to: .now)
        if metrica.unidad == .milisegundos {
            registrar(.init(
                metrica: metrica,
                valor: Self.milisegundos(duracion),
                escenario: escenario,
                dispositivo: dispositivo,
                sistema: sistema,
                commit: commit))
        } else {
            assertionFailure("medir() requiere una métrica en milisegundos")
        }
        return resultado
    }

    func medirAsync<T>(metrica: MetricaRendimientoCuadernUIPad,
                       escenario: String,
                       dispositivo: String,
                       sistema: String,
                       commit: String? = nil,
                       bloque: () async throws -> T) async rethrows -> T {
        let inicio = ContinuousClock.now
        let resultado = try await bloque()
        let duracion = inicio.duration(to: .now)
        if metrica.unidad == .milisegundos {
            registrar(.init(
                metrica: metrica,
                valor: Self.milisegundos(duracion),
                escenario: escenario,
                dispositivo: dispositivo,
                sistema: sistema,
                commit: commit))
        } else {
            assertionFailure("medirAsync() requiere una métrica en milisegundos")
        }
        return resultado
    }

    func registrarThroughput(
        metrica: MetricaRendimientoCuadernUIPad,
        bytes: Int64,
        duracionSegundos: TimeInterval,
        escenario: String,
        dispositivo: String,
        sistema: String,
        commit: String? = nil
    ) {
        guard metrica.unidad == .megabytesPorSegundo,
              bytes >= 0, duracionSegundos.isFinite, duracionSegundos > 0 else { return }
        let mb = Double(bytes) / 1_048_576
        registrar(.init(
            metrica: metrica,
            valor: mb / duracionSegundos,
            escenario: escenario,
            dispositivo: dispositivo,
            sistema: sistema,
            commit: commit))
    }

    func registrarMemoria(
        metrica: MetricaRendimientoCuadernUIPad,
        bytes: UInt64,
        escenario: String,
        dispositivo: String,
        sistema: String,
        commit: String? = nil
    ) {
        guard metrica.unidad == .bytes else { return }
        registrar(.init(
            metrica: metrica,
            valor: Double(bytes),
            escenario: escenario,
            dispositivo: dispositivo,
            sistema: sistema,
            commit: commit))
    }

    private static func milisegundos(_ duracion: Duration) -> Double {
        let componentes = duracion.components
        return Double(componentes.seconds) * 1000
            + Double(componentes.attoseconds) / 1_000_000_000_000_000
    }

    private func persistir() {
        let e = JSONEncoder(); e.dateEncodingStrategy = .iso8601
        e.outputFormatting = [.sortedKeys]
        if let datos = try? e.encode(archivo) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }
}
