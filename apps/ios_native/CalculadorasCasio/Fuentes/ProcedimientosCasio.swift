import Foundation

/// «¿Cómo hago esto en mi calculadora?»
///
/// El problema que resuelve no es matemático. Las dos máquinas hacen lo mismo pero
/// por sitios distintos: matrices es `MODE 6` en la VN PLUS y `MENU 4` en la LA X;
/// estadística es `3` en una y `6` en la otra. Buscar «matriz» y que te diga la ruta
/// de **la que tienes abierta** ahorra abrir el manual.
///
/// Nada de esto está sacado de un PDF: los números salen del menú que muestra cada
/// ROM, leído de la pantalla. `Tools/pruebas/verificar_procedimientos.py` lo vuelve a
/// comprobar contra el firmware, así que si un número estuviera mal, fallaría.
struct PasoRuta {
    /// Nombre lógico, el mismo que usa `Disposiciones`. Sirve para iluminar la tecla
    /// sobre la carátula.
    let tecla: String
    /// Cómo se le llama al usuario. No siempre coincide: la entrada `A` del menú de
    /// la ClassWiz se elige con la tecla `(−)`, que es la que lleva la A en amarillo.
    let rotulo: String

    init(_ tecla: String, rotulo: String? = nil) {
        self.tecla = tecla
        self.rotulo = rotulo ?? tecla
    }
}

struct RutaModelo {
    let pasos: [PasoRuta]
    /// Lo que la propia calculadora escribe en su menú para esta entrada. Es la
    /// prueba de que el número es el correcto, y lo que se compara contra la ROM.
    let confirmacion: String
    /// Indicador de la fila de estado que debe encenderse al llegar, si lo hay.
    let indicador: String?
    /// Qué te vas a encontrar nada más llegar. Varias entradas no entran del todo:
    /// abren un submenú. Encontrarte una pregunta que no esperabas es justo donde
    /// uno se atasca, así que se avisa antes.
    let despues: String?
    /// Solo para la comprobación contra la ROM: lo que habría que pulsar además
    /// para acabar de entrar y poder ver encenderse el indicador de modo.
    let paraComprobar: [String]

    init(_ pasos: [PasoRuta], confirmacion: String, indicador: String? = nil,
         despues: String? = nil, paraComprobar: [String] = []) {
        self.pasos = pasos
        self.confirmacion = confirmacion
        self.indicador = indicador
        self.despues = despues
        self.paraComprobar = paraComprobar
    }

    var teclas: [String] { pasos.map(\.tecla) }
    var comoSeLee: String { pasos.map(\.rotulo).joined(separator: " → ") }
}

struct Procedimiento: Identifiable {
    let id: String
    let nombre: String
    /// Con lo que uno lo busca de verdad, no el nombre oficial.
    let sinonimos: [String]
    let rutas: [String: RutaModelo]

    func ruta(en modelo: String) -> RutaModelo? { rutas[modelo] }
}

enum ProcedimientosCasio {

    /// Atajos del menú de la ClassWiz que no son dígitos: las entradas A, B y C se
    /// eligen con las teclas que llevan esa letra en amarillo. Comprobado sobre la
    /// ROM, no deducido: pulsar `(−)` desde el menú entra en `A:Equation/Func`.
    private static let letraA = PasoRuta("NEGATIVO", rotulo: "A  (la tecla (−))")
    private static let letraB = PasoRuta("GRADOS", rotulo: "B  (la tecla °’”)")
    private static let letraC = PasoRuta("INVERSO", rotulo: "C  (la tecla x⁻¹)")

    private static let modo = PasoRuta("MODE")
    private static let menu = PasoRuta("MENU")

