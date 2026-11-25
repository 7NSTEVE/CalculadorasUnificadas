import XCTest
@testable import CuadernUEstudio

final class EdicionesImagenDocumentoIPadTests: XCTestCase {
    func testDuplicacionConservaEdicionPeroLaCopiaPuedeDivergir() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        let medio = "imagen-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        var fuente = AjustesEdicionImagen()
        fuente.forma = .circular
        fuente.invertir = true
        fuente.contraste = 1.84
        fuente.enderezarGrados = 7.5
        fuente.ocr = [BloqueOCR(texto: "V = I·R", confianza: 0.98,
                                caja: CGRect(x: 0.1, y: 0.2,
                                             width: 0.6, height: 0.2))]
        XCTAssertTrue(repo.guardar(fuente, documento: origen, id: medio))

        XCTAssertEqual(try repo.duplicar(desde: origen, hacia: destino), 1)
        XCTAssertEqual(repo.ajustes(documento: destino, id: medio), fuente)

        var copia = repo.ajustes(documento: destino, id: medio)
        copia.invertir = false
        copia.contraste = 0.71
        copia.enderezarGrados = -3
        XCTAssertTrue(repo.guardar(copia, documento: destino, id: medio))

        XCTAssertEqual(repo.ajustes(documento: origen, id: medio), fuente)
        XCTAssertEqual(repo.ajustes(documento: destino, id: medio), copia)
        XCTAssertNotEqual(repo.ajustes(documento: origen, id: medio),
                          repo.ajustes(documento: destino, id: medio))
    }

    func testDuplicarElementoDentroDelDocumentoCreaAjustesIndependientes() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let documento = idDocumentoAleatorio()
        let origen = "imagen-origen-\(UUID().uuidString)"
        let destino = "imagen-copia-\(UUID().uuidString)"
        defer { try? repo.eliminarDocumento(documento) }

        var fuente = AjustesEdicionImagen()
        fuente.forma = .poligonal
        fuente.monocromo = true
        fuente.contraste = 1.63
        XCTAssertTrue(repo.guardar(fuente, documento: documento, id: origen))

        XCTAssertTrue(repo.duplicarElemento(
            documento: documento, origen: origen, destino: destino))
        XCTAssertEqual(repo.ajustes(documento: documento, id: destino), fuente)

        var copia = repo.ajustes(documento: documento, id: destino)
        copia.monocromo = false
        copia.contraste = 0.82
        XCTAssertTrue(repo.guardar(copia, documento: documento, id: destino))

        XCTAssertEqual(repo.ajustes(documento: documento, id: origen), fuente)
        XCTAssertEqual(repo.ajustes(documento: documento, id: destino), copia)
    }

    func testLegacyOCRSeEnrutaCuandoElIdPerteneceAUnSoloDocumento() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let legacy = RepositorioEdicionesImagen.compartido
        let documento = idDocumentoAleatorio()
        let medio = "ocr-unico-\(UUID().uuidString)"
        defer { try? repo.eliminarDocumento(documento) }

        var historico = AjustesEdicionImagen()
        historico.contraste = 0.63
        legacy.guardarLegacy(historico, id: medio)

        var base = AjustesEdicionImagen()
        base.contraste = 1.4
        XCTAssertTrue(repo.guardar(base, documento: documento, id: medio))
        XCTAssertEqual(repo.documentoUnico(materializadoPara: medio), documento)

        var lectura = legacy.ajustes(id: medio)
        XCTAssertEqual(lectura.documentoContexto, documento)
        lectura.ocr = [BloqueOCR(texto: "R1 = 10 kΩ", confianza: 0.99,
                                 caja: CGRect(x: 0.1, y: 0.1,
                                              width: 0.5, height: 0.2))]
        legacy.guardar(lectura, id: medio)

        XCTAssertEqual(repo.ajustes(documento: documento, id: medio).ocr,
                       lectura.ocr)
        XCTAssertNil(repo.ajustes(documento: documento, id: medio).documentoContexto)
        XCTAssertEqual(legacy.ajustesLegacy(id: medio), historico)
    }

    func testIdCompartidoRequiereContextoExplicitoYNoEligeDocumentoAlAzar() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let legacy = RepositorioEdicionesImagen.compartido
        let documentoA = idDocumentoAleatorio()
        let documentoB = idDocumentoAleatorio(excluyendo: [documentoA])
        let medio = "ocr-compartido-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(documentoA)
            try? repo.eliminarDocumento(documentoB)
        }

        var historico = AjustesEdicionImagen()
        historico.brillo = 0.12
        legacy.guardarLegacy(historico, id: medio)

        var ajustesA = AjustesEdicionImagen()
        ajustesA.contraste = 1.2
        var ajustesB = AjustesEdicionImagen()
        ajustesB.contraste = 1.8
        XCTAssertTrue(repo.guardar(ajustesA, documento: documentoA, id: medio))
        XCTAssertTrue(repo.guardar(ajustesB, documento: documentoB, id: medio))
        XCTAssertNil(repo.documentoUnico(materializadoPara: medio))

        let ambiguo = legacy.ajustes(id: medio)
        XCTAssertNil(ambiguo.documentoContexto)
        XCTAssertEqual(ambiguo, historico)

        legacy.conContexto(documento: documentoB, id: medio) {
            var lectura = legacy.ajustes(id: medio)
            XCTAssertEqual(lectura.documentoContexto, documentoB)
            lectura.contraste = 2.2
            lectura.ocr = [BloqueOCR(texto: "solo B", confianza: 1,
                                     caja: CGRect(x: 0, y: 0,
                                                  width: 1, height: 0.1))]
            legacy.guardar(lectura, id: medio)
        }

        XCTAssertEqual(repo.ajustes(documento: documentoA, id: medio), ajustesA)
        XCTAssertEqual(repo.ajustes(documento: documentoB, id: medio).contraste, 2.2)
        XCTAssertNil(repo.ajustes(documento: documentoB, id: medio).documentoContexto)
        XCTAssertEqual(repo.ajustes(documento: documentoB, id: medio).ocr.first?.texto,
                       "solo B")
        XCTAssertEqual(legacy.ajustesLegacy(id: medio), historico)
    }

    func testLegacySeMaterializaPorDocumentoAntesDeDivergir() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let copia = idDocumentoAleatorio(excluyendo: [origen])
        let medio = "legacy-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(copia)
        }

        var heredado = AjustesEdicionImagen()
        heredado.monocromo = true
        heredado.contraste = 1.57
        RepositorioEdicionesImagen.compartido.guardar(heredado, id: medio)

        XCTAssertEqual(repo.ajustes(documento: origen, id: medio), heredado)
        XCTAssertEqual(repo.ajustes(documento: copia, id: medio), heredado)
        XCTAssertEqual(repo.instantanea(documento: origen)[medio], heredado)
        XCTAssertEqual(repo.instantanea(documento: copia)[medio], heredado)

        var editada = heredado
        editada.monocromo = false
        editada.saturacion = 0.42
        XCTAssertTrue(repo.guardar(editada, documento: copia, id: medio))

        XCTAssertEqual(repo.ajustes(documento: origen, id: medio), heredado)
        XCTAssertEqual(repo.ajustes(documento: copia, id: medio), editada)
    }

    func testNoSobrescribeNamespaceDestinoConContenido() throws {
        let repo = RepositorioEdicionesImagenDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        var fuente = AjustesEdicionImagen()
        fuente.brillo = 0.2
        var existente = AjustesEdicionImagen()
        existente.brillo = -0.3
        XCTAssertTrue(repo.guardar(fuente, documento: origen, id: "medio-fuente"))
        XCTAssertTrue(repo.guardar(existente, documento: destino, id: "medio-destino"))

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.ajustes(documento: destino, id: "medio-destino"),
                       existente)
        XCTAssertNil(repo.instantanea(documento: destino)["medio-fuente"])
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
