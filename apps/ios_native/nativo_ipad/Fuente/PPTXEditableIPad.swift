import Foundation
import UIKit

enum RepresentacionImportacionPPTXIPad: String, Codable, CaseIterable {
    case hibrida = "Híbrida: vista fiel + objetos editables"
    case raster = "Vista rasterizada, fuente intacta"
    case editable = "Objetos editables, vista de respaldo"
}

enum DestinoImportacionPPTXIPad: String, Codable, CaseIterable {
    case biblioteca = "Guardar en biblioteca de diapositivas"
    case paginaActual = "Insertar las diapositivas en la página actual"
    case paginasImprimibles = "Cada diapositiva como página separada"
    case presentacionHibrida = "Slide + espacio de apuntes"
}

struct OpcionesImportacionPPTXIPad: Codable, Equatable {
    var representacion: RepresentacionImportacionPPTXIPad = .hibrida
    var destino: DestinoImportacionPPTXIPad = .biblioteca
    var conservarTextoSeleccionable = true
    var conservarHipervinculos = true
    var conservarImagenesOriginales = true
    var importarNotasPresentador = true
    var importarComentarios = true
    var separarAnimaciones = true
    var extraerRecursos = true

    static let completa = OpcionesImportacionPPTXIPad()
}

enum TipoElementoPPTXIPad: String, Codable, CaseIterable {
    case texto
    case imagen
    case formaVectorial
    case ecuacion
    case tabla
    case grafica
    case diagrama
    case audio
    case video
    case objetoIncrustado
    case desconocido

    var nombre: String {
        switch self {
        case .texto: return "Texto"
        case .imagen: return "Imagen"
        case .formaVectorial: return "Forma vectorial"
        case .ecuacion: return "Ecuación"
        case .tabla: return "Tabla"
        case .grafica: return "Gráfica"
        case .diagrama: return "Diagrama"
        case .audio: return "Audio"
        case .video: return "Vídeo"
        case .objetoIncrustado: return "Objeto incrustado"
        case .desconocido: return "Elemento conservado"
        }
    }
}

struct HipervinculoPPTXIPad: Codable, Equatable, Hashable {
    var texto: String
    var destino: String
    var externo: Bool
}

struct ComentarioPPTXIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var autor: String?
    var texto: String
    var fecha: Date?
}

struct TablaPPTXIPad: Codable, Equatable {
    var filas: [[String]]

    var cantidadColumnas: Int { filas.map(\.count).max() ?? 0 }
}

struct RecursoPPTXIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var rutaOpenXML: String
    var hash: String
    var tipo: TipoElementoPPTXIPad
    var bytes: Int64
}

struct ElementoPPTXIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var idOffice: String?
    var nombre: String
    var tipo: TipoElementoPPTXIPad
    /// Coordenadas 0...1 respecto de la diapositiva original.
    var marcoNormalizado: CGRect
    var texto: String = ""
    var hashRecurso: String?
    var nombreRecurso: String?
    var hipervinculo: HipervinculoPPTXIPad?
    var pasoAnimacion: Int?
    var geometria: String?
    var colorRellenoARGB: UInt32?
    var colorTrazoARGB: UInt32?
    var grosorTrazo: CGFloat?
    var tabla: TablaPPTXIPad?
    var vectorial = false
}

enum ModoSeparacionPPTXIPad {
    case elementos
    case pasosAnimacion
}

struct ResultadoSeparacionPPTXIPad {
    var textos = 0
    var recursos = 0
    var bloques = 0
    var vectores = 0
    var omitidos = 0

    var resumen: String {
        "Textos: \(textos) · recursos: \(recursos) · bloques: \(bloques) · vectores: \(vectores) · conservados sin materializar: \(omitidos)"
    }
}

struct MetadatoObjetoPPTXIPad: Codable, Equatable {
    var diapositivaId: UUID
    var presentacion: String
    var numeroDiapositiva: Int
    var idElementoPPTX: UUID
    var idOffice: String?
    var tipo: TipoElementoPPTXIPad
    var hashFuente: String
    var hipervinculo: HipervinculoPPTXIPad?
    var pasoAnimacion: Int?
}

/// Mantiene procedencia, vínculo y paso de animación después de separar una
/// diapositiva. El objeto nativo puede moverse o editarse sin perder el enlace
/// al PPTX inmutable que originó el contenido.
final class RepositorioObjetosPPTXIPad {
    static let compartido = RepositorioObjetosPPTXIPad()
    private let bloqueo = NSLock()
    private let url: URL
    private var documentos: [String: [String: MetadatoObjetoPPTXIPad]] = [:]

