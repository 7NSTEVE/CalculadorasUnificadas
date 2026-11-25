import Foundation
import UIKit

struct ConfiguracionBusquedaTecnicaIPad: Codable, Equatable {
    var idiomasOCR: [String] = ["es-ES", "en-US"]
    var palabrasTecnicas: [String] = Self.palabrasIniciales
    var alias: [String: [String]] = Self.aliasIniciales
    var toleranciaManuscrita = true

    static let palabrasIniciales = [
        "Thévenin", "Norton", "Kirchhoff", "fasor", "MOSFET", "JFET",
        "op-amp", "KCL", "KVL", "RLC", "Nyquist", "Bode", "Smith",
        "impedancia", "admitancia", "reactancia", "inductancia",
        "capacitancia", "transconductancia", "duty cycle", "flip-flop",
        "multiplexor", "Schmitt", "Zener", "Thevenin"
    ]

    static let aliasIniciales: [String: [String]] = [
        "resistencia": ["resistor", "resistor eléctrico"],
        "capacitor": ["condensador", "capacitancia"],
        "inductor": ["bobina", "inductancia"],
        "tierra": ["ground", "gnd", "masa"],
        "voltaje": ["voltage", "tensión", "diferencia de potencial"],
        "corriente": ["current", "intensidad"],
        "potencia": ["power"],
        "amplificador operacional": ["op amp", "op-amp", "operational amplifier"],
        "mosfet": ["transistor mos", "metal oxide semiconductor fet"],
        "kcl": ["ley de corrientes de kirchhoff", "kirchhoff current law"],
        "kvl": ["ley de voltajes de kirchhoff", "kirchhoff voltage law"],
        "thevenin": ["thévenin", "tevenin"],
        "frecuencia de corte": ["cutoff frequency", "corner frequency"],
        "ohm": ["ohmio", "ohmios", "omega", "Ω"],
        "micro": ["µ", "μ", "u"],
        "sumatoria": ["summation", "sigma", "∑"],
        "integral": ["integration", "∫"],
        "derivada parcial": ["partial derivative", "∂"]
    ]
}

/// Preferencias locales usadas tanto por Vision OCR como por el índice. No
/// reemplazan la imagen ni la tinta original: solo mejoran candidatos y
/// coincidencias que el usuario puede revisar.
final class RepositorioBusquedaTecnicaIPad {
    static let compartido = RepositorioBusquedaTecnicaIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var valor: ConfiguracionBusquedaTecnicaIPad

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("busqueda-tecnica-ipad.json")
        valor = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ConfiguracionBusquedaTecnicaIPad.self,
                                      from: $0)
        } ?? ConfiguracionBusquedaTecnicaIPad()
    }

    func instantanea() -> ConfiguracionBusquedaTecnicaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return valor
    }

    func guardar(_ configuracion: ConfiguracionBusquedaTecnicaIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var limpia = configuracion
        limpia.idiomasOCR = Array(Set(configuracion.idiomasOCR.map {
            $0.trimmingCharacters(in: .whitespacesAndNewlines)
        }.filter { !$0.isEmpty })).sorted()
        limpia.palabrasTecnicas = Array(Set(configuracion.palabrasTecnicas.map {
            $0.trimmingCharacters(in: .whitespacesAndNewlines)
        }.filter { !$0.isEmpty })).sorted {
            $0.localizedCaseInsensitiveCompare($1) == .orderedAscending
        }
        limpia.alias = Dictionary(uniqueKeysWithValues: configuracion.alias.compactMap {
            clave, valores in
            let claveLimpia = Normalizador.texto(clave)
            let valoresLimpios = Array(Set(valores.map(Normalizador.texto)
                .filter { !$0.isEmpty && $0 != claveLimpia })).sorted()
            return claveLimpia.isEmpty ? nil : (claveLimpia, valoresLimpios)
        })
        valor = limpia
        guard let datos = try? JSONEncoder().encode(limpia) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    func palabrasOCR() -> [String] { instantanea().palabrasTecnicas }
    func idiomasOCR() -> [String] { instantanea().idiomasOCR }

    func variantes(_ termino: String) -> Set<String> {
        let configuracion = instantanea()
        let normal = Normalizador.texto(termino)
        var salida: Set<String> = [normal]
        for (canonico, valores) in configuracion.alias {
            let grupo = Set([canonico] + valores.map(Normalizador.texto))
            if grupo.contains(normal) { salida.formUnion(grupo) }
        }
        return salida
    }

    func agregarPalabra(_ palabra: String) {
        var configuracion = instantanea()
        configuracion.palabrasTecnicas.append(palabra)
        guardar(configuracion)
    }

    func eliminarPalabra(_ palabra: String) {
        var configuracion = instantanea()
        let objetivo = Normalizador.texto(palabra)
        configuracion.palabrasTecnicas.removeAll {
            Normalizador.texto($0) == objetivo
        }
        guardar(configuracion)
    }

    func agregarAlias(canonico: String, valor: String) {
        var configuracion = instantanea()
        let clave = Normalizador.texto(canonico)
        guard !clave.isEmpty else { return }
        configuracion.alias[clave, default: []].append(valor)
        guardar(configuracion)
    }

    func eliminarGrupoAlias(_ canonico: String) {
        var configuracion = instantanea()
        configuracion.alias.removeValue(forKey: Normalizador.texto(canonico))
        guardar(configuracion)
    }
}

