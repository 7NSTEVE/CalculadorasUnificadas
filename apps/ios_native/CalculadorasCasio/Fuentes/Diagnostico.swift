import Foundation

/// Qué máquina se está usando exactamente, y cómo demostrarlo.
///
/// Existe por una razón concreta: esta app afirma que los resultados son los de una
/// Casio de verdad. Una afirmación así tiene que poder comprobarse, y sin una huella
/// de la ROM a la vista es solo una promesa.
///
/// También sirve para lo otro: si algún día un cálculo sale raro, hace falta poder
/// mandar el caso entero —ROM, estado inicial, teclas— y que otra persona lo
/// reproduzca exactamente.

/// Identidad de una máquina emulada.
struct FichaFidelidad {
    let modelo: String
    let romBytes: Int
    /// Huella de la ROM. Es el dato que permite decir «esta es la misma ROM».
    let romHuella: String
    let hardwareId: Int
    /// Segmentos de memoria de código menos uno.
    let csrMask: Int
    /// Falso cuando la ROM salió del emulador oficial de Casio en vez de una
    /// calculadora física. Es una distinción honesta, no un detalle técnico.
    let hardwareReal: Bool
    let pantalla: String

    /// Lo que se puede afirmar sin exagerar.
    ///
    /// Se redacta con cuidado a propósito: lo verificado es que esta ROM es la que
    /// usa el emulador oficial de Casio, byte a byte. Que esa ROM coincida con la de
    /// una calculadora física comprada en una tienda **no** se ha comprobado, y decir
    /// lo contrario sería mentir.
    var afirmacion: String {
        hardwareReal
            ? "Volcado de una calculadora física."
            : "ROM del emulador oficial de Casio, verificada byte a byte. "
            + "No se ha comprobado que coincida con la de una unidad física."
    }

    var lineas: [(String, String)] {
        [
            ("Modelo", modelo),
            ("ROM", "\(romBytes) bytes"),
            ("Huella", romHuella),
            ("Familia", hardwareId == 4 ? "ClassWiz (nX-U8)" : "ES PLUS (nX-U8)"),
            ("Memoria de código", "\((csrMask + 1) * 64) KiB"),
            ("Pantalla", pantalla),
            ("Núcleo", "CasioEmu portado, sin SDL ni Lua"),
        ]
    }
}

/// Un fallo empaquetado para poder reproducirlo en otra máquina.
///
/// Sin esto, «me dio un resultado raro» no se puede investigar: falta el estado de
/// partida, que es justo lo que nadie recuerda.
struct InformeFallo: Codable {
    let modelo: String
    let romHuella: String
    let teclas: [UInt8]
    let pantallaFinal: [String]
    let momento: Date
    let nota: String

    /// Texto plano para pegarlo donde sea. No se comparte nada más que esto: ni
    /// contactos, ni ubicación, ni la sesión de estudio.
    var texto: String {
        var lineas = [
            "# Fallo en \(modelo)",
            "",
            "ROM: \(romHuella)",
            "Fecha: \(ISO8601DateFormatter().string(from: momento))",
        ]
        if !nota.isEmpty { lineas.append("Nota: \(nota)") }
        lineas.append("")
        lineas.append("## Teclas desde el encendido (\(teclas.count))")
        lineas.append(teclas.map { String(format: "%02X", $0) }.joined(separator: " "))
        lineas.append("")
        lineas.append("## Pantalla al terminar")
        lineas.append(contentsOf: pantallaFinal.map { "  \($0)" })
        lineas.append("")
        lineas.append("Para reproducirlo:")
        lineas.append("  CalcEmuCore/build/harness.exe <rom> <hw> <real> <csr> <pd> \\")
        lineas.append("    salida \"\(guionHarness)\"")
        return lineas.joined(separator: "\n")
    }

    /// El guion en el formato que entiende el banco de pruebas, para reproducirlo
    /// sin tener que teclear nada a mano.
    var guionHarness: String {
        var tokens = ["w1000", "kFF", "w3000"]
        tokens.append(contentsOf: teclas.map { String(format: "k%02X", $0) })
        tokens.append("!final")
        return tokens.joined(separator: ",")
    }
}

enum Diagnostico {

    /// Huella corta y estable de la ROM.
    ///
    /// Es FNV-1a, la misma que usa el núcleo para validar las instantáneas. No es
    /// criptográfica y no lo pretende: solo tiene que distinguir una ROM de otra y
    /// caber en una línea que se pueda leer en voz alta.
    static func huella(_ datos: Data) -> String {
        var valor: UInt64 = 1469598103934665603
        for byte in datos {
            valor ^= UInt64(byte)
            valor = valor &* 1099511628211
        }
        return String(format: "%016llX", valor)
    }

    /// Huella de la ROM de un modelo, leída del paquete.
    static func huellaDeModelo(_ recurso: String) -> String {
        guard let datos = Paquete.datos(recurso, extension: "bin") else { return "sin ROM" }
        return huella(datos)
    }
}
