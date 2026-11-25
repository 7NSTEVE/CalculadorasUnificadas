import UIKit

enum ErrorNavegacionProfundaIPad: LocalizedError {
    case destinoSinDocumento
    case destinoNoLocalizado
    case origenNoLocalizado
    case cuadernoAbiertoEnOtraVentana
    case pestanaNoDisponible

    var errorDescription: String? {
        switch self {
        case .destinoSinDocumento:
            return "El enlace no apunta a un documento de CuadernU."
        case .destinoNoLocalizado:
            return "El documento de destino ya no aparece en la biblioteca local."
        case .origenNoLocalizado:
            return "No se pudo resolver el cuaderno desde el que se abrió el enlace."
        case .cuadernoAbiertoEnOtraVentana:
            return "El cuaderno de destino ya está abierto en otra ventana. Ábralo desde esa ventana para evitar dos escritores simultáneos."
        case .pestanaNoDisponible:
            return "La pestaña de destino no pudo reabrirse sin alterar su archivo original."
        }
    }
}

enum ClaveDestinoProfundoIPad: Equatable {
    case trazo(String)
    case texto(String)
    case medio(String)
    case region(CGRect)
    case legado(String)

    static func leer(_ valor: String) -> ClaveDestinoProfundoIPad {
        if valor.hasPrefix("t:") { return .trazo(String(valor.dropFirst(2))) }
        if valor.hasPrefix("x:") { return .texto(String(valor.dropFirst(2))) }
        if valor.hasPrefix("m:") { return .medio(String(valor.dropFirst(2))) }
        if let region = CodecAnclaRegionIPad.decodificar(valor) {
            return .region(region)
        }
        return .legado(valor)
    }
}

enum ResultadoPreparacionPestanaIPad: Equatable {
    case yaAbierta
    case reabierta
    case noEncontrada
}

extension RepositorioPestanasIPad {
    /// Promueve una pestaña cerrada al conjunto abierto sin tocar su blob de
    /// contenido. La escritura del manifiesto sigue pasando por la validación
    /// y el write atómico del repositorio canónico.
    @discardableResult
    func prepararPestanaParaNavegacion(
        id: UUID,
        panel: PanelPestanasIPad,
        contexto: String
    ) throws -> ResultadoPreparacionPestanaIPad {
        guard case .cargado(var estado) = leer(panel, contexto: contexto) else {
            return .noEncontrada
        }
        if estado.abiertas.contains(where: { $0.id == id }) {
            estado.activa = id
            try guardar(estado, panel: panel, contexto: contexto)
            return .yaAbierta
        }
        guard let indice = estado.cerradas.firstIndex(where: { $0.id == id }) else {
            return .noEncontrada
        }
        let registro = estado.cerradas.remove(at: indice)
        estado.abiertas.append(registro)
        estado.activa = id
        try guardar(estado, panel: panel, contexto: contexto)
        return .reabierta
    }
}

extension ControladorPestanasDocumentoIPad {
    /// Se obtiene por jerarquía UIKit en vez de exponer el arreglo privado de
    /// pestañas. Sólo el UINavigationController visible conserva su canvas GPU.
    func lienzoVisibleParaNavegacionProfunda() -> ControladorLienzo? {
        for hijo in children {
            guard let navegacion = hijo as? UINavigationController,
                  navegacion.viewIfLoaded?.isHidden != true else { continue }
            if let lienzo = navegacion.viewControllers.first as? ControladorLienzo {
                return lienzo
            }
        }
        return nil
    }
}

private extension UIView {
    func primeraVista<T: UIView>(de tipo: T.Type) -> T? {
        if let valor = self as? T { return valor }
        for hija in subviews {
            if let valor = hija.primeraVista(de: tipo) { return valor }
        }
        return nil
    }
}

extension ControladorLienzo {
    /// Navega sin broadcast global: únicamente el controlador de la pestaña ya
    /// activada recibe el destino, evitando alertas de las pestañas ocultas.
    @discardableResult
    func abrirDestinoProfundo(_ destino: DestinoEnlaceTinta) -> Bool {
        loadViewIfNeeded()
        guard let navegador = view.primeraVista(de: VistaLienzoNavegable.self),
              navegador.lienzo.sesion.documentoPersistenteId == destino.documentoId
        else { return false }

        if let elemento = destino.elementoId,
           let marco = marcoDestinoProfundo(elemento, navegador: navegador) {
            navegador.mostrarRegion(
                marco.insetBy(dx: -80, dy: -80), animado: true)
            UIAccessibility.post(
                notification: .screenChanged,
                argument: "Destino: \(destino.titulo ?? "referencia")")
            return true
        }

        if let x = destino.x, let y = destino.y {
            let lado: CGFloat
            if let zoom = destino.zoom, zoom.isFinite, zoom > 0 {
                lado = min(max(420 / zoom, 120), 1_200)
            } else {
                lado = 420
            }
            navegador.mostrarRegion(
                CGRect(x: x - lado / 2, y: y - lado / 2,
                       width: lado, height: lado),
                animado: true)
            return true
        }
        return destino.elementoId == nil
    }