    private init() {
        let base = FileManager.default.urls(for: .documentDirectory,
                                            in: .userDomainMask).first!
        let carpeta = base.appendingPathComponent("Academico/PPTXObjetos",
                                                  isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("procedencia.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(
            [String: [String: MetadatoObjetoPPTXIPad]].self, from: datos) {
            documentos = leido
        }
    }

    func registrar(_ metadato: MetadatoObjetoPPTXIPad,
                   objetoId: String, documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        documentos[String(documento), default: [:]][objetoId] = metadato
        guardarSinBloquear()
    }

    func metadato(objetoId: String, documento: UInt64) -> MetadatoObjetoPPTXIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return documentos[String(documento)]?[objetoId]
    }

    private func guardarSinBloquear() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let datos = try? encoder.encode(documentos) {
            try? datos.write(to: url, options: .atomic)
        }
    }
}

enum ErrorSeparacionPPTXIPad: LocalizedError {
    case sinElementos

    var errorDescription: String? {
        "La diapositiva no contiene elementos editables extraídos. La vista fiel y el PPTX original siguen disponibles."
    }
}

/// Materializa los elementos Open XML en capas nativas. No borra ni modifica
/// la vista de la diapositiva, y cada objeto conserva procedencia independiente.
enum SeparadorDiapositivaPPTXIPad {
    static func insertar(
        _ diapositiva: DiapositivaLocalIPad,
        imagen: UIImage,
        en navegador: VistaLienzoNavegable,
        desde origen: CGPoint,
        modo: ModoSeparacionPPTXIPad
    ) throws -> ResultadoSeparacionPPTXIPad {
        guard let elementos = diapositiva.elementosPPTX, !elementos.isEmpty else {
            throw ErrorSeparacionPPTXIPad.sinElementos
        }
        let ancho: CGFloat = 760
        let alto = max(320, ancho * imagen.size.height / max(imagen.size.width, 1))
        let tamano = CGSize(width: ancho, height: alto)
        var resultado = ResultadoSeparacionPPTXIPad()

        switch modo {
        case .elementos:
            insertarGrupo(elementos, diapositiva: diapositiva,
                          navegador: navegador,
                          marco: CGRect(origin: origen, size: tamano),
                          resultado: &resultado)
        case .pasosAnimacion:
            let maximo = elementos.compactMap(\.pasoAnimacion).max() ?? 0
            let pasos = max(maximo, 1)
            for paso in 0...pasos {
                let columna = paso % 2, fila = paso / 2
                let punto = CGPoint(
                    x: origen.x + CGFloat(columna) * (tamano.width + 56),
                    y: origen.y + CGFloat(fila) * (tamano.height + 112))
                _ = navegador.capaElementos.insertarTexto(
                    "\(diapositiva.tituloPresentacion) · diapositiva \(diapositiva.numero) · paso \(paso)",
                    marco: CGRect(x: punto.x, y: punto.y - 42,
                                  width: tamano.width, height: 34))
                let visibles = elementos.filter {
                    guard let animacion = $0.pasoAnimacion else { return true }
                    return animacion <= paso
                }
                insertarGrupo(visibles, diapositiva: diapositiva,
                              navegador: navegador,
                              marco: CGRect(origin: punto, size: tamano),
                              resultado: &resultado)
            }
        }
        return resultado
    }

