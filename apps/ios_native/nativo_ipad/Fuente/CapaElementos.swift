import AVFoundation
import PDFKit
import QuickLookThumbnailing
import UIKit

enum DestinoFragmento: String, CaseIterable {
    case tarea = "Tarea"
    case flashcard = "Flashcard"
    case pregunta = "Pregunta"
    case duda = "Duda para el profesor"
    case nota = "Nueva nota"
}

struct ReferenciaElemento {
    var id: String
    var titulo: String
    var tipo: String
    /// Clave durable del documento para los repositorios Swift. No es el
    /// `TintaDocumentoId` efímero que usa el núcleo C durante este proceso.
    var paginaDocumento: UInt64
}

/// Objetos UIKit sobre la tinta Metal. El fondo transparente deja pasar la
/// escritura; solo los propios objetos capturan gestos.
final class CapaElementos: UIView {
    let sesion: Sesion
    var alConvertir: ((ReferenciaElemento, DestinoFragmento) -> Void)?
    var alCompartir: ((URL) -> Void)?
    var alEditarRecorte: ((ElementoMedio) -> Void)?
    var alCrearClipPDF: ((ElementoMedio) -> Void)?
    var alAbrirFuente: ((ElementoMedio) -> Void)?
    var alVistaPeek: ((ElementoMedio) -> Void)?
    var alCopiarCita: ((ElementoMedio) -> Void)?
    var alEditarFondo: ((ElementoMedio) -> Void)?
    var alCambiarFondos: (() -> Void)?
    var alEditarTextoAvanzado: ((ElementoTexto) -> Void)?
    var alCambiarTexto: ((ElementoTexto) -> Void)?
    var alEliminarTexto: ((String) -> Void)?
    var mostrarTextos = true { didSet { recargar() } }
    var mostrarMedios = true { didSet { recargar() } }
    var bloquearTextos = false { didSet { aplicarBloqueo() } }
    var bloquearMedios = false { didSet { aplicarBloqueo() } }
    var opacidadTextos: CGFloat = 1 { didSet { aplicarOpacidad() } }
    var opacidadMedios: CGFloat = 1 { didSet { aplicarOpacidad() } }
    private var idsSeleccionados = Set<String>()

    var politicaVirtualizacion = PoliticaVirtualizacionElementosIPad()
    private var viewportVirtualizacion: CGRect?
    private var regionMaterializacion: CGRect?
    private weak var scrollVirtualizacion: UIScrollView?
    private var observacionOffset: NSKeyValueObservation?
    private var observacionZoom: NSKeyValueObservation?

    var totalElementosModelo: Int { sesion.textos.count + sesion.medios.count }
    var cantidadVistasMaterializadas: Int {
        subviews.reduce(into: 0) { total, vista in
            if vista is VistaTextoElemento || vista is VistaMedioElemento { total += 1 }
        }
    }

    init(sesion: Sesion) {
        self.sesion = sesion
        super.init(frame: .zero)
        backgroundColor = .clear
        autoresizingMask = [.flexibleWidth, .flexibleHeight]
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        if window == nil {
            observacionOffset = nil
            observacionZoom = nil
            scrollVirtualizacion = nil
        } else {
            instalarObservacionViewportSiHaceFalta()
            actualizarViewportDesdeScroll()
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        instalarObservacionViewportSiHaceFalta()
        actualizarViewportDesdeScroll()
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        guard !isHidden, alpha > 0.01 else { return nil }
        for vista in subviews.reversed() {
            let local = vista.convert(point, from: self)
            if let resultado = vista.hitTest(local, with: event) { return resultado }
        }
        return nil
    }

    func actualizarRegionVisible(_ viewport: CGRect) {
        guard viewport.width > 0, viewport.height > 0,
              !viewport.isNull, !viewport.isInfinite else { return }
        viewportVirtualizacion = viewport
        let anterior = regionMaterializacion
        let plan = planVirtualizacion(
            textosExistentes: vistasTextoPorId(),
            mediosExistentes: vistasMedioPorId())
        regionMaterializacion = plan.regionMaterializacion
        if plan.virtualizacionActiva,
           anterior != plan.regionMaterializacion {
            recargar()
        } else if !plan.virtualizacionActiva,
                  anterior != nil {
            regionMaterializacion = nil
            recargar()
        }
    }

    func recargar() {
        let textosExistentes = vistasTextoPorId()
        let mediosExistentes = vistasMedioPorId()
        let plan = planVirtualizacion(
            textosExistentes: textosExistentes,
            mediosExistentes: mediosExistentes)
        regionMaterializacion = plan.regionMaterializacion
        let idsPermitidos = plan.idsMaterializados

        var conservar = Set<ObjectIdentifier>()
        var orden: [UIView] = []

        if mostrarTextos {
            for texto in sesion.textos where idsPermitidos.contains(texto.id) {
                let vista: VistaTextoElemento
                if let existente = textosExistentes[texto.id] {
                    existente.sincronizar(texto)
                    vista = existente
                } else {
                    vista = VistaTextoElemento(elemento: texto, propietario: self)
                    addSubview(vista)
                }
                conservar.insert(ObjectIdentifier(vista))
                orden.append(vista)
            }
        }

        if mostrarMedios {
            let documento = sesion.documentoPersistenteId
            for medio in sesion.medios where idsPermitidos.contains(medio.id) {
                let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
                    documento: documento, elemento: medio.id)
                guard !fondo.esFondo else { continue }
                let vista: VistaMedioElemento
                if let existente = mediosExistentes[medio.id] {
                    existente.sincronizar(medio)
                    vista = existente
                } else {
                    vista = VistaMedioElemento(elemento: medio, propietario: self)
                    addSubview(vista)
                }
                conservar.insert(ObjectIdentifier(vista))
                orden.append(vista)
            }
        }

        for vista in subviews where vista is VistaTextoElemento || vista is VistaMedioElemento {
            if !conservar.contains(ObjectIdentifier(vista)) {
                vista.removeFromSuperview()
            }
        }
        for vista in orden { bringSubviewToFront(vista) }
        aplicarBloqueo(); aplicarOpacidad(); aplicarSeleccionVisible()
    }

