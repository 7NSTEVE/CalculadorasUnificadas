import UIKit

@MainActor
final class CentroExportacionRapidaToolbarIPad {
    static let compartido = CentroExportacionRapidaToolbarIPad()

    func abrir() {
        guard let contexto = contextoPrincipal() else { return }
        let hoja = UIAlertController(
            title: "Compartir / exportar",
            message: "Estas salidas son explícitas: PNG del contenido visible del documento, texto OCR manuscrito o snapshot nativo. El exportador PDF completo conserva su workflow separado hasta completar su validación física.",
            preferredStyle: .actionSheet)
        hoja.addAction(UIAlertAction(
            title: "PNG del contenido (máx. 4096 px)",
            style: .default) { [weak self] _ in
                self?.exportarPNG(contexto)
            })
        hoja.addAction(UIAlertAction(
            title: "Markdown de escritura reconocida",
            style: .default) { [weak self] _ in
                self?.exportarMarkdown(contexto)
            })
        hoja.addAction(UIAlertAction(
            title: "Snapshot nativo .tinta",
            style: .default) { [weak self] _ in
                self?.exportarTinta(contexto)
            })
        hoja.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        hoja.popoverPresentationController?.sourceView = contexto.presentador.view
        contexto.presentador.present(hoja, animated: true)
    }

    private struct Contexto {
        let navegador: VistaLienzoNavegable
        let presentador: UIViewController
        let titulo: String
    }

