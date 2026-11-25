import UIKit

enum CategoriaCapa: String, Codable, CaseIterable {
    case tinta = "Tinta"
    case textos = "Texto"
    case medios = "Fotografías y adjuntos"
    case bloques = "Bloques y simulación"
    case tecnico = "Dibujo técnico y correcciones"
}

struct CapaDocumento: Codable, Identifiable, Equatable {
    var id = UUID(); var categoria: CategoriaCapa; var nombre: String
    var visible = true; var bloqueada = false; var exportar = true
    var opacidad: CGFloat = 1; var colorARGB: UInt32
}

struct EstadoCapasDocumento: Codable, Equatable {
    var capas: [CapaDocumento] = [
        CapaDocumento(categoria:.tinta,nombre:"Tinta",colorARGB:0xFF1A1A1A),
        CapaDocumento(categoria:.textos,nombre:"Texto",colorARGB:0xFF8E8E93),
        CapaDocumento(categoria:.medios,nombre:"Fotos / PDFs",colorARGB:0xFFFF9F0A),
        CapaDocumento(categoria:.bloques,nombre:"Simulación / respuestas",colorARGB:0xFF5856D6),
        CapaDocumento(categoria:.tecnico,nombre:"Correcciones técnicas",colorARGB:0xFF0A84FF)
    ]
}

final class RepositorioCapas {
    static let compartido = RepositorioCapas()
    private let url: URL
    private let bloqueo = NSRecursiveLock()
    private var documentos: [String: EstadoCapasDocumento] = [:]

    private init() {
        let base = FileManager.default.urls(for: .documentDirectory,
                                            in: .userDomainMask).first!
        let carpeta = base.appendingPathComponent("Capas", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("capas.json")
        if let datos = try? Data(contentsOf: url),
           let valor = try? JSONDecoder().decode(
            [String: EstadoCapasDocumento].self, from: datos) {
            documentos = valor
        }
    }

    func estado(_ documento: UInt64) -> EstadoCapasDocumento {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return documentos[String(documento)] ?? EstadoCapasDocumento()
    }

    @discardableResult
    func guardar(_ estado: EstadoCapasDocumento, documento: UInt64) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        var siguiente = documentos
        siguiente[String(documento)] = estado
        do {
            try persistir(siguiente)
            documentos = siguiente
            return true
        } catch {
            return false
        }
    }

