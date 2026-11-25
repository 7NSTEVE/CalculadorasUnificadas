import CryptoKit
import Foundation
import PDFKit
import UIKit
import UniformTypeIdentifiers
import VisionKit

// MARK: - Modelos de práctica, entrega y evaluación

enum NivelConfianzaEjercicio: String, Codable, CaseIterable, Identifiable {
    case adivine
    case medio
    case seguro

    var id: String { rawValue }
    var nombre: String {
        switch self {
        case .adivine: return "Adiviné"
        case .medio: return "Medio"
        case .seguro: return "Seguro"
        }
    }
    var valor: Double {
        switch self { case .adivine: return 0.2; case .medio: return 0.58; case .seguro: return 0.92 }
    }
}

struct IntentoEjercicioIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var conceptoId: UUID?
    var enunciado: String
    var respuesta: String
    var respuestaEsperada: String
    var correcto: Bool
    var requiereRevision = false
    var confianza: NivelConfianzaEjercicio
    var segundos: Double
    var fecha = Date()
    var referencia: ReferenciaFuente?
    var pistasReveladas = 0
}

struct TransicionAprendizajeIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var conceptoId: UUID?
    var deEstado: String
    var aEstado: String
    var explicacionQueFunciono: String
    var fecha = Date()
    var referencia: ReferenciaFuente?
}

struct CriterioRubricaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var titulo: String
    var descripcion = ""
    var puntosMaximos: Double
    var puntosObtenidos: Double?
    var comentarioProfesor = ""
    var errorId: UUID?
}

struct RubricaAcademicaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var titulo: String
    var tareaId: UUID?
    var fecha = Date()
    var criterios: [CriterioRubricaIPad] = []
    var comentarioGeneral = ""
}

struct PreguntaExamenIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var conceptoId: UUID?
    var enunciado: String
    var respuestaEsperada: String
    var referencia: ReferenciaFuente?
    var categoria: String
    var puntos: Double = 1
}

struct RespuestaExamenIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var preguntaId: UUID
    var texto: String
    var confianza: NivelConfianzaEjercicio
    var segundos: Double
    var correctaAutomatica: Bool?
    var requiereRevision = false
}

struct ExamenSimuladoIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var titulo: String
    var creado = Date()
    var inicio: Date?
    var fin: Date?
    var duracionMinutos: Int
    var preguntas: [PreguntaExamenIPad]
    var respuestas: [RespuestaExamenIPad] = []
    var asistenteTintaDesactivado = true
    var finalizado = false
    var calificacionAutomatica: Double?
    var requiereRevisionManual = false
}

enum EstadoEntregaIPad: String, Codable {
    case borrador
    case preparada
    case compartida
    case entregadaConfirmada
    case archivada
}

struct ConfiguracionPortadaEntregaIPad: Codable, Equatable {
    var incluir = true
    var estudiante = ""
    var identificacion = ""
    var curso = ""
    var profesor = ""
    var titulo = "Tarea"
    var fecha = Date()
}

struct EntregaAcademicaIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var cursoId: UUID?
    var tareaId: UUID?
    var titulo: String
    var fecha = Date()
    var portada = ConfiguracionPortadaEntregaIPad()
    var nombreArchivo: String?
    var paginas = 0
    var estado: EstadoEntregaIPad = .borrador
    var hashPDF: String?
    var referenciaDocumento: UInt64?
}

struct BloqueoPracticaIPad: Identifiable {
    var id: String { categoria }
    var categoria: String
    var segundosPromedio: Double
    var factorSobrePromedio: Double
    var intentos: Int
}

private struct EstadoEvaluacionIPad: Codable {
    var version = 1
    var intentos: [IntentoEjercicioIPad] = []
    var transiciones: [TransicionAprendizajeIPad] = []
    var rubricas: [RubricaAcademicaIPad] = []
    var examenes: [ExamenSimuladoIPad] = []
    var entregas: [EntregaAcademicaIPad] = []

    enum CodingKeys: String, CodingKey {
        case version, intentos, transiciones, rubricas, examenes, entregas
    }

    init() {}
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        version = try c.decodeIfPresent(Int.self, forKey: .version) ?? 1
        intentos = try c.decodeIfPresent([IntentoEjercicioIPad].self,
                                         forKey: .intentos) ?? []
        transiciones = try c.decodeIfPresent([TransicionAprendizajeIPad].self,
                                             forKey: .transiciones) ?? []
        rubricas = try c.decodeIfPresent([RubricaAcademicaIPad].self,
                                         forKey: .rubricas) ?? []
        examenes = try c.decodeIfPresent([ExamenSimuladoIPad].self,
                                         forKey: .examenes) ?? []
        entregas = try c.decodeIfPresent([EntregaAcademicaIPad].self,
                                         forKey: .entregas) ?? []
    }
}

struct InstantaneaEvaluacionIPad {
    var intentos: [IntentoEjercicioIPad]
    var transiciones: [TransicionAprendizajeIPad]
    var rubricas: [RubricaAcademicaIPad]
    var examenes: [ExamenSimuladoIPad]
    var entregas: [EntregaAcademicaIPad]
}

final class CentroEvaluacionIPad {
    static let compartido = CentroEvaluacionIPad()

