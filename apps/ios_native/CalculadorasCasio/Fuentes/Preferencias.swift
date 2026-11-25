import Combine
import Foundation

/// Ajustes de manejo (puntos 97, 100, 101, 102, 103, 107, 109, 110).
///
/// Todos son de la CAPA de interfaz. Ninguno toca la emulación: por debajo, la
/// máquina recibe exactamente los mismos códigos de tecla en el mismo orden, se toque
/// donde se toque. Es la regla dura del proyecto y aquí se cumple por construcción,
/// porque lo único que cambia es dónde se dibujan los botones.

/// Dónde va la barra de teclas rápidas.
enum LadoDominante: String, CaseIterable, Identifiable, Codable {
    case derecha, izquierda

    var id: String { rawValue }
    var nombre: String { self == .derecha ? "Diestro" : "Zurdo" }
}

/// Cómo se comporta SHIFT al pulsarlo.
enum ModoShift: String, CaseIterable, Identifiable, Codable {
    /// Como en la calculadora real: se pulsa y queda armado para la siguiente tecla.
    case pegajoso
    /// Se mantiene con el dedo, como la mayúscula de un teclado.
    ///
    /// No cambia lo que recibe la Casio: se le manda SHIFT y luego la tecla, igual
    /// que en el modo pegajoso. Lo único que cambia es cuándo se manda.
    case momentaneo

    var id: String { rawValue }
    var nombre: String { self == .pegajoso ? "Como la real" : "Mantener pulsado" }
}

/// Preferencias de manejo, persistidas.
final class Preferencias: ObservableObject {

    @Published var lado: LadoDominante { didSet { guardar() } }
    @Published var modoShift: ModoShift { didSet { guardar() } }
    /// Barra con las teclas que más se usan, para no buscar en toda la carátula.
    @Published var barraEsenciales: Bool { didSet { guardar() } }
    /// Teclado numérico grande superpuesto, para escribir números largos deprisa.
    @Published var tecladoGrande: Bool { didSet { guardar() } }
    @Published var sonidoTeclas: Bool { didSet { guardar() } }
    /// Vibración al armar SHIFT o ALPHA, que si no no se nota que están activos.
    @Published var hapticaModificadores: Bool { didSet { guardar() } }
    /// Deslizar hacia atrás para deshacer.
    @Published var gestoDeshacer: Bool { didSet { guardar() } }
    /// Zoom de la carátula. Las zonas táctiles escalan con ella, que es lo que evita
    /// que ampliar la imagen deje los toques donde estaban.
    @Published var zoom: Double { didSet { guardar() } }

    /// Teclas de la barra rápida, por nombre. Son las que se usan en casi todos los
    /// ejercicios de circuitos; el resto ya están en la carátula.
    static let esencialesPorDefecto = ["AC", "DEL", "IGUAL", "MAS", "MENOS",
                                       "POR", "ENTRE", "PAREN_ABRE", "PAREN_CIERRA"]

    private let almacen: UserDefaults

    init(almacen: UserDefaults = .standard) {
        self.almacen = almacen
        lado = LadoDominante(rawValue: almacen.string(forKey: "pref.lado") ?? "")
            ?? .derecha
        modoShift = ModoShift(rawValue: almacen.string(forKey: "pref.shift") ?? "")
            ?? .pegajoso
        barraEsenciales = almacen.object(forKey: "pref.esenciales") as? Bool ?? false
        tecladoGrande = almacen.object(forKey: "pref.tecladoGrande") as? Bool ?? false
        sonidoTeclas = almacen.object(forKey: "pref.sonido") as? Bool ?? false
        hapticaModificadores = almacen.object(forKey: "pref.hapticaMod") as? Bool ?? true
        gestoDeshacer = almacen.object(forKey: "pref.gestoDeshacer") as? Bool ?? true
        // 1.0 es la carátula a tamaño natural. Se acota al leer por si un fichero
        // viejo trae un valor absurdo que dejaría la calculadora invisible.
        let leido = almacen.object(forKey: "pref.zoom") as? Double ?? 1.0
        zoom = min(max(leido, 1.0), 2.5)
    }

    private func guardar() {
        almacen.set(lado.rawValue, forKey: "pref.lado")
        almacen.set(modoShift.rawValue, forKey: "pref.shift")
        almacen.set(barraEsenciales, forKey: "pref.esenciales")
        almacen.set(tecladoGrande, forKey: "pref.tecladoGrande")
        almacen.set(sonidoTeclas, forKey: "pref.sonido")
        almacen.set(hapticaModificadores, forKey: "pref.hapticaMod")
        almacen.set(gestoDeshacer, forKey: "pref.gestoDeshacer")
        almacen.set(zoom, forKey: "pref.zoom")
    }

    /// Zoom acotado. Por debajo de 1 la carátula dejaría hueco y por encima de 2,5
    /// no cabe ni una fila de teclas en pantalla.
    func ajustarZoom(_ nuevo: Double) {
        zoom = min(max(nuevo, 1.0), 2.5)
    }

    func limpiar() {
        for clave in ["pref.lado", "pref.shift", "pref.esenciales", "pref.tecladoGrande",
                      "pref.sonido", "pref.hapticaMod", "pref.gestoDeshacer", "pref.zoom"] {
            almacen.removeObject(forKey: clave)
        }
        lado = .derecha
        modoShift = .pegajoso
        barraEsenciales = false
        tecladoGrande = false
        sonidoTeclas = false
        hapticaModificadores = true
        gestoDeshacer = true
        zoom = 1.0
    }
}
