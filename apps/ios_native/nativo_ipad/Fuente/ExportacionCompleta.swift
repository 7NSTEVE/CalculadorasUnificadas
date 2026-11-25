import PDFKit
import UIKit

struct PaqueteTintaAutocontenido:Codable {
    var version=2;var creado=Date();var documentoId:UInt64;var titulo:String
    var tinta:Data;var bloques:[BloqueLienzo];var tecnicos:[ObjetoTecnico]
    var regiones:[RegionFondoDocumento];var capas:EstadoCapasDocumento
    var fondos:[String:AjustesFondoObjetoIPad]
    var recursos:[String:Data];var manifiesto:[String:String]
}

enum FormatoExportacionIPad:String,CaseIterable {
    case pdfCompleto="PDF completo"
    case pdfVisible="PDF de región visible"
    case pngVisible="PNG de región visible"
    case svgTinta="SVG vectorial de tinta"
    case tintaNativa="Documento .tinta"
    case paquete="Paquete autocontenido .tintapack"
    case markdown="Texto/Markdown portable"
}

enum ErrorExportacionCompleta: LocalizedError {
    case noSerializa
    case noRenderiza
    case noRecurso
    case regionInvalida
    case demasiadasPaginas(Int)

    var errorDescription: String? {
        switch self {
        case .noSerializa: return "No se pudo serializar la sesión."
        case .noRenderiza: return "No se pudo renderizar la selección."
        case .noRecurso: return "Falta un recurso referenciado."
        case .regionInvalida:
            return "El lienzo no tiene un tamaño finito y válido para exportar."
        case .demasiadasPaginas(let cantidad):
            return "El lienzo produciría \(cantidad) páginas. Recórtelo al contenido o exporte una región visible."
        }
    }
}

struct PaginaPDFTinta: Equatable {
    let regionDocumento: CGRect
}

enum PlanificadorPDFTinta {
    static func paginas(
        para region: CGRect,
        maximoPaginas: Int = 500
    ) throws -> [PaginaPDFTinta] {
        guard region.esFinitaPositiva else {
            throw ErrorExportacionCompleta.regionInvalida
        }

        let area = region.width * region.height
        if max(region.width, region.height) <= 4096, area <= 8_000_000 {
            return [PaginaPDFTinta(regionDocumento: region)]
        }

        let apaisado = region.width > region.height
        let tamanoBase = apaisado
            ? CGSize(width: 1754, height: 1240)
            : CGSize(width: 1240, height: 1754)
        let anchoPagina = min(region.width, tamanoBase.width)
        let altoPagina = min(region.height, tamanoBase.height)
        let columnasCalculadas = ceil(region.width / anchoPagina)
        let filasCalculadas = ceil(region.height / altoPagina)
        guard columnasCalculadas <= CGFloat(maximoPaginas),
              filasCalculadas <= CGFloat(maximoPaginas) else {
            throw ErrorExportacionCompleta.demasiadasPaginas(Int.max)
        }
        let columnas = max(Int(columnasCalculadas), 1)
        let filas = max(Int(filasCalculadas), 1)
        guard columnas <= maximoPaginas,
              filas <= maximoPaginas,
              columnas <= maximoPaginas / filas else {
            let estimadas = columnas.multipliedReportingOverflow(by: filas)
            throw ErrorExportacionCompleta.demasiadasPaginas(
                estimadas.overflow ? Int.max : estimadas.partialValue)
        }

        var salida: [PaginaPDFTinta] = []
        salida.reserveCapacity(columnas * filas)
        for fila in 0..<filas {
            for columna in 0..<columnas {
                let x = region.minX + CGFloat(columna) * anchoPagina
                let y = region.minY + CGFloat(fila) * altoPagina
                salida.append(PaginaPDFTinta(regionDocumento: CGRect(
                    x: x, y: y,
                    width: min(anchoPagina, region.maxX - x),
                    height: min(altoPagina, region.maxY - y))))
            }
        }
        return salida
    }
}

private extension CGRect {
    var esFinitaPositiva: Bool {
        [minX, minY, width, height, maxX, maxY].allSatisfy(\.isFinite)
            && width > 0 && height > 0 && !isNull && !isInfinite
    }
}

