import UIKit

enum TexturaPluma: Int, Codable, CaseIterable {
    case solida, lapiz, tintaSeca, marcador
    var nombre: String {
        switch self { case .solida: return "Sólida"; case .lapiz: return "Lápiz"
        case .tintaSeca: return "Tinta seca"; case .marcador: return "Marcador" }
    }
}

enum NivelRefinamientoTinta: Int, Codable, CaseIterable {
    case original, suavizado, legibilidad, limpio
    var nombre: String {
        switch self { case .original: return "0 — Original"; case .suavizado: return "1 — Suavizado"
        case .legibilidad: return "2 — Legibilidad"; case .limpio: return "3 — Limpio" }
    }
}

struct AjustesPlumaAvanzada: Codable, Equatable {
    var opacidad: Float = 1
    var respuestaPresion: Float = 0.7
    var anchoMinimo: Float = 0.25
    var respuestaVelocidad: Float = 3
    var suavizadoAncho = 4
    var estabilizacion: Float = 0
    var taperInicio: Float = 0
    var taperFin: Float = 0
    var inclinacion: Float = 0
    var textura: TexturaPluma = .solida
    var refinamiento: NivelRefinamientoTinta = .original

    var puente: TintaAjustesPluma {
        TintaAjustesPluma(opacidad: opacidad, exponente_presion: respuestaPresion,
                          factor_ancho_minimo: anchoMinimo,
                          velocidad_ancho_minimo: respuestaVelocidad,
                          suavizado_ancho: Int32(suavizadoAncho),
                          estabilizacion: estabilizacion, taper_inicio: taperInicio,
                          taper_fin: taperFin, sensibilidad_inclinacion: inclinacion,
                          textura: Int32(textura.rawValue),
                          nivel_refinado: Int32(refinamiento.rawValue))
    }
}

struct PlumaMateria: Codable, Identifiable, Equatable {
    var id = UUID(); var nombre: String; var colorARGB: UInt32; var grosor: Float
    var ajustes: AjustesPlumaAvanzada; var resaltador = false
}

struct PerfilPlumasMateria: Codable, Identifiable, Equatable {
    var id = UUID(); var materia: String; var plumas: [PlumaMateria]

    static let iniciales: [PerfilPlumasMateria] = [
        PerfilPlumasMateria(materia: "Circuitos", plumas: [
            PlumaMateria(nombre: "Cálculos", colorARGB: 0xFF1A1A1A, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.2)),
            PlumaMateria(nombre: "Corriente", colorARGB: 0xFFC62828, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.2)),
            PlumaMateria(nombre: "Voltaje", colorARGB: 0xFF1B5FA8, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.2)),
            PlumaMateria(nombre: "Resultados", colorARGB: 0xFF2E7D32, grosor: 5,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.28,
                                                      refinamiento: .suavizado))
        ]),
        PerfilPlumasMateria(materia: "Física", plumas: [
            PlumaMateria(nombre: "Desarrollo", colorARGB: 0xFF202124, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.18)),
            PlumaMateria(nombre: "Vectores", colorARGB: 0xFF1565C0, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(taperFin: 0.12)),
            PlumaMateria(nombre: "Fuerzas", colorARGB: 0xFFD32F2F, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(taperFin: 0.12))
        ]),
        PerfilPlumasMateria(materia: "Matemáticas", plumas: [
            PlumaMateria(nombre: "Escritura", colorARGB: 0xFF161616, grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.3,
                                                      refinamiento: .suavizado)),
            PlumaMateria(nombre: "Resultado", colorARGB: 0xFF2E7D32, grosor: 5,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.35,
                                                      refinamiento: .legibilidad))
        ]),
        PerfilPlumasMateria(materia: "Clase", plumas: [
            PlumaMateria(nombre: "Explicación", colorARGB: 0xFF1B5FA8,
                         grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.18)),
            PlumaMateria(nombre: "Importancia", colorARGB: 0xFFC62828,
                         grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.18)),
            PlumaMateria(nombre: "Ejemplo", colorARGB: 0xFF2E7D32,
                         grosor: 4,
                         ajustes: AjustesPlumaAvanzada(estabilizacion: 0.18)),
            PlumaMateria(nombre: "Resaltado", colorARGB: 0xFFF9A825,
                         grosor: 16,
                         ajustes: AjustesPlumaAvanzada(opacidad: 0.62,
                                                      textura: .marcador),
                         resaltador: true)
        ])
    ]
}

