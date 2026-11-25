import Darwin
import ObjectiveC
import QuartzCore
import UIKit

enum PlanificadorViewportTintaIPad {
    static let prefetchFraccion: CGFloat = 0.22
    static let margenHisteresisFraccion: CGFloat = 0.08

    static func regionRender(
        visible: CGRect,
        documento: CGRect,
        prefetch: CGFloat = prefetchFraccion
    ) -> CGRect {
        guard visible.width.isFinite, visible.height.isFinite,
              documento.width.isFinite, documento.height.isFinite,
              visible.width > 0, visible.height > 0,
              documento.width > 0, documento.height > 0 else { return .null }
        let base = visible.intersection(documento)
        guard !base.isNull, !base.isEmpty else { return .null }
        let f = min(max(prefetch, 0), 0.5)
        return base.insetBy(
            dx: -base.width * f,
            dy: -base.height * f).intersection(documento)
    }

    static func necesitaMoverTile(
        actual: CGRect,
        visible: CGRect,
        documento: CGRect
    ) -> Bool {
        guard !actual.isNull, !actual.isEmpty else { return true }
        let objetivo = visible.intersection(documento)
        guard !objetivo.isNull, !objetivo.isEmpty else { return false }
        let mx = min(
            max(objetivo.width * margenHisteresisFraccion, 24),
            actual.width * 0.18)
        let my = min(
            max(objetivo.height * margenHisteresisFraccion, 24),
            actual.height * 0.18)
        let zonaSegura = actual.insetBy(dx: mx, dy: my)
        return !zonaSegura.contains(objetivo)
    }

    static func escalaRender(
        escalaPantalla: CGFloat,
        zoom: CGFloat
    ) -> CGFloat {
        min(max(
            escalaPantalla * max(zoom, 0.01),
            RenderizadorMetal.escalaMinimaRegional), 4)
    }
}

final class VistaTintaViewportMetalIPad: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }

    private var renderizador: RenderizadorMetal?
    private(set) var regionDocumento: CGRect = .null
    private(set) var escalaRender: CGFloat = 1

    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = false
        backgroundColor = .clear
        isUserInteractionEnabled = false
        accessibilityElementsHidden = true
        renderizador = RenderizadorMetal(capa: layer as! CAMetalLayer)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    func configurar(
        region: CGRect,
        escala: CGFloat,
        mostrarTinta: Bool,
        fondos: [RegionFondoRender]
    ) {
        regionDocumento = region
        escalaRender = escala
        frame = region
        renderizador?.mostrarTinta = mostrarTinta
        renderizador?.regionesFondo = fondos
    }

    func actualizarEstilo(
        mostrarTinta: Bool,
        fondos: [RegionFondoRender]
    ) {
        renderizador?.mostrarTinta = mostrarTinta
        renderizador?.regionesFondo = fondos
    }

    func rehornear(sesion: Sesion, hasta tiempo: Double? = nil) {
        guard !regionDocumento.isNull, !regionDocumento.isEmpty else { return }
        renderizador?.hornearTintaSeca(
            sesion: sesion,
            regionDocumento: regionDocumento,
            escala: escalaRender,
            hasta: tiempo)
        _ = renderizador?.dibujar(
            sesion: sesion,
            regionDocumento: regionDocumento,
            escala: escalaRender)
    }

    func dibujarTrazoEnCurso(
        sesion: Sesion,
        alCompletarGPU: ((CFTimeInterval) -> Void)? = nil
    ) {
        guard !regionDocumento.isNull, !regionDocumento.isEmpty else { return }
        _ = renderizador?.dibujar(
            sesion: sesion,
            regionDocumento: regionDocumento,
            escala: escalaRender,
            alCompletarGPU: alCompletarGPU)
    }

    func liberarRecursos() {
        renderizador?.liberarRecursosVolatiles()
        if let metal = layer as? CAMetalLayer {
            metal.drawableSize = CGSize(width: 1, height: 1)
            metal.contentsScale = 1
        }
        regionDocumento = .null
        isHidden = true
    }
}

