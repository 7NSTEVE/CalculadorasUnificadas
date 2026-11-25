import Foundation

/// Lee las memorias A-F, X, Y y M directamente de la RAM de la Casio (punto 33).
///
/// La diferencia con el panel de variables que ya existe: aquel guarda lo que TÚ
/// apuntaste, y puede quedarse viejo en cuanto guardas algo sin decírselo. Esto es lo
/// que la calculadora tiene de verdad, ahora mismo.
///
/// Dónde vive cada memoria no está documentado en ningún sitio. Se averiguó por
/// análisis diferencial con `Tools/analysis/localizar_variables.py`: se guarda un
/// valor, se vuelca la RAM, se guarda otro distinto y se mira qué bytes cambiaron,
/// descartando lo que cambia igual guardes donde guardes. El resultado salió limpio:
/// una tabla de ranuras de 10 bytes, seguidas y en orden alfabético.

/// Dónde está cada memoria en la RAM de un modelo.
struct MapaVariables {
    /// Dirección de `A`, relativa al principio de la RAM.
    let baseA: Int
    /// Cuántos bytes ocupa cada memoria.
    let paso: Int
    /// Orden de las memorias a partir de `baseA`.
    let orden: [String]
    /// `M` está fuera de la tabla, antes que las demás.
    let direccionM: Int

    /// fx-570VN PLUS. Medido, no supuesto: A=0x023A y de ahí cada 10 bytes hasta
    /// Y=0x0280, con M aparte en 0x0226.
    static let esPlus = MapaVariables(
        baseA: 0x023A, paso: 0x0A,
        orden: ["A", "B", "C", "D", "E", "F", "X", "Y"],
        direccionM: 0x0226)

    /// fx-570/991LA X. Sin confirmar todavía: el escaneo no encontró ranuras
    /// propias, así que aquí NO se inventa una dirección. Mientras sea nil, la app
    /// dice que no puede leer las memorias de este modelo en vez de enseñar basura.
    static let classWiz: MapaVariables? = nil

    static func para(hardwareId: Int32) -> MapaVariables? {
        hardwareId == 4 ? classWiz : esPlus
    }

    /// Dirección de una memoria por su nombre.
    func direccion(de nombre: String) -> Int? {
        if nombre == "M" { return direccionM }
        guard let indice = orden.firstIndex(of: nombre) else { return nil }
        return baseA + indice * paso
    }

    var nombres: [String] { orden + ["M"] }
}

/// Una memoria leída de la máquina.
struct MemoriaCasio: Identifiable, Equatable {
    var id: String { nombre }
    let nombre: String
    /// Valor decodificado, o nil si la ranura no contiene un número válido.
    let valor: Double?
    /// Bytes crudos, para el modo diagnóstico.
    let crudo: [UInt8]

    var estaVacia: Bool { valor == nil || valor == 0 }
}

enum VariablesCasio {

    /// Decodifica el número BCD de una ranura de 10 bytes.
    ///
    /// El formato NO está documentado: se dedujo midiendo. Guardando valores conocidos
    /// y volcando la RAM salió esto, y cada línea de aquí abajo es una medición:
    ///
    ///     +12345678  ->  01 23 45 67 80 00 00 00 07 01
    ///     -12345678  ->  01 23 45 67 80 00 00 00 07 06
    ///     -5         ->  05 00 00 00 00 00 00 00 00 06
    ///      0,125     ->  01 25 00 00 00 00 00 00 99 00
    ///
    /// De donde:
    ///   - bytes 0-7: mantisa en BCD, dos dígitos por byte. El PRIMER nibble no es
    ///     parte del número; la mantisa empieza en el segundo y vale `d,ddddddd…`.
    ///   - byte 8: exponente en BCD, en complemento a diez. `99` es −1, que es lo que
    ///     hace que 0,125 salga como 1,25×10⁻¹.
    ///   - byte 9: banderas. El bit 0x04 marca negativo; se confirmó con dos valores
    ///     negativos y tres positivos. Los demás bits cambian y todavía no se sabe
    ///     qué significan, así que no se tocan.
    static func decodificar(_ bytes: [UInt8]) -> Double? {
        guard bytes.count >= 10 else { return nil }

        // Una ranura sin estrenar está a ceros, y eso sí es un cero de verdad.
        if bytes.allSatisfy({ $0 == 0 }) { return 0 }

        // Mantisa: nibbles 1..15, el primero es la cifra de las unidades.
        var digitos: [Int] = []
        for byte in bytes[0..<8] {
            digitos.append(Int(byte >> 4))
            digitos.append(Int(byte & 0x0F))
        }
        guard digitos.allSatisfy({ $0 <= 9 }) else { return nil }

        var mantisa = 0.0
        var peso = 1.0
        for digito in digitos.dropFirst() {
            mantisa += Double(digito) * peso
            peso /= 10
        }

        // Exponente en BCD y en complemento a diez: 0x99 es −1, 0x07 es 7.
        let byteExponente = bytes[8]
        let alto = Int(byteExponente >> 4), bajo = Int(byteExponente & 0x0F)
        guard alto <= 9, bajo <= 9 else { return nil }
        var exponente = alto * 10 + bajo
        if exponente > 50 { exponente -= 100 }

        let negativo = (bytes[9] & 0x04) != 0
        let resultado = (negativo ? -1.0 : 1.0) * mantisa * pow(10, Double(exponente))
        return resultado.isFinite ? resultado : nil
    }

    /// Lee todas las memorias de una RAM volcada.
    ///
    /// Devuelve vacío si el modelo no tiene mapa confirmado. Es a propósito: enseñar
    /// números sacados de una dirección adivinada sería exactamente el tipo de dato
    /// que parece bueno y está mal.
    static func leer(ram: [UInt8], mapa: MapaVariables?) -> [MemoriaCasio] {
        guard let mapa else { return [] }

        return mapa.nombres.compactMap { nombre in
            guard let direccion = mapa.direccion(de: nombre),
                  direccion >= 0, direccion + mapa.paso <= ram.count else { return nil }
            let trozo = Array(ram[direccion..<(direccion + mapa.paso)])
            return MemoriaCasio(nombre: nombre, valor: decodificar(trozo), crudo: trozo)
        }
    }

    /// Por qué no se pueden leer las memorias de un modelo, para poder decirlo.
    static func explicacionSinMapa(_ modelo: String) -> String {
        "Todavía no se sabe dónde guarda \(modelo) sus memorias. Se localiza por "
        + "análisis diferencial de la RAM y en este modelo el escaneo aún no dio una "
        + "ranura propia. Antes que enseñar un número de una dirección adivinada, no "
        + "se enseña nada."
    }
}
