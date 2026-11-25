import UIKit

private struct ErrorLecturaBibliotecaIPad: LocalizedError {
    let descripcion: String
    var errorDescription: String? { descripcion }
}

/// Un mismo cuaderno no puede tener dos escritores simultáneos en escenas del
/// proceso. Sin este lease, dos ventanas cargarían la misma revisión y el
/// último cierre reemplazaría silenciosamente el trabajo de la otra.
final class ArrendamientoCuadernoIPad {
    let cuadernoId: String
    private weak var centro: CentroAperturasCuadernosIPad?

    fileprivate init(cuadernoId: String,
                     centro: CentroAperturasCuadernosIPad) {
        self.cuadernoId = cuadernoId
        self.centro = centro
    }

    deinit { centro?.liberar(cuadernoId) }
}

final class CentroAperturasCuadernosIPad {
    static let compartido = CentroAperturasCuadernosIPad()
    private let bloqueo = NSLock()
    private var abiertos = Set<String>()

    func adquirir(_ cuadernoId: String) -> ArrendamientoCuadernoIPad? {
        bloqueo.lock()
        defer { bloqueo.unlock() }
        guard abiertos.insert(cuadernoId).inserted else { return nil }
        return ArrendamientoCuadernoIPad(cuadernoId: cuadernoId,
                                         centro: self)
    }

    fileprivate func liberar(_ cuadernoId: String) {
        bloqueo.lock()
        abiertos.remove(cuadernoId)
        bloqueo.unlock()
    }
}

protocol GestionBibliotecaCuadernU {
    func listarCuadernos() throws -> [CuadernoBibliotecaIPad]
    func crearCuaderno(titulo: String) throws -> CuadernoBibliotecaIPad
    func renombrarCuaderno(id: String, nombre: String) throws
    func actualizarColor(id: String, indiceColor: Int) throws
    func eliminarCuaderno(id: String) throws
}

extension GestorArchivosCuadernU: GestionBibliotecaCuadernU {}

/// Pantalla inicial nativa del iPad.
///
/// La versión SwiftUI que sustituyó esta pantalla sólo dibujaba la grilla. En
/// una instalación sin cuadernos la grilla tenía cero celdas y su botón de
/// creación estaba en un `toolbar` sin `NavigationStack`, por lo que el usuario
/// recibía una superficie vacía. Esta vista mantiene siempre una acción UIKit
/// visible y deja el estado vacío bajo prueba unitaria.
final class BibliotecaCuadernosIPad: UIViewController {
    private var cuadernos: [CuadernoBibliotecaIPad] = []
    private var cuadernosFiltrados: [CuadernoBibliotecaIPad] = []
    private var presentandoEditor = false
    private var errorPendiente: Error?
    private var presentandoError = false

    private let gestor: GestionBibliotecaCuadernU

    private lazy var coleccion: UICollectionView = {
        let vista = UICollectionView(frame: .zero,
                                     collectionViewLayout: crearLayout())
        vista.backgroundColor = .clear
        vista.delegate = self
        vista.dataSource = self
        vista.register(CeldaCuadernoIPad.self,
                       forCellWithReuseIdentifier: CeldaCuadernoIPad.reuseID)
        vista.accessibilityIdentifier = "biblioteca.cuadernos"
        return vista
    }()

    private let buscador: UISearchBar = {
        let vista = UISearchBar()
        vista.placeholder = "Buscar cuadernos…"
        vista.searchBarStyle = .minimal
        vista.accessibilityIdentifier = "biblioteca.buscar"
        return vista
    }()

    private let iconoVacio: UIImageView = {
        let vista = UIImageView(image: UIImage(
            systemName: "books.vertical.fill"))
        vista.tintColor = .systemIndigo
        vista.contentMode = .scaleAspectFit
        vista.preferredSymbolConfiguration = UIImage.SymbolConfiguration(
            pointSize: 54, weight: .semibold)
        return vista
    }()

