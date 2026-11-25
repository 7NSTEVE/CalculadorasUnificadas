import Foundation

/// A que ritmo se teclea una expresion enviada desde fuera.
///
/// Instantaneo es lo practico. Las velocidades lentas existen por otro motivo: ver
/// donde estan fisicamente las teclas. Si la app siempre teclea sola, uno nunca
/// aprende a hacerlo en la calculadora de verdad, que es la que hay en el examen.
enum VelocidadEnvio: Double, CaseIterable, Identifiable, Codable {
    case instantaneo = 0
    case cuarto = 0.25
    case media = 0.5
    case normal = 1
    case doble = 2

    var id: Double { rawValue }

    var animada: Bool { self != .instantaneo }

    var etiqueta: String {
        switch self {
        case .instantaneo: return "Instantáneo"
        case .cuarto: return "0,25×"
        case .media: return "0,5×"
        case .normal: return "1×"
        case .doble: return "2×"
        }
    }

    /// Cuanto dura cada tecla en pantalla. 450 ms es aproximadamente lo que tarda
    /// una persona en encadenar pulsaciones sin ir a la carrera.
    var segundosPorTecla: Double {
        rawValue == 0 ? 0 : 0.45 / rawValue
    }
}
