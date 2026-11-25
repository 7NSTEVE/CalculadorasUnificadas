import CoreImage
import CoreImage.CIFilterBuiltins
import Foundation
import PDFKit
import QuickLookThumbnailing
import UIKit

enum RecolorFondoObjetoIPad: String, Codable, CaseIterable {
    case original
    case escalaGrises
    case sepia
    case azulTecnico
    case altoContraste

    var nombre: String {
        switch self {
        case .original: return "Original"
        case .escalaGrises: return "Escala de grises"
        case .sepia: return "Sepia"
        case .azulTecnico: return "Azul técnico"
        case .altoContraste: return "Alto contraste"
        }
    }
}

struct AjustesFondoObjetoIPad: Codable, Equatable {
    var esFondo = false
    var bloqueado = false
    var oculto = false
    var opacidad: CGFloat = 1
    var contraste: Float = 1
    var escala: CGFloat = 1
    var recolor: RecolorFondoObjetoIPad = .original
}

struct RegistroFondoObjetoIPad: Identifiable {
    var id: String
    var documento: UInt64
    var nombre: String
    var ajustes: AjustesFondoObjetoIPad
}

private struct ArchivoFondosObjetoIPad: Codable {
    var formato = 1
    var valores: [String: AjustesFondoObjetoIPad] = [:]
    var nombres: [String: String] = [:]
}

