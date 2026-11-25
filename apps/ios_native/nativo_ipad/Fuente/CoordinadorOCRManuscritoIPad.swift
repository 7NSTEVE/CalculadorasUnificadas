import ObjectiveC
import UIKit

private enum AsociadosOCRManuscritoIPad {
    static var capturador: UInt8 = 0
}

@MainActor
private final class CapturadorOCRManuscritoIPad: NSObject {
    private weak var lienzo: LienzoCuadernU?
    private let indexador: IndexadorManuscritoVisionIPad
    private var trabajo: DispatchWorkItem?
    private var revisionCambios = 0
    private var escribiendo = false

    private var terminarAnterior: ((CGPoint, Bool) -> Void)?
    private var actividadAnterior: ((Bool) -> Void)?
    private var contenidoAnterior: (() -> Void)?

    init(
        lienzo: LienzoCuadernU,
        indexador: IndexadorManuscritoVisionIPad = .compartido
    ) {
        self.lienzo = lienzo
        self.indexador = indexador
        super.init()

        terminarAnterior = lienzo.alTerminarEscritura
        actividadAnterior = lienzo.alCambiarActividad
        contenidoAnterior = lienzo.alCambiarContenidoSeleccion

        lienzo.alTerminarEscritura = { [weak self] punto, crecio in
            self?.terminarAnterior?(punto, crecio)
            self?.marcarCambio()
        }
        lienzo.alCambiarContenidoSeleccion = { [weak self] in
            self?.contenidoAnterior?()
            self?.marcarCambio()
        }
        lienzo.alCambiarActividad = { [weak self] activa in
            self?.actividadAnterior?(activa)
            guard let self else { return }
            self.escribiendo = activa
            if activa {
                self.cancelarTrabajoActual()
            } else {
                self.programar()
            }
        }

        // Una página reabierta puede contener tinta nunca indexada. El primer
        // pase se difiere para no competir con la restauración de la escena.
        programar(retraso: 2.5)
    }

    deinit {
        trabajo?.cancel()
    }

    func cancelarPorMemoria() {
        cancelarTrabajoActual()
    }

    private func cancelarTrabajoActual() {
        trabajo?.cancel()
        trabajo = nil
        if let lienzo {
            indexador.cancelar(documentoId: lienzo.sesion.documentoPersistenteId)
        }
    }

    private func marcarCambio() {
        revisionCambios &+= 1
        // Una transformación/Undo/Paste puede ocurrir mientras Vision analiza
        // un snapshot anterior. Invalidamos su generación inmediatamente para
        // que ese resultado nunca alcance el sidecar reconstruible.
        if let lienzo {
            indexador.cancelar(documentoId: lienzo.sesion.documentoPersistenteId)
        }
        programar()
    }

    private func programar(retraso: TimeInterval = 1.35) {
        guard !escribiendo else { return }
        trabajo?.cancel()
        let revision = revisionCambios
        let nuevo = DispatchWorkItem { [weak self] in
            guard let self else { return }
            Task { @MainActor in
                self.ejecutar(revisionProgramada: revision)
            }
        }
        trabajo = nuevo
        DispatchQueue.main.asyncAfter(deadline: .now() + retraso,
                                      execute: nuevo)
    }

    private func ejecutar(revisionProgramada: Int) {
        guard !escribiendo, let lienzo else { return }
        trabajo = nil
        indexador.indexar(sesion: lienzo.sesion) { [weak self] resultado in
            guard let self else { return }
            // Otra edición durante el pase o un lote incompleto obliga a una
            // segunda pasada; el índice anterior permaneció válido entretanto.
            if self.revisionCambios != revisionProgramada
                || resultado.regionesPendientes > 0
                || resultado.error != nil {
                self.programar(retraso: resultado.error == nil ? 0.8 : 3.0)
            }
        }
    }
}

@MainActor
final class CoordinadorOCRManuscritoIPad {
    static let compartido = CoordinadorOCRManuscritoIPad()

    private var observadorEscena: NSObjectProtocol?
    private var observadorMemoria: NSObjectProtocol?
    private var temporizador: Timer?

    func instalar() {
        guard observadorEscena == nil else {
            escanear()
            return
        }
        observadorEscena = NotificationCenter.default.addObserver(
            forName: UIScene.didActivateNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        observadorMemoria = NotificationCenter.default.addObserver(
            forName: UIApplication.didReceiveMemoryWarningNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.cancelarPorMemoria() }
            }
        let timer = Timer.scheduledTimer(
            withTimeInterval: 3.0,
            repeats: true) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        timer.tolerance = 0.8
        temporizador = timer
        escanear()
    }

    private func escanear() {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            for ventana in escena.windows where !ventana.isHidden {
                instalar(en: ventana)
            }
        }
    }

    private func instalar(en vista: UIView) {
        if let lienzo = vista as? LienzoCuadernU {
            guard objc_getAssociatedObject(
                lienzo, &AsociadosOCRManuscritoIPad.capturador) == nil else {
                return
            }
            let capturador = CapturadorOCRManuscritoIPad(lienzo: lienzo)
            objc_setAssociatedObject(
                lienzo,
                &AsociadosOCRManuscritoIPad.capturador,
                capturador,
                .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
            return
        }
        vista.subviews.forEach(instalar)
    }

    private func cancelarPorMemoria() {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes {
            for ventana in escena.windows {
                cancelar(en: ventana)
            }
        }
    }

    private func cancelar(en vista: UIView) {
        if let lienzo = vista as? LienzoCuadernU,
           let capturador = objc_getAssociatedObject(
            lienzo, &AsociadosOCRManuscritoIPad.capturador)
                as? CapturadorOCRManuscritoIPad {
            capturador.cancelarPorMemoria()
            return
        }
        vista.subviews.forEach(cancelar)
    }
}
