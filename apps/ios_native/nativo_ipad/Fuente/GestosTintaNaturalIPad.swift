import ObjectiveC
import QuartzCore
import UIKit

enum TipoGestoTintaNaturalIPad: Equatable {
    case ninguno
    case circulo
    case garabato
}

struct AnalisisGestoTintaNaturalIPad: Equatable {
    var tipo: TipoGestoTintaNaturalIPad
    var marco: CGRect
    var longitud: CGFloat
    var reversionesX: Int
    var reversionesY: Int
    var cierreRelativo: CGFloat
}

enum DetectorGestoTintaNaturalIPad {
    static func analizar(_ puntos: [CGPoint]) -> AnalisisGestoTintaNaturalIPad {
        let limpios = simplificar(puntos)
        guard limpios.count >= 8 else {
            return .init(tipo: .ninguno, marco: .null, longitud: 0,
                         reversionesX: 0, reversionesY: 0,
                         cierreRelativo: .infinity)
        }
        let marco = bounding(limpios)
        guard marco.width >= 12, marco.height >= 8,
              marco.width.isFinite, marco.height.isFinite else {
            return .init(tipo: .ninguno, marco: marco, longitud: 0,
                         reversionesX: 0, reversionesY: 0,
                         cierreRelativo: .infinity)
        }
        let longitud = longitudCamino(limpios)
        let diagonal = max(hypot(marco.width, marco.height), 1)
        let cierre = distancia(limpios.first!, limpios.last!) / diagonal
        let rx = reversiones(limpios.map(\.x))
        let ry = reversiones(limpios.map(\.y))
        let densidad = longitud / diagonal

        let esGarabato = limpios.count >= 12
            && densidad >= 3.15
            && max(rx, ry) >= 4
            && min(marco.width, marco.height) >= 8

        let proporcion = marco.width / max(marco.height, 1)
        let esCirculo = limpios.count >= 12
            && cierre <= 0.28
            && densidad >= 1.75 && densidad <= 3.05
            && proporcion >= 0.32 && proporcion <= 3.1
            && rx <= 4 && ry <= 4

        return AnalisisGestoTintaNaturalIPad(
            tipo: esGarabato ? .garabato : (esCirculo ? .circulo : .ninguno),
            marco: marco, longitud: longitud,
            reversionesX: rx, reversionesY: ry,
            cierreRelativo: cierre)
    }

    static func bounding(_ puntos: [CGPoint]) -> CGRect {
        guard let primero = puntos.first else { return .null }
        var minX = primero.x, maxX = primero.x
        var minY = primero.y, maxY = primero.y
        for p in puntos.dropFirst() {
            minX = min(minX, p.x); maxX = max(maxX, p.x)
            minY = min(minY, p.y); maxY = max(maxY, p.y)
        }
        return CGRect(x: minX, y: minY,
                      width: max(maxX - minX, 0.01),
                      height: max(maxY - minY, 0.01))
    }

    private static func simplificar(_ puntos: [CGPoint]) -> [CGPoint] {
        guard let primero = puntos.first else { return [] }
        var salida = [primero]
        salida.reserveCapacity(puntos.count)
        for punto in puntos.dropFirst() {
            guard let ultimo = salida.last,
                  distancia(ultimo, punto) >= 1.2 else { continue }
            salida.append(punto)
        }
        return salida
    }

    private static func longitudCamino(_ puntos: [CGPoint]) -> CGFloat {
        zip(puntos, puntos.dropFirst()).reduce(0) { parcial, par in
            parcial + distancia(par.0, par.1)
        }
    }

    private static func distancia(_ a: CGPoint, _ b: CGPoint) -> CGFloat {
        hypot(a.x - b.x, a.y - b.y)
    }

    private static func reversiones(_ valores: [CGFloat]) -> Int {
        guard valores.count >= 3 else { return 0 }
        var signoAnterior = 0
        var cambios = 0
        for (a, b) in zip(valores, valores.dropFirst()) {
            let delta = b - a
            guard abs(delta) >= 1.8 else { continue }
            let signo = delta > 0 ? 1 : -1
            if signoAnterior != 0, signo != signoAnterior { cambios += 1 }
            signoAnterior = signo
        }
        return cambios
    }
}

