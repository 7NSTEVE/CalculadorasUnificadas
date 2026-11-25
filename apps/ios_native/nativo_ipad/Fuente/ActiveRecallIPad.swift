import Foundation
import UIKit
import Vision

enum EstiloCubiertaRecallIPad: String, Codable, CaseIterable {
    case desenfoque
    case papel
    case numeros
    case variable
    case circuito
    case etiquetas
    case valores

    var nombre: String {
        switch self {
        case .desenfoque: return "Desenfocar"
        case .papel: return "Cubrir con papel"
        case .numeros: return "Solo números"
        case .variable: return "Solo variable"
        case .circuito: return "Parte del circuito"
        case .etiquetas: return "Etiquetas"
        case .valores: return "Valores"
        }
    }

    var segmentablePorOCR: Bool {
        switch self {
        case .numeros, .variable, .etiquetas, .valores: return true
        default: return false
        }
    }
}

struct FragmentoRecallVisualIPad: Equatable {
    var texto: String
    var region: CGRect
}

/// Segmenta únicamente la imagen temporal de la selección. Vision trabaja en
/// el dispositivo y las cajas resultantes se guardan como sidecar; no se
/// modifica ni se sustituye la tinta, PDF o fotografía original.
enum AnalizadorRecallVisualIPad {
    static func detectar(en imagen: UIImage, estilo: EstiloCubiertaRecallIPad,
                         regionDocumento: CGRect,
                         completado: @escaping ([FragmentoRecallVisualIPad]) -> Void) {
        guard estilo.segmentablePorOCR, let cg = imagen.cgImage else {
            completado([]); return
        }
        let solicitud = VNRecognizeTextRequest { solicitud, _ in
            let observaciones = solicitud.results as? [VNRecognizedTextObservation] ?? []
            var salida: [FragmentoRecallVisualIPad] = []
            for observacion in observaciones {
                guard let candidato = observacion.topCandidates(1).first else { continue }
                let texto = candidato.string
                guard let expresion = expresion(estilo) else { continue }
                let rangoCompleto = NSRange(texto.startIndex..., in: texto)
                for coincidencia in expresion.matches(in: texto, range: rangoCompleto) {
                    guard let rango = Range(coincidencia.range, in: texto) else { continue }
                    do {
                        guard let rectangulo = try candidato.boundingBox(for: rango) else {
                            continue
                        }
                        let cajaVision = rectangulo.boundingBox
                        let cajaUIKit = CGRect(
                            x: regionDocumento.minX
                                + cajaVision.minX * regionDocumento.width,
                            y: regionDocumento.minY
                                + (1 - cajaVision.maxY) * regionDocumento.height,
                            width: cajaVision.width * regionDocumento.width,
                            height: cajaVision.height * regionDocumento.height)
                            .insetBy(dx: -4, dy: -3)
                        guard cajaUIKit.width > 2, cajaUIKit.height > 2 else { continue }
                        salida.append(FragmentoRecallVisualIPad(
                            texto: String(texto[rango]), region: cajaUIKit))
                    } catch { continue }
                }
            }
            let unicos = salida.reduce(into: [FragmentoRecallVisualIPad]()) {
                acumulado, fragmento in
                guard !acumulado.contains(where: {
                    $0.region.intersection(fragmento.region).areaRecall
                        >= min($0.region.areaRecall, fragmento.region.areaRecall) * 0.82
                }) else { return }
                acumulado.append(fragmento)
            }
            DispatchQueue.main.async { completado(unicos) }
        }
        solicitud.recognitionLevel = .accurate
        solicitud.usesLanguageCorrection = false
        solicitud.recognitionLanguages = RepositorioBusquedaTecnicaIPad.compartido
            .idiomasOCR()
        solicitud.customWords = RepositorioBusquedaTecnicaIPad.compartido.palabrasOCR()
        solicitud.automaticallyDetectsLanguage = true
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try VNImageRequestHandler(cgImage: cg, options: [:])
                    .perform([solicitud])
            } catch { DispatchQueue.main.async { completado([]) } }
        }
    }

    private static func expresion(_ estilo: EstiloCubiertaRecallIPad)
        -> NSRegularExpression? {
        let patron: String
        switch estilo {
        case .numeros:
            patron = #"(?<![A-Za-z])[-+]?\d+(?:[.,]\d+)?(?:[eE][-+]?\d+)?"#
        case .variable:
            patron = #"(?<![A-Za-z])(?:[A-Za-z]|[α-ωΑ-Ω])(?:[_]?[A-Za-z0-9]{0,4})(?![A-Za-z])"#
        case .etiquetas:
            patron = #"(?i)(?<![A-Za-z0-9])(?:R|C|L|Q|D|U|J|SW|V|I)\d{1,4}|\b(?:GND|VCC|VDD|VSS|VIN|VOUT|CLK|DATA)\b"#
        case .valores:
            patron = #"(?i)(?<![A-Za-z])[-+]?\d+(?:[.,]\d+)?\s*(?:p|n|u|µ|μ|m|k|meg|M|G|T)?(?:Ω|ohm|F|H|V|A|W|Hz|s|%)"#
        default: return nil
        }
        return try? NSRegularExpression(pattern: patron)
    }
}