    static let todos: [Procedimiento] = [
        Procedimiento(
            id: "calculo",
            nombre: "Cálculo normal",
            sinonimos: ["comp", "calcular", "volver a lo normal", "salir del modo"],
            rutas: [
                "esplus": RutaModelo([modo, PasoRuta("1")], confirmacion: "1:COMP"),
                "classwiz": RutaModelo([menu, PasoRuta("1")], confirmacion: "1:Calculate"),
            ]),

        Procedimiento(
            id: "complejos",
            nombre: "Números complejos",
            sinonimos: ["complejo", "imaginario", "fasor", "polar", "rectangular", "i"],
            rutas: [
                "esplus": RutaModelo([modo, PasoRuta("2")],
                                     confirmacion: "2:CMPLX", indicador: "CMPLX"),
                "classwiz": RutaModelo([menu, PasoRuta("2")], confirmacion: "2:Complex"),
            ]),

        Procedimiento(
            id: "estadistica",
            nombre: "Estadística",
            sinonimos: ["desviación estándar", "desviacion tipica", "media", "promedio",
                        "varianza", "regresión", "stat"],
            rutas: [
                "esplus": RutaModelo(
                    [modo, PasoRuta("3")], confirmacion: "3:STAT", indicador: "STAT",
                    despues: "Pregunta el tipo: 1:1-VAR para una variable, "
                             + "2:A+BX para regresión lineal.",
                    paraComprobar: ["1"]),
                "classwiz": RutaModelo(
                    [menu, PasoRuta("6")], confirmacion: "6:Statistics",
                    despues: "Pregunta el modelo: 1:1-Variable, 2:y=a+bx, "
                             + "3:y=a+bx+cx², 4:y=a+b·ln(x)…"),
            ]),

        Procedimiento(
            id: "basen",
            nombre: "Binario, octal y hexadecimal",
            sinonimos: ["base", "binario", "hexadecimal", "octal", "base-n", "bits"],
            rutas: [
                "esplus": RutaModelo([modo, PasoRuta("4")], confirmacion: "4:BASE-N"),
                "classwiz": RutaModelo([menu, PasoRuta("3")], confirmacion: "3:Base-N"),
            ]),

        Procedimiento(
            id: "ecuaciones",
            nombre: "Ecuaciones y sistemas",
            sinonimos: ["ecuacion", "sistema", "polinomio", "raices", "cuadrática",
                        "eqn", "despejar"],
            rutas: [
                "esplus": RutaModelo([modo, PasoRuta("5")], confirmacion: "5:EQN"),
                "classwiz": RutaModelo([menu, letraA], confirmacion: "A:Equation/Func"),
            ]),

        Procedimiento(
            id: "matrices",
            nombre: "Matrices",
            sinonimos: ["matriz", "determinante", "inversa", "mat"],
            rutas: [
                "esplus": RutaModelo(
                    [modo, PasoRuta("6")], confirmacion: "6:MATRIX", indicador: "MAT",
                    despues: "Pregunta qué matriz definir (1:MatA, 2:MatB, 3:MatC) "
                             + "y luego sus dimensiones (1:3×3, 2:3×2…).",
                    paraComprobar: ["1", "1"]),
                "classwiz": RutaModelo(
                    [menu, PasoRuta("4")], confirmacion: "4:Matrix",
                    despues: "Define Matrix: 1:MatA, 2:MatB, 3:MatC, 4:MatD."),
            ]),

        Procedimiento(
            id: "tabla",
            nombre: "Tabla de valores",
            sinonimos: ["tabla", "table", "tabular", "f(x)"],
            rutas: [
                "esplus": RutaModelo([modo, PasoRuta("7")], confirmacion: "7:TABLE"),
                "classwiz": RutaModelo([menu, PasoRuta("9")], confirmacion: "9:Table"),
            ]),

        Procedimiento(
            id: "vectores",
            nombre: "Vectores",
            sinonimos: ["vector", "producto escalar", "producto vectorial", "vct"],
            rutas: [
                "esplus": RutaModelo(
                    [modo, PasoRuta("8")], confirmacion: "8:VECTOR", indicador: "VCT",
                    despues: "Pregunta qué vector definir (1:VctA, 2:VctB, 3:VctC) "
                             + "y luego su dimensión (1:3, 2:2).",
                    paraComprobar: ["1", "1"]),
                "classwiz": RutaModelo(
                    [menu, PasoRuta("5")], confirmacion: "5:Vector",
                    despues: "Define Vector: 1:VctA, 2:VctB, 3:VctC, 4:VctD."),
            ]),

        // Las cuatro siguientes no existen en la VN PLUS. Decirlo es parte del
        // servicio: perder diez minutos buscando un menú que no está es peor que
        // saberlo de entrada.
        Procedimiento(
            id: "distribuciones",
            nombre: "Distribuciones",
            sinonimos: ["normal", "binomial", "poisson", "distribucion", "probabilidad"],
            rutas: ["classwiz": RutaModelo(
                [menu, PasoRuta("7")], confirmacion: "7:Distribution",
                despues: "1:Normal PD, 2:Normal CD, 3:Inverse Normal, "
                         + "4:Binomial PD…")]),

        Procedimiento(
            id: "hoja",
            nombre: "Hoja de cálculo",
            sinonimos: ["hoja", "spreadsheet", "celdas", "tabla de celdas"],
            rutas: ["classwiz": RutaModelo(
                [menu, PasoRuta("8")], confirmacion: "8:Spreadsheet",
                despues: "Ojo al salir: aquí AC no te saca, borra la celda. "
                         + "Para volver, MENU directamente.")]),

        Procedimiento(
            id: "inecuaciones",
            nombre: "Inecuaciones",
            sinonimos: ["inecuacion", "desigualdad", "mayor que", "menor que"],
            rutas: ["classwiz": RutaModelo([menu, letraB],
                                           confirmacion: "B:Inequality")]),

        Procedimiento(
            id: "proporciones",
            nombre: "Proporciones",
            sinonimos: ["proporcion", "regla de tres", "ratio", "a:b"],
            rutas: ["classwiz": RutaModelo([menu, letraC], confirmacion: "C:Ratio")]),
    ]

    /// Busca sin distinguir mayúsculas ni tildes: se escribe «desviacion» con prisa.
    static func buscar(_ texto: String) -> [Procedimiento] {
        let aguja = normalizar(texto)
        guard !aguja.isEmpty else { return todos }
        return todos.filter { procedimiento in
            if normalizar(procedimiento.nombre).contains(aguja) { return true }
            return procedimiento.sinonimos.contains { normalizar($0).contains(aguja) }
        }
    }

    static func normalizar(_ texto: String) -> String {
        texto.folding(options: [.diacriticInsensitive, .caseInsensitive], locale: nil)
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
