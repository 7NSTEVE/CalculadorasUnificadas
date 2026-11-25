import UIKit

final class PanelBusquedaUniversalIPad: UITableViewController, UISearchResultsUpdating {
    var alAbrirResultado: ((ResultadoBusquedaUniversalIPad) -> Void)?
    private let indice: IndiceBusquedaUniversalIPad
    private let buscador = UISearchController(searchResultsController: nil)
    private var resultados: [ResultadoBusquedaUniversalIPad] = []
    private var categorias: Set<CategoriaBusquedaUniversalIPad> = []
    private let cursoId: UUID?
    private var trabajoBusqueda: DispatchWorkItem?
    private var generacionBusqueda = 0
    private var preparandoIndice = false
    private var cantidadElementosIndexados = 0

    init(cursoId: UUID? = nil, indice: IndiceBusquedaUniversalIPad = .compartido) {
        self.cursoId = cursoId
        self.indice = indice
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Buscar en CuadernU"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Tipo", style: .plain, target: self, action: #selector(elegirTipo))
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.autocapitalizationType = .none
        buscador.searchBar.autocorrectionType = .no
        buscador.searchBar.placeholder = "OCR, PDF, audio, problemas, fórmulas…"
        buscador.searchBar.accessibilityLabel = "Buscar en todos mis materiales"
        navigationItem.searchController = buscador
        navigationItem.hidesSearchBarWhenScrolling = false
        definesPresentationContext = true

        // Contar una vez por generación del índice evita copiar el catálogo en
        // cada pulsación. Ese tamaño se adjunta a la telemetría de latencia.
        cantidadElementosIndexados = indice.instantanea().count
        programarRefresco(inmediato: true)
        prepararIndiceSiHaceFalta()
    }

    deinit { trabajoBusqueda?.cancel() }

