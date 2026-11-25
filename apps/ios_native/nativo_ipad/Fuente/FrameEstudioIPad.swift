import Foundation

/// Estado académico de un frame usado para resolver un ejercicio. Vive dentro
/// del propio `FrameApunteIPad`: el objetivo viaja con el documento, mientras
/// que una copia documental puede reiniciar los intentos para no falsificar el
/// historial de estudio del cuaderno nuevo.
enum EstadoFrameEstudioIPad: String, Codable, CaseIterable {
    case pendiente
    case intentando
    case repetir
    case dominado

    var nombre: String {
        switch self {
        case .pendiente: return "Pendiente"
        case .intentando: return "Intentando"
        case .repetir: return "Repetir"
        case .dominado: return "Dominado"
        }
    }
}

enum ResultadoIntentoFrameIPad: String, Codable {
    case correcto
    case repetir
}

struct IntentoFrameEstudioIPad: Codable, Identifiable, Equatable {
    var id = UUID()
    var inicio: Date
    var fin: Date?
    var resultado: ResultadoIntentoFrameIPad?
    var confianza: Double = 0.5
    var error = ""

    var activo: Bool { fin == nil }

    var duracion: TimeInterval? {
        fin.map { max(0, $0.timeIntervalSince(inicio)) }
    }
}

struct EstadoEstudioFrameIPad: Codable, Equatable {
    var objetivo = ""
    /// Identidad canónica del ejercicio cuando este frame representa un
    /// problema del banco. Es opcional para conservar compatibilidad con todos
    /// los frames creados antes de esta integración.
    var problemaId: UUID?
    var estado: EstadoFrameEstudioIPad = .pendiente
    var intentos: [IntentoFrameEstudioIPad] = []

    /// Son opcionales a propósito: builds anteriores ya serializaron
    /// `EstadoEstudioFrameIPad` sin estas claves y deben seguir abriendo.
    var proximoRepaso: Date?
    var ultimoRepaso: Date?
    var rachaCorrectos: Int?

    var intentoActivo: IntentoFrameEstudioIPad? {
        intentos.last(where: \.activo)
    }

    var intentosTerminados: [IntentoFrameEstudioIPad] {
        intentos.filter { !$0.activo }
    }

    var aciertos: Int {
        intentosTerminados.filter { $0.resultado == .correcto }.count
    }

    var confianzaPromedio: Double? {
        let terminados = intentosTerminados
        guard !terminados.isEmpty else { return nil }
        return terminados.reduce(0) { $0 + $1.confianza } / Double(terminados.count)
    }

    /// Iniciar dos veces sin cerrar no crea dos cronómetros para el mismo
    /// ejercicio. Se devuelve el intento que ya estaba activo.
    @discardableResult
    mutating func iniciar(ahora: Date = Date()) -> UUID {
        if let activo = intentoActivo {
            estado = .intentando
            return activo.id
        }
        let intento = IntentoFrameEstudioIPad(inicio: ahora)
        intentos.append(intento)
        estado = .intentando
        return intento.id
    }

    /// Cierra únicamente el intento indicado. Esto evita que una UI atrasada
    /// pueda cerrar por accidente un intento nuevo que empezó después.
    @discardableResult
    mutating func cerrar(
        intentoId: UUID,
        resultado: ResultadoIntentoFrameIPad,
        confianza: Double,
        error: String = "",
        ahora: Date = Date()
    ) -> Bool {
        guard let indice = intentos.firstIndex(where: {
            $0.id == intentoId && $0.activo
        }) else { return false }

        let fin = max(ahora, intentos[indice].inicio)
        let confianzaSegura = min(max(confianza, 0), 1)
        intentos[indice].fin = fin
        intentos[indice].resultado = resultado
        intentos[indice].confianza = confianzaSegura
        intentos[indice].error = error.trimmingCharacters(in: .whitespacesAndNewlines)
        estado = resultado == .correcto ? .dominado : .repetir

        let programacion = ProgramadorRepasoFrameIPad.programar(
            resultado: resultado,
            confianza: confianzaSegura,
            rachaPrevia: rachaCorrectos ?? 0,
            ahora: fin)
        ultimoRepaso = fin
        rachaCorrectos = programacion.rachaCorrectos
        proximoRepaso = programacion.proximoRepaso
        return true
    }

    /// La copia conserva qué había que practicar y qué problema representa,
    /// pero no hereda cronómetros, aciertos, errores ni agenda de repaso del
    /// documento de origen.
    func reiniciadoParaCopiaDocumental() -> EstadoEstudioFrameIPad {
        EstadoEstudioFrameIPad(
            objetivo: objetivo,
            problemaId: problemaId,
            estado: .pendiente,
            intentos: [],
            proximoRepaso: nil,
            ultimoRepaso: nil,
            rachaCorrectos: nil)
    }

