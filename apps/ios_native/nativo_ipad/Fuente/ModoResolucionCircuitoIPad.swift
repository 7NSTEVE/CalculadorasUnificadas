import UIKit

enum EtapaResolucionCircuitoIPad: String, Codable, CaseIterable {
    case datos = "Datos"
    case incognitas = "Incógnitas"
    case ecuaciones = "Ecuaciones"
    case desarrollo = "Desarrollo"
    case resultado = "Resultado"
    case comprobacion = "Comprobación"

    var tituloCorto: String {
        switch self {
        case .incognitas: return "Incóg."
        case .ecuaciones: return "Ecuac."
        case .desarrollo: return "Desarr."
        case .resultado: return "Result."
        case .comprobacion: return "Comprob."
        case .datos: return rawValue
        }
    }
}

struct HojaResolucionCircuitoIPad: Codable, Identifiable {
    var id = UUID()
    var documentoId: UInt64
    var esquemaId: UUID
    var titulo: String
    var contenido: [EtapaResolucionCircuitoIPad: String] = [:]
    var esquemaFuente: EsquemaEditable
    var creada = Date()
    var actualizada = Date()

    func textoCompleto() -> String {
        var lineas = ["# \(titulo)"]
        for etapa in EtapaResolucionCircuitoIPad.allCases {
            lineas += ["", "## \(etapa.rawValue)",
                       contenido[etapa]?.trimmingCharacters(in: .whitespacesAndNewlines)
                           ?? ""]
        }
        return lineas.joined(separator: "\n")
    }
}

private struct ArchivoResolucionCircuitosIPad: Codable {
    var version = 1
    var hojas: [HojaResolucionCircuitoIPad] = []
}

