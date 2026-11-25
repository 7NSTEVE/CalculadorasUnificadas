import XCTest
@testable import CuadernUEstudio

final class DuplicacionPrivacidadCuadernoIPadTests: XCTestCase {
    func testDuplicaPoliticaProtegidaConDocumentoNuevo() throws {
        let duplicador = DuplicadorPrivacidadCuadernoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        var fuente = ConfiguracionProteccionCuaderno(documentoId: origen)
        fuente.nivel = .protegido
        fuente.ocultarPrevisualizaciones = true
        fuente.cifrarRespaldos = true
        fuente.compresion = .maxima
        fuente.generacionesAutoguardado = 28
        RepositorioPrivacidadCuadernos.compartido.guardar(fuente)

        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 1)
        let copia = try XCTUnwrap(duplicador.persistida(documento: destino))
        XCTAssertEqual(copia.documentoId, destino)
        XCTAssertEqual(copia.nivel, .protegido)
        XCTAssertTrue(copia.ocultarPrevisualizaciones)
        XCTAssertTrue(copia.cifrarRespaldos)
        XCTAssertEqual(copia.compresion, .maxima)
        XCTAssertEqual(copia.generacionesAutoguardado, 28)
        XCTAssertNotEqual(copia.documentoId, fuente.documentoId)
    }

    func testRollbackRetiraPoliticaExplicitaYDejaDefaultsEnCache() throws {
        let duplicador = DuplicadorPrivacidadCuadernoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        var fuente = ConfiguracionProteccionCuaderno(documentoId: origen)
        fuente.nivel = .privado
        fuente.ocultarPrevisualizaciones = true
        RepositorioPrivacidadCuadernos.compartido.guardar(fuente)
        _ = try duplicador.duplicar(desde: origen, hacia: destino)

        try duplicador.eliminarDocumento(destino)
        XCTAssertNil(try duplicador.persistida(documento: destino))
        let efectiva = RepositorioPrivacidadCuadernos.compartido
            .configuracion(destino)
        XCTAssertEqual(efectiva.documentoId, destino)
        XCTAssertEqual(efectiva.nivel, .normal)
        XCTAssertFalse(efectiva.ocultarPrevisualizaciones)

        // La misma identidad vuelve a ser utilizable por la transacción.
        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 1)
        XCTAssertEqual(try duplicador.persistida(documento: destino)?.nivel,
                       .privado)
    }

    func testOrigenSinConfiguracionExplicitaNoMaterializaDestino() throws {
        let duplicador = DuplicadorPrivacidadCuadernoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        XCTAssertNil(try duplicador.persistida(documento: origen))
        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 0)
        XCTAssertNil(try duplicador.persistida(documento: destino))
        XCTAssertEqual(RepositorioPrivacidadCuadernos.compartido
            .configuracion(destino).nivel, .normal)
    }

    func testNoSobrescribePoliticaDestinoLegitima() throws {
        let duplicador = DuplicadorPrivacidadCuadernoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        var fuente = ConfiguracionProteccionCuaderno(documentoId: origen)
        fuente.nivel = .protegido
        var existente = ConfiguracionProteccionCuaderno(documentoId: destino)
        existente.nivel = .privado
        existente.compresion = .ninguna
        RepositorioPrivacidadCuadernos.compartido.guardar(fuente)
        RepositorioPrivacidadCuadernos.compartido.guardar(existente)

        XCTAssertThrowsError(try duplicador.duplicar(desde: origen, hacia: destino))
        let conservada = try XCTUnwrap(duplicador.persistida(documento: destino))
        XCTAssertEqual(conservada.nivel, .privado)
        XCTAssertEqual(conservada.compresion, .ninguna)
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
