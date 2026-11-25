import UIKit

enum TipoBloqueLienzo: String, Codable, CaseIterable {
    case ecuacion = "Ecuación"
    case codigo = "Código"
    case circuito = "Circuito"
    case imagen = "Imagen"
    case tabla = "Tabla"
    case grafica = "Gráfica"
    case texto = "Texto"
    case manuscrito = "Handwriting"
    case pdf = "PDF"
    case audio = "Audio"
    case markdown = "Markdown"
    case diagramaBloques = "Diagrama de bloques"
    case diagramaLogico = "Diagrama lógico"
    case diagramaEstados = "Diagrama de estados"

    var simbolo: String {
        switch self { case .ecuacion: return "function"; case .codigo: return "chevron.left.forwardslash.chevron.right"
        case .circuito: return "resistor"; case .imagen: return "photo"; case .tabla: return "tablecells"
        case .grafica: return "chart.xyaxis.line"; case .texto: return "textformat"; case .manuscrito: return "pencil.line"
        case .pdf: return "doc.richtext"; case .audio: return "waveform"; case .markdown: return "text.document"
        case .diagramaBloques: return "square.3.layers.3d"; case .diagramaLogico: return "switch.2"
        case .diagramaEstados: return "point.3.connected.trianglepath.dotted" }
    }
}

struct CeldaTablaLienzo: Codable, Identifiable, Equatable {
    var id = UUID(); var fila: Int; var columna: Int; var texto = ""; var formula: String?
}

struct TablaLienzo: Codable, Equatable {
    var filas = 2; var columnas = 2; var celdas: [CeldaTablaLienzo] = []
    init(filas: Int = 2, columnas: Int = 2) {
        self.filas = max(filas, 1); self.columnas = max(columnas, 1)
        celdas = (0..<self.filas).flatMap { f in (0..<self.columnas).map {
            CeldaTablaLienzo(fila: f, columna: $0) } }
    }
    mutating func agregarFila() { let f=filas;filas += 1;for c in 0..<columnas{celdas.append(CeldaTablaLienzo(fila:f,columna:c))} }
    mutating func agregarColumna() { let c=columnas;columnas += 1;for f in 0..<filas{celdas.append(CeldaTablaLienzo(fila:f,columna:c))} }
    mutating func ordenar(columna: Int, ascendente: Bool) {
        guard columna >= 0, columna < columnas else { return }
        let filasOrdenadas=(0..<filas).sorted{a,b in
            let x=celdas.first{$0.fila==a&&$0.columna==columna}?.texto ?? ""
            let y=celdas.first{$0.fila==b&&$0.columna==columna}?.texto ?? ""
            return ascendente ? x.localizedStandardCompare(y) == .orderedAscending : x.localizedStandardCompare(y) == .orderedDescending }
        let antiguas=celdas;celdas=[]
        for (n,antigua) in filasOrdenadas.enumerated(){for c in 0..<columnas{if var celda=antiguas.first(where:{$0.fila==antigua&&$0.columna==c}){celda.fila=n;celdas.append(celda)}}}
    }
    var tsv: String { (0..<filas).map { f in (0..<columnas).map { c in
        celdas.first { $0.fila==f && $0.columna==c }?.texto ?? "" }.joined(separator:"\t") }.joined(separator:"\n") }
}

enum TipoNodoDiagrama: String, Codable, CaseIterable {
    case bloque, estado, and, or, xor, nand, nor, not, flipFlop, contador, mux
}
struct NodoDiagrama: Codable, Identifiable, Equatable {
    var id=UUID();var tipo:TipoNodoDiagrama;var etiqueta:String;var marco:CGRect
}
struct ConexionDiagrama: Codable, Identifiable, Equatable {
    var id=UUID();var origen:UUID;var destino:UUID;var etiqueta="";var negada=false
}
struct DiagramaEditable: Codable, Equatable {
    var nodos:[NodoDiagrama]=[];var conexiones:[ConexionDiagrama]=[]
    var tablaVerdad: [[Bool]]? {
        let entradas=nodos.filter{$0.etiqueta.lowercased().hasPrefix("in")}
        guard entradas.count <= 8, nodos.contains(where: { [.and,.or,.xor,.nand,.nor,.not].contains($0.tipo) }) else{return nil}
        return (0..<(1<<entradas.count)).map{valor in (0..<entradas.count).map{(valor & (1<<$0)) != 0}}
    }
}

