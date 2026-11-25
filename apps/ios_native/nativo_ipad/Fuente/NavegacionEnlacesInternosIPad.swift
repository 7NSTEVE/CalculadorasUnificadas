import UIKit

struct UbicacionDocumentoCuadernUIPad: Equatable, Sendable {
    let cuadernoId: String
    let tituloCuaderno: String
    let pestanaId: UUID
    let abierta: Bool
}

enum ResultadoNavegacionEnlaceIPad: Equatable {
    case abiertoEnEscenaActual
    case cambioCuadernoIniciado
    case activadaEscenaExistente
    case abiertoEnNavegador
    case documentoNoEncontrado
    case pestanaCerrada
    case destinoNoSoportado
    case cuadernoOcupado
    case shellNoDisponible
    case guardadoFallido
    case enlaceInvalido
}

/// Checkpoint explícito para las transiciones entre cuadernos. A diferencia del
/// snapshot de UIScene, esta API propaga el error al router: si un panel no puede
/// confirmar sus bytes en disco, la navegación se cancela y el editor actual
/// permanece visible y conserva su lease.
extension ControladorRaizIPad {
    func prepararCambioSeguroDeCuaderno() throws {
        try (viewController(for: .secondary)
            as? ControladorPestanasDocumentoIPad)?.guardarAhora()
        try (viewController(for: .primary)
            as? ControladorPestanasDocumentoIPad)?.guardarAhora()
    }
}

/// Índice ligero que resuelve la identidad documental durable sin depender de
/// títulos. Lee únicamente manifiestos de pestañas; no materializa tinta ni crea
/// vistas, por lo que puede usarse desde búsqueda/backlinks con un coste bajo.
final class ResolutorUbicacionDocumentoIPad {
    static let compartido = ResolutorUbicacionDocumentoIPad()

    private let repositorio: RepositorioPestanasIPad
    private let gestor: GestionBibliotecaCuadernU

    init(repositorio: RepositorioPestanasIPad = .compartido,
         gestor: GestionBibliotecaCuadernU = GestorArchivosCuadernU.compartido) {
        self.repositorio = repositorio
        self.gestor = gestor
    }

    func resolver(_ documento: DocumentoPersistenteID) -> UbicacionDocumentoCuadernUIPad? {
        resolverRegistro { $0.id == documento.rawValue }
    }

    /// Puente para el índice y los sidecars históricos que usan la clave UInt64
    /// determinista derivada del UUID. Nunca compara contra el handle C++.
    func resolver(claveCompatibilidad documento: UInt64)
        -> UbicacionDocumentoCuadernUIPad? {
        resolverRegistro {
            DocumentoPersistenteID($0.id).claveCompatibilidadUInt64 == documento
        }
    }

    private func resolverRegistro(_ coincide: (RegistroPestanaIPad) -> Bool)
        -> UbicacionDocumentoCuadernUIPad? {
        guard let cuadernos = try? gestor.listarCuadernos() else { return nil }
        for cuaderno in cuadernos {
            guard case .cargado(let estado) = repositorio.leer(
                .principal, contexto: cuaderno.id) else { continue }
            if let registro = estado.abiertas.first(where: coincide) {
                return .init(cuadernoId: cuaderno.id,
                             tituloCuaderno: cuaderno.titulo,
                             pestanaId: registro.id,
                             abierta: true)
            }
            if let registro = estado.cerradas.first(where: coincide) {
                return .init(cuadernoId: cuaderno.id,
                             tituloCuaderno: cuaderno.titulo,
                             pestanaId: registro.id,
                             abierta: false)
            }
        }
        return nil
    }
}

/// Router único para enlaces internos y resultados de búsqueda.
///
/// Reglas de seguridad:
/// - nunca identifica un documento por su título;
/// - nunca adquiere un segundo lease del mismo cuaderno;
/// - si el destino ya está abierto en otra UIScene, activa esa ventana;
/// - antes de sustituir el editor de la escena actual exige checkpoint exitoso;
/// - adquiere el lease destino antes de cerrar la vista actual;
/// - no restaura silenciosamente una pestaña cerrada por el usuario.
@MainActor
final class CoordinadorNavegacionEnlacesIPad {
    static let compartido = CoordinadorNavegacionEnlacesIPad()

    private let resolutor: ResolutorUbicacionDocumentoIPad
    private let estadoEscenas: RepositorioEstadoEscenasCuadernUIPad
    private let centroAperturas: CentroAperturasCuadernosIPad

    init(resolutor: ResolutorUbicacionDocumentoIPad = .compartido,
         estadoEscenas: RepositorioEstadoEscenasCuadernUIPad = .compartido,
         centroAperturas: CentroAperturasCuadernosIPad = .compartido) {
        self.resolutor = resolutor
        self.estadoEscenas = estadoEscenas
        self.centroAperturas = centroAperturas
    }

    @discardableResult
    func abrir(_ url: URL, desde escena: UIWindowScene) -> ResultadoNavegacionEnlaceIPad {
        if let destino = try? CodecEnlacesInternosIPad.decodificar(url) {
            return navegar(destino, desde: escena)
        }
        // Compatibilidad: el contrato de colaboración/Handoff anterior sigue
        // teniendo `tinta://abrir` con documento UInt64. Se entrega al centro
        // histórico en vez de reinterpretarlo como UUID.
        if DestinoEnlaceTinta.leer(url) != nil {
            CentroEnlacesProfundos.compartido.recibir(url)
            return .abiertoEnEscenaActual
        }
        return .enlaceInvalido
    }

