import UIKit
import UniformTypeIdentifiers

enum PanelPestanasIPad: String, Codable {
    case principal
    case secundario
}

struct RegistroPestanaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var colorARGB: UInt32
    var fijada = false
    var grupo: String?
    var archivo: String
    var vista: EstadoVistaLienzo?
    var ultimoUso = Date()
}

struct EstadoPestanasIPad: Codable, Equatable {
    var formato = 1
    var abiertas: [RegistroPestanaIPad] = []
    var cerradas: [RegistroPestanaIPad] = []
    var activa: UUID?
}

enum ErrorPersistenciaPestanasIPad: LocalizedError, Equatable {
    case manifiestoDanado
    case nombreArchivoInvalido
    case recuperacionIncompleta
    case manifiestoInconsistente

    var errorDescription: String? {
        switch self {
        case .manifiestoDanado:
            return "No se puede eliminar el cuaderno porque su índice de pestañas está dañado."
        case .nombreArchivoInvalido:
            return "El cuaderno contiene una referencia de archivo no válida."
        case .recuperacionIncompleta:
            return "No se pudieron restaurar todos los archivos del cuaderno."
        case .manifiestoInconsistente:
            return "El índice de pestañas contiene referencias inconsistentes y se conservó sin modificar."
        }
    }
}

enum ResultadoCargaPestanasIPad {
    case inexistente
    case cargado(EstadoPestanasIPad)
    case danado(Error)
}

enum ResultadoCargaContenidoPestana {
    case inexistente
    case cargado(Data)
    case ilegible(Error)
}

private struct ErrorLecturaContenidoPestana: LocalizedError {
    let descripcion: String
    var errorDescription: String? { descripcion }
}

/// Movimiento reversible usado para que eliminar una tarjeta y sus pestañas
/// sea una sola operación lógica. Hasta llamar `confirmar`, todos los archivos
/// pueden volver a su ubicación original.
final class TransaccionEliminacionPestanasIPad {
    fileprivate struct Movimiento {
        let origen: URL
        let temporal: URL
    }

    private let archivos: FileManager
    private let carpetaTemporal: URL?
    private let movimientos: [Movimiento]
    private var terminada = false

    fileprivate init(archivos: FileManager, carpetaTemporal: URL?,
                     movimientos: [Movimiento]) {
        self.archivos = archivos
        self.carpetaTemporal = carpetaTemporal
        self.movimientos = movimientos
    }

    func confirmar() throws {
        guard !terminada else { return }
        if let carpetaTemporal,
           archivos.fileExists(atPath: carpetaTemporal.path) {
            try archivos.removeItem(at: carpetaTemporal)
        }
        terminada = true
    }

    func revertir() throws {
        guard !terminada else { return }
        var fallo = false
        for movimiento in movimientos.reversed() {
            guard archivos.fileExists(atPath: movimiento.temporal.path) else {
                continue
            }
            if archivos.fileExists(atPath: movimiento.origen.path) {
                fallo = true
                continue
            }
            do {
                try archivos.moveItem(at: movimiento.temporal,
                                      to: movimiento.origen)
            } catch {
                fallo = true
            }
        }
        if !fallo, let carpetaTemporal,
           archivos.fileExists(atPath: carpetaTemporal.path) {
            try? archivos.removeItem(at: carpetaTemporal)
        }
        terminada = !fallo
        if fallo { throw ErrorPersistenciaPestanasIPad.recuperacionIncompleta }
    }
}

private struct DiarioEliminacionPestanasIPad: Codable {
    let version: Int
    let contexto: String
    let archivos: [String]
}

final class RepositorioPestanasIPad {
    static let compartido = RepositorioPestanasIPad()
    private static let nombreDiarioEliminacion = "transaccion.json"

    private let archivos: FileManager
    private let base: URL
    private let cola = DispatchQueue(label: "tinta.pestanas.persistencia")

    init(directorioBase: URL? = nil, archivos: FileManager = .default) {
        self.archivos = archivos
        if let directorioBase {
            base = directorioBase
        } else {
            let soporte = archivos.urls(for: .applicationSupportDirectory,
                                        in: .userDomainMask)[0]
            base = soporte.appendingPathComponent("Tinta/Pestanas",
                                                  isDirectory: true)
        }
        try? archivos.createDirectory(at: base,
                                      withIntermediateDirectories: true)
    }

    private func manifiesto(_ panel: PanelPestanasIPad, contexto: String) -> URL {
        let seguro = contexto.map {
            $0.isLetter || $0.isNumber || $0 == "-" ? $0 : "_"
        }
        return base.appendingPathComponent(
            "\(panel.rawValue)-\(String(seguro)).json")
    }

    private func archivoSeguro(_ registro: RegistroPestanaIPad) throws -> URL {
        let nombre = registro.archivo
        guard !nombre.isEmpty,
              (nombre as NSString).lastPathComponent == nombre,
              nombre != ".", nombre != ".." else {
            throw ErrorPersistenciaPestanasIPad.nombreArchivoInvalido
        }
        return base.appendingPathComponent(nombre, isDirectory: false)
    }

    private func validar(_ estado: EstadoPestanasIPad,
                         panel: PanelPestanasIPad) throws {
        let registros = estado.abiertas + estado.cerradas
        let ids = registros.map(\.id)
        let nombres = registros.map(\.archivo)
        guard estado.formato == 1, registros.count <= 5_000,
              Set(ids).count == ids.count,
              Set(nombres).count == nombres.count,
              estado.activa.map({ activa in
                  estado.abiertas.contains { $0.id == activa }
              }) ?? true,
              registros.allSatisfy({ registro in
                  !registro.titulo.isEmpty && registro.titulo.count <= 240
                      && registro.archivo
                          == "\(panel.rawValue)-\(registro.id.uuidString).tinta"
              }) else {
            throw ErrorPersistenciaPestanasIPad.manifiestoInconsistente
        }
    }

