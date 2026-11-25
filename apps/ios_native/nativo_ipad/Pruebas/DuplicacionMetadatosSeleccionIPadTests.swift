import UIKit
import XCTest
@testable import CuadernUEstudio

final class DuplicacionMetadatosSeleccionIPadTests: XCTestCase {
    func testDuplicaGrupoBloqueoEtiquetasYAccesibilidadConGrupoNuevo() throws {
        let origenId = DocumentoPersistenteID(UUID())
        let origen = try XCTUnwrap(Sesion(idPersistente: origenId))
        let destino = idDocumentoAleatorio(excluyendo: [origen.documentoPersistenteId])
        let duplicador = DuplicadorMetadatosSeleccionIPad.compartido
        defer {
            try? duplicador.eliminarDocumento(origen.documentoPersistenteId)
            try? duplicador.eliminarDocumento(destino)
        }

        let idA = try XCTUnwrap(origen.anadirTexto(
            "Ley de Ohm", marco: CGRect(x: 20, y: 30, width: 220, height: 60)))
        let idB = try XCTUnwrap(origen.anadirTexto(
            "V = I·R", marco: CGRect(x: 20, y: 110, width: 220, height: 60)))
        let seleccion = SeleccionLienzo(textos: [0, 1])
        let repo = RepositorioMetadatosSeleccionIPad.compartido
        repo.agrupar(seleccion, sesion: origen)
        repo.bloquear(seleccion, sesion: origen, valor: true)
        repo.actualizarEtiquetas(
            ["circuitos", "fórmula"],
            descripcion: "Relación fundamental entre voltaje, corriente y resistencia",
            seleccion: seleccion,
            sesion: origen)

        let fuente = try duplicador.instantanea(
            documento: origen.documentoPersistenteId)
        let claveA = "x:\(idA)"
        let claveB = "x:\(idB)"
        let grupoFuente = try XCTUnwrap(fuente[claveA]?.grupo)
        XCTAssertEqual(fuente[claveB]?.grupo, grupoFuente)
        XCTAssertEqual(fuente[claveA]?.bloqueado, true)
        XCTAssertEqual(Set(fuente[claveA]?.etiquetas ?? []),
                       Set(["circuitos", "fórmula"]))

        XCTAssertEqual(try duplicador.duplicar(
            desde: origen.documentoPersistenteId, hacia: destino), 2)
        let copia = try duplicador.instantanea(documento: destino)
        let grupoCopia = try XCTUnwrap(copia[claveA]?.grupo)

        XCTAssertEqual(copia[claveB]?.grupo, grupoCopia)
        XCTAssertNotEqual(grupoCopia, grupoFuente)
        XCTAssertEqual(copia[claveA]?.bloqueado, true)
        XCTAssertEqual(Set(copia[claveA]?.etiquetas ?? []),
                       Set(["circuitos", "fórmula"]))
        XCTAssertEqual(copia[claveA]?.descripcion,
                       "Relación fundamental entre voltaje, corriente y resistencia")
    }

    func testDestinoVacioPreinicializadoEsAceptado() throws {
        let origenId = DocumentoPersistenteID(UUID())
        let destinoId = DocumentoPersistenteID(UUID())
        let origen = try XCTUnwrap(Sesion(idPersistente: origenId))
        let destino = try XCTUnwrap(Sesion(idPersistente: destinoId))
        let duplicador = DuplicadorMetadatosSeleccionIPad.compartido
        defer {
            try? duplicador.eliminarDocumento(origen.documentoPersistenteId)
            try? duplicador.eliminarDocumento(destino.documentoPersistenteId)
        }

        _ = try XCTUnwrap(origen.anadirTexto(
            "dato", marco: CGRect(x: 0, y: 0, width: 120, height: 50)))
        let seleccion = SeleccionLienzo(textos: [0])
        RepositorioMetadatosSeleccionIPad.compartido.bloquear(
            seleccion, sesion: origen, valor: true)

        // La UI puede consultar el destino antes de que llegue la copia. Esa
        // consulta no debe convertir un documento vacío en una colisión falsa.
        _ = RepositorioMetadatosSeleccionIPad.compartido
            .filtrarDesbloqueados(SeleccionLienzo(), sesion: destino)

        XCTAssertEqual(try duplicador.duplicar(
            desde: origen.documentoPersistenteId,
            hacia: destino.documentoPersistenteId), 1)
    }

    private func idDocumentoAleatorio(excluyendo: Set<UInt64> = []) -> UInt64 {
        var valor: UInt64
        repeat { valor = UInt64.random(in: 1...UInt64.max) }
        while excluyendo.contains(valor)
        return valor
    }
}
