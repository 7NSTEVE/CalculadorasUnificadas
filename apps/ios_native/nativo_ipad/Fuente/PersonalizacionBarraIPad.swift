import UIKit

final class BotonSateliteBarraIPad: UIButton {
    let sateliteID: UUID
    let accion: AccionIPad

    init(_ satelite: HerramientaSateliteIPad) {
        sateliteID = satelite.id
        accion = satelite.accion
        var configuracion = UIButton.Configuration.filled()
        configuracion.image = UIImage(systemName: satelite.accion.simbolo)
        configuracion.cornerStyle = .capsule
        configuracion.baseBackgroundColor = .secondarySystemBackground
        configuracion.baseForegroundColor = .label
        configuracion.contentInsets = NSDirectionalEdgeInsets(
            top: 12, leading: 12, bottom: 12, trailing: 12)
        // Evitar el inicializador Swift `UIButton(configuration:primaryAction:)`.
        // En GitHub Actions, Xcode 26.6 enlazó ese símbolo desde el SDK 26.5,
        // pero el runtime 26.4 del simulador no lo contenía y la app abortó antes
        // incluso de arrancar XCTest. El inicializador Objective-C estable más
        // asignación de la propiedad produce la misma interfaz sin esa dependencia.
        super.init(frame: .zero)
        self.configuration = configuracion
        accessibilityLabel = "Herramienta flotante: \(satelite.accion.nombre)"
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOpacity = 0.18
        layer.shadowRadius = 8
        layer.shadowOffset = CGSize(width: 0, height: 3)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
}

/// Editor explícito de la barra. Ninguna sugerencia basada en uso modifica la
/// disposición por sí sola: agregar, quitar, ordenar, ocultar y desprender son
/// decisiones directas de la persona.
final class PanelPersonalizacionBarraIPad: UITableViewController {
    var alCambiar: ((InstantaneaExperienciaIPad) -> Void)?

    private var estado = CentroExperienciaIPad.compartido.instantanea()

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Personalizar barra"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = editButtonItem
        tableView.cellLayoutMarginsFollowReadableWidth = true
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 5 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return UbicacionBarraIPad.allCases.count
        case 1: return 3
        case 2: return estado.ordenBarra.count + 1
        case 3: return estado.barra.satelites.count + 1
        default: return 1
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Posición", "Tamaño y visibilidad", "Herramientas de la barra",
         "Herramientas satélite", "Recuperación"][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return "En modo flotante, arrastre la barra hasta el sitio que prefiera."
        case 1:
            return "Si la oculta, queda un botón pequeño para recuperarla o abrir este editor."
        case 2:
            return "Use Editar para arrastrar el orden o eliminar botones. Las funciones eliminadas siguen disponibles en Más."
        case 3:
            return "Cada satélite es un botón independiente: se puede arrastrar, ejecutar o volver a acoplar."
        default:
            return "Restaurar no modifica el documento, el papel ni los trazos."
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            let ubicacion = UbicacionBarraIPad.allCases[indexPath.row]
            celda.textLabel?.text = ubicacion.nombre
            celda.imageView?.image = UIImage(systemName: ubicacion.simbolo)
            celda.accessoryType = estado.barra.ubicacion == ubicacion
                ? .checkmark : .none
        case 1:
            let opciones = [
                ("Maximizada", "Controles y accesos elegidos", "rectangle.expand.vertical"),
                ("Minimizada", "Herramienta activa, accesos prioritarios y Más", "rectangle.compress.vertical"),
                ("Oculta", "Solo queda el botón de recuperación", "eye.slash"),
            ]
            let opcion = opciones[indexPath.row]
            celda.textLabel?.text = opcion.0
            celda.detailTextLabel?.text = opcion.1
            celda.imageView?.image = UIImage(systemName: opcion.2)
            let activa: Bool
            switch indexPath.row {
            case 0: activa = estado.interfaz.mostrarBarra
                && !estado.interfaz.barraCompacta
            case 1: activa = estado.interfaz.mostrarBarra
                && estado.interfaz.barraCompacta
            default: activa = !estado.interfaz.mostrarBarra
            }
            celda.accessoryType = activa ? .checkmark : .none
        case 2 where indexPath.row == estado.ordenBarra.count:
            celda.textLabel?.text = "Agregar herramientas…"
            celda.detailTextLabel?.text = "Buscar entre todas las funciones reales de Tinta"
            celda.imageView?.image = UIImage(systemName: "plus.circle")
            celda.accessoryType = .disclosureIndicator
        case 2:
            let accion = estado.ordenBarra[indexPath.row]
            celda.textLabel?.text = accion.nombre
            celda.imageView?.image = UIImage(systemName: accion.simbolo)
            celda.detailTextLabel?.text = "Posición \(indexPath.row + 1)"
        case 3 where indexPath.row == estado.barra.satelites.count:
            celda.textLabel?.text = "Agregar herramienta satélite…"
            celda.detailTextLabel?.text = "Botón flotante independiente tipo AssistiveTouch"
            celda.imageView?.image = UIImage(systemName: "plus.circle")
            celda.accessoryType = .disclosureIndicator
        case 3:
            let satelite = estado.barra.satelites[indexPath.row]
            celda.textLabel?.text = satelite.accion.nombre
            celda.detailTextLabel?.text = "Arrástrela directamente sobre el lienzo"
            celda.imageView?.image = UIImage(systemName: satelite.accion.simbolo)
        default:
            celda.textLabel?.text = "Restaurar barra predeterminada"
            celda.detailTextLabel?.text = "Arriba, minimizada, sin satélites"
            celda.imageView?.image = UIImage(systemName: "arrow.counterclockwise")
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        defer { tableView.deselectRow(at: indexPath, animated: true) }
        switch indexPath.section {
        case 0:
            var barra = estado.barra
            barra.ubicacion = UbicacionBarraIPad.allCases[indexPath.row]
            CentroExperienciaIPad.compartido.actualizarBarra(barra)
            cambio(secciones: IndexSet(integer: 0))
        case 1:
            var interfaz = estado.interfaz
            interfaz.mostrarBarra = indexPath.row != 2
            interfaz.barraCompacta = indexPath.row == 1
            CentroExperienciaIPad.compartido.actualizarInterfaz(interfaz)
            cambio(secciones: IndexSet(integer: 1))
        case 2 where indexPath.row == estado.ordenBarra.count:
            abrirSelector(destino: .barra)
        case 3 where indexPath.row == estado.barra.satelites.count:
            abrirSelector(destino: .satelite)
        case 4:
            confirmarRestauracion()
        default: break
        }
    }

