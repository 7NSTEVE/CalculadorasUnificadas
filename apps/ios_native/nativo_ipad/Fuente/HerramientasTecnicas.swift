import UIKit

enum TipoHerramientaTecnica: String, Codable, CaseIterable {
    case seleccionar = "Seleccionar / mover"
    case reglaInfinita = "Regla infinita"
    case reglaFisica = "Regla física calibrada"
    case escuadra45 = "Escuadra 45°"
    case escuadra3060 = "Escuadra 30°/60°"
    case transportador = "Transportador"
    case compas = "Compás"
    case circulo = "Círculo"
    case elipse = "Elipse"
    case spline = "Spline"
    case bezier = "Bézier"
    case plantillaElectronica = "Plantilla electrónica"
    case plantillaLogica = "Plantilla lógica"
    case diagramaBloques = "Diagrama de bloques"
    case flecha = "Flecha"
    case dimension = "Dimensión"
    case coordenadas = "Coordenadas"
    case ejes = "Ejes inteligentes"
    case vector = "Vector semántico"
    case cuerpoLibre = "Diagrama de cuerpo libre"
    case masa = "Masa"
    case polea = "Polea"
    case resorte = "Resorte"
    case campo = "Campo"
    case carga = "Carga"
    case rayo = "Rayo"
    case lente = "Lente"

    var simbolo: String {
        switch self {
        case .seleccionar: return "cursorarrow.motionlines"
        case .reglaInfinita, .reglaFisica: return "ruler"
        case .escuadra45, .escuadra3060: return "triangle"
        case .transportador: return "gauge.with.dots.needle.50percent"
        case .compas, .circulo: return "circle"
        case .elipse: return "oval"
        case .spline, .bezier: return "point.topleft.down.curvedto.point.bottomright.up"
        case .plantillaElectronica: return "resistor"
        case .plantillaLogica: return "switch.2"
        case .diagramaBloques: return "square.3.layers.3d"
        case .flecha, .vector: return "arrow.right"
        case .dimension: return "arrow.left.and.right"
        case .coordenadas, .ejes: return "chart.xyaxis.line"
        case .cuerpoLibre: return "figure.stand"
        case .masa: return "shippingbox"
        case .polea: return "circle.hexagongrid"
        case .resorte: return "waveform.path"
        case .campo: return "lines.measurement.horizontal"
        case .carga: return "plusminus.circle"
        case .rayo: return "light.beacon.max"
        case .lente: return "eye"
        }
    }
}

struct EstiloTecnico: Codable, Equatable {
    var colorARGB: UInt32 = 0xFF0A84FF
    var grosor: CGFloat = 2
    var opacidad: CGFloat = 1
    var rellenoARGB: UInt32? = nil
    var ajusteCuadricula: CGFloat = 5
    var calibracionPuntosPorCentimetro: CGFloat = 72 / 2.54
}

struct ObjetoTecnico: Codable, Identifiable, Equatable {
    var id = UUID()
    var tipo: TipoHerramientaTecnica
    var puntos: [CGPoint]
    var etiqueta = ""
    var unidad = ""
    var magnitud: Double?
    var estilo = EstiloTecnico()
    var bloqueado = false
    var fecha = Date()
}

