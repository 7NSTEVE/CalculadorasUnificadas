import UIKit

enum TipoComponenteElectrico: Int32, Codable, CaseIterable {
    case resistor = 0, capacitor, inductor, fuenteVoltaje, fuenteCorriente
    case diodo, led, bjtNpn, bjtPnp, mosfetN, mosfetP, opAmp, transformador
    case interruptor, cristal, rele, tierra, cable, probeVoltaje, probeCorriente
    case and, or, not, nand, nor, xor, integrado, conector
    // Se agregan al final para conservar los rawValue persistidos por versiones
    // anteriores del sidecar de esquemas.
    case capacitorPolarizado, diodoZener, fotodiodo, jfetN, jfetP, scr, triac
    case comparador, fuenteAC, fuenteVoltajeDependiente, fuenteCorrienteDependiente
    case pulsador, vcc, bateria, fusible, motor, antena
    case flipFlopD, flipFlopJK, multiplexor, demultiplexor
    case potenciometro, voltimetro, amperimetro, osciloscopio

    var nombre: String {
        switch self {
        case .resistor: return "Resistor"
        case .capacitor: return "Capacitor"
        case .inductor: return "Inductor"
        case .fuenteVoltaje: return "Fuente de voltaje"
        case .fuenteCorriente: return "Fuente de corriente"
        case .diodo: return "Diodo"
        case .led: return "LED"
        case .bjtNpn: return "BJT NPN"
        case .bjtPnp: return "BJT PNP"
        case .mosfetN: return "MOSFET N"
        case .mosfetP: return "MOSFET P"
        case .opAmp: return "Op-amp"
        case .transformador: return "Transformador"
        case .interruptor: return "Interruptor"
        case .cristal: return "Cristal"
        case .rele: return "Relé"
        case .tierra: return "Tierra"
        case .cable: return "Cable"
        case .probeVoltaje: return "Probe V"
        case .probeCorriente: return "Probe I"
        case .and: return "AND"
        case .or: return "OR"
        case .not: return "NOT"
        case .nand: return "NAND"
        case .nor: return "NOR"
        case .xor: return "XOR"
        case .integrado: return "Integrado"
        case .conector: return "Conector"
        case .capacitorPolarizado: return "Capacitor polarizado"
        case .diodoZener: return "Diodo Zener"
        case .fotodiodo: return "Fotodiodo"
        case .jfetN: return "JFET canal N"
        case .jfetP: return "JFET canal P"
        case .scr: return "SCR"
        case .triac: return "TRIAC"
        case .comparador: return "Comparador"
        case .fuenteAC: return "Fuente AC"
        case .fuenteVoltajeDependiente: return "Fuente de voltaje dependiente"
        case .fuenteCorrienteDependiente: return "Fuente de corriente dependiente"
        case .pulsador: return "Pulsador"
        case .vcc: return "VCC / alimentación"
        case .bateria: return "Batería"
        case .fusible: return "Fusible"
        case .motor: return "Motor"
        case .antena: return "Antena"
        case .flipFlopD: return "Flip-flop D"
        case .flipFlopJK: return "Flip-flop JK"
        case .multiplexor: return "Multiplexor"
        case .demultiplexor: return "Demultiplexor"
        case .potenciometro: return "Potenciómetro"
        case .voltimetro: return "Voltímetro"
        case .amperimetro: return "Amperímetro"
        case .osciloscopio: return "Osciloscopio"
        }
    }

    var prefijo: String {
        switch self {
        case .resistor: return "R"
        case .capacitor, .capacitorPolarizado: return "C"
        case .inductor: return "L"; case .fuenteVoltaje: return "V"
        case .fuenteAC: return "VAC"
        case .fuenteVoltajeDependiente: return "E"
        case .fuenteCorriente, .fuenteCorrienteDependiente: return "I"
        case .diodo, .led, .diodoZener, .fotodiodo: return "D"
        case .bjtNpn, .bjtPnp, .jfetN, .jfetP, .scr, .triac: return "Q"
        case .mosfetN, .mosfetP, .motor: return "M"
        case .opAmp, .comparador, .integrado, .flipFlopD, .flipFlopJK,
             .multiplexor, .demultiplexor: return "U"
        case .transformador: return "T"
        case .interruptor, .pulsador: return "S"
        case .cristal: return "Y"
        case .bateria: return "B"
        case .fusible: return "F"
        case .antena: return "AE"
        case .potenciometro: return "RV"
        case .voltimetro: return "VM"
        case .amperimetro: return "AM"
        case .osciloscopio: return "OSC"
        case .vcc: return "PWR"
        case .rele: return "K"; default: return "X"
        }
    }

    /// Cero significa “sin confirmar”. No se asignan valores eléctricos
    /// plausibles por apariencia porque terminarían en cálculos o netlists.
    var valorDefecto: Double { 0 }

    var requiereValor: Bool {
        switch self {
        case .resistor, .capacitor, .capacitorPolarizado, .inductor,
             .fuenteVoltaje, .fuenteCorriente, .fuenteAC, .bateria,
             .fusible, .motor, .potenciometro: return true
        default: return false
        }
    }

    var tieneModeloDC: Bool {
        switch self {
        case .resistor, .capacitor, .capacitorPolarizado, .inductor,
             .fuenteVoltaje, .fuenteCorriente, .bateria, .interruptor,
             .pulsador, .tierra, .cable, .probeVoltaje, .probeCorriente:
            return true
        case .voltimetro, .osciloscopio:
            return true
        default:
            return false
        }
    }

