import Foundation
import CoreGraphics

protocol DelegadoInteraccionMotor: AnyObject {
    func solicitarTexto(titulo: String, mensaje: String, placeholder: String, completado: @escaping (String?) -> Void)
    func mostrarMensaje(_ mensaje: String)
}

class MotorCircuito {
    static let compartido = MotorCircuito()
    weak var delegado: DelegadoInteraccionMotor?
    
    // NOTA: TipoComponenteElectrico y EsquemaEditable se asumen definidos en el entorno (CircuitoEditable.swift)
    
    func insertar(_ tipo: TipoComponenteElectrico, en esquema: EsquemaEditable, posicion: CGPoint, alTerminar: @escaping (EsquemaEditable) -> Void) {
        guard tipo.requiereValor else {
            var modificado = esquema
            modificado.agregar(tipo, posicion: posicion)
            alTerminar(modificado)
            return
        }
        
        delegado?.solicitarTexto(titulo: tipo.nombre, mensaje: "Escriba un valor confirmado. Acepta 10k, 4u7, 2.2Meg y unidades; cancelar no crea el componente.", placeholder: "Valor sin inventar") { [weak self] texto in
            guard let texto = texto, let valor = NotacionIngenieria.interpretar(texto), valor > 0 else {
                self?.delegado?.mostrarMensaje("No se creó: el valor no es válido o no fue confirmado.")
                return
            }
            var modificado = esquema
            modificado.agregar(tipo, posicion: posicion)
            modificado.componentes[modificado.componentes.count - 1].valor = valor
            alTerminar(modificado)
        }
    }
}
