import UIKit
import UniformTypeIdentifiers

enum TipoArrastreTintaIPad {
    static let seleccion = UTType(exportedAs:
        "com.calculadorasunificadas.tinta.seleccion")
}

private struct RectanguloArrastreIPad: Codable {
    var x: Double; var y: Double; var ancho: Double; var alto: Double
    init(_ rect: CGRect) {
        x = rect.minX; y = rect.minY; ancho = rect.width; alto = rect.height
    }
    var cg: CGRect { CGRect(x: x, y: y, width: ancho, height: alto) }
}

private struct TextoArrastreIPad: Codable {
    var texto: String
    var marco: RectanguloArrastreIPad
    var rotacion: Double
    var altoAutomatico: Bool
    var desdeDictado: Bool
}

private struct MedioArrastreIPad: Codable {
    var hash: String
    var nombre: String
    var tipo: TipoMedio
    var marco: RectanguloArrastreIPad
    var rotacion: Double
    var opacidad: Double
    var recorte: Medios.Recorte
    var paginaFuente: Int
    var segundoInicio: Double
}

private struct PaqueteSeleccionArrastreIPad: Codable {
    var formato = 1
    var documentoOrigen: UInt64
    var origenX: Double
    var origenY: Double
    var trazos: [Data]
    var textos: [TextoArrastreIPad]
    var medios: [MedioArrastreIPad]

    static func crear(_ seleccion: SeleccionLienzo,
                      sesion: Sesion) -> PaqueteSeleccionArrastreIPad? {
        guard let marco = ExportadorSeleccionIPad.marco(seleccion, sesion: sesion) else {
            return nil
        }
        let trazos = seleccion.trazos.compactMap(sesion.serializarTrazo)
        let textosSesion = sesion.textos
        let textos = seleccion.textos.compactMap { indice -> TextoArrastreIPad? in
            guard textosSesion.indices.contains(indice) else { return nil }
            let valor = textosSesion[indice]
            return TextoArrastreIPad(
                texto: valor.texto, marco: RectanguloArrastreIPad(valor.marco),
                rotacion: valor.rotacion, altoAutomatico: valor.altoAutomatico,
                desdeDictado: valor.desdeDictado)
        }
        let mediosSesion = sesion.medios
        let medios = seleccion.medios.compactMap { indice -> MedioArrastreIPad? in
            guard mediosSesion.indices.contains(indice) else { return nil }
            let valor = mediosSesion[indice]
            return MedioArrastreIPad(
                hash: valor.hash, nombre: valor.nombre, tipo: valor.tipo,
                marco: RectanguloArrastreIPad(valor.marco),
                rotacion: valor.rotacion, opacidad: valor.opacidad,
                recorte: valor.recorte, paginaFuente: valor.paginaFuente,
                segundoInicio: valor.segundoInicio)
        }
        guard !trazos.isEmpty || !textos.isEmpty || !medios.isEmpty else { return nil }
        return PaqueteSeleccionArrastreIPad(
            documentoOrigen: sesion.documentoPersistenteId,
            origenX: marco.minX, origenY: marco.minY,
            trazos: trazos, textos: textos, medios: medios)
    }

    /// Representación estable para decidir si una selección local sigue siendo
    /// exactamente el snapshot que se copió. `sortedKeys` evita que el orden de
    /// las claves JSON convierta dos snapshots idénticos en distintos.
    var snapshotCanonico: Data? {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        return try? encoder.encode(self)
    }

