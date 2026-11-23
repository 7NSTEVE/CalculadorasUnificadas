import 'dart:ui';

enum CasioCalcKind {
  cg50,
  cw2,
  classwiz,
  naturalEs,
}

class CasioKeyDef {
  final int id;
  final double cx;
  final double cy;
  final double w;
  final double h;
  final int matrixCode;
  final String name;
  final String displayLabel;

  const CasioKeyDef(
    this.id,
    this.cx,
    this.cy,
    this.w,
    this.h,
    this.matrixCode,
    this.name,
    this.displayLabel,
  );
}

class CasioSkinGeometry {
  final CasioCalcKind kind;
  final String shortLabel;
  final String title;
  final String subtitle;
  final String assetPath;
  final double skinWidth;
  final double skinHeight;
  final Rect lcdRect;
  final int pixelWidth;
  final int pixelHeight;
  final Color lcdBackground;
  final Color lcdForeground;
  final bool hasTopIndicatorStrip;
  final bool isCw2IndicatorLayout;
  final List<CasioKeyDef> keys;

  const CasioSkinGeometry({
    required this.kind,
    required this.shortLabel,
    required this.title,
    required this.subtitle,
    required this.assetPath,
    required this.skinWidth,
    required this.skinHeight,
    required this.lcdRect,
    required this.pixelWidth,
    required this.pixelHeight,
    required this.lcdBackground,
    required this.lcdForeground,
    required this.hasTopIndicatorStrip,
    this.isCw2IndicatorLayout = false,
    required this.keys,
  });

  CasioKeyDef? hitTest(double localX, double localY, Size renderSize) {
    if (renderSize.width <= 0 || renderSize.height <= 0) return null;
    final double sx = localX * (skinWidth / renderSize.width);
    final double sy = localY * (skinHeight / renderSize.height);
    for (final k in keys) {
      if ((sx - k.cx).abs() <= k.w * 0.5 && (sy - k.cy).abs() <= k.h * 0.5) {
        return k;
      }
    }
    CasioKeyDef? best;
    double bestDist = double.infinity;
    final double margin = skinWidth >= 900 ? 10.0 : 6.0;
    for (final k in keys) {
      final dx = (sx - k.cx).abs() - k.w * 0.5;
      final dy = (sy - k.cy).abs() - k.h * 0.5;
      if (dx <= margin && dy <= margin) {
        final d = (sx - k.cx) * (sx - k.cx) + (sy - k.cy) * (sy - k.cy);
        if (d < bestDist) {
          bestDist = d;
          best = k;
        }
      }
    }
    return best;
  }
}

class CasioIndicatorBits {
  static const int shift = 1 << 0;
  static const int alpha = 1 << 1;
  static const int m = 1 << 2;
  static const int sto = 1 << 3;
  static const int math = 1 << 4;
  static const int deg = 1 << 5;
  static const int rad = 1 << 6;
  static const int gra = 1 << 7;
  static const int fix = 1 << 8;
  static const int sci = 1 << 9;
  static const int down = 1 << 10;
  static const int up = 1 << 11;
}

class CasioCw2IndicatorBits {
  static const int shift = 1 << 0;
  static const int math = 1 << 1;
  static const int deg = 1 << 2;
  static const int rad = 1 << 3;
  static const int gra = 1 << 4;
  static const int fix = 1 << 5;
  static const int sci = 1 << 6;
  static const int fx = 1 << 7;
  static const int eng = 1 << 8;
  static const int cmplx = 1 << 9;
  static const int angle = 1 << 10;
  static const int wdown = 1 << 11;
  static const int verify = 1 << 12;
  static const int gx = 1 << 13;
  static const int left = 1 << 14;
  static const int down = 1 << 15;
  static const int up = 1 << 16;
  static const int right = 1 << 17;
  static const int pause = 1 << 18;
  static const int sun = 1 << 19;
}

