import CoreImage
import CoreImage.CIFilterBuiltins
import UIKit
import Vision

enum FormaRecorteImagen: String, Codable, CaseIterable {
    case rectangular, cuadrado, circular, eliptico, poligonal, libre, lazo
    case objeto, parrafo, ecuacion, grafica, circuito, tabla, figura, diapositiva

    var nombre: String {
        switch self {
        case .rectangular: return "Rectangular"
        case .cuadrado: return "Cuadrado"
        case .circular: return "Circular"
        case .eliptico: return "Elíptico"
        case .poligonal: return "Poligonal"
        case .libre: return "Forma libre"
        case .lazo: return "Lazo"
        case .objeto: return "Detectar objeto"
        case .parrafo: return "Detectar párrafo"
        case .ecuacion: return "Detectar ecuación"
        case .grafica: return "Detectar gráfica"
        case .circuito: return "Detectar circuito"
        case .tabla: return "Detectar tabla"
        case .figura: return "Detectar figura"
        case .diapositiva: return "Detectar diapositiva"
        }
    }

    var esSemantica: Bool {
        switch self {
        case .objeto, .parrafo, .ecuacion, .grafica, .circuito, .tabla, .figura,
             .diapositiva: return true
        default: return false
        }
    }
}

struct PuntoNormalizado: Codable, Equatable {
    var x: CGFloat
    var y: CGFloat
}

struct AjustesEdicionImagen: Codable, Equatable {
    var forma: FormaRecorteImagen = .rectangular
    var puntosMascara: [PuntoNormalizado] = []
    var eliminarFondo = false
    var blancoTransparente = false
    var negroTransparente = false
    var invertir = false
    var monocromo = false
    var contraste: Float = 1
    var saturacion: Float = 1
    var brillo: Float = 0
    var enderezarGrados: Float = 0
    var perspectiva: [PuntoNormalizado] = []
    var escalaResolucion: Float = 1
    var vectorizar = false
    var ocr: [BloqueOCR] = []

    /// Contexto efímero para consumidores que todavía escriben por `medio.id`.
    /// Los repositorios lo limpian antes de persistir cualquier JSON.
    var documentoContexto: UInt64? = nil
}

struct BloqueOCR: Codable, Equatable, Identifiable {
    var id = UUID()
    var texto: String
    var confianza: Float
    var caja: CGRect
}

/// Sidecar local y atómico para filtros no destructivos. Los bytes originales
/// del adjunto nunca se modifican.
final class RepositorioEdicionesImagen {
    static let compartido = RepositorioEdicionesImagen()
    private let url: URL
    private var valores: [String: AjustesEdicionImagen] = [:]
    private let cola = DispatchQueue(label: "tinta.ediciones-imagen")
    private let bloqueoContexto = NSLock()
    private var contextoDocumentoPorImagen: [String: UInt64] = [:]

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Metadatos", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("ediciones-imagen.json")
        if let datos = try? Data(contentsOf: url),
           let leido = try? JSONDecoder().decode([String: AjustesEdicionImagen].self,
                                                 from: datos) {
            valores = leido.mapValues(Self.limpiarContexto)
        }
    }

    private static func limpiarContexto(_ valor: AjustesEdicionImagen)
        -> AjustesEdicionImagen {
        var limpio = valor
        limpio.documentoContexto = nil
        return limpio
    }

    /// Contexto sincrónico para callbacks legacy como el aviso de guardado del
    /// editor. El valor leído dentro del bloque conserva la identidad documental
    /// en el struct, por lo que un `guardar` posterior puede ocurrir ya fuera del
    /// bloque sin perder el destino correcto.
    func conContexto(documento: UInt64, id: String, _ cuerpo: () -> Void) {
        bloqueoContexto.lock()
        let anterior = contextoDocumentoPorImagen[id]
        contextoDocumentoPorImagen[id] = documento
        bloqueoContexto.unlock()
        defer {
            bloqueoContexto.lock()
            if let anterior {
                contextoDocumentoPorImagen[id] = anterior
            } else if contextoDocumentoPorImagen[id] == documento {
                contextoDocumentoPorImagen.removeValue(forKey: id)
            }
            bloqueoContexto.unlock()
        }
        cuerpo()
    }

    func ajustes(id: String) -> AjustesEdicionImagen {
        bloqueoContexto.lock()
        let explicito = contextoDocumentoPorImagen[id]
        bloqueoContexto.unlock()
        let documento = explicito
            ?? RepositorioEdicionesImagenDocumentoIPad.compartido
                .documentoUnico(materializadoPara: id)
        guard let documento else { return ajustesLegacy(id: id) }
        var valor = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
            documento: documento, id: id)
        valor.documentoContexto = documento
        return valor
    }

    /// Lectura pura del sidecar histórico para migración lazy.
    func ajustesLegacy(id: String) -> AjustesEdicionImagen {
        cola.sync {
            Self.limpiarContexto(valores[id] ?? AjustesEdicionImagen())
        }
    }

    func guardar(_ ajustes: AjustesEdicionImagen, id: String) {
        if let documento = ajustes.documentoContexto {
            _ = RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
                Self.limpiarContexto(ajustes), documento: documento, id: id)
            return
        }
        guardarLegacy(ajustes, id: id)
    }

    func guardarLegacy(_ ajustes: AjustesEdicionImagen, id: String) {
        cola.sync {
            valores[id] = Self.limpiarContexto(ajustes)
            if let datos = try? JSONEncoder().encode(valores) {
                try? datos.write(to: url, options: .atomic)
            }
        }
    }

    func duplicar(origen: String, destino: String) {
        guardarLegacy(ajustesLegacy(id: origen), id: destino)
    }
}

