import Combine
import Foundation

/// Lo que ayuda a no perder el hilo (puntos 19, 20, 21, 23, 25, 30).
///
/// Nada de aquí bloquea la calculadora ni roba el foco. Esa es la regla: una app que
/// te interrumpe para decirte que te estás distrayendo es otra distracción más.
/// Todo son avisos pasivos que se pueden ignorar sin consecuencias.

/// Pasos del ejercicio, para enseñar pocos a la vez.
///
/// Punto 19: como mucho tres visibles. Con la lista entera delante se lee todo y no
/// se hace nada, que es exactamente el fallo que se intenta evitar.
struct GuionEjercicio: Equatable {
    var pasos: [String] = []
    /// En cuál se está. Los anteriores se dan por hechos.
    var actual: Int = 0

    /// Solo el que toca (punto 20).
    var soloSiguiente: String? {
        pasos.indices.contains(actual) ? pasos[actual] : nil
    }

    /// Como mucho tres: el actual y los dos siguientes.
    ///
    /// No se enseña ninguno anterior: lo hecho ya está en la cinta, y repetirlo aquí
    /// solo gasta el sitio que necesita lo que falta.
    func visibles(maximo: Int = 3) -> [(indice: Int, texto: String)] {
        guard !pasos.isEmpty else { return [] }
        let desde = min(actual, pasos.count - 1)
        let hasta = min(desde + maximo, pasos.count)
        return (desde..<hasta).map { ($0, pasos[$0]) }
    }

    var terminado: Bool { actual >= pasos.count && !pasos.isEmpty }

    mutating func avanzar() {
        if actual < pasos.count { actual += 1 }
    }

    mutating func retroceder() {
        if actual > 0 { actual -= 1 }
    }
}

/// Vigila si el ejercicio se quedó a medias (puntos 21 y 23).
///
/// No mide "concentración", que no se puede medir desde aquí. Mide algo concreto y
/// comprobable: cuánto hace que no llega una tecla. Si llevas diez minutos sin tocar
/// nada con un objetivo escrito a medias, probablemente te fuiste.
final class VigilanteAtencion: ObservableObject {

    /// Segundos sin teclear tras los cuales se considera abandonado el ejercicio.
    ///
    /// Cinco minutos: menos daría falsos positivos cada vez que te paras a pensar,
    /// que en un ejercicio de circuitos es la mitad del tiempo.
    static let segundosAbandono: Double = 300

    /// Temporizador opcional de sesión. Cero lo desactiva.
    @Published var minutosObjetivo: Double = 0

    @Published private(set) var segundosSinTeclear: Double = 0
    @Published private(set) var segundosDeSesion: Double = 0
    @Published private(set) var pareceAbandonado = false
    /// Cierto una vez cumplido el objetivo de tiempo, hasta que se reinicie.
    @Published private(set) var objetivoCumplido = false

    /// Se llama desde el reloj que ya existe, con los segundos transcurridos.
    func avanzar(segundos: Double, hayEjercicioEnCurso: Bool) {
        segundosDeSesion += segundos
        segundosSinTeclear += segundos

        pareceAbandonado = hayEjercicioEnCurso
            && segundosSinTeclear >= Self.segundosAbandono

        if minutosObjetivo > 0, segundosDeSesion >= minutosObjetivo * 60 {
            objetivoCumplido = true
        }
    }

    func registrarTecla() {
        segundosSinTeclear = 0
        pareceAbandonado = false
    }

    func reiniciarSesion() {
        segundosDeSesion = 0
        segundosSinTeclear = 0
        objetivoCumplido = false
        pareceAbandonado = false
    }

    /// Texto del temporizador, o nil si está apagado.
    var textoTemporizador: String? {
        guard minutosObjetivo > 0 else { return nil }
        let restante = max(0, minutosObjetivo * 60 - segundosDeSesion)
        let minutos = Int(restante) / 60
        let segundos = Int(restante) % 60
        return String(format: "%d:%02d", minutos, segundos)
    }
}

/// Detecta que la calculadora cambió de modo angular sin que te dieras cuenta.
///
/// Punto 30, y es el error más caro que existe en un examen de trigonometría: haces
/// veinte ejercicios en RAD cuando el enunciado estaba en grados y todos salen mal
/// por el mismo motivo. La Casio lo dice en una esquina del LCD, en letra diminuta.
final class VigilanteIndicadores: ObservableObject {

    /// Modos angulares tal y como los escribe el LCD.
    static let modosAngulares: Set<String> = ["DEG", "RAD", "GRA", "GRAD"]

    @Published private(set) var modoActual: String?
    /// Se enciende cuando el modo cambia sin pasar por el menú de configuración.
    @Published private(set) var cambioSospechoso: String?

    /// Última vez que el usuario entró a SETUP a propósito. Un cambio dentro de los
    /// segundos siguientes es intencionado y no se avisa.
    private var instanteSetup: Date?
    private static let margenSetupSegundos: Double = 20

    func registrarEntradaASetup() {
        instanteSetup = Date()
    }

    /// Se llama con los indicadores leídos del LCD.
    func observar(indicadores: [String]) {
        guard let nuevo = indicadores.first(where: { Self.modosAngulares.contains($0) })
        else { return }

        defer { modoActual = nuevo }
        guard let anterior = modoActual, anterior != nuevo else { return }

        // Si acabas de entrar en SETUP, el cambio lo hiciste tú.
        if let instanteSetup,
           Date().timeIntervalSince(instanteSetup) < Self.margenSetupSegundos {
            return
        }
        cambioSospechoso = "La calculadora pasó de \(anterior) a \(nuevo)."
    }

    func descartarAviso() {
        cambioSospechoso = nil
    }
}

/// Registro de teclas, para explicarle a alguien lo que estás haciendo.
///
/// Puntos 133, 134 y 135. En clase, cuando alguien te pregunta «¿qué apretaste?»,
/// la respuesta útil no es el resultado sino la secuencia.
final class RegistroTeclas: ObservableObject {

    struct Pulsacion: Identifiable, Equatable {
        let id = UUID()
        let nombre: String
        let momento: Date
    }

    @Published private(set) var recientes: [Pulsacion] = []
    /// Si se enseña el rótulo grande. Se puede apagar (punto 135), porque en un examen
    /// tener las teclas en pantalla llama la atención.
    @Published var visible = false
    /// Cuántas se ven a la vez en la tira.
    @Published var cuantasVisibles = 8

    private static let maximo = 40

    func registrar(_ nombre: String) {
        recientes.append(Pulsacion(nombre: nombre, momento: Date()))
        if recientes.count > Self.maximo { recientes.removeFirst() }
    }

    /// Las últimas, de izquierda a derecha como se leen.
    var tira: [Pulsacion] {
        Array(recientes.suffix(cuantasVisibles))
    }

    /// La secuencia en texto, para pegarla en un mensaje.
    var texto: String {
        recientes.map(\.nombre).joined(separator: " ")
    }

    func limpiar() {
        recientes = []
    }
}
