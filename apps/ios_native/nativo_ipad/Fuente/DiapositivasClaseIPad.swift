import AVFoundation
import PDFKit
import PhotosUI
import UIKit
import UniformTypeIdentifiers
import Vision

struct PresentacionLocalIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var hashFuente: String
    var nombreFuente: String
    var importada = Date()
    var cantidadDiapositivas: Int
    var opcionesPPTX: OpcionesImportacionPPTXIPad? = nil
    var recursosPPTX: [RecursoPPTXIPad]? = nil
}

struct DiapositivaLocalIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var presentacionId: UUID
    var numero: Int
    var tituloPresentacion: String
    var hashImagen: String
    var textoExtraido: String
    var notasPresentador: String?
    var hashFuente: String
    var elementosPPTX: [ElementoPPTXIPad]? = nil
    var comentariosPPTX: [ComentarioPPTXIPad]? = nil
    var cantidadPasosAnimacion: Int? = nil
    var opcionesPPTX: OpcionesImportacionPPTXIPad? = nil
}

struct CoincidenciaDiapositivaIPad {
    var diapositiva: DiapositivaLocalIPad
    var puntuacion: Double
    var distanciaVisual: Float
    var coincidenciaTexto: Double
}

struct DiapositivaExtraidaIPad {
    var imagen: UIImage
    var texto: String
    var notasPresentador: String?
    var elementosPPTX: [ElementoPPTXIPad]? = nil
    var comentariosPPTX: [ComentarioPPTXIPad]? = nil
    var cantidadPasosAnimacion: Int? = nil
    var opcionesPPTX: OpcionesImportacionPPTXIPad? = nil
}

struct VinculoFotoDiapositivaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var fotoHash: String
    var diapositivaId: UUID
    var elementoId: UUID?
    var fecha = Date()
    var segundoNotaViva: Double?
    var conservadaComoReferencia = true
}

private struct EstadoDiapositivasIPad: Codable {
    var presentaciones: [PresentacionLocalIPad] = []
    var diapositivas: [DiapositivaLocalIPad] = []
    var vinculos: [VinculoFotoDiapositivaIPad] = []
}

enum ErrorDiapositivasIPad: LocalizedError {
    case pdfInvalido
    case paginaInvalida
    case imagenInvalida
    case manifiestoInvalido

    var errorDescription: String? {
        switch self {
        case .pdfInvalido: return "El PDF de diapositivas no pudo abrirse."
        case .paginaInvalida: return "Una diapositiva no pudo representarse."
        case .imagenInvalida: return "La imagen no pudo indexarse."
        case .manifiestoInvalido: return "La Nota Viva no pudo actualizarse."
        }
    }
}

/// Biblioteca local de slides por contenido. El PPT/PDF original y cada vista
/// de página quedan referenciados por hash, sin copias repetidas en cuadernos.
final class RepositorioDiapositivasIPad {
    static let compartido = RepositorioDiapositivasIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado = EstadoDiapositivasIPad()

    private init() {
        let documentos = FileManager.default.urls(
            for: .documentDirectory, in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent(
            "Academico/Diapositivas", isDirectory: true)
        try? FileManager.default.createDirectory(
            at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("biblioteca.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let recuperado = try? decoder.decode(EstadoDiapositivasIPad.self,
                                                from: datos) {
            estado = recuperado
        }
    }

    var presentaciones: [PresentacionLocalIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.presentaciones.sorted { $0.importada > $1.importada }
    }

    var diapositivas: [DiapositivaLocalIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.diapositivas.sorted {
            $0.tituloPresentacion == $1.tituloPresentacion
                ? $0.numero < $1.numero
                : $0.tituloPresentacion.localizedCaseInsensitiveCompare(
                    $1.tituloPresentacion) == .orderedAscending
        }
    }

    func diapositiva(id: UUID) -> DiapositivaLocalIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.diapositivas.first { $0.id == id }
    }

    func presentacion(id: UUID) -> PresentacionLocalIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.presentaciones.first { $0.id == id }
    }

    func diapositivas(presentacionId: UUID) -> [DiapositivaLocalIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.diapositivas.filter { $0.presentacionId == presentacionId }
            .sorted { $0.numero < $1.numero }
    }

