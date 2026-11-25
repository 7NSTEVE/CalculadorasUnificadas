import Foundation

/// Lee lo que la Casio ya tiene en pantalla y lo convierte en algo dibujable.
///
/// Importa de dónde salen estos números: **no se calculan aquí**. La calculadora
/// emulada resuelve, el lector de pantalla saca el texto y esto solo lo interpreta
/// para poder pintarlo. Si alguna vez el dibujo y la pantalla no coinciden, manda la
/// pantalla, porque es la Casio de verdad.
///
/// El motivo de todo el módulo es que un `3+4i` en una línea de LCD no dice nada
/// sobre el ángulo, y en alterna el ángulo es la mitad del ejercicio.

/// Un número complejo, con las dos formas de leerlo.
struct Complejo: Equatable {
    var real: Double
    var imaginaria: Double

    /// Longitud del fasor.
    var modulo: Double { (real * real + imaginaria * imaginaria).squareRoot() }

    /// Ángulo en grados, de −180 a 180, que es como lo da la Casio en modo `r∠θ`.
    var argumentoGrados: Double {
        guard real != 0 || imaginaria != 0 else { return 0 }
        return atan2(imaginaria, real) * 180 / .pi
    }

    var argumentoRadianes: Double {
        guard real != 0 || imaginaria != 0 else { return 0 }
        return atan2(imaginaria, real)
    }

    static func desdePolar(modulo: Double, gradosArgumento: Double) -> Complejo {
        let radianes = gradosArgumento * .pi / 180
        return Complejo(real: modulo * cos(radianes), imaginaria: modulo * sin(radianes))
    }

    /// `3+4i`, con la coma decimal del modelo latinoamericano si se pide.
    func rectangular(decimales: Int = 4, separador: String = ",") -> String {
        let parteReal = numero(real, decimales: decimales, separador: separador)
        let parteImaginaria = numero(abs(imaginaria), decimales: decimales, separador: separador)
        if imaginaria == 0 { return parteReal }
        if real == 0 {
            return imaginaria < 0 ? "-\(parteImaginaria)i" : "\(parteImaginaria)i"
        }
        return "\(parteReal)\(imaginaria < 0 ? "-" : "+")\(parteImaginaria)i"
    }

    /// `5∠53,13°`, que es como se escribe un fasor en el cuaderno.
    func polar(decimales: Int = 4, separador: String = ",") -> String {
        "\(numero(modulo, decimales: decimales, separador: separador))∠"
        + "\(numero(argumentoGrados, decimales: decimales, separador: separador))°"
    }

    private func numero(_ valor: Double, decimales: Int, separador: String) -> String {
        guard valor.isFinite else { return "—" }
        var texto = String(format: "%.\(decimales)f", valor)
        if texto.contains(".") {
            while texto.hasSuffix("0") { texto.removeLast() }
            if texto.hasSuffix(".") { texto.removeLast() }
        }
        // "-0" es matemáticamente cero pero se lee como un error de la app.
        if texto == "-0" { texto = "0" }
        return texto.replacingOccurrences(of: ".", with: separador)
    }
}

/// Lo que se saca de una línea de LCD.
enum ContenidoPantalla: Equatable {
    case complejo(Complejo)
    case vector([Double])
    case matriz([[Double]])
    case numero(Double)
    case nada
}

enum LectorContenido {

    /// Interpreta una línea del LCD.
    ///
    /// Acepta las dos notaciones que producen estas máquinas: rectangular (`3+4i`) y
    /// polar (`5∠53,13`). La coma decimal de la LA X se admite en las dos, porque es
    /// lo que esa ROM escribe de verdad y ya nos costó una vez creer que no.
    static func interpretar(_ linea: String) -> ContenidoPantalla {
        let texto = linea.trimmingCharacters(in: .whitespaces)
        guard !texto.isEmpty else { return .nada }

        if let complejo = complejoPolar(texto) { return .complejo(complejo) }
        if let complejo = complejoRectangular(texto) { return .complejo(complejo) }
        if let valor = numero(texto) { return .numero(valor) }
        return .nada
    }

