import XCTest
import UIKit
import PDFKit
@testable import CuadernUEstudio

final class DocumentosPdfLigeroTests: XCTestCase {
    private func crearPDF(_ tamanos: [CGSize]) throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("pdf-ligero-\(UUID().uuidString).pdf")
        let base = tamanos.first ?? CGSize(width: 612, height: 792)
        let renderer = UIGraphicsPDFRenderer(bounds: CGRect(origin: .zero, size: base))
        try renderer.writePDF(to: url) { contexto in
            for (i, tamano) in tamanos.enumerated() {
                contexto.beginPage(
                    withBounds: CGRect(origin: .zero, size: tamano),
                    pageInfo: [:])
                let texto = "Página \(i + 1)" as NSString
                texto.draw(at: CGPoint(x: 24, y: 24), withAttributes: [
                    .font: UIFont.systemFont(ofSize: 18),
                    .foregroundColor: UIColor.black,
                ])
            }
        }
        return url
    }

    func testCatalogoLigeroRespetaRangoYNoPierdeTotal() throws {
        let url = try crearPDF([
            CGSize(width: 612, height: 792),
            CGSize(width: 792, height: 612),
            CGSize(width: 500, height: 700),
            CGSize(width: 700, height: 500),
        ])
        defer { try? FileManager.default.removeItem(at: url) }

        let catalogo = DocumentosPdf.catalogar(url, rango: 1..<3)

        XCTAssertEqual(catalogo.totalPaginas, 4)
        XCTAssertEqual(catalogo.paginas.map(\.indice), [1, 2])
        XCTAssertTrue(catalogo.paginas.allSatisfy {
            $0.tamano.width > 0 && $0.tamano.height > 0
                && $0.marco.width > 0 && $0.marco.height > 0
        })
        XCTAssertFalse(catalogo.estaVacio)
    }

    func testCatalogoFueraDeRangoEsVacioPeroConservaTotal() throws {
        let url = try crearPDF([CGSize(width: 612, height: 792)])
        defer { try? FileManager.default.removeItem(at: url) }

        let catalogo = DocumentosPdf.catalogar(url, rango: 10..<20)
        XCTAssertEqual(catalogo.totalPaginas, 1)
        XCTAssertTrue(catalogo.paginas.isEmpty)
        XCTAssertTrue(catalogo.estaVacio)
    }

    func testStreamingEntregaPaginasEnOrdenSinRetenerArrayPDFPage() throws {
        let url = try crearPDF(Array(repeating: CGSize(width: 612, height: 792), count: 5))
        defer { try? FileManager.default.removeItem(at: url) }
        var indices: [Int] = []
        var textos: [String] = []

        let cantidad = try DocumentosPdf.enumerarPaginas(url, rango: 1..<4) {
            descriptor, pagina in
            indices.append(descriptor.indice)
            textos.append(pagina.string ?? "")
        }

        XCTAssertEqual(cantidad, 3)
        XCTAssertEqual(indices, [1, 2, 3])
        XCTAssertTrue(textos[0].contains("Página 2"))
        XCTAssertTrue(textos[2].contains("Página 4"))
    }

    func testStreamingDetieneElDocumentoCuandoElConsumidorFalla() throws {
        enum ErrorPrueba: Error { case detener }
        let url = try crearPDF(Array(repeating: CGSize(width: 612, height: 792), count: 6))
        defer { try? FileManager.default.removeItem(at: url) }
        var visitadas: [Int] = []

        XCTAssertThrowsError(try DocumentosPdf.enumerarPaginas(url) { descriptor, _ in
            visitadas.append(descriptor.indice)
            if descriptor.indice == 2 { throw ErrorPrueba.detener }
        }) { error in
            guard case ErrorPrueba.detener = error else {
                return XCTFail("Error inesperado: \(error)")
            }
        }
        XCTAssertEqual(visitadas, [0, 1, 2])
    }

    func testPreviewRealSeRenderizaAsincronamente() throws {
        let url = try crearPDF([CGSize(width: 612, height: 792)])
        defer { try? FileManager.default.removeItem(at: url) }
        let listo = expectation(description: "preview PDF")
        var token: TokenMiniaturaPDFIPad?

        token = DocumentosPdf.solicitarImagen(
            url: url,
            indice: 0,
            tamano: CGSize(width: 240, height: 320),
            escala: 2) { imagen in
                XCTAssertTrue(Thread.isMainThread)
                XCTAssertNotNil(imagen)
                listo.fulfill()
            }
        wait(for: [listo], timeout: 4)
        _ = token
    }

    func testArchivoInexistenteNoInventaCatalogo() {
        let url = URL(fileURLWithPath: "/tmp/no-existe-\(UUID().uuidString).pdf")
        let catalogo = DocumentosPdf.catalogar(url)
        XCTAssertEqual(catalogo.totalPaginas, 0)
        XCTAssertTrue(catalogo.paginas.isEmpty)
    }
}