    override func tableView(_ tableView: UITableView,
                            canEditRowAt indexPath: IndexPath) -> Bool {
        (indexPath.section == 2 && indexPath.row < estado.ordenBarra.count)
            || (indexPath.section == 3
                && indexPath.row < estado.barra.satelites.count)
    }

    override func tableView(_ tableView: UITableView,
                            editingStyleForRowAt indexPath: IndexPath)
        -> UITableViewCell.EditingStyle {
        .delete
    }

    override func tableView(_ tableView: UITableView,
                            commit editingStyle: UITableViewCell.EditingStyle,
                            forRowAt indexPath: IndexPath) {
        guard editingStyle == .delete else { return }
        if indexPath.section == 2 {
            var acciones = estado.ordenBarra
            acciones.remove(at: indexPath.row)
            CentroExperienciaIPad.compartido.guardarOrdenBarra(acciones)
            cambio(secciones: IndexSet(integer: 2))
        } else if indexPath.section == 3 {
            var barra = estado.barra
            barra.satelites.remove(at: indexPath.row)
            CentroExperienciaIPad.compartido.actualizarBarra(barra)
            cambio(secciones: IndexSet(integer: 3))
        }
    }

    override func tableView(_ tableView: UITableView,
                            canMoveRowAt indexPath: IndexPath) -> Bool {
        indexPath.section == 2 && indexPath.row < estado.ordenBarra.count
    }

    override func tableView(_ tableView: UITableView,
                            moveRowAt sourceIndexPath: IndexPath,
                            to destinationIndexPath: IndexPath) {
        guard sourceIndexPath.section == 2,
              destinationIndexPath.section == 2,
              sourceIndexPath.row < estado.ordenBarra.count else {
            tableView.reloadData()
            return
        }
        var acciones = estado.ordenBarra
        let accion = acciones.remove(at: sourceIndexPath.row)
        let destino = min(destinationIndexPath.row, acciones.count)
        acciones.insert(accion, at: destino)
        CentroExperienciaIPad.compartido.guardarOrdenBarra(acciones)
        cambio(secciones: IndexSet(integer: 2))
    }

