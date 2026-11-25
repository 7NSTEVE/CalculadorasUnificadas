import Combine
// `Foundation` ya trae CGRect en iOS y en el Swift de Windows; importar
// CoreGraphics a secas rompe el banco de pruebas, donde ese modulo no existe.
import Foundation

/// Lo que hace que teclear en un cristal no sea peor que teclear en la calculadora.
///
/// El problema real: en la Casio física los dedos aciertan porque las teclas tienen
/// relieve y tope. En un cristal no, así que se falla más — y siempre en las mismas
/// teclas. Esto lo mide y lo corrige donde duele, en vez de agrandarlo todo, que
/// haría la carátula menos parecida a la de verdad.
///
/// Regla que no se toca: la carátula se dibuja igual. Lo que crece es la zona
/// SENSIBLE, que es invisible.
enum Ergonomia {

    /// Cuánto se agranda la zona táctil por encima de la tecla dibujada.
    ///
    /// 44 pt es el mínimo que recomienda iPadOS para un objetivo táctil. Muchas teclas
    /// de la ClassWiz, escaladas a un iPhone, quedan por debajo, y ahí es donde se
    /// falla. Se agranda hasta ese mínimo, nunca por debajo del tamaño dibujado.
    static let objetivoMinimoPuntos: CGFloat = 44

    /// La zona sensible de una tecla, dado su rectángulo dibujado.
    ///
    /// `extra` sale del contador de errores: las teclas que se fallan mucho crecen un
    /// poco más. Es el punto 113, y depende de haber medido antes (111 y 112).
    static func zonaSensible(_ dibujada: CGRect, extra: CGFloat = 0) -> CGRect {
        let anchoObjetivo = max(dibujada.width, objetivoMinimoPuntos) + extra
        let altoObjetivo = max(dibujada.height, objetivoMinimoPuntos) + extra
        return CGRect(x: dibujada.midX - anchoObjetivo / 2,
                      y: dibujada.midY - altoObjetivo / 2,
                      width: anchoObjetivo, height: altoObjetivo)
    }

    /// Dos toques en la misma tecla más rápidos que esto son un rebote del dedo.
    ///
    /// 120 ms sale de lo que tarda un dedo en volver a bajar sin querer. Por debajo
    /// de eso nadie pulsa dos veces a propósito; por encima, sí (repetir un dígito).
    static let msRebote: Double = 120

    /// Si un toque hay que ignorarlo por ser rebote del anterior.
    ///
    /// Solo cuenta como rebote si es la MISMA tecla: pulsar `1` y luego `2` muy
    /// rápido es escribir deprisa, no un accidente.
    static func esRebote(codigo: UInt8, anterior: (codigo: UInt8, instante: Double)?,
                         ahora: Double) -> Bool {
        guard let anterior, anterior.codigo == codigo else { return false }
        return (ahora - anterior.instante) * 1000 < msRebote
    }
}

/// Cuenta dónde se falla al teclear, para poder corregirlo.
///
/// Qué se considera un fallo: pulsar una tecla y borrarla acto seguido con `DEL`.
/// No es una medida perfecta —a veces se borra porque cambiaste de idea— pero es la
/// única señal honesta que hay sin preguntar, y con volumen suficiente el ruido se
/// diluye y las teclas problemáticas destacan igual.
final class ContadorErrores: ObservableObject {

    /// Código de tecla -> veces que se borró justo después de pulsarla.
    @Published private(set) var fallos: [UInt8: Int] = [:]
    /// Código -> veces pulsada. Sin esto, la tecla más usada parecería la peor.
    @Published private(set) var pulsaciones: [UInt8: Int] = [:]

    private let clave = "ergonomia.fallos"
    private let clavePulsaciones = "ergonomia.pulsaciones"
    private let almacen: UserDefaults

    /// Últimas teclas pulsadas, para saber cuál borró el `DEL`.
    private var ultima: UInt8?

    init(almacen: UserDefaults = .standard) {
        self.almacen = almacen
        fallos = Self.leer(almacen, clave)
        pulsaciones = Self.leer(almacen, clavePulsaciones)
    }

    private static func leer(_ almacen: UserDefaults, _ clave: String) -> [UInt8: Int] {
        guard let crudo = almacen.dictionary(forKey: clave) as? [String: Int] else {
            return [:]
        }
        // Las claves de UserDefaults son String; se guardan como el número decimal.
        return crudo.reduce(into: [:]) { salida, par in
            if let codigo = UInt8(par.key) { salida[codigo] = par.value }
        }
    }

    private func escribir(_ tabla: [UInt8: Int], _ clave: String) {
        let crudo = tabla.reduce(into: [String: Int]()) { salida, par in
            salida[String(par.key)] = par.value
        }
        almacen.set(crudo, forKey: clave)
    }

    /// Se llama con cada tecla. `codigoDel` es el código de `DEL` en este modelo.
    func registrar(_ codigo: UInt8, codigoDel: UInt8) {
        pulsaciones[codigo, default: 0] += 1
        escribir(pulsaciones, clavePulsaciones)

        if codigo == codigoDel, let anterior = ultima, anterior != codigoDel {
            fallos[anterior, default: 0] += 1
            escribir(fallos, clave)
        }
        ultima = codigo
    }

    /// Proporción de veces que una tecla acabó borrada.
    ///
    /// Se pide un mínimo de pulsaciones antes de dar un número: con tres intentos,
    /// un solo borrado daría un 33% que no significa nada.
    func tasaDeFallo(_ codigo: UInt8, minimo: Int = 10) -> Double? {
        let veces = pulsaciones[codigo] ?? 0
        guard veces >= minimo else { return nil }
        return Double(fallos[codigo] ?? 0) / Double(veces)
    }

    /// Cuánto agrandar la zona sensible de una tecla, en puntos.
    ///
    /// Se limita a 10 pt: más que eso empieza a robarle sitio a las vecinas, y
    /// entonces se falla en la de al lado. Cambiar un problema por otro no es
    /// arreglarlo.
    func margenExtra(_ codigo: UInt8) -> CGFloat {
        guard let tasa = tasaDeFallo(codigo) else { return 0 }
        guard tasa > 0.08 else { return 0 }
        return min(10, CGFloat((tasa - 0.08) * 100))
    }

    /// Las teclas que más se fallan, para el mapa del punto 112.
    func peores(cuantas: Int = 5) -> [(codigo: UInt8, tasa: Double)] {
        pulsaciones.keys
            .compactMap { codigo in tasaDeFallo(codigo).map { (codigo, $0) } }
            .filter { $0.1 > 0 }
            .sorted { $0.1 > $1.1 }
            .prefix(cuantas)
            .map { (codigo: $0.0, tasa: $0.1) }
    }

    func limpiar() {
        fallos = [:]; pulsaciones = [:]; ultima = nil
        almacen.removeObject(forKey: clave)
        almacen.removeObject(forKey: clavePulsaciones)
    }
}