    func importarPDF(_ origen: URL) throws -> PresentacionLocalIPad {
        let acceso = origen.startAccessingSecurityScopedResource()
        defer { if acceso { origen.stopAccessingSecurityScopedResource() } }
        guard let pdf = PDFDocument(url: origen), pdf.pageCount > 0 else {
            throw ErrorDiapositivasIPad.pdfInvalido
        }
        let fuente = try RepositorioAdjuntos.compartido.importar(url: origen)
        bloqueo.lock()
        if let existente = estado.presentaciones.first(where: {
            $0.hashFuente == fuente.hash
        }) {
            bloqueo.unlock()
            return existente
        }
        bloqueo.unlock()

        let presentacion = PresentacionLocalIPad(
            titulo: origen.deletingPathExtension().lastPathComponent,
            hashFuente: fuente.hash,
            nombreFuente: origen.lastPathComponent,
            cantidadDiapositivas: pdf.pageCount)
        var nuevas: [DiapositivaLocalIPad] = []
        for indice in 0..<pdf.pageCount {
            guard let pagina = pdf.page(at: indice) else {
                throw ErrorDiapositivasIPad.paginaInvalida
            }
            let imagen = renderizar(pagina)
            let adjunto = try RepositorioAdjuntos.compartido.importar(
                imagen: imagen,
                nombre: "\(presentacion.titulo) — slide \(indice + 1).png")
            let textoPDF = pagina.string ?? ""
            let texto = textoPDF.isEmpty ? OCRDiapositivasIPad.texto(imagen) : textoPDF
            nuevas.append(DiapositivaLocalIPad(
                presentacionId: presentacion.id,
                numero: indice + 1,
                tituloPresentacion: presentacion.titulo,
                hashImagen: adjunto.hash,
                textoExtraido: texto,
                hashFuente: fuente.hash))
        }
        bloqueo.lock()
        estado.presentaciones.append(presentacion)
        estado.diapositivas.append(contentsOf: nuevas)
        guardarSinBloquear()
        bloqueo.unlock()
        indexar(nuevas)
        return presentacion
    }

    func importarImagenes(_ imagenes: [UIImage], titulo: String) throws
        -> PresentacionLocalIPad {
        guard !imagenes.isEmpty else { throw ErrorDiapositivasIPad.imagenInvalida }
        let id = UUID()
        var huellas: [String] = []
        var nuevas: [DiapositivaLocalIPad] = []
        for (indice, imagen) in imagenes.enumerated() {
            let adjunto = try RepositorioAdjuntos.compartido.importar(
                imagen: imagen, nombre: "\(titulo) — slide \(indice + 1).png")
            huellas.append(adjunto.hash)
            nuevas.append(DiapositivaLocalIPad(
                presentacionId: id, numero: indice + 1,
                tituloPresentacion: titulo,
                hashImagen: adjunto.hash,
                textoExtraido: OCRDiapositivasIPad.texto(imagen),
                hashFuente: adjunto.hash))
        }
        let hashConjunto = huellas.joined(separator: ":")
        bloqueo.lock()
        if let existente = estado.presentaciones.first(where: {
            $0.hashFuente == hashConjunto
        }) {
            bloqueo.unlock()
            return existente
        }
        let presentacion = PresentacionLocalIPad(
            id: id, titulo: titulo, hashFuente: hashConjunto,
            nombreFuente: "Imágenes importadas",
            cantidadDiapositivas: imagenes.count)
        estado.presentaciones.append(presentacion)
        estado.diapositivas.append(contentsOf: nuevas)
        guardarSinBloquear()
        bloqueo.unlock()
        indexar(nuevas)
        return presentacion
    }

