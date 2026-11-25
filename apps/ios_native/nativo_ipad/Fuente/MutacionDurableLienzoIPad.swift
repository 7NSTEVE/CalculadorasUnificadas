import Foundation

/// Canal interno para avisar que una operación de geometría modificó el
/// documento y por tanto debe entrar al mismo debounce de autoguardado que la
/// tinta. Se identifica por el handle efímero de la sesión nativa, no solo por
/// el UUID durable: el mismo cuaderno puede estar abierto en más de un viewport
/// y una mutación no debe despertar coordinadores de otra sesión.
enum CanalMutacionDurableLienzoIPad {
    static let nombre = Notification.Name("CuadernU.mutacionDurableLienzo")
    static let claveManejadorNucleo = "manejadorNucleo"

    static func publicar(
        manejadorNucleo: UInt64,
        centro: NotificationCenter = .default
    ) {
        centro.post(
            name: nombre,
            object: nil,
            userInfo: [claveManejadorNucleo: NSNumber(value: manejadorNucleo)])
    }

    static func manejadorNucleo(de notificacion: Notification) -> UInt64? {
        (notificacion.userInfo?[claveManejadorNucleo] as? NSNumber)?.uint64Value
    }
}

/// Coalesce una interacción continua (por ejemplo arrastrar un borde) en una
/// sola mutación durable al terminar el gesto. El núcleo puede recibir decenas
/// de deltas visuales, pero el autoguardado solo necesita una generación nueva.
struct AcumuladorMutacionDurableLienzoIPad {
    private(set) var huboCambio = false

    mutating func comenzar() {
        huboCambio = false
    }

    mutating func registrarCambio() {
        huboCambio = true
    }

    mutating func terminar() -> Bool {
        let resultado = huboCambio
        huboCambio = false
        return resultado
    }
}
