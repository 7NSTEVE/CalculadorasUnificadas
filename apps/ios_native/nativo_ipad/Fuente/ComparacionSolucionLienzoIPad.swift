import PDFKit
import UIKit

struct EventoComparacionSolucionIPad: Equatable {
    var documentoId: UInt64
    var frameId: UUID
    var problemaId: UUID
    var intentoId: UUID
    var region: CGRect
}

enum CanalComparacionSolucionIPad {
    static let nombre = Notification.Name("CuadernU.comparacionSolucionPostIntento")
    private static let documento = "documento"
    private static let frame = "frame"
    private static let problema = "problema"
    private static let intento = "intento"
    private static let region = "region"

    static func mostrar(
        documentoId: UInt64,
        frameId: UUID,
        problemaId: UUID,
        intentoId: UUID,
        region: CGRect,
        centro: NotificationCenter = .default
    ) {
        centro.post(
            name: nombre,
            object: nil,
            userInfo: [
                documento: NSNumber(value: documentoId),
                frame: frameId.uuidString,
                problema: problemaId.uuidString,
                intento: intentoId.uuidString,
                region: NSValue(cgRect: region),
            ])
    }

    static func ocultar(
        documentoId: UInt64,
        centro: NotificationCenter = .default
    ) {
        centro.post(
            name: nombre,
            object: nil,
            userInfo: [documento: NSNumber(value: documentoId)])
    }

    static func evento(de notificacion: Notification) -> EventoComparacionSolucionIPad? {
        guard notificacion.name == nombre,
              let documentoNumero = notificacion.userInfo?[documento] as? NSNumber,
              let frameTexto = notificacion.userInfo?[frame] as? String,
              let problemaTexto = notificacion.userInfo?[problema] as? String,
              let intentoTexto = notificacion.userInfo?[intento] as? String,
              let frameId = UUID(uuidString: frameTexto),
              let problemaId = UUID(uuidString: problemaTexto),
              let intentoId = UUID(uuidString: intentoTexto),
              let region = (notificacion.userInfo?[region] as? NSValue)?.cgRectValue else {
            return nil
        }
        return EventoComparacionSolucionIPad(
            documentoId: documentoNumero.uint64Value,
            frameId: frameId,
            problemaId: problemaId,
            intentoId: intentoId,
            region: region)
    }

    static func esOcultar(_ notificacion: Notification, documentoId: UInt64) -> Bool {
        guard notificacion.name == nombre,
              let numero = notificacion.userInfo?[documento] as? NSNumber,
              numero.uint64Value == documentoId else { return false }
        return notificacion.userInfo?[frame] == nil
    }
}

/// Panel flotante de comparación. Vive encima del viewport, no dentro de
/// `contenido`, de modo que no se exporta ni modifica la tinta. Ocupa solo el
/// lateral derecho; el resto de la hoja continúa disponible para lápiz del iPad.
final class VistaComparacionSolucionIPad: UIVisualEffectView, UIScrollViewDelegate {
    var alCerrar: (() -> Void)?

    private let titulo = UILabel()
    private let detalle = UILabel()
    private let cerrar = UIButton(type: .system)
    private let contenido = UIView()
    private let selector = UISegmentedControl(items: ["Coincide", "Diferencia", "Duda"])
    private let paso = UITextField()
    private let correccion = UITextField()
    private let nota = UITextField()
    private let guardar = UIButton(type: .system)
    private let estado = UILabel()
    private var vistaContenido: UIView?
    private var contexto: EventoComparacionSolucionIPad?
    private var problemaActual: ProblemaBancoIPad?

    init() {
        super.init(effect: UIBlurEffect(style: .systemChromeMaterial))
        configurar()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        effect = UIBlurEffect(style: .systemChromeMaterial)
        configurar()
    }

