import Foundation
import PDFKit
import UIKit

struct EntradaOutlinePDFIPad: Equatable, Identifiable {
    var id: String { "outline:\(rutaJerarquica):\(pagina ?? -1)" }
    var titulo: String
    var pagina: Int?
    var punto: CGPoint?
    var profundidad: Int
    var rutaJerarquica: String
}

enum DestinoEnlacePDFIPad: Equatable {
    case pagina(Int, CGPoint?)
    case url(URL)
}

struct EnlacePDFIPad: Equatable, Identifiable {
    var id: String {
        switch destino {
        case .pagina(let pagina, let punto):
            return "link:\(paginaOrigen):\(indiceEnPagina):p:\(pagina):\(punto?.x ?? 0):\(punto?.y ?? 0)"
        case .url(let url):
            return "link:\(paginaOrigen):\(indiceEnPagina):u:\(url.absoluteString)"
        }
    }
    var paginaOrigen: Int
    var indiceEnPagina: Int
    var rect: CGRect
    var etiqueta: String
    var destino: DestinoEnlacePDFIPad
}

struct ResumenNavegacionPDFIPad: Equatable {
    var totalPaginas: Int
    var outline: [EntradaOutlinePDFIPad]
    var outlineTruncado: Bool
}

struct ResultadoEnlacesPDFIPad: Equatable {
    var enlaces: [EnlacePDFIPad]
    var truncado: Bool
}

enum AnalizadorNavegacionPDFIPad {
    static let maximoOutline = 2_000
    static let maximaProfundidad = 32
    static let maximoEnlaces = 5_000

    static func resumen(_ url: URL) -> ResumenNavegacionPDFIPad {
        conDocumento(url) { documento in
            guard let root = documento.outlineRoot else {
                return ResumenNavegacionPDFIPad(
                    totalPaginas: documento.pageCount,
                    outline: [], outlineTruncado: false)
            }
            var salida: [EntradaOutlinePDFIPad] = []
            var truncado = false

            func recorrer(_ nodo: PDFOutline, profundidad: Int, ruta: String) {
                guard salida.count < maximoOutline else {
                    truncado = true
                    return
                }
                guard profundidad <= maximaProfundidad else {
                    truncado = true
                    return
                }
                let cantidad = nodo.numberOfChildren
                guard cantidad > 0 else { return }
                for indice in 0..<cantidad {
                    guard salida.count < maximoOutline else {
                        truncado = true
                        return
                    }
                    guard let hijo = nodo.child(at: indice) else { continue }
                    let rutaHijo = ruta.isEmpty ? String(indice) : "\(ruta).\(indice)"
                    let destino = hijo.destination
                    let pagina = destino?.page.flatMap { pagina -> Int? in
                        let valor = documento.index(for: pagina)
                        return valor == NSNotFound ? nil : valor
                    }
                    let titulo = hijo.label?.trimmingCharacters(
                        in: .whitespacesAndNewlines)
                    salida.append(.init(
                        titulo: (titulo?.isEmpty == false ? titulo! : "Sección sin título"),
                        pagina: pagina,
                        punto: destino?.point,
                        profundidad: profundidad,
                        rutaJerarquica: rutaHijo))
                    recorrer(hijo, profundidad: profundidad + 1, ruta: rutaHijo)
                }
            }

            recorrer(root, profundidad: 0, ruta: "")
            return ResumenNavegacionPDFIPad(
                totalPaginas: documento.pageCount,
                outline: salida,
                outlineTruncado: truncado)
        } ?? ResumenNavegacionPDFIPad(
            totalPaginas: 0, outline: [], outlineTruncado: false)
    }