    private let bloqueo = NSRecursiveLock()
    private let url: URL
    private var estado: EstadoEvaluacionIPad

    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Academico", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("evaluacion-ipad.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(EstadoEvaluacionIPad.self, from: datos) {
            estado = leido
        } else {
            estado = EstadoEvaluacionIPad()
        }
    }

    func instantanea() -> InstantaneaEvaluacionIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return InstantaneaEvaluacionIPad(
            intentos: estado.intentos, transiciones: estado.transiciones,
            rubricas: estado.rubricas, examenes: estado.examenes,
            entregas: estado.entregas)
    }

    @discardableResult
    private func cambiar<T>(_ cuerpo: (inout EstadoEvaluacionIPad) -> T) -> T {
        bloqueo.lock(); defer { bloqueo.unlock() }
        let resultado = cuerpo(&estado)
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        if let datos = try? encoder.encode(estado) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
        return resultado
    }

    func registrarIntento(_ intento: IntentoEjercicioIPad) {
        cambiar { $0.intentos.append(intento) }
        if !intento.correcto, !intento.requiereRevision {
            _ = CentroAcademico.compartido.registrarError(
                cursoId: intento.cursoId, conceptoId: intento.conceptoId,
                enunciado: intento.enunciado, respuestaDada: intento.respuesta,
                correcta: intento.respuestaEsperada,
                causa: intento.confianza == .seguro
                    ? "Respuesta incorrecta con confianza alta" : "Práctica",
                referencia: intento.referencia)
        }
        if let conceptoId = intento.conceptoId {
            CentroAcademico.compartido.actualizarDominio(
                conceptoId: conceptoId, correcto: intento.correcto,
                confianza: intento.confianza.valor)
        }
    }

    func registrarEntendido(cursoId: UUID?, conceptoId: UUID?,
                            explicacion: String, referencia: ReferenciaFuente?) {
        let transicion = TransicionAprendizajeIPad(
            cursoId: cursoId, conceptoId: conceptoId,
            deEstado: "duda", aEstado: "entendido",
            explicacionQueFunciono: explicacion, referencia: referencia)
        cambiar { $0.transiciones.append(transicion) }
        if let conceptoId {
            CentroAcademico.compartido.transaccion { base in
                if let indice = base.conceptos.firstIndex(where: { $0.id == conceptoId }) {
                    base.conceptos[indice].dominio = max(base.conceptos[indice].dominio, 0.72)
                    base.conceptos[indice].ultimoContacto = Date()
                }
                for indice in base.errores.indices where
                    base.errores[indice].conceptoId == conceptoId {
                    base.errores[indice].resuelto = true
                }
            }
        }
    }

    func guardarRubrica(_ rubrica: RubricaAcademicaIPad) {
        cambiar { estado in
            if let indice = estado.rubricas.firstIndex(where: { $0.id == rubrica.id }) {
                estado.rubricas[indice] = rubrica
            } else {
                estado.rubricas.append(rubrica)
            }
        }
    }

    func guardarExamen(_ examen: ExamenSimuladoIPad) {
        cambiar { estado in
            if let indice = estado.examenes.firstIndex(where: { $0.id == examen.id }) {
                estado.examenes[indice] = examen
            } else {
                estado.examenes.append(examen)
            }
        }
    }

    func guardarEntrega(_ entrega: EntregaAcademicaIPad) {
        cambiar { estado in
            if let indice = estado.entregas.firstIndex(where: { $0.id == entrega.id }) {
                estado.entregas[indice] = entrega
            } else {
                estado.entregas.append(entrega)
            }
        }
    }

    func bloqueos(cursoId: UUID?) -> [BloqueoPracticaIPad] {
        let intentos = instantanea().intentos.filter {
            cursoId == nil || $0.cursoId == cursoId
        }
        guard !intentos.isEmpty else { return [] }
        let promedioGlobal = intentos.map(\.segundos).reduce(0, +)
            / Double(intentos.count)
        let agrupados = Dictionary(grouping: intentos) { intento in
            intento.conceptoId?.uuidString
                ?? Self.categoria(intento.enunciado)
        }
        return agrupados.compactMap { entrada in
            let (categoria, grupo) = entrada
            let promedio = grupo.map(\.segundos).reduce(0, +) / Double(grupo.count)
            let factor = promedioGlobal > 0 ? promedio / promedioGlobal : 1
            guard grupo.count >= 2, factor >= 1.35 else { return nil }
            return BloqueoPracticaIPad(
                categoria: categoria, segundosPromedio: promedio,
                factorSobrePromedio: factor, intentos: grupo.count)
        }.sorted { $0.factorSobrePromedio > $1.factorSobrePromedio }
    }

    private static func categoria(_ texto: String) -> String {
        let palabras = texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                                     locale: .current).lowercased()
            .split(whereSeparator: \.isWhitespace)
            .filter { $0.count > 4 }
        return palabras.first.map { String($0) } ?? "general"
    }
}

// MARK: - Examen local con interleaving y revisión honesta

enum GeneradorExamenLocalIPad {
    static func generar(cursoId: UUID?, cantidad: Int, minutos: Int)
        -> ExamenSimuladoIPad {
        let base = CentroAcademico.compartido.instantanea()
        var grupos: [String: [PreguntaExamenIPad]] = [:]
        for tarjeta in base.flashcards where cursoId == nil || tarjeta.cursoId == cursoId {
            let categoria = tarjeta.conceptoId?.uuidString
                ?? tarjeta.pregunta.split(separator: " ").first.map(String.init)
                ?? "flashcards"
            grupos[categoria, default: []].append(PreguntaExamenIPad(
                cursoId: tarjeta.cursoId, conceptoId: tarjeta.conceptoId,
                enunciado: tarjeta.pregunta,
                respuestaEsperada: tarjeta.respuesta,
                referencia: tarjeta.referencia, categoria: categoria))
        }
        for error in base.errores where !error.resuelto
            && (cursoId == nil || error.cursoId == cursoId) {
            let categoria = error.conceptoId?.uuidString ?? "errores"
            grupos[categoria, default: []].append(PreguntaExamenIPad(
                cursoId: error.cursoId, conceptoId: error.conceptoId,
                enunciado: error.enunciado,
                respuestaEsperada: error.respuestaCorrecta,
                referencia: error.referencia, categoria: categoria))
        }

        var colas = grupos.keys.sorted().compactMap { clave -> (String, [PreguntaExamenIPad])? in
            guard let preguntas = grupos[clave], !preguntas.isEmpty else { return nil }
            return (clave, preguntas.sorted { $0.enunciado < $1.enunciado })
        }
        var seleccion: [PreguntaExamenIPad] = []
        while seleccion.count < max(1, cantidad), !colas.isEmpty {
            var siguientes: [(String, [PreguntaExamenIPad])] = []
            for entrada in colas where seleccion.count < cantidad {
                let clave = entrada.0
                var preguntas = entrada.1
                seleccion.append(preguntas.removeFirst())
                if !preguntas.isEmpty { siguientes.append((clave, preguntas)) }
            }
            colas = siguientes
        }
        let curso = base.cursos.first { $0.id == cursoId }?.nombre ?? "Estudio"
        return ExamenSimuladoIPad(
            cursoId: cursoId, titulo: "Examen simulado · \(curso)",
            duracionMinutos: max(1, minutos), preguntas: seleccion)
    }