    func mostrarSeleccion(_ seleccion: SeleccionLienzo) {
        let textos = sesion.textos
        let medios = sesion.medios
        idsSeleccionados = Set(seleccion.textos.compactMap {
            textos.indices.contains($0) ? textos[$0].id : nil
        } + seleccion.medios.compactMap {
            medios.indices.contains($0) ? medios[$0].id : nil
        })
        if totalElementosModelo > max(politicaVirtualizacion.umbralElementos, 1) {
            recargar()
        } else {
            aplicarSeleccionVisible()
        }
    }

    private func vistasTextoPorId() -> [String: VistaTextoElemento] {
        Dictionary(uniqueKeysWithValues: subviews.compactMap { vista in
            guard let texto = vista as? VistaTextoElemento else { return nil }
            return (texto.elemento.id, texto)
        })
    }

    private func vistasMedioPorId() -> [String: VistaMedioElemento] {
        Dictionary(uniqueKeysWithValues: subviews.compactMap { vista in
            guard let medio = vista as? VistaMedioElemento else { return nil }
            return (medio.elemento.id, medio)
        })
    }

    private func planVirtualizacion(
        textosExistentes: [String: VistaTextoElemento],
        mediosExistentes: [String: VistaMedioElemento]
    ) -> PlanVirtualizacionElementosIPad {
        let viewport = viewportVirtualizacion ?? viewportInicialSeguro()
        var elementos: [DescriptorVirtualizableIPad] = []
        elementos.reserveCapacity(totalElementosModelo)

        for texto in sesion.textos {
            let existente = textosExistentes[texto.id]
            elementos.append(.init(
                id: texto.id,
                marco: texto.marco,
                anclado: idsSeleccionados.contains(texto.id)
                    || existente?.editor.isFirstResponder == true
                    || gestoActivo(en: existente)))
        }
        for medio in sesion.medios {
            elementos.append(.init(
                id: medio.id,
                marco: medio.marco,
                anclado: idsSeleccionados.contains(medio.id)
                    || gestoActivo(en: mediosExistentes[medio.id])))
        }
        return PlanificadorVirtualizacionElementosIPad.plan(
            elementos: elementos,
            viewport: viewport,
            regionAnterior: regionMaterializacion,
            politica: politicaVirtualizacion)
    }

    private func viewportInicialSeguro() -> CGRect {
        CGRect(x: 0, y: 0,
               width: max(bounds.width, 1_200),
               height: max(bounds.height, 1_800))
    }

    private func gestoActivo(en vista: UIView?) -> Bool {
        vista?.gestureRecognizers?.contains {
            $0.state == .began || $0.state == .changed
        } ?? false
    }

    private func instalarObservacionViewportSiHaceFalta() {
        guard let scroll = ancestroScroll() else { return }
        guard scrollVirtualizacion !== scroll else { return }
        observacionOffset = nil
        observacionZoom = nil
        scrollVirtualizacion = scroll
        observacionOffset = scroll.observe(\.contentOffset, options: [.new]) {
            [weak self] _, _ in
            guard let self else { return }
            DispatchQueue.main.async { [weak self] in self?.actualizarViewportDesdeScroll() }
        }
        observacionZoom = scroll.observe(\.zoomScale, options: [.new]) {
            [weak self] _, _ in
            guard let self else { return }
            DispatchQueue.main.async { [weak self] in self?.actualizarViewportDesdeScroll() }
        }
    }

    private func ancestroScroll() -> UIScrollView? {
        var candidato = superview
        while let vista = candidato {
            if let scroll = vista as? UIScrollView { return scroll }
            candidato = vista.superview
        }
        return nil
    }

    private func actualizarViewportDesdeScroll() {
        guard let scroll = scrollVirtualizacion,
              window != nil else { return }
        let visible = convert(scroll.bounds, from: scroll)
        let recortado = visible.intersection(bounds)
        actualizarRegionVisible(recortado.isNull || recortado.isEmpty ? visible : recortado)
    }

    private func aplicarSeleccionVisible() {
        for vista in subviews {
            let id: String?
            if let texto = vista as? VistaTextoElemento { id = texto.elemento.id }
            else if let medio = vista as? VistaMedioElemento { id = medio.elemento.id }
            else { id = nil }
            let seleccionada = id.map(idsSeleccionados.contains) ?? false
            vista.layer.borderWidth = seleccionada ? 3 : 1
            vista.layer.borderColor = (seleccionada ? UIColor.systemBlue
                                                    : UIColor.systemBlue.withAlphaComponent(0.42)).cgColor
        }
    }