    var tieneModeloAC: Bool {
        switch self {
        case .resistor, .capacitor, .capacitorPolarizado, .inductor,
             .fuenteVoltaje, .fuenteCorriente, .fuenteAC, .interruptor,
             .pulsador, .tierra, .cable, .probeVoltaje, .probeCorriente:
            return true
        case .voltimetro, .osciloscopio:
            return true
        default:
            return false
        }
    }
}

struct NodoEsquema: Codable, Identifiable, Equatable {
    var id = UUID(); var numero: Int; var posicion: CGPoint; var nombre = ""
}

enum TipoConexionEsquema: String, Codable, CaseIterable {
    case conductor = "Cable"
    case libre = "Cable libre"
    case curva = "Cable curvo"
    case puente = "Puente sin unión en cruces"
    case bus = "Bus"
}

enum EstandarSimboloElectrico: String, Codable, CaseIterable {
    case ansi = "ANSI"
    case iec = "IEC"
}

struct ConexionEsquema: Codable, Identifiable, Equatable {
    var id = UUID()
    var nodoA: Int
    var nodoB: Int
    var tipo: TipoConexionEsquema = .conductor
    var etiqueta = ""
    var corriente = ""
    var polaridadA = ""
    var bloqueada = false
}

struct ComponenteEsquema: Codable, Identifiable, Equatable {
    var id = UUID()
    var referencia: String
    var tipo: TipoComponenteElectrico
    var nodoA: Int
    var nodoB: Int
    var nodoControlA = 0
    var nodoControlB = 0
    var posicion: CGPoint
    var rotacion: CGFloat = 0
    var valor: Double
    var auxiliar: Double = 0
    var faseGrados: Double = 0
    var activo = true
    /// Opcionales para decodificar sidecars anteriores sin migración destructiva.
    var espejado: Bool?
    var estandarSimbolo: EstandarSimboloElectrico?
    var reconocidoDesdeTinta = false
    var confianzaReconocimiento: Float?
}

struct EsquemaEditable: Codable, Equatable {
    var id = UUID(); var titulo = "Circuito"
    var nodos: [NodoEsquema] = [NodoEsquema(numero: 0, posicion: .zero, nombre: "GND")]
    var componentes: [ComponenteEsquema] = []
    /// Opcional para abrir sidecars creados antes de existir el cableado.
    var conexiones: [ConexionEsquema]? = []
    var fechaModificacion = Date()

    mutating func agregar(_ tipo: TipoComponenteElectrico, posicion: CGPoint,
                          terminalA: CGPoint? = nil, terminalB: CGPoint? = nil,
                          confianza: Float? = nil) {
        let a = nodoCercano(terminalA ?? CGPoint(x: posicion.x - 55, y: posicion.y))
        let b = tipo == .tierra ? 0
            : nodoCercano(terminalB ?? CGPoint(x: posicion.x + 55, y: posicion.y))
        let numero = componentes.filter { $0.tipo == tipo }.count + 1
        componentes.append(ComponenteEsquema(
            referencia: "\(tipo.prefijo)\(numero)", tipo: tipo, nodoA: a, nodoB: b,
            posicion: posicion, valor: tipo.valorDefecto,
            reconocidoDesdeTinta: confianza != nil,
            confianzaReconocimiento: confianza))
        fechaModificacion = Date()
    }

    private mutating func nodoCercano(_ punto: CGPoint) -> Int {
        if let existente = nodos.min(by: { distancia($0.posicion, punto) < distancia($1.posicion, punto) }),
           distancia(existente.posicion, punto) <= 24 { return existente.numero }
        let numero = (nodos.map(\.numero).max() ?? 0) + 1
        nodos.append(NodoEsquema(numero: numero, posicion: punto))
        return numero
    }

    mutating func nodoCableado(en punto: CGPoint, tolerancia: CGFloat = 28) -> Int {
        if let existente = nodos.min(by: {
            distancia($0.posicion, punto) < distancia($1.posicion, punto)
        }), distancia(existente.posicion, punto) <= tolerancia {
            return existente.numero
        }
        return nodoCercano(punto)
    }

    mutating func conectar(_ nodoA: Int, _ nodoB: Int,
                           tipo: TipoConexionEsquema) -> Bool {
        guard nodoA != nodoB,
              nodos.contains(where: { $0.numero == nodoA }),
              nodos.contains(where: { $0.numero == nodoB }) else { return false }
        var lista = conexiones ?? []
        guard !lista.contains(where: {
            Set([$0.nodoA, $0.nodoB]) == Set([nodoA, nodoB])
        }) else { return false }
        lista.append(ConexionEsquema(nodoA: nodoA, nodoB: nodoB, tipo: tipo))
        conexiones = lista; fechaModificacion = Date(); return true
    }

    func mapaNodosElectricos() -> [Int: Int] {
        var grupos = nodos.map { Set([$0.numero]) }
        for conexion in conexiones ?? [] {
            guard let ia = grupos.firstIndex(where: { $0.contains(conexion.nodoA) }),
                  let ib = grupos.firstIndex(where: { $0.contains(conexion.nodoB) }),
                  ia != ib else { continue }
            let unidos = grupos[ia].union(grupos[ib])
            grupos.remove(at: max(ia, ib)); grupos.remove(at: min(ia, ib))
            grupos.append(unidos)
        }
        var mapa: [Int: Int] = [:]
        for grupo in grupos {
            let canonico = grupo.contains(0) ? 0 : (grupo.min() ?? 0)
            for nodo in grupo { mapa[nodo] = canonico }
        }
        return mapa
    }

