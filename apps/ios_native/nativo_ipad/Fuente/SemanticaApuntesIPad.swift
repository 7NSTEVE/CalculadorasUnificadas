import Foundation
import UIKit

enum TipoMarcacionSemanticaIPad: String, Codable, CaseIterable {
    case formula
    case definicion
    case importante
    case examen
    case duda
    case ejemplo
    case entendido
    case encabezado
    case pregunta

    var nombre: String {
        switch self {
        case .formula: return "Fórmula"
        case .definicion: return "Definición"
        case .importante: return "Importante"
        case .examen: return "Probable examen"
        case .duda: return "Duda"
        case .ejemplo: return "Ejemplo"
        case .entendido: return "Entendido"
        case .encabezado: return "Encabezado"
        case .pregunta: return "Pregunta"
        }
    }

    var simbolo: String {
        switch self {
        case .formula: return "function"
        case .definicion: return "text.book.closed"
        case .importante: return "star.fill"
        case .examen: return "exclamationmark.triangle"
        case .duda: return "questionmark.circle"
        case .ejemplo: return "lightbulb"
        case .entendido: return "checkmark.circle"
        case .encabezado: return "textformat.size.larger"
        case .pregunta: return "questionmark.bubble"
        }
    }
}

struct MarcacionSemanticaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var documentoId: UInt64
    var clavesElemento: [String]
    var tipo: TipoMarcacionSemanticaIPad
    var titulo: String
    var region: CGRect?
    var fecha = Date()
    var cursoId: UUID?
}

struct ReferenciaCruzadaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var documentoOrigen: UInt64
    var clavesOrigen: [String]
    var tituloOrigen: String
    var destino: ReferenciaFuente
    var fecha = Date()
}

private struct EstadoSemanticaIPad: Codable {
    var formato = 1
    var marcaciones: [MarcacionSemanticaIPad] = []
    var referencias: [ReferenciaCruzadaIPad] = []
}

