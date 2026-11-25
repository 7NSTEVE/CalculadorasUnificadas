import UniformTypeIdentifiers
import UIKit

@MainActor
final class PanelComparativaGoodnotesIPad: UITableViewController,
                                           UIDocumentPickerDelegate {
    private let repositorio: RepositorioComparativaGoodnotesIPad
    private var resumen: ResumenComparativaGoodnotesIPad
    private var reporte: ReporteSuperioridadGoodnotesIPad

    init(repositorio: RepositorioComparativaGoodnotesIPad = .compartido) {
        self.repositorio = repositorio
        resumen = repositorio.resumen()
        reporte = repositorio.reporte()
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "QA vs Goodnotes"
        view.accessibilityIdentifier = "qa.goodnotes.panel"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "ellipsis.circle"),
            menu: menuAcciones())
    }

    private func menuAcciones() -> UIMenu {
        var acciones: [UIMenuElement] = [
            UIAction(title: "Exportar evidencia JSON",
                     image: UIImage(systemName: "square.and.arrow.up")) {
                [weak self] _ in self?.exportar()
            },
            UIAction(title: "Importar evidencia JSON",
                     image: UIImage(systemName: "square.and.arrow.down")) {
                [weak self] _ in self?.importar()
            },
        ]
#if DEBUG
        acciones += [
            UIAction(title: "Añadir muestra manual",
                     image: UIImage(systemName: "plus.circle")) {
                [weak self] _ in self?.pedirMuestraManual()
            },
            UIAction(title: "Añadir evidencia funcional",
                     image: UIImage(systemName: "checklist")) {
                [weak self] _ in self?.pedirFuncionManual()
            },
        ]
#endif
        acciones.append(UIAction(
            title: "Reiniciar evidencia",
            image: UIImage(systemName: "trash"),
            attributes: .destructive) { [weak self] _ in
                self?.confirmarReinicio()
            })
        return UIMenu(children: acciones)
    }

    private func recargar() {
        resumen = repositorio.resumen()
        reporte = repositorio.reporte()
        tableView.reloadData()
        navigationItem.rightBarButtonItem?.menu = menuAcciones()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 5 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0: return "Veredicto"
        case 1: return "Rendimiento comparable"
        case 2: return "Cobertura funcional"
        case 3: return "Comparaciones"
        default: return "Qué falta para poder afirmarlo"
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        if section == 0 {
            return "«Supera» sólo aparece cuando el gate conservador tiene muestras recientes suficientes, paridad completa con las capacidades Goodnotes comparadas, ninguna regresión inaceptable y diferenciadores exclusivos verificados."
        }
        if section == 1 {
            return "Una serie sólo es comparable si CuadernU y Goodnotes se midieron con el mismo escenario, dispositivo y versión de sistema y alcanzan el mínimo de muestras configurado."
        }
        return nil
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 1
        case 1: return 4
        case 2: return 3
        case 3: return max(reporte.comparaciones.count, 1)
        default: return max(reporte.faltantes.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.detailTextLabel?.numberOfLines = 0
        switch indexPath.section {
        case 0:
            celda.textLabel?.text = tituloVeredicto(reporte.veredicto)
            celda.detailTextLabel?.text = detalleVeredicto(reporte.veredicto)
            celda.imageView?.image = UIImage(systemName: simboloVeredicto(reporte.veredicto))
            celda.selectionStyle = .none
        case 1:
            let filas: [(String, String)] = [
                ("Series comparables", "\(resumen.seriesComparables) de \(resumen.seriesTotales)"),
                ("Muestras CuadernU", "\(resumen.muestrasCuadernU)"),
                ("Muestras Goodnotes", "\(resumen.muestrasGoodnotes)"),
                ("Regresiones inaceptables", "\(reporte.comparaciones.filter(\.regresionInaceptable).count)"),
            ]
            celda.textLabel?.text = filas[indexPath.row].0
            celda.detailTextLabel?.text = filas[indexPath.row].1
            celda.selectionStyle = .none
        case 2:
            let filas: [(String, String)] = [
                ("Capacidades Goodnotes comparadas", "\(resumen.funcionesGoodnotes)"),
                ("Funciones CuadernU verificadas", "\(resumen.funcionesCuadernUVerificadas)"),
                ("Diferenciadores exclusivos verificados", "\(resumen.diferenciadoresExclusivosVerificados)"),
            ]
            celda.textLabel?.text = filas[indexPath.row].0
            celda.detailTextLabel?.text = filas[indexPath.row].1
            celda.selectionStyle = .none
        case 3:
            guard !reporte.comparaciones.isEmpty else {
                celda.textLabel?.text = "Aún no hay métricas comparables"
                celda.detailTextLabel?.text = "Importe o mida ambos productos bajo el mismo escenario."
                celda.selectionStyle = .none
                return celda
            }
            let comparacion = reporte.comparaciones[indexPath.row]
            celda.textLabel?.text = comparacion.id
            celda.detailTextLabel?.text = String(
                format: "CuadernU %.3f · Goodnotes %.3f · ventaja %.2f %%",
                comparacion.medianaCuadernU,
                comparacion.medianaGoodnotes,
                comparacion.ventajaCuadernUPorcentaje)
            celda.imageView?.image = UIImage(systemName:
                comparacion.regresionInaceptable
                    ? "exclamationmark.triangle.fill"
                    : (comparacion.cuentaComoVentaja
                        ? "arrow.up.right.circle.fill"
                        : "equal.circle"))
            celda.selectionStyle = .none
        default:
            guard !reporte.faltantes.isEmpty else {
                celda.textLabel?.text = "El gate no reporta evidencia faltante"
                celda.detailTextLabel?.text = reporte.permiteAfirmarSuperioridad
                    ? "La evidencia actual permite la afirmación bajo la política configurada."
                    : "Revise las comparaciones y regresiones."
                celda.selectionStyle = .none
                return celda
            }
            celda.textLabel?.text = reporte.faltantes[indexPath.row]
            celda.imageView?.image = UIImage(systemName: "circle.dashed")
            celda.selectionStyle = .none
        }
        return celda
    }

    private func tituloVeredicto(_ valor: VeredictoSuperioridadGoodnotesIPad) -> String {
        switch valor {
        case .evidenciaInsuficiente: return "Evidencia insuficiente"
        case .noSupera: return "Aún no supera Goodnotes bajo este gate"
        case .supera: return "Supera Goodnotes bajo la evidencia cargada"
        }
    }

    private func detalleVeredicto(_ valor: VeredictoSuperioridadGoodnotesIPad) -> String {
        switch valor {
        case .evidenciaInsuficiente:
            return "CuadernU no hará una afirmación de superioridad hasta completar el corpus mínimo."
        case .noSupera:
            return "Hay evidencia suficiente para evaluar, pero existe una brecha funcional, una regresión o falta ventaja de rendimiento."
        case .supera:
            return "El gate pasó. Este resultado sigue dependiendo del dispositivo, OS, escenarios y fecha de la evidencia persistida."
        }
    }

    private func simboloVeredicto(_ valor: VeredictoSuperioridadGoodnotesIPad) -> String {
        switch valor {
        case .evidenciaInsuficiente: return "questionmark.diamond"
        case .noSupera: return "xmark.seal"
        case .supera: return "checkmark.seal.fill"
        }
    }

    private func exportar() {
        do {
            let datos = try repositorio.exportar()
            let destino = FileManager.default.temporaryDirectory
                .appendingPathComponent("CuadernU-vs-Goodnotes-\(selloFecha()).json")
            try datos.write(to: destino, options: .atomic)
            let compartir = UIActivityViewController(
                activityItems: [destino], applicationActivities: nil)
            compartir.completionWithItemsHandler = { _, _, _, _ in
                try? FileManager.default.removeItem(at: destino)
            }
            compartir.popoverPresentationController?.sourceView = view
            present(compartir, animated: true)
        } catch {
            avisar(error.localizedDescription)
        }
    }

    private func importar() {
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: [.json], asCopy: true)
        selector.delegate = self
        selector.allowsMultipleSelection = false
        present(selector, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else { return }
        do {
            let datos = try Data(contentsOf: url, options: .mappedIfSafe)
            guard datos.count <= 16 * 1_024 * 1_024 else {
                throw NSError(
                    domain: "CuadernU.QA.Goodnotes",
                    code: 1,
                    userInfo: [NSLocalizedDescriptionKey:
                        "El archivo comparativo supera el límite de 16 MB."])
            }
            try repositorio.importar(datos)
            recargar()
            avisar("Evidencia importada y revalidada. No se aceptaron muestras implícitas ni escenarios mezclados.")
        } catch {
            avisar("No se importó la evidencia: \(error.localizedDescription)")
        }
    }

#if DEBUG
    private func pedirMuestraManual() {
        let alerta = UIAlertController(
            title: "Muestra comparativa QA",
            message: "Formato: producto | dirección | métrica | escenario | dispositivo | sistema | valor\nProducto: cuadernu/goodnotes · dirección: menor/mayor",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "cuadernu" }
        alerta.addTextField { $0.placeholder = "menor" }
        alerta.addTextField { $0.placeholder = "p95_ms" }
        alerta.addTextField { $0.placeholder = "Pencil huge-doc 500p" }
        alerta.addTextField { $0.placeholder = "iPad16,6" }
        alerta.addTextField { $0.placeholder = "iPadOS 27.0" }
        alerta.addTextField {
            $0.placeholder = "12.5"
            $0.keyboardType = .decimalPad
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Registrar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let campos = alerta.textFields?.map { $0.text ?? "" } ?? []
            guard campos.count == 7,
                  let valor = Double(campos[6].replacingOccurrences(of: ",", with: ".")) else {
                self.avisar("Complete todos los campos con un valor numérico válido.")
                return
            }
            let producto: ProductoMuestraComparativaIPad =
                campos[0].lowercased().contains("good") ? .goodnotes : .cuadernU
            let direccion: DireccionMetricaComparativaIPad =
                campos[1].lowercased().contains("mayor") ? .mayorEsMejor : .menorEsMejor
            do {
                _ = try self.repositorio.registrarMuestra(
                    producto: producto,
                    metrica: campos[2],
                    escenario: campos[3],
                    dispositivo: campos[4],
                    sistema: campos[5],
                    direccion: direccion,
                    valor: valor)
                self.recargar()
            } catch { self.avisar(error.localizedDescription) }
        })
        present(alerta, animated: true)
    }

    private func pedirFuncionManual() {
        let alerta = UIAlertController(
            title: "Evidencia funcional QA",
            message: "Use esta entrada sólo después de una verificación reproducible. Estado: inexistente/parcial/implementada/verificada.",
            preferredStyle: .alert)
        ["id", "nombre", "categoria", "goodnotes si/no", "estado", "diferenciador si/no", "evidencia"].forEach {
            placeholder in alerta.addTextField { $0.placeholder = placeholder }
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Registrar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let c = alerta.textFields?.map { $0.text ?? "" } ?? []
            guard c.count == 7,
                  let estado = EstadoFuncionComparativaIPad(
                    rawValue: c[4].trimmingCharacters(in: .whitespacesAndNewlines).lowercased()) else {
                self.avisar("Estado funcional no reconocido.")
                return
            }
            let si: (String) -> Bool = {
                ["si", "sí", "yes", "true", "1"].contains(
                    $0.trimmingCharacters(in: .whitespacesAndNewlines).lowercased())
            }
            do {
                try self.repositorio.registrarFuncion(.init(
                    id: c[0], nombre: c[1], categoria: c[2],
                    disponibleEnGoodnotes: si(c[3]),
                    estadoCuadernU: estado,
                    diferenciadorCuadernU: si(c[5]),
                    evidencia: c[6]))
                self.recargar()
            } catch { self.avisar(error.localizedDescription) }
        })
        present(alerta, animated: true)
    }
#endif

    private func confirmarReinicio() {
        let alerta = UIAlertController(
            title: "Reiniciar evidencia comparativa",
            message: "Borra sólo el dataset QA local CuadernU vs Goodnotes. No modifica cuadernos ni benchmarks generales.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(
            title: "Reiniciar", style: .destructive) { [weak self] _ in
                self?.repositorio.reiniciar()
                self?.recargar()
            })
        present(alerta, animated: true)
    }

    private func selloFecha() -> String {
        let formato = DateFormatter()
        formato.locale = Locale(identifier: "en_US_POSIX")
        formato.dateFormat = "yyyyMMdd-HHmmss"
        return formato.string(from: Date())
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(
            title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
