import Foundation

enum EstadoTermicoComparativaIPad: String, Codable, CaseIterable {
    case nominal
    case fair
    case serious
    case critical
    case unknown

    var aceptableParaComparacion: Bool {
        self == .nominal || self == .fair
    }
}

struct CondicionesCorridaGoodnotesIPad: Codable, Equatable {
    var dispositivo: String
    var sistema: String
    var refrescoHz: Int
    var estadoTermico: EstadoTermicoComparativaIPad
    var pencilFisico: Bool
    var sha256Corpus: String?

    var claveHardware: String {
        "\(dispositivo)|\(sistema)|\(refrescoHz)Hz"
    }
}

enum ErrorIngestaComparativaGoodnotesIPad: LocalizedError, Equatable {
    case escenarioDesconocido(String)
    case refrescoInsuficiente(Int)
    case faltaPencilFisico
    case estadoTermicoNoComparable(String)
    case faltaCorpus
    case corpusInvalido
    case valorInvalido

    var errorDescription: String? {
        switch self {
        case .escenarioDesconocido(let id):
            return "El escenario QA no existe o usa otra versión: \(id)."
        case .refrescoInsuficiente(let hz):
            return "El escenario exige 120 Hz y la corrida reporta \(hz) Hz."
        case .faltaPencilFisico:
            return "El escenario exige una corrida con lápiz del iPad físico."
        case .estadoTermicoNoComparable(let valor):
            return "La corrida no es comparable con thermal \(valor). Use nominal o fair."
        case .faltaCorpus:
            return "El escenario exige el SHA-256 del corpus compartido."
        case .corpusInvalido:
            return "El SHA-256 del corpus no tiene formato válido."
        case .valorInvalido:
            return "La muestra debe ser finita y no negativa."
        }
    }
}

enum IngestaComparativaGoodnotesIPad {
    static func requiereCorpus(_ escenario: EscenarioComparativaGoodnotesIPad) -> Bool {
        switch escenario.id {
        case CatalogoEscenariosGoodnotesIPad.huge500ScrollJank.id,
             CatalogoEscenariosGoodnotesIPad.huge500Reopen.id,
             CatalogoEscenariosGoodnotesIPad.export500PDF.id:
            return true
        default:
            return false
        }
    }

    static func validar(
        escenario: EscenarioComparativaGoodnotesIPad,
        condiciones: CondicionesCorridaGoodnotesIPad,
        valor: Double
    ) throws {
        guard valor.isFinite, valor >= 0 else {
            throw ErrorIngestaComparativaGoodnotesIPad.valorInvalido
        }
        guard condiciones.estadoTermico.aceptableParaComparacion else {
            throw ErrorIngestaComparativaGoodnotesIPad.estadoTermicoNoComparable(
                condiciones.estadoTermico.rawValue)
        }
        if escenario.requiere120Hz, condiciones.refrescoHz < 119 {
            throw ErrorIngestaComparativaGoodnotesIPad.refrescoInsuficiente(
                condiciones.refrescoHz)
        }
        if escenario.requierePencilFisico, !condiciones.pencilFisico {
            throw ErrorIngestaComparativaGoodnotesIPad.faltaPencilFisico
        }
        if requiereCorpus(escenario) {
            guard let sha = condiciones.sha256Corpus else {
                throw ErrorIngestaComparativaGoodnotesIPad.faltaCorpus
            }
            guard shaValido(sha) else {
                throw ErrorIngestaComparativaGoodnotesIPad.corpusInvalido
            }
        } else if let sha = condiciones.sha256Corpus, !shaValido(sha) {
            throw ErrorIngestaComparativaGoodnotesIPad.corpusInvalido
        }
    }

    static func nombreEscenarioPersistente(
        _ escenario: EscenarioComparativaGoodnotesIPad,
        condiciones: CondicionesCorridaGoodnotesIPad
    ) -> String {
        let corpus = condiciones.sha256Corpus.map {
            "|corpus=\(String($0.lowercased().prefix(16)))"
        } ?? ""
        return "\(escenario.idVersionado)|\(condiciones.refrescoHz)Hz\(corpus)"
    }

    private static func shaValido(_ valor: String) -> Bool {
        valor.count == 64 && valor.unicodeScalars.allSatisfy { escalar in
            switch escalar.value {
            case 48...57, 65...70, 97...102: return true
            default: return false
            }
        }
    }
}

extension RepositorioComparativaGoodnotesIPad {
    @discardableResult
    func registrarMuestraCanonica(
        producto: ProductoMuestraComparativaIPad,
        escenarioIdVersionado: String,
        condiciones: CondicionesCorridaGoodnotesIPad,
        valor: Double,
        fecha: Date = Date(),
        idMuestra: String? = nil
    ) throws -> SerieComparativaRendimientoIPad {
        guard let escenario = CatalogoEscenariosGoodnotesIPad.escenario(
            idVersionado: escenarioIdVersionado) else {
            throw ErrorIngestaComparativaGoodnotesIPad.escenarioDesconocido(
                escenarioIdVersionado)
        }
        try IngestaComparativaGoodnotesIPad.validar(
            escenario: escenario,
            condiciones: condiciones,
            valor: valor)
        return try registrarMuestra(
            producto: producto,
            metrica: escenario.metrica,
            escenario: IngestaComparativaGoodnotesIPad.nombreEscenarioPersistente(
                escenario, condiciones: condiciones),
            dispositivo: condiciones.dispositivo,
            sistema: condiciones.sistema,
            direccion: escenario.direccion,
            valor: valor,
            fecha: fecha,
            idMuestra: idMuestra)
    }
}