    static func evaluar(respuesta: String, esperada: String) -> (correcta: Bool?, revision: Bool) {
        let dada = tokens(respuesta)
        let correcta = tokens(esperada)
        guard !dada.isEmpty, !correcta.isEmpty else { return (nil, true) }
        if dada == correcta { return (true, false) }
        let importantes = correcta.filter { $0.count >= 3 }
        guard importantes.count >= 2 else { return (nil, true) }
        let coincidencias = Set(dada).intersection(importantes).count
        let proporcion = Double(coincidencias) / Double(Set(importantes).count)
        if proporcion >= 0.78 { return (true, true) }
        if proporcion <= 0.25 { return (false, false) }
        return (nil, true)
    }

    private static func tokens(_ texto: String) -> [String] {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive],
                      locale: .current).lowercased()
            .components(separatedBy: CharacterSet.alphanumerics.inverted)
            .filter { !$0.isEmpty }
    }
}

final class ControladorExamenSimuladoIPad: UIViewController {
    private var examen: ExamenSimuladoIPad
    private var indice = 0
    private var inicioPregunta = Date()
    private var temporizador: Timer?

    private let pregunta = UILabel()
    private let respuesta = UITextView()
    private let confianza = UISegmentedControl(
        items: NivelConfianzaEjercicio.allCases.map(\.nombre))
    private let tiempo = UILabel()
    private let progreso = UIProgressView(progressViewStyle: .default)
    private let siguiente = UIButton(type: .system)

    init(examen: ExamenSimuladoIPad) {
        self.examen = examen
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = examen.titulo
        isModalInPresentation = true
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            title: "Terminar", style: .plain, target: self,
            action: #selector(confirmarFin))

        pregunta.numberOfLines = 0
        pregunta.font = .preferredFont(forTextStyle: .title2)
        respuesta.font = .preferredFont(forTextStyle: .body)
        respuesta.layer.borderWidth = 1
        respuesta.layer.borderColor = UIColor.separator.cgColor
        respuesta.layer.cornerRadius = 12
        confianza.selectedSegmentIndex = 1
        tiempo.font = .monospacedDigitSystemFont(ofSize: 16, weight: .semibold)
        tiempo.textAlignment = .right
        siguiente.configuration = .filled()
        siguiente.configuration?.title = "Guardar y continuar"
        siguiente.addTarget(self, action: #selector(guardarRespuesta),
                            for: .touchUpInside)

        let aviso = UILabel()
        aviso.text = "La ayuda de Tinta está oculta durante este simulacro. iPadOS no permite a una app bloquear otras apps del dispositivo."
        aviso.numberOfLines = 0
        aviso.textColor = .secondaryLabel
        aviso.font = .preferredFont(forTextStyle: .footnote)

        let pila = UIStackView(arrangedSubviews: [tiempo, progreso, pregunta,
                                                   respuesta, confianza,
                                                   siguiente, aviso])
        pila.axis = .vertical
        pila.spacing = 16
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                           constant: 24),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                            constant: -24),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                       constant: 24),
            respuesta.heightAnchor.constraint(greaterThanOrEqualToConstant: 220)
        ])

        examen.inicio = Date()
        CentroEvaluacionIPad.compartido.guardarExamen(examen)
        UIApplication.shared.isIdleTimerDisabled = true
        mostrarPregunta()
        temporizador = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) {
            [weak self] _ in self?.actualizarTiempo()
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        UIApplication.shared.isIdleTimerDisabled = false
        temporizador?.invalidate()
    }

    private func mostrarPregunta() {
        guard !examen.preguntas.isEmpty, indice < examen.preguntas.count else {
            finalizar(); return
        }
        let actual = examen.preguntas[indice]
        pregunta.text = "\(indice + 1). \(actual.enunciado)"
        respuesta.text = ""
        confianza.selectedSegmentIndex = 1
        progreso.progress = Float(indice) / Float(max(examen.preguntas.count, 1))
        inicioPregunta = Date()
        respuesta.becomeFirstResponder()
    }

    @objc private func guardarRespuesta() {
        guard indice < examen.preguntas.count else { return }
        let actual = examen.preguntas[indice]
        let texto = respuesta.text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !texto.isEmpty else {
            anunciar("Escriba una respuesta o termine el examen.")
            return
        }
        let nivel = NivelConfianzaEjercicio(
            rawValue: NivelConfianzaEjercicio.allCases[confianza.selectedSegmentIndex].rawValue)
            ?? .medio
        let evaluacion = GeneradorExamenLocalIPad.evaluar(
            respuesta: texto, esperada: actual.respuestaEsperada)
        let segundos = Date().timeIntervalSince(inicioPregunta)
        examen.respuestas.append(RespuestaExamenIPad(
            preguntaId: actual.id, texto: texto, confianza: nivel,
            segundos: segundos, correctaAutomatica: evaluacion.correcta,
            requiereRevision: evaluacion.revision))
        CentroEvaluacionIPad.compartido.registrarIntento(IntentoEjercicioIPad(
            cursoId: actual.cursoId, conceptoId: actual.conceptoId,
            enunciado: actual.enunciado, respuesta: texto,
            respuestaEsperada: actual.respuestaEsperada,
            correcto: evaluacion.correcta ?? false,
            requiereRevision: evaluacion.correcta == nil || evaluacion.revision,
            confianza: nivel, segundos: segundos,
            referencia: actual.referencia))
        indice += 1
        CentroEvaluacionIPad.compartido.guardarExamen(examen)
        mostrarPregunta()
    }

    private func actualizarTiempo() {
        guard let inicio = examen.inicio else { return }
        let duracion = TimeInterval(examen.duracionMinutos * 60)
        let restante = max(0, duracion - Date().timeIntervalSince(inicio))
        let s = Int(restante.rounded(.up))
        tiempo.text = String(format: "%02d:%02d", s / 60, s % 60)
        if restante <= 0 { finalizar() }
    }

    @objc private func confirmarFin() {
        let alerta = UIAlertController(
            title: "Terminar simulacro",
            message: "Las respuestas guardadas se califican localmente; las ambiguas quedan para revisión manual.",
            preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Continuar examen", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Terminar", style: .destructive) {
            [weak self] _ in self?.finalizar()
        })
        present(alerta, animated: true)
    }

    private func finalizar() {
        guard !examen.finalizado else { return }
        examen.fin = Date()
        examen.finalizado = true
        let automaticas = examen.respuestas.compactMap(\.correctaAutomatica)
        examen.requiereRevisionManual = examen.respuestas.contains {
            $0.requiereRevision || $0.correctaAutomatica == nil
        }
        if !automaticas.isEmpty {
            examen.calificacionAutomatica = Double(automaticas.filter { $0 }.count)
                / Double(automaticas.count) * 100
        }
        CentroEvaluacionIPad.compartido.guardarExamen(examen)
        temporizador?.invalidate()
        UIApplication.shared.isIdleTimerDisabled = false
        let nota = examen.calificacionAutomatica.map { String(format: "%.1f %%", $0) }
            ?? "sin calificación automática"
        let sufijo = examen.requiereRevisionManual
            ? " Hay respuestas que requieren revisión manual." : ""
        let alerta = UIAlertController(title: "Simulacro finalizado",
                                       message: "Resultado provisional: \(nota).\(sufijo)",
                                       preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .default) {
            [weak self] _ in self?.dismiss(animated: true)
        })
        present(alerta, animated: true)
    }

    private func anunciar(_ texto: String) {
        UIAccessibility.post(notification: .announcement, argument: texto)
    }
}

