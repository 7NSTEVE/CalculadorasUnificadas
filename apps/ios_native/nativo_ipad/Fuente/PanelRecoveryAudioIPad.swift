import UIKit

@MainActor
final class PanelRecoveryAudioIPad: UITableViewController {
    private struct Fila {
        var manifiesto: ManifiestoAudioResilienteIPad
        var diagnostico: DiagnosticoSesionAudioResilienteIPad?
        var plan: PlanRecoveryAudioIPad?
        var error: String?
    }

    private let repositorio: RepositorioAudioResilienteIPad
    private let vinculos: RepositorioVinculosCapturaAudioIPad
    private var filas: [Fila] = []
    private var journals: [VinculoCapturaAudioIPad] = []

    init(
        repositorio: RepositorioAudioResilienteIPad = .compartido,
        vinculos: RepositorioVinculosCapturaAudioIPad = .compartido
    ) {
        self.repositorio = repositorio
        self.vinculos = vinculos
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Recovery de audio"
        view.accessibilityIdentifier = "audio.recovery.panel"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "arrow.clockwise"),
            style: .plain,
            target: self,
            action: #selector(recargar))
        recargar()
    }

    @objc private func recargar() {
        filas = repositorio.sesiones().map { manifiesto in
            do {
                let diagnostico = try repositorio.diagnosticar(manifiesto.id)
                return Fila(
                    manifiesto: manifiesto,
                    diagnostico: diagnostico,
                    plan: ConstructorPlanRecoveryAudioIPad.construir(diagnostico),
                    error: nil)
            } catch {
                return Fila(
                    manifiesto: manifiesto,
                    diagnostico: nil,
                    plan: nil,
                    error: error.localizedDescription)
            }
        }.sorted { a, b in
            let prioridadA = prioridad(a.manifiesto.estado)
            let prioridadB = prioridad(b.manifiesto.estado)
            if prioridadA != prioridadB { return prioridadA < prioridadB }
            return a.manifiesto.actualizada > b.manifiesto.actualizada
        }
        journals = vinculos.pendientesRecovery()
        tableView.reloadData()
    }

    private func prioridad(_ estado: EstadoSesionAudioResilienteIPad) -> Int {
        switch estado {
        case .requiereRecuperacion: return 0
        case .grabando: return 1
        case .pausada: return 2
        case .finalizada: return 3
        }
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 3 }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0: return "Sesiones que requieren atención"
        case 1: return "Journals de captura interrumpida"
        default: return "Otras sesiones"
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return "CuadernU nunca adopta ni elimina un chunk huérfano automáticamente. Un archivo alterado por SHA tampoco se reproduce como confiable."
        case 1:
            return "Un journal conserva qué archivo estaba produciendo el capturador. Si el manifest de chunks se dañó, la fuente se muestra como evidencia pero no se adopta ni se modifica automáticamente."
        default:
            return nil
        }
    }

    private func filasAtencion() -> [Fila] {
        filas.filter {
            $0.manifiesto.estado == .requiereRecuperacion
                || $0.error != nil
                || $0.plan?.requiereDecisionUsuario == true
                || ($0.plan?.estado == .parcialRecuperable)
        }
    }

    private func filasOtras() -> [Fila] {
        let ids = Set(filasAtencion().map { $0.manifiesto.id })
        return filas.filter { !ids.contains($0.manifiesto.id) }
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return max(filasAtencion().count, 1)
        case 1: return max(journals.count, 1)
        default: return max(filasOtras().count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.detailTextLabel?.numberOfLines = 0

        if indexPath.section == 1 {
            guard journals.indices.contains(indexPath.row) else {
                celda.textLabel?.text = "No hay journals de captura pendientes"
                celda.detailTextLabel?.text = "Las fuentes activas anteriores quedaron confirmadas o canceladas."
                celda.selectionStyle = .none
                celda.imageView?.image = UIImage(systemName: "checkmark.circle")
                return celda
            }
            let journal = journals[indexPath.row]
            let fuente = vinculos.fuente(journal)
            let existe = fuente.map {
                FileManager.default.fileExists(atPath: $0.path)
            } ?? false
            celda.textLabel?.text = "\(journal.modo.rawValue) · \(journal.id.uuidString.prefix(8))"
            celda.detailTextLabel?.text = [
                "Documento \(journal.documentoId) · \(journal.estado.rawValue)",
                journal.fuenteRelativa.map { "Fuente: \($0)" }
                    ?? "Fuente todavía no publicada por el capturador",
                existe ? "Los bytes de la fuente siguen presentes."
                    : "La fuente ya no está disponible en su ruta registrada.",
                journal.nota,
            ].compactMap { $0 }.joined(separator: "\n")
            celda.imageView?.image = UIImage(systemName:
                existe ? "waveform.badge.exclamationmark"
                    : "externaldrive.badge.xmark")
            celda.accessoryType = .disclosureIndicator
            return celda
        }

        let datos = indexPath.section == 0 ? filasAtencion() : filasOtras()
        guard datos.indices.contains(indexPath.row) else {
            celda.textLabel?.text = indexPath.section == 0
                ? "No hay sesiones de audio pendientes"
                : "No hay otras sesiones"
            celda.selectionStyle = .none
            celda.imageView?.image = UIImage(systemName: "checkmark.circle")
            return celda
        }

        let fila = datos[indexPath.row]
        celda.textLabel?.text = "Documento \(fila.manifiesto.documentoId) · \(fila.manifiesto.id.uuidString.prefix(8))"
        if let error = fila.error {
            celda.detailTextLabel?.text = "Manifest/diagnóstico no legible: \(error)"
            celda.imageView?.image = UIImage(systemName: "exclamationmark.octagon.fill")
        } else if let plan = fila.plan {
            let confiable = formatoDuracion(plan.duracionConfiable)
            let timeline = formatoDuracion(plan.duracionTimeline)
            celda.detailTextLabel?.text = [
                "\(fila.manifiesto.estado.rawValue) · \(plan.estado.rawValue)",
                "Confiable \(confiable) / timeline \(timeline)",
                "Huecos \(plan.huecos.count) · faltantes \(plan.archivosFaltantes.count) · alterados \(plan.archivosAlterados.count) · huérfanos \(plan.archivosHuerfanos.count)",
            ].joined(separator: "\n")
            celda.imageView?.image = UIImage(systemName: simbolo(plan))
            celda.accessoryType = .disclosureIndicator
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if indexPath.section == 1 {
            guard journals.indices.contains(indexPath.row) else { return }
            mostrarJournal(journals[indexPath.row])
            return
        }
        let datos = indexPath.section == 0 ? filasAtencion() : filasOtras()
        guard datos.indices.contains(indexPath.row) else { return }
        mostrarAcciones(datos[indexPath.row])
    }

    private func mostrarJournal(_ journal: VinculoCapturaAudioIPad) {
        let fuente = vinculos.fuente(journal)
        var evidencia = fuente
        if journal.esManifiestoNotaViva,
           let fuente,
           let audio = try? ResolutorAudioFinalNotaVivaIPad.resolver(
                manifiestoURL: fuente),
           FileManager.default.fileExists(atPath: audio.path) {
            evidencia = audio
        }

        let hoja = UIAlertController(
            title: "Journal \(journal.id.uuidString.prefix(8))",
            message: resumenJournal(journal, evidencia: evidencia),
            preferredStyle: .actionSheet)
        if let evidencia,
           FileManager.default.fileExists(atPath: evidencia.path) {
            hoja.addAction(UIAlertAction(
                title: journal.esManifiestoNotaViva
                    ? "Compartir evidencia / audio disponible"
                    : "Compartir fuente preservada",
                style: .default) { [weak self] _ in
                    guard let self else { return }
                    let compartir = UIActivityViewController(
                        activityItems: [evidencia], applicationActivities: nil)
                    compartir.popoverPresentationController?.sourceView = self.view
                    self.present(compartir, animated: true)
                })
        }
        hoja.addAction(UIAlertAction(
            title: "Copiar diagnóstico",
            style: .default) { _ in
                UIPasteboard.general.string = self.resumenJournal(
                    journal, evidencia: evidencia)
            })
        hoja.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func mostrarAcciones(_ fila: Fila) {
        guard let plan = fila.plan else {
            avisar(fila.error ?? "No se pudo preparar un plan de recovery.")
            return
        }
        let hoja = UIAlertController(
            title: "Sesión \(fila.manifiesto.id.uuidString.prefix(8))",
            message: resumen(plan),
            preferredStyle: .actionSheet)

        if plan.estado == .integro,
           fila.manifiesto.estado != .finalizada {
            hoja.addAction(UIAlertAction(
                title: "Finalizar segmentos confirmados",
                style: .default) { [weak self] _ in
                    guard let self else { return }
                    do {
                        try self.repositorio.finalizar(fila.manifiesto.id)
                        self.recargar()
                    } catch {
                        self.avisar("No se finalizó: \(error.localizedDescription)")
                    }
                })
        }

        hoja.addAction(UIAlertAction(
            title: "Copiar diagnóstico",
            style: .default) { _ in
                UIPasteboard.general.string = self.resumen(plan)
            })
        hoja.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func resumenJournal(
        _ journal: VinculoCapturaAudioIPad,
        evidencia: URL?
    ) -> String {
        [
            "Documento: \(journal.documentoId)",
            "Journal: \(journal.id.uuidString)",
            "Modo: \(journal.modo.rawValue)",
            "Estado: \(journal.estado.rawValue)",
            "Fuente relativa: \(journal.fuenteRelativa ?? "sin fuente")",
            "Evidencia disponible: \(evidencia?.lastPathComponent ?? "no")",
            "Nota: \(journal.nota ?? "—")",
        ].joined(separator: "\n")
    }

    private func resumen(_ plan: PlanRecoveryAudioIPad) -> String {
        var lineas = [
            "Estado: \(plan.estado.rawValue)",
            "Documento: \(plan.documentoId)",
            "Sesión: \(plan.sesionId.uuidString)",
            "Duración confiable: \(formatoDuracion(plan.duracionConfiable))",
            "Timeline: \(formatoDuracion(plan.duracionTimeline))",
            "Segmentos confiables: \(plan.intervalosConfiables.count)",
            "Huecos: \(plan.huecos.count)",
            "Faltantes: \(plan.archivosFaltantes.count)",
            "Alterados: \(plan.archivosAlterados.count)",
            "Huérfanos: \(plan.archivosHuerfanos.count)",
        ]
        if !plan.archivosFaltantes.isEmpty {
            lineas.append("Faltantes: \(plan.archivosFaltantes.joined(separator: ", "))")
        }
        if !plan.archivosAlterados.isEmpty {
            lineas.append("Alterados: \(plan.archivosAlterados.joined(separator: ", "))")
        }
        if !plan.archivosHuerfanos.isEmpty {
            lineas.append("Huérfanos preservados: \(plan.archivosHuerfanos.joined(separator: ", "))")
        }
        return lineas.joined(separator: "\n")
    }

    private func simbolo(_ plan: PlanRecoveryAudioIPad) -> String {
        switch plan.estado {
        case .integro: return "checkmark.shield.fill"
        case .parcialRecuperable: return "waveform.badge.exclamationmark"
        case .requiereEvidenciaManual: return "exclamationmark.triangle.fill"
        case .sinAudioConfirmado: return "waveform.slash"
        }
    }

    private func formatoDuracion(_ segundos: Double) -> String {
        guard segundos.isFinite, segundos >= 0 else { return "—" }
        let total = Int(segundos.rounded())
        let h = total / 3600
        let m = (total % 3600) / 60
        let s = total % 60
        return h > 0
            ? String(format: "%d:%02d:%02d", h, m, s)
            : String(format: "%d:%02d", m, s)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(
            title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
