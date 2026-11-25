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
        XCTAssertEqual(chem["ok"] as? Bool, true)

        let mna = bridge.resolverCircuito(netlist: "V1 1 0 12\nR1 1 2 1000\nR2 2 0 1000\n", modo: "dc")
        XCTAssertEqual(mna["ok"] as? Bool, true)
    }
}
