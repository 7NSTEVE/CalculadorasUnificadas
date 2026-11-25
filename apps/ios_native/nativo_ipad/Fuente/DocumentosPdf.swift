import PDFKit
import UIKit

/// Importar PDF y exportar hojas a PDF.
///
/// PDFKit abre el archivo y pinta la página. Lo que **no** sabe PDFKit es qué
/// tamaño de lienzo le toca a cada hoja, si la presentación viene apaisada, o
/// cuánto margen dejar al lado para anotar. Eso es geometría y lo decide el
/// núcleo C++, donde está probado.
enum DocumentosPdf {

    /// API histórica: conserva un PDFPage por entrada. Se mantiene para no romper
    /// consumidores antiguos, pero no debe usarse con documentos grandes porque
    /// fuerza al array resultante a retener todo el árbol de páginas.
    struct PaginaImportada {
        let indice: Int
        /// Tamaño de la hoja de la nota, en píxeles lógicos.
        let tamano: CGSize
        /// Dónde cae el PDF dentro de esa hoja. Si se pidió margen para anotar,
        /// el PDF ocupa menos que la hoja y el hueco queda a la derecha.
        let marco: CGRect
        let horizontal: Bool
        /// La página tal como la da PDFKit, para pintarla.
        let pagina: PDFPage
    }

    /// Descriptor ligero para documentos grandes. No retiene PDFPage ni
    /// PDFDocument; una biblioteca de 500 páginas mantiene sólo geometría.
    struct PaginaImportadaLigera: Equatable, Identifiable {
        var id: Int { indice }
        let indice: Int
        let tamano: CGSize
        let marco: CGRect
        let horizontal: Bool
    }

    struct CatalogoLigero: Equatable {
        let url: URL
        let totalPaginas: Int
        let paginas: [PaginaImportadaLigera]

        var estaVacio: Bool { totalPaginas == 0 || paginas.isEmpty }
    }

    enum Ajuste: Int32 { case tamanoReal = 0, anchoDelLienzo, encajar }

    struct Opciones {
        var ppp: Float = 150
        var ajuste: Ajuste = .tamanoReal
        var anchoObjetivo: Float = 1240
        var altoObjetivo: Float = 1754
        /// Deja sitio en blanco a la derecha para escribir al lado de la
        /// diapositiva en vez de encima, que es lo que la tapa.
        var margenParaNotas = false
        var fraccionMargen: Float = 0.35
    }

    // MARK: - Importar

    /// Compatibilidad con la ruta antigua. Para PDFs grandes usar `catalogar` y
    /// `solicitarImagen`, que no retienen todas las PDFPage simultáneamente.
    @available(*, deprecated, message: "Use catalogar(_:opciones:rango:) + solicitarImagen para PDFs grandes")
    static func importar(_ url: URL, opciones: Opciones = Opciones()) -> [PaginaImportada] {
        conDocumento(url) { documento in
            var salida: [PaginaImportada] = []
            salida.reserveCapacity(documento.pageCount)
            for indice in 0..<documento.pageCount {
                guard let pagina = documento.page(at: indice),
                      let geometria = geometria(pagina: pagina, opciones: opciones) else {
                    continue
                }
                salida.append(PaginaImportada(
                    indice: indice,
                    tamano: geometria.tamano,
                    marco: geometria.marco,
                    horizontal: geometria.horizontal,
                    pagina: pagina))
            }
            return salida
        } ?? []
    }

    /// Lee únicamente la geometría necesaria para construir el lienzo. El
    /// PDFDocument vive sólo durante esta llamada; el resultado no retiene
    /// páginas PDF y por eso escala mucho mejor a cientos o miles de hojas.
    static func catalogar(
        _ url: URL,
        opciones: Opciones = Opciones(),
        rango: Range<Int>? = nil
    ) -> CatalogoLigero {
        conDocumento(url) { documento in
            let total = documento.pageCount
            let limite = rango ?? 0..<total
            let inicio = max(limite.lowerBound, 0)
            let fin = min(limite.upperBound, total)
            guard inicio < fin else {
                return CatalogoLigero(url: url.standardizedFileURL,
                                      totalPaginas: total,
                                      paginas: [])
            }

            var paginas: [PaginaImportadaLigera] = []
            paginas.reserveCapacity(fin - inicio)
            for indice in inicio..<fin {
                autoreleasepool {
                    guard let pagina = documento.page(at: indice),
                          let geometria = geometria(pagina: pagina, opciones: opciones) else {
                        return
                    }
                    paginas.append(PaginaImportadaLigera(
                        indice: indice,
                        tamano: geometria.tamano,
                        marco: geometria.marco,
                        horizontal: geometria.horizontal))
                }
            }
            return CatalogoLigero(url: url.standardizedFileURL,
                                  totalPaginas: total,
                                  paginas: paginas)
        } ?? CatalogoLigero(url: url.standardizedFileURL,
                            totalPaginas: 0,
                            paginas: [])
    }