// MARK: - Preparación de entregas A4

enum ErrorEntregaIPad: LocalizedError {
    case sinContenido
    case noGeneraPDF

    var errorDescription: String? {
        switch self {
        case .sinContenido: return "La nota no tiene contenido exportable."
        case .noGeneraPDF: return "No se pudo generar el PDF de entrega."
        }
    }
}

enum PreparadorEntregaIPad {
    private static let a4 = CGSize(width: 595.28, height: 841.89)

    static func preparar(sesion: Sesion, navegador: VistaLienzoNavegable,
                         entrega: inout EntregaAcademicaIPad) throws -> URL {
        let region = regionContenido(sesion: sesion, navegador: navegador)
        guard region.width > 1, region.height > 1 else { throw ErrorEntregaIPad.sinContenido }
        let temporal = FileManager.default.temporaryDirectory
            .appendingPathComponent("contenido-\(UUID().uuidString).pdf")
        try ExportadorCompletoIPad.pdf(
            sesion: sesion, navegador: navegador, region: region, destino: temporal)
        guard let fuente = PDFDocument(url: temporal), fuente.pageCount > 0 else {
            throw ErrorEntregaIPad.noGeneraPDF
        }

        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Entregas", isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        let nombreSeguro = entrega.titulo.replacingOccurrences(of: "/", with: "-")
        let destino = carpeta.appendingPathComponent(
            "\(nombreSeguro)-\(UUID().uuidString)").appendingPathExtension("pdf")
        let formato = UIGraphicsPDFRendererFormat()
        formato.documentInfo = [
            kCGPDFContextTitle as String: entrega.titulo,
            kCGPDFContextCreator as String: "Tinta para iPad"
        ]
        let renderer = UIGraphicsPDFRenderer(bounds: CGRect(origin: .zero, size: a4),
                                             format: formato)
        try renderer.writePDF(to: destino) { contexto in
            if entrega.portada.incluir {
                contexto.beginPage()
                dibujarPortada(entrega.portada, en: contexto.cgContext)
            }
            for indice in 0..<fuente.pageCount {
                guard let pagina = fuente.page(at: indice) else { continue }
                contexto.beginPage()
                dibujar(pagina: pagina, en: contexto.cgContext,
                        numero: indice + 1,
                        total: fuente.pageCount)
            }
        }
        entrega.paginas = fuente.pageCount + (entrega.portada.incluir ? 1 : 0)
        entrega.nombreArchivo = destino.lastPathComponent
        entrega.estado = .preparada
        entrega.referenciaDocumento = sesion.documentoPersistenteId
        entrega.hashPDF = try hash(destino)
        CentroEvaluacionIPad.compartido.guardarEntrega(entrega)
        return destino
    }

