import ObjectiveC
import UIKit

private enum AsociadosAccesoRecoveryAudioIPad {
    static var instalado: UInt8 = 0
}

@MainActor
final class CoordinadorAccesoRecoveryAudioIPad: NSObject {
    static let compartido = CoordinadorAccesoRecoveryAudioIPad()

    private weak var panelActual: PanelGrabacion?
    private var observadorEscena: NSObjectProtocol?
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
        let timer = Timer.scheduledTimer(
            withTimeInterval: 2.0,
            repeats: true) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        timer.tolerance = 0.4
        temporizador = timer
        escanear()
    }

    private var requiereAtencion: Bool {
        let sesiones = RepositorioAudioResilienteIPad.compartido.sesiones()
        if sesiones.contains(where: { $0.estado == .requiereRecuperacion }) {
            return true
        }
        if !RepositorioVinculosCapturaAudioIPad.compartido
            .pendientesRecovery().isEmpty {
            return true
        }
        return CoordinadorRecoveryAudioArranqueIPad.compartido
            .ultimoResultado.error != nil
    }

    private func escanear() {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            for ventana in escena.windows where !ventana.isHidden {
                guard let raiz = ventana.rootViewController else { continue }
                buscarPanel(en: raiz)
            }
        }
    }

    private func buscarPanel(en controlador: UIViewController) {
        if let panel = controlador as? PanelGrabacion {
            actualizar(panel)
        }
        if let presentado = controlador.presentedViewController {
            buscarPanel(en: presentado)
        }
        if let nav = controlador as? UINavigationController {
            nav.viewControllers.forEach(buscarPanel)
        } else if let tabs = controlador as? UITabBarController {
            tabs.viewControllers?.forEach(buscarPanel)
        } else if let split = controlador as? UISplitViewController {
            split.viewControllers.forEach(buscarPanel)
        } else {
            controlador.children.forEach(buscarPanel)
        }
    }

    private func actualizar(_ panel: PanelGrabacion) {
        panelActual = panel
        let yaInstalado = objc_getAssociatedObject(
            panel,
            &AsociadosAccesoRecoveryAudioIPad.instalado) as? UIBarButtonItem

        guard requiereAtencion else {
            if let yaInstalado {
                panel.navigationItem.rightBarButtonItems =
                    panel.navigationItem.rightBarButtonItems?.filter {
                        $0 !== yaInstalado
                    }
                objc_setAssociatedObject(
                    panel,
                    &AsociadosAccesoRecoveryAudioIPad.instalado,
                    nil,
                    .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
            }
            return
        }
        guard yaInstalado == nil else { return }

        let boton = UIBarButtonItem(
            image: UIImage(systemName: "waveform.badge.exclamationmark"),
            style: .plain,
            target: self,
            action: #selector(abrirRecovery(_:)))
        boton.accessibilityLabel = "Recuperar grabaciones interrumpidas"
        boton.accessibilityIdentifier = "audio.recovery.abrir"
        boton.tintColor = .systemOrange

        var actuales = panel.navigationItem.rightBarButtonItems ?? []
        if actuales.isEmpty, let unico = panel.navigationItem.rightBarButtonItem {
            actuales = [unico]
        }
        if !actuales.contains(where: { $0 === boton }) {
            actuales.append(boton)
        }
        panel.navigationItem.rightBarButtonItems = actuales
        objc_setAssociatedObject(
            panel,
            &AsociadosAccesoRecoveryAudioIPad.instalado,
            boton,
            .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }

    @objc private func abrirRecovery(_ sender: UIBarButtonItem) {
        guard let panel = panelActual,
              panel.presentedViewController == nil else { return }
        let recovery = PanelRecoveryAudioIPad()
        let nav = UINavigationController(rootViewController: recovery)
        nav.modalPresentationStyle = .formSheet
        panel.present(nav, animated: true)
    }
}