private struct VentanaLatenciaViewportIPad {
    static let capacidad = 240
    private(set) var valores: [Double] = []

    mutating func registrar(_ milisegundos: Double) {
        guard milisegundos.isFinite,
              milisegundos >= 0,
              milisegundos <= 250 else { return }
        valores.append(milisegundos)
        if valores.count > Self.capacidad {
            valores.removeFirst(valores.count - Self.capacidad)
        }
    }

    var completa: Bool { valores.count >= Self.capacidad }

    func percentil(_ p: Double) -> Double {
        let ordenados = valores.sorted()
        guard !ordenados.isEmpty else { return 0 }
        let limitado = min(max(p, 0), 1)
        let posicion = limitado * Double(ordenados.count - 1)
        let inferior = Int(floor(posicion))
        let superior = min(inferior + 1, ordenados.count - 1)
        let fraccion = posicion - Double(inferior)
        return ordenados[inferior]
            + (ordenados[superior] - ordenados[inferior]) * fraccion
    }
}

/// Observa únicamente timestamps de Pencil. No cancela los touches del lienzo,
/// no modifica geometría y permite reconocimiento simultáneo con Circle/Scribble.
private final class RastreadorTimestampPencilViewportIPad: UIGestureRecognizer {
    var alMuestra: ((TimeInterval) -> Void)?

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        emitir(touches, evento: event)
        state = .began
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent) {
        emitir(touches, evento: event)
        state = .changed
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
        emitir(touches, evento: event)
        state = .ended
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .cancelled
    }

    private func emitir(_ touches: Set<UITouch>, evento: UIEvent) {
        guard let toque = touches.first else { return }
        let muestras = evento.coalescedTouches(for: toque) ?? [toque]
        if let ultima = muestras.max(by: { $0.timestamp < $1.timestamp }) {
            alMuestra?(ultima.timestamp)
        }
    }
}

