import Foundation

/// Lee la pantalla de la calculadora y la convierte a texto.
///
/// Es el mismo algoritmo del banco de pruebas, portado a Swift: se trocea la matriz
/// de puntos en glifos (grupos de columnas con tinta, separados por columnas vacias)
/// y se buscan en una tabla de fuente APRENDIDA DE LA PROPIA CALCULADORA, no
/// dibujada a mano.
///
/// Sirve para que la cinta de calculo registre numeros de verdad y no solo teclas.
/// Los glifos que no estan en la tabla salen como "?"; hoy cubre digitos, punto,
/// signo menos y los operadores, en las dos fuentes.
enum LectorPantalla {

    private static let fuente: [String: String] = {
        guard let datos = Paquete.datos("lcdfont", extension: "json"),
              let tabla = try? JSONDecoder().decode([String: String].self, from: datos)
        else { return [:] }
        return tabla
    }()

    /// Devuelve las lineas de texto que hay en pantalla, de arriba abajo.
    static func leer(pixeles: [UInt8], ancho: Int, alto: Int) -> [String] {
        guard ancho > 0, alto > 0, pixeles.count >= ancho * alto, !fuente.isEmpty else {
            return []
        }
        return bandas(pixeles: pixeles, ancho: ancho, alto: alto).map {
            decodificar(banda: $0, pixeles: pixeles, ancho: ancho)
        }
    }

    /// La ultima linea con contenido, que es donde la calculadora deja el resultado.
    static func resultado(pixeles: [UInt8], ancho: Int, alto: Int) -> String? {
        leer(pixeles: pixeles, ancho: ancho, alto: alto).last { !$0.isEmpty }
    }

    // MARK: - Troceado

    /// Filas con tinta agrupadas en bandas: cada banda es una linea de texto.
    private static func bandas(pixeles: [UInt8], ancho: Int, alto: Int) -> [Range<Int>] {
        var resultado: [Range<Int>] = []
        var inicio: Int?
        for y in 0..<alto {
            let hayTinta = (0..<ancho).contains { pixeles[y * ancho + $0] != 0 }
            if hayTinta, inicio == nil {
                inicio = y
            } else if !hayTinta, let comienzo = inicio {
                resultado.append(comienzo..<y)
                inicio = nil
            }
        }
        if let comienzo = inicio { resultado.append(comienzo..<alto) }
        return resultado
    }

    private static func decodificar(banda: Range<Int>, pixeles: [UInt8], ancho: Int) -> String {
        var columnas: [Bool] = []
        for x in 0..<ancho {
            columnas.append(banda.contains { pixeles[$0 * ancho + x] != 0 })
        }

        var recortes: [Range<Int>] = []
        var inicio: Int?
        for x in 0..<ancho {
            if columnas[x], inicio == nil {
                inicio = x
            } else if !columnas[x], let comienzo = inicio {
                recortes.append(comienzo..<x)
                inicio = nil
            }
        }
        if let comienzo = inicio { recortes.append(comienzo..<ancho) }
        guard !recortes.isEmpty else { return "" }

        // El umbral de "esto es un espacio" no puede ser fijo: la fuente de la
        // ClassWiz es bastante mas ancha que la de la ES PLUS, y un hueco normal
        // entre letras alli es mayor que un espacio real aqui.
        //
        // El factor 1.2 sale de medir pantallas reales: con 1.5 el umbral quedaba en
        // 15 px en "Math ERROR", cuyo espacio mide 12, y se lo comia. Debe seguir
        // sincronizado con Tools/pruebas/lcdfont.py, que es donde se midio.
        let anchos = recortes.map { $0.count }.sorted()
        let tipico = anchos[anchos.count / 2]
        let hueco = max(4, Int(Double(tipico) * 1.2))

        var texto = ""
        var finAnterior: Int?
        for recorte in recortes {
            if let anterior = finAnterior, recorte.lowerBound - anterior >= hueco {
                texto += " "
            }
            texto += fuente[clave(recorte: recorte, banda: banda, pixeles: pixeles, ancho: ancho)] ?? "?"
            finAnterior = recorte.upperBound
        }
        return texto.trimmingCharacters(in: .whitespaces)
    }

    /// Representacion normalizada del glifo, identica a la que genera el banco de
    /// pruebas: filas de '#' y '.', sin filas vacias arriba ni abajo, unidas por \n.
    private static func clave(recorte: Range<Int>, banda: Range<Int>,
                              pixeles: [UInt8], ancho: Int) -> String {
        var filas: [String] = []
        for y in banda {
            var fila = ""
            for x in recorte {
                fila += pixeles[y * ancho + x] != 0 ? "#" : "."
            }
            filas.append(fila)
        }
        while let primera = filas.first, !primera.contains("#") { filas.removeFirst() }
        while let ultima = filas.last, !ultima.contains("#") { filas.removeLast() }
        return filas.joined(separator: "\n")
    }
}
