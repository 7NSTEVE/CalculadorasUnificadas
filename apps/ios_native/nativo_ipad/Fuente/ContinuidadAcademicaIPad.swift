import Foundation
import UIKit

struct GrupoTimelineAcademicoIPad: Identifiable {
    var semestre: String
    var cursos: [CursoAcademico]
    var eventos: [EventoAcademico]
    var id: String { semestre }
}

extension CentroAcademico {
    func archivarSemestre(_ semestre: String, valor: Bool) {
        let buscado = Normalizador.texto(semestre)
        transaccion { base in
            for indice in base.cursos.indices
            where Normalizador.texto(base.cursos[indice].semestre) == buscado {
                base.cursos[indice].archivado = valor
            }
            base.timeline.append(EventoAcademico(
                tipo: .archivo,
                titulo: valor ? "Semestre archivado: \(semestre)"
                    : "Semestre restaurado: \(semestre)",
                detalle: "El contenido conserva búsqueda, fuentes y relaciones."))
        }
        CentroExperienciaIPad.compartido.archivarEspacios(
            semestre: semestre, valor: valor)
    }

    @discardableResult
    func duplicarCurso(_ cursoId: UUID,
                       nuevoSemestre: String) -> CursoAcademico? {
        let nuevo: CursoAcademico? = transaccion { base in
            guard let origen = base.cursos.first(where: { $0.id == cursoId }) else {
                return nil
            }
            let semestre = nuevoSemestre.trimmingCharacters(
                in: .whitespacesAndNewlines)
            let codigoOrigen = Normalizador.texto(origen.codigo)
            let nombreOrigen = Normalizador.texto(origen.nombre)
            let semestreDestino = Normalizador.texto(semestre)
            guard !semestre.isEmpty,
                  !base.cursos.contains(where: { curso in
                      let mismaAsignatura = codigoOrigen.isEmpty
                          ? Normalizador.texto(curso.nombre) == nombreOrigen
                          : Normalizador.texto(curso.codigo) == codigoOrigen
                      return mismaAsignatura
                          && Normalizador.texto(curso.semestre) == semestreDestino
                  }) else { return nil }
            var copia = origen
            copia.id = UUID()
            copia.semestre = semestre
            copia.archivado = false
            base.cursos.append(copia)
            base.timeline.append(EventoAcademico(
                tipo: .archivo,
                titulo: "Curso reutilizado: \(origen.nombre)",
                detalle: "Plantillas y herramientas pasan a \(semestre); el contenido académico no se duplica.",
                cursoId: copia.id))
            return copia
        }
        if let nuevo {
            _ = CentroExperienciaIPad.compartido.duplicarEspacio(
                origenCursoId: cursoId, para: nuevo)
            CentroExperienciaIPad.compartido.relacionar(
                origen: "curso:\(cursoId.uuidString)",
                destino: "curso:\(nuevo.id.uuidString)",
                tipo: .continua, cursoId: nuevo.id,
                nota: "Continuidad hacia \(nuevo.semestre)")
        }
        return nuevo
    }

    @discardableResult
    func reutilizarConceptos(de cursoOrigen: UUID,
                             en cursoDestino: UUID) -> Int {
        let pares: [(UUID, UUID)] = transaccion { base in
            let origen = base.conceptos.filter { $0.cursoId == cursoOrigen }
            let existentes = Set(base.conceptos.filter {
                $0.cursoId == cursoDestino
            }.map { Normalizador.texto($0.nombre) })
            var creados: [(UUID, UUID)] = []
            for concepto in origen
            where !existentes.contains(Normalizador.texto(concepto.nombre)) {
                var copia = concepto
                copia.id = UUID()
                copia.cursoId = cursoDestino
                copia.dominio = 0
                copia.confianzaDeclarada = 0
                copia.intentos = 0
                copia.aciertos = 0
                copia.ultimoContacto = nil
                copia.prerrequisitos = Array(Set(
                    concepto.prerrequisitos + [concepto.id]))
                copia.etiquetas = Array(Set(concepto.etiquetas + ["reutilizado"]))
                base.conceptos.append(copia)
                creados.append((concepto.id, copia.id))
            }
            return creados
        }
        for par in pares {
            CentroExperienciaIPad.compartido.relacionar(
                origen: "concepto:\(par.0.uuidString)",
                destino: "concepto:\(par.1.uuidString)",
                tipo: .continua, cursoId: cursoDestino,
                nota: "Concepto reutilizado sin duplicar el material fuente")
        }
        return pares.count
    }

