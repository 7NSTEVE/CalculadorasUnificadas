import UIKit

private extension UIView {
    func primeraVistaGlobal<T: UIView>(de tipo: T.Type) -> T? {
        if let valor = self as? T { return valor }
        for hija in subviews {
            if let valor = hija.primeraVistaGlobal(de: tipo) { return valor }
        }
        return nil
    }
}

private extension UIViewController {
    func primerControladorGlobal<T: UIViewController>(de tipo: T.Type) -> T? {
        if let valor = self as? T { return valor }
        if let presentado = presentedViewController,
           let valor = presentado.primerControladorGlobal(de: tipo) {
            return valor
        }
        if let nav = self as? UINavigationController,
           let visible = nav.visibleViewController,
           let valor = visible.primerControladorGlobal(de: tipo) {
            return valor
        }
        if let tabs = self as? UITabBarController,
           let seleccionado = tabs.selectedViewController,
           let valor = seleccionado.primerControladorGlobal(de: tipo) {
            return valor
        }
        if let split = self as? UISplitViewController {
            for hijo in split.viewControllers.reversed() {
                if let valor = hijo.primerControladorGlobal(de: tipo) {
                    return valor
                }
            }
        }
        for hijo in children.reversed() {
            if let valor = hijo.primerControladorGlobal(de: tipo) {
                return valor
            }
        }
        return nil
    }

    func superiorGlobal() -> UIViewController {
        if let presentado = presentedViewController {
            return presentado.superiorGlobal()
        }
        if let nav = self as? UINavigationController,
           let visible = nav.visibleViewController {
            return visible.superiorGlobal()
        }
        if let tabs = self as? UITabBarController,
           let seleccionado = tabs.selectedViewController {
            return seleccionado.superiorGlobal()
        }
        return self
    }
}

extension ControladorLienzo {
    func documentoVisibleParaEnlaceGlobal() -> UInt64? {
        loadViewIfNeeded()
        return view.primeraVistaGlobal(de: VistaLienzoNavegable.self)?
            .lienzo.sesion.documentoPersistenteId
    }

    @discardableResult
    func abrirDestinoGlobal(_ destino: DestinoEnlaceTinta) -> Bool {
        loadViewIfNeeded()
        guard let navegador = view.primeraVistaGlobal(
            de: VistaLienzoNavegable.self),
              navegador.lienzo.sesion.documentoPersistenteId
                == destino.documentoId else { return false }

        if let bloqueId = destino.bloqueId,
           let bloque = navegador.capaBloques.bloques.first(where: {
               $0.id == bloqueId
           }) {
            navegador.mostrarRegion(
                bloque.marco.insetBy(dx: -70, dy: -70), animado: true)
            return true
        }
        return abrirDestinoProfundo(destino)
    }

    /// Entrega dirigida a la UI de colaboración ya existente. Se conserva como
    /// puente temporal hasta mover ese handler privado fuera de AppPrincipal.
    @discardableResult
    func entregarInvitacionGlobal(_ url: URL) -> Bool {
        let selector = NSSelectorFromString("recibirEnlaceProfundo:")
        guard responds(to: selector) else { return false }
        _ = perform(
            selector,
            with: Notification(name: .enlaceProfundoTinta, object: url))
        return true
    }
}

enum ErrorRouterEnlaceGlobalIPad: LocalizedError {
    case destinoNoLocalizado
    case sinVentanaActiva
    case cuadernoAbiertoEnOtraVentana
    case pestanaNoDisponible
    case noSePudoGuardarOrigen
    case colaboracionNoDisponible

    var errorDescription: String? {
        switch self {
        case .destinoNoLocalizado:
            return "El cuaderno del enlace ya no aparece en la biblioteca local."
        case .sinVentanaActiva:
            return "No hay una ventana activa en la que abrir este enlace."
        case .cuadernoAbiertoEnOtraVentana:
            return "El cuaderno del enlace ya está abierto en otra ventana. Use esa ventana para mantener un único escritor."
        case .pestanaNoDisponible:
            return "La pestaña del enlace no pudo restaurarse sin alterar su archivo original."
        case .noSePudoGuardarOrigen:
            return "El cuaderno actual no pudo completar su guardado antes de abrir el enlace."
        case .colaboracionNoDisponible:
            return "El cuaderno se abrió, pero esta vista no pudo recibir la invitación de colaboración."
        }
    }
}

