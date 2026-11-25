import XCTest
@testable import CuadernUEstudio

final class MigracionesVersionadasIPadTests: XCTestCase {
    private func base() throws -> URL {
        let u = FileManager.default.temporaryDirectory
            .appendingPathComponent("migraciones-v-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }

    private func paso(_ id: String, _ desde: Int, _ hasta: Int,
                      sufijo: String) -> MigracionDatosVersionadaIPad {
        .init(identificador: id, desdeVersion: desde, hastaVersion: hasta) { datos in
            var texto = String(data: datos, encoding: .utf8) ?? ""
            texto += sufijo
            return Data(texto.utf8)
        }
    }

    func testCadenaAplicaEnOrdenYRegistraHashes() throws {
        let resultado = try MotorMigracionesVersionadasIPad.aplicar(
            datos: Data("base".utf8),
            versionActual: 1,
            versionDestino: 3,
            migraciones: [paso("2-a-3", 2, 3, sufijo: "-v3"),
                          paso("1-a-2", 1, 2, sufijo: "-v2")])
        XCTAssertEqual(String(data: resultado.datos, encoding: .utf8), "base-v2-v3")
        XCTAssertEqual(resultado.versionFinal, 3)
        XCTAssertEqual(resultado.pasos.map(\.identificador), ["1-a-2", "2-a-3"])
        XCTAssertTrue(resultado.pasos.allSatisfy { $0.sha256Antes != $0.sha256Despues })
    }

    func testVersionYaActualEsIdempotente() throws {
        let datos = Data("sin-cambio".utf8)
        let resultado = try MotorMigracionesVersionadasIPad.aplicar(
            datos: datos,
            versionActual: 4,
            versionDestino: 4,
            migraciones: [])
        XCTAssertEqual(resultado.datos, datos)
        XCTAssertTrue(resultado.pasos.isEmpty)
    }

    func testSaltoFaltanteNoProduceDatosParciales() {
        XCTAssertThrowsError(try MotorMigracionesVersionadasIPad.aplicar(
            datos: Data("base".utf8),
            versionActual: 1,
            versionDestino: 3,
            migraciones: [paso("1-a-2", 1, 2, sufijo: "-v2")])) {
                XCTAssertEqual($0 as? ErrorMigracionVersionadaIPad, .saltoFaltante(2))
            }
    }

    func testRechazaDosRutasDesdeLaMismaVersion() {
        XCTAssertThrowsError(try MotorMigracionesVersionadasIPad.aplicar(
            datos: Data(), versionActual: 1, versionDestino: 2,
            migraciones: [
                paso("a", 1, 2, sufijo: "a"),
                paso("b", 1, 2, sufijo: "b")
            ])) {
                XCTAssertEqual($0 as? ErrorMigracionVersionadaIPad,
                               .pasosDuplicados(1))
            }
    }

    func testLimiteDeSalidaDetieneMigracion() {
        let grande = MigracionDatosVersionadaIPad(
            identificador: "explota", desdeVersion: 1, hastaVersion: 2) { _ in
                Data(repeating: 1, count: 101)
            }
        XCTAssertThrowsError(try MotorMigracionesVersionadasIPad.aplicar(
            datos: Data(), versionActual: 1, versionDestino: 2,
            migraciones: [grande], maximoSalida: 100)) {
                XCTAssertEqual($0 as? ErrorMigracionVersionadaIPad,
                               .salidaDemasiadoGrande(101))
            }
    }

    func testConjuntoMigraDosArchivosEnUnaTransaccion() throws {
        let raiz = try base(), a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")
        try Data("A".utf8).write(to: a); try Data("B".utf8).write(to: b)
        let migrador = MigradorConjuntoVersionadoIPad(
            transacciones: .init(base: raiz.appendingPathComponent("journals")))
        let resultado = try migrador.migrar(
            documentoId: DocumentoPersistenteID(),
            solicitudes: [
                .init(nombre: "doc", archivo: a, versionActual: 1, versionDestino: 2,
                      migraciones: [paso("doc-1-2", 1, 2, sufijo: "2")]),
                .init(nombre: "side", archivo: b, versionActual: 7, versionDestino: 8,
                      migraciones: [paso("side-7-8", 7, 8, sufijo: "8")])
            ])
        XCTAssertTrue(resultado.huboCambios)
        XCTAssertEqual(String(data: try Data(contentsOf: a), encoding: .utf8), "A2")
        XCTAssertEqual(String(data: try Data(contentsOf: b), encoding: .utf8), "B8")
    }

    func testFalloEnSegundoArchivoDejaAmbosOriginales() throws {
        enum ErrorPrueba: Error { case invalido }
        let raiz = try base(), a = raiz.appendingPathComponent("a"), b = raiz.appendingPathComponent("b")
        try Data("A".utf8).write(to: a); try Data("B".utf8).write(to: b)
        let falla = MigracionDatosVersionadaIPad(
            identificador: "falla", desdeVersion: 1, hastaVersion: 2) { _ in
                throw ErrorPrueba.invalido
            }
        let migrador = MigradorConjuntoVersionadoIPad(
            transacciones: .init(base: raiz.appendingPathComponent("journals")))
        XCTAssertThrowsError(try migrador.migrar(
            documentoId: DocumentoPersistenteID(),
            solicitudes: [
                .init(nombre: "a", archivo: a, versionActual: 1, versionDestino: 2,
                      migraciones: [paso("ok", 1, 2, sufijo: "2")]),
                .init(nombre: "b", archivo: b, versionActual: 1, versionDestino: 2,
                      migraciones: [falla])
            ]))
        XCTAssertEqual(String(data: try Data(contentsOf: a), encoding: .utf8), "A")
        XCTAssertEqual(String(data: try Data(contentsOf: b), encoding: .utf8), "B")
    }

    func testConjuntoSinCambiosNoCreaTransaccion() throws {
        let raiz = try base(), a = raiz.appendingPathComponent("a")
        try Data("A".utf8).write(to: a)
        let resultado = try MigradorConjuntoVersionadoIPad(
            transacciones: .init(base: raiz.appendingPathComponent("journals")))
            .migrar(documentoId: DocumentoPersistenteID(), solicitudes: [
                .init(nombre: "a", archivo: a,
                      versionActual: 2, versionDestino: 2,
                      migraciones: [])
            ])
        XCTAssertFalse(resultado.huboCambios)
        XCTAssertNil(resultado.transaccionId)
        XCTAssertEqual(String(data: try Data(contentsOf: a), encoding: .utf8), "A")
    }
}
