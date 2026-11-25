import Combine
import Foundation

/// Aprender la calculadora, no solo usarla (puntos 66, 67, 69, 70).
///
/// El problema que resuelve: la app puede teclear sola, y eso es cómodo hasta el día
/// del examen, cuando la calculadora no teclea sola. Si la app siempre hace el
/// trabajo, nunca aprendes dónde está `∫` y llegas al examen peor que si no la
/// hubieras usado.
///
/// Por eso hay dos modos que van en dirección contraria a la comodidad: entrenamiento
/// (te señala pero no pulsa) y examen (no señala nada y te cronometra).

/// Cómo se comporta la app respecto a ayudarte.
enum ModoAprendizaje: String, CaseIterable, Identifiable, Codable {
    /// Normal: la app puede teclear por ti.
    case asistido
    /// Te señala la tecla, pero la pulsas tú. Es lo que hace que se aprenda.
    case entrenamiento
    /// Sin ayudas y con cronómetro, como el examen de verdad.
    case examen

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .asistido: return "Asistido"
        case .entrenamiento: return "Entrenamiento"
        case .examen: return "Examen"
        }
    }

    var explicacion: String {
        switch self {
        case .asistido:
            return "La app puede teclear por ti. Cómodo, pero no enseña dónde están las teclas."
        case .entrenamiento:
            return "Te señala la tecla y la pulsas tú. Se tarda más y por eso se aprende."
        case .examen:
            return "Sin ayudas, sin teclear automático y con cronómetro. Como el día del examen."
        }
    }

    /// Si la app puede pulsar teclas ella misma.
    var permiteTecleoAutomatico: Bool { self == .asistido }

    /// Si se pueden señalar teclas sobre la carátula.
    var permiteSenalar: Bool { self != .examen }

    /// Si se enseñan las rutas de menú paso a paso.
    var permiteProcedimientos: Bool { self != .examen }
}

/// Una entrada del manual: qué es, para qué sirve y dónde está.
///
/// El manual se construye sobre los procedimientos ya verificados contra la ROM, en
/// vez de escribirse aparte. Un manual que se redacta a mano acaba diciendo lo que la
/// calculadora hacía hace tres versiones.
struct EntradaManual: Identifiable {
    var id: String { procedimiento.id }
    let procedimiento: Procedimiento
    /// Ruta en el modelo abierto, o nil si ese modelo no lo tiene.
    let ruta: RutaModelo?

    var disponible: Bool { ruta != nil }

    /// Cómo se lee la ruta: `MODE → 6 → 1`. Vacío si el modelo no lo trae.
    var comoSeLee: String { ruta?.comoSeLee ?? "" }
}

enum Manual {

    /// El manual entero para un modelo, ordenado como el catálogo.
    static func para(modelo: String) -> [EntradaManual] {
        ProcedimientosCasio.todos.map { procedimiento in
            EntradaManual(procedimiento: procedimiento,
                          ruta: procedimiento.ruta(en: modelo))
        }
    }

    /// Busca en el manual y devuelve los pasos del modelo abierto (punto 67).
    ///
    /// Devuelve también los que ese modelo NO tiene, marcados como no disponibles: es
    /// información útil —«la VN PLUS no trae hoja de cálculo»— y esconderlos haría
    /// pensar que la búsqueda falló.
    static func buscar(_ texto: String, modelo: String) -> [EntradaManual] {
        ProcedimientosCasio.buscar(texto).map { procedimiento in
            EntradaManual(procedimiento: procedimiento,
                          ruta: procedimiento.ruta(en: modelo))
        }
    }
}

/// Cronómetro y cuenta de ejercicios del modo examen (punto 70).
///
/// Lo que aporta no es la nota, que aquí no se puede saber: es el ritmo. Si en la
/// práctica tardas seis minutos por ejercicio y el examen da tres, eso hay que
/// descubrirlo antes del examen y no durante.
final class SesionExamen: ObservableObject {

    @Published private(set) var corriendo = false
    @Published private(set) var segundos: Double = 0
    @Published private(set) var ejerciciosHechos = 0
    /// Segundos que costó cada ejercicio, en orden.
    @Published private(set) var tiempos: [Double] = []

    private var segundosDelEjercicio: Double = 0

    func empezar() {
        corriendo = true
        segundos = 0
        segundosDelEjercicio = 0
        ejerciciosHechos = 0
        tiempos = []
    }

    func parar() {
        corriendo = false
    }

    /// Se llama desde el reloj que ya existe.
    func avanzar(_ delta: Double) {
        guard corriendo else { return }
        segundos += delta
        segundosDelEjercicio += delta
    }

    /// Cierra el ejercicio en curso y empieza a contar el siguiente.
    func siguienteEjercicio() {
        guard corriendo else { return }
        tiempos.append(segundosDelEjercicio)
        ejerciciosHechos += 1
        segundosDelEjercicio = 0
    }

    var mediaSegundos: Double? {
        guard !tiempos.isEmpty else { return nil }
        return tiempos.reduce(0, +) / Double(tiempos.count)
    }

    /// El más lento, que es el que conviene mirar: la media esconde el que se atascó.
    var peorSegundos: Double? { tiempos.max() }

    static func reloj(_ segundos: Double) -> String {
        let minutos = Int(segundos) / 60
        let resto = Int(segundos) % 60
        return String(format: "%d:%02d", minutos, resto)
    }

    /// Resumen para después, en texto plano.
    var resumen: String {
        guard !tiempos.isEmpty else { return "Sin ejercicios cronometrados todavía." }
        var lineas = ["\(ejerciciosHechos) ejercicios en \(Self.reloj(segundos))"]
        if let media = mediaSegundos {
            lineas.append("Media: \(Self.reloj(media)) por ejercicio")
        }
        if let peor = peorSegundos {
            lineas.append("El más lento: \(Self.reloj(peor))")
        }
        return lineas.joined(separator: "\n")
    }
}
