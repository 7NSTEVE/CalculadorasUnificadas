import XCTest
@testable import CuadernUEstudio

final class IdentidadDocumentoDurableIPadTests: XCTestCase {
    func testCodableRoundTripConservaUUIDExacto() throws {
        let uuid = try XCTUnwrap(UUID(uuidString: "12345678-1234-5678-9ABC-DEF012345678"))
        let original = DocumentoPersistenteID(uuid)
        let datos = try JSONEncoder().encode(original)
        let leida = try JSONDecoder().decode(DocumentoPersistenteID.self, from: datos)
        XCTAssertEqual(leida, original)
        XCTAssertEqual(leida.rawValue, uuid)
    }

    func testClaveCompatibilidadEsDeterministaYSeparaHandlesBajos() throws {
        let uuid = try XCTUnwrap(UUID(uuidString: "12345678-1234-5678-9ABC-DEF012345678"))
        let identidad = DocumentoPersistenteID(uuid)
        XCTAssertEqual(identidad.claveCompatibilidadUInt64,
                       DocumentoPersistenteID(uuid).claveCompatibilidadUInt64)
        XCTAssertNotEqual(identidad.claveCompatibilidadUInt64 & (UInt64(1) << 63), 0)
        XCTAssertGreaterThan(identidad.claveCompatibilidadUInt64, 1_000_000)
    }

    func testDosDocumentosFijosNoCompartenClaveCompatibilidad() throws {
        let a = DocumentoPersistenteID(try XCTUnwrap(
            UUID(uuidString: "00000000-0000-0000-0000-000000000001")))
        let b = DocumentoPersistenteID(try XCTUnwrap(
            UUID(uuidString: "00000000-0000-0000-0000-000000000002")))
        XCTAssertNotEqual(a, b)
        XCTAssertNotEqual(a.claveCompatibilidadUInt64, b.claveCompatibilidadUInt64)
    }

    func testJSONInvalidoNoSeConvierteSilenciosamenteEnOtroDocumento() {
        let datos = Data("\"no-es-un-uuid\"".utf8)
        XCTAssertThrowsError(try JSONDecoder().decode(DocumentoPersistenteID.self, from: datos))
    }

    func testDescriptionEsUUIDCanonicoLowercase() throws {
        let identidad = DocumentoPersistenteID(try XCTUnwrap(
            UUID(uuidString: "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE")))
        XCTAssertEqual(identidad.description, "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
    }
}
