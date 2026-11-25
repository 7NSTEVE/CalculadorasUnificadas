import Foundation

extension Sesion {
    /// Ejecuta varias llamadas mutantes del núcleo como una sola entrada de
    /// Undo. Si el cuerpo falla, devuelve `false` o lanza, el núcleo restaura
    /// página + pilas Undo/Redo exactamente al estado anterior.
    ///
    /// Se usa para comandos compuestos (paste/import), no para el camino de
    /// lápiz del iPad: la transacción copia una página una vez al comenzar.
    @discardableResult
    func conTransaccionHistorial(
        _ cuerpo: () throws -> Bool
    ) rethrows -> Bool {
        guard let transaccion = tinta_historial_transaccion_comenzar(puntero)
        else { return false }
        var consumida = false
        defer {
            if !consumida {
                tinta_historial_transaccion_cancelar(transaccion)
            }
        }

        let exito = try cuerpo()
        guard exito else {
            tinta_historial_transaccion_cancelar(transaccion)
            consumida = true
            return false
        }

        let confirmada = tinta_historial_transaccion_confirmar(transaccion)
        consumida = true
        return confirmada
    }
}
