import Foundation
import UIKit
import UniformTypeIdentifiers

struct ResultadoPreparacionOneNote: Sendable {
    enum Estado: Sendable {
        case listoParaDecodificar
        case requiereExtractorPaquete
        case yaRegistrado(EstadoImportacionOneNote)
        case rechazado(String)
    }

    var nombre: String
    var formato: FormatoImportacionOneNote
    var estado: Estado
    var sha256: String?
    var archivoDurable: URL?
    var inspeccion: InspeccionOneStore?
}

/// Bandeja durable de fuentes. El archivo seleccionado se copia primero a staging y luego se
/// publica aquí por hash; de esta forma un cierre de la app no obliga a pedir el archivo otra vez.
actor AlmacenFuentesOneNote {
    static let compartido = AlmacenFuentesOneNote()

    private let raiz: URL
    private let fm: FileManager

    init(raiz: URL? = nil, fileManager: FileManager = .default) {
        self.fm = fileManager
        if let raiz {
            self.raiz = raiz
        } else {
            let base = (try? fileManager.url(
                for: .applicationSupportDirectory,
                in: .userDomainMask,
                appropriateFor: nil,
                create: true
            )) ?? fileManager.temporaryDirectory
            self.raiz = base
                .appendingPathComponent("CuadernU", isDirectory: true)
                .appendingPathComponent("Importaciones", isDirectory: true)
                .appendingPathComponent("OneNote", isDirectory: true)
                .appendingPathComponent("Fuentes", isDirectory: true)
        }
    }

    func publicar(_ fuente: FuenteOneNotePreparada) throws -> URL {
        let sha = fuente.huella.sha256.lowercased()
        let carpeta = raiz.appendingPathComponent(sha, isDirectory: true)
        try fm.createDirectory(at: carpeta, withIntermediateDirectories: true)

        let ext = fuente.formato.rawValue
        let destino = carpeta.appendingPathComponent("fuente.\(ext)", isDirectory: false)
        if fm.fileExists(atPath: destino.path) { return destino }

        let temporal = carpeta.appendingPathComponent(".fuente-\(UUID().uuidString).tmp")
        do {
            try fm.copyItem(at: fuente.archivoPreparado, to: temporal)
            // La fuente se vuelve visible con un único rename local.
            try fm.moveItem(at: temporal, to: destino)
            return destino
        } catch {
            try? fm.removeItem(at: temporal)
            throw error
        }
    }

    func urlFuente(sha256: String, formato: FormatoImportacionOneNote) -> URL? {
        let url = raiz
            .appendingPathComponent(sha256.lowercased(), isDirectory: true)
            .appendingPathComponent("fuente.\(formato.rawValue)", isDirectory: false)
        return fm.fileExists(atPath: url.path) ? url : nil
    }
}

/// Pipeline de preparación separado del MainActor. No publica un cuaderno CuadernU hasta que
/// exista un convertidor semántico para el formato correspondiente.
actor MotorPreparacionImportacionOneNote {
    static let compartido = MotorPreparacionImportacionOneNote()

    func procesar(urls: [URL]) async -> [ResultadoPreparacionOneNote] {
        var resultados: [ResultadoPreparacionOneNote] = []
        resultados.reserveCapacity(urls.count)

        for url in urls {
            do {
                let preparada = try await AlmacenTemporalImportacionOneNote.compartido
                    .preparar(urlOriginal: url)
                let resultado = await procesarPreparada(preparada)
                await AlmacenTemporalImportacionOneNote.compartido.limpiar(preparada)
                resultados.append(resultado)
            } catch {
                resultados.append(ResultadoPreparacionOneNote(
                    nombre: url.lastPathComponent,
                    formato: FormatoImportacionOneNote(url: url),
                    estado: .rechazado(error.localizedDescription),
                    sha256: nil,
                    archivoDurable: nil,
                    inspeccion: nil
                ))
            }
        }
        return resultados
    }

    private func procesarPreparada(_ preparada: FuenteOneNotePreparada) async -> ResultadoPreparacionOneNote {
        do {
            if let existente = await RepositorioRegistroImportacionesOneNote.compartido
                .registro(sha256: preparada.huella.sha256) {
                let durable = await AlmacenFuentesOneNote.compartido.urlFuente(
                    sha256: preparada.huella.sha256,
                    formato: preparada.formato
                )
                return ResultadoPreparacionOneNote(
                    nombre: preparada.huella.nombreOriginal,
                    formato: preparada.formato,
                    estado: .yaRegistrado(existente.estado),
                    sha256: preparada.huella.sha256,
                    archivoDurable: durable,
                    inspeccion: nil
                )
            }

            var inspeccion: InspeccionOneStore?
            let estadoRegistro: EstadoImportacionOneNote
            let estadoResultado: ResultadoPreparacionOneNote.Estado
            var advertencias: [AdvertenciaImportacionOneNote] = []

            switch preparada.formato {
            case .one, .onetoc2:
                inspeccion = try InspectorOneStoreIPad.inspeccionar(url: preparada.archivoPreparado)
                estadoRegistro = .inspeccionada
                estadoResultado = .listoParaDecodificar
            case .onepkg:
                estadoRegistro = .requiereCompatibilidad
                estadoResultado = .requiereExtractorPaquete
                advertencias.append(AdvertenciaImportacionOneNote(
                    codigo: "ONENOTE_ONEPKG_EXTRACTOR_PENDING",
                    mensaje: "El paquete se preservó íntegro; el extractor semántico .onepkg todavía no debe marcarse como conversión completa.",
                    severidad: .advertencia,
                    rutaLogica: nil
                ))
            case .desconocido:
                throw ErrorPreparacionOneNote.formatoNoSoportado
            }

            let durable = try await AlmacenFuentesOneNote.compartido.publicar(preparada)
            let registro = RegistroImportacionOneNote(
                huella: preparada.huella,
                formato: preparada.formato,
                estado: estadoRegistro,
                advertencias: advertencias
            )
            _ = try await RepositorioRegistroImportacionesOneNote.compartido.guardar(registro)

            return ResultadoPreparacionOneNote(
                nombre: preparada.huella.nombreOriginal,
                formato: preparada.formato,
                estado: estadoResultado,
                sha256: preparada.huella.sha256,
                archivoDurable: durable,
                inspeccion: inspeccion
            )
        } catch {
            return ResultadoPreparacionOneNote(
                nombre: preparada.huella.nombreOriginal,
                formato: preparada.formato,
                estado: .rechazado(error.localizedDescription),
                sha256: preparada.huella.sha256,
                archivoDurable: nil,
                inspeccion: nil
            )
        }
    }
}

