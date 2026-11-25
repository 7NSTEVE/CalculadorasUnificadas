import Foundation
import UIKit

enum EstadoBootstrapIntegridadIPad: Equatable {
    case noEjecutado
    case listo(recovery: Int)
    case bloqueado(conflictos: [ConflictoRecoveryDocumentoIPad])
}

struct ResultadoResolucionIntegridadIPad: Equatable {
    var diagnosticos: [URL]
    var estadoFinal: EstadoBootstrapIntegridadIPad
}

/// Ejecuta el orden de recuperación P0 antes de que la primera escena restaure
/// documentos y conserva un estado de proceso consultable por la UI.
///
/// El ciclo de vida limpia staging huérfano seguro antes de recuperar journals.
/// Si recovery encuentra bytes externos desconocidos, el bootstrap bloquea la
/// edición: nunca convierte un conflicto en un guardado aparentemente válido.
final class CoordinadorBootstrapIntegridadIPad {
    static let compartido: CoordinadorBootstrapIntegridadIPad = {
        let ciclo = CoordinadorCicloVidaIntegridadIPad()
        let diagnosticos = GestorDiagnosticosPersistenciaIPad()
        return CoordinadorBootstrapIntegridadIPad(
            ejecutarGate: { ciclo.prepararArranque().recovery },
            resolverConservandoActuales: { id, destino in
                _ = try diagnosticos.conservarActualesYResolver(
                    transaccionId: id,
                    diagnosticoDestino: destino)
            },
            destinoDiagnostico: { diagnosticos.destinoDiagnosticoAutomatico(
                transaccionId: $0)
            })
    }()

    private let ejecutarGateImpl: () -> EstadoGateIntegridadIPad
    private let resolverImpl: (UUID, URL) throws -> Void
    private let destinoImpl: (UUID) -> URL
    private let archivos: FileManager
    private let bloqueo = NSLock()
    private var estadoInterno: EstadoBootstrapIntegridadIPad = .noEjecutado

    init(
        ejecutarGate: @escaping () -> EstadoGateIntegridadIPad,
        resolverConservandoActuales: @escaping (UUID, URL) throws -> Void,
        destinoDiagnostico: @escaping (UUID) -> URL,
        archivos: FileManager = .default
    ) {
        self.ejecutarGateImpl = ejecutarGate
        self.resolverImpl = resolverConservandoActuales
        self.destinoImpl = destinoDiagnostico
        self.archivos = archivos
    }

    var estado: EstadoBootstrapIntegridadIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estadoInterno
    }

    @discardableResult
    func ejecutar() -> EstadoBootstrapIntegridadIPad {
        let nuevo = Self.mapear(ejecutarGateImpl())
        bloqueo.lock()
        estadoInterno = nuevo
        bloqueo.unlock()
        return nuevo
    }

    /// Resuelve únicamente los conflictos visibles al comenzar la operación.
    /// Cada journal se exporta a Documentos antes de retirarse. Si aparece un
    /// conflicto nuevo, la reevaluación final conserva el bloqueo de la UI.
    func resolverTodosConservandoActuales() throws
        -> ResultadoResolucionIntegridadIPad {
        let conflictos: [ConflictoRecoveryDocumentoIPad]
        switch estado {
        case .bloqueado(let actuales): conflictos = actuales
        default:
            return .init(diagnosticos: [], estadoFinal: ejecutar())
        }

        var salidas: [URL] = []
        do {
            for conflicto in conflictos {
                let destino = destinoDisponible(para: conflicto.id)
                try resolverImpl(conflicto.id, destino)
                salidas.append(destino)
            }
        } catch {
            _ = ejecutar()
            throw error
        }

        return .init(diagnosticos: salidas, estadoFinal: ejecutar())
    }

    private func destinoDisponible(para id: UUID) -> URL {
        let preferido = destinoImpl(id).standardizedFileURL
        guard archivos.fileExists(atPath: preferido.path) else {
            return preferido
        }
        let padre = preferido.deletingLastPathComponent()
        let nombre = preferido.lastPathComponent
        return padre.appendingPathComponent(
            "\(nombre)-\(UUID().uuidString.lowercased())",
            isDirectory: true)
    }

    private static func mapear(_ estado: EstadoGateIntegridadIPad)
        -> EstadoBootstrapIntegridadIPad {
        switch estado {
        case .limpio:
            return .listo(recovery: 0)
        case .recuperado(let transacciones):
            return .listo(recovery: transacciones)
        case .bloqueado(let conflictos):
            return .bloqueado(conflictos: conflictos)
        }
    }
}

