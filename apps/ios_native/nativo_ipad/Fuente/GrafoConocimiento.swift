import PhotosUI
import UIKit
import Vision

struct RelacionConocimiento:Identifiable,Equatable { var id=UUID();var origen:UUID;var destino:UUID;var peso:Double;var motivo:String }
struct ConceptoGrafo:Identifiable,Equatable { var id=UUID();var nombre:String;var nodos:[NodoConocimiento];var centro:CGPoint = .zero }
struct GrafoConocimientoLocal { var conceptos:[ConceptoGrafo];var relaciones:[RelacionConocimiento] }

enum ConstructorGrafoConocimiento {
    static func construir(_ nodos:[NodoConocimiento],cursoId:UUID?=nil)->GrafoConocimientoLocal {
        let filtrados=nodos.filter{cursoId==nil || $0.fuente.cursoId==cursoId};var grupos:[String:[NodoConocimiento]]=[:]
        for nodo in filtrados { let claves=nodo.etiquetas.isEmpty ? EtiquetadorLocal.etiquetas(nodo.texto):nodo.etiquetas;for clave in claves.prefix(5){grupos[Normalizador.texto(clave),default:[]].append(nodo)} }
        var conceptos=grupos.filter{$0.value.count>=1}.map{ConceptoGrafo(nombre:$0.key,nodos:Array(Dictionary(grouping:$0.value,by:{$0.id}).values.compactMap(\.first)))}.sorted{$0.nodos.count>$1.nodos.count};if conceptos.count>80{conceptos=Array(conceptos.prefix(80))}
        let columnas=max(Int(ceil(sqrt(Double(max(conceptos.count,1))))),1);for i in conceptos.indices{conceptos[i].centro=CGPoint(x:CGFloat(i%columnas)*180+100,y:CGFloat(i/columnas)*130+80)}
        var relaciones: [RelacionConocimiento] = []
        for i in conceptos.indices {
            for j in conceptos.indices where j > i {
                let a = Set(conceptos[i].nodos.map(\.id))
                let b = Set(conceptos[j].nodos.map(\.id))
                let comunes = a.intersection(b).count
                guard comunes > 0 else { continue }
                let peso = Double(comunes) / Double(max(min(a.count, b.count), 1))
                relaciones.append(RelacionConocimiento(origen: conceptos[i].id, destino: conceptos[j].id, peso: peso, motivo: "\(comunes) fuentes compartidas"))
            }
        }
        return GrafoConocimientoLocal(conceptos: conceptos, relaciones: relaciones)
    }
    static func backlinks(para nodo: NodoConocimiento, en nodos: [NodoConocimiento]) -> [(NodoConocimiento, String)] {
        let etiquetas = Set(nodo.etiquetas.map(Normalizador.texto))
        let palabras = Set(Normalizador.texto(nodo.texto).split(separator: " ").filter { $0.count > 5 }.map(String.init))
        return nodos.filter { $0.id != nodo.id }.compactMap { otro in
            let e = etiquetas.intersection(Set(otro.etiquetas.map(Normalizador.texto)))
            let p = palabras.intersection(Set(Normalizador.texto(otro.texto).split(separator: " ").map(String.init)))
            let puntaje = e.count * 3 + p.count
            return puntaje > 0 ? (otro, "\(e.count) etiquetas y \(p.count) términos compartidos") : nil
        }.sorted { $0.0.fecha > $1.0.fecha }
    }
}

struct EntradaWikiLocal {
    var titulo: String
    var resumen: String
    var fuentes: [ReferenciaFuente]
    var relacionados: [String]
}

