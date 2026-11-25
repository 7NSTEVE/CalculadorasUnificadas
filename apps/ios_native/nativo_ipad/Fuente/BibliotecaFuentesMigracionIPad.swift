import CryptoKit
import PDFKit
import QuickLook
import Compression
import UIKit
import UniformTypeIdentifiers

enum OrigenMaterialIPad: String, Codable, CaseIterable {
    case archivos = "Archivos"
    case campus = "Campus / LMS"
    case onedrive = "OneDrive"
    case infinitoXY = "Formato Infinito exportado"
    case paginadoFijo = "Formato Paginado exportado"
    case libro = "Libro"
    case web = "Web"
    case otro = "Otro"
}

struct MetadatosFuenteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var hash: String
    var nombreOriginal: String
    var origen: OrigenMaterialIPad
    var rutaRelativa: String?
    var autor: String?
    var tituloBibliografico: String?
    var edicion: String?
    var curso: String?
    var bookmark: Data?
    var tamano: Int64?
    var modificacion: Date?
    var registrado = Date()

    func cita(pagina: Int?) -> String {
        var partes: [String] = []
        if let autor, !autor.isEmpty { partes.append(autor) }
        if let tituloBibliografico, !tituloBibliografico.isEmpty {
            partes.append(tituloBibliografico)
        } else { partes.append(nombreOriginal) }
        if let edicion, !edicion.isEmpty { partes.append(edicion) }
        if let pagina { partes.append("p. \(pagina + 1)") }
        return partes.joined(separator: ", ")
    }
}

enum FormaClipPDFIPad: String, Codable, CaseIterable {
    case rectangular, libre, poligonal, parrafo, ecuacion, tabla, grafica, circuito
    var nombre: String {
        switch self {
        case .rectangular: return "Rect"
        case .libre: return "Libre"
        case .poligonal: return "Polígono"
        case .parrafo: return "Párrafo"
        case .ecuacion: return "Ecuación"
        case .tabla: return "Tabla"
        case .grafica: return "Gráfica"
        case .circuito: return "Circuito"
        }
    }
}

struct ClipConFuenteIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var elementoId: String
    var hashFuente: String
    var pagina: Int
    var regionNormalizada: CGRect
    var forma: FormaClipPDFIPad? = nil
    var puntosNormalizados: [CGPoint]? = nil
    var fecha = Date()
    var nota = ""
}

enum RenderizadorClipPDFIPad {
    static func ruta(elementoId: String, en rect: CGRect) -> UIBezierPath? {
        guard let clip = RepositorioProcedenciaIPad.compartido.clip(
            elementoId: elementoId),
              clip.forma == .libre || clip.forma == .poligonal,
              let puntos = clip.puntosNormalizados, puntos.count >= 3,
              clip.regionNormalizada.width > 0, clip.regionNormalizada.height > 0
        else { return nil }
        let ruta = UIBezierPath()
        for (indice, punto) in puntos.enumerated() {
            let local = CGPoint(
                x: rect.minX + (punto.x - clip.regionNormalizada.minX)
                    / clip.regionNormalizada.width * rect.width,
                y: rect.minY + (punto.y - clip.regionNormalizada.minY)
                    / clip.regionNormalizada.height * rect.height)
            if indice == 0 { ruta.move(to: local) } else { ruta.addLine(to: local) }
        }
        ruta.close(); return ruta
    }

    static func enmascarar(_ imagen: UIImage, elementoId: String) -> UIImage {
        guard ruta(elementoId: elementoId,
                   en: CGRect(origin: .zero, size: imagen.size)) != nil else {
            return imagen
        }
        let formato = UIGraphicsImageRendererFormat(); formato.opaque = false
        formato.scale = imagen.scale
        return UIGraphicsImageRenderer(size: imagen.size, format: formato).image { contexto in
            ruta(elementoId: elementoId,
                 en: CGRect(origin: .zero, size: imagen.size))?.addClip()
            imagen.draw(at: .zero)
        }
    }
}