    /// Procesamiento streaming para importadores que quieran materializar el
    /// contenido una página a la vez. Mantiene un solo PDFDocument abierto y no
    /// construye un array de PDFPage. Devuelve cuántas páginas fueron entregadas.
    @discardableResult
    static func enumerarPaginas(
        _ url: URL,
        opciones: Opciones = Opciones(),
        rango: Range<Int>? = nil,
        visitar: (PaginaImportadaLigera, PDFPage) throws -> Void
    ) throws -> Int {
        var errorCapturado: Error?
        var cantidad = 0
        _ = conDocumento(url) { documento -> Void in
            let limite = rango ?? 0..<documento.pageCount
            let inicio = max(limite.lowerBound, 0)
            let fin = min(limite.upperBound, documento.pageCount)
            guard inicio < fin else { return }
            for indice in inicio..<fin {
                autoreleasepool {
                    guard errorCapturado == nil,
                          let pagina = documento.page(at: indice),
                          let geometria = geometria(pagina: pagina, opciones: opciones) else {
                        return
                    }
                    do {
                        try visitar(PaginaImportadaLigera(
                            indice: indice,
                            tamano: geometria.tamano,
                            marco: geometria.marco,
                            horizontal: geometria.horizontal), pagina)
                        cantidad += 1
                    } catch {
                        errorCapturado = error
                    }
                }
                if errorCapturado != nil { break }
            }
        }
        if let errorCapturado { throw errorCapturado }
        return cantidad
    }

    /// Preview recomendado para UI: abre/rasteriza fuera del main thread, usa
    /// caché por coste y coalescing de solicitudes simultáneas.
    @discardableResult
    static func solicitarImagen(
        url: URL,
        indice: Int,
        tamano: CGSize,
        escala: CGFloat = UIScreen.main.scale,
        completion: @escaping (UIImage?) -> Void
    ) -> TokenMiniaturaPDFIPad {
        RenderizadorMiniaturasPDFIPad.compartido.solicitar(
            url: url,
            pagina: indice,
            tamano: tamano,
            escala: escala,
            caja: .mediaBox,
            completion: completion)
    }

    /// Pinta una página ya cargada. Se conserva para exportadores/operaciones
    /// explícitas donde el llamador ya posee PDFPage. No debe llamarse desde un
    /// scroll de UI para previews repetidos.
    static func imagen(de pagina: PDFPage, tamano: CGSize, escala: CGFloat = 0) -> UIImage {
        let formato = UIGraphicsImageRendererFormat.default()
        formato.scale = escala > 0 ? escala : UIScreen.main.scale
        formato.opaque = true

        return UIGraphicsImageRenderer(size: tamano, format: formato).image { ctx in
            UIColor.white.setFill()
            ctx.fill(CGRect(origin: .zero, size: tamano))

            let caja = pagina.bounds(for: .mediaBox)
            let girado = pagina.rotation == 90 || pagina.rotation == 270
            let anchoFuente = girado ? caja.height : caja.width
            let altoFuente = girado ? caja.width : caja.height
            guard anchoFuente > 0, altoFuente > 0 else { return }

            let cg = ctx.cgContext
            cg.saveGState()
            // PDFKit dibuja con el origen abajo a la izquierda; UIKit, arriba.
            cg.translateBy(x: 0, y: tamano.height)
            cg.scaleBy(x: tamano.width / anchoFuente, y: -tamano.height / altoFuente)
            pagina.draw(with: .mediaBox, to: cg)
            cg.restoreGState()
        }
    }

    private static func conDocumento<T>(_ url: URL, _ bloque: (PDFDocument) -> T) -> T? {
        let necesitaPermiso = url.startAccessingSecurityScopedResource()
        defer { if necesitaPermiso { url.stopAccessingSecurityScopedResource() } }
        guard let documento = PDFDocument(url: url) else { return nil }
        return bloque(documento)
    }

