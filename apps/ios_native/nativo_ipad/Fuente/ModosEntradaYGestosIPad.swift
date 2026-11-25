import UIKit

enum ModoEntradaLienzoIPad: String, Codable, CaseIterable, Equatable {
    case escritura, lectura, seleccion
    var nombre: String {
        switch self {
        case .escritura: return "Escritura"
        case .lectura: return "Lectura segura"
        case .seleccion: return "Selección"
        }
    }
    var detalle: String {
        switch self {
        case .escritura: return "Pencil escribe; el dedo desplaza."
        case .lectura: return "Bloquea tinta accidental; enlaces y desplazamiento continúan."
        case .seleccion: return "Pencil selecciona; el dedo desplaza."
        }
    }
}

enum DireccionDesplazamientoIPad: String, Codable, CaseIterable, Equatable {
    case vertical, horizontal, libre, paginada
    var nombre: String {
        switch self {
        case .vertical: return "Vertical"
        case .horizontal: return "Horizontal"
        case .libre: return "Libre XY"
        case .paginada: return "Páginas"
        }
    }
}

struct ConfiguracionEntradaIPad: Codable, Equatable {
    var modo: ModoEntradaLienzoIPad = .escritura
    var direccion: DireccionDesplazamientoIPad = .libre
    var panConDosDedos = false
    var navegacionTresDedos = true
    var reconocerEstrellaComoMarcador = false
    var squeezeMuestraMenuRadial = true
}

enum PreferenciasEntradaIPad {
    private static let clave = "Tinta.ConfiguracionEntradaIPad.v1"
    static var actual: ConfiguracionEntradaIPad {
        get {
            guard let datos = UserDefaults.standard.data(forKey: clave),
                  let valor = try? JSONDecoder().decode(
                    ConfiguracionEntradaIPad.self, from: datos) else {
                return ConfiguracionEntradaIPad()
            }
            return valor
        }
        set {
            guard let datos = try? JSONEncoder().encode(newValue) else { return }
            UserDefaults.standard.set(datos, forKey: clave)
        }
    }
}

enum DetectorMarcadorDibujadoIPad {
    static func esEstrella(_ originales: [CGPoint]) -> Bool {
        guard originales.count >= 12 else { return false }
        let marco = originales.reduce(CGRect.null) {
            $0.union(CGRect(origin: $1, size: CGSize(width: 0.01, height: 0.01)))
        }
        let diagonal = hypot(marco.width, marco.height)
        guard diagonal >= 16, diagonal <= 240,
              marco.width / max(marco.height, 1) > 0.45,
              marco.width / max(marco.height, 1) < 2.2,
              let primero = originales.first, let ultimo = originales.last,
              hypot(primero.x - ultimo.x, primero.y - ultimo.y) < diagonal * 0.5
        else { return false }

        let paso = max(1, originales.count / 70)
        var puntos = stride(from: 0, to: originales.count, by: paso).map {
            originales[$0]
        }
        if puntos.last != originales.last, let ultimo = originales.last { puntos.append(ultimo) }
        guard puntos.count >= 8 else { return false }
        var cruces = 0
        for i in 0..<(puntos.count - 1) {
            for j in (i + 2)..<(puntos.count - 1) {
                if i == 0 && j == puntos.count - 2 { continue }
                if intersectan(puntos[i], puntos[i + 1], puntos[j], puntos[j + 1]) {
                    cruces += 1
                    if cruces >= 3 { return true }
                }
            }
        }
        return false
    }

    private static func intersectan(_ a: CGPoint, _ b: CGPoint,
                                    _ c: CGPoint, _ d: CGPoint) -> Bool {
        func lado(_ p: CGPoint, _ q: CGPoint, _ r: CGPoint) -> CGFloat {
            (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x)
        }
        let abC = lado(a, b, c), abD = lado(a, b, d)
        let cdA = lado(c, d, a), cdB = lado(c, d, b)
        let epsilon: CGFloat = 0.001
        return abC * abD < -epsilon && cdA * cdB < -epsilon
    }
}

struct AccionMenuRadialIPad {
    var titulo: String
    var simbolo: String
    var ejecutar: () -> Void
}


final class PanelModosEntradaIPad: UITableViewController {
    var alAplicar: ((ConfiguracionEntradaIPad) -> Void)?
    private var configuracion: ConfiguracionEntradaIPad

    init(configuracion: ConfiguracionEntradaIPad) {
        self.configuracion = configuracion; super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Entrada, scroll y gestos"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 3 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? ModoEntradaLienzoIPad.allCases.count
            : (section == 1 ? DireccionDesplazamientoIPad.allCases.count : 4)
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Modo de interacción", "Dirección por página", "Gestos opcionales"][section]
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let modo = ModoEntradaLienzoIPad.allCases[indexPath.row]
            celda.textLabel?.text = modo.nombre; celda.detailTextLabel?.text = modo.detalle
            celda.accessoryType = configuracion.modo == modo ? .checkmark : .none
        } else if indexPath.section == 1 {
            let direccion = DireccionDesplazamientoIPad.allCases[indexPath.row]
            celda.textLabel?.text = direccion.nombre
            celda.accessoryType = configuracion.direccion == direccion ? .checkmark : .none
        } else {
            let titulos = ["Pan solo con dos dedos", "Navegar con tres dedos",
                           "Dibujar ⭐ crea marcador", "Squeeze abre menú radial"]
            celda.textLabel?.text = titulos[indexPath.row]
            let interruptor = UISwitch(); interruptor.tag = indexPath.row
            interruptor.isOn = [configuracion.panConDosDedos,
                                configuracion.navegacionTresDedos,
                                configuracion.reconocerEstrellaComoMarcador,
                                configuracion.squeezeMuestraMenuRadial][indexPath.row]
            interruptor.addTarget(self, action: #selector(cambiarInterruptor(_:)),
                                  for: .valueChanged)
            celda.accessoryView = interruptor; celda.selectionStyle = .none
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 0 {
            configuracion.modo = ModoEntradaLienzoIPad.allCases[indexPath.row]
        } else if indexPath.section == 1 {
            configuracion.direccion = DireccionDesplazamientoIPad.allCases[indexPath.row]
        } else { return }
        persistir(); tableView.reloadSections(IndexSet(integer: indexPath.section), with: .none)
    }
    @objc private func cambiarInterruptor(_ sender: UISwitch) {
        switch sender.tag {
        case 0: configuracion.panConDosDedos = sender.isOn
        case 1: configuracion.navegacionTresDedos = sender.isOn
        case 2: configuracion.reconocerEstrellaComoMarcador = sender.isOn
        case 3: configuracion.squeezeMuestraMenuRadial = sender.isOn
        default: break
        }
        persistir()
    }
    private func persistir() {
        PreferenciasEntradaIPad.actual = configuracion; alAplicar?(configuracion)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