/// Sidecar abierto y local para significado, outline y enlaces. Mantiene la
/// tinta intacta y usa sus IDs colaborativos estables.
final class RepositorioSemanticaIPad {
    static let compartido = RepositorioSemanticaIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoSemanticaIPad

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Semantica",
                                                     isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("semantica-ipad.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        estado = (try? Data(contentsOf: url)).flatMap {
            try? decoder.decode(EstadoSemanticaIPad.self, from: $0)
        } ?? EstadoSemanticaIPad()
    }

    @discardableResult
    func marcar(documento: UInt64, claves: [String],
                tipo: TipoMarcacionSemanticaIPad, titulo: String,
                region: CGRect?, cursoId: UUID?) -> MarcacionSemanticaIPad? {
        let unicas = Array(Set(claves)).sorted()
        guard !unicas.isEmpty else { return nil }
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.marcaciones.removeAll {
            $0.documentoId == documento && $0.tipo == tipo
                && Set($0.clavesElemento) == Set(unicas)
        }
        let marca = MarcacionSemanticaIPad(
            documentoId: documento, clavesElemento: unicas, tipo: tipo,
            titulo: titulo, region: region, cursoId: cursoId)
        estado.marcaciones.append(marca); persistir()
        return marca
    }

    @discardableResult
    func vincular(documento: UInt64, claves: [String], titulo: String,
                  destino: ReferenciaFuente) -> ReferenciaCruzadaIPad? {
        let unicas = Array(Set(claves)).sorted()
        guard !unicas.isEmpty else { return nil }
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard !estado.referencias.contains(where: {
            $0.documentoOrigen == documento
                && Set($0.clavesOrigen) == Set(unicas)
                && $0.destino.id == destino.id
        }) else { return nil }
        let enlace = ReferenciaCruzadaIPad(
            documentoOrigen: documento, clavesOrigen: unicas,
            tituloOrigen: titulo, destino: destino)
        estado.referencias.append(enlace); persistir()
        return enlace
    }

    func marcaciones(documento: UInt64) -> [MarcacionSemanticaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.marcaciones.filter { $0.documentoId == documento }
            .sorted { a, b in
                if a.region?.minY != b.region?.minY {
                    return (a.region?.minY ?? .greatestFiniteMagnitude)
                        < (b.region?.minY ?? .greatestFiniteMagnitude)
                }
                return a.fecha < b.fecha
            }
    }

    func referencias(documento: UInt64) -> [ReferenciaCruzadaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.referencias.filter { $0.documentoOrigen == documento }
            .sorted { $0.fecha > $1.fecha }
    }

    /// Backlinks entrantes hacia cualquier elemento estable del documento.
    /// Esto permite mostrar quién enlaza al cuaderno sin necesitar que cada
    /// destino se haya convertido antes en una marcación semántica separada.
    func backlinks(documento: UInt64) -> [ReferenciaCruzadaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.referencias.filter { $0.destino.documentoId == documento }
            .sorted { $0.fecha > $1.fecha }
    }

    func backlinks(hacia fuente: ReferenciaFuente) -> [ReferenciaCruzadaIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return estado.referencias.filter {
            $0.destino.id == fuente.id
                || ($0.destino.elementoId != nil
                    && $0.destino.elementoId == fuente.elementoId
                    && $0.destino.documentoId == fuente.documentoId)
        }
    }

    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64)
        throws -> (marcaciones: Int, referencias: Int) {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }
        guard !estado.marcaciones.contains(where: { $0.documentoId == destino }),
              !estado.referencias.contains(where: { $0.documentoOrigen == destino }) else {
            throw CocoaError(.fileWriteFileExists)
        }

        let marcasFuente = estado.marcaciones.filter { $0.documentoId == origen }
        let referenciasFuente = estado.referencias.filter {
            $0.documentoOrigen == origen
        }

        let marcasNuevas = marcasFuente.map { original -> MarcacionSemanticaIPad in
            var copia = original
            copia.id = UUID()
            copia.documentoId = destino
            return copia
        }
        let referenciasNuevas = referenciasFuente.map {
            original -> ReferenciaCruzadaIPad in
            var copia = original
            copia.id = UUID()
            copia.documentoOrigen = destino
            if copia.destino.documentoId == origen {
                copia.destino.id = UUID()
                copia.destino.documentoId = destino
            }
            return copia
        }

        var nuevoEstado = estado
        nuevoEstado.marcaciones.append(contentsOf: marcasNuevas)
        nuevoEstado.referencias.append(contentsOf: referenciasNuevas)
        try persistir(nuevoEstado)
        estado = nuevoEstado
        return (marcasNuevas.count, referenciasNuevas.count)
    }

    func eliminarMarcacion(_ id: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.marcaciones.removeAll { $0.id == id }; persistir()
    }

    func eliminarReferencia(_ id: UUID) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        estado.referencias.removeAll { $0.id == id }; persistir()
    }

    private func persistir(_ valor: EstadoSemanticaIPad) throws {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }

    private func persistir() {
        try? persistir(estado)
    }
}

extension LienzoCuadernU {
    var clavesSeleccionActual: [String] {
        RepositorioMetadatosSeleccionIPad.compartido.claves(
            de: seleccionActual, sesion: sesion)
    }
}

final class PanelSemanticaDocumentoIPad: UITableViewController {
    var alAbrirRegion: ((CGRect) -> Void)?
    var alAbrirFuente: ((ReferenciaFuente) -> Void)?

    private let documento: UInt64
    private var marcaciones: [MarcacionSemanticaIPad] = []
    private var referencias: [ReferenciaCruzadaIPad] = []
    private var backlinks: [ReferenciaCruzadaIPad] = []

    init(documento: UInt64) {
        self.documento = documento
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Outline, links y backlinks"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        refrescar()
    }

