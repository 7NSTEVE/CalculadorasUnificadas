import UIKit

// MARK: - Modelo

enum TipoNotaAdhesivaIPad: String, Codable, CaseIterable {
    case nota
    case callout
    case comentario
    case nodoMapa

    var simbolo: String {
        switch self {
        case .nota: return "note.text"
        case .callout: return "arrowtriangle.left.and.line.vertical.and.arrowtriangle.right"
        case .comentario: return "text.bubble"
        case .nodoMapa: return "circle.grid.cross"
        }
    }

    var nombre: String {
        switch self {
        case .nota: return "Nota adhesiva"
        case .callout: return "Callout"
        case .comentario: return "Comentario personal"
        case .nodoMapa: return "Nodo de mapa mental"
        }
    }
}

enum EstadoNotaAdhesivaIPad: String, Codable {
    case colapsada
    case expandida
    case destacada
}

struct NotaAdhesivaLienzo: Codable, Identifiable, Equatable {
    var id = UUID()
    var tipo: TipoNotaAdhesivaIPad = .nota
    var estado: EstadoNotaAdhesivaIPad = .expandida
    var posicion: CGPoint = .zero
    var tamano: CGSize = CGSize(width: 180, height: 120)
    var texto: String = ""
    var colorHex: UInt32 = 0xFFFFE066
    var etiquetas: [String] = []
    var exportable = true
    var fecha = Date()
    var conexiones: [UUID] = []
    var puntoCallout: CGPoint?
    var zIndice: Int = 0

    var color: UIColor {
        UIColor(
            red: CGFloat((colorHex >> 16) & 0xFF) / 255,
            green: CGFloat((colorHex >> 8) & 0xFF) / 255,
            blue: CGFloat(colorHex & 0xFF) / 255,
            alpha: CGFloat((colorHex >> 24) & 0xFF) / 255)
    }
}

struct ConexionMapaMentalIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var origenId: UUID
    var destinoId: UUID
    var etiqueta: String = ""
    var estiloCurva: Bool = true
}

private struct ArchivoNotasAdhesivasIPad: Codable {
    var formato = 1
    var notas: [NotaAdhesivaLienzo] = []
    var conexiones: [ConexionMapaMentalIPad] = []
}

final class RepositorioNotasAdhesivasIPad {
    static let compartido = RepositorioNotasAdhesivasIPad()
    private let cola = DispatchQueue(label: "tinta.notas.adhesivas")
    private let carpeta: URL
    private var archivos: [UInt64: ArchivoNotasAdhesivasIPad] = [:]

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        carpeta = soporte.appendingPathComponent("Tinta/NotasAdhesivas", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
    }

    private func urlArchivo(_ documento: UInt64) -> URL {
        carpeta.appendingPathComponent("doc-\(documento).json")
    }

    func notas(_ documento: UInt64) -> [NotaAdhesivaLienzo] {
        cola.sync { cargar(documento).notas }
    }

