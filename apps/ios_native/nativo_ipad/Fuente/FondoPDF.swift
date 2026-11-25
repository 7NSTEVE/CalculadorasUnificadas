import UIKit
import PDFKit

/// Fondo PDF basado en PDFView, con apertura del documento fuera del main thread.
/// PDFView sigue haciendo el render/paging nativo, pero un PDF grande ya no
/// bloquea la interacción mientras PDFDocument parsea xref, catálogo y páginas.
final class FondoPDF: UIView {
    private let pdfView = PDFView(frame: .zero)
    private let colaCarga = DispatchQueue(
        label: "cuadernu.pdf.fondo.carga", qos: .userInitiated)
    private let bloqueo = NSLock()
    private var generacionCarga: UInt64 = 0
    private var urlActual: URL?
    private var observadorMemoria: NSObjectProtocol?

    override init(frame: CGRect) {
        super.init(frame: frame)
        setupView()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupView()
    }

    deinit {
        if let observadorMemoria {
            NotificationCenter.default.removeObserver(observadorMemoria)
        }
    }

    private func setupView() {
        backgroundColor = .clear

        pdfView.frame = bounds
        pdfView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        pdfView.backgroundColor = .clear
        pdfView.autoScales = true
        pdfView.displayMode = .singlePageContinuous
        pdfView.displayDirection = .vertical
        pdfView.displaysPageBreaks = false
        addSubview(pdfView)

        observadorMemoria = NotificationCenter.default.addObserver(
            forName: UIApplication.didReceiveMemoryWarningNotification,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            guard let self, self.window == nil else { return }
            self.descargarDocumento()
        }
    }

    /// Conserva la firma histórica, pero la apertura ya es asíncrona.
    func loadPDF(from url: URL) {
        cargarPDF(from: url, completion: nil)
    }

    /// Apertura cancelable por generación: si A tarda y mientras tanto se pide B,
    /// el resultado tardío de A se descarta y nunca reemplaza B en pantalla.
    func cargarPDF(
        from url: URL,
        completion: ((Bool) -> Void)?
    ) {
        let generacion = siguienteGeneracion()
        urlActual = url.standardizedFileURL

        colaCarga.async { [weak self] in
            guard let self else { return }
            let acceso = url.startAccessingSecurityScopedResource()
            let documento = autoreleasepool { PDFDocument(url: url) }
            if acceso { url.stopAccessingSecurityScopedResource() }

            DispatchQueue.main.async { [weak self] in
                guard let self,
                      self.generacionActual() == generacion else { return }
                guard let documento else {
                    completion?(false)
                    return
                }
                self.pdfView.document = documento
                self.pdfView.autoScales = true
                completion?(true)
            }
        }
    }

    /// Invalida cualquier parseo en curso y libera la referencia a PDFDocument.
    /// Se usa al reciclar una vista o ante memory warning si ya está fuera de UI.
    func descargarDocumento() {
        _ = siguienteGeneracion()
        urlActual = nil
        if Thread.isMainThread {
            pdfView.document = nil
        } else {
            DispatchQueue.main.async { [weak self] in self?.pdfView.document = nil }
        }
    }

    /// Permite reintentar la URL actual después de volver del background sin que
    /// el dueño de la vista tenga que conservar otra copia del path.
    func recargarSiNecesario() {
        guard pdfView.document == nil, let urlActual else { return }
        loadPDF(from: urlActual)
    }

    private func siguienteGeneracion() -> UInt64 {
        bloqueo.lock(); defer { bloqueo.unlock() }
        generacionCarga &+= 1
        return generacionCarga
    }

    private func generacionActual() -> UInt64 {
        bloqueo.lock(); defer { bloqueo.unlock() }
        return generacionCarga
    }

    /// Useful if we need to sync scrolling with the Metal canvas.
    var scrollView: UIScrollView? {
        pdfView.subviews.compactMap { $0 as? UIScrollView }.first
    }

    var documento: PDFDocument? { pdfView.document }
}
