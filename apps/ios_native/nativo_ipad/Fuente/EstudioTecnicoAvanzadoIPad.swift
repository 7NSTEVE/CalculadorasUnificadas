import Foundation
import PDFKit
import UIKit
import UniformTypeIdentifiers
import Vision
import VisionKit

// MARK: - Modelos académicos técnicos

enum DificultadProblemaIPad: Int, Codable, CaseIterable {
    case introduccion = 1, practica, examen, desafio
    var nombre: String { ["", "Introducción", "Práctica", "Examen", "Desafío"][rawValue] }
}

enum EstadoProblemaIPad: String, Codable, CaseIterable, Hashable {
    case pendiente, intentando, correcto, incorrecto, repetir, archivado
}

struct SolucionVinculadaIPad: Codable, Equatable {
    var texto = ""
    var hashAdjunto: String?
    var fuente: ReferenciaFuente?
    var ocultaDurantePractica = true
}

struct PasoErrorProblemaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var fecha = Date()
    var descripcion: String
    var respuestaDada = ""
    var correccion = ""
    var elementoId: String?
    var segundoAudio: Double?
}

struct IntentoProblemaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var fecha = Date()
    var duracionSegundos: Double = 0
    var correcto: Bool?
    var confianza: Double = 0.5
    var nota = ""
    var documentoId: UInt64?
    var mostrarSolucion = false
}

struct ProblemaBancoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var tema = "Sin tema"
    var etiquetas: [String] = []
    var dificultad: DificultadProblemaIPad = .practica
    var estado: EstadoProblemaIPad = .pendiente
    var enunciado: String
    var hashImagen: String?
    var fuente: ReferenciaFuente?
    var solucion: SolucionVinculadaIPad?
    var intentos: [IntentoProblemaIPad] = []
    var erroresDePaso: [PasoErrorProblemaIPad] = []
    var problemaPadreId: UUID?
    var esVariacionNumerica = false
    var sustitucionesVariacion: [String] = []
    var fecha = Date()
    var ultimoUso: Date?
}

struct FormulaTecnicaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var expresion: String
    var nombre = ""
    var cursoId: UUID?
    var parcial = ""
    var tema = ""
    var frecuenciaUso = 0
    var favorita = false
    var manuscritaPrincipal = true
    var fuente: ReferenciaFuente?
    var fecha = Date()
}

enum EstadoDudaProfesorIPad: String, Codable { case pendiente, respondida, descartada }

struct DudaProfesorIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var pregunta: String
    var contexto = ""
    var cursoId: UUID?
    var fuente: ReferenciaFuente?
    var estado: EstadoDudaProfesorIPad = .pendiente
    var respuesta = ""
    var fecha = Date()
}

struct MedicionLaboratorioIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var magnitud: String
    var valor: String
    var unidad = ""
    var incertidumbre = ""
    var instante = Date()
}

struct SesionLaboratorioIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var titulo: String
    var objetivo = ""
    var hipotesis = ""
    var equipo: [String] = []
    var procedimiento: [String] = []
    var mediciones: [MedicionLaboratorioIPad] = []
    var riesgos: [String] = []
    var hallazgos: [String] = []
    var hashesAdjuntos: [String] = []
    var inicio = Date()
    var fin: Date?
}

enum ModoTrabajoTecnicoIPad: String, Codable, CaseIterable, Identifiable {
    case estudiarClase, officeHours, assignment, laboratorio, examen
    case panicoPreExamen, autoBusCaminando
    var id: String { rawValue }
    var nombre: String {
        switch self {
        case .estudiarClase: return "Estudiar esta clase"
        case .officeHours: return "Office Hours"
        case .assignment: return "Assignment"
        case .laboratorio: return "Laboratorio"
        case .examen: return "Examen"
        case .panicoPreExamen: return "Urgencia pre-examen"
        case .autoBusCaminando: return "Auto / bus / caminando"
        }
    }
    var detalle: String {
        switch self {
        case .estudiarClase: return "Recuerdo activo, errores, fórmulas y fuentes de una clase."
        case .officeHours: return "Dudas con contexto exacto y espacio para la respuesta."
        case .assignment: return "Enunciado, pasos, evidencia, rúbrica y entrega."
        case .laboratorio: return "Objetivo, equipo, procedimiento, mediciones y hallazgos."
        case .examen: return "Práctica cronometrada sin mostrar soluciones."
        case .panicoPreExamen: return "Prioriza dependencias y errores con las horas disponibles."
        case .autoBusCaminando: return "Estudio oral y controles grandes, sin exigir escritura."
        }
    }
}

struct SesionTrabajoTecnicoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var modo: ModoTrabajoTecnicoIPad
    var cursoId: UUID?
    var objetivo: String
    var inicio = Date()
    var fin: Date?
    var documentoId: UInt64?
    var elementos: [UUID] = []
}

struct PlantillaProblemaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var ancho: Double
    var alto: Double
    var columnas = 1
    var zonas: [String] = ["Enunciado", "Desarrollo", "Resultado", "Error"]
}

struct PaqueteCursoTecnicoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID
    var nombre: String
    var creado = Date()
    var problemaIds: [UUID] = []
    var formulaIds: [UUID] = []
    var conceptoIds: [UUID] = []
    var fuenteIds: [UUID] = []
    var tareaIds: [UUID] = []
    var errorIds: [UUID] = []
}

private struct EstadoEstudioTecnicoIPad: Codable {
    var version = 1
    var problemas: [ProblemaBancoIPad] = []
    var formulas: [FormulaTecnicaIPad] = []
    var dudas: [DudaProfesorIPad] = []
    var laboratorios: [SesionLaboratorioIPad] = []
    var sesiones: [SesionTrabajoTecnicoIPad] = []
    var plantillas: [PlantillaProblemaIPad] = []
    var paquetes: [PaqueteCursoTecnicoIPad] = []
}

struct InstantaneaEstudioTecnicoIPad {
    var problemas: [ProblemaBancoIPad]
    var formulas: [FormulaTecnicaIPad]
    var dudas: [DudaProfesorIPad]
    var laboratorios: [SesionLaboratorioIPad]
    var sesiones: [SesionTrabajoTecnicoIPad]
    var plantillas: [PlantillaProblemaIPad]
    var paquetes: [PaqueteCursoTecnicoIPad]
}

struct FiltroProblemasIPad {
    var cursoId: UUID?
    var tema: String?
    var dificultad: DificultadProblemaIPad?
    var estados: Set<EstadoProblemaIPad> = []
    var soloConErrores = false
    var texto = ""
}

// MARK: - Fuente de verdad local del estudio técnico