    func registrarPresentacionExtraida(
        titulo: String,
        fuente: AdjuntoImportado,
        vistas: [DiapositivaExtraidaIPad],
        recursosPPTX: [RecursoPPTXIPad] = []
    ) throws -> PresentacionLocalIPad {
        guard !vistas.isEmpty else { throw ErrorDiapositivasIPad.imagenInvalida }
        bloqueo.lock()
        if let existente = estado.presentaciones.first(where: {
            $0.hashFuente == fuente.hash
        }) {
            bloqueo.unlock(); return existente
        }
        bloqueo.unlock()
        let presentacion = PresentacionLocalIPad(
            titulo: titulo, hashFuente: fuente.hash,
            nombreFuente: fuente.nombreOriginal,
            cantidadDiapositivas: vistas.count,
            opcionesPPTX: vistas.first?.opcionesPPTX,
            recursosPPTX: recursosPPTX.isEmpty ? nil : recursosPPTX)
        var nuevas: [DiapositivaLocalIPad] = []
        for (indice, vista) in vistas.enumerated() {
            let adjunto = try RepositorioAdjuntos.compartido.importar(
                imagen: vista.imagen,
                nombre: "\(titulo) — slide \(indice + 1).png")
            nuevas.append(DiapositivaLocalIPad(
                presentacionId: presentacion.id,
                numero: indice + 1,
                tituloPresentacion: titulo,
                hashImagen: adjunto.hash,
                textoExtraido: vista.texto,
                notasPresentador: vista.notasPresentador,
                hashFuente: fuente.hash,
                elementosPPTX: vista.elementosPPTX,
                comentariosPPTX: vista.comentariosPPTX,
                cantidadPasosAnimacion: vista.cantidadPasosAnimacion,
                opcionesPPTX: vista.opcionesPPTX))
        }
        bloqueo.lock()
        estado.presentaciones.append(presentacion)
        estado.diapositivas.append(contentsOf: nuevas)
        guardarSinBloquear()
        bloqueo.unlock()
        indexar(nuevas)
        return presentacion
    }

    func buscar(
        foto: UIImage,
        limite: Int = 8,
        completado: @escaping (Result<[CoincidenciaDiapositivaIPad], Error>) -> Void
    ) {
        let candidatas = diapositivas
        DispatchQueue.global(qos: .userInitiated).async {
            guard let huellaFoto = OCRDiapositivasIPad.huella(foto) else {
                completado(.failure(ErrorDiapositivasIPad.imagenInvalida)); return
            }
            let palabrasFoto = OCRDiapositivasIPad.palabras(
                OCRDiapositivasIPad.texto(foto))
            var resultados: [CoincidenciaDiapositivaIPad] = []
            for candidata in candidatas {
                guard let url = RepositorioAdjuntos.compartido.url(
                        hash: candidata.hashImagen),
                      let imagen = UIImage(contentsOfFile: url.path),
                      let huella = OCRDiapositivasIPad.huella(imagen) else { continue }
                var distancia = Float.greatestFiniteMagnitude
                do { try huellaFoto.computeDistance(&distancia, to: huella) }
                catch { continue }
                let palabras = OCRDiapositivasIPad.palabras(candidata.textoExtraido)
                let union = palabras.union(palabrasFoto)
                let texto = union.isEmpty ? 0
                    : Double(palabras.intersection(palabrasFoto).count)
                        / Double(union.count)
                let visual = exp(-Double(max(distancia, 0)) * 3.2)
                let puntuacion = visual * 0.74 + texto * 0.26
                resultados.append(CoincidenciaDiapositivaIPad(
                    diapositiva: candidata, puntuacion: puntuacion,
                    distanciaVisual: distancia, coincidenciaTexto: texto))
            }
            completado(.success(Array(resultados.sorted {
                $0.puntuacion > $1.puntuacion
            }.prefix(max(1, limite)))))
        }
    }

    func registrarVinculo(_ vinculo: VinculoFotoDiapositivaIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard !estado.vinculos.contains(where: {
            $0.fotoHash == vinculo.fotoHash && $0.diapositivaId == vinculo.diapositivaId
        }) else { return }
        estado.vinculos.append(vinculo)
        guardarSinBloquear()
    }

    func adjunto(de diapositiva: DiapositivaLocalIPad) -> AdjuntoImportado? {
        guard let url = RepositorioAdjuntos.compartido.url(hash: diapositiva.hashImagen)
        else { return nil }
        let bytes = Int64((try? url.resourceValues(
            forKeys: [.fileSizeKey]).fileSize) ?? 0)
        return AdjuntoImportado(
            hash: diapositiva.hashImagen, url: url,
            nombreOriginal: "\(diapositiva.tituloPresentacion) — slide \(diapositiva.numero).png",
            bytes: bytes)
    }

    private func renderizar(_ pagina: PDFPage) -> UIImage {
        let caja = pagina.bounds(for: .mediaBox)
        let maximo: CGFloat = 2_560
        let escala = min(min(maximo / max(caja.width, 1),
                             maximo / max(caja.height, 1)), 3)
        let tamano = CGSize(width: max(1, caja.width * escala),
                            height: max(1, caja.height * escala))
        return UIGraphicsImageRenderer(size: tamano).image { contexto in
            UIColor.white.setFill()
            contexto.fill(CGRect(origin: .zero, size: tamano))
            contexto.cgContext.saveGState()
            contexto.cgContext.translateBy(x: 0, y: tamano.height)
            contexto.cgContext.scaleBy(x: escala, y: -escala)
            pagina.draw(with: .mediaBox, to: contexto.cgContext)
            contexto.cgContext.restoreGState()
        }
    }