    private func distancia(_ a: CGPoint, _ b: CGPoint) -> CGFloat { hypot(a.x - b.x, a.y - b.y) }

    func spice() -> String {
        var lineas = ["* \(titulo) — generado localmente por Tinta"]
        for c in componentes where c.activo {
            if c.tipo.requiereValor && !(c.valor > 0) {
                lineas.append("* OMITIDO \(c.referencia): falta un valor confirmado")
                continue
            }
            let valor = NotacionIngenieria.formatear(c.valor)
            switch c.tipo {
            case .resistor, .capacitor, .capacitorPolarizado, .inductor,
                 .fuenteVoltaje, .fuenteCorriente:
                lineas.append("\(c.referencia) \(c.nodoA) \(c.nodoB) \(valor)")
            case .diodo, .led, .diodoZener, .fotodiodo:
                lineas.append("* \(c.referencia) \(c.nodoA) \(c.nodoB): defina un modelo SPICE confirmado")
            default:
                lineas.append("* \(c.referencia) \(c.tipo.nombre): defina modelo, pines y parámetros antes de simular")
            }
        }
        lineas.append(".end")
        return lineas.joined(separator: "\n")
    }
}

final class RepositorioEsquemas {
    static let compartido = RepositorioEsquemas()
    private let url: URL
    private var esquemas: [String: EsquemaEditable] = [:]
    private init() {
        let documentos = FileManager.default.urls(for: .documentDirectory,
                                                   in: .userDomainMask).first!
        let carpeta = documentos.appendingPathComponent("Ingenieria", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta, withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("esquemas.json")
        if let datos = try? Data(contentsOf: url),
           let leidos = try? JSONDecoder().decode([String: EsquemaEditable].self, from: datos) {
            esquemas = leidos
        }
    }
    func esquema(documento: UInt64) -> EsquemaEditable { esquemas[String(documento)] ?? EsquemaEditable() }
    func guardar(_ esquema: EsquemaEditable, documento: UInt64) {
        esquemas[String(documento)] = esquema
        if let datos = try? JSONEncoder().encode(esquemas) { try? datos.write(to: url, options: .atomic) }
    }
}

struct ResultadoSimulacionDC { var voltajes: [Double]; var error: String? }
struct ResultadoSimulacionAC { var voltajes: [ComplexLocal]; var error: String? }
struct MuestraAnalisisCircuitoIPad: Equatable {
    var eje: Double
    var real: Double
    var imaginario: Double
    var magnitud: Double { hypot(real, imaginario) }
    var faseGrados: Double { atan2(imaginario, real) * 180 / .pi }
}
struct ResultadoAnalisisCircuitoIPad {
    var muestras: [MuestraAnalisisCircuitoIPad]
    var error: String?
}
struct ComplexLocal { var real: Double; var imaginario: Double
    var magnitud: Double { hypot(real, imaginario) }
    var fase: Double { atan2(imaginario, real) * 180 / .pi }
}

enum MotorCircuitoLocal {
    static func dc(_ esquema: EsquemaEditable) -> ResultadoSimulacionDC {
        if let error = validar(esquema, analisis: .dc) {
            return ResultadoSimulacionDC(voltajes: [], error: error)
        }
        let componentes = puente(esquema)
        let capacidad = max((esquema.nodos.map(\.numero).max() ?? 0) + 1, 1)
        var voltajes = [Double](repeating: 0, count: capacidad)
        var error = [CChar](repeating: 0, count: 512)
        let capacidadSalida = voltajes.count
        let tamanoError = error.count
        let cantidad = componentes.withUnsafeBufferPointer {
            tinta_circuito_resolver_dc($0.baseAddress, $0.count, &voltajes,
                                       capacidadSalida, &error, tamanoError)
        }
        guard cantidad >= 0 else {
            return ResultadoSimulacionDC(voltajes: [], error: String(cString: error))
        }
        return ResultadoSimulacionDC(voltajes: Array(voltajes.prefix(Int(cantidad))), error: nil)
    }

    static func ac(_ esquema: EsquemaEditable, frecuencia: Double) -> ResultadoSimulacionAC {
        if let error = validar(esquema, analisis: .ac) {
            return ResultadoSimulacionAC(voltajes: [], error: error)
        }
        let componentes = puente(esquema)
        let capacidad = max((esquema.nodos.map(\.numero).max() ?? 0) + 1, 1)
        var voltajes = [TintaComplejo](repeating: TintaComplejo(), count: capacidad)
        var error = [CChar](repeating: 0, count: 512)
        let capacidadSalida = voltajes.count
        let tamanoError = error.count
        let cantidad = componentes.withUnsafeBufferPointer {
            tinta_circuito_resolver_ac($0.baseAddress, $0.count, frecuencia,
                                       &voltajes, capacidadSalida, &error, tamanoError)
        }
        guard cantidad >= 0 else {
            return ResultadoSimulacionAC(voltajes: [], error: String(cString: error))
        }
        return ResultadoSimulacionAC(voltajes: Array(voltajes.prefix(Int(cantidad)).map {
            ComplexLocal(real: $0.real, imaginario: $0.imaginario)
        }), error: nil)
    }

