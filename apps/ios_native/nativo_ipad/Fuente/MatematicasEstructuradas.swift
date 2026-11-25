import UIKit

enum OperacionMatematicaEstructurada: String, CaseIterable {
    case raiz = "Resolver raíz numérica"
    case limite = "Límite bilateral/lateral"
    case sumatoria = "Sumatoria o serie finita"
    case determinante = "Determinante"
    case inversa = "Matriz inversa"
    case sistema = "Sistema de ecuaciones lineales"
    case punto = "Producto punto"
    case cruz = "Producto cruz"
    case fourier = "Transformada discreta de Fourier"
    case laplace = "Transformada de Laplace numérica"
    case edo = "Ecuación diferencial de primer orden"
    case dimensiones = "Verificación dimensional"
    case latex = "LaTeX y MathML sin escribir código"
}

final class ControladorMatematicasEstructuradas: UITableViewController {
    var alInsertar: ((String) -> Void)?
    private let variables: () -> [VariableMatematica]

    init(variables: @escaping () -> [VariableMatematica]) {
        self.variables = variables
        super.init(style: .insetGrouped)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Matemática estructurada"
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "operacion")
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        OperacionMatematicaEstructurada.allCases.count
    }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = tableView.dequeueReusableCell(withIdentifier: "operacion", for: indexPath)
        celda.textLabel?.text = OperacionMatematicaEstructurada.allCases[indexPath.row].rawValue
        celda.accessoryType = .disclosureIndicator; return celda
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        ejecutar(OperacionMatematicaEstructurada.allCases[indexPath.row])
    }

    private func ejecutar(_ operacion: OperacionMatematicaEstructurada) {
        switch operacion {
        case .raiz:
            pedir(operacion.rawValue, ["f(x)", "estimación"], ["x^2-2", "1"]) { [weak self] v in
                guard let self, let x = numero(v[1]) else { return }
                self.mostrar(self.escalar(MotorMatematicoLocal.buscarRaiz(
                    v[0], estimacion: x, variables: self.variables()), prefijo: "x = "))
            }
        case .limite:
            pedir(operacion.rawValue, ["f(x)", "punto", "lado: -1, 0 o 1"],
                  ["sin(x)/x", "0", "0"]) { [weak self] v in
                guard let self, let x = numero(v[1]), let lado = Int(v[2]) else { return }
                self.mostrar(self.escalar(MotorMatematicoLocal.limite(
                    v[0], punto: x, lado: lado, variables: self.variables()), prefijo: "límite = "))
            }
        case .sumatoria:
            pedir(operacion.rawValue, ["término en n", "n inicial", "n final"],
                  ["1/n^2", "1", "1000"]) { [weak self] v in
                guard let self, let a = Int64(v[1]), let b = Int64(v[2]) else { return }
                self.mostrar(self.escalar(MotorMatematicoLocal.sumatoria(
                    v[0], inicio: a, fin: b, variables: self.variables()), prefijo: "Σ = "))
            }
        case .determinante:
            pedirMatriz(titulo: operacion.rawValue) { [weak self] matriz in
                guard let self else { return }
                self.mostrar(self.escalar(MotorMatematicoLocal.determinante(matriz), prefijo: "det = "))
            }
        case .inversa:
            pedirMatriz(titulo: operacion.rawValue) { [weak self] matriz in
                let r = MotorMatematicoLocal.inversa(matriz)
                self?.mostrar(r.0.map(formatearMatriz) ?? "Error: \(r.1 ?? "desconocido")")
            }
        case .sistema:
            pedir(operacion.rawValue, ["filas A|b separadas por ;"],
                  ["2,1|5; 1,-1|1"]) { [weak self] v in
                guard let self, let sistema = self.leerSistema(v[0]) else {
                    self?.mostrar("Formato inválido. Use a,b|c; d,e|f."); return
                }
                let r = MotorMatematicoLocal.resolverSistema(sistema.0, terminos: sistema.1)
                self.mostrar(r.0.map { $0.enumerated().map {
                    "x\($0.offset + 1) = \($0.element.texto())" }.joined(separator: "\n")
                } ?? "Error: \(r.1 ?? "desconocido")")
            }
        case .punto, .cruz:
            pedir(operacion.rawValue, ["vector A", "vector B"], ["1,2,3", "4,5,6"]) {
                [weak self] v in
                guard let self, let a = self.leerVector(v[0]), let b = self.leerVector(v[1]) else { return }
                if operacion == .punto {
                    self.mostrar(self.escalar(MotorMatematicoLocal.productoPunto(a, b), prefijo: "A·B = "))
                } else {
                    let r = MotorMatematicoLocal.productoCruz(a, b)
                    self.mostrar(r.0.map { "A×B = [\($0.map { $0.texto() }.joined(separator: ", "))]" }
                        ?? "Error: \(r.1 ?? "desconocido")")
                }
            }
        case .fourier:
            pedir(operacion.rawValue, ["muestras separadas por coma"], ["1,0,-1,0"]) {
                [weak self] v in
                guard let self, let muestras = self.leerVector(v[0]) else { return }
                let espectro = MotorMatematicoLocal.fourier(muestras)
                self.mostrar(espectro.enumerated().map { "X[\($0.offset)] = \($0.element.texto())" }
                    .joined(separator: "\n"))
            }
        case .laplace:
            pedir(operacion.rawValue, ["f(t)", "s real", "s imaginario", "t máximo"],
                  ["exp(-t)", "1", "0", "20"]) { [weak self] v in
                guard let self, let sr = numero(v[1]), let si = numero(v[2]),
                      let t = numero(v[3]) else { return }
                self.mostrar(self.escalar(MotorMatematicoLocal.laplace(
                    v[0], s: ComplejoMatematico(real: sr, imaginario: si), tiempoMaximo: t,
                    variables: self.variables()), prefijo: "F(s) ≈ "))
            }
        case .edo:
            pedir(operacion.rawValue, ["dy/dt = f(t,y)", "t0", "y0", "t1", "pasos"],
                  ["-y", "0", "1", "5", "200"]) { [weak self] v in
                guard let self, let t0 = numero(v[1]), let y0 = numero(v[2]),
                      let t1 = numero(v[3]), let pasos = Int(v[4]) else { return }
                let r = MotorMatematicoLocal.edo(
                    v[0], t0: t0, y0: ComplejoMatematico(real: y0, imaginario: 0),
                    t1: t1, pasos: pasos, variables: self.variables())
                guard let puntos = r.0 else { self.mostrar("Error: \(r.1 ?? "desconocido")"); return }
                let reducidos = puntos.enumerated().filter {
                    $0.offset == 0 || $0.offset == puntos.count - 1 ||
                    $0.offset.isMultiple(of: max(puntos.count / 12, 1))
                }.map { "t=\(NotacionIngenieria.formatear($0.element.0))  y=\($0.element.1.texto())" }
                self.mostrar(reducidos.joined(separator: "\n"))
            }
        case .dimensiones:
            pedir(operacion.rawValue, ["ecuación"], ["P = V*I"]) { [weak self] v in
                let r = AnalizadorDimensional.verificar(v[0])
                self?.mostrar(r.correcta ? "✓ \(r.explicacion)" : "⚠️ \(r.explicacion)")
            }
        case .latex:
            pedir(operacion.rawValue, ["ecuación legible"], ["V = R*I"]) { [weak self] v in
                let latex = FormateadorEcuaciones.latex(v[0])
                let mathML = FormateadorEcuaciones.mathML(v[0])
                self?.mostrar("LaTeX:\n\(latex)\n\nMathML:\n\(mathML)")
            }
        }
    }

    private func escalar(_ resultado: MotorMatematicoLocal.Resultado, prefijo: String) -> String {
        resultado.valor.map { prefijo + $0.texto() } ?? "Error: \(resultado.error ?? "desconocido")"
    }

    private func pedirMatriz(titulo: String, completado: @escaping ([[ComplejoMatematico]]) -> Void) {
        pedir(titulo, ["filas por ; y columnas por ,"], ["1,2;3,4"]) { [weak self] v in
            guard let matriz = self?.leerMatriz(v[0]) else {
                self?.mostrar("Matriz inválida o no cuadrada."); return
            }
            completado(matriz)
        }
    }

    private func leerMatriz(_ texto: String) -> [[ComplejoMatematico]]? {
        let filas = texto.split(separator: ";").map { fila in
            fila.split(separator: ",").compactMap { evaluarCelda(String($0)) }
        }
        guard !filas.isEmpty, filas.allSatisfy({ $0.count == filas.count }) else { return nil }
        return filas
    }

    private func leerSistema(_ texto: String) -> ([[ComplejoMatematico]], [ComplejoMatematico])? {
        var a: [[ComplejoMatematico]] = [], b: [ComplejoMatematico] = []
        for fila in texto.split(separator: ";") {
            let partes = fila.split(separator: "|", maxSplits: 1)
            guard partes.count == 2,
                  let termino = evaluarCelda(String(partes[1])) else { return nil }
            let coeficientes = partes[0].split(separator: ",").compactMap {
                evaluarCelda(String($0))
            }
            a.append(coeficientes); b.append(termino)
        }
        guard !a.isEmpty, a.allSatisfy({ $0.count == a.count }) else { return nil }
        return (a, b)
    }

    private func leerVector(_ texto: String) -> [ComplejoMatematico]? {
        let valores = texto.split(separator: ",").compactMap { evaluarCelda(String($0)) }
        return valores.isEmpty ? nil : valores
    }

    private func evaluarCelda(_ texto: String) -> ComplejoMatematico? {
        MotorMatematicoLocal.evaluar(
            texto.trimmingCharacters(in: .whitespacesAndNewlines), variables: variables()).valor
    }

    private func pedir(_ titulo: String, _ campos: [String], _ valores: [String],
                       completado: @escaping ([String]) -> Void) {
        let alerta = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        for (i, campo) in campos.enumerated() {
            alerta.addTextField { $0.placeholder = campo; $0.text = valores[i]
                $0.autocapitalizationType = .none; $0.autocorrectionType = .no }
        }
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) { _ in
            completado(alerta.textFields?.map { $0.text ?? "" } ?? [])
        }); alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(alerta, animated: true)
    }

    private func mostrar(_ texto: String) {
        let alerta = UIAlertController(title: "Resultado local", message: texto,
                                       preferredStyle: .actionSheet)
        alerta.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
            [weak self] _ in self?.alInsertar?(texto)
        })
        alerta.addAction(UIAlertAction(title: "Copiar", style: .default) { _ in
            UIPasteboard.general.string = texto
        })
        alerta.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        alerta.popoverPresentationController?.sourceView = view; present(alerta, animated: true)
    }
}

