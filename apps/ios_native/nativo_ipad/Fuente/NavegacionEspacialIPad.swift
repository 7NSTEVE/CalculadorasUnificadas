import UIKit

enum TipoFrameIPad: String, Codable, CaseIterable {
    case visible = "Región libre"
    case a4 = "A4"
    case carta = "Carta"
    case presentacion = "Presentación 16:9"
}

struct FrameApunteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var region: CGRect
    var tipo: TipoFrameIPad
    var colorARGB: UInt32 = 0xFF0A84FF
    var incluirFondo = true
    var creado = Date()
    /// Opcional para mantener compatibilidad con frames creados por builds
    /// anteriores. Un frame normal sigue siendo únicamente una región.
    var estudio: EstadoEstudioFrameIPad?
}

struct EscenaApunteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var vista: EstadoVistaLienzo
    var frameId: UUID?
    var modoFoco: ModoFocoVisualIPad?
    var creado = Date()
}

struct FavoritoEspacialTemporalIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var region: CGRect
    var vence: Date?
    var colorARGB: UInt32 = 0xFFFF9F0A
    var vigente: Bool { vence.map { $0 > Date() } ?? true }
}

private struct ArchivoNavegacionEspacialIPad: Codable {
    var formato = 1
    var frames: [FrameApunteIPad] = []
    var escenas: [EscenaApunteIPad] = []
    var favoritos: [FavoritoEspacialTemporalIPad] = []
}