    func conexiones(_ documento: UInt64) -> [ConexionMapaMentalIPad] {
        cola.sync { cargar(documento).conexiones }
    }

    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64)
        throws -> (notas: Int, conexiones: Int) {
        try cola.sync {
            guard origen != destino else {
                throw CocoaError(.fileWriteInvalidFileName)
            }
            let rutaDestino = urlArchivo(destino)
            let cacheDestino = archivos[destino]
            let cacheTieneContenido = cacheDestino.map {
                !$0.notas.isEmpty || !$0.conexiones.isEmpty
            } ?? false
            guard !FileManager.default.fileExists(atPath: rutaDestino.path),
                  !cacheTieneContenido else {
                throw CocoaError(.fileWriteFileExists)
            }

            let fuente = cargar(origen)
            var mapa: [UUID: UUID] = [:]
            for nota in fuente.notas { mapa[nota.id] = UUID() }

            let notasNuevas = fuente.notas.map { original -> NotaAdhesivaLienzo in
                var copia = original
                copia.id = mapa[original.id]!
                copia.conexiones = original.conexiones.compactMap { mapa[$0] }
                return copia
            }
            let conexionesNuevas = fuente.conexiones.compactMap {
                original -> ConexionMapaMentalIPad? in
                guard let origenNuevo = mapa[original.origenId],
                      let destinoNuevo = mapa[original.destinoId] else {
                    return nil
                }
                var copia = original
                copia.id = UUID()
                copia.origenId = origenNuevo
                copia.destinoId = destinoNuevo
                return copia
            }
            guard conexionesNuevas.count == fuente.conexiones.count else {
                throw CocoaError(.fileWriteUnknown)
            }

            let copia = ArchivoNotasAdhesivasIPad(
                formato: fuente.formato,
                notas: notasNuevas,
                conexiones: conexionesNuevas)
            let datos = try JSONEncoder().encode(copia)
            try datos.write(to: rutaDestino,
                            options: [.atomic, .completeFileProtection])
            archivos[destino] = copia
            return (notasNuevas.count, conexionesNuevas.count)
        }
    }

    /// Rollback de una copia descartada. A diferencia de eliminar las notas una
    /// por una, retira también el archivo vacío y el caché del documento para
    /// que el mismo UUID destino pueda reintentarse limpiamente.
    func eliminarDocumento(_ documento: UInt64) throws {
        try cola.sync {
            let ruta = urlArchivo(documento)
            if FileManager.default.fileExists(atPath: ruta.path) {
                try FileManager.default.removeItem(at: ruta)
            }
            archivos.removeValue(forKey: documento)
        }
    }

    func agregar(_ nota: NotaAdhesivaLienzo, documento: UInt64) {
        cola.sync {
            var a = cargar(documento)
            a.notas.append(nota)
            archivos[documento] = a
            persistir(a, documento: documento)
        }
    }

    func actualizar(_ nota: NotaAdhesivaLienzo, documento: UInt64) {
        cola.sync {
            var a = cargar(documento)
            if let i = a.notas.firstIndex(where: { $0.id == nota.id }) {
                a.notas[i] = nota
            }
            archivos[documento] = a
            persistir(a, documento: documento)
        }
    }

    func eliminar(_ id: UUID, documento: UInt64) {
        cola.sync {
            var a = cargar(documento)
            a.notas.removeAll { $0.id == id }
            a.conexiones.removeAll { $0.origenId == id || $0.destinoId == id }
            archivos[documento] = a
            persistir(a, documento: documento)
        }
    }

    func conectar(origen: UUID, destino: UUID, documento: UInt64) {
        cola.sync {
            var a = cargar(documento)
            guard !a.conexiones.contains(where: {
                ($0.origenId == origen && $0.destinoId == destino) ||
                ($0.origenId == destino && $0.destinoId == origen)
            }) else { return }
            a.conexiones.append(ConexionMapaMentalIPad(origenId: origen,
                                                       destinoId: destino))
            archivos[documento] = a
            persistir(a, documento: documento)
        }
    }

    func desconectar(_ id: UUID, documento: UInt64) {
        cola.sync {
            var a = cargar(documento)
            a.conexiones.removeAll { $0.id == id }
            archivos[documento] = a
            persistir(a, documento: documento)
        }
    }

    private func cargar(_ documento: UInt64) -> ArchivoNotasAdhesivasIPad {
        if let cached = archivos[documento] { return cached }
        let url = urlArchivo(documento)
        let a = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ArchivoNotasAdhesivasIPad.self, from: $0)
        } ?? ArchivoNotasAdhesivasIPad()
        archivos[documento] = a
        return a
    }

    private func persistir(_ a: ArchivoNotasAdhesivasIPad, documento: UInt64) {
        guard let datos = try? JSONEncoder().encode(a) else { return }
        try? datos.write(to: urlArchivo(documento),
                         options: [.atomic, .completeFileProtection])
    }
}

