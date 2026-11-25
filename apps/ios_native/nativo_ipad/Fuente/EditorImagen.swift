import UIKit

final class EditorImagen: UIViewController {
    var alGuardar: (() -> Void)?
    private let medio: ElementoMedio
    private let sesion: Sesion
    private let originalVisible: UIImage
    private var ajustes: AjustesEdicionImagen
    private let vistaRecorte: VistaRecorteInteractivo
    private let botonForma = UIButton(type: .system)
    private let eliminarFondo = UISwitch()
    private let blancoTransparente = UISwitch()
    private let negroTransparente = UISwitch()
    private let invertir = UISwitch()
    private let monocromo = UISwitch()
    private let vectorizar = UISwitch()
    private let contraste = UISlider()
    private let enderezar = UISlider()
    private let resolucion = UISegmentedControl(items: ["Original", "2×", "4×"])

    init(medio: ElementoMedio, imagen: UIImage, sesion: Sesion) {
        self.medio = medio
        self.sesion = sesion
        originalVisible = Medios.recortar(imagen, con: medio.recorte)
        ajustes = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
            documento: sesion.documentoPersistenteId, id: medio.id)
        vistaRecorte = VistaRecorteInteractivo(imagen: originalVisible)
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Recorte y mejora"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cancelar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Guardar", style: .done, target: self, action: #selector(guardar))

        let pila = UIStackView()
        pila.axis = .vertical
        pila.spacing = 10
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        vistaRecorte.heightAnchor.constraint(greaterThanOrEqualToConstant: 330).isActive = true
        pila.addArrangedSubview(vistaRecorte)

        botonForma.configuration = .bordered()
        botonForma.configuration?.title = ajustes.forma.nombre
        botonForma.showsMenuAsPrimaryAction = true
        botonForma.menu = UIMenu(children: FormaRecorteImagen.allCases.map { forma in
            UIAction(title: forma.nombre) { [weak self] _ in self?.cambiarForma(forma) }
        })
        pila.addArrangedSubview(fila("Forma o detección", botonForma))

