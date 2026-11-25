import UIKit

final class PanelDocumentoTextoEstructuradoIPad: UITableViewController {
    private let repo: RepositorioDocumentosTextoIPad
    private var documento: DocumentoTextoEstructuradoIPad
    private var errorGuardado: Error?

    var alCambiar: ((DocumentoTextoEstructuradoIPad) -> Void)?

    init(documento: DocumentoTextoEstructuradoIPad,
         repo: RepositorioDocumentosTextoIPad = .compartido) {
        self.documento = documento
        self.repo = repo
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = documento.titulo
        tableView.keyboardDismissMode = .interactive
        tableView.dragInteractionEnabled = true
        navigationItem.rightBarButtonItems = [
            editButtonItem,
            UIBarButtonItem(barButtonSystemItem: .add, target: self, action: #selector(agregar))
        ]
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "square.and.arrow.up"),
            style: .plain, target: self, action: #selector(exportar))
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        guardar()
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        documento.bloques.count
    }

    override func tableView(_ tableView: UITableView, canMoveRowAt indexPath: IndexPath) -> Bool { true }

    override func tableView(_ tableView: UITableView,
                            moveRowAt sourceIndexPath: IndexPath,
                            to destinationIndexPath: IndexPath) {
        guard documento.mover(desde: sourceIndexPath.row, hacia: destinationIndexPath.row) else { return }
        guardar()
    }