final class CentroEstudioTecnicoIPad {
    static let compartido = CentroEstudioTecnicoIPad()
    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoEstudioTecnicoIPad

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask)[0]
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("estudio-tecnico-ipad.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(EstadoEstudioTecnicoIPad.self, from: datos) {
            estado = leido
        } else {
            estado = EstadoEstudioTecnicoIPad()
        }
    }

    func instantanea() -> InstantaneaEstudioTecnicoIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return InstantaneaEstudioTecnicoIPad(
            problemas: estado.problemas, formulas: estado.formulas,
            dudas: estado.dudas, laboratorios: estado.laboratorios,
            sesiones: estado.sesiones, plantillas: estado.plantillas,
            paquetes: estado.paquetes)
    }

    @discardableResult
    private func cambiar<T>(_ cuerpo: (inout EstadoEstudioTecnicoIPad) -> T) -> T {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let resultado = cuerpo(&estado)
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        if let datos = try? encoder.encode(estado) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
        return resultado
    }

    @discardableResult
    func agregarProblema(_ enunciado: String, cursoId: UUID?, tema: String,
                         dificultad: DificultadProblemaIPad,
                         fuente: ReferenciaFuente? = nil,
                         hashImagen: String? = nil) -> ProblemaBancoIPad {
        cambiar { estado in
            let normal = Normalizador.texto(enunciado)
            if let existente = estado.problemas.first(where: {
                Normalizador.texto($0.enunciado) == normal
                    && $0.cursoId == cursoId && $0.fuente == fuente
            }) { return existente }
            let problema = ProblemaBancoIPad(
                cursoId: cursoId, tema: tema.isEmpty ? "Sin tema" : tema,
                etiquetas: EtiquetadorLocal.etiquetas(enunciado),
                dificultad: dificultad, enunciado: enunciado,
                hashImagen: hashImagen, fuente: fuente)
            estado.problemas.append(problema)
            return problema
        }
    }

    func buscarProblemas(_ filtro: FiltroProblemasIPad) -> [ProblemaBancoIPad] {
        instantanea().problemas.filter { problema in
            (filtro.cursoId == nil || problema.cursoId == filtro.cursoId)
                && (filtro.tema == nil || Normalizador.texto(problema.tema)
                    == Normalizador.texto(filtro.tema ?? ""))
                && (filtro.dificultad == nil || problema.dificultad == filtro.dificultad)
                && (filtro.estados.isEmpty || filtro.estados.contains(problema.estado))
                && (!filtro.soloConErrores || !problema.erroresDePaso.isEmpty)
                && (filtro.texto.isEmpty || Normalizador.texto(
                    problema.enunciado + " " + problema.tema + " "
                        + problema.etiquetas.joined(separator: " "))
                    .contains(Normalizador.texto(filtro.texto)))
        }.sorted { ($0.ultimoUso ?? $0.fecha) > ($1.ultimoUso ?? $1.fecha) }
    }

    func practicaAleatoria(_ cantidad: Int, filtro: FiltroProblemasIPad)
        -> [ProblemaBancoIPad] {
        Array(buscarProblemas(filtro).filter { $0.estado != .archivado }
            .shuffled().prefix(max(1, cantidad)))
    }

    @discardableResult
    func iniciarIntento(_ problemaId: UUID, documentoId: UInt64?) -> IntentoProblemaIPad? {
        cambiar { estado in
            guard let indice = estado.problemas.firstIndex(where: { $0.id == problemaId })
            else { return nil }
            let intento = IntentoProblemaIPad(documentoId: documentoId,
                                               mostrarSolucion: false)
            estado.problemas[indice].intentos.append(intento)
            estado.problemas[indice].estado = .intentando
            estado.problemas[indice].ultimoUso = Date()
            return intento
        }
    }

    func cerrarIntento(_ problemaId: UUID, intentoId: UUID, correcto: Bool?,
                       confianza: Double, segundos: Double, nota: String) {
        cambiar { estado in
            guard let p = estado.problemas.firstIndex(where: { $0.id == problemaId }),
                  let i = estado.problemas[p].intentos.firstIndex(where: {
                      $0.id == intentoId
                  }) else { return }
            estado.problemas[p].intentos[i].correcto = correcto
            estado.problemas[p].intentos[i].confianza = min(max(confianza, 0), 1)
            estado.problemas[p].intentos[i].duracionSegundos = max(0, segundos)
            estado.problemas[p].intentos[i].nota = nota
            estado.problemas[p].estado = correcto == true ? .correcto
                : (correcto == false ? .repetir : .pendiente)
        }
    }

    func vincularSolucion(_ solucion: SolucionVinculadaIPad, problemaId: UUID) {
        cambiar { estado in
            guard let indice = estado.problemas.firstIndex(where: { $0.id == problemaId })
            else { return }
            estado.problemas[indice].solucion = solucion
        }
    }

    func registrarError(_ error: PasoErrorProblemaIPad, problemaId: UUID) {
        cambiar { estado in
            guard let indice = estado.problemas.firstIndex(where: { $0.id == problemaId })
            else { return }
            estado.problemas[indice].erroresDePaso.append(error)
            estado.problemas[indice].estado = .repetir
            let problema = estado.problemas[indice]
            _ = CentroAcademico.compartido.registrarError(
                cursoId: problema.cursoId, conceptoId: nil,
                enunciado: problema.enunciado, respuestaDada: error.respuestaDada,
                correcta: error.correccion, causa: error.descripcion,
                referencia: problema.fuente)
        }
    }

    @discardableResult
    func crearVariacionNumerica(_ problemaId: UUID) -> ProblemaBancoIPad? {
        cambiar { estado in
            guard let original = estado.problemas.first(where: { $0.id == problemaId })
            else { return nil }
            let resultado = VariadorNumericoLocalIPad.variar(original.enunciado)
            guard resultado.texto != original.enunciado else { return nil }
            var copia = original
            copia.id = UUID(); copia.enunciado = resultado.texto
            copia.estado = .pendiente; copia.intentos = []; copia.erroresDePaso = []
            copia.solucion = nil; copia.problemaPadreId = original.id
            copia.esVariacionNumerica = true
            copia.sustitucionesVariacion = resultado.cambios
            copia.fecha = Date(); copia.ultimoUso = nil
            estado.problemas.append(copia)
            return copia
        }
    }

    @discardableResult
    func agregarFormula(_ expresion: String, nombre: String, cursoId: UUID?,
                        tema: String, parcial: String,
                        fuente: ReferenciaFuente?) -> FormulaTecnicaIPad {
        cambiar { estado in
            let normal = Normalizador.texto(expresion)
            if let indice = estado.formulas.firstIndex(where: {
                Normalizador.texto($0.expresion) == normal && $0.cursoId == cursoId
            }) {
                estado.formulas[indice].frecuenciaUso += 1
                return estado.formulas[indice]
            }
            let formula = FormulaTecnicaIPad(
                expresion: expresion, nombre: nombre, cursoId: cursoId,
                parcial: parcial, tema: tema, frecuenciaUso: 1, fuente: fuente)
            estado.formulas.append(formula)
            return formula
        }
    }

    func alternarFormulaFavorita(_ id: UUID) {
        cambiar { estado in
            guard let indice = estado.formulas.firstIndex(where: { $0.id == id }) else {
                return
            }
            estado.formulas[indice].favorita.toggle()
        }
    }

    @discardableResult
    func agregarDuda(_ pregunta: String, contexto: String, cursoId: UUID?,
                     fuente: ReferenciaFuente?) -> DudaProfesorIPad {
        cambiar { estado in
            let duda = DudaProfesorIPad(pregunta: pregunta, contexto: contexto,
                                         cursoId: cursoId, fuente: fuente)
            estado.dudas.append(duda); return duda
        }
    }

    func responderDuda(_ id: UUID, respuesta: String) {
        cambiar { estado in
            guard let indice = estado.dudas.firstIndex(where: { $0.id == id }) else {
                return
            }
            estado.dudas[indice].respuesta = respuesta
            estado.dudas[indice].estado = .respondida
        }
    }

    @discardableResult
    func iniciarLaboratorio(_ titulo: String, cursoId: UUID?) -> SesionLaboratorioIPad {
        cambiar { estado in
            let laboratorio = SesionLaboratorioIPad(cursoId: cursoId, titulo: titulo)
            estado.laboratorios.append(laboratorio); return laboratorio
        }
    }

    func actualizarLaboratorio(_ laboratorio: SesionLaboratorioIPad) {
        cambiar { estado in
            guard let indice = estado.laboratorios.firstIndex(where: {
                $0.id == laboratorio.id
            }) else { return }
            estado.laboratorios[indice] = laboratorio
        }
    }

    @discardableResult
    func iniciarSesion(_ modo: ModoTrabajoTecnicoIPad, cursoId: UUID?,
                       objetivo: String, documentoId: UInt64?) -> SesionTrabajoTecnicoIPad {
        cambiar { estado in
            let sesion = SesionTrabajoTecnicoIPad(
                modo: modo, cursoId: cursoId, objetivo: objetivo,
                documentoId: documentoId)
            estado.sesiones.append(sesion); return sesion
        }
    }

    @discardableResult
    func guardarPlantilla(_ plantilla: PlantillaProblemaIPad) -> PlantillaProblemaIPad {
        cambiar { estado in
            if let indice = estado.plantillas.firstIndex(where: {
                $0.id == plantilla.id
            }) { estado.plantillas[indice] = plantilla }
            else { estado.plantillas.append(plantilla) }
            return plantilla
        }
    }

    @discardableResult
    func construirPaquete(cursoId: UUID, nombre: String) -> PaqueteCursoTecnicoIPad {
        let academico = CentroAcademico.compartido.instantanea()
        return cambiar { estado in
            var paquete = PaqueteCursoTecnicoIPad(cursoId: cursoId, nombre: nombre)
            paquete.problemaIds = estado.problemas.filter { $0.cursoId == cursoId }.map(\.id)
            paquete.formulaIds = estado.formulas.filter { $0.cursoId == cursoId }.map(\.id)
            paquete.conceptoIds = academico.conceptos.filter { $0.cursoId == cursoId }.map(\.id)
            paquete.fuenteIds = academico.conocimiento.filter {
                $0.fuente.cursoId == cursoId
            }.map { $0.fuente.id }
            paquete.tareaIds = academico.tareas.filter { $0.cursoId == cursoId }.map(\.id)
            paquete.errorIds = academico.errores.filter { $0.cursoId == cursoId }.map(\.id)
            if let indice = estado.paquetes.firstIndex(where: {
                $0.cursoId == cursoId && Normalizador.texto($0.nombre)
                    == Normalizador.texto(nombre)
            }) {
                paquete.id = estado.paquetes[indice].id
                estado.paquetes[indice] = paquete
            } else { estado.paquetes.append(paquete) }
            return paquete
        }
    }

    func rutaPrerequisitos(hasta conceptoId: UUID) -> [ConceptoDominio] {
        let conceptos = CentroAcademico.compartido.instantanea().conceptos
        let porId = Dictionary(uniqueKeysWithValues: conceptos.map { ($0.id, $0) })
        var visitados = Set<UUID>(); var salida: [ConceptoDominio] = []
        func visitar(_ id: UUID) {
            guard visitados.insert(id).inserted, let concepto = porId[id] else { return }
            for previo in concepto.prerrequisitos { visitar(previo) }
            salida.append(concepto)
        }
        visitar(conceptoId)
        return salida
    }
}