enum ProcesadorImagenLocal {
    private static let contexto = CIContext(options: [.cacheIntermediates: true])

    static func procesar(_ original: UIImage, ajustes: AjustesEdicionImagen) -> UIImage {
        guard var imagen = CIImage(image: original) else { return original }

        if ajustes.enderezarGrados != 0 {
            let radianes = CGFloat(ajustes.enderezarGrados) * .pi / 180
            let centro = CGPoint(x: imagen.extent.midX, y: imagen.extent.midY)
            imagen = imagen.transformed(by: CGAffineTransform(
                translationX: centro.x, y: centro.y)
                .rotated(by: radianes)
                .translatedBy(x: -centro.x, y: -centro.y))
        }

        if ajustes.perspectiva.count == 4 {
            let e = imagen.extent
            func punto(_ p: PuntoNormalizado) -> CIVector {
                CIVector(x: e.minX + p.x * e.width,
                         y: e.minY + (1 - p.y) * e.height)
            }
            if let filtro = CIFilter(name: "CIPerspectiveCorrection") {
                filtro.setValue(imagen, forKey: kCIInputImageKey)
                filtro.setValue(punto(ajustes.perspectiva[0]), forKey: "inputTopLeft")
                filtro.setValue(punto(ajustes.perspectiva[1]), forKey: "inputTopRight")
                filtro.setValue(punto(ajustes.perspectiva[2]), forKey: "inputBottomRight")
                filtro.setValue(punto(ajustes.perspectiva[3]), forKey: "inputBottomLeft")
                imagen = filtro.outputImage ?? imagen
            }
        }

        let color = CIFilter.colorControls()
        color.inputImage = imagen
        color.contrast = ajustes.contraste
        color.saturation = ajustes.monocromo ? 0 : ajustes.saturacion
        color.brightness = ajustes.brillo
        imagen = color.outputImage ?? imagen

        if ajustes.invertir {
            let filtro = CIFilter.colorInvert()
            filtro.inputImage = imagen
            imagen = filtro.outputImage ?? imagen
        }
        if ajustes.monocromo {
            let filtro = CIFilter.colorMonochrome()
            filtro.inputImage = imagen
            filtro.color = CIColor.white
            filtro.intensity = 1
            imagen = filtro.outputImage ?? imagen
        }
        if ajustes.blancoTransparente || ajustes.negroTransparente {
            imagen = aplicarTransparencia(imagen, blanco: ajustes.blancoTransparente,
                                          negro: ajustes.negroTransparente)
        }
        if ajustes.escalaResolucion > 1 {
            let filtro = CIFilter.lanczosScaleTransform()
            filtro.inputImage = imagen
            filtro.scale = ajustes.escalaResolucion
            filtro.aspectRatio = 1
            imagen = filtro.outputImage ?? imagen
        }

        guard let cg = contexto.createCGImage(imagen, from: imagen.extent) else { return original }
        var salida = UIImage(cgImage: cg, scale: original.scale,
                             orientation: original.imageOrientation)
        salida = aplicarMascara(salida, ajustes: ajustes)
        return salida
    }

