import UIKit

final class PanelBibliotecaElementosReutilizablesIPad: UITableViewController, UISearchResultsUpdating {
    var alInsertarElemento: ((ElementoReutilizableIPad, Data) -> Void)?
    private let biblioteca: BibliotecaElementosReutilizablesIPad
    private let buscador = UISearchController(searchResultsController: nil)
    private var elementos: [ElementoReutilizableIPad] = []

    init(biblioteca: BibliotecaElementosReutilizablesIPad = .compartido) {
        self.biblioteca = biblioteca
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Elementos reutilizables"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(crearColeccion))
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.placeholder = "Fórmula, circuito, figura…"
        navigationItem.searchController = buscador
        refrescar()
    }

    func updateSearchResults(for searchController: UISearchController) { refrescar() }

    private func refrescar() {
        elementos = biblioteca.buscar(buscador.searchBar.text ?? "", limite: 500)
        tableView.reloadData()
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        max(elementos.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard elementos.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "No hay elementos"
            celda.detailTextLabel?.text = "Cree una colección y guarde una selección del lienzo."
            celda.imageView?.image = UIImage(systemName: "square.grid.2x2")
            celda.selectionStyle = .none
            return celda
        }
        let item = elementos[indexPath.row]
        celda.textLabel?.text = item.nombre
        celda.detailTextLabel?.text = ([item.tipo.nombre] + item.etiquetas.prefix(4))
            .joined(separator: " · ")
        celda.imageView?.image = UIImage(systemName: item.favorito ? "star.fill" : "square.on.square")
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard elementos.indices.contains(indexPath.row) else { return }
        let item = elementos[indexPath.row]
        do {
            guard let payload = try biblioteca.payload(elementoId: item.id) else { return }
            biblioteca.registrarUso(elementoId: item.id)
            alInsertarElemento?(item, payload)
            dismiss(animated: true)
        } catch { mostrar(error) }
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard elementos.indices.contains(indexPath.row) else { return nil }
        let item = elementos[indexPath.row]
        let favorito = UIContextualAction(style: .normal, title: "Favorito") {
            [weak self] _, _, fin in
            self?.biblioteca.favorito(elementoId: item.id, valor: !item.favorito)
            self?.refrescar(); fin(true)
        }
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") {
            [weak self] _, _, fin in
            self?.biblioteca.eliminar(elementoId: item.id)
            self?.refrescar(); fin(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar, favorito])
    }

    @objc private func crearColeccion() {
        let a = UIAlertController(title: "Nueva colección", message: nil, preferredStyle: .alert)
        a.addTextField { $0.placeholder = "Circuitos frecuentes" }
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        a.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self] _ in
            guard let self, let nombre = a.textFields?.first?.text else { return }
            do { _ = try biblioteca.crearColeccion(nombre: nombre); refrescar() }
            catch { mostrar(error) }
        })
        present(a, animated: true)
    }

    private func mostrar(_ error: Error) {
        let a = UIAlertController(title: "Elementos", message: error.localizedDescription,
                                  preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Cerrar", style: .cancel)); present(a, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