    func vincularPrerrequisito(_ prerrequisito: UUID,
                               con concepto: UUID, nota: String) {
        let destinoCurso: UUID? = transaccion { base in
            guard let indice = base.conceptos.firstIndex(where: {
                $0.id == concepto
            }), base.conceptos.contains(where: { $0.id == prerrequisito }) else {
                return nil
            }
            if !base.conceptos[indice].prerrequisitos.contains(prerrequisito) {
                base.conceptos[indice].prerrequisitos.append(prerrequisito)
            }
            return base.conceptos[indice].cursoId
        }
        guard let destinoCurso else { return }
        CentroExperienciaIPad.compartido.relacionar(
            origen: "concepto:\(prerrequisito.uuidString)",
            destino: "concepto:\(concepto.uuidString)",
            tipo: .prerrequisito, cursoId: destinoCurso, nota: nota)
    }

    func timelineAcademico() -> [GrupoTimelineAcademicoIPad] {
        let datos = instantanea()
        let cursosPorId = Dictionary(uniqueKeysWithValues: datos.cursos.map {
            ($0.id, $0)
        })
        var semestres = Dictionary(grouping: datos.cursos) {
            $0.semestre.isEmpty ? "Sin semestre" : $0.semestre
        }
        var eventos: [String: [EventoAcademico]] = [:]
        for evento in datos.timeline {
            let semestre = evento.cursoId.flatMap { cursosPorId[$0]?.semestre }
                .flatMap { $0.isEmpty ? nil : $0 }
                ?? Self.anio.string(from: evento.fecha)
            eventos[semestre, default: []].append(evento)
            if semestres[semestre] == nil { semestres[semestre] = [] }
        }
        return semestres.map { semestre, cursos in
            GrupoTimelineAcademicoIPad(
                semestre: semestre,
                cursos: cursos.sorted { $0.nombre < $1.nombre },
                eventos: (eventos[semestre] ?? []).sorted { $0.fecha > $1.fecha })
        }.sorted { $0.semestre.localizedStandardCompare($1.semestre) == .orderedDescending }
    }

    private static let anio: DateFormatter = {
        let formato = DateFormatter(); formato.dateFormat = "yyyy"; return formato
    }()
}