struct ConsultaAcademicaIPad {
    var texto: String
    var filtrosCurso: [String]

    static func analizar(_ consulta: String) -> ConsultaAcademicaIPad {
        let patron = #"(?i)(?:^|\s)in\s*:\s*(\"[^\"]+\"|[^\s]+)"#
        guard let regex = try? NSRegularExpression(pattern: patron) else {
            return ConsultaAcademicaIPad(texto: consulta, filtrosCurso: [])
        }
        let rango = NSRange(consulta.startIndex..<consulta.endIndex, in: consulta)
        let coincidencias = regex.matches(in: consulta, range: rango)
        let filtros = coincidencias.compactMap { coincidencia -> String? in
            guard let rangoSwift = Range(coincidencia.range(at: 1), in: consulta) else {
                return nil
            }
            return String(consulta[rangoSwift]).trimmingCharacters(
                in: CharacterSet(charactersIn: "\"")
                    .union(.whitespacesAndNewlines))
        }
        let mutable = NSMutableString(string: consulta)
        for coincidencia in coincidencias.reversed() {
            mutable.replaceCharacters(in: coincidencia.range(at: 0), with: " ")
        }
        return ConsultaAcademicaIPad(
            texto: String(mutable).trimmingCharacters(in: .whitespacesAndNewlines),
            filtrosCurso: filtros)
    }

    func idsCurso(en cursos: [CursoAcademico]) -> Set<UUID>? {
        guard !filtrosCurso.isEmpty else { return nil }
        var ids = Set<UUID>()
        for filtro in filtrosCurso {
            let buscado = Normalizador.texto(filtro)
            for curso in cursos {
                let campos = [curso.codigo, curso.nombre, curso.semestre]
                    .map(Normalizador.texto)
                if campos.contains(buscado)
                    || campos.contains(where: { !$0.isEmpty && $0.contains(buscado) }) {
                    ids.insert(curso.id)
                }
            }
        }
        return ids
    }
}

enum SimilitudManuscritaIPad {
    static func puntuacion(consulta: String, candidato: String) -> Double {
        let repositorio = RepositorioBusquedaTecnicaIPad.compartido
        let variantesConsulta = repositorio.variantes(consulta)
        let variantesCandidato = repositorio.variantes(candidato)
        if !variantesConsulta.isDisjoint(with: variantesCandidato) { return 1 }
        guard repositorio.instantanea().toleranciaManuscrita else { return 0 }

        var mejor = 0.0
        for a in variantesConsulta {
            for b in variantesCandidato {
                let ca = canonicoVisual(a), cb = canonicoVisual(b)
                guard !ca.isEmpty, !cb.isEmpty else { continue }
                let distancia = distanciaDamerau(ca, cb)
                let largo = max(ca.count, cb.count)
                let tolerada = largo <= 3 ? 1 : (largo <= 7 ? 2 : 3)
                if distancia <= tolerada {
                    mejor = max(mejor, 1 - Double(distancia) / Double(max(largo, 1)))
                }
            }
        }
        return mejor
    }

    private static func canonicoVisual(_ valor: String) -> String {
        Normalizador.texto(valor)
            .replacingOccurrences(of: "l", with: "1")
            .replacingOccurrences(of: "i", with: "1")
            .replacingOccurrences(of: "|", with: "1")
            .replacingOccurrences(of: "o", with: "0")
            .replacingOccurrences(of: "µ", with: "u")
            .replacingOccurrences(of: "μ", with: "u")
            .replacingOccurrences(of: "ω", with: "ohm")
            .replacingOccurrences(of: "Ω", with: "ohm")
            .filter { $0.isLetter || $0.isNumber }
    }

    private static func distanciaDamerau(_ a: String, _ b: String) -> Int {
        let izquierda = Array(a), derecha = Array(b)
        if izquierda.isEmpty { return derecha.count }
        if derecha.isEmpty { return izquierda.count }
        var matriz = Array(repeating: Array(repeating: 0,
                                             count: derecha.count + 1),
                           count: izquierda.count + 1)
        for i in 0...izquierda.count { matriz[i][0] = i }
        for j in 0...derecha.count { matriz[0][j] = j }
        for i in 1...izquierda.count {
            for j in 1...derecha.count {
                let costo = izquierda[i - 1] == derecha[j - 1] ? 0 : 1
                matriz[i][j] = min(
                    min(matriz[i - 1][j] + 1,
                        matriz[i][j - 1] + 1),
                    matriz[i - 1][j - 1] + costo)
                if i > 1, j > 1,
                   izquierda[i - 1] == derecha[j - 2],
                   izquierda[i - 2] == derecha[j - 1] {
                    matriz[i][j] = min(matriz[i][j],
                                       matriz[i - 2][j - 2] + costo)
                }
            }
        }
        return matriz[izquierda.count][derecha.count]
    }
}

