import PDFKit
import UIKit
import Vision

enum FormatoExportacionSeleccionIPad: String, CaseIterable {
    case png
    case pdf
    case svg

    var nombre: String { rawValue.uppercased() }
}

enum ErrorExportacionSeleccionIPad: LocalizedError {
    case vacia
    case lienzoDemasiadoGrande
    case noCodifica

    var errorDescription: String? {
        switch self {
        case .vacia: return "La selección no contiene elementos exportables."
        case .lienzoDemasiadoGrande:
            return "La selección supera el límite seguro de rasterización. Use PDF o SVG."
        case .noCodifica: return "No se pudo codificar la selección."
        }
    }
}

/// Exportación local de una selección. PNG rasteriza con límite de memoria;
/// PDF y SVG conservan la geometría vectorial de la tinta.
enum ExportadorSeleccionIPad {
    private static let margen: CGFloat = 18

    static func marco(_ seleccion: SeleccionLienzo, sesion: Sesion) -> CGRect? {
        var marcos = seleccion.trazos.compactMap(sesion.marcoTrazo)
        let textos = sesion.textos
        marcos += seleccion.textos.compactMap {
            textos.indices.contains($0) ? textos[$0].marco : nil
        }
        let medios = sesion.medios
        marcos += seleccion.medios.compactMap {
            medios.indices.contains($0) ? medios[$0].marco : nil
        }
        return marcos.reduce(nil as CGRect?) { $0?.union($1) ?? $1 }
            .map { $0.insetBy(dx: -margen, dy: -margen) }
    }

    static func imagen(seleccion: SeleccionLienzo, sesion: Sesion,
                       escalaSolicitada: CGFloat = 2,
                       limitePixeles: CGFloat = 8192) throws -> UIImage {
        guard let caja = marco(seleccion, sesion: sesion) else {
            throw ErrorExportacionSeleccionIPad.vacia
        }
        let maximo = min(max(limitePixeles, 512), 8192)
        let escala = min(max(escalaSolicitada, 1),
                         maximo / max(max(caja.width, caja.height), 1))
        guard caja.width * escala <= maximo, caja.height * escala <= maximo,
              caja.width > 0, caja.height > 0 else {
            throw ErrorExportacionSeleccionIPad.lienzoDemasiadoGrande
        }
        let formato = UIGraphicsImageRendererFormat()
        formato.scale = escala
        formato.opaque = false
        return UIGraphicsImageRenderer(size: caja.size, format: formato).image { contexto in
            contexto.cgContext.translateBy(x: -caja.minX, y: -caja.minY)
            dibujar(seleccion, sesion: sesion, en: contexto.cgContext)
        }
    }

    static func exportar(_ formato: FormatoExportacionSeleccionIPad,
                         seleccion: SeleccionLienzo, sesion: Sesion,
                         titulo: String = "seleccion") throws -> URL {
        guard let caja = marco(seleccion, sesion: sesion) else {
            throw ErrorExportacionSeleccionIPad.vacia
        }
        let carpeta = try carpetaExportaciones()
        let nombre = nombreSeguro(titulo) + "-" + selloFecha() + "." + formato.rawValue
        let destino = carpeta.appendingPathComponent(nombre)
        switch formato {
        case .png:
            guard let datos = try imagen(seleccion: seleccion, sesion: sesion).pngData()
            else { throw ErrorExportacionSeleccionIPad.noCodifica }
            try datos.write(to: destino, options: .atomic)
        case .pdf:
            let pagina = CGRect(origin: .zero, size: caja.size)
            let datos = UIGraphicsPDFRenderer(bounds: pagina).pdfData { renderer in
                renderer.beginPage()
                UIColor.white.setFill(); renderer.cgContext.fill(pagina)
                renderer.cgContext.translateBy(x: -caja.minX, y: -caja.minY)
                dibujar(seleccion, sesion: sesion, en: renderer.cgContext)
            }
            try datos.write(to: destino, options: .atomic)
        case .svg:
            let datos = Data(svg(seleccion: seleccion, sesion: sesion,
                                 caja: caja).utf8)
            try datos.write(to: destino, options: .atomic)
        }
        return destino
    }

    static func textoDirecto(_ seleccion: SeleccionLienzo,
                             sesion: Sesion) -> String {
        let textos = sesion.textos
        return seleccion.textos.compactMap {
            textos.indices.contains($0) ? textos[$0].texto : nil
        }.joined(separator: "\n\n")
    }

