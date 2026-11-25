import Foundation

enum NivelPistaProblemaIPad: Int, Codable, CaseIterable, Comparable {
    case orientar = 1
    case recordarConcepto = 2
    case sugerirMetodo = 3
    case diagnosticarError = 4
    case reveladoParcial = 5

    static func < (lhs: NivelPistaProblemaIPad, rhs: NivelPistaProblemaIPad) -> Bool {
        lhs.rawValue < rhs.rawValue
    }

    var nombre: String {
        switch self {
        case .orientar: return "Orientación"
        case .recordarConcepto: return "Concepto"
        case .sugerirMetodo: return "Método"
        case .diagnosticarError: return "Diagnóstico"
        case .reveladoParcial: return "Revelado parcial"
        }
    }
}

struct PistaGraduadaProblemaIPad: Codable, Equatable, Identifiable {
    var id: String { "\(problemaId.uuidString):\(nivel.rawValue)" }
    var problemaId: UUID
    var nivel: NivelPistaProblemaIPad
    var titulo: String
    var texto: String
    var revelaSolucion: Bool
    var evidencia: [String]
}

struct EstadoAccesoPistasProblemaIPad: Codable, Equatable {
    var problemaId: UUID
    var nivelesVistos: Set<Int> = []
    var ultimaConsulta: Date?
    var totalConsultas = 0
}

private struct ArchivoPistasProblemaIPad: Codable {
    var version = 1
    var estados: [EstadoAccesoPistasProblemaIPad] = []
}

enum ErrorPistaProblemaIPad: LocalizedError, Equatable {
    case problemaInexistente
    case nivelBloqueado
    case solucionNoDisponible

    var errorDescription: String? {
        switch self {
        case .problemaInexistente: return "El problema ya no existe."
        case .nivelBloqueado: return "Este nivel de pista sigue bloqueado mientras el intento está activo o no hay un intento cerrado."
        case .solucionNoDisponible: return "No existe una solución vinculada para revelar parcialmente."
        }
    }
}

final class RepositorioAccesoPistasIPad {
    static let compartido = RepositorioAccesoPistasIPad()
    private let bloqueo = NSLock()
    private let url: URL
    private var archivo: ArchivoPistasProblemaIPad

    init(base: URL? = nil, archivos: FileManager = .default) {
        let carpeta = base ?? archivos.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Academico/Pistas", isDirectory: true)
        try? archivos.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("pistas-v1.json")
        let decoder = JSONDecoder(); decoder.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decoder.decode(ArchivoPistasProblemaIPad.self, from: datos) {
            archivo = leido
        } else { archivo = ArchivoPistasProblemaIPad() }
    }

    func estado(problemaId: UUID) -> EstadoAccesoPistasProblemaIPad {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return archivo.estados.first { $0.problemaId == problemaId }
            ?? EstadoAccesoPistasProblemaIPad(problemaId: problemaId)
    }

    func registrar(problemaId: UUID, nivel: NivelPistaProblemaIPad) {
        bloqueo.lock(); defer { bloqueo.unlock() }
        if let i = archivo.estados.firstIndex(where: { $0.problemaId == problemaId }) {
            archivo.estados[i].nivelesVistos.insert(nivel.rawValue)
            archivo.estados[i].ultimaConsulta = Date()
            archivo.estados[i].totalConsultas += 1
        } else {
            archivo.estados.append(.init(
                problemaId: problemaId,
                nivelesVistos: [nivel.rawValue],
                ultimaConsulta: Date(),
                totalConsultas: 1))
        }
        persistirSinBloqueo()
    }

    private func persistirSinBloqueo() {
        let encoder = JSONEncoder(); encoder.dateEncodingStrategy = .iso8601
        encoder.outputFormatting = [.sortedKeys]
        if let datos = try? encoder.encode(archivo) {
            try? datos.write(to: url, options: [.atomic, .completeFileProtection])
        }
    }
}