    static func equivalente(_ esquema: EsquemaEditable, a: Int, b: Int)
        -> (v: Double, r: Double, i: Double, error: String?) {
        if let error = validar(esquema, analisis: .dc) {
            return (0, 0, 0, error)
        }
        let componentes = puente(esquema)
        var v = 0.0, r = 0.0, i = 0.0
        var error = [CChar](repeating: 0, count: 512)
        let tamanoError = error.count
        let mapa = esquema.mapaNodosElectricos()
        let nodoA = mapa[a] ?? a
        let nodoB = mapa[b] ?? b
        let bien = componentes.withUnsafeBufferPointer {
            tinta_circuito_equivalente($0.baseAddress, $0.count,
                                       Int32(nodoA), Int32(nodoB),
                                       &v, &r, &i, &error, tamanoError)
        }
        return (v, r, i, bien ? nil : String(cString: error))
    }

    static func barridoDC(_ esquema: EsquemaEditable, indiceFuente: Int,
                          inicio: Double, fin: Double, puntos: Int,
                          nodo: Int) -> ResultadoAnalisisCircuitoIPad {
        if let error = validar(esquema, analisis: .dc) {
            return ResultadoAnalisisCircuitoIPad(muestras: [], error: error)
        }
        guard puntos >= 2, puntos <= 100_000,
              esquema.componentes.indices.contains(indiceFuente) else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: "Parámetros de barrido DC inválidos.")
        }
        let componentes = puente(esquema)
        let nodoElectrico = esquema.mapaNodosElectricos()[nodo] ?? nodo
        var salida = [TintaMuestraCircuito](repeating: TintaMuestraCircuito(),
                                             count: puntos)
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count
        let tamanoError = error.count
        let cantidad = componentes.withUnsafeBufferPointer {
            tinta_circuito_barrido_dc(
                $0.baseAddress, $0.count, Int32(indiceFuente), inicio, fin,
                Int32(puntos), Int32(nodoElectrico), &salida, capacidad,
                &error, tamanoError)
        }
        return convertir(salida, cantidad: cantidad, error: error)
    }

    static func barridoAC(_ esquema: EsquemaEditable, inicioHz: Double,
                          finHz: Double, puntosPorDecada: Int,
                          nodo: Int) -> ResultadoAnalisisCircuitoIPad {
        if let error = validar(esquema, analisis: .ac) {
            return ResultadoAnalisisCircuitoIPad(muestras: [], error: error)
        }
        guard inicioHz > 0, finHz > inicioHz, puntosPorDecada > 0 else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: "Parámetros de barrido AC inválidos.")
        }
        let estimado = ceil(log10(finHz / inicioHz) * Double(puntosPorDecada)) + 1
        guard estimado.isFinite, estimado <= 100_000 else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: "El barrido AC excede 100 000 muestras.")
        }
        let componentes = puente(esquema)
        let nodoElectrico = esquema.mapaNodosElectricos()[nodo] ?? nodo
        var salida = [TintaMuestraCircuito](repeating: TintaMuestraCircuito(),
                                             count: max(Int(estimado), 2))
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count
        let tamanoError = error.count
        let cantidad = componentes.withUnsafeBufferPointer {
            tinta_circuito_barrido_ac(
                $0.baseAddress, $0.count, inicioHz, finHz,
                Int32(puntosPorDecada), Int32(nodoElectrico), &salida, capacidad,
                &error, tamanoError)
        }
        return convertir(salida, cantidad: cantidad, error: error)
    }

    static func transitorio(_ esquema: EsquemaEditable, duracion: Double,
                            paso: Double, nodo: Int)
        -> ResultadoAnalisisCircuitoIPad {
        if let error = validar(esquema, analisis: .dc) {
            return ResultadoAnalisisCircuitoIPad(muestras: [], error: error)
        }
        guard duracion > 0, paso > 0 else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: "Duración o paso transitorio inválido.")
        }
        let estimado = ceil(duracion / paso) + 1
        guard estimado.isFinite, estimado <= 2_000_000 else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: "El transitorio excede 2 000 000 muestras.")
        }
        let componentes = puente(esquema)
        let nodoElectrico = esquema.mapaNodosElectricos()[nodo] ?? nodo
        var salida = [TintaMuestraCircuito](repeating: TintaMuestraCircuito(),
                                             count: max(Int(estimado), 2))
        var error = [CChar](repeating: 0, count: 512)
        let capacidad = salida.count
        let tamanoError = error.count
        let cantidad = componentes.withUnsafeBufferPointer {
            tinta_circuito_transitorio(
                $0.baseAddress, $0.count, duracion, paso, Int32(nodoElectrico),
                &salida, capacidad, &error, tamanoError)
        }
        return convertir(salida, cantidad: cantidad, error: error)
    }

    private static func convertir(_ salida: [TintaMuestraCircuito], cantidad: Int32,
                                  error: [CChar]) -> ResultadoAnalisisCircuitoIPad {
        guard cantidad >= 0 else {
            return ResultadoAnalisisCircuitoIPad(
                muestras: [], error: String(cString: error))
        }
        let muestras = salida.prefix(Int(cantidad)).map {
            MuestraAnalisisCircuitoIPad(eje: $0.eje, real: $0.real,
                                        imaginario: $0.imaginario)
        }
        return ResultadoAnalisisCircuitoIPad(muestras: muestras, error: nil)
    }

    private enum Analisis { case dc, ac }

    private static func validar(_ esquema: EsquemaEditable,
                                analisis: Analisis) -> String? {
        for componente in esquema.componentes where componente.activo {
            let disponible: Bool
            let nombreAnalisis: String
            switch analisis {
            case .dc:
                disponible = componente.tipo.tieneModeloDC
                nombreAnalisis = "DC"
            case .ac:
                disponible = componente.tipo.tieneModeloAC
                nombreAnalisis = "AC"
            }
            guard disponible else {
                return "\(componente.referencia) (\(componente.tipo.nombre)) no tiene "
                    + "un modelo \(nombreAnalisis) implementado. "
                    + "No se calculó un resultado parcial."
            }
            if componente.tipo.requiereValor,
               (!(componente.valor > 0) || !componente.valor.isFinite) {
                return "Confirme un valor positivo para \(componente.referencia) "
                    + "(\(componente.tipo.nombre)) antes de simular."
            }
            guard componente.faseGrados.isFinite, componente.auxiliar.isFinite else {
                return "\(componente.referencia) contiene un parámetro no finito."
            }
        }
        return nil
    }

    private static func puente(_ esquema: EsquemaEditable) -> [TintaComponenteElectrico] {
        let mapa = esquema.mapaNodosElectricos()
        return esquema.componentes.map { c in
            TintaComponenteElectrico(
                id: nil, tipo: c.tipo.rawValue,
                nodo_a: Int32(mapa[c.nodoA] ?? c.nodoA),
                nodo_b: Int32(mapa[c.nodoB] ?? c.nodoB),
                nodo_control_a: Int32(mapa[c.nodoControlA] ?? c.nodoControlA),
                nodo_control_b: Int32(mapa[c.nodoControlB] ?? c.nodoControlB), valor: c.valor,
                auxiliar: c.auxiliar, fase_grados: c.faseGrados, activo: c.activo)
        }
    }
}

