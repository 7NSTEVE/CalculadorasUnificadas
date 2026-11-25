import Foundation

/// Convierte lo hecho en la calculadora a algo que se pueda pegar en OneNote.
///
/// Todo lo de aquí es generación de texto, sin UIKit, para poder probarlo sin Mac.
/// Lo que sí necesita dibujar —la tarjeta en PNG con fondo transparente— vive en
/// `TarjetaCalculo.swift`, que es donde está el `UIGraphicsImageRenderer`.
///
/// El criterio para todos los formatos es el mismo: lo que se pega tiene que
/// entenderse dentro de dos semanas sin la app delante. Por eso van el número de
/// referencia y el objetivo, no solo el resultado.

/// En qué se convierte una sesión al exportarla.
enum FormatoExportacion: String, CaseIterable, Identifiable {
    case textoLineal
    case markdown
    case latex
    case mathml

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .textoLineal: return "Texto lineal"
        case .markdown: return "Markdown"
        case .latex: return "LaTeX"
        case .mathml: return "MathML"
        }
    }

    /// Para el menú: qué app espera cada cosa.
    var pista: String {
        switch self {
        case .textoLineal: return "pegar en cualquier sitio"
        case .markdown: return "OneNote, Obsidian, cuadernos"
        case .latex: return "informes y ecuaciones"
        case .mathml: return "Word y OneNote como ecuación editable"
        }
    }
}

/// Qué entradas entran en la exportación.
enum AlcanceExportacion: String, CaseIterable, Identifiable {
    /// Todo lo que hay en la cinta.
    case todo
    /// Solo lo marcado con estrella. Es el punto 81: la cinta de una tarde tiene
    /// cincuenta líneas y al cuaderno van cuatro.
    case soloDestacado

    var id: String { rawValue }

    var nombre: String {
        switch self {
        case .todo: return "Toda la cinta"
        case .soloDestacado: return "Solo lo destacado ⭐"
        }
    }
}

/// Una línea ya lista para exportar: número, expresión, resultado.
struct LineaExportable: Equatable {
    /// Numeración automática dentro del documento: (1), (2)… (punto 89).
    let numero: Int
    let expresion: String
    let resultado: String
    let modo: String
    let destacada: Bool
    /// Código corto de referencia, para poder citarla en el cuaderno (punto 90).
    let referencia: String
}

enum Exportacion {

    // MARK: - Códigos de referencia

    /// Código corto y estable de una entrada: `CIR-04-R3`.
    ///
    /// Sirve para escribir a mano «ver CIR-04-R3» en el cuaderno y poder volver.
    /// Se construye con el prefijo de la materia, el número de ejercicio y el de
    /// resultado dentro del ejercicio; nada de UUID, que no se copian a mano.
    static func referencia(materia: String, ejercicio: Int, resultado: Int) -> String {
        let prefijo = prefijoMateria(materia)
        return String(format: "%@-%02d-R%d", prefijo, max(0, ejercicio), max(1, resultado))
    }

    /// Tres letras en mayúscula, sin tildes. «Circuitos» → `CIR`.
    static func prefijoMateria(_ materia: String) -> String {
        let limpio = materia
            .folding(options: .diacriticInsensitive, locale: nil)
            .uppercased()
            .filter { $0.isLetter }
        guard !limpio.isEmpty else { return "GEN" }
        return String(limpio.prefix(3))
    }

    /// Enlace que devuelve a la app y abre esa entrada (punto 91).
    ///
    /// Va al final de cada bloque exportado. Si el esquema no está registrado en el
    /// Info.plist, iOS simplemente no lo abre: no rompe nada del texto pegado.
    static func enlace(referencia: String, modelo: String) -> String {
        var partes = URLComponents()
        partes.scheme = "casiocalc"
        partes.host = "entrada"
        partes.queryItems = [
            URLQueryItem(name: "ref", value: referencia),
            URLQueryItem(name: "modelo", value: modelo),
        ]
        return partes.url?.absoluteString ?? "casiocalc://entrada"
    }

    // MARK: - Preparar las líneas