private extension CGRect {
    var areaRecall: CGFloat { isNull ? 0 : max(width, 0) * max(height, 0) }
}

struct CubiertaRecallIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var documento: UInt64
    var claves: [String]
    var region: CGRect
    var estilo: EstiloCubiertaRecallIPad
    var titulo: String
    var progresoRevelado: CGFloat = 0
    var automatica = false
    var etiquetaExamen = false
    var creada = Date()
}

private struct ArchivoRecallIPad: Codable {
    var formato = 1
    var activoPorDocumento: [String: Bool] = [:]
    /// Opcional para decodificar sin pérdida los sidecars creados antes de que
    /// existiera el filtro de examen.
    var soloExamenPorDocumento: [String: Bool]? = nil
    var cubiertas: [CubiertaRecallIPad] = []
}

final class RepositorioRecallIPad {
    static let compartido = RepositorioRecallIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoRecallIPad

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Recall", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("cubiertas.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? decoder.decode(ArchivoRecallIPad.self, from: $0)
        } ?? ArchivoRecallIPad()
    }

    func activo(_ documento: UInt64) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.activoPorDocumento[String(documento)] ?? false
    }

    func activar(_ valor: Bool, documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.activoPorDocumento[String(documento)] = valor; persistir()
    }

    func soloExamen(_ documento: UInt64) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.soloExamenPorDocumento?[String(documento)] ?? false
    }

    func filtrarSoloExamen(_ valor: Bool, documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var filtros = archivo.soloExamenPorDocumento ?? [:]
        filtros[String(documento)] = valor
        archivo.soloExamenPorDocumento = filtros
        persistir()
    }

    func cubiertas(_ documento: UInt64,
                   soloExamen: Bool = false) -> [CubiertaRecallIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.cubiertas.filter {
            $0.documento == documento && (!soloExamen || $0.etiquetaExamen)
        }.sorted { $0.creada < $1.creada }
    }

    @discardableResult
    func agregar(documento: UInt64, claves: [String], region: CGRect,
                 estilo: EstiloCubiertaRecallIPad, titulo: String,
                 automatica: Bool = false,
                 examen: Bool = false) -> CubiertaRecallIPad? {
        guard !region.isNull, region.width > 2, region.height > 2 else { return nil }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let clavesUnicas = Array(Set(claves)).sorted()
        if let existente = archivo.cubiertas.first(where: {
            $0.documento == documento && Set($0.claves) == Set(clavesUnicas)
                && $0.estilo == estilo && abs($0.region.midX - region.midX) < 2
                && abs($0.region.midY - region.midY) < 2
        }) { return existente }
        let cubierta = CubiertaRecallIPad(
            documento: documento, claves: clavesUnicas,
            region: region, estilo: estilo, titulo: titulo,
            automatica: automatica, etiquetaExamen: examen)
        archivo.cubiertas.append(cubierta); persistir(); return cubierta
    }

    func revelarPaso(_ id: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let indice = archivo.cubiertas.firstIndex(where: { $0.id == id }) else {
            return
        }
        archivo.cubiertas[indice].progresoRevelado = min(
            1, archivo.cubiertas[indice].progresoRevelado + 0.25)
        persistir()
    }

    func restablecer(_ documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for indice in archivo.cubiertas.indices
        where archivo.cubiertas[indice].documento == documento {
            archivo.cubiertas[indice].progresoRevelado = 0
        }
        persistir()
    }

    func revelarTodo(_ documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        for indice in archivo.cubiertas.indices
        where archivo.cubiertas[indice].documento == documento {
            archivo.cubiertas[indice].progresoRevelado = 1
        }
        persistir()
    }

    func eliminar(_ id: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        archivo.cubiertas.removeAll { $0.id == id }; persistir()
    }

    @discardableResult
    func generarDesdeSemantica(documento: UInt64) -> Int {
        let marcas = RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento).filter {
                [.formula, .definicion, .examen, .ejemplo]
                    .contains($0.tipo) && $0.region != nil
            }
        var creadas = 0
        for marca in marcas {
            let antes = cubiertas(documento).count
            _ = agregar(
                documento: documento, claves: marca.clavesElemento,
                region: marca.region!,
                estilo: marca.tipo == .formula ? .variable : .desenfoque,
                titulo: marca.titulo, automatica: true,
                examen: marca.tipo == .examen)
            if cubiertas(documento).count > antes { creadas += 1 }
        }
        return creadas
    }

    /// Duplica la definición del modo de estudio, no el avance momentáneo.
    /// Las cubiertas reciben UUID nuevos y vuelven a 0 % revelado, mientras que
    /// las claves de los elementos siguen siendo válidas porque la página
    /// duplicada conserva sus IDs internos estables.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let claveOrigen = String(origen)
        let claveDestino = String(destino)
        let filtros = archivo.soloExamenPorDocumento ?? [:]
        guard archivo.activoPorDocumento[claveDestino] == nil,
              filtros[claveDestino] == nil,
              !archivo.cubiertas.contains(where: { $0.documento == destino }) else {
            throw CocoaError(.fileWriteFileExists)
        }

        var siguiente = archivo
        siguiente.activoPorDocumento[claveDestino] =
            archivo.activoPorDocumento[claveOrigen] ?? false
        var filtrosSiguientes = siguiente.soloExamenPorDocumento ?? [:]
        filtrosSiguientes[claveDestino] = filtros[claveOrigen] ?? false
        siguiente.soloExamenPorDocumento = filtrosSiguientes

        let nuevas = archivo.cubiertas.filter { $0.documento == origen }.map {
            original -> CubiertaRecallIPad in
            var copia = original
            copia.id = UUID()
            copia.documento = destino
            copia.progresoRevelado = 0
            return copia
        }
        siguiente.cubiertas.append(contentsOf: nuevas)
        try persistir(siguiente)
        archivo = siguiente
        return nuevas.count
    }

    /// Compensación para una duplicación que no alcanzó a completar todos sus
    /// sidecars. Retira configuración, filtro y cubiertas del destino en una
    /// sola escritura antes de reemplazar el estado en memoria.
    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let clave = String(documento)
        let tieneDatos = archivo.activoPorDocumento[clave] != nil
            || archivo.soloExamenPorDocumento?[clave] != nil
            || archivo.cubiertas.contains(where: { $0.documento == documento })
        guard tieneDatos else { return }
        var siguiente = archivo
        siguiente.activoPorDocumento.removeValue(forKey: clave)
        var filtros = siguiente.soloExamenPorDocumento ?? [:]
        filtros.removeValue(forKey: clave)
        siguiente.soloExamenPorDocumento = filtros.isEmpty ? nil : filtros
        siguiente.cubiertas.removeAll { $0.documento == documento }
        try persistir(siguiente)
        archivo = siguiente
    }

    private func persistir(_ valor: ArchivoRecallIPad) throws {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    private func persistir() {
        try? persistir(archivo)
    }
}

