import UIKit

@MainActor
final class PanelBusquedaManuscritaIPad: UITableViewController,
                                         UISearchResultsUpdating {
    private let repositorio: RepositorioOCRManuscritoIPad
    private let cola = DispatchQueue(
        label: "cuadernu.busqueda.manuscrita",
        qos: .userInitiated)
    private var resultados: [ResultadoBusquedaManuscritaIPad] = []
    private var consultaActual = ""
    private var generacion = UUID()

    init(repositorio: RepositorioOCRManuscritoIPad = .compartido) {
        self.repositorio = repositorio
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Buscar escritura manuscrita"
        view.accessibilityIdentifier = "busqueda.manuscrita.panel"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))

        let buscador = UISearchController(searchResultsController: nil)
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.placeholder = "Texto escrito a mano"
        buscador.searchBar.autocapitalizationType = .none
        buscador.searchBar.autocorrectionType = .no
        buscador.searchBar.accessibilityIdentifier = "busqueda.manuscrita.campo"
        navigationItem.searchController = buscador
        navigationItem.hidesSearchBarWhenScrolling = false
        definesPresentationContext = true
        tableView.keyboardDismissMode = .onDrag
        tableView.accessibilityIdentifier = "busqueda.manuscrita.resultados"
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        if consultaActual.isEmpty {
            navigationItem.searchController?.searchBar.becomeFirstResponder()
        }
    }

    func updateSearchResults(for searchController: UISearchController) {
        buscar(searchController.searchBar.text ?? "")
    }

    private func buscar(_ texto: String) {
        let consulta = texto.trimmingCharacters(in: .whitespacesAndNewlines)
        consultaActual = consulta
        generacion = UUID()
        let token = generacion

        guard !consulta.isEmpty else {
            resultados = []
            tableView.reloadData()
            return
        }

        let repositorio = self.repositorio
        cola.async { [weak self] in
            let encontrados = repositorio.buscar(consulta, limite: 200)
            DispatchQueue.main.async {
                guard let self,
                      self.generacion == token,
                      self.consultaActual == consulta else { return }
                self.resultados = encontrados
                self.tableView.reloadData()
                UIAccessibility.post(
                    notification: .announcement,
                    argument: encontrados.isEmpty
                        ? "Sin resultados manuscritos"
                        : "\(encontrados.count) resultados manuscritos")
            }
        }
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 1 }

    override func tableView(
        _ tableView: UITableView,
        numberOfRowsInSection section: Int
    ) -> Int {
        if consultaActual.isEmpty { return 1 }
        return max(resultados.count, 1)
    }

    override func tableView(
        _ tableView: UITableView,
        titleForFooterInSection section: Int
    ) -> String? {
        "Busca sólo texto derivado de escritura manuscrita confirmada por Vision. El índice es reconstruible y nunca modifica la tinta original."
    }

    override func tableView(
        _ tableView: UITableView,
        cellForRowAt indexPath: IndexPath
    ) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.detailTextLabel?.numberOfLines = 2

        guard !consultaActual.isEmpty else {
            celda.textLabel?.text = "Escriba una palabra o frase"
            celda.detailTextLabel?.text =
                "Incluye handwriting reconocido en todos los cuadernos indexados."
            celda.imageView?.image = UIImage(systemName: "pencil.and.scribble")
            celda.selectionStyle = .none
            return celda
        }
        guard resultados.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "Sin coincidencias"
            celda.detailTextLabel?.text =
                "La tinta reciente puede tardar unos segundos en entrar al índice después de terminar de escribir."
            celda.imageView?.image = UIImage(systemName: "magnifyingglass")
            celda.selectionStyle = .none
            return celda
        }

        let resultado = resultados[indexPath.row]
        let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: resultado.documentoId)
        celda.textLabel?.text = resultado.fragmento
        celda.detailTextLabel?.text = [
            ubicacion?.titulo ?? "Documento \(resultado.documentoId)",
            "x \(Int(resultado.marco.midX)) · y \(Int(resultado.marco.midY))",
        ].joined(separator: " · ")
        celda.imageView?.image = UIImage(systemName: "pencil.line")
        celda.accessoryType = .disclosureIndicator
        celda.accessibilityIdentifier = "busqueda.manuscrita.resultado.\(indexPath.row)"
        return celda
    }

    override func tableView(
        _ tableView: UITableView,
        didSelectRowAt indexPath: IndexPath
    ) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard resultados.indices.contains(indexPath.row) else { return }
        let resultado = resultados[indexPath.row]
        let mayor = max(resultado.marco.width, resultado.marco.height, 120)
        let zoom = Double(min(max(420 / mayor, 0.35), 2.5))
        let destino = DestinoEnlaceTinta(
            documentoId: resultado.documentoId,
            elementoId: nil,
            bloqueId: nil,
            x: resultado.marco.midX,
            y: resultado.marco.midY,
            zoom: zoom,
            segundoAudio: nil,
            titulo: resultado.fragmento)
        let url = destino.enlace()
        dismiss(animated: true) {
            if let url {
                CoordinadorRouterEnlacesProfundosGlobalIPad.compartido.recibir(url)
            }
        }
    }

    @objc private func cerrar() {
        generacion = UUID()
        dismiss(animated: true)
    }
}

@MainActor
final class CentroBusquedaManuscritaIPad {
    static let compartido = CentroBusquedaManuscritaIPad()

    private weak var panelPresentado: PanelBusquedaManuscritaIPad?

    func abrirDesdeVentanaActiva() {
        if let panelPresentado,
           panelPresentado.viewIfLoaded?.window != nil {
            panelPresentado.navigationItem.searchController?
                .searchBar.becomeFirstResponder()
            return
        }
        guard let presentador = presentadorActivo() else { return }
        let panel = PanelBusquedaManuscritaIPad()
        panelPresentado = panel
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        presentador.present(navegacion, animated: true)
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
