import Foundation

/// Reloj monotónico puro para Nota Viva. Se mantiene separado de AVFoundation
/// para poder probar pausas largas, interrupciones y reanudaciones en Windows
/// sin fingir que eso valida el hardware del iPad.
struct RelojNotaViva {
    enum Estado {
        case detenido
        case grabando
        case pausado
    }

    private(set) var estado: Estado = .detenido
    private(set) var acumulado = 0.0
    private var inicioTramo = 0.0

    func segundo(en monotono: Double) -> Double {
        guard estado == .grabando else { return acumulado }
        return acumulado + max(0, monotono - inicioTramo)
    }

    @discardableResult
    mutating func comenzar(en monotono: Double) -> Bool {
        guard estado == .detenido else { return false }
        acumulado = 0
        inicioTramo = monotono
        estado = .grabando
        return true
    }

    mutating func pausar(en monotono: Double) -> Double? {
        guard estado == .grabando else { return nil }
        acumulado = segundo(en: monotono)
        estado = .pausado
        return acumulado
    }

    @discardableResult
    mutating func reanudar(en monotono: Double) -> Bool {
        guard estado == .pausado else { return false }
        inicioTramo = monotono
        estado = .grabando
        return true
    }

    mutating func detener(en monotono: Double) -> Double {
        if estado == .grabando {
            acumulado = segundo(en: monotono)
        }
        estado = .detenido
        return acumulado
    }

    mutating func reiniciar() {
        estado = .detenido
        acumulado = 0
        inicioTramo = 0
    }
}