final class RepositorioObjetosTecnicos {
    static let compartido = RepositorioObjetosTecnicos()
    private let url: URL
    private var porDocumento: [String: [ObjetoTecnico]] = [:]
    private init() {
        let base = FileManager.default.urls(for: .documentDirectory,
                                            in: .userDomainMask).first!
        let carpeta = base.appendingPathComponent("ObjetosTecnicos", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("objetos.json")
        let d = JSONDecoder(); d.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? d.decode([String: [ObjetoTecnico]].self, from: datos) {
            porDocumento = leido
        }
    }
    func objetos(documento: UInt64) -> [ObjetoTecnico] { porDocumento[String(documento)] ?? [] }
    func guardar(_ objetos: [ObjetoTecnico], documento: UInt64) {
        porDocumento[String(documento)] = objetos
        let e = JSONEncoder(); e.outputFormatting = [.sortedKeys]; e.dateEncodingStrategy = .iso8601
        if let datos = try? e.encode(porDocumento) { try? datos.write(to: url, options: .atomic) }
    }
}

final class CapaHerramientasTecnicas: UIView {
    let documentoId: UInt64
    var objetos: [ObjetoTecnico] { didSet { setNeedsDisplay(); persistir() } }
    var herramientaActiva: TipoHerramientaTecnica? { didSet { actualizarInteraccion() } }
    var estilo = EstiloTecnico()
    var ajustarCuadricula = true
    var alCambiar: (([ObjetoTecnico]) -> Void)?
    var alSeleccionar: ((ObjetoTecnico) -> Void)?

    private var puntosActuales: [CGPoint] = []
    private var indiceSeleccionado: Int?
    private var ultimoPuntoArrastre: CGPoint?

    init(documentoId: UInt64) {
        self.documentoId = documentoId
        objetos = RepositorioObjetosTecnicos.compartido.objetos(documento: documentoId)
        super.init(frame: .zero)
        backgroundColor = .clear; isOpaque = false
        isMultipleTouchEnabled = false
        actualizarInteraccion()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        herramientaActiva == nil ? nil : super.hitTest(point, with: event)
    }

    private func actualizarInteraccion() {
        isUserInteractionEnabled = herramientaActiva != nil
        puntosActuales.removeAll(); indiceSeleccionado = nil; setNeedsDisplay()
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let punto = touches.first?.preciseLocation(in: self), herramientaActiva != nil else { return }
        let p = ajustar(punto); puntosActuales = [p]; ultimoPuntoArrastre = p
        if herramientaActiva == .seleccionar {
            indiceSeleccionado = indiceCercano(a: p)
            if let i = indiceSeleccionado { alSeleccionar?(objetos[i]) }
        }
        setNeedsDisplay()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let punto = touches.first?.preciseLocation(in: self), let herramientaActiva else { return }
        let p = ajustar(punto)
        if herramientaActiva == .seleccionar, let i = indiceSeleccionado,
           !objetos[i].bloqueado, let anterior = ultimoPuntoArrastre {
            let delta = CGPoint(x: p.x - anterior.x, y: p.y - anterior.y)
            objetos[i].puntos = objetos[i].puntos.map {
                CGPoint(x: $0.x + delta.x, y: $0.y + delta.y)
            }
            ultimoPuntoArrastre = p; return
        }
        if herramientaActiva == .spline || herramientaActiva == .bezier {
            if let ultimo = puntosActuales.last, hypot(ultimo.x - p.x, ultimo.y - p.y) > 4 {
                puntosActuales.append(p)
            }
        } else if puntosActuales.count == 1 { puntosActuales.append(p) }
        else { puntosActuales[puntosActuales.count - 1] = p }
        setNeedsDisplay()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        defer { puntosActuales.removeAll(); indiceSeleccionado = nil
            ultimoPuntoArrastre = nil; setNeedsDisplay() }
        guard let herramientaActiva else { return }
        if herramientaActiva == .seleccionar { persistir(); return }
        guard puntosActuales.count >= 2 else { return }
        let objeto = ObjetoTecnico(tipo: herramientaActiva,
                                   puntos: normalizar(puntosActuales, para: herramientaActiva),
                                   estilo: estilo)
        objetos.append(objeto)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        puntosActuales.removeAll(); indiceSeleccionado = nil; setNeedsDisplay()
    }

