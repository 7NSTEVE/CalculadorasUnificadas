import XCTest
import UIKit
@testable import CuadernUEstudio

final class RenderizadorMiniaturasPDFIPadTests: XCTestCase {
    private func imagen(_ tamano: CGSize = CGSize(width: 40, height: 60)) -> UIImage {
        UIGraphicsImageRenderer(size: tamano).image { ctx in
            UIColor.white.setFill()
            ctx.fill(CGRect(origin: .zero, size: tamano))
        }
    }

    func testCoalesceDosSolicitudesIgualesEnUnSoloRender() {
        let render = expectation(description: "render")
        let uno = expectation(description: "uno")
        let dos = expectation(description: "dos")
        let bloqueo = NSLock()
        var llamadas = 0
        let salida = imagen()

        let servicio = RenderizadorMiniaturasPDFIPad(
            limiteCacheBytes: 16 * 1_024 * 1_024,
            concurrencia: 1,
            motor: { _, _, _, _ in
                bloqueo.lock(); llamadas += 1; bloqueo.unlock()
                render.fulfill()
                Thread.sleep(forTimeInterval: 0.08)
                return salida
            })
        let url = URL(fileURLWithPath: "/tmp/coalesce.pdf")

        let t1 = servicio.solicitar(
            url: url, pagina: 3, tamano: CGSize(width: 300, height: 400), escala: 2) {
                XCTAssertNotNil($0); uno.fulfill()
            }
        let t2 = servicio.solicitar(
            url: url, pagina: 3, tamano: CGSize(width: 302, height: 398), escala: 2) {
                XCTAssertNotNil($0); dos.fulfill()
            }
        _ = [t1, t2]

        wait(for: [render, uno, dos], timeout: 2)
        XCTAssertEqual(llamadas, 1)
        XCTAssertEqual(servicio.metricas.rendersIniciados, 1)
        XCTAssertEqual(servicio.metricas.solicitudesCoalescidas, 1)
    }

    func testSegundaSolicitudTrasRenderUsaCache() {
        let primero = expectation(description: "primero")
        let segundo = expectation(description: "segundo")
        var llamadas = 0
        let servicio = RenderizadorMiniaturasPDFIPad(
            motor: { _, _, _, _ in
                llamadas += 1
                return self.imagen()
            })
        let url = URL(fileURLWithPath: "/tmp/cache.pdf")
        var tokens: [TokenMiniaturaPDFIPad] = []
        tokens.append(servicio.solicitar(
            url: url, pagina: 0, tamano: CGSize(width: 220, height: 300), escala: 2) { _ in
                primero.fulfill()
            })
        wait(for: [primero], timeout: 2)

        tokens.append(servicio.solicitar(
            url: url, pagina: 0, tamano: CGSize(width: 220, height: 300), escala: 2) { _ in
                segundo.fulfill()
            })
        wait(for: [segundo], timeout: 2)
        _ = tokens

        XCTAssertEqual(llamadas, 1)
        XCTAssertEqual(servicio.metricas.aciertosCache, 1)
    }

    func testCancelarUnConsumidorNoCancelaElCompartido() {
        let segundo = expectation(description: "segundo")
        let primeroNoDebeVolver = expectation(description: "primero cancelado")
        primeroNoDebeVolver.isInverted = true
        let servicio = RenderizadorMiniaturasPDFIPad(
            concurrencia: 1,
            motor: { _, _, _, _ in
                Thread.sleep(forTimeInterval: 0.08)
                return self.imagen()
            })
        let url = URL(fileURLWithPath: "/tmp/cancel.pdf")

        let t1 = servicio.solicitar(
            url: url, pagina: 1, tamano: CGSize(width: 200, height: 280)) { _ in
                primeroNoDebeVolver.fulfill()
            }
        let t2 = servicio.solicitar(
            url: url, pagina: 1, tamano: CGSize(width: 200, height: 280)) { _ in
                segundo.fulfill()
            }
        t1.cancelar()
        _ = t2

        wait(for: [segundo, primeroNoDebeVolver], timeout: 1)
        XCTAssertEqual(servicio.metricas.rendersCompletados, 1)
    }

    func testMemoryWarningVaciaCacheYObligaNuevoRender() {
        let centro = NotificationCenter()
        var llamadas = 0
        let servicio = RenderizadorMiniaturasPDFIPad(
            centroNotificaciones: centro,
            motor: { _, _, _, _ in
                llamadas += 1
                return self.imagen()
            })
        let url = URL(fileURLWithPath: "/tmp/memory.pdf")
        let a = expectation(description: "a")
        let b = expectation(description: "b")
        var tokens: [TokenMiniaturaPDFIPad] = []

        tokens.append(servicio.solicitar(
            url: url, pagina: 0, tamano: CGSize(width: 180, height: 240)) { _ in a.fulfill() })
        wait(for: [a], timeout: 2)
        centro.post(name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
        tokens.append(servicio.solicitar(
            url: url, pagina: 0, tamano: CGSize(width: 180, height: 240)) { _ in b.fulfill() })
        wait(for: [b], timeout: 2)
        _ = tokens

        XCTAssertEqual(llamadas, 2)
    }

    func testTamanoInvalidoNoArrancaRender() {
        let completado = expectation(description: "nil")
        var llamadas = 0
        let servicio = RenderizadorMiniaturasPDFIPad(
            motor: { _, _, _, _ in llamadas += 1; return self.imagen() })
        let token = servicio.solicitar(
            url: URL(fileURLWithPath: "/tmp/invalido.pdf"),
            pagina: -1,
            tamano: .zero) {
                XCTAssertNil($0)
                completado.fulfill()
            }
        _ = token
        wait(for: [completado], timeout: 1)
        XCTAssertEqual(llamadas, 0)
    }
}
