import CoreImage
import UIKit
import Vision

struct ResultadoPizarra {
    var imagen: UIImage
    var pizarrasDetectadas: [CGRect]
    var texto: [BloqueOCR]
    var hayPersonaTapando: Bool
    var confianza: Float
}

enum ProcesadorPizarraLocal {
    private static let contexto = CIContext()

    static func procesar(_ imagen: UIImage,
                         completado: @escaping (Result<ResultadoPizarra, Error>) -> Void) {
        guard let cg = imagen.cgImage else {
            completado(.failure(ErrorImportacion.sinRepresentacion)); return
        }
        let rectangulos = VNDetectRectanglesRequest()
        rectangulos.maximumObservations = 8
        rectangulos.minimumConfidence = 0.35
        rectangulos.minimumAspectRatio = 0.2
        rectangulos.quadratureTolerance = 35
        let personas = VNDetectHumanRectanglesRequest()
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let manejador = VNImageRequestHandler(cgImage: cg, options: [:])
                try manejador.perform([rectangulos, personas])
                let regiones = rectangulos.results?.map(\.boundingBox) ?? []
                let mayor = rectangulos.results?.max {
                    $0.boundingBox.width * $0.boundingBox.height
                        < $1.boundingBox.width * $1.boundingBox.height
                }
                let corregida = mayor.flatMap { corregirPerspectiva(imagen, rectangulo: $0) }
                    ?? imagen
                let mejorada = mejorar(corregida)
                AnalizadorImagenLocal.reconocerTexto(mejorada) { resultadoOCR in
                    let ocr = (try? resultadoOCR.get()) ?? []
                    let humanos = personas.results ?? []
                    let tapando = humanos.contains { persona in
                        regiones.contains { $0.intersects(persona.boundingBox) }
                    }
                    completado(.success(ResultadoPizarra(
                        imagen: mejorada,
                        pizarrasDetectadas: regiones.map {
                            CGRect(x: $0.minX, y: 1 - $0.maxY,
                                   width: $0.width, height: $0.height)
                        }, texto: ocr, hayPersonaTapando: tapando,
                        confianza: mayor?.confidence ?? 0)))
                }
            } catch { completado(.failure(error)) }
        }
    }

    private static func corregirPerspectiva(_ imagen: UIImage,
                                            rectangulo: VNRectangleObservation) -> UIImage? {
        guard let entrada = CIImage(image: imagen) else { return nil }
        let e = entrada.extent
        func p(_ punto: CGPoint) -> CIVector {
            CIVector(x: e.minX + punto.x * e.width,
                     y: e.minY + punto.y * e.height)
        }
        guard let filtro = CIFilter(name: "CIPerspectiveCorrection") else { return nil }
        filtro.setValue(entrada, forKey: kCIInputImageKey)
        filtro.setValue(p(rectangulo.topLeft), forKey: "inputTopLeft")
        filtro.setValue(p(rectangulo.topRight), forKey: "inputTopRight")
        filtro.setValue(p(rectangulo.bottomRight), forKey: "inputBottomRight")
        filtro.setValue(p(rectangulo.bottomLeft), forKey: "inputBottomLeft")
        guard let salida = filtro.outputImage,
              let cg = contexto.createCGImage(salida, from: salida.extent) else { return nil }
        return UIImage(cgImage: cg, scale: imagen.scale, orientation: .up)
    }

    private static func mejorar(_ imagen: UIImage) -> UIImage {
        guard var actual = CIImage(image: imagen) else { return imagen }
        let auto = actual.autoAdjustmentFilters(options: [
            .enhance: true, .redEye: false, .level: true, .crop: false,
        ])
        for filtro in auto { filtro.setValue(actual, forKey: kCIInputImageKey)
            actual = filtro.outputImage ?? actual }
        if let sombras = CIFilter(name: "CIHighlightShadowAdjust") {
            sombras.setValue(actual, forKey: kCIInputImageKey)
            sombras.setValue(0.45, forKey: "inputShadowAmount")
            sombras.setValue(0.8, forKey: "inputHighlightAmount")
            actual = sombras.outputImage ?? actual
        }
        if let ruido = CIFilter(name: "CINoiseReduction") {
            ruido.setValue(actual, forKey: kCIInputImageKey)
            ruido.setValue(0.015, forKey: "inputNoiseLevel")
            ruido.setValue(0.5, forKey: "inputSharpness")
            actual = ruido.outputImage ?? actual
        }
        if let color = CIFilter(name: "CIColorControls") {
            color.setValue(actual, forKey: kCIInputImageKey)
            color.setValue(1.28, forKey: kCIInputContrastKey)
            color.setValue(0.04, forKey: kCIInputBrightnessKey)
            color.setValue(1.05, forKey: kCIInputSaturationKey)
            actual = color.outputImage ?? actual
        }
        guard let cg = contexto.createCGImage(actual, from: actual.extent) else { return imagen }
        return UIImage(cgImage: cg, scale: imagen.scale, orientation: .up)
    }
}

/// Conserva hasta cinco fotos recientes y calcula una mediana por píxel. Con
/// tres tomas, una persona que tapa una zona en solo una de ellas desaparece
/// sin escoger ciegamente la imagen más clara (que borraría el marcador).
final class FusionPizarraLocal {
    private static let cola = DispatchQueue(
        label: "tinta.pizarra.fusion", qos: .userInitiated)
    private var fotos: [(Date, UIImage)] = []

    var cantidadFotos: Int { fotos.count }

    func agregar(_ imagen: UIImage, fecha: Date = Date()) {
        precondition(Thread.isMainThread,
                     "La colección de fotos pertenece al hilo principal")
        fotos.append((fecha, imagen))
        fotos = fotos.filter { fecha.timeIntervalSince($0.0) <= 120 }
        if fotos.count > 5 { fotos.removeFirst(fotos.count - 5) }
    }

