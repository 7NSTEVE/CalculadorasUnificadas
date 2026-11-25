import UIKit

@MainActor
final class PanelCorpusComparativaGoodnotesIPad: UIViewController {
    private let tituloEstado = UILabel()
    private let detalle = UILabel()
    private let progreso = UIProgressView(progressViewStyle: .default)
    private let generar = UIButton(type: .system)
    private let compartir = UIButton(type: .system)
    private let validar = UIButton(type: .system)
    private let cancelar = UIButton(type: .system)

    private let bloqueoCancelacion = NSLock()
    private var cancelacionSolicitada = false
    private var generando = false

    private var carpeta: URL {
        let docs = FileManager.default.urls(
            for: .documentDirectory, in: .userDomainMask)[0]
        return docs.appendingPathComponent(
            "QA/ComparativaGoodnotes/Corpus", isDirectory: true)
    }

    private var pdfCanonico: URL {
        carpeta.appendingPathComponent(
            "\(GeneradorCorpusGoodnotesIPad.corpusCanonicoId).pdf")
    }

    private var manifiestoCanonico: URL {
        carpeta.appendingPathComponent(
            "\(GeneradorCorpusGoodnotesIPad.corpusCanonicoId).json")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Corpus 500 páginas"
        view.backgroundColor = .systemGroupedBackground
        view.accessibilityIdentifier = "qa.goodnotes.corpus.panel"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .close,
            target: self,
            action: #selector(cerrar))

        tituloEstado.font = .preferredFont(forTextStyle: .title2)
        tituloEstado.adjustsFontForContentSizeCategory = true
        tituloEstado.textAlignment = .center
        tituloEstado.numberOfLines = 0

        detalle.font = .preferredFont(forTextStyle: .body)
        detalle.adjustsFontForContentSizeCategory = true
        detalle.textColor = .secondaryLabel
        detalle.textAlignment = .center
        detalle.numberOfLines = 0

        progreso.progress = 0
        progreso.isHidden = true
        progreso.accessibilityIdentifier = "qa.goodnotes.corpus.progreso"

        configurarBoton(
            generar,
            titulo: "Generar corpus canónico de 500 páginas",
            simbolo: "doc.badge.gearshape",
            accion: #selector(generarCorpus))
        configurarBoton(
            compartir,
            titulo: "Compartir PDF + manifiesto",
            simbolo: "square.and.arrow.up",
            accion: #selector(compartirCorpus))
        configurarBoton(
            validar,
            titulo: "Validar SHA-256 y páginas",
            simbolo: "checkmark.shield",
            accion: #selector(validarCorpus))
        configurarBoton(
            cancelar,
            titulo: "Cancelar generación",
            simbolo: "xmark.circle",
            accion: #selector(cancelarGeneracion))
        cancelar.configuration?.baseForegroundColor = .systemRed
        cancelar.isHidden = true

        let pila = UIStackView(arrangedSubviews: [
            tituloEstado, detalle, progreso,
            generar, compartir, validar, cancelar,
        ])
        pila.axis = .vertical
        pila.spacing = 14
        pila.alignment = .fill
        pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
            pila.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor,
                                           constant: 28),
            pila.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor,
                                            constant: -28),
            generar.heightAnchor.constraint(greaterThanOrEqualToConstant: 48),
            compartir.heightAnchor.constraint(greaterThanOrEqualToConstant: 48),
            validar.heightAnchor.constraint(greaterThanOrEqualToConstant: 48),
            cancelar.heightAnchor.constraint(greaterThanOrEqualToConstant: 48),
        ])
        refrescarEstado()
    }

    private func configurarBoton(
        _ boton: UIButton,
        titulo: String,
        simbolo: String,
        accion: Selector
    ) {
        var config = UIButton.Configuration.filled()
        config.title = titulo
        config.image = UIImage(systemName: simbolo)
        config.imagePadding = 8
        config.cornerStyle = .large
        boton.configuration = config
        boton.addTarget(self, action: accion, for: .touchUpInside)
    }