enum ExportadorCompletoIPad {
    static func exportar(_ formato:FormatoExportacionIPad,sesion:Sesion,navegador:VistaLienzoNavegable,titulo:String="Apunte")throws->URL {
        let region: CGRect
        if formato == .pdfVisible || formato == .pngVisible {
            region = navegador.regionVisibleDocumento
        } else {
            region = try regionCompleta(sesion: sesion, navegador: navegador)
        }
        guard region.esFinitaPositiva else {
            throw ErrorExportacionCompleta.regionInvalida
        }
        let ext:String;switch formato{case .pdfCompleto,.pdfVisible:ext="pdf";case .pngVisible:ext="png";case .svgTinta:ext="svg";case .tintaNativa:ext="tinta";case .paquete:ext="tintapack";case .markdown:ext="md"}
        let nombre=titulo.replacingOccurrences(of:"/",with:"-")+"-\(ISO8601DateFormatter().string(from:Date()).replacingOccurrences(of:":",with:"-"))"
        let destino=FileManager.default.temporaryDirectory.appendingPathComponent(nombre).appendingPathExtension(ext)
        switch formato {
        case .pdfCompleto,.pdfVisible:try pdf(sesion:sesion,navegador:navegador,region:region,destino:destino)
        case .pngVisible:guard let imagen=imagen(sesion:sesion,navegador:navegador,region:region),let datos=imagen.pngData()else{throw ErrorExportacionCompleta.noRenderiza};try datos.write(to:destino,options:.atomic)
        case .svgTinta:try svg(sesion:sesion,region:region).write(to:destino,atomically:true,encoding:.utf8)
        case .tintaNativa:guard let datos=sesion.guardar()else{throw ErrorExportacionCompleta.noSerializa};try datos.write(to:destino,options:.atomic)
        case .paquete:let datos=try paquete(sesion:sesion,navegador:navegador,titulo:titulo);try datos.write(to:destino,options:.atomic)
        case .markdown:try markdown(sesion:sesion,navegador:navegador,titulo:titulo).write(to:destino,atomically:true,encoding:.utf8)
        };return destino
    }

    static func pdf(sesion: Sesion, navegador: VistaLienzoNavegable,
                    region: CGRect, destino: URL) throws {
        let paginas = try PlanificadorPDFTinta.paginas(para: region)
        guard let primera = paginas.first else {
            throw ErrorExportacionCompleta.regionInvalida
        }
        let formato = UIGraphicsPDFRendererFormat()
        formato.documentInfo = [
            kCGPDFContextTitle as String: "Tinta — exportación paginada",
            kCGPDFContextCreator as String: "Tinta iPad",
        ]
        let renderer = UIGraphicsPDFRenderer(
            bounds: CGRect(origin: .zero, size: primera.regionDocumento.size),
            format: formato)
        try renderer.writePDF(to: destino) { contexto in
            for pagina in paginas {
                let bounds = CGRect(origin: .zero,
                                    size: pagina.regionDocumento.size)
                contexto.beginPage(withBounds: bounds, pageInfo: [:])
                contexto.cgContext.saveGState()
                contexto.cgContext.clip(to: bounds)
                contexto.cgContext.translateBy(
                    x: -pagina.regionDocumento.minX,
                    y: -pagina.regionDocumento.minY)
                dibujar(sesion: sesion, navegador: navegador,
                        en: contexto.cgContext,
                        limites: pagina.regionDocumento)
                contexto.cgContext.restoreGState()
            }
        }
    }

    static func imagen(sesion: Sesion, navegador: VistaLienzoNavegable,
                       region: CGRect) -> UIImage? {
        guard region.esFinitaPositiva else { return nil }
        let escalaPantalla = min(UIScreen.main.scale, 2)
        let escalaSegura = min(
            escalaPantalla,
            4096 / max(max(region.width, region.height), 1))
        guard escalaSegura > 0 else { return nil }
        let formato = UIGraphicsImageRendererFormat()
        formato.scale = escalaSegura
        return UIGraphicsImageRenderer(size: region.size, format: formato).image {
            contexto in
            contexto.cgContext.translateBy(x: -region.minX, y: -region.minY)
            dibujar(sesion: sesion, navegador: navegador,
                    en: contexto.cgContext, limites: region)
        }
    }