    /// Toma una instantánea de la colección en el hilo principal y ejecuta el
    /// redimensionado, la mediana y la creación del bitmap fuera de la UI. El
    /// cierre se invoca en la cola de trabajo para que el llamador pueda
    /// codificar/guardar la imagen antes de volver al hilo principal.
    func fusionar(_ completado: @escaping (UIImage?) -> Void) {
        precondition(Thread.isMainThread,
                     "La colección de fotos pertenece al hilo principal")
        let imagenes = fotos.map(\.1)
        Self.cola.async {
            autoreleasepool {
                completado(Self.fusionar(imagenes))
            }
        }
    }

    private static func fusionar(_ imagenes: [UIImage]) -> UIImage? {
        guard let primera = imagenes.first else { return nil }
        guard imagenes.count >= 2 else { return primera }
        let maximo: CGFloat = 2048
        let escala = min(1, maximo / max(primera.size.width, primera.size.height))
        let tamano = CGSize(width: floor(primera.size.width * escala),
                            height: floor(primera.size.height * escala))
        guard tamano.width >= 1, tamano.height >= 1 else { return nil }
        let buffers = imagenes.compactMap {
            rgba(redimensionar($0, a: tamano), tamano: tamano)
        }
        guard buffers.count >= 2 else { return primera }
        let cantidad = Int(tamano.width * tamano.height * 4)
        var salida = [UInt8](repeating: 0, count: cantidad)

        // `fotos` conserva como máximo cinco entradas. Elegir una ruta una vez
        // evita crear/ordenar un Array millones de veces (una por canal RGBA).
        switch buffers.count {
        case 2:
            let a = buffers[0], b = buffers[1]
            for indice in 0..<cantidad {
                salida[indice] = mediana2(a[indice], b[indice])
            }
        case 3:
            let a = buffers[0], b = buffers[1], c = buffers[2]
            for indice in 0..<cantidad {
                salida[indice] = mediana3(a[indice], b[indice], c[indice])
            }
        case 4:
            let a = buffers[0], b = buffers[1], c = buffers[2], d = buffers[3]
            for indice in 0..<cantidad {
                salida[indice] = mediana4(
                    a[indice], b[indice], c[indice], d[indice])
            }
        default:
            let a = buffers[0], b = buffers[1], c = buffers[2]
            let d = buffers[3], e = buffers[4]
            for indice in 0..<cantidad {
                salida[indice] = mediana5(
                    a[indice], b[indice], c[indice], d[indice], e[indice])
            }
        }
        let datos = Data(salida) as CFData
        guard let proveedor = CGDataProvider(data: datos),
              let cg = CGImage(width: Int(tamano.width), height: Int(tamano.height),
                               bitsPerComponent: 8, bitsPerPixel: 32,
                               bytesPerRow: Int(tamano.width) * 4,
                               space: CGColorSpaceCreateDeviceRGB(),
                               bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                               provider: proveedor, decode: nil,
                               shouldInterpolate: true, intent: .defaultIntent) else { return nil }
        return UIImage(cgImage: cg)
    }

    private static func redimensionar(_ imagen: UIImage, a tamano: CGSize) -> UIImage {
        UIGraphicsImageRenderer(size: tamano).image { _ in
            imagen.draw(in: CGRect(origin: .zero, size: tamano))
        }
    }

    private static func rgba(_ imagen: UIImage, tamano: CGSize) -> [UInt8]? {
        guard let cg = imagen.cgImage else { return nil }
        var datos = [UInt8](repeating: 0, count: Int(tamano.width * tamano.height * 4))
        guard let contexto = CGContext(data: &datos, width: Int(tamano.width),
                                       height: Int(tamano.height), bitsPerComponent: 8,
                                       bytesPerRow: Int(tamano.width) * 4,
                                       space: CGColorSpaceCreateDeviceRGB(),
                                       bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        else { return nil }
        contexto.draw(cg, in: CGRect(origin: .zero, size: tamano))
        return datos
    }

    @inline(__always)
    private static func ordenar(_ a: inout UInt8, _ b: inout UInt8) {
        if a > b { swap(&a, &b) }
    }

    /// Para una cantidad par preservamos la semántica anterior: la mediana
    /// superior, no el promedio. Estas funciones son internas para probar las
    /// redes completas sin exponerlas como API de la app.
    @inline(__always)
    static func mediana2(_ a: UInt8, _ b: UInt8) -> UInt8 {
        max(a, b)
    }

    @inline(__always)
    static func mediana3(_ a: UInt8, _ b: UInt8, _ c: UInt8) -> UInt8 {
        var a = a, b = b, c = c
        ordenar(&a, &b); ordenar(&b, &c); ordenar(&a, &b)
        return b
    }

    @inline(__always)
    static func mediana4(_ a: UInt8, _ b: UInt8, _ c: UInt8,
                         _ d: UInt8) -> UInt8 {
        var a = a, b = b, c = c, d = d
        ordenar(&a, &b); ordenar(&c, &d); ordenar(&a, &c)
        ordenar(&b, &d); ordenar(&b, &c)
        return c
    }

    @inline(__always)
    static func mediana5(_ a: UInt8, _ b: UInt8, _ c: UInt8,
                         _ d: UInt8, _ e: UInt8) -> UInt8 {
        var a = a, b = b, c = c, d = d, e = e
        ordenar(&a, &b); ordenar(&d, &e); ordenar(&c, &e)
        ordenar(&c, &d); ordenar(&b, &e); ordenar(&a, &d)
        ordenar(&a, &c); ordenar(&b, &d); ordenar(&b, &c)
        return c
    }
}
