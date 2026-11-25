import PDFKit
import UIKit
import Vision

enum OrientacionPrintoutIPad: String, CaseIterable {
    case vertical = "Vertical"
    case horizontal = "Horizontal"
}

enum PresetPaginasPrintoutIPad: String, CaseIterable {
    case todas
    case primeras5
    case primeras10
    case impares
    case pares
    case rango

    var nombre: String {
        switch self {
        case .todas: return "Todas"
        case .primeras5: return "1–5"
        case .primeras10: return "1–10"
        case .impares: return "Impares"
        case .pares: return "Pares"
        case .rango: return "Rango personalizado"
        }
    }
}

enum EspacioNotasPrintoutIPad: Int, CaseIterable {
    case ninguno = 0
    case breve = 180
    case normal = 320
    case amplio = 520

    var nombre: String {
        switch self {
        case .ninguno: return "Sin"
        case .breve: return "Breve"
        case .normal: return "Normal"
        case .amplio: return "Amplio"
        }
    }

    var puntos: CGFloat { CGFloat(rawValue) }
}

/// Selector puro y testeable. Los números que ve el usuario son 1-based;
/// `IndexSet` conserva los índices 0-based que consume PDFKit.
enum SelectorPaginasPrintoutIPad {
    static func seleccionar(_ preset: PresetPaginasPrintoutIPad,
                            total: Int,
                            rangoPersonalizado: String = "") -> IndexSet {
        guard total > 0 else { return [] }
        switch preset {
        case .todas:
            return IndexSet(integersIn: 0..<total)
        case .primeras5:
            return IndexSet(integersIn: 0..<min(5, total))
        case .primeras10:
            return IndexSet(integersIn: 0..<min(10, total))
        case .impares:
            return IndexSet(stride(from: 0, to: total, by: 2))
        case .pares:
            return IndexSet(stride(from: 1, to: total, by: 2))
        case .rango:
            return parsear(rangoPersonalizado, total: total)
        }
    }

    static func parsear(_ texto: String, total: Int) -> IndexSet {
        guard total > 0 else { return [] }
        let normalizado = texto
            .replacingOccurrences(of: "–", with: "-")
            .replacingOccurrences(of: "—", with: "-")
            .replacingOccurrences(of: ";", with: ",")
            .replacingOccurrences(of: "\n", with: ",")
        var salida = IndexSet()
        for fragmento in normalizado.split(separator: ",") {
            let limites = fragmento.split(separator: "-").compactMap {
                Int($0.trimmingCharacters(in: .whitespacesAndNewlines))
            }
            if limites.count == 1, (1...total).contains(limites[0]) {
                salida.insert(limites[0] - 1)
            } else if limites.count == 2 {
                let a = max(min(limites[0], limites[1]), 1)
                let b = min(max(limites[0], limites[1]), total)
                if a <= b { salida.insert(integersIn: (a - 1)..<b) }
            }
        }
        return salida
    }
}

struct OpcionesPrintoutPDFIPad {
    var paginas: IndexSet
    var porHoja = 1
    var escala: CGFloat = 1
    var espacio: CGFloat = 48
    var margenSuperior: CGFloat = 36
    var margenLateral: CGFloat = 36
    var espacioNotas: CGFloat = 320
    var notasALaDerecha = true
    var orientacion: OrientacionPrintoutIPad = .vertical
}

struct ColocacionPaginaPDFIPad: Identifiable {
    var id = UUID()
    var pagina: Int
    var marco: CGRect
    var hojaLogica: Int
}

