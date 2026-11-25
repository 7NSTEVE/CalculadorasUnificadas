import Foundation
import UIKit



enum ErrorImportadorPPTXIPad: LocalizedError {
    case contenedor(String)
    case estructuraInvalida
    case sinDiapositivas

    var errorDescription: String? {
        switch self {
        case .contenedor(let detalle): return "No se pudo abrir el PPTX: \(detalle)"
        case .estructuraInvalida: return "El archivo no contiene una estructura PowerPoint válida."
        case .sinDiapositivas: return "El PowerPoint no contiene diapositivas legibles."
        }
    }
}

/// Importación directa y local del contenedor Open XML. El archivo fuente y
/// todos los XML permanecen inmutables; la vista raster sirve de referencia y
/// los objetos extraídos se pueden separar sin convertir el original.
enum ImportadorPPTXIPad {
    private static let bloqueoExtraccion = NSLock()

    static func importar(
        _ origen: URL,
        opciones: OpcionesImportacionPPTXIPad = .completa
    ) throws -> PresentacionLocalIPad {
        let acceso = origen.startAccessingSecurityScopedResource()
        defer { if acceso { origen.stopAccessingSecurityScopedResource() } }
        let fuente = try RepositorioAdjuntos.compartido.importar(url: origen)
        let documentos = FileManager.default.urls(
            for: .documentDirectory, in: .userDomainMask).first!
        let base = documentos.appendingPathComponent(
            "Academico/PPTX", isDirectory: true)
        let carpeta = try contenedorExtraido(
            origen: origen, hash: fuente.hash, base: base)
        let presentacionXML = carpeta.appendingPathComponent("ppt/presentation.xml")
        guard FileManager.default.fileExists(atPath: presentacionXML.path) else {
            throw ErrorImportadorPPTXIPad.estructuraInvalida
        }

        let tamano = lectorTamano(presentacionXML)
        let carpetaSlides = carpeta.appendingPathComponent("ppt/slides")
        let archivos = (try? FileManager.default.contentsOfDirectory(
            at: carpetaSlides, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        let slides = archivos.filter {
            $0.pathExtension.lowercased() == "xml"
                && $0.lastPathComponent.hasPrefix("slide")
        }.sorted { numeroSlide($0) < numeroSlide($1) }
        guard !slides.isEmpty else { throw ErrorImportadorPPTXIPad.sinDiapositivas }

        let autores = autoresComentarios(carpeta)
        let recursos = opciones.extraerRecursos
            ? extraerRecursosPresentacion(carpeta) : []
        var vistas: [DiapositivaExtraidaIPad] = []
        for slideURL in slides {
            let parser = LectorSlidePPTX()
            guard let xml = XMLParser(contentsOf: slideURL) else { continue }
            xml.delegate = parser; _ = xml.parse()
            let relaciones = relacionesDeSlide(slideURL)
            let imagen = renderizar(
                parser.formas, relaciones: relaciones,
                carpetaSlide: slideURL.deletingLastPathComponent(),
                raiz: carpeta, tamanoEMU: tamano)
            let texto = parser.formas.map(\.texto).filter { !$0.isEmpty }
                .joined(separator: "\n")
            let numero = numeroSlide(slideURL)
            let notas = opciones.importarNotasPresentador
                ? notasDeSlide(slideURL, relaciones: relaciones,
                               raiz: carpeta, numero: numero) : ""
            let comentarios = opciones.importarComentarios
                ? comentariosDeSlide(slideURL, relaciones: relaciones,
                                     raiz: carpeta, numero: numero,
                                     autores: autores) : []
            var elementos: [ElementoPPTXIPad]? = nil
            if opciones.representacion != .raster {
                elementos = parser.formas.flatMap {
                    elementosEditables(
                        $0, relaciones: relaciones, slide: slideURL,
                        raiz: carpeta, tamanoEMU: tamano,
                        animaciones: parser.ordenAnimacion,
                        opciones: opciones)
                }
            }
            vistas.append(DiapositivaExtraidaIPad(
                imagen: imagen,
                texto: texto,
                notasPresentador: notas.isEmpty ? nil : notas,
                elementosPPTX: elementos,
                comentariosPPTX: comentarios.isEmpty ? nil : comentarios,
                cantidadPasosAnimacion: opciones.separarAnimaciones
                    ? parser.ordenAnimacion.values.max() : nil,
                opcionesPPTX: opciones))
        }
        guard !vistas.isEmpty else { throw ErrorImportadorPPTXIPad.sinDiapositivas }
        return try RepositorioDiapositivasIPad.compartido.registrarPresentacionExtraida(
            titulo: origen.deletingPathExtension().lastPathComponent,
            fuente: fuente, vistas: vistas, recursosPPTX: recursos)
    }

    /// Una extracción solo se vuelve visible cuando contiene la estructura
    /// mínima y el marcador final. Un fallo, cancelación o cierre no deja una
    /// caché parcial que el siguiente intento pueda confundir con un PPTX sano.
    private static func contenedorExtraido(
        origen: URL, hash: String, base: URL
    ) throws -> URL {
        let archivos = FileManager.default
        let final = base.appendingPathComponent(hash, isDirectory: true)
        let marcador = final.appendingPathComponent(
            ".extraccion-completa-v1", isDirectory: false)
        let presentacion = final.appendingPathComponent("ppt/presentation.xml")
        let selloEsperado = Data("pptx-cache-v1".utf8)
        let cacheCompleta = {
            archivos.fileExists(atPath: presentacion.path)
                && (try? Data(contentsOf: marcador)) == selloEsperado
        }
        if cacheCompleta() { return final }

        bloqueoExtraccion.lock()
        defer { bloqueoExtraccion.unlock() }
        if cacheCompleta() { return final }

        try archivos.createDirectory(at: base, withIntermediateDirectories: true)
        let temporal = base.appendingPathComponent(
            ".\(hash)-\(UUID().uuidString).parcial", isDirectory: true)
        try archivos.createDirectory(at: temporal, withIntermediateDirectories: true)
        defer { try? archivos.removeItem(at: temporal) }

        var mensaje = [CChar](repeating: 0, count: 1_024)
        let resultado = mensaje.withUnsafeMutableBufferPointer { buffer in
            origen.path.withCString { origenC in
                temporal.path.withCString { temporalC in
                    tinta_pptx_extraer(
                        origenC, temporalC, buffer.baseAddress, buffer.count)
                }
            }
        }
        guard resultado == 1 else {
            let detalle = mensaje.withUnsafeBufferPointer {
                String(cString: $0.baseAddress!)
            }
            throw ErrorImportadorPPTXIPad.contenedor(detalle)
        }
        guard archivos.fileExists(atPath: temporal.appendingPathComponent(
            "ppt/presentation.xml").path) else {
            throw ErrorImportadorPPTXIPad.estructuraInvalida
        }

        if archivos.fileExists(atPath: final.path) {
            try archivos.removeItem(at: final)
        }
        try archivos.moveItem(at: temporal, to: final)
        try selloEsperado.write(to: marcador, options: .atomic)
        return final
    }

    private static func numeroSlide(_ url: URL) -> Int {
        Int(url.deletingPathExtension().lastPathComponent
            .replacingOccurrences(of: "slide", with: "")) ?? Int.max
    }

    private static func lectorTamano(_ url: URL) -> CGSize {
        guard let parser = XMLParser(contentsOf: url) else {
            return CGSize(width: 12_192_000, height: 6_858_000)
        }
        let delegado = LectorTamanoPPTX(); parser.delegate = delegado; _ = parser.parse()
        return delegado.tamano ?? CGSize(width: 12_192_000, height: 6_858_000)
    }

    private static func relacionesDeSlide(_ slide: URL) -> [String: RelacionPPTX] {
        let url = slide.deletingLastPathComponent()
            .appendingPathComponent("_rels/\(slide.lastPathComponent).rels")
        guard let parser = XMLParser(contentsOf: url) else { return [:] }
        let delegado = LectorRelacionesPPTX(); parser.delegate = delegado; _ = parser.parse()
        return delegado.relaciones
    }

    private static func destinoSeguro(
        _ relacion: RelacionPPTX,
        relativoA archivo: URL,
        raiz: URL
    ) -> URL? {
        guard !relacion.externa else { return nil }
        let decodificado = relacion.objetivo.removingPercentEncoding
            ?? relacion.objetivo
        guard !decodificado.contains("\0") else { return nil }
        let relativo = decodificado.trimmingCharacters(in: CharacterSet(charactersIn: "/\\"))
        let destino = archivo.deletingLastPathComponent()
            .appendingPathComponent(relativo).standardizedFileURL
        let base = raiz.standardizedFileURL.path
        guard destino.path == base || destino.path.hasPrefix(base + "/") else { return nil }
        return destino
    }

    private static func textoSimple(_ url: URL) -> String {
        guard FileManager.default.fileExists(atPath: url.path),
              let parser = XMLParser(contentsOf: url) else { return "" }
        let lector = LectorTextoPPTX(); parser.delegate = lector; _ = parser.parse()
        return lector.textos.joined(separator: "\n")
    }

    private static func notasDeSlide(
        _ slide: URL,
        relaciones: [String: RelacionPPTX],
        raiz: URL,
        numero: Int
    ) -> String {
        if let relacion = relaciones.values.first(where: {
            $0.tipo.lowercased().contains("notesslide")
        }), let url = destinoSeguro(relacion, relativoA: slide, raiz: raiz) {
            return textoSimple(url)
        }
        return textoSimple(raiz.appendingPathComponent(
            "ppt/notesSlides/notesSlide\(numero).xml"))
    }

    private static func comentariosDeSlide(
        _ slide: URL,
        relaciones: [String: RelacionPPTX],
        raiz: URL,
        numero: Int,
        autores: [String: String]
    ) -> [ComentarioPPTXIPad] {
        var urls = relaciones.values.filter {
            $0.tipo.lowercased().contains("comment")
                && !$0.tipo.lowercased().contains("author")
        }.compactMap { destinoSeguro($0, relativoA: slide, raiz: raiz) }
        if urls.isEmpty {
            let candidata = raiz.appendingPathComponent("ppt/comments/comment\(numero).xml")
            if FileManager.default.fileExists(atPath: candidata.path) { urls.append(candidata) }
        }
        return urls.flatMap { url -> [ComentarioPPTXIPad] in
            guard let parser = XMLParser(contentsOf: url) else { return [] }
            let lector = LectorComentariosPPTX(autores: autores)
            parser.delegate = lector; _ = parser.parse()
            return lector.comentarios
        }
    }

    private static func autoresComentarios(_ raiz: URL) -> [String: String] {
        let candidatas = [
            raiz.appendingPathComponent("ppt/commentAuthors.xml"),
            raiz.appendingPathComponent("ppt/authors.xml"),
        ]
        var salida: [String: String] = [:]
        for url in candidatas where FileManager.default.fileExists(atPath: url.path) {
            guard let parser = XMLParser(contentsOf: url) else { continue }
            let lector = LectorAutoresPPTX(); parser.delegate = lector; _ = parser.parse()
            salida.merge(lector.autores) { actual, _ in actual }
        }
        return salida
    }

    private static func extraerRecursosPresentacion(_ raiz: URL) -> [RecursoPPTXIPad] {
        let carpetas = ["ppt/media", "ppt/embeddings", "ppt/charts",
                        "ppt/diagrams", "ppt/activeX"]
        var recursos: [RecursoPPTXIPad] = []
        for relativa in carpetas {
            let carpeta = raiz.appendingPathComponent(relativa, isDirectory: true)
            guard let enumerador = FileManager.default.enumerator(
                at: carpeta, includingPropertiesForKeys: [.fileSizeKey],
                options: [.skipsHiddenFiles, .skipsPackageDescendants]) else { continue }
            for case let archivo as URL in enumerador {
                guard (try? archivo.resourceValues(forKeys: [.isRegularFileKey])
                    .isRegularFile) == true,
                      let adjunto = try? RepositorioAdjuntos.compartido.importar(url: archivo)
                else { continue }
                let tipo = tipoRecurso(archivo, tipoRelacion: "")
                recursos.append(RecursoPPTXIPad(
                    nombre: archivo.lastPathComponent,
                    rutaOpenXML: archivo.path.replacingOccurrences(
                        of: raiz.path + "/", with: ""),
                    hash: adjunto.hash, tipo: tipo, bytes: adjunto.bytes))
            }
        }
        var vistos = Set<String>()
        return recursos.filter { vistos.insert($0.hash).inserted }
            .sorted { $0.rutaOpenXML < $1.rutaOpenXML }
    }

    private static func elementosEditables(
        _ forma: FormaPPTX,
        relaciones: [String: RelacionPPTX],
        slide: URL,
        raiz: URL,
        tamanoEMU: CGSize,
        animaciones: [String: Int],
        opciones: OpcionesImportacionPPTXIPad
    ) -> [ElementoPPTXIPad] {
        let marco = CGRect(
            x: CGFloat(forma.x) / max(tamanoEMU.width, 1),
            y: CGFloat(forma.y) / max(tamanoEMU.height, 1),
            width: CGFloat(forma.ancho) / max(tamanoEMU.width, 1),
            height: CGFloat(forma.alto) / max(tamanoEMU.height, 1))
        let marcoSeguro = marco.width > 0 && marco.height > 0
            ? marco : CGRect(x: 0.08, y: 0.08, width: 0.84, height: 0.16)
        let paso = opciones.separarAnimaciones
            ? forma.idOffice.flatMap { animaciones[$0] } : nil
        let vinculo: HipervinculoPPTXIPad? = {
            guard opciones.conservarHipervinculos,
                  let id = forma.relacionHipervinculo,
                  let relacion = relaciones[id] else { return nil }
            return HipervinculoPPTXIPad(
                texto: forma.texto,
                destino: relacion.objetivo,
                externo: relacion.externa)
        }()

        let relacionRecursoId = forma.relacionImagen ?? forma.relacionObjeto
        var hashRecurso: String?
        var nombreRecurso: String?
        var tipoRelacion = ""
        if let id = relacionRecursoId, let relacion = relaciones[id],
           let url = destinoSeguro(relacion, relativoA: slide, raiz: raiz),
           (opciones.extraerRecursos || opciones.conservarImagenesOriginales),
           let adjunto = try? RepositorioAdjuntos.compartido.importar(url: url) {
            hashRecurso = adjunto.hash
            nombreRecurso = url.lastPathComponent
            tipoRelacion = relacion.tipo
        }

        let tipoPrincipal: TipoElementoPPTXIPad = {
            if forma.esTabla { return .tabla }
            if forma.esEcuacion { return .ecuacion }
            if forma.esGrafica { return .grafica }
            if forma.esDiagrama { return .diagrama }
            if forma.esAudio { return .audio }
            if forma.esVideo { return .video }
            if forma.contenedor == "pic" { return .imagen }
            if hashRecurso != nil { return tipoRecurso(
                URL(fileURLWithPath: nombreRecurso ?? ""),
                tipoRelacion: tipoRelacion) }
            return forma.texto.isEmpty ? .formaVectorial : .texto
        }()

        var salida: [ElementoPPTXIPad] = []
        if tipoPrincipal != .texto && tipoPrincipal != .formaVectorial {
            salida.append(ElementoPPTXIPad(
                idOffice: forma.idOffice,
                nombre: forma.nombre.isEmpty ? tipoPrincipal.nombre : forma.nombre,
                tipo: tipoPrincipal, marcoNormalizado: marcoSeguro,
                texto: forma.texto, hashRecurso: hashRecurso,
                nombreRecurso: nombreRecurso, hipervinculo: vinculo,
                pasoAnimacion: paso, geometria: forma.geometria,
                colorRellenoARGB: forma.colorRellenoARGB,
                colorTrazoARGB: forma.colorTrazoARGB,
                grosorTrazo: forma.grosorTrazo,
                tabla: forma.filasTabla.isEmpty
                    ? nil : TablaPPTXIPad(filas: forma.filasTabla),
                vectorial: tipoPrincipal == .grafica || tipoPrincipal == .diagrama))
            return salida
        }

        let tieneFormaVisible = forma.geometria != nil
            && (forma.colorRellenoARGB != nil || forma.colorTrazoARGB != nil
                || forma.texto.isEmpty)
        if tieneFormaVisible {
            salida.append(ElementoPPTXIPad(
                idOffice: forma.idOffice,
                nombre: forma.nombre.isEmpty ? "Forma" : forma.nombre,
                tipo: .formaVectorial, marcoNormalizado: marcoSeguro,
                texto: forma.texto, hipervinculo: vinculo,
                pasoAnimacion: paso, geometria: forma.geometria,
                colorRellenoARGB: forma.colorRellenoARGB,
                colorTrazoARGB: forma.colorTrazoARGB,
                grosorTrazo: forma.grosorTrazo, vectorial: true))
        }
        if opciones.conservarTextoSeleccionable && !forma.texto.isEmpty {
            salida.append(ElementoPPTXIPad(
                idOffice: forma.idOffice,
                nombre: forma.nombre.isEmpty ? "Texto" : forma.nombre,
                tipo: .texto, marcoNormalizado: marcoSeguro,
                texto: forma.texto, hipervinculo: vinculo,
                pasoAnimacion: paso, vectorial: true))
        } else if salida.isEmpty {
            salida.append(ElementoPPTXIPad(
                idOffice: forma.idOffice,
                nombre: forma.nombre.isEmpty ? "Elemento" : forma.nombre,
                tipo: .desconocido, marcoNormalizado: marcoSeguro,
                texto: forma.texto, hipervinculo: vinculo,
                pasoAnimacion: paso))
        }
        return salida
    }

    private static func tipoRecurso(_ url: URL,
                                    tipoRelacion: String) -> TipoElementoPPTXIPad {
        let ext = url.pathExtension.lowercased()
        if ["png", "jpg", "jpeg", "heic", "tif", "tiff", "webp", "gif",
            "bmp", "svg", "emf", "wmf"].contains(ext) { return .imagen }
        if ["m4a", "mp3", "wav", "aac", "aiff", "caf"].contains(ext) {
            return .audio
        }
        if ["mp4", "mov", "m4v", "avi"].contains(ext) { return .video }
        let tipo = tipoRelacion.lowercased()
        if tipo.contains("chart") { return .grafica }
        if tipo.contains("diagram") { return .diagrama }
        return .objetoIncrustado
    }

    private static func renderizar(
        _ formas: [FormaPPTX],
        relaciones: [String: RelacionPPTX],
        carpetaSlide: URL,
        raiz: URL,
        tamanoEMU: CGSize
    ) -> UIImage {
        let ancho: CGFloat = 1_920
        let alto = max(720, ancho * tamanoEMU.height / max(tamanoEMU.width, 1))
        let tamano = CGSize(width: ancho, height: alto)
        let sx = ancho / max(tamanoEMU.width, 1)
        let sy = alto / max(tamanoEMU.height, 1)
        return UIGraphicsImageRenderer(size: tamano).image { contexto in
            UIColor.white.setFill(); contexto.fill(CGRect(origin: .zero, size: tamano))
            var flujoY: CGFloat = 70
            for forma in formas {
                var marco = CGRect(
                    x: CGFloat(forma.x) * sx, y: CGFloat(forma.y) * sy,
                    width: CGFloat(forma.ancho) * sx,
                    height: CGFloat(forma.alto) * sy)
                if marco.width < 2 || marco.height < 2 {
                    marco = CGRect(x: 80, y: flujoY, width: ancho - 160, height: 120)
                    flujoY += 132
                }
                dibujarForma(forma, marco: marco, contexto: contexto.cgContext)
                if let relacion = forma.relacionImagen,
                   let datosRelacion = relaciones[relacion],
                   let destino = destinoSeguro(
                    datosRelacion,
                    relativoA: carpetaSlide.appendingPathComponent("slide.xml"),
                    raiz: raiz),
                   let imagen = UIImage(contentsOfFile: destino.path) {
                    imagen.draw(in: marco, blendMode: .normal, alpha: 1)
                }
                if forma.esTabla, !forma.filasTabla.isEmpty {
                    dibujarTabla(forma.filasTabla, marco: marco,
                                 contexto: contexto.cgContext)
                } else if !forma.texto.isEmpty {
                    let tamanoFuente = min(max(marco.height * 0.20, 22), 58)
                    let atributos: [NSAttributedString.Key: Any] = [
                        .font: UIFont.systemFont(ofSize: tamanoFuente,
                                                weight: forma.esTitulo ? .semibold : .regular),
                        .foregroundColor: UIColor.label,
                    ]
                    (forma.texto as NSString).draw(
                        with: marco.insetBy(dx: 10, dy: 8),
                        options: [.usesLineFragmentOrigin, .usesFontLeading],
                        attributes: atributos, context: nil)
                }
            }
            if formas.isEmpty {
                ("Diapositiva conservada en el PPTX original" as NSString).draw(
                    at: CGPoint(x: 80, y: 80),
                    withAttributes: [.font: UIFont.systemFont(ofSize: 34),
                                     .foregroundColor: UIColor.secondaryLabel])
            }
        }
    }

    private static func dibujarForma(_ forma: FormaPPTX, marco: CGRect,
                                     contexto: CGContext) {
        guard forma.geometria != nil,
              forma.colorRellenoARGB != nil || forma.colorTrazoARGB != nil else { return }
        contexto.saveGState(); defer { contexto.restoreGState() }
        if let relleno = forma.colorRellenoARGB {
            contexto.setFillColor(UIColor(argb: relleno).cgColor)
        } else { contexto.setFillColor(UIColor.clear.cgColor) }
        contexto.setStrokeColor(UIColor(argb: forma.colorTrazoARGB ?? 0xFF444444).cgColor)
        contexto.setLineWidth(max(forma.grosorTrazo ?? 1, 1))
        let geometria = forma.geometria?.lowercased() ?? "rect"
        if geometria.contains("ellipse") || geometria.contains("oval") {
            contexto.addEllipse(in: marco)
            contexto.drawPath(using: .fillStroke)
        } else if geometria.contains("line") {
            contexto.move(to: CGPoint(x: marco.minX, y: marco.midY))
            contexto.addLine(to: CGPoint(x: marco.maxX, y: marco.midY))
            contexto.strokePath()
        } else {
            contexto.addPath(UIBezierPath(roundedRect: marco,
                                          cornerRadius: geometria.contains("round") ? 18 : 0).cgPath)
            contexto.drawPath(using: .fillStroke)
        }
    }

    private static func dibujarTabla(_ filas: [[String]], marco: CGRect,
                                     contexto: CGContext) {
        let columnas = max(filas.map(\.count).max() ?? 0, 1)
        let cantidadFilas = max(filas.count, 1)
        let ancho = marco.width / CGFloat(columnas)
        let alto = marco.height / CGFloat(cantidadFilas)
        contexto.saveGState(); defer { contexto.restoreGState() }
        contexto.setStrokeColor(UIColor.separator.cgColor)
        for fila in 0...cantidadFilas {
            contexto.move(to: CGPoint(x: marco.minX, y: marco.minY + CGFloat(fila) * alto))
            contexto.addLine(to: CGPoint(x: marco.maxX, y: marco.minY + CGFloat(fila) * alto))
        }
        for columna in 0...columnas {
            contexto.move(to: CGPoint(x: marco.minX + CGFloat(columna) * ancho, y: marco.minY))
            contexto.addLine(to: CGPoint(x: marco.minX + CGFloat(columna) * ancho, y: marco.maxY))
        }
        contexto.strokePath()
        for fila in filas.indices {
            for columna in filas[fila].indices {
                let celda = CGRect(x: marco.minX + CGFloat(columna) * ancho + 5,
                                   y: marco.minY + CGFloat(fila) * alto + 4,
                                   width: ancho - 10, height: alto - 8)
                (filas[fila][columna] as NSString).draw(
                    in: celda,
                    withAttributes: [.font: UIFont.systemFont(ofSize: 18),
                                     .foregroundColor: UIColor.label])
            }
        }
    }
}

private struct RelacionPPTX {
    var objetivo: String
    var tipo: String
    var externa: Bool
}

private struct FormaPPTX {
    var contenedor = "sp"
    var idOffice: String?
    var nombre = ""
    var x: Double = 0
    var y: Double = 0
    var ancho: Double = 0
    var alto: Double = 0
    var texto = ""
    var relacionImagen: String?
    var relacionObjeto: String?
    var relacionHipervinculo: String?
    var esTitulo = false
    var geometria: String?
    var colorRellenoARGB: UInt32?
    var colorTrazoARGB: UInt32?
    var grosorTrazo: CGFloat?
    var esTabla = false
    var esGrafica = false
    var esDiagrama = false
    var esEcuacion = false
    var esAudio = false
    var esVideo = false
    var filasTabla: [[String]] = []
}

private final class LectorSlidePPTX: NSObject, XMLParserDelegate {
    private(set) var formas: [FormaPPTX] = []
    private(set) var ordenAnimacion: [String: Int] = [:]
    private var pila: [FormaPPTX] = []
    private var leyendoTexto = false
    private var acumulado = ""
    private var profundidadXfrm = 0
    private var profundidadLinea = 0
    private var profundidadRelleno = 0
    private var celdaActual: String?
    private var ordenSiguiente = 1

    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        let nombre = local(qName ?? elementName)
        if ["sp", "pic", "graphicFrame", "cxnSp"].contains(nombre) {
            pila.append(FormaPPTX(contenedor: nombre))
            return
        }
        guard !pila.isEmpty || nombre == "spTgt" else { return }
        switch nombre {
        case "cNvPr":
            modificarActual {
                $0.idOffice = atributo("id", attributeDict)
                $0.nombre = atributo("name", attributeDict) ?? ""
            }
        case "xfrm": profundidadXfrm += 1
        case "off" where profundidadXfrm > 0:
            modificarActual {
                $0.x = Double(atributo("x", attributeDict) ?? "0") ?? 0
                $0.y = Double(atributo("y", attributeDict) ?? "0") ?? 0
            }
        case "ext" where profundidadXfrm > 0:
            modificarActual {
                $0.ancho = Double(atributo("cx", attributeDict) ?? "0") ?? 0
                $0.alto = Double(atributo("cy", attributeDict) ?? "0") ?? 0
            }
        case "blip":
            modificarActual { $0.relacionImagen = atributo("embed", attributeDict) }
        case "hlinkClick":
            modificarActual {
                $0.relacionHipervinculo = atributo("id", attributeDict)
                    ?? atributo("action", attributeDict)
            }
        case "ph":
            let tipo = atributo("type", attributeDict) ?? ""
            modificarActual { $0.esTitulo = tipo == "title" || tipo == "ctrTitle" }
        case "prstGeom":
            modificarActual { $0.geometria = atributo("prst", attributeDict) }
        case "solidFill": profundidadRelleno += 1
        case "ln":
            profundidadLinea += 1
            if let ancho = Double(atributo("w", attributeDict) ?? "") {
                modificarActual { $0.grosorTrazo = CGFloat(ancho / 12_700) }
            }
        case "srgbClr" where profundidadRelleno > 0:
            guard let valor = atributo("val", attributeDict),
                  let color = colorARGB(valor) else { break }
            modificarActual {
                if profundidadLinea > 0 { $0.colorTrazoARGB = color }
                else { $0.colorRellenoARGB = color }
            }
        case "tbl": modificarActual { $0.esTabla = true }
        case "tr": modificarActual { $0.filasTabla.append([]) }
        case "tc": celdaActual = ""
        case "chart":
            modificarActual {
                $0.esGrafica = true
                $0.relacionObjeto = atributo("id", attributeDict)
            }
        case "relIds", "dataModel":
            modificarActual {
                $0.esDiagrama = true
                $0.relacionObjeto = atributo("dm", attributeDict)
                    ?? atributo("id", attributeDict)
            }
        case "oMath", "oMathPara": modificarActual { $0.esEcuacion = true }
        case "oleObj":
            modificarActual { $0.relacionObjeto = atributo("id", attributeDict) }
        case "audioFile", "snd":
            modificarActual {
                $0.esAudio = true
                $0.relacionObjeto = atributo("link", attributeDict)
                    ?? atributo("embed", attributeDict)
            }
        case "videoFile":
            modificarActual {
                $0.esVideo = true
                $0.relacionObjeto = atributo("link", attributeDict)
                    ?? atributo("embed", attributeDict)
            }
        case "t": leyendoTexto = true; acumulado = ""
        case "spTgt":
            if let id = atributo("spid", attributeDict), ordenAnimacion[id] == nil {
                ordenAnimacion[id] = ordenSiguiente; ordenSiguiente += 1
            }
        default: break
        }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if leyendoTexto { acumulado += string }
    }

    func parser(_ parser: XMLParser, didEndElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?) {
        let nombre = local(qName ?? elementName)
        if nombre == "t", leyendoTexto {
            let parte = acumulado
            modificarActual {
                if !$0.texto.isEmpty && !parte.isEmpty { $0.texto += " " }
                $0.texto += parte
            }
            if celdaActual != nil {
                if !(celdaActual?.isEmpty ?? true) && !parte.isEmpty { celdaActual? += " " }
                celdaActual? += parte
            }
            leyendoTexto = false
        } else if nombre == "tc", let celda = celdaActual {
            modificarActual {
                if $0.filasTabla.isEmpty { $0.filasTabla.append([]) }
                $0.filasTabla[$0.filasTabla.count - 1].append(celda)
            }
            celdaActual = nil
        } else if nombre == "xfrm" {
            profundidadXfrm = max(profundidadXfrm - 1, 0)
        } else if nombre == "solidFill" {
            profundidadRelleno = max(profundidadRelleno - 1, 0)
        } else if nombre == "ln" {
            profundidadLinea = max(profundidadLinea - 1, 0)
        } else if ["sp", "pic", "graphicFrame", "cxnSp"].contains(nombre),
                  let forma = pila.popLast() {
            formas.append(forma)
        }
    }

    private func modificarActual(_ cambio: (inout FormaPPTX) -> Void) {
        guard !pila.isEmpty else { return }
        cambio(&pila[pila.count - 1])
    }
    private func local(_ nombre: String) -> String {
        nombre.split(separator: ":").last.map(String.init) ?? nombre
    }
    private func atributo(_ local: String, _ atributos: [String: String]) -> String? {
        atributos.first { $0.key.split(separator: ":").last == Substring(local) }?.value
    }
    private func colorARGB(_ valor: String) -> UInt32? {
        guard valor.count == 6, let rgb = UInt32(valor, radix: 16) else { return nil }
        return 0xFF00_0000 | rgb
    }
}

private final class LectorTamanoPPTX: NSObject, XMLParserDelegate {
    var tamano: CGSize?
    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        let nombre = (qName ?? elementName).split(separator: ":").last.map(String.init)
        guard nombre == "sldSz",
              let cx = Double(attributeDict["cx"] ?? ""),
              let cy = Double(attributeDict["cy"] ?? "") else { return }
        tamano = CGSize(width: cx, height: cy)
    }
}

