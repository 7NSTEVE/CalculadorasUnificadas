import UIKit

/// Editor de una selección de tinta creada por Circle-to-Lasso. No toca el
/// estado privado del lazo: trabaja sobre índices estables y publica los mismos
/// deltas colaborativos que las demás operaciones de tinta.
final class EditorSeleccionCircularIPad: UIView {
    private weak var navegador: VistaLienzoNavegable?
    private weak var presentador: UIViewController?
    private let sesion: Sesion
    private var trazos: [Int]
    private var marcoDocumento: CGRect
    private let capa = CAShapeLayer()
    private let etiqueta = UILabel()
    private var gestoArrastre: UIPanGestureRecognizer?
    private var marcoVistaInicial: CGRect = .zero

    static func presentar(
        trazos: [Int],
        navegador: VistaLienzoNavegable,
        presentador: UIViewController
    ) {
        let validos = Array(Set(trazos)).sorted().filter {
            $0 >= 0 && $0 < navegador.lienzo.sesion.cantidadTrazos
        }
        guard !validos.isEmpty else { return }
        let marcos = validos.compactMap(navegador.lienzo.sesion.marcoTrazo)
        guard let primero = marcos.first else { return }
        let union = marcos.dropFirst().reduce(primero) { $0.union($1) }
        let vista = EditorSeleccionCircularIPad(
            trazos: validos, marco: union,
            navegador: navegador, presentador: presentador)
        vista.frame = navegador.bounds
        vista.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        navegador.addSubview(vista)
        vista.mostrarMenu()
    }

    private init(
        trazos: [Int], marco: CGRect,
        navegador: VistaLienzoNavegable,
        presentador: UIViewController
    ) {
        self.trazos = trazos
        self.marcoDocumento = marco
        self.navegador = navegador
        self.presentador = presentador
        self.sesion = navegador.lienzo.sesion
        super.init(frame: .zero)
        backgroundColor = .clear
        accessibilityIdentifier = "handwriting.circle.selection"

        capa.fillColor = UIColor.systemBlue.withAlphaComponent(0.08).cgColor
        capa.strokeColor = UIColor.systemBlue.cgColor
        capa.lineWidth = 2
        capa.lineDashPattern = [7, 5]
        layer.addSublayer(capa)

        etiqueta.text = "Selección por círculo · \(trazos.count) trazo\(trazos.count == 1 ? "" : "s")"
        etiqueta.font = .preferredFont(forTextStyle: .headline)
        etiqueta.textAlignment = .center
        etiqueta.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.94)
        etiqueta.layer.cornerRadius = 12
        etiqueta.layer.masksToBounds = true
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        addSubview(etiqueta)
        NSLayoutConstraint.activate([
            etiqueta.topAnchor.constraint(equalTo: safeAreaLayoutGuide.topAnchor, constant: 12),
            etiqueta.centerXAnchor.constraint(equalTo: centerXAnchor),
            etiqueta.widthAnchor.constraint(lessThanOrEqualTo: widthAnchor, multiplier: 0.78),
            etiqueta.heightAnchor.constraint(greaterThanOrEqualToConstant: 44),
        ])
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func layoutSubviews() {
        super.layoutSubviews()
        capa.frame = bounds
        actualizarMarco()
    }

    private func actualizarMarco() {
        guard let navegador else { return }
        let rect = convert(marcoDocumento, from: navegador.lienzo)
        marcoVistaInicial = rect
        CATransaction.begin(); CATransaction.setDisableActions(true)
        capa.path = UIBezierPath(
            roundedRect: rect.insetBy(dx: -7, dy: -7), cornerRadius: 9).cgPath
        CATransaction.commit()
    }

