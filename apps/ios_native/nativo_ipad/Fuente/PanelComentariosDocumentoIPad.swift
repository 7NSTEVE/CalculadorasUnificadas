import UIKit

final class PanelComentariosDocumentoIPad: UITableViewController {
    var alAbrirAnclaje: ((AnclajeComentarioDocumentoIPad) -> Void)?
    private let documentoId: DocumentoPersistenteID
    private let repo: RepositorioComentariosDocumentoIPad
    private var hilos: [HiloComentarioDocumentoIPad] = []
    private var mostrarResueltos = false

    init(documentoId: DocumentoPersistenteID,
         repositorio: RepositorioComentariosDocumentoIPad = .compartido) {
        self.documentoId = documentoId; repo = repositorio
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Comentarios"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(barButtonSystemItem: .add, target: self, action: #selector(nuevo)),
            UIBarButtonItem(title: "Resueltos", style: .plain, target: self, action: #selector(filtro))]
        refrescar()
    }

    func crearComentario(en anclaje: AnclajeComentarioDocumentoIPad) {
        guard anclaje.documentoId == documentoId else { return }
        pedir("Nuevo comentario") { [weak self] texto in
            guard let self else { return }
            do { _ = try repo.crearHilo(anclaje: anclaje, autorId: "local",
                autorNombre: "Yo", texto: texto); refrescar() } catch { mostrar(error) }
        }
    }

    private func refrescar() {
        hilos = repo.hilos(documentoId: documentoId, incluirResueltos: mostrarResueltos)
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        max(hilos.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard hilos.indices.contains(indexPath.row) else {
            c.textLabel?.text = "No hay comentarios pendientes"; c.selectionStyle = .none; return c
        }
        let h = hilos[indexPath.row]
        c.textLabel?.text = h.ultimoMensaje?.texto ?? "Comentario"
        var d = ["\(h.mensajes.count) mensaje(s)"]
        if let p = h.anclaje.pagina { d.insert("p. \(p + 1)", at: 0) }
        if let x = h.anclaje.contexto, !x.isEmpty { d.append(x) }
        c.detailTextLabel?.text = d.joined(separator: " · ")
        c.imageView?.image = UIImage(systemName: h.resuelto ? "checkmark.bubble" : "text.bubble")
        c.accessoryType = .disclosureIndicator; return c
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard hilos.indices.contains(indexPath.row) else { return }; abrir(hilos[indexPath.row])
    }

    private func abrir(_ h: HiloComentarioDocumentoIPad) {
        let a = UIAlertController(title: "Comentario", message: h.ultimoMensaje?.texto,
                                  preferredStyle: .actionSheet)
        a.addAction(UIAlertAction(title: "Abrir anclaje", style: .default) { [weak self] _ in
            self?.alAbrirAnclaje?(h.anclaje)
        })
        a.addAction(UIAlertAction(title: "Responder", style: .default) { [weak self] _ in
            self?.pedir("Responder") { texto in
                guard let self else { return }
                do { _ = try repo.responder(hiloId: h.id, documentoId: documentoId,
                    autorId: "local", autorNombre: "Yo", texto: texto); refrescar() }
                catch { mostrar(error) }
            }
        })
        a.addAction(UIAlertAction(title: h.resuelto ? "Reabrir" : "Resolver", style: .default) {
            [weak self] _ in guard let self else { return }
            do { try repo.resolver(hiloId: h.id, documentoId: documentoId,
                                   valor: !h.resuelto); refrescar() } catch { mostrar(error) }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        a.popoverPresentationController?.sourceView = view; present(a, animated: true)
    }

    private func pedir(_ titulo: String, _ completar: @escaping (String) -> Void) {
        let a = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        a.addTextField { $0.placeholder = "Comentario" }
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        a.addAction(UIAlertAction(title: "Guardar", style: .default) { _ in
            if let t = a.textFields?.first?.text, !t.isEmpty { completar(t) }
        }); present(a, animated: true)
    }
    private func mostrar(_ error: Error) {
        let a = UIAlertController(title: "Comentarios", message: error.localizedDescription,
                                  preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Cerrar", style: .cancel)); present(a, animated: true)
    }
    @objc private func nuevo() { crearComentario(en: .init(documentoId: documentoId, contexto: "Documento")) }
    @objc private func filtro() { mostrarResueltos.toggle(); refrescar() }
    @objc private func cerrar() { dismiss(animated: true) }
}
