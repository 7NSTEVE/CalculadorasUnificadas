import Foundation
import PDFKit
import UIKit

struct MetricasMiniaturasPDFIPad: Equatable {
    var aciertosCache = 0
    var fallosCache = 0
    var solicitudesCoalescidas = 0
    var rendersIniciados = 0
    var rendersCompletados = 0
    var rendersFallidos = 0
}

/// Token de una solicitud de preview. Cancelarlo retira solamente ese consumidor;
/// si otra vista pidió la misma página, el render compartido continúa y alimenta
/// la caché para ambas rutas. Liberar el token NO cancela implícitamente: así una
/// llamada fire-and-forget sigue recibiendo completion aunque no conserve token.
final class TokenMiniaturaPDFIPad {
    private let cancelarImpl: () -> Void
    private let bloqueo = NSLock()
    private var cancelado = false

    init(cancelar: @escaping () -> Void) {
        cancelarImpl = cancelar
    }

    func cancelar() {
        bloqueo.lock()
        guard !cancelado else { bloqueo.unlock(); return }
        cancelado = true
        bloqueo.unlock()
        cancelarImpl()
    }
}

private struct ClaveMiniaturaPDFIPad: Hashable {
    var ruta: String
    var pagina: Int
    var anchoPx: Int
    var altoPx: Int
    var firmaArchivo: String
    var caja: Int

    var cacheKey: NSString {
        "\(ruta)|\(pagina)|\(anchoPx)x\(altoPx)|\(firmaArchivo)|\(caja)" as NSString
    }
}

private struct SolicitudRenderPDFIPad {
    var url: URL
    var pagina: Int
    var tamanoPuntos: CGSize
    var caja: PDFDisplayBox
}

/// Renderizador compartido para previews de PDF de documentos grandes.
///
/// Propiedades importantes para rendimiento:
/// - jamás abre PDFDocument ni rasteriza una página en el main thread;
/// - deduplica solicitudes simultáneas de la misma página/tamaño;
/// - limita la concurrencia para evitar varios bitmaps grandes a la vez;
/// - NSCache usa coste aproximado RGBA y responde a memory warning;
/// - cuantiza tamaños para que pequeños cambios de layout reutilicen preview.
final class RenderizadorMiniaturasPDFIPad {
    static let compartido = RenderizadorMiniaturasPDFIPad()

    typealias MotorRender = (URL, Int, CGSize, PDFDisplayBox) -> UIImage?

    private final class PeticionEnVuelo {
        var callbacks: [UUID: (UIImage?) -> Void] = [:]
    }

    private let cache = NSCache<NSString, UIImage>()
    private let cola: OperationQueue
    private let bloqueo = NSLock()
    private var enVuelo: [ClaveMiniaturaPDFIPad: PeticionEnVuelo] = [:]
    private var metricasInternas = MetricasMiniaturasPDFIPad()
    private let motor: MotorRender
    private let centroNotificaciones: NotificationCenter
    private var observadorMemoria: NSObjectProtocol?

    /// 96 MiB es suficientemente grande para previews visibles y suficientemente
    /// pequeño para no competir con Metal, tinta, imágenes importadas y PDFKit.
    /// En aparatos con poca memoria se reduce de forma conservadora.
    init(
        limiteCacheBytes: Int? = nil,
        concurrencia: Int = 2,
        centroNotificaciones: NotificationCenter = .default,
        motor: @escaping MotorRender = RenderizadorMiniaturasPDFIPad.renderPorDefecto
    ) {
        self.motor = motor
        self.centroNotificaciones = centroNotificaciones
        cola = OperationQueue()
        cola.name = "cuadernu.pdf.preview"
        cola.qualityOfService = .userInitiated
        cola.maxConcurrentOperationCount = max(1, min(concurrencia, 3))

        let memoria = ProcessInfo.processInfo.physicalMemory
        let minCacheBytes: UInt64 = 32 * 1_024 * 1_024
        let maxCacheBytes: UInt64 = 96 * 1_024 * 1_024
        let adaptativo = Int(min(max(memoria / 32, minCacheBytes), maxCacheBytes))
        cache.totalCostLimit = max(limiteCacheBytes ?? adaptativo, 8 * 1_024 * 1_024)
        cache.countLimit = 192

        observadorMemoria = centroNotificaciones.addObserver(
            forName: UIApplication.didReceiveMemoryWarningNotification,
            object: nil,
            queue: nil
        ) { [weak self] _ in
            self?.vaciarCache()
        }
    }

    deinit {
        if let observadorMemoria {
            centroNotificaciones.removeObserver(observadorMemoria)
        }
    }

    var metricas: MetricasMiniaturasPDFIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return metricasInternas
    }

    var limiteCacheBytes: Int { cache.totalCostLimit }

    func vaciarCache() {
        cache.removeAllObjects()
    }

    /// Solicita una miniatura sin bloquear la UI. El completion siempre vuelve
    /// al main thread y puede recibir nil si el PDF desapareció o la página ya
    /// no existe.
    @discardableResult
    func solicitar(
        url: URL,
        pagina: Int,
        tamano: CGSize,
        escala: CGFloat = UIScreen.main.scale,
        caja: PDFDisplayBox = .mediaBox,
        completion: @escaping (UIImage?) -> Void
    ) -> TokenMiniaturaPDFIPad {
        let id = UUID()
        guard pagina >= 0, tamano.width > 0, tamano.height > 0 else {
            DispatchQueue.main.async { completion(nil) }
            return TokenMiniaturaPDFIPad(cancelar: {})
        }

        let normalizado = Self.normalizar(tamano: tamano, escala: escala)
        let firma = Self.firmaArchivo(url)
        let clave = ClaveMiniaturaPDFIPad(
            ruta: url.standardizedFileURL.path,
            pagina: pagina,
            anchoPx: normalizado.anchoPx,
            altoPx: normalizado.altoPx,
            firmaArchivo: firma,
            caja: caja.rawValue)

        if let existente = cache.object(forKey: clave.cacheKey) {
            bloqueo.lock(); metricasInternas.aciertosCache += 1; bloqueo.unlock()
            DispatchQueue.main.async { completion(existente) }
            return TokenMiniaturaPDFIPad(cancelar: {})
        }

        var iniciarRender = false
        bloqueo.lock()
        metricasInternas.fallosCache += 1
        if let peticion = enVuelo[clave] {
            peticion.callbacks[id] = completion
            metricasInternas.solicitudesCoalescidas += 1
        } else {
            let peticion = PeticionEnVuelo()
            peticion.callbacks[id] = completion
            enVuelo[clave] = peticion
            metricasInternas.rendersIniciados += 1
            iniciarRender = true
        }
        bloqueo.unlock()

        if iniciarRender {
            let solicitud = SolicitudRenderPDFIPad(
                url: url,
                pagina: pagina,
                tamanoPuntos: normalizado.tamanoPuntos,
                caja: caja)
            cola.addOperation { [weak self] in
                guard let self else { return }
                let imagen = autoreleasepool {
                    self.motor(solicitud.url, solicitud.pagina,
                               solicitud.tamanoPuntos, solicitud.caja)
                }
                if let imagen {
                    self.cache.setObject(
                        imagen,
                        forKey: clave.cacheKey,
                        cost: Self.costeAproximado(imagen))
                }

                self.bloqueo.lock()
                let callbacks = self.enVuelo.removeValue(forKey: clave)?
                    .callbacks.values.map { $0 } ?? []
                if imagen == nil { self.metricasInternas.rendersFallidos += 1 }
                else { self.metricasInternas.rendersCompletados += 1 }
                self.bloqueo.unlock()

                guard !callbacks.isEmpty else { return }
                DispatchQueue.main.async {
                    callbacks.forEach { $0(imagen) }
                }
            }
        }

        return TokenMiniaturaPDFIPad { [weak self] in
            self?.cancelar(id: id, clave: clave)
        }
    }

    /// Prefetch acotado alrededor de la página visible. El llamador decide el
    /// radio; se limita aquí para impedir que un scroll rápido rasterice cientos
    /// de páginas que nunca llegaron a pantalla.
    @discardableResult
    func precargar(
        url: URL,
        paginas: [Int],
        tamano: CGSize,
        escala: CGFloat = UIScreen.main.scale,
        caja: PDFDisplayBox = .mediaBox
    ) -> [TokenMiniaturaPDFIPad] {
        Array(Set(paginas.filter { $0 >= 0 })).sorted().prefix(8).map {
            solicitar(url: url, pagina: $0, tamano: tamano,
                      escala: escala, caja: caja) { _ in }
        }
    }

    private func cancelar(id: UUID, clave: ClaveMiniaturaPDFIPad) {
        bloqueo.lock()
        enVuelo[clave]?.callbacks.removeValue(forKey: id)
        bloqueo.unlock()
    }

    private static func normalizar(tamano: CGSize, escala: CGFloat)
        -> (anchoPx: Int, altoPx: Int, tamanoPuntos: CGSize) {
        let s = max(escala, 1)
        let maxLadoPx: CGFloat = 1_600
        var ancho = max(tamano.width * s, 64)
        var alto = max(tamano.height * s, 64)
        let factor = min(1, maxLadoPx / max(ancho, alto))
        ancho *= factor; alto *= factor

        // Bloques de 64 px: un resize de pocos puntos no destruye la tasa de hit.
        let anchoPx = max(64, Int(ceil(ancho / 64) * 64))
        let altoPx = max(64, Int(ceil(alto / 64) * 64))
        return (anchoPx, altoPx,
                CGSize(width: CGFloat(anchoPx) / s,
                       height: CGFloat(altoPx) / s))
    }

    private static func firmaArchivo(_ url: URL) -> String {
        guard let atributos = try? FileManager.default.attributesOfItem(atPath: url.path) else {
            return "sin-meta"
        }
        let bytes = (atributos[.size] as? NSNumber)?.int64Value ?? -1
        let fecha = (atributos[.modificationDate] as? Date)?.timeIntervalSince1970 ?? -1
        return "\(bytes):\(Int64(fecha * 1_000))"
    }

    private static func costeAproximado(_ imagen: UIImage) -> Int {
        guard let cg = imagen.cgImage else {
            return max(Int(imagen.size.width * imagen.size.height * imagen.scale * 4), 1)
        }
        return max(cg.bytesPerRow * cg.height, 1)
    }

    private static func renderPorDefecto(
        _ url: URL,
        _ pagina: Int,
        _ tamano: CGSize,
        _ caja: PDFDisplayBox
    ) -> UIImage? {
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        guard let documento = PDFDocument(url: url),
              pagina < documento.pageCount,
              let hoja = documento.page(at: pagina) else { return nil }
        return hoja.thumbnail(of: tamano, for: caja)
    }
}