// Authoritative fx-991CW 48-key geometry (from cw2_keys_exact.h, 992x2082 faceplate)
const List<CasioKeyDef> kCw2Keys = [
  CasioKeyDef(0, 132, 795, 90, 90, 0xFF, 'ON', 'ON'),
  CasioKeyDef(1, 278, 797, 96, 96, 0x17, 'HOME', 'HOME'),
  CasioKeyDef(2, 570, 797, 96, 96, 0x37, 'ARRIBA', '▲'),
  CasioKeyDef(3, 862, 811, 96, 110, 0x57, 'RE_PAG', '⇞'),
  CasioKeyDef(4, 132, 939, 96, 96, 0x06, 'SETTINGS', 'SETTINGS'),
  CasioKeyDef(5, 278, 939, 96, 96, 0x16, 'ATRAS', 'BACK'),
  CasioKeyDef(6, 424, 939, 96, 96, 0x26, 'IZQUIERDA', '◀'),
  CasioKeyDef(7, 570, 939, 96, 96, 0x36, 'OK', 'OK'),
  CasioKeyDef(8, 716, 939, 96, 96, 0x46, 'DERECHA', '▶'),
  CasioKeyDef(9, 862, 939, 96, 96, 0x56, 'AV_PAG', '⇟'),
  CasioKeyDef(10, 570, 1065, 96, 96, 0x35, 'ABAJO', '▼'),
  CasioKeyDef(11, 132, 1087, 96, 96, 0x05, 'SHIFT', 'SHIFT'),
  CasioKeyDef(12, 278, 1087, 96, 96, 0x15, 'VARIABLE', 'VAR'),
  CasioKeyDef(13, 424, 1087, 96, 96, 0x25, 'FUNCTION', 'f(x)'),
  CasioKeyDef(14, 716, 1087, 96, 96, 0x45, 'CATALOG', 'CATALOG'),
  CasioKeyDef(15, 862, 1087, 96, 96, 0x55, 'TOOLS', 'TOOLS'),
  CasioKeyDef(16, 132, 1222, 94, 94, 0x04, 'X_VAR', 'x'),
  CasioKeyDef(17, 278, 1222, 94, 94, 0x14, 'FRACCION', '□/□'),
  CasioKeyDef(18, 424, 1222, 94, 94, 0x24, 'RAIZ', '√'),
  CasioKeyDef(19, 570, 1222, 94, 94, 0x34, 'POTENCIA', 'xⁿ'),
  CasioKeyDef(20, 716, 1222, 94, 94, 0x44, 'CUADRADO', 'x²'),
  CasioKeyDef(21, 862, 1222, 94, 94, 0x54, 'LOG_B', 'log'),
  CasioKeyDef(22, 132, 1350, 94, 94, 0x03, 'ANS', 'Ans'),
  CasioKeyDef(23, 278, 1350, 94, 94, 0x13, 'SIN', 'sin'),
  CasioKeyDef(24, 424, 1350, 94, 94, 0x23, 'COS', 'cos'),
  CasioKeyDef(25, 570, 1350, 94, 94, 0x33, 'TAN', 'tan'),
  CasioKeyDef(26, 716, 1350, 94, 94, 0x43, 'PAREN_ABRE', '('),
  CasioKeyDef(27, 862, 1350, 94, 94, 0x53, 'PAREN_CIERRA', ')'),
  CasioKeyDef(28, 139, 1493, 122, 122, 0x02, '7', '7'),
  CasioKeyDef(29, 318, 1493, 122, 122, 0x12, '8', '8'),
  CasioKeyDef(30, 497, 1493, 122, 122, 0x22, '9', '9'),
  CasioKeyDef(31, 676, 1493, 122, 122, 0x32, 'DEL', 'DEL'),
  CasioKeyDef(32, 855, 1493, 122, 122, 0x42, 'AC', 'AC'),
  CasioKeyDef(33, 139, 1643, 122, 122, 0x01, '4', '4'),
  CasioKeyDef(34, 318, 1643, 122, 122, 0x11, '5', '5'),
  CasioKeyDef(35, 497, 1643, 122, 122, 0x21, '6', '6'),
  CasioKeyDef(36, 676, 1643, 122, 122, 0x31, 'POR', '×'),
  CasioKeyDef(37, 855, 1643, 122, 122, 0x41, 'ENTRE', '÷'),
  CasioKeyDef(38, 139, 1793, 122, 122, 0x00, '1', '1'),
  CasioKeyDef(39, 318, 1793, 122, 122, 0x10, '2', '2'),
  CasioKeyDef(40, 497, 1793, 122, 122, 0x20, '3', '3'),
  CasioKeyDef(41, 676, 1793, 122, 122, 0x30, 'MAS', '+'),
  CasioKeyDef(42, 855, 1793, 122, 122, 0x40, 'MENOS', '-'),
  CasioKeyDef(43, 139, 1943, 122, 122, 0x64, '0', '0'),
  CasioKeyDef(44, 318, 1943, 122, 122, 0x63, 'PUNTO', '.'),
  CasioKeyDef(45, 497, 1943, 122, 122, 0x62, 'EXP', '×10ˣ'),
  CasioKeyDef(46, 676, 1943, 122, 122, 0x61, 'FORMAT', 'FORMAT'),
  CasioKeyDef(47, 855, 1943, 122, 122, 0x60, 'EXE', 'EXE'),
];

