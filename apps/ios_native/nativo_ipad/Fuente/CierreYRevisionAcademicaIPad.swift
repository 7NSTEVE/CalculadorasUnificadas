import Foundation
import UIKit

// MARK: - Títulos sugeridos, nunca impuestos

struct RegistroTituloAutomaticoIPad: Codable, Equatable {
    var documentoId: UInt64
    var sugerencias: [String]
    var aceptado: String?
    var actualizado = Date()
}

private struct ArchivoTitulosAutomaticosIPad: Codable {
    var formato = 1
    var registros: [String: RegistroTituloAutomaticoIPad] = [:]
}

final class RepositorioTitulosAutomaticosIPad {
    static let compartido = RepositorioTitulosAutomaticosIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var archivo: ArchivoTitulosAutomaticosIPad

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/TitulosAutomaticos",
                                                     isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("titulos.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? decoder.decode(ArchivoTitulosAutomaticosIPad.self, from: $0)
        } ?? ArchivoTitulosAutomaticosIPad()
    }

    func registro(_ documento: UInt64) -> RegistroTituloAutomaticoIPad? {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.registros[String(documento)]
    }

    func guardarSugerencias(_ sugerencias: [String], documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let limpias = sugerencias.map {
            $0.trimmingCharacters(in: .whitespacesAndNewlines)
        }.filter { !$0.isEmpty }
        let unicas = NSOrderedSet(array: limpias).array.compactMap { $0 as? String }
        var registro = archivo.registros[String(documento)]
            ?? RegistroTituloAutomaticoIPad(documentoId: documento,
                                             sugerencias: [])
        registro.sugerencias = Array(unicas.prefix(6))
        registro.actualizado = Date()
        archivo.registros[String(documento)] = registro
        persistir()
    }

    func aceptar(_ titulo: String, documento: UInt64) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let limpio = titulo.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else { return }
        var registro = archivo.registros[String(documento)]
            ?? RegistroTituloAutomaticoIPad(documentoId: documento,
                                             sugerencias: [limpio])
        registro.aceptado = limpio
        registro.actualizado = Date()
        archivo.registros[String(documento)] = registro
        persistir()
    }

    private func persistir() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        guard let datos = try? encoder.encode(archivo) else { return }
        try? datos.write(to: url, options: [.atomic, .completeFileProtection])
    }
}

enum GeneradorTitulosAutomaticosIPad {
    static func sugerencias(sesion: Sesion, clase: SesionClase?) -> [String] {
        let documento = sesion.documentoPersistenteId
        let base = CentroAcademico.compartido.instantanea()
        let curso = clase?.cursoId.flatMap { id in
            base.cursos.first { $0.id == id }
        }
        let marcas = RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento)
        let nodos = base.conocimiento.filter { $0.fuente.documentoId == documento }

        var temas: [String] = marcas.filter { $0.tipo == .encabezado }
            .map(\.titulo)
        temas += nodos.flatMap { nodo in
            [nodo.titulo] + nodo.etiquetas
        }
        temas += sesion.textos.compactMap { texto in
            let primera = texto.texto.components(separatedBy: .newlines).first?
                .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
            guard (4...72).contains(primera.count) else { return nil }
            return primera
        }
        temas = depurarTemas(temas)

