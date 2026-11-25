import UIKit

/// Captura un único toque sobre la tinta y permite editar el grupo geométrico
/// (palabra o línea) sin convertirlo a imagen ni crear un segundo modelo.
final class CapturadorEdicionManuscritaIPad: UIView {
    private weak var navegador: VistaLienzoNavegable?
    private weak var presentador: UIViewController?
    private let nivel: NivelEdicionManuscritaIPad
    private let sesion: Sesion
    private let alCambiar: () -> Void
    private let capaSeleccion = CAShapeLayer()
    private let etiqueta = UILabel()
    private let botonCancelar = UIButton(type: .system)
    private var grupoArrastre: GrupoManuscritoIPad?
    private var marcoArrastreVista: CGRect?
    private var gestoToque: UITapGestureRecognizer?
    private var gestoArrastre: UIPanGestureRecognizer?

    static func instalar(
        sobre navegador: VistaLienzoNavegable,
        nivel: NivelEdicionManuscritaIPad,
        presentador: UIViewController,
        alCambiar: @escaping () -> Void
    ) {
        navegador.subviews.compactMap { $0 as? CapturadorEdicionManuscritaIPad }
            .forEach { $0.removeFromSuperview() }
        let vista = CapturadorEdicionManuscritaIPad(
            navegador: navegador,
            nivel: nivel,
            presentador: presentador,
            alCambiar: alCambiar)
        vista.frame = navegador.bounds
        vista.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        navegador.addSubview(vista)
    }

    private init(
        navegador: VistaLienzoNavegable,
        nivel: NivelEdicionManuscritaIPad,
        presentador: UIViewController,
        alCambiar: @escaping () -> Void
    ) {
        self.navegador = navegador
        self.nivel = nivel
        self.presentador = presentador
        self.sesion = navegador.lienzo.sesion
        self.alCambiar = alCambiar
        super.init(frame: .zero)
        backgroundColor = .clear
        isOpaque = false
        accessibilityIdentifier = "handwriting.semantic.capture.\(nivel.rawValue)"

        capaSeleccion.fillColor = UIColor.systemBlue.withAlphaComponent(0.08).cgColor
        capaSeleccion.strokeColor = UIColor.systemBlue.cgColor
        capaSeleccion.lineWidth = 2
        capaSeleccion.lineDashPattern = [8, 5]
        layer.addSublayer(capaSeleccion)

        etiqueta.text = nivel == .palabra
            ? "Toque una palabra manuscrita"
            : "Toque una línea manuscrita"
        etiqueta.font = .preferredFont(forTextStyle: .headline)
        etiqueta.textColor = .label
        etiqueta.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.94)
        etiqueta.layer.cornerRadius = 12
        etiqueta.layer.masksToBounds = true
        etiqueta.textAlignment = .center
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        addSubview(etiqueta)

