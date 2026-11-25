import UIKit

struct ComplejoMatematico: Codable, Equatable {
    var real: Double
    var imaginario: Double

    var magnitud: Double { hypot(real, imaginario) }
    var faseGrados: Double { atan2(imaginario, real) * 180 / .pi }

    func texto(formato: FormatoNumero = .ingenieria) -> String {
        if abs(imaginario) < 1e-12 { return formato.formatear(real) }
        if abs(real) < 1e-12 { return "\(formato.formatear(imaginario))j" }
        let signo = imaginario >= 0 ? "+" : "−"
        return "\(formato.formatear(real)) \(signo) j\(formato.formatear(abs(imaginario)))"
    }

    var polar: String {
        "\(NotacionIngenieria.formatear(magnitud)) ∠ \(String(format: "%.5g", faseGrados))°"
    }

    var exponencial: String {
        "\(NotacionIngenieria.formatear(magnitud))e^(j\(String(format: "%.5g", faseGrados * .pi / 180)))"
    }
}

enum FormatoNumero: String, CaseIterable, Codable {
    case decimal = "Decimal"
    case cientifica = "Científica"
    case ingenieria = "Ingeniería"

    func formatear(_ valor: Double) -> String {
        guard valor.isFinite else { return String(describing: valor) }
        switch self {
        case .decimal: return String(format: "%.12g", valor)
        case .cientifica: return String(format: "%.8e", valor)
        case .ingenieria: return NotacionIngenieria.formatear(valor)
        }
    }
}

struct VariableMatematica: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var valor: ComplejoMatematico
    var unidad: String?
    var congelada = false
}

struct RegistroCalculo: Codable, Identifiable, Equatable {
    var id = UUID()
    var expresion: String
    var resultado: ComplejoMatematico
    var unidad: String?
    var fecha = Date()
    var congelado = false
}

enum TipoGrafica: String, CaseIterable, Codable {
    case cartesiana = "Cartesiana"
    case polar = "Polar"
    case parametrica = "Paramétrica"
    case logLog = "Log-log"
    case semilogX = "Semilog X"
    case semilogY = "Semilog Y"
    case bode = "Bode"
    case nyquist = "Nyquist"
    case smith = "Smith"
    case histograma = "Histograma"
    case scatter = "Scatter"
    case waveform = "Waveform"
}

struct MuestraGrafica: Codable, Equatable {
    var parametro: Double
    var x: Double
    var y: Double
    var valida: Bool
}

struct FuncionGrafica: Codable, Identifiable, Equatable {
    var id = UUID()
    var nombre: String
    var expresionY: String
    var expresionX: String?
    var tipo: TipoGrafica = .cartesiana
    var inicio: Double = -10
    var fin: Double = 10
    var colorARGB: UInt32 = 0xFF0A84FF
    var visible = true
    var congelada = false
    var muestrasCongeladas: [MuestraGrafica] = []
}

struct EstadoMatematicoDocumento: Codable, Equatable {
    var variables: [VariableMatematica] = []
    var historial: [RegistroCalculo] = []
    var funciones: [FuncionGrafica] = []
    var calculoVivo = false
    var formato: FormatoNumero = .ingenieria
}

final class RepositorioMatematicas {
    static let compartido = RepositorioMatematicas()
    private let url: URL
    private var documentos: [String: EstadoMatematicoDocumento] = [:]

    private init() {
        let base = FileManager.default.urls(for: .documentDirectory,
                                            in: .userDomainMask).first!
        let carpeta = base.appendingPathComponent("Matematicas", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("estado-matematico.json")
        let decodificador = JSONDecoder()
        decodificador.dateDecodingStrategy = .iso8601
        if let datos = try? Data(contentsOf: url),
           let leido = try? decodificador.decode(
                [String: EstadoMatematicoDocumento].self, from: datos) {
            documentos = leido
        }
    }

    func estado(documento: UInt64) -> EstadoMatematicoDocumento {
        documentos[String(documento)] ?? EstadoMatematicoDocumento()
    }

    func guardar(_ estado: EstadoMatematicoDocumento, documento: UInt64) {
        documentos[String(documento)] = estado
        let codificador = JSONEncoder()
        codificador.outputFormatting = [.sortedKeys]
        codificador.dateEncodingStrategy = .iso8601
        if let datos = try? codificador.encode(documentos) {
            try? datos.write(to: url, options: .atomic)
        }
    }
}

enum MotorMatematicoLocal {
    struct Resultado {
        var valor: ComplejoMatematico?
        var error: String?
    }

    static func evaluar(_ expresion: String,
                        variables: [VariableMatematica]) -> Resultado {
        var salida = TintaComplejo()
        var error = [CChar](repeating: 0, count: 512)
        let tamanoError = error.count
        let correcto = expresion.withCString { texto in
            conVariables(variables) { puente, cantidad in
                tinta_matematica_evaluar(texto, puente, cantidad, &salida,
                                         &error, tamanoError)
            }
        }
        return correcto
            ? Resultado(valor: ComplejoMatematico(real: salida.real,
                                                  imaginario: salida.imaginario))
            : Resultado(valor: nil, error: String(cString: error))
    }

    static func derivar(_ expresion: String, variable: String, x: Double,
                        variables: [VariableMatematica]) -> Resultado {
        var salida = TintaComplejo()
        var error = [CChar](repeating: 0, count: 512)
        let tamanoError = error.count
        let correcto = expresion.withCString { texto in
            variable.withCString { nombre in
                conVariables(variables) { puente, cantidad in
                    tinta_matematica_derivar(texto, nombre, x, puente, cantidad,
                                             &salida, &error, tamanoError)
                }
            }
        }
        return correcto
            ? Resultado(valor: ComplejoMatematico(real: salida.real,
                                                  imaginario: salida.imaginario))
            : Resultado(valor: nil, error: String(cString: error))
    }