// Authoritative ClassWiz (fx-570LA X) 50-key geometry
const List<CasioKeyDef> kClasswizKeys = [
  CasioKeyDef(0, 123, 503, 57, 57, 0x07, 'SHIFT', 'SHIFT'),
  CasioKeyDef(1, 192, 503, 57, 57, 0x17, 'ALPHA', 'ALPHA'),
  CasioKeyDef(2, 322, 495, 58, 46, 0x27, 'ARRIBA', '▲'),
  CasioKeyDef(3, 258, 535, 48, 58, 0x26, 'IZQUIERDA', '◀'),
  CasioKeyDef(4, 386, 535, 48, 58, 0x37, 'DERECHA', '▶'),
  CasioKeyDef(5, 322, 575, 58, 46, 0x36, 'ABAJO', '▼'),
  CasioKeyDef(6, 450, 503, 57, 57, 0x47, 'MENU', 'MENU'),
  CasioKeyDef(7, 520, 503, 57, 57, 0xFF, 'ON', 'ON'),
  CasioKeyDef(8, 127, 594, 66, 45, 0x06, 'OPTN', 'OPTN'),
  CasioKeyDef(9, 205, 594, 66, 45, 0x16, 'CALC', 'CALC'),
  CasioKeyDef(10, 439, 594, 66, 45, 0x46, 'INTEGRAL', '∫dx'),
  CasioKeyDef(11, 517, 594, 66, 45, 0x56, 'VARIABLE', 'x'),
  CasioKeyDef(12, 127, 659, 66, 45, 0x05, 'FRACCION', '□/□'),
  CasioKeyDef(13, 205, 659, 66, 45, 0x15, 'RAIZ', '√'),
  CasioKeyDef(14, 283, 659, 66, 45, 0x25, 'CUADRADO', 'x²'),
  CasioKeyDef(15, 361, 659, 66, 45, 0x35, 'POTENCIA', 'xⁿ'),
  CasioKeyDef(16, 439, 659, 66, 45, 0x45, 'LOG_B', 'logₐb'),
  CasioKeyDef(17, 517, 659, 66, 45, 0x55, 'LN', 'ln'),
  CasioKeyDef(18, 127, 719, 66, 45, 0x04, 'NEGATIVO', '(-)'),
  CasioKeyDef(19, 205, 719, 66, 45, 0x14, 'GRADOS', '°′″'),
  CasioKeyDef(20, 283, 719, 66, 45, 0x24, 'INVERSO', 'x⁻¹'),
  CasioKeyDef(21, 361, 719, 66, 45, 0x34, 'SIN', 'sin'),
  CasioKeyDef(22, 439, 719, 66, 45, 0x44, 'COS', 'cos'),
  CasioKeyDef(23, 517, 719, 66, 45, 0x54, 'TAN', 'tan'),
  CasioKeyDef(24, 127, 789, 66, 45, 0x03, 'STO', 'STO'),
  CasioKeyDef(25, 205, 789, 66, 45, 0x13, 'ENG', 'ENG'),
  CasioKeyDef(26, 283, 789, 66, 45, 0x23, 'PAREN_ABRE', '('),
  CasioKeyDef(27, 361, 789, 66, 45, 0x33, 'PAREN_CIERRA', ')'),
  CasioKeyDef(28, 439, 789, 66, 45, 0x43, 'SD', 'S⇔D'),
  CasioKeyDef(29, 517, 789, 66, 45, 0x53, 'MAS_M', 'M+'),
  CasioKeyDef(30, 135, 868, 81, 56, 0x02, '7', '7'),
  CasioKeyDef(31, 229, 868, 81, 56, 0x12, '8', '8'),
  CasioKeyDef(32, 323, 868, 81, 56, 0x22, '9', '9'),
  CasioKeyDef(33, 417, 868, 81, 56, 0x32, 'DEL', 'DEL'),
  CasioKeyDef(34, 511, 868, 81, 56, 0x42, 'AC', 'AC'),
  CasioKeyDef(35, 135, 948, 81, 56, 0x01, '4', '4'),
  CasioKeyDef(36, 229, 948, 81, 56, 0x11, '5', '5'),
  CasioKeyDef(37, 323, 948, 81, 56, 0x21, '6', '6'),
  CasioKeyDef(38, 417, 948, 81, 56, 0x31, 'POR', '×'),
  CasioKeyDef(39, 511, 948, 81, 56, 0x41, 'ENTRE', '÷'),
  CasioKeyDef(40, 135, 1028, 81, 56, 0x00, '1', '1'),
  CasioKeyDef(41, 229, 1028, 81, 56, 0x10, '2', '2'),
  CasioKeyDef(42, 323, 1028, 81, 56, 0x20, '3', '3'),
  CasioKeyDef(43, 417, 1028, 81, 56, 0x30, 'MAS', '+'),
  CasioKeyDef(44, 511, 1028, 81, 56, 0x40, 'MENOS', '-'),
  CasioKeyDef(45, 135, 1108, 81, 56, 0x64, '0', '0'),
  CasioKeyDef(46, 229, 1108, 81, 56, 0x63, 'COMA', '.'),
  CasioKeyDef(47, 323, 1108, 81, 56, 0x62, 'EXP', '×10ˣ'),
  CasioKeyDef(48, 417, 1108, 81, 56, 0x61, 'ANS', 'Ans'),
  CasioKeyDef(49, 511, 1108, 81, 56, 0x60, 'IGUAL', '='),
];