struct BloqueLienzo: Codable, Identifiable, Equatable {
    var id=UUID();var tipo:TipoBloqueLienzo;var titulo:String;var contenido:String
    var marco:CGRect;var rotacion:CGFloat=0;var bloqueado=false;var capaId:UUID?
    var lenguaje="C++";var tabla:TablaLienzo?;var diagrama:DiagramaEditable?
    var fuenteElementoId:String?;var creado=Date();var modificado=Date()
}

final class RepositorioBloquesLienzo {
    static let compartido=RepositorioBloquesLienzo();private let url:URL
    private var documentos:[String:[BloqueLienzo]]=[:]
    private init(){let base=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask).first!
        let carpeta=base.appendingPathComponent("Bloques",isDirectory:true);try? FileManager.default.createDirectory(at:carpeta,withIntermediateDirectories:true)
        url=carpeta.appendingPathComponent("bloques.json");let d=JSONDecoder();d.dateDecodingStrategy = .iso8601
        if let datos=try? Data(contentsOf:url),let leido=try? d.decode([String:[BloqueLienzo]].self,from:datos){documentos=leido}}
    func bloques(documento:UInt64)->[BloqueLienzo]{documentos[String(documento)] ?? []}
    func guardar(_ bloques:[BloqueLienzo],documento:UInt64){documentos[String(documento)]=bloques;let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;e.outputFormatting=[.sortedKeys];if let datos=try? e.encode(documentos){try? datos.write(to:url,options:.atomic)}}
}

final class CapaBloquesLienzo: UIView {
    let documentoId: UInt64
    private(set) var bloques: [BloqueLienzo]
    var alConvertir: ((BloqueLienzo, DestinoFragmento) -> Void)?
    var alCambiar: ((BloqueLienzo) -> Void)?
    var alEliminar: ((UUID) -> Void)?
    
    init(documentoId: UInt64) {
        self.documentoId = documentoId
        bloques = RepositorioBloquesLienzo.compartido.bloques(documento: documentoId)
        super.init(frame: .zero)
        backgroundColor = .clear
        recargar()
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    
    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        for vista in subviews.reversed() {
            let p = vista.convert(point, from: self)
            if let r = vista.hitTest(p, with: event) { return r }
        }
        return nil
    }
    
    func recargar() {
        subviews.forEach { $0.removeFromSuperview() }
        for bloque in bloques {
            addSubview(VistaBloqueLienzo(bloque: bloque, propietario: self))
        }
    }
    
    @discardableResult func insertar(tipo: TipoBloqueLienzo, en punto: CGPoint, titulo: String? = nil, contenido: String = "") -> UUID {
        var b = BloqueLienzo(tipo: tipo, titulo: titulo ?? tipo.rawValue, contenido: contenido, marco: CGRect(x: punto.x, y: punto.y, width: tipo == .tabla ? 440 : 360, height: tipo == .codigo ? 240 : 220))
        if tipo == .tabla { b.tabla = TablaLienzo() }
        if [.diagramaBloques, .diagramaLogico, .diagramaEstados].contains(tipo) { b.diagrama = DiagramaEditable() }
        bloques.append(b)
        guardar()
        recargar()
        alCambiar?(b)
        return b.id
    }
    
