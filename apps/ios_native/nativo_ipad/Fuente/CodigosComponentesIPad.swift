import Foundation
import UIKit

enum ColorBandaResistorIPad: String, CaseIterable {
    case negro, marron = "marrón", rojo, naranja, amarillo, verde, azul
    case violeta, gris, blanco, dorado, plateado

    var digito: Int? {
        [.negro: 0, .marron: 1, .rojo: 2, .naranja: 3, .amarillo: 4,
         .verde: 5, .azul: 6, .violeta: 7, .gris: 8, .blanco: 9][self]
    }
    var exponente: Int? {
        if let digito { return digito }
        if self == .dorado { return -1 }
        if self == .plateado { return -2 }
        return nil
    }
    var tolerancia: Double? {
        [.marron: 1, .rojo: 2, .verde: 0.5, .azul: 0.25,
         .violeta: 0.1, .gris: 0.05, .dorado: 5, .plateado: 10][self]
    }
}

enum CodificadorComponentesIPad {
    static func bandasResistor(valor: Double, tolerancia: Double,
                               cincoBandas: Bool) -> [ColorBandaResistorIPad]? {
        guard valor > 0, valor.isFinite else { return nil }
        let cifras = cincoBandas ? 3 : 2
        var exponente = Int(floor(log10(valor))) - (cifras - 1)
        var significativo = Int((valor / pow(10, Double(exponente))).rounded())
        let limite = Int(pow(10, Double(cifras)))
        if significativo >= limite { significativo /= 10; exponente += 1 }
        guard (-2...9).contains(exponente),
              significativo >= Int(pow(10, Double(cifras - 1))) else { return nil }
        let texto = String(format: "%0*d", cifras, significativo)
        var salida = texto.compactMap { caracter in
            caracter.wholeNumberValue.flatMap(colorDigito)
        }
        guard salida.count == cifras,
              let multiplicador = colorExponente(exponente),
              let bandaTolerancia = colorTolerancia(tolerancia) else { return nil }
        salida.append(multiplicador); salida.append(bandaTolerancia)
        return salida
    }