final class RastreadorGestoTintaNaturalIPad: UIGestureRecognizer {
    private(set) var puntos: [CGPoint] = []

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        guard touches.count == 1, let toque = touches.first else {
            state = .failed; return
        }
        puntos = muestras(toque, evento: event)
        state = .began
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent) {
        guard let toque = touches.first else { return }
        puntos.append(contentsOf: muestras(toque, evento: event))
        state = .changed
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
        if let toque = touches.first {
            puntos.append(contentsOf: muestras(toque, evento: event))
        }
        state = .ended
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) {
        state = .cancelled
    }

    override func reset() {
        puntos.removeAll(keepingCapacity: true)
        super.reset()
    }

    private func muestras(_ toque: UITouch, evento: UIEvent) -> [CGPoint] {
        let disponibles = evento.coalescedTouches(for: toque) ?? [toque]
        return disponibles.map { $0.preciseLocation(in: view) }
    }
}

private struct CandidatoCirculoTintaIPad {
    var idTrazo: String
    var puntos: [CGPoint]
    var marco: CGRect
    var creado: CFTimeInterval
}

private final class CoordinadorGestosTintaNaturalIPad: NSObject,
                                                       UIGestureRecognizerDelegate {
    weak var lienzo: LienzoCuadernU?
    private var candidato: CandidatoCirculoTintaIPad?

    init(lienzo: LienzoCuadernU) { self.lienzo = lienzo }

    func instalar() {
        guard let lienzo else { return }
        let rastreador = RastreadorGestoTintaNaturalIPad(
            target: self, action: #selector(recibirTrazo(_:)))
        rastreador.cancelsTouchesInView = false
        rastreador.delaysTouchesBegan = false
        rastreador.delaysTouchesEnded = false
        rastreador.allowedTouchTypes = [NSNumber(value: UITouch.TouchType.pencil.rawValue)]
        rastreador.delegate = self
        rastreador.name = "cuadernu.gesto.tinta.natural"
        lienzo.addGestureRecognizer(rastreador)

        let mantener = UILongPressGestureRecognizer(
            target: self, action: #selector(mantenerCirculo(_:)))
        mantener.minimumPressDuration = 0.38
        mantener.cancelsTouchesInView = false
        mantener.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.pencil.rawValue),
            NSNumber(value: UITouch.TouchType.direct.rawValue),
        ]
        mantener.delegate = self
        mantener.name = "cuadernu.gesto.circle-to-lasso"
        lienzo.addGestureRecognizer(mantener)
    }

    @objc private func recibirTrazo(_ gesto: RastreadorGestoTintaNaturalIPad) {
        guard gesto.state == .ended, let lienzo,
              lienzo.herramienta == .pluma else { return }
        let puntos = gesto.puntos
        let analisis = DetectorGestoTintaNaturalIPad.analizar(puntos)
        DispatchQueue.main.async { [weak self, weak lienzo] in
            guard let self, let lienzo, lienzo.herramienta == .pluma else { return }
            self.procesar(analisis: analisis, puntos: puntos, lienzo: lienzo)
        }
    }

    private func procesar(analisis: AnalisisGestoTintaNaturalIPad,
                          puntos: [CGPoint], lienzo: LienzoCuadernU) {
        guard lienzo.sesion.cantidadTrazos > 0 else { return }
        let preferencias = PreferenciasGestosTintaIPad.actual
        switch analisis.tipo {
        case .garabato:
            candidato = nil
            guard preferencias.scribbleToErase else { return }
            aplicarGarabato(puntos: puntos, lienzo: lienzo)
        case .circulo:
            guard preferencias.circleToLasso else {
                candidato = nil; return
            }
            let indice = lienzo.sesion.cantidadTrazos - 1
            guard let id = lienzo.sesion.asegurarIdTrazo(indice) else { return }
            candidato = CandidatoCirculoTintaIPad(
                idTrazo: id, puntos: puntos, marco: analisis.marco,
                creado: CACurrentMediaTime())
            lienzo.alNotificarGestoRegion?(
                "Círculo detectado: manténgalo pulsado para seleccionar su contenido.")
        case .ninguno:
            candidato = nil
        }
    }

    private func aplicarGarabato(puntos: [CGPoint], lienzo: LienzoCuadernU) {
        let sesion = lienzo.sesion
        let indiceGarabato = sesion.cantidadTrazos - 1
        guard indiceGarabato >= 0,
              let idGarabato = sesion.asegurarIdTrazo(indiceGarabato) else { return }
        var ajustes = AjustesSeleccionLienzo()
        ajustes.filtro = .tinta
        ajustes.requerirCompleto = false
        ajustes.radioPincel = max(8, lienzo.anchoPluma * 1.8)
        let tocada = sesion.seleccionarPincel(puntos, ajustes: ajustes)
        let idsObjetivo = tocada.trazos
            .filter { $0 != indiceGarabato }
            .compactMap { sesion.asegurarIdTrazo($0) }

        guard sesion.eliminarTrazos([indiceGarabato]) else { return }
        lienzo.alEliminarTrazosColaborativos?([idGarabato])

        let indicesObjetivo = idsObjetivo.compactMap { sesion.indiceTrazo(id: $0) }
        if !indicesObjetivo.isEmpty,
           sesion.eliminarTrazos(indicesObjetivo) {
            lienzo.alEliminarTrazosColaborativos?(idsObjetivo)
        }
        lienzo.rehornearYDibujar()
        lienzo.alCambiarContenidoSeleccion?()
        UINotificationFeedbackGenerator().notificationOccurred(.success)
        lienzo.alNotificarGestoRegion?(
            idsObjetivo.isEmpty
                ? "Garabato reconocido; no había tinta debajo."
                : "Garabato: \(idsObjetivo.count) trazo\(idsObjetivo.count == 1 ? "" : "s") borrado\(idsObjetivo.count == 1 ? "" : "s").")
    }

    @objc private func mantenerCirculo(_ gesto: UILongPressGestureRecognizer) {
        guard PreferenciasGestosTintaIPad.actual.circleToLasso,
              gesto.state == .began, let lienzo, let candidato else { return }
        guard CACurrentMediaTime() - candidato.creado <= 4.0,
              let indiceCirculo = lienzo.sesion.indiceTrazo(id: candidato.idTrazo) else {
            self.candidato = nil; return
        }
        let punto = gesto.location(in: lienzo)
        let margen = max(candidato.marco.width, candidato.marco.height) * 0.16 + 12
        guard candidato.marco.insetBy(dx: -margen, dy: -margen).contains(punto) else {
            return
        }

        if lienzo.sesion.hayTrazoEnCurso {
            lienzo.sesion.descartarTrazo()
            lienzo.sesion.limpiarProvisionales()
        }

        var ajustes = AjustesSeleccionLienzo()
        ajustes.filtro = .tinta
        ajustes.requerirCompleto = false
        let seleccion = lienzo.sesion.seleccionarPoligono(
            candidato.puntos, ajustes: ajustes)
        let ids = seleccion.trazos
            .filter { $0 != indiceCirculo }
            .compactMap { lienzo.sesion.asegurarIdTrazo($0) }
        _ = lienzo.sesion.eliminarTrazos([indiceCirculo])
        lienzo.alEliminarTrazosColaborativos?([candidato.idTrazo])
        self.candidato = nil

        let indices = ids.compactMap { lienzo.sesion.indiceTrazo(id: $0) }
        lienzo.rehornearYDibujar()
        lienzo.alCambiarContenidoSeleccion?()
        guard !indices.isEmpty,
              let navegador = lienzo.superviewHasta(VistaLienzoNavegable.self),
              let presentador = lienzo.controladorVisibleSuperior() else {
            lienzo.alNotificarGestoRegion?("Círculo convertido; no había tinta seleccionable dentro.")
            return
        }
        EditorSeleccionCircularIPad.presentar(
            trazos: indices,
            navegador: navegador,
            presentador: presentador)
    }

    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer)
        -> Bool { true }
}

