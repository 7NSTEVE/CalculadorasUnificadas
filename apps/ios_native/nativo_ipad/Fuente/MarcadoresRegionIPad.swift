import Foundation
import UIKit

struct MarcadorRegionIPad: Codable, Equatable, Identifiable {
    var id = UUID()
    var documentoId: UInt64
    var titulo: String
    var x: Double
    var y: Double
    var zoom: Double
    var creado = Date()
    var actualizado = Date()

    var destino: DestinoEnlaceTinta {
        DestinoEnlaceTinta(
            documentoId: documentoId,
            elementoId: nil,
            bloqueId: nil,
            x: x,
            y: y,
            zoom: zoom,
            segundoAudio: nil,
            titulo: titulo)
    }
}

private struct ArchivoMarcadoresRegionIPad: Codable {
    var version = 1
    var marcadores: [MarcadorRegionIPad] = []
}

enum ErrorMarcadoresRegionIPad: LocalizedError {
    case demasiadosMarcadores
    case marcadorInvalido
    case persistencia(String)

    var errorDescription: String? {
        switch self {
        case .demasiadosMarcadores:
            return "Se alcanzó el límite seguro de marcadores de región."
        case .marcadorInvalido:
            return "El marcador contiene una posición o zoom inválidos."
        case .persistencia(let detalle):
            return "No se pudo guardar el marcador: \(detalle)"
        }
    }
}

final class RepositorioMarcadoresRegionIPad: @unchecked Sendable {
    static let compartido = RepositorioMarcadoresRegionIPad()
    static let maximoMarcadores = 10_000

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoMarcadoresRegionIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask)[0]
            .appendingPathComponent("Tinta/MarcadoresRegion", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("marcadores-v1.json")
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url, options: .mappedIfSafe),
           let leido = try? decoder.decode(
                ArchivoMarcadoresRegionIPad.self, from: datos),
           leido.version == 1,
           (try? Self.validar(leido.marcadores)) != nil {
            archivo = leido
        } else {
            archivo = .init()
        }
    }

    func todos(documentoId: UInt64? = nil) -> [MarcadorRegionIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let valores = documentoId.map { id in
            archivo.marcadores.filter { $0.documentoId == id }
        } ?? archivo.marcadores
        return valores.sorted { $0.actualizado > $1.actualizado }
    }

    func cercano(
        documentoId: UInt64,
        x: Double,
        y: Double,
        radio: Double
    ) -> MarcadorRegionIPad? {
        guard x.isFinite, y.isFinite, radio.isFinite, radio >= 0 else { return nil }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let radio2 = radio * radio
        return archivo.marcadores
            .filter { $0.documentoId == documentoId }
            .min {
                distancia2($0, x: x, y: y) < distancia2($1, x: x, y: y)
            }
            .flatMap { distancia2($0, x: x, y: y) <= radio2 ? $0 : nil }
    }

    @discardableResult
    func guardar(_ marcador: MarcadorRegionIPad) throws -> MarcadorRegionIPad {
        try Self.validar([marcador])
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        if let indice = candidato.marcadores.firstIndex(where: { $0.id == marcador.id }) {
            candidato.marcadores[indice] = marcador
        } else {
            guard candidato.marcadores.count < Self.maximoMarcadores else {
                throw ErrorMarcadoresRegionIPad.demasiadosMarcadores
            }
            candidato.marcadores.append(marcador)
        }
        try Self.validar(candidato.marcadores)
        try persistir(candidato)
        archivo = candidato
        return marcador
    }

    func eliminar(_ id: UUID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        candidato.marcadores.removeAll { $0.id == id }
        try persistir(candidato)
        archivo = candidato
    }

    func eliminarDocumento(_ documentoId: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var candidato = archivo
        candidato.marcadores.removeAll { $0.documentoId == documentoId }
        try persistir(candidato)
        archivo = candidato
    }

    private func persistir(_ valor: ArchivoMarcadoresRegionIPad) throws {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        do {
            try encoder.encode(valor).write(
                to: url,
                options: [.atomic, .completeFileProtection])
        } catch {
            throw ErrorMarcadoresRegionIPad.persistencia(
                error.localizedDescription)
        }
    }

    private func distancia2(
        _ marcador: MarcadorRegionIPad,
        x: Double,
        y: Double
    ) -> Double {
        let dx = marcador.x - x
        let dy = marcador.y - y
        return dx * dx + dy * dy
    }

    private static func validar(_ valores: [MarcadorRegionIPad]) throws {
        guard valores.count <= maximoMarcadores else {
            throw ErrorMarcadoresRegionIPad.demasiadosMarcadores
        }
        guard Set(valores.map(\.id)).count == valores.count else {
            throw ErrorMarcadoresRegionIPad.marcadorInvalido
        }
        for valor in valores {
            guard valor.x.isFinite,
                  valor.y.isFinite,
                  valor.zoom.isFinite,
                  valor.zoom > 0,
                  valor.zoom <= 20,
                  !valor.titulo.trimmingCharacters(
                    in: .whitespacesAndNewlines).isEmpty,
                  valor.titulo.count <= 240 else {
                throw ErrorMarcadoresRegionIPad.marcadorInvalido
            }
        }
    }
}

