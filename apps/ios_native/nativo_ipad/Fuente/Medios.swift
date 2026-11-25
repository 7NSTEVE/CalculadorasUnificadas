import PhotosUI
import UIKit
import UniformTypeIdentifiers

/// Traer cosas de fuera: fotos, vídeos, PDF y documentos de Word.
///
/// Todo con selectores del sistema. `PHPickerViewController` no necesita
/// permiso de fotos —el usuario elige en un proceso aparte y la app solo recibe
/// lo elegido—, y `UIDocumentPickerViewController` da acceso a iCloud Drive y a
/// cualquier otra app de archivos sin pedir nada.
enum Medios {

    struct ResultadoImportacion {
        let adjunto: AdjuntoImportado
        let tipo: TipoMedio
        let tamanoSugerido: CGSize?
    }

    /// Lo que la app sabe recibir. Word entra por su tipo real, no por la
    /// extensión: un `.docx` renombrado seguiría siendo un `.docx`.
    static var tiposImportables: [UTType] {
        var tipos: [UTType] = [.pdf, .image, .movie, .plainText, .rtf, .presentation]
        if let docx = UTType("org.openxmlformats.wordprocessingml.document") {
            tipos.append(docx)
        }
        if let doc = UTType("com.microsoft.word.doc") { tipos.append(doc) }
        if let pptx = UTType("org.openxmlformats.presentationml.presentation") {
            tipos.append(pptx)
        }
        tipos.append(.item)  // el resto, para no cerrar la puerta
        return tipos
    }

    // MARK: - Fotos y vídeos

    static func selectorDeFotos(limite: Int = 0,
                                delegado: PHPickerViewControllerDelegate) -> PHPickerViewController {
        var config = PHPickerConfiguration(photoLibrary: .shared())
        config.selectionLimit = limite            // 0 = sin límite
        config.filter = .any(of: [.images, .videos])
        config.preferredAssetRepresentationMode = .current
        let selector = PHPickerViewController(configuration: config)
        selector.delegate = delegado
        return selector
    }

    /// Saca la imagen de un resultado del selector.
    static func imagen(de resultado: PHPickerResult,
                       completado: @escaping (UIImage?) -> Void) {
        let proveedor = resultado.itemProvider
        guard proveedor.canLoadObject(ofClass: UIImage.self) else {
            return completado(nil)
        }
        proveedor.loadObject(ofClass: UIImage.self) { objeto, _ in
            DispatchQueue.main.async { completado(objeto as? UIImage) }
        }
    }

    /// Copia el vídeo a una carpeta propia. El archivo que da el selector vive
    /// en un sitio temporal y desaparece.
    static func copiarVideo(de resultado: PHPickerResult, a carpeta: URL,
                            completado: @escaping (URL?) -> Void) {
        let proveedor = resultado.itemProvider
        proveedor.loadFileRepresentation(forTypeIdentifier: UTType.movie.identifier) { url, _ in
            guard let url else { return DispatchQueue.main.async { completado(nil) } }
            do {
                try FileManager.default.createDirectory(at: carpeta,
                                                        withIntermediateDirectories: true)
                let destino = carpeta.appendingPathComponent(url.lastPathComponent)
                try? FileManager.default.removeItem(at: destino)
                try FileManager.default.copyItem(at: url, to: destino)
                DispatchQueue.main.async { completado(destino) }
            } catch {
                DispatchQueue.main.async { completado(nil) }
            }
        }
    }

    /// Conserva la representación original elegida (HEIC, TIFF, WebP o vídeo)
    /// en el almacén por hash. Solo cae a PNG si Fotos no entrega archivo.
    static func importar(_ resultado: PHPickerResult,
                         completado: @escaping (Result<ResultadoImportacion, Error>) -> Void) {
        let proveedor = resultado.itemProvider
        if let identificador = proveedor.registeredTypeIdentifiers.first(where: {
            UTType($0)?.conforms(to: .image) == true
        }) {
            proveedor.loadFileRepresentation(forTypeIdentifier: identificador) { url, error in
                if let url {
                    do {
                        let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
                        let imagen = UIImage(contentsOfFile: url.path)
                        let tamano = imagen.map { tamanoDeInsercion($0.size) }
                        DispatchQueue.main.async {
                            completado(.success(ResultadoImportacion(
                                adjunto: adjunto, tipo: .imagen, tamanoSugerido: tamano)))
                        }
                    } catch {
                        DispatchQueue.main.async { completado(.failure(error)) }
                    }
                } else if proveedor.canLoadObject(ofClass: UIImage.self) {
                    proveedor.loadObject(ofClass: UIImage.self) { objeto, segundoError in
                        do {
                            guard let imagen = objeto as? UIImage else {
                                throw segundoError ?? ErrorImportacion.sinRepresentacion
                            }
                            let adjunto = try RepositorioAdjuntos.compartido.importar(imagen: imagen)
                            DispatchQueue.main.async {
                                completado(.success(ResultadoImportacion(
                                    adjunto: adjunto, tipo: .imagen,
                                    tamanoSugerido: tamanoDeInsercion(imagen.size))))
                            }
                        } catch {
                            DispatchQueue.main.async { completado(.failure(error)) }
                        }
                    }
                } else {
                    DispatchQueue.main.async {
                        completado(.failure(error ?? ErrorImportacion.sinRepresentacion))
                    }
                }
            }
            return
        }
        if let identificador = proveedor.registeredTypeIdentifiers.first(where: {
            UTType($0)?.conforms(to: .movie) == true
        }) {
            proveedor.loadFileRepresentation(forTypeIdentifier: identificador) { url, error in
                do {
                    guard let url else { throw error ?? ErrorImportacion.sinRepresentacion }
                    let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
                    DispatchQueue.main.async {
                        completado(.success(ResultadoImportacion(
                            adjunto: adjunto, tipo: .video,
                            tamanoSugerido: CGSize(width: 480, height: 270))))
                    }
                } catch {
                    DispatchQueue.main.async { completado(.failure(error)) }
                }
            }
            return
        }
        completado(.failure(ErrorImportacion.tipoNoCompatible))
    }

