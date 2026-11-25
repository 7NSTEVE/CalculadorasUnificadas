import PDFKit
import UIKit
import XCTest

final class ImpresionDocumentoIPadTests: XCTestCase {
    private var temporales: [URL] = []

    override func tearDown() {
        for url in temporales { try? FileManager.default.removeItem(at: url) }
        temporales.removeAll()
        super.tearDown()
    }

    func testPreflightAceptaPDFMultipaginaYDetectaOrientacion() throws {
        let url = try crearPDF(tamanos: [
            CGSize(width: 842, height: 595),
            CGSize(width: 595, height: 842),
            CGSize(width: 595, height: 842),
        ])

        let reporte = try ValidadorImpresionPDFIPad.preflight(url)

        XCTAssertEqual(reporte.paginas, 3)
        XCTAssertGreaterThan(reporte.bytes, 0)
        XCTAssertEqual(reporte.primeraPagina.width, 842, accuracy: 1)
        XCTAssertEqual(reporte.primeraPagina.height, 595, accuracy: 1)
        XCTAssertTrue(reporte.apaisado)
    }

    func testPreflightDetectaRetrato() throws {
        let url = try crearPDF(tamanos: [CGSize(width: 595, height: 842)])
        let reporte = try ValidadorImpresionPDFIPad.preflight(url)
        XCTAssertEqual(reporte.paginas, 1)
        XCTAssertFalse(reporte.apaisado)
    }

    func testPreflightRechazaArchivoAusente() {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString)
            .appendingPathExtension("pdf")
        XCTAssertThrowsError(try ValidadorImpresionPDFIPad.preflight(url)) { error in
            XCTAssertEqual(error as? ErrorImpresionDocumentoIPad, .archivoNoExiste)
        }
    }

    func testPreflightRechazaArchivoVacio() throws {
        let url = temporal("vacio.pdf")
        try Data().write(to: url)
        XCTAssertThrowsError(try ValidadorImpresionPDFIPad.preflight(url)) { error in
            XCTAssertEqual(error as? ErrorImpresionDocumentoIPad, .archivoVacio)
        }
    }

    func testPreflightRechazaBytesQueNoSonPDF() throws {
        let url = temporal("corrupto.pdf")
        try Data("esto no es un pdf".utf8).write(to: url)
        XCTAssertThrowsError(try ValidadorImpresionPDFIPad.preflight(url)) { error in
            XCTAssertEqual(error as? ErrorImpresionDocumentoIPad, .pdfInvalido)
        }
    }

    private func crearPDF(tamanos: [CGSize]) throws -> URL {
        let url = temporal("documento.pdf")
        guard let primero = tamanos.first else {
            XCTFail("El fixture necesita una página")
            return url
        }
        let renderer = UIGraphicsPDFRenderer(
            bounds: CGRect(origin: .zero, size: primero))
        try renderer.writePDF(to: url) { contexto in
            for tamano in tamanos {
                contexto.beginPage(withBounds: CGRect(origin: .zero, size: tamano),
                                    pageInfo: [:])
                ("Prueba" as NSString).draw(
                    at: CGPoint(x: 24, y: 24),
                    withAttributes: [.font: UIFont.systemFont(ofSize: 14)])
            }
        }
        return url
    }

    private func temporal(_ nombre: String) -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("CuadernU-Impresion-\(UUID().uuidString)",
                                    isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        temporales.append(url)
        return url.appendingPathComponent(nombre)
    }
}