@MainActor
final class PanelMarcadoresRegionIPad: UITableViewController {
    private let repositorio: RepositorioMarcadoresRegionIPad
    private var marcadores: [MarcadorRegionIPad] = []

    init(repositorio: RepositorioMarcadoresRegionIPad = .compartido) {
        self.repositorio = repositorio
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Marcadores"
        view.accessibilityIdentifier = "marcadores.region.panel"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))
        recargar()
    }

    private func recargar() {
        marcadores = repositorio.todos()
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 1 }

    override func tableView(
        _ tableView: UITableView,
        numberOfRowsInSection section: Int
    ) -> Int {
        max(marcadores.count, 1)
    }

    override func tableView(
        _ tableView: UITableView,
        titleForFooterInSection section: Int
    ) -> String? {
        "Cada marcador conserva documento, posición y zoom. Puede abrir zonas exactas incluso en otro cuaderno."
    }

    override func tableView(
        _ tableView: UITableView,
        cellForRowAt indexPath: IndexPath
    ) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard marcadores.indices.contains(indexPath.row) else {
            celda.textLabel?.text = "Aún no hay marcadores"
            celda.detailTextLabel?.text =
                "Use el icono de marcador en el toolbar para guardar la zona visible."
            celda.imageView?.image = UIImage(systemName: "bookmark")
            celda.selectionStyle = .none
            return celda
        }
        let marcador = marcadores[indexPath.row]
        let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: marcador.documentoId)
        celda.textLabel?.text = marcador.titulo
        celda.detailTextLabel?.text = [
            ubicacion?.titulo ?? "Documento \(marcador.documentoId)",
            "x \(Int(marcador.x)) · y \(Int(marcador.y)) · \(String(format: "%.2f", marcador.zoom))×",
        ].joined(separator: " · ")
        celda.imageView?.image = UIImage(systemName: "bookmark.fill")
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(
        _ tableView: UITableView,
        didSelectRowAt indexPath: IndexPath
    ) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard marcadores.indices.contains(indexPath.row) else { return }
        let url = marcadores[indexPath.row].destino.enlace()
        dismiss(animated: true) {
            if let url {
                CoordinadorRouterEnlacesProfundosGlobalIPad.compartido.recibir(url)
            }
        }
    }

    override func tableView(
        _ tableView: UITableView,
        trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath
    ) -> UISwipeActionsConfiguration? {
        guard marcadores.indices.contains(indexPath.row) else { return nil }
        let marcador = marcadores[indexPath.row]
        let eliminar = UIContextualAction(
            style: .destructive,
            title: "Eliminar") { [weak self] _, _, terminar in
                guard let self else { terminar(false); return }
                do {
                    try self.repositorio.eliminar(marcador.id)
                    self.recargar()
                    terminar(true)
                } catch {
                    terminar(false)
                }
            }
        eliminar.image = UIImage(systemName: "trash")
        return UISwipeActionsConfiguration(actions: [eliminar])
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

@MainActor
final class CentroMarcadoresRegionIPad {
    static let compartido = CentroMarcadoresRegionIPad()

    func alternarZonaActual() {
        guard let contexto = contextoPrincipal() else { return }
        let visible = contexto.navegador.regionVisibleDocumento
        let centro = CGPoint(x: visible.midX, y: visible.midY)
        let zoom = max(Double(contexto.navegador.scroll.zoomScale), 0.01)
        let radio = max(Double(max(visible.width, visible.height)) * 0.12, 36)
        let documento = contexto.navegador.lienzo.sesion.documentoPersistenteId

        do {
            if let existente = RepositorioMarcadoresRegionIPad.compartido.cercano(
                documentoId: documento,
                x: Double(centro.x),
                y: Double(centro.y),
                radio: radio) {
                try RepositorioMarcadoresRegionIPad.compartido.eliminar(existente.id)
                UINotificationFeedbackGenerator().notificationOccurred(.success)
                UIAccessibility.post(
                    notification: .announcement,
                    argument: "Marcador eliminado")
                return
            }
            let tituloBase = LocalizadorDocumentoIPad.localizar(
                documentoId: documento)?.titulo ?? "Cuaderno"
            let titulo = "\(tituloBase) · \(Int(centro.x)), \(Int(centro.y))"
            try RepositorioMarcadoresRegionIPad.compartido.guardar(.init(
                documentoId: documento,
                titulo: titulo,
                x: Double(centro.x),
                y: Double(centro.y),
                zoom: zoom))
            UINotificationFeedbackGenerator().notificationOccurred(.success)
            UIAccessibility.post(
                notification: .announcement,
                argument: "Zona marcada")
        } catch {
            avisar(error.localizedDescription, desde: contexto.presentador)
        }
    }

    func mostrarLista() {
        guard let presentador = contextoPrincipal()?.presentador,
              presentador.presentedViewController == nil else { return }
        let panel = PanelMarcadoresRegionIPad()
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .formSheet
        presentador.present(nav, animated: true)
    }

    private struct Contexto {
        let navegador: VistaLienzoNavegable
        let presentador: UIViewController
    }

    private func contextoPrincipal() -> Contexto? {
        let escenas = UIApplication.shared.connectedScenes.compactMap {
            $0 as? UIWindowScene
        }.filter { $0.activationState == .foregroundActive }
        guard let ventana = escenas.flatMap(\.windows).first(where: \.isKeyWindow)
            ?? escenas.flatMap(\.windows).first(where: { !$0.isHidden }),
              let raizVentana = ventana.rootViewController,
              let raiz = buscarRaiz(en: raizVentana),
              let host = raiz.viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad,
              let lienzo = host.lienzoVisibleParaNavegacionProfunda()
        else { return nil }
        lienzo.loadViewIfNeeded()
        guard let navegador = primeraVista(
            en: lienzo.view, tipo: VistaLienzoNavegable.self) else { return nil }
        return .init(navegador: navegador, presentador: superior(raizVentana))
    }

    private func buscarRaiz(en controlador: UIViewController)
        -> ControladorRaizIPad? {
        if let valor = controlador as? ControladorRaizIPad { return valor }
        if let presentado = controlador.presentedViewController,
           let valor = buscarRaiz(en: presentado) { return valor }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController,
           let valor = buscarRaiz(en: visible) { return valor }
        for hijo in controlador.children.reversed() {
            if let valor = buscarRaiz(en: hijo) { return valor }
        }
        return nil
    }

    private func primeraVista<T: UIView>(en vista: UIView, tipo: T.Type) -> T? {
        if let valor = vista as? T { return valor }
        for hija in vista.subviews {
            if let valor = primeraVista(en: hija, tipo: tipo) { return valor }
        }
        return nil
    }

    private func superior(_ controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return superior(presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return superior(visible)
        }
        return controlador
    }

    private func avisar(_ texto: String, desde presentador: UIViewController) {
        let alerta = UIAlertController(
            title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        presentador.present(alerta, animated: true)
    }
}