    func leer(_ panel: PanelPestanasIPad,
              contexto: String) -> ResultadoCargaPestanasIPad {
        cola.sync {
            let ruta = manifiesto(panel, contexto: contexto)
            guard archivos.fileExists(atPath: ruta.path) else {
                return .inexistente
            }
            do {
                let datos = try Data(contentsOf: ruta)
                let estado = try JSONDecoder().decode(
                    EstadoPestanasIPad.self, from: datos)
                try validar(estado, panel: panel)
                return .cargado(estado)
            } catch {
                return .danado(error)
            }
        }
    }

    func guardar(_ estado: EstadoPestanasIPad, panel: PanelPestanasIPad,
                 contexto: String) throws {
        try cola.sync {
            try validar(estado, panel: panel)
            let datos = try JSONEncoder().encode(estado)
            try datos.write(to: manifiesto(panel, contexto: contexto),
                            options: [.atomic, .completeFileProtection])
        }
    }

    func guardarContenido(_ datos: Data,
                          registro: RegistroPestanaIPad) throws {
        try cola.sync {
            try guardarContenidoSincronizado(datos, registro: registro)
        }
    }

    /// Variante no bloqueante para el autoguardado. Comparte la misma cola con
    /// las lecturas y checkpoints explícitos para conservar el orden exacto.
    func guardarContenido(_ datos: Data,
                          registro: RegistroPestanaIPad,
                          terminar: @escaping (Result<Void, Error>) -> Void) {
        cola.async {
            let resultado = Result {
                try self.guardarContenidoSincronizado(datos, registro: registro)
            }
            DispatchQueue.main.async { terminar(resultado) }
        }
    }

    private func guardarContenidoSincronizado(
        _ datos: Data, registro: RegistroPestanaIPad
    ) throws {
        let ruta = try archivoSeguro(registro)
        try datos.write(to: ruta,
                        options: [.atomic, .completeFileProtection])
    }

    func contenido(_ registro: RegistroPestanaIPad)
        -> ResultadoCargaContenidoPestana {
        cola.sync {
            do {
                let ruta = try archivoSeguro(registro)
                guard archivos.fileExists(atPath: ruta.path) else {
                    return .inexistente
                }
                return .cargado(try Data(contentsOf: ruta,
                                         options: .mappedIfSafe))
            } catch {
                return .ilegible(error)
            }
        }
    }

    func prepararEliminacion(contexto: String) throws
        -> TransaccionEliminacionPestanasIPad {
        try cola.sync {
            var objetivos: [URL] = []
            var manifiestosObjetivo = Set<String>()

            for panel in [PanelPestanasIPad.principal, .secundario] {
                let rutaManifiesto = manifiesto(panel, contexto: contexto)
                guard archivos.fileExists(atPath: rutaManifiesto.path) else {
                    continue
                }
                manifiestosObjetivo.insert(rutaManifiesto.standardizedFileURL.path)
                let estado: EstadoPestanasIPad
                do {
                    let datos = try Data(contentsOf: rutaManifiesto)
                    estado = try JSONDecoder().decode(EstadoPestanasIPad.self,
                                                       from: datos)
                    try validar(estado, panel: panel)
                } catch {
                    throw ErrorPersistenciaPestanasIPad.manifiestoDanado
                }
                for registro in estado.abiertas + estado.cerradas {
                    objetivos.append(try archivoSeguro(registro))
                }
                objetivos.append(rutaManifiesto)
            }

            // El almacén histórico es plano. Antes de mover un blob se prueba
            // que ningún otro manifiesto lo reclame; una referencia cruzada se
            // trata como corrupción y se preservan ambos cuadernos.
            let blobsObjetivo = Set(objetivos.filter {
                $0.pathExtension.lowercased() != "json"
            }.map(\.lastPathComponent))
            if !blobsObjetivo.isEmpty {
                let entradas = try archivos.contentsOfDirectory(
                    at: base, includingPropertiesForKeys: [.isRegularFileKey],
                    options: [.skipsHiddenFiles])
                for ruta in entradas where ruta.pathExtension.lowercased() == "json"
                    && !manifiestosObjetivo.contains(
                        ruta.standardizedFileURL.path) {
                    let panelAjeno: PanelPestanasIPad
                    if ruta.lastPathComponent.hasPrefix("principal-") {
                        panelAjeno = .principal
                    } else if ruta.lastPathComponent.hasPrefix("secundario-") {
                        panelAjeno = .secundario
                    } else {
                        throw ErrorPersistenciaPestanasIPad.manifiestoInconsistente
                    }
                    let datos = try Data(contentsOf: ruta)
                    let ajeno = try JSONDecoder().decode(
                        EstadoPestanasIPad.self, from: datos)
                    try validar(ajeno, panel: panelAjeno)
                    let referencias = Set((ajeno.abiertas + ajeno.cerradas)
                        .map(\.archivo))
                    guard referencias.isDisjoint(with: blobsObjetivo) else {
                        throw ErrorPersistenciaPestanasIPad
                            .manifiestoInconsistente
                    }
                }
            }

            var rutasVistas = Set<String>()
            let unicos = objetivos.map(\.standardizedFileURL).filter {
                rutasVistas.insert($0.path).inserted
                    && archivos.fileExists(atPath: $0.path)
            }
            guard !unicos.isEmpty else {
                return TransaccionEliminacionPestanasIPad(
                    archivos: archivos, carpetaTemporal: nil, movimientos: [])
            }

            let temporal = base.appendingPathComponent(
                ".eliminacion-\(UUID().uuidString)", isDirectory: true)
            try archivos.createDirectory(at: temporal,
                                         withIntermediateDirectories: false)
            do {
                let diario = DiarioEliminacionPestanasIPad(
                    version: 1,
                    contexto: contexto,
                    archivos: unicos.map(\.lastPathComponent))
                let datos = try JSONEncoder().encode(diario)
                try datos.write(to: temporal.appendingPathComponent(
                    Self.nombreDiarioEliminacion), options: .atomic)
            } catch {
                try? archivos.removeItem(at: temporal)
                throw error
            }
            var movimientos: [TransaccionEliminacionPestanasIPad.Movimiento] = []
            do {
                for origen in unicos {
                    let destino = temporal.appendingPathComponent(
                        origen.lastPathComponent, isDirectory: false)
                    try archivos.moveItem(at: origen, to: destino)
                    movimientos.append(.init(origen: origen, temporal: destino))
                }
            } catch {
                let transaccion = TransaccionEliminacionPestanasIPad(
                    archivos: archivos, carpetaTemporal: temporal,
                    movimientos: movimientos)
                try? transaccion.revertir()
                throw error
            }
            return TransaccionEliminacionPestanasIPad(
                archivos: archivos, carpetaTemporal: temporal,
                movimientos: movimientos)
        }
    }