private struct ArchivoProcedenciaIPad: Codable {
    var formato = 1
    var fuentes: [String: MetadatosFuenteIPad] = [:]
    var clips: [String: ClipConFuenteIPad] = [:]
}

final class RepositorioProcedenciaIPad {
    static let compartido = RepositorioProcedenciaIPad()
    private let url: URL
    private var archivo: ArchivoProcedenciaIPad
    private let cola = DispatchQueue(label: "tinta.procedencia")

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Procedencia", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("fuentes.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ArchivoProcedenciaIPad.self, from: $0)
        } ?? ArchivoProcedenciaIPad()
    }

    func registrar(adjunto: AdjuntoImportado, original: URL,
                   origen: OrigenMaterialIPad = .archivos,
                   rutaRelativa: String? = nil, autor: String? = nil,
                   titulo: String? = nil, edicion: String? = nil,
                   curso: String? = nil) {
        cola.sync {
            let acceso = original.startAccessingSecurityScopedResource()
            defer { if acceso { original.stopAccessingSecurityScopedResource() } }
            let valores = try? original.resourceValues(forKeys: [
                .fileSizeKey, .contentModificationDateKey,
            ])
            let bookmark = try? original.bookmarkData(
                options: [],
                includingResourceValuesForKeys: [.fileSizeKey,
                                                  .contentModificationDateKey],
                relativeTo: nil)
            archivo.fuentes[adjunto.hash] = MetadatosFuenteIPad(
                hash: adjunto.hash, nombreOriginal: adjunto.nombreOriginal,
                origen: origen, rutaRelativa: rutaRelativa, autor: autor,
                tituloBibliografico: titulo, edicion: edicion, curso: curso,
                bookmark: bookmark, tamano: valores?.fileSize.map(Int64.init),
                modificacion: valores?.contentModificationDate)
            persistir()
        }
        if let fuente = fuente(hash: adjunto.hash) {
            CentroVigilanciaArchivosIPad.compartido.vigilar(fuente)
        }
    }

    func registrarClip(_ clip: ClipConFuenteIPad) {
        cola.sync { archivo.clips[clip.elementoId] = clip; persistir() }
    }

    func fuente(hash: String) -> MetadatosFuenteIPad? {
        cola.sync { archivo.fuentes[hash] }
    }

    func clip(elementoId: String) -> ClipConFuenteIPad? {
        cola.sync { archivo.clips[elementoId] }
    }

    func todas() -> [MetadatosFuenteIPad] {
        cola.sync { archivo.fuentes.values.sorted { $0.registrado > $1.registrado } }
    }

    func urlOriginal(_ fuente: MetadatosFuenteIPad) -> URL? {
        guard let bookmark = fuente.bookmark else { return nil }
        var obsoleto = false
        return try? URL(resolvingBookmarkData: bookmark, options: [],
                        relativeTo: nil, bookmarkDataIsStale: &obsoleto)
    }

    private func persistir() {
        guard let datos = try? JSONEncoder().encode(archivo) else { return }
        try? datos.write(to: url, options: .atomic)
    }
}

extension Notification.Name {
    static let materialVinculadoCambioIPad = Notification.Name(
        "TintaMaterialVinculadoCambioIPad")
}

private final class PresentadorArchivoVinculadoIPad: NSObject, NSFilePresenter {
    let presentedItemURL: URL?
    let presentedItemOperationQueue = OperationQueue()
    let hashAdjunto: String

    init(url: URL, hash: String) {
        presentedItemURL = url; self.hashAdjunto = hash
        presentedItemOperationQueue.maxConcurrentOperationCount = 1
    }

    func presentedItemDidChange() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: .materialVinculadoCambioIPad,
                                            object: self.hashAdjunto)
        }
    }
}

final class CentroVigilanciaArchivosIPad {
    static let compartido = CentroVigilanciaArchivosIPad()
    private var presentadores: [String: PresentadorArchivoVinculadoIPad] = [:]
    private init() {}