/// Consumidor único para `tinta://abrir` y `tinta://colaborar`. El panel
/// `.secondary` es SIEMPRE el documento principal; `.primary` es una referencia
/// opcional y nunca debe redefinir el contexto de origen cuando ambas existen.
@MainActor
final class CoordinadorRouterEnlacesProfundosGlobalIPad {
    static let compartido = CoordinadorRouterEnlacesProfundosGlobalIPad()

    private var observador: NSObjectProtocol?
    private var observadorEscena: NSObjectProtocol?
    private var temporizador: Timer?

    func instalar() {
        if observador == nil {
            observador = NotificationCenter.default.addObserver(
                forName: .enlaceProfundoTinta,
                object: nil,
                queue: .main) { [weak self] notificacion in
                    guard let url = notificacion.object as? URL else { return }
                    Task { @MainActor in self?.recibir(url) }
                }
        }
        if observadorEscena == nil {
            observadorEscena = NotificationCenter.default.addObserver(
                forName: UIScene.didActivateNotification,
                object: nil,
                queue: .main) { [weak self] _ in
                    Task { @MainActor in self?.refrescarListenersLegados() }
                }
        }
        if temporizador == nil {
            let timer = Timer.scheduledTimer(
                withTimeInterval: 1.0,
                repeats: true) { [weak self] _ in
                    Task { @MainActor in self?.refrescarListenersLegados() }
                }
            timer.tolerance = 0.25
            temporizador = timer
        }
        refrescarListenersLegados()
    }

    func recibir(_ url: URL) {
        // Se vuelve a limpiar justo antes de resolver para acortar la ventana en
        // la que un canvas creado después del último scan conserve el listener.
        refrescarListenersLegados()
        if let invitacion = InvitacionColaboracion.leer(url) {
            recibirInvitacion(invitacion, url: url)
            return
        }
        guard let destino = DestinoEnlaceTinta.leer(url) else { return }
        do {
            try enrutar(destino: destino, alAbrir: nil)
        } catch {
            avisar(error)
        }
    }

    private func recibirInvitacion(
        _ invitacion: InvitacionColaboracion,
        url: URL
    ) {
        do {
            guard let ventana = ventanaObjetivo(),
                  let raizVentana = ventana.rootViewController else {
                throw ErrorRouterEnlaceGlobalIPad.sinVentanaActiva
            }
            if let editor = raizVentana.primerControladorGlobal(
                de: ControladorRaizIPad.self),
               let lienzo = lienzoPrincipalVisible(en: editor),
               lienzo.documentoVisibleParaEnlaceGlobal()
                    == invitacion.documentoId {
                guard lienzo.entregarInvitacionGlobal(url) else {
                    throw ErrorRouterEnlaceGlobalIPad.colaboracionNoDisponible
                }
                return
            }

            let destino = DestinoEnlaceTinta(
                documentoId: invitacion.documentoId,
                elementoId: nil,
                bloqueId: nil,
                x: nil,
                y: nil,
                zoom: nil,
                segundoAudio: nil,
                titulo: "Colaboración")
            try enrutar(destino: destino) { [weak self] lienzo in
                guard lienzo.entregarInvitacionGlobal(url) else {
                    self?.avisar(ErrorRouterEnlaceGlobalIPad
                        .colaboracionNoDisponible)
                    return
                }
            }
        } catch {
            avisar(error)
        }
    }

    private func enrutar(
        destino: DestinoEnlaceTinta,
        alAbrir: ((ControladorLienzo) -> Void)?
    ) throws {
        guard let ventana = ventanaObjetivo(),
              let raizVentana = ventana.rootViewController else {
            throw ErrorRouterEnlaceGlobalIPad.sinVentanaActiva
        }
        guard let ubicacion = LocalizadorDocumentoIPad.localizar(
            documentoId: destino.documentoId) else {
            throw ErrorRouterEnlaceGlobalIPad.destinoNoLocalizado
        }

        if let editor = raizVentana.primerControladorGlobal(
            de: ControladorRaizIPad.self),
           let documentoActual = documentoPrincipalVisible(en: editor),
           let ubicacionActual = LocalizadorDocumentoIPad.localizar(
                documentoId: documentoActual) {
            if ubicacionActual.contextoCuaderno
                == ubicacion.contextoCuaderno {
                try abrirEnEditorActual(
                    destino: destino,
                    ubicacion: ubicacion,
                    editor: editor,
                    alAbrir: alAbrir)
            } else {
                try abrirCuadernoNuevo(
                    destino: destino,
                    ubicacion: ubicacion,
                    desde: editor,
                    guardarOrigen: editor,
                    alAbrir: alAbrir)
            }
            return
        }

        try abrirCuadernoNuevo(
            destino: destino,
            ubicacion: ubicacion,
            desde: raizVentana.superiorGlobal(),
            guardarOrigen: nil,
            alAbrir: alAbrir)
    }

