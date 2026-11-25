import PhotosUI
import UIKit

@MainActor
final class CentroAccionesToolbarDocumentoIPad: NSObject,
                                                PHPickerViewControllerDelegate {
    static let compartido = CentroAccionesToolbarDocumentoIPad()

    private weak var presentadorFoto: UIViewController?
    private weak var navegadorFoto: VistaLienzoNavegable?

    func rehacer() {
        guard let contexto = contextoPrincipalVisible(),
              let navegador = contexto.navegador else { return }
        navegador.lienzo.rehacer()
        navegador.lienzo.alCambiarContenidoSeleccion?()
        UIAccessibility.post(
            notification: .announcement,
            argument: "Acción rehecha")
    }

    func abrirBusquedaUniversal() {
        guard let contexto = contextoPrincipalVisible(),
              let presentador = contexto.presentador,
              let navegador = contexto.navegador else { return }
        let panel = PanelBusquedaUniversalIPad()
        panel.alAbrirResultado = { [weak self, weak panel, weak navegador] resultado in
            guard let self, let navegador else { return }
            self.abrir(resultado, navegador: navegador, panel: panel)
        }
        let navegacion = UINavigationController(rootViewController: panel)
        navegacion.modalPresentationStyle = .formSheet
        presentador.present(navegacion, animated: true)
    }

    func insertarTexto() {
        guard let contexto = contextoPrincipalVisible() else { return }
        let alerta = UIAlertController(
            title: "Insertar texto",
            message: "Se añadirá como objeto editable en el centro de la región visible.",
            preferredStyle: .alert)
        alerta.addTextField {
            $0.placeholder = "Texto"
            $0.autocapitalizationType = .sentences
            $0.clearButtonMode = .whileEditing
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(
            title: "Insertar",
            style: .default) { [weak self, weak navegador = contexto.navegador] _ in
                guard let self, let navegador,
                      let texto = alerta.textFields?.first?.text?
                        .trimmingCharacters(in: .whitespacesAndNewlines),
                      !texto.isEmpty else { return }
                let punto = navegador.centroVisibleDocumento
                guard navegador.capaElementos.insertarTexto(texto, en: punto) != nil
                else {
                    if let presentador = contexto.presentador {
                        self.avisar("No se pudo insertar el texto.", desde: presentador)
                    }
                    return
                }
                navegador.capaElementos.recargar()
                navegador.lienzo.alCambiarContenidoSeleccion?()
                UIAccessibility.post(
                    notification: .announcement,
                    argument: "Texto insertado")
            })
        if let presentador = contexto.presentador {
            presentador.present(alerta, animated: true)
        }
    }

    func insertarFoto() {
        guard let contexto = contextoPrincipalVisible(),
              let presentador = contexto.presentador else { return }
        var configuracion = PHPickerConfiguration(photoLibrary: .shared())
        configuracion.filter = .images
        configuracion.selectionLimit = 1
        configuracion.preferredAssetRepresentationMode = .current
        let picker = PHPickerViewController(configuration: configuracion)
        picker.delegate = self
        picker.modalPresentationStyle = .formSheet
        presentadorFoto = presentador
        navegadorFoto = contexto.navegador
        presentador.present(picker, animated: true)
    }

    func picker(_ picker: PHPickerViewController,
                didFinishPicking results: [PHPickerResult]) {
        picker.dismiss(animated: true)
        guard let resultado = results.first,
              resultado.itemProvider.canLoadObject(ofClass: UIImage.self) else {
            limpiarFoto()
            return
        }
        let proveedor = resultado.itemProvider
        proveedor.loadObject(ofClass: UIImage.self) { [weak self] objeto, error in
            guard let self else { return }
            guard let imagen = objeto as? UIImage, error == nil else {
                DispatchQueue.main.async {
                    if let presentador = self.presentadorFoto {
                        self.avisar(
                            error?.localizedDescription
                                ?? "La imagen seleccionada no pudo cargarse.",
                            desde: presentador)
                    }
                    self.limpiarFoto()
                }
                return
            }
            do {
                let adjunto = try RepositorioAdjuntos.compartido.importar(
                    imagen: imagen,
                    nombre: "Foto \(ISO8601DateFormatter().string(from: Date())).png")
                DispatchQueue.main.async {
                    guard let navegador = self.navegadorFoto else {
                        self.limpiarFoto()
                        return
                    }
                    let punto = navegador.centroVisibleDocumento
                    guard navegador.capaElementos.insertarAdjunto(
                        adjunto,
                        tipo: .imagen,
                        en: punto,
                        tamano: Medios.tamanoDeInsercion(imagen.size)) != nil else {
                        if let presentador = self.presentadorFoto {
                            self.avisar("No se pudo insertar la foto en el lienzo.",
                                       desde: presentador)
                        }
                        self.limpiarFoto()
                        return
                    }
                    navegador.capaElementos.recargar()
                    navegador.lienzo.alCambiarContenidoSeleccion?()
                    UIAccessibility.post(
                        notification: .announcement,
                        argument: "Foto insertada")
                    self.limpiarFoto()
                }
            } catch {
                DispatchQueue.main.async {
                    if let presentador = self.presentadorFoto {
                        self.avisar(error.localizedDescription, desde: presentador)
                    }
                    self.limpiarFoto()
                }
            }
        }
    }

    private func abrir(
        _ resultado: ResultadoBusquedaUniversalIPad,
        navegador: VistaLienzoNavegable,
        panel: UIViewController?
    ) {
        let localizacion = resultado.entrada.localizacion
        let documentoActual = navegador.lienzo.sesion.documentoPersistenteId

        if let documento = localizacion.documentoId,
           documento != documentoActual {
            abrirResultadoExterno(
                resultado,
                documentoCompatibilidad: documento,
                navegador: navegador,
                panel: panel)
            return
        }

        if let frameId = localizacion.frameId,
           let frame = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(documentoActual).frames.first(where: { $0.id == frameId }) {
            panel?.dismiss(animated: true) {
                navegador.mostrarRegion(frame.region.insetBy(dx: -36, dy: -36),
                                        animado: true)
                UIAccessibility.post(notification: .screenChanged,
                                     argument: resultado.entrada.titulo)
            }
            return
        }

        if let pagina = localizacion.pagina {
            let alto = max(CGFloat(navegador.lienzo.sesion.altoPagina), 1)
            panel?.dismiss(animated: true) {
                navegador.irA(yOffset: CGFloat(max(pagina, 0)) * alto)
                UIAccessibility.post(notification: .screenChanged,
                                     argument: "Página \(pagina + 1)")
            }
            return
        }

        if let problemaId = localizacion.problemaId {
            abrirProblema(
                problemaId,
                resultado: resultado,
                documentoId: documentoActual,
                desde: panel)
            return
        }

        if let segundo = localizacion.segundoAudio {
            let marca = String(format: "%d:%02d", Int(segundo) / 60, Int(segundo) % 60)
            let alerta = UIAlertController(
                title: resultado.entrada.titulo,
                message: "Este resultado apunta al audio en \(marca). El marcador se conserva; abra la Nota Viva asociada para reproducir desde ese instante.",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
            panel?.present(alerta, animated: true)
            return
        }

        let alerta = UIAlertController(
            title: resultado.entrada.titulo,
            message: [resultado.entrada.contenido, localizacion.contexto]
                .compactMap { $0?.isEmpty == false ? $0 : nil }
                .joined(separator: "\n\n"),
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        panel?.present(alerta, animated: true)
    }

    private func abrirResultadoExterno(
        _ resultado: ResultadoBusquedaUniversalIPad,
        documentoCompatibilidad: UInt64,
        navegador: VistaLienzoNavegable,
        panel: UIViewController?
    ) {
        guard let escena = navegador.window?.windowScene else {
            if let panel {
                avisar("La ventana activa no está disponible para abrir el resultado.",
                       desde: panel)
            }
            return
        }
        guard let ubicacion = ResolutorUbicacionDocumentoIPad.compartido
            .resolver(claveCompatibilidad: documentoCompatibilidad) else {
            if let panel {
                avisar("El documento indexado ya no existe en la biblioteca.",
                       desde: panel)
            }
            return
        }

        let destino = DestinoEnlaceIPad.documento(
            DocumentoPersistenteID(ubicacion.pestanaId),
            pagina: resultado.entrada.localizacion.pagina,
            frameId: resultado.entrada.localizacion.frameId)
        let ejecutar = { [weak self, weak escena] in
            guard let self, let escena else { return }
            let estado = CoordinadorNavegacionEnlacesIPad.compartido
                .navegar(destino, desde: escena)
            switch estado {
            case .abiertoEnEscenaActual, .cambioCuadernoIniciado,
                 .activadaEscenaExistente:
                UIAccessibility.post(notification: .screenChanged,
                                     argument: resultado.entrada.titulo)
            case .pestanaCerrada:
                self.avisarNavegacion(
                    "La nota está cerrada. CuadernU no la restaura sin permiso.",
                    escena: escena)
            case .cuadernoOcupado:
                self.avisarNavegacion(
                    "Ese cuaderno está siendo editado en otra ventana y no se pudo activar de forma segura.",
                    escena: escena)
            case .documentoNoEncontrado:
                self.avisarNavegacion(
                    "El documento ya no está disponible en ese cuaderno.",
                    escena: escena)
            case .shellNoDisponible:
                self.avisarNavegacion(
                    "La interfaz está ocupada. Termine la operación actual e inténtelo de nuevo.",
                    escena: escena)
            case .guardadoFallido:
                self.avisarNavegacion(
                    "No se cambió de cuaderno porque CuadernU no pudo confirmar el guardado del documento actual. Sus notas permanecen abiertas para evitar pérdida de datos.",
                    escena: escena)
            case .destinoNoSoportado, .enlaceInvalido, .abiertoEnNavegador:
                self.avisarNavegacion(
                    "No se pudo abrir este tipo de resultado.", escena: escena)
            }
        }
        if let panel {
            panel.dismiss(animated: true, completion: ejecutar)
        } else {
            ejecutar()
        }
    }

    private func abrirProblema(
        _ problemaId: UUID,
        resultado: ResultadoBusquedaUniversalIPad,
        documentoId: UInt64,
        desde panel: UIViewController?
    ) {
        guard CentroEstudioTecnicoIPad.compartido.instantanea().problemas
            .contains(where: { $0.id == problemaId }) else {
            if let panel {
                avisar("El problema ya no existe en el Problem Bank.", desde: panel)
            }
            return
        }
        let estudio = PanelEstudioTecnicoIPad(
            documentoId: documentoId,
            cursoId: resultado.entrada.cursoId,
            fuente: nil,
            textoSeleccionado: resultado.entrada.titulo)
        panel?.dismiss(animated: true) {
            guard let contexto = self.contextoPrincipalVisible(),
                  let presentador = contexto.presentador else { return }
            presentador.present(
                UINavigationController(rootViewController: estudio),
                animated: true)
        }
    }

    private struct ContextoVisible {
        weak var navegador: VistaLienzoNavegable?
        weak var presentador: UIViewController?
    }

    private func contextoPrincipalVisible() -> ContextoVisible? {
        guard let ventana = ventanaActiva(),
              let raiz = ventana.rootViewController,
              let controlador = buscarPrincipal(en: raiz) else { return nil }
        controlador.loadViewIfNeeded()
        guard let navegador = primeraVista(
            en: controlador.view,
            tipo: VistaLienzoNavegable.self) else { return nil }
        let presentador = superior(raiz)
        guard presentador.presentedViewController == nil
                || presentador === controlador
                || presentador.navigationController === controlador.navigationController
        else { return nil }
        return ContextoVisible(
            navegador: navegador,
            presentador: presentador)
    }

    private func buscarPrincipal(en controlador: UIViewController)
        -> ControladorLienzo? {
        if let raiz = controlador as? ControladorRaizIPad,
           let host = raiz.viewController(for: .secondary)
            as? ControladorPestanasDocumentoIPad,
           let lienzo = host.lienzoVisibleParaNavegacionProfunda() {
            return lienzo
        }
        if let presentado = controlador.presentedViewController,
           let valor = buscarPrincipal(en: presentado) { return valor }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController,
           let valor = buscarPrincipal(en: visible) { return valor }
        if let tabs = controlador as? UITabBarController,
           let seleccionado = tabs.selectedViewController,
           let valor = buscarPrincipal(en: seleccionado) { return valor }
        if let split = controlador as? UISplitViewController {
            for hijo in split.viewControllers.reversed() {
                if let valor = buscarPrincipal(en: hijo) { return valor }
            }
        }
        for hijo in controlador.children.reversed() {
            if let valor = buscarPrincipal(en: hijo) { return valor }
        }
        return controlador as? ControladorLienzo
    }

    private func primeraVista<T: UIView>(en vista: UIView, tipo: T.Type) -> T? {
        if let valor = vista as? T { return valor }
        for hija in vista.subviews {
            if let valor = primeraVista(en: hija, tipo: tipo) { return valor }
        }
        return nil
    }

    private func ventanaActiva() -> UIWindow? {
        let escenas = UIApplication.shared.connectedScenes.compactMap {
            $0 as? UIWindowScene
        }.filter { $0.activationState == .foregroundActive }
        return escenas.flatMap(\.windows).first(where: \.isKeyWindow)
            ?? escenas.flatMap(\.windows).first(where: { !$0.isHidden })
    }

    private func superior(_ controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return superior(presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return superior(visible)
        }
        if let tabs = controlador as? UITabBarController,
           let seleccionado = tabs.selectedViewController {
            return superior(seleccionado)
        }
        return controlador
    }

    private func avisarNavegacion(_ texto: String, escena: UIWindowScene) {
        guard let raiz = escena.windows.first(where: \.isKeyWindow)?.rootViewController
                ?? escena.windows.first(where: { !$0.isHidden })?.rootViewController else {
            return
        }
        let presentador = superior(raiz)
        guard presentador.presentedViewController == nil else { return }
        avisar(texto, desde: presentador)
    }

    private func avisar(_ texto: String, desde presentador: UIViewController) {
        let alerta = UIAlertController(
            title: nil,
            message: texto,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        presentador.present(alerta, animated: true)
    }

    private func limpiarFoto() {
        presentadorFoto = nil
        navegadorFoto = nil
    }
}