// Authoritative fx-570VN PLUS (Natural-V.P.A.M.) 50-key geometry
const List<CasioKeyDef> kGrayEsKeys = [
  CasioKeyDef(0, 66, 313, 52, 42, 0x07, 'SHIFT', 'SHIFT'),
  CasioKeyDef(1, 341, 313, 52, 42, 0xFF, 'ON', 'ON'),
  CasioKeyDef(2, 202, 310, 31, 31, 0x27, 'ARRIBA', '▲'),
  CasioKeyDef(3, 120, 321, 52, 42, 0x17, 'ALPHA', 'ALPHA'),
  CasioKeyDef(4, 287, 321, 52, 42, 0x47, 'MODE', 'MODE'),
  CasioKeyDef(5, 168, 338, 31, 31, 0x26, 'IZQUIERDA', '◀'),
  CasioKeyDef(6, 236, 338, 31, 31, 0x37, 'DERECHA', '▶'),
  CasioKeyDef(7, 202, 366, 31, 31, 0x36, 'ABAJO', '▼'),
  CasioKeyDef(8, 63, 379, 53, 33, 0x06, 'CALC', 'CALC'),
  CasioKeyDef(9, 117, 379, 53, 33, 0x16, 'INTEGRAL', '∫dx'),
  CasioKeyDef(10, 289, 379, 51, 33, 0x46, 'INVERSO', 'x⁻¹'),
  CasioKeyDef(11, 342, 379, 51, 33, 0x56, 'LOG_B', 'logₐb'),
  CasioKeyDef(12, 68, 426, 53, 33, 0x05, 'FRACCION', '□/□'),
  CasioKeyDef(13, 122, 426, 53, 33, 0x15, 'RAIZ', '√'),
  CasioKeyDef(14, 176, 426, 53, 33, 0x25, 'CUADRADO', 'x²'),
  CasioKeyDef(15, 230, 426, 53, 33, 0x35, 'POTENCIA', 'xⁿ'),
  CasioKeyDef(16, 284, 426, 53, 33, 0x45, 'LOG', 'log'),
  CasioKeyDef(17, 338, 426, 53, 33, 0x55, 'LN', 'ln'),
  CasioKeyDef(18, 68, 472, 53, 33, 0x04, 'NEGATIVO', '(-)'),
  CasioKeyDef(19, 122, 472, 53, 33, 0x14, 'GRADOS', '°′″'),
  CasioKeyDef(20, 176, 472, 53, 33, 0x24, 'HYP', 'hyp'),
  CasioKeyDef(21, 230, 472, 53, 33, 0x34, 'SIN', 'sin'),
  CasioKeyDef(22, 284, 472, 53, 33, 0x44, 'COS', 'cos'),
  CasioKeyDef(23, 338, 472, 53, 33, 0x54, 'TAN', 'tan'),
  CasioKeyDef(24, 68, 518, 53, 33, 0x03, 'RCL', 'RCL'),
  CasioKeyDef(25, 122, 518, 53, 33, 0x13, 'ENG', 'ENG'),
  CasioKeyDef(26, 176, 518, 53, 33, 0x23, 'PAREN_ABRE', '('),
  CasioKeyDef(27, 230, 518, 53, 33, 0x33, 'PAREN_CIERRA', ')'),
  CasioKeyDef(28, 284, 518, 53, 33, 0x43, 'SD', 'S⇔D'),
  CasioKeyDef(29, 338, 518, 53, 33, 0x53, 'MAS_M', 'M+'),
  CasioKeyDef(30, 73, 571, 64, 46, 0x02, '7', '7'),
  CasioKeyDef(31, 138, 571, 64, 46, 0x12, '8', '8'),
  CasioKeyDef(32, 203, 571, 64, 46, 0x22, '9', '9'),
  CasioKeyDef(33, 268, 571, 64, 46, 0x32, 'DEL', 'DEL'),
  CasioKeyDef(34, 333, 571, 64, 46, 0x42, 'AC', 'AC'),
  CasioKeyDef(35, 73, 628, 64, 46, 0x01, '4', '4'),
  CasioKeyDef(36, 138, 628, 64, 46, 0x11, '5', '5'),
  CasioKeyDef(37, 203, 628, 64, 46, 0x21, '6', '6'),
  CasioKeyDef(38, 268, 628, 64, 46, 0x31, 'POR', '×'),
  CasioKeyDef(39, 333, 628, 64, 46, 0x41, 'ENTRE', '÷'),
  CasioKeyDef(40, 73, 685, 64, 46, 0x00, '1', '1'),
  CasioKeyDef(41, 138, 685, 64, 46, 0x10, '2', '2'),
  CasioKeyDef(42, 203, 685, 64, 46, 0x20, '3', '3'),
  CasioKeyDef(43, 268, 685, 64, 46, 0x30, 'MAS', '+'),
  CasioKeyDef(44, 333, 685, 64, 46, 0x40, 'MENOS', '-'),
  CasioKeyDef(45, 73, 742, 64, 46, 0x64, '0', '0'),
  CasioKeyDef(46, 138, 742, 64, 46, 0x63, 'PUNTO', '.'),
  CasioKeyDef(47, 203, 742, 64, 46, 0x62, 'EXP', '×10ˣ'),
  CasioKeyDef(48, 268, 742, 64, 46, 0x61, 'ANS', 'Ans'),
  CasioKeyDef(49, 333, 742, 64, 46, 0x60, 'IGUAL', '='),
];