enum NotacionIngenieria {
    static func interpretar(_ entrada: String) -> Double? {
        var texto = entrada.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "µ", with: "u")
            .replacingOccurrences(of: "μ", with: "u")
            .replacingOccurrences(of: ",", with: ".")
            .replacingOccurrences(of: " ", with: "")
        texto = texto.replacingOccurrences(
            of: #"(?i)(ohms?|ohmios?|Ω|faradios?|henrios?|volts?|voltios?|amperios?|amps?|watts?|hertz|hz|segundos?|f|h|v|a|w|s)$"#,
            with: "", options: .regularExpression)
        let incrustada = #"^([+-]?\d+)(p|n|u|m|k|M|G|T)(\d+)$"#
        if let expresion = try? NSRegularExpression(pattern: incrustada),
           let coincidencia = expresion.firstMatch(
                in: texto, range: NSRange(texto.startIndex..., in: texto)),
           coincidencia.numberOfRanges == 4,
           let r1 = Range(coincidencia.range(at: 1), in: texto),
           let rp = Range(coincidencia.range(at: 2), in: texto),
           let r2 = Range(coincidencia.range(at: 3), in: texto),
           let entero = Double(texto[r1]),
           let decimales = Double("0." + texto[r2]),
           let escala = escalaPrefijo(String(texto[rp])) {
            let signo = entero < 0 ? -1.0 : 1.0
            return (entero + signo * decimales) * escala
        }
        let sufijo = #"^([+-]?(?:\d+(?:\.\d*)?|\.\d+))(Meg|p|n|u|m|k|M|G|T)?$"#
        guard let expresion = try? NSRegularExpression(pattern: sufijo),
              let coincidencia = expresion.firstMatch(
                in: texto, range: NSRange(texto.startIndex..., in: texto)),
              let rv = Range(coincidencia.range(at: 1), in: texto),
              let valor = Double(texto[rv]) else { return nil }
        guard coincidencia.range(at: 2).location != NSNotFound,
              let rp = Range(coincidencia.range(at: 2), in: texto) else {
            return valor
        }
        return escalaPrefijo(String(texto[rp])).map { valor * $0 }
    }

    private static func escalaPrefijo(_ prefijo: String) -> Double? {
        ["p": 1e-12, "n": 1e-9, "u": 1e-6, "m": 1e-3,
         "k": 1e3, "M": 1e6, "Meg": 1e6, "G": 1e9, "T": 1e12][prefijo]
    }

    static func formatear(_ valor: Double, unidad: String = "") -> String {
        guard valor != 0, valor.isFinite else { return "\(valor)\(unidad)" }
        let prefijos: [Int: String] = [-12: "p", -9: "n", -6: "u", -3: "m",
                                       0: "", 3: "k", 6: "Meg", 9: "G", 12: "T"]
        let exponente = min(max(Int(floor(log10(abs(valor)) / 3)) * 3, -12), 12)
        return String(format: "%.4g%@%@", valor / pow(10, Double(exponente)),
                      prefijos[exponente] ?? "", unidad)
    }
}

final class VistaEsquemaEditable: UIView {
    var esquema: EsquemaEditable { didSet { setNeedsDisplay() } }
    var alCambiar: ((EsquemaEditable) -> Void)?
    var modoCableado = false { didSet { nodoCableInicial = nil; setNeedsDisplay() } }
    var tipoConexion: TipoConexionEsquema = .conductor
    private var componenteArrastrado: UUID?
    private var nodoCableInicial: Int?
    private var nodoResaltado: Int?