/// Presentación full-screen deliberadamente no descartable. Dejar el editor
/// utilizable cuando hay un journal con bytes externos desconocidos derrotaría
/// el gate: un autosave legacy podría sobrescribir la evidencia protegida.
@MainActor
final class ControladorBloqueoIntegridadIPad: UIViewController {
    private let coordinador: CoordinadorBootstrapIntegridadIPad
    private let tituloEstado = UILabel()
    private let detalle = UILabel()
    private let progreso = UIActivityIndicatorView(style: .medium)
    private let resolver = UIButton(type: .system)

    init(coordinador: CoordinadorBootstrapIntegridadIPad) {
        self.coordinador = coordinador
        super.init(nibName: nil, bundle: nil)
        modalPresentationStyle = .fullScreen
        isModalInPresentation = true
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) no implementado")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        view.accessibilityIdentifier = "persistencia.integridad.bloqueo"

        let icono = UIImageView(image: UIImage(
            systemName: "externaldrive.badge.exclamationmark"))
        icono.preferredSymbolConfiguration = UIImage.SymbolConfiguration(pointSize: 52)
        icono.tintColor = .systemOrange
        icono.contentMode = .scaleAspectFit
        icono.setContentHuggingPriority(.required, for: .vertical)

        tituloEstado.font = .preferredFont(forTextStyle: .title2)
        tituloEstado.adjustsFontForContentSizeCategory = true
        tituloEstado.textAlignment = .center
        tituloEstado.numberOfLines = 0

        detalle.font = .preferredFont(forTextStyle: .body)
        detalle.adjustsFontForContentSizeCategory = true
        detalle.textColor = .secondaryLabel
        detalle.textAlignment = .center
        detalle.numberOfLines = 0

        var configuracion = UIButton.Configuration.filled()
        configuracion.title = "Proteger evidencia y continuar"
        configuracion.subtitle = "Exporta diagnóstico y conserva los archivos actuales"
        configuracion.image = UIImage(systemName: "lock.shield")
        configuracion.imagePadding = 8
        resolver.configuration = configuracion
        resolver.accessibilityIdentifier = "persistencia.integridad.resolver"
        resolver.addTarget(self, action: #selector(resolverConflictos),
                           for: .touchUpInside)

        let pila = UIStackView(arrangedSubviews: [
            icono, tituloEstado, detalle, progreso, resolver
        ])
        pila.axis = .vertical
        pila.alignment = .center
        pila.spacing = 18
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            pila.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            pila.leadingAnchor.constraint(
                greaterThanOrEqualTo: view.safeAreaLayoutGuide.leadingAnchor,
                constant: 32),
            pila.trailingAnchor.constraint(
                lessThanOrEqualTo: view.safeAreaLayoutGuide.trailingAnchor,
                constant: -32),
            pila.widthAnchor.constraint(lessThanOrEqualToConstant: 640),
            resolver.heightAnchor.constraint(greaterThanOrEqualToConstant: 50),
        ])
        refrescar()
    }

    private func refrescar(mensaje: String? = nil) {
        switch coordinador.estado {
        case .bloqueado(let conflictos):
            let rutas = conflictos.reduce(0) { $0 + $1.rutas.count }
            tituloEstado.text = conflictos.count == 1
                ? "Cuaderno protegido por recovery"
                : "\(conflictos.count) cuadernos protegidos por recovery"
            detalle.text = mensaje ??
                "CuadernU detectó un guardado interrumpido con contenido externo que no puede sobrescribir con seguridad. Se bloqueará la edición hasta preservar la evidencia. Conflictos: \(conflictos.count) · archivos implicados: \(rutas)."
            resolver.isEnabled = true
        default:
            tituloEstado.text = "Integridad restaurada"
            detalle.text = mensaje ?? "Los documentos ya pueden abrirse con seguridad."
            resolver.isEnabled = false
        }
    }

    @objc private func resolverConflictos() {
        resolver.isEnabled = false
        progreso.startAnimating()
        detalle.text = "Exportando evidencia y verificando el estado actual…"

        let coordinador = self.coordinador
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            let resultado = Result {
                try coordinador.resolverTodosConservandoActuales()
            }
            DispatchQueue.main.async {
                guard let self else { return }
                self.progreso.stopAnimating()
                switch resultado {
                case .success(let resuelto):
                    if case .bloqueado = resuelto.estadoFinal {
                        self.refrescar(mensaje:
                            "Se conservaron los conflictos resueltos, pero apareció o permanece otro conflicto. La edición continúa bloqueada.")
                    } else {
                        let cantidad = resuelto.diagnosticos.count
                        self.refrescar(mensaje:
                            "Recovery terminó sin sobrescribir contenido desconocido. Se guardaron \(cantidad) paquete(s) de diagnóstico en Documentos/RecuperacionConflictos.")
                        self.dismiss(animated: true)
                    }
                case .failure(let error):
                    self.refrescar(mensaje:
                        "No se retiró la protección porque no se pudo preservar toda la evidencia: \(error.localizedDescription)")
                }
            }
        }
    }
}