    static func prepararEscaneo(_ imagenes: [UIImage],
                                entrega: inout EntregaAcademicaIPad) throws -> URL {
        guard !imagenes.isEmpty else { throw ErrorEntregaIPad.sinContenido }
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Entregas", isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        let destino = carpeta.appendingPathComponent(
            "Escaneo-\(UUID().uuidString)").appendingPathExtension("pdf")
        let renderer = UIGraphicsPDFRenderer(bounds: CGRect(origin: .zero, size: a4))
        try renderer.writePDF(to: destino) { contexto in
            if entrega.portada.incluir {
                contexto.beginPage(); dibujarPortada(entrega.portada, en: contexto.cgContext)
            }
            for (indice, imagen) in imagenes.enumerated() {
                contexto.beginPage()
                let caja = CGRect(x: 36, y: 36, width: a4.width - 72,
                                  height: a4.height - 72)
                let escala = min(caja.width / imagen.size.width,
                                 caja.height / imagen.size.height)
                let tamano = CGSize(width: imagen.size.width * escala,
                                    height: imagen.size.height * escala)
                imagen.draw(in: CGRect(x: caja.midX - tamano.width / 2,
                                       y: caja.midY - tamano.height / 2,
                                       width: tamano.width, height: tamano.height))
                dibujarNumero(indice + 1, total: imagenes.count,
                              en: contexto.cgContext)
            }
        }
        entrega.paginas = imagenes.count + (entrega.portada.incluir ? 1 : 0)
        entrega.nombreArchivo = destino.lastPathComponent
        entrega.estado = .preparada
        entrega.hashPDF = try hash(destino)
        CentroEvaluacionIPad.compartido.guardarEntrega(entrega)
        return destino
    }

    static func url(entrega: EntregaAcademicaIPad) -> URL? {
        guard let nombre = entrega.nombreArchivo else { return nil }
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let url = documentos.appendingPathComponent("Entregas", isDirectory: true)
            .appendingPathComponent(nombre)
        return FileManager.default.fileExists(atPath: url.path) ? url : nil
    }

    private static func regionContenido(sesion: Sesion,
                                        navegador: VistaLienzoNavegable) -> CGRect {
        var marcos = (0..<sesion.cantidadTrazos).compactMap(sesion.marcoTrazo)
        marcos.append(contentsOf: sesion.textos.map(\.marco))
        marcos.append(contentsOf: sesion.medios.map(\.marco))
        marcos.append(contentsOf: navegador.capaBloques.bloques.map(\.marco))
        for objeto in navegador.capaTecnica.objetos where !objeto.puntos.isEmpty {
            let xs = objeto.puntos.map(\.x), ys = objeto.puntos.map(\.y)
            if let minX = xs.min(), let maxX = xs.max(),
               let minY = ys.min(), let maxY = ys.max() {
                marcos.append(CGRect(x: minX, y: minY,
                                     width: max(maxX - minX, 1),
                                     height: max(maxY - minY, 1)))
            }
        }
        let union = marcos.reduce(nil as CGRect?) { $0?.union($1) ?? $1 }
            ?? navegador.regionVisibleDocumento
        let pagina = CGRect(x: 0, y: 0, width: CGFloat(sesion.anchoPagina),
                            height: CGFloat(sesion.altoPagina))
        return union.insetBy(dx: -28, dy: -28).intersection(pagina)
    }

    private static func dibujarPortada(_ portada: ConfiguracionPortadaEntregaIPad,
                                       en contexto: CGContext) {
        contexto.setFillColor(UIColor.white.cgColor)
        contexto.fill(CGRect(origin: .zero, size: a4))
        let titulo = portada.titulo as NSString
        titulo.draw(in: CGRect(x: 56, y: 180, width: a4.width - 112, height: 100),
                    withAttributes: [
                        .font: UIFont.systemFont(ofSize: 30, weight: .bold),
                        .foregroundColor: UIColor.black
                    ])
        let fecha = DateFormatter.localizedString(from: portada.fecha,
                                                   dateStyle: .long, timeStyle: .none)
        let detalles = [portada.estudiante, portada.identificacion,
                        portada.curso, portada.profesor, fecha]
            .filter { !$0.isEmpty }.joined(separator: "\n") as NSString
        detalles.draw(in: CGRect(x: 56, y: 330, width: a4.width - 112, height: 260),
                      withAttributes: [
                        .font: UIFont.systemFont(ofSize: 17),
                        .foregroundColor: UIColor.darkGray
                      ])
    }

    private static func dibujar(pagina: PDFPage, en contexto: CGContext,
                                numero: Int, total: Int) {
        contexto.setFillColor(UIColor.white.cgColor)
        contexto.fill(CGRect(origin: .zero, size: a4))
        let cajaFuente = pagina.bounds(for: .mediaBox)
        let destino = CGRect(x: 36, y: 36, width: a4.width - 72,
                             height: a4.height - 86)
        let escala = min(destino.width / cajaFuente.width,
                         destino.height / cajaFuente.height)
        contexto.saveGState()
        contexto.translateBy(x: destino.midX - cajaFuente.width * escala / 2,
                             y: destino.midY + cajaFuente.height * escala / 2)
        contexto.scaleBy(x: escala, y: -escala)
        pagina.draw(with: .mediaBox, to: contexto)
        contexto.restoreGState()
        dibujarNumero(numero, total: total, en: contexto)
    }

    private static func dibujarNumero(_ numero: Int, total: Int,
                                      en contexto: CGContext) {
        let texto = "\(numero) / \(total)" as NSString
        texto.draw(in: CGRect(x: 36, y: a4.height - 30,
                             width: a4.width - 72, height: 18),
                   withAttributes: [
                    .font: UIFont.systemFont(ofSize: 10),
                    .foregroundColor: UIColor.gray,
                    .paragraphStyle: {
                        let estilo = NSMutableParagraphStyle()
                        estilo.alignment = .center
                        return estilo
                    }()
                   ])
    }

    private static func hash(_ url: URL) throws -> String {
        let datos = try Data(contentsOf: url, options: .mappedIfSafe)
        return SHA256.hash(data: datos).map { String(format: "%02x", $0) }.joined()
    }
}

// MARK: - Interfaces de entrega, rúbrica y práctica

final class VistaPreviaEntregaIPad: UIViewController {
    private var entrega: EntregaAcademicaIPad
    private let url: URL
    private let pdf = PDFView()

    init(entrega: EntregaAcademicaIPad, url: URL) {
        self.entrega = entrega; self.url = url
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Vista previa de entrega"
        view.backgroundColor = .systemBackground
        pdf.autoScales = true
        pdf.displayMode = .singlePageContinuous
        pdf.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pdf)
        NSLayoutConstraint.activate([
            pdf.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            pdf.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            pdf.topAnchor.constraint(equalTo: view.topAnchor),
            pdf.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        pdf.document = PDFDocument(url: url)
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .action, target: self, action: #selector(compartir))
    }

    @objc private func compartir() {
        let hoja = UIActivityViewController(activityItems: [url], applicationActivities: nil)
        hoja.popoverPresentationController?.barButtonItem = navigationItem.rightBarButtonItem
        present(hoja, animated: true)
        entrega.estado = .compartida
        CentroEvaluacionIPad.compartido.guardarEntrega(entrega)
    }
}

final class ComparadorSolucionIPad: UIViewController {
    var alRegistrarDiferencia: ((String) -> Void)?

    private let propiaURL: URL
    private let solucionURL: URL
    private let propia = PDFView()
    private let solucion = PDFView()