final class PanelContinuidadAcademicaIPad: UITableViewController {
    private let centro = CentroAcademico.compartido
    private var datos = BaseAcademica()
    private var semestres: [String] = []
    private var timeline: [GrupoTimelineAcademicoIPad] = []

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Semestres y continuidad"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        refrescar()
    }

    private func refrescar() {
        datos = centro.instantanea()
        semestres = Array(Set(datos.cursos.map(\.semestre).filter { !$0.isEmpty }))
            .sorted { $0.localizedStandardCompare($1) == .orderedDescending }
        timeline = centro.timelineAcademico()
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Semestres", "Cursos y reutilización", "Prerequisite notes",
         "Timeline académico"][section]
    }
    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return "Archivar quita el semestre del Home, pero la búsqueda global y el operador in: siguen incluyendo sus fuentes."
        case 1:
            return "Duplicar un curso conserva su perfil de papel, plumas y herramientas; no copia clases, tareas ni evaluaciones."
        case 2:
            return "Una nota de prerrequisito enlaza conceptos entre cursos sin copiar el documento fuente."
        default:
            return "La cronología conserva semestres archivados y permite revisar años anteriores."
        }
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return max(semestres.count, 1)
        case 1: return max(datos.cursos.count, 1)
        case 2: return max(datos.conceptos.count, 1)
        default: return max(timeline.count, 1)
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.accessoryType = .disclosureIndicator
        switch indexPath.section {
        case 0:
            guard !semestres.isEmpty else {
                return vacia(celda, "No hay semestres definidos")
            }
            let semestre = semestres[indexPath.row]
            let cursos = datos.cursos.filter { $0.semestre == semestre }
            let archivado = !cursos.isEmpty && cursos.allSatisfy { $0.archivado }
            celda.textLabel?.text = semestre
            celda.detailTextLabel?.text = "\(cursos.count) cursos · \(archivado ? "archivado y buscable" : "activo")"
            celda.imageView?.image = UIImage(systemName: archivado ? "archivebox.fill" : "calendar")
        case 1:
            guard !datos.cursos.isEmpty else { return vacia(celda, "No hay cursos") }
            let curso = cursosOrdenados[indexPath.row]
            celda.textLabel?.text = [curso.codigo, curso.nombre]
                .filter { !$0.isEmpty }.joined(separator: " — ")
            celda.detailTextLabel?.text = "\(curso.semestre) · \(curso.archivado ? "archivado" : "activo")"
            celda.imageView?.image = UIImage(systemName: "rectangle.3.group")
        case 2:
            guard !datos.conceptos.isEmpty else {
                return vacia(celda, "Todavía no hay conceptos indexados")
            }
            let concepto = conceptosOrdenados[indexPath.row]
            celda.textLabel?.text = concepto.nombre
            let curso = datos.cursos.first { $0.id == concepto.cursoId }
            celda.detailTextLabel?.text = "\(curso?.codigo ?? curso?.nombre ?? "Sin curso") · \(concepto.prerrequisitos.count) prerrequisitos"
            celda.imageView?.image = UIImage(systemName: "point.3.connected.trianglepath.dotted")
        default:
            guard !timeline.isEmpty else { return vacia(celda, "Timeline vacío") }
            let grupo = timeline[indexPath.row]
            celda.textLabel?.text = grupo.semestre
            celda.detailTextLabel?.text = "\(grupo.cursos.count) cursos · \(grupo.eventos.count) eventos"
            celda.imageView?.image = UIImage(systemName: "clock.arrow.circlepath")
        }
        return celda
    }

    private var cursosOrdenados: [CursoAcademico] {
        datos.cursos.sorted {
            if $0.archivado != $1.archivado { return !$0.archivado }
            if $0.semestre != $1.semestre {
                return $0.semestre.localizedStandardCompare($1.semestre)
                    == .orderedDescending
            }
            return $0.nombre.localizedCaseInsensitiveCompare($1.nombre)
                == .orderedAscending
        }
    }

    private var conceptosOrdenados: [ConceptoDominio] {
        datos.conceptos.sorted {
            $0.nombre.localizedCaseInsensitiveCompare($1.nombre) == .orderedAscending
        }
    }

    private func vacia(_ celda: UITableViewCell, _ texto: String) -> UITableViewCell {
        celda.textLabel?.text = texto
        celda.accessoryType = .none
        celda.selectionStyle = .none
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch indexPath.section {
        case 0 where !semestres.isEmpty:
            gestionarSemestre(semestres[indexPath.row])
        case 1 where !datos.cursos.isEmpty:
            gestionarCurso(cursosOrdenados[indexPath.row])
        case 2 where !datos.conceptos.isEmpty:
            gestionarConcepto(conceptosOrdenados[indexPath.row])
        case 3 where !timeline.isEmpty:
            navigationController?.pushViewController(
                PanelTimelineSemestreIPad(grupo: timeline[indexPath.row]),
                animated: true)
        default: break
        }
    }

    private func gestionarSemestre(_ semestre: String) {
        let cursos = datos.cursos.filter { $0.semestre == semestre }
        let archivado = !cursos.isEmpty && cursos.allSatisfy { $0.archivado }
        let hoja = UIAlertController(title: semestre,
                                     message: "El contenido nunca se elimina.",
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(
            title: archivado ? "Restaurar en Home" : "Archivar semestre",
            style: .default) { [weak self] _ in
                self?.centro.archivarSemestre(semestre, valor: !archivado)
                self?.refrescar()
            })
        hoja.addAction(UIAlertAction(
            title: "Buscar dentro de este semestre", style: .default) { _ in
                UIPasteboard.general.string = "in:\"\(semestre)\" "
            })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func gestionarCurso(_ curso: CursoAcademico) {
        let hoja = UIAlertController(title: curso.nombre,
                                     message: curso.semestre,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Duplicar al nuevo semestre",
                                     style: .default) { [weak self] _ in
            self?.pedirSemestreNuevo(curso)
        })
        let destinos = datos.cursos.filter { $0.id != curso.id && !$0.archivado }
        if !destinos.isEmpty {
            hoja.addAction(UIAlertAction(title: "Elegir curso para reutilizar conceptos…",
                                         style: .default) { [weak self] _ in
                let selector = UIAlertController(title: "Destino",
                                                 message: nil,
                                                 preferredStyle: .actionSheet)
                selector.view.accessibilityLabel = "Destino de conceptos"
                for destino in destinos {
                    selector.addAction(UIAlertAction(
                        title: destino.nombre, style: .default) { [weak self] _ in
                            guard let self else { return }
                            let cantidad = self.centro.reutilizarConceptos(
                                de: curso.id, en: destino.id)
                            self.avisar("Se vincularon \(cantidad) conceptos con sus orígenes; no se duplicaron documentos.")
                            self.refrescar()
                        })
                }
                selector.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
                selector.popoverPresentationController?.sourceView = self?.view
                self?.present(selector, animated: true)
            })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func pedirSemestreNuevo(_ curso: CursoAcademico) {
        let alerta = UIAlertController(title: "Nuevo semestre",
                                       message: "Se conservan plantillas y herramientas; el contenido académico empieza vacío.",
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Ej.: I semestre 2027" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let semestre = alerta.textFields?.first?.text,
                  let creado = self?.centro.duplicarCurso(
                    curso.id, nuevoSemestre: semestre) else {
                self?.avisar("No se creó: revise el semestre o si ya existe el mismo código.")
                return
            }
            self?.avisar("Se creó \(creado.nombre) para \(creado.semestre) sin copiar clases ni tareas.")
            self?.refrescar()
        })
        present(alerta, animated: true)
    }

    private func gestionarConcepto(_ concepto: ConceptoDominio) {
        let candidatos = conceptosOrdenados.filter { $0.id != concepto.id }
        let hoja = UIAlertController(
            title: concepto.nombre,
            message: "Elegir un concepto que dependa de este prerrequisito.",
            preferredStyle: .actionSheet)
        for destino in candidatos.prefix(40) {
            let curso = datos.cursos.first { $0.id == destino.cursoId }
            hoja.addAction(UIAlertAction(
                title: "\(curso?.codigo ?? "Curso") → \(destino.nombre)",
                style: .default) { [weak self] _ in
                    self?.pedirNotaPrerrequisito(origen: concepto,
                                                 destino: destino)
                })
        }
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }

    private func pedirNotaPrerrequisito(origen: ConceptoDominio,
                                        destino: ConceptoDominio) {
        let alerta = UIAlertController(
            title: "Prerequisite note",
            message: "\(destino.nombre) se relacionará con \(origen.nombre).",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nota: repasar Thévenin antes…" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Vincular", style: .default) {
            [weak self] _ in
            self?.centro.vincularPrerrequisito(
                origen.id, con: destino.id,
                nota: alerta.textFields?.first?.text ?? "")
            self?.refrescar()
        })
        present(alerta, animated: true)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto,
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

final class PanelTimelineSemestreIPad: UITableViewController {
    private let grupo: GrupoTimelineAcademicoIPad

    init(grupo: GrupoTimelineAcademicoIPad) {
        self.grupo = grupo
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = grupo.semestre
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Cursos" : "Eventos"
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        section == 0 ? max(grupo.cursos.count, 1) : max(grupo.eventos.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            if grupo.cursos.isEmpty { celda.textLabel?.text = "Sin cursos asociados" }
            else {
                let curso = grupo.cursos[indexPath.row]
                celda.textLabel?.text = curso.nombre
                celda.detailTextLabel?.text = curso.codigo
            }
        } else if grupo.eventos.isEmpty {
            celda.textLabel?.text = "Sin eventos registrados"
        } else {
            let evento = grupo.eventos[indexPath.row]
            celda.textLabel?.text = evento.titulo
            celda.detailTextLabel?.text = "\(evento.tipo.rawValue) · \(Self.fecha.string(from: evento.fecha))"
        }
        return celda
    }

    private static let fecha: DateFormatter = {
        let formato = DateFormatter(); formato.dateStyle = .medium
        formato.timeStyle = .short; return formato
    }()
}