    func vigilar(_ fuente: MetadatosFuenteIPad) {
        guard presentadores[fuente.hash] == nil,
              let url = RepositorioProcedenciaIPad.compartido.urlOriginal(fuente) else { return }
        let presentador = PresentadorArchivoVinculadoIPad(url: url, hash: fuente.hash)
        presentadores[fuente.hash] = presentador
        NSFileCoordinator.addFilePresenter(presentador)
    }
}

enum TipoMaterialCursoIPad: String, Codable, CaseIterable, Hashable {
    case programa = "Programa / cronograma"
    case libro = "Libro"
    case presentacion = "Presentación"
    case guia = "Guía / práctica"
    case tarea = "Tarea"
    case examen = "Examen"
    case datos = "Datos"
    case imagen = "Imagen"
    case audioVideo = "Audio / vídeo"
    case otro = "Otro"
}

struct EntradaCarpetaCursoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var rutaRelativa: String
    var nombre: String
    var tipo: TipoMaterialCursoIPad
    var tamano: Int64
}

struct PlanCarpetaCursoIPad: Codable, Equatable {
    var raiz: String
    var entradas: [EntradaCarpetaCursoIPad]
    var omitidos: [String]
    var totalBytes: Int64
}

enum ErrorImportacionCarpetaIPad: LocalizedError {
    case noDirectorio
    case limite
    var errorDescription: String? {
        switch self {
        case .noDirectorio: return "La selección no es una carpeta legible."
        case .limite: return "La carpeta supera 10 000 archivos o 2 GB; divídala antes de importar."
        }
    }
}

enum AnalizadorCarpetaCursoIPad {
    static func analizar(_ raiz: URL) throws -> PlanCarpetaCursoIPad {
        let acceso = raiz.startAccessingSecurityScopedResource()
        defer { if acceso { raiz.stopAccessingSecurityScopedResource() } }
        guard let enumerador = FileManager.default.enumerator(
            at: raiz, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey],
            options: [.skipsHiddenFiles, .skipsPackageDescendants]) else {
            throw ErrorImportacionCarpetaIPad.noDirectorio
        }
        var entradas: [EntradaCarpetaCursoIPad] = []
        var omitidos: [String] = []
        var total: Int64 = 0
        for case let url as URL in enumerador {
            let valores = try? url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
            guard valores?.isRegularFile == true else { continue }
            let relativa = url.path.replacingOccurrences(of: raiz.path, with: "")
                .trimmingCharacters(in: CharacterSet(charactersIn: "/"))
            let tamano = Int64(valores?.fileSize ?? 0)
            if tamano <= 0 { omitidos.append(relativa); continue }
            entradas.append(EntradaCarpetaCursoIPad(
                rutaRelativa: relativa, nombre: url.lastPathComponent,
                tipo: clasificar(url), tamano: tamano))
            total += tamano
            if entradas.count > 10_000 || total > 2_147_483_648 {
                throw ErrorImportacionCarpetaIPad.limite
            }
        }
        return PlanCarpetaCursoIPad(raiz: raiz.path, entradas: entradas,
                                    omitidos: omitidos, totalBytes: total)
    }

    static func importar(_ plan: PlanCarpetaCursoIPad, raiz: URL)
        -> [Result<AdjuntoImportado, Error>] {
        let acceso = raiz.startAccessingSecurityScopedResource()
        defer { if acceso { raiz.stopAccessingSecurityScopedResource() } }
        return plan.entradas.map { entrada in
            Result {
                let url = raiz.appendingPathComponent(entrada.rutaRelativa)
                let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
                RepositorioProcedenciaIPad.compartido.registrar(
                    adjunto: adjunto, original: url, origen: .archivos,
                    rutaRelativa: entrada.rutaRelativa)
                return adjunto
            }
        }
    }

    private static func clasificar(_ url: URL) -> TipoMaterialCursoIPad {
        let nombre = url.deletingPathExtension().lastPathComponent.lowercased()
        let ext = url.pathExtension.lowercased()
        if nombre.contains("programa") || nombre.contains("cronograma")
            || nombre.contains("syllabus") { return .programa }
        if nombre.contains("examen") || nombre.contains("parcial")
            || nombre.contains("quiz") { return .examen }
        if nombre.contains("tarea") || nombre.contains("assignment") { return .tarea }
        if nombre.contains("guia") || nombre.contains("practica")
            || nombre.contains("worksheet") { return .guia }
        if ["ppt", "pptx", "key"].contains(ext) { return .presentacion }
        if ["epub", "mobi"].contains(ext) || (ext == "pdf" && nombre.contains("book")) {
            return .libro
        }
        if ["csv", "tsv", "xlsx", "xls"].contains(ext) { return .datos }
        if ["png", "jpg", "jpeg", "heic", "tif", "tiff", "webp", "svg"].contains(ext) {
            return .imagen
        }
        if ["mp3", "m4a", "wav", "caf", "mp4", "mov"].contains(ext) {
            return .audioVideo
        }
        return .otro
    }
}

