import XCTest
@testable import CuadernUEstudio

final class IdentidadCopiaDocumentoIPadTests: XCTestCase {
    func testExtraeLaIdentidadDurableDelDocumentoSerializado() throws {
        let id = DocumentoPersistenteID(UUID())
        let sesion = try XCTUnwrap(Sesion(idPersistente: id))
        let datos = try XCTUnwrap(sesion.guardar())

        XCTAssertEqual(IdentidadCopiaDocumentoIPad.origen(de: datos), id)
    }

    func testDatosCorruptosNoInventanUnaIdentidad() {
        let datos = Data([0x54, 0x49, 0x4E, 0x54, 0x41, 0xFF, 0x00])
        XCTAssertNil(IdentidadCopiaDocumentoIPad.origen(de: datos))
    }

    func testAgregadoRechazaQueOrigenYDestinoSeanLaMismaIdentidad() {
        let id = DocumentoPersistenteID(UUID())
        XCTAssertFalse(IdentidadCopiaDocumentoIPad.copiarAgregadoSiExiste(
            origen: id, destino: id))
    }

    func testImportacionSinIdentidadNoFabricaSidecars() {
        let destino = DocumentoPersistenteID(UUID())
        XCTAssertTrue(IdentidadCopiaDocumentoIPad.copiarAgregadoSiExiste(
            origen: nil, destino: destino))
    }
}