    /// Devuelve los contextos que usaba la aplicación antes de la biblioteca.
    /// Los identificadores de `UISceneSession` son seguros y se conservaron en
    /// el nombre de los manifiestos principal/secundario.
    func contextosPersistidos() throws -> Set<String> {
        try cola.sync {
            let entradas = try archivos.contentsOfDirectory(
                at: base, includingPropertiesForKeys: [.isRegularFileKey],
                options: [.skipsHiddenFiles])
            var contextos = Set<String>()
            for entrada in entradas {
                let nombre = entrada.lastPathComponent
                for panel in [PanelPestanasIPad.principal, .secundario] {
                    let prefijo = "\(panel.rawValue)-"
                    guard nombre.hasPrefix(prefijo), nombre.hasSuffix(".json") else {
                        continue
                    }
                    let inicio = nombre.index(nombre.startIndex,
                                              offsetBy: prefijo.count)
                    let fin = nombre.index(nombre.endIndex, offsetBy: -5)
                    let contexto = String(nombre[inicio..<fin])
                    if !contexto.isEmpty { contextos.insert(contexto) }
                }
            }
            return contextos
        }
    }

    /// Resuelve una terminación en mitad del borrado consultando el catálogo:
    /// si la tarjeta aún existe, restaura los archivos; si ya no existe,
    /// completa la eliminación. Un diario ilegible se conserva sin tocar para
    /// evitar pérdida de datos.
    func recuperarEliminacionesInterrumpidas(
        contextosExistentes: Set<String>?) throws {
        try cola.sync {
            let entradas = try archivos.contentsOfDirectory(
                at: base, includingPropertiesForKeys: [.isDirectoryKey],
                options: [])
            for carpeta in entradas where carpeta.lastPathComponent.hasPrefix(
                ".eliminacion-") {
                let rutaDiario = carpeta.appendingPathComponent(
                    Self.nombreDiarioEliminacion)
                let datos: Data
                let diario: DiarioEliminacionPestanasIPad
                do {
                    datos = try Data(contentsOf: rutaDiario)
                    diario = try JSONDecoder().decode(
                        DiarioEliminacionPestanasIPad.self, from: datos)
                } catch {
                    throw ErrorPersistenciaPestanasIPad.recuperacionIncompleta
                }
                guard diario.version == 1, !diario.contexto.isEmpty else {
                    throw ErrorPersistenciaPestanasIPad.recuperacionIncompleta
                }

                if let contextosExistentes,
                   !contextosExistentes.contains(diario.contexto) {
                    try archivos.removeItem(at: carpeta)
                    continue
                }

                for nombre in diario.archivos {
                    guard (nombre as NSString).lastPathComponent == nombre,
                          nombre != Self.nombreDiarioEliminacion,
                          nombre != ".", nombre != ".." else {
                        throw ErrorPersistenciaPestanasIPad.recuperacionIncompleta
                    }
                    let temporal = carpeta.appendingPathComponent(nombre)
                    let original = base.appendingPathComponent(nombre)
                    let existeTemporal = archivos.fileExists(atPath: temporal.path)
                    let existeOriginal = archivos.fileExists(atPath: original.path)
                    switch (existeTemporal, existeOriginal) {
                    case (true, false):
                        try archivos.moveItem(at: temporal, to: original)
                    case (false, true):
                        break
                    default:
                        throw ErrorPersistenciaPestanasIPad.recuperacionIncompleta
                    }
                }
                try archivos.removeItem(at: carpeta)
            }
        }
    }
}

func prepararEliminacionPestanasIPad(contexto: String) throws
    -> TransaccionEliminacionPestanasIPad {
    try RepositorioPestanasIPad.compartido.prepararEliminacion(
        contexto: contexto)
}

func contextosPersistidosPestanasIPad() throws -> Set<String> {
    try RepositorioPestanasIPad.compartido.contextosPersistidos()
}

func recuperarEliminacionesPestanasIPad(
    contextosExistentes: Set<String>?) throws {
    try RepositorioPestanasIPad.compartido.recuperarEliminacionesInterrumpidas(
        contextosExistentes: contextosExistentes)
}

/// Pestañas reales dentro de cada columna del iPad. Cada pestaña conserva su
/// propio controlador, sesión de tinta, zoom y archivo local; las dos columnas
/// siguen teniendo desplazamiento y zoom independientes.
final class ControladorPestanasDocumentoIPad: UIViewController {
    private struct PestanaViva {
        var registro: RegistroPestanaIPad
        var lienzo: ControladorLienzo
        var navegacion: UINavigationController
        var escrituraBloqueada: Bool
        var errorCarga: String?
    }

    private let panel: PanelPestanasIPad
    private let viewport: TintaViewport
    private let contextoEscena: String
    private let repositorio: RepositorioPestanasIPad
    private var estado: EstadoPestanasIPad
    private let errorCarga: Error?
    private var contenidoMaterializado = false
    private var idsNuevosSinContenido = Set<UUID>()
    private var vivas: [PestanaViva] = []
    private var indiceActivo = 0
    private var errorPersistenciaPendiente: Error?
    private var presentandoErrorPersistencia = false
    private var escrituraGlobalBloqueada = false
    private let barra = UIScrollView()