private final class LectorRelacionesPPTX: NSObject, XMLParserDelegate {
    var relaciones: [String: RelacionPPTX] = [:]
    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        let nombre = (qName ?? elementName).split(separator: ":").last.map(String.init)
        if nombre == "Relationship", let id = attributeDict["Id"],
           let objetivo = attributeDict["Target"] {
            relaciones[id] = RelacionPPTX(
                objetivo: objetivo,
                tipo: attributeDict["Type"] ?? "",
                externa: (attributeDict["TargetMode"] ?? "")
                    .caseInsensitiveCompare("External") == .orderedSame)
        }
    }
}

private final class LectorTextoPPTX: NSObject, XMLParserDelegate {
    var textos: [String] = []
    private var leyendo = false
    private var actual = ""
    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        if (qName ?? elementName).split(separator: ":").last == "t" {
            leyendo = true; actual = ""
        }
    }
    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if leyendo { actual += string }
    }
    func parser(_ parser: XMLParser, didEndElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?) {
        if (qName ?? elementName).split(separator: ":").last == "t" {
            if !actual.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                textos.append(actual)
            }
            leyendo = false
        }
    }
}

private final class LectorAutoresPPTX: NSObject, XMLParserDelegate {
    var autores: [String: String] = [:]
    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        let nombre = (qName ?? elementName).split(separator: ":").last.map(String.init) ?? ""
        guard ["cmAuthor", "author", "person"].contains(nombre),
              let id = attributeDict["id"] ?? attributeDict["authorId"] else { return }
        autores[id] = attributeDict["name"] ?? attributeDict["displayName"]
            ?? attributeDict["initials"] ?? "Autor \(id)"
    }
}