enum PlanificadorPrintoutPDFIPad {
    static func plan(documento: PDFDocument, origen: CGPoint,
                     opciones: OpcionesPrintoutPDFIPad) -> [ColocacionPaginaPDFIPad] {
        let paginas = opciones.paginas.filter { $0 >= 0 && $0 < documento.pageCount }
        guard !paginas.isEmpty else { return [] }
        let porHoja = [1, 2, 4].contains(opciones.porHoja) ? opciones.porHoja : 1
        var salida: [ColocacionPaginaPDFIPad] = []
        var y = origen.y + opciones.margenSuperior
        var hoja = 0
        for grupo in stride(from: 0, to: paginas.count, by: porHoja) {
            let lote = paginas[grupo..<min(grupo + porHoja, paginas.count)]
            var x = origen.x + opciones.margenLateral
            var altoFila: CGFloat = 0
            for paginaIndice in lote {
                guard let pagina = documento.page(at: paginaIndice) else { continue }
                var tamano = pagina.bounds(for: .mediaBox).size
                if pagina.rotation == 90 || pagina.rotation == 270 {
                    tamano = CGSize(width: tamano.height, height: tamano.width)
                }
                let base: CGFloat = porHoja == 1 ? 760 : (porHoja == 2 ? 520 : 360)
                let factor = base / max(tamano.width, 1) * min(max(opciones.escala, 0.2), 4)
                var marcoTamano = CGSize(width: tamano.width * factor,
                                         height: tamano.height * factor)
                if opciones.orientacion == .horizontal {
                    marcoTamano = CGSize(width: marcoTamano.height,
                                         height: marcoTamano.width)
                }
                let marco = CGRect(origin: CGPoint(x: x, y: y), size: marcoTamano)
                salida.append(ColocacionPaginaPDFIPad(
                    pagina: paginaIndice, marco: marco, hojaLogica: hoja))
                x += marcoTamano.width + opciones.espacio +
                    (opciones.notasALaDerecha ? opciones.espacioNotas : 0)
                altoFila = max(altoFila, marcoTamano.height +
                               (opciones.notasALaDerecha ? 0 : opciones.espacioNotas))
            }
            y += altoFila + opciones.espacio
            hoja += 1
        }
        return salida
    }
}

enum ErrorEdicionPDFIPad: LocalizedError {
    case noAbre
    case paginasInvalidas
    case noEscribe
    var errorDescription: String? {
        switch self {
        case .noAbre: return "No se pudo abrir el PDF."
        case .paginasInvalidas: return "La selección de páginas no es válida."
        case .noEscribe: return "No se pudo guardar el PDF resultante."
        }
    }
}

/// Operaciones PDF locales y explícitas. Siempre escriben un archivo nuevo;
/// nunca reemplazan el original importado.
enum EditorPDFLocalIPad {
    static func extraer(_ origen: URL, paginas: IndexSet,
                        nombre: String = "paginas-extraidas") throws -> URL {
        guard let documento = PDFDocument(url: origen) else { throw ErrorEdicionPDFIPad.noAbre }
        let salida = PDFDocument()
        for indice in paginas.sorted() where indice >= 0 && indice < documento.pageCount {
            if let pagina = documento.page(at: indice) { salida.insert(pagina, at: salida.pageCount) }
        }
        guard salida.pageCount > 0 else { throw ErrorEdicionPDFIPad.paginasInvalidas }
        return try guardar(salida, nombre: nombre)
    }

    static func fusionar(_ archivos: [URL], nombre: String = "pdf-fusionado") throws -> URL {
        let salida = PDFDocument()
        for archivo in archivos {
            guard let documento = PDFDocument(url: archivo) else { continue }
            for indice in 0..<documento.pageCount {
                if let pagina = documento.page(at: indice) { salida.insert(pagina, at: salida.pageCount) }
            }
        }
        guard salida.pageCount > 0 else { throw ErrorEdicionPDFIPad.noAbre }
        return try guardar(salida, nombre: nombre)
    }

    static func dividir(_ origen: URL, cada cantidad: Int) throws -> [URL] {
        guard let documento = PDFDocument(url: origen) else { throw ErrorEdicionPDFIPad.noAbre }
        let bloque = max(cantidad, 1)
        return try stride(from: 0, to: documento.pageCount, by: bloque).map { inicio in
            try extraer(origen, paginas: IndexSet(integersIn: inicio..<min(inicio + bloque,
                                                                           documento.pageCount)),
                        nombre: "\(origen.deletingPathExtension().lastPathComponent)-\(inicio + 1)")
        }
    }