private enum AsociadosGestosTintaNaturalIPad {
    static var coordinador: UInt8 = 0
}

extension LienzoCuadernU {
    func activarGestosTintaNaturalIPad() {
        guard objc_getAssociatedObject(
            self, &AsociadosGestosTintaNaturalIPad.coordinador) == nil else { return }
        let coordinador = CoordinadorGestosTintaNaturalIPad(lienzo: self)
        objc_setAssociatedObject(
            self, &AsociadosGestosTintaNaturalIPad.coordinador,
            coordinador, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
        coordinador.instalar()
    }

    fileprivate func superviewHasta<T: UIView>(_ tipo: T.Type) -> T? {
        var actual: UIView? = self
        while let vista = actual {
            if let resultado = vista as? T { return resultado }
            actual = vista.superview
        }
        return nil
    }

    fileprivate func controladorVisibleSuperior() -> UIViewController? {
        guard let raiz = window?.rootViewController else { return nil }
        func superior(_ vc: UIViewController) -> UIViewController {
            if let presentado = vc.presentedViewController { return superior(presentado) }
            if let nav = vc as? UINavigationController,
               let visible = nav.visibleViewController { return superior(visible) }
            if let tabs = vc as? UITabBarController,
               let seleccionado = tabs.selectedViewController { return superior(seleccionado) }
            if let split = vc as? UISplitViewController,
               let ultimo = split.viewControllers.last { return superior(ultimo) }
            return vc
        }
        return superior(raiz)
    }
}