    private func configurar() {
        clipsToBounds = true
        layer.cornerRadius = 18
        layer.borderWidth = 1 / UIScreen.main.scale
        layer.borderColor = UIColor.separator.cgColor
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOpacity = 0.18
        layer.shadowRadius = 18
        layer.shadowOffset = CGSize(width: 0, height: 8)
        accessibilityIdentifier = "estudio.comparacion.solucion"

        titulo.text = "Solución · post-intento"
        titulo.font = .preferredFont(forTextStyle: .headline)
        titulo.adjustsFontForContentSizeCategory = true

        detalle.font = .preferredFont(forTextStyle: .caption1)
        detalle.textColor = .secondaryLabel
        detalle.numberOfLines = 2

        cerrar.setImage(UIImage(systemName: "xmark.circle.fill"), for: .normal)
        cerrar.accessibilityLabel = "Cerrar comparación"
        cerrar.addTarget(self, action: #selector(cerrarPanel), for: .touchUpInside)

        contenido.backgroundColor = UIColor.secondarySystemBackground.withAlphaComponent(0.6)
        contenido.layer.cornerRadius = 12
        contenido.clipsToBounds = true
        contenido.translatesAutoresizingMaskIntoConstraints = false

        selector.selectedSegmentIndex = 0
        selector.addTarget(self, action: #selector(cambiarResultado), for: .valueChanged)

        paso.placeholder = "Paso donde me desvié"
        correccion.placeholder = "Corrección que debo recordar"
        nota.placeholder = "Nota opcional"
        for campo in [paso, correccion, nota] {
            campo.borderStyle = .roundedRect
            campo.clearButtonMode = .whileEditing
        }

        guardar.configuration = .filled()
        guardar.configuration?.title = "Guardar revisión"
        guardar.addTarget(self, action: #selector(guardarRevision), for: .touchUpInside)

        estado.font = .preferredFont(forTextStyle: .footnote)
        estado.textColor = .secondaryLabel
        estado.numberOfLines = 0

        let encabezado = UIStackView(arrangedSubviews: [titulo, UIView(), cerrar])
        encabezado.axis = .horizontal
        encabezado.alignment = .center
        encabezado.spacing = 8

        let formulario = UIStackView(arrangedSubviews: [
            selector, paso, correccion, nota, guardar, estado,
        ])
        formulario.axis = .vertical
        formulario.spacing = 8

        let pila = UIStackView(arrangedSubviews: [encabezado, detalle, contenido, formulario])
        pila.axis = .vertical
        pila.spacing = 10
        pila.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 12),
            pila.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -12),
            pila.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 12),
            pila.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -12),
            contenido.heightAnchor.constraint(greaterThanOrEqualToConstant: 220),
        ])
        isHidden = true
        cambiarResultado()
    }

    func mostrar(
        evento: EventoComparacionSolucionIPad,
        problema: ProblemaBancoIPad,
        revisionPrevia: RevisionSolucionPostIntentoIPad?
    ) {
        contexto = evento
        problemaActual = problema
        detalle.text = problema.tema.isEmpty
            ? "Compare con su hoja; la tinta permanece editable."
            : "\(problema.tema) · compare con su hoja; la tinta permanece editable."
        cargarSolucion(problema.solucion)
        if let revisionPrevia {
            selector.selectedSegmentIndex = indice(revisionPrevia.resultado)
            paso.text = revisionPrevia.paso
            correccion.text = revisionPrevia.correccion
            nota.text = revisionPrevia.nota
        } else {
            selector.selectedSegmentIndex = 0
            paso.text = nil
            correccion.text = nil
            nota.text = nil
        }
        estado.text = "La solución solo se habilita después de cerrar el intento."
        cambiarResultado()
        isHidden = false
    }

    func ocultar() {
        contexto = nil
        problemaActual = nil
        vistaContenido?.removeFromSuperview()
        vistaContenido = nil
        isHidden = true
        endEditing(true)
    }

    private func cargarSolucion(_ solucion: SolucionVinculadaIPad?) {
        vistaContenido?.removeFromSuperview()
        vistaContenido = nil
        guard let solucion else {
            mostrarTexto("No hay solución vinculada.")
            return
        }

        let texto = solucion.texto.trimmingCharacters(in: .whitespacesAndNewlines)
        if !texto.isEmpty {
            mostrarTexto(texto)
            return
        }

        let hash = solucion.hashAdjunto ?? solucion.fuente?.hashArchivo
        guard let hash,
              let url = RepositorioAdjuntos.compartido.url(hash: hash) else {
            mostrarTexto("La solución está vinculada, pero su archivo no está disponible localmente.")
            return
        }

        if url.pathExtension.lowercased() == "pdf",
           let documento = PDFDocument(url: url) {
            let pdf = PDFView()
            pdf.document = documento
            pdf.autoScales = true
            pdf.displayMode = .singlePageContinuous
            pdf.backgroundColor = .clear
            instalarContenido(pdf)
            if let pagina = solucion.fuente?.pagina,
               pagina >= 0, pagina < documento.pageCount,
               let destino = documento.page(at: pagina) {
                pdf.go(to: destino)
            }
            return
        }

        if let imagen = UIImage(contentsOfFile: url.path) {
            let scroll = UIScrollView()
            scroll.minimumZoomScale = 1
            scroll.maximumZoomScale = 6
            scroll.delegate = self
            let vista = UIImageView(image: imagen)
            vista.tag = 7_301
            vista.contentMode = .scaleAspectFit
            vista.translatesAutoresizingMaskIntoConstraints = false
            scroll.addSubview(vista)
            NSLayoutConstraint.activate([
                vista.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor),
                vista.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor),
                vista.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor),
                vista.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor),
                vista.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor),
                vista.heightAnchor.constraint(equalTo: scroll.frameLayoutGuide.heightAnchor),
            ])
            instalarContenido(scroll)
            return
        }

        mostrarTexto("El formato de la solución vinculada todavía no tiene previsualizador integrado.")
    }

    private func mostrarTexto(_ texto: String) {
        let vista = UITextView()
        vista.isEditable = false
        vista.font = .preferredFont(forTextStyle: .body)
        vista.backgroundColor = .clear
        vista.text = texto
        instalarContenido(vista)
    }

    private func instalarContenido(_ vista: UIView) {
        vistaContenido = vista
        vista.translatesAutoresizingMaskIntoConstraints = false
        contenido.addSubview(vista)
        NSLayoutConstraint.activate([
            vista.leadingAnchor.constraint(equalTo: contenido.leadingAnchor),
            vista.trailingAnchor.constraint(equalTo: contenido.trailingAnchor),
            vista.topAnchor.constraint(equalTo: contenido.topAnchor),
            vista.bottomAnchor.constraint(equalTo: contenido.bottomAnchor),
        ])
    }

    func viewForZooming(in scrollView: UIScrollView) -> UIView? {
        scrollView.viewWithTag(7_301)
    }

    @objc private func cambiarResultado() {
        let diferencia = selector.selectedSegmentIndex == 1
        let duda = selector.selectedSegmentIndex == 2
        paso.isHidden = !(diferencia || duda)
        correccion.isHidden = !diferencia
        paso.placeholder = duda ? "Punto exacto que no entendí" : "Paso donde me desvié"
        nota.placeholder = duda
            ? "Pregunta que quiero llevar al profesor"
            : "Nota opcional"
    }

    @objc private func guardarRevision() {
        guard let contexto else { return }
        let resultado: ResultadoRevisionSolucionIPad
        switch selector.selectedSegmentIndex {
        case 1: resultado = .diferencia
        case 2: resultado = .duda
        default: resultado = .coincide
        }
        let entrada = RevisionSolucionPostIntentoIPad(
            documentoId: contexto.documentoId,
            frameId: contexto.frameId,
            problemaId: contexto.problemaId,
            intentoId: contexto.intentoId,
            resultado: resultado,
            paso: paso.text ?? "",
            correccion: correccion.text ?? "",
            nota: nota.text ?? "")
        let guardado = ServicioRevisionSolucionPostIntentoIPad.guardar(entrada)
        guard guardado != .noSePudoGuardar else {
            estado.text = "No se pudo confirmar la revisión en disco."
            return
        }

        if resultado == .duda {
            switch SincronizadorDudaRevisionSolucionIPad.sincronizar(entrada) {
            case .creada:
                estado.text = "Revisión guardada y duda añadida a Office Hours con el contexto del intento."
            case .yaExistia:
                estado.text = "Revisión guardada; esta duda ya estaba en Office Hours."
            case .problemaNoDisponible:
                estado.text = "Revisión guardada, pero el problema ya no está disponible para enlazar la duda."
            case .noEsDuda:
                estado.text = "Revisión guardada."
            }
            return
        }

        switch guardado {
        case .guardada:
            estado.text = "Revisión guardada."
        case .guardadaYReflejadaComoError:
            estado.text = "Revisión guardada y diferencia añadida a sus errores de práctica."
        case .noSePudoGuardar:
            break
        }
    }

    private func indice(_ resultado: ResultadoRevisionSolucionIPad) -> Int {
        switch resultado {
        case .coincide: return 0
        case .diferencia: return 1
        case .duda: return 2
        }
    }

    @objc private func cerrarPanel() { alCerrar?() }
}
