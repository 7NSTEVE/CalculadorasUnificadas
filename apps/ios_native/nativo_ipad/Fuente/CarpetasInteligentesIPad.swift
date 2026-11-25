import UIKit

// MARK: - Modelo

enum TipoReglaFiltroIPad: String, Codable, CaseIterable {
    case curso
    case etiqueta
    case fechaDespuesDe
    case fechaAntesDe
    case tipoContenido     // TipoBloqueLienzo
    case noRevisado
    case conEstrella
    case sinCurso
    case sinEtiqueta
    case conAudio
    case conCircuito
    case conError
    case porBuscar

    var nombre: String {
        switch self {
        case .curso:            return "Pertenece a curso"
        case .etiqueta:         return "Tiene etiqueta"
        case .fechaDespuesDe:   return "Creado después de"
        case .fechaAntesDe:     return "Creado antes de"
        case .tipoContenido:    return "Contiene tipo de bloque"
        case .noRevisado:       return "Material no revisado"
        case .conEstrella:      return "Marcado con estrella"
        case .sinCurso:         return "Sin curso asignado"
        case .sinEtiqueta:      return "Sin etiquetas"
        case .conAudio:         return "Con grabación de audio"
        case .conCircuito:      return "Con circuito"
        case .conError:         return "Con errores registrados"
        case .porBuscar:        return "Coincide con búsqueda"
        }
    }
}

struct ReglaFiltroIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var tipo: TipoReglaFiltroIPad
    var valor: String = ""    // Interpretación depende de tipo
    var negada = false        // Invertir: "NO pertenece a curso X"
}

struct CarpetaInteligenteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var icono: String = "folder.badge.gearshape"
    var colorHex: UInt32 = 0xFF007AFF
    var reglas: [ReglaFiltroIPad] = []
    var operadorY = true // true = AND (todas), false = OR (alguna)
    var orden: Int = 0
    var creadaAutomaticamente = false

    var color: UIColor {
        UIColor(
            red:   CGFloat((colorHex >> 16) & 0xFF) / 255,
            green: CGFloat((colorHex >> 8) & 0xFF) / 255,
            blue:  CGFloat(colorHex & 0xFF) / 255,
            alpha: 1)
    }
}

// MARK: - Documento huérfano

struct DocumentoHuerfanoIPad: Identifiable {
    var id: UInt64
    var titulo: String
    var razon: String   // "Sin curso", "Sin etiquetas", "Nunca accedido", etc.
    var ultimoAcceso: Date?
}

// MARK: - Repositorio

