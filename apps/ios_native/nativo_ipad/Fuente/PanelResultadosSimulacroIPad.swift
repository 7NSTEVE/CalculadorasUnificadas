import UIKit

final class PanelResultadosSimulacroIPad: UITableViewController {
    var alRevisarFrame: ((UUID) -> Void)?
    var alNuevoSimulacro: (() -> Void)?

    private let sesion: EstadoSesionSimulacroIPad
    private let informe: InformeSimulacroIPad
    private let resumen: ResumenSimulacroIPad
    private let prioritarios: [ResultadoEjercicioSimulacroIPad]
    private let comparables: [ResultadoEjercicioSimulacroIPad]
    private let recuperacion: [AccionRecuperacionSimulacroIPad]
    private let framesPorId: [UUID: FrameApunteIPad]

    init(sesion: EstadoSesionSimulacroIPad, frames: [FrameApunteIPad]) {
        let mapaFrames = Dictionary(uniqueKeysWithValues: frames.map { ($0.id, $0) })
        let estudios = Dictionary(uniqueKeysWithValues: frames.compactMap { frame in
            frame.estudio.map { (frame.id, $0) }
        })
        let titulos = Dictionary(uniqueKeysWithValues: frames.map { ($0.id, $0.titulo) })
        let resultados = RecopiladorResultadosSimulacroIPad.recopilar(
            sesion: sesion,
            estudiosPorFrame: estudios,
            titulosPorFrame: titulos)
        let informeCalculado = AnalizadorSimulacroIPad.analizar(resultados)
        let problemas = Dictionary(uniqueKeysWithValues:
            CentroEstudioTecnicoIPad.compartido.instantanea().problemas.map { ($0.id, $0) })
        let disponibles = resultados.filter { resultado in
            guard let frame = mapaFrames[resultado.frameId],
                  let estudio = frame.estudio,
                  let problemaId = estudio.problemaId,
                  let problema = problemas[problemaId],
                  let intentoId = sesion.intentoId(para: resultado.frameId),
                  case .permitido = PoliticaAccesoSolucionPostIntentoIPad.evaluar(
                    estudio: estudio,
                    problema: problema,
                    intentoId: intentoId) else { return false }
            return true
        }

        self.sesion = sesion
        self.framesPorId = mapaFrames
        self.informe = informeCalculado
        self.resumen = sesion.resumen()
        self.prioritarios = AnalizadorSimulacroIPad.prioridadRevision(informeCalculado)
        self.comparables = disponibles
        self.recuperacion = PlanificadorRecuperacionSimulacroIPad.construir(
            informeCalculado, limite: 6)
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Resultados del simulacro"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Nuevo", style: .plain, target: self, action: #selector(nuevo))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 7 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 4
        case 1: return max(informe.erroresAltaConfianza.count, 1)
        case 2: return max(informe.aciertosInseguros.count, 1)
        case 3: return max(informe.ejerciciosLentos.count, 1)
        case 4: return max(prioritarios.count, 1)
        case 5: return max(comparables.count, 1)
        default: return max(recuperacion.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0: return "Resumen"
        case 1: return "Errores con alta confianza"
        case 2: return "Aciertos con baja confianza"
        case 3: return "Cuellos de botella de tiempo"
        case 4: return "Repasar primero"
        case 5: return "Comparar con solución"
        default: return "Plan de recuperación"
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 1:
            return "Fallar estando seguro suele merecer revisión prioritaria: la respuesta fue incorrecta y la confianza fue ≥ 75 %."
        case 2:
            return "Acertar con < 45 % de confianza puede indicar dominio frágil o un acierto poco estable."
        case 3:
            return "Se compara contra la mediana de esta misma sesión; no contra otros estudiantes."
        case 4:
            return prioritarios.isEmpty
                ? "No se detectaron señales que requieran revisión especial en los intentos registrados."
                : "Tocar un ejercicio vuelve directamente a su frame para practicarlo con lápiz del iPad."
        case 5:
            return comparables.isEmpty
                ? "Solo aparecen intentos terminados que tengan una solución vinculada. Nunca se revela durante un intento activo."
                : "Abre la solución al lado de su hoja manuscrita y permite registrar dónde divergió su procedimiento."
        case 6:
            return recuperacion.isEmpty
                ? "No hay una acción de recuperación especial que añadir a este simulacro."
                : "Orden explícito: error con alta confianza → otros fallos → acierto inseguro → lentitud. No cambia el calendario del motor de repaso."
        default: return nil
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        if indexPath.section == 0 { return celdaResumen(indexPath.row) }
        if indexPath.section == 6 {
            guard recuperacion.indices.contains(indexPath.row) else {
                return celdaVacia("Sin recuperación especial pendiente")
            }
            return celdaRecuperacion(recuperacion[indexPath.row])
        }

        let grupo: [ResultadoEjercicioSimulacroIPad]
        let vacio: String
        switch indexPath.section {
        case 1:
            grupo = informe.erroresAltaConfianza
            vacio = "Ningún error con alta confianza"
        case 2:
            grupo = informe.aciertosInseguros
            vacio = "Ningún acierto inseguro"
        case 3:
            grupo = informe.ejerciciosLentos
            vacio = "Sin outliers claros de tiempo"
        case 4:
            grupo = prioritarios
            vacio = "Nada prioritario por ahora"
        default:
            grupo = comparables
            vacio = "Ninguna solución post-intento disponible"
        }

        guard grupo.indices.contains(indexPath.row) else {
            return celdaVacia(vacio)
        }
        if indexPath.section == 5 {
            return celdaComparacion(grupo[indexPath.row])
        }
        return celdaResultado(grupo[indexPath.row])
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard indexPath.section > 0 else { return }

        if indexPath.section == 6 {
            guard recuperacion.indices.contains(indexPath.row) else { return }
            let frameId = recuperacion[indexPath.row].frameId
            let callback = alRevisarFrame
            dismiss(animated: true) { callback?(frameId) }
            return
        }

        if indexPath.section == 5 {
            guard comparables.indices.contains(indexPath.row),
                  let documentoId = sesion.documentoId else { return }
            let resultado = comparables[indexPath.row]
            guard let frame = framesPorId[resultado.frameId],
                  let estudio = frame.estudio,
                  let problemaId = estudio.problemaId,
                  let intentoId = sesion.intentoId(para: frame.id) else { return }
            dismiss(animated: true) {
                ComparacionSolucionPostIntentoIPad.mostrar(
                    documentoId: documentoId,
                    frameId: frame.id,
                    problemaId: problemaId,
                    intentoId: intentoId,
                    region: frame.region)
            }
            return
        }

        let grupo: [ResultadoEjercicioSimulacroIPad]
        switch indexPath.section {
        case 1: grupo = informe.erroresAltaConfianza
        case 2: grupo = informe.aciertosInseguros
        case 3: grupo = informe.ejerciciosLentos
        default: grupo = prioritarios
        }
        guard grupo.indices.contains(indexPath.row) else { return }
        let frameId = grupo[indexPath.row].frameId
        let callback = alRevisarFrame
        dismiss(animated: true) { callback?(frameId) }
    }

    private func celdaResumen(_ fila: Int) -> UITableViewCell {
        let celda = UITableViewCell(style: .value1, reuseIdentifier: nil)
        celda.selectionStyle = .none
        switch fila {
        case 0:
            celda.textLabel?.text = "Precisión"
            celda.detailTextLabel?.text = "\(informe.aciertos)/\(informe.resultados.count) · \(Int((informe.precision * 100).rounded()))%"
        case 1:
            celda.textLabel?.text = "Completados"
            celda.detailTextLabel?.text = "\(resumen.completados)/\(resumen.total)"
        case 2:
            celda.textLabel?.text = "Confianza media"
            celda.detailTextLabel?.text = "\(Int((informe.confianzaPromedio * 100).rounded()))%"
        default:
            celda.textLabel?.text = "Tiempo"
            celda.detailTextLabel?.text = formatoTiempo(resumen.segundosUsados)
        }
        return celda
    }

    private func celdaResultado(_ resultado: ResultadoEjercicioSimulacroIPad)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = resultado.titulo
        let simbolo = resultado.resultado == .correcto ? "✓" : "↻"
        var detalle = "\(simbolo) · confianza \(Int((resultado.confianza * 100).rounded()))% · \(formatoTiempo(resultado.segundos))"
        if !resultado.error.isEmpty { detalle += " · \(resultado.error)" }
        celda.detailTextLabel?.text = detalle
        celda.detailTextLabel?.numberOfLines = 3
        celda.accessoryType = .disclosureIndicator
        celda.imageView?.image = UIImage(systemName:
            resultado.resultado == .correcto ? "checkmark.circle" : "arrow.clockwise.circle")
        return celda
    }