final class RepositorioPlumas {
    static let compartido = RepositorioPlumas()
    private let url: URL
    private(set) var perfiles: [PerfilPlumasMateria]
    private init() {
        let base = FileManager.default.urls(for: .documentDirectory,
                                            in: .userDomainMask).first!
        let carpeta = base.appendingPathComponent("Preferencias", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("plumas-por-materia.json")
        if let datos = try? Data(contentsOf: url),
           let leidos = try? JSONDecoder().decode([PerfilPlumasMateria].self, from: datos) {
            perfiles = leidos
        } else { perfiles = PerfilPlumasMateria.iniciales }
        for inicial in PerfilPlumasMateria.iniciales
        where !perfiles.contains(where: {
            Normalizador.texto($0.materia) == Normalizador.texto(inicial.materia)
        }) {
            perfiles.append(inicial)
        }
        persistir()
    }
    func guardar(_ perfiles: [PerfilPlumasMateria]) {
        self.perfiles = perfiles
        persistir()
    }
    func guardarFavorita(_ pluma: PlumaMateria) {
        if let indice = perfiles.firstIndex(where: { $0.materia == "Favoritas" }) {
            perfiles[indice].plumas.removeAll { $0.nombre == pluma.nombre }
            perfiles[indice].plumas.append(pluma)
        } else {
            perfiles.append(PerfilPlumasMateria(
                materia: "Favoritas", plumas: [pluma]))
        }
        persistir()
    }
    func guardarEnPaleta(_ pluma: PlumaMateria, materia: String) {
        let nombre = materia.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !nombre.isEmpty else { return }
        if let indice = perfiles.firstIndex(where: {
            Normalizador.texto($0.materia) == Normalizador.texto(nombre)
        }) {
            perfiles[indice].plumas.removeAll { $0.nombre == pluma.nombre }
            perfiles[indice].plumas.append(pluma)
        } else {
            perfiles.append(PerfilPlumasMateria(materia: nombre,
                                                plumas: [pluma]))
        }
        persistir()
    }
    func eliminarFavorita(_ id: UUID) {
        guard let indice = perfiles.firstIndex(where: {
            $0.materia == "Favoritas"
        }) else { return }
        perfiles[indice].plumas.removeAll { $0.id == id }
        if perfiles[indice].plumas.isEmpty { perfiles.remove(at: indice) }
        persistir()
    }
    private func persistir() {
        if let datos = try? JSONEncoder().encode(perfiles) {
            try? datos.write(to: url,
                             options: [.atomic, .completeFileProtection])
        }
    }
}

final class PanelPlumaAvanzada: UITableViewController {
    var alAplicar: ((PlumaMateria) -> Void)?
    var alRefinarTodos: ((NivelRefinamientoTinta) -> Void)?
    private var ajustes: AjustesPlumaAvanzada
    private var color: UInt32; private var grosor: Float
    private var resaltador: Bool
    private let nombres = ["Opacidad", "Respuesta a presión", "Ancho mínimo",
                           "Respuesta a velocidad", "Suavizado", "Estabilización",
                           "Taper al inicio", "Taper al final", "Sensibilidad a inclinación"]