    init(propia: URL, solucion: URL) {
        propiaURL = propia; solucionURL = solucion
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Mi solución ↔ solución oficial"
        view.backgroundColor = .systemBackground
        propia.document = PDFDocument(url: propiaURL)
        solucion.document = PDFDocument(url: solucionURL)
        for visor in [propia, solucion] {
            visor.autoScales = true
            visor.displayMode = .singlePageContinuous
        }
        let izquierda = contenedor(titulo: "Mi solución", visor: propia)
        let derecha = contenedor(titulo: "Solución oficial", visor: solucion)
        let pila = UIStackView(arrangedSubviews: [izquierda, derecha])
        pila.axis = .horizontal
        pila.distribution = .fillEqually
        pila.spacing = 1
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            pila.topAnchor.constraint(equalTo: view.topAnchor),
            pila.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Marcar diferencia", style: .plain,
            target: self, action: #selector(marcarDiferencia))
    }

    private func contenedor(titulo: String, visor: PDFView) -> UIView {
        let etiqueta = UILabel()
        etiqueta.text = titulo
        etiqueta.textAlignment = .center
        etiqueta.font = .preferredFont(forTextStyle: .headline)
        visor.translatesAutoresizingMaskIntoConstraints = false
        let pila = UIStackView(arrangedSubviews: [etiqueta, visor])
        pila.axis = .vertical
        pila.spacing = 8
        return pila
    }

    @objc private func marcarDiferencia() {
        let alerta = UIAlertController(title: "Diferencia encontrada", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Qué cambió o dónde estuvo el error" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Registrar", style: .default) {
            [weak self] _ in
            guard let texto = alerta.textFields?.first?.text,
                  !texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                return
            }
            self?.alRegistrarDiferencia?(texto)
        })
        present(alerta, animated: true)
    }
}

final class PanelRubricaIPad: UITableViewController {
    private var rubrica: RubricaAcademicaIPad

    init(rubrica: RubricaAcademicaIPad) {
        self.rubrica = rubrica
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = rubrica.titulo
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add, target: self, action: #selector(anadirCriterio))
    }

    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        max(1, rubrica.criterios.count)
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        guard !rubrica.criterios.isEmpty else {
            celda.textLabel?.text = "Agregue un criterio de evaluación"
            return celda
        }
        let criterio = rubrica.criterios[indexPath.row]
        celda.textLabel?.text = criterio.titulo
        let puntos = criterio.puntosObtenidos.map { String($0) } ?? "—"
        celda.detailTextLabel?.text = "\(puntos) / \(criterio.puntosMaximos) · \(criterio.comentarioProfesor)"
        celda.accessoryType = .disclosureIndicator
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        guard !rubrica.criterios.isEmpty else { anadirCriterio(); return }
        editar(indexPath.row)
    }

    @objc private func anadirCriterio() {
        editar(nil)
    }

    private func editar(_ indice: Int?) {
        let actual = indice.map { rubrica.criterios[$0] }
        let alerta = UIAlertController(title: actual == nil ? "Nuevo criterio" : "Editar criterio",
                                       message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Criterio"; $0.text = actual?.titulo }
        alerta.addTextField { $0.placeholder = "Puntos máximos"; $0.keyboardType = .decimalPad; $0.text = actual.map { String($0.puntosMaximos) } }
        alerta.addTextField {
            $0.placeholder = "Puntos obtenidos"
            $0.keyboardType = .decimalPad
            if let puntos = actual?.puntosObtenidos { $0.text = String(puntos) }
        }
        alerta.addTextField { $0.placeholder = "Comentario del profesor"; $0.text = actual?.comentarioProfesor }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self,
                  let titulo = alerta.textFields?[0].text,
                  !titulo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
                  let maximos = Double(alerta.textFields?[1].text ?? "") else { return }
            var criterio = actual ?? CriterioRubricaIPad(
                titulo: titulo, puntosMaximos: maximos)
            criterio.titulo = titulo
            criterio.puntosMaximos = maximos
            criterio.puntosObtenidos = Double(alerta.textFields?[2].text ?? "")
            criterio.comentarioProfesor = alerta.textFields?[3].text ?? ""
            if let obtenidos = criterio.puntosObtenidos, obtenidos < maximos,
               !criterio.comentarioProfesor.isEmpty {
                let error = CentroAcademico.compartido.registrarError(
                    cursoId: self.rubrica.cursoId, conceptoId: nil,
                    enunciado: criterio.titulo,
                    respuestaDada: "\(obtenidos) de \(maximos) puntos",
                    correcta: "Cumplir el criterio completo",
                    causa: criterio.comentarioProfesor)
                criterio.errorId = error.id
            }
            if let indice { self.rubrica.criterios[indice] = criterio }
            else { self.rubrica.criterios.append(criterio) }
            CentroEvaluacionIPad.compartido.guardarRubrica(self.rubrica)
            self.tableView.reloadData()
        })
        present(alerta, animated: true)
    }
}