    func persistir(_ vista: VistaBloqueLienzo) {
        guard let i = bloques.firstIndex(where: { $0.id == vista.bloque.id }) else { return }
        var b = vista.bloque
        b.marco = vista.marcoDocumento
        b.rotacion = vista.rotacion
        b.modificado = Date()
        bloques[i] = b
        guardar()
        alCambiar?(b)
    }
    
    func eliminar(_ id: UUID) {
        bloques.removeAll { $0.id == id }
        guardar()
        recargar()
        alEliminar?(id)
    }
    
    func borrarObjeto(en punto: CGPoint) -> Bool {
        guard let b = bloques.reversed().first(where: { $0.marco.contains(punto) }) else { return false }
        guard !b.bloqueado else { return false }
        eliminar(b.id)
        return true
    }
    
    func duplicar(_ id: UUID) {
        guard var b = bloques.first(where: { $0.id == id }) else { return }
        b.id = UUID()
        b.marco = b.marco.offsetBy(dx: 28, dy: 28)
        b.creado = Date()
        b.modificado = Date()
        bloques.append(b)
        guardar()
        recargar()
        alCambiar?(b)
    }
    
    func actualizar(_ bloque: BloqueLienzo) {
        guard let i = bloques.firstIndex(where: { $0.id == bloque.id }) else { return }
        bloques[i] = bloque
        guardar()
        recargar()
        alCambiar?(bloque)
    }
    
    func reemplazar(_ nuevos: [BloqueLienzo]) {
        bloques = nuevos
        guardar()
        recargar()
    }
    
    private func guardar() {
        RepositorioBloquesLienzo.compartido.guardar(bloques, documento: documentoId)
    }
}

final class VistaBloqueLienzo: UIView, UIContextMenuInteractionDelegate, UITextViewDelegate, UIGestureRecognizerDelegate {
    var bloque: BloqueLienzo
    weak var propietario: CapaBloquesLienzo?
    var rotacion: CGFloat
    private let editor = UITextView()
    private let titulo = UILabel()
    private let contenido = VistaContenidoBloque()

    init(bloque: BloqueLienzo, propietario: CapaBloquesLienzo) {
        self.bloque = bloque
        self.propietario = propietario
        rotacion = bloque.rotacion
        super.init(frame: bloque.marco)
        backgroundColor = UIColor.secondarySystemBackground.withAlphaComponent(0.92)
        layer.cornerRadius = 10
        layer.borderWidth = 1
        layer.borderColor = UIColor.systemIndigo.cgColor
        clipsToBounds = true
        transform = CGAffineTransform(rotationAngle: rotacion)
        construir()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }

    var marcoDocumento: CGRect {
        CGRect(x: center.x - bounds.width / 2, y: center.y - bounds.height / 2, width: bounds.width, height: bounds.height)
    }

