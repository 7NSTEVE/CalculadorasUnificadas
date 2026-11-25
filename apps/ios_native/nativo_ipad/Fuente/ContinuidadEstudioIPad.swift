import Foundation
import UIKit

extension Notification.Name {
    static let reentradaDesdeSegundoPlanoIPad = Notification.Name(
        "Tinta.reentradaDesdeSegundoPlano")
}

/// Handoff del ecosistema iPadOS complementa —no reemplaza— el enlace `tinta:`
/// que ya funciona entre dispositivos. v8 publica UUID durables cuando el
/// llamador dispone de ellos y mantiene lectura del contrato UInt64 anterior.
final class GestorContinuidadIPad {
    static let compartido = GestorContinuidadIPad()
    private var actividad: NSUserActivity?
    private let tipoNota = "com.calculadorasunificadas.tinta.continuarNota"
    private let tipoClase = "com.calculadorasunificadas.tinta.continuarClase"
    private init() {}

    func publicar(destino: DestinoEnlaceIPad, titulo: String) {
        guard let enlace = try? CodecEnlacesInternosIPad.crearURL(para: destino) else {
            return
        }
        publicar(enlace: enlace,
                 titulo: titulo,
                 identificador: destino.documentoId.map {
                    "documento-v1-\($0.description)"
                 } ?? "destino-v1-\(destino.clave)",
                 documento: destino.documentoId?.description)
    }

    func publicar(destino: DestinoEnlaceTinta, titulo: String) {
        guard let enlace = destino.enlace() else { return }
        publicar(enlace: enlace,
                 titulo: titulo,
                 identificador: "documento-legacy-\(destino.documentoId)",
                 documento: String(destino.documentoId))
    }

    private func publicar(enlace: URL,
                          titulo: String,
                          identificador: String,
                          documento: String?) {
        actividad?.resignCurrent()
        actividad?.invalidate()
        let nueva = NSUserActivity(activityType: tipoNota)
        nueva.title = titulo
        nueva.isEligibleForHandoff = true
        nueva.isEligibleForSearch = false
        nueva.isEligibleForPublicIndexing = false
        nueva.targetContentIdentifier = identificador
        var userInfo: [AnyHashable: Any] = [
            "enlace": enlace.absoluteString,
        ]
        if let documento {
            userInfo["documento"] = documento
        }
        nueva.userInfo = userInfo
        nueva.requiredUserInfoKeys = ["enlace"]
        nueva.needsSave = true
        nueva.becomeCurrent()
        actividad = nueva
    }

    @discardableResult
    func continuar(_ actividad: NSUserActivity) -> Bool {
        guard actividad.activityType == tipoNota
                || actividad.activityType == tipoClase,
              let texto = actividad.userInfo?["enlace"] as? String,
              let url = URL(string: texto) else { return false }

        let nuevo = (try? CodecEnlacesInternosIPad.decodificar(url)) != nil
        let legacy = DestinoEnlaceTinta.leer(url) != nil
        guard nuevo || legacy else { return false }
        CentroEnlacesProfundos.compartido.recibir(url)
        return true
    }
}

struct InterrupcionEstudioIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var salida = Date()
    var regreso: Date?
    var marcadaPorUsuario = false
    var nota: String?

    var segundosFuera: Double? {
        regreso.map { max(0, $0.timeIntervalSince(salida)) }
    }
}

struct ResumenDistraccionIPad {
    var salidas: Int
    var regresoPromedioSegundos: Double
    var interrupcionesMarcadas: Int
}

final class MonitorDistraccionIPad {
    static let compartido = MonitorDistraccionIPad()
    private let bloqueo = NSLock()
    private let url: URL
    private let claveActivo = "Tinta.MetricasDistraccion.Activas"
    private var interrupciones: [InterrupcionEstudioIPad] = []

