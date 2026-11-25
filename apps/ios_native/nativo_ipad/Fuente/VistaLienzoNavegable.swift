import UIKit

struct MarcadorLienzo: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var x: CGFloat
    var y: CGFloat
    var colorARGB: UInt32 = 0xFFFF_9500
}

/// Contenedor del papel: zoom, desplazamiento, minimapa, historial de vistas,
/// marcadores, cortes de impresión y bordes físicos redimensionables.
final class VistaLienzoNavegable: UIView, UIScrollViewDelegate {
    let lienzo: LienzoCuadernU
    let capaFondosObjetos: CapaFondosObjetoIPad
    let capaElementos: CapaElementos
    let capaBloques: CapaBloquesLienzo
    let capaTecnica: CapaHerramientasTecnicas
    let capaRecall: CapaRecallIPad
    let capaNotasAdhesivas = CapaNotasAdhesivasIPad()
    let scroll = UIScrollView()

    private let contenido = UIView()
    private let capaCortes = CAShapeLayer()
    private let capaMarcadores = CAShapeLayer()
    private let capaPreviaReflujo = CAShapeLayer()
    private let capaFrames = CAShapeLayer()
    private let capaFocoPagina = CAShapeLayer()
    private let guiaFocoPagina = VistaGuiaFocoIPad()
    private let minimapa = MinimapaLienzo()
    private let bordeIzquierdo = TiradorBorde(ejeVertical: true)
    private let bordeDerecho = TiradorBorde(ejeVertical: true)
    private let bordeInferior = TiradorBorde(ejeVertical: false)
    private var primerAjuste = true
    private var ultimoDeltaBorde: CGFloat = 0
    private var acumuladorMutacionBorde = AcumuladorMutacionDurableLienzoIPad()
    private var regionFocoPagina: CGRect?
    private var observadorFocoPagina: NSObjectProtocol?