    private func construir() {
        titulo.text = "  \(bloque.tipo.rawValue) · \(bloque.titulo)"
        titulo.font = .preferredFont(forTextStyle: .caption1)
        titulo.backgroundColor = UIColor.systemIndigo.withAlphaComponent(0.16)
        titulo.frame = CGRect(x: 0, y: 0, width: bounds.width, height: 28)
        titulo.autoresizingMask = [.flexibleWidth]
        addSubview(titulo)
        
        contenido.frame = CGRect(x: 0, y: 28, width: bounds.width, height: bounds.height - 28)
        contenido.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        contenido.bloque = bloque
        addSubview(contenido)
        
        if [.texto, .codigo, .markdown, .ecuacion].contains(bloque.tipo) {
            editor.frame = contenido.bounds
            editor.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            editor.backgroundColor = .clear
            editor.text = bloque.contenido
            editor.font = bloque.tipo == .codigo ? .monospacedSystemFont(ofSize: 14, weight: .regular) : .preferredFont(forTextStyle: .body)
            editor.delegate = self
            contenido.addSubview(editor)
        }
        
        let pan = UIPanGestureRecognizer(target: self, action: #selector(mover(_:)))
        addGestureRecognizer(pan)
        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(escalar(_:)))
        pinch.delegate = self
        addGestureRecognizer(pinch)
        let giro = UIRotationGestureRecognizer(target: self, action: #selector(girar(_:)))
        giro.delegate = self
        addGestureRecognizer(giro)
        addInteraction(UIContextMenuInteraction(delegate: self))
    }

    @objc private func mover(_ g: UIPanGestureRecognizer) {
        guard !bloque.bloqueado, let s = superview else { return }
        let d = g.translation(in: s)
        center = CGPoint(x: center.x + d.x, y: center.y + d.y)
        g.setTranslation(.zero, in: s)
        if g.state == .ended { persistir() }
    }

    @objc private func escalar(_ g: UIPinchGestureRecognizer) {
        guard !bloque.bloqueado else { return }
        bounds.size = CGSize(width: min(max(bounds.width * g.scale, 120), 4000), height: min(max(bounds.height * g.scale, 80), 4000))
        g.scale = 1
        if g.state == .ended { persistir() }
    }

    @objc private func girar(_ g: UIRotationGestureRecognizer) {
        guard !bloque.bloqueado else { return }
        rotacion += g.rotation
        transform = CGAffineTransform(rotationAngle: rotacion)
        g.rotation = 0
        if g.state == .ended { persistir() }
    }

    func textViewDidChange(_ textView: UITextView) { bloque.contenido = textView.text }
    func textViewDidEndEditing(_ textView: UITextView) { persistir() }
    
    private func persistir() {
        bloque.marco = marcoDocumento
        bloque.rotacion = rotacion
        propietario?.persistir(self)
    }
    
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool { true }
    
    func contextMenuInteraction(_ interaction: UIContextMenuInteraction, configurationForMenuAtLocation location: CGPoint) -> UIContextMenuConfiguration? {
        UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { [weak self] _ in
            guard let self else { return UIMenu() }
            var acciones: [UIMenuElement] = []
            
            if var tabla = self.bloque.tabla {
                let fila = UIAction(title: "Añadir fila", image: UIImage(systemName: "rectangle.split.1x2")) { [weak self] _ in
                    guard let self else { return }
                    tabla.agregarFila()
                    self.bloque.tabla = tabla
                    self.propietario?.actualizar(self.bloque)
                }
                let columna = UIAction(title: "Añadir columna", image: UIImage(systemName: "rectangle.split.2x1")) { [weak self] _ in
                    guard let self else { return }
                    tabla.agregarColumna()
                    self.bloque.tabla = tabla
                    self.propietario?.actualizar(self.bloque)
                }
                let copiar = UIAction(title: "Copiar TSV para Excel", image: UIImage(systemName: "doc.on.doc")) { _ in UIPasteboard.general.string = tabla.tsv }
                acciones += [fila, columna, copiar]
            }
            
            let conversiones: [UIMenuElement] = DestinoFragmento.allCases.map { d in
                UIAction(title: "Convertir en \(d.rawValue)") { [weak self] _ in
                    guard let self else { return }
                    self.propietario?.alConvertir?(self.bloque, d)
                }
            }
            
            acciones.append(UIMenu(title: "Usar fragmento", children: conversiones))
            acciones.append(UIAction(title: self.bloque.bloqueado ? "Desbloquear" : "Bloquear", image: UIImage(systemName: "lock")) { [weak self] _ in
                self?.bloque.bloqueado.toggle()
                self?.persistir()
            })
            acciones.append(UIAction(title: "Duplicar", image: UIImage(systemName: "plus.square.on.square")) { [weak self] _ in
                guard let self else { return }
                self.propietario?.duplicar(self.bloque.id)
            })
            acciones.append(UIAction(title: "Eliminar", image: UIImage(systemName: "trash"), attributes: .destructive) { [weak self] _ in
                guard let self else { return }
                self.propietario?.eliminar(self.bloque.id)
            })
            
            return UIMenu(children: acciones)
        }
    }
}

final class VistaContenidoBloque: UIView {
    var bloque: BloqueLienzo? { didSet { setNeedsDisplay() } }
    