    private init() {
        let documentos = FileManager.default.urls(
            for: .documentDirectory, in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("interrupciones.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let guardadas = try? decoder.decode([InterrupcionEstudioIPad].self,
                                               from: datos) {
            interrupciones = guardadas
        }
    }

    var activo: Bool {
        get { UserDefaults.standard.bool(forKey: claveActivo) }
        set { UserDefaults.standard.set(newValue, forKey: claveActivo) }
    }

    func registrarSalida() {
        guard activo else { return }
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard interrupciones.last?.regreso != nil || interrupciones.isEmpty else { return }
        interrupciones.append(InterrupcionEstudioIPad())
        guardarSinBloquear()
    }

    func registrarRegreso() {
        bloqueo.lock()
        if activo, let indice = interrupciones.indices.last,
           interrupciones[indice].regreso == nil {
            interrupciones[indice].regreso = Date()
            guardarSinBloquear()
        }
        bloqueo.unlock()
        DispatchQueue.main.async {
            NotificationCenter.default.post(
                name: .reentradaDesdeSegundoPlanoIPad, object: nil)
        }
    }

    func marcarInterrupcion(_ nota: String?) {
        guard activo else { return }
        bloqueo.lock(); defer { bloqueo.unlock() }
        interrupciones.append(InterrupcionEstudioIPad(
            salida: Date(), regreso: Date(), marcadaPorUsuario: true, nota: nota))
        guardarSinBloquear()
    }

    func resumen(ultimosDias: Int = 30) -> ResumenDistraccionIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let desde = Calendar.current.date(byAdding: .day, value: -ultimosDias,
                                          to: Date()) ?? .distantPast
        let periodo = interrupciones.filter { $0.salida >= desde }
        let tiempos = periodo.compactMap(\.segundosFuera)
        return ResumenDistraccionIPad(
            salidas: periodo.filter { !$0.marcadaPorUsuario }.count,
            regresoPromedioSegundos: tiempos.isEmpty ? 0
                : tiempos.reduce(0, +) / Double(tiempos.count),
            interrupcionesMarcadas: periodo.filter(\.marcadaPorUsuario).count)
    }

    func borrarMetricas() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        interrupciones.removeAll(); guardarSinBloquear()
    }

    private func guardarSinBloquear() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let datos = try? encoder.encode(interrupciones) {
            try? datos.write(to: url, options: .atomic)
        }
    }
}

final class PanelDistraccionIPad: UITableViewController {
    private let monitor = MonitorDistraccionIPad.compartido
    private let interruptor = UISwitch()

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Interrupciones explícitas"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        interruptor.isOn = monitor.activo
        interruptor.addTarget(self, action: #selector(cambiar), for: .valueChanged)
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 1 : 2
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Opt-in" : "Últimos 30 días"
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        section == 0
            ? "Solo se registran salida, regreso e interrupciones que usted marque. No se usa cámara ni se diagnostica atención."
            : nil
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            celda.textLabel?.text = "Medir interrupciones"
            celda.accessoryView = interruptor
        } else {
            let resumen = monitor.resumen()
            if indexPath.row == 0 {
                celda.textLabel?.text = "Salidas: \(resumen.salidas)"
                celda.detailTextLabel?.text = String(
                    format: "Regreso promedio: %.1f min",
                    resumen.regresoPromedioSegundos / 60)
            } else {
                celda.textLabel?.text = "Interrupciones marcadas: \(resumen.interrupcionesMarcadas)"
                celda.detailTextLabel?.text = "Tocar para registrar una interrupción consciente"
                celda.accessoryType = .disclosureIndicator
            }
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard indexPath.section == 1, indexPath.row == 1 else { return }
        let alerta = UIAlertController(title: "Marcar interrupción", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Opcional: qué interrumpió" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Registrar", style: .default) {
            [weak self] _ in
            self?.monitor.marcarInterrupcion(alerta.textFields?.first?.text)
            self?.tableView.reloadData()
        })
        present(alerta, animated: true)
    }
    @objc private func cambiar() { monitor.activo = interruptor.isOn; tableView.reloadData() }
    @objc private func cerrar() { dismiss(animated: true) }
}
