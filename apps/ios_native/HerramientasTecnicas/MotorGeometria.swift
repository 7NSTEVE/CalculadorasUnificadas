import Foundation
import CoreGraphics

/// Motor para herramientas de geometría.
class MotorGeometria {
    static let compartido = MotorGeometria()
    
    enum HerramientaGeometria: String {
        case regla = "Regla"
        case compas = "Compás"
        case escuadra = "Escuadra"
        case transportador = "Transportador"
    }
    
    func insertar(_ herramienta: HerramientaGeometria, posicion: CGPoint) {
        // Lógica del motor para manejar geometría independientemente de la UI.
        print("Geometría insertada: \(herramienta.rawValue) en \(posicion)")
    }
    
    func calcularInterseccion(p1: CGPoint, p2: CGPoint, p3: CGPoint, p4: CGPoint) -> CGPoint? {
        // Ejemplo de lógica pura del motor
        let divisor = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x)
        if divisor == 0 { return nil }
        let t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / divisor
        return CGPoint(x: p1.x + t * (p2.x - p1.x), y: p1.y + t * (p2.y - p1.y))
    }
}
