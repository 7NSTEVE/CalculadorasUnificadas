import UIKit

// Compatibilidad temporal para ContextoVisible, que conserva el presentador como
// referencia débil. Evita forzar unwraps si una escena se cierra entre resolver
// el lienzo y presentar la UI. Se retirará cuando el owner del toolbar se mueva
// al coordinador y el contexto pueda ser una referencia fuerte de vida corta.
extension Optional where Wrapped: UIViewController {
    @MainActor
    func presentarSiSigueVisible(
        _ controlador: UIViewController,
        animated: Bool = true
    ) {
        guard let presentador = self,
              presentador.viewIfLoaded?.window != nil else { return }
        presentador.present(controlador, animated: animated)
    }

    @MainActor
    func present(_ controlador: UIViewController, animated: Bool) {
        presentarSiSigueVisible(controlador, animated: animated)
    }
}

extension CentroAccionesToolbarDocumentoIPad {
    @MainActor
    func avisar(
        _ texto: String,
        desde presentador: UIViewController?
    ) {
        guard let presentador,
              presentador.viewIfLoaded?.window != nil else { return }
        let alerta = UIAlertController(
            title: nil,
            message: texto,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        presentador.present(alerta, animated: true)
    }
}