    private func abrirEnEditorActual(
        destino: DestinoEnlaceTinta,
        ubicacion: UbicacionDocumentoIPad,
        editor: ControladorRaizIPad,
        alAbrir: ((ControladorLienzo) -> Void)?
    ) throws {
        let columnaDestino = columna(ubicacion.panel)
        if let host = editor.viewController(for: columnaDestino)
            as? ControladorPestanasDocumentoIPad,
           host.activarPestana(id: ubicacion.pestanaId),
           let lienzo = host.lienzoVisibleParaNavegacionProfunda() {
            mostrar(columnaDestino, en: editor)
            _ = lienzo.abrirDestinoGlobal(destino)
            alAbrir?(lienzo)
            return
        }

        try guardar(editor)
        let preparado = try RepositorioPestanasIPad.compartido
            .prepararPestanaParaNavegacion(
                id: ubicacion.pestanaId,
                panel: ubicacion.panel,
                contexto: ubicacion.contextoCuaderno)
        guard preparado != .noEncontrada else {
            throw ErrorRouterEnlaceGlobalIPad.pestanaNoDisponible
        }

        let nuevo = ControladorPestanasDocumentoIPad(
            panel: ubicacion.panel,
            viewport: ubicacion.panel == .principal ? .principal : .secundario,
            contextoEscena: ubicacion.contextoCuaderno)
        configurarCierre(nuevo, tipo: ubicacion.panel, raiz: editor)
        editor.setViewController(nuevo, for: columnaDestino)
        nuevo.loadViewIfNeeded()
        guard nuevo.activarPestana(id: ubicacion.pestanaId),
              let lienzo = nuevo.lienzoVisibleParaNavegacionProfunda() else {
            throw ErrorRouterEnlaceGlobalIPad.pestanaNoDisponible
        }
        mostrar(columnaDestino, en: editor)
        _ = lienzo.abrirDestinoGlobal(destino)
        alAbrir?(lienzo)
    }

    private func abrirCuadernoNuevo(
        destino: DestinoEnlaceTinta,
        ubicacion: UbicacionDocumentoIPad,
        desde presentador: UIViewController,
        guardarOrigen: ControladorRaizIPad?,
        alAbrir: ((ControladorLienzo) -> Void)?
    ) throws {
        if let guardarOrigen { try guardar(guardarOrigen) }
        let preparado = try RepositorioPestanasIPad.compartido
            .prepararPestanaParaNavegacion(
                id: ubicacion.pestanaId,
                panel: ubicacion.panel,
                contexto: ubicacion.contextoCuaderno)
        guard preparado != .noEncontrada else {
            throw ErrorRouterEnlaceGlobalIPad.pestanaNoDisponible
        }
        guard let lease = CentroAperturasCuadernosIPad.compartido
            .adquirir(ubicacion.contextoCuaderno) else {
            throw ErrorRouterEnlaceGlobalIPad.cuadernoAbiertoEnOtraVentana
        }

        let raiz = ControladorRaizIPad(
            contextoEscena: ubicacion.contextoCuaderno,
            arrendamiento: lease)
        raiz.title = ubicacion.titulo
        raiz.modalPresentationStyle = .fullScreen
        raiz.loadViewIfNeeded()
        let columnaDestino = columna(ubicacion.panel)
        guard let host = raiz.viewController(for: columnaDestino)
            as? ControladorPestanasDocumentoIPad else {
            throw ErrorRouterEnlaceGlobalIPad.pestanaNoDisponible
        }
        host.loadViewIfNeeded()
        guard host.activarPestana(id: ubicacion.pestanaId) else {
            throw ErrorRouterEnlaceGlobalIPad.pestanaNoDisponible
        }
        mostrar(columnaDestino, en: raiz)
        presentador.present(raiz, animated: true) {
            guard let lienzo = host.lienzoVisibleParaNavegacionProfunda()
            else { return }
            _ = lienzo.abrirDestinoGlobal(destino)
            alAbrir?(lienzo)
        }
    }