        let principal = temas.first ?? curso?.nombre ?? "Apuntes de clase"
        let segundo = temas.dropFirst().first
        let temaCompuesto = segundo.map { "\(principal) y \($0)" } ?? principal
        let numeroClase: Int = {
            guard let cursoId = curso?.id else { return 0 }
            return base.sesiones.filter {
                $0.cursoId == cursoId && $0.inicio <= (clase?.inicio ?? Date())
            }.count
        }()
        let fecha = DateFormatter.tituloClase.string(from: clase?.inicio ?? Date())
        var salida: [String] = []
        if numeroClase > 0 {
            salida.append("Clase \(numeroClase) — \(temaCompuesto)")
        }
        if let curso { salida.append("\(curso.nombre) — \(temaCompuesto)") }
        salida.append("\(fecha) — \(temaCompuesto)")
        salida.append(temaCompuesto)
        let unicas = NSOrderedSet(array: salida).array.compactMap { $0 as? String }
        RepositorioTitulosAutomaticosIPad.compartido.guardarSugerencias(
            unicas, documento: documento)
        return unicas
    }

    static func esGenerico(_ titulo: String?) -> Bool {
        let normal = Normalizador.texto(titulo ?? "")
        return normal.isEmpty || normal == "tinta" || normal == "apuntes"
            || normal == "nueva nota" || normal == "referencia"
            || normal.hasPrefix("apunte ")
    }

    private static func depurarTemas(_ valores: [String]) -> [String] {
        let genericos = Set(["apunte", "apuntes", "clase", "pagina", "nota",
                             "documento", "imagen", "texto", "ocr"])
        var vistos = Set<String>()
        return valores.compactMap { valor in
            let limpio = valor.trimmingCharacters(in: .whitespacesAndNewlines)
                .replacingOccurrences(of: #"^[#•*\-\s]+"#, with: "",
                                      options: .regularExpression)
            let normal = Normalizador.texto(limpio)
            guard (3...80).contains(limpio.count), !genericos.contains(normal),
                  !vistos.contains(normal) else { return nil }
            vistos.insert(normal); return limpio
        }
    }
}

// MARK: - Cierre de clase y recuperación activa

struct ResumenCierreClaseIPad {
    var clase: SesionClase
    var tituloSugerido: String?
    var temas: [String]
    var pendientes: [TareaAcademica]
    var dudas: [MarcacionSemanticaIPad]
    var importantes: [MarcacionSemanticaIPad]
    var preguntasRecuperacion: [String]
    var fuentes: [ReferenciaFuente]
}

enum MotorCierreClaseIPad {
    static func preparar(clase: SesionClase, sesion: Sesion) -> ResumenCierreClaseIPad {
        let documento = sesion.documentoPersistenteId
        let centro = CentroAcademico.compartido
        let fuenteDocumento = ReferenciaFuente(
            tipo: .apuntePropio, titulo: clase.titulo,
            cursoId: clase.cursoId, sesionId: clase.id,
            documentoId: documento)
        let textoLocal = sesion.textos.map(\.texto).joined(separator: "\n")
        let nodos = centro.instantanea().conocimiento.filter {
            $0.fuente.documentoId == documento || $0.fuente.sesionId == clase.id
        }
        let textoIndexado = nodos.map(\.texto).joined(separator: "\n")
        let nuevas = centro.extraerTareas(
            texto: [textoLocal, textoIndexado].joined(separator: "\n"),
            cursoId: clase.cursoId, fuente: fuenteDocumento)
        let marcas = RepositorioSemanticaIPad.compartido
            .marcaciones(documento: documento)
        let dudas = marcas.filter { $0.tipo == .duda || $0.tipo == .pregunta }
        let importantes = marcas.filter {
            $0.tipo == .importante || $0.tipo == .examen || $0.tipo == .formula
        }
        var temas = marcas.filter {
            [.encabezado, .definicion, .ejemplo].contains($0.tipo)
        }.map(\.titulo)
        temas += nodos.flatMap { [$0.titulo] + $0.etiquetas }
        temas = unicos(temas).prefix(10).map { $0 }
        if temas.isEmpty { temas = ["Contenido manuscrito de la sesión"] }

        var preguntas = temas.prefix(3).map {
            "Sin mirar, ¿cómo explicaría \($0) y qué evidencia usaría?"
        }
        preguntas += dudas.prefix(2).map {
            "¿Cómo resolvería ahora esta duda: \($0.titulo)?"
        }
        preguntas += importantes.prefix(max(0, 5 - preguntas.count)).map {
            "¿Por qué \($0.titulo) fue marcado como \($0.tipo.nombre.lowercased())?"
        }
        while preguntas.count < 5 {
            preguntas.append("¿Qué parte de esta clase podría reconstruir sin volver a leer?")
        }
        preguntas = Array(preguntas.prefix(5))

        let sugerencias = GeneradorTitulosAutomaticosIPad.sugerencias(
            sesion: sesion, clase: clase)
        let resumenTexto = construirResumen(
            temas: temas, pendientes: nuevas, dudas: dudas,
            importantes: importantes, preguntas: preguntas)
        centro.finalizarClase(
            clase.id, resumen: resumenTexto,
            nombresAutomaticos: sugerencias)
        return ResumenCierreClaseIPad(
            clase: clase, tituloSugerido: sugerencias.first,
            temas: temas, pendientes: nuevas, dudas: dudas,
            importantes: importantes, preguntasRecuperacion: preguntas,
            fuentes: unicasFuentes(nodos.map(\.fuente) + [fuenteDocumento]))
    }

