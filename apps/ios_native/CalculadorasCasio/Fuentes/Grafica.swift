import Foundation

/// Análisis de una curva a partir de puntos **muestreados de la propia Casio**.
///
/// Decisión importante y deliberada: aquí no hay motor matemático. Este proyecto
/// decidió que quien calcula es la calculadora emulada, y un graficador con su propio
/// evaluador rompería eso: la gráfica y el resultado de `=` podrían no coincidir, y
/// entonces habría dos verdades.
///
/// A cambio, muestrear cuesta. Cada punto es teclear el valor de `x` y leer el LCD:
/// unos 100 ms. Una curva de 50 puntos son ~5 s. Es lento y es honesto, y para un
/// botón que se pulsa una vez por ejercicio es un precio razonable.
///
/// Lo de este fichero es lo que se hace DESPUÉS de tener los puntos: encontrar
/// raíces, extremos e intersecciones. Todo por interpolación entre muestras, así que
/// la precisión depende de cuántas haya, y eso se dice en vez de disimularlo.

/// Un punto de la curva. `y` es nil donde la calculadora dio error (dividir entre
/// cero, raíz de negativo): esos huecos son información, no fallos que ocultar.
struct MuestraCurva: Equatable {
    let x: Double
    let y: Double?
}

/// Algo señalado sobre la curva.
struct PuntoNotable: Identifiable, Equatable {
    var id: String { "\(clase.rawValue)@\(x)" }
    let x: Double
    let y: Double
    let clase: Clase

    enum Clase: String {
        case raiz = "corte con X"
        case maximo = "máximo"
        case minimo = "mínimo"
        case interseccion = "intersección"
    }
}

enum Grafica {

    /// Los valores de `x` en los que se va a preguntar a la calculadora.
    ///
    /// Se reparten uniformemente porque no se sabe nada de la función de antemano:
    /// concentrar muestras donde "parece interesante" pediría evaluarla primero, que
    /// es justo lo caro.
    static func puntosDeMuestreo(desde: Double, hasta: Double, cantidad: Int) -> [Double] {
        guard cantidad >= 2, hasta > desde else { return [] }
        let paso = (hasta - desde) / Double(cantidad - 1)
        return (0..<cantidad).map { desde + Double($0) * paso }
    }

    /// Raíces: donde la curva cruza el eje X.
    ///
    /// Se detecta el cambio de signo entre dos muestras y se interpola linealmente.
    /// Eso encuentra los cruces, pero **no** las raíces dobles que solo tocan el eje
    /// sin cruzarlo; decir lo contrario sería prometer de más.
    static func raices(_ muestras: [MuestraCurva]) -> [PuntoNotable] {
        var encontradas: [PuntoNotable] = []
        for (anterior, actual) in zip(muestras, muestras.dropFirst()) {
            guard let y0 = anterior.y, let y1 = actual.y else { continue }

            if y0 == 0 {
                encontradas.append(PuntoNotable(x: anterior.x, y: 0, clase: .raiz))
                continue
            }
            guard (y0 < 0) != (y1 < 0), y1 != 0 else { continue }

            // Interpolación lineal al cruce. y1 - y0 no puede ser cero aquí porque
            // tienen signos distintos.
            let t = y0 / (y0 - y1)
            let x = anterior.x + t * (actual.x - anterior.x)
            encontradas.append(PuntoNotable(x: x, y: 0, clase: .raiz))
        }
        // La última muestra exactamente en cero se escapa del bucle de pares.
        if let ultima = muestras.last, ultima.y == 0,
           !encontradas.contains(where: { $0.x == ultima.x }) {
            encontradas.append(PuntoNotable(x: ultima.x, y: 0, clase: .raiz))
        }
        return encontradas
    }