    private func celdaComparacion(_ resultado: ResultadoEjercicioSimulacroIPad)
        -> UITableViewCell {
        let celda = celdaResultado(resultado)
        celda.imageView?.image = UIImage(systemName: "rectangle.split.2x1")
        celda.accessoryType = .disclosureIndicator
        if let documentoId = sesion.documentoId,
           let intentoId = sesion.intentoId(para: resultado.frameId),
           RepositorioRevisionesSolucionIPad.compartido.revision(
                documento: documentoId,
                frameId: resultado.frameId,
                intentoId: intentoId) != nil {
            celda.detailTextLabel?.text = (celda.detailTextLabel?.text ?? "")
                + " · revisión guardada"
        }
        return celda
    }

    private func celdaRecuperacion(_ accion: AccionRecuperacionSimulacroIPad)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = "\(accion.prioridad.nombre) · \(accion.titulo)"
        let evidencia = accion.senales.joined(separator: " · ")
        celda.detailTextLabel?.text = "\(accion.tipo.nombre)\n\(evidencia)\n\(accion.instruccion)\nCierre: \(accion.criterioCierre)"
        celda.detailTextLabel?.numberOfLines = 6
        celda.accessoryType = .disclosureIndicator
        celda.imageView?.image = UIImage(systemName: simboloRecuperacion(accion.tipo))
        return celda
    }

    private func celdaVacia(_ texto: String) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = texto
        celda.textLabel?.textColor = .secondaryLabel
        celda.selectionStyle = .none
        celda.imageView?.image = UIImage(systemName: "checkmark.circle")
        return celda
    }

    private func simboloRecuperacion(_ tipo: TipoRecuperacionSimulacroIPad) -> String {
        switch tipo {
        case .corregirModeloMental: return "exclamationmark.bubble"
        case .reconstruirProcedimiento: return "arrow.trianglehead.2.clockwise.rotate.90"
        case .consolidarConfianza: return "checkmark.seal"
        case .ganarFluidez: return "timer"
        }
    }

    private func formatoTiempo(_ segundos: TimeInterval) -> String {
        let total = max(Int(segundos.rounded()), 0)
        if total < 60 { return "\(total) s" }
        return String(format: "%d:%02d", total / 60, total % 60)
    }

    @objc private func nuevo() {
        let callback = alNuevoSimulacro
        dismiss(animated: true) { callback?() }
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