final class RepositorioResolucionCircuitosIPad {
    static let compartido = RepositorioResolucionCircuitosIPad()
    private let bloqueo = NSLock()
    private let url: URL
    private var archivo: ArchivoResolucionCircuitosIPad

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Ingenieria", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("resoluciones-circuitos.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? decoder.decode(ArchivoResolucionCircuitosIPad.self, from: $0)
        } ?? ArchivoResolucionCircuitosIPad()
    }

    func hoja(documentoId: UInt64, esquema: EsquemaEditable)
        -> HojaResolucionCircuitoIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let existente = archivo.hojas.first(where: {
            $0.documentoId == documentoId && $0.esquemaId == esquema.id
        }) { return existente }
        return HojaResolucionCircuitoIPad(
            documentoId: documentoId, esquemaId: esquema.id,
            titulo: "Resolución · \(esquema.titulo)", esquemaFuente: esquema)
    }

    func guardar(_ hoja: HojaResolucionCircuitoIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var nueva = hoja; nueva.actualizada = Date()
        if let indice = archivo.hojas.firstIndex(where: { $0.id == nueva.id }) {
            archivo.hojas[indice] = nueva
        } else {
            archivo.hojas.append(nueva)
        }
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let datos = try? encoder.encode(archivo) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

enum GeneradorResolucionCircuitoIPad {
    static func sugerencia(_ etapa: EtapaResolucionCircuitoIPad,
                           esquema: EsquemaEditable) -> String {
        switch etapa {
        case .datos: return datos(esquema)
        case .incognitas: return incognitas(esquema)
        case .ecuaciones: return ecuaciones(esquema)
        case .desarrollo:
            return "1. Definir tierra y signos de referencia.\n"
                + "2. Sustituir valores con unidades SI.\n"
                + "3. Resolver el sistema conservando pasos intermedios.\n"
                + "4. Revisar signo, magnitud y unidades antes de aceptar."
        case .resultado: return resultado(esquema)
        case .comprobacion: return comprobacion(esquema)
        }
    }

    private static func datos(_ esquema: EsquemaEditable) -> String {
        let mapa = esquema.mapaNodosElectricos()
        let componentes = esquema.componentes.filter(\.activo).map { componente in
            let a = mapa[componente.nodoA] ?? componente.nodoA
            let b = mapa[componente.nodoB] ?? componente.nodoB
            let valor = componente.tipo.requiereValor
                ? NotacionIngenieria.formatear(componente.valor) : "sin parámetro numérico"
            return "• \(componente.referencia): \(componente.tipo.nombre), "
                + "nodos \(a)–\(b), \(valor)"
        }
        return (["Referencia: nodo 0 (GND)"] + componentes).joined(separator: "\n")
    }

    private static func incognitas(_ esquema: EsquemaEditable) -> String {
        let mapa = esquema.mapaNodosElectricos()
        let nodos = Set(esquema.componentes.filter(\.activo).flatMap {
            [mapa[$0.nodoA] ?? $0.nodoA, mapa[$0.nodoB] ?? $0.nodoB]
        }).filter { $0 > 0 }.sorted()
        var lineas = nodos.map { "• V(\($0)) respecto a GND" }
        lineas += esquema.componentes.filter {
            $0.activo && ($0.tipo == .fuenteVoltaje || $0.tipo == .bateria)
        }.map { "• Corriente de \($0.referencia)" }
        return lineas.isEmpty ? "Defina explícitamente la magnitud buscada."
            : lineas.joined(separator: "\n")
    }

    private static func ecuaciones(_ esquema: EsquemaEditable) -> String {
        let mapa = esquema.mapaNodosElectricos()
        let activos = esquema.componentes.filter(\.activo)
        let nodos = Set(activos.flatMap {
            [mapa[$0.nodoA] ?? $0.nodoA, mapa[$0.nodoB] ?? $0.nodoB]
        }).filter { $0 > 0 }.sorted()
        var lineas: [String] = []
        for nodo in nodos {
            var terminos: [String] = []
            for componente in activos {
                let a = mapa[componente.nodoA] ?? componente.nodoA
                let b = mapa[componente.nodoB] ?? componente.nodoB
                let otro = a == nodo ? b : (b == nodo ? a : -1)
                guard otro >= 0 else { continue }
                let vOtro = otro == 0 ? "0" : "V\(otro)"
                switch componente.tipo {
                case .resistor:
                    terminos.append("(V\(nodo) - \(vOtro))/\(componente.referencia)")
                case .fuenteCorriente:
                    terminos.append(a == nodo ? "+\(componente.referencia)" :
                                                    "-\(componente.referencia)")
                default: break
                }
            }
            if !terminos.isEmpty {
                lineas.append("KCL nodo \(nodo): \(terminos.joined(separator: " + ")) = 0")
            }
        }
        for fuente in activos where fuente.tipo == .fuenteVoltaje || fuente.tipo == .bateria {
            let a = mapa[fuente.nodoA] ?? fuente.nodoA
            let b = mapa[fuente.nodoB] ?? fuente.nodoB
            let va = a == 0 ? "0" : "V\(a)"
            let vb = b == 0 ? "0" : "V\(b)"
            lineas.append("Restricción \(fuente.referencia): \(va) - \(vb) = "
                          + NotacionIngenieria.formatear(fuente.valor, unidad: "V"))
        }
        let sinModelo = activos.filter { !$0.tipo.tieneModeloDC }.map(\.referencia)
        if !sinModelo.isEmpty {
            lineas.append("Requieren un modelo DC elegido antes de resolver: "
                          + sinModelo.joined(separator: ", "))
        }
        return lineas.isEmpty ? "No se pudieron proponer ecuaciones DC con los datos actuales."
            : lineas.joined(separator: "\n")
    }

    private static func resultado(_ esquema: EsquemaEditable) -> String {
        let solucion = MotorCircuitoLocal.dc(esquema)
        if let error = solucion.error { return "No calculado: \(error)" }
        return solucion.voltajes.enumerated().map {
            "V(\($0.offset)) = \(NotacionIngenieria.formatear($0.element, unidad: "V"))"
        }.joined(separator: "\n")
    }

    private static func comprobacion(_ esquema: EsquemaEditable) -> String {
        let diagnostico = AnalizadorTopologiaCircuitoIPad.analizar(esquema)
        let solucion = MotorCircuitoLocal.dc(esquema)
        var lineas = [diagnostico.texto]
        if let error = solucion.error {
            lineas += ["", "Comprobación numérica bloqueada: \(error)"]
        } else {
            lineas += ["", "El sistema DC produjo una solución finita. "
                       + "Confirme manualmente polaridades, unidades y la magnitud pedida."]
        }
        return lineas.joined(separator: "\n")
    }
}

final class PanelResolucionCircuitoIPad: UIViewController, UITextViewDelegate {
    var alInsertarTexto: ((String) -> Void)?
    private var hoja: HojaResolucionCircuitoIPad
    private let esquema: EsquemaEditable
    private let etapas = EtapaResolucionCircuitoIPad.allCases
    private let selector: UISegmentedControl
    private let editor = UITextView()
    private let estado = UILabel()
    private var etapa: EtapaResolucionCircuitoIPad = .datos

    init(documentoId: UInt64, esquema: EsquemaEditable) {
        self.esquema = esquema
        hoja = RepositorioResolucionCircuitosIPad.compartido
            .hoja(documentoId: documentoId, esquema: esquema)
        selector = UISegmentedControl(items: EtapaResolucionCircuitoIPad.allCases
            .map(\.tituloCorto))
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Solve Mode"
        view.backgroundColor = .systemBackground
        selector.selectedSegmentIndex = 0
        selector.addTarget(self, action: #selector(cambiarEtapa(_:)), for: .valueChanged)
        selector.translatesAutoresizingMaskIntoConstraints = false
        editor.font = .preferredFont(forTextStyle: .body)
        editor.autocorrectionType = .yes
        editor.alwaysBounceVertical = true
        editor.delegate = self
        editor.translatesAutoresizingMaskIntoConstraints = false
        estado.text = "Guardado local · la sugerencia no reemplaza texto sin confirmar"
        estado.font = .preferredFont(forTextStyle: .footnote)
        estado.textColor = .secondaryLabel
        estado.numberOfLines = 0
        estado.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(selector); view.addSubview(editor); view.addSubview(estado)
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Insertar", style: .plain, target: self,
                            action: #selector(insertar)),
            UIBarButtonItem(title: "Informe", style: .plain, target: self,
                            action: #selector(abrirInforme)),
            UIBarButtonItem(title: "Sugerir", style: .plain, target: self,
                            action: #selector(sugerir))]
        NSLayoutConstraint.activate([
            selector.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                          constant: 12),
            selector.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 12),
            selector.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -12),
            editor.topAnchor.constraint(equalTo: selector.bottomAnchor, constant: 12),
            editor.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            editor.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            editor.bottomAnchor.constraint(equalTo: estado.topAnchor, constant: -8),
            estado.leadingAnchor.constraint(equalTo: editor.leadingAnchor),
            estado.trailingAnchor.constraint(equalTo: editor.trailingAnchor),
            estado.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                           constant: -8)])
        cargarEtapa()
    }

    override func viewWillDisappear(_ animated: Bool) {
        guardarEtapa(); super.viewWillDisappear(animated)
    }

    func textViewDidChange(_ textView: UITextView) {
        estado.text = "Cambios locales sin guardar…"
    }

    @objc private func cambiarEtapa(_ control: UISegmentedControl) {
        guardarEtapa()
        guard etapas.indices.contains(control.selectedSegmentIndex) else { return }
        etapa = etapas[control.selectedSegmentIndex]
        cargarEtapa()
    }

    private func cargarEtapa() {
        editor.text = hoja.contenido[etapa] ?? ""
        estado.text = "\(etapa.rawValue) · guardado local"
    }

    private func guardarEtapa() {
        hoja.contenido[etapa] = editor.text
        hoja.esquemaFuente = esquema
        RepositorioResolucionCircuitosIPad.compartido.guardar(hoja)
        estado.text = "\(etapa.rawValue) · guardado local"
    }

    @objc private func sugerir() {
        let propuesta = GeneradorResolucionCircuitoIPad.sugerencia(etapa, esquema: esquema)
        let alerta = UIAlertController(
            title: "Sugerencia para \(etapa.rawValue)",
            message: propuesta, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Agregar al final", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let separador = self.editor.text.isEmpty ? "" : "\n\n"
            self.editor.text += separador + propuesta
            self.guardarEtapa()
        })
        alerta.addAction(UIAlertAction(title: "Reemplazar esta etapa", style: .destructive) {
            [weak self] _ in
            self?.editor.text = propuesta
            self?.guardarEtapa()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func abrirInforme() {
        guardarEtapa()
        let panel = PanelInformeLaboratorioCircuitoIPad(esquema: esquema)
        panel.alInsertarTexto = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    @objc private func insertar() {
        guardarEtapa()
        alInsertarTexto?(hoja.textoCompleto())
    }
}