    init(esquema: EsquemaEditable) {
        self.esquema = esquema
        super.init(frame: .zero)
        backgroundColor = .systemBackground
        let arrastre = UIPanGestureRecognizer(target: self, action: #selector(arrastrar(_:)))
        let toque = UITapGestureRecognizer(target: self, action: #selector(cablear(_:)))
        arrastre.require(toFail: toque)
        addGestureRecognizer(arrastre); addGestureRecognizer(toque)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func draw(_ rect: CGRect) {
        guard let c = UIGraphicsGetCurrentContext() else { return }
        UIColor.systemGray5.setStroke(); c.setLineWidth(0.5)
        stride(from: 0 as CGFloat, through: bounds.width, by: 20).forEach {
            c.move(to: CGPoint(x: $0, y: 0)); c.addLine(to: CGPoint(x: $0, y: bounds.height))
        }
        stride(from: 0 as CGFloat, through: bounds.height, by: 20).forEach {
            c.move(to: CGPoint(x: 0, y: $0)); c.addLine(to: CGPoint(x: bounds.width, y: $0))
        }
        c.strokePath()
        for conexion in esquema.conexiones ?? [] { dibujar(conexion, contexto: c) }
        for componente in esquema.componentes {
            dibujarEnlacesTerminales(componente, contexto: c)
            dibujar(componente, contexto: c)
        }
        let mapa = esquema.mapaNodosElectricos()
        let redSeleccionada = nodoResaltado.flatMap { mapa[$0] }
        for nodo in esquema.nodos where nodo.numero != 0 {
            let pertenece = redSeleccionada != nil && mapa[nodo.numero] == redSeleccionada
            (pertenece ? UIColor.systemOrange : UIColor.label).setFill()
            c.fillEllipse(in: CGRect(x: nodo.posicion.x - (pertenece ? 5 : 3),
                                     y: nodo.posicion.y - (pertenece ? 5 : 3),
                                     width: pertenece ? 10 : 6,
                                     height: pertenece ? 10 : 6))
        }
    }

    private func dibujar(_ conexion: ConexionEsquema, contexto c: CGContext) {
        guard let a = esquema.nodos.first(where: { $0.numero == conexion.nodoA })?.posicion,
              let b = esquema.nodos.first(where: { $0.numero == conexion.nodoB })?.posicion else {
            return
        }
        c.saveGState(); defer { c.restoreGState() }
        let mapa = esquema.mapaNodosElectricos()
        let redSeleccionada = nodoResaltado.flatMap { mapa[$0] }
        let resaltada = redSeleccionada != nil && mapa[conexion.nodoA] == redSeleccionada
        c.setStrokeColor((resaltada ? UIColor.systemOrange : UIColor.systemBlue).cgColor)
        c.setLineWidth(conexion.tipo == .bus ? 5 : 2)
        let ruta = UIBezierPath(); ruta.move(to: a)
        switch conexion.tipo {
        case .puente:
            let centro = CGPoint(x: (a.x + b.x) / 2, y: (a.y + b.y) / 2)
            ruta.addLine(to: CGPoint(x: centro.x - 10, y: centro.y))
            ruta.addArc(withCenter: centro, radius: 10,
                        startAngle: .pi, endAngle: 0, clockwise: true)
            ruta.addLine(to: b)
        case .libre:
            ruta.addLine(to: b)
        case .curva:
            let dx = b.x - a.x
            ruta.addCurve(to: b,
                          controlPoint1: CGPoint(x: a.x + dx * 0.42, y: a.y),
                          controlPoint2: CGPoint(x: b.x - dx * 0.42, y: b.y))
        case .conductor, .bus:
            let esquina = CGPoint(x: b.x, y: a.y)
            ruta.addLine(to: esquina); ruta.addLine(to: b)
        }
        if conexion.bloqueada { ruta.setLineDash([5, 4], count: 2, phase: 0) }
        ruta.stroke()
        let etiqueta = [conexion.polaridadA, conexion.etiqueta, conexion.corriente]
            .filter { !$0.isEmpty }.joined(separator: "  ")
        if !etiqueta.isEmpty {
            (etiqueta as NSString).draw(
                at: CGPoint(x: (a.x + b.x) / 2 + 6,
                            y: (a.y + b.y) / 2 - 20),
                withAttributes: [.font: UIFont.systemFont(ofSize: 11),
                                 .foregroundColor: UIColor.label])
        }
        if !conexion.corriente.isEmpty {
            let angulo = atan2(b.y - a.y, b.x - a.x)
            let punta = CGPoint(x: (a.x + b.x) / 2, y: (a.y + b.y) / 2)
            c.move(to: punta)
            c.addLine(to: CGPoint(x: punta.x - 12 * cos(angulo - .pi / 6),
                                  y: punta.y - 12 * sin(angulo - .pi / 6)))
            c.move(to: punta)
            c.addLine(to: CGPoint(x: punta.x - 12 * cos(angulo + .pi / 6),
                                  y: punta.y - 12 * sin(angulo + .pi / 6)))
            c.strokePath()
        }
    }

    private func dibujar(_ e: ComponenteEsquema, contexto c: CGContext) {
        c.saveGState(); c.translateBy(x: e.posicion.x, y: e.posicion.y); c.rotate(by: e.rotacion)
        if e.espejado == true { c.scaleBy(x: -1, y: 1) }
        let mapa = esquema.mapaNodosElectricos()
        let redSeleccionada = nodoResaltado.flatMap { mapa[$0] }
        let resaltado = redSeleccionada != nil &&
            (mapa[e.nodoA] == redSeleccionada || mapa[e.nodoB] == redSeleccionada)
        (resaltado ? UIColor.systemOrange : UIColor.systemBlue).setStroke()
        c.setLineWidth(resaltado ? 3 : 2)
        let ruta = UIBezierPath(); ruta.move(to: CGPoint(x: -55, y: 0)); ruta.addLine(to: CGPoint(x: -28, y: 0))
        switch e.tipo {
        case .resistor, .potenciometro:
            if (e.estandarSimbolo ?? .ansi) == .iec {
                ruta.append(UIBezierPath(rect: CGRect(x: -28, y: -9,
                                                       width: 56, height: 18)))
            } else {
                for i in 0..<7 {
                    ruta.addLine(to: CGPoint(x: -28 + CGFloat(i + 1) * 7,
                                             y: i.isMultiple(of: 2) ? -10 : 10))
                }
            }
            if e.tipo == .potenciometro {
                ruta.move(to: CGPoint(x: 0, y: -30))
                ruta.addLine(to: CGPoint(x: 0, y: -8))
                ruta.move(to: CGPoint(x: -6, y: -16))
                ruta.addLine(to: CGPoint(x: 0, y: -8))
                ruta.addLine(to: CGPoint(x: 6, y: -16))
            }
        case .capacitor, .capacitorPolarizado:
            ruta.move(to: CGPoint(x: -8, y: -20)); ruta.addLine(to: CGPoint(x: -8, y: 20))
            ruta.move(to: CGPoint(x: 8, y: -20)); ruta.addLine(to: CGPoint(x: 8, y: 20))
        case .inductor:
            for i in 0..<4 { ruta.addArc(withCenter: CGPoint(x: -21 + CGFloat(i) * 14, y: 0),
                                         radius: 7, startAngle: .pi, endAngle: 0, clockwise: true) }
        case .fuenteVoltaje, .fuenteCorriente, .fuenteAC, .motor:
            ruta.append(UIBezierPath(ovalIn: CGRect(x: -20, y: -20, width: 40, height: 40)))
        case .fuenteVoltajeDependiente, .fuenteCorrienteDependiente:
            ruta.move(to: CGPoint(x: 0, y: -24)); ruta.addLine(to: CGPoint(x: 24, y: 0))
            ruta.addLine(to: CGPoint(x: 0, y: 24)); ruta.addLine(to: CGPoint(x: -24, y: 0))
            ruta.close()
        case .bateria:
            ruta.move(to: CGPoint(x: -9, y: -20)); ruta.addLine(to: CGPoint(x: -9, y: 20))
            ruta.move(to: CGPoint(x: 9, y: -12)); ruta.addLine(to: CGPoint(x: 9, y: 12))
        case .diodo, .led, .diodoZener, .fotodiodo:
            ruta.move(to: CGPoint(x: -18, y: -18)); ruta.addLine(to: CGPoint(x: 8, y: 0))
            ruta.addLine(to: CGPoint(x: -18, y: 18)); ruta.close()
            ruta.move(to: CGPoint(x: 10, y: -20)); ruta.addLine(to: CGPoint(x: 10, y: 20))
        case .opAmp, .comparador:
            ruta.move(to: CGPoint(x: -24, y: -24)); ruta.addLine(to: CGPoint(x: 24, y: 0))
            ruta.addLine(to: CGPoint(x: -24, y: 24)); ruta.close()
        case .interruptor, .pulsador:
            ruta.move(to: CGPoint(x: -22, y: 0)); ruta.addLine(to: CGPoint(x: 18, y: -18))
            ruta.append(UIBezierPath(ovalIn: CGRect(x: -25, y: -3, width: 6, height: 6)))
            ruta.append(UIBezierPath(ovalIn: CGRect(x: 19, y: -3, width: 6, height: 6)))
        case .fusible:
            ruta.append(UIBezierPath(roundedRect: CGRect(x: -22, y: -8,
                                                         width: 44, height: 16),
                                     cornerRadius: 8))
        case .vcc:
            ruta.move(to: CGPoint(x: 0, y: 18)); ruta.addLine(to: CGPoint(x: 0, y: -18))
            ruta.move(to: CGPoint(x: -10, y: -8)); ruta.addLine(to: CGPoint(x: 0, y: -18))
            ruta.addLine(to: CGPoint(x: 10, y: -8))
        case .antena:
            ruta.move(to: CGPoint(x: 0, y: 20)); ruta.addLine(to: CGPoint(x: 0, y: -12))
            ruta.move(to: CGPoint(x: 0, y: -12)); ruta.addLine(to: CGPoint(x: -20, y: -28))
            ruta.move(to: CGPoint(x: 0, y: -12)); ruta.addLine(to: CGPoint(x: 20, y: -28))
        case .voltimetro, .amperimetro:
            ruta.append(UIBezierPath(ovalIn: CGRect(x: -21, y: -21,
                                                     width: 42, height: 42)))
            let letra = e.tipo == .voltimetro ? "V" : "A"
            (letra as NSString).draw(at: CGPoint(x: -7, y: -11), withAttributes: [
                .font: UIFont.boldSystemFont(ofSize: 18),
                .foregroundColor: UIColor.systemBlue])
        case .osciloscopio:
            ruta.append(UIBezierPath(roundedRect: CGRect(x: -25, y: -20,
                                                         width: 50, height: 40),
                                     cornerRadius: 5))
            ruta.move(to: CGPoint(x: -18, y: 5))
            ruta.addCurve(to: CGPoint(x: 18, y: -5),
                          controlPoint1: CGPoint(x: -8, y: -20),
                          controlPoint2: CGPoint(x: 8, y: 20))
        case .tierra:
            ruta.move(to: .zero); ruta.addLine(to: CGPoint(x: 0, y: 18))
            for i in 0..<3 { let w = CGFloat(18 - i * 6)
                ruta.move(to: CGPoint(x: -w / 2, y: 18 + CGFloat(i * 5)))
                ruta.addLine(to: CGPoint(x: w / 2, y: 18 + CGFloat(i * 5))) }
        default:
            ruta.append(UIBezierPath(roundedRect: CGRect(x: -24, y: -18, width: 48, height: 36),
                                     cornerRadius: 5))
        }
        ruta.move(to: CGPoint(x: 28, y: 0)); ruta.addLine(to: CGPoint(x: 55, y: 0))
        ruta.stroke()
        let detalleValor = e.valor > 0 ? "  \(NotacionIngenieria.formatear(e.valor))" : ""
        let texto = "\(e.referencia)\(detalleValor)" as NSString
        texto.draw(at: CGPoint(x: -45, y: 25), withAttributes: [
            .font: UIFont.systemFont(ofSize: 11), .foregroundColor: UIColor.label])
        c.restoreGState()
    }

    private func dibujarEnlacesTerminales(_ componente: ComponenteEsquema,
                                          contexto c: CGContext) {
        guard let nodoA = esquema.nodos.first(where: { $0.numero == componente.nodoA }),
              let nodoB = esquema.nodos.first(where: { $0.numero == componente.nodoB }) else {
            return
        }
        let a = terminal(componente, lado: -1)
        let b = terminal(componente, lado: 1)
        let mapa = esquema.mapaNodosElectricos()
        let redSeleccionada = nodoResaltado.flatMap { mapa[$0] }
        let resaltado = redSeleccionada != nil &&
            (mapa[componente.nodoA] == redSeleccionada ||
             mapa[componente.nodoB] == redSeleccionada)
        c.saveGState(); defer { c.restoreGState() }
        c.setStrokeColor((resaltado ? UIColor.systemOrange : UIColor.systemBlue).cgColor)
        c.setLineWidth(resaltado ? 3 : 1.5)
        let ruta = UIBezierPath()
        ruta.move(to: a)
        ruta.addLine(to: CGPoint(x: nodoA.posicion.x, y: a.y))
        ruta.addLine(to: nodoA.posicion)
        ruta.move(to: b)
        ruta.addLine(to: CGPoint(x: nodoB.posicion.x, y: b.y))
        ruta.addLine(to: nodoB.posicion)
        ruta.stroke()
    }

    private func terminal(_ componente: ComponenteEsquema, lado: CGFloat) -> CGPoint {
        let direccion = componente.espejado == true ? -lado : lado
        let local = CGPoint(x: direccion * 55, y: 0)
        let coseno = cos(componente.rotacion)
        let seno = sin(componente.rotacion)
        return CGPoint(x: componente.posicion.x + local.x * coseno - local.y * seno,
                       y: componente.posicion.y + local.x * seno + local.y * coseno)
    }

    @objc private func arrastrar(_ gesto: UIPanGestureRecognizer) {
        guard !modoCableado else { return }
        let punto = gesto.location(in: self)
        if gesto.state == .began {
            componenteArrastrado = esquema.componentes.min(by: {
                hypot($0.posicion.x - punto.x, $0.posicion.y - punto.y)
                    < hypot($1.posicion.x - punto.x, $1.posicion.y - punto.y)
            }).flatMap { hypot($0.posicion.x - punto.x, $0.posicion.y - punto.y) < 55 ? $0.id : nil }
        }
        if let id = componenteArrastrado,
           let i = esquema.componentes.firstIndex(where: { $0.id == id }) {
            esquema.componentes[i].posicion = CGPoint(x: round(punto.x / 10) * 10,
                                                       y: round(punto.y / 10) * 10)
            setNeedsDisplay()
        }
        if gesto.state == .ended { componenteArrastrado = nil; alCambiar?(esquema) }
    }

    @objc private func cablear(_ gesto: UITapGestureRecognizer) {
        let punto = gesto.location(in: self)
        guard modoCableado else {
            if let cercano = esquema.nodos.min(by: {
                hypot($0.posicion.x - punto.x, $0.posicion.y - punto.y)
                    < hypot($1.posicion.x - punto.x, $1.posicion.y - punto.y)
            }), hypot(cercano.posicion.x - punto.x,
                      cercano.posicion.y - punto.y) <= 32 {
                nodoResaltado = nodoResaltado == cercano.numero ? nil : cercano.numero
                setNeedsDisplay()
            }
            return
        }
        let nodo = esquema.nodoCableado(en: punto)
        guard let inicial = nodoCableInicial else {
            nodoCableInicial = nodo; setNeedsDisplay(); return
        }
        _ = esquema.conectar(inicial, nodo, tipo: tipoConexion)
        nodoCableInicial = nil; alCambiar?(esquema); setNeedsDisplay()
    }
}