    private static func geometria(
        pagina: PDFPage,
        opciones: Opciones
    ) -> (tamano: CGSize, marco: CGRect, horizontal: Bool)? {
        let caja = pagina.bounds(for: .mediaBox)
        guard caja.width > 0, caja.height > 0 else { return nil }
        let rotacion = Int32(pagina.rotation)

        var ancho: Float = 0
        var alto: Float = 0
        tinta_pdf_tamano_pagina(Float(caja.width), Float(caja.height), rotacion,
                                opciones.ppp, opciones.ajuste.rawValue,
                                opciones.anchoObjetivo, opciones.altoObjetivo,
                                opciones.margenParaNotas, opciones.fraccionMargen,
                                &ancho, &alto)

        var mx: Float = 0, my: Float = 0, mw: Float = 0, mh: Float = 0
        tinta_pdf_marco(Float(caja.width), Float(caja.height), rotacion,
                        opciones.ppp, opciones.ajuste.rawValue,
                        opciones.anchoObjetivo, opciones.altoObjetivo,
                        &mx, &my, &mw, &mh)

        let horizontal = tinta_pdf_orientacion(Float(caja.width), Float(caja.height),
                                               rotacion) == 1
        return (
            CGSize(width: CGFloat(ancho), height: CGFloat(alto)),
            CGRect(x: CGFloat(mx), y: CGFloat(my),
                   width: CGFloat(mw), height: CGFloat(mh)),
            horizontal)
    }

    // MARK: - Exportar

    /// Exporta hojas a un PDF de verdad, no a una foto de la pantalla.
    ///
    /// La tinta sale como polígonos, así que el PDF se puede ampliar sin que se
    /// pixele y pesa una fracción de lo que pesaría en mapa de bits. La pauta
    /// del cuaderno —rayado, cuadrícula, puntos— va dibujada igual, que es lo
    /// que se pidió: exportar *una página rayada* como PDF.
    static func exportar(sesiones: [Sesion], tamanos: [CGSize], a destino: URL) throws {
        let formato = UIGraphicsPDFRendererFormat()
        let primera = tamanos.first ?? CGSize(width: 612, height: 792)
        let renderizador = UIGraphicsPDFRenderer(
            bounds: CGRect(origin: .zero, size: primera), format: formato)

        try renderizador.writePDF(to: destino) { ctx in
            for (indice, sesion) in sesiones.enumerated() {
                let tamano = indice < tamanos.count ? tamanos[indice] : primera
                ctx.beginPage(withBounds: CGRect(origin: .zero, size: tamano),
                              pageInfo: [:])
                dibujar(sesion: sesion, tamano: tamano, en: ctx.cgContext)
            }
        }
    }

    static func exportarPagina(_ sesion: Sesion, tamano: CGSize, a destino: URL) throws {
        try exportar(sesiones: [sesion], tamanos: [tamano], a: destino)
    }

    private static func dibujar(sesion: Sesion, tamano: CGSize, en cg: CGContext) {
        // Fondo del cuaderno: el color lo decide el núcleo, igual que en
        // pantalla, para que lo exportado se vea como lo que se escribió.
        cg.setFillColor(UIColor(argb: sesion.colorPapel).cgColor)
        cg.fill(CGRect(origin: .zero, size: tamano))

        // Pauta. Solo se pide la visible, que en un lienzo de alto infinito
        // dibujarla entera no terminaría nunca.
        let pauta = sesion.pautaVisible(x: 0, y: 0,
                                        ancho: Float(tamano.width),
                                        alto: Float(tamano.height))
        if !pauta.isEmpty {
            cg.setStrokeColor(UIColor(argb: sesion.colorPauta).cgColor)
            cg.setLineWidth(1)
            var i = 0
            while i + 3 < pauta.count {
                cg.move(to: CGPoint(x: CGFloat(pauta[i]), y: CGFloat(pauta[i + 1])))
                cg.addLine(to: CGPoint(x: CGFloat(pauta[i + 2]), y: CGFloat(pauta[i + 3])))
                i += 4
            }
            cg.strokePath()
        }

        // Tinta. El núcleo entrega triángulos sueltos: los mismos que se suben a
        // Metal, así que el PDF y la pantalla dibujan exactamente la misma forma.
        for indice in 0..<sesion.cantidadTrazos {
            guard let trazo = sesion.verticesDelTrazo(indice) else { continue }
            cg.setFillColor(UIColor(argb: trazo.color).cgColor)
            var v = 0
            while v + 2 < trazo.cantidad {
                let p = { (n: Int) in
                    CGPoint(x: CGFloat(trazo.datos[n * 2]),
                            y: CGFloat(trazo.datos[n * 2 + 1]))
                }
                cg.move(to: p(v))
                cg.addLine(to: p(v + 1))
                cg.addLine(to: p(v + 2))
                cg.closePath()
                v += 3
            }
            cg.fillPath()
        }
    }
}