    /// Máximos y mínimos locales, mirando cada muestra contra sus vecinas.
    ///
    /// Son los extremos **de las muestras**, no los de la función: si el pico real cae
    /// entre dos puntos, lo que se marca es el punto más alto que se llegó a mirar.
    /// Con más muestras se afina, y por eso la vista dice cuántas se usaron.
    static func extremos(_ muestras: [MuestraCurva]) -> [PuntoNotable] {
        guard muestras.count >= 3 else { return [] }
        var encontrados: [PuntoNotable] = []

        for indice in 1..<(muestras.count - 1) {
            guard let anterior = muestras[indice - 1].y,
                  let actual = muestras[indice].y,
                  let siguiente = muestras[indice + 1].y else { continue }

            if actual > anterior && actual > siguiente {
                encontrados.append(PuntoNotable(x: muestras[indice].x, y: actual,
                                                clase: .maximo))
            } else if actual < anterior && actual < siguiente {
                encontrados.append(PuntoNotable(x: muestras[indice].x, y: actual,
                                                clase: .minimo))
            }
        }
        return encontrados
    }

    /// Dónde se cruzan dos curvas muestreadas en los mismos `x`.
    ///
    /// Es el mismo truco que las raíces aplicado a la diferencia. Sirve para el caso
    /// de circuitos de verdad: dónde la recta de carga corta la curva del diodo.
    static func intersecciones(_ primera: [MuestraCurva],
                               _ segunda: [MuestraCurva]) -> [PuntoNotable] {
        guard primera.count == segunda.count else { return [] }

        let diferencia = zip(primera, segunda).map { uno, otro -> MuestraCurva in
            guard let a = uno.y, let b = otro.y, uno.x == otro.x else {
                return MuestraCurva(x: uno.x, y: nil)
            }
            return MuestraCurva(x: uno.x, y: a - b)
        }

        return raices(diferencia).map { cruce in
            // La altura del cruce se interpola sobre la primera curva, para poder
            // enseñar el par (x, y) completo y no solo la x.
            PuntoNotable(x: cruce.x, y: interpolar(primera, en: cruce.x) ?? 0,
                         clase: .interseccion)
        }
    }

    /// Valor de la curva en un `x` cualquiera, interpolando entre muestras.
    ///
    /// Lo usa el cursor que se arrastra sobre la gráfica: pedirle un punto nuevo a la
    /// calculadora en cada movimiento del dedo costaría 100 ms y se sentiría atascado.
    static func interpolar(_ muestras: [MuestraCurva], en x: Double) -> Double? {
        guard muestras.count >= 2 else { return muestras.first?.y }
        guard let primera = muestras.first, let ultima = muestras.last,
              x >= primera.x, x <= ultima.x else { return nil }

        for (anterior, actual) in zip(muestras, muestras.dropFirst()) {
            guard x >= anterior.x, x <= actual.x else { continue }
            guard let y0 = anterior.y, let y1 = actual.y else { return nil }
            let ancho = actual.x - anterior.x
            guard ancho != 0 else { return y0 }
            return y0 + (y1 - y0) * (x - anterior.x) / ancho
        }
        return nil
    }

    /// Rango vertical que hay que enseñar para que quepa la curva.
    ///
    /// Se recortan los valores absurdos: una asíntota manda `y` a 10^300 y, si se
    /// respeta, el resto de la curva queda aplastado contra el eje y no se ve nada.
    static func rangoVertical(_ muestras: [MuestraCurva]) -> (minimo: Double, maximo: Double)? {
        let valores = muestras.compactMap(\.y).filter { $0.isFinite }
        guard !valores.isEmpty else { return nil }

        let ordenados = valores.sorted()
        // Se descarta el 5% de cada extremo antes de decidir la escala, pero los
        // puntos siguen dibujándose: lo que se ajusta es la ventana, no los datos.
        let recorte = max(0, ordenados.count / 20)
        let utiles = ordenados.count > 2 * recorte
            ? Array(ordenados[recorte..<(ordenados.count - recorte)])
            : ordenados

        var minimo = utiles.first ?? 0
        var maximo = utiles.last ?? 0
        if minimo == maximo {
            // Una recta horizontal daría altura cero y una división entre cero al
            // escalar.
            minimo -= 1
            maximo += 1
        }
        let margen = (maximo - minimo) * 0.1
        return (minimo - margen, maximo + margen)
    }

    /// Cuánto se tarda en muestrear, para poder avisar antes de empezar.
    ///
    /// El número sale de medir el coste real de teclear un valor y leer el LCD. Si un
    /// botón va a tardar cinco segundos, hay que decirlo antes, no después.
    static func segundosEstimados(muestras: Int) -> Double {
        Double(muestras) * 0.1
    }
}