    private func aplicarBloqueo() {
        for case let vista as VistaTextoElemento in subviews {
            vista.bloqueado = bloquearTextos
            vista.actualizarEstadoBloqueo()
        }
        for case let vista as VistaMedioElemento in subviews {
            let ajustes = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: sesion.documentoPersistenteId,
                elemento: vista.elemento.id)
            vista.bloqueado = bloquearMedios || ajustes.bloqueado || ajustes.esFondo
            vista.isUserInteractionEnabled = !ajustes.esFondo && !ajustes.oculto
            vista.layer.zPosition = ajustes.esFondo ? -100 : 0
        }
    }

    private func aplicarOpacidad() {
        for case let vista as VistaTextoElemento in subviews { vista.alpha = opacidadTextos }
        for case let vista as VistaMedioElemento in subviews {
            let ajustes = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: sesion.documentoPersistenteId,
                elemento: vista.elemento.id)
            vista.isHidden = ajustes.oculto
            vista.alpha = vista.elemento.opacidad * opacidadMedios
                * (ajustes.esFondo ? ajustes.opacidad : 1)
        }
    }

    func marcoElemento(id: String) -> CGRect? {
        if let texto = sesion.textos.first(where: { $0.id == id }) { return texto.marco }
        return sesion.medios.first(where: { $0.id == id })?.marco
    }

    func borrarObjeto(en punto: CGPoint) -> Bool {
        if !bloquearMedios {
            let medios = sesion.medios
            for indice in medios.indices.reversed()
            where medios[indice].marco.contains(punto) {
                guard medios[indice].tipo != .pdf else { return false }
                let seleccion = SeleccionLienzo(medios: [indice])
                let editable = RepositorioMetadatosSeleccionIPad.compartido
                    .filtrarDesbloqueados(seleccion, sesion: sesion)
                guard editable.medios.contains(indice) else { return false }
                guard sesion.eliminarMedio(id: medios[indice].id) else { return false }
                recargar(); return true
            }
        }
        if !bloquearTextos {
            let textos = sesion.textos
            for indice in textos.indices.reversed()
            where textos[indice].marco.contains(punto) {
                let seleccion = SeleccionLienzo(textos: [indice])
                let editable = RepositorioMetadatosSeleccionIPad.compartido
                    .filtrarDesbloqueados(seleccion, sesion: sesion)
                guard editable.textos.contains(indice) else { return false }
                guard sesion.eliminarTexto(id: textos[indice].id) else { return false }
                alEliminarTexto?(textos[indice].id)
                reindexarBacklinksTexto()
                recargar(); return true
            }
        }
        return false
    }

    @discardableResult
    func insertarTexto(_ texto: String, en punto: CGPoint, dictado: Bool = false) -> String? {
        insertarTexto(texto,
                      marco: CGRect(x: punto.x, y: punto.y,
                                    width: 340, height: 120),
                      dictado: dictado)
    }

    @discardableResult
    func insertarTexto(_ texto: String, marco: CGRect,
                       dictado: Bool = false) -> String? {
        guard let id = sesion.anadirTexto(texto, marco: marco, dictado: dictado) else {
            return nil
        }
        recargar()
        if let elemento = sesion.textos.first(where: { $0.id == id }) {
            alCambiarTexto?(elemento)
        }
        reindexarBacklinksTexto()
        return id
    }

    fileprivate func accionesPPTX(elementoId: String) -> [UIMenuElement] {
        guard let metadato = RepositorioObjetosPPTXIPad.compartido.metadato(
            objetoId: elementoId,
            documento: sesion.documentoPersistenteId) else { return [] }
        var acciones: [UIMenuElement] = [
            UIAction(
                title: "Procedencia: slide \(metadato.numeroDiapositiva)",
                image: UIImage(systemName: "rectangle.stack"),
                attributes: .disabled) { _ in },
        ]
        if let vinculo = metadato.hipervinculo {
            if let url = URL(string: vinculo.destino),
               ["http", "https", "mailto"].contains(url.scheme?.lowercased() ?? "") {
                acciones.append(UIAction(
                    title: "Abrir hipervínculo de PowerPoint",
                    image: UIImage(systemName: "link")) { _ in
                        UIApplication.shared.open(url)
                    })
            } else {
                acciones.append(UIAction(
                    title: "Copiar destino del hipervínculo",
                    image: UIImage(systemName: "doc.on.doc")) { _ in
                        UIPasteboard.general.string = vinculo.destino
                    })
            }
        }
        return acciones
    }

    @discardableResult
    func insertarAdjunto(_ adjunto: AdjuntoImportado, tipo: TipoMedio,
                         en punto: CGPoint, tamano: CGSize? = nil,
                         paginaFuente: Int = 0, segundoInicio: Double = 0) -> String? {
        let propuesto = tamano ?? CGSize(width: 420, height: 300)
        let marco = CGRect(origin: punto, size: propuesto)
        guard let id = sesion.anadirMedio(hash: adjunto.hash,
                                          nombre: adjunto.nombreOriginal,
                                          tipo: tipo, marco: marco) else { return nil }
        _ = sesion.configurarFuenteMedio(id: id, pagina: paginaFuente,
                                         segundo: segundoInicio)
        if tipo == .imagen {
            _ = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: sesion.documentoPersistenteId, id: id)
        }
        recargar()
        return id
    }

    fileprivate func persistir(_ vista: VistaTextoElemento) {
        var elemento = vista.elemento
        elemento.texto = vista.editor.text
        elemento.marco = vista.marcoDocumento
        elemento.rotacion = vista.rotacion
        _ = sesion.actualizarTexto(elemento)
        vista.elemento = elemento
        alCambiarTexto?(elemento)
    }

    fileprivate func persistir(_ vista: VistaMedioElemento) {
        var elemento = vista.elemento
        elemento.marco = vista.marcoDocumento
        elemento.rotacion = vista.rotacion
        _ = sesion.actualizarMedio(elemento)
        vista.elemento = elemento
    }

    fileprivate func eliminar(_ vista: VistaElementoBase) {
        var cambioTexto = false
        switch vista {
        case let texto as VistaTextoElemento:
            _ = sesion.eliminarTexto(id: texto.elemento.id)
            alEliminarTexto?(texto.elemento.id)
            cambioTexto = true
        case let medio as VistaMedioElemento:
            _ = sesion.eliminarMedio(id: medio.elemento.id)
        default: break
        }
        vista.removeFromSuperview()
        if cambioTexto { reindexarBacklinksTexto() }
    }

    fileprivate func duplicar(_ vista: VistaElementoBase) {
        var cambioTexto = false
        switch vista {
        case let texto as VistaTextoElemento:
            let marco = texto.marcoDocumento.offsetBy(dx: 28, dy: 28)
            if let id = sesion.anadirTexto(texto.editor.text, marco: marco,
                                           dictado: texto.elemento.desdeDictado),
               let elemento = sesion.textos.first(where: { $0.id == id }) {
                _ = RepositorioAjustesTextoDocumentoIPad.compartido
                    .duplicarElemento(
                        documento: sesion.documentoPersistenteId,
                        origen: texto.elemento.id, destino: id)
                alCambiarTexto?(elemento)
                cambioTexto = true
            }
        case let medio as VistaMedioElemento:
            let e = medio.elemento
            let marco = medio.marcoDocumento.offsetBy(dx: 28, dy: 28)
            if let nuevo = sesion.anadirMedio(hash: e.hash, nombre: e.nombre,
                                              tipo: e.tipo, marco: marco) {
                _ = RepositorioEdicionesImagenDocumentoIPad.compartido
                    .duplicarElemento(
                        documento: sesion.documentoPersistenteId,
                        origen: e.id, destino: nuevo)
                if e.tipo == .pdf {
                    _ = RepositorioClipsPDFDocumentoIPad.compartido
                        .duplicarElemento(
                            documento: sesion.documentoPersistenteId,
                            origen: e.id, destino: nuevo)
                }
            }
        default: break
        }
        if cambioTexto { reindexarBacklinksTexto() }
        recargar()
    }

    fileprivate func reindexarBacklinksTexto() {
        _ = IndexadorBacklinksTextoIPad.reconstruir(sesion: sesion)
    }

    fileprivate func convertir(_ vista: VistaElementoBase, a destino: DestinoFragmento) {
        let referencia: ReferenciaElemento
        if let texto = vista as? VistaTextoElemento {
            referencia = ReferenciaElemento(id: texto.elemento.id,
                                            titulo: String(texto.editor.text.prefix(100)),
                                            tipo: "texto",
                                            paginaDocumento: sesion.documentoPersistenteId)
        } else if let medio = vista as? VistaMedioElemento {
            referencia = ReferenciaElemento(id: medio.elemento.id,
                                            titulo: medio.elemento.nombre,
                                            tipo: medio.elemento.tipo.nombre,
                                            paginaDocumento: sesion.documentoPersistenteId)
        } else { return }
        alConvertir?(referencia, destino)
    }
}