    func irA(yOffset: CGFloat) {
        let maxOffset = max(0, scroll.contentSize.height - scroll.bounds.height)
        let safeY = min(max(0, yOffset), maxOffset)
        scroll.setContentOffset(CGPoint(x: scroll.contentOffset.x, y: safeY), animated: true)
    }
    private var historial: [EstadoVistaLienzo] = []
    private var ultimaEscritura: CGPoint?
    private var regionZoomEscritura: CGRect?
    private var avanceZoom: CGFloat = 0.68
    private var altoLineaZoom: CGFloat = 80
    private var corrigiendoDireccion = false
    private var anclaDireccion = CGPoint.zero
    private lazy var gestoTresIzquierda: UISwipeGestureRecognizer = {
        let gesto = UISwipeGestureRecognizer(target: self,
                                              action: #selector(navegarTresDedos(_:)))
        gesto.direction = .left; gesto.numberOfTouchesRequired = 3
        gesto.isEnabled = false; return gesto
    }()
    private lazy var gestoTresDerecha: UISwipeGestureRecognizer = {
        let gesto = UISwipeGestureRecognizer(target: self,
                                              action: #selector(navegarTresDedos(_:)))
        gesto.direction = .right; gesto.numberOfTouchesRequired = 3
        gesto.isEnabled = false; return gesto
    }()
    private(set) var direccionDesplazamiento: DireccionDesplazamientoIPad = .libre

    var mostrarMinimapa = false {
        didSet {
            minimapa.isHidden = !mostrarMinimapa
            setNeedsLayout()
        }
    }

    /// Los tiradores cambian físicamente el tamaño del papel. En el cuaderno
    /// normal deben permanecer ocultos para no parecer cursores de depuración
    /// ni capturar gestos por accidente. El panel "Lienzo y papel" permite
    /// mostrarlos explícitamente cuando el usuario quiere editar ese tamaño.
    var mostrarTiradoresTamano = false {
        didSet { setNeedsLayout() }
    }

    var marcadores: [MarcadorLienzo] = [] { didSet { redibujarMarcadores() } }
    var mostrarCortes = false { didSet { actualizarCortes() } }
    var altoHojaImpresion: Float = 1754 { didSet { actualizarCortes() } }
    var margenSeguroImpresion: Float = 36 { didSet { actualizarCortes() } }
    var alCambiarVista: ((EstadoVistaLienzo) -> Void)?
    var alTerminarTrazo: ((CGPoint) -> Void)?

    init(lienzo: LienzoCuadernU) {
        self.lienzo = lienzo
        self.capaFondosObjetos = CapaFondosObjetoIPad(sesion: lienzo.sesion)
        self.capaElementos = CapaElementos(sesion: lienzo.sesion)
        self.capaBloques = CapaBloquesLienzo(
            documentoId: lienzo.sesion.documentoPersistenteId)
        self.capaTecnica = CapaHerramientasTecnicas(
            documentoId: lienzo.sesion.documentoPersistenteId)
        self.capaRecall = CapaRecallIPad(
            documento: lienzo.sesion.documentoPersistenteId)
        super.init(frame: .zero)
        configurar()
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    private func configurar() {
        clipsToBounds = true
        backgroundColor = .secondarySystemBackground

        scroll.delegate = self
        scroll.minimumZoomScale = 0.1
        scroll.maximumZoomScale = 8
        scroll.bouncesZoom = true
        scroll.decelerationRate = .fast
        scroll.keyboardDismissMode = .onDrag
        scroll.delaysContentTouches = false
        scroll.panGestureRecognizer.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.direct.rawValue)]
        addSubview(scroll)
        addGestureRecognizer(gestoTresIzquierda)
        addGestureRecognizer(gestoTresDerecha)

        contenido.backgroundColor = .clear
        contenido.layer.shadowColor = UIColor.black.cgColor
        contenido.layer.shadowOpacity = 0.16
        contenido.layer.shadowRadius = 12
        contenido.layer.shadowOffset = CGSize(width: 0, height: 4)
        scroll.addSubview(contenido)

        contenido.backgroundColor = UIColor(argb: lienzo.sesion.colorPapel)
        contenido.addSubview(capaFondosObjetos)
        capaFondosObjetos.recargar()
        contenido.addSubview(lienzo)
        capaCortes.fillColor = UIColor.clear.cgColor
        capaCortes.strokeColor = UIColor.systemRed.withAlphaComponent(0.55).cgColor
        capaCortes.lineDashPattern = [10, 8]
        capaCortes.lineWidth = 1
        contenido.layer.addSublayer(capaCortes)

        capaMarcadores.fillColor = UIColor.systemOrange.cgColor
        capaMarcadores.strokeColor = UIColor.white.cgColor
        capaMarcadores.lineWidth = 2
        contenido.layer.addSublayer(capaMarcadores)
        capaPreviaReflujo.fillColor = UIColor.clear.cgColor
        capaPreviaReflujo.strokeColor = UIColor.systemPurple.cgColor
        capaPreviaReflujo.lineWidth = 2
        capaPreviaReflujo.lineDashPattern = [7, 5]
        contenido.layer.addSublayer(capaPreviaReflujo)
        capaFrames.fillColor = UIColor.clear.cgColor
        capaFrames.lineWidth = 2
        capaFrames.lineDashPattern = [12, 7]
        contenido.layer.addSublayer(capaFrames)
        contenido.addSubview(capaElementos)
        capaElementos.recargar()
        capaElementos.alCambiarFondos = { [weak self] in
            self?.capaFondosObjetos.recargar()
        }
        lienzo.activarArrastreMasivo(capaElementos: capaElementos)
        lienzo.activarArrastreDeSeleccion()
        lienzo.activarGestoCopiaRegion()
        lienzo.regionesFondo = RepositorioRegionesFondo.render(
            RepositorioRegionesFondo.compartido.regiones(
                lienzo.sesion.documentoPersistenteId))
        contenido.addSubview(capaBloques)
        // Encima de los objetos solo cuando una herramienta está activa. Su
        // hitTest devuelve nil en reposo, por lo que no bloquea texto, medios
        // ni escritura normal.
        contenido.addSubview(capaTecnica)
        // Active Recall cubre únicamente sus regiones. Fuera de ellas el
        // hit-test devuelve nil y la escritura continúa con normalidad.
        contenido.addSubview(capaRecall)
        capaNotasAdhesivas.documentoId = lienzo.sesion.documentoPersistenteId
        contenido.addSubview(capaNotasAdhesivas)

        minimapa.layer.cornerRadius = 10
        minimapa.layer.borderWidth = 1
        minimapa.layer.borderColor = UIColor.separator.cgColor
        minimapa.isHidden = true
        minimapa.addGestureRecognizer(UITapGestureRecognizer(target: self,
                                                              action: #selector(tocarMinimapa(_:))))
        addSubview(minimapa)

        for tirador in [bordeIzquierdo, bordeDerecho, bordeInferior] {
            addSubview(tirador)
        }
        bordeIzquierdo.addGestureRecognizer(UIPanGestureRecognizer(
            target: self, action: #selector(arrastrarBordeIzquierdo(_:))))
        bordeDerecho.addGestureRecognizer(UIPanGestureRecognizer(
            target: self, action: #selector(arrastrarBordeDerecho(_:))))
        bordeInferior.addGestureRecognizer(UIPanGestureRecognizer(
            target: self, action: #selector(arrastrarBordeInferior(_:))))

        capaFocoPagina.fillRule = .evenOdd
        capaFocoPagina.fillColor = UIColor.black.withAlphaComponent(0.52).cgColor
        capaFocoPagina.zPosition = 1_000
        capaFocoPagina.isHidden = true
        layer.addSublayer(capaFocoPagina)
        observadorFocoPagina = NotificationCenter.default.addObserver(
            forName: CanalFocoPaginaActualIPad.nombre,
            object: nil,
            queue: .main) { [weak self] nota in
                self?.recibirFocoPagina(nota)
            }

        guiaFocoPagina.translatesAutoresizingMaskIntoConstraints = false
        guiaFocoPagina.layer.zPosition = 1_001
        addSubview(guiaFocoPagina)
        let anchoGuia = guiaFocoPagina.widthAnchor.constraint(
            equalTo: widthAnchor, multiplier: 0.72)
        anchoGuia.priority = .defaultHigh
        NSLayoutConstraint.activate([
            guiaFocoPagina.topAnchor.constraint(equalTo: safeAreaLayoutGuide.topAnchor,
                                                 constant: 12),
            guiaFocoPagina.centerXAnchor.constraint(equalTo: centerXAnchor),
            guiaFocoPagina.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor,
                                                     constant: 16),
            guiaFocoPagina.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor,
                                                      constant: -16),
            guiaFocoPagina.widthAnchor.constraint(lessThanOrEqualToConstant: 620),
            anchoGuia,
        ])

        lienzo.alTerminarEscritura = { [weak self] punto, crecio in
            guard let self else { return }
            self.ultimaEscritura = punto
            if crecio { self.sincronizarTamanoDocumento(conservarCentro: true) }
            self.minimapa.ultimaEscritura = punto
            self.minimapa.setNeedsDisplay()
            self.avanzarZoomEscritura(siHaceFalta: punto)
            self.alTerminarTrazo?(punto)
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        scroll.frame = bounds
        sincronizarTamanoDocumento(conservarCentro: !primerAjuste)
        if primerAjuste, bounds.width > 0 {
            primerAjuste = false
            ajustarAlAncho()
        }
        centrarContenidoPequeno()
        colocarControles()
        actualizarMinimapa()
        actualizarMascaraFocoPagina()
    }

    func viewForZooming(in scrollView: UIScrollView) -> UIView? { contenido }

    func scrollViewDidZoom(_ scrollView: UIScrollView) {
        centrarContenidoPequeno()
        colocarControles()
        actualizarMinimapa()
        actualizarMascaraFocoPagina()
    }

    func scrollViewDidScroll(_ scrollView: UIScrollView) {
        corregirDireccionSiHaceFalta()
        colocarControles()
        actualizarMinimapa()
        actualizarMascaraFocoPagina()
        alCambiarVista?(estadoActual)
    }

    func scrollViewDidEndDecelerating(_ scrollView: UIScrollView) { registrarVista() }
    func scrollViewDidEndZooming(_ scrollView: UIScrollView, with view: UIView?,
                                 atScale scale: CGFloat) { registrarVista() }

    var estadoActual: EstadoVistaLienzo {
        EstadoVistaLienzo(zoom: scroll.zoomScale, desplazamiento: scroll.contentOffset)
    }

    var centroVisibleDocumento: CGPoint { puntoDocumentoEnCentro() }

    func borrarObjetoEditable(en punto: CGPoint) -> Bool {
        let capas = RepositorioCapas.compartido
            .estado(lienzo.sesion.documentoPersistenteId).capas
        func editable(_ categoria: CategoriaCapa) -> Bool {
            guard let capa = capas.first(where: { $0.categoria == categoria }) else {
                return true
            }
            return capa.visible && !capa.bloqueada
        }
        if !capaTecnica.isHidden, editable(.tecnico),
           capaTecnica.borrarObjeto(en: punto) { return true }
        if !capaBloques.isHidden, editable(.bloques),
           capaBloques.borrarObjeto(en: punto) { return true }
        return capaElementos.borrarObjeto(en: punto)
    }
    var regionVisibleDocumento: CGRect { convert(bounds, to: contenido).intersection(contenido.bounds) }

    /// Permite que el lápiz del iPad desplace el documento cuando la herramienta
    /// activa es Mano. Los dedos siguen usando el gesto nativo de UIScrollView.
    func desplazarConHerramienta(_ deltaDocumento: CGSize) {
        var destino = scroll.contentOffset
        destino.x -= deltaDocumento.width * scroll.zoomScale
        destino.y -= deltaDocumento.height * scroll.zoomScale
        let insercion = scroll.adjustedContentInset
        let minimoX = -insercion.left
        let minimoY = -insercion.top
        let maximoX = max(minimoX, scroll.contentSize.width * scroll.zoomScale
            - scroll.bounds.width + insercion.right)
        let maximoY = max(minimoY, scroll.contentSize.height * scroll.zoomScale
            - scroll.bounds.height + insercion.bottom)
        destino.x = min(max(destino.x, minimoX), maximoX)
        destino.y = min(max(destino.y, minimoY), maximoY)
        scroll.setContentOffset(destino, animated: false)
    }

    func restaurar(_ estado: EstadoVistaLienzo, animado: Bool = false) {
        scroll.setZoomScale(min(max(estado.zoom, scroll.minimumZoomScale),
                                scroll.maximumZoomScale), animated: animado)
        scroll.setContentOffset(offsetValido(estado.desplazamiento), animated: animado)
    }

    func ajustarAlAncho() {
        let ancho = max(CGFloat(lienzo.sesion.anchoPagina), 1)
        let escala = max(min((bounds.width - 32) / ancho, 1), 0.1)
        scroll.minimumZoomScale = min(escala, 1)
        scroll.setZoomScale(escala, animated: false)
        scroll.setContentOffset(CGPoint(x: -scroll.adjustedContentInset.left,
                                        y: -scroll.adjustedContentInset.top), animated: false)
    }

    func ajustarPaginaCompleta() {
        let ancho = max(CGFloat(lienzo.sesion.anchoPagina), 1)
        let alto = max(CGFloat(lienzo.sesion.altoPagina), 1)
        let escala = max(min((bounds.width - 32) / ancho,
                             (bounds.height - 32) / alto, 1), 0.1)
        scroll.setZoomScale(escala, animated: true)
    }

    func volverUltimaEscritura() {
        guard let punto = ultimaEscritura else { return }
        mostrarRegion(CGRect(x: punto.x - 180, y: punto.y - 180,
                             width: 360, height: 360), animado: true)
    }

    func volverVistaAnterior() {
        guard historial.count > 1 else { return }
        historial.removeLast()
        if let anterior = historial.last { restaurar(anterior, animado: true) }
    }

    func mostrarRegion(_ region: CGRect, animado: Bool) {
        scroll.zoom(to: region.intersection(contenido.bounds), animated: animado)
    }

    func agregarMarcador(titulo: String, en punto: CGPoint, color: UInt32 = 0xFFFF_9500) {
        marcadores.append(MarcadorLienzo(titulo: titulo, x: punto.x, y: punto.y,
                                         colorARGB: color))
    }

    func activarZoomEscritura(region: CGRect, altoLinea: CGFloat, avance: CGFloat = 0.68) {
        regionZoomEscritura = region.intersection(contenido.bounds)
        altoLineaZoom = max(altoLinea, 24); avanceZoom = min(max(avance, 0.3), 0.9)
        if let regionZoomEscritura { mostrarRegion(regionZoomEscritura, animado: true) }
    }

    func desactivarZoomEscritura() { regionZoomEscritura = nil }

    func aplicarDireccion(_ direccion: DireccionDesplazamientoIPad) {
        direccionDesplazamiento = direccion
        anclaDireccion = scroll.contentOffset
        scroll.isPagingEnabled = direccion == .paginada
        scroll.isDirectionalLockEnabled = direccion != .libre
        scroll.alwaysBounceVertical = direccion != .horizontal
        scroll.alwaysBounceHorizontal = direccion != .vertical
    }

    func configurarGestosContextuales(panConDosDedos: Bool,
                                      navegacionTresDedos: Bool) {
        scroll.panGestureRecognizer.minimumNumberOfTouches = panConDosDedos ? 2 : 1
        gestoTresIzquierda.isEnabled = navegacionTresDedos
        gestoTresDerecha.isEnabled = navegacionTresDedos
    }

    private func corregirDireccionSiHaceFalta() {
        guard !corrigiendoDireccion else { return }
        var destino = scroll.contentOffset
        switch direccionDesplazamiento {
        case .vertical: destino.x = anclaDireccion.x
        case .horizontal: destino.y = anclaDireccion.y
        case .libre, .paginada: return
        }
        guard destino != scroll.contentOffset else { return }
        corrigiendoDireccion = true
        scroll.contentOffset = destino
        corrigiendoDireccion = false
    }

    @objc private func navegarTresDedos(_ gesto: UISwipeGestureRecognizer) {
        if gesto.direction == .right { volverVistaAnterior() }
        else { volverUltimaEscritura() }
    }

    func previsualizarReflujo(_ plan: [TintaTransformacionTrazo]) {
        let ruta = UIBezierPath()
        for movimiento in plan {
            guard let marco = lienzo.sesion.marcoTrazo(Int(movimiento.indice_trazo)) else { continue }
            let a = CGPoint(x: marco.midX, y: marco.midY)
            let b = CGPoint(x: a.x + CGFloat(movimiento.delta_x),
                            y: a.y + CGFloat(movimiento.delta_y))
            ruta.move(to: a); ruta.addLine(to: b)
            ruta.append(UIBezierPath(ovalIn: CGRect(x: b.x - 3, y: b.y - 3,
                                                    width: 6, height: 6)))
        }
        capaPreviaReflujo.path = ruta.cgPath; capaPreviaReflujo.isHidden = false
    }

    func previsualizarLimpieza(_ plan: PlanLimpiezaPostClaseIPad) {
        let ruta = UIBezierPath()
        for movimiento in plan.transformacionesTinta {
            guard let marco = lienzo.sesion.marcoTrazo(
                Int(movimiento.indice_trazo)) else { continue }
            let origen = CGPoint(x: marco.midX, y: marco.midY)
            let destino = CGPoint(
                x: origen.x + CGFloat(movimiento.delta_x),
                y: origen.y + CGFloat(movimiento.delta_y))
            ruta.move(to: origen); ruta.addLine(to: destino)
        }
        for cambio in plan.marcosMedios {
            ruta.move(to: CGPoint(x: cambio.origen.midX, y: cambio.origen.midY))
            ruta.addLine(to: CGPoint(x: cambio.destino.midX,
                                     y: cambio.destino.midY))
            ruta.append(UIBezierPath(rect: cambio.destino))
        }
        for encabezado in plan.encabezados {
            ruta.move(to: CGPoint(x: encabezado.region.minX,
                                  y: encabezado.region.maxY + 3))
            ruta.addLine(to: CGPoint(x: encabezado.region.maxX,
                                     y: encabezado.region.maxY + 3))
        }
        for seccion in plan.secciones {
            ruta.append(UIBezierPath(rect: seccion.region))
        }
        capaPreviaReflujo.path = ruta.cgPath
        capaPreviaReflujo.isHidden = false
    }

    func quitarPreviaReflujo() { capaPreviaReflujo.path = nil }

    func mostrarFrames(_ frames: [FrameApunteIPad]) {
        let ruta = UIBezierPath()
        for frame in frames { ruta.append(UIBezierPath(rect: frame.region)) }
        capaFrames.strokeColor = UIColor.systemCyan.withAlphaComponent(0.72).cgColor
        capaFrames.path = ruta.cgPath
        capaFrames.isHidden = frames.isEmpty
    }

    func aplicarModo(_ modo: ModoPaginaLienzo) {
        lienzo.sesion.aplicarModoPagina(modo)
        sincronizarTamanoDocumento(conservarCentro: true)
    }

    func aplicarTamano(ancho: Float, alto: Float, unidad: UnidadLienzo,
                       reacomodo: ReacomodoLienzo) {
        guard lienzo.sesion.cambiarTamano(ancho: ancho, alto: alto, unidad: unidad,
                                          reacomodo: reacomodo) else { return }
        sincronizarTamanoDocumento(conservarCentro: true)
        lienzo.rehornearYDibujar()
    }

    func recortarAlContenido(margen: Float = 24) {
        guard lienzo.sesion.ajustarAlContenido(margen: margen) else { return }
        sincronizarTamanoDocumento(conservarCentro: true)
        lienzo.rehornearYDibujar()
        notificarMutacionDurable()
    }

    func insertarEspacio(eje: EjeLienzo, desde: Float, cantidad: Float) {
        guard lienzo.sesion.insertarEspacio(eje: eje, desde: desde,
                                            cantidad: cantidad) else { return }
        sincronizarTamanoDocumento(conservarCentro: true)
        lienzo.rehornearYDibujar()
        notificarMutacionDurable()
    }

    private func notificarMutacionDurable() {
        CanalMutacionDurableLienzoIPad.publicar(
            manejadorNucleo: UInt64(lienzo.sesion.manejadorNucleo))
    }

    private func recibirFocoPagina(_ notificacion: Notification) {
        guard let evento = CanalFocoPaginaActualIPad.evento(de: notificacion),
              evento.documento == lienzo.sesion.documentoPersistenteId else { return }
        guard let region = evento.region else {
            regionFocoPagina = nil
            guiaFocoPagina.ocultar()
            actualizarMascaraFocoPagina()
            return
        }
        let centro = CGPoint(x: region.midX, y: region.midY)
        guard regionVisibleDocumento.contains(centro) else { return }
        regionFocoPagina = region
        if let guia = guiaParaFoco(region: region, evento: evento) {
            guiaFocoPagina.mostrar(guia)
        } else {
            guiaFocoPagina.ocultar()
        }
        actualizarMascaraFocoPagina()
    }

    private func guiaParaFoco(
        region: CGRect,
        evento: EventoFocoPaginaActualIPad
    ) -> String? {
        if let guia = evento.guia?.trimmingCharacters(in: .whitespacesAndNewlines),
           !guia.isEmpty {
            return guia
        }
        let frames = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(lienzo.sesion.documentoPersistenteId).frames
        return frames.first(where: {
            $0.region == region && $0.estudio?.intentoActivo != nil
        })?.estudio?.objetivo
    }

    private func actualizarMascaraFocoPagina() {
        capaFocoPagina.frame = bounds
        guard let regionFocoPagina else {
            capaFocoPagina.path = nil
            capaFocoPagina.isHidden = true
            return
        }
        let clara = contenido.convert(regionFocoPagina, to: self)
        let exterior = UIBezierPath(rect: bounds)
        exterior.append(UIBezierPath(roundedRect: clara, cornerRadius: 16))
        exterior.usesEvenOddFillRule = true
        capaFocoPagina.path = exterior.cgPath
        capaFocoPagina.isHidden = false
    }

    func sincronizarTamanoDocumento(conservarCentro: Bool) {
        let centroPrevio = conservarCentro ? puntoDocumentoEnCentro() : nil
        let tamano = CGSize(width: max(CGFloat(lienzo.sesion.anchoPagina), 64),
                            height: max(CGFloat(lienzo.sesion.altoPagina), 64))
        contenido.backgroundColor = UIColor(argb: lienzo.sesion.colorPapel)
        guard contenido.bounds.size != tamano else {
            colocarControles()
            actualizarCortes()
            actualizarMascaraFocoPagina()
            return
        }
        contenido.frame = CGRect(origin: .zero, size: tamano)
        capaFondosObjetos.frame = contenido.bounds
        lienzo.frame = contenido.bounds
        capaElementos.frame = contenido.bounds
        capaBloques.frame = contenido.bounds
        capaTecnica.frame = contenido.bounds
        capaRecall.frame = contenido.bounds
        capaNotasAdhesivas.frame = contenido.bounds
        capaCortes.frame = contenido.bounds
        capaMarcadores.frame = contenido.bounds
        capaPreviaReflujo.frame = contenido.bounds
        capaFrames.frame = contenido.bounds
        scroll.contentSize = tamano
        lienzo.setNeedsLayout()
        actualizarCortes()
        redibujarMarcadores()
        actualizarMascaraFocoPagina()
        if let centroPrevio {
            DispatchQueue.main.async { [weak self] in self?.centrar(en: centroPrevio) }
        }
    }

    /// Refresca objetos interactivos y printouts de fondo como una sola unidad.
    /// Debe usarse después de importar, restaurar o cambiar un sidecar visual.
    func recargarElementos() {
        capaFondosObjetos.recargar()
        capaElementos.recargar()
    }

    func puntoDocumentoEnCentro() -> CGPoint {
        convert(CGPoint(x: bounds.midX, y: bounds.midY), to: contenido)
    }

    private func centrar(en punto: CGPoint) {
        let destino = contenido.convert(punto, to: scroll)
        let offset = CGPoint(x: destino.x - bounds.width / 2,
                             y: destino.y - bounds.height / 2)
        scroll.contentOffset = offsetValido(offset)
    }

    private func offsetValido(_ valor: CGPoint) -> CGPoint {
        let minX = -scroll.adjustedContentInset.left
        let minY = -scroll.adjustedContentInset.top
        let maxX = max(minX, scroll.contentSize.width * scroll.zoomScale
                       - scroll.bounds.width + scroll.adjustedContentInset.right)
        let maxY = max(minY, scroll.contentSize.height * scroll.zoomScale
                       - scroll.bounds.height + scroll.adjustedContentInset.bottom)
        return CGPoint(x: min(max(valor.x, minX), maxX),
                       y: min(max(valor.y, minY), maxY))
    }

    private func centrarContenidoPequeno() {
        let ancho = contenido.bounds.width * scroll.zoomScale
        let alto = contenido.bounds.height * scroll.zoomScale
        scroll.contentInset = UIEdgeInsets(
            top: max((scroll.bounds.height - alto) / 2, 16),
            left: max((scroll.bounds.width - ancho) / 2, 16),
            bottom: 16, right: 16)
    }

    private func registrarVista() {
        let actual = estadoActual
        if let ultima = historial.last,
           abs(ultima.zoom - actual.zoom) < 0.03,
           hypot(ultima.desplazamiento.x - actual.desplazamiento.x,
                 ultima.desplazamiento.y - actual.desplazamiento.y) < 80 { return }
        historial.append(actual)
        if historial.count > 50 { historial.removeFirst(historial.count - 50) }
    }

    private func colocarControles() {
        let superiorIzquierdo = contenido.convert(CGPoint.zero, to: self)
        let inferiorDerecho = contenido.convert(
            CGPoint(x: contenido.bounds.maxX, y: contenido.bounds.maxY), to: self)
        bordeIzquierdo.frame = CGRect(x: superiorIzquierdo.x - 11,
                                      y: max(superiorIzquierdo.y, 0),
                                      width: 22,
                                      height: max(min(inferiorDerecho.y, bounds.maxY)
                                                  - max(superiorIzquierdo.y, 0), 44))
        bordeDerecho.frame = CGRect(x: inferiorDerecho.x - 11,
                                    y: max(superiorIzquierdo.y, 0),
                                    width: 22,
                                    height: bordeIzquierdo.bounds.height)
        bordeInferior.frame = CGRect(x: max(superiorIzquierdo.x, 0),
                                     y: inferiorDerecho.y - 11,
                                     width: max(min(inferiorDerecho.x, bounds.maxX)
                                                - max(superiorIzquierdo.x, 0), 44),
                                     height: 22)
        bordeIzquierdo.isHidden = !mostrarTiradoresTamano
            || lienzo.sesion.anchoInfinito
        bordeDerecho.isHidden = !mostrarTiradoresTamano
            || lienzo.sesion.anchoInfinito
        bordeInferior.isHidden = !mostrarTiradoresTamano
            || lienzo.sesion.altoInfinito
        if mostrarMinimapa {
            minimapa.frame = CGRect(x: bounds.maxX - 154, y: bounds.maxY - 114,
                                    width: 138, height: 98)
        }
    }

    private func actualizarCortes() {
        capaCortes.isHidden = !mostrarCortes
        guard mostrarCortes else { return }
        let cortes = lienzo.sesion.cortesImpresion(
            altoHoja: altoHojaImpresion, margenSeguro: margenSeguroImpresion)
        let ruta = UIBezierPath()
        for y in cortes {
            ruta.move(to: CGPoint(x: 0, y: CGFloat(y)))
            ruta.addLine(to: CGPoint(x: contenido.bounds.width, y: CGFloat(y)))
        }
        capaCortes.path = ruta.cgPath
    }

    private func redibujarMarcadores() {
        let ruta = UIBezierPath()
        for marcador in marcadores {
            let caja = CGRect(x: marcador.x - 7, y: marcador.y - 7,
                              width: 14, height: 14)
            ruta.append(UIBezierPath(ovalIn: caja))
        }
        capaMarcadores.path = ruta.cgPath
    }

    private func actualizarMinimapa() {
        minimapa.tamanoDocumento = contenido.bounds.size
        minimapa.regionVisible = convert(bounds, to: contenido)
            .intersection(contenido.bounds)
        minimapa.marcadores = marcadores.map { CGPoint(x: $0.x, y: $0.y) }
        minimapa.setNeedsDisplay()
    }

    private func avanzarZoomEscritura(siHaceFalta punto: CGPoint) {
        guard var region = regionZoomEscritura,
              punto.x > region.minX + region.width * 0.82 else { return }
        let desplazamiento = region.width * avanceZoom
        if region.maxX + desplazamiento <= contenido.bounds.maxX {
            region.origin.x += desplazamiento
        } else {
            region.origin.x = max(0, min(punto.x - region.width * 0.15,
                                         contenido.bounds.maxX - region.width))
            region.origin.y = min(region.origin.y + altoLineaZoom,
                                  max(contenido.bounds.maxY - region.height, 0))
        }
        regionZoomEscritura = region
        mostrarRegion(region, animado: true)
    }

    @objc private func tocarMinimapa(_ gesto: UITapGestureRecognizer) {
        let punto = minimapa.puntoDocumento(desde: gesto.location(in: minimapa))
        centrar(en: punto)
    }

    @objc private func arrastrarBordeIzquierdo(_ gesto: UIPanGestureRecognizer) {
        redimensionar(gesto, borde: .izquierdo, componente: gesto.translation(in: self).x)
    }

    @objc private func arrastrarBordeDerecho(_ gesto: UIPanGestureRecognizer) {
        redimensionar(gesto, borde: .derecho, componente: gesto.translation(in: self).x)
    }

    @objc private func arrastrarBordeInferior(_ gesto: UIPanGestureRecognizer) {
        redimensionar(gesto, borde: .inferior, componente: gesto.translation(in: self).y)
    }

    private func redimensionar(_ gesto: UIPanGestureRecognizer, borde: BordeLienzo,
                               componente: CGFloat) {
        if gesto.state == .began {
            ultimoDeltaBorde = 0
            acumuladorMutacionBorde.comenzar()
        }
        let totalDocumento = componente / max(scroll.zoomScale, 0.01)
        let incremento = totalDocumento - ultimoDeltaBorde
        if incremento != 0 {
            ultimoDeltaBorde = totalDocumento
            if lienzo.sesion.moverBorde(borde, delta: Float(incremento)) {
                acumuladorMutacionBorde.registrarCambio()
                sincronizarTamanoDocumento(conservarCentro: false)
                lienzo.rehornearYDibujar()
            }
        }
        if gesto.state == .ended || gesto.state == .cancelled {
            if acumuladorMutacionBorde.terminar() {
                notificarMutacionDurable()
            }
            registrarVista()
        }
    }

    deinit {
        if let observadorFocoPagina {
            NotificationCenter.default.removeObserver(observadorFocoPagina)
        }
    }
}

struct EstadoVistaLienzo: Codable, Equatable {
    var zoom: CGFloat
    var desplazamiento: CGPoint
}

private final class TiradorBorde: UIView {
    init(ejeVertical: Bool) {
        super.init(frame: .zero)
        isUserInteractionEnabled = true
        backgroundColor = .clear
        let linea = UIView()
        linea.backgroundColor = UIColor.systemBlue.withAlphaComponent(0.7)
        linea.layer.cornerRadius = 1.5
        linea.translatesAutoresizingMaskIntoConstraints = false
        addSubview(linea)
        NSLayoutConstraint.activate(ejeVertical ? [
            linea.centerXAnchor.constraint(equalTo: centerXAnchor),
            linea.centerYAnchor.constraint(equalTo: centerYAnchor),
            linea.widthAnchor.constraint(equalToConstant: 3),
            linea.heightAnchor.constraint(equalToConstant: 44),
        ] : [
            linea.centerXAnchor.constraint(equalTo: centerXAnchor),
            linea.centerYAnchor.constraint(equalTo: centerYAnchor),
            linea.widthAnchor.constraint(equalToConstant: 44),
            linea.heightAnchor.constraint(equalToConstant: 3),
        ])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
}

private final class MinimapaLienzo: UIView {
    var tamanoDocumento: CGSize = .zero
    var regionVisible: CGRect = .zero
    var ultimaEscritura: CGPoint?
    var marcadores: [CGPoint] = []

    override func draw(_ rect: CGRect) {
        guard let contexto = UIGraphicsGetCurrentContext(),
              tamanoDocumento.width > 0, tamanoDocumento.height > 0 else { return }
        UIColor.secondarySystemBackground.withAlphaComponent(0.93).setFill()
        contexto.fill(bounds)
        let escala = min((bounds.width - 12) / tamanoDocumento.width,
                         (bounds.height - 12) / tamanoDocumento.height)
        let origen = CGPoint(x: (bounds.width - tamanoDocumento.width * escala) / 2,
                             y: (bounds.height - tamanoDocumento.height * escala) / 2)
        let papel = CGRect(x: origen.x, y: origen.y,
                           width: tamanoDocumento.width * escala,
                           height: tamanoDocumento.height * escala)
        UIColor.systemBackground.setFill()
        contexto.fill(papel)
        UIColor.separator.setStroke()
        contexto.stroke(papel)
        let visible = CGRect(x: origen.x + regionVisible.minX * escala,
                             y: origen.y + regionVisible.minY * escala,
                             width: regionVisible.width * escala,
                             height: regionVisible.height * escala)
        UIColor.systemBlue.withAlphaComponent(0.2).setFill()
        contexto.fill(visible)
        UIColor.systemBlue.setStroke()
        contexto.stroke(visible, width: 1.5)
        UIColor.systemOrange.setFill()
        for punto in marcadores {
            contexto.fillEllipse(in: CGRect(x: origen.x + punto.x * escala - 2,
                                            y: origen.y + punto.y * escala - 2,
                                            width: 4, height: 4))
        }
        if let ultimaEscritura {
            UIColor.systemGreen.setFill()
            contexto.fillEllipse(in: CGRect(
                x: origen.x + ultimaEscritura.x * escala - 3,
                y: origen.y + ultimaEscritura.y * escala - 3,
                width: 6, height: 6))
        }
    }

    func puntoDocumento(desde punto: CGPoint) -> CGPoint {
        let escala = min((bounds.width - 12) / max(tamanoDocumento.width, 1),
                         (bounds.height - 12) / max(tamanoDocumento.height, 1))
        let origen = CGPoint(x: (bounds.width - tamanoDocumento.width * escala) / 2,
                             y: (bounds.height - tamanoDocumento.height * escala) / 2)
        return CGPoint(x: min(max((punto.x - origen.x) / escala, 0), tamanoDocumento.width),
                       y: min(max((punto.y - origen.y) / escala, 0), tamanoDocumento.height))
    }
}
