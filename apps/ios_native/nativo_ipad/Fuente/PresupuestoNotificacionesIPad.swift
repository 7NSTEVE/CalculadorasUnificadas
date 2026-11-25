import UIKit
import UserNotifications

// MARK: - Modelo

enum CategoriaNotificacionTintaIPad: String, Codable, CaseIterable {
    case examen
    case revision
    case clase
    case tarea
    case recordatorio
    case resumen

    var nombre: String {
        switch self {
        case .examen:       return "Exámenes"
        case .revision:     return "Revisión y repaso"
        case .clase:        return "Clases"
        case .tarea:        return "Tareas pendientes"
        case .recordatorio: return "Recordatorios personales"
        case .resumen:      return "Resúmenes"
        }
    }

    var prioridadBase: Int {
        switch self {
        case .examen: return 5
        case .revision: return 3
        case .clase: return 4
        case .tarea: return 3
        case .recordatorio: return 2
        case .resumen: return 1
        }
    }
}

struct PresupuestoNotificacionesIPad: Codable, Equatable {
    var maximoDiario: Int = 8
    var maximoPorCurso: Int = 3
    var horaInicioSilencio: Int = 22  // 22:00
    var horaFinSilencio: Int = 7      // 07:00
    var presupuestoPorCategoria: [String: Int] = [
        "examen": 3, "revision": 2, "clase": 2,
        "tarea": 2, "recordatorio": 1, "resumen": 1,
    ]
    var consolidarExcedentes = true     // Si excede, enviar un solo resumen
    var urgentesSinPresupuesto = true   // Urgentes pasan incluso si se excede
}

struct RegistroNotificacionIPad: Codable, Identifiable {
    var id = UUID()
    var fecha: Date
    var categoria: CategoriaNotificacionTintaIPad
    var cursoId: String?
    var titulo: String
    var urgente: Bool = false
    var entregada: Bool = false
    var consolidada: Bool = false
}

private struct ArchivoPresupuestoIPad: Codable {
    var formato = 1
    var presupuesto = PresupuestoNotificacionesIPad()
    var registro: [RegistroNotificacionIPad] = []
}

// MARK: - Gestor de notificaciones con presupuesto

