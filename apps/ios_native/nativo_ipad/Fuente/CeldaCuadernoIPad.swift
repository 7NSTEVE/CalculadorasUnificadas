import UIKit

// MARK: - Celda visual de un cuaderno en la biblioteca

final class CeldaCuadernoIPad: UICollectionViewCell {
    static let reuseID = "CeldaCuaderno"

    // --- Subvistas ---
    private let contenedorPortada = UIView()
    private let etiquetaTitulo = UILabel()
    private let etiquetaFecha = UILabel()
    private let indicadorColor = UIView()
    private let iconoCuaderno = UIImageView()

    // --- Colores de portada disponibles ---
    static let coloresPortada: [UIColor] = [
        UIColor(red: 0.35, green: 0.56, blue: 0.97, alpha: 1),   // Azul
        UIColor(red: 0.95, green: 0.45, blue: 0.45, alpha: 1),   // Rojo
        UIColor(red: 0.40, green: 0.80, blue: 0.55, alpha: 1),   // Verde
        UIColor(red: 0.95, green: 0.72, blue: 0.30, alpha: 1),   // Amarillo
        UIColor(red: 0.70, green: 0.48, blue: 0.90, alpha: 1),   // Morado
        UIColor(red: 0.98, green: 0.55, blue: 0.38, alpha: 1),   // Naranja
        UIColor(red: 0.45, green: 0.75, blue: 0.82, alpha: 1),   // Teal
        UIColor(red: 0.85, green: 0.50, blue: 0.70, alpha: 1),   // Rosa
    ]
    static let nombresColoresPortada = [
        "Azul", "Rojo", "Verde", "Amarillo",
        "Morado", "Naranja", "Turquesa", "Rosa",
    ]

    override init(frame: CGRect) {
        super.init(frame: frame)
        configurar()
    }

    required init?(coder: NSCoder) { nil }

    private func configurar() {
        // Sombra del contenedor
        layer.shadowColor = UIColor.black.cgColor
        layer.shadowOpacity = 0.12
        layer.shadowRadius = 8
        layer.shadowOffset = CGSize(width: 0, height: 4)

        // Portada
        contenedorPortada.layer.cornerRadius = 16
        contenedorPortada.layer.cornerCurve = .continuous
        contenedorPortada.clipsToBounds = true
        contenedorPortada.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(contenedorPortada)

        // Icono decorativo sobre la portada
        iconoCuaderno.image = UIImage(systemName: "book.closed.fill")
        iconoCuaderno.tintColor = UIColor.white.withAlphaComponent(0.35)
        iconoCuaderno.contentMode = .scaleAspectFit
        iconoCuaderno.translatesAutoresizingMaskIntoConstraints = false
        contenedorPortada.addSubview(iconoCuaderno)

        // Indicador de color (barra inferior)
        indicadorColor.layer.cornerRadius = 2
        indicadorColor.translatesAutoresizingMaskIntoConstraints = false
        contenedorPortada.addSubview(indicadorColor)

        // Título
        etiquetaTitulo.font = .systemFont(ofSize: 14, weight: .semibold)
        etiquetaTitulo.textColor = .label
        etiquetaTitulo.numberOfLines = 2
        etiquetaTitulo.lineBreakMode = .byTruncatingTail
        etiquetaTitulo.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(etiquetaTitulo)

        // Fecha
        etiquetaFecha.font = .systemFont(ofSize: 11, weight: .regular)
        etiquetaFecha.textColor = .secondaryLabel
        etiquetaFecha.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(etiquetaFecha)

        NSLayoutConstraint.activate([
            contenedorPortada.topAnchor.constraint(equalTo: contentView.topAnchor),
            contenedorPortada.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            contenedorPortada.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
            contenedorPortada.heightAnchor.constraint(equalTo: contentView.widthAnchor,
                                                       multiplier: 1.25),

            iconoCuaderno.centerXAnchor.constraint(equalTo: contenedorPortada.centerXAnchor),
            iconoCuaderno.centerYAnchor.constraint(equalTo: contenedorPortada.centerYAnchor,
                                                     constant: -8),
            iconoCuaderno.widthAnchor.constraint(equalToConstant: 48),
            iconoCuaderno.heightAnchor.constraint(equalToConstant: 48),

            indicadorColor.leadingAnchor.constraint(equalTo: contenedorPortada.leadingAnchor,
                                                      constant: 16),
            indicadorColor.trailingAnchor.constraint(equalTo: contenedorPortada.trailingAnchor,
                                                       constant: -16),
            indicadorColor.bottomAnchor.constraint(equalTo: contenedorPortada.bottomAnchor,
                                                     constant: -12),
            indicadorColor.heightAnchor.constraint(equalToConstant: 4),

            etiquetaTitulo.topAnchor.constraint(equalTo: contenedorPortada.bottomAnchor,
                                                  constant: 8),
            etiquetaTitulo.leadingAnchor.constraint(equalTo: contentView.leadingAnchor,
                                                      constant: 4),
            etiquetaTitulo.trailingAnchor.constraint(equalTo: contentView.trailingAnchor,
                                                       constant: -4),

            etiquetaFecha.topAnchor.constraint(equalTo: etiquetaTitulo.bottomAnchor, constant: 2),
            etiquetaFecha.leadingAnchor.constraint(equalTo: etiquetaTitulo.leadingAnchor),
            etiquetaFecha.trailingAnchor.constraint(equalTo: etiquetaTitulo.trailingAnchor),
            etiquetaFecha.bottomAnchor.constraint(equalTo: contentView.bottomAnchor,
                                                    constant: -4),
        ])
    }

    func configurar(titulo: String, fecha: Date, indiceColor: Int) {
        etiquetaTitulo.text = titulo
        let formateador = DateFormatter()
        formateador.dateStyle = .medium
        formateador.timeStyle = .short
        etiquetaFecha.text = formateador.string(from: fecha)

        let cantidad = Self.coloresPortada.count
        let resto = indiceColor % cantidad
        let indiceSeguro = resto >= 0 ? resto : resto + cantidad
        let color = Self.coloresPortada[indiceSeguro]
        contenedorPortada.backgroundColor = color
        indicadorColor.backgroundColor = color.withAlphaComponent(0.6)
        isAccessibilityElement = true
        accessibilityLabel = "Cuaderno \(titulo), modificado \(etiquetaFecha.text ?? "")"
    }

    // Animación de presión
    override var isHighlighted: Bool {
        didSet {
            UIView.animate(withDuration: 0.2, delay: 0,
                           options: [.allowUserInteraction, .beginFromCurrentState]) {
                self.transform = self.isHighlighted
                    ? CGAffineTransform(scaleX: 0.95, y: 0.95)
                    : .identity
                self.alpha = self.isHighlighted ? 0.85 : 1
            }
        }
    }
}