// Authoritative fx-CG50 PRIZM 50-key geometry (16-bit KIU matrix codes)
const List<CasioKeyDef> kCg50Keys = [
  CasioKeyDef(0, 112, 559, 54, 48, 0x070A, 'F1', 'F1'),
  CasioKeyDef(1, 200, 559, 54, 48, 0x060A, 'F2', 'F2'),
  CasioKeyDef(2, 288, 559, 54, 48, 0x050A, 'F3', 'F3'),
  CasioKeyDef(3, 376, 559, 54, 48, 0x040A, 'F4', 'F4'),
  CasioKeyDef(4, 464, 559, 54, 48, 0x030A, 'F5', 'F5'),
  CasioKeyDef(5, 552, 559, 54, 48, 0x020A, 'F6', 'F6'),
  CasioKeyDef(6, 115, 642, 64, 42, 0x0709, 'SHIFT', 'SHIFT'),
  CasioKeyDef(7, 202, 642, 64, 42, 0x0609, 'OPTN', 'OPTN'),
  CasioKeyDef(8, 289, 642, 64, 42, 0x0509, 'VARS', 'VARS'),
  CasioKeyDef(9, 376, 642, 64, 42, 0x0409, 'MENU', 'MENU'),
  CasioKeyDef(10, 512, 628, 52, 42, 0x0209, 'ARRIBA', '▲'),
  CasioKeyDef(11, 460, 676, 42, 52, 0x0309, 'IZQUIERDA', '◀'),
  CasioKeyDef(12, 564, 676, 42, 52, 0x0208, 'DERECHA', '▶'),
  CasioKeyDef(13, 512, 724, 52, 42, 0x0308, 'ABAJO', '▼'),
  CasioKeyDef(14, 115, 711, 64, 42, 0x0708, 'ALPHA', 'ALPHA'),
  CasioKeyDef(15, 202, 711, 64, 42, 0x0608, 'X^2', 'x²'),
  CasioKeyDef(16, 289, 711, 64, 42, 0x0508, 'POT', '^'),
  CasioKeyDef(17, 376, 711, 64, 42, 0x0408, 'EXIT', 'EXIT'),
  CasioKeyDef(18, 112, 789, 64, 42, 0x0707, 'X_THETA', 'X,θ,T'),
  CasioKeyDef(19, 200, 789, 64, 42, 0x0607, 'LOG', 'log'),
  CasioKeyDef(20, 288, 789, 64, 42, 0x0507, 'LN', 'ln'),
  CasioKeyDef(21, 376, 789, 64, 42, 0x0407, 'SIN', 'sin'),
  CasioKeyDef(22, 464, 789, 64, 42, 0x0307, 'COS', 'cos'),
  CasioKeyDef(23, 552, 789, 64, 42, 0x0207, 'TAN', 'tan'),
  CasioKeyDef(24, 112, 858, 64, 42, 0x0706, 'FRAC', '□/□'),
  CasioKeyDef(25, 200, 858, 64, 42, 0x0606, 'SD', 'F⇔D'),
  CasioKeyDef(26, 288, 858, 64, 42, 0x0506, 'PAREN_A', '('),
  CasioKeyDef(27, 376, 858, 64, 42, 0x0406, 'PAREN_C', ')'),
  CasioKeyDef(28, 464, 858, 64, 42, 0x0306, 'COMA', ','),
  CasioKeyDef(29, 552, 858, 64, 42, 0x0206, 'FLECHA', '→'),
  CasioKeyDef(30, 121, 956, 88, 56, 0x0705, '7', '7'),
  CasioKeyDef(31, 226, 956, 88, 56, 0x0605, '8', '8'),
  CasioKeyDef(32, 332, 956, 88, 56, 0x0505, '9', '9'),
  CasioKeyDef(33, 437, 956, 88, 56, 0x0405, 'DEL', 'DEL'),
  CasioKeyDef(34, 542, 956, 88, 56, 0x0101, 'AC', 'AC/ON'),
  CasioKeyDef(35, 121, 1038, 88, 56, 0x0704, '4', '4'),
  CasioKeyDef(36, 226, 1038, 88, 56, 0x0604, '5', '5'),
  CasioKeyDef(37, 332, 1038, 88, 56, 0x0504, '6', '6'),
  CasioKeyDef(38, 437, 1038, 88, 56, 0x0404, 'POR', '×'),
  CasioKeyDef(39, 542, 1038, 88, 56, 0x0304, 'ENTRE', '÷'),
  CasioKeyDef(40, 121, 1120, 88, 56, 0x0703, '1', '1'),
  CasioKeyDef(41, 226, 1120, 88, 56, 0x0603, '2', '2'),
  CasioKeyDef(42, 332, 1120, 88, 56, 0x0503, '3', '3'),
  CasioKeyDef(43, 437, 1120, 88, 56, 0x0403, 'MAS', '+'),
  CasioKeyDef(44, 542, 1120, 88, 56, 0x0303, 'MENOS', '-'),
  CasioKeyDef(45, 121, 1202, 88, 56, 0x0702, '0', '0'),
  CasioKeyDef(46, 226, 1202, 88, 56, 0x0602, 'PUNTO', '.'),
  CasioKeyDef(47, 332, 1202, 88, 56, 0x0502, 'EXP', '×10ˣ'),
  CasioKeyDef(48, 437, 1202, 88, 56, 0x0402, 'ANS', '(-)'),
  CasioKeyDef(49, 542, 1202, 88, 56, 0x0302, 'EXE', 'EXE'),
];

