import UIKit
import UniformTypeIdentifiers

final class ShareViewController: UIViewController {
    private let destino = UISegmentedControl(items: ["Inbox", "Curso sugerido"])
    private let curso = UITextField()
    private let estado = UILabel()
    private let guardar = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .systemBackground
        destino.selectedSegmentIndex = 0
        curso.placeholder = "Curso opcional"
        curso.borderStyle = .roundedRect
        estado.text = "El contenido se copiará al contenedor local de Tinta."
        estado.numberOfLines = 0; estado.textColor = .secondaryLabel
        guardar.configuration = .filled(); guardar.configuration?.title = "Guardar en Tinta"
        guardar.addTarget(self, action: #selector(capturar), for: .touchUpInside)
        let cancelar = UIButton(type: .system); cancelar.setTitle("Cancelar", for: .normal)
        cancelar.addTarget(self, action: #selector(cancelarAccion), for: .touchUpInside)
        let pila = UIStackView(arrangedSubviews: [destino, curso, estado, guardar, cancelar])
        pila.axis = .vertical; pila.spacing = 14; pila.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(pila)
        NSLayoutConstraint.activate([
            pila.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 22),
            pila.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -22),
            pila.centerYAnchor.constraint(equalTo: view.centerYAnchor),
        ])
    }

    @objc private func capturar() {
        guard let elementos = extensionContext?.inputItems as? [NSExtensionItem] else {
            terminar(error: true); return
        }
        guardar.isEnabled = false; estado.text = "Copiando…"
        let grupo = DispatchGroup(); let bloqueo = NSLock()
        let destinoElegido = destino.selectedSegmentIndex == 0 ? "Inbox" : "Curso"
        let cursoElegido = curso.text?.trimmingCharacters(in: .whitespacesAndNewlines)
        let cursoSugerido = cursoElegido?.isEmpty == false ? cursoElegido : nil
        var guardadas = 0
        for proveedor in elementos.flatMap({ $0.attachments ?? [] }) {
            if let tipoArchivo = tipoArchivoRegistrado(en: proveedor) {
                grupo.enter()
                proveedor.loadFileRepresentation(forTypeIdentifier: tipoArchivo.identifier) {
                    [weak self] url, _ in
                    defer { grupo.leave() }
                    guard let self, let url, let datos = try? Data(contentsOf: url) else { return }
                    var entrada = self.entradaBase(tipo: self.tipo(de: url),
                                                    destino: destinoElegido,
                                                    curso: cursoSugerido)
                    entrada.nombreOriginal = url.lastPathComponent
                    entrada.tipoUTI = tipoArchivo.identifier
                    if (try? BuzonCompartidoSistemaIPad.publicar(
                        entrada, datosArchivo: datos)) != nil {
                        bloqueo.lock(); guardadas += 1; bloqueo.unlock()
                    }
                }
            } else if proveedor.hasItemConformingToTypeIdentifier(
                UTType.fileURL.identifier) {
                grupo.enter()
                proveedor.loadItem(forTypeIdentifier: UTType.fileURL.identifier) {
                    [weak self] objeto, _ in
                    defer { grupo.leave() }
                    guard let self, let url = objeto as? URL,
                          let datos = try? Data(contentsOf: url) else { return }
                    var entrada = self.entradaBase(tipo: self.tipo(de: url),
                                                    destino: destinoElegido,
                                                    curso: cursoSugerido)
                    entrada.nombreOriginal = url.lastPathComponent
                    entrada.tipoUTI = UTType(filenameExtension: url.pathExtension)?.identifier
                    if (try? BuzonCompartidoSistemaIPad.publicar(
                        entrada, datosArchivo: datos)) != nil {
                        bloqueo.lock(); guardadas += 1; bloqueo.unlock()
                    }
                }
            } else if proveedor.hasItemConformingToTypeIdentifier(UTType.url.identifier) {
                grupo.enter()
                proveedor.loadItem(forTypeIdentifier: UTType.url.identifier) {
                    [weak self] objeto, _ in
                    defer { grupo.leave() }
                    guard let self, let url = objeto as? URL else { return }
                    var entrada = self.entradaBase(tipo: .enlace,
                                                    destino: destinoElegido,
                                                    curso: cursoSugerido)
                    entrada.texto = url.absoluteString
                    if (try? BuzonCompartidoSistemaIPad.publicar(entrada)) != nil {
                        bloqueo.lock(); guardadas += 1; bloqueo.unlock()
                    }
                }
            } else if proveedor.hasItemConformingToTypeIdentifier(UTType.text.identifier) {
                grupo.enter()
                proveedor.loadItem(forTypeIdentifier: UTType.text.identifier) {
                    [weak self] objeto, _ in
                    defer { grupo.leave() }
                    guard let self, let texto = objeto as? String else { return }
                    var entrada = self.entradaBase(tipo: .texto,
                                                    destino: destinoElegido,
                                                    curso: cursoSugerido)
                    entrada.texto = texto
                    if (try? BuzonCompartidoSistemaIPad.publicar(entrada)) != nil {
                        bloqueo.lock(); guardadas += 1; bloqueo.unlock()
                    }
                }
            }
        }
        grupo.notify(queue: .main) { [weak self] in
            self?.estado.text = "Guardados \(guardadas) elemento(s)."
            self?.terminar(error: guardadas == 0)
        }
    }

    /// Escoge el identificador real que ofrece el proveedor. Consultar
    /// `fileURL` y luego pedir `data` no es válido para todos los hosts de la
    /// hoja Compartir; algunas apps solo publican su UTI concreto (PDF, JPEG,
    /// audio, etc.).
    private func tipoArchivoRegistrado(en proveedor: NSItemProvider) -> UTType? {
        proveedor.registeredTypeIdentifiers.lazy
            .compactMap(UTType.init)
            .first { tipo in
                tipo.conforms(to: .content)
                    && !tipo.conforms(to: .url)
                    && !tipo.conforms(to: .text)
            }
    }

    private func entradaBase(tipo: TipoEntradaSistemaIPad, destino: String,
                             curso: String?) -> EntradaSistemaIPad {
        EntradaSistemaIPad(tipo: tipo,
                           destino: destino, cursoSugerido: curso)
    }
    private func tipo(de url: URL) -> TipoEntradaSistemaIPad {
        let ext = url.pathExtension.lowercased()
        if ["png","jpg","jpeg","heic","tif","tiff","webp"].contains(ext) { return .imagen }
        if ["m4a","mp3","wav","caf"].contains(ext) { return .audio }
        return .archivo
    }
    private func terminar(error: Bool) {
        extensionContext?.completeRequest(returningItems: nil)
    }
    @objc private func cancelarAccion() {
        extensionContext?.cancelRequest(withError: NSError(domain: "CuadernUCompartir",
                                                            code: NSUserCancelledError))
    }
}
