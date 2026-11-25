import UIKit

// MARK: - Modelo

struct ConfiguracionMenuRadialIPad: Codable, Equatable {
    var elementos: [ElementoMenuRadialIPad]
    var radioInterno: CGFloat = 35
    var radioExterno: CGFloat = 120
    var colorFondo: UInt32 = 0xCC1C1C1E  // ARGB oscuro semitransparente
    var haptica = true
}

struct ElementoMenuRadialIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var accion: String   // rawValue de AccionIPad
    var icono: String    // SF Symbol
    var etiqueta: String
    var colorHex: UInt32 = 0xFF007AFF
}

// MARK: - Repositorio

final class RepositorioMenuRadialIPad {
    static let compartido = RepositorioMenuRadialIPad()
    private let cola = DispatchQueue(label: "tinta.menu.radial")
    private let url: URL
    private var configuraciones: [String: ConfiguracionMenuRadialIPad] = [:]

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/MenuRadial", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("radial.json")
        configuraciones = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode([String: ConfiguracionMenuRadialIPad].self, from: $0)
        } ?? [:]
    }

    func configuracion(_ perfil: String) -> ConfiguracionMenuRadialIPad {
        cola.sync { configuraciones[perfil] ?? Self.porDefecto() }
    }

    func guardar(_ config: ConfiguracionMenuRadialIPad, perfil: String) {
        cola.sync {
            configuraciones[perfil] = config
            guard let datos = try? JSONEncoder().encode(configuraciones) else { return }
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }

    static func porDefecto() -> ConfiguracionMenuRadialIPad {
        ConfiguracionMenuRadialIPad(elementos: [
            ElementoMenuRadialIPad(accion: "deshacer", icono: "arrow.uturn.backward",
                                   etiqueta: "Deshacer"),
            ElementoMenuRadialIPad(accion: "rehacer", icono: "arrow.uturn.forward",
                                   etiqueta: "Rehacer"),
            ElementoMenuRadialIPad(accion: "borradorObjeto", icono: "eraser",
                                   etiqueta: "Borrador"),
            ElementoMenuRadialIPad(accion: "resaltador", icono: "highlighter",
                                   etiqueta: "Resaltador"),
            ElementoMenuRadialIPad(accion: "lazo", icono: "lasso",
                                   etiqueta: "Lazo"),
            ElementoMenuRadialIPad(accion: "pluma", icono: "pencil.tip",
                                   etiqueta: "Pluma"),
            ElementoMenuRadialIPad(accion: "marcador", icono: "bookmark",
                                   etiqueta: "Marcador"),
            ElementoMenuRadialIPad(accion: "escanearPizarra", icono: "camera.viewfinder",
                                   etiqueta: "Capturar"),
        ])
    }
}

// MARK: - Vista del menú radial

final class MenuRadialIPad: UIView {

    var alSeleccionar: ((String) -> Void)?  // rawValue de AccionIPad

    private var configuracion: ConfiguracionMenuRadialIPad
    private var capaBotones: [CAShapeLayer] = []
    private var capaIconos: [CALayer] = []
    private var seleccionActual: Int? = nil
    private let centroMenu: CGPoint
    private let generadorHaptico = UIImpactFeedbackGenerator(style: .medium)