final class CapaRecallIPad: UIView {
    let documento: UInt64
    private(set) var soloExamen = false

    init(documento: UInt64) {
        self.documento = documento
        super.init(frame: .zero)
        backgroundColor = .clear
        autoresizingMask = [.flexibleWidth, .flexibleHeight]
        recargar()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    func recargar() {
        subviews.forEach { $0.removeFromSuperview() }
        let repositorio = RepositorioRecallIPad.compartido
        soloExamen = repositorio.soloExamen(documento)
        isHidden = !repositorio.activo(documento)
        isUserInteractionEnabled = !isHidden
        guard !isHidden else { return }
        for cubierta in repositorio.cubiertas(documento, soloExamen: soloExamen)
        where cubierta.progresoRevelado < 1 {
            let vista = VistaCubiertaRecallIPad(cubierta: cubierta)
            vista.alRevelar = { [weak self] id in
                RepositorioRecallIPad.compartido.revelarPaso(id)
                self?.recargar()
            }
            addSubview(vista)
        }
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        guard !isHidden else { return nil }
        for vista in subviews.reversed() {
            let local = vista.convert(point, from: self)
            if let resultado = vista.hitTest(local, with: event) { return resultado }
        }
        return nil
    }
}

private final class VistaCubiertaRecallIPad: UIControl {
    var alRevelar: ((UUID) -> Void)?
    private let cubierta: CubiertaRecallIPad

