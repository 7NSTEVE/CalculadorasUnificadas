import UIKit

extension PanelComparativaGoodnotesIPad {
    @objc func abrirCorpusCanonicoGoodnotesIPad() {
        guard presentedViewController == nil else { return }
        let panel = PanelCorpusComparativaGoodnotesIPad()
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .formSheet
        present(nav, animated: true)
    }
}

@MainActor
final class CoordinadorAccesoCorpusQAGoodnotesIPad {
    static let compartido = CoordinadorAccesoCorpusQAGoodnotesIPad()

    private var temporizador: Timer?
    private var observadorFondo: NSObjectProtocol?

    func instalar() {
        if observadorFondo == nil {
            observadorFondo = NotificationCenter.default.addObserver(
                forName: UIApplication.didEnterBackgroundNotification,
                object: nil,
                queue: .main) { [weak self] _ in
                    Task { @MainActor in self?.pausar() }
                }
        }
        guard temporizador == nil else {
            escanear(); return
        }
        let timer = Timer.scheduledTimer(
            withTimeInterval: 2.0, repeats: true) { [weak self] _ in
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
        if let panel = controlador as? PanelComparativaGoodnotesIPad {
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

    private func instalarBoton(en panel: PanelComparativaGoodnotesIPad) {
        let actuales = panel.navigationItem.rightBarButtonItems ?? []
        guard !actuales.contains(where: {
            $0.accessibilityIdentifier == "qa.goodnotes.corpus.abrir"
        }) else { return }
        let boton = UIBarButtonItem(
            title: "Corpus",
            style: .plain,
            target: panel,
            action: #selector(
                PanelComparativaGoodnotesIPad.abrirCorpusCanonicoGoodnotesIPad))
        boton.accessibilityLabel = "Preparar corpus comparativo de 500 páginas"
        boton.accessibilityIdentifier = "qa.goodnotes.corpus.abrir"
        panel.navigationItem.rightBarButtonItems = [boton] + actuales
    }
}

extension AppDelegate {
    func applicationDidBecomeActive(_ application: UIApplication) {
        guard !EntornoEjecucionIPad.esHostPruebasUnitarias() else { return }
        CoordinadorAccesoCorpusQAGoodnotesIPad.compartido.instalar()
    }
}
