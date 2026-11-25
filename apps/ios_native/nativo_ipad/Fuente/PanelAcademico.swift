import AVFAudio
import UIKit

private enum SeccionCentro: Int, CaseIterable {
    case hoy, bandeja, tareas, repaso, errores, buscar
    var nombre: String { ["Hoy", "Bandeja", "Tareas", "Repaso", "Errores", "Buscar"][rawValue] }
}

final class PanelAcademico: UITableViewController, UISearchResultsUpdating {
    var alRestaurarEstado: ((EstadoSesionExacto) -> Void)?
    var alAbrirReferencia: ((ReferenciaFuente) -> Void)?
    var alIniciarClase: ((SesionClase) -> Void)?
    private let centro = CentroAcademico.compartido
    private let documentoId: UInt64
    private var seccion: SeccionCentro = .hoy
    private var datos = BaseAcademica()
    private var resumen = ResumenHoy(siguiente: nil, tareasVencidas: [], tareasHoy: [],
                                     tarjetasPendientes: [], erroresPendientes: [],
                                     sesionesSinRevisar: [])
    private var resultados: [ResultadoConFuente] = []
    private let selector = UISegmentedControl(items: SeccionCentro.allCases.map(\.nombre))

    init(documentoId: UInt64) {
        self.documentoId = documentoId
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Estudio"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        let herramientas = UIBarButtonItem(
            image: UIImage(systemName: "ellipsis.circle"),
            menu: UIMenu(children: [
                UIAction(title: "Semestres y continuidad",
                         image: UIImage(systemName: "archivebox")) { [weak self] _ in
                    self?.abrirContinuidad()
                },
                 UIAction(title: "OCR y diccionario técnico",
                          image: UIImage(systemName: "text.magnifyingglass")) { [weak self] _ in
                     self?.abrirDiccionario()
                 },
                 UIAction(title: "Revisión semanal",
                          image: UIImage(systemName: "calendar.badge.checkmark")) {
                     [weak self] _ in self?.abrirRevisionSemanal()
                 },
            ]))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(barButtonSystemItem: .add, target: self,
                            action: #selector(anadir)),
            UIBarButtonItem(title: "Foco", style: .plain, target: self,
                            action: #selector(abrirFoco)),
            herramientas,
        ]
        selector.selectedSegmentIndex = 0
        selector.addTarget(self, action: #selector(cambiarSeccion), for: .valueChanged)
        selector.frame = CGRect(x: 0, y: 0, width: 620, height: 42)
        tableView.tableHeaderView = selector
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "celda")
        refrescar()
    }

    private func refrescar() {
        datos = centro.instantanea()
        resumen = centro.hoy()
        tableView.reloadData()
    }