    static func decodificarSMD(_ entrada: String) -> Double? {
        let codigo = entrada.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: ",", with: ".")
        if codigo.range(of: #"(?i)^\d*R\d*$"#,
                        options: .regularExpression) != nil {
            return Double(codigo.uppercased().replacingOccurrences(of: "R", with: "."))
        }
        if codigo.count == 3, codigo.allSatisfy(\.isNumber),
           let cifras = Int(codigo.prefix(2)), let exponente = Int(codigo.suffix(1)) {
            return Double(cifras) * pow(10, Double(exponente))
        }
        if codigo.count == 4, codigo.allSatisfy(\.isNumber),
           let cifras = Int(codigo.prefix(3)), let exponente = Int(codigo.suffix(1)) {
            return Double(cifras) * pow(10, Double(exponente))
        }
        let mayuscula = codigo.uppercased()
        guard mayuscula.count == 3,
              let indice = Int(mayuscula.prefix(2)), (1...96).contains(indice),
              let letra = mayuscula.last,
              let multiplicador = multiplicadoresEIA96[letra] else { return nil }
        return Double(valoresEIA96[indice - 1]) * multiplicador
    }

    static func decodificarCapacitor(_ entrada: String) -> Double? {
        let limpio = entrada.trimmingCharacters(in: .whitespacesAndNewlines)
        if limpio.count == 3, limpio.allSatisfy(\.isNumber),
           let cifras = Int(limpio.prefix(2)), let exponente = Int(limpio.suffix(1)) {
            return Double(cifras) * pow(10, Double(exponente)) * 1e-12
        }
        return NotacionIngenieria.interpretar(limpio)
    }

    static func diametroAWG(_ calibre: Int) -> Double? {
        guard (-3...40).contains(calibre) else { return nil }
        return 0.127 * pow(92, Double(36 - calibre) / 39)
    }

    static func areaAWG(_ calibre: Int) -> Double? {
        diametroAWG(calibre).map { .pi * $0 * $0 / 4 }
    }

    static func calibreAWG(areaMM2: Double) -> Int? {
        guard areaMM2 > 0, areaMM2.isFinite else { return nil }
        return (-3...40).min {
            abs((areaAWG($0) ?? .greatestFiniteMagnitude) - areaMM2)
                < abs((areaAWG($1) ?? .greatestFiniteMagnitude) - areaMM2)
        }
    }

    static func valorSerieMasCercano(_ valor: Double, serie: Int) -> Double? {
        guard valor > 0, [6, 12, 24, 48, 96].contains(serie) else { return nil }
        let decada = floor(log10(valor))
        var candidatos: [Double] = []
        for desplazamiento in -1...1 {
            let base = pow(10, decada + Double(desplazamiento))
            for indice in 0..<serie {
                candidatos.append(base * pow(10, Double(indice) / Double(serie)))
            }
        }
        guard let candidato = candidatos.min(by: {
            abs($0 - valor) < abs($1 - valor)
        }) else { return nil }
        let cifras = serie <= 24 ? 2 : 3
        let exponente = floor(log10(abs(candidato)))
        let escala = pow(10, exponente - Double(cifras - 1))
        return (candidato / escala).rounded() * escala
    }

    private static func colorDigito(_ digito: Int) -> ColorBandaResistorIPad? {
        ColorBandaResistorIPad.allCases.first { $0.digito == digito }
    }
    private static func colorExponente(_ exponente: Int) -> ColorBandaResistorIPad? {
        ColorBandaResistorIPad.allCases.first { $0.exponente == exponente }
    }
    private static func colorTolerancia(_ tolerancia: Double)
        -> ColorBandaResistorIPad? {
        ColorBandaResistorIPad.allCases.min {
            abs(($0.tolerancia ?? 1_000) - tolerancia)
                < abs(($1.tolerancia ?? 1_000) - tolerancia)
        }.flatMap { abs(($0.tolerancia ?? 1_000) - tolerancia) < 0.001 ? $0 : nil }
    }

    private static let multiplicadoresEIA96: [Character: Double] = [
        "Z": 0.001, "Y": 0.01, "X": 0.1, "A": 1, "B": 10,
        "C": 100, "D": 1_000, "E": 10_000, "F": 100_000,
    ]
    private static let valoresEIA96 = [
        100,102,105,107,110,113,115,118,121,124,127,130,
        133,137,140,143,147,150,154,158,162,165,169,174,
        178,182,187,191,196,200,205,210,215,221,226,232,
        237,243,249,255,261,267,274,280,287,294,301,309,
        316,324,332,340,348,357,365,374,383,392,402,412,
        422,432,442,453,464,475,487,499,511,523,536,549,
        562,576,590,604,619,634,649,665,681,698,715,732,
        750,768,787,806,825,845,866,887,909,931,953,976,
    ]
}

final class PanelCodigosComponentesIPad: UITableViewController {
    var alInsertar: ((String) -> Void)?
    private let acciones = [
        "Bandas de resistor", "Decodificar resistor SMD",
        "Decodificar capacitor", "AWG → diámetro y mm²",
        "mm² → AWG", "Valor normalizado E6/E12/E24/E48/E96",
    ]

