import Foundation

/// Preferencias pequeñas y estables de captura. Los UID pueden dejar de estar
/// disponibles cuando se desconecta un accesorio; Microfonos resuelve entonces
/// una alternativa sin borrar lo que la persona había elegido.
enum PreferenciasCaptura {
    private static let claveMicrofono = "captura.microfono.seleccion.v1"

    static var microfono: SeleccionMicrofono? {
        get {
            guard let datos = UserDefaults.standard.data(forKey: claveMicrofono) else {
                return nil
            }
            return try? JSONDecoder().decode(SeleccionMicrofono.self, from: datos)
        }
        set {
            if let nuevo = newValue, let datos = try? JSONEncoder().encode(nuevo) {
                UserDefaults.standard.set(datos, forKey: claveMicrofono)
            } else {
                UserDefaults.standard.removeObject(forKey: claveMicrofono)
            }
        }
    }
}