final class RepositorioFondosObjetoIPad {
    static let compartido = RepositorioFondosObjetoIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoFondosObjetoIPad

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/FondosObjeto",
                                                     isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("fondos-objeto.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ArchivoFondosObjetoIPad.self, from: $0)
        } ?? ArchivoFondosObjetoIPad()
    }

    private func clave(documento: UInt64, elemento: String) -> String {
        "\(documento):\(elemento)"
    }

    func ajustes(documento: UInt64, elemento: String) -> AjustesFondoObjetoIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.valores[clave(documento: documento, elemento: elemento)]
            ?? AjustesFondoObjetoIPad()
    }

    @discardableResult
    func guardar(_ ajustes: AjustesFondoObjetoIPad, documento: UInt64,
                 elemento: String, nombre: String) -> Bool {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let id = clave(documento: documento, elemento: elemento)
        var siguiente = archivo
        siguiente.valores[id] = ajustes
        siguiente.nombres[id] = nombre
        do {
            try persistir(siguiente)
            archivo = siguiente
            return true
        } catch {
            return false
        }
    }

    func registros(documento: UInt64) -> [RegistroFondoObjetoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let prefijo = "\(documento):"
        return archivo.valores.compactMap { clave, ajustes in
            guard clave.hasPrefix(prefijo), ajustes.esFondo else { return nil }
            return RegistroFondoObjetoIPad(
                id: String(clave.dropFirst(prefijo.count)), documento: documento,
                nombre: archivo.nombres[clave] ?? "Printout",
                ajustes: ajustes)
        }.sorted { $0.nombre.localizedCaseInsensitiveCompare($1.nombre)
            == .orderedAscending }
    }

    /// Sidecars del documento indexados por el ID real del elemento. Se usa
    /// al exportar el formato abierto y evita que la apariencia del printout
    /// dependa de una preferencia que quede fuera del paquete.
    func ajustes(documento: UInt64) -> [String: AjustesFondoObjetoIPad] {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let prefijo = "\(documento):"
        return Dictionary(uniqueKeysWithValues: archivo.valores.compactMap {
            clave, ajustes in
            guard clave.hasPrefix(prefijo) else { return nil }
            return (String(clave.dropFirst(prefijo.count)), ajustes)
        })
    }

    /// Copia los ajustes de fondo usando los IDs estables de los medios del
    /// documento. Esos IDs se conservan deliberadamente en `cargarComoCopia`;
    /// lo que cambia es la identidad del documento que los contiene.
    @discardableResult
    func duplicar(desde origen: UInt64, hacia destino: UInt64) throws -> Int {
        guard origen != destino else { throw CocoaError(.fileWriteInvalidFileName) }
        bloqueo.lock(); defer { bloqueo.unlock() }
        let prefijoOrigen = "\(origen):"
        let prefijoDestino = "\(destino):"
        guard !archivo.valores.keys.contains(where: { $0.hasPrefix(prefijoDestino) }),
              !archivo.nombres.keys.contains(where: { $0.hasPrefix(prefijoDestino) }) else {
            throw CocoaError(.fileWriteFileExists)
        }

        var siguiente = archivo
        var copiados = 0
        for (claveOrigen, ajustes) in archivo.valores
        where claveOrigen.hasPrefix(prefijoOrigen) {
            let elemento = String(claveOrigen.dropFirst(prefijoOrigen.count))
            let claveDestino = "\(prefijoDestino)\(elemento)"
            siguiente.valores[claveDestino] = ajustes
            siguiente.nombres[claveDestino] = archivo.nombres[claveOrigen] ?? "Elemento"
            copiados += 1
        }
        try persistir(siguiente)
        archivo = siguiente
        return copiados
    }

    /// Retira todas las claves que pertenecen a un documento descartado. La
    /// operación es copy-on-write para que un rollback fallido no deje memoria
    /// adelantada respecto del archivo global.
    func eliminarDocumento(_ documento: UInt64) throws {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let prefijo = "\(documento):"
        let tieneDatos = archivo.valores.keys.contains { $0.hasPrefix(prefijo) }
            || archivo.nombres.keys.contains { $0.hasPrefix(prefijo) }
        guard tieneDatos else { return }
        var siguiente = archivo
        siguiente.valores = siguiente.valores.filter { !$0.key.hasPrefix(prefijo) }
        siguiente.nombres = siguiente.nombres.filter { !$0.key.hasPrefix(prefijo) }
        try persistir(siguiente)
        archivo = siguiente
    }

    private func persistir(_ valor: ArchivoFondosObjetoIPad) throws {
        let datos = try JSONEncoder().encode(valor)
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

enum ProcesadorFondoObjetoIPad {
    private static let contexto = CIContext(options: [.cacheIntermediates: true])

    static func procesar(_ imagen: UIImage,
                         ajustes: AjustesFondoObjetoIPad) -> UIImage {
        guard ajustes.esFondo, var entrada = CIImage(image: imagen) else {
            return imagen
        }
        let color = CIFilter.colorControls()
        color.inputImage = entrada
        color.contrast = max(0.1, ajustes.contraste)
        color.saturation = ajustes.recolor == .escalaGrises ? 0 : 1
        entrada = color.outputImage ?? entrada
        switch ajustes.recolor {
        case .original, .escalaGrises: break
        case .sepia:
            let filtro = CIFilter.sepiaTone(); filtro.inputImage = entrada
            filtro.intensity = 0.72; entrada = filtro.outputImage ?? entrada
        case .azulTecnico:
            let filtro = CIFilter.colorMonochrome(); filtro.inputImage = entrada
            filtro.color = CIColor(red: 0.12, green: 0.34, blue: 0.62)
            filtro.intensity = 0.72; entrada = filtro.outputImage ?? entrada
        case .altoContraste:
            let filtro = CIFilter.colorControls(); filtro.inputImage = entrada
            filtro.contrast = max(1.55, ajustes.contraste)
            filtro.saturation = 0; entrada = filtro.outputImage ?? entrada
        }
        guard let cg = contexto.createCGImage(entrada, from: entrada.extent) else {
            return imagen
        }
        return UIImage(cgImage: cg, scale: imagen.scale,
                       orientation: imagen.imageOrientation)
    }
}

final class PanelAjustesFondoObjetoIPad: UIViewController {
    var alAplicar: ((AjustesFondoObjetoIPad) -> Void)?
    private var ajustes: AjustesFondoObjetoIPad
    private let fondo = UISwitch()
    private let bloqueo = UISwitch()
    private let oculto = UISwitch()
    private let opacidad = UISlider()
    private let contraste = UISlider()
    private let escala = UISlider()
    private let recolor = UIButton(type: .system)

    init(ajustes: AjustesFondoObjetoIPad) {
        self.ajustes = ajustes
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Printout como fondo"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Aplicar", style: .done, target: self, action: #selector(aplicar))

        fondo.isOn = ajustes.esFondo
        bloqueo.isOn = ajustes.bloqueado
        oculto.isOn = ajustes.oculto
        opacidad.minimumValue = 0.05; opacidad.maximumValue = 1
        opacidad.value = Float(ajustes.opacidad)
        contraste.minimumValue = 0.4; contraste.maximumValue = 2.5
        contraste.value = ajustes.contraste
        escala.minimumValue = 0.25; escala.maximumValue = 4
        escala.value = Float(ajustes.escala)
        recolor.showsMenuAsPrimaryAction = true
        actualizarMenuRecolor()

        let pila = UIStackView(arrangedSubviews: [
            fila("Tratar como fondo", fondo),
            fila("Bloquear", bloqueo),
            fila("Ocultar", oculto),
            etiqueta("Opacidad"), opacidad,
            etiqueta("Contraste"), contraste,
            etiqueta("Escala respecto al tamaño actual"), escala,
            etiqueta("Recolor no destructivo"), recolor,
            nota("El adjunto original no se modifica. Un fondo oculto se puede recuperar desde Capas → Fondos/printouts."),
        ])
        pila.axis = .vertical; pila.spacing = 14
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                       constant: 22),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),
        ])
    }

    private func actualizarMenuRecolor() {
        recolor.configuration = .bordered()
        recolor.configuration?.title = ajustes.recolor.nombre
        recolor.menu = UIMenu(children: RecolorFondoObjetoIPad.allCases.map { valor in
            UIAction(title: valor.nombre,
                     state: valor == ajustes.recolor ? .on : .off) { [weak self] _ in
                self?.ajustes.recolor = valor
                self?.actualizarMenuRecolor()
            }
        })
    }

    private func fila(_ titulo: String, _ control: UIView) -> UIView {
        let texto = UILabel(); texto.text = titulo
        let pila = UIStackView(arrangedSubviews: [texto, control])
        pila.axis = .horizontal; pila.distribution = .equalSpacing
        return pila
    }
    private func etiqueta(_ texto: String) -> UILabel {
        let etiqueta = UILabel(); etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .headline); return etiqueta
    }
    private func nota(_ texto: String) -> UILabel {
        let etiqueta = UILabel(); etiqueta.text = texto; etiqueta.numberOfLines = 0
        etiqueta.textColor = .secondaryLabel
        etiqueta.font = .preferredFont(forTextStyle: .footnote); return etiqueta
    }

    @objc private func aplicar() {
        ajustes.esFondo = fondo.isOn
        ajustes.bloqueado = bloqueo.isOn || fondo.isOn
        ajustes.oculto = oculto.isOn
        ajustes.opacidad = CGFloat(opacidad.value)
        ajustes.contraste = contraste.value
        ajustes.escala = CGFloat(escala.value)
        alAplicar?(ajustes); dismiss(animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

final class PanelFondosObjetoIPad: UITableViewController {
    var alCambiar: (() -> Void)?
    private let documento: UInt64
    private weak var sesion: Sesion?
    private var registros: [RegistroFondoObjetoIPad] = []

    init(documento: UInt64, sesion: Sesion? = nil) {
        self.documento = documento
        self.sesion = sesion
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Fondos / printouts"; refrescar()
    }
    private func refrescar() {
        registros = RepositorioFondosObjetoIPad.compartido
            .registros(documento: documento)
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(registros.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !registros.isEmpty else {
            celda.textLabel?.text = "No hay printouts configurados como fondo"
            celda.selectionStyle = .none; return celda
        }
        let registro = registros[indexPath.row]
        celda.textLabel?.text = registro.nombre
        celda.detailTextLabel?.text = "\(registro.ajustes.oculto ? "oculto" : "visible") · \(Int(registro.ajustes.opacidad * 100))% · \(registro.ajustes.recolor.nombre)"
        celda.accessoryType = registro.ajustes.oculto ? .none : .checkmark
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard registros.indices.contains(indexPath.row) else { return }
        let registro = registros[indexPath.row]
        var ajustes = registro.ajustes; ajustes.oculto.toggle()
        RepositorioFondosObjetoIPad.compartido.guardar(
            ajustes, documento: documento,
            elemento: registro.id, nombre: registro.nombre)
        alCambiar?(); refrescar()
    }

    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard registros.indices.contains(indexPath.row) else { return nil }
        let registro = registros[indexPath.row]
        let quitar = UIContextualAction(style: .destructive, title: "Dejar fondo") {
            [weak self] _, _, terminar in
            guard let self else { terminar(false); return }
            var ajustes = registro.ajustes
            ajustes.esFondo = false; ajustes.bloqueado = false
            ajustes.oculto = false
            RepositorioFondosObjetoIPad.compartido.guardar(
                ajustes, documento: self.documento,
                elemento: registro.id, nombre: registro.nombre)
            self.alCambiar?(); self.refrescar(); terminar(true)
        }
        return UISwipeActionsConfiguration(actions: [quitar])
    }

    override func tableView(_ tableView: UITableView,
                            leadingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard registros.indices.contains(indexPath.row) else { return nil }
        let registro = registros[indexPath.row]
        let editar = UIContextualAction(style: .normal, title: "Editar") {
            [weak self] _, _, terminar in
            guard let self else { terminar(false); return }
            self.editar(registro); terminar(true)
        }
        editar.backgroundColor = .systemIndigo
        return UISwipeActionsConfiguration(actions: [editar])
    }

    private func editar(_ registro: RegistroFondoObjetoIPad) {
        let anteriores = registro.ajustes
        let panel = PanelAjustesFondoObjetoIPad(ajustes: anteriores)
        panel.alAplicar = { [weak self] nuevos in
            guard let self else { return }
            if var medio = self.sesion?.medios.first(where: {
                $0.id == registro.id
            }) {
                let relacion = nuevos.escala / max(anteriores.escala, 0.01)
                if abs(relacion - 1) > 0.001 {
                    let centro = CGPoint(x: medio.marco.midX, y: medio.marco.midY)
                    medio.marco.size = CGSize(
                        width: min(max(medio.marco.width * relacion, 44), 8_000),
                        height: min(max(medio.marco.height * relacion, 44), 8_000))
                    medio.marco.origin = CGPoint(
                        x: centro.x - medio.marco.width / 2,
                        y: centro.y - medio.marco.height / 2)
                    _ = self.sesion?.actualizarMedio(medio)
                }
            }
            RepositorioFondosObjetoIPad.compartido.guardar(
                nuevos, documento: self.documento,
                elemento: registro.id, nombre: registro.nombre)
            self.alCambiar?(); self.refrescar()
        }
        present(UINavigationController(rootViewController: panel), animated: true)
    }
}

/// Los printouts se componen entre el papel y la capa Metal transparente. De
/// esta forma la tinta nueva y la ya horneada siempre permanecen encima del
/// PDF o imagen, no solo se bloquea su interacción.
final class CapaFondosObjetoIPad: UIView {
    private let sesion: Sesion

    init(sesion: Sesion) {
        self.sesion = sesion
        super.init(frame: .zero)
        backgroundColor = .clear
        isUserInteractionEnabled = false
        autoresizingMask = [.flexibleWidth, .flexibleHeight]
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? { nil }

    func recargar() {
        subviews.forEach { $0.removeFromSuperview() }
        let documento = sesion.documentoPersistenteId
        for medio in sesion.medios {
            let ajustes = RepositorioFondosObjetoIPad.compartido.ajustes(
                documento: documento, elemento: medio.id)
            guard ajustes.esFondo, !ajustes.oculto else { continue }
            let vista = UIImageView(frame: medio.marco)
            vista.accessibilityIdentifier = medio.id
            vista.contentMode = .scaleAspectFit
            vista.clipsToBounds = true
            vista.alpha = medio.opacidad * ajustes.opacidad
            vista.transform = CGAffineTransform(rotationAngle: medio.rotacion)
            addSubview(vista)
            aplicarMascaraClip(siCorresponde: medio, a: vista)
            cargar(medio, ajustes: ajustes, en: vista)
        }
    }

    private func cargar(_ medio: ElementoMedio,
                        ajustes: AjustesFondoObjetoIPad,
                        en vista: UIImageView) {
        guard let url = RepositorioAdjuntos.compartido.url(hash: medio.hash) else {
            vista.image = UIImage(systemName: "doc.questionmark")
            return
        }
        if medio.tipo == .imagen,
           let original = UIImage(contentsOfFile: url.path) {
            let recortada = Medios.recortar(original, con: medio.recorte)
            let edicion = RepositorioEdicionesImagenDocumentoIPad.compartido.ajustes(
                documento: sesion.documentoPersistenteId, id: medio.id)
            let procesada = ProcesadorImagenLocal.procesar(recortada,
                                                            ajustes: edicion)
            vista.image = ProcesadorFondoObjetoIPad.procesar(
                procesada, ajustes: ajustes)
            return
        }
        if medio.tipo == .pdf, let pdf = PDFDocument(url: url),
           let pagina = pdf.page(at: medio.paginaFuente) {
            let tamano = CGSize(width: max(medio.marco.width, 1) * 2,
                                height: max(medio.marco.height, 1) * 2)
            let completa = pagina.thumbnail(of: tamano, for: .mediaBox)
            let recortada = medio.recorte.esCompleto
                ? completa : Medios.recortar(completa, con: medio.recorte)
            vista.image = ProcesadorFondoObjetoIPad.procesar(
                recortada, ajustes: ajustes)
            return
        }
        let solicitud = QLThumbnailGenerator.Request(
            fileAt: url, size: CGSize(width: 1_200, height: 1_200),
            scale: UIScreen.main.scale, representationTypes: .thumbnail)
        QLThumbnailGenerator.shared.generateBestRepresentation(for: solicitud) {
            [weak vista] miniatura, _ in
            guard let base = miniatura?.uiImage else { return }
            let salida = ProcesadorFondoObjetoIPad.procesar(base,
                                                             ajustes: ajustes)
            DispatchQueue.main.async { vista?.image = salida }
        }
    }

    private func aplicarMascaraClip(siCorresponde medio: ElementoMedio,
                                    a vista: UIImageView) {
        guard medio.tipo == .pdf,
              let ruta = RenderizadorClipPDFDocumentoIPad.ruta(
                documento: sesion.documentoPersistenteId,
                elementoId: medio.id, en: vista.bounds) else { return }
        let mascara = CAShapeLayer()
        mascara.frame = vista.bounds
        mascara.path = ruta.cgPath
        mascara.fillColor = UIColor.black.cgColor
        vista.layer.mask = mascara
    }
}