    override func draw(_ rect: CGRect) {
        guard let b = bloque, let c = UIGraphicsGetCurrentContext() else { return }
        switch b.tipo {
        case .tabla: dibujarTabla(b.tabla ?? TablaLienzo(), c)
        case .diagramaBloques, .diagramaLogico, .diagramaEstados: dibujarDiagrama(b.diagrama ?? DiagramaEditable(), c)
        case .circuito: icono("resistor", b.contenido)
        case .grafica: icono("chart.xyaxis.line", b.contenido)
        case .imagen: icono("photo", b.contenido)
        case .pdf: icono("doc.richtext", b.contenido)
        case .audio: icono("waveform", b.contenido)
        case .manuscrito: icono("pencil.line", b.contenido)
        case .ecuacion, .codigo, .texto, .markdown: break
        }
    }
    
    private func dibujarTabla(_ t: TablaLienzo, _ c: CGContext) {
        let w = bounds.width / CGFloat(t.columnas), h = bounds.height / CGFloat(t.filas)
        c.setStrokeColor(UIColor.separator.cgColor)
        for f in 0...t.filas {
            c.move(to: CGPoint(x: 0, y: CGFloat(f) * h))
            c.addLine(to: CGPoint(x: bounds.width, y: CGFloat(f) * h))
        }
        for col in 0...t.columnas {
            c.move(to: CGPoint(x: CGFloat(col) * w, y: 0))
            c.addLine(to: CGPoint(x: CGFloat(col) * w, y: bounds.height))
        }
        c.strokePath()
        for celda in t.celdas {
            let p = CGPoint(x: CGFloat(celda.columna) * w + 5, y: CGFloat(celda.fila) * h + 5)
            (celda.texto as NSString).draw(at: p, withAttributes: [.font: UIFont.systemFont(ofSize: 12), .foregroundColor: UIColor.label])
        }
    }
    
    private func dibujarDiagrama(_ d: DiagramaEditable, _ c: CGContext) {
        for con in d.conexiones {
            guard let a = d.nodos.first(where: { $0.id == con.origen }),
                  let b = d.nodos.first(where: { $0.id == con.destino }) else { continue }
            c.move(to: CGPoint(x: a.marco.midX, y: a.marco.midY))
            c.addLine(to: CGPoint(x: b.marco.midX, y: b.marco.midY))
            c.strokePath()
        }
        for n in d.nodos {
            let r = n.marco
            c.setStrokeColor(UIColor.systemBlue.cgColor)
            if n.tipo == .estado {
                c.strokeEllipse(in: r)
            } else {
                c.stroke(r)
            }
            (n.etiqueta as NSString).draw(at: CGPoint(x: r.minX + 5, y: r.midY - 8), withAttributes: [.font: UIFont.systemFont(ofSize: 12), .foregroundColor: UIColor.label])
        }
    }
    
    private func icono(_ nombre: String, _ texto: String) {
        let config = UIImage.SymbolConfiguration(pointSize: 44)
        UIImage(systemName: nombre, withConfiguration: config)?.draw(at: CGPoint(x: 16, y: 16))
        (texto as NSString).draw(in: CGRect(x: 76, y: 16, width: bounds.width - 88, height: bounds.height - 24), withAttributes: [.font: UIFont.systemFont(ofSize: 14), .foregroundColor: UIColor.label])
    }
}

enum ReconocedorBloquesTinta {
    static func marcosUltimos(_ cantidad: Int, sesion: Sesion) -> [CGRect] {
        guard cantidad > 0 else { return [] }
        let inicio = max(sesion.cantidadTrazos - cantidad, 0)
        return (inicio..<sesion.cantidadTrazos).compactMap { marcoTrazo($0, sesion: sesion) }
    }
    
