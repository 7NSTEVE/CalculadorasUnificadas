import QuartzCore
import UIKit

/// Modo de predicción, para poder comparar los dos con la misma mano.
enum ModoPrediccion: Int {
    case apagada
    case sistema  // predictedTouches de UIKit
    case nucleo   // el predictor del núcleo C++, el mismo que corre en Windows
}

enum HerramientaLienzo: Int, Codable, CaseIterable {
    case pluma
    case resaltador
    case borradorObjeto
    case borradorParcial
    case lazo
    case mano

    var nombre: String {
        switch self {
        case .pluma: return "Pluma"
        case .resaltador: return "Resaltador"
        case .borradorObjeto: return "Borrar trazo"
        case .borradorParcial: return "Borrar píxel"
        case .lazo: return "Lazo"
        case .mano: return "Mano"
        }
    }

    var simbolo: String {
        switch self {
        case .pluma: return "pencil.tip"
        case .resaltador: return "highlighter"
        case .borradorObjeto: return "eraser"
        case .borradorParcial: return "eraser.line.dashed"
        case .lazo: return "lasso"
        case .mano: return "hand.draw"
        }
    }
}

/// Lienzo de tinta.
///
/// Las decisiones que fijan la latencia:
///
/// 1. **El evento nunca espera al GPU**. `touchesMoved` incorpora todas las
///    muestras y solicita un frame; un `CADisplayLink` fusiona las solicitudes a
///    la frecuencia que el panel realmente puede mostrar.
/// 2. **`coalescedTouches`**. UIKit entrega un `UITouch` por frame, pero el
///    lápiz del iPad muestrea a 240 Hz: leer solo el toque principal tira tres de
///    cada cuatro muestras y el trazo sale anguloso en los trazos rápidos.
/// 3. **`preciseLocation(in:)`** en vez de `location(in:)`. Da la posición
///    subpíxel real del Pencil; `location` viene ajustada para el dedo.
final class LienzoCuadernU: UIView {

    private struct MuestraEstimadaPendiente {
        let idNucleo: UInt64
        var propiedadesPendientes: UITouch.Properties
        let idTrazo: String
    }

    private enum PropiedadMuestraNucleo {
        static let posicion: UInt32 = 1 << 0
        static let fuerza: UInt32 = 1 << 1
        static let altitud: UInt32 = 1 << 2
        static let azimut: UInt32 = 1 << 3
    }

    // MARK: - Configuración visible

    var anchoPluma: Float = 5.0
    var colorPluma: UInt32 = 0xFF1A_1A1A
    var resaltador = false
    var radioBorrador: Float = 18
    var ajustesBorrador = AjustesBorradorAvanzadoIPad.actual {
        didSet { radioBorrador = ajustesBorrador.radio }
    }
    var herramienta: HerramientaLienzo = .pluma {
        didSet {
            guard herramienta != oldValue else { return }
            herramientaAnterior = oldValue
            resaltador = herramienta == .resaltador
            if herramienta != .lazo { limpiarSeleccion() }
            capaPuntero.isHidden = herramienta == .mano
        }
    }
    private(set) var herramientaAnterior: HerramientaLienzo = .pluma
    var mostrarTinta = true {
        didSet { renderizador?.mostrarTinta = mostrarTinta; rehornearYDibujar() }
    }
    var regionesFondo: [RegionFondoRender] = [] {
        didSet { renderizador?.regionesFondo = regionesFondo; rehornearYDibujar() }
    }
    var ajustesPluma = AjustesPlumaAvanzada() {
        didSet { sesion.configurarPluma(ajustesPluma.puente) }
    }

    /// Rechazo de palma: en cuanto se ve el lápiz del iPad, los dedos dejan de
    /// escribir y pasan a navegar. Es lo que permite apoyar la mano en el
    /// cristal, que es como se escribe de verdad.
    var rechazoPalma = true
    private var haVistoLapiz = false

    var modoPrediccion: ModoPrediccion = .sistema {
        didSet { sesion.prediccionActiva = (modoPrediccion == .nucleo) }
    }

    /// Ajustes del puntero de hover. **No es la pluma**: el grosor es el ancho
    /// de lo que queda escrito y el radio del puntero es la marca que señala en
    /// el aire dónde está la punta antes de tocar. Los dos números son
    /// independientes y el núcleo los trata como tales.
    var puntero = AjustesPuntero() {
        didSet { actualizarPuntero() }
    }

    /// Se llama cuando el lápiz empieza o deja de escribir, para que la app
    /// decida si mantiene la pantalla encendida.
    var alCambiarActividad: ((Bool) -> Void)?

    /// Qué hacer cuando el lápiz pide la paleta de colores.
    var alPedirPaleta: (() -> Void)?

    /// Borrado de cajas, imágenes y bloques. La capa contenedora decide qué
    /// objeto está arriba y protege PDF, bloqueos y capas no editables.
    var alBorrarObjetoEnPunto: ((CGPoint) -> Bool)?
    var alNotificarGestoRegion: ((String) -> Void)?

    /// Qué hacer al apretar el lápiz del iPad Pro.
    var alApretarLapiz: (() -> Void)?

    /// Informa la posición de la última escritura y si el núcleo hizo crecer
    /// un eje infinito. El contenedor usa esto para conservar el viewport.
    var alTerminarEscritura: ((CGPoint, Bool) -> Void)?

    /// Operaciones unitarias para colaboración local. El núcleo conserva el ID
    /// dentro del archivo; así una reconexión puede reenviar sin duplicar.
    var alCrearTrazoColaborativo: ((String, Data) -> Void)?
    var alEliminarTrazosColaborativos: (([String]) -> Void)?
    var alCambiarSeleccion: ((Int) -> Void)?
    var alCambiarSeleccionDetallada: ((SeleccionLienzo) -> Void)?
    var alCambiarContenidoSeleccion: (() -> Void)?
    var alDesplazarConHerramienta: ((CGSize) -> Void)?
    var alDetectarMarcadorDibujado: ((CGPoint) -> Void)?
    weak var capaElementosDestinoArrastre: CapaElementos?
    var modoEntradaIPad: ModoEntradaLienzoIPad = .escritura
    var reconocerMarcadoresDibujados = false
    var ajustesSeleccion = AjustesSeleccionLienzo() {
        didSet {
            ajustesSeleccion.radioPincel = min(max(ajustesSeleccion.radioPincel, 4), 160)
            ajustesSeleccion.pasoCuadricula = min(max(ajustesSeleccion.pasoCuadricula, 2), 256)
        }
    }

    // MARK: - Estado