final class CapaNotasAdhesivasIPad: UIView {
    var documentoId: UInt64 = 0 { didSet { recargar() } }
    var alSeleccionar: ((NotaAdhesivaLienzo) -> Void)?
    var escala: CGFloat = 1 { didSet { setNeedsDisplay() } }
    var desplazamiento: CGPoint = .zero { didSet { setNeedsDisplay() } }

    private let repo = RepositorioNotasAdhesivasIPad.compartido
    private var notas: [NotaAdhesivaLienzo] = []
    private var conexiones: [ConexionMapaMentalIPad] = []
    private var vistaActiva: UIView?

    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = false
        backgroundColor = .clear
        clipsToBounds = false
        isUserInteractionEnabled = true
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        guard !isHidden, alpha > 0.01, isUserInteractionEnabled else { return nil }
        for vista in subviews.reversed() {
            let local = vista.convert(point, from: self)
            if let resultado = vista.hitTest(local, with: event) { return resultado }
        }
        return nil
    }

    func recargar() {
        notas = repo.notas(documentoId)
        conexiones = repo.conexiones(documentoId)
        reconstruir()
    }

    @discardableResult
    func agregar(_ tipo: TipoNotaAdhesivaIPad,
                 en posicion: CGPoint) -> NotaAdhesivaLienzo {
        var nota = NotaAdhesivaLienzo()
        nota.tipo = tipo
        nota.posicion = posicion
        nota.exportable = (tipo != .comentario)
        nota.zIndice = (notas.map(\.zIndice).max() ?? 0) + 1
        repo.agregar(nota, documento: documentoId)
        notas.append(nota)
        agregarVistaParaNota(nota)
        return nota
    }

    override func draw(_ rect: CGRect) {
        guard let ctx = UIGraphicsGetCurrentContext() else { return }
        ctx.setStrokeColor(UIColor.systemBlue.withAlphaComponent(0.5).cgColor)
        ctx.setLineWidth(2 / escala)

        for conexion in conexiones {
            guard let origen = notas.first(where: { $0.id == conexion.origenId }),
                  let destino = notas.first(where: { $0.id == conexion.destinoId }) else {
                continue
            }
            let pOrigen = puntoEnPantalla(origen.posicion)
            let pDestino = puntoEnPantalla(destino.posicion)
            if conexion.estiloCurva {
                let cpx = (pOrigen.x + pDestino.x) / 2
                ctx.move(to: pOrigen)
                ctx.addCurve(to: pDestino,
                             control1: CGPoint(x: cpx, y: pOrigen.y),
                             control2: CGPoint(x: cpx, y: pDestino.y))
            } else {
                ctx.move(to: pOrigen)
                ctx.addLine(to: pDestino)
            }
            ctx.strokePath()

            if !conexion.etiqueta.isEmpty {
                let medio = CGPoint(x: (pOrigen.x + pDestino.x) / 2,
                                    y: (pOrigen.y + pDestino.y) / 2)
                (conexion.etiqueta as NSString).draw(
                    at: medio,
                    withAttributes: [
                        .font: UIFont.systemFont(ofSize: 11 / escala),
                        .foregroundColor: UIColor.secondaryLabel,
                    ])
            }
        }

        ctx.setStrokeColor(UIColor.systemOrange.withAlphaComponent(0.7).cgColor)
        for nota in notas where nota.tipo == .callout {
            guard let punto = nota.puntoCallout else { continue }
            let pNota = puntoEnPantalla(nota.posicion)
            let pTarget = puntoEnPantalla(punto)
            ctx.move(to: pNota)
            ctx.addLine(to: pTarget)
            ctx.strokePath()

            let angulo = atan2(pTarget.y - pNota.y, pTarget.x - pNota.x)
            let longitud: CGFloat = 10 / escala
            let p1 = CGPoint(x: pTarget.x - longitud * cos(angulo - .pi / 6),
                             y: pTarget.y - longitud * sin(angulo - .pi / 6))
            let p2 = CGPoint(x: pTarget.x - longitud * cos(angulo + .pi / 6),
                             y: pTarget.y - longitud * sin(angulo + .pi / 6))
            ctx.move(to: pTarget)
            ctx.addLine(to: p1)
            ctx.move(to: pTarget)
            ctx.addLine(to: p2)
            ctx.strokePath()
        }
    }

    private func puntoEnPantalla(_ p: CGPoint) -> CGPoint {
        CGPoint(x: (p.x + desplazamiento.x) * escala,
                y: (p.y + desplazamiento.y) * escala)
    }

    private func reconstruir() {
        subviews.forEach { $0.removeFromSuperview() }
        for nota in notas.sorted(by: { $0.zIndice < $1.zIndice }) {
            agregarVistaParaNota(nota)
        }
        setNeedsDisplay()
    }

    private func agregarVistaParaNota(_ nota: NotaAdhesivaLienzo) {
        let vista = VistaNota(nota: nota)
        vista.alMover = { [weak self] nuevaPos in
            guard let self,
                  let i = self.notas.firstIndex(where: { $0.id == nota.id }) else {
                return
            }
            self.notas[i].posicion = nuevaPos
            self.repo.actualizar(self.notas[i], documento: self.documentoId)
            self.setNeedsDisplay()
        }
        vista.alTocar = { [weak self] in
            guard let self,
                  let actual = self.notas.first(where: { $0.id == nota.id }) else {
                return
            }
            self.alSeleccionar?(actual)
        }
        vista.alColapsar = { [weak self] colapsada in
            guard let self,
                  let i = self.notas.firstIndex(where: { $0.id == nota.id }) else {
                return
            }
            self.notas[i].estado = colapsada ? .colapsada : .expandida
            self.repo.actualizar(self.notas[i], documento: self.documentoId)
        }
        addSubview(vista)
    }
}