// MARK: - Extracción local y variaciones sin inventar soluciones

enum ExtractorEjerciciosLocalIPad {
    static func desdeTexto(_ texto: String) -> [String] {
        let lineas = texto.components(separatedBy: .newlines)
        let patron = try? NSRegularExpression(
            pattern: #"^\s*(?:problema|ejercicio|pregunta)?\s*(?:\d+|[A-Z])(?:[.)\-:])\s+"#,
            options: [.caseInsensitive])
        var salida: [String] = []; var actual: [String] = []
        for linea in lineas {
            let rango = NSRange(linea.startIndex..<linea.endIndex, in: linea)
            let inicia = patron?.firstMatch(in: linea, range: rango) != nil
            if inicia, !actual.isEmpty {
                let bloque = actual.joined(separator: "\n").trimmingCharacters(
                    in: .whitespacesAndNewlines)
                if bloque.count > 8 { salida.append(bloque) }
                actual = []
            }
            if inicia || !actual.isEmpty { actual.append(linea) }
        }
        let ultimo = actual.joined(separator: "\n").trimmingCharacters(
            in: .whitespacesAndNewlines)
        if ultimo.count > 8 { salida.append(ultimo) }
        if salida.isEmpty, texto.trimmingCharacters(in: .whitespacesAndNewlines).count > 8 {
            return [texto.trimmingCharacters(in: .whitespacesAndNewlines)]
        }
        return salida
    }

    static func desdePDF(_ url: URL) -> [(pagina: Int, enunciado: String)] {
        guard let documento = PDFDocument(url: url) else { return [] }
        var salida: [(Int, String)] = []
        for pagina in 0..<documento.pageCount {
            guard let texto = documento.page(at: pagina)?.string else { continue }
            salida.append(contentsOf: desdeTexto(texto).map { (pagina, $0) })
        }
        return salida
    }

    static func desdeImagen(_ imagen: CGImage,
                            completado: @escaping ([String]) -> Void) {
        let solicitud = VNRecognizeTextRequest { solicitud, _ in
            let lineas = (solicitud.results as? [VNRecognizedTextObservation])?
                .compactMap { $0.topCandidates(1).first?.string } ?? []
            completado(desdeTexto(lineas.joined(separator: "\n")))
        }
        solicitud.recognitionLevel = .accurate
        solicitud.usesLanguageCorrection = true
        solicitud.recognitionLanguages = ["es-ES", "en-US"]
        solicitud.customWords = ["Thévenin", "Kirchhoff", "Norton", "MOSFET",
                                  "KCL", "KVL", "op-amp", "fasor"]
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                try VNImageRequestHandler(cgImage: imagen).perform([solicitud])
            } catch {
                completado([])
            }
        }
    }
}

private enum VariadorNumericoLocalIPad {
    struct Resultado { var texto: String; var cambios: [String] }

    static func variar(_ texto: String) -> Resultado {
        guard let expresion = try? NSRegularExpression(
            pattern: #"(?<![A-Za-z_])[-+]?\d+(?:[.,]\d+)?(?:[eE][-+]?\d+)?"#)
        else { return Resultado(texto: texto, cambios: []) }
        let ns = texto as NSString
        let coincidencias = expresion.matches(
            in: texto, range: NSRange(location: 0, length: ns.length))
        var salida = texto; var cambios: [String] = []
        for coincidencia in coincidencias.reversed() {
            let original = ns.substring(with: coincidencia.range)
            guard let valor = Double(original.replacingOccurrences(of: ",", with: ".")),
                  valor != 0 else { continue }
            let factores = [0.6, 0.75, 0.8, 1.2, 1.25, 1.5]
            let nuevo = valor * (factores.randomElement() ?? 1.2)
            let reemplazo = abs(nuevo.rounded() - nuevo) < 0.000_001
                ? String(Int(nuevo.rounded()))
                : String(format: "%.3g", nuevo)
            guard let rango = Range(coincidencia.range, in: salida) else { continue }
            salida.replaceSubrange(rango, with: reemplazo)
            cambios.append("\(original) → \(reemplazo)")
        }
        return Resultado(texto: salida, cambios: Array(cambios.reversed()))
    }
}

// MARK: - Centro visual de estudio técnico

private struct CandidatoEjercicioIPad: Identifiable {
    var id = UUID()
    var enunciado: String
    var pagina: Int?
    var hashImagen: String?
    var fuente: ReferenciaFuente?
    var seleccionado = true
}

final class PanelEstudioTecnicoIPad: UITableViewController {
    var alInsertarTexto: ((String) -> Void)?

    private let documentoId: UInt64
    private let fuenteActual: ReferenciaFuente?
    private let textoActual: String
    private var cursoId: UUID?

