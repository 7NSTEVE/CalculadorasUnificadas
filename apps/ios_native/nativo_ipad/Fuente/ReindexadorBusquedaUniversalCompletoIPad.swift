import Foundation

enum ReindexadorBusquedaUniversalCompletoIPad {
    static func ejecutar(indice: IndiceBusquedaUniversalIPad = .compartido) {
        ProveedorBusquedaCentroAcademicoIPad.reindexar(indice: indice)
        ProveedorBusquedaEstudioTecnicoIPad.reindexar(indice: indice)
        ProveedorBusquedaElementosReutilizablesIPad.reindexar(indice: indice)
        ProveedorBusquedaComentariosIPad.reindexar(indice: indice)
        ProveedorBusquedaDocumentosTextoIPad.reindexar(indice: indice)
    }
}

/// Serializa y agrupa las peticiones de reindexado del shell. Abrir Buscar ya no
/// recorre todos los repositorios en el hilo principal ni repite el mismo trabajo
/// si dos superficies lo solicitan casi al mismo tiempo.
final class CoordinadorReindexacionBusquedaUniversalIPad {
    static let compartido = CoordinadorReindexacionBusquedaUniversalIPad()

    private let bloqueo = NSLock()
    private let cola: DispatchQueue
    private let vigencia: TimeInterval
    private let ahora: () -> Date
    private let trabajo: () -> Void
    private var ultimaIndexacion: Date?
    private var indexando = false
    private var finalizaciones: [() -> Void] = []

    init(
        vigencia: TimeInterval = 45,
        cola: DispatchQueue = DispatchQueue(
            label: "cuadernu.busqueda.reindexacion",
            qos: .utility),
        ahora: @escaping () -> Date = Date.init,
        trabajo: @escaping () -> Void = {
            ReindexadorBusquedaUniversalCompletoIPad.ejecutar()
        }
    ) {
        self.vigencia = max(vigencia, 0)
        self.cola = cola
        self.ahora = ahora
        self.trabajo = trabajo
    }

    /// Ejecuta como máximo un reindexado simultáneo. Las solicitudes que llegan
    /// mientras está en curso se adhieren al mismo trabajo y reciben su callback
    /// al finalizar. Un índice reciente se reutiliza salvo que `forzar` sea true.
    func solicitar(forzar: Bool = false, completar: @escaping () -> Void) {
        bloqueo.lock()
        let fecha = ahora()
        if !forzar,
           let ultimaIndexacion,
           fecha.timeIntervalSince(ultimaIndexacion) < vigencia {
            bloqueo.unlock()
            DispatchQueue.main.async(execute: completar)
            return
        }

        finalizaciones.append(completar)
        guard !indexando else {
            bloqueo.unlock()
            return
        }
        indexando = true
        bloqueo.unlock()

        cola.async { [weak self] in
            guard let self else { return }
            self.trabajo()
            self.terminarIndexacion()
        }
    }

    private func terminarIndexacion() {
        bloqueo.lock()
        ultimaIndexacion = ahora()
        indexando = false
        let callbacks = finalizaciones
        finalizaciones.removeAll(keepingCapacity: true)
        bloqueo.unlock()

        DispatchQueue.main.async {
            callbacks.forEach { $0() }
        }
    }

    /// Visible a pruebas para verificar coalescencia sin exponer estado mutable.
    var estaIndexando: Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return indexando
    }
}