    override func tableView(_ tableView: UITableView,
                            commit editingStyle: UITableViewCell.EditingStyle,
                            forRowAt indexPath: IndexPath) {
        guard editingStyle == .delete, documento.bloques.indices.contains(indexPath.row) else { return }
        let id = documento.bloques[indexPath.row].id
        guard documento.eliminar(id) else { return }
        tableView.reloadData(); guardar()
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let bloque = documento.bloques[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.numberOfLines = 0
        celda.detailTextLabel?.numberOfLines = 2
        celda.accessoryType = .disclosureIndicator

        switch bloque.tipo {
        case .titulo1:
            celda.textLabel?.font = .preferredFont(forTextStyle: .title1)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Título 1" : bloque.texto
        case .titulo2:
            celda.textLabel?.font = .preferredFont(forTextStyle: .title2)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Título 2" : bloque.texto
        case .titulo3:
            celda.textLabel?.font = .preferredFont(forTextStyle: .title3)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Título 3" : bloque.texto
        case .parrafo:
            celda.textLabel?.font = .preferredFont(forTextStyle: .body)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Párrafo" : bloque.texto
        case .listaVinetas:
            celda.textLabel?.text = "• \(bloque.texto)"
        case .listaNumerada:
            celda.textLabel?.text = "\(indexPath.row + 1). \(bloque.texto)"
        case .tarea:
            celda.textLabel?.text = "\(bloque.completado ? "☑︎" : "☐") \(bloque.texto)"
            celda.imageView?.image = UIImage(systemName: bloque.completado ? "checkmark.circle.fill" : "circle")
        case .cita:
            celda.textLabel?.text = "❝ \(bloque.texto)"
            celda.detailTextLabel?.text = "Cita"
        case .codigo:
            celda.textLabel?.font = .monospacedSystemFont(ofSize: 15, weight: .regular)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Código" : bloque.texto
            celda.detailTextLabel?.text = bloque.lenguajeCodigo ?? "Código"
        case .ecuacion:
            celda.textLabel?.font = .monospacedSystemFont(ofSize: 17, weight: .medium)
            celda.textLabel?.text = bloque.texto.isEmpty ? "Ecuación / LaTeX" : bloque.texto
            celda.detailTextLabel?.text = "Ecuación estructurada"
        case .tabla:
            let f = bloque.tabla?.filas.count ?? 0, c = bloque.tabla?.columnas ?? 0
            celda.textLabel?.text = "Tabla \(f)×\(c)"
            celda.detailTextLabel?.text = bloque.tabla?.filas.first?.map(\.texto).joined(separator: " · ")
        case .imagen, .archivo:
            celda.textLabel?.text = bloque.adjunto?.nombre ?? (bloque.tipo == .imagen ? "Imagen" : "Archivo")
            celda.detailTextLabel?.text = bloque.adjunto?.descripcionAccesible
            celda.imageView?.image = UIImage(systemName: bloque.tipo == .imagen ? "photo" : "paperclip")
        case .divisor:
            celda.textLabel?.text = "──────────────"
            celda.accessoryType = .none
        }
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard documento.bloques.indices.contains(indexPath.row) else { return }
        let bloque = documento.bloques[indexPath.row]
        if bloque.tipo == .tarea {
            _ = documento.alternarTarea(bloque.id)
            tableView.reloadRows(at: [indexPath], with: .automatic)
            guardar(); return
        }
        if bloque.tipo == .divisor { return }
        presentarEditor(indice: indexPath.row)
    }

    @objc private func agregar() {
        let menu = UIAlertController(title: "Nuevo bloque", message: nil, preferredStyle: .actionSheet)
        let tipos: [(String, TipoBloqueDocumentoTextoIPad)] = [
            ("Párrafo", .parrafo), ("Título", .titulo1), ("Subtítulo", .titulo2),
            ("Lista con viñetas", .listaVinetas), ("Lista numerada", .listaNumerada),
            ("Tarea", .tarea), ("Cita", .cita), ("Código", .codigo),
            ("Ecuación / LaTeX", .ecuacion), ("Tabla", .tabla), ("Divisor", .divisor)
        ]
        for (nombre, tipo) in tipos {
            menu.addAction(UIAlertAction(title: nombre, style: .default) { [weak self] _ in
                guard let self else { return }
                self.documento.insertar(BloqueDocumentoTextoIPad(tipo: tipo))
                self.tableView.reloadData(); self.guardar()
                if tipo != .divisor { self.presentarEditor(indice: self.documento.bloques.count - 1) }
            })
        }
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.barButtonItem = navigationItem.rightBarButtonItems?.last
        present(menu, animated: true)
    }

    private func presentarEditor(indice: Int) {
        guard documento.bloques.indices.contains(indice) else { return }
        let bloque = documento.bloques[indice]
        if bloque.tipo == .tabla { presentarEditorTabla(indice: indice); return }

        let alerta = UIAlertController(title: nombre(bloque.tipo), message: nil, preferredStyle: .alert)
        alerta.addTextField { campo in
            campo.text = bloque.texto
            campo.autocapitalizationType = [.codigo, .ecuacion].contains(bloque.tipo) ? .none : .sentences
            campo.placeholder = bloque.tipo == .ecuacion ? "Ej. V = I R o LaTeX" : "Contenido"
        }
        if bloque.tipo == .codigo {
            alerta.addTextField { campo in campo.text = bloque.lenguajeCodigo; campo.placeholder = "Lenguaje (Swift, C++, Python…)" }
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self, weak alerta] _ in
            guard let self, var b = self.documento.bloques[safe: indice] else { return }
            b.texto = alerta?.textFields?.first?.text ?? ""
            if b.tipo == .codigo { b.lenguajeCodigo = alerta?.textFields?.dropFirst().first?.text }
            _ = self.documento.actualizar(b)
            self.tableView.reloadRows(at: [IndexPath(row: indice, section: 0)], with: .automatic)
            self.guardar()
        })
        present(alerta, animated: true)
    }

    private func presentarEditorTabla(indice: Int) {
        guard let tabla = documento.bloques[indice].tabla else { return }
        let alerta = UIAlertController(title: "Tabla", message: "Use tabuladores entre columnas y saltos de línea entre filas.", preferredStyle: .alert)
        alerta.addTextField { campo in
            campo.text = tabla.filas.map { $0.map(\.texto).joined(separator: "\t") }.joined(separator: " | ")
            campo.placeholder = "A\tB | C\tD"
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self, weak alerta] _ in
            guard let self, var b = self.documento.bloques[safe: indice] else { return }
            let texto = alerta?.textFields?.first?.text ?? ""
            let filasTexto = texto.components(separatedBy: " | ")
            let columnas = max(filasTexto.map { $0.components(separatedBy: "\t").count }.max() ?? 1, 1)
            var nueva = TablaDocumentoTextoIPad(filas: max(filasTexto.count, 1), columnas: columnas)
            for (f, fila) in filasTexto.enumerated() where f < nueva.filas.count {
                for (c, valor) in fila.components(separatedBy: "\t").enumerated() where c < nueva.filas[f].count {
                    nueva.filas[f][c].texto = valor
                }
            }
            b.tabla = nueva; _ = self.documento.actualizar(b)
            self.tableView.reloadRows(at: [IndexPath(row: indice, section: 0)], with: .automatic)
            self.guardar()
        })
        present(alerta, animated: true)
    }

    @objc private func exportar() {
        let md = ExportadorDocumentoTextoIPad.markdown(documento)
        let controlador = UIActivityViewController(activityItems: [md], applicationActivities: nil)
        controlador.popoverPresentationController?.barButtonItem = navigationItem.leftBarButtonItem
        present(controlador, animated: true)
    }

    private func guardar() {
        do {
            try repo.guardar(documento)
            errorGuardado = nil
            alCambiar?(documento)
        } catch {
            errorGuardado = error
            navigationItem.prompt = "No se pudo guardar: \(error.localizedDescription)"
        }
    }

    private func nombre(_ tipo: TipoBloqueDocumentoTextoIPad) -> String {
        switch tipo {
        case .titulo1: return "Título 1"; case .titulo2: return "Título 2"; case .titulo3: return "Título 3"
        case .parrafo: return "Párrafo"; case .listaVinetas: return "Lista"; case .listaNumerada: return "Lista numerada"
        case .tarea: return "Tarea"; case .cita: return "Cita"; case .codigo: return "Código"; case .ecuacion: return "Ecuación"
        case .tabla: return "Tabla"; case .imagen: return "Imagen"; case .archivo: return "Archivo"; case .divisor: return "Divisor"
        }
    }
}

private extension Collection {
    subscript(safe index: Index) -> Element? { indices.contains(index) ? self[index] : nil }
}