    private func marcoDestinoProfundo(
        _ elemento: String,
        navegador: VistaLienzoNavegable
    ) -> CGRect? {
        let sesion = navegador.lienzo.sesion
        switch ClaveDestinoProfundoIPad.leer(elemento) {
        case .trazo(let id):
            for indice in 0..<sesion.cantidadTrazos where sesion.idTrazo(indice) == id {
                return sesion.marcoTrazo(indice)
            }
        case .texto(let id):
            return sesion.textos.first(where: { $0.id == id })?.marco
        case .medio(let id):
            return sesion.medios.first(where: { $0.id == id })?.marco
        case .region(let region):
            return region
        case .legado(let id):
            if let marco = navegador.capaElementos.marcoElemento(id: id) {
                return marco
            }
            if let texto = sesion.textos.first(where: { $0.id == id }) {
                return texto.marco
            }
            if let medio = sesion.medios.first(where: { $0.id == id }) {
                return medio.marco
            }
            for indice in 0..<sesion.cantidadTrazos where sesion.idTrazo(indice) == id {
                return sesion.marcoTrazo(indice)
            }
        }
        return nil
    }
}

enum RouterNavegacionDocumentosIPad {
    @discardableResult
    static func abrir(
        _ fuente: ReferenciaFuente,
        desdeDocumento documentoOrigen: UInt64,
        presentador: UIViewController
    ) -> Bool {
        do {
            guard let documentoDestino = fuente.documentoId else {
                throw ErrorNavegacionProfundaIPad.destinoSinDocumento
            }
            guard let ubicacionDestino = LocalizadorDocumentoIPad.localizar(
                documentoId: documentoDestino) else {
                throw ErrorNavegacionProfundaIPad.destinoNoLocalizado
            }
            guard let ubicacionOrigen = LocalizadorDocumentoIPad.localizar(
                documentoId: documentoOrigen) else {
                throw ErrorNavegacionProfundaIPad.origenNoLocalizado
            }
            guard let raiz = raizContenedora(desde: presentador) else {
                throw ErrorNavegacionProfundaIPad.origenNoLocalizado
            }

            let destino = DestinoEnlaceTinta(
                documentoId: documentoDestino,
                elementoId: fuente.elementoId,
                bloqueId: nil,
                x: nil, y: nil, zoom: nil,
                segundoAudio: fuente.segundoAudio,
                titulo: fuente.titulo)

            if ubicacionOrigen.contextoCuaderno == ubicacionDestino.contextoCuaderno {
                try abrirEnMismoCuaderno(
                    ubicacionDestino,
                    destino: destino,
                    raiz: raiz,
                    presentador: presentador)
            } else {
                try abrirOtroCuaderno(
                    ubicacionDestino,
                    destino: destino,
                    raizOrigen: raiz,
                    presentador: presentador)
            }
            return true
        } catch {
            avisar(error, desde: presentador)
            return false
        }
    }

    private static func abrirEnMismoCuaderno(
        _ ubicacion: UbicacionDocumentoIPad,
        destino: DestinoEnlaceTinta,
        raiz: ControladorRaizIPad,
        presentador: UIViewController
    ) throws {
        let columna = columnaPara(ubicacion.panel)
        if let panel = raiz.viewController(for: columna)
            as? ControladorPestanasDocumentoIPad,
           panel.activarPestana(id: ubicacion.pestanaId) {
            mostrar(columna: columna, en: raiz)
            cerrarPresentador(presentador) {
                _ = panel.lienzoVisibleParaNavegacionProfunda()?
                    .abrirDestinoProfundo(destino)
            }
            return
        }

        try guardarPaneles(de: raiz)
        let resultado = try RepositorioPestanasIPad.compartido
            .prepararPestanaParaNavegacion(
                id: ubicacion.pestanaId,
                panel: ubicacion.panel,
                contexto: ubicacion.contextoCuaderno)
        guard resultado != .noEncontrada else {
            throw ErrorNavegacionProfundaIPad.pestanaNoDisponible
        }

        let nuevo = ControladorPestanasDocumentoIPad(
            panel: ubicacion.panel,
            viewport: ubicacion.panel == .principal ? .principal : .secundario,
            contextoEscena: ubicacion.contextoCuaderno)
        configurarCierre(nuevo, panel: ubicacion.panel, raiz: raiz)
        raiz.setViewController(nuevo, for: columna)
        nuevo.loadViewIfNeeded()
        guard nuevo.activarPestana(id: ubicacion.pestanaId) else {
            throw ErrorNavegacionProfundaIPad.pestanaNoDisponible
        }
        mostrar(columna: columna, en: raiz)
        cerrarPresentador(presentador) {
            _ = nuevo.lienzoVisibleParaNavegacionProfunda()?
                .abrirDestinoProfundo(destino)
        }
    }

