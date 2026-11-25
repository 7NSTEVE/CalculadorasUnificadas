import Foundation
import CoreGraphics

/// Motor para herramientas de física (dinámica, cinemática, electromagnetismo).
class MotorFisica {
    static let compartido = MotorFisica()
    
    enum TipoHerramientaFisica: String {
        case cuerpoLibre = "Diagrama de cuerpo libre"
        case resorte = "Resorte"
        case masa = "Masa"
        case polea = "Polea"
        case campo = "Campo magnético/eléctrico"
        case carga = "Carga puntual"
        case rayo = "Rayo de luz"
        case lente = "Lente óptica"
    }
    
    func insertar(_ tipo: TipoHerramientaFisica, posicion: CGPoint) {
        print("Insertando objeto físico: \(tipo.rawValue) en \(posicion)")
    }
    
    func calcularLeyHooke(k: Double, x: Double) -> Double {
        return -k * x
    }
    
    func calcularFuerzaElectrica(q1: Double, q2: Double, distancia: Double) -> Double {
        let k = 8.9875517923e9
        if distancia == 0 { return 0 }
        return k * (q1 * q2) / (distancia * distancia)
    }
}