    func eliminarUltimo() {
        guard !objetos.isEmpty else { return }; objetos.removeLast()
    }
    func agregar(_ objeto: ObjetoTecnico) { objetos.append(objeto) }
    func borrarObjeto(en punto: CGPoint) -> Bool {
        guard let indice = objetos.indices.reversed().first(where: {
            !objetos[$0].bloqueado && distancia(objetos[$0], punto) < 48
        }) else { return false }
        objetos.remove(at: indice); return true
    }
    func eliminar(id: UUID) { objetos.removeAll { $0.id == id } }
    func bloquear(id: UUID, _ bloqueado: Bool) {
        guard let i = objetos.firstIndex(where: { $0.id == id }) else { return }
        objetos[i].bloqueado = bloqueado
    }
    func actualizarEtiqueta(id: UUID, etiqueta: String, unidad: String, magnitud: Double?) {
        guard let i = objetos.firstIndex(where: { $0.id == id }) else { return }
        objetos[i].etiqueta = etiqueta; objetos[i].unidad = unidad; objetos[i].magnitud = magnitud
    }

    override func draw(_ rect: CGRect) {
        guard let contexto = UIGraphicsGetCurrentContext() else { return }
        for objeto in objetos { DibujanteTecnico.dibujar(objeto, en: contexto, limites: bounds) }
        if let tipo = herramientaActiva, tipo != .seleccionar, puntosActuales.count >= 2 {
            let previo = ObjetoTecnico(tipo: tipo, puntos: normalizar(puntosActuales, para: tipo),
                                       estilo: estilo)
            contexto.saveGState(); contexto.setAlpha(0.55)
            DibujanteTecnico.dibujar(previo, en: contexto, limites: bounds)
            contexto.restoreGState()
        }
    }

    private func ajustar(_ p: CGPoint) -> CGPoint {
        guard ajustarCuadricula, estilo.ajusteCuadricula > 0 else { return p }
        let paso = estilo.ajusteCuadricula
        return CGPoint(x: round(p.x / paso) * paso, y: round(p.y / paso) * paso)
    }

    private func normalizar(_ puntos: [CGPoint], para tipo: TipoHerramientaTecnica) -> [CGPoint] {
        guard let a = puntos.first, let b = puntos.last else { return puntos }
        switch tipo {
        case .circulo, .compas, .transportador:
            let r = hypot(b.x - a.x, b.y - a.y)
            return [a, CGPoint(x: a.x + r, y: a.y)]
        case .reglaFisica:
            let minimo = estilo.calibracionPuntosPorCentimetro * 3
            if hypot(b.x - a.x, b.y - a.y) < minimo {
                return [a, CGPoint(x: a.x + minimo, y: a.y)]
            }
            return [a, b]
        default: return puntos
        }
    }