    let sesion: Sesion
    let viewport: TintaViewport
    private var renderizador: RenderizadorMetal?
    /// Una pestaña oculta conserva su sesión y su viewport, pero no necesita
    /// mantener texturas privadas de tamaño completo. El hospedador de pestañas
    /// cambia este estado explícitamente al activar otra nota.
    private var recursosGraficosSuspendidos = false
    var conservaRecursosGraficosActivos: Bool {
        !recursosGraficosSuspendidos && renderizador != nil
    }
    private var planificadorFrames = PlanificadorFramesTinta()
    private var enlaceFrames: CADisplayLink?
    private var ultimoTamanoDrawable = CGSize.zero
    private var medidorLatencia = MedidorLatenciaTinta()
    private let capaPuntero = CAShapeLayer()
    private let capaLazo = CAShapeLayer()
    private let capaSeleccion = CAShapeLayer()
    private var dibujando = false
    private var puntosLazo: [CGPoint] = []
    private(set) var seleccionActual = SeleccionLienzo()
    var trazosSeleccionados: [Int] { seleccionActual.trazos }
    private var moviendoSeleccion = false
    private var origenMovimiento = CGPoint.zero
    private var ultimoPuntoHerramienta = CGPoint.zero
    private var desplazamientoSeleccion = CGSize.zero
    private var instantaneaAntesEdicion: [String: Data]?
    private var idTrazoEnCurso: String?
    private var muestrasEstimadas: [NSNumber: MuestraEstimadaPendiente] = [:]
    /// Timestamp monotónico absoluto que se está reconstruyendo. `nil` muestra
    /// la página completa y permite escribir; con valor, el lienzo queda en
    /// modo lector para no mezclar entrada nueva con la reproducción.
    private(set) var tiempoReproduccion: Double?
    
    init(viewport: TintaViewport = .principal,
         idPersistente: DocumentoPersistenteID = DocumentoPersistenteID()) {
        self.viewport = viewport
        self.sesion = Sesion(idPersistente: idPersistente)
            ?? Sesion(idPersistente: idPersistente)!
        super.init(frame: .zero)
        
        // Registrar este documento en el viewport correspondiente
        tinta_viewport_set_documento(
            viewport == .principal ? TintaViewportPrincipal : TintaViewportSecundario,
            sesion.manejadorNucleo)
        
        configurar()
    }
    
    override init(frame: CGRect) {
        self.viewport = .principal
        self.sesion = Sesion() ?? Sesion()!
        super.init(frame: frame)
        tinta_viewport_set_documento(
            self.viewport == .principal ? TintaViewportPrincipal : TintaViewportSecundario,
            sesion.manejadorNucleo)
        configurar()
    }

    required init?(coder: NSCoder) {
        self.viewport = .principal
        self.sesion = Sesion() ?? Sesion()!
        super.init(coder: coder)
        tinta_viewport_set_documento(
            self.viewport == .principal ? TintaViewportPrincipal : TintaViewportSecundario,
            sesion.manejadorNucleo)
        configurar()
    }

    private(set) var latenciaMs: Double = 0
    private(set) var latenciaP95Ms: Double = 0
    private(set) var latenciaP99Ms: Double = 0
    private(set) var latenciaMaximaMs: Double = 0
    private(set) var framesDiferidos = 0

    /// Se llama tras cada frame para que la interfaz refresque sus contadores.
    var alActualizarEstadisticas: (() -> Void)?

    override class var layerClass: AnyClass { CAMetalLayer.self }
    private var capaMetal: CAMetalLayer { layer as! CAMetalLayer }

    // MARK: - Ciclo de vida



    private func configurar() {
        accessibilityIdentifier = "editor.canvas.tinta"
        accessibilityLabel = "Lienzo de escritura"
        isMultipleTouchEnabled = false
        // El papel y los printouts están debajo de esta capa. Metal solo
        // compone pauta y tinta, lo que garantiza que la escritura quede arriba.
        isOpaque = false
        backgroundColor = .clear

        renderizador = RenderizadorMetal(capa: capaMetal)
        renderizador?.mostrarTinta = mostrarTinta
        renderizador?.regionesFondo = regionesFondo
        sesion.prediccionActiva = (modoPrediccion == .nucleo)
        sesion.configurarPluma(ajustesPluma.puente)
        radioBorrador = ajustesBorrador.radio

        capaPuntero.isHidden = true
        capaLazo.fillColor = UIColor.systemBlue.withAlphaComponent(0.08).cgColor
        capaLazo.strokeColor = UIColor.systemBlue.cgColor
        capaLazo.lineWidth = 1.5
        capaLazo.lineDashPattern = [6, 4]
        layer.addSublayer(capaLazo)

        capaSeleccion.fillColor = UIColor.systemBlue.withAlphaComponent(0.06).cgColor
        capaSeleccion.strokeColor = UIColor.systemBlue.cgColor
        capaSeleccion.lineWidth = 2
        capaSeleccion.lineDashPattern = [8, 4]
        layer.addSublayer(capaSeleccion)

        layer.addSublayer(capaPuntero)
        actualizarPuntero()

        let hover = UIHoverGestureRecognizer(target: self, action: #selector(alFlotar(_:)))
        hover.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.pencil.rawValue)]
        addGestureRecognizer(hover)

