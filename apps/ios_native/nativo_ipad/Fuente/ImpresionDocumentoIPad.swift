import Foundation
import PDFKit
import UIKit

struct PreflightImpresionPDFIPad: Equatable {
    var paginas: Int
    var bytes: Int64
    var primeraPagina: CGSize
    var apaisado: Bool
}

enum ErrorImpresionDocumentoIPad: LocalizedError, Equatable {
    case impresionNoDisponible
    case archivoNoExiste
    case archivoVacio
    case pdfInvalido
    case sinPaginas
    case paginaInvalida(Int)
    case sinVentanaActiva
    case noSePudoPresentar

    var errorDescription: String? {
        switch self {
        case .impresionNoDisponible:
            return "La impresión no está disponible en este dispositivo."
        case .archivoNoExiste:
            return "El PDF preparado para impresión ya no existe."
        case .archivoVacio:
            return "El PDF preparado para impresión está vacío."
        case .pdfInvalido:
            return "El archivo generado no es un PDF válido."
        case .sinPaginas:
            return "El PDF no contiene páginas imprimibles."
        case .paginaInvalida(let pagina):
            return "La página \(pagina + 1) tiene un tamaño inválido y no se enviará a impresión."
        case .sinVentanaActiva:
            return "No hay una ventana activa desde la que mostrar las opciones de impresión."
        case .noSePudoPresentar:
            return "iPadOS no pudo presentar las opciones de impresión."
        }
    }
}

enum ValidadorImpresionPDFIPad {
    static func preflight(_ url: URL) throws -> PreflightImpresionPDFIPad {
        guard FileManager.default.fileExists(atPath: url.path) else {
            throw ErrorImpresionDocumentoIPad.archivoNoExiste
        }
        let atributos = try FileManager.default.attributesOfItem(atPath: url.path)
        let bytes = (atributos[.size] as? NSNumber)?.int64Value ?? 0
        guard bytes > 0 else { throw ErrorImpresionDocumentoIPad.archivoVacio }
        guard let documento = PDFDocument(url: url) else {
            throw ErrorImpresionDocumentoIPad.pdfInvalido
        }
        guard documento.pageCount > 0 else {
            throw ErrorImpresionDocumentoIPad.sinPaginas
        }

        var primera = CGSize.zero
        for indice in 0..<documento.pageCount {
            guard let pagina = documento.page(at: indice) else {
                throw ErrorImpresionDocumentoIPad.paginaInvalida(indice)
            }
            let marco = pagina.bounds(for: .mediaBox)
            let valores = [marco.minX, marco.minY, marco.width, marco.height,
                           marco.maxX, marco.maxY]
            guard valores.allSatisfy(\.isFinite),
                  marco.width > 0, marco.height > 0,
                  !marco.isNull, !marco.isInfinite else {
                throw ErrorImpresionDocumentoIPad.paginaInvalida(indice)
            }
            if indice == 0 { primera = marco.size }
        }
        return PreflightImpresionPDFIPad(
            paginas: documento.pageCount,
            bytes: bytes,
            primeraPagina: primera,
            apaisado: primera.width > primera.height)
    }
}

final class CoordinadorImpresionDocumentoIPad {
    struct Dependencias {
        var impresionDisponible: () -> Bool = {
            UIPrintInteractionController.isPrintingAvailable
        }
        var borrarTemporal: (URL) -> Void = { url in
            try? FileManager.default.removeItem(at: url)
        }
    }

    private let dependencias: Dependencias

    init(dependencias: Dependencias = Dependencias()) {
        self.dependencias = dependencias
    }

    func imprimir(
        sesion: Sesion,
        navegador: VistaLienzoNavegable,
        titulo: String,
        desde presentador: UIViewController,
        alFinalizar: ((Result<Bool, Error>) -> Void)? = nil
    ) {
        guard dependencias.impresionDisponible() else {
            alFinalizar?(.failure(ErrorImpresionDocumentoIPad.impresionNoDisponible))
            return
        }

        do {
            let url = try ExportadorCompletoIPad.exportar(
                .pdfCompleto,
                sesion: sesion,
                navegador: navegador,
                titulo: titulo)
            let preflight = try ValidadorImpresionPDFIPad.preflight(url)
            presentar(
                url: url,
                preflight: preflight,
                titulo: titulo,
                desde: presentador,
                alFinalizar: alFinalizar)
        } catch {
            alFinalizar?(.failure(error))
        }
    }

    private func presentar(
        url: URL,
        preflight: PreflightImpresionPDFIPad,
        titulo: String,
        desde presentador: UIViewController,
        alFinalizar: ((Result<Bool, Error>) -> Void)?
    ) {
        guard presentador.viewIfLoaded?.window != nil else {
            dependencias.borrarTemporal(url)
            alFinalizar?(.failure(ErrorImpresionDocumentoIPad.sinVentanaActiva))
            return
        }

        let controlador = UIPrintInteractionController.shared
        let info = UIPrintInfo(dictionary: nil)
        info.jobName = titulo.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            ? "CuadernU" : titulo
        info.outputType = .general
        info.orientation = preflight.apaisado ? .landscape : .portrait
        controlador.printInfo = info
        controlador.printingItem = url
        controlador.showsNumberOfCopies = true

        let ancla = CGRect(
            x: presentador.view.bounds.midX,
            y: max(presentador.view.safeAreaInsets.top + 8, 8),
            width: 1, height: 1)
        let presentado = controlador.present(
            from: ancla,
            in: presentador.view,
            animated: true) { [dependencias] _, completado, error in
                dependencias.borrarTemporal(url)
                if let error {
                    alFinalizar?(.failure(error))
                } else {
                    alFinalizar?(.success(completado))
                }
            }
        guard presentado else {
            dependencias.borrarTemporal(url)
            alFinalizar?(.failure(ErrorImpresionDocumentoIPad.noSePudoPresentar))
            return
        }
    }
}