    static func tabla(sesion: Sesion, cantidad: Int) -> (TablaLienzo, CGRect, Float)? {
        let m = marcosUltimos(cantidad, sesion: sesion)
        let horizontales = m.filter { $0.width > $0.height * 4 }
        let verticales = m.filter { $0.height > $0.width * 4 }
        guard horizontales.count >= 2, verticales.count >= 2 else { return nil }
        let union = m.reduce(m[0]) { $0.union($1) }
        let tabla = TablaLienzo(filas: max(horizontales.count - 1, 1), columnas: max(verticales.count - 1, 1))
        let confianza = Float(min(1, Double(horizontales.count + verticales.count) / 8))
        return (tabla, union, confianza)
    }
    
    static func diagrama(sesion: Sesion, cantidad: Int, tipo: TipoBloqueLienzo) -> (DiagramaEditable, CGRect, Float)? {
        let marcos = marcosUltimos(cantidad, sesion: sesion)
        guard marcos.count >= 2 else { return nil }
        let union = marcos.dropFirst().reduce(marcos[0]) { $0.union($1) }
        var d = DiagramaEditable()
        let candidatos = marcos.filter { $0.width > 30 && $0.height > 20 }
        for (i, r) in candidatos.enumerated() {
            let nodoTipo: TipoNodoDiagrama = tipo == .diagramaEstados ? .estado : (tipo == .diagramaLogico ? .and : .bloque)
            d.nodos.append(NodoDiagrama(tipo: nodoTipo, etiqueta: tipo == .diagramaEstados ? "S\(i)" : (tipo == .diagramaLogico ? "G\(i)" : "H\(i)(s)"), marco: r.offsetBy(dx: -union.minX, dy: -union.minY)))
        }
        for i in 0..<(max(d.nodos.count - 1, 0)) {
            d.conexiones.append(ConexionDiagrama(origen: d.nodos[i].id, destino: d.nodos[i + 1].id))
        }
        return (d, union, Float(min(0.85, 0.35 + Double(candidatos.count) * 0.1)))
    }
    
    private static func marcoTrazo(_ indice: Int, sesion: Sesion) -> CGRect? {
        guard let v = sesion.verticesDelTrazo(indice), v.cantidad > 0 else { return nil }
        var xmin = CGFloat.greatestFiniteMagnitude
        var ymin = xmin
        var xmax = -xmin
        var ymax = -xmin
        for i in 0..<v.cantidad {
            let x = CGFloat(v.datos[i * 2])
            let y = CGFloat(v.datos[i * 2 + 1])
            xmin = min(xmin, x)
            xmax = max(xmax, x)
            ymin = min(ymin, y)
            ymax = max(ymax, y)
        }
        return CGRect(x: xmin, y: ymin, width: max(xmax - xmin, 1), height: max(ymax - ymin, 1))
    }
}

final class PanelBloquesLienzo: UITableViewController {
    private weak var capa: CapaBloquesLienzo?
    private let sesion: Sesion
    private let centro: () -> CGPoint
    
