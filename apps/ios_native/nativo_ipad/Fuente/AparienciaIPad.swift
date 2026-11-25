import UIKit

extension Notification.Name {
    static let aparienciaTintaCambio = Notification.Name("aparienciaTintaCambio")
}

enum AparienciaTintaIPad: String, CaseIterable, Identifiable {
    case sistema
    case clara
    case oscura
    case cuidadoVisual
    case sol

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .sistema: return "Sistema"
        case .clara: return "Clara"
        case .oscura: return "Oscura"
        case .cuidadoVisual: return "Oscura · cuidado visual"
        case .sol: return "Sol · alto contraste"
        }
    }

    var detalle: String {
        switch self {
        case .sistema:
            return "Sigue automáticamente la apariencia configurada en iPadOS."
        case .clara:
            return "Interfaz clara estable; el papel conserva su color propio."
        case .oscura:
            return "Interfaz oscura estable; no convierte ni reexporta el papel."
        case .cuidadoVisual:
            return "Interfaz oscura y filtro cálido sobre el lienzo para reducir el contraste nocturno."
        case .sol:
            return "Interfaz clara y controles definidos para exteriores; no cambia el brillo del sistema."
        }
    }

    var simbolo: String {
        switch self {
        case .sistema: return "circle.lefthalf.filled"
        case .clara: return "sun.max"
        case .oscura: return "moon.stars"
        case .cuidadoVisual: return "eye"
        case .sol: return "sun.max.trianglebadge.exclamationmark"
        }
    }

    var estiloInterfaz: UIUserInterfaceStyle {
        switch self {
        case .sistema: return .unspecified
        case .clara, .sol: return .light
        case .oscura, .cuidadoVisual: return .dark
        }
    }

    /// El filtro no forma parte del documento ni de una exportación. Es una
    /// vista sin interacción situada únicamente encima del viewport.
    var colorFiltroLienzo: UIColor {
        switch self {
        case .cuidadoVisual:
            return UIColor(red: 0.16, green: 0.09, blue: 0.035, alpha: 0.24)
        case .sol:
            return UIColor(red: 1.0, green: 0.82, blue: 0.30, alpha: 0.025)
        default:
            return .clear
        }
    }
}

enum PreferenciasAparienciaTintaIPad {
    private static let clave = "tinta.apariencia.interfaz.v1"

    static var actual: AparienciaTintaIPad {
        let valor = UserDefaults.standard.string(forKey: clave)
        return valor.flatMap(AparienciaTintaIPad.init(rawValue:)) ?? .sistema
    }

    static func guardar(_ apariencia: AparienciaTintaIPad) {
        UserDefaults.standard.set(apariencia.rawValue, forKey: clave)
        NotificationCenter.default.post(name: .aparienciaTintaCambio,
                                        object: apariencia)
    }

    static func aplicarEnVentana(_ ventana: UIWindow) {
        ventana.overrideUserInterfaceStyle = actual.estiloInterfaz
        ventana.tintColor = actual == .sol ? .systemIndigo : nil
    }
}

final class PanelAparienciaTintaIPad: UITableViewController {
    var alCambiar: ((AparienciaTintaIPad) -> Void)?
    private var apariencia = PreferenciasAparienciaTintaIPad.actual

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Apariencia"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        AparienciaTintaIPad.allCases.count
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        "Interfaz y comodidad visual"
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        "La apariencia no modifica el color del documento guardado ni lo que se exporta. El filtro de cuidado visual tampoco recibe toques."
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let opcion = AparienciaTintaIPad.allCases[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = opcion.nombre
        celda.detailTextLabel?.text = opcion.detalle
        celda.detailTextLabel?.numberOfLines = 2
        celda.imageView?.image = UIImage(systemName: opcion.simbolo)
        celda.accessoryType = opcion == apariencia ? .checkmark : .none
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        apariencia = AparienciaTintaIPad.allCases[indexPath.row]
        PreferenciasAparienciaTintaIPad.guardar(apariencia)
        tableView.reloadData()
        alCambiar?(apariencia)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
