import PDFKit
import UIKit

/// Vista de solo lectura para volver al material original del que salió un
/// problema. No conoce ni muestra `SolucionVinculadaIPad`: el objetivo es
/// comprobar el enunciado y su contexto sin romper una práctica limpia.
final class ControladorFuenteProblemaIPad: UIViewController, UIScrollViewDelegate {
    private let referencia: ReferenciaFuente
    private let imagenScroll = UIScrollView()
    private let imagen = UIImageView()

    init(referencia: ReferenciaFuente) {
        self.referencia = referencia
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = tituloFuente
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))

        guard referencia.tipo != .solucion else {
            mostrarEstado("La solución vinculada permanece oculta durante la práctica. Termine el intento antes de consultarla desde el Problem Bank.")
            return
        }

        guard let hash = referencia.hashArchivo,
              let url = RepositorioAdjuntos.compartido.url(hash: hash) else {
            mostrarEstado("La referencia existe, pero el archivo original no está disponible en el almacén local.")
            return
        }

        if url.pathExtension.lowercased() == "pdf" {
            mostrarPDF(url)
        } else if let uiImage = UIImage(contentsOfFile: url.path) {
            mostrarImagen(uiImage)
        } else {
            mostrarEstado("CuadernU conserva la referencia a este archivo, pero este formato todavía no tiene previsualizador integrado.")
        }
    }

    private var tituloFuente: String {
        var partes = [referencia.titulo.isEmpty ? "Fuente original" : referencia.titulo]
        if let pagina = referencia.pagina { partes.append("p. \(pagina + 1)") }
        return partes.joined(separator: " · ")
    }

    private func mostrarPDF(_ url: URL) {
        guard let documento = PDFDocument(url: url) else {
            mostrarEstado("El archivo PDF no pudo abrirse sin modificar el original.")
            return
        }
        let pdf = PDFView(frame: .zero)
        pdf.translatesAutoresizingMaskIntoConstraints = false
        pdf.document = documento
        pdf.autoScales = true
        pdf.displayMode = .singlePageContinuous
        pdf.displayDirection = .vertical
        view.addSubview(pdf)
        NSLayoutConstraint.activate([
            pdf.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            pdf.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            pdf.topAnchor.constraint(equalTo: view.topAnchor),
            pdf.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])
        if let indice = referencia.pagina,
           indice >= 0, indice < documento.pageCount,
           let pagina = documento.page(at: indice) {
            pdf.go(to: pagina)
        }
    }

    private func mostrarImagen(_ uiImage: UIImage) {
        imagenScroll.delegate = self
        imagenScroll.minimumZoomScale = 1
        imagenScroll.maximumZoomScale = 6
        imagenScroll.translatesAutoresizingMaskIntoConstraints = false
        imagen.contentMode = .scaleAspectFit
        imagen.image = uiImage
        imagen.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(imagenScroll)
        imagenScroll.addSubview(imagen)
        NSLayoutConstraint.activate([
            imagenScroll.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            imagenScroll.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            imagenScroll.topAnchor.constraint(equalTo: view.topAnchor),
            imagenScroll.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            imagen.leadingAnchor.constraint(equalTo: imagenScroll.contentLayoutGuide.leadingAnchor),
            imagen.trailingAnchor.constraint(equalTo: imagenScroll.contentLayoutGuide.trailingAnchor),
            imagen.topAnchor.constraint(equalTo: imagenScroll.contentLayoutGuide.topAnchor),
            imagen.bottomAnchor.constraint(equalTo: imagenScroll.contentLayoutGuide.bottomAnchor),
            imagen.widthAnchor.constraint(equalTo: imagenScroll.frameLayoutGuide.widthAnchor),
            imagen.heightAnchor.constraint(equalTo: imagenScroll.frameLayoutGuide.heightAnchor),
        ])
    }

    func viewForZooming(in scrollView: UIScrollView) -> UIView? { imagen }

    private func mostrarEstado(_ texto: String) {
        let etiqueta = UILabel()
        etiqueta.text = texto
        etiqueta.textColor = .secondaryLabel
        etiqueta.font = .preferredFont(forTextStyle: .body)
        etiqueta.textAlignment = .center
        etiqueta.numberOfLines = 0
        etiqueta.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(etiqueta)
        NSLayoutConstraint.activate([
            etiqueta.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                               constant: 28),
            etiqueta.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                                constant: -28),
            etiqueta.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