    private func indexar(_ diapositivas: [DiapositivaLocalIPad]) {
        for slide in diapositivas where !slide.textoExtraido.isEmpty {
            let fuente = ReferenciaFuente(
                tipo: .diapositivaProfesor,
                titulo: "\(slide.tituloPresentacion), diapositiva \(slide.numero)",
                elementoId: slide.id.uuidString,
                hashArchivo: slide.hashFuente,
                pagina: slide.numero)
            CentroAcademico.compartido.agregarNodo(NodoConocimiento(
                titulo: fuente.titulo, texto: slide.textoExtraido,
                fuente: fuente, etiquetas: EtiquetadorLocal.etiquetas(slide.textoExtraido)))
        }
    }

    private func guardarSinBloquear() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let datos = try? encoder.encode(estado) {
            try? datos.write(to: url, options: .atomic)
        }
    }
}

private enum OCRDiapositivasIPad {
    static func texto(_ imagen: UIImage) -> String {
        guard let cg = imagen.cgImage else { return "" }
        let solicitud = VNRecognizeTextRequest()
        solicitud.recognitionLevel = .accurate
        solicitud.usesLanguageCorrection = true
        solicitud.recognitionLanguages = ["es-ES", "en-US"]
        let manejador = VNImageRequestHandler(cgImage: cg, orientation: .up)
        try? manejador.perform([solicitud])
        return (solicitud.results ?? []).compactMap {
            $0.topCandidates(1).first?.string
        }.joined(separator: "\n")
    }

    static func huella(_ imagen: UIImage) -> VNFeaturePrintObservation? {
        guard let cg = imagen.cgImage else { return nil }
        let solicitud = VNGenerateImageFeaturePrintRequest()
        try? VNImageRequestHandler(cgImage: cg, orientation: .up)
            .perform([solicitud])
        return solicitud.results?.first as? VNFeaturePrintObservation
    }

    static func palabras(_ texto: String) -> Set<String> {
        Set(texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                          locale: Locale(identifier: "es"))
            .components(separatedBy: .alphanumerics.inverted)
            .filter { $0.count >= 3 })
    }
}

/// Solo anuncia un cambio cuando hay una coincidencia diferente y suficiente.
/// La UI todavía pide confirmación; una señal débil nunca mueve la clase sola.
final class DetectorCambioDiapositivaIPad {
    private(set) var actual: UUID?
    func evaluar(_ coincidencia: CoincidenciaDiapositivaIPad) -> Bool {
        guard coincidencia.puntuacion >= 0.54,
              coincidencia.diapositiva.id != actual else { return false }
        actual = coincidencia.diapositiva.id
        return true
    }
    func fijar(_ id: UUID?) { actual = id }
}

enum VinculadorVideoNotaVivaIPad {
    static func vincular(video origen: URL, a manifiestoURL: URL) throws {
        let acceso = origen.startAccessingSecurityScopedResource()
        defer { if acceso { origen.stopAccessingSecurityScopedResource() } }
        guard (try? RepositorioManifiestoNotaViva.cargar(manifiestoURL)) != nil
        else { throw ErrorDiapositivasIPad.manifiestoInvalido }
        let carpeta = manifiestoURL.deletingLastPathComponent()
        let extensionPedida = origen.pathExtension.lowercased()
        let ext = !extensionPedida.isEmpty && extensionPedida.count <= 8
            && extensionPedida.allSatisfy { $0.isLetter || $0.isNumber }
            ? extensionPedida : "mov"
        let nombre = "Video autorizado de clase-\(UUID().uuidString.prefix(8)).\(ext)"
        let destino = carpeta.appendingPathComponent(nombre)
        do {
            try FileManager.default.copyItem(at: origen, to: destino)
            try FileManager.default.setAttributes(
                [.protectionKey: FileProtectionType.complete],
                ofItemAtPath: destino.path)
            _ = try RepositorioManifiestoNotaViva.actualizar(manifiestoURL) {
                $0.archivoVideoAutorizado = nombre
                $0.videoAutorizadoConfirmado = true
                $0.eventos.append(EventoNotaViva(
                    tipo: .videoClase, segundo: 0,
                    titulo: "Vídeo autorizado vinculado", recursoId: nombre))
            }
        } catch {
            try? FileManager.default.removeItem(at: destino)
            throw error
        }
    }
}