    override func tableView(_ tableView: UITableView,
                            targetIndexPathForMoveFromRowAt sourceIndexPath: IndexPath,
                            toProposedIndexPath proposedDestinationIndexPath: IndexPath)
        -> IndexPath {
        guard proposedDestinationIndexPath.section == 2 else {
            return sourceIndexPath
        }
        return IndexPath(row: min(proposedDestinationIndexPath.row,
                                  max(estado.ordenBarra.count - 1, 0)),
                         section: 2)
    }

    private enum DestinoSelector { case barra, satelite }

    private func abrirSelector(destino: DestinoSelector) {
        let seleccionadas: Set<AccionIPad> = destino == .barra
            ? Set(estado.ordenBarra)
            : Set(estado.barra.satelites.map(\.accion))
        let selector = SelectorAccionesBarraIPad(
            titulo: destino == .barra ? "Herramientas de la barra" : "Satélites",
            seleccionadas: seleccionadas)
        selector.alAlternar = { [weak self] accion, seleccionada in
            guard let self else { return }
            switch destino {
            case .barra:
                var acciones = self.estado.ordenBarra
                acciones.removeAll { $0 == accion }
                if seleccionada { acciones.append(accion) }
                CentroExperienciaIPad.compartido.guardarOrdenBarra(acciones)
                self.cambio(secciones: IndexSet(integer: 2))
            case .satelite:
                var barra = self.estado.barra
                barra.satelites.removeAll { $0.accion == accion }
                if seleccionada {
                    let indice = barra.satelites.count
                    let columna = indice % 2
                    let fila = indice / 2
                    barra.satelites.append(HerramientaSateliteIPad(
                        accion: accion,
                        x: columna == 0 ? 0.88 : 0.78,
                        y: min(0.20 + Double(fila) * 0.10, 0.82)))
                }
                CentroExperienciaIPad.compartido.actualizarBarra(barra)
                self.cambio(secciones: IndexSet(integer: 3))
            }
        }
        navigationController?.pushViewController(selector, animated: true)
    }

    private func confirmarRestauracion() {
        let alerta = UIAlertController(
            title: "¿Restaurar la barra?",
            message: "Se restablecen posición, tamaño, orden y satélites. El documento no cambia.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Restaurar", style: .destructive) {
            [weak self] _ in
            guard let self else { return }
            self.estado = CentroExperienciaIPad.compartido.restaurarBarra()
            self.tableView.reloadData()
            self.alCambiar?(self.estado)
        })
        present(alerta, animated: true)
    }

    private func cambio(secciones: IndexSet) {
        estado = CentroExperienciaIPad.compartido.instantanea()
        tableView.reloadSections(secciones, with: .none)
        alCambiar?(estado)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

private final class SelectorAccionesBarraIPad: UITableViewController,
    UISearchResultsUpdating {
    var alAlternar: ((AccionIPad, Bool) -> Void)?

    private var seleccionadas: Set<AccionIPad>
    private var consulta = ""
    private let tituloPanel: String

    init(titulo: String, seleccionadas: Set<AccionIPad>) {
        tituloPanel = titulo
        self.seleccionadas = seleccionadas
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    private var acciones: [AccionIPad] {
        let ordenadas = AccionIPad.allCases.sorted {
            $0.nombre.localizedCaseInsensitiveCompare($1.nombre) == .orderedAscending
        }
        guard !consulta.isEmpty else { return ordenadas }
        let buscada = Normalizador.texto(consulta)
        return ordenadas.filter {
            Normalizador.texto($0.nombre + " " + $0.palabrasClave).contains(buscada)
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = tituloPanel
        let buscador = UISearchController(searchResultsController: nil)
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        buscador.searchBar.placeholder = "Buscar herramienta"
        navigationItem.searchController = buscador
        navigationItem.hidesSearchBarWhenScrolling = false
        definesPresentationContext = true
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        acciones.count
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let accion = acciones[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = accion.nombre
        celda.detailTextLabel?.text = accion.palabrasClave
        celda.imageView?.image = UIImage(systemName: accion.simbolo)
        celda.accessoryType = seleccionadas.contains(accion) ? .checkmark : .none
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        let accion = acciones[indexPath.row]
        let seleccionada: Bool
        if seleccionadas.remove(accion) != nil {
            seleccionada = false
        } else {
            seleccionadas.insert(accion)
            seleccionada = true
        }
        tableView.reloadRows(at: [indexPath], with: .none)
        alAlternar?(accion, seleccionada)
    }

    func updateSearchResults(for searchController: UISearchController) {
        consulta = searchController.searchBar.text?
            .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        tableView.reloadData()
    }
}