    private static func insertarGrupo(
        _ elementos: [ElementoPPTXIPad],
        diapositiva: DiapositivaLocalIPad,
        navegador: VistaLienzoNavegable,
        marco: CGRect,
        resultado: inout ResultadoSeparacionPPTXIPad
    ) {
        for elemento in elementos {
            let destino = marcoElemento(elemento.marcoNormalizado, dentro: marco)
            let objetoId: String?
            switch elemento.tipo {
            case .texto:
                guard !elemento.texto.trimmingCharacters(
                    in: .whitespacesAndNewlines).isEmpty else {
                    resultado.omitidos += 1; continue
                }
                objetoId = navegador.capaElementos.insertarTexto(
                    elemento.texto, marco: destino)
                if objetoId != nil { resultado.textos += 1 }
            case .imagen, .audio, .video, .objetoIncrustado:
                objetoId = insertarRecurso(elemento, marco: destino,
                                           navegador: navegador)
                if objetoId != nil { resultado.recursos += 1 }
            case .tabla, .ecuacion, .grafica, .diagrama:
                objetoId = insertarBloque(elemento, marco: destino,
                                          navegador: navegador)
                if objetoId != nil { resultado.bloques += 1 }
            case .formaVectorial:
                objetoId = insertarVector(elemento, marco: destino,
                                          navegador: navegador)
                if objetoId != nil { resultado.vectores += 1 }
            case .desconocido:
                if elemento.hashRecurso != nil {
                    objetoId = insertarRecurso(elemento, marco: destino,
                                               navegador: navegador)
                    if objetoId != nil { resultado.recursos += 1 }
                } else if !elemento.texto.isEmpty {
                    objetoId = navegador.capaElementos.insertarTexto(
                        elemento.texto, marco: destino)
                    if objetoId != nil { resultado.textos += 1 }
                } else {
                    objetoId = nil; resultado.omitidos += 1
                }
            }
            guard let objetoId else { continue }
            RepositorioObjetosPPTXIPad.compartido.registrar(
                MetadatoObjetoPPTXIPad(
                    diapositivaId: diapositiva.id,
                    presentacion: diapositiva.tituloPresentacion,
                    numeroDiapositiva: diapositiva.numero,
                    idElementoPPTX: elemento.id,
                    idOffice: elemento.idOffice,
                    tipo: elemento.tipo,
                    hashFuente: diapositiva.hashFuente,
                    hipervinculo: elemento.hipervinculo,
                    pasoAnimacion: elemento.pasoAnimacion),
                objetoId: objetoId,
                documento: navegador.lienzo.sesion.documentoPersistenteId)
        }
    }

    private static func marcoElemento(_ normalizado: CGRect,
                                      dentro marco: CGRect) -> CGRect {
        let x = marco.minX + max(0, min(normalizado.minX, 1)) * marco.width
        let y = marco.minY + max(0, min(normalizado.minY, 1)) * marco.height
        let ancho = max(44, min(max(normalizado.width, 0.01), 1) * marco.width)
        let alto = max(32, min(max(normalizado.height, 0.01), 1) * marco.height)
        return CGRect(x: x, y: y, width: ancho, height: alto)
    }

    private static func insertarRecurso(
        _ elemento: ElementoPPTXIPad,
        marco: CGRect,
        navegador: VistaLienzoNavegable
    ) -> String? {
        guard let hash = elemento.hashRecurso,
              let url = RepositorioAdjuntos.compartido.url(hash: hash) else { return nil }
        let bytes = Int64((try? url.resourceValues(
            forKeys: [.fileSizeKey]).fileSize) ?? 0)
        let adjunto = AdjuntoImportado(
            hash: hash, url: url,
            nombreOriginal: elemento.nombreRecurso ?? elemento.nombre,
            bytes: bytes)
        let tipo: TipoMedio
        switch elemento.tipo {
        case .imagen: tipo = .imagen
        case .audio: tipo = .audio
        case .video: tipo = .video
        default: tipo = .documento
        }
        return navegador.capaElementos.insertarAdjunto(
            adjunto, tipo: tipo, en: marco.origin, tamano: marco.size)
    }

    private static func insertarBloque(
        _ elemento: ElementoPPTXIPad,
        marco: CGRect,
        navegador: VistaLienzoNavegable
    ) -> String? {
        let tipo: TipoBloqueLienzo
        switch elemento.tipo {
        case .tabla: tipo = .tabla
        case .ecuacion: tipo = .ecuacion
        case .grafica: tipo = .grafica
        default: tipo = .diagramaBloques
        }
        let id = navegador.capaBloques.insertar(
            tipo: tipo, en: marco.origin,
            titulo: elemento.nombre, contenido: elemento.texto)
        guard var bloque = navegador.capaBloques.bloques.first(
            where: { $0.id == id }) else { return nil }
        bloque.marco = marco
        bloque.fuenteElementoId = elemento.id.uuidString
        if let tabla = elemento.tabla {
            let filas = max(tabla.filas.count, 1)
            let columnas = max(tabla.cantidadColumnas, 1)
            var nativa = TablaLienzo(filas: filas, columnas: columnas)
            for fila in tabla.filas.indices {
                for columna in tabla.filas[fila].indices {
                    guard let indice = nativa.celdas.firstIndex(where: {
                        $0.fila == fila && $0.columna == columna
                    }) else { continue }
                    nativa.celdas[indice].texto = tabla.filas[fila][columna]
                }
            }
            bloque.tabla = nativa
        }
        navegador.capaBloques.actualizar(bloque)
        return id.uuidString
    }