        var config = UIButton.Configuration.filled()
        config.title = "Cancelar"
        config.cornerStyle = .capsule
        botonCancelar.configuration = config
        botonCancelar.translatesAutoresizingMaskIntoConstraints = false
        botonCancelar.addTarget(self, action: #selector(cancelar), for: .touchUpInside)
        addSubview(botonCancelar)

        NSLayoutConstraint.activate([
            etiqueta.topAnchor.constraint(equalTo: safeAreaLayoutGuide.topAnchor, constant: 12),
            etiqueta.centerXAnchor.constraint(equalTo: centerXAnchor),
            etiqueta.widthAnchor.constraint(lessThanOrEqualTo: widthAnchor, multiplier: 0.72),
            etiqueta.heightAnchor.constraint(greaterThanOrEqualToConstant: 44),
            botonCancelar.topAnchor.constraint(equalTo: etiqueta.bottomAnchor, constant: 8),
            botonCancelar.centerXAnchor.constraint(equalTo: centerXAnchor),
        ])

        let toque = UITapGestureRecognizer(target: self, action: #selector(tocar(_:)))
        gestoToque = toque
        addGestureRecognizer(toque)
        UIAccessibility.post(notification: .announcement, argument: etiqueta.text)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func layoutSubviews() {
        super.layoutSubviews()
        capaSeleccion.frame = bounds
    }

    @objc private func cancelar() { removeFromSuperview() }

    @objc private func tocar(_ gesto: UITapGestureRecognizer) {
        guard gesto.state == .ended,
              grupoArrastre == nil,
              let navegador else { return }
        let puntoVista = gesto.location(in: self)
        if etiqueta.frame.insetBy(dx: -12, dy: -8).contains(puntoVista)
            || botonCancelar.frame.insetBy(dx: -12, dy: -8).contains(puntoVista) {
            return
        }
        let puntoDocumento = navegador.lienzo.convert(puntoVista, from: self)
        guard let grupo = MotorEdicionManuscritaIPad.grupo(
            cerca: puntoDocumento, nivel: nivel, sesion: sesion) else {
            etiqueta.text = "No detecté \(nivel.rawValue) aquí. Toque más cerca de la tinta."
            UINotificationFeedbackGenerator().notificationOccurred(.warning)
            return
        }
        mostrar(grupo, navegador: navegador)
    }

    private func mostrar(_ grupo: GrupoManuscritoIPad,
                         navegador: VistaLienzoNavegable) {
        let marcoVista = convert(grupo.marco, from: navegador.lienzo)
        capaSeleccion.path = UIBezierPath(
            roundedRect: marcoVista.insetBy(dx: -7, dy: -7),
            cornerRadius: 9).cgPath
        etiqueta.text = "\(grupo.trazos.count) trazo\(grupo.trazos.count == 1 ? "" : "s") · \(nivel.rawValue)"
        isUserInteractionEnabled = false

        guard let presentador = presentadorSuperior() else {
            removeFromSuperview(); return
        }
        let hoja = UIAlertController(
            title: nivel == .palabra ? "Editar palabra manuscrita" : "Editar línea manuscrita",
            message: "La tinta permanece vectorial y cada cambio se puede deshacer.",
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Mover con arrastre", style: .default) {
            [weak self] _ in
            DispatchQueue.main.async { self?.activarArrastre(grupo) }
        })
        hoja.addAction(UIAlertAction(title: "Copiar", style: .default) {
            [weak self] _ in self?.copiar(grupo, cortar: false)
        })
        hoja.addAction(UIAlertAction(title: "Cortar", style: .default) {
            [weak self] _ in self?.copiar(grupo, cortar: true)
        })
        hoja.addAction(UIAlertAction(title: "Copiar ancla exacta", style: .default) {
            [weak self] _ in self?.copiarAncla(grupo)
        })
        let enlazar = UIAlertAction(title: "Enlazar aquí al ancla copiada", style: .default) {
            [weak self] _ in self?.enlazarAAncla(grupo)
        }
        enlazar.isEnabled = PortapapelesAnclasDirectasIPad.compartido.ancla != nil
        hoja.addAction(enlazar)
        hoja.addAction(UIAlertAction(title: "Duplicar", style: .default) {
            [weak self] _ in self?.duplicar(grupo)
        })
        hoja.addAction(UIAlertAction(title: "Recolorear…", style: .default) {
            [weak self] _ in
            DispatchQueue.main.async { self?.pedirColor(grupo) }
        })
        hoja.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self] _ in self?.eliminar(grupo)
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
            [weak self] _ in self?.removeFromSuperview()
        })
        hoja.popoverPresentationController?.sourceView = self
        hoja.popoverPresentationController?.sourceRect = marcoVista
        presentador.present(hoja, animated: true)
    }

    private func activarArrastre(_ grupo: GrupoManuscritoIPad) {
        guard let navegador else { removeFromSuperview(); return }
        isUserInteractionEnabled = true
        grupoArrastre = grupo
        marcoArrastreVista = convert(grupo.marco, from: navegador.lienzo)
        etiqueta.text = "Arrastre la \(nivel.rawValue) y suelte para aplicar"
        gestoToque?.isEnabled = false
        let pan = UIPanGestureRecognizer(target: self,
                                         action: #selector(arrastrarGrupo(_:)))
        gestoArrastre = pan
        addGestureRecognizer(pan)
        UIAccessibility.post(notification: .announcement, argument: etiqueta.text)
    }

    @objc private func arrastrarGrupo(_ gesto: UIPanGestureRecognizer) {
        guard let grupo = grupoArrastre,
              let navegador,
              let marco = marcoArrastreVista else { return }
        let traslacion = gesto.translation(in: self)
        switch gesto.state {
        case .began, .changed:
            CATransaction.begin()
            CATransaction.setDisableActions(true)
            capaSeleccion.path = UIBezierPath(
                roundedRect: marco.offsetBy(dx: traslacion.x, dy: traslacion.y)
                    .insetBy(dx: -7, dy: -7),
                cornerRadius: 9).cgPath
            CATransaction.commit()
        case .ended:
            let origenDocumento = navegador.lienzo.convert(CGPoint.zero, from: self)
            let destinoDocumento = navegador.lienzo.convert(
                CGPoint(x: traslacion.x, y: traslacion.y), from: self)
            let delta = CGSize(
                width: destinoDocumento.x - origenDocumento.x,
                height: destinoDocumento.y - origenDocumento.y)
            if abs(delta.width) < 0.5 && abs(delta.height) < 0.5 {
                removeFromSuperview(); return
            }
            aplicarCambio(grupo: grupo) {
                MotorEdicionManuscritaIPad.mover(grupo, delta: delta, sesion: sesion)
            }
        case .cancelled, .failed:
            removeFromSuperview()
        default:
            break
        }
    }

    private func copiar(_ grupo: GrupoManuscritoIPad, cortar: Bool) {
        guard let lienzo = navegador?.lienzo,
              lienzo.prepararSeleccionParaPegadoRapido(
                SeleccionLienzo(trazos: grupo.trazos),
                incluirHistorial: true) else {
            removeFromSuperview(); return
        }
        if !cortar {
            UINotificationFeedbackGenerator().notificationOccurred(.success)
            removeFromSuperview()
            return
        }
        // El renderer de compatibilidad PNG se encola antes de este bloque.
        // Borrar en el siguiente turno conserva tanto el paquete vectorial como
        // la miniatura externa sin leer índices ya eliminados.
        isUserInteractionEnabled = false
        etiqueta.text = "Cortando sin rasterizar la tinta…"
        DispatchQueue.main.async { [weak self] in
            self?.eliminar(grupo)
        }
    }

    private func copiarAncla(_ grupo: GrupoManuscritoIPad) {
        let seleccion = SeleccionLienzo(trazos: grupo.trazos)
        let curso = CentroExperienciaIPad.compartido.instantanea()
            .espacioActivo?.cursoId
        guard PortapapelesAnclasDirectasIPad.compartido.copiar(
            seleccion: seleccion,
            sesion: sesion,
            titulo: nivel == .palabra ? "Palabra manuscrita" : "Línea manuscrita",
            cursoId: curso) != nil else {
            UINotificationFeedbackGenerator().notificationOccurred(.error)
            removeFromSuperview(); return
        }
        UINotificationFeedbackGenerator().notificationOccurred(.success)
        navegador?.lienzo.alNotificarGestoRegion?(
            "Ancla exacta copiada. Seleccione otra handwriting y elija «Enlazar aquí al ancla copiada».")
        removeFromSuperview()
    }

    private func enlazarAAncla(_ grupo: GrupoManuscritoIPad) {
        guard let ancla = PortapapelesAnclasDirectasIPad.compartido.ancla,
              EnlazadorDirectoIPad.vincular(
                seleccionOrigen: SeleccionLienzo(trazos: grupo.trazos),
                sesion: sesion,
                tituloOrigen: nivel == .palabra ? "Palabra manuscrita" : "Línea manuscrita",
                ancla: ancla) != nil else {
            UINotificationFeedbackGenerator().notificationOccurred(.warning)
            navegador?.lienzo.alNotificarGestoRegion?(
                "No se creó el enlace: falta un ancla o ambas selecciones se superponen.")
            removeFromSuperview(); return
        }
        UINotificationFeedbackGenerator().notificationOccurred(.success)
        navegador?.lienzo.alNotificarGestoRegion?(
            "Enlace directo creado → \(ancla.fuente.titulo). El backlink quedó registrado.")
        removeFromSuperview()
    }

    private func duplicar(_ grupo: GrupoManuscritoIPad) {
        let antes = Set(idsYDatos(grupo.trazos).keys)
        let nueva = MotorEdicionManuscritaIPad.duplicar(grupo, sesion: sesion)
        guard !nueva.trazos.isEmpty else { removeFromSuperview(); return }
        publicarNuevos(nueva.trazos, excluyendo: antes)
        finalizarCambio()
    }

    private func pedirColor(_ grupo: GrupoManuscritoIPad) {
        let colores: [(String, UInt32)] = [
            ("Negro", 0xFF1A1A1A), ("Azul", 0xFF1B5FA8),
            ("Rojo", 0xFFC62828), ("Verde", 0xFF2E7D32),
            ("Naranja", 0xFFF57C00),
        ]
        let hoja = UIAlertController(title: "Recolorear \(nivel.rawValue)",
                                     message: nil, preferredStyle: .actionSheet)
        for (nombre, color) in colores {
            hoja.addAction(UIAlertAction(title: nombre, style: .default) {
                [weak self] _ in
                guard let self else { return }
                self.aplicarCambio(grupo: grupo) {
                    self.sesion.recolorearTrazos(grupo.trazos, color: color)
                }
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel) {
            [weak self] _ in self?.removeFromSuperview()
        })
        hoja.popoverPresentationController?.sourceView = self
        hoja.popoverPresentationController?.sourceRect = capaSeleccion.path?.boundingBox ?? bounds
        presentarCuandoLibre(hoja)
    }

    private func eliminar(_ grupo: GrupoManuscritoIPad) {
        aplicarCambio(grupo: grupo) {
            MotorEdicionManuscritaIPad.eliminar(grupo, sesion: sesion)
        }
    }

    private func aplicarCambio(grupo: GrupoManuscritoIPad,
                               operacion: () -> Bool) {
        let antes = idsYDatos(grupo.trazos)
        guard operacion() else { removeFromSuperview(); return }
        publicarDiferencia(antes: antes)
        finalizarCambio()
    }

    private func finalizarCambio() {
        navegador?.lienzo.rehornearYDibujar()
        navegador?.lienzo.alCambiarContenidoSeleccion?()
        alCambiar()
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
        removeFromSuperview()
    }

    private func idsYDatos(_ indices: [Int]) -> [String: Data] {
        var salida: [String: Data] = [:]
        for indice in indices {
            guard let id = sesion.asegurarIdTrazo(indice),
                  let datos = sesion.serializarTrazo(indice) else { continue }
            salida[id] = datos
        }
        return salida
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
            presentador.present(controlador, animated: true)
            return
        }
        guard intento < 8 else {
            removeFromSuperview(); return
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) { [weak self] in
            self?.presentarCuandoLibre(controlador, intento: intento + 1)
        }
    }

    private func presentadorSuperior() -> UIViewController? {
        guard let base = presentador else { return nil }
        var actual = base
        while let siguiente = actual.presentedViewController { actual = siguiente }
        if let nav = actual as? UINavigationController,
           let visible = nav.visibleViewController { return visible }
        return actual
    }
}
