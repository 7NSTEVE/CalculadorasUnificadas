import Foundation
import UIKit

enum DimensionCajaTextoIPad: String, Codable, CaseIterable {
    case anchoFijoAltoAutomatico
    case tamanoAutomatico
    case cajaFija

    var nombre: String {
        switch self {
        case .anchoFijoAltoAutomatico: return "Ancho fijo · alto automático"
        case .tamanoAutomatico: return "Autosize"
        case .cajaFija: return "Caja fija"
        }
    }
}

struct AjustesCajaTextoIPad: Codable, Equatable {
    var dimension: DimensionCajaTextoIPad = .anchoFijoAltoAutomatico
    var padding: CGFloat = 10
    var markdown = false
    var continuarListas = true
    var checkboxes = true
    var matematicasInline = true

    /// Contexto efímero usado únicamente para que consumidores legacy que aún
    /// reciben `texto.id` puedan escribir en el namespace documental correcto.
    /// Los repositorios limpian este valor antes de persistirlo.
    var documentoContexto: UInt64? = nil
}

private struct ArchivoAjustesTextoIPad: Codable {
    var formato = 1
    var valores: [String: AjustesCajaTextoIPad] = [:]
}

final class RepositorioAjustesTextoIPad {
    static let compartido = RepositorioAjustesTextoIPad()
    private let cola = DispatchQueue(label: "tinta.texto.avanzado")
    private let bloqueoContexto = NSLock()
    private let url: URL
    private var archivo: ArchivoAjustesTextoIPad
    private var contextoDocumentoPorTexto: [String: UInt64] = [:]

    private init() {
        let soporte = FileManager.default.urls(for: .applicationSupportDirectory,
                                               in: .userDomainMask)[0]
        let carpeta = soporte.appendingPathComponent("Tinta/Texto", isDirectory: true)
        try? FileManager.default.createDirectory(at: carpeta,
                                                 withIntermediateDirectories: true)
        url = carpeta.appendingPathComponent("ajustes-cajas.json")
        archivo = (try? Data(contentsOf: url)).flatMap {
            try? JSONDecoder().decode(ArchivoAjustesTextoIPad.self, from: $0)
        } ?? ArchivoAjustesTextoIPad()
        archivo.valores = archivo.valores.mapValues(Self.limpiarContexto)
    }

    private static func limpiarContexto(_ valor: AjustesCajaTextoIPad)
        -> AjustesCajaTextoIPad {
        var limpio = valor
        limpio.documentoContexto = nil
        return limpio
    }

    /// Abre un contexto corto y explícito alrededor de una llamada legacy.
    /// `AppPrincipal` lee los ajustes de forma sincrónica antes de presentar el
    /// panel; el struct devuelto conserva `documentoContexto` y por eso el save
    /// posterior sigue dirigido al documento correcto aunque este contexto ya
    /// se haya liberado.
    func conContexto(documento: UInt64, id: String, _ cuerpo: () -> Void) {
        bloqueoContexto.lock()
        let anterior = contextoDocumentoPorTexto[id]
        contextoDocumentoPorTexto[id] = documento
        bloqueoContexto.unlock()
        defer {
            bloqueoContexto.lock()
            if let anterior {
                contextoDocumentoPorTexto[id] = anterior
            } else if contextoDocumentoPorTexto[id] == documento {
                contextoDocumentoPorTexto.removeValue(forKey: id)
            }
            bloqueoContexto.unlock()
        }
        cuerpo()
    }

    func ajustes(_ id: String) -> AjustesCajaTextoIPad {
        bloqueoContexto.lock()
        let documento = contextoDocumentoPorTexto[id]
        bloqueoContexto.unlock()
        guard let documento else { return ajustesLegacy(id) }
        var valor = RepositorioAjustesTextoDocumentoIPad.compartido.ajustes(
            documento: documento, id: id)
        valor.documentoContexto = documento
        return valor
    }

    /// Lectura pura del formato histórico. La usa el repositorio documental
    /// para migración lazy y evita una recursión cuando existe contexto activo.
    func ajustesLegacy(_ id: String) -> AjustesCajaTextoIPad {
        cola.sync {
            Self.limpiarContexto(archivo.valores[id] ?? AjustesCajaTextoIPad())
        }
    }

    func guardar(_ ajustes: AjustesCajaTextoIPad, id: String) {
        if let documento = ajustes.documentoContexto {
            _ = RepositorioAjustesTextoDocumentoIPad.compartido.guardar(
                Self.limpiarContexto(ajustes), documento: documento, id: id)
            return
        }
        guardarLegacy(ajustes, id: id)
    }

    func guardarLegacy(_ ajustes: AjustesCajaTextoIPad, id: String) {
        cola.sync {
            archivo.valores[id] = Self.limpiarContexto(ajustes)
            guard let datos = try? JSONEncoder().encode(archivo) else { return }
            try? datos.write(to: url,
                             options: [.atomic, .completeFileProtection])
        }
    }

    func duplicar(origen: String, destino: String) {
        guardarLegacy(ajustesLegacy(origen), id: destino)
    }
}

