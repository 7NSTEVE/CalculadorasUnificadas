import UIKit
import XCTest
@testable import CuadernUEstudio

final class DuplicacionRegionesFondoIPadTests: XCTestCase {
    func testDuplicaRegionesYVerificaPersistenciaReal() throws {
        let duplicador = DuplicadorRegionesFondoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let cuadricula = RegionFondoDocumento(
            nombre: "Laboratorio",
            marco: CGRect(x: 40, y: 80, width: 720, height: 520),
            tipo: .cuadricula,
            colorARGB: 0xFFF8_F8F6,
            colorPautaARGB: 0x4458_6A7A,
            separacion: 20)
        let rayado = RegionFondoDocumento(
            nombre: "Apuntes",
            marco: CGRect(x: 40, y: 640, width: 720, height: 900),
            tipo: .rayado,
            colorARGB: 0xFFFF_F9EA,
            colorPautaARGB: 0x3350_6070,
            separacion: 28)
        RepositorioRegionesFondo.compartido.guardar(
            [cuadricula, rayado], documento: origen)

        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 2)
        XCTAssertEqual(RepositorioRegionesFondo.compartido.regiones(destino),
                       [cuadricula, rayado])
        XCTAssertEqual(try duplicador.persistidas(documento: destino),
                       [cuadricula, rayado])
    }

    func testConservaUUIDDentroDelNuevoNamespaceDocumental() throws {
        let duplicador = DuplicadorRegionesFondoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let region = RegionFondoDocumento(
            nombre: "Zona técnica",
            marco: CGRect(x: 100, y: 200, width: 500, height: 500),
            tipo: .oscuro,
            colorARGB: 0xFF1C_1C1E)
        RepositorioRegionesFondo.compartido.guardar([region], documento: origen)
        _ = try duplicador.duplicar(desde: origen, hacia: destino)

        XCTAssertEqual(RepositorioRegionesFondo.compartido
            .regiones(destino).first?.id, region.id)
    }

    func testNoSobrescribeDestinoConRegionesExistentes() throws {
        let duplicador = DuplicadorRegionesFondoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let fuente = RegionFondoDocumento(
            nombre: "Fuente",
            marco: CGRect(x: 0, y: 0, width: 300, height: 300),
            tipo: .blanco,
            colorARGB: 0xFFFF_FFFF)
        let existente = RegionFondoDocumento(
            nombre: "Destino legítimo",
            marco: CGRect(x: 10, y: 10, width: 240, height: 240),
            tipo: .crema,
            colorARGB: 0xFFFF_F5DC)
        RepositorioRegionesFondo.compartido.guardar([fuente], documento: origen)
        RepositorioRegionesFondo.compartido.guardar([existente], documento: destino)

        XCTAssertThrowsError(try duplicador.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(RepositorioRegionesFondo.compartido.regiones(destino),
                       [existente])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