enum GeneradorPistasGraduadasIPad {
    static func pista(
        problema: ProblemaBancoIPad,
        nivel: NivelPistaProblemaIPad,
        formulas: [FormulaTecnicaIPad]
    ) throws -> PistaGraduadaProblemaIPad {
        let intentoActivo = problema.intentos.contains { $0.correcto == nil }
        let intentosCerrados = problema.intentos.filter { $0.correcto != nil }

        switch nivel {
        case .orientar:
            return .init(
                problemaId: problema.id,
                nivel: nivel,
                titulo: "Separe datos, incógnita y restricciones",
                texto: orientacion(problema),
                revelaSolucion: false,
                evidencia: ["enunciado", "tema:\(problema.tema)"])

        case .recordarConcepto:
            let relacionadas = formulasRelacionadas(problema: problema, formulas: formulas)
            let texto: String
            if relacionadas.isEmpty {
                texto = "Antes de operar números, escriba qué principio físico o relación matemática conecta directamente lo conocido con lo que se pide. Defina también unidades y convención de signos."
            } else {
                texto = "Revise estas relaciones del mismo contexto antes de sustituir valores: " +
                    relacionadas.prefix(3).map { formula in
                        formula.nombre.isEmpty ? formula.expresion : "\(formula.nombre): \(formula.expresion)"
                    }.joined(separator: " · ")
            }
            return .init(
                problemaId: problema.id, nivel: nivel,
                titulo: "Recupere el concepto antes de calcular",
                texto: texto, revelaSolucion: false,
                evidencia: relacionadas.prefix(3).map { "formula:\($0.id.uuidString)" })

        case .sugerirMetodo:
            return .init(
                problemaId: problema.id, nivel: nivel,
                titulo: "Construya el procedimiento sin saltar al resultado",
                texto: metodo(problema), revelaSolucion: false,
                evidencia: ["dificultad:\(problema.dificultad.rawValue)", "tema:\(problema.tema)"])

        case .diagnosticarError:
            let errores = problema.erroresDePaso.suffix(3)
            let texto: String
            if errores.isEmpty {
                texto = "No hay un error histórico registrado para este problema. Verifique, en este orden: diagrama/modelo, ecuación usada, signos, unidades, despeje y sustitución numérica."
            } else {
                texto = "Sus errores anteriores sugieren revisar específicamente: " +
                    errores.map { $0.descripcion }.joined(separator: " · ") +
                    ". No mire todavía la corrección: localice por su cuenta el primer paso donde deja de poder justificar la transformación."
            }
            return .init(
                problemaId: problema.id, nivel: nivel,
                titulo: "Busque el primer paso no justificable",
                texto: texto, revelaSolucion: false,
                evidencia: errores.map { "error:\($0.id.uuidString)" })

        case .reveladoParcial:
            guard !intentoActivo, !intentosCerrados.isEmpty else {
                throw ErrorPistaProblemaIPad.nivelBloqueado
            }
            guard let solucion = problema.solucion,
                  !solucion.texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
                throw ErrorPistaProblemaIPad.solucionNoDisponible
            }
            return .init(
                problemaId: problema.id, nivel: nivel,
                titulo: "Compare sólo el arranque de la solución",
                texto: extractoSeguro(solucion.texto),
                revelaSolucion: true,
                evidencia: ["solucion-vinculada"])
        }
    }

    static func disponibles(problema: ProblemaBancoIPad) -> [NivelPistaProblemaIPad] {
        var niveles: [NivelPistaProblemaIPad] = [.orientar, .recordarConcepto, .sugerirMetodo, .diagnosticarError]
        let activo = problema.intentos.contains { $0.correcto == nil }
        if !activo,
           problema.intentos.contains(where: { $0.correcto != nil }),
           problema.solucion?.texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty == false {
            niveles.append(.reveladoParcial)
        }
        return niveles
    }

    private static func orientacion(_ problema: ProblemaBancoIPad) -> String {
        let tema = problema.tema.trimmingCharacters(in: .whitespacesAndNewlines)
        var instrucciones = [
            "Reescriba en una línea qué cantidad debe encontrar.",
            "Liste los datos con símbolo, valor y unidad; no sustituya aún.",
            "Anote condiciones implícitas del modelo y una convención de signos si aplica."
        ]
        if !tema.isEmpty && tema != "Sin tema" {
            instrucciones.insert("Clasifique el problema dentro de «\(tema)» y descarte relaciones de otros temas que no correspondan.", at: 0)
        }
        return instrucciones.map { "• \($0)" }.joined(separator: "\n")
    }

    private static func metodo(_ problema: ProblemaBancoIPad) -> String {
        switch problema.dificultad {
        case .introduccion:
            return "Dibuje o describa el sistema, elija una sola relación fundamental, despeje simbólicamente y después sustituya valores con unidades."
        case .practica:
            return "Construya una cadena corta: modelo/diagrama → principio principal → ecuaciones auxiliares → despeje simbólico → chequeo dimensional → números."
        case .examen:
            return "Trabaje como en examen: identifique primero el principio que reduce el problema, escriba supuestos, resuelva simbólicamente y reserve el último paso para números y verificación de orden de magnitud."
        case .desafio:
            return "Divida el problema en subproblemas con variables intermedias. Resuelva dependencias antes de sustituir y compruebe cada interfaz entre subproblemas con unidades o límites físicos."
        }
    }

    private static func formulasRelacionadas(
        problema: ProblemaBancoIPad,
        formulas: [FormulaTecnicaIPad]
    ) -> [FormulaTecnicaIPad] {
        let tema = Normalizador.texto(problema.tema)
        let etiquetas = Set(problema.etiquetas.map(Normalizador.texto))
        return formulas.filter { formula in
            (problema.cursoId == nil || formula.cursoId == problema.cursoId) &&
            (tema.isEmpty || Normalizador.texto(formula.tema) == tema ||
             etiquetas.contains(Normalizador.texto(formula.tema)))
        }.sorted {
            if $0.favorita != $1.favorita { return $0.favorita && !$1.favorita }
            return $0.frecuenciaUso > $1.frecuenciaUso
        }
    }

    private static func extractoSeguro(_ texto: String) -> String {
        let limpio = texto.trimmingCharacters(in: .whitespacesAndNewlines)
        let lineas = limpio.split(whereSeparator: \.isNewline)
            .map { String($0).trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }
        let primera = lineas.first ?? String(limpio.prefix(220))
        let limitado = String(primera.prefix(220))
        return limitado + (primera.count > limitado.count ? "…" : "") +
            "\n\nDeténgase aquí e intente continuar sin abrir el resto de la solución."
    }
}

final class ServicioTeachMeProblemasIPad {
    private let estudio: CentroEstudioTecnicoIPad
    private let accesos: RepositorioAccesoPistasIPad

    init(estudio: CentroEstudioTecnicoIPad = .compartido,
         accesos: RepositorioAccesoPistasIPad = .compartido) {
        self.estudio = estudio; self.accesos = accesos
    }

    func pista(problemaId: UUID, nivel: NivelPistaProblemaIPad) throws -> PistaGraduadaProblemaIPad {
        let snapshot = estudio.instantanea()
        guard let problema = snapshot.problemas.first(where: { $0.id == problemaId }) else {
            throw ErrorPistaProblemaIPad.problemaInexistente
        }
        let pista = try GeneradorPistasGraduadasIPad.pista(
            problema: problema, nivel: nivel, formulas: snapshot.formulas)
        accesos.registrar(problemaId: problemaId, nivel: nivel)
        return pista
    }
}
