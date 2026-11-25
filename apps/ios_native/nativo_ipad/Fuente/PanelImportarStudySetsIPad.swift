import UIKit
import UniformTypeIdentifiers

final class PanelImportarStudySetsIPad: UIViewController, UIDocumentPickerDelegate {
    var alImportar: ((ResumenImportacionStudySetsIPad) -> Void)?
    private let cursoId: UUID?
    private let referencia: ReferenciaFuente?
    private let limiteBytes = 5 * 1024 * 1024

    init(cursoId: UUID?, referencia: ReferenciaFuente? = nil) {
        self.cursoId = cursoId; self.referencia = referencia
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no implementado") }

    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground; title = "Importar Study Set"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close, target: self, action: #selector(cerrar))

        let titulo = UILabel(); titulo.text = "CSV o TSV"; titulo.font = .preferredFont(forTextStyle: .title2)
        let detalle = UILabel(); detalle.numberOfLines = 0; detalle.textColor = .secondaryLabel
        detalle.text = "Columnas: pregunta/respuesta o question/answer. También acepta front/back. Los duplicados del mismo curso no se vuelven a insertar."
        let boton = UIButton(type: .system); boton.configuration = .filled()
        boton.configuration?.title = "Elegir archivo"
        boton.addTarget(self, action: #selector(elegirArchivo), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [titulo, detalle, boton])
        pila.axis = .vertical; pila.spacing = 18; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            pila.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 36)])
    }

    @objc private func elegirArchivo() {
        let csv = UTType(filenameExtension: "csv") ?? .commaSeparatedText
        let tsv = UTType(filenameExtension: "tsv") ?? .tabSeparatedText
        let selector = UIDocumentPickerViewController(forOpeningContentTypes: [csv, tsv, .plainText], asCopy: true)
        selector.delegate = self; selector.allowsMultipleSelection = false
        present(selector, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController,
                        didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else { return }
        do {
            let valores = try url.resourceValues(forKeys: [.fileSizeKey])
            if let bytes = valores.fileSize, bytes > limiteBytes {
                mostrar("El archivo supera 5 MB. Divídalo antes de importarlo para mantener la operación acotada.", titulo: "Archivo demasiado grande")
                return
            }
            let texto = try String(contentsOf: url, encoding: .utf8)
            let separador: Character? = url.pathExtension.lowercased() == "tsv" ? "\t" : nil
            let resumen = try ImportadorStudySetsAcademicoIPad.importar(
                texto, cursoId: cursoId, referencia: referencia, separador: separador)
            alImportar?(resumen)
            mostrar("Se crearon \(resumen.creadas) tarjeta(s). Duplicadas omitidas: \(resumen.duplicadas).",
                    titulo: "Importación terminada")
        } catch {
            mostrar("No se pudo importar el archivo: \(error.localizedDescription)", titulo: "Importación fallida")
        }
    }

    private func mostrar(_ texto: String, titulo: String) {
        let alerta = UIAlertController(title: titulo, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "OK", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() { dismiss(animated: true) }
}