    init(ajustes: AjustesPlumaAvanzada, color: UInt32, grosor: Float,
         resaltador: Bool) {
        self.ajustes = ajustes; self.color = color; self.grosor = grosor
        self.resaltador = resaltador
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Pluma avanzada"
        navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .close,
                                                            target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Aplicar", style: .done,
                            target: self, action: #selector(aplicar)),
            UIBarButtonItem(image: UIImage(systemName: "star"), style: .plain,
                            target: self, action: #selector(guardarFavorita)),
            UIBarButtonItem(title: "Paleta", style: .plain,
                            target: self, action: #selector(guardarEnPaleta)),
        ]
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 4 }
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        ["Presets por materia", "Comportamiento de la tinta", "Textura y refinamiento",
         "Corrección no destructiva existente"][section]
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch section { case 0: return RepositorioPlumas.compartido.perfiles.flatMap(\.plumas).count
        case 1: return nombres.count; case 2: return 2; default: return NivelRefinamientoTinta.allCases.count }
    }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let c = UITableViewCell(style: .value1, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let entradas = presets; let item = entradas[indexPath.row]
            c.textLabel?.text = "\(item.0) · \(item.1.nombre)"; c.detailTextLabel?.text = "\(item.1.grosor) pt"
            c.imageView?.image = circulo(UIColor(argb: item.1.colorARGB))
        } else if indexPath.section == 1 {
            c.textLabel?.text = nombres[indexPath.row]; c.detailTextLabel?.text = valor(indexPath.row)
            c.accessoryType = .disclosureIndicator
        } else if indexPath.section == 2 {
            c.textLabel?.text = indexPath.row == 0 ? "Textura" : "Nivel para trazos nuevos"
            c.detailTextLabel?.text = indexPath.row == 0 ? ajustes.textura.nombre : ajustes.refinamiento.nombre
            c.accessoryType = .disclosureIndicator
        } else {
            let nivel = NivelRefinamientoTinta.allCases[indexPath.row]
            c.textLabel?.text = nivel.nombre
            c.detailTextLabel?.text = nivel == .original ? "restaura geometría original" : "no cambia muestras"
        }
        return c
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 0 { let p = presets[indexPath.row].1; ajustes = p.ajustes
            color = p.colorARGB; grosor = p.grosor; resaltador = p.resaltador
            tableView.reloadData(); alAplicar?(p) }
        else if indexPath.section == 1 { editarNumero(indexPath.row) }
        else if indexPath.section == 2 { elegirEnum(indexPath.row) }
        else { alRefinarTodos?(NivelRefinamientoTinta.allCases[indexPath.row]) }
    }
    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard indexPath.section == 0 else { return nil }
        let entrada = presets[indexPath.row]
        guard entrada.0 == "Favoritas" else { return nil }
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") {
            _, _, terminar in
            RepositorioPlumas.compartido.eliminarFavorita(entrada.1.id)
            tableView.reloadData(); terminar(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar])
    }
    private var presets: [(String, PlumaMateria)] {
        RepositorioPlumas.compartido.perfiles.flatMap { perfil in perfil.plumas.map { (perfil.materia, $0) } }
    }
    private func valor(_ i: Int) -> String {
        let v: Float = [ajustes.opacidad, ajustes.respuestaPresion, ajustes.anchoMinimo,
                        ajustes.respuestaVelocidad, Float(ajustes.suavizadoAncho),
                        ajustes.estabilizacion, ajustes.taperInicio, ajustes.taperFin,
                        ajustes.inclinacion][i]
        return String(format: "%.3g", v)
    }
    private func editarNumero(_ i: Int) {
        let a = UIAlertController(title: nombres[i], message: nil, preferredStyle: .alert)
        a.addTextField { $0.text = self.valor(i); $0.keyboardType = .decimalPad }
        a.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self, let texto = a.textFields?.first?.text?.replacingOccurrences(of: ",", with: "."),
                  let v = Float(texto) else { return }
            switch i { case 0: self.ajustes.opacidad = min(max(v,0),1)
            case 1: self.ajustes.respuestaPresion = min(max(v,0.1),4)
            case 2: self.ajustes.anchoMinimo = min(max(v,0),1)
            case 3: self.ajustes.respuestaVelocidad = min(max(v,0.1),20)
            case 4: self.ajustes.suavizadoAncho = min(max(Int(v),0),24)
            case 5: self.ajustes.estabilizacion = min(max(v,0),1)
            case 6: self.ajustes.taperInicio = min(max(v,0),0.5)
            case 7: self.ajustes.taperFin = min(max(v,0),0.5)
            default: self.ajustes.inclinacion = min(max(v,0),1) }
            self.tableView.reloadData()
        }); a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }
    private func elegirEnum(_ fila: Int) {
        let a = UIAlertController(title: fila == 0 ? "Textura" : "Refinamiento",
                                  message: nil, preferredStyle: .actionSheet)
        if fila == 0 { for t in TexturaPluma.allCases { a.addAction(UIAlertAction(title: t.nombre,
            style: .default) { [weak self] _ in self?.ajustes.textura = t; self?.tableView.reloadData() }) } }
        else { for n in NivelRefinamientoTinta.allCases { a.addAction(UIAlertAction(title: n.nombre,
            style: .default) { [weak self] _ in self?.ajustes.refinamiento = n; self?.tableView.reloadData() }) } }
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); a.popoverPresentationController?.sourceView = view
        present(a, animated: true)
    }
    private func circulo(_ color: UIColor) -> UIImage {
        UIGraphicsImageRenderer(size: CGSize(width: 24, height: 24)).image { ctx in
            color.setFill(); ctx.cgContext.fillEllipse(in: CGRect(x: 2, y: 2, width: 20, height: 20)) }
    }
    @objc private func aplicar() { alAplicar?(PlumaMateria(nombre: "Personalizada",
        colorARGB: color, grosor: grosor, ajustes: ajustes,
        resaltador: resaltador)); dismiss(animated: true) }
    @objc private func guardarFavorita() {
        let alerta = UIAlertController(
            title: "Favorite stroke",
            message: "Guarda herramienta, color, grosor, presión, textura y refinamiento.",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nombre del trazo favorito" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self,
                  let nombre = alerta.textFields?.first?.text?.trimmingCharacters(
                    in: .whitespacesAndNewlines), !nombre.isEmpty else { return }
            let pluma = PlumaMateria(
                nombre: nombre, colorARGB: self.color,
                grosor: self.grosor, ajustes: self.ajustes,
                resaltador: self.resaltador)
            RepositorioPlumas.compartido.guardarFavorita(pluma)
            self.tableView.reloadData()
        })
        present(alerta, animated: true)
    }
    @objc private func guardarEnPaleta() {
        let hoja = UIAlertController(
            title: "Lecture palette",
            message: "Agregar el trazo completo a una materia o crear una paleta nueva.",
            preferredStyle: .actionSheet)
        for perfil in RepositorioPlumas.compartido.perfiles
        where perfil.materia != "Favoritas" {
            hoja.addAction(UIAlertAction(title: perfil.materia, style: .default) {
                [weak self] _ in self?.pedirNombrePaleta(perfil.materia)
            })
        }
        hoja.addAction(UIAlertAction(title: "Nueva materia…", style: .default) {
            [weak self] _ in self?.pedirMateriaNueva()
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func pedirNombrePaleta(_ materia: String) {
        let alerta = UIAlertController(title: materia, message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nombre: explicación, corriente…" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self,
                  let nombre = alerta.textFields?.first?.text?.trimmingCharacters(
                    in: .whitespacesAndNewlines), !nombre.isEmpty else { return }
            RepositorioPlumas.compartido.guardarEnPaleta(
                PlumaMateria(nombre: nombre, colorARGB: self.color,
                             grosor: self.grosor, ajustes: self.ajustes,
                             resaltador: self.resaltador),
                materia: materia)
            self.tableView.reloadData()
        })
        present(alerta, animated: true)
    }

    private func pedirMateriaNueva() {
        let alerta = UIAlertController(title: "Nueva paleta de curso",
                                       message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Curso o materia" }
        alerta.addTextField { $0.placeholder = "Nombre del trazo" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let self,
                  let materia = alerta.textFields?[0].text,
                  let nombre = alerta.textFields?[1].text,
                  !materia.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
                  !nombre.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            else { return }
            RepositorioPlumas.compartido.guardarEnPaleta(
                PlumaMateria(nombre: nombre, colorARGB: self.color,
                             grosor: self.grosor, ajustes: self.ajustes,
                             resaltador: self.resaltador),
                materia: materia)
            self.tableView.reloadData()
        })
        present(alerta, animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