    private let tituloVacio: UILabel = {
        let etiqueta = UILabel()
        etiqueta.text = "Empiece su primer cuaderno"
        etiqueta.font = .preferredFont(forTextStyle: .title2)
        etiqueta.adjustsFontForContentSizeCategory = true
        etiqueta.textAlignment = .center
        etiqueta.textColor = .label
        etiqueta.numberOfLines = 0
        etiqueta.accessibilityIdentifier = "biblioteca.vacia.titulo"
        return etiqueta
    }()

    private let detalleVacio: UILabel = {
        let etiqueta = UILabel()
        etiqueta.text = "Cree un cuaderno para escribir, importar material y grabar una Nota Viva."
        etiqueta.font = .preferredFont(forTextStyle: .body)
        etiqueta.adjustsFontForContentSizeCategory = true
        etiqueta.textAlignment = .center
        etiqueta.textColor = .secondaryLabel
        etiqueta.numberOfLines = 0
        return etiqueta
    }()

    private lazy var botonVacio: UIButton = {
        var configuracion = UIButton.Configuration.filled()
        configuracion.title = "Crear cuaderno"
        configuracion.image = UIImage(systemName: "plus")
        configuracion.imagePadding = 8
        configuracion.cornerStyle = .large
        let boton = UIButton(configuration: configuracion)
        boton.addTarget(self, action: #selector(solicitarCreacion),
                        for: .touchUpInside)
        boton.accessibilityIdentifier = "biblioteca.crear.primero"
        return boton
    }()

    private lazy var estadoVacio: UIStackView = {
        let pila = UIStackView(arrangedSubviews: [
            iconoVacio, tituloVacio, detalleVacio, botonVacio
        ])
        pila.axis = .vertical
        pila.alignment = .center
        pila.spacing = 14
        pila.setCustomSpacing(22, after: detalleVacio)
        pila.accessibilityIdentifier = "biblioteca.estado.vacio"
        return pila
    }()

    init(errorInicial: Error? = nil,
         gestor: GestionBibliotecaCuadernU = GestorArchivosCuadernU.compartido) {
        self.gestor = gestor
        errorPendiente = errorInicial
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { nil }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "CuadernU"
        view.backgroundColor = .systemGroupedBackground
        configurarNavegacion()
        configurarContenido()
        recargar()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        navigationController?.setNavigationBarHidden(false, animated: animated)
        recargar()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        presentarErrorPendienteSiHaceFalta()
        guard presentedViewController == nil,
              let escenaId = view.window?.windowScene?.session.persistentIdentifier
        else { return }
        CoordinadorEscenasCuadernUIPad.compartido
            .registrarBibliotecaVisible(escenaId: escenaId)
    }

