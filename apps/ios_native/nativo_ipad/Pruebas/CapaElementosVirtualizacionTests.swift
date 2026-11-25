import XCTest
import UIKit
@testable import CuadernUEstudio

final class CapaElementosVirtualizacionTests: XCTestCase {
    private func sesionConTextos(_ cantidad: Int, saltoY: CGFloat = 2_000) throws
        -> (Sesion, [String]) {
        let sesion = try XCTUnwrap(Sesion())
        var ids: [String] = []
        for i in 0..<cantidad {
            let id = try XCTUnwrap(sesion.anadirTexto(
                "T\(i)",
                marco: CGRect(x: 40, y: CGFloat(i) * saltoY,
                              width: 260, height: 100)))
            ids.append(id)
        }
        return (sesion, ids)
    }

    private func politicaPrueba() -> PoliticaVirtualizacionElementosIPad {
        var p = PoliticaVirtualizacionElementosIPad()
        p.umbralElementos = 3
        p.prefetchHorizontal = 0
        p.prefetchVertical = 0
        p.margenMinimoHorizontal = 100
        p.margenMinimoVertical = 150
        p.fraccionHistéresis = 0.1
        return p
    }

    func testDocumentoGrandeNoCreaUnaVistaPorCadaElemento() throws {
        let (sesion, _) = try sesionConTextos(12)
        let capa = CapaElementos(sesion: sesion)
        capa.politicaVirtualizacion = politicaPrueba()

        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 0, width: 800, height: 900))

        XCTAssertEqual(capa.totalElementosModelo, 12)
        XCTAssertLessThan(capa.cantidadVistasMaterializadas, 12)
        XCTAssertGreaterThan(capa.cantidadVistasMaterializadas, 0)
        let ys = capa.subviews.compactMap { ($0 as? VistaTextoElemento)?.elemento.marco.minY }
        XCTAssertTrue(ys.allSatisfy { $0 < 1_500 })
    }

    func testCambiarViewportReciclaLejanasYMaterializaLasNuevas() throws {
        let (sesion, ids) = try sesionConTextos(10)
        let capa = CapaElementos(sesion: sesion)
        capa.politicaVirtualizacion = politicaPrueba()
        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 0, width: 800, height: 900))
        let iniciales = Set(capa.subviews.compactMap {
            ($0 as? VistaTextoElemento)?.elemento.id
        })
        XCTAssertTrue(iniciales.contains(ids[0]))
        XCTAssertFalse(iniciales.contains(ids[6]))

        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 11_700, width: 800, height: 900))
        let finales = Set(capa.subviews.compactMap {
            ($0 as? VistaTextoElemento)?.elemento.id
        })

        XCTAssertFalse(finales.contains(ids[0]))
        XCTAssertTrue(finales.contains(ids[6]))
        XCTAssertLessThan(finales.count, ids.count)
    }

    func testSeleccionFueraDelViewportQuedaAncladaYVisible() throws {
        let (sesion, ids) = try sesionConTextos(8)
        let capa = CapaElementos(sesion: sesion)
        capa.politicaVirtualizacion = politicaPrueba()
        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 0, width: 800, height: 900))
        XCTAssertNil(capa.subviews.compactMap { $0 as? VistaTextoElemento }
            .first { $0.elemento.id == ids[7] })

        capa.mostrarSeleccion(SeleccionLienzo(textos: [7]))

        let seleccionada = try XCTUnwrap(capa.subviews.compactMap {
            $0 as? VistaTextoElemento
        }.first { $0.elemento.id == ids[7] })
        XCTAssertEqual(seleccionada.layer.borderWidth, 3)
    }

    func testDocumentoDebajoDelUmbralConservaComportamientoCompleto() throws {
        let (sesion, ids) = try sesionConTextos(3)
        let capa = CapaElementos(sesion: sesion)
        capa.politicaVirtualizacion = politicaPrueba()
        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 0, width: 800, height: 900))
        capa.recargar()

        let visibles = Set(capa.subviews.compactMap {
            ($0 as? VistaTextoElemento)?.elemento.id
        })
        XCTAssertEqual(visibles, Set(ids))
    }

    func testHisteresisNoReconstruyeVistasConScrollPequeno() throws {
        let (sesion, _) = try sesionConTextos(8, saltoY: 600)
        let capa = CapaElementos(sesion: sesion)
        var p = politicaPrueba()
        p.margenMinimoVertical = 2_000
        p.fraccionHistéresis = 0.2
        capa.politicaVirtualizacion = p
        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 0, width: 800, height: 900))
        let antes = capa.subviews.compactMap { $0 as? VistaTextoElemento }
        let identidadesAntes = Set(antes.map { ObjectIdentifier($0) })

        capa.actualizarRegionVisible(
            CGRect(x: 0, y: 80, width: 800, height: 900))
        let despues = capa.subviews.compactMap { $0 as? VistaTextoElemento }
        let identidadesDespues = Set(despues.map { ObjectIdentifier($0) })

        XCTAssertEqual(identidadesAntes, identidadesDespues)
    }
}