    @discardableResult
    func navegar(_ destino: DestinoEnlaceIPad,
                 desde escena: UIWindowScene) -> ResultadoNavegacionEnlaceIPad {
        if destino.tipo == .web {
            guard let url = URL(string: destino.clave),
                  ["http", "https"].contains(url.scheme?.lowercased() ?? "") else {
                return .enlaceInvalido
            }
            UIApplication.shared.open(url)
            return .abiertoEnNavegador
        }

        guard destino.tipo == .documento,
              let documento = destino.documentoId,
              let ubicacion = resolutor.resolver(documento) else {
            return destino.tipo == .documento
                ? .documentoNoEncontrado : .destinoNoSoportado
        }
        guard ubicacion.abierta else { return .pestanaCerrada }

        let escenaId = escena.session.persistentIdentifier
        let contextoActual = estadoEscenas.estado(escenaId: escenaId).cuadernoId

        // El mismo cuaderno ya pertenece a esta escena: no toca leases ni shell.
        if contextoActual == ubicacion.cuadernoId,
           let editor = editor(en: escena) {
            return aplicar(destino, pestanaId: ubicacion.pestanaId, en: editor)
                ? .abiertoEnEscenaActual : .documentoNoEncontrado
        }

        // Si otra ventana ya posee el cuaderno, el resultado debe viajar a esa
        // escena en lugar de intentar abrir un segundo escritor.
        if let otra = escenaQueYaTiene(cuadernoId: ubicacion.cuadernoId,
                                      excluyendo: escena),
           let editorDestino = editor(en: otra) {
            guard aplicar(destino, pestanaId: ubicacion.pestanaId,
                          en: editorDestino) else {
                return .documentoNoEncontrado
            }
            UIApplication.shared.requestSceneSessionActivation(
                otra.session, userActivity: nil, options: nil, errorHandler: nil)
            return .activadaEscenaExistente
        }

        // Se toma el lease antes de desmontar la vista actual. Si otro escritor
        // ganó la carrera, esta escena permanece exactamente donde estaba.
        guard let arrendamiento = centroAperturas.adquirir(ubicacion.cuadernoId) else {
            return .cuadernoOcupado
        }
        guard let biblioteca = biblioteca(en: escena) else {
            _ = arrendamiento // se libera al salir del scope
            return .shellNoDisponible
        }

        let nuevo = ControladorRaizIPad(
            contextoEscena: ubicacion.cuadernoId,
            arrendamiento: arrendamiento)
        nuevo.title = ubicacion.tituloCuaderno
        nuevo.modalPresentationStyle = .fullScreen

        let presentarDestino = { [weak nuevo, weak biblioteca, weak escena, weak self] in
            guard let self, let nuevo, let biblioteca, let escena else { return }
            _ = CoordinadorEscenasCuadernUIPad.compartido.registrarApertura(
                cuadernoId: ubicacion.cuadernoId,
                escenaId: escena.session.persistentIdentifier)
            biblioteca.present(nuevo, animated: false) {
                _ = self.aplicar(destino, pestanaId: ubicacion.pestanaId, en: nuevo)
                CoordinadorEscenasCuadernUIPad.compartido.actualizarDisposicion(escena)
            }
        }

        if let actual = editor(en: escena) {
            do {
                try actual.prepararCambioSeguroDeCuaderno()
            } catch {
                // `nuevo` sale de scope junto con su arrendamiento. Ninguna
                // jerarquía se desmonta si el documento actual no se confirmó.
                return .guardadoFallido
            }
            // El contenido ya quedó confirmado; ahora sí se actualiza el snapshot
            // ligero de la UIScene antes de desmontar la jerarquía anterior.
            CoordinadorEscenasCuadernUIPad.compartido.persistir(escena)
            actual.dismiss(animated: false, completion: presentarDestino)
            return .cambioCuadernoIniciado
        }

        guard biblioteca.presentedViewController == nil else {
            return .shellNoDisponible
        }
        presentarDestino()
        return .cambioCuadernoIniciado
    }

    private func aplicar(_ destino: DestinoEnlaceIPad,
                         pestanaId: UUID,
                         en editor: ControladorRaizIPad) -> Bool {
        guard let principal = editor.viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad,
              principal.activarPestana(id: pestanaId) else { return false }

        // La activación es síncrona, pero se deja el ajuste espacial al próximo
        // ciclo para que Auto Layout termine de revelar el canvas seleccionado.
        DispatchQueue.main.async { [weak principal] in
            guard let principal,
                  let navegable = self.buscarNavegable(en: principal.view) else { return }
            if let frameId = destino.frameId {
                let documentoNucleo = navegable.lienzo.sesion.documentoPersistenteId
                if let frame = RepositorioNavegacionEspacialIPad.compartido
                    .instantanea(documentoNucleo).frames
                    .first(where: { $0.id == frameId }) {
                    navegable.mostrarRegion(frame.region, animado: true)
                    return
                }
            }
            if let pagina = destino.pagina {
                let alto = max(CGFloat(navegable.lienzo.sesion.altoPagina), 1)
                navegable.irA(yOffset: CGFloat(max(0, pagina)) * alto)
            }
        }
        return true
    }

    private func escenaQueYaTiene(cuadernoId: String,
                                  excluyendo origen: UIWindowScene) -> UIWindowScene? {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first { escena in
                escena.session.persistentIdentifier != origen.session.persistentIdentifier
                    && estadoEscenas.estado(
                        escenaId: escena.session.persistentIdentifier).cuadernoId == cuadernoId
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

    private func buscarNavegable(en vista: UIView) -> VistaLienzoNavegable? {
        if vista.isHidden || vista.alpha <= 0.01 { return nil }
        if let navegable = vista as? VistaLienzoNavegable { return navegable }
        for subvista in vista.subviews.reversed() {
            if let encontrada = buscarNavegable(en: subvista) { return encontrada }
        }
        return nil
    }
}