    /// El documento principal vive en `.secondary`. La referencia `.primary`
    /// sólo se consulta como fallback si el principal todavía no materializó su
    /// canvas (por ejemplo durante una transición de pestaña).
    private func lienzoPrincipalVisible(en raiz: ControladorRaizIPad)
        -> ControladorLienzo? {
        for columna in [UISplitViewController.Column.secondary, .primary] {
            if let host = raiz.viewController(for: columna)
                as? ControladorPestanasDocumentoIPad,
               let lienzo = host.lienzoVisibleParaNavegacionProfunda() {
                return lienzo
            }
        }
        return nil
    }

    private func documentoPrincipalVisible(en raiz: ControladorRaizIPad)
        -> UInt64? {
        lienzoPrincipalVisible(en: raiz)?.documentoVisibleParaEnlaceGlobal()
    }

    private func guardar(_ raiz: ControladorRaizIPad) throws {
        do {
            try (raiz.viewController(for: .primary)
                as? ControladorPestanasDocumentoIPad)?.guardarAhora()
            try (raiz.viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad)?.guardarAhora()
        } catch {
            throw ErrorRouterEnlaceGlobalIPad.noSePudoGuardarOrigen
        }
    }

    private func configurarCierre(
        _ host: ControladorPestanasDocumentoIPad,
        tipo: PanelPestanasIPad,
        raiz: ControladorRaizIPad
    ) {
        if tipo == .secundario {
            host.alCerrarDocumento = { [weak raiz] in
                guard let raiz, raiz.referenciaVisible else { return }
                raiz.alternarReferencia()
            }
        } else {
            host.alCerrarDocumento = { [weak raiz] in
                guard let raiz else { return }
                do {
                    try (raiz.viewController(for: .primary)
                        as? ControladorPestanasDocumentoIPad)?.guardarAhora()
                    try (raiz.viewController(for: .secondary)
                        as? ControladorPestanasDocumentoIPad)?.guardarAhora()
                    raiz.dismiss(animated: true)
                } catch {
                    let alerta = UIAlertController(
                        title: "No se puede cerrar todavía",
                        message: error.localizedDescription,
                        preferredStyle: .alert)
                    alerta.addAction(UIAlertAction(
                        title: "Aceptar", style: .default))
                    raiz.present(alerta, animated: true)
                }
            }
        }
    }

    private func columna(_ panel: PanelPestanasIPad)
        -> UISplitViewController.Column {
        panel == .principal ? .secondary : .primary
    }

    private func mostrar(
        _ columna: UISplitViewController.Column,
        en raiz: ControladorRaizIPad
    ) {
        if columna == .primary {
            if !raiz.referenciaVisible { raiz.alternarReferencia() }
            raiz.show(.primary)
        } else {
            raiz.show(.secondary)
        }
    }

    private func refrescarListenersLegados() {
        for case let escena as UIWindowScene in UIApplication.shared.connectedScenes
        where escena.activationState != .unattached {
            for ventana in escena.windows where !ventana.isHidden,
                  let raiz = ventana.rootViewController {
                quitarListenerLegado(en: raiz)
            }
        }
    }

    private func quitarListenerLegado(en controlador: UIViewController) {
        if let lienzo = controlador as? ControladorLienzo {
            NotificationCenter.default.removeObserver(
                lienzo,
                name: .enlaceProfundoTinta,
                object: nil)
        }
        if let presentado = controlador.presentedViewController {
            quitarListenerLegado(en: presentado)
        }
        if let nav = controlador as? UINavigationController {
            nav.viewControllers.forEach(quitarListenerLegado)
        } else if let tabs = controlador as? UITabBarController {
            tabs.viewControllers?.forEach(quitarListenerLegado)
        } else if let split = controlador as? UISplitViewController {
            split.viewControllers.forEach(quitarListenerLegado)
        } else {
            controlador.children.forEach(quitarListenerLegado)
        }
    }

    private func ventanaObjetivo() -> UIWindow? {
        let escenas = UIApplication.shared.connectedScenes.compactMap {
            $0 as? UIWindowScene
        }.filter { $0.activationState == .foregroundActive }
        if let key = escenas.flatMap(\.windows).first(where: \.isKeyWindow) {
            return key
        }
        return escenas.flatMap(\.windows).first(where: { !$0.isHidden })
    }

    private func avisar(_ error: Error) {
        guard let ventana = ventanaObjetivo(),
              let raiz = ventana.rootViewController else { return }
        let superior = raiz.superiorGlobal()
        guard !(superior is UIAlertController) else { return }
        let alerta = UIAlertController(
            title: "No se pudo abrir el enlace",
            message: error.localizedDescription,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        superior.present(alerta, animated: true)
    }
}