final class PanelDiccionarioTecnicoIPad: UITableViewController {
    private let repositorio = RepositorioBusquedaTecnicaIPad.compartido
    private var configuracion = ConfiguracionBusquedaTecnicaIPad()

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "OCR y búsqueda técnica"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(anadir))
        refrescar()
    }

    private func refrescar() {
        configuracion = repositorio.instantanea()
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["OCR bilingüe y tolerancia", "Diccionario técnico", "Alias configurables"][section]
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return "Vision recibe español e inglés en la misma solicitud. La tolerancia contempla confusiones frecuentes como R1/Rl sin cambiar el original."
        case 1:
            return "Las palabras personalizadas mejoran candidatos OCR; no autocorrigen ni sustituyen tinta o imágenes."
        default:
            return "Ejemplo: resistor, resistencia y condensador pueden encontrarse con cualquiera de sus alias."
        }
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 2
        case 1: return max(configuracion.palabrasTecnicas.count, 1)
        default: return max(configuracion.alias.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0 where indexPath.row == 0:
            celda.textLabel?.text = "Idiomas OCR"
            celda.detailTextLabel?.text = configuracion.idiomasOCR.joined(separator: " + ")
            celda.selectionStyle = .none
        case 0:
            celda.textLabel?.text = "Búsqueda manuscrita tolerante"
            celda.detailTextLabel?.text = "R1 ≈ Rl · Ω ≈ ohm · µ ≈ u"
            let interruptor = UISwitch()
            interruptor.isOn = configuracion.toleranciaManuscrita
            interruptor.addTarget(self, action: #selector(cambiarTolerancia(_:)),
                                  for: .valueChanged)
            celda.accessoryView = interruptor
        case 1:
            if configuracion.palabrasTecnicas.isEmpty {
                celda.textLabel?.text = "Añadir primera palabra"
                celda.imageView?.image = UIImage(systemName: "plus.circle")
            } else {
                celda.textLabel?.text = configuracion.palabrasTecnicas[indexPath.row]
                celda.detailTextLabel?.text = "Candidato OCR protegido"
            }
        default:
            let grupos = configuracion.alias.sorted { $0.key < $1.key }
            if grupos.isEmpty {
                celda.textLabel?.text = "Añadir primer alias"
                celda.imageView?.image = UIImage(systemName: "plus.circle")
            } else {
                let grupo = grupos[indexPath.row]
                celda.textLabel?.text = grupo.key
                celda.detailTextLabel?.text = grupo.value.joined(separator: " · ")
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard indexPath.section > 0 else { return nil }
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") {
            [weak self] _, _, terminar in
            guard let self else { terminar(false); return }
            if indexPath.section == 1,
               self.configuracion.palabrasTecnicas.indices.contains(indexPath.row) {
                self.repositorio.eliminarPalabra(
                    self.configuracion.palabrasTecnicas[indexPath.row])
            } else if indexPath.section == 2 {
                let grupos = self.configuracion.alias.sorted { $0.key < $1.key }
                if grupos.indices.contains(indexPath.row) {
                    self.repositorio.eliminarGrupoAlias(grupos[indexPath.row].key)
                }
            }
            self.refrescar(); terminar(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar])
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if indexPath.section == 1 && configuracion.palabrasTecnicas.isEmpty {
            pedirPalabra()
        } else if indexPath.section == 2 && configuracion.alias.isEmpty {
            pedirAlias()
        }
    }

    @objc private func cambiarTolerancia(_ sender: UISwitch) {
        configuracion.toleranciaManuscrita = sender.isOn
        repositorio.guardar(configuracion)
    }

    @objc private func anadir() {
        let hoja = UIAlertController(title: "Añadir", message: nil,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Palabra técnica", style: .default) {
            [weak self] _ in self?.pedirPalabra()
        })
        hoja.addAction(UIAlertAction(title: "Alias bilingüe", style: .default) {
            [weak self] _ in self?.pedirAlias()
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func pedirPalabra() {
        let alerta = UIAlertController(title: "Palabra técnica", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Ej.: transconductancia" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let palabra = alerta.textFields?.first?.text else { return }
            self?.repositorio.agregarPalabra(palabra); self?.refrescar()
        })
        present(alerta, animated: true)
    }

    private func pedirAlias() {
        let alerta = UIAlertController(title: "Alias de búsqueda",
                                       message: "Ambos términos encontrarán el mismo concepto.",
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Término canónico: resistencia" }
        alerta.addTextField { $0.placeholder = "Alias: resistor" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let canonico = alerta.textFields?[0].text,
                  let alias = alerta.textFields?[1].text else { return }
            self?.repositorio.agregarAlias(canonico: canonico, valor: alias)
            self?.refrescar()
        })
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
