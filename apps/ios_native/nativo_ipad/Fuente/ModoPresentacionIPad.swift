import Foundation
import UIKit

struct PuntoPresentadorIPad: Codable, Equatable {
    var xNormalizado: CGFloat
    var yNormalizado: CGFloat
    var visible: Bool

    init(xNormalizado: CGFloat = 0.5,
         yNormalizado: CGFloat = 0.5,
         visible: Bool = false) {
        self.xNormalizado = min(max(xNormalizado, 0), 1)
        self.yNormalizado = min(max(yNormalizado, 0), 1)
        self.visible = visible
    }
}

struct EstadoPresentacionIPad: Codable, Equatable {
    var version = 1
    var documentoId: DocumentoPersistenteID
    var pagina: Int?
    var frameId: UUID?
    var pantallaNegra = false
    var ocultarInterfazAudiencia = true
    var mostrarPuntero = true
    var puntero = PuntoPresentadorIPad()
    var notasPrivadas = ""
    var iniciada = Date()
    var actualizada = Date()

    mutating func ir(pagina nuevaPagina: Int?) {
        pagina = nuevaPagina.map { max($0, 0) }
        actualizada = Date()
    }

    mutating func ir(frameId nuevoFrame: UUID?) {
        frameId = nuevoFrame
        actualizada = Date()
    }

    mutating func moverPuntero(a punto: CGPoint, en tamano: CGSize) {
        guard tamano.width > 0, tamano.height > 0 else { return }
        puntero = PuntoPresentadorIPad(
            xNormalizado: punto.x / tamano.width,
            yNormalizado: punto.y / tamano.height,
            visible: mostrarPuntero)
        actualizada = Date()
    }

    mutating func alternarNegro() {
        pantallaNegra.toggle()
        actualizada = Date()
    }
}

enum ErrorPresentacionIPad: LocalizedError {
    case sesionInexistente
    case documentoDistinto

    var errorDescription: String? {
        switch self {
        case .sesionInexistente:
            return "No hay una presentación activa."
        case .documentoDistinto:
            return "La presentación activa pertenece a otro documento."
        }
    }
}

private struct ArchivoPresentacionIPad: Codable {
    var version = 1
    var ultimaSesion: EstadoPresentacionIPad?
}

final class CoordinadorPresentacionIPad {
    static let compartido = CoordinadorPresentacionIPad()

    static let cambio = Notification.Name("CuadernU.Presentacion.Cambio")
    static let finalizo = Notification.Name("CuadernU.Presentacion.Finalizo")

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var activa: EstadoPresentacionIPad?

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("Presentacion", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("ultima-sesion.json")
        if let datos = try? Data(contentsOf: url),
           let archivo = try? JSONDecoder.presentacion.decode(ArchivoPresentacionIPad.self,
                                                               from: datos) {
            activa = archivo.ultimaSesion
        }
    }

    func estado() -> EstadoPresentacionIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return activa
    }

    @discardableResult
    func iniciar(documentoId: DocumentoPersistenteID,
                 pagina: Int? = nil,
                 frameId: UUID? = nil) -> EstadoPresentacionIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var estado = EstadoPresentacionIPad(documentoId: documentoId)
        estado.pagina = pagina.map { max($0, 0) }
        estado.frameId = frameId
        activa = estado
        guardarSinBloqueo()
        publicar(estado)
        return estado
    }

    @discardableResult
    func mutar(_ cuerpo: (inout EstadoPresentacionIPad) -> Void) throws
        -> EstadoPresentacionIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard var estado = activa else { throw ErrorPresentacionIPad.sesionInexistente }
        cuerpo(&estado)
        estado.actualizada = Date()
        activa = estado
        guardarSinBloqueo()
        publicar(estado)
        return estado
    }

    func asegurar(documentoId: DocumentoPersistenteID) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard let activa else { throw ErrorPresentacionIPad.sesionInexistente }
        guard activa.documentoId == documentoId else {
            throw ErrorPresentacionIPad.documentoDistinto
        }
    }

    func finalizar() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        activa = nil
        guardarSinBloqueo()
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: Self.finalizo, object: self)
        }
    }

    private func publicar(_ estado: EstadoPresentacionIPad) {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: Self.cambio,
                                            object: self,
                                            userInfo: ["estado": estado])
        }
    }

    private func guardarSinBloqueo() {
        let encoder = JSONEncoder.presentacion
        guard let datos = try? encoder.encode(ArchivoPresentacionIPad(ultimaSesion: activa)) else {
            return
        }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

private extension JSONEncoder {
    static var presentacion: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }
}

private extension JSONDecoder {
    static var presentacion: JSONDecoder {
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return decoder
    }
}

/// Contrato entre el modo presentador y la superficie que realmente sabe dibujar
/// notebook/PDF/whiteboard. Evita que este módulo capture screenshots o duplique Canvas.
protocol FuenteVisualPresentacionIPad: AnyObject {
    func renderizarPresentacion(_ estado: EstadoPresentacionIPad,
                                en vista: UIView)
}

final class VistaAudienciaPresentacionIPad: UIView {
    weak var fuente: FuenteVisualPresentacionIPad?
    private var estado: EstadoPresentacionIPad?
    private let negro = UIView()
    private let puntero = UIView()

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .black
        negro.backgroundColor = .black
        negro.isUserInteractionEnabled = false
        puntero.backgroundColor = .systemRed
        puntero.layer.cornerRadius = 7
        puntero.layer.shadowOpacity = 0.45
        puntero.layer.shadowRadius = 5
        puntero.isUserInteractionEnabled = false
        addSubview(negro)
        addSubview(puntero)
        isAccessibilityElement = true
        accessibilityLabel = "Contenido de presentación"
        NotificationCenter.default.addObserver(
            self, selector: #selector(cambio(_:)), name: CoordinadorPresentacionIPad.cambio,
            object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(finalizo), name: CoordinadorPresentacionIPad.finalizo,
            object: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    deinit { NotificationCenter.default.removeObserver(self) }

    override func layoutSubviews() {
        super.layoutSubviews()
        negro.frame = bounds
        actualizarPuntero()
        if let estado { fuente?.renderizarPresentacion(estado, en: self) }
    }

    @objc private func cambio(_ nota: Notification) {
        guard let nuevo = nota.userInfo?["estado"] as? EstadoPresentacionIPad else { return }
        estado = nuevo
        negro.isHidden = !nuevo.pantallaNegra
        puntero.isHidden = nuevo.pantallaNegra || !nuevo.mostrarPuntero || !nuevo.puntero.visible
        actualizarPuntero()
        setNeedsLayout()
    }

    @objc private func finalizo() {
        estado = nil
        negro.isHidden = false
        puntero.isHidden = true
        setNeedsLayout()
    }

    private func actualizarPuntero() {
        guard let estado else { return }
        let x = bounds.width * estado.puntero.xNormalizado
        let y = bounds.height * estado.puntero.yNormalizado
        puntero.frame = CGRect(x: x - 7, y: y - 7, width: 14, height: 14)
    }
}