const CasioSkinGeometry kCg50Skin = CasioSkinGeometry(
  kind: CasioCalcKind.cg50,
  shortLabel: 'CG50',
  title: 'fx-CG50',
  subtitle: 'Gráfica a color',
  assetPath: 'assets_staged/skins/caratula_cg50.png',
  skinWidth: 664,
  skinHeight: 1364,
  lcdRect: Rect.fromLTRB(92, 153, 572, 403),
  pixelWidth: 384,
  pixelHeight: 216,
  lcdBackground: Color(0xFFFFFFFF),
  lcdForeground: Color(0xFF101820),
  hasTopIndicatorStrip: false,
  keys: kCg50Keys,
);

const CasioSkinGeometry kCw2Skin = CasioSkinGeometry(
  kind: CasioCalcKind.cw2,
  shortLabel: '991CW',
  title: 'fx-991CW',
  subtitle: 'ClassWiz II',
  assetPath: 'assets_staged/skins/caratula_cw2.png',
  skinWidth: 992,
  skinHeight: 2082,
  lcdRect: Rect.fromLTRB(112, 318, 880, 610),
  pixelWidth: 192,
  pixelHeight: 63,
  lcdBackground: Color(0xFFE4EBE4),
  lcdForeground: Color(0xFF14243A),
  hasTopIndicatorStrip: true,
  isCw2IndicatorLayout: true,
  keys: kCw2Keys,
);