private func numero(_ texto: String) -> Double? {
    Double(texto.replacingOccurrences(of: ",", with: "."))
}

private func formatearMatriz(_ matriz: [[ComplejoMatematico]]) -> String {
    matriz.map { "[ " + $0.map { $0.texto() }.joined(separator: "   ") + " ]" }
        .joined(separator: "\n")
}

struct ResultadoDimensional {
    var correcta: Bool
    var explicacion: String
}

private struct DimensionSI: Equatable {
    var l = 0, m = 0, t = 0, i = 0
    static func + (a: Self, b: Self) -> Self {
        Self(l: a.l + b.l, m: a.m + b.m, t: a.t + b.t, i: a.i + b.i)
    }
    static func - (a: Self, b: Self) -> Self {
        Self(l: a.l - b.l, m: a.m - b.m, t: a.t - b.t, i: a.i - b.i)
    }
    func potencia(_ n: Int) -> Self { Self(l: l*n, m: m*n, t: t*n, i: i*n) }
    var texto: String { "L^\(l) M^\(m) T^\(t) I^\(i)" }
}

enum AnalizadorDimensional {
    private static let simbolos: [String: DimensionSI] = [
        "m": .init(l: 1), "s": .init(t: 1), "kg": .init(m: 1), "A": .init(i: 1),
        "v": .init(l: 1, t: -1), "a": .init(l: 1, t: -2),
        "F": .init(l: 1, m: 1, t: -2), "N": .init(l: 1, m: 1, t: -2),
        "P": .init(l: 2, m: 1, t: -3), "W": .init(l: 2, m: 1, t: -3),
        "E": .init(l: 2, m: 1, t: -2), "J": .init(l: 2, m: 1, t: -2),
        "V": .init(l: 2, m: 1, t: -3, i: -1),
        "I": .init(i: 1), "R": .init(l: 2, m: 1, t: -3, i: -2),
        "Z": .init(l: 2, m: 1, t: -3, i: -2),
        "C": .init(l: -2, m: -1, t: 4, i: 2),
        "L": .init(l: 2, m: 1, t: -2, i: -2),
        "Q": .init(t: 1, i: 1), "f": .init(t: -1), "omega": .init(t: -1)
    ]

