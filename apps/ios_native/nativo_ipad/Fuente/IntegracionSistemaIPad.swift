import AppIntents
import Foundation
import UIKit
import WidgetKit

// MARK: - Consumo confirmado del buzón compartido

/// Evita que dos escenas o dos pestañas procesen el mismo manifiesto a la vez.
/// El archivo sigue pendiente si el receptor devuelve `false`.
final class CoordinadorEntradasSistemaIPad {
    static let compartido = CoordinadorEntradasSistemaIPad()
    private let bloqueo = NSLock()
    private var consumiendo = false

    private init() {}

    @discardableResult
    func consumir(_ procesar: (EntradaSistemaIPad, URL?) -> Bool) -> Int {
        bloqueo.lock()
        guard !consumiendo else { bloqueo.unlock(); return 0 }
        consumiendo = true
        bloqueo.unlock()
        defer {
            bloqueo.lock(); consumiendo = false; bloqueo.unlock()
        }

        var confirmadas = 0
        for pendiente in BuzonCompartidoSistemaIPad.pendientesSinConfirmar() {
            guard procesar(pendiente.entrada, pendiente.adjunto),
                  BuzonCompartidoSistemaIPad.confirmar(pendiente) else { continue }
            confirmadas += 1
        }
        return confirmadas
    }
}

enum ImportadorEntradaSistemaIPad {
    @discardableResult
    static func guardarEnBandeja(_ entrada: EntradaSistemaIPad,
                                 adjunto url: URL?) -> Bool {
        let cursoId = resolverCurso(entrada.cursoSugerido)
        let prefijoCurso: String
        if entrada.destino == "Curso", cursoId == nil,
           let nombre = entrada.cursoSugerido, !nombre.isEmpty {
            prefijoCurso = "Curso sugerido sin asociar: \(nombre)\n"
        } else {
            prefijoCurso = ""
        }

        switch entrada.tipo {
        case .texto:
            guard let texto = entrada.texto?.trimmingCharacters(
                in: .whitespacesAndNewlines), !texto.isEmpty else { return false }
            _ = CentroAcademico.compartido.capturar(
                prefijoCurso + texto, tipo: .texto, cursoSugerido: cursoId)
            return true
        case .enlace:
            guard let texto = entrada.texto,
                  URL(string: texto) != nil else { return false }
            _ = CentroAcademico.compartido.capturar(
                prefijoCurso + texto, tipo: .enlace, cursoSugerido: cursoId)
            return true
        case .archivo, .imagen, .audio:
            guard let url, FileManager.default.fileExists(atPath: url.path) else {
                return false
            }
            do {
                let archivo = try RepositorioAdjuntos.compartido.importar(url: url)
                let tipo: TipoCaptura = entrada.tipo == .imagen ? .foto
                    : (entrada.tipo == .audio ? .audio : .archivo)
                let nombre = entrada.nombreOriginal ?? archivo.nombreOriginal
                _ = CentroAcademico.compartido.capturar(
                    prefijoCurso + nombre, tipo: tipo, hash: archivo.hash,
                    cursoSugerido: cursoId)
                return true
            } catch {
                return false
            }
        case .nuevaNota, .continuar, .capturarFoto, .capturarVoz:
            // Estas dos acciones pertenecen al hospedador de pestañas.
            return false
        }
    }

    private static func resolverCurso(_ consulta: String?) -> UUID? {
        guard let consulta, !consulta.isEmpty else { return nil }
        let buscado = normalizar(consulta)
        return CentroAcademico.compartido.instantanea().cursos.first { curso in
            normalizar(curso.nombre) == buscado || normalizar(curso.codigo) == buscado
        }?.id
    }

    private static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.caseInsensitive, .diacriticInsensitive],
                      locale: .current)
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

// MARK: - Estado local de widgets

enum PublicadorWidgetTintaIPad {
    static func actualizar() {
        let base = CentroAcademico.compartido.instantanea()
        let experiencia = CentroExperienciaIPad.compartido.instantanea()
        var estado = EstadoWidgetTintaIPad()

        if let clase = siguienteClase(base) {
            estado.proximaClase = "\(clase.curso.nombre) · \(fecha.string(from: clase.fecha))"
        }
        if let reciente = experiencia.recientes.max(by: { $0.fecha < $1.fecha }) {
            estado.continuar = "Continuar: \(reciente.titulo)"
        }
        if let tarea = base.tareas
            .filter({ $0.estado != .completada && $0.estado != .archivada
                && $0.fechaLimite != nil })
            .min(by: { ($0.fechaLimite ?? .distantFuture)
                < ($1.fechaLimite ?? .distantFuture) }) {
            let fStr = fecha.string(from: tarea.fechaLimite ?? Date())
            estado.proximaEvaluacion = "\(tarea.titulo) · \(fStr)"
        }
        // let resumen = CentroAcademico.compartido.resumenHoy()
        estado.accion = "Abrir Tinta"
        BuzonCompartidoSistemaIPad.guardarEstadoWidget(estado)
        WidgetCenter.shared.reloadAllTimelines()
    }