        let interaccionLapiz = UIPencilInteraction()
        interaccionLapiz.delegate = self
        addInteraction(interaccionLapiz)
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let escala = window?.screen.scale ?? UIScreen.main.scale
        contentScaleFactor = escala
        capaMetal.contentsScale = escala
        let tamanoDrawable = CGSize(
            width: ceil(bounds.width * escala),
            height: ceil(bounds.height * escala))
        capaMetal.drawableSize = tamanoDrawable
        capaLazo.frame = bounds
        capaSeleccion.frame = bounds
        guard !recursosGraficosSuspendidos else { return }
        guard tamanoDrawable != ultimoTamanoDrawable,
              tamanoDrawable.width > 0, tamanoDrawable.height > 0 else { return }
        ultimoTamanoDrawable = tamanoDrawable
        cancelarFramePendiente()
        renderizador?.invalidarTextura()
        renderizador?.hornearTintaSeca(sesion: sesion, tamano: bounds.size,
                                      escala: escala, hasta: tiempoReproduccion)
        solicitarDibujo(timestampEvento: nil)
    }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        // CADisplayLink retiene su target. Cortarlo al sacar el lienzo de la
        // jerarquía evita conservar un documento cerrado por un frame pendiente.
        if window == nil { cancelarFramePendiente() }
    }

    /// Libera el presupuesto GPU de una pestaña que ya no está visible.
    ///
    /// El renderizador puede tener command buffers en vuelo. Metal retiene las
    /// texturas y buffers ya codificados; aquí se entregan únicamente las
    /// referencias de caché usadas para construir los frames siguientes.
    func suspenderRecursosGraficos() {
        guard !recursosGraficosSuspendidos else { return }
        recursosGraficosSuspendidos = true
        cancelarFramePendiente()
        renderizador?.liberarRecursosVolatiles()
        ultimoTamanoDrawable = .zero
    }

    /// Reconstruye las texturas de la pestaña activa desde el modelo durable.
    /// No conserva un snapshot raster oculto: la sesión C++ es la fuente de
    /// verdad y se vuelve a hornear antes de presentar el primer frame.
    func reanudarRecursosGraficos() {
        guard recursosGraficosSuspendidos || renderizador == nil else { return }
        let activo: RenderizadorMetal
        if let renderizador {
            activo = renderizador
        } else {
            guard let nuevo = RenderizadorMetal(capa: capaMetal) else {
                recursosGraficosSuspendidos = true
                return
            }
            renderizador = nuevo
            activo = nuevo
        }

        recursosGraficosSuspendidos = false
        activo.mostrarTinta = mostrarTinta
        activo.regionesFondo = regionesFondo

        guard window != nil, bounds.width > 0, bounds.height > 0 else {
            setNeedsLayout()
            return
        }

        let escala = window?.screen.scale ?? UIScreen.main.scale
        let tamanoDrawable = CGSize(
            width: ceil(bounds.width * escala),
            height: ceil(bounds.height * escala))
        capaMetal.drawableSize = tamanoDrawable
        ultimoTamanoDrawable = tamanoDrawable
        activo.hornearTintaSeca(
            sesion: sesion, tamano: bounds.size, escala: escala,
            hasta: tiempoReproduccion)
        solicitarDibujo(timestampEvento: nil)
    }

    // MARK: - Entrada

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard tiempoReproduccion == nil, modoEntradaIPad != .lectura,
              let touch = touches.first, aceptar(touch) else { return }
        let punto = touch.preciseLocation(in: self)
        ultimoPuntoHerramienta = punto
        switch herramienta {
        case .pluma, .resaltador:
            limpiarSeleccion()
            dibujando = true
            alCambiarActividad?(true)
            idTrazoEnCurso = UUID().uuidString
            sesion.comenzarTrazo(ancho: anchoPluma, color: colorPluma,
                                 resaltador: herramienta == .resaltador)
            agregar(touch: touch, event: event)
            dibujar(referencia: touch)
        case .borradorObjeto, .borradorParcial:
            limpiarSeleccion()
            dibujando = true
            instantaneaAntesEdicion = instantaneaColaborativa()
            alCambiarActividad?(true)
            borrar(en: punto)
        case .lazo:
            dibujando = true
            alCambiarActividad?(true)
            if let marco = marcoSeleccionActual()?.insetBy(dx: -18, dy: -18),
               marco.contains(punto), !seleccionActual.vacia {
                moviendoSeleccion = true
                origenMovimiento = punto
                desplazamientoSeleccion = .zero
                instantaneaAntesEdicion = instantaneaColaborativa()
            } else {
                moviendoSeleccion = false
                if ajustesSeleccion.combinacion == .reemplazar {
                    seleccionActual = SeleccionLienzo()
                    capaSeleccion.path = nil
                    notificarSeleccion()
                }
                puntosLazo = [punto]
                actualizarRutaLazo()
            }
        case .mano:
            dibujando = true
            capaPuntero.isHidden = true
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard tiempoReproduccion == nil,
              let touch = touches.first, aceptar(touch), dibujando else { return }
        let punto = touch.preciseLocation(in: self)
        switch herramienta {
        case .pluma, .resaltador:
            agregar(touch: touch, event: event)
            dibujar(referencia: touch)
        case .borradorObjeto, .borradorParcial:
            borrar(en: punto)
        case .lazo where moviendoSeleccion:
            desplazamientoSeleccion = CGSize(
                width: punto.x - origenMovimiento.x,
                height: punto.y - origenMovimiento.y)
            CATransaction.begin()
            CATransaction.setDisableActions(true)
            capaSeleccion.setAffineTransform(CGAffineTransform(
                translationX: desplazamientoSeleccion.width,
                y: desplazamientoSeleccion.height))
            CATransaction.commit()
        case .lazo:
            if ajustesSeleccion.forma == .rectangulo ||
                ajustesSeleccion.forma == .elipse {
                puntosLazo = [puntosLazo.first ?? punto, punto]
            } else {
                puntosLazo.append(punto)
            }
            actualizarRutaLazo()
        case .mano:
            let delta = CGSize(width: punto.x - ultimoPuntoHerramienta.x,
                               height: punto.y - ultimoPuntoHerramienta.y)
            alDesplazarConHerramienta?(delta)
            ultimoPuntoHerramienta = punto
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first, dibujando else { return }
        let ultimaPosicion = touch.preciseLocation(in: self)
        switch herramienta {
        case .pluma, .resaltador:
            agregar(touch: touch, event: event, incluirPredichos: false)
            let id = idTrazoEnCurso ?? UUID().uuidString
            var indiceTerminado: Int?
            if let indice = sesion.terminarTrazo(id: id) {
                indiceTerminado = indice
                if let datos = sesion.serializarTrazo(indice) {
                    alCrearTrazoColaborativo?(id, datos)
                }
                if reconocerMarcadoresDibujados {
                    let puntos = sesion.puntosTrazo(indice)
                    if DetectorMarcadorDibujadoIPad.esEstrella(puntos),
                       let marco = sesion.marcoTrazo(indice) {
                        alDetectarMarcadorDibujado?(CGPoint(
                            x: marco.midX, y: marco.midY))
                    }
                }
            } else {
                // Un ID duplicado o un trazo vacío no debe dejar una operación
                // a medio abrir ni identificadores de estimación retenidos.
                sesion.descartarTrazo()
                liberarMuestrasEstimadas(delTrazo: id)
            }
            idTrazoEnCurso = nil
            sesion.limpiarProvisionales()
            terminarActividad()
            let crecio = sesion.crecerSiHaceFalta(margen: 120)
            if let indiceTerminado {
                hornearTrazoTerminadoYDibujar(indice: indiceTerminado)
            } else {
                rehornearYDibujar()
            }
            alTerminarEscritura?(ultimaPosicion, crecio)
        case .borradorObjeto, .borradorParcial:
            borrar(en: ultimaPosicion)
            publicarEdicionPendiente()
            terminarActividad()
            rehornearYDibujar()
        case .lazo where moviendoSeleccion:
            if abs(desplazamientoSeleccion.width) > 0.5 ||
                abs(desplazamientoSeleccion.height) > 0.5 {
                var delta = desplazamientoSeleccion
                if ajustesSeleccion.ajustarCuadricula {
                    let paso = ajustesSeleccion.pasoCuadricula
                    delta.width = (delta.width / paso).rounded() * paso
                    delta.height = (delta.height / paso).rounded() * paso
                }
                let editable = RepositorioMetadatosSeleccionIPad.compartido
                    .filtrarDesbloqueados(seleccionActual, sesion: sesion)
                _ = sesion.transformarSeleccion(
                    editable, desplazamiento: delta)
            }
            capaSeleccion.setAffineTransform(.identity)
            publicarEdicionPendiente()
            alCambiarContenidoSeleccion?()
            moviendoSeleccion = false
            desplazamientoSeleccion = .zero
            actualizarMarcoSeleccion()
            terminarActividad()
            rehornearYDibujar()
        case .lazo:
            if ajustesSeleccion.forma == .rectangulo ||
                ajustesSeleccion.forma == .elipse {
                puntosLazo = [puntosLazo.first ?? ultimaPosicion, ultimaPosicion]
            } else {
                puntosLazo.append(ultimaPosicion)
            }
            let nueva = calcularSeleccionTrazada()
            seleccionActual = seleccionActual.combinada(
                con: nueva, modo: ajustesSeleccion.combinacion)
            actualizarMarcoSeleccion()
            notificarSeleccion()
            if !seleccionActual.vacia {
                prepararSeleccionParaPegadoRapido()
            }
            terminarActividad()
        case .mano:
            terminarActividad()
        }
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard dibujando else { return }
        switch herramienta {
        case .pluma, .resaltador:
            sesion.descartarTrazo()
            sesion.limpiarProvisionales()
            if let idTrazoEnCurso {
                liberarMuestrasEstimadas(delTrazo: idTrazoEnCurso)
            }
            idTrazoEnCurso = nil
        case .borradorObjeto, .borradorParcial:
            publicarEdicionPendiente()
        case .lazo:
            capaSeleccion.setAffineTransform(.identity)
            puntosLazo.removeAll()
            capaLazo.path = nil
            moviendoSeleccion = false
            instantaneaAntesEdicion = nil
        case .mano:
            break
        }
        terminarActividad()
        rehornearYDibujar()
    }

    /// Rechazo de palma. En cuanto aparece el lápiz, la mano apoyada deja de
    /// pintar: sin esto no se puede escribir apoyando la muñeca, que es como se
    /// escribe de verdad.
    private func aceptar(_ touch: UITouch) -> Bool {
        if touch.type == .pencil {
            haVistoLapiz = true
            return true
        }
        // El rechazo de palma no debe inutilizar para siempre el dedo después
        // de haber usado el Pencil. En lazo y mano el dedo es una intención
        // explícita: seleccionar, mover o desplazar. La escritura conserva el
        // rechazo estricto para que la muñeca no produzca tinta accidental.
        if touch.type == .direct,
           herramienta == .lazo || herramienta == .mano {
            return true
        }
        return !(rechazoPalma && haVistoLapiz)
    }

    private func terminarActividad() {
        dibujando = false
        alCambiarActividad?(false)
        capaPuntero.isHidden = herramienta == .mano
            || !puntero.debeVerse(hayHover: !capaPuntero.isHidden,
                                  dibujando: false)
    }

    private func borrar(en punto: CGPoint) {
        if ajustesBorrador.modo == .objeto {
            guard alBorrarObjetoEnPunto?(punto) == true else { return }
            rehornearYDibujar(); return
        }
        if ajustesBorrador.filtro == .soloCapaActual {
            let tinta = RepositorioCapas.compartido
                .estado(sesion.documentoPersistenteId).capas
                .first { $0.categoria == .tinta }
            guard tinta?.visible != false, tinta?.bloqueada != true else { return }
        }
        let parcial = ajustesBorrador.modo == .pixel
            || herramienta == .borradorParcial
        guard sesion.borrar(
            x: Float(punto.x), y: Float(punto.y), radio: radioBorrador,
            parcial: parcial, filtro: ajustesBorrador.filtro,
            colorARGB: ajustesBorrador.colorARGB) else { return }
        rehornearYDibujar()
    }

    private func actualizarRutaLazo() {
        guard let primero = puntosLazo.first else { capaLazo.path = nil; return }
        if ajustesSeleccion.forma == .pincel {
            capaLazo.fillColor = UIColor.clear.cgColor
            capaLazo.lineWidth = ajustesSeleccion.radioPincel * 2
            capaLazo.strokeColor = UIColor.systemBlue.withAlphaComponent(0.28).cgColor
            capaLazo.lineDashPattern = nil
        } else {
            capaLazo.fillColor = UIColor.systemBlue.withAlphaComponent(0.08).cgColor
            capaLazo.lineWidth = 1.5
            capaLazo.strokeColor = UIColor.systemBlue.cgColor
            capaLazo.lineDashPattern = [6, 4]
        }
        if (ajustesSeleccion.forma == .rectangulo ||
            ajustesSeleccion.forma == .elipse), let ultimo = puntosLazo.last {
            let rectangulo = rectanguloNormalizado(primero, ultimo)
            capaLazo.path = (ajustesSeleccion.forma == .rectangulo
                ? UIBezierPath(rect: rectangulo)
                : UIBezierPath(ovalIn: rectangulo)).cgPath
            return
        }
        let ruta = UIBezierPath()
        ruta.move(to: primero)
        puntosLazo.dropFirst().forEach { ruta.addLine(to: $0) }
        capaLazo.path = ruta.cgPath
    }

    private func rectanguloNormalizado(_ a: CGPoint, _ b: CGPoint) -> CGRect {
        CGRect(x: min(a.x, b.x), y: min(a.y, b.y),
               width: max(abs(a.x - b.x), 1),
               height: max(abs(a.y - b.y), 1))
    }

    private func calcularSeleccionTrazada() -> SeleccionLienzo {
        let seleccion: SeleccionLienzo
        switch ajustesSeleccion.forma {
        case .lazoLibre:
            seleccion = sesion.seleccionarPoligono(puntosLazo,
                                                   ajustes: ajustesSeleccion)
        case .rectangulo:
            guard let a = puntosLazo.first, let b = puntosLazo.last else {
                return SeleccionLienzo()
            }
            seleccion = sesion.seleccionarRectangulo(rectanguloNormalizado(a, b),
                                                      ajustes: ajustesSeleccion)
        case .elipse:
            guard let a = puntosLazo.first, let b = puntosLazo.last else {
                return SeleccionLienzo()
            }
            seleccion = sesion.seleccionarElipse(rectanguloNormalizado(a, b),
                                                  ajustes: ajustesSeleccion)
        case .pincel:
            seleccion = sesion.seleccionarPincel(puntosLazo,
                                                 ajustes: ajustesSeleccion)
        }
        return RepositorioMetadatosSeleccionIPad.compartido.expandirGrupos(
            de: seleccion, sesion: sesion)
    }

    func marcoSeleccionActual() -> CGRect? {
        var marcos = seleccionActual.trazos.compactMap(sesion.marcoTrazo)
        let textos = sesion.textos
        marcos.append(contentsOf: seleccionActual.textos.compactMap {
            textos.indices.contains($0) ? textos[$0].marco : nil
        })
        let medios = sesion.medios
        marcos.append(contentsOf: seleccionActual.medios.compactMap {
            medios.indices.contains($0) ? medios[$0].marco : nil
        })
        return marcos.reduce(nil as CGRect?) { parcial, marco in
            parcial?.union(marco) ?? marco
        }
    }

    private func actualizarMarcoSeleccion() {
        capaLazo.path = nil
        puntosLazo.removeAll()
        guard let marco = marcoSeleccionActual() else {
            capaSeleccion.path = nil
            return
        }
        capaSeleccion.path = UIBezierPath(
            roundedRect: marco.insetBy(dx: -8, dy: -8), cornerRadius: 8).cgPath
    }

    func limpiarSeleccion() {
        puntosLazo.removeAll()
        seleccionActual = SeleccionLienzo()
        capaLazo.path = nil
        capaSeleccion.path = nil
        capaSeleccion.setAffineTransform(.identity)
        notificarSeleccion()
    }

    private func notificarSeleccion() {
        alCambiarSeleccion?(seleccionActual.total)
        alCambiarSeleccionDetallada?(seleccionActual)
    }

    private func instantaneaColaborativa() -> [String: Data] {
        var salida: [String: Data] = [:]
        for indice in 0..<sesion.cantidadTrazos {
            guard let id = sesion.asegurarIdTrazo(indice),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            salida[id] = datos
        }
        return salida
    }

    private func publicarEdicionPendiente() {
        guard let antes = instantaneaAntesEdicion else { return }
        instantaneaAntesEdicion = nil
        publicarDiferencia(desde: antes)
    }

    private func publicarDiferencia(desde antes: [String: Data]) {
        let despues = instantaneaColaborativa()
        let quitados = Set(antes.keys).subtracting(despues.keys)
        let modificados = Set<String>(despues.compactMap { entrada in
            guard let anterior = antes[entrada.key],
                  anterior != entrada.value else { return nil }
            return entrada.key
        })
        let tumbas = Array(quitados.union(modificados)).sorted()
        if !tumbas.isEmpty { alEliminarTrazosColaborativos?(tumbas) }
        for id in despues.keys.sorted() {
            guard antes[id] == nil || modificados.contains(id),
                  let datos = despues[id] else { continue }
            alCrearTrazoColaborativo?(id, datos)
        }
    }

    private func agregar(touch: UITouch, event: UIEvent?, incluirPredichos: Bool = true) {
        // Todas las muestras acumuladas desde el frame anterior, no solo la
        // última. Aquí es donde se recuperan los 240 Hz del lápiz del iPad.
        let reales = event?.coalescedTouches(for: touch) ?? [touch]
        for muestra in reales {
            let p = muestra.preciseLocation(in: self)
            let fuerza = presion(de: muestra)
            let altitud = Float(muestra.altitudeAngle)
            let azimut = Float(muestra.azimuthAngle(in: self))
            let gestionadas: UITouch.Properties = [
                .location, .force, .altitude, .azimuth
            ]
            let pendientes = muestra.estimatedPropertiesExpectingUpdates
                .intersection(gestionadas)
            if !pendientes.isEmpty,
               let indiceActualizacion = muestra.estimationUpdateIndex,
               let idTrazo = idTrazoEnCurso,
               let idNucleo = sesion.anadirMuestraActualizable(
                   x: Float(p.x), y: Float(p.y), presion: fuerza,
                   altitud: altitud, azimut: azimut,
                   tiempo: muestra.timestamp) {
                if let anterior = muestrasEstimadas[indiceActualizacion] {
                    sesion.finalizarActualizacionesMuestra(anterior.idNucleo)
                }
                muestrasEstimadas[indiceActualizacion] =
                    MuestraEstimadaPendiente(
                        idNucleo: idNucleo,
                        propiedadesPendientes: pendientes,
                        idTrazo: idTrazo)
            } else {
                sesion.anadirMuestra(x: Float(p.x), y: Float(p.y),
                                     presion: fuerza, altitud: altitud,
                                     azimut: azimut, tiempo: muestra.timestamp)
            }
        }

        sesion.limpiarProvisionales()
        guard incluirPredichos, modoPrediccion == .sistema else { return }

        // Las predichas se dibujan pero no se guardan: en cuanto llegan las
        // reales se descartan. Nunca entran al documento ni al archivo.
        for predicho in event?.predictedTouches(for: touch) ?? [] {
            let p = predicho.preciseLocation(in: self)
            sesion.anadirProvisional(x: Float(p.x),
                                     y: Float(p.y),
                                     presion: presion(de: predicho),
                                     altitud: Float(predicho.altitudeAngle),
                                     azimut: Float(predicho.azimuthAngle(in: self)),
                                     tiempo: predicho.timestamp)
        }
    }

    /// UIKit puede completar presión, inclinación, dirección o posición cuando
    /// la muestra original ya se dibujó e incluso después de levantar el Pencil.
    /// La identidad de UIKit localiza la muestra y la identidad del núcleo hace
    /// que la corrección sobreviva a terminar, deshacer y rehacer el trazo.
    override func touchesEstimatedPropertiesUpdated(_ touches: Set<UITouch>) {
        var corrigioTrazoActivo = false
        var trazosFinalizados = Set<String>()

        for touch in touches {
            guard let indice = touch.estimationUpdateIndex,
                  var pendiente = muestrasEstimadas[indice] else { continue }

            let aunPendientes = pendiente.propiedadesPendientes.intersection(
                touch.estimatedPropertiesExpectingUpdates)
            let resueltas = pendiente.propiedadesPendientes
                .subtracting(aunPendientes)

            var propiedades: UInt32 = 0
            if resueltas.contains(.location) {
                propiedades |= PropiedadMuestraNucleo.posicion
            }
            if resueltas.contains(.force) {
                propiedades |= PropiedadMuestraNucleo.fuerza
            }
            if resueltas.contains(.altitude) {
                propiedades |= PropiedadMuestraNucleo.altitud
            }
            if resueltas.contains(.azimuth) {
                propiedades |= PropiedadMuestraNucleo.azimut
            }

            if propiedades != 0 {
                let p = touch.preciseLocation(in: self)
                let actualizada = sesion.actualizarMuestra(
                    pendiente.idNucleo, propiedades: propiedades,
                    x: Float(p.x), y: Float(p.y), presion: presion(de: touch),
                    altitud: Float(touch.altitudeAngle),
                    azimut: Float(touch.azimuthAngle(in: self)))
                if actualizada {
                    if sesion.indiceTrazo(id: pendiente.idTrazo) != nil {
                        trazosFinalizados.insert(pendiente.idTrazo)
                    } else {
                        corrigioTrazoActivo = true
                    }
                }
            }

            pendiente.propiedadesPendientes = aunPendientes
            if aunPendientes.isEmpty {
                sesion.finalizarActualizacionesMuestra(pendiente.idNucleo)
                muestrasEstimadas.removeValue(forKey: indice)
            } else {
                muestrasEstimadas[indice] = pendiente
            }
        }

        if !trazosFinalizados.isEmpty {
            rehornearYDibujar()
            for id in trazosFinalizados.sorted() {
                publicarCorreccionEstimada(idTrazo: id)
            }
        } else if corrigioTrazoActivo {
            solicitarDibujo(timestampEvento: touches.map(\.timestamp).max())
        }
    }

    private func liberarMuestrasEstimadas(delTrazo idTrazo: String) {
        let indices = muestrasEstimadas.compactMap { entrada in
            entrada.value.idTrazo == idTrazo ? entrada.key : nil
        }
        for indice in indices {
            guard let pendiente = muestrasEstimadas.removeValue(
                forKey: indice) else { continue }
            sesion.finalizarActualizacionesMuestra(pendiente.idNucleo)
        }
    }

    private func publicarCorreccionEstimada(idTrazo: String) {
        guard let indice = sesion.indiceTrazo(id: idTrazo),
              let datos = sesion.serializarTrazo(indice) else { return }
        alEliminarTrazosColaborativos?([idTrazo])
        alCrearTrazoColaborativo?(idTrazo, datos)
    }

    /// Presión normalizada a 0..1, o negativa si el dispositivo no la reporta.
    ///
    /// El dedo en un iPad sin 3D Touch devuelve siempre `maximumPossibleForce`
    /// igual a cero: eso no es presión cero, es ausencia de sensor, y el núcleo
    /// lo distingue para modular el ancho por velocidad en su lugar.
    private func presion(de touch: UITouch) -> Float {
        guard touch.type == .pencil, touch.maximumPossibleForce > 0 else { return -1 }
        return Float(min(max(touch.force / touch.maximumPossibleForce, 0), 1))
    }

    // MARK: - Dibujo

    private func dibujar(referencia touch: UITouch) {
        guard !recursosGraficosSuspendidos else { return }
        // El horizonte de predicción se fija a la latencia medida en vivo, no a
        // una constante: predecir más de lo que se tarda adelanta la tinta a la
        // punta, y predecir menos deja retraso sin tapar.
        sesion.horizonteMs = Float(min(max(latenciaMs, 3), 32))
        
        guard let renderizador = renderizador else { return }
        let enviado = renderizador.dibujar(sesion: sesion, tamano: bounds.size) { [weak self] tiempoGPU in
            DispatchQueue.main.async { [weak self] in
                self?.registrarLatencia(timestampEvento: touch.timestamp,
                                        tiempoCompletadoGPU: tiempoGPU)
            }
        }
        
        if !enviado {
            framesDiferidos += 1
            solicitarDibujo(timestampEvento: touch.timestamp)
        }
    }

    func rehornearYDibujar() {
        guard !recursosGraficosSuspendidos else { return }
        cancelarFramePendiente()
        let escala = window?.screen.scale ?? UIScreen.main.scale
        renderizador?.hornearTintaSeca(sesion: sesion, tamano: bounds.size,
                                      escala: escala, hasta: tiempoReproduccion)
        solicitarDibujo(timestampEvento: nil)
        alActualizarEstadisticas?()
    }

    private func hornearTrazoTerminadoYDibujar(indice: Int) {
        cancelarFramePendiente()
        let escala = window?.screen.scale ?? UIScreen.main.scale
        let incorporado = renderizador?.hornearTrazoTerminado(
            sesion: sesion, indice: indice, tamano: bounds.size,
            escala: escala) ?? false
        if !incorporado {
            renderizador?.hornearTintaSeca(
                sesion: sesion, tamano: bounds.size, escala: escala,
                hasta: tiempoReproduccion)
        }
        solicitarDibujo(timestampEvento: nil)
        alActualizarEstadisticas?()
    }

    private func solicitarDibujo(timestampEvento: TimeInterval?) {
        guard !recursosGraficosSuspendidos else { return }
        planificadorFrames.solicitar(timestampEvento: timestampEvento)
        guard enlaceFrames == nil else { return }
        let enlace = CADisplayLink(target: self,
                                   selector: #selector(entregarFramePendiente(_:)))
        enlace.preferredFrameRateRange = CAFrameRateRange(
            minimum: 60, maximum: 120, preferred: 120)
        enlace.add(to: .main, forMode: .common)
        enlaceFrames = enlace
    }

    @objc private func entregarFramePendiente(_ enlace: CADisplayLink) {
        guard let solicitud = planificadorFrames.consumir() else {
            if !dibujando { detenerEnlaceFrames() }
            return
        }
        guard let renderizador else {
            detenerEnlaceFrames()
            return
        }

        let timestamp = solicitud.timestampEvento
        let enviado = renderizador.dibujar(
            sesion: sesion, tamano: bounds.size) { [weak self] tiempoGPU in
                guard let timestamp else { return }
                DispatchQueue.main.async { [weak self] in
                    self?.registrarLatencia(timestampEvento: timestamp,
                                            tiempoCompletadoGPU: tiempoGPU)
                }
            }

        if !enviado {
            planificadorFrames.reponer(solicitud)
            framesDiferidos += 1
            return
        }
        if timestamp == nil { alActualizarEstadisticas?() }
        if !dibujando && !planificadorFrames.pendiente { detenerEnlaceFrames() }
    }

    private func registrarLatencia(timestampEvento: TimeInterval,
                                   tiempoCompletadoGPU: TimeInterval) {
        guard medidorLatencia.registrar(
            timestampEvento: timestampEvento,
            tiempoCompletadoGPU: tiempoCompletadoGPU) else { return }
        let resumen = medidorLatencia.resumen
        latenciaMs = resumen.p50
        latenciaP95Ms = resumen.p95
        latenciaP99Ms = resumen.p99
        latenciaMaximaMs = resumen.maxima
        alActualizarEstadisticas?()
    }

    private func cancelarFramePendiente() {
        planificadorFrames.cancelar()
        detenerEnlaceFrames()
    }

    private func detenerEnlaceFrames() {
        enlaceFrames?.invalidate()
        enlaceFrames = nil
    }

    // MARK: - Puntero del lápiz del iPad

    /// Rehace la forma y el color del puntero.
    ///
    /// Disco o anillo: el anillo deja ver lo que hay justo debajo de la punta,
    /// que es lo que se quiere al señalar sobre un PDF ya escrito.
    private func actualizarPuntero() {
        let radio = puntero.radioEfectivo
        let color = puntero.colorEfectivo(colorPluma: colorPluma)
        let caja = CGRect(x: 0, y: 0, width: radio * 2, height: radio * 2)

        capaPuntero.bounds = caja
        capaPuntero.path = UIBezierPath(ovalIn: caja).cgPath

        if puntero.anillo {
            let grosor = puntero.grosorAnilloEfectivo
            // El trazo se dibuja centrado en el camino, así que hay que meterlo
            // media anchura para que el anillo no se salga del radio pedido.
            let interior = caja.insetBy(dx: grosor / 2, dy: grosor / 2)
            capaPuntero.path = UIBezierPath(ovalIn: interior).cgPath
            capaPuntero.fillColor = UIColor.clear.cgColor
            capaPuntero.strokeColor = color.cgColor
            capaPuntero.lineWidth = grosor
        } else {
            capaPuntero.fillColor = color.cgColor
            capaPuntero.strokeColor = nil
            capaPuntero.lineWidth = 0
        }
    }

    @objc private func alFlotar(_ gesto: UIHoverGestureRecognizer) {
        switch gesto.state {
        case .began, .changed:
            guard herramienta != .mano,
                  puntero.debeVerse(hayHover: true, dibujando: dibujando) else {
                capaPuntero.isHidden = true
                return
            }
            let punto = gesto.location(in: self)
            // Sin desactivar las acciones implícitas, Core Animation interpola
            // la posición y el puntero se arrastra por detrás de la punta.
            CATransaction.begin()
            CATransaction.setDisableActions(true)
            capaPuntero.position = punto
            capaPuntero.transform = CATransform3DMakeScale(escalaPorAltura(gesto),
                                                           escalaPorAltura(gesto), 1)
            capaPuntero.isHidden = false
            CATransaction.commit()
        default:
            capaPuntero.isHidden = true
        }
    }

    /// El puntero encoge conforme la punta baja hacia el cristal.
    ///
    /// Es información que el lápiz del iPad da gratis (`zOffset`, en puntos) y que
    /// convierte el puntero en algo que además dice *cuánto falta* para
    /// escribir, no solo dónde.
    private func escalaPorAltura(_ gesto: UIHoverGestureRecognizer) -> CGFloat {
        guard #available(iOS 16.4, *) else { return 1 }
        let altura = gesto.zOffset  // 0 al tocar, ~1 en el límite del hover
        return 1.0 + min(max(altura, 0), 1) * 0.6
    }

    // MARK: - Acciones

    func deshacer() {
        let antes = instantaneaColaborativa()
        sesion.deshacer()
        publicarDiferencia(desde: antes)
        limpiarSeleccion()
        rehornearYDibujar()
    }

    func rehacer() {
        let antes = instantaneaColaborativa()
        sesion.rehacer()
        publicarDiferencia(desde: antes)
        limpiarSeleccion()
        rehornearYDibujar()
    }

    func limpiar() {
        let antes = instantaneaColaborativa()
        sesion.limpiar()
        publicarDiferencia(desde: antes)
        limpiarSeleccion()
        rehornearYDibujar()
    }

    func eliminarSeleccion() {
        guard !seleccionActual.vacia else { return }
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        guard !editable.vacia else { return }
        let antes = instantaneaColaborativa()
        if sesion.eliminarSeleccion(editable) {
            publicarDiferencia(desde: antes)
            limpiarSeleccion()
            rehornearYDibujar()
            alCambiarContenidoSeleccion?()
            alCambiarActividad?(false)
        }
    }

    func recolorearSeleccion(_ color: UInt32) {
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        guard !editable.trazos.isEmpty else { return }
        let antes = instantaneaColaborativa()
        if sesion.recolorearTrazos(editable.trazos, color: color) {
            publicarDiferencia(desde: antes)
            rehornearYDibujar()
        }
    }

    func seleccionarTodo() {
        seleccionActual = sesion.seleccionarTodo(
            filtro: ajustesSeleccion.filtro,
            colorARGB: ajustesSeleccion.colorARGB)
        actualizarMarcoSeleccion()
        notificarSeleccion()
    }

    func invertirSeleccion() {
        let todo = sesion.seleccionarTodo(filtro: ajustesSeleccion.filtro,
                                          colorARGB: ajustesSeleccion.colorARGB)
        seleccionActual = todo.combinada(con: seleccionActual, modo: .restar)
        actualizarMarcoSeleccion()
        notificarSeleccion()
    }

    func duplicarSeleccion() {
        guard !seleccionActual.vacia else { return }
        let antes = instantaneaColaborativa()
        seleccionActual = sesion.duplicarSeleccion(seleccionActual)
        publicarDiferencia(desde: antes)
        actualizarMarcoSeleccion()
        rehornearYDibujar()
        alCambiarContenidoSeleccion?()
        notificarSeleccion()
    }

    func ordenarSeleccion(alFrente: Bool) {
        guard !seleccionActual.vacia else { return }
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        guard !editable.vacia else { return }
        seleccionActual = sesion.ordenarSeleccion(editable,
                                                  alFrente: alFrente)
        actualizarMarcoSeleccion()
        rehornearYDibujar()
        alCambiarContenidoSeleccion?()
        notificarSeleccion()
    }

    func transformarSeleccion(escalaX: CGFloat = 1, escalaY: CGFloat = 1,
                              rotacion: CGFloat = 0,
                              reflejarHorizontal: Bool = false,
                              reflejarVertical: Bool = false) {
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        guard !editable.vacia, let marco = marco(de: editable) else { return }
        let antes = instantaneaColaborativa()
        guard sesion.transformarSeleccion(
            editable, escalaX: escalaX, escalaY: escalaY,
            rotacion: rotacion, pivote: CGPoint(x: marco.midX, y: marco.midY),
            reflejarHorizontal: reflejarHorizontal,
            reflejarVertical: reflejarVertical) else { return }
        publicarDiferencia(desde: antes)
        actualizarMarcoSeleccion()
        rehornearYDibujar()
        alCambiarContenidoSeleccion?()
        notificarSeleccion()
    }

    func agruparSeleccion() {
        guard seleccionActual.total > 1 else { return }
        RepositorioMetadatosSeleccionIPad.compartido.agrupar(
            seleccionActual, sesion: sesion)
    }

    func desagruparSeleccion() {
        guard !seleccionActual.vacia else { return }
        RepositorioMetadatosSeleccionIPad.compartido.desagrupar(
            seleccionActual, sesion: sesion)
    }

    func bloquearSeleccion(_ valor: Bool) {
        guard !seleccionActual.vacia else { return }
        RepositorioMetadatosSeleccionIPad.compartido.bloquear(
            seleccionActual, sesion: sesion, valor: valor)
    }

    private func marco(de seleccion: SeleccionLienzo) -> CGRect? {
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
    }

    private func componentes(de seleccion: SeleccionLienzo)
        -> [(seleccion: SeleccionLienzo, marco: CGRect)] {
        var salida: [(SeleccionLienzo, CGRect)] = []
        for indice in seleccion.trazos {
            if let marco = sesion.marcoTrazo(indice) {
                salida.append((SeleccionLienzo(trazos: [indice]), marco))
            }
        }
        let textos = sesion.textos
        for indice in seleccion.textos where textos.indices.contains(indice) {
            salida.append((SeleccionLienzo(textos: [indice]), textos[indice].marco))
        }
        let medios = sesion.medios
        for indice in seleccion.medios where medios.indices.contains(indice) {
            salida.append((SeleccionLienzo(medios: [indice]), medios[indice].marco))
        }
        return salida
    }

    func alinearSeleccion(_ alineacion: AlineacionSeleccionIPad) {
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        let piezas = componentes(de: editable)
        guard piezas.count > 1,
              let conjunto = piezas.map(\.marco).reduce(nil as CGRect?, {
                $0?.union($1) ?? $1
              }) else { return }
        let antes = instantaneaColaborativa()
        for pieza in piezas {
            let delta: CGSize
            switch alineacion {
            case .izquierda:
                delta = CGSize(width: conjunto.minX - pieza.marco.minX, height: 0)
            case .centroHorizontal:
                delta = CGSize(width: conjunto.midX - pieza.marco.midX, height: 0)
            case .derecha:
                delta = CGSize(width: conjunto.maxX - pieza.marco.maxX, height: 0)
            case .arriba:
                delta = CGSize(width: 0, height: conjunto.minY - pieza.marco.minY)
            case .centroVertical:
                delta = CGSize(width: 0, height: conjunto.midY - pieza.marco.midY)
            case .abajo:
                delta = CGSize(width: 0, height: conjunto.maxY - pieza.marco.maxY)
            }
            _ = sesion.transformarSeleccion(pieza.seleccion,
                                             desplazamiento: delta)
        }
        publicarDiferencia(desde: antes)
        actualizarMarcoSeleccion(); rehornearYDibujar()
        alCambiarContenidoSeleccion?(); notificarSeleccion()
    }

    func distribuirSeleccion(_ distribucion: DistribucionSeleccionIPad) {
        let editable = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(seleccionActual, sesion: sesion)
        var piezas = componentes(de: editable)
        guard piezas.count > 2 else { return }
        switch distribucion {
        case .horizontal: piezas.sort { $0.marco.midX < $1.marco.midX }
        case .vertical: piezas.sort { $0.marco.midY < $1.marco.midY }
        }
        let inicio = distribucion == .horizontal ? piezas.first!.marco.midX
                                                 : piezas.first!.marco.midY
        let fin = distribucion == .horizontal ? piezas.last!.marco.midX
                                              : piezas.last!.marco.midY
        let paso = (fin - inicio) / CGFloat(piezas.count - 1)
        let antes = instantaneaColaborativa()
        for (indice, pieza) in piezas.enumerated().dropFirst().dropLast() {
            let destino = inicio + CGFloat(indice) * paso
            let actual = distribucion == .horizontal ? pieza.marco.midX
                                                     : pieza.marco.midY
            let delta = distribucion == .horizontal
                ? CGSize(width: destino - actual, height: 0)
                : CGSize(width: 0, height: destino - actual)
            _ = sesion.transformarSeleccion(pieza.seleccion,
                                             desplazamiento: delta)
        }
        publicarDiferencia(desde: antes)
        actualizarMarcoSeleccion(); rehornearYDibujar()
        alCambiarContenidoSeleccion?(); notificarSeleccion()
    }

    func reproducirTinta(hasta tiempoMonotono: Double) {
        tiempoReproduccion = tiempoMonotono
        capaPuntero.isHidden = true
        rehornearYDibujar()
    }

    func finalizarReproduccion() {
        tiempoReproduccion = nil
        rehornearYDibujar()
    }

    /// Cambia el fondo del cuaderno. El color del papel y el de la tinta que le
    /// toca los decide el núcleo: en papel oscuro la tinta negra se aclara sola,
    /// que escribir negro sobre negro es el error fácil.
    func cambiarPapel(tipo: TipoPapel, pauta: Pauta, separacion: Float = 32,
                      margen: Float = 0) {
        sesion.configurarPapel(tipo: tipo, pauta: pauta,
                               separacion: separacion, margen: margen)
        let papel = sesion.colorPapel
        renderizador?.colorPapel = SIMD4<Float>(
            Float((papel >> 16) & 0xFF) / 255.0,
            Float((papel >> 8) & 0xFF) / 255.0,
            Float(papel & 0xFF) / 255.0,
            1.0)
        colorPluma = sesion.colorTintaSugerido
        actualizarPuntero()
        rehornearYDibujar()
    }

    func cambiarPapel(tipo: TipoPapel, ajustes: AjustesPapel) {
        sesion.configurarPapel(tipo: tipo, pauta: ajustes.pauta,
                               separacion: ajustes.separacion,
                               margen: ajustes.margenIzquierdo)
        sesion.configurarPapelAvanzado(ajustes)
        let papel = sesion.colorPapel
        renderizador?.colorPapel = SIMD4<Float>(
            Float((papel >> 16) & 0xFF) / 255.0,
            Float((papel >> 8) & 0xFF) / 255.0,
            Float(papel & 0xFF) / 255.0,
            1.0)
        colorPluma = sesion.colorTintaSugerido
        actualizarPuntero()
        rehornearYDibujar()
    }
}

// MARK: - Gestos del lápiz del iPad

extension LienzoCuadernU: UIPencilInteractionDelegate {

    /// Doble toque. Se respeta lo que el usuario haya elegido en Ajustes del
    /// sistema en vez de imponer una acción: si tiene puesto «cambiar a
    /// borrador», eso es lo que espera que pase en todas las apps.
    func pencilInteractionDidTap(_ interaction: UIPencilInteraction) {
        switch UIPencilInteraction.preferredTapAction {
        case .switchEraser:
            herramienta = herramienta == .borradorObjeto
                || herramienta == .borradorParcial
                ? herramientaAnterior : .borradorObjeto
        case .switchPrevious:
            herramienta = herramientaAnterior
        case .showColorPalette:
            alPedirPaleta?()
        default:
            break
        }
    }

    /// Apretón del lápiz del iPad Pro. Escrito a ciegas: no hay hardware donde
    /// probarlo, así que va detrás de una comprobación de versión y no toca
    /// nada del trazo.
    @available(iOS 17.5, *)
    func pencilInteraction(_ interaction: UIPencilInteraction,
                           didReceiveSqueeze squeeze: UIPencilInteraction.Squeeze) {
        guard squeeze.phase == .ended else { return }
        alApretarLapiz?()
    }
}