    private func configurarNavegacion() {
        navigationController?.navigationBar.prefersLargeTitles = true
        navigationItem.largeTitleDisplayMode = .always
        let nuevo = UIBarButtonItem(
            image: UIImage(systemName: "plus.circle.fill"),
            style: .plain,
            target: self,
            action: #selector(solicitarCreacion))
        nuevo.accessibilityLabel = "Crear cuaderno nuevo"
        nuevo.accessibilityIdentifier = "biblioteca.crear.barra"
        navigationItem.rightBarButtonItem = nuevo
    }

    private func configurarContenido() {
        buscador.delegate = self
        buscador.translatesAutoresizingMaskIntoConstraints = false
        coleccion.translatesAutoresizingMaskIntoConstraints = false
        estadoVacio.translatesAutoresizingMaskIntoConstraints = false
        iconoVacio.translatesAutoresizingMaskIntoConstraints = false
        botonVacio.translatesAutoresizingMaskIntoConstraints = false

        view.addSubview(buscador)
        view.addSubview(coleccion)
        view.addSubview(estadoVacio)

        NSLayoutConstraint.activate([
            buscador.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            buscador.leadingAnchor.constraint(equalTo: view.leadingAnchor,
                                               constant: 16),
            buscador.trailingAnchor.constraint(equalTo: view.trailingAnchor,
                                                constant: -16),

            coleccion.topAnchor.constraint(equalTo: buscador.bottomAnchor,
                                            constant: 4),
            coleccion.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            coleccion.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            coleccion.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            estadoVacio.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            estadoVacio.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
            estadoVacio.widthAnchor.constraint(lessThanOrEqualTo: view.widthAnchor,
                                                multiplier: 0.72),
            estadoVacio.widthAnchor.constraint(lessThanOrEqualToConstant: 520),
            iconoVacio.widthAnchor.constraint(equalToConstant: 76),
            iconoVacio.heightAnchor.constraint(equalToConstant: 76),
            botonVacio.heightAnchor.constraint(greaterThanOrEqualToConstant: 48),
        ])
    }

    private func crearLayout() -> UICollectionViewCompositionalLayout {
        UICollectionViewCompositionalLayout { _, entorno in
            let ancho = entorno.container.effectiveContentSize.width
            let columnas = max(1, Int(ancho / 210))
            let tamanoItem = NSCollectionLayoutSize(
                widthDimension: .fractionalWidth(1 / CGFloat(columnas)),
                heightDimension: .estimated(290))
            let item = NSCollectionLayoutItem(layoutSize: tamanoItem)
            item.contentInsets = NSDirectionalEdgeInsets(
                top: 8, leading: 12, bottom: 8, trailing: 12)
            let tamanoGrupo = NSCollectionLayoutSize(
                widthDimension: .fractionalWidth(1),
                heightDimension: .estimated(290))
            let grupo = NSCollectionLayoutGroup.horizontal(
                layoutSize: tamanoGrupo,
                repeatingSubitem: item,
                count: columnas)
            let seccion = NSCollectionLayoutSection(group: grupo)
            seccion.contentInsets = NSDirectionalEdgeInsets(
                top: 8, leading: 16, bottom: 24, trailing: 16)
            return seccion
        }
    }

    private func recargar() {
        do {
            cuadernos = try gestor.listarCuadernos()
            aplicarFiltro()
        } catch {
            cuadernos = []
            cuadernosFiltrados = []
            coleccion.reloadData()
            actualizarEstadoVacio()
            errorPendiente = error
            presentarErrorPendienteSiHaceFalta()
        }
    }

    private func aplicarFiltro() {
        let texto = buscador.text?.trimmingCharacters(
            in: .whitespacesAndNewlines) ?? ""
        if texto.isEmpty {
            cuadernosFiltrados = cuadernos
        } else {
            cuadernosFiltrados = cuadernos.filter {
                $0.titulo.localizedCaseInsensitiveContains(texto)
            }
        }
        coleccion.reloadData()
        actualizarEstadoVacio()
    }

    private func actualizarEstadoVacio() {
        let consulta = buscador.text?.trimmingCharacters(
            in: .whitespacesAndNewlines) ?? ""
        let sinResultados = !consulta.isEmpty && cuadernosFiltrados.isEmpty
        estadoVacio.isHidden = !cuadernosFiltrados.isEmpty
        if sinResultados {
            iconoVacio.image = UIImage(systemName: "magnifyingglass")
            tituloVacio.text = "No se encontraron cuadernos"
            detalleVacio.text = "Pruebe otro nombre o borre el texto de búsqueda."
            botonVacio.isHidden = true
        } else {
            iconoVacio.image = UIImage(systemName: "books.vertical.fill")
            tituloVacio.text = "Empiece su primer cuaderno"
            detalleVacio.text = "Cree un cuaderno para escribir, importar material y grabar una Nota Viva."
            botonVacio.isHidden = false
        }
    }

    private func cuaderno(en indexPath: IndexPath) -> CuadernoBibliotecaIPad? {
        guard cuadernosFiltrados.indices.contains(indexPath.item) else {
            return nil
        }
        return cuadernosFiltrados[indexPath.item]
    }

    @objc private func solicitarCreacion() {
        let alerta = UIAlertController(
            title: "Nuevo cuaderno",
            message: "El nombre se puede cambiar después.",
            preferredStyle: .alert)
        alerta.addTextField {
            $0.placeholder = "Nombre del cuaderno"
            $0.autocapitalizationType = .words
            $0.clearButtonMode = .whileEditing
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self, weak alerta] _ in
            guard let self else { return }
            let nombre = alerta?.textFields?.first?.text ?? ""
            do {
                let creado = try self.gestor.crearCuaderno(titulo: nombre)
                self.recargar()
                let abrir: () -> Void = { [weak self] in
                    self?.abrirCuaderno(creado)
                }
                // Presentar en el siguiente ciclo no garantiza que la
                // animación automática del UIAlertController haya terminado.
                // El completion de un cierre explícito sí establece el punto
                // exacto en el que UIKit permite presentar el editor.
                if let alerta, alerta.presentingViewController != nil {
                    alerta.dismiss(animated: true, completion: abrir)
                } else {
                    abrir()
                }
            } catch {
                let presentarError: () -> Void = { [weak self] in
                    self?.mostrar(error)
                }
                if let alerta, alerta.presentingViewController != nil {
                    alerta.dismiss(animated: true, completion: presentarError)
                } else {
                    presentarError()
                }
            }
        })
        present(alerta, animated: true)
    }

    private func abrirCuaderno(_ info: CuadernoBibliotecaIPad) {
        guard !presentandoEditor, presentedViewController == nil else { return }
        guard let arrendamiento = CentroAperturasCuadernosIPad.compartido
            .adquirir(info.id) else {
            let error = ErrorLecturaBibliotecaIPad(
                descripcion: "Este cuaderno ya está abierto en otra ventana. Ciérrelo allí antes de editarlo aquí.")
            mostrar(error)
            return
        }
        presentandoEditor = true
        let controlador = ControladorRaizIPad(
            contextoEscena: info.id, arrendamiento: arrendamiento)
        controlador.title = info.titulo
        controlador.modalPresentationStyle = .fullScreen

        if let escenaId = view.window?.windowScene?.session.persistentIdentifier {
            _ = CoordinadorEscenasCuadernUIPad.compartido.registrarApertura(
                cuadernoId: info.id, escenaId: escenaId)
        }

        // UIKit no admite introducir un UISplitViewController mediante push en
        // un UINavigationController. El editor es un root modal de pantalla
        // completa y conserva su propia jerarquía de dos columnas.
        present(controlador, animated: true) { [weak self] in
            self?.presentandoEditor = false
        }
    }

    private func solicitarRenombrado(_ info: CuadernoBibliotecaIPad) {
        let alerta = UIAlertController(title: "Renombrar cuaderno",
                                       message: nil, preferredStyle: .alert)
        alerta.addTextField {
            $0.text = info.titulo
            $0.autocapitalizationType = .words
            $0.clearButtonMode = .whileEditing
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self, weak alerta] _ in
            guard let self else { return }
            do {
                try self.gestor.renombrarCuaderno(
                    id: info.id,
                    nombre: alerta?.textFields?.first?.text ?? "")
                self.recargar()
            } catch {
                self.mostrar(error, despuesDeCerrar: alerta)
            }
        })
        present(alerta, animated: true)
    }

    private func solicitarColor(_ info: CuadernoBibliotecaIPad,
                                desde vista: UIView) {
        let alerta = UIAlertController(title: "Color de portada", message: nil,
                                       preferredStyle: .actionSheet)
        for (indice, nombre) in CeldaCuadernoIPad.nombresColoresPortada.enumerated() {
            alerta.addAction(UIAlertAction(title: nombre, style: .default) {
                [weak self, weak alerta] _ in
                guard let self else { return }
                do {
                    try self.gestor.actualizarColor(
                        id: info.id, indiceColor: indice)
                    self.recargar()
                } catch {
                    self.mostrar(error, despuesDeCerrar: alerta)
                }
            })
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = vista
        alerta.popoverPresentationController?.sourceRect = vista.bounds
        present(alerta, animated: true)
    }

    private func solicitarEliminacion(_ info: CuadernoBibliotecaIPad) {
        let alerta = UIAlertController(
            title: "Eliminar «\(info.titulo)»",
            message: "Se eliminarán sus pestañas y contenido guardado. Esta acción no se puede deshacer.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Eliminar", style: .destructive) {
            [weak self, weak alerta] _ in
            guard let self else { return }
            do {
                try self.gestor.eliminarCuaderno(id: info.id)
                self.recargar()
            } catch {
                self.mostrar(error, despuesDeCerrar: alerta)
            }
        })
        present(alerta, animated: true)
    }

    private func mostrar(_ error: Error) {
        errorPendiente = error
        presentarErrorPendienteSiHaceFalta()
    }

    private func mostrar(_ error: Error,
                         despuesDeCerrar alerta: UIAlertController?) {
        let presentar: () -> Void = { [weak self] in self?.mostrar(error) }
        if let alerta, alerta.presentingViewController != nil {
            alerta.dismiss(animated: true, completion: presentar)
        } else {
            presentar()
        }
    }

    private func presentarErrorPendienteSiHaceFalta() {
        guard viewIfLoaded?.window != nil, presentedViewController == nil,
              !presentandoError,
              let error = errorPendiente else { return }
        errorPendiente = nil
        presentandoError = true
        let alerta = UIAlertController(
            title: "No se pudo completar la operación",
            message: error.localizedDescription,
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default) {
            [weak self] _ in
            self?.presentandoError = false
            self?.presentarErrorPendienteSiHaceFalta()
        })
        present(alerta, animated: true)
    }
}

