import SwiftUI
import UIKit
import XCTest
@testable import CuadernU

final class CuadernU4CalculadorasTests: XCTestCase {

    func testFxCg50BootsAndRendersColorLcd() {
        let emu = EmuladorCasioV47(modelo: .cg50)
        XCTAssertTrue(emu.cargada, "fx-CG50 ROMs must load from CuadernU app bundle")
        XCTAssertEqual(emu.ancho, 384)
        XCTAssertEqual(emu.alto, 216)

        emu.avanzarDeterminista(pasos: 4)
        XCTAssertNotNil(emu.imagenLCD, "fx-CG50 must produce a valid CGImage framebuffer")
        XCTAssertGreaterThan(emu.contarPixelesNoUniformes(), 500,
                             "fx-CG50 warm-state LCD must contain visible MAIN MENU pixels")
    }

    func testFx991CwBootsAndRenders4GrayLcd() {
        let emu = EmuladorCasioV47(modelo: .cw2)
        XCTAssertTrue(emu.cargada, "fx-991CW ROM must load from CuadernU app bundle")
        XCTAssertEqual(emu.ancho, 192)
        XCTAssertEqual(emu.alto, 63)

        emu.avanzarDeterminista(pasos: 6)
        XCTAssertNotNil(emu.imagenLCD, "fx-991CW must produce a valid CGImage framebuffer")
        XCTAssertGreaterThan(emu.contarPixelesNoUniformes(), 10,
                             "fx-991CW LCD must render active pixels after power-on")
    }

    func testFx570LaXBootsAndRendersLcd() {
        let emu = EmuladorCalculadora(modelo: .classwiz)
        XCTAssertTrue(emu.cargada, "fx-570LA X must initialize via calcemu")
        XCTAssertEqual(emu.ancho, 192)
        XCTAssertEqual(emu.alto, 63)
        emu.avanzarDeterminista(ms: 600)
        emu.pulsar(0x42) // AC/ON
        emu.avanzarDeterminista(ms: 300)
        emu.soltar()
        emu.avanzarDeterminista(ms: 400)
        let encendidos = emu.pixeles.reduce(0) { $0 + ($1 != 0 ? 1 : 0) }
        XCTAssertGreaterThan(encendidos, 20, "fx-570LA X LCD must render visible pixels")
    }

    func testFx570VnPlusBootsAndRendersLcd() {
        let emu = EmuladorCalculadora(modelo: .esplus)
        XCTAssertTrue(emu.cargada, "fx-570VN PLUS must initialize via calcemu")
        XCTAssertEqual(emu.ancho, 96)
        XCTAssertEqual(emu.alto, 31)
        emu.avanzarDeterminista(ms: 600)
        emu.pulsar(0x42) // AC/ON
        emu.avanzarDeterminista(ms: 300)
        emu.soltar()
        emu.avanzarDeterminista(ms: 400)
        let encendidos = emu.pixeles.reduce(0) { $0 + ($1 != 0 ? 1 : 0) }
        XCTAssertGreaterThan(encendidos, 10, "fx-570VN PLUS LCD must render visible pixels")
    }

    func testCuadernUStudioBridgeCxx17FfiWorksOnIPadOS() {
        let bridge = CuadernUStudioBridge.compartido
        let caps = bridge.capacidades()
        let features = (caps["features"] as? [String]) ?? []
        XCTAssertTrue(features.contains("cudoc_zip_interop"))
        XCTAssertTrue(features.contains("engineering_cas_varsub"))
        XCTAssertTrue(features.contains("mna_circuit_dc_ac_bode_transient_laplace"))

        let cas = bridge.evaluarCas("12 V / 2 kohm | mA", grados: true)
        XCTAssertEqual(cas["ok"] as? Bool, true)

        let chem = bridge.analizarQuimica("Fe + O2 -> Fe2O3")
        XCTAssertEqual(chem["valid"] as? Bool, true)
        XCTAssertEqual(chem["balanced"] as? Bool, true)

        let mna = bridge.resolverCircuito(netlist: "V1 1 0 12\nR1 1 2 1000\nR2 2 0 1000\n", modo: "dc")
        XCTAssertEqual(mna["ok"] as? Bool, true)

        // Verify .cudoc binary roundtrip interoperable with Windows & Android
        let tmpPath = NSTemporaryDirectory() + "cuadernu_ipados_test.cudoc"
        try? FileManager.default.removeItem(atPath: tmpPath)
        XCTAssertTrue(bridge.exportarResumenCudoc(
            titulo: "Apuntes iPadOS Verificados",
            bloquesTexto: ["Ley de Ohm: V = I * R", "fx-CG50 + CuadernU v47"],
            rutaDestino: tmpPath))
        let snap = bridge.inspeccionarCudoc(ruta: tmpPath)
        XCTAssertNotNil(snap)
        try? FileManager.default.removeItem(atPath: tmpPath)
    }

