import XCTest
@testable import CuadernUEstudio

final class DuplicacionFondosIPadTests: XCTestCase {
    func testFondosConservanAjustesBajoElMismoIdDeMedio() throws {
        let repo = RepositorioFondosObjetoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        let elemento = "medio-pdf-estable"
        let ajustes = AjustesFondoObjetoIPad(
            esFondo: true,
            bloqueado: true,
            oculto: false,
            opacidad: 0.42,
            contraste: 1.73,
            escala: 1.35,
            recolor: .azulTecnico)

        XCTAssertTrue(repo.guardar(
            ajustes, documento: origen, elemento: elemento,
            nombre: "Guía de circuitos"))

        let cantidad = try repo.duplicar(desde: origen, hacia: destino)

        XCTAssertEqual(cantidad, 1)
        XCTAssertEqual(repo.ajustes(documento: destino, elemento: elemento), ajustes)
        let registro = try XCTUnwrap(repo.registros(documento: destino).first)
        XCTAssertEqual(registro.id, elemento)
        XCTAssertEqual(registro.nombre, "Guía de circuitos")
    }

    func testFondosNoSobrescribenDestinoExistente() throws {
        let repo = RepositorioFondosObjetoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        var fuente = AjustesFondoObjetoIPad(); fuente.esFondo = true
        var existente = AjustesFondoObjetoIPad(); existente.esFondo = true
        existente.opacidad = 0.19
        XCTAssertTrue(repo.guardar(
            fuente, documento: origen, elemento: "fuente", nombre: "Fuente"))
        XCTAssertTrue(repo.guardar(
            existente, documento: destino, elemento: "destino", nombre: "Destino"))

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.ajustes(documento: destino, elemento: "destino"),
                       existente)
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
