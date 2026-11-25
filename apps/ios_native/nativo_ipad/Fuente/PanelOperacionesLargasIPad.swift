import UIKit

final class PanelOperacionesLargasIPad: UITableViewController {
    private let coordinador: CoordinadorOperacionesLargasIPad
    private var operaciones: [InstantaneaOperacionLargaIPad] = []

    init(coordinador: CoordinadorOperacionesLargasIPad = .compartido) {
        self.coordinador = coordinador
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Operaciones"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Limpiar", style: .plain, target: self, action: #selector(limpiar))
        NotificationCenter.default.addObserver(self, selector: #selector(recargar),
                                               name: CoordinadorOperacionesLargasIPad.cambio,
                                               object: nil)
        recargar()
    }

    deinit { NotificationCenter.default.removeObserver(self) }

    @objc private func recargar() {
        operaciones = coordinador.instantaneas(incluirTerminadas: true)
        tableView.reloadData()
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        max(operaciones.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard operaciones.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "Sin operaciones"
            celda.detailTextLabel?.text = "Importaciones y exportaciones largas aparecerán aquí."
            celda.imageView?.image = UIImage(systemName: "checkmark.circle")
            celda.selectionStyle = .none
            return celda
        }
        let op = operaciones[indexPath.row]
        celda.textLabel?.text = op.titulo
        var detalle = "\(op.fase) · \(Int(op.progreso * 100))%"
        if let total = op.bytesTotales, total > 0 {
            let hechos = ByteCountFormatter.string(fromByteCount: op.bytesProcesados, countStyle: .file)
            let totalTexto = ByteCountFormatter.string(fromByteCount: total, countStyle: .file)
            detalle += " · \(hechos)/\(totalTexto)"
        }
        if let error = op.mensajeError { detalle += " · \(error)" }
        celda.detailTextLabel?.text = detalle
        celda.detailTextLabel?.numberOfLines = 3
        celda.imageView?.image = UIImage(systemName: simbolo(op.estado))
        celda.accessibilityLabel = op.titulo
        celda.accessibilityValue = detalle
        if op.puedeCancelar {
            let boton = UIButton(type: .system)
            boton.setImage(UIImage(systemName: "xmark.circle.fill"), for: .normal)
            boton.accessibilityLabel = "Cancelar \(op.titulo)"
            boton.tag = indexPath.row
            boton.addTarget(self, action: #selector(cancelar(_:)), for: .touchUpInside)
            boton.frame = CGRect(x: 0, y: 0, width: 44, height: 44)
            celda.accessoryView = boton
        }
        return celda
    }

    private func simbolo(_ estado: EstadoOperacionLargaIPad) -> String {
        switch estado {
        case .preparando: return "hourglass"
        case .ejecutando: return "arrow.triangle.2.circlepath"
        case .cancelando: return "xmark.circle"
        case .completada: return "checkmark.circle.fill"
        case .cancelada: return "nosign"
        case .fallida: return "exclamationmark.triangle.fill"
        }
    }

    @objc private func cancelar(_ boton: UIButton) {
        guard operaciones.indices.contains(boton.tag) else { return }
        coordinador.cancelar(operaciones[boton.tag].id)
        AnunciadorAccesibilidadIPad.anunciar("Cancelando \(operaciones[boton.tag].titulo)")
    }

    @objc private func limpiar() { coordinador.descartarTerminadas() }
}