    private func mostrarMenu() {
        guard let presentador else { removeFromSuperview(); return }
        let hoja = UIAlertController(
            title: "Selección por círculo",
            message: "Tinta vectorial seleccionada sin cambiar de herramienta.",
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Mover con arrastre", style: .default) {
            [weak self] _ in DispatchQueue.main.async { self?.activarArrastre() }
        })
        hoja.addAction(UIAlertAction(title: "Copiar", style: .default) {
            [weak self] _ in self?.copiar(cortar: false)
        })
        hoja.addAction(UIAlertAction(title: "Cortar", style: .default) {
            [weak self] _ in self?.copiar(cortar: true)
        })
        hoja.addAction(UIAlertAction(title: "Duplicar", style: .default) {
            [weak self] _ in self?.duplicar()
        })
        hoja.addAction(UIAlertAction(title: "Recolorear…", style: .default) {
            [weak self] _ in DispatchQueue.main.async { self?.pedirColor() }
        })
        hoja.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self] _ in self?.eliminar()
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
            [weak self] _ in self?.removeFromSuperview()
        })
        hoja.popoverPresentationController?.sourceView = self
        hoja.popoverPresentationController?.sourceRect = marcoVistaInicial
        presentador.present(hoja, animated: true)
    }

    private func activarArrastre() {
        isUserInteractionEnabled = true
        etiqueta.text = "Arrastre la selección y suelte para aplicar"
        let pan = UIPanGestureRecognizer(target: self, action: #selector(arrastrar(_:)))
        gestoArrastre = pan
        addGestureRecognizer(pan)
    }

    @objc private func arrastrar(_ gesto: UIPanGestureRecognizer) {
        guard let navegador else { return }
        let traslacion = gesto.translation(in: self)
        switch gesto.state {
        case .began, .changed:
            CATransaction.begin(); CATransaction.setDisableActions(true)
            capa.path = UIBezierPath(
                roundedRect: marcoVistaInicial
                    .offsetBy(dx: traslacion.x, dy: traslacion.y)
                    .insetBy(dx: -7, dy: -7), cornerRadius: 9).cgPath
            CATransaction.commit()
        case .ended:
            let p0 = navegador.lienzo.convert(CGPoint.zero, from: self)
            let p1 = navegador.lienzo.convert(
                CGPoint(x: traslacion.x, y: traslacion.y), from: self)
            let delta = CGSize(width: p1.x - p0.x, height: p1.y - p0.y)
            guard abs(delta.width) >= 0.5 || abs(delta.height) >= 0.5 else {
                removeFromSuperview(); return
            }
            aplicarCambio {
                sesion.transformarTrazos(
                    trazos, desplazamiento: delta,
                    pivote: CGPoint(x: marcoDocumento.midX, y: marcoDocumento.midY))
            }
        case .cancelled, .failed:
            removeFromSuperview()
        default: break
        }
    }

    private func copiar(cortar: Bool) {
        guard let lienzo = navegador?.lienzo,
              lienzo.prepararSeleccionParaPegadoRapido(
                SeleccionLienzo(trazos: trazos), incluirHistorial: true) else {
            removeFromSuperview(); return
        }
        if !cortar {
            UINotificationFeedbackGenerator().notificationOccurred(.success)
            removeFromSuperview(); return
        }
        isUserInteractionEnabled = false
        etiqueta.text = "Cortando selección vectorial…"
        DispatchQueue.main.async { [weak self] in self?.eliminar() }
    }

    private func duplicar() {
        let idsAntes = Set(idsYDatos().keys)
        let copia = sesion.duplicarSeleccion(
            SeleccionLienzo(trazos: trazos), desplazamiento: CGSize(width: 28, height: 28))
        guard !copia.trazos.isEmpty else { removeFromSuperview(); return }
        publicarNuevos(copia.trazos, excluyendo: idsAntes)
        finalizarCambio()
    }

    private func pedirColor() {
        let colores: [(String, UInt32)] = [
            ("Negro", 0xFF1A1A1A), ("Azul", 0xFF1B5FA8),
            ("Rojo", 0xFFC62828), ("Verde", 0xFF2E7D32),
            ("Naranja", 0xFFF57C00),
        ]
        let hoja = UIAlertController(title: "Recolorear selección",
                                     message: nil, preferredStyle: .actionSheet)
        for (nombre, color) in colores {
            hoja.addAction(UIAlertAction(title: nombre, style: .default) {
                [weak self] _ in
                self?.aplicarCambio {
                    self?.sesion.recolorearTrazos(self?.trazos ?? [], color: color) == true
                }
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
            [weak self] _ in self?.removeFromSuperview()
        })
        hoja.popoverPresentationController?.sourceView = self
        hoja.popoverPresentationController?.sourceRect = marcoVistaInicial
        presentarCuandoLibre(hoja)
    }

    private func eliminar() {
        aplicarCambio { sesion.eliminarTrazos(trazos) }
    }

    private func aplicarCambio(_ operacion: () -> Bool) {
        let antes = idsYDatos()
        guard operacion() else { removeFromSuperview(); return }
        publicarDiferencia(antes: antes)
        finalizarCambio()
    }

    private func finalizarCambio() {
        navegador?.lienzo.rehornearYDibujar()
        navegador?.lienzo.alCambiarContenidoSeleccion?()
        navegador?.capaElementos.recargar()
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
        removeFromSuperview()
    }

    private func idsYDatos() -> [String: Data] {
        var resultado: [String: Data] = [:]
        for indice in trazos {
            guard indice >= 0, indice < sesion.cantidadTrazos,
                  let id = sesion.asegurarIdTrazo(indice),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            resultado[id] = datos
        }
        return resultado
    }

    private func publicarDiferencia(antes: [String: Data]) {
        guard let lienzo = navegador?.lienzo else { return }
        let idsAntes = Set(antes.keys)
        var despues: [String: Data] = [:]
        for indice in 0..<sesion.cantidadTrazos {
            guard let id = sesion.idTrazo(indice), idsAntes.contains(id),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            despues[id] = datos
        }
        let eliminados = idsAntes.subtracting(despues.keys)
        let cambiados = despues.compactMap { id, datos in
            antes[id] != datos ? id : nil
        }
        let tumbas = Array(eliminados.union(cambiados)).sorted()
        if !tumbas.isEmpty { lienzo.alEliminarTrazosColaborativos?(tumbas) }
        for id in cambiados.sorted() {
            if let datos = despues[id] { lienzo.alCrearTrazoColaborativo?(id, datos) }
        }
    }

    private func publicarNuevos(_ indices: [Int], excluyendo anteriores: Set<String>) {
        guard let lienzo = navegador?.lienzo else { return }
        for indice in indices {
            guard let id = sesion.asegurarIdTrazo(indice),
                  !anteriores.contains(id),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            lienzo.alCrearTrazoColaborativo?(id, datos)
        }
    }

    private func presentarCuandoLibre(_ controlador: UIViewController, intento: Int = 0) {
        guard let presentador,
              presentador.viewIfLoaded?.window != nil else {
            removeFromSuperview(); return
        }
        if presentador.presentedViewController == nil {
            presentador.present(controlador, animated: true); return
        }
        guard intento < 8 else { removeFromSuperview(); return }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.presentarCuandoLibre(controlador, intento: intento + 1)
        }
    }
}