@MainActor
final class CoordinadorImportacionOneNoteIPad: NSObject, UIDocumentPickerDelegate {
    typealias Finalizacion = ([ResultadoPreparacionOneNote]) -> Void

    private weak var presentador: UIViewController?
    private var alFinalizar: Finalizacion?
    private weak var alertaProgreso: UIAlertController?

    static var tiposDocumento: [UTType] {
        var tipos: [UTType] = []
        for ext in ["one", "onepkg", "onetoc2"] {
            if let tipo = UTType(filenameExtension: ext), !tipos.contains(tipo) {
                tipos.append(tipo)
            }
        }
        if tipos.isEmpty { tipos = [.data] }
        return tipos
    }

    func presentarSelector(
        desde presentador: UIViewController,
        alFinalizar: Finalizacion? = nil
    ) {
        self.presentador = presentador
        self.alFinalizar = alFinalizar

        let selector = UIDocumentPickerViewController(
            forOpeningContentTypes: Self.tiposDocumento,
            asCopy: true
        )
        selector.delegate = self
        selector.allowsMultipleSelection = true
        selector.modalPresentationStyle = .formSheet
        presentador.present(selector, animated: true)
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
        alFinalizar = nil
    }

    func documentPicker(
        _ controller: UIDocumentPickerViewController,
        didPickDocumentsAt urls: [URL]
    ) {
        ejecutar(urls: urls)
    }

    /// Permite reutilizar exactamente el mismo pipeline para "Abrir con CuadernU".
    func importarURLsExternas(
        _ urls: [URL],
        desde presentador: UIViewController,
        alFinalizar: Finalizacion? = nil
    ) {
        self.presentador = presentador
        self.alFinalizar = alFinalizar
        ejecutar(urls: urls)
    }

    private func ejecutar(urls: [URL]) {
        guard !urls.isEmpty else { return }
        mostrarProgreso(cantidad: urls.count)
        Task { [weak self] in
            let resultados = await MotorPreparacionImportacionOneNote.compartido.procesar(urls: urls)
            guard let self else { return }
            self.cerrarProgresoYMostrar(resultados)
        }
    }

    private func cerrarProgresoYMostrar(_ resultados: [ResultadoPreparacionOneNote]) {
        let finalizar = { [weak self] in
            guard let self else { return }
            self.presentarResumen(resultados)
            self.alFinalizar?(resultados)
            self.alFinalizar = nil
        }
        if let alertaProgreso, alertaProgreso.presentingViewController != nil {
            alertaProgreso.dismiss(animated: true, completion: finalizar)
        } else {
            finalizar()
        }
    }

    private func mostrarProgreso(cantidad: Int) {
        let alerta = UIAlertController(
            title: "Preparando OneNote",
            message: cantidad == 1
                ? "Validando y preservando el archivo sin modificar el original…"
                : "Validando y preservando \(cantidad) archivos sin modificar los originales…",
            preferredStyle: .alert
        )
        let indicador = UIActivityIndicatorView(style: .medium)
        indicador.translatesAutoresizingMaskIntoConstraints = false
        indicador.startAnimating()
        alerta.view.addSubview(indicador)
        NSLayoutConstraint.activate([
            indicador.centerXAnchor.constraint(equalTo: alerta.view.centerXAnchor),
            indicador.bottomAnchor.constraint(equalTo: alerta.view.bottomAnchor, constant: -18)
        ])
        alertaProgreso = alerta
        presentador?.present(alerta, animated: true)
    }

    private func presentarResumen(_ resultados: [ResultadoPreparacionOneNote]) {
        guard let presentador else { return }
        let lineas = resultados.map { resultado -> String in
            switch resultado.estado {
            case .listoParaDecodificar:
                let tipo = resultado.inspeccion?.tipo == .tablaContenidosOneToc2 ? "índice" : "sección"
                return "✓ \(resultado.nombre): MS-ONESTORE válido (\(tipo)); listo para decodificación semántica."
            case .requiereExtractorPaquete:
                return "◷ \(resultado.nombre): paquete preservado; extractor .onepkg pendiente."
            case .yaRegistrado(let estado):
                return "↺ \(resultado.nombre): ya estaba registrado (\(estado.rawValue))."
            case .rechazado(let motivo):
                return "✕ \(resultado.nombre): \(motivo)"
            }
        }

        let alerta = UIAlertController(
            title: "Importación OneNote",
            message: lineas.joined(separator: "\n\n"),
            preferredStyle: .alert
        )
        alerta.addAction(UIAlertAction(title: "Aceptar", style: .default))
        presentador.present(alerta, animated: true)
    }
}
