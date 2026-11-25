import UIKit

private struct SegmentoUnionCircuitoIPad {
    var a: CGPoint
    var b: CGPoint
    var conexion: UUID
}

private func segmentosParaUnion(_ esquema: EsquemaEditable)
    -> [SegmentoUnionCircuitoIPad] {
    var salida: [SegmentoUnionCircuitoIPad] = []
    for conexion in esquema.conexiones ?? [] {
        guard conexion.tipo != .puente, conexion.tipo != .curva,
              let a = esquema.nodos.first(where: { $0.numero == conexion.nodoA })?.posicion,
              let b = esquema.nodos.first(where: { $0.numero == conexion.nodoB })?.posicion else {
            continue
        }
        if conexion.tipo == .libre {
            salida.append(SegmentoUnionCircuitoIPad(a: a, b: b,
                                                     conexion: conexion.id))
        } else {
            let esquina = CGPoint(x: b.x, y: a.y)
            salida.append(SegmentoUnionCircuitoIPad(a: a, b: esquina,
                                                     conexion: conexion.id))
            salida.append(SegmentoUnionCircuitoIPad(a: esquina, b: b,
                                                     conexion: conexion.id))
        }
    }
    return salida
}

private func interseccionParaUnion(_ p: SegmentoUnionCircuitoIPad,
                                   _ q: SegmentoUnionCircuitoIPad) -> CGPoint? {
    let r = CGPoint(x: p.b.x - p.a.x, y: p.b.y - p.a.y)
    let s = CGPoint(x: q.b.x - q.a.x, y: q.b.y - q.a.y)
    let denominador = r.x * s.y - r.y * s.x
    guard abs(denominador) > 1e-8 else { return nil }
    let diferencia = CGPoint(x: q.a.x - p.a.x, y: q.a.y - p.a.y)
    let t = (diferencia.x * s.y - diferencia.y * s.x) / denominador
    let u = (diferencia.x * r.y - diferencia.y * r.x) / denominador
    guard t > 0.001, t < 0.999, u > 0.001, u < 0.999 else { return nil }
    return CGPoint(x: p.a.x + t * r.x, y: p.a.y + t * r.y)
}

extension EsquemaEditable {
    /// Convierte cruces de cables ortogonales/libres en nodos compartidos.
    /// La llamada debe estar precedida por confirmación de usuario.
    @discardableResult
    mutating func crearUnionesEnCruces(limite: Int = 256) -> Int {
        var creadas = 0
        while creadas < limite, let candidato = primerCruceUnible() {
            let numeroNodo: Int
            if let existente = nodos.first(where: {
                hypot($0.posicion.x - candidato.punto.x,
                      $0.posicion.y - candidato.punto.y) <= 8
            }) {
                numeroNodo = existente.numero
            } else {
                numeroNodo = (nodos.map(\.numero).max() ?? 0) + 1
                nodos.append(NodoEsquema(numero: numeroNodo,
                                         posicion: candidato.punto,
                                         nombre: "J\(numeroNodo)"))
            }
            dividirConexion(candidato.primera, por: numeroNodo)
            dividirConexion(candidato.segunda, por: numeroNodo)
            creadas += 1
        }
        if creadas > 0 { fechaModificacion = Date() }
        return creadas
    }

    private func primerCruceUnible() -> (primera: UUID, segunda: UUID,
                                         punto: CGPoint)? {
        let segmentos = segmentosParaUnion(self)
        for i in segmentos.indices {
            for j in segmentos.indices where j > i &&
                segmentos[i].conexion != segmentos[j].conexion {
                if let punto = interseccionParaUnion(segmentos[i], segmentos[j]) {
                    return (segmentos[i].conexion, segmentos[j].conexion, punto)
                }
            }
        }
        return nil
    }

    private mutating func dividirConexion(_ id: UUID, por nodo: Int) {
        guard var lista = conexiones,
              let indice = lista.firstIndex(where: { $0.id == id }) else { return }
        let original = lista.remove(at: indice)
        guard original.nodoA != nodo, original.nodoB != nodo else {
            lista.append(original); conexiones = lista; return
        }
        var primera = original
        primera.id = UUID(); primera.nodoB = nodo
        var segunda = original
        segunda.id = UUID(); segunda.nodoA = nodo
        // Evita repetir la misma anotación visual en ambos tramos.
        segunda.etiqueta = ""; segunda.corriente = ""; segunda.polaridadA = ""
        lista.append(primera); lista.append(segunda)
        conexiones = lista
    }
}

enum SeveridadDiagnosticoCircuitoIPad: Int, Codable {
    case informacion
    case advertencia
    case bloqueo

    var icono: String {
        switch self {
        case .informacion: return "info.circle"
        case .advertencia: return "exclamationmark.triangle"
        case .bloqueo: return "xmark.octagon"
        }
    }
}

struct HallazgoCircuitoIPad: Identifiable, Equatable {
    var id = UUID()
    var severidad: SeveridadDiagnosticoCircuitoIPad
    var titulo: String
    var detalle: String
}