    static func integrar(_ expresion: String, variable: String,
                         inicio: Double, fin: Double,
                         variables: [VariableMatematica]) -> Resultado {
        var salida = TintaComplejo()
        var error = [CChar](repeating: 0, count: 512)
        let tamanoError = error.count
        let correcto = expresion.withCString { texto in
            variable.withCString { nombre in
                conVariables(variables) { puente, cantidad in
                    tinta_matematica_integrar(texto, nombre, inicio, fin, 512,
                                              puente, cantidad, &salida,
                                              &error, tamanoError)
                }
            }
        }
        return correcto
            ? Resultado(valor: ComplejoMatematico(real: salida.real,
                                                  imaginario: salida.imaginario))
            : Resultado(valor: nil, error: String(cString: error))
    }

    static func buscarRaiz(_ expresion: String, variable: String = "x",
                           estimacion: Double,
                           variables: [VariableMatematica]) -> Resultado {
        operacionEscalar(expresion, variable: variable, variables: variables) {
            texto, nombre, puente, cantidad, salida, error, tamano in
            tinta_matematica_buscar_raiz(texto, nombre, estimacion, puente,
                                         cantidad, salida, error, tamano)
        }
    }

    static func limite(_ expresion: String, variable: String = "x",
                       punto: Double, lado: Int,
                       variables: [VariableMatematica]) -> Resultado {
        operacionEscalar(expresion, variable: variable, variables: variables) {
            texto, nombre, puente, cantidad, salida, error, tamano in
            tinta_matematica_limite(texto, nombre, punto, Int32(lado), puente,
                                    cantidad, salida, error, tamano)
        }
    }

    static func sumatoria(_ expresion: String, variable: String = "n",
                          inicio: Int64, fin: Int64,
                          variables: [VariableMatematica]) -> Resultado {
        operacionEscalar(expresion, variable: variable, variables: variables) {
            texto, nombre, puente, cantidad, salida, error, tamano in
            tinta_matematica_sumatoria(texto, nombre, inicio, fin, puente,
                                       cantidad, salida, error, tamano)
        }
    }

    static func laplace(_ expresion: String, variable: String = "t",
                        s: ComplejoMatematico, tiempoMaximo: Double,
                        variables: [VariableMatematica]) -> Resultado {
        let parametro = puente(s)
        return operacionEscalar(expresion, variable: variable, variables: variables) {
            texto, nombre, variablesC, cantidad, salida, error, tamano in
            tinta_matematica_laplace(texto, nombre, parametro, tiempoMaximo, 1024,
                                     variablesC, cantidad, salida, error, tamano)
        }
    }

    static func determinante(_ matriz: [[ComplejoMatematico]]) -> Resultado {
        guard let n = ordenValido(matriz) else {
            return Resultado(valor: nil, error: "La matriz debe ser cuadrada.")
        }
        var entrada = matriz.flatMap { $0 }.map(puente)
        var salida = TintaComplejo(); var error = [CChar](repeating: 0, count: 512)
        let tamano = error.count
        let correcto = tinta_matriz_determinante(&entrada, n, &salida, &error, tamano)
        return correcto ? Resultado(valor: complejo(salida), error: nil)
            : Resultado(valor: nil, error: String(cString: error))
    }

    static func inversa(_ matriz: [[ComplejoMatematico]])
        -> ([[ComplejoMatematico]]?, String?) {
        guard let n = ordenValido(matriz) else { return (nil, "La matriz debe ser cuadrada.") }
        var entrada = matriz.flatMap { $0 }.map(puente)
        var salida = [TintaComplejo](repeating: TintaComplejo(), count: n * n)
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count, tamano = error.count
        let total = tinta_matriz_invertir(&entrada, n, &salida, capacidad, &error, tamano)
        guard total == capacidad else { return (nil, String(cString: error)) }
        return (stride(from: 0, to: capacidad, by: n).map {
            salida[$0..<($0 + n)].map(complejo)
        }, nil)
    }

    static func resolverSistema(_ coeficientes: [[ComplejoMatematico]],
                                terminos: [ComplejoMatematico])
        -> ([ComplejoMatematico]?, String?) {
        guard let n = ordenValido(coeficientes), terminos.count == n else {
            return (nil, "Las dimensiones del sistema no coinciden.")
        }
        var a = coeficientes.flatMap { $0 }.map(puente), b = terminos.map(puente)
        var salida = [TintaComplejo](repeating: TintaComplejo(), count: n)
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count, tamano = error.count
        let total = tinta_sistema_lineal_resolver(&a, &b, n, &salida, capacidad,
                                                  &error, tamano)
        return total == n ? (salida.map(complejo), nil) : (nil, String(cString: error))
    }

    static func productoPunto(_ a: [ComplejoMatematico], _ b: [ComplejoMatematico])
        -> Resultado {
        guard !a.isEmpty, a.count == b.count else {
            return Resultado(valor: nil, error: "Los vectores no tienen la misma dimensión.")
        }
        var aa = a.map(puente), bb = b.map(puente), salida = TintaComplejo()
        var error = [CChar](repeating: 0, count: 512); let tamano = error.count
        let cantidad = aa.count
        let correcto = tinta_vector_producto_punto(&aa, &bb, cantidad, &salida,
                                                   &error, tamano)
        return correcto ? Resultado(valor: complejo(salida), error: nil)
            : Resultado(valor: nil, error: String(cString: error))
    }