    private static func insertarVector(
        _ elemento: ElementoPPTXIPad,
        marco: CGRect,
        navegador: VistaLienzoNavegable
    ) -> String? {
        let geometria = (elemento.geometria ?? "").lowercased()
        let tipo: TipoHerramientaTecnica
        if geometria.contains("ellipse") || geometria.contains("oval") {
            tipo = .elipse
        } else if geometria.contains("line") || geometria.contains("arrow") {
            tipo = .flecha
        } else if geometria.contains("rect") {
            tipo = .diagramaBloques
        } else {
            tipo = .bezier
        }
        let puntos: [CGPoint]
        if tipo == .bezier {
            puntos = [
                CGPoint(x: marco.minX, y: marco.midY),
                CGPoint(x: marco.minX + marco.width * 0.33, y: marco.minY),
                CGPoint(x: marco.minX + marco.width * 0.66, y: marco.maxY),
                CGPoint(x: marco.maxX, y: marco.midY),
            ]
        } else {
            puntos = [CGPoint(x: marco.minX, y: marco.minY),
                      CGPoint(x: marco.maxX, y: marco.maxY)]
        }
        var estilo = EstiloTecnico()
        estilo.colorARGB = elemento.colorTrazoARGB ?? 0xFF1C1C1E
        estilo.rellenoARGB = elemento.colorRellenoARGB
        estilo.grosor = elemento.grosorTrazo ?? 2
        let objeto = ObjetoTecnico(
            tipo: tipo, puntos: puntos, etiqueta: elemento.texto,
            estilo: estilo)
        navegador.capaTecnica.agregar(objeto)
        return objeto.id.uuidString
    }
}

final class PanelOpcionesImportacionPPTXIPad: UITableViewController {
    var alConfirmar: ((OpcionesImportacionPPTXIPad) -> Void)?
    private var opciones: OpcionesImportacionPPTXIPad
    private let fidelidad = [
        "Texto seleccionable",
        "Hipervínculos",
        "Imágenes originales",
        "Notas del profesor",
        "Comentarios",
        "Animaciones como pasos",
        "Extraer todos los recursos",
    ]

    init(opciones: OpcionesImportacionPPTXIPad = .completa) {
        self.opciones = opciones
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Importar PowerPoint"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel, target: self, action: #selector(cancelar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Elegir archivo", style: .done,
            target: self, action: #selector(confirmar))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return RepresentacionImportacionPPTXIPad.allCases.count
        case 1: return fidelidad.count
        default: return DestinoImportacionPPTXIPad.allCases.count
        }
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Representación", "Conservar del Open XML", "Después de importar"][section]
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        guard section == 0 else { return nil }
        return "El PPTX original siempre se conserva inmutable. La opción editable controla qué objetos puede separar dentro del lienzo."
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            let valor = RepresentacionImportacionPPTXIPad.allCases[indexPath.row]
            celda.textLabel?.text = valor.rawValue
            celda.accessoryType = opciones.representacion == valor ? .checkmark : .none
        case 1:
            celda.textLabel?.text = fidelidad[indexPath.row]
            celda.accessoryType = valorFidelidad(indexPath.row) ? .checkmark : .none
        default:
            let valor = DestinoImportacionPPTXIPad.allCases[indexPath.row]
            celda.textLabel?.text = valor.rawValue
            celda.accessoryType = opciones.destino == valor ? .checkmark : .none
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        switch indexPath.section {
        case 0:
            opciones.representacion = RepresentacionImportacionPPTXIPad.allCases[indexPath.row]
        case 1: alternarFidelidad(indexPath.row)
        default:
            opciones.destino = DestinoImportacionPPTXIPad.allCases[indexPath.row]
        }
        tableView.reloadData()
    }

    private func valorFidelidad(_ indice: Int) -> Bool {
        switch indice {
        case 0: return opciones.conservarTextoSeleccionable
        case 1: return opciones.conservarHipervinculos
        case 2: return opciones.conservarImagenesOriginales
        case 3: return opciones.importarNotasPresentador
        case 4: return opciones.importarComentarios
        case 5: return opciones.separarAnimaciones
        default: return opciones.extraerRecursos
        }
    }
    private func alternarFidelidad(_ indice: Int) {
        switch indice {
        case 0: opciones.conservarTextoSeleccionable.toggle()
        case 1: opciones.conservarHipervinculos.toggle()
        case 2: opciones.conservarImagenesOriginales.toggle()
        case 3: opciones.importarNotasPresentador.toggle()
        case 4: opciones.importarComentarios.toggle()
        case 5: opciones.separarAnimaciones.toggle()
        default: opciones.extraerRecursos.toggle()
        }
    }
    @objc private func confirmar() {
        let elegidas = opciones
        let completado = alConfirmar
        dismiss(animated: true) { completado?(elegidas) }
    }
    @objc private func cancelar() { dismiss(animated: true) }
}