    init() { super.init(style: .insetGrouped) }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Códigos y conductores"
    }
    override func tableView(_ tableView: UITableView,
                            numberOfRowsInSection section: Int) -> Int { acciones.count }
    override func tableView(_ tableView: UITableView,
                            cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let celda = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        celda.textLabel?.text = acciones[indexPath.row]
        celda.detailTextLabel?.text = indexPath.row < 3
            ? "Cálculo local; revise el componente físico antes de conectar."
            : "Conversión geométrica local, sin tabla externa."
        celda.accessoryType = .disclosureIndicator
        return celda
    }
    override func tableView(_ tableView: UITableView,
                            didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        switch indexPath.row {
        case 0: pedirBandas()
        case 1: pedirUno("Código SMD", ejemplo: "103, 1001, 4R7 o 01C") {
            CodificadorComponentesIPad.decodificarSMD($0).map {
                "SMD \($0): \(NotacionIngenieria.formatear($0, unidad: "Ω"))"
            }
        }
        case 2: pedirUno("Código de capacitor", ejemplo: "104, 4u7 o 100nF") {
            CodificadorComponentesIPad.decodificarCapacitor($0).map {
                "Capacitor \($0): \(NotacionIngenieria.formatear($0, unidad: "F"))"
            }
        }
        case 3: pedirUno("Calibre AWG", ejemplo: "24") { texto in
            guard let awg = Int(texto),
                  let diametro = CodificadorComponentesIPad.diametroAWG(awg),
                  let area = CodificadorComponentesIPad.areaAWG(awg) else { return nil }
            return String(format: "AWG %d: %.4f mm de diámetro · %.5f mm²",
                          awg, diametro, area)
        }
        case 4: pedirUno("Área del conductor", ejemplo: "0.205 mm²") { texto in
            let limpio = texto.lowercased()
                .replacingOccurrences(of: "mm²", with: "")
                .replacingOccurrences(of: "mm2", with: "")
                .trimmingCharacters(in: .whitespacesAndNewlines)
            guard let area = Double(limpio.replacingOccurrences(of: ",", with: ".")),
                  let awg = CodificadorComponentesIPad.calibreAWG(areaMM2: area),
                  let real = CodificadorComponentesIPad.areaAWG(awg) else { return nil }
            return String(format: "%.5f mm² ≈ AWG %d (%.5f mm²)", area, awg, real)
        }
        default: pedirSerie()
        }
    }

    private func pedirBandas() {
        let alerta = UIAlertController(
            title: "Bandas de resistor",
            message: "Valor, tolerancia en % y cantidad de bandas (4 o 5).",
            preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "10kΩ" }
        alerta.addTextField { $0.placeholder = "1"; $0.text = "1" }
        alerta.addTextField { $0.placeholder = "4 o 5"; $0.text = "4" }
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            guard let valor = NotacionIngenieria.interpretar(
                    alerta.textFields?[0].text ?? ""),
                  let tolerancia = Double(alerta.textFields?[1].text ?? ""),
                  let cantidad = Int(alerta.textFields?[2].text ?? ""),
                  [4, 5].contains(cantidad),
                  let bandas = CodificadorComponentesIPad.bandasResistor(
                    valor: valor, tolerancia: tolerancia,
                    cincoBandas: cantidad == 5) else {
                self?.mostrarResultado(nil); return
            }
            self?.mostrarResultado(
                "\(NotacionIngenieria.formatear(valor, unidad: "Ω")): "
                    + bandas.map(\.rawValue).joined(separator: " · "))
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func pedirSerie() {
        let alerta = UIAlertController(title: "Valor normalizado",
                                       message: nil, preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = "Valor; ej. 3.3k" }
        alerta.addTextField { $0.placeholder = "Serie"; $0.text = "24" }
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            guard let valor = NotacionIngenieria.interpretar(
                    alerta.textFields?[0].text ?? ""),
                  let serie = Int(alerta.textFields?[1].text ?? ""),
                  let cercano = CodificadorComponentesIPad.valorSerieMasCercano(
                    valor, serie: serie) else { self?.mostrarResultado(nil); return }
            self?.mostrarResultado("E\(serie): \(NotacionIngenieria.formatear(cercano))")
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func pedirUno(_ titulo: String, ejemplo: String,
                          convertir: @escaping (String) -> String?) {
        let alerta = UIAlertController(title: titulo, message: nil,
                                       preferredStyle: .alert)
        alerta.addTextField { $0.placeholder = ejemplo }
        alerta.addAction(UIAlertAction(title: "Calcular", style: .default) {
            [weak self] _ in
            self?.mostrarResultado(convertir(alerta.textFields?.first?.text ?? ""))
        })
        alerta.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        present(alerta, animated: true)
    }

    private func mostrarResultado(_ resultado: String?) {
        guard let resultado else {
            let alerta = UIAlertController(
                title: "Dato no válido",
                message: "Revise el código, rango o unidad. No se asumió un valor.",
                preferredStyle: .alert)
            alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
            present(alerta, animated: true); return
        }
        let hoja = UIAlertController(title: "Resultado", message: resultado,
                                     preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(title: "Insertar en el apunte", style: .default) {
            [weak self] _ in self?.alInsertar?(resultado)
        })
        hoja.addAction(UIAlertAction(title: "Copiar", style: .default) { _ in
            UIPasteboard.general.string = resultado
        })
        hoja.addAction(UIAlertAction(title: "Cerrar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = view
        present(hoja, animated: true)
    }
}