    private static func siguienteClase(_ base: BaseAcademica,
                                       ahora: Date = Date())
        -> (fecha: Date, curso: CursoAcademico)? {
        let calendario = Calendar.current
        var candidatas: [(Date, CursoAcademico)] = []
        for bloque in base.horarios {
            guard let curso = base.cursos.first(where: {
                $0.id == bloque.cursoId && !$0.archivado
            }) else { continue }
            var componentes = DateComponents()
            componentes.weekday = bloque.diaSemana
            componentes.hour = bloque.horaInicio
            componentes.minute = bloque.minutoInicio
            guard let fecha = calendario.nextDate(
                after: ahora.addingTimeInterval(-60), matching: componentes,
                matchingPolicy: .nextTime, repeatedTimePolicy: .first,
                direction: .forward) else { continue }
            candidatas.append((fecha, curso))
        }
        return candidatas.min(by: { $0.0 < $1.0 }).map {
            (fecha: $0.0, curso: $0.1)
        }
    }

    private static let fecha: DateFormatter = {
        let formato = DateFormatter()
        formato.locale = .current
        formato.setLocalizedDateFormatFromTemplate("EEE d MMM HH:mm")
        return formato
    }()
}

// MARK: - App Intents y Atajos de Siri, sin servicio externo

struct NuevaNotaTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Crear nueva nota en Tinta"
    static var description = IntentDescription(
        "Crea una pestaña de nota local y abre Tinta en el iPad.")
    static var openAppWhenRun = true

    @Parameter(title: "Título")
    var titulo: String?

    static var parameterSummary: some ParameterSummary {
        Summary("Crear nota \(\.$titulo)")
    }

    func perform() async throws -> some IntentResult & ProvidesDialog {
        var entrada = EntradaSistemaIPad(tipo: .nuevaNota)
        entrada.texto = titulo?.trimmingCharacters(in: .whitespacesAndNewlines)
        try BuzonCompartidoSistemaIPad.publicar(entrada)
        return .result(dialog: "La nueva nota quedó preparada en Tinta.")
    }
}

struct CapturarPensamientoTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Capturar pensamiento en Tinta"
    static var description = IntentDescription(
        "Envía texto a la bandeja local de Tinta sin usar una nube.")

    @Parameter(title: "Texto")
    var texto: String

    @Parameter(title: "Curso opcional")
    var curso: String?

    static var parameterSummary: some ParameterSummary {
        Summary("Capturar \(\.$texto)") {
            \.$curso
        }
    }

    func perform() async throws -> some IntentResult & ProvidesDialog {
        let limpio = texto.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !limpio.isEmpty else {
            return .result(dialog: "No se recibió texto para guardar.")
        }
        var entrada = EntradaSistemaIPad(tipo: .texto, texto: limpio)
        entrada.destino = curso?.isEmpty == false ? "Curso" : "Inbox"
        entrada.cursoSugerido = curso
        try BuzonCompartidoSistemaIPad.publicar(entrada)
        return .result(dialog: "El pensamiento quedó en la bandeja local de Tinta.")
    }
}

struct ContinuarTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Continuar en Tinta"
    static var description = IntentDescription(
        "Abre la última pestaña y posición conservada en el iPad.")
    static var openAppWhenRun = true

    func perform() async throws -> some IntentResult & ProvidesDialog {
        return .result(dialog: "Abriendo la última nota de Tinta.")
    }
}

struct CapturarPizarraTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Capturar pizarra en Tinta"
    static var description = IntentDescription(
        "Abre directamente el escáner local de pizarra en el iPad.")
    static var openAppWhenRun = true
    func perform() async throws -> some IntentResult & ProvidesDialog {
        try BuzonCompartidoSistemaIPad.publicar(
            EntradaSistemaIPad(tipo: .capturarFoto))
        return .result(dialog: "Abriendo la captura de pizarra.")
    }
}

struct CapturarVozTintaIntent: AppIntent {
    static var title: LocalizedStringResource = "Capturar voz en Tinta"
    static var description = IntentDescription(
        "Abre Tinta en modo de dictado; el audio no sale del iPad.")
    static var openAppWhenRun = true
    func perform() async throws -> some IntentResult & ProvidesDialog {
        try BuzonCompartidoSistemaIPad.publicar(
            EntradaSistemaIPad(tipo: .capturarVoz))
        return .result(dialog: "Abriendo el dictado de Tinta.")
    }
}

struct AtajosTintaIPad: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: NuevaNotaTintaIntent(),
            phrases: ["Crear una nota en \(.applicationName)"],
            shortTitle: "Nueva nota",
            systemImageName: "square.and.pencil")
        AppShortcut(
            intent: CapturarPensamientoTintaIntent(),
            phrases: ["Capturar un pensamiento en \(.applicationName)"],
            shortTitle: "Capturar pensamiento",
            systemImageName: "tray.and.arrow.down")
        AppShortcut(
            intent: ContinuarTintaIntent(),
            phrases: ["Continuar estudiando en \(.applicationName)"],
            shortTitle: "Continuar",
            systemImageName: "arrow.forward.circle")
        AppShortcut(
            intent: CapturarPizarraTintaIntent(),
            phrases: ["Capturar una pizarra en \(.applicationName)"],
            shortTitle: "Capturar pizarra",
            systemImageName: "camera.viewfinder")
        AppShortcut(
            intent: CapturarVozTintaIntent(),
            phrases: ["Dictar una nota en \(.applicationName)"],
            shortTitle: "Capturar voz",
            systemImageName: "mic")
    }
}