    static func rotar(_ origen: URL, paginas: IndexSet, grados: Int) throws -> URL {
        guard let documento = PDFDocument(url: origen) else { throw ErrorEdicionPDFIPad.noAbre }
        for indice in paginas where indice >= 0 && indice < documento.pageCount {
            documento.page(at: indice)?.rotation =
                ((documento.page(at: indice)?.rotation ?? 0) + grados) % 360
        }
        return try guardar(documento, nombre: origen.deletingPathExtension()
            .lastPathComponent + "-rotado")
    }

    static func recortarMargenes(_ origen: URL, paginas: IndexSet,
                                 margenes: UIEdgeInsets) throws -> URL {
        guard let documento = PDFDocument(url: origen) else { throw ErrorEdicionPDFIPad.noAbre }
        for indice in paginas where indice >= 0 && indice < documento.pageCount {
            guard let pagina = documento.page(at: indice) else { continue }
            let caja = pagina.bounds(for: .mediaBox)
            let nueva = CGRect(x: caja.minX + margenes.left,
                               y: caja.minY + margenes.bottom,
                               width: caja.width - margenes.left - margenes.right,
                               height: caja.height - margenes.top - margenes.bottom)
            if nueva.width > 20, nueva.height > 20 { pagina.setBounds(nueva, for: .cropBox) }
        }
        return try guardar(documento, nombre: origen.deletingPathExtension()
            .lastPathComponent + "-recortado")
    }

    private static func guardar(_ documento: PDFDocument, nombre: String) throws -> URL {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Tinta PDFs", isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        let limpio = nombre.replacingOccurrences(of: "/", with: "-")
        let url = carpeta.appendingPathComponent("\(limpio)-\(UUID().uuidString.prefix(8)).pdf")
        guard documento.write(to: url) else { throw ErrorEdicionPDFIPad.noEscribe }
        return url
    }
}

final class PanelImportacionPDFAvanzadaIPad: UIViewController {
    private let url: URL
    private let documento: PDFDocument
    private let paginas = UITextField()
    private let preset = UIButton(type: .system)
    private var presetActual: PresetPaginasPrintoutIPad = .todas
    private let porHoja = UISegmentedControl(items: ["1", "2", "4"])
    private let orientacion = UISegmentedControl(
        items: OrientacionPrintoutIPad.allCases.map(\.rawValue))
    private let escala = UISlider()
    private let espacio = UISlider()
    private let espacioNotas = UISegmentedControl(
        items: EspacioNotasPrintoutIPad.allCases.map(\.nombre))
    private let lateral = UISwitch()
    private let resumen = UILabel()
    var alImportar: ((URL, OpcionesPrintoutPDFIPad) -> Void)?

