import XCTest
@testable import CuadernUEstudio

final class DuplicacionSesionIntegracionTests: XCTestCase {
    func testCargarComoCopiaDuplicaNucleoYAgregado() throws {
        let origenId = DocumentoPersistenteID(UUID())
        let destinoId = DocumentoPersistenteID(UUID())
        let origen = try XCTUnwrap(Sesion(idPersistente: origenId))
        let destino = try XCTUnwrap(Sesion(idPersistente: destinoId))

        // Contenido canónico mínimo para demostrar que la página también viaja.
        origen.comenzarTrazo(ancho: 3, color: 0xFF11_2233, resaltador: false)
        origen.anadirMuestra(x: 10, y: 20, presion: 0.7,
                             altitud: 1, azimut: 0, tiempo: 1)
        origen.anadirMuestra(x: 40, y: 60, presion: 0.8,
                             altitud: 1, azimut: 0, tiempo: 2)
        XCTAssertNotNil(origen.terminarTrazo(id: "trazo-integracion"))

        var nota = NotaAdhesivaLienzo()
        nota.texto = "Sidecar de integración"
        RepositorioNotasAdhesivasIPad.compartido.agregar(
            nota, documento: origen.documentoPersistenteId)
        _ = RepositorioSemanticaIPad.compartido.marcar(
            documento: origen.documentoPersistenteId,
            claves: ["trazo-integracion"],
            tipo: .importante,
            titulo: "No perder",
            region: CGRect(x: 8, y: 18, width: 36, height: 46),
            cursoId: nil)

        // Simula la construcción real de la pestaña destino: la capa adhesiva
        // puede haber consultado el repositorio antes de recibir el blob copia.
        XCTAssertTrue(RepositorioNotasAdhesivasIPad.compartido
            .notas(destino.documentoPersistenteId).isEmpty)

        let bytes = try XCTUnwrap(origen.guardar())
        XCTAssertTrue(destino.cargarComoCopia(bytes))

        XCTAssertEqual(destino.cantidadTrazos, origen.cantidadTrazos)
        XCTAssertEqual(destino.idTrazo(0), "trazo-integracion")
        XCTAssertEqual(RepositorioNotasAdhesivasIPad.compartido
            .notas(destino.documentoPersistenteId).map(\.texto),
                       ["Sidecar de integración"])
        XCTAssertEqual(RepositorioSemanticaIPad.compartido
            .marcaciones(documento: destino.documentoPersistenteId).map(\.titulo),
                       ["No perder"])

        let bytesDestino = try XCTUnwrap(destino.guardar())
        XCTAssertEqual(IdentidadCopiaDocumentoIPad.origen(de: bytesDestino), destinoId)
        XCTAssertNotEqual(IdentidadCopiaDocumentoIPad.origen(de: bytesDestino), origenId)
    }

    func testCargarComoCopiaNoCopiaAgregadoSiElNucleoRechazaBytes() throws {
        let destinoId = DocumentoPersistenteID(UUID())
        let destino = try XCTUnwrap(Sesion(idPersistente: destinoId))
        let corrupto = Data([0x54, 0x49, 0x4E, 0x54, 0x41, 0x00, 0xFF])

        XCTAssertFalse(destino.cargarComoCopia(corrupto))
        XCTAssertTrue(RepositorioNotasAdhesivasIPad.compartido
            .notas(destino.documentoPersistenteId).isEmpty)
        XCTAssertTrue(RepositorioSemanticaIPad.compartido
            .marcaciones(documento: destino.documentoPersistenteId).isEmpty)
    }
}
