import SwiftUI
import Combine
import PhotosUI
import Photos
import AVKit
import PDFKit
import UIKit
import UniformTypeIdentifiers
import VisionKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    func application(_ application: UIApplication,
                     configurationForConnecting sesion: UISceneSession,
                     options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        let config = UISceneConfiguration(name: nil, sessionRole: sesion.role)
        config.delegateClass = EscenaPrincipal.self
        return config
    }
}

/// Distingue el proceso anfitrion de XCTest de una ejecucion real de la app.
///
/// Los unit tests cargan el ejecutable completo de CuadernU. Arrancar en ese
/// proceso la biblioteca, los listeners LAN y toda la escena introduce trabajo
/// ajeno a la unidad bajo prueba y, en el runner compartido de CI, llego a agotar
/// el plazo de un test de calculadora. Los UI tests son distintos: lanzan la app
/// como otro proceso y pasan `--pruebas-ui`, por lo que necesitan la interfaz real.
enum EntornoEjecucionIPad {
    static func esHostPruebasUnitarias(
        argumentos: [String] = ProcessInfo.processInfo.arguments,
        entorno: [String: String] = ProcessInfo.processInfo.environment
    ) -> Bool {
        guard !argumentos.contains("--pruebas-ui") else { return false }
        return entorno["XCTestConfigurationFilePath"] != nil
            || entorno["XCTestBundlePath"] != nil
            || entorno["XCTestSessionIdentifier"] != nil
    }
}

final class EscenaPrincipal: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?

    func scene(_ scene: UIScene,
               willConnectTo sesion: UISceneSession,
               options: UIScene.ConnectionOptions) {
        guard let escena = scene as? UIWindowScene else { return }
        let ventana = UIWindow(windowScene: escena)

        if EntornoEjecucionIPad.esHostPruebasUnitarias() {
            ventana.rootViewController = UIViewController()
            ventana.makeKeyAndVisible()
            window = ventana
            return
        }
        
        // Configurar el almacén local-first (C++)
        GestorArchivosCuadernU.compartido.configurarRutaBase()
        var errorBiblioteca: Error?
        do {
            try GestorArchivosCuadernU.compartido.prepararBiblioteca(
                contextoAnterior: sesion.persistentIdentifier)
        } catch {
            // La pantalla inicial mostrará el fallo sin reemplazar ni borrar el
            // catálogo que pueda recuperarse.
            errorBiblioteca = error
        }
        
        // Todos los targets declaran TARGETED_DEVICE_FAMILY=2. Los servicios
        // LAN son opt-in: se encienden solo al abrir su panel y CuadernU nunca
        // anuncia el iPad por el mero hecho de arrancar.
        let biblioteca = BibliotecaCuadernosIPad(errorInicial: errorBiblioteca)
        let navegacion = UINavigationController(rootViewController: biblioteca)
        ventana.rootViewController = navegacion
        PreferenciasAparienciaTintaIPad.aplicarEnVentana(ventana)
        ventana.makeKeyAndVisible()
        window = ventana
        if let url = options.urlContexts.first?.url {
            DispatchQueue.main.async {
                CentroEnlacesProfundos.compartido.recibir(url)
            }
        }
    }

    func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
        if let url = URLContexts.first?.url {
            CentroEnlacesProfundos.compartido.recibir(url)
        }
        if let ventana = scene as? UIWindowScene {
            let urls = URLContexts.map(\.url)
            CentroEntradaOneNoteIPad.compartido.recibir(urls: urls, en: ventana)
        }
    }

    func scene(_ scene: UIScene, continue userActivity: NSUserActivity) {
        _ = GestorContinuidadIPad.compartido.continuar(userActivity)
    }

    func sceneDidEnterBackground(_ scene: UIScene) {
        MonitorDistraccionIPad.compartido.registrarSalida()
    }

    func sceneWillEnterForeground(_ scene: UIScene) {
        MonitorDistraccionIPad.compartido.registrarRegreso()
        if let ventana = scene as? UIWindowScene {
            CentroEntradaOneNoteIPad.compartido.instalarBotonImportar(en: ventana)
        }
    }
}

/// La aplicación arranca con un único cuaderno a ancho completo. La columna de
/// referencia existe, pero solo aparece cuando el usuario pulsa "Referencia".
/// Antes se forzaba `.oneBesideSecondary` al iniciar y se construían dos
/// interfaces completas a la vez, que es exactamente la duplicación observada
/// en el iPad físico.
final class ControladorRaizIPad: UISplitViewController {
    private(set) var referenciaVisible = false
    private var cerrando = false
    private var arrendamiento: ArrendamientoCuadernoIPad?

    convenience init(
        contextoEscena: String,
        arrendamiento: ArrendamientoCuadernoIPad? = nil
    ) {
        let principal = ControladorPestanasDocumentoIPad(
            panel: .principal, viewport: .principal,
            contextoEscena: contextoEscena)
        let referencia = ControladorPestanasDocumentoIPad(
            panel: .secundario, viewport: .secundario,
            contextoEscena: contextoEscena)
        self.init(principal: principal, referencia: referencia)
        self.arrendamiento = arrendamiento
        principal.alCerrarDocumento = { [weak self] in self?.cerrarEditor() }
        referencia.alCerrarDocumento = { [weak self] in
            self?.ocultarReferencia()
        }
    }

    init(principal: UIViewController, referencia: UIViewController) {
        super.init(style: .doubleColumn)
        preferredSplitBehavior = .tile
        preferredDisplayMode = .secondaryOnly
        preferredPrimaryColumnWidthFraction = 0.38
        presentsWithGesture = false
        displayModeButtonVisibility = .never
        setViewController(referencia, for: .primary)
        setViewController(principal, for: .secondary)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.accessibilityIdentifier = "editor.raiz"
    }

    private func cerrarEditor() {
        guard !cerrando else { return }
        cerrando = true
        do {
            try (viewController(for: .primary)
                as? ControladorPestanasDocumentoIPad)?.guardarAhora()
            try (viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad)?.guardarAhora()
            dismiss(animated: true) { [weak self] in
                self?.arrendamiento = nil
            }
        } catch {
            cerrando = false
            let alerta = UIAlertController(
                title: "No se puede cerrar todavía",
                message: "El cuaderno no pudo completar su guardado: \(error.localizedDescription)",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Entendido", style: .default))
            present(alerta, animated: true)
        }
    }

    func alternarReferencia() {
        referenciaVisible.toggle()
        if referenciaVisible {
            preferredDisplayMode = .oneBesideSecondary
            show(.primary)
        } else {
            ocultarReferencia()
        }
    }

    private func ocultarReferencia() {
        referenciaVisible = false
        preferredDisplayMode = .secondaryOnly
        show(.secondary)
    }
}

/// La pantalla de escribir.
///
/// Sigue siendo también el banco de pruebas de latencia —los contadores de
/// abajo son los que dicen si el motor alcanza el listón—, pero ya cuelga de
/// aquí lo demás: grabar la pantalla, el papel, importar PDF y fotos, exportar
/// a PDF, y escribir a teclado o a voz.
final class ControladorLienzo: UIViewController {

    /// El hospedador de pestañas persiste el nombre aceptado por el usuario.
    /// Las sugerencias automáticas nunca llaman este cierre por sí solas.
    var alCambiarTituloDocumento: ((String) -> Void)?

    let viewport: TintaViewport
    private let lienzo: LienzoCuadernU
    private let navegador: VistaLienzoNavegable
    private let etiqueta = UILabel()
    private let selectorPrediccion = UISegmentedControl(items: ["Sin predicción", "Sistema", "Núcleo"])
    private let selectorModo = UISegmentedControl(items: ModoEscritura.allCases.map(\.nombre))
    private let selectorHerramienta: UISegmentedControl = {
        let items: [Any] = HerramientaLienzo.allCases.map {
            UIImage(systemName: $0.simbolo) ?? UIImage()
        }
        return UISegmentedControl(items: items)
    }()
    private lazy var gestoAjustesBorrador = UILongPressGestureRecognizer(
        target: self, action: #selector(mantenerBorrador(_:)))
    private let etiquetaGrabacion = UILabel()
    private var barraPrincipal = UIView()
    private var restriccionesBarra: [NSLayoutConstraint] = []
    private var barraCentroX: NSLayoutConstraint?
    private var barraCentroY: NSLayoutConstraint?
    private var arrastrandoBarra = false
    private var lienzoConBarra: NSLayoutConstraint?
    private var lienzoSinBarra: NSLayoutConstraint?
    private let botonControlBarra = UIButton(type: .system)
    private let botonRecuperarBarra = UIButton(type: .system)
    private var botonesSatelite: [UUID: BotonSateliteBarraIPad] = [:]
    private let mascaraFoco = MascaraFocoIPad()
    private let filtroCuidadoVisual = UIView()
    private let controlFoco = ControlFocoFlotanteIPad()
    private let coordinadorFoco = CoordinadorFocoIPad()
    private var configuracionInterfaz = CentroExperienciaIPad.compartido
        .instantanea().interfaz
    private var configuracionBarra = CentroExperienciaIPad.compartido
        .instantanea().barra
    private var configuracionEntrada = PreferenciasEntradaIPad.actual
    
    init(viewport: TintaViewport = .principal,
         idPersistente: DocumentoPersistenteID = DocumentoPersistenteID()) {
        self.viewport = viewport
        let lienzo = LienzoCuadernU(
            viewport: viewport, idPersistente: idPersistente)
        self.lienzo = lienzo
        self.navegador = VistaLienzoNavegable(lienzo: lienzo)
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private var estadoBarra = EstadoBarraFlotante(herramienta: .pluma, color: 0xFF1A_1A1A)
    private var observadoresBarra: [AnyCancellable] = []
    private let grabadora = CoordinadorGrabacion()
    private let dictado = DictadoEnVivo()
    private var capacidades = TintaCapacidades.deEsteAparato()
    private var ajustesGrabacion = TintaAjustesGrabacion.porDefecto
    private var seleccionMicrofono = PreferenciasCaptura.microfono
    private var ajustesPantalla = AjustesPantalla()
    private var temporizador: Timer?
    private var ultimaGrabacion: URL?

    private var textoDictado = ""
    private var escribiendo = false
    private var sesionAcademica: SesionClase?
    private var guardadoEstadoPendiente: DispatchWorkItem?
    private var reentradaOfrecida = false
    private var cursoAutomaticoOfrecido: UUID?
    private var shelfLateral: UINavigationController?
    private var frenteTarjetaPendiente: CaraTarjetaManuscritaIPad?
    private var editarSiguienteImagenImportada = false
    private let fusionPizarra = FusionPizarraLocal()
    private let detectorSlide = DetectorCambioDiapositivaIPad()
    private let indicadorAutoguardado = UIButton(type: .system)
    private let botonHerramientaSimple = UIButton(type: .system)
    private let botonColorSimple = UIButton(type: .system)
    private let botonLienzoSimple = UIButton(type: .system)
    private let botonAgregarSimple = UIButton(type: .system)
    private lazy var autoguardado = CoordinadorAutoguardado(
        sesion: lienzo.sesion, navegador: navegador)
    private lazy var colaboracion = CoordinadorColaboracionDocumento(
        sesion: lienzo.sesion, navegador: navegador, autoguardado: autoguardado)

    // --- Las Casio, dentro del cuaderno ---
    // El reparto de la pantalla lo decide el núcleo C++; aquí solo se aplica.
    /// El panel embebido comparte el núcleo completo, pero muestra la experiencia
    /// ligera: ninguna cinta, OCR o panel académico debe ejecutarse detrás de una
    /// calculadora que se usa mientras se escribe en el lienzo.
    private let centroCasio = CentroCasioCuadernU(modoSoloCalculadora: true)
    private lazy var panelCasio = PanelCasio(centro: centroCasio)
    private var ajustesPanel = AjustesPanelCasio()
    // Se guardan para poder moverlas al abrir y cerrar el panel: con el panel
    // repartiendo, el lienzo deja de llegar al borde.
    private var lienzoIzquierda: NSLayoutConstraint?
    private var lienzoDerecha: NSLayoutConstraint?

    private let colores: [UInt32] = [0xFF1A_1A1A, 0xFF1B_5FA8, 0xFFC6_2828, 0xFF2E_7D32, 0xFFF9_A825]
    private var colorRetornoTemporal: UInt32?

    // MARK: - Ciclo de vida

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        if title?.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ?? true {
            title = "Nueva nota"
        }

        ajustesGrabacion = ajustesGrabacion.ajustado(a: capacidades)
        sincronizarModoPaginaDesdeSesion()

        navegador.translatesAutoresizingMaskIntoConstraints = false
        navegador.accessibilityIdentifier = "editor.canvas.contenedor"
        view.addSubview(navegador)
        navegador.capaNotasAdhesivas.alSeleccionar = { [weak self] nota in
            self?.editarNotaAdhesiva(nota)
        }

        barraPrincipal = construirBarra()
        barraPrincipal.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(barraPrincipal)

        configurarBotonRecuperacionBarra()
        botonRecuperarBarra.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(botonRecuperarBarra)

        etiqueta.numberOfLines = 0
        etiqueta.font = .monospacedSystemFont(ofSize: 13, weight: .regular)
        etiqueta.textColor = .secondaryLabel
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(etiqueta)

        etiquetaGrabacion.font = .monospacedDigitSystemFont(ofSize: 15, weight: .semibold)
        etiquetaGrabacion.textColor = .systemRed
        etiquetaGrabacion.isHidden = true
        etiquetaGrabacion.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(etiquetaGrabacion)

        mascaraFoco.isHidden = true
        mascaraFoco.translatesAutoresizingMaskIntoConstraints = false
        view.insertSubview(mascaraFoco, aboveSubview: navegador)

        filtroCuidadoVisual.isUserInteractionEnabled = false
        filtroCuidadoVisual.accessibilityElementsHidden = true
        filtroCuidadoVisual.translatesAutoresizingMaskIntoConstraints = false
        view.insertSubview(filtroCuidadoVisual, aboveSubview: navegador)

        controlFoco.isHidden = true
        controlFoco.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(controlFoco)

        let izquierda = navegador.leadingAnchor.constraint(equalTo: view.leadingAnchor)
        let derecha = navegador.trailingAnchor.constraint(equalTo: view.trailingAnchor)
        lienzoIzquierda = izquierda
        lienzoDerecha = derecha
        let conBarra = navegador.topAnchor.constraint(
            equalTo: barraPrincipal.bottomAnchor, constant: 8)
        let sinBarra = navegador.topAnchor.constraint(
            equalTo: view.safeAreaLayoutGuide.topAnchor)
        lienzoConBarra = conBarra
        lienzoSinBarra = sinBarra

        NSLayoutConstraint.activate([
            conBarra,
            izquierda,
            derecha,
            navegador.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            botonRecuperarBarra.trailingAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),
            botonRecuperarBarra.bottomAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -16),
            botonRecuperarBarra.widthAnchor.constraint(equalToConstant: 52),
            botonRecuperarBarra.heightAnchor.constraint(equalToConstant: 52),

            mascaraFoco.leadingAnchor.constraint(equalTo: navegador.leadingAnchor),
            mascaraFoco.trailingAnchor.constraint(equalTo: navegador.trailingAnchor),
            mascaraFoco.topAnchor.constraint(equalTo: navegador.topAnchor),
            mascaraFoco.bottomAnchor.constraint(equalTo: navegador.bottomAnchor),

            filtroCuidadoVisual.leadingAnchor.constraint(equalTo: navegador.leadingAnchor),
            filtroCuidadoVisual.trailingAnchor.constraint(equalTo: navegador.trailingAnchor),
            filtroCuidadoVisual.topAnchor.constraint(equalTo: navegador.topAnchor),
            filtroCuidadoVisual.bottomAnchor.constraint(equalTo: navegador.bottomAnchor),

            controlFoco.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                                   constant: -16),
            controlFoco.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                                 constant: -16),

