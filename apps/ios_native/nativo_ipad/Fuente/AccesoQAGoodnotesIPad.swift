import UIKit

extension PanelDisposicionInteligente {
    @objc func abrirQAGoodnotesIPad() {
        guard presentedViewController == nil else { return }
        let panel = PanelComparativaGoodnotesIPad()
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: true)
    }
}

/// Integración temporal y aislada del laboratorio comparativo en la superficie
/// avanzada. Evita reescribir `DisposicionInteligente.swift` completo mediante
/// la API Contents mientras la rama cambia concurrentemente.
///
/// El escaneo es deliberadamente lento: no toca archivos, Metal ni contenido de
/// notas; únicamente busca el controlador visible y añade un UIBarButtonItem una
/// sola vez. Cuando exista un Settings/QA central, este coordinador se elimina.
@MainActor
final class CoordinadorAccesoQAGoodnotesIPad {
    static let compartido = CoordinadorAccesoQAGoodnotesIPad()

    private var observadorActivacion: NSObjectProtocol?
    private var observadorFondo: NSObjectProtocol?
    private var temporizador: Timer?

    func instalar() {
        guard observadorActivacion == nil else {
            escanear(); return
        }
        observadorActivacion = NotificationCenter.default.addObserver(
            forName: UIApplication.didBecomeActiveNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.reanudar() }
            }
        observadorFondo = NotificationCenter.default.addObserver(
            forName: UIApplication.didEnterBackgroundNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.pausar() }
            }
        reanudar()
    }

    private func reanudar() {
        guard temporizador == nil else {
            escanear(); return
        }
        let timer = Timer.scheduledTimer(
            withTimeInterval: 2.0,
            repeats: true) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        timer.tolerance = 0.5
        temporizador = timer
        escanear()
    }

    private func pausar() {
        temporizador?.invalidate()
        temporizador = nil
    }

    private func escanear() {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            for ventana in escena.windows where !ventana.isHidden {
                guard let raiz = ventana.rootViewController else { continue }
                visitar(raiz)
            }
        }
    }

    private func visitar(_ controlador: UIViewController) {
        if let panel = controlador as? PanelDisposicionInteligente {
            instalarBoton(en: panel)
        }
        if let presentado = controlador.presentedViewController {
            visitar(presentado)
        }
        if let nav = controlador as? UINavigationController {
            if let visible = nav.visibleViewController { visitar(visible) }
            return
        }
        if let tabs = controlador as? UITabBarController {
            if let seleccionado = tabs.selectedViewController { visitar(seleccionado) }
            return
        }
        if let split = controlador as? UISplitViewController {
            split.viewControllers.forEach(visitar)
            return
        }
        controlador.children.forEach(visitar)
    }

    private func instalarBoton(en panel: PanelDisposicionInteligente) {
        let actuales = panel.navigationItem.rightBarButtonItems ?? []
        guard !actuales.contains(where: { $0.accessibilityIdentifier == "qa.goodnotes.abrir" })
        else { return }

        let boton = UIBarButtonItem(
            title: "QA",
            style: .plain,
            target: panel,
            action: #selector(PanelDisposicionInteligente.abrirQAGoodnotesIPad))
        boton.accessibilityLabel = "QA comparativa contra Goodnotes"
        boton.accessibilityIdentifier = "qa.goodnotes.abrir"
        panel.navigationItem.rightBarButtonItems = [boton] + actuales
    }
}