extension BibliotecaCuadernosIPad: UICollectionViewDataSource {
    func collectionView(_ collectionView: UICollectionView,
                        numberOfItemsInSection section: Int) -> Int {
        cuadernosFiltrados.count
    }

    func collectionView(_ collectionView: UICollectionView,
                        cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        guard let celda = collectionView.dequeueReusableCell(
            withReuseIdentifier: CeldaCuadernoIPad.reuseID,
            for: indexPath) as? CeldaCuadernoIPad,
              let info = cuaderno(en: indexPath) else {
            return UICollectionViewCell()
        }
        celda.configurar(titulo: info.titulo,
                         fecha: info.fechaModificacion,
                         indiceColor: info.indiceColor)
        celda.accessibilityIdentifier = "biblioteca.cuaderno.\(info.id)"
        return celda
    }
}

extension BibliotecaCuadernosIPad: UICollectionViewDelegate {
    func collectionView(_ collectionView: UICollectionView,
                        didSelectItemAt indexPath: IndexPath) {
        collectionView.deselectItem(at: indexPath, animated: true)
        guard let info = cuaderno(en: indexPath) else { return }
        abrirCuaderno(info)
    }

    func collectionView(_ collectionView: UICollectionView,
                        contextMenuConfigurationForItemsAt indexPaths: [IndexPath],
                        point: CGPoint) -> UIContextMenuConfiguration? {
        guard let indexPath = indexPaths.first,
              let info = cuaderno(en: indexPath) else { return nil }
        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) {
            [weak self, weak collectionView] _ in
            let renombrar = UIAction(
                title: "Renombrar", image: UIImage(systemName: "pencil")) {
                    [weak self] _ in self?.solicitarRenombrado(info)
                }
            let color = UIAction(
                title: "Color de portada",
                image: UIImage(systemName: "paintpalette")) { [weak self] _ in
                    guard let self else { return }
                    guard let origen = collectionView?.cellForItem(at: indexPath)
                        ?? self.viewIfLoaded else { return }
                    self.solicitarColor(info, desde: origen)
                }
            let eliminar = UIAction(
                title: "Eliminar", image: UIImage(systemName: "trash"),
                attributes: .destructive) { [weak self] _ in
                    self?.solicitarEliminacion(info)
                }
            return UIMenu(title: info.titulo,
                          children: [renombrar, color, eliminar])
        }
    }
}

extension BibliotecaCuadernosIPad: UISearchBarDelegate {
    func searchBar(_ searchBar: UISearchBar, textDidChange searchText: String) {
        aplicarFiltro()
    }

    func searchBarSearchButtonClicked(_ searchBar: UISearchBar) {
        searchBar.resignFirstResponder()
    }

    func searchBarCancelButtonClicked(_ searchBar: UISearchBar) {
        searchBar.text = ""
        aplicarFiltro()
        searchBar.resignFirstResponder()
    }
}