final class RepositorioNavegacionEspacialIPad {
    static let compartido = RepositorioNavegacionEspacialIPad()
    private let base: URL
    private var cache: [UInt64: ArchivoNavegacionEspacialIPad] = [:]
    private let cola = DispatchQueue(label: "tinta.navegacion.espacial")

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        base = soporte.appendingPathComponent("Tinta/NavegacionEspacial",
                                              isDirectory: true)
        try? FileManager.default.createDirectory(at: base,
                                                 withIntermediateDirectories: true)
    }

    private func url(_ documento: UInt64) -> URL {
        base.appendingPathComponent("\(documento).json")
    }

    private func archivo(_ documento: UInt64) -> ArchivoNavegacionEspacialIPad {
        if let valor = cache[documento] { return valor }
        let valor = (try? Data(contentsOf: url(documento)))
            .flatMap { try? JSONDecoder().decode(ArchivoNavegacionEspacialIPad.self,
                                                  from: $0) }
            ?? ArchivoNavegacionEspacialIPad()
        cache[documento] = valor
        return valor
    }

    private func persistir(_ archivo: ArchivoNavegacionEspacialIPad,
                           documento: UInt64) throws {
        let datos = try JSONEncoder().encode(archivo)
        try datos.write(to: url(documento),
                        options: [.atomic, .completeFileProtection])
    }

    @discardableResult
    private func guardar(_ archivo: ArchivoNavegacionEspacialIPad,
                         documento: UInt64) -> Bool {
        do {
            try persistir(archivo, documento: documento)
            cache[documento] = archivo
            return true
        } catch {
            // La UI histórica no expone errores de este repositorio. Al menos
            // no adelantamos el caché si el disco no confirmó la escritura.
            return false
        }
    }

    func instantanea(_ documento: UInt64)
        -> (frames: [FrameApunteIPad], escenas: [EscenaApunteIPad],
            favoritos: [FavoritoEspacialTemporalIPad]) {
        cola.sync {
            var valor = archivo(documento)
            valor.favoritos.removeAll { !$0.vigente }
            guardar(valor, documento: documento)
            return (valor.frames, valor.escenas, valor.favoritos)
        }
    }

    func agregar(_ frame: FrameApunteIPad, documento: UInt64) {
        cola.sync {
            var valor = archivo(documento); valor.frames.append(frame)
            guardar(valor, documento: documento)
        }
    }

    func agregar(_ escena: EscenaApunteIPad, documento: UInt64) {
        cola.sync {
            var valor = archivo(documento); valor.escenas.append(escena)
            guardar(valor, documento: documento)
        }
    }

    func agregar(_ favorito: FavoritoEspacialTemporalIPad, documento: UInt64) {
        cola.sync {
            var valor = archivo(documento); valor.favoritos.append(favorito)
            guardar(valor, documento: documento)
        }
    }

    /// Enlaza un frame existente con la identidad canónica del Problem Bank.
    /// El enunciado solo se usa como objetivo visible; la fuente de verdad del
    /// problema sigue siendo `CentroEstudioTecnicoIPad`.
    @discardableResult
    func vincularProblema(
        frameId: UUID,
        documento: UInt64,
        problemaId: UUID,
        objetivo: String
    ) -> Bool {
        cola.sync {
            var valor = archivo(documento)
            guard let indice = valor.frames.firstIndex(where: { $0.id == frameId }),
                  var estudio = valor.frames[indice].estudio else { return false }
            estudio.problemaId = problemaId
            if !objetivo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                estudio.objetivo = objetivo.trimmingCharacters(in: .whitespacesAndNewlines)
            }
            valor.frames[indice].estudio = estudio
            return guardar(valor, documento: documento)
        }
    }

    @discardableResult
    func iniciarIntento(frameId: UUID, documento: UInt64) -> UUID? {
        cola.sync {
            var valor = archivo(documento)
            guard let indice = valor.frames.firstIndex(where: { $0.id == frameId }),
                  var estudio = valor.frames[indice].estudio else { return nil }
            let intentoId = estudio.iniciar()
            valor.frames[indice].estudio = estudio
            return guardar(valor, documento: documento) ? intentoId : nil
        }
    }

    @discardableResult
    func cerrarIntento(
        frameId: UUID,
        documento: UInt64,
        resultado: ResultadoIntentoFrameIPad,
        confianza: Double,
        error: String = ""
    ) -> Bool {
        cola.sync {
            var valor = archivo(documento)
            guard let indice = valor.frames.firstIndex(where: { $0.id == frameId }),
                  var estudio = valor.frames[indice].estudio,
                  let intentoId = estudio.intentoActivo?.id,
                  estudio.cerrar(
                    intentoId: intentoId,
                    resultado: resultado,
                    confianza: confianza,
                    error: error) else { return false }
            valor.frames[indice].estudio = estudio
            return guardar(valor, documento: documento)
        }
    }

    func eliminar(_ id: UUID, documento: UInt64) {
        cola.sync {
            var valor = archivo(documento)
            valor.frames.removeAll { $0.id == id }
            valor.escenas.removeAll { $0.id == id }
            valor.favoritos.removeAll { $0.id == id }
            guardar(valor, documento: documento)
        }
    }

    /// Copia frames, escenas y favoritos bajo una identidad documental nueva.
    /// Un destino ya inicializado por la UI sigue siendo válido si está vacío:
    /// construir la pestaña puede materializar su JSON antes de que llegue la
    /// tinta. Solo se rechaza contenido documental real preexistente.
    ///
    /// Los frames de estudio conservan su objetivo y vínculo con el problema,
    /// pero reinician intentos, aciertos y errores para que la copia no herede
    /// un historial espacial que ocurrió en otro documento.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64)
        throws -> (frames: Int, escenas: Int, favoritos: Int) {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        return try cola.sync {
            let destinoActual = archivo(destino)
            guard destinoActual.frames.isEmpty,
                  destinoActual.escenas.isEmpty,
                  destinoActual.favoritos.isEmpty else {
                throw CocoaError(.fileWriteFileExists)
            }

            let fuente = archivo(origen)
            var mapaFrames: [UUID: UUID] = [:]
            let frames = fuente.frames.map { original -> FrameApunteIPad in
                var copia = original
                let nuevoId = UUID()
                mapaFrames[original.id] = nuevoId
                copia.id = nuevoId
                if let estudio = original.estudio {
                    copia.estudio = estudio.reiniciadoParaCopiaDocumental()
                }
                return copia
            }
            let escenas = fuente.escenas.map { original -> EscenaApunteIPad in
                var copia = original
                copia.id = UUID()
                if let frameId = original.frameId {
                    copia.frameId = mapaFrames[frameId]
                }
                return copia
            }
            let favoritos = fuente.favoritos.filter(\.vigente).map {
                original -> FavoritoEspacialTemporalIPad in
                var copia = original
                copia.id = UUID()
                return copia
            }
            let copia = ArchivoNavegacionEspacialIPad(
                formato: fuente.formato,
                frames: frames,
                escenas: escenas,
                favoritos: favoritos)
            try persistir(copia, documento: destino)
            cache[destino] = copia
            return (frames.count, escenas.count, favoritos.count)
        }
    }
}

