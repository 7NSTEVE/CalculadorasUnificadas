import UIKit

final class PanelErroresFramesIPad: UITableViewController {
    var alAbrirFrame: ((UUID) -> Void)?

    private let errores: [ErrorRecurrenteFrameIPad]

    init(frames: [FrameApunteIPad]) {
        let entradas = frames.compactMap { frame -> EntradaAnaliticaFrameIPad? in
            guard let estudio = frame.estudio else { return nil }
            return EntradaAnaliticaFrameIPad(
                frameId: frame.id, titulo: frame.titulo, estudio: estudio)
        }
        errores = AnalizadorErroresFramesIPad.recurrentes(entradas)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Debilidades del cuaderno"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(errores.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        errores.isEmpty ? nil : "Errores que realmente se repitieron"
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        errores.isEmpty
            ? "CuadernU solo muestra errores que usted registró al cerrar intentos como Repetir."
            : "La frecuencia proviene de intentos reales de este cuaderno; no es una inferencia de IA."
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard errores.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "Todavía no hay errores recurrentes"
            celda.detailTextLabel?.text = "Marque un intento como Repetir y anote qué falló exactamente."
            celda.imageView?.image = UIImage(systemName: "checkmark.seal")
            celda.selectionStyle = .none
            return celda
        }

        let error = errores[indexPath.row]
        celda.textLabel?.text = error.descripcion
        celda.detailTextLabel?.text = "\(error.apariciones)× · \(error.frameIds.count) frame(s) · confianza media \(Int((error.confianzaPromedio * 100).rounded()))%"
        celda.detailTextLabel?.numberOfLines = 2
        celda.imageView?.image = UIImage(systemName:
            error.apariciones >= 3 ? "exclamationmark.triangle.fill" : "arrow.clockwise.circle")
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard errores.indices.contains(indexPath.row) else { return }
        presentarFrames(errores[indexPath.row])
    }

    private func presentarFrames(_ error: ErrorRecurrenteFrameIPad) {
        let menu = UIAlertController(
            title: error.descripcion,
            message: "Elija un ejercicio donde ocurrió este error.",
            preferredStyle: .actionSheet)
        for (indice, frameId) in error.frameIds.enumerated() {
            let titulo = error.titulosFrames.indices.contains(indice)
                ? error.titulosFrames[indice] : "Ejercicio \(indice + 1)"
            menu.addAction(UIAlertAction(title: titulo, style: .default) {
                [weak self] _ in
                let callback = self?.alAbrirFrame
                self?.dismiss(animated: true) { callback?(frameId) }
            })
        }
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.sourceView = view
        present(menu, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