    private static func construirResumen(
        temas: [String], pendientes: [TareaAcademica],
        dudas: [MarcacionSemanticaIPad], importantes: [MarcacionSemanticaIPad],
        preguntas: [String]) -> String {
        var lineas = ["Se vio:"] + temas.map { "• \($0)" }
        lineas += ["", "Pendiente:"]
            + (pendientes.isEmpty ? ["• Sin tareas nuevas detectadas"]
               : pendientes.map { "• \($0.titulo)" })
        lineas += ["", "Dudas:"]
            + (dudas.isEmpty ? ["• Sin dudas marcadas"]
               : dudas.map { "• \($0.titulo)" })
        lineas += ["", "Importante:"]
            + (importantes.isEmpty ? ["• Sin marcas"]
               : importantes.map { "• \($0.titulo)" })
        lineas += ["", "Revisión activa de 5 minutos:"]
            + preguntas.enumerated().map { "\($0.offset + 1). \($0.element)" }
        return lineas.joined(separator: "\n")
    }

    private static func unicos(_ valores: [String]) -> [String] {
        var vistos = Set<String>()
        return valores.compactMap { valor in
            let limpio = valor.trimmingCharacters(in: .whitespacesAndNewlines)
            let normal = Normalizador.texto(limpio)
            guard !limpio.isEmpty, !vistos.contains(normal) else { return nil }
            vistos.insert(normal); return limpio
        }
    }

    private static func unicasFuentes(_ fuentes: [ReferenciaFuente])
        -> [ReferenciaFuente] {
        var vistos = Set<String>()
        return fuentes.filter {
            let clave = "\($0.documentoId ?? 0):\($0.elementoId ?? ""):\($0.hashArchivo ?? ""):\($0.pagina ?? -1)"
            return vistos.insert(clave).inserted
        }
    }
}

extension CentroAcademico {
    func finalizarClase(_ id: UUID, resumen: String,
                        nombresAutomaticos: [String]) {
        transaccion { base in
            guard let indice = base.sesiones.firstIndex(where: { $0.id == id }) else {
                return
            }
            base.sesiones[indice].fin = base.sesiones[indice].fin ?? Date()
            base.sesiones[indice].resumenPostClase = resumen
            base.sesiones[indice].nombresAutomaticos = nombresAutomaticos
            base.timeline.append(EventoAcademico(
                tipo: .clase, titulo: "Clase cerrada: \(base.sesiones[indice].titulo)",
                detalle: "Resumen y revisión de cinco minutos preparados.",
                cursoId: base.sesiones[indice].cursoId, sesionId: id))
        }
    }

    func marcarClaseRevisada(_ id: UUID) {
        transaccion { base in
            if let indice = base.sesiones.firstIndex(where: { $0.id == id }) {
                base.sesiones[indice].revisada = true
                base.timeline.append(EventoAcademico(
                    tipo: .repaso, titulo: "Revisión postclase completada",
                    cursoId: base.sesiones[indice].cursoId, sesionId: id))
            }
        }
    }
}

final class PanelCierreClaseIPad: UITableViewController {
    var alAceptarTitulo: ((String) -> Void)?
    var alAbrirFuente: ((ReferenciaFuente) -> Void)?
    private let resumen: ResumenCierreClaseIPad

