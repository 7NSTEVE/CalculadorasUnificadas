import UIKit

@MainActor
final class PanelBacklinksDocumentoIPad: UITableViewController {
    private let documento: DocumentoPersistenteID
    private var backlinks: [BacklinkCuadernUIPad] = []

    init(documento: DocumentoPersistenteID) {
        self.documento = documento
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Backlinks"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "backlink")
        recargar()
    }

    private func recargar() {
        backlinks = RepositorioBacklinksIPad.compartido
            .backlinks(haciaDocumento: documento)
        tableView.reloadData()
        backgroundView()
    }

    private func backgroundView() {
        guard backlinks.isEmpty else {
            tableView.backgroundView = nil
            return
        }
        let etiqueta = UILabel()
        etiqueta.text = "Aún no hay notas que enlacen hacia este documento."
        etiqueta.textAlignment = .center
        etiqueta.numberOfLines = 0
        etiqueta.textColor = .secondaryLabel
        etiqueta.font = .preferredFont(forTextStyle: .body)
        etiqueta.accessibilityIdentifier = "backlinks.vacio"
        tableView.backgroundView = etiqueta
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        backlinks.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        let backlink = backlinks[indexPath.row]
        celda.textLabel?.text = backlink.etiqueta.isEmpty
            ? "Nota relacionada" : backlink.etiqueta
        var detalle = backlink.contexto
        if let pagina = backlink.paginaOrigen {
            let sufijo = "Página \(pagina + 1)"
            detalle = detalle.isEmpty ? sufijo : "\(sufijo) · \(detalle)"
        }
        celda.detailTextLabel?.text = detalle
        celda.detailTextLabel?.numberOfLines = 2
        celda.accessoryType = .disclosureIndicator
        celda.accessibilityIdentifier = "backlinks.fila.\(indexPath.row)"
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard backlinks.indices.contains(indexPath.row),
              let escena = view.window?.windowScene else { return }
        let backlink = backlinks[indexPath.row]
        let destino = DestinoEnlaceIPad.documento(
            backlink.documentoOrigen,
            pagina: backlink.paginaOrigen,
            frameId: backlink.frameOrigen)
        dismiss(animated: true) {
            _ = CoordinadorNavegacionEnlacesIPad.compartido
                .navegar(destino, desde: escena)
        }
    }

    @objc private func cerrar() {
        dismiss(animated: true)
    }

    static func presentar(documento: DocumentoPersistenteID, desde vista: UIView) {
        guard let escena = vista.window?.windowScene,
              let raiz = escena.windows.first(where: \.isKeyWindow)?.rootViewController
                ?? escena.windows.first(where: { !$0.isHidden })?.rootViewController else {
            return
        }
        let panel = PanelBacklinksDocumentoIPad(documento: documento)
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .formSheet
        superior(raiz).present(nav, animated: true)
    }

    private static func superior(_ controlador: UIViewController) -> UIViewController {
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
        if let split = controlador as? UISplitViewController {
            for hijo in split.viewControllers.reversed() where !hijo.view.isHidden {
                return superior(hijo)
            }
        }
        return controlador
    }
}