enum PlataformaMigracionIPad: String, Codable {
    case infinitoXY = "Formato Infinito"
    case paginadoFijo = "Formato Paginado"
}

struct ManifiestoMigracionIPad: Codable, Identifiable {
    var id = UUID()
    var plataforma: PlataformaMigracionIPad
    var originalHash: String?
    var importados: [String]
    var estructura: [String: String]
    var advertencias: [String]
    var fecha = Date()
}

enum MigradorNotasIPad {
    static func migrar(_ url: URL, plataforma: PlataformaMigracionIPad)
        throws -> (ManifiestoMigracionIPad, [AdjuntoImportado]) {
        let acceso = url.startAccessingSecurityScopedResource()
        defer { if acceso { url.stopAccessingSecurityScopedResource() } }
        var adjuntos: [AdjuntoImportado] = []
        var estructura: [String: String] = [:]
        var advertencias: [String] = []
        var originalHash: String?
        let valores = try? url.resourceValues(forKeys: [.isDirectoryKey])
        if valores?.isDirectory == true {
            let plan = try AnalizadorCarpetaCursoIPad.analizar(url)
            for (entrada, resultado) in zip(plan.entradas,
                                            AnalizadorCarpetaCursoIPad.importar(plan, raiz: url)) {
                if case .success(let adjunto) = resultado {
                    adjuntos.append(adjunto); estructura[adjunto.hash] = entrada.rutaRelativa
                }
            }
        } else {
            let original = try RepositorioAdjuntos.compartido.importar(url: url)
            originalHash = original.hash
            RepositorioProcedenciaIPad.compartido.registrar(
                adjunto: original, original: url,
                origen: plataforma == .infinitoXY ? .infinitoXY : .paginadoFijo)
            let ext = url.pathExtension.lowercased()
            if ["pdf", "html", "htm", "mht", "mhtml", "docx"].contains(ext) {
                adjuntos.append(original); estructura[original.hash] = url.lastPathComponent
            } else if plataforma == .paginadoFijo && ext == "Formato Paginado" {
                let carpeta = try carpetaMigracion(original.hash)
                var mensaje = [CChar](repeating: 0, count: 1_024)
                let bien = mensaje.withUnsafeMutableBufferPointer { buffer in
                    url.path.withCString { origen in
                        carpeta.path.withCString { destino in
                            tinta_zip_extraer_seguro(origen, destino,
                                                     buffer.baseAddress!,
                                                     buffer.count) == 1
                        }
                    }
                }
                if bien {
                    let plan = try AnalizadorCarpetaCursoIPad.analizar(carpeta)
                    for entrada in plan.entradas where ["pdf", "png", "jpg", "jpeg"]
                        .contains(URL(fileURLWithPath: entrada.nombre).pathExtension.lowercased()) {
                        let recurso = carpeta.appendingPathComponent(entrada.rutaRelativa)
                        if let adjunto = try? RepositorioAdjuntos.compartido.importar(url: recurso) {
                            adjuntos.append(adjunto); estructura[adjunto.hash] = entrada.rutaRelativa
                        }
                    }
                    advertencias.append("La tinta propietaria solo se conserva en el paquete original; se importaron las representaciones PDF/imagen disponibles.")
                } else {
                    let detalle = mensaje.withUnsafeBufferPointer {
                        String(cString: $0.baseAddress!)
                    }
                    advertencias.append("El paquete Formato Paginado se conservó íntegro, pero su estructura no pudo extraerse de forma segura: \(detalle).")
                }
            } else {
                advertencias.append("El formato propietario se conservó íntegro. Exporte también PDF/HTML desde \(plataforma.rawValue) para obtener contenido visible.")
            }
        }
        let manifiesto = ManifiestoMigracionIPad(
            plataforma: plataforma, originalHash: originalHash,
            importados: adjuntos.map(\.hash), estructura: estructura,
            advertencias: advertencias)
        try guardarManifiesto(manifiesto)
        return (manifiesto, adjuntos)
    }