    init?(url: URL) {
        guard let documento = PDFDocument(url: url) else { return nil }
        self.url = url; self.documento = documento
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Importar PDF como printout"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Insertar", style: .done, target: self, action: #selector(insertar))

        paginas.placeholder = "Ej.: 1-5, 8, 12-14"
        paginas.text = "1-\(documento.pageCount)"
        paginas.borderStyle = .roundedRect
        paginas.isHidden = true
        paginas.addTarget(self, action: #selector(actualizarResumen), for: .editingChanged)

        configurarMenuPreset()
        porHoja.selectedSegmentIndex = 0
        orientacion.selectedSegmentIndex = 0
        escala.minimumValue = 0.2; escala.maximumValue = 2; escala.value = 1
        espacio.minimumValue = 0; espacio.maximumValue = 240; espacio.value = 48
        espacioNotas.selectedSegmentIndex = 2
        lateral.isOn = true

        for control in [porHoja, orientacion, espacioNotas] {
            control.addTarget(self, action: #selector(actualizarResumen), for: .valueChanged)
        }
        escala.addTarget(self, action: #selector(actualizarResumen), for: .valueChanged)
        espacio.addTarget(self, action: #selector(actualizarResumen), for: .valueChanged)
        lateral.addTarget(self, action: #selector(actualizarResumen), for: .valueChanged)

        resumen.numberOfLines = 0
        resumen.font = .preferredFont(forTextStyle: .footnote)
        resumen.textColor = .secondaryLabel
        resumen.accessibilityIdentifier = "pdf.printout.resumen"

        let pila = UIStackView(arrangedSubviews: [
            etiqueta("Qué páginas importar"), preset, paginas,
            etiqueta("Páginas por grupo"), porHoja,
            etiqueta("Orientación del printout"), orientacion,
            etiqueta("Escala"), escala,
            etiqueta("Separación entre páginas"), espacio,
            etiqueta("Espacio para resolver / anotar"), espacioNotas,
            fila("Colocar espacio de notas a la derecha", lateral),
            resumen,
            nota("Cada página sigue vinculada al PDF y a su número original. Puede dejar un área amplia en blanco junto a cada página para resolver ejercicios con lápiz del iPad sin escribir encima del enunciado."),
        ])
        pila.axis = .vertical; pila.spacing = 14; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 22),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -22),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 22),
            pila.bottomAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.bottomAnchor,
                                         constant: -18),
        ])
        actualizarResumen()
    }

    private func configurarMenuPreset() {
        preset.configuration = .bordered()
        preset.showsMenuAsPrimaryAction = true
        preset.menu = UIMenu(children: PresetPaginasPrintoutIPad.allCases.map { valor in
            UIAction(title: valor.nombre,
                     state: valor == presetActual ? .on : .off) { [weak self] _ in
                guard let self else { return }
                self.presetActual = valor
                self.paginas.isHidden = valor != .rango
                self.configurarMenuPreset()
                self.actualizarResumen()
            }
        })
        preset.configuration?.title = presetActual.nombre
    }

    private func seleccionActual() -> IndexSet {
        SelectorPaginasPrintoutIPad.seleccionar(
            presetActual, total: documento.pageCount,
            rangoPersonalizado: paginas.text ?? "")
    }

    private func espacioNotasActual() -> EspacioNotasPrintoutIPad {
        let indice = max(espacioNotas.selectedSegmentIndex, 0)
        return EspacioNotasPrintoutIPad.allCases[
            min(indice, EspacioNotasPrintoutIPad.allCases.count - 1)]
    }

    @objc private func actualizarResumen() {
        let seleccion = seleccionActual()
        let valores = [1, 2, 4]
        let indicePorHoja = min(max(porHoja.selectedSegmentIndex, 0), valores.count - 1)
        let cantidadPorGrupo = valores[indicePorHoja]
        let grupos = seleccion.isEmpty ? 0
            : Int(ceil(Double(seleccion.count) / Double(cantidadPorGrupo)))
        let notas = espacioNotasActual()
        let orientacionActual = OrientacionPrintoutIPad.allCases[
            min(max(orientacion.selectedSegmentIndex, 0),
                OrientacionPrintoutIPad.allCases.count - 1)]
        let detalleNotas = notas == .ninguno
            ? "sin espacio extra para notas"
            : "\(Int(notas.puntos)) pt para notas \(lateral.isOn ? "a la derecha" : "debajo")"
        resumen.text = "\(seleccion.count) página(s) · \(cantidadPorGrupo) por grupo · \(grupos) grupo(s) · \(orientacionActual.rawValue.lowercased()) · \(detalleNotas)"
        navigationItem.rightBarButtonItem?.isEnabled = !seleccion.isEmpty
        lateral.isEnabled = notas != .ninguno
    }

    private func etiqueta(_ texto: String) -> UILabel {
        let l = UILabel(); l.text = texto; l.font = .preferredFont(forTextStyle: .headline); return l
    }
    private func nota(_ texto: String) -> UILabel {
        let l = UILabel(); l.text = texto; l.numberOfLines = 0
        l.font = .preferredFont(forTextStyle: .footnote); l.textColor = .secondaryLabel; return l
    }
    private func fila(_ texto: String, _ control: UIView) -> UIStackView {
        let l = UILabel(); l.text = texto
        let f = UIStackView(arrangedSubviews: [l, UIView(), control]); f.axis = .horizontal
        return f
    }

    @objc private func insertar() {
        let seleccion = seleccionActual()
        guard !seleccion.isEmpty else { return }
        let valores = [1, 2, 4]
        let indicePorHoja = min(max(porHoja.selectedSegmentIndex, 0), valores.count - 1)
        let indiceOrientacion = min(max(orientacion.selectedSegmentIndex, 0),
                                    OrientacionPrintoutIPad.allCases.count - 1)
        let notas = espacioNotasActual()
        let opciones = OpcionesPrintoutPDFIPad(
            paginas: seleccion,
            porHoja: valores[indicePorHoja],
            escala: CGFloat(escala.value),
            espacio: CGFloat(espacio.value),
            espacioNotas: notas.puntos,
            notasALaDerecha: lateral.isOn,
            orientacion: OrientacionPrintoutIPad.allCases[indiceOrientacion])
        alImportar?(url, opciones); dismiss(animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

final class ControladorClipPDFIPad: UIViewController {
    private let medio: ElementoMedio
    private let sesion: Sesion
    private let imagen = UIImageView()
    private let seleccion = VistaSeleccionClipPDFIPad()
    private let selectorForma = UISegmentedControl(
        items: FormaClipPDFIPad.allCases.map(\.nombre))
    private var miniatura: UIImage?
    var alCrear: ((String) -> Void)?

    init(medio: ElementoMedio, sesion: Sesion) {
        self.medio = medio; self.sesion = sesion; super.init(nibName:nil,bundle:nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Recorte PDF con origen"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Crear clip", style: .done, target: self, action: #selector(crear))
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash),
              let documento = PDFDocument(url: url),
              let pagina = documento.page(at: medio.paginaFuente) else { return }
        miniatura = pagina.thumbnail(of: CGSize(width: 1800, height: 2200), for: .mediaBox)
        imagen.image = miniatura
        imagen.contentMode = .scaleAspectFit; imagen.translatesAutoresizingMaskIntoConstraints = false
        selectorForma.selectedSegmentIndex = 0
        selectorForma.addTarget(self, action: #selector(cambiarForma), for: .valueChanged)
        selectorForma.translatesAutoresizingMaskIntoConstraints = false
        seleccion.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(selectorForma); view.addSubview(imagen); view.addSubview(seleccion)
        NSLayoutConstraint.activate([
            selectorForma.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                                    constant: 12),
            selectorForma.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                                     constant: -12),
            selectorForma.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                                constant: 8),
            imagen.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            imagen.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            imagen.topAnchor.constraint(equalTo: selectorForma.bottomAnchor, constant: 8),
            imagen.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            seleccion.leadingAnchor.constraint(equalTo: imagen.leadingAnchor),
            seleccion.trailingAnchor.constraint(equalTo: imagen.trailingAnchor),
            seleccion.topAnchor.constraint(equalTo: imagen.topAnchor),
            seleccion.bottomAnchor.constraint(equalTo: imagen.bottomAnchor),
        ])
        seleccion.configurar(.rectangular, imagen: miniatura)
    }
    @objc private func cambiarForma() {
        guard FormaClipPDFIPad.allCases.indices.contains(
            selectorForma.selectedSegmentIndex) else { return }
        seleccion.configurar(
            FormaClipPDFIPad.allCases[selectorForma.selectedSegmentIndex],
            imagen: miniatura)
    }
    @objc private func crear() {
        let region = seleccion.regionNormalizada
        let id = UUID().uuidString
        guard sesion.anadirMedio(hash: medio.hash, nombre: medio.nombre,
                                 tipo: .pdf, marco: medio.marco.offsetBy(dx: 36, dy: 36),
                                 id: id) != nil else { return }
        guard sesion.configurarFuenteMedio(id: id, pagina: medio.paginaFuente),
              sesion.recortarMedio(id: id, recorteRelativo: region) else {
            _ = sesion.eliminarMedio(id: id)
            return
        }
        let clip = ClipConFuenteIPad(
            elementoId: id, hashFuente: medio.hash, pagina: medio.paginaFuente,
            regionNormalizada: region, forma: seleccion.forma,
            puntosNormalizados: seleccion.puntosParaPersistir)
        guard RepositorioClipsPDFDocumentoIPad.compartido.registrar(
            clip, documento: sesion.documentoPersistenteId) else {
            _ = sesion.eliminarMedio(id: id)
            return
        }
        alCrear?(id); dismiss(animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

private final class VistaSeleccionClipPDFIPad: UIView {
    private var inicio = CGPoint.zero
    private var actual = CGRect(x: 0.15, y: 0.15, width: 0.7, height: 0.45)
    private var puntos: [CGPoint] = []
    private var candidatos: [CGRect] = []
    private(set) var forma: FormaClipPDFIPad = .rectangular
    var regionNormalizada: CGRect { actual }
    var puntosParaPersistir: [CGPoint]? {
        (forma == .libre || forma == .poligonal) && puntos.count >= 3 ? puntos : nil
    }

    func configurar(_ forma: FormaClipPDFIPad, imagen: UIImage?) {
        self.forma = forma; puntos.removeAll(); candidatos.removeAll()
        if forma == .rectangular {
            actual = CGRect(x: 0.15, y: 0.15, width: 0.7, height: 0.45)
            setNeedsDisplay(); return
        }
        if forma == .libre || forma == .poligonal {
            actual = .zero; setNeedsDisplay(); return
        }
        guard let cg = imagen?.cgImage else { return }
        AnalizadorClipSemanticoPDFIPad.detectar(cg, forma: forma) { [weak self] regiones in
            DispatchQueue.main.async {
                self?.candidatos = regiones
                if let primera = regiones.first { self?.actual = primera }
                self?.setNeedsDisplay()
            }
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        inicio = touches.first?.location(in: self) ?? .zero
        if forma == .libre || forma == .poligonal {
            puntos = [normalizar(inicio)]; actual = .zero
        } else if forma != .rectangular {
            elegirCandidato(en: normalizar(inicio))
        }
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let fin = touches.first?.location(in: self), bounds.width > 0,
              bounds.height > 0 else { return }
        if forma == .libre || forma == .poligonal {
            puntos.append(normalizar(fin)); actualizarRegionDePuntos(); setNeedsDisplay(); return
        }
        guard forma == .rectangular else { return }
        let r = CGRect(x: min(inicio.x, fin.x) / bounds.width,
                       y: min(inicio.y, fin.y) / bounds.height,
                       width: abs(inicio.x - fin.x) / bounds.width,
                       height: abs(inicio.y - fin.y) / bounds.height)
        actual = r.intersection(CGRect(x: 0, y: 0, width: 1, height: 1)); setNeedsDisplay()
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard forma == .libre || forma == .poligonal,
              let fin = touches.first?.location(in: self) else { return }
        puntos.append(normalizar(fin)); actualizarRegionDePuntos(); setNeedsDisplay()
    }

    private func normalizar(_ punto: CGPoint) -> CGPoint {
        CGPoint(x: min(max(punto.x / max(bounds.width, 1), 0), 1),
                y: min(max(punto.y / max(bounds.height, 1), 0), 1))
    }
    private func actualizarRegionDePuntos() {
        actual = puntos.reduce(CGRect.null) {
            $0.union(CGRect(x: $1.x, y: $1.y, width: 0.0001, height: 0.0001))
        }.intersection(CGRect(x: 0, y: 0, width: 1, height: 1))
    }
    private func elegirCandidato(en punto: CGPoint) {
        if let exacto = candidatos.first(where: { $0.insetBy(dx: -0.02, dy: -0.02)
            .contains(punto) }) {
            actual = exacto
        } else if let cercano = candidatos.min(by: {
            hypot($0.midX - punto.x, $0.midY - punto.y)
                < hypot($1.midX - punto.x, $1.midY - punto.y)
        }) { actual = cercano }
        setNeedsDisplay()
    }
    override func draw(_ rect: CGRect) {
        let mascara = UIBezierPath(rect: bounds)
        let visible = CGRect(x: actual.minX * bounds.width, y: actual.minY * bounds.height,
                             width: actual.width * bounds.width,
                             height: actual.height * bounds.height)
        let seleccion: UIBezierPath
        if (forma == .libre || forma == .poligonal), puntos.count >= 2 {
            seleccion = UIBezierPath()
            for (indice, punto) in puntos.enumerated() {
                let local = CGPoint(x: punto.x * bounds.width, y: punto.y * bounds.height)
                if indice == 0 { seleccion.move(to: local) }
                else { seleccion.addLine(to: local) }
            }
            if forma == .poligonal || puntos.count > 2 { seleccion.close() }
        } else { seleccion = UIBezierPath(rect: visible) }
        mascara.append(seleccion); mascara.usesEvenOddFillRule = true
        UIColor.black.withAlphaComponent(0.35).setFill(); mascara.fill()
        UIColor.systemBlue.setStroke(); seleccion.lineWidth = 3; seleccion.stroke()
        UIColor.systemTeal.withAlphaComponent(0.8).setStroke()
        for candidato in candidatos where candidato != actual {
            let r = CGRect(x: candidato.minX * bounds.width,
                           y: candidato.minY * bounds.height,
                           width: candidato.width * bounds.width,
                           height: candidato.height * bounds.height)
            let ruta = UIBezierPath(rect: r); ruta.lineWidth = 1; ruta.stroke()
        }
    }
}

private enum AnalizadorClipSemanticoPDFIPad {
    static func detectar(_ imagen: CGImage, forma: FormaClipPDFIPad,
                         completado: @escaping ([CGRect]) -> Void) {
        DispatchQueue.global(qos: .userInitiated).async {
            if forma == .parrafo || forma == .ecuacion || forma == .tabla {
                let solicitud = VNRecognizeTextRequest()
                solicitud.recognitionLevel = .accurate
                solicitud.recognitionLanguages = ["es-ES", "en-US"]
                solicitud.usesLanguageCorrection = true
                do { try VNImageRequestHandler(cgImage: imagen).perform([solicitud]) }
                catch { completado([]); return }
                var regiones = (solicitud.results ?? []).compactMap { observacion -> CGRect? in
                    let texto = observacion.topCandidates(1).first?.string ?? ""
                    if forma == .ecuacion,
                       texto.range(of: #"[=+−*/^∫∑√Ωµ]"#, options: .regularExpression) == nil {
                        return nil
                    }
                    let caja = observacion.boundingBox
                    return CGRect(x: caja.minX, y: 1 - caja.maxY,
                                  width: caja.width, height: caja.height)
                        .insetBy(dx: -0.012, dy: -0.012)
                        .intersection(CGRect(x: 0, y: 0, width: 1, height: 1))
                }
                if forma == .tabla, !regiones.isEmpty {
                    let union = regiones.reduce(CGRect.null) { $0.union($1) }
                    regiones.insert(union, at: 0)
                }
                completado(regiones.sorted { $0.width * $0.height > $1.width * $1.height })
                return
            }
            let solicitud = VNDetectRectanglesRequest()
            solicitud.maximumObservations = 24
            solicitud.minimumSize = 0.08
            solicitud.minimumConfidence = 0.35
            do { try VNImageRequestHandler(cgImage: imagen).perform([solicitud]) }
            catch { completado([]); return }
            let regiones = (solicitud.results ?? []).map { observacion in
                let caja = observacion.boundingBox
                return CGRect(x: caja.minX, y: 1 - caja.maxY,
                              width: caja.width, height: caja.height)
            }
            completado(regiones.isEmpty
                ? [CGRect(x: 0.12, y: 0.18, width: 0.76, height: 0.55)] : regiones)
        }
    }
}