private final class GestorRenderTintaVirtualizadoIPad: NSObject,
                                                       UIGestureRecognizerDelegate {
    private weak var navegador: VistaLienzoNavegable?
    private let viewport = VistaTintaViewportMetalIPad()
    private let rastreadorTimestamp = RastreadorTimestampPencilViewportIPad()
    private var observaciones: [NSKeyValueObservation] = []
    private var enlaceFrames: CADisplayLink?
    private var trabajoRehorneado: DispatchWorkItem?
    private var regionActual: CGRect = .null
    private var visibleEnJerarquia = false
    private var virtualizando = false

    private var callbackActividadAnterior: ((Bool) -> Void)?
    private var callbackEstadisticasAnterior: (() -> Void)?
    private var callbackTerminarAnterior: ((CGPoint) -> Void)?

    private var ultimoTimestampEvento: TimeInterval?
    private var ultimoTimestampMedido: TimeInterval?
    private var ventanaLatencia = VentanaLatenciaViewportIPad()
    private var muestrasDesdePublicacion = 0
    private let dispositivo: String
    private let sistema: String
    private let refrescoHz: Int

    init(navegador: VistaLienzoNavegable) {
        self.navegador = navegador
        self.dispositivo = Self.identificadorDispositivo()
        self.sistema = "iPadOS \(UIDevice.current.systemVersion)"
        self.refrescoHz = navegador.window?.screen.maximumFramesPerSecond
            ?? UIScreen.main.maximumFramesPerSecond
        super.init()
        viewport.isHidden = true

        if let contenido = navegador.lienzo.superview {
            contenido.insertSubview(viewport, belowSubview: navegador.lienzo)
        }

        rastreadorTimestamp.cancelsTouchesInView = false
        rastreadorTimestamp.delaysTouchesBegan = false
        rastreadorTimestamp.delaysTouchesEnded = false
        rastreadorTimestamp.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.pencil.rawValue)
        ]
        rastreadorTimestamp.delegate = self
        rastreadorTimestamp.alMuestra = { [weak self] timestamp in
            guard let self else { return }
            self.ultimoTimestampEvento = max(
                self.ultimoTimestampEvento ?? timestamp, timestamp)
        }
        navegador.lienzo.addGestureRecognizer(rastreadorTimestamp)

        callbackActividadAnterior = navegador.lienzo.alCambiarActividad
        navegador.lienzo.alCambiarActividad = { [weak self] activo in
            self?.callbackActividadAnterior?(activo)
            self?.cambiarActividad(activo)
        }

        callbackEstadisticasAnterior = navegador.lienzo.alActualizarEstadisticas
        navegador.lienzo.alActualizarEstadisticas = { [weak self] in
            self?.callbackEstadisticasAnterior?()
            self?.estadisticasActualizadas()
        }

        callbackTerminarAnterior = navegador.alTerminarTrazo
        navegador.alTerminarTrazo = { [weak self] punto in
            self?.callbackTerminarAnterior?(punto)
            self?.reevaluarModoYViewport(forzar: false)
            self?.programarRehorneado(retardo: 0)
        }

        observaciones.append(navegador.scroll.observe(
            \.contentOffset, options: [.new]) { [weak self] _, _ in
                DispatchQueue.main.async { self?.viewportCambio() }
            })
        observaciones.append(navegador.scroll.observe(
            \.zoomScale, options: [.new]) { [weak self] _, _ in
                DispatchQueue.main.async { self?.viewportCambio(forzar: true) }
            })
        observaciones.append(navegador.scroll.observe(
            \.contentSize, options: [.new]) { [weak self] _, _ in
                DispatchQueue.main.async {
                    self?.reevaluarModoYViewport(forzar: true)
                }
            })
    }

    deinit {
        trabajoRehorneado?.cancel()
        enlaceFrames?.invalidate()
    }

    func establecerVisible(_ visible: Bool) {
        guard visibleEnJerarquia != visible else {
            if visible { reevaluarModoYViewport(forzar: false) }
            return
        }
        visibleEnJerarquia = visible
        if visible {
            reevaluarModoYViewport(forzar: true)
        } else {
            enlaceFrames?.invalidate()
            enlaceFrames = nil
            trabajoRehorneado?.cancel()
            trabajoRehorneado = nil
            viewport.liberarRecursos()
        }
    }

    private func documentoRect() -> CGRect {
        guard let sesion = navegador?.lienzo.sesion else { return .null }
        return CGRect(
            x: 0, y: 0,
            width: max(CGFloat(sesion.anchoPagina), 1),
            height: max(CGFloat(sesion.altoPagina), 1))
    }

    private func requiereVirtualizacion() -> Bool {
        guard let navegador else { return false }
        let escala = navegador.window?.screen.scale ?? UIScreen.main.scale
        return RenderizadorMetal.requiereVirtualizacion(
            tamanoDocumento: documentoRect().size,
            escala: escala)
    }

    private func reevaluarModoYViewport(forzar: Bool) {
        guard visibleEnJerarquia, let navegador else { return }
        let necesita = requiereVirtualizacion()
        if !necesita {
            if virtualizando {
                virtualizando = false
                regionActual = .null
                viewport.liberarRecursos()
                navegador.lienzo.reanudarRecursosGraficos()
            }
            return
        }

        virtualizando = true
        if viewport.superview == nil,
           let contenido = navegador.lienzo.superview {
            contenido.insertSubview(viewport, belowSubview: navegador.lienzo)
        }
        actualizarViewport(forzar: forzar)
    }

    private func viewportCambio(forzar: Bool = false) {
        guard visibleEnJerarquia, virtualizando || requiereVirtualizacion() else {
            return
        }
        if !virtualizando {
            reevaluarModoYViewport(forzar: true)
            return
        }
        actualizarViewport(forzar: forzar)
    }

    private func actualizarViewport(forzar: Bool) {
        guard let navegador else { return }
        let documento = documentoRect()
        let visible = navegador.regionVisibleDocumento
        guard !visible.isNull, !visible.isEmpty else { return }
        guard forzar || PlanificadorViewportTintaIPad.necesitaMoverTile(
            actual: regionActual,
            visible: visible,
            documento: documento) else { return }

        let region = PlanificadorViewportTintaIPad.regionRender(
            visible: visible, documento: documento)
        guard !region.isNull, !region.isEmpty else { return }
        let escalaPantalla = navegador.window?.screen.scale ?? UIScreen.main.scale
        let escala = PlanificadorViewportTintaIPad.escalaRender(
            escalaPantalla: escalaPantalla,
            zoom: navegador.scroll.zoomScale)

        regionActual = region
        viewport.isHidden = false
        viewport.configurar(
            region: region,
            escala: escala,
            mostrarTinta: navegador.lienzo.mostrarTinta,
            fondos: navegador.lienzo.regionesFondo)
        viewport.rehornear(
            sesion: navegador.lienzo.sesion,
            hasta: navegador.lienzo.tiempoReproduccion)
    }

    private func cambiarActividad(_ activo: Bool) {
        guard visibleEnJerarquia else { return }
        if activo {
            if !virtualizando { reevaluarModoYViewport(forzar: false) }
            guard virtualizando else { return }
            trabajoRehorneado?.cancel()
            ultimoTimestampEvento = nil
            ultimoTimestampMedido = nil
            ventanaLatencia = VentanaLatenciaViewportIPad()
            muestrasDesdePublicacion = 0
            iniciarFrames()
        } else {
            enlaceFrames?.invalidate()
            enlaceFrames = nil
            if virtualizando { programarRehorneado(retardo: 0.03) }
        }
    }

    private func iniciarFrames() {
        guard enlaceFrames == nil else { return }
        let enlace = CADisplayLink(
            target: self, selector: #selector(entregarFrame(_:)))
        enlace.preferredFrameRateRange = CAFrameRateRange(
            minimum: 60, maximum: 120, preferred: 120)
        enlace.add(to: .main, forMode: .common)
        enlaceFrames = enlace
    }

    @objc private func entregarFrame(_ enlace: CADisplayLink) {
        guard visibleEnJerarquia, virtualizando,
              let navegador else {
            enlace.invalidate(); enlaceFrames = nil; return
        }
        viewportCambio()
        viewport.actualizarEstilo(
            mostrarTinta: navegador.lienzo.mostrarTinta,
            fondos: navegador.lienzo.regionesFondo)

        let referencia = ultimoTimestampEvento
        if let referencia, referencia != ultimoTimestampMedido {
            ultimoTimestampMedido = referencia
            viewport.dibujarTrazoEnCurso(
                sesion: navegador.lienzo.sesion) { [weak self] finGPU in
                    DispatchQueue.main.async {
                        self?.registrarLatenciaGPU(
                            finGPU: finGPU,
                            timestampEvento: referencia)
                    }
                }
        } else {
            viewport.dibujarTrazoEnCurso(sesion: navegador.lienzo.sesion)
        }
    }

    private func registrarLatenciaGPU(
        finGPU: CFTimeInterval,
        timestampEvento: TimeInterval
    ) {
        let ms = (finGPU - timestampEvento) * 1_000
        let cantidadAntes = ventanaLatencia.valores.count
        ventanaLatencia.registrar(ms)
        guard ventanaLatencia.valores.count > cantidadAntes else { return }
        muestrasDesdePublicacion += 1
        guard ventanaLatencia.completa,
              muestrasDesdePublicacion >= VentanaLatenciaViewportIPad.capacidad / 2,
              navegador?.lienzo.sesion.documentoPersistenteId != nil
        else { return }
        muestrasDesdePublicacion = 0

        let escenario = "Pencil huge-doc · viewport Metal regional · \(refrescoHz)Hz"
        let repo = RepositorioBenchmarksCuadernUIPad.compartido
        repo.registrar(.init(
            metrica: .latenciaPencilP95Ms,
            valor: ventanaLatencia.percentil(0.95),
            escenario: escenario,
            dispositivo: dispositivo,
            sistema: sistema,
            commit: nil))
        repo.registrar(.init(
            metrica: .latenciaPencilP99Ms,
            valor: ventanaLatencia.percentil(0.99),
            escenario: escenario,
            dispositivo: dispositivo,
            sistema: sistema,
            commit: nil))
    }

    private func estadisticasActualizadas() {
        guard visibleEnJerarquia, virtualizando,
              let lienzo = navegador?.lienzo,
              !lienzo.sesion.hayTrazoEnCurso else { return }
        programarRehorneado(retardo: 0.05)
    }

    private func programarRehorneado(retardo: TimeInterval) {
        trabajoRehorneado?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in
            guard let self, self.visibleEnJerarquia, self.virtualizando,
                  let navegador = self.navegador else { return }
            self.viewport.actualizarEstilo(
                mostrarTinta: navegador.lienzo.mostrarTinta,
                fondos: navegador.lienzo.regionesFondo)
            self.viewport.rehornear(
                sesion: navegador.lienzo.sesion,
                hasta: navegador.lienzo.tiempoReproduccion)
        }
        trabajoRehorneado = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + retardo, execute: trabajo)
    }

    func gestureRecognizer(
        _ gestureRecognizer: UIGestureRecognizer,
        shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer
    ) -> Bool { true }

    private static func identificadorDispositivo() -> String {
        var sistema = utsname()
        uname(&sistema)
        let espejo = Mirror(reflecting: sistema.machine)
        let valor = espejo.children.reduce(into: "") { salida, elemento in
            guard let byte = elemento.value as? Int8, byte != 0 else { return }
            salida.append(Character(
                UnicodeScalar(UInt8(bitPattern: byte))))
        }
        return valor.isEmpty ? UIDevice.current.model : valor
    }
}