    private static func abrirOtroCuaderno(
        _ ubicacion: UbicacionDocumentoIPad,
        destino: DestinoEnlaceTinta,
        raizOrigen: ControladorRaizIPad,
        presentador: UIViewController
    ) throws {
        try guardarPaneles(de: raizOrigen)
        let resultado = try RepositorioPestanasIPad.compartido
            .prepararPestanaParaNavegacion(
                id: ubicacion.pestanaId,
                panel: ubicacion.panel,
                contexto: ubicacion.contextoCuaderno)
        guard resultado != .noEncontrada else {
            throw ErrorNavegacionProfundaIPad.pestanaNoDisponible
        }
        guard let arrendamiento = CentroAperturasCuadernosIPad.compartido
            .adquirir(ubicacion.contextoCuaderno) else {
            throw ErrorNavegacionProfundaIPad.cuadernoAbiertoEnOtraVentana
        }

        let destinoRaiz = ControladorRaizIPad(
            contextoEscena: ubicacion.contextoCuaderno,
            arrendamiento: arrendamiento)
        destinoRaiz.title = ubicacion.titulo
        destinoRaiz.modalPresentationStyle = .fullScreen
        destinoRaiz.loadViewIfNeeded()
        let columna = columnaPara(ubicacion.panel)
        guard let panel = destinoRaiz.viewController(for: columna)
            as? ControladorPestanasDocumentoIPad else {
            throw ErrorNavegacionProfundaIPad.pestanaNoDisponible
        }
        panel.loadViewIfNeeded()
        guard panel.activarPestana(id: ubicacion.pestanaId) else {
            throw ErrorNavegacionProfundaIPad.pestanaNoDisponible
        }
        mostrar(columna: columna, en: destinoRaiz)

        cerrarPresentador(presentador) {
            raizOrigen.present(destinoRaiz, animated: true) {
                _ = panel.lienzoVisibleParaNavegacionProfunda()?
                    .abrirDestinoProfundo(destino)
            }
        }
    }

    private static func configurarCierre(
        _ panel: ControladorPestanasDocumentoIPad,
        panel tipo: PanelPestanasIPad,
        raiz: ControladorRaizIPad
    ) {
        if tipo == .secundario {
            panel.alCerrarDocumento = { [weak raiz] in
                guard let raiz, raiz.referenciaVisible else { return }
                raiz.alternarReferencia()
            }
        } else {
            panel.alCerrarDocumento = { [weak raiz] in
                guard let raiz else { return }
                do {
                    try guardarPaneles(de: raiz)
                    raiz.dismiss(animated: true)
                } catch {
                    avisar(error, desde: raiz)
                }
            }
        }
    }

    private static func guardarPaneles(de raiz: ControladorRaizIPad) throws {
        try (raiz.viewController(for: .primary)
            as? ControladorPestanasDocumentoIPad)?.guardarAhora()
        try (raiz.viewController(for: .secondary)
            as? ControladorPestanasDocumentoIPad)?.guardarAhora()
    }

    private static func columnaPara(
        _ panel: PanelPestanasIPad
    ) -> UISplitViewController.Column {
        panel == .principal ? .secondary : .primary
    }

    private static func mostrar(
        columna: UISplitViewController.Column,
        en raiz: ControladorRaizIPad
    ) {
        if columna == .primary {
            if !raiz.referenciaVisible { raiz.alternarReferencia() }
            raiz.show(.primary)
        } else {
            raiz.show(.secondary)
        }
    }

    private static func raizContenedora(
        desde controlador: UIViewController
    ) -> ControladorRaizIPad? {
        var pendientes: [UIViewController] = [controlador]
        var visitados = Set<ObjectIdentifier>()
        while let actual = pendientes.first {
            pendientes.removeFirst()
            let identidad = ObjectIdentifier(actual)
            guard visitados.insert(identidad).inserted else { continue }
            if let raiz = actual as? ControladorRaizIPad { return raiz }
            if let padre = actual.parent { pendientes.append(padre) }
            if let presentador = actual.presentingViewController {
                pendientes.append(presentador)
            }
        }
        return nil
    }

    private static func cerrarPresentador(
        _ presentador: UIViewController,
        completion: @escaping () -> Void
    ) {
        let modal = presentador.navigationController ?? presentador
        if modal.presentingViewController != nil {
            modal.dismiss(animated: true, completion: completion)
        } else {
            completion()
        }
    }

    private static func avisar(_ error: Error, desde presentador: UIViewController) {
        let alerta = UIAlertController(
            title: "No se pudo abrir la referencia",
            message: error.localizedDescription,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        let destino = presentador.presentedViewController ?? presentador
        destino.present(alerta, animated: true)
    }
}