    /// Crea un sidecar de capas independiente. Se regeneran los UUID porque la
    /// copia puede editar o borrar sus capas sin compartir identidad con el
    /// documento fuente. La tabla en memoria solo cambia después de la
    /// escritura atómica confirmada.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let claveDestino = String(destino)
        guard documentos[claveDestino] == nil else {
            throw CocoaError(.fileWriteFileExists)
        }
        var copia = documentos[String(origen)] ?? EstadoCapasDocumento()
        copia.capas = copia.capas.map { original in
            var valor = original
            valor.id = UUID()
            return valor
        }
        var siguiente = documentos
        siguiente[claveDestino] = copia
        try persistir(siguiente)
        documentos = siguiente
        return copia.capas.count
    }

    /// Elimina solo el estado del documento indicado. Se usa como compensación
    /// si una duplicación multirepositorio falla después de haber escrito capas.
    /// La memoria solo cambia cuando el archivo global confirma el rollback.
    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let clave = String(documento)
        guard documentos[clave] != nil else { return }
        var siguiente = documentos
        siguiente.removeValue(forKey: clave)
        try persistir(siguiente)
        documentos = siguiente
    }

    private func persistir(_ valor: [String: EstadoCapasDocumento]) throws {
        let datos = try JSONEncoder().encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

extension VistaLienzoNavegable {
    func aplicarCapas(_ estado:EstadoCapasDocumento) {
        func capa(_ categoria:CategoriaCapa)->CapaDocumento?{estado.capas.first{$0.categoria==categoria}}
        if let c=capa(.tinta){lienzo.mostrarTinta=c.visible;lienzo.isUserInteractionEnabled = !c.bloqueada}
        if let c=capa(.textos){capaElementos.mostrarTextos=c.visible;capaElementos.bloquearTextos=c.bloqueada;capaElementos.opacidadTextos=c.opacidad}
        if let c=capa(.medios){capaElementos.mostrarMedios=c.visible;capaElementos.bloquearMedios=c.bloqueada;capaElementos.opacidadMedios=c.opacidad}
        if let c=capa(.bloques){capaBloques.isHidden = !c.visible;capaBloques.isUserInteractionEnabled = !c.bloqueada;capaBloques.alpha=c.opacidad}
        if let c=capa(.tecnico){capaTecnica.isHidden = !c.visible;capaTecnica.isUserInteractionEnabled = !c.bloqueada && capaTecnica.herramientaActiva != nil;capaTecnica.alpha=c.opacidad}
    }
}

final class PanelCapas: UITableViewController {
    private let documentoId: UInt64
    private weak var navegador: VistaLienzoNavegable?
    private var estado: EstadoCapasDocumento
    
    init(documentoId: UInt64, navegador: VistaLienzoNavegable) {
        self.documentoId = documentoId
        self.navegador = navegador
        estado = RepositorioCapas.compartido.estado(documentoId)
        super.init(style: .insetGrouped)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Capas"
        navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Fondos", style: .plain, target: self, action: #selector(abrirFondos))
        navegador?.aplicarCapas(estado)
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        estado.capas.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let capa = estado.capas[indexPath.row]
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        c.textLabel?.text = capa.nombre
        c.detailTextLabel?.text = "\(capa.bloqueada ? "Bloqueada · " : "")\(capa.exportar ? "se exporta" : "no se exporta") · \(Int(capa.opacidad * 100))%"
        c.imageView?.image = circulo(UIColor(argb: capa.colorARGB))
        c.accessoryType = capa.visible ? .checkmark : .none
        return c
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        estado.capas[indexPath.row].visible.toggle()
        guardar()
    }
    
    override func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration? {
        let bloquear = UIContextualAction(style: .normal, title: estado.capas[indexPath.row].bloqueada ? "Desbloquear" : "Bloquear") { [weak self] _, _, fin in
            self?.estado.capas[indexPath.row].bloqueada.toggle()
            self?.guardar()
            fin(true)
        }
        bloquear.backgroundColor = .systemOrange
        
        let exportar = UIContextualAction(style: .normal, title: "Exportar") { [weak self] _, _, fin in
            self?.estado.capas[indexPath.row].exportar.toggle()
            self?.guardar()
            fin(true)
        }
        exportar.backgroundColor = .systemBlue
        
        return UISwipeActionsConfiguration(actions: [bloquear, exportar])
    }
    
    override func tableView(_ tableView: UITableView, leadingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration? {
        let opacidad = UIContextualAction(style: .normal, title: "Opacidad") { [weak self] _, _, fin in
            self?.pedirOpacidad(indexPath.row)
            fin(true)
        }
        opacidad.backgroundColor = .systemPurple
        return UISwipeActionsConfiguration(actions: [opacidad])
    }
    
    private func pedirOpacidad(_ indice: Int) {
        let a = UIAlertController(title: "Opacidad 0–100", message: nil, preferredStyle: .alert)
        a.addTextField {
            $0.text = "\(Int(self.estado.capas[indice].opacidad * 100))"
            $0.keyboardType = .numberPad
        }
        a.addAction(UIAlertAction(title: "Aplicar", style: .default) { [weak self] _ in
            guard let self, let t = a.textFields?.first?.text, let v = Double(t) else { return }
            self.estado.capas[indice].opacidad = CGFloat(min(max(v, 0), 100) / 100)
            self.guardar()
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
    }
    
    private func guardar() {
        RepositorioCapas.compartido.guardar(estado, documento: documentoId)
        navegador?.aplicarCapas(estado)
        tableView.reloadData()
    }
    
    private func circulo(_ color: UIColor) -> UIImage {
        UIGraphicsImageRenderer(size: CGSize(width: 20, height: 20)).image { ctx in
            color.setFill()
            ctx.cgContext.fillEllipse(in: CGRect(x: 2, y: 2, width: 16, height: 16))
        }
    }
    
    @objc private func abrirFondos() {
        let panel = PanelFondosObjetoIPad(documento: documentoId, sesion: navegador?.lienzo.sesion)
        panel.alCambiar = { [weak self] in self?.navegador?.recargarElementos() }
        navigationController?.pushViewController(panel, animated: true)
    }
    
    @objc private func cerrar() {
        dismiss(animated: true)
    }
}

struct InstantaneaDocumento: Codable, Identifiable, Equatable {
    var id = UUID()
    var documentoId: UInt64
    var nombre: String
    var fecha = Date()
    var archivoTinta: String
    var bloques: [BloqueLienzo]
    var tecnicos: [ObjetoTecnico]
    var capas: EstadoCapasDocumento
    var motivo: String
    var inmutable = true
}

final class GestorVersionesDocumento {
    typealias EscritorManifiesto = (Data, URL) throws -> Void

    static let compartido = GestorVersionesDocumento()
    private let base: URL
    private let manifiesto: URL
    private let archivos: FileManager
    private let escribirManifiesto: EscritorManifiesto
    private let bloqueo = NSRecursiveLock()
    private var versiones: [InstantaneaDocumento] = []

    init(
        base: URL? = nil,
        archivos: FileManager = .default,
        escribirManifiesto: @escaping EscritorManifiesto = { datos, url in
            try datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    ) {
        self.archivos = archivos
        self.escribirManifiesto = escribirManifiesto
        let raiz = base ?? archivos.urls(
            for: .documentDirectory, in: .userDomainMask).first!
            .appendingPathComponent("Versiones", isDirectory: true)
        self.base = raiz
        self.manifiesto = raiz.appendingPathComponent("versiones.json")
        try? archivos.createDirectory(at: raiz, withIntermediateDirectories: true)

        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let existeManifiesto = archivos.fileExists(atPath: manifiesto.path)
        var manifiestoConfiable = !existeManifiesto
        if existeManifiesto,
           let datos = try? Data(contentsOf: manifiesto),
           let leidas = try? decoder.decode([InstantaneaDocumento].self, from: datos) {
            versiones = leidas
            manifiestoConfiable = true
        }
        // Si el índice está corrupto preservamos todos los snapshots: borrar en
        // ese estado convertiría una corrupción recuperable en pérdida de datos.
        if manifiestoConfiable {
            limpiarInstantaneasHuerfanas()
        }
    }

    func listar(_ documento: UInt64) -> [InstantaneaDocumento] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return versiones.filter { $0.documentoId == documento }
            .sorted { $0.fecha > $1.fecha }
    }

    func crear(
        nombre: String,
        motivo: String,
        sesion: Sesion,
        bloques: [BloqueLienzo],
        tecnicos: [ObjetoTecnico],
        capas: EstadoCapasDocumento
    ) -> InstantaneaDocumento? {
        guard let datos = sesion.guardar() else { return nil }
        return crear(
            documentoId: sesion.documentoPersistenteId,
            nombre: nombre,
            motivo: motivo,
            datosTinta: datos,
            bloques: bloques,
            tecnicos: tecnicos,
            capas: capas)
    }

    /// Frontera durable usada también por las regresiones de almacenamiento.
    /// Publica la caché sólo después de que snapshot + manifiesto estén escritos.
    @discardableResult
    func crear(
        documentoId: UInt64,
        nombre: String,
        motivo: String,
        datosTinta: Data,
        bloques: [BloqueLienzo],
        tecnicos: [ObjetoTecnico],
        capas: EstadoCapasDocumento
    ) -> InstantaneaDocumento? {
        guard !datosTinta.isEmpty else { return nil }
        bloqueo.lock(); defer { bloqueo.unlock() }

        let id = UUID()
        let archivo = "\(id.uuidString).tinta"
        let url = base.appendingPathComponent(archivo)
        let nombreLimpio = nombre.trimmingCharacters(in: .whitespacesAndNewlines)
        let motivoLimpio = motivo.trimmingCharacters(in: .whitespacesAndNewlines)
        let version = InstantaneaDocumento(
            id: id,
            documentoId: documentoId,
            nombre: nombreLimpio.isEmpty ? "Versión" : nombreLimpio,
            archivoTinta: archivo,
            bloques: bloques,
            tecnicos: tecnicos,
            capas: capas,
            motivo: motivoLimpio.isEmpty ? "manual" : motivoLimpio)
        var siguientes = versiones
        siguientes.append(version)

        do {
            try datosTinta.write(
                to: url, options: [.atomic, .completeFileProtection])
            do {
                try persistir(siguientes)
            } catch {
                // Compensación inmediata: el índice antiguo sigue siendo la
                // fuente de verdad y el snapshot recién escrito no debe quedar
                // ocupando espacio de forma invisible.
                try? archivos.removeItem(at: url)
                throw error
            }
            versiones = siguientes
            return version
        } catch {
            return nil
        }
    }

    func restaurar(
        _ version: InstantaneaDocumento,
        sesion: Sesion,
        bloques: CapaBloquesLienzo,
        tecnicos: CapaHerramientasTecnicas,
        navegador: VistaLienzoNavegable
    ) -> Bool {
        guard version.documentoId == sesion.documentoPersistenteId,
              let tintaAnterior = sesion.guardar() else {
            return false
        }
        let bloquesAnteriores = bloques.bloques
        let tecnicosAnteriores = tecnicos.objetos
        let capasAnteriores = RepositorioCapas.compartido.estado(
            version.documentoId)
        let url = base.appendingPathComponent(version.archivoTinta)
        guard let datos = try? Data(contentsOf: url), sesion.cargar(datos) else {
            return false
        }
        bloques.reemplazar(version.bloques)
        tecnicos.objetos = version.tecnicos
        guard RepositorioCapas.compartido.guardar(
            version.capas, documento: version.documentoId) else {
            // La tinta, los bloques y los objetos técnicos ya estaban en RAM.
            // Si la última frontera durable (capas) falla, restauramos todo lo
            // anterior antes de devolver fallo para no dejar una vista híbrida.
            _ = sesion.cargar(tintaAnterior)
            bloques.reemplazar(bloquesAnteriores)
            tecnicos.objetos = tecnicosAnteriores
            navegador.aplicarCapas(capasAnteriores)
            navegador.sincronizarTamanoDocumento(conservarCentro: true)
            navegador.lienzo.rehornearYDibujar()
            navegador.recargarElementos()
            return false
        }
        navegador.aplicarCapas(version.capas)
        navegador.sincronizarTamanoDocumento(conservarCentro: true)
        navegador.lienzo.rehornearYDibujar()
        navegador.recargarElementos()
        return true
    }

    private func persistir(_ valores: [InstantaneaDocumento]) throws {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        let datos = try encoder.encode(valores)
        try escribirManifiesto(datos, manifiesto)
    }

    private func limpiarInstantaneasHuerfanas() {
        let referenciadas = Set(versiones.map(\.archivoTinta))
        guard let urls = try? archivos.contentsOfDirectory(
            at: base,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]) else { return }
        for url in urls where url.pathExtension.lowercased() == "tinta"
            && !referenciadas.contains(url.lastPathComponent) {
            try? archivos.removeItem(at: url)
        }
    }
}

final class PanelVersiones: UITableViewController {
    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private var versiones: [InstantaneaDocumento] = []
    
    init(sesion: Sesion, navegador: VistaLienzoNavegable) {
        self.sesion = sesion
        self.navegador = navegador
        super.init(style: .insetGrouped)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) no está implementado")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Versiones y antes/después"
        navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Instantánea", style: .done, target: self, action: #selector(crear))
        recargar()
    }
    
    private func recargar() {
        versiones = GestorVersionesDocumento.compartido.listar(
            sesion.documentoPersistenteId)
        tableView.reloadData()
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        versiones.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let v = versiones[indexPath.row]
        let c = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        c.textLabel?.text = v.nombre
        c.detailTextLabel?.text = "\(v.fecha.formatted(date: .abbreviated, time: .shortened)) · \(v.motivo) · original inmutable"
        c.accessoryType = .disclosureIndicator
        return c
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        let v = versiones[indexPath.row]
        let a = UIAlertController(title: v.nombre, message: "La restauración crea primero una instantánea de seguridad del estado actual. La versión histórica no se modifica.", preferredStyle: .actionSheet)
        a.addAction(UIAlertAction(title: "Restaurar", style: .destructive) { [weak self] _ in self?.restaurar(v) })
        a.addAction(UIAlertAction(title: "Copiar resumen antes/después", style: .default) { _ in UIPasteboard.general.string = "Versión \(v.nombre) — \(v.fecha.formatted())\nBloques: \(v.bloques.count), objetos técnicos: \(v.tecnicos.count)" })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        a.popoverPresentationController?.sourceView = view
        present(a, animated: true)
    }
    
    @objc private func crear() {
        let a = UIAlertController(title: "Nueva instantánea", message: nil, preferredStyle: .alert)
        a.addTextField { $0.placeholder = "nombre" }
        a.addTextField { $0.placeholder = "motivo: antes de resolver, corrección…" }
        a.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self, let n = self.navegador else { return }
            _ = GestorVersionesDocumento.compartido.crear(
                nombre: a.textFields?[0].text ?? "Versión",
                motivo: a.textFields?[1].text ?? "manual",
                sesion: self.sesion, bloques: n.capaBloques.bloques,
                tecnicos: n.capaTecnica.objetos,
                capas: RepositorioCapas.compartido.estado(
                    self.sesion.documentoPersistenteId))
            self.recargar()
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(a, animated: true)
    }
    
    private func restaurar(_ v: InstantaneaDocumento) {
        guard let n = navegador else { return }
        _ = GestorVersionesDocumento.compartido.crear(
            nombre: "Antes de restaurar \(v.nombre)",
            motivo: "respaldo automático", sesion: sesion,
            bloques: n.capaBloques.bloques, tecnicos: n.capaTecnica.objetos,
            capas: RepositorioCapas.compartido.estado(
                sesion.documentoPersistenteId))
        let ok = GestorVersionesDocumento.compartido.restaurar(
            v, sesion: sesion, bloques: n.capaBloques,
            tecnicos: n.capaTecnica, navegador: n)
        let a = UIAlertController(
            title: ok ? "Versión restaurada" : "No se restauró",
            message: ok
                ? "Se conservó una instantánea del estado anterior."
                : "El archivo de la versión no está disponible o no es válido.",
            preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(a, animated: true)
    }
    
    @objc private func cerrar() {
        dismiss(animated: true)
    }
}