    init(documentoId: UInt64, cursoId: UUID?, fuente: ReferenciaFuente?,
         textoSeleccionado: String) {
        self.documentoId = documentoId
        self.cursoId = cursoId
        self.fuenteActual = fuente
        self.textoActual = textoSeleccionado
        super.init(style: .insetGrouped)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Estudio técnico"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Curso", style: .plain, target: self, action: #selector(elegirCurso))
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 4 }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return ModoTrabajoTecnicoIPad.allCases.count
        case 1: return 5
        case 2: return 3
        default: return 4
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Modos de trabajo", "Problemas y práctica",
         "Conocimiento técnico", "Curso y seguimiento"][section]
    }

    override func tableView(_ tableView: UITableView,
                            titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0: return "El modo cambia el flujo, no reescribe ni oculta permanentemente sus apuntes."
        case 1: return "Las soluciones permanecen ocultas hasta que usted decide mostrarlas."
        case 2: return "Fórmulas y dependencias conservan un enlace a la fuente original."
        default: return "Todos los datos se guardan localmente en el iPad."
        }
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.accessoryType = .disclosureIndicator
        if indexPath.section == 0 {
            let modo = ModoTrabajoTecnicoIPad.allCases[indexPath.row]
            celda.textLabel?.text = modo.nombre
            celda.detailTextLabel?.text = modo.detalle
            celda.imageView?.image = UIImage(systemName: simbolo(modo))
        } else if indexPath.section == 1 {
            let filas = [
                ("Banco de problemas", "Filtrar, repetir, comparar y anotar errores", "tray.full"),
                ("Guardar selección como problema", "Conserva página y objeto de origen", "plus.square"),
                ("Extraer ejercicios de PDF", "Propone límites antes de importar", "doc.text.magnifyingglass"),
                ("Escanear ejercicios físicos", "Escáner + OCR local; usted confirma", "camera.viewfinder"),
                ("Práctica aleatoria", "Usa únicamente problemas que ya guardó", "shuffle")]
            let fila = filas[indexPath.row]
            celda.textLabel?.text = fila.0; celda.detailTextLabel?.text = fila.1
            celda.imageView?.image = UIImage(systemName: fila.2)
        } else if indexPath.section == 2 {
            let filas = [
                ("Formula Sheet dinámica", "Curso, parcial, tema y frecuencia", "function"),
                ("Dependency Graph", "Prerrequisitos y punto real de bloqueo", "point.3.connected.trianglepath.dotted"),
                ("Variables, prefijos y unidades", "Ayuda de ingeniería sin sustituir el cálculo", "ruler")]
            let fila = filas[indexPath.row]
            celda.textLabel?.text = fila.0; celda.detailTextLabel?.text = fila.1
            celda.imageView?.image = UIImage(systemName: fila.2)
        } else {
            let filas = [
                ("Course Pack", "Materiales, fórmulas, errores y ejercicios", "shippingbox"),
                ("Dudas para el profesor", "Office Hours con contexto exacto", "questionmark.bubble"),
                ("Cuaderno de laboratorio", "Procedimiento, mediciones y hallazgos", "testtube.2"),
                ("Layouts reutilizables", "Estructura limpia para volver a intentar", "rectangle.3.group")]
            let fila = filas[indexPath.row]
            celda.textLabel?.text = fila.0; celda.detailTextLabel?.text = fila.1
            celda.imageView?.image = UIImage(systemName: fila.2)
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch (indexPath.section, indexPath.row) {
        case (0, _): iniciarModo(ModoTrabajoTecnicoIPad.allCases[indexPath.row])
        case (1, 0): abrirBanco()
        case (1, 1): guardarSeleccionComoProblema()
        case (1, 2): elegirPDF()
        case (1, 3): escanearEjercicios()
        case (1, 4): practicaAleatoria()
        case (2, 0): abrirFormulas()
        case (2, 1): abrirDependencias()
        case (2, 2): mostrarPrefijosYUnidades()
        case (3, 0): abrirCoursePack()
        case (3, 1): abrirDudas()
        case (3, 2): abrirLaboratorios()
        case (3, 3): abrirLayouts()
        default: break
        }
    }

    private func iniciarModo(_ modo: ModoTrabajoTecnicoIPad) {
        let alerta = UIAlertController(title: modo.nombre, message: modo.detalle,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Objetivo en una frase" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Iniciar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let objetivo = alerta.textFields?.first?.text?.trimmingCharacters(
                in: .whitespacesAndNewlines) ?? ""
            _ = CentroEstudioTecnicoIPad.compartido.iniciarSesion(
                modo, cursoId: self.cursoId, objetivo: objetivo,
                documentoId: self.documentoId)
            if modo == .panicoPreExamen {
                self.mostrarPlanUrgencia()
            } else if modo == .autoBusCaminando {
                self.present(UINavigationController(rootViewController:
                    PanelEstudioOralIPad()), animated: true)
            } else {
                self.mostrarMensaje("Modo iniciado. El documento original permanece intacto.")
            }
        })
        present(alerta, animated: true)
    }

    private func mostrarPlanUrgencia() {
        let alerta = UIAlertController(title: "Horas disponibles", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.text = "4"; $0.keyboardType = .decimalPad }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Priorizar", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let horas = Double(alerta.textFields?.first?.text?.replacingOccurrences(
                of: ",", with: ".") ?? "") ?? 4
            let tareas = CentroAcademico.compartido.planUrgencia(
                horas: horas, cursoId: self.cursoId)
            let texto = tareas.enumerated().map {
                "\($0.offset + 1). \($0.element.titulo) · \($0.element.estimadoMinutos) min"
            }.joined(separator: "\n")
            self.mostrarMensaje(texto.isEmpty
                ? "No hay tareas locales suficientes para construir el plan."
                : texto, titulo: "Plan sin castigar atrasos")
        })
        present(alerta, animated: true)
    }

    private func abrirBanco() {
        let panel = PanelBancoProblemasIPad(cursoId: cursoId,
                                             documentoId: documentoId)
        panel.alInsertarEnunciado = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    private func guardarSeleccionComoProblema() {
        let alerta = UIAlertController(title: "Guardar problema", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { campo in
            campo.placeholder = "Enunciado"
            campo.text = self.textoActual
        }
        alerta.addTextField { $0.placeholder = "Tema" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self, let enunciado = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines),
                  !enunciado.isEmpty else { return }
            _ = CentroEstudioTecnicoIPad.compartido.agregarProblema(
                enunciado, cursoId: self.cursoId,
                tema: alerta.textFields?[1].text ?? "",
                dificultad: .practica, fuente: self.fuenteActual)
            self.mostrarMensaje("Problema guardado con su referencia de origen.")
        })
        present(alerta, animated: true)
    }

    private func elegirPDF() {
        let selector = UIDocumentPickerViewController(forOpeningContentTypes: [.pdf],
                                                      asCopy: false)
        selector.delegate = self; selector.allowsMultipleSelection = false
        present(selector, animated: true)
    }

    private func escanearEjercicios() {
        guard VNDocumentCameraViewController.isSupported else {
            mostrarMensaje("El escáner de documentos no está disponible en este iPad.")
            return
        }
        let escaner = VNDocumentCameraViewController(); escaner.delegate = self
        present(escaner, animated: true)
    }

    private func practicaAleatoria() {
        var filtro = FiltroProblemasIPad(); filtro.cursoId = cursoId
        let problemas = CentroEstudioTecnicoIPad.compartido.practicaAleatoria(
            5, filtro: filtro)
        guard !problemas.isEmpty else {
            mostrarMensaje("Primero guarde o importe problemas de este curso.")
            return
        }
        let panel = PanelPracticaProblemasIPad(
            problemas: problemas, documentoId: documentoId)
        panel.alInsertarEnunciado = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    private func abrirFormulas() {
        let panel = PanelFormulaSheetIPad(cursoId: cursoId, fuenteInicial: fuenteActual,
                                          expresionInicial: textoActual)
        panel.alInsertar = alInsertarTexto
        navigationController?.pushViewController(panel, animated: true)
    }

    private func abrirDependencias() {
        navigationController?.pushViewController(
            PanelDependenciasConceptosIPad(cursoId: cursoId), animated: true)
    }

    private func mostrarPrefijosYUnidades() {
        let texto = """
        Prefijos: p 10⁻¹² · n 10⁻⁹ · µ 10⁻⁶ · m 10⁻³ · k 10³ · M 10⁶ · G 10⁹

        La calculadora de Ingeniería valida dimensiones y puede evaluar expresiones con unidades. Esta ayuda no modifica lo escrito ni elige automáticamente entre m y M.
        """
        let alerta = UIAlertController(title: "Prefijos y Units Engine", message: texto,
                                       preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(title: "Copiar µ", style: .default) { _ in
            UIPasteboard.general.string = "µ"
        })
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }

    private func abrirCoursePack() {
        navigationController?.pushViewController(
            PanelCoursePackIPad(cursoId: cursoId), animated: true)
    }

    private func abrirDudas() {
        navigationController?.pushViewController(
            PanelDudasProfesorIPad(cursoId: cursoId, fuenteInicial: fuenteActual,
                                    contextoInicial: textoActual), animated: true)
    }

    private func abrirLaboratorios() {
        navigationController?.pushViewController(
            PanelLaboratoriosIPad(cursoId: cursoId), animated: true)
    }

    private func abrirLayouts() {
        navigationController?.pushViewController(
            PanelLayoutsProblemaIPad(alInsertar: alInsertarTexto), animated: true)
    }

    @objc private func elegirCurso() {
        let cursos = CentroAcademico.compartido.instantanea().cursos.filter { !$0.archivado }
        let menu = UIAlertController(title: "Curso", message: nil,
                                     preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Sin curso", style: .default) {
            [weak self] _ in self?.cursoId = nil
        })
        for curso in cursos {
            menu.addAction(UIAlertAction(title: curso.nombre, style: .default) {
                [weak self] _ in self?.cursoId = curso.id
            })
        }
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.barButtonItem = navigationItem.rightBarButtonItem
        present(menu, animated: true)
    }

    private func presentarCandidatos(_ candidatos: [CandidatoEjercicioIPad]) {
        guard !candidatos.isEmpty else {
            mostrarMensaje("No se detectaron límites de ejercicios. El original no fue modificado.")
            return
        }
        let panel = PanelPropuestaEjerciciosIPad(candidatos: candidatos,
                                                  cursoId: cursoId)
        navigationController?.pushViewController(panel, animated: true)
    }

    private func mostrarMensaje(_ mensaje: String, titulo: String? = nil) {
        let alerta = UIAlertController(title: titulo, message: mensaje,
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Vale", style: .default))
        present(alerta, animated: true)
    }

    private func simbolo(_ modo: ModoTrabajoTecnicoIPad) -> String {
        switch modo {
        case .estudiarClase: return "book"
        case .officeHours: return "person.2"
        case .assignment: return "doc.text"
        case .laboratorio: return "testtube.2"
        case .examen: return "timer"
        case .panicoPreExamen: return "exclamationmark.triangle"
        case .autoBusCaminando: return "figure.walk"
        }
    }

    @objc private func cerrar() { dismiss(animated: true) }
}

extension PanelEstudioTecnicoIPad: UIDocumentPickerDelegate {
    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard let origen = urls.first else { return }
        let curso = cursoId
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let adjunto = try RepositorioAdjuntos.compartido.importar(url: origen)
                let extraidos = ExtractorEjerciciosLocalIPad.desdePDF(adjunto.url)
                let candidatos = extraidos.map { item in
                    CandidatoEjercicioIPad(
                        enunciado: item.enunciado, pagina: item.pagina,
                        fuente: ReferenciaFuente(
                            tipo: .libro, titulo: adjunto.nombreOriginal,
                            cursoId: curso, hashArchivo: adjunto.hash,
                            pagina: item.pagina))
                }
                DispatchQueue.main.async { self?.presentarCandidatos(candidatos) }
            } catch {
                DispatchQueue.main.async {
                    self?.mostrarMensaje(
                        "No se pudo copiar el PDF al almacenamiento local.")
                }
            }
        }
    }
}

