import XCTest
@testable import CuadernUEstudio

final class DuplicacionCapasIPadTests: XCTestCase {
    func testCapasConservanConfiguracionPeroRegeneranIds() throws {
        let repo = RepositorioCapas.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        var estado = EstadoCapasDocumento()
        estado.capas[0].visible = false
        estado.capas[1].bloqueada = true
        estado.capas[2].opacidad = 0.37
        estado.capas[3].exportar = false
        XCTAssertTrue(repo.guardar(estado, documento: origen))

        let fuente = repo.estado(origen)
        let cantidad = try repo.duplicar(desde: origen, hacia: destino)
        let copia = repo.estado(destino)

        XCTAssertEqual(cantidad, fuente.capas.count)
        XCTAssertEqual(copia.capas.count, fuente.capas.count)
        XCTAssertEqual(copia.capas.map(\.categoria), fuente.capas.map(\.categoria))
        XCTAssertEqual(copia.capas.map(\.nombre), fuente.capas.map(\.nombre))
        XCTAssertEqual(copia.capas.map(\.visible), fuente.capas.map(\.visible))
        XCTAssertEqual(copia.capas.map(\.bloqueada), fuente.capas.map(\.bloqueada))
        XCTAssertEqual(copia.capas.map(\.exportar), fuente.capas.map(\.exportar))
        XCTAssertEqual(copia.capas.map(\.opacidad), fuente.capas.map(\.opacidad))
        XCTAssertEqual(copia.capas.map(\.colorARGB), fuente.capas.map(\.colorARGB))

        let idsFuente = Set(fuente.capas.map(\.id))
        let idsCopia = Set(copia.capas.map(\.id))
        XCTAssertTrue(idsFuente.isDisjoint(with: idsCopia))
    }

    func testCapasNoSobrescribenDestinoExistente() throws {
        let repo = RepositorioCapas.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])

        var fuente = EstadoCapasDocumento()
        fuente.capas[0].nombre = "Fuente"
        var existente = EstadoCapasDocumento()
        existente.capas[0].nombre = "Destino"
        XCTAssertTrue(repo.guardar(fuente, documento: origen))
        XCTAssertTrue(repo.guardar(existente, documento: destino))

        XCTAssertThrowsError(try repo.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(repo.estado(destino).capas[0].nombre, "Destino")
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