private final class LectorComentariosPPTX: NSObject, XMLParserDelegate {
    private let autores: [String: String]
    private(set) var comentarios: [ComentarioPPTXIPad] = []
    private var dentroComentario = false
    private var leyendoTexto = false
    private var texto = ""
    private var autorId: String?
    private var fecha: Date?

    init(autores: [String: String]) { self.autores = autores }

    func parser(_ parser: XMLParser, didStartElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?,
                attributes attributeDict: [String: String] = [:]) {
        let nombre = (qName ?? elementName).split(separator: ":").last.map(String.init) ?? ""
        if ["cm", "comment"].contains(nombre) {
            dentroComentario = true; texto = ""
            autorId = attributeDict["authorId"] ?? attributeDict["authorId"]
            if let raw = attributeDict["dt"] ?? attributeDict["created"] {
                fecha = ISO8601DateFormatter().date(from: raw)
            }
        } else if dentroComentario && ["text", "t"].contains(nombre) {
            leyendoTexto = true
        }
    }
    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if leyendoTexto { texto += string }
    }
    func parser(_ parser: XMLParser, didEndElement elementName: String,
                namespaceURI: String?, qualifiedName qName: String?) {
        let nombre = (qName ?? elementName).split(separator: ":").last.map(String.init) ?? ""
        if ["text", "t"].contains(nombre) { leyendoTexto = false }
        if ["cm", "comment"].contains(nombre), dentroComentario {
            let limpio = texto.trimmingCharacters(in: .whitespacesAndNewlines)
            if !limpio.isEmpty {
                comentarios.append(ComentarioPPTXIPad(
                    autor: autorId.flatMap { autores[$0] }, texto: limpio,
                    fecha: fecha))
            }
            dentroComentario = false; texto = ""; autorId = nil; fecha = nil
        }
    }
}