    private func exportarPNG(_ contexto: Contexto) {
        let sesion = contexto.navegador.lienzo.sesion
        let seleccion = SeleccionLienzo(
            trazos: Array(0..<sesion.cantidadTrazos),
            textos: Array(0..<sesion.textos.count),
            medios: Array(0..<sesion.medios.count))
        guard !seleccion.vacia else {
            avisar("El documento no contiene elementos exportables.",
                   desde: contexto.presentador)
            return
        }
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let imagen = try ExportadorSeleccionIPad.imagen(
                    seleccion: seleccion,
                    sesion: sesion,
                    escalaSolicitada: 2,
                    limitePixeles: 4_096)
                guard let datos = imagen.pngData() else {
                    throw NSError(
                        domain: "CuadernU.ExportacionRapida",
                        code: 1,
                        userInfo: [NSLocalizedDescriptionKey:
                            "No se pudo codificar la imagen PNG."])
                }
                let url = try Self.temporal(
                    nombre: contexto.titulo,
                    extension: "png",
                    datos: datos)
                DispatchQueue.main.async {
                    self?.compartir([url], desde: contexto.presentador)
                }
            } catch {
                DispatchQueue.main.async {
                    self?.avisar(error.localizedDescription,
                                 desde: contexto.presentador)
                }
            }
        }
    }

    private func exportarMarkdown(_ contexto: Contexto) {
        let documento = contexto.navegador.lienzo.sesion.documentoPersistenteId
        let registros = RepositorioOCRManuscritoIPad.compartido
            .registros(documentoId: documento)
            .filter { !$0.texto.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
            .sorted { ($0.marco.minY, $0.marco.minX) < ($1.marco.minY, $1.marco.minX) }
        guard !registros.isEmpty else {
            avisar(
                "Todavía no hay escritura manuscrita reconocida para exportar. Termine de escribir y permita que el índice Vision complete una pasada.",
                desde: contexto.presentador)
            return
        }
        var lineas = ["# \(contexto.titulo)", "", "_OCR manuscrito derivado; la tinta original no fue modificada._", ""]
        for registro in registros {
            lineas.append(registro.texto)
            lineas.append("")
        }
        let datos = Data(lineas.joined(separator: "\n").utf8)
        do {
            let url = try Self.temporal(
                nombre: contexto.titulo + " - handwriting",
                extension: "md",
                datos: datos)
            compartir([url], desde: contexto.presentador)
        } catch {
            avisar(error.localizedDescription, desde: contexto.presentador)
        }
    }

    private func exportarTinta(_ contexto: Contexto) {
        let sesion = contexto.navegador.lienzo.sesion
        guard let datos = sesion.guardar(), !datos.isEmpty else {
            avisar("No se pudo serializar el snapshot nativo.",
                   desde: contexto.presentador)
            return
        }
        do {
            let url = try Self.temporal(
                nombre: contexto.titulo,
                extension: "tinta",
                datos: datos)
            compartir([url], desde: contexto.presentador)
        } catch {
            avisar(error.localizedDescription, desde: contexto.presentador)
        }
    }

    private func compartir(_ urls: [URL], desde presentador: UIViewController) {
        guard !urls.isEmpty,
              presentador.presentedViewController == nil else { return }
        let compartir = UIActivityViewController(
            activityItems: urls,
            applicationActivities: nil)
        compartir.completionWithItemsHandler = { _, _, _, _ in
            urls.forEach { try? FileManager.default.removeItem(at: $0) }
        }
        compartir.popoverPresentationController?.sourceView = presentador.view
        presentador.present(compartir, animated: true)
    }

    private static func temporal(
        nombre: String,
        extension ext: String,
        datos: Data
    ) throws -> URL {
        let limpio = nombre
            .replacingOccurrences(of: "/", with: "-")
            .replacingOccurrences(of: ":", with: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let base = limpio.isEmpty ? "CuadernU" : String(limpio.prefix(120))
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(
            "\(base)-\(UUID().uuidString.prefix(8)).\(ext)")
        try datos.write(to: url, options: [.atomic, .completeFileProtection])
        return url
    }

    private func contextoPrincipal() -> Contexto? {
        let escenas = UIApplication.shared.connectedScenes.compactMap {
            $0 as? UIWindowScene
        }.filter { $0.activationState == .foregroundActive }
        guard let ventana = escenas.flatMap(\.windows).first(where: \.isKeyWindow)
            ?? escenas.flatMap(\.windows).first(where: { !$0.isHidden }),
              let raizVentana = ventana.rootViewController,
              let raiz = buscarRaiz(en: raizVentana),
              let host = raiz.viewController(for: .secondary)
                as? ControladorPestanasDocumentoIPad,
              let lienzo = host.lienzoVisibleParaNavegacionProfunda()
        else { return nil }
        lienzo.loadViewIfNeeded()
        guard let navegador = primeraVista(
            en: lienzo.view, tipo: VistaLienzoNavegable.self) else { return nil }
        let documento = navegador.lienzo.sesion.documentoPersistenteId
        let titulo = LocalizadorDocumentoIPad.localizar(documentoId: documento)?.titulo
            ?? "CuadernU"
        return .init(
            navegador: navegador,
            presentador: superior(raizVentana),
            titulo: titulo)
    }

    private func buscarRaiz(en controlador: UIViewController)
        -> ControladorRaizIPad? {
        if let valor = controlador as? ControladorRaizIPad { return valor }
        if let presentado = controlador.presentedViewController,
           let valor = buscarRaiz(en: presentado) { return valor }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController,
           let valor = buscarRaiz(en: visible) { return valor }
        for hijo in controlador.children.reversed() {
            if let valor = buscarRaiz(en: hijo) { return valor }
        }
        return nil
    }

    private func primeraVista<T: UIView>(en vista: UIView, tipo: T.Type) -> T? {
        if let valor = vista as? T { return valor }
        for hija in vista.subviews {
            if let valor = primeraVista(en: hija, tipo: tipo) { return valor }
        }
        return nil
    }

    private func superior(_ controlador: UIViewController) -> UIViewController {
        if let presentado = controlador.presentedViewController {
            return superior(presentado)
        }
        if let nav = controlador as? UINavigationController,
           let visible = nav.visibleViewController {
            return superior(visible)
        }
        return controlador
    }

    private func avisar(_ texto: String, desde presentador: UIViewController) {
        let alerta = UIAlertController(
            title: nil, message: texto, preferredStyle: .alert)
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        presentador.present(alerta, animated: true)
    }
}