private enum AsociadosRenderTintaVirtualizadoIPad {
    static var gestor: UInt8 = 0
}

@MainActor
final class CoordinadorRenderTintaVirtualizadoIPad {
    static let compartido = CoordinadorRenderTintaVirtualizadoIPad()

    private let gestores = NSHashTable<GestorRenderTintaVirtualizadoIPad>
        .weakObjects()
    private var observadorEscena: NSObjectProtocol?
    private var temporizador: Timer?

    func instalar() {
        guard observadorEscena == nil else {
            escanear(); return
        }
        observadorEscena = NotificationCenter.default.addObserver(
            forName: UIScene.didActivateNotification,
            object: nil,
            queue: .main) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        temporizador = Timer.scheduledTimer(
            withTimeInterval: 0.75, repeats: true) { [weak self] _ in
                Task { @MainActor in self?.escanear() }
            }
        escanear()
    }

    private func escanear() {
        var visibles = Set<ObjectIdentifier>()
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            for ventana in escena.windows where !ventana.isHidden && ventana.alpha > 0.01 {
                recorrer(vista: ventana, visibles: &visibles)
            }
        }
        for gestor in gestores.allObjects {
            gestor.establecerVisible(visibles.contains(ObjectIdentifier(gestor)))
        }
    }

    private func recorrer(
        vista: UIView,
        visibles: inout Set<ObjectIdentifier>
    ) {
        guard !vista.isHidden, vista.alpha > 0.01,
              vista.window != nil else { return }
        if let navegador = vista as? VistaLienzoNavegable {
            let gestor: GestorRenderTintaVirtualizadoIPad
            if let existente = objc_getAssociatedObject(
                navegador, &AsociadosRenderTintaVirtualizadoIPad.gestor)
                as? GestorRenderTintaVirtualizadoIPad {
                gestor = existente
            } else {
                gestor = GestorRenderTintaVirtualizadoIPad(navegador: navegador)
                objc_setAssociatedObject(
                    navegador, &AsociadosRenderTintaVirtualizadoIPad.gestor,
                    gestor, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
                gestores.add(gestor)
            }
            visibles.insert(ObjectIdentifier(gestor))
        }
        for hija in vista.subviews {
            recorrer(vista: hija, visibles: &visibles)
        }
    }
}