const CasioSkinGeometry kClasswizSkin = CasioSkinGeometry(
  kind: CasioCalcKind.classwiz,
  shortLabel: '570LA X',
  title: 'fx-570LA X',
  subtitle: 'ClassWiz',
  assetPath: 'assets_staged/skins/caratula_classwiz.png',
  skinWidth: 644,
  skinHeight: 1250,
  lcdRect: Rect.fromLTRB(130, 256, 514, 382),
  pixelWidth: 192,
  pixelHeight: 63,
  lcdBackground: Color(0xFFD0DEC8),
  lcdForeground: Color(0xFF1C2638),
  hasTopIndicatorStrip: true,
  keys: kClasswizKeys,
);

const CasioSkinGeometry kNaturalEsSkin = CasioSkinGeometry(
  kind: CasioCalcKind.naturalEs,
  shortLabel: '570VN+',
  title: 'fx-570VN PLUS',
  subtitle: 'Natural-V.P.A.M.',
  assetPath: 'assets_staged/skins/caratula_esplus.png',
  skinWidth: 405,
  skinHeight: 816,
  lcdRect: Rect.fromLTRB(58, 144, 346, 237),
  pixelWidth: 96,
  pixelHeight: 31,
  lcdBackground: Color(0xFFCDD9CD),
  lcdForeground: Color(0xFF192332),
  hasTopIndicatorStrip: true,
  keys: kGrayEsKeys,
);

const List<CasioSkinGeometry> kAllSkins = [
  kCg50Skin,
  kCw2Skin,
  kClasswizSkin,
  kNaturalEsSkin,
];

CasioSkinGeometry skinForKind(CasioCalcKind kind) {
  switch (kind) {
    case CasioCalcKind.cg50:
      return kCg50Skin;
    case CasioCalcKind.cw2:
      return kCw2Skin;
    case CasioCalcKind.classwiz:
      return kClasswizSkin;
    case CasioCalcKind.naturalEs:
      return kNaturalEsSkin;
  }
}
