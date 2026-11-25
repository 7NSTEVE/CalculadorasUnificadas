import Foundation
import UIKit

enum PasoOnboardingCuadernUIPad: String, Codable, CaseIterable {
    case cuaderno
    case buscar
    case importar
    case foco

    var titulo: String {
        switch self {
        case .cuaderno: return "Abra o cree un cuaderno"
        case .buscar: return "Encuentre cualquier cosa"
        case .importar: return "Importe material de clase"
        case .foco: return "Pruebe el modo foco"
        }
    }
    var detalle: String {
        switch self {
        case .cuaderno: return "Empiece con un documento real; no se crea contenido de demostración que luego haya que borrar."
        case .buscar: return "Use la búsqueda universal para problemas, texto, fórmulas, comentarios y más."
        case .importar: return "Abra PDF, material del profesor o Study Sets desde sus archivos."
        case .foco: return "Inicie una sesión corta y confirme que el temporizador se recupera al volver."
        }
    }
    var simbolo: String {
        switch self {
        case .cuaderno: return "book.closed.fill"
        case .buscar: return "magnifyingglass"
        case .importar: return "square.and.arrow.down"
        case .foco: return "timer"
        }
    }
}

private struct ArchivoOnboardingCuadernUIPad: Codable {
    var versionFormato = 1
    var versionProducto = 1
    var completados: Set<PasoOnboardingCuadernUIPad> = []
    var descartado = false
    var actualizado = Date()
}

final class RepositorioOnboardingCuadernUIPad {
    static let compartido = RepositorioOnboardingCuadernUIPad()
    static let versionActual = 1
    private let bloqueo = NSLock(); private let url: URL
    private var estado: ArchivoOnboardingCuadernUIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Onboarding", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("estado.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(ArchivoOnboardingCuadernUIPad.self, from: datos),
           leido.versionProducto == Self.versionActual {
            estado = leido
        } else { estado = ArchivoOnboardingCuadernUIPad(versionProducto: Self.versionActual) }
    }

    func completados() -> Set<PasoOnboardingCuadernUIPad> {
        bloqueo.lock(); defer { bloqueo.unlock() }; return estado.completados
    }
    var debeMostrarse: Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return !estado.descartado && estado.completados.count < PasoOnboardingCuadernUIPad.allCases.count
    }
    var completado: Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.completados.count == PasoOnboardingCuadernUIPad.allCases.count
    }

    func marcar(_ paso: PasoOnboardingCuadernUIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.completados.insert(paso); estado.actualizado = Date(); guardarSinBloqueo()
    }
    func descartar() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.descartado = true; estado.actualizado = Date(); guardarSinBloqueo()
    }
    func reiniciar() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado = ArchivoOnboardingCuadernUIPad(versionProducto: Self.versionActual); guardarSinBloqueo()
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601; encoder.outputFormatting = [.sortedKeys]
        if let datos = try? encoder.encode(estado) { try? datos.write(to: url, options: [.atomic, .completeFileProtection]) }
    }
}

final class PanelOnboardingCuadernUIPad: UITableViewController {
    var alEjecutarPaso: ((PasoOnboardingCuadernUIPad, @escaping (Bool) -> Void) -> Void)?
    var alTerminar: (() -> Void)?
    private let repo: RepositorioOnboardingCuadernUIPad
    private var completados = Set<PasoOnboardingCuadernUIPad>()

    init(repo: RepositorioOnboardingCuadernUIPad = .compartido) {
        self.repo = repo; super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Conozca CuadernU"
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Omitir", style: .plain,
                                                            target: self, action: #selector(omitir))
        recargar()
    }

    private func recargar() {
        completados = repo.completados(); tableView.reloadData()
        if repo.completado { alTerminar?() }
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        PasoOnboardingCuadernUIPad.allCases.count
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let paso = PasoOnboardingCuadernUIPad.allCases[indexPath.row]
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        cell.textLabel?.text = paso.titulo; cell.detailTextLabel?.text = paso.detalle
        cell.detailTextLabel?.numberOfLines = 3
        cell.imageView?.image = UIImage(systemName: completados.contains(paso) ? "checkmark.circle.fill" : paso.simbolo)
        cell.accessoryType = completados.contains(paso) ? .checkmark : .disclosureIndicator
        cell.accessibilityLabel = paso.titulo
        cell.accessibilityValue = completados.contains(paso) ? "Completado" : "Pendiente"
        return cell
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let paso = PasoOnboardingCuadernUIPad.allCases[indexPath.row]
        guard !completados.contains(paso) else { return }
        guard let alEjecutarPaso else {
            let alerta = UIAlertController(title: "Acción no integrada",
                message: "Este paso necesita conectarse a la acción real del shell; no se marcará como completado de forma ficticia.",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel)); present(alerta, animated: true); return
        }
        alEjecutarPaso(paso) { [weak self] exito in
            guard let self, exito else { return }
            self.repo.marcar(paso); self.recargar()
            AnunciadorAccesibilidadIPad.anunciar("Paso completado: \(paso.titulo)")
        }
    }

    @objc private func omitir() {
        repo.descartar(); alTerminar?(); dismiss(animated: true)
    }
}