    init(centro: CGPoint, configuracion: ConfiguracionMenuRadialIPad) {
        self.centroMenu = centro
        self.configuracion = configuracion
        let lado = configuracion.radioExterno * 2 + 40
        let marco = CGRect(x: centro.x - lado / 2, y: centro.y - lado / 2,
                           width: lado, height: lado)
        super.init(frame: marco)
        backgroundColor = .clear
        isUserInteractionEnabled = true
        clipsToBounds = false
        construir()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    // MARK: Construcción

    private func construir() {
        let n = configuracion.elementos.count
        guard n > 0 else { return }
        let anguloSlice = 2 * CGFloat.pi / CGFloat(n)
        let centroLocal = CGPoint(x: bounds.midX, y: bounds.midY)
        let rInt = configuracion.radioInterno
        let rExt = configuracion.radioExterno

        // Fondo circular semitransparente
        let fondoLayer = CAShapeLayer()
        fondoLayer.path = UIBezierPath(arcCenter: centroLocal, radius: rExt,
                                       startAngle: 0, endAngle: 2 * .pi,
                                       clockwise: true).cgPath
        let hueco = UIBezierPath(arcCenter: centroLocal, radius: rInt,
                                 startAngle: 0, endAngle: 2 * .pi, clockwise: true)
        fondoLayer.path = {
            let p = UIBezierPath(arcCenter: centroLocal, radius: rExt,
                                 startAngle: 0, endAngle: 2 * .pi, clockwise: true)
            p.append(hueco); p.usesEvenOddFillRule = true; return p.cgPath
        }()
        fondoLayer.fillColor = UIColor(white: 0.11, alpha: 0.82).cgColor
        fondoLayer.fillRule = .evenOdd
        layer.addSublayer(fondoLayer)

        for i in 0..<n {
            let anguloInicio = CGFloat(i) * anguloSlice - CGFloat.pi / 2
            let anguloFin = anguloInicio + anguloSlice

            // Sector
            let sector = CAShapeLayer()
            let path = UIBezierPath()
            path.addArc(withCenter: centroLocal, radius: rExt,
                        startAngle: anguloInicio, endAngle: anguloFin, clockwise: true)
            path.addArc(withCenter: centroLocal, radius: rInt,
                        startAngle: anguloFin, endAngle: anguloInicio, clockwise: false)
            path.close()
            sector.path = path.cgPath
            sector.fillColor = UIColor.clear.cgColor
            sector.strokeColor = UIColor.separator.cgColor
            sector.lineWidth = 0.5
            layer.addSublayer(sector)
            capaBotones.append(sector)

            // Icono en el centro del arco
            let anguloMedio = (anguloInicio + anguloFin) / 2
            let radioMedio = (rInt + rExt) / 2
            let posIcono = CGPoint(
                x: centroLocal.x + radioMedio * cos(anguloMedio),
                y: centroLocal.y + radioMedio * sin(anguloMedio))

            let tamIcono: CGFloat = 22
            let imagen = UIImage(systemName: configuracion.elementos[i].icono,
                                 withConfiguration: UIImage.SymbolConfiguration(
                                     pointSize: tamIcono, weight: .semibold))?
                .withTintColor(.white, renderingMode: .alwaysOriginal)
            let capaIcono = CALayer()
            capaIcono.frame = CGRect(x: posIcono.x - tamIcono / 2,
                                     y: posIcono.y - tamIcono / 2,
                                     width: tamIcono, height: tamIcono)
            capaIcono.contents = imagen?.cgImage
            capaIcono.contentsGravity = .resizeAspect
            layer.addSublayer(capaIcono)
            capaIconos.append(capaIcono)
        }

        // Animación de entrada
        alpha = 0
        transform = CGAffineTransform(scaleX: 0.3, y: 0.3)
        UIView.animate(withDuration: 0.28, delay: 0,
                       usingSpringWithDamping: 0.65, initialSpringVelocity: 0) {
            self.alpha = 1; self.transform = .identity
        }
    }

    // MARK: Interacción

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let toque = touches.first else { return }
        let punto = toque.location(in: self)
        let centroLocal = CGPoint(x: bounds.midX, y: bounds.midY)
        let dx = punto.x - centroLocal.x
        let dy = punto.y - centroLocal.y
        let distancia = sqrt(dx * dx + dy * dy)

        guard distancia > configuracion.radioInterno,
              distancia < configuracion.radioExterno + 20 else {
            actualizarSeleccion(nil); return
        }

        var angulo = atan2(dy, dx) + CGFloat.pi / 2
        if angulo < 0 { angulo += 2 * .pi }
        let n = configuracion.elementos.count
        let indice = Int(angulo / (2 * .pi / CGFloat(n))) % n
        actualizarSeleccion(indice)
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let idx = seleccionActual, idx < configuracion.elementos.count {
            if configuracion.haptica { generadorHaptico.impactOccurred() }
            let accion = configuracion.elementos[idx].accion
            UIView.animate(withDuration: 0.2, animations: {
                self.alpha = 0; self.transform = CGAffineTransform(scaleX: 0.5, y: 0.5)
            }) { _ in
                self.removeFromSuperview()
                self.alSeleccionar?(accion)
            }
        } else {
            UIView.animate(withDuration: 0.15, animations: { self.alpha = 0 }) { _ in
                self.removeFromSuperview()
            }
        }
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        UIView.animate(withDuration: 0.15, animations: { self.alpha = 0 }) { _ in
            self.removeFromSuperview()
        }
    }

    private func actualizarSeleccion(_ nuevo: Int?) {
        guard nuevo != seleccionActual else { return }
        if let anterior = seleccionActual, anterior < capaBotones.count {
            capaBotones[anterior].fillColor = UIColor.clear.cgColor
        }
        seleccionActual = nuevo
        if let actual = nuevo, actual < capaBotones.count {
            capaBotones[actual].fillColor = UIColor.systemBlue.withAlphaComponent(0.4).cgColor
            if configuracion.haptica {
                UISelectionFeedbackGenerator().selectionChanged()
            }
        }
    }
}

// MARK: - Panel de configuración del menú radial

final class PanelConfiguracionMenuRadialIPad: UITableViewController {
    var alAplicar: ((ConfiguracionMenuRadialIPad) -> Void)?
    private var config: ConfiguracionMenuRadialIPad

    init(config: ConfiguracionMenuRadialIPad) {
        self.config = config
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Menú radial"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Guardar", style: .done, target: self, action: #selector(guardar))
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "celda")
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Elementos (\(config.elementos.count) / 8)" : "Opciones"
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? config.elementos.count : 2
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = tableView.dequeueReusableCell(withIdentifier: "celda",
                                                  for: indexPath)
        if indexPath.section == 0 {
            let elem = config.elementos[indexPath.row]
            celda.imageView?.image = UIImage(systemName: elem.icono)
            celda.textLabel?.text = elem.etiqueta
            celda.detailTextLabel?.text = elem.accion
        } else {
            switch indexPath.row {
            case 0:
                celda.textLabel?.text = "Háptica"
                let sw = UISwitch()
                sw.isOn = config.haptica
                sw.addAction(UIAction { [weak self] _ in
                    self?.config.haptica = sw.isOn
                }, for: .valueChanged)
                celda.accessoryView = sw
            default:
                celda.textLabel?.text = "Restaurar valores por defecto"
                celda.textLabel?.textColor = .systemRed
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if indexPath.section == 1 && indexPath.row == 1 {
            config = RepositorioMenuRadialIPad.porDefecto()
            tableView.reloadData()
        }
    }

    override func tableView(_ tableView: UITableView,
                            canEditRowAt indexPath: IndexPath) -> Bool {
        indexPath.section == 0
    }

    override func tableView(_ tableView: UITableView,
                            commit editingStyle: UITableViewCell.EditingStyle,
                            forRowAt indexPath: IndexPath) {
        guard editingStyle == .delete, indexPath.section == 0 else { return }
        config.elementos.remove(at: indexPath.row)
        tableView.deleteRows(at: [indexPath], with: .automatic)
    }

    @objc private func guardar() {
        alAplicar?(config)
        dismiss(animated: true)
    }
}