            etiqueta.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 20),
            etiqueta.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                             constant: -16),

            etiquetaGrabacion.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20),
            etiquetaGrabacion.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                                      constant: -16),
        ])
        instalarRestriccionesBarra()

        lienzo.alActualizarEstadisticas = { [weak self] in self?.refrescar() }
        lienzo.alCambiarActividad = { [weak self] activo in self?.marcarEscritura(activo) }
        lienzo.alPedirPaleta = { [weak self] in self?.mostrarPaleta() }
        lienzo.alApretarLapiz = { [weak self] in self?.accionApretonLapiz() }
        lienzo.alDesplazarConHerramienta = { [weak self] delta in
            self?.navegador.desplazarConHerramienta(delta)
        }
        lienzo.alCambiarSeleccion = { [weak self] cantidad in
            self?.actualizarControlesSeleccion(cantidad)
        }
        lienzo.alCambiarSeleccionDetallada = { [weak self] seleccion in
            self?.navegador.capaElementos.mostrarSeleccion(seleccion)
        }
        lienzo.alCambiarContenidoSeleccion = { [weak self] in
            guard let self else { return }
            self.navegador.recargarElementos()
            self.autoguardado.marcarCambio()
        }
        lienzo.alDetectarMarcadorDibujado = { [weak self] punto in
            guard let self else { return }
            self.navegador.agregarMarcador(titulo: "⭐ Marcador dibujado", en: punto)
            self.programarGuardadoEstado()
        }
        lienzo.alBorrarObjetoEnPunto = { [weak self] punto in
            guard let self else { return false }
            let cambio = self.navegador.borrarObjetoEditable(en: punto)
            if cambio { self.autoguardado.marcarCambio() }
            return cambio
        }
        lienzo.alNotificarGestoRegion = { [weak self] texto in
            self?.navigationItem.prompt = texto
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.4) { [weak self] in
                guard let self, self.navigationItem.prompt == texto else { return }
                self.actualizarControlesSeleccion(self.lienzo.seleccionActual.total)
            }
        }
        navegador.capaElementos.alCompartir = { [weak self] url in
            guard let self else { return }
            let compartir = UIActivityViewController(activityItems: [url],
                                                     applicationActivities: nil)
            compartir.popoverPresentationController?.sourceView = self.view
            self.present(compartir, animated: true)
        }
        navegador.capaElementos.alEditarRecorte = { [weak self] medio in
            self?.abrirEditorImagen(medio)
        }
        navegador.capaElementos.alCrearClipPDF = { [weak self] medio in
            guard let self else { return }
            let panel = ControladorClipPDFIPad(medio: medio, sesion: self.lienzo.sesion)
            panel.alCrear = { [weak self] _ in
                self?.navegador.recargarElementos()
                self?.autoguardado.marcarCambio()
            }
            self.present(UINavigationController(rootViewController: panel), animated: true)
        }
        navegador.capaElementos.alAbrirFuente = { [weak self] medio in
            self?.abrirOriginal(medio, peek: false)
        }
        navegador.capaElementos.alVistaPeek = { [weak self] medio in
            self?.abrirOriginal(medio, peek: true)
        }
        navegador.capaElementos.alCopiarCita = { [weak self] medio in
            guard let fuente = RepositorioProcedenciaIPad.compartido.fuente(
                hash: medio.hash) else { return }
            UIPasteboard.general.string = fuente.cita(pagina: medio.paginaFuente)
            self?.avisar("Referencia copiada.")
        }
        navegador.capaElementos.alEditarFondo = { [weak self] medio in
            self?.abrirEditorFondo(medio)
        }
        navegador.capaElementos.alEditarTextoAvanzado = { [weak self] texto in
            self?.abrirAjustesTexto(texto)
        }
        navegador.capaElementos.alConvertir = { referencia, destino in
            _ = CentroAcademico.compartido.convertir(referencia, a: destino)
        }
        navegador.alCambiarVista = { [weak self] _ in self?.programarGuardadoEstado() }
        navegador.alTerminarTrazo = { [weak self] _ in
            self?.restaurarColorTemporalSiHaceFalta()
        }
        navegador.mostrarFrames(
            RepositorioNavegacionEspacialIPad.compartido.instantanea(
                lienzo.sesion.documentoPersistenteId).frames)
        if viewport == .principal {
            NotificationCenter.default.addObserver(
                self, selector: #selector(recibirCapturaRemota(_:)),
                name: .capturaRemotaRecibida, object: nil)
            NotificationCenter.default.addObserver(
                self, selector: #selector(recibirComandoRemoto(_:)),
                name: .comandoRemotoRecibido, object: nil)
        }
        NotificationCenter.default.addObserver(
            self, selector: #selector(recibirEnlaceProfundo(_:)),
            name: .enlaceProfundoTinta, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(entrarSegundoPlano),
            name: UIApplication.didEnterBackgroundNotification, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(regresarPrimerPlano),
            name: .reentradaDesdeSegundoPlanoIPad, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(materialVinculadoCambio(_:)),
            name: .materialVinculadoCambioIPad, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(cambiarAparienciaDesdePreferencias(_:)),
            name: .aparienciaTintaCambio, object: nil)

        grabadora.alCambiarEstado = { [weak self] estado in self?.alCambiarGrabacion(estado) }
        grabadora.alFallar = { [weak self] error in
            self?.avisar("La grabación se interrumpió: \(error.localizedDescription)")
        }
        grabadora.alAdvertir = { [weak self] error in
            self?.avisar("La grabación se guardó con una advertencia: \(error.localizedDescription)")
        }

        autoguardado.alCambiarEstado = { [weak self] estado in
            self?.actualizarIndicadorAutoguardado(estado)
        }
        actualizarIndicadorAutoguardado(.sinCambios)
        let recuperacionPendiente = autoguardado.iniciar()
        _ = colaboracion
        if recuperacionPendiente {
            DispatchQueue.main.async { [weak self] in self?.ofrecerRecuperacionPendiente() }
        }

        montarPanelCasio()

        selectorPrediccion.selectedSegmentIndex = 1
        selectorModo.selectedSegmentIndex = 0
        selectorHerramienta.selectedSegmentIndex = HerramientaLienzo.pluma.rawValue
        configurarFoco()
        aplicarConfiguracionEntrada(configuracionEntrada)
        aplicarApariencia(PreferenciasAparienciaTintaIPad.actual)
        aplicarInterfaz(configuracionInterfaz)
        estadoBarra.$herramientaActiva.sink { [weak self] h in self?.lienzo.herramienta = h }.store(in: &observadoresBarra)
        estadoBarra.$colorActivo.sink { [weak self] c in self?.lienzo.colorPluma = c }.store(in: &observadoresBarra)
        registrarDocumentoActual()
        aplicarPantalla()
        refrescar()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        becomeFirstResponder()
        ofrecerReentradaSiCorresponde()
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { [weak self] in
            self?.ofrecerCursoAutomatico()
        }
    }

    override var canBecomeFirstResponder: Bool { true }

    /// Las pestañas inactivas mantienen el documento y el estado de navegación,
    /// pero entregan sus texturas Metal. Evita multiplicar el backing completo
    /// del canvas por cada pestaña restaurada.
    func suspenderRecursosGraficos() {
        lienzo.suspenderRecursosGraficos()
    }

    func reanudarRecursosGraficos() {
        loadViewIfNeeded()
        lienzo.reanudarRecursosGraficos()
    }

    var conservaRecursosGraficosActivos: Bool {
        isViewLoaded && lienzo.conservaRecursosGraficosActivos
    }

    override var keyCommands: [UIKeyCommand]? {
        var comandos = CentroExperienciaIPad.compartido.instantanea().atajos.map { atajo in
            let comando = UIKeyCommand(input: atajo.entrada,
                                       modifierFlags: atajo.flags,
                                       action: #selector(ejecutarAtajo(_:)))
            comando.discoverabilityTitle = atajo.accion.nombre
            comando.wantsPriorityOverSystemBehavior = atajo.modificadores == 0
            return comando
        }
        let eliminar = UIKeyCommand(input: "\u{8}",
                                    modifierFlags: [],
                                    action: #selector(eliminarSeleccionConTeclado(_:)))
        eliminar.discoverabilityTitle = "Eliminar selección"
        comandos.append(eliminar)
        return comandos
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        guardarEstadoExacto()
        guardarReentrada()
        autoguardado.guardarAhora(motivo: "cambio de pantalla")
        // No dejar la pantalla clavada encendida al salir: es batería que se va
        // sin que nadie lo note.
        UIApplication.shared.isIdleTimerDisabled = false
    }

    // MARK: - Las Casio dentro del cuaderno

    private func montarPanelCasio() {
        addChild(panelCasio)
        panelCasio.view.isHidden = true
        view.addSubview(panelCasio.view)
        panelCasio.didMove(toParent: self)

        centroCasio.alEntregarCaptura = { [weak self] imagen, nombre in
            guard let self = self else { return }
            guard let adjunto = try? RepositorioAdjuntos.compartido.importar(imagen: imagen, nombre: nombre + ".png") else { return }
            let puntoTinta = self.navegador.centroVisibleDocumento
            _ = self.navegador.capaElementos.insertarAdjunto(adjunto, tipo: .imagen, en: puntoTinta)
            self.autoguardado.marcarCambio()
        }

        panelCasio.alCambiarFraccion = { [weak self] fraccion in
            guard let self else { return }
            // Un 0 significa que se arrastró hasta el borde: eso cierra.
            if fraccion <= 0 {
                self.ajustesPanel.visible = false
            } else {
                self.ajustesPanel.fraccion = fraccion
            }
            self.aplicarReparto()
        }

        panelCasio.alCerrar = { [weak self] in
            guard let self else { return }
            self.ajustesPanel.visible = false
            self.aplicarReparto()
        }

        panelCasio.alMoverFlotante = { [weak self] gesto in
            self?.arrastrarPanelCasio(gesto)
        }
    }

    @objc private func alternarCasio(_ sender: Any) {
        ajustesPanel.visible.toggle()
        if ajustesPanel.visible {
            ajustesPanel.modo = .flotante
            view.bringSubviewToFront(panelCasio.view)
        }
        aplicarReparto()
    }

    @objc private func abrirHerramientasCasio(_ sender: Any) {
        let alerta = UIAlertController(title: "Herramientas Casio", message: nil, preferredStyle: .actionSheet)
        if let popover = alerta.popoverPresentationController {
            if let vista = sender as? UIView {
                popover.sourceView = vista
                popover.sourceRect = vista.bounds
            } else {
                // Las acciones de la command palette llegan con el controlador
                // como sender. Un action sheet sin ancla produce una excepción en
                // iPad, por eso siempre existe este origen seguro.
                popover.sourceView = view
                popover.sourceRect = CGRect(x: view.bounds.midX,
                                            y: view.safeAreaInsets.top + 24,
                                            width: 1, height: 1)
                popover.permittedArrowDirections = []
            }
        }

        alerta.addAction(UIAlertAction(
            title: ajustesPanel.visible ? "Ocultar calculadora" : "Mostrar calculadora",
            style: .default) { [weak self] _ in
                guard let self else { return }
                self.alternarCasio(self)
            })

        for modelo in ModeloCalculadora.todos {
            let marca = centroCasio.modeloActivo?.id == modelo.id ? "✓ " : ""
            alerta.addAction(UIAlertAction(
                title: "\(marca)Abrir \(modelo.nombre) flotante",
                style: .default) { [weak self] _ in
                    guard let self else { return }
                    self.centroCasio.seleccionarModelo(modelo.id)
                    self.ajustesPanel.visible = true
                    self.ajustesPanel.modo = .flotante
                    self.view.bringSubviewToFront(self.panelCasio.view)
                    self.aplicarReparto()
                })
        }
        
        let comandos: [(String, ComandoCasioTinta, String)] = [
            ("Mostrar Cinta", .mostrarCinta, "list.bullet.rectangle"),
            ("Guardar Punto", .guardarPunto, "flag.fill"),
            ("Deshacer Tecla", .deshacerTecla, "arrow.uturn.backward"),
            ("Mostrar Rebobinado", .mostrarRebobinado, "clock.arrow.circlepath"),
            ("Mostrar Fórmulas", .mostrarFormulas, "function"),
            ("Mostrar Procedimientos", .mostrarProcedimientos, "text.book.closed"),
            ("Mostrar Plano", .mostrarPlano, "chart.xyaxis.line"),
            ("Mostrar Gráfica", .mostrarGrafica, "waveform.path.ecg"),
            ("Mostrar Intentos", .mostrarIntentos, "arrow.2.squarepath"),
            ("Mostrar Exportación", .mostrarExportacion, "square.and.arrow.up"),
            ("Nuevo Ejercicio", .nuevoEjercicio, "plus.circle"),
            ("Capturar LCD", .capturarLCD, "camera.viewfinder")
        ]
        
        for (titulo, comando, icono) in comandos {
            let accion = UIAlertAction(title: titulo, style: .default) { [weak self] _ in
                self?.centroCasio.enviar(comando)
            }
            accion.setValue(UIImage(systemName: icono), forKey: "image")
            alerta.addAction(accion)
        }
        
        alerta.addAction(UIAlertAction(
            title: ajustesPanel.modo == .flotante
                ? "Acoplar calculadora" : "Convertir en flotante",
            style: .default) { [weak self] _ in
            guard let self = self else { return }
            self.ajustesPanel.modo = (self.ajustesPanel.modo == .acoplado) ? .flotante : .acoplado
            self.ajustesPanel.visible = true
            self.aplicarReparto()
        })
        
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        // También al girar el iPad: al pasar a vertical puede dejar de caber el
        // reparto y el panel tiene que pasar a cubrir la pantalla.
        aplicarReparto()
        posicionarBarraFlotanteSiHaceFalta()
        posicionarSatelites()
    }

    /// Coloca lienzo y panel donde diga el núcleo. Ni un número se decide aquí.
    private func aplicarReparto() {
        let total = view.bounds.width
        guard total > 0 else { return }

        panelCasio.view.isHidden = !ajustesPanel.visible
        panelCasio.modo = ajustesPanel.modo
        if ajustesPanel.visible { view.bringSubviewToFront(panelCasio.view) }
        
        if ajustesPanel.modo == .flotante {
            // El lienzo ocupa todo
            lienzoIzquierda?.constant = 0
            lienzoDerecha?.constant = 0
            
            // El panel Casio flota
            panelCasio.mostrarDivisor = false
            let margen: CGFloat = 12
            let anchoDisponible = max(1, total - margen * 2)
            let altoDisponible = max(1, view.bounds.height - margen * 2)
            let w = min(ajustesPanel.tamanoFlotante.width, anchoDisponible)
            let h = min(ajustesPanel.tamanoFlotante.height, altoDisponible)
            let cxPropuesto = ajustesPanel.centroFlotanteNormalizado.x * total
            let cyPropuesto = ajustesPanel.centroFlotanteNormalizado.y * view.bounds.height
            let cx = min(max(cxPropuesto, margen + w / 2),
                         max(margen + w / 2, total - margen - w / 2))
            let cy = min(max(cyPropuesto, margen + h / 2),
                         max(margen + h / 2,
                             view.bounds.height - margen - h / 2))
            
            panelCasio.view.frame = CGRect(x: cx - w/2, y: cy - h/2, width: w, height: h)
        } else {
            // Modo acoplado
            let r = ajustesPanel.reparto(anchoTotal: total)
            panelCasio.aLaIzquierda = ajustesPanel.aLaIzquierda
            panelCasio.maxFraccion = ajustesPanel.maxFraccion
            panelCasio.mostrarDivisor = !ajustesPanel.cubreTodo(anchoTotal: total)
            panelCasio.view.frame = CGRect(x: r.panel.x, y: 0,
                                           width: r.panel.ancho, height: view.bounds.height)

            lienzoIzquierda?.constant = r.lienzo.x
            lienzoDerecha?.constant = r.lienzo.x + r.lienzo.ancho - total
        }
    }
    
    @objc private func arrastrarPanelCasio(_ gesto: UIPanGestureRecognizer) {
        guard ajustesPanel.modo == .flotante else { return }
        let traslacion = gesto.translation(in: view)
        
        if gesto.state == .changed || gesto.state == .began {
            let margen: CGFloat = 12
            let medioAncho = panelCasio.view.bounds.width / 2
            let medioAlto = panelCasio.view.bounds.height / 2
            let cx = min(max(panelCasio.view.center.x + traslacion.x,
                             margen + medioAncho),
                         max(margen + medioAncho,
                             view.bounds.width - margen - medioAncho))
            let cy = min(max(panelCasio.view.center.y + traslacion.y,
                             margen + medioAlto),
                         max(margen + medioAlto,
                             view.bounds.height - margen - medioAlto))
            panelCasio.view.center = CGPoint(x: cx, y: cy)
            gesto.setTranslation(.zero, in: view)
        } else if gesto.state == .ended || gesto.state == .cancelled {
            let cxNorm = panelCasio.view.center.x / view.bounds.width
            let cyNorm = panelCasio.view.center.y / view.bounds.height
            ajustesPanel.centroFlotanteNormalizado = CGPoint(x: cxNorm, y: cyNorm)
        }
    }

    // MARK: - Barra

    private func construirBarra() -> UIView {
        let hosting = UIHostingController(rootView: BarraSuperiorApp(
            estado: self.estadoBarra,
            colores: self.colores,
            alTocarDeshacer: { [weak self] in self?.deshacer(self as Any) },
            alTocarLimpiar: { [weak self] in self?.limpiar(self as Any) },
            // Un toque abre/cierra inmediatamente el panel flotante. Las
            // operaciones avanzadas siguen disponibles desde la command palette
            // mediante `.herramientasCasio`.
            alAlternarCalculadora: { [weak self] in
                guard let self else { return }
                self.alternarCasio(self)
            },
            alTocarIndice: { [weak self] in self?.mostrarIndice() },
            alCerrar: { [weak self] in 
                self?.alCambiarTituloDocumento?("Cerrar")
                self?.dismiss(animated: true)
            }
        ))
        hosting.view.backgroundColor = .clear
        return hosting.view
    }

    private func mostrarIndice() {
        let altoPagina: CGFloat = max(CGFloat(lienzo.sesion.altoPagina), 1000)
        let temas = lienzo.sesion.generarIndiceAutomatico(altoPagina: altoPagina)
        
        let panel = PanelIndiceTemas(temas: temas, alSeleccionarTema: { [weak self] tema in
            self?.navegador.irA(yOffset: tema.yOffset)
        }, alCerrar: { [weak self] in
            self?.dismiss(animated: true)
        })
        
        let hc = UIHostingController(rootView: panel)
        hc.modalPresentationStyle = .popover
        if let popover = hc.popoverPresentationController {
            popover.sourceView = barraPrincipal
            popover.sourceRect = barraPrincipal.bounds
        }
        present(hc, animated: true)
    }

    private func boton(_ simbolo: String, _ accion: Selector) -> UIButton {
        let boton = UIButton(type: .system)
        boton.setImage(UIImage(systemName: simbolo), for: .normal)
        boton.addTarget(self, action: accion, for: .touchUpInside)
        return boton
    }

    private func botonAccion(_ accion: AccionIPad,
                             tituloVisible: String? = nil) -> UIButton {
        let boton: UIButton
        if let tituloVisible {
            var configuracion = UIButton.Configuration.tinted()
            configuracion.title = tituloVisible
            configuracion.image = UIImage(systemName: accion.simbolo)
            configuracion.imagePadding = 6
            boton = UIButton(type: .system)
            boton.configuration = configuracion
        } else {
            boton = UIButton(type: .system)
            boton.setImage(UIImage(systemName: accion.simbolo), for: .normal)
        }
        boton.accessibilityLabel = accion.nombre
        boton.accessibilityIdentifier = accion.rawValue
        boton.addTarget(self, action: #selector(ejecutarBotonAccion(_:)),
                        for: .touchUpInside)
        return boton
    }

    private func configurarBotonControlBarra() {
        var configuracion = UIButton.Configuration.plain()
        configuracion.image = UIImage(systemName: "slider.horizontal.3")
        botonControlBarra.configuration = configuracion
        botonControlBarra.accessibilityLabel = "Personalizar barra"
        botonControlBarra.showsMenuAsPrimaryAction = true

        let posiciones = UbicacionBarraIPad.allCases.map { ubicacion in
            UIAction(title: ubicacion.nombre,
                     image: UIImage(systemName: ubicacion.simbolo),
                     state: configuracionBarra.ubicacion == ubicacion ? .on : .off) {
                [weak self] _ in self?.moverBarra(a: ubicacion)
            }
        }
        let tamano = UIAction(
            title: configuracionInterfaz.barraCompacta
                ? "Maximizar" : "Minimizar",
            image: UIImage(systemName: configuracionInterfaz.barraCompacta
                ? "rectangle.expand.vertical" : "rectangle.compress.vertical")) {
            [weak self] _ in self?.alternarTamanoBarra()
        }
        let ocultar = UIAction(
            title: "Ocultar barra", image: UIImage(systemName: "eye.slash")) {
            [weak self] _ in self?.ocultarBarraPersonalizable()
        }
        let editar = UIAction(
            title: "Editar herramientas…",
            image: UIImage(systemName: "square.grid.3x3.square")) {
            [weak self] _ in self?.abrirPersonalizacionBarra()
        }
        let apariencia = UIAction(
            title: "Apariencia…",
            image: UIImage(systemName: PreferenciasAparienciaTintaIPad.actual.simbolo)) {
            [weak self] _ in self?.abrirApariencia()
        }
        botonControlBarra.menu = UIMenu(children: [
            UIMenu(title: "Mover barra", children: posiciones),
            tamano, apariencia, editar, ocultar,
        ])
    }

    private func configurarBotonRecuperacionBarra() {
        var configuracion = UIButton.Configuration.filled()
        configuracion.image = UIImage(systemName: "pencil.tip.crop.circle")
        configuracion.cornerStyle = .capsule
        configuracion.baseBackgroundColor = .systemBlue
        configuracion.baseForegroundColor = .white
        botonRecuperarBarra.configuration = configuracion
        botonRecuperarBarra.accessibilityLabel = "Mostrar barra de herramientas"
        botonRecuperarBarra.layer.shadowColor = UIColor.black.cgColor
        botonRecuperarBarra.layer.shadowOpacity = 0.2
        botonRecuperarBarra.layer.shadowRadius = 8
        botonRecuperarBarra.layer.shadowOffset = CGSize(width: 0, height: 3)
        botonRecuperarBarra.addTarget(
            self, action: #selector(mostrarBarraPersonalizable),
            for: .touchUpInside)
        botonRecuperarBarra.menu = UIMenu(children: [
            UIAction(title: "Personalizar…",
                     image: UIImage(systemName: "slider.horizontal.3")) {
                [weak self] _ in self?.abrirPersonalizacionBarra()
            },
        ])
    }

    private func moverBarra(a ubicacion: UbicacionBarraIPad) {
        configuracionBarra.ubicacion = ubicacion
        CentroExperienciaIPad.compartido.actualizarBarra(configuracionBarra)
        reconstruirBarraPrincipal()
    }

    private func alternarTamanoBarra() {
        configuracionInterfaz.barraCompacta.toggle()
        configuracionInterfaz.mostrarBarra = true
        CentroExperienciaIPad.compartido.actualizarInterfaz(configuracionInterfaz)
        reconstruirBarraPrincipal()
    }

    private func ocultarBarraPersonalizable() {
        configuracionInterfaz.mostrarBarra = false
        CentroExperienciaIPad.compartido.actualizarInterfaz(configuracionInterfaz)
        aplicarInterfaz(configuracionInterfaz)
        estadoBarra.$herramientaActiva.sink { [weak self] h in self?.lienzo.herramienta = h }.store(in: &observadoresBarra)
        estadoBarra.$colorActivo.sink { [weak self] c in self?.lienzo.colorPluma = c }.store(in: &observadoresBarra)
    }

    @objc private func mostrarBarraPersonalizable() {
        configuracionInterfaz.mostrarBarra = true
        CentroExperienciaIPad.compartido.actualizarInterfaz(configuracionInterfaz)
        aplicarInterfaz(configuracionInterfaz)
        estadoBarra.$herramientaActiva.sink { [weak self] h in self?.lienzo.herramienta = h }.store(in: &observadoresBarra)
        estadoBarra.$colorActivo.sink { [weak self] c in self?.lienzo.colorPluma = c }.store(in: &observadoresBarra)
    }

    private func abrirPersonalizacionBarra() {
        let panel = PanelPersonalizacionBarraIPad()
        panel.alCambiar = { [weak self] instantanea in
            guard let self else { return }
            self.configuracionInterfaz = instantanea.interfaz
            self.configuracionBarra = instantanea.barra
            self.reconstruirBarraPrincipal()
            self.reconstruirSatelites()
        }
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: !configuracionInterfaz.reducirMovimiento)
    }

    private func abrirApariencia() {
        let panel = PanelAparienciaTintaIPad()
        panel.alCambiar = { [weak self] apariencia in
            self?.aplicarApariencia(apariencia)
        }
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: !configuracionInterfaz.reducirMovimiento)
    }

    @objc private func cambiarAparienciaDesdePreferencias(_ nota: Notification) {
        let apariencia = nota.object as? AparienciaTintaIPad
            ?? PreferenciasAparienciaTintaIPad.actual
        aplicarApariencia(apariencia)
    }

    private func aplicarApariencia(_ apariencia: AparienciaTintaIPad) {
        overrideUserInterfaceStyle = apariencia.estiloInterfaz
        if let ventana = view.window {
            PreferenciasAparienciaTintaIPad.aplicarEnVentana(ventana)
        }
        filtroCuidadoVisual.backgroundColor = apariencia.colorFiltroLienzo
        filtroCuidadoVisual.isHidden = apariencia != .cuidadoVisual
            && apariencia != .sol
        UIAccessibility.post(notification: .layoutChanged, argument: nil)
    }

    private func instalarRestriccionesBarra() {
        NSLayoutConstraint.deactivate(restriccionesBarra)
        restriccionesBarra.removeAll()
        barraCentroX = nil
        barraCentroY = nil
        // Nueva barra anclada superior completa
        restriccionesBarra = [
            barraPrincipal.topAnchor.constraint(equalTo: view.topAnchor),
            barraPrincipal.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            barraPrincipal.trailingAnchor.constraint(equalTo: view.trailingAnchor)
        ]
        NSLayoutConstraint.activate(restriccionesBarra)
    }

    private func posicionarBarraFlotanteSiHaceFalta() {
        guard configuracionBarra.ubicacion == .flotante, !arrastrandoBarra,
              let centroX = barraCentroX, let centroY = barraCentroY else { return }
        let marco = view.safeAreaLayoutGuide.layoutFrame
        guard marco.width > 0, marco.height > 0 else { return }
        centroX.constant = CGFloat(configuracionBarra.centroFlotanteX - 0.5)
            * marco.width
        centroY.constant = CGFloat(configuracionBarra.centroFlotanteY - 0.5)
            * marco.height
    }

    @objc private func arrastrarBarra(_ gesto: UIPanGestureRecognizer) {
        guard configuracionBarra.ubicacion == .flotante,
              let centroX = barraCentroX, let centroY = barraCentroY else { return }
        if gesto.state == .began { arrastrandoBarra = true }
        let traslacion = gesto.translation(in: view)
        centroX.constant += traslacion.x
        centroY.constant += traslacion.y
        gesto.setTranslation(.zero, in: view)
        view.layoutIfNeeded()

        guard gesto.state == .ended || gesto.state == .cancelled else { return }
        let marco = view.safeAreaLayoutGuide.layoutFrame
        guard marco.width > 0, marco.height > 0 else {
            arrastrandoBarra = false
            return
        }
        let mitad = CGSize(width: barraPrincipal.bounds.width / 2,
                           height: barraPrincipal.bounds.height / 2)
        let x = min(max(barraPrincipal.center.x, marco.minX + mitad.width),
                    marco.maxX - mitad.width)
        let y = min(max(barraPrincipal.center.y, marco.minY + mitad.height),
                    marco.maxY - mitad.height)
        configuracionBarra.centroFlotanteX = Double((x - marco.minX) / marco.width)
        configuracionBarra.centroFlotanteY = Double((y - marco.minY) / marco.height)
        CentroExperienciaIPad.compartido.actualizarBarra(configuracionBarra)
        arrastrandoBarra = false
        posicionarBarraFlotanteSiHaceFalta()
    }

    private func reconstruirSatelites() {
        botonesSatelite.values.forEach { $0.removeFromSuperview() }
        botonesSatelite.removeAll()
        for satelite in configuracionBarra.satelites {
            let boton = BotonSateliteBarraIPad(satelite)
            boton.addTarget(self, action: #selector(ejecutarSatelite(_:)),
                            for: .touchUpInside)
            boton.addGestureRecognizer(UIPanGestureRecognizer(
                target: self, action: #selector(arrastrarSatelite(_:))))
            boton.menu = UIMenu(children: [
                UIAction(title: "Acoplar a la barra",
                         image: UIImage(systemName: "rectangle.and.hand.point.up.left")) {
                    [weak self] _ in self?.acoplarSatelite(satelite.id)
                },
                UIAction(title: "Eliminar satélite",
                         image: UIImage(systemName: "trash"),
                         attributes: .destructive) {
                    [weak self] _ in self?.eliminarSatelite(satelite.id)
                },
            ])
            boton.frame.size = CGSize(width: 52, height: 52)
            view.addSubview(boton)
            botonesSatelite[satelite.id] = boton
        }
        posicionarSatelites()
        view.bringSubviewToFront(botonRecuperarBarra)
    }

    private func posicionarSatelites() {
        let marco = view.safeAreaLayoutGuide.layoutFrame
        guard marco.width > 0, marco.height > 0 else { return }
        for satelite in configuracionBarra.satelites {
            guard let boton = botonesSatelite[satelite.id],
                  boton.gestureRecognizers?.contains(where: {
                      $0.state == .began || $0.state == .changed
                  }) != true else { continue }
            boton.center = CGPoint(
                x: min(max(marco.minX + CGFloat(satelite.x) * marco.width,
                           marco.minX + 26), marco.maxX - 26),
                y: min(max(marco.minY + CGFloat(satelite.y) * marco.height,
                           marco.minY + 26), marco.maxY - 26))
        }
    }

    @objc private func ejecutarSatelite(_ boton: BotonSateliteBarraIPad) {
        ejecutar(boton.accion)
    }

    @objc private func arrastrarSatelite(_ gesto: UIPanGestureRecognizer) {
        guard let boton = gesto.view as? BotonSateliteBarraIPad else { return }
        let traslacion = gesto.translation(in: view)
        boton.center.x += traslacion.x
        boton.center.y += traslacion.y
        gesto.setTranslation(.zero, in: view)
        let marco = view.safeAreaLayoutGuide.layoutFrame
        guard marco.width > 0, marco.height > 0 else { return }
        boton.center.x = min(max(boton.center.x, marco.minX + 26), marco.maxX - 26)
        boton.center.y = min(max(boton.center.y, marco.minY + 26), marco.maxY - 26)
        guard gesto.state == .ended || gesto.state == .cancelled else { return }
        let x = Double((boton.center.x - marco.minX) / marco.width)
        let y = Double((boton.center.y - marco.minY) / marco.height)
        CentroExperienciaIPad.compartido.moverSatelite(
            boton.sateliteID, x: x, y: y)
        configuracionBarra = CentroExperienciaIPad.compartido.instantanea().barra
    }

    private func acoplarSatelite(_ id: UUID) {
        guard let satelite = configuracionBarra.satelites.first(where: {
            $0.id == id
        }) else { return }
        configuracionBarra.satelites.removeAll { $0.id == id }
        CentroExperienciaIPad.compartido.actualizarBarra(configuracionBarra)
        var orden = CentroExperienciaIPad.compartido.instantanea().ordenBarra
        if !orden.contains(satelite.accion) { orden.append(satelite.accion) }
        CentroExperienciaIPad.compartido.guardarOrdenBarra(orden)
        reconstruirBarraPrincipal()
        reconstruirSatelites()
    }

    private func eliminarSatelite(_ id: UUID) {
        configuracionBarra.satelites.removeAll { $0.id == id }
        CentroExperienciaIPad.compartido.actualizarBarra(configuracionBarra)
        reconstruirSatelites()
    }

    private func actualizarBotonHerramientaSimple() {
        let herramienta = lienzo.herramienta
        var configuracion = UIButton.Configuration.tinted()
        configuracion.title = herramienta.nombre
        configuracion.image = UIImage(systemName: herramienta.simbolo)
        configuracion.imagePadding = 6
        botonHerramientaSimple.configuration = configuracion
        botonHerramientaSimple.showsMenuAsPrimaryAction = true
        botonHerramientaSimple.accessibilityLabel = "Herramienta: \(herramienta.nombre)"
        botonHerramientaSimple.menu = UIMenu(
            title: "Herramienta",
            children: HerramientaLienzo.allCases.map { opcion in
                UIAction(title: opcion.nombre,
                         image: UIImage(systemName: opcion.simbolo),
                         state: opcion == herramienta ? .on : .off) { [weak self] _ in
                    self?.aplicarHerramienta(opcion)
                }
            })
    }

    private func actualizarBotonColorSimple() {
        botonColorSimple.configuration?.baseBackgroundColor = UIColor(
            argb: lienzo.colorPluma)
        botonColorSimple.accessibilityLabel = "Color del trazo"
    }

    @objc private func abrirPaletaSimple() {
        mostrarPaleta()
    }

    private func configurarBotonAgregarSimple() {
        var configuracion = UIButton.Configuration.tinted()
        configuracion.title = "Agregar"
        configuracion.image = UIImage(systemName: "plus")
        configuracion.imagePadding = 6
        botonAgregarSimple.configuration = configuracion
        botonAgregarSimple.showsMenuAsPrimaryAction = true
        botonAgregarSimple.accessibilityLabel = "Agregar contenido"
        botonAgregarSimple.menu = UIMenu(children: [
            UIAction(title: "Tomar foto", image: UIImage(systemName: "camera")) {
                [weak self] _ in self?.tomarFoto()
            },
            UIAction(title: "Elegir fotos", image: UIImage(systemName: "photo.on.rectangle")) {
                [weak self] _ in self?.abrirSelectorFotos()
            },
            UIAction(title: "Elegir una foto y recortarla",
                     image: UIImage(systemName: "crop.rotate")) {
                [weak self] _ in self?.abrirSelectorFotos(limite: 1, editar: true)
            },
            UIMenu(title: "Agregar fotos recientes",
                   image: UIImage(systemName: "clock.arrow.circlepath"), children: [
                UIAction(title: "Últimas 3") { [weak self] _ in
                    self?.importarFotosRecientes(3)
                },
                UIAction(title: "Últimas 5") { [weak self] _ in
                    self?.importarFotosRecientes(5)
                },
                UIAction(title: "Últimas 10") { [weak self] _ in
                    self?.importarFotosRecientes(10)
                },
            ]),
            UIAction(title: "Texto", image: UIImage(systemName: "keyboard")) {
                [weak self] _ in self?.ejecutar(.teclado)
            },
            UIAction(title: "Dictado", image: UIImage(systemName: "mic")) {
                [weak self] _ in self?.ejecutar(.voz)
            },
            UIAction(title: "Importar archivo",
                     image: UIImage(systemName: "square.and.arrow.down")) {
                [weak self] _ in self?.ejecutar(.importar)
            },
            UIAction(title: "Escanear y enderezar documento",
                     image: UIImage(systemName: "camera.viewfinder")) {
                [weak self] _ in self?.ejecutar(.escanearPizarra)
            },
        ])
    }

    private func configurarBotonLienzoSimple() {
        var configuracion = UIButton.Configuration.tinted()
        configuracion.title = modoPagina.nombreCorto
        configuracion.image = UIImage(systemName: modoPagina.simbolo)
        configuracion.imagePadding = 6
        botonLienzoSimple.configuration = configuracion
        botonLienzoSimple.showsMenuAsPrimaryAction = true
        botonLienzoSimple.accessibilityLabel =
            "Lienzo: \(modoPagina.nombre). Cambiar tamaño, dirección o papel"

        let modosRapidos: [ModoPaginaLienzo] = [
            .paginadoFijo, .hibrido, .horizontalInfinito, .infinitoXY,
        ]
        let menuModos = UIMenu(title: "Tamaño y crecimiento", children:
            modosRapidos.map { modo in
                UIAction(title: modo.nombre,
                         image: UIImage(systemName: modo.simbolo),
                         state: modoPagina == modo ? .on : .off) { [weak self] _ in
                    self?.aplicarModoPaginaRapido(modo)
                }
            })
        let pautasRapidas: [Pauta] = [.ninguna, .rayado, .cuadricula, .puntos]
        let menuPauta = UIMenu(title: "Pauta rápida",
                               image: UIImage(systemName: "square.grid.3x3"),
                               children: pautasRapidas.map { pauta in
            UIAction(title: pauta.nombre,
                     state: pautaActual == pauta ? .on : .off) { [weak self] _ in
                self?.aplicarPautaRapida(pauta)
            }
        })
        let editarBordes = UIAction(
            title: "Mostrar bordes para cambiar tamaño",
            image: UIImage(systemName: "arrow.up.left.and.arrow.down.right"),
            state: navegador.mostrarTiradoresTamano ? .on : .off) { [weak self] _ in
                guard let self else { return }
                self.navegador.mostrarTiradoresTamano.toggle()
                self.configurarBotonLienzoSimple()
            }
        let avanzado = UIAction(
            title: "Lienzo y papel…",
            image: UIImage(systemName: "slider.horizontal.3")) { [weak self] _ in
                guard let self else { return }
                self.abrirLienzo(self.botonLienzoSimple)
            }
        botonLienzoSimple.menu = UIMenu(children: [
            menuModos, menuPauta, editarBordes, avanzado,
        ])
    }

    private func aplicarModoPaginaRapido(_ modo: ModoPaginaLienzo) {
        modoPagina = modo
        navegador.aplicarModo(modo)
        configurarBotonLienzoSimple()
        autoguardado.marcarCambio()
        programarGuardadoEstado()
    }

    private func aplicarPautaRapida(_ pauta: Pauta) {
        pautaActual = pauta
        aplicarPapel()
        configurarBotonLienzoSimple()
        autoguardado.marcarCambio()
    }

    private func refrescar() {
        let modo: String
        switch lienzo.modoPrediccion {
        case .apagada: modo = "apagada"
        case .sistema: modo = "predictedTouches"
        case .nucleo: modo = "núcleo C++"
        }
        etiqueta.text = """
            latencia    P50 \(String(format: "%.1f", lienzo.latenciaMs)) ms  (evento → fin GPU)
            colas       P95 \(String(format: "%.1f", lienzo.latenciaP95Ms)) · P99 \(String(format: "%.1f", lienzo.latenciaP99Ms)) · máx \(String(format: "%.1f", lienzo.latenciaMaximaMs)) ms
            muestreo    \(String(format: "%.0f", lienzo.sesion.frecuenciaHz)) Hz
            predicción  \(modo), \(lienzo.sesion.muestrasPredichas) puntos
            trazos      \(lienzo.sesion.cantidadTrazos)
            """
    }

    // MARK: - Herramientas

    @objc private func elegirColor(_ boton: UIButton) {
        colorRetornoTemporal = nil
        lienzo.colorPluma = colores[boton.tag]
        actualizarBotonColorSimple()
    }

    @objc private func elegirColorTemporal(_ gesto: UILongPressGestureRecognizer) {
        guard gesto.state == .began,
              let boton = gesto.view as? UIButton,
              colores.indices.contains(boton.tag) else { return }
        let temporal = colores[boton.tag]
        guard temporal != lienzo.colorPluma else { return }
        if colorRetornoTemporal == nil { colorRetornoTemporal = lienzo.colorPluma }
        lienzo.colorPluma = temporal
        navigationItem.prompt = "Color temporal: vuelve al terminar el siguiente trazo"
        UIImpactFeedbackGenerator(style: .light).impactOccurred()
    }

    private func restaurarColorTemporalSiHaceFalta() {
        guard let anterior = colorRetornoTemporal else { return }
        colorRetornoTemporal = nil
        lienzo.colorPluma = anterior
        navigationItem.prompt = nil
        UIAccessibility.post(notification: .announcement,
                             argument: "Se restauró el color anterior")
    }

    @objc private func cambiarGrosor(_ deslizador: UISlider) {
        // El grosor de la pluma. **No** toca el puntero: son dos ajustes
        // distintos y el del puntero está en la pantalla de ajustes.
        lienzo.anchoPluma = deslizador.value
    }

    @objc private func cambiarPrediccion(_ selector: UISegmentedControl) {
        lienzo.modoPrediccion = ModoPrediccion(rawValue: selector.selectedSegmentIndex) ?? .sistema
        refrescar()
    }

    @objc private func cambiarHerramienta(_ selector: UISegmentedControl) {
        guard let herramienta = HerramientaLienzo(rawValue: selector.selectedSegmentIndex) else {
            return
        }
        aplicarHerramienta(herramienta)
    }

    private func aplicarHerramienta(_ herramienta: HerramientaLienzo) {
        lienzo.herramienta = herramienta
        // En modo lazo, un dedo selecciona o mueve y dos dedos desplazan el
        // documento. Así el UIScrollView no cancela el arrastre de la selección.
        navegador.scroll.panGestureRecognizer.minimumNumberOfTouches =
            herramienta == .lazo ? 2 : 1
        let borrandoObjeto = (herramienta == .borradorObjeto
                              || herramienta == .borradorParcial)
            && lienzo.ajustesBorrador.modo == .objeto
        navegador.capaElementos.isUserInteractionEnabled = herramienta != .lazo
            && !borrandoObjeto
        if borrandoObjeto {
            navegador.capaBloques.isUserInteractionEnabled = false
            navegador.capaTecnica.isUserInteractionEnabled = false
        } else {
            navegador.aplicarCapas(RepositorioCapas.compartido.estado(
                lienzo.sesion.documentoPersistenteId))
        }
        selectorHerramienta.selectedSegmentIndex = herramienta.rawValue
        actualizarBotonHerramientaSimple()
        navigationItem.prompt = nil
        CentroExperienciaIPad.compartido.registrarUso(accion(para: herramienta))
        guardarEstadoExacto()
    }

    @objc private func mantenerBorrador(_ gesto: UILongPressGestureRecognizer) {
        guard gesto.state == .began else { return }
        let punto = gesto.location(in: selectorHerramienta)
        let segmentos = max(selectorHerramienta.numberOfSegments, 1)
        let indice = min(max(Int(punto.x / max(selectorHerramienta.bounds.width, 1)
            * CGFloat(segmentos)), 0), segmentos - 1)
        guard indice == HerramientaLienzo.borradorObjeto.rawValue
                || indice == HerramientaLienzo.borradorParcial.rawValue else { return }
        abrirBorradorAvanzado()
    }

    private func abrirBorradorAvanzado() {
        let panel = PanelBorradorAvanzadoIPad(ajustes: lienzo.ajustesBorrador)
        panel.alAplicar = { [weak self] ajustes in
            guard let self else { return }
            self.lienzo.ajustesBorrador = ajustes
            self.aplicarHerramienta(ajustes.modo == .pixel
                ? .borradorParcial : .borradorObjeto)
        }
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: true)
    }

    private func actualizarControlesSeleccion(_ cantidad: Int) {
        guard cantidad > 0 else {
            navigationItem.prompt = nil
            navigationItem.rightBarButtonItem = nil
            return
        }
        navigationItem.prompt = "\(cantidad) elemento\(cantidad == 1 ? "" : "s") seleccionado\(cantidad == 1 ? "" : "s")"
        let coloresMenu: [UIMenuElement] = colores.enumerated().map { entrada in
            let (indice, color) = entrada
            return UIAction(title: "Color \(indice + 1)",
                     image: UIImage(systemName: "circle.fill")) { [weak self] _ in
                self?.lienzo.recolorearSeleccion(color)
                self?.autoguardado.marcarCambio()
            }
        }
        let recolorear = UIMenu(title: "Recolorear", image: UIImage(systemName: "paintpalette"),
                                children: coloresMenu)
        let ajustes = UIAction(title: "Ajustes del lazo",
                               image: UIImage(systemName: "slider.horizontal.3")) {
            [weak self] _ in self?.abrirAjustesSeleccion()
        }
        let seleccionarTodo = UIAction(title: "Seleccionar todo",
                                       image: UIImage(systemName: "rectangle.dashed")) {
            [weak self] _ in self?.lienzo.seleccionarTodo()
        }
        let invertir = UIAction(title: "Invertir selección",
                                image: UIImage(systemName: "arrow.triangle.2.circlepath")) {
            [weak self] _ in self?.lienzo.invertirSeleccion()
        }
        let transformar = UIMenu(title: "Transformar",
                                 image: UIImage(systemName: "arrow.up.left.and.arrow.down.right"),
                                 children: [
            UIAction(title: "Aumentar 10 %") { [weak self] _ in
                self?.lienzo.transformarSeleccion(escalaX: 1.1, escalaY: 1.1)
            },
            UIAction(title: "Reducir 10 %") { [weak self] _ in
                self?.lienzo.transformarSeleccion(escalaX: 0.9, escalaY: 0.9)
            },
            UIAction(title: "Ensachar 10 %") { [weak self] _ in
                self?.lienzo.transformarSeleccion(escalaX: 1.1, escalaY: 1)
            },
            UIAction(title: "Aumentar alto 10 %") { [weak self] _ in
                self?.lienzo.transformarSeleccion(escalaX: 1, escalaY: 1.1)
            },
            UIAction(title: "Girar 15° a la izquierda") { [weak self] _ in
                self?.lienzo.transformarSeleccion(rotacion: -.pi / 12)
            },
            UIAction(title: "Girar 15° a la derecha") { [weak self] _ in
                self?.lienzo.transformarSeleccion(rotacion: .pi / 12)
            },
            UIAction(title: "Girar 90°") { [weak self] _ in
                self?.lienzo.transformarSeleccion(rotacion: .pi / 2)
            },
            UIAction(title: "Reflejar horizontal") { [weak self] _ in
                self?.lienzo.transformarSeleccion(reflejarHorizontal: true)
            },
            UIAction(title: "Reflejar vertical") { [weak self] _ in
                self?.lienzo.transformarSeleccion(reflejarVertical: true)
            },
        ])
        let alinear = UIMenu(title: "Alinear y distribuir",
                             image: UIImage(systemName: "align.horizontal.left"),
                             children: AlineacionSeleccionIPad.allCases.map { valor in
            UIAction(title: valor.rawValue) { [weak self] _ in
                self?.lienzo.alinearSeleccion(valor)
            }
        } + DistribucionSeleccionIPad.allCases.map { valor in
            UIAction(title: valor.rawValue) { [weak self] _ in
                self?.lienzo.distribuirSeleccion(valor)
            }
        })
        let organizar = UIMenu(title: "Organizar",
                               image: UIImage(systemName: "square.3.layers.3d"),
                               children: [
            UIAction(title: "Duplicar", image: UIImage(systemName: "plus.square.on.square")) {
                [weak self] _ in self?.lienzo.duplicarSeleccion()
            },
            UIAction(title: "Traer al frente", image: UIImage(systemName: "square.3.layers.3d.top.filled")) {
                [weak self] _ in self?.lienzo.ordenarSeleccion(alFrente: true)
            },
            UIAction(title: "Enviar atrás", image: UIImage(systemName: "square.3.layers.3d.bottom.filled")) {
                [weak self] _ in self?.lienzo.ordenarSeleccion(alFrente: false)
            },
            UIAction(title: "Agrupar", image: UIImage(systemName: "square.stack.3d.up")) {
                [weak self] _ in self?.lienzo.agruparSeleccion()
            },
            UIAction(title: "Desagrupar", image: UIImage(systemName: "square.stack.3d.down.right")) {
                [weak self] _ in self?.lienzo.desagruparSeleccion()
            },
            UIAction(title: "Bloquear", image: UIImage(systemName: "lock")) {
                [weak self] _ in self?.lienzo.bloquearSeleccion(true)
            },
            UIAction(title: "Desbloquear", image: UIImage(systemName: "lock.open")) {
                [weak self] _ in self?.lienzo.bloquearSeleccion(false)
            },
            UIAction(title: "Etiquetas y accesibilidad",
                     image: UIImage(systemName: "tag")) { [weak self] _ in
                self?.abrirEtiquetasSeleccion()
            },
        ])
        let tiposSemanticos: [TipoMarcacionSemanticaIPad] = [
            .formula, .definicion, .importante, .examen, .duda,
            .ejemplo, .entendido, .pregunta,
        ]
        let semantica = UIMenu(
            title: "Significado y enlaces",
            image: UIImage(systemName: "tag.square"),
            children: tiposSemanticos.map { tipo in
                UIAction(title: "Marcar como \(tipo.nombre)",
                         image: UIImage(systemName: tipo.simbolo)) { [weak self] _ in
                    self?.marcarSeleccionSemantica(tipo)
                }
            } + [
                UIAction(title: "Convertir en encabezado",
                         image: UIImage(systemName: "textformat.size.larger")) {
                    [weak self] _ in self?.convertirSeleccionEnEncabezado()
                },
                UIAction(title: "Crear referencia cruzada",
                         image: UIImage(systemName: "arrow.triangle.branch")) {
                    [weak self] _ in self?.crearReferenciaCruzada()
                },
                UIAction(title: "Ver outline y backlinks",
                         image: UIImage(systemName: "list.bullet.indent")) {
                    [weak self] _ in self?.abrirSemanticaDocumento()
                },
            ])
        let recall = UIMenu(
            title: "Tapar respuestas",
            image: UIImage(systemName: "rectangle.fill.on.rectangle.fill"),
            children: EstiloCubiertaRecallIPad.allCases.map { estilo in
                UIAction(title: estilo.nombre) { [weak self] _ in
                    self?.crearCubiertaRecall(estilo)
                }
            } + [
                UIAction(title: "Administrar Active Recall",
                         image: UIImage(systemName: "eye.slash")) {
                    [weak self] _ in self?.abrirPanelRecall()
                },
            ])
        let tarjetaManuscrita = UIMenu(
            title: "Flashcard manuscrita",
            image: UIImage(systemName: "rectangle.on.rectangle"),
            children: [
                UIAction(
                    title: frenteTarjetaPendiente == nil
                        ? "Guardar selección como frente"
                        : "Reemplazar frente pendiente",
                    image: UIImage(systemName: "rectangle.topthird.inset.filled")) {
                    [weak self] _ in self?.guardarFrenteTarjetaManuscrita()
                },
                UIAction(
                    title: "Usar selección como reverso",
                    image: UIImage(systemName: "rectangle.bottomthird.inset.filled"),
                    attributes: frenteTarjetaPendiente == nil ? .disabled : []) {
                    [weak self] _ in self?.completarTarjetaManuscrita()
                },
                UIAction(title: "Cancelar frente pendiente",
                         image: UIImage(systemName: "xmark"),
                         attributes: frenteTarjetaPendiente == nil ? .disabled : []) {
                    [weak self] _ in self?.cancelarFrenteTarjetaManuscrita()
                },
            ])
        let copiar = UIMenu(title: "Copiar / interpretar",
                            image: UIImage(systemName: "doc.on.doc"), children: [
            UIAction(title: "Copiar como imagen", image: UIImage(systemName: "photo.on.rectangle")) {
                [weak self] _ in self?.copiarSeleccionComoImagen()
            },
            UIAction(title: "Copiar texto de cajas", image: UIImage(systemName: "textformat")) {
                [weak self] _ in self?.copiarTextoSeleccionado()
            },
            UIAction(title: "OCR local de la selección", image: UIImage(systemName: "viewfinder")) {
                [weak self] _ in self?.copiarOCRSeleccion()
            },
            UIAction(title: "Copiar como LaTeX", image: UIImage(systemName: "function")) {
                [weak self] _ in self?.copiarLaTeXSeleccion()
            },
            UIAction(title: "Interpretar como circuito",
                     image: UIImage(systemName: "point.3.connected.trianglepath.dotted")) {
                [weak self] _ in self?.reconocerCircuitoSeleccion()
            },
        ])
        let exportar = UIMenu(title: "Exportar selección",
                              image: UIImage(systemName: "square.and.arrow.up"),
                              children: FormatoExportacionSeleccionIPad.allCases.map { formato in
            UIAction(title: formato.nombre) { [weak self] _ in
                self?.compartirSeleccion(formato)
            }
        })
        let eliminar = UIAction(title: "Eliminar selección",
                                image: UIImage(systemName: "trash"),
                                attributes: .destructive) { [weak self] _ in
            self?.lienzo.eliminarSeleccion()
            self?.autoguardado.marcarCambio()
        }
        let menu = UIMenu(title: "Selección", children: [
            ajustes, UIMenu(options: .displayInline,
                            children: [seleccionarTodo, invertir]),
            recolorear, transformar, alinear, organizar, semantica, recall,
            tarjetaManuscrita,
            copiar, exportar, eliminar,
        ])
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "ellipsis.circle"), menu: menu)
    }

    private func abrirAjustesSeleccion() {
        let panel = PanelAjustesSeleccionIPad(lienzo: lienzo, colores: colores)
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: true)
    }

    private func abrirEtiquetasSeleccion() {
        let panel = PanelEtiquetasSeleccionIPad(lienzo: lienzo)
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: true)
    }

    private func tituloDirectoSeleccion(
        tipo: TipoMarcacionSemanticaIPad) -> String {
        let texto = ExportadorSeleccionIPad.textoDirecto(
            lienzo.seleccionActual, sesion: lienzo.sesion)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return texto.isEmpty ? tipo.nombre : String(texto.prefix(180))
    }

    private func marcarSeleccionSemantica(
        _ tipo: TipoMarcacionSemanticaIPad, titulo: String? = nil) {
        let claves = lienzo.clavesSeleccionActual
        let texto = titulo ?? tituloDirectoSeleccion(tipo: tipo)
        let documento = lienzo.sesion.documentoPersistenteId
        let curso = CentroExperienciaIPad.compartido.instantanea()
            .espacioActivo?.cursoId
        guard RepositorioSemanticaIPad.compartido.marcar(
            documento: documento, claves: claves, tipo: tipo,
            titulo: texto, region: lienzo.marcoSeleccionActual(),
            cursoId: curso) != nil else {
            avisar("Seleccione tinta, texto o medios antes de asignar significado.")
            return
        }
        let fuente = ReferenciaFuente(
            tipo: .apuntePropio, titulo: texto, cursoId: curso,
            documentoId: documento,
            elementoId: claves.first)
        CentroAcademico.compartido.agregarNodo(NodoConocimiento(
            titulo: texto, texto: texto, fuente: fuente,
            etiquetas: [tipo.rawValue]))
        CentroAcademico.compartido.transaccion { base in
            let evento: TipoEventoAcademico = tipo == .examen ? .examen
                : (tipo == .duda ? .mePerdi
                   : (tipo == .importante ? .importante : .marcador))
            base.timeline.append(EventoAcademico(
                tipo: evento, titulo: "\(tipo.nombre): \(texto)",
                cursoId: curso, referencia: fuente))
        }
        avisar("La selección quedó marcada como \(tipo.nombre); la tinta original no cambió.")
    }

    private func convertirSeleccionEnEncabezado() {
        let sugerido = tituloDirectoSeleccion(tipo: .encabezado)
        let alerta = UIAlertController(
            title: "Encabezado manuscrito",
            message: "La apariencia original permanece intacta y el título alimenta el outline.",
            preferredStyle: .alert)
        alerta.addTextField { $0.text = sugerido; $0.placeholder = "Título" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear encabezado", style: .default) {
            [weak self] _ in
            guard let titulo = alerta.textFields?.first?.text?.trimmingCharacters(
                in: .whitespacesAndNewlines), !titulo.isEmpty else { return }
            self?.marcarSeleccionSemantica(.encabezado, titulo: titulo)
        })
        present(alerta, animated: true)
    }

    private func crearReferenciaCruzada() {
        let alerta = UIAlertController(
            title: "Referencia cruzada",
            message: "Busque por ejemplo «clase 4», «Thévenin» o «RLC in:EL2113».",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Destino dentro de sus materiales" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Buscar", style: .default) {
            [weak self] _ in
            guard let self,
                  let consulta = alerta.textFields?.first?.text,
                  !consulta.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            else { return }
            let resultados = CentroAcademico.compartido.buscar(consulta, limite: 12)
            guard !resultados.isEmpty else {
                self.avisar("No se encontró un destino local; no se creó un enlace fantasma.")
                return
            }
            let selector = UIAlertController(
                title: "Elegir destino", message: nil,
                preferredStyle: .actionSheet)
            for resultado in resultados {
                selector.addAction(UIAlertAction(
                    title: resultado.fuente.titulo, style: .default) {
                    [weak self] _ in self?.guardarReferenciaCruzada(
                        resultado.fuente)
                })
            }
            selector.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
            selector.popoverPresentationController?.sourceView = self.view
            self.present(selector, animated: true)
        })
        present(alerta, animated: true)
    }

    private func guardarReferenciaCruzada(_ destino: ReferenciaFuente) {
        let documento = lienzo.sesion.documentoPersistenteId
        let claves = lienzo.clavesSeleccionActual
        let titulo = tituloDirectoSeleccion(tipo: .definicion)
        guard RepositorioSemanticaIPad.compartido.vincular(
            documento: documento, claves: claves,
            titulo: titulo, destino: destino) != nil else {
            avisar("La referencia ya existía o la selección dejó de estar disponible.")
            return
        }
        CentroExperienciaIPad.compartido.relacionar(
            origen: "seleccion:\(documento):\(claves.joined(separator: ","))",
            destino: "fuente:\(destino.id.uuidString)",
            tipo: .explica, cursoId: destino.cursoId,
            nota: titulo)
        avisar("Referencia creada hacia \(destino.titulo).")
    }

    private func abrirSemanticaDocumento() {
        let panel = PanelSemanticaDocumentoIPad(
            documento: lienzo.sesion.documentoPersistenteId)
        panel.alAbrirRegion = { [weak self] region in
            self?.navegador.mostrarRegion(
                region.insetBy(dx: -60, dy: -60), animado: true)
        }
        panel.alAbrirFuente = { [weak self] fuente in
            self?.abrirReferencia(fuente)
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func crearCubiertaRecall(_ estilo: EstiloCubiertaRecallIPad) {
        guard let region = lienzo.marcoSeleccionActual() else {
            avisar("Seleccione la respuesta o región que desea tapar.")
            return
        }
        let documento = lienzo.sesion.documentoPersistenteId
        let titulo = tituloDirectoSeleccion(tipo: .pregunta)
        let claves = lienzo.clavesSeleccionActual
        let examen = RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento).contains {
                guard $0.tipo == .examen,
                      let regionExamen = $0.region else { return false }
                return !regionExamen.intersection(region).isNull
            }
        if estilo.segmentablePorOCR,
           let regionRender = ExportadorSeleccionIPad.marco(
                lienzo.seleccionActual, sesion: lienzo.sesion),
           let imagen = try? ExportadorSeleccionIPad.imagen(
                seleccion: lienzo.seleccionActual, sesion: lienzo.sesion) {
            AnalizadorRecallVisualIPad.detectar(
                en: imagen, estilo: estilo, regionDocumento: regionRender) {
                    [weak self] fragmentos in
                    guard let self else { return }
                    let repositorio = RepositorioRecallIPad.compartido
                    if fragmentos.isEmpty {
                        _ = repositorio.agregar(
                            documento: documento, claves: claves, region: region,
                            estilo: estilo, titulo: titulo, automatica: true,
                            examen: examen)
                    } else {
                        for fragmento in fragmentos {
                            _ = repositorio.agregar(
                                documento: documento, claves: claves,
                                region: fragmento.region, estilo: estilo,
                                titulo: fragmento.texto, automatica: true,
                                examen: examen)
                        }
                    }
                    repositorio.activar(true, documento: documento)
                    self.navegador.capaRecall.recargar()
                    self.avisar(fragmentos.isEmpty
                        ? "Vision no separó tokens; se creó una cubierta regional reversible."
                        : "Se crearon \(fragmentos.count) cubiertas OCR locales para \(estilo.nombre.lowercased()).")
                }
            return
        }
        guard RepositorioRecallIPad.compartido.agregar(
            documento: documento, claves: lienzo.clavesSeleccionActual,
            region: region, estilo: estilo, titulo: titulo,
            examen: examen) != nil else {
            avisar("No se pudo crear una cubierta para esa región.")
            return
        }
        RepositorioRecallIPad.compartido.activar(true, documento: documento)
        navegador.capaRecall.recargar()
        avisar("Cubierta creada. Cada toque revela 25 %; el contenido original permanece intacto.")
    }

    private func abrirPanelRecall() {
        let panel = PanelRecallIPad(
            documento: lienzo.sesion.documentoPersistenteId)
        panel.alCambiar = { [weak self] in self?.navegador.capaRecall.recargar() }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func capturarCaraTarjetaManuscrita(
        nombre: String) throws -> CaraTarjetaManuscritaIPad {
        guard let region = lienzo.marcoSeleccionActual() else {
            throw NSError(domain: "Tinta.Tarjeta", code: 1,
                          userInfo: [NSLocalizedDescriptionKey:
                            "La selección no tiene una región válida."])
        }
        let imagen = try ExportadorSeleccionIPad.imagen(
            seleccion: lienzo.seleccionActual, sesion: lienzo.sesion)
        let adjunto = try RepositorioAdjuntos.compartido.importar(
            imagen: imagen, nombre: "\(nombre)-\(UUID().uuidString).png")
        let documento = lienzo.sesion.documentoPersistenteId
        let claves = lienzo.clavesSeleccionActual
        let curso = CentroExperienciaIPad.compartido.instantanea()
            .espacioActivo?.cursoId
        let fuente = ReferenciaFuente(
            tipo: .apuntePropio, titulo: nombre, cursoId: curso,
            documentoId: documento, elementoId: claves.first,
            hashArchivo: adjunto.hash)
        return CaraTarjetaManuscritaIPad(
            hashImagen: adjunto.hash, claves: claves,
            region: region, fuente: fuente)
    }

    private func guardarFrenteTarjetaManuscrita() {
        do {
            frenteTarjetaPendiente = try capturarCaraTarjetaManuscrita(
                nombre: "Frente manuscrito")
            actualizarControlesSeleccion(lienzo.seleccionActual.total)
            avisar("Frente guardado. Seleccione ahora el reverso y elija «Usar selección como reverso».")
        } catch { avisar(error.localizedDescription) }
    }

    private func completarTarjetaManuscrita() {
        guard let frente = frenteTarjetaPendiente else {
            avisar("Primero guarde una selección como frente.")
            return
        }
        do {
            let reverso = try capturarCaraTarjetaManuscrita(
                nombre: "Reverso manuscrito")
            let curso = frente.fuente.cursoId
            let flashcard = FlashcardAcademica(
                pregunta: "Recuerde la región manuscrita del frente",
                respuesta: "Compare con el reverso manuscrito",
                cursoId: curso, referencia: frente.fuente)
            CentroAcademico.compartido.transaccion {
                $0.flashcards.append(flashcard)
            }
            let documento = lienzo.sesion.documentoPersistenteId
            let cubiertas = RepositorioRecallIPad.compartido
                .cubiertas(documento).filter {
                    !$0.region.intersection(frente.region).isNull
                        || !$0.region.intersection(reverso.region).isNull
                }.map(\.id)
            RepositorioTarjetasManuscritasIPad.compartido.guardar(
                TarjetaManuscritaIPad(
                    flashcardId: flashcard.id, frente: frente,
                    reverso: reverso, cubiertasRecall: cubiertas))
            frenteTarjetaPendiente = nil
            actualizarControlesSeleccion(lienzo.seleccionActual.total)
            avisar("Tarjeta creada con frente y reverso manuscritos, fuente original y repetición espaciada.")
        } catch { avisar(error.localizedDescription) }
    }

    private func cancelarFrenteTarjetaManuscrita() {
        frenteTarjetaPendiente = nil
        actualizarControlesSeleccion(lienzo.seleccionActual.total)
        avisar("Se descartó el frente pendiente; los apuntes no cambiaron.")
    }

    private func copiarSeleccionComoImagen() {
        do {
            UIPasteboard.general.image = try ExportadorSeleccionIPad.imagen(
                seleccion: lienzo.seleccionActual, sesion: lienzo.sesion)
            avisar("La selección se copió como imagen sin modificar el original.")
        } catch { avisar(error.localizedDescription) }
    }

    private func copiarTextoSeleccionado() {
        let texto = ExportadorSeleccionIPad.textoDirecto(
            lienzo.seleccionActual, sesion: lienzo.sesion)
        guard !texto.isEmpty else {
            avisar("La selección no contiene cajas de texto. Use OCR local para tinta o imágenes.")
            return
        }
        UIPasteboard.general.string = texto
        avisar("Texto copiado.")
    }

    private func copiarOCRSeleccion() {
        ExportadorSeleccionIPad.reconocerTextoLocal(
            lienzo.seleccionActual, sesion: lienzo.sesion) { [weak self] resultado in
            switch resultado {
            case .success(let texto) where !texto.isEmpty:
                UIPasteboard.general.string = texto
                self?.avisar("OCR local copiado. Revise el resultado antes de usarlo.")
            case .success:
                self?.avisar("El OCR local no encontró texto legible.")
            case .failure(let error): self?.avisar(error.localizedDescription)
            }
        }
    }

    private func copiarLaTeXSeleccion() {
        let texto = ExportadorSeleccionIPad.textoDirecto(
            lienzo.seleccionActual, sesion: lienzo.sesion)
        guard !texto.isEmpty else {
            avisar("Para evitar inventar una ecuación, primero obtenga o corrija el texto con OCR local.")
            return
        }
        UIPasteboard.general.string = FormateadorEcuaciones.latex(texto)
        avisar("LaTeX copiado. La conversión es estructural; confirme símbolos y exponentes.")
    }

    private func compartirSeleccion(_ formato: FormatoExportacionSeleccionIPad) {
        do {
            let url = try ExportadorSeleccionIPad.exportar(
                formato, seleccion: lienzo.seleccionActual, sesion: lienzo.sesion,
                titulo: title ?? "seleccion")
            let compartir = UIActivityViewController(activityItems: [url],
                                                     applicationActivities: nil)
            compartir.popoverPresentationController?.sourceView = view
            present(compartir, animated: true)
        } catch { avisar(error.localizedDescription) }
    }

    private func reconocerCircuitoSeleccion() {
        guard let (candidato, descriptor) = lienzo.sesion.reconocerCircuitoEntrenableSeleccion(
            lienzo.seleccionActual.trazos) else {
            avisar("La tinta seleccionada no produjo un candidato fiable. Puede enseñar ese símbolo desde Ingeniería sin modificar el original.")
            return
        }
        let porcentaje = Int((candidato.confianza * 100).rounded())
        let alerta = UIAlertController(
            title: "Candidato: \(candidato.tipo.nombre)",
            message: "Confianza local \(porcentaje) %. El original permanece intacto. Confirme para crear un componente editable y luego revise valor y conexiones.",
            preferredStyle: .alert)
        let crear: () -> Void = { [weak self] in
            guard let self else { return }
            let documento = self.lienzo.sesion.documentoPersistenteId
            var esquema = RepositorioEsquemas.compartido.esquema(documento: documento)
            let centro = CGPoint(x: (candidato.terminalA.x + candidato.terminalB.x) / 2,
                                 y: (candidato.terminalA.y + candidato.terminalB.y) / 2)
            esquema.agregar(candidato.tipo, posicion: centro,
                            terminalA: candidato.terminalA,
                            terminalB: candidato.terminalB,
                            confianza: candidato.confianza)
            RepositorioEsquemas.compartido.guardar(esquema, documento: documento)
            self.avisar("Componente editable creado. Revise valor, nodos y orientación en Ingeniería.")
        }
        alerta.addAction(UIAlertAction(title: "Crear componente editable", style: .default) {
            _ in crear()
        })
        alerta.addAction(UIAlertAction(title: "Crear y aprender este ejemplo", style: .default) {
            _ in
            RepositorioPlantillasCircuitoIPad.compartido.agregar(
                tipo: candidato.tipo, caracteristicas: descriptor.caracteristicas)
            crear()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func accion(para herramienta: HerramientaLienzo) -> AccionIPad {
        switch herramienta {
        case .pluma: return .pluma
        case .resaltador: return .resaltador
        case .borradorObjeto: return .borradorObjeto
        case .borradorParcial: return .borradorParcial
        case .lazo: return .lazo
        case .mano: return .mano
        }
    }

    @objc private func ejecutarBotonAccion(_ sender: UIButton) {
        guard let valor = sender.accessibilityIdentifier,
              let accion = AccionIPad(rawValue: valor) else { return }
        ejecutar(accion)
    }

    @objc private func ejecutarAtajo(_ sender: UIKeyCommand) {
        let atajo = CentroExperienciaIPad.compartido.instantanea().atajos.first {
            $0.entrada.lowercased() == (sender.input ?? "").lowercased()
                && $0.modificadores == sender.modifierFlags.rawValue
        }
        guard let accion = atajo?.accion else { return }
        ejecutar(accion)
    }

    @objc private func eliminarSeleccionConTeclado(_ sender: UIKeyCommand) {
        lienzo.eliminarSeleccion()
        autoguardado.marcarCambio()
    }

    private func ejecutar(_ accion: AccionIPad) {
        CentroExperienciaIPad.compartido.registrarUso(accion)
        switch accion {
        case .paletaComandos: abrirPaletaComandos(self)
        case .espaciosCurso: abrirExperienciaIPad(self)
        case .pluma: aplicarHerramienta(.pluma)
        case .resaltador: aplicarHerramienta(.resaltador)
        case .borradorObjeto: aplicarHerramienta(.borradorObjeto)
        case .borradorParcial: aplicarHerramienta(.borradorParcial)
        case .borradorAvanzado: abrirBorradorAvanzado()
        case .lazo: aplicarHerramienta(.lazo)
        case .mano: aplicarHerramienta(.mano)
        case .teclado:
            selectorModo.selectedSegmentIndex = ModoEscritura.teclado.rawValue
            cambiarModo(selectorModo)
        case .voz:
            selectorModo.selectedSegmentIndex = ModoEscritura.voz.rawValue
            cambiarModo(selectorModo)
        case .deshacer: deshacer(self)
        case .rehacer:
            lienzo.rehacer(); autoguardado.marcarCambio()
        case .limpiar: limpiar(self)
        case .papel: elegirPapel(self)
        case .lienzo: abrirLienzo(self)
        case .navegacionEspacial: abrirNavegacionEspacial(self)
        case .ultimaEscritura: volverUltimaEscritura(self)
        case .vistaAnterior: volverVistaAnterior(self)
        case .marcador: agregarMarcador(self)
        case .centroAcademico: abrirCentroAcademico(self)
        case .calendarioCampus: abrirIntegracionesAcademicas(self)
        case .evaluacionEntrega: abrirEvaluacionAcademica(self)
        case .multimediaClase: abrirMultimediaClase(self)
        case .bibliotecaFuentes: abrirBibliotecaFuentes(self)
        case .estudioOral: abrirEstudioOral(self)
        case .estudioTecnico: abrirEstudioTecnico(self)
        case .asistenteLocal: abrirAsistenteLocal(self)
        case .matematicas: abrirMatematicas(self)
        case .ingenieria: abrirIngenieria(self)
        case .herramientasTecnicas: abrirHerramientasTecnicas(self)
        case .plumaAvanzada: abrirPlumaAvanzada(self)
        case .bloques: abrirBloques(self)
        case .capas: abrirCapas(self)
        case .versiones: abrirVersiones(self)
        case .portapapeles: abrirPortapapeles(self)
        case .disposicion: abrirDisposicion(self)
        case .conocimiento: abrirConocimiento(self)
        case .colaboracion: abrirColaboracion(self)
        case .controlRemotoWeb: abrirControlRemotoWeb(self)
        case .seguridad: abrirSeguridad(self)
        case .foco: abrirFocoIPad(self)
        case .modosEntrada: abrirModosEntrada(self)
        case .metricasInterrupcion: abrirMetricasInterrupcion(self)
        case .proximaAccion: queHagoAhora(self)
        case .recuperarCincoMinutos: recuperarCincoMinutos(self)
        case .scratchGlobal: abrirScratchGlobal()
        case .importar: importar(self)
        case .escanearPizarra: escanearPizarra(self)
        case .exportar: exportar(self)
        case .grabar: alternarGrabacion(self)
        case .pausarGrabacion: alternarPausaGrabacion(self)
        case .marcarMePerdi: marcarMePerdi(self)
        case .marcarImportante: marcarImportante(self)
        case .marcarExamen: marcarExamen(self)
        case .reproducirNotaViva: reproducirUltimaGrabacion(self)
        case .ajustes: abrirAjustes(self)
        case .ventanaDividida: abrirVentanaDividida(self)
        case .menuRadial: abrirMenuRadial(self)
        case .notasAdhesivas: abrirNotasAdhesivas(self)
        case .carpetasInteligentes: abrirCarpetasInteligentes(self)
        case .calculadora: alternarCasio(self)
        case .herramientasCasio: abrirHerramientasCasio(self)
        }
    }

    @objc private func deshacer(_ sender: Any) {
        lienzo.deshacer(); autoguardado.marcarCambio()
    }
    @objc private func limpiar(_ sender: Any) {
        lienzo.limpiar(); autoguardado.marcarCambio()
    }

    private func mostrarPaleta() {
        let selector = UIColorPickerViewController()
        selector.selectedColor = UIColor(argb: lienzo.colorPluma)
        selector.delegate = self
        present(selector, animated: true)
    }

    @objc private func abrirModosEntrada(_ sender: Any) {
        let panel = PanelModosEntradaIPad(configuracion: configuracionEntrada)
        panel.alAplicar = { [weak self] configuracion in
            self?.aplicarConfiguracionEntrada(configuracion)
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func aplicarConfiguracionEntrada(_ configuracion: ConfiguracionEntradaIPad) {
        configuracionEntrada = configuracion
        lienzo.modoEntradaIPad = configuracion.modo
        lienzo.reconocerMarcadoresDibujados = configuracion.reconocerEstrellaComoMarcador
        navegador.aplicarDireccion(configuracion.direccion)
        navegador.configurarGestosContextuales(
            panConDosDedos: configuracion.panConDosDedos,
            navegacionTresDedos: configuracion.navegacionTresDedos)
        switch configuracion.modo {
        case .seleccion:
            aplicarHerramienta(.lazo)
        case .escritura where lienzo.herramienta == .lazo:
            aplicarHerramienta(.pluma)
        case .lectura:
            navegador.capaElementos.isUserInteractionEnabled = true
            lienzo.limpiarSeleccion()
        default: break
        }
    }

    private func mostrarMenuRadial() {
        view.subviews.compactMap { $0 as? MenuRadialIPad }.forEach {
            $0.removeFromSuperview()
        }
        let config = RepositorioMenuRadialIPad.compartido.configuracion("predeterminado")
        let centro = CGPoint(x: view.bounds.midX, y: view.bounds.midY)
        let menu = MenuRadialIPad(centro: centro, configuracion: config)
        menu.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        menu.alSeleccionar = { [weak self] accionStr in
            guard let self = self, let accion = AccionIPad(rawValue: accionStr) else { return }
            self.ejecutar(accion)
        }
        view.addSubview(menu)
    }

    /// Fondos de cuaderno y pautas. El color de la tinta lo recalcula el núcleo:
    /// en papel oscuro la negra se aclara sola.
    @objc private func elegirPapel(_ sender: Any) {
        let papeles = TipoPapel.allCases.map { ($0.nombre, $0) }
        let pautas = Pauta.allCases.map { ($0.nombre, $0) }

        let hoja = UIAlertController(title: "Papel", message: nil, preferredStyle: .actionSheet)
        for (nombre, tipo) in papeles {
            hoja.addAction(UIAlertAction(title: nombre, style: .default) { [weak self] _ in
                self?.papelActual = tipo
                self?.aplicarPapel()
            })
        }
        for (nombre, pauta) in pautas {
            hoja.addAction(UIAlertAction(title: nombre, style: .default) { [weak self] _ in
                self?.pautaActual = pauta
                self?.aplicarPapel()
            })
        }
        hoja.addAction(UIAlertAction(title: "Ajustes avanzados…", style: .default) {
            [weak self] _ in self?.abrirLienzo(sender)
        })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private var papelActual: TipoPapel = .blanco
    private var pautaActual: Pauta = .ninguna
    private var ajustesPapel = AjustesPapel()
    private var modoPagina: ModoPaginaLienzo = .hibrido

    private func aplicarPapel() {
        ajustesPapel.pauta = pautaActual
        lienzo.cambiarPapel(tipo: papelActual, ajustes: ajustesPapel)
        CentroExperienciaIPad.compartido.actualizarPapelActivo(
            tipo: papelActual, pauta: pautaActual,
            separacion: ajustesPapel.separacion,
            margen: ajustesPapel.margenIzquierdo)
        configurarBotonLienzoSimple()
    }

    private func sincronizarModoPaginaDesdeSesion() {
        switch (lienzo.sesion.anchoInfinito, lienzo.sesion.altoInfinito) {
        case (false, false): modoPagina = .paginadoFijo
        case (false, true): modoPagina = .hibrido
        case (true, false): modoPagina = .horizontalInfinito
        case (true, true): modoPagina = .infinitoXY
        }
        if isViewLoaded { configurarBotonLienzoSimple() }
    }

    @objc private func abrirLienzo(_ sender: Any) {
        var papel = ajustesPapel
        papel.pauta = pautaActual
        let configuracion = ConfiguracionLienzo(
            modo: modoPagina,
            ancho: lienzo.sesion.anchoPagina,
            alto: lienzo.sesion.altoPagina,
            unidad: .pixeles,
            reacomodo: .conservar,
            tipoPapel: papelActual,
            papel: papel,
            mostrarTiradoresTamano: navegador.mostrarTiradoresTamano,
            mostrarCortes: navegador.mostrarCortes,
            altoHoja: navegador.altoHojaImpresion,
            margenSeguro: navegador.margenSeguroImpresion)
        let panel = PanelLienzo(configuracion: configuracion)
        panel.alAplicar = { [weak self] nueva in
            guard let self else { return }
            self.modoPagina = nueva.modo
            self.papelActual = nueva.tipoPapel
            self.pautaActual = nueva.papel.pauta
            self.ajustesPapel = nueva.papel
            self.navegador.aplicarModo(nueva.modo)
            self.navegador.aplicarTamano(ancho: nueva.ancho, alto: nueva.alto,
                                         unidad: nueva.unidad,
                                         reacomodo: nueva.reacomodo)
            self.navegador.mostrarTiradoresTamano = nueva.mostrarTiradoresTamano
            self.navegador.mostrarCortes = nueva.mostrarCortes
            self.navegador.altoHojaImpresion = nueva.altoHoja
            self.navegador.margenSeguroImpresion = nueva.margenSeguro
            self.lienzo.cambiarPapel(tipo: nueva.tipoPapel, ajustes: nueva.papel)
            CentroExperienciaIPad.compartido.actualizarPapelActivo(
                tipo: nueva.tipoPapel, pauta: nueva.papel.pauta,
                separacion: nueva.papel.separacion,
                margen: nueva.papel.margenIzquierdo)
            self.configurarBotonLienzoSimple()
            self.autoguardado.marcarCambio()
        }
        panel.alAjustarContenido = { [weak self] in self?.navegador.recortarAlContenido() }
        panel.alInsertarEspacio = { [weak self] eje, desde, cantidad in
            self?.navegador.insertarEspacio(eje: eje, desde: desde, cantidad: cantidad)
        }
        panel.alAjustarAncho = { [weak self] in self?.navegador.ajustarAlAncho() }
        panel.alAjustarPagina = { [weak self] in self?.navegador.ajustarPaginaCompleta() }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func volverUltimaEscritura(_ sender: Any) {
        navegador.volverUltimaEscritura()
    }

    @objc private func abrirNavegacionEspacial(_ sender: Any) {
        let documento = lienzo.sesion.documentoPersistenteId
        let panel = PanelNavegacionEspacialIPad(
            documento: documento,
            estadoVista: { [weak self] in
                self?.navegador.estadoActual
                    ?? EstadoVistaLienzo(zoom: 1, desplazamiento: .zero)
            },
            regionVisible: { [weak self] in
                self?.navegador.regionVisibleDocumento ?? .zero
            })
        panel.alMostrarRegion = { [weak self] region in
            self?.navegador.mostrarRegion(region, animado: true)
        }
        panel.alRestaurarVista = { [weak self] estado in
            self?.navegador.restaurar(estado, animado: true)
        }
        panel.alCambiarFrames = { [weak self] frames in
            self?.navegador.mostrarFrames(frames)
        }
        panel.alExportarFrame = { [weak self] frame in self?.exportarFrame(frame) }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func exportarFrame(_ frame: FrameApunteIPad) {
        var ajustes = AjustesSeleccionLienzo()
        ajustes.filtro = .todo
        ajustes.requerirCompleto = false
        let seleccion = lienzo.sesion.seleccionarRectangulo(frame.region,
                                                            ajustes: ajustes)
        do {
            let url = try ExportadorSeleccionIPad.exportar(
                .pdf, seleccion: seleccion, sesion: lienzo.sesion,
                titulo: frame.titulo)
            let compartir = UIActivityViewController(activityItems: [url],
                                                     applicationActivities: nil)
            compartir.popoverPresentationController?.sourceView = view
            present(compartir, animated: true)
        } catch { avisar(error.localizedDescription) }
    }

    @objc private func volverVistaAnterior(_ sender: Any) {
        navegador.volverVistaAnterior()
    }

    @objc private func agregarMarcador(_ sender: Any) {
        let numero = navegador.marcadores.count + 1
        navegador.agregarMarcador(titulo: "Marcador \(numero)",
                                   en: navegador.centroVisibleDocumento)
    }

    @objc private func abrirCentroAcademico(_ sender: Any) {
        guardarEstadoExacto()
        let panel = PanelAcademico(documentoId: lienzo.sesion.documentoPersistenteId)
        panel.alRestaurarEstado = { [weak self] estado in self?.restaurarEstado(estado) }
        panel.alIniciarClase = { [weak self] sesion in self?.sesionAcademica = sesion }
        panel.alAbrirReferencia = { [weak self] referencia in
            self?.abrirReferencia(referencia)
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirIntegracionesAcademicas(_ sender: Any) {
        let panel = PanelIntegracionesAcademicasIPad()
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirBibliotecaFuentes(_ sender: Any) {
        let panel = PanelBibliotecaFuentesIPad(style: .insetGrouped)
        panel.alInsertarAdjuntos = { [weak self] adjuntos in
            self?.insertarAdjuntosDeBiblioteca(adjuntos)
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func insertarAdjuntosDeBiblioteca(_ adjuntos: [AdjuntoImportado]) {
        var cursor = navegador.centroVisibleDocumento
        for adjunto in adjuntos.prefix(120) {
            let tipo = Medios.tipoMedio(de: adjunto.url)
            if tipo == .pdf, let documento = PDFDocument(url: adjunto.url) {
                for indice in 0..<min(documento.pageCount, 80) {
                    guard let pagina = documento.page(at: indice) else { continue }
                    let tamano = Medios.tamanoDeInsercion(
                        pagina.bounds(for: .mediaBox).size)
                    _ = navegador.capaElementos.insertarAdjunto(
                        adjunto, tipo: .pdf, en: cursor, tamano: tamano,
                        paginaFuente: indice)
                    cursor.y += tamano.height + 40
                }
            } else {
                _ = navegador.capaElementos.insertarAdjunto(
                    adjunto, tipo: tipo, en: cursor,
                    tamano: CGSize(width: 440, height: 310))
                cursor.y += 340
            }
        }
        autoguardado.marcarCambio()
        if adjuntos.count > 120 {
            avisar("La migración quedó completa en la biblioteca. Se insertaron los primeros 120 recursos para no saturar esta página.")
        }
    }

    private func insertarPrintoutPDF(_ url: URL, opciones: OpcionesPrintoutPDFIPad) {
        do {
            let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
            RepositorioProcedenciaIPad.compartido.registrar(
                adjunto: adjunto, original: url, origen: .archivos)
            guard let documento = PDFDocument(url: adjunto.url) else {
                avisar("No se pudo abrir el PDF importado."); return
            }
            let plan = PlanificadorPrintoutPDFIPad.plan(
                documento: documento, origen: navegador.centroVisibleDocumento,
                opciones: opciones)
            for colocacion in plan {
                let id = navegador.capaElementos.insertarAdjunto(
                    adjunto, tipo: .pdf, en: colocacion.marco.origin,
                    tamano: colocacion.marco.size, paginaFuente: colocacion.pagina)
                if let id, let texto = documento.page(at: colocacion.pagina)?.string {
                    indexarTexto(texto, elementoId: id, tipo: .libro,
                                 titulo: "\(url.lastPathComponent) · página \(colocacion.pagina + 1)",
                                 hash: adjunto.hash, pagina: colocacion.pagina)
                }
            }
            autoguardado.marcarCambio()
        } catch { avisar(error.localizedDescription) }
    }

    @objc private func materialVinculadoCambio(_ nota: Notification) {
        guard let hash = nota.object as? String,
              let fuente = RepositorioProcedenciaIPad.compartido.fuente(hash: hash),
              presentedViewController == nil else { return }
        let alerta = UIAlertController(
            title: "Existe una versión más reciente",
            message: "\(fuente.nombreOriginal) cambió fuera de Tinta. La copia usada en sus notas no se actualizará silenciosamente.",
            preferredStyle: .alert)
        if fuente.nombreOriginal.lowercased().hasSuffix(".pdf"),
           let original = RepositorioProcedenciaIPad.compartido.urlOriginal(fuente),
           let local = RepositorioAdjuntos.compartido.url(hash: hash) {
            alerta.addAction(UIAlertAction(title: "Comparar PDF", style: .default) {
                [weak self] _ in
                DispatchQueue.global(qos: .userInitiated).async {
                    do {
                        let informe = try ComparadorPDFLocalIPad.comparar(local, original)
                        DispatchQueue.main.async {
                            self?.navigationController?.pushViewController(
                                PanelInformePDFIPad(informe), animated: true)
                        }
                    } catch { DispatchQueue.main.async { self?.avisar(error.localizedDescription) } }
                }
            })
        }
        alerta.addAction(UIAlertAction(title: "Conservar versión actual", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func abrirEvaluacionAcademica(_ sender: Any) {
        let panel = PanelEvaluacionAcademicaIPad(
            sesion: lienzo.sesion, navegador: navegador)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirMultimediaClase(_ sender: Any) {
        let manifiesto = ultimaGrabacion?.lastPathComponent == "manifest.json"
            ? ultimaGrabacion : nil
        let panel = PanelDiapositivasClaseIPad(manifiestoNotaViva: manifiesto)
        panel.alUsarDiapositiva = { [weak self] slide, adjunto, imagen in
            guard let self,
                  let elemento = self.navegador.capaElementos.insertarAdjunto(
                    adjunto, tipo: .imagen,
                    en: self.navegador.centroVisibleDocumento,
                    tamano: Medios.tamanoDeInsercion(imagen.size)) else { return }
            self.indexarTexto(
                slide.textoExtraido, elementoId: elemento,
                tipo: .diapositivaProfesor,
                titulo: "\(slide.tituloPresentacion) — \(slide.numero)",
                hash: slide.hashFuente)
            self.grabadora.marcar(
                .cambioDiapositiva,
                titulo: "\(slide.tituloPresentacion) — slide \(slide.numero)",
                pagina: slide.numero,
                punto: self.navegador.centroVisibleDocumento,
                recursoId: slide.hashImagen,
                diapositivaId: slide.id)
            self.registrarDiapositivaAcademica(slide, elementoId: elemento)
            self.autoguardado.marcarCambio()
        }
        panel.alSepararDiapositiva = { [weak self] slide, imagen, modo in
            guard let self else { return }
            self.dismiss(animated: true) {
                self.separarDiapositivaPPTX(slide, imagen: imagen, modo: modo)
            }
        }
        panel.alInsertarLote = { [weak self] slides, destino in
            guard let self else { return }
            self.dismiss(animated: true) {
                self.insertarLotePPTX(slides, destino: destino)
            }
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func separarDiapositivaPPTX(
        _ slide: DiapositivaLocalIPad,
        imagen: UIImage,
        modo: ModoSeparacionPPTXIPad
    ) {
        do {
            let resultado = try SeparadorDiapositivaPPTXIPad.insertar(
                slide, imagen: imagen, en: navegador,
                desde: navegador.centroVisibleDocumento, modo: modo)
            indexarTexto(
                slide.textoExtraido, elementoId: nil,
                tipo: .diapositivaProfesor,
                titulo: "\(slide.tituloPresentacion) — elementos de slide \(slide.numero)",
                hash: slide.hashFuente)
            grabadora.marcar(
                .cambioDiapositiva,
                titulo: "Slide \(slide.numero) separada en objetos",
                pagina: slide.numero, punto: navegador.centroVisibleDocumento,
                recursoId: slide.hashImagen, diapositivaId: slide.id)
            autoguardado.marcarCambio()
            avisar("Separación no destructiva terminada. \(resultado.resumen). El PPTX y la vista fiel permanecen intactos.")
        } catch {
            avisar(error.localizedDescription)
        }
    }

    private func insertarLotePPTX(
        _ slides: [DiapositivaLocalIPad],
        destino: DestinoImportacionPPTXIPad
    ) {
        guard !slides.isEmpty else { return }
        let inicio = navegador.centroVisibleDocumento
        var y = inicio.y
        var insertadas = 0
        let documento = lienzo.sesion.documentoPersistenteId
        for slide in slides.sorted(by: { $0.numero < $1.numero }) {
            guard let adjunto = RepositorioDiapositivasIPad.compartido.adjunto(de: slide),
                  let imagen = UIImage(contentsOfFile: adjunto.url.path) else { continue }
            let ancho: CGFloat = 760
            let alto = max(320, ancho * imagen.size.height / max(imagen.size.width, 1))
            let punto = CGPoint(x: inicio.x, y: y)
            guard let elemento = navegador.capaElementos.insertarAdjunto(
                adjunto, tipo: .imagen, en: punto,
                tamano: CGSize(width: ancho, height: alto)) else { continue }
            indexarTexto(
                slide.textoExtraido, elementoId: elemento,
                tipo: .diapositivaProfesor,
                titulo: "\(slide.tituloPresentacion) — \(slide.numero)",
                hash: slide.hashFuente)
            registrarDiapositivaAcademica(slide, elementoId: elemento)
            insertadas += 1

            var altoRegion = alto
            if destino == .presentacionHibrida {
                let notasY = y + alto + 28
                _ = navegador.capaElementos.insertarTexto(
                    "Apuntes de la diapositiva \(slide.numero)\n",
                    marco: CGRect(x: inicio.x, y: notasY,
                                  width: ancho, height: 420))
                altoRegion += 468
            }
            if destino == .paginasImprimibles || destino == .presentacionHibrida {
                RepositorioNavegacionEspacialIPad.compartido.agregar(
                    FrameApunteIPad(
                        titulo: "\(slide.tituloPresentacion) — slide \(slide.numero)",
                        region: CGRect(x: inicio.x - 24, y: y - 24,
                                       width: ancho + 48, height: altoRegion + 48),
                        tipo: destino == .paginasImprimibles ? .presentacion : .visible),
                    documento: documento)
            }
            y += altoRegion + 72
        }
        let frames = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(documento).frames
        navegador.mostrarFrames(frames)
        autoguardado.marcarCambio()
        avisar("Se insertaron \(insertadas) diapositivas. Cada objeto conserva la referencia al PowerPoint original.")
    }

    @objc private func abrirEstudioOral(_ sender: Any) {
        let panel = PanelEstudioOralIPad()
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirEstudioTecnico(_ sender: Any) {
        let seleccion = lienzo.seleccionActual
        let textos = seleccion.textos.compactMap { indice in
            lienzo.sesion.textos.indices.contains(indice)
                ? lienzo.sesion.textos[indice] : nil
        }
        let medios = seleccion.medios.compactMap { indice in
            lienzo.sesion.medios.indices.contains(indice)
                ? lienzo.sesion.medios[indice] : nil
        }
        let textoSeleccionado = textos.map(\.texto).joined(separator: "\n")
        let elementoId = textos.first?.id ?? medios.first?.id
        let cursoId = sesionAcademica?.cursoId
            ?? CentroExperienciaIPad.compartido.instantanea().espacioActivo?.cursoId
        let fuente = elementoId.map {
            ReferenciaFuente(
                tipo: .apuntePropio, titulo: "Selección de la nota",
                cursoId: cursoId, sesionId: sesionAcademica?.id,
                documentoId: lienzo.sesion.documentoPersistenteId, elementoId: $0,
                segundoAudio: grabadora.estado == .detenida
                    ? nil : grabadora.segundosGrabados)
        }
        let panel = PanelEstudioTecnicoIPad(
            documentoId: lienzo.sesion.documentoPersistenteId, cursoId: cursoId,
            fuente: fuente, textoSeleccionado: textoSeleccionado)
        panel.alInsertarTexto = { [weak self] texto in
            guard let self,
                  let id = self.navegador.capaElementos.insertarTexto(
                    texto, en: self.navegador.centroVisibleDocumento) else { return }
            self.indexarTexto(texto, elementoId: id, tipo: .apuntePropio,
                              titulo: "Material de estudio técnico")
            self.autoguardado.marcarCambio()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func registrarDiapositivaAcademica(
        _ slide: DiapositivaLocalIPad, elementoId: String
    ) {
        let sesion = sesionAcademica
        let fuente = ReferenciaFuente(
            tipo: .diapositivaProfesor,
            titulo: "\(slide.tituloPresentacion) — slide \(slide.numero)",
            cursoId: sesion?.cursoId, sesionId: sesion?.id,
            documentoId: lienzo.sesion.documentoPersistenteId,
            elementoId: elementoId,
            hashArchivo: slide.hashFuente, pagina: slide.numero,
            segundoAudio: grabadora.estado == .detenida
                ? nil : grabadora.segundosGrabados)
        CentroAcademico.compartido.transaccion { base in
            base.timeline.append(EventoAcademico(
                tipo: .diapositiva, titulo: fuente.titulo,
                cursoId: sesion?.cursoId, sesionId: sesion?.id,
                referencia: fuente))
        }
    }

    @objc private func abrirAsistenteLocal(_ sender: Any) {
        let asistente = AsistenteLocal()
        asistente.alAbrirFuente = { [weak self] fuente in self?.abrirReferencia(fuente) }
        present(UINavigationController(rootViewController: asistente), animated: true)
    }

    @objc private func abrirIngenieria(_ sender: Any) {
        let panel = PanelIngenieria(documentoId: lienzo.sesion.documentoPersistenteId,
                                    sesion: lienzo.sesion)
        panel.alInsertarTexto = { [weak self] texto in
            guard let self,
                  let id = self.navegador.capaElementos.insertarTexto(
                    texto, en: self.navegador.centroVisibleDocumento) else { return }
            self.indexarTexto(texto, elementoId: id, tipo: .apuntePropio,
                              titulo: "Resultado de ingeniería")
        }
        panel.alInsertarImagen = { [weak self] imagen, nombre in
            guard let self,
                  let adjunto = try? RepositorioAdjuntos.compartido.importar(
                    imagen: imagen, nombre: "\(nombre).png") else { return }
            _ = self.navegador.capaElementos.insertarAdjunto(
                adjunto, tipo: .imagen, en: self.navegador.centroVisibleDocumento,
                tamano: Medios.tamanoDeInsercion(imagen.size))
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirMatematicas(_ sender: Any) {
        let panel = PanelMatematicas(documentoId: lienzo.sesion.documentoPersistenteId)
        panel.alInsertar = { [weak self] texto in
            guard let self,
                  let id = self.navegador.capaElementos.insertarTexto(
                    texto, en: self.navegador.centroVisibleDocumento) else { return }
            self.indexarTexto(texto, elementoId: id, tipo: .apuntePropio,
                              titulo: "Resultado matemático")
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirHerramientasTecnicas(_ sender: Any) {
        let panel = PanelHerramientasTecnicas(capa: navegador.capaTecnica)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirPlumaAvanzada(_ sender: Any) {
        let panel = PanelPlumaAvanzada(ajustes: lienzo.ajustesPluma,
                                       color: lienzo.colorPluma,
                                       grosor: lienzo.anchoPluma,
                                       resaltador: lienzo.herramienta == .resaltador)
        panel.alAplicar = { [weak self] pluma in
            guard let self else { return }
            self.lienzo.ajustesPluma = pluma.ajustes
            self.lienzo.colorPluma = pluma.colorARGB
            self.lienzo.anchoPluma = pluma.grosor
            self.aplicarHerramienta(pluma.resaltador ? .resaltador : .pluma)
            CentroExperienciaIPad.compartido.actualizarPlumaActiva(pluma.id)
        }
        panel.alRefinarTodos = { [weak self] nivel in
            guard let self, self.lienzo.sesion.refinarTodos(nivel: nivel.rawValue) else { return }
            self.lienzo.rehornearYDibujar()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirBloques(_ sender: Any) {
        let panel = PanelBloquesLienzo(capa: navegador.capaBloques,
                                       sesion: lienzo.sesion) { [weak self] in
            self?.navegador.centroVisibleDocumento ?? .zero
        }
        navegador.capaBloques.alConvertir = { [weak self] bloque, destino in
            guard let self else { return }
            let referencia = ReferenciaElemento(id: bloque.id.uuidString,
                                                titulo: bloque.titulo,
                                                tipo: bloque.tipo.rawValue,
                                                paginaDocumento: self.lienzo.sesion.documentoPersistenteId)
            _ = CentroAcademico.compartido.convertir(referencia, a: destino)
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirCapas(_ sender: Any) {
        let panel = PanelCapas(documentoId: lienzo.sesion.documentoPersistenteId,
                               navegador: navegador)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirVersiones(_ sender: Any) {
        let panel = PanelVersiones(sesion: lienzo.sesion, navegador: navegador)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirPortapapeles(_ sender: Any) {
        if shelfLateral != nil {
            cerrarShelfLateral()
            return
        }
        let panel = PanelPortapapeles(style: .insetGrouped)
        panel.alInsertarTexto = { [weak self] texto in
            guard let self else { return }
            _ = self.navegador.capaElementos.insertarTexto(
                texto, en: self.navegador.centroVisibleDocumento)
        }
        panel.alInsertarAdjunto = { [weak self] adjunto, tipo in
            guard let self else { return }
            _ = self.navegador.capaElementos.insertarAdjunto(
                adjunto, tipo: tipo, en: self.navegador.centroVisibleDocumento)
        }
        panel.alPegarComo = { [weak self] entrada, modo in
            self?.pegarInteligente(entrada, como: modo)
        }
        panel.alCerrar = { [weak self] in self?.cerrarShelfLateral() }
        let navegacionShelf = UINavigationController(rootViewController: panel)
        addChild(navegacionShelf)
        navegacionShelf.view.translatesAutoresizingMaskIntoConstraints = false
        navegacionShelf.view.layer.cornerRadius = 14
        navegacionShelf.view.layer.maskedCorners = [.layerMinXMinYCorner,
                                                     .layerMinXMaxYCorner]
        navegacionShelf.view.layer.shadowColor = UIColor.black.cgColor
        navegacionShelf.view.layer.shadowOpacity = 0.2
        navegacionShelf.view.layer.shadowRadius = 14
        navegacionShelf.view.layer.shadowOffset = CGSize(width: -4, height: 0)
        view.addSubview(navegacionShelf.view)
        let anchoDisponible = max(view.bounds.width, 320)
        let anchoShelf = min(430, max(280, anchoDisponible * 0.42))
        NSLayoutConstraint.activate([
            navegacionShelf.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            navegacionShelf.view.topAnchor.constraint(
                equalTo: view.safeAreaLayoutGuide.topAnchor),
            navegacionShelf.view.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            navegacionShelf.view.leadingAnchor.constraint(
                greaterThanOrEqualTo: view.leadingAnchor, constant: 8),
            navegacionShelf.view.widthAnchor.constraint(equalToConstant: anchoShelf),
        ])
        navegacionShelf.didMove(toParent: self)
        shelfLateral = navegacionShelf
    }

    private func pegarInteligente(_ entrada: EntradaPortapapeles,
                                  como modo: ModoPegadoInteligenteIPad) {
        switch modo {
        case .original:
            insertarEntradaOriginalPortapapeles(entrada)
        case .imagen:
            if let imagen = imagenDeEntradaPortapapeles(entrada) {
                insertarImagenes([imagen])
            } else if let texto = entrada.texto {
                insertarImagenes([imagenDeTexto(texto)])
            } else {
                avisar("No existe una representación visual local de este elemento.")
            }
        case .texto:
            if let texto = entrada.texto {
                insertarTextoPegado(texto, titulo: "Texto del portapapeles")
            } else {
                reconocerEntradaPortapapeles(entrada, objetivo: .texto)
            }
        case .latex:
            guard let texto = entrada.texto, !texto.isEmpty else {
                reconocerEntradaPortapapeles(entrada, objetivo: .ecuacion); return
            }
            insertarBloquePegado(.ecuacion,
                                 titulo: "LaTeX del portapapeles",
                                 contenido: FormateadorEcuaciones.latex(texto))
        case .enlace:
            guard let texto = entrada.texto,
                  URL(string: texto.trimmingCharacters(in: .whitespacesAndNewlines)) != nil
            else { avisar("El contenido no contiene un enlace válido."); return }
            insertarTextoPegado(texto, titulo: "Enlace del portapapeles")
        case .fuenteCitada:
            let cuerpo = entrada.texto ?? entrada.nombre
            let cita = "Fuente: \(entrada.nombre) · capturada \(entrada.fecha.formatted(date: .abbreviated, time: .shortened))\n\n\(cuerpo)"
            insertarTextoPegado(cita, titulo: "Fragmento con fuente")
        case .ocr:
            reconocerEntradaPortapapeles(entrada, objetivo: .texto)
        case .ecuacion:
            reconocerEntradaPortapapeles(entrada, objetivo: .ecuacion)
        case .circuito:
            reconocerEntradaPortapapeles(entrada, objetivo: .circuito)
        }
        autoguardado.marcarCambio()
    }

    private func insertarEntradaOriginalPortapapeles(_ entrada: EntradaPortapapeles) {
        if let texto = entrada.texto {
            switch entrada.tipo {
            case .codigo:
                insertarBloquePegado(.codigo, titulo: entrada.nombre, contenido: texto)
            case .ecuacion:
                insertarBloquePegado(.ecuacion, titulo: entrada.nombre, contenido: texto)
            default:
                insertarTextoPegado(texto, titulo: entrada.nombre)
            }
            return
        }
        guard let adjunto = adjuntoDeEntradaPortapapeles(entrada) else {
            avisar("El recurso original ya no está disponible en el almacén local."); return
        }
        let ext = adjunto.url.pathExtension.lowercased()
        let tipo: TipoMedio = entrada.tipo == .imagen ? .imagen
            : (ext == "pdf" ? .pdf
                : (["mov", "mp4", "m4v"].contains(ext) ? .video : .documento))
        _ = navegador.capaElementos.insertarAdjunto(
            adjunto, tipo: tipo, en: navegador.centroVisibleDocumento,
            tamano: imagenDeEntradaPortapapeles(entrada).map {
                Medios.tamanoDeInsercion($0.size)
            })
    }

    private func insertarTextoPegado(_ texto: String, titulo: String) {
        guard let id = navegador.capaElementos.insertarTexto(
            texto, en: navegador.centroVisibleDocumento) else { return }
        indexarTexto(texto, elementoId: id, tipo: .apuntePropio, titulo: titulo)
    }

    private func insertarBloquePegado(_ tipo: TipoBloqueLienzo,
                                      titulo: String, contenido: String) {
        _ = navegador.capaBloques.insertar(
            tipo: tipo, en: navegador.centroVisibleDocumento,
            titulo: titulo, contenido: contenido)
        indexarTexto(contenido, elementoId: nil, tipo: .apuntePropio, titulo: titulo)
    }

    private enum ObjetivoReconocimientoPortapapeles: Equatable {
        case texto, ecuacion, circuito
    }

    private func reconocerEntradaPortapapeles(
        _ entrada: EntradaPortapapeles,
        objetivo: ObjetivoReconocimientoPortapapeles
    ) {
        guard let imagen = imagenDeEntradaPortapapeles(entrada) else {
            if let texto = entrada.texto {
                if objetivo == .ecuacion {
                    insertarBloquePegado(.ecuacion, titulo: "Ecuación detectada",
                                         contenido: FormateadorEcuaciones.latex(texto))
                } else { insertarTextoPegado(texto, titulo: entrada.nombre) }
            } else { avisar("Este recurso no tiene imagen local para analizar.") }
            return
        }
        AnalizadorImagenLocal.reconocerTexto(imagen) { [weak self] resultado in
            DispatchQueue.main.async {
                guard let self else { return }
                let bloques = (try? resultado.get()) ?? []
                let texto = bloques.map(\.texto).joined(separator: "\n")
                switch objetivo {
                case .texto:
                    guard !texto.isEmpty else {
                        self.avisar("El OCR local no encontró texto. La imagen original no se modificó."); return
                    }
                    self.insertarTextoPegado(texto, titulo: "OCR de \(entrada.nombre)")
                case .ecuacion:
                    guard !texto.isEmpty else {
                        self.avisar("No se detectó texto suficiente para proponer una ecuación."); return
                    }
                    self.insertarBloquePegado(
                        .ecuacion, titulo: "Ecuación candidata · revisar",
                        contenido: FormateadorEcuaciones.latex(texto))
                case .circuito:
                    let descripcion = texto.isEmpty
                        ? "Candidato visual sin etiquetas OCR. Revise símbolos, valores y conexiones."
                        : "Etiquetas detectadas:\n\(texto)\n\nRevise símbolos, valores y conexiones antes de usarlo."
                    self.insertarBloquePegado(
                        .circuito, titulo: "Circuito candidato · original conservado",
                        contenido: descripcion)
                    self.insertarEntradaOriginalPortapapeles(entrada)
                }
                self.autoguardado.marcarCambio()
            }
        }
    }

    private func adjuntoDeEntradaPortapapeles(
        _ entrada: EntradaPortapapeles
    ) -> AdjuntoImportado? {
        guard let hash = entrada.hashAdjunto,
              let url = RepositorioAdjuntos.compartido.url(hash: hash) else { return nil }
        let bytes = Int64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
        return AdjuntoImportado(hash: hash, url: url,
                                nombreOriginal: entrada.nombre, bytes: bytes)
    }

    private func imagenDeEntradaPortapapeles(_ entrada: EntradaPortapapeles) -> UIImage? {
        guard let adjunto = adjuntoDeEntradaPortapapeles(entrada) else { return nil }
        if let imagen = UIImage(contentsOfFile: adjunto.url.path) { return imagen }
        if adjunto.url.pathExtension.lowercased() == "pdf",
           let pagina = PDFDocument(url: adjunto.url)?.page(at: 0) {
            return pagina.thumbnail(of: CGSize(width: 1_600, height: 1_600),
                                    for: .mediaBox)
        }
        return nil
    }

    private func imagenDeTexto(_ texto: String) -> UIImage {
        let ancho: CGFloat = 900
        let atributos: [NSAttributedString.Key: Any] = [
            .font: UIFont.monospacedSystemFont(ofSize: 28, weight: .regular),
            .foregroundColor: UIColor.label,
        ]
        let limite = CGSize(width: ancho - 64, height: 4_000)
        let altoTexto = (texto as NSString).boundingRect(
            with: limite, options: [.usesLineFragmentOrigin, .usesFontLeading],
            attributes: atributos, context: nil).height
        let tamano = CGSize(width: ancho, height: min(max(altoTexto + 64, 160), 4_064))
        return UIGraphicsImageRenderer(size: tamano).image { contexto in
            UIColor.systemBackground.setFill()
            contexto.fill(CGRect(origin: .zero, size: tamano))
            (texto as NSString).draw(
                in: CGRect(x: 32, y: 32, width: tamano.width - 64,
                           height: tamano.height - 64),
                withAttributes: atributos)
        }
    }

    private func cerrarShelfLateral() {
        guard let shelf = shelfLateral else { return }
        shelf.willMove(toParent: nil)
        shelf.view.removeFromSuperview()
        shelf.removeFromParent()
        shelfLateral = nil
    }

    @objc private func abrirDisposicion(_ sender: Any) {
        let panel = PanelDisposicionInteligente(navegador: navegador,
                                                sesion: lienzo.sesion,
                                                tituloActual: { [weak self] in
                                                    self?.title ?? ""
                                                },
                                                clase: sesionAcademica)
        panel.alAceptarTitulo = { [weak self] titulo in
            self?.aceptarTituloDocumento(titulo)
        }
        panel.alCerrarClase = { [weak self] in self?.cerrarClaseInteligente() }
        panel.alAplicarCambios = { [weak self] in
            self?.autoguardado.marcarCambio()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func aceptarTituloDocumento(_ titulo: String) {
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { return }
        title = limpio
        alCambiarTituloDocumento?(limpio)
        let espacio = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        CentroExperienciaIPad.compartido.registrarDocumento(
            lienzo.sesion.documentoPersistenteId, titulo: limpio,
            cursoId: espacio?.cursoId)
    }

    private func cerrarClaseInteligente() {
        guard let clase = sesionAcademica else {
            avisar("No hay una sesión de clase activa para cerrar."); return
        }
        let terminarCierre = { [weak self] in
            guard let self else { return }
            self.guardarEstadoExacto()
            self.autoguardado.guardarAhora(motivo: "cierre de clase")
            let resumen = MotorCierreClaseIPad.preparar(
                clase: clase, sesion: self.lienzo.sesion)
            let panel = PanelCierreClaseIPad(resumen: resumen)
            panel.alAceptarTitulo = { [weak self] titulo in
                self?.aceptarTituloDocumento(titulo)
            }
            panel.alAbrirFuente = { [weak self] fuente in
                self?.abrirReferencia(fuente)
            }
            self.sesionAcademica = nil
            self.present(UINavigationController(rootViewController: panel),
                         animated: true)
        }
        guard grabadora.estado != .detenida else {
            terminarCierre(); return
        }
        grabadora.detener { [weak self] resultado in
            if case .success(let url) = resultado {
                self?.ultimaGrabacion = url
                CentroAcademico.compartido.transaccion { base in
                    guard let indice = base.sesiones.firstIndex(where: {
                        $0.id == clase.id
                    }) else { return }
                    base.sesiones[indice].fin = Date()
                    if url.pathExtension.lowercased() == "m4a" {
                        base.sesiones[indice].archivoAudio = url.path
                    } else if url.lastPathComponent == "manifest.json" {
                        base.sesiones[indice].archivoNotaViva =
                            url.deletingLastPathComponent().path
                    }
                }
            }
            DispatchQueue.main.async(execute: terminarCierre)
        }
    }

    @objc private func abrirConocimiento(_ sender: Any) {
        let panel = PanelConocimiento(sesion: lienzo.sesion)
        panel.alAbrirFuente = { [weak self] fuente in self?.abrirReferencia(fuente) }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirSeguridad(_ sender: Any) {
        let panel = PanelSeguridadRecuperacion(
            sesion: lienzo.sesion, navegador: navegador, autoguardado: autoguardado)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirColaboracion(_ sender: Any) {
        let panel = PanelColaboracionLocal(
            coordinador: colaboracion,
            documentoId: lienzo.sesion.documentoPersistenteId)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirControlRemotoWeb(_ sender: Any) {
        let panel = PanelControlRemotoWebIPad()
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: true)
    }

    @objc private func abrirMetricasInterrupcion(_ sender: Any) {
        let panel = PanelDistraccionIPad()
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func abrirPaletaComandos(_ sender: Any) {
        let panel = PaletaComandosIPad()
        panel.alEjecutar = { [weak self] accion in self?.ejecutar(accion) }
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        present(navegacion, animated: !configuracionInterfaz.reducirMovimiento)
    }

    @objc private func abrirExperienciaIPad(_ sender: Any) {
        let panel = PanelExperienciaIPad()
        panel.alAplicarInterfaz = { [weak self] configuracion in
            self?.aplicarInterfaz(configuracion)
        }
        panel.alCambiarEspacio = { [weak self] espacio in
            self?.solicitarCambioEspacio(espacio)
        }
        panel.alAbrirPaleta = { [weak self] in
            guard let self else { return }
            self.abrirPaletaComandos(self)
        }
        panel.alAbrirFoco = { [weak self] in
            guard let self else { return }
            self.abrirFocoIPad(self)
        }
        panel.alAbrirScratch = { [weak self] scratch in
            self?.presentarScratch(scratch)
        }
        panel.alCambiarBarra = { [weak self] in self?.reconstruirBarraPrincipal() }
        present(UINavigationController(rootViewController: panel),
                animated: !configuracionInterfaz.reducirMovimiento)
    }

    private func solicitarCambioEspacio(_ espacio: EspacioCursoIPad) {
        let actual = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        guard actual?.id != espacio.id else {
            aplicarEspacio(espacio)
            return
        }
        if coordinadorFoco.activo,
           coordinadorFoco.configuracion?.bloquearCambioCurso == true {
            let alerta = UIAlertController(
                title: "Está en una sesión de enfoque",
                message: "Cambiar de \(actual?.nombre ?? "este espacio") a \(espacio.nombre) modifica papel, herramientas y contexto. Confirme el cambio conscientemente.",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Seguir en el curso actual",
                                           style: .cancel))
            alerta.addAction(UIAlertAction(title: "Aparcar objetivo y cambiar",
                                           style: .default) { [weak self] _ in
                guard let self else { return }
                if let objetivo = self.coordinadorFoco.configuracion?.objetivo,
                   !objetivo.isEmpty {
                    self.coordinadorFoco.aparcar("Retomar: \(objetivo)")
                }
                self.terminarFoco()
                self.aplicarEspacio(espacio)
            })
            present(alerta, animated: true)
        } else {
            aplicarEspacio(espacio)
        }
    }

    private func aplicarEspacio(_ espacio: EspacioCursoIPad) {
        CentroExperienciaIPad.compartido.activarEspacio(
            espacio.id, documentoId: lienzo.sesion.documentoPersistenteId)
        let interfaz = CentroExperienciaIPad.compartido.instantanea().interfaz
        aplicarInterfaz(interfaz)
        if let papel = TipoPapel(rawValue: espacio.papel.tipoPapel),
           let pauta = Pauta(rawValue: espacio.papel.pauta) {
            papelActual = papel
            pautaActual = pauta
            ajustesPapel.pauta = pauta
            ajustesPapel.separacion = espacio.papel.separacion
            ajustesPapel.margenIzquierdo = espacio.papel.margen
            aplicarPapel()
        }
        if let valor = espacio.perfilPlumaId,
           let id = UUID(uuidString: valor),
           let pluma = RepositorioPlumas.compartido.perfiles
            .flatMap(\.plumas).first(where: { $0.id == id }) {
            lienzo.ajustesPluma = pluma.ajustes
            lienzo.colorPluma = pluma.colorARGB
            lienzo.anchoPluma = pluma.grosor
            aplicarHerramienta(pluma.resaltador ? .resaltador : .pluma)
        }
        title = espacio.nombre
        CentroExperienciaIPad.compartido.registrarDocumento(
            lienzo.sesion.documentoPersistenteId, titulo: espacio.nombre,
            cursoId: espacio.cursoId)
        CentroExperienciaIPad.compartido.relacionar(
            origen: "documento:\(lienzo.sesion.documentoPersistenteId)",
            destino: "curso:\(espacio.cursoId?.uuidString ?? espacio.id.uuidString)",
            tipo: .perteneceA, cursoId: espacio.cursoId)
    }

    private func aplicarInterfaz(_ configuracion: ConfiguracionInterfazIPad) {
        let requiereReconstruir = configuracion.nivel != configuracionInterfaz.nivel
            || configuracion.barraCompacta != configuracionInterfaz.barraCompacta
        configuracionInterfaz = configuracion
        if requiereReconstruir, barraPrincipal.superview != nil {
            reconstruirBarraPrincipal()
            return
        }
        let mostrarBarra = configuracion.mostrarBarra
            && !(coordinadorFoco.configuracion?.ocultarBarra ?? false)
        lienzoConBarra?.isActive = false
        lienzoSinBarra?.isActive = false
        barraPrincipal.isHidden = !mostrarBarra
        botonRecuperarBarra.isHidden = mostrarBarra
            || (coordinadorFoco.configuracion?.ocultarBarra ?? false)
        if mostrarBarra && configuracionBarra.ubicacion == .superior {
            lienzoConBarra?.isActive = true
        } else {
            lienzoSinBarra?.isActive = true
        }
        #if DEBUG
        etiqueta.isHidden = !configuracion.mostrarIndicadores
        #else
        // La telemetría de latencia es una herramienta de ingeniería, no parte
        // de la interfaz de producción que se instala en el iPad.
        etiqueta.isHidden = true
        #endif
        etiquetaGrabacion.accessibilityElementsHidden = false
        selectorPrediccion.isHidden = configuracion.nivel == .simple
        navegador.mostrarMinimapa = configuracion.nivel == .avanzado
            && configuracion.mostrarIndicadores
        if botonesSatelite.count != configuracionBarra.satelites.count {
            reconstruirSatelites()
        }
        view.bringSubviewToFront(barraPrincipal)
        botonesSatelite.values.forEach { view.bringSubviewToFront($0) }
        view.bringSubviewToFront(botonRecuperarBarra)
        let cambios = { self.view.layoutIfNeeded() }
        if configuracion.reducirMovimiento {
            UIView.performWithoutAnimation(cambios)
        } else {
            UIView.animate(withDuration: 0.18, animations: cambios)
        }
    }

    private func reconstruirBarraPrincipal() {
        let instantanea = CentroExperienciaIPad.compartido.instantanea()
        configuracionInterfaz = instantanea.interfaz
        configuracionBarra = instantanea.barra
        let anterior = barraPrincipal
        lienzoConBarra?.isActive = false
        lienzoSinBarra?.isActive = false
        NSLayoutConstraint.deactivate(restriccionesBarra)
        restriccionesBarra.removeAll()

        let nueva = construirBarra()
        nueva.translatesAutoresizingMaskIntoConstraints = false
        view.insertSubview(nueva, aboveSubview: anterior)
        barraPrincipal = nueva
        instalarRestriccionesBarra()
        lienzoConBarra = navegador.topAnchor.constraint(
            equalTo: nueva.bottomAnchor, constant: 8)
        lienzoSinBarra = navegador.topAnchor.constraint(
            equalTo: view.safeAreaLayoutGuide.topAnchor)
        NSLayoutConstraint.deactivate(view.constraints.filter { restriccion in
            (restriccion.firstItem as? UIView) === anterior
                || (restriccion.secondItem as? UIView) === anterior
        })
        anterior.removeFromSuperview()
        aplicarInterfaz(configuracionInterfaz)
        estadoBarra.$herramientaActiva.sink { [weak self] h in self?.lienzo.herramienta = h }.store(in: &observadoresBarra)
        estadoBarra.$colorActivo.sink { [weak self] c in self?.lienzo.colorPluma = c }.store(in: &observadoresBarra)
    }

    @objc private func abrirFocoIPad(_ sender: Any) {
        let panel = PanelFocoIPad()
        panel.alIniciar = { [weak self] configuracion in
            self?.iniciarFoco(configuracion)
        }
        present(UINavigationController(rootViewController: panel),
                animated: !configuracionInterfaz.reducirMovimiento)
    }

    private func configurarFoco() {
        controlFoco.alAparcar = { [weak self] in self?.pedirPensamientoParking() }
        controlFoco.alInterrupcion = { [weak self] in
            self?.coordinadorFoco.registrarInterrupcion()
            UIAccessibility.post(notification: .announcement,
                                 argument: "Interrupción registrada")
        }
        controlFoco.alTerminar = { [weak self] in self?.terminarFoco() }
        coordinadorFoco.alActualizar = { [weak self] restante, duracion, apariencia in
            guard let self, let configuracion = self.coordinadorFoco.configuracion else {
                return
            }
            self.controlFoco.actualizar(
                objetivo: configuracion.objetivo, restantes: restante,
                duracion: duracion, apariencia: apariencia)
        }
        coordinadorFoco.alTerminar = { [weak self] in
            guard let self else { return }
            self.ocultarInterfazFoco()
            self.avisar("Terminó la sesión de enfoque.")
        }
    }

    private func iniciarFoco(_ configuracion: ConfiguracionFocoIPad) {
        coordinadorFoco.iniciar(configuracion)
        mascaraFoco.modo = configuracion.modo
        mascaraFoco.isHidden = configuracion.modo == .escrituraLibre
        controlFoco.isHidden = false
        aplicarInterfaz(configuracionInterfaz)
        estadoBarra.$herramientaActiva.sink { [weak self] h in self?.lienzo.herramienta = h }.store(in: &observadoresBarra)
        estadoBarra.$colorActivo.sink { [weak self] c in self?.lienzo.colorPluma = c }.store(in: &observadoresBarra)
        UIAccessibility.post(notification: .announcement,
                             argument: "Modo de enfoque iniciado")
    }

    private func terminarFoco() {
        coordinadorFoco.terminar()
        ocultarInterfazFoco()
    }

    private func ocultarInterfazFoco() {
        mascaraFoco.isHidden = true
        controlFoco.isHidden = true
        aplicarInterfaz(CentroExperienciaIPad.compartido.instantanea().interfaz)
    }

    private func pedirPensamientoParking() {
        let alerta = UIAlertController(title: "Aparcar pensamiento", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Lo atenderé después" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Aparcar", style: .default) {
            [weak self] _ in
            guard let texto = alerta.textFields?.first?.text else { return }
            self?.coordinadorFoco.aparcar(texto)
        })
        present(alerta, animated: true)
    }

    private func abrirScratchGlobal() {
        presentarScratch(CentroExperienciaIPad.compartido.asegurarScratchGlobal())
    }

    private func presentarScratch(_ scratch: ScratchSpace) {
        let controlador = ControladorScratchGlobalIPad(scratch: scratch)
        present(UINavigationController(rootViewController: controlador),
                animated: !configuracionInterfaz.reducirMovimiento)
    }

    @objc private func recibirEnlaceProfundo(_ notificacion: Notification) {
        guard let url = notificacion.object as? URL else { return }
        if let invitacion = InvitacionColaboracion.leer(url) {
            guard invitacion.documentoId == lienzo.sesion.documentoPersistenteId else {
                avisar("La invitación corresponde a otro cuaderno. Ábralo antes de unirse.")
                return
            }
            let panel = PanelColaboracionLocal(
                coordinador: colaboracion,
                documentoId: lienzo.sesion.documentoPersistenteId)
            present(UINavigationController(rootViewController: panel), animated: true) {
                panel.ofrecer(invitacion)
            }
            return
        }
        guard let destino = DestinoEnlaceTinta.leer(url) else { return }
        guard destino.documentoId == lienzo.sesion.documentoPersistenteId else {
            avisar("El enlace apunta a otro cuaderno. El destino se conserva, pero todavía no está abierto.")
            return
        }
        if let elemento = destino.elementoId,
           let marco = navegador.capaElementos.marcoElemento(id: elemento) {
            navegador.mostrarRegion(marco.insetBy(dx: -70, dy: -70), animado: true)
        } else if let bloqueId = destino.bloqueId,
                  let bloque = navegador.capaBloques.bloques.first(where: {
                      $0.id == bloqueId
                  }) {
            navegador.mostrarRegion(bloque.marco.insetBy(dx: -70, dy: -70),
                                    animado: true)
        } else if let x = destino.x, let y = destino.y {
            let lado = max(120, 700 / max(destino.zoom ?? 1, 0.2))
            navegador.mostrarRegion(
                CGRect(x: x - lado / 2, y: y - lado / 2,
                       width: lado, height: lado), animado: true)
        }
        if let segundo = destino.segundoAudio {
            avisar(String(format: "Este enlace cita el audio en %02d:%02d.",
                          Int(segundo) / 60, Int(segundo) % 60))
        }
    }

    private func actualizarIndicadorAutoguardado(_ estado: EstadoAutoguardado) {
        var configuracion = indicadorAutoguardado.configuration ?? .plain()
        configuracion.title = estado.texto
        configuracion.image = UIImage(systemName: estado.simbolo)
        configuracion.imagePadding = 5
        indicadorAutoguardado.configuration = configuracion
        indicadorAutoguardado.accessibilityLabel = "Estado del documento: \(estado.texto)"
        indicadorAutoguardado.accessibilityValue = estado.detalleAccesible
        if case .fallo = estado {
            indicadorAutoguardado.tintColor = .systemRed
        } else if case .guardando = estado {
            indicadorAutoguardado.tintColor = .systemOrange
        } else {
            indicadorAutoguardado.tintColor = .systemGreen
        }
    }

    private func ofrecerRecuperacionPendiente() {
        let alerta = UIAlertController(
            title: "Se encontró una sesión sin cierre confirmado",
            message: "Hay un autoguardado íntegro en el iPad. Puede restaurarlo, revisarlo o conservar el estado actual; ninguna opción elimina la copia.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Restaurar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            _ = GestorVersionesDocumento.compartido.crear(
                nombre: "Antes de recuperación automática",
                motivo: "respaldo de seguridad",
                sesion: self.lienzo.sesion,
                bloques: self.navegador.capaBloques.bloques,
                tecnicos: self.navegador.capaTecnica.objetos,
                capas: RepositorioCapas.compartido.estado(
                    self.lienzo.sesion.documentoPersistenteId))
            self.autoguardado.autorizarYRestaurar { [weak self] resultado in
                switch resultado {
                case .success: self?.avisar("Se restauró la última generación íntegra.")
                case .failure(let error): self?.avisar(error.localizedDescription)
                }
            }
        })
        alerta.addAction(UIAlertAction(title: "Revisar diagnóstico", style: .default) {
            [weak self] _ in
            guard let self else { return }
            self.abrirSeguridad(self)
        })
        alerta.addAction(UIAlertAction(title: "Conservar estado actual", style: .cancel))
        present(alerta, animated: true)
    }

    @objc private func queHagoAhora(_ sender: Any) {
        guard let tarea = CentroAcademico.compartido.siguienteAccion(
            minutos: 30, energia: .normal) else {
            avisar("No hay una próxima acción elegible. Use Capturar primero para vaciar lo pendiente.")
            return
        }
        let alerta = UIAlertController(title: "Una sola próxima acción",
                                       message: tarea.titulo, preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(title: "Empezar 5 minutos", style: .default) { [weak self] _ in
            let foco = ControladorFoco(objetivo: tarea.titulo, minutos: 5, tareaId: tarea.id)
            self?.present(UINavigationController(rootViewController: foco), animated: true)
        })
        alerta.addAction(UIAlertAction(title: "Abrir origen", style: .default) { [weak self] _ in
            if let fuente = tarea.referencia { self?.abrirReferencia(fuente) }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    @objc private func recuperarCincoMinutos(_ sender: Any) {
        guard let sesion = sesionAcademica else {
            avisar("Primero inicie una sesión de clase desde Estudio.")
            return
        }
        let resumen = CentroAcademico.compartido.recuperarUltimosCincoMinutos(
            sesionId: sesion.id)
        let texto = resumen.texto + (resumen.advertencias.isEmpty
            ? "" : "\n\n" + resumen.advertencias.joined(separator: "\n"))
        avisar(texto)
    }

    private func programarGuardadoEstado() {
        guardadoEstadoPendiente?.cancel()
        let trabajo = DispatchWorkItem { [weak self] in self?.guardarEstadoExacto() }
        guardadoEstadoPendiente = trabajo
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.7, execute: trabajo)
    }

    private func guardarEstadoExacto() {
        let estado = EstadoSesionExacto(
            documentoId: lienzo.sesion.documentoPersistenteId,
            vista: navegador.estadoActual,
            herramienta: lienzo.herramienta.nombre,
            colorARGB: lienzo.colorPluma, grosor: lienzo.anchoPluma,
            segundoAudio: grabadora.segundosGrabados,
            panelAbierto: ajustesPanel.visible ? "casio" : nil)
        CentroAcademico.compartido.guardarEstado(estado)
        let punto = navegador.centroVisibleDocumento
        let espacio = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        GestorContinuidadIPad.compartido.publicar(
            destino: DestinoEnlaceTinta(
                documentoId: lienzo.sesion.documentoPersistenteId,
                elementoId: nil, bloqueId: nil,
                x: punto.x, y: punto.y, zoom: navegador.estadoActual.zoom,
                segundoAudio: grabadora.estado == .detenida
                    ? nil : grabadora.segundosGrabados,
                titulo: espacio?.nombre ?? title),
            titulo: "Continuar \(espacio?.nombre ?? title ?? "apunte")")
    }

    @objc private func entrarSegundoPlano() {
        guardarEstadoExacto()
        guardarReentrada()
        autoguardado.guardarAhora(motivo: "app en segundo plano")
    }

    @objc private func regresarPrimerPlano() {
        guard presentedViewController == nil else { return }
        reentradaOfrecida = false
        ofrecerReentradaSiCorresponde()
    }

    private func registrarDocumentoActual() {
        let espacio = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        let titulo = espacio?.nombre ?? "Apunte \(lienzo.sesion.documentoPersistenteId)"
        CentroExperienciaIPad.compartido.registrarDocumento(
            lienzo.sesion.documentoPersistenteId, titulo: titulo,
            cursoId: espacio?.cursoId)
    }

    private func guardarReentrada() {
        let espacio = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        let estado = ReentradaIPad(
            documentoId: lienzo.sesion.documentoPersistenteId,
            titulo: espacio?.nombre ?? title ?? "Apunte",
            cursoId: espacio?.cursoId,
            objetivo: coordinadorFoco.configuracion?.objetivo,
            accionPendiente: sesionAcademica?.objetivo,
            vista: navegador.estadoActual)
        CentroExperienciaIPad.compartido.guardarReentrada(estado)
    }

    private func ofrecerReentradaSiCorresponde() {
        guard !reentradaOfrecida,
              let reentrada = CentroExperienciaIPad.compartido.instantanea().reentrada,
              Date().timeIntervalSince(reentrada.fecha) < 48 * 3_600,
              reentrada.documentoId == lienzo.sesion.documentoPersistenteId else { return }
        reentradaOfrecida = true
        let objetivo = reentrada.objetivo.flatMap { $0.isEmpty ? nil : $0 }
        let mensaje = ["Última posición: \(reentrada.titulo)", objetivo]
            .compactMap { $0 }.joined(separator: "\n")
        let alerta = UIAlertController(title: "Retomar donde estaba",
                                       message: mensaje, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Volver", style: .default) {
            [weak self] _ in self?.navegador.restaurar(reentrada.vista, animado: true)
        })
        alerta.addAction(UIAlertAction(title: "Ahora no", style: .cancel))
        present(alerta, animated: true)
    }

    private func ofrecerCursoAutomatico() {
        guard presentedViewController == nil, !coordinadorFoco.activo,
              let curso = CentroAcademico.compartido.cursoEnHorario(),
              cursoAutomaticoOfrecido != curso.id else { return }
        cursoAutomaticoOfrecido = curso.id
        let espacio = CentroExperienciaIPad.compartido.asegurarEspacio(para: curso)
        if CentroExperienciaIPad.compartido.instantanea().espacioActivo?.id == espacio.id {
            aplicarEspacio(espacio)
            if sesionAcademica == nil {
                sesionAcademica = CentroAcademico.compartido.iniciarClase(
                    documentoId: lienzo.sesion.documentoPersistenteId)
            }
            return
        }
        let alerta = UIAlertController(
            title: "Clase detectada: \(curso.nombre)",
            message: "El horario coincide. Puede activar su papel, plumas y perfil de clase, o conservar el espacio actual.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Conservar espacio actual", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Usar espacio de la clase", style: .default) {
            [weak self] _ in
            guard let self else { return }
            self.aplicarEspacio(espacio)
            self.sesionAcademica = CentroAcademico.compartido.iniciarClase(
                documentoId: self.lienzo.sesion.documentoPersistenteId)
        })
        present(alerta, animated: true)
    }

    private func restaurarEstado(_ estado: EstadoSesionExacto) {
        guard estado.documentoId == lienzo.sesion.documentoPersistenteId else {
            avisar("El estado pertenece a otro documento abierto.")
            return
        }
        navegador.restaurar(estado.vista, animado: true)
        lienzo.colorPluma = estado.colorARGB
        lienzo.anchoPluma = estado.grosor
        if let herramienta = HerramientaLienzo.allCases.first(where: {
            $0.nombre == estado.herramienta
        }) {
            aplicarHerramienta(herramienta)
        }
        if estado.panelAbierto == "casio", !ajustesPanel.visible {
            ajustesPanel.visible = true
            aplicarReparto()
        }
    }

    private func abrirReferencia(_ referencia: ReferenciaFuente) {
        guard referencia.documentoId == nil
                || referencia.documentoId == lienzo.sesion.documentoPersistenteId else {
            avisar("La fuente pertenece a otro documento; se conserva el enlace profundo.")
            return
        }
        if let id = referencia.elementoId,
           let marco = marcoDeReferencia(elementoId: id) {
            navegador.mostrarRegion(marco.insetBy(dx: -80, dy: -80), animado: true)
        } else if let segundo = referencia.segundoAudio {
            avisar(String(format: "Fuente de audio en %02d:%02d",
                          Int(segundo) / 60, Int(segundo) % 60))
        }
    }

    private func marcoDeReferencia(elementoId: String) -> CGRect? {
        if elementoId.hasPrefix("t:") {
            let idTrazo = String(elementoId.dropFirst(2))
            for indice in 0..<lienzo.sesion.cantidadTrazos
            where lienzo.sesion.idTrazo(indice) == idTrazo {
                return lienzo.sesion.marcoTrazo(indice)
            }
            return nil
        }
        let limpio = (elementoId.hasPrefix("x:") || elementoId.hasPrefix("m:"))
            ? String(elementoId.dropFirst(2)) : elementoId
        return navegador.capaElementos.marcoElemento(id: limpio)
    }

    // MARK: - Escribir a teclado o a voz

    @objc private func cambiarModo(_ selector: UISegmentedControl) {
        let modo = ModoEscritura(rawValue: selector.selectedSegmentIndex) ?? .lapiz
        switch modo {
        case .lapiz:
            dictado.detener()
        case .teclado:
            dictado.detener()
            pedirTexto()
        case .voz:
            comenzarDictado()
        }
    }

    private func pedirTexto() {
        let alerta = UIAlertController(title: "Escribir a teclado", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField()
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel) { [weak self] _ in
            self?.selectorModo.selectedSegmentIndex = 0
        })
        alerta.addAction(UIAlertAction(title: "Poner", style: .default) { [weak self] _ in
            guard let self, let texto = alerta.textFields?.first?.text,
                  !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
            if let id = self.navegador.capaElementos.insertarTexto(
                texto, en: self.navegador.centroVisibleDocumento) {
                self.indexarTexto(texto, elementoId: id, tipo: .apuntePropio,
                                  titulo: "Texto escrito")
            }
            self.selectorModo.selectedSegmentIndex = 0
        })
        present(alerta, animated: true)
    }

    private func comenzarDictado() {
        DictadoEnVivo.pedirPermisos { [weak self] concedido in
            guard let self else { return }
            guard concedido else {
                self.selectorModo.selectedSegmentIndex = 0
                self.avisar("Sin permiso de dictado o de micrófono.")
                return
            }
            self.textoDictado = ""
            self.dictado.alTranscribir = { [weak self] texto, final in
                self?.textoDictado = texto
                self?.etiquetaGrabacion.isHidden = false
                self?.etiquetaGrabacion.textColor = .systemBlue
                self?.etiquetaGrabacion.text = final ? texto : "… \(texto)"
                if final, let self,
                   !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    if let id = self.navegador.capaElementos.insertarTexto(
                        texto, en: self.navegador.centroVisibleDocumento,
                        dictado: true) {
                        self.indexarTexto(texto, elementoId: id, tipo: .transcripcion,
                                          titulo: "Dictado")
                    }
                    self.selectorModo.selectedSegmentIndex = 0
                }
            }
            self.dictado.alFallar = { [weak self] error in
                self?.avisar(error.localizedDescription)
                self?.selectorModo.selectedSegmentIndex = 0
            }
            do {
                try self.dictado.comenzar()
            } catch {
                self.avisar(error.localizedDescription)
                self.selectorModo.selectedSegmentIndex = 0
            }
        }
    }

    // MARK: - Importar y exportar

    private func abrirSelectorFotos(limite: Int = 0, editar: Bool = false) {
        editarSiguienteImagenImportada = editar
        present(Medios.selectorDeFotos(limite: limite, delegado: self),
                animated: true)
    }

    private func tomarFoto() {
        guard UIImagePickerController.isSourceTypeAvailable(.camera) else {
            avisar("La cámara no está disponible en este iPad.")
            return
        }
        let camara = UIImagePickerController()
        camara.sourceType = .camera
        camara.cameraCaptureMode = .photo
        camara.delegate = self
        camara.modalPresentationStyle = .fullScreen
        present(camara, animated: true)
    }

    private func importarFotosRecientes(_ cantidad: Int) {
        let continuar: (PHAuthorizationStatus) -> Void = { [weak self] estado in
            guard let self else { return }
            guard estado == .authorized || estado == .limited else {
                self.avisar("Para insertar fotos recientes, permita acceso a Fotos. También puede usar “Elegir fotos” sin dar acceso a toda la galería.")
                return
            }
            let opciones = PHFetchOptions()
            opciones.sortDescriptors = [NSSortDescriptor(
                key: "creationDate", ascending: false)]
            opciones.fetchLimit = max(1, min(cantidad, 25))
            let resultado = PHAsset.fetchAssets(with: .image, options: opciones)
            var recursos: [PHAsset] = []
            resultado.enumerateObjects { recurso, _, _ in recursos.append(recurso) }
            guard !recursos.isEmpty else {
                self.avisar("No hay fotos disponibles dentro del acceso concedido.")
                return
            }

            let gestor = PHImageManager.default()
            let opcionesImagen = PHImageRequestOptions()
            opcionesImagen.isNetworkAccessAllowed = true
            opcionesImagen.deliveryMode = .highQualityFormat
            let grupo = DispatchGroup()
            let candado = NSLock()
            var imagenes = Array<UIImage?>(repeating: nil, count: recursos.count)
            for (indice, recurso) in recursos.enumerated() {
                grupo.enter()
                gestor.requestImageDataAndOrientation(
                    for: recurso, options: opcionesImagen) { datos, _, _, _ in
                        let imagen = datos.flatMap(UIImage.init(data:))
                        candado.lock(); imagenes[indice] = imagen; candado.unlock()
                        grupo.leave()
                    }
            }
            grupo.notify(queue: .main) { [weak self] in
                guard let self else { return }
                let disponibles = imagenes.compactMap { $0 }
                guard !disponibles.isEmpty else {
                    self.avisar("No se pudieron leer las fotos seleccionadas.")
                    return
                }
                self.insertarImagenes(disponibles, nombreBase: "Foto reciente")
            }
        }

        let estado = PHPhotoLibrary.authorizationStatus(for: .readWrite)
        if estado == .notDetermined {
            PHPhotoLibrary.requestAuthorization(for: .readWrite) { nuevo in
                DispatchQueue.main.async { continuar(nuevo) }
            }
        } else {
            continuar(estado)
        }
    }

    @objc private func importar(_ sender: Any) {
        let hoja = UIAlertController(title: "Importar", message: nil,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Fotos y vídeos", style: .default) { [weak self] _ in
            guard let self else { return }
            self.present(Medios.selectorDeFotos(delegado: self), animated: true)
        })
        hoja.addAction(UIAlertAction(title: "PDF, Word y otros", style: .default) { [weak self] _ in
            guard let self else { return }
            self.present(Medios.selectorDeDocumentos(delegado: self), animated: true)
        })
        hoja.addAction(UIAlertAction(title: "Carpeta, libro o migración", style: .default) {
            [weak self] _ in
            guard let self else { return }
            self.abrirBibliotecaFuentes(self)
        })
        if Medios.hayImagenQuePegar {
            hoja.addAction(UIAlertAction(title: "Pegar imagen", style: .default) { [weak self] _ in
                let imagenes = Medios.imagenesDelPortapapeles()
                self?.insertarImagenes(imagenes)
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    @objc private func escanearPizarra(_ sender: Any) {
        guard VNDocumentCameraViewController.isSupported else {
            avisar("El escáner de documentos no está disponible en este dispositivo.")
            return
        }
        let escaner = VNDocumentCameraViewController()
        escaner.delegate = self
        present(escaner, animated: true)
    }

    @objc private func recibirCapturaRemota(_ nota: Notification) {
        guard let captura = nota.object as? CapturaRemotaRecibida,
              let imagen = UIImage(contentsOfFile: captura.adjunto.url.path) else { return }
        if captura.metadatos.corregirPizarra {
            ProcesadorPizarraLocal.procesar(imagen) { [weak self] resultado in
                DispatchQueue.main.async {
                    guard let self else { return }
                    switch resultado {
                    case .success(let pizarra):
                        if captura.metadatos.tipo == .diapositiva {
                            self.sugerirSlideOriginal(
                                para: pizarra, fotoOriginal: captura.adjunto)
                        } else {
                            self.insertarResultadoPizarra(
                                pizarra, original: captura.adjunto,
                                tipo: captura.metadatos.tipo)
                        }
                    case .failure(let error):
                        self.avisar("La foto llegó, pero no se pudo corregir: \(error.localizedDescription)")
                        _ = self.navegador.capaElementos.insertarAdjunto(
                            captura.adjunto, tipo: .imagen,
                            en: self.navegador.centroVisibleDocumento,
                            tamano: Medios.tamanoDeInsercion(imagen.size))
                    }
                }
            }
        } else if let id = navegador.capaElementos.insertarAdjunto(
            captura.adjunto, tipo: .imagen, en: navegador.centroVisibleDocumento,
            tamano: Medios.tamanoDeInsercion(imagen.size)) {
            indexarImagen(imagen, elementoId: id, titulo: captura.metadatos.nombre)
        }
    }

    private func sugerirSlideOriginal(
        para resultado: ResultadoPizarra,
        fotoOriginal: AdjuntoImportado
    ) {
        RepositorioDiapositivasIPad.compartido.buscar(foto: resultado.imagen) {
            [weak self] respuesta in
            DispatchQueue.main.async {
                guard let self else { return }
                guard case .success(let coincidencias) = respuesta,
                      let mejor = coincidencias.first,
                      mejor.puntuacion >= 0.42,
                      let adjunto = RepositorioDiapositivasIPad.compartido.adjunto(
                        de: mejor.diapositiva),
                      let imagen = UIImage(contentsOfFile: adjunto.url.path) else {
                    self.insertarResultadoPizarra(
                        resultado, original: fotoOriginal, tipo: .diapositiva)
                    return
                }
                _ = self.detectorSlide.evaluar(mejor)
                let slide = mejor.diapositiva
                let alerta = UIAlertController(
                    title: "¿Usar la slide original?",
                    message: String(
                        format: "Parece %@, diapositiva %d (coincidencia %.0f%%). La foto se conservará como referencia y nada se sustituye sin su confirmación.",
                        slide.tituloPresentacion, slide.numero,
                        mejor.puntuacion * 100),
                    preferredStyle: .alert)
                alerta.addAction(UIAlertAction(
                    title: "Usar original", style: .default) { [weak self] _ in
                    self?.insertarSlideOriginal(
                        slide, adjunto: adjunto, imagen: imagen,
                        fotoOriginal: fotoOriginal)
                })
                alerta.addAction(UIAlertAction(
                    title: "Conservar foto corregida", style: .default) { [weak self] _ in
                    self?.insertarResultadoPizarra(
                        resultado, original: fotoOriginal, tipo: .diapositiva)
                })
                alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
                self.present(alerta, animated: true)
            }
        }
    }

    private func insertarSlideOriginal(
        _ slide: DiapositivaLocalIPad,
        adjunto: AdjuntoImportado,
        imagen: UIImage,
        fotoOriginal: AdjuntoImportado?
    ) {
        guard let elemento = navegador.capaElementos.insertarAdjunto(
            adjunto, tipo: .imagen, en: navegador.centroVisibleDocumento,
            tamano: Medios.tamanoDeInsercion(imagen.size)) else { return }
        indexarTexto(
            slide.textoExtraido, elementoId: elemento,
            tipo: .diapositivaProfesor,
            titulo: "\(slide.tituloPresentacion) — \(slide.numero)",
            hash: slide.hashFuente)
        grabadora.marcar(
            .cambioDiapositiva,
            titulo: "\(slide.tituloPresentacion) — slide \(slide.numero)",
            pagina: slide.numero, punto: navegador.centroVisibleDocumento,
            recursoId: slide.hashImagen, diapositivaId: slide.id)
        registrarDiapositivaAcademica(slide, elementoId: elemento)
        if let fotoOriginal {
            RepositorioDiapositivasIPad.compartido.registrarVinculo(
                VinculoFotoDiapositivaIPad(
                    fotoHash: fotoOriginal.hash,
                    diapositivaId: slide.id,
                    elementoId: UUID(uuidString: elemento),
                    segundoNotaViva: grabadora.estado == .detenida
                        ? nil : grabadora.segundosGrabados))
        }
        autoguardado.marcarCambio()
    }

    private func insertarResultadoPizarra(_ resultado: ResultadoPizarra,
                                          original: AdjuntoImportado,
                                          tipo: TipoCapturaRemota) {
        do {
            let procesada = try RepositorioAdjuntos.compartido.importar(
                imagen: resultado.imagen, nombre: "Corregida-\(original.nombreOriginal).png")
            let id = navegador.capaElementos.insertarAdjunto(
                procesada, tipo: .imagen, en: navegador.centroVisibleDocumento,
                tamano: Medios.tamanoDeInsercion(resultado.imagen.size))
            if let id = id, UUID(uuidString: id) != nil {
                indexarTexto(resultado.texto.map(\.texto).joined(separator: "\n"),
                              elementoId: id,
                              tipo: tipo == .pizarra ? .pizarra : .diapositivaProfesor,
                              titulo: original.nombreOriginal, hash: original.hash)
                var ajustes = RepositorioEdicionesImagen.compartido.ajustes(id: id)
                ajustes.ocr = resultado.texto
                RepositorioEdicionesImagen.compartido.guardar(ajustes, id: id)
            }
            fusionPizarra.agregar(resultado.imagen)
            grabadora.marcar(tipo == .pizarra ? .fotoPizarra : .cambioDiapositiva,
                             titulo: original.nombreOriginal,
                             punto: navegador.centroVisibleDocumento)
            autoguardado.marcarCambio()
            if resultado.hayPersonaTapando, fusionPizarra.cantidadFotos >= 3 {
                let punto = CGPoint(x: navegador.centroVisibleDocumento.x + 36,
                                    y: navegador.centroVisibleDocumento.y + 36)
                fusionPizarra.fusionar { [weak self] fusionada in
                    guard let fusionada,
                          let adjunto = try? RepositorioAdjuntos.compartido.importar(
                            imagen: fusionada, nombre: "Pizarra-fusionada.png")
                    else { return }
                    DispatchQueue.main.async {
                        guard let self else { return }
                        _ = self.navegador.capaElementos.insertarAdjunto(
                            adjunto, tipo: .imagen, en: punto,
                            tamano: Medios.tamanoDeInsercion(fusionada.size))
                        self.autoguardado.marcarCambio()
                    }
                }
            }
        } catch { avisar(error.localizedDescription) }
    }

    @objc private func recibirComandoRemoto(_ nota: Notification) {
        guard let comando = nota.object as? ComandoRemoto else { return }
        switch comando {
        case .mePerdi: marcarMePerdi(self)
        case .importante: marcarImportante(self)
        case .examen: marcarExamen(self)
        case .deshacer: lienzo.deshacer()
        case .siguienteSlide:
            grabadora.marcar(.cambioDiapositiva, titulo: "Siguiente slide")
        case .anteriorSlide:
            grabadora.marcar(.cambioDiapositiva, titulo: "Slide anterior")
        case .foto: break
        }
    }

    @discardableResult
    private func insertarImagenes(_ imagenes: [UIImage],
                                  nombreBase: String = "Imagen") -> [String] {
        let centro = navegador.centroVisibleDocumento
        var insertadas: [String] = []
        for (indice, imagen) in imagenes.enumerated() {
            do {
                let adjunto = try RepositorioAdjuntos.compartido.importar(
                    imagen: imagen, nombre: "\(nombreBase)-\(indice + 1).png")
                let punto = CGPoint(x: centro.x + CGFloat(indice % 3) * 28,
                                    y: centro.y + CGFloat(indice) * 32)
                if let id = navegador.capaElementos.insertarAdjunto(
                    adjunto, tipo: .imagen, en: punto,
                    tamano: Medios.tamanoDeInsercion(imagen.size)) {
                    insertadas.append(id)
                    indexarImagen(imagen, elementoId: id, titulo: adjunto.nombreOriginal)
                }
            } catch {
                avisar("No se pudo guardar una imagen: \(error.localizedDescription)")
            }
        }
        if !insertadas.isEmpty { autoguardado.marcarCambio() }
        return insertadas
    }

    private func abrirEditorImagen(_ medio: ElementoMedio) {
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash),
              let imagen = UIImage(contentsOfFile: url.path) else {
            avisar("No se encontró la imagen original para editarla.")
            return
        }
        let editor = EditorImagen(medio: medio, imagen: imagen, sesion: lienzo.sesion)
        editor.alGuardar = { [weak self] in
            guard let self else { return }
            self.navegador.recargarElementos()
            let ajustes = RepositorioEdicionesImagen.compartido.ajustes(id: medio.id)
            self.indexarTexto(ajustes.ocr.map(\.texto).joined(separator: "\n"),
                              elementoId: medio.id, tipo: .apuntePropio,
                              titulo: medio.nombre, hash: medio.hash)
        }
        present(UINavigationController(rootViewController: editor), animated: true)
    }

    private func abrirEditorFondo(_ medio: ElementoMedio) {
        let documento = lienzo.sesion.documentoPersistenteId
        let anteriores = RepositorioFondosObjetoIPad.compartido.ajustes(
            documento: documento, elemento: medio.id)
        let panel = PanelAjustesFondoObjetoIPad(ajustes: anteriores)
        panel.alAplicar = { [weak self] nuevos in
            guard let self else { return }
            var actualizado = self.lienzo.sesion.medios.first {
                $0.id == medio.id
            } ?? medio
            let relacion = nuevos.escala / max(anteriores.escala, 0.01)
            if abs(relacion - 1) > 0.001 {
                let centro = CGPoint(x: actualizado.marco.midX,
                                     y: actualizado.marco.midY)
                actualizado.marco.size = CGSize(
                    width: min(max(actualizado.marco.width * relacion, 44), 8_000),
                    height: min(max(actualizado.marco.height * relacion, 44), 8_000))
                actualizado.marco.origin = CGPoint(
                    x: centro.x - actualizado.marco.width / 2,
                    y: centro.y - actualizado.marco.height / 2)
                _ = self.lienzo.sesion.actualizarMedio(actualizado)
            }
            RepositorioFondosObjetoIPad.compartido.guardar(
                nuevos, documento: documento,
                elemento: medio.id, nombre: medio.nombre)
            self.navegador.recargarElementos()
            self.autoguardado.marcarCambio()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func abrirAjustesTexto(_ texto: ElementoTexto) {
        let actuales = RepositorioAjustesTextoIPad.compartido.ajustes(texto.id)
        let panel = PanelAjustesCajaTextoIPad(ajustes: actuales)
        panel.alAplicar = { [weak self] ajustes in
            RepositorioAjustesTextoIPad.compartido.guardar(
                ajustes, id: texto.id)
            self?.navegador.recargarElementos()
            self?.autoguardado.marcarCambio()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    private func abrirOriginal(_ medio: ElementoMedio, peek: Bool) {
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash) else {
            avisar("El original no está disponible en el almacenamiento local.")
            return
        }
        let panel = ControladorPeekFuenteIPad(url: url, pagina: medio.paginaFuente)
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = peek ? .popover : .pageSheet
        if peek {
            navegacion.preferredContentSize = CGSize(width: 680, height: 760)
            navegacion.popoverPresentationController?.sourceView = view
        }
        present(navegacion, animated: true)
    }

    private func indexarTexto(_ texto: String, elementoId: String?, tipo: TipoFuente,
                              titulo: String, hash: String? = nil, pagina: Int? = nil) {
        guard !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        let fuente = ReferenciaFuente(
            tipo: tipo, titulo: titulo, cursoId: sesionAcademica?.cursoId,
            sesionId: sesionAcademica?.id,
            documentoId: lienzo.sesion.documentoPersistenteId, elementoId: elementoId,
            hashArchivo: hash, pagina: pagina)
        CentroAcademico.compartido.agregarNodo(NodoConocimiento(
            titulo: titulo, texto: texto, fuente: fuente,
            etiquetas: EtiquetadorLocal.etiquetas(texto)))
    }

    private func indexarImagen(_ imagen: UIImage, elementoId: String, titulo: String) {
        AnalizadorImagenLocal.reconocerTexto(imagen) { [weak self] resultado in
            guard let self, case .success(let bloques) = resultado else { return }
            let texto = bloques.map(\.texto).joined(separator: "\n")
            self.indexarTexto(texto, elementoId: elementoId, tipo: .apuntePropio,
                              titulo: titulo)
            var ajustes = RepositorioEdicionesImagen.compartido.ajustes(id: elementoId)
            ajustes.ocr = bloques
            RepositorioEdicionesImagen.compartido.guardar(ajustes, id: elementoId)
        }
    }

    /// Exporta la hoja a un PDF de verdad: la tinta va como polígonos y la pauta
    /// dibujada, así que se puede ampliar sin que se pixele.
    @objc private func exportar(_ sender: Any) {
        let panel = PanelExportacionCompleta(sesion: lienzo.sesion,
                                             navegador: navegador,
                                             presentador: self)
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    // MARK: - Nuevos paneles integrados
    @objc private func abrirVentanaDividida(_ sender: Any) {
        if let raiz = splitViewController as? ControladorRaizIPad {
            raiz.alternarReferencia()
            return
        }
        guard let divisor = splitViewController else {
            avisar("La segunda columna no está disponible en esta ventana.")
            return
        }
        divisor.preferredSplitBehavior = .tile
        divisor.preferredDisplayMode = .oneBesideSecondary
        divisor.show(.secondary)
    }

    @objc private func abrirMenuRadial(_ sender: Any) {
        mostrarMenuRadial()
    }

    @objc private func abrirNotasAdhesivas(_ sender: Any) {
        let alerta = UIAlertController(
            title: "Nueva nota sobre el lienzo",
            message: "Elija el tipo. Después puede tocarla para editarla o eliminarla.",
            preferredStyle: .actionSheet)
        for tipo in TipoNotaAdhesivaIPad.allCases {
            alerta.addAction(UIAlertAction(
                title: tipo.nombre,
                style: .default) { [weak self] _ in
                guard let self else { return }
                let nota = self.navegador.capaNotasAdhesivas.agregar(
                    tipo, en: self.navegador.centroVisibleDocumento)
                self.editarNotaAdhesiva(nota)
            })
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        alerta.popoverPresentationController?.sourceRect = CGRect(
            x: view.bounds.midX, y: view.bounds.midY, width: 1, height: 1)
        present(alerta, animated: true)
    }

    @objc private func abrirCarpetasInteligentes(_ sender: Any) {
        present(UINavigationController(
            rootViewController: PanelCarpetasInteligentesIPad()), animated: true)
    }

    private func editarNotaAdhesiva(_ nota: NotaAdhesivaLienzo) {
        let alerta = UIAlertController(
            title: nota.tipo.nombre,
            message: "El cambio se guarda únicamente en este documento.",
            preferredStyle: .alert)
        alerta.addTextField { campo in
            campo.text = nota.texto
            campo.placeholder = nota.tipo == .comentario
                ? "Comentario personal" : "Texto de la nota"
            campo.clearButtonMode = .whileEditing
        }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            var actualizada = nota
            actualizada.texto = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
            RepositorioNotasAdhesivasIPad.compartido.actualizar(
                actualizada, documento: self.lienzo.sesion.documentoPersistenteId)
            self.navegador.capaNotasAdhesivas.recargar()
        })
        alerta.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self] _ in
            guard let self else { return }
            RepositorioNotasAdhesivasIPad.compartido.eliminar(
                nota.id, documento: self.lienzo.sesion.documentoPersistenteId)
            self.navegador.capaNotasAdhesivas.recargar()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    // MARK: - Grabación

    @objc private func abrirAjustes(_ sender: Any) {
        let panel = PanelGrabacion(ajustes: ajustesGrabacion, pantalla: ajustesPantalla,
                                   seleccionMicrofono: seleccionMicrofono,
                                   capacidades: capacidades)
        panel.alAceptar = { [weak self] ajustes, pantalla, microfono in
            self?.ajustesGrabacion = ajustes
            self?.ajustesPantalla = pantalla
            self?.seleccionMicrofono = microfono
            PreferenciasCaptura.microfono = microfono
            self?.aplicarPantalla()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }

    @objc private func alternarGrabacion(_ sender: Any) {
        switch grabadora.estado {
        case .detenida:
            if sesionAcademica == nil {
                sesionAcademica = CentroAcademico.compartido.iniciarClase(
                    documentoId: lienzo.sesion.documentoPersistenteId)
            }
            let carpeta = try? FileManager.default.url(
                for: .documentDirectory, in: .userDomainMask,
                appropriateFor: nil, create: true)
            let destino = (carpeta ?? FileManager.default.temporaryDirectory)
                .appendingPathComponent("Grabaciones", isDirectory: true)

            grabadora.comenzar(ajustes: ajustesGrabacion, capacidades: capacidades,
                               seleccionMicrofono: seleccionMicrofono,
                               carpeta: destino, sesion: lienzo.sesion) { [weak self] resultado in
                if case .failure(let error) = resultado {
                    self?.avisar(error.localizedDescription)
                }
            }
        case .grabando, .pausada:
            grabadora.detener { [weak self] resultado in
                switch resultado {
                case .success(let url):
                    self?.ultimaGrabacion = url
                    if let sesionId = self?.sesionAcademica?.id {
                        CentroAcademico.compartido.transaccion { base in
                            if let i = base.sesiones.firstIndex(where: { $0.id == sesionId }) {
                                base.sesiones[i].fin = Date()
                                if url.pathExtension.lowercased() == "m4a" {
                                    base.sesiones[i].archivoAudio = url.path
                                } else if url.lastPathComponent == "manifest.json" {
                                    base.sesiones[i].archivoNotaViva = url.deletingLastPathComponent().path
                                }
                            }
                        }
                    }
                    self?.avisar("Grabación guardada: \(url.lastPathComponent)")
                case .failure(let error):
                    self?.avisar(error.localizedDescription)
                }
            }
        }
    }

    @objc private func alternarPausaGrabacion(_ sender: Any) {
        switch grabadora.estado {
        case .detenida: break
        case .grabando: grabadora.pausar()
        case .pausada: grabadora.reanudar()
        }
    }

    @objc private func marcarMePerdi(_ sender: Any) {
        grabadora.marcar(.mePerdi, titulo: "Me perdí")
        registrarMarcaAcademica(.mePerdi, titulo: "Me perdí")
        navegador.agregarMarcador(titulo: "Me perdí",
                                   en: navegador.centroVisibleDocumento,
                                   color: 0xFFFF_3B30)
        mostrarMarcaBreve("?  Me perdí")
    }

    @objc private func marcarImportante(_ sender: Any) {
        grabadora.marcar(.importante, titulo: "Importante")
        registrarMarcaAcademica(.importante, titulo: "Importante")
        mostrarMarcaBreve("!  Importante")
    }

    @objc private func marcarExamen(_ sender: Any) {
        grabadora.marcar(.examen, titulo: "Probablemente examen")
        registrarMarcaAcademica(.examen, titulo: "Probablemente examen")
        mostrarMarcaBreve("✓  Examen")
    }

    private func registrarMarcaAcademica(_ tipo: TipoEventoAcademico, titulo: String) {
        guard let sesion = sesionAcademica else { return }
        let fuente = ReferenciaFuente(
            tipo: .audioClase, titulo: titulo, cursoId: sesion.cursoId,
            sesionId: sesion.id, documentoId: lienzo.sesion.documentoPersistenteId,
            segundoAudio: grabadora.segundosGrabados)
        CentroAcademico.compartido.transaccion { base in
            base.timeline.append(EventoAcademico(
                tipo: tipo, titulo: titulo, cursoId: sesion.cursoId,
                sesionId: sesion.id, referencia: fuente))
        }
    }

    private func accionApretonLapiz() {
        if configuracionEntrada.squeezeMuestraMenuRadial {
            mostrarMenuRadial()
        } else if grabadora.estado == .detenida {
            lienzo.deshacer()
            autoguardado.marcarCambio()
        } else {
            marcarImportante(self)
        }
    }

    private func mostrarMarcaBreve(_ texto: String) {
        guard grabadora.estado != .detenida else { return }
        etiquetaGrabacion.isHidden = false
        etiquetaGrabacion.textColor = .systemOrange
        etiquetaGrabacion.text = texto
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { [weak self] in
            guard let self, self.grabadora.estado != .detenida else { return }
            self.etiquetaGrabacion.textColor = .systemRed
        }
    }

    @objc private func reproducirUltimaGrabacion(_ sender: Any) {
        guard let url = ultimaGrabacion else {
            avisar("Todavía no hay una grabación terminada en esta sesión.")
            return
        }
        if url.lastPathComponent == "manifest.json" {
            let reproductor = ReproductorNotaViva(manifiestoURL: url)
            present(UINavigationController(rootViewController: reproductor), animated: true)
        } else {
            let reproductor = AVPlayerViewController()
            reproductor.player = AVPlayer(url: url)
            present(reproductor, animated: true) { reproductor.player?.play() }
        }
    }

    private func alCambiarGrabacion(_ estado: EstadoGrabacion) {
        aplicarPantalla()
        temporizador?.invalidate()

        guard estado != .detenida else {
            etiquetaGrabacion.isHidden = true
            return
        }
        etiquetaGrabacion.isHidden = false
        etiquetaGrabacion.textColor = .systemRed
        temporizador = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            guard let self else { return }
            let s = Int(self.grabadora.segundosGrabados)
            let mb = self.grabadora.megabytesEstimados(self.capacidades)
            self.etiquetaGrabacion.text = String(format: "● %02d:%02d · %.0f MB",
                                                 s / 60, s % 60, mb)
        }
    }

    // MARK: - Pantalla siempre activa

    private func marcarEscritura(_ activo: Bool) {
        escribiendo = activo
        if !activo { autoguardado.marcarCambio() }
        aplicarPantalla()
    }

    private func aplicarPantalla() {
        ajustesPantalla.aplicar(grabando: grabadora.estado != .detenida,
                                escribiendo: escribiendo)
    }

    // MARK: - Hospedaje en pestañas

    func configurarPersistenciaCanonica(
        _ persistidor: @escaping CoordinadorAutoguardado.PersistidorCanonico
    ) {
        autoguardado.configurarPersistidorCanonico(persistidor)
    }

    func datosPersistiblesDePestana() -> Data? {
        guardarEstadoExacto()
        return lienzo.sesion.guardar()
    }

    func cargarDatosDePestana(_ datos: Data) -> Bool {
        guard lienzo.sesion.cargar(datos) else { return false }
        sincronizarModoPaginaDesdeSesion()
        lienzo.limpiarSeleccion()
        navegador.sincronizarTamanoDocumento(conservarCentro: false)
        navegador.recargarElementos()
        navegador.capaBloques.recargar()
        navegador.capaTecnica.setNeedsDisplay()
        lienzo.rehornearYDibujar()
        // Abrir no es editar. Marcar esta carga como cambio podía crear una
        // generación nueva basada en el canónico viejo antes de que el usuario
        // aceptara recuperar una sesión interrumpida, ocultando el journal útil.
        return true
    }

    func cargarDatosComoCopiaDePestana(_ datos: Data) -> Bool {
        guard lienzo.sesion.cargarComoCopia(datos) else { return false }
        sincronizarModoPaginaDesdeSesion()
        lienzo.limpiarSeleccion()
        navegador.sincronizarTamanoDocumento(conservarCentro: false)
        navegador.recargarElementos()
        navegador.capaBloques.recargar()
        navegador.capaTecnica.setNeedsDisplay()
        lienzo.rehornearYDibujar()
        return true
    }

    var estadoVistaParaPestana: EstadoVistaLienzo { navegador.estadoActual }

    func restaurarVistaDePestana(_ estado: EstadoVistaLienzo) {
        navegador.restaurar(estado)
    }

    var regionVisibleParaNavegacion: CGRect { navegador.regionVisibleDocumento }
    var centroVisibleParaNavegacion: CGPoint { navegador.centroVisibleDocumento }

    func mostrarRegionDeNavegacion(_ region: CGRect) {
        navegador.mostrarRegion(region, animado: true)
    }

    func recibirItemsArrastrados(_ items: [UIDragItem]) {
        lienzo.importarItemsArrastrados(items, en: navegador.centroVisibleDocumento)
    }

    @discardableResult
    func ejecutarCapturaDelSistema(_ tipo: TipoEntradaSistemaIPad) -> Bool {
        switch tipo {
        case .capturarFoto:
            escanearPizarra(self); return true
        case .capturarVoz:
            selectorModo.selectedSegmentIndex = ModoEscritura.voz.rawValue
            cambiarModo(selectorModo); return true
        default:
            return false
        }
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Vale", style: .default))
        present(alerta, animated: true)
    }
}

// MARK: - Selectores del sistema

extension ControladorLienzo: UIImagePickerControllerDelegate,
                              UINavigationControllerDelegate {
    func imagePickerController(
        _ picker: UIImagePickerController,
        didFinishPickingMediaWithInfo info: [UIImagePickerController.InfoKey: Any]
    ) {
        guard let imagen = (info[.editedImage] ?? info[.originalImage]) as? UIImage else {
            picker.dismiss(animated: true)
            return
        }
        picker.dismiss(animated: true) { [weak self] in
            guard let self else { return }
            let ids = self.insertarImagenes([imagen], nombreBase: "Foto")
            guard let id = ids.first,
                  let medio = self.lienzo.sesion.medios.first(where: { $0.id == id })
            else { return }
            self.abrirEditorImagen(medio)
        }
    }

    func imagePickerControllerDidCancel(_ picker: UIImagePickerController) {
        picker.dismiss(animated: true)
    }
}

extension ControladorLienzo: PHPickerViewControllerDelegate {
    func picker(_ picker: PHPickerViewController, didFinishPicking results: [PHPickerResult]) {
        let editarAlInsertar = editarSiguienteImagenImportada && results.count == 1
        editarSiguienteImagenImportada = false
        picker.dismiss(animated: true)
        guard !results.isEmpty else { return }
        let centro = navegador.centroVisibleDocumento
        for (indice, resultado) in results.enumerated() {
            Medios.importar(resultado) { [weak self] importacion in
                guard let self else { return }
                switch importacion {
                case .success(let recurso):
                    let punto = CGPoint(x: centro.x + CGFloat(indice % 3) * 30,
                                        y: centro.y + CGFloat(indice) * 34)
                    let id = self.navegador.capaElementos.insertarAdjunto(
                        recurso.adjunto, tipo: recurso.tipo, en: punto,
                        tamano: recurso.tamanoSugerido)
                    if recurso.tipo == .imagen, let id,
                       let imagen = UIImage(contentsOfFile: recurso.adjunto.url.path) {
                        self.indexarImagen(imagen, elementoId: id,
                                           titulo: recurso.adjunto.nombreOriginal)
                        if editarAlInsertar,
                           let medio = self.lienzo.sesion.medios.first(
                            where: { $0.id == id }) {
                            self.abrirEditorImagen(medio)
                        }
                    }
                    if id != nil { self.autoguardado.marcarCambio() }
                case .failure(let error):
                    self.avisar("No se pudo importar un recurso: \(error.localizedDescription)")
                }
            }
        }
    }
}

extension ControladorLienzo: UIDocumentPickerDelegate {
    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        if urls.count == 1, let url = urls.first,
           url.pathExtension.lowercased() == "pdf",
           let panel = PanelImportacionPDFAvanzadaIPad(url: url) {
            panel.alImportar = { [weak self] url, opciones in
                self?.insertarPrintoutPDF(url, opciones: opciones)
            }
            present(UINavigationController(rootViewController: panel), animated: true)
            return
        }
        var cursor = navegador.centroVisibleDocumento
        for url in urls {
            do {
                let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
                RepositorioProcedenciaIPad.compartido.registrar(
                    adjunto: adjunto, original: url, origen: .archivos)
                let tipo = Medios.tipoMedio(de: url)
                if tipo == .pdf, let documento = PDFDocument(url: adjunto.url),
                   documento.pageCount > 0 {
                    for indice in 0..<documento.pageCount {
                        guard let pagina = documento.page(at: indice) else { continue }
                        let caja = pagina.bounds(for: .mediaBox)
                        let tamano = Medios.tamanoDeInsercion(
                            caja.size)
                        let id = navegador.capaElementos.insertarAdjunto(
                            adjunto, tipo: .pdf, en: cursor, tamano: tamano,
                            paginaFuente: indice)
                        if let texto = pagina.string, let id {
                            indexarTexto(texto, elementoId: id,
                                         tipo: caja.width > caja.height * 1.2
                                            ? .diapositivaProfesor : .libro,
                                         titulo: "\(url.lastPathComponent) · página \(indice + 1)",
                                         hash: adjunto.hash, pagina: indice)
                        }
                        cursor.y += tamano.height + 48
                    }
                } else {
                    let id = navegador.capaElementos.insertarAdjunto(
                        adjunto, tipo: tipo, en: cursor,
                        tamano: CGSize(width: 460, height: 320))
                    if tipo == .imagen, let id,
                       let imagen = UIImage(contentsOfFile: adjunto.url.path) {
                        indexarImagen(imagen, elementoId: id, titulo: url.lastPathComponent)
                    } else if let id, ["txt", "md", "csv", "tex"].contains(
                        url.pathExtension.lowercased()),
                              let texto = try? String(contentsOf: adjunto.url,
                                                      encoding: .utf8) {
                        indexarTexto(texto, elementoId: id, tipo: .libro,
                                     titulo: url.lastPathComponent, hash: adjunto.hash)
                    }
                    cursor.y += 352
                }
            } catch {
                avisar("No se pudo importar \(url.lastPathComponent): \(error.localizedDescription)")
            }
        }
    }
}

extension ControladorLienzo: VNDocumentCameraViewControllerDelegate {
    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFinishWith scan: VNDocumentCameraScan) {
        controller.dismiss(animated: true)
        for indice in 0..<scan.pageCount {
            let imagen = scan.imageOfPage(at: indice)
            ProcesadorPizarraLocal.procesar(imagen) { [weak self] resultado in
                DispatchQueue.main.async {
                    guard let self, case .success(let pizarra) = resultado,
                          let original = try? RepositorioAdjuntos.compartido.importar(
                            imagen: imagen, nombre: "Pizarra-\(indice + 1).png") else { return }
                    self.insertarResultadoPizarra(pizarra, original: original, tipo: .pizarra)
                }
            }
        }
    }

    func documentCameraViewControllerDidCancel(_ controller: VNDocumentCameraViewController) {
        controller.dismiss(animated: true)
    }

    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFailWithError error: Error) {
        controller.dismiss(animated: true)
        avisar(error.localizedDescription)
    }
}

extension ControladorLienzo: UIColorPickerViewControllerDelegate {
    func colorPickerViewControllerDidSelectColor(_ viewController: UIColorPickerViewController) {
        lienzo.colorPluma = viewController.selectedColor.argb
        actualizarBotonColorSimple()
    }
}