    private static func regionCompleta(
        sesion: Sesion,
        navegador: VistaLienzoNavegable
    ) throws -> CGRect {
        let documento = CGRect(x: 0, y: 0,
                               width: CGFloat(sesion.anchoPagina),
                               height: CGFloat(sesion.altoPagina))
        guard documento.esFinitaPositiva else {
            throw ErrorExportacionCompleta.regionInvalida
        }
        guard sesion.anchoInfinito || sesion.altoInfinito else {
            return documento
        }

        var contenido: CGRect?
        func incluir(_ marco: CGRect) {
            guard marco.esFinitaPositiva else { return }
            contenido = contenido?.union(marco) ?? marco
        }
        for indice in 0..<sesion.cantidadTrazos {
            if let marco = sesion.marcoTrazo(indice) { incluir(marco) }
        }
        sesion.textos.forEach { incluir($0.marco) }
        sesion.medios.forEach { incluir($0.marco) }
        navegador.capaBloques.bloques.forEach { incluir($0.marco) }
        navegador.capaTecnica.objetos.forEach { objeto in
            guard let primero = objeto.puntos.first else { return }
            let marco = objeto.puntos.dropFirst().reduce(
                CGRect(origin: primero, size: .zero)) { parcial, punto in
                    parcial.union(CGRect(origin: punto, size: .zero))
                }.insetBy(dx: -12, dy: -12)
            incluir(marco)
        }
        RepositorioRegionesFondo.compartido
            .regiones(sesion.documentoPersistenteId).forEach { incluir($0.marco) }

        let visible = navegador.regionVisibleDocumento.intersection(documento)
        var resultado = (contenido?.insetBy(dx: -36, dy: -36) ?? visible)
            .intersection(documento)
        if !sesion.anchoInfinito {
            resultado.origin.x = documento.minX
            resultado.size.width = documento.width
        }
        if !sesion.altoInfinito {
            resultado.origin.y = documento.minY
            resultado.size.height = documento.height
        }
        return resultado.esFinitaPositiva ? resultado : documento
    }