    static func reconocerTextoLocal(_ seleccion: SeleccionLienzo, sesion: Sesion,
                                    completado: @escaping (Result<String, Error>) -> Void) {
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let imagen = try self.imagen(seleccion: seleccion, sesion: sesion,
                                             escalaSolicitada: 2)
                guard let cg = imagen.cgImage else {
                    throw ErrorExportacionSeleccionIPad.noCodifica
                }
                let solicitud = VNRecognizeTextRequest()
                solicitud.recognitionLevel = .accurate
                solicitud.usesLanguageCorrection = true
                solicitud.recognitionLanguages = ["es-ES", "en-US"]
                try VNImageRequestHandler(cgImage: cg).perform([solicitud])
                let texto = (solicitud.results ?? []).compactMap {
                    $0.topCandidates(1).first?.string
                }.joined(separator: "\n")
                DispatchQueue.main.async { completado(.success(texto)) }
            } catch {
                DispatchQueue.main.async { completado(.failure(error)) }
            }
        }
    }

    private static func dibujar(_ seleccion: SeleccionLienzo, sesion: Sesion,
                                en contexto: CGContext) {
        contexto.saveGState()
        for indice in seleccion.trazos {
            guard let trazo = sesion.verticesDelTrazo(indice) else { continue }
            let puntos = (0..<trazo.cantidad).map {
                CGPoint(x: CGFloat(trazo.datos[$0 * 2]),
                        y: CGFloat(trazo.datos[$0 * 2 + 1]))
            }
            contexto.setFillColor(UIColor(argb: trazo.color).cgColor)
            var indicePunto = 0
            while indicePunto + 2 < puntos.count {
                contexto.move(to: puntos[indicePunto])
                contexto.addLine(to: puntos[indicePunto + 1])
                contexto.addLine(to: puntos[indicePunto + 2])
                contexto.closePath()
                indicePunto += 3
            }
            contexto.fillPath()
        }

        let textos = sesion.textos
        for indice in seleccion.textos where textos.indices.contains(indice) {
            let elemento = textos[indice]
            contexto.saveGState()
            contexto.translateBy(x: elemento.marco.midX, y: elemento.marco.midY)
            contexto.rotate(by: elemento.rotacion)
            let local = CGRect(x: -elemento.marco.width / 2,
                               y: -elemento.marco.height / 2,
                               width: elemento.marco.width,
                               height: elemento.marco.height)
            (elemento.texto as NSString).draw(
                in: local,
                withAttributes: [.font: UIFont.preferredFont(forTextStyle: .body),
                                 .foregroundColor: UIColor(
                                    argb: sesion.colorTintaSugerido)])
            contexto.restoreGState()
        }

        let medios = sesion.medios
        for indice in seleccion.medios where medios.indices.contains(indice) {
            dibujar(medios[indice], documento: sesion.documentoPersistenteId,
                    en: contexto)
        }
        contexto.restoreGState()
    }

    private static func dibujar(_ medio: ElementoMedio, documento: UInt64,
                                en contexto: CGContext) {
        let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
            documento: documento, elemento: medio.id)
        guard !fondo.oculto else { return }
        contexto.saveGState(); defer { contexto.restoreGState() }
        contexto.translateBy(x: medio.marco.midX, y: medio.marco.midY)
        contexto.rotate(by: medio.rotacion)
        contexto.setAlpha(medio.opacidad * (fondo.esFondo ? fondo.opacidad : 1))
        let local = CGRect(x: -medio.marco.width / 2, y: -medio.marco.height / 2,
                           width: medio.marco.width, height: medio.marco.height)
        if let ruta = RenderizadorClipPDFDocumentoIPad.ruta(
            documento: documento, elementoId: medio.id, en: local) {
            contexto.addPath(ruta.cgPath); contexto.clip()
        }
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash) else {
            UIColor.tertiarySystemFill.setFill(); contexto.fill(local)
            return
        }
        if medio.tipo == .imagen, let original = UIImage(contentsOfFile: url.path) {
            let recortada = Medios.recortar(original, con: medio.recorte)
            let edicion = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: documento, id: medio.id)
            let procesada = ProcesadorImagenLocal.procesar(recortada,
                                                            ajustes: edicion)
            ProcesadorFondoObjetoIPad.procesar(procesada, ajustes: fondo)
                .draw(in: local)
        } else if medio.tipo == .pdf,
                  let completa = CachePDFExportacionIPad.compartido.imagen(
                    url: url, pagina: medio.paginaFuente, tamano: local.size) {
            let visible = medio.recorte.esCompleto
                ? completa : Medios.recortar(completa, con: medio.recorte)
            ProcesadorFondoObjetoIPad.procesar(visible, ajustes: fondo)
                .draw(in: local)
        } else {
            UIColor.tertiarySystemFill.setFill(); contexto.fill(local)
            let nombre = medio.nombre as NSString
            nombre.draw(in: local.insetBy(dx: 8, dy: 8),
                        withAttributes: [.font: UIFont.preferredFont(forTextStyle: .caption1),
                                         .foregroundColor: UIColor.secondaryLabel])
        }
    }

    private static func svg(seleccion: SeleccionLienzo, sesion: Sesion,
                            caja: CGRect) -> String {
        var partes = [
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
            "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"\(numero(caja.width))\" height=\"\(numero(caja.height))\" viewBox=\"\(numero(caja.minX)) \(numero(caja.minY)) \(numero(caja.width)) \(numero(caja.height))\">",
        ]
        for indice in seleccion.trazos {
            guard let trazo = sesion.verticesDelTrazo(indice) else { continue }
            let color = colorSVG(trazo.color)
            var punto = 0
            while punto + 2 < trazo.cantidad {
                let valores = (0..<3).map { desplazamiento -> String in
                    let i = punto + desplazamiento
                    return "\(numero(CGFloat(trazo.datos[i * 2]))),\(numero(CGFloat(trazo.datos[i * 2 + 1])))"
                }.joined(separator: " ")
                partes.append("<polygon points=\"\(valores)\" fill=\"\(color.rgb)\" fill-opacity=\"\(color.alpha)\"/>")
                punto += 3
            }
        }
        let textos = sesion.textos
        for indice in seleccion.textos where textos.indices.contains(indice) {
            let elemento = textos[indice]
            let centroX = elemento.marco.midX, centroY = elemento.marco.midY
            partes.append("<foreignObject x=\"\(numero(elemento.marco.minX))\" y=\"\(numero(elemento.marco.minY))\" width=\"\(numero(elemento.marco.width))\" height=\"\(numero(elemento.marco.height))\" transform=\"rotate(\(numero(elemento.rotacion * 180 / .pi)) \(numero(centroX)) \(numero(centroY)))\"><div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"font:17px system-ui;white-space:pre-wrap\">\(escaparXML(elemento.texto))</div></foreignObject>")
        }
        let medios = sesion.medios
        for indice in seleccion.medios where medios.indices.contains(indice) {
            let medio = medios[indice]
            let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: sesion.documentoPersistenteId, elemento: medio.id)
            if fondo.oculto { continue }
            guard let miniatura = miniatura(
                medio, documento: sesion.documentoPersistenteId),
                  let png = miniatura.pngData() else {
                partes.append("<rect x=\"\(numero(medio.marco.minX))\" y=\"\(numero(medio.marco.minY))\" width=\"\(numero(medio.marco.width))\" height=\"\(numero(medio.marco.height))\" fill=\"#d1d1d6\"/>")
                continue
            }
            let opacidad = medio.opacidad * (fondo.esFondo ? fondo.opacidad : 1)
            partes.append("<image x=\"\(numero(medio.marco.minX))\" y=\"\(numero(medio.marco.minY))\" width=\"\(numero(medio.marco.width))\" height=\"\(numero(medio.marco.height))\" opacity=\"\(numero(opacidad))\" transform=\"rotate(\(numero(medio.rotacion * 180 / .pi)) \(numero(medio.marco.midX)) \(numero(medio.marco.midY)))\" href=\"data:image/png;base64,\(png.base64EncodedString())\"/>")
        }
        partes.append("</svg>")
        return partes.joined(separator: "\n")
    }

    private static func miniatura(_ medio: ElementoMedio,
                                  documento: UInt64) -> UIImage? {
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash) else {
            return nil
        }
        let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
            documento: documento, elemento: medio.id)
        if medio.tipo == .imagen, let imagen = UIImage(contentsOfFile: url.path) {
            let recortada = Medios.recortar(imagen, con: medio.recorte)
            let edicion = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: documento, id: medio.id)
            return ProcesadorFondoObjetoIPad.procesar(
                ProcesadorImagenLocal.procesar(recortada, ajustes: edicion),
                ajustes: fondo)
        }
        if medio.tipo == .pdf,
           let completa = CachePDFExportacionIPad.compartido.imagen(
            url: url, pagina: medio.paginaFuente,
            tamano: CGSize(width: 1200, height: 1200)) {
            let visible = medio.recorte.esCompleto
                ? completa : Medios.recortar(completa, con: medio.recorte)
            return ProcesadorFondoObjetoIPad.procesar(
                RenderizadorClipPDFDocumentoIPad.enmascarar(
                    visible, documento: documento, elementoId: medio.id),
                ajustes: fondo)
        }
        return nil
    }

    private static func carpetaExportaciones() throws -> URL {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                  in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Tinta Exportaciones/Selecciones",
                                                        isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        return carpeta
    }

    private static func nombreSeguro(_ valor: String) -> String {
        let permitido = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_"))
        let limpio = valor.unicodeScalars.map { permitido.contains($0) ? Character(String($0)) : "-" }
        return String(limpio).prefix(80).description
    }

    private static func selloFecha() -> String {
        let formato = DateFormatter()
        formato.locale = Locale(identifier: "en_US_POSIX")
        formato.dateFormat = "yyyyMMdd-HHmmss"
        return formato.string(from: Date())
    }

    private static func numero(_ valor: CGFloat) -> String {
        String(format: "%.3f", Double(valor))
    }

    private static func escaparXML(_ texto: String) -> String {
        texto.replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\"", with: "&quot;")
    }

    private static func colorSVG(_ argb: UInt32) -> (rgb: String, alpha: String) {
        let r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF
        let a = CGFloat((argb >> 24) & 0xFF) / 255
        return (String(format: "#%02X%02X%02X", r, g, b), numero(a))
    }
}