    private static func carpetaMigracion(_ hash: String) throws -> URL {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Tinta Migraciones/\(hash)",
                                                        isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        return carpeta
    }

    private static func guardarManifiesto(_ manifiesto: ManifiestoMigracionIPad) throws {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Tinta Migraciones/Manifiestos",
                                                        isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        let datos = try JSONEncoder().encode(manifiesto)
        try datos.write(to: carpeta.appendingPathComponent(
            "\(manifiesto.id.uuidString).json"), options: .atomic)
    }
}

enum TipoCambioPaginaPDFIPad: String, Codable {
    case igual = "Sin cambios"
    case modificada = "Modificada"
    case agregada = "Agregada"
    case eliminada = "Eliminada"
    case movida = "Movida"
}

struct CambioPaginaPDFIPad: Codable, Identifiable {
    var id = UUID()
    var tipo: TipoCambioPaginaPDFIPad
    var paginaAnterior: Int?
    var paginaNueva: Int?
    var distanciaVisual: Int?
}

struct InformeComparacionPDFIPad: Codable {
    var anterior: String
    var nueva: String
    var cambios: [CambioPaginaPDFIPad]
}

enum ComparadorPDFLocalIPad {
    static func comparar(_ anterior: URL, _ nueva: URL) throws -> InformeComparacionPDFIPad {
        guard let a = PDFDocument(url: anterior), let b = PDFDocument(url: nueva) else {
            throw NSError(domain: "TintaPDF", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "No se pudieron abrir ambos PDF."])
        }
        let firmasA = (0..<a.pageCount).map { firma(a.page(at: $0)) }
        let firmasB = (0..<b.pageCount).map { firma(b.page(at: $0)) }
        var usadosB = Set<Int>()
        var cambios: [CambioPaginaPDFIPad] = []
        for i in firmasA.indices {
            if firmasB.indices.contains(i), firmasA[i].texto == firmasB[i].texto,
               distancia(firmasA[i].visual, firmasB[i].visual) <= 3 {
                usadosB.insert(i)
                cambios.append(CambioPaginaPDFIPad(tipo: .igual,
                                                   paginaAnterior: i, paginaNueva: i,
                                                   distanciaVisual: 0))
                continue
            }
            if let movida = firmasB.indices.first(where: {
                !usadosB.contains($0) && firmasB[$0].texto == firmasA[i].texto
            }) {
                usadosB.insert(movida)
                cambios.append(CambioPaginaPDFIPad(tipo: .movida,
                                                   paginaAnterior: i,
                                                   paginaNueva: movida,
                                                   distanciaVisual: distancia(
                                                    firmasA[i].visual,
                                                    firmasB[movida].visual)))
            } else if firmasB.indices.contains(i), !usadosB.contains(i) {
                usadosB.insert(i)
                cambios.append(CambioPaginaPDFIPad(tipo: .modificada,
                                                   paginaAnterior: i, paginaNueva: i,
                                                   distanciaVisual: distancia(
                                                    firmasA[i].visual,
                                                    firmasB[i].visual)))
            } else {
                cambios.append(CambioPaginaPDFIPad(tipo: .eliminada,
                                                   paginaAnterior: i,
                                                   paginaNueva: nil))
            }
        }
        for i in firmasB.indices where !usadosB.contains(i) {
            cambios.append(CambioPaginaPDFIPad(tipo: .agregada,
                                               paginaAnterior: nil, paginaNueva: i))
        }
        return InformeComparacionPDFIPad(
            anterior: anterior.lastPathComponent, nueva: nueva.lastPathComponent,
            cambios: cambios)
    }

    private static func firma(_ pagina: PDFPage?) -> (texto: String, visual: [Bool]) {
        guard let pagina else { return ("", []) }
        let normal = (pagina.string ?? "").lowercased()
            .components(separatedBy: .whitespacesAndNewlines).joined()
        let texto = SHA256.hash(data: Data(normal.utf8)).map {
            String(format: "%02x", $0)
        }.joined()
        let imagen = pagina.thumbnail(of: CGSize(width: 64, height: 64), for: .mediaBox)
        guard let cg = imagen.cgImage else { return (texto, []) }
        let ancho = 16, alto = 16
        var pixeles = [UInt8](repeating: 0, count: ancho * alto)
        pixeles.withUnsafeMutableBytes { buffer in
            let contexto = CGContext(data: buffer.baseAddress, width: ancho, height: alto,
                                     bitsPerComponent: 8, bytesPerRow: ancho,
                                     space: CGColorSpaceCreateDeviceGray(),
                                     bitmapInfo: CGImageAlphaInfo.none.rawValue)
            contexto?.draw(cg, in: CGRect(x: 0, y: 0, width: ancho, height: alto))
        }
        let media = pixeles.reduce(0, { $0 + Int($1) }) / max(pixeles.count, 1)
        return (texto, pixeles.map { Int($0) >= media })
    }

    private static func distancia(_ a: [Bool], _ b: [Bool]) -> Int {
        guard a.count == b.count else { return max(a.count, b.count) }
        return zip(a, b).reduce(0) { $0 + ($1.0 == $1.1 ? 0 : 1) }
    }
}