final class PanelDetalleDiapositivaPPTXIPad: UITableViewController {
    private let diapositiva: DiapositivaLocalIPad
    private var vinculos: [HipervinculoPPTXIPad] {
        Array(Set((diapositiva.elementosPPTX ?? []).compactMap(\.hipervinculo)))
            .sorted { $0.destino < $1.destino }
    }
    private var recursos: [ElementoPPTXIPad] {
        (diapositiva.elementosPPTX ?? []).filter { $0.hashRecurso != nil }
    }
    private var recursosPresentacion: [RecursoPPTXIPad] {
        RepositorioDiapositivasIPad.compartido.presentacion(
            id: diapositiva.presentacionId)?.recursosPPTX ?? []
    }

    init(_ diapositiva: DiapositivaLocalIPad) {
        self.diapositiva = diapositiva
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Diapositiva \(diapositiva.numero)"
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 6 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 1
        case 1: return diapositiva.notasPresentador?.isEmpty == false ? 1 : 0
        case 2: return diapositiva.comentariosPPTX?.count ?? 0
        case 3: return vinculos.count
        case 4: return recursos.count
        default: return recursosPresentacion.count
        }
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Contenido Open XML", "Notas del profesor", "Comentarios",
         "Hipervínculos", "Recursos vinculados a esta diapositiva",
         "Todos los recursos extraídos de la presentación"][section]
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.detailTextLabel?.numberOfLines = 0
        switch indexPath.section {
        case 0:
            let elementos = diapositiva.elementosPPTX ?? []
            celda.textLabel?.text = "\(elementos.count) elementos separables"
            celda.detailTextLabel?.text = elementos.reduce(into: [String: Int]()) {
                $0[$1.tipo.nombre, default: 0] += 1
            }.sorted { $0.key < $1.key }.map { "\($0.key): \($0.value)" }
                .joined(separator: " · ")
        case 1:
            celda.textLabel?.text = diapositiva.notasPresentador
        case 2:
            let comentario = diapositiva.comentariosPPTX?[indexPath.row]
            celda.textLabel?.text = comentario?.autor ?? "Comentario"
            celda.detailTextLabel?.text = comentario?.texto
        case 3:
            let vinculo = vinculos[indexPath.row]
            celda.textLabel?.text = vinculo.texto.isEmpty ? vinculo.destino : vinculo.texto
            celda.detailTextLabel?.text = vinculo.destino
            celda.accessoryType = .disclosureIndicator
        case 4:
            let recurso = recursos[indexPath.row]
            celda.textLabel?.text = recurso.nombreRecurso ?? recurso.nombre
            celda.detailTextLabel?.text = recurso.tipo.nombre
            celda.accessoryType = .detailButton
        default:
            let recurso = recursosPresentacion[indexPath.row]
            celda.textLabel?.text = recurso.nombre
            celda.detailTextLabel?.text = "\(recurso.tipo.nombre) · \(ByteCountFormatter.string(fromByteCount: recurso.bytes, countStyle: .file)) · \(recurso.rutaOpenXML)"
            celda.accessoryType = .detailButton
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard indexPath.section == 3 else { return }
        let destino = vinculos[indexPath.row].destino
        if let url = URL(string: destino),
           ["http", "https", "mailto"].contains(url.scheme?.lowercased() ?? "") {
            UIApplication.shared.open(url)
        } else {
            UIPasteboard.general.string = destino
            let aviso = UIAlertController(
                title: "Vínculo interno conservado",
                message: "El destino Open XML se copió: \(destino)",
                preferredStyle: .alert)
            aviso.addAction(UIAlertAction(title: "Aceptar", style: .default))
            present(aviso, animated: true)
        }
    }
    override func tableView(_ tableView: UITableView,
                            accessoryButtonTappedForRowWith indexPath: IndexPath) {
        guard indexPath.section >= 4 else { return }
        let hash: String?
        if indexPath.section == 4 { hash = recursos[indexPath.row].hashRecurso }
        else { hash = recursosPresentacion[indexPath.row].hash }
        guard let hash,
              let url = RepositorioAdjuntos.compartido.url(hash: hash) else { return }
        let compartir = UIActivityViewController(activityItems: [url],
                                                  applicationActivities: nil)
        compartir.popoverPresentationController?.sourceView = tableView.cellForRow(at: indexPath)
        present(compartir, animated: true)
    }
}