    private func refrescar() {
        marcaciones = RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento)
        referencias = RepositorioSemanticaIPad.compartido
            .referencias(documento: documento)
        backlinks = RepositorioSemanticaIPad.compartido
            .backlinks(documento: documento)
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0: return "Outline semántico"
        case 1: return "Referencias salientes"
        default: return "Backlinks entrantes"
        }
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return max(marcaciones.count, 1)
        case 1: return max(referencias.count, 1)
        default: return max(backlinks.count, 1)
        }
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            guard !marcaciones.isEmpty else {
                celda.textLabel?.text = "Sin marcaciones semánticas"
                celda.selectionStyle = .none; return celda
            }
            let marca = marcaciones[indexPath.row]
            celda.textLabel?.text = marca.titulo
            celda.detailTextLabel?.text = marca.tipo.nombre
            celda.imageView?.image = UIImage(systemName: marca.tipo.simbolo)
            celda.accessoryType = marca.region == nil ? .none : .disclosureIndicator
        case 1:
            guard !referencias.isEmpty else {
                celda.textLabel?.text = "Sin referencias salientes"
                celda.selectionStyle = .none; return celda
            }
            let referencia = referencias[indexPath.row]
            celda.textLabel?.text = referencia.tituloOrigen
            celda.detailTextLabel?.text = "→ \(referencia.destino.titulo)"
            celda.imageView?.image = UIImage(systemName: "arrow.triangle.branch")
            celda.accessoryType = .disclosureIndicator
        default:
            guard !backlinks.isEmpty else {
                celda.textLabel?.text = "Sin backlinks entrantes"
                celda.selectionStyle = .none; return celda
            }
            let enlace = backlinks[indexPath.row]
            celda.textLabel?.text = enlace.tituloOrigen
            celda.detailTextLabel?.text = enlace.documentoOrigen == documento
                ? "← desde este documento"
                : "← desde otro documento"
            celda.imageView?.image = UIImage(systemName: "arrow.uturn.backward")
            celda.accessoryType = .disclosureIndicator
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch indexPath.section {
        case 0:
            guard marcaciones.indices.contains(indexPath.row),
                  let region = marcaciones[indexPath.row].region else { return }
            alAbrirRegion?(region)
        case 1:
            guard referencias.indices.contains(indexPath.row) else { return }
            let fuente = referencias[indexPath.row].destino
            if let destino = fuente.documentoId, destino != documento {
                _ = RouterNavegacionDocumentosIPad.abrir(
                    fuente, desdeDocumento: documento, presentador: self)
            } else {
                alAbrirFuente?(fuente)
            }
        default:
            guard backlinks.indices.contains(indexPath.row) else { return }
            let enlace = backlinks[indexPath.row]
            let fuente = ReferenciaFuente(
                tipo: .apuntePropio,
                titulo: enlace.tituloOrigen,
                documentoId: enlace.documentoOrigen,
                elementoId: enlace.clavesOrigen.first)
            if enlace.documentoOrigen != documento {
                _ = RouterNavegacionDocumentosIPad.abrir(
                    fuente, desdeDocumento: documento, presentador: self)
            } else {
                alAbrirFuente?(fuente)
            }
        }
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard indexPath.section != 0
                || marcaciones.indices.contains(indexPath.row) else { return nil }
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") {
            [weak self] _, _, terminar in
            guard let self else { terminar(false); return }
            switch indexPath.section {
            case 0:
                guard self.marcaciones.indices.contains(indexPath.row) else {
                    terminar(false); return
                }
                RepositorioSemanticaIPad.compartido.eliminarMarcacion(
                    self.marcaciones[indexPath.row].id)
            case 1:
                guard self.referencias.indices.contains(indexPath.row) else {
                    terminar(false); return
                }
                RepositorioSemanticaIPad.compartido.eliminarReferencia(
                    self.referencias[indexPath.row].id)
            default:
                guard self.backlinks.indices.contains(indexPath.row) else {
                    terminar(false); return
                }
                RepositorioSemanticaIPad.compartido.eliminarReferencia(
                    self.backlinks[indexPath.row].id)
            }
            self.refrescar(); terminar(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar])
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