extension PanelEstudioTecnicoIPad: VNDocumentCameraViewControllerDelegate {
    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFinishWith scan: VNDocumentCameraScan) {
        controller.dismiss(animated: true)
        let grupo = DispatchGroup(); let bloqueo = NSLock()
        var salida: [CandidatoEjercicioIPad] = []
        for indice in 0..<scan.pageCount {
            let imagen = scan.imageOfPage(at: indice)
            guard let cg = imagen.cgImage else { continue }
            let adjunto = try? RepositorioAdjuntos.compartido.importar(
                imagen: imagen, nombre: "Ejercicios escaneados \(indice + 1).png")
            grupo.enter()
            ExtractorEjerciciosLocalIPad.desdeImagen(cg) { ejercicios in
                let candidatos = ejercicios.map {
                    CandidatoEjercicioIPad(
                        enunciado: $0, pagina: indice,
                        hashImagen: adjunto?.hash,
                        fuente: ReferenciaFuente(
                            tipo: .apuntePropio,
                            titulo: "Escaneo físico, página \(indice + 1)",
                            cursoId: self.cursoId,
                            hashArchivo: adjunto?.hash, pagina: indice))
                }
                bloqueo.lock(); salida.append(contentsOf: candidatos); bloqueo.unlock()
                grupo.leave()
            }
        }
        grupo.notify(queue: .main) { [weak self] in
            self?.presentarCandidatos(salida.sorted {
                ($0.pagina ?? 0) < ($1.pagina ?? 0)
            })
        }
    }

    func documentCameraViewControllerDidCancel(_ controller: VNDocumentCameraViewController) {
        controller.dismiss(animated: true)
    }

    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFailWithError error: Error) {
        controller.dismiss(animated: true) { [weak self] in
            self?.mostrarMensaje("El escáner no pudo completar la captura.")
        }
    }
}

private final class PanelPropuestaEjerciciosIPad: UITableViewController {
    private var candidatos: [CandidatoEjercicioIPad]
    private let cursoId: UUID?

    init(candidatos: [CandidatoEjercicioIPad], cursoId: UUID?) {
        self.candidatos = candidatos; self.cursoId = cursoId
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Confirmar ejercicios"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Importar", style: .done, target: self, action: #selector(importar))
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { candidatos.count }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let item = candidatos[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = String(item.enunciado.prefix(150))
        celda.detailTextLabel?.text = item.pagina.map { "Página \($0 + 1)" }
        celda.accessoryType = item.seleccionado ? .checkmark : .none
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        candidatos[indexPath.row].seleccionado.toggle()
        tableView.reloadRows(at: [indexPath], with: .none)
    }
    @objc private func importar() {
        for item in candidatos where item.seleccionado {
            _ = CentroEstudioTecnicoIPad.compartido.agregarProblema(
                item.enunciado, cursoId: cursoId, tema: "Importado",
                dificultad: .practica, fuente: item.fuente,
                hashImagen: item.hashImagen)
        }
        navigationController?.popViewController(animated: true)
    }
}

// MARK: - Problem Bank, práctica limpia y comparación

private final class PanelBancoProblemasIPad: UITableViewController,
    UISearchResultsUpdating {
    var alInsertarEnunciado: ((String) -> Void)?
    private let cursoId: UUID?
    private let documentoId: UInt64
    private var problemas: [ProblemaBancoIPad] = []
    private let buscador = UISearchController(searchResultsController: nil)

    init(cursoId: UUID?, documentoId: UInt64) {
        self.cursoId = cursoId; self.documentoId = documentoId
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Banco de problemas"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(agregar))
        buscador.searchResultsUpdater = self
        buscador.obscuresBackgroundDuringPresentation = false
        navigationItem.searchController = buscador
        refrescar()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated); refrescar()
    }

    func updateSearchResults(for searchController: UISearchController) { refrescar() }

    private func refrescar() {
        var filtro = FiltroProblemasIPad(); filtro.cursoId = cursoId
        filtro.texto = buscador.searchBar.text ?? ""
        problemas = CentroEstudioTecnicoIPad.compartido.buscarProblemas(filtro)
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 1 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(problemas.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !problemas.isEmpty else {
            celda.textLabel?.text = "Sin problemas guardados"
            celda.detailTextLabel?.text = "Importe un examen o guarde una selección."
            celda.selectionStyle = .none; return celda
        }
        let problema = problemas[indexPath.row]
        celda.textLabel?.text = String(problema.enunciado.prefix(140))
        celda.detailTextLabel?.text = "\(problema.tema) · \(problema.dificultad.nombre) · \(problema.estado.rawValue)"
            + (problema.erroresDePaso.isEmpty
               ? "" : " · \(problema.erroresDePaso.count) error(es)")
        celda.imageView?.image = UIImage(systemName: problema.solucion == nil
                                         ? "square.and.pencil" : "eye.slash")
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard problemas.indices.contains(indexPath.row) else { return }
        acciones(problemas[indexPath.row])
    }

    private func acciones(_ problema: ProblemaBancoIPad) {
        let menu = UIAlertController(title: problema.tema,
                                     message: String(problema.enunciado.prefix(500)),
                                     preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Intentar de nuevo limpio", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let panel = PanelIntentoProblemaIPad(problema: problema,
                                                  documentoId: self.documentoId)
            self.navigationController?.pushViewController(panel, animated: true)
        })
        menu.addAction(UIAlertAction(title: "Insertar en la nota", style: .default) {
            [weak self] _ in self?.alInsertarEnunciado?(problema.enunciado)
        })
        menu.addAction(UIAlertAction(title: "Vincular solución", style: .default) {
            [weak self] _ in self?.pedirSolucion(problema)
        })
        let accionComp = UIAlertAction(title: "Comparar con solución", style: .default) {
            [weak self] _ in
            let panel = PanelComparadorSolucionIPad(problema: problema)
            self?.navigationController?.pushViewController(panel, animated: true)
        }
        accionComp.isEnabled = problema.solucion != nil
        menu.addAction(accionComp)
        menu.addAction(UIAlertAction(title: "Anotar error exacto", style: .default) {
            [weak self] _ in self?.pedirError(problema)
        })
        menu.addAction(UIAlertAction(title: "Crear variación numérica", style: .default) {
            [weak self] _ in
            _ = CentroEstudioTecnicoIPad.compartido.crearVariacionNumerica(problema.id)
            self?.refrescar()
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.sourceView = view
        present(menu, animated: true)
    }

    private func pedirSolucion(_ problema: ProblemaBancoIPad) {
        let alerta = UIAlertController(
            title: "Vincular solución",
            message: "Se mantendrá oculta durante la práctica hasta pulsar Mostrar solución.",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Solución o referencia" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Vincular", style: .default) {
            [weak self] _ in
            let texto = alerta.textFields?.first?.text ?? ""
            CentroEstudioTecnicoIPad.compartido.vincularSolucion(
                SolucionVinculadaIPad(texto: texto), problemaId: problema.id)
            self?.refrescar()
        })
        present(alerta, animated: true)
    }

    private func pedirError(_ problema: ProblemaBancoIPad) {
        let alerta = UIAlertController(title: "Este fue mi error", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Paso o causa" }
        alerta.addTextField { $0.placeholder = "Lo que hice" }
        alerta.addTextField { $0.placeholder = "Corrección" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar error", style: .default) {
            [weak self] _ in
            guard let descripcion = alerta.textFields?[0].text,
                  !descripcion.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            else { return }
            let error = PasoErrorProblemaIPad(
                descripcion: descripcion,
                respuestaDada: alerta.textFields?[1].text ?? "",
                correccion: alerta.textFields?[2].text ?? "")
            CentroEstudioTecnicoIPad.compartido.registrarError(
                error, problemaId: problema.id)
            self?.refrescar()
        })
        present(alerta, animated: true)
    }

    @objc private func agregar() {
        let alerta = UIAlertController(title: "Nuevo problema", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Enunciado" }
        alerta.addTextField { $0.placeholder = "Tema" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self, let texto = alerta.textFields?[0].text,
                  !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                return
            }
            _ = CentroEstudioTecnicoIPad.compartido.agregarProblema(
                texto, cursoId: self.cursoId, tema: alerta.textFields?[1].text ?? "",
                dificultad: .practica)
            self.refrescar()
        })
        present(alerta, animated: true)
    }
}

private final class PanelPracticaProblemasIPad: UITableViewController {
    var alInsertarEnunciado: ((String) -> Void)?
    private let problemas: [ProblemaBancoIPad]
    private let documentoId: UInt64

    init(problemas: [ProblemaBancoIPad], documentoId: UInt64) {
        self.problemas = problemas; self.documentoId = documentoId
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() { super.viewDidLoad(); title = "Práctica aleatoria" }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { problemas.count }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let problema = problemas[indexPath.row]
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = "\(indexPath.row + 1). \(problema.tema)"
        celda.detailTextLabel?.text = String(problema.enunciado.prefix(160))
        celda.accessoryType = .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        let panel = PanelIntentoProblemaIPad(problema: problemas[indexPath.row],
                                              documentoId: documentoId)
        navigationController?.pushViewController(panel, animated: true)
    }
}

private final class PanelIntentoProblemaIPad: UIViewController {
    private let problema: ProblemaBancoIPad
    private let documentoId: UInt64
    private let inicio = Date()
    private var intento: IntentoProblemaIPad?
    private let cronometro = UILabel()
    private let enunciado = UITextView()
    private let desarrollo = UITextView()
    private let confianza = UISlider()
    private let solucion = UITextView()
    private var temporizador: Timer?

