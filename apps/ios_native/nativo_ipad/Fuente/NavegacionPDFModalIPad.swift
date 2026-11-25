import UIKit

extension ControladorPDFNavegableIPad {
    func prepararPresentacionModal() {
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrarNavegadorPDF))
    }

    @objc func cerrarNavegadorPDF() {
        dismiss(animated: true)
    }
}