    static func enlaces(_ url: URL) -> ResultadoEnlacesPDFIPad {
        conDocumento(url) { documento in
            var salida: [EnlacePDFIPad] = []
            var truncado = false
            guard documento.pageCount > 0 else {
                return ResultadoEnlacesPDFIPad(enlaces: [], truncado: false)
            }

            paginaLoop: for paginaIndice in 0..<documento.pageCount {
                guard let pagina = documento.page(at: paginaIndice) else { continue }
                for (indice, anotacion) in pagina.annotations.enumerated() {
                    guard salida.count < maximoEnlaces else {
                        truncado = true
                        break paginaLoop
                    }
                    if let urlDestino = anotacion.url,
                       esquemaExternoPermitido(urlDestino) {
                        salida.append(.init(
                            paginaOrigen: paginaIndice,
                            indiceEnPagina: indice,
                            rect: anotacion.bounds,
                            etiqueta: etiqueta(anotacion, fallback: urlDestino.absoluteString),
                            destino: .url(urlDestino)))
                        continue
                    }
                    guard let destino = anotacion.destination,
                          let paginaDestino = destino.page else { continue }
                    let destinoIndice = documento.index(for: paginaDestino)
                    guard destinoIndice != NSNotFound else { continue }
                    salida.append(.init(
                        paginaOrigen: paginaIndice,
                        indiceEnPagina: indice,
                        rect: anotacion.bounds,
                        etiqueta: etiqueta(
                            anotacion,
                            fallback: "Página \(destinoIndice + 1)"),
                        destino: .pagina(destinoIndice, destino.point)))
                }
            }
            return ResultadoEnlacesPDFIPad(enlaces: salida, truncado: truncado)
        } ?? ResultadoEnlacesPDFIPad(enlaces: [], truncado: false)
    }

    private static func etiqueta(_ anotacion: PDFAnnotation, fallback: String) -> String {
        let texto = anotacion.contents?.trimmingCharacters(in: .whitespacesAndNewlines)
        return texto?.isEmpty == false ? texto! : fallback
    }

    static func esquemaExternoPermitido(_ url: URL) -> Bool {
        guard let esquema = url.scheme?.lowercased() else { return false }
        return ["https", "http", "mailto"].contains(esquema)
    }

    private static func conDocumento<T>(_ url: URL, _ cuerpo: (PDFDocument) -> T) -> T? {
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        guard let documento = PDFDocument(url: url) else { return nil }
        return cuerpo(documento)
    }
}

