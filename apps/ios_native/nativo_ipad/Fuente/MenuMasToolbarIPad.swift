import UIKit

@MainActor
final class CentroMenuMasToolbarIPad {
    static let compartido = CentroMenuMasToolbarIPad()

    func abrir() {
        guard let presentador = presentadorActivo(),
              presentador.presentedViewController == nil else { return }
        let hoja = UIAlertController(
            title: "Más herramientas",
            message: nil,
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(
            title: "Buscar escritura manuscrita",
            style: .default) { _ in
                CentroBusquedaManuscritaIPad.compartido.abrirDesdeVentanaActiva()
            })
        hoja.addAction(UIAlertAction(
            title: "Ver marcadores",
            style: .default) { _ in
                CentroMarcadoresRegionIPad.compartido.mostrarLista()
            })
        hoja.addAction(UIAlertAction(
            title: "Recovery de audio",
            style: .default) { [weak self] _ in
                self?.abrirRecoveryAudio()
            })
        hoja.addAction(UIAlertAction(
            title: "QA vs Goodnotes",
            style: .default) { [weak self] _ in
                self?.abrirQAGoodnotes()
            })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = presentador.view
        presentador.present(hoja, animated: true)
    }

    private func abrirRecoveryAudio() {
        guard let presentador = presentadorActivo(),
              presentador.presentedViewController == nil else { return }
        let panel = PanelRecoveryAudioIPad()
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .formSheet
        presentador.present(nav, animated: true)
    }

    private func abrirQAGoodnotes() {
        guard let presentador = presentadorActivo(),
              presentador.presentedViewController == nil else { return }
        let panel = PanelComparativaGoodnotesIPad()
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .formSheet
        presentador.present(nav, animated: true)
    }

    private func presentadorActivo() -> UIViewController? {
        let escenas = UIApplication.shared.connectedScenes.compactMap {
            $0 as? UIWindowScene
        }.filter { $0.activationState == .foregroundActive }
        let ventana = escenas.flatMap(\.windows).first(where: \.isKeyWindow)
            ?? escenas.flatMap(\.windows).first(where: { !$0.isHidden })
        guard let raiz = ventana?.rootViewController else { return nil }
        return superior(raiz)
    }

    private func superior(_ controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return superior(presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return superior(visible)
        }
        if let tabs = controlador as? UITabBarController,
           let seleccionado = tabs.selectedViewController {
            return superior(seleccionado)
        }
        return controlador
    }
}