    init(cubierta: CubiertaRecallIPad) {
        self.cubierta = cubierta
        let revelado = min(max(cubierta.progresoRevelado, 0), 1)
        let restante = 1 - revelado
        let marco = CGRect(
            x: cubierta.region.minX + cubierta.region.width * revelado,
            y: cubierta.region.minY,
            width: max(cubierta.region.width * restante, 2),
            height: cubierta.region.height)
        super.init(frame: marco)
        layer.cornerRadius = min(10, bounds.height * 0.2)
        clipsToBounds = true
        accessibilityLabel = "Respuesta oculta: \(cubierta.titulo)"
        accessibilityHint = "Toque para revelar otro 25 por ciento"
        addTarget(self, action: #selector(revelar), for: .touchUpInside)
        construir()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    private func construir() {
        if cubierta.estilo == .desenfoque {
            let desenfoque = UIVisualEffectView(
                effect: UIBlurEffect(style: .systemChromeMaterial))
            desenfoque.frame = bounds
            desenfoque.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            desenfoque.isUserInteractionEnabled = false
            addSubview(desenfoque)
        } else {
            backgroundColor = cubierta.estilo == .papel
                ? .systemBackground : UIColor.systemIndigo.withAlphaComponent(0.90)
        }
        let etiqueta = UILabel(frame: bounds.insetBy(dx: 8, dy: 4))
        etiqueta.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        etiqueta.textAlignment = .center
        etiqueta.numberOfLines = 2
        etiqueta.adjustsFontSizeToFitWidth = true
        etiqueta.text = cubierta.estilo == .papel ? "Toque para revelar"
            : "\(cubierta.estilo.nombre) · revelar 25 %"
        etiqueta.textColor = cubierta.estilo == .papel ? .secondaryLabel : .white
        etiqueta.isUserInteractionEnabled = false
        addSubview(etiqueta)
    }

    @objc private func revelar() { alRevelar?(cubierta.id) }
}

final class PanelRecallIPad: UITableViewController {
    var alCambiar: (() -> Void)?
    private let documento: UInt64
    private var cubiertas: [CubiertaRecallIPad] = []

    init(documento: UInt64) {
        self.documento = documento
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Tapar respuestas"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        refrescar()
    }
    private func refrescar() {
        cubiertas = RepositorioRecallIPad.compartido.cubiertas(documento)
        tableView.reloadData()
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Modo" : "Cubiertas"
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 5 : max(cubiertas.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let titulos = ["Activar / desactivar", "Solo contenido marcado para examen",
                           "Generar desde fórmulas y definiciones",
                           "Ocultar todo de nuevo", "Revelar todo"]
            celda.textLabel?.text = titulos[indexPath.row]
            if indexPath.row == 0 {
                celda.accessoryType = RepositorioRecallIPad.compartido.activo(documento)
                    ? .checkmark : .none
            } else if indexPath.row == 1 {
                celda.accessoryType = RepositorioRecallIPad.compartido
                    .soloExamen(documento) ? .checkmark : .none
            }
        } else if cubiertas.isEmpty {
            celda.textLabel?.text = "No hay respuestas cubiertas"
            celda.selectionStyle = .none
        } else {
            let cubierta = cubiertas[indexPath.row]
            celda.textLabel?.text = cubierta.titulo
            celda.detailTextLabel?.text = "\(cubierta.estilo.nombre) · revelado \(Int(cubierta.progresoRevelado * 100))%\(cubierta.automatica ? " · automática" : "")"
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard indexPath.section == 0 else { return }
        let repositorio = RepositorioRecallIPad.compartido
        switch indexPath.row {
        case 0: repositorio.activar(!repositorio.activo(documento), documento: documento)
        case 1:
            repositorio.filtrarSoloExamen(
                !repositorio.soloExamen(documento), documento: documento)
        case 2: _ = repositorio.generarDesdeSemantica(documento: documento)
        case 3: repositorio.restablecer(documento)
        default: repositorio.revelarTodo(documento)
        }
        alCambiar?(); refrescar()
    }
    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard indexPath.section == 1,
              cubiertas.indices.contains(indexPath.row) else { return nil }
        let cubierta = cubiertas[indexPath.row]
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") {
            [weak self] _, _, terminar in
            RepositorioRecallIPad.compartido.eliminar(cubierta.id)
            self?.alCambiar?(); self?.refrescar(); terminar(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar])
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