    func updateSearchResults(for searchController: UISearchController) {
        programarRefresco()
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(resultados.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard resultados.indices.contains(indexPath.row) else {
            if preparandoIndice {
                celda.textLabel?.text = "Actualizando el índice…"
                celda.imageView?.image = UIImage(systemName: "arrow.triangle.2.circlepath")
            } else {
                celda.textLabel?.text = buscador.searchBar.text?.isEmpty == false
                    ? "Sin coincidencias" : "Busque en sus materiales"
                celda.imageView?.image = UIImage(systemName: "magnifyingglass")
            }
            celda.textLabel?.textColor = .secondaryLabel
            celda.selectionStyle = .none
            celda.accessibilityLabel = celda.textLabel?.text
            return celda
        }

        let resultado = resultados[indexPath.row]
        let entrada = resultado.entrada
        celda.textLabel?.text = entrada.titulo.isEmpty
            ? entrada.categoria.nombre : entrada.titulo
        var detalle = [entrada.categoria.nombre]
        if let fuente = entrada.fuente, !fuente.isEmpty { detalle.append(fuente) }
        if let pagina = entrada.localizacion.pagina { detalle.append("p. \(pagina + 1)") }
        if let segundo = entrada.localizacion.segundoAudio {
            detalle.append(String(format: "%d:%02d", Int(segundo) / 60, Int(segundo) % 60))
        }
        if !entrada.contenido.isEmpty {
            detalle.append(String(entrada.contenido.prefix(150)))
        }
        celda.detailTextLabel?.text = detalle.joined(separator: " · ")
        celda.detailTextLabel?.numberOfLines = 3
        celda.accessoryType = .disclosureIndicator
        celda.imageView?.image = UIImage(systemName: simbolo(entrada.categoria))
        celda.accessibilityLabel = [celda.textLabel?.text, celda.detailTextLabel?.text]
            .compactMap { $0 }.joined(separator: ", ")
        celda.accessibilityHint = "Abrir resultado"
        return celda
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard resultados.indices.contains(indexPath.row) else { return }
        let resultado = resultados[indexPath.row]
        if let alAbrirResultado {
            alAbrirResultado(resultado)
            return
        }
        let alerta = UIAlertController(
            title: resultado.entrada.titulo,
            message: resultado.entrada.contenido,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        present(alerta, animated: true)
    }

    private func prepararIndiceSiHaceFalta() {
        preparandoIndice = true
        navigationItem.prompt = "Actualizando índice en segundo plano…"
        tableView.reloadData()
        CoordinadorReindexacionBusquedaUniversalIPad.compartido.solicitar {
            [weak self] in
            guard let self else { return }
            self.preparandoIndice = false
            self.navigationItem.prompt = nil
            self.cantidadElementosIndexados = self.indice.instantanea().count
            self.programarRefresco(inmediato: true)
        }
    }

    /// 140 ms evita ejecutar un ranking completo por cada evento del teclado,
    /// pero sigue sintiéndose instantáneo al escribir. Las generaciones impiden
    /// que una consulta lenta sustituya resultados de una consulta más reciente.
    private func programarRefresco(inmediato: Bool = false) {
        trabajoBusqueda?.cancel()
        generacionBusqueda &+= 1
        let generacion = generacionBusqueda
        let elementosIndexados = cantidadElementosIndexados
        let consulta = ConsultaBusquedaUniversalIPad(
            texto: buscador.searchBar.text ?? "",
            categorias: categorias,
            cursoId: cursoId,
            limite: 100)

        let trabajo = DispatchWorkItem { [weak self] in
            guard let self else { return }
            let inicio = ProcessInfo.processInfo.systemUptime
            let encontrados = self.indice.buscar(consulta)
            let duracionMs = (ProcessInfo.processInfo.systemUptime - inicio) * 1_000
            TelemetriaBusquedaUniversalIPad.compartido.registrar(
                duracionMs: duracionMs,
                elementosIndexados: elementosIndexados)
            DispatchQueue.main.async { [weak self] in
                guard let self, generacion == self.generacionBusqueda else { return }
                self.resultados = encontrados
                self.tableView.reloadData()
            }
        }
        trabajoBusqueda = trabajo
        let demora: DispatchTimeInterval = inmediato ? .milliseconds(0) : .milliseconds(140)
        DispatchQueue.global(qos: .userInitiated).asyncAfter(
            deadline: .now() + demora,
            execute: trabajo)
    }

    @objc private func elegirTipo() {
        let hoja = UIAlertController(title: "Tipo", message: nil, preferredStyle: .actionSheet)
        let opciones: [(String, Set<CategoriaBusquedaUniversalIPad>)] = [
            ("Todos", []),
            ("Problemas y errores", [.problema, .error]),
            ("Fórmulas", [.formula]),
            ("Elementos", [.objeto]),
            ("Escritura indexada y OCR", [.manuscrito, .ocr]),
            ("PDF y texto", [.pdf, .texto]),
            ("Transcripción", [.transcripcion]),
            ("Fuentes y etiquetas", [.fuente, .etiqueta])
        ]
        for (titulo, valor) in opciones {
            hoja.addAction(UIAlertAction(title: titulo, style: .default) { [weak self] _ in
                self?.categorias = valor
                self?.programarRefresco(inmediato: true)
            })
        }
        // Conserva el buscador especializado que analiza los trazos del lienzo
        // actual. La búsqueda universal no debe eliminar una capacidad existente
        // mientras la indexación incremental de tinta se termina de unificar.
        hoja.addAction(UIAlertAction(
            title: "Buscar manuscrito en este cuaderno…",
            style: .default) { [weak self] _ in
                self?.dismiss(animated: true) {
                    CentroBusquedaManuscritaIPad.compartido.abrirDesdeVentanaActiva()
                }
            })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.barButtonItem = navigationItem.rightBarButtonItem
        present(hoja, animated: true)
    }

    private func simbolo(_ categoria: CategoriaBusquedaUniversalIPad) -> String {
        switch categoria {
        case .manuscrito: return "pencil.and.outline"
        case .texto: return "text.alignleft"
        case .pdf: return "doc.richtext"
        case .ocr: return "viewfinder"
        case .transcripcion: return "waveform"
        case .formula: return "function"
        case .problema: return "questionmark.square"
        case .error: return "exclamationmark.triangle"
        case .etiqueta: return "tag"
        case .fuente: return "books.vertical"
        case .objeto: return "square.on.square"
        }
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
