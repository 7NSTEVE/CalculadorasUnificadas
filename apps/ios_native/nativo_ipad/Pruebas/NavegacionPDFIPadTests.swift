import XCTest
import PDFKit
import UIKit
@testable import CuadernUEstudio

final class NavegacionPDFIPadTests: XCTestCase {
    private func basePDF(paginas: Int = 3) throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("nav-pdf-\(UUID().uuidString).pdf")
        let renderer = UIGraphicsPDFRenderer(
            bounds: CGRect(x: 0, y: 0, width: 612, height: 792))
        try renderer.writePDF(to: url) { contexto in
            for indice in 0..<paginas {
                contexto.beginPage()
                ("Página \(indice + 1)" as NSString).draw(
                    at: CGPoint(x: 40, y: 40),
                    withAttributes: [.font: UIFont.systemFont(ofSize: 20)])
            }
        }
        return url
    }

    private func pdfConNavegacion() throws -> URL {
        let url = try basePDF()
        let documento = try XCTUnwrap(PDFDocument(url: url))
        let p0 = try XCTUnwrap(documento.page(at: 0))
        let p1 = try XCTUnwrap(documento.page(at: 1))
        let p2 = try XCTUnwrap(documento.page(at: 2))

        let raiz = PDFOutline()
        let capitulo = PDFOutline()
        capitulo.label = "Capítulo 1"
        capitulo.destination = PDFDestination(page: p0, at: CGPoint(x: 0, y: 700))
        let seccion = PDFOutline()
        seccion.label = "Sección profunda"
        seccion.destination = PDFDestination(page: p2, at: CGPoint(x: 10, y: 650))
        capitulo.insertChild(seccion, at: 0)
        raiz.insertChild(capitulo, at: 0)
        documento.outlineRoot = raiz

        let interno = PDFAnnotation(
            bounds: CGRect(x: 100, y: 100, width: 80, height: 30),
            forType: .link,
            withProperties: nil)
        interno.contents = "Ir a página 2"
        interno.destination = PDFDestination(page: p1, at: CGPoint(x: 0, y: 720))
        p0.addAnnotation(interno)

        let externo = PDFAnnotation(
            bounds: CGRect(x: 100, y: 150, width: 100, height: 30),
            forType: .link,
            withProperties: nil)
        externo.contents = "Documentación"
        externo.url = URL(string: "https://example.com/manual")
        p0.addAnnotation(externo)

        let inseguro = PDFAnnotation(
            bounds: CGRect(x: 100, y: 200, width: 100, height: 30),
            forType: .link,
            withProperties: nil)
        inseguro.url = URL(fileURLWithPath: "/tmp/no-abrir")
        p0.addAnnotation(inseguro)

        XCTAssertTrue(documento.write(to: url))
        return url
    }

    func testExtraeOutlineJerarquicoConPaginaYPunto() throws {
        let url = try pdfConNavegacion()
        defer { try? FileManager.default.removeItem(at: url) }

        let resumen = AnalizadorNavegacionPDFIPad.resumen(url)

        XCTAssertEqual(resumen.totalPaginas, 3)
        XCTAssertEqual(resumen.outline.count, 2)
        XCTAssertEqual(resumen.outline[0].titulo, "Capítulo 1")
        XCTAssertEqual(resumen.outline[0].pagina, 0)
        XCTAssertEqual(resumen.outline[0].profundidad, 0)
        XCTAssertEqual(resumen.outline[1].titulo, "Sección profunda")
        XCTAssertEqual(resumen.outline[1].pagina, 2)
        XCTAssertEqual(resumen.outline[1].profundidad, 1)
        XCTAssertFalse(resumen.outlineTruncado)
    }

    func testExtraeEnlaceInternoYHTTPSPeroIgnoraFileURL() throws {
        let url = try pdfConNavegacion()
        defer { try? FileManager.default.removeItem(at: url) }

        let resultado = AnalizadorNavegacionPDFIPad.enlaces(url)

        XCTAssertEqual(resultado.enlaces.count, 2)
        XCTAssertFalse(resultado.truncado)
        let interno = try XCTUnwrap(resultado.enlaces.first {
            if case .pagina = $0.destino { return true }
            return false
        })
        if case .pagina(let pagina, let punto) = interno.destino {
            XCTAssertEqual(pagina, 1)
            XCTAssertEqual(punto?.y ?? .nan, 720, accuracy: 0.01)
        } else {
            XCTFail("Se esperaba destino interno")
        }

        let externo = try XCTUnwrap(resultado.enlaces.first {
            if case .url = $0.destino { return true }
            return false
        })
        if case .url(let destino) = externo.destino {
            XCTAssertEqual(destino.absoluteString, "https://example.com/manual")
        } else {
            XCTFail("Se esperaba URL externa")
        }
    }

    func testEsquemasExternosPermitidosSonConservadores() {
        XCTAssertTrue(AnalizadorNavegacionPDFIPad.esquemaExternoPermitido(
            URL(string: "https://example.com")!))
        XCTAssertTrue(AnalizadorNavegacionPDFIPad.esquemaExternoPermitido(
            URL(string: "mailto:profesor@example.com")!))
        XCTAssertFalse(AnalizadorNavegacionPDFIPad.esquemaExternoPermitido(
            URL(fileURLWithPath: "/tmp/a")))
        XCTAssertFalse(AnalizadorNavegacionPDFIPad.esquemaExternoPermitido(
            URL(string: "javascript:alert(1)")!))
    }

    func testPDFSinOutlineNiEnlacesNoInventaResultados() throws {
        let url = try basePDF(paginas: 1)
        defer { try? FileManager.default.removeItem(at: url) }

        let resumen = AnalizadorNavegacionPDFIPad.resumen(url)
        let enlaces = AnalizadorNavegacionPDFIPad.enlaces(url)
        XCTAssertEqual(resumen.totalPaginas, 1)
        XCTAssertTrue(resumen.outline.isEmpty)
        XCTAssertTrue(enlaces.enlaces.isEmpty)
    }
}