final class PanelInformePDFIPad: UITableViewController {
    private let informe: InformeComparacionPDFIPad
    init(_ informe: InformeComparacionPDFIPad) {
        self.informe = informe; super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() { super.viewDidLoad(); title = "Cambios del PDF" }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        informe.cambios.count
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cambio = informe.cambios[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = cambio.tipo.rawValue
        let antes = cambio.paginaAnterior.map { "antes \($0 + 1)" } ?? "—"
        let despues = cambio.paginaNueva.map { "ahora \($0 + 1)" } ?? "—"
        celda.detailTextLabel?.text = "\(antes) · \(despues)" +
            (cambio.distanciaVisual.map { " · diferencia visual \($0)/256" } ?? "")
        return celda
    }
}

final class ControladorPeekFuenteIPad: UIViewController {
    private let url: URL
    private let pagina: Int
    init(url: URL, pagina: Int = 0) { self.url = url; self.pagina = pagina; super.init(nibName:nil,bundle:nil) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        if url.pathExtension.lowercased() == "pdf", let documento = PDFDocument(url: url) {
            let pdf = PDFView(frame: view.bounds); pdf.autoresizingMask = [.flexibleWidth,.flexibleHeight]
            pdf.document = documento; pdf.autoScales = true; view.addSubview(pdf)
            if documento.pageCount > 0,
               let pagina = documento.page(at: min(max(self.pagina, 0),
                                                   documento.pageCount - 1)) {
                pdf.go(to: pagina)
            }
        } else if let imagen = UIImage(contentsOfFile: url.path) {
            let vista = UIImageView(image: imagen); vista.frame = view.bounds
            vista.autoresizingMask = [.flexibleWidth,.flexibleHeight]; vista.contentMode = .scaleAspectFit
            view.addSubview(vista)
        }
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

final class PanelBibliotecaFuentesIPad: UITableViewController,
                                        UIDocumentPickerDelegate {
    private enum OperacionPicker {
        case carpeta
        case infinitoXY
        case paginadoFijo
        case libro
        case compararPrimero
        case compararSegundo(URL)
    }

    private var operacion: OperacionPicker?
    private var fuentes = RepositorioProcedenciaIPad.compartido.todas()
    var alInsertarAdjuntos: (([AdjuntoImportado]) -> Void)?

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Biblioteca, fuentes y migración"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Importar sin perder procedencia" : "Fuentes vinculadas"
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? 5 : max(fuentes.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let valores = [
                ("Importar carpeta completa de curso", "Analiza y propone; no obliga a organizar", "folder.badge.plus"),
                ("Migrar exportación de Formato Infinito", "Carpeta, PDF, HTML/MHTML o DOCX", "arrow.down.doc"),
                ("Migrar exportación de Formato Paginado", "PDF, carpeta o paquete .paginadoFijo", "arrow.down.doc"),
                ("Vincular libro con metadatos", "Autor, título, edición y acceso al original", "books.vertical"),
                ("Comparar dos versiones de PDF", "Páginas añadidas, eliminadas, movidas o cambiadas", "doc.text.magnifyingglass"),
            ][indexPath.row]
            celda.textLabel?.text = valores.0; celda.detailTextLabel?.text = valores.1
            celda.imageView?.image = UIImage(systemName: valores.2)
            celda.accessoryType = .disclosureIndicator
        } else if fuentes.isEmpty {
            celda.textLabel?.text = "Todavía no hay fuentes vinculadas"
            celda.detailTextLabel?.text = "Los archivos importados siguen funcionando localmente."
        } else {
            let fuente = fuentes[indexPath.row]
            celda.textLabel?.text = fuente.tituloBibliografico ?? fuente.nombreOriginal
            celda.detailTextLabel?.text = "\(fuente.origen.rawValue) · \(fuente.cita(pagina: nil))"
            celda.imageView?.image = UIImage(systemName: fuente.origen == .libro
                                             ? "book.closed" : "link")
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard indexPath.section == 0 else {
            if !fuentes.isEmpty { abrirFuente(fuentes[indexPath.row]) }
            return
        }
        switch indexPath.row {
        case 0: elegir(.carpeta, tipos: [.folder], comoCopia: false)
        case 1:
            elegir(.infinitoXY, tipos: [.folder, .pdf, .html, .plainText,
                                      .data], comoCopia: false)
        case 2:
            var tipos: [UTType] = [.folder, .pdf, .data]
            if let paginadoFijo = UTType(filenameExtension: "Formato Paginado") { tipos.append(paginadoFijo) }
            elegir(.paginadoFijo, tipos: tipos, comoCopia: false)
        case 3:
            elegir(.libro, tipos: [.pdf, .epub, .data], comoCopia: false)
        default: elegir(.compararPrimero, tipos: [.pdf], comoCopia: false)
        }
    }

    private func elegir(_ operacion: OperacionPicker, tipos: [UTType], comoCopia: Bool) {
        self.operacion = operacion
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: tipos,
                                                    asCopy: comoCopia)
        picker.delegate = self; picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first, let operacion else { return }
        switch operacion {
        case .carpeta: analizarCarpeta(url)
        case .infinitoXY: migrar(url, plataforma: .infinitoXY)
        case .paginadoFijo: migrar(url, plataforma: .paginadoFijo)
        case .libro: registrarLibro(url)
        case .compararPrimero:
            elegir(.compararSegundo(url), tipos: [.pdf], comoCopia: false)
        case .compararSegundo(let anterior): comparar(anterior, url)
        }
    }

    private func analizarCarpeta(_ url: URL) {
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let plan = try AnalizadorCarpetaCursoIPad.analizar(url)
                DispatchQueue.main.async {
                    let grupos = Dictionary(grouping: plan.entradas, by: \.tipo)
                    let detalle = grupos.keys.sorted { $0.rawValue < $1.rawValue }.map {
                        "\($0.rawValue): \(grupos[$0]?.count ?? 0)"
                    }.joined(separator: "\n")
                    let alerta = UIAlertController(
                        title: "Propuesta para \(plan.entradas.count) archivos",
                        message: detalle + "\n\nLa estructura relativa se conserva. Puede organizar después.",
                        preferredStyle: .alert)
                    alerta.addAction(UIAlertAction(title: "Importar", style: .default) {
                        [weak self] _ in self?.ejecutarPlan(plan, raiz: url)
                    })
                    alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
                    self?.present(alerta, animated: true)
                }
            } catch { DispatchQueue.main.async { self?.avisar(error.localizedDescription) } }
        }
    }

