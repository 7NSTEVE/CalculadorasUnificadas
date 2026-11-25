import UIKit
import XCTest
@testable import CuadernUEstudio

final class ClipsPDFDocumentoIPadTests: XCTestCase {
    func testDuplicacionConservaMascaraConIdentidadPropia() throws {
        let repo = RepositorioClipsPDFDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        let elemento = "clip-\(UUID().uuidString)"
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        let fuente = ClipConFuenteIPad(
            elementoId: elemento,
            hashFuente: "hash-pdf-prueba",
            pagina: 7,
            regionNormalizada: CGRect(x: 0.12, y: 0.18,
                                      width: 0.62, height: 0.44),
            forma: .poligonal,
            puntosNormalizados: [
                CGPoint(x: 0.12, y: 0.18),
                CGPoint(x: 0.74, y: 0.20),
                CGPoint(x: 0.66, y: 0.62),
                CGPoint(x: 0.18, y: 0.58),
            ],
            nota: "original")
        XCTAssertTrue(repo.registrar(fuente, documento: origen))

        XCTAssertEqual(try repo.duplicar(desde: origen, hacia: destino), 1)
        let copia = try XCTUnwrap(repo.clip(documento: destino,
                                             elementoId: elemento))
        XCTAssertNotEqual(copia.id, fuente.id)
        XCTAssertEqual(copia.elementoId, fuente.elementoId)
        XCTAssertEqual(copia.hashFuente, fuente.hashFuente)
        XCTAssertEqual(copia.pagina, fuente.pagina)
        XCTAssertEqual(copia.regionNormalizada, fuente.regionNormalizada)
        XCTAssertEqual(copia.forma, fuente.forma)
        XCTAssertEqual(copia.puntosNormalizados, fuente.puntosNormalizados)

        var editada = copia
        editada.nota = "solo en la copia"
        editada.regionNormalizada = CGRect(x: 0.2, y: 0.2,
                                           width: 0.4, height: 0.3)
        XCTAssertTrue(repo.registrar(editada, documento: destino))

        XCTAssertEqual(repo.clip(documento: origen,
                                  elementoId: elemento)?.nota, "original")
        XCTAssertEqual(repo.clip(documento: destino,
                                  elementoId: elemento)?.nota, "solo en la copia")
        XCTAssertNotEqual(repo.clip(documento: origen,
                                     elementoId: elemento)?.regionNormalizada,
                          repo.clip(documento: destino,
                                     elementoId: elemento)?.regionNormalizada)
    }

    func testDuplicarElementoDentroDelDocumentoCreaMascaraIndependiente() throws {
        let repo = RepositorioClipsPDFDocumentoIPad.compartido
        let documento = idDocumentoAleatorio()
        let origen = "clip-origen-\(UUID().uuidString)"
        let destino = "clip-copia-\(UUID().uuidString)"
        defer { try? repo.eliminarDocumento(documento) }

        let fuente = ClipConFuenteIPad(
            elementoId: origen,
            hashFuente: "hash-local",
            pagina: 2,
            regionNormalizada: CGRect(x: 0.1, y: 0.1,
                                      width: 0.7, height: 0.5),
            forma: .libre,
            puntosNormalizados: [
                CGPoint(x: 0.1, y: 0.1),
                CGPoint(x: 0.8, y: 0.14),
                CGPoint(x: 0.72, y: 0.6),
                CGPoint(x: 0.18, y: 0.55),
            ],
            nota: "fuente")
        XCTAssertTrue(repo.registrar(fuente, documento: documento))

        XCTAssertTrue(repo.duplicarElemento(
            documento: documento, origen: origen, destino: destino))
        let copia = try XCTUnwrap(repo.clip(documento: documento,
                                             elementoId: destino))
        XCTAssertNotEqual(copia.id, fuente.id)
        XCTAssertEqual(copia.elementoId, destino)
        XCTAssertEqual(copia.puntosNormalizados, fuente.puntosNormalizados)

        var editada = copia
        editada.nota = "editada"
        editada.regionNormalizada = CGRect(x: 0.25, y: 0.2,
                                           width: 0.35, height: 0.25)
        XCTAssertTrue(repo.registrar(editada, documento: documento))

        XCTAssertEqual(repo.clip(documento: documento,
                                  elementoId: origen)?.nota, "fuente")
        XCTAssertEqual(repo.clip(documento: documento,
                                  elementoId: destino)?.nota, "editada")
        XCTAssertNotEqual(repo.clip(documento: documento,
                                     elementoId: origen)?.regionNormalizada,
                          repo.clip(documento: documento,
                                     elementoId: destino)?.regionNormalizada)
    }

    func testNoSobrescribeDestinoConClipsExistentes() throws {
        let repo = RepositorioClipsPDFDocumentoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? repo.eliminarDocumento(origen)
            try? repo.eliminarDocumento(destino)
        }

        let fuente = ClipConFuenteIPad(
            elementoId: "fuente-\(UUID().uuidString)",
            hashFuente: "a", pagina: 0,
            regionNormalizada: CGRect(x: 0, y: 0, width: 0.5, height: 0.5))
        let existente = ClipConFuenteIPad(
            elementoId: "existente-\(UUID().uuidString)",
            hashFuente: "b", pagina: 1,
            regionNormalizada: CGRect(x: 0.2, y: 0.2, width: 0.4, height: 0.4))
        XCTAssertTrue(repo.registrar(fuente, documento: origen))
        XCTAssertTrue(repo.registrar(existente, documento: destino))

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.instantanea(documento: destino).count, 1)
        XCTAssertEqual(repo.clip(documento: destino,
                                  elementoId: existente.elementoId), existente)
        XCTAssertNil(repo.clip(documento: destino,
                                elementoId: fuente.elementoId))
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