enum FormateadorMarkdownCajaIPad {
    static func atribuido(_ texto: String, ajustes: AjustesCajaTextoIPad,
                          fuente: UIFont, color: UIColor) -> NSAttributedString {
        let salida = NSMutableAttributedString(
            string: texto,
            attributes: [.font: fuente, .foregroundColor: color])
        guard ajustes.markdown || ajustes.matematicasInline else { return salida }
        let rangoTotal = NSRange(location: 0, length: salida.length)
        if ajustes.markdown {
            aplicar(#"(?m)^#{1,3}\s+.*$"#, en: salida, rango: rangoTotal,
                    atributos: [
                        .font: UIFont.preferredFont(forTextStyle: .title3),
                        .foregroundColor: UIColor.label,
                    ])
            aplicar(#"\*\*[^\n*]+\*\*"#, en: salida, rango: rangoTotal,
                    atributos: [.font: UIFont.boldSystemFont(ofSize: fuente.pointSize)])
            aplicar(#"(?m)^\s*(?:-|\*|•)\s+"#, en: salida, rango: rangoTotal,
                    atributos: [.foregroundColor: UIColor.systemBlue])
            aplicar(#"(?m)^\s*\[[ xX]\]\s+"#, en: salida, rango: rangoTotal,
                    atributos: [.foregroundColor: UIColor.systemGreen])
        }
        if ajustes.matematicasInline {
            aplicar(#"\$[^\n$]+\$"#, en: salida, rango: rangoTotal,
                    atributos: [
                        .font: UIFont.monospacedSystemFont(
                            ofSize: fuente.pointSize, weight: .medium),
                        .backgroundColor: UIColor.systemPurple.withAlphaComponent(0.12),
                    ])
        }
        return salida
    }

    static func prefijoContinuacion(_ texto: String) -> String? {
        guard texto.hasSuffix("\n") else { return nil }
        let lineas = texto.dropLast().split(separator: "\n",
                                            omittingEmptySubsequences: false)
        guard let anterior = lineas.last else { return nil }
        let cadena = String(anterior)
        if cadena.range(of: #"^\s*[-*•]\s+.+"#,
                        options: .regularExpression) != nil {
            let sangria = cadena.prefix { $0 == " " || $0 == "\t" }
            return "\(sangria)- "
        }
        if cadena.range(of: #"^\s*\[[ xX]\]\s+.+"#,
                        options: .regularExpression) != nil {
            let sangria = cadena.prefix { $0 == " " || $0 == "\t" }
            return "\(sangria)[ ] "
        }
        return nil
    }

    private static func aplicar(_ patron: String,
                                en texto: NSMutableAttributedString,
                                rango: NSRange,
                                atributos: [NSAttributedString.Key: Any]) {
        guard let regex = try? NSRegularExpression(pattern: patron) else { return }
        for coincidencia in regex.matches(in: texto.string, range: rango) {
            texto.addAttributes(atributos, range: coincidencia.range)
        }
    }
}

final class PanelAjustesCajaTextoIPad: UIViewController {
    var alAplicar: ((AjustesCajaTextoIPad) -> Void)?
    private var ajustes: AjustesCajaTextoIPad
    private let dimension = UISegmentedControl(
        items: ["Ancho fijo", "Autosize", "Caja fija"])
    private let padding = UISlider()
    private let markdown = UISwitch()
    private let listas = UISwitch()
    private let checkboxes = UISwitch()
    private let matematicas = UISwitch()

    init(ajustes: AjustesCajaTextoIPad) {
        self.ajustes = ajustes
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); title = "Caja de texto"; view.backgroundColor = .systemBackground
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Aplicar", style: .done, target: self, action: #selector(aplicar))
        dimension.selectedSegmentIndex = DimensionCajaTextoIPad.allCases
            .firstIndex(of: ajustes.dimension) ?? 0
        padding.minimumValue = 0; padding.maximumValue = 40
        padding.value = Float(ajustes.padding)
        markdown.isOn = ajustes.markdown
        listas.isOn = ajustes.continuarListas
        checkboxes.isOn = ajustes.checkboxes
        matematicas.isOn = ajustes.matematicasInline
        let pila = UIStackView(arrangedSubviews: [
            etiqueta("Dimensionamiento"), dimension,
            etiqueta("Padding"), padding,
            fila("Markdown opcional", markdown),
            fila("Continuar listas al pulsar retorno", listas),
            fila("Checkboxes [ ]", checkboxes),
            fila("Matemática inline $…$", matematicas),
        ])
        pila.axis = .vertical; pila.spacing = 15
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                       constant: 24),
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24),
        ])
    }

    private func etiqueta(_ texto: String) -> UILabel {
        let etiqueta = UILabel(); etiqueta.text = texto
        etiqueta.font = .preferredFont(forTextStyle: .headline); return etiqueta
    }
    private func fila(_ texto: String, _ control: UIView) -> UIView {
        let etiqueta = UILabel(); etiqueta.text = texto
        let fila = UIStackView(arrangedSubviews: [etiqueta, control])
        fila.axis = .horizontal; fila.distribution = .equalSpacing; return fila
    }
    @objc private func aplicar() {
        ajustes.dimension = DimensionCajaTextoIPad.allCases[
            max(dimension.selectedSegmentIndex, 0)]
        ajustes.padding = CGFloat(padding.value)
        ajustes.markdown = markdown.isOn
        ajustes.continuarListas = listas.isOn
        ajustes.checkboxes = checkboxes.isOn
        ajustes.matematicasInline = matematicas.isOn
        alAplicar?(ajustes); dismiss(animated: true)
    }
    @objc private func cerrar() { dismiss(animated: true) }
}