    private func indiceCercano(a punto: CGPoint) -> Int? {
        objetos.indices.min(by: { distancia(objetos[$0], punto) < distancia(objetos[$1], punto) })
            .flatMap { distancia(objetos[$0], punto) < 48 ? $0 : nil }
    }
    private func distancia(_ objeto: ObjetoTecnico, _ punto: CGPoint) -> CGFloat {
        objeto.puntos.map { hypot($0.x - punto.x, $0.y - punto.y) }.min() ?? .greatestFiniteMagnitude
    }
    private func persistir() {
        RepositorioObjetosTecnicos.compartido.guardar(objetos, documento: documentoId)
        alCambiar?(objetos)
    }
}

enum DibujanteTecnico {
    static func dibujar(_ objeto: ObjetoTecnico, en c: CGContext, limites: CGRect) {
        guard let a = objeto.puntos.first, let b = objeto.puntos.last else { return }
        c.saveGState(); defer { c.restoreGState() }
        c.setStrokeColor(UIColor(argb: objeto.estilo.colorARGB)
            .withAlphaComponent(objeto.estilo.opacidad).cgColor)
        c.setLineWidth(objeto.estilo.grosor); c.setLineCap(.round); c.setLineJoin(.round)
        if let relleno = objeto.estilo.rellenoARGB {
            c.setFillColor(UIColor(argb: relleno).withAlphaComponent(0.14).cgColor)
        } else { c.setFillColor(UIColor.clear.cgColor) }
        switch objeto.tipo {
        case .seleccionar: break
        case .reglaInfinita: lineaInfinita(a, b, limites, c)
        case .reglaFisica: regla(a, b, objeto.estilo.calibracionPuntosPorCentimetro, c)
        case .escuadra45: triangulo(a, b, proporcion: 1, c)
        case .escuadra3060: triangulo(a, b, proporcion: sqrt(3), c)
        case .transportador: transportador(a, b, c)
        case .compas: compas(a, b, c)
        case .circulo:
            let r = hypot(b.x-a.x, b.y-a.y); c.strokeEllipse(in: CGRect(x: a.x-r, y: a.y-r, width: 2*r, height: 2*r))
        case .elipse:
            c.strokeEllipse(in: rectangulo(a, b))
        case .spline: curva(objeto.puntos, bezier: false, c)
        case .bezier: curva(objeto.puntos, bezier: true, c)
        case .plantillaElectronica: resistor(a, b, c)
        case .plantillaLogica: compuerta(a, b, c)
        case .diagramaBloques: bloque(a, b, objeto.etiqueta.isEmpty ? "H(s)" : objeto.etiqueta, c)
        case .flecha: flecha(a, b, c)
        case .dimension: dimension(a, b, objeto, c)
        case .coordenadas: coordenadas(a, b, c)
        case .ejes: ejes(a, b, objeto, c)
        case .vector: vector(a, b, objeto, c)
        case .cuerpoLibre: cuerpoLibre(a, b, c)
        case .masa: masa(a, b, c)
        case .polea: polea(a, b, c)
        case .resorte: resorte(a, b, c)
        case .campo: campo(a, b, c)
        case .carga: carga(a, b, objeto, c)
        case .rayo: flecha(a, b, c)
        case .lente: lente(a, b, c)
        }
        if !objeto.etiqueta.isEmpty && ![TipoHerramientaTecnica.diagramaBloques,
                                         .dimension, .ejes, .vector, .carga].contains(objeto.tipo) {
            texto(objeto.etiqueta, en: CGPoint(x: (a.x+b.x)/2, y: (a.y+b.y)/2 - 22))
        }
        if objeto.bloqueado {
            texto("🔒", en: CGPoint(x: min(a.x,b.x), y: min(a.y,b.y) - 18))
        }
    }