enum GeneradorWikiLocal {
    static func entrada(concepto: ConceptoGrafo, grafo: GrafoConocimientoLocal) -> EntradaWikiLocal {
        let oraciones = concepto.nodos.flatMap { $0.texto.components(separatedBy: CharacterSet(charactersIn: ".!?\n")) }
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }.filter { $0.count > 20 }
        let resumen = Array(oraciones.prefix(6)).joined(separator: ". ")
        let relacionados = grafo.relaciones.filter { $0.origen == concepto.id || $0.destino == concepto.id }.compactMap { r in
            let id = r.origen == concepto.id ? r.destino : r.origen
            return grafo.conceptos.first { $0.id == id }?.nombre
        }
        return EntradaWikiLocal(titulo: concepto.nombre.capitalized, resumen: resumen.isEmpty ? "No hay texto suficiente; abra las fuentes originales." : resumen, fuentes: concepto.nodos.map(\.fuente), relacionados: relacionados)
    }
    static func formulario(_ nodos: [NodoConocimiento]) -> [(String, ReferenciaFuente)] {
        var salida: [(String, ReferenciaFuente)] = []
        let patron = #"[^\n]{1,80}=\s*[^\n]{1,120}"#
        let regex = try? NSRegularExpression(pattern: patron)
        for nodo in nodos {
            let texto = nodo.texto as NSString
            let rango = NSRange(location: 0, length: texto.length)
            for r in regex?.matches(in: nodo.texto, range: rango) ?? [] {
                let formula = texto.substring(with: r.range).trimmingCharacters(in: .whitespaces)
                if !salida.contains(where: { $0.0 == formula }) {
                    salida.append((formula, nodo.fuente))
                }
            }
        }
        return salida
    }
}

final class VistaMapaConceptos: UIView {
    var grafo = GrafoConocimientoLocal(conceptos: [], relaciones: []) {
        didSet { setNeedsDisplay() }
    }
    var alElegir: ((ConceptoGrafo) -> Void)?
    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .systemBackground
        addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(tocar(_:))))
    }
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    override func draw(_ rect: CGRect) {
        guard let c = UIGraphicsGetCurrentContext() else { return }
        c.setStrokeColor(UIColor.systemGray3.cgColor)
        for r in grafo.relaciones {
            guard let a = grafo.conceptos.first(where: { $0.id == r.origen }),
                  let b = grafo.conceptos.first(where: { $0.id == r.destino }) else { continue }
            c.setLineWidth(CGFloat(max(r.peso * 3, 0.5)))
            c.move(to: a.centro)
            c.addLine(to: b.centro)
        }
        c.strokePath()
        for concepto in grafo.conceptos {
            let radio = CGFloat(min(42 + concepto.nodos.count * 3, 68))
            let marco = CGRect(x: concepto.centro.x - radio, y: concepto.centro.y - radio / 2, width: radio * 2, height: radio)
            UIColor.systemIndigo.withAlphaComponent(0.16).setFill()
            UIColor.systemIndigo.setStroke()
            UIBezierPath(roundedRect: marco, cornerRadius: 18).fill()
            UIBezierPath(roundedRect: marco, cornerRadius: 18).stroke()
            (concepto.nombre.capitalized as NSString).draw(in: marco.insetBy(dx: 7, dy: 7), withAttributes: [.font: UIFont.systemFont(ofSize: 12, weight: .semibold), .foregroundColor: UIColor.label])
        }
    }
    @objc private func tocar(_ g: UITapGestureRecognizer) {
        let p = g.location(in: self)
        if let c = grafo.conceptos.min(by: { hypot($0.centro.x - p.x, $0.centro.y - p.y) < hypot($1.centro.x - p.x, $1.centro.y - p.y) }),
           hypot(c.centro.x - p.x, c.centro.y - p.y) < 80 {
            alElegir?(c)
        }
    }
}

struct CoincidenciaVisual { var medio: ElementoMedio; var distancia: Float }
enum BuscadorVisualLocal {
    static func buscar(imagen: UIImage, en medios: [ElementoMedio], completado: @escaping ([CoincidenciaVisual]) -> Void) {
        guard let consulta = feature(imagen) else { completado([]); return }
        DispatchQueue.global(qos: .userInitiated).async {
            var salida: [CoincidenciaVisual] = []
            for medio in medios where medio.tipo == .imagen {
                guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash),
                      let imagen = UIImage(contentsOfFile: url.path),
                      let candidata = feature(imagen) else { continue }
                var distancia = Float.greatestFiniteMagnitude
                try? consulta.computeDistance(&distancia, to: candidata)
                salida.append(CoincidenciaVisual(medio: medio, distancia: distancia))
            }
            completado(salida.sorted { $0.distancia < $1.distancia })
        }
    }
    private static func feature(_ imagen: UIImage) -> VNFeaturePrintObservation? {
        guard let cg = imagen.cgImage else { return nil }
        let solicitud = VNGenerateImageFeaturePrintRequest()
        let manejador = VNImageRequestHandler(cgImage: cg, orientation: .up)
        try? manejador.perform([solicitud])
        return solicitud.results?.first as? VNFeaturePrintObservation
    }
}