    /// El paste serializado es todo-o-nada y una sola operación de Undo. La
    /// transacción vive en el núcleo: si cualquier subpaso falla, restaura la
    /// página y también las pilas Undo/Redo previas.
    @discardableResult
    func insertar(en sesion: Sesion, punto: CGPoint) -> Bool {
        sesion.conTransaccionHistorial {
            let trazoInicial = sesion.cantidadTrazos
            for datos in trazos {
                guard sesion.importarTrazoComoCopia(datos) else { return false }
            }
            let trazoFinal = sesion.cantidadTrazos

            let textoInicial = sesion.textos.count
            for texto in textos {
                guard let id = sesion.anadirTexto(
                    texto.texto, marco: texto.marco.cg,
                    dictado: texto.desdeDictado),
                      sesion.actualizarTexto(ElementoTexto(
                        id: id, texto: texto.texto, marco: texto.marco.cg,
                        rotacion: texto.rotacion,
                        altoAutomatico: texto.altoAutomatico,
                        desdeDictado: texto.desdeDictado)) else {
                    return false
                }
            }
            let textoFinal = sesion.textos.count

            let medioInicial = sesion.medios.count
            for medio in medios {
                guard let id = sesion.anadirMedio(
                    hash: medio.hash, nombre: medio.nombre, tipo: medio.tipo,
                    marco: medio.marco.cg),
                      sesion.actualizarMedio(ElementoMedio(
                        id: id, hash: medio.hash, nombre: medio.nombre,
                        tipo: medio.tipo, marco: medio.marco.cg,
                        rotacion: medio.rotacion, opacidad: medio.opacidad,
                        recorte: medio.recorte,
                        paginaFuente: medio.paginaFuente,
                        segundoInicio: medio.segundoInicio)),
                      sesion.configurarFuenteMedio(
                        id: id, pagina: medio.paginaFuente,
                        segundo: medio.segundoInicio) else {
                    return false
                }
                if !medio.recorte.esCompleto {
                    guard sesion.recortarMedio(
                        id: id, recorteRelativo: CGRect(
                            x: CGFloat(medio.recorte.x),
                            y: CGFloat(medio.recorte.y),
                            width: CGFloat(medio.recorte.ancho),
                            height: CGFloat(medio.recorte.alto))) else {
                        return false
                    }
                }
            }
            let medioFinal = sesion.medios.count
            let importada = SeleccionLienzo(
                trazos: trazoFinal > trazoInicial
                    ? Array(trazoInicial..<trazoFinal) : [],
                textos: textoFinal > textoInicial
                    ? Array(textoInicial..<textoFinal) : [],
                medios: medioFinal > medioInicial
                    ? Array(medioInicial..<medioFinal) : [])
            guard !importada.vacia else { return false }
            let delta = CGSize(
                width: punto.x - origenX,
                height: punto.y - origenY)
            return sesion.transformarSeleccion(
                importada, desplazamiento: delta)
        }
    }
}

private final class ContextoArrastreSeleccionIPad {
    weak var sesionOrigen: Sesion?
    let seleccionOrigen: SeleccionLienzo
    let paquete: PaqueteSeleccionArrastreIPad
    init(sesion: Sesion, seleccion: SeleccionLienzo,
         paquete: PaqueteSeleccionArrastreIPad) {
        sesionOrigen = sesion; seleccionOrigen = seleccion; self.paquete = paquete
    }
}

private final class PortapapelesRegionTintaIPad {
    static let compartido = PortapapelesRegionTintaIPad()
    var paquete: PaqueteSeleccionArrastreIPad?
    weak var sesionOrigen: Sesion?
    var clavesOrigen: [String] = []
    var snapshotCanonico: Data?
    var generacion = UUID()

    func publicar(
        paquete: PaqueteSeleccionArrastreIPad,
        seleccion: SeleccionLienzo,
        sesion: Sesion,
        generacion: UUID
    ) {
        self.paquete = paquete
        self.sesionOrigen = sesion
        self.clavesOrigen = RepositorioMetadatosSeleccionIPad.compartido
            .claves(de: seleccion, sesion: sesion)
        self.snapshotCanonico = paquete.snapshotCanonico
        self.generacion = generacion
    }

    /// Sólo devuelve una selección para la ruta atómica si continúa apuntando a
    /// los mismos IDs y sus bytes siguen siendo exactamente los copiados. Si el
    /// original cambió, el caller debe usar `paquete.insertar` para respetar el
    /// snapshot del clipboard.
    func seleccionLocalIntacta(en sesion: Sesion) -> SeleccionLienzo? {
        guard sesionOrigen === sesion,
              !clavesOrigen.isEmpty,
              let snapshotCanonico else { return nil }
        let seleccion = RepositorioMetadatosSeleccionIPad.compartido.seleccion(
            de: Set(clavesOrigen), sesion: sesion)
        guard !seleccion.vacia,
              let actual = PaqueteSeleccionArrastreIPad.crear(
                seleccion, sesion: sesion),
              actual.snapshotCanonico == snapshotCanonico else { return nil }
        return seleccion
    }
}