    init(capa: CapaBloquesLienzo, sesion: Sesion, centro: @escaping () -> CGPoint) {
        self.capa = capa
        self.sesion = sesion
        self.centro = centro
        super.init(style: .insetGrouped)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Bloques en el lienzo"
        navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        2
    }
    
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Insertar sin limitar escritura libre" : "Reconocer últimos trazos sin borrar original"
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        section == 0 ? TipoBloqueLienzo.allCases.count : 4
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let t = TipoBloqueLienzo.allCases[indexPath.row]
            c.textLabel?.text = t.rawValue
            c.imageView?.image = UIImage(systemName: t.simbolo)
        } else {
            let nombres = ["Tabla dibujada", "Diagrama de bloques", "Diagrama lógico", "Diagrama de estados"]
            c.textLabel?.text = nombres[indexPath.row]
            c.detailTextLabel?.text = "Pide confirmación y conserva la tinta"
        }
        c.accessoryType = .disclosureIndicator
        return c
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 0 {
            insertar(TipoBloqueLienzo.allCases[indexPath.row])
        } else {
            reconocer(indexPath.row)
        }
    }
    
    private func insertar(_ tipo: TipoBloqueLienzo) {
        if tipo == .tabla {
            pedirTabla()
            return
        }
        _ = capa?.insertar(tipo: tipo, en: centro())
        dismiss(animated: true)
    }
    
    private func pedirTabla() {
        let a = UIAlertController(title: "Tamaño de tabla", message: "filas,columnas", preferredStyle: .alert)
        a.addTextField { $0.text = "3,3" }
        a.addAction(UIAlertAction(title: "Insertar", style: .default) { [weak self] _ in
            guard let self else { return }
            let v = a.textFields?.first?.text?.split(separator: ",").compactMap { Int($0) } ?? []
            guard v.count == 2 else { return }
            let id = self.capa?.insertar(tipo: .tabla, en: self.centro())
            if let id, let b = self.capa?.bloques.first(where: { $0.id == id }) {
                var nuevo = b
                nuevo.tabla = TablaLienzo(filas: v[0], columnas: v[1])
                self.capa?.actualizar(nuevo)
            }
            self.dismiss(animated: true)
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
    }
    
    private func reconocer(_ fila: Int) {
        let a = UIAlertController(title: "Cantidad de últimos trazos", message: "La tinta original no se modifica.", preferredStyle: .alert)
        a.addTextField {
            $0.text = fila == 0 ? "6" : "8"
            $0.keyboardType = .numberPad
        }
        a.addAction(UIAlertAction(title: "Analizar", style: .default) { [weak self] _ in
            guard let self, let n = Int(a.textFields?.first?.text ?? "") else { return }
            if fila == 0, let r = ReconocedorBloquesTinta.tabla(sesion: self.sesion, cantidad: n) {
                self.confirmarTabla(r)
            } else {
                let tipo = [TipoBloqueLienzo.tabla, .diagramaBloques, .diagramaLogico, .diagramaEstados][fila]
                if let r = ReconocedorBloquesTinta.diagrama(sesion: self.sesion, cantidad: n, tipo: tipo) {
                    self.confirmarDiagrama(r, tipo: tipo)
                } else {
                    self.aviso("No se detectó una estructura suficiente.")
                }
            }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
    }
    
    private func confirmarTabla(_ r: (TablaLienzo, CGRect, Float)) {
        let a = UIAlertController(title: "Candidato a tabla · \(Int(r.2 * 100))%", message: "\(r.0.filas) × \(r.0.columnas). Confirme antes de crear el objeto editable.", preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self] _ in
            guard let self else { return }
            let id = self.capa?.insertar(tipo: .tabla, en: r.1.origin)
            if let id, let b = self.capa?.bloques.first(where: { $0.id == id }) {
                var nuevo = b
                nuevo.marco = r.1
                nuevo.tabla = r.0
                self.capa?.actualizar(nuevo)
            }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
    }
    
    private func confirmarDiagrama(_ r: (DiagramaEditable, CGRect, Float), tipo: TipoBloqueLienzo) {
        let a = UIAlertController(title: "Candidato · \(Int(r.2 * 100))%", message: "Revise tipo, etiquetas y conexiones. El dibujo queda intacto.", preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Crear editable", style: .default) { [weak self] _ in
            guard let self else { return }
            let id = self.capa?.insertar(tipo: tipo, en: r.1.origin)
            if let id, let b = self.capa?.bloques.first(where: { $0.id == id }) {
                var nuevo = b
                nuevo.marco = r.1
                nuevo.diagrama = r.0
                self.capa?.actualizar(nuevo)
            }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
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