    /// Numera y referencia las entradas que toca exportar.
    ///
    /// La cinta viene con la más reciente primero, que es como se lee en pantalla;
    /// para el cuaderno se le da la vuelta, que es como se lee un procedimiento.
    static func preparar(cinta: [EntradaCinta], destacadas: Set<UUID>,
                         alcance: AlcanceExportacion, materia: String,
                         ejercicio: Int) -> [LineaExportable] {
        let ordenadas = cinta.reversed().filter { entrada in
            alcance == .todo || destacadas.contains(entrada.id)
        }
        return ordenadas.enumerated().map { indice, entrada in
            LineaExportable(
                numero: indice + 1,
                expresion: entrada.teclas,
                resultado: entrada.resultado,
                modo: entrada.modo,
                destacada: destacadas.contains(entrada.id),
                referencia: referencia(materia: materia, ejercicio: ejercicio,
                                       resultado: indice + 1))
        }
    }

    // MARK: - Formatos

    /// Una línea suelta en texto plano: `(1) 12/4700 = 2,553191489E-03`.
    static func textoLineal(_ linea: LineaExportable) -> String {
        let cuerpo = linea.resultado.isEmpty
            ? linea.expresion
            : "\(linea.expresion) = \(linea.resultado)"
        return "(\(linea.numero)) \(cuerpo)"
    }

    static func documentoTextoLineal(_ lineas: [LineaExportable], objetivo: String) -> String {
        var salida: [String] = []
        if !objetivo.isEmpty { salida.append(objetivo) }
        salida.append(contentsOf: lineas.map(textoLineal))
        return salida.joined(separator: "\n")
    }

    /// Markdown con tabla. Es lo que mejor pega OneNote conservando la estructura.
    static func documentoMarkdown(_ lineas: [LineaExportable], objetivo: String,
                                  modelo: String) -> String {
        var salida: [String] = []
        salida.append("## \(objetivo.isEmpty ? "Cálculos" : objetivo)")
        salida.append("")
        salida.append("| # | Operación | Resultado | Ref |")
        salida.append("|---|---|---|---|")
        for linea in lineas {
            let marca = linea.destacada ? " ⭐" : ""
            salida.append("| \(linea.numero) | `\(escaparMarkdown(linea.expresion))`"
                          + " | **\(escaparMarkdown(linea.resultado))**\(marca)"
                          + " | \(linea.referencia) |")
        }
        salida.append("")
        salida.append("<sub>Calculado en \(modelo) · "
                      + "[volver](\(enlace(referencia: lineas.first?.referencia ?? "", modelo: modelo)))</sub>")
        return salida.joined(separator: "\n")
    }

    /// La barra vertical parte la tabla, y el acento invertido cierra el código.
    static func escaparMarkdown(_ texto: String) -> String {
        texto
            .replacingOccurrences(of: "|", with: "\\|")
            .replacingOccurrences(of: "`", with: "'")
    }

    /// LaTeX en `align`, que es como se entrega un informe.
    static func documentoLatex(_ lineas: [LineaExportable], objetivo: String) -> String {
        var salida: [String] = []
        if !objetivo.isEmpty {
            salida.append("% \(objetivo)")
        }
        salida.append("\\begin{align}")
        for linea in lineas {
            let izquierda = aLatex(linea.expresion)
            let derecha = aLatex(linea.resultado)
            salida.append("  \(izquierda) &= \(derecha) \\tag{\(linea.numero)} \\\\")
        }
        salida.append("\\end{align}")
        return salida.joined(separator: "\n")
    }