    private func ejecutarPlan(_ plan: PlanCarpetaCursoIPad, raiz: URL) {
        DispatchQueue.global(qos: .utility).async { [weak self] in
            let resultados = AnalizadorCarpetaCursoIPad.importar(plan, raiz: raiz)
            let adjuntos = resultados.compactMap { try? $0.get() }
            let fallos = resultados.count - adjuntos.count
            DispatchQueue.main.async {
                self?.fuentes = RepositorioProcedenciaIPad.compartido.todas()
                self?.tableView.reloadData()
                self?.avisar("Importados \(adjuntos.count). Fallos conservados en el informe: \(fallos).")
            }
        }
    }

    private func migrar(_ url: URL, plataforma: PlataformaMigracionIPad) {
        DispatchQueue.global(qos: .utility).async { [weak self] in
            do {
                let resultado = try MigradorNotasIPad.migrar(url, plataforma: plataforma)
                DispatchQueue.main.async {
                    self?.alInsertarAdjuntos?(resultado.1)
                    let advertencias = resultado.0.advertencias.isEmpty
                        ? "Sin advertencias estructurales."
                        : resultado.0.advertencias.joined(separator: "\n")
                    self?.avisar("Importados \(resultado.1.count) recursos.\n\n\(advertencias)")
                }
            } catch { DispatchQueue.main.async { self?.avisar(error.localizedDescription) } }
        }
    }