    private func refrescarEstado() {
        guard !generando else { return }
        do {
            let metadata = try GeneradorCorpusGoodnotesIPad.validar(
                pdf: pdfCanonico,
                manifiesto: manifiestoCanonico)
            tituloEstado.text = "Corpus canónico válido"
            detalle.text = "\(metadata.paginas) páginas · \(formatoBytes(metadata.bytesPDF))\nSHA-256: \(metadata.sha256PDF)\n\nUse exactamente estos mismos dos archivos para las corridas de CuadernU y Goodnotes."
            compartir.isEnabled = true
            validar.isEnabled = true
            generar.configuration?.title = "Regenerar corpus de 500 páginas"
        } catch {
            tituloEstado.text = "Corpus canónico no preparado"
            detalle.text = "Genere una vez el PDF de 500 páginas y comparta ese mismo binario con ambas apps. La comparación almacenará su SHA-256."
            compartir.isEnabled = false
            validar.isEnabled = FileManager.default.fileExists(atPath: pdfCanonico.path)
                || FileManager.default.fileExists(atPath: manifiestoCanonico.path)
            generar.configuration?.title = "Generar corpus canónico de 500 páginas"
        }
    }

    @objc private func generarCorpus() {
        guard !generando else { return }
        generando = true
        bloqueoCancelacion.lock()
        cancelacionSolicitada = false
        bloqueoCancelacion.unlock()

        progreso.progress = 0
        progreso.isHidden = false
        cancelar.isHidden = false
        generar.isEnabled = false
        compartir.isEnabled = false
        validar.isEnabled = false
        navigationItem.leftBarButtonItem?.isEnabled = false
        tituloEstado.text = "Generando corpus…"
        detalle.text = "Se escribe página por página; no se construyen 500 imágenes en memoria."

        let carpeta = self.carpeta
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            let resultado = Result {
                try GeneradorCorpusGoodnotesIPad.generar(
                    en: carpeta,
                    cancelado: { [weak self] in
                        guard let self else { return true }
                        self.bloqueoCancelacion.lock()
                        defer { self.bloqueoCancelacion.unlock() }
                        return self.cancelacionSolicitada
                    },
                    progreso: { [weak self] valor in
                        DispatchQueue.main.async {
                            self?.progreso.setProgress(Float(valor), animated: true)
                            self?.detalle.text = "Página \(Int(valor * 500)) de 500 · escritura streaming"
                        }
                    })
            }
            DispatchQueue.main.async { [weak self] in
                guard let self else { return }
                self.generando = false
                self.progreso.isHidden = true
                self.cancelar.isHidden = true
                self.generar.isEnabled = true
                self.navigationItem.leftBarButtonItem?.isEnabled = true
                switch resultado {
                case .success(let corpus):
                    self.tituloEstado.text = "Corpus generado y validado"
                    self.detalle.text = "SHA-256: \(corpus.metadata.sha256PDF)"
                    self.refrescarEstado()
                case .failure(let error):
                    self.tituloEstado.text = error is ErrorCorpusGoodnotesIPad
                        ? "Generación detenida" : "No se generó el corpus"
                    self.detalle.text = error.localizedDescription
                    self.refrescarEstado()
                }
            }
        }
    }

    @objc private func cancelarGeneracion() {
        bloqueoCancelacion.lock()
        cancelacionSolicitada = true
        bloqueoCancelacion.unlock()
        cancelar.isEnabled = false
        detalle.text = "Cancelando al terminar la página actual…"
    }

    @objc private func validarCorpus() {
        do {
            let metadata = try GeneradorCorpusGoodnotesIPad.validar(
                pdf: pdfCanonico, manifiesto: manifiestoCanonico)
            avisar("Corpus válido: \(metadata.paginas) páginas\nSHA-256: \(metadata.sha256PDF)")
            refrescarEstado()
        } catch {
            avisar("El corpus no es válido: \(error.localizedDescription)")
            refrescarEstado()
        }
    }

    @objc private func compartirCorpus() {
        do {
            _ = try GeneradorCorpusGoodnotesIPad.validar(
                pdf: pdfCanonico, manifiesto: manifiestoCanonico)
            let actividad = UIActivityViewController(
                activityItems: [pdfCanonico, manifiestoCanonico],
                applicationActivities: nil)
            actividad.popoverPresentationController?.sourceView = view
            present(actividad, animated: true)
        } catch {
            avisar("No se comparte un corpus sin validar: \(error.localizedDescription)")
        }
    }

    private func formatoBytes(_ bytes: Int64) -> String {
        ByteCountFormatter.string(
            fromByteCount: bytes,
            countStyle: .file)
    }

    private func avisar(_ texto: String) {
        let alerta = UIAlertController(
            title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        present(alerta, animated: true)
    }

    @objc private func cerrar() {
        guard !generando else { return }
        dismiss(animated: true)
    }
}