    static func tipoMedio(de url: URL) -> TipoMedio {
        let tipo = (try? url.resourceValues(forKeys: [.contentTypeKey]).contentType)
        if tipo?.conforms(to: .image) == true { return .imagen }
        if tipo?.conforms(to: .movie) == true { return .video }
        if tipo?.conforms(to: .audio) == true { return .audio }
        if tipo?.conforms(to: .pdf) == true { return .pdf }
        return .documento
    }

    static func tamanoDeInsercion(_ original: CGSize) -> CGSize {
        guard original.width > 0, original.height > 0 else {
            return CGSize(width: 420, height: 300)
        }
        let escala = min(min(520 / original.width, 420 / original.height), 1)
        return CGSize(width: max(original.width * escala, 120),
                      height: max(original.height * escala, 90))
    }

    // MARK: - Documentos

    static func selectorDeDocumentos(delegado: UIDocumentPickerDelegate)
        -> UIDocumentPickerViewController {
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: tiposImportables, asCopy: true)
        selector.delegate = delegado
        selector.allowsMultipleSelection = true
        return selector
    }

    // MARK: - Portapapeles

    /// Pegar una imagen. Se mira `images` y no solo `image` porque copiar varias
    /// de golpe es normal en iPadOS.
    static func imagenesDelPortapapeles() -> [UIImage] {
        let tablero = UIPasteboard.general
        if let varias = tablero.images, !varias.isEmpty { return varias }
        if let una = tablero.image { return [una] }
        return []
    }

    static var hayImagenQuePegar: Bool {
        UIPasteboard.general.hasImages
    }

    /// Copiar una imagen al portapapeles, para pegarla en otra app.
    static func copiar(_ imagen: UIImage) {
        // PNG y no JPEG: lo que se copia de una nota suele llevar tinta sobre
        // fondo plano, y el JPEG lo llena de suciedad en los bordes.
        if let png = imagen.pngData() {
            UIPasteboard.general.setData(png, forPasteboardType: UTType.png.identifier)
        } else {
            UIPasteboard.general.image = imagen
        }
    }

    // MARK: - Recortes

    /// Recorte de una imagen, en fracciones del original.
    ///
    /// Se guarda como recorte y **no** reescribiendo los píxeles: así siempre se
    /// puede volver atrás. La composición de un recorte sobre otro la hace el
    /// núcleo, que es donde está probada.
    struct Recorte: Codable, Equatable {
        var x: Float = 0
        var y: Float = 0
        var ancho: Float = 1
        var alto: Float = 1

        /// Aplica un recorte nuevo expresado en fracciones de **lo que hoy se
        /// ve**, no del original.
        mutating func componer(x nx: Float, y ny: Float, ancho na: Float, alto nl: Float) {
            tinta_recorte_componer(&x, &y, &ancho, &alto, nx, ny, na, nl)
        }

        mutating func quitar() {
            x = 0; y = 0; ancho = 1; alto = 1
        }

        var esCompleto: Bool {
            x == 0 && y == 0 && ancho == 1 && alto == 1
        }

        /// La región visible del original, en píxeles.
        func region(en tamano: CGSize) -> CGRect {
            CGRect(x: CGFloat(x) * tamano.width,
                   y: CGFloat(y) * tamano.height,
                   width: CGFloat(ancho) * tamano.width,
                   height: CGFloat(alto) * tamano.height)
        }
    }

    /// Aplica el recorte para enseñarlo o exportarlo. La imagen de origen no se
    /// toca: esto devuelve una copia.
    static func recortar(_ imagen: UIImage, con recorte: Recorte) -> UIImage {
        guard !recorte.esCompleto, let cg = imagen.cgImage else { return imagen }
        let tamano = CGSize(width: CGFloat(cg.width), height: CGFloat(cg.height))
        let region = recorte.region(en: tamano).integral
        guard region.width >= 1, region.height >= 1,
              let recortada = cg.cropping(to: region) else { return imagen }
        return UIImage(cgImage: recortada, scale: imagen.scale,
                       orientation: imagen.imageOrientation)
    }
}

enum ErrorImportacion: LocalizedError {
    case sinRepresentacion, tipoNoCompatible
    var errorDescription: String? {
        switch self {
        case .sinRepresentacion: return "El selector no entregó una copia utilizable."
        case .tipoNoCompatible: return "Ese recurso todavía no tiene una representación importable."
        }
    }
}
