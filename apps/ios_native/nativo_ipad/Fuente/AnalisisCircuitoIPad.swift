import UIKit

enum ModoGraficaCircuitoIPad {
    case lineal(unidadX: String, unidadY: String)
    case magnitudFase
    case bode(referencia: Double)

    var usaEjeLogaritmico: Bool {
        switch self {
        case .lineal: return false
        case .magnitudFase, .bode: return true
        }
    }

    var tituloPrincipal: String {
        switch self {
        case .lineal(_, let unidadY): return "Respuesta [\(unidadY)]"
        case .magnitudFase: return "Magnitud [V]"
        case .bode: return "20·log₁₀(|Vout/Vin|) [dB]"
        }
    }

    var tituloSecundario: String? {
        switch self {
        case .lineal: return nil
        case .magnitudFase, .bode: return "Fase [°]"
        }
    }
}

final class VistaGraficaCircuitoIPad: UIView {
    var muestras: [MuestraAnalisisCircuitoIPad] = [] { didSet { setNeedsDisplay() } }
    var modo: ModoGraficaCircuitoIPad = .lineal(unidadX: "", unidadY: "") {
        didSet { setNeedsDisplay() }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .secondarySystemBackground
        layer.cornerRadius = 12
        clipsToBounds = true
        isAccessibilityElement = true
        accessibilityLabel = "Gráfica de análisis del circuito"
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
    }

    override func draw(_ rect: CGRect) {
        guard bounds.width > 80, bounds.height > 100 else { return }
        let margen = UIEdgeInsets(top: 30, left: 58, bottom: 38, right: 22)
        let area = bounds.inset(by: margen)
        guard area.width > 0, area.height > 0 else { return }
        let datos = muestrasGraficables()
        guard datos.count >= 2 else {
            dibujarMensaje("No hay suficientes muestras finitas", en: area)
            return
        }
        if modo.tituloSecundario == nil {
            dibujarSerie(datos.map { ($0.x, $0.y1) }, en: area,
                          color: .systemBlue, titulo: modo.tituloPrincipal,
                          etiquetaX: etiquetaEjeX())
        } else {
            let altoPrincipal = area.height * 0.57
            let separacion: CGFloat = 35
            let superior = CGRect(x: area.minX, y: area.minY,
                                  width: area.width, height: altoPrincipal)
            let inferior = CGRect(x: area.minX,
                                  y: superior.maxY + separacion,
                                  width: area.width,
                                  height: max(area.maxY - superior.maxY - separacion, 40))
            dibujarSerie(datos.map { ($0.x, $0.y1) }, en: superior,
                          color: .systemBlue, titulo: modo.tituloPrincipal,
                          etiquetaX: nil)
            dibujarSerie(datos.map { ($0.x, $0.y2 ?? 0) }, en: inferior,
                          color: .systemOrange,
                          titulo: modo.tituloSecundario ?? "Fase",
                          etiquetaX: etiquetaEjeX())
        }
    }

    private struct PuntoGrafica { var x: Double; var y1: Double; var y2: Double? }

    private func muestrasGraficables() -> [PuntoGrafica] {
        guard !muestras.isEmpty else { return [] }
        // Mantiene extremos y limita el trabajo de dibujo en barridos grandes.
        let salto = max(muestras.count / 2_000, 1)
        var indices = Array(stride(from: 0, to: muestras.count, by: salto))
        if indices.last != muestras.count - 1 { indices.append(muestras.count - 1) }
        return indices.compactMap { indice in
            let muestra = muestras[indice]
            let x = modo.usaEjeLogaritmico ? log10(muestra.eje) : muestra.eje
            let punto: PuntoGrafica
            switch modo {
            case .lineal:
                punto = PuntoGrafica(x: x, y1: muestra.real, y2: nil)
            case .magnitudFase:
                punto = PuntoGrafica(x: x, y1: muestra.magnitud,
                                     y2: muestra.faseGrados)
            case .bode(let referencia):
                let db = 20 * log10(max(muestra.magnitud / referencia, 1e-15))
                punto = PuntoGrafica(x: x, y1: db, y2: muestra.faseGrados)
            }
            guard punto.x.isFinite, punto.y1.isFinite,
                  punto.y2?.isFinite ?? true else { return nil }
            return punto
        }
    }