final class PanelNavegacionEspacialIPad: UITableViewController {
    private let documento: UInt64
    private let estadoVista: () -> EstadoVistaLienzo
    private let regionVisible: () -> CGRect
    private var datos: (frames: [FrameApunteIPad], escenas: [EscenaApunteIPad],
                        favoritos: [FavoritoEspacialTemporalIPad])
    var alMostrarRegion: ((CGRect) -> Void)?
    var alRestaurarVista: ((EstadoVistaLienzo) -> Void)?
    var alCambiarFrames: (([FrameApunteIPad]) -> Void)?
    var alExportarFrame: ((FrameApunteIPad) -> Void)?

    init(documento: UInt64, estadoVista: @escaping () -> EstadoVistaLienzo,
         regionVisible: @escaping () -> CGRect) {
        self.documento = documento
        self.estadoVista = estadoVista
        self.regionVisible = regionVisible
        datos = RepositorioNavegacionEspacialIPad.compartido.instantanea(documento)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Escenas y frames"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        let agregar = UIBarButtonItem(barButtonSystemItem: .add, target: nil, action: nil)
        agregar.menu = UIMenu(children: [
            UIAction(title: "¿Qué practico ahora?",
                     image: UIImage(systemName: "target")) {
                [weak self] _ in self?.practicarSiguiente()
            },
            UIAction(title: "Simulacro adaptativo",
                     image: UIImage(systemName: "timer")) {
                [weak self] _ in self?.abrirSimulacroAdaptativo()
            },
            UIAction(title: "Debilidades del cuaderno",
                     image: UIImage(systemName: "exclamationmark.triangle")) {
                [weak self] _ in self?.abrirDebilidades()
            },
            UIAction(title: "Crear frame de ejercicio",
                     image: UIImage(systemName: "scope")) {
                [weak self] _ in self?.pedirFrameEstudio()
            },
            UIAction(title: "Crear desde Problem Bank",
                     image: UIImage(systemName: "square.and.pencil")) {
                [weak self] _ in self?.crearDesdeProblemBank()
            },
            UIAction(title: "Enfocar página actual",
                     image: UIImage(systemName: "doc.viewfinder")) {
                [weak self] _ in self?.enfocarPaginaActual()
            },
            UIAction(title: "Quitar enfoque de página",
                     image: UIImage(systemName: "eye.slash")) {
                [weak self] _ in self?.quitarFocoPaginaActual()
            },
            UIAction(title: "Guardar escena actual", image: UIImage(systemName: "camera.viewfinder")) {
                [weak self] _ in self?.pedirEscena()
            },
            UIMenu(title: "Crear frame", image: UIImage(systemName: "rectangle.dashed"),
                   children: TipoFrameIPad.allCases.map { tipo in
                UIAction(title: tipo.rawValue) { [weak self] _ in self?.pedirFrame(tipo) }
            }),
            UIAction(title: "Favorito por una semana", image: UIImage(systemName: "star")) {
                [weak self] _ in self?.pedirFavorito()
            },
        ])
        navigationItem.rightBarButtonItem = agregar
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Frames imprimibles y de estudio", "Escenas (zoom guardado)",
         "Favoritos temporales"][section]
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return datos.frames.count
        case 1: return datos.escenas.count
        default: return datos.favoritos.count
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.accessoryType = .disclosureIndicator
        switch indexPath.section {
        case 0:
            let frame = datos.frames[indexPath.row]
            celda.textLabel?.text = frame.titulo
            if let estudio = frame.estudio {
                let objetivo = estudio.objetivo.isEmpty ? "Ejercicio" : estudio.objetivo
                celda.detailTextLabel?.text = "\(objetivo) · \(estudio.resumenCorto)"
                celda.imageView?.image = UIImage(systemName:
                    estudio.estado == .dominado ? "checkmark.seal" : "scope")
            } else {
                celda.detailTextLabel?.text = "\(frame.tipo.rawValue) · \(Int(frame.region.width)) × \(Int(frame.region.height)) pt"
                celda.imageView?.image = UIImage(systemName: "rectangle.dashed")
            }
        case 1:
            let escena = datos.escenas[indexPath.row]
            celda.textLabel?.text = escena.titulo
            celda.detailTextLabel?.text = String(format: "Zoom %.0f %%", escena.vista.zoom * 100)
            celda.imageView?.image = UIImage(systemName: "camera.viewfinder")
        default:
            let favorito = datos.favoritos[indexPath.row]
            celda.textLabel?.text = favorito.titulo
            celda.detailTextLabel?.text = favorito.vence.map {
                "Temporal hasta \($0.formatted(date: .abbreviated, time: .omitted))"
            } ?? "Sin vencimiento"
            celda.imageView?.image = UIImage(systemName: "star")
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch indexPath.section {
        case 0: presentarAcciones(datos.frames[indexPath.row])
        case 1: alRestaurarVista?(datos.escenas[indexPath.row].vista)
        default: alMostrarRegion?(datos.favoritos[indexPath.row].region)
        }
    }