    private static func aplicarTransparencia(_ imagen: CIImage, blanco: Bool,
                                             negro: Bool) -> CIImage {
        let dimension = 32
        var cubo = [Float](repeating: 0, count: dimension * dimension * dimension * 4)
        for b in 0..<dimension {
            for g in 0..<dimension {
                for r in 0..<dimension {
                    let rf = Float(r) / Float(dimension - 1)
                    let gf = Float(g) / Float(dimension - 1)
                    let bf = Float(b) / Float(dimension - 1)
                    var alfa: Float = 1
                    if blanco {
                        alfa *= min(max((1 - min(rf, min(gf, bf))) / 0.16, 0), 1)
                    }
                    if negro {
                        alfa *= min(max(max(rf, max(gf, bf)) / 0.16, 0), 1)
                    }
                    let indice = ((b * dimension + g) * dimension + r) * 4
                    cubo[indice] = rf * alfa
                    cubo[indice + 1] = gf * alfa
                    cubo[indice + 2] = bf * alfa
                    cubo[indice + 3] = alfa
                }
            }
        }
        let datos = cubo.withUnsafeBufferPointer { Data(buffer: $0) }
        let filtro = CIFilter.colorCube()
        filtro.inputImage = imagen
        filtro.cubeDimension = Float(dimension)
        filtro.cubeData = datos
        return filtro.outputImage ?? imagen
    }

    private static func aplicarMascara(_ imagen: UIImage,
                                       ajustes: AjustesEdicionImagen) -> UIImage {
        switch ajustes.forma {
        case .circular, .eliptico, .poligonal, .libre, .lazo:
            let formato = UIGraphicsImageRendererFormat()
            formato.scale = imagen.scale
            return UIGraphicsImageRenderer(size: imagen.size, format: formato).image { _ in
                let caja = CGRect(origin: .zero, size: imagen.size)
                let ruta: UIBezierPath
                switch ajustes.forma {
                case .circular:
                    let lado = min(imagen.size.width, imagen.size.height)
                    ruta = UIBezierPath(ovalIn: CGRect(
                        x: (imagen.size.width - lado) / 2,
                        y: (imagen.size.height - lado) / 2, width: lado, height: lado))
                case .eliptico:
                    ruta = UIBezierPath(ovalIn: caja)
                default:
                    ruta = UIBezierPath()
                    for (indice, punto) in ajustes.puntosMascara.enumerated() {
                        let p = CGPoint(x: punto.x * imagen.size.width,
                                        y: punto.y * imagen.size.height)
                        if indice == 0 { ruta.move(to: p) } else { ruta.addLine(to: p) }
                    }
                    ruta.close()
                }
                ruta.addClip()
                imagen.draw(in: caja)
            }
        default: return imagen
        }
    }
}

enum AnalizadorImagenLocal {
    static func reconocerTexto(_ imagen: UIImage,
                               completado: @escaping (Result<[BloqueOCR], Error>) -> Void) {
        guard let cg = imagen.cgImage else {
            completado(.failure(ErrorImportacion.sinRepresentacion)); return
        }
        let solicitud = VNRecognizeTextRequest { request, error in
            if let error { return completado(.failure(error)) }
            let bloques = (request.results as? [VNRecognizedTextObservation] ?? []).compactMap {
                observacion -> BloqueOCR? in
                guard let candidato = observacion.topCandidates(1).first else { return nil }
                let b = observacion.boundingBox
                return BloqueOCR(texto: candidato.string, confianza: candidato.confidence,
                                 caja: CGRect(x: b.minX, y: 1 - b.maxY,
                                              width: b.width, height: b.height))
            }
            completado(.success(bloques))
        }
        solicitud.recognitionLevel = .accurate
        solicitud.usesLanguageCorrection = true
        solicitud.recognitionLanguages = RepositorioBusquedaTecnicaIPad.compartido
            .idiomasOCR()
        solicitud.customWords = RepositorioBusquedaTecnicaIPad.compartido
            .palabrasOCR()
        solicitud.automaticallyDetectsLanguage = true
        DispatchQueue.global(qos: .userInitiated).async {
            do { try VNImageRequestHandler(cgImage: cg, options: [:]).perform([solicitud]) }
            catch { completado(.failure(error)) }
        }
    }