    @MainActor
    func testCaptureLiveIPadOSScreenshots() {
        let outDir = ProcessInfo.processInfo.environment["CUADERNU_SHOTS_DIR"]
            ?? (NSTemporaryDirectory() + "ipados_screenshots")
        try? FileManager.default.createDirectory(atPath: outDir, withIntermediateDirectories: true)

        let ipadSize = CGSize(width: 820, height: 1180)

        capturarVistaSwiftUI(
            VistaSelector(),
            nombre: "ipados_01_selector_4calcs.png",
            tamano: ipadSize,
            esperaSegundos: 0.6,
            directorio: outDir
        )

        capturarVistaSwiftUI(
            NavigationStack { VistaCalculadoraV47(modelo: .cg50) },
            nombre: "ipados_02_fx_cg50_live.png",
            tamano: ipadSize,
            esperaSegundos: 1.5,
            directorio: outDir
        )

        capturarVistaSwiftUI(
            NavigationStack { VistaCalculadoraV47(modelo: .cw2) },
            nombre: "ipados_03_fx_991cw_live.png",
            tamano: ipadSize,
            esperaSegundos: 1.5,
            directorio: outDir
        )

        capturarVistaSwiftUI(
            NavigationStack { VistaCalculadora(modelo: .classwiz) },
            nombre: "ipados_04_fx_570lax_live.png",
            tamano: ipadSize,
            esperaSegundos: 1.2,
            directorio: outDir
        )

        capturarVistaSwiftUI(
            NavigationStack { VistaCalculadora(modelo: .esplus) },
            nombre: "ipados_05_fx_570vnplus_live.png",
            tamano: ipadSize,
            esperaSegundos: 1.2,
            directorio: outDir
        )

        GestorArchivosCuadernU.compartido.configurarRutaBase()
        let bibliotecaNav = UINavigationController(rootViewController: BibliotecaCuadernosIPad())
        capturarControlador(
            bibliotecaNav,
            nombre: "ipados_06_cuadernu_biblioteca.png",
            tamano: CGSize(width: 1180, height: 820),
            esperaSegundos: 0.8,
            directorio: outDir
        )
    }

    @MainActor
    private func capturarVistaSwiftUI<V: View>(
        _ vista: V,
        nombre: String,
        tamano: CGSize,
        esperaSegundos: TimeInterval,
        directorio: String
    ) {
        let host = UIHostingController(rootView: vista)
        capturarControlador(host, nombre: nombre, tamano: tamano, esperaSegundos: esperaSegundos, directorio: directorio)
    }

    @MainActor
    private func capturarControlador(
        _ vc: UIViewController,
        nombre: String,
        tamano: CGSize,
        esperaSegundos: TimeInterval,
        directorio: String
    ) {
        let escena = UIApplication.shared.connectedScenes.first as? UIWindowScene
        let ventana = escena.map { UIWindow(windowScene: $0) } ?? UIWindow(frame: CGRect(origin: .zero, size: tamano))
        ventana.frame = CGRect(origin: .zero, size: tamano)
        ventana.overrideUserInterfaceStyle = .light
        ventana.rootViewController = vc
        ventana.makeKeyAndVisible()

        let fin = Date().addingTimeInterval(esperaSegundos)
        while Date() < fin {
            RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        }
        ventana.layoutIfNeeded()

        let format = UIGraphicsImageRendererFormat()
        format.scale = 2.0
        format.opaque = true
        let renderer = UIGraphicsImageRenderer(size: tamano, format: format)
        let img = renderer.image { _ in
            ventana.drawHierarchy(in: ventana.bounds, afterScreenUpdates: true)
        }

        if let data = img.pngData() {
            let destino = URL(fileURLWithPath: directorio).appendingPathComponent(nombre)
            try? data.write(to: destino, options: .atomic)
            XCTAssertGreaterThan(data.count, 4096, "Screenshot \(nombre) should be a valid non-empty PNG")
            let att = XCTAttachment(image: img)
            att.name = nombre
            att.lifetime = .keepAlways
            add(att)
        } else {
            XCTFail("Failed to encode PNG for \(nombre)")
        }

        ventana.isHidden = true
        ventana.rootViewController = nil
    }
}
