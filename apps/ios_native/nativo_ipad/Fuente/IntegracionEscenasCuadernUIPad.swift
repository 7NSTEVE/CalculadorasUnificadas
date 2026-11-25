import UIKit

/// Une el estado durable por UIScene con el shell UIKit sin convertir el ID de
/// escena en ID de cuaderno. Los manifiestos de pestañas siguen siendo la fuente
/// canónica de contenido/zoom/offset; este coordinador sólo recuerda qué contexto
/// documental pertenece a cada ventana y qué pestaña debe reactivarse.
final class CoordinadorEscenasCuadernUIPad {
    static let compartido = CoordinadorEscenasCuadernUIPad()

    private let repoEscenas: RepositorioEstadoEscenasCuadernUIPad
    private let repoPestanas: RepositorioPestanasIPad
    private let bloqueo = NSLock()
    private var escenasActivadas = Set<String>()
    private var escenasRestaurando = Set<String>()

    init(repoEscenas: RepositorioEstadoEscenasCuadernUIPad = .compartido,
         repoPestanas: RepositorioPestanasIPad = .compartido) {
        self.repoEscenas = repoEscenas
        self.repoPestanas = repoPestanas
    }

    @discardableResult
    func registrarApertura(cuadernoId: String, escenaId: String) -> Bool {
        repoEscenas.actualizar(escenaId: escenaId) { estado in
            if estado.cuadernoId != cuadernoId {
                estado.cerrarDocumento()
            }
            estado.cuadernoId = cuadernoId
        }
    }

    func registrarBibliotecaVisible(escenaId: String) {
        bloqueo.lock()
        let activa = escenasActivadas.contains(escenaId)
        let restaurando = escenasRestaurando.contains(escenaId)
        bloqueo.unlock()
        guard activa, !restaurando else { return }
        _ = repoEscenas.actualizar(escenaId: escenaId) { $0.cerrarDocumento() }
    }

    func activar(_ escena: UIWindowScene) {
        let id = escena.session.persistentIdentifier
        bloqueo.lock()
        escenasActivadas.insert(id)
        let puedeRestaurar = escenasRestaurando.insert(id).inserted
        bloqueo.unlock()
        guard puedeRestaurar else { return }
        defer {
            bloqueo.lock(); escenasRestaurando.remove(id); bloqueo.unlock()
        }

        if editor(en: escena) != nil {
            actualizarDisposicion(escena)
            return
        }
        guard let biblioteca = biblioteca(en: escena),
              biblioteca.presentedViewController == nil else { return }

        let guardado = repoEscenas.estado(escenaId: id)
        guard let cuadernoId = guardado.cuadernoId else { return }
        guard let info = (try? GestorArchivosCuadernU.compartido.listarCuadernos())?
            .first(where: { $0.id == cuadernoId }) else {
            _ = repoEscenas.actualizar(escenaId: id) { $0.cerrarDocumento() }
            return
        }

        guard let arrendamiento = CentroAperturasCuadernosIPad.compartido
            .adquirir(cuadernoId) else { return }

        let controlador = ControladorRaizIPad(
            contextoEscena: cuadernoId, arrendamiento: arrendamiento)
        controlador.title = info.titulo
        controlador.modalPresentationStyle = .fullScreen
        biblioteca.present(controlador, animated: false) { [weak self, weak escena] in
            if let idPestana = guardado.pestanaSeleccionadaId,
               let principal = controlador.viewController(for: .secondary)
                    as? ControladorPestanasDocumentoIPad {
                _ = principal.activarPestana(id: idPestana)
            }
            if guardado.referenciaVisible == true,
               !controlador.referenciaVisible {
                controlador.alternarReferencia()
            }
            if let escena { self?.actualizarDisposicion(escena) }
        }
    }

    func actualizarDisposicion(_ escena: UIWindowScene) {
        guard let controlador = editor(en: escena) else { return }
        let tamano = escena.coordinateSpace.bounds.size
        let politica = PoliticaDisposicionCuadernUIPad.calcular(tamano: tamano)
        let ancho = max(tamano.width, 1)
        controlador.preferredPrimaryColumnWidthFraction = min(
            max(politica.anchoPanelSugerido / ancho, 0.25), 0.45)

        if politica.permitePanelLateralFijo {
            controlador.preferredSplitBehavior = .tile
            controlador.preferredDisplayMode = controlador.referenciaVisible
                ? .oneBesideSecondary : .secondaryOnly
        } else {
            controlador.preferredSplitBehavior = .overlay
            controlador.preferredDisplayMode = controlador.referenciaVisible
                ? .oneOverSecondary : .secondaryOnly
        }
    }

