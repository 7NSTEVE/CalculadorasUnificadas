import XCTest
@testable import CuadernUEstudio

final class AjustesTextoDocumentoIPadTests: XCTestCase {
    func testDuplicacionConservaAjustesPeroPermiteDivergir() throws {
        let repo = RepositorioAjustesTextoDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        let texto = "texto-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        var fuente = AjustesCajaTextoIPad()
        fuente.dimension = .cajaFija
        fuente.padding = 22
        fuente.markdown = true
        fuente.continuarListas = false
        fuente.checkboxes = true
        fuente.matematicasInline = true
        XCTAssertTrue(repo.guardar(fuente, documento: origen, id: texto))

        XCTAssertEqual(try repo.duplicar(desde: origen, hacia: destino), 1)
        XCTAssertEqual(repo.ajustes(documento: destino, id: texto), fuente)

        var copia = repo.ajustes(documento: destino, id: texto)
        copia.padding = 4
        copia.markdown = false
        copia.checkboxes = false
        XCTAssertTrue(repo.guardar(copia, documento: destino, id: texto))

        XCTAssertEqual(repo.ajustes(documento: origen, id: texto), fuente)
        XCTAssertEqual(repo.ajustes(documento: destino, id: texto), copia)
        XCTAssertNotEqual(repo.ajustes(documento: origen, id: texto),
                          repo.ajustes(documento: destino, id: texto))
    }

    func testDuplicarElementoDentroDelDocumentoAislaLosAjustes() throws {
        let repo = RepositorioAjustesTextoDocumentoIPad.compartido
        let documento = idDocumentoAleatorio()
        let origen = "texto-origen-\(UUID().uuidString)"
        let destino = "texto-copia-\(UUID().uuidString)"
        defer { try? repo.eliminarDocumento(documento) }

        var fuente = AjustesCajaTextoIPad()
        fuente.padding = 19
        fuente.markdown = true
        fuente.checkboxes = true
        fuente.matematicasInline = true
        XCTAssertTrue(repo.guardar(fuente, documento: documento, id: origen))

        XCTAssertTrue(repo.duplicarElemento(
            documento: documento, origen: origen, destino: destino))
        XCTAssertEqual(repo.ajustes(documento: documento, id: destino), fuente)

        var copia = repo.ajustes(documento: documento, id: destino)
        copia.padding = 3
        copia.markdown = false
        copia.checkboxes = false
        XCTAssertTrue(repo.guardar(copia, documento: documento, id: destino))

        XCTAssertEqual(repo.ajustes(documento: documento, id: origen), fuente)
        XCTAssertEqual(repo.ajustes(documento: documento, id: destino), copia)
    }

    func testContextoLegacyEnrutaLecturaYGuardadoAlDocumentoCorrecto() throws {
        let repo = RepositorioAjustesTextoDocumentoIPad.compartido
        let legacy = RepositorioAjustesTextoIPad.compartido
        let documentoA = idDocumentoAleatorio()
        let documentoB = idDocumentoAleatorio(excluyendo: [documentoA])
        let texto = "texto-contexto-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(documentoA)
            try? repo.eliminarDocumento(documentoB)
        }

        var historico = AjustesCajaTextoIPad()
        historico.padding = 31
        legacy.guardarLegacy(historico, id: texto)

        var ajustesA = AjustesCajaTextoIPad()
        ajustesA.padding = 5
        ajustesA.markdown = true
        var ajustesB = AjustesCajaTextoIPad()
        ajustesB.padding = 17
        ajustesB.checkboxes = false
        XCTAssertTrue(repo.guardar(ajustesA, documento: documentoA, id: texto))
        XCTAssertTrue(repo.guardar(ajustesB, documento: documentoB, id: texto))

        legacy.conContexto(documento: documentoA, id: texto) {
            var lectura = legacy.ajustes(texto)
            XCTAssertEqual(lectura.padding, 5)
            XCTAssertEqual(lectura.documentoContexto, documentoA)
            lectura.padding = 9
            lectura.markdown = false
            legacy.guardar(lectura, id: texto)
        }

        XCTAssertEqual(repo.ajustes(documento: documentoA, id: texto).padding, 9)
        XCTAssertNil(repo.ajustes(documento: documentoA, id: texto).documentoContexto)
        XCTAssertEqual(repo.ajustes(documento: documentoB, id: texto), ajustesB)
        XCTAssertEqual(legacy.ajustesLegacy(texto), historico)

        legacy.conContexto(documento: documentoB, id: texto) {
            let lectura = legacy.ajustes(texto)
            XCTAssertEqual(lectura.padding, 17)
            XCTAssertEqual(lectura.documentoContexto, documentoB)
        }
        XCTAssertNil(legacy.ajustesLegacy(texto).documentoContexto)
    }

    func testLegacySeMaterializaPorDocumentoAntesDeDivergir() throws {
        let repo = RepositorioAjustesTextoDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let copia = idDocumentoAleatorio(excluyendo: [origen])
        let texto = "legacy-texto-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(copia)
        }

        var heredado = AjustesCajaTextoIPad()
        heredado.dimension = .tamanoAutomatico
        heredado.padding = 16
        heredado.markdown = true
        heredado.matematicasInline = false
        RepositorioAjustesTextoIPad.compartido.guardar(heredado, id: texto)

        XCTAssertEqual(repo.ajustes(documento: origen, id: texto), heredado)
        XCTAssertEqual(repo.ajustes(documento: copia, id: texto), heredado)
        XCTAssertEqual(repo.instantanea(documento: origen)[texto], heredado)
        XCTAssertEqual(repo.instantanea(documento: copia)[texto], heredado)

        var editada = heredado
        editada.padding = 2
        editada.markdown = false
        XCTAssertTrue(repo.guardar(editada, documento: copia, id: texto))

        XCTAssertEqual(repo.ajustes(documento: origen, id: texto), heredado)
        XCTAssertEqual(repo.ajustes(documento: copia, id: texto), editada)
    }

    func testNoSobrescribeNamespaceDestinoConContenido() throws {
        let repo = RepositorioAjustesTextoDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        var fuente = AjustesCajaTextoIPad()
        fuente.padding = 30
        var existente = AjustesCajaTextoIPad()
        existente.padding = 1
        XCTAssertTrue(repo.guardar(fuente, documento: origen, id: "fuente"))
        XCTAssertTrue(repo.guardar(existente, documento: destino, id: "existente"))

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.ajustes(documento: destino, id: "existente"), existente)
        XCTAssertNil(repo.instantanea(documento: destino)["fuente"])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