final class PanelEvaluacionAcademicaIPad: UITableViewController,
                                          VNDocumentCameraViewControllerDelegate,
                                          UIDocumentPickerDelegate {
    private let sesion: Sesion
    private weak var navegador: VistaLienzoNavegable?
    private var estado = CentroEvaluacionIPad.compartido.instantanea()
    private var esperandoSolucion = false
    private var cursoId: UUID? {
        CentroExperienciaIPad.compartido.instantanea().espacioActivo?.cursoId
    }

    init(sesion: Sesion, navegador: VistaLienzoNavegable) {
        self.sesion = sesion; self.navegador = navegador
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Entregas y práctica"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        refrescar()
    }

    private func refrescar() {
        estado = CentroEvaluacionIPad.compartido.instantanea()
        tableView.reloadData()
    }

    override func numberOfSections(in tableView: UITableView) -> Int { 5 }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0: return 3
        case 1: return 2
        case 2: return max(1, estado.rubricas.filter { cursoId == nil || $0.cursoId == cursoId }.count)
        case 3: return max(1, CentroEvaluacionIPad.compartido.bloqueos(cursoId: cursoId).count)
        default: return 2
        }
    }

    override func tableView(_ tableView: UITableView,
                            titleForHeaderInSection section: Int) -> String? {
        ["Entregar tarea", "Examen y práctica", "Rúbricas",
         "Dónde se traba", "Aprendizaje"][section]
    }

    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        switch indexPath.section {
        case 0 where indexPath.row == 0:
            celda.textLabel?.text = "Preparar la nota actual como PDF A4"
            celda.detailTextLabel?.text = "Recorta contenido, ajusta, numera y muestra vista previa"
            celda.imageView?.image = UIImage(systemName: "doc.badge.arrow.up")
        case 0:
            if indexPath.row == 1 {
                celda.textLabel?.text = "Escanear tarea en papel"
                celda.detailTextLabel?.text = "Secuencia, perspectiva, portada y PDF"
                celda.imageView?.image = UIImage(systemName: "doc.viewfinder")
            } else {
                celda.textLabel?.text = "Comparar con solución oficial"
                celda.detailTextLabel?.text = "Vista lado a lado y registro de diferencias"
                celda.imageView?.image = UIImage(systemName: "rectangle.split.2x1")
            }
        case 1 where indexPath.row == 0:
            celda.textLabel?.text = "Crear examen simulado"
            celda.detailTextLabel?.text = "Interleaving de tarjetas y errores; ayuda de Tinta oculta"
            celda.imageView?.image = UIImage(systemName: "timer")
        case 1:
            celda.textLabel?.text = "Registrar un intento"
            celda.detailTextLabel?.text = "Respuesta, tiempo, confianza y error recurrente"
            celda.imageView?.image = UIImage(systemName: "checklist")
        case 2:
            let rubricas = estado.rubricas.filter { cursoId == nil || $0.cursoId == cursoId }
            guard !rubricas.isEmpty else {
                celda.textLabel?.text = "Crear rúbrica / retroalimentación"
                celda.imageView?.image = UIImage(systemName: "plus.circle")
                return celda
            }
            let rubrica = rubricas[indexPath.row]
            celda.textLabel?.text = rubrica.titulo
            celda.detailTextLabel?.text = "\(rubrica.criterios.count) criterios"
            celda.accessoryType = .disclosureIndicator
        case 3:
            let bloqueos = CentroEvaluacionIPad.compartido.bloqueos(cursoId: cursoId)
            guard !bloqueos.isEmpty else {
                celda.textLabel?.text = "Aún no hay suficientes intentos comparables"
                celda.selectionStyle = .none
                return celda
            }
            let bloqueo = bloqueos[indexPath.row]
            celda.textLabel?.text = bloqueo.categoria
            celda.detailTextLabel?.text = String(
                format: "%.1f× más lento · %.0f s · %d intentos",
                bloqueo.factorSobrePromedio, bloqueo.segundosPromedio, bloqueo.intentos)
        default:
            if indexPath.row == 0 {
                celda.textLabel?.text = "Marcar: aprendí esto"
                celda.detailTextLabel?.text = "Registra qué explicación cambió duda → entendido"
                celda.imageView?.image = UIImage(systemName: "checkmark.seal")
            } else {
                let recurrentes = CentroAcademico.compartido.instantanea().errores
                    .filter { !$0.resuelto && $0.repeticiones >= 2
                        && (cursoId == nil || $0.cursoId == cursoId) }
                celda.textLabel?.text = "Errores repetidos"
                celda.detailTextLabel?.text = "\(recurrentes.count) patrones pendientes"
                celda.imageView?.image = UIImage(systemName: "repeat.circle")
            }
        }
        return celda
    }

    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        defer { tableView.deselectRow(at: indexPath, animated: true) }
        switch indexPath.section {
        case 0 where indexPath.row == 0: configurarEntregaEscaneada(false)
        case 0 where indexPath.row == 1: abrirEscaner()
        case 0: elegirSolucionOficial()
        case 1 where indexPath.row == 0: configurarExamen()
        case 1: registrarIntentoManual()
        case 2:
            let rubricas = estado.rubricas.filter { cursoId == nil || $0.cursoId == cursoId }
            if rubricas.isEmpty { crearRubrica() }
            else { navigationController?.pushViewController(
                PanelRubricaIPad(rubrica: rubricas[indexPath.row]), animated: true) }
        case 4 where indexPath.row == 0: marcarEntendido()
        case 4: mostrarErroresRepetidos()
        default: break
        }
    }

    private func configurarEntregaEscaneada(_ escaneo: Bool,
                                             imagenes: [UIImage] = []) {
        let espacio = CentroExperienciaIPad.compartido.instantanea().espacioActivo
        let alerta = UIAlertController(title: "Preparar entrega", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Título"; $0.text = "Tarea" }
        alerta.addTextField { $0.placeholder = "Nombre del estudiante" }
        alerta.addTextField { $0.placeholder = "Identificación" }
        alerta.addTextField { $0.placeholder = "Profesor" }
        alerta.addTextField { $0.placeholder = "Incluir portada (sí/no)"; $0.text = "sí" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Generar vista previa", style: .default) {
            [weak self] _ in
            guard let self, let navegador = self.navegador else { return }
            var entrega = EntregaAcademicaIPad(
                cursoId: self.cursoId,
                titulo: alerta.textFields?[0].text ?? "Tarea")
            entrega.portada = ConfiguracionPortadaEntregaIPad(
                incluir: !["no", "n"].contains(
                    (alerta.textFields?[4].text ?? "sí").lowercased()),
                estudiante: alerta.textFields?[1].text ?? "",
                identificacion: alerta.textFields?[2].text ?? "",
                curso: espacio?.nombre ?? "",
                profesor: alerta.textFields?[3].text ?? "",
                titulo: entrega.titulo)
            do {
                let url = try escaneo
                    ? PreparadorEntregaIPad.prepararEscaneo(imagenes, entrega: &entrega)
                    : PreparadorEntregaIPad.preparar(
                        sesion: self.sesion, navegador: navegador, entrega: &entrega)
                self.navigationController?.pushViewController(
                    VistaPreviaEntregaIPad(entrega: entrega, url: url), animated: true)
                self.refrescar()
            } catch { self.avisar(error.localizedDescription) }
        })
        present(alerta, animated: true)
    }

    private func abrirEscaner() {
        guard VNDocumentCameraViewController.isSupported else {
            avisar("El escáner de documentos no está disponible en este iPad.")
            return
        }
        let scanner = VNDocumentCameraViewController()
        scanner.delegate = self
        present(scanner, animated: true)
    }

    private func elegirSolucionOficial() {
        esperandoSolucion = true
        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: [.pdf], asCopy: false)
        selector.delegate = self
        present(selector, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard esperandoSolucion, let origen = urls.first,
              let navegador else { return }
        esperandoSolucion = false
        do {
            let adjunto = try RepositorioAdjuntos.compartido.importar(url: origen)
            let propia = try ExportadorCompletoIPad.exportar(
                .pdfCompleto, sesion: sesion, navegador: navegador,
                titulo: "Mi solución")
            let comparador = ComparadorSolucionIPad(
                propia: propia, solucion: adjunto.url)
            comparador.alRegistrarDiferencia = { [weak self] texto in
                guard let self else { return }
                _ = CentroAcademico.compartido.registrarError(
                    cursoId: self.cursoId, conceptoId: nil,
                    enunciado: "Diferencia con solución oficial",
                    respuestaDada: texto,
                    correcta: "Revisar la solución oficial",
                    causa: "Comparación lado a lado")
                self.avisar("Diferencia vinculada al registro de errores.")
            }
            navigationController?.pushViewController(comparador, animated: true)
        } catch { avisar(error.localizedDescription) }
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
        esperandoSolucion = false
    }

    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFinishWith scan: VNDocumentCameraScan) {
        controller.dismiss(animated: true)
        let imagenes = (0..<scan.pageCount).map { scan.imageOfPage(at: $0) }
        configurarEntregaEscaneada(true, imagenes: imagenes)
    }

    func documentCameraViewControllerDidCancel(_ controller: VNDocumentCameraViewController) {
        controller.dismiss(animated: true)
    }

    func documentCameraViewController(_ controller: VNDocumentCameraViewController,
                                      didFailWithError error: Error) {
        controller.dismiss(animated: true)
        avisar(error.localizedDescription)
    }

    private func configurarExamen() {
        let alerta = UIAlertController(title: "Examen simulado", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Cantidad de preguntas"; $0.keyboardType = .numberPad; $0.text = "10" }
        alerta.addTextField { $0.placeholder = "Minutos"; $0.keyboardType = .numberPad; $0.text = "30" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Empezar", style: .default) { [weak self] _ in
            guard let self else { return }
            let cantidad = Int(alerta.textFields?[0].text ?? "") ?? 10
            let minutos = Int(alerta.textFields?[1].text ?? "") ?? 30
            let examen = GeneradorExamenLocalIPad.generar(
                cursoId: self.cursoId, cantidad: cantidad, minutos: minutos)
            guard !examen.preguntas.isEmpty else {
                self.avisar("No hay suficientes tarjetas o errores con respuesta para crear el examen.")
                return
            }
            let controlador = ControladorExamenSimuladoIPad(examen: examen)
            self.present(UINavigationController(rootViewController: controlador), animated: true)
        })
        present(alerta, animated: true)
    }

    private func registrarIntentoManual() {
        let alerta = UIAlertController(title: "Registrar intento", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Enunciado" }
        alerta.addTextField { $0.placeholder = "Su respuesta" }
        alerta.addTextField { $0.placeholder = "Respuesta correcta" }
        alerta.addTextField { $0.placeholder = "Segundos"; $0.keyboardType = .numberPad }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        for nivel in NivelConfianzaEjercicio.allCases {
            alerta.addAction(UIAlertAction(title: nivel.nombre, style: .default) {
                [weak self] _ in
                guard let self,
                      let enunciado = alerta.textFields?[0].text,
                      let dada = alerta.textFields?[1].text,
                      let correcta = alerta.textFields?[2].text else { return }
                let evaluacion = GeneradorExamenLocalIPad.evaluar(
                    respuesta: dada, esperada: correcta)
                CentroEvaluacionIPad.compartido.registrarIntento(
                    IntentoEjercicioIPad(
                        cursoId: self.cursoId, enunciado: enunciado,
                        respuesta: dada, respuestaEsperada: correcta,
                        correcto: evaluacion.correcta ?? false,
                        requiereRevision: evaluacion.correcta == nil || evaluacion.revision,
                        confianza: nivel,
                        segundos: Double(alerta.textFields?[3].text ?? "") ?? 0))
                self.refrescar()
            })
        }
        present(alerta, animated: true)
    }

    private func crearRubrica() {
        let alerta = UIAlertController(title: "Nueva rúbrica", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Título" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Crear", style: .default) { [weak self] _ in
            guard let self, let titulo = alerta.textFields?.first?.text,
                  !titulo.isEmpty else { return }
            let rubrica = RubricaAcademicaIPad(cursoId: self.cursoId, titulo: titulo)
            CentroEvaluacionIPad.compartido.guardarRubrica(rubrica)
            self.navigationController?.pushViewController(
                PanelRubricaIPad(rubrica: rubrica), animated: true)
            self.refrescar()
        })
        present(alerta, animated: true)
    }

    private func marcarEntendido() {
        let alerta = UIAlertController(title: "Aprendí esto", message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Concepto" }
        alerta.addTextField { $0.placeholder = "¿Qué explicación funcionó?" }
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        alerta.addAction(UIAlertAction(title: "Registrar", style: .default) { [weak self] _ in
            guard let self else { return }
            let conceptoTexto = alerta.textFields?[0].text ?? ""
            let concepto = CentroAcademico.compartido.instantanea().conceptos.first {
                $0.cursoId == self.cursoId
                    && $0.nombre.localizedCaseInsensitiveCompare(conceptoTexto) == .orderedSame
            }
            CentroEvaluacionIPad.compartido.registrarEntendido(
                cursoId: self.cursoId, conceptoId: concepto?.id,
                explicacion: alerta.textFields?[1].text ?? "",
                referencia: nil)
            self.refrescar()
        })
        present(alerta, animated: true)
    }

    private func mostrarErroresRepetidos() {
        let errores = CentroAcademico.compartido.instantanea().errores
            .filter { !$0.resuelto && $0.repeticiones >= 2
                && (cursoId == nil || $0.cursoId == cursoId) }
            .sorted { $0.repeticiones > $1.repeticiones }
        let texto = errores.prefix(20).map {
            "×\($0.repeticiones) · \($0.enunciado)\nCausa: \($0.causa)"
        }.joined(separator: "\n\n")
        avisar(texto.isEmpty ? "No hay errores repetidos pendientes." : texto)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