    /// `5∠53,13` o `5∠53,13°`.
    static func complejoPolar(_ texto: String) -> Complejo? {
        let partes = texto.components(separatedBy: "∠")
        guard partes.count == 2,
              let modulo = numero(partes[0]),
              let angulo = numero(partes[1].replacingOccurrences(of: "°", with: ""))
        else { return nil }
        return Complejo.desdePolar(modulo: modulo, gradosArgumento: angulo)
    }

    /// `3+4i`, `-4i`, `3-4i`, `4i`.
    static func complejoRectangular(_ texto: String) -> Complejo? {
        guard texto.hasSuffix("i") else { return nil }
        let cuerpo = String(texto.dropLast())

        // El signo que separa las dos partes es el último + o − que NO sea el signo
        // de un exponente (1E-3) ni el del primer carácter.
        var corte: String.Index?
        var anterior: Character?
        for indice in cuerpo.indices {
            let caracter = cuerpo[indice]
            guard caracter == "+" || caracter == "-" || caracter == "−" else {
                anterior = caracter
                continue
            }
            if indice == cuerpo.startIndex { anterior = caracter; continue }
            if let previo = anterior, previo == "E" || previo == "e" {
                anterior = caracter
                continue
            }
            corte = indice
            anterior = caracter
        }

        guard let posicion = corte else {
            // Solo parte imaginaria: "4i" o "-4i". Un cuerpo vacío es "i" = 1i.
            let valor = cuerpo.isEmpty ? 1.0
                      : (cuerpo == "-" || cuerpo == "−") ? -1.0
                      : numero(cuerpo)
            guard let imaginaria = valor else { return nil }
            return Complejo(real: 0, imaginaria: imaginaria)
        }

        let izquierda = String(cuerpo[cuerpo.startIndex..<posicion])
        var derecha = String(cuerpo[posicion...])
        // "3+i" es 3+1i, no 3+0i.
        if derecha == "+" || derecha == "-" || derecha == "−" { derecha += "1" }

        guard let real = numero(izquierda), let imaginaria = numero(derecha) else { return nil }
        return Complejo(real: real, imaginaria: imaginaria)
    }

    /// Convierte texto de la Casio a Double: coma decimal, menos tipográfico y `E`.
    static func numero(_ texto: String) -> Double? {
        var limpio = texto.trimmingCharacters(in: .whitespaces)
            .replacingOccurrences(of: ",", with: ".")
            .replacingOccurrences(of: "−", with: "-")   // U+2212, el que dibuja el LCD
            .replacingOccurrences(of: " ", with: "")
        guard !limpio.isEmpty else { return nil }
        // "3." y ".5" son válidos para la Casio y Double los acepta, pero un "+" a la
        // izquierda no.
        if limpio.hasPrefix("+") { limpio.removeFirst() }
        return Double(limpio)
    }
}

/// Puntos ya calculados para dibujar un fasor, en coordenadas de pantalla.
///
/// Va aquí y no en la vista porque la conversión es lo único que puede salir mal, y
/// aquí se puede probar sin abrir la app.
struct DibujoFasor: Equatable {
    /// Escala: cuántos puntos de pantalla mide una unidad.
    let escala: Double
    /// Centro del plano, en puntos de pantalla.
    let centro: (x: Double, y: Double)

    static func == (izquierda: DibujoFasor, derecha: DibujoFasor) -> Bool {
        izquierda.escala == derecha.escala
            && izquierda.centro == derecha.centro
    }

    /// Dónde cae un complejo dentro del lienzo.
    ///
    /// El eje Y se invierte porque en pantalla crece hacia abajo, y un fasor dibujado
    /// del revés es peor que no dibujarlo.
    func punto(_ complejo: Complejo) -> (x: Double, y: Double) {
        (x: centro.x + complejo.real * escala,
         y: centro.y - complejo.imaginaria * escala)
    }

    /// Escala que deja el mayor de los fasores dentro del lienzo con margen.
    static func para(_ complejos: [Complejo], lado: Double,
                     margen: Double = 0.15) -> DibujoFasor {
        let mayor = complejos.map(\.modulo).max() ?? 0
        let util = lado * (0.5 - margen)
        // Si todo son ceros no hay escala posible; se pone una cualquiera para no
        // dividir entre cero ni dibujar infinito.
        let escala = mayor > 0 ? util / mayor : 1
        return DibujoFasor(escala: escala, centro: (x: lado / 2, y: lado / 2))
    }