    private func dibujarSerie(_ puntos: [(Double, Double)], en area: CGRect,
                              color: UIColor, titulo: String,
                              etiquetaX: String?) {
        guard let minimoX = puntos.map(\.0).min(), let maximoX = puntos.map(\.0).max(),
              let minimoY = puntos.map(\.1).min(), let maximoY = puntos.map(\.1).max() else {
            return
        }
        let rangoX = max(maximoX - minimoX, 1e-12)
        let centroY = (minimoY + maximoY) / 2
        let rangoY = max(maximoY - minimoY, max(abs(centroY) * 0.02, 1e-12))

        let cuadricula = UIBezierPath()
        for paso in 0...4 {
            let x = area.minX + area.width * CGFloat(paso) / 4
            cuadricula.move(to: CGPoint(x: x, y: area.minY))
            cuadricula.addLine(to: CGPoint(x: x, y: area.maxY))
            let y = area.minY + area.height * CGFloat(paso) / 4
            cuadricula.move(to: CGPoint(x: area.minX, y: y))
            cuadricula.addLine(to: CGPoint(x: area.maxX, y: y))
        }
        UIColor.separator.setStroke(); cuadricula.lineWidth = 0.5; cuadricula.stroke()

        let ruta = UIBezierPath()
        for (indice, valor) in puntos.enumerated() {
            let x = area.minX + CGFloat((valor.0 - minimoX) / rangoX) * area.width
            let y = area.maxY - CGFloat((valor.1 - minimoY) / rangoY) * area.height
            let punto = CGPoint(x: x, y: y)
            if indice == 0 { ruta.move(to: punto) } else { ruta.addLine(to: punto) }
        }
        color.setStroke(); ruta.lineWidth = 2.2; ruta.lineJoinStyle = .round; ruta.stroke()

        let atributos: [NSAttributedString.Key: Any] = [
            .font: UIFont.monospacedSystemFont(ofSize: 10, weight: .regular),
            .foregroundColor: UIColor.secondaryLabel]
        (titulo as NSString).draw(at: CGPoint(x: area.minX, y: area.minY - 20),
                                  withAttributes: atributos)
        (formato(maximoY) as NSString).draw(
            at: CGPoint(x: 4, y: area.minY - 6), withAttributes: atributos)
        (formato(minimoY) as NSString).draw(
            at: CGPoint(x: 4, y: area.maxY - 10), withAttributes: atributos)
        if let etiquetaX {
            let inicio = modo.usaEjeLogaritmico ? pow(10, minimoX) : minimoX
            let fin = modo.usaEjeLogaritmico ? pow(10, maximoX) : maximoX
            ("\(formato(inicio)) … \(formato(fin)) \(etiquetaX)" as NSString).draw(
                at: CGPoint(x: area.minX, y: area.maxY + 8), withAttributes: atributos)
        }
    }

    private func etiquetaEjeX() -> String {
        switch modo {
        case .lineal(let unidad, _): return unidad
        case .magnitudFase, .bode: return "Hz · escala log"
        }
    }

    private func formato(_ valor: Double) -> String {
        String(format: abs(valor) >= 1_000 || (abs(valor) > 0 && abs(valor) < 0.01)
               ? "%.2e" : "%.3g", valor)
    }

    private func dibujarMensaje(_ texto: String, en area: CGRect) {
        (texto as NSString).draw(in: area, withAttributes: [
            .font: UIFont.preferredFont(forTextStyle: .body),
            .foregroundColor: UIColor.secondaryLabel])
    }
}

final class ControladorResultadoCircuitoIPad: UIViewController {
    var alInsertarImagen: ((UIImage, String) -> Void)?
    var alInsertarTexto: ((String) -> Void)?

    private let tituloAnalisis: String
    private let muestras: [MuestraAnalisisCircuitoIPad]
    private let modo: ModoGraficaCircuitoIPad
    private let grafica = VistaGraficaCircuitoIPad()
    private let resumen = UITextView()