final class ControladorPDFNavegableIPad: UIViewController,
                                        UITableViewDataSource,
                                        UITableViewDelegate,
                                        UISearchBarDelegate {
    private enum Seccion: Int { case outline, enlaces }

    private let url: URL
    private let paginaInicial: Int
    private let pdfView = PDFView()
    private let tabla = UITableView(frame: .zero, style: .plain)
    private let busqueda = UISearchBar()
    private let selector = UISegmentedControl(items: ["Índice", "Enlaces"])
    private let estado = UILabel()
    private let panelLateral = UIView()
    private var seccion: Seccion = .outline
    private var resumen = ResumenNavegacionPDFIPad(
        totalPaginas: 0, outline: [], outlineTruncado: false)
    private var enlaces = ResultadoEnlacesPDFIPad(enlaces: [], truncado: false)
    private var enlacesCargados = false
    private var filtro = ""
    private var generacionCarga: UInt64 = 0
    private let bloqueo = NSLock()
    private let cola = DispatchQueue(label: "cuadernu.pdf.navegacion", qos: .userInitiated)

    init(url: URL, pagina: Int = 0) {
        self.url = url
        paginaInicial = max(pagina, 0)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = url.lastPathComponent
        view.backgroundColor = .systemBackground
        configurarUI()
        cargarDocumento()
    }

    private func configurarUI() {
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Página", style: .plain,
                            target: self, action: #selector(pedirPagina)),
            UIBarButtonItem(title: "Compartir", style: .plain,
                            target: self, action: #selector(compartir)),
        ]

        selector.selectedSegmentIndex = 0
        selector.addTarget(self, action: #selector(cambiarSeccion), for: .valueChanged)
        selector.translatesAutoresizingMaskIntoConstraints = false
        busqueda.placeholder = "Buscar en índice"
        busqueda.delegate = self
        busqueda.translatesAutoresizingMaskIntoConstraints = false
        tabla.dataSource = self
        tabla.delegate = self
        tabla.rowHeight = UITableView.automaticDimension
        tabla.estimatedRowHeight = 52
        tabla.translatesAutoresizingMaskIntoConstraints = false
        estado.font = .preferredFont(forTextStyle: .footnote)
        estado.textColor = .secondaryLabel
        estado.numberOfLines = 0
        estado.translatesAutoresizingMaskIntoConstraints = false

        panelLateral.backgroundColor = .secondarySystemBackground
        panelLateral.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(panelLateral)
        [selector, busqueda, tabla, estado].forEach(panelLateral.addSubview)

        pdfView.autoScales = true
        pdfView.displayMode = .singlePageContinuous
        pdfView.displayDirection = .vertical
        pdfView.displaysPageBreaks = true
        pdfView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pdfView)

        NSLayoutConstraint.activate([
            panelLateral.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            panelLateral.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            panelLateral.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            panelLateral.widthAnchor.constraint(equalToConstant: 310),

            selector.topAnchor.constraint(equalTo: panelLateral.topAnchor, constant: 10),
            selector.leadingAnchor.constraint(equalTo: panelLateral.leadingAnchor, constant: 10),
            selector.trailingAnchor.constraint(equalTo: panelLateral.trailingAnchor, constant: -10),
            busqueda.topAnchor.constraint(equalTo: selector.bottomAnchor, constant: 6),
            busqueda.leadingAnchor.constraint(equalTo: panelLateral.leadingAnchor),
            busqueda.trailingAnchor.constraint(equalTo: panelLateral.trailingAnchor),
            estado.leadingAnchor.constraint(equalTo: panelLateral.leadingAnchor, constant: 12),
            estado.trailingAnchor.constraint(equalTo: panelLateral.trailingAnchor, constant: -12),
            estado.bottomAnchor.constraint(equalTo: panelLateral.safeAreaLayoutGuide.bottomAnchor,
                                            constant: -8),
            tabla.topAnchor.constraint(equalTo: busqueda.bottomAnchor),
            tabla.leadingAnchor.constraint(equalTo: panelLateral.leadingAnchor),
            tabla.trailingAnchor.constraint(equalTo: panelLateral.trailingAnchor),
            tabla.bottomAnchor.constraint(equalTo: estado.topAnchor, constant: -6),

            pdfView.leadingAnchor.constraint(equalTo: panelLateral.trailingAnchor),
            pdfView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            pdfView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            pdfView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])
    }

    private func cargarDocumento() {
        let generacion = siguienteGeneracion()
        estado.text = "Abriendo PDF e índice…"
        let url = self.url
        cola.async { [weak self] in
            let resumen = AnalizadorNavegacionPDFIPad.resumen(url)
            let acceso = url.startAccessingSecurityScopedResource()
            let documento = PDFDocument(url: url)
            if acceso { url.stopAccessingSecurityScopedResource() }
            DispatchQueue.main.async {
                guard let self, self.generacionActual() == generacion else { return }
                self.resumen = resumen
                self.pdfView.document = documento
                self.pdfView.autoScales = true
                self.irAPagina(self.paginaInicial, punto: nil)
                self.actualizarEstado()
                self.tabla.reloadData()
            }
        }
    }

    @objc private func cambiarSeccion() {
        seccion = Seccion(rawValue: selector.selectedSegmentIndex) ?? .outline
        busqueda.placeholder = seccion == .outline ? "Buscar en índice" : "Buscar enlaces"
        if seccion == .enlaces && !enlacesCargados {
            cargarEnlaces()
        } else {
            actualizarEstado(); tabla.reloadData()
        }
    }

    private func cargarEnlaces() {
        estado.text = "Analizando enlaces sin bloquear el PDF…"
        let generacion = generacionActual()
        let url = self.url
        cola.async { [weak self] in
            let resultado = AnalizadorNavegacionPDFIPad.enlaces(url)
            DispatchQueue.main.async {
                guard let self, self.generacionActual() == generacion else { return }
                self.enlaces = resultado
                self.enlacesCargados = true
                self.actualizarEstado()
                self.tabla.reloadData()
            }
        }
    }

    private func outlineFiltrado() -> [EntradaOutlinePDFIPad] {
        guard !filtro.isEmpty else { return resumen.outline }
        return resumen.outline.filter {
            $0.titulo.localizedCaseInsensitiveContains(filtro)
                || $0.pagina.map { String($0 + 1).contains(filtro) } == true
        }
    }

    private func enlacesFiltrados() -> [EnlacePDFIPad] {
        guard !filtro.isEmpty else { return enlaces.enlaces }
        return enlaces.enlaces.filter {
            $0.etiqueta.localizedCaseInsensitiveContains(filtro)
                || String($0.paginaOrigen + 1).contains(filtro)
        }
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        seccion == .outline ? outlineFiltrado().count : enlacesFiltrados().count
    }

    func tableView(_ tableView: UITableView,
                   cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let identificador = "pdf-nav"
        let celda = tableView.dequeueReusableCell(withIdentifier: identificador)
            ?? UITableViewCell(style: .subtitle, reuseIdentifier: identificador)
        celda.accessoryType = .disclosureIndicator
        celda.textLabel?.numberOfLines = 2
        if seccion == .outline {
            let item = outlineFiltrado()[indexPath.row]
            celda.indentationLevel = min(item.profundidad, 8)
            celda.textLabel?.text = item.titulo
            celda.detailTextLabel?.text = item.pagina.map { "Página \($0 + 1)" }
                ?? "Sin destino directo"
            celda.imageView?.image = UIImage(systemName: "list.bullet.indent")
        } else {
            let item = enlacesFiltrados()[indexPath.row]
            celda.indentationLevel = 0
            celda.textLabel?.text = item.etiqueta
            switch item.destino {
            case .pagina(let destino, _):
                celda.detailTextLabel?.text = "p. \(item.paginaOrigen + 1) → p. \(destino + 1)"
                celda.imageView?.image = UIImage(systemName: "arrow.turn.down.right")
            case .url(let url):
                celda.detailTextLabel?.text = "p. \(item.paginaOrigen + 1) · \(url.host ?? url.scheme ?? "enlace")"
                celda.imageView?.image = UIImage(systemName: "link")
            }
        }
        return celda
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if seccion == .outline {
            let item = outlineFiltrado()[indexPath.row]
            guard let pagina = item.pagina else { return }
            irAPagina(pagina, punto: item.punto)
            return
        }
        let item = enlacesFiltrados()[indexPath.row]
        switch item.destino {
        case .pagina(let pagina, let punto):
            irAPagina(pagina, punto: punto)
        case .url(let url):
            guard AnalizadorNavegacionPDFIPad.esquemaExternoPermitido(url) else { return }
            UIApplication.shared.open(url)
        }
    }

    func searchBar(_ searchBar: UISearchBar, textDidChange searchText: String) {
        filtro = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        tabla.reloadData()
    }

    private func irAPagina(_ indice: Int, punto: CGPoint?) {
        guard let documento = pdfView.document,
              documento.pageCount > 0 else { return }
        let seguro = min(max(indice, 0), documento.pageCount - 1)
        guard let pagina = documento.page(at: seguro) else { return }
        if let punto {
            pdfView.go(to: PDFDestination(page: pagina, at: punto))
        } else {
            pdfView.go(to: pagina)
        }
        navigationItem.prompt = "Página \(seguro + 1) de \(documento.pageCount)"
    }

    @objc private func pedirPagina() {
        let alerta = UIAlertController(
            title: "Ir a página",
            message: resumen.totalPaginas > 0 ? "1–\(resumen.totalPaginas)" : nil,
            preferredStyle: .alert)
        alerta.addTextField {
            $0.keyboardType = .numberPad
            $0.placeholder = "Número de página"
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Ir", style: .default) { [weak self] _ in
            guard let self,
                  let texto = alerta.textFields?.first?.text,
                  let pagina = Int(texto), pagina > 0 else { return }
            self.irAPagina(pagina - 1, punto: nil)
        })
        present(alerta, animated: true)
    }

    @objc private func compartir() {
        let actividad = UIActivityViewController(activityItems: [url], applicationActivities: nil)
        actividad.popoverPresentationController?.barButtonItem = navigationItem.rightBarButtonItems?.last
        present(actividad, animated: true)
    }

    private func actualizarEstado() {
        switch seccion {
        case .outline:
            if resumen.outline.isEmpty {
                estado.text = "\(resumen.totalPaginas) páginas · este PDF no contiene outline."
            } else {
                estado.text = "\(resumen.totalPaginas) páginas · \(resumen.outline.count) entradas"
                    + (resumen.outlineTruncado ? " · índice truncado por seguridad" : "")
            }
        case .enlaces:
            if !enlacesCargados {
                estado.text = "Enlaces aún no analizados."
            } else {
                estado.text = "\(enlaces.enlaces.count) enlaces"
                    + (enlaces.truncado ? " · análisis truncado por seguridad" : "")
            }
        }
    }

    private func siguienteGeneracion() -> UInt64 {
        bloqueo.lock(); defer { bloqueo.unlock() }
        generacionCarga &+= 1
        return generacionCarga
    }

    private func generacionActual() -> UInt64 {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return generacionCarga
    }
}
