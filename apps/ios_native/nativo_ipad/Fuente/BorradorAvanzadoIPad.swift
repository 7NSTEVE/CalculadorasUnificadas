import UIKit

enum ModoBorradorAvanzadoIPad: String, Codable, CaseIterable {
    case trazo = "Trazo completo"
    case pixel = "Píxel / corte parcial"
    case objeto = "Objeto del lienzo"
}

enum FiltroBorradorAvanzadoIPad: Int32, Codable, CaseIterable {
    case todos = 0
    case soloResaltador = 1
    case soloColor = 2
    case soloManuscrito = 3
    case soloCapaActual = 4

    var nombre: String {
        switch self {
        case .todos: return "Todo lo editable"
        case .soloResaltador: return "Solo resaltador"
        case .soloColor: return "Solo el color elegido"
        case .soloManuscrito: return "Solo escritura / tinta"
        case .soloCapaActual: return "Solo la capa de tinta actual"
        }
    }
}

struct AjustesBorradorAvanzadoIPad: Codable, Equatable {
    var modo: ModoBorradorAvanzadoIPad = .trazo
    var filtro: FiltroBorradorAvanzadoIPad = .todos
    var colorARGB: UInt32 = 0xFFFF_3B30
    var radio: Float = 18

    static var actual: AjustesBorradorAvanzadoIPad {
        get {
            guard let datos = UserDefaults.standard.data(
                forKey: "tinta.borrador.avanzado"),
                  let valor = try? JSONDecoder().decode(
                    AjustesBorradorAvanzadoIPad.self, from: datos) else {
                return AjustesBorradorAvanzadoIPad()
            }
            return valor
        }
        set {
            if let datos = try? JSONEncoder().encode(newValue) {
                UserDefaults.standard.set(datos, forKey: "tinta.borrador.avanzado")
            }
        }
    }
}

final class PanelBorradorAvanzadoIPad: UITableViewController {
    var alAplicar: ((AjustesBorradorAvanzadoIPad) -> Void)?
    private var ajustes: AjustesBorradorAvanzadoIPad
    private let colores: [(String, UInt32)] = [
        ("Rojo", 0xFFFF_3B30), ("Azul", 0xFF0A_84FF),
        ("Negro", 0xFF1A_1A1A), ("Verde", 0xFF34_C759),
        ("Naranja", 0xFFFF_9500), ("Morado", 0xFFAF_52DE),
    ]
    private let radios: [Float] = [6, 12, 18, 28, 44, 72]

    init(ajustes: AjustesBorradorAvanzadoIPad) {
        self.ajustes = ajustes
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Borrador avanzado"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Aplicar", style: .done, target: self, action: #selector(aplicar))
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 4 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return ModoBorradorAvanzadoIPad.allCases.count
        case 1: return FiltroBorradorAvanzadoIPad.allCases.count
        case 2: return colores.count
        default: return radios.count
        }
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Qué borra", "Filtro", "Color del filtro", "Radio del borrador"][section]
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        guard section == 0 else { return nil }
        return "Objeto elimina texto, imágenes y bloques editables. Los PDF, objetos bloqueados y capas bloqueadas quedan protegidos."
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            let valor = ModoBorradorAvanzadoIPad.allCases[indexPath.row]
            celda.textLabel?.text = valor.rawValue
            celda.accessoryType = ajustes.modo == valor ? .checkmark : .none
        case 1:
            let valor = FiltroBorradorAvanzadoIPad.allCases[indexPath.row]
            celda.textLabel?.text = valor.nombre
            celda.accessoryType = ajustes.filtro == valor ? .checkmark : .none
        case 2:
            let valor = colores[indexPath.row]
            celda.textLabel?.text = valor.0
            celda.imageView?.image = muestraColor(valor.1)
            celda.accessoryType = ajustes.colorARGB == valor.1 ? .checkmark : .none
        default:
            let valor = radios[indexPath.row]
            celda.textLabel?.text = "\(Int(valor)) pt"
            celda.accessoryType = abs(ajustes.radio - valor) < 0.1 ? .checkmark : .none
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        switch indexPath.section {
        case 0: ajustes.modo = ModoBorradorAvanzadoIPad.allCases[indexPath.row]
        case 1: ajustes.filtro = FiltroBorradorAvanzadoIPad.allCases[indexPath.row]
        case 2:
            ajustes.colorARGB = colores[indexPath.row].1
            ajustes.filtro = .soloColor
        default: ajustes.radio = radios[indexPath.row]
        }
        tableView.reloadData()
    }

    private func muestraColor(_ argb: UInt32) -> UIImage {
        UIGraphicsImageRenderer(size: CGSize(width: 24, height: 24)).image { contexto in
            UIColor(argb: argb).setFill()
            contexto.cgContext.fillEllipse(in: CGRect(x: 2, y: 2, width: 20, height: 20))
        }
    }
    @objc private func aplicar() {
        AjustesBorradorAvanzadoIPad.actual = ajustes
        let accion = alAplicar
        let valor = ajustes
        dismiss(animated: true) { accion?(valor) }
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