extension LienzoCuadernU {
    /// Deja lista la selección actual para pegado/arrastre editable.
    @discardableResult
    func prepararSeleccionParaPegadoRapido(incluirHistorial: Bool = false) -> Bool {
        prepararSeleccionParaPegadoRapido(
            seleccionActual, incluirHistorial: incluirHistorial)
    }

    /// Variante explícita usada por edición semántica de handwriting. Permite
    /// copiar una palabra/línea geométrica sin tener que alterar el estado del
    /// lazo ni crear un segundo modelo de selección.
    @discardableResult
    func prepararSeleccionParaPegadoRapido(
        _ seleccion: SeleccionLienzo,
        incluirHistorial: Bool = false
    ) -> Bool {
        guard !seleccion.vacia,
              let paquete = PaqueteSeleccionArrastreIPad.crear(
                seleccion, sesion: sesion) else { return false }

        let portapapeles = PortapapelesRegionTintaIPad.compartido
        let generacion = UUID()
        portapapeles.publicar(
            paquete: paquete,
            seleccion: seleccion,
            sesion: sesion,
            generacion: generacion)

        UINotificationFeedbackGenerator().notificationOccurred(.success)
        alNotificarGestoRegion?(
            "Selección lista: arrástrela o mantenga pulsado para pegar una copia.")

        DispatchQueue.main.async { [weak self] in
            guard let self,
                  PortapapelesRegionTintaIPad.compartido.generacion == generacion,
                  let imagen = try? ExportadorSeleccionIPad.imagen(
                    seleccion: seleccion, sesion: self.sesion,
                    escalaSolicitada: 1, limitePixeles: 2048) else { return }
            UIPasteboard.general.image = imagen
            guard incluirHistorial,
                  let adjunto = try? RepositorioAdjuntos.compartido.importar(
                    imagen: imagen, nombre: "Región copiada con Pencil.png") else {
                return
            }
            HistorialPortapapeles.compartido.agregar(EntradaPortapapeles(
                tipo: .imagen, hashAdjunto: adjunto.hash,
                nombre: "Región copiada del lienzo"))
        }
        return true
    }

