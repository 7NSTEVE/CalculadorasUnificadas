import UIKit
import XCTest

final class CachePDFExportacionIPadTests: XCTestCase {
    private var temporales: [URL] = []
    private let cache = CachePDFExportacionIPad.compartido

    override func setUp() {
        super.setUp()
        cache.vaciar()
        cache.reiniciarEstadisticas()
    }

    override func tearDown() {
        cache.vaciar()
        for url in temporales { try? FileManager.default.removeItem(at: url) }
        temporales.removeAll()
        super.tearDown()
    }

    func testMismaPaginaYTamanosCuantizadosReutilizanImagen() throws {
        let url = try crearPDF(paginas: 2, textoExtra: "A")
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 333, height: 477)))
        var stats = cache.estadisticas()
        XCTAssertEqual(stats.missesImagen, 1)
        XCTAssertEqual(stats.hitsImagen, 0)
        XCTAssertEqual(stats.documentosAbiertos, 1)

        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 340, height: 470)))
        stats = cache.estadisticas()
        XCTAssertEqual(stats.missesImagen, 1)
        XCTAssertEqual(stats.hitsImagen, 1)
        XCTAssertEqual(stats.documentosAbiertos, 1)
    }

    func testPaginaDistintaEsMissPeroNoReabreDocumento() throws {
        let url = try crearPDF(paginas: 2, textoExtra: "B")
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 320, height: 480)))
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 1, tamano: CGSize(width: 320, height: 480)))
        let stats = cache.estadisticas()
        XCTAssertEqual(stats.missesImagen, 2)
        XCTAssertEqual(stats.hitsImagen, 0)
        XCTAssertEqual(stats.documentosAbiertos, 1)
    }

    func testPaginaFueraDeRangoNoRenderiza() throws {
        let url = try crearPDF(paginas: 1, textoExtra: "C")
        XCTAssertNil(cache.imagen(
            url: url, pagina: 9, tamano: CGSize(width: 320, height: 480)))
        let stats = cache.estadisticas()
        XCTAssertEqual(stats.missesImagen, 1)
        XCTAssertEqual(stats.documentosAbiertos, 1)
    }

    func testVaciarObligaAReabrirDocumento() throws {
        let url = try crearPDF(paginas: 1, textoExtra: "D")
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 320, height: 480)))
        cache.vaciar()
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 320, height: 480)))
        let stats = cache.estadisticas()
        XCTAssertEqual(stats.missesImagen, 2)
        XCTAssertEqual(stats.documentosAbiertos, 2)
    }

    func testCambioDeArchivoInvalidaFirmaAnterior() throws {
        let url = try crearPDF(paginas: 1, textoExtra: "E")
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 0, tamano: CGSize(width: 320, height: 480)))
        let antes = cache.estadisticas()
        XCTAssertEqual(antes.documentosAbiertos, 1)

        try escribirPDF(url: url, paginas: 3,
                        textoExtra: String(repeating: "contenido nuevo ", count: 120))
        XCTAssertNotNil(cache.imagen(
            url: url, pagina: 2, tamano: CGSize(width: 320, height: 480)))
        let despues = cache.estadisticas()
        XCTAssertEqual(despues.documentosAbiertos, 2)
        XCTAssertEqual(despues.missesImagen, 2)
    }

    private func crearPDF(paginas: Int, textoExtra: String) throws -> URL {
        let carpeta = FileManager.default.temporaryDirectory.appendingPathComponent(
            "CuadernU-CachePDF-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: carpeta,
                                                withIntermediateDirectories: true)
        temporales.append(carpeta)
        let url = carpeta.appendingPathComponent("fixture.pdf")
        try escribirPDF(url: url, paginas: paginas, textoExtra: textoExtra)
        return url
    }

    private func escribirPDF(url: URL, paginas: Int, textoExtra: String) throws {
        let tamano = CGSize(width: 595, height: 842)
        let renderer = UIGraphicsPDFRenderer(
            bounds: CGRect(origin: .zero, size: tamano))
        try renderer.writePDF(to: url) { contexto in
            for indice in 0..<paginas {
                contexto.beginPage()
                ("Página \(indice + 1) \(textoExtra)" as NSString).draw(
                    in: CGRect(x: 30, y: 30, width: 520, height: 700),
                    withAttributes: [.font: UIFont.systemFont(ofSize: 14)])
            }
        }
    }
}