    func descripcionRepaso(ahora: Date = Date()) -> String? {
        guard let proximoRepaso else { return nil }
        let segundos = proximoRepaso.timeIntervalSince(ahora)
        if segundos <= 0 { return "repasar ahora" }
        if segundos < 3_600 {
            return "repasar en \(max(1, Int(ceil(segundos / 60)))) min"
        }
        if segundos < 86_400 {
            return "repasar en \(max(1, Int(ceil(segundos / 3_600)))) h"
        }
        return "repasar en \(max(1, Int(ceil(segundos / 86_400)))) días"
    }

    var resumenCorto: String {
        let terminados = intentosTerminados.count
        var texto = "\(estado.nombre) · \(terminados) intento\(terminados == 1 ? "" : "s")"
        if problemaId != nil { texto += " · Problem Bank" }
        if let confianzaPromedio {
            texto += " · confianza \(Int((confianzaPromedio * 100).rounded()))%"
        }
        if let repaso = descripcionRepaso() { texto += " · \(repaso)" }
        return texto
    }
}

struct ProgramacionRepasoFrameIPad: Equatable {
    var proximoRepaso: Date
    var rachaCorrectos: Int
}

/// Programación local y explicable. No intenta adivinar conocimiento con IA:
/// usa el resultado, la confianza declarada y la racha de aciertos. Los fallos
/// vuelven pronto; los aciertos seguros separan progresivamente el siguiente
/// repaso y el intervalo nunca supera 90 días.
enum ProgramadorRepasoFrameIPad {
    static func programar(
        resultado: ResultadoIntentoFrameIPad,
        confianza: Double,
        rachaPrevia: Int,
        ahora: Date
    ) -> ProgramacionRepasoFrameIPad {
        let confianzaSegura = min(max(confianza, 0), 1)
        switch resultado {
        case .repetir:
            let segundos: TimeInterval = confianzaSegura < 0.4 ? 20 * 60 : 60 * 60
            return ProgramacionRepasoFrameIPad(
                proximoRepaso: ahora.addingTimeInterval(segundos),
                rachaCorrectos: 0)
        case .correcto:
            let racha = max(0, rachaPrevia) + 1
            let diasBase: Double
            if confianzaSegura < 0.45 { diasBase = 1 }
            else if confianzaSegura < 0.75 { diasBase = 3 }
            else { diasBase = 7 }
            let multiplicador = pow(1.7, Double(max(racha - 1, 0)))
            let dias = min(diasBase * multiplicador, 90)
            return ProgramacionRepasoFrameIPad(
                proximoRepaso: ahora.addingTimeInterval(dias * 86_400),
                rachaCorrectos: racha)
        }
    }
}

/// Decide qué unidad conviene practicar sin depender de IA ni de red. La regla
/// es deliberadamente explicable: continuar lo abierto, respetar el descanso
/// breve tras un fallo, atender repasos vencidos, cubrir pendientes y solo
/// entonces adelantar material ya programado para más adelante.
enum PlanificadorFramesEstudioIPad {
    static func indiceSiguiente(
        _ estudios: [EstadoEstudioFrameIPad],
        ahora: Date = Date()
    ) -> Int? {
        estudios.indices.min { izquierda, derecha in
            esMejor(estudios[izquierda], que: estudios[derecha], ahora: ahora)
        }
    }

    static func esMejor(
        _ candidato: EstadoEstudioFrameIPad,
        que actual: EstadoEstudioFrameIPad,
        ahora: Date = Date()
    ) -> Bool {
        let nivelCandidato = nivel(candidato, ahora: ahora)
        let nivelActual = nivel(actual, ahora: ahora)
        if nivelCandidato != nivelActual { return nivelCandidato < nivelActual }

        if candidato.proximoRepaso != actual.proximoRepaso {
            switch (candidato.proximoRepaso, actual.proximoRepaso) {
            case let (.some(a), .some(b)) where a != b: return a < b
            case (.some, .none): return true
            case (.none, .some): return false
            default: break
            }
        }

        let confianzaCandidato = candidato.confianzaPromedio ?? 0
        let confianzaActual = actual.confianzaPromedio ?? 0
        if abs(confianzaCandidato - confianzaActual) > 0.000_001 {
            return confianzaCandidato < confianzaActual
        }

        if candidato.intentosTerminados.count != actual.intentosTerminados.count {
            return candidato.intentosTerminados.count < actual.intentosTerminados.count
        }
        return candidato.objetivo.localizedCaseInsensitiveCompare(actual.objetivo)
            == .orderedAscending
    }

    private static func nivel(_ estudio: EstadoEstudioFrameIPad, ahora: Date) -> Int {
        if estudio.intentoActivo != nil || estudio.estado == .intentando { return 0 }

        let vencido = estudio.proximoRepaso.map { $0 <= ahora }
            ?? (estudio.estado == .repetir)
        if estudio.estado == .repetir && vencido { return 1 }
        if vencido { return 2 }
        if estudio.estado == .pendiente { return 3 }
        if estudio.estado == .repetir { return 4 }
        return 5
    }
}
