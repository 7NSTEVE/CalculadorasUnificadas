import XCTest

final class AnclasDirectasIPadTests: XCTestCase {
    override func tearDown() {
        PortapapelesAnclasDirectasIPad.compartido.limpiar()
        super.tearDown()
    }

    func testCopiaAnclaDeTextoConClaveEstable() throws {
        let sesion = try crearSesion()
        guard let id = sesion.anadirTexto(
            "Thévenin",
            marco: CGRect(x: 100, y: 200, width: 180, height: 60)) else {
            return XCTFail("No se pudo crear texto")
        }
        let ancla = PortapapelesAnclasDirectasIPad.compartido.copiar(
            seleccion: SeleccionLienzo(textos: [0]),
            sesion: sesion,
            titulo: "Teorema de Thévenin")

        XCTAssertEqual(ancla?.fuente.documentoId, sesion.documentoPersistenteId)
        XCTAssertEqual(ancla?.fuente.elementoId, "x:\(id)")
        XCTAssertEqual(ancla?.claves, ["x:\(id)"])
        XCTAssertEqual(ancla?.region,
                       CGRect(x: 100, y: 200, width: 180, height: 60))
    }

    func testRechazaEnlaceDeUnaSeleccionConsigoMisma() throws {
        let sesion = try crearSesion()
        _ = sesion.anadirTexto("R1", marco: CGRect(x: 10, y: 10, width: 80, height: 40))
        let seleccion = SeleccionLienzo(textos: [0])
        guard let ancla = PortapapelesAnclasDirectasIPad.compartido.copiar(
            seleccion: seleccion, sesion: sesion, titulo: "R1") else {
            return XCTFail("No se creó ancla")
        }

        XCTAssertNil(EnlazadorDirectoIPad.vincular(
            seleccionOrigen: seleccion,
            sesion: sesion,
            tituloOrigen: "R1",
            ancla: ancla))
    }

    func testVinculaDosSeleccionesYGeneraBacklink() throws {
        let sesion = try crearSesion()
        _ = sesion.anadirTexto("Definición", marco: CGRect(x: 20, y: 20, width: 130, height: 50))
        _ = sesion.anadirTexto("Ejemplo", marco: CGRect(x: 20, y: 220, width: 130, height: 50))
        guard let ancla = PortapapelesAnclasDirectasIPad.compartido.copiar(
            seleccion: SeleccionLienzo(textos: [0]),
            sesion: sesion,
            titulo: "Definición") else {
            return XCTFail("No se creó ancla")
        }

        let enlace = EnlazadorDirectoIPad.vincular(
            seleccionOrigen: SeleccionLienzo(textos: [1]),
            sesion: sesion,
            tituloOrigen: "Ejemplo",
            ancla: ancla)

        XCTAssertNotNil(enlace)
        XCTAssertEqual(enlace?.destino.elementoId, ancla.fuente.elementoId)
        let backlinks = RepositorioSemanticaIPad.compartido.backlinks(
            hacia: ancla.fuente)
        XCTAssertTrue(backlinks.contains { $0.id == enlace?.id })
    }

    func testSeleccionMixtaPrefiereAnclaCercanaAlCentro() throws {
        let sesion = try crearSesion()
        let a = sesion.anadirTexto("A", marco: CGRect(x: 0, y: 0, width: 40, height: 40))
        let b = sesion.anadirTexto("B", marco: CGRect(x: 180, y: 0, width: 40, height: 40))
        let c = sesion.anadirTexto("C", marco: CGRect(x: 92, y: 0, width: 40, height: 40))
        XCTAssertNotNil(a); XCTAssertNotNil(b); XCTAssertNotNil(c)

        let ancla = PortapapelesAnclasDirectasIPad.compartido.copiar(
            seleccion: SeleccionLienzo(textos: [0, 1, 2]),
            sesion: sesion,
            titulo: "Grupo")

        XCTAssertEqual(ancla?.fuente.elementoId, c.map { "x:\($0)" })
    }

    private func crearSesion() throws -> Sesion {
        guard let sesion = Sesion() else {
            throw NSError(domain: "CuadernUTests", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "No se pudo crear Sesion"])
        }
        return sesion
    }
}