    func persistir(_ escena: UIWindowScene) {
        let id = escena.session.persistentIdentifier
        bloqueo.lock(); let activa = escenasActivadas.contains(id); bloqueo.unlock()
        guard activa else { return }

        guard let controlador = editor(en: escena) else {
            _ = repoEscenas.actualizar(escenaId: id) { $0.cerrarDocumento() }
            return
        }
        guard let principal = controlador.viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad else { return }
        do {
            try principal.guardarAhora()
        } catch {
            return
        }

        guard let contexto = repoEscenas.estado(escenaId: id).cuadernoId,
              case .cargado(let pestanas) = repoPestanas.leer(
                .principal, contexto: contexto) else { return }

        _ = repoEscenas.actualizar(escenaId: id) { estado in
            estado.cuadernoId = contexto
            estado.pestanaSeleccionadaId = pestanas.activa
            estado.documentoId = pestanas.activa.map { DocumentoPersistenteID($0) }
            estado.referenciaVisible = controlador.referenciaVisible
        }
    }

    func desconectar(_ escena: UIWindowScene) {
        persistir(escena)
        let id = escena.session.persistentIdentifier
        bloqueo.lock()
        escenasActivadas.remove(id)
        escenasRestaurando.remove(id)
        bloqueo.unlock()
    }

    func descartar(_ sesiones: Set<UISceneSession>) {
        for sesion in sesiones {
            _ = repoEscenas.eliminar(escenaId: sesion.persistentIdentifier)
            bloqueo.lock()
            escenasActivadas.remove(sesion.persistentIdentifier)
            escenasRestaurando.remove(sesion.persistentIdentifier)
            bloqueo.unlock()
        }
    }

    private func editor(en escena: UIWindowScene) -> ControladorRaizIPad? {
        for ventana in escena.windows where !ventana.isHidden {
            if let encontrado = buscarEditor(en: ventana.rootViewController) {
                return encontrado
            }
        }
        return nil
    }

    private func buscarEditor(en controlador: UIViewController?) -> ControladorRaizIPad? {
        guard let controlador else { return nil }
        if let editor = controlador as? ControladorRaizIPad { return editor }
        if let presentado = controlador.presentedViewController,
           let editor = buscarEditor(en: presentado) { return editor }
        if let nav = controlador as? UINavigationController {
            for hijo in nav.viewControllers.reversed() {
                if let editor = buscarEditor(en: hijo) { return editor }
            }
        }
        for hijo in controlador.children.reversed() {
            if let editor = buscarEditor(en: hijo) { return editor }
        }
        return nil
    }

    private func biblioteca(en escena: UIWindowScene) -> BibliotecaCuadernosIPad? {
        for ventana in escena.windows where !ventana.isHidden {
            if let nav = ventana.rootViewController as? UINavigationController,
               let biblioteca = nav.viewControllers.first as? BibliotecaCuadernosIPad {
                return biblioteca
            }
            if let biblioteca = ventana.rootViewController as? BibliotecaCuadernosIPad {
                return biblioteca
            }
        }
        return nil
    }
}

extension EscenaPrincipal {
    func sceneDidBecomeActive(_ scene: UIScene) {
        guard let ventana = scene as? UIWindowScene else { return }
        PuenteEnlacesProfundosV8IPad.compartido.instalarSiHaceFalta()
        CoordinadorEscenasCuadernUIPad.compartido.activar(ventana)
    }

    func sceneWillResignActive(_ scene: UIScene) {
        guard let ventana = scene as? UIWindowScene else { return }
        CoordinadorEscenasCuadernUIPad.compartido.persistir(ventana)
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        guard let ventana = scene as? UIWindowScene else { return }
        CoordinadorEscenasCuadernUIPad.compartido.desconectar(ventana)
    }

    func windowScene(_ windowScene: UIWindowScene,
                     didUpdate previousCoordinateSpace: UICoordinateSpace,
                     interfaceOrientation previousInterfaceOrientation: UIInterfaceOrientation,
                     traitCollection previousTraitCollection: UITraitCollection) {
        CoordinadorEscenasCuadernUIPad.compartido
            .actualizarDisposicion(windowScene)
    }
}

extension AppDelegate {
    func application(_ application: UIApplication,
                     didDiscardSceneSessions sceneSessions: Set<UISceneSession>) {
        CoordinadorEscenasCuadernUIPad.compartido.descartar(sceneSessions)
    }
}
