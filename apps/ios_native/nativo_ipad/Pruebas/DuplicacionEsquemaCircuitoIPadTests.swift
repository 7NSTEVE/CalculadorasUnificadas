import UIKit
import XCTest
@testable import CuadernUEstudio

final class DuplicacionEsquemaCircuitoIPadTests: XCTestCase {
    func testDuplicaCircuitoCompletoYConservaIdentidadesInternas() throws {
        let duplicador = DuplicadorEsquemaCircuitoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let esquema = divisorVoltaje()
        RepositorioEsquemas.compartido.guardar(esquema, documento: origen)

        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 1)
        let copia = try XCTUnwrap(duplicador.persistido(documento: destino))
        XCTAssertEqual(copia, esquema)
        XCTAssertEqual(copia.id, esquema.id)
        XCTAssertEqual(copia.nodos.map(\.id), esquema.nodos.map(\.id))
        XCTAssertEqual(copia.componentes.map(\.id), esquema.componentes.map(\.id))
        XCTAssertEqual(copia.conexiones?.map(\.id), esquema.conexiones?.map(\.id))
        XCTAssertEqual(copia.spice(), esquema.spice())
    }

    func testEditarCopiaNoModificaCircuitoOriginal() throws {
        let duplicador = DuplicadorEsquemaCircuitoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let fuente = divisorVoltaje()
        RepositorioEsquemas.compartido.guardar(fuente, documento: origen)
        _ = try duplicador.duplicar(desde: origen, hacia: destino)

        var copia = RepositorioEsquemas.compartido.esquema(documento: destino)
        copia.titulo = "Divisor editado"
        copia.componentes[1].valor = 4_700
        RepositorioEsquemas.compartido.guardar(copia, documento: destino)

        XCTAssertEqual(try duplicador.persistido(documento: origen), fuente)
        XCTAssertEqual(try duplicador.persistido(documento: destino), copia)
        XCTAssertNotEqual(try duplicador.persistido(documento: origen),
                          try duplicador.persistido(documento: destino))
    }

    func testRollbackEliminaClaveCanonicaYPermiteReintentarUUID() throws {
        let duplicador = DuplicadorEsquemaCircuitoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let fuente = divisorVoltaje()
        RepositorioEsquemas.compartido.guardar(fuente, documento: origen)
        _ = try duplicador.duplicar(desde: origen, hacia: destino)
        try duplicador.eliminarDocumento(destino)

        XCTAssertNil(try duplicador.persistido(documento: destino))
        XCTAssertEqual(try duplicador.duplicar(desde: origen, hacia: destino), 1)
        XCTAssertEqual(try duplicador.persistido(documento: destino), fuente)
    }

    func testNoSobrescribeCircuitoDestinoLegitimo() throws {
        let duplicador = DuplicadorEsquemaCircuitoIPad.compartido
        let origen = idDocumentoAleatorio()
        let destino = idDocumentoAleatorio(excluyendo: [origen])
        defer {
            try? duplicador.eliminarDocumento(origen)
            try? duplicador.eliminarDocumento(destino)
        }

        let fuente = divisorVoltaje()
        var existente = divisorVoltaje()
        existente.titulo = "Circuito que ya existía"
        existente.componentes[0].valor = 12
        RepositorioEsquemas.compartido.guardar(fuente, documento: origen)
        RepositorioEsquemas.compartido.guardar(existente, documento: destino)

        XCTAssertThrowsError(try duplicador.duplicar(desde: origen, hacia: destino))
        XCTAssertEqual(try duplicador.persistido(documento: destino), existente)
    }

    private func divisorVoltaje() -> EsquemaEditable {
        let gnd = NodoEsquema(numero: 0, posicion: CGPoint(x: 260, y: 360),
                              nombre: "GND")
        let entrada = NodoEsquema(numero: 1, posicion: CGPoint(x: 120, y: 120),
                                  nombre: "Vin")
        let salida = NodoEsquema(numero: 2, posicion: CGPoint(x: 260, y: 220),
                                 nombre: "Vout")
        let fuente = ComponenteEsquema(
            referencia: "V1", tipo: .fuenteVoltaje,
            nodoA: 1, nodoB: 0,
            posicion: CGPoint(x: 80, y: 240), valor: 20)
        let r1 = ComponenteEsquema(
            referencia: "R1", tipo: .resistor,
            nodoA: 1, nodoB: 2,
            posicion: CGPoint(x: 220, y: 160), valor: 10_000)
        let r2 = ComponenteEsquema(
            referencia: "R2", tipo: .resistor,
            nodoA: 2, nodoB: 0,
            posicion: CGPoint(x: 260, y: 290), valor: 2_200)
        let cable = ConexionEsquema(nodoA: 2, nodoB: 0,
                                    tipo: .conductor, etiqueta: "retorno")
        var esquema = EsquemaEditable()
        esquema.titulo = "Divisor de voltaje"
        esquema.nodos = [gnd, entrada, salida]
        esquema.componentes = [fuente, r1, r2]
        esquema.conexiones = [cable]
        return esquema
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
