import UIKit

enum ContenidoGuiaFocoIPad {
    private static let prefijo = "[[CUADERNU_ESTADO]]"

    static func codificar(enunciado: String, estado: String?) -> String {
        let limpio = enunciado.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let estado = estado?.trimmingCharacters(in: .whitespacesAndNewlines),
              !estado.isEmpty else { return limpio }
        return "\(prefijo)\(estado)\n\(limpio)"
    }

    static func decodificar(_ texto: String) -> (enunciado: String, estado: String?) {
        let limpio = texto.trimmingCharacters(in: .whitespacesAndNewlines)
        guard limpio.hasPrefix(prefijo),
              let salto = limpio.firstIndex(of: "\n") else {
            return (limpio, nil)
        }
        let estado = String(limpio[limpio.index(limpio.startIndex,
                                                  offsetBy: prefijo.count)..<salto])
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let inicioEnunciado = limpio.index(after: salto)
        let enunciado = String(limpio[inicioEnunciado...])
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return (enunciado, estado.isEmpty ? nil : estado)
    }
}

/// Tarjeta efímera para mantener el enunciado/objetivo visible mientras se
/// escribe en una hoja de práctica. No modifica el documento ni forma parte de
/// la exportación. Solo su pequeña superficie recibe toques para poder plegarla.
final class VistaGuiaFocoIPad: UIVisualEffectView {
    private let etiqueta = UILabel()
    private let estado = UILabel()
    private let botonPlegar = UIButton(type: .system)
    private var plegada = false
    private var temporizadorEstado: Timer?
    private var proveedorEstado: (() -> String?)?

    init() {
        super.init(effect: UIBlurEffect(style: .systemMaterial))
        configurar()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        effect = UIBlurEffect(style: .systemMaterial)
        configurar()
    }

    private func configurar() {
        clipsToBounds = true
        layer.cornerRadius = 14
        layer.borderWidth = 1 / UIScreen.main.scale
        layer.borderColor = UIColor.separator.cgColor
        accessibilityIdentifier = "foco.guia.enunciado"

        etiqueta.font = .preferredFont(forTextStyle: .subheadline)
        etiqueta.textColor = .label
        etiqueta.numberOfLines = 4
        etiqueta.adjustsFontForContentSizeCategory = true
        etiqueta.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        estado.font = .monospacedDigitSystemFont(ofSize: 12, weight: .semibold)
        estado.textColor = .secondaryLabel
        estado.textAlignment = .right
        estado.adjustsFontForContentSizeCategory = true
        estado.setContentCompressionResistancePriority(.required, for: .horizontal)
        estado.isHidden = true

        botonPlegar.setImage(UIImage(systemName: "chevron.up"), for: .normal)
        botonPlegar.accessibilityLabel = "Plegar enunciado"
        botonPlegar.addTarget(self, action: #selector(alternarPlegado), for: .touchUpInside)

        let encabezado = UILabel()
        encabezado.text = "Enunciado"
        encabezado.font = .preferredFont(forTextStyle: .caption1)
        encabezado.textColor = .secondaryLabel
        encabezado.adjustsFontForContentSizeCategory = true

        let superior = UIStackView(arrangedSubviews: [
            encabezado, UIView(), estado, botonPlegar,
        ])
        superior.axis = .horizontal
        superior.alignment = .center
        superior.spacing = 8

        let pila = UIStackView(arrangedSubviews: [superior, etiqueta])
        pila.axis = .vertical
        pila.spacing = 5
        pila.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 12),
            pila.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -8),
            pila.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 8),
            pila.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -9),
        ])
        isHidden = true
    }

    func mostrar(
        _ texto: String,
        estadoDinamico: (() -> String?)? = nil
    ) {
        let contenido = ContenidoGuiaFocoIPad.decodificar(texto)
        guard !contenido.enunciado.isEmpty else { ocultar(); return }
        etiqueta.text = contenido.enunciado
        estado.text = contenido.estado
        estado.isHidden = contenido.estado?.isEmpty ?? true
        plegada = false
        aplicarPlegado()
        proveedorEstado = estadoDinamico
        reiniciarTemporizadorEstado()
        if estadoDinamico != nil { actualizarEstado() }
        isHidden = false
        actualizarAccesibilidad(enunciado: contenido.enunciado)
    }

    func ocultar() {
        detenerTemporizadorEstado()
        etiqueta.text = nil
        estado.text = nil
        estado.isHidden = true
        isHidden = true
        plegada = false
        accessibilityLabel = nil
    }

    private func reiniciarTemporizadorEstado() {
        detenerTemporizadorEstado()
        guard proveedorEstado != nil else { return }
        let timer = Timer(timeInterval: 1, repeats: true) { [weak self] _ in
            self?.actualizarEstado()
        }
        RunLoop.main.add(timer, forMode: .common)
        temporizadorEstado = timer
    }

    private func detenerTemporizadorEstado() {
        temporizadorEstado?.invalidate()
        temporizadorEstado = nil
        proveedorEstado = nil
    }

    private func actualizarEstado() {
        guard let proveedorEstado else { return }
        let texto = proveedorEstado()?.trimmingCharacters(in: .whitespacesAndNewlines)
        estado.text = texto
        estado.isHidden = texto?.isEmpty ?? true
        if let enunciado = etiqueta.text {
            actualizarAccesibilidad(enunciado: enunciado)
        }
    }

    private func actualizarAccesibilidad(enunciado: String) {
        if let textoEstado = estado.text, !textoEstado.isEmpty {
            accessibilityLabel = "Enunciado: \(enunciado). \(textoEstado)"
        } else {
            accessibilityLabel = "Enunciado: \(enunciado)"
        }
    }

    @objc private func alternarPlegado() {
        plegada.toggle()
        aplicarPlegado()
    }

    private func aplicarPlegado() {
        etiqueta.numberOfLines = plegada ? 1 : 4
        botonPlegar.setImage(
            UIImage(systemName: plegada ? "chevron.down" : "chevron.up"),
            for: .normal)
        botonPlegar.accessibilityLabel = plegada ? "Expandir enunciado" : "Plegar enunciado"
        invalidateIntrinsicContentSize()
        superview?.setNeedsLayout()
    }

    deinit { temporizadorEstado?.invalidate() }
}
