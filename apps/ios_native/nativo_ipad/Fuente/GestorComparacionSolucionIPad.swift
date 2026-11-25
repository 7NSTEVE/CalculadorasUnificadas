import UIKit

/// Fachada única para mostrar/ocultar la comparación. Inicializa el observador
/// global antes de publicar el evento, evitando depender de AppPrincipal.
enum ComparacionSolucionPostIntentoIPad {
    static func mostrar(
        documentoId: UInt64,
        frameId: UUID,
        problemaId: UUID,
        intentoId: UUID,
        region: CGRect
    ) {
        _ = GestorComparacionSolucionGlobalIPad.compartido
        CanalComparacionSolucionIPad.mostrar(
            documentoId: documentoId,
            frameId: frameId,
            problemaId: problemaId,
            intentoId: intentoId,
            region: region)
    }

    static func ocultar(documentoId: UInt64) {
        _ = GestorComparacionSolucionGlobalIPad.compartido
        CanalComparacionSolucionIPad.ocultar(documentoId: documentoId)
    }
}

/// Añade el panel directamente a la UIWindow activa. Como la vista ocupa solo
/// el lateral derecho, los eventos fuera de su frame siguen llegando al lienzo:
/// el usuario puede escribir con lápiz del iPad mientras lee la solución.
final class GestorComparacionSolucionGlobalIPad {
    static let compartido = GestorComparacionSolucionGlobalIPad()

    private let panel = VistaComparacionSolucionIPad()
    private var observador: NSObjectProtocol?
    private var documentoVisible: UInt64?

    private init() {
        panel.translatesAutoresizingMaskIntoConstraints = false
        panel.alCerrar = { [weak self] in
            guard let documento = self?.documentoVisible else { return }
            ComparacionSolucionPostIntentoIPad.ocultar(documentoId: documento)
        }
        observador = NotificationCenter.default.addObserver(
            forName: CanalComparacionSolucionIPad.nombre,
            object: nil,
            queue: .main) { [weak self] notificacion in
                self?.recibir(notificacion)
            }
    }

    private func recibir(_ notificacion: Notification) {
        if let documentoVisible,
           CanalComparacionSolucionIPad.esOcultar(
            notificacion, documentoId: documentoVisible) {
            ocultarPanel()
            return
        }
        guard let evento = CanalComparacionSolucionIPad.evento(de: notificacion) else {
            return
        }
        presentar(evento)
    }

    private func presentar(_ evento: EventoComparacionSolucionIPad) {
        let navegacion = RepositorioNavegacionEspacialIPad.compartido
            .instantanea(evento.documentoId)
        guard let frame = navegacion.frames.first(where: { $0.id == evento.frameId }),
              let estudio = frame.estudio,
              estudio.problemaId == evento.problemaId else {
            return
        }

        let problema = CentroEstudioTecnicoIPad.compartido
            .instantanea().problemas.first { $0.id == evento.problemaId }
        guard case .permitido = PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                estudio: estudio,
                problema: problema,
                intentoId: evento.intentoId),
              let problema else {
            return
        }

        guard let ventana = ventanaActiva() else { return }
        if let navegador = navegadorActivo(
            en: ventana,
            documentoId: evento.documentoId,
            region: evento.region) {
            let contexto = GeometriaFocoPaginaActualIPad.contextoVisual(
                para: evento.region,
                margenRelativo: 0.08)
            navegador.mostrarRegion(contexto, animado: true)
            CanalFocoPaginaActualIPad.activarCuandoSeaVisible(
                documento: evento.documentoId,
                region: evento.region)
        }

        // Se deja terminar el viaje del viewport antes de entrar el panel para
        // que el usuario vea primero su propia hoja y después la solución.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.22) { [weak self] in
            self?.presentarPanel(evento, problema: problema, en: ventana)
        }
    }

    private func presentarPanel(
        _ evento: EventoComparacionSolucionIPad,
        problema: ProblemaBancoIPad,
        en ventana: UIWindow
    ) {
        if panel.superview !== ventana {
            panel.removeFromSuperview()
            ventana.addSubview(panel)
            NSLayoutConstraint.activate([
                panel.trailingAnchor.constraint(
                    equalTo: ventana.safeAreaLayoutGuide.trailingAnchor,
                    constant: -12),
                panel.topAnchor.constraint(
                    equalTo: ventana.safeAreaLayoutGuide.topAnchor,
                    constant: 12),
                panel.bottomAnchor.constraint(
                    equalTo: ventana.safeAreaLayoutGuide.bottomAnchor,
                    constant: -12),
                panel.widthAnchor.constraint(
                    lessThanOrEqualToConstant: 620),
                panel.widthAnchor.constraint(
                    equalTo: ventana.safeAreaLayoutGuide.widthAnchor,
                    multiplier: 0.42),
            ])
        }

        documentoVisible = evento.documentoId
        let previa = RepositorioRevisionesSolucionIPad.compartido.revision(
            documento: evento.documentoId,
            frameId: evento.frameId,
            intentoId: evento.intentoId)
        panel.mostrar(evento: evento, problema: problema, revisionPrevia: previa)
        panel.alpha = 0
        panel.transform = CGAffineTransform(translationX: 30, y: 0)
        UIView.animate(withDuration: 0.2) {
            self.panel.alpha = 1
            self.panel.transform = .identity
        }
    }

    private func navegadorActivo(
        en ventana: UIWindow,
        documentoId: UInt64,
        region: CGRect
    ) -> VistaLienzoNavegable? {
        let candidatos = descendientes(de: ventana)
            .compactMap { $0 as? VistaLienzoNavegable }
            .filter { $0.lienzo.sesion.documentoPersistenteId == documentoId }
        if let visible = candidatos.first(where: {
            $0.regionVisibleDocumento.intersects(region)
        }) {
            return visible
        }
        return candidatos.first
    }

    private func descendientes(de vista: UIView) -> [UIView] {
        var salida: [UIView] = [vista]
        for hija in vista.subviews {
            salida.append(contentsOf: descendientes(de: hija))
        }
        return salida
    }

    private func ocultarPanel() {
        documentoVisible = nil
        UIView.animate(withDuration: 0.16, animations: {
            self.panel.alpha = 0
            self.panel.transform = CGAffineTransform(translationX: 24, y: 0)
        }, completion: { _ in
            self.panel.ocultar()
            self.panel.removeFromSuperview()
            self.panel.alpha = 1
            self.panel.transform = .identity
        })
    }

    private func ventanaActiva() -> UIWindow? {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .filter { $0.activationState == .foregroundActive }
            .flatMap(\.windows)
            .first(where: \.isKeyWindow)
            ?? UIApplication.shared.connectedScenes
                .compactMap { $0 as? UIWindowScene }
                .flatMap(\.windows)
                .first(where: { !$0.isHidden })
    }

    deinit {
        if let observador { NotificationCenter.default.removeObserver(observador) }
    }
}