final class PanelDiapositivasClaseIPad: UITableViewController,
                                         UIDocumentPickerDelegate,
                                         PHPickerViewControllerDelegate {
    var alUsarDiapositiva: ((DiapositivaLocalIPad, AdjuntoImportado, UIImage) -> Void)?
    var alSepararDiapositiva: ((DiapositivaLocalIPad, UIImage, ModoSeparacionPPTXIPad) -> Void)?
    var alInsertarLote: (([DiapositivaLocalIPad], DestinoImportacionPPTXIPad) -> Void)?
    private let repositorio = RepositorioDiapositivasIPad.compartido
    private let manifiestoNotaViva: URL?
    private var resultados: [CoincidenciaDiapositivaIPad] = []
    private var importandoVideo = false
    private var opcionesPPTXPendientes = OpcionesImportacionPPTXIPad.completa

    init(manifiestoNotaViva: URL?) {
        self.manifiestoNotaViva = manifiestoNotaViva
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Slides, fotos y vídeo"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        let importar = UIBarButtonItem(title: "Importar", image: nil,
                                      primaryAction: nil, menu: menuImportar())
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Buscar foto", style: .plain,
                            target: self, action: #selector(buscarFoto)),
            importar,
        ]
    }

    private func menuImportar() -> UIMenu {
        var acciones: [UIAction] = [
            UIAction(title: "PowerPoint .pptx directo",
                     image: UIImage(systemName: "rectangle.stack")) { [weak self] _ in
                self?.importarPPTX()
            },
            UIAction(title: "PDF de diapositivas",
                     image: UIImage(systemName: "doc.richtext")) { [weak self] _ in
                self?.importarPDF()
            },
            UIAction(title: "Imágenes de slides",
                     image: UIImage(systemName: "photo.on.rectangle.angled")) { [weak self] _ in
                self?.importarImagenes()
            },
        ]
        if manifiestoNotaViva != nil {
            acciones.append(UIAction(
                title: "Vincular vídeo autorizado a Nota Viva",
                image: UIImage(systemName: "pip")) { [weak self] _ in
                    self?.confirmarVideo()
                })
        }
        return UIMenu(children: acciones)
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? resultados.count : repositorio.diapositivas.count
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Coincidencias sugeridas" : "Biblioteca local"
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        guard section == 0, !resultados.isEmpty else { return nil }
        return "La foto no se sustituye hasta que usted elija una coincidencia. Se conserva como referencia."
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        let slide = indexPath.section == 0
            ? resultados[indexPath.row].diapositiva
            : repositorio.diapositivas[indexPath.row]
        celda.textLabel?.text = "\(slide.tituloPresentacion) — \(slide.numero)"
        if indexPath.section == 0 {
            let resultado = resultados[indexPath.row]
            celda.detailTextLabel?.text = String(
                format: "Coincidencia sugerida %.0f%% · texto %.0f%%",
                resultado.puntuacion * 100, resultado.coincidenciaTexto * 100)
        } else {
            celda.detailTextLabel?.text = String(slide.textoExtraido.prefix(120))
        }
        celda.detailTextLabel?.numberOfLines = 2
        celda.accessoryType = .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        let slide = indexPath.section == 0
            ? resultados[indexPath.row].diapositiva
            : repositorio.diapositivas[indexPath.row]
        mostrarAcciones(slide, origen: tableView.cellForRow(at: indexPath))
    }

    override func tableView(_ tableView: UITableView,
                            contextMenuConfigurationForRowAt indexPath: IndexPath,
                            point: CGPoint) -> UIContextMenuConfiguration? {
        let slide = indexPath.section == 0
            ? resultados[indexPath.row].diapositiva
            : repositorio.diapositivas[indexPath.row]
        return UIContextMenuConfiguration(identifier: slide.id.uuidString as NSString,
                                           previewProvider: nil) { [weak self] _ in
            self?.menuAcciones(slide) ?? UIMenu()
        }
    }

    private func usar(_ slide: DiapositivaLocalIPad) {
        guard let adjunto = repositorio.adjunto(de: slide),
              let imagen = UIImage(contentsOfFile: adjunto.url.path) else { return }
        alUsarDiapositiva?(slide, adjunto, imagen)
    }

    private func menuAcciones(_ slide: DiapositivaLocalIPad) -> UIMenu {
        var acciones: [UIMenuElement] = [
            UIAction(title: "Insertar vista fiel",
                     image: UIImage(systemName: "rectangle.on.rectangle")) {
                [weak self] _ in self?.usar(slide)
            },
        ]
        if !(slide.elementosPPTX ?? []).isEmpty {
            acciones.append(UIAction(
                title: "Separar elementos editables",
                image: UIImage(systemName: "square.3.layers.3d")) { [weak self] _ in
                    self?.separar(slide, modo: .elementos)
                })
            if (slide.cantidadPasosAnimacion ?? 0) > 0 {
                acciones.append(UIAction(
                    title: "Animación como pasos separados",
                    image: UIImage(systemName: "square.stack.3d.up")) { [weak self] _ in
                        self?.separar(slide, modo: .pasosAnimacion)
                    })
            }
        }
        acciones.append(UIAction(
            title: "Notas, comentarios, vínculos y recursos",
            image: UIImage(systemName: "info.circle")) { [weak self] _ in
                self?.navigationController?.pushViewController(
                    PanelDetalleDiapositivaPPTXIPad(slide), animated: true)
            })
        return UIMenu(children: acciones)
    }

    private func mostrarAcciones(_ slide: DiapositivaLocalIPad, origen: UIView?) {
        let alerta = UIAlertController(
            title: "\(slide.tituloPresentacion) — \(slide.numero)",
            message: "El PPTX original permanece intacto.",
            preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(
            title: "Insertar vista fiel", style: .default) { [weak self] _ in
                self?.usar(slide)
            })
        if !(slide.elementosPPTX ?? []).isEmpty {
            alerta.addAction(UIAlertAction(
                title: "Separar elementos editables", style: .default) { [weak self] _ in
                    self?.separar(slide, modo: .elementos)
                })
            if (slide.cantidadPasosAnimacion ?? 0) > 0 {
                alerta.addAction(UIAlertAction(
                    title: "Animación como pasos separados", style: .default) {
                        [weak self] _ in
                        self?.separar(slide, modo: .pasosAnimacion)
                    })
            }
        }
        alerta.addAction(UIAlertAction(
            title: "Notas, comentarios, vínculos y recursos", style: .default) {
                [weak self] _ in
                self?.navigationController?.pushViewController(
                    PanelDetalleDiapositivaPPTXIPad(slide), animated: true)
            })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = origen ?? view
        alerta.popoverPresentationController?.sourceRect = origen?.bounds ?? CGRect(
            x: view.bounds.midX, y: view.bounds.midY, width: 1, height: 1)
        present(alerta, animated: true)
    }

    private func separar(_ slide: DiapositivaLocalIPad,
                         modo: ModoSeparacionPPTXIPad) {
        guard let adjunto = repositorio.adjunto(de: slide),
              let imagen = UIImage(contentsOfFile: adjunto.url.path) else { return }
        alSepararDiapositiva?(slide, imagen, modo)
    }

    private func importarPDF() {
        importandoVideo = false
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: [.pdf], asCopy: true)
        selector.delegate = self; selector.allowsMultipleSelection = false
        present(selector, animated: true)
    }

    private func importarPPTX() {
        importandoVideo = false
        let opciones = PanelOpcionesImportacionPPTXIPad(
            opciones: opcionesPPTXPendientes)
        opciones.alConfirmar = { [weak self] elegidas in
            self?.opcionesPPTXPendientes = elegidas
            self?.elegirPPTX()
        }
        present(UINavigationController(rootViewController: opciones), animated: true)
    }

    private func elegirPPTX() {
        let tipo = UTType(filenameExtension: "pptx") ?? .data
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: [tipo], asCopy: true)
        selector.delegate = self; selector.allowsMultipleSelection = false
        present(selector, animated: true)
    }

    private func importarImagenes() {
        var configuracion = PHPickerConfiguration(photoLibrary: .shared())
        configuracion.filter = .images; configuracion.selectionLimit = 0
        let selector = PHPickerViewController(configuration: configuracion)
        selector.delegate = self; present(selector, animated: true)
    }

    @objc private func buscarFoto() {
        var configuracion = PHPickerConfiguration(photoLibrary: .shared())
        configuracion.filter = .images; configuracion.selectionLimit = 1
        configuracion.preferredAssetRepresentationMode = .current
        let selector = PHPickerViewController(configuration: configuracion)
        selector.delegate = self; selector.view.tag = 991
        present(selector, animated: true)
    }

    private func confirmarVideo() {
        let alerta = UIAlertController(
            title: "Vincular vídeo autorizado",
            message: "Confirme que tiene permiso para conservar y reproducir este vídeo de clase. La app no activa cámaras ni realiza grabaciones ocultas.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Confirmo autorización", style: .default) {
            [weak self] _ in self?.elegirVideo()
        })
        present(alerta, animated: true)
    }

    private func elegirVideo() {
        importandoVideo = true
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: [.movie], asCopy: true)
        selector.delegate = self; present(selector, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard let origen = urls.first else { return }
        if importandoVideo, let manifiestoNotaViva {
            do {
                try VinculadorVideoNotaVivaIPad.vincular(
                    video: origen, a: manifiestoNotaViva)
                aviso("El vídeo autorizado quedó dentro del paquete y ya puede abrirse en PiP.")
            } catch { aviso(error.localizedDescription) }
            importandoVideo = false; return
        }
        let esPPTX = origen.pathExtension.lowercased() == "pptx"
        let opcionesPPTX = opcionesPPTXPendientes
        let espera = esperaImportacion("Importando e indexando slides…")
        present(espera, animated: true)
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let presentacion: PresentacionLocalIPad?
                if esPPTX {
                    presentacion = try ImportadorPPTXIPad.importar(
                        origen, opciones: opcionesPPTX)
                } else {
                    presentacion = try self?.repositorio.importarPDF(origen)
                }
                DispatchQueue.main.async {
                    espera.dismiss(animated: true)
                    self?.tableView.reloadData()
                    if esPPTX, opcionesPPTX.destino != .biblioteca,
                       let id = presentacion?.id {
                        let slides = self?.repositorio.diapositivas(
                            presentacionId: id) ?? []
                        self?.alInsertarLote?(slides, opcionesPPTX.destino)
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    espera.dismiss(animated: true) { self?.aviso(error.localizedDescription) }
                }
            }
        }
    }

    func picker(_ picker: PHPickerViewController,
                didFinishPicking results: [PHPickerResult]) {
        let buscando = picker.view.tag == 991
        picker.dismiss(animated: true)
        guard !results.isEmpty else { return }
        cargarImagenes(results) { [weak self] imagenes in
            guard let self, !imagenes.isEmpty else { return }
            if buscando, let foto = imagenes.first {
                self.repositorio.buscar(foto: foto) { resultado in
                    DispatchQueue.main.async {
                        if case .success(let coincidencias) = resultado {
                            self.resultados = coincidencias
                            self.tableView.reloadData()
                        }
                    }
                }
            } else {
                let titulo = "Slides \(Date().formatted(date: .abbreviated, time: .shortened))"
                DispatchQueue.global(qos: .userInitiated).async {
                    _ = try? self.repositorio.importarImagenes(imagenes, titulo: titulo)
                    DispatchQueue.main.async { self.tableView.reloadData() }
                }
            }
        }
    }

    private func cargarImagenes(_ resultados: [PHPickerResult],
                                completado: @escaping ([UIImage]) -> Void) {
        let grupo = DispatchGroup(); let bloqueo = NSLock()
        var imagenes: [(Int, UIImage)] = []
        for (indice, resultado) in resultados.enumerated() {
            let proveedor = resultado.itemProvider
            guard proveedor.canLoadObject(ofClass: UIImage.self) else { continue }
            grupo.enter()
            proveedor.loadObject(ofClass: UIImage.self) { objeto, _ in
                if let imagen = objeto as? UIImage {
                    bloqueo.lock(); imagenes.append((indice, imagen)); bloqueo.unlock()
                }
                grupo.leave()
            }
        }
        grupo.notify(queue: .main) {
            completado(imagenes.sorted { $0.0 < $1.0 }.map(\.1))
        }
    }

    private func esperaImportacion(_ texto: String) -> UIAlertController {
        let alerta = UIAlertController(title: texto,
                                       message: "OCR y comparación ocurren localmente.",
                                       preferredStyle: .alert)
        return alerta
    }
    private func aviso(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