    /// Traduce la notación de la Casio a LaTeX.
    ///
    /// No pretende cubrir todo: cubre lo que sale de verdad de estas dos máquinas
    /// —fracciones, raíces, potencias, π y los operadores— y lo que no reconoce lo
    /// deja tal cual, que es preferible a inventarse una traducción.
    static func aLatex(_ expresion: String) -> String {
        var salida = expresion
        // Primero los símbolos que la Casio escribe con caracteres propios.
        let directos: [(String, String)] = [
            ("×", " \\times "), ("÷", " \\div "), ("−", "-"),
            ("π", "\\pi "), ("√", "\\sqrt"), ("≤", "\\leq "), ("≥", "\\geq "),
            ("Ω", "\\Omega "), ("µ", "\\mu "), ("°", "^{\\circ}"),
        ]
        for (casio, latex) in directos {
            salida = salida.replacingOccurrences(of: casio, with: latex)
        }
        // La división pasa a \frac solo cuando los dos lados son simples; si hay
        // paréntesis por medio, forzarlo saldría peor que dejar la barra.
        salida = fraccionSimple(salida)
        // El asterisco no existe en notación matemática escrita.
        salida = salida.replacingOccurrences(of: "*", with: " \\cdot ")
        return salida.trimmingCharacters(in: .whitespaces)
    }

    /// `12/4700` → `\frac{12}{4700}`. Solo si no hay paréntesis ni otra barra.
    static func fraccionSimple(_ texto: String) -> String {
        let trozos = texto.components(separatedBy: "/")
        guard trozos.count == 2 else { return texto }
        let arriba = trozos[0].trimmingCharacters(in: .whitespaces)
        let abajo = trozos[1].trimmingCharacters(in: .whitespaces)
        guard !arriba.isEmpty, !abajo.isEmpty,
              !arriba.contains("("), !abajo.contains("("),
              !arriba.contains(")"), !abajo.contains(")") else { return texto }
        return "\\frac{\(arriba)}{\(abajo)}"
    }

    /// MathML, que es lo que Word y OneNote pegan como ecuación editable.
    ///
    /// Se genera plano (`mn`/`mo`/`mi` seguidos) a propósito: construir el árbol
    /// completo pediría analizar la expresión, y una estructura mal deducida se pega
    /// como una ecuación que parece correcta y no lo es.
    static func documentoMathML(_ lineas: [LineaExportable]) -> String {
        var salida: [String] = []
        for linea in lineas {
            salida.append("<math xmlns=\"http://www.w3.org/1998/Math/MathML\" display=\"block\">")
            salida.append("  <mrow>")
            salida.append(contentsOf: nodosMathML(linea.expresion).map { "    \($0)" })
            if !linea.resultado.isEmpty {
                salida.append("    <mo>=</mo>")
                salida.append(contentsOf: nodosMathML(linea.resultado).map { "    \($0)" })
            }
            salida.append("  </mrow>")
            salida.append("</math>")
        }
        return salida.joined(separator: "\n")
    }

    /// Trocea en números, operadores e identificadores.
    static func nodosMathML(_ expresion: String) -> [String] {
        var nodos: [String] = []
        var numero = ""

        func cerrarNumero() {
            guard !numero.isEmpty else { return }
            nodos.append("<mn>\(numero)</mn>")
            numero = ""
        }

        for caracter in expresion {
            if caracter.isNumber || caracter == "." || caracter == "," {
                numero.append(caracter)
            } else if caracter.isWhitespace {
                cerrarNumero()
            } else if caracter.isLetter {
                cerrarNumero()
                nodos.append("<mi>\(escaparXML(String(caracter)))</mi>")
            } else {
                cerrarNumero()
                nodos.append("<mo>\(escaparXML(String(caracter)))</mo>")
            }
        }
        cerrarNumero()
        return nodos
    }

    /// Sin esto, un `<` o un `&` en la expresión rompe el XML entero al pegarlo.
    static func escaparXML(_ texto: String) -> String {
        texto
            .replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\"", with: "&quot;")
    }

    /// El documento completo en el formato pedido.
    static func documento(_ lineas: [LineaExportable], formato: FormatoExportacion,
                          objetivo: String, modelo: String) -> String {
        switch formato {
        case .textoLineal: return documentoTextoLineal(lineas, objetivo: objetivo)
        case .markdown: return documentoMarkdown(lineas, objetivo: objetivo, modelo: modelo)
        case .latex: return documentoLatex(lineas, objetivo: objetivo)
        case .mathml: return documentoMathML(lineas)
        }
    }
}
