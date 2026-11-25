import Foundation
import UIKit

struct CursorRemotoCuadernUIPad: Codable, Equatable, Identifiable {
    var id: String { autorId }
    var autorId: String
    var nombre: String
    var documentoId: DocumentoPersistenteID
    var xNormalizado: CGFloat
    var yNormalizado: CGFloat
    var colorARGB: UInt32
    var seleccion: CGRect?
    var actualizado = Date()

    init(autorId: String, nombre: String,
         documentoId: DocumentoPersistenteID,
         punto: CGPoint, tamano: CGSize,
         colorARGB: UInt32, seleccion: CGRect? = nil) {
        self.autorId = autorId; self.nombre = nombre; self.documentoId = documentoId
        if tamano.width > 0, tamano.height > 0 {
            xNormalizado = min(max(punto.x / tamano.width, 0), 1)
            yNormalizado = min(max(punto.y / tamano.height, 0), 1)
        } else { xNormalizado = 0.5; yNormalizado = 0.5 }
        self.colorARGB = colorARGB; self.seleccion = seleccion
    }

    var puntoNormalizado: CGPoint { CGPoint(x: xNormalizado, y: yNormalizado) }
}

final class CoordinadorPresenciaColaborativaIPad {
    static let cambio = Notification.Name("CuadernU.Colaboracion.Presencia.Cambio")
    private let documentoId: DocumentoPersistenteID
    private let servicio: ServicioColaboracionLocal
    private let bloqueo = NSRecursiveLock()
    private var remotos: [String: CursorRemotoCuadernUIPad] = [:]
    private var observador: NSObjectProtocol?
    var ttl: TimeInterval = 12

    init(documentoId: DocumentoPersistenteID,
         servicio: ServicioColaboracionLocal = .compartido,
         observar: Bool = true) {
        self.documentoId = documentoId; self.servicio = servicio
        if observar {
            observador = NotificationCenter.default.addObserver(
                forName: .operacionColaborativa, object: nil, queue: .main) {
                    [weak self] nota in
                    guard let op = nota.object as? OperacionColaborativa else { return }
                    self?.procesar(op)
                }
        }
    }

    deinit { if let observador { NotificationCenter.default.removeObserver(observador) } }

    @discardableResult
    func publicar(nombre: String,
                  punto: CGPoint,
                  tamano: CGSize,
                  colorARGB: UInt32,
                  seleccion: CGRect? = nil) -> OperacionColaborativa? {
        guard servicio.invitacion?.documentoId == documentoId.claveCompatibilidadUInt64 else {
            return nil
        }
        let cursor = CursorRemotoCuadernUIPad(
            autorId: RelojLamportColaboracion.compartido.autor,
            nombre: nombre,
            documentoId: documentoId,
            punto: punto, tamano: tamano,
            colorARGB: colorARGB, seleccion: seleccion)
        guard let datos = try? JSONEncoder.presencia.encode(cursor) else { return nil }
        return servicio.publicar(tipo: .cursor,
                                 objetoId: "cursor:\(cursor.autorId)", carga: datos)
    }

    func procesar(_ operacion: OperacionColaborativa, ahora: Date = Date()) {
        guard operacion.tipo == .cursor,
              operacion.documentoId == documentoId.claveCompatibilidadUInt64,
              let datos = operacion.carga,
              var cursor = try? JSONDecoder.presencia.decode(CursorRemotoCuadernUIPad.self,
                                                              from: datos),
              cursor.documentoId == documentoId,
              cursor.autorId == operacion.marca.autor else { return }
        cursor.actualizado = ahora
        bloqueo.lock(); remotos[cursor.autorId] = cursor; depurarSinBloqueo(ahora: ahora); bloqueo.unlock()
        publicarCambio()
    }

    func cursores(ahora: Date = Date()) -> [CursorRemotoCuadernUIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        depurarSinBloqueo(ahora: ahora)
        return remotos.values.sorted { $0.nombre.localizedCaseInsensitiveCompare($1.nombre) == .orderedAscending }
    }

    func limpiar() {
        bloqueo.lock(); remotos.removeAll(); bloqueo.unlock(); publicarCambio()
    }

    private func depurarSinBloqueo(ahora: Date) {
        remotos = remotos.filter { ahora.timeIntervalSince($0.value.actualizado) <= max(ttl, 1) }
    }
    private func publicarCambio() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: Self.cambio, object: self)
        }
    }
}

private extension JSONEncoder {
    static var presencia: JSONEncoder {
        let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;return e
    }
}
private extension JSONDecoder {
    static var presencia: JSONDecoder {
        let d=JSONDecoder();d.dateDecodingStrategy = .iso8601;return d
    }
}

/// Overlay reusable; Canvas sólo debe alimentarlo con bounds/transform correctos.
final class VistaCursoresRemotosIPad: UIView {
    var cursores: [CursorRemotoCuadernUIPad] = [] { didSet { setNeedsDisplay() } }
    override init(frame: CGRect) { super.init(frame: frame); isOpaque = false; backgroundColor = .clear; isUserInteractionEnabled = false; accessibilityElementsHidden = true }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func draw(_ rect: CGRect) {
        guard let c=UIGraphicsGetCurrentContext() else{return}
        for cursor in cursores {
            let punto=CGPoint(x:bounds.width*cursor.xNormalizado,y:bounds.height*cursor.yNormalizado)
            let color=UIColor(argbPresencia:cursor.colorARGB)
            c.setFillColor(color.cgColor);c.fillEllipse(in:CGRect(x:punto.x-6,y:punto.y-6,width:12,height:12))
            let texto=cursor.nombre as NSString
            texto.draw(at:CGPoint(x:punto.x+10,y:punto.y-9),withAttributes:[.font:UIFont.preferredFont(forTextStyle:.caption1),.foregroundColor:color])
        }
    }
}

private extension UIColor {
    convenience init(argbPresencia v:UInt32){self.init(red:CGFloat((v>>16)&0xFF)/255,green:CGFloat((v>>8)&0xFF)/255,blue:CGFloat(v&0xFF)/255,alpha:CGFloat((v>>24)&0xFF)/255)}
}
