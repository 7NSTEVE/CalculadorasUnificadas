import Foundation
import UIKit

struct ResultadoSincronizacionComparativaPencilIPad: Equatable {
    var sesionesEvaluadas = 0
    var sesionesOmitidas = 0
    var muestrasAceptadas = 0
    var errores: [String] = []
}

enum SincronizadorComparativaPencilIPad {
    @discardableResult
    static func sincronizar(
        sesiones: [SesionQAPencilFisicoIPad],
        repositorio: RepositorioComparativaGoodnotesIPad = .compartido
    ) -> ResultadoSincronizacionComparativaPencilIPad {
        var resultado = ResultadoSincronizacionComparativaPencilIPad()
        for sesion in sesiones {
            guard sesion.fin != nil,
                  sesion.aptaParaBenchmark,
                  let snapshot = sesion.ultimo,
                  snapshot.p95GPUms.isFinite,
                  snapshot.p99GPUms.isFinite,
                  snapshot.p95GPUms >= 0,
                  snapshot.p99GPUms >= 0 else {
                resultado.sesionesOmitidas += 1
                continue
            }
            resultado.sesionesEvaluadas += 1
            let termico = EstadoTermicoComparativaIPad(
                rawValue: snapshot.estadoTermico) ?? .unknown
            let condiciones = CondicionesCorridaGoodnotesIPad(
                dispositivo: sesion.dispositivo,
                sistema: sesion.sistema,
                refrescoHz: snapshot.refrescoHz,
                estadoTermico: termico,
                pencilFisico: true,
                sha256Corpus: nil)
            let fecha = sesion.fin ?? sesion.inicio

            let entradas: [(EscenarioComparativaGoodnotesIPad, Double)] = [
                (CatalogoEscenariosGoodnotesIPad.pencilP95, snapshot.p95GPUms),
                (CatalogoEscenariosGoodnotesIPad.pencilP99, snapshot.p99GPUms),
            ]
            for (escenario, valor) in entradas {
                let idMuestra = "pencil:\(sesion.id.uuidString.lowercased()):\(escenario.idVersionado)"
                do {
                    _ = try repositorio.registrarMuestraCanonica(
                        producto: .cuadernU,
                        escenarioIdVersionado: escenario.idVersionado,
                        condiciones: condiciones,
                        valor: valor,
                        fecha: fecha,
                        idMuestra: idMuestra)
                    resultado.muestrasAceptadas += 1
                } catch {
                    resultado.errores.append(
                        "\(idMuestra): \(error.localizedDescription)")
                }
            }
        }
        resultado.errores.sort()
        return resultado
    }
}

/// El sincronizador corre fuera del main thread porque puede escribir el JSON
/// comparativo. No mide nada nuevo: sólo importa sesiones físicas ya cerradas,
/// por lo que no añade trabajo al camino event→GPU de lápiz del iPad.
@MainActor
final class CoordinadorSincronizacionComparativaPencilIPad {
    static let compartido = CoordinadorSincronizacionComparativaPencilIPad()

    private var observador: NSObjectProtocol?
    private var temporizador: Timer?
    private var trabajando = false
    private(set) var ultimoResultado = ResultadoSincronizacionComparativaPencilIPad()

    func instalar() {
        guard observador == nil else {
            lanzar()
            return
        }
        observador = NotificationCenter.default.addObserver(
            forName: UIScene.didActivateNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.lanzar() }
            }
        let timer = Timer.scheduledTimer(
            withTimeInterval: 15.0,
            repeats: true) { [weak self] _ in
                Task { @MainActor in self?.lanzar() }
            }
        timer.tolerance = 3.0
        temporizador = timer
        lanzar()
    }

    private func lanzar() {
        guard !trabajando else { return }
        trabajando = true
        let sesiones = RepositorioQAPencilFisicoIPad.compartido.sesiones()
        DispatchQueue.global(qos: .utility).async { [weak self] in
            let resultado = SincronizadorComparativaPencilIPad.sincronizar(
                sesiones: sesiones)
            DispatchQueue.main.async {
                guard let self else { return }
                self.ultimoResultado = resultado
                self.trabajando = false
            }
        }
    }
}
