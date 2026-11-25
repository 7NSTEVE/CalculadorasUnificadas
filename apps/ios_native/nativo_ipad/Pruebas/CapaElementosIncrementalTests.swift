import XCTest
import UIKit
@testable import CuadernUEstudio

final class CapaElementosIncrementalTests: XCTestCase {
    private func sesion() throws -> Sesion {
        try XCTUnwrap(Sesion())
    }

    func testRecargarConservaVistaTextoExistenteAlAgregarOtroTexto() throws {
        let sesion = try sesion()
        let capa = CapaElementos(sesion: sesion)
        let idPrimero = try XCTUnwrap(sesion.anadirTexto(
            "Primero", marco: CGRect(x: 10, y: 20, width: 220, height: 90)))
        capa.recargar()

        let primera = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == idPrimero })
        _ = sesion.anadirTexto(
            "Segundo", marco: CGRect(x: 260, y: 20, width: 220, height: 90))
        capa.recargar()

        let despues = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == idPrimero })
        XCTAssertTrue(primera === despues,
                      "Agregar otro elemento no debe reconstruir las vistas ya existentes.")
    }

    func testRecargarActualizaGeometriaSinRecrearVistaTexto() throws {
        let sesion = try sesion()
        let capa = CapaElementos(sesion: sesion)
        let id = try XCTUnwrap(sesion.anadirTexto(
            "Mover", marco: CGRect(x: 10, y: 20, width: 200, height: 80)))
        capa.recargar()
        let vista = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == id })

        var actualizado = try XCTUnwrap(sesion.textos.first { $0.id == id })
        actualizado.marco = CGRect(x: 140, y: 160, width: 360, height: 120)
        actualizado.rotacion = 0.2
        XCTAssertTrue(sesion.actualizarTexto(actualizado))
        capa.recargar()

        let misma = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == id })
        XCTAssertTrue(vista === misma)
        XCTAssertEqual(misma.bounds.size.width, 360, accuracy: 0.5)
        XCTAssertEqual(misma.bounds.size.height, 120, accuracy: 0.5)
        XCTAssertEqual(misma.center.x, actualizado.marco.midX, accuracy: 0.5)
        XCTAssertEqual(misma.center.y, actualizado.marco.midY, accuracy: 0.5)
    }

    func testRecargarConservaVistaMedioCuandoSoloCambiaGeometria() throws {
        let sesion = try sesion()
        let capa = CapaElementos(sesion: sesion)
        let id = try XCTUnwrap(sesion.anadirMedio(
            hash: "adjunto-inexistente-test",
            nombre: "Documento",
            tipo: .documento,
            marco: CGRect(x: 20, y: 30, width: 240, height: 180)))
        capa.recargar()
        let vista = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaMedioElemento }
            .first { $0.elemento.id == id })

        var medio = try XCTUnwrap(sesion.medios.first { $0.id == id })
        medio.marco = CGRect(x: 300, y: 350, width: 420, height: 280)
        XCTAssertTrue(sesion.actualizarMedio(medio))
        capa.recargar()

        let misma = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaMedioElemento }
            .first { $0.elemento.id == id })
        XCTAssertTrue(vista === misma)
        XCTAssertEqual(misma.bounds.size.width, 420, accuracy: 0.5)
        XCTAssertEqual(misma.bounds.size.height, 280, accuracy: 0.5)
    }

    func testOcultarTextosRetiraSoloVistasTexto() throws {
        let sesion = try sesion()
        let capa = CapaElementos(sesion: sesion)
        _ = sesion.anadirTexto("Texto", marco: CGRect(x: 0, y: 0, width: 200, height: 80))
        _ = sesion.anadirMedio(hash: "x", nombre: "Archivo", tipo: .documento,
                               marco: CGRect(x: 0, y: 120, width: 200, height: 160))
        capa.recargar()
        let medio = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaMedioElemento }.first)

        capa.mostrarTextos = false

        XCTAssertTrue(capa.subviews.compactMap { $0 as? VistaTextoElemento }.isEmpty)
        let medioDespues = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaMedioElemento }.first)
        XCTAssertTrue(medio === medioDespues)
    }

    func testEliminarUnElementoNoRecreaLosRestantes() throws {
        let sesion = try sesion()
        let capa = CapaElementos(sesion: sesion)
        let conservar = try XCTUnwrap(sesion.anadirTexto(
            "Conservar", marco: CGRect(x: 0, y: 0, width: 200, height: 80)))
        let borrar = try XCTUnwrap(sesion.anadirTexto(
            "Borrar", marco: CGRect(x: 0, y: 100, width: 200, height: 80)))
        capa.recargar()
        let vistaConservar = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == conservar })

        XCTAssertTrue(sesion.eliminarTexto(id: borrar))
        capa.recargar()

        XCTAssertNil(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == borrar })
        let misma = try XCTUnwrap(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == conservar })
        XCTAssertTrue(vistaConservar === misma)
    }
}