    static func verificar(_ ecuacion: String) -> ResultadoDimensional {
        let lados = ecuacion.split(separator: "=", maxSplits: 1).map(String.init)
        guard lados.count == 2 else {
            return ResultadoDimensional(correcta: false, explicacion: "Se requiere una igualdad con =.")
        }
        guard let izquierda = dimension(lados[0]), let derecha = dimension(lados[1]) else {
            return ResultadoDimensional(correcta: false,
                explicacion: "Hay símbolos sin dimensión conocida. Defínalos antes de verificar.")
        }
        return izquierda == derecha
            ? ResultadoDimensional(correcta: true,
                explicacion: "Ambos lados tienen dimensión \(izquierda.texto).")
            : ResultadoDimensional(correcta: false,
                explicacion: "No coinciden: izquierda \(izquierda.texto), derecha \(derecha.texto).")
    }

    private static func dimension(_ expresion: String) -> DimensionSI? {
        let limpio = expresion.replacingOccurrences(of: " ", with: "")
        var total = DimensionSI(), operacion = 1, indice = limpio.startIndex
        while indice < limpio.endIndex {
            let c = limpio[indice]
            if c == "*" || c == "·" { operacion = 1; indice = limpio.index(after: indice); continue }
            if c == "/" { operacion = -1; indice = limpio.index(after: indice); continue }
            if c == "(" || c == ")" || c.isNumber || c == "." || c == "-" || c == "+" {
                indice = limpio.index(after: indice); continue
            }
            let inicio = indice
            while indice < limpio.endIndex,
                  limpio[indice].isLetter || limpio[indice].isNumber || limpio[indice] == "_" {
                indice = limpio.index(after: indice)
            }
            let nombre = String(limpio[inicio..<indice])
            let base = simbolos[nombre] ?? simbolos[String(nombre.prefix(1))]
            guard let base else { return nil }
            var exponente = 1
            if indice < limpio.endIndex, limpio[indice] == "^" {
                indice = limpio.index(after: indice); let comienzo = indice
                if indice < limpio.endIndex, limpio[indice] == "-" { indice = limpio.index(after: indice) }
                while indice < limpio.endIndex, limpio[indice].isNumber { indice = limpio.index(after: indice) }
                exponente = Int(limpio[comienzo..<indice]) ?? 1
            }
            total = operacion > 0 ? total + base.potencia(exponente)
                                  : total - base.potencia(exponente)
            operacion = 1
        }
        return total
    }
}

enum FormateadorEcuaciones {
    static func latex(_ texto: String) -> String {
        var salida = texto.replacingOccurrences(of: "*", with: " \\cdot ")
        salida = salida.replacingOccurrences(of: "sqrt(", with: "\\sqrt{")
        if salida.contains("\\sqrt{") && !salida.hasSuffix("}") {
            salida = salida.replacingOccurrences(of: ")", with: "}")
        }
        salida = salida.replacingOccurrences(of: "<=", with: "\\le ")
            .replacingOccurrences(of: ">=", with: "\\ge ")
            .replacingOccurrences(of: "pi", with: "\\pi ")
        return "\\(\(salida)\\)"
    }

    static func mathML(_ texto: String) -> String {
        let escapado = texto.replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
        return "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><mtext>\(escapado)</mtext></math>"
    }
}