    static func productoCruz(_ a: [ComplejoMatematico], _ b: [ComplejoMatematico])
        -> ([ComplejoMatematico]?, String?) {
        guard a.count == 3, b.count == 3 else { return (nil, "Se requieren tres componentes.") }
        var aa = a.map(puente), bb = b.map(puente)
        var salida = [TintaComplejo](repeating: TintaComplejo(), count: 3)
        var error = [CChar](repeating: 0, count: 512); let tamano = error.count
        let capacidad = salida.count
        let total = tinta_vector_producto_cruz(&aa, &bb, &salida, capacidad,
                                               &error, tamano)
        return total == 3 ? (salida.map(complejo), nil) : (nil, String(cString: error))
    }

    static func fourier(_ muestras: [ComplejoMatematico], inversa: Bool = false)
        -> [ComplejoMatematico] {
        guard !muestras.isEmpty else { return [] }
        var entrada = muestras.map(puente)
        var salida = [TintaComplejo](repeating: TintaComplejo(), count: entrada.count)
        let capacidad = salida.count, cantidad = entrada.count
        let total = tinta_matematica_fourier(&entrada, cantidad, inversa,
                                             &salida, capacidad)
        return salida.prefix(min(Int(total), capacidad)).map(complejo)
    }

    static func edo(_ derivada: String, t0: Double, y0: ComplejoMatematico,
                    t1: Double, pasos: Int, variables: [VariableMatematica])
        -> ([(Double, ComplejoMatematico)]?, String?) {
        let inicial = puente(y0)
        var salida = [TintaMuestraEDO](repeating: TintaMuestraEDO(),
                                      count: max(pasos + 1, 2))
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count, tamano = error.count
        let total = derivada.withCString { texto in
            conVariables(variables) { puenteVariables, cantidad in
                tinta_matematica_edo_primer_orden(
                    texto, t0, inicial, t1, max(pasos, 1), puenteVariables, cantidad,
                    &salida, capacidad, &error, tamano)
            }
        }
        guard total > 0 else { return (nil, String(cString: error)) }
        return (salida.prefix(min(Int(total), capacidad)).map { ($0.t, complejo($0.y)) }, nil)
    }

    static func convertir(_ valor: Double, de origen: String,
                          a destino: String) -> (Double?, String?) {
        var salida = 0.0
        var error = [CChar](repeating: 0, count: 512)
        let tamanoError = error.count
        let correcto = origen.withCString { a in
            destino.withCString { b in
                tinta_matematica_convertir_unidad(valor, a, b, &salida,
                                                   &error, tamanoError)
            }
        }
        return correcto ? (salida, nil) : (nil, String(cString: error))
    }

    static func muestrear(_ funcion: FuncionGrafica,
                          variables: [VariableMatematica],
                          cantidad: Int = 600) -> ([MuestraGrafica], String?) {
        if funcion.congelada, !funcion.muestrasCongeladas.isEmpty {
            return (funcion.muestrasCongeladas, nil)
        }
        if funcion.tipo == .histograma || funcion.tipo == .scatter {
            return (leerPares(funcion.expresionY), nil)
        }
        let y = muestrasCrudas(funcion.expresionY, variable: "x",
                               inicio: funcion.inicio, fin: funcion.fin,
                               variables: variables, cantidad: cantidad)
        guard y.error == nil else { return ([], y.error) }
        var xParametrica: [ComplejoMatematico] = []
        if funcion.tipo == .parametrica, let expresionX = funcion.expresionX {
            let x = muestrasCrudas(expresionX, variable: "x",
                                   inicio: funcion.inicio, fin: funcion.fin,
                                   variables: variables, cantidad: cantidad)
            if let error = x.error { return ([], error) }
            xParametrica = x.valores.map { $0.1 }
        }
        let puntos = y.valores.enumerated().map { indice, elemento -> MuestraGrafica in
            let t = elemento.0
            let z = elemento.1
            guard elemento.2 else { return MuestraGrafica(parametro: t, x: 0, y: 0, valida: false) }
            let punto: (Double, Double)
            switch funcion.tipo {
            case .cartesiana, .waveform: punto = (t, z.real)
            case .polar: punto = (z.real * cos(t), z.real * sin(t))
            case .parametrica:
                let px = indice < xParametrica.count ? xParametrica[indice].real : .nan
                punto = (px, z.real)
            case .logLog:
                punto = (t > 0 ? log10(t) : .nan, z.real > 0 ? log10(z.real) : .nan)
            case .semilogX: punto = (t > 0 ? log10(t) : .nan, z.real)
            case .semilogY: punto = (t, z.real > 0 ? log10(z.real) : .nan)
            case .bode: punto = (t > 0 ? log10(t) : .nan,
                                 z.magnitud > 0 ? 20 * log10(z.magnitud) : .nan)
            case .nyquist: punto = (z.real, z.imaginario)
            case .smith:
                let denominador = pow(z.real + 1, 2) + pow(z.imaginario, 2)
                punto = denominador > 0
                    ? ((pow(z.real, 2) + pow(z.imaginario, 2) - 1) / denominador,
                       2 * z.imaginario / denominador)
                    : (.nan, .nan)
            case .histograma, .scatter: punto = (t, z.real)
            }
            return MuestraGrafica(parametro: t, x: punto.0, y: punto.1,
                                  valida: punto.0.isFinite && punto.1.isFinite)
        }
        return (puntos, nil)
    }