    /// Métrica estructural usada por diagnóstico y regresión de memoria. Una
    /// columna visible debe conservar recursos GPU únicamente para su pestaña
    /// activa; las demás mantienen el modelo pero no sus texturas de canvas.
    var cantidadLienzosConRecursosGraficos: Int {
        vivas.reduce(into: 0) { cantidad, viva in
            if viva.lienzo.conservaRecursosGraficosActivos { cantidad += 1 }
        }
    }
    private let pila = UIStackView()
    private let agregar = UIButton(type: .system)
    private let contenedor = UIView()
    var alCerrarDocumento: (() -> Void)?

    init(panel: PanelPestanasIPad, viewport: TintaViewport,
         contextoEscena: String = "principal",
         repositorio: RepositorioPestanasIPad = .compartido) {
        self.panel = panel
        self.viewport = viewport
        self.contextoEscena = contextoEscena
        self.repositorio = repositorio
        switch repositorio.leer(panel, contexto: contextoEscena) {
        case .inexistente:
            self.estado = EstadoPestanasIPad()
            self.errorCarga = nil
        case .cargado(let estado):
            self.estado = estado
            self.errorCarga = nil
        case .danado(let error):
            self.estado = EstadoPestanasIPad()
            self.errorCarga = error
        }
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        view.accessibilityIdentifier = panel == .principal
            ? "editor.panel.principal" : "editor.panel.referencia"
        configurarBarra()
        guard errorCarga == nil else {
            presentarManifiestoDanado()
            return
        }
        contenidoMaterializado = true
        restaurarPestanas()
        // El primer manifiesto debe existir antes de que la superficie quede
        // editable. `restaurarPestanas` puede crear la pestaña inicial.
        do {
            for id in idsNuevosSinContenido {
                if let indice = vivas.firstIndex(where: { $0.registro.id == id }) {
                    try guardar(indice)
                }
            }
            idsNuevosSinContenido.removeAll()
            try sincronizarEstado()
        } catch {
            bloquearEdicionPorFalloInicial()
            presentarErrorPersistencia(error)
        }
        NotificationCenter.default.addObserver(
            self, selector: #selector(guardarTodas),
            name: UIApplication.didEnterBackgroundNotification, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(alActivarseAplicacion(_:)),
            name: UIApplication.didBecomeActiveNotification, object: nil)
        if panel == .principal {
            DispatchQueue.main.async { [weak self] in
                self?.consumirEntradasDelSistema()
                PublicadorWidgetTintaIPad.actualizar()
            }
        }
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        if vivas.indices.contains(indiceActivo) {
            vivas[indiceActivo].lienzo.reanudarRecursosGraficos()
        }
        presentarErrorPersistenciaPendienteSiEsPosible()
        consumirEntradasDelSistema()
    }

    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        for viva in vivas { viva.lienzo.suspenderRecursosGraficos() }
    }

    deinit { NotificationCenter.default.removeObserver(self) }

    private func presentarManifiestoDanado() {
        barra.isHidden = true
        agregar.isHidden = true
        let mensaje = UILabel()
        mensaje.translatesAutoresizingMaskIntoConstraints = false
        mensaje.textAlignment = .center
        mensaje.numberOfLines = 0
        mensaje.font = .preferredFont(forTextStyle: .body)
        mensaje.adjustsFontForContentSizeCategory = true
        mensaje.textColor = .secondaryLabel
        mensaje.text = "No se pudo abrir el índice de pestañas. El archivo original se conservó para recuperación."
        mensaje.accessibilityIdentifier = "editor.manifiesto.danado"
        if panel == .principal {
            var configuracion = UIButton.Configuration.filled()
            configuracion.title = "Volver a CuadernU"
            configuracion.image = UIImage(systemName: "chevron.backward")
            configuracion.imagePadding = 8
            let volver = UIButton(configuration: configuracion)
            volver.addTarget(self, action: #selector(cerrarDocumento),
                             for: .touchUpInside)
            volver.accessibilityIdentifier = "editor.volver.biblioteca"
            let pila = UIStackView(arrangedSubviews: [mensaje, volver])
            pila.axis = .vertical
            pila.alignment = .center
            pila.spacing = 20
            pila.translatesAutoresizingMaskIntoConstraints = false
            contenedor.addSubview(pila)
            NSLayoutConstraint.activate([
                pila.centerXAnchor.constraint(equalTo: contenedor.centerXAnchor),
                pila.centerYAnchor.constraint(equalTo: contenedor.centerYAnchor),
                pila.leadingAnchor.constraint(
                    greaterThanOrEqualTo: contenedor.leadingAnchor, constant: 32),
                pila.trailingAnchor.constraint(
                    lessThanOrEqualTo: contenedor.trailingAnchor, constant: -32),
                volver.heightAnchor.constraint(greaterThanOrEqualToConstant: 44),
            ])
        } else {
            contenedor.addSubview(mensaje)
            NSLayoutConstraint.activate([
                mensaje.centerXAnchor.constraint(equalTo: contenedor.centerXAnchor),
                mensaje.centerYAnchor.constraint(equalTo: contenedor.centerYAnchor),
                mensaje.leadingAnchor.constraint(
                    greaterThanOrEqualTo: contenedor.leadingAnchor, constant: 32),
                mensaje.trailingAnchor.constraint(
                    lessThanOrEqualTo: contenedor.trailingAnchor, constant: -32),
            ])
        }
    }

    private func configurarBarra() {
        barra.showsHorizontalScrollIndicator = false
        barra.translatesAutoresizingMaskIntoConstraints = false
        pila.axis = .horizontal; pila.spacing = 5; pila.alignment = .fill
        pila.translatesAutoresizingMaskIntoConstraints = false
        barra.addSubview(pila)

        agregar.setImage(UIImage(systemName: "plus"), for: .normal)
        agregar.accessibilityLabel = "Nueva pestaña"
        agregar.accessibilityIdentifier = "editor.pestana.nueva"
        agregar.showsMenuAsPrimaryAction = true
        agregar.translatesAutoresizingMaskIntoConstraints = false
        contenedor.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(barra); view.addSubview(agregar); view.addSubview(contenedor)
        NSLayoutConstraint.activate([
            barra.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            barra.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            barra.heightAnchor.constraint(equalToConstant: 42),
            barra.trailingAnchor.constraint(equalTo: agregar.leadingAnchor),
            agregar.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -6),
            agregar.centerYAnchor.constraint(equalTo: barra.centerYAnchor),
            agregar.widthAnchor.constraint(equalToConstant: 44),
            agregar.heightAnchor.constraint(equalToConstant: 44),
            pila.leadingAnchor.constraint(equalTo: barra.contentLayoutGuide.leadingAnchor,
                                          constant: 6),
            pila.trailingAnchor.constraint(equalTo: barra.contentLayoutGuide.trailingAnchor,
                                           constant: -6),
            pila.topAnchor.constraint(equalTo: barra.contentLayoutGuide.topAnchor, constant: 4),
            pila.bottomAnchor.constraint(equalTo: barra.contentLayoutGuide.bottomAnchor,
                                         constant: -4),
            pila.heightAnchor.constraint(equalTo: barra.frameLayoutGuide.heightAnchor,
                                         constant: -8),
            contenedor.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            contenedor.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            contenedor.topAnchor.constraint(equalTo: barra.bottomAnchor),
            contenedor.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])
        reconstruirMenuAgregar()
    }

    private func restaurarPestanas() {
        if estado.abiertas.isEmpty {
            let registro = nuevoRegistro(titulo: panel == .principal
                                         ? "Apuntes" : "Referencia")
            idsNuevosSinContenido.insert(registro.id)
            estado.abiertas = [registro]
            estado.activa = estado.abiertas.first?.id
        }
        let idActivo = estado.activa ?? estado.abiertas.first?.id
        vivas = estado.abiertas.map { registro in
            crearPestana(
                registro,
                persistida: !idsNuevosSinContenido.contains(registro.id),
                recursosGraficosActivos: registro.id == idActivo)
        }
        for viva in vivas {
            addChild(viva.navegacion)
            viva.navegacion.view.translatesAutoresizingMaskIntoConstraints = false
            viva.navegacion.view.isHidden = true
            contenedor.addSubview(viva.navegacion.view)
            NSLayoutConstraint.activate([
                viva.navegacion.view.leadingAnchor.constraint(equalTo: contenedor.leadingAnchor),
                viva.navegacion.view.trailingAnchor.constraint(equalTo: contenedor.trailingAnchor),
                viva.navegacion.view.topAnchor.constraint(equalTo: contenedor.topAnchor),
                viva.navegacion.view.bottomAnchor.constraint(equalTo: contenedor.bottomAnchor),
            ])
            viva.navegacion.didMove(toParent: self)
        }
        indiceActivo = max(estado.activa.flatMap { id in
            vivas.firstIndex(where: { $0.registro.id == id })
        } ?? 0, 0)
        reconstruirBarra()
        activar(indiceActivo, guardarAnterior: false)
    }

    private func crearPestana(_ registro: RegistroPestanaIPad,
                              persistida: Bool,
                              recursosGraficosActivos: Bool = true) -> PestanaViva {
        let lienzo = ControladorLienzo(
            viewport: viewport,
            idPersistente: DocumentoPersistenteID(registro.id))
        lienzo.configurarPersistenciaCanonica {
            [repositorio = self.repositorio, registro] datos, terminar in
            repositorio.guardarContenido(
                datos, registro: registro, terminar: terminar)
        }
        lienzo.title = registro.titulo
        if !recursosGraficosActivos { lienzo.suspenderRecursosGraficos() }
        let tituloVolver = panel == .principal ? "CuadernU" : "Cerrar referencia"
        let volver = UIBarButtonItem(
            title: tituloVolver, style: .plain,
            target: self, action: #selector(cerrarDocumento))
        volver.accessibilityLabel = panel == .principal
            ? "Volver a CuadernU" : "Cerrar referencia"
        volver.accessibilityIdentifier = panel == .principal
            ? "editor.volver.biblioteca" : "editor.referencia.cerrar"
        lienzo.navigationItem.leftBarButtonItem = volver
        var escrituraBloqueada = false
        var mensajeCarga: String?
        switch repositorio.contenido(registro) {
        case .inexistente:
            if persistida {
                escrituraBloqueada = true
                mensajeCarga = "No se encontró el contenido de esta pestaña. El manifiesto se conservó y no se creará un archivo vacío encima."
            }
        case .cargado(let datos):
            if !lienzo.cargarDatosDePestana(datos) {
                escrituraBloqueada = true
                mensajeCarga = "El archivo de esta pestaña es incompatible, está dañado o pertenece a otro documento. Se conservó sin sobrescribir."
            }
        case .ilegible(let error):
            escrituraBloqueada = true
            mensajeCarga = "No se pudo leer esta pestaña: \(error.localizedDescription). Se conservó sin sobrescribir."
        }
        if let vista = registro.vista {
            DispatchQueue.main.async { lienzo.restaurarVistaDePestana(vista) }
        }
        lienzo.alCambiarTituloDocumento = { [weak self, weak lienzo] titulo in
            guard let self, let lienzo,
                  let indice = self.vivas.firstIndex(where: {
                      $0.lienzo === lienzo
                  }) else { return }
            self.vivas[indice].registro.titulo = titulo
            self.vivas[indice].registro.ultimoUso = Date()
            self.sincronizarEstadoControlandoError()
            self.reconstruirBarra()
        }
        if escrituraBloqueada {
            // La barra de navegación pertenece al UINavigationController y
            // sigue permitiendo volver; solo se congela la superficie que no
            // pudo cargarse para impedir ediciones imposibles de conservar.
            lienzo.view.isUserInteractionEnabled = false
            lienzo.navigationItem.prompt = "Pestaña protegida: no se sobrescribirá"
        }
        return PestanaViva(
            registro: registro, lienzo: lienzo,
            navegacion: UINavigationController(rootViewController: lienzo),
            escrituraBloqueada: escrituraBloqueada,
            errorCarga: mensajeCarga)
    }

    @objc private func cerrarDocumento() {
        alCerrarDocumento?()
    }

    private func nuevoRegistro(titulo: String) -> RegistroPestanaIPad {
        let id = UUID()
        let colores: [UInt32] = [0xFF0A84FF, 0xFF30D158, 0xFFFF9F0A,
                                  0xFFFF453A, 0xFFBF5AF2]
        return RegistroPestanaIPad(
            id: id, titulo: titulo,
            colorARGB: colores[estado.abiertas.count % colores.count],
            archivo: "\(panel.rawValue)-\(id.uuidString).tinta")
    }

    private func reconstruirBarra() {
        pila.arrangedSubviews.forEach { vista in
            pila.removeArrangedSubview(vista); vista.removeFromSuperview()
        }
        for (indice, viva) in vivas.enumerated() {
            var configuracion = UIButton.Configuration.gray()
            configuracion.title = (viva.registro.fijada ? "● " : "") + viva.registro.titulo
            configuracion.baseForegroundColor = UIColor(argb: viva.registro.colorARGB)
            configuracion.background.cornerRadius = 9
            let boton = UIButton(type: .system)
            boton.configuration = configuracion
            boton.tag = indice
            boton.addTarget(self, action: #selector(tocarPestana(_:)), for: .touchUpInside)
            boton.menu = menuPestana(indice)
            boton.showsMenuAsPrimaryAction = false
            boton.accessibilityHint = "Mantenga pulsado para organizar o cerrar"
            boton.addInteraction(UIDropInteraction(delegate: self))
            if indice == indiceActivo {
                boton.configuration?.background.backgroundColor = UIColor(
                    argb: viva.registro.colorARGB).withAlphaComponent(0.16)
            }
            pila.addArrangedSubview(boton)
        }
        reconstruirMenuAgregar()
    }

    private func menuPestana(_ indice: Int) -> UIMenu {
        guard vivas.indices.contains(indice) else { return UIMenu(children: []) }
        let registro = vivas[indice].registro
        let colores: [UInt32] = [0xFF0A84FF, 0xFF30D158, 0xFFFF9F0A,
                                  0xFFFF453A, 0xFFBF5AF2]
        let color = UIMenu(title: "Color", children: colores.enumerated().map { entrada in
            UIAction(title: "Color \(entrada.offset + 1)",
                     state: entrada.element == registro.colorARGB ? .on : .off) {
                [weak self] _ in self?.cambiarColor(indice, entrada.element)
            }
        })
        return UIMenu(children: [
            UIAction(title: "Renombrar", image: UIImage(systemName: "pencil")) {
                [weak self] _ in self?.pedirNombre(indice)
            },
            UIAction(title: registro.fijada ? "Desfijar" : "Fijar",
                     image: UIImage(systemName: "pin")) { [weak self] _ in
                self?.alternarFijada(indice)
            },
            color,
            UIAction(title: "Mover a la izquierda",
                     image: UIImage(systemName: "arrow.left")) { [weak self] _ in
                self?.mover(indice, delta: -1)
            },
            UIAction(title: "Mover a la derecha",
                     image: UIImage(systemName: "arrow.right")) { [weak self] _ in
                self?.mover(indice, delta: 1)
            },
            UIAction(title: "Duplicar", image: UIImage(systemName: "plus.square.on.square")) {
                [weak self] _ in self?.duplicar(indice)
            },
            UIAction(title: "Cerrar", image: UIImage(systemName: "xmark"),
                     attributes: registro.fijada ? [.disabled] : []) { [weak self] _ in
                self?.cerrar(indice)
            },
        ])
    }

    private func reconstruirMenuAgregar() {
        var acciones: [UIMenuElement] = [UIAction(
            title: "Nueva pestaña", image: UIImage(systemName: "plus")) {
                [weak self] _ in self?.nuevaPestana()
            }]
        if !estado.cerradas.isEmpty {
            acciones.append(UIMenu(title: "Restaurar cerrada",
                                   children: estado.cerradas.reversed().prefix(20).map { registro in
                UIAction(title: registro.titulo,
                         image: UIImage(systemName: "arrow.uturn.backward")) {
                    [weak self] _ in self?.restaurarCerrada(registro.id)
                }
            }))
        }
        agregar.menu = UIMenu(children: acciones)
    }

    @objc private func tocarPestana(_ boton: UIButton) { activar(boton.tag) }

    @discardableResult
    func activarPestana(id: UUID) -> Bool {
        guard let indice = vivas.firstIndex(where: { $0.registro.id == id }) else {
            return false
        }
        activar(indice)
        return indiceActivo == indice
    }

    private func activar(_ indice: Int, guardarAnterior: Bool = true) {
        guard vivas.indices.contains(indice) else { return }
        if guardarAnterior, vivas.indices.contains(indiceActivo) {
            do { try guardar(indiceActivo) }
            catch { presentarErrorPersistencia(error); return }
        }
        // Primero entrega todas las cachés inactivas y solo después reconstruye
        // la seleccionada; así el pico del cambio no suma dos canvas completos.
        for (i, viva) in vivas.enumerated() where i != indice {
            viva.navegacion.view.isHidden = true
            viva.lienzo.suspenderRecursosGraficos()
        }
        vivas[indice].navegacion.view.isHidden = false
        vivas[indice].lienzo.reanudarRecursosGraficos()
        indiceActivo = indice
        vivas[indice].registro.ultimoUso = Date()
        estado.activa = vivas[indice].registro.id
        sincronizarEstadoControlandoError(); reconstruirBarra()
        if let mensaje = vivas[indice].errorCarga {
            presentarErrorPersistencia(ErrorLecturaContenidoPestana(
                descripcion: mensaje))
        }
    }

    private func nuevaPestana(titulo: String = "Nueva nota") {
        guard !escrituraGlobalBloqueada else { return }
        if vivas.indices.contains(indiceActivo) {
            do { try guardar(indiceActivo) }
            catch { presentarErrorPersistencia(error); return }
        }
        let registro = nuevoRegistro(titulo: titulo)
        let viva = crearPestana(
            registro, persistida: false, recursosGraficosActivos: false)
        vivas.append(viva)
        do {
            try guardar(vivas.count - 1)
        } catch {
            _ = vivas.popLast()
            presentarErrorPersistencia(error)
            return
        }
        addChild(viva.navegacion)
        viva.navegacion.view.translatesAutoresizingMaskIntoConstraints = false
        viva.navegacion.view.isHidden = true
        contenedor.addSubview(viva.navegacion.view)
        NSLayoutConstraint.activate([
            viva.navegacion.view.leadingAnchor.constraint(equalTo: contenedor.leadingAnchor),
            viva.navegacion.view.trailingAnchor.constraint(equalTo: contenedor.trailingAnchor),
            viva.navegacion.view.topAnchor.constraint(equalTo: contenedor.topAnchor),
            viva.navegacion.view.bottomAnchor.constraint(equalTo: contenedor.bottomAnchor),
        ])
        viva.navegacion.didMove(toParent: self)
        activar(vivas.count - 1, guardarAnterior: false)
    }

    private func cerrar(_ indice: Int) {
        guard vivas.indices.contains(indice), !vivas[indice].registro.fijada else { return }
        do { try guardar(indice) }
        catch { presentarErrorPersistencia(error); return }
        let cerrada = vivas.remove(at: indice)
        cerrada.navegacion.willMove(toParent: nil)
        cerrada.navegacion.view.removeFromSuperview()
        cerrada.navegacion.removeFromParent()
        estado.cerradas.append(cerrada.registro)
        if estado.cerradas.count > 40 { estado.cerradas.removeFirst() }
        if vivas.isEmpty { nuevaPestana() }
        else { activar(min(indice, vivas.count - 1), guardarAnterior: false) }
    }

    private func restaurarCerrada(_ id: UUID) {
        guard let indice = estado.cerradas.firstIndex(where: { $0.id == id }) else { return }
        let registro = estado.cerradas.remove(at: indice)
        let viva = crearPestana(
            registro, persistida: true, recursosGraficosActivos: false)
        vivas.append(viva)
        addChild(viva.navegacion)
        viva.navegacion.view.translatesAutoresizingMaskIntoConstraints = false
        viva.navegacion.view.isHidden = true
        contenedor.addSubview(viva.navegacion.view)
        NSLayoutConstraint.activate([
            viva.navegacion.view.leadingAnchor.constraint(equalTo: contenedor.leadingAnchor),
            viva.navegacion.view.trailingAnchor.constraint(equalTo: contenedor.trailingAnchor),
            viva.navegacion.view.topAnchor.constraint(equalTo: contenedor.topAnchor),
            viva.navegacion.view.bottomAnchor.constraint(equalTo: contenedor.bottomAnchor),
        ])
        viva.navegacion.didMove(toParent: self)
        activar(vivas.count - 1)
    }

    private func duplicar(_ indice: Int) {
        guard vivas.indices.contains(indice) else { return }
        do {
            try guardar(indice)
        } catch {
            presentarErrorPersistencia(error)
            return
        }
        let origen = vivas[indice]
        let registro = nuevoRegistro(titulo: origen.registro.titulo + " copia")
        guard case .cargado(let datos) = repositorio.contenido(origen.registro)
        else {
            presentarErrorPersistencia(ErrorLecturaContenidoPestana(
                descripcion: "No se pudo leer la pestaña original para duplicarla."))
            return
        }
        do {
            try PreparadorCopiaDocumentoIPad.publicar(
                datos,
                destino: DocumentoPersistenteID(registro.id)
            ) { bytesCopia in
                try repositorio.guardarContenido(bytesCopia, registro: registro)
            }
            nuevaPestanaDesdeRegistro(registro)
        } catch {
            presentarErrorPersistencia(error)
        }
    }

    private func nuevaPestanaDesdeRegistro(_ registro: RegistroPestanaIPad) {
        let viva = crearPestana(
            registro, persistida: true, recursosGraficosActivos: false)
        vivas.append(viva)
        addChild(viva.navegacion)
        viva.navegacion.view.translatesAutoresizingMaskIntoConstraints = false
        viva.navegacion.view.isHidden = true; contenedor.addSubview(viva.navegacion.view)
        NSLayoutConstraint.activate([
            viva.navegacion.view.leadingAnchor.constraint(equalTo: contenedor.leadingAnchor),
            viva.navegacion.view.trailingAnchor.constraint(equalTo: contenedor.trailingAnchor),
            viva.navegacion.view.topAnchor.constraint(equalTo: contenedor.topAnchor),
            viva.navegacion.view.bottomAnchor.constraint(equalTo: contenedor.bottomAnchor),
        ])
        viva.navegacion.didMove(toParent: self); activar(vivas.count - 1)
    }

    private func pedirNombre(_ indice: Int) {
        guard vivas.indices.contains(indice) else { return }
        let alerta = UIAlertController(title: "Nombre de pestaña", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.text = self.vivas[indice].registro.titulo }
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self, let nombre = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines), !nombre.isEmpty else { return }
            self.vivas[indice].registro.titulo = nombre
            self.vivas[indice].lienzo.title = nombre
            self.sincronizarEstadoControlandoError()
            self.reconstruirBarra()
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func alternarFijada(_ indice: Int) {
        guard vivas.indices.contains(indice) else { return }
        vivas[indice].registro.fijada.toggle()
        sincronizarEstadoControlandoError(); reconstruirBarra()
    }

    private func cambiarColor(_ indice: Int, _ color: UInt32) {
        guard vivas.indices.contains(indice) else { return }
        vivas[indice].registro.colorARGB = color
        sincronizarEstadoControlandoError(); reconstruirBarra()
    }

    private func mover(_ indice: Int, delta: Int) {
        let destino = indice + delta
        guard vivas.indices.contains(indice), vivas.indices.contains(destino) else { return }
        vivas.swapAt(indice, destino)
        indiceActivo = destino
        sincronizarEstadoControlandoError(); reconstruirBarra()
    }

    private func guardar(_ indice: Int) throws {
        guard vivas.indices.contains(indice) else { return }
        guard !escrituraGlobalBloqueada,
              !vivas[indice].escrituraBloqueada else { return }
        vivas[indice].registro.vista = vivas[indice].lienzo.estadoVistaParaPestana
        guard let datos = vivas[indice].lienzo.datosPersistiblesDePestana() else {
            throw ErrorLecturaContenidoPestana(
                descripcion: "El motor de tinta no pudo preparar el contenido para guardar.")
        }
        try repositorio.guardarContenido(datos, registro: vivas[indice].registro)
    }

    @objc private func guardarTodas() {
        do { try guardarTodasConfirmando() }
        catch { presentarErrorPersistencia(error) }
    }

    private func guardarTodasConfirmando() throws {
        for indice in vivas.indices { try guardar(indice) }
        try sincronizarEstado()
        if panel == .principal {
            GestorArchivosCuadernU.compartido.marcarModificado(
                id: contextoEscena)
            PublicadorWidgetTintaIPad.actualizar()
        }
    }

    func guardarAhora() throws {
        // El modelo se carga durante init, separado del lifecycle de UIKit.
        // Solo las sesiones documentales materializadas necesitan checkpoint;
        // un panel diferido o un manifiesto dañado no se reescribe.
        guard errorCarga == nil, contenidoMaterializado else { return }
        try guardarTodasConfirmando()
    }

    @objc private func alActivarseAplicacion(_ notificacion: Notification) {
        consumirEntradasDelSistema()
        if panel == .principal { PublicadorWidgetTintaIPad.actualizar() }
    }

    private func consumirEntradasDelSistema() {
        guard !escrituraGlobalBloqueada, panel == .principal,
              vivas.indices.contains(indiceActivo) else { return }
        let procesadas = CoordinadorEntradasSistemaIPad.compartido.consumir {
            [weak self] entrada, adjunto in
            guard let self else { return false }
            switch entrada.tipo {
            case .nuevaNota:
                let limpio = entrada.texto?.trimmingCharacters(
                    in: .whitespacesAndNewlines)
                let titulo = limpio.flatMap { $0.isEmpty ? nil : $0 } ?? "Nueva nota"
                self.nuevaPestana(titulo: titulo)
                return true
            case .continuar:
                // RestaurarPestanas ya abrió la última pestaña, incluida su
                // vista exacta. Confirmar no modifica el documento.
                return true
            case .capturarFoto, .capturarVoz:
                return self.vivas[self.indiceActivo].lienzo
                    .ejecutarCapturaDelSistema(entrada.tipo)
            default:
                return ImportadorEntradaSistemaIPad.guardarEnBandeja(
                    entrada, adjunto: adjunto)
            }
        }
        if procesadas > 0 { PublicadorWidgetTintaIPad.actualizar() }
    }

    private func sincronizarEstado() throws {
        guard errorCarga == nil, contenidoMaterializado,
              !escrituraGlobalBloqueada else { return }
        estado.abiertas = vivas.map(\.registro)
        estado.activa = vivas.indices.contains(indiceActivo)
            ? vivas[indiceActivo].registro.id : nil
        try repositorio.guardar(
            estado, panel: panel, contexto: contextoEscena)
    }

    private func sincronizarEstadoControlandoError() {
        do { try sincronizarEstado() }
        catch { presentarErrorPersistencia(error) }
    }

    private func presentarErrorPersistencia(_ error: Error) {
        errorPersistenciaPendiente = error
        presentarErrorPersistenciaPendienteSiEsPosible()
    }

    private func presentarErrorPersistenciaPendienteSiEsPosible() {
        guard viewIfLoaded?.window != nil, presentedViewController == nil,
              !presentandoErrorPersistencia,
              let error = errorPersistenciaPendiente else { return }
        errorPersistenciaPendiente = nil
        presentandoErrorPersistencia = true
        let alerta = UIAlertController(
            title: "No se guardó el cuaderno",
            message: error.localizedDescription,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Entendido", style: .default) {
            [weak self] _ in
            self?.presentandoErrorPersistencia = false
            self?.presentarErrorPersistenciaPendienteSiEsPosible()
        })
        present(alerta, animated: true)
    }

    private func bloquearEdicionPorFalloInicial() {
        escrituraGlobalBloqueada = true
        agregar.isEnabled = false
        barra.isUserInteractionEnabled = false
        for viva in vivas {
            viva.lienzo.view.isUserInteractionEnabled = false
            viva.lienzo.navigationItem.prompt =
                "No se pudo crear el checkpoint inicial; edición bloqueada"
        }
    }
}

extension ControladorPestanasDocumentoIPad: UIDropInteractionDelegate {
    func dropInteraction(_ interaction: UIDropInteraction,
                         canHandle session: UIDropSession) -> Bool {
        session.hasItemsConforming(toTypeIdentifiers: [
            TipoArrastreTintaIPad.seleccion.identifier,
            UTType.image.identifier, UTType.pdf.identifier,
            UTType.fileURL.identifier, UTType.text.identifier])
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         sessionDidEnter session: UIDropSession) {
        guard let boton = interaction.view as? UIButton,
              vivas.indices.contains(boton.tag), boton.tag != indiceActivo else { return }
        activar(boton.tag)
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         sessionDidUpdate session: UIDropSession) -> UIDropProposal {
        UIDropProposal(operation: .copy)
    }

    func dropInteraction(_ interaction: UIDropInteraction,
                         performDrop session: UIDropSession) {
        guard let boton = interaction.view as? UIButton,
              vivas.indices.contains(boton.tag) else { return }
        if boton.tag != indiceActivo { activar(boton.tag) }
        vivas[boton.tag].lienzo.recibirItemsArrastrados(session.items)
    }
}
