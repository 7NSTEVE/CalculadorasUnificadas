import UIKit

/// Selector ligero para llevar un problema académico al espacio manuscrito sin
/// copiar el enunciado a otra base de datos. Devuelve el objeto canónico del
/// Problem Bank y el frame conserva únicamente su `problemaId`.
final class PanelSeleccionProblemaFrameIPad: UITableViewController,
    UISearchResultsUpdating {
    var alSeleccionar: ((ProblemaBancoIPad) -> Void)?

    private let buscador = UISearchController(searchResultsController: nil)
    private var problemas: [ProblemaBancoIPad] = []

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Problem Bank"
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.placeholder = "Buscar tema, fuente o enunciado"
        navigationItem.searchController = buscador
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        refrescar()
    }

    func updateSearchResults(for searchController: UISearchController) {
        refrescar()
    }

    private func refrescar() {
        let consulta = normalizar(buscador.searchBar.text ?? "")
        problemas = CentroEstudioTecnicoIPad.compartido.instantanea().problemas
            .filter { problema in
                guard problema.estado != .archivado else { return false }
                guard !consulta.isEmpty else { return true }
                let tituloFuente = problema.fuente?.tipo == .solucion
                    ? "" : (problema.fuente?.titulo ?? "")
                return normalizar(
                    problema.tema + " " + problema.enunciado + " "
                    + problema.etiquetas.joined(separator: " ") + " "
                    + tituloFuente)
                    .contains(consulta)
            }
            .sorted(by: esMejor)
        tableView.reloadData()
    }

    private func esMejor(_ a: ProblemaBancoIPad, _ b: ProblemaBancoIPad) -> Bool {
        let pa = prioridad(a.estado)
        let pb = prioridad(b.estado)
        if pa != pb { return pa < pb }
        let fechaA = a.ultimoUso ?? a.fecha
        let fechaB = b.ultimoUso ?? b.fecha
        if fechaA != fechaB { return fechaA > fechaB }
        return a.tema.localizedCaseInsensitiveCompare(b.tema) == .orderedAscending
    }

    private func prioridad(_ estado: EstadoProblemaIPad) -> Int {
        switch estado {
        case .intentando: return 0
        case .repetir, .incorrecto: return 1
        case .pendiente: return 2
        case .correcto: return 3
        case .archivado: return 4
        }
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(problemas.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard problemas.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "No hay problemas que coincidan"
            celda.detailTextLabel?.text = "Importe un PDF/examen o guarde un problema primero."
            celda.imageView?.image = UIImage(systemName: "tray")
            celda.selectionStyle = .none
            return celda
        }
        let problema = problemas[indexPath.row]
        celda.textLabel?.text = problema.tema.isEmpty ? "Sin tema" : problema.tema
        celda.detailTextLabel?.text = detalle(problema)
        celda.detailTextLabel?.numberOfLines = 4
        celda.imageView?.image = UIImage(systemName: simbolo(problema.estado))
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard problemas.indices.contains(indexPath.row) else { return }
        let problema = problemas[indexPath.row]
        let callback = alSeleccionar
        dismiss(animated: true) { callback?(problema) }
    }

    override func tableView(
        _ tableView: UITableView,
        contextMenuConfigurationForRowAt indexPath: IndexPath,
        point: CGPoint
    ) -> UIContextMenuConfiguration? {
        guard problemas.indices.contains(indexPath.row),
              let fuente = problemas[indexPath.row].fuente,
              fuente.tipo != .solucion,
              fuente.hashArchivo != nil else { return nil }
        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) {
            [weak self] _ in
            UIMenu(children: [
                UIAction(title: "Ver fuente original",
                         image: UIImage(systemName: "doc.text.magnifyingglass")) { _ in
                    guard let self else { return }
                    self.present(
                        UINavigationController(
                            rootViewController: ControladorFuenteProblemaIPad(
                                referencia: fuente)),
                        animated: true)
                }
            ])
        }
    }

    private func detalle(_ problema: ProblemaBancoIPad) -> String {
        var lineas: [String] = []
        if let fuente = problema.fuente, fuente.tipo != .solucion {
            var origen = fuente.titulo.isEmpty ? fuente.tipo.rawValue : fuente.titulo
            if let pagina = fuente.pagina { origen += " · p. \(pagina + 1)" }
            lineas.append(origen)
        }
        lineas.append(String(problema.enunciado.prefix(180)))
        return lineas.joined(separator: "\n")
    }

    private func simbolo(_ estado: EstadoProblemaIPad) -> String {
        switch estado {
        case .intentando: return "timer"
        case .repetir, .incorrecto: return "arrow.clockwise.circle"
        case .pendiente: return "circle"
        case .correcto: return "checkmark.circle"
        case .archivado: return "archivebox"
        }
    }

    private func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current).lowercased()
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