    private static func muestrasCrudas(_ expresion: String, variable: String,
                                       inicio: Double, fin: Double,
                                       variables: [VariableMatematica], cantidad: Int)
        -> (valores: [(Double, ComplejoMatematico, Bool)], error: String?) {
        var salida = [TintaMuestraFuncion](repeating: TintaMuestraFuncion(),
                                           count: max(cantidad, 2))
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count
        let tamanoError = error.count
        let total = expresion.withCString { texto in
            variable.withCString { nombre in
                conVariables(variables) { puente, numeroVariables in
                    tinta_matematica_muestrear(texto, nombre, inicio, fin,
                                               capacidad, puente, numeroVariables,
                                               &salida, capacidad, &error, tamanoError)
                }
            }
        }
        if total == 0 { return ([], String(cString: error)) }
        let valores = salida.prefix(min(Int(total), salida.count)).map {
            ($0.x, ComplejoMatematico(real: $0.y.real,
                                     imaginario: $0.y.imaginario), $0.valida)
        }
        return (valores, error.first == 0 ? nil : String(cString: error))
    }

    private static func leerPares(_ texto: String) -> [MuestraGrafica] {
        texto.split(whereSeparator: { ";\n".contains($0) }).enumerated().compactMap {
            indice, fila in
            let numeros = fila.split(whereSeparator: { ", \t".contains($0) })
                .compactMap { Double($0.replacingOccurrences(of: ",", with: ".")) }
            guard numeros.count >= 2 else { return nil }
            return MuestraGrafica(parametro: Double(indice), x: numeros[0],
                                  y: numeros[1], valida: true)
        }
    }

    private static func operacionEscalar(
        _ expresion: String, variable: String, variables: [VariableMatematica],
        _ ejecutar: (UnsafePointer<CChar>, UnsafePointer<CChar>,
                     UnsafePointer<TintaVariableCompleja>?, Int,
                     UnsafeMutablePointer<TintaComplejo>,
                     UnsafeMutablePointer<CChar>, Int) -> Bool) -> Resultado {
        var salida = TintaComplejo(); var error = [CChar](repeating: 0, count: 512)
        let tamano = error.count
        let correcto = expresion.withCString { texto in
            variable.withCString { nombre in
                conVariables(variables) { puente, cantidad in
                    ejecutar(texto, nombre, puente, cantidad, &salida, &error, tamano)
                }
            }
        }
        return correcto ? Resultado(valor: complejo(salida), error: nil)
            : Resultado(valor: nil, error: String(cString: error))
    }

    private static func puente(_ valor: ComplejoMatematico) -> TintaComplejo {
        var salida = TintaComplejo(); salida.real = valor.real
        salida.imaginario = valor.imaginario; return salida
    }
    private static func complejo(_ valor: TintaComplejo) -> ComplejoMatematico {
        ComplejoMatematico(real: valor.real, imaginario: valor.imaginario)
    }
    private static func ordenValido(_ matriz: [[ComplejoMatematico]]) -> Int? {
        guard !matriz.isEmpty, matriz.allSatisfy({ $0.count == matriz.count }) else { return nil }
        return matriz.count
    }

    private static func conVariables<R>(
        _ variables: [VariableMatematica],
        _ cuerpo: (UnsafePointer<TintaVariableCompleja>?, Int) -> R) -> R {
        let nombres = variables.map { ContiguousArray($0.nombre.utf8CString) }
        func anidar(_ indice: Int, _ punteros: [UnsafePointer<CChar>]) -> R {
            if indice == nombres.count {
                let puente = variables.enumerated().map { i, variable in
                    TintaVariableCompleja(nombre: punteros[i], real: variable.valor.real,
                                          imaginario: variable.valor.imaginario)
                }
                return puente.withUnsafeBufferPointer {
                    cuerpo($0.baseAddress, $0.count)
                }
            }
            return nombres[indice].withUnsafeBufferPointer { buffer in
                anidar(indice + 1, punteros + [buffer.baseAddress!])
            }
        }
        if variables.isEmpty { return cuerpo(nil, 0) }
        return anidar(0, [])
    }
}

struct ConstanteFisica: Identifiable {
    var id: String { simbolo }
    var nombre: String
    var simbolo: String
    var valor: Double
    var unidad: String
    var fuente: String