struct DiagnosticoCircuitoIPad {
    var nodos: Int
    var ramas: Int
    var lazosIndependientes: Int
    var subcircuitos: Int
    var hallazgos: [HallazgoCircuitoIPad]

    var texto: String {
        let cabecera = "Nodos: \(nodos) · Ramas: \(ramas) · "
            + "Lazos independientes: \(lazosIndependientes) · "
            + "Subcircuitos: \(subcircuitos)"
        guard !hallazgos.isEmpty else { return cabecera + "\nSin hallazgos estructurales." }
        return ([cabecera] + hallazgos.map {
            "[\($0.severidad)] \($0.titulo): \($0.detalle)"
        }).joined(separator: "\n")
    }
}

enum AnalizadorTopologiaCircuitoIPad {
    static func analizar(_ esquema: EsquemaEditable) -> DiagnosticoCircuitoIPad {
        let mapa = esquema.mapaNodosElectricos()
        let activos = esquema.componentes.filter(\.activo)
        var hallazgos: [HallazgoCircuitoIPad] = []
        var terminalesPorNodo: [Int: Int] = [:]
        var adyacencia: [Int: Set<Int>] = [:]
        var nodos: Set<Int> = []
        var ramas = 0

        for componente in activos {
            let a = mapa[componente.nodoA] ?? componente.nodoA
            let b = mapa[componente.nodoB] ?? componente.nodoB
            nodos.insert(a); nodos.insert(b)
            terminalesPorNodo[a, default: 0] += 1
            terminalesPorNodo[b, default: 0] += 1
            if componente.tipo != .probeVoltaje && componente.tipo != .probeCorriente &&
                componente.tipo != .voltimetro && componente.tipo != .osciloscopio {
                ramas += 1
                adyacencia[a, default: []].insert(b)
                adyacencia[b, default: []].insert(a)
            }
            if componente.tipo.requiereValor &&
                (!(componente.valor > 0) || !componente.valor.isFinite) {
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: .bloqueo,
                    titulo: "Valor sin confirmar",
                    detalle: "\(componente.referencia) · \(componente.tipo.nombre)"))
            }
            if a == b {
                let fuente = componente.tipo == .fuenteVoltaje ||
                    componente.tipo == .fuenteAC || componente.tipo == .bateria
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: fuente ? .bloqueo : .advertencia,
                    titulo: fuente ? "Fuente en cortocircuito" : "Terminales en la misma red",
                    detalle: "\(componente.referencia) conecta el nodo \(a) consigo mismo."))
            }
            if !componente.tipo.tieneModeloDC && !componente.tipo.tieneModeloAC {
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: .informacion,
                    titulo: "Sin modelo numérico",
                    detalle: "\(componente.referencia) se conserva editable, pero no se simula."))
            } else if !componente.tipo.tieneModeloDC {
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: .informacion, titulo: "Solo modelo AC",
                    detalle: "\(componente.referencia) no participa en análisis DC."))
            } else if !componente.tipo.tieneModeloAC {
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: .informacion, titulo: "Solo modelo DC",
                    detalle: "\(componente.referencia) no participa en análisis AC."))
            }
        }

        for (nodo, cantidad) in terminalesPorNodo where nodo != 0 && cantidad < 2 {
            hallazgos.append(HallazgoCircuitoIPad(
                severidad: .advertencia,
                titulo: "Terminal flotante",
                detalle: "La red del nodo \(nodo) solo tiene \(cantidad) terminal conectado."))
        }

        for conexion in esquema.conexiones ?? [] {
            if !esquema.nodos.contains(where: { $0.numero == conexion.nodoA }) ||
                !esquema.nodos.contains(where: { $0.numero == conexion.nodoB }) {
                hallazgos.append(HallazgoCircuitoIPad(
                    severidad: .bloqueo, titulo: "Cable con nodo inexistente",
                    detalle: "Conexión \(conexion.nodoA)–\(conexion.nodoB)."))
            }
        }
        hallazgos += crucesAmbiguos(esquema)

        let componentesConectados = contarComponentesConectados(
            nodos: nodos, adyacencia: adyacencia)
        let lazos = max(ramas - nodos.count + componentesConectados, 0)
        if componentesConectados > 1 && ramas > 0 {
            hallazgos.append(HallazgoCircuitoIPad(
                severidad: .advertencia, titulo: "Subcircuitos desconectados",
                detalle: "Se detectaron \(componentesConectados) grupos eléctricos."))
        }
        return DiagnosticoCircuitoIPad(
            nodos: nodos.count, ramas: ramas, lazosIndependientes: lazos,
            subcircuitos: componentesConectados, hallazgos: hallazgos)
    }

    private static func contarComponentesConectados(
        nodos: Set<Int>, adyacencia: [Int: Set<Int>]) -> Int {
        let relevantes = nodos.filter { adyacencia[$0]?.isEmpty == false }
        var pendientes = Set(relevantes)
        var cantidad = 0
        while let inicio = pendientes.first {
            cantidad += 1
            var pila = [inicio]
            pendientes.remove(inicio)
            while let actual = pila.popLast() {
                for vecino in adyacencia[actual] ?? [] where pendientes.contains(vecino) {
                    pendientes.remove(vecino); pila.append(vecino)
                }
            }
        }
        return cantidad
    }

    private struct Segmento { var a: CGPoint; var b: CGPoint; var conexion: UUID }

    private static func crucesAmbiguos(_ esquema: EsquemaEditable) -> [HallazgoCircuitoIPad] {
        var segmentos: [Segmento] = []
        for conexion in esquema.conexiones ?? [] where conexion.tipo != .puente {
            guard let a = esquema.nodos.first(where: { $0.numero == conexion.nodoA })?.posicion,
                  let b = esquema.nodos.first(where: { $0.numero == conexion.nodoB })?.posicion else {
                continue
            }
            switch conexion.tipo {
            case .libre, .curva:
                segmentos.append(Segmento(a: a, b: b, conexion: conexion.id))
            case .conductor, .bus:
                let esquina = CGPoint(x: b.x, y: a.y)
                segmentos.append(Segmento(a: a, b: esquina, conexion: conexion.id))
                segmentos.append(Segmento(a: esquina, b: b, conexion: conexion.id))
            case .puente:
                break
            }
        }
        var salida: [HallazgoCircuitoIPad] = []
        var ubicacionesReportadas: Set<String> = []
        for i in segmentos.indices {
            for j in segmentos.indices where j > i &&
                segmentos[i].conexion != segmentos[j].conexion {
                guard let punto = interseccionPropia(segmentos[i], segmentos[j]) else { continue }
                let hayNodo = esquema.nodos.contains {
                    hypot($0.posicion.x - punto.x, $0.posicion.y - punto.y) <= 8
                }
                if !hayNodo {
                    let clave = "\(Int(punto.x.rounded())):\(Int(punto.y.rounded()))"
                    guard ubicacionesReportadas.insert(clave).inserted else { continue }
                    salida.append(HallazgoCircuitoIPad(
                        severidad: .advertencia,
                        titulo: "Cruce ambiguo",
                        detalle: "Cruce cerca de (\(Int(punto.x)), \(Int(punto.y))) sin nodo ni puente explícito."))
                }
            }
        }
        return salida
    }

    private static func interseccionPropia(_ p: Segmento, _ q: Segmento) -> CGPoint? {
        let r = CGPoint(x: p.b.x - p.a.x, y: p.b.y - p.a.y)
        let s = CGPoint(x: q.b.x - q.a.x, y: q.b.y - q.a.y)
        let denominador = r.x * s.y - r.y * s.x
        guard abs(denominador) > 1e-8 else { return nil }
        let diferencia = CGPoint(x: q.a.x - p.a.x, y: q.a.y - p.a.y)
        let t = (diferencia.x * s.y - diferencia.y * s.x) / denominador
        let u = (diferencia.x * r.y - diferencia.y * r.x) / denominador
        guard t > 0.001, t < 0.999, u > 0.001, u < 0.999 else { return nil }
        return CGPoint(x: p.a.x + t * r.x, y: p.a.y + t * r.y)
    }
}