@MainActor
final class PresentadorIntegridadArranqueIPad {
    static let compartido = PresentadorIntegridadArranqueIPad()

    private var observador: NSObjectProtocol?
    private weak var presentado: ControladorBloqueoIntegridadIPad?

    func instalar(coordinador: CoordinadorBootstrapIntegridadIPad = .compartido) {
        guard observador == nil else {
            presentarEnEscenasActivas(coordinador: coordinador)
            return
        }
        observador = NotificationCenter.default.addObserver(
            forName: UIScene.didActivateNotification,
            object: nil,
            queue: .main) { [weak self] notificacion in
                guard let escena = notificacion.object as? UIWindowScene else { return }
                Task { @MainActor in
                    self?.presentarSiNecesario(
                        en: escena, coordinador: coordinador)
                }
            }
        presentarEnEscenasActivas(coordinador: coordinador)
    }

    private func presentarEnEscenasActivas(
        coordinador: CoordinadorBootstrapIntegridadIPad
    ) {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState == .foregroundActive {
            presentarSiNecesario(en: escena, coordinador: coordinador)
        }
    }

    private func presentarSiNecesario(
        en escena: UIWindowScene,
        coordinador: CoordinadorBootstrapIntegridadIPad
    ) {
        guard case .bloqueado = coordinador.estado,
              presentado == nil,
              let ventana = escena.windows.first(where: \.isKeyWindow)
                ?? escena.windows.first,
              let raiz = ventana.rootViewController else { return }
        let superior = Self.superior(raiz)
        if let existente = superior as? ControladorBloqueoIntegridadIPad {
            presentado = existente
            return
        }
        let controlador = ControladorBloqueoIntegridadIPad(
            coordinador: coordinador)
        presentado = controlador
        superior.present(controlador, animated: false)
    }

    private static func superior(_ controlador: UIViewController)
        -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return superior(presentado)
        }
        if let navegacion = controlador as? UINavigationController,
           let visible = navegacion.visibleViewController {
            return superior(visible)
        }
        if let pestanas = controlador as? UITabBarController,
           let seleccionado = pestanas.selectedViewController {
            return superior(seleccionado)
        }
        return controlador
    }
}

extension AppDelegate {
    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        guard !EntornoEjecucionIPad.esHostPruebasUnitarias() else { return true }

        let coordinador = CoordinadorBootstrapIntegridadIPad.compartido
        _ = coordinador.ejecutar()
        PresentadorIntegridadArranqueIPad.compartido.instalar(
            coordinador: coordinador)
        CoordinadorRenderTintaVirtualizadoIPad.compartido.instalar()
        CoordinadorTelemetriaPencilFisicaIPad.compartido.instalar()
        CoordinadorOCRManuscritoIPad.compartido.instalar()
        CoordinadorSincronizacionComparativaPencilIPad.compartido.instalar()
        CoordinadorAccesoQAGoodnotesIPad.compartido.instalar()
        CoordinadorRouterEnlacesProfundosGlobalIPad.compartido.instalar()
        CoordinadorAccesoRecoveryAudioIPad.compartido.instalar()
        return true
    }
}