final class GestorNotificacionesIPad {
    static let compartido = GestorNotificacionesIPad()
    private let cola = DispatchQueue(label: "tinta.notificaciones.presupuesto")
    private let url: URL
    private var archivo: ArchivoPresupuestoIPad
    private let centro = UNUserNotificationCenter.current()

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Notificaciones",
                                                      isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("presupuesto.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ArchivoPresupuestoIPad.self, from: $0)
        } ?? ArchivoPresupuestoIPad()
    }

    var presupuesto: PresupuestoNotificacionesIPad {
        get { cola.sync { archivo.presupuesto } }
        set { cola.sync { archivo.presupuesto = newValue; persistir() } }
    }

    // MARK: Solicitar permiso

    func solicitarPermiso(completado: @escaping (Bool) -> Void) {
        centro.requestAuthorization(options: [.alert, .sound, .badge]) { ok, _ in
            DispatchQueue.main.async { completado(ok) }
        }
    }

    // MARK: Programar notificación respetando presupuesto

    func programar(titulo: String, cuerpo: String,
                   categoria: CategoriaNotificacionTintaIPad,
                   cursoId: String? = nil,
                   en fecha: Date,
                   urgente: Bool = false,
                   completado: ((Bool) -> Void)? = nil) {
        cola.async { [weak self] in
            guard let self else { return }
            let pres = self.archivo.presupuesto

            // Limpiar registros viejos (más de 24h)
            let hace24h = Date().addingTimeInterval(-24 * 3600)
            self.archivo.registro.removeAll { $0.fecha < hace24h }

            let hoy = self.registrosHoy()

            // Verificar hora silenciosa
            let hora = Calendar.current.component(.hour, from: fecha)
            let enSilencio: Bool
            if pres.horaInicioSilencio > pres.horaFinSilencio {
                enSilencio = hora >= pres.horaInicioSilencio
                    || hora <= pres.horaFinSilencio
            } else {
                enSilencio = hora >= pres.horaInicioSilencio
                    && hora <= pres.horaFinSilencio
            }
            if enSilencio && !urgente {
                DispatchQueue.main.async { completado?(false) }
                return
            }

            // Verificar presupuesto diario
            let totalHoy = hoy.filter { $0.entregada && !$0.consolidada }.count
            let categoriaKey = categoria.rawValue
            let totalCategoria = hoy.filter {
                $0.categoria == categoria && $0.entregada && !$0.consolidada
            }.count
            let maxCategoria = pres.presupuestoPorCategoria[categoriaKey]
                ?? pres.maximoDiario

            // Verificar presupuesto por curso
            let totalCurso = cursoId.flatMap { cId in
                hoy.filter { $0.cursoId == cId && $0.entregada && !$0.consolidada }.count
            } ?? 0

            let excedido = totalHoy >= pres.maximoDiario
                || totalCategoria >= maxCategoria
                || totalCurso >= pres.maximoPorCurso

            if excedido && !urgente {
                if pres.consolidarExcedentes {
                    self.consolidar(titulo: titulo, cuerpo: cuerpo,
                                    categoria: categoria, cursoId: cursoId,
                                    fecha: fecha)
                }
                DispatchQueue.main.async { completado?(false) }
                return
            }

            // Crear y enviar
            let registro = RegistroNotificacionIPad(
                fecha: fecha, categoria: categoria, cursoId: cursoId,
                titulo: titulo, urgente: urgente, entregada: true)

            let contenido = UNMutableNotificationContent()
            contenido.title = titulo
            contenido.body = cuerpo
            contenido.sound = urgente ? .defaultCriticalSound(withAudioVolume: 1.0) : .default
            contenido.categoryIdentifier = categoriaKey
            if let curso = cursoId { contenido.threadIdentifier = curso }

            let componentes = Calendar.current.dateComponents(
                [.year, .month, .day, .hour, .minute], from: fecha)
            let trigger = UNCalendarNotificationTrigger(
                dateMatching: componentes, repeats: false)

            let solicitud = UNNotificationRequest(
                identifier: registro.id.uuidString,
                content: contenido, trigger: trigger)

            self.centro.add(solicitud) { [weak self] error in
                if error == nil {
                    self?.cola.async {
                        self?.archivo.registro.append(registro)
                        self?.persistir()
                    }
                }
                DispatchQueue.main.async { completado?(error == nil) }
            }
        }
    }

    // MARK: Consolidar excedentes

    private func consolidar(titulo: String, cuerpo: String,
                            categoria: CategoriaNotificacionTintaIPad,
                            cursoId: String?, fecha: Date) {
        // Buscar notificación resumen existente para hoy
        let idResumen = "resumen-\(categoria.rawValue)-\(cursoId ?? "global")"
        let pendientes = archivo.registro.filter {
            $0.consolidada && $0.categoria == categoria
        }

        let registro = RegistroNotificacionIPad(
            fecha: fecha, categoria: categoria, cursoId: cursoId,
            titulo: titulo, urgente: false, entregada: true, consolidada: true)
        archivo.registro.append(registro)

        let total = pendientes.count + 1
        let contenido = UNMutableNotificationContent()
        contenido.title = "📋 \(categoria.nombre): \(total) pendientes"
        contenido.body = "Último: \(titulo)\n\(cuerpo)"
        contenido.sound = .default
        if let curso = cursoId { contenido.threadIdentifier = curso }

        centro.removePendingNotificationRequests(withIdentifiers: [idResumen])
        let trigger = UNTimeIntervalNotificationTrigger(
            timeInterval: max(1, fecha.timeIntervalSinceNow), repeats: false)
        let solicitud = UNNotificationRequest(
            identifier: idResumen, content: contenido, trigger: trigger)
        centro.add(solicitud)
        persistir()
    }

    // MARK: Estadísticas

    func registrosHoy() -> [RegistroNotificacionIPad] {
        let inicioDia = Calendar.current.startOfDay(for: Date())
        return archivo.registro.filter { $0.fecha >= inicioDia }
    }

    func resumenPresupuesto() -> (enviadas: Int, maximo: Int,
                                   porCategoria: [(String, Int, Int)]) {
        let hoy = registrosHoy().filter { $0.entregada }
        let pres = archivo.presupuesto
        var porCat: [(String, Int, Int)] = []
        for cat in CategoriaNotificacionTintaIPad.allCases {
            let enviadas = hoy.filter { $0.categoria == cat }.count
            let max = pres.presupuestoPorCategoria[cat.rawValue] ?? pres.maximoDiario
            porCat.append((cat.nombre, enviadas, max))
        }
        return (hoy.count, pres.maximoDiario, porCat)
    }

    func cancelarTodas() {
        centro.removeAllPendingNotificationRequests()
        cola.sync { archivo.registro.removeAll(); persistir() }
    }

    // MARK: Persistencia

    private func persistir() {
        guard let datos = try? JSONEncoder().encode(archivo) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

// MARK: - Panel de configuración de presupuesto

final class PanelPresupuestoNotificacionesIPad: UITableViewController {
    var alAplicar: ((PresupuestoNotificacionesIPad) -> Void)?
    private var pres: PresupuestoNotificacionesIPad

    init() {
        pres = GestorNotificacionesIPad.compartido.presupuesto
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Presupuesto de notificaciones"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Guardar", style: .done, target: self, action: #selector(guardar))
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "celda")
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0: return "Límites"
        case 1: return "Por categoría"
        default: return "Opciones"
        }
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 3
        case 1: return CategoriaNotificacionTintaIPad.allCases.count
        default: return 2
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = tableView.dequeueReusableCell(withIdentifier: "celda",
                                                  for: indexPath)
        switch indexPath.section {
        case 0:
            switch indexPath.row {
            case 0:
                celda.textLabel?.text = "Máximo diario: \(pres.maximoDiario)"
                let stepper = UIStepper()
                stepper.value = Double(pres.maximoDiario)
                stepper.minimumValue = 1; stepper.maximumValue = 30
                stepper.addAction(UIAction { [weak self] _ in
                    self?.pres.maximoDiario = Int(stepper.value)
                    celda.textLabel?.text = "Máximo diario: \(Int(stepper.value))"
                }, for: .valueChanged)
                celda.accessoryView = stepper
            case 1:
                celda.textLabel?.text = "Máximo por curso: \(pres.maximoPorCurso)"
                let stepper = UIStepper()
                stepper.value = Double(pres.maximoPorCurso)
                stepper.minimumValue = 1; stepper.maximumValue = 10
                stepper.addAction(UIAction { [weak self] _ in
                    self?.pres.maximoPorCurso = Int(stepper.value)
                    celda.textLabel?.text = "Máximo por curso: \(Int(stepper.value))"
                }, for: .valueChanged)
                celda.accessoryView = stepper
            default:
                celda.textLabel?.text = "Horas silenciosas: \(pres.horaInicioSilencio):00–\(pres.horaFinSilencio):00"
            }
        case 1:
            let cat = CategoriaNotificacionTintaIPad.allCases[indexPath.row]
            let max = pres.presupuestoPorCategoria[cat.rawValue] ?? pres.maximoDiario
            celda.textLabel?.text = "\(cat.nombre): \(max)"
            let stepper = UIStepper()
            stepper.value = Double(max)
            stepper.minimumValue = 0; stepper.maximumValue = 10
            stepper.tag = indexPath.row
            stepper.addAction(UIAction { [weak self] _ in
                let cats = CategoriaNotificacionTintaIPad.allCases
                let key = cats[stepper.tag].rawValue
                self?.pres.presupuestoPorCategoria[key] = Int(stepper.value)
                celda.textLabel?.text = "\(cats[stepper.tag].nombre): \(Int(stepper.value))"
            }, for: .valueChanged)
            celda.accessoryView = stepper
        default:
            switch indexPath.row {
            case 0:
                celda.textLabel?.text = "Consolidar excedentes"
                let sw = UISwitch()
                sw.isOn = pres.consolidarExcedentes
                sw.addAction(UIAction { [weak self] _ in
                    self?.pres.consolidarExcedentes = sw.isOn
                }, for: .valueChanged)
                celda.accessoryView = sw
            default:
                celda.textLabel?.text = "Urgentes ignoran presupuesto"
                let sw = UISwitch()
                sw.isOn = pres.urgentesSinPresupuesto
                sw.addAction(UIAction { [weak self] _ in
                    self?.pres.urgentesSinPresupuesto = sw.isOn
                }, for: .valueChanged)
                celda.accessoryView = sw
            }
        }
        return celda
    }

    @objc private func guardar() {
        GestorNotificacionesIPad.compartido.presupuesto = pres
        alAplicar?(pres)
        dismiss(animated: true)
    }
}