final class RepositorioCarpetasInteligentesIPad {
    static let compartido = RepositorioCarpetasInteligentesIPad()
    private let cola = DispatchQueue(label: "tinta.carpetas.inteligentes")
    private let url: URL
    private(set) var carpetas: [CarpetaInteligenteIPad] = []

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpetaDir = soporte.appendingPathComponent("Tinta/CarpetasInteligentes",
                                                         isDirectory: true)
        try? FileManager.default.createDirectory(at: carpetaDir,
                                                 withIntermediateDirectories: true)
        url = carpetaDir.appendingPathComponent("carpetas.json")
        carpetas = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode([CarpetaInteligenteIPad].self, from: $0)
        } ?? []
        if carpetas.isEmpty { generarPorDefecto() }
    }

    func agregar(_ carpeta: CarpetaInteligenteIPad) {
        cola.sync {
            carpetas.append(carpeta); persistir()
        }
    }

    func actualizar(_ carpeta: CarpetaInteligenteIPad) {
        cola.sync {
            if let i = carpetas.firstIndex(where: { $0.id == carpeta.id }) {
                carpetas[i] = carpeta
            }
            persistir()
        }
    }

    func eliminar(_ id: UUID) {
        cola.sync {
            carpetas.removeAll { $0.id == id }; persistir()
        }
    }

    func reordenar(_ indices: [UUID]) {
        cola.sync {
            for (orden, id) in indices.enumerated() {
                if let i = carpetas.firstIndex(where: { $0.id == id }) {
                    carpetas[i].orden = orden
                }
            }
            carpetas.sort { $0.orden < $1.orden }
            persistir()
        }
    }

    // MARK: Carpetas automáticas

    func carpetasAutomaticas(cursos: [String]) -> [CarpetaInteligenteIPad] {
        var auto: [CarpetaInteligenteIPad] = []

        // Una carpeta por curso
        for curso in cursos {
            auto.append(CarpetaInteligenteIPad(
                nombre: "📚 \(curso)",
                icono: "book.closed",
                reglas: [ReglaFiltroIPad(tipo: .curso, valor: curso)],
                creadaAutomaticamente: true))
        }

        // Carpetas especiales
        auto.append(CarpetaInteligenteIPad(
            nombre: "⚠️ No revisado",
            icono: "exclamationmark.triangle",
            colorHex: 0xFFFF9500,
            reglas: [ReglaFiltroIPad(tipo: .noRevisado)],
            creadaAutomaticamente: true))

        auto.append(CarpetaInteligenteIPad(
            nombre: "⭐ Favoritos",
            icono: "star.fill",
            colorHex: 0xFFFFCC00,
            reglas: [ReglaFiltroIPad(tipo: .conEstrella)],
            creadaAutomaticamente: true))

        auto.append(CarpetaInteligenteIPad(
            nombre: "🎵 Con audio",
            icono: "waveform",
            reglas: [ReglaFiltroIPad(tipo: .conAudio)],
            creadaAutomaticamente: true))

        auto.append(CarpetaInteligenteIPad(
            nombre: "⚡ Con circuitos",
            icono: "resistor",
            colorHex: 0xFF5856D6,
            reglas: [ReglaFiltroIPad(tipo: .conCircuito)],
            creadaAutomaticamente: true))

        auto.append(CarpetaInteligenteIPad(
            nombre: "❌ Con errores",
            icono: "xmark.circle",
            colorHex: 0xFFFF3B30,
            reglas: [ReglaFiltroIPad(tipo: .conError)],
            creadaAutomaticamente: true))

        auto.append(CarpetaInteligenteIPad(
            nombre: "🗂 Sin clasificar",
            icono: "questionmark.folder",
            colorHex: 0xFF8E8E93,
            reglas: [
                ReglaFiltroIPad(tipo: .sinCurso),
                ReglaFiltroIPad(tipo: .sinEtiqueta),
            ],
            operadorY: false,
            creadaAutomaticamente: true))

        return auto
    }

    // MARK: Detección de huérfanos

    struct InfoDocumento {
        var id: UInt64; var titulo: String; var cursoId: String?
        var etiquetas: [String]; var ultimoAcceso: Date?
        var tieneAudio: Bool; var referenciado: Bool
    }

    func detectarHuerfanos(documentos: [InfoDocumento]) -> [DocumentoHuerfanoIPad] {
        var resultado: [DocumentoHuerfanoIPad] = []

        for doc in documentos {
            var razones: [String] = []

            if doc.cursoId == nil || doc.cursoId?.isEmpty == true {
                razones.append("Sin curso asignado")
            }
            if doc.etiquetas.isEmpty {
                razones.append("Sin etiquetas")
            }
            if !doc.referenciado {
                razones.append("No referenciado por otros documentos")
            }
            if let acceso = doc.ultimoAcceso,
               Date().timeIntervalSince(acceso) > 90 * 24 * 3600 {
                razones.append("Sin acceso en +90 días")
            }

            if !razones.isEmpty {
                resultado.append(DocumentoHuerfanoIPad(
                    id: doc.id, titulo: doc.titulo,
                    razon: razones.joined(separator: " · "),
                    ultimoAcceso: doc.ultimoAcceso))
            }
        }

        return resultado.sorted { ($0.ultimoAcceso ?? .distantPast) <
            ($1.ultimoAcceso ?? .distantPast) }
    }

    // MARK: Privado

    private func generarPorDefecto() {
        carpetas = carpetasAutomaticas(cursos: [])
        persistir()
    }

    private func persistir() {
        guard let datos = try? JSONEncoder().encode(carpetas) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

// MARK: - Panel de carpetas inteligentes

final class PanelCarpetasInteligentesIPad: UITableViewController {

    var alSeleccionarCarpeta: ((CarpetaInteligenteIPad) -> Void)?
    private let repo = RepositorioCarpetasInteligentesIPad.compartido

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Carpetas inteligentes"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self,
            action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(nueva))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Mis carpetas" : "Automáticas"
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        let manuales = repo.carpetas.filter { !$0.creadaAutomaticamente }
        let auto = repo.carpetas.filter { $0.creadaAutomaticamente }
        return section == 0 ? manuales.count : auto.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = tableView.dequeueReusableCell(withIdentifier: "celda")
            ?? UITableViewCell(style: .subtitle, reuseIdentifier: "celda")
        let lista = indexPath.section == 0
            ? repo.carpetas.filter { !$0.creadaAutomaticamente }
            : repo.carpetas.filter { $0.creadaAutomaticamente }
        guard indexPath.row < lista.count else { return celda }
        let carpeta = lista[indexPath.row]
        celda.imageView?.image = UIImage(systemName: carpeta.icono)
        celda.imageView?.tintColor = carpeta.color
        celda.textLabel?.text = carpeta.nombre
        celda.detailTextLabel?.text = "\(carpeta.reglas.count) regla(s)"
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let lista = indexPath.section == 0
            ? repo.carpetas.filter { !$0.creadaAutomaticamente }
            : repo.carpetas.filter { $0.creadaAutomaticamente }
        guard indexPath.row < lista.count else { return }
        let carpeta = lista[indexPath.row]
        if let alSeleccionarCarpeta {
            alSeleccionarCarpeta(carpeta)
        } else {
            presentarDetalle(carpeta)
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
        let manuales = repo.carpetas.filter { !$0.creadaAutomaticamente }
        guard indexPath.row < manuales.count else { return }
        repo.eliminar(manuales[indexPath.row].id)
        tableView.deleteRows(at: [indexPath], with: .automatic)
    }

    @objc private func nueva() {
        let alerta = UIAlertController(title: "Nueva carpeta inteligente",
                                       message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nombre" }
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let nombre = alerta.textFields?.first?.text,
                  !nombre.isEmpty else { return }
            var carpeta = CarpetaInteligenteIPad(nombre: nombre)
            carpeta.orden = (self?.repo.carpetas.count ?? 0) + 1
            self?.repo.agregar(carpeta)
            self?.tableView.reloadData()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }

    private func presentarDetalle(_ carpeta: CarpetaInteligenteIPad) {
        let reglas = carpeta.reglas.map { regla -> String in
            let valor = regla.valor.isEmpty ? "" : ": \(regla.valor)"
            return "• \(regla.negada ? "NO " : "")\(regla.tipo.nombre)\(valor)"
        }
        let mensaje = reglas.isEmpty
            ? "Esta carpeta todavía no tiene reglas."
            : "Coincidencia \(carpeta.operadorY ? "con todas" : "con cualquiera"):\n"
                + reglas.joined(separator: "\n")
        let alerta = UIAlertController(
            title: carpeta.nombre, message: mensaje, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        present(alerta, animated: true)
    }
}

// MARK: - Panel de detección de huérfanos

final class PanelHuerfanosIPad: UITableViewController {
    var alSeleccionarDocumento: ((UInt64) -> Void)?
    private var huerfanos: [DocumentoHuerfanoIPad] = []
    private let formateadorFecha: DateFormatter = {
        let f = DateFormatter()
        f.dateStyle = .medium; f.timeStyle = .short
        return f
    }()

    init(huerfanos: [DocumentoHuerfanoIPad]) {
        self.huerfanos = huerfanos
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Documentos huérfanos (\(huerfanos.count))"
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "celda")
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        huerfanos.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle,
                                    reuseIdentifier: "celda")
        let h = huerfanos[indexPath.row]
        celda.textLabel?.text = h.titulo
        var detalle = h.razon
        if let acceso = h.ultimoAcceso {
            detalle += " · Último acceso: \(formateadorFecha.string(from: acceso))"
        }
        celda.detailTextLabel?.text = detalle
        celda.detailTextLabel?.textColor = .secondaryLabel
        celda.imageView?.image = UIImage(systemName: "questionmark.folder")
        celda.imageView?.tintColor = .systemOrange
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        alSeleccionarDocumento?(huerfanos[indexPath.row].id)
    }
}
