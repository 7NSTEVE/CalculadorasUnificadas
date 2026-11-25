import XCTest
@testable import CuadernUEstudio

final class DuplicacionRecallIPadTests: XCTestCase {
    func testRecallCopiaConfiguracionPeroReiniciaRevelado() throws {
        let repo = RepositorioRecallIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        repo.activar(true, documento: origen)
        repo.filtrarSoloExamen(true, documento: origen)
        let fuente = try XCTUnwrap(repo.agregar(
            documento: origen,
            claves: ["trazo-r1", "texto-voltaje"],
            region: CGRect(x: 120, y: 300, width: 240, height: 90),
            estilo: .variable,
            titulo: "Despeje de V",
            automatica: true,
            examen: true))
        repo.revelarPaso(fuente.id)
        XCTAssertEqual(try XCTUnwrap(repo.cubiertas(origen).first)
            .progresoRevelado, 0.25, accuracy: 0.000_001)

        let cantidad = try repo.duplicar(desde: origen, hacia: destino)
        let copia = try XCTUnwrap(repo.cubiertas(destino).first)

        XCTAssertEqual(cantidad, 1)
        XCTAssertTrue(repo.activo(destino))
        XCTAssertTrue(repo.soloExamen(destino))
        XCTAssertNotEqual(copia.id, fuente.id)
        XCTAssertEqual(copia.documento, destino)
        XCTAssertEqual(copia.claves, fuente.claves)
        XCTAssertEqual(copia.region, fuente.region)
        XCTAssertEqual(copia.estilo, fuente.estilo)
        XCTAssertEqual(copia.titulo, fuente.titulo)
        XCTAssertEqual(copia.automatica, fuente.automatica)
        XCTAssertEqual(copia.etiquetaExamen, fuente.etiquetaExamen)
        XCTAssertEqual(copia.progresoRevelado, 0, accuracy: 0.000_001)
    }

    func testRecallNoSobrescribeUnDestinoExistente() throws {
        let repo = RepositorioRecallIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        repo.activar(true, documento: origen)
        _ = repo.agregar(
            documento: origen,
            claves: ["fuente"],
            region: CGRect(x: 10, y: 10, width: 100, height: 60),
            estilo: .papel,
            titulo: "Fuente")
        repo.activar(false, documento: destino)
        _ = repo.agregar(
            documento: destino,
            claves: ["destino"],
            region: CGRect(x: 20, y: 20, width: 110, height: 70),
            estilo: .desenfoque,
            titulo: "No tocar")

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.cubiertas(destino).map(\.titulo), ["No tocar"])
        XCTAssertFalse(repo.activo(destino))
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