    /// Marcas de la rejilla, en unidades del plano.
    ///
    /// Se eligen redondas (1, 2, 5 y sus múltiplos de diez) porque una rejilla cada
    /// 3,7 unidades no ayuda a leer nada.
    static func pasoRejilla(maximo: Double) -> Double {
        guard maximo > 0, maximo.isFinite else { return 1 }
        let magnitud = pow(10, floor(log10(maximo)))
        let normalizado = maximo / magnitud
        let paso: Double
        if normalizado < 1.5 { paso = 0.2 }
        else if normalizado < 3 { paso = 0.5 }
        else if normalizado < 7 { paso = 1 }
        else { paso = 2 }
        return paso * magnitud
    }
}

/// Vectores y matrices leídos del LCD (puntos 44 y 45).
///
/// Igual que con los complejos: los calcula la Casio, aquí solo se interpretan para
/// poder verlos en grande. Una matriz 3×3 en un LCD de 192×63 obliga a desplazarse
/// celda a celda, y perder de vista la fila de arriba mientras miras la de abajo es
/// exactamente cómo se cuela un error de signo.
extension LectorContenido {

    /// `[1 2 3]`, `[1,2,3]` o `(1 2 3)`.
    static func vector(_ texto: String) -> [Double]? {
        let limpio = texto.trimmingCharacters(in: .whitespaces)
        guard let interior = interiorDeCorchetes(limpio) else { return nil }
        guard !interior.contains("[") else { return nil }   // eso es una matriz
        let valores = trocear(interior).compactMap(numero)
        guard valores.count >= 2, valores.count == trocear(interior).count else { return nil }
        return valores
    }

    /// `[[1 2][3 4]]`: filas entre corchetes dentro de un corchete.
    static func matriz(_ texto: String) -> [[Double]]? {
        let limpio = texto.trimmingCharacters(in: .whitespaces)
        guard let interior = interiorDeCorchetes(limpio), interior.contains("[") else {
            return nil
        }

        var filas: [[Double]] = []
        var actual = ""
        var dentro = false
        for caracter in interior {
            if caracter == "[" { dentro = true; actual = ""; continue }
            if caracter == "]" {
                dentro = false
                let valores = trocear(actual).compactMap(numero)
                guard valores.count == trocear(actual).count, !valores.isEmpty else {
                    return nil
                }
                filas.append(valores)
                continue
            }
            if dentro { actual.append(caracter) }
        }
        // Una matriz con filas de distinta longitud no es una matriz: es un error de
        // lectura, y dibujarla daría una tabla con huecos que parecen ceros.
        guard let ancho = filas.first?.count, !filas.isEmpty,
              filas.allSatisfy({ $0.count == ancho }) else { return nil }
        return filas
    }

    /// Quita el corchete o paréntesis exterior. Nil si no lo hay o no cierra.
    private static func interiorDeCorchetes(_ texto: String) -> String? {
        let abre: Set<Character> = ["[", "("]
        let cierra: Set<Character> = ["]", ")"]
        guard let primero = texto.first, let ultimo = texto.last,
              abre.contains(primero), cierra.contains(ultimo), texto.count > 2 else {
            return nil
        }
        return String(texto.dropFirst().dropLast())
    }

    /// Separa por comas o por espacios, lo que use la pantalla.
    private static func trocear(_ texto: String) -> [String] {
        let separadores = CharacterSet(charactersIn: ", ")
        return texto.components(separatedBy: separadores).filter { !$0.isEmpty }
    }

    /// Interpreta una línea sabiendo también leer vectores y matrices.
    ///
    /// El orden importa: una matriz también empieza por corchete, así que hay que
    /// probarla antes que el vector o `[[1 2][3 4]]` se leería como un vector roto.
    static func interpretarCompleto(_ linea: String) -> ContenidoPantalla {
        let texto = linea.trimmingCharacters(in: .whitespaces)
        if let filas = matriz(texto) { return .matriz(filas) }
        if let valores = vector(texto) { return .vector(valores) }
        return interpretar(texto)
    }
}