    private static func dibujar(sesion: Sesion, navegador: VistaLienzoNavegable, en cg: CGContext, limites: CGRect) {
        let documento = sesion.documentoPersistenteId
        let estado = RepositorioCapas.compartido.estado(documento)
        
        func exporta(_ cat: CategoriaCapa) -> Bool {
            estado.capas.first { $0.categoria == cat }?.exportar ?? true
        }
        
        cg.setFillColor(UIColor(argb: sesion.colorPapel).cgColor)
        cg.fill(limites)
        
        if exporta(.medios) {
            for medio in sesion.medios {
                let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(documento: documento, elemento: medio.id)
                if fondo.esFondo && !fondo.oculto {
                    dibujar(medio, documento: documento, en: cg)
                }
            }
        }
        
        for r in RepositorioRegionesFondo.compartido.regiones(sesion.documentoPersistenteId) {
            cg.setFillColor(UIColor(argb: r.colorARGB).cgColor)
            cg.fill(r.marco)
            if r.tipo == .rayado || r.tipo == .cuadricula {
                cg.setStrokeColor(UIColor(argb: r.colorPautaARGB).cgColor)
                cg.setLineWidth(0.7)
                var y = r.marco.minY
                while y <= r.marco.maxY {
                    cg.move(to: CGPoint(x: r.marco.minX, y: y))
                    cg.addLine(to: CGPoint(x: r.marco.maxX, y: y))
                    y += r.separacion
                }
                if r.tipo == .cuadricula {
                    var x = r.marco.minX
                    while x <= r.marco.maxX {
                        cg.move(to: CGPoint(x: x, y: r.marco.minY))
                        cg.addLine(to: CGPoint(x: x, y: r.marco.maxY))
                        x += r.separacion
                    }
                }
                cg.strokePath()
            }
        }
        
        let pauta = sesion.pautaVisible(
            x: Float(limites.minX), y: Float(limites.minY),
            ancho: Float(limites.width), alto: Float(limites.height))
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
        
        if exporta(.tinta) {
            for indice in 0..<sesion.cantidadTrazos {
                guard let trazo = sesion.verticesDelTrazo(indice) else { continue }
                cg.setFillColor(UIColor(argb: trazo.color).cgColor)
                var v = 0
                while v + 2 < trazo.cantidad {
                    func p(_ n: Int) -> CGPoint {
                        CGPoint(x: CGFloat(trazo.datos[n * 2]), y: CGFloat(trazo.datos[n * 2 + 1]))
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
        
        if exporta(.textos) {
            for texto in sesion.textos {
                cg.saveGState()
                cg.translateBy(x: texto.marco.midX, y: texto.marco.midY)
                cg.rotate(by: texto.rotacion)
                let r = CGRect(x: -texto.marco.width / 2, y: -texto.marco.height / 2, width: texto.marco.width, height: texto.marco.height)
                (texto.texto as NSString).draw(in: r, withAttributes: [
                    .font: UIFont.preferredFont(forTextStyle: .body),
                    .foregroundColor: UIColor(argb: sesion.colorTintaSugerido),
                ])
                cg.restoreGState()
            }
        }
        
        if exporta(.medios) {
            for medio in sesion.medios {
                let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(documento: documento, elemento: medio.id)
                if !fondo.esFondo && !fondo.oculto {
                    dibujar(medio, documento: documento, en: cg)
                }
            }
        }
        
        if exporta(.bloques) {
            for bloque in navegador.capaBloques.bloques {
                DibujanteBloqueExportacion.dibujar(bloque, en: cg)
            }
        }
        
        if exporta(.tecnico) {
            for objeto in navegador.capaTecnica.objetos {
                DibujanteTecnico.dibujar(objeto, en: cg, limites: limites)
            }
        }
    }
    
    private static func dibujar(_ medio: ElementoMedio, documento: UInt64, en cg: CGContext) {
        let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(documento: documento, elemento: medio.id)
        guard !fondo.oculto else { return }
        
        cg.saveGState()
        defer { cg.restoreGState() }
        
        cg.translateBy(x: medio.marco.midX, y: medio.marco.midY)
        cg.rotate(by: medio.rotacion)
        
        let marco = CGRect(x: -medio.marco.width / 2, y: -medio.marco.height / 2, width: medio.marco.width, height: medio.marco.height)
        cg.setAlpha(medio.opacidad * (fondo.esFondo ? fondo.opacidad : 1))
        
        if let ruta = RenderizadorClipPDFDocumentoIPad.ruta(
            documento: documento, elementoId: medio.id, en: marco) {
            cg.addPath(ruta.cgPath)
            cg.clip()
        }
        
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash) else {
            marcadorArchivo(medio.nombre, marco, cg)
            return
        }
        
        if medio.tipo == .imagen, let original = UIImage(contentsOfFile: url.path) {
            let recorte = Medios.recortar(original, con: medio.recorte)
            let ajustes = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: documento, id: medio.id)
            let procesada = ProcesadorImagenLocal.procesar(recorte, ajustes: ajustes)
            ProcesadorFondoObjetoIPad.procesar(procesada, ajustes: fondo).draw(in: marco)
            return
        }
        
        if medio.tipo == .pdf,
           let completa = CachePDFExportacionIPad.compartido.imagen(
            url: url,
            pagina: medio.paginaFuente,
            tamano: CGSize(
                width: max(marco.width, 1) * 2,
                height: max(marco.height, 1) * 2)) {
            let visible = medio.recorte.esCompleto
                ? completa : Medios.recortar(completa, con: medio.recorte)
            ProcesadorFondoObjetoIPad.procesar(visible, ajustes: fondo).draw(in: marco)
            return
        }
        
        marcadorArchivo(medio.nombre, marco, cg)
    }
    
    private static func marcadorArchivo(_ nombre: String, _ marco: CGRect, _ cg: CGContext) {
        cg.setFillColor(UIColor.secondarySystemBackground.cgColor)
        cg.fill(marco)
        (nombre as NSString).draw(in: marco.insetBy(dx: 12, dy: 12), withAttributes: [.font:UIFont.systemFont(ofSize: 14), .foregroundColor: UIColor.label])
    }

    private static func svg(sesion:Sesion,region:CGRect)->String{var s="<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"\(region.minX) \(region.minY) \(region.width) \(region.height)\">\n<rect x=\"\(region.minX)\" y=\"\(region.minY)\" width=\"\(region.width)\" height=\"\(region.height)\" fill=\"#\(hex(sesion.colorPapel & 0xFFFFFF))\"/>\n";for i in 0..<sesion.cantidadTrazos{guard let t=sesion.verticesDelTrazo(i)else{continue};let color="#\(hex(t.color & 0xFFFFFF))",opacidad=Double((t.color>>24)&0xFF)/255;var v=0;while v+2<t.cantidad{let puntos=(v...v+2).map{"\(t.datos[$0*2]),\(t.datos[$0*2+1])"}.joined(separator:" ");s += "<polygon points=\"\(puntos)\" fill=\"\(color)\" fill-opacity=\"\(opacidad)\"/>\n";v += 3}};return s+"</svg>"}
    private static func hex(_ v:UInt32)->String{String(format:"%06X",v)}

    private static func paquete(sesion:Sesion,navegador:VistaLienzoNavegable,titulo:String)throws->Data{guard let tinta=sesion.guardar()else{throw ErrorExportacionCompleta.noSerializa};var recursos:[String:Data]=[:];for medio in sesion.medios{guard recursos[medio.hash]==nil,let url=RepositorioAdjuntos.compartido.url(hash:medio.hash),let data=try? Data(contentsOf:url)else{continue};recursos[medio.hash]=data};let documento=sesion.documentoPersistenteId,fondos=RepositorioFondosObjetoIPad.compartido.ajustes(documento:documento),p=PaqueteTintaAutocontenido(documentoId:documento,titulo:titulo,tinta:tinta,bloques:navegador.capaBloques.bloques,tecnicos:navegador.capaTecnica.objetos,regiones:RepositorioRegionesFondo.compartido.regiones(documento),capas:RepositorioCapas.compartido.estado(documento),fondos:fondos,recursos:recursos,manifiesto:["formato":"Tinta autocontenido","sin_nube":"true","recursos":"\(recursos.count)","fondos":"\(fondos.count)"]);let e=JSONEncoder();e.dateEncodingStrategy = .iso8601;e.outputFormatting=[.sortedKeys];return try e.encode(p)}
    private static func markdown(sesion:Sesion,navegador:VistaLienzoNavegable,titulo:String)->String{var lineas=["# \(titulo)","","> Exportado localmente desde Tinta. Los enlaces conservan sus IDs de origen.",""];for t in sesion.textos{lineas += [t.texto,"","<!-- tinta-elemento: \(t.id) -->",""]};for b in navegador.capaBloques.bloques{lineas.append("## \(b.titulo)");if b.tipo == .codigo{lineas += ["```\(b.lenguaje.lowercased())",b.contenido,"```",""]}else if let tabla=b.tabla{lineas += ["```tsv",tabla.tsv,"```",""]}else{lineas += [b.contenido,""]}};return lineas.joined(separator:"\n")}
}

enum DibujanteBloqueExportacion {
    static func dibujar(_ b:BloqueLienzo,en cg:CGContext){cg.saveGState();defer{cg.restoreGState()};cg.translateBy(x:b.marco.midX,y:b.marco.midY);cg.rotate(by:b.rotacion);let r=CGRect(x:-b.marco.width/2,y:-b.marco.height/2,width:b.marco.width,height:b.marco.height);cg.setFillColor(UIColor.secondarySystemBackground.cgColor);cg.fill(r);cg.setStrokeColor(UIColor.systemIndigo.cgColor);cg.stroke(r);("\(b.tipo.rawValue) · \(b.titulo)" as NSString).draw(at:CGPoint(x:r.minX+8,y:r.minY+7),withAttributes:[.font:UIFont.boldSystemFont(ofSize:12),.foregroundColor:UIColor.label]);let contenido=r.insetBy(dx:10,dy:30);if let tabla=b.tabla{let w=contenido.width/CGFloat(tabla.columnas),h=contenido.height/CGFloat(tabla.filas);for f in 0...tabla.filas{cg.move(to:CGPoint(x:contenido.minX,y:contenido.minY+CGFloat(f)*h));cg.addLine(to:CGPoint(x:contenido.maxX,y:contenido.minY+CGFloat(f)*h))};for c in 0...tabla.columnas{cg.move(to:CGPoint(x:contenido.minX+CGFloat(c)*w,y:contenido.minY));cg.addLine(to:CGPoint(x:contenido.minX+CGFloat(c)*w,y:contenido.maxY))};cg.strokePath();for celda in tabla.celdas{(celda.texto as NSString).draw(at:CGPoint(x:contenido.minX+CGFloat(celda.columna)*w+4,y:contenido.minY+CGFloat(celda.fila)*h+4),withAttributes:[.font:UIFont.systemFont(ofSize:10),.foregroundColor:UIColor.label])}}else{(b.contenido as NSString).draw(in:contenido,withAttributes:[.font:b.tipo == .codigo ? UIFont.monospacedSystemFont(ofSize:11,weight:.regular):UIFont.systemFont(ofSize:12),.foregroundColor:UIColor.label])}}
}

final class PanelExportacionCompleta: UITableViewController {
    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private weak var presentador: UIViewController?
    private let coordinadorImpresion = CoordinadorImpresionDocumentoIPad()

    init(sesion: Sesion, navegador: VistaLienzoNavegable, presentador: UIViewController) {
        self.sesion = sesion
        self.navegador = navegador
        self.presentador = presentador
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Exportar e imprimir"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Respaldo local", style: .plain,
            target: self, action: #selector(respaldo))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Impresión" : "Exportación local"
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 1 : FormatoExportacionIPad.allCases.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        c.accessoryType = .disclosureIndicator
        if indexPath.section == 0 {
            c.textLabel?.text = "Imprimir documento"
            c.detailTextLabel?.text = "AirPrint · PDF vectorial · rango de páginas"
            c.imageView?.image = UIImage(systemName: "printer")
            return c
        }

        let f = FormatoExportacionIPad.allCases[indexPath.row]
        c.textLabel?.text = f.rawValue
        c.detailTextLabel?.text = f == .paquete
            ? "Incluye tinta, recursos, capas, bloques y referencias"
            : "Se genera localmente"
        return c
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard let n = navegador else { return }
        if indexPath.section == 0 {
            imprimir(n)
            return
        }
        do {
            let url = try ExportadorCompletoIPad.exportar(
                FormatoExportacionIPad.allCases[indexPath.row],
                sesion: sesion, navegador: n)
            let compartir = UIActivityViewController(
                activityItems: [url], applicationActivities: nil)
            compartir.popoverPresentationController?.sourceView = view
            present(compartir, animated: true)
        } catch {
            avisar(error.localizedDescription)
        }
    }

    private func imprimir(_ navegador: VistaLienzoNavegable) {
        guard navigationItem.prompt == nil else { return }
        navigationItem.prompt = "Preparando PDF para impresión…"
        tableView.isUserInteractionEnabled = false
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.coordinadorImpresion.imprimir(
                sesion: self.sesion,
                navegador: navegador,
                titulo: self.presentador?.title ?? "CuadernU",
                desde: self) { [weak self] resultado in
                    guard let self else { return }
                    DispatchQueue.main.async {
                        self.navigationItem.prompt = nil
                        self.tableView.isUserInteractionEnabled = true
                        if case .failure(let error) = resultado {
                            self.avisar(error.localizedDescription)
                        }
                    }
                }
        }
    }

    @objc private func respaldo() {
        guard let n = navegador else { return }
        do {
            let origen = try ExportadorCompletoIPad.exportar(
                .paquete, sesion: sesion, navegador: n, titulo: "Respaldo")
            let docs = FileManager.default.urls(
                for: .documentDirectory, in: .userDomainMask).first!
            let carpeta = docs.appendingPathComponent("Backups", isDirectory: true)
            try FileManager.default.createDirectory(
                at: carpeta, withIntermediateDirectories: true)
            var destino = carpeta.appendingPathComponent(origen.lastPathComponent)
            if FileManager.default.fileExists(atPath: destino.path) {
                destino = carpeta.appendingPathComponent(
                    "\(origen.deletingPathExtension().lastPathComponent)-\(UUID().uuidString)")
                    .appendingPathExtension(origen.pathExtension)
            }
            try FileManager.default.copyItem(at: origen, to: destino)
            avisar("Respaldo local creado en Archivos/Tinta/Backups:\n\(destino.lastPathComponent)")
        } catch {
            avisar(error.localizedDescription)
        }
    }

    private func avisar(_ texto: String) {
        let a = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(a, animated: true)
    }

    @objc private func cerrar() {
        dismiss(animated: true)
    }
}
