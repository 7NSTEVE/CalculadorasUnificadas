import UIKit

final class PanelPlantillasPaginaIPad: UITableViewController, UISearchResultsUpdating {
    var alAplicar: ((PlantillaPaginaIPad) -> Void)?
    private let biblioteca: BibliotecaPlantillasPaginaIPad
    private let buscador = UISearchController(searchResultsController: nil)
    private var plantillas: [PlantillaPaginaIPad] = []

    init(biblioteca: BibliotecaPlantillasPaginaIPad = .compartido) {
        self.biblioteca = biblioteca
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Plantillas"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.placeholder = "Cornell, Bode, cuadrícula…"
        navigationItem.searchController = buscador
        refrescar()
    }

    func updateSearchResults(for searchController: UISearchController) { refrescar() }
    private func refrescar() {
        plantillas = biblioteca.buscar(buscador.searchBar.text ?? "")
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { max(grupos.count, 1) }
    private var grupos: [(String, [PlantillaPaginaIPad])] {
        Dictionary(grouping: plantillas, by: \.categoria).sorted { $0.key < $1.key }
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        grupos.indices.contains(section) ? grupos[section].0 : nil
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        guard grupos.indices.contains(section) else { return 1 }
        return max(grupos[section].1.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard grupos.indices.contains(indexPath.section),
              grupos[indexPath.section].1.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "Sin plantillas"
            celda.selectionStyle = .none; return celda
        }
        let p = grupos[indexPath.section].1[indexPath.row]
        celda.textLabel?.text = p.nombre
        let tamaño = "\(Int(p.tamano.anchoPuntos))×\(Int(p.tamano.altoPuntos)) pt"
        celda.detailTextLabel?.text = "\(p.pauta.nombre) · \(tamaño)"
        celda.imageView?.image = UIImage(systemName: icono(p.pauta))
        celda.accessoryType = biblioteca.esFavorita(p.id) ? .checkmark : .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard grupos.indices.contains(indexPath.section),
              grupos[indexPath.section].1.indices.contains(indexPath.row) else { return }
        let p = grupos[indexPath.section].1[indexPath.row]
        alAplicar?(p); dismiss(animated: true)
    }
    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard grupos.indices.contains(indexPath.section),
              grupos[indexPath.section].1.indices.contains(indexPath.row) else { return nil }
        let p = grupos[indexPath.section].1[indexPath.row]
        let favorito = UIContextualAction(style: .normal, title: "Favorito") {
            [weak self] _, _, fin in
            guard let self else { fin(false); return }
            biblioteca.favorito(p.id, valor: !biblioteca.esFavorita(p.id)); refrescar(); fin(true)
        }
        let duplicar = UIContextualAction(style: .normal, title: "Duplicar") {
            [weak self] _, _, fin in
            do { _ = try self?.biblioteca.duplicar(p.id); self?.refrescar(); fin(true) }
            catch { fin(false) }
        }
        return UISwipeActionsConfiguration(actions: [favorito, duplicar])
    }

    private func icono(_ pauta: TipoPautaPlantillaIPad) -> String {
        switch pauta {
        case .lisa: return "doc"
        case .lineas: return "line.3.horizontal"
        case .cuadricula: return "grid"
        case .puntos: return "circle.grid.3x3"
        case .cornell: return "rectangle.split.2x1"
        case .isometrica, .hexagonal: return "hexagon"
        case .polar: return "scope"
        case .semilog: return "chart.xyaxis.line"
        case .ingenieria: return "ruler"
        }
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