    init(problema: ProblemaBancoIPad, documentoId: UInt64) {
        self.problema = problema; self.documentoId = documentoId
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground
        title = "Intento limpio"
        intento = CentroEstudioTecnicoIPad.compartido.iniciarIntento(
            problema.id, documentoId: documentoId)
        cronometro.font = .monospacedDigitSystemFont(ofSize: 18, weight: .semibold)
        cronometro.textAlignment = .center
        enunciado.text = problema.enunciado; enunciado.isEditable = false
        enunciado.backgroundColor = .secondarySystemBackground
        enunciado.layer.cornerRadius = 10
        desarrollo.font = .preferredFont(forTextStyle: .body)
        desarrollo.layer.borderColor = UIColor.separator.cgColor
        desarrollo.layer.borderWidth = 1; desarrollo.layer.cornerRadius = 10
        desarrollo.text = "Desarrolle aquí o en una hoja limpia del lienzo…"
        confianza.minimumValue = 0; confianza.maximumValue = 1; confianza.value = 0.5
        solucion.isHidden = true; solucion.isEditable = false
        solucion.backgroundColor = .tertiarySystemBackground
        solucion.layer.cornerRadius = 10

        let mostrar = boton("Mostrar solución", #selector(mostrarSolucion))
        mostrar.isEnabled = problema.solucion != nil
        let correcto = boton("Correcto", #selector(marcarCorrecto))
        correcto.configuration = .filled(); correcto.configuration?.baseBackgroundColor = .systemGreen
        let incorrecto = boton("Anotar error", #selector(marcarIncorrecto))
        incorrecto.configuration = .filled(); incorrecto.configuration?.baseBackgroundColor = .systemRed
        let fila = UIStackView(arrangedSubviews: [correcto, incorrecto])
        fila.axis = .horizontal; fila.spacing = 10; fila.distribution = .fillEqually
        let etiquetaConfianza = UILabel(); etiquetaConfianza.text = "Confianza"
        let pila = UIStackView(arrangedSubviews: [cronometro, enunciado, desarrollo,
                                                   etiquetaConfianza, confianza,
                                                   mostrar, solucion, fila])
        pila.axis = .vertical; pila.spacing = 10; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                           constant: 18),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                            constant: -18),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
            enunciado.heightAnchor.constraint(equalToConstant: 150),
            desarrollo.heightAnchor.constraint(equalToConstant: 220),
            solucion.heightAnchor.constraint(equalToConstant: 130),
        ])
        temporizador = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) {
            [weak self] _ in self?.actualizarTiempo()
        }
        actualizarTiempo()
    }

    deinit { temporizador?.invalidate() }

    private func boton(_ titulo: String, _ accion: Selector) -> UIButton {
        let boton = UIButton(type: .system); boton.configuration = .bordered()
        boton.configuration?.title = titulo
        boton.addTarget(self, action: accion, for: .touchUpInside); return boton
    }
    private func actualizarTiempo() {
        let segundos = Int(Date().timeIntervalSince(inicio))
        cronometro.text = String(format: "%02d:%02d", segundos / 60, segundos % 60)
    }
    @objc private func mostrarSolucion() {
        guard let vinculada = problema.solucion else { return }
        solucion.text = vinculada.texto.isEmpty
            ? "Solución adjunta: \(vinculada.hashAdjunto ?? "sin texto")"
            : vinculada.texto
        solucion.isHidden = false
    }
    @objc private func marcarCorrecto() { cerrar(correcto: true) }
    @objc private func marcarIncorrecto() {
        let alerta = UIAlertController(title: "Error exacto", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Paso o causa" }
        alerta.addTextField { $0.placeholder = "Corrección" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar y repetir", style: .default) {
            [weak self] _ in
            guard let self else { return }
            let error = PasoErrorProblemaIPad(
                descripcion: alerta.textFields?[0].text ?? "Error sin clasificar",
                respuestaDada: self.desarrollo.text,
                correccion: alerta.textFields?[1].text ?? "")
            CentroEstudioTecnicoIPad.compartido.registrarError(
                error, problemaId: self.problema.id)
            self.cerrar(correcto: false)
        })
        present(alerta, animated: true)
    }
    private func cerrar(correcto: Bool) {
        temporizador?.invalidate()
        if let intento {
            CentroEstudioTecnicoIPad.compartido.cerrarIntento(
                problema.id, intentoId: intento.id, correcto: correcto,
                confianza: Double(confianza.value),
                segundos: Date().timeIntervalSince(inicio), nota: desarrollo.text)
        }
        navigationController?.popViewController(animated: true)
    }
}

private final class PanelComparadorSolucionIPad: UIViewController {
    private let problema: ProblemaBancoIPad
    init(problema: ProblemaBancoIPad) {
        self.problema = problema; super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground
        title = "Mi solución | Fuente"
        let mia = UITextView(); mia.font = .preferredFont(forTextStyle: .body)
        mia.text = problema.intentos.last?.nota ?? "Escriba o pegue su solución…"
        let fuente = UITextView(); fuente.font = .preferredFont(forTextStyle: .body)
        fuente.isEditable = false; fuente.backgroundColor = .secondarySystemBackground
        fuente.text = problema.solucion?.texto ?? "La solución vinculada no contiene texto."
        let izquierda = contenedor("Mi solución", mia)
        let derecha = contenedor("Solución vinculada", fuente)
        let pila = UIStackView(arrangedSubviews: [izquierda, derecha])
        pila.axis = .horizontal; pila.spacing = 12; pila.distribution = .fillEqually
        pila.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                           constant: 12),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                            constant: -12),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12),
            pila.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                          constant: -12),
        ])
    }
    private func contenedor(_ titulo: String, _ texto: UITextView) -> UIView {
        let etiqueta = UILabel(); etiqueta.text = titulo; etiqueta.font = .preferredFont(
            forTextStyle: .headline)
        let pila = UIStackView(arrangedSubviews: [etiqueta, texto])
        pila.axis = .vertical; pila.spacing = 8; return pila
    }
}

// MARK: - Fórmulas, dependencias y paquetes de curso

private final class PanelFormulaSheetIPad: UITableViewController {
    var alInsertar: ((String) -> Void)?
    private let cursoId: UUID?
    private let fuenteInicial: ReferenciaFuente?
    private let expresionInicial: String
    private var formulas: [FormulaTecnicaIPad] = []