    private func presentarAcciones(_ frame: FrameApunteIPad) {
        let menu = UIAlertController(
            title: frame.titulo,
            message: mensajeFrame(frame),
            preferredStyle: .actionSheet)

        if let estudio = frame.estudio {
            if estudio.intentoActivo != nil {
                menu.addAction(UIAlertAction(
                    title: "Continuar intento en concentración", style: .default) {
                    [weak self] _ in self?.enfocarFrameEstudio(frame)
                })
                menu.addAction(UIAlertAction(
                    title: "Terminar: correcto", style: .default) {
                    [weak self] _ in self?.pedirCierre(frame, resultado: .correcto)
                })
                menu.addAction(UIAlertAction(
                    title: "Terminar: repetir", style: .default) {
                    [weak self] _ in self?.pedirCierre(frame, resultado: .repetir)
                })
            } else {
                menu.addAction(UIAlertAction(
                    title: "Iniciar intento en concentración", style: .default) {
                    [weak self] _ in self?.iniciarIntento(frame)
                })
            }
            menu.addAction(UIAlertAction(title: "Ver progreso", style: .default) {
                [weak self] _ in self?.mostrarProgreso(estudio, titulo: frame.titulo)
            })
            menu.addAction(UIAlertAction(
                title: estudio.problemaId == nil
                    ? "Vincular con Problem Bank" : "Cambiar vínculo de Problem Bank",
                style: .default) { [weak self] _ in
                    self?.seleccionarProblema(para: frame)
                })
        }

        menu.addAction(UIAlertAction(title: "Ir al frame", style: .default) {
            [weak self] _ in self?.alMostrarRegion?(frame.region)
        })
        menu.addAction(UIAlertAction(title: "Exportar este frame", style: .default) {
            [weak self] _ in self?.alExportarFrame?(frame)
        })
        menu.addAction(UIAlertAction(title: "Copiar enlace profundo", style: .default) {
            _ in UIPasteboard.general.string = "tinta://frame/\(frame.id.uuidString)"
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.sourceView = view
        present(menu, animated: true)
    }

    private func mensajeFrame(_ frame: FrameApunteIPad) -> String? {
        guard let estudio = frame.estudio else { return nil }
        var partes: [String] = []
        if !estudio.objetivo.isEmpty { partes.append(estudio.objetivo) }
        if let problemaId = estudio.problemaId {
            if let problema = problemaProblemBank(problemaId) {
                partes.append("Problem Bank · \(problema.tema)")
            } else {
                partes.append("Problem Bank · vínculo no disponible")
            }
        }
        return partes.isEmpty ? nil : partes.joined(separator: "\n")
    }

    private func problemaProblemBank(_ id: UUID) -> ProblemaBancoIPad? {
        CentroEstudioTecnicoIPad.compartido.instantanea().problemas.first { $0.id == id }
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        let id: UUID
        switch indexPath.section {
        case 0: id = datos.frames[indexPath.row].id
        case 1: id = datos.escenas[indexPath.row].id
        default: id = datos.favoritos[indexPath.row].id
        }
        return UISwipeActionsConfiguration(actions: [UIContextualAction(
            style: .destructive, title: "Eliminar") { [weak self] _, _, fin in
                guard let self else { fin(false); return }
                RepositorioNavegacionEspacialIPad.compartido.eliminar(
                    id, documento: self.documento)
                self.recargar(); fin(true)
            }])
    }

    private func enfocarPaginaActual() {
        let region = GeometriaFocoPaginaActualIPad.region(
            visible: regionVisible(),
            marcosPagina: datos.frames.map(\.region))
        CanalFocoPaginaActualIPad.activar(documento: documento, region: region)
        let contexto = GeometriaFocoPaginaActualIPad.contextoVisual(para: region)
        dismiss(animated: true) { [weak self] in
            self?.alMostrarRegion?(contexto)
        }
    }

    private func quitarFocoPaginaActual() {
        CanalFocoPaginaActualIPad.desactivar(documento: documento)
        dismiss(animated: true)
    }

    private func enfocarFrameEstudio(_ frame: FrameApunteIPad) {
        let contexto = GeometriaFocoPaginaActualIPad.contextoVisual(para: frame.region)
        dismiss(animated: true) { [weak self] in
            guard let self else { return }
            self.alMostrarRegion?(contexto)
            CanalFocoPaginaActualIPad.activarCuandoSeaVisible(
                documento: self.documento, region: frame.region)
        }
    }

    private func iniciarIntento(_ frame: FrameApunteIPad) {
        guard RepositorioNavegacionEspacialIPad.compartido.iniciarIntento(
            frameId: frame.id, documento: documento) != nil else {
            mostrarMensaje("No se pudo iniciar el intento sin confirmar su estado en disco.")
            return
        }
        recargar()
        enfocarFrameEstudio(frameActualizado(frame.id) ?? frame)
    }

    private func practicarSiguiente() {
        let frames = datos.frames.filter { $0.estudio != nil }
        let estudios = frames.compactMap(\.estudio)
        guard let indice = PlanificadorFramesEstudioIPad.indiceSiguiente(estudios),
              frames.indices.contains(indice) else {
            mostrarMensaje(
                "Todavía no hay frames de ejercicio. Cree uno sobre una página o región que quiera practicar.",
                titulo: "¿Qué practico ahora?")
            return
        }
        let frame = frames[indice]
        if frame.estudio?.intentoActivo != nil {
            enfocarFrameEstudio(frame)
        } else {
            iniciarIntento(frame)
        }
    }

    private func abrirSimulacroAdaptativo() {
        let repo = RepositorioSimulacroDocumentoIPad.compartido
        if let existente = repo.cargar(documento: documento) {
            if existente.frameActualId != nil {
                let alerta = UIAlertController(
                    title: "Simulacro en curso",
                    message: descripcionSimulacro(existente),
                    preferredStyle: .actionSheet)
                alerta.addAction(UIAlertAction(title: "Continuar", style: .default) {
                    [weak self] _ in self?.continuarSimulacro(existente)
                })
                alerta.addAction(UIAlertAction(title: "Empezar uno nuevo", style: .destructive) {
                    [weak self] _ in self?.presentarConfiguracionSimulacro()
                })
                alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
                alerta.popoverPresentationController?.sourceView = view
                present(alerta, animated: true)
            } else {
                mostrarResumenSimulacro(existente)
            }
            return
        }
        presentarConfiguracionSimulacro()
    }

    private func presentarConfiguracionSimulacro() {
        let disponibles = datos.frames.filter { $0.estudio != nil }.count
        guard disponibles > 0 else {
            mostrarMensaje(
                "Primero cree al menos un frame de ejercicio o importe uno desde Problem Bank.",
                titulo: "Simulacro adaptativo")
            return
        }
        let alerta = UIAlertController(
            title: "Simulacro adaptativo",
            message: "CuadernU prioriza intentos abiertos, repasos vencidos, errores y material pendiente. No muestra soluciones durante la práctica.",
            preferredStyle: .actionSheet)
        let presets: [(Int, DuracionSimulacroIPad)] = [
            (5, .rapido), (10, .normal), (15, .largo),
        ]
        for (cantidad, duracion) in presets {
            let real = min(cantidad, disponibles)
            alerta.addAction(UIAlertAction(
                title: "\(real) ejercicios · \(duracion.nombre)", style: .default) {
                [weak self] _ in
                self?.crearSimulacro(cantidad: real, duracion: duracion)
            })
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    private func crearSimulacro(
        cantidad: Int,
        duracion: DuracionSimulacroIPad
    ) {
        let candidatos = datos.frames.compactMap { frame -> CandidatoSimulacroFrameIPad? in
            guard let estudio = frame.estudio else { return nil }
            return CandidatoSimulacroFrameIPad(
                frameId: frame.id,
                problemaId: estudio.problemaId,
                titulo: frame.titulo,
                estudio: estudio)
        }
        let plan = PlanificadorSimulacroFramesIPad.construir(
            candidatos: candidatos,
            cantidadSolicitada: cantidad,
            duracion: duracion)
        guard !plan.frameIds.isEmpty else {
            mostrarMensaje("No hay ejercicios válidos para construir el simulacro.")
            return
        }
        let sesion = EstadoSesionSimulacroIPad(plan: plan)
        guard RepositorioSimulacroDocumentoIPad.compartido.guardar(
            sesion, documento: documento) else {
            mostrarMensaje("No se pudo guardar el simulacro antes de empezar.")
            return
        }
        continuarSimulacro(sesion)
    }

    private func continuarSimulacro(_ sesion: EstadoSesionSimulacroIPad) {
        guard let frameId = sesion.frameActualId else {
            mostrarResumenSimulacro(sesion)
            return
        }
        guard let frame = datos.frames.first(where: { $0.id == frameId }) else {
            var reparada = sesion
            _ = reparada.omitirActual()
            guard RepositorioSimulacroDocumentoIPad.compartido.guardar(
                reparada, documento: documento) else {
                mostrarMensaje("El siguiente ejercicio ya no existe y no se pudo reparar el plan.")
                return
            }
            continuarSimulacro(reparada)
            return
        }
        if frame.estudio?.intentoActivo != nil {
            enfocarFrameEstudio(frame)
        } else {
            iniciarIntento(frame)
        }
    }

    private func continuarSimulacroDespuesDeCierre() {
        guard let sesion = RepositorioSimulacroDocumentoIPad.compartido
            .cargar(documento: documento) else { return }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.18) { [weak self] in
            self?.continuarSimulacro(sesion)
        }
    }

    private func descripcionSimulacro(_ sesion: EstadoSesionSimulacroIPad) -> String {
        let restante = Int(ceil(sesion.segundosRestantes() / 60))
        return "\(sesion.terminados)/\(sesion.plan.cantidad) completados · \(max(restante, 0)) min restantes"
    }

    private func mostrarResumenSimulacro(_ sesion: EstadoSesionSimulacroIPad) {
        let panel = PanelResultadosSimulacroIPad(
            sesion: sesion,
            frames: datos.frames)
        panel.alRevisarFrame = { [weak self] frameId in
            guard let self,
                  let frame = self.datos.frames.first(where: { $0.id == frameId }) else {
                return
            }
            if frame.estudio?.intentoActivo != nil {
                self.enfocarFrameEstudio(frame)
            } else {
                self.iniciarIntento(frame)
            }
        }
        panel.alNuevoSimulacro = { [weak self] in
            guard let self else { return }
            _ = RepositorioSimulacroDocumentoIPad.compartido.eliminar(
                documento: self.documento)
            self.presentarConfiguracionSimulacro()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func abrirDebilidades() {
        let panel = PanelErroresFramesIPad(frames: datos.frames)
        panel.alAbrirFrame = { [weak self] frameId in
            guard let self,
                  let frame = self.datos.frames.first(where: { $0.id == frameId }) else {
                return
            }
            if frame.estudio?.intentoActivo != nil {
                self.enfocarFrameEstudio(frame)
            } else {
                self.iniciarIntento(frame)
            }
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func pedirCierre(
        _ frame: FrameApunteIPad,
        resultado: ResultadoIntentoFrameIPad
    ) {
        let repetir = resultado == .repetir
        let alerta = UIAlertController(
            title: repetir ? "Marcar para repetir" : "Marcar correcto",
            message: "Registre su confianza de 0 a 100 %. El intento seguirá abierto si el valor no es válido.",
            preferredStyle: .alert)
        alerta.addTextField {
            $0.placeholder = "Confianza 0–100"
            $0.keyboardType = .decimalPad
        }
        if repetir {
            alerta.addTextField { $0.placeholder = "¿Qué falló exactamente?" }
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar intento", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let texto = alerta.textFields?.first?.text?.replacingOccurrences(
                of: ",", with: ".") ?? ""
            guard let porcentaje = Double(texto), (0...100).contains(porcentaje) else {
                self.mostrarMensaje("La confianza debe ser un número entre 0 y 100. El intento quedó abierto.")
                return
            }
            let eraSimulacro = RepositorioSimulacroDocumentoIPad.compartido
                .cargar(documento: self.documento)?.frameActualId == frame.id
            let error = repetir ? (alerta.textFields?[1].text ?? "") : ""
            guard RepositorioNavegacionEspacialIPad.compartido.cerrarIntento(
                frameId: frame.id,
                documento: self.documento,
                resultado: resultado,
                confianza: porcentaje / 100,
                error: error) else {
                self.mostrarMensaje("No se pudo cerrar el intento de forma persistente.")
                return
            }
            self.recargar()
            if let actualizado = self.frameActualizado(frame.id),
               let estudio = actualizado.estudio {
                let resultados = SincronizadorProblemaFrameIPad.sincronizarTerminados(
                    estudio: estudio,
                    documentoId: self.documento,
                    frameId: actualizado.id)
                if resultados.contains(.problemaNoDisponible) {
                    self.mostrarMensaje(
                        "El intento quedó guardado en el frame, pero el problema vinculado ya no está disponible en Problem Bank. Puede cambiar el vínculo y CuadernU volverá a sincronizarlo.",
                        titulo: "Intento conservado")
                }
                _ = ReconciliadorSimulacroFramesIPad.registrarTerminados(
                    estudio: estudio,
                    documentoId: self.documento,
                    frameId: actualizado.id)
            }
            CanalFocoPaginaActualIPad.desactivar(documento: self.documento)
            if eraSimulacro {
                self.continuarSimulacroDespuesDeCierre()
            }
        })
        present(alerta, animated: true)
    }

    private func mostrarProgreso(_ estudio: EstadoEstudioFrameIPad, titulo: String) {
        var lineas: [String] = []
        if !estudio.objetivo.isEmpty { lineas.append("Objetivo: \(estudio.objetivo)") }
        if let problemaId = estudio.problemaId,
           let problema = problemaProblemBank(problemaId) {
            lineas.append("Problem Bank: \(problema.tema) · \(problema.estado.rawValue)")
        }
        lineas.append(estudio.resumenCorto)
        let recientes = estudio.intentosTerminados.suffix(5)
        for (indice, intento) in recientes.enumerated() {
            let simbolo = intento.resultado == .correcto ? "✓" : "↻"
            let segundos = Int((intento.duracion ?? 0).rounded())
            let tiempo = String(format: "%02d:%02d", segundos / 60, segundos % 60)
            var detalle = "\(simbolo) \(recientes.count - indice). \(tiempo) · \(Int((intento.confianza * 100).rounded()))%"
            if !intento.error.isEmpty { detalle += " · \(intento.error)" }
            lineas.append(detalle)
        }
        if estudio.intentoActivo != nil { lineas.append("● Hay un intento activo") }
        mostrarMensaje(lineas.joined(separator: "\n"), titulo: titulo)
    }

    private func crearDesdeProblemBank() {
        presentarSelectorProblema { [weak self] problema in
            guard let self else { return }
            if let existente = self.datos.frames.first(where: {
                $0.estudio?.problemaId == problema.id
            }) {
                if existente.estudio?.intentoActivo != nil {
                    self.enfocarFrameEstudio(existente)
                } else {
                    self.iniciarIntento(existente)
                }
                return
            }

            let region = GeometriaFocoPaginaActualIPad.region(
                visible: self.regionVisible(),
                marcosPagina: self.datos.frames.map(\.region))
            var frame = FrameApunteIPad(
                titulo: problema.tema.isEmpty ? "Problema" : problema.tema,
                region: region,
                tipo: .visible,
                colorARGB: 0xFFFF9F0A)
            frame.estudio = EstadoEstudioFrameIPad(
                objetivo: problema.enunciado,
                problemaId: problema.id)
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                frame, documento: self.documento)
            self.recargar()
            self.iniciarIntento(self.frameActualizado(frame.id) ?? frame)
        }
    }

    private func seleccionarProblema(para frame: FrameApunteIPad) {
        presentarSelectorProblema { [weak self] problema in
            guard let self else { return }
            guard RepositorioNavegacionEspacialIPad.compartido.vincularProblema(
                frameId: frame.id,
                documento: self.documento,
                problemaId: problema.id,
                objetivo: problema.enunciado) else {
                self.mostrarMensaje("No se pudo confirmar el vínculo del frame en disco.")
                return
            }
            self.recargar()
            if let actualizado = self.frameActualizado(frame.id),
               let estudio = actualizado.estudio {
                _ = SincronizadorProblemaFrameIPad.sincronizarTerminados(
                    estudio: estudio,
                    documentoId: self.documento,
                    frameId: actualizado.id)
            }
        }
    }

    private func presentarSelectorProblema(
        alElegir: @escaping (ProblemaBancoIPad) -> Void
    ) {
        let selector = PanelSeleccionProblemaFrameIPad()
        selector.alSeleccionar = alElegir
        present(UINavigationController(rootViewController: selector), animated: true)
    }

    private func pedirEscena() {
        pedirNombre(titulo: "Guardar escena", sugerido: "Escena \(datos.escenas.count + 1)") {
            [weak self] nombre in
            guard let self else { return }
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                EscenaApunteIPad(titulo: nombre, vista: self.estadoVista()),
                documento: self.documento)
            self.recargar()
        }
    }

    private func pedirFrameEstudio() {
        let cantidad = datos.frames.filter { $0.estudio != nil }.count + 1
        let alerta = UIAlertController(
            title: "Frame de ejercicio",
            message: "Convierte la página actual en una unidad de práctica con intentos, confianza y errores, sin modificar la tinta.",
            preferredStyle: .alert)
        alerta.addTextField { $0.text = "Ejercicio \(cantidad)" }
        alerta.addTextField { $0.placeholder = "Objetivo: ¿qué debe poder resolver?" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let titulo = alerta.textFields?[0].text?.trimmingCharacters(
                in: .whitespacesAndNewlines) ?? ""
            let objetivo = alerta.textFields?[1].text?.trimmingCharacters(
                in: .whitespacesAndNewlines) ?? ""
            let region = GeometriaFocoPaginaActualIPad.region(
                visible: self.regionVisible(),
                marcosPagina: self.datos.frames.map(\.region))
            var frame = FrameApunteIPad(
                titulo: titulo.isEmpty ? "Ejercicio \(cantidad)" : titulo,
                region: region,
                tipo: .visible,
                colorARGB: 0xFFFF9F0A)
            frame.estudio = EstadoEstudioFrameIPad(objetivo: objetivo)
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                frame, documento: self.documento)
            self.recargar()
        })
        present(alerta, animated: true)
    }

    private func pedirFrame(_ tipo: TipoFrameIPad) {
        pedirNombre(titulo: "Nuevo frame", sugerido: tipo.rawValue) { [weak self] nombre in
            guard let self else { return }
            let visible = self.regionVisible()
            let tamano: CGSize
            switch tipo {
            case .visible: tamano = visible.size
            case .a4: tamano = CGSize(width: 1240, height: 1754)
            case .carta: tamano = CGSize(width: 1275, height: 1650)
            case .presentacion: tamano = CGSize(width: 1600, height: 900)
            }
            let region = CGRect(x: visible.midX - tamano.width / 2,
                                y: visible.midY - tamano.height / 2,
                                width: tamano.width, height: tamano.height)
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                FrameApunteIPad(titulo: nombre, region: region, tipo: tipo),
                documento: self.documento)
            self.recargar()
        }
    }

    private func pedirFavorito() {
        pedirNombre(titulo: "Favorito temporal", sugerido: "Revisar") { [weak self] nombre in
            guard let self else { return }
            RepositorioNavegacionEspacialIPad.compartido.agregar(
                FavoritoEspacialTemporalIPad(
                    titulo: nombre, region: self.regionVisible(),
                    vence: Calendar.current.date(byAdding: .day, value: 7, to: Date())),
                documento: self.documento)
            self.recargar()
        }
    }

    private func pedirNombre(titulo: String, sugerido: String,
                             guardar: @escaping (String) -> Void) {
        let alerta = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.text = sugerido }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { _ in
            let nombre = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines) ?? sugerido
            guardar(nombre.isEmpty ? sugerido : nombre)
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func mostrarMensaje(_ mensaje: String, titulo: String? = nil) {
        let alerta = UIAlertController(title: titulo, message: mensaje,
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .default))
        present(alerta, animated: true)
    }

    private func frameActualizado(_ id: UUID) -> FrameApunteIPad? {
        datos.frames.first { $0.id == id }
    }

    private func recargar() {
        datos = RepositorioNavegacionEspacialIPad.compartido.instantanea(documento)
        tableView.reloadData(); alCambiarFrames?(datos.frames)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
