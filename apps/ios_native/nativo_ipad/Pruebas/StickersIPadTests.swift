import XCTest
import UIKit
@testable import CuadernUEstudio

final class StickersIPadTests: XCTestCase {
    private func base() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("stickers-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    private func png() -> Data {
        let renderer = UIGraphicsImageRenderer(size: CGSize(width: 20, height: 10))
        return renderer.pngData { contexto in
            UIColor.systemBlue.setFill()
            contexto.fill(CGRect(x: 0, y: 0, width: 20, height: 10))
        }
    }

    func testImportarCreaColeccionStickerYElementoImagen() throws {
        let carpeta = try base()
        let biblioteca = BibliotecaElementosReutilizablesIPad(base: carpeta)
        let gestor = GestorStickersIPad(biblioteca: biblioteca)
        let resultado = try gestor.importar(datos: png(), nombre: "Ohm",
                                            extensionArchivo: "png",
                                            etiquetas: ["circuitos"])
        XCTAssertEqual(resultado.elemento.tipo, .imagen)
        XCTAssertTrue(resultado.coleccion.etiquetas.contains(GestorStickersIPad.etiquetaSistema))
        XCTAssertEqual(resultado.elemento.anchoNatural, 20)
        XCTAssertEqual(resultado.elemento.altoNatural, 10)
        XCTAssertNotNil(try gestor.payload(resultado.elemento.id))
    }

    func testBusquedaSoloDevuelveColeccionesSticker() throws {
        let carpeta = try base()
        let biblioteca = BibliotecaElementosReutilizablesIPad(base: carpeta)
        let normal = try biblioteca.crearColeccion(nombre: "Normal")
        _ = try biblioteca.guardarElemento(
            coleccionId: normal.id, tipo: .imagen, nombre: "No sticker",
            payload: png(), extensionArchivo: "png", anchoNatural: 20, altoNatural: 10)
        let gestor = GestorStickersIPad(biblioteca: biblioteca)
        _ = try gestor.importar(datos: png(), nombre: "Sticker real", extensionArchivo: "png")
        XCTAssertEqual(gestor.stickers().count, 1)
        XCTAssertEqual(gestor.stickers().first?.nombre, "Sticker real")
    }

    func testBibliotecaReabreStickers() throws {
        let carpeta = try base()
        do {
            let biblioteca = BibliotecaElementosReutilizablesIPad(base: carpeta)
            _ = try GestorStickersIPad(biblioteca: biblioteca)
                .importar(datos: png(), nombre: "Persistente", extensionArchivo: "png")
        }
        let reabierta = GestorStickersIPad(
            biblioteca: BibliotecaElementosReutilizablesIPad(base: carpeta))
        XCTAssertEqual(reabierta.stickers().map(\.nombre), ["Persistente"])
    }

    func testRechazaDatosQueNoSonImagen() throws {
        let gestor = GestorStickersIPad(
            biblioteca: BibliotecaElementosReutilizablesIPad(base: try base()))
        XCTAssertThrowsError(try gestor.importar(
            datos: Data("hola".utf8), nombre: "malo", extensionArchivo: "png")) {
            guard case ErrorStickerIPad.imagenInvalida = $0 else {
                return XCTFail("Error inesperado: \($0)")
            }
        }
    }
}
