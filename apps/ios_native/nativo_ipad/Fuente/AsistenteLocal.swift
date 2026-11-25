import UIKit

final class AsistenteLocal: UIViewController, UITableViewDataSource, UITableViewDelegate {
    var alAbrirFuente: ((ReferenciaFuente) -> Void)?
    private let centro = CentroAcademico.compartido
    private let consulta = UITextView()
    private let salida = UITextView()
    private let modo = UISegmentedControl(items: ["Solo fuentes", "Fuentes + inferencias marcadas"])
    private let tabla = UITableView(frame: .zero, style: .insetGrouped)
    private var citas: [ReferenciaFuente] = []

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Asistente local con fuentes"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        modo.selectedSegmentIndex = 0
        consulta.font = .preferredFont(forTextStyle: .body)
        consulta.layer.borderWidth = 1; consulta.layer.borderColor = UIColor.separator.cgColor
        consulta.layer.cornerRadius = 8
        consulta.text = "¿Qué quiere entender de sus materiales?"
        consulta.heightAnchor.constraint(equalToConstant: 82).isActive = true
        salida.isEditable = false
        salida.font = .preferredFont(forTextStyle: .body)
        salida.backgroundColor = .secondarySystemBackground
        salida.layer.cornerRadius = 8
        tabla.dataSource = self; tabla.delegate = self
        tabla.heightAnchor.constraint(equalToConstant: 170).isActive = true

        let acciones = UIStackView()
        acciones.axis = .horizontal; acciones.distribution = .fillEqually; acciones.spacing = 6
        acciones.addArrangedSubview(boton("Responder", #selector(responder)))
        acciones.addArrangedSubview(boton("Socrático", #selector(socratico)))
        acciones.addArrangedSubview(boton("¿Lo dijo el profe?", #selector(dijoProfesor)))
        acciones.addArrangedSubview(boton("Contradicciones", #selector(contradicciones)))
        acciones.addArrangedSubview(boton("Índice", #selector(indice)))

        let pila = UIStackView(arrangedSubviews: [consulta, modo, acciones, salida, tabla])
        pila.axis = .vertical; pila.spacing = 12; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            pila.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -12),
        ])
    }

    private func boton(_ titulo: String, _ selector: Selector) -> UIButton {
        let boton = UIButton(type: .system); boton.configuration = .bordered()
        boton.configuration?.title = titulo
        boton.titleLabel?.numberOfLines = 2; boton.addTarget(self, action: selector,
                                                            for: .touchUpInside)
        return boton
    }

    @objc private func responder() {
        mostrar(centro.responder(consulta.text,
            modo: modo.selectedSegmentIndex == 0 ? .estrictoFuentes : .asistidoConInferencias))
    }

    @objc private func socratico() { mostrar(centro.modoSocratico(consulta.text)) }
    @objc private func dijoProfesor() { mostrar(centro.loDijoElProfesor(consulta.text)) }

    @objc private func contradicciones() {
        let conflictos = centro.detectarContradicciones()
        citas = conflictos.flatMap { [$0.fuenteA, $0.fuenteB] }
        salida.text = conflictos.isEmpty
            ? "No se detectaron contradicciones candidatas. Esto no demuestra consistencia completa."
            : conflictos.enumerated().map {
                "\($0.offset + 1). \($0.element.motivo)\nA: \($0.element.afirmacionA)\nB: \($0.element.afirmacionB)"
            }.joined(separator: "\n\n")
        tabla.reloadData()
    }

    @objc private func indice() {
        let entradas = centro.indiceAutomatico()
        citas = entradas.flatMap(\.fuentes)
        salida.text = entradas.isEmpty ? "Todavía no hay material indexado."
            : entradas.map { "• \($0.titulo) (\($0.cantidad))" }.joined(separator: "\n")
        tabla.reloadData()
    }

    private func mostrar(_ respuesta: RespuestaLocal) {
        citas = respuesta.citas
        salida.text = respuesta.texto + (respuesta.advertencias.isEmpty ? ""
            : "\n\nAdvertencias:\n" + respuesta.advertencias.map { "• \($0)" }.joined(separator: "\n"))
        tabla.reloadData()
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        citas.count
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        let cita = citas[indexPath.row]
        celda.textLabel?.text = "[\(indexPath.row + 1)] \(cita.titulo)"
        var detalle = cita.tipo.rawValue
        if let pagina = cita.pagina { detalle += " · pág. \(pagina + 1)" }
        if let segundo = cita.segundoAudio {
            detalle += String(format: " · %02d:%02d", Int(segundo) / 60, Int(segundo) % 60)
        }
        celda.detailTextLabel?.text = detalle
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        alAbrirFuente?(citas[indexPath.row])
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