    func activarGestoCopiaRegion() {
        activarGestosTintaNaturalIPad()
        let existeDoble = gestureRecognizers?.contains {
            ($0 as? UITapGestureRecognizer)?.name == "tinta.copiar.region"
        } ?? false
        if !existeDoble {
            let doble = UITapGestureRecognizer(target: self,
                                               action: #selector(copiarRegionConDobleToque(_:)))
            doble.numberOfTapsRequired = 2
            doble.cancelsTouchesInView = false
            doble.delaysTouchesBegan = false
            doble.allowedTouchTypes = [
                NSNumber(value: UITouch.TouchType.direct.rawValue),
                NSNumber(value: UITouch.TouchType.pencil.rawValue),
            ]
            doble.name = "tinta.copiar.region"
            addGestureRecognizer(doble)
        }
        let existePegado = gestureRecognizers?.contains {
            ($0 as? UILongPressGestureRecognizer)?.name == "tinta.pegar.region"
        } ?? false
        if !existePegado {
            let mantener = UILongPressGestureRecognizer(
                target: self, action: #selector(pegarRegionManteniendo(_:)))
            mantener.minimumPressDuration = 0.55
            mantener.cancelsTouchesInView = false
            mantener.allowedTouchTypes = [
                NSNumber(value: UITouch.TouchType.direct.rawValue),
            ]
            mantener.name = "tinta.pegar.region"
            addGestureRecognizer(mantener)
        }
    }

    @objc private func copiarRegionConDobleToque(_ gesto: UITapGestureRecognizer) {
        guard herramienta == .lazo, !seleccionActual.vacia,
              let marco = marcoSeleccionActual()?.insetBy(dx: -18, dy: -18),
              marco.contains(gesto.location(in: self)) else { return }
        _ = prepararSeleccionParaPegadoRapido(incluirHistorial: true)
    }

    @objc private func pegarRegionManteniendo(_ gesto: UILongPressGestureRecognizer) {
        guard gesto.state == .began else { return }
        let portapapeles = PortapapelesRegionTintaIPad.compartido
        guard let paquete = portapapeles.paquete else { return }
        let punto = gesto.location(in: self)

        let pegada: Bool
        if let seleccion = portapapeles.seleccionLocalIntacta(en: sesion) {
            let delta = CGSize(
                width: punto.x - paquete.origenX,
                height: punto.y - paquete.origenY)
            let copia = sesion.duplicarSeleccion(
                seleccion, desplazamiento: delta)
            pegada = !copia.vacia
        } else {
            pegada = paquete.insertar(en: sesion, punto: punto)
        }
        guard pegada else { return }

        capaElementosDestinoArrastre?.recargar()
        rehornearYDibujar()
        alCambiarContenidoSeleccion?()
        UINotificationFeedbackGenerator().notificationOccurred(.success)
        alNotificarGestoRegion?(
            "Región pegada como una sola operación reversible.")
    }
}

extension LienzoCuadernU: UIDragInteractionDelegate {
    func activarArrastreDeSeleccion() {
        guard !interactions.contains(where: { $0 is UIDragInteraction }) else { return }
        let interaccion = UIDragInteraction(delegate: self)
        interaccion.isEnabled = true; addInteraction(interaccion)
    }

    func dragInteraction(_ interaction: UIDragInteraction,
                         itemsForBeginning session: UIDragSession) -> [UIDragItem] {
        guard !seleccionActual.vacia,
              let paquete = PaqueteSeleccionArrastreIPad.crear(
                seleccionActual, sesion: sesion),
              let datos = try? JSONEncoder().encode(paquete) else { return [] }
        let proveedor: NSItemProvider
        if let imagen = try? ExportadorSeleccionIPad.imagen(
            seleccion: seleccionActual, sesion: sesion, escalaSolicitada: 1.5) {
            proveedor = NSItemProvider(object: imagen)
        } else {
            proveedor = NSItemProvider()
        }
        proveedor.registerDataRepresentation(
            forTypeIdentifier: TipoArrastreTintaIPad.seleccion.identifier,
            visibility: .all) { completar in
                completar(datos, nil); return nil
            }
        let item = UIDragItem(itemProvider: proveedor)
        item.localObject = ContextoArrastreSeleccionIPad(
            sesion: sesion, seleccion: seleccionActual, paquete: paquete)
        return [item]
    }

    func dragInteraction(_ interaction: UIDragInteraction,
                         sessionAllowsMoveOperation session: UIDragSession) -> Bool {
        false
    }
}

extension LienzoCuadernU: UIDropInteractionDelegate {
    func activarArrastreMasivo(capaElementos: CapaElementos) {
        capaElementosDestinoArrastre = capaElementos
        guard !interactions.contains(where: { $0 is UIDropInteraction }) else { return }
        addInteraction(UIDropInteraction(delegate: self))
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         canHandle session: UIDropSession) -> Bool {
        session.hasItemsConforming(toTypeIdentifiers: [
            TipoArrastreTintaIPad.seleccion.identifier, UTType.image.identifier,
            UTType.pdf.identifier, UTType.fileURL.identifier, UTType.text.identifier])
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         sessionDidUpdate session: UIDropSession) -> UIDropProposal {
        UIDropProposal(operation: .copy)
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         performDrop session: UIDropSession) {
        importarItemsArrastrados(session.items, en: session.location(in: self))
    }

    func importarItemsArrastrados(_ items: [UIDragItem], en base: CGPoint) {
        for (indice, item) in items.enumerated() {
            let punto = CGPoint(x: base.x + CGFloat(indice % 5) * 34,
                                y: base.y + CGFloat(indice / 5) * 34)
            if item.itemProvider.hasItemConformingToTypeIdentifier(
                TipoArrastreTintaIPad.seleccion.identifier) {
                recibirSeleccionArrastrada(item, en: punto)
            } else {
                recibirElementoExterno(item, en: punto)
            }
        }
    }

    private func recibirSeleccionArrastrada(_ item: UIDragItem, en punto: CGPoint) {
        if let contexto = item.localObject as? ContextoArrastreSeleccionIPad {
            if contexto.sesionOrigen === sesion {
                let delta = CGSize(width: punto.x - contexto.paquete.origenX,
                                   height: punto.y - contexto.paquete.origenY)
                let copia = sesion.duplicarSeleccion(
                    contexto.seleccionOrigen, desplazamiento: delta)
                if !copia.vacia { finalizarImportacionArrastre() }
            } else if contexto.paquete.insertar(en: sesion, punto: punto) {
                finalizarImportacionArrastre()
            }
            return
        }
        item.itemProvider.loadDataRepresentation(
            forTypeIdentifier: TipoArrastreTintaIPad.seleccion.identifier) {
                [weak self] datos, _ in
                guard let self, let datos,
                      let paquete = try? JSONDecoder().decode(
                        PaqueteSeleccionArrastreIPad.self, from: datos) else { return }
                DispatchQueue.main.async {
                    if paquete.insertar(en: self.sesion, punto: punto) {
                        self.finalizarImportacionArrastre()
                    }
                }
            }
    }

    private func recibirElementoExterno(_ item: UIDragItem, en punto: CGPoint) {
        let proveedor = item.itemProvider
        guard let capa = capaElementosDestinoArrastre else { return }
        if proveedor.canLoadObject(ofClass: UIImage.self) {
            proveedor.loadObject(ofClass: UIImage.self) { [weak self, weak capa] objeto, _ in
                guard let imagen = objeto as? UIImage,
                      let adjunto = try? RepositorioAdjuntos.compartido.importar(
                    imagen: imagen, nombre: "Imagen arrastrada.png") else { return }
                DispatchQueue.main.async {
                    _ = capa?.insertarAdjunto(
                        adjunto, tipo: .imagen, en: punto,
                        tamano: Medios.tamanoDeInsercion(imagen.size))
                    self?.finalizarImportacionArrastre()
                }
            }
        } else if proveedor.hasItemConformingToTypeIdentifier(UTType.pdf.identifier) {
            proveedor.loadFileRepresentation(forTypeIdentifier: UTType.pdf.identifier) {
                [weak self, weak capa] url, _ in
                guard let url,
                      let adjunto = try? RepositorioAdjuntos.compartido.importar(url: url)
                else { return }
                DispatchQueue.main.async {
                    _ = capa?.insertarAdjunto(adjunto, tipo: .pdf, en: punto)
                    self?.finalizarImportacionArrastre()
                }
            }
        } else if proveedor.hasItemConformingToTypeIdentifier(
            UTType.fileURL.identifier) {
            proveedor.loadItem(forTypeIdentifier: UTType.fileURL.identifier) {
                [weak self, weak capa] objeto, _ in
                guard let url = objeto as? URL,
                      let adjunto = try? RepositorioAdjuntos.compartido.importar(url: url)
                else { return }
                let ext = url.pathExtension.lowercased()
                let tipo: TipoMedio
                if ["m4a", "mp3", "wav", "caf", "aac"].contains(ext) {
                    tipo = .audio
                } else if ["mov", "mp4", "m4v"].contains(ext) {
                    tipo = .video
                } else if ext == "pdf" {
                    tipo = .pdf
                } else {
                    tipo = .documento
                }
                DispatchQueue.main.async {
                    _ = capa?.insertarAdjunto(adjunto, tipo: tipo, en: punto)
                    self?.finalizarImportacionArrastre()
                }
            }
        } else if proveedor.canLoadObject(ofClass: NSString.self) {
            proveedor.loadObject(ofClass: NSString.self) { [weak self, weak capa] objeto, _ in
                guard let texto = objeto as? String else { return }
                DispatchQueue.main.async {
                    _ = capa?.insertarTexto(texto, en: punto)
                    self?.finalizarImportacionArrastre()
                }
            }
        }
    }

    private func finalizarImportacionArrastre() {
        capaElementosDestinoArrastre?.recargar()
        rehornearYDibujar()
        alCambiarContenidoSeleccion?()
    }
}