class VistaElementoBase: UIView, UIContextMenuInteractionDelegate {
    weak var propietario: CapaElementos?
    var rotacion: CGFloat = 0
    var bloqueado = false

    init(marco: CGRect, rotacion: CGFloat, propietario: CapaElementos) {
        self.propietario = propietario
        self.rotacion = rotacion
        super.init(frame: marco)
        layer.cornerRadius = 8
        layer.borderWidth = 1
        layer.borderColor = UIColor.systemBlue.withAlphaComponent(0.42).cgColor
        clipsToBounds = true
        transform = CGAffineTransform(rotationAngle: rotacion)
        isUserInteractionEnabled = true
        addGestureRecognizer(UIPanGestureRecognizer(target: self, action: #selector(mover(_:))))
        let pinza = UIPinchGestureRecognizer(target: self, action: #selector(escalar(_:)))
        pinza.delegate = self
        addGestureRecognizer(pinza)
        let giro = UIRotationGestureRecognizer(target: self, action: #selector(girar(_:)))
        giro.delegate = self
        addGestureRecognizer(giro)
        addInteraction(UIContextMenuInteraction(delegate: self))
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    var marcoDocumento: CGRect {
        CGRect(x: center.x - bounds.width / 2, y: center.y - bounds.height / 2,
               width: bounds.width, height: bounds.height)
    }

    func sincronizarGeometria(marco: CGRect, rotacion: CGFloat) {
        self.rotacion = rotacion
        bounds.size = marco.size
        center = CGPoint(x: marco.midX, y: marco.midY)
        transform = CGAffineTransform(rotationAngle: rotacion)
    }

    @objc private func mover(_ gesto: UIPanGestureRecognizer) {
        guard !bloqueado, let superficie = superview else { return }
        let delta = gesto.translation(in: superficie)
        center = CGPoint(x: center.x + delta.x, y: center.y + delta.y)
        gesto.setTranslation(.zero, in: superficie)
        if gesto.state == .ended { persistir() }
    }

    @objc private func escalar(_ gesto: UIPinchGestureRecognizer) {
        guard !bloqueado else { return }
        let nuevo = CGSize(width: min(max(bounds.width * gesto.scale, 44), 5000),
                           height: min(max(bounds.height * gesto.scale, 44), 5000))
        bounds.size = nuevo
        gesto.scale = 1
        if gesto.state == .ended { persistir() }
    }

    @objc private func girar(_ gesto: UIRotationGestureRecognizer) {
        guard !bloqueado else { return }
        rotacion += gesto.rotation
        transform = CGAffineTransform(rotationAngle: rotacion)
        gesto.rotation = 0
        if gesto.state == .ended { persistir() }
    }

    func persistir() {}

    func contextMenuInteraction(_ interaction: UIContextMenuInteraction,
                                configurationForMenuAtLocation location: CGPoint)
        -> UIContextMenuConfiguration? {
        UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { [weak self] _ in
            guard let self else { return UIMenu(title: "", children: []) }
            let conversiones: [UIMenuElement] = DestinoFragmento.allCases.map { destino in
                UIAction(title: "Convertir en \(destino.rawValue)",
                         image: UIImage(systemName: destino.simbolo)) { [weak self] _ in
                    guard let self else { return }
                    self.propietario?.convertir(self, a: destino)
                }
            }
            let convertir = UIMenu(title: "Usar fragmento", image: UIImage(systemName: "wand.and.stars"),
                                   children: conversiones)
            let duplicar = UIAction(title: "Duplicar", image: UIImage(systemName: "plus.square.on.square")) {
                [weak self] _ in guard let self else { return }; self.propietario?.duplicar(self)
            }
            let bloquear = UIAction(title: self.bloqueado ? "Desbloquear" : "Bloquear",
                                    image: UIImage(systemName: self.bloqueado ? "lock.open" : "lock")) {
                [weak self] _ in
                guard let self else { return }
                self.bloqueado.toggle()
                (self as? VistaTextoElemento)?.actualizarEstadoBloqueo()
            }
            let eliminar = UIAction(title: "Eliminar", image: UIImage(systemName: "trash"),
                                    attributes: .destructive) { [weak self] _ in
                guard let self else { return }; self.propietario?.eliminar(self)
            }
            return UIMenu(children: self.accionesEspecificas() + [convertir, duplicar, bloquear, eliminar])
        }
    }

    func accionesEspecificas() -> [UIMenuElement] { [] }
}

extension VistaElementoBase: UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer)
        -> Bool { true }
}

final class VistaTextoElemento: VistaElementoBase, UITextViewDelegate {
    var elemento: ElementoTexto
    let editor = UITextView()
    private var aplicandoEstilo = false

    init(elemento: ElementoTexto, propietario: CapaElementos) {
        self.elemento = elemento
        super.init(marco: elemento.marco, rotacion: elemento.rotacion,
                   propietario: propietario)
        editor.frame = bounds
        editor.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        editor.backgroundColor = .clear
        editor.text = elemento.texto
        editor.font = .preferredFont(forTextStyle: .body)
        editor.isScrollEnabled = false
        editor.isSelectable = true
        editor.linkTextAttributes = [
            .foregroundColor: UIColor.link,
            .underlineStyle: NSUnderlineStyle.single.rawValue,
        ]
        editor.delegate = self
        addSubview(editor)
        configurarTextoAvanzado()
        actualizarEstadoBloqueo()
        actualizarAparienciaEdicion(false)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func persistir() { propietario?.persistir(self) }

    func sincronizar(_ nuevo: ElementoTexto) {
        let editando = editor.isFirstResponder
        elemento = nuevo
        sincronizarGeometria(marco: nuevo.marco, rotacion: nuevo.rotacion)
        guard !editando else { return }
        if editor.text != nuevo.texto {
            editor.text = nuevo.texto
            aplicarEstilo(ajustesDocumentales)
        }
        actualizarEstadoBloqueo()
        actualizarAparienciaEdicion(false)
    }

    private var ajustesDocumentales: AjustesCajaTextoIPad {
        guard let documento = propietario?.sesion.documentoPersistenteId else {
            return RepositorioAjustesTextoIPad.compartido.ajustes(elemento.id)
        }
        return RepositorioAjustesTextoDocumentoIPad.compartido.ajustes(
            documento: documento, id: elemento.id)
    }

    func textViewDidBeginEditing(_ textView: UITextView) {
        actualizarAparienciaEdicion(true)
    }

    func textViewDidEndEditing(_ textView: UITextView) {
        actualizarAparienciaEdicion(false)
        persistir()
        propietario?.reindexarBacklinksTexto()
    }

    func textViewDidChange(_ textView: UITextView) {
        guard !aplicandoEstilo else { return }
        let ajustes = ajustesDocumentales
        aplicarEstilo(ajustes)
        ajustarDimension(ajustes)
    }

    func textView(_ textView: UITextView,
                  shouldChangeTextIn range: NSRange,
                  replacementText text: String) -> Bool {
        guard text == "\n" else { return true }
        let ajustes = ajustesDocumentales
        guard ajustes.continuarListas else { return true }
        let actual = textView.text as NSString
        guard range.location <= actual.length,
              NSMaxRange(range) <= actual.length else { return true }
        let antes = actual.substring(to: range.location) + "\n"
        guard let prefijo = FormateadorMarkdownCajaIPad.prefijoContinuacion(antes),
              !prefijo.isEmpty else { return true }
        let mutable = NSMutableString(string: textView.text)
        let insercion = "\n" + prefijo
        mutable.replaceCharacters(in: range, with: insercion)
        textView.text = String(mutable)
        textView.selectedRange = NSRange(
            location: range.location + (insercion as NSString).length,
            length: 0)
        textViewDidChange(textView)
        return false
    }

    func textView(_ textView: UITextView,
                  shouldInteractWith URL: URL,
                  in characterRange: NSRange,
                  interaction: UITextItemInteraction) -> Bool {
        abrirEnlace(URL)
        return false
    }

    func actualizarEstadoBloqueo() {
        editor.isSelectable = true
        editor.isEditable = !bloqueado
        if bloqueado, editor.isFirstResponder {
            editor.resignFirstResponder()
        }
    }

    private func actualizarAparienciaEdicion(_ editando: Bool) {
        backgroundColor = editando
            ? UIColor.systemBackground.withAlphaComponent(0.92)
            : .clear
        layer.borderWidth = editando ? 1 : 0
        layer.borderColor = UIColor.systemBlue.withAlphaComponent(0.55).cgColor
        let padding = ajustesDocumentales.padding
        editor.textContainerInset = editando
            ? UIEdgeInsets(top: padding, left: padding,
                           bottom: padding, right: padding)
            : .zero
    }

    private func configurarTextoAvanzado() {
        let ajustes = ajustesDocumentales
        editor.textContainerInset = UIEdgeInsets(
            top: ajustes.padding, left: ajustes.padding,
            bottom: ajustes.padding, right: ajustes.padding)
        aplicarEstilo(ajustes)
        ajustarDimension(ajustes)
    }

    private func aplicarEstilo(_ ajustes: AjustesCajaTextoIPad) {
        let fuente = editor.font ?? (editor.typingAttributes[.font] as? UIFont)
            ?? UIFont.preferredFont(forTextStyle: .body)
        let seleccion = editor.selectedRange
        aplicandoEstilo = true
        let atribuidoBase = FormateadorMarkdownCajaIPad.atribuido(
            editor.text, ajustes: ajustes, fuente: fuente, color: .label)
        let atribuido = NSMutableAttributedString(attributedString: atribuidoBase)
        for enlace in DetectorEnlacesAutomaticosIPad.detectar(en: editor.text) {
            let rango = NSRange(location: enlace.rango.localizacionUTF16,
                                length: enlace.rango.longitudUTF16)
            guard rango.location >= 0,
                  NSMaxRange(rango) <= atribuido.length else { continue }
            atribuido.addAttribute(.link, value: enlace.url, range: rango)
        }
        editor.attributedText = atribuido
        editor.typingAttributes = [
            .font: fuente,
            .foregroundColor: UIColor.label,
        ]
        let ubicacion = min(seleccion.location, editor.attributedText.length)
        editor.selectedRange = NSRange(
            location: ubicacion,
            length: min(seleccion.length,
                        max(editor.attributedText.length - ubicacion, 0)))
        aplicandoEstilo = false
    }

    private func ajustarDimension(_ ajustes: AjustesCajaTextoIPad) {
        guard ajustes.dimension != .cajaFija else {
            editor.isScrollEnabled = true; return
        }
        editor.isScrollEnabled = false
        let limiteAncho: CGFloat = ajustes.dimension == .tamanoAutomatico
            ? 1_200 : bounds.width
        let calculado = editor.sizeThatFits(CGSize(
            width: limiteAncho, height: .greatestFiniteMagnitude))
        let ancho = ajustes.dimension == .tamanoAutomatico
            ? min(max(calculado.width, 120), 1_200) : bounds.width
        let alto = min(max(calculado.height, 44), 4_000)
        guard abs(bounds.width - ancho) > 0.5 || abs(bounds.height - alto) > 0.5
        else { return }
        bounds.size = CGSize(width: ancho, height: alto)
        editor.frame = bounds
        persistir()
    }

    override func accionesEspecificas() -> [UIMenuElement] {
        var acciones = propietario?.accionesPPTX(elementoId: elemento.id) ?? []
        let ajustes = ajustesDocumentales
        acciones.append(UIAction(title: "Copiar enlace a esta nota",
                                 image: UIImage(systemName: "link")) {
            [weak self] _ in self?.copiarEnlacePropio()
        })
        acciones.append(UIAction(title: "Backlinks",
                                 image: UIImage(systemName: "arrowshape.turn.up.backward")) {
            [weak self] _ in
            guard let self, let propietario = self.propietario else { return }
            PanelBacklinksDocumentoIPad.presentar(
                documento: propietario.sesion.idPersistente,
                desde: self)
        })

        let enlaces = DetectorEnlacesAutomaticosIPad.detectar(en: editor.text)
        if !enlaces.isEmpty {
            let items: [UIMenuElement] = enlaces.prefix(8).map { enlace in
                UIAction(title: String(enlace.texto.prefix(72)),
                         image: UIImage(systemName: "arrow.up.right.square")) {
                    [weak self] _ in self?.abrirEnlace(enlace.url)
                }
            }
            acciones.append(UIMenu(title: "Enlaces del texto",
                                   image: UIImage(systemName: "link.circle"),
                                   children: items))
        }

        acciones.append(UIAction(title: "Ajustes de caja de texto",
                                 image: UIImage(systemName: "textformat.size")) {
            [weak self] _ in
            guard let self, let propietario = self.propietario else { return }
            RepositorioAjustesTextoIPad.compartido.conContexto(
                documento: propietario.sesion.documentoPersistenteId,
                id: self.elemento.id) {
                propietario.alEditarTextoAvanzado?(self.elemento)
            }
        })
        acciones.append(UIAction(title: "Insertar viñeta",
                                 image: UIImage(systemName: "list.bullet")) {
            [weak self] _ in self?.insertarPrefijo("• ")
        })
        if ajustes.checkboxes {
            acciones.append(UIAction(title: "Insertar checkbox",
                                     image: UIImage(systemName: "checklist")) {
                [weak self] _ in self?.insertarPrefijo("[ ] ")
            })
        }
        return acciones
    }

    private func copiarEnlacePropio() {
        guard let propietario else { return }
        let alto = max(CGFloat(propietario.sesion.altoPagina), 1)
        let pagina = Int(max(0, floor(marcoDocumento.midY / alto)))
        let destino = DestinoEnlaceIPad.documento(
            propietario.sesion.idPersistente,
            pagina: pagina)
        guard let url = try? CodecEnlacesInternosIPad.crearURL(para: destino) else {
            return
        }
        UIPasteboard.general.string = url.absoluteString
        UIAccessibility.post(notification: .announcement,
                             argument: "Enlace interno copiado")
    }

    private func abrirEnlace(_ url: URL) {
        let esquema = url.scheme?.lowercased() ?? ""
        if esquema == "tinta" {
            guard let escena = window?.windowScene else { return }
            _ = CoordinadorNavegacionEnlacesIPad.compartido
                .abrir(url, desde: escena)
            return
        }
        guard ["http", "https"].contains(esquema) else { return }
        UIApplication.shared.open(url)
    }

    private func insertarPrefijo(_ prefijo: String) {
        let rango = editor.selectedRange
        guard let rangoTexto = Range(rango, in: editor.text) else { return }
        editor.text.replaceSubrange(rangoTexto, with: prefijo)
        editor.selectedRange = NSRange(
            location: rango.location + (prefijo as NSString).length, length: 0)
        textViewDidChange(editor)
    }
}

final class VistaMedioElemento: VistaElementoBase {
    var elemento: ElementoMedio
    private let imagen = UIImageView()
    private let nombre = UILabel()
    private var tokenPreviewPDF: TokenMiniaturaPDFIPad?
    private var ultimoTamanoPreviewPDF = CGSize.zero
    private var ajustesFondoRenderizados: AjustesFondoObjetoIPad?

    init(elemento: ElementoMedio, propietario: CapaElementos) {
        self.elemento = elemento
        super.init(marco: elemento.marco, rotacion: elemento.rotacion,
                   propietario: propietario)
        backgroundColor = .tertiarySystemBackground
        alpha = elemento.opacidad
        imagen.frame = bounds
        imagen.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        imagen.contentMode = .scaleAspectFit
        imagen.clipsToBounds = true
        addSubview(imagen)
        nombre.text = elemento.nombre
        nombre.font = .preferredFont(forTextStyle: .caption1)
        nombre.backgroundColor = UIColor.systemBackground.withAlphaComponent(0.82)
        nombre.textAlignment = .center
        nombre.translatesAutoresizingMaskIntoConstraints = false
        addSubview(nombre)
        NSLayoutConstraint.activate([
            nombre.leadingAnchor.constraint(equalTo: leadingAnchor),
            nombre.trailingAnchor.constraint(equalTo: trailingAnchor),
            nombre.bottomAnchor.constraint(equalTo: bottomAnchor),
            nombre.heightAnchor.constraint(greaterThanOrEqualToConstant: 24),
        ])
        cargarVistaPrevia()
    }

    deinit { tokenPreviewPDF?.cancelar() }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    func sincronizar(_ nuevo: ElementoMedio) {
        let fuenteCambio = nuevo.hash != elemento.hash
            || nuevo.tipo != elemento.tipo
            || nuevo.paginaFuente != elemento.paginaFuente
            || nuevo.recorte != elemento.recorte
        let fondoActual = ajustesFondoActual()
        let fondoCambio = ajustesFondoRenderizados != nil
            && ajustesFondoRenderizados != fondoActual

        if fuenteCambio {
            tokenPreviewPDF?.cancelar()
            tokenPreviewPDF = nil
            ultimoTamanoPreviewPDF = .zero
        }

        elemento = nuevo
        sincronizarGeometria(marco: nuevo.marco, rotacion: nuevo.rotacion)
        nombre.text = nuevo.nombre
        alpha = nuevo.opacidad

        if fuenteCambio || fondoCambio {
            cargarVistaPrevia()
        } else if nuevo.tipo == .pdf,
                  let url = RepositorioAdjuntos.compartido.url(hash: nuevo.hash) {
            cargarPreviewPDF(url: url, forzar: false)
        }
    }

    override func persistir() {
        propietario?.persistir(self)
        if elemento.tipo == .pdf,
           let url = RepositorioAdjuntos.compartido.url(hash: elemento.hash) {
            cargarPreviewPDF(url: url, forzar: false)
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        guard elemento.tipo == .pdf,
              let documento = propietario?.sesion.documentoPersistenteId,
              let ruta = RenderizadorClipPDFDocumentoIPad.ruta(
                documento: documento, elementoId: elemento.id,
                en: imagen.bounds) else {
            imagen.layer.mask = nil; return
        }
        let mascara = CAShapeLayer(); mascara.frame = imagen.bounds
        mascara.path = ruta.cgPath; mascara.fillColor = UIColor.black.cgColor
        imagen.layer.mask = mascara
    }

    private func cargarVistaPrevia() {
        guard let url = RepositorioAdjuntos.compartido.url(hash: elemento.hash) else {
            tokenPreviewPDF?.cancelar()
            tokenPreviewPDF = nil
            imagen.image = UIImage(systemName: elemento.tipo.simbolo)
            return
        }
        if elemento.tipo == .imagen, let original = UIImage(contentsOfFile: url.path) {
            tokenPreviewPDF?.cancelar()
            tokenPreviewPDF = nil
            let recortada = Medios.recortar(original, con: elemento.recorte)
            guard let documento = propietario?.sesion.documentoPersistenteId else {
                imagen.image = recortada
                return
            }
            let ajustes = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: documento, id: elemento.id)
            var sinFondo = ajustes
            sinFondo.eliminarFondo = false
            let procesada = ProcesadorImagenLocal.procesar(recortada, ajustes: sinFondo)
            imagen.image = aplicarFondo(procesada)
            if ajustes.eliminarFondo, #available(iOS 17.0, *) {
                AnalizadorImagenLocal.quitarFondo(procesada) { [weak self] resultado in
                    DispatchQueue.main.async {
                        if case .success(let salida) = resultado {
                            self?.imagen.image = self?.aplicarFondo(salida)
                        }
                    }
                }
            }
            return
        }
        if elemento.tipo == .pdf {
            cargarPreviewPDF(url: url, forzar: true)
            return
        }
        tokenPreviewPDF?.cancelar()
        tokenPreviewPDF = nil
        let solicitud = QLThumbnailGenerator.Request(
            fileAt: url, size: CGSize(width: 720, height: 720),
            scale: UIScreen.main.scale, representationTypes: .thumbnail)
        let hashEsperado = elemento.hash
        let idEsperado = elemento.id
        QLThumbnailGenerator.shared.generateBestRepresentation(for: solicitud) {
            [weak self] miniatura, _ in
            DispatchQueue.main.async {
                guard let self,
                      self.elemento.id == idEsperado,
                      self.elemento.hash == hashEsperado else { return }
                let base = miniatura?.uiImage
                    ?? UIImage(systemName: self.elemento.tipo.simbolo)
                self.imagen.image = base.map(self.aplicarFondo)
            }
        }
    }

    private func cargarPreviewPDF(url: URL, forzar: Bool) {
        let objetivo = CGSize(
            width: max(imagen.bounds.width, 180),
            height: max(imagen.bounds.height, 240))
        if !forzar, !Self.cambioMaterial(desde: ultimoTamanoPreviewPDF, hacia: objetivo) {
            return
        }
        ultimoTamanoPreviewPDF = objetivo
        tokenPreviewPDF?.cancelar()
        imagen.image = imagen.image ?? UIImage(systemName: "doc.richtext")

        let idEsperado = elemento.id
        let hashEsperado = elemento.hash
        let paginaEsperada = elemento.paginaFuente
        let recorteEsperado = elemento.recorte
        tokenPreviewPDF = DocumentosPdf.solicitarImagen(
            url: url,
            indice: paginaEsperada,
            tamano: objetivo,
            escala: UIScreen.main.scale) { [weak self] completa in
                guard let self,
                      self.elemento.id == idEsperado,
                      self.elemento.tipo == .pdf,
                      self.elemento.hash == hashEsperado,
                      self.elemento.paginaFuente == paginaEsperada,
                      self.elemento.recorte == recorteEsperado,
                      let completa else { return }
                let base = recorteEsperado.esCompleto
                    ? completa
                    : Medios.recortar(completa, con: recorteEsperado)
                self.imagen.image = self.aplicarFondo(base)
            }
    }

    private static func cambioMaterial(desde anterior: CGSize, hacia nuevo: CGSize) -> Bool {
        guard anterior.width > 0, anterior.height > 0 else { return true }
        let dw = abs(nuevo.width - anterior.width) / max(anterior.width, 1)
        let dh = abs(nuevo.height - anterior.height) / max(anterior.height, 1)
        return dw >= 0.18 || dh >= 0.18
    }

    private func ajustesFondoActual() -> AjustesFondoObjetoIPad {
        let documento = propietario?.sesion.documentoPersistenteId ?? 0
        return RepositorioFondosObjetoIPad.compartido.ajustes(
            documento: documento, elemento: elemento.id)
    }

    private func aplicarFondo(_ original: UIImage) -> UIImage {
        let ajustes = ajustesFondoActual()
        ajustesFondoRenderizados = ajustes
        return ProcesadorFondoObjetoIPad.procesar(original, ajustes: ajustes)
    }

    private func abrirNavegadorPDF() {
        guard elemento.tipo == .pdf,
              let url = RepositorioAdjuntos.compartido.url(hash: elemento.hash),
              let raiz = window?.rootViewController else { return }
        let panel = ControladorPDFNavegableIPad(url: url, pagina: elemento.paginaFuente)
        panel.prepararPresentacionModal()
        let nav = UINavigationController(rootViewController: panel)
        nav.modalPresentationStyle = .pageSheet
        Self.controladorSuperior(raiz).present(nav, animated: true)
    }

    private static func controladorSuperior(_ controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return controladorSuperior(presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return controladorSuperior(visible)
        }
        if let tabs = controlador as? UITabBarController,
           let seleccionado = tabs.selectedViewController {
            return controladorSuperior(seleccionado)
        }
        if let split = controlador as? UISplitViewController,
           let ultimo = split.viewControllers.last {
            return controladorSuperior(ultimo)
        }
        return controlador
    }

    override func accionesEspecificas() -> [UIMenuElement] {
        var acciones: [UIMenuElement] = propietario?.accionesPPTX(
            elementoId: elemento.id) ?? []
        let documento = propietario?.sesion.documentoPersistenteId ?? 0
        let fondo = RepositorioFondosObjetoIPad.compartido.ajustes(
            documento: documento, elemento: elemento.id)
        acciones.append(UIAction(
            title: fondo.esFondo ? "Dejar de usar como fondo" : "Set as Background",
            image: UIImage(systemName: fondo.esFondo ? "rectangle.on.rectangle.slash"
                : "rectangle.on.rectangle")) { [weak self] _ in
            guard let self, let propietario = self.propietario else { return }
            var ajustes = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: propietario.sesion.documentoPersistenteId,
                elemento: self.elemento.id)
            ajustes.esFondo.toggle()
            ajustes.bloqueado = ajustes.esFondo
            ajustes.oculto = false
            RepositorioFondosObjetoIPad.compartido.guardar(
                ajustes, documento: propietario.sesion.documentoPersistenteId,
                elemento: self.elemento.id, nombre: self.elemento.nombre)
            propietario.recargar()
            propietario.alCambiarFondos?()
        })
        acciones.append(UIAction(title: "Ajustes de printout/fondo",
                                 image: UIImage(systemName: "slider.horizontal.3")) {
            [weak self] _ in
            guard let self else { return }
            self.propietario?.alEditarFondo?(self.elemento)
        })
        if elemento.tipo == .imagen {
            acciones.append(UIAction(title: "Recortar y mejorar",
                                     image: UIImage(systemName: "crop")) { [weak self] _ in
                guard let self else { return }
                self.propietario?.alEditarRecorte?(self.elemento)
            })
            if !elemento.recorte.esCompleto {
                acciones.append(UIAction(title: "Restaurar imagen original",
                                         image: UIImage(systemName: "arrow.counterclockwise")) {
                    [weak self] _ in
                    guard let self else { return }
                    _ = self.propietario?.sesion.quitarRecorteMedio(id: self.elemento.id)
                    self.propietario?.recargar()
                })
            }
        }
        if elemento.tipo == .pdf {
            acciones.append(UIAction(title: "Navegar índice y enlaces",
                                     image: UIImage(systemName: "sidebar.left")) { [weak self] _ in
                self?.abrirNavegadorPDF()
            })
            acciones.append(UIAction(title: "Crear clip con referencia",
                                     image: UIImage(systemName: "scissors")) { [weak self] _ in
                guard let self else { return }
                self.propietario?.alCrearClipPDF?(self.elemento)
            })
        }
        if RepositorioProcedenciaIPad.compartido.fuente(hash: elemento.hash) != nil {
            acciones.append(UIAction(title: "Ir al original",
                                     image: UIImage(systemName: "arrow.up.right.square")) {
                [weak self] _ in guard let self else { return }
                self.propietario?.alAbrirFuente?(self.elemento)
            })
            acciones.append(UIAction(title: "Vista Peek del original",
                                     image: UIImage(systemName: "eye")) {
                [weak self] _ in guard let self else { return }
                self.propietario?.alVistaPeek?(self.elemento)
            })
            acciones.append(UIAction(title: "Copiar con referencia",
                                     image: UIImage(systemName: "quote.opening")) {
                [weak self] _ in guard let self else { return }
                self.propietario?.alCopiarCita?(self.elemento)
            })
        }
        if let url = RepositorioAdjuntos.compartido.url(hash: elemento.hash) {
            acciones.append(UIAction(title: "Compartir original",
                                     image: UIImage(systemName: "square.and.arrow.up")) {
                [weak self] _ in self?.propietario?.alCompartir?(url)
            })
        }
        return acciones
    }
}

private extension TipoMedio {
    var simbolo: String {
        switch self {
        case .imagen: return "photo"
        case .video: return "film"
        case .pdf: return "doc.richtext"
        case .documento: return "doc"
        case .audio: return "waveform"
        }
    }
    var nombre: String {
        switch self {
        case .imagen: return "imagen"
        case .video: return "vídeo"
        case .pdf: return "PDF"
        case .documento: return "documento"
        case .audio: return "audio"
        }
    }
}

private extension DestinoFragmento {
    var simbolo: String {
        switch self {
        case .tarea: return "checkmark.circle"
        case .flashcard: return "rectangle.on.rectangle"
        case .pregunta: return "questionmark.bubble"
        case .duda: return "person.crop.circle.badge.questionmark"
        case .nota: return "note.text"
        }
    }
}