private final class VistaNota: UIView {
    var alMover: ((CGPoint) -> Void)?
    var alTocar: (() -> Void)?
    var alColapsar: ((Bool) -> Void)?

    private var nota: NotaAdhesivaLienzo
    private let etiqueta = UILabel()
    private let indicadorColapso = UIButton(type: .system)

    init(nota: NotaAdhesivaLienzo) {
        self.nota = nota
        super.init(frame: CGRect(origin: nota.posicion, size: nota.tamano))
        backgroundColor = nota.color.withAlphaComponent(0.9)
        layer.cornerRadius = 8
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOpacity = 0.15
        layer.shadowRadius = 6
        layer.shadowOffset = CGSize(width: 0, height: 3)

        if nota.tipo == .comentario {
            layer.borderColor = UIColor.systemPurple.withAlphaComponent(0.4).cgColor
            layer.borderWidth = 1.5
        }

        etiqueta.text = nota.texto.isEmpty
            ? (nota.tipo == .comentario ? "Comentario…" : "Nota…")
            : nota.texto
        etiqueta.numberOfLines = 0
        etiqueta.font = .systemFont(ofSize: 13)
        etiqueta.textColor = .label
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        addSubview(etiqueta)

        indicadorColapso.setImage(
            UIImage(systemName: nota.estado == .colapsada
                    ? "chevron.down.circle" : "chevron.up.circle"),
            for: .normal)
        indicadorColapso.translatesAutoresizingMaskIntoConstraints = false
        indicadorColapso.addAction(UIAction { [weak self] _ in
            guard let self else { return }
            let colapsada = self.nota.estado != .colapsada
            self.nota.estado = colapsada ? .colapsada : .expandida
            self.aplicarEstado()
            self.alColapsar?(colapsada)
        }, for: .touchUpInside)
        addSubview(indicadorColapso)

        NSLayoutConstraint.activate([
            indicadorColapso.topAnchor.constraint(equalTo: topAnchor, constant: 4),
            indicadorColapso.trailingAnchor.constraint(equalTo: trailingAnchor,
                                                        constant: -4),
            indicadorColapso.widthAnchor.constraint(equalToConstant: 24),
            indicadorColapso.heightAnchor.constraint(equalToConstant: 24),
            etiqueta.topAnchor.constraint(equalTo: topAnchor, constant: 8),
            etiqueta.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
            etiqueta.trailingAnchor.constraint(equalTo: indicadorColapso.leadingAnchor,
                                                constant: -4),
            etiqueta.bottomAnchor.constraint(lessThanOrEqualTo: bottomAnchor,
                                              constant: -8),
        ])

        let pan = UIPanGestureRecognizer(target: self, action: #selector(mover(_:)))
        addGestureRecognizer(pan)
        let toque = UITapGestureRecognizer(target: self, action: #selector(tocar))
        addGestureRecognizer(toque)
        aplicarEstado()
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    private func aplicarEstado() {
        let colapsada = nota.estado == .colapsada
        UIView.animate(withDuration: 0.25) {
            self.etiqueta.alpha = colapsada ? 0 : 1
            let lado: CGFloat = colapsada ? 20 : self.nota.tamano.width
            let alto: CGFloat = colapsada ? 20 : self.nota.tamano.height
            self.bounds = CGRect(x: 0, y: 0, width: lado, height: alto)
            self.layer.cornerRadius = colapsada ? 10 : 8
        }
        indicadorColapso.setImage(
            UIImage(systemName: colapsada ? "chevron.down.circle"
                    : "chevron.up.circle"), for: .normal)
    }

    @objc private func mover(_ gesto: UIPanGestureRecognizer) {
        let traslacion = gesto.translation(in: superview)
        center = CGPoint(x: center.x + traslacion.x, y: center.y + traslacion.y)
        gesto.setTranslation(.zero, in: superview)
        if gesto.state == .ended {
            nota.posicion = frame.origin
            alMover?(nota.posicion)
        }
    }

    @objc private func tocar() { alTocar?() }
}

enum LayoutMapaMentalIPad {
    static func organizar(_ notas: inout [NotaAdhesivaLienzo],
                          conexiones: [ConexionMapaMentalIPad],
                          centro: CGPoint) {
        guard let raiz = notas.first(where: { $0.tipo == .nodoMapa }) else { return }
        var visitados = Set<UUID>()
        var cola: [(UUID, CGFloat, CGFloat)] = [(raiz.id, 0, 2 * .pi)]
        visitados.insert(raiz.id)

        if let i = notas.firstIndex(where: { $0.id == raiz.id }) {
            notas[i].posicion = centro
        }

        let radioBase: CGFloat = 200
        var nivel = 0
        while !cola.isEmpty {
            nivel += 1
            var siguienteNivel: [(UUID, CGFloat, CGFloat)] = []
            for (padreId, anguloInicio, anguloFin) in cola {
                let hijos = conexiones
                    .filter { $0.origenId == padreId || $0.destinoId == padreId }
                    .compactMap { c -> UUID? in
                        let otro = c.origenId == padreId ? c.destinoId : c.origenId
                        return visitados.contains(otro) ? nil : otro
                    }
                guard !hijos.isEmpty else { continue }
                let anguloSlice = (anguloFin - anguloInicio) / CGFloat(hijos.count)
                for (j, hijoId) in hijos.enumerated() {
                    visitados.insert(hijoId)
                    let angulo = anguloInicio + anguloSlice * (CGFloat(j) + 0.5)
                    let radio = radioBase * CGFloat(nivel)
                    let pos = CGPoint(x: centro.x + radio * cos(angulo),
                                      y: centro.y + radio * sin(angulo))
                    if let i = notas.firstIndex(where: { $0.id == hijoId }) {
                        notas[i].posicion = pos
                    }
                    siguienteNivel.append((hijoId,
                                           angulo - anguloSlice / 2,
                                           angulo + anguloSlice / 2))
                }
            }
            cola = siguienteNivel
        }
    }
}