final class PanelDiagnosticoCircuitoIPad: UITableViewController {
    var alInsertarTexto: ((String) -> Void)?
    private let diagnostico: DiagnosticoCircuitoIPad

    init(esquema: EsquemaEditable) {
        diagnostico = AnalizadorTopologiaCircuitoIPad.analizar(esquema)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Topología y diagnóstico"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Exportar", style: .plain, target: self,
            action: #selector(exportar(_:)))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 4 : max(diagnostico.hallazgos.count, 1)
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Estructura" : "Comprobaciones no destructivas"
    }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let datos = [("Nodos", diagnostico.nodos), ("Ramas", diagnostico.ramas),
                         ("Lazos independientes", diagnostico.lazosIndependientes),
                         ("Subcircuitos", diagnostico.subcircuitos)]
            celda.textLabel?.text = datos[indexPath.row].0
            celda.detailTextLabel?.text = String(datos[indexPath.row].1)
        } else if diagnostico.hallazgos.isEmpty {
            celda.textLabel?.text = "Sin hallazgos estructurales"
            celda.imageView?.image = UIImage(systemName: "checkmark.circle")
        } else {
            let hallazgo = diagnostico.hallazgos[indexPath.row]
            celda.textLabel?.text = hallazgo.titulo
            celda.detailTextLabel?.text = hallazgo.detalle
            celda.detailTextLabel?.numberOfLines = 0
            celda.imageView?.image = UIImage(systemName: hallazgo.severidad.icono)
        }
        return celda
    }

    @objc private func exportar(_ remitente: UIBarButtonItem) {
        let menu = UIAlertController(title: "Diagnóstico", message: nil,
                                     preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
            [weak self] _ in guard let self else { return }
            self.alInsertarTexto?(self.diagnostico.texto)
        })
        menu.addAction(UIAlertAction(title: "Copiar", style: .default) {
            [weak self] _ in UIPasteboard.general.string = self?.diagnostico.texto
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.barButtonItem = remitente
        present(menu, animated: true)
    }
}