    static func detectarRegion(_ forma: FormaRecorteImagen, en imagen: UIImage,
                               completado: @escaping (Result<CGRect, Error>) -> Void) {
        guard let cg = imagen.cgImage else {
            completado(.failure(ErrorImportacion.sinRepresentacion)); return
        }
        if forma == .parrafo || forma == .ecuacion {
            reconocerTexto(imagen) { resultado in
                completado(resultado.flatMap { bloques in
                    let filtrados = forma == .ecuacion
                        ? bloques.filter { $0.texto.range(of: #"[=+−×÷∫∑√^_]"#,
                                                          options: .regularExpression) != nil }
                        : bloques
                    let union = filtrados.map(\.caja).reduce(CGRect.null) { $0.union($1) }
                    guard !union.isNull else { return .failure(ErrorAnalisisVisual.sinRegion) }
                    return .success(union.insetBy(dx: -0.02, dy: -0.02))
                })
            }
            return
        }
        let solicitud = VNDetectRectanglesRequest { request, error in
            if let error { return completado(.failure(error)) }
            let rectangulos = request.results as? [VNRectangleObservation] ?? []
            guard let mejor = rectangulos.max(by: {
                $0.boundingBox.width * $0.boundingBox.height
                    < $1.boundingBox.width * $1.boundingBox.height
            }) else { return completado(.failure(ErrorAnalisisVisual.sinRegion)) }
            let b = mejor.boundingBox
            completado(.success(CGRect(x: b.minX, y: 1 - b.maxY,
                                       width: b.width, height: b.height)))
        }
        solicitud.maximumObservations = 20
        solicitud.minimumConfidence = 0.35
        solicitud.minimumAspectRatio = forma == .diapositiva ? 0.45 : 0.15
        DispatchQueue.global(qos: .userInitiated).async {
            do { try VNImageRequestHandler(cgImage: cg, options: [:]).perform([solicitud]) }
            catch { completado(.failure(error)) }
        }
    }

    /// Devuelve las cuatro esquinas en coordenadas UIKit normalizadas. Vision
    /// usa origen inferior izquierdo, mientras que el sidecar de edición usa
    /// origen superior izquierdo; la conversión se hace aquí una sola vez.
    static func detectarPerspectiva(en imagen: UIImage,
                                    completado: @escaping (
                                        Result<[PuntoNormalizado], Error>) -> Void) {
        guard let cg = imagen.cgImage else {
            completado(.failure(ErrorImportacion.sinRepresentacion)); return
        }
        let solicitud = VNDetectRectanglesRequest { request, error in
            if let error { return completado(.failure(error)) }
            let rectangulos = request.results as? [VNRectangleObservation] ?? []
            guard let mejor = rectangulos.max(by: {
                $0.confidence * Float($0.boundingBox.width * $0.boundingBox.height)
                    < $1.confidence * Float($1.boundingBox.width * $1.boundingBox.height)
            }), mejor.boundingBox.width * mejor.boundingBox.height >= 0.18 else {
                return completado(.failure(ErrorAnalisisVisual.sinRegion))
            }
            func convertir(_ punto: CGPoint) -> PuntoNormalizado {
                PuntoNormalizado(x: min(max(punto.x, 0), 1),
                                 y: min(max(1 - punto.y, 0), 1))
            }
            let puntos = [mejor.topLeft, mejor.topRight,
                          mejor.bottomRight, mejor.bottomLeft].map(convertir)
            completado(.success(puntos))
        }
        solicitud.maximumObservations = 12
        solicitud.minimumConfidence = 0.42
        solicitud.minimumAspectRatio = 0.2
        solicitud.quadratureTolerance = 32
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try VNImageRequestHandler(cgImage: cg, options: [:])
                    .perform([solicitud])
            }
            catch { completado(.failure(error)) }
        }
    }

    @available(iOS 17.0, *)
    static func quitarFondo(_ imagen: UIImage,
                            completado: @escaping (Result<UIImage, Error>) -> Void) {
        guard let cg = imagen.cgImage else {
            completado(.failure(ErrorImportacion.sinRepresentacion)); return
        }
        let solicitud = VNGenerateForegroundInstanceMaskRequest { request, error in
            do {
                if let error { throw error }
                guard let resultado = request.results?.first as? VNInstanceMaskObservation else {
                    throw ErrorAnalisisVisual.sinRegion
                }
                let manejador = VNImageRequestHandler(cgImage: cg, options: [:])
                let mascara = try resultado.generateScaledMaskForImage(
                    forInstances: resultado.allInstances, from: manejador)
                let original = CIImage(cgImage: cg)
                let mask = CIImage(cvPixelBuffer: mascara)
                let transparente = CIImage(color: .clear).cropped(to: original.extent)
                let filtro = CIFilter.blendWithMask()
                filtro.inputImage = original
                filtro.backgroundImage = transparente
                filtro.maskImage = mask
                guard let salida = filtro.outputImage,
                      let cgSalida = CIContext().createCGImage(salida, from: original.extent) else {
                    throw ErrorAnalisisVisual.sinRegion
                }
                completado(.success(UIImage(cgImage: cgSalida, scale: imagen.scale,
                                            orientation: imagen.imageOrientation)))
            } catch { completado(.failure(error)) }
        }
        DispatchQueue.global(qos: .userInitiated).async {
            do { try VNImageRequestHandler(cgImage: cg, options: [:]).perform([solicitud]) }
            catch { completado(.failure(error)) }
        }
    }
}

enum ErrorAnalisisVisual: LocalizedError {
    case sinRegion
    var errorDescription: String? { "No se detectó una región con suficiente confianza." }
}