    @objc private func cambiarSeccion() {
        seccion = SeccionCentro(rawValue: selector.selectedSegmentIndex) ?? .hoy
        if seccion == .buscar {
            let buscador = UISearchController(searchResultsController: nil)
            buscador.searchResultsUpdater = self
            buscador.obscuresBackgroundDuringPresentation = false
            buscador.searchBar.placeholder = "Texto, OCR, fórmula…  RLC in:EL2113"
            navigationItem.searchController = buscador
        } else {
            navigationItem.searchController = nil
        }
        refrescar()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 1 }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        guard seccion == .buscar else { return nil }
        return "La búsqueda global incluye semestres archivados. Use in:CÓDIGO o in:\"Nombre del curso\" para limitar el contexto; acepta alias bilingües y confusiones manuscritas frecuentes."
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch seccion {
        case .hoy: return 6
        case .bandeja: return max(datos.bandeja.filter { !$0.procesada }.count, 1)
        case .tareas: return max(datos.tareas.filter { $0.estado != .archivada }.count, 1)
        case .repaso: return max(resumen.tarjetasPendientes.count, 1)
        case .errores: return max(datos.errores.filter { !$0.resuelto }.count, 1)
        case .buscar: return max(resultados.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = tableView.dequeueReusableCell(withIdentifier: "celda", for: indexPath)
        var contenido = celda.defaultContentConfiguration()
        contenido.secondaryTextProperties.numberOfLines = 3
        celda.accessoryType = .disclosureIndicator
        switch seccion {
        case .hoy:
            configurarHoy(indexPath.row, contenido: &contenido, celda: celda)
        case .bandeja:
            let items = datos.bandeja.filter { !$0.procesada }
            if items.isEmpty { contenido.text = "Bandeja vacía"; celda.accessoryType = .none }
            else {
                let item = items[indexPath.row]
                contenido.text = item.contenido
                contenido.secondaryText = "\(item.tipo.rawValue) · \(FormateadorUI.relativo.localizedString(for: item.fecha, relativeTo: Date()))"
            }
        case .tareas:
            let items = datos.tareas.filter { $0.estado != .archivada }
            if items.isEmpty { contenido.text = "No hay tareas"; celda.accessoryType = .none }
            else {
                let item = items[indexPath.row]
                contenido.text = item.titulo
                contenido.secondaryText = "\(item.estado.rawValue) · \(item.estimadoMinutos) min"
                if item.fechaLimite != nil { contenido.secondaryText! += " · tiene fecha límite" }
            }
        case .repaso:
            if resumen.tarjetasPendientes.isEmpty {
                contenido.text = "No hay tarjetas vencidas"; celda.accessoryType = .none
            } else {
                let item = resumen.tarjetasPendientes[indexPath.row]
                contenido.text = item.pregunta
                contenido.secondaryText = "Fallos: \(item.fallos) · confianza \(Int(item.confianza * 100))%"
            }
        case .errores:
            let items = datos.errores.filter { !$0.resuelto }
            if items.isEmpty { contenido.text = "Error Book al día"; celda.accessoryType = .none }
            else {
                let item = items[indexPath.row]
                contenido.text = item.enunciado
                contenido.secondaryText = "Repetido \(item.repeticiones) vez/veces · \(item.causa)"
            }
        case .buscar:
            if resultados.isEmpty {
                contenido.text = "Busque en todo el cuaderno"; celda.accessoryType = .none
            } else {
                let item = resultados[indexPath.row]
                contenido.text = item.fuente.titulo
                contenido.secondaryText = item.texto
            }
        }
        celda.contentConfiguration = contenido
        return celda
    }

    private func configurarHoy(_ fila: Int, contenido: inout UIListContentConfiguration,
                               celda: UITableViewCell) {
        switch fila {
        case 0:
            contenido.text = "¿Qué hago ahora?"
            contenido.secondaryText = resumen.siguiente?.titulo
                ?? "No hay una próxima acción elegible. Capture lo que tenga pendiente."
            contenido.image = UIImage(systemName: "scope")
        case 1:
            contenido.text = "Solo empiece 5 minutos"
            contenido.secondaryText = resumen.siguiente?.pasos.first(where: { !$0.completado })?.titulo
                ?? "Abra el material que más le preocupa."
            contenido.image = UIImage(systemName: "timer")
        case 2:
            contenido.text = "Repaso real"
            contenido.secondaryText = "\(resumen.tarjetasPendientes.count) tarjetas · \(resumen.erroresPendientes.count) errores"
            contenido.image = UIImage(systemName: "brain.head.profile")
        case 3:
            contenido.text = "Clase ahora"
            contenido.secondaryText = centro.cursoEnHorario()?.nombre
                ?? "Crear sesión sin curso y ordenar después"
            contenido.image = UIImage(systemName: "record.circle")
        case 4:
            contenido.text = "Continuar exactamente donde estaba"
            contenido.secondaryText = centro.estado(documentoId: documentoId) == nil
                ? "Todavía no hay estado guardado" : "Restaura zoom, posición, herramienta y contexto"
            contenido.image = UIImage(systemName: "arrow.uturn.forward.circle")
            if centro.estado(documentoId: documentoId) == nil { celda.accessoryType = .none }
        default:
            contenido.text = "Material no revisado"
            contenido.secondaryText = "\(centro.materialNoRevisado().count) sesiones · \(centro.elementosHuerfanos().count) capturas huérfanas"
            contenido.image = UIImage(systemName: "tray.full")
        }
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch seccion {
        case .hoy: seleccionarHoy(indexPath.row)
        case .bandeja:
            let items = datos.bandeja.filter { !$0.procesada }
            if indexPath.row < items.count { organizar(items[indexPath.row]) }
        case .tareas:
            let items = datos.tareas.filter { $0.estado != .archivada }
            if indexPath.row < items.count { editar(items[indexPath.row]) }
        case .repaso:
            if indexPath.row < resumen.tarjetasPendientes.count {
                abrirRepaso(resumen.tarjetasPendientes[indexPath.row])
            }
        case .errores:
            let items = datos.errores.filter { !$0.resuelto }
            if indexPath.row < items.count { abrirError(items[indexPath.row]) }
        case .buscar:
            if indexPath.row < resultados.count {
                alAbrirReferencia?(resultados[indexPath.row].fuente)
            }
        }
    }

    private func seleccionarHoy(_ fila: Int) {
        switch fila {
        case 0: if let tarea = resumen.siguiente { editar(tarea) }
        case 1:
            let foco = ControladorFoco(objetivo: resumen.siguiente?.titulo ?? "Solo empezar",
                                       minutos: 5, tareaId: resumen.siguiente?.id)
            present(UINavigationController(rootViewController: foco), animated: true)
        case 2:
            if let tarjeta = resumen.tarjetasPendientes.first { abrirRepaso(tarjeta) }
            else if let error = resumen.erroresPendientes.first { abrirError(error) }
        case 3:
            let sesion = centro.iniciarClase(documentoId: documentoId)
            alIniciarClase?(sesion)
            avisar("Sesión iniciada: \(sesion.titulo)")
        case 4:
            if let estado = centro.estado(documentoId: documentoId) {
                alRestaurarEstado?(estado); dismiss(animated: true)
            }
        default:
            seccion = .bandeja; selector.selectedSegmentIndex = seccion.rawValue; refrescar()
        }
    }

    private func editar(_ tarea: TareaAcademica) {
        let hoja = UIAlertController(title: tarea.titulo, message: tarea.detalle,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Empezar", style: .default) { [weak self] _ in
            self?.centro.transaccion { base in
                if let i = base.tareas.firstIndex(where: { $0.id == tarea.id }) {
                    base.tareas[i].estado = .enCurso
                }
            }
            let foco = ControladorFoco(objetivo: tarea.titulo,
                                       minutos: min(max(tarea.estimadoMinutos, 5), 50),
                                       tareaId: tarea.id)
            self?.present(UINavigationController(rootViewController: foco), animated: true)
        })
        hoja.addAction(UIAlertAction(title: "Completar", style: .default) { [weak self] _ in
            self?.centro.transaccion { base in
                if let i = base.tareas.firstIndex(where: { $0.id == tarea.id }) {
                    base.tareas[i].estado = .completada
                }
            }
            self?.refrescar()
        })
        if let referencia = tarea.referencia {
            hoja.addAction(UIAlertAction(title: "Abrir origen", style: .default) { [weak self] _ in
                self?.alAbrirReferencia?(referencia)
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func organizar(_ captura: CapturaBandeja) {
        let hoja = UIAlertController(title: "Organizar captura", message: captura.contenido,
                                     preferredStyle: .actionSheet)
        for curso in datos.cursos.filter({ !$0.archivado }) {
            hoja.addAction(UIAlertAction(title: curso.nombre, style: .default) { [weak self] _ in
                self?.centro.transaccion { base in
                    if let i = base.bandeja.firstIndex(where: { $0.id == captura.id }) {
                        base.bandeja[i].cursoConfirmado = curso.id
                        base.bandeja[i].procesada = true
                    }
                }
                self?.refrescar()
            })
        }
        hoja.addAction(UIAlertAction(title: "Convertir en tarea", style: .default) { [weak self] _ in
            self?.centro.transaccion { base in
                base.tareas.append(TareaAcademica(titulo: captura.contenido,
                                                  cursoId: captura.cursoSugerido))
                if let i = base.bandeja.firstIndex(where: { $0.id == captura.id }) {
                    base.bandeja[i].procesada = true
                }
            }
            self?.refrescar()
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func abrirRepaso(_ tarjeta: FlashcardAcademica) {
        let controlador = ControladorRepaso(tarjeta: tarjeta)
        controlador.alTerminar = { [weak self] in self?.refrescar() }
        present(UINavigationController(rootViewController: controlador), animated: true)
    }

    private func abrirError(_ error: ErrorAprendizaje) {
        let controlador = ControladorError(error: error)
        controlador.alTerminar = { [weak self] in self?.refrescar() }
        present(UINavigationController(rootViewController: controlador), animated: true)
    }

    @objc private func anadir() {
        let alerta = UIAlertController(title: "Capturar primero", message: "Se ordena después.",
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Tarea, idea, duda, enlace…" }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let texto = alerta.textFields?.first?.text, !texto.isEmpty else { return }
            _ = self?.centro.capturar(texto, tipo: .texto)
            self?.refrescar()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func abrirFoco() {
        let foco = ControladorFoco(objetivo: resumen.siguiente?.titulo ?? "Objetivo de esta sesión",
                                   minutos: 25, tareaId: resumen.siguiente?.id)
        present(UINavigationController(rootViewController: foco), animated: true)
    }

    private func abrirContinuidad() {
        let panel = PanelContinuidadAcademicaIPad()
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func abrirDiccionario() {
        let panel = PanelDiccionarioTecnicoIPad()
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func abrirRevisionSemanal() {
        let panel = PanelRevisionSemanalIPad(style: .insetGrouped)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    func updateSearchResults(for searchController: UISearchController) {
        resultados = centro.buscar(searchController.searchBar.text ?? "")
        tableView.reloadData()
    }

    @objc private func cerrar() { dismiss(animated: true) }
    private func avisar(_ texto: String) {
        let a = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Aceptar", style: .default)); present(a, animated: true)
    }
}

final class ControladorRepaso: UIViewController {
    var alTerminar: (() -> Void)?
    private let tarjeta: FlashcardAcademica
    private let respuesta = UILabel()
    private let calidad = UISegmentedControl(items: ["Otra vez", "Difícil", "Bien", "Fácil"])
    private let confianza = UISlider()
    private var inicio = Date()
    private weak var vistaReverso: UIView?

    init(tarjeta: FlashcardAcademica) { self.tarjeta = tarjeta; super.init(nibName: nil, bundle: nil) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Active recall"; view.backgroundColor = .systemBackground
        let pregunta = UILabel(); pregunta.text = tarjeta.pregunta
        pregunta.font = .preferredFont(forTextStyle: .title2); pregunta.numberOfLines = 0
        respuesta.text = tarjeta.respuesta; respuesta.numberOfLines = 0; respuesta.isHidden = true
        calidad.selectedSegmentIndex = 2
        confianza.minimumValue = 0; confianza.maximumValue = 1; confianza.value = 0.5
        let revelar = UIButton(type: .system); revelar.configuration = .filled()
        revelar.configuration?.title = "Mostrar respuesta"
        revelar.addTarget(self, action: #selector(mostrar), for: .touchUpInside)
        let enviar = UIButton(type: .system); enviar.configuration = .borderedProminent()
        enviar.configuration?.title = "Registrar"
        enviar.addTarget(self, action: #selector(registrar), for: .touchUpInside)
        var contenido: [UIView] = [pregunta]
        if let manuscrita = RepositorioTarjetasManuscritasIPad.compartido
            .tarjeta(flashcardId: tarjeta.id) {
            let frente = VistaCaraTarjetaManuscritaIPad(
                imagen: RepositorioTarjetasManuscritasIPad.compartido
                    .imagen(cara: manuscrita.frente),
                titulo: "Frente · toque Mostrar respuesta cuando haya intentado recordar")
            contenido.append(frente)
            let reverso = VistaCaraTarjetaManuscritaIPad(
                imagen: RepositorioTarjetasManuscritasIPad.compartido
                    .imagen(cara: manuscrita.reverso),
                titulo: "Reverso manuscrito · compare con el origen")
            reverso.isHidden = true
            vistaReverso = reverso
            contenido += [revelar, respuesta, reverso]
        } else {
            contenido += [revelar, respuesta]
        }
        contenido += [calidad, UILabel(textoGrande: "Confianza"), confianza, enviar]
        let pila = UIStackView(arrangedSubviews: contenido)
        pila.axis = .vertical; pila.spacing = 20; pila.translatesAutoresizingMaskIntoConstraints = false
        let desplazamiento = UIScrollView()
        desplazamiento.alwaysBounceVertical = true
        desplazamiento.keyboardDismissMode = .interactive
        desplazamiento.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(desplazamiento)
        desplazamiento.addSubview(pila)
        NSLayoutConstraint.activate([
            desplazamiento.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            desplazamiento.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            desplazamiento.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            desplazamiento.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            pila.leadingAnchor.constraint(
                equalTo: desplazamiento.contentLayoutGuide.leadingAnchor, constant: 28),
            pila.trailingAnchor.constraint(
                equalTo: desplazamiento.contentLayoutGuide.trailingAnchor, constant: -28),
            pila.topAnchor.constraint(
                equalTo: desplazamiento.contentLayoutGuide.topAnchor, constant: 24),
            pila.bottomAnchor.constraint(
                equalTo: desplazamiento.contentLayoutGuide.bottomAnchor, constant: -28),
            pila.widthAnchor.constraint(
                equalTo: desplazamiento.frameLayoutGuide.widthAnchor, constant: -56),
        ])
    }
    @objc private func mostrar() {
        respuesta.isHidden = false
        vistaReverso?.isHidden = false
    }
    @objc private func registrar() {
        let calidades = [1, 3, 4, 5]
        CentroAcademico.compartido.registrarRepaso(
            tarjetaId: tarjeta.id, calidad: calidades[max(calidad.selectedSegmentIndex, 0)],
            confianza: Double(confianza.value), segundos: Date().timeIntervalSince(inicio))
        alTerminar?(); dismiss(animated: true)
    }
}

final class ControladorError: UIViewController {
    var alTerminar: (() -> Void)?
    private let error: ErrorAprendizaje
    private let pista = UILabel()
    private var nivel = 0
    init(error: ErrorAprendizaje) { self.error = error; super.init(nibName: nil, bundle: nil) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Error Book"; view.backgroundColor = .systemBackground
        let enunciado = UILabel(textoGrande: error.enunciado); enunciado.numberOfLines = 0
        pista.numberOfLines = 0; pista.textColor = .secondaryLabel
        let botonPista = UIButton(type: .system); botonPista.configuration = .bordered()
        botonPista.configuration?.title = "Pista gradual"; botonPista.addTarget(
            self, action: #selector(siguientePista), for: .touchUpInside)
        let resuelto = UIButton(type: .system); resuelto.configuration = .borderedProminent()
        resuelto.configuration?.title = "Ya lo puedo explicar"; resuelto.addTarget(
            self, action: #selector(marcarResuelto), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [enunciado, pista, botonPista, resuelto])
        pila.axis = .vertical; pila.spacing = 20; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 28),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -28),
            pila.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])
    }
    @objc private func siguientePista() {
        nivel = min(nivel + 1, 5)
        pista.text = ["", "Pista: revise la causa: \(error.causa)",
                      "Concepto: identifique qué regla aplica.",
                      "Primer paso: escriba los datos y la incógnita.",
                      "Procedimiento: compare cada paso con el origen.",
                      "Solución: \(error.respuestaCorrecta)"][nivel]
    }
    @objc private func marcarResuelto() {
        CentroAcademico.compartido.transaccion { base in
            if let i = base.errores.firstIndex(where: { $0.id == error.id }) {
                base.errores[i].resuelto = true
            }
        }
        alTerminar?(); dismiss(animated: true)
    }
}

final class ControladorFoco: UIViewController {
    private let objetivo: String
    private let minutos: Int
    private let tareaId: UUID?
    private let reloj = UILabel()
    private var timer: Timer?
    private var sesionId: UUID?
    private var terminando = false

    init(objetivo: String, minutos: Int, tareaId: UUID?) {
        self.objetivo = objetivo; self.minutos = minutos; self.tareaId = tareaId
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Una sola cosa"; view.backgroundColor = .systemBackground
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Terminar", style: .done, target: self, action: #selector(terminar))
        let objetivoLabel = UILabel(textoGrande: objetivo); objetivoLabel.numberOfLines = 0
        objetivoLabel.textAlignment = .center
        reloj.font = .monospacedDigitSystemFont(ofSize: 52, weight: .medium)
        reloj.textAlignment = .center
        reloj.adjustsFontForContentSizeCategory = true
        reloj.accessibilityLabel = "Tiempo restante"
        let parking = UIButton(type: .system); parking.configuration = .bordered()
        parking.configuration?.title = "Parking lot: pensé en otra cosa"
        parking.addTarget(self, action: #selector(anotarInterrupcion), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [objetivoLabel, reloj, parking])
        pila.axis = .vertical; pila.spacing = 30; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 32),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -32),
            pila.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])

        sesionId = resolverSesionPersistente()
        timer = Timer.scheduledTimer(timeInterval: 1, target: self,
                                     selector: #selector(actualizar), userInfo: nil, repeats: true)
        actualizar()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        actualizar()
    }

    private func resolverSesionPersistente() -> UUID {
        CentroAcademico.compartido.transaccion { base in
            TimeKeeperSesionFocoIPad.reconciliarExpiradas(&base)
            if let activa = TimeKeeperSesionFocoIPad.activa(en: base),
               coincideConSolicitud(activa) {
                return activa.id
            }
            return TimeKeeperSesionFocoIPad.iniciar(
                en: &base,
                objetivo: objetivo,
                minutos: minutos,
                tareaId: tareaId).id
        }
    }

    private func coincideConSolicitud(_ sesion: SesionFoco) -> Bool {
        if let tareaId { return sesion.tareaId == tareaId }
        let actual = sesion.objetivo.trimmingCharacters(in: .whitespacesAndNewlines)
        let pedido = objetivo.trimmingCharacters(in: .whitespacesAndNewlines)
        return actual.caseInsensitiveCompare(pedido) == .orderedSame
    }

    @objc private func actualizar() {
        guard !terminando, let id = sesionId else { return }
        let base = CentroAcademico.compartido.instantanea()
        guard let sesion = base.foco.first(where: { $0.id == id }), sesion.activa else {
            terminarSinPersistirDeNuevo()
            return
        }
        let restantes = sesion.segundosRestantesTimeKeeper()
        let segundos = max(Int(ceil(restantes)), 0)
        reloj.text = String(format: "%02d:%02d", segundos / 60, segundos % 60)
        reloj.accessibilityValue = "\(segundos / 60) minutos, \(segundos % 60) segundos"
        if restantes <= 0 { terminar() }
    }

    @objc private func anotarInterrupcion() {
        let alerta = UIAlertController(title: "Guardar para después", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Pensamiento o pendiente" }
        alerta.addAction(UIAlertAction(title: "Guardar y volver", style: .default) { _ in
            guard let texto = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines), !texto.isEmpty else { return }
            CentroAcademico.compartido.transaccion { base in
                base.parking.append(PensamientoParking(texto: texto))
                if let id = self.sesionId {
                    _ = TimeKeeperSesionFocoIPad.registrarInterrupcion(id, en: &base)
                }
            }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func terminar() {
        guard !terminando else { return }
        terminando = true
        timer?.invalidate()
        CentroAcademico.compartido.transaccion { base in
            if let id = sesionId {
                _ = TimeKeeperSesionFocoIPad.terminar(id, en: &base)
            }
        }
        dismiss(animated: true)
    }

    private func terminarSinPersistirDeNuevo() {
        guard !terminando else { return }
        terminando = true
        timer?.invalidate()
        dismiss(animated: true)
    }
}

private enum FormateadorUI {
    static let relativo = RelativeDateTimeFormatter()
}

private extension UILabel {
    convenience init(textoGrande: String) {
        self.init(); text = textoGrande; font = .preferredFont(forTextStyle: .title2)
    }
}