        eliminarFondo.isOn = ajustes.eliminarFondo
        blancoTransparente.isOn = ajustes.blancoTransparente
        negroTransparente.isOn = ajustes.negroTransparente
        invertir.isOn = ajustes.invertir
        monocromo.isOn = ajustes.monocromo
        vectorizar.isOn = ajustes.vectorizar
        for control in [eliminarFondo, blancoTransparente, negroTransparente,
                        invertir, monocromo, vectorizar] {
            control.addTarget(self, action: #selector(actualizarPreview), for: .valueChanged)
        }
        let filtrosA = UIStackView(arrangedSubviews: [
            fila("Eliminar fondo", eliminarFondo), fila("Blanco transparente", blancoTransparente),
            fila("Negro transparente", negroTransparente),
        ])
        filtrosA.axis = .horizontal; filtrosA.distribution = .fillEqually; filtrosA.spacing = 8
        pila.addArrangedSubview(filtrosA)
        let filtrosB = UIStackView(arrangedSubviews: [
            fila("Invertir", invertir), fila("Blanco y negro", monocromo),
            fila("Vectorizar", vectorizar),
        ])
        filtrosB.axis = .horizontal; filtrosB.distribution = .fillEqually; filtrosB.spacing = 8
        pila.addArrangedSubview(filtrosB)

        contraste.minimumValue = 0.5; contraste.maximumValue = 2.5
        contraste.value = ajustes.contraste
        contraste.addTarget(self, action: #selector(actualizarPreview), for: .valueChanged)
        pila.addArrangedSubview(fila("Contraste", contraste))
        enderezar.minimumValue = -15; enderezar.maximumValue = 15
        enderezar.value = ajustes.enderezarGrados
        enderezar.addTarget(self, action: #selector(actualizarPreview), for: .valueChanged)
        pila.addArrangedSubview(fila("Enderezar −15°…15°", enderezar))
        resolucion.selectedSegmentIndex = ajustes.escalaResolucion >= 4 ? 2
            : (ajustes.escalaResolucion >= 2 ? 1 : 0)
        pila.addArrangedSubview(fila("Resolución", resolucion))

        let acciones = UIStackView()
        acciones.axis = .horizontal
        acciones.distribution = .fillEqually
        acciones.spacing = 8
        acciones.addArrangedSubview(boton("OCR local", #selector(ejecutarOCR)))
        acciones.addArrangedSubview(boton("Copiar vista", #selector(copiarVista)))
        acciones.addArrangedSubview(boton("Restaurar", #selector(restaurar)))
        pila.addArrangedSubview(acciones)

        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            pila.bottomAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.bottomAnchor,
                                         constant: -12),
        ])
        vistaRecorte.forma = ajustes.forma
        vistaRecorte.puntos = ajustes.puntosMascara
        actualizarPreview()
    }

    private func cambiarForma(_ forma: FormaRecorteImagen) {
        ajustes.forma = forma
        botonForma.configuration?.title = forma.nombre
        vistaRecorte.forma = forma
        if forma == .cuadrado || forma == .circular { vistaRecorte.hacerCuadrado() }
        if forma.esSemantica {
            botonForma.configuration?.showsActivityIndicator = true
            AnalizadorImagenLocal.detectarRegion(forma, en: originalVisible) { [weak self] resultado in
                DispatchQueue.main.async {
                    guard let self else { return }
                    self.botonForma.configuration?.showsActivityIndicator = false
                    switch resultado {
                    case .success(let caja): self.vistaRecorte.regionNormalizada = caja
                    case .failure(let error): self.avisar(error.localizedDescription)
                    }
                }
            }
        }
    }

    @objc private func actualizarPreview() {
        ajustes.eliminarFondo = eliminarFondo.isOn
        ajustes.blancoTransparente = blancoTransparente.isOn
        ajustes.negroTransparente = negroTransparente.isOn
        ajustes.invertir = invertir.isOn
        ajustes.monocromo = monocromo.isOn
        ajustes.vectorizar = vectorizar.isOn
        ajustes.contraste = contraste.value
        ajustes.enderezarGrados = enderezar.value
        var sinFondo = ajustes
        sinFondo.eliminarFondo = false
        let base = ProcesadorImagenLocal.procesar(originalVisible, ajustes: sinFondo)
        guard ajustes.eliminarFondo, #available(iOS 17.0, *) else {
            vistaRecorte.imagen = base
            return
        }
        AnalizadorImagenLocal.quitarFondo(base) { [weak self] resultado in
            DispatchQueue.main.async {
                if case .success(let salida) = resultado { self?.vistaRecorte.imagen = salida }
            }
        }
    }

    @objc private func ejecutarOCR() {
        AnalizadorImagenLocal.reconocerTexto(originalVisible) { [weak self] resultado in
            DispatchQueue.main.async {
                guard let self else { return }
                switch resultado {
                case .success(let bloques):
                    self.ajustes.ocr = bloques
                    self.vistaRecorte.cajasOCR = bloques.map(\.caja)
                    self.avisar("OCR local: \(bloques.count) bloque(s) vinculados al original.")
                case .failure(let error): self.avisar(error.localizedDescription)
                }
            }
        }
    }

    @objc private func copiarVista() {
        Medios.copiar(vistaRecorte.imagenRecortada())
    }

    @objc private func restaurar() {
        ajustes = AjustesEdicionImagen()
        vistaRecorte.regionNormalizada = CGRect(x: 0, y: 0, width: 1, height: 1)
        vistaRecorte.puntos = []
        vistaRecorte.forma = .rectangular
        _ = sesion.quitarRecorteMedio(id: medio.id)
        _ = RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
            ajustes, documento: sesion.documentoPersistenteId, id: medio.id)
        notificarGuardado()
        dismiss(animated: true)
    }

    @objc private func guardar() {
        var region = vistaRecorte.regionNormalizada.intersection(
            CGRect(x: 0, y: 0, width: 1, height: 1))
        guard region.width > 0.005, region.height > 0.005 else {
            avisar("El recorte es demasiado pequeño."); return
        }
        ajustes.forma = vistaRecorte.forma
        ajustes.puntosMascara = vistaRecorte.puntos
        if !ajustes.puntosMascara.isEmpty {
            let xs = ajustes.puntosMascara.map(\.x)
            let ys = ajustes.puntosMascara.map(\.y)
            if let minX = xs.min(), let maxX = xs.max(),
               let minY = ys.min(), let maxY = ys.max() {
                region = CGRect(x: minX, y: minY, width: maxX - minX, height: maxY - minY)
                ajustes.puntosMascara = ajustes.puntosMascara.map {
                    PuntoNormalizado(x: ($0.x - region.minX) / max(region.width, 0.001),
                                     y: ($0.y - region.minY) / max(region.height, 0.001))
                }
            }
        }
        ajustes.escalaResolucion = Float([1, 2, 4][max(resolucion.selectedSegmentIndex, 0)])
        _ = sesion.recortarMedio(id: medio.id, recorteRelativo: region)
        _ = RepositorioEdicionesImagenDocumentoIPad.compartido.guardar(
            ajustes, documento: sesion.documentoPersistenteId, id: medio.id)
        notificarGuardado()
        dismiss(animated: true)
    }

    private func notificarGuardado() {
        RepositorioEdicionesImagen.compartido.conContexto(
            documento: sesion.documentoPersistenteId, id: medio.id) {
            alGuardar?()
        }
    }

    @objc private func cancelar() { dismiss(animated: true) }

    private func fila(_ texto: String, _ control: UIView) -> UIView {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .caption1)
        etiqueta.numberOfLines = 0
        let pila = UIStackView(arrangedSubviews: [etiqueta, control])
        pila.axis = .vertical
        pila.spacing = 3
        return pila
    }

    private func boton(_ titulo: String, _ selector: Selector) -> UIButton {
        let boton = UIButton(type: .system)
        boton.configuration = .bordered()
        boton.configuration?.title = titulo
        boton.addTarget(self, action: selector, for: .touchUpInside)
        return boton
    }

    private func avisar(_ mensaje: String) {
        let alerta = UIAlertController(title: nil, message: mensaje, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }
}

private final class VistaRecorteInteractivo: UIView {
    var imagen: UIImage { didSet { imagenView.image = imagen } }
    var forma: FormaRecorteImagen = .rectangular { didSet { setNeedsLayout() } }
    var puntos: [PuntoNormalizado] = [] { didSet { setNeedsDisplay() } }
    var cajasOCR: [CGRect] = [] { didSet { setNeedsDisplay() } }
    var regionNormalizada = CGRect(x: 0.05, y: 0.05, width: 0.9, height: 0.9) {
        didSet { colocarMarco() }
    }
    private let imagenView = UIImageView()
    private let marco = UIView()
    private var tiradores: [UIView] = []

    init(imagen: UIImage) {
        self.imagen = imagen
        super.init(frame: .zero)
        backgroundColor = .black
        clipsToBounds = true
        imagenView.image = imagen
        imagenView.contentMode = .scaleAspectFit
        imagenView.frame = bounds
        imagenView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        addSubview(imagenView)
        marco.layer.borderColor = UIColor.systemYellow.cgColor
        marco.layer.borderWidth = 2
        marco.backgroundColor = .clear
        marco.addGestureRecognizer(UIPanGestureRecognizer(target: self,
                                                          action: #selector(moverMarco(_:))))
        addSubview(marco)
        for indice in 0..<4 {
            let tirador = UIView()
            tirador.backgroundColor = .systemYellow
            tirador.layer.cornerRadius = 8
            tirador.tag = indice
            tirador.addGestureRecognizer(UIPanGestureRecognizer(
                target: self, action: #selector(moverEsquina(_:))))
            addSubview(tirador)
            tiradores.append(tirador)
        }
        addGestureRecognizer(UITapGestureRecognizer(target: self,
                                                     action: #selector(agregarPunto(_:))))
        let libre = UIPanGestureRecognizer(target: self, action: #selector(dibujarLibre(_:)))
        libre.minimumNumberOfTouches = 1
        addGestureRecognizer(libre)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func layoutSubviews() {
        super.layoutSubviews()
        colocarMarco()
    }

    private var rectImagen: CGRect {
        let s = imagen.size
        guard s.width > 0, s.height > 0 else { return bounds }
        let escala = min(bounds.width / s.width, bounds.height / s.height)
        let tamano = CGSize(width: s.width * escala, height: s.height * escala)
        return CGRect(x: (bounds.width - tamano.width) / 2,
                      y: (bounds.height - tamano.height) / 2,
                      width: tamano.width, height: tamano.height)
    }

    private func colocarMarco() {
        let r = rectImagen
        marco.frame = CGRect(x: r.minX + regionNormalizada.minX * r.width,
                             y: r.minY + regionNormalizada.minY * r.height,
                             width: regionNormalizada.width * r.width,
                             height: regionNormalizada.height * r.height)
        let posiciones = [marco.frame.origin,
                          CGPoint(x: marco.frame.maxX, y: marco.frame.minY),
                          CGPoint(x: marco.frame.maxX, y: marco.frame.maxY),
                          CGPoint(x: marco.frame.minX, y: marco.frame.maxY)]
        for (vista, punto) in zip(tiradores, posiciones) {
            vista.frame = CGRect(x: punto.x - 8, y: punto.y - 8, width: 16, height: 16)
        }
        setNeedsDisplay()
    }

    func hacerCuadrado() {
        let lado = min(regionNormalizada.width, regionNormalizada.height)
        regionNormalizada = CGRect(x: regionNormalizada.midX - lado / 2,
                                   y: regionNormalizada.midY - lado / 2,
                                   width: lado, height: lado)
    }

    @objc private func moverMarco(_ gesto: UIPanGestureRecognizer) {
        let delta = gesto.translation(in: self)
        gesto.setTranslation(.zero, in: self)
        let r = rectImagen
        var nueva = regionNormalizada.offsetBy(dx: delta.x / r.width,
                                                dy: delta.y / r.height)
        nueva.origin.x = min(max(nueva.origin.x, 0), 1 - nueva.width)
        nueva.origin.y = min(max(nueva.origin.y, 0), 1 - nueva.height)
        regionNormalizada = nueva
    }

    @objc private func moverEsquina(_ gesto: UIPanGestureRecognizer) {
        guard let tirador = gesto.view else { return }
        let punto = gesto.location(in: self)
        let r = rectImagen
        let x = min(max((punto.x - r.minX) / r.width, 0), 1)
        let y = min(max((punto.y - r.minY) / r.height, 0), 1)
        let opuesto: CGPoint
        switch tirador.tag {
        case 0: opuesto = CGPoint(x: regionNormalizada.maxX, y: regionNormalizada.maxY)
        case 1: opuesto = CGPoint(x: regionNormalizada.minX, y: regionNormalizada.maxY)
        case 2: opuesto = CGPoint(x: regionNormalizada.minX, y: regionNormalizada.minY)
        default: opuesto = CGPoint(x: regionNormalizada.maxX, y: regionNormalizada.minY)
        }
        var nueva = CGRect(x: min(x, opuesto.x), y: min(y, opuesto.y),
                           width: abs(x - opuesto.x), height: abs(y - opuesto.y))
        if forma == .cuadrado || forma == .circular {
            let lado = min(nueva.width, nueva.height)
            nueva.size = CGSize(width: lado, height: lado)
        }
        if nueva.width > 0.01, nueva.height > 0.01 { regionNormalizada = nueva }
    }

    @objc private func agregarPunto(_ gesto: UITapGestureRecognizer) {
        guard forma == .poligonal else { return }
        if let p = normalizar(gesto.location(in: self)) { puntos.append(p) }
    }

    @objc private func dibujarLibre(_ gesto: UIPanGestureRecognizer) {
        guard forma == .libre || forma == .lazo else { return }
        if gesto.state == .began { puntos.removeAll() }
        if let p = normalizar(gesto.location(in: self)),
           puntos.last.map({ hypot($0.x - p.x, $0.y - p.y) > 0.006 }) ?? true {
            puntos.append(p)
        }
    }

    private func normalizar(_ punto: CGPoint) -> PuntoNormalizado? {
        let r = rectImagen
        guard r.contains(punto) else { return nil }
        return PuntoNormalizado(x: (punto.x - r.minX) / r.width,
                                y: (punto.y - r.minY) / r.height)
    }

    override func draw(_ rect: CGRect) {
        guard let contexto = UIGraphicsGetCurrentContext() else { return }
        contexto.setFillColor(UIColor.black.withAlphaComponent(0.42).cgColor)
        let exterior = UIBezierPath(rect: rectImagen)
        let interior = UIBezierPath(rect: marco.frame)
        exterior.append(interior)
        exterior.usesEvenOddFillRule = true
        exterior.fill()
        contexto.setStrokeColor(UIColor.systemTeal.cgColor)
        contexto.setLineWidth(1)
        for caja in cajasOCR {
            let r = rectImagen
            contexto.stroke(CGRect(x: r.minX + caja.minX * r.width,
                                   y: r.minY + caja.minY * r.height,
                                   width: caja.width * r.width,
                                   height: caja.height * r.height))
        }
        if puntos.count > 1 {
            let ruta = UIBezierPath()
            for (indice, punto) in puntos.enumerated() {
                let p = CGPoint(x: rectImagen.minX + punto.x * rectImagen.width,
                                y: rectImagen.minY + punto.y * rectImagen.height)
                if indice == 0 { ruta.move(to: p) } else { ruta.addLine(to: p) }
            }
            ruta.close()
            UIColor.systemYellow.setStroke()
            ruta.lineWidth = 2
            ruta.stroke()
        }
    }

    func imagenRecortada() -> UIImage {
        let procesada = imagen
        let r = CGRect(x: regionNormalizada.minX * procesada.size.width,
                       y: regionNormalizada.minY * procesada.size.height,
                       width: regionNormalizada.width * procesada.size.width,
                       height: regionNormalizada.height * procesada.size.height)
        guard let cg = procesada.cgImage?.cropping(to: r) else { return procesada }
        return UIImage(cgImage: cg, scale: procesada.scale,
                       orientation: procesada.imageOrientation)
    }
}