    private static func lineaInfinita(_ a: CGPoint, _ b: CGPoint, _ limite: CGRect, _ c: CGContext) {
        let dx = b.x-a.x, dy = b.y-a.y, longitud = max(hypot(dx,dy), 1)
        let u = CGPoint(x: dx/longitud, y: dy/longitud), alcance = max(limite.width, limite.height) * 3
        c.move(to: CGPoint(x: a.x-u.x*alcance, y: a.y-u.y*alcance))
        c.addLine(to: CGPoint(x: a.x+u.x*alcance, y: a.y+u.y*alcance)); c.strokePath()
    }
    private static func regla(_ a: CGPoint, _ b: CGPoint, _ puntosCm: CGFloat, _ c: CGContext) {
        let dx=b.x-a.x, dy=b.y-a.y, l=max(hypot(dx,dy),1), ux=dx/l, uy=dy/l, nx = -uy, ny = ux
        c.move(to: a); c.addLine(to: b); c.addLine(to: CGPoint(x:b.x+nx*32,y:b.y+ny*32))
        c.addLine(to: CGPoint(x:a.x+nx*32,y:a.y+ny*32)); c.closePath(); c.drawPath(using: .fillStroke)
        var d: CGFloat = 0
        while d <= l {
            let indice = Int(round(d / max(puntosCm / 10, 1)))
            let alto: CGFloat = indice.isMultiple(of: 10) ? 16 : (indice.isMultiple(of: 5) ? 11 : 6)
            let p=CGPoint(x:a.x+ux*d,y:a.y+uy*d); c.move(to:p)
            c.addLine(to:CGPoint(x:p.x+nx*alto,y:p.y+ny*alto)); d += max(puntosCm/10,1)
        }; c.strokePath()
    }
    private static func triangulo(_ a: CGPoint, _ b: CGPoint, proporcion: CGFloat, _ c: CGContext) {
        let dx=b.x-a.x, dy=b.y-a.y
        let tercero=CGPoint(x:a.x-dy*proporcion,y:a.y+dx*proporcion)
        c.move(to:a); c.addLine(to:b); c.addLine(to:tercero); c.closePath(); c.drawPath(using:.fillStroke)
    }
    private static func transportador(_ a: CGPoint, _ b: CGPoint, _ c: CGContext) {
        let r=max(hypot(b.x-a.x,b.y-a.y),20); c.addArc(center:a,radius:r,startAngle:.pi,endAngle:0,clockwise:false)
        c.move(to:CGPoint(x:a.x-r,y:a.y)); c.addLine(to:CGPoint(x:a.x+r,y:a.y))
        for grados in stride(from: 0, through: 180, by: 10) {
            let angulo=CGFloat(grados) * .pi/180, largo:CGFloat=grados.isMultiple(of:30) ? 14:8
            c.move(to:CGPoint(x:a.x+cos(angulo)*r,y:a.y-sin(angulo)*r))
            c.addLine(to:CGPoint(x:a.x+cos(angulo)*(r-largo),y:a.y-sin(angulo)*(r-largo)))
        }; c.strokePath()
    }
    private static func compas(_ a: CGPoint, _ b: CGPoint, _ c: CGContext) {
        let r=hypot(b.x-a.x,b.y-a.y); c.strokeEllipse(in:CGRect(x:a.x-r,y:a.y-r,width:2*r,height:2*r))
        c.move(to:a); c.addLine(to:b); c.move(to:CGPoint(x:a.x-8,y:a.y-16)); c.addLine(to:a)
        c.addLine(to:CGPoint(x:a.x+8,y:a.y-16)); c.strokePath()
    }
    private static func curva(_ puntos:[CGPoint], bezier:Bool, _ c:CGContext) {
        guard puntos.count > 1 else{return}; let p=UIBezierPath(); p.move(to:puntos[0])
        if bezier, puntos.count >= 4 {
            var i=1; while i+2<puntos.count { p.addCurve(to:puntos[i+2],controlPoint1:puntos[i],controlPoint2:puntos[i+1]); i += 3 }
            if i<puntos.count { for j in i..<puntos.count { p.addLine(to:puntos[j]) } }
        } else {
            for i in 1..<puntos.count { let previo=puntos[i-1], actual=puntos[i]
                p.addQuadCurve(to:actual,controlPoint:CGPoint(x:(previo.x+actual.x)/2,y:(previo.y+actual.y)/2)) }
        }; p.lineWidth=c.convertToUserSpace(CGSize(width:2,height:0)).width; p.stroke()
    }
    private static func resistor(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        let dx=(b.x-a.x)/10,dy=(b.y-a.y)/10;c.move(to:a)
        for i in 1...9 { let n:CGFloat=i.isMultiple(of:2) ? -8:8, l=max(hypot(b.x-a.x,b.y-a.y),1)
            c.addLine(to:CGPoint(x:a.x+dx*CGFloat(i)-((b.y-a.y)/l)*n,y:a.y+dy*CGFloat(i)+((b.x-a.x)/l)*n)) }
        c.addLine(to:b);c.strokePath()
    }
    private static func compuerta(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        let r=rectangulo(a,b);c.move(to:CGPoint(x:r.minX,y:r.minY));c.addLine(to:CGPoint(x:r.midX,y:r.minY))
        c.addQuadCurve(to:CGPoint(x:r.midX,y:r.maxY),control:CGPoint(x:r.maxX,y:r.midY));c.addLine(to:CGPoint(x:r.minX,y:r.maxY));c.closePath();c.strokePath()
    }
    private static func bloque(_ a:CGPoint,_ b:CGPoint,_ etiqueta:String,_ c:CGContext){
        let r=rectangulo(a,b);c.stroke(r);texto(etiqueta,en:CGPoint(x:r.midX-18,y:r.midY-9))
    }
    private static func flecha(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        c.move(to:a);c.addLine(to:b);let ang=atan2(b.y-a.y,b.x-a.x),l:CGFloat=14
        c.move(to:b);c.addLine(to:CGPoint(x:b.x-l*cos(ang - .pi/6),y:b.y-l*sin(ang - .pi/6)))
        c.move(to:b);c.addLine(to:CGPoint(x:b.x-l*cos(ang + .pi/6),y:b.y-l*sin(ang + .pi/6)));c.strokePath()
    }
    private static func dimension(_ a:CGPoint,_ b:CGPoint,_ o:ObjetoTecnico,_ c:CGContext){
        flecha(a,b,c);flecha(b,a,c);let d=hypot(b.x-a.x,b.y-a.y)/max(o.estilo.calibracionPuntosPorCentimetro,1)
        let valor=o.magnitud.map{String(format:"%.4g",$0)} ?? String(format:"%.2f",d)
        texto("\(o.etiqueta) \(valor) \(o.unidad.isEmpty ? "cm" : o.unidad)",en:CGPoint(x:(a.x+b.x)/2,y:(a.y+b.y)/2-22))
    }
    private static func coordenadas(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        c.setLineDash(phase:0,lengths:[6,5]);c.move(to:a);c.addLine(to:CGPoint(x:b.x, y:a.y));c.addLine(to:b);c.strokePath()
        texto("(\(Int(b.x-a.x)), \(Int(a.y-b.y)))",en:CGPoint(x:b.x+6,y:b.y-18))
    }
    private static func ejes(_ a:CGPoint,_ b:CGPoint,_ o:ObjetoTecnico,_ c:CGContext){
        flecha(CGPoint(x:a.x,y:b.y),CGPoint(x:b.x,y:b.y),c);flecha(CGPoint(x:a.x,y:b.y),CGPoint(x:a.x,y:a.y),c)
        let etiqueta=o.etiqueta.isEmpty ? "x / y" : o.etiqueta;texto(etiqueta,en:CGPoint(x:b.x-40,y:a.y))
        for i in 1..<10 { let x=a.x+(b.x-a.x)*CGFloat(i)/10;c.move(to:CGPoint(x:x,y:b.y-4));c.addLine(to:CGPoint(x:x,y:b.y+4)) }
        c.strokePath()
    }
    private static func vector(_ a:CGPoint,_ b:CGPoint,_ o:ObjetoTecnico,_ c:CGContext){
        flecha(a,b,c);let m=o.magnitud ?? Double(hypot(b.x-a.x,b.y-a.y));texto("\(o.etiqueta.isEmpty ? "v" : o.etiqueta) = \(String(format:"%.4g",m)) \(o.unidad)",en:CGPoint(x:(a.x+b.x)/2,y:(a.y+b.y)/2-20))
    }
    private static func cuerpoLibre(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        let centro=CGPoint(x:(a.x+b.x)/2,y:(a.y+b.y)/2),r=max(min(abs(b.x-a.x),abs(b.y-a.y))/5,12);c.strokeEllipse(in:CGRect(x:centro.x-r,y:centro.y-r,width:2*r,height:2*r))
        flecha(centro,CGPoint(x:centro.x,y:a.y),c);flecha(centro,CGPoint(x:centro.x,y:b.y),c);flecha(centro,CGPoint(x:a.x,y:centro.y),c);flecha(centro,CGPoint(x:b.x,y:centro.y),c)
    }
    private static func masa(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){c.stroke(rectangulo(a,b));texto("m",en:CGPoint(x:(a.x+b.x)/2-5,y:(a.y+b.y)/2-8))}
    private static func polea(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){let r=hypot(b.x-a.x,b.y-a.y);c.strokeEllipse(in:CGRect(x:a.x-r,y:a.y-r,width:2*r,height:2*r));c.move(to:a);c.addLine(to:CGPoint(x:a.x,y:a.y+r));c.strokePath()}
    private static func resorte(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){
        let dx = (b.x-a.x)/12, dy = (b.y-a.y)/12
        let l = max(hypot(b.x-a.x,b.y-a.y), 1)
        let nx = -(b.y-a.y)/l, ny = (b.x-a.x)/l
        c.move(to:a)
        for i in 1..<12 {
            let n: CGFloat = i.isMultiple(of:2) ? 9 : -9
            c.addLine(to:CGPoint(x:a.x+dx*CGFloat(i)+nx*n, y:a.y+dy*CGFloat(i)+ny*n))
        }
        c.addLine(to:b)
        c.strokePath()
    }
    private static func campo(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){for i in 0..<5{let t=CGFloat(i)/4,ini=CGPoint(x:a.x,y:a.y+(b.y-a.y)*t),fin=CGPoint(x:b.x,y:a.y+(b.y-a.y)*t);flecha(ini,fin,c)}}
    private static func carga(_ a:CGPoint,_ b:CGPoint,_ o:ObjetoTecnico,_ c:CGContext){let r=max(hypot(b.x-a.x,b.y-a.y),14);c.strokeEllipse(in:CGRect(x:a.x-r,y:a.y-r,width:2*r,height:2*r));texto(o.etiqueta.isEmpty ? "+q":o.etiqueta,en:CGPoint(x:a.x-10,y:a.y-9))}
    private static func lente(_ a:CGPoint,_ b:CGPoint,_ c:CGContext){let r=rectangulo(a,b),p=UIBezierPath();p.move(to:CGPoint(x:r.midX,y:r.minY));p.addCurve(to:CGPoint(x:r.midX,y:r.maxY),controlPoint1:CGPoint(x:r.maxX,y:r.midY),controlPoint2:CGPoint(x:r.maxX,y:r.midY));p.addCurve(to:CGPoint(x:r.midX,y:r.minY),controlPoint1:CGPoint(x:r.minX,y:r.midY),controlPoint2:CGPoint(x:r.minX,y:r.midY));p.stroke()}
    private static func rectangulo(_ a:CGPoint,_ b:CGPoint)->CGRect{CGRect(x:min(a.x,b.x),y:min(a.y,b.y),width:max(abs(b.x-a.x),1),height:max(abs(b.y-a.y),1))}
    private static func texto(_ valor:String,en p:CGPoint){(valor as NSString).draw(at:p,withAttributes:[.font:UIFont.monospacedSystemFont(ofSize:12,weight:.medium),.foregroundColor:UIColor.label])}
}

final class PanelHerramientasTecnicas: UITableViewController {
    private weak var capa: CapaHerramientasTecnicas?
    init(capa: CapaHerramientasTecnicas) { self.capa = capa; super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad(){super.viewDidLoad();title="Herramientas técnicas";navigationItem.leftBarButtonItem=UIBarButtonItem(barButtonSystemItem:.close,target:self,action:#selector(cerrar));navigationItem.rightBarButtonItem=UIBarButtonItem(title:"Terminar",style:.done,target:self,action:#selector(terminar))}
    override func numberOfSections(in tableView:UITableView)->Int{3}
    override func tableView(_ tableView:UITableView,titleForHeaderInSection section:Int)->String?{section==0 ? "Herramienta editable":(section==1 ? "Ajustes":"Acciones")}
    override func tableView(_ tableView:UITableView,numberOfRowsInSection section:Int)->Int{section==0 ? TipoHerramientaTecnica.allCases.count:(section==1 ? 4:2)}
    override func tableView(_ tableView:UITableView,cellForRowAt indexPath:IndexPath)->UITableViewCell{
        let c=UITableViewCell(style:.value1,reuseIdentifier:nil)
        if indexPath.section==0 {let t=TipoHerramientaTecnica.allCases[indexPath.row];c.textLabel?.text=t.rawValue;c.imageView?.image=UIImage(systemName:t.simbolo);c.accessoryType=capa?.herramientaActiva==t ? .checkmark:.none}
        else if indexPath.section==1 {let titulos=["Ajustar a cuadrícula","Paso de cuadrícula","Puntos por centímetro impreso","Estilo/color"];c.textLabel?.text=titulos[indexPath.row];if indexPath.row==0{c.accessoryType=capa?.ajustarCuadricula==true ? .checkmark:.none}else if indexPath.row==1{c.detailTextLabel?.text="\(Int(capa?.estilo.ajusteCuadricula ?? 5)) pt"}else if indexPath.row==2{c.detailTextLabel?.text=String(format:"%.3f",capa?.estilo.calibracionPuntosPorCentimetro ?? 0)}}
        else {c.textLabel?.text=indexPath.row==0 ? "Eliminar último objeto":"Desactivar herramienta";c.textLabel?.textColor=indexPath.row==0 ? .systemRed:.label}
        return c
    }
    override func tableView(_ tableView:UITableView,didSelectRowAt indexPath:IndexPath){
        if indexPath.section==0 {capa?.herramientaActiva=TipoHerramientaTecnica.allCases[indexPath.row];tableView.reloadSections(IndexSet(integer:0),with:.automatic);dismiss(animated:true)}
        else if indexPath.section==1 {ajuste(indexPath.row)}
        else if indexPath.row==0 {capa?.eliminarUltimo();tableView.reloadData()} else {capa?.herramientaActiva=nil;tableView.reloadData()}
    }
    private func ajuste(_ indice:Int){
        guard let capa else{return}
        if indice==0{capa.ajustarCuadricula.toggle();tableView.reloadData();return}
        if indice==3{let p=UIColorPickerViewController();p.selectedColor=UIColor(argb:capa.estilo.colorARGB);p.delegate=self;present(p,animated:true);return}
        let a=UIAlertController(title:indice==1 ? "Paso de cuadrícula":"Calibración de impresión",message:indice==2 ? "Puntos del documento por centímetro. 72/2,54 mantiene escala física en PDF.":nil,preferredStyle:.alert)
        a.addTextField{$0.keyboardType = .decimalPad; $0.text = String(format: "%.4g", indice == 1 ? capa.estilo.ajusteCuadricula : capa.estilo.calibracionPuntosPorCentimetro)}
        a.addAction(UIAlertAction(title:"Aplicar",style:.default){[weak self,weak capa]_ in guard let texto=a.textFields?.first?.text?.replacingOccurrences(of:",",with:"."),let v=Double(texto),v>0 else{return};if indice==1{capa?.estilo.ajusteCuadricula=CGFloat(v)}else{capa?.estilo.calibracionPuntosPorCentimetro=CGFloat(v)};self?.tableView.reloadData()});a.addAction(UIAlertAction(title:"Cancelar",style:.cancel));present(a,animated:true)
    }
    @objc private func terminar(){capa?.herramientaActiva=nil;dismiss(animated:true)}
    @objc private func cerrar(){dismiss(animated:true)}
}

extension PanelHerramientasTecnicas: UIColorPickerViewControllerDelegate {
    func colorPickerViewControllerDidFinish(_ viewController:UIColorPickerViewController){guard let capa else{return};capa.estilo.colorARGB=viewController.selectedColor.argb}
    func colorPickerViewControllerDidSelectColor(_ viewController:UIColorPickerViewController){guard let capa else{return};capa.estilo.colorARGB=viewController.selectedColor.argb}
}