    init(resumen: ResumenCierreClaseIPad) {
        self.resumen = resumen
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Clase guardada"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Revisión hecha", style: .done,
            target: self, action: #selector(revisada))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 7 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Título sugerido", "Se vio", "Pendiente", "Dudas marcadas",
         "Importante", "Revisión de 5 minutos", "Fuentes originales"][section]
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return resumen.tituloSugerido == nil ? 0 : 1
        case 1: return resumen.temas.count
        case 2: return max(resumen.pendientes.count, 1)
        case 3: return max(resumen.dudas.count, 1)
        case 4: return max(resumen.importantes.count, 1)
        case 5: return resumen.preguntasRecuperacion.count
        default: return max(resumen.fuentes.count, 1)
        }
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            celda.textLabel?.text = resumen.tituloSugerido
            celda.detailTextLabel?.text = "Toque para aceptar; nunca reemplaza el título por sí solo."
            celda.accessoryType = .disclosureIndicator
        case 1: celda.textLabel?.text = resumen.temas[indexPath.row]
        case 2:
            celda.textLabel?.text = resumen.pendientes.isEmpty
                ? "Sin tareas nuevas detectadas" : resumen.pendientes[indexPath.row].titulo
        case 3:
            celda.textLabel?.text = resumen.dudas.isEmpty
                ? "Sin dudas marcadas" : resumen.dudas[indexPath.row].titulo
        case 4:
            celda.textLabel?.text = resumen.importantes.isEmpty
                ? "Sin marcas importantes" : resumen.importantes[indexPath.row].titulo
        case 5:
            celda.textLabel?.text = "Pregunta \(indexPath.row + 1)"
            celda.detailTextLabel?.text = resumen.preguntasRecuperacion[indexPath.row]
            celda.detailTextLabel?.numberOfLines = 0
        default:
            guard !resumen.fuentes.isEmpty else {
                celda.textLabel?.text = "No hay una fuente enlazable"
                celda.detailTextLabel?.text = "El resumen conserva el vínculo cuando existe una página, objeto o archivo de origen."
                celda.detailTextLabel?.numberOfLines = 0
                return celda
            }
            let fuente = resumen.fuentes[indexPath.row]
            celda.textLabel?.text = fuente.titulo.isEmpty
                ? nombreLegible(fuente.tipo.rawValue) : fuente.titulo
            celda.detailTextLabel?.text = detalle(fuente)
            celda.detailTextLabel?.numberOfLines = 0
            celda.accessoryType = .disclosureIndicator
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        if indexPath.section == 0, let titulo = resumen.tituloSugerido {
            RepositorioTitulosAutomaticosIPad.compartido.aceptar(
                titulo, documento: resumen.clase.documentoId ?? 0)
            alAceptarTitulo?(titulo)
        } else if indexPath.section == 6,
                  resumen.fuentes.indices.contains(indexPath.row) {
            alAbrirFuente?(resumen.fuentes[indexPath.row])
        }
    }
    @objc private func revisada() {
        CentroAcademico.compartido.marcarClaseRevisada(resumen.clase.id)
        dismiss(animated: true)
    }
    private func detalle(_ fuente: ReferenciaFuente) -> String {
        var partes = [nombreLegible(fuente.tipo.rawValue)]
        if let pagina = fuente.pagina { partes.append("página \(pagina + 1)") }
        if let segundo = fuente.segundoAudio {
            let minutos = Int(segundo) / 60
            let segundos = Int(segundo) % 60
            partes.append(String(format: "%d:%02d", minutos, segundos))
        }
        if fuente.documentoId != nil { partes.append("apunte enlazado") }
        if fuente.elementoId != nil { partes.append("objeto exacto") }
        if fuente.hashArchivo != nil { partes.append("archivo original") }
        return partes.joined(separator: " · ")
    }
    private func nombreLegible(_ valor: String) -> String {
        valor.unicodeScalars.reduce(into: "") { salida, escalar in
            if CharacterSet.uppercaseLetters.contains(escalar), !salida.isEmpty {
                salida.append(" ")
            }
            salida.append(contentsOf: String(escalar))
        }.capitalized
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

// MARK: - Revisión semanal, sin rachas ni culpa

struct ResumenCursoSemanalIPad: Identifiable {
    var curso: CursoAcademico
    var clases: Int
    var dudasPendientes: Int
    var ejerciciosResueltos: Int
    var erroresRecurrentes: Int
    var tarjetasPendientes: Int
    var temas: [String]
    var id: UUID { curso.id }
}

struct ResumenSemanalIPad {
    var intervalo: DateInterval
    var clases: [SesionClase]
    var paginasCreadas: Int
    var dudasPendientes: [String]
    var tareas: [TareaAcademica]
    var temasEstudiados: [String]
    var temasNuncaRevisados: [ConceptoDominio]
    var proximosExamenes: [TareaAcademica]
    var cursos: [ResumenCursoSemanalIPad]
}

enum GeneradorRevisionSemanalIPad {
    static func generar(fecha: Date = Date()) -> ResumenSemanalIPad {
        let calendario = Calendar.current
        let intervalo = calendario.dateInterval(of: .weekOfYear, for: fecha)
            ?? DateInterval(start: calendario.startOfDay(for: fecha),
                            duration: 7 * 86_400)
        let base = CentroAcademico.compartido.instantanea()
        let evaluacion = CentroEvaluacionIPad.compartido.instantanea()
        let clases = base.sesiones.filter { intervalo.contains($0.inicio) }
        let documentos = Set(clases.compactMap(\.documentoId))
        var dudas = base.bandeja.filter { !$0.procesada && $0.tipo == .duda }
            .map(\.contenido)
        for documento in documentos {
            dudas += RepositorioSemanticaIPad.compartido
                .marcaciones(documento: documento)
                .filter { $0.tipo == .duda || $0.tipo == .pregunta }
                .map(\.titulo)
        }
        dudas = unicos(dudas)
        let tareas = base.tareas.filter {
            intervalo.contains($0.fechaCreacion)
                || $0.fechaLimite.map { intervalo.contains($0) } == true
                || (($0.estado == .pendiente || $0.estado == .enCurso)
                    && $0.fechaLimite == nil)
        }
        let temasSemana = base.conocimiento.filter { intervalo.contains($0.fecha) }
            .flatMap { [$0.titulo] + $0.etiquetas }
        let temas = unicos(temasSemana)
        let nunca = base.conceptos.filter {
            $0.ultimoContacto == nil || ($0.ultimoContacto ?? .distantPast) < intervalo.start
        }.sorted { $0.dominio < $1.dominio }
        let limiteExamen = calendario.date(byAdding: .day, value: 35, to: fecha)
            ?? fecha.addingTimeInterval(35 * 86_400)
        let examenes = base.tareas.filter { tarea in
            let texto = Normalizador.texto(tarea.titulo + " " + tarea.etiquetas.joined(separator: " "))
            guard texto.contains("examen") || texto.contains("quiz")
                    || texto.contains("parcial") else { return false }
            return tarea.fechaLimite.map { $0 >= fecha && $0 <= limiteExamen } ?? false
        }.sorted { ($0.fechaLimite ?? .distantFuture) < ($1.fechaLimite ?? .distantFuture) }

        let cursos = base.cursos.filter { !$0.archivado }.compactMap {
            curso -> ResumenCursoSemanalIPad? in
            let clasesCurso = clases.filter { $0.cursoId == curso.id }
            let dudasCurso = base.bandeja.filter {
                !$0.procesada && $0.tipo == .duda
                    && ($0.cursoConfirmado == curso.id || $0.cursoSugerido == curso.id)
            }.count + documentos.reduce(0) { total, documento in
                total + RepositorioSemanticaIPad.compartido
                    .marcaciones(documento: documento).filter {
                        ($0.tipo == .duda || $0.tipo == .pregunta)
                            && $0.cursoId == curso.id
                    }.count
            }
            let intentos = evaluacion.intentos.filter {
                $0.cursoId == curso.id && intervalo.contains($0.fecha)
            }
            let errores = base.errores.filter {
                $0.cursoId == curso.id && !$0.resuelto && $0.repeticiones > 1
            }.count
            let tarjetas = base.flashcards.filter {
                $0.cursoId == curso.id && !$0.suspendida && $0.proximoRepaso <= fecha
            }.count
            let temasCurso = unicos(base.conocimiento.filter {
                $0.fuente.cursoId == curso.id && intervalo.contains($0.fecha)
            }.flatMap { [$0.titulo] + $0.etiquetas })
            guard !clasesCurso.isEmpty || dudasCurso > 0 || !intentos.isEmpty
                    || errores > 0 || tarjetas > 0 || !temasCurso.isEmpty else {
                return nil
            }
            return ResumenCursoSemanalIPad(
                curso: curso, clases: clasesCurso.count,
                dudasPendientes: dudasCurso,
                ejerciciosResueltos: intentos.filter { $0.correcto }.count,
                erroresRecurrentes: errores, tarjetasPendientes: tarjetas,
                temas: Array(temasCurso.prefix(6)))
        }
        return ResumenSemanalIPad(
            intervalo: intervalo, clases: clases,
            paginasCreadas: documentos.count, dudasPendientes: dudas,
            tareas: tareas, temasEstudiados: temas,
            temasNuncaRevisados: nunca, proximosExamenes: examenes,
            cursos: cursos)
    }

    private static func unicos(_ valores: [String]) -> [String] {
        var vistos = Set<String>()
        return valores.compactMap {
            let limpio = $0.trimmingCharacters(in: .whitespacesAndNewlines)
            let normal = Normalizador.texto(limpio)
            guard !limpio.isEmpty, !vistos.contains(normal) else { return nil }
            vistos.insert(normal); return limpio
        }
    }
}

final class PanelRevisionSemanalIPad: UITableViewController {
    private let resumen = GeneradorRevisionSemanalIPad.generar()

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Revisión semanal"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 6 }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Esta semana", "Por curso", "Dudas pendientes", "Tareas",
         "Temas nunca revisados", "Próximos exámenes"][section]
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 4
        case 1: return max(resumen.cursos.count, 1)
        case 2: return max(resumen.dudasPendientes.count, 1)
        case 3: return max(resumen.tareas.count, 1)
        case 4: return max(resumen.temasNuncaRevisados.count, 1)
        default: return max(resumen.proximosExamenes.count, 1)
        }
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            let valores = [
                ("Clases tomadas", "\(resumen.clases.count)"),
                ("Páginas/documentos creados", "\(resumen.paginasCreadas)"),
                ("Temas estudiados", "\(resumen.temasEstudiados.count)"),
                ("Volviste esta semana", "Sin rachas ni castigo por días ausentes"),
            ]
            celda.textLabel?.text = valores[indexPath.row].0
            celda.detailTextLabel?.text = valores[indexPath.row].1
        case 1:
            guard !resumen.cursos.isEmpty else {
                celda.textLabel?.text = "Todavía no hay actividad por curso"; return celda
            }
            let curso = resumen.cursos[indexPath.row]
            celda.textLabel?.text = curso.curso.nombre
            celda.detailTextLabel?.numberOfLines = 0
            celda.detailTextLabel?.text = "\(curso.clases) clases · \(curso.dudasPendientes) dudas · \(curso.ejerciciosResueltos) ejercicios correctos · \(curso.erroresRecurrentes) errores recurrentes · \(curso.tarjetasPendientes) tarjetas\n\(curso.temas.joined(separator: ", "))"
        case 2:
            celda.textLabel?.text = resumen.dudasPendientes.isEmpty
                ? "Sin dudas pendientes detectadas" : resumen.dudasPendientes[indexPath.row]
        case 3:
            if resumen.tareas.isEmpty { celda.textLabel?.text = "Sin tareas activas" }
            else {
                let tarea = resumen.tareas[indexPath.row]
                celda.textLabel?.text = tarea.titulo
                celda.detailTextLabel?.text = tarea.fechaLimite?.formatted(
                    date: .abbreviated, time: .shortened) ?? "Sin fecha límite"
            }
        case 4:
            if resumen.temasNuncaRevisados.isEmpty {
                celda.textLabel?.text = "No hay temas pendientes de primer repaso"
            } else {
                let concepto = resumen.temasNuncaRevisados[indexPath.row]
                celda.textLabel?.text = concepto.nombre
                celda.detailTextLabel?.text = "Dominio \(Int(concepto.dominio * 100)) %"
            }
        default:
            if resumen.proximosExamenes.isEmpty {
                celda.textLabel?.text = "No hay exámenes fechados en los próximos 35 días"
            } else {
                let examen = resumen.proximosExamenes[indexPath.row]
                celda.textLabel?.text = examen.titulo
                celda.detailTextLabel?.text = examen.fechaLimite?.formatted(
                    date: .complete, time: .shortened)
            }
        }
        return celda
    }
    @objc private func cerrar() { dismiss(animated: true) }
}

private extension DateFormatter {
    static let tituloClase: DateFormatter = {
        let formato = DateFormatter()
        formato.locale = .current
        formato.dateFormat = "d MMM"
        return formato
    }()
}