    init(cursoId: UUID?, fuenteInicial: ReferenciaFuente?, expresionInicial: String) {
        self.cursoId = cursoId; self.fuenteInicial = fuenteInicial
        self.expresionInicial = expresionInicial
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Formula Sheet"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(agregar))
        refrescar()
    }
    private func refrescar() {
        formulas = CentroEstudioTecnicoIPad.compartido.instantanea().formulas
            .filter { cursoId == nil || $0.cursoId == cursoId }
            .sorted {
                if $0.favorita != $1.favorita { return $0.favorita }
                if $0.frecuenciaUso != $1.frecuenciaUso {
                    return $0.frecuenciaUso > $1.frecuenciaUso
                }
                return $0.fecha > $1.fecha
            }
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(formulas.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !formulas.isEmpty else {
            celda.textLabel?.text = "Marque su primera fórmula"
            celda.detailTextLabel?.text = "La escritura puede seguir siendo el objeto principal."
            celda.selectionStyle = .none; return celda
        }
        let formula = formulas[indexPath.row]
        celda.textLabel?.text = "\(formula.favorita ? "★ " : "")\(formula.expresion)"
        celda.detailTextLabel?.text = [formula.nombre, formula.parcial, formula.tema,
                                       "usada \(formula.frecuenciaUso)×"]
            .filter { !$0.isEmpty }.joined(separator: " · ")
        celda.imageView?.image = UIImage(systemName: formula.manuscritaPrincipal
                                         ? "pencil.and.outline" : "function")
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard formulas.indices.contains(indexPath.row) else { return }
        let formula = formulas[indexPath.row]
        _ = CentroEstudioTecnicoIPad.compartido.agregarFormula(
            formula.expresion, nombre: formula.nombre, cursoId: formula.cursoId,
            tema: formula.tema, parcial: formula.parcial, fuente: formula.fuente)
        alInsertar?(formula.expresion); refrescar()
    }
    override func tableView(_ tableView: UITableView,
                            trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        guard formulas.indices.contains(indexPath.row) else { return nil }
        let formula = formulas[indexPath.row]
        let favorita = UIContextualAction(style: .normal, title: formula.favorita
                                          ? "Quitar ★" : "Favorita") {
            [weak self] _, _, fin in
            CentroEstudioTecnicoIPad.compartido.alternarFormulaFavorita(formula.id)
            self?.refrescar(); fin(true)
        }
        favorita.backgroundColor = .systemYellow
        return UISwipeActionsConfiguration(actions: [favorita])
    }
    @objc private func agregar() {
        let alerta = UIAlertController(title: "Marcar fórmula", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { campo in
            campo.placeholder = "Expresión"; campo.text = self.expresionInicial
        }
        alerta.addTextField { $0.placeholder = "Nombre" }
        alerta.addTextField { $0.placeholder = "Tema" }
        alerta.addTextField { $0.placeholder = "Parcial o evaluación" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self, let expresion = alerta.textFields?[0].text?
                .trimmingCharacters(in: .whitespacesAndNewlines), !expresion.isEmpty
            else { return }
            _ = CentroEstudioTecnicoIPad.compartido.agregarFormula(
                expresion, nombre: alerta.textFields?[1].text ?? "",
                cursoId: self.cursoId, tema: alerta.textFields?[2].text ?? "",
                parcial: alerta.textFields?[3].text ?? "", fuente: self.fuenteInicial)
            self.refrescar()
        })
        present(alerta, animated: true)
    }
}

private final class PanelDependenciasConceptosIPad: UITableViewController {
    private let cursoId: UUID?
    private var conceptos: [ConceptoDominio] = []
    init(cursoId: UUID?) { self.cursoId = cursoId; super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() { super.viewDidLoad(); title = "Dependency Graph"; refrescar() }
    private func refrescar() {
        conceptos = CentroAcademico.compartido.instantanea().conceptos.filter {
            cursoId == nil || $0.cursoId == cursoId
        }.sorted { $0.dominio < $1.dominio }
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(conceptos.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !conceptos.isEmpty else {
            celda.textLabel?.text = "Sin conceptos indexados"
            celda.detailTextLabel?.text = "Cree conceptos en el Grafo de conocimiento."
            celda.selectionStyle = .none; return celda
        }
        let concepto = conceptos[indexPath.row]
        celda.textLabel?.text = concepto.nombre
        celda.detailTextLabel?.text = "Dominio \(Int(concepto.dominio * 100))% · \(concepto.prerrequisitos.count) prerrequisito(s)"
        celda.imageView?.image = UIImage(systemName: concepto.dominio < 0.55
                                         ? "exclamationmark.circle" : "checkmark.circle")
        celda.accessoryType = .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard conceptos.indices.contains(indexPath.row) else { return }
        let concepto = conceptos[indexPath.row]
        let ruta = CentroEstudioTecnicoIPad.compartido.rutaPrerequisitos(hasta: concepto.id)
        let mensaje = ruta.enumerated().map {
            "\($0.offset + 1). \($0.element.nombre) · dominio \(Int($0.element.dominio * 100))%"
        }.joined(separator: "\n")
        let alerta = UIAlertController(title: "Ruta para \(concepto.nombre)",
                                       message: mensaje, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .default))
        present(alerta, animated: true)
    }
}

private final class PanelCoursePackIPad: UITableViewController {
    private var cursoId: UUID?
    private var paquetes: [PaqueteCursoTecnicoIPad] = []
    init(cursoId: UUID?) { self.cursoId = cursoId; super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Course Pack"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(construir))
        refrescar()
    }
    private func refrescar() {
        paquetes = CentroEstudioTecnicoIPad.compartido.instantanea().paquetes.filter {
            cursoId == nil || $0.cursoId == cursoId
        }.sorted { $0.creado > $1.creado }
        tableView.reloadData()
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 2 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        if section == 0 { return 1 }
        return max(paquetes.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        section == 0 ? "Curso" : "Paquetes"
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.section == 0 {
            let curso = CentroAcademico.compartido.instantanea().cursos.first {
                $0.id == cursoId
            }
            celda.textLabel?.text = curso?.nombre ?? "Elegir curso"
            celda.detailTextLabel?.text = "El paquete nunca duplica los archivos; guarda referencias."
            celda.accessoryType = .disclosureIndicator
        } else if paquetes.isEmpty {
            celda.textLabel?.text = "Sin paquetes"
            celda.detailTextLabel?.text = "Pulse + para preparar un parcial o unidad."
            celda.selectionStyle = .none
        } else {
            let paquete = paquetes[indexPath.row]
            celda.textLabel?.text = paquete.nombre
            celda.detailTextLabel?.text = "\(paquete.problemaIds.count) problemas · \(paquete.formulaIds.count) fórmulas · \(paquete.errorIds.count) errores"
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 0 { elegirCurso() }
    }
    private func elegirCurso() {
        let alerta = UIAlertController(title: "Curso", message: nil,
                                       preferredStyle: .actionSheet)
        for curso in CentroAcademico.compartido.instantanea().cursos where !curso.archivado {
            alerta.addAction(UIAlertAction(title: curso.nombre, style: .default) {
                [weak self] _ in self?.cursoId = curso.id; self?.refrescar()
            })
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view
        present(alerta, animated: true)
    }
    @objc private func construir() {
        guard let cursoId else { elegirCurso(); return }
        let alerta = UIAlertController(title: "Nuevo Course Pack", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Ej.: Parcial I" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Construir", style: .default) {
            [weak self] _ in
            let nombre = alerta.textFields?.first?.text?.trimmingCharacters(
                in: .whitespacesAndNewlines)
            let titulo = nombre.flatMap { $0.isEmpty ? nil : $0 } ?? "Unidad"
            _ = CentroEstudioTecnicoIPad.compartido.construirPaquete(
                cursoId: cursoId, nombre: titulo)
            self?.refrescar()
        })
        present(alerta, animated: true)
    }
}

// MARK: - Office Hours, laboratorio y layouts reutilizables

private final class PanelDudasProfesorIPad: UITableViewController {
    private let cursoId: UUID?
    private let fuenteInicial: ReferenciaFuente?
    private let contextoInicial: String
    private var dudas: [DudaProfesorIPad] = []

    init(cursoId: UUID?, fuenteInicial: ReferenciaFuente?, contextoInicial: String) {
        self.cursoId = cursoId; self.fuenteInicial = fuenteInicial
        self.contextoInicial = contextoInicial
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Dudas para el profesor"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(agregar))
        refrescar()
    }
    private func refrescar() {
        dudas = CentroEstudioTecnicoIPad.compartido.instantanea().dudas.filter {
            cursoId == nil || $0.cursoId == cursoId
        }.sorted {
            if $0.estado != $1.estado { return $0.estado == .pendiente }
            return $0.fecha > $1.fecha
        }
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { max(dudas.count, 1) }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !dudas.isEmpty else {
            celda.textLabel?.text = "Sin dudas pendientes"
            celda.detailTextLabel?.text = "Guarde una duda sin detener la clase."
            celda.selectionStyle = .none; return celda
        }
        let duda = dudas[indexPath.row]
        celda.textLabel?.text = duda.pregunta
        celda.detailTextLabel?.text = duda.estado.rawValue
            + (duda.contexto.isEmpty ? "" : " · con contexto")
        celda.imageView?.image = UIImage(systemName: duda.estado == .pendiente
                                         ? "questionmark.circle" : "checkmark.circle")
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard dudas.indices.contains(indexPath.row) else { return }
        let duda = dudas[indexPath.row]
        let alerta = UIAlertController(title: duda.pregunta,
                                       message: duda.contexto.isEmpty ? nil : duda.contexto,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Respuesta del profesor"; $0.text = duda.respuesta }
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar respuesta", style: .default) {
            [weak self] _ in
            CentroEstudioTecnicoIPad.compartido.responderDuda(
                duda.id, respuesta: alerta.textFields?.first?.text ?? "")
            self?.refrescar()
        })
        present(alerta, animated: true)
    }
    @objc private func agregar() {
        let alerta = UIAlertController(title: "Nueva duda", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Pregunta para el profesor" }
        alerta.addTextField { campo in
            campo.placeholder = "Contexto"; campo.text = self.contextoInicial
        }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self, let pregunta = alerta.textFields?[0].text?
                .trimmingCharacters(in: .whitespacesAndNewlines), !pregunta.isEmpty
            else { return }
            _ = CentroEstudioTecnicoIPad.compartido.agregarDuda(
                pregunta, contexto: alerta.textFields?[1].text ?? "",
                cursoId: self.cursoId, fuente: self.fuenteInicial)
            self.refrescar()
        })
        present(alerta, animated: true)
    }
}

private final class PanelLaboratoriosIPad: UITableViewController {
    private let cursoId: UUID?
    private var laboratorios: [SesionLaboratorioIPad] = []
    init(cursoId: UUID?) { self.cursoId = cursoId; super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Cuaderno de laboratorio"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(agregar))
        refrescar()
    }
    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); refrescar() }
    private func refrescar() {
        laboratorios = CentroEstudioTecnicoIPad.compartido.instantanea().laboratorios
            .filter { cursoId == nil || $0.cursoId == cursoId }
            .sorted { $0.inicio > $1.inicio }
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(laboratorios.count, 1)
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !laboratorios.isEmpty else {
            celda.textLabel?.text = "Sin sesiones de laboratorio"
            celda.detailTextLabel?.text = "Pulse + para registrar una."
            celda.selectionStyle = .none; return celda
        }
        let lab = laboratorios[indexPath.row]
        celda.textLabel?.text = lab.titulo
        celda.detailTextLabel?.text = "\(lab.mediciones.count) mediciones · \(lab.hallazgos.count) hallazgos"
        celda.imageView?.image = UIImage(systemName: "testtube.2")
        celda.accessoryType = .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard laboratorios.indices.contains(indexPath.row) else { return }
        navigationController?.pushViewController(
            EditorLaboratorioIPad(laboratorio: laboratorios[indexPath.row]), animated: true)
    }
    @objc private func agregar() {
        let alerta = UIAlertController(title: "Nueva sesión de laboratorio", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Título" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) {
            [weak self] _ in
            guard let self, let titulo = alerta.textFields?.first?.text?
                .trimmingCharacters(in: .whitespacesAndNewlines), !titulo.isEmpty else {
                return
            }
            let lab = CentroEstudioTecnicoIPad.compartido.iniciarLaboratorio(
                titulo, cursoId: self.cursoId)
            self.navigationController?.pushViewController(
                EditorLaboratorioIPad(laboratorio: lab), animated: true)
        })
        present(alerta, animated: true)
    }
}

