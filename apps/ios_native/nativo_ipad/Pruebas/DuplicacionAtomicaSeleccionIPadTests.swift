import XCTest

final class DuplicacionAtomicaSeleccionIPadTests: XCTestCase {
    func testDuplicarTrazoTextoYMedioSeDeshaceEnUnSoloPaso() throws {
        let sesion = try XCTUnwrap(Sesion())

        sesion.comenzarTrazo(ancho: 3, color: 0xFF102030, resaltador: false)
        sesion.anadirMuestra(
            x: 20, y: 30, presion: 0.6,
            altitud: 1, azimut: 0, tiempo: 1)
        sesion.anadirMuestra(
            x: 70, y: 55, presion: 0.7,
            altitud: 1, azimut: 0, tiempo: 1.01)
        _ = try XCTUnwrap(sesion.terminarTrazo())

        _ = try XCTUnwrap(sesion.anadirTexto(
            "RLC", marco: CGRect(x: 100, y: 120, width: 140, height: 52),
            id: "texto-original"))
        _ = try XCTUnwrap(sesion.anadirMedio(
            hash: "hash-prueba", nombre: "diagrama.png", tipo: .imagen,
            marco: CGRect(x: 280, y: 160, width: 180, height: 120),
            id: "medio-original"))

        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertEqual(sesion.textos.count, 1)
        XCTAssertEqual(sesion.medios.count, 1)

        let original = SeleccionLienzo(trazos: [0], textos: [0], medios: [0])
        let copia = sesion.duplicarSeleccion(
            original, desplazamiento: CGSize(width: 48, height: 64))

        XCTAssertEqual(copia.total, 3)
        XCTAssertEqual(sesion.cantidadTrazos, 2)
        XCTAssertEqual(sesion.textos.count, 2)
        XCTAssertEqual(sesion.medios.count, 2)
        XCTAssertTrue(sesion.puedeDeshacer)

        // La duplicación completa debe ser una sola OperacionPaginaCompleta.
        sesion.deshacer()
        XCTAssertEqual(sesion.cantidadTrazos, 1)
        XCTAssertEqual(sesion.textos.count, 1)
        XCTAssertEqual(sesion.medios.count, 1)
        XCTAssertEqual(sesion.textos.first?.id, "texto-original")
        XCTAssertEqual(sesion.medios.first?.id, "medio-original")

        sesion.rehacer()
        XCTAssertEqual(sesion.cantidadTrazos, 2)
        XCTAssertEqual(sesion.textos.count, 2)
        XCTAssertEqual(sesion.medios.count, 2)
    }

    func testDuplicacionNoReutilizaIdentidadesEstables() throws {
        let sesion = try XCTUnwrap(Sesion())
        sesion.comenzarTrazo(ancho: 2, color: 0xFF000000, resaltador: false)
        sesion.anadirMuestra(
            x: 10, y: 10, presion: 0.5,
            altitud: 1, azimut: 0, tiempo: 1)
        sesion.anadirMuestra(
            x: 50, y: 20, presion: 0.5,
            altitud: 1, azimut: 0, tiempo: 1.01)
        let indice = try XCTUnwrap(sesion.terminarTrazo())
        let idOriginal = try XCTUnwrap(sesion.asegurarIdTrazo(indice))

        let copia = sesion.duplicarSeleccion(
            SeleccionLienzo(trazos: [indice]),
            desplazamiento: CGSize(width: 30, height: 30))
        let indiceCopia = try XCTUnwrap(copia.trazos.first)
        let idCopia = try XCTUnwrap(sesion.idTrazo(indiceCopia))

        XCTAssertNotEqual(idOriginal, idCopia)
    }
}