    init(titulo: String, muestras: [MuestraAnalisisCircuitoIPad],
         modo: ModoGraficaCircuitoIPad) {
        tituloAnalisis = titulo
        self.muestras = muestras
        self.modo = modo
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no está implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = tituloAnalisis
        view.backgroundColor = .systemBackground
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Insertar", style: .plain, target: self,
            action: #selector(insertar(_:)))
        grafica.muestras = muestras
        grafica.modo = modo
        grafica.translatesAutoresizingMaskIntoConstraints = false
        resumen.text = textoResumen()
        resumen.font = .monospacedSystemFont(ofSize: 13, weight: .regular)
        resumen.isEditable = false
        resumen.backgroundColor = .tertiarySystemBackground
        resumen.layer.cornerRadius = 10
        resumen.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(grafica); view.addSubview(resumen)
        NSLayoutConstraint.activate([
            grafica.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                          constant: 12),
            grafica.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            grafica.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            grafica.heightAnchor.constraint(equalTo: view.heightAnchor, multiplier: 0.58),
            resumen.topAnchor.constraint(equalTo: grafica.bottomAnchor, constant: 12),
            resumen.leadingAnchor.constraint(equalTo: grafica.leadingAnchor),
            resumen.trailingAnchor.constraint(equalTo: grafica.trailingAnchor),
            resumen.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor,
                                             constant: -12),
        ])
    }

    @objc private func insertar(_ remitente: UIBarButtonItem) {
        let menu = UIAlertController(title: "Resultado en el apunte", message: nil,
                                     preferredStyle: .actionSheet)
        menu.addAction(UIAlertAction(title: "Insertar gráfica", style: .default) {
            [weak self] _ in self?.insertarGrafica()
        })
        menu.addAction(UIAlertAction(title: "Insertar resumen numérico", style: .default) {
            [weak self] _ in guard let self else { return }
            self.alInsertarTexto?(self.textoResumen())
        })
        menu.addAction(UIAlertAction(title: "Insertar gráfica y resumen", style: .default) {
            [weak self] _ in guard let self else { return }
            self.insertarGrafica(); self.alInsertarTexto?(self.textoResumen())
        })
        menu.addAction(UIAlertAction(title: "Copiar CSV", style: .default) {
            [weak self] _ in UIPasteboard.general.string = self?.csv()
        })
        menu.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        menu.popoverPresentationController?.barButtonItem = remitente
        present(menu, animated: true)
    }

    private func insertarGrafica() {
        guard grafica.bounds.width > 0, grafica.bounds.height > 0 else { return }
        let imagen = UIGraphicsImageRenderer(bounds: grafica.bounds).image { contexto in
            grafica.layer.render(in: contexto.cgContext)
        }
        alInsertarImagen?(imagen, tituloAnalisis)
    }

    private func textoResumen() -> String {
        guard let primera = muestras.first, let ultima = muestras.last else {
            return "\(tituloAnalisis)\nSin muestras"
        }
        let valores: [Double]
        switch modo {
        case .lineal: valores = muestras.map(\.real)
        case .magnitudFase: valores = muestras.map(\.magnitud)
        case .bode(let referencia):
            valores = muestras.map {
                20 * log10(max($0.magnitud / referencia, 1e-15))
            }
        }
        let minimo = valores.filter(\.isFinite).min() ?? 0
        let maximo = valores.filter(\.isFinite).max() ?? 0
        return [tituloAnalisis,
                "Muestras: \(muestras.count)",
                "Eje: \(formato(primera.eje)) … \(formato(ultima.eje))",
                "Mínimo: \(formato(minimo))",
                "Máximo: \(formato(maximo))",
                "Inicial: \(formato(valores.first ?? 0))",
                "Final: \(formato(valores.last ?? 0))"].joined(separator: "\n")
    }

    private func csv() -> String {
        let filas = muestras.map {
            "\($0.eje),\($0.real),\($0.imaginario),\($0.magnitud),\($0.faseGrados)"
        }
        return (["eje,real,imaginario,magnitud,fase_grados"] + filas)
            .joined(separator: "\n")
    }

    private func formato(_ valor: Double) -> String {
        String(format: "%.6g", valor)
    }
}
