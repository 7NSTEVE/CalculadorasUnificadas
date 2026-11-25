// Posiciones exactas sobre las caratulas reales de los 4 modelos Casio v47
// (fx-CG50, fx-991CW, fx-570LA X, fx-570VN PLUS), normalizadas (0..1) con
// paridad 1-a-1 frente a Windows (cg50_keys_exact.h / cw2_keys_exact.h) y Android.

import CoreGraphics

struct TeclaFisica {
    let nombre: String
    /// Codigo de matriz de 8 bits (nX-U8 / CW2). Para CG50 usar `codigoMatriz16`.
    let codigo: UInt8
    /// Codigo de matriz completo de 16 bits (KIU 16-bit para fx-CG50; igual a `codigo` en 8-bit).
    let codigoMatriz16: UInt16
    /// Rectangulo normalizado sobre la imagen de la caratula.
    let area: CGRect

    init(nombre: String, codigo: UInt8, area: CGRect) {
        self.nombre = nombre
        self.codigo = codigo
        self.codigoMatriz16 = UInt16(codigo)
        self.area = area
    }

    init(nombre: String, codigo16: UInt16, area: CGRect) {
        self.nombre = nombre
        self.codigo = UInt8(truncatingIfNeeded: codigo16)
        self.codigoMatriz16 = codigo16
        self.area = area
    }
}

struct DisposicionCalculadora {
    let caratula: String
    let proporcion: CGFloat
    let areaPantalla: CGRect
    let teclas: [TeclaFisica]
}

private func normRect(cx: CGFloat, cy: CGFloat, w: CGFloat, h: CGFloat, sw: CGFloat, sh: CGFloat) -> CGRect {
    CGRect(x: (cx - w * 0.5) / sw, y: (cy - h * 0.5) / sh, width: w / sw, height: h / sh)
}

