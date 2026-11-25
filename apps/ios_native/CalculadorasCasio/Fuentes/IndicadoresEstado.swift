import Foundation

/// Indicadores de la fila superior del LCD: DEG, RAD, Math, M, STO, CMPLX...
///
/// Viven en la fila 0 del buffer de pantalla, un bit cada uno. Las mascaras y
/// desplazamientos salen del emulador de escritorio y se comprobaron contra volcados
/// reales: en la fx-570VN PLUS el byte 0x07 valia 0x20 (DEG) y el 0x0A valia 0x40
/// (Math), justo lo que predice la tabla.
///
/// La calculadora fisica los tiene serigrafiados y solo los enciende; aqui hay que
/// dibujarlos, porque la caratula extraida no los trae.
struct IndicadorEstado {
    let etiqueta: String
    let mascara: UInt8
    let desplazamiento: Int
    /// Los de modo angular y formato conviene destacarlos: equivocarse de DEG/RAD es
    /// el error silencioso mas facil de cometer.
    let destacado: Bool

    static let esPlus: [IndicadorEstado] = [
        IndicadorEstado(etiqueta: "S", mascara: 0x10, desplazamiento: 0x00, destacado: false),
        IndicadorEstado(etiqueta: "A", mascara: 0x04, desplazamiento: 0x00, destacado: false),
        IndicadorEstado(etiqueta: "M", mascara: 0x10, desplazamiento: 0x01, destacado: false),
        IndicadorEstado(etiqueta: "STO", mascara: 0x02, desplazamiento: 0x01, destacado: false),
        IndicadorEstado(etiqueta: "RCL", mascara: 0x40, desplazamiento: 0x02, destacado: false),
        IndicadorEstado(etiqueta: "STAT", mascara: 0x40, desplazamiento: 0x03, destacado: false),
        IndicadorEstado(etiqueta: "CMPLX", mascara: 0x80, desplazamiento: 0x04, destacado: false),
        IndicadorEstado(etiqueta: "MAT", mascara: 0x40, desplazamiento: 0x05, destacado: false),
        // 0x02, no 0x01: medido entrando en modo VECTOR sobre la ROM, donde el byte
        // 0x05 vale 0x02 (y 0x40 en modo MATRIX, que comparte byte). Con 0x01 este
        // indicador no se encendia nunca.
        IndicadorEstado(etiqueta: "VCT", mascara: 0x02, desplazamiento: 0x05, destacado: false),
        IndicadorEstado(etiqueta: "DEG", mascara: 0x20, desplazamiento: 0x07, destacado: true),
        IndicadorEstado(etiqueta: "RAD", mascara: 0x02, desplazamiento: 0x07, destacado: true),
        IndicadorEstado(etiqueta: "GRA", mascara: 0x10, desplazamiento: 0x08, destacado: true),
        IndicadorEstado(etiqueta: "FIX", mascara: 0x01, desplazamiento: 0x08, destacado: true),
        IndicadorEstado(etiqueta: "SCI", mascara: 0x20, desplazamiento: 0x09, destacado: true),
        IndicadorEstado(etiqueta: "Math", mascara: 0x40, desplazamiento: 0x0A, destacado: false),
        IndicadorEstado(etiqueta: "▼", mascara: 0x08, desplazamiento: 0x0A, destacado: false),
        IndicadorEstado(etiqueta: "▲", mascara: 0x80, desplazamiento: 0x0B, destacado: false),
        IndicadorEstado(etiqueta: "Disp", mascara: 0x10, desplazamiento: 0x0B, destacado: false),
    ]

    static let classWiz: [IndicadorEstado] = [
        IndicadorEstado(etiqueta: "S", mascara: 0x01, desplazamiento: 0x00, destacado: false),
        IndicadorEstado(etiqueta: "A", mascara: 0x01, desplazamiento: 0x01, destacado: false),
        IndicadorEstado(etiqueta: "M", mascara: 0x01, desplazamiento: 0x02, destacado: false),
        IndicadorEstado(etiqueta: "STO", mascara: 0x01, desplazamiento: 0x03, destacado: false),
        IndicadorEstado(etiqueta: "Math", mascara: 0x01, desplazamiento: 0x05, destacado: false),
        IndicadorEstado(etiqueta: "DEG", mascara: 0x01, desplazamiento: 0x06, destacado: true),
        IndicadorEstado(etiqueta: "RAD", mascara: 0x01, desplazamiento: 0x07, destacado: true),
        IndicadorEstado(etiqueta: "GRA", mascara: 0x01, desplazamiento: 0x08, destacado: true),
        IndicadorEstado(etiqueta: "FIX", mascara: 0x01, desplazamiento: 0x09, destacado: true),
        IndicadorEstado(etiqueta: "SCI", mascara: 0x01, desplazamiento: 0x0A, destacado: true),
        IndicadorEstado(etiqueta: "E", mascara: 0x01, desplazamiento: 0x0B, destacado: false),
        IndicadorEstado(etiqueta: "i", mascara: 0x01, desplazamiento: 0x0C, destacado: false),
        IndicadorEstado(etiqueta: "∠", mascara: 0x01, desplazamiento: 0x0D, destacado: false),
        IndicadorEstado(etiqueta: "▼", mascara: 0x01, desplazamiento: 0x0F, destacado: false),
        IndicadorEstado(etiqueta: "◄", mascara: 0x01, desplazamiento: 0x10, destacado: false),
        IndicadorEstado(etiqueta: "▽", mascara: 0x01, desplazamiento: 0x11, destacado: false),
        IndicadorEstado(etiqueta: "△", mascara: 0x01, desplazamiento: 0x12, destacado: false),
        IndicadorEstado(etiqueta: "►", mascara: 0x01, desplazamiento: 0x13, destacado: false),
        IndicadorEstado(etiqueta: "⏸", mascara: 0x01, desplazamiento: 0x15, destacado: false),
        IndicadorEstado(etiqueta: "☀", mascara: 0x01, desplazamiento: 0x16, destacado: false),
    ]

    /// Devuelve los indicadores encendidos leyendo la fila 0 del buffer.
    static func activos(en fila: [UInt8], tabla: [IndicadorEstado]) -> [IndicadorEstado] {
        tabla.filter { indicador in
            guard indicador.desplazamiento < fila.count else { return false }
            return fila[indicador.desplazamiento] & indicador.mascara != 0
        }
    }
}
