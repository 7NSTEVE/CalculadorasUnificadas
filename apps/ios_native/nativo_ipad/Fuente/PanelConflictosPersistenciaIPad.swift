import UIKit

final class PanelConflictosPersistenciaIPad: UITableViewController {
    var alResolverTodos: (() -> Void)?
    private let gestor: GestorDiagnosticosPersistenciaIPad
    private var conflictos: [ConflictoPersistenciaListadoIPad] = []

    init(gestor: GestorDiagnosticosPersistenciaIPad = .init()) {
        self.gestor = gestor
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Recuperación de datos"
        navigationItem.hidesBackButton = true
        recargar()
    }

    private func recargar() {
        conflictos = gestor.listarConflictos()
        tableView.reloadData()
        if conflictos.isEmpty { alResolverTodos?() }
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Qué ocurrió" : "Documentos bloqueados"
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 1 : conflictos.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            cell.textLabel?.text = "CuadernU evitó sobrescribir cambios desconocidos"
            cell.detailTextLabel?.text = "Los archivos conflictivos se conservaron intactos. Abra cada caso para guardar evidencia antes de continuar."
            cell.detailTextLabel?.numberOfLines = 4
            cell.imageView?.image = UIImage(systemName: "exclamationmark.shield.fill")
            cell.selectionStyle = .none
            cell.accessibilityLabel = "Conflicto de recuperación detectado"
            cell.accessibilityValue = cell.detailTextLabel?.text
            return cell
        }

        let conflicto = conflictos[indexPath.row]
        cell.textLabel?.text = "Documento \(conflicto.documentoId.description.prefix(8))…"
        cell.detailTextLabel?.text = "\(conflicto.rutas.count) archivo(s) · \(conflicto.actualizado.formatted(date: .abbreviated, time: .shortened))"
        cell.imageView?.image = UIImage(systemName: "doc.badge.ellipsis")
        cell.accessoryType = .disclosureIndicator
        cell.accessibilityLabel = "Documento con conflicto de recuperación"
        cell.accessibilityValue = cell.detailTextLabel?.text
        return cell
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard indexPath.section == 1 else { return }
        mostrar(conflictos[indexPath.row])
    }

    private func mostrar(_ conflicto: ConflictoPersistenciaListadoIPad) {
        let rutas = conflicto.rutas.map { "• \($0)" }.joined(separator: "\n")
        let alerta = UIAlertController(
            title: "Conflicto de persistencia",
            message: "CuadernU no puede decidir automáticamente qué versión conservar.\n\n\(rutas)",
            preferredStyle: .actionSheet)

        alerta.addAction(UIAlertAction(
            title: "Conservar archivos actuales + guardar evidencia",
            style: .default) { [weak self] _ in
                self?.confirmarConservarActual(conflicto)
            })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        if let popover = alerta.popoverPresentationController {
            popover.sourceView = view
            popover.sourceRect = CGRect(x: view.bounds.midX, y: view.bounds.midY,
                                        width: 1, height: 1)
        }
        present(alerta, animated: true)
    }

    private func confirmarConservarActual(_ conflicto: ConflictoPersistenciaListadoIPad) {
        let confirmar = UIAlertController(
            title: "¿Conservar la versión actual?",
            message: "Se exportará un paquete de diagnóstico con original, propuesta y versión actual. Luego se retirará el journal conflictivo. Esta acción NO modifica los archivos actuales.",
            preferredStyle: .alert)
        confirmar.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        confirmar.addAction(UIAlertAction(
            title: "Guardar evidencia y continuar",
            style: .default) { [weak self] _ in
                self?.resolverConservandoActual(conflicto)
            })
        present(confirmar, animated: true)
    }

    private func resolverConservandoActual(_ conflicto: ConflictoPersistenciaListadoIPad) {
        var destino = gestor.destinoDiagnosticoAutomatico(
            transaccionId: conflicto.id)
        if FileManager.default.fileExists(atPath: destino.path) {
            destino = destino.deletingLastPathComponent()
                .appendingPathComponent(
                    "conflicto-\(conflicto.id.uuidString.lowercased())-\(Int(Date().timeIntervalSince1970))",
                    isDirectory: true)
        }
        do {
            _ = try gestor.conservarActualesYResolver(
                transaccionId: conflicto.id,
                diagnosticoDestino: destino)
            AnunciadorAccesibilidadIPad.anunciar(
                "Conflicto resuelto conservando los archivos actuales")
            recargar()
        } catch {
            let alerta = UIAlertController(
                title: "No se pudo cerrar el conflicto",
                message: error.localizedDescription,
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
            present(alerta, animated: true)
        }
    }
}
