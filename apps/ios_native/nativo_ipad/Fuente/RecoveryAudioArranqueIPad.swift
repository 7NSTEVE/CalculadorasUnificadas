import Foundation
import UIKit

/// Resultado mínimo del preflight de audio. Se conserva en memoria para que QA
/// y la UI de recovery distingan "no había nada" de "falló la inspección" sin
/// convertir ese fallo en borrado o reparación automática.
struct ResultadoRecoveryAudioArranqueIPad: Equatable {
    var sesionesMarcadas: [UUID]
    var vinculosMarcados: [UUID] = []
    var error: String?

    var requiereAtencion: Bool {
        !sesionesMarcadas.isEmpty || !vinculosMarcados.isEmpty || error != nil
    }
}

final class CoordinadorRecoveryAudioArranqueIPad {
    static let compartido = CoordinadorRecoveryAudioArranqueIPad()

    private let bloqueo = NSLock()
    private let ejecutarImpl: () throws -> [UUID]
    private let ejecutarVinculosImpl: () throws -> [UUID]
    private var ultimoResultadoInterno = ResultadoRecoveryAudioArranqueIPad(
        sesionesMarcadas: [], vinculosMarcados: [], error: nil)
    private var ejecutado = false

    init(
        ejecutar: @escaping () throws -> [UUID] = {
            try RepositorioAudioResilienteIPad.compartido
                .marcarInterrumpidasParaRecovery()
        },
        ejecutarVinculos: @escaping () throws -> [UUID] = {
            try RepositorioVinculosCapturaAudioIPad.compartido
                .marcarInterrumpidosParaRecovery()
        }
    ) {
        ejecutarImpl = ejecutar
        ejecutarVinculosImpl = ejecutarVinculos
    }

    var ultimoResultado: ResultadoRecoveryAudioArranqueIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return ultimoResultadoInterno
    }

    /// Idempotente por proceso. Cada store se inspecciona de forma independiente:
    /// si uno falla, los IDs que el otro sí logró proteger no se pierden del
    /// diagnóstico. Ningún error se interpreta como un estado limpio.
    @discardableResult
    func ejecutarUnaVez() -> ResultadoRecoveryAudioArranqueIPad {
        bloqueo.lock()
        if ejecutado {
            let actual = ultimoResultadoInterno
            bloqueo.unlock()
            return actual
        }
        ejecutado = true
        bloqueo.unlock()

        var sesiones: [UUID] = []
        var vinculos: [UUID] = []
        var errores: [String] = []
        do {
            sesiones = try ejecutarImpl()
        } catch {
            errores.append("manifest: \(error.localizedDescription)")
        }
        do {
            vinculos = try ejecutarVinculosImpl()
        } catch {
            errores.append("journal: \(error.localizedDescription)")
        }

        let resultado = ResultadoRecoveryAudioArranqueIPad(
            sesionesMarcadas: sesiones.sorted { $0.uuidString < $1.uuidString },
            vinculosMarcados: vinculos.sorted { $0.uuidString < $1.uuidString },
            error: errores.isEmpty ? nil : errores.joined(separator: " · "))

        bloqueo.lock()
        ultimoResultadoInterno = resultado
        bloqueo.unlock()
        return resultado
    }
}

extension AppDelegate {
    func application(
        _ application: UIApplication,
        willFinishLaunchingWithOptions launchOptions:
            [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        guard !EntornoEjecucionIPad.esHostPruebasUnitarias() else { return true }
        _ = CoordinadorRecoveryAudioArranqueIPad.compartido.ejecutarUnaVez()
        return true
    }
}
