import UIKit

final class PanelControlPresentacionIPad: UIViewController, UITextViewDelegate {
    var alAnterior: (() -> Void)?
    var alSiguiente: (() -> Void)?
    var alFinalizar: (() -> Void)?
    var alSolicitarPantallaExterna: (() -> Void)?

    private let coordinador: CoordinadorPresentacionIPad
    private let documentoId: DocumentoPersistenteID
    private let estadoLabel = UILabel()
    private let notas = UITextView()
    private let negro = UIButton(type: .system)
    private let puntero = UIButton(type: .system)
    private let externa = UIButton(type: .system)

    init(documentoId: DocumentoPersistenteID,
         coordinador: CoordinadorPresentacionIPad = .compartido) {
        self.documentoId = documentoId
        self.coordinador = coordinador
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Presentador"
        view.backgroundColor = .systemBackground
        let finalizarItem = UIBarButtonItem(
            title: "Finalizar", style: .plain,
            target: self, action: #selector(finalizar))
        finalizarItem.tintColor = .systemRed
        finalizarItem.accessibilityLabel = "Finalizar presentación"
        navigationItem.leftBarButtonItem = finalizarItem

        if coordinador.estado() == nil {
            _ = coordinador.iniciar(documentoId: documentoId)
        }
        do { try coordinador.asegurar(documentoId: documentoId) }
        catch { _ = coordinador.iniciar(documentoId: documentoId) }

        estadoLabel.font = .preferredFont(forTextStyle: .headline)
        estadoLabel.adjustsFontForContentSizeCategory = true
        estadoLabel.numberOfLines = 2
        estadoLabel.accessibilityIdentifier = "presentacion.estado"

        configurar(negro, titulo: "Pantalla negra", simbolo: "rectangle.fill")
        configurar(puntero, titulo: "Puntero", simbolo: "dot.scope")
        configurar(externa, titulo: "Pantalla externa", simbolo: "display")
        negro.addTarget(self, action: #selector(alternarNegro), for: .touchUpInside)
        puntero.addTarget(self, action: #selector(alternarPuntero), for: .touchUpInside)
        externa.addTarget(self, action: #selector(pedirExterna), for: .touchUpInside)

        let anterior = boton("Anterior", "chevron.left", #selector(irAnterior))
        let siguiente = boton("Siguiente", "chevron.right", #selector(irSiguiente))
        let navegacion = UIStackView(arrangedSubviews: [anterior, siguiente])
        navegacion.axis = .horizontal; navegacion.distribution = .fillEqually; navegacion.spacing = 12

        let herramientas = UIStackView(arrangedSubviews: [negro, puntero, externa])
        herramientas.axis = .horizontal; herramientas.distribution = .fillEqually; herramientas.spacing = 8

        let tituloNotas = UILabel()
        tituloNotas.text = "Notas privadas del presentador"
        tituloNotas.font = .preferredFont(forTextStyle: .headline)
        tituloNotas.adjustsFontForContentSizeCategory = true

        notas.font = .preferredFont(forTextStyle: .body)
        notas.adjustsFontForContentSizeCategory = true
        notas.layer.cornerRadius = 10; notas.layer.borderWidth = 1; notas.layer.borderColor = UIColor.separator.cgColor
        notas.delegate = self
        notas.accessibilityLabel = "Notas privadas del presentador"
        notas.accessibilityHint = "Estas notas no forman parte de la superficie de audiencia."
        notas.heightAnchor.constraint(greaterThanOrEqualToConstant: 180).isActive = true

        let ayuda = UILabel()
        ayuda.text = "←/→ navegar · B pantalla negra · L puntero · Esc finalizar"
        ayuda.font = .preferredFont(forTextStyle: .footnote)
        ayuda.textColor = .secondaryLabel
        ayuda.adjustsFontForContentSizeCategory = true
        ayuda.numberOfLines = 0

        let pila = UIStackView(arrangedSubviews: [estadoLabel, navegacion, herramientas, tituloNotas, notas, ayuda])
        pila.axis = .vertical; pila.spacing = 16; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 20),
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 20),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20),
            pila.bottomAnchor.constraint(lessThanOrEqualTo: view.keyboardLayoutGuide.topAnchor, constant: -12),
        ])

        NotificationCenter.default.addObserver(self, selector: #selector(cambioEstado),
                                               name: CoordinadorPresentacionIPad.cambio, object: nil)
        actualizar()
    }

    deinit { NotificationCenter.default.removeObserver(self) }
    override var canBecomeFirstResponder: Bool { true }

    override var keyCommands: [UIKeyCommand]? {
        [
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: [],
                         action: #selector(irAnterior), discoverabilityTitle: "Anterior"),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: [],
                         action: #selector(irSiguiente), discoverabilityTitle: "Siguiente"),
            UIKeyCommand(input: "b", modifierFlags: [], action: #selector(alternarNegro),
                         discoverabilityTitle: "Pantalla negra"),
            UIKeyCommand(input: "l", modifierFlags: [], action: #selector(alternarPuntero),
                         discoverabilityTitle: "Alternar puntero"),
            UIKeyCommand(input: UIKeyCommand.inputEscape, modifierFlags: [], action: #selector(finalizar),
                         discoverabilityTitle: "Finalizar presentación"),
        ]
    }

    func textViewDidChange(_ textView: UITextView) {
        try? coordinador.mutar { $0.notasPrivadas = textView.text }
    }

    @objc private func cambioEstado() { actualizar() }

    private func actualizar() {
        guard let estado = coordinador.estado(), estado.documentoId == documentoId else {
            estadoLabel.text = "Presentación no disponible"; return
        }
        var partes = ["Presentando"]
        if let pagina = estado.pagina { partes.append("página \(pagina + 1)") }
        if estado.pantallaNegra { partes.append("audiencia en negro") }
        estadoLabel.text = partes.joined(separator: " · ")
        if !notas.isFirstResponder { notas.text = estado.notasPrivadas }
        negro.configuration?.baseBackgroundColor = estado.pantallaNegra ? .label : .secondarySystemBackground
        puntero.configuration?.baseBackgroundColor = estado.mostrarPuntero ? .systemRed.withAlphaComponent(0.18) : .secondarySystemBackground
        negro.accessibilityValue = estado.pantallaNegra ? "Activada" : "Desactivada"
        puntero.accessibilityValue = estado.mostrarPuntero ? "Activado" : "Desactivado"
    }

    private func configurar(_ boton: UIButton, titulo: String, simbolo: String) {
        var c = UIButton.Configuration.tinted(); c.title = titulo; c.image = UIImage(systemName: simbolo)
        c.imagePlacement = .top; c.imagePadding = 6; boton.configuration = c
        boton.accessibilityLabel = titulo
        boton.heightAnchor.constraint(greaterThanOrEqualToConstant: 64).isActive = true
    }

    private func boton(_ titulo: String, _ simbolo: String, _ accion: Selector) -> UIButton {
        let boton = UIButton(type: .system); var c = UIButton.Configuration.filled()
        c.title = titulo; c.image = UIImage(systemName: simbolo); c.imagePadding = 8
        boton.configuration = c; boton.addTarget(self, action: accion, for: .touchUpInside)
        boton.accessibilityLabel = titulo
        boton.heightAnchor.constraint(greaterThanOrEqualToConstant: 50).isActive = true
        return boton
    }

    @objc private func irAnterior() { alAnterior?() }
    @objc private func irSiguiente() { alSiguiente?() }
    @objc private func alternarNegro() { _ = try? coordinador.mutar { $0.alternarNegro() } }
    @objc private func alternarPuntero() {
        _ = try? coordinador.mutar { $0.mostrarPuntero.toggle(); if !$0.mostrarPuntero { $0.puntero.visible = false } }
    }
    @objc private func pedirExterna() { alSolicitarPantallaExterna?() }
    @objc private func finalizar() { coordinador.finalizar(); alFinalizar?(); dismiss(animated: true) }
}
