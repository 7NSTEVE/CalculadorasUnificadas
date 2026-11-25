import PDFKit
import UIKit

struct EstadisticasCachePDFExportacionIPad: Equatable {
    var hitsImagen: UInt64
    var missesImagen: UInt64
    var documentosAbiertos: UInt64
}

/// Caché acotado para rutas de exportación/impresión. A diferencia del caché de
/// previews UI, ésta es una API síncrona porque UIGraphicsPDFRenderer también
/// recorre el documento síncronamente. Evita reabrir el mismo PDF por cada
/// objeto incrustado y reutiliza rasterizaciones repetidas del mismo tamaño.
final class CachePDFExportacionIPad {
    static let compartido = CachePDFExportacionIPad()

    private let documentos = NSCache<NSString, PDFDocument>()
    private let imagenes = NSCache<NSString, UIImage>()
    private let bloqueo = NSRecursiveLock()
    private var hitsImagen: UInt64 = 0
    private var missesImagen: UInt64 = 0
    private var documentosAbiertos: UInt64 = 0
    private var observadorMemoria: NSObjectProtocol?

    private init() {
        documentos.countLimit = 8
        imagenes.countLimit = 160
        imagenes.totalCostLimit = 128 * 1024 * 1024
        observadorMemoria = NotificationCenter.default.addObserver(
            forName: UIApplication.didReceiveMemoryWarningNotification,
            object: nil, queue: .main) { [weak self] _ in
                self?.vaciar()
            }
    }

    deinit {
        if let observadorMemoria { NotificationCenter.default.removeObserver(observadorMemoria) }
    }

    func imagen(
        url: URL,
        pagina indice: Int,
        tamano: CGSize,
        caja: PDFDisplayBox = .mediaBox
    ) -> UIImage? {
        guard indice >= 0,
              tamano.width.isFinite, tamano.height.isFinite,
              tamano.width > 0, tamano.height > 0 else { return nil }
        let objetivo = Self.cuantizar(tamano)
        let firma = Self.firma(url)
        let claveImagen = "\(firma)|p:\(indice)|w:\(Int(objetivo.width))|h:\(Int(objetivo.height))|b:\(caja.rawValue)" as NSString

        bloqueo.lock(); defer { bloqueo.unlock() }
        if let imagen = imagenes.object(forKey: claveImagen) {
            hitsImagen &+= 1
            return imagen
        }
        missesImagen &+= 1
        guard let documento = documento(url: url, firma: firma),
              indice < documento.pageCount,
              let pagina = documento.page(at: indice) else { return nil }
        let imagen = pagina.thumbnail(of: objetivo, for: caja)
        guard imagen.size.width > 0, imagen.size.height > 0 else { return nil }
        let pixeles = max(Int(imagen.size.width * imagen.scale), 1)
            * max(Int(imagen.size.height * imagen.scale), 1)
        imagenes.setObject(imagen, forKey: claveImagen,
                           cost: min(pixeles * 4, Int.max / 4))
        return imagen
    }

    func estadisticas() -> EstadisticasCachePDFExportacionIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return .init(hitsImagen: hitsImagen,
                     missesImagen: missesImagen,
                     documentosAbiertos: documentosAbiertos)
    }

    func reiniciarEstadisticas() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        hitsImagen = 0; missesImagen = 0; documentosAbiertos = 0
    }

    func vaciar() {
        bloqueo.lock(); defer { bloqueo.unlock() }
        documentos.removeAllObjects()
        imagenes.removeAllObjects()
    }

    private func documento(url: URL, firma: String) -> PDFDocument? {
        let clave = firma as NSString
        if let existente = documentos.object(forKey: clave) { return existente }
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        guard let nuevo = PDFDocument(url: url) else { return nil }
        documentos.setObject(nuevo, forKey: clave)
        documentosAbiertos &+= 1
        return nuevo
    }

    private static func firma(_ url: URL) -> String {
        let normal = url.standardizedFileURL
        let atributos = try? FileManager.default.attributesOfItem(atPath: normal.path)
        let bytes = (atributos?[.size] as? NSNumber)?.int64Value ?? -1
        let fecha = (atributos?[.modificationDate] as? Date)?.timeIntervalSince1970 ?? -1
        return "\(normal.path)|\(bytes)|\(fecha)"
    }

    private static func cuantizar(_ tamano: CGSize) -> CGSize {
        func q(_ valor: CGFloat) -> CGFloat {
            let limitado = min(max(valor, 64), 4096)
            return ceil(limitado / 32) * 32
        }
        return CGSize(width: q(tamano.width), height: q(tamano.height))
    }
}