    private func registrarLibro(_ url: URL) {
        let alerta = UIAlertController(title: "Datos del libro", message: nil,
                                       preferredStyle: .alert)
        ["Autor", "Título", "Edición", "Curso opcional"].enumerated().forEach { i, valor in
            alerta.addTextField { campo in
                campo.placeholder = valor
                if i == 1 { campo.text = url.deletingPathExtension().lastPathComponent }
            }
        }
        alerta.addAction(UIAlertAction(title: "Vincular", style: .default) { [weak self] _ in
            do {
                let adjunto = try RepositorioAdjuntos.compartido.importar(url: url)
                let campos = alerta.textFields ?? []
                RepositorioProcedenciaIPad.compartido.registrar(
                    adjunto: adjunto, original: url, origen: .libro,
                    autor: campos[safe: 0]?.text, titulo: campos[safe: 1]?.text,
                    edicion: campos[safe: 2]?.text, curso: campos[safe: 3]?.text)
                self?.alInsertarAdjuntos?([adjunto])
                self?.fuentes = RepositorioProcedenciaIPad.compartido.todas()
                self?.tableView.reloadData()
            } catch { self?.avisar(error.localizedDescription) }
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func comparar(_ anterior: URL, _ nueva: URL) {
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let informe = try ComparadorPDFLocalIPad.comparar(anterior, nueva)
                DispatchQueue.main.async {
                    self?.navigationController?.pushViewController(
                        PanelInformePDFIPad(informe), animated: true)
                }
            } catch { DispatchQueue.main.async { self?.avisar(error.localizedDescription) } }
        }
    }

    private func abrirFuente(_ fuente: MetadatosFuenteIPad) {
        guard let url = RepositorioAdjuntos.compartido.url(hash: fuente.hash) else {
            avisar("La copia local ya no está disponible."); return
        }
        present(UINavigationController(rootViewController:
            ControladorPeekFuenteIPad(url: url)), animated: true)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

private extension Array {
    subscript(safe indice: Index) -> Element? { indices.contains(indice) ? self[indice] : nil }
}
