import UIKit

final class PanelCarpetasBibliotecaIPad: UITableViewController {
    var alAbrirCuaderno: ((CuadernoBibliotecaIPad) -> Void)?
    private let repo: RepositorioOrganizacionBibliotecaIPad
    private let gestor: GestionBibliotecaCuadernU
    private let carpetaId: UUID?
    private var carpetas: [CarpetaBibliotecaIPad] = []
    private var cuadernos: [CuadernoBibliotecaIPad] = []

    init(carpetaId: UUID? = nil,
         repo: RepositorioOrganizacionBibliotecaIPad = .compartido,
         gestor: GestionBibliotecaCuadernU = GestorArchivosCuadernU.compartido) {
        self.carpetaId = carpetaId; self.repo = repo; self.gestor = gestor
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = repo.ruta(carpetaId: carpetaId).last?.nombre ?? "Biblioteca"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "folder.badge.plus"), style: .plain,
            target: self, action: #selector(crearCarpeta))
        navigationItem.rightBarButtonItem?.accessibilityLabel = "Crear carpeta"
        recargar()
    }

    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); recargar() }

    private func recargar() {
        carpetas = repo.carpetas(en: carpetaId)
        let todos = (try? gestor.listarCuadernos()) ?? []
        cuadernos = todos.filter { repo.metadata(cuadernoId: $0.id).carpetaId == carpetaId }
            .sorted {
                let a = repo.metadata(cuadernoId: $0.id), b = repo.metadata(cuadernoId: $1.id)
                if a.fijado != b.fijado { return a.fijado }
                if a.favorito != b.favorito { return a.favorito }
                return $0.fechaModificacion > $1.fechaModificacion
            }
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Carpetas" : "Cuadernos"
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        section == 0 ? carpetas.count : cuadernos.count
    }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let item = carpetas[indexPath.row]
            cell.textLabel?.text = item.nombre
            cell.imageView?.image = UIImage(systemName: item.simboloSF)
            cell.accessoryType = .disclosureIndicator
            cell.accessibilityLabel = "Carpeta \(item.nombre)"
        } else {
            let item = cuadernos[indexPath.row], meta = repo.metadata(cuadernoId: item.id)
            cell.textLabel?.text = item.titulo
            cell.detailTextLabel?.text = meta.fijado ? "Fijado" : (meta.favorito ? "Favorito" : nil)
            cell.imageView?.image = UIImage(systemName: meta.fijado ? "pin.fill" : (meta.favorito ? "star.fill" : "book.closed.fill"))
            cell.accessoryType = .disclosureIndicator
        }
        return cell
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if indexPath.section == 0 {
            let siguiente = PanelCarpetasBibliotecaIPad(carpetaId: carpetas[indexPath.row].id,
                                                        repo: repo, gestor: gestor)
            siguiente.alAbrirCuaderno = alAbrirCuaderno
            navigationController?.pushViewController(siguiente, animated: true)
        } else { alAbrirCuaderno?(cuadernos[indexPath.row]) }
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard indexPath.section == 1 else { return nil }
        let item = cuadernos[indexPath.row], meta = repo.metadata(cuadernoId: item.id)
        let favorito = UIContextualAction(style: .normal, title: meta.favorito ? "Quitar ★" : "★") { [weak self] _,_,fin in
            self?.repo.favorito(item.id, !meta.favorito); self?.recargar(); fin(true)
        }
        let fijar = UIContextualAction(style: .normal, title: meta.fijado ? "Desfijar" : "Fijar") { [weak self] _,_,fin in
            self?.repo.fijado(item.id, !meta.fijado); self?.recargar(); fin(true)
        }
        return UISwipeActionsConfiguration(actions: [fijar, favorito])
    }

    @objc private func crearCarpeta() {
        let a = UIAlertController(title: "Nueva carpeta", message: nil, preferredStyle: .alert)
        a.addTextField { $0.placeholder = "Nombre" }
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        a.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self, weak a] _ in
            guard let self else { return }
            _ = try? self.repo.crearCarpeta(nombre: a?.textFields?.first?.text ?? "", padreId: self.carpetaId)
            self.recargar()
        })
        present(a, animated: true)
    }
}