    static let catalogo: [ConstanteFisica] = [
        .init(nombre: "Velocidad de la luz", simbolo: "c0", valor: 299_792_458,
              unidad: "m/s", fuente: "SI 2019, valor exacto"),
        .init(nombre: "Constante de Planck", simbolo: "h", valor: 6.626_070_15e-34,
              unidad: "J·s", fuente: "SI 2019, valor exacto"),
        .init(nombre: "Carga elemental", simbolo: "qe", valor: 1.602_176_634e-19,
              unidad: "C", fuente: "SI 2019, valor exacto"),
        .init(nombre: "Constante de Boltzmann", simbolo: "kB", valor: 1.380_649e-23,
              unidad: "J/K", fuente: "SI 2019, valor exacto"),
        .init(nombre: "Avogadro", simbolo: "NA", valor: 6.022_140_76e23,
              unidad: "mol⁻¹", fuente: "SI 2019, valor exacto"),
        .init(nombre: "Gravedad estándar", simbolo: "g0", valor: 9.80665,
              unidad: "m/s²", fuente: "ISO 80000, valor convencional"),
        .init(nombre: "Gravitación universal", simbolo: "G", valor: 6.67430e-11,
              unidad: "m³/(kg·s²)", fuente: "CODATA 2018"),
        .init(nombre: "Permitividad del vacío", simbolo: "eps0", valor: 8.854_187_8128e-12,
              unidad: "F/m", fuente: "CODATA 2018")
    ]
}

final class VistaGraficaMatematica: UIView {
    var funciones: [(FuncionGrafica, [MuestraGrafica])] = [] { didSet { setNeedsDisplay() } }
    var alTrazar: ((String) -> Void)?
    private var puntoTrazado: CGPoint?

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .secondarySystemBackground
        layer.cornerRadius = 12
        addGestureRecognizer(UIPanGestureRecognizer(target: self,
                                                    action: #selector(trazar(_:))))
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func draw(_ rect: CGRect) {
        guard let contexto = UIGraphicsGetCurrentContext() else { return }
        let validos = funciones.flatMap { $0.1 }.filter(\.valida)
        guard let xmin = validos.map(\.x).min(), let xmax = validos.map(\.x).max(),
              let ymin = validos.map(\.y).min(), let ymax = validos.map(\.y).max() else {
            let texto = "Agregue una función o pares x,y" as NSString
            texto.draw(at: CGPoint(x: 20, y: 20), withAttributes: [
                .font: UIFont.systemFont(ofSize: 14), .foregroundColor: UIColor.secondaryLabel])
            return
        }
        let dx = max(xmax - xmin, 1e-12), dy = max(ymax - ymin, 1e-12)
        func pantalla(_ p: MuestraGrafica) -> CGPoint {
            CGPoint(x: 18 + (p.x - xmin) / dx * (bounds.width - 36),
                    y: bounds.height - 18 - (p.y - ymin) / dy * (bounds.height - 36))
        }
        contexto.setStrokeColor(UIColor.tertiaryLabel.cgColor)
        contexto.setLineWidth(0.7)
        if xmin <= 0, xmax >= 0 {
            let x = pantalla(MuestraGrafica(parametro: 0, x: 0, y: ymin, valida: true)).x
            contexto.move(to: CGPoint(x: x, y: 8)); contexto.addLine(to: CGPoint(x: x, y: bounds.height - 8))
        }
        if ymin <= 0, ymax >= 0 {
            let y = pantalla(MuestraGrafica(parametro: 0, x: xmin, y: 0, valida: true)).y
            contexto.move(to: CGPoint(x: 8, y: y)); contexto.addLine(to: CGPoint(x: bounds.width - 8, y: y))
        }
        contexto.strokePath()

        for (funcion, muestras) in funciones where funcion.visible {
            UIColor(argb: funcion.colorARGB).setStroke()
            let ruta = UIBezierPath(); ruta.lineWidth = 2
            var segmento = false
            for muestra in muestras {
                guard muestra.valida else { segmento = false; continue }
                let p = pantalla(muestra)
                if segmento { ruta.addLine(to: p) } else { ruta.move(to: p); segmento = true }
            }
            ruta.stroke()
        }
        if let p = puntoTrazado {
            UIColor.systemOrange.setStroke(); contexto.setLineWidth(1)
            contexto.move(to: CGPoint(x: p.x, y: 0)); contexto.addLine(to: CGPoint(x: p.x, y: bounds.height))
            contexto.strokePath()
        }
    }

    @objc private func trazar(_ gesto: UIPanGestureRecognizer) {
        let punto = gesto.location(in: self)
        puntoTrazado = punto; setNeedsDisplay()
        var candidatos: [(String, MuestraGrafica)] = []
        for funcion in funciones {
            for muestra in funcion.1 where muestra.valida {
                candidatos.append((funcion.0.nombre, muestra))
            }
        }
        guard let candidato = candidatos.min(by: { a, b in
            abs(a.1.x - Double(punto.x)) < abs(b.1.x - Double(punto.x))
        }) else { return }
        alTrazar?("\(candidato.0): x=\(NotacionIngenieria.formatear(candidato.1.x)), y=\(NotacionIngenieria.formatear(candidato.1.y))")
        if gesto.state == .ended || gesto.state == .cancelled { puntoTrazado = nil; setNeedsDisplay() }
    }
}

final class PanelMatematicas: UIViewController, UITableViewDataSource, UITableViewDelegate {
    var alInsertar: ((String) -> Void)?
    private let documentoId: UInt64
    private var estado: EstadoMatematicoDocumento
    private let expresion = UITextField()
    private let resultado = UILabel()
    private let grafica = VistaGraficaMatematica()
    private let tabla = UITableView(frame: .zero, style: .insetGrouped)
    private let tipoGrafica = UIButton(type: .system)
    private let inicio = UITextField(); private let fin = UITextField()
    private let valorUnidad = UITextField(); private let unidadOrigen = UITextField()
    private let unidadDestino = UITextField(); private let resultadoUnidad = UILabel()
    private let formato = UISegmentedControl(items: FormatoNumero.allCases.map(\.rawValue))
    private let vivo = UISwitch()

    init(documentoId: UInt64) {
        self.documentoId = documentoId
        estado = RepositorioMatematicas.compartido.estado(documento: documentoId)
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Matemáticas locales"
        view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItems = [
            UIBarButtonItem(title: "Estructuras", style: .plain, target: self,
                            action: #selector(estructuras)),
            UIBarButtonItem(title: "Constantes", style: .plain, target: self,
                            action: #selector(constantes)),
            UIBarButtonItem(title: "Congelar", style: .plain, target: self,
                            action: #selector(congelar))
        ]
        construir()
        actualizarGrafica()
    }

    private func construir() {
        expresion.borderStyle = .roundedRect
        expresion.placeholder = "Ej.: sin(x), (3+j4)^2, R1*I"
        expresion.autocorrectionType = .no
        expresion.autocapitalizationType = .none
        resultado.numberOfLines = 0
        resultado.font = .monospacedSystemFont(ofSize: 16, weight: .medium)

        formato.selectedSegmentIndex = FormatoNumero.allCases.firstIndex(of: estado.formato) ?? 2
        formato.addTarget(self, action: #selector(cambiarFormato), for: .valueChanged)
        vivo.isOn = estado.calculoVivo
        vivo.addTarget(self, action: #selector(cambiarVivo), for: .valueChanged)

        let acciones = UIStackView(arrangedSubviews: [
            boton("= Evaluar", #selector(evaluar)), boton("d/dx", #selector(derivar)),
            boton("∫ dx", #selector(integrar)), boton("Graficar", #selector(agregarGrafica)),
            boton("Insertar", #selector(insertarResultado)), boton("Variable", #selector(agregarVariable))
        ])
        acciones.axis = .horizontal; acciones.spacing = 7; acciones.distribution = .fillEqually

        tipoGrafica.configuration = .bordered()
        tipoGrafica.menu = UIMenu(children: TipoGrafica.allCases.map { tipo in
            UIAction(title: tipo.rawValue) { [weak self] _ in
                self?.tipoGrafica.setTitle(tipo.rawValue, for: .normal)
                self?.tipoGrafica.accessibilityIdentifier = tipo.rawValue
            }
        }); tipoGrafica.showsMenuAsPrimaryAction = true
        tipoGrafica.setTitle(TipoGrafica.cartesiana.rawValue, for: .normal)
        tipoGrafica.accessibilityIdentifier = TipoGrafica.cartesiana.rawValue
        [inicio, fin].forEach { $0.borderStyle = .roundedRect; $0.keyboardType = .numbersAndPunctuation }
        inicio.placeholder = "inicio"; inicio.text = "-10"; fin.placeholder = "fin"; fin.text = "10"
        let opcionesGrafica = UIStackView(arrangedSubviews: [tipoGrafica, inicio, fin])
        opcionesGrafica.axis = .horizontal; opcionesGrafica.spacing = 8

        grafica.heightAnchor.constraint(equalToConstant: 300).isActive = true
        grafica.alTrazar = { [weak self] texto in self?.resultado.text = texto }

        tabla.dataSource = self; tabla.delegate = self
        tabla.heightAnchor.constraint(equalToConstant: 250).isActive = true

        [valorUnidad, unidadOrigen, unidadDestino].forEach {
            $0.borderStyle = .roundedRect; $0.autocapitalizationType = .none
        }
        valorUnidad.placeholder = "valor"; valorUnidad.keyboardType = .numbersAndPunctuation
        unidadOrigen.placeholder = "de: V, ohm, degf…"; unidadDestino.placeholder = "a: mV, kohm, degc…"
        let filaUnidad = UIStackView(arrangedSubviews: [valorUnidad, unidadOrigen, unidadDestino,
                                                        boton("Convertir", #selector(convertirUnidad))])
        filaUnidad.axis = .horizontal; filaUnidad.spacing = 8
        resultadoUnidad.numberOfLines = 0

        let filaModo = UIStackView(arrangedSubviews: [UILabel(texto: "Formato"), formato,
                                                      UILabel(texto: "Cálculo vivo"), vivo])
        filaModo.axis = .horizontal; filaModo.spacing = 8; filaModo.alignment = .center

        let contenido = UIStackView(arrangedSubviews: [
            expresion, acciones, resultado, filaModo,
            UILabel(titulo: "Gráficas editables"), opcionesGrafica, grafica,
            UILabel(titulo: "Variables e historial"), tabla,
            UILabel(titulo: "Conversión dimensional"), filaUnidad, resultadoUnidad
        ])
        contenido.axis = .vertical; contenido.spacing = 12
        contenido.translatesAutoresizingMaskIntoConstraints = false
        let scroll = UIScrollView(); scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(contenido); view.addSubview(scroll)
        NSLayoutConstraint.activate([
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            contenido.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 16),
            contenido.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -24),
            contenido.leadingAnchor.constraint(equalTo: scroll.frameLayoutGuide.leadingAnchor, constant: 18),
            contenido.trailingAnchor.constraint(equalTo: scroll.frameLayoutGuide.trailingAnchor, constant: -18)
        ])
    }

    private func boton(_ titulo: String, _ accion: Selector) -> UIButton {
        let b = UIButton(type: .system); b.configuration = .bordered()
        b.configuration?.title = titulo; b.addTarget(self, action: accion, for: .touchUpInside)
        return b
    }

    @objc private func evaluar() {
        guard let texto = expresion.text, !texto.isEmpty else { return }
        let r = MotorMatematicoLocal.evaluar(texto, variables: estado.variables)
        if let error = r.error { resultado.text = "Error: \(error)"; return }
        guard let valor = r.valor else { return }
        estado.historial.insert(RegistroCalculo(expresion: texto, resultado: valor), at: 0)
        if estado.historial.count > 500 { estado.historial.removeLast(estado.historial.count - 500) }
        resultado.text = presentar(valor)
        guardar(); tabla.reloadData()
    }

    @objc private func derivar() {
        guard let texto = expresion.text else { return }
        pedirIntervalo(titulo: "Derivada numérica", campos: ["x"], valores: ["0"]) {
            [weak self] valores in
            guard let self else { return }
            let r = MotorMatematicoLocal.derivar(texto, variable: "x", x: valores[0],
                                                 variables: self.estado.variables)
            self.resultado.text = r.valor.map { "d/dx = \(self.presentar($0))" }
                ?? "Error: \(r.error ?? "desconocido")"
        }
    }

    @objc private func integrar() {
        guard let texto = expresion.text else { return }
        pedirIntervalo(titulo: "Integral definida", campos: ["inicio", "fin"],
                       valores: [inicio.text ?? "0", fin.text ?? "1"]) { [weak self] v in
            guard let self else { return }
            let r = MotorMatematicoLocal.integrar(texto, variable: "x", inicio: v[0],
                                                  fin: v[1], variables: self.estado.variables)
            self.resultado.text = r.valor.map { "∫ = \(self.presentar($0))" }
                ?? "Error: \(r.error ?? "desconocido")"
        }
    }

    @objc private func agregarGrafica() {
        guard let texto = expresion.text, !texto.isEmpty,
              let a = Double((inicio.text ?? "").replacingOccurrences(of: ",", with: ".")),
              let b = Double((fin.text ?? "").replacingOccurrences(of: ",", with: ".")) else { return }
        let tipo = TipoGrafica(rawValue: tipoGrafica.accessibilityIdentifier ?? "") ?? .cartesiana
        var funcion = FuncionGrafica(nombre: "f\(estado.funciones.count + 1)",
                                     expresionY: texto, tipo: tipo, inicio: a, fin: b,
                                     colorARGB: colores[estado.funciones.count % colores.count])
        if tipo == .parametrica {
            let partes = texto.split(separator: ";", maxSplits: 1).map(String.init)
            guard partes.count == 2 else {
                resultado.text = "Para paramétrica use x(t); y(t)."; return
            }
            funcion.expresionX = partes[0]; funcion.expresionY = partes[1]
        }
        estado.funciones.append(funcion); guardar(); actualizarGrafica(); tabla.reloadData()
    }

    @objc private func insertarResultado() {
        guard let texto = resultado.text, !texto.isEmpty else { return }
        alInsertar?(texto)
    }

    @objc private func agregarVariable() {
        let a = UIAlertController(title: "Variable compartida con la página", message: nil,
                                  preferredStyle: .alert)
        a.addTextField { $0.placeholder = "nombre, ej. R1"; $0.autocapitalizationType = .none }
        a.addTextField { $0.placeholder = "valor o expresión"; $0.keyboardType = .numbersAndPunctuation }
        a.addTextField { $0.placeholder = "unidad opcional" }
        a.addAction(UIAlertAction(title: "Guardar", style: .default) { [weak self] _ in
            guard let self, let nombre = a.textFields?[0].text, !nombre.isEmpty,
                  let expresion = a.textFields?[1].text else { return }
            let r = MotorMatematicoLocal.evaluar(expresion, variables: self.estado.variables)
            guard let valor = r.valor else { self.resultado.text = r.error; return }
            if let i = self.estado.variables.firstIndex(where: {
                $0.nombre.caseInsensitiveCompare(nombre) == .orderedSame }) {
                guard !self.estado.variables[i].congelada else {
                    self.resultado.text = "La variable está congelada."; return
                }
                self.estado.variables[i].valor = valor
                self.estado.variables[i].unidad = a.textFields?[2].text
            } else {
                self.estado.variables.append(VariableMatematica(
                    nombre: nombre, valor: valor, unidad: a.textFields?[2].text))
            }
            self.guardar(); self.tabla.reloadData()
            if self.estado.calculoVivo { self.recalcularVivas() }
        })
        a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }

    @objc private func convertirUnidad() {
        guard let texto = valorUnidad.text?.replacingOccurrences(of: ",", with: "."),
              let valor = Double(texto), let origen = unidadOrigen.text,
              let destino = unidadDestino.text else { return }
        let r = MotorMatematicoLocal.convertir(valor, de: origen, a: destino)
        resultadoUnidad.text = r.0.map { "\(estado.formato.formatear($0)) \(destino)" }
            ?? "No se convirtió: \(r.1 ?? "error desconocido")"
    }

    @objc private func cambiarFormato() {
        estado.formato = FormatoNumero.allCases[max(formato.selectedSegmentIndex, 0)]
        guardar()
    }

    @objc private func cambiarVivo() {
        estado.calculoVivo = vivo.isOn; guardar()
        if vivo.isOn { recalcularVivas() }
    }

    @objc private func congelar() {
        for i in estado.variables.indices { estado.variables[i].congelada = true }
        for i in estado.historial.indices { estado.historial[i].congelado = true }
        for i in estado.funciones.indices {
            let muestras = MotorMatematicoLocal.muestrear(estado.funciones[i],
                                                          variables: estado.variables).0
            estado.funciones[i].muestrasCongeladas = muestras
            estado.funciones[i].congelada = true
        }
        estado.calculoVivo = false; vivo.isOn = false
        guardar(); actualizarGrafica(); tabla.reloadData()
        resultado.text = "Resultados, variables y gráficas congelados en su estado actual."
    }

    @objc private func constantes() {
        let lista = UIAlertController(title: "Constantes físicas",
                                      message: "Cada valor muestra su fuente o versión.",
                                      preferredStyle: .actionSheet)
        for constante in ConstanteFisica.catalogo {
            lista.addAction(UIAlertAction(
                title: "\(constante.simbolo) — \(constante.nombre)", style: .default) {
                    [weak self] _ in
                    guard let self else { return }
                    if let i = self.estado.variables.firstIndex(where: { $0.nombre == constante.simbolo }) {
                        if !self.estado.variables[i].congelada {
                            self.estado.variables[i].valor = ComplejoMatematico(real: constante.valor,
                                                                               imaginario: 0)
                        }
                    } else {
                        self.estado.variables.append(VariableMatematica(
                            nombre: constante.simbolo,
                            valor: ComplejoMatematico(real: constante.valor, imaginario: 0),
                            unidad: constante.unidad, congelada: true))
                    }
                    self.resultado.text = "\(constante.simbolo) = \(NotacionIngenieria.formatear(constante.valor, unidad: constante.unidad))\nFuente: \(constante.fuente)"
                    self.guardar(); self.tabla.reloadData()
                })
        }
        lista.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        lista.popoverPresentationController?.sourceView = view; present(lista, animated: true)
    }

    @objc private func estructuras() {
        let controlador = ControladorMatematicasEstructuradas { [weak self] in
            self?.estado.variables ?? []
        }
        controlador.alInsertar = alInsertar
        navigationController?.pushViewController(controlador, animated: true)
    }

    private func recalcularVivas() {
        guard estado.calculoVivo else { return }
        actualizarGrafica()
        resultado.text = "Cálculo vivo actualizado. Los apuntes ya insertados no se alteraron."
    }

    private func presentar(_ valor: ComplejoMatematico) -> String {
        let rectangular = valor.texto(formato: estado.formato)
        guard abs(valor.imaginario) >= 1e-12 else { return rectangular }
        return "\(rectangular)\n\(valor.polar)\n\(valor.exponencial)"
    }

    private func actualizarGrafica() {
        grafica.funciones = estado.funciones.compactMap { funcion in
            let r = MotorMatematicoLocal.muestrear(funcion, variables: estado.variables)
            if let error = r.1 { resultado.text = "\(funcion.nombre): \(error)" }
            return r.0.isEmpty ? nil : (funcion, r.0)
        }
    }

    private func guardar() {
        RepositorioMatematicas.compartido.guardar(estado, documento: documentoId)
    }

    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        estado.variables.count + estado.funciones.count + min(estado.historial.count, 100)
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath)
        -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        if indexPath.row < estado.variables.count {
            let v = estado.variables[indexPath.row]
            celda.textLabel?.text = "\(v.nombre) = \(presentar(v.valor))"
            celda.detailTextLabel?.text = [v.unidad, v.congelada ? "congelada" : nil]
                .compactMap { $0 }.joined(separator: " · ")
        } else if indexPath.row < estado.variables.count + estado.funciones.count {
            let i = indexPath.row - estado.variables.count
            let f = estado.funciones[i]
            celda.textLabel?.text = "\(f.nombre): \(f.expresionY)"
            celda.detailTextLabel?.text = "\(f.tipo.rawValue)\(f.congelada ? " · congelada" : "")"
        } else {
            let i = indexPath.row - estado.variables.count - estado.funciones.count
            let h = estado.historial[i]
            celda.textLabel?.text = "\(h.expresion) = \(h.resultado.texto(formato: estado.formato))"
            celda.detailTextLabel?.text = h.fecha.formatted(date: .abbreviated, time: .shortened)
        }
        celda.textLabel?.numberOfLines = 2; return celda
    }

    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if indexPath.row < estado.variables.count {
            let v = estado.variables[indexPath.row]
            expresion.text = v.nombre
            resultado.text = presentar(v.valor)
        } else if indexPath.row < estado.variables.count + estado.funciones.count {
            let i = indexPath.row - estado.variables.count
            estado.funciones[i].visible.toggle(); guardar(); actualizarGrafica(); tabla.reloadRows(at: [indexPath], with: .automatic)
        } else {
            let i = indexPath.row - estado.variables.count - estado.funciones.count
            expresion.text = estado.historial[i].expresion
            resultado.text = presentar(estado.historial[i].resultado)
        }
    }

    func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath)
        -> UISwipeActionsConfiguration? {
        let borrar = UIContextualAction(style: .destructive, title: "Borrar") { [weak self] _, _, fin in
            guard let self else { fin(false); return }
            if indexPath.row < self.estado.variables.count {
                guard !self.estado.variables[indexPath.row].congelada else { fin(false); return }
                self.estado.variables.remove(at: indexPath.row)
            } else if indexPath.row < self.estado.variables.count + self.estado.funciones.count {
                self.estado.funciones.remove(at: indexPath.row - self.estado.variables.count)
            } else {
                self.estado.historial.remove(at: indexPath.row - self.estado.variables.count - self.estado.funciones.count)
            }
            self.guardar(); self.actualizarGrafica(); tableView.reloadData(); fin(true)
        }
        return UISwipeActionsConfiguration(actions: [borrar])
    }

    private func pedirIntervalo(titulo: String, campos: [String], valores: [String],
                                completado: @escaping ([Double]) -> Void) {
        let a = UIAlertController(title: titulo, message: nil, preferredStyle: .alert)
        for (i, campo) in campos.enumerated() {
            a.addTextField { $0.placeholder = campo; $0.text = valores[i]
                $0.keyboardType = .numbersAndPunctuation }
        }
        a.addAction(UIAlertAction(title: "Calcular", style: .default) { _ in
            let v = a.textFields?.compactMap {
                Double(($0.text ?? "").replacingOccurrences(of: ",", with: "."))
            } ?? []
            if v.count == campos.count { completado(v) }
        }); a.addAction(UIAlertAction(title: "Cancelar", style: .cancel)); present(a, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }

    private let colores: [UInt32] = [0xFF0A84FF, 0xFFFF453A, 0xFF30D158,
                                     0xFFFF9F0A, 0xFFBF5AF2, 0xFF64D2FF]
}

private extension UILabel {
    convenience init(texto: String) {
        self.init(); self.text = texto; self.font = .preferredFont(forTextStyle: .subheadline)
    }
    convenience init(titulo: String) {
        self.init(); self.text = titulo; self.font = .preferredFont(forTextStyle: .headline)
    }
}