struct FavoritoConocimiento: Codable, Identifiable, Equatable { var id = UUID(); var titulo: String; var fuente: ReferenciaFuente; var creado = Date(); var vence: Date? }
final class RepositorioFavoritosConocimiento {
    static let compartido = RepositorioFavoritosConocimiento()
    private let url: URL
    private(set) var favoritos: [FavoritoConocimiento] = []
    private init() {
        let base = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        url = base.appendingPathComponent("Academico/favoritos.json")
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url), let f = try? d.decode([FavoritoConocimiento].self, from: datos) {
            favoritos = f
        }
        purgar()
    }
    func agregar(_ f:FavoritoConocimiento){favoritos.removeAll{$0.fuente==f.fuente};favoritos.insert(f,at:0);guardar()}
    func purgar(){favoritos.removeAll{($0.vence ?? .distantFuture)<Date()};guardar()}
    private func guardar(){let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;if let d=try? e.encode(favoritos){try? d.write(to:url,options:.atomic)}}
}

final class PanelConocimiento:UIViewController,UITableViewDataSource,UITableViewDelegate,PHPickerViewControllerDelegate {
    var alAbrirFuente:((ReferenciaFuente)->Void)?;private let sesion:Sesion;private let centro=CentroAcademico.compartido
    private let consulta = UISearchBar()
    private let tabla = UITableView(frame: .zero, style: .insetGrouped)
    private var resultados: [ResultadoConFuente] = []
    init(sesion: Sesion) {
        self.sesion = sesion
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Conocimiento del cuaderno"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Mapa", style: .plain, target: self, action: #selector(mapa)),
            UIBarButtonItem(title: "Formulario", style: .plain, target: self, action: #selector(formulario)),
            UIBarButtonItem(title: "Visual", style: .plain, target: self, action: #selector(visual))
        ]
        consulta.placeholder = "Texto, OCR, ecuación, etiqueta o fuente"
        consulta.delegate = self
        consulta.translatesAutoresizingMaskIntoConstraints = false
        tabla.dataSource = self
        tabla.delegate = self
        tabla.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(consulta)
        view.addSubview(tabla)
        NSLayoutConstraint.activate([
            consulta.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            consulta.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            consulta.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tabla.topAnchor.constraint(equalTo: consulta.bottomAnchor),
            tabla.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tabla.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tabla.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
    }
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int { resultados.count }
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let r = resultados[indexPath.row]
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        c.textLabel?.text = r.fuente.titulo
        c.detailTextLabel?.text = String(r.texto.prefix(180))
        c.detailTextLabel?.numberOfLines = 3
        c.accessoryType = .disclosureIndicator
        return c
    }
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        let r = resultados[indexPath.row]
        let a = UIAlertController(title: r.fuente.titulo, message: String(r.texto.prefix(1200)), preferredStyle: .actionSheet)
        a.addAction(UIAlertAction(title: "Abrir fuente exacta", style: .default) { [weak self] _ in self?.alAbrirFuente?(r.fuente) })
        a.addAction(UIAlertAction(title: "Backlinks", style: .default) { [weak self] _ in self?.mostrarBacklinks(r) })
        a.addAction(UIAlertAction(title: "Favorito temporal 7 días", style: .default) { _ in
            RepositorioFavoritosConocimiento.compartido.agregar(FavoritoConocimiento(titulo: r.fuente.titulo, fuente: r.fuente, vence: Calendar.current.date(byAdding: .day, value: 7, to: Date())))
        })
        a.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        a.popoverPresentationController?.sourceView = view
        present(a, animated: true)
    }
    private func mostrarBacklinks(_ resultado: ResultadoConFuente) {
        let base = centro.instantanea().conocimiento
        guard let nodo = base.first(where: { $0.fuente == resultado.fuente }) else { return }
        let enlaces = ConstructorGrafoConocimiento.backlinks(para: nodo, en: base)
        let texto = enlaces.prefix(20).map { "• \($0.0.titulo) — \($0.1)" }.joined(separator: "\n")
        aviso(texto.isEmpty ? "No hay backlinks todavía." : texto)
    }
    @objc private func mapa() {
        let grafo = ConstructorGrafoConocimiento.construir(centro.instantanea().conocimiento)
        let controlador = UIViewController()
        controlador.title = "Mapa de conceptos"
        controlador.view.backgroundColor = .systemBackground
        let vista = VistaMapaConceptos(frame: CGRect(origin: .zero, size: CGSize(width: 2000, height: 2000)))
        vista.grafo = grafo
        vista.alElegir = { [weak self, weak controlador] concepto in
            let wiki = GeneradorWikiLocal.entrada(concepto: concepto, grafo: grafo)
            let texto = wiki.resumen + "\n\nRelacionado: " + wiki.relacionados.joined(separator: ", ")
            let a = UIAlertController(title: wiki.titulo, message: texto, preferredStyle: .actionSheet)
            for (i, f) in wiki.fuentes.prefix(8).enumerated() {
                a.addAction(UIAlertAction(title: "Fuente \(i+1): \(f.titulo)", style: .default) { [weak self] _ in self?.alAbrirFuente?(f) })
            }
            a.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
            a.popoverPresentationController?.sourceView = controlador?.view
            controlador?.present(a, animated: true)
        }
        let scroll = UIScrollView(frame: controlador.view.bounds)
        scroll.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        scroll.contentSize = vista.bounds.size
        scroll.minimumZoomScale = 0.2
        scroll.maximumZoomScale = 2
        scroll.addSubview(vista)
        controlador.view.addSubview(scroll)
        navigationController?.pushViewController(controlador, animated: true)
    }
    @objc private func formulario() {
        let formulas = GeneradorWikiLocal.formulario(centro.instantanea().conocimiento)
        let texto = formulas.prefix(100).enumerated().map { "\($0.offset+1). \($0.element.0)\n   Fuente: \($0.element.1.titulo)" }.joined(separator: "\n\n")
        aviso(texto.isEmpty ? "Todavía no se detectaron igualdades en el índice local." : texto)
    }
    @objc private func visual() {
        var config = PHPickerConfiguration(photoLibrary: .shared())
        config.selectionLimit = 1
        config.filter = .images
        let picker = PHPickerViewController(configuration: config)
        picker.delegate = self
        present(picker, animated: true)
    }
    func picker(_ picker: PHPickerViewController, didFinishPicking results: [PHPickerResult]) {
        picker.dismiss(animated: true)
        guard let proveedor = results.first?.itemProvider, proveedor.canLoadObject(ofClass: UIImage.self) else { return }
        proveedor.loadObject(ofClass: UIImage.self) { [weak self] objeto, _ in
            guard let self = self, let imagen = objeto as? UIImage else { return }
            BuscadorVisualLocal.buscar(imagen: imagen, en: self.sesion.medios) { coincidencias in
                DispatchQueue.main.async {
                    self.resultados = coincidencias.prefix(30).map {
                        ResultadoConFuente(texto: "Coincidencia visual local, distancia \(String(format: "%.4f", $0.distancia))", fuente: ReferenciaFuente(tipo: .apuntePropio, titulo: $0.medio.nombre, documentoId: self.sesion.documentoPersistenteId, elementoId: $0.medio.id), puntuacion: Double(1 - $0.distancia))
                    }
                    self.tabla.reloadData()
                }
            }
        }
    }
    private func aviso(_ texto: String) {
        let a = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(a, animated: true)
    }
    @objc private func cerrar() {
        dismiss(animated: true)
    }
}

extension PanelConocimiento: UISearchBarDelegate {
    func searchBarSearchButtonClicked(_ searchBar: UISearchBar) {
        resultados = centro.buscar(searchBar.text ?? "", limite: 100)
        tabla.reloadData()
        searchBar.resignFirstResponder()
    }
}
