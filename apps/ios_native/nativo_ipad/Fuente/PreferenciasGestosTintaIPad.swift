import Foundation

struct PreferenciasGestosTintaIPad: Codable, Equatable {
    var circleToLasso = true
    var scribbleToErase = true

    private static let clave = "CuadernU.PreferenciasGestosTinta.v1"

    static var actual: PreferenciasGestosTintaIPad {
        get {
            guard let datos = UserDefaults.standard.data(forKey: clave),
                  let valor = try? JSONDecoder().decode(Self.self, from: datos)
            else { return Self() }
            return valor
        }
        set {
            guard let datos = try? JSONEncoder().encode(newValue) else { return }
            UserDefaults.standard.set(datos, forKey: clave)
            NotificationCenter.default.post(
                name: .preferenciasGestosTintaCambioIPad,
                object: newValue)
        }
    }

    static func restaurar() {
        actual = Self()
    }
}

extension Notification.Name {
    static let preferenciasGestosTintaCambioIPad = Notification.Name(
        "CuadernU.PreferenciasGestosTintaCambio")
}