enum Disposiciones {
    // 1. fx-CG50 PRIZM (664x1364, 50 teclas con codigos KIU 16-bit exactos)
    static let cg50 = DisposicionCalculadora(
        caratula: "caratula_cg50",
        proporcion: 664.0 / 1364.0,
        areaPantalla: CGRect(x: 92.0 / 664.0, y: 153.0 / 1364.0, width: 480.0 / 664.0, height: 250.0 / 1364.0),
        teclas: [
            TeclaFisica(nombre: "F1", codigo16: 0x070A, area: normRect(cx: 112, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "F2", codigo16: 0x060A, area: normRect(cx: 200, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "F3", codigo16: 0x050A, area: normRect(cx: 288, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "F4", codigo16: 0x040A, area: normRect(cx: 376, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "F5", codigo16: 0x030A, area: normRect(cx: 464, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "F6", codigo16: 0x020A, area: normRect(cx: 552, cy: 559, w: 54, h: 48, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "SHIFT", codigo16: 0x0709, area: normRect(cx: 115, cy: 642, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "OPTN", codigo16: 0x0609, area: normRect(cx: 202, cy: 642, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "VARS", codigo16: 0x0509, area: normRect(cx: 289, cy: 642, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "MENU", codigo16: 0x0409, area: normRect(cx: 376, cy: 642, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "ARRIBA", codigo16: 0x0209, area: normRect(cx: 512, cy: 628, w: 52, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "IZQUIERDA", codigo16: 0x0309, area: normRect(cx: 460, cy: 676, w: 42, h: 52, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "DERECHA", codigo16: 0x0208, area: normRect(cx: 564, cy: 676, w: 42, h: 52, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "ABAJO", codigo16: 0x0308, area: normRect(cx: 512, cy: 724, w: 52, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "ALPHA", codigo16: 0x0708, area: normRect(cx: 115, cy: 711, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "CUADRADO", codigo16: 0x0608, area: normRect(cx: 202, cy: 711, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "POTENCIA", codigo16: 0x0508, area: normRect(cx: 289, cy: 711, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "EXIT", codigo16: 0x0408, area: normRect(cx: 376, cy: 711, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "X_THETA", codigo16: 0x0707, area: normRect(cx: 112, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "LOG", codigo16: 0x0607, area: normRect(cx: 200, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "LN", codigo16: 0x0507, area: normRect(cx: 288, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "SIN", codigo16: 0x0407, area: normRect(cx: 376, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "COS", codigo16: 0x0307, area: normRect(cx: 464, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "TAN", codigo16: 0x0207, area: normRect(cx: 552, cy: 789, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "FRACCION", codigo16: 0x0706, area: normRect(cx: 112, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "SD", codigo16: 0x0606, area: normRect(cx: 200, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "PAREN_ABRE", codigo16: 0x0506, area: normRect(cx: 288, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "PAREN_CIERRA", codigo16: 0x0406, area: normRect(cx: 376, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "COMA", codigo16: 0x0306, area: normRect(cx: 464, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "FLECHA", codigo16: 0x0206, area: normRect(cx: 552, cy: 858, w: 64, h: 42, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "7", codigo16: 0x0705, area: normRect(cx: 121, cy: 956, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "8", codigo16: 0x0605, area: normRect(cx: 226, cy: 956, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "9", codigo16: 0x0505, area: normRect(cx: 332, cy: 956, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "DEL", codigo16: 0x0405, area: normRect(cx: 437, cy: 956, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "AC", codigo16: 0x0101, area: normRect(cx: 542, cy: 956, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "4", codigo16: 0x0704, area: normRect(cx: 121, cy: 1038, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "5", codigo16: 0x0604, area: normRect(cx: 226, cy: 1038, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "6", codigo16: 0x0504, area: normRect(cx: 332, cy: 1038, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "POR", codigo16: 0x0404, area: normRect(cx: 437, cy: 1038, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "ENTRE", codigo16: 0x0304, area: normRect(cx: 542, cy: 1038, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "1", codigo16: 0x0703, area: normRect(cx: 121, cy: 1120, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "2", codigo16: 0x0603, area: normRect(cx: 226, cy: 1120, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "3", codigo16: 0x0503, area: normRect(cx: 332, cy: 1120, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "MAS", codigo16: 0x0403, area: normRect(cx: 437, cy: 1120, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "MENOS", codigo16: 0x0303, area: normRect(cx: 542, cy: 1120, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "0", codigo16: 0x0702, area: normRect(cx: 121, cy: 1202, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "PUNTO", codigo16: 0x0602, area: normRect(cx: 226, cy: 1202, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "EXP", codigo16: 0x0502, area: normRect(cx: 332, cy: 1202, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "NEGATIVO", codigo16: 0x0402, area: normRect(cx: 437, cy: 1202, w: 88, h: 56, sw: 664, sh: 1364)),
            TeclaFisica(nombre: "EXE", codigo16: 0x0302, area: normRect(cx: 542, cy: 1202, w: 88, h: 56, sw: 664, sh: 1364)),
        ])

    // 2. fx-991CW ClassWiz II (992x2082, 48 teclas exactas)
    static let cw2 = DisposicionCalculadora(
        caratula: "caratula_cw2",
        proporcion: 992.0 / 2082.0,
        areaPantalla: CGRect(x: 112.0 / 992.0, y: 318.0 / 2082.0, width: 768.0 / 992.0, height: 292.0 / 2082.0),
        teclas: [
            TeclaFisica(nombre: "ON", codigo: 0xFF, area: normRect(cx: 132, cy: 795, w: 90, h: 90, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "HOME", codigo: 0x17, area: normRect(cx: 278, cy: 797, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "ARRIBA", codigo: 0x37, area: normRect(cx: 570, cy: 797, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "RE_PAG", codigo: 0x57, area: normRect(cx: 862, cy: 811, w: 96, h: 110, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "SETTINGS", codigo: 0x06, area: normRect(cx: 132, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "ATRAS", codigo: 0x16, area: normRect(cx: 278, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "IZQUIERDA", codigo: 0x26, area: normRect(cx: 424, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "OK", codigo: 0x36, area: normRect(cx: 570, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "DERECHA", codigo: 0x46, area: normRect(cx: 716, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "AV_PAG", codigo: 0x56, area: normRect(cx: 862, cy: 939, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "ABAJO", codigo: 0x35, area: normRect(cx: 570, cy: 1065, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "SHIFT", codigo: 0x05, area: normRect(cx: 132, cy: 1087, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "VARIABLE", codigo: 0x15, area: normRect(cx: 278, cy: 1087, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "FUNCTION", codigo: 0x25, area: normRect(cx: 424, cy: 1087, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "CATALOG", codigo: 0x45, area: normRect(cx: 716, cy: 1087, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "TOOLS", codigo: 0x55, area: normRect(cx: 862, cy: 1087, w: 96, h: 96, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "X_VAR", codigo: 0x04, area: normRect(cx: 132, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "FRACCION", codigo: 0x14, area: normRect(cx: 278, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "RAIZ", codigo: 0x24, area: normRect(cx: 424, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "POTENCIA", codigo: 0x34, area: normRect(cx: 570, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "CUADRADO", codigo: 0x44, area: normRect(cx: 716, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "LOG_B", codigo: 0x54, area: normRect(cx: 862, cy: 1222, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "ANS", codigo: 0x03, area: normRect(cx: 132, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "SIN", codigo: 0x13, area: normRect(cx: 278, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "COS", codigo: 0x23, area: normRect(cx: 424, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "TAN", codigo: 0x33, area: normRect(cx: 570, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "PAREN_ABRE", codigo: 0x43, area: normRect(cx: 716, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "PAREN_CIERRA", codigo: 0x53, area: normRect(cx: 862, cy: 1350, w: 94, h: 94, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "7", codigo: 0x02, area: normRect(cx: 139, cy: 1493, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "8", codigo: 0x12, area: normRect(cx: 318, cy: 1493, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "9", codigo: 0x22, area: normRect(cx: 497, cy: 1493, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "DEL", codigo: 0x32, area: normRect(cx: 676, cy: 1493, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "AC", codigo: 0x42, area: normRect(cx: 855, cy: 1493, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "4", codigo: 0x01, area: normRect(cx: 139, cy: 1643, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "5", codigo: 0x11, area: normRect(cx: 318, cy: 1643, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "6", codigo: 0x21, area: normRect(cx: 497, cy: 1643, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "POR", codigo: 0x31, area: normRect(cx: 676, cy: 1643, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "ENTRE", codigo: 0x41, area: normRect(cx: 855, cy: 1643, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "1", codigo: 0x00, area: normRect(cx: 139, cy: 1793, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "2", codigo: 0x10, area: normRect(cx: 318, cy: 1793, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "3", codigo: 0x20, area: normRect(cx: 497, cy: 1793, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "MAS", codigo: 0x30, area: normRect(cx: 676, cy: 1793, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "MENOS", codigo: 0x40, area: normRect(cx: 855, cy: 1793, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "0", codigo: 0x64, area: normRect(cx: 139, cy: 1943, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "PUNTO", codigo: 0x63, area: normRect(cx: 318, cy: 1943, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "EXP", codigo: 0x62, area: normRect(cx: 497, cy: 1943, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "FORMAT", codigo: 0x61, area: normRect(cx: 676, cy: 1943, w: 122, h: 122, sw: 992, sh: 2082)),
            TeclaFisica(nombre: "EXE", codigo: 0x60, area: normRect(cx: 855, cy: 1943, w: 122, h: 122, sw: 992, sh: 2082)),
        ])

    // 3. fx-570VN PLUS (405x816, 50 teclas)
    static let esplus = DisposicionCalculadora(
        caratula: "caratula_esplus",
        proporcion: 405.0 / 816.0,
        areaPantalla: CGRect(x: 0.14321, y: 0.17647, width: 0.71111, height: 0.11397),
        teclas: [
            TeclaFisica(nombre: "SHIFT", codigo: 0x07, area: CGRect(x: 0.09877, y: 0.35784, width: 0.12840, height: 0.05147)),
            TeclaFisica(nombre: "ON", codigo: 0xFF, area: CGRect(x: 0.77778, y: 0.35784, width: 0.12840, height: 0.05147)),
            TeclaFisica(nombre: "ARRIBA", codigo: 0x27, area: CGRect(x: 0.46049, y: 0.36091, width: 0.07654, height: 0.03799)),
            TeclaFisica(nombre: "ALPHA", codigo: 0x17, area: CGRect(x: 0.23210, y: 0.36765, width: 0.12840, height: 0.05147)),
            TeclaFisica(nombre: "MODE", codigo: 0x47, area: CGRect(x: 0.64444, y: 0.36765, width: 0.12840, height: 0.05147)),
            TeclaFisica(nombre: "IZQUIERDA", codigo: 0x26, area: CGRect(x: 0.37654, y: 0.39522, width: 0.07654, height: 0.03799)),
            TeclaFisica(nombre: "DERECHA", codigo: 0x37, area: CGRect(x: 0.54444, y: 0.39522, width: 0.07654, height: 0.03799)),
            TeclaFisica(nombre: "ABAJO", codigo: 0x36, area: CGRect(x: 0.46049, y: 0.42953, width: 0.07654, height: 0.03799)),
            TeclaFisica(nombre: "CALC", codigo: 0x06, area: CGRect(x: 0.09012, y: 0.44424, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "INTEGRAL", codigo: 0x16, area: CGRect(x: 0.22346, y: 0.44424, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "INVERSO", codigo: 0x46, area: CGRect(x: 0.64815, y: 0.44424, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "LOG_B", codigo: 0x56, area: CGRect(x: 0.77654, y: 0.44424, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "FRACCION", codigo: 0x05, area: CGRect(x: 0.10247, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "RAIZ", codigo: 0x15, area: CGRect(x: 0.23580, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "CUADRADO", codigo: 0x25, area: CGRect(x: 0.36914, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "POTENCIA", codigo: 0x35, area: CGRect(x: 0.50247, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "LOG", codigo: 0x45, area: CGRect(x: 0.63580, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "LN", codigo: 0x55, area: CGRect(x: 0.76914, y: 0.50184, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "NEGATIVO", codigo: 0x04, area: CGRect(x: 0.10247, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "GRADOS", codigo: 0x14, area: CGRect(x: 0.23580, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "HYP", codigo: 0x24, area: CGRect(x: 0.36914, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "SIN", codigo: 0x34, area: CGRect(x: 0.50247, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "COS", codigo: 0x44, area: CGRect(x: 0.63580, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "TAN", codigo: 0x54, area: CGRect(x: 0.76914, y: 0.55821, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "RCL", codigo: 0x03, area: CGRect(x: 0.10247, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "ENG", codigo: 0x13, area: CGRect(x: 0.23580, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "PAREN_ABRE", codigo: 0x23, area: CGRect(x: 0.36914, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "PAREN_CIERRA", codigo: 0x33, area: CGRect(x: 0.50247, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "SD", codigo: 0x43, area: CGRect(x: 0.63580, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "MAS_M", codigo: 0x53, area: CGRect(x: 0.76914, y: 0.61458, width: 0.13086, height: 0.04044)),
            TeclaFisica(nombre: "7", codigo: 0x02, area: CGRect(x: 0.10123, y: 0.67157, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "8", codigo: 0x12, area: CGRect(x: 0.26173, y: 0.67157, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "9", codigo: 0x22, area: CGRect(x: 0.42222, y: 0.67157, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "DEL", codigo: 0x32, area: CGRect(x: 0.58272, y: 0.67157, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "AC", codigo: 0x42, area: CGRect(x: 0.74321, y: 0.67157, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "4", codigo: 0x01, area: CGRect(x: 0.10123, y: 0.74142, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "5", codigo: 0x11, area: CGRect(x: 0.26173, y: 0.74142, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "6", codigo: 0x21, area: CGRect(x: 0.42222, y: 0.74142, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "POR", codigo: 0x31, area: CGRect(x: 0.58272, y: 0.74142, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "ENTRE", codigo: 0x41, area: CGRect(x: 0.74321, y: 0.74142, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "1", codigo: 0x00, area: CGRect(x: 0.10123, y: 0.81127, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "2", codigo: 0x10, area: CGRect(x: 0.26173, y: 0.81127, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "3", codigo: 0x20, area: CGRect(x: 0.42222, y: 0.81127, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "MAS", codigo: 0x30, area: CGRect(x: 0.58272, y: 0.81127, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "MENOS", codigo: 0x40, area: CGRect(x: 0.74321, y: 0.81127, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "0", codigo: 0x64, area: CGRect(x: 0.10123, y: 0.88113, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "PUNTO", codigo: 0x63, area: CGRect(x: 0.26173, y: 0.88113, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "EXP", codigo: 0x62, area: CGRect(x: 0.42222, y: 0.88113, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "ANS", codigo: 0x61, area: CGRect(x: 0.58272, y: 0.88113, width: 0.15802, height: 0.05637)),
            TeclaFisica(nombre: "IGUAL", codigo: 0x60, area: CGRect(x: 0.74321, y: 0.88113, width: 0.15802, height: 0.05637)),
        ])

    // 4. fx-570LA X ClassWiz (644x1250, 50 teclas)
    static let classwiz = DisposicionCalculadora(
        caratula: "caratula_classwiz",
        proporcion: 644.0 / 1250.0,
        areaPantalla: CGRect(x: 0.20186, y: 0.20480, width: 0.59627, height: 0.10080),
        teclas: [
            TeclaFisica(nombre: "SHIFT", codigo: 0x07, area: CGRect(x: 0.14674, y: 0.37960, width: 0.08851, height: 0.04560)),
            TeclaFisica(nombre: "ALPHA", codigo: 0x17, area: CGRect(x: 0.25388, y: 0.37960, width: 0.08851, height: 0.04560)),
            TeclaFisica(nombre: "MENU", codigo: 0x47, area: CGRect(x: 0.65450, y: 0.37960, width: 0.08851, height: 0.04560)),
            TeclaFisica(nombre: "ON", codigo: 0xFF, area: CGRect(x: 0.76320, y: 0.37960, width: 0.08851, height: 0.04560)),
            TeclaFisica(nombre: "ARRIBA", codigo: 0x27, area: CGRect(x: 0.45497, y: 0.37760, width: 0.09006, height: 0.03680)),
            TeclaFisica(nombre: "ABAJO", codigo: 0x36, area: CGRect(x: 0.45497, y: 0.44160, width: 0.09006, height: 0.03680)),
            TeclaFisica(nombre: "IZQUIERDA", codigo: 0x26, area: CGRect(x: 0.36335, y: 0.40480, width: 0.07453, height: 0.04640)),
            TeclaFisica(nombre: "DERECHA", codigo: 0x37, area: CGRect(x: 0.56211, y: 0.40480, width: 0.07453, height: 0.04640)),
            TeclaFisica(nombre: "OPTN", codigo: 0x06, area: CGRect(x: 0.14596, y: 0.45720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "CALC", codigo: 0x16, area: CGRect(x: 0.26708, y: 0.45720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "INTEGRAL", codigo: 0x46, area: CGRect(x: 0.63043, y: 0.45720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "VARIABLE", codigo: 0x56, area: CGRect(x: 0.75155, y: 0.45720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "FRACCION", codigo: 0x05, area: CGRect(x: 0.14596, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "RAIZ", codigo: 0x15, area: CGRect(x: 0.26708, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "CUADRADO", codigo: 0x25, area: CGRect(x: 0.38820, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "POTENCIA", codigo: 0x35, area: CGRect(x: 0.50932, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "LOG_B", codigo: 0x45, area: CGRect(x: 0.63043, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "LN", codigo: 0x55, area: CGRect(x: 0.75155, y: 0.50920, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "NEGATIVO", codigo: 0x04, area: CGRect(x: 0.14596, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "GRADOS", codigo: 0x14, area: CGRect(x: 0.26708, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "INVERSO", codigo: 0x24, area: CGRect(x: 0.38820, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "SIN", codigo: 0x34, area: CGRect(x: 0.50932, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "COS", codigo: 0x44, area: CGRect(x: 0.63043, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "TAN", codigo: 0x54, area: CGRect(x: 0.75155, y: 0.55720, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "STO", codigo: 0x03, area: CGRect(x: 0.14596, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "ENG", codigo: 0x13, area: CGRect(x: 0.26708, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "PAREN_ABRE", codigo: 0x23, area: CGRect(x: 0.38820, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "PAREN_CIERRA", codigo: 0x33, area: CGRect(x: 0.50932, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "SD", codigo: 0x43, area: CGRect(x: 0.63043, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "MAS_M", codigo: 0x53, area: CGRect(x: 0.75155, y: 0.61320, width: 0.10248, height: 0.03600)),
            TeclaFisica(nombre: "7", codigo: 0x02, area: CGRect(x: 0.14674, y: 0.67200, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "8", codigo: 0x12, area: CGRect(x: 0.29270, y: 0.67200, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "9", codigo: 0x22, area: CGRect(x: 0.43866, y: 0.67200, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "DEL", codigo: 0x32, area: CGRect(x: 0.58463, y: 0.67200, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "AC", codigo: 0x42, area: CGRect(x: 0.73059, y: 0.67200, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "4", codigo: 0x01, area: CGRect(x: 0.14674, y: 0.73600, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "5", codigo: 0x11, area: CGRect(x: 0.29270, y: 0.73600, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "6", codigo: 0x21, area: CGRect(x: 0.43866, y: 0.73600, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "POR", codigo: 0x31, area: CGRect(x: 0.58463, y: 0.73600, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "ENTRE", codigo: 0x41, area: CGRect(x: 0.73059, y: 0.73600, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "1", codigo: 0x00, area: CGRect(x: 0.14674, y: 0.80000, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "2", codigo: 0x10, area: CGRect(x: 0.29270, y: 0.80000, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "3", codigo: 0x20, area: CGRect(x: 0.43866, y: 0.80000, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "MAS", codigo: 0x30, area: CGRect(x: 0.58463, y: 0.80000, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "MENOS", codigo: 0x40, area: CGRect(x: 0.73059, y: 0.80000, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "0", codigo: 0x64, area: CGRect(x: 0.14674, y: 0.86400, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "COMA", codigo: 0x63, area: CGRect(x: 0.29270, y: 0.86400, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "EXP", codigo: 0x62, area: CGRect(x: 0.43866, y: 0.86400, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "ANS", codigo: 0x61, area: CGRect(x: 0.58463, y: 0.86400, width: 0.12578, height: 0.04480)),
            TeclaFisica(nombre: "IGUAL", codigo: 0x60, area: CGRect(x: 0.73059, y: 0.86400, width: 0.12578, height: 0.04480)),
        ])
}