private final class EditorLaboratorioIPad: UITableViewController {
    private var laboratorio: SesionLaboratorioIPad
    init(laboratorio: SesionLaboratorioIPad) {
        self.laboratorio = laboratorio; super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = laboratorio.titulo
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Medición", style: .done, target: self,
                            action: #selector(agregarMedicion)),
            UIBarButtonItem(title: "Paso", style: .plain, target: self,
                            action: #selector(agregarPaso))]
    }
    override func numberOfSections(in tableView: UITableView) -> Int { 5 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 2
        case 1: return max(laboratorio.equipo.count, 1)
        case 2: return max(laboratorio.procedimiento.count, 1)
        case 3: return max(laboratorio.mediciones.count, 1)
        default: return 3
        }
    }
    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Diseño", "Equipo", "Procedimiento", "Mediciones", "Cierre"][section]
    }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0:
            celda.textLabel?.text = indexPath.row == 0 ? "Objetivo" : "Hipótesis"
            celda.detailTextLabel?.text = indexPath.row == 0
                ? laboratorio.objetivo : laboratorio.hipotesis
            celda.accessoryType = .disclosureIndicator
        case 1:
            celda.textLabel?.text = laboratorio.equipo.isEmpty
                ? "Agregar equipo" : laboratorio.equipo[indexPath.row]
        case 2:
            celda.textLabel?.text = laboratorio.procedimiento.isEmpty
                ? "Agregar paso" : "\(indexPath.row + 1). \(laboratorio.procedimiento[indexPath.row])"
        case 3:
            if laboratorio.mediciones.isEmpty { celda.textLabel?.text = "Agregar medición" }
            else {
                let medicion = laboratorio.mediciones[indexPath.row]
                celda.textLabel?.text = "\(medicion.magnitud): \(medicion.valor) \(medicion.unidad)"
                celda.detailTextLabel?.text = medicion.incertidumbre.isEmpty
                    ? nil : "± \(medicion.incertidumbre)"
            }
        default:
            let titulos = ["Riesgos", "Hallazgos", "Finalizar sesión"]
            celda.textLabel?.text = titulos[indexPath.row]
            celda.detailTextLabel?.text = indexPath.row == 0
                ? laboratorio.riesgos.joined(separator: " · ")
                : (indexPath.row == 1 ? laboratorio.hallazgos.joined(separator: " · ")
                   : laboratorio.fin?.formatted(date: .abbreviated, time: .shortened))
            celda.accessoryType = .disclosureIndicator
        }
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        switch indexPath.section {
        case 0: editarCampo(indexPath.row == 0 ? "Objetivo" : "Hipótesis",
                            actual: indexPath.row == 0
                            ? laboratorio.objetivo : laboratorio.hipotesis) { [weak self] valor in
            if indexPath.row == 0 { self?.laboratorio.objetivo = valor }
            else { self?.laboratorio.hipotesis = valor }
        }
        case 1: agregarLista("Equipo") { [weak self] in self?.laboratorio.equipo.append($0) }
        case 2: agregarPaso()
        case 3: agregarMedicion()
        case 4 where indexPath.row == 0:
            agregarLista("Riesgo o precaución") { [weak self] in self?.laboratorio.riesgos.append($0) }
        case 4 where indexPath.row == 1:
            agregarLista("Hallazgo") { [weak self] in self?.laboratorio.hallazgos.append($0) }
        case 4:
            laboratorio.fin = Date(); guardar()
        default: break
        }
    }
    private func editarCampo(_ titulo: String, actual: String,
                             aplicar: @escaping (String) -> Void) {
        let alerta = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.text = actual }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in aplicar(alerta.textFields?.first?.text ?? ""); self?.guardar()
        })
        present(alerta, animated: true)
    }
    private func agregarLista(_ titulo: String, aplicar: @escaping (String) -> Void) {
        editarCampo(titulo, actual: "") { valor in
            if !valor.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty { aplicar(valor) }
        }
    }
    @objc private func agregarPaso() {
        agregarLista("Paso del procedimiento") { [weak self] in
            self?.laboratorio.procedimiento.append($0)
        }
    }
    @objc private func agregarMedicion() {
        let alerta = UIAlertController(title: "Medición", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Magnitud" }
        alerta.addTextField { $0.placeholder = "Valor" }
        alerta.addTextField { $0.placeholder = "Unidad" }
        alerta.addTextField { $0.placeholder = "Incertidumbre" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let self, let magnitud = alerta.textFields?[0].text,
                  !magnitud.isEmpty else { return }
            self.laboratorio.mediciones.append(MedicionLaboratorioIPad(
                magnitud: magnitud, valor: alerta.textFields?[1].text ?? "",
                unidad: alerta.textFields?[2].text ?? "",
                incertidumbre: alerta.textFields?[3].text ?? ""))
            self.guardar()
        })
        present(alerta, animated: true)
    }
    private func guardar() {
        CentroEstudioTecnicoIPad.compartido.actualizarLaboratorio(laboratorio)
        tableView.reloadData()
    }
}

private final class PanelLayoutsProblemaIPad: UITableViewController {
    private let alInsertar: ((String) -> Void)?
    private var plantillas: [PlantillaProblemaIPad] = []
    init(alInsertar: ((String) -> Void)?) {
        self.alInsertar = alInsertar; super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Layouts reutilizables"
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(agregar))
        refrescar()
    }
    private func refrescar() {
        plantillas = CentroEstudioTecnicoIPad.compartido.instantanea().plantillas
        tableView.reloadData()
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { max(plantillas.count, 1) }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !plantillas.isEmpty else {
            celda.textLabel?.text = "Sin layouts guardados"
            celda.detailTextLabel?.text = "Guarde la estructura de un problema, no su respuesta."
            celda.selectionStyle = .none; return celda
        }
        let plantilla = plantillas[indexPath.row]
        celda.textLabel?.text = plantilla.nombre
        celda.detailTextLabel?.text = "\(Int(plantilla.ancho)) × \(Int(plantilla.alto)) · \(plantilla.columnas) columna(s)"
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard plantillas.indices.contains(indexPath.row) else { return }
        let plantilla = plantillas[indexPath.row]
        let texto = plantilla.zonas.map { "## \($0)\n" }.joined(separator: "\n")
        alInsertar?(texto)
    }
    @objc private func agregar() {
        let alerta = UIAlertController(title: "Guardar layout", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Nombre" }
        alerta.addTextField { $0.placeholder = "Ancho"; $0.text = "800"; $0.keyboardType = .numberPad }
        alerta.addTextField { $0.placeholder = "Alto"; $0.text = "1100"; $0.keyboardType = .numberPad }
        alerta.addTextField { $0.placeholder = "Zonas separadas por coma"; $0.text = "Enunciado,Desarrollo,Resultado,Error" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) {
            [weak self] _ in
            guard let nombre = alerta.textFields?[0].text,
                  !nombre.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                return
            }
            let plantilla = PlantillaProblemaIPad(
                nombre: nombre, ancho: Double(alerta.textFields?[1].text ?? "") ?? 800,
                alto: Double(alerta.textFields?[2].text ?? "") ?? 1100,
                zonas: (alerta.textFields?[3].text ?? "").split(separator: ",")
                    .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) })
            _ = CentroEstudioTecnicoIPad.compartido.guardarPlantilla(plantilla)
            self?.refrescar()
        })
        present(alerta, animated: true)
    }
}